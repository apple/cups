/*
 * "$Id: printers.c,v 1.11 1999/04/21 19:33:16 mike Exp $"
 *
 *   for the Common UNIX Printing System (CUPS).
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
 *   AddPrinter()        - Add a printer to the system.
 *   DeleteAllPrinters() - Delete all printers from the system.
 *   DeletePrinter()     - Delete a printer from the system.
 *   FindPrinter()       - Find a printer in the list.
 *   LoadAllPrinters()   - Load printers from the printers.conf file.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * Local functions...
 */

static void	set_printer_attrs(printer_t *);


/*
 * 'AddPrinter()' - Add a printer to the system.
 */

printer_t *			/* O - New printer */
AddPrinter(char *name)		/* I - Name of printer */
{
  printer_t	*p,		/* New printer */
		*current,	/* Current printer in list */
		*prev;		/* Previous printer in list */


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
  p->state     = IPP_PRINTER_STOPPED;
  p->accepting = 1;
  set_printer_attrs(p);

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

  return (p);
}


/*
 * 'DeleteAllPrinters()' - Delete all printers from the system.
 */

void
DeleteAllPrinters(void)
{
  while (Printers != NULL)
    DeletePrinter(Printers);
}


/*
 * 'DeletePrinter()' - Delete a printer from the system.
 */

void
DeletePrinter(printer_t *p)	/* I - Printer to delete */
{
  printer_t	*current,	/* Current printer in list */
		*prev;		/* Previous printer in list */


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
    fputs("cupsd: WARNING - tried to delete a non-existent printer!\n", stderr);
    return;
  }

  if (prev == NULL)
    Printers = p->next;
  else
    prev->next = p->next;

  free(p);
}


/*
 * 'FindPrinter()' - Find a printer in the list.
 */

printer_t *			/* O - Printer in list */
FindPrinter(char *name)		/* I - Name of printer to find */
{
  printer_t	*p;		/* Current printer */


  for (p = Printers; p != NULL; p = p->next)
    switch (strcasecmp(name, p->name))
    {
      case 1 : /* name > p->name */
          break;
      case 0 : /* name == p->name */
          return (p);
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
		super[MIME_MAX_SUPER],	/* Super-type name */
		type[MIME_MAX_TYPE],	/* Type name */
		*temp,			/* Temporary pointer */
		*filter;		/* Filter program */
  mime_type_t	**temptype;		/* MIME type looping var */
  int		cost;			/* Cost of filter */
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

  DefaultPrinter[0] = '\0';

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
        line[len - 1] = '\0';

        p           = AddPrinter(value);
	p->filetype = mimeAddType(MimeDatabase, "printer", value);

        if (strcmp(name, "<DefaultPrinter") == 0)
	  strcpy(DefaultPrinter, value);
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
        p = NULL;
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
    else if (strcmp(name, "Username") == 0)
      strncpy(p->username, value, sizeof(p->username) - 1);
    else if (strcmp(name, "Password") == 0)
      strncpy(p->password, value, sizeof(p->password) - 1);
    else if (strcmp(name, "AddFilter") == 0)
    {
     /*
      * Get the source super-type and type names from the beginning of
      * the value.
      */

      lineptr = value;
      temp    = super;

      while (*lineptr != '/' && *lineptr != '\n' && *lineptr != '\0' &&
             (temp - super + 1) < MIME_MAX_SUPER)
	*temp++ = tolower(*lineptr++);

      *temp = '\0';

      if (*lineptr != '/')
	continue;

      lineptr ++;
      temp = type;

      while (*lineptr != ' ' && *lineptr != '\t' && *lineptr != '\n' &&
             *lineptr != '\0' && (temp - type + 1) < MIME_MAX_TYPE)
	*temp++ = tolower(*lineptr++);

      *temp = '\0';

     /*
      * Then get the cost and filter program...
      */

      while (*lineptr == ' ' || *lineptr == '\t')
	lineptr ++;

      if (*lineptr < '0' || *lineptr > '9')
	continue;

      cost = atoi(lineptr);

      while (*lineptr != ' ' && *lineptr != '\t' && *lineptr != '\0')
	lineptr ++;
      while (*lineptr == ' ' || *lineptr == '\t')
	lineptr ++;

      if (*lineptr == '\0' || *lineptr == '\n')
	continue;

      filter = lineptr;
      if (filter[strlen(filter) - 1] == '\n')
	filter[strlen(filter) - 1] = '\0';

     /*
      * Add the filter to the MIME database, supporting wildcards as needed...
      */

      for (temptype = MimeDatabase->types, i = 0;
           i < MimeDatabase->num_types;
	   i ++, temptype ++)
	if ((super[0] == '*' || strcmp((*temptype)->super, super) == 0) &&
            (type[0] == '*' || strcmp((*temptype)->type, type) == 0))
	  mimeAddFilter(MimeDatabase, *temptype, p->filetype, cost, filter);
    }
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

    /**** Add Order, Deny, Allow, AuthType, and AuthClass stuff! ****/
  }

  if (DefaultPrinter[0] == '\0')
    strcpy(DefaultPrinter, Printers->name);

  fclose(fp);
}


