/*
 * "$Id: printers.c,v 1.49 2000/01/04 13:46:10 mike Exp $"
 *
 *   Printer routines for the Common UNIX Printing System (CUPS).
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
 *   AddPrinter()        - Add a printer to the system.
 *   DeleteAllPrinters() - Delete all printers from the system.
 *   DeletePrinter()     - Delete a printer from the system.
 *   FindPrinter()       - Find a printer in the list.
 *   LoadAllPrinters()   - Load printers from the printers.conf file.
 *   SaveAllPrinters()   - Save all printer definitions to the printers.conf
 *   SetPrinterAttrs()   - Set printer attributes based upon the PPD file.
 *   SetPrinterState()   - Update the current state of a printer.
 *   SortPrinters()      - Sort the printer list when a printer name is
 *                         changed.
 *   StopPrinter()       - Stop a printer from printing any jobs...
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * Local functions...
 */

static void	write_printcap(void);


/*
 * 'AddPrinter()' - Add a printer to the system.
 */

printer_t *			/* O - New printer */
AddPrinter(const char *name)	/* I - Name of printer */
{
  printer_t	*p,		/* New printer */
		*current,	/* Current printer in list */
		*prev;		/* Previous printer in list */


  DEBUG_printf(("AddPrinter(\"%s\")\n", name));

 /*
  * Range check input...
  */

  if (name == NULL)
    return (NULL);

 /*
  * Create a new printer entity...
  */

  if ((p = calloc(sizeof(printer_t), 1)) == NULL)
    return (NULL);

  strcpy(p->name, name);
  strcpy(p->hostname, ServerName);
  sprintf(p->uri, "ipp://%s:%d/printers/%s", ServerName,
          ntohs(Listeners[0].address.sin_port), name);

  p->state     = IPP_PRINTER_STOPPED;
  p->accepting = 0;
  p->filetype  = mimeAddType(MimeDatabase, "printer", name);

 /*
  * Setup required filters and IPP attributes...
  */

  SetPrinterAttrs(p);

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

  write_printcap();

  return (p);
}


/*
 * 'AddPrinterFilter()' - Add a MIME filter for a printer.
 */

void
AddPrinterFilter(printer_t *p,		/* I - Printer to add to */
                 char      *filter)	/* I - Filter to add */
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

  if (p == NULL || filter == NULL)
    return;

 /*
  * Parse the filter string; it should be in the following format:
  *
  *     super/type cost program
  */

  if (sscanf(filter, "%15[^/]/%31s%d%1023s", super, type, &cost, program) != 4)
  {
    LogMessage(LOG_ERROR, "AddPrinterFilter: Invalid filter string \"%s\"!",
               filter);
    return;
  }

 /*
  * Add the filter to the MIME database, supporting wildcards as needed...
  */

  for (temptype = MimeDatabase->types, i = MimeDatabase->num_types;
       i > 0;
       i --, temptype ++)
    if (((super[0] == '*' && strcmp((*temptype)->super, "printer") != 0) ||
         strcmp((*temptype)->super, super) == 0) &&
        (type[0] == '*' || strcmp((*temptype)->type, type) == 0))
    {
      LogMessage(LOG_DEBUG, "Adding filter %s/%s %s/%s %d %s",
                 (*temptype)->super, (*temptype)->type,
		 p->filetype->super, p->filetype->type,
                 cost, program);
      mimeAddFilter(MimeDatabase, *temptype, p->filetype, cost, program);
    }
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
      DeletePrinter(p);
  }
}


/*
 * 'DeletePrinter()' - Delete a printer from the system.
 */

