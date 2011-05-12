/*
 * "$Id: usersys.c 8498 2009-04-13 17:03:15Z mike $"
 *
 *   User, system, and password routines for CUPS.
 *
 *   Copyright 2007-2011 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsEncryption()        - Get the current encryption settings.
 *   cupsGetPassword()       - Get a password from the user.
 *   cupsGetPassword2()      - Get a password from the user using the advanced
 *                             password callback.
 *   cupsServer()            - Return the hostname/address of the current
 *                             server.
 *   cupsSetClientCertCB()   - Set the client certificate callback.
 *   cupsSetEncryption()     - Set the encryption preference.
 *   cupsSetPasswordCB()     - Set the password callback for CUPS.
 *   cupsSetPasswordCB2()    - Set the advanced password callback for CUPS.
 *   cupsSetServer()         - Set the default server name and port.
 *   cupsSetServerCertCB()   - Set the server certificate callback.
 *   cupsSetUser()           - Set the default user name.
 *   cupsUser()              - Return the current user's name.
 *   _cupsGetPassword()      - Get a password from the user.
 *   _cupsGSSServiceName()   - Get the GSS (Kerberos) service name.
 *   _cupsSetDefaults()      - Set the default server, port, and encryption.
 *   cups_read_client_conf() - Read a client.conf file.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include <stdlib.h>
#include <sys/stat.h>
#ifdef WIN32
#  include <windows.h>
#else
#  include <pwd.h>
#endif /* WIN32 */


/*
 * Local functions...
 */

static void	cups_read_client_conf(cups_file_t *fp,
		                      _cups_globals_t *cg,
		                      const char *cups_encryption,
				      const char *cups_server,
#ifdef HAVE_GSSAPI
                                      const char *cups_gssservicename,
#endif /* HAVE_GSSAPI */
				      const char *cups_anyroot,
				      const char *cups_expiredroot,
				      const char *cups_expiredcerts);


/*
 * 'cupsEncryption()' - Get the current encryption settings.
 *
 * The default encryption setting comes from the CUPS_ENCRYPTION
 * environment variable, then the ~/.cups/client.conf file, and finally the
 * /etc/cups/client.conf file. If not set, the default is
 * @code HTTP_ENCRYPT_IF_REQUESTED@.
 *
 * Note: The current encryption setting is tracked separately for each thread
 * in a program. Multi-threaded programs that override the setting via the
 * @link cupsSetEncryption@ function need to do so in each thread for the same
 * setting to be used.
 */

http_encryption_t			/* O - Encryption settings */
cupsEncryption(void)
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (cg->encryption == (http_encryption_t)-1)
    _cupsSetDefaults();

  return (cg->encryption);
}


/*
 * 'cupsGetPassword()' - Get a password from the user.
 *
 * Uses the current password callback function. Returns @code NULL@ if the
 * user does not provide a password.
 *
 * Note: The current password callback function is tracked separately for each
 * thread in a program. Multi-threaded programs that override the setting via
 * the @link cupsSetPasswordCB@ or @link cupsSetPasswordCB2@ functions need to
 * do so in each thread for the same function to be used.
 */

const char *				/* O - Password */
cupsGetPassword(const char *prompt)	/* I - Prompt string */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  return ((cg->password_cb)(prompt, NULL, NULL, NULL, cg->password_data));
}


/*
 * 'cupsGetPassword2()' - Get a password from the user using the advanced
 *                        password callback.
 *
 * Uses the current password callback function. Returns @code NULL@ if the
 * user does not provide a password.
 *
 * Note: The current password callback function is tracked separately for each
 * thread in a program. Multi-threaded programs that override the setting via
 * the @link cupsSetPasswordCB@ or @link cupsSetPasswordCB2@ functions need to
 * do so in each thread for the same function to be used.
 *
 * @since CUPS 1.4/Mac OS X 10.6@
 */

