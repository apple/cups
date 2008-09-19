/*
 * "$Id$"
 *
 *   Directory services routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cupsdDeregisterPrinter()   - Stop sending broadcast information for a local
 *                                printer and remove any pending references to
 *                                remote printers.
 *   cupsdLoadRemoteCache()     - Load the remote printer cache.
 *   cupsdRegisterPrinter()     - Start sending broadcast information for a
 *                                printer or update the broadcast contents.
 *   cupsdRestartPolling()      - Restart polling servers as needed.
 *   cupsdSaveRemoteCache()     - Save the remote printer cache.
 *   cupsdSendBrowseList()      - Send new browsing information as necessary.
 *   ldap_rebind_proc()         - Callback function for LDAP rebind
 *   ldap_connect()             - Start new LDAP connection
 *   ldap_reconnect()           - Reconnect to LDAP Server
 *   ldap_disconnect()          - Disconnect from LDAP Server
 *   cupsdStartBrowsing()       - Start sending and receiving broadcast
 *                                information.
 *   cupsdStartPolling()        - Start polling servers as needed.
 *   cupsdStopBrowsing()        - Stop sending and receiving broadcast
 *                                information.
 *   cupsdStopPolling()         - Stop polling servers as needed.
 *   cupsdUpdateDNSSDName()     - Update the computer name we use for
 *                                browsing...
 *   cupsdUpdateLDAPBrowse()    - Scan for new printers via LDAP...
 *   cupsdUpdateSLPBrowse()     - Get browsing information via SLP.
 *   dequote()                  - Remote quotes from a string.
 *   dnssdBuildTxtRecord()      - Build a TXT record from printer info.
 *   dnssdComparePrinters()     - Compare the registered names of two printers.
 *   dnssdDeregisterPrinter()   - Stop sending broadcast information for a
 *                                printer.
 *   dnssdPackTxtRecord()       - Pack an array of key/value pairs into the TXT
 *                                record format.
 *   dnssdRegisterCallback()    - DNSServiceRegister callback.
 *   dnssdRegisterPrinter()     - Start sending broadcast information for a
 *                                printer or update the broadcast contents.
 *   dnssdUpdate()              - Handle DNS-SD queries.
 *   get_hostconfig()           - Get an /etc/hostconfig service setting.
 *   is_local_queue()           - Determine whether the URI points at a local
 *                                queue.
 *   process_browse_data()      - Process new browse data.
 *   process_implicit_classes() - Create/update implicit classes as needed.
 *   send_cups_browse()         - Send new browsing information using the CUPS
 *                                protocol.
 *   ldap_search_rec()          - LDAP Search with reconnect
 *   ldap_freeres()             - Free LDAPMessage
 *   ldap_getval_char()         - Get first LDAP value and convert to string
 *   send_ldap_ou()             - Send LDAP ou registrations.
 *   send_ldap_browse()         - Send LDAP printer registrations.
 *   ldap_dereg_printer()       - Delete printer from directory
 *   send_slp_browse()          - Register the specified printer with SLP.
 *   slp_attr_callback()        - SLP attribute callback
 *   slp_dereg_printer()        - SLPDereg() the specified printer
 *   slp_get_attr()             - Get an attribute from an SLP registration.
 *   slp_reg_callback()         - Empty SLPRegReport.
 *   slp_url_callback()         - SLP service url callback
 *   update_cups_browse()       - Update the browse lists using the CUPS
 *                                protocol.
 *   update_lpd()               - Update the LPD configuration as needed.
 *   update_polling()           - Read status messages from the poll daemons.
 *   update_smb()               - Update the SMB configuration as needed.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <grp.h>

#ifdef HAVE_DNSSD
#  include <dns_sd.h>
#  ifdef __APPLE__
#    include <nameser.h>
#    ifdef HAVE_COREFOUNDATION
#      include <CoreFoundation/CoreFoundation.h>
#    endif /* HAVE_COREFOUNDATION */
#    ifdef HAVE_SYSTEMCONFIGURATION
#      include <SystemConfiguration/SystemConfiguration.h>
#    endif /* HAVE_SYSTEMCONFIGURATION */
#  endif /* __APPLE__ */
#endif /* HAVE_DNSSD */


/*
 * Local functions...
 */

static char	*dequote(char *d, const char *s, int dlen);
#ifdef __APPLE__
static int	get_hostconfig(const char *name);
#endif /* __APPLE__ */
static int	is_local_queue(const char *uri, char *host, int hostlen,
		               char *resource, int resourcelen);
static void	process_browse_data(const char *uri, const char *host,
		                    const char *resource, cups_ptype_t type,
				    ipp_pstate_t state, const char *location,
				    const char *info, const char *make_model,
				    int num_attrs, cups_option_t *attrs);
static void	process_implicit_classes(void);
static void	send_cups_browse(cupsd_printer_t *p);
#ifdef HAVE_LDAP
static LDAP	*ldap_connect(void);
static void	ldap_reconnect(void);
static void	ldap_disconnect(LDAP *ld);
static int	ldap_search_rec(LDAP *ld, char *base, int scope,
                                char *filter, char *attrs[],
                                int attrsonly, LDAPMessage **res);
static int	ldap_getval_firststring(LDAP *ld, LDAPMessage *entry,
                                        char *attr, char *retval,
                                        unsigned long maxsize);
static void	ldap_freeres(LDAPMessage *entry);
static void	send_ldap_ou(char *ou, char *basedn, char *descstring);
static void	send_ldap_browse(cupsd_printer_t *p);
static void	ldap_dereg_printer(cupsd_printer_t *p);
static void	ldap_dereg_ou(char *ou, char *basedn);
#  ifdef HAVE_LDAP_REBIND_PROC
#    if defined(LDAP_API_FEATURE_X_OPENLDAP) && (LDAP_API_VERSION > 2000)
static int	ldap_rebind_proc(LDAP *RebindLDAPHandle,
                                 LDAP_CONST char *refsp,
                                 ber_tag_t request,
                                 ber_int_t msgid,
                                 void *params);
#    else
static int	ldap_rebind_proc(LDAP *RebindLDAPHandle,
                                 char **dnp,
                                 char **passwdp,
                                 int *authmethodp,
                                 int freeit,
                                 void *arg);
#    endif /* defined(LDAP_API_FEATURE_X_OPENLDAP) && (LDAP_API_VERSION > 2000) */
#  endif /* HAVE_LDAP_REBIND_PROC */
#endif /* HAVE_LDAP */
#ifdef HAVE_LIBSLP
static void	send_slp_browse(cupsd_printer_t *p);
#endif /* HAVE_LIBSLP */
static void	update_cups_browse(void);
static void	update_lpd(int onoff);
static void	update_polling(void);
static void	update_smb(int onoff);


#ifdef HAVE_DNSSD
static char	*dnssdBuildTxtRecord(int *txt_len, cupsd_printer_t *p,
		                     int for_lpd);
static int	dnssdComparePrinters(cupsd_printer_t *a, cupsd_printer_t *b);
static void	dnssdDeregisterPrinter(cupsd_printer_t *p);
static char	*dnssdPackTxtRecord(int *txt_len, char *keyvalue[][2],
		                    int count);
static void	dnssdRegisterCallback(DNSServiceRef sdRef,
		                      DNSServiceFlags flags, 
				      DNSServiceErrorType errorCode,
				      const char *name, const char *regtype,
				      const char *domain, void *context);
static void	dnssdRegisterPrinter(cupsd_printer_t *p);
static void	dnssdUpdate(void);
#endif /* HAVE_DNSSD */

#ifdef HAVE_LDAP
static const char * const ldap_attrs[] =/* CUPS LDAP attributes */
		{
		  "printerDescription",
		  "printerLocation",
		  "printerMakeAndModel",
		  "printerType",
		  "printerURI",
		  NULL
		};
#endif /* HAVE_LDAP */

#ifdef HAVE_LIBSLP 
/*
 * SLP definitions...
 */

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
 * 'cupsdDeregisterPrinter()' - Stop sending broadcast information for a 
 *				local printer and remove any pending
 *                              references to remote printers.
 */

void
cupsdDeregisterPrinter(
    cupsd_printer_t *p,			/* I - Printer to register */
    int             removeit)		/* I - Printer being permanently removed */
{
 /*
  * Only deregister if browsing is enabled and it's a local printer...
  */

  if (!Browsing || !p->shared ||
      (p->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT)))
    return;

 /*
  * Announce the deletion...
  */

  if ((BrowseLocalProtocols & BROWSE_CUPS) && BrowseSocket >= 0)
  {
    cups_ptype_t savedtype = p->type;	/* Saved printer type */

    p->type |= CUPS_PRINTER_DELETE;

    send_cups_browse(p);

    p->type = savedtype;
  }

#ifdef HAVE_LIBSLP
  if (BrowseLocalProtocols & BROWSE_SLP)
    slp_dereg_printer(p);
#endif /* HAVE_LIBSLP */

#ifdef HAVE_LDAP
  if (BrowseLocalProtocols & BROWSE_LDAP)
    ldap_dereg_printer(p);
#endif /* HAVE_LDAP */

#ifdef HAVE_DNSSD
  if (removeit && (BrowseLocalProtocols & BROWSE_DNSSD) && DNSSDRef)
    dnssdDeregisterPrinter(p);
#endif /* HAVE_DNSSD */
}


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
			*valueptr,	/* Pointer into value */
			scheme[32],	/* Scheme portion of URI */
			username[64],	/* Username portion of URI */
			host[HTTP_MAX_HOST],
					/* Hostname portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port number */
  cupsd_printer_t	*p;		/* Current printer */
  time_t		now;		/* Current time */


 /*
  * Don't load the cache if the remote protocols are disabled...
  */

  if (!Browsing)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG,
                    "cupsdLoadRemoteCache: Not loading remote cache.");
    return;
  }

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

        if ((p = cupsdFindDest(value)) != NULL)
	{
	  if (p->type & CUPS_PRINTER_CLASS)
	  {
	    cupsdLogMessage(CUPSD_LOG_WARN,
	                    "Cached remote printer \"%s\" conflicts with "
			    "existing class!",
	                    value);
	    p = NULL;
	    continue;
	  }
	}
	else
          p = cupsdAddPrinter(value);

	p->accepting     = 1;
	p->state         = IPP_PRINTER_IDLE;
	p->type          |= CUPS_PRINTER_REMOTE | CUPS_PRINTER_DISCOVERED;
	p->browse_time   = now;
	p->browse_expire = now + BrowseTimeout;

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
        break;
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

        if ((p = cupsdFindDest(value)) != NULL)
	  p->type = CUPS_PRINTER_CLASS;
	else
          p = cupsdAddClass(value);

	p->accepting     = 1;
	p->state         = IPP_PRINTER_IDLE;
	p->type          |= CUPS_PRINTER_REMOTE | CUPS_PRINTER_DISCOVERED;
	p->browse_time   = now;
	p->browse_expire = now + BrowseTimeout;

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
        break;
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
        break;
      }
    }
    else if (!p)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Syntax error on line %d of remote.cache.", linenum);
      break;
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
      {
	httpSeparateURI(HTTP_URI_CODING_ALL, value, scheme, sizeof(scheme),
	                username, sizeof(username), host, sizeof(host), &port,
			resource, sizeof(resource));

	cupsdSetString(&p->hostname, host);
	cupsdSetString(&p->uri, value);
	cupsdSetDeviceURI(p, value);
      }
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of remote.cache.", linenum);
	break;
      }
    }
    else if (!strcasecmp(line, "Option") && value)
    {
     /*
      * Option name value
      */

      for (valueptr = value; *valueptr && !isspace(*valueptr & 255); valueptr ++);

      if (!*valueptr)
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of remote.cache.", linenum);
      else
      {
        for (; *valueptr && isspace(*valueptr & 255); *valueptr++ = '\0');

        p->num_options = cupsAddOption(value, valueptr, p->num_options,
	                               &(p->options));
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
	break;
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
	break;
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
	break;
      }
    }
    else if (!strcasecmp(line, "BrowseTime"))
    {
      if (value)
      {
        time_t t = atoi(value);

	if (t > p->browse_expire)
          p->browse_expire = t;
      }
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of remote.cache.", linenum);
	break;
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
            *valueptr = '\0';

	  cupsdSetString(&p->job_sheets[1], value);
	}
      }
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of remote.cache.", linenum);
	break;
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
	break;
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
	break;
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

  process_implicit_classes();
}


/*
 * 'cupsdRegisterPrinter()' - Start sending broadcast information for a
 *                            printer or update the broadcast contents.
 */

void
cupsdRegisterPrinter(cupsd_printer_t *p)/* I - Printer */
{
  if (!Browsing || !BrowseLocalProtocols ||
      (p->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT)))
    return;

#ifdef HAVE_LIBSLP
/*  if (BrowseLocalProtocols & BROWSE_SLP)
    slpRegisterPrinter(p); */
#endif /* HAVE_LIBSLP */

#ifdef HAVE_DNSSD
  if ((BrowseLocalProtocols & BROWSE_DNSSD) && DNSSDRef)
    dnssdRegisterPrinter(p);
#endif /* HAVE_DNSSD */
}


/*
 * 'cupsdRestartPolling()' - Restart polling servers as needed.
 */

void
cupsdRestartPolling(void)
{
  int			i;		/* Looping var */
  cupsd_dirsvc_poll_t	*pollp;		/* Current polling server */


  for (i = 0, pollp = Polled; i < NumPolled; i ++, pollp ++)
    if (pollp->pid)
      kill(pollp->pid, SIGHUP);
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
  cups_option_t		*option;	/* Current option */


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
    cupsdLogMessage(CUPSD_LOG_DEBUG, "Saving remote.cache...");

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

    if (!(printer->type & CUPS_PRINTER_DISCOVERED))
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

    cupsFilePrintf(fp, "BrowseTime %d\n", (int)printer->browse_expire);

    if (printer->info)
      cupsFilePrintf(fp, "Info %s\n", printer->info);

    if (printer->make_model)
      cupsFilePrintf(fp, "MakeModel %s\n", printer->make_model);

    if (printer->location)
      cupsFilePrintf(fp, "Location %s\n", printer->location);

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

    for (i = printer->num_options, option = printer->options;
         i > 0;
	 i --, option ++)
      cupsFilePrintf(fp, "Option %s %s\n", option->name, option->value);

    if (printer->type & CUPS_PRINTER_CLASS)
      cupsFilePuts(fp, "</Class>\n");
    else
      cupsFilePuts(fp, "</Printer>\n");
  }

  cupsFileClose(fp);
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


  if (!Browsing || !Printers)
    return;

 /*
  * Compute the update and timeout times...
  */

  to = time(NULL);
  ut = to - BrowseInterval;

 /*
  * Figure out how many printers need an update...
  */

  if (BrowseInterval > 0 && BrowseLocalProtocols)
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

	if ((BrowseLocalProtocols & BROWSE_CUPS) && BrowseSocket >= 0)
          send_cups_browse(p);

