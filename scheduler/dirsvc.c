/*
 * "$Id$"
 *
 *   Directory services routines for the Common UNIX Printing System (CUPS).
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
 *   cupsdLoadRemoteCache()        - Load the remote printer cache.
 *   cupsdProcessBrowseData()      - Process new browse data.
 *   cupsdProcessImplicitClasses() - Create/update implicit classes as needed.
 *   cupsdSaveRemoteCache()        - Save the remote printer cache.
 *   cupsdSendBrowseDelete()       - Send a "browse delete" message for a
 *                                   printer.
 *   cupsdSendBrowseList()         - Send new browsing information as necessary.
 *   cupsdSendCUPSBrowse()         - Send new browsing information using the
 *                                   CUPS protocol.
 *   cupsdSendSLPBrowse()          - Register the specified printer with SLP.
 *   cupsdStartBrowsing()          - Start sending and receiving broadcast
 *                                   information.
 *   cupsdStartPolling()           - Start polling servers as needed.
 *   cupsdStopBrowsing()           - Stop sending and receiving broadcast
 *                                   information.
 *   cupsdStopPolling()            - Stop polling servers as needed.
 *   cupsdUpdateCUPSBrowse()       - Update the browse lists using the CUPS
 *                                   protocol.
 *   cupsdUpdatePolling()          - Read status messages from the poll daemons.
 *   cupsdUpdateSLPBrowse()        - Get browsing information via SLP.
 *   slp_attr_callback()           - SLP attribute callback 
 *   slp_dereg_printer()           - SLPDereg() the specified printer
 *   slp_get_attr()                - Get an attribute from an SLP registration.
 *   slp_reg_callback()            - Empty SLPRegReport.
 *   slp_url_callback()            - SLP service url callback
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <grp.h>


/*
 * SLP definitions...
 */

#ifdef HAVE_LIBSLP 
/*
 * SLP service name for CUPS...
 */

#  define SLP_CUPS_SRVTYPE	"service:printer"
#  define SLP_CUPS_SRVLEN	15


/* 
 * Printer service URL structure
 */

typedef struct _slpsrvurl_s		/**** SLP URL list ****/
{
  struct _slpsrvurl_s	*next;		/* Next URL in list */
  char			url[HTTP_MAX_URI];
					/* URL */
} slpsrvurl_t;


/*
 * Local functions...
 */

static SLPBoolean	slp_attr_callback(SLPHandle hslp, const char *attrlist,
			                  SLPError errcode, void *cookie);
static void		slp_dereg_printer(cupsd_printer_t *p);
static int 		slp_get_attr(const char *attrlist, const char *tag,
			             char **valbuf);
static void		slp_reg_callback(SLPHandle hslp, SLPError errcode,
					 void *cookie);
static SLPBoolean	slp_url_callback(SLPHandle hslp, const char *srvurl,
			                 unsigned short lifetime,
			                 SLPError errcode, void *cookie);
#endif /* HAVE_LIBSLP */


/*
 * 'cupsdLoadRemoteCache()' - Load the remote printer cache.
 */

void
cupsdLoadRemoteCache(void)
{
  cups_file_t		*fp;		/* remote.cache file */
  int			linenum;	/* Current line number */
  char			line[1024],	/* Line from file */
			*value,		/* Pointer to value */
			*valueptr;	/* Pointer into value */
  cupsd_printer_t	*p;		/* Current printer */
  time_t		now;		/* Current time */


 /*
  * Open the remote.cache file...
  */

  snprintf(line, sizeof(line), "%s/remote.cache", CacheDir);
  if ((fp = cupsFileOpen(line, "r")) == NULL)
    return;

 /*
  * Read printer configurations until we hit EOF...
  */

  linenum = 0;
  p       = NULL;
  now     = time(NULL);

  while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
  {
   /*
    * Decode the directive...
    */

    if (!strcasecmp(line, "<Printer") ||
        !strcasecmp(line, "<DefaultPrinter"))
    {
     /*
      * <Printer name> or <DefaultPrinter name>
      */

      if (p == NULL && value)
      {
       /*
        * Add the printer and a base file type...
	*/

        cupsdLogMessage(CUPSD_LOG_DEBUG,
	                "cupsdLoadRemoteCache: Loading printer %s...", value);

        p = cupsdAddPrinter(value);
	p->accepting   = 1;
	p->state       = IPP_PRINTER_IDLE;
	p->type        |= CUPS_PRINTER_REMOTE;
	p->browse_time = now + BrowseTimeout;

       /*
        * Set the default printer as needed...
	*/

        if (!strcasecmp(line, "<DefaultPrinter"))
	  DefaultPrinter = p;
      }
      else
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of remote.cache.", linenum);
        return;
      }
    }
    else if (!strcasecmp(line, "<Class") ||
             !strcasecmp(line, "<DefaultClass"))
    {
     /*
      * <Class name> or <DefaultClass name>
      */

      if (p == NULL && value)
      {
       /*
        * Add the printer and a base file type...
	*/

        cupsdLogMessage(CUPSD_LOG_DEBUG,
	                "cupsdLoadRemoteCache: Loading class %s...", value);

        p = cupsdAddClass(value);
	p->accepting   = 1;
	p->state       = IPP_PRINTER_IDLE;
	p->type        |= CUPS_PRINTER_REMOTE;
	p->browse_time = now + BrowseTimeout;

       /*
        * Set the default printer as needed...
	*/

        if (!strcasecmp(line, "<DefaultClass"))
	  DefaultPrinter = p;
      }
      else
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of remote.cache.", linenum);
        return;
      }
    }
    else if (!strcasecmp(line, "</Printer>") ||
             !strcasecmp(line, "</Class>"))
    {
      if (p != NULL)
      {
       /*
        * Close out the current printer...
	*/

        cupsdSetPrinterAttrs(p);

        p = NULL;
      }
      else
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of remote.cache.", linenum);
        return;
      }
    }
    else if (!p)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Syntax error on line %d of remote.cache.", linenum);
      return;
    }
    else if (!strcasecmp(line, "Info"))
    {
      if (value)
	cupsdSetString(&p->info, value);
    }
    else if (!strcasecmp(line, "MakeModel"))
    {
      if (value)
	cupsdSetString(&p->make_model, value);
    }
    else if (!strcasecmp(line, "Location"))
    {
      if (value)
	cupsdSetString(&p->location, value);
    }
    else if (!strcasecmp(line, "DeviceURI"))
    {
      if (value)
	cupsdSetString(&p->device_uri, value);
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of remote.cache.", linenum);
	return;
      }
    }
    else if (!strcasecmp(line, "State"))
    {
     /*
      * Set the initial queue state...
      */

      if (value && !strcasecmp(value, "idle"))
        p->state = IPP_PRINTER_IDLE;
      else if (value && !strcasecmp(value, "stopped"))
        p->state = IPP_PRINTER_STOPPED;
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of remote.cache.", linenum);
	return;
      }
    }
    else if (!strcasecmp(line, "StateMessage"))
    {
     /*
      * Set the initial queue state message...
      */

      if (value)
	strlcpy(p->state_message, value, sizeof(p->state_message));
    }
    else if (!strcasecmp(line, "Accepting"))
    {
     /*
      * Set the initial accepting state...
      */

      if (value &&
          (!strcasecmp(value, "yes") ||
           !strcasecmp(value, "on") ||
           !strcasecmp(value, "true")))
        p->accepting = 1;
      else if (value &&
               (!strcasecmp(value, "no") ||
        	!strcasecmp(value, "off") ||
        	!strcasecmp(value, "false")))
        p->accepting = 0;
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of remote.cache.", linenum);
	return;
      }
    }
    else if (!strcasecmp(line, "Type"))
    {
      if (value)
        p->type = atoi(value);
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of remote.cache.", linenum);
	return;
      }
    }
    else if (!strcasecmp(line, "BrowseTime"))
    {
      if (value)
      {
        time_t t = atoi(value);

	if (t > (now + BrowseInterval))
          p->browse_time = t;
      }
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of remote.cache.", linenum);
	return;
      }
    }
    else if (!strcasecmp(line, "JobSheets"))
    {
     /*
      * Set the initial job sheets...
      */

      if (value)
      {
	for (valueptr = value; *valueptr && !isspace(*valueptr & 255); valueptr ++);

	if (*valueptr)
          *valueptr++ = '\0';

	cupsdSetString(&p->job_sheets[0], value);

	while (isspace(*valueptr & 255))
          valueptr ++;

	if (*valueptr)
	{
          for (value = valueptr; *valueptr && !isspace(*valueptr & 255); valueptr ++);

	  if (*valueptr)
            *valueptr++ = '\0';

	  cupsdSetString(&p->job_sheets[1], value);
	}
      }
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of remote.cache.", linenum);
	return;
      }
    }
    else if (!strcasecmp(line, "AllowUser"))
    {
      if (value)
      {
        p->deny_users = 0;
        cupsdAddPrinterUser(p, value);
      }
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of remote.cache.", linenum);
	return;
      }
    }
    else if (!strcasecmp(line, "DenyUser"))
    {
      if (value)
      {
        p->deny_users = 1;
        cupsdAddPrinterUser(p, value);
      }
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of remote.cache.", linenum);
	return;
      }
    }
    else
    {
     /*
      * Something else we don't understand...
      */

      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unknown configuration directive %s on line %d of remote.cache.",
	              line, linenum);
    }
  }

  cupsFileClose(fp);

 /*
  * Do auto-classing if needed...
  */

  cupsdProcessImplicitClasses();
}


