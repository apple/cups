/*
 * "$Id: dirsvc.c,v 1.12 1999/04/23 17:09:19 mike Exp $"
 *
 *   Directory services routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *
 */

/*
 * Include necessary headers...
 */

#define DEBUG
#include "cupsd.h"


/*
 * 'StartBrowsing()' - Start sending and receiving broadcast information.
 */

void
StartBrowsing(void)
{
  int			val;	/* Socket option value */
  struct sockaddr_in	addr;	/* Broadcast address */


  if (!Browsing)
    return;

 /*
  * Create the broadcast socket...
  */

  if ((BrowseSocket = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
  {
    LogMessage(LOG_ERROR, "StartBrowsing: Unable to create broadcast socket - %s.",
               strerror(errno));
    return;
  }

 /*
  * Set the "broadcast" and "allow port reuse" flags...
  */

  val = 1;
  if (setsockopt(BrowseSocket, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)))
    perror("StartBrowsing/SO_BROADCAST");

 /*
  * Bind the socket to browse port...
  */

  memset(&addr, 0, sizeof(addr));
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons(BrowsePort);

  if (bind(BrowseSocket, &addr, sizeof(addr)))
  {
    LogMessage(LOG_ERROR, "StartBrowsing: Unable to bind broadcast socket - %s.",
               strerror(errno));

#if defined(WIN32) || defined(__EMX__)
    closesocket(BrowseSocket);
#else
    close(BrowseSocket);
#endif /* WIN32 || __EMX__ */

    BrowseSocket = -1;
    return;
  }

 /*
  * Finally, add the socket to the input selection set...
  */

  FD_SET(BrowseSocket, &InputSet);
}


/*
 * 'StopBrowsing()' - Stop sending and receiving broadcast information.
 */

void
StopBrowsing(void)
{
  if (!Browsing)
    return;

 /*
  * Close the socket and remove it from the input selection set.
  */

  if (BrowseSocket >= 0)
  {
#if defined(WIN32) || defined(__EMX__)
    closesocket(BrowseSocket);
#else
    close(BrowseSocket);
#endif /* WIN32 || __EMX__ */

    FD_CLR(BrowseSocket, &InputSet);
  }
}


/*
 * 'UpdateBrowseList()' - Update the browse lists for any new browse data.
 */

void
UpdateBrowseList(void)
{
  int		bytes;			/* Number of bytes left */
  char		packet[1540];		/* Broadcast packet */
  cups_ptype_t	type;			/* Printer type */
  ipp_pstate_t	state;			/* Printer state */
  char		uri[HTTP_MAX_URI],	/* Printer URI */
		method[HTTP_MAX_URI],	/* Method portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  int		port;			/* Port portion of URI */
  char		name[IPP_MAX_NAME],	/* Name of printer */
		*ptr;			/* Pointer into hostname */
  printer_t	*p;			/* Printer information */


 /*
  * Read a packet from the browse socket...
  */

  if ((bytes = recv(BrowseSocket, packet, sizeof(packet), 0)) <= 0)
  {
    LogMessage(LOG_ERROR, "UpdateBrowseList: recv failed - %s.",
               strerror(errno));
    return;
  }

  packet[bytes] = '\0';
  DEBUG_printf(("UpdateBrowseList: (%d bytes) %s", bytes, packet));

  if (sscanf(packet, "%x%x%s", &type, &state, uri) != 3)
  {
    LogMessage(LOG_WARN, "UpdateBrowseList: Garbled browse packet - %s",
               packet);
    return;
  }

 /*
  * Pull the URI apart to see if this is a local or remote printer...
  */

  httpSeparate(uri, method, username, host, &port, resource);

  if (strcasecmp(host, ServerName) == 0)
    return;

 /*
  * OK, this isn't a local printer; see if we already have it listed in
  * the Printers list, and add it if not...
  */

  type += CUPS_PRINTER_REMOTE;

  if ((ptr = strchr(host, '.')) != NULL)
    *ptr = '\0';

  if (strncmp(resource, "/printers/", 10) == 0)
    sprintf(name, "%s@%s", resource + 10, host);
  else if (strncmp(resource, "/classes/", 9) == 0)
    sprintf(name, "%s@%s", resource + 9, host);
  else
    return;

  if ((p = FindPrinter(name)) == NULL)
  {
   /*
    * Printer doesn't exist; add it...
    */

    p = AddPrinter(name);

   /*
    * First the URI to point to the real server...
    */

    strcpy(p->uri, uri);
    free(p->attrs->attrs->values[0].string.text);
    p->attrs->attrs->values[0].string.text = strdup(uri);
  }

 /*
  * Update the state...
  */

  p->type        = type;
  p->state       = state;
  p->browse_time = time(NULL);
}


/*
 * 'SendBrowseList()' - Send new browsing information.
 */

void
SendBrowseList(void)
{
  int			i;	/* Looping var */
  printer_t		*p,	/* Current printer */
			*np;	/* Next printer */
  time_t		ut,	/* Minimum update time */
			to;	/* Timeout time */
  int			bytes;	/* Length of packet */
  char			packet[1540];
				/* Browse data packet */


 /*
  * Compute the update time...
  */

  ut = time(NULL) - BrowseInterval;
  to = time(NULL) - BrowseTimeout;

 /*
  * Loop through all of the printers and send local updates as needed...
  */

  for (p = Printers; p != NULL; p = np)
  {
    np = p->next;

    if (p->type & CUPS_PRINTER_REMOTE)
    {
     /*
      * See if this printer needs to be timed out...
      */

      if (p->browse_time < to)
        DeletePrinter(p);
    }
    else if (p->browse_time < ut)
    {
     /*
      * Need to send an update...
      */

      p->browse_time = time(NULL);

      sprintf(packet, "%x %x %s\n", p->type, p->state, p->uri);
      bytes = strlen(packet);
      DEBUG_printf(("SendBrowseList: (%d bytes) %s", bytes, packet));

     /*
      * Send a packet to each browse address...
      */

      for (i = 0; i < NumBrowsers; i ++)
	if (sendto(BrowseSocket, packet, bytes, 0, Browsers + i,
	           sizeof(Browsers[0])) <= 0)
	  LogMessage(LOG_ERROR, "SendBrowseList: sendto failed for browser %d - %s.",
	           i + 1, strerror(errno));
    }
  }
}


/*
 * End of "$Id: dirsvc.c,v 1.12 1999/04/23 17:09:19 mike Exp $".
 */
