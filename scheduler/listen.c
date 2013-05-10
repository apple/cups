/*
 * "$Id$"
 *
 *   Server listening routines for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cupsdDeleteAllListeners() - Delete all listeners.
 *   cupsdPauseListening()     - Clear input polling on all listening sockets...
 *   cupsdResumeListening()    - Set input polling on all listening sockets...
 *   cupsdStartListening()     - Create all listening sockets...
 *   cupsdStopListening()      - Close all listening sockets...
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * Make sure the IPV6_V6ONLY is defined on Linux - older versions of
 * glibc don't define it even if the kernel supports it...
 */

#if defined(__linux) && !defined(IPV6_V6ONLY)
#  define IPV6_V6ONLY 26
#endif /* __linux && !IPV6_V6ONLY */


/*
 * 'cupsdDeleteAllListeners()' - Delete all listeners.
 */

void
cupsdDeleteAllListeners(void)
{
  cupsd_listener_t	*lis;		/* Current listening socket */


  for (lis = (cupsd_listener_t *)cupsArrayFirst(Listeners);
       lis;
       lis = (cupsd_listener_t *)cupsArrayNext(Listeners))
    free(lis);

  cupsArrayDelete(Listeners);
  Listeners = NULL;
}


/*
 * 'cupsdPauseListening()' - Clear input polling on all listening sockets...
 */

void
cupsdPauseListening(void)
{
  cupsd_listener_t	*lis;		/* Current listening socket */


  if (cupsArrayCount(Listeners) < 1)
    return;

  if (cupsArrayCount(Clients) == MaxClients)
    cupsdLogMessage(CUPSD_LOG_WARN,
                    "Max clients reached, holding new connections...");
  else if (errno == ENFILE || errno == EMFILE)
    cupsdLogMessage(CUPSD_LOG_WARN,
                    "Too many open files, holding new connections for "
		    "30 seconds...");

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdPauseListening: Clearing input bits...");

  for (lis = (cupsd_listener_t *)cupsArrayFirst(Listeners);
       lis;
       lis = (cupsd_listener_t *)cupsArrayNext(Listeners))
    cupsdRemoveSelect(lis->fd);

  ListeningPaused = time(NULL) + 30;
}


/*
 * 'cupsdResumeListening()' - Set input polling on all listening sockets...
 */

void
cupsdResumeListening(void)
{
  cupsd_listener_t	*lis;		/* Current listening socket */


  if (cupsArrayCount(Listeners) < 1)
    return;

  cupsdLogMessage(CUPSD_LOG_INFO, "Resuming new connection processing...");
  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdResumeListening: Setting input bits...");

  for (lis = (cupsd_listener_t *)cupsArrayFirst(Listeners);
       lis;
       lis = (cupsd_listener_t *)cupsArrayNext(Listeners))
    cupsdAddSelect(lis->fd, (cupsd_selfunc_t)cupsdAcceptClient, NULL, lis);

  ListeningPaused = 0;
}


/*
 * 'cupsdStartListening()' - Create all listening sockets...
 */

void
cupsdStartListening(void)
{
  int			status;		/* Bind result */
  int			p,		/* Port number */
			val;		/* Parameter value */
  cupsd_listener_t	*lis;		/* Current listening socket */
  char			s[256];		/* String addresss */
  const char		*have_domain;	/* Have a domain socket? */
  static const char * const encryptions[] =
		{			/* Encryption values */
		  "IfRequested",
		  "Never",
		  "Required",
		  "Always"
		};


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdStartListening: %d Listeners",
                  cupsArrayCount(Listeners));

 /*
  * Get the server's IP address...
  */

  if (ServerAddrs)
    httpAddrFreeList(ServerAddrs);

  if ((ServerAddrs = httpAddrGetList(ServerName, AF_UNSPEC, NULL)) == NULL)
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to find IP address for server name \"%s\"!\n",
		    ServerName);

 /*
  * Setup socket listeners...
  */

  for (lis = (cupsd_listener_t *)cupsArrayFirst(Listeners), LocalPort = 0,
           have_domain = NULL;
       lis;
       lis = (cupsd_listener_t *)cupsArrayNext(Listeners))
  {
    httpAddrString(&(lis->address), s, sizeof(s));

#ifdef AF_INET6
    if (lis->address.addr.sa_family == AF_INET6)
      p = ntohs(lis->address.ipv6.sin6_port);
    else
#endif /* AF_INET6 */
#ifdef AF_LOCAL
    if (lis->address.addr.sa_family == AF_LOCAL)
      p = 0;
    else
#endif /* AF_LOCAL */
    p = ntohs(lis->address.ipv4.sin_port);

   /*
    * If needed, create a socket for listening...
    */

    if (lis->fd == -1)
    {
     /*
      * Create a socket for listening...
      */

      lis->fd = socket(lis->address.addr.sa_family, SOCK_STREAM, 0);

      if (lis->fd == -1)
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
			"Unable to open listen socket for address %s:%d - %s.",
			s, p, strerror(errno));
	continue;
      }

     /*
      * Set things up to reuse the local address for this port.
      */

      val = 1;
#ifdef __sun
      setsockopt(lis->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&val, sizeof(val));
