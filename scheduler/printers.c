/*
 * "$Id: printers.c,v 1.93.2.69 2004/10/04 20:24:17 mike Exp $"
 *
 *   Printer routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2004 by Easy Software Products, all rights reserved.
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
 *   AddPrinter()           - Add a printer to the system.
 *   AddPrinterFilter()     - Add a MIME filter for a printer.
 *   AddPrinterHistory()    - Add the current printer state to the history.
 *   AddPrinterUser()       - Add a user to the ACL.
 *   DeleteAllPrinters()    - Delete all printers from the system.
 *   DeletePrinter()        - Delete a printer from the system.
 *   DeletePrinterFilters() - Delete all MIME filters for a printer.
 *   FindPrinter()          - Find a printer in the list.
 *   FreePrinterUsers()     - Free allow/deny users.
 *   LoadAllPrinters()      - Load printers from the printers.conf file.
 *   SaveAllPrinters()      - Save all printer definitions to the printers.conf
 *   SetPrinterAttrs()      - Set printer attributes based upon the PPD file.
 *   SetPrinterReasons()    - Set/update the reasons strings.
 *   SetPrinterState()      - Update the current state of a printer.
 *   SortPrinters()         - Sort the printer list when a printer name is
 *                            changed.
 *   StopPrinter()          - Stop a printer from printing any jobs...
 *   ValidateDest()         - Validate a printer/class destination.
 *   WritePrintcap()        - Write a pseudo-printcap file for older
 *                            applications that need it...
 *   cupsdSanitizeURI()     - Sanitize a device URI...
 *   write_irix_config()    - Update the config files used by the IRIX
 *                            desktop tools.
 *   write_irix_state()     - Update the status files used by IRIX printing
 *                            desktop tools.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * Local functions...
 */

#ifdef __sgi
static void	write_irix_state(printer_t *p);
#endif /* __sgi */


/*
 * 'AddPrinter()' - Add a printer to the system.
 */

printer_t *			/* O - New printer */
AddPrinter(const char *name)	/* I - Name of printer */
{
  printer_t	*p,		/* New printer */
		*current,	/* Current printer in list */
		*prev;		/* Previous printer in list */


 /*
  * Range check input...
  */

  LogMessage(L_DEBUG2, "AddPrinter(\"%s\")", name ? name : "(null)");

  if (name == NULL)
    return (NULL);

 /*
  * Create a new printer entity...
  */

  if ((p = calloc(1, sizeof(printer_t))) == NULL)
  {
    LogMessage(L_ERROR, "Unable to allocate memory for printer - %s",
               strerror(errno));
    return (NULL);
  }

  SetString(&p->name, name);
  SetString(&p->info, name);
  SetString(&p->hostname, ServerName);

  SetStringf(&p->uri, "ipp://%s:%d/printers/%s", ServerName, LocalPort, name);
  SetStringf(&p->device_uri, "file:/dev/null");

  p->state     = IPP_PRINTER_STOPPED;
  p->accepting = 0;
  p->filetype  = mimeAddType(MimeDatabase, "printer", name);

  SetString(&p->job_sheets[0], "none");
  SetString(&p->job_sheets[1], "none");

  SetString(&p->error_policy, "stop-printer");
  SetString(&p->op_policy, DefaultPolicy);

  p->op_policy_ptr = DefaultPolicyPtr;

  if (MaxPrinterHistory)
    p->history = calloc(MaxPrinterHistory, sizeof(ipp_t *));

 /*
  * Insert the printer in the printer list alphabetically...
  */

  for (prev = NULL, current = Printers;
       current != NULL;
       prev = current, current = current->next)
    if (strcasecmp(p->name, current->name) < 0)
      break;

 /*
  * Insert this printer before the current one...
  */

  if (prev == NULL)
    Printers = p;
  else
    prev->next = p;

  p->next = current;

 /*
  * Write a new /etc/printcap or /var/spool/lp/pstatus file.
  */

  WritePrintcap();

 /*
  * Bump the printer count and return...
  */

  NumPrinters ++;

  return (p);
}


/*
 * 'AddPrinterFilter()' - Add a MIME filter for a printer.
 */

