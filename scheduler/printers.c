/*
 * "$Id$"
 *
 *   Printer routines for the Common UNIX Printing System (CUPS).
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
 *   cupsdAddPrinter()           - Add a printer to the system.
 *   cupsdAddPrinterHistory()    - Add the current printer state to the history.
 *   cupsdAddPrinterUser()       - Add a user to the ACL.
 *   cupsdCreateCommonData()     - Create the common printer data.
 *   cupsdDeleteAllPrinters()    - Delete all printers from the system.
 *   cupsdDeletePrinter()        - Delete a printer from the system.
 *   cupsdFindPrinter()          - Find a printer in the list.
 *   cupsdFreePrinterUsers()     - Free allow/deny users.
 *   cupsdLoadAllPrinters()      - Load printers from the printers.conf file.
 *   cupsdRenamePrinter()        - Rename a printer.
 *   cupsdSaveAllPrinters()      - Save all printer definitions to the
 *                                 printers.conf file.
 *   cupsdSetAuthInfoRequired()  - Set the required authentication info.
 *   cupsdSetPrinterAttr()       - Set a printer attribute.
 *   cupsdSetPrinterAttrs()      - Set printer attributes based upon the PPD
 *                                 file.
 *   cupsdSetPrinterReasons()    - Set/update the reasons strings.
 *   cupsdSetPrinterState()      - Update the current state of a printer.
 *   cupsdStopPrinter()          - Stop a printer from printing any jobs...
 *   cupsdUpdatePrinters()       - Update printers after a partial reload.
 *   cupsdValidateDest()         - Validate a printer/class destination.
 *   cupsdWritePrintcap()        - Write a pseudo-printcap file for older
 *                                 applications that need it...
 *   cupsdSanitizeURI()          - Sanitize a device URI...
 *   add_printer_defaults()      - Add name-default attributes to the printer
 *                                 attributes.
 *   add_printer_filter()        - Add a MIME filter for a printer.
 *   add_printer_formats()       - Add document-format-supported values for
 *                                 a printer.
 *   compare_printers()          - Compare two printers.
 *   delete_printer_filters()    - Delete all MIME filters for a printer.
 *   write_irix_config()         - Update the config files used by the IRIX
 *                                 desktop tools.
 *   write_irix_state()          - Update the status files used by IRIX
 *                                 printing desktop tools.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <cups/dir.h>


/*
 * Local functions...
 */

static void	add_printer_defaults(cupsd_printer_t *p);
static void	add_printer_filter(cupsd_printer_t *p, mime_type_t *type,
				   const char *filter);
static void	add_printer_formats(cupsd_printer_t *p);
static int	compare_printers(void *first, void *second, void *data);
static void	delete_printer_filters(cupsd_printer_t *p);
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

  p->state      = IPP_PRINTER_STOPPED;
  p->state_time = time(NULL);
  p->accepting  = 0;
  p->shared     = DefaultShared;
  p->filetype   = mimeAddType(MimeDatabase, "printer", name);

  cupsdSetString(&p->job_sheets[0], "none");
  cupsdSetString(&p->job_sheets[1], "none");

  cupsdSetString(&p->error_policy, ErrorPolicy);
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
  * Return the new printer...
  */

  return (p);
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
  ippAddBoolean(history, IPP_TAG_PRINTER, "printer-is-shared", p->shared);
  ippAddString(history, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-state-message",
               NULL, p->state_message);
#ifdef __APPLE__
  if (p->recoverable)
    ippAddString(history, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                 "com.apple.print.recoverable-message", NULL, p->recoverable);
