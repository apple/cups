/*
 * "$Id: dirsvc.c,v 1.73.2.5 2002/01/02 18:05:01 mike Exp $"
 *
 *   Directory services routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products, all rights reserved.
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
 *   ProcessBrowseData() - Process new browse data.
 *   SendBrowseList()    - Send new browsing information as necessary.
 *   SendCUPSBrowse()    - Send new browsing information using the CUPS protocol.
 *   StartBrowsing()     - Start sending and receiving broadcast information.
 *   StartPolling()      - Start polling servers as needed.
 *   StopBrowsing()      - Stop sending and receiving broadcast information.
 *   StopPolling()       - Stop polling servers as needed.
 *   UpdateCUPSBrowse()  - Update the browse lists using the CUPS protocol.
 *   RegReportCallback() - Empty SLPRegReport.
 *   SendSLPBrowse()     - Register the specified printer with SLP.
 *   SLPDeregPrinter()   - SLPDereg() the specified printer
 *   GetSlpAttrVal()     - Get an attribute from an SLP registration.
 *   AttrCallback()      - SLP attribute callback 
 *   SrvUrlCallback()    - SLP service url callback
 *   UpdateSLPBrowse()   - Get browsing information via SLP.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <grp.h>


/*
 * 'ProcessBrowseData()' - Process new browse data.
 */