void
DeletePrinter(printer_t *p)	/* I - Printer to delete */
{
  int		i;		/* Looping var */
  printer_t	*current,	/* Current printer in list */
		*prev;		/* Previous printer in list */
#ifdef __sgi
  char		filename[1024];	/* Interface script filename */
#endif /* __sgi */


  DEBUG_printf(("DeletePrinter(%08x): p->name = \"%s\"...\n", p, p->name));

 /*
  * Range check input...
  */

  if (p == NULL)
    return;

 /*
  * Stop printing on this printer...
  */

  StopPrinter(p);

 /*
  * Remove the printer from the list...
  */

  for (prev = NULL, current = Printers;
       current != NULL;
       prev = current, current = current->next)
    if (p == current)
      break;

  if (current == NULL)
  {
    LogMessage(LOG_ERROR, "Tried to delete a non-existent printer %s!\n",
               p->name);
    return;
  }

  if (prev == NULL)
    Printers = p->next;
  else
    prev->next = p->next;

  if (p->printers != NULL)
    free(p->printers);

  ippDelete(p->attrs);

  free(p);

 /*
  * If p is the default printer, assign the next one...
  */

  if (p == DefaultPrinter)
    DefaultPrinter = Printers;

 /*
  * Write a new /etc/printcap file, and delete the dummy interface and GUI
  * scripts to fool SGI's stupid printing tools.
  */

  write_printcap();

#ifdef __sgi
  sprintf(filename, "/var/spool/lp/interface/%s", p->name);
  unlink(filename);

  sprintf(filename, "/var/spool/lp/gui_interface/ELF/%s.gui", p->name);
  unlink(filename);

  sprintf(filename, "/var/spool/lp/activeicons/%s", p->name);
  unlink(filename);
#endif /* __sgi */
}


/*
 * 'AddPrinterFilter()' - Add a MIME filter for a printer.
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
        memcpy(filter, filter + 1, sizeof(mime_filter_t) * (i - 1));

      filter --;
    }
}


/*
 * 'FindPrinter()' - Find a printer in the list.
 */

printer_t *			/* O - Printer in list */
FindPrinter(const char *name)	/* I - Name of printer to find */
{
  printer_t	*p;		/* Current printer */


  for (p = Printers; p != NULL; p = p->next)
    switch (strcasecmp(name, p->name))
    {
      case 0 : /* name == p->name */
          if (!(p->type & CUPS_PRINTER_CLASS))
	    return (p);
      case 1 : /* name > p->name */
          break;
      case -1 : /* name < p->name */
          return (NULL);
    }

  return (NULL);
}


/*
 * 'LoadAllPrinters()' - Load printers from the printers.conf file.
 */

void
LoadAllPrinters(void)
{
  FILE		*fp;			/* printers.conf file */
  int		i;			/* Looping var */
  int		linenum;		/* Current line number */
  int		len;			/* Length of line */
  char		line[HTTP_MAX_BUFFER],	/* Line from file */
		name[256],		/* Parameter name */
		*nameptr,		/* Pointer into name */
		*value,			/* Pointer to value */
		*lineptr,		/* Pointer in line */
		*temp;			/* Temporary pointer */
  printer_t	*p;			/* Current printer */


 /*
  * Open the printer.conf file...
  */

  sprintf(line, "%s/conf/printers.conf", ServerRoot);
  if ((fp = fopen(line, "r")) == NULL)
    return;

 /*
  * Read printer configurations until we hit EOF...
  */

  linenum = 0;
  p       = NULL;

  while (fgets(line, sizeof(line), fp) != NULL)
  {
    linenum ++;

   /*
    * Skip comment lines...
    */

    if (line[0] == '#')
      continue;

   /*
    * Strip trailing newline, if any...
    */

    len = strlen(line);

    if (line[len - 1] == '\n')
    {
      len --;
      line[len] = '\0';
    }

   /*
    * Extract the name from the beginning of the line...
    */

    for (value = line; isspace(*value); value ++);

    for (nameptr = name; *value != '\0' && !isspace(*value);)
      *nameptr++ = *value++;
    *nameptr = '\0';

    while (isspace(*value))
      value ++;

    if (name[0] == '\0')
      continue;

   /*
    * Decode the directive...
    */

    if (strcmp(name, "<Printer") == 0 ||
        strcmp(name, "<DefaultPrinter") == 0)
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

        p = AddPrinter(value);
	p->accepting = 1;
	p->state     = IPP_PRINTER_IDLE;

       /*
        * Set the default printer as needed...
	*/

        if (strcmp(name, "<DefaultPrinter") == 0)
	  DefaultPrinter = p;
      }
      else
      {
        LogMessage(LOG_ERROR, "Syntax error on line %d of printers.conf.",
	           linenum);
        return;
      }
    }
    else if (strcmp(name, "</Printer>") == 0)
    {
      if (p != NULL)
      {
        SetPrinterAttrs(p);
        p = NULL;
      }
      else
      {
        LogMessage(LOG_ERROR, "Syntax error on line %d of printers.conf.",
	           linenum);
        return;
      }
    }
    else if (p == NULL)
    {
      LogMessage(LOG_ERROR, "Syntax error on line %d of printers.conf.",
	         linenum);
      return;
    }
    
    else if (strcmp(name, "Info") == 0)
      strncpy(p->info, value, sizeof(p->info) - 1);
    else if (strcmp(name, "MoreInfo") == 0)
      strncpy(p->more_info, value, sizeof(p->more_info) - 1);
    else if (strcmp(name, "Location") == 0)
      strncpy(p->location, value, sizeof(p->location) - 1);
    else if (strcmp(name, "DeviceURI") == 0)
      strncpy(p->device_uri, value, sizeof(p->device_uri) - 1);
    else if (strcmp(name, "State") == 0)
    {
     /*
      * Set the initial queue state...
      */

      if (strcasecmp(value, "idle") == 0)
        p->state = IPP_PRINTER_IDLE;
      else if (strcasecmp(value, "stopped") == 0)
        p->state = IPP_PRINTER_STOPPED;
    }
    else if (strcmp(name, "Accepting") == 0)
    {
     /*
      * Set the initial accepting state...
      */

      if (strcasecmp(value, "yes") == 0)
        p->accepting = 1;
      else
        p->accepting = 0;
    }
    else
    {
     /*
      * Something else we don't understand...
      */

      LogMessage(LOG_ERROR, "Unknown configuration directive %s on line %d of printers.conf.",
	         name, linenum);
    }
  }

  fclose(fp);
}