/*
 * 'cupsdProcessBrowseData()' - Process new browse data.
 */

void
cupsdProcessBrowseData(
    const char    *uri,			/* I - URI of printer/class */
    cups_ptype_t  type,			/* I - Printer type */
    ipp_pstate_t  state,		/* I - Printer state */
    const char    *location,		/* I - Printer location */
    const char    *info,		/* I - Printer information */
    const char    *make_model,		/* I - Printer make and model */
    int		  num_attrs,		/* I - Number of attributes */
    cups_option_t *attrs)		/* I - Attributes */
{
  int		update;			/* Update printer attributes? */
  char		finaluri[HTTP_MAX_URI],	/* Final URI for printer */
		method[HTTP_MAX_URI],	/* Method portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  int		port;			/* Port portion of URI */
  char		name[IPP_MAX_NAME],	/* Name of printer */
		*hptr,			/* Pointer into hostname */
		*sptr;			/* Pointer into ServerName */
  char		local_make_model[IPP_MAX_NAME];
					/* Local make and model */
  cupsd_printer_t *p;			/* Printer information */
  const char	*ipp_options;		/* ipp-options value */


 /*
  * Pull the URI apart to see if this is a local or remote printer...
  */

  httpSeparate(uri, method, username, host, &port, resource);

 /*
  * Determine if the URI contains any illegal characters in it...
  */

  if (strncmp(uri, "ipp://", 6) || !host[0] ||
      (strncmp(resource, "/printers/", 10) &&
       strncmp(resource, "/classes/", 9)))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "cupsdProcessBrowseData: Bad printer URI in browse data: %s",
                    uri);
    return;
  }

  if (strchr(resource, '?') ||
      (!strncmp(resource, "/printers/", 10) && strchr(resource + 10, '/')) ||
      (!strncmp(resource, "/classes/", 9) && strchr(resource + 9, '/')))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "cupsdProcessBrowseData: Bad resource in browse data: %s",
                    resource);
    return;
  }

 /*
  * OK, this isn't a local printer; add any remote options...
  */

  ipp_options = cupsGetOption("ipp-options", num_attrs, attrs);

  if (BrowseRemoteOptions)
  {
    if (BrowseRemoteOptions[0] == '?')
    {
     /*
      * Override server-supplied options...
      */

      snprintf(finaluri, sizeof(finaluri), "%s%s", uri, BrowseRemoteOptions);
    }
    else if (ipp_options)
    {
     /*
      * Combine the server and local options...
      */

      snprintf(finaluri, sizeof(finaluri), "%s?%s+%s", uri, ipp_options,
               BrowseRemoteOptions);
    }
    else
    {
     /*
      * Just use the local options...
      */

      snprintf(finaluri, sizeof(finaluri), "%s?%s", uri, BrowseRemoteOptions);
    }

    uri = finaluri;
  }
  else if (ipp_options)
  {
   /*
    * Just use the server-supplied options...
    */

    snprintf(finaluri, sizeof(finaluri), "%s?%s", uri, ipp_options);
    uri = finaluri;
  }

 /*
  * See if we already have it listed in the Printers list, and add it if not...
  */

  type   |= CUPS_PRINTER_REMOTE;
  type   &= ~CUPS_PRINTER_IMPLICIT;
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
      if (!strcasecmp(hptr, sptr))
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

    if (!strncmp(resource, "/classes/", 9))
      snprintf(name, sizeof(name), "%s@%s", resource + 9, host);
    else
      return;

    if ((p = cupsdFindClass(name)) == NULL && BrowseShortNames)
    {
      if ((p = cupsdFindClass(resource + 9)) != NULL)
      {
        if (p->hostname && strcasecmp(p->hostname, host))
	{
	 /*
	  * Nope, this isn't the same host; if the hostname isn't the local host,
	  * add it to the other class and then find a class using the full host
	  * name...
	  */

	  if (p->type & CUPS_PRINTER_REMOTE)
	  {
	    cupsdLogMessage(CUPSD_LOG_INFO,
	                    "Renamed remote class \"%s\" to \"%s@%s\"...",
	                    p->name, p->name, p->hostname);
	    cupsdAddEvent(CUPSD_EVENT_PRINTER_DELETED, p, NULL,
                	  "Class \'%s\' deleted by directory services.",
			  p->name);

            cupsArrayRemove(Printers, p);
            cupsdSetStringf(&p->name, "%s@%s", p->name, p->hostname);
	    cupsdSetPrinterAttrs(p);
	    cupsArrayAdd(Printers, p);

	    cupsdAddEvent(CUPSD_EVENT_PRINTER_ADDED, p, NULL,
                	  "Class \'%s\' added by directory services.",
			  p->name);
	  }

          p = NULL;
	}
	else if (!p->hostname)
	{
	 /*
	  * Hostname not set, so this must be a cached remote printer
	  * that was created for a pending print job...
	  */

          cupsdSetString(&p->hostname, host);
	  cupsdSetString(&p->uri, uri);
	  cupsdSetString(&p->device_uri, uri);
          update = 1;
        }
      }
      else
      {
       /*
        * Use the short name for this shared class.
	*/

        strlcpy(name, resource + 9, sizeof(name));
      }
    }
    else if (p && !p->hostname)
    {
     /*
      * Hostname not set, so this must be a cached remote printer
      * that was created for a pending print job...
      */

      cupsdSetString(&p->hostname, host);
      cupsdSetString(&p->uri, uri);
      cupsdSetString(&p->device_uri, uri);
      update = 1;
    }

    if (!p)
    {
     /*
      * Class doesn't exist; add it...
      */

      p = cupsdAddClass(name);

      cupsdLogMessage(CUPSD_LOG_INFO, "Added remote class \"%s\"...", name);

      cupsdAddEvent(CUPSD_EVENT_PRINTER_ADDED, p, NULL,
                    "Class \'%s\' added by directory services.", name);

     /*
      * Force the URI to point to the real server...
      */

      p->type      = type & ~CUPS_PRINTER_REJECTING;
      p->accepting = 1;
      cupsdSetString(&p->uri, uri);
      cupsdSetString(&p->device_uri, uri);
      cupsdSetString(&p->hostname, host);

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

    if ((p = cupsdFindPrinter(name)) == NULL && BrowseShortNames)
    {
      if ((p = cupsdFindPrinter(resource + 10)) != NULL)
      {
        if (p->hostname && strcasecmp(p->hostname, host))
	{
	 /*
	  * Nope, this isn't the same host; if the hostname isn't the local host,
	  * add it to the other printer and then find a printer using the full host
	  * name...
	  */

	  if (p->type & CUPS_PRINTER_REMOTE)
	  {
	    cupsdLogMessage(CUPSD_LOG_INFO,
	                    "Renamed remote printer \"%s\" to \"%s@%s\"...",
	                    p->name, p->name, p->hostname);
	    cupsdAddEvent(CUPSD_EVENT_PRINTER_DELETED, p, NULL,
                	  "Printer \'%s\' deleted by directory services.",
			  p->name);

	    cupsArrayRemove(Printers, p);
	    cupsdSetStringf(&p->name, "%s@%s", p->name, p->hostname);
	    cupsdSetPrinterAttrs(p);
	    cupsArrayAdd(Printers, p);

	    cupsdAddEvent(CUPSD_EVENT_PRINTER_ADDED, p, NULL,
                	  "Printer \'%s\' added by directory services.",
			  p->name);
	  }

          p = NULL;
	}
	else if (!p->hostname)
	{
	 /*
	  * Hostname not set, so this must be a cached remote printer
	  * that was created for a pending print job...
	  */

          cupsdSetString(&p->hostname, host);
	  cupsdSetString(&p->uri, uri);
	  cupsdSetString(&p->device_uri, uri);
          update = 1;
        }
      }
      else
      {
       /*
        * Use the short name for this shared printer.
	*/

        strlcpy(name, resource + 10, sizeof(name));
      }
    }
    else if (p && !p->hostname)
    {
     /*
      * Hostname not set, so this must be a cached remote printer
      * that was created for a pending print job...
      */

      cupsdSetString(&p->hostname, host);
      cupsdSetString(&p->uri, uri);
      cupsdSetString(&p->device_uri, uri);
      update = 1;
    }

    if (!p)
    {
     /*
      * Printer doesn't exist; add it...
      */

      p = cupsdAddPrinter(name);

      cupsdAddEvent(CUPSD_EVENT_PRINTER_ADDED, p, NULL,
                    "Printer \'%s\' added by directory services.", name);

      cupsdLogMessage(CUPSD_LOG_INFO, "Added remote printer \"%s\"...", name);

     /*
      * Force the URI to point to the real server...
      */

      p->type      = type & ~CUPS_PRINTER_REJECTING;
      p->accepting = 1;
      cupsdSetString(&p->hostname, host);
      cupsdSetString(&p->uri, uri);
      cupsdSetString(&p->device_uri, uri);

      update = 1;
    }
  }

 /*
  * Update the state...
  */

  p->state       = state;
  p->browse_time = time(NULL);

  if (type & CUPS_PRINTER_REJECTING)
  {
    type &= ~CUPS_PRINTER_REJECTING;

    if (p->accepting)
    {
      update       = 1;
      p->accepting = 0;
    }
  }
  else if (!p->accepting)
  {
    update       = 1;
    p->accepting = 1;
  }

  if (p->type != type)
  {
    p->type = type;
    update  = 1;
  }

  if (location && (!p->location || strcmp(p->location, location)))
  {
    cupsdSetString(&p->location, location);
    update = 1;
  }

  if (info && (!p->info || strcmp(p->info, info)))
  {
    cupsdSetString(&p->info, info);
    update = 1;
  }

  if (!make_model || !make_model[0])
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

  if (!p->make_model || strcmp(p->make_model, local_make_model))
  {
    cupsdSetString(&p->make_model, local_make_model);
    update = 1;
  }

  if (type & CUPS_PRINTER_DELETE)
  {
    cupsdAddEvent(CUPSD_EVENT_PRINTER_DELETED, p, NULL,
                  "%s \'%s\' deleted by directory services.",
		  (type & CUPS_PRINTER_CLASS) ? "Class" : "Printer", p->name);

    cupsdExpireSubscriptions(p, NULL);
 
    cupsdDeletePrinter(p, 1);
    cupsdUpdateImplicitClasses();
  }
  else if (update)
  {
    cupsdSetPrinterAttrs(p);
    cupsdUpdateImplicitClasses();
  }

 /*
  * See if we have a default printer...  If not, make the first printer the
  * default.
  */

  if (DefaultPrinter == NULL && Printers != NULL)
  {
    DefaultPrinter = (cupsd_printer_t *)cupsArrayFirst(Printers);

    cupsdWritePrintcap();
  }

 /*
  * Do auto-classing if needed...
  */

  cupsdProcessImplicitClasses();
}


