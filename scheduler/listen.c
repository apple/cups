/*
 * "$Id: listen.c,v 1.6 2000/09/14 18:54:14 mike Exp $"
 *
 *   Server listening routines for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 1997-2000 by Easy Software Products, all rights reserved.
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


  if (!FD_ISSET(Listeners[0].fd, &InputSet))
    return;

  LogMessage(L_DEBUG, "PauseListening() clearing input bits...");

  for (i = NumListeners, lis = Listeners; i > 0; i --, lis ++)
    FD_CLR(lis->fd, &InputSet);
}


/*
 * 'ResumeListening()' - Set input polling on all listening sockets...
 */

void
ResumeListening(void)
{
  int		i;		/* Looping var */
  listener_t	*lis;		/* Current listening socket */


  if (FD_ISSET(Listeners[0].fd, &InputSet))
    return;

  LogMessage(L_DEBUG, "ResumeListening() setting input bits...");

  for (i = NumListeners, lis = Listeners; i > 0; i --, lis ++)
    FD_SET(lis->fd, &InputSet);
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


  LogMessage(L_DEBUG, "StartListening() NumListeners=%d", NumListeners);

 /*
  * Get the server's IP address...
  */

  memset(&ServerAddr, 0, sizeof(ServerAddr));

  if ((host = gethostbyname(ServerName)) != NULL)
  {
   /*
    * Found the server's address!
    */

    memcpy((char *)&(ServerAddr.sin_addr), host->h_addr, host->h_length);
    ServerAddr.sin_family = host->h_addrtype;
  }
  else
  {
   /*
    * Didn't find it!  Use an address of 0...
    */

    LogMessage(L_ERROR, "StartListening() Unable to find IP address for server name \"%s\"!\n",
               ServerName);

    ServerAddr.sin_family = AF_INET;
  }

 /*
  * Setup socket listeners...
  */

  for (i = NumListeners, lis = Listeners; i > 0; i --, lis ++)
  {
    LogMessage(L_DEBUG, "StartListening() address=%08x port=%d",
               ntohl(lis->address.sin_addr.s_addr),
	       ntohs(lis->address.sin_port));

    if ((lis->fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
      LogMessage(L_ERROR, "StartListening() Unable to open listen socket - %s.",
                 strerror(errno));
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

    if (bind(lis->fd, (struct sockaddr *)&(lis->address), sizeof(lis->address)) < 0)
    {
      LogMessage(L_ERROR, "StartListening() Unable to bind socket - %s.", strerror(errno));
      exit(errno);
    }

   /*
    * Listen for new clients.
    */

    if (listen(lis->fd, SOMAXCONN) < 0)
    {
      LogMessage(L_ERROR, "StartListening() Unable to listen for clients - %s.",
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


  LogMessage(L_DEBUG, "StopListening()");

  PauseListening();

  for (i = NumListeners, lis = Listeners; i > 0; i --, lis ++)
#if defined(WIN32) || defined(__EMX__)
    closesocket(lis->fd);
#else
    close(lis->fd);
#endif /* WIN32 || __EMX__ */
}


/*
 * End of "$Id: listen.c,v 1.6 2000/09/14 18:54:14 mike Exp $".
 */
