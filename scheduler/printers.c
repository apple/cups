/*
 * "$Id: printers.c,v 1.93.2.1 2001/04/02 19:51:50 mike Exp $"
 *
 *   Printer routines for the Common UNIX Printing System (CUPS).
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
 *   AddPrinter()           - Add a printer to the system.
 *   AddPrinterFilter()     - Add a MIME filter for a printer.
 *   AddPrinterUser()       - Add a user to the ACL.
 *   DeleteAllPrinters()    - Delete all printers from the system.
 *   DeletePrinter()        - Delete a printer from the system.
 *   DeletePrinterFilters() - Delete all MIME filters for a printer.
 *   FindPrinter()          - Find a printer in the list.
 *   FreePrinterUsers()     - Free allow/deny users.
 *   LoadAllPrinters()      - Load printers from the printers.conf file.
 *   SaveAllPrinters()      - Save all printer definitions to the printers.conf
 *   SetPrinterAttrs()      - Set printer attributes based upon the PPD file.
 *   SetPrinterState()      - Update the current state of a printer.
 *   SortPrinters()         - Sort the printer list when a printer name is
 *                            changed.
 *   StopPrinter()          - Stop a printer from printing any jobs...
 *   ValidateDest()         - Validate a printer/class destination.
 *   write_printcap()       - Write a pseudo-printcap file for older
 *                            applications that need it...
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

  if ((p = calloc(1, sizeof(printer_t))) == NULL)
    return (NULL);

  strncpy(p->name, name, sizeof(p->name) - 1);
  strncpy(p->hostname, ServerName, sizeof(p->hostname) - 1);

#ifdef AF_INET6
  if (Listeners[0].address.addr.sa_family == AF_INET6)
    snprintf(p->uri, sizeof(p->uri), "ipp://%s:%d/printers/%s", ServerName,
             ntohs(Listeners[0].address.ipv6.sin6_port), name);
  else
#endif /* AF_INET6 */
  snprintf(p->uri, sizeof(p->uri), "ipp://%s:%d/printers/%s", ServerName,
           ntohs(Listeners[0].address.ipv4.sin_port), name);

 /*
  * Since uri and more_info are URIs and use the HTTP_MAX_URI constant
  * to determine the size of the strings, this copy is safe.
  */

  strcpy(p->more_info, p->uri);

  p->state     = IPP_PRINTER_STOPPED;
  p->accepting = 0;
  p->filetype  = mimeAddType(MimeDatabase, "printer", name);

  if (Classification[0])
    strcpy(p->job_sheets[0], Classification);
  else
    strcpy(p->job_sheets[0], "none");

  strcpy(p->job_sheets[1], "none");

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

  if (p == NULL || filter == NULL)
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
    if (((super[0] == '*' && strcmp((*temptype)->super, "printer") != 0) ||
         strcmp((*temptype)->super, super) == 0) &&
        (type[0] == '*' || strcmp((*temptype)->type, type) == 0))
    {
      LogMessage(L_DEBUG2, "Adding filter %s/%s %s/%s %d %s",
                 (*temptype)->super, (*temptype)->type,
		 p->filetype->super, p->filetype->type,
                 cost, program);
      mimeAddFilter(MimeDatabase, *temptype, p->filetype, cost, program);
    }
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
  * Remove the printer from the list...
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

  if (prev == NULL)
    Printers = p->next;
  else
    prev->next = p->next;

 /*
  * Stop printing on this printer...
  */

  StopPrinter(p);

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
#endif /* __sgi */

 /*
  * If p is the default printer, assign the next one...
  */

  if (p == DefaultPrinter)
    DefaultPrinter = Printers;

 /*
  * Remove this printer from any classes...
  */

  if (!(p->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT)))
    DeletePrinterFromClasses(p);

 /*
  * Free all memory used by the printer...
  */

  if (p->printers != NULL)
    free(p->printers);

  ippDelete(p->attrs);

  DeletePrinterFilters(p);

  FreePrinterUsers(p);
  FreeQuotas(p);

  free(p);

 /*
  * Write a new /etc/printcap file...
  */

  write_printcap();
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
  FILE		*fp;			/* printers.conf file */
  int		linenum;		/* Current line number */
  int		len;			/* Length of line */
  char		line[1024],		/* Line from file */
		name[256],		/* Parameter name */
		*nameptr,		/* Pointer into name */
		*value,			/* Pointer to value */
		*valueptr;		/* Pointer into value */
  printer_t	*p;			/* Current printer */


 /*
  * Open the printer.conf file...
  */

  snprintf(line, sizeof(line), "%s/printers.conf", ServerRoot);
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

    for (nameptr = name; *value != '\0' && !isspace(*value) &&
                             nameptr < (name + sizeof(name) - 1);)
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
        LogMessage(L_ERROR, "Syntax error on line %d of printers.conf.",
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
    else if (strcmp(name, "Info") == 0)
      strncpy(p->info, value, sizeof(p->info) - 1);
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
    else if (strcmp(name, "StateMessage") == 0)
    {
     /*
      * Set the initial queue state message...
      */

      while (isspace(*value))
        value ++;

      strncpy(p->state_message, value, sizeof(p->state_message) - 1);
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
    else if (strcmp(name, "JobSheets") == 0)
    {
     /*
      * Set the initial job sheets...
      */

      for (valueptr = value; *valueptr && !isspace(*valueptr); valueptr ++);

      if (*valueptr)
        *valueptr++ = '\0';

      strncpy(p->job_sheets[0], value, sizeof(p->job_sheets[0]) - 1);

      while (isspace(*valueptr))
        valueptr ++;

      if (*valueptr)
      {
        for (value = valueptr; *valueptr && !isspace(*valueptr); valueptr ++);

	if (*valueptr)
          *valueptr++ = '\0';

	strncpy(p->job_sheets[1], value, sizeof(p->job_sheets[1]) - 1);
      }
    }
    else if (strcmp(name, "AllowUser") == 0)
    {
      p->deny_users = 0;
      AddPrinterUser(p, value);
    }
    else if (strcmp(name, "DenyUser") == 0)
    {
      p->deny_users = 1;
      AddPrinterUser(p, value);
    }
    else if (strcmp(name, "QuotaPeriod") == 0)
      p->quota_period = atoi(value);
    else if (strcmp(name, "PageLimit") == 0)
      p->page_limit = atoi(value);
    else if (strcmp(name, "KLimit") == 0)
      p->k_limit = atoi(value);
    else
    {
     /*
      * Something else we don't understand...
      */

      LogMessage(L_ERROR, "Unknown configuration directive %s on line %d of printers.conf.",
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
  int		i;			/* Looping var */
  FILE		*fp;			/* printers.conf file */
  char		temp[1024];		/* Temporary string */
  printer_t	*printer;		/* Current printer class */
  time_t	curtime;		/* Current time */
  struct tm	*curdate;		/* Current date */


 /*
  * Create the printers.conf file...
  */

  snprintf(temp, sizeof(temp), "%s/printers.conf", ServerRoot);
  if ((fp = fopen(temp, "w")) == NULL)
  {
    LogMessage(L_ERROR, "Unable to save printers.conf - %s", strerror(errno));
    return;
  }
  else
    LogMessage(L_INFO, "Saving printers.conf...");

 /*
  * Restrict access to the file...
  */

  fchown(fileno(fp), User, Group);
  fchmod(fileno(fp), 0600);

 /*
  * Write a small header to the file...
  */

  curtime = time(NULL);
  curdate = gmtime(&curtime);
  strftime(temp, sizeof(temp) - 1, CUPS_STRFTIME_FORMAT, curdate);

  fputs("# Printer configuration file for " CUPS_SVERSION "\n", fp);
  fprintf(fp, "# Written by cupsd on %s\n", temp);

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
      fprintf(fp, "<DefaultPrinter %s>\n", printer->name);
    else
      fprintf(fp, "<Printer %s>\n", printer->name);

    if (printer->info[0])
      fprintf(fp, "Info %s\n", printer->info);
    if (printer->more_info[0])
      fprintf(fp, "Location %s\n", printer->location);
    if (printer->device_uri[0])
      fprintf(fp, "DeviceURI %s\n", printer->device_uri);
    if (printer->state == IPP_PRINTER_STOPPED)
    {
      fputs("State Stopped\n", fp);
      fprintf(fp, "StateMessage %s\n", printer->state_message);
    }
    else
      fputs("State Idle\n", fp);
    if (printer->accepting)
      fputs("Accepting Yes\n", fp);
    else
      fputs("Accepting No\n", fp);
    fprintf(fp, "JobSheets %s %s\n", printer->job_sheets[0],
            printer->job_sheets[1]);

    if (printer->quota_period)
    {
      fprintf(fp, "QuotaPeriod %d\n", printer->quota_period);
      fprintf(fp, "PageLimit %d\n", printer->page_limit);
      fprintf(fp, "KLimit %d\n", printer->k_limit);
    }

    for (i = 0; i < printer->num_users; i ++)
      fprintf(fp, "%sUser %s\n", printer->deny_users ? "Deny" : "Allow",
              printer->users[i]);

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
  location_t	*auth;			/* Pointer to authentication element */
  int		auth_len;		/* Length of class or printer resource */
  const char	*auth_supported;	/* Authentication supported */
  ppd_file_t	*ppd;			/* PPD file data */
  ppd_option_t	*input_slot,		/* InputSlot options */
		*media_type,		/* MediaType options */
		*page_size,		/* PageSize options */
		*output_bin;		/* OutputBin options */
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
  const char	*versions[] =		/* ipp-versions-supported values */
		{
		  "1.0",
		  "1.1"
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
		  IPP_SET_JOB_ATTRIBUTES,
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
		  "windows-1258"
		};
  int		num_finishings;
  ipp_finish_t	finishings[5];
  const char	*multiple_document_handling[] =
		{
		  "single-document",
		  "separate-documents-uncollated-copies",
		  "separate-documents-collated-copies",
		  "single-document-new-sheet"
		};
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
  * Figure out the authentication that is required for the printer.
  */

  auth_supported = "requesting-user-name";
  if (!(p->type & CUPS_PRINTER_REMOTE))
  {
    if (p->type & CUPS_PRINTER_CLASS)
    {
      auth_len = 8;
      snprintf(resource, sizeof(resource), "/classes/%s", p->name);
    }
    else
    {
      auth_len = 9;
      snprintf(resource, sizeof(resource), "/printers/%s", p->name);
    }

    for (i = NumLocations, auth = Locations; i > 0; i --, auth ++)
      if (strcmp(auth->location, resource) == 0)
      {
       /*
        * Exact match...
	*/

	if (auth->type == AUTH_BASIC)
	  auth_supported = "basic";
	else if (auth->type == AUTH_DIGEST)
	  auth_supported = "digest";
	break;
      }
      else if (strcmp(auth->location, "/") == 0 ||
               (strncmp(auth->location, resource, auth_len) == 0 &&
                (strlen(auth->location) == auth_len ||
	         strlen(auth->location) == (auth_len + 1))))
      {
       /*
        * Matches base printer or class resources...
	*/

	if (auth->type == AUTH_BASIC)
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
               NULL, p->location);
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info",
               NULL, p->info);
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-more-info",
               NULL, p->more_info);
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
               "pdl-override-supported", NULL, "not-attempted");
  ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                "ipp-versions-supported", sizeof(versions) / sizeof(versions[0]),
		NULL, versions);
  ippAddIntegers(p->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "operations-supported",
                 sizeof(ops) / sizeof(ops[0]) + JobFiles - 1, (int *)ops);
  ippAddBoolean(p->attrs, IPP_TAG_PRINTER, "multiple-document-jobs-supported", 1);
  ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "multiple-operation-time-out", 60);
  ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                "multiple-document-handling-supported",
                sizeof(multiple_document_handling) /
		    sizeof(multiple_document_handling[0]), NULL,
	        multiple_document_handling);
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
  ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
               "compression-supported", NULL, "none");
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

  if (NumBanners > 0)
  {
   /*
    * Setup the job-sheets-supported and job-sheets-default attributes...
    */

    if (Classification[0])
      attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME,
                	   "job-sheets-supported", 2, NULL, NULL);
    else
      attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME,
                	   "job-sheets-supported", NumBanners + 1, NULL, NULL);

    if (attr == NULL)
      LogMessage(L_EMERG, "SetPrinterAttrs: Unable to allocate memory for "
                          "job-sheets-supported attribute: %s!",
	         strerror(errno));
    else
    {
      attr->values[0].string.text = strdup("none");

      if (Classification[0])
	attr->values[1].string.text = strdup(Classification);
      else
      {
	for (i = 0; i < NumBanners; i ++)
	  attr->values[i + 1].string.text = strdup(Banners[i].name);
      }
    }

    attr = ippAddStrings(p->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME,
                	 "job-sheets-default", 2, NULL, NULL);

    if (attr != NULL)
    {
      attr->values[0].string.text = strdup(p->job_sheets[0]);
      attr->values[1].string.text = strdup(p->job_sheets[1]);
    }
  }

  if (p->type & CUPS_PRINTER_REMOTE)
  {
   /*
    * Tell the client this is a remote printer of some type...
    */

    ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                 "printer-make-and-model", NULL, p->make_model);
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

      if (strstr(p->device_uri, "://") != NULL)
      {
       /*
        * http://..., ipp://..., etc.
	*/

        httpSeparate(p->device_uri, method, username, host, &port, resource);
	if (port)
	  snprintf(uri, sizeof(uri), "%s://%s:%d%s", method, host, port,
	           resource);
	else
	  snprintf(uri, sizeof(uri), "%s://%s%s", method, host, resource);
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

	ippAddBoolean(p->attrs, IPP_TAG_PRINTER, "color-supported",
                      ppd->color_device);
	if (ppd->throughput)
	  ippAddInteger(p->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
	                "pages-per-minute", ppd->throughput);
	ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                     "printer-make-and-model", NULL, ppd->nickname);

        strncpy(p->make_model, ppd->nickname, sizeof(p->make_model) - 1);

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
        if (attr != NULL)
	{
	  val = attr->values;

	  if (input_slot != NULL)
	    for (i = 0; i < input_slot->num_choices; i ++, val ++)
	      val->string.text = strdup(input_slot->choices[i].choice);

	  if (media_type != NULL)
	    for (i = 0; i < media_type->num_choices; i ++, val ++)
	      val->string.text = strdup(media_type->choices[i].choice);

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
	  else
	    ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-default",
                	 NULL, "none");
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
      }
      else if (access(filename, 0) == 0)
      {
	LogMessage(L_ERROR, "PPD file for %s cannot be loaded!", p->name);

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
                    "finishings-default", IPP_FINISHINGS_NONE);
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

  snprintf(filename, sizeof(filename), "/var/spool/lp/interface/%s", p->name);
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

  snprintf(filename, sizeof(filename), "/var/spool/lp/member/%s", p->name);
  if ((fp = fopen(filename, "w")) != NULL)
  {
    fputs("/dev/null\n", fp);
    fclose(fp);
    chmod(filename, 0644);
  }

  snprintf(filename, sizeof(filename), "/var/spool/lp/gui_interface/ELF/%s.gui", p->name);
  if ((fp = fopen(filename, "w")) != NULL)
  {
    fputs("#!/bin/sh\n", fp);
    fprintf(fp, "/usr/bin/glpoptions -d %s -o \"$3\"\n", p->name);
    fclose(fp);
    chmod(filename, 0755);
  }

  snprintf(filename, sizeof(filename), "/var/spool/lp/activeicons/%s", p->name);
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
  job_t	*job;			/* Active print job */


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

    StopJob(job->id);

   /*
    * Reset the state to pending...
    */

    job->state->values[0].integer = IPP_JOB_PENDING;

    SaveJob(job->id);
  }

  p->state = IPP_PRINTER_STOPPED;
}