/*
 * 'cupsdProcessImplicitClasses()' - Create/update implicit classes as needed.
 */

void
cupsdProcessImplicitClasses(void)
{
  int		i;			/* Looping var */
  int		update;			/* Update printer attributes? */
  char		name[IPP_MAX_NAME],	/* Name of printer */
		*hptr;			/* Pointer into hostname */
  cupsd_printer_t *p,			/* Printer information */
		*pclass,		/* Printer class */
		*first;			/* First printer in class */
  int		offset,			/* Offset of name */
		len;			/* Length of name */


  if (!ImplicitClasses || !Printers)
    return;

 /*
  * Loop through all available printers and create classes as needed...
  */

  for (p = (cupsd_printer_t *)cupsArrayFirst(Printers), len = 0, offset = 0,
           update = 0, pclass = NULL, first = NULL;
       p != NULL;
       p = (cupsd_printer_t *)cupsArrayNext(Printers))
  {
   /*
    * Skip implicit classes...
    */

    if (p->type & CUPS_PRINTER_IMPLICIT)
    {
      len = 0;
      continue;
    }

   /*
    * If len == 0, get the length of this printer name up to the "@"
    * sign (if any).
    */

    cupsArraySave(Printers);

    if (len > 0 &&
	strncasecmp(p->name, name + offset, len) == 0 &&
	(p->name[len] == '\0' || p->name[len] == '@'))
    {
     /*
      * We have more than one printer with the same name; see if
      * we have a class, and if this printer is a member...
      */

      if (pclass && strcasecmp(pclass->name, name))
      {
	if (update)
	  cupsdSetPrinterAttrs(pclass);

	update = 0;
	pclass = NULL;
      }

      if (!pclass && (pclass = cupsdFindDest(name)) == NULL)
      {
       /*
	* Need to add the class...
	*/

	pclass = cupsdAddPrinter(name);
	cupsArrayAdd(ImplicitPrinters, pclass);

	pclass->type      |= CUPS_PRINTER_IMPLICIT;
	pclass->accepting = 1;
	pclass->state     = IPP_PRINTER_IDLE;

        cupsdSetString(&pclass->location, p->location);
        cupsdSetString(&pclass->info, p->info);

        update = 1;

        cupsdLogMessage(CUPSD_LOG_INFO, "Added implicit class \"%s\"...",
	                name);
      }

      if (first != NULL)
      {
        for (i = 0; i < pclass->num_printers; i ++)
	  if (pclass->printers[i] == first)
	    break;

        if (i >= pclass->num_printers)
	{
	  first->in_implicit_class = 1;
	  cupsdAddPrinterToClass(pclass, first);
        }

	first = NULL;
      }

      for (i = 0; i < pclass->num_printers; i ++)
	if (pclass->printers[i] == p)
	  break;

      if (i >= pclass->num_printers)
      {
	p->in_implicit_class = 1;
	cupsdAddPrinterToClass(pclass, p);
	update = 1;
      }
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

      if ((first = (hptr ? cupsdFindDest(name) : p)) != NULL &&
	  !(first->type & CUPS_PRINTER_IMPLICIT))
      {
       /*
	* Can't use same name as a local printer; add "Any" to the
	* front of the name, unless we have explicitly disabled
	* the "ImplicitAnyClasses"...
	*/

        if (ImplicitAnyClasses && len < (sizeof(name) - 4))
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
          cupsArrayRestore(Printers);
	  continue;
	}
      }

      first = p;
    }

    cupsArrayRestore(Printers);
  }

 /*
  * Update the last printer class as needed...
  */

  if (pclass && update)
    cupsdSetPrinterAttrs(pclass);
}