const char *				/* O - Password */
cupsGetPassword2(const char *prompt,	/* I - Prompt string */
		 http_t     *http,	/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
		 const char *method,	/* I - Request method ("GET", "POST", "PUT") */
		 const char *resource)	/* I - Resource path */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (!http)
    http = _cupsConnect();

  return ((cg->password_cb)(prompt, http, method, resource, cg->password_data));
}


/*
 * 'cupsServer()' - Return the hostname/address of the current server.
 *
 * The default server comes from the CUPS_SERVER environment variable, then the
 * ~/.cups/client.conf file, and finally the /etc/cups/client.conf file. If not
 * set, the default is the local system - either "localhost" or a domain socket
 * path.
 *
 * The returned value can be a fully-qualified hostname, a numeric IPv4 or IPv6
 * address, or a domain socket pathname.
 *
 * Note: The current server is tracked separately for each thread in a program.
 * Multi-threaded programs that override the server via the
 * @link cupsSetServer@ function need to do so in each thread for the same
 * server to be used.
 */

const char *				/* O - Server name */
cupsServer(void)
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (!cg->server[0])
    _cupsSetDefaults();

  return (cg->server);
}


/*
 * 'cupsSetClientCertCB()' - Set the client certificate callback.
 *
 * Pass @code NULL@ to restore the default callback.
 *
 * Note: The current certificate callback is tracked separately for each thread
 * in a program. Multi-threaded programs that override the callback need to do
 * so in each thread for the same callback to be used.
 *
 * @since CUPS 1.5/Mac OS X 10.7@
 */

void
cupsSetClientCertCB(
    cups_client_cert_cb_t cb,		/* I - Callback function */
    void                  *user_data)	/* I - User data pointer */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  cg->client_cert_cb	= cb;
  cg->client_cert_data	= user_data;
}


/*
 * 'cupsSetCredentials()' - Set the default credentials to be used for SSL/TLS
 *			    connections.
 *
 * Note: The default credentials are tracked separately for each thread in a
 * program. Multi-threaded programs that override the setting need to do so in
 * each thread for the same setting to be used.
 *
 * @since CUPS 1.5/Mac OS X 10.7@
 */

int					/* O - Status of call (0 = success) */
cupsSetCredentials(
    cups_array_t *credentials)		/* I - Array of credentials */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (cupsArrayCount(credentials) < 1)
    return (-1);

  _httpFreeCredentials(cg->tls_credentials);
  cg->tls_credentials = _httpConvertCredentials(credentials);

  return (cg->tls_credentials ? 0 : -1);
}


/*
 * 'cupsSetEncryption()' - Set the encryption preference.
 *
 * The default encryption setting comes from the CUPS_ENCRYPTION
 * environment variable, then the ~/.cups/client.conf file, and finally the
 * /etc/cups/client.conf file. If not set, the default is
 * @code HTTP_ENCRYPT_IF_REQUESTED@.
 *
 * Note: The current encryption setting is tracked separately for each thread
 * in a program. Multi-threaded programs that override the setting need to do
 * so in each thread for the same setting to be used.
 */

void
cupsSetEncryption(http_encryption_t e)	/* I - New encryption preference */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  cg->encryption = e;

  if (cg->http)
    httpEncryption(cg->http, e);
}


/*
 * 'cupsSetPasswordCB()' - Set the password callback for CUPS.
 *
 * Pass @code NULL@ to restore the default (console) password callback, which
 * reads the password from the console. Programs should call either this
 * function or @link cupsSetPasswordCB2@, as only one callback can be registered
 * by a program per thread.
 *
 * Note: The current password callback is tracked separately for each thread
 * in a program. Multi-threaded programs that override the callback need to do
 * so in each thread for the same callback to be used.
 */

void
cupsSetPasswordCB(cups_password_cb_t cb)/* I - Callback function */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (cb == (cups_password_cb_t)0)
    cg->password_cb = (cups_password_cb2_t)_cupsGetPassword;
  else
    cg->password_cb = (cups_password_cb2_t)cb;

  cg->password_data = NULL;
}