#ifdef HAVE_LIBSLP
	if (BrowseLocalProtocols & BROWSE_SLP)
          send_slp_browse(p);
#endif /* HAVE_LIBSLP */

#ifdef HAVE_LDAP
	if (BrowseLocalProtocols & BROWSE_LDAP)
          send_ldap_browse(p);
#endif /* HAVE_LDAP */
      }
    }

   /*
    * Save where we left off so that all printers get updated...
    */

    BrowseNext = p;
  }

 /*
  * Loop through all of the printers and timeout old printers as needed...
  */

  for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
       p;
       p = (cupsd_printer_t *)cupsArrayNext(Printers))
  {
   /*
    * If this is a remote queue, see if it needs to be timed out...
    */

    if ((p->type & CUPS_PRINTER_DISCOVERED) &&
        !(p->type & CUPS_PRINTER_IMPLICIT) &&
	p->browse_expire < to)
    {
      cupsdAddEvent(CUPSD_EVENT_PRINTER_DELETED, p, NULL,
		    "%s \'%s\' deleted by directory services (timeout).",
		    (p->type & CUPS_PRINTER_CLASS) ? "Class" : "Printer",
		    p->name);

      cupsdLogMessage(CUPSD_LOG_DEBUG,
		      "Remote destination \"%s\" has timed out; "
		      "deleting it...",
		      p->name);

      cupsArraySave(Printers);
      cupsdDeletePrinter(p, 1);
      cupsArrayRestore(Printers);
      cupsdMarkDirty(CUPSD_DIRTY_PRINTCAP | CUPSD_DIRTY_REMOTE);
    }
  }
}


#ifdef HAVE_LDAP_REBIND_PROC
#  if defined(LDAP_API_FEATURE_X_OPENLDAP) && (LDAP_API_VERSION > 2000)
/*
 * 'ldap_rebind_proc()' - Callback function for LDAP rebind
 */

static int
ldap_rebind_proc (LDAP *RebindLDAPHandle,
                  LDAP_CONST char *refsp,
                  ber_tag_t request,
                  ber_int_t msgid,
                  void *params)
{
  int               rc;

 /*
  * Bind to new LDAP server...
  */

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "ldap_rebind_proc: Rebind to %s", refsp);

#    if LDAP_API_VERSION > 3000
  struct berval bval;
  bval.bv_val = BrowseLDAPPassword;
  bval.bv_len = (BrowseLDAPPassword == NULL) ? 0 : strlen(BrowseLDAPPassword);

  rc = ldap_sasl_bind_s(RebindLDAPHandle, BrowseLDAPBindDN, LDAP_SASL_SIMPLE, &bval, NULL, NULL, NULL);
#    else
  rc = ldap_bind_s(RebindLDAPHandle, BrowseLDAPBindDN,
                   BrowseLDAPPassword, LDAP_AUTH_SIMPLE);
#    endif /* LDAP_API_VERSION > 3000 */

  return (rc);
}

#  else /* defined(LDAP_API_FEATURE_X_OPENLDAP) && (LDAP_API_VERSION > 2000) */

/*
 * 'ldap_rebind_proc()' - Callback function for LDAP rebind
 */

static int
ldap_rebind_proc (LDAP *RebindLDAPHandle,
                  char **dnp,
                  char **passwdp,
                  int *authmethodp,
                  int freeit,
                  void *arg)
{
  switch ( freeit ) {

  case 1:

     /*
      * Free current values...
      */

      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                     "ldap_rebind_proc: Free values...");

      if ( dnp && *dnp ) {
        free( *dnp );
      }
      if ( passwdp && *passwdp ) {
        free( *passwdp );
      }
      break;

  case 0:

     /*
      * Return credentials for LDAP referal...
      */

      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                     "ldap_rebind_proc: Return necessary values...");

      *dnp = strdup(BrowseLDAPBindDN);
      *passwdp = strdup(BrowseLDAPPassword);
      *authmethodp = LDAP_AUTH_SIMPLE;
      break;

  default:

     /*
      * Should never happen...
      */

      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "LDAP rebind has been called with wrong freeit value!");
      break;

  }

  return (LDAP_SUCCESS);
}
#  endif /* defined(LDAP_API_FEATURE_X_OPENLDAP) && (LDAP_API_VERSION > 2000) */
#endif /* HAVE_LDAP_REBIND_PROC */


#ifdef HAVE_LDAP
/*
 * 'ldap_connect()' - Start new LDAP connection
 */

static LDAP *
ldap_connect(void)
{
 /* 
  * Open LDAP handle...
  */

  int		rc;				/* LDAP API status */
  int		version = 3;			/* LDAP version */
  struct berval	bv = {0, ""};			/* SASL bind value */
  LDAP		*TempBrowseLDAPHandle=NULL;	/* Temporary LDAP Handle */
#  if defined(HAVE_LDAP_SSL) && defined (HAVE_MOZILLA_LDAP)
  int		ldap_ssl = 0;			/* LDAP SSL indicator */
  int		ssl_err = 0;			/* LDAP SSL error value */
#  endif /* defined(HAVE_LDAP_SSL) && defined (HAVE_MOZILLA_LDAP) */

#  ifdef HAVE_OPENLDAP
#    ifdef HAVE_LDAP_SSL

 /*
  * Set the certificate file to use for encrypted LDAP sessions...
  */

  if (BrowseLDAPCACertFile)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG,
	            "cupsdStartBrowsing: Setting CA certificate file \"%s\"",
                    BrowseLDAPCACertFile);

    if ((rc = ldap_set_option(NULL, LDAP_OPT_X_TLS_CACERTFILE,
	                      (void *)BrowseLDAPCACertFile)) != LDAP_SUCCESS)
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to set CA certificate file for LDAP "
                      "connections: %d - %s", rc, ldap_err2string(rc));
  }

#    endif /* HAVE_LDAP_SSL */
 /*
  * Initialize OPENLDAP connection...
  * LDAP stuff currently only supports ldapi EXTERNAL SASL binds...
  */

  if (!BrowseLDAPServer || !strcasecmp(BrowseLDAPServer, "localhost")) 
    rc = ldap_initialize(&TempBrowseLDAPHandle, "ldapi:///");
  else	
    rc = ldap_initialize(&TempBrowseLDAPHandle, BrowseLDAPServer);

#  else /* HAVE_OPENLDAP */

  int		ldap_port = 0;			/* LDAP port */
  char		ldap_protocol[11],		/* LDAP protocol */
		ldap_host[255];			/* LDAP host */

 /*
  * Split LDAP URI into its components...
  */

  if (! BrowseLDAPServer)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "BrowseLDAPServer not configured! Disable LDAP browsing!");
    BrowseLocalProtocols &= ~BROWSE_LDAP;
    BrowseRemoteProtocols &= ~BROWSE_LDAP;
    return (NULL);
  }

  sscanf(BrowseLDAPServer, "%10[^:]://%254[^:/]:%d", ldap_protocol, ldap_host, &ldap_port);

  if (strcmp(ldap_protocol, "ldap") == 0) {
    ldap_ssl = 0;
  } else if (strcmp(ldap_protocol, "ldaps") == 0) {
    ldap_ssl = 1;
  } else {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "unrecognised ldap protocol (%s)!", ldap_protocol);
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Disable LDAP browsing!");
    BrowseLocalProtocols &= ~BROWSE_LDAP;
    BrowseRemoteProtocols &= ~BROWSE_LDAP;
    return (NULL);
  }

  if (ldap_port == 0)
  {
    if (ldap_ssl)
      ldap_port = LDAPS_PORT;
    else
      ldap_port = LDAP_PORT;
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG,
                  "LDAP Connection Details: PROT:%s HOST:%s PORT:%d",
                  ldap_protocol, ldap_host, ldap_port);

 /*
  * Initialize LDAP connection...
  */

  if (! ldap_ssl)
  {
    if ((TempBrowseLDAPHandle = ldap_init(ldap_host, ldap_port)) == NULL)
      rc = LDAP_OPERATIONS_ERROR;
    else
      rc = LDAP_SUCCESS;

#    ifdef HAVE_LDAP_SSL
  }
  else
  {

   /*
    * Initialize SSL LDAP connection...
    */
    if (BrowseLDAPCACertFile)
    {
      rc = ldapssl_client_init(BrowseLDAPCACertFile, (void *)NULL);
      if (rc != LDAP_SUCCESS) {
        cupsdLogMessage(CUPSD_LOG_ERROR,
                        "Failed to initialize LDAP SSL client!");
        rc = LDAP_OPERATIONS_ERROR;
      } else {
        if ((TempBrowseLDAPHandle = ldapssl_init(ldap_host, ldap_port, 1)) == NULL)
          rc = LDAP_OPERATIONS_ERROR;
        else
          rc = LDAP_SUCCESS;
      }
    }
    else
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "LDAP SSL certificate file/database not configured!");
      rc = LDAP_OPERATIONS_ERROR;
    }

#    else /* HAVE_LDAP_SSL */

   /*
    * Return error, because client libraries doesn't support SSL
    */

    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "LDAP client libraries does not support TLS");
    rc = LDAP_OPERATIONS_ERROR;

#    endif /* HAVE_LDAP_SSL */
  }
#  endif /* HAVE_OPENLDAP */

 /*
  * Check return code from LDAP initialize...
  */

  if (rc != LDAP_SUCCESS)
  {
    if ((rc == LDAP_SERVER_DOWN) || (rc == LDAP_CONNECT_ERROR))
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to initialize LDAP! Temporary disable LDAP browsing...");
    }
    else
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to initialize LDAP! Disable LDAP browsing!");
      BrowseLocalProtocols &= ~BROWSE_LDAP;
      BrowseRemoteProtocols &= ~BROWSE_LDAP;
    }

    ldap_disconnect(TempBrowseLDAPHandle);
    TempBrowseLDAPHandle = NULL;
  }

 /*
  * Upgrade LDAP version...
  */

  else if (ldap_set_option(TempBrowseLDAPHandle, LDAP_OPT_PROTOCOL_VERSION,
                           (const void *)&version) != LDAP_SUCCESS)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                   "Unable to set LDAP protocol version %d! Disable LDAP browsing!",
                   version);
    BrowseLocalProtocols &= ~BROWSE_LDAP;
    BrowseRemoteProtocols &= ~BROWSE_LDAP;
    ldap_disconnect(TempBrowseLDAPHandle);
    TempBrowseLDAPHandle = NULL;
  }
  else
  {

   /*
    * Register LDAP rebind procedure...
    */

#  ifdef HAVE_LDAP_REBIND_PROC
#    if defined(LDAP_API_FEATURE_X_OPENLDAP) && (LDAP_API_VERSION > 2000)

    rc = ldap_set_rebind_proc(TempBrowseLDAPHandle, &ldap_rebind_proc, (void *)NULL);
    if ( rc != LDAP_SUCCESS )
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Setting LDAP rebind function failed with status %d: %s",
                      rc, ldap_err2string(rc));

#    else

    ldap_set_rebind_proc(TempBrowseLDAPHandle, &ldap_rebind_proc, (void *)NULL);

#    endif /* defined(LDAP_API_FEATURE_X_OPENLDAP) && (LDAP_API_VERSION > 2000) */
#  endif /* HAVE_LDAP_REBIND_PROC */

   /*
    * Start LDAP bind...
    */

#  if LDAP_API_VERSION > 3000
    struct berval bval;
    bval.bv_val = BrowseLDAPPassword;
    bval.bv_len = (BrowseLDAPPassword == NULL) ? 0 : strlen(BrowseLDAPPassword);

    if (!BrowseLDAPServer || !strcasecmp(BrowseLDAPServer, "localhost"))
      rc = ldap_sasl_bind_s(TempBrowseLDAPHandle, NULL, "EXTERNAL", &bv, NULL,
                            NULL, NULL);
    else
      rc = ldap_sasl_bind_s(TempBrowseLDAPHandle, BrowseLDAPBindDN, LDAP_SASL_SIMPLE, &bval, NULL, NULL, NULL);
#  else
      rc = ldap_bind_s(TempBrowseLDAPHandle, BrowseLDAPBindDN,
                       BrowseLDAPPassword, LDAP_AUTH_SIMPLE);
#  endif /* LDAP_API_VERSION > 3000 */

    if (rc != LDAP_SUCCESS)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                     "LDAP bind failed with error %d: %s",
                      rc, ldap_err2string(rc));
#  if defined(HAVE_LDAP_SSL) && defined (HAVE_MOZILLA_LDAP)
      if (ldap_ssl && ((rc == LDAP_SERVER_DOWN) || (rc == LDAP_CONNECT_ERROR)))
      {
        ssl_err = PORT_GetError();
        if (ssl_err != 0)
          cupsdLogMessage(CUPSD_LOG_ERROR,
                          "LDAP SSL error %d: %s",
                          ssl_err, ldapssl_err2string(ssl_err));
      }
#  endif /* defined(HAVE_LDAP_SSL) && defined (HAVE_MOZILLA_LDAP) */
      ldap_disconnect(TempBrowseLDAPHandle);
      TempBrowseLDAPHandle = NULL;
    }
    else
    {
      cupsdLogMessage(CUPSD_LOG_INFO,
                     "LDAP connection established");
    }

  }
  return (TempBrowseLDAPHandle);
}


/*
 * 'ldap_reconnect()' - Reconnect to LDAP Server
 */