#endif /* __APPLE__ */
  if (p->num_reasons == 0)
    ippAddString(history, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                 "printer-state-reasons", NULL,
		 p->state == IPP_PRINTER_STOPPED ? "paused" : "none");
  else
    ippAddStrings(history, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                  "printer-state-reasons", p->num_reasons, NULL,
		  (const char * const *)p->reasons);
  ippAddInteger(history, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "printer-state-change-time", p->state_time);
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
  cups_dir_t		*dir;		/* Notifier directory */
  cups_dentry_t		*dent;		/* Notifier directory entry */
  cups_array_t		*notifiers;	/* Notifier array */
  char			filename[1024],	/* Filename */
			*notifier;	/* Current notifier */
  cupsd_policy_t	*p;		/* Current policy */
  static const int nups[] =		/* number-up-supported values */
		{ 1, 2, 4, 6, 9, 16 };
  static const int orients[4] =/* orientation-requested-supported values */
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
  static const int	ops[] =		/* operations-supported values */
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
		  CUPS_GET_PPD,
		  CUPS_GET_DOCUMENT,
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
  static const char * const notify_attrs[] =
		{			/* notify-attributes-supported values */
		  "printer-state-change-time",
		  "notify-lease-expiration-time",
		  "notify-subscriber-user-name"
		};
  static const char * const notify_events[] =
		{			/* notify-events-supported values */
        	  "job-completed",
        	  "job-config-changed",
        	  "job-created",
        	  "job-progress",
        	  "job-state-changed",
        	  "job-stopped",
        	  "printer-added",
        	  "printer-changed",
        	  "printer-config-changed",
        	  "printer-deleted",
        	  "printer-finishings-changed",
        	  "printer-media-changed",
        	  "printer-modified",
        	  "printer-restarted",
        	  "printer-shutdown",
        	  "printer-state-changed",
        	  "printer-stopped",
        	  "server-audit",
        	  "server-restarted",
        	  "server-started",
        	  "server-stopped"
		};


  if (CommonData)
    ippDelete(CommonData);

  CommonData = ippNew();

 /*
  * This list of attributes is sorted to improve performance when the
  * client provides a requested-attributes attribute...
  */

  /* charset-configured */
  ippAddString(CommonData, IPP_TAG_PRINTER, IPP_TAG_CHARSET,
               "charset-configured", NULL, DefaultCharset);

  /* charset-supported */
  ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_CHARSET,
                "charset-supported", sizeof(charsets) / sizeof(charsets[0]),
		NULL, charsets);

  /* compression-supported */
  ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
        	"compression-supported",
		sizeof(compressions) / sizeof(compressions[0]),
		NULL, compressions);

  /* copies-supported */
  ippAddRange(CommonData, IPP_TAG_PRINTER, "copies-supported", 1, MaxCopies);

  /* cups-version */
  ippAddString(CommonData, IPP_TAG_PRINTER, IPP_TAG_TEXT, "cups-version",
               NULL, CUPS_SVERSION + 6);

  /* generated-natural-language-supported */
  ippAddString(CommonData, IPP_TAG_PRINTER, IPP_TAG_LANGUAGE,
               "generated-natural-language-supported", NULL, DefaultLanguage);

  /* ipp-versions-supported */
  ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                "ipp-versions-supported", sizeof(versions) / sizeof(versions[0]),
		NULL, versions);

  /* job-hold-until-supported */
  ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                "job-hold-until-supported", sizeof(holds) / sizeof(holds[0]),
		NULL, holds);

  /* job-priority-supported */
  ippAddInteger(CommonData, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "job-priority-supported", 100);

  /* job-sheets-supported */
  if (cupsArrayCount(Banners) > 0)
  {
   /*
    * Setup the job-sheets-supported attribute...
    */

    if (Classification && !ClassifyOverride)
      attr = ippAddString(CommonData, IPP_TAG_PRINTER, IPP_TAG_NAME,
                	  "job-sheets-supported", NULL, Classification);
    else
      attr = ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_NAME,
                	   "job-sheets-supported", cupsArrayCount(Banners) + 1,
			   NULL, NULL);

    if (attr == NULL)
      cupsdLogMessage(CUPSD_LOG_EMERG,
                      "Unable to allocate memory for "
                      "job-sheets-supported attribute: %s!", strerror(errno));
    else if (!Classification || ClassifyOverride)
    {
      cupsd_banner_t	*banner;	/* Current banner */


      attr->values[0].string.text = _cupsStrAlloc("none");

      for (i = 1, banner = (cupsd_banner_t *)cupsArrayFirst(Banners);
	   banner;
	   i ++, banner = (cupsd_banner_t *)cupsArrayNext(Banners))
	attr->values[i].string.text = _cupsStrAlloc(banner->name);
    }
  }
  else
    ippAddString(CommonData, IPP_TAG_PRINTER, IPP_TAG_NAME,
                 "job-sheets-supported", NULL, "none");

  /* multiple-document-handling-supported */
  ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                "multiple-document-handling-supported",
                sizeof(multiple_document_handling) /
		    sizeof(multiple_document_handling[0]), NULL,
	        multiple_document_handling);

  /* multiple-document-jobs-supported */
  ippAddBoolean(CommonData, IPP_TAG_PRINTER,
                "multiple-document-jobs-supported", 1);

  /* multiple-operation-time-out */
  ippAddInteger(CommonData, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "multiple-operation-time-out", 60);

  /* natural-language-configured */
  ippAddString(CommonData, IPP_TAG_PRINTER, IPP_TAG_LANGUAGE,
               "natural-language-configured", NULL, DefaultLanguage);

  /* notify-attributes-supported */
  ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                "notify-attributes-supported",
		(int)(sizeof(notify_attrs) / sizeof(notify_attrs[0])),
		NULL, notify_attrs);

  /* notify-lease-duration-supported */
  ippAddRange(CommonData, IPP_TAG_PRINTER,
              "notify-lease-duration-supported", 0,
	      MaxLeaseDuration ? MaxLeaseDuration : 2147483647);

  /* notify-max-events-supported */
  ippAddInteger(CommonData, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
               "notify-max-events-supported", MaxEvents);

  /* notify-events-supported */
  ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                "notify-events-supported",
		(int)(sizeof(notify_events) / sizeof(notify_events[0])),
		NULL, notify_events);

  /* notify-pull-method-supported */
  ippAddString(CommonData, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
               "notify-pull-method-supported", NULL, "ippget");

  /* notify-schemes-supported */
  snprintf(filename, sizeof(filename), "%s/notifier", ServerBin);
  if ((dir = cupsDirOpen(filename)) != NULL)
  {
    notifiers = cupsArrayNew((cups_array_func_t)strcmp, NULL);

    while ((dent = cupsDirRead(dir)) != NULL)
      if (S_ISREG(dent->fileinfo.st_mode) &&
          (dent->fileinfo.st_mode & S_IXOTH) != 0)
        cupsArrayAdd(notifiers, _cupsStrAlloc(dent->filename));

    if (cupsArrayCount(notifiers) > 0)
    {
      attr = ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
        	           "notify-schemes-supported",
			   cupsArrayCount(notifiers), NULL, NULL);

      for (i = 0, notifier = (char *)cupsArrayFirst(notifiers);
           notifier;
	   i ++, notifier = (char *)cupsArrayNext(notifiers))
	attr->values[i].string.text = notifier;
    }

    cupsArrayDelete(notifiers);
    cupsDirClose(dir);
  }

  /* number-up-supported */
  ippAddIntegers(CommonData, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                 "number-up-supported", sizeof(nups) / sizeof(nups[0]), nups);

  /* operations-supported */
  ippAddIntegers(CommonData, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                 "operations-supported",
                 sizeof(ops) / sizeof(ops[0]) + JobFiles - 1, ops);

  /* orientation-requested-supported */
  ippAddIntegers(CommonData, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                 "orientation-requested-supported", 4, orients);

  /* page-ranges-supported */
  ippAddBoolean(CommonData, IPP_TAG_PRINTER, "page-ranges-supported", 1);

  /* pdf-override-supported */
  ippAddString(CommonData, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
               "pdl-override-supported", NULL, "not-attempted");

  /* printer-error-policy-supported */
  ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_NAME,
                "printer-error-policy-supported",
		sizeof(errors) / sizeof(errors[0]), NULL, errors);

  /* printer-op-policy-supported */
  attr = ippAddStrings(CommonData, IPP_TAG_PRINTER, IPP_TAG_NAME,
                       "printer-op-policy-supported", cupsArrayCount(Policies),
		       NULL, NULL);
  for (i = 0, p = (cupsd_policy_t *)cupsArrayFirst(Policies);
       p;
       i ++, p = (cupsd_policy_t *)cupsArrayNext(Policies))
    attr->values[i].string.text = _cupsStrAlloc(p->name);

  ippAddBoolean(CommonData, IPP_TAG_PRINTER, "server-is-sharing-printers",
                BrowseLocalProtocols != 0 && Browsing);
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


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdDeletePrinter(p=%p(%s), update=%d)",
                  p, p->name, update);

 /*
  * Save the current position in the Printers array...
  */

  cupsArraySave(Printers);

 /*
  * Stop printing on this printer...
  */

  cupsdStopPrinter(p, update);

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

  if (p->type & CUPS_PRINTER_IMPLICIT)
    cupsArrayRemove(ImplicitPrinters, p);

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
  * If p is the default printer, assign a different one...
  */

  if (p == DefaultPrinter)
  {
    DefaultPrinter = NULL;

    if (UseNetworkDefault)
    {
     /*
      * Find the first network default printer and use it...
      */

      cupsd_printer_t	*dp;		/* New default printer */


      for (dp = (cupsd_printer_t *)cupsArrayFirst(Printers);
	   dp;
	   dp = (cupsd_printer_t *)cupsArrayNext(Printers))
	if (dp != p && (dp->type & CUPS_PRINTER_DEFAULT))
	{
	  DefaultPrinter = dp;
	  break;
	}
    }
  }

 /*
  * Remove this printer from any classes...
  */

  if (!(p->type & CUPS_PRINTER_IMPLICIT))
  {
    cupsdDeletePrinterFromClasses(p);

   /*
    * Deregister from any browse protocols...
    */

    cupsdDeregisterPrinter(p, 1);
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

  delete_printer_filters(p);

  mimeDeleteType(MimeDatabase, p->filetype);
  mimeDeleteType(MimeDatabase, p->prefiltertype);

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

  cupsdClearString(&p->alert);
  cupsdClearString(&p->alert_description);

#ifdef HAVE_DNSSD
  cupsdClearString(&p->product);
  cupsdClearString(&p->pdl);
#endif /* HAVE_DNSSD */

  cupsArrayDelete(p->filetypes);

  if (p->browse_attrs)
    free(p->browse_attrs);

#ifdef __APPLE__
  cupsdClearString(&p->recoverable);
#endif /* __APPLE__ */

  cupsFreeOptions(p->num_options, p->options);

  free(p);

 /*
  * Restore the previous position in the Printers array...
  */

  cupsArrayRestore(Printers);
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
    if (errno != ENOENT)
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to open %s - %s", line,
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

        cupsdLogMessage(CUPSD_LOG_DEBUG, "Loading printer %s...", value);

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
	break;
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
	break;
      }
    }
    else if (!p)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Syntax error on line %d of printers.conf.", linenum);
      break;
    }
    else if (!strcasecmp(line, "AuthInfoRequired"))
    {
      if (!cupsdSetAuthInfoRequired(p, value, NULL))
	cupsdLogMessage(CUPSD_LOG_ERROR,
			"Bad AuthInfoRequired on line %d of printers.conf.",
			linenum);
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
	                "Syntax error on line %d of printers.conf.", linenum);
      else
      {
        for (; *valueptr && isspace(*valueptr & 255); *valueptr++ = '\0');

        p->num_options = cupsAddOption(value, valueptr, p->num_options,
	                               &(p->options));
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
	break;
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
    else if (!strcasecmp(line, "StateTime"))
    {
     /*
      * Set the state time...
      */

      if (value)
        p->state_time = atoi(value);
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
	break;
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
	                "Syntax error on line %d of printers.conf.", linenum);
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
	                "Syntax error on line %d of printers.conf.", linenum);
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
	                "Syntax error on line %d of printers.conf.", linenum);
	break;
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
	break;
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
	break;
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
	break;
      }
    }
    else if (!strcasecmp(line, "OpPolicy"))
    {
      if (value)
      {
        cupsd_policy_t *pol;		/* Policy */


        if ((pol = cupsdFindPolicy(value)) != NULL)
	{
          cupsdSetString(&p->op_policy, value);
	  p->op_policy_ptr = pol;
	}
	else
	  cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "Bad policy \"%s\" on line %d of printers.conf",
			  value, linenum);
      }
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of printers.conf.", linenum);
	break;
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
	break;
      }
    }
    else if (!strcasecmp(line, "Attribute") && value)
    {
      for (valueptr = value; *valueptr && !isspace(*valueptr & 255); valueptr ++);

      if (!*valueptr)
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of printers.conf.", linenum);
      else
      {
        for (; *valueptr && isspace(*valueptr & 255); *valueptr++ = '\0');

        if (!p->attrs)
	  cupsdSetPrinterAttrs(p);

        cupsdSetPrinterAttr(p, value, valueptr);

	if (!strncmp(value, "marker-", 7))
	  p->marker_time = time(NULL);
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
 * 'cupsdRenamePrinter()' - Rename a printer.
 */

void
cupsdRenamePrinter(
    cupsd_printer_t *p,			/* I - Printer */
    const char      *name)		/* I - New name */
{
 /*
  * Remove the printer from the array(s) first...
  */

  cupsArrayRemove(Printers, p);

  if (p->type & CUPS_PRINTER_IMPLICIT)
    cupsArrayRemove(ImplicitPrinters, p);

 /*
  * Rename the printer type...
  */

  mimeDeleteType(MimeDatabase, p->filetype);
  p->filetype = mimeAddType(MimeDatabase, "printer", name);

  mimeDeleteType(MimeDatabase, p->prefiltertype);
  p->prefiltertype = mimeAddType(MimeDatabase, "prefilter", name);

 /*
  * Rename the printer...
  */

  cupsdSetString(&p->name, name);

 /*
  * Reset printer attributes...
  */

  cupsdSetPrinterAttrs(p);

 /*
  * Add the printer back to the printer array(s)...
  */

  cupsArrayAdd(Printers, p);

  if (p->type & CUPS_PRINTER_IMPLICIT)
    cupsArrayAdd(ImplicitPrinters, p);
}


/*
 * 'cupsdSaveAllPrinters()' - Save all printer definitions to the printers.conf
 *                            file.
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
  cups_option_t		*option;	/* Current option */
  const char		*ptr;		/* Pointer into info/location */
  ipp_attribute_t	*marker;	/* Current marker attribute */


 /*
  * Create the printers.conf file...
  */

  snprintf(temp, sizeof(temp), "%s/printers.conf", ServerRoot);
  snprintf(backup, sizeof(backup), "%s/printers.conf.O", ServerRoot);

  if (rename(temp, backup))
  {
    if (errno != ENOENT)
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to backup printers.conf - %s", strerror(errno));
  }

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
  fchmod(cupsFileNumber(fp), 0600);

 /*
  * Write a small header to the file...
  */

  curtime = time(NULL);
  curdate = localtime(&curtime);
  strftime(temp, sizeof(temp) - 1, "%Y-%m-%d %H:%M", curdate);

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

    if ((printer->type & CUPS_PRINTER_DISCOVERED) ||
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

    if (printer->num_auth_info_required > 0)
    {
      cupsFilePrintf(fp, "AuthInfoRequired %s", printer->auth_info_required[0]);
      for (i = 1; i < printer->num_auth_info_required; i ++)
        cupsFilePrintf(fp, ",%s", printer->auth_info_required[i]);
      cupsFilePutChar(fp, '\n');
    }

    if (printer->info)
    {
      if ((ptr = strchr(printer->info, '#')) != NULL)
      {
       /*
        * Need to quote the first # in the info string...
	*/

        cupsFilePuts(fp, "Info ");
	cupsFileWrite(fp, printer->info, ptr - printer->info);
	cupsFilePutChar(fp, '\\');
	cupsFilePuts(fp, ptr);
	cupsFilePutChar(fp, '\n');
      }
      else
        cupsFilePrintf(fp, "Info %s\n", printer->info);
    }

    if (printer->location)
    {
      if ((ptr = strchr(printer->info, '#')) != NULL)
      {
       /*
        * Need to quote the first # in the location string...
	*/

        cupsFilePuts(fp, "Location ");
	cupsFileWrite(fp, printer->location, ptr - printer->location);
	cupsFilePutChar(fp, '\\');
	cupsFilePuts(fp, ptr);
	cupsFilePutChar(fp, '\n');
      }
      else
        cupsFilePrintf(fp, "Location %s\n", printer->location);
    }
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

    cupsFilePrintf(fp, "StateTime %d\n", (int)printer->state_time);

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
    {
      if ((ptr = strchr(printer->users[i], '#')) != NULL)
      {
       /*
        * Need to quote the first # in the user string...
	*/

        cupsFilePrintf(fp, "%sUser ", printer->deny_users ? "Deny" : "Allow");
	cupsFileWrite(fp, printer->users[i], ptr - printer->users[i]);
	cupsFilePutChar(fp, '\\');
	cupsFilePuts(fp, ptr);
	cupsFilePutChar(fp, '\n');
      }
      else
        cupsFilePrintf(fp, "%sUser %s\n",
	               printer->deny_users ? "Deny" : "Allow",
                       printer->users[i]);
    }

    if (printer->op_policy)
      cupsFilePrintf(fp, "OpPolicy %s\n", printer->op_policy);
    if (printer->error_policy)
      cupsFilePrintf(fp, "ErrorPolicy %s\n", printer->error_policy);

    for (i = printer->num_options, option = printer->options;
         i > 0;
	 i --, option ++)
    {
      if ((ptr = strchr(option->value, '#')) != NULL)
      {
       /*
        * Need to quote the first # in the option string...
	*/

        cupsFilePrintf(fp, "Option %s ", option->name);
	cupsFileWrite(fp, option->value, ptr - option->value);
	cupsFilePutChar(fp, '\\');
	cupsFilePuts(fp, ptr);
	cupsFilePutChar(fp, '\n');
      }
      else
        cupsFilePrintf(fp, "Option %s %s\n", option->name, option->value);
    }

    if ((marker = ippFindAttribute(printer->attrs, "marker-colors",
                                   IPP_TAG_NAME)) != NULL)
    {
      cupsFilePrintf(fp, "Attribute %s ", marker->name);

      for (i = 0, ptr = NULL; i < marker->num_values; i ++)
      {
        if (i)
	  cupsFilePutChar(fp, ',');

        if (!ptr && (ptr = strchr(marker->values[i].string.text, '#')) != NULL)
	{
	  cupsFileWrite(fp, marker->values[i].string.text,
	                ptr - marker->values[i].string.text);
	  cupsFilePutChar(fp, '\\');
	  cupsFilePuts(fp, ptr);
	}
	else
          cupsFilePuts(fp, marker->values[i].string.text);
      }

      cupsFilePuts(fp, "\n");
    }

    if ((marker = ippFindAttribute(printer->attrs, "marker-levels",
                                   IPP_TAG_INTEGER)) != NULL)
    {
      cupsFilePrintf(fp, "Attribute %s %d", marker->name,
                     marker->values[0].integer);
      for (i = 1; i < marker->num_values; i ++)
        cupsFilePrintf(fp, ",%d", marker->values[i].integer);
      cupsFilePuts(fp, "\n");
    }

    if ((marker = ippFindAttribute(printer->attrs, "marker-message",
                                   IPP_TAG_TEXT)) != NULL)
    {
      cupsFilePrintf(fp, "Attribute %s ", marker->name);

      if ((ptr = strchr(marker->values[0].string.text, '#')) != NULL)
      {
	cupsFileWrite(fp, marker->values[0].string.text,
		      ptr - marker->values[0].string.text);
	cupsFilePutChar(fp, '\\');
	cupsFilePuts(fp, ptr);
      }
      else
	cupsFilePuts(fp, marker->values[0].string.text);

      cupsFilePuts(fp, "\n");
    }

    if ((marker = ippFindAttribute(printer->attrs, "marker-names",
                                   IPP_TAG_NAME)) != NULL)
    {
      cupsFilePrintf(fp, "Attribute %s ", marker->name);

      for (i = 0, ptr = NULL; i < marker->num_values; i ++)
      {
        if (i)
	  cupsFilePutChar(fp, ',');

        if (!ptr && (ptr = strchr(marker->values[i].string.text, '#')) != NULL)
	{
	  cupsFileWrite(fp, marker->values[i].string.text,
	                ptr - marker->values[i].string.text);
	  cupsFilePutChar(fp, '\\');
	  cupsFilePuts(fp, ptr);
	}
	else
          cupsFilePuts(fp, marker->values[i].string.text);
      }

      cupsFilePuts(fp, "\n");
    }

    if ((marker = ippFindAttribute(printer->attrs, "marker-types",
                                   IPP_TAG_KEYWORD)) != NULL)
    {
      cupsFilePrintf(fp, "Attribute %s ", marker->name);

      for (i = 0, ptr = NULL; i < marker->num_values; i ++)
      {
        if (i)
	  cupsFilePutChar(fp, ',');

        if (!ptr && (ptr = strchr(marker->values[i].string.text, '#')) != NULL)
	{
	  cupsFileWrite(fp, marker->values[i].string.text,
	                ptr - marker->values[i].string.text);
	  cupsFilePutChar(fp, '\\');
	  cupsFilePuts(fp, ptr);
	}
	else
          cupsFilePuts(fp, marker->values[i].string.text);
      }

      cupsFilePuts(fp, "\n");
    }

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
 * 'cupsdSetAuthInfoRequired()' - Set the required authentication info.
 */

int					/* O - 1 if value OK, 0 otherwise */
cupsdSetAuthInfoRequired(
    cupsd_printer_t *p,			/* I - Printer */
    const char      *values,		/* I - Plain text value (or NULL) */
    ipp_attribute_t *attr)		/* I - IPP attribute value (or NULL) */
{
  int	i;				/* Looping var */


  p->num_auth_info_required = 0;

 /*
  * Do we have a plain text value?
  */

  if (values)
  {
   /*
    * Yes, grab the keywords...
    */

    const char	*end;			/* End of current value */


    while (*values && p->num_auth_info_required < 4)
    {
      if ((end = strchr(values, ',')) == NULL)
        end = values + strlen(values);

      if ((end - values) == 4 && !strncmp(values, "none", 4))
      {
        if (p->num_auth_info_required != 0 || *end)
	  return (0);

        p->auth_info_required[p->num_auth_info_required] = "none";
	p->num_auth_info_required ++;

	return (1);
      }
      else if ((end - values) == 9 && !strncmp(values, "negotiate", 9))
      {
        if (p->num_auth_info_required != 0 || *end)
	  return (0);

        p->auth_info_required[p->num_auth_info_required] = "negotiate";
	p->num_auth_info_required ++;
      }
      else if ((end - values) == 6 && !strncmp(values, "domain", 6))
      {
        p->auth_info_required[p->num_auth_info_required] = "domain";
	p->num_auth_info_required ++;
      }
      else if ((end - values) == 8 && !strncmp(values, "password", 8))
      {
        p->auth_info_required[p->num_auth_info_required] = "password";
	p->num_auth_info_required ++;
      }
      else if ((end - values) == 8 && !strncmp(values, "username", 8))
      {
        p->auth_info_required[p->num_auth_info_required] = "username";
	p->num_auth_info_required ++;
      }
      else
        return (0);

      values = (*end) ? end + 1 : end;
    }

    if (p->num_auth_info_required == 0)
    {
      p->auth_info_required[0]  = "none";
      p->num_auth_info_required = 1;
    }

   /*
    * Update the printer-type value as needed...
    */

    if (p->num_auth_info_required > 1 ||
        strcmp(p->auth_info_required[0], "none"))
      p->type |= CUPS_PRINTER_AUTHENTICATED;
    else
      p->type &= ~CUPS_PRINTER_AUTHENTICATED;

    return (1);
  }

 /*
  * Grab values from an attribute instead...
  */

  if (!attr || attr->num_values > 4)
    return (0);

 /*
  * Update the printer-type value as needed...
  */

  if (attr->num_values > 1 ||
      strcmp(attr->values[0].string.text, "none"))
    p->type |= CUPS_PRINTER_AUTHENTICATED;
  else
    p->type &= ~CUPS_PRINTER_AUTHENTICATED;

  for (i = 0; i < attr->num_values; i ++)
  {
    if (!strcmp(attr->values[i].string.text, "none"))
    {
      if (p->num_auth_info_required != 0 || attr->num_values != 1)
	return (0);

      p->auth_info_required[p->num_auth_info_required] = "none";
      p->num_auth_info_required ++;

      return (1);
    }
    else if (!strcmp(attr->values[i].string.text, "negotiate"))
    {
      if (p->num_auth_info_required != 0 || attr->num_values != 1)
	return (0);

      p->auth_info_required[p->num_auth_info_required] = "negotiate";
      p->num_auth_info_required ++;

      return (1);
    }
    else if (!strcmp(attr->values[i].string.text, "domain"))
    {
      p->auth_info_required[p->num_auth_info_required] = "domain";
      p->num_auth_info_required ++;
    }
    else if (!strcmp(attr->values[i].string.text, "password"))
    {
      p->auth_info_required[p->num_auth_info_required] = "password";
      p->num_auth_info_required ++;
    }
    else if (!strcmp(attr->values[i].string.text, "username"))
    {
      p->auth_info_required[p->num_auth_info_required] = "username";
      p->num_auth_info_required ++;
    }
    else
      return (0);
  }

  return (1);
}


/*
 * 'cupsdSetPrinterAttr()' - Set a printer attribute.
 */

void
cupsdSetPrinterAttr(
    cupsd_printer_t *p,			/* I - Printer */
    const char      *name,		/* I - Attribute name */
    char            *value)		/* I - Attribute value string */
{
  ipp_attribute_t	*attr;		/* Attribute */
  int			i,		/* Looping var */
			count;		/* Number of values */
  char			*ptr;		/* Pointer into value */
  ipp_tag_t		value_tag;	/* Value tag for this attribute */


 /*
  * Count the number of values...
  */

  for (count = 1, ptr = value;
       (ptr = strchr(ptr, ',')) != NULL;
       ptr ++, count ++);

 /*
  * Then add or update the attribute as needed...
  */

  if (!strcmp(name, "marker-levels"))
  {
   /*
    * Integer values...
    */

    if ((attr = ippFindAttribute(p->attrs, name, IPP_TAG_INTEGER)) != NULL &&
        attr->num_values < count)
    {
      ippDeleteAttribute(p->attrs, attr);
      attr = NULL;
    }

    if (attr)
      attr->num_values = count;
    else
      attr = ippAddIntegers(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, name,
                            count, NULL);

    if (!attr)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to allocate memory for printer attribute "
		      "(%d values)", count);
      return;
    }

    for (i = 0; i < count; i ++)
    {
      if ((ptr = strchr(value, ',')) != NULL)
        *ptr++ = '\0';

      attr->values[i].integer = strtol(value, NULL, 10);

      if (ptr)
        value = ptr;
    }
  }
  else
  {
   /*
    * Name or keyword values...
    */

    if (!strcmp(name, "marker-types"))
      value_tag = IPP_TAG_KEYWORD;
    else if (!strcmp(name, "marker-message"))
      value_tag = IPP_TAG_TEXT;
    else
      value_tag = IPP_TAG_NAME;

    if ((attr = ippFindAttribute(p->attrs, name, value_tag)) != NULL &&
        attr->num_values < count)
    {
      ippDeleteAttribute(p->attrs, attr);
      attr = NULL;
    }

    if (attr)
    {
      for (i = 0; i < attr->num_values; i ++)
	_cupsStrFree(attr->values[i].string.text);

      attr->num_values = count;
    }
    else
      attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, value_tag, name,
                           count, NULL, NULL);

    if (!attr)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to allocate memory for printer attribute "
		      "(%d values)", count);
      return;
    }

    for (i = 0; i < count; i ++)
    {
      if ((ptr = strchr(value, ',')) != NULL)
        *ptr++ = '\0';

      attr->values[i].string.text = _cupsStrAlloc(value);

      if (ptr)
        value = ptr;
    }
  }

  cupsdMarkDirty(CUPSD_DIRTY_PRINTERS);
}