void
ProcessBrowseData(const char   *uri,	/* I - URI of printer/class */
                  cups_ptype_t type,	/* I - Printer type */
		  ipp_pstate_t state,	/* I - Printer state */
                  const char   *location,/* I - Printer location */
		  const char   *info,	/* I - Printer information */
                  const char   *make_model) /* I - Printer make and model */
{
  int		i;			/* Looping var */
  int		update;			/* Update printer attributes? */
  char		method[HTTP_MAX_URI],	/* Method portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  int		port;			/* Port portion of URI */
  char		name[IPP_MAX_NAME],	/* Name of printer */
		*hptr,			/* Pointer into hostname */
		*sptr;			/* Pointer into ServerName */
  char		local_make_model[IPP_MAX_NAME];
					/* Local make and model */
  printer_t	*p,			/* Printer information */
		*pclass,		/* Printer class */
		*first,			/* First printer in class */
		*next;			/* Next printer in list */
  int		offset,			/* Offset of name */
		len;			/* Length of name */


 /*
  * Pull the URI apart to see if this is a local or remote printer...
  */

  httpSeparate(uri, method, username, host, &port, resource);

 /*
  * OK, this isn't a local printer; see if we already have it listed in
  * the Printers list, and add it if not...
  */

  update = 0;
  hptr   = strchr(host, '.');
  sptr   = strchr(ServerName, '.');

  if (sptr != NULL && hptr != NULL)
  {
   /*
    * Strip the common domain name components...
    */

    while (hptr != NULL)
    {
      if (strcasecmp(hptr, sptr) == 0)
      {
        *hptr = '\0';
	break;
      }
      else
        hptr = strchr(hptr + 1, '.');
    }
  }

  if (type & CUPS_PRINTER_CLASS)
  {
   /*
    * Remote destination is a class...
    */

    if (strncmp(resource, "/classes/", 9) == 0)
      snprintf(name, sizeof(name), "%s@%s", resource + 9, host);
    else
      return;

    if ((p = FindClass(name)) == NULL && BrowseShortNames)
    {
      if ((p = FindClass(resource + 9)) != NULL)
      {
        if (strcasecmp(p->hostname, host) != 0 && p->hostname[0])
	{
	 /*
	  * Nope, this isn't the same host; if the hostname isn't the local host,
	  * add it to the other class and then find a class using the full host
	  * name...
	  */

	  if (p->type & CUPS_PRINTER_REMOTE)
	  {
	    /* No "p->var[sizeof(p->var) - 1] = '\0';" because p is zeroed */
            strncat(p->name, "@", sizeof(p->name) - 1);
	    strncat(p->name, p->hostname, sizeof(p->name) - 1);
	    SetPrinterAttrs(p);
	    SortPrinters();
	  }

          p = NULL;
	}
	else if (!p->hostname[0])
	{
	  /* No "p->var[sizeof(p->var) - 1] = '\0';" because p is zeroed */
          strncpy(p->hostname, host, sizeof(p->hostname) - 1);
	  strncpy(p->uri, uri, sizeof(p->uri) - 1);
	  strncpy(p->device_uri, uri, sizeof(p->device_uri) - 1);
          update = 1;
        }
      }
      else
      {
        strncpy(name, resource + 9, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
      }
    }
    else if (p != NULL && !p->hostname[0])
    {
      /* No "p->var[sizeof(p->var) - 1] = '\0';" because p is zeroed */
      strncpy(p->hostname, host, sizeof(p->hostname) - 1);
      strncpy(p->uri, uri, sizeof(p->uri) - 1);
      strncpy(p->device_uri, uri, sizeof(p->device_uri) - 1);
      update = 1;
    }

    if (p == NULL)
    {
     /*
      * Class doesn't exist; add it...
      */

      p = AddClass(name);

      LogMessage(L_INFO, "Added remote class \"%s\"...", name);

     /*
      * Force the URI to point to the real server...
      */

      p->type = type;
      /* No "p->var[sizeof(p->var) - 1] = '\0';" because p is zeroed */
      strncpy(p->uri, uri, sizeof(p->uri) - 1);
      strncpy(p->device_uri, uri, sizeof(p->device_uri) - 1);
      strncpy(p->hostname, host, sizeof(p->hostname) - 1);

      update = 1;
    }
  }
  else
  {
   /*
    * Remote destination is a printer...
    */

    if (strncmp(resource, "/printers/", 10) == 0)
      snprintf(name, sizeof(name), "%s@%s", resource + 10, host);
    else
      return;

    if ((p = FindPrinter(name)) == NULL && BrowseShortNames)
    {
      if ((p = FindPrinter(resource + 10)) != NULL)
      {
        if (strcasecmp(p->hostname, host) != 0 && p->hostname[0])
	{
	 /*
	  * Nope, this isn't the same host; if the hostname isn't the local host,
	  * add it to the other printer and then find a printer using the full host
	  * name...
	  */

	  if (p->type & CUPS_PRINTER_REMOTE)
	  {
	    /* No "p->var[sizeof(p->var) - 1] = '\0';" because p is zeroed */
            strncat(p->name, "@", sizeof(p->name) - 1);
	    strncat(p->name, p->hostname, sizeof(p->name) - 1);
	    SetPrinterAttrs(p);
	    SortPrinters();
	  }

          p = NULL;
	}
	else if (!p->hostname[0])
	{
	  /* No "p->var[sizeof(p->var) - 1] = '\0';" because p is zeroed */
          strncpy(p->hostname, host, sizeof(p->hostname) - 1);
	  strncpy(p->uri, uri, sizeof(p->uri) - 1);
	  strncpy(p->device_uri, uri, sizeof(p->device_uri) - 1);
          update = 1;
        }
      }
      else
      {
        strncpy(name, resource + 10, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
      }
    }
    else if (p != NULL && !p->hostname[0])
    {
      /* No "p->var[sizeof(p->var) - 1] = '\0';" because p is zeroed */
      strncpy(p->hostname, host, sizeof(p->hostname) - 1);
      strncpy(p->uri, uri, sizeof(p->uri) - 1);
      strncpy(p->device_uri, uri, sizeof(p->device_uri) - 1);
      update = 1;
    }

    if (p == NULL)
    {
     /*
      * Printer doesn't exist; add it...
      */

      p = AddPrinter(name);

      LogMessage(L_INFO, "Added remote printer \"%s\"...", name);

     /*
      * Force the URI to point to the real server...
      */

      p->type = type;
      /* No "p->var[sizeof(p->var) - 1] = '\0';" because p is zeroed */
      strncpy(p->hostname, host, sizeof(p->hostname) - 1);
      strncpy(p->uri, uri, sizeof(p->uri) - 1);
      strncpy(p->device_uri, uri, sizeof(p->device_uri) - 1);

      update = 1;
    }
  }

 /*
  * Update the state...
  */

  p->state       = state;
  p->accepting   = state != IPP_PRINTER_STOPPED;
  p->browse_time = time(NULL);

  if (p->type != type)
  {
    p->type = type;
    update  = 1;
  }

  if (strcmp(p->location, location))
  {
    /* No "p->var[sizeof(p->var) - 1] = '\0';" because p is zeroed */
    strncpy(p->location, location, sizeof(p->location) - 1);
    update = 1;
  }

  if (strcmp(p->info, info))
  {
    /* No "p->var[sizeof(p->var) - 1] = '\0';" because p is zeroed */
    strncpy(p->info, info, sizeof(p->info) - 1);
    update = 1;
  }

  if (!make_model[0])
  {
    if (type & CUPS_PRINTER_CLASS)
      snprintf(local_make_model, sizeof(local_make_model),
               "Remote Class on %s", host);
    else
      snprintf(local_make_model, sizeof(local_make_model),
               "Remote Printer on %s", host);
  }
  else
    snprintf(local_make_model, sizeof(local_make_model),
             "%s on %s", make_model, host);

  if (strcmp(p->make_model, local_make_model))
  {
    /* No "p->var[sizeof(p->var) - 1] = '\0';" because p is zeroed */
    strncpy(p->make_model, local_make_model, sizeof(p->make_model) - 1);
    update = 1;
  }

  if (update)
    SetPrinterAttrs(p);

 /*
  * See if we have a default printer...  If not, make the first printer the
  * default.
  */

  if (DefaultPrinter == NULL && Printers != NULL)
    DefaultPrinter = Printers;

 /*
  * Do auto-classing if needed...
  */

  if (ImplicitClasses)
  {
   /*
    * Loop through all available printers and create classes as needed...
    */

    for (p = Printers, len = 0, offset = 0, first = NULL;
         p != NULL;
	 p = next)
    {
     /*
      * Get next printer in list...
      */

      next = p->next;

     /*
      * Skip classes...
      */

      if (p->type & (CUPS_PRINTER_IMPLICIT | CUPS_PRINTER_CLASS))
      {
        len = 0;
        continue;
      }

     /*
      * If len == 0, get the length of this printer name up to the "@"
      * sign (if any).
      */

      if (len > 0 &&
	  strncasecmp(p->name, name + offset, len) == 0 &&
	  (p->name[len] == '\0' || p->name[len] == '@'))
      {
       /*
	* We have more than one printer with the same name; see if
	* we have a class, and if this printer is a member...
	*/

        if ((pclass = FindPrinter(name)) == NULL)
	{
	 /*
	  * Need to add the class...
	  */

	  pclass = AddPrinter(name);
	  pclass->type      |= CUPS_PRINTER_IMPLICIT;
	  pclass->accepting = 1;
	  pclass->state     = IPP_PRINTER_IDLE;

          SetPrinterAttrs(pclass);

          LogMessage(L_INFO, "Added implicit class \"%s\"...", name);
	}

        if (first != NULL)
	{
          for (i = 0; i < pclass->num_printers; i ++)
	    if (pclass->printers[i] == first)
	      break;

          if (i >= pclass->num_printers)
	    AddPrinterToClass(pclass, first);

	  first = NULL;
	}

        for (i = 0; i < pclass->num_printers; i ++)
	  if (pclass->printers[i] == p)
	    break;

        if (i >= pclass->num_printers)
	  AddPrinterToClass(pclass, p);
      }
      else
      {
       /*
        * First time around; just get name length and mark it as first
	* in the list...
	*/

	if ((hptr = strchr(p->name, '@')) != NULL)
	  len = hptr - p->name;
	else
	  len = strlen(p->name);

        strncpy(name, p->name, len);
	name[len] = '\0';
	offset    = 0;

	if ((pclass = FindPrinter(name)) != NULL &&
	    !(pclass->type & CUPS_PRINTER_IMPLICIT))
	{
	 /*
	  * Can't use same name as a local printer; add "Any" to the
	  * front of the name, unless we have explicitly disabled
	  * the "ImplicitAnyClasses"...
	  */

          if (ImplicitAnyClasses)
	  {
	   /*
	    * Add "Any" to the class name...
	    */

            strcpy(name, "Any");
            strncpy(name + 3, p->name, len);
	    name[len + 3] = '\0';
	    offset        = 3;
	  }
	  else
	  {
	   /*
	    * Don't create an implicit class if we have a local printer
	    * with the same name...
	    */

	    len = 0;
	    continue;
	  }
	}

	first = p;
      }
    }
  }
}


/*
 * 'SendBrowseList()' - Send new browsing information as necessary.
 */

void
SendBrowseList(void)
{
  printer_t		*p,	/* Current printer */
			*np;	/* Next printer */
  time_t		ut,	/* Minimum update time */
			to;	/* Timeout time */


  if (!Browsing || !(BrowseProtocols & BROWSE_CUPS))
    return;

 /*
  * Compute the update and timeout times...
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
      {
        LogMessage(L_INFO, "Remote destination \"%s\" has timed out; deleting it...",
	           p->name);
        DeletePrinter(p);
      }
    }
    else if (p->browse_time < ut &&  BrowseInterval > 0 &&
            !(p->type & CUPS_PRINTER_IMPLICIT))
    {
     /*
      * Need to send an update...
      */

      p->browse_time = time(NULL);

      if (BrowseProtocols & BROWSE_CUPS)
        SendCUPSBrowse(p);

#ifdef HAVE_LIBSLP
      if (BrowseProtocols & BROWSE_SLP)
        SendSLPBrowse(p);
#endif /* HAVE_LIBSLP */
    }
  }
}


