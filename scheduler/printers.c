/*
 * "$Id$"
 *
 *   Printer routines for the Common UNIX Printing System (CUPS).
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
 *   cupsdAddPrinter()           - Add a printer to the system.
 *   cupsdAddPrinterFilter()     - Add a MIME filter for a printer.
 *   cupsdAddPrinterHistory()    - Add the current printer state to the history.
 *   cupsdAddPrinterUser()       - Add a user to the ACL.
 *   cupsdDeleteAllPrinters()    - Delete all printers from the system.
 *   cupsdDeletePrinter()        - Delete a printer from the system.
 *   cupsdDeletePrinterFilters() - Delete all MIME filters for a printer.
 *   cupsdFindPrinter()          - Find a printer in the list.
 *   cupsdFreePrinterUsers()     - Free allow/deny users.
 *   cupsdLoadAllPrinters()      - Load printers from the printers.conf file.
 *   cupsdSaveAllPrinters()      - Save all printer definitions to the printers.conf
 *   cupsdSetPrinterAttrs()      - Set printer attributes based upon the PPD file.
 *   cupsdSetPrinterReasons()    - Set/update the reasons strings.
 *   cupsdSetPrinterState()      - Update the current state of a printer.
 *   cupsdStopPrinter()          - Stop a printer from printing any jobs...
 *   cupsdValidateDest()         - Validate a printer/class destination.
 *   cupsdWritePrintcap()        - Write a pseudo-printcap file for older
 *                                 applications that need it...
 *   cupsdSanitizeURI()          - Sanitize a device URI...
 *   compare_printers()          - Compare two printers.
 *   write_irix_config()         - Update the config files used by the IRIX
 *                                 desktop tools.
 *   write_irix_state()          - Update the status files used by IRIX printing
 *                                 desktop tools.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * Local functions...
 */

static int	compare_printers(void *first, void *second, void *data);
#ifdef __sgi
static void	write_irix_config(cupsd_printer_t *p);
static void	write_irix_state(cupsd_printer_t *p);
#endif /* __sgi */


/*
 * 'cupsdAddPrinter()' - Add a printer to the system.
 */

cupsd_printer_t *			/* O - New printer */
cupsdAddPrinter(const char *name)	/* I - Name of printer */
{
  cupsd_printer_t	*p;		/* New printer */


 /*
  * Range check input...
  */

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdAddPrinter(\"%s\")", name);

 /*
  * Create a new printer entity...
  */

  if ((p = calloc(1, sizeof(cupsd_printer_t))) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_CRIT, "Unable to allocate memory for printer - %s",
                    strerror(errno));
    return (NULL);
  }

  cupsdSetString(&p->name, name);
  cupsdSetString(&p->info, name);
  cupsdSetString(&p->hostname, ServerName);

  cupsdSetStringf(&p->uri, "ipp://%s:%d/printers/%s", ServerName, LocalPort, name);
  cupsdSetStringf(&p->device_uri, "file:/dev/null");

  p->state     = IPP_PRINTER_STOPPED;
  p->accepting = 0;
  p->shared    = 1;
  p->filetype  = mimeAddType(MimeDatabase, "printer", name);

  cupsdSetString(&p->job_sheets[0], "none");
  cupsdSetString(&p->job_sheets[1], "none");

  cupsdSetString(&p->error_policy, "stop-printer");
  cupsdSetString(&p->op_policy, DefaultPolicy);

  p->op_policy_ptr = DefaultPolicyPtr;

  if (MaxPrinterHistory)
    p->history = calloc(MaxPrinterHistory, sizeof(ipp_t *));

 /*
  * Insert the printer in the printer list alphabetically...
  */

  if (!Printers)
    Printers = cupsArrayNew(compare_printers, NULL);

  cupsArrayAdd(Printers, p);

  if (!ImplicitPrinters)
    ImplicitPrinters = cupsArrayNew(compare_printers, NULL);

 /*
  * Write a new /etc/printcap or /var/spool/lp/pstatus file.
  */

  cupsdWritePrintcap();

 /*
  * Return the new printer...
  */

  return (p);
}


/*
 * 'cupsdAddPrinterFilter()' - Add a MIME filter for a printer.
 */

void
cupsdAddPrinterFilter(
    cupsd_printer_t  *p,		/* I - Printer to add to */
    const char       *filter)		/* I - Filter to add */
{
  int		i;			/* Looping var */
  char		super[MIME_MAX_SUPER],	/* Super-type for filter */
		type[MIME_MAX_TYPE],	/* Type for filter */
		program[1024];		/* Program/filter name */
  int		cost;			/* Cost of filter */
  mime_type_t	**temptype;		/* MIME type looping var */


 /*
  * Range check input...
  */

  if (p == NULL || p->filetype == NULL || filter == NULL)
    return;

 /*
  * Parse the filter string; it should be in the following format:
  *
  *     super/type cost program
  */

  if (sscanf(filter, "%15[^/]/%31s%d%1023s", super, type, &cost, program) != 4)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "cupsdAddPrinterFilter: Invalid filter string \"%s\"!",
                    filter);
    return;
  }

 /*
  * Add the filter to the MIME database, supporting wildcards as needed...
  */

  for (temptype = MimeDatabase->types, i = MimeDatabase->num_types;
       i > 0;
       i --, temptype ++)
    if (((super[0] == '*' && strcasecmp((*temptype)->super, "printer") != 0) ||
         !strcasecmp((*temptype)->super, super)) &&
        (type[0] == '*' || !strcasecmp((*temptype)->type, type)))
    {
      cupsdLogMessage(CUPSD_LOG_DEBUG2, "Adding filter %s/%s %s/%s %d %s",
                      (*temptype)->super, (*temptype)->type,
		      p->filetype->super, p->filetype->type,
                      cost, program);
      mimeAddFilter(MimeDatabase, *temptype, p->filetype, cost, program);
    }
}


/*
 * 'cupsdAddPrinterHistory()' - Add the current printer state to the history.
 */

void
cupsdAddPrinterHistory(
    cupsd_printer_t *p)			/* I - Printer */
{
  ipp_t	*history;			/* History collection */


 /*
  * Stop early if we aren't keeping history data...
  */

  if (MaxPrinterHistory <= 0)
    return;

 /*
  * Retire old history data as needed...
  */

  p->sequence_number ++;

  if (p->num_history >= MaxPrinterHistory)
  {
    p->num_history --;
    ippDelete(p->history[0]);
    memmove(p->history, p->history + 1, p->num_history * sizeof(ipp_t *));
  }

 /*
  * Create a collection containing the current printer-state, printer-up-time,
  * printer-state-message, and printer-state-reasons attributes.
  */

  history = ippNew();
  ippAddInteger(history, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state",
                p->state);
  ippAddBoolean(history, IPP_TAG_PRINTER, "printer-is-accepting-jobs",
                p->accepting);
  ippAddString(history, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-state-message",
               NULL, p->state_message);
  if (p->num_reasons == 0)
    ippAddString(history, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                 "printer-state-reasons", NULL,
		 p->state == IPP_PRINTER_STOPPED ? "paused" : "none");
  else
    ippAddStrings(history, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                  "printer-state-reasons", p->num_reasons, NULL,
		  (const char * const *)p->reasons);
  ippAddInteger(history, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "printer-state-time", p->state_time);
  ippAddInteger(history, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "printer-state-sequence-number", p->sequence_number);

  p->history[p->num_history] = history;
  p->num_history ++;
}


/*
 * 'cupsdAddPrinterUser()' - Add a user to the ACL.
 */

void
cupsdAddPrinterUser(
    cupsd_printer_t *p,			/* I - Printer */
    const char      *username)		/* I - User */
{
  const char	**temp;			/* Temporary array pointer */


  if (!p || !username)
    return;

  if (p->num_users == 0)
    temp = malloc(sizeof(char **));
  else
    temp = realloc(p->users, sizeof(char **) * (p->num_users + 1));

  if (!temp)
    return;

  p->users = temp;
  temp     += p->num_users;

  if ((*temp = strdup(username)) != NULL)
    p->num_users ++;
}


/*
 * 'cupsdCreateCommonData()' - Create the common printer data.
 */