void
AddPrinterFilter(printer_t  *p,		/* I - Printer to add to */
                 const char *filter)	/* I - Filter to add */
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
    LogMessage(L_ERROR, "AddPrinterFilter: Invalid filter string \"%s\"!",
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
         strcasecmp((*temptype)->super, super) == 0) &&
        (type[0] == '*' || strcasecmp((*temptype)->type, type) == 0))
    {
      LogMessage(L_DEBUG2, "Adding filter %s/%s %s/%s %d %s",
                 (*temptype)->super, (*temptype)->type,
		 p->filetype->super, p->filetype->type,
                 cost, program);
      mimeAddFilter(MimeDatabase, *temptype, p->filetype, cost, program);
    }
}


/*
 * 'AddPrinterHistory()' - Add the current printer state to the history.
 */

void
AddPrinterHistory(printer_t *p)		/* I - Printer */
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
 * 'AddPrinterUser()' - Add a user to the ACL.
 */

void
AddPrinterUser(printer_t  *p,		/* I - Printer */
               const char *username)	/* I - User */
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
 * 'CreateCommonData()' - Create the common printer data.
 */

void
CreateCommonData(void)
{
  int		i;			/* Looping var */
  ipp_attribute_t *attr;		/* Attribute data */
  printer_t	*p;			/* Current printer */
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
		  CUPS_GET_DEVICES,
		  CUPS_GET_PPDS,
		  IPP_RESTART_JOB
		};
  static const char * const charsets[] =/* charset-supported values */
		{
		  "us-ascii",
		  "iso-8859-1",
		  "iso-8859-2",
		  "iso-8859-3",
		  "iso-8859-4",
		  "iso-8859-5",
		  "iso-8859-6",
		  "iso-8859-7",
		  "iso-8859-8",
		  "iso-8859-9",
		  "iso-8859-10",
		  "iso-8859-13",
		  "iso-8859-14",
		  "iso-8859-15",
		  "utf-8",
		  "windows-874",
		  "windows-1250",
		  "windows-1251",
		  "windows-1252",
		  "windows-1253",
		  "windows-1254",
		  "windows-1255",
		  "windows-1256",
		  "windows-1257",
		  "windows-1258",
		  "koi8-r",
		  "koi8-u",
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
      LogMessage(L_EMERG, "SetPrinterAttrs: Unable to allocate memory for "
                          "job-sheets-supported attribute: %s!",
	         strerror(errno));
    else if (!Classification || ClassifyOverride)
    {
      attr->values[0].string.text = strdup("none");

      for (i = 0; i < NumBanners; i ++)
	attr->values[i + 1].string.text = strdup(Banners[i].name);
    }
  }

 /*
  * Loop through the printers and update the op_policy_ptr values...
  */

  for (p = Printers; p; p = p->next)
    if ((p->op_policy_ptr = FindPolicy(p->op_policy)) == NULL)
      p->op_policy_ptr = DefaultPolicyPtr;
}


/*
 * 'DeleteAllPrinters()' - Delete all printers from the system.
 */

void
DeleteAllPrinters(void)
{
  printer_t	*p,	/* Pointer to current printer/class */
		*next;	/* Pointer to next printer in list */


  for (p = Printers; p != NULL; p = next)
  {
    next = p->next;

    if (!(p->type & CUPS_PRINTER_CLASS))
      DeletePrinter(p, 0);
  }
}


/*
 * 'DeletePrinter()' - Delete a printer from the system.
 */

void
DeletePrinter(printer_t *p,		/* I - Printer to delete */
	      int       update)		/* I - Update printers.conf? */
{
  int		i;			/* Looping var */
  printer_t	*current,		/* Current printer in list */
		*prev;			/* Previous printer in list */
#ifdef __sgi
  char		filename[1024];		/* Interface script filename */
#endif /* __sgi */


  DEBUG_printf(("DeletePrinter(%08x): p->name = \"%s\"...\n", p, p->name));

 /*
  * Range check input...
  */

  if (p == NULL)
    return;

 /*
  * Find the printer in the list...
  */

  for (prev = NULL, current = Printers;
       current != NULL;
       prev = current, current = current->next)
    if (p == current)
      break;

  if (current == NULL)
  {
    LogMessage(L_ERROR, "Tried to delete a non-existent printer %s!\n",
               p->name);
    return;
  }

 /*
  * Remove the printer from the list...
  */

  if (prev == NULL)
    Printers = p->next;
  else
    prev->next = p->next;

  NumPrinters --;

 /*
  * Stop printing on this printer...
  */

  StopPrinter(p, update);

 /*
  * If this printer is the next for browsing, point to the next one...
  */

  if (p == BrowseNext)
    BrowseNext = p->next;

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
    DefaultPrinter = Printers;

    WritePrintcap();
  }

 /*
  * Remove this printer from any classes and send a browse delete message...
  */

  if (!(p->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT)))
  {
    DeletePrinterFromClasses(p);
    SendBrowseDelete(p);
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

  DeletePrinterFilters(p);

  FreePrinterUsers(p);
  FreeQuotas(p);

  ClearString(&p->uri);
  ClearString(&p->hostname);
  ClearString(&p->name);
  ClearString(&p->location);
  ClearString(&p->make_model);
  ClearString(&p->info);
  ClearString(&p->job_sheets[0]);
  ClearString(&p->job_sheets[1]);
  ClearString(&p->device_uri);
  ClearString(&p->op_policy);
  ClearString(&p->error_policy);

  free(p);

 /*
  * Write a new /etc/printcap file...
  */

  WritePrintcap();
}