/*
 * 'SendCUPSBrowse()' - Send new browsing information using the CUPS protocol.
 */

void
SendCUPSBrowse(printer_t *p)	/* I - Printer to send */
{
  int			i;	/* Looping var */
  int			bytes;	/* Length of packet */
  char			packet[1453];
				/* Browse data packet */


  snprintf(packet, sizeof(packet), "%x %x %s \"%s\" \"%s\" \"%s\"\n",
           p->type | CUPS_PRINTER_REMOTE, p->state, p->uri,
	   p->location, p->info, p->make_model);

  bytes = strlen(packet);
  LogMessage(L_DEBUG2, "SendBrowseList: (%d bytes) %s", bytes, packet);

 /*
  * Send a packet to each browse address...
  */

  for (i = 0; i < NumBrowsers; i ++)
    if (sendto(BrowseSocket, packet, bytes, 0,
	       (struct sockaddr *)Browsers + i, sizeof(Browsers[0])) <= 0)
    {
      LogMessage(L_ERROR, "SendBrowseList: sendto failed for browser %d - %s.",
	         i + 1, strerror(errno));
      LogMessage(L_ERROR, "Browsing turned off.");

      StopBrowsing();
      Browsing = 0;
      return;
    }
}


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

  if (BrowseProtocols & BROWSE_CUPS)
  {
   /*
    * Create the broadcast socket...
    */

    if ((BrowseSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
      LogMessage(L_ERROR, "StartBrowsing: Unable to create broadcast socket - %s.",
        	 strerror(errno));
      Browsing = 0;
      return;
    }

   /*
    * Set the "broadcast" flag...
    */

    val = 1;
    if (setsockopt(BrowseSocket, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)))
    {
      LogMessage(L_ERROR, "StartBrowsing: Unable to set broadcast mode - %s.",
        	 strerror(errno));

  #if defined(WIN32) || defined(__EMX__)
      closesocket(BrowseSocket);
  #else
      close(BrowseSocket);
  #endif /* WIN32 || __EMX__ */

      BrowseSocket = -1;
      Browsing     = 0;
      return;
    }

   /*
    * Bind the socket to browse port...
    */

    memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(BrowsePort);

    if (bind(BrowseSocket, (struct sockaddr *)&addr, sizeof(addr)))
    {
      LogMessage(L_ERROR, "StartBrowsing: Unable to bind broadcast socket - %s.",
        	 strerror(errno));

  #if defined(WIN32) || defined(__EMX__)
      closesocket(BrowseSocket);
  #else
      close(BrowseSocket);
  #endif /* WIN32 || __EMX__ */

      BrowseSocket = -1;
      Browsing     = 0;
      return;
    }

   /*
    * Finally, add the socket to the input selection set...
    */

    LogMessage(L_DEBUG2, "StartBrowsing: Adding fd %d to InputSet...",
               BrowseSocket);

    FD_SET(BrowseSocket, &InputSet);
  }