void
cupsdCreateCommonData(void)
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* Attribute data */
  cupsd_printer_t	*p;		/* Current printer */
  static const int nups[] =		/* number-up-supported values */
		{ 1, 2, 4, 6, 9, 16 };
  static const ipp_orient_t orients[4] =/* orientation-requested-supported values */
		{
		  IPP_PORTRAIT,
		  IPP_LANDSCAPE,
		  IPP_REVERSE_LANDSCAPE,
		  IPP_REVERSE_PORTRAIT
		};
  static const char * const holds[] =	/* job-hold-until-supported values */
		{
		  "no-hold",
		  "indefinite",
		  "day-time",
		  "evening",
		  "night",
		  "second-shift",
		  "third-shift",
		  "weekend"
		};
  static const char * const versions[] =/* ipp-versions-supported values */
		{
		  "1.0",
		  "1.1"
		};
  static const ipp_op_t	ops[] =		/* operations-supported values */
		{
		  IPP_PRINT_JOB,
		  IPP_VALIDATE_JOB,
		  IPP_CREATE_JOB,
		  IPP_SEND_DOCUMENT,
		  IPP_CANCEL_JOB,
		  IPP_GET_JOB_ATTRIBUTES,
		  IPP_GET_JOBS,
		  IPP_GET_PRINTER_ATTRIBUTES,
		  IPP_HOLD_JOB,
		  IPP_RELEASE_JOB,
		  IPP_PAUSE_PRINTER,
		  IPP_RESUME_PRINTER,
		  IPP_PURGE_JOBS,
		  IPP_SET_JOB_ATTRIBUTES,
		  IPP_CREATE_PRINTER_SUBSCRIPTION,
		  IPP_CREATE_JOB_SUBSCRIPTION,
		  IPP_GET_SUBSCRIPTION_ATTRIBUTES,
		  IPP_GET_SUBSCRIPTIONS,
		  IPP_RENEW_SUBSCRIPTION,
		  IPP_CANCEL_SUBSCRIPTION,
		  IPP_GET_NOTIFICATIONS,
		  IPP_ENABLE_PRINTER,
		  IPP_DISABLE_PRINTER,
		  CUPS_GET_DEFAULT,
		  CUPS_GET_PRINTERS,
		  CUPS_ADD_PRINTER,
		  CUPS_DELETE_PRINTER,
		  CUPS_GET_CLASSES,
		  CUPS_ADD_CLASS,
		  CUPS_DELETE_CLASS,
		  CUPS_ACCEPT_JOBS,
		  CUPS_REJECT_JOBS,
		  CUPS_SET_DEFAULT,
		  CUPS_GET_DEVICES,
		  CUPS_GET_PPDS,
		  CUPS_MOVE_JOB,
		  CUPS_AUTHENTICATE_JOB,
		  IPP_RESTART_JOB
		};
  static const char * const charsets[] =/* charset-supported values */
		{
		  "us-ascii",
		  "utf-8"
		};
  static const char * const compressions[] =
		{			/* document-compression-supported values */
		  "none"
#ifdef HAVE_LIBZ
		  ,"gzip"
#endif /* HAVE_LIBZ */
		};
  static const char * const multiple_document_handling[] =
		{			/* multiple-document-handling-supported values */
		  "separate-documents-uncollated-copies",
		  "separate-documents-collated-copies"
		};
  static const char * const errors[] =	/* printer-error-policy-supported values */
		{
		  "abort-job",
		  "retry-job",
		  "stop-printer"
		};


  if (CommonData)
    ippDelete(CommonData);

  CommonData = ippNew();

  ippAddString(CommonData, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
               "pdl-override-supported", NULL, "not-attempted");
  ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                "ipp-versions-supported", sizeof(versions) / sizeof(versions[0]),
		NULL, versions);
  ippAddIntegers(CommonData, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                 "operations-supported",
                 sizeof(ops) / sizeof(ops[0]) + JobFiles - 1, (int *)ops);
  ippAddBoolean(CommonData, IPP_TAG_PRINTER,
                "multiple-document-jobs-supported", 1);
  ippAddInteger(CommonData, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "multiple-operation-time-out", 60);
  ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                "multiple-document-handling-supported",
                sizeof(multiple_document_handling) /
		    sizeof(multiple_document_handling[0]), NULL,
	        multiple_document_handling);
  ippAddString(CommonData, IPP_TAG_PRINTER, IPP_TAG_CHARSET,
               "charset-configured", NULL, DefaultCharset);
  ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_CHARSET,
                "charset-supported", sizeof(charsets) / sizeof(charsets[0]),
		NULL, charsets);
  ippAddString(CommonData, IPP_TAG_PRINTER, IPP_TAG_LANGUAGE,
               "natural-language-configured", NULL, DefaultLanguage);
  ippAddString(CommonData, IPP_TAG_PRINTER, IPP_TAG_LANGUAGE,
               "generated-natural-language-supported", NULL, DefaultLanguage);
  ippAddString(CommonData, IPP_TAG_PRINTER, IPP_TAG_MIMETYPE,
               "document-format-default", NULL, "application/octet-stream");
  ippAddStrings(CommonData, IPP_TAG_PRINTER,
                (ipp_tag_t)(IPP_TAG_MIMETYPE | IPP_TAG_COPY),
                "document-format-supported", NumMimeTypes, NULL, MimeTypes);
  ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
        	"compression-supported",
		sizeof(compressions) / sizeof(compressions[0]),
		NULL, compressions);
  ippAddInteger(CommonData, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "job-priority-supported", 100);
  ippAddInteger(CommonData, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "job-priority-default", 50);
  ippAddRange(CommonData, IPP_TAG_PRINTER, "copies-supported", 1, MaxCopies);
  ippAddInteger(CommonData, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "copies-default", 1);
  ippAddBoolean(CommonData, IPP_TAG_PRINTER, "page-ranges-supported", 1);
  ippAddIntegers(CommonData, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                 "number-up-supported", sizeof(nups) / sizeof(nups[0]), nups);
  ippAddInteger(CommonData, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "number-up-default", 1);
  ippAddIntegers(CommonData, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                 "orientation-requested-supported", 4, (int *)orients);
  ippAddInteger(CommonData, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                "orientation-requested-default", IPP_PORTRAIT);
  ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                "job-hold-until-supported", sizeof(holds) / sizeof(holds[0]),
		NULL, holds);
  ippAddString(CommonData, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
               "job-hold-until-default", NULL, "no-hold");
  attr = ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_NAME,
                       "printer-op-policy-supported", NumPolicies, NULL, NULL);
  for (i = 0; i < NumPolicies; i ++)
    attr->values[i].string.text = strdup(Policies[i]->name);
  ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_NAME,
                "printer-error-policy-supported",
		sizeof(errors) / sizeof(errors[0]), NULL, errors);

  if (NumBanners > 0)
  {
   /*
    * Setup the job-sheets-supported and job-sheets-default attributes...
    */

    if (Classification && !ClassifyOverride)
      attr = ippAddString(CommonData, IPP_TAG_PRINTER, IPP_TAG_NAME,
                	  "job-sheets-supported", NULL, Classification);
    else
      attr = ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_NAME,
                	   "job-sheets-supported", NumBanners + 1, NULL, NULL);

    if (attr == NULL)
      cupsdLogMessage(CUPSD_LOG_EMERG,
                      "cupsdSetPrinterAttrs: Unable to allocate memory for "
                      "job-sheets-supported attribute: %s!", strerror(errno));
    else if (!Classification || ClassifyOverride)
    {
      attr->values[0].string.text = strdup("none");

      for (i = 0; i < NumBanners; i ++)
	attr->values[i + 1].string.text = strdup(Banners[i].name);
    }
  }

  if (Printers)
  {
   /*
    * Loop through the printers and update the op_policy_ptr values...
    */

    for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
         p;
	 p = (cupsd_printer_t *)cupsArrayNext(Printers))
      if ((p->op_policy_ptr = cupsdFindPolicy(p->op_policy)) == NULL)
	p->op_policy_ptr = DefaultPolicyPtr;
  }
}


/*
 * 'cupsdDeleteAllPrinters()' - Delete all printers from the system.
 */

void
cupsdDeleteAllPrinters(void)
{
  cupsd_printer_t	*p;		/* Pointer to current printer/class */


  for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
       p;
       p = (cupsd_printer_t *)cupsArrayNext(Printers))
    if (!(p->type & CUPS_PRINTER_CLASS))
      cupsdDeletePrinter(p, 0);
}


/*
 * 'cupsdDeletePrinter()' - Delete a printer from the system.
 */