/*
 * 'SaveAllPrinters()' - Save all printer definitions to the printers.conf
 *                       file.
 */

void
SaveAllPrinters(void)
{
  FILE		*fp;			/* printers.conf file */
  char		temp[1024];		/* Temporary string */
  printer_t	*printer;		/* Current printer class */
  int		i;			/* Looping var */
  time_t	curtime;		/* Current time */
  struct tm	*curdate;		/* Current date */


 /*
  * Create the printers.conf file...
  */

  sprintf(temp, "%s/conf/printers.conf", ServerRoot);
  if ((fp = fopen(temp, "w")) == NULL)
  {
    LogMessage(LOG_ERROR, "Unable to save printers.conf - %s", strerror(errno));
    return;
  }
  else
    LogMessage(LOG_INFO, "Saving printers.conf...");

 /*
  * Write a small header to the file...
  */

  curtime = time(NULL);
  curdate = gmtime(&curtime);
  strftime(temp, sizeof(temp) - 1, "# Written by cupsd on %c\n", curdate);

  fputs("# Printer configuration file for " CUPS_SVERSION "\n", fp);
  fputs(temp, fp);

 /*
  * Write each local printer known to the system...
  */

  for (printer = Printers; printer != NULL; printer = printer->next)
  {
   /*
    * Skip remote destinations and printer classes...
    */

    if ((printer->type & CUPS_PRINTER_REMOTE) ||
        (printer->type & CUPS_PRINTER_CLASS))
      continue;

   /*
    * Write printers as needed...
    */

    if (printer == DefaultPrinter)
      fprintf(fp, "<DefaultPrinter %s>\n", printer->name);
    else
      fprintf(fp, "<Printer %s>\n", printer->name);

    if (printer->info[0])
      fprintf(fp, "Info %s\n", printer->info);
    if (printer->more_info[0])
      fprintf(fp, "MoreInfo %s\n", printer->more_info);
    if (printer->location[0])
      fprintf(fp, "Location %s\n", printer->location);
    if (printer->device_uri[0])
      fprintf(fp, "DeviceURI %s\n", printer->device_uri);
    if (printer->state == IPP_PRINTER_STOPPED)
      fputs("State Stopped\n", fp);
    else
      fputs("State Idle\n", fp);
    if (printer->accepting)
      fputs("Accepting Yes\n", fp);
    else
      fputs("Accepting No\n", fp);

    fputs("</Printer>\n", fp);
  }

  fclose(fp);
}


/*
 * 'SetPrinterAttrs()' - Set printer attributes based upon the PPD file.
 */

