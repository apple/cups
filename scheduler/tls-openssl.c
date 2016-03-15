/*
 * "$Id: tls-openssl.c 3755 2012-03-30 05:59:14Z msweet $"
 *
 *   TLS support code for the CUPS scheduler using OpenSSL.
 *
 *   Copyright 2007-2012 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cupsdEndTLS()	- Shutdown a secure session with the client.
 *   cupsdStartTLS()	- Start a secure session with the client.
 *   make_certificate() - Make a self-signed SSL/TLS certificate.
 */


/*
 * Local functions...
 */

static int		make_certificate(cupsd_client_t *con);


/*
 * 'cupsdEndTLS()' - Shutdown a secure session with the client.
 */

int					/* O - 1 on success, 0 on error */
cupsdEndTLS(cupsd_client_t *con)	/* I - Client connection */
{
  SSL_CTX	*context;		/* Context for encryption */
  unsigned long	error;			/* Error code */
  int		status;			/* Return status */


  context = SSL_get_SSL_CTX(con->http.tls);

  switch (SSL_shutdown(con->http.tls))
  {
    case 1 :
	cupsdLogMessage(CUPSD_LOG_DEBUG,
			"SSL shutdown successful!");
	status = 1;
	break;

    case -1 :
	cupsdLogMessage(CUPSD_LOG_ERROR,
			"Fatal error during SSL shutdown!");

    default :
	while ((error = ERR_get_error()) != 0)
	  cupsdLogMessage(CUPSD_LOG_ERROR, "SSL shutdown failed: %s",
			  ERR_error_string(error, NULL));
	status = 0;
	break;
  }

  SSL_CTX_free(context);
  SSL_free(con->http.tls);
  con->http.tls = NULL;

  return (status);
}


/*
 * 'cupsdStartTLS()' - Start a secure session with the client.
 */

int					/* O - 1 on success, 0 on error */
cupsdStartTLS(cupsd_client_t *con)	/* I - Client connection */
{
  SSL_CTX	*context;		/* Context for encryption */
  BIO		*bio;			/* BIO data */
  unsigned long	error;			/* Error code */


  cupsdLogMessage(CUPSD_LOG_DEBUG, "[Client %d] Encrypting connection.",
                  con->http.fd);

 /*
  * Verify that we have a certificate...
  */

  if (access(ServerKey, 0) || access(ServerCertificate, 0))
  {
   /*
    * Nope, make a self-signed certificate...
    */

    if (!make_certificate(con))
      return (0);
  }

 /*
  * Create the SSL context and accept the connection...
  */

  context = SSL_CTX_new(SSLv23_server_method());

  SSL_CTX_set_options(context, SSL_OP_NO_SSLv2); /* Only use SSLv3 or TLS */
  if (SSLOptions & CUPSD_SSL_NOEMPTY)
    SSL_CTX_set_options(context, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
  SSL_CTX_use_PrivateKey_file(context, ServerKey, SSL_FILETYPE_PEM);
  SSL_CTX_use_certificate_chain_file(context, ServerCertificate);

  bio = BIO_new(_httpBIOMethods());
  BIO_ctrl(bio, BIO_C_SET_FILE_PTR, 0, (char *)HTTP(con));

  con->http.tls = SSL_new(context);
  SSL_set_bio(con->http.tls, bio, bio);

  if (SSL_accept(con->http.tls) != 1)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to encrypt connection from %s.",
                    con->http.hostname);

    while ((error = ERR_get_error()) != 0)
      cupsdLogMessage(CUPSD_LOG_ERROR, "%s", ERR_error_string(error, NULL));

    SSL_CTX_free(context);
    SSL_free(con->http.tls);
    con->http.tls = NULL;
    return (0);
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG, "Connection from %s now encrypted.",
                  con->http.hostname);

  return (1);
}


/*
 * 'make_certificate()' - Make a self-signed SSL/TLS certificate.
 */