void
cupsdDeletePrinter(
    cupsd_printer_t *p,			/* I - Printer to delete */
    int             update)		/* I - Update printers.conf? */
{
  int		i;			/* Looping var */
#ifdef __sgi
  char		filename[1024];		/* Interface script filename */
#endif /* __sgi */


  DEBUG_printf(("cupsdDeletePrinter(%08x): p->name = \"%s\"...\n", p, p->name));

 /*
  * If this printer is the next for browsing, point to the next one...
  */

  if (p == BrowseNext)
  {
    cupsArrayFind(Printers, p);
    BrowseNext = (cupsd_printer_t *)cupsArrayNext(Printers);
  }

 /*
  * Remove the printer from the list...
  */

  cupsArrayRemove(Printers, p);

 /*
  * Stop printing on this printer...
  */

  cupsdStopPrinter(p, update);

 /*
  * Remove the dummy interface/icon/option files under IRIX...
  */

#ifdef __sgi
  snprintf(filename, sizeof(filename), "/var/spool/lp/interface/%s", p->name);
  unlink(filename);

  snprintf(filename, sizeof(filename), "/var/spool/lp/gui_interface/ELF/%s.gui",
           p->name);
  unlink(filename);

  snprintf(filename, sizeof(filename), "/var/spool/lp/activeicons/%s", p->name);
  unlink(filename);

  snprintf(filename, sizeof(filename), "/var/spool/lp/pod/%s.config", p->name);
  unlink(filename);

  snprintf(filename, sizeof(filename), "/var/spool/lp/pod/%s.status", p->name);
  unlink(filename);

  snprintf(filename, sizeof(filename), "/var/spool/lp/member/%s", p->name);
  unlink(filename);
#endif /* __sgi */

 /*
  * If p is the default printer, assign the next one...
  */

  if (p == DefaultPrinter)
  {
    DefaultPrinter = (cupsd_printer_t *)cupsArrayFirst(Printers);

    cupsdWritePrintcap();
  }

 /*
  * Remove this printer from any classes and send a browse delete message...
  */

  if (!(p->type & CUPS_PRINTER_IMPLICIT))
  {
    cupsdDeletePrinterFromClasses(p);
    cupsdSendBrowseDelete(p);
  }

 /*
  * Free all memory used by the printer...
  */

  if (p->printers != NULL)
    free(p->printers);

  if (MaxPrinterHistory)
  {
    for (i = 0; i < p->num_history; i ++)
      ippDelete(p->history[i]);

    free(p->history);
  }

  for (i = 0; i < p->num_reasons; i ++)
    free(p->reasons[i]);

  ippDelete(p->attrs);

  cupsdDeletePrinterFilters(p);

  cupsdFreePrinterUsers(p);
  cupsdFreeQuotas(p);

  cupsdClearString(&p->uri);
  cupsdClearString(&p->hostname);
  cupsdClearString(&p->name);
  cupsdClearString(&p->location);
  cupsdClearString(&p->make_model);
  cupsdClearString(&p->info);
  cupsdClearString(&p->job_sheets[0]);
  cupsdClearString(&p->job_sheets[1]);
  cupsdClearString(&p->device_uri);
  cupsdClearString(&p->port_monitor);
  cupsdClearString(&p->op_policy);
  cupsdClearString(&p->error_policy);

  free(p);

 /*
  * Write a new /etc/printcap file...
  */

  cupsdWritePrintcap();
}


/*
 * 'cupsdDeletePrinterFilters()' - Delete all MIME filters for a printer.
 */

void
cupsdDeletePrinterFilters(
    cupsd_printer_t *p)			/* I - Printer to remove from */
{
  int		i;			/* Looping var */
  mime_filter_t	*filter;		/* MIME filter looping var */


 /*
  * Range check input...
  */

  if (p == NULL)
    return;

 /*
  * Remove all filters from the MIME database that have a destination
  * type == printer...
  */

  for (filter = MimeDatabase->filters, i = MimeDatabase->num_filters;
       i > 0;
       i --, filter ++)
    if (filter->dst == p->filetype)
    {
     /*
      * Delete the current filter...
      */

      MimeDatabase->num_filters --;

      if (i > 1)
        memmove(filter, filter + 1, sizeof(mime_filter_t) * (i - 1));

      filter --;
    }
}


/*
 * 'cupsdFindDest()' - Find a destination in the list.
 */

cupsd_printer_t *			/* O - Destination in list */
cupsdFindDest(const char *name)		/* I - Name of printer or class to find */
{
  cupsd_printer_t	key;		/* Search key */


  key.name = (char *)name;
  return ((cupsd_printer_t *)cupsArrayFind(Printers, &key));
}


/*
 * 'cupsdFindPrinter()' - Find a printer in the list.
 */

cupsd_printer_t *			/* O - Printer in list */
cupsdFindPrinter(const char *name)	/* I - Name of printer to find */
{
  cupsd_printer_t	*p;		/* Printer in list */


  if ((p = cupsdFindDest(name)) != NULL && (p->type & CUPS_PRINTER_CLASS))
    return (NULL);
  else
    return (p);
}


/*
 * 'cupsdFreePrinterUsers()' - Free allow/deny users.
 */

void
cupsdFreePrinterUsers(
    cupsd_printer_t *p)			/* I - Printer */
{
  int	i;				/* Looping var */


  if (!p || !p->num_users)
    return;

  for (i = 0; i < p->num_users; i ++)
    free((void *)p->users[i]);

  free(p->users);

  p->num_users = 0;
  p->users     = NULL;
}


/*
 * 'cupsdLoadAllPrinters()' - Load printers from the printers.conf file.
 */

void
cupsdLoadAllPrinters(void)
{
  cups_file_t		*fp;		/* printers.conf file */
  int			linenum;	/* Current line number */
  char			line[1024],	/* Line from file */
			*value,		/* Pointer to value */
			*valueptr;	/* Pointer into value */
  cupsd_printer_t	*p;		/* Current printer */


 /*
  * Open the printers.conf file...
  */

  snprintf(line, sizeof(line), "%s/printers.conf", ServerRoot);
  if ((fp = cupsFileOpen(line, "r")) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "cupsdLoadAllPrinters: Unable to open %s - %s", line,
                    strerror(errno));
    return;
  }

 /*
  * Read printer configurations until we hit EOF...
  */

  linenum = 0;
  p       = NULL;

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
	                "cupsdLoadAllPrinters: Loading printer %s...", value);

        p = cupsdAddPrinter(value);
	p->accepting = 1;
	p->state     = IPP_PRINTER_IDLE;

       /*
        * Set the default printer as needed...
	*/

        if (!strcasecmp(line, "<DefaultPrinter"))
	  DefaultPrinter = p;
      }
      else
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of printers.conf.", linenum);
        return;
      }
    }
    else if (!strcasecmp(line, "</Printer>"))
    {
      if (p != NULL)
      {
       /*
        * Close out the current printer...
	*/

        cupsdSetPrinterAttrs(p);
	cupsdAddPrinterHistory(p);

        if (p->device_uri && strncmp(p->device_uri, "file:", 5) &&
	    p->state != IPP_PRINTER_STOPPED)
	{
	 /*
          * See if the backend exists...
	  */

	  snprintf(line, sizeof(line), "%s/backend/%s", ServerBin,
	           p->device_uri);

          if ((valueptr = strchr(line + strlen(ServerBin), ':')) != NULL)
	    *valueptr = '\0';		/* Chop everything but URI scheme */

          if (access(line, 0))
	  {
	   /*
	    * Backend does not exist, stop printer...
	    */

	    p->state = IPP_PRINTER_STOPPED;
	    snprintf(p->state_message, sizeof(p->state_message),
	             "Backend %s does not exist!", line);
	  }
        }

        p = NULL;
      }
      else
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of printers.conf.", linenum);
        return;
      }
    }
    else if (!p)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Syntax error on line %d of printers.conf.", linenum);
      return;
    }
    else if (!strcasecmp(line, "Info"))
    {
      if (value)
	cupsdSetString(&p->info, value);
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
	                "Syntax error on line %d of printers.conf.", linenum);
	return;
      }
    }
    else if (!strcasecmp(line, "PortMonitor"))
    {
      if (value && strcmp(value, "none"))
	cupsdSetString(&p->port_monitor, value);
      else if (value)
        cupsdClearString(&p->port_monitor);
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of printers.conf.", linenum);
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
	                "Syntax error on line %d of printers.conf.", linenum);
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
	                "Syntax error on line %d of printers.conf.", linenum);
	return;
      }
    }
    else if (!strcasecmp(line, "Shared"))
    {
     /*
      * Set the initial shared state...
      */

      if (value &&
          (!strcasecmp(value, "yes") ||
           !strcasecmp(value, "on") ||
           !strcasecmp(value, "true")))
        p->shared = 1;
      else if (value &&
               (!strcasecmp(value, "no") ||
        	!strcasecmp(value, "off") ||
        	!strcasecmp(value, "false")))
        p->shared = 0;
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of printers.conf.", linenum);
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
	                "Syntax error on line %d of printers.conf.", linenum);
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
	                "Syntax error on line %d of printers.conf.", linenum);
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
	                "Syntax error on line %d of printers.conf.", linenum);
	return;
      }
    }
    else if (!strcasecmp(line, "QuotaPeriod"))
    {
      if (value)
        p->quota_period = atoi(value);
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of printers.conf.", linenum);
	return;
      }
    }
    else if (!strcasecmp(line, "PageLimit"))
    {
      if (value)
        p->page_limit = atoi(value);
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of printers.conf.", linenum);
	return;
      }
    }
    else if (!strcasecmp(line, "KLimit"))
    {
      if (value)
        p->k_limit = atoi(value);
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of printers.conf.", linenum);
	return;
      }
    }
    else if (!strcasecmp(line, "OpPolicy"))
    {
      if (value)
        cupsdSetString(&p->op_policy, value);
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of printers.conf.", linenum);
	return;
      }
    }
    else if (!strcasecmp(line, "ErrorPolicy"))
    {
      if (value)
        cupsdSetString(&p->error_policy, value);
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of printers.conf.", linenum);
	return;
      }
    }
    else
    {
     /*
      * Something else we don't understand...
      */

      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unknown configuration directive %s on line %d of printers.conf.",
	              line, linenum);
    }
  }

  cupsFileClose(fp);
}


