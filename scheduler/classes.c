/*
 * "$Id: classes.c,v 1.15 2000/01/20 13:05:41 mike Exp $"
 *
 *   Printer class routines for the Common UNIX Printing System (CUPS).
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
 *   AddClass()                 - Add a class to the system.
 *   AddPrinterToClass()        - Add a printer to a class...
 *   DeletePrinterFromClass()   - Delete a printer from a class.
 *   DeletePrinterFromClasses() - Delete a printer from all classes.
 *   DeleteAllClasses()         - Remove all classes from the system.
 *   FindAvailablePrinter()     - Find an available printer in a class.
 *   FindClass()                - Find the named class.
 *   LoadAllClasses()           - Load classes from the classes.conf file.
 *   SaveAllClasses()           - Save classes to the classes.conf file.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * 'AddClass()' - Add a class to the system.
 */

printer_t *			/* O - New class */
AddClass(const char *name)	/* I - Name of class */
{
  printer_t	*c;		/* New class */


 /*
  * Add the printer and set the type to "class"...
  */

  if ((c = AddPrinter(name)) != NULL)
  {
    c->type = CUPS_PRINTER_CLASS;
    sprintf(c->uri, "ipp://%s:%d/classes/%s", ServerName,
            ntohs(Listeners[0].address.sin_port), name);
    SetPrinterAttrs(c);
  }

  return (c);
}


/*
 * 'AddPrinterToClass()' - Add a printer to a class...
 */

void
AddPrinterToClass(printer_t *c,	/* I - Class to add to */
                  printer_t *p)	/* I - Printer to add */
{
  printer_t	**temp;		/* Pointer to printer array */


 /*
  * Allocate memory as needed...
  */

  if (c->num_printers == 0)
    temp = malloc(sizeof(printer_t *));
  else
    temp = realloc(c->printers, sizeof(printer_t *) * (c->num_printers + 1));

  if (temp == NULL)
  {
    LogMessage(LOG_ERROR, "Unable to add printer %s to class %s!",
               p->name, c->name);
    return;
  }

 /*
  * Add the printer to the end of the array and update the number of printers.
  */

  c->printers = temp;
  temp        += c->num_printers;
  c->num_printers ++;

  *temp = p;

 /*
  * Update the IPP attributes...
  */

  SetPrinterAttrs(c);
}


/*
 * 'DeletePrinterFromClass()' - Delete a printer from a class.
 */

void
DeletePrinterFromClass(printer_t *c,	/* I - Class to delete from */
                       printer_t *p)	/* I - Printer to delete */
{
  int	i;				/* Looping var */


 /*
  * See if the printer is in the class...
  */

  for (i = 0; i < c->num_printers; i ++)
    if (p == c->printers[i])
      break;

 /*
  * If it is, remove it from the list...
  */

  if (i < c->num_printers)
  {
   /*
    * Yes, remove the printer...
    */

    c->num_printers --;
    if (i < c->num_printers)
      memcpy(c->printers + i, c->printers + i + 1,
             (c->num_printers - i) * sizeof(printer_t *));
  }

 /*
  * If there are no more printers in this class, delete the class...
  */

  if (c->num_printers == 0)
  {
    DeletePrinter(c);
    return;
  }

 /*
  * Recompute the printer type mask...
  */

  c->type = ~CUPS_PRINTER_REMOTE;

  for (i = 0; i < c->num_printers; i ++)
    c->type &= c->printers[i]->type;

  c->type |= CUPS_PRINTER_CLASS;

 /*
  * Update the IPP attributes...
  */

  SetPrinterAttrs(c);
}


/*
 * 'DeletePrinterFromClasses()' - Delete a printer from all classes.
 */

void
DeletePrinterFromClasses(printer_t *p)	/* I - Printer to delete */
{
  printer_t	*c;			/* Pointer to current class */


 /*
  * Loop through the printer/class list and remove the printer
  * from each class listed...
  */

  for (c = Printers; c != NULL; c = c->next)
    if (c->type & CUPS_PRINTER_CLASS)
      DeletePrinterFromClass(c, p);
}


/*
 * 'DeleteAllClasses()' - Remove all classes from the system.
 */

void
DeleteAllClasses(void)
{
  printer_t	*c,	/* Pointer to current printer/class */
		*next;	/* Pointer to next printer in list */


  for (c = Printers; c != NULL; c = next)
  {
    next = c->next;

    if (c->type & CUPS_PRINTER_CLASS)
      DeletePrinter(c);
  }
}


/*
 * 'FindAvailablePrinter()' - Find an available printer in a class.
 */

printer_t *				/* O - Available printer or NULL */
FindAvailablePrinter(const char *name)	/* I - Class to check */
{
  int		i;			/* Looping var */
  printer_t	*c;			/* Printer class */


 /*
  * Find the class...
  */

  if ((c = FindClass(name)) == NULL)
    return (NULL);

 /*
  * Loop through the printers in the class and return the first idle
  * printer... [MRS - might want to rotate amongst the available
  * printers in a future incarnation]
  */

  for (i = 0; i < c->num_printers; i ++)
    if (c->printers[i]->state == IPP_PRINTER_IDLE)
      return (c->printers[i]);

  return (NULL);
}


/*
 * 'FindClass()' - Find the named class.
 */

printer_t *			/* O - Matching class or NULL */
FindClass(const char *name)	/* I - Name of class */
{
  printer_t	*c;		/* Current class/printer */


  for (c = Printers; c != NULL; c = c->next)
    switch (strcasecmp(name, c->name))
    {
      case 0 : /* name == c->name */
          if (c->type & CUPS_PRINTER_CLASS)
	    return (c);
      case 1 : /* name > c->name */
          break;
      case -1 : /* name < c->name */
          return (NULL);
    }

  return (NULL);
}