static void
ldap_reconnect(void)
{
  LDAP		*TempBrowseLDAPHandle = NULL;	/* Temp Handle to LDAP server */

  cupsdLogMessage(CUPSD_LOG_INFO,
                  "Try LDAP reconnect...");

 /*
  * Get a new LDAP Handle and replace the global Handle
  * if the new connection was successful
  */

  TempBrowseLDAPHandle = ldap_connect();

  if (TempBrowseLDAPHandle != NULL)
  {
    if (BrowseLDAPHandle != NULL)
    {
      ldap_disconnect(BrowseLDAPHandle);
    }
    BrowseLDAPHandle = TempBrowseLDAPHandle;
  }
}


/*
 * 'ldap_disconnect()' - Disconnect from LDAP Server
 */

static void
ldap_disconnect(LDAP *ld)	/* I - LDAP handle */
{
  int	rc;	/* return code */

 /*
  * Close LDAP handle...
  */

#  if defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000
  rc = ldap_unbind_ext_s(ld, NULL, NULL);
#  else
  rc = ldap_unbind_s(ld);
#  endif /* defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000 */
  if (rc != LDAP_SUCCESS)
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unbind from LDAP server failed with status %d: %s",
                    rc, ldap_err2string(rc));
}
#endif /* HAVE_LDAP */


/*
 * 'cupsdStartBrowsing()' - Start sending and receiving broadcast information.
 */

void
cupsdStartBrowsing(void)
{
  int			val;		/* Socket option value */
  struct sockaddr_in	addr;		/* Broadcast address */
  cupsd_printer_t	*p;		/* Current printer */


  BrowseNext = NULL;

  if (!Browsing || !(BrowseLocalProtocols | BrowseRemoteProtocols))
    return;

  if ((BrowseLocalProtocols | BrowseRemoteProtocols) & BROWSE_CUPS)
  {
    if (BrowseSocket < 0)
    {
     /*
      * Create the broadcast socket...
      */

      if ((BrowseSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
			"Unable to create broadcast socket - %s.",
			strerror(errno));
	BrowseLocalProtocols &= ~BROWSE_CUPS;
	BrowseRemoteProtocols &= ~BROWSE_CUPS;

	if (FatalErrors & CUPSD_FATAL_BROWSE)
	  cupsdEndProcess(getpid(), 0);
      }
    }

    if (BrowseSocket >= 0)
    {
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
			"Unable to bind broadcast socket - %s.",
			strerror(errno));

#ifdef WIN32
	closesocket(BrowseSocket);
#else
	close(BrowseSocket);
#endif /* WIN32 */

	BrowseSocket = -1;
	BrowseLocalProtocols &= ~BROWSE_CUPS;
	BrowseRemoteProtocols &= ~BROWSE_CUPS;

	if (FatalErrors & CUPSD_FATAL_BROWSE)
	  cupsdEndProcess(getpid(), 0);
      }
    }

    if (BrowseSocket >= 0)
    {
     /*
      * Set the "broadcast" flag...
      */

      val = 1;
      if (setsockopt(BrowseSocket, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)))
      {
	cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to set broadcast mode - %s.",
			strerror(errno));

#ifdef WIN32
	closesocket(BrowseSocket);
#else
	close(BrowseSocket);
#endif /* WIN32 */

	BrowseSocket = -1;
	BrowseLocalProtocols &= ~BROWSE_CUPS;
	BrowseRemoteProtocols &= ~BROWSE_CUPS;

	if (FatalErrors & CUPSD_FATAL_BROWSE)
	  cupsdEndProcess(getpid(), 0);
      }
    }

    if (BrowseSocket >= 0)
    {
     /*
      * Close the socket on exec...
      */

      fcntl(BrowseSocket, F_SETFD, fcntl(BrowseSocket, F_GETFD) | FD_CLOEXEC);

     /*
      * Finally, add the socket to the input selection set as needed...
      */

      if (BrowseRemoteProtocols & BROWSE_CUPS)
      {
       /*
	* We only listen if we want remote printers...
	*/

	cupsdAddSelect(BrowseSocket, (cupsd_selfunc_t)update_cups_browse,
		       NULL, NULL);
      }
    }
  }
  else
    BrowseSocket = -1;

#ifdef HAVE_DNSSD
  if ((BrowseLocalProtocols | BrowseRemoteProtocols) & BROWSE_DNSSD)
  {
    DNSServiceErrorType error;		/* Error from service creation */
    cupsd_listener_t	*lis;		/* Current listening socket */


   /*
    * First create a "master" connection for all registrations...
    */

    if ((error = DNSServiceCreateConnection(&DNSSDRef))
	    != kDNSServiceErr_NoError)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
		      "Unable to create master DNS-SD reference: %d", error);

      if (FatalErrors & CUPSD_FATAL_BROWSE)
	cupsdEndProcess(getpid(), 0);
    }
    else
    {
     /*
      * Add the master connection to the select list...
      */

      cupsdAddSelect(DNSServiceRefSockFD(DNSSDRef),
		     (cupsd_selfunc_t)dnssdUpdate, NULL, NULL);

     /*
      * Then get the port we use for registrations.  If we are not listening
      * on any non-local ports, there is no sense sharing local printers via
      * Bonjour...
      */

      DNSSDPort = 0;

      for (lis = (cupsd_listener_t *)cupsArrayFirst(Listeners);
	   lis;
	   lis = (cupsd_listener_t *)cupsArrayNext(Listeners))
      {
	if (httpAddrLocalhost(&(lis->address)))
	  continue;

	if (lis->address.addr.sa_family == AF_INET)
	{
	  DNSSDPort = ntohs(lis->address.ipv4.sin_port);
	  break;
	}
	else if (lis->address.addr.sa_family == AF_INET6)
	{
	  DNSSDPort = ntohs(lis->address.ipv6.sin6_port);
	  break;
	}
      }

     /*
      * Create an array to track the printers we share...
      */

      if (BrowseRemoteProtocols & BROWSE_DNSSD)
        DNSSDPrinters = cupsArrayNew((cups_array_func_t)dnssdComparePrinters,
	                             NULL);

     /*
      * Set the computer name and register the web interface...
      */

      cupsdUpdateDNSSDName();
    }
  }
#endif /* HAVE_DNSSD */

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
      BrowseSLPHandle = NULL;

      if (FatalErrors & CUPSD_FATAL_BROWSE)
	cupsdEndProcess(getpid(), 0);
    }

    BrowseSLPRefresh = 0;
  }
  else
    BrowseSLPHandle = NULL;
#endif /* HAVE_LIBSLP */

#ifdef HAVE_LDAP
  if ((BrowseLocalProtocols | BrowseRemoteProtocols) & BROWSE_LDAP)
  {
    if (!BrowseLDAPDN)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Need to set BrowseLDAPDN to use LDAP browsing!");
      BrowseLocalProtocols &= ~BROWSE_LDAP;
      BrowseRemoteProtocols &= ~BROWSE_LDAP;

      if (FatalErrors & CUPSD_FATAL_BROWSE)
	cupsdEndProcess(getpid(), 0);
    }
    else
    {
     /*
      * Open LDAP handle...
      */

      if ((BrowseLDAPHandle = ldap_connect()) == NULL &&
          (FatalErrors & CUPSD_FATAL_BROWSE))
	cupsdEndProcess(getpid(), 0);
    }

    BrowseLDAPRefresh = 0;
  }
#endif /* HAVE_LDAP */

 /*
  * Enable LPD and SMB printer sharing as needed through external programs...
  */

  if (BrowseLocalProtocols & BROWSE_LPD)
    update_lpd(1);

  if (BrowseLocalProtocols & BROWSE_SMB)
    update_smb(1);

 /*
  * Register the individual printers
  */

  for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
       p;
       p = (cupsd_printer_t *)cupsArrayNext(Printers))
    if (!(p->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT)))
      cupsdRegisterPrinter(p);
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
  char			sport[255];	/* Server port */
  char			bport[255];	/* Browser port */
  char			interval[255];	/* Poll interval */
  int			statusfds[2];	/* Status pipe */
  char			*argv[6];	/* Arguments */
  char			*envp[100];	/* Environment */


 /*
  * Don't do anything if we aren't polling...
  */

  if (NumPolled == 0 || BrowseSocket < 0)
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

    if (cupsdStartProcess(polld, argv, envp, -1, -1, statusfds[1], -1, -1,
                          0, DefaultProfile, &(pollp->pid)) < 0)
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

  cupsdAddSelect(PollPipe, (cupsd_selfunc_t)update_polling, NULL, NULL);
}


/*
 * 'cupsdStopBrowsing()' - Stop sending and receiving broadcast information.
 */

void
cupsdStopBrowsing(void)
{
  cupsd_printer_t	*p;		/* Current printer */


  if (!Browsing || !(BrowseLocalProtocols | BrowseRemoteProtocols))
    return;

 /*
  * De-register the individual printers
  */

  for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
       p;
       p = (cupsd_printer_t *)cupsArrayNext(Printers))
    if (!(p->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT)))
      cupsdDeregisterPrinter(p, 1);

 /*
  * Shut down browsing sockets...
  */

  if (((BrowseLocalProtocols | BrowseRemoteProtocols) & BROWSE_CUPS) &&
      BrowseSocket >= 0)
  {
   /*
    * Close the socket and remove it from the input selection set.
    */

#ifdef WIN32
    closesocket(BrowseSocket);
#else
    close(BrowseSocket);
#endif /* WIN32 */

    cupsdRemoveSelect(BrowseSocket);
    BrowseSocket = -1;
  }

#ifdef HAVE_DNSSD
  if ((BrowseLocalProtocols & BROWSE_DNSSD) && DNSSDRef)
  {
    if (WebIFRef)
    {
      DNSServiceRefDeallocate(WebIFRef);
      WebIFRef = NULL;
    }

    if (RemoteRef)
    {
      DNSServiceRefDeallocate(RemoteRef);
      RemoteRef = NULL;
    }

    cupsdRemoveSelect(DNSServiceRefSockFD(DNSSDRef));

    DNSServiceRefDeallocate(DNSSDRef);
    DNSSDRef = NULL;

    cupsArrayDelete(DNSSDPrinters);
    DNSSDPrinters = NULL;

    DNSSDPort = 0;
  }
#endif /* HAVE_DNSSD */

#ifdef HAVE_LIBSLP
  if (((BrowseLocalProtocols | BrowseRemoteProtocols) & BROWSE_SLP) &&
      BrowseSLPHandle)
  {
   /* 
    * Close SLP handle...
    */

    SLPClose(BrowseSLPHandle);
    BrowseSLPHandle = NULL;
  }
#endif /* HAVE_LIBSLP */

#ifdef HAVE_LDAP
  if (((BrowseLocalProtocols | BrowseRemoteProtocols) & BROWSE_LDAP) &&
      BrowseLDAPHandle)
  {
    ldap_dereg_ou(ServerName, BrowseLDAPDN);
    ldap_disconnect(BrowseLDAPHandle);
    BrowseLDAPHandle = NULL;
  }
#endif /* HAVE_OPENLDAP */

 /*
  * Disable LPD and SMB printer sharing as needed through external programs...
  */

  if (BrowseLocalProtocols & BROWSE_LPD)
    update_lpd(0);

  if (BrowseLocalProtocols & BROWSE_SMB)
    update_smb(0);
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

    cupsdRemoveSelect(PollPipe);

    PollPipe         = -1;
    PollStatusBuffer = NULL;
  }

  for (i = 0, pollp = Polled; i < NumPolled; i ++, pollp ++)
    if (pollp->pid)
      cupsdEndProcess(pollp->pid, 0);
}


#ifdef HAVE_DNSSD
/*
 * 'cupsdUpdateDNSSDName()' - Update the computer name we use for browsing...
 */

void
cupsdUpdateDNSSDName(void)
{
  DNSServiceErrorType error;		/* Error from service creation */
  char		webif[1024];		/* Web interface share name */
#ifdef HAVE_COREFOUNDATION_H
  CFStringRef	nameRef;		/* Computer name CFString */
  char		nameBuffer[1024];	/* C-string buffer */
  CFStringEncoding nameEncoding;	/* Computer name encoding */
#endif	/* HAVE_COREFOUNDATION_H */


 /*
  * Only share the web interface and printers when non-local listening is
  * enabled...
  */

  if (!DNSSDPort)
    return;

 /*
  * Get the computer name as a c-string...
  */

#ifdef HAVE_COREFOUNDATION_H
  cupsdClearString(&DNSSDName);

  if ((nameRef = SCDynamicStoreCopyComputerName(NULL,
						&nameEncoding)) != NULL)
  {
    if (CFStringGetCString(nameRef, nameBuffer, sizeof(nameBuffer),
			   kCFStringEncodingUTF8))
      cupsdSetString(&DNSSDName, nameBuffer);

    CFRelease(nameRef);
  }

#else
  cupsdSetString(&DNSSDName, ServerName);
#endif	/* HAVE_COREFOUNDATION_H */

 /*
  * Then (re)register the web interface if enabled...
  */

  if (BrowseWebIF)
  {
    if (DNSSDName)
      snprintf(webif, sizeof(webif), "CUPS @ %s", DNSSDName);
    else
      strlcpy(webif, "CUPS Web Interface", sizeof(webif));

    if (WebIFRef)
      DNSServiceRefDeallocate(WebIFRef);

    WebIFRef = DNSSDRef;
    if ((error = DNSServiceRegister(&WebIFRef,
				    kDNSServiceFlagsShareConnection,
				    0, webif, "_http._tcp", NULL,
				    NULL, htons(DNSSDPort), 7,
				    "\006path=/", dnssdRegisterCallback,
				    NULL)) != kDNSServiceErr_NoError)
      cupsdLogMessage(CUPSD_LOG_ERROR,
		      "DNS-SD web interface registration failed: %d", error);
  }
}
#endif /* HAVE_DNSSD */


#ifdef HAVE_LDAP
/*
 * 'cupsdUpdateLDAPBrowse()' - Scan for new printers via LDAP...
 */