/*
 * 'cupsdSetPrinterAttrs()' - Set printer attributes based upon the PPD file.
 */

void
cupsdSetPrinterAttrs(cupsd_printer_t *p)/* I - Printer to setup */
{
  int		i,			/* Looping var */
		length;			/* Length of browse attributes */
  char		uri[HTTP_MAX_URI];	/* URI for printer */
  char		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  char		filename[1024];		/* Name of PPD file */
  int		num_air;		/* Number of auth-info-required values */
  const char	* const *air;		/* auth-info-required values */
  int		num_media;		/* Number of media options */
  cupsd_location_t *auth;		/* Pointer to authentication element */
  const char	*auth_supported;	/* Authentication supported */
  ppd_file_t	*ppd;			/* PPD file data */
  ppd_option_t	*input_slot,		/* InputSlot options */
		*media_type,		/* MediaType options */
		*page_size,		/* PageSize options */
		*output_bin,		/* OutputBin options */
		*media_quality,		/* EFMediaQualityMode options */
		*duplex;		/* Duplex options */
  ppd_attr_t	*ppdattr;		/* PPD attribute */
  ipp_t		*oldattrs;		/* Old printer attributes */
  ipp_attribute_t *attr;		/* Attribute data */
  ipp_value_t	*val;			/* Attribute value */
  int		num_finishings;		/* Number of finishings */
  int		finishings[5];		/* finishings-supported values */
  cups_option_t	*option;		/* Current printer option */
  static const char * const sides[3] =	/* sides-supported values */
		{
		  "one-sided",
		  "two-sided-long-edge",
		  "two-sided-short-edge"
		};
  static const char * const air_userpass[] =
		{			/* Basic/Digest authentication */
		  "username",
		  "password"
		};
#ifdef HAVE_GSSAPI
  static const char * const air_negotiate[] =
		{			/* Kerberos authentication */
		  "negotiate"
		};
#endif /* HAVE_GSSAPI */
  static const char * const air_none[] =
		{			/* No authentication */
		  "none"
		};
  static const char * const standard_commands[] =
		{			/* Standard CUPS commands */
		  "AutoConfigure",
		  "Clean",
		  "PrintSelfTestPage",
		  "ReportLevels"
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

  delete_printer_filters(p);

 /*
  * Figure out the authentication that is required for the printer.
  */

  auth_supported = "requesting-user-name";
  num_air        = 1;
  air            = air_none;

  if (p->num_auth_info_required > 0 && strcmp(p->auth_info_required[0], "none"))
  {
    num_air = p->num_auth_info_required;
    air     = p->auth_info_required;

    if (!strcmp(air[0], "username"))
      auth_supported = "basic";
    else
      auth_supported = "negotiate";
  }
  else if (!(p->type & CUPS_PRINTER_DISCOVERED))
  {
    if (p->type & CUPS_PRINTER_CLASS)
      snprintf(resource, sizeof(resource), "/classes/%s", p->name);
    else
      snprintf(resource, sizeof(resource), "/printers/%s", p->name);

    if ((auth = cupsdFindBest(resource, HTTP_POST)) == NULL ||
        auth->type == CUPSD_AUTH_NONE)
      auth = cupsdFindPolicyOp(p->op_policy_ptr, IPP_PRINT_JOB);

    if (auth)
    {
      if (auth->type == CUPSD_AUTH_BASIC || auth->type == CUPSD_AUTH_BASICDIGEST)
      {
	auth_supported = "basic";
	num_air        = 2;
	air            = air_userpass;
      }
      else if (auth->type == CUPSD_AUTH_DIGEST)
      {
	auth_supported = "digest";
	num_air        = 2;
	air            = air_userpass;
      }
#ifdef HAVE_GSSAPI
      else if (auth->type == CUPSD_AUTH_NEGOTIATE)
      {
	auth_supported = "negotiate";
	num_air        = 1;
	air            = air_negotiate;
      }
#endif /* HAVE_GSSAPI */

      if (auth->type != CUPSD_AUTH_NONE)
        p->type |= CUPS_PRINTER_AUTHENTICATED;
      else
        p->type &= ~CUPS_PRINTER_AUTHENTICATED;
    }
    else
      p->type &= ~CUPS_PRINTER_AUTHENTICATED;
  }
  else if (p->type & CUPS_PRINTER_AUTHENTICATED)
  {
    num_air = 2;
    air     = air_userpass;
  }

 /*
  * Create the required IPP attributes for a printer...
  */

  oldattrs = p->attrs;
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
  ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
		"auth-info-required", num_air, NULL, air);

  if (cupsArrayCount(Banners) > 0 && !(p->type & CUPS_PRINTER_DISCOVERED))
  {
   /*
    * Setup the job-sheets-default attribute...
    */

    attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME,
                	 "job-sheets-default", 2, NULL, NULL);

    if (attr != NULL)
    {
      attr->values[0].string.text = _cupsStrAlloc(Classification ?
	                                   Classification : p->job_sheets[0]);
      attr->values[1].string.text = _cupsStrAlloc(Classification ?
	                                   Classification : p->job_sheets[1]);
    }
  }

  p->raw    = 0;
  p->remote = 0;

  if (p->type & CUPS_PRINTER_DISCOVERED)
  {
   /*
    * Tell the client this is a remote printer of some type...
    */

    ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_URI,
	         "printer-uri-supported", NULL, p->uri);

    if (p->make_model)
      ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                   "printer-make-and-model", NULL, p->make_model);

    ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uri", NULL,
        	 p->uri);

    p->raw    = 1;
    p->remote = 1;
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

      if ((p->type & CUPS_PRINTER_IMPLICIT) && p->num_printers > 0 &&
          p->printers[0]->make_model)
	ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                     "printer-make-and-model", NULL, p->printers[0]->make_model);
      else
	ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                     "printer-make-and-model", NULL, "Local Printer Class");

      ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uri", NULL,
        	   "file:///dev/null");

      if (p->num_printers > 0)
      {
       /*
	* Add a list of member names; URIs are added in copy_printer_attrs...
	*/

	attr    = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME,
                                "member-names", p->num_printers, NULL, NULL);
        p->type |= CUPS_PRINTER_OPTIONS;

	for (i = 0; i < p->num_printers; i ++)
	{
          if (attr != NULL)
            attr->values[i].string.text = _cupsStrAlloc(p->printers[i]->name);

	  p->type &= ~CUPS_PRINTER_OPTIONS | p->printers[i]->type;
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
	{
	 /*
	  * The NickName can be localized in the character set specified
	  * by the LanugageEncoding attribute.  However, ppdOpen2() has
	  * already converted the ppd->nickname member to UTF-8 for us
	  * (the original attribute value is available separately)
	  */

          cupsdSetString(&p->make_model, ppd->nickname);
	}
	else if (ppd->modelname)
	{
	 /*
	  * Model name can only contain specific characters...
	  */

          cupsdSetString(&p->make_model, ppd->modelname);
	}
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
	  cupsdLogMessage(CUPSD_LOG_CRIT,
	                  "The PPD file for printer %s contains no media "
	                  "options and is therefore invalid!", p->name);
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
		val->string.text = _cupsStrAlloc(input_slot->choices[i].choice);

	    if (media_type != NULL)
	      for (i = 0; i < media_type->num_choices; i ++, val ++)
		val->string.text = _cupsStrAlloc(media_type->choices[i].choice);

	    if (media_quality != NULL)
	      for (i = 0; i < media_quality->num_choices; i ++, val ++)
		val->string.text = _cupsStrAlloc(media_quality->choices[i].choice);

	    if (page_size != NULL)
	    {
	      for (i = 0; i < page_size->num_choices; i ++, val ++)
		val->string.text = _cupsStrAlloc(page_size->choices[i].choice);

	      ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
	                   "media-default", NULL, page_size->defchoice);
            }
	    else if (input_slot != NULL)
	      ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
	                   "media-default", NULL, input_slot->defchoice);
	    else if (media_type != NULL)
	      ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
	                   "media-default", NULL, media_type->defchoice);
	    else if (media_quality != NULL)
	      ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
	                   "media-default", NULL, media_quality->defchoice);
	    else
	      ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
	                   "media-default", NULL, "none");
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
	      val->string.text = _cupsStrAlloc(output_bin->choices[i].choice);
          }
	}

       /*
        * Duplexing, etc...
	*/

	if ((duplex = ppdFindOption(ppd, "Duplex")) == NULL)
	  if ((duplex = ppdFindOption(ppd, "EFDuplex")) == NULL)
	    if ((duplex = ppdFindOption(ppd, "EFDuplexing")) == NULL)
	      if ((duplex = ppdFindOption(ppd, "KD03Duplex")) == NULL)
		duplex = ppdFindOption(ppd, "JCLDuplex");

	if (duplex && duplex->num_choices > 1)
	{
	  p->type |= CUPS_PRINTER_DUPLEX;

	  ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
	                "sides-supported", 3, NULL, sides);

          if (!strcasecmp(duplex->defchoice, "DuplexTumble"))
	    ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
	        	 "sides-default", NULL, "two-sided-short-edge");
          else if (!strcasecmp(duplex->defchoice, "DuplexNoTumble"))
	    ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
	        	 "sides-default", NULL, "two-sided-long-edge");
	  else
	    ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
	        	 "sides-default", NULL, "one-sided");
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

        add_printer_filter(p, p->filetype, "application/vnd.cups-raw 0 -");

       /*
	* Add any pre-filters in the PPD file...
	*/

	if ((ppdattr = ppdFindAttr(ppd, "cupsPreFilter", NULL)) != NULL)
	{
	  p->prefiltertype = mimeAddType(MimeDatabase, "prefilter", p->name);

	  for (; ppdattr; ppdattr = ppdFindNextAttr(ppd, "cupsPreFilter", NULL))
	    if (ppdattr->value)
	      add_printer_filter(p, p->prefiltertype, ppdattr->value);
	}

       /*
	* Add any filters in the PPD file...
	*/

	DEBUG_printf(("ppd->num_filters = %d\n", ppd->num_filters));
	for (i = 0; i < ppd->num_filters; i ++)
	{
          DEBUG_printf(("ppd->filters[%d] = \"%s\"\n", i, ppd->filters[i]));
          add_printer_filter(p, p->filetype, ppd->filters[i]);
	}

	if (ppd->num_filters == 0)
	{
	 /*
	  * If there are no filters, add PostScript printing filters.
	  */

          add_printer_filter(p, p->filetype,
	                     "application/vnd.cups-command 0 commandtops");
          add_printer_filter(p, p->filetype,
	                     "application/vnd.cups-postscript 0 -");

          p->type |= CUPS_PRINTER_COMMANDS;
        }
	else if (!(p->type & CUPS_PRINTER_COMMANDS))
	{
	 /*
	  * See if this is a PostScript device without a command filter...
	  */

	  for (i = 0; i < ppd->num_filters; i ++)
	    if (!strncasecmp(ppd->filters[i],
	                     "application/vnd.cups-postscript", 31))
	      break;

          if (i < ppd->num_filters)
	  {
	   /*
	    * Add the generic PostScript command filter...
	    */

	    add_printer_filter(p, p->filetype,
			       "application/vnd.cups-command 0 commandtops");
	    p->type |= CUPS_PRINTER_COMMANDS;
	  }
	}

        if (p->type & CUPS_PRINTER_COMMANDS)
	{
	  char	*commands,		/* Copy of commands */
	  	*start,			/* Start of name */
		*end;			/* End of name */
          int	count;			/* Number of commands */


          if ((ppdattr = ppdFindAttr(ppd, "cupsCommands", NULL)) != NULL &&
	      ppdattr->value && ppdattr->value[0])
	  {
	    for (count = 0, start = ppdattr->value; *start; count ++)
	    {
	      while (isspace(*start & 255))
		start ++;

	      if (!*start)
		break;

	      while (*start && !isspace(*start & 255))
		start ++;
	    }
	  }
	  else
	    count = 0;

          if (count > 0)
	  {
	   /*
	    * Make a copy of the commands string and count how many ...
	    */

	    attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
	                         "printer-commands", count, NULL, NULL);

	    commands = strdup(ppdattr->value);

	    for (count = 0, start = commands; *start; count ++)
	    {
	      while (isspace(*start & 255))
		start ++;

	      if (!*start)
		break;

              end = start;
	      while (*end && !isspace(*end & 255))
		end ++;

              if (*end)
	        *end++ = '\0';

              attr->values[count].string.text = _cupsStrAlloc(start);

              start = end;
	    }

	    free(commands);
          }
	  else
	  {
	   /*
	    * Add the standard list of commands...
	    */

	    ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
	                  "printer-commands",
			  (int)(sizeof(standard_commands) /
			        sizeof(standard_commands[0])), NULL,
			  standard_commands);
	  }
	}
	else
	{
	 /*
	  * No commands supported...
	  */

	  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
	               "printer-commands", NULL, "none");
        }

       /*
	* Show current and available port monitors for this printer...
	*/

	ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME, "port-monitor",
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

        attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME,
	                     "port-monitor-supported", i, NULL, NULL);

        attr->values[0].string.text = _cupsStrAlloc("none");

        for (i = 1, ppdattr = ppdFindAttr(ppd, "cupsPortMonitor", NULL);
	     ppdattr;
	     i ++, ppdattr = ppdFindNextAttr(ppd, "cupsPortMonitor", NULL))
	  attr->values[i].string.text = _cupsStrAlloc(ppdattr->value);

        if (ppd->protocols)
	{
	  if (strstr(ppd->protocols, "TBCP"))
	    attr->values[i].string.text = _cupsStrAlloc("tbcp");
	  else if (strstr(ppd->protocols, "BCP"))
	    attr->values[i].string.text = _cupsStrAlloc("bcp");
	}