/*
 * 'cupsdSaveRemoteCache()' - Save the remote printer cache.
 */

void
cupsdSaveRemoteCache(void)
{
  int			i;		/* Looping var */
  cups_file_t		*fp;		/* printers.conf file */
  char			temp[1024];	/* Temporary string */
  cupsd_printer_t	*printer;	/* Current printer class */
  time_t		curtime;	/* Current time */
  struct tm		*curdate;	/* Current date */


 /*
  * Create the remote.cache file...
  */

  snprintf(temp, sizeof(temp), "%s/remote.cache", CacheDir);

  if ((fp = cupsFileOpen(temp, "w")) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to save remote.cache - %s", strerror(errno));
    return;
  }
  else
    cupsdLogMessage(CUPSD_LOG_INFO, "Saving remote.cache...");

 /*
  * Restrict access to the file...
  */

  fchown(cupsFileNumber(fp), getuid(), Group);
  fchmod(cupsFileNumber(fp), ConfigFilePerm);

 /*
  * Write a small header to the file...
  */

  curtime = time(NULL);
  curdate = localtime(&curtime);
  strftime(temp, sizeof(temp) - 1, "%Y-%m-%d %H:%M", curdate);

  cupsFilePuts(fp, "# Remote cache file for " CUPS_SVERSION "\n");
  cupsFilePrintf(fp, "# Written by cupsd on %s\n", temp);

 /*
  * Write each local printer known to the system...
  */

  for (printer = (cupsd_printer_t *)cupsArrayFirst(Printers);
       printer;
       printer = (cupsd_printer_t *)cupsArrayNext(Printers))
  {
   /*
    * Skip local destinations...
    */

    if (!(printer->type & CUPS_PRINTER_REMOTE))
      continue;

   /*
    * Write printers as needed...
    */

    if (printer == DefaultPrinter)
      cupsFilePuts(fp, "<Default");
    else
      cupsFilePutChar(fp, '<');

    if (printer->type & CUPS_PRINTER_CLASS)
      cupsFilePrintf(fp, "Class %s>\n", printer->name);
    else
      cupsFilePrintf(fp, "Printer %s>\n", printer->name);

    cupsFilePrintf(fp, "Type %d\n", printer->type);

    cupsFilePrintf(fp, "BrowseTime %d\n", (int)printer->browse_time);

    if (printer->info)
      cupsFilePrintf(fp, "Info %s\n", printer->info);

    if (printer->make_model)
      cupsFilePrintf(fp, "MakeModel %s\n", printer->make_model);

    if (printer->location)
      cupsFilePrintf(fp, "Location %s\n", printer->location);

    if (printer->device_uri)
      cupsFilePrintf(fp, "DeviceURI %s\n", printer->device_uri);

    if (printer->state == IPP_PRINTER_STOPPED)
    {
      cupsFilePuts(fp, "State Stopped\n");
      cupsFilePrintf(fp, "StateMessage %s\n", printer->state_message);
    }
    else
      cupsFilePuts(fp, "State Idle\n");

    if (printer->accepting)
      cupsFilePuts(fp, "Accepting Yes\n");
    else
      cupsFilePuts(fp, "Accepting No\n");

    cupsFilePrintf(fp, "JobSheets %s %s\n", printer->job_sheets[0],
            printer->job_sheets[1]);

    for (i = 0; i < printer->num_users; i ++)
      cupsFilePrintf(fp, "%sUser %s\n", printer->deny_users ? "Deny" : "Allow",
              printer->users[i]);

    if (printer->type & CUPS_PRINTER_CLASS)
      cupsFilePuts(fp, "</Class>\n");
    else
      cupsFilePuts(fp, "</Printer>\n");
  }

  cupsFileClose(fp);
}


/*
 * 'cupsdSendBrowseDelete()' - Send a "browse delete" message for a printer.
 */

void
cupsdSendBrowseDelete(
    cupsd_printer_t *p)			/* I - Printer to delete */
{
 /*
  * Only announce if browsing is enabled...
  */

  if (!Browsing || !p->shared)
    return;

 /*
  * First mark the printer for deletion...
  */

  p->type |= CUPS_PRINTER_DELETE;

 /*
  * Announce the deletion...
  */

  if (BrowseLocalProtocols & BROWSE_CUPS)
    cupsdSendCUPSBrowse(p);
#ifdef HAVE_LIBSLP
  if (BrowseLocalProtocols & BROWSE_SLP)
    slp_dereg_printer(p);
#endif /* HAVE_LIBSLP */
}


/*
 * 'cupsdSendBrowseList()' - Send new browsing information as necessary.
 */

void
cupsdSendBrowseList(void)
{
  int			count;		/* Number of dests to update */
  cupsd_printer_t	*p;		/* Current printer */
  time_t		ut,		/* Minimum update time */
			to;		/* Timeout time */


  if (!Browsing || !BrowseLocalProtocols || !Printers)
    return;

 /*
  * Compute the update and timeout times...
  */

  ut = time(NULL) - BrowseInterval;
  to = time(NULL) - BrowseTimeout;

 /*
  * Figure out how many printers need an update...
  */

  if (BrowseInterval > 0)
  {
    int	max_count;			/* Maximum number to update */


   /*
    * Throttle the number of printers we'll be updating this time
    * around based on the number of queues that need updating and
    * the maximum number of queues to update each second...
    */

    max_count = 2 * cupsArrayCount(Printers) / BrowseInterval + 1;

    for (count = 0, p = (cupsd_printer_t *)cupsArrayFirst(Printers);
         count < max_count && p != NULL;
	 p = (cupsd_printer_t *)cupsArrayNext(Printers))
      if (!(p->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT)) &&
          p->shared && p->browse_time < ut)
        count ++;

   /*
    * Loop through all of the printers and send local updates as needed...
    */

    if (BrowseNext)
      p = (cupsd_printer_t *)cupsArrayFind(Printers, BrowseNext);
    else
      p = (cupsd_printer_t *)cupsArrayFirst(Printers);

    for (;
         count > 0;
	 p = (cupsd_printer_t *)cupsArrayNext(Printers))
    {
     /*
      * Check for wraparound...
      */

      if (!p)
        p = (cupsd_printer_t *)cupsArrayFirst(Printers);

      if (!p)
        break;
      else if ((p->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT)) ||
               !p->shared)
        continue;
      else if (p->browse_time < ut)
      {
       /*
	* Need to send an update...
	*/

	count --;

	p->browse_time = time(NULL);

	if (BrowseLocalProtocols & BROWSE_CUPS)
          cupsdSendCUPSBrowse(p);

#ifdef HAVE_LIBSLP
	if (BrowseLocalProtocols & BROWSE_SLP)
          cupsdSendSLPBrowse(p);
#endif /* HAVE_LIBSLP */
      }
    }

   /*
    * Save where we left off so that all printers get updated...
    */

    BrowseNext = p;
  }

 /*
  * Loop through all of the printers and send local updates as needed...
  */

  for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
       p;
       p = (cupsd_printer_t *)cupsArrayNext(Printers))
  {
   /*
    * If this is a remote queue, see if it needs to be timed out...
    */

    if (p->type & CUPS_PRINTER_REMOTE)
    {
      if (p->browse_time < to)
      {
	cupsdAddEvent(CUPSD_EVENT_PRINTER_DELETED, p, NULL,
                      "%s \'%s\' deleted by directory services (timeout).",
		      (p->type & CUPS_PRINTER_CLASS) ? "Class" : "Printer",
		      p->name);

        cupsdLogMessage(CUPSD_LOG_INFO,
	                "Remote destination \"%s\" has timed out; deleting it...",
	                p->name);

        cupsArraySave(Printers);
        cupsdDeletePrinter(p, 1);
        cupsArrayRestore(Printers);
      }
    }
  }
}