#ifdef HAVE_LIBSLP
  if (BrowseProtocols & BROWSE_SLP)
  {
   /* 
    * Open SLP handle...
    */

    if (SLPOpen("en", SLP_FALSE, &BrowseSLPHandle) != SLP_OK)
    {
      LogMessage(L_ERROR, "Unable to open an SLP handle; disabling SLP browsing!");
      BrowseProtocols &= ~BROWSE_SLP;
    }

    BrowseSLPRefresh = 0;
  }
#endif /* HAVE_LIBSLP */
}


/*
 * 'StartPolling()' - Start polling servers as needed.
 */

void
StartPolling(void)
{
  int		i;		/* Looping var */
  dirsvc_poll_t	*poll;		/* Current polling server */
  int		pid;		/* New process ID */
  char		sport[10];	/* Server port */
  char		bport[10];	/* Browser port */
  char		interval[10];	/* Poll interval */


  sprintf(bport, "%d", BrowsePort);

  if (BrowseInterval)
    sprintf(interval, "%d", BrowseInterval);
  else
    strcpy(interval, "30");

  for (i = 0, poll = Polled; i < NumPolled; i ++, poll ++)
  {
    sprintf(sport, "%d", poll->port);

    if ((pid = fork()) == 0)
    {
     /*
      * Child...
      */

      if (getuid() == 0)
      {
       /*
	* Running as root, so change to non-priviledged user...
	*/

	if (setgid(Group))
          exit(errno);

	if (setuid(User))
          exit(errno);
      }

     /*
      * Reset group membership to just the main one we belong to.
      */

      setgroups(0, NULL);

     /*
      * Execute the polling daemon...
      */

      execl(CUPS_SERVERBIN "/daemon/cups-polld", "cups-polld", poll->hostname,
            sport, interval, bport, NULL);
      exit(errno);
    }
    else if (pid < 0)
    {
      LogMessage(L_ERROR, "StartPolling: Unable to fork polling daemon - %s",
                 strerror(errno));
      poll->pid = 0;
      break;
    }
    else
    {
      poll->pid = pid;
      LogMessage(L_DEBUG, "StartPolling: Started polling daemon for %s:%d, pid = %d",
                 poll->hostname, poll->port, pid);
    }
  }
}


/*
 * 'StopBrowsing()' - Stop sending and receiving broadcast information.
 */

void
StopBrowsing(void)
{
  if (!Browsing)
    return;

  if (BrowseProtocols & BROWSE_CUPS)
  {
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

      LogMessage(L_DEBUG2, "StopBrowsing: Removing fd %d from InputSet...",
        	 BrowseSocket);

      FD_CLR(BrowseSocket, &InputSet);
      BrowseSocket = 0;
    }
  }

#ifdef HAVE_LIBSLP
  if (BrowseProtocols & BROWSE_SLP)
  {
   /* 
    * Close SLP handle...
    */

    SLPClose(BrowseSLPHandle);
  }
#endif /* HAVE_LIBSLP */
}


/*
 * 'StopPolling()' - Stop polling servers as needed.
 */

void
StopPolling(void)
{
  int		i;		/* Looping var */
  dirsvc_poll_t	*poll;		/* Current polling server */


  for (i = 0, poll = Polled; i < NumPolled; i ++, poll ++)
    if (poll->pid)
      kill(poll->pid, SIGTERM);
}


/*
 * 'UpdateCUPSBrowse()' - Update the browse lists using the CUPS protocol.
 */

