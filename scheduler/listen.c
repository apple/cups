/*
 * "$Id: listen.c 12178 2014-09-30 18:56:48Z msweet $"
 *
 * Server listening routines for the CUPS scheduler.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
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
#if defined(HAVE_LAUNCHD) || defined(HAVE_SYSTEMD)
    if (!lis->on_demand)
#endif /* HAVE_LAUNCHD || HAVE_SYSTEMD */
    {
      cupsArrayRemove(Listeners, lis);
      free(lis);
    }

  if (cupsArrayCount(Listeners) == 0)
  {
    cupsArrayDelete(Listeners);
    Listeners = NULL;
  }
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
  int			p;		/* Port number */
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
  * Setup socket listeners...
  */

  for (lis = (cupsd_listener_t *)cupsArrayFirst(Listeners), LocalPort = 0,
           have_domain = NULL;
       lis;
       lis = (cupsd_listener_t *)cupsArrayNext(Listeners))
  {
    httpAddrString(&(lis->address), s, sizeof(s));
    p = httpAddrPort(&(lis->address));

   /*
    * If needed, create a socket for listening...
    */

    if (lis->fd == -1)
    {
     /*
      * Create a socket for listening...
      */

      lis->fd = httpAddrListen(&(lis->address), p);

      if (lis->fd == -1)
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
			"Unable to open listen socket for address %s:%d - %s.",
			s, p, strerror(errno));

#ifdef AF_INET6
       /*
        * IPv6 is often disabled while DNS returns IPv6 addresses...
	*/

	if (lis->address.addr.sa_family != AF_INET6 &&
	    (FatalErrors & CUPSD_FATAL_LISTEN))
	  cupsdEndProcess(getpid(), 0);
#else
	if (FatalErrors & CUPSD_FATAL_LISTEN)
	  cupsdEndProcess(getpid(), 0);
#endif /* AF_INET6 */

	continue;
      }
    }

    if (p)
      cupsdLogMessage(CUPSD_LOG_INFO, "Listening to %s:%d on fd %d...",
        	      s, p, lis->fd);
    else
      cupsdLogMessage(CUPSD_LOG_INFO, "Listening to %s on fd %d...",
        	      s, lis->fd);

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
		    "localhost.");

    if (FatalErrors & (CUPSD_FATAL_CONFIG | CUPSD_FATAL_LISTEN))
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
#if defined(HAVE_LAUNCHD) || defined(HAVE_SYSTEMD)
    if (!lis->on_demand && lis->fd != -1)
    {
      httpAddrClose(&(lis->address), lis->fd);
      lis->fd = -1;
    }

#else
    if (lis->fd != -1)
    {
      httpAddrClose(&(lis->address), lis->fd);
      lis->fd = -1;
    }
#endif /* HAVE_LAUNCHD || HAVE_SYSTEMD */
  }
}


/*
 * End of "$Id: listen.c 12178 2014-09-30 18:56:48Z msweet $".
 */