/*
 * 'cupsdSendCUPSBrowse()' - Send new browsing information using the CUPS protocol.
 */

void
cupsdSendCUPSBrowse(cupsd_printer_t *p)	/* I - Printer to send */
{
  int			i;		/* Looping var */
  cups_ptype_t		type;		/* Printer type */
  cupsd_dirsvc_addr_t	*b;		/* Browse address */
  int			bytes;		/* Length of packet */
  char			packet[1453];	/* Browse data packet */
  char			uri[1024];	/* Printer URI */
  char			options[1024];	/* Browse local options */
  cupsd_netif_t		*iface;		/* Network interface */


 /*
  * Figure out the printer type value...
  */

  type = p->type | CUPS_PRINTER_REMOTE;

  if (!p->accepting)
    type |= CUPS_PRINTER_REJECTING;

 /*
  * Initialize the browse options...
  */

  if (BrowseLocalOptions)
    snprintf(options, sizeof(options), " ipp-options=%s", BrowseLocalOptions);
  else
    options[0] = '\0';

 /*
  * Send a packet to each browse address...
  */

  for (i = NumBrowsers, b = Browsers; i > 0; i --, b ++)
    if (b->iface[0])
    {
     /*
      * Send the browse packet to one or more interfaces...
      */

      if (strcmp(b->iface, "*") == 0)
      {
       /*
        * Send to all local interfaces...
	*/

        cupsdNetIFUpdate();

        for (iface = NetIFList; iface != NULL; iface = iface->next)
	{
	 /*
	  * Only send to local, IPv4 interfaces...
	  */

	  if (!iface->is_local || !iface->port ||
	      iface->address.addr.sa_family != AF_INET)
	    continue;

	  httpAssembleURIf(uri, sizeof(uri), "ipp", NULL, iface->hostname,
	                   iface->port,
			   (p->type & CUPS_PRINTER_CLASS) ? "/classes/%s%s" :
			                                    "/printers/%s",
			   p->name);
	  snprintf(packet, sizeof(packet), "%x %x %s \"%s\" \"%s\" \"%s\"%s\n",
        	   type, p->state, uri, p->location ? p->location : "",
		   p->info ? p->info : "",
		   p->make_model ? p->make_model : "Unknown", options);

	  bytes = strlen(packet);

	  cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                  "cupsdSendBrowseList: (%d bytes to \"%s\") %s", bytes,
        	          iface->name, packet);

          iface->broadcast.ipv4.sin_port = htons(BrowsePort);

	  sendto(BrowseSocket, packet, bytes, 0,
		 (struct sockaddr *)&(iface->broadcast),
		 sizeof(struct sockaddr_in));
        }
      }
      else if ((iface = cupsdNetIFFind(b->iface)) != NULL)
      {
       /*
        * Send to the named interface using the IPv4 address...
	*/

        while (iface)
	  if (strcasecmp(b->iface, iface->name))
	  {
	    iface = NULL;
	    break;
	  }
	  else if (iface->address.addr.sa_family == AF_INET && iface->port)
	    break;
	  else
	    iface = iface->next;

        if (iface)
	{
	  httpAssembleURIf(uri, sizeof(uri), "ipp", NULL, iface->hostname,
	                   iface->port,
			   (p->type & CUPS_PRINTER_CLASS) ? "/classes/%s%s" :
			                                    "/printers/%s",
			   p->name);
	  snprintf(packet, sizeof(packet), "%x %x %s \"%s\" \"%s\" \"%s\"%s\n",
        	   type, p->state, uri, p->location ? p->location : "",
		   p->info ? p->info : "",
		   p->make_model ? p->make_model : "Unknown", options);

	  bytes = strlen(packet);

	  cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                  "cupsdSendBrowseList: (%d bytes to \"%s\") %s", bytes,
        	          iface->name, packet);

          iface->broadcast.ipv4.sin_port = htons(BrowsePort);

	  sendto(BrowseSocket, packet, bytes, 0,
		 (struct sockaddr *)&(iface->broadcast),
		 sizeof(struct sockaddr_in));
        }
      }
    }
    else
    {
     /*
      * Send the browse packet to the indicated address using
      * the default server name...
      */

      snprintf(packet, sizeof(packet), "%x %x %s \"%s\" \"%s\" \"%s\"%s\n",
       	       type, p->state, p->uri,
	       p->location ? p->location : "",
	       p->info ? p->info : "",
	       p->make_model ? p->make_model : "Unknown", options);

      bytes = strlen(packet);
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdSendBrowseList: (%d bytes) %s", bytes, packet);

      if (sendto(BrowseSocket, packet, bytes, 0,
		 (struct sockaddr *)&(b->to),
		 sizeof(struct sockaddr_in)) <= 0)
      {
       /*
        * Unable to send browse packet, so remove this address from the
	* list...
	*/

	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "cupsdSendBrowseList: sendto failed for browser %d - %s.",
	                b - Browsers + 1, strerror(errno));

        if (i > 1)
	  memcpy(b, b + 1, (i - 1) * sizeof(cupsd_dirsvc_addr_t));

	b --;
	NumBrowsers --;
      }
    }
}


#ifdef HAVE_LIBSLP
/*
 * 'cupsdSendSLPBrowse()' - Register the specified printer with SLP.
 */