void
UpdateCUPSBrowse(void)
{
  int		i;			/* Looping var */
  int		auth;			/* Authorization status */
  int		len;			/* Length of name string */
  int		bytes;			/* Number of bytes left */
  char		packet[1540],		/* Broadcast packet */
		*pptr;			/* Pointer into packet */
  http_addr_t	srcaddr;		/* Source address */
  char		srcname[1024];		/* Source hostname */
  unsigned	address[4],		/* Source address */
		temp;			/* Temporary address var (host order) */
  cups_ptype_t	type;			/* Printer type */
  ipp_pstate_t	state;			/* Printer state */
  char		uri[HTTP_MAX_URI],	/* Printer URI */
		method[HTTP_MAX_URI],	/* Method portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI],	/* Resource portion of URI */
		info[IPP_MAX_NAME],	/* Information string */
		location[IPP_MAX_NAME],	/* Location string */
		make_model[IPP_MAX_NAME];/* Make and model string */
  int		port;			/* Port portion of URI */


 /*
  * Read a packet from the browse socket...
  */

  len = sizeof(srcaddr);
  if ((bytes = recvfrom(BrowseSocket, packet, sizeof(packet), 0, 
                        (struct sockaddr *)&srcaddr, &len)) <= 0)
  {
   /*
    * "Connection refused" is returned under Linux if the destination port
    * or address is unreachable from a previous sendto(); check for the
    * error here and ignore it for now...
    */

    if (errno != ECONNREFUSED)
    {
      LogMessage(L_ERROR, "Browse recv failed - %s.", strerror(errno));
      LogMessage(L_ERROR, "Browsing turned off.");

      StopBrowsing();
      Browsing = 0;
    }

    return;
  }

  packet[bytes] = '\0';

 /*
  * Figure out where it came from...
  */

#ifdef AF_INET6
  if (srcaddr.addr.sa_family == AF_INET6)
  {
    address[0] = ntohl(srcaddr.ipv6.sin6_addr.s6_addr32[0]);
    address[1] = ntohl(srcaddr.ipv6.sin6_addr.s6_addr32[1]);
    address[2] = ntohl(srcaddr.ipv6.sin6_addr.s6_addr32[2]);
    address[3] = ntohl(srcaddr.ipv6.sin6_addr.s6_addr32[3]);
  }
  else
#endif /* AF_INET6 */
  {
    temp = ntohl(srcaddr.ipv4.sin_addr.s_addr);

    address[3] = temp & 255;
    temp       >>= 8;
    address[2] = temp & 255;
    temp       >>= 8;
    address[1] = temp & 255;
    temp       >>= 8;
    address[0] = temp & 255;
  }

  if (HostNameLookups)
    httpAddrLookup(&srcaddr, srcname, sizeof(srcname));
  else
    httpAddrString(&srcaddr, srcname, sizeof(srcname));

  len = strlen(srcname);

 /*
  * Do ACL stuff...
  */

  if (BrowseACL && (BrowseACL->num_allow || BrowseACL->num_deny))
  {
    if (httpAddrLocalhost(&srcaddr) || strcasecmp(srcname, "localhost") == 0)
    {
     /*
      * Access from localhost (127.0.0.1) is always allowed...
      */

      auth = AUTH_ALLOW;
    }
    else
    {
     /*
      * Do authorization checks on the domain/address...
      */

      switch (BrowseACL->order_type)
      {
        default :
	    auth = AUTH_DENY;	/* anti-compiler-warning-code */
	    break;

	case AUTH_ALLOW : /* Order Deny,Allow */
            auth = AUTH_ALLOW;

            if (CheckAuth(address, srcname, len,
	        	  BrowseACL->num_deny, BrowseACL->deny))
	      auth = AUTH_DENY;

            if (CheckAuth(address, srcname, len,
	        	  BrowseACL->num_allow, BrowseACL->allow))
	      auth = AUTH_ALLOW;
	    break;

	case AUTH_DENY : /* Order Allow,Deny */
            auth = AUTH_DENY;

            if (CheckAuth(address, srcname, len,
	        	  BrowseACL->num_allow, BrowseACL->allow))
	      auth = AUTH_ALLOW;

            if (CheckAuth(address, srcname, len,
	        	  BrowseACL->num_deny, BrowseACL->deny))
	      auth = AUTH_DENY;
	    break;
      }
    }
  }
  else
    auth = AUTH_ALLOW;

  if (auth == AUTH_DENY)
  {
    LogMessage(L_DEBUG, "UpdateBrowseList: Refused %d bytes from %s", bytes,
               srcname);
    return;
  }

  LogMessage(L_DEBUG2, "UpdateBrowseList: (%d bytes from %s) %s", bytes, srcname,
             packet);

 /*
  * Parse packet...
  */

  if (sscanf(packet, "%x%x%1023s", (unsigned *)&type, (unsigned *)&state,
             uri) < 3)
  {
    LogMessage(L_WARN, "UpdateBrowseList: Garbled browse packet - %s",
               packet);
    return;
  }

  strcpy(location, "Location Unknown");
  strcpy(info, "No Information Available");
  make_model[0] = '\0';

  if ((pptr = strchr(packet, '\"')) != NULL)
  {
   /*
    * Have extended information; can't use sscanf for it because not all
    * sscanf's allow empty strings with %[^\"]...
    */

    for (i = 0, pptr ++;
         i < (sizeof(location) - 1) && *pptr && *pptr != '\"';
         i ++, pptr ++)
      location[i] = *pptr;

    if (i)
      location[i] = '\0';

    if (*pptr == '\"')
      pptr ++;

    while (*pptr && isspace(*pptr))
      pptr ++;

    if (*pptr == '\"')
    {
      for (i = 0, pptr ++;
           i < (sizeof(info) - 1) && *pptr && *pptr != '\"';
           i ++, pptr ++)
	info[i] = *pptr;

      if (i)
	info[i] = '\0';

      if (*pptr == '\"')
	pptr ++;

      while (*pptr && isspace(*pptr))
	pptr ++;

      if (*pptr == '\"')
      {
	for (i = 0, pptr ++;
             i < (sizeof(make_model) - 1) && *pptr && *pptr != '\"';
             i ++, pptr ++)
	  make_model[i] = *pptr;

	if (i)
	  make_model[i] = '\0';
      }
    }
  }

  DEBUG_puts(packet);
  DEBUG_printf(("type=%x, state=%x, uri=\"%s\"\n"
                "location=\"%s\", info=\"%s\", make_model=\"%s\"\n",
	        type, state, uri, location, info, make_model));

 /*
  * Pull the URI apart to see if this is a local or remote printer...
  */

  httpSeparate(uri, method, username, host, &port, resource);

  DEBUG_printf(("host=\"%s\", ServerName=\"%s\"\n", host, ServerName));

  if (strcasecmp(host, ServerName) == 0)
    return;

 /*
  * Do relaying...
  */

  for (i = 0; i < NumRelays; i ++)
    if (CheckAuth(address, srcname, len, 1, &(Relays[i].from)))
      if (sendto(BrowseSocket, packet, bytes, 0,
                 (struct sockaddr *)&(Relays[i].to),
		 sizeof(http_addr_t)) <= 0)
      {
	LogMessage(L_ERROR, "UpdateBrowseList: sendto failed for relay %d - %s.",
	           i + 1, strerror(errno));
	return;
      }

 /*
  * Process the browse data...
  */

  ProcessBrowseData(uri, type, state, location, info, make_model);
}