/*
 * 'LoadAllClasses()' - Load classes from the classes.conf file.
 */

void
LoadAllClasses(void)
{
  FILE		*fp;			/* classes.conf file */
  int		i;			/* Looping var */
  int		linenum;		/* Current line number */
  int		len;			/* Length of line */
  char		line[HTTP_MAX_BUFFER],	/* Line from file */
		name[256],		/* Parameter name */
		*nameptr,		/* Pointer into name */
		*value,			/* Pointer to value */
		*lineptr;		/* Pointer in line */
  printer_t	*p,			/* Current printer class */
		*temp;			/* Temporary pointer to printer */


 /*
  * Open the classes.conf file...
  */

  sprintf(line, "%s/classes.conf", ServerRoot);
  if ((fp = fopen(line, "r")) == NULL)
    return;

 /*
  * Read class configurations until we hit EOF...
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

    if (strcmp(name, "<Class") == 0 ||
        strcmp(name, "<DefaultClass") == 0)
    {
     /*
      * <Class name> or <DefaultClass name>
      */

      if (line[len - 1] == '>' && p == NULL)
      {
        line[len - 1] = '\0';

        p = AddClass(value);
	p->accepting = 1;
	p->state     = IPP_PRINTER_IDLE;

        if (strcmp(name, "<DefaultClass") == 0)
	  DefaultPrinter = p;
      }
      else
      {
        LogMessage(LOG_ERROR, "Syntax error on line %d of classes.conf.",
	           linenum);
        return;
      }
    }
    else if (strcmp(name, "</Class>") == 0)
    {
      if (p != NULL)
      {
        SetPrinterAttrs(p);
        p = NULL;
      }
      else
      {
        LogMessage(LOG_ERROR, "Syntax error on line %d of classes.conf.",
	           linenum);
        return;
      }
    }
    else if (p == NULL)
    {
      LogMessage(LOG_ERROR, "Syntax error on line %d of classes.conf.",
	         linenum);
      return;
    }
    
    else if (strcmp(name, "Info") == 0)
      strncpy(p->info, value, sizeof(p->info) - 1);
    else if (strcmp(name, "MoreInfo") == 0)
      strncpy(p->more_info, value, sizeof(p->more_info) - 1);
    else if (strcmp(name, "Location") == 0)
      strncpy(p->location, value, sizeof(p->location) - 1);
    else if (strcmp(name, "Printer") == 0)
    {
      if ((temp = FindPrinter(value)) != NULL)
        AddPrinterToClass(p, temp);
      else
	LogMessage(LOG_WARN, "Unknown printer %s on line %d of classes.conf.",
	           value, linenum);
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

      LogMessage(LOG_ERROR, "Unknown configuration directive %s on line %d of classes.conf.",
	         name, linenum);
    }
  }

  fclose(fp);
}


/*
 * 'SaveAllClasses()' - Save classes to the classes.conf file.
 */

void
SaveAllClasses(void)
{
  FILE		*fp;			/* classes.conf file */
  char		temp[1024];		/* Temporary string */
  printer_t	*pclass;		/* Current printer class */
  int		i;			/* Looping var */
  time_t	curtime;		/* Current time */
  struct tm	*curdate;		/* Current date */


 /*
  * Create the classes.conf file...
  */

  sprintf(temp, "%s/classes.conf", ServerRoot);
  if ((fp = fopen(temp, "w")) == NULL)
  {
    LogMessage(LOG_ERROR, "Unable to save classes.conf - %s", strerror(errno));
    return;
  }
  else
    LogMessage(LOG_INFO, "Saving classes.conf...");

 /*
  * Write a small header to the file...
  */

  curtime = time(NULL);
  curdate = gmtime(&curtime);
  strftime(temp, sizeof(temp) - 1, "# Written by cupsd on %c\n", curdate);

  fputs("# Class configuration file for " CUPS_SVERSION "\n", fp);
  fputs(temp, fp);

 /*
  * Write each local class known to the system...
  */

  for (pclass = Printers; pclass != NULL; pclass = pclass->next)
  {
   /*
    * Skip remote destinations and regular printers...
    */

    if ((pclass->type & CUPS_PRINTER_REMOTE) ||
        (pclass->type & CUPS_PRINTER_IMPLICIT) ||
        !(pclass->type & CUPS_PRINTER_CLASS))
      continue;

   /*
    * Write printers as needed...
    */

    if (pclass == DefaultPrinter)
      fprintf(fp, "<DefaultClass %s>\n", pclass->name);
    else
      fprintf(fp, "<Class %s>\n", pclass->name);

    if (pclass->info[0])
      fprintf(fp, "Info %s\n", pclass->info);
    if (pclass->more_info[0])
      fprintf(fp, "MoreInfo %s\n", pclass->more_info);
    if (pclass->location[0])
      fprintf(fp, "Location %s\n", pclass->location);
    if (pclass->state == IPP_PRINTER_STOPPED)
      fputs("State Stopped\n", fp);
    else
      fputs("State Idle\n", fp);
    if (pclass->accepting)
      fputs("Accepting Yes\n", fp);
    else
      fputs("Accepting No\n", fp);

    for (i = 0; i < pclass->num_printers; i ++)
      fprintf(fp, "Printer %s\n", pclass->printers[i]->name);

    fputs("</Class>\n", fp);
  }

  fclose(fp);
}


/*
 * End of "$Id: classes.c,v 1.15 2000/01/20 13:05:41 mike Exp $".
 */