void
SaveAllPrinters(void)
{
}


/*
 * 'StartPrinter()' - Start printing jobs on a printer.
 */

void
StartPrinter(printer_t *p)	/* I - Printer to start */
{
  if (p->state == IPP_PRINTER_STOPPED)
    p->state = IPP_PRINTER_IDLE;

  CheckJobs();
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
 * 'set_printer_attrs()' - Set printer attributes based upon the PPD file.
 */

static void
set_printer_attrs(printer_t *p)	/* I - Printer to setup */
{
  char		uri[HTTP_MAX_URI];/* URI for printer */
  int		i;		/* Looping var */
  char		filename[1024];	/* Name of PPD file */
  int		num_media;	/* Number of media options */
  ppd_file_t	*ppd;		/* PPD file data */
  ppd_option_t	*input_slot,	/* InputSlot options */
		*media_type,	/* MediaType options */
		*page_size;	/* PageSize options */
  ipp_attribute_t *attr;	/* Attribute data */
  ipp_value_t	*val;		/* Attribute value */
  int		nups[3] =	/* number-up-supported values */
		{ 1, 2, 4 };
  ipp_orient_t	orients[4] =	/* orientation-requested-supported values */
		{
		  IPP_PORTRAIT,
		  IPP_LANDSCAPE,
		  IPP_REVERSE_LANDSCAPE,
		  IPP_REVERSE_PORTRAIT
		};
  char		*sides[3] =	/* sides-supported values */
		{
		  "one",
		  "two-long-edge",
		  "two-short-edge"
		};
  ipp_op_t	ops[] =		/* operations-supported values */
		{
		  IPP_PRINT_JOB,
		  IPP_VALIDATE_JOB,
		  IPP_CANCEL_JOB,
		  IPP_GET_JOB_ATTRIBUTES,
		  IPP_GET_JOBS,
		  IPP_GET_PRINTER_ATTRIBUTES,
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
		  CUPS_REJECT_JOBS
		};
  char		*charsets[] =	/* charset-supported values */
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


 /*
  * Create the required IPP attributes for a printer...
  */

  if (p->attrs)
    ippDelete(p->attrs);

  p->attrs = ippNew();

  sprintf(uri, "ipp://%s:%d/printers/%s", ServerName,
          ntohs(Listeners[0].address.sin_port), p->name);
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri-supported",
               NULL, uri);
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
                 sizeof(ops) / sizeof(ops[0]), ops);
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
  ippAddRange(p->attrs, IPP_TAG_PRINTER, "copies-supported", 1, 100);
  ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "copies-default", 1);
  ippAddBoolean(p->attrs, IPP_TAG_PRINTER, "page-ranges-supported", 1);
  ippAddIntegers(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                 "number-up-supported", 3, nups);
  ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "number-up-default", 1);
  ippAddIntegers(p->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                 "orientation-requested-supported", 4, orients);
  ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                "orientation-requested-default", IPP_PORTRAIT);

 /*
  * Assign additional attributes from the PPD file (if any)...
  */

  sprintf(filename, "%s/ppd/%s.ppd", ServerRoot, p->name);
  if ((ppd = ppdOpenFile(filename)) != NULL)
  {
   /*
    * Add make/model and other various attributes...
    */

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
      ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "sides-supported",
                    3, NULL, sides);
      ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "sides-default",
                   NULL, "one");
    }

    ppdClose(ppd);
  }
}


/*
 * End of "$Id: printers.c,v 1.11 1999/04/21 19:33:16 mike Exp $".
 */