#else
      setsockopt(lis->fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
#endif /* __sun */

     /*
      * Bind to the port we found...
      */

#ifdef AF_INET6
      if (lis->address.addr.sa_family == AF_INET6)
      {
#  ifdef IPV6_V6ONLY
       /*
	* Accept only IPv6 connections on this socket, to avoid
	* potential security issues and to make all platforms behave
	* the same.
	*/

	val = 1;
#    ifdef __sun
	setsockopt(lis->fd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&val, sizeof(val));
#    else
	setsockopt(lis->fd, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val));
#    endif /* __sun */
#  endif /* IPV6_V6ONLY */

	status = bind(lis->fd, (struct sockaddr *)&(lis->address),
		      httpAddrLength(&(lis->address)));
      }
      else
#endif /* AF_INET6 */
#ifdef AF_LOCAL
      if (lis->address.addr.sa_family == AF_LOCAL)
      {
	mode_t	mask;			/* Umask setting */


       /*
	* Remove any existing domain socket file...
	*/

	unlink(lis->address.un.sun_path);

       /*
	* Save the curent umask and set it to 0...
	*/

	mask = umask(0);

       /*
	* Bind the domain socket...
	*/

	status = bind(lis->fd, (struct sockaddr *)&(lis->address),
		      httpAddrLength(&(lis->address)));

       /*
	* Restore the umask...
	*/

	umask(mask);
      }
      else
#endif /* AF_LOCAL */
      status = bind(lis->fd, (struct sockaddr *)&(lis->address),
		    sizeof(lis->address.ipv4));

      if (status < 0)
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
			"Unable to bind socket for address %s:%d - %s.",
			s, p, strerror(errno));
	close(lis->fd);
	lis->fd = -1;
	continue;
      }

     /*
      * Listen for new clients.
      */

      if (listen(lis->fd, ListenBackLog) < 0)
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
			"Unable to listen for clients on address %s:%d - %s.",
			s, p, strerror(errno));
	exit(errno);
      }
    }

    fcntl(lis->fd, F_SETFD, fcntl(lis->fd, F_GETFD) | FD_CLOEXEC);

    if (p)
      cupsdLogMessage(CUPSD_LOG_INFO, "Listening to %s:%d on fd %d...",
        	      s, p, lis->fd);
    else
    {
      cupsdLogMessage(CUPSD_LOG_INFO, "Listening to %s on fd %d...",
        	      s, lis->fd);

      if (chmod(s, 0140777))
	cupsdLogMessage(CUPSD_LOG_ERROR,
			"Unable to change permisssions on domain socket "
			"\"%s\" - %s", s, strerror(errno));
    }

   /*
    * Save the first port that is bound to the local loopback or
    * "any" address...
    */

    if ((!LocalPort || LocalEncryption == HTTP_ENCRYPT_ALWAYS) && p > 0 &&
        (httpAddrLocalhost(&(lis->address)) ||
         httpAddrAny(&(lis->address))))
    {
      LocalPort       = p;
      LocalEncryption = lis->encryption;
    }

#ifdef AF_LOCAL
    if (lis->address.addr.sa_family == AF_LOCAL && !have_domain)
      have_domain = lis->address.un.sun_path;
#endif /* AF_LOCAL */
  }

 /*
  * Make sure that we are listening on localhost!
  */

  if (!LocalPort && !have_domain)
  {
    cupsdLogMessage(CUPSD_LOG_EMERG,
                    "No Listen or Port lines were found to allow access via "
		    "localhost!");

   /*
    * Commit suicide...
    */

    cupsdEndProcess(getpid(), 0);
  }

 /*
  * Set the CUPS_SERVER, IPP_PORT, and CUPS_ENCRYPTION variables based on
  * the listeners...
  */

  if (have_domain)
  {
   /*
    * Use domain sockets for the local connection...
    */

    cupsdSetEnv("CUPS_SERVER", have_domain);

    LocalEncryption = HTTP_ENCRYPT_IF_REQUESTED;
  }
  else
  {
   /*
    * Use the default local loopback address for the server...
    */

    cupsdSetEnv("CUPS_SERVER", "localhost");
  }

  cupsdSetEnv("CUPS_ENCRYPTION", encryptions[LocalEncryption]);

  if (LocalPort)
    cupsdSetEnvf("IPP_PORT", "%d", LocalPort);

 /*
  * Resume listening for connections...
  */

  cupsdResumeListening();
}


/*
 * 'cupsdStopListening()' - Close all listening sockets...
 */

void
cupsdStopListening(void)
{
  cupsd_listener_t	*lis;		/* Current listening socket */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdStopListening: closing all listen sockets.");

  cupsdPauseListening();

  for (lis = (cupsd_listener_t *)cupsArrayFirst(Listeners);
       lis;
       lis = (cupsd_listener_t *)cupsArrayNext(Listeners))
  {
    if (lis->fd != -1)
    {
#ifdef WIN32
      closesocket(lis->fd);
#else
      close(lis->fd);
#endif /* WIN32 */

#ifdef AF_LOCAL
     /*
      * Remove domain sockets...
      */

#  ifdef HAVE_LAUNCH_H
      if (lis->address.addr.sa_family == AF_LOCAL && !Launchd)
#  else
      if (lis->address.addr.sa_family == AF_LOCAL)
#  endif /* HAVE_LAUNCH_H */
	unlink(lis->address.un.sun_path);
#endif /* AF_LOCAL */
    }
  }
}


/*
 * End of "$Id$".
 */
