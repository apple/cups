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
 *   PauseListening()  - Clear input polling on all listening sockets...
 *   ResumeListening() - Set input polling on all listening sockets...
 *   StartListening()  - Create all listening sockets...
 *   StopListening()   - Close all listening sockets...
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * 'PauseListening()' - Clear input polling on all listening sockets...
 */

void
PauseListening(void)
{
  int		i;		/* Looping var */
  listener_t	*lis;		/* Current listening socket */


  if (NumListeners < 1 || !FD_ISSET(Listeners[0].fd, InputSet))
    return;

  if (NumClients == MaxClients)
    LogMessage(L_WARN, "Max clients reached, holding new connections...");

  LogMessage(L_DEBUG, "PauseListening: Clearing input bits...");

  for (i = NumListeners, lis = Listeners; i > 0; i --, lis ++)
  {
    LogMessage(L_DEBUG2, "PauseListening: Removing fd %d from InputSet...",
               lis->fd);

    FD_CLR(lis->fd, InputSet);
  }
}


/*
 * 'ResumeListening()' - Set input polling on all listening sockets...
 */

void
ResumeListening(void)
{
  int		i;		/* Looping var */
  listener_t	*lis;		/* Current listening socket */


  if (NumListeners < 1 || FD_ISSET(Listeners[0].fd, InputSet))
    return;

  if (NumClients >= (MaxClients - 1))
    LogMessage(L_WARN, "Resuming new connection processing...");

  LogMessage(L_DEBUG, "ResumeListening: Setting input bits...");

  for (i = NumListeners, lis = Listeners; i > 0; i --, lis ++)
  {
    LogMessage(L_DEBUG2, "ResumeListening: Adding fd %d to InputSet...",
               lis->fd);
    FD_SET(lis->fd, InputSet);
  }
}


/*
 * 'StartListening()' - Create all listening sockets...
 */

void
StartListening(void)
{
  int		status;			/* Bind result */
  int		i,			/* Looping var */
		p,			/* Port number */
		val;			/* Parameter value */
  listener_t	*lis;			/* Current listening socket */
  struct hostent *host;			/* Host entry for server address */
  char		s[256];			/* String addresss */
  int		have_domain;		/* Have a domain socket */


  LogMessage(L_DEBUG, "StartListening: NumListeners=%d", NumListeners);

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

    LogMessage(L_ERROR, "StartListening: Unable to find IP address for server name \"%s\" - %s\n",
               ServerName, hstrerror(h_errno));

    ServerAddr.ipv4.sin_family = AF_INET;
  }

 /*
  * Setup socket listeners...
  */

  for (i = NumListeners, lis = Listeners, LocalPort = 0, have_domain = 0;
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
      have_domain = 1;
      p           = 0;
    }
    else
#endif /* AF_LOCAL */
    p = ntohs(lis->address.ipv4.sin_port);

    LogMessage(L_DEBUG, "StartListening: address=%s port=%d", s, p);

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

    if ((lis->fd = socket(lis->address.addr.sa_family, SOCK_STREAM, 0)) == -1)
    {
      LogMessage(L_ERROR, "StartListening: Unable to open listen socket for address %s:%d - %s.",
                 s, p, strerror(errno));
      exit(errno);
    }

    LogMessage(L_DEBUG2, "StartListening: fd=%d", lis->fd);

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
	            sizeof(lis->address.ipv6));

      if (status < 0 &&
          (httpAddrLocalhost(&(lis->address)) ||
           httpAddrAny(&(lis->address))))
      {
       /*
        * Try binding to an IPv4 address instead...
	*/

        LogMessage(L_NOTICE, "StartListening: Unable to bind to IPv6 address, trying IPv4...");

        p = ntohs(lis->address.ipv6.sin6_port);

        if (httpAddrAny(&(lis->address)))
	  lis->address.ipv4.sin_addr.s_addr = htonl(0x00000000);
        else
	  lis->address.ipv4.sin_addr.s_addr = htonl(0x7f000001);

        lis->address.ipv4.sin_port  = htons(p);
	lis->address.addr.sa_family = AF_INET;

	status = bind(lis->fd, (struct sockaddr *)&(lis->address),
	              sizeof(lis->address.ipv4));
      }
#ifdef IPV6_V6ONLY
      else if (httpAddrLocalhost(&(lis->address)) ||
               httpAddrAny(&(lis->address)))
      {
       /*
        * Make sure that wildcard and loopback addresses accept
	* connections from both IPv6 and IPv4 clients.
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
	            SUN_LEN(&(lis->address.un)));

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
      LogMessage(L_ERROR, "StartListening: Unable to bind socket for address %s:%d - %s.",
                 s, p, strerror(errno));
      exit(errno);
    }

   /*
    * Listen for new clients.
    */

    if (listen(lis->fd, ListenBackLog) < 0)
    {
      LogMessage(L_ERROR, "StartListening: Unable to listen for clients on address %s:%d - %s.",
                 s, p, strerror(errno));
      exit(errno);
    }
  }

 /*
  * Make sure that we are listening on localhost!
  */

  if (!LocalPort && !have_domain)
  {
    LogMessage(L_EMERG, "No Listen or Port lines were found to allow access via localhost!");

   /*
    * Commit suicide...
    */

    cupsdEndProcess(getpid(), 0);
  }

  ResumeListening();
}


/*
 * 'StopListening()' - Close all listening sockets...
 */

void
StopListening(void)
{
  int		i;		/* Looping var */
  listener_t	*lis;		/* Current listening socket */


  LogMessage(L_DEBUG, "StopListening: closing all listen sockets.");

  PauseListening();

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