/*
 * 'cupsdSaveAllPrinters()' - Save all printer definitions to the printers.conf
 *                       file.
 */

void
cupsdSaveAllPrinters(void)
{
  int			i;		/* Looping var */
  cups_file_t		*fp;		/* printers.conf file */
  char			temp[1024];	/* Temporary string */
  char			backup[1024];	/* printers.conf.O file */
  cupsd_printer_t	*printer;	/* Current printer class */
  time_t		curtime;	/* Current time */
  struct tm		*curdate;	/* Current date */


 /*
  * Create the printers.conf file...
  */

  snprintf(temp, sizeof(temp), "%s/printers.conf", ServerRoot);
  snprintf(backup, sizeof(backup), "%s/printers.conf.O", ServerRoot);

  if (rename(temp, backup))
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to backup printers.conf - %s", strerror(errno));

  if ((fp = cupsFileOpen(temp, "w")) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to save printers.conf - %s", strerror(errno));

    if (rename(backup, temp))
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to restore printers.conf - %s", strerror(errno));
    return;
  }
  else
    cupsdLogMessage(CUPSD_LOG_INFO, "Saving printers.conf...");

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
  strftime(temp, sizeof(temp) - 1, "%c", curdate);

  cupsFilePuts(fp, "# Printer configuration file for " CUPS_SVERSION "\n");
  cupsFilePrintf(fp, "# Written by cupsd on %s\n", temp);

 /*
  * Write each local printer known to the system...
  */

  for (printer = (cupsd_printer_t *)cupsArrayFirst(Printers);
       printer;
       printer = (cupsd_printer_t *)cupsArrayNext(Printers))
  {
   /*
    * Skip remote destinations and printer classes...
    */

    if ((printer->type & CUPS_PRINTER_REMOTE) ||
        (printer->type & CUPS_PRINTER_CLASS) ||
	(printer->type & CUPS_PRINTER_IMPLICIT))
      continue;

   /*
    * Write printers as needed...
    */

    if (printer == DefaultPrinter)
      cupsFilePrintf(fp, "<DefaultPrinter %s>\n", printer->name);
    else
      cupsFilePrintf(fp, "<Printer %s>\n", printer->name);

    if (printer->info)
      cupsFilePrintf(fp, "Info %s\n", printer->info);

    if (printer->location)
      cupsFilePrintf(fp, "Location %s\n", printer->location);

    if (printer->device_uri)
      cupsFilePrintf(fp, "DeviceURI %s\n", printer->device_uri);

    if (printer->port_monitor)
      cupsFilePrintf(fp, "PortMonitor %s\n", printer->port_monitor);

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

    if (printer->shared)
      cupsFilePuts(fp, "Shared Yes\n");
    else
      cupsFilePuts(fp, "Shared No\n");

    cupsFilePrintf(fp, "JobSheets %s %s\n", printer->job_sheets[0],
            printer->job_sheets[1]);

    cupsFilePrintf(fp, "QuotaPeriod %d\n", printer->quota_period);
    cupsFilePrintf(fp, "PageLimit %d\n", printer->page_limit);
    cupsFilePrintf(fp, "KLimit %d\n", printer->k_limit);

    for (i = 0; i < printer->num_users; i ++)
      cupsFilePrintf(fp, "%sUser %s\n", printer->deny_users ? "Deny" : "Allow",
              printer->users[i]);

    if (printer->op_policy)
      cupsFilePrintf(fp, "OpPolicy %s\n", printer->op_policy);
    if (printer->error_policy)
      cupsFilePrintf(fp, "ErrorPolicy %s\n", printer->error_policy);

    cupsFilePuts(fp, "</Printer>\n");

#ifdef __sgi
    /*
     * Make IRIX desktop & printer status happy
     */

    write_irix_state(printer);
#endif /* __sgi */
  }

  cupsFileClose(fp);
}


/*
 * 'cupsdSetPrinterAttrs()' - Set printer attributes based upon the PPD file.
 */