void
cupsdUpdateLDAPBrowse(void)
{
  char		uri[HTTP_MAX_URI],	/* Printer URI */
		host[HTTP_MAX_URI],	/* Hostname */
		resource[HTTP_MAX_URI],	/* Resource path */
		location[1024],		/* Printer location */
		info[1024],		/* Printer information */
		make_model[1024],	/* Printer make and model */
		type_num[30];		/* Printer type number */
  int		type;			/* Printer type */
  int		rc;			/* LDAP status */
  int		limit;			/* Size limit */
  LDAPMessage	*res,			/* LDAP search results */
		  *e;			/* Current entry from search */

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "UpdateLDAPBrowse: %s", ServerName);

  BrowseLDAPRefresh = time(NULL) + BrowseInterval;

 /*
  * Reconnect if LDAP Handle is invalid...
  */

  if (! BrowseLDAPHandle)
  {
    ldap_reconnect();
    return;
  }

 /*
  * Search for cups printers in LDAP directory...
  */

  rc = ldap_search_rec(BrowseLDAPHandle, BrowseLDAPDN, LDAP_SCOPE_SUBTREE,
                       "(objectclass=cupsPrinter)", (char **)ldap_attrs, 0, &res);

 /*
  * If ldap search was successfull then exit function
  * and temporary disable LDAP updates...
  */

  if (rc != LDAP_SUCCESS) 
  {
    if (BrowseLDAPUpdate && ((rc == LDAP_SERVER_DOWN) || (rc == LDAP_CONNECT_ERROR)))
    {
      BrowseLDAPUpdate = FALSE;
      cupsdLogMessage(CUPSD_LOG_INFO,
                      "LDAP update temporary disabled");
    }
    return;
  }

 /*
  * If LDAP updates were disabled, we will reenable them...
  */

  if (! BrowseLDAPUpdate)
  {
    BrowseLDAPUpdate = TRUE;
    cupsdLogMessage(CUPSD_LOG_INFO,
                    "LDAP update enabled");
  }

 /*
  * Count LDAP entries and return if no entry exist...
  */

  limit = ldap_count_entries(BrowseLDAPHandle, res);
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "LDAP search returned %d entries", limit);
  if (limit < 1)
  {
    ldap_freeres(res);
    return;
  }

 /*
  * Loop through the available printers...
  */

  for (e = ldap_first_entry(BrowseLDAPHandle, res);
       e;
       e = ldap_next_entry(BrowseLDAPHandle, e))
  {
   /*
    * Get the required values from this entry...
    */

    if (ldap_getval_firststring(BrowseLDAPHandle, e,
                                "printerDescription", info, sizeof(info)) == -1)
      continue;

    if (ldap_getval_firststring(BrowseLDAPHandle, e,
                                "printerLocation", location, sizeof(location)) == -1)
      continue;

    if (ldap_getval_firststring(BrowseLDAPHandle, e,
                                "printerMakeAndModel", make_model, sizeof(make_model)) == -1)
      continue;

    if (ldap_getval_firststring(BrowseLDAPHandle, e,
                                "printerType", type_num, sizeof(type_num)) == -1)
      continue;

    type = atoi(type_num);

    if (ldap_getval_firststring(BrowseLDAPHandle, e,
                                "printerURI", uri, sizeof(uri)) == -1)
      continue;

   /*
    * Process the entry as browse data...
    */

    if (!is_local_queue(uri, host, sizeof(host), resource, sizeof(resource)))
      process_browse_data(uri, host, resource, type, IPP_PRINTER_IDLE,
                          location, info, make_model, 0, NULL);

  }

  ldap_freeres(res);
}
#endif /* HAVE_LDAP */


#ifdef HAVE_LIBSLP 
/*
 * 'cupsdUpdateSLPBrowse()' - Get browsing information via SLP.
 */

void
cupsdUpdateSLPBrowse(void)
{
  slpsrvurl_t	*s,			/* Temporary list of service URLs */
		*next;			/* Next service in list */
  cupsd_printer_t p;			/* Printer information */
  const char	*uri;			/* Pointer to printer URI */
  char		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI];	/* Resource portion of URI */


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

      if (!is_local_queue(uri, host, sizeof(host), resource, sizeof(resource)))
        process_browse_data(uri, host, resource, p.type, IPP_PRINTER_IDLE,
	                    p.location,  p.info, p.make_model, 0, NULL);
    }

   /*
    * Free this listing...
    */

    cupsdClearString(&p.info);
    cupsdClearString(&p.location);
    cupsdClearString(&p.make_model);

    free(s);
  }       
}
#endif /* HAVE_LIBSLP */


/*
 * 'dequote()' - Remote quotes from a string.
 */

static char *				/* O - Dequoted string */
dequote(char       *d,			/* I - Destination string */
        const char *s,			/* I - Source string */
	int        dlen)		/* I - Destination length */
{
  char	*dptr;				/* Pointer into destination */


  if (s)
  {
    for (dptr = d, dlen --; *s && dlen > 0; s ++)
      if (*s != '\"')
      {
	*dptr++ = *s;
	dlen --;
      }

    *dptr = '\0';
  }
  else
    *d = '\0';

  return (d);
}


#ifdef HAVE_DNSSD
/*
 * 'dnssdBuildTxtRecord()' - Build a TXT record from printer info.
 */

static char *				/* O - TXT record */
dnssdBuildTxtRecord(
    int             *txt_len,		/* O - TXT record length */
    cupsd_printer_t *p,			/* I - Printer information */
    int             for_lpd)		/* I - 1 = LPD, 0 = IPP */
{
  int		i, j;			/* Looping vars */
  char		type_str[32],		/* Type to string buffer */
		state_str[32],		/* State to string buffer */
		rp_str[1024],		/* Queue name string buffer */
		air_str[1024],		/* auth-info-required string buffer */
		*keyvalue[32][2];	/* Table of key/value pairs */


 /*
  * Load up the key value pairs...
  */

  i = 0;

  keyvalue[i  ][0] = "txtvers";
  keyvalue[i++][1] = "1";

  keyvalue[i  ][0] = "qtotal";
  keyvalue[i++][1] = "1";

  keyvalue[i  ][0] = "rp";
  keyvalue[i++][1] = rp_str;
  if (for_lpd)
    strlcpy(rp_str, p->name, sizeof(rp_str));
  else
    snprintf(rp_str, sizeof(rp_str), "%s/%s", 
	     (p->type & CUPS_PRINTER_CLASS) ? "classes" : "printers", p->name);

  keyvalue[i  ][0] = "ty";
  keyvalue[i++][1] = p->make_model;

  if (p->location && *p->location != '\0')
  {
    keyvalue[i  ][0] = "note";
    keyvalue[i++][1] = p->location;
  }

  keyvalue[i  ][0] = "priority";
  keyvalue[i++][1] = for_lpd ? "100" : "0";

  keyvalue[i  ][0] = "product";
  keyvalue[i++][1] = p->product ? p->product : "Unknown";

  snprintf(type_str, sizeof(type_str), "0x%X", p->type | CUPS_PRINTER_REMOTE);
  snprintf(state_str, sizeof(state_str), "%d", p->state);

  keyvalue[i  ][0] = "printer-state";
  keyvalue[i++][1] = state_str;

  keyvalue[i  ][0] = "printer-type";
  keyvalue[i++][1] = type_str;

  keyvalue[i  ][0] = "Transparent";
  keyvalue[i++][1] = "T";

  keyvalue[i  ][0] = "Binary";
  keyvalue[i++][1] = "T";

  if ((p->type & CUPS_PRINTER_FAX))
  {
    keyvalue[i  ][0] = "Fax";
    keyvalue[i++][1] = "T";
  }

  if ((p->type & CUPS_PRINTER_COLOR))
  {
    keyvalue[i  ][0] = "Color";
    keyvalue[i++][1] = "T";
  }

  if ((p->type & CUPS_PRINTER_DUPLEX))
  {
    keyvalue[i  ][0] = "Duplex";
    keyvalue[i++][1] = "T";
  }

  if ((p->type & CUPS_PRINTER_STAPLE))
  {
    keyvalue[i  ][0] = "Staple";
    keyvalue[i++][1] = "T";
  }

  if ((p->type & CUPS_PRINTER_COPIES))
  {
    keyvalue[i  ][0] = "Copies";
    keyvalue[i++][1] = "T";
  }

  if ((p->type & CUPS_PRINTER_COLLATE))
  {
    keyvalue[i  ][0] = "Collate";
    keyvalue[i++][1] = "T";
  }

  if ((p->type & CUPS_PRINTER_PUNCH))
  {
    keyvalue[i  ][0] = "Punch";
    keyvalue[i++][1] = "T";
  }

  if ((p->type & CUPS_PRINTER_BIND))
  {
    keyvalue[i  ][0] = "Bind";
    keyvalue[i++][1] = "T";
  }

  if ((p->type & CUPS_PRINTER_SORT))
  {
    keyvalue[i  ][0] = "Sort";
    keyvalue[i++][1] = "T";
  }

  keyvalue[i  ][0] = "pdl";
  keyvalue[i++][1] = p->pdl ? p->pdl : "application/postscript";

  if (p->num_auth_info_required)
  {
    char	*air = air_str;		/* Pointer into string */


    for (j = 0; j < p->num_auth_info_required; j ++)
    {
      if (air >= (air_str + sizeof(air_str) - 2))
        break;

      if (j)
        *air++ = ',';

      strlcpy(air, p->auth_info_required[j], sizeof(air_str) - (air - air_str));
      air += strlen(air);
    }

    keyvalue[i  ][0] = "air";
    keyvalue[i++][1] = air_str;
  }

 /*
  * Then pack them into a proper txt record...
  */

  return (dnssdPackTxtRecord(txt_len, keyvalue, i));
}


/*
 * 'dnssdComparePrinters()' - Compare the registered names of two printers.
 */

static int				/* O - Result of comparison */
dnssdComparePrinters(cupsd_printer_t *a,/* I - First printer */
                     cupsd_printer_t *b)/* I - Second printer */
{
  return (strcasecmp(a->reg_name, b->reg_name));
}


/*
 * 'dnssdDeregisterPrinter()' - Stop sending broadcast information for a
 *                              printer.
 */

static void
dnssdDeregisterPrinter(
    cupsd_printer_t *p)			/* I - Printer */
{
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "dnssdDeregisterPrinter(%s)", p->name);

 /*
  * Closing the socket deregisters the service
  */

  if (p->ipp_ref)
  {
    DNSServiceRefDeallocate(p->ipp_ref);
    p->ipp_ref = NULL;
  }

  cupsArrayRemove(DNSSDPrinters, p);
  cupsdClearString(&p->reg_name);

  if (p->ipp_txt)
  {
   /*
    * p->ipp_txt is malloc'd, not _cupsStrAlloc'd...
    */

    free(p->ipp_txt);
    p->ipp_txt = NULL;
  }
}


/*
 * 'dnssdPackTxtRecord()' - Pack an array of key/value pairs into the
 *                          TXT record format.
 */

static char *				/* O - TXT record */
dnssdPackTxtRecord(int  *txt_len,	/* O - TXT record length */
		   char *keyvalue[][2],	/* I - Table of key value pairs */
		   int  count)		/* I - Items in table */
{
  int  i;				/* Looping var */
  int  length;				/* Length of TXT record */
  int  length2;				/* Length of value */
  char *txtRecord;			/* TXT record buffer */
  char *cursor;				/* Looping pointer */


 /*
  * Calculate the buffer size
  */

  for (length = i = 0; i < count; i++)
    length += 1 + strlen(keyvalue[i][0]) + 
	      (keyvalue[i][1] ? 1 + strlen(keyvalue[i][1]) : 0);

 /*
  * Allocate and fill it
  */

  txtRecord = malloc(length);
  if (txtRecord)
  {
    *txt_len = length;

    for (cursor = txtRecord, i = 0; i < count; i++)
    {
     /*
      * Drop in the p-string style length byte followed by the data
      */

      length  = strlen(keyvalue[i][0]);
      length2 = keyvalue[i][1] ? 1 + strlen(keyvalue[i][1]) : 0;

      *cursor++ = (unsigned char)(length + length2);

      memcpy(cursor, keyvalue[i][0], length);
      cursor += length;

      if (length2)
      {
        length2 --;
	*cursor++ = '=';
	memcpy(cursor, keyvalue[i][1], length2);
	cursor += length2;
      }
    }
  }

  return (txtRecord);
}


/*
 * 'dnssdRegisterCallback()' - DNSServiceRegister callback.
 */

static void
dnssdRegisterCallback(
    DNSServiceRef	sdRef,		/* I - DNS Service reference */
    DNSServiceFlags	flags,		/* I - Reserved for future use */
    DNSServiceErrorType	errorCode,	/* I - Error code */
    const char		*name,     	/* I - Service name */
    const char		*regtype,  	/* I - Service type */
    const char		*domain,   	/* I - Domain. ".local" for now */
    void		*context)	/* I - User-defined context */
{
  cupsd_printer_t *p = (cupsd_printer_t *)context;
					/* Current printer */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "dnssdRegisterCallback(%s, %s) for %s",
                  name, regtype, p ? p->name : "Web Interface");

  if (errorCode)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, 
		    "DNSServiceRegister failed with error %d", (int)errorCode);
    return;
  }
  else if (p && strcasecmp(name, p->reg_name))
  {
    cupsdLogMessage(CUPSD_LOG_INFO, "Using service name \"%s\" for \"%s\"",
                    name, p->name);

    cupsArrayRemove(DNSSDPrinters, p);
    cupsdSetString(&p->reg_name, name);
    cupsArrayAdd(DNSSDPrinters, p);

    LastEvent |= CUPSD_EVENT_PRINTER_MODIFIED;
  }
}


/*
 * 'dnssdRegisterPrinter()' - Start sending broadcast information for a printer
 *		              or update the broadcast contents.
 */