void 
cupsdSendSLPBrowse(cupsd_printer_t *p)	/* I - Printer to register */
{
  char		srvurl[HTTP_MAX_URI],	/* Printer service URI */
		attrs[8192],		/* Printer attributes */
		finishings[1024],	/* Finishings to support */
		make_model[IPP_MAX_NAME * 2],
					/* Make and model, quoted */
		location[IPP_MAX_NAME * 2],
					/* Location, quoted */
		info[IPP_MAX_NAME * 2],	/* Info, quoted */
		*src,			/* Pointer to original string */
		*dst;			/* Pointer to destination string */
  ipp_attribute_t *authentication;	/* uri-authentication-supported value */
  SLPError	error;			/* SLP error, if any */


  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdSendSLPBrowse(%p = \"%s\")", p,
                  p->name);

 /*
  * Make the SLP service URL that conforms to the IANA 
  * 'printer:' template.
  */

  snprintf(srvurl, sizeof(srvurl), SLP_CUPS_SRVTYPE ":%s", p->uri);

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "Service URL = \"%s\"", srvurl);

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
      strlcat(finishings, ",bind", sizeof(finishings));
    else
      strcpy(finishings, "bind");
  }

  if (p->type & CUPS_PRINTER_PUNCH)
  {
    if (finishings[0])
      strlcat(finishings, ",punch", sizeof(finishings));
    else
      strcpy(finishings, "punch");
  }

  if (p->type & CUPS_PRINTER_COVER)
  {
    if (finishings[0])
      strlcat(finishings, ",cover", sizeof(finishings));
    else
      strcpy(finishings, "cover");
  }

  if (p->type & CUPS_PRINTER_SORT)
  {
    if (finishings[0])
      strlcat(finishings, ",sort", sizeof(finishings));
    else
      strcpy(finishings, "sort");
  }

  if (!finishings[0])
    strcpy(finishings, "none");

 /*
  * Quote any commas in the make and model, location, and info strings...
  */

  for (src = p->make_model, dst = make_model;
       src && *src && dst < (make_model + sizeof(make_model) - 2);)
  {
    if (*src == ',' || *src == '\\' || *src == ')')
      *dst++ = '\\';

    *dst++ = *src++;
  }

  *dst = '\0';

  if (!make_model[0])
    strcpy(make_model, "Unknown");

  for (src = p->location, dst = location;
       src && *src && dst < (location + sizeof(location) - 2);)
  {
    if (*src == ',' || *src == '\\' || *src == ')')
      *dst++ = '\\';

    *dst++ = *src++;
  }

  *dst = '\0';

  if (!location[0])
    strcpy(location, "Unknown");

  for (src = p->info, dst = info;
       src && *src && dst < (info + sizeof(info) - 2);)
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
#ifdef HAVE_SSL
           "(uri-security-supported=tls>),"
#else
           "(uri-security-supported=none>),"
#endif /* HAVE_SSL */
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

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "Attributes = \"%s\"", attrs);

 /*
  * Register the printer with the SLP server...
  */

  error = SLPReg(BrowseSLPHandle, srvurl, BrowseTimeout,
	         SLP_CUPS_SRVTYPE, attrs, SLP_TRUE, slp_reg_callback, 0);

  if (error != SLP_OK)
    cupsdLogMessage(CUPSD_LOG_ERROR, "SLPReg of \"%s\" failed with status %d!", p->name,
                    error);
}
#endif /* HAVE_LIBSLP */


/*
 * 'cupsdStartBrowsing()' - Start sending and receiving broadcast information.
 */

void
cupsdStartBrowsing(void)
{
  int			val;		/* Socket option value */
  struct sockaddr_in	addr;		/* Broadcast address */


  BrowseNext = NULL;

  if (!Browsing || !(BrowseLocalProtocols | BrowseRemoteProtocols))
    return;

  if ((BrowseLocalProtocols | BrowseRemoteProtocols) & BROWSE_CUPS)
  {
   /*
    * Create the broadcast socket...
    */

    if ((BrowseSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "cupsdStartBrowsing: Unable to create broadcast socket - %s.",
        	      strerror(errno));
      BrowseLocalProtocols &= ~BROWSE_CUPS;
      BrowseRemoteProtocols &= ~BROWSE_CUPS;
      return;
    }

   /*
    * Set the "broadcast" flag...
    */

    val = 1;
    if (setsockopt(BrowseSocket, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)))
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "cupsdStartBrowsing: Unable to set broadcast mode - %s.",
        	      strerror(errno));

#ifdef WIN32
      closesocket(BrowseSocket);
#else
      close(BrowseSocket);
#endif /* WIN32 */

      BrowseSocket = -1;
      BrowseLocalProtocols &= ~BROWSE_CUPS;
      BrowseRemoteProtocols &= ~BROWSE_CUPS;
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
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "cupsdStartBrowsing: Unable to bind broadcast socket - %s.",
        	      strerror(errno));

#ifdef WIN32
      closesocket(BrowseSocket);
#else
      close(BrowseSocket);
#endif /* WIN32 */

      BrowseSocket = -1;
      BrowseLocalProtocols &= ~BROWSE_CUPS;
      BrowseRemoteProtocols &= ~BROWSE_CUPS;
      return;
    }

   /*
    * Close the socket on exec...
    */

    fcntl(BrowseSocket, F_SETFD, fcntl(BrowseSocket, F_GETFD) | FD_CLOEXEC);

   /*
    * Finally, add the socket to the input selection set...
    */

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdStartBrowsing: Adding fd %d to InputSet...",
                    BrowseSocket);

    FD_SET(BrowseSocket, InputSet);
  }
  else
    BrowseSocket = -1;

#ifdef HAVE_LIBSLP
  if ((BrowseLocalProtocols | BrowseRemoteProtocols) & BROWSE_SLP)
  {
   /* 
    * Open SLP handle...
    */

    if (SLPOpen("en", SLP_FALSE, &BrowseSLPHandle) != SLP_OK)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to open an SLP handle; disabling SLP browsing!");
      BrowseLocalProtocols &= ~BROWSE_SLP;
      BrowseRemoteProtocols &= ~BROWSE_SLP;
    }

    BrowseSLPRefresh = 0;
  }
#endif /* HAVE_LIBSLP */
}


/*
 * 'cupsdStartPolling()' - Start polling servers as needed.
 */

void
cupsdStartPolling(void)
{
  int			i;		/* Looping var */
  cupsd_dirsvc_poll_t	*pollp;		/* Current polling server */
  char			polld[1024];	/* Poll daemon path */
  char			sport[10];	/* Server port */
  char			bport[10];	/* Browser port */
  char			interval[10];	/* Poll interval */
  int			statusfds[2];	/* Status pipe */
  char			*argv[6];	/* Arguments */
  char			*envp[100];	/* Environment */


 /*
  * Don't do anything if we aren't polling...
  */

  if (NumPolled == 0)
  {
    PollPipe         = -1;
    PollStatusBuffer = NULL;
    return;
  }

 /*
  * Setup string arguments for polld, port and interval options.
  */

  snprintf(polld, sizeof(polld), "%s/daemon/cups-polld", ServerBin);

  sprintf(bport, "%d", BrowsePort);

  if (BrowseInterval)
    sprintf(interval, "%d", BrowseInterval);
  else
    strcpy(interval, "30");

  argv[0] = "cups-polld";
  argv[2] = sport;
  argv[3] = interval;
  argv[4] = bport;
  argv[5] = NULL;

  cupsdLoadEnv(envp, (int)(sizeof(envp) / sizeof(envp[0])));

 /*
  * Create a pipe that receives the status messages from each
  * polling daemon...
  */

  if (cupsdOpenPipe(statusfds))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to create polling status pipes - %s.",
	            strerror(errno));
    PollPipe         = -1;
    PollStatusBuffer = NULL;
    return;
  }

  PollPipe         = statusfds[0];
  PollStatusBuffer = cupsdStatBufNew(PollPipe, "[Poll]");

 /*
  * Run each polling daemon, redirecting stderr to the polling pipe...
  */

  for (i = 0, pollp = Polled; i < NumPolled; i ++, pollp ++)
  {
    sprintf(sport, "%d", pollp->port);

    argv[1] = pollp->hostname;

    if (cupsdStartProcess(polld, argv, envp, -1, -1, statusfds[1], -1,
                          0, &(pollp->pid)) < 0)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "cupsdStartPolling: Unable to fork polling daemon - %s",
                      strerror(errno));
      pollp->pid = 0;
      break;
    }
    else
      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "cupsdStartPolling: Started polling daemon for %s:%d, pid = %d",
                      pollp->hostname, pollp->port, pollp->pid);
  }

  close(statusfds[1]);

 /*
  * Finally, add the pipe to the input selection set...
  */

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdStartPolling: Adding fd %d to InputSet...", PollPipe);

  FD_SET(PollPipe, InputSet);
}


/*
 * 'cupsdStopBrowsing()' - Stop sending and receiving broadcast information.
 */