#ifdef HAVE_DNSSD
	cupsdSetString(&p->product, ppd->product);
#endif /* HAVE_DNSSD */

        if (ppdFindAttr(ppd, "APRemoteQueueID", NULL))
	  p->type |= CUPS_PRINTER_REMOTE;

       /*
        * Close the PPD and set the type...
	*/

	ppdClose(ppd);
      }
      else if (!access(filename, 0))
      {
        int		pline;			/* PPD line number */
	ppd_status_t	pstatus;		/* PPD load status */


        pstatus = ppdLastError(&pline);

	cupsdLogMessage(CUPSD_LOG_ERROR, "PPD file for %s cannot be loaded!",
	                p->name);

	if (pstatus <= PPD_ALLOC_ERROR)
	  cupsdLogMessage(CUPSD_LOG_ERROR, "%s", strerror(errno));
        else
	  cupsdLogMessage(CUPSD_LOG_ERROR, "%s on line %d.",
	                  ppdErrorString(pstatus), pline);

        cupsdLogMessage(CUPSD_LOG_INFO,
	                "Hint: Run \"cupstestppd %s\" and fix any errors.",
	                filename);

       /*
	* Add a filter from application/vnd.cups-raw to printer/name to
	* handle "raw" printing by users.
	*/

        add_printer_filter(p, p->filetype, "application/vnd.cups-raw 0 -");

       /*
        * Add a PostScript filter, since this is still possibly PS printer.
	*/

	add_printer_filter(p, p->filetype,
	                   "application/vnd.cups-postscript 0 -");
      }
      else
      {
       /*
	* If we have an interface script, add a filter entry for it...
	*/

	snprintf(filename, sizeof(filename), "%s/interfaces/%s", ServerRoot,
	         p->name);
	if (!access(filename, X_OK))
	{
	 /*
	  * Yes, we have a System V style interface script; use it!
	  */

	  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                       "printer-make-and-model", NULL,
		       "Local System V Printer");

	  snprintf(filename, sizeof(filename), "*/* 0 %s/interfaces/%s",
	           ServerRoot, p->name);
	  add_printer_filter(p, p->filetype, filename);
	}
	else if (p->device_uri &&
	         !strncmp(p->device_uri, "ipp://", 6) &&
	         (strstr(p->device_uri, "/printers/") != NULL ||
		  strstr(p->device_uri, "/classes/") != NULL))
        {
	 /*
	  * Tell the client this is really a hard-wired remote printer.
	  */

          p->type |= CUPS_PRINTER_REMOTE;

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

	  p->raw    = 1;
	  p->remote = 1;
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
                     "finishings-supported", num_finishings, finishings);
      ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                    "finishings-default", IPP_FINISHINGS_NONE);
    }
  }

 /*
  * Copy marker attributes as needed...
  */

  if (oldattrs)
  {
    ipp_attribute_t *oldattr;		/* Old attribute */


    if ((oldattr = ippFindAttribute(oldattrs, "marker-colors",
                                    IPP_TAG_NAME)) != NULL)
    {
      if ((attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME,
                                "marker-colors", oldattr->num_values, NULL,
				NULL)) != NULL)
      {
	for (i = 0; i < oldattr->num_values; i ++)
	  attr->values[i].string.text =
	      _cupsStrAlloc(oldattr->values[i].string.text);
      }
    }

    if ((oldattr = ippFindAttribute(oldattrs, "marker-levels",
                                    IPP_TAG_INTEGER)) != NULL)
    {
      if ((attr = ippAddIntegers(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                                 "marker-levels", oldattr->num_values,
				 NULL)) != NULL)
      {
	for (i = 0; i < oldattr->num_values; i ++)
	  attr->values[i].integer = oldattr->values[i].integer;
      }
    }

    if ((oldattr = ippFindAttribute(oldattrs, "marker-names",
                                    IPP_TAG_NAME)) != NULL)
    {
      if ((attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME,
                                "marker-names", oldattr->num_values, NULL,
				NULL)) != NULL)
      {
	for (i = 0; i < oldattr->num_values; i ++)
	  attr->values[i].string.text =
	      _cupsStrAlloc(oldattr->values[i].string.text);
      }
    }

    if ((oldattr = ippFindAttribute(oldattrs, "marker-types",
                                    IPP_TAG_KEYWORD)) != NULL)
    {
      if ((attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                                "marker-types", oldattr->num_values, NULL,
				NULL)) != NULL)
      {
	for (i = 0; i < oldattr->num_values; i ++)
	  attr->values[i].string.text =
	      _cupsStrAlloc(oldattr->values[i].string.text);
      }
    }

    ippDelete(oldattrs);
  }

 /*
  * Force sharing off for remote queues...
  */

  if (p->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT))
    p->shared = 0;
  else
  {
   /*
    * Copy the printer options into a browse attributes string we can re-use.
    */

    const char	*valptr;		/* Pointer into value */
    char	*attrptr;		/* Pointer into attribute string */


   /*
    * Free the old browse attributes as needed...
    */

    if (p->browse_attrs)
      free(p->browse_attrs);

   /*
    * Compute the length of all attributes + job-sheets, lease-duration,
    * and BrowseLocalOptions.
    */

    for (length = 1, i = p->num_options, option = p->options;
         i > 0;
	 i --, option ++)
    {
      length += strlen(option->name) + 2;

      if (option->value)
      {
        for (valptr = option->value; *valptr; valptr ++)
	  if (strchr(" \"\'\\", *valptr))
	    length += 2;
	  else
	    length ++;
      }
    }

    length += 13 + strlen(p->job_sheets[0]) + strlen(p->job_sheets[1]);
    length += 32;
    if (BrowseLocalOptions)
      length += 12 + strlen(BrowseLocalOptions);

    if (p->num_auth_info_required > 0)
    {
      length += 18;			/* auth-info-required */

      for (i = 0; i < p->num_auth_info_required; i ++)
        length += strlen(p->auth_info_required[i]) + 1;
    }

   /*
    * Allocate the new string...
    */

    if ((p->browse_attrs = calloc(1, length)) == NULL)
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to allocate %d bytes for browse data!",
		      length);
    else
    {
     /*
      * Got the allocated string, now copy the options and attributes over...
      */

      sprintf(p->browse_attrs, "job-sheets=%s,%s lease-duration=%d",
              p->job_sheets[0], p->job_sheets[1], BrowseTimeout);
      attrptr = p->browse_attrs + strlen(p->browse_attrs);

      if (BrowseLocalOptions)
      {
        sprintf(attrptr, " ipp-options=%s", BrowseLocalOptions);
        attrptr += strlen(attrptr);
      }

      for (i = p->num_options, option = p->options;
           i > 0;
	   i --, option ++)
      {
        *attrptr++ = ' ';
	strcpy(attrptr, option->name);
	attrptr += strlen(attrptr);

	if (option->value)
	{
	  *attrptr++ = '=';

          for (valptr = option->value; *valptr; valptr ++)
	  {
	    if (strchr(" \"\'\\", *valptr))
	      *attrptr++ = '\\';

	    *attrptr++ = *valptr;
	  }
	}
      }

      if (p->num_auth_info_required > 0)
      {
        strcpy(attrptr, "auth-info-required");
	attrptr += 18;

	for (i = 0; i < p->num_auth_info_required; i ++)
	{
	  *attrptr++ = i ? ',' : '=';
	  strcpy(attrptr, p->auth_info_required[i]);
	  attrptr += strlen(attrptr);
	}
      }
      else
	*attrptr = '\0';
    }
  }

 /*
  * Populate the document-format-supported attribute...
  */

  add_printer_formats(p);

  DEBUG_printf(("cupsdSetPrinterAttrs: leaving name = %s, type = %x\n", p->name,
                p->type));

 /*
  * Add name-default attributes...
  */

  add_printer_defaults(p);