void
cupsdSetPrinterAttrs(cupsd_printer_t *p)/* I - Printer to setup */
{
  char			uri[HTTP_MAX_URI];
					/* URI for printer */
  char			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			i;		/* Looping var */
  char			filename[1024];	/* Name of PPD file */
  int			num_media;	/* Number of media options */
  cupsd_location_t	*auth;		/* Pointer to authentication element */
  const char		*auth_supported;/* Authentication supported */
  cups_ptype_t		cupsd_printer_type;
					/* Printer type data */
  ppd_file_t		*ppd;		/* PPD file data */
  ppd_option_t		*input_slot,	/* InputSlot options */
			*media_type,	/* MediaType options */
			*page_size,	/* PageSize options */
			*output_bin,	/* OutputBin options */
			*media_quality;	/* EFMediaQualityMode options */
  ppd_attr_t		*ppdattr;	/* PPD attribute */
  ipp_attribute_t	*attr;		/* Attribute data */
  ipp_value_t		*val;		/* Attribute value */
  int			num_finishings;	/* Number of finishings */
  ipp_finish_t		finishings[5];	/* finishings-supported values */
  static const char * const sides[3] =	/* sides-supported values */
		{
		  "one",
		  "two-long-edge",
		  "two-short-edge"
		};


  DEBUG_printf(("cupsdSetPrinterAttrs: entering name = %s, type = %x\n", p->name,
                p->type));

 /*
  * Make sure that we have the common attributes defined...
  */

  if (!CommonData)
    cupsdCreateCommonData();

 /*
  * Clear out old filters, if any...
  */

  cupsdDeletePrinterFilters(p);

 /*
  * Figure out the authentication that is required for the printer.
  */

  auth_supported = "requesting-user-name";
  if (!(p->type & CUPS_PRINTER_REMOTE))
  {
    if (p->type & CUPS_PRINTER_CLASS)
      snprintf(resource, sizeof(resource), "/classes/%s", p->name);
    else
      snprintf(resource, sizeof(resource), "/printers/%s", p->name);

    if ((auth = cupsdFindBest(resource, HTTP_POST)) == NULL)
      auth = cupsdFindPolicyOp(p->op_policy_ptr, IPP_PRINT_JOB);

    if (auth)
    {
      if (auth->type == AUTH_BASIC || auth->type == AUTH_BASICDIGEST)
	auth_supported = "basic";
      else if (auth->type == AUTH_DIGEST)
	auth_supported = "digest";

      if (auth->type != AUTH_NONE)
        p->type |= CUPS_PRINTER_AUTHENTICATED;
      else
        p->type &= ~CUPS_PRINTER_AUTHENTICATED;
    }
    else
      p->type &= ~CUPS_PRINTER_AUTHENTICATED;
  }

 /*
  * Create the required IPP attributes for a printer...
  */

  if (p->attrs)
    ippDelete(p->attrs);

  p->attrs = ippNew();

  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
               "uri-authentication-supported", NULL, auth_supported);
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
               "uri-security-supported", NULL, "none");
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-name", NULL,
               p->name);
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-location",
               NULL, p->location ? p->location : "");
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info",
               NULL, p->info ? p->info : "");
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-more-info",
               NULL, p->uri);

  if (p->num_users)
  {
    if (p->deny_users)
      ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME,
                    "requesting-user-name-denied", p->num_users, NULL,
		    p->users);
    else
      ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME,
                    "requesting-user-name-allowed", p->num_users, NULL,
		    p->users);
  }

  ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "job-quota-period", p->quota_period);
  ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "job-k-limit", p->k_limit);
  ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "job-page-limit", p->page_limit);

  if (NumBanners > 0 && !(p->type & CUPS_PRINTER_REMOTE))
  {
   /*
    * Setup the job-sheets-default attribute...
    */

    attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME,
                	 "job-sheets-default", 2, NULL, NULL);

    if (attr != NULL)
    {
      attr->values[0].string.text = strdup(Classification ?
	                                   Classification : p->job_sheets[0]);
      attr->values[1].string.text = strdup(Classification ?
	                                   Classification : p->job_sheets[1]);
    }
  }

  cupsd_printer_type = p->type;

  p->raw = 0;

  if (p->type & CUPS_PRINTER_REMOTE)
  {
   /*
    * Tell the client this is a remote printer of some type...
    */

    ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_URI,
	         "printer-uri-supported", NULL, p->uri);

    ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                 "printer-make-and-model", NULL, p->make_model);
    p->raw = 1;
  }
  else
  {
   /*
    * Assign additional attributes depending on whether this is a printer
    * or class...
    */

    p->type &= ~CUPS_PRINTER_OPTIONS;

    if (p->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT))
    {
      p->raw = 1;

     /*
      * Add class-specific attributes...
      */

      if ((p->type & CUPS_PRINTER_IMPLICIT) && p->num_printers > 0)
	ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                     "printer-make-and-model", NULL, p->printers[0]->make_model);
      else
	ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                     "printer-make-and-model", NULL, "Local Printer Class");

      if (p->num_printers > 0)
      {
       /*
	* Add a list of member URIs and names...
	*/

	attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_URI,
                             "member-uris", p->num_printers, NULL, NULL);
        p->type |= CUPS_PRINTER_OPTIONS;

	for (i = 0; i < p->num_printers; i ++)
	{
          if (attr != NULL)
            attr->values[i].string.text = strdup(p->printers[i]->uri);

	  p->type &= ~CUPS_PRINTER_OPTIONS | p->printers[i]->type;
        }

	attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME,
                             "member-names", p->num_printers, NULL, NULL);

	if (attr != NULL)
	{
	  for (i = 0; i < p->num_printers; i ++)
            attr->values[i].string.text = strdup(p->printers[i]->name);
        }
      }
    }
    else
    {
     /*
      * Add printer-specific attributes...  Start by sanitizing the device
      * URI so it doesn't have a username or password in it...
      */

      if (!p->device_uri)
        strcpy(uri, "file:/dev/null");
      else if (strstr(p->device_uri, "://") != NULL)
      {
       /*
        * http://..., ipp://..., etc.
	*/

        cupsdSanitizeURI(p->device_uri, uri, sizeof(uri));
      }
      else
      {
       /*
        * file:..., serial:..., etc.
	*/

        strlcpy(uri, p->device_uri, sizeof(uri));
      }

      ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uri", NULL,
        	   uri);

     /*
      * Assign additional attributes from the PPD file (if any)...
      */

      p->type        |= CUPS_PRINTER_BW;
      finishings[0]  = IPP_FINISHINGS_NONE;
      num_finishings = 1;

      snprintf(filename, sizeof(filename), "%s/ppd/%s.ppd", ServerRoot,
               p->name);

      if ((ppd = ppdOpenFile(filename)) != NULL)
      {
       /*
	* Add make/model and other various attributes...
	*/

	if (ppd->color_device)
	  p->type |= CUPS_PRINTER_COLOR;
	if (ppd->variable_sizes)
	  p->type |= CUPS_PRINTER_VARIABLE;
	if (!ppd->manual_copies)
	  p->type |= CUPS_PRINTER_COPIES;
        if ((ppdattr = ppdFindAttr(ppd, "cupsFax", NULL)) != NULL)
	  if (ppdattr->value && !strcasecmp(ppdattr->value, "true"))
	    p->type |= CUPS_PRINTER_FAX;

	ippAddBoolean(p->attrs, IPP_TAG_PRINTER, "color-supported",
                      ppd->color_device);
	if (ppd->throughput)
	  ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
	                "pages-per-minute", ppd->throughput);

        if (ppd->nickname)
          cupsdSetString(&p->make_model, ppd->nickname);
	else if (ppd->modelname)
          cupsdSetString(&p->make_model, ppd->modelname);
	else
	  cupsdSetString(&p->make_model, "Bad PPD File");

	ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                     "printer-make-and-model", NULL, p->make_model);

       /*
	* Add media options from the PPD file...
	*/

	if ((input_slot = ppdFindOption(ppd, "InputSlot")) != NULL)
	  num_media = input_slot->num_choices;
	else
	  num_media = 0;

	if ((media_type = ppdFindOption(ppd, "MediaType")) != NULL)
	  num_media += media_type->num_choices;

	if ((page_size = ppdFindOption(ppd, "PageSize")) != NULL)
	  num_media += page_size->num_choices;

	if ((media_quality = ppdFindOption(ppd, "EFMediaQualityMode")) != NULL)
	  num_media += media_quality->num_choices;

        if (num_media == 0)
	{
	  cupsdLogMessage(CUPSD_LOG_CRIT, "cupsdSetPrinterAttrs: The PPD file for printer %s "
	                     "contains no media options and is therefore "
			     "invalid!", p->name);
	}
	else
	{
	  attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                               "media-supported", num_media, NULL, NULL);
          if (attr != NULL)
	  {
	    val = attr->values;

	    if (input_slot != NULL)
	      for (i = 0; i < input_slot->num_choices; i ++, val ++)
		val->string.text = strdup(input_slot->choices[i].choice);

	    if (media_type != NULL)
	      for (i = 0; i < media_type->num_choices; i ++, val ++)
		val->string.text = strdup(media_type->choices[i].choice);

	    if (media_quality != NULL)
	      for (i = 0; i < media_quality->num_choices; i ++, val ++)
		val->string.text = strdup(media_quality->choices[i].choice);

	    if (page_size != NULL)
	    {
	      for (i = 0; i < page_size->num_choices; i ++, val ++)
		val->string.text = strdup(page_size->choices[i].choice);

	      ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-default",
                	   NULL, page_size->defchoice);
            }
	    else if (input_slot != NULL)
	      ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-default",
                	   NULL, input_slot->defchoice);
	    else if (media_type != NULL)
	      ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-default",
                	   NULL, media_type->defchoice);
	    else if (media_quality != NULL)
	      ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-default",
                	   NULL, media_quality->defchoice);
	    else
	      ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-default",
                	   NULL, "none");
          }
        }

       /*
        * Output bin...
	*/

	if ((output_bin = ppdFindOption(ppd, "OutputBin")) != NULL)
	{
	  attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                               "output-bin-supported", output_bin->num_choices,
			       NULL, NULL);

          if (attr != NULL)
	  {
	    for (i = 0, val = attr->values;
		 i < output_bin->num_choices;
		 i ++, val ++)
	      val->string.text = strdup(output_bin->choices[i].choice);
          }
	}

       /*
        * Duplexing, etc...
	*/

	if (ppdFindOption(ppd, "Duplex") != NULL)
	{
	  p->type |= CUPS_PRINTER_DUPLEX;

	  ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "sides-supported",
                	3, NULL, sides);
	  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "sides-default",
                       NULL, "one");
	}

	if (ppdFindOption(ppd, "Collate") != NULL)
	  p->type |= CUPS_PRINTER_COLLATE;

	if (ppdFindOption(ppd, "StapleLocation") != NULL)
	{
	  p->type |= CUPS_PRINTER_STAPLE;
	  finishings[num_finishings++] = IPP_FINISHINGS_STAPLE;
	}

	if (ppdFindOption(ppd, "BindEdge") != NULL)
	{
	  p->type |= CUPS_PRINTER_BIND;
	  finishings[num_finishings++] = IPP_FINISHINGS_BIND;
	}

	for (i = 0; i < ppd->num_sizes; i ++)
	  if (ppd->sizes[i].length > 1728)
            p->type |= CUPS_PRINTER_LARGE;
	  else if (ppd->sizes[i].length > 1008)
            p->type |= CUPS_PRINTER_MEDIUM;
	  else
            p->type |= CUPS_PRINTER_SMALL;

       /*
	* Add a filter from application/vnd.cups-raw to printer/name to
	* handle "raw" printing by users.
	*/

        cupsdAddPrinterFilter(p, "application/vnd.cups-raw 0 -");

       /*
	* Add any filters in the PPD file...
	*/

	DEBUG_printf(("ppd->num_filters = %d\n", ppd->num_filters));
	for (i = 0; i < ppd->num_filters; i ++)
	{
          DEBUG_printf(("ppd->filters[%d] = \"%s\"\n", i, ppd->filters[i]));
          cupsdAddPrinterFilter(p, ppd->filters[i]);
	}

	if (ppd->num_filters == 0)
	{
	 /*
	  * If there are no filters, add a PostScript printing filter.
	  */

          cupsdAddPrinterFilter(p, "application/vnd.cups-postscript 0 -");
        }

       /*
	* Show current and available port monitors for this printer...
	*/

	ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "port-monitor",
                     NULL, p->port_monitor ? p->port_monitor : "none");


        for (i = 1, ppdattr = ppdFindAttr(ppd, "cupsPortMonitor", NULL);
	     ppdattr;
	     i ++, ppdattr = ppdFindNextAttr(ppd, "cupsPortMonitor", NULL));

        if (ppd->protocols)
	{
	  if (strstr(ppd->protocols, "TBCP"))
	    i ++;
	  else if (strstr(ppd->protocols, "BCP"))
	    i ++;
	}

        attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
	                     "port-monitor-supported", i, NULL, NULL);

        attr->values[0].string.text = strdup("none");

        for (i = 1, ppdattr = ppdFindAttr(ppd, "cupsPortMonitor", NULL);
	     ppdattr;
	     i ++, ppdattr = ppdFindNextAttr(ppd, "cupsPortMonitor", NULL))
	  attr->values[i].string.text = strdup(ppdattr->value);

        if (ppd->protocols)
	{
	  if (strstr(ppd->protocols, "TBCP"))
	    attr->values[i].string.text = strdup("tbcp");
	  else if (strstr(ppd->protocols, "BCP"))
	    attr->values[i].string.text = strdup("bcp");
	}

       /*
        * Close the PPD and set the type...
	*/

	ppdClose(ppd);

        cupsd_printer_type = p->type;
      }
      else if (!access(filename, 0))
      {
        int		pline;			/* PPD line number */
	ppd_status_t	pstatus;		/* PPD load status */


        pstatus = ppdLastError(&pline);

	cupsdLogMessage(CUPSD_LOG_ERROR, "PPD file for %s cannot be loaded!", p->name);

	if (pstatus <= PPD_ALLOC_ERROR)
	  cupsdLogMessage(CUPSD_LOG_ERROR, "%s", strerror(errno));
        else
	  cupsdLogMessage(CUPSD_LOG_ERROR, "%s on line %d.", ppdErrorString(pstatus),
	             pline);

        cupsdLogMessage(CUPSD_LOG_INFO, "Hint: Run \"cupstestppd %s\" and fix any errors.",
	           filename);

       /*
	* Add a filter from application/vnd.cups-raw to printer/name to
	* handle "raw" printing by users.
	*/

        cupsdAddPrinterFilter(p, "application/vnd.cups-raw 0 -");

       /*
        * Add a PostScript filter, since this is still possibly PS printer.
	*/

	cupsdAddPrinterFilter(p, "application/vnd.cups-postscript 0 -");
      }
      else
      {
       /*
	* If we have an interface script, add a filter entry for it...
	*/

	snprintf(filename, sizeof(filename), "%s/interfaces/%s", ServerRoot,
	         p->name);
	if (access(filename, X_OK) == 0)
	{
	 /*
	  * Yes, we have a System V style interface script; use it!
	  */

	  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                       "printer-make-and-model", NULL, "Local System V Printer");

	  snprintf(filename, sizeof(filename), "*/* 0 %s/interfaces/%s",
	           ServerRoot, p->name);
	  cupsdAddPrinterFilter(p, filename);
	}
	else if (p->device_uri &&
	         !strncmp(p->device_uri, "ipp://", 6) &&
	         (strstr(p->device_uri, "/printers/") != NULL ||
		  strstr(p->device_uri, "/classes/") != NULL))
        {
	 /*
	  * Tell the client this is really a hard-wired remote printer.
	  */

          cupsd_printer_type |= CUPS_PRINTER_REMOTE;

         /*
	  * Point the printer-uri-supported attribute to the
	  * remote printer...
	  */

	  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_URI,
	               "printer-uri-supported", NULL, p->device_uri);

         /*
	  * Then set the make-and-model accordingly...
	  */

	  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                       "printer-make-and-model", NULL, "Remote Printer");

         /*
	  * Print all files directly...
	  */

	  p->raw = 1;
	}
	else
	{
	 /*
          * Otherwise we have neither - treat this as a "dumb" printer
	  * with no PPD file...
	  */

	  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                       "printer-make-and-model", NULL, "Local Raw Printer");

	  p->raw = 1;
	}
      }

      ippAddIntegers(p->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                     "finishings-supported", num_finishings, (int *)finishings);
      ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                    "finishings-default", IPP_FINISHINGS_NONE);
    }
  }

 /*
  * Add the CUPS-specific printer-type attribute...
  */

  if (!p->shared)
    p->type |= CUPS_PRINTER_NOT_SHARED;
  else
    p->type &= ~CUPS_PRINTER_NOT_SHARED;

  ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-type",
                cupsd_printer_type);

  DEBUG_printf(("cupsdSetPrinterAttrs: leaving name = %s, type = %x\n", p->name,
                p->type));