/*
 * 'DeletePrinterFilters()' - Delete all MIME filters for a printer.
 */

void
DeletePrinterFilters(printer_t *p)	/* I - Printer to remove from */
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
 * 'FindDest()' - Find a destination in the list.
 */

printer_t *			/* O - Destination in list */
FindDest(const char *name)	/* I - Name of printer or class to find */
{
  printer_t	*p;		/* Current destination */
  int		diff;		/* Difference */


  for (p = Printers; p != NULL; p = p->next)
    if ((diff = strcasecmp(name, p->name)) == 0)/* name == p->name */
      return (p);
    else if (diff < 0)				/* name < p->name */
      return (NULL);

  return (NULL);
}


/*
 * 'FindPrinter()' - Find a printer in the list.
 */

printer_t *			/* O - Printer in list */
FindPrinter(const char *name)	/* I - Name of printer to find */
{
  printer_t	*p;		/* Current printer */
  int		diff;		/* Difference */


  for (p = Printers; p != NULL; p = p->next)
    if ((diff = strcasecmp(name, p->name)) == 0 &&
        !(p->type & CUPS_PRINTER_CLASS))	/* name == p->name */
      return (p);
    else if (diff < 0)				/* name < p->name */
      return (NULL);

  return (NULL);
}


/*
 * 'FreePrinterUsers()' - Free allow/deny users.
 */

void
FreePrinterUsers(printer_t *p)	/* I - Printer */
{
  int	i;			/* Looping var */


  if (!p || !p->num_users)
    return;

  for (i = 0; i < p->num_users; i ++)
    free((void *)p->users[i]);

  free(p->users);

  p->num_users = 0;
  p->users     = NULL;
}


/*
 * 'LoadAllPrinters()' - Load printers from the printers.conf file.
 */