static int				/* O - 1 on success, 0 on failure */
make_certificate(cupsd_client_t *con)	/* I - Client connection */
{
#ifdef HAVE_WAITPID
  int		pid,			/* Process ID of command */
		status;			/* Status of command */
  char		command[1024],		/* Command */
		*argv[12],		/* Command-line arguments */
		*envp[MAX_ENV + 1],	/* Environment variables */
		infofile[1024],		/* Type-in information for cert */
		seedfile[1024];		/* Random number seed file */
  int		envc,			/* Number of environment variables */
		bytes;			/* Bytes written */
  cups_file_t	*fp;			/* Seed/info file */
  int		infofd;			/* Info file descriptor */


 /*
  * Run the "openssl" command to seed the random number generator and
  * generate a self-signed certificate that is good for 10 years:
  *
  *     openssl rand -rand seedfile 1
  *
  *     openssl req -new -x509 -keyout ServerKey \
  *             -out ServerCertificate -days 3650 -nodes
  *
  * The seeding step is crucial in ensuring that the openssl command
  * does not block on systems without sufficient entropy...
  */

  if (!cupsFileFind("openssl", getenv("PATH"), 1, command, sizeof(command)))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "No SSL certificate and openssl command not found!");
    return (0);
  }

  if (access("/dev/urandom", 0))
  {
   /*
    * If the system doesn't provide /dev/urandom, then any random source
    * will probably be blocking-style, so generate some random data to
    * use as a seed for the certificate.  Note that we have already
    * seeded the random number generator in cupsdInitCerts()...
    */

    cupsdLogMessage(CUPSD_LOG_INFO,
                    "Seeding the random number generator...");

   /*
    * Write the seed file...
    */

    if ((fp = cupsTempFile2(seedfile, sizeof(seedfile))) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to create seed file %s - %s",
                      seedfile, strerror(errno));
      return (0);
    }

    for (bytes = 0; bytes < 262144; bytes ++)
      cupsFilePutChar(fp, CUPS_RAND());

    cupsFileClose(fp);

   /*
    * Run the openssl command to seed its random number generator...
    */

    argv[0] = "openssl";
    argv[1] = "rand";
    argv[2] = "-rand";
    argv[3] = seedfile;
    argv[4] = "1";
    argv[5] = NULL;

    envc = cupsdLoadEnv(envp, MAX_ENV);
    envp[envc] = NULL;

    if (!cupsdStartProcess(command, argv, envp, -1, -1, -1, -1, -1, 1, NULL,
                           NULL, &pid))
    {
      unlink(seedfile);
      return (0);
    }

    while (waitpid(pid, &status, 0) < 0)
      if (errno != EINTR)
      {
	status = 1;
	break;
      }

    cupsdFinishProcess(pid, command, sizeof(command), NULL);

   /*
    * Remove the seed file, as it is no longer needed...
    */

    unlink(seedfile);

    if (status)
    {
      if (WIFEXITED(status))
	cupsdLogMessage(CUPSD_LOG_ERROR,
                	"Unable to seed random number generator - "
			"the openssl command stopped with status %d!",
	        	WEXITSTATUS(status));
      else
	cupsdLogMessage(CUPSD_LOG_ERROR,
                	"Unable to seed random number generator - "
			"the openssl command crashed on signal %d!",
	        	WTERMSIG(status));

      return (0);
    }
  }

 /*
  * Create a file with the certificate information fields...
  *
  * Note: This assumes that the default questions are asked by the openssl
  * command...
  */

  if ((fp = cupsTempFile2(infofile, sizeof(infofile))) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to create certificate information file %s - %s",
                    infofile, strerror(errno));
    return (0);
  }

  cupsFilePrintf(fp, ".\n.\n.\n%s\n.\n%s\n%s\n",
                 ServerName, ServerName, ServerAdmin);
  cupsFileClose(fp);

  cupsdLogMessage(CUPSD_LOG_INFO,
                  "Generating SSL server key and certificate...");

  argv[0]  = "openssl";
  argv[1]  = "req";
  argv[2]  = "-new";
  argv[3]  = "-x509";
  argv[4]  = "-keyout";
  argv[5]  = ServerKey;
  argv[6]  = "-out";
  argv[7]  = ServerCertificate;
  argv[8]  = "-days";
  argv[9]  = "3650";
  argv[10] = "-nodes";
  argv[11] = NULL;

  cupsdLoadEnv(envp, MAX_ENV);

  infofd = open(infofile, O_RDONLY);

  if (!cupsdStartProcess(command, argv, envp, infofd, -1, -1, -1, -1, 1, NULL,
                         NULL, &pid))
  {
    close(infofd);
    unlink(infofile);
    return (0);
  }

  close(infofd);
  unlink(infofile);

  while (waitpid(pid, &status, 0) < 0)
    if (errno != EINTR)
    {
      status = 1;
      break;
    }

  cupsdFinishProcess(pid, command, sizeof(command), NULL);

  if (status)
  {
    if (WIFEXITED(status))
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to create SSL server key and certificate - "
		      "the openssl command stopped with status %d!",
	              WEXITSTATUS(status));
    else
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to create SSL server key and certificate - "
		      "the openssl command crashed on signal %d!",
	              WTERMSIG(status));
  }
  else
  {
    cupsdLogMessage(CUPSD_LOG_INFO, "Created SSL server key file \"%s\"...",
		    ServerKey);
    cupsdLogMessage(CUPSD_LOG_INFO,
                    "Created SSL server certificate file \"%s\"...",
		    ServerCertificate);
  }

  return (!status);

#else
  return (0);
#endif /* HAVE_WAITPID */
}


/*
 * End of "$Id: tls-openssl.c 3755 2012-03-30 05:59:14Z msweet $".
 */