/***********************************************************************
 **** SLP Support Code *************************************************
 ***********************************************************************/

#ifdef HAVE_LIBSLP 
/*
 * SLP service name for CUPS...
 */

#  define SLP_CUPS_SRVTYPE	"service:printer"
#  define SLP_CUPS_SRVLEN	15

typedef struct _slpsrvurl
{
  struct _slpsrvurl	*next;
  char			url[HTTP_MAX_URI];
} slpsrvurl_t;


/*
 * 'RegReportCallback()' - Empty SLPRegReport.
 */

void
RegReportCallback(SLPHandle hslp,
                  SLPError  errcode,
		  void      *cookie)
{
  (void)hslp;
  (void)errcode;
  (void)cookie;

  return;
}


/*
 * 'SendSLPBrowse()' - Register the specified printer with SLP.
 */

void 
SendSLPBrowse(printer_t *p)		/* I - Printer to register */
{
  char		srvurl[HTTP_MAX_URI],	/* Printer service URI */
		attrs[8192],		/* Printer attributes */
		finishings[1024],	/* Finishings to support */
		make_model[IPP_MAX_NAME * 2],
					/* Make and model, quoted */
		location[IPP_MAX_NAME * 2],
					/* Location, quoted */
		info[IPP_MAX_NAME * 2],
					/* Info, quoted */
		*src,			/* Pointer to original string */
		*dst;			/* Pointer to destination string */
  ipp_attribute_t *authentication;	/* uri-authentication-supported value */
  SLPError	error;			/* SLP error, if any */


  LogMessage(L_DEBUG, "SendSLPBrowse(%p = \"%s\")", p, p->name);

 /*
  * Make the SLP service URL that conforms to the IANA 
  * 'printer:' template.
  */

  snprintf(srvurl, sizeof(srvurl), SLP_CUPS_SRVTYPE ":%s", p->uri);

  LogMessage(L_DEBUG2, "Service URL = \"%s\"", srvurl);

 /*
  * Figure out the finishings string...
  */

  if (p->type & CUPS_PRINTER_STAPLE)
    strcpy(finishings, "staple");
  else
    finishings[0] = '\0';

  if (p->type & CUPS_PRINTER_BIND)
  {
    if (finishings[0])
      strncat(finishings, ",bind", sizeof(finishings) - 1);
    else
      strcpy(finishings, "bind");
  }

  if (p->type & CUPS_PRINTER_PUNCH)
  {
    if (finishings[0])
      strncat(finishings, ",punch", sizeof(finishings) - 1);
    else
      strcpy(finishings, "punch");
  }

  if (p->type & CUPS_PRINTER_COVER)
  {
    if (finishings[0])
      strncat(finishings, ",cover", sizeof(finishings) - 1);
    else
      strcpy(finishings, "cover");
  }

  if (p->type & CUPS_PRINTER_SORT)
  {
    if (finishings[0])
      strncat(finishings, ",sort", sizeof(finishings) - 1);
    else
      strcpy(finishings, "sort");
  }

  if (!finishings[0])
    strcpy(finishings, "none");

  finishings[sizeof(finishings) - 1] = '\0';

 /*
  * Quote any commas in the make and model, location, and info strings
  * (local strings are twice the size of the ones in the printer_t
  * structure, so no buffer overflow is possible...)
  */

  for (src = p->make_model, dst = make_model; *src;)
  {
    if (*src == ',' || *src == '\\' || *src == ')')
      *dst++ = '\\';

    *dst++ = *src++;
  }

  *dst = '\0';

  if (!make_model[0])
    strcpy(make_model, "Unknown");

  for (src = p->location, dst = location; *src;)
  {
    if (*src == ',' || *src == '\\' || *src == ')')
      *dst++ = '\\';

    *dst++ = *src++;
  }

  *dst = '\0';

  if (!location[0])
    strcpy(location, "Unknown");

  for (src = p->info, dst = info; *src;)
  {
    if (*src == ',' || *src == '\\' || *src == ')')
      *dst++ = '\\';

    *dst++ = *src++;
  }

  *dst = '\0';

  if (!info[0])
    strcpy(info, "Unknown");

 /*
  * Get the authentication value...
  */

  authentication = ippFindAttribute(p->attrs, "uri-authentication-supported",
                                    IPP_TAG_KEYWORD);

 /*
  * Make the SLP attribute string list that conforms to
  * the IANA 'printer:' template.
  */

  snprintf(attrs, sizeof(attrs),
           "(printer-uri-supported=%s),"
           "(uri-authentication-supported=%s>),"
#ifdef HAVE_LIBSSL
           "(uri-security-supported=tls>),"
#else
           "(uri-security-supported=none>),"
#endif /* HAVE_LIBSSL */
           "(printer-name=%s),"
           "(printer-location=%s),"
           "(printer-info=%s),"
           "(printer-more-info=%s),"
           "(printer-make-and-model=%s),"
	   "(charset-supported=utf-8),"
	   "(natural-language-configured=%s),"
	   "(natural-language-supported=de,en,es,fr,it),"
           "(color-supported=%s),"
           "(finishings-supported=%s),"
           "(sides-supported=one-sided%s),"
	   "(multiple-document-jobs-supported=true)"
	   "(ipp-versions-supported=1.0,1.1)",
	   p->uri, authentication->values[0].string.text, p->name, location,
	   info, p->uri, make_model, DefaultLanguage,
           p->type & CUPS_PRINTER_COLOR ? "true" : "false",
           finishings,
           p->type & CUPS_PRINTER_DUPLEX ?
	       ",two-sided-long-edge,two-sided-short-edge" : "");

  LogMessage(L_DEBUG2, "Attributes = \"%s\"", attrs);

 /*
  * Register the printer with the SLP server...
  */

  error = SLPReg(BrowseSLPHandle, srvurl, BrowseTimeout,
	         SLP_CUPS_SRVTYPE, attrs, SLP_TRUE, RegReportCallback, 0);

  if (error != SLP_OK)
    LogMessage(L_ERROR, "SLPReg of \"%s\" failed with status %d!", p->name,
               error);
}