static void 
dnssdRegisterPrinter(cupsd_printer_t *p)/* I - Printer */
{
  DNSServiceErrorType	se;		/* dnssd errors */
  char			*ipp_txt,	/* IPP TXT record buffer */
			*printer_txt,	/* LPD TXT record buffer */
			name[1024],	/* Service name */
			*nameptr;	/* Pointer into name */
  int			ipp_len,	/* IPP TXT record length */
			printer_len;	/* LPD TXT record length */
  char			resource[1024];	/* Resource path for printer */
  const char		*regtype;	/* Registration type */
  const char		*domain;   	/* Registration domain */
  cupsd_location_t	*location,	/* Printer location */
			*policy;	/* Operation policy for Print-Job */
  unsigned		address[4];	/* INADDR_ANY address */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "dnssdRegisterPrinter(%s) %s", p->name,
                  !p->ipp_ref ? "new" : "update");

 /*
  * If per-printer sharing was just disabled make sure we're not
  * registered before returning.
  */

  if (!p->shared)
  {
    dnssdDeregisterPrinter(p);
    return;
  }

 /*
  * The registered name takes the form of "<printer-info> @ <computer name>"...
  */

  if (p->info && strlen(p->info) > 0)
  {
    if (DNSSDName)
      snprintf(name, sizeof(name), "%s @ %s", p->info, DNSSDName);
    else
      strlcpy(name, p->info, sizeof(name));
  }
  else if (DNSSDName)
    snprintf(name, sizeof(name), "%s @ %s", p->name, DNSSDName);
  else
    strlcpy(name, p->name, sizeof(name));

 /*
  * If an existing printer was renamed, unregister it and start over...
  */

  if (p->reg_name && strcmp(p->reg_name, name))
    dnssdDeregisterPrinter(p);

  if (!p->reg_name)
  {
    cupsdSetString(&p->reg_name, name);
    cupsArrayAdd(DNSSDPrinters, p);
  }

 /*
  * If 'Allow printing from the Internet' is enabled (i.e. from any address)
  * let dnssd decide on the domain, otherwise restrict it to ".local".
  */

  if (p->type & CUPS_PRINTER_CLASS)
    snprintf(resource, sizeof(resource), "/classes/%s", p->name);
  else
    snprintf(resource, sizeof(resource), "/printers/%s", p->name);

  address[0] = address[1] = address[2] = address[3] = 0;
  location   = cupsdFindBest(resource, HTTP_POST);
  policy     = cupsdFindPolicyOp(p->op_policy_ptr, IPP_PRINT_JOB);

  if ((location && !cupsdCheckAccess(address, "", 0, location)) ||
      (policy && !cupsdCheckAccess(address, "", 0, policy)))
    domain = "local.";
  else
    domain = NULL;

 /*
  * Register IPP and (optionally) LPD...
  */

  ipp_len = 0;				/* anti-compiler-warning-code */
  ipp_txt = dnssdBuildTxtRecord(&ipp_len, p, 0);

  if (!p->ipp_ref)
  {
   /*
    * Initial registration.  Use the _fax subtype for fax queues...
    */

    regtype = (p->type & CUPS_PRINTER_FAX) ? "_fax-ipp._tcp" :
                                             "_ipp._tcp,_cups";

    cupsdLogMessage(CUPSD_LOG_DEBUG, 
		    "Registering DNS-SD printer %s with name \"%s\", "
		    "type \"%s\", and domain \"%s\"", p->name, name, regtype,
		    domain ? domain : "(null)");

   /*
    * Register the queue, dropping characters as needed until we succeed...
    */

    nameptr = name + strlen(name);

    do
    {
      p->ipp_ref = DNSSDRef;
      if ((se = DNSServiceRegister(&p->ipp_ref, kDNSServiceFlagsShareConnection,
                                   0, name, regtype, domain, NULL,
				   htons(DNSSDPort), ipp_len, ipp_txt,
				   dnssdRegisterCallback,
				   p)) == kDNSServiceErr_BadParam)
      {
       /*
        * Name is too long, drop trailing characters, taking into account
	* UTF-8 encoding...
	*/

        nameptr --;

        while (nameptr > name && (*nameptr & 0xc0) == 0x80)
	  nameptr --;

        if (nameptr > name)
          *nameptr = '\0';
      }
    }
    while (se == kDNSServiceErr_BadParam && nameptr > name);

    if (se == kDNSServiceErr_NoError)
    {
      p->ipp_txt = ipp_txt;
      p->ipp_len = ipp_len;
      ipp_txt    = NULL;
    }
    else
      cupsdLogMessage(CUPSD_LOG_WARN,
                      "DNS-SD IPP registration of \"%s\" failed: %d",
		      p->name, se);
  }
  else if (ipp_len != p->ipp_len || memcmp(ipp_txt, p->ipp_txt, ipp_len))
  {
   /*
    * Update the existing registration...
    */

    /* A TTL of 0 means use record's original value (Radar 3176248) */
    DNSServiceUpdateRecord(p->ipp_ref, NULL, 0, ipp_len, ipp_txt, 0);

    if (p->ipp_txt)
      free(p->ipp_txt);

    p->ipp_txt = ipp_txt;
    p->ipp_len = ipp_len;
    ipp_txt    = NULL;
  }

  if (ipp_txt)
    free(ipp_txt);

  if (BrowseLocalProtocols & BROWSE_LPD)
  {
    printer_len = 0;			/* anti-compiler-warning-code */
    printer_txt = dnssdBuildTxtRecord(&printer_len, p, 1);

    if (!p->printer_ref)
    {
     /*
      * Initial registration...
      */

      cupsdLogMessage(CUPSD_LOG_DEBUG, 
		      "Registering DNS-SD printer %s with name \"%s\", "
		      "type \"_printer._tcp\", and domain \"%s\"", p->name,
		      name, domain ? domain : "(null)");

      p->printer_ref = DNSSDRef;
      if ((se = DNSServiceRegister(&p->printer_ref,
                                   kDNSServiceFlagsShareConnection,
				   0, name, "_printer._tcp", domain, NULL,
				   htons(515), printer_len, printer_txt,
				   dnssdRegisterCallback,
				   p)) == kDNSServiceErr_NoError)
      {
	p->printer_txt = printer_txt;
	p->printer_len = printer_len;
	printer_txt    = NULL;
      }
      else
	cupsdLogMessage(CUPSD_LOG_WARN,
	                "DNS-SD LPD registration of \"%s\" failed: %d",
			p->name, se);
    }
    else if (printer_len != p->printer_len ||
             memcmp(printer_txt, p->printer_txt, printer_len))
    {
     /*
      * Update the existing registration...
      */

      /* A TTL of 0 means use record's original value (Radar 3176248) */
      DNSServiceUpdateRecord(p->printer_ref, NULL, 0, printer_len,
                             printer_txt, 0);

      if (p->printer_txt)
	free(p->printer_txt);

      p->printer_txt = printer_txt;
      p->printer_len = printer_len;
      printer_txt    = NULL;
    }

    if (printer_txt)
      free(printer_txt);
  }
}


/*
 * 'dnssdUpdate()' - Handle DNS-SD queries.
 */

static void
dnssdUpdate(void)
{
  DNSServiceErrorType	sdErr;		/* Service discovery error */


  if ((sdErr = DNSServiceProcessResult(DNSSDRef)) != kDNSServiceErr_NoError)
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "DNS Service Discovery registration error %d!",
	            sdErr);
}
#endif /* HAVE_DNSSD */


#ifdef __APPLE__
/*
 * 'get_hostconfig()' - Get an /etc/hostconfig service setting.
 */

static int				/* O - 1 for YES or AUTOMATIC, 0 for NO */
get_hostconfig(const char *name)	/* I - Name of service */
{
  cups_file_t	*fp;			/* Hostconfig file */
  char		line[1024],		/* Line from file */
		*ptr;			/* Pointer to value */
  int		state = 1;		/* State of service */


 /*
  * Try opening the /etc/hostconfig file; if we can't open it, assume that
  * the service is enabled/auto.
  */

  if ((fp = cupsFileOpen("/etc/hostconfig", "r")) != NULL)
  {
   /*
    * Read lines from the file until we find the service...
    */

    while (cupsFileGets(fp, line, sizeof(line)))
    {
      if (line[0] == '#' || (ptr = strchr(line, '=')) == NULL)
        continue;

      *ptr++ = '\0';

      if (!strcasecmp(line, name))
      {
       /*
        * Found the service, see if it is set to "-NO-"...
	*/

	if (!strncasecmp(ptr, "-NO-", 4))
	  state = 0;
        break;
      }
    }

    cupsFileClose(fp);
  }

  return (state);
}
#endif /* __APPLE__ */


/*
 * 'is_local_queue()' - Determine whether the URI points at a local queue.
 */

static int				/* O - 1 = local, 0 = remote, -1 = bad URI */
is_local_queue(const char *uri,		/* I - Printer URI */
               char       *host,	/* O - Host string */
	       int        hostlen,	/* I - Length of host buffer */
               char       *resource,	/* O - Resource string */
	       int        resourcelen)	/* I - Length of resource buffer */
{
  char		scheme[32],		/* Scheme portion of URI */
		username[HTTP_MAX_URI];	/* Username portion of URI */
  int		port;			/* Port portion of URI */
  cupsd_netif_t	*iface;			/* Network interface */


 /*
  * Pull the URI apart to see if this is a local or remote printer...
  */

  if (httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, sizeof(scheme),
                      username, sizeof(username), host, hostlen, &port,
		      resource, resourcelen) < HTTP_URI_OK)
    return (-1);

  DEBUG_printf(("host=\"%s\", ServerName=\"%s\"\n", host, ServerName));

 /*
  * Check for local server addresses...
  */

  if (!strcasecmp(host, ServerName) && port == LocalPort)
    return (1);

  cupsdNetIFUpdate();

  for (iface = (cupsd_netif_t *)cupsArrayFirst(NetIFList);
       iface;
       iface = (cupsd_netif_t *)cupsArrayNext(NetIFList))
    if (!strcasecmp(host, iface->hostname) && port == iface->port)
      return (1);

 /*
  * If we get here, the printer is remote...
  */

  return (0);
}


/*
 * 'process_browse_data()' - Process new browse data.
 */

static void
process_browse_data(
    const char    *uri,			/* I - URI of printer/class */
    const char    *host,		/* I - Hostname */
    const char    *resource,		/* I - Resource path */
    cups_ptype_t  type,			/* I - Printer type */
    ipp_pstate_t  state,		/* I - Printer state */
    const char    *location,		/* I - Printer location */
    const char    *info,		/* I - Printer information */
    const char    *make_model,		/* I - Printer make and model */
    int		  num_attrs,		/* I - Number of attributes */
    cups_option_t *attrs)		/* I - Attributes */
{
  int		i;			/* Looping var */
  int		update;			/* Update printer attributes? */
  char		finaluri[HTTP_MAX_URI],	/* Final URI for printer */
		name[IPP_MAX_NAME],	/* Name of printer */
		newname[IPP_MAX_NAME],	/* New name of printer */
		*hptr,			/* Pointer into hostname */
		*sptr;			/* Pointer into ServerName */
  const char	*shortname;		/* Short queue name (queue) */
  char		local_make_model[IPP_MAX_NAME];
					/* Local make and model */
  cupsd_printer_t *p;			/* Printer information */
  const char	*ipp_options,		/* ipp-options value */
		*lease_duration;	/* lease-duration value */
  int		is_class;		/* Is this queue a class? */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "process_browse_data(uri=\"%s\", host=\"%s\", "
		  "resource=\"%s\", type=%x, state=%d, location=\"%s\", "
		  "info=\"%s\", make_model=\"%s\", num_attrs=%d, attrs=%p)",
		  uri, host, resource, type, state,
		  location ? location : "(nil)", info ? info : "(nil)",
		  make_model ? make_model : "(nil)", num_attrs, attrs);

 /*
  * Determine if the URI contains any illegal characters in it...
  */

  if (strncmp(uri, "ipp://", 6) || !host[0] ||
      (strncmp(resource, "/printers/", 10) &&
       strncmp(resource, "/classes/", 9)))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Bad printer URI in browse data: %s", uri);
    return;
  }

  if (strchr(resource, '?') ||
      (!strncmp(resource, "/printers/", 10) && strchr(resource + 10, '/')) ||
      (!strncmp(resource, "/classes/", 9) && strchr(resource + 9, '/')))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Bad resource in browse data: %s",
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

  type     |= CUPS_PRINTER_REMOTE | CUPS_PRINTER_DISCOVERED;
  type     &= ~CUPS_PRINTER_IMPLICIT;
  update   = 0;
  hptr     = strchr(host, '.');
  sptr     = strchr(ServerName, '.');
  is_class = type & CUPS_PRINTER_CLASS;

  if (!ServerNameIsIP && sptr != NULL && hptr != NULL)
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

  if (is_class)
  {
   /*
    * Remote destination is a class...
    */

    if (!strncmp(resource, "/classes/", 9))
      snprintf(name, sizeof(name), "%s@%s", resource + 9, host);
    else
      return;

    shortname = resource + 9;
  }
  else
  {
   /*
    * Remote destination is a printer...
    */

    if (!strncmp(resource, "/printers/", 10))
      snprintf(name, sizeof(name), "%s@%s", resource + 10, host);
    else
      return;

    shortname = resource + 10;
  }

  if (hptr && !*hptr)
    *hptr = '.';			/* Resource FQDN */

  if ((p = cupsdFindDest(name)) == NULL && BrowseShortNames)
  {
   /*
    * Long name doesn't exist, try short name...
    */

    cupsdLogMessage(CUPSD_LOG_DEBUG, "process_browse_data: %s not found...",
                    name);

    if ((p = cupsdFindDest(shortname)) == NULL)
    {
     /*
      * Short name doesn't exist, use it for this shared queue.
      */

      cupsdLogMessage(CUPSD_LOG_DEBUG2, "process_browse_data: %s not found...",
		      shortname);
      strlcpy(name, shortname, sizeof(name));
    }
    else
    {
     /*
      * Short name exists...
      */

      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "process_browse_data: %s found, type=%x, hostname=%s...",
		      shortname, p->type, p->hostname ? p->hostname : "(nil)");

      if (p->type & CUPS_PRINTER_IMPLICIT)
        p = NULL;			/* Don't replace implicit classes */
      else if (p->hostname && strcasecmp(p->hostname, host))
      {
       /*
	* Short name exists but is for a different host.  If this is a remote
	* queue, rename it and use the long name...
	*/

	if (p->type & CUPS_PRINTER_REMOTE)
	{
	  cupsdLogMessage(CUPSD_LOG_DEBUG,
			  "Renamed remote %s \"%s\" to \"%s@%s\"...",
			  is_class ? "class" : "printer", p->name, p->name,
			  p->hostname);
	  cupsdAddEvent(CUPSD_EVENT_PRINTER_DELETED, p, NULL,
			"%s \'%s\' deleted by directory services.",
			is_class ? "Class" : "Printer", p->name);

	  snprintf(newname, sizeof(newname), "%s@%s", p->name, p->hostname);
	  cupsdRenamePrinter(p, newname);

	  cupsdAddEvent(CUPSD_EVENT_PRINTER_ADDED, p, NULL,
			"%s \'%s\' added by directory services.",
			is_class ? "Class" : "Printer", p->name);
	}

       /*
        * Force creation with long name...
	*/

	p = NULL;
      }
    }
  }
  else if (p)
    cupsdLogMessage(CUPSD_LOG_DEBUG2,
		    "process_browse_data: %s found, type=%x, hostname=%s...",
		    name, p->type, p->hostname ? p->hostname : "(nil)");

  if (!p)
  {
   /*
    * Queue doesn't exist; add it...
    */

    if (is_class)
      p = cupsdAddClass(name);
    else
      p = cupsdAddPrinter(name);

    cupsdClearString(&(p->hostname));

    cupsdLogMessage(CUPSD_LOG_DEBUG, "Added remote %s \"%s\"...",
                    is_class ? "class" : "printer", name);

    cupsdAddEvent(CUPSD_EVENT_PRINTER_ADDED, p, NULL,
		  "%s \'%s\' added by directory services.",
		  is_class ? "Class" : "Printer", name);

   /*
    * Force the URI to point to the real server...
    */

    p->type      = type & ~CUPS_PRINTER_REJECTING;
    p->accepting = 1;

    cupsdMarkDirty(CUPSD_DIRTY_PRINTCAP);
  }

  if (!p)
    return;

  if (!p->hostname)
  {
   /*
    * Hostname not set, so this must be a cached remote printer
    * that was created for a pending print job...
    */

    cupsdSetString(&p->hostname, host);
    cupsdSetString(&p->uri, uri);
    cupsdSetString(&p->device_uri, uri);
    update = 1;

    cupsdMarkDirty(CUPSD_DIRTY_REMOTE);
  }

 /*
  * Update the state...
  */

  p->state       = state;
  p->browse_time = time(NULL);

  if ((lease_duration = cupsGetOption("lease-duration", num_attrs,
                                      attrs)) != NULL)
  {
   /*
    * Grab the lease-duration for the browse data; anything less then 1
    * second or more than 1 week gets the default BrowseTimeout...
    */

    i = atoi(lease_duration);
    if (i < 1 || i > 604800)
      i = BrowseTimeout;

    p->browse_expire = p->browse_time + i;
  }
  else
    p->browse_expire = p->browse_time + BrowseTimeout;

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

    cupsdMarkDirty(CUPSD_DIRTY_PRINTCAP | CUPSD_DIRTY_REMOTE);
  }

  if (!make_model || !make_model[0])
  {
    if (is_class)
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

  if (p->num_options)
  {
    if (!update && !(type & CUPS_PRINTER_DELETE))
    {
     /*
      * See if we need to update the attributes...
      */

      if (p->num_options != num_attrs)
	update = 1;
      else
      {
	for (i = 0; i < num_attrs; i ++)
          if (strcmp(attrs[i].name, p->options[i].name) ||
	      (!attrs[i].value != !p->options[i].value) ||
	      (attrs[i].value && strcmp(attrs[i].value, p->options[i].value)))
          {
	    update = 1;
	    break;
          }
      }
    }

   /*
    * Free the old options...
    */

    cupsFreeOptions(p->num_options, p->options);
  }

  p->num_options = num_attrs;
  p->options     = attrs;

  if (type & CUPS_PRINTER_DELETE)
  {
    cupsdAddEvent(CUPSD_EVENT_PRINTER_DELETED, p, NULL,
                  "%s \'%s\' deleted by directory services.",
		  is_class ? "Class" : "Printer", p->name);

    cupsdExpireSubscriptions(p, NULL);
 
    cupsdDeletePrinter(p, 1);
    cupsdUpdateImplicitClasses();
    cupsdMarkDirty(CUPSD_DIRTY_PRINTCAP | CUPSD_DIRTY_REMOTE);
  }
  else if (update)
  {
    cupsdSetPrinterAttrs(p);
    cupsdUpdateImplicitClasses();
  }

 /*
  * See if we have a default printer...  If not, make the first network
  * default printer the default.
  */

  if (DefaultPrinter == NULL && Printers != NULL && UseNetworkDefault)
  {
   /*
    * Find the first network default printer and use it...
    */

    for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
         p;
	 p = (cupsd_printer_t *)cupsArrayNext(Printers))
      if (p->type & CUPS_PRINTER_DEFAULT)
      {
        DefaultPrinter = p;
        cupsdMarkDirty(CUPSD_DIRTY_PRINTCAP | CUPSD_DIRTY_REMOTE);
	break;
      }
  }

 /*
  * Do auto-classing if needed...
  */

  process_implicit_classes();
}


