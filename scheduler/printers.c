/*
 * "$Id: printers.c,v 1.9 1999/04/16 20:47:48 mike Exp $"
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
 * 'AddPrinter()' - Add a printer to the system.
 */

printer_t *			/* O - New printer */
AddPrinter(char *name)		/* I - Name of printer */
{
  printer_t	*p,		/* New printer */
		*current,	/* Current printer in list */
		*prev;		/* Previous printer in list */
  ipp_attribute_t *attr;	/* IPP attribute for printer */


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

  strcpy((char *)p->name, name);
  p->state = IPP_PRINTER_STOPPED;
  p->attrs = ippNew();
  attr     = ippAddString(p->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME,
                          "printer-name", NULL, name);

 /*
  * Insert the printer in the printer list alphabetically...
  */

  for (prev = NULL, current = Printers;
       current != NULL;
       prev = current, current = current->next)
    if (strcasecmp((char *)p->name, (char *)current->name) < 0)
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
    switch (strcasecmp(name, (char *)p->name))
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
      strncpy((char *)p->info, value, sizeof(p->info) - 1);
    else if (strcmp(name, "MoreInfo") == 0)
      strncpy((char *)p->more_info, value, sizeof(p->more_info) - 1);
    else if (strcmp(name, "Location") == 0)
      strncpy((char *)p->location, value, sizeof(p->location) - 1);
    else if (strcmp(name, "DeviceURI") == 0)
      strncpy(p->device_uri, value, sizeof(p->device_uri) - 1);
    else if (strcmp(name, "Username") == 0)
      strncpy((char *)p->username, value, sizeof(p->username) - 1);
    else if (strcmp(name, "Password") == 0)
      strncpy((char *)p->password, value, sizeof(p->password) - 1);
    else if (strcmp(name, "PPDFile") == 0)
      strncpy(p->ppd, value, sizeof(p->ppd) - 1);
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


void
StartPrinter(printer_t *p)
{
}


void
StopPrinter(printer_t *p)
{
  if (p->job)
    StopJob(((job_t *)p->job)->id);

  p->state = IPP_PRINTER_STOPPED;
}


/*
 * End of "$Id: printers.c,v 1.9 1999/04/16 20:47:48 mike Exp $".
 */