void
SetPrinterAttrs(printer_t *p)		/* I - Printer to setup */
{
  char		uri[HTTP_MAX_URI];	/* URI for printer */
  char		method[HTTP_MAX_URI],	/* Method portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  int		port;			/* Port portion of URI */
  int		i;			/* Looping var */
  char		filename[1024];		/* Name of PPD file */
  int		num_media;		/* Number of media options */
  ppd_file_t	*ppd;			/* PPD file data */
  ppd_option_t	*input_slot,		/* InputSlot options */
		*media_type,		/* MediaType options */
		*page_size;		/* PageSize options */
  ipp_attribute_t *attr;		/* Attribute data */
  ipp_value_t	*val;			/* Attribute value */
  int		nups[3] =		/* number-up-supported values */
		{ 1, 2, 4 };
  ipp_orient_t	orients[4] =		/* orientation-requested-supported values */
		{
		  IPP_PORTRAIT,
		  IPP_LANDSCAPE,
		  IPP_REVERSE_LANDSCAPE,
		  IPP_REVERSE_PORTRAIT
		};
  const char	*sides[3] =		/* sides-supported values */
		{
		  "one",
		  "two-long-edge",
		  "two-short-edge"
		};
  ipp_op_t	ops[] =			/* operations-supported values */
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
		  CUPS_GET_PPDS
		};
  const char	*charsets[] =		/* charset-supported values */
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
		  "utf-8"
		};
  int		num_finishings;
  ipp_finish_t	finishings[5];
#ifdef __sgi
  FILE		*fp;		/* Interface script file */
