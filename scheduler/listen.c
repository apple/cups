/*
 * "$Id$"
 *
 *   Server listening routines for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   cupsdPauseListening()  - Clear input polling on all listening sockets...
 *   cupsdResumeListening() - Set input polling on all listening sockets...
 *   cupsdStartListening()  - Create all listening sockets...
 *   cupsdStopListening()   - Close all listening sockets...
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * 'cupsdPauseListening()' - Clear input polling on all listening sockets...
 */

void
cupsdPauseListening(void)
{
  int		i;		/* Looping var */
  cupsd_listener_t	*lis;		/* Current listening socket */


  if (NumListeners < 1 || !FD_ISSET(Listeners[0].fd, InputSet))
    return;

  if (NumClients == MaxClients)
    cupsdLogMessage(L_WARN, "Max clients reached, holding new connections...");

  cupsdLogMessage(L_DEBUG, "cupsdPauseListening: Clearing input bits...");

  for (i = NumListeners, lis = Listeners; i > 0; i --, lis ++)
  {
    cupsdLogMessage(L_DEBUG2, "cupsdPauseListening: Removing fd %d from InputSet...",
               lis->fd);

    FD_CLR(lis->fd, InputSet);
  }
}


/*
 * 'cupsdResumeListening()' - Set input polling on all listening sockets...
 */

void
cupsdResumeListening(void)
{
  int		i;		/* Looping var */
  cupsd_listener_t	*lis;		/* Current listening socket */


  if (NumListeners < 1 || FD_ISSET(Listeners[0].fd, InputSet))
    return;

  if (NumClients >= (MaxClients - 1))
    cupsdLogMessage(L_WARN, "Resuming new connection processing...");

  cupsdLogMessage(L_DEBUG, "cupsdResumeListening: Setting input bits...");

  for (i = NumListeners, lis = Listeners; i > 0; i --, lis ++)
  {
    cupsdLogMessage(L_DEBUG2, "cupsdResumeListening: Adding fd %d to InputSet...",
               lis->fd);
    FD_SET(lis->fd, InputSet);
  }
}


/*
 * 'cupsdStartListening()' - Create all listening sockets...
 */