void
LoadAllPrinters(void)
{
  cups_file_t	*fp;			/* printers.conf file */
  int		linenum;		/* Current line number */
  int		len;			/* Length of line */
  char		line[1024],		/* Line from file */
		name[256],		/* Parameter name */
		*nameptr,		/* Pointer into name */
		*value,			/* Pointer to value */
		*valueptr;		/* Pointer into value */
  printer_t	*p;			/* Current printer */


 /*
  * Open the printers.conf file...
  */

  snprintf(line, sizeof(line), "%s/printers.conf", ServerRoot);
  if ((fp = cupsFileOpen(line, "r")) == NULL)
  {
    LogMessage(L_ERROR, "LoadAllPrinters: Unable to open %s - %s", line,
               strerror(errno));
    return;
  }

 /*
  * Read printer configurations until we hit EOF...
  */

  linenum = 0;
  p       = NULL;

  while (cupsFileGets(fp, line, sizeof(line)) != NULL)
  {
    linenum ++;

   /*
    * Skip comment lines...
    */

    if (line[0] == '#')
      continue;

   /*
    * Strip trailing whitespace, if any...
    */

    len = strlen(line);

    while (len > 0 && isspace(line[len - 1] & 255))
    {
      len --;
      line[len] = '\0';
    }

   /*
    * Extract the name from the beginning of the line...
    */

    for (value = line; isspace(*value & 255); value ++);

    for (nameptr = name; *value != '\0' && !isspace(*value & 255) &&
                             nameptr < (name + sizeof(name) - 1);)
      *nameptr++ = *value++;
    *nameptr = '\0';

    while (isspace(*value & 255))
      value ++;

    if (name[0] == '\0')
      continue;

   /*
    * Decode the directive...
    */

    if (!strcasecmp(name, "<Printer") ||
        !strcasecmp(name, "<DefaultPrinter"))
    {
     /*
      * <Printer name> or <DefaultPrinter name>
      */

      if (line[len - 1] == '>' && p == NULL)
      {
       /*
        * Add the printer and a base file type...
	*/

        line[len - 1] = '\0';

        LogMessage(L_DEBUG, "LoadAllPrinters: Loading printer %s...", value);

        p = AddPrinter(value);
	p->accepting = 1;
	p->state     = IPP_PRINTER_IDLE;

       /*
        * Set the default printer as needed...
	*/

        if (strcasecmp(name, "<DefaultPrinter") == 0)
	  DefaultPrinter = p;
      }
      else
      {
        LogMessage(L_ERROR, "Syntax error on line %d of printers.conf.",
	           linenum);
        return;
      }
    }
    else if (!strcasecmp(name, "</Printer>"))
    {
      if (p != NULL)
      {
        SetPrinterAttrs(p);
	AddPrinterHistory(p);
        p = NULL;
      }
      else
      {
        LogMessage(L_ERROR, "Syntax error on line %d of printers.conf.",
	           linenum);
        return;
      }
    }
    else if (p == NULL)
    {
      LogMessage(L_ERROR, "Syntax error on line %d of printers.conf.",
	         linenum);
      return;
    }
    else if (!strcasecmp(name, "Info"))
      SetString(&p->info, value);
    else if (!strcasecmp(name, "Location"))
      SetString(&p->location, value);
    else if (!strcasecmp(name, "DeviceURI"))
      SetString(&p->device_uri, value);
    else if (!strcasecmp(name, "State"))
    {
     /*
      * Set the initial queue state...
      */

      if (!strcasecmp(value, "idle"))
        p->state = IPP_PRINTER_IDLE;
      else if (!strcasecmp(value, "stopped"))
        p->state = IPP_PRINTER_STOPPED;
    }
    else if (!strcasecmp(name, "StateMessage"))
    {
     /*
      * Set the initial queue state message...
      */

      while (isspace(*value & 255))
        value ++;

      strlcpy(p->state_message, value, sizeof(p->state_message));
    }
    else if (!strcasecmp(name, "Accepting"))
    {
     /*
      * Set the initial accepting state...
      */

      if (!strcasecmp(value, "yes") ||
          !strcasecmp(value, "on") ||
          !strcasecmp(value, "true"))
        p->accepting = 1;
      else
        p->accepting = 0;
    }
    else if (!strcasecmp(name, "JobSheets"))
    {
     /*
      * Set the initial job sheets...
      */

      for (valueptr = value; *valueptr && !isspace(*valueptr & 255); valueptr ++);

      if (*valueptr)
        *valueptr++ = '\0';

      SetString(&p->job_sheets[0], value);

      while (isspace(*valueptr & 255))
        valueptr ++;

      if (*valueptr)
      {
        for (value = valueptr; *valueptr && !isspace(*valueptr & 255); valueptr ++);

	if (*valueptr)
          *valueptr++ = '\0';

	SetString(&p->job_sheets[1], value);
      }
    }
    else if (!strcasecmp(name, "AllowUser"))
    {
      p->deny_users = 0;
      AddPrinterUser(p, value);
    }
    else if (!strcasecmp(name, "DenyUser"))
    {
      p->deny_users = 1;
      AddPrinterUser(p, value);
    }
    else if (!strcasecmp(name, "QuotaPeriod"))
      p->quota_period = atoi(value);
    else if (!strcasecmp(name, "PageLimit"))
      p->page_limit = atoi(value);
    else if (!strcasecmp(name, "KLimit"))
      p->k_limit = atoi(value);
    else if (!strcasecmp(name, "OpPolicy"))
      SetString(&p->op_policy, value);
    else if (!strcasecmp(name, "ErrorPolicy"))
      SetString(&p->error_policy, value);
    else
    {
     /*
      * Something else we don't understand...
      */

      LogMessage(L_ERROR, "Unknown configuration directive %s on line %d of printers.conf.",
	         name, linenum);
    }
  }

  cupsFileClose(fp);
}