#ifdef __sgi
 /*
  * Write the IRIX printer config and status files...
  */

  write_irix_config(p);
  write_irix_state(p);
#endif /* __sgi */
}


/*
 * 'cupsdSetPrinterReasons()' - Set/update the reasons strings.
 */

void
cupsdSetPrinterReasons(
    cupsd_printer_t  *p,		/* I - Printer */
    const char *s)			/* I - Reasons strings */
{
  int		i;			/* Looping var */
  const char	*sptr;			/* Pointer into reasons */
  char		reason[255],		/* Reason string */
		*rptr;			/* Pointer into reason */


  if (s[0] == '-' || s[0] == '+')
  {
   /*
    * Add/remove reasons...
    */

    sptr = s + 1;
  }
  else
  {
   /*
    * Replace reasons...
    */

    sptr = s;

    for (i = 0; i < p->num_reasons; i ++)
      free(p->reasons[i]);

    p->num_reasons = 0;
  }

 /*
  * Loop through all of the reasons...
  */

  while (*sptr)
  {
   /*
    * Skip leading whitespace and commas...
    */

    while (isspace(*sptr & 255) || *sptr == ',')
      sptr ++;

    for (rptr = reason; *sptr && !isspace(*sptr & 255) && *sptr != ','; sptr ++)
      if (rptr < (reason + sizeof(reason) - 1))
        *rptr++ = *sptr;

    if (rptr == reason)
      break;

    *rptr = '\0';

    if (s[0] == '-')
    {
     /*
      * Remove reason...
      */

      for (i = 0; i < p->num_reasons; i ++)
        if (!strcasecmp(reason, p->reasons[i]))
	{
	 /*
	  * Found a match, so remove it...
	  */

	  p->num_reasons --;
	  free(p->reasons[i]);

	  if (i < p->num_reasons)
	    memmove(p->reasons + i, p->reasons + i + 1,
	            (p->num_reasons - i) * sizeof(char *));

	  i --;
	}
    }
    else if (p->num_reasons < (int)(sizeof(p->reasons) / sizeof(p->reasons[0])))
    {
     /*
      * Add reason...
      */

      for (i = 0; i < p->num_reasons; i ++)
        if (!strcasecmp(reason, p->reasons[i]))
	  break;

      if (i >= p->num_reasons)
      {
        p->reasons[i] = strdup(reason);
	p->num_reasons ++;
      }
    }
  }
}


/*
 * 'cupsdSetPrinterState()' - Update the current state of a printer.
 */

void
cupsdSetPrinterState(
    cupsd_printer_t *p,			/* I - Printer to change */
    ipp_pstate_t    s,			/* I - New state */
    int             update)		/* I - Update printers.conf? */
{
  ipp_pstate_t	old_state;		/* Old printer state */


 /*
  * Can't set status of remote printers...
  */

  if (p->type & CUPS_PRINTER_REMOTE)
    return;

 /*
  * Set the new state...
  */

  old_state = p->state;
  p->state  = s;

  if (old_state != s)
  {
   /*
    * Let the browse code know this needs to be updated...
    */

    BrowseNext     = p;
    p->state_time  = time(NULL);
    p->browse_time = 0;

#ifdef __sgi
    write_irix_state(p);
#endif /* __sgi */
  }

  cupsdAddPrinterHistory(p);

 /*
  * Save the printer configuration if a printer goes from idle or processing
  * to stopped (or visa-versa)...
  */

  if ((old_state == IPP_PRINTER_STOPPED) != (s == IPP_PRINTER_STOPPED) &&
      update)
  {
    if (p->type & CUPS_PRINTER_CLASS)
      cupsdSaveAllClasses();
    else
      cupsdSaveAllPrinters();
  }
}


/*
 * 'cupsdStopPrinter()' - Stop a printer from printing any jobs...
 */