/*
 * 'cupsSetPasswordCB2()' - Set the advanced password callback for CUPS.
 *
 * Pass @code NULL@ to restore the default (console) password callback, which
 * reads the password from the console. Programs should call either this
 * function or @link cupsSetPasswordCB2@, as only one callback can be registered
 * by a program per thread.
 *
 * Note: The current password callback is tracked separately for each thread
 * in a program. Multi-threaded programs that override the callback need to do
 * so in each thread for the same callback to be used.
 *
 * @since CUPS 1.4/Mac OS X 10.6@
 */

void
cupsSetPasswordCB2(
    cups_password_cb2_t cb,		/* I - Callback function */
    void                *user_data)	/* I - User data pointer */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (cb == (cups_password_cb2_t)0)
    cg->password_cb = (cups_password_cb2_t)_cupsGetPassword;
  else
    cg->password_cb = cb;

  cg->password_data = user_data;
}


/*
 * 'cupsSetServer()' - Set the default server name and port.
 *
 * The "server" string can be a fully-qualified hostname, a numeric
 * IPv4 or IPv6 address, or a domain socket pathname. Hostnames and numeric IP
 * addresses can be optionally followed by a colon and port number to override
 * the default port 631, e.g. "hostname:8631". Pass @code NULL@ to restore the
 * default server name and port.
 *
 * Note: The current server is tracked separately for each thread in a program.
 * Multi-threaded programs that override the server need to do so in each
 * thread for the same server to be used.
 */

void
cupsSetServer(const char *server)	/* I - Server name */
{
  char		*port;			/* Pointer to port */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (server)
  {
    strlcpy(cg->server, server, sizeof(cg->server));

    if (cg->server[0] != '/' && (port = strrchr(cg->server, ':')) != NULL &&
        !strchr(port, ']') && isdigit(port[1] & 255))
    {
      *port++ = '\0';

      cg->ipp_port = atoi(port);
    }

    if (cg->server[0] == '/')
      strcpy(cg->servername, "localhost");
    else
      strlcpy(cg->servername, cg->server, sizeof(cg->servername));
  }
  else
  {
    cg->server[0]     = '\0';
    cg->servername[0] = '\0';
  }

  if (cg->http)
  {
    httpClose(cg->http);
    cg->http = NULL;
  }
}


/*
 * 'cupsSetServerCertCB()' - Set the server certificate callback.
 *
 * Pass @code NULL@ to restore the default callback.
 *
 * Note: The current credentials callback is tracked separately for each thread
 * in a program. Multi-threaded programs that override the callback need to do
 * so in each thread for the same callback to be used.
 *
 * @since CUPS 1.5/Mac OS X 10.7@
 */

void
cupsSetServerCertCB(
    cups_server_cert_cb_t cb,		/* I - Callback function */
    void		  *user_data)	/* I - User data pointer */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  cg->server_cert_cb	= cb;
  cg->server_cert_data	= user_data;
}


/*
 * 'cupsSetUser()' - Set the default user name.
 *
 * Pass @code NULL@ to restore the default user name.
 *
 * Note: The current user name is tracked separately for each thread in a
 * program. Multi-threaded programs that override the user name need to do so
 * in each thread for the same user name to be used.
 */

void
cupsSetUser(const char *user)		/* I - User name */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (user)
    strlcpy(cg->user, user, sizeof(cg->user));
  else
    cg->user[0] = '\0';
}


/*
 * 'cupsUser()' - Return the current user's name.
 *
 * Note: The current user name is tracked separately for each thread in a
 * program. Multi-threaded programs that override the user name with the
 * @link cupsSetUser@ function need to do so in each thread for the same user
 * name to be used.
 */