/*
 * 'ValidateDest()' - Validate a printer/class destination.
 */

const char *				/* O - Printer or class name */
ValidateDest(const char   *hostname,	/* I - Host name */
             const char   *resource,	/* I - Resource name */
             cups_ptype_t *dtype)	/* O - Type (printer or class) */
{
  printer_t	*p;			/* Current printer */
  char		localname[1024],	/* Localized hostname */
		*lptr,			/* Pointer into localized hostname */
		*sptr;			/* Pointer into server name */


  DEBUG_printf(("ValidateDest(\"%s\", \"%s\", %p)\n", hostname, resource, dtype));

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
    *dtype = p->type & CUPS_PRINTER_CLASS;
    return (p->name);
  }

 /*
  * Change localhost to the server name...
  */

  if (strcasecmp(hostname, "localhost") == 0)
    hostname = ServerName;

  strncpy(localname, hostname, sizeof(localname) - 1);
  localname[sizeof(localname) - 1] = '\0';

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
      *dtype = p->type & CUPS_PRINTER_CLASS;
      return (p->name);
    }

  return (NULL);
}


/*
 * 'write_printcap()' - Write a pseudo-printcap file for older applications
 *                      that need it...
 */

static void
write_printcap(void)
{
  FILE		*fp;		/* printcap file */
  printer_t	*p;		/* Current printer */


 /*
  * See if we have a printcap file; if not, don't bother writing it.
  */

  if (!Printcap[0])
    return;

 /*
  * Open the printcap file...
  */

  if ((fp = fopen(Printcap, "w")) == NULL)
    return;

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

	for (p = Printers; p != NULL; p = p->next)
	  fprintf(fp, "%s:\n", p->name);
        break;

    case PRINTCAP_SOLARIS:
       /*
        * Each printer is put in the file as:
	*
	*    _all:all=Printer1,Printer2,Printer3,...,PrinterN
	*    _default:use=DefaultPrinter
	*    Printer1:
	*    Printer2:
	*    Printer3:
	*    ...
	*    PrinterN:
	*/

        fputs("_all:all=", fp);
	for (p = Printers; p != NULL; p = p->next)
	  fprintf(fp, "%s%c", p->name, p->next ? ',' : '\n');

        if (DefaultPrinter)
	  fprintf(fp, "_default:use=%s\n", DefaultPrinter->name);

	for (p = Printers; p != NULL; p = p->next)
	  fprintf(fp, "%s:\n", p->name);
        break;
  }

 /*
  * Close the file...
  */

  fclose(fp);
}


/*
 * End of "$Id: printers.c,v 1.93.2.1 2001/04/02 19:51:50 mike Exp $".
 */