void
cupsdStopPrinter(cupsd_printer_t *p,	/* I - Printer to stop */
                 int             update)/* I - Update printers.conf? */
{
  cupsd_job_t	*job;			/* Active print job */


 /*
  * Set the printer state...
  */

  cupsdSetPrinterState(p, IPP_PRINTER_STOPPED, update);

 /*
  * See if we have a job printing on this printer...
  */

  if (p->job)
  {
   /*
    * Get pointer to job...
    */

    job = (cupsd_job_t *)p->job;

   /*
    * Stop it...
    */

    cupsdStopJob(job, 0);

   /*
    * Reset the state to pending...
    */

    job->state->values[0].integer = IPP_JOB_PENDING;

    cupsdSaveJob(job);
  }
}


/*
 * 'cupsdValidateDest()' - Validate a printer/class destination.
 */

const char *				/* O - Printer or class name */
cupsdValidateDest(
    const char      *hostname,		/* I - Host name */
    const char      *resource,		/* I - Resource name */
    cups_ptype_t    *dtype,		/* O - Type (printer or class) */
    cupsd_printer_t **printer)		/* O - Printer pointer */
{
  cupsd_printer_t	*p;		/* Current printer */
  char			localname[1024],/* Localized hostname */
			*lptr,		/* Pointer into localized hostname */
			*sptr;		/* Pointer into server name */


  DEBUG_printf(("cupsdValidateDest(\"%s\", \"%s\", %p, %p)\n", hostname, resource,
                dtype, printer));

 /*
  * Initialize return values...
  */

  if (printer)
    *printer = NULL;

  *dtype = (cups_ptype_t)0;

 /*
  * See if the resource is a class or printer...
  */

  if (!strncmp(resource, "/classes/", 9))
  {
   /*
    * Class...
    */

    resource += 9;
  }
  else if (!strncmp(resource, "/printers/", 10))
  {
   /*
    * Printer...
    */

    resource += 10;
  }
  else
  {
   /*
    * Bad resource name...
    */

    return (NULL);
  }

 /*
  * See if the printer or class name exists...
  */

  p = cupsdFindDest(resource);

  if (p == NULL && strchr(resource, '@') == NULL)
    return (NULL);
  else if (p != NULL)
  {
    if (printer)
      *printer = p;

    *dtype = p->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT |
                        CUPS_PRINTER_REMOTE);
    return (p->name);
  }

 /*
  * Change localhost to the server name...
  */

  if (!strcasecmp(hostname, "localhost"))
    hostname = ServerName;

  strlcpy(localname, hostname, sizeof(localname));

  if (!strcasecmp(hostname, ServerName))
  {
   /*
    * Localize the hostname...
    */

    lptr = strchr(localname, '.');
    sptr = strchr(ServerName, '.');

    if (sptr != NULL && lptr != NULL)
    {
     /*
      * Strip the common domain name components...
      */

      while (lptr != NULL)
      {
	if (!strcasecmp(lptr, sptr))
	{
          *lptr = '\0';
	  break;
	}
	else
          lptr = strchr(lptr + 1, '.');
      }
    }
  }

  DEBUG_printf(("localized hostname is \"%s\"...\n", localname));

 /*
  * Find a matching printer or class...
  */

  for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
       p;
       p = (cupsd_printer_t *)cupsArrayNext(Printers))
    if (!strcasecmp(p->hostname, localname) &&
        !strcasecmp(p->name, resource))
    {
      if (printer)
        *printer = p;

      *dtype = p->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT |
                          CUPS_PRINTER_REMOTE);
      return (p->name);
    }

  return (NULL);
}


/*
 * 'cupsdWritePrintcap()' - Write a pseudo-printcap file for older applications
 *                          that need it...
 */

void
cupsdWritePrintcap(void)
{
  cups_file_t		*fp;		/* printcap file */
  cupsd_printer_t	*p;		/* Current printer */


#ifdef __sgi
 /*
  * Update the IRIX printer state for the default printer; if
  * no printers remain, then the default printer file will be
  * removed...
  */

  write_irix_state(DefaultPrinter);
#endif /* __sgi */

 /*
  * See if we have a printcap file; if not, don't bother writing it.
  */

  if (!Printcap || !*Printcap)
    return;

 /*
  * Open the printcap file...
  */

  if ((fp = cupsFileOpen(Printcap, "w")) == NULL)
    return;

 /*
  * Put a comment header at the top so that users will know where the
  * data has come from...
  */

  cupsFilePuts(fp, "# This file was automatically generated by cupsd(8) from the\n");
  cupsFilePrintf(fp, "# %s/printers.conf file.  All changes to this file\n",
                 ServerRoot);
  cupsFilePuts(fp, "# will be lost.\n");

  if (Printers)
  {
   /*
    * Write a new printcap with the current list of printers.
    */

    switch (PrintcapFormat)
    {
      case PRINTCAP_BSD:
	 /*
          * Each printer is put in the file as:
	  *
	  *    Printer1:
	  *    Printer2:
	  *    Printer3:
	  *    ...
	  *    PrinterN:
	  */

          if (DefaultPrinter)
	    cupsFilePrintf(fp, "%s|%s:rm=%s:rp=%s:\n", DefaultPrinter->name,
	            DefaultPrinter->info, ServerName, DefaultPrinter->name);

	  for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
	       p;
	       p = (cupsd_printer_t *)cupsArrayNext(Printers))
	    if (p != DefaultPrinter)
	      cupsFilePrintf(fp, "%s|%s:rm=%s:rp=%s:\n", p->name, p->info,
	              ServerName, p->name);
          break;

      case PRINTCAP_SOLARIS:
	 /*
          * Each printer is put in the file as:
	  *
	  *    _all:all=Printer1,Printer2,Printer3,...,PrinterN
	  *    _default:use=DefaultPrinter
	  *    Printer1:\
	  *            :bsdaddr=ServerName,Printer1:\
	  *            :description=Description:
	  *    Printer2:
	  *            :bsdaddr=ServerName,Printer2:\
	  *            :description=Description:
	  *    Printer3:
	  *            :bsdaddr=ServerName,Printer3:\
	  *            :description=Description:
	  *    ...
	  *    PrinterN:
	  *            :bsdaddr=ServerName,PrinterN:\
	  *            :description=Description:
	  */

          cupsFilePuts(fp, "_all:all=");
	  for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
	       p;
	       p = (cupsd_printer_t *)cupsArrayCurrent(Printers))
	    cupsFilePrintf(fp, "%s%c", p->name,
	                   cupsArrayNext(Printers) ? ',' : '\n');

          if (DefaultPrinter)
	    cupsFilePrintf(fp, "_default:use=%s\n", DefaultPrinter->name);

	  for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
	       p;
	       p = (cupsd_printer_t *)cupsArrayNext(Printers))
	    cupsFilePrintf(fp, "%s:\\\n"
	        	"\t:bsdaddr=%s,%s:\\\n"
			"\t:description=%s:\n",
		    p->name, ServerName, p->name, p->info ? p->info : "");
          break;
    }
  }

 /*
  * Close the file...
  */

  cupsFileClose(fp);
}


/*
 * 'cupsdSanitizeURI()' - Sanitize a device URI...
 */

char *					/* O - New device URI */
cupsdSanitizeURI(const char *uri,	/* I - Original device URI */
                 char       *buffer,	/* O - New device URI */
                 int        buflen)	/* I - Size of new device URI buffer */
{
  char	*start,				/* Start of data after scheme */
	*slash,				/* First slash after scheme:// */
	*ptr;				/* Pointer into user@host:port part */


 /*
  * Range check input...
  */

  if (!uri || !buffer || buflen < 2)
    return (NULL);

 /*
  * Copy the device URI to the new buffer...
  */

  strlcpy(buffer, uri, buflen);

 /*
  * Find the end of the scheme:// part...
  */

  if ((ptr = strchr(buffer, ':')) == NULL)
    return (buffer);			/* No scheme: part... */

  for (start = ptr + 1; *start; start ++)
    if (*start != '/')
      break;

 /*
  * Find the next slash (/) in the URI...
  */

  if ((slash = strchr(start, '/')) == NULL)
    slash = start + strlen(start);	/* No slash, point to the end */

 /*
  * Check for an @ sign before the slash...
  */

  if ((ptr = strchr(start, '@')) != NULL && ptr < slash)
  {
   /*
    * Found an @ sign and it is before the resource part, so we have
    * an authentication string.  Copy the remaining URI over the
    * authentication string...
    */

    _cups_strcpy(start, ptr + 1);
  }

 /*
  * Return the new device URI...
  */

  return (buffer);
}


/*
 * 'compare_printers()' - Compare two printers.
 */

static int				/* O - Result of comparison */
compare_printers(void *first,		/* I - First printer */
                 void *second,		/* I - Second printer */
		 void *data)		/* I - App data (not used) */
{
  return (strcasecmp(((cupsd_printer_t *)first)->name,
                     ((cupsd_printer_t *)second)->name));
}


#ifdef __sgi
/*
 * 'write_irix_config()' - Update the config files used by the IRIX
 *                         desktop tools.
 */