/*
 * 'SLPDeregPrinter()' - SLPDereg() the specified printer
 */

void 
SLPDeregPrinter(printer_t *p)
{
  char	srvurl[HTTP_MAX_URI];	/* Printer service URI */


  if((p->type & CUPS_PRINTER_REMOTE) == 0)
  {
   /*
    * Make the SLP service URL that conforms to the IANA 
    * 'printer:' template.
    */

    snprintf(srvurl, sizeof(srvurl), SLP_CUPS_SRVTYPE ":%s", p->uri);

   /*
    * Deregister the printer...
    */

    SLPDereg(BrowseSLPHandle, srvurl, RegReportCallback, 0);
  }
}


/*
 * 'GetSlpAttrVal()' - Get an attribute from an SLP registration.
 */

int 					/* O - 0 on success */
GetSlpAttrVal(const char *attrlist,	/* I - Attribute list string */
              const char *tag,		/* I - Name of attribute */
              char       *valbuf,	/* O - Value */
              int        valbuflen)	/* I - Max length of value */
{
  char	*ptr1,				/* Pointer into string */
	*ptr2;				/* ... */


  valbuf[0] = '\0';

  if ((ptr1 = strstr(attrlist, tag)) != NULL)
  {
    ptr1 += strlen(tag);

    if ((ptr2 = strchr(ptr1,')')) != NULL)
    {
      if (valbuflen > (ptr2 - ptr1))
      {
       /*
        * Copy the value...
	*/

        strncpy(valbuf, ptr1, ptr2 - ptr1);
	valbuf[ptr2 - ptr1] = '\0';

       /*
        * Dequote the value...
	*/

	for (ptr1 = valbuf; *ptr1; ptr1 ++)
	  if (*ptr1 == '\\' && ptr1[1])
	    strcpy(ptr1, ptr1 + 1);

        return (0);
      }
    }
  }

  return (-1);
}


/*
 * 'AttrCallback()' - SLP attribute callback 
 */