/*
 * 'process_implicit_classes()' - Create/update implicit classes as needed.
 */

static void
process_implicit_classes(void)
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
	!strncasecmp(p->name, name + offset, len) &&
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

        cupsdSetString(&pclass->job_sheets[0], p->job_sheets[0]);
        cupsdSetString(&pclass->job_sheets[1], p->job_sheets[1]);

        update = 1;

	cupsdMarkDirty(CUPSD_DIRTY_PRINTCAP | CUPSD_DIRTY_REMOTE);

        cupsdLogMessage(CUPSD_LOG_DEBUG, "Added implicit class \"%s\"...",
	                name);
	cupsdAddEvent(CUPSD_EVENT_PRINTER_ADDED, p, NULL,
                      "Implicit class \'%s\' added by directory services.",
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
 * 'send_cups_browse()' - Send new browsing information using the CUPS
 *                        protocol.
 */

static void
send_cups_browse(cupsd_printer_t *p)	/* I - Printer to send */
{
  int			i;		/* Looping var */
  cups_ptype_t		type;		/* Printer type */
  cupsd_dirsvc_addr_t	*b;		/* Browse address */
  int			bytes;		/* Length of packet */
  char			packet[1453],	/* Browse data packet */
			uri[1024],	/* Printer URI */
			location[1024],	/* printer-location */
			info[1024],	/* printer-info */
			make_model[1024];
					/* printer-make-and-model */
  cupsd_netif_t		*iface;		/* Network interface */


 /*
  * Figure out the printer type value...
  */

  type = p->type | CUPS_PRINTER_REMOTE;

  if (!p->accepting)
    type |= CUPS_PRINTER_REJECTING;

  if (p == DefaultPrinter)
    type |= CUPS_PRINTER_DEFAULT;

 /*
  * Remove quotes from printer-info, printer-location, and
  * printer-make-and-model attributes...
  */

  dequote(location, p->location, sizeof(location));
  dequote(info, p->info, sizeof(info));

  if (p->make_model)
    dequote(make_model, p->make_model, sizeof(make_model));
  else if (p->type & CUPS_PRINTER_CLASS)
  {
    if (p->num_printers > 0 && p->printers[0]->make_model)
      strlcpy(make_model, p->printers[0]->make_model, sizeof(make_model));
    else
      strlcpy(make_model, "Local Printer Class", sizeof(make_model));
  }
  else if (p->raw)
    strlcpy(make_model, "Local Raw Printer", sizeof(make_model));
  else
    strlcpy(make_model, "Local System V Printer", sizeof(make_model));

 /*
  * Send a packet to each browse address...
  */

  for (i = NumBrowsers, b = Browsers; i > 0; i --, b ++)
    if (b->iface[0])
    {
     /*
      * Send the browse packet to one or more interfaces...
      */

      if (!strcmp(b->iface, "*"))
      {
       /*
        * Send to all local interfaces...
	*/

        cupsdNetIFUpdate();

	for (iface = (cupsd_netif_t *)cupsArrayFirst(NetIFList);
	     iface;
	     iface = (cupsd_netif_t *)cupsArrayNext(NetIFList))
	{
	 /*
	  * Only send to local, IPv4 interfaces...
	  */

	  if (!iface->is_local || !iface->port ||
	      iface->address.addr.sa_family != AF_INET)
	    continue;

	  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
	                   iface->hostname, iface->port,
			   (p->type & CUPS_PRINTER_CLASS) ? "/classes/%s" :
			                                    "/printers/%s",
			   p->name);
	  snprintf(packet, sizeof(packet), "%x %x %s \"%s\" \"%s\" \"%s\" %s\n",
        	   type, p->state, uri, location, info, make_model,
		   p->browse_attrs ? p->browse_attrs : "");

	  bytes = strlen(packet);

	  cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                  "cupsdSendBrowseList: (%d bytes to \"%s\") %s", bytes,
        	          iface->name, packet);

          iface->broadcast.ipv4.sin_port = htons(BrowsePort);

	  sendto(BrowseSocket, packet, bytes, 0,
		 (struct sockaddr *)&(iface->broadcast),
		 httpAddrLength(&(iface->broadcast)));
        }
      }
      else if ((iface = cupsdNetIFFind(b->iface)) != NULL)
      {
       /*
        * Send to the named interface using the IPv4 address...
	*/

        while (iface)
	  if (strcmp(b->iface, iface->name))
	  {
	    iface = NULL;
	    break;
	  }
	  else if (iface->address.addr.sa_family == AF_INET && iface->port)
	    break;
	  else
            iface = (cupsd_netif_t *)cupsArrayNext(NetIFList);

        if (iface)
	{
	  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
	                   iface->hostname, iface->port,
			   (p->type & CUPS_PRINTER_CLASS) ? "/classes/%s" :
			                                    "/printers/%s",
			   p->name);
	  snprintf(packet, sizeof(packet), "%x %x %s \"%s\" \"%s\" \"%s\" %s\n",
        	   type, p->state, uri, location, info, make_model,
		   p->browse_attrs ? p->browse_attrs : "");

	  bytes = strlen(packet);

	  cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                  "cupsdSendBrowseList: (%d bytes to \"%s\") %s", bytes,
        	          iface->name, packet);

          iface->broadcast.ipv4.sin_port = htons(BrowsePort);

	  sendto(BrowseSocket, packet, bytes, 0,
		 (struct sockaddr *)&(iface->broadcast),
		 httpAddrLength(&(iface->broadcast)));
        }
      }
    }
    else
    {
     /*
      * Send the browse packet to the indicated address using
      * the default server name...
      */

      snprintf(packet, sizeof(packet), "%x %x %s \"%s\" \"%s\" \"%s\" %s\n",
       	       type, p->state, p->uri, location, info, make_model,
	       p->browse_attrs ? p->browse_attrs : "");

      bytes = strlen(packet);
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdSendBrowseList: (%d bytes) %s", bytes, packet);

      if (sendto(BrowseSocket, packet, bytes, 0,
		 (struct sockaddr *)&(b->to),
		 httpAddrLength(&(b->to))) <= 0)
      {
       /*
        * Unable to send browse packet, so remove this address from the
	* list...
	*/

	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "cupsdSendBrowseList: sendto failed for browser "
			"%d - %s.",
	                (int)(b - Browsers + 1), strerror(errno));

        if (i > 1)
	  memmove(b, b + 1, (i - 1) * sizeof(cupsd_dirsvc_addr_t));

	b --;
	NumBrowsers --;
      }
    }
}


#ifdef HAVE_LDAP
/*
 * 'ldap_search_rec()' - LDAP Search with reconnect
 */

static int
ldap_search_rec(LDAP        *ld,	/* I - LDAP handler */
                char        *base,	/* I - Base dn */
                int         scope,	/* I - LDAP search scope */
                char        *filter,	/* I - Filter string */
                char        *attrs[],	/* I - Requested attributes */
                int         attrsonly,	/* I - Return only attributes? */
                LDAPMessage **res)	/* I - LDAP handler */
{
  int	rc;				/* Return code */


#  if defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000
  rc = ldap_search_ext_s(ld, base, scope, filter, attrs, attrsonly, NULL, NULL,
                         NULL, LDAP_NO_LIMIT, res);
#  else
  rc = ldap_search_s(ld, base, scope, filter, attrs, attrsonly, res);
#  endif /* defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000 */

 /*
  * If we have a connection problem try again...
  */

  if (rc == LDAP_SERVER_DOWN || rc == LDAP_CONNECT_ERROR)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "LDAP search failed with status %d: %s",
                     rc, ldap_err2string(rc));
    cupsdLogMessage(CUPSD_LOG_INFO,
                    "We try the LDAP search once again after reconnecting to "
		    "the server");
    ldap_freeres(*res);
    ldap_reconnect();

#  if defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000
    rc = ldap_search_ext_s(ld, base, scope, filter, attrs, attrsonly, NULL,
                           NULL, NULL, LDAP_NO_LIMIT, res);
#  else
    rc = ldap_search_s(ld, base, scope, filter, attrs, attrsonly, res);
#  endif /* defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000 */
  }

  if (rc == LDAP_NO_SUCH_OBJECT)
    cupsdLogMessage(CUPSD_LOG_DEBUG,
                    "ldap_search_rec: LDAP entry/object not found");
  else if (rc != LDAP_SUCCESS)
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "ldap_search_rec: LDAP search failed with status %d: %s",
                     rc, ldap_err2string(rc));

  if (rc != LDAP_SUCCESS)
    ldap_freeres(*res);

  return (rc);
}


/*
 * 'ldap_freeres()' - Free LDAPMessage
 */

static void
ldap_freeres(LDAPMessage *entry)	/* I - LDAP handler */
{
  int	rc;				/* Return value */


  rc = ldap_msgfree(entry);
  if (rc == -1)
    cupsdLogMessage(CUPSD_LOG_WARN,
                    "Can't free LDAPMessage!");
  else if (rc == 0)
    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "Freeing LDAPMessage was unnecessary");
}


/*
 * 'ldap_getval_char()' - Get first LDAP value and convert to string
 */