#ifdef __sgi
 /*
  * Write the IRIX printer config and status files...
  */

  write_irix_config(p);
  write_irix_state(p);
#endif /* __sgi */

 /*
  * Let the browse protocols reflect the change
  */

  cupsdRegisterPrinter(p);
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

  if (!strcmp(s, "none"))
    return;

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

          if (!strcmp(reason, "paused") && p->state == IPP_PRINTER_STOPPED)
	    cupsdSetPrinterState(p, IPP_PRINTER_IDLE, 1);
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

	if (!strcmp(reason, "paused") && p->state != IPP_PRINTER_STOPPED)
	  cupsdSetPrinterState(p, IPP_PRINTER_STOPPED, 1);
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

  if (p->type & CUPS_PRINTER_DISCOVERED)
    return;

 /*
  * Set the new state...
  */

  old_state = p->state;
  p->state  = s;

  if (old_state != s)
  {
    cupsdAddEvent(s == IPP_PRINTER_STOPPED ? CUPSD_EVENT_PRINTER_STOPPED :
                      CUPSD_EVENT_PRINTER_STATE, p, NULL,
		  "%s \"%s\" state changed.",
		  (p->type & CUPS_PRINTER_CLASS) ? "Class" : "Printer",
		  p->name);

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
  * Let the browse protocols reflect the change...
  */

  if (update)
    cupsdRegisterPrinter(p);

 /*
  * Save the printer configuration if a printer goes from idle or processing
  * to stopped (or visa-versa)...
  */

  if ((old_state == IPP_PRINTER_STOPPED) != (s == IPP_PRINTER_STOPPED) &&
      update)
  {
    if (p->type & CUPS_PRINTER_CLASS)
      cupsdMarkDirty(CUPSD_DIRTY_CLASSES);
    else
      cupsdMarkDirty(CUPSD_DIRTY_PRINTERS);
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
    job->state_value              = IPP_JOB_PENDING;
    job->dirty                    = 1;

    cupsdMarkDirty(CUPSD_DIRTY_JOBS);

    cupsdAddEvent(CUPSD_EVENT_JOB_STOPPED, p, job,
		  "Job stopped due to printer being paused");
  }
}


