/*
 * "$Id: listen.c,v 1.9.2.11 2003/07/20 03:13:10 mike Exp $"
 *
 *   Server listening routines for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 1997-2003 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
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
  int		i,		/* Looping var */
		val;		/* Parameter value */
  listener_t	*lis;		/* Current listening socket */
  struct hostent *host;		/* Host entry for server address */
  char		s[256];		/* String addresss */


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

  for (i = NumListeners, lis = Listeners; i > 0; i --, lis ++)
  {
    httpAddrString(&(lis->address), s, sizeof(s));

#ifdef AF_INET6
    if (lis->address.addr.sa_family == AF_INET6)
      LogMessage(L_DEBUG, "StartListening: address=%s port=%d (IPv6)", s,
		 ntohs(lis->address.ipv6.sin6_port));
    else
#endif /* AF_INET6 */
    LogMessage(L_DEBUG, "StartListening: address=%s port=%d", s,
	       ntohs(lis->address.ipv4.sin_port));

   /*
    * Save the first port that is bound to the local loopback or
    * "any" address...
    */

    if (httpAddrLocalhost(&(lis->address)) ||
        httpAddrAny(&(lis->address)))
    {
#ifdef AF_INET6
      if (lis->address.addr.sa_family == AF_INET6)
	LocalPort = ntohs(lis->address.ipv6.sin6_port);
      else
#endif /* AF_INET6 */
      LocalPort = ntohs(lis->address.ipv4.sin_port);
    }

   /*
    * Create a socket for listening...
    */

    if ((lis->fd = socket(lis->address.addr.sa_family, SOCK_STREAM, 0)) == -1)
    {
      LogMessage(L_ERROR, "StartListening: Unable to open listen socket - %s.",
                 strerror(errno));
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
    if (bind(lis->fd, (struct sockaddr *)&(lis->address),
	     lis->address.addr.sa_family == AF_INET ?
		 sizeof(lis->address.ipv4) :
		 sizeof(lis->address.ipv6)) < 0)
#else
    if (bind(lis->fd, (struct sockaddr *)&(lis->address),
             sizeof(lis->address.ipv4)) < 0)
#endif
    {
      LogMessage(L_ERROR, "StartListening: Unable to bind socket - %s.",
                 strerror(errno));
      exit(errno);
    }

   /*
    * Listen for new clients.
    */

    if (listen(lis->fd, ListenBackLog) < 0)
    {
      LogMessage(L_ERROR, "StartListening: Unable to listen for clients - %s.",
                 strerror(errno));
      exit(errno);
    }
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
#ifdef WIN32
    closesocket(lis->fd);
#else
    close(lis->fd);
#endif /* WIN32 */
}


/*
 * End of "$Id: listen.c,v 1.9.2.11 2003/07/20 03:13:10 mike Exp $".
 */