#endif /* __sgi */


  DEBUG_printf(("SetPrinterAttrs: entering name = %s, type = %x\n", p->name,
                p->type));

 /*
  * Clear out old filters and add a filter from application/vnd.cups-raw to
  * printer/name to handle "raw" printing by users.
  */

  DeletePrinterFilters(p);
  AddPrinterFilter(p, "application/vnd.cups-raw 0 -");

 /*
  * Create the required IPP attributes for a printer...
  */

  if (p->attrs)
    ippDelete(p->attrs);

  p->attrs = ippNew();

  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri-supported",
               NULL, p->uri);
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
               "uri-security-supported", NULL, "none");
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-name", NULL,
               p->name);
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-location",
               NULL, p->location);
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info",
               NULL, p->info);
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-more-info",
               NULL, p->more_info);
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
               "pdl-override-supported", NULL, "not-attempted");
  ippAddIntegers(p->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "operations-supported",
                 sizeof(ops) / sizeof(ops[0]), (int *)ops);
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_CHARSET, "charset-configured",
               NULL, DefaultCharset);
  ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_CHARSET, "charset-supported",
                sizeof(charsets) / sizeof(charsets[0]), NULL, charsets);
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_LANGUAGE,
               "natural-language-configured", NULL, DefaultLanguage);
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_LANGUAGE,
               "generated-natural-language-supported", NULL, DefaultLanguage);
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_MIMETYPE,
               "document-format-default", NULL, "application/octet-stream");
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_MIMETYPE,
               "document-format-supported", NULL, "application/octet-stream");
  ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "job-priority-supported", 100);
  ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "job-priority-default", 50);
  ippAddRange(p->attrs, IPP_TAG_PRINTER, "copies-supported", 1, 65535);
  ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "copies-default", 1);
  ippAddBoolean(p->attrs, IPP_TAG_PRINTER, "page-ranges-supported", 1);
  ippAddIntegers(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                 "number-up-supported", 3, nups);
  ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "number-up-default", 1);
  ippAddIntegers(p->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                 "orientation-requested-supported", 4, (int *)orients);
  ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                "orientation-requested-default", IPP_PORTRAIT);

  if (p->type & CUPS_PRINTER_REMOTE)
  {
   /*
    * Tell the client this is a remote printer of some type...
    */

    if (p->type & CUPS_PRINTER_CLASS)
      snprintf(filename, sizeof(filename), "Remote Printer Class on %s",
               p->hostname);
    else
      snprintf(filename, sizeof(filename), "Remote Printer on %s", p->hostname);

    ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                 "printer-make-and-model", NULL, filename);
  }
  else
  {
   /*
    * Assign additional attributes depending on whether this is a printer
    * or class...
    */

    p->type &= ~CUPS_PRINTER_OPTIONS;

    if (p->type & CUPS_PRINTER_CLASS)
    {
     /*
      * Add class-specific attributes...
      */

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
          attr->values[i].string.text = strdup(p->printers[i]->uri);

	  p->type &= ~CUPS_PRINTER_OPTIONS | p->printers[i]->type;
        }

	attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME,
                             "member-names", p->num_printers, NULL, NULL);

	for (i = 0; i < p->num_printers; i ++)
          attr->values[i].string.text = strdup(p->printers[i]->name);
      }
    }
    else
    {
     /*
      * Add printer-specific attributes...  Start by sanitizing the device
      * URI so it doesn't have a username or password in it...
      */

      if (strstr(p->device_uri, "://") != NULL)
      {
       /*
        * http://..., ipp://..., etc.
	*/

        httpSeparate(p->device_uri, method, username, host, &port, resource);
	if (port)
	  sprintf(uri, "%s://%s:%d%s", method, host, port, resource);
	else
	  sprintf(uri, "%s://%s%s", method, host, resource);
      }
      else
      {
       /*
        * file:..., serial:..., etc.
	*/

        strcpy(uri, p->device_uri);
      }

      ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uri", NULL,
        	   uri);

     /*
      * Assign additional attributes from the PPD file (if any)...
      */

      p->type        |= CUPS_PRINTER_BW;
      finishings[0]  = IPP_FINISH_NONE;
      num_finishings = 1;

      sprintf(filename, "%s/ppd/%s.ppd", ServerRoot, p->name);
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

	ippAddBoolean(p->attrs, IPP_TAG_PRINTER, "color-supported",
                      ppd->color_device);
	ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                     "printer-make-and-model", NULL, ppd->nickname);

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

	attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                             "media-supported", num_media, NULL, NULL);
	val  = attr->values;

	if (input_slot != NULL)
	  for (i = 0; i < input_slot->num_choices; i ++, val ++)
	    val->string.text = strdup(input_slot->choices[i].choice);

	if (media_type != NULL)
	  for (i = 0; i < media_type->num_choices; i ++, val ++)
	    val->string.text = strdup(media_type->choices[i].choice);

	if (page_size != NULL)
	  for (i = 0; i < page_size->num_choices; i ++, val ++)
	    val->string.text = strdup(page_size->choices[i].choice);

	ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-default",
                     NULL, page_size->defchoice);

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
	  finishings[num_finishings++] = IPP_FINISH_STAPLE;
	}

	if (ppdFindOption(ppd, "BindEdge") != NULL)
	{
	  p->type |= CUPS_PRINTER_BIND;
	  finishings[num_finishings++] = IPP_FINISH_BIND;
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
      }
      else if (access(filename, 0) == 0)
      {
	LogMessage(LOG_ERROR, "PPD file for %s cannot be loaded!", p->name);

	AddPrinterFilter(p, "application/vnd.cups-postscript 0 -");
      }
      else
      {
       /*
	* If we have an interface script, add a filter entry for it...
	*/

	sprintf(filename, "%s/interfaces/%s", ServerRoot, p->name);
	if (access(filename, X_OK) == 0)
	{
	 /*
	  * Yes, we have a System V style interface script; use it!
	  */

	  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                       "printer-make-and-model", NULL, "Local System V Printer");

	  sprintf(filename, "*/* 0 %s/interfaces/%s", ServerRoot, p->name);
	  AddPrinterFilter(p, filename);
	}
	else
	{
	 /*
          * Otherwise we have neither - treat this as a "dumb" printer
	  * with no PPD file...
	  */

	  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                       "printer-make-and-model", NULL, "Local Raw Printer");

	  AddPrinterFilter(p, "*/* 0 -");
	}
      }

      ippAddIntegers(p->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                     "finishings-supported", num_finishings, (int *)finishings);
      ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                    "finishings-default", IPP_FINISH_NONE);
    }
  }

 /*
  * Add the CUPS-specific printer-type attribute...
  */

  ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-type", p->type);

  DEBUG_printf(("SetPrinterAttrs: leaving name = %s, type = %x\n", p->name,
                p->type));