static int
ldap_getval_firststring(
    LDAP          *ld,			/* I - LDAP handler */
    LDAPMessage   *entry,		/* I - LDAP message or search result */
    char          *attr,		/* I - the wanted attribute  */
    char          *retval,		/* O - String to return */
    unsigned long maxsize)		/* I - Max string size */
{
  char			*dn;		/* LDAP DN */
  int			rc = 0;		/* Return code */
#  if defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000
  struct berval		**bval;		/* LDAP value array */
  unsigned long		size;		/* String size */


 /*
  * Get value from LDAPMessage...
  */

  if ((bval = ldap_get_values_len(ld, entry, attr)) == NULL)
  {
    rc = -1;
    dn = ldap_get_dn(ld, entry);
    cupsdLogMessage(CUPSD_LOG_WARN,
                    "Failed to get LDAP value %s for %s!",
                    attr, dn);
    ldap_memfree(dn);
  }
  else
  {

   /*
    * Check size and copy value into our string...
    */

    size = maxsize;
    if (size < bval[0]->bv_len)
    {
      rc = -1;
      dn = ldap_get_dn(ld, entry);
      cupsdLogMessage(CUPSD_LOG_WARN,
                      "Attribute %s is too big! (dn: %s)",
                      attr, dn);
      ldap_memfree(dn);
    }
    else
      size = bval[0]->bv_len;

    strlcpy(retval, bval[0]->bv_val, size);
    ldap_value_free_len(bval);
  }
#  else
  char			**value;	/* LDAP value */

 /*
  * Get value from LDAPMessage...
  */

  if ((value = (char **)ldap_get_values(ld, entry, attr)) == NULL)
  {
    rc = -1;
    dn = ldap_get_dn(ld, entry);
    cupsdLogMessage(CUPSD_LOG_WARN,
                    "Failed to get LDAP value %s for %s!",
                    attr, dn);
    ldap_memfree(dn);
  }
  else
  {
    strlcpy(retval, *value, maxsize);
    ldap_value_free(value);
  }
#  endif /* defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000 */

  return (rc);
}


/*
 * 'send_ldap_ou()' - Send LDAP ou registrations.
 */

static void
send_ldap_ou(char *ou,			/* I - Servername/ou to register */
             char *basedn,		/* I - Our base dn */
             char *descstring)		/* I - Description for ou */
{
  int           i;                      /* Looping var... */
  LDAPMod       mods[3];                /* The 3 attributes we will be adding */
  LDAPMod       *pmods[4];              /* Pointers to the 3 attributes + NULL */
  LDAPMessage   *res,                   /* Search result token */
		*e;			/* Current entry from search */
  int           rc;                     /* LDAP status */
  char          dn[1024],               /* DN of the organizational unit we are adding */
                *desc[2],               /* Change records */
                *ou_value[2];
  char		old_desc[1024];		/* Old description */
  static const char * const objectClass_values[] =
		{			/* The 2 objectClass's we use in */
		  "top",		/* our LDAP entries              */
		  "organizationalUnit",
		  NULL
		};
  static const char * const ou_attrs[] =/* CUPS LDAP attributes */
		{
		  "description",
		  NULL
		};


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "send_ldap_ou: %s", ou);

 /*
  * Reconnect if LDAP Handle is invalid...
  */

  if (! BrowseLDAPHandle)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "send_ldap_ou: LDAP Handle is invalid. Try "
		    "reconnecting...");
    ldap_reconnect();
    return;
  }

 /*
  * Prepare ldap search...
  */

  snprintf(dn, sizeof(dn), "ou=%s, %s", ou, basedn);
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "send_ldap_ou: dn=\"%s\"", dn);

  ou_value[0] = ou;
  ou_value[1] = NULL;
  desc[0]     = descstring;
  desc[1]     = NULL;
  
  mods[0].mod_type   = "ou";
  mods[0].mod_values = ou_value;
  mods[1].mod_type   = "description";
  mods[1].mod_values = desc;
  mods[2].mod_type   = "objectClass";
  mods[2].mod_values = (char **)objectClass_values;

  rc = ldap_search_rec(BrowseLDAPHandle, dn, LDAP_SCOPE_BASE, NULL,
                       (char **)ou_attrs, 0, &res);

 /*
  * If ldap search was not successfull then exit function...
  */

  if (rc != LDAP_SUCCESS && rc != LDAP_NO_SUCH_OBJECT)
    return;

 /*
  * Check if we need to insert or update the LDAP entry...
  */

  if (ldap_count_entries(BrowseLDAPHandle, res) > 0 &&
      rc != LDAP_NO_SUCH_OBJECT)
  {
   /*
    * Printserver has already been registered, check if
    * modification is required...
    */

    e = ldap_first_entry(BrowseLDAPHandle, res);

   /*
    * Get the required values from this entry...
    */

    if (ldap_getval_firststring(BrowseLDAPHandle, e, "description", old_desc,
                                sizeof(old_desc)) == -1)
      old_desc[0] = '\0';

   /*
    * Check if modification is required...
    */

    if ( strcmp(desc[0], old_desc) == 0 )
    {
     /*
      * LDAP entry for the printer exists.
      * Printer has already been registered,
      * no modifications required...
      */
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "send_ldap_ou: No updates required for %s", ou);
    }
    else
    {

      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "send_ldap_ou: Replace entry for %s", ou);

      for (i = 0; i < 3; i ++)
      {
        pmods[i]         = mods + i;
        pmods[i]->mod_op = LDAP_MOD_REPLACE;
      }
      pmods[i] = NULL;

#  if defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000
      if ((rc = ldap_modify_ext_s(BrowseLDAPHandle, dn, pmods, NULL,
                                  NULL)) != LDAP_SUCCESS)
#  else
      if ((rc = ldap_modify_s(BrowseLDAPHandle, dn, pmods)) != LDAP_SUCCESS)
#  endif /* defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000 */
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
                        "LDAP modify for %s failed with status %d: %s",
                        ou, rc, ldap_err2string(rc));
        if ( LDAP_SERVER_DOWN == rc )
          ldap_reconnect();
      }
    }
  }
  else
  {
   /*
    * Printserver has never been registered,
    * add registration...
    */

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "send_ldap_ou: Add entry for %s", ou);

    for (i = 0; i < 3; i ++)
    {
      pmods[i]         = mods + i;
      pmods[i]->mod_op = LDAP_MOD_ADD;
    }
    pmods[i] = NULL;

#  if defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000
    if ((rc = ldap_add_ext_s(BrowseLDAPHandle, dn, pmods, NULL,
                             NULL)) != LDAP_SUCCESS)
#  else
    if ((rc = ldap_add_s(BrowseLDAPHandle, dn, pmods)) != LDAP_SUCCESS)
#  endif /* defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000 */
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "LDAP add for %s failed with status %d: %s",
                      ou, rc, ldap_err2string(rc));
      if ( LDAP_SERVER_DOWN == rc )
        ldap_reconnect();
    }
  }

  ldap_freeres(res);
}


/*
 * 'send_ldap_browse()' - Send LDAP printer registrations.
 */

static void
send_ldap_browse(cupsd_printer_t *p)	/* I - Printer to register */
{
  int		i;			/* Looping var... */
  LDAPMod	mods[7];		/* The 7 attributes we will be adding */
  LDAPMod	*pmods[8];		/* Pointers to the 7 attributes + NULL */
  LDAPMessage	*res,			/* Search result token */
		*e;			/* Current entry from search */
  char		*cn_value[2],		/* Change records */
		*uri[2],
		*info[2],
		*location[2],
		*make_model[2],
		*type[2],
		typestring[255],	/* String to hold printer-type */
		dn[1024];		/* DN of the printer we are adding */
  int		rc;			/* LDAP status */
  char		old_uri[HTTP_MAX_URI],	/* Printer URI */
		old_location[1024],	/* Printer location */
		old_info[1024],		/* Printer information */
		old_make_model[1024],	/* Printer make and model */
		old_type_string[30];	/* Temporary type number */
  int		old_type;		/* Printer type */
  static const char * const objectClass_values[] =
		{			/* The 3 objectClass's we use in */
		  "top",		/* our LDAP entries              */
		  "device",
		  "cupsPrinter",
		  NULL
		};


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "send_ldap_browse: %s", p->name);

 /*
  * Exit function if LDAP updates has been disabled...
  */

  if (!BrowseLDAPUpdate)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "send_ldap_browse: Updates temporary disabled; "
		    "skipping...");
    return;
  }

 /*
  * Reconnect if LDAP Handle is invalid...
  */

  if (!BrowseLDAPHandle)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "send_ldap_browse: LDAP Handle is invalid. Try "
		    "reconnecting...");
    ldap_reconnect();
    return;
  }

 /*
  * Everything in ldap is ** so we fudge around it...
  */

  sprintf(typestring, "%u", p->type);

  cn_value[0]   = p->name;
  cn_value[1]   = NULL;
  info[0]       = p->info ? p->info : "Unknown";
  info[1]       = NULL;
  location[0]   = p->location ? p->location : "Unknown";
  location[1]   = NULL;
  make_model[0] = p->make_model ? p->make_model : "Unknown";
  make_model[1] = NULL;
  type[0]       = typestring;
  type[1]       = NULL;
  uri[0]        = p->uri;
  uri[1]        = NULL;

 /*
  * Get ldap entry for printer ...
  */

  snprintf(dn, sizeof(dn), "cn=%s, ou=%s, %s", p->name, ServerName,
           BrowseLDAPDN);
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "send_ldap_browse: dn=\"%s\"", dn);

  rc = ldap_search_rec(BrowseLDAPHandle, dn, LDAP_SCOPE_BASE, NULL,
                       (char **)ldap_attrs, 0, &res);

 /*
  * If ldap search was not successfull then exit function
  * and temporary disable LDAP updates...
  */

  if (rc != LDAP_SUCCESS && rc != LDAP_NO_SUCH_OBJECT)
  {
    if (BrowseLDAPUpdate &&
        (rc == LDAP_SERVER_DOWN || rc == LDAP_CONNECT_ERROR))
    {
      BrowseLDAPUpdate = FALSE;
      cupsdLogMessage(CUPSD_LOG_INFO,
                      "LDAP update temporary disabled");
    }

    return;
  }

 /*
  * Fill modification array...
  */

  mods[0].mod_type   = "cn";
  mods[0].mod_values = cn_value;
  mods[1].mod_type   = "printerDescription";
  mods[1].mod_values = info;
  mods[2].mod_type   = "printerURI";
  mods[2].mod_values = uri;
  mods[3].mod_type   = "printerLocation";
  mods[3].mod_values = location;
  mods[4].mod_type   = "printerMakeAndModel";
  mods[4].mod_values = make_model;
  mods[5].mod_type   = "printerType";
  mods[5].mod_values = type;
  mods[6].mod_type   = "objectClass";
  mods[6].mod_values = (char **)objectClass_values;

 /*
  * Check if we need to insert or update the LDAP entry...
  */

  if (ldap_count_entries(BrowseLDAPHandle, res) > 0 &&
      rc != LDAP_NO_SUCH_OBJECT)
  {
   /*
    * Printer has already been registered, check if
    * modification is required...
    */

    e = ldap_first_entry(BrowseLDAPHandle, res);

   /*
    * Get the required values from this entry...
    */

    if (ldap_getval_firststring(BrowseLDAPHandle, e, "printerDescription",
                                old_info, sizeof(old_info)) == -1)
      old_info[0] = '\0';

    if (ldap_getval_firststring(BrowseLDAPHandle, e, "printerLocation",
                                old_location, sizeof(old_location)) == -1)
      old_info[0] = '\0';

    if (ldap_getval_firststring(BrowseLDAPHandle, e, "printerMakeAndModel",
                                old_make_model, sizeof(old_make_model)) == -1)
      old_info[0] = '\0';

    if (ldap_getval_firststring(BrowseLDAPHandle, e, "printerType",
                                old_type_string, sizeof(old_type_string)) == -1)
      old_info[0] = '\0';

    old_type = atoi(old_type_string);

    if (ldap_getval_firststring(BrowseLDAPHandle, e, "printerURI", old_uri,
                                sizeof(old_uri)) == -1)
      old_info[0] = '\0';

   /*
    * Check if modification is required...
    */

    if (!strcmp(info[0], old_info) && !strcmp(uri[0], old_uri) &&
        !strcmp(location[0], old_location) &&
	!strcmp(make_model[0], old_make_model) && p->type == old_type)
    {
     /*
      * LDAP entry for the printer exists. Printer has already been registered,
      * no modifications required...
      */

      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                       "send_ldap_browse: No updates required for %s", p->name);
    }
    else
    {
     /*
      * LDAP entry for the printer exists.  Printer has already been registered,
      * modify the current registration...
      */

      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "send_ldap_browse: Replace entry for %s", p->name);

      for (i = 0; i < 7; i ++)
      {
        pmods[i]         = mods + i;
        pmods[i]->mod_op = LDAP_MOD_REPLACE;
      }
      pmods[i] = NULL;

#  if defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000
      if ((rc = ldap_modify_ext_s(BrowseLDAPHandle, dn, pmods, NULL,
                                  NULL)) != LDAP_SUCCESS)
#  else
      if ((rc = ldap_modify_s(BrowseLDAPHandle, dn, pmods)) != LDAP_SUCCESS)
#  endif /* defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000 */
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
                        "LDAP modify for %s failed with status %d: %s",
                        p->name, rc, ldap_err2string(rc));
        if (rc == LDAP_SERVER_DOWN)
          ldap_reconnect();
      }
    }
  }
  else 
  {
   /*
    * No LDAP entry exists for the printer.  Printer has never been registered,
    * add the current registration...
    */

    send_ldap_ou(ServerName, BrowseLDAPDN, "CUPS Server");

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "send_ldap_browse: Add entry for %s", p->name);

    for (i = 0; i < 7; i ++)
    {
      pmods[i]         = mods + i;
      pmods[i]->mod_op = LDAP_MOD_ADD;
    }
    pmods[i] = NULL;

#  if defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000
    if ((rc = ldap_add_ext_s(BrowseLDAPHandle, dn, pmods, NULL,
                             NULL)) != LDAP_SUCCESS)
#  else
    if ((rc = ldap_add_s(BrowseLDAPHandle, dn, pmods)) != LDAP_SUCCESS)
#  endif /* defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000 */
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "LDAP add for %s failed with status %d: %s",
                      p->name, rc, ldap_err2string(rc));
      if (rc == LDAP_SERVER_DOWN)
        ldap_reconnect();
    }
  }

  ldap_freeres(res);
}


/*
 * 'ldap_dereg_printer()' - Delete printer from directory
 */

static void
ldap_dereg_printer(cupsd_printer_t *p)	/* I - Printer to deregister */
{
  char		dn[1024];		/* DN of the printer */
  int		rc;			/* LDAP status */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "ldap_dereg_printer: Remove entry for %s",
                  p->name);

 /*
  * Reconnect if LDAP Handle is invalid...
  */

  if (!BrowseLDAPHandle)
  {
    ldap_reconnect();
    return;
  }

 /*
  * Get dn for printer and delete LDAP entry...
  */

  snprintf(dn, sizeof(dn), "cn=%s, ou=%s, %s", p->name, ServerName,
           BrowseLDAPDN);
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "ldap_dereg_printer: dn=\"%s\"", dn);