void
cupsdStartListening(void)
{
  int		status;			/* Bind result */
  int		i,			/* Looping var */
		p,			/* Port number */
		val;			/* Parameter value */
  cupsd_listener_t	*lis;			/* Current listening socket */
  struct hostent *host;			/* Host entry for server address */
  char		s[256];			/* String addresss */
  const char	*have_domain;		/* Have a domain socket? */
  static const char * const encryptions[] =
		{			/* Encryption values */
		  "IfRequested",
		  "Never",
		  "Required",
		  "Always"
		};


  cupsdLogMessage(L_DEBUG, "cupsdStartListening: NumListeners=%d", NumListeners);

 /*
  * Get the server's IP address...
  */

  memset(&ServerAddr, 0, sizeof(ServerAddr));

  if ((host = httpGetHostByName(ServerName)) != NULL)
  {
   /*
    * Found the server's address!
    */

    httpAddrLoad(host, 0, 0, &ServerAddr);
  }
  else
  {
   /*
    * Didn't find it!  Use an address of 0...
    */

    cupsdLogMessage(L_ERROR, "cupsdStartListening: Unable to find IP address for server name \"%s\" - %s\n",
               ServerName, hstrerror(h_errno));

    ServerAddr.ipv4.sin_family = AF_INET;
  }

 /*
  * Setup socket listeners...
  */

  for (i = NumListeners, lis = Listeners, LocalPort = 0, have_domain = NULL;
       i > 0; i --, lis ++)
  {
    httpAddrString(&(lis->address), s, sizeof(s));

#ifdef AF_INET6
    if (lis->address.addr.sa_family == AF_INET6)
      p = ntohs(lis->address.ipv6.sin6_port);
    else
#endif /* AF_INET6 */
#ifdef AF_LOCAL
    if (lis->address.addr.sa_family == AF_LOCAL)
    {
      have_domain = lis->address.un.sun_path;
      p           = 0;
    }
    else
#endif /* AF_LOCAL */
    p = ntohs(lis->address.ipv4.sin_port);

   /*
    * Save the first port that is bound to the local loopback or
    * "any" address...
    */

    if (!LocalPort && p > 0 &&
        (httpAddrLocalhost(&(lis->address)) ||
         httpAddrAny(&(lis->address))))
    {
      LocalPort       = p;
      LocalEncryption = lis->encryption;
    }

   /*
    * Create a socket for listening...
    */

    lis->fd = socket(lis->address.addr.sa_family, SOCK_STREAM, 0);

#ifdef AF_INET6
    if (lis->fd == -1 && lis->address.addr.sa_family == AF_INET6 &&
        (httpAddrLocalhost(&(lis->address)) || httpAddrAny(&(lis->address))))
    {
     /*
      * Try binding to an IPv4 address instead...
      */

      cupsdLogMessage(L_NOTICE, "cupsdStartListening: Unable to use IPv6 address, trying IPv4...");

      p = ntohs(lis->address.ipv6.sin6_port);

      if (httpAddrAny(&(lis->address)))
	lis->address.ipv4.sin_addr.s_addr = htonl(0x00000000);
      else
	lis->address.ipv4.sin_addr.s_addr = htonl(0x7f000001);

      lis->address.ipv4.sin_port  = htons(p);
      lis->address.addr.sa_family = AF_INET;

      lis->fd = socket(lis->address.addr.sa_family, SOCK_STREAM, 0);
    }
#endif /* AF_INET6 */

    if (lis->fd == -1)
    {
      cupsdLogMessage(L_ERROR, "cupsdStartListening: Unable to open listen socket for address %s:%d - %s.",
                 s, p, strerror(errno));
      exit(errno);
    }

    fcntl(lis->fd, F_SETFD, fcntl(lis->fd, F_GETFD) | FD_CLOEXEC);

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
      status = bind(lis->fd, (struct sockaddr *)&(lis->address),
	            httpAddrLength(&(lis->address)));

#ifdef IPV6_V6ONLY
      if (status >= 0 &&
          (httpAddrLocalhost(&(lis->address)) || httpAddrAny(&(lis->address))))
      {
       /*
        * Make sure that wildcard and loopback addresses accept
	* connections from both IPv6 and IPv4 clients.
	*
	* NOTE: This DOES NOT WORK for OpenBSD, since they adopted a
	*       stricter behavior in the name of security.  For OpenBSD,
	*       you must list IPv4 and IPv6 listen addresses separately.
	*/

        val = 0;
#  ifdef __sun
        setsockopt(lis->fd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&val, sizeof(val));
#  else
        setsockopt(lis->fd, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val));
#  endif /* __sun */
      }
#endif /* IPV6_V6ONLY */
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
      cupsdLogMessage(L_ERROR, "cupsdStartListening: Unable to bind socket for address %s:%d - %s.",
                 s, p, strerror(errno));
      exit(errno);
    }

   /*
    * Listen for new clients.
    */

    if (listen(lis->fd, ListenBackLog) < 0)
    {
      cupsdLogMessage(L_ERROR, "cupsdStartListening: Unable to listen for clients on address %s:%d - %s.",
                 s, p, strerror(errno));
      exit(errno);
    }

    if (p)
      cupsdLogMessage(L_INFO, "cupsdStartListening: Listening to %s:%d on fd %d...",
        	 s, p, lis->fd);
    else
      cupsdLogMessage(L_INFO, "cupsdStartListening: Listening to %s on fd %d...",
        	 s, lis->fd);
  }

 /*
  * Make sure that we are listening on localhost!
  */

  if (!LocalPort && !have_domain)
  {
    cupsdLogMessage(L_EMERG, "No Listen or Port lines were found to allow access via localhost!");

   /*
    * Commit suicide...
    */

    cupsdEndProcess(getpid(), 0);
  }

 /*
  * Set the CUPS_SERVER and IPP_PORT variables based on the listeners...
  */

  if (have_domain)
  {
   /*
    * Use domain sockets for the local connection...
    */

    cupsdSetEnv("CUPS_SERVER", have_domain);
  }
  else
  {
   /*
    * Use the default local loopback address for the server...
    */

    cupsdSetEnv("CUPS_SERVER", "localhost");
    cupsdSetEnvf("IPP_PORT", "%d", LocalPort);
    cupsdSetEnv("CUPS_ENCRYPTION", encryptions[LocalEncryption]);
  }

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
  int		i;		/* Looping var */
  cupsd_listener_t	*lis;		/* Current listening socket */


  cupsdLogMessage(L_DEBUG, "cupsdStopListening: closing all listen sockets.");

  cupsdPauseListening();

  for (i = NumListeners, lis = Listeners; i > 0; i --, lis ++)
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

    if (lis->address.addr.sa_family == AF_LOCAL)
      unlink(lis->address.un.sun_path);
#endif /* AF_LOCAL */
  }
}


/*
 * End of "$Id$".
 */