#ifdef __sgi
 /*
  * Add dummy interface and GUI scripts to fool SGI's "challenged" printing
  * tools.
  */

  sprintf(filename, "/var/spool/lp/interface/%s", p->name);
  if ((fp = fopen(filename, "w")) != NULL)
  {
    fputs("#!/bin/sh\n", fp);

    if ((attr = ippFindAttribute(p->attrs, "printer-make-and-model",
                                 IPP_TAG_TEXT)) != NULL)
      fprintf(fp, "NAME=\"%s\"\n", attr->values[0].string.text);
    else if (p->type & CUPS_PRINTER_CLASS)
      fputs("NAME=\"Printer Class\"\n", fp);
    else
      fputs("NAME=\"Remote Destination\"\n", fp);

    if (p->type & CUPS_PRINTER_COLOR)
      fputs("TYPE=ColorPostScript\n", fp);
    else
      fputs("TYPE=PostScript\n", fp);

    fclose(fp);
    chmod(filename, 0755);
  }

  sprintf(filename, "/var/spool/lp/member/%s", p->name);
  if ((fp = fopen(filename, "w")) != NULL)
  {
    fputs("/dev/null\n", fp);
    fclose(fp);
    chmod(filename, 0644);
  }

  sprintf(filename, "/var/spool/lp/gui_interface/ELF/%s.gui", p->name);
  if ((fp = fopen(filename, "w")) != NULL)
  {
    fputs("#!/bin/sh\n", fp);
    fprintf(fp, "/usr/bin/glpoptions -d %s -o \"$3\"\n", p->name);
    fclose(fp);
    chmod(filename, 0755);
  }

  sprintf(filename, "/var/spool/lp/activeicons/%s", p->name);
  if ((fp = fopen(filename, "w")) != NULL)
  {
    fputs("#!/bin/sh\n", fp);
    if (p->type & CUPS_PRINTER_COLOR)
      fputs("#Tag 66240\n", fp);
    else
      fputs("#Tag 66208\n", fp);
    fclose(fp);
    chmod(filename, 0755);
  }
#endif /* __sgi */
}


/*
 * 'SetPrinterState()' - Update the current state of a printer.
 */

void
SetPrinterState(printer_t    *p,	/* I - Printer to change */
                ipp_pstate_t s)		/* I - New state */
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

  old_state      = p->state;
  p->state       = s;
  p->state_time  = time(NULL);

  if (old_state != s)
    p->browse_time = 0;

 /*
  * Save the printer configuration if a printer goes from idle or processing
  * to stopped (or visa-versa)...
  */

  if ((old_state == IPP_PRINTER_STOPPED) != (s == IPP_PRINTER_STOPPED))
    SaveAllPrinters();

 /*
  * Check to see if any pending jobs can now be printed...
  */

  CheckJobs();
}


/*
 * 'SortPrinters()' - Sort the printer list when a printer name is changed.
 */

void
SortPrinters(void)
{
  printer_t	*current,	/* Current printer */
 		*start,		/* Starting printer */
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
      }
      else
	current = current->next;
  }
  while (did_swap);
}


/*
 * 'StopPrinter()' - Stop a printer from printing any jobs...
 */

void
StopPrinter(printer_t *p)	/* I - Printer to stop */
{
  if (p->job)
    StopJob(((job_t *)p->job)->id);

  p->state = IPP_PRINTER_STOPPED;
}


/*
 * 'write_printcap()' - Write a pseudo-printcap file to /etc/printcap for
 *                      older applications that need it...
 */

static void
write_printcap(void)
{
  FILE		*fp;		/* printcap file */
  printer_t	*p;		/* Current printer */


 /*
  * See if we have a printcap file; if not, don't bother writing it.
  */

  if (access("/etc/printcap", 0))
    return;

 /*
  * Write a new /etc/printcap with the current list of printers. Each printer
  * is put in the file as:
  *
  *    Printer1:
  *    Printer2:
  *    Printer3:
  *    ...
  *    PrinterN:
  */

  if ((fp = fopen("/etc/printcap", "w")) == NULL)
    return;

  for (p = Printers; p != NULL; p = p->next)
    fprintf(fp, "%s:\n", p->name);

 /*
  * Close the file...
  */

  fclose(fp);
}


/*
 * End of "$Id: printers.c,v 1.49 2000/01/04 13:46:10 mike Exp $".
 */