void
cupsdStopBrowsing(void)
{
  if (!Browsing || !(BrowseLocalProtocols | BrowseRemoteProtocols))
    return;

  if ((BrowseLocalProtocols | BrowseRemoteProtocols) & BROWSE_CUPS)
  {
   /*
    * Close the socket and remove it from the input selection set.
    */

    if (BrowseSocket >= 0)
    {
#ifdef WIN32
      closesocket(BrowseSocket);
#else
      close(BrowseSocket);
#endif /* WIN32 */

      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdStopBrowsing: Removing fd %d from InputSet...",
        	      BrowseSocket);

      FD_CLR(BrowseSocket, InputSet);
      BrowseSocket = -1;
    }
  }

#ifdef HAVE_LIBSLP
  if ((BrowseLocalProtocols | BrowseRemoteProtocols) & BROWSE_SLP)
  {
   /* 
    * Close SLP handle...
    */

    SLPClose(BrowseSLPHandle);
  }
#endif /* HAVE_LIBSLP */
}


/*
 * 'cupsdStopPolling()' - Stop polling servers as needed.
 */

void
cupsdStopPolling(void)
{
  int			i;		/* Looping var */
  cupsd_dirsvc_poll_t	*pollp;		/* Current polling server */


  if (PollPipe >= 0)
  {
    cupsdStatBufDelete(PollStatusBuffer);
    close(PollPipe);

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdStopPolling: removing fd %d from InputSet.", PollPipe);
    FD_CLR(PollPipe, InputSet);

    PollPipe         = -1;
    PollStatusBuffer = NULL;
  }

  for (i = 0, pollp = Polled; i < NumPolled; i ++, pollp ++)
    if (pollp->pid)
      cupsdEndProcess(pollp->pid, 0);
}


/*
 * 'cupsdUpdateCUPSBrowse()' - Update the browse lists using the CUPS protocol.
 */

void
cupsdUpdateCUPSBrowse(void)
{
  int		i;			/* Looping var */
  int		auth;			/* Authorization status */
  int		len;			/* Length of name string */
  int		bytes;			/* Number of bytes left */
  char		packet[1541],		/* Broadcast packet */
		*pptr;			/* Pointer into packet */
  socklen_t	srclen;			/* Length of source address */
  http_addr_t	srcaddr;		/* Source address */
  char		srcname[1024];		/* Source hostname */
  unsigned	address[4];		/* Source address */
  unsigned	type;			/* Printer type */
  unsigned	state;			/* Printer state */
  char		uri[HTTP_MAX_URI],	/* Printer URI */
		method[HTTP_MAX_URI],	/* Method portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI],	/* Resource portion of URI */
		info[IPP_MAX_NAME],	/* Information string */
		location[IPP_MAX_NAME],	/* Location string */
		make_model[IPP_MAX_NAME];/* Make and model string */
  int		port;			/* Port portion of URI */
  cupsd_netif_t	*iface;			/* Network interface */
  int		num_attrs;		/* Number of attributes */
  cups_option_t	*attrs;			/* Attributes */


 /*
  * Read a packet from the browse socket...
  */

  srclen = sizeof(srcaddr);
  if ((bytes = recvfrom(BrowseSocket, packet, sizeof(packet) - 1, 0, 
                        (struct sockaddr *)&srcaddr, &srclen)) < 0)
  {
   /*
    * "Connection refused" is returned under Linux if the destination port
    * or address is unreachable from a previous sendto(); check for the
    * error here and ignore it for now...
    */

    if (errno != ECONNREFUSED && errno != EAGAIN)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Browse recv failed - %s.",
                      strerror(errno));
      cupsdLogMessage(CUPSD_LOG_ERROR, "Browsing turned off.");

      cupsdStopBrowsing();
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
    address[0] = 0;
    address[1] = 0;
    address[2] = 0;
    address[3] = ntohl(srcaddr.ipv4.sin_addr.s_addr);
  }

  if (HostNameLookups)
    httpAddrLookup(&srcaddr, srcname, sizeof(srcname));
  else
    httpAddrString(&srcaddr, srcname, sizeof(srcname));

  len = strlen(srcname);

 /*
  * Do ACL stuff...
  */

  if (BrowseACL)
  {
    if (httpAddrLocalhost(&srcaddr) || !strcasecmp(srcname, "localhost"))
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

            if (cupsdCheckAuth(address, srcname, len,
	        	  BrowseACL->num_deny, BrowseACL->deny))
	      auth = AUTH_DENY;

            if (cupsdCheckAuth(address, srcname, len,
	        	  BrowseACL->num_allow, BrowseACL->allow))
	      auth = AUTH_ALLOW;
	    break;

	case AUTH_DENY : /* Order Allow,Deny */
            auth = AUTH_DENY;

            if (cupsdCheckAuth(address, srcname, len,
	        	  BrowseACL->num_allow, BrowseACL->allow))
	      auth = AUTH_ALLOW;

            if (cupsdCheckAuth(address, srcname, len,
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
    cupsdLogMessage(CUPSD_LOG_DEBUG,
                    "cupsdUpdateCUPSBrowse: Refused %d bytes from %s", bytes,
                    srcname);
    return;
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdUpdateCUPSBrowse: (%d bytes from %s) %s", bytes,
		  srcname, packet);

 /*
  * Parse packet...
  */

  if (sscanf(packet, "%x%x%1023s", &type, &state, uri) < 3)
  {
    cupsdLogMessage(CUPSD_LOG_WARN,
                    "cupsdUpdateCUPSBrowse: Garbled browse packet - %s", packet);
    return;
  }

  strcpy(location, "Location Unknown");
  strcpy(info, "No Information Available");
  make_model[0] = '\0';
  num_attrs     = 0;
  attrs         = NULL;

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

    while (*pptr && isspace(*pptr & 255))
      pptr ++;

    if (*pptr == '\"')
    {
      for (i = 0, pptr ++;
           i < (sizeof(info) - 1) && *pptr && *pptr != '\"';
           i ++, pptr ++)
	info[i] = *pptr;

      info[i] = '\0';

      if (*pptr == '\"')
	pptr ++;

      while (*pptr && isspace(*pptr & 255))
	pptr ++;

      if (*pptr == '\"')
      {
	for (i = 0, pptr ++;
             i < (sizeof(make_model) - 1) && *pptr && *pptr != '\"';
             i ++, pptr ++)
	  make_model[i] = *pptr;

	if (*pptr == '\"')
	  pptr ++;

	make_model[i] = '\0';

        if (*pptr)
	  num_attrs = cupsParseOptions(pptr, num_attrs, &attrs);
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

 /*
  * Check for packets from the local server...
  */

  if (!strcasecmp(host, ServerName) && port == LocalPort)
  {
    cupsFreeOptions(num_attrs, attrs);
    return;
  }

  cupsdNetIFUpdate();

  for (iface = NetIFList; iface != NULL; iface = iface->next)
    if (!strcasecmp(host, iface->hostname) && port == iface->port)
    {
      cupsFreeOptions(num_attrs, attrs);
      return;
    }

 /*
  * Do relaying...
  */

  for (i = 0; i < NumRelays; i ++)
    if (cupsdCheckAuth(address, srcname, len, 1, &(Relays[i].from)))
      if (sendto(BrowseSocket, packet, bytes, 0,
                 (struct sockaddr *)&(Relays[i].to),
		 sizeof(http_addr_t)) <= 0)
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "cupsdUpdateCUPSBrowse: sendto failed for relay %d - %s.",
	                i + 1, strerror(errno));
	cupsFreeOptions(num_attrs, attrs);
	return;
      }

 /*
  * Process the browse data...
  */

  cupsdProcessBrowseData(uri, (cups_ptype_t)type, (ipp_pstate_t)state, location,
                         info, make_model, num_attrs, attrs);
  cupsFreeOptions(num_attrs, attrs);
}


/*
 * 'cupsdUpdatePolling()' - Read status messages from the poll daemons.
 */

void
cupsdUpdatePolling(void)
{
  char		*ptr,			/* Pointer to end of line in buffer */
		message[1024];		/* Pointer to message text */
  int		loglevel;		/* Log level for message */


  while ((ptr = cupsdStatBufUpdate(PollStatusBuffer, &loglevel,
                                   message, sizeof(message))) != NULL)
    if (!strchr(PollStatusBuffer->buffer, '\n'))
      break;

  if (ptr == NULL)
  {
   /*
    * All polling processes have died; stop polling...
    */

    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "cupsdUpdatePolling: all polling processes have exited!");
    cupsdStopPolling();
  }
}