SLPBoolean
AttrCallback(SLPHandle  hslp, 
             const char *attrlist, 
             SLPError   errcode, 
             void       *cookie)
{
  char         tmp[IPP_MAX_NAME];
  printer_t    *p = (printer_t*)cookie;


 /*
  * Let the compiler know we won't be using these...
  */

  (void)hslp;

 /*
  * Bail if there was an error
  */

  if (errcode != SLP_OK)
    return (SLP_TRUE);

 /*
  * Parse the attrlist to obtain things needed to build CUPS browse packet
  */

  memset(p, 0, sizeof(printer_t));

  p->type = CUPS_PRINTER_REMOTE;

  if (GetSlpAttrVal(attrlist, "(printer-location=", p->location,
                    sizeof(p->location)))
    return (SLP_FALSE);
  if (GetSlpAttrVal(attrlist, "(printer-make-and-model=", p->make_model,
                    sizeof(p->make_model)))
    return (SLP_FALSE);

  if (GetSlpAttrVal(attrlist, "(color-supported=", tmp, sizeof(tmp)))
    return (SLP_FALSE);
  if (strcasecmp(tmp, "true") == 0)
    p->type |= CUPS_PRINTER_COLOR;

  if (GetSlpAttrVal(attrlist, "(finishings-supported=", tmp, sizeof(tmp)))
    return (SLP_FALSE);
  if (strstr(tmp, "staple"))
    p->type |= CUPS_PRINTER_STAPLE;
  if (strstr(tmp, "bind"))
    p->type |= CUPS_PRINTER_BIND;
  if (strstr(tmp, "punch"))
    p->type |= CUPS_PRINTER_PUNCH;

  if (GetSlpAttrVal(attrlist, "(sides-supported=", tmp, sizeof(tmp)))
    return (SLP_FALSE);
  if (strstr(tmp,"two-sided"))
    p->type |= CUPS_PRINTER_DUPLEX;

  return (SLP_TRUE);
}


/*
 * 'SrvUrlCallback()' - SLP service url callback
 */

SLPBoolean				/* O - TRUE = OK, FALSE = error */
SrvUrlCallback(SLPHandle      hslp, 	/* I - SLP handle */
               const char     *srvurl, 	/* I - URL of service */
               unsigned short lifetime,	/* I - Life of service */
               SLPError       errcode, 	/* I - Existing error code */
               void           *cookie)	/* I - Pointer to service list */
{
  slpsrvurl_t	*s,			/* New service entry */
		**head;			/* Pointer to head of entry */


 /*
  * Let the compiler know we won't be using these vars...
  */

  (void)hslp;
  (void)lifetime;

 /*
  * Bail if there was an error
  */

  if (errcode != SLP_OK)
    return (SLP_TRUE);

 /*
  * Grab the head of the list...
  */

  head = (slpsrvurl_t**)cookie;

 /*
  * Allocate a *temporary* slpsrvurl_t to hold this entry.
  */

  if ((s = (slpsrvurl_t *)calloc(1, sizeof(slpsrvurl_t))) == NULL)
    return (SLP_FALSE);

 /*
  * Copy the SLP service URL...
  */

  strncpy(s->url, srvurl, sizeof(s->url));

 /* 
  * Link the SLP service URL into the head of the list
  */

  if (*head)
    s->next = *head;

  *head = s;

  return (SLP_TRUE);
}


/*
 * 'UpdateSLPBrowse()' - Get browsing information via SLP.
 */

void
UpdateSLPBrowse(void)
{
  slpsrvurl_t	*s,			/* Temporary list of service URLs */
		*next;			/* Next service in list */
  printer_t	p;			/* Printer information */
  const char	*uri;			/* Pointer to printer URI */
  char		method[HTTP_MAX_URI],	/* Method portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  int		port;			/* Port portion of URI */


  LogMessage(L_DEBUG, "UpdateSLPBrowse() Start...");

 /*
  * Reset the refresh time...
  */

  BrowseSLPRefresh = time(NULL) + BrowseTimeout - BrowseInterval;

 /* 
  * Poll for remote printers using SLP...
  */

  s = NULL;

  SLPFindSrvs(BrowseSLPHandle, SLP_CUPS_SRVTYPE, "", "",
	      SrvUrlCallback, &s);

 /*
  * Loop through the list of available printers...
  */

  for (; s; s = next)
  {
   /* 
    * Load a printer_t structure with the SLP service attributes...
    */

    SLPFindAttrs(BrowseSLPHandle, s->url, "", "", AttrCallback, &p);

   /*
    * Process this printer entry...
    */

    uri = s->url + SLP_CUPS_SRVLEN + 1;

    if (strncmp(uri, "http://", 7) == 0 ||
        strncmp(uri, "ipp://", 6) == 0)
    {
     /*
      * Pull the URI apart to see if this is a local or remote printer...
      */

      httpSeparate(uri, method, username, host, &port, resource);

      if (strcasecmp(host, ServerName) == 0)
	continue;

     /*
      * OK, at least an IPP printer, see if it is a CUPS printer or
      * class...
      */

      if (strstr(uri, "/printers/") != NULL)
        ProcessBrowseData(uri, p.type, IPP_PRINTER_IDLE, p.location,
	                  p.info, p.make_model);
      else if (strstr(uri, "/classes/") != NULL)
        ProcessBrowseData(uri, p.type | CUPS_PRINTER_CLASS, IPP_PRINTER_IDLE,
	                  p.location, p.info, p.make_model);
    }

   /*
    * Save the "next" pointer and free this listing...
    */

    next = s->next;
    free(s);
  }       

  LogMessage(L_DEBUG, "UpdateSLPBrowse() End...");
}
#endif /* HAVE_LIBSLP */


/*
 * End of "$Id: dirsvc.c,v 1.73.2.5 2002/01/02 18:05:01 mike Exp $".
 */