static void
write_irix_config(cupsd_printer_t *p)	/* I - Printer to update */
{
  char		filename[1024];		/* Interface script filename */
  cups_file_t	*fp;			/* Interface script file */
  int		tag;			/* Status tag value */



 /*
  * Add dummy interface and GUI scripts to fool SGI's "challenged" printing
  * tools.  First the interface script that tells the tools what kind of
  * printer we have...
  */

  snprintf(filename, sizeof(filename), "/var/spool/lp/interface/%s", p->name);

  if (p->type & CUPS_PRINTER_CLASS)
    unlink(filename);
  else if ((fp = cupsFileOpen(filename, "w")) != NULL)
  {
    cupsFilePuts(fp, "#!/bin/sh\n");

    if ((attr = ippFindAttribute(p->attrs, "printer-make-and-model",
                                 IPP_TAG_TEXT)) != NULL)
      cupsFilePrintf(fp, "NAME=\"%s\"\n", attr->values[0].string.text);
    else if (p->type & CUPS_PRINTER_CLASS)
      cupsFilePuts(fp, "NAME=\"Printer Class\"\n");
    else
      cupsFilePuts(fp, "NAME=\"Remote Destination\"\n");

    if (p->type & CUPS_PRINTER_COLOR)
      cupsFilePuts(fp, "TYPE=ColorPostScript\n");
    else
      cupsFilePuts(fp, "TYPE=MonoPostScript\n");

    cupsFilePrintf(fp, "HOSTNAME=%s\n", ServerName);
    cupsFilePrintf(fp, "HOSTPRINTER=%s\n", p->name);

    cupsFileClose(fp);

    chmod(filename, 0755);
    chown(filename, User, Group);
  }

 /*
  * Then the member file that tells which device file the queue is connected
  * to...  Networked printers use "/dev/null" in this file, so that's what
  * we use (the actual device URI can confuse some apps...)
  */

  snprintf(filename, sizeof(filename), "/var/spool/lp/member/%s", p->name);

  if (p->type & CUPS_PRINTER_CLASS)
    unlink(filename);
  else if ((fp = cupsFileOpen(filename, "w")) != NULL)
  {
    cupsFilePuts(fp, "/dev/null\n");

    cupsFileClose(fp);

    chmod(filename, 0644);
    chown(filename, User, Group);
  }

 /*
  * The gui_interface file is a script or program that launches a GUI
  * option panel for the printer, using options specified on the
  * command-line in the third argument.  The option panel must send
  * any printing options to stdout on a single line when the user
  * accepts them, or nothing if the user cancels the dialog.
  *
  * The default options panel program is /usr/bin/glpoptions, from
  * the ESP Print Pro software.  You can select another using the
  * PrintcapGUI option.
  */

  snprintf(filename, sizeof(filename), "/var/spool/lp/gui_interface/ELF/%s.gui", p->name);

  if (p->type & CUPS_PRINTER_CLASS)
    unlink(filename);
  else if ((fp = cupsFileOpen(filename, "w")) != NULL)
  {
    cupsFilePuts(fp, "#!/bin/sh\n");
    cupsFilePrintf(fp, "%s -d %s -o \"$3\"\n", PrintcapGUI, p->name);

    cupsFileClose(fp);

    chmod(filename, 0755);
    chown(filename, User, Group);
  }

 /*
  * The POD config file is needed by the printstatus command to show
  * the printer location and device.
  */

  snprintf(filename, sizeof(filename), "/var/spool/lp/pod/%s.config", p->name);

  if (p->type & CUPS_PRINTER_CLASS)
    unlink(filename);
  else if ((fp = cupsFileOpen(filename, "w")) != NULL)
  {
    cupsFilePrintf(fp, "Printer Class      | %s\n",
            (p->type & CUPS_PRINTER_COLOR) ? "ColorPostScript" : "MonoPostScript");
    cupsFilePrintf(fp, "Printer Model      | %s\n", p->make_model ? p->make_model : "");
    cupsFilePrintf(fp, "Location Code      | %s\n", p->location ? p->location : "");
    cupsFilePrintf(fp, "Physical Location  | %s\n", p->info ? p->info : "");
    cupsFilePrintf(fp, "Port Path          | %s\n", p->device_uri ? p->device_uri : "");
    cupsFilePrintf(fp, "Config Path        | /var/spool/lp/pod/%s.config\n", p->name);
    cupsFilePrintf(fp, "Active Status Path | /var/spool/lp/pod/%s.status\n", p->name);
    cupsFilePuts(fp, "Status Update Wait | 10 seconds\n");

    cupsFileClose(fp);

    chmod(filename, 0664);
    chown(filename, User, Group);
  }
}


/*
 * 'write_irix_state()' - Update the status files used by IRIX printing
 *                        desktop tools.
 */

static void
write_irix_state(cupsd_printer_t *p)	/* I - Printer to update */
{
  char		filename[1024];		/* Interface script filename */
  cups_file_t	*fp;			/* Interface script file */
  int		tag;			/* Status tag value */


  if (p)
  {
   /*
    * The POD status file is needed for the printstatus window to
    * provide the current status of the printer.
    */

    snprintf(filename, sizeof(filename), "/var/spool/lp/pod/%s.status", p->name);

    if (p->type & CUPS_PRINTER_CLASS)
      unlink(filename);
    else if ((fp = cupsFileOpen(filename, "w")) != NULL)
    {
      cupsFilePrintf(fp, "Operational Status | %s\n",
              (p->state == IPP_PRINTER_IDLE)       ? "Idle" :
              (p->state == IPP_PRINTER_PROCESSING) ? "Busy" :
                                                     "Faulted");
      cupsFilePrintf(fp, "Information        | 01 00 00 | %s\n", CUPS_SVERSION);
      cupsFilePrintf(fp, "Information        | 02 00 00 | Device URI: %s\n",
              p->device_uri ? p->device_uri : "");
      cupsFilePrintf(fp, "Information        | 03 00 00 | %s jobs\n",
              p->accepting ? "Accepting" : "Not accepting");
      cupsFilePrintf(fp, "Information        | 04 00 00 | %s\n", p->state_message);

      cupsFileClose(fp);

      chmod(filename, 0664);
      chown(filename, User, Group);
    }

   /*
    * The activeicons file is needed to provide desktop icons for printers:
    *
    * [ quoted from /usr/lib/print/tagit ]
    *
    * --- Type of printer tags (base values)
    *
    * Dumb=66048			# 0x10200
    * DumbColor=66080		# 0x10220
    * Raster=66112		# 0x10240
    * ColorRaster=66144		# 0x10260
    * Plotter=66176		# 0x10280
    * PostScript=66208		# 0x102A0
    * ColorPostScript=66240	# 0x102C0
    * MonoPostScript=66272	# 0x102E0
    *
    * --- Printer state modifiers for local printers
    *
    * Idle=0			# 0x0
    * Busy=1			# 0x1
    * Faulted=2			# 0x2
    * Unknown=3			# 0x3 (Faulted due to unknown reason)
    *
    * --- Printer state modifiers for network printers
    *
    * NetIdle=8			# 0x8
    * NetBusy=9			# 0x9
    * NetFaulted=10		# 0xA
    * NetUnknown=11		# 0xB (Faulted due to unknown reason)
    */

    snprintf(filename, sizeof(filename), "/var/spool/lp/activeicons/%s", p->name);

    if (p->type & CUPS_PRINTER_CLASS)
      unlink(filename);
    else if ((fp = cupsFileOpen(filename, "w")) != NULL)
    {
      if (p->type & CUPS_PRINTER_COLOR)
	tag = 66240;
      else
	tag = 66272;

      if (p->type & CUPS_PRINTER_REMOTE)
	tag |= 8;

      if (p->state == IPP_PRINTER_PROCESSING)
	tag |= 1;

      else if (p->state == IPP_PRINTER_STOPPED)
	tag |= 2;

      cupsFilePuts(fp, "#!/bin/sh\n");
      cupsFilePrintf(fp, "#Tag %d\n", tag);

      cupsFileClose(fp);

      chmod(filename, 0755);
      chown(filename, User, Group);
    }
  }

 /*
  * The default file is needed by the printers window to show
  * the default printer.
  */

  snprintf(filename, sizeof(filename), "/var/spool/lp/default");

  if (DefaultPrinter != NULL)
  {
    if ((fp = cupsFileOpen(filename, "w")) != NULL)
    {
      cupsFilePrintf(fp, "%s\n", DefaultPrinter->name);

      cupsFileClose(fp);

      chmod(filename, 0644);
      chown(filename, User, Group);
    }
  }
  else
    unlink(filename);
}
#endif /* __sgi */


/*
 * End of "$Id$".
 */