#ifdef HAVE_LIBSLP 
/*
 * 'cupsdUpdateSLPBrowse()' - Get browsing information via SLP.
 */

void
cupsdUpdateSLPBrowse(void)
{
  slpsrvurl_t		*s,		/* Temporary list of service URLs */
			*next;		/* Next service in list */
  cupsd_printer_t	p;		/* Printer information */
  const char		*uri;		/* Pointer to printer URI */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */


  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdUpdateSLPBrowse() Start...");

 /*
  * Reset the refresh time...
  */

  BrowseSLPRefresh = time(NULL) + BrowseInterval;

 /* 
  * Poll for remote printers using SLP...
  */

  s = NULL;

  SLPFindSrvs(BrowseSLPHandle, SLP_CUPS_SRVTYPE, "", "",
	      slp_url_callback, &s);

 /*
  * Loop through the list of available printers...
  */

  for (; s; s = next)
  {
   /*
    * Save the "next" pointer...
    */

    next = s->next;

   /* 
    * Load a cupsd_printer_t structure with the SLP service attributes...
    */

    SLPFindAttrs(BrowseSLPHandle, s->url, "", "", slp_attr_callback, &p);

   /*
    * Process this printer entry...
    */

    uri = s->url + SLP_CUPS_SRVLEN + 1;

    if (!strncmp(uri, "http://", 7) || !strncmp(uri, "ipp://", 6))
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
        cupsdProcessBrowseData(uri, p.type, IPP_PRINTER_IDLE, p.location,
	                  p.info, p.make_model);
      else if (strstr(uri, "/classes/") != NULL)
        cupsdProcessBrowseData(uri, p.type | CUPS_PRINTER_CLASS, IPP_PRINTER_IDLE,
	                  p.location, p.info, p.make_model);
    }

   /*
    * Free this listing...
    */

    free(s);
  }       

  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdUpdateSLPBrowse() End...");
}


/*
 * 'slp_attr_callback()' - SLP attribute callback 
 */

static SLPBoolean			/* O - SLP_TRUE for success */
slp_attr_callback(
    SLPHandle  hslp,			/* I - SLP handle */
    const char *attrlist,		/* I - Attribute list */
    SLPError   errcode,			/* I - Parsing status for this attr */
    void       *cookie)			/* I - Current printer */
{
  char			*tmp = 0;
  cupsd_printer_t	*p = (cupsd_printer_t*)cookie;


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

  memset(p, 0, sizeof(cupsd_printer_t));

  p->type = CUPS_PRINTER_REMOTE;

  if (slp_get_attr(attrlist, "(printer-location=", &(p->location)))
    return (SLP_FALSE);
  if (slp_get_attr(attrlist, "(printer-info=", &(p->info)))
    return (SLP_FALSE);
  if (slp_get_attr(attrlist, "(printer-make-and-model=", &(p->make_model)))
    return (SLP_FALSE);

  if (slp_get_attr(attrlist, "(color-supported=", &tmp))
    return (SLP_FALSE);
  if (strcasecmp(tmp, "true") == 0)
    p->type |= CUPS_PRINTER_COLOR;

  if (slp_get_attr(attrlist, "(finishings-supported=", &tmp))
    return (SLP_FALSE);
  if (strstr(tmp, "staple"))
    p->type |= CUPS_PRINTER_STAPLE;
  if (strstr(tmp, "bind"))
    p->type |= CUPS_PRINTER_BIND;
  if (strstr(tmp, "punch"))
    p->type |= CUPS_PRINTER_PUNCH;

  if (slp_get_attr(attrlist, "(sides-supported=", &tmp))
    return (SLP_FALSE);
  if (strstr(tmp,"two-sided"))
    p->type |= CUPS_PRINTER_DUPLEX;

  cupsdClearString(&tmp);

  return (SLP_TRUE);
}


/*
 * 'slp_dereg_printer()' - SLPDereg() the specified printer
 */

static void 
slp_dereg_printer(cupsd_printer_t *p)	/* I - Printer */
{
  char	srvurl[HTTP_MAX_URI];		/* Printer service URI */


  cupsdLogMessage(CUPSD_LOG_DEBUG, "slp_dereg_printer: printer=\"%s\"", p->name);

  if (!(p->type & CUPS_PRINTER_REMOTE))
  {
   /*
    * Make the SLP service URL that conforms to the IANA 
    * 'printer:' template.
    */

    snprintf(srvurl, sizeof(srvurl), SLP_CUPS_SRVTYPE ":%s", p->uri);

   /*
    * Deregister the printer...
    */

    SLPDereg(BrowseSLPHandle, srvurl, slp_reg_callback, 0);
  }
}


/*
 * 'slp_get_attr()' - Get an attribute from an SLP registration.
 */

static int 				/* O - 0 on success */
slp_get_attr(const char *attrlist,	/* I - Attribute list string */
             const char *tag,		/* I - Name of attribute */
             char       **valbuf)	/* O - Value */
{
  char	*ptr1,				/* Pointer into string */
	*ptr2;				/* ... */


  cupsdClearString(valbuf);

  if ((ptr1 = strstr(attrlist, tag)) != NULL)
  {
    ptr1 += strlen(tag);

    if ((ptr2 = strchr(ptr1,')')) != NULL)
    {
     /*
      * Copy the value...
      */

      *valbuf = calloc(ptr2 - ptr1 + 1, 1);
      strncpy(*valbuf, ptr1, ptr2 - ptr1);

     /*
      * Dequote the value...
      */

      for (ptr1 = *valbuf; *ptr1; ptr1 ++)
	if (*ptr1 == '\\' && ptr1[1])
	  _cups_strcpy(ptr1, ptr1 + 1);

      return (0);
    }
  }

  return (-1);
}


/*
 * 'slp_reg_callback()' - Empty SLPRegReport.
 */

static void
slp_reg_callback(SLPHandle hslp,	/* I - SLP handle */
                 SLPError  errcode,	/* I - Error code, if any */
		 void      *cookie)	/* I - App data */
{
  (void)hslp;
  (void)errcode;
  (void)cookie;

  return;
}


/*
 * 'slp_url_callback()' - SLP service url callback
 */

static SLPBoolean			/* O - TRUE = OK, FALSE = error */
slp_url_callback(
    SLPHandle      hslp,	 	/* I - SLP handle */
    const char     *srvurl, 		/* I - URL of service */
    unsigned short lifetime,		/* I - Life of service */
    SLPError       errcode, 		/* I - Existing error code */
    void           *cookie)		/* I - Pointer to service list */
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

  strlcpy(s->url, srvurl, sizeof(s->url));

 /* 
  * Link the SLP service URL into the head of the list
  */

  if (*head)
    s->next = *head;

  *head = s;

  return (SLP_TRUE);
}
#endif /* HAVE_LIBSLP */


/*
 * End of "$Id$".
 */