/*
 * 'SaveAllPrinters()' - Save all printer definitions to the printers.conf
 *                       file.
 */

void
SaveAllPrinters(void)
{
  int		i;			/* Looping var */
  cups_file_t	*fp;			/* printers.conf file */
  char		temp[1024];		/* Temporary string */
  char		backup[1024];		/* printers.conf.O file */
  printer_t	*printer;		/* Current printer class */
  time_t	curtime;		/* Current time */
  struct tm	*curdate;		/* Current date */


 /*
  * Create the printers.conf file...
  */

  snprintf(temp, sizeof(temp), "%s/printers.conf", ServerRoot);
  snprintf(backup, sizeof(backup), "%s/printers.conf.O", ServerRoot);

  if (rename(temp, backup))
    LogMessage(L_ERROR, "Unable to backup printers.conf - %s", strerror(errno));

  if ((fp = cupsFileOpen(temp, "w")) == NULL)
  {
    LogMessage(L_ERROR, "Unable to save printers.conf - %s", strerror(errno));

    if (rename(backup, temp))
      LogMessage(L_ERROR, "Unable to restore printers.conf - %s", strerror(errno));
    return;
  }
  else
    LogMessage(L_INFO, "Saving printers.conf...");

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
  strftime(temp, sizeof(temp) - 1, CUPS_STRFTIME_FORMAT, curdate);

  cupsFilePuts(fp, "# Printer configuration file for " CUPS_SVERSION "\n");
  cupsFilePrintf(fp, "# Written by cupsd on %s\n", temp);

 /*
  * Write each local printer known to the system...
  */

  for (printer = Printers; printer != NULL; printer = printer->next)
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

    cupsFilePrintf(fp, "QuotaPeriod %d\n", printer->quota_period);
    cupsFilePrintf(fp, "PageLimit %d\n", printer->page_limit);
    cupsFilePrintf(fp, "KLimit %d\n", printer->k_limit);

    for (i = 0; i < printer->num_users; i ++)
      cupsFilePrintf(fp, "%sUser %s\n", printer->deny_users ? "Deny" : "Allow",
              printer->users[i]);

    cupsFilePrintf(fp, "OpPolicy %s\n", printer->op_policy);
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
 * 'SetPrinterAttrs()' - Set printer attributes based upon the PPD file.
 */

void
SetPrinterAttrs(printer_t *p)		/* I - Printer to setup */
{
  char		uri[HTTP_MAX_URI];	/* URI for printer */
  char		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  int		i;			/* Looping var */
  char		filename[1024];		/* Name of PPD file */
  int		num_media;		/* Number of media options */
  location_t	*auth;			/* Pointer to authentication element */
  const char	*auth_supported;	/* Authentication supported */
  cups_ptype_t	printer_type;		/* Printer type data */
  ppd_file_t	*ppd;			/* PPD file data */
  ppd_option_t	*input_slot,		/* InputSlot options */
		*media_type,		/* MediaType options */
		*page_size,		/* PageSize options */
		*output_bin,		/* OutputBin options */
		*media_quality;		/* EFMediaQualityMode options */
  ppd_attr_t	*ppdattr;		/* PPD attribute */
  ipp_attribute_t *attr;		/* Attribute data */
  ipp_value_t	*val;			/* Attribute value */
  int		num_finishings;
  ipp_finish_t	finishings[5];		/* finishings-supported values */
  static const char * const sides[3] =	/* sides-supported values */
		{
		  "one",
		  "two-long-edge",
		  "two-short-edge"
		};


  DEBUG_printf(("SetPrinterAttrs: entering name = %s, type = %x\n", p->name,
                p->type));

 /*
  * Make sure that we have the common attributes defined...
  */

  if (!CommonData)
    CreateCommonData();

 /*
  * Clear out old filters and add a filter from application/vnd.cups-raw to
  * printer/name to handle "raw" printing by users.
  */

  DeletePrinterFilters(p);
  AddPrinterFilter(p, "application/vnd.cups-raw 0 -");

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

    if ((auth = FindBest(resource, HTTP_POST)) != NULL)
    {
      if (auth->type == AUTH_BASIC || auth->type == AUTH_BASICDIGEST)
	auth_supported = "basic";
      else if (auth->type == AUTH_DIGEST)
	auth_supported = "digest";
    }
  }

 /*
  * Create the required IPP attributes for a printer...
  */

  if (p->attrs)
    ippDelete(p->attrs);

  p->attrs = ippNew();

  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri-supported",
               NULL, p->uri);
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

  printer_type = p->type;

  p->raw = 0;

  if (p->type & CUPS_PRINTER_REMOTE)
  {
   /*
    * Tell the client this is a remote printer of some type...
    */

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
          SetString(&p->make_model, ppd->nickname);
	else if (ppd->modelname)
          SetString(&p->make_model, ppd->modelname);
	else
	  SetString(&p->make_model, "Bad PPD File");

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
	  LogMessage(L_CRIT, "SetPrinterAttrs: The PPD file for printer %s "
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
	* Add any filters in the PPD file...
	*/

	DEBUG_printf(("ppd->num_filters = %d\n", ppd->num_filters));
	for (i = 0; i < ppd->num_filters; i ++)
	{
          DEBUG_printf(("ppd->filters[%d] = \"%s\"\n", i, ppd->filters[i]));
          AddPrinterFilter(p, ppd->filters[i]);
	}

	if (ppd->num_filters == 0)
          AddPrinterFilter(p, "application/vnd.cups-postscript 0 -");

	ppdClose(ppd);

        printer_type = p->type;
      }
      else if (access(filename, 0) == 0)
      {
        int		pline;			/* PPD line number */
	ppd_status_t	pstatus;		/* PPD load status */


        pstatus = ppdLastError(&pline);

	LogMessage(L_ERROR, "PPD file for %s cannot be loaded!", p->name);

	if (pstatus <= PPD_ALLOC_ERROR)
	  LogMessage(L_ERROR, "%s", strerror(errno));
        else
	  LogMessage(L_ERROR, "%s on line %d.", ppdErrorString(pstatus),
	             pline);

        LogMessage(L_INFO, "Hint: Run \"cupstestppd %s\" and fix any errors.",
	           filename);

	AddPrinterFilter(p, "application/vnd.cups-postscript 0 -");
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
	  AddPrinterFilter(p, filename);
	}
	else if (p->device_uri &&
	         strncmp(p->device_uri, "ipp://", 6) == 0 &&
	         (strstr(p->device_uri, "/printers/") != NULL ||
		  strstr(p->device_uri, "/classes/") != NULL))
        {
	 /*
	  * Tell the client this is really a hard-wired remote printer.
	  */

          printer_type |= CUPS_PRINTER_REMOTE;

         /*
	  * Reset the printer-uri-supported attribute to point at the
	  * remote printer...
	  */

	  attr = ippFindAttribute(p->attrs, "printer-uri-supported", IPP_TAG_URI);
	  free(attr->values[0].string.text);
	  attr->values[0].string.text = strdup(p->device_uri);

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

  ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-type",
                printer_type);

  DEBUG_printf(("SetPrinterAttrs: leaving name = %s, type = %x\n", p->name,
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
 * 'SetPrinterReasons()' - Set/update the reasons strings.
 */

void
SetPrinterReasons(printer_t  *p,	/* I - Printer */
                  const char *s)	/* I - Reasons strings */
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
 * 'SetPrinterState()' - Update the current state of a printer.
 */

void
SetPrinterState(printer_t    *p,	/* I - Printer to change */
                ipp_pstate_t s,		/* I - New state */
		int          update)	/* I - Update printers.conf? */
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

  AddPrinterHistory(p);

 /*
  * Save the printer configuration if a printer goes from idle or processing
  * to stopped (or visa-versa)...
  */

  if ((old_state == IPP_PRINTER_STOPPED) != (s == IPP_PRINTER_STOPPED) &&
      update)
  {
    if (p->type & CUPS_PRINTER_CLASS)
      SaveAllClasses();
    else
      SaveAllPrinters();
  }
}


/*
 * 'SortPrinters()' - Sort the printer list when a printer name is changed.
 */

void
SortPrinters(void)
{
  printer_t	*current,	/* Current printer */
		*prev,		/* Previous printer */
		*next;		/* Next printer */
  int		did_swap;	/* Non-zero if we did a swap */


  do
  {
    for (did_swap = 0, current = Printers, prev = NULL; current != NULL;)
      if (current->next == NULL)
	break;
      else if (strcasecmp(current->name, current->next->name) > 0)
      {
	DEBUG_printf(("Swapping %s and %s...\n", current->name,
                      current->next->name));

       /*
	* Need to swap these two printers...
	*/

        did_swap = 1;

	if (prev == NULL)
          Printers = current->next;
	else
          prev->next = current->next;

       /*
	* Yes, we can all get a headache from the next bunch of pointer
	* swapping...
	*/

	next          = current->next;
	current->next = next->next;
	next->next    = current;
	prev          = next;
      }
      else
      {
        prev    = current;
	current = current->next;
      }
  }
  while (did_swap);
}


/*
 * 'StopPrinter()' - Stop a printer from printing any jobs...
 */

void
StopPrinter(printer_t *p,		/* I - Printer to stop */
            int       update)		/* I - Update printers.conf? */
{
  job_t	*job;				/* Active print job */


 /*
  * Set the printer state...
  */

  SetPrinterState(p, IPP_PRINTER_STOPPED, update);

 /*
  * See if we have a job printing on this printer...
  */

  if (p->job)
  {
   /*
    * Get pointer to job...
    */

    job = (job_t *)p->job;

   /*
    * Stop it...
    */

    StopJob(job->id, 0);

   /*
    * Reset the state to pending...
    */

    job->state->values[0].integer = IPP_JOB_PENDING;

    SaveJob(job->id);
  }
}


/*
 * 'ValidateDest()' - Validate a printer/class destination.
 */

const char *				/* O - Printer or class name */
ValidateDest(const char   *hostname,	/* I - Host name */
             const char   *resource,	/* I - Resource name */
             cups_ptype_t *dtype,	/* O - Type (printer or class) */
	     printer_t    **printer)	/* O - Printer pointer */
{
  printer_t	*p;			/* Current printer */
  char		localname[1024],	/* Localized hostname */
		*lptr,			/* Pointer into localized hostname */
		*sptr;			/* Pointer into server name */


  DEBUG_printf(("ValidateDest(\"%s\", \"%s\", %p, %p)\n", hostname, resource,
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

  if (strncmp(resource, "/classes/", 9) == 0)
  {
   /*
    * Class...
    */

    resource += 9;
  }
  else if (strncmp(resource, "/printers/", 10) == 0)
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

  if ((p = FindPrinter(resource)) == NULL)
    p = FindClass(resource);

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

  if (strcasecmp(hostname, "localhost") == 0)
    hostname = ServerName;

  strlcpy(localname, hostname, sizeof(localname));

  if (strcasecmp(hostname, ServerName) != 0)
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
	if (strcasecmp(lptr, sptr) == 0)
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

  for (p = Printers; p != NULL; p = p->next)
    if (strcasecmp(p->hostname, localname) == 0 &&
        strcasecmp(p->name, resource) == 0)
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
 * 'WritePrintcap()' - Write a pseudo-printcap file for older applications
 *                     that need it...
 */

void
WritePrintcap(void)
{
  cups_file_t	*fp;		/* printcap file */
  printer_t	*p;		/* Current printer */


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

  if (!Printcap[0])
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

	for (p = Printers; p != NULL; p = p->next)
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
	for (p = Printers; p != NULL; p = p->next)
	  cupsFilePrintf(fp, "%s%c", p->name, p->next ? ',' : '\n');

        if (DefaultPrinter)
	  cupsFilePrintf(fp, "_default:use=%s\n", DefaultPrinter->name);

	for (p = Printers; p != NULL; p = p->next)
	  cupsFilePrintf(fp, "%s:\\\n"
	              "\t:bsdaddr=%s,%s:\\\n"
		      "\t:description=%s:\n",
		  p->name, ServerName, p->name, p->info ? p->info : "");
        break;
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

    cups_strcpy(start, ptr + 1);
  }

 /*
  * Return the new device URI...
  */

  return (buffer);
}


#ifdef __sgi
/*
 * 'write_irix_config()' - Update the config files used by the IRIX
 *                         desktop tools.
 */

static void
write_irix_config(printer_t *p)		/* I - Printer to update */
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


/*
 * 'write_irix_state()' - Update the status files used by IRIX printing
 *                        desktop tools.
 */

static void
write_irix_state(printer_t *p)		/* I - Printer to update */
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
 * End of "$Id: printers.c,v 1.93.2.69 2004/10/04 20:24:17 mike Exp $".
 */