const char *				/* O - User name */
cupsUser(void)
{
  const char	*user;			/* USER environment variable */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (!cg->user[0])
  {
#ifdef WIN32
   /*
    * Get the current user name from the OS...
    */

    DWORD	size;			/* Size of string */

    size = sizeof(cg->user);
    if (!GetUserName(cg->user, &size))
#else
   /*
    * Get the user name corresponding to the current UID...
    */

    struct passwd	*pwd;		/* User/password entry */

    setpwent();
    if ((pwd = getpwuid(getuid())) != NULL)
    {
     /*
      * Found a match!
      */

      strlcpy(cg->user, pwd->pw_name, sizeof(cg->user));
    }
    else
#endif /* WIN32 */
    if ((user = getenv("USER")) != NULL)
    {
     /*
      * Use the username from the "USER" environment variable...
      */
      strlcpy(cg->user, user, sizeof(cg->user));
    }
    else
    {
     /*
      * Use the default "unknown" user name...
      */

      strcpy(cg->user, "unknown");
    }
  }

  return (cg->user);
}


/*
 * '_cupsGetPassword()' - Get a password from the user.
 */

const char *				/* O - Password */
_cupsGetPassword(const char *prompt)	/* I - Prompt string */
{
#ifdef WIN32
 /*
  * Currently no console password support is provided on Windows.
  */

  return (NULL);

#else
 /*
  * Use the standard getpass function to get a password from the console.  An
  * empty password is treated as canceling the authentication request.
  */

  const char	*password = getpass(prompt);
					/* Password string */

  if (!password || !password[0])
    return (NULL);
  else
    return (password);
#endif /* WIN32 */
}


#ifdef HAVE_GSSAPI
/*
 * '_cupsGSSServiceName()' - Get the GSS (Kerberos) service name.
 */

const char *
_cupsGSSServiceName(void)
{
  _cups_globals_t *cg = _cupsGlobals();	/* Thread globals */


  if (!cg->gss_service_name[0])
    _cupsSetDefaults();

  return (cg->gss_service_name);
}
#endif /* HAVE_GSSAPI */


/*
 * '_cupsSetDefaults()' - Set the default server, port, and encryption.
 */

void
_cupsSetDefaults(void)
{
  cups_file_t	*fp;			/* File */
  const char	*home,			/* Home directory of user */
		*cups_encryption,	/* CUPS_ENCRYPTION env var */
		*cups_server,		/* CUPS_SERVER env var */
#ifdef HAVE_GSSAPI
		*cups_gssservicename,	/* CUPS_GSSSERVICENAME env var */
#endif /* HAVE_GSSAPI */
		*cups_anyroot,		/* CUPS_ANYROOT env var */
		*cups_expiredroot,	/* CUPS_EXPIREDROOT env var */
		*cups_expiredcerts;	/* CUPS_EXPIREDCERTS env var */
  char		filename[1024];		/* Filename */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  DEBUG_puts("_cupsSetDefaults()");

 /*
  * First collect environment variables...
  */

  cups_encryption     = getenv("CUPS_ENCRYPTION");
  cups_server	      = getenv("CUPS_SERVER");
#ifdef HAVE_GSSAPI
  cups_gssservicename = getenv("CUPS_GSSSERVICENAME");
#endif /* HAVE_GSSAPI */
  cups_anyroot	      = getenv("CUPS_ANYROOT");
  cups_expiredroot    = getenv("CUPS_EXPIREDROOT");
  cups_expiredcerts   = getenv("CUPS_EXPIREDCERTS");

 /*
  * Then, if needed, read the ~/.cups/client.conf or /etc/cups/client.conf
  * files to get the default values...
  */

  if (cg->encryption == (http_encryption_t)-1 || !cg->server[0] ||
      !cg->ipp_port)
  {
    if ((home = getenv("HOME")) != NULL)
    {
     /*
      * Look for ~/.cups/client.conf...
      */

      snprintf(filename, sizeof(filename), "%s/.cups/client.conf", home);
      fp = cupsFileOpen(filename, "r");
    }
    else
      fp = NULL;

    if (!fp)
    {
     /*
      * Look for CUPS_SERVERROOT/client.conf...
      */

      snprintf(filename, sizeof(filename), "%s/client.conf",
               cg->cups_serverroot);
      fp = cupsFileOpen(filename, "r");
    }

   /*
    * Read the configuration file and apply any environment variables; both
    * functions handle NULL cups_file_t pointers...
    */

    cups_read_client_conf(fp, cg, cups_encryption, cups_server,
#ifdef HAVE_GSSAPI
			  cups_gssservicename,
#endif /* HAVE_GSSAPI */
			  cups_anyroot, cups_expiredroot,
			  cups_expiredcerts);
    cupsFileClose(fp);
  }
}