/*
 * 'cupsdUpdatePrinterPPD()' - Update keywords in a printer's PPD file.
 */

int					/* O - 1 if successful, 0 otherwise */
cupsdUpdatePrinterPPD(
    cupsd_printer_t *p,			/* I - Printer */
    int             num_keywords,	/* I - Number of keywords */
    cups_option_t   *keywords)		/* I - Keywords */
{
  int		i;			/* Looping var */
  cups_file_t	*src,			/* Original file */
		*dst;			/* New file */
  char		srcfile[1024],		/* Original filename */
		dstfile[1024],		/* New filename */
		line[1024],		/* Line from file */
		keystring[41];		/* Keyword from line */
  cups_option_t	*keyword;		/* Current keyword */


  cupsdLogMessage(CUPSD_LOG_INFO, "Updating keywords in PPD file for %s...",
                  p->name);

 /*
  * Get the old and new PPD filenames...
  */

  snprintf(srcfile, sizeof(srcfile), "%s/ppd/%s.ppd.O", ServerRoot, p->name);
  snprintf(dstfile, sizeof(srcfile), "%s/ppd/%s.ppd", ServerRoot, p->name);

 /*
  * Rename the old file and open the old and new...
  */

  if (rename(dstfile, srcfile))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to backup PPD file for %s: %s",
                    p->name, strerror(errno));
    return (0);
  }

  if ((src = cupsFileOpen(srcfile, "r")) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to open PPD file \"%s\": %s",
                    srcfile, strerror(errno));
    rename(srcfile, dstfile);
    return (0);
  }

  if ((dst = cupsFileOpen(dstfile, "w")) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to create PPD file \"%s\": %s",
                    dstfile, strerror(errno));
    cupsFileClose(src);
    rename(srcfile, dstfile);
    return (0);
  }

 /*
  * Copy the first line and then write out all of the keywords...
  */

  if (!cupsFileGets(src, line, sizeof(line)))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to read PPD file \"%s\": %s",
                    srcfile, strerror(errno));
    cupsFileClose(src);
    cupsFileClose(dst);
    rename(srcfile, dstfile);
    return (0);
  }

  cupsFilePrintf(dst, "%s\n", line);

  for (i = num_keywords, keyword = keywords; i > 0; i --, keyword ++)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG, "*%s: %s", keyword->name, keyword->value);
    cupsFilePrintf(dst, "*%s: %s\n", keyword->name, keyword->value);
  }

 /*
  * Then copy the rest of the PPD file, dropping any keywords we changed.
  */

  while (cupsFileGets(src, line, sizeof(line)))
  {
   /*
    * Skip keywords we've already set...
    */

    if (sscanf(line, "*%40[^:]:", keystring) == 1 &&
        cupsGetOption(keystring, num_keywords, keywords))
      continue;

   /*
    * Otherwise write the line...
    */

    cupsFilePrintf(dst, "%s\n", line);
  }

 /*
  * Close files and return...
  */

  cupsFileClose(src);
  cupsFileClose(dst);

  return (1);
}


