/*
 * "$Id: classes.c,v 1.32 2001/02/09 18:40:34 mike Exp $"
 *
 *   Printer class routines for the Common UNIX Printing System (CUPS).
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
   /*
    * Change from a printer to a class...
    */

    c->type = CUPS_PRINTER_CLASS;
    snprintf(c->uri, sizeof(c->uri), "ipp://%s:%d/classes/%s", ServerName,
             ntohs(Listeners[0].address.sin_port), name);

   /*
    * MRS - more_info and uri are both HTTP_MAX_URI in size, and will always
    * be, so this strcpy is safe...
    */

    strcpy(c->more_info, c->uri);

   /*
    * Set the printer attributes to make this a class.
    */

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
    LogMessage(L_ERROR, "Unable to add printer %s to class %s!",
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
  int		i;			/* Looping var */
  cups_ptype_t	type;			/* Class type */


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

  type    = c->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT);
  c->type = ~CUPS_PRINTER_REMOTE;

  for (i = 0; i < c->num_printers; i ++)
    c->type &= c->printers[i]->type;

  c->type |= type;

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
    if (c->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT))
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
  {
    LogMessage(L_ERROR, "Unable to find class \"%s\"!", name);
    return (NULL);
  }

 /*
  * Loop through the printers in the class and return the first idle
  * printer...  We keep track of the last printer that we used so that
  * a "round robin" type of scheduling is realized (otherwise the first
  * server might be saturated with print jobs...)
  *
  * Thanks to Joel Fredrikson for helping us get this right!
  */

  for (i = c->last_printer + 1; ; i ++)
  {
    if (i >= c->num_printers)
      i = 0;

    if (c->printers[i]->state == IPP_PRINTER_IDLE ||
        ((c->printers[i]->type & CUPS_PRINTER_REMOTE) && !c->printers[i]->job))
    {
      c->last_printer = i;
      return (c->printers[i]);
    }

    if (i == c->last_printer)
      break;
  }

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
          if (c->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT))
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
  int		linenum;		/* Current line number */
  int		len;			/* Length of line */
  char		line[1024],		/* Line from file */
		name[256],		/* Parameter name */
		*nameptr,		/* Pointer into name */
		*value;			/* Pointer to value */
  printer_t	*p,			/* Current printer class */
		*temp;			/* Temporary pointer to printer */


 /*
  * Open the classes.conf file...
  */

  snprintf(line, sizeof(line), "%s/classes.conf", ServerRoot);
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
        LogMessage(L_ERROR, "Syntax error on line %d of classes.conf.",
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
        LogMessage(L_ERROR, "Syntax error on line %d of classes.conf.",
	           linenum);
        return;
      }
    }
    else if (p == NULL)
    {
      LogMessage(L_ERROR, "Syntax error on line %d of classes.conf.",
	         linenum);
      return;
    }
    
    else if (strcmp(name, "Info") == 0)
      strncpy(p->info, value, sizeof(p->info) - 1);
    else if (strcmp(name, "Location") == 0)
      strncpy(p->location, value, sizeof(p->location) - 1);
    else if (strcmp(name, "Printer") == 0)
    {
      if ((temp = FindPrinter(value)) != NULL)
        AddPrinterToClass(p, temp);
      else
	LogMessage(L_WARN, "Unknown printer %s on line %d of classes.conf.",
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

      LogMessage(L_ERROR, "Unknown configuration directive %s on line %d of classes.conf.",
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

  snprintf(temp, sizeof(temp), "%s/classes.conf", ServerRoot);
  if ((fp = fopen(temp, "w")) == NULL)
  {
    LogMessage(L_ERROR, "Unable to save classes.conf - %s", strerror(errno));
    return;
  }
  else
    LogMessage(L_INFO, "Saving classes.conf...");

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

  fputs("# Class configuration file for " CUPS_SVERSION "\n", fp);
  fprintf(fp, "# Written by cupsd on %s\n", temp);

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
      fprintf(fp, "Location %s\n", pclass->location);
    if (pclass->state == IPP_PRINTER_STOPPED)
    {
      fputs("State Stopped\n", fp);
      fprintf(fp, "StateMessage %s\n", pclass->state_message);
    }
    else
      fputs("State Idle\n", fp);
    if (pclass->accepting)
      fputs("Accepting Yes\n", fp);
    else
      fputs("Accepting No\n", fp);

    for (i = 0; i < pclass->num_printers; i ++)
      fprintf(fp, "Printer %s\n", pclass->printers[i]->name);

    if (pclass->quota_period)
    {
      fprintf(fp, "QuotaPeriod %d\n", pclass->quota_period);
      fprintf(fp, "PageLimit %d\n", pclass->page_limit);
      fprintf(fp, "KLimit %d\n", pclass->k_limit);
    }

    for (i = 0; i < pclass->num_users; i ++)
      fprintf(fp, "%sUser %s\n", pclass->deny_users ? "Deny" : "Allow",
              pclass->users[i]);

    fputs("</Class>\n", fp);
  }

  fclose(fp);
}


/*
 * End of "$Id: classes.c,v 1.32 2001/02/09 18:40:34 mike Exp $".
 */