/*
 * 'cups_read_client_conf()' - Read a client.conf file.
 */

static void
cups_read_client_conf(
    cups_file_t     *fp,		/* I - File to read */
    _cups_globals_t *cg,		/* I - Global data */
    const char      *cups_encryption,	/* I - CUPS_ENCRYPTION env var */
    const char      *cups_server,	/* I - CUPS_SERVER env var */
#ifdef HAVE_GSSAPI
    const char      *cups_gssservicename,
					/* I - CUPS_GSSSERVICENAME env var */
#endif /* HAVE_GSSAPI */
    const char	    *cups_anyroot,	/* I - CUPS_ANYROOT env var */
    const char	    *cups_expiredroot,	/* I - CUPS_EXPIREDROOT env var */
    const char	    *cups_expiredcerts)	/* I - CUPS_EXPIREDCERTS env var */
{
  int	linenum;			/* Current line number */
  char	line[1024],			/* Line from file */
        *value,				/* Pointer into line */
	encryption[1024],		/* Encryption value */
	server_name[1024],		/* ServerName value */
	any_root[1024],			/* AllowAnyRoot value */
	expired_root[1024],		/* AllowExpiredRoot value */
	expired_certs[1024];		/* AllowExpiredCerts value */
#ifdef HAVE_GSSAPI
  char	gss_service_name[32];		/* GSSServiceName value */
#endif /* HAVE_GSSAPI */


 /*
  * Read from the file...
  */

  linenum = 0;
  while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
  {
    if (!cups_encryption && cg->encryption == (http_encryption_t)-1 &&
        !strcasecmp(line, "Encryption") && value)
    {
      strlcpy(encryption, value, sizeof(encryption));
      cups_encryption = encryption;
    }
    else if (!cups_server && (!cg->server[0] || !cg->ipp_port) &&
             !strcasecmp(line, "ServerName") && value)
    {
      strlcpy(server_name, value, sizeof(server_name));
      cups_server = server_name;
    }
    else if (!cups_anyroot && !strcasecmp(line, "AllowAnyRoot") && value)
    {
      strlcpy(any_root, value, sizeof(any_root));
      cups_anyroot = any_root;
    }
    else if (!cups_expiredroot && !strcasecmp(line, "AllowExpiredRoot") &&
             value)
    {
      strlcpy(expired_root, value, sizeof(expired_root));
      cups_expiredroot = expired_root;
    }
    else if (!cups_expiredcerts && !strcasecmp(line, "AllowExpiredCerts") &&
             value)
    {
      strlcpy(expired_certs, value, sizeof(expired_certs));
      cups_expiredcerts = expired_certs;
    }
#ifdef HAVE_GSSAPI
    else if (!cups_gssservicename && !strcasecmp(line, "GSSServiceName") &&
             value)
    {
      strlcpy(gss_service_name, value, sizeof(gss_service_name));
      cups_gssservicename = gss_service_name;
    }
#endif /* HAVE_GSSAPI */
  }

 /*
  * Set values...
  */

  if (cg->encryption == (http_encryption_t)-1 && cups_encryption)
  {
    if (!strcasecmp(cups_encryption, "never"))
      cg->encryption = HTTP_ENCRYPT_NEVER;
    else if (!strcasecmp(cups_encryption, "always"))
      cg->encryption = HTTP_ENCRYPT_ALWAYS;
    else if (!strcasecmp(cups_encryption, "required"))
      cg->encryption = HTTP_ENCRYPT_REQUIRED;
    else
      cg->encryption = HTTP_ENCRYPT_IF_REQUESTED;
  }

  if ((!cg->server[0] || !cg->ipp_port) && cups_server)
  {
    if (!cg->server[0])
    {
     /*
      * Copy server name...
      */

      strlcpy(cg->server, cups_server, sizeof(cg->server));

      if (cg->server[0] != '/' && (value = strrchr(cg->server, ':')) != NULL &&
	  !strchr(value, ']') && isdigit(value[1] & 255))
        *value++ = '\0';
      else
        value = NULL;

      if (cg->server[0] == '/')
	strcpy(cg->servername, "localhost");
      else
	strlcpy(cg->servername, cg->server, sizeof(cg->servername));
    }
    else if (cups_server[0] != '/' &&
             (value = strrchr(cups_server, ':')) != NULL &&
	     !strchr(value, ']') && isdigit(value[1] & 255))
      value ++;
    else
      value = NULL;

    if (!cg->ipp_port && value)
      cg->ipp_port = atoi(value);
  }

  if (!cg->server[0])
  {
#ifdef CUPS_DEFAULT_DOMAINSOCKET
   /*
    * If we are compiled with domain socket support, only use the
    * domain socket if it exists and has the right permissions...
    */

    struct stat	sockinfo;		/* Domain socket information */

    if (!stat(CUPS_DEFAULT_DOMAINSOCKET, &sockinfo) &&
	(sockinfo.st_mode & S_IRWXO) == S_IRWXO)
      cups_server = CUPS_DEFAULT_DOMAINSOCKET;
    else
#endif /* CUPS_DEFAULT_DOMAINSOCKET */
      cups_server = "localhost";

    cupsSetServer(cups_server);
  }

  if (!cg->ipp_port)
  {
    const char		*ipp_port;	/* IPP_PORT environment variable */
    struct servent	*service;	/* Port number info */


    if ((ipp_port = getenv("IPP_PORT")) != NULL)
    {
      if ((cg->ipp_port = atoi(ipp_port)) <= 0)
        cg->ipp_port = CUPS_DEFAULT_IPP_PORT;
    }
    else if ((service = getservbyname("ipp", NULL)) == NULL ||
             service->s_port <= 0)
      cg->ipp_port = CUPS_DEFAULT_IPP_PORT;
    else
      cg->ipp_port = ntohs(service->s_port);
  }

#ifdef HAVE_GSSAPI
  if (!cups_gssservicename)
    cups_gssservicename = CUPS_DEFAULT_GSSSERVICENAME;

  strlcpy(cg->gss_service_name, cups_gssservicename,
	  sizeof(cg->gss_service_name));
#endif /* HAVE_GSSAPI */

  if (cups_anyroot)
    cg->any_root = !strcasecmp(cups_anyroot, "yes") ||
		   !strcasecmp(cups_anyroot, "on")  ||
		   !strcasecmp(cups_anyroot, "true");

  if (cups_expiredroot)
    cg->expired_root  = !strcasecmp(cups_expiredroot, "yes") ||
			!strcasecmp(cups_expiredroot, "on")  ||
			!strcasecmp(cups_expiredroot, "true");

  if (cups_expiredcerts)
    cg->expired_certs = !strcasecmp(cups_expiredcerts, "yes") ||
			!strcasecmp(cups_expiredcerts, "on")  ||
			!strcasecmp(cups_expiredcerts, "true");
}


/*
 * End of "$Id: usersys.c 8498 2009-04-13 17:03:15Z mike $".
 */