/*
 * 'cupsdUpdatePrinters()' - Update printers after a partial reload.
 */

void
cupsdUpdatePrinters(void)
{
  cupsd_printer_t	*p;		/* Current printer */


 /*
  * Loop through the printers and recreate the printer attributes
  * for any local printers since the policy and/or access control
  * stuff may have changed.  Also, if browsing is disabled, remove
  * any remote printers...
  */

  for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
       p;
       p = (cupsd_printer_t *)cupsArrayNext(Printers))
  {
   /*
    * Remove remote printers if we are no longer browsing...
    */

    if (!Browsing &&
        (p->type & (CUPS_PRINTER_IMPLICIT | CUPS_PRINTER_DISCOVERED)))
    {
      if (p->type & CUPS_PRINTER_IMPLICIT)
        cupsArrayRemove(ImplicitPrinters, p);

      cupsArraySave(Printers);
      cupsdDeletePrinter(p, 0);
      cupsArrayRestore(Printers);
      continue;
    }

   /*
    * Update the operation policy pointer...
    */

    if ((p->op_policy_ptr = cupsdFindPolicy(p->op_policy)) == NULL)
      p->op_policy_ptr = DefaultPolicyPtr;

   /*
    * Update printer attributes as needed...
    */

    if (!(p->type & CUPS_PRINTER_DISCOVERED))
      cupsdSetPrinterAttrs(p);
  }
}


/*
 * 'cupsdValidateDest()' - Validate a printer/class destination.
 */

const char *				/* O - Printer or class name */
cupsdValidateDest(
    const char      *uri,		/* I - Printer URI */
    cups_ptype_t    *dtype,		/* O - Type (printer or class) */
    cupsd_printer_t **printer)		/* O - Printer pointer */
{
  cupsd_printer_t	*p;		/* Current printer */
  char			localname[1024],/* Localized hostname */
			*lptr,		/* Pointer into localized hostname */
			*sptr,		/* Pointer into server name */
			*rptr,		/* Pointer into resource */
			scheme[32],	/* Scheme portion of URI */
			username[64],	/* Username portion of URI */
			hostname[HTTP_MAX_HOST],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */


  DEBUG_printf(("cupsdValidateDest(uri=\"%s\", dtype=%p, printer=%p)\n", uri,
                dtype, printer));

 /*
  * Initialize return values...
  */

  if (printer)
    *printer = NULL;

  if (dtype)
    *dtype = (cups_ptype_t)0;

 /*
  * Pull the hostname and resource from the URI...
  */

  httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, sizeof(scheme),
                  username, sizeof(username), hostname, sizeof(hostname),
		  &port, resource, sizeof(resource));

 /*
  * See if the resource is a class or printer...
  */

  if (!strncmp(resource, "/classes/", 9))
  {
   /*
    * Class...
    */

    rptr = resource + 9;
  }
  else if (!strncmp(resource, "/printers/", 10))
  {
   /*
    * Printer...
    */

    rptr = resource + 10;
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

  p = cupsdFindDest(rptr);

  if (p == NULL && strchr(rptr, '@') == NULL)
    return (NULL);
  else if (p != NULL)
  {
    if (printer)
      *printer = p;

    if (dtype)
      *dtype = p->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT |
                          CUPS_PRINTER_REMOTE | CUPS_PRINTER_DISCOVERED);

    return (p->name);
  }

 /*
  * Change localhost to the server name...
  */

  if (!strcasecmp(hostname, "localhost"))
    strlcpy(hostname, ServerName, sizeof(hostname));

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
        !strcasecmp(p->name, rptr))
    {
      if (printer)
        *printer = p;

      if (dtype)
	*dtype = p->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT |
                            CUPS_PRINTER_REMOTE | CUPS_PRINTER_DISCOVERED);

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

  cupsFilePuts(fp,
               "# This file was automatically generated by cupsd(8) from the\n");
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
	                   DefaultPrinter->info, ServerName,
			   DefaultPrinter->name);

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
		           p->name, ServerName, p->name,
			   p->info ? p->info : "");
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
 * 'add_printer_defaults()' - Add name-default attributes to the printer attributes.
 */

static void
add_printer_defaults(cupsd_printer_t *p)/* I - Printer */
{
  int		i;			/* Looping var */
  int		num_options;		/* Number of default options */
  cups_option_t	*options,		/* Default options */
		*option;		/* Current option */
  char		name[256];		/* name-default */


 /*
  * Maintain a common array of default attribute names...
  */

  if (!CommonDefaults)
  {
    CommonDefaults = cupsArrayNew((cups_array_func_t)strcmp, NULL);

    cupsArrayAdd(CommonDefaults, _cupsStrAlloc("copies-default"));
    cupsArrayAdd(CommonDefaults, _cupsStrAlloc("document-format-default"));
    cupsArrayAdd(CommonDefaults, _cupsStrAlloc("finishings-default"));
    cupsArrayAdd(CommonDefaults, _cupsStrAlloc("job-hold-until-default"));
    cupsArrayAdd(CommonDefaults, _cupsStrAlloc("job-priority-default"));
    cupsArrayAdd(CommonDefaults, _cupsStrAlloc("job-sheets-default"));
    cupsArrayAdd(CommonDefaults, _cupsStrAlloc("media-default"));
    cupsArrayAdd(CommonDefaults, _cupsStrAlloc("number-up-default"));
    cupsArrayAdd(CommonDefaults,
                 _cupsStrAlloc("orientation-requested-default"));
    cupsArrayAdd(CommonDefaults, _cupsStrAlloc("sides-default"));
  }

 /*
  * Add all of the default options from the .conf files...
  */

  for (num_options = 0, options = NULL, i = p->num_options, option = p->options;
       i > 0;
       i --, option ++)
  {
    if (strcmp(option->name, "ipp-options") &&
	strcmp(option->name, "job-sheets") &&
        strcmp(option->name, "lease-duration"))
    {
      snprintf(name, sizeof(name), "%s-default", option->name);
      num_options = cupsAddOption(name, option->value, num_options, &options);

      if (!cupsArrayFind(CommonDefaults, name))
        cupsArrayAdd(CommonDefaults, _cupsStrAlloc(name));
    }
  }

 /*
  * Convert options to IPP attributes...
  */

  cupsEncodeOptions2(p->attrs, num_options, options, IPP_TAG_PRINTER);
  cupsFreeOptions(num_options, options);

 /*
  * Add standard -default attributes as needed...
  */

  if (!cupsGetOption("copies", p->num_options, p->options))
    ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "copies-default",
                  1);

  if (!cupsGetOption("document-format", p->num_options, p->options))
    ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_MIMETYPE,
        	 "document-format-default", NULL, "application/octet-stream");

  if (!cupsGetOption("job-hold-until", p->num_options, p->options))
    ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                 "job-hold-until-default", NULL, "no-hold");

  if (!cupsGetOption("job-priority", p->num_options, p->options))
    ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                  "job-priority-default", 50);

  if (!cupsGetOption("number-up", p->num_options, p->options))
    ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                  "number-up-default", 1);

  if (!cupsGetOption("orientation-requested", p->num_options, p->options))
    ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_NOVALUE,
                 "orientation-requested-default", NULL, NULL);

  if (!cupsGetOption("notify-lease-duration", p->num_options, p->options))
    ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
        	  "notify-lease-duration-default", DefaultLeaseDuration);

  if (!cupsGetOption("notify-events", p->num_options, p->options))
    ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
        	 "notify-events-default", NULL, "job-completed");
}