#  if defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000
  if ((rc = ldap_delete_ext_s(BrowseLDAPHandle, dn, NULL,
                              NULL)) != LDAP_SUCCESS)
#  else
  if ((rc = ldap_delete_s(BrowseLDAPHandle, dn)) != LDAP_SUCCESS)
#  endif /* defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000 */
  {
    cupsdLogMessage(CUPSD_LOG_WARN,
                    "LDAP delete for %s failed with status %d: %s",
                    p->name, rc, ldap_err2string(rc));

   /*
    * If we had a connection problem (connection timed out, etc.)
    * we should reconnect and try again to delete the entry...
    */

    if (rc == LDAP_SERVER_DOWN || rc == LDAP_CONNECT_ERROR)
    {
      cupsdLogMessage(CUPSD_LOG_INFO,
                      "Retry deleting LDAP entry for %s after a reconnect...", p->name);
      ldap_reconnect();

#  if defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000
      if ((rc = ldap_delete_ext_s(BrowseLDAPHandle, dn, NULL,
                                  NULL)) != LDAP_SUCCESS)
#  else
      if ((rc = ldap_delete_s(BrowseLDAPHandle, dn)) != LDAP_SUCCESS)
#  endif /* defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000 */
        cupsdLogMessage(CUPSD_LOG_WARN,
                        "LDAP delete for %s failed with status %d: %s",
                        p->name, rc, ldap_err2string(rc));
    }
  }
}


static void
ldap_dereg_ou(char *ou,			/* I - Organizational unit (servername) */
              char *basedn)		/* I - Dase dn */
{
  char		dn[1024];		/* DN of the printer */
  int		rc;			/* LDAP status */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "ldap_dereg_ou: Remove entry for %s", ou);

 /*
  * Reconnect if LDAP Handle is invalid...
  */

  if (!BrowseLDAPHandle)
  {
    ldap_reconnect();
    return;
  }

 /*
  * Get dn for printer and delete LDAP entry...
  */

  snprintf(dn, sizeof(dn), "ou=%s, %s", ou, basedn);
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "ldap_dereg_ou: dn=\"%s\"", dn);

#  if defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000
  if ((rc = ldap_delete_ext_s(BrowseLDAPHandle, dn, NULL,
                              NULL)) != LDAP_SUCCESS)
#  else
  if ((rc = ldap_delete_s(BrowseLDAPHandle, dn)) != LDAP_SUCCESS)
#  endif /* defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000 */
  {
    cupsdLogMessage(CUPSD_LOG_WARN,
                    "LDAP delete for %s failed with status %d: %s",
                    ou, rc, ldap_err2string(rc));

   /*
    * If we had a connection problem (connection timed out, etc.)
    * we should reconnect and try again to delete the entry...
    */

    if (rc == LDAP_SERVER_DOWN || rc == LDAP_CONNECT_ERROR)
    {
      cupsdLogMessage(CUPSD_LOG_INFO,
                      "Retry deleting LDAP entry for %s after a reconnect...", ou);
      ldap_reconnect();
#  if defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000
      if ((rc = ldap_delete_ext_s(BrowseLDAPHandle, dn, NULL,
                                  NULL)) != LDAP_SUCCESS)
#  else
      if ((rc = ldap_delete_s(BrowseLDAPHandle, dn)) != LDAP_SUCCESS)
#  endif /* defined(HAVE_OPENLDAP) && LDAP_API_VERSION > 3000 */
        cupsdLogMessage(CUPSD_LOG_WARN,
                        "LDAP delete for %s failed with status %d: %s",
                        ou, rc, ldap_err2string(rc));
    }

  }
}
#endif /* HAVE_LDAP */


#ifdef HAVE_LIBSLP
/*
 * 'send_slp_browse()' - Register the specified printer with SLP.
 */

static void
send_slp_browse(cupsd_printer_t *p)	/* I - Printer to register */
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


  cupsdLogMessage(CUPSD_LOG_DEBUG, "send_slp_browse(%p = \"%s\")", p,
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
	   "(printer-type=%d),"
	   "(charset-supported=utf-8),"
	   "(natural-language-configured=%s),"
	   "(natural-language-supported=de,en,es,fr,it),"
           "(color-supported=%s),"
           "(finishings-supported=%s),"
           "(sides-supported=one-sided%s),"
	   "(multiple-document-jobs-supported=true)"
	   "(ipp-versions-supported=1.0,1.1)",
	   p->uri, authentication->values[0].string.text, p->name, location,
	   info, p->uri, make_model, p->type, DefaultLanguage,
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
  char			*tmp = 0;	/* Temporary string */
  cupsd_printer_t	*p = (cupsd_printer_t*)cookie;
					/* Current printer */


  (void)hslp;				/* anti-compiler-warning-code */

 /*
  * Bail if there was an error
  */

  if (errcode != SLP_OK)
    return (SLP_TRUE);

 /*
  * Parse the attrlist to obtain things needed to build CUPS browse packet
  */

  memset(p, 0, sizeof(cupsd_printer_t));

  if (slp_get_attr(attrlist, "(printer-location=", &(p->location)))
    return (SLP_FALSE);
  if (slp_get_attr(attrlist, "(printer-info=", &(p->info)))
    return (SLP_FALSE);
  if (slp_get_attr(attrlist, "(printer-make-and-model=", &(p->make_model)))
    return (SLP_FALSE);
  if (!slp_get_attr(attrlist, "(printer-type=", &tmp))
    p->type = atoi(tmp);
  else
    p->type = CUPS_PRINTER_REMOTE;

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
 * 'update_cups_browse()' - Update the browse lists using the CUPS protocol.
 */

static void
update_cups_browse(void)
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
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI],	/* Resource portion of URI */
		info[IPP_MAX_NAME],	/* Information string */
		location[IPP_MAX_NAME],	/* Location string */
		make_model[IPP_MAX_NAME];/* Make and model string */
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
      cupsdLogMessage(CUPSD_LOG_ERROR, "CUPS browsing turned off.");

#ifdef WIN32
      closesocket(BrowseSocket);
#else
      close(BrowseSocket);
#endif /* WIN32 */

      cupsdRemoveSelect(BrowseSocket);
      BrowseSocket = -1;

      BrowseLocalProtocols  &= ~BROWSE_CUPS;
      BrowseRemoteProtocols &= ~BROWSE_CUPS;
    }

    return;
  }

  packet[bytes] = '\0';

 /*
  * If we're about to sleep, ignore incoming browse packets.
  */

  if (Sleeping)
    return;

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

      auth = CUPSD_AUTH_ALLOW;
    }
    else
    {
     /*
      * Do authorization checks on the domain/address...
      */

      switch (BrowseACL->order_type)
      {
        default :
	    auth = CUPSD_AUTH_DENY;	/* anti-compiler-warning-code */
	    break;

	case CUPSD_AUTH_ALLOW : /* Order Deny,Allow */
            auth = CUPSD_AUTH_ALLOW;

            if (cupsdCheckAuth(address, srcname, len,
	        	  BrowseACL->num_deny, BrowseACL->deny))
	      auth = CUPSD_AUTH_DENY;

            if (cupsdCheckAuth(address, srcname, len,
	        	  BrowseACL->num_allow, BrowseACL->allow))
	      auth = CUPSD_AUTH_ALLOW;
	    break;

	case CUPSD_AUTH_DENY : /* Order Allow,Deny */
            auth = CUPSD_AUTH_DENY;

            if (cupsdCheckAuth(address, srcname, len,
	        	  BrowseACL->num_allow, BrowseACL->allow))
	      auth = CUPSD_AUTH_ALLOW;

            if (cupsdCheckAuth(address, srcname, len,
	        	  BrowseACL->num_deny, BrowseACL->deny))
	      auth = CUPSD_AUTH_DENY;
	    break;
      }
    }
  }
  else
    auth = CUPSD_AUTH_ALLOW;

  if (auth == CUPSD_AUTH_DENY)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG,
                    "update_cups_browse: Refused %d bytes from %s", bytes,
                    srcname);
    return;
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "update_cups_browse: (%d bytes from %s) %s", bytes,
		  srcname, packet);

 /*
  * Parse packet...
  */

  if (sscanf(packet, "%x%x%1023s", &type, &state, uri) < 3)
  {
    cupsdLogMessage(CUPSD_LOG_WARN,
                    "update_cups_browse: Garbled browse packet - %s", packet);
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

  if (is_local_queue(uri, host, sizeof(host), resource, sizeof(resource)))
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
		 httpAddrLength(&(Relays[i].to))) <= 0)
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "update_cups_browse: sendto failed for relay %d - %s.",
	                i + 1, strerror(errno));
	cupsFreeOptions(num_attrs, attrs);
	return;
      }

 /*
  * Process the browse data...
  */

  process_browse_data(uri, host, resource, (cups_ptype_t)type,
                      (ipp_pstate_t)state, location, info, make_model,
		      num_attrs, attrs);
}


/*
 * 'update_lpd()' - Update the LPD configuration as needed.
 */

static void
update_lpd(int onoff)			/* - 1 = turn on, 0 = turn off */
{
  if (!LPDConfigFile)
    return;

#ifdef __APPLE__
 /*
  * Allow /etc/hostconfig CUPS_LPD service setting to override cupsd.conf
  * setting for backwards-compatibility.
  */

  if (onoff && !get_hostconfig("CUPS_LPD"))
    onoff = 0;
#endif /* __APPLE__ */

  if (!strncmp(LPDConfigFile, "xinetd:///", 10))
  {
   /*
    * Enable/disable LPD via the xinetd.d config file for cups-lpd...
    */

    char	newfile[1024];		/* New cups-lpd.N file */
    cups_file_t	*ofp,			/* Original file pointer */
		*nfp;			/* New file pointer */
    char	line[1024];		/* Line from file */


    snprintf(newfile, sizeof(newfile), "%s.N", LPDConfigFile + 9);

    if ((ofp = cupsFileOpen(LPDConfigFile + 9, "r")) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to open \"%s\" - %s",
                      LPDConfigFile + 9, strerror(errno));
      return;
    }

    if ((nfp = cupsFileOpen(newfile, "w")) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to create \"%s\" - %s",
                      newfile, strerror(errno));
      cupsFileClose(ofp);
      return;
    }

   /*
    * Copy all of the lines from the cups-lpd file...
    */

    while (cupsFileGets(ofp, line, sizeof(line)))
    {
      if (line[0] == '{')
      {
        cupsFilePrintf(nfp, "%s\n", line);
        snprintf(line, sizeof(line), "\tdisable = %s",
	         onoff ? "no" : "yes");
      }
      else if (!strstr(line, "disable ="))
        cupsFilePrintf(nfp, "%s\n", line);
    }

    cupsFileClose(nfp);
    cupsFileClose(ofp);
    rename(newfile, LPDConfigFile + 9);
  }
#ifdef __APPLE__
  else if (!strncmp(LPDConfigFile, "launchd:///", 11))
  {
   /*
    * Enable/disable LPD via the launchctl command...
    */

    char	*argv[5],		/* Arguments for command */
		*envp[MAX_ENV];		/* Environment for command */
    int		pid;			/* Process ID */


    cupsdLoadEnv(envp, (int)(sizeof(envp) / sizeof(envp[0])));
    argv[0] = (char *)"launchctl";
    argv[1] = (char *)(onoff ? "load" : "unload");
    argv[2] = (char *)"-w";
    argv[3] = LPDConfigFile + 10;
    argv[4] = NULL;

    cupsdStartProcess("/bin/launchctl", argv, envp, -1, -1, -1, -1, -1, 1,
                      NULL, &pid);
  }
#endif /* __APPLE__ */
  else
    cupsdLogMessage(CUPSD_LOG_INFO, "Unknown LPDConfigFile scheme!");
}


/*
 * 'update_polling()' - Read status messages from the poll daemons.
 */

static void
update_polling(void)
{
  char		*ptr,			/* Pointer to end of line in buffer */
		message[1024];		/* Pointer to message text */
  int		loglevel;		/* Log level for message */


  while ((ptr = cupsdStatBufUpdate(PollStatusBuffer, &loglevel,
                                   message, sizeof(message))) != NULL)
    if (!strchr(PollStatusBuffer->buffer, '\n'))
      break;

  if (ptr == NULL && !PollStatusBuffer->bufused)
  {
   /*
    * All polling processes have died; stop polling...
    */

    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "update_polling: all polling processes have exited!");
    cupsdStopPolling();
  }
}


/*
 * 'update_smb()' - Update the SMB configuration as needed.
 */

static void
update_smb(int onoff)			/* I - 1 = turn on, 0 = turn off */
{
  if (!SMBConfigFile)
    return;

  if (!strncmp(SMBConfigFile, "samba:///", 9))
  {
   /*
    * Enable/disable SMB via the specified smb.conf config file...
    */

    char	newfile[1024];		/* New smb.conf.N file */
    cups_file_t	*ofp,			/* Original file pointer */
		*nfp;			/* New file pointer */
    char	line[1024];		/* Line from file */
    int		in_printers;		/* In [printers] section? */


    snprintf(newfile, sizeof(newfile), "%s.N", SMBConfigFile + 8);

    if ((ofp = cupsFileOpen(SMBConfigFile + 8, "r")) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to open \"%s\" - %s",
                      SMBConfigFile + 8, strerror(errno));
      return;
    }

    if ((nfp = cupsFileOpen(newfile, "w")) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to create \"%s\" - %s",
                      newfile, strerror(errno));
      cupsFileClose(ofp);
      return;
    }

   /*
    * Copy all of the lines from the smb.conf file...
    */

    in_printers = 0;

    while (cupsFileGets(ofp, line, sizeof(line)))
    {
      if (in_printers && strstr(line, "printable ="))
        snprintf(line, sizeof(line), "    printable = %s",
	         onoff ? "yes" : "no");

      cupsFilePrintf(nfp, "%s\n", line);

      if (line[0] == '[')
        in_printers = !strcmp(line, "[printers]");
    }

    cupsFileClose(nfp);
    cupsFileClose(ofp);
    rename(newfile, SMBConfigFile + 8);
  }
  else
    cupsdLogMessage(CUPSD_LOG_INFO, "Unknown SMBConfigFile scheme!");
}


/*
 * End of "$Id$".
 */
