/*
 * "$Id: dirsvc.c,v 1.69 2001/01/22 15:03:59 mike Exp $"
 *
 *   Directory services routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2001 by Easy Software Products, all rights reserved.
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
 *   StartBrowsing()    - Start sending and receiving broadcast information.
 *   StopBrowsing()     - Stop sending and receiving broadcast information.
 *   UpdateBrowseList() - Update the browse lists for any new browse data.
 *   SendBrowseList()   - Send new browsing information.
 *   StartPolling()     - Start polling servers as needed.
 *   StopPolling()      - Stop polling servers as needed.
 */

/*
 * Include necessary headers...
 */

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
    BrowseSocket = 0;
  }
}


/*
 * 'UpdateBrowseList()' - Update the browse lists for any new browse data.
 */

void
UpdateBrowseList(void)
{
  int		i;			/* Looping var */
  int		update;			/* Update printer attributes? */
  int		auth;			/* Authorization status */
  int		len,			/* Length of name string */
		offset;			/* Offset in name string */
  int		bytes;			/* Number of bytes left */
  char		packet[1540],		/* Broadcast packet */
		*pptr;			/* Pointer into packet */
  struct sockaddr_in srcaddr;		/* Source address */
  char		srcname[1024];		/* Source hostname */
  unsigned	address;		/* Source address (host order) */
  struct hostent *srchost;		/* Host entry for source address */
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
  char		name[IPP_MAX_NAME],	/* Name of printer */
		*hptr,			/* Pointer into hostname */
		*sptr;			/* Pointer into ServerName */
  printer_t	*p,			/* Printer information */
		*pclass,		/* Printer class */
		*first;			/* First printer in class */


 /*
  * Read a packet from the browse socket...
  */

  len = sizeof(srcaddr);
  if ((bytes = recvfrom(BrowseSocket, packet, sizeof(packet), 0, 
                        (struct sockaddr *)&srcaddr, &len)) <= 0)
  {
    LogMessage(L_ERROR, "Browse recv failed - %s.",
               strerror(errno));
    LogMessage(L_ERROR, "Browsing turned off.");

    StopBrowsing();
    Browsing = 0;
    return;
  }

  packet[bytes] = '\0';

 /*
  * Figure out where it came from...
  */

  address = ntohl(srcaddr.sin_addr.s_addr);

  if (HostNameLookups)
#ifndef __sgi
    srchost = gethostbyaddr((char *)&(srcaddr.sin_addr), sizeof(struct in_addr),
                            AF_INET);
#else
    srchost = gethostbyaddr(&(srcaddr.sin_addr), sizeof(struct in_addr),
                            AF_INET);
#endif /* !__sgi */
  else
    srchost = NULL;

  if (srchost == NULL)
    sprintf(srcname, "%d.%d.%d.%d", address >> 24, (address >> 16) & 255,
            (address >> 8) & 255, address & 255);
  else
  {
    strncpy(srcname, srchost->h_name, sizeof(srcname) - 1);
    srcname[sizeof(srcname) - 1] = '\0';
  }

  len = strlen(srcname);

 /*
  * Do ACL stuff...
  */

  if (BrowseACL)
  {
    if (address == 0x7f000001 || strcasecmp(srcname, "localhost") == 0)
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

  update = 0;

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
		 sizeof(struct sockaddr_in)) <= 0)
      {
	LogMessage(L_ERROR, "UpdateBrowseList: sendto failed for relay %d - %s.",
	           i + 1, strerror(errno));
	return;
      }

 /*
  * OK, this isn't a local printer; see if we already have it listed in
  * the Printers list, and add it if not...
  */

  hptr = strchr(host, '.');
  sptr = strchr(ServerName, '.');

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
	  strncpy(p->more_info, uri, sizeof(p->more_info) - 1);
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
    else if (!p->hostname[0])
    {
      /* No "p->var[sizeof(p->var) - 1] = '\0';" because p is zeroed */
      strncpy(p->hostname, host, sizeof(p->hostname) - 1);
      strncpy(p->uri, uri, sizeof(p->uri) - 1);
      strncpy(p->more_info, uri, sizeof(p->more_info) - 1);
      strncpy(p->device_uri, uri, sizeof(p->device_uri) - 1);
      update = 1;
    }

    if (p == NULL)
    {
     /*
      * Class doesn't exist; add it...
      */

      p = AddClass(name);

     /*
      * Force the URI to point to the real server...
      */

      p->type = type;
      /* No "p->var[sizeof(p->var) - 1] = '\0';" because p is zeroed */
      strncpy(p->uri, uri, sizeof(p->uri) - 1);
      strncpy(p->more_info, uri, sizeof(p->more_info) - 1);
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
	  strncpy(p->more_info, uri, sizeof(p->more_info) - 1);
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
    else if (!p->hostname[0])
    {
      /* No "p->var[sizeof(p->var) - 1] = '\0';" because p is zeroed */
      strncpy(p->hostname, host, sizeof(p->hostname) - 1);
      strncpy(p->uri, uri, sizeof(p->uri) - 1);
      strncpy(p->more_info, uri, sizeof(p->more_info) - 1);
      strncpy(p->device_uri, uri, sizeof(p->device_uri) - 1);
      update = 1;
    }

    if (p == NULL)
    {
     /*
      * Printer doesn't exist; add it...
      */

      p = AddPrinter(name);

     /*
      * Force the URI to point to the real server...
      */

      p->type = type;
      /* No "p->var[sizeof(p->var) - 1] = '\0';" because p is zeroed */
      strncpy(p->hostname, host, sizeof(p->hostname) - 1);
      strncpy(p->uri, uri, sizeof(p->uri) - 1);
      strncpy(p->more_info, uri, sizeof(p->more_info) - 1);
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
      snprintf(make_model, sizeof(p->make_model), "Remote Class on %s",
               host);
    else
      snprintf(make_model, sizeof(p->make_model), "Remote Printer on %s",
               host);
  }
  else
  {
    strncat(make_model, " on ", sizeof(make_model) - 1);
    strncat(make_model, host, sizeof(make_model) - 1);
    make_model[sizeof(make_model) - 1] = '\0';
  }

  if (strcmp(p->make_model, make_model))
  {
    /* No "p->var[sizeof(p->var) - 1] = '\0';" because p is zeroed */
    strncpy(p->make_model, make_model, sizeof(p->make_model) - 1);
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
	 p = p->next)
    {
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

          DEBUG_printf(("Added new class \"%s\", type = %x\n", name,
	                pclass->type));
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
	  * Can't use same name as printer; add "Any" to the front of the
	  * name...
	  */

          strcpy(name, "Any");
          strncpy(name + 3, p->name, len);
	  name[len + 3] = '\0';
	  offset        = 3;
	}

	first = p;
      }
    }
  }
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
  char			packet[1453];
				/* Browse data packet */


  if (!Browsing || BrowseInterval == 0)
    return;

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
      {
        LogMessage(L_INFO, "Remote destination \"%s\" has timed out; deleting it...",
	           p->name);
        DeletePrinter(p);
      }
    }
    else if (p->browse_time < ut && !(p->type & CUPS_PRINTER_IMPLICIT))
    {
     /*
      * Need to send an update...
      */

      p->browse_time = time(NULL);

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
  }
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

      setgid(Group);
      setuid(User);

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
 * End of "$Id: dirsvc.c,v 1.69 2001/01/22 15:03:59 mike Exp $".
 */