/*
 * 'add_printer_filter()' - Add a MIME filter for a printer.
 */

static void
add_printer_filter(
    cupsd_printer_t  *p,		/* I - Printer to add to */
    mime_type_t	     *filtertype,	/* I - Filter or prefilter MIME type */
    const char       *filter)		/* I - Filter to add */
{
  char		super[MIME_MAX_SUPER],	/* Super-type for filter */
		type[MIME_MAX_TYPE],	/* Type for filter */
		program[1024];		/* Program/filter name */
  int		cost;			/* Cost of filter */
  mime_type_t	*temptype;		/* MIME type looping var */
  char		filename[1024];		/* Full filter filename */


 /*
  * Parse the filter string; it should be in the following format:
  *
  *     super/type cost program
  */

  if (sscanf(filter, "%15[^/]/%31s%d%*[ \t]%1023[^\n]", super, type, &cost,
             program) != 4)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "%s: invalid filter string \"%s\"!",
                    p->name, filter);
    return;
  }

 /*
  * See if the filter program exists; if not, stop the printer and flag
  * the error!
  */

  if (strcmp(program, "-"))
  {
    if (program[0] == '/')
      strlcpy(filename, program, sizeof(filename));
    else
      snprintf(filename, sizeof(filename), "%s/filter/%s", ServerBin, program);

    if (access(filename, X_OK))
    {
      snprintf(p->state_message, sizeof(p->state_message),
               "Filter \"%s\" for printer \"%s\" not available: %s",
	       program, p->name, strerror(errno));
      cupsdSetPrinterReasons(p, "+cups-missing-filter-error");
      cupsdSetPrinterState(p, IPP_PRINTER_STOPPED, 0);

      cupsdLogMessage(CUPSD_LOG_ERROR, "%s", p->state_message);
    }
  }

 /*
  * Mark the CUPS_PRINTER_COMMANDS bit if we have a filter for
  * application/vnd.cups-command...
  */

  if (!strcasecmp(super, "application") &&
      !strcasecmp(type, "vnd.cups-command"))
    p->type |= CUPS_PRINTER_COMMANDS;

 /*
  * Add the filter to the MIME database, supporting wildcards as needed...
  */

  for (temptype = mimeFirstType(MimeDatabase);
       temptype;
       temptype = mimeNextType(MimeDatabase))
    if (((super[0] == '*' && strcasecmp(temptype->super, "printer")) ||
         !strcasecmp(temptype->super, super)) &&
        (type[0] == '*' || !strcasecmp(temptype->type, type)))
    {
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "add_printer_filter: %s: adding filter %s/%s %s/%s %d %s",
                      p->name, temptype->super, temptype->type,
		      filtertype->super, filtertype->type,
                      cost, program);
      mimeAddFilter(MimeDatabase, temptype, filtertype, cost, program);
    }
}


/*
 * 'add_printer_formats()' - Add document-format-supported values for a printer.
 */

static void
add_printer_formats(cupsd_printer_t *p)	/* I - Printer */
{
  int		i;			/* Looping var */
  mime_type_t	*type;			/* Current MIME type */
  cups_array_t	*filters;		/* Filters */
  ipp_attribute_t *attr;		/* document-format-supported attribute */
  char		mimetype[MIME_MAX_SUPER + MIME_MAX_TYPE + 2];
					/* MIME type name */


 /*
  * Raw (and remote) queues advertise all of the supported MIME
  * types...
  */

  cupsArrayDelete(p->filetypes);
  p->filetypes = NULL;

  if (p->raw)
  {
    ippAddStrings(p->attrs, IPP_TAG_PRINTER,
                  (ipp_tag_t)(IPP_TAG_MIMETYPE | IPP_TAG_COPY),
                  "document-format-supported", NumMimeTypes, NULL, MimeTypes);
    return;
  }

 /*
  * Otherwise, loop through the supported MIME types and see if there
  * are filters for them...
  */

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "add_printer_formats: %d types, %d filters",
                  mimeNumTypes(MimeDatabase), mimeNumFilters(MimeDatabase));

  p->filetypes = cupsArrayNew(NULL, NULL);

  for (type = mimeFirstType(MimeDatabase);
       type;
       type = mimeNextType(MimeDatabase))
  {
    snprintf(mimetype, sizeof(mimetype), "%s/%s", type->super, type->type);

    if ((filters = mimeFilter(MimeDatabase, type, p->filetype, NULL)) != NULL)
    {
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "add_printer_formats: %s: %s needs %d filters",
                      p->name, mimetype, cupsArrayCount(filters));

      cupsArrayDelete(filters);
      cupsArrayAdd(p->filetypes, type);
    }
    else
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "add_printer_formats: %s: %s not supported",
                      p->name, mimetype);
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "add_printer_formats: %s: %d supported types",
		  p->name, cupsArrayCount(p->filetypes) + 1);

 /*
  * Add the file formats that can be filtered...
  */

  if ((type = mimeType(MimeDatabase, "application", "octet-stream")) == NULL ||
      !cupsArrayFind(p->filetypes, type))
    i = 1;
  else
    i = 0;

  attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_MIMETYPE,
                       "document-format-supported",
		       cupsArrayCount(p->filetypes) + 1, NULL, NULL);

  if (i)
    attr->values[0].string.text = _cupsStrAlloc("application/octet-stream");

  for (type = (mime_type_t *)cupsArrayFirst(p->filetypes);
       type;
       i ++, type = (mime_type_t *)cupsArrayNext(p->filetypes))
  {
    snprintf(mimetype, sizeof(mimetype), "%s/%s", type->super, type->type);

    attr->values[i].string.text = _cupsStrAlloc(mimetype);
  }

#ifdef HAVE_DNSSD
  {
    char		pdl[1024];	/* Buffer to build pdl list */
    mime_filter_t	*filter;	/* MIME filter looping var */


    pdl[0] = '\0';

    if (mimeType(MimeDatabase, "application", "pdf"))
      strlcat(pdl, "application/pdf,", sizeof(pdl));

    if (mimeType(MimeDatabase, "application", "postscript"))
      strlcat(pdl, "application/postscript,", sizeof(pdl));

    if (mimeType(MimeDatabase, "application", "vnd.cups-raster"))
      strlcat(pdl, "application/vnd.cups-raster,", sizeof(pdl));

   /*
    * Determine if this is a Tioga PrintJobMgr based queue...
    */

    for (filter = (mime_filter_t *)cupsArrayFirst(MimeDatabase->filters);
	 filter;
	 filter = (mime_filter_t *)cupsArrayNext(MimeDatabase->filters))
    {
      if (filter->dst == p->filetype && filter->filter && 
	  strstr(filter->filter, "PrintJobMgr"))
	break;
    }

   /*
    * We only support raw printing if this is not a Tioga PrintJobMgr based
    * queue and if application/octet-stream is a known conversion...
    */

    if (!filter && mimeType(MimeDatabase, "application", "octet-stream"))
      strlcat(pdl, "application/octet-stream,", sizeof(pdl));

    if (mimeType(MimeDatabase, "image", "png"))
      strlcat(pdl, "image/png,", sizeof(pdl));

    if (pdl[0])
      pdl[strlen(pdl) - 1] = '\0';	/* Remove trailing comma */

    cupsdSetString(&p->pdl, pdl);
  }
#endif /* HAVE_DNSSD */
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


/*
 * 'delete_printer_filters()' - Delete all MIME filters for a printer.
 */

static void
delete_printer_filters(
    cupsd_printer_t *p)			/* I - Printer to remove from */
{
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

  for (filter = mimeFirstFilter(MimeDatabase);
       filter;
       filter = mimeNextFilter(MimeDatabase))
    if (filter->dst == p->filetype)
    {
     /*
      * Delete the current filter...
      */

      mimeDeleteFilter(MimeDatabase, filter);
    }
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
  ipp_attribute_t *attr;		/* Attribute data */


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
