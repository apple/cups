/*
 * "$Id$"
 *
 *   Printer class routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
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
 *   cupsdAddClass()                 - Add a class to the system.
 *   cupsdAddPrinterToClass()        - Add a printer to a class...
 *   cupsdDeletePrinterFromClass()   - Delete a printer from a class.
 *   cupsdDeletePrinterFromClasses() - Delete a printer from all classes.
 *   cupsdDeleteAllClasses()         - Remove all classes from the system.
 *   cupsdFindAvailablePrinter()     - Find an available printer in a class.
 *   cupsdFindClass()                - Find the named class.
 *   cupsdLoadAllClasses()           - Load classes from the classes.conf file.
 *   cupsdSaveAllClasses()           - Save classes to the classes.conf file.
 *   cupsdUpdateImplicitClasses()    - Update the accepting state of implicit
 *                                     classes.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * 'cupsdAddClass()' - Add a class to the system.
 */

cupsd_printer_t *			/* O - New class */
cupsdAddClass(const char *name)		/* I - Name of class */
{
  cupsd_printer_t	*c;		/* New class */


 /*
  * Add the printer and set the type to "class"...
  */

  if ((c = cupsdAddPrinter(name)) != NULL)
  {
   /*
    * Change from a printer to a class...
    */

    c->type = CUPS_PRINTER_CLASS;

    cupsdSetStringf(&c->uri, "ipp://%s:%d/classes/%s", ServerName, LocalPort,
                    name);
    cupsdSetString(&c->error_policy, "retry-job");
  }

  return (c);
}


/*
 * 'cupsdAddPrinterToClass()' - Add a printer to a class...
 */

void
cupsdAddPrinterToClass(
    cupsd_printer_t *c,			/* I - Class to add to */
    cupsd_printer_t *p)			/* I - Printer to add */
{
  int			i;		/* Looping var */
  cupsd_printer_t	**temp;		/* Pointer to printer array */


 /*
  * See if this printer is already a member of the class...
  */

  for (i = 0; i < c->num_printers; i ++)
    if (c->printers[i] == p)
      return;

 /*
  * Allocate memory as needed...
  */

  if (c->num_printers == 0)
    temp = malloc(sizeof(cupsd_printer_t *));
  else
    temp = realloc(c->printers, sizeof(cupsd_printer_t *) * (c->num_printers + 1));

  if (temp == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to add printer %s to class %s!",
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
}


/*
 * 'cupsdDeletePrinterFromClass()' - Delete a printer from a class.
 */

void
cupsdDeletePrinterFromClass(
    cupsd_printer_t *c,			/* I - Class to delete from */
    cupsd_printer_t *p)			/* I - Printer to delete */
{
  int		i;			/* Looping var */
  cups_ptype_t	type,			/* Class type */
		oldtype;		/* Old class type */


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
      memmove(c->printers + i, c->printers + i + 1,
              (c->num_printers - i) * sizeof(cupsd_printer_t *));
  }
  else
    return;

 /*
  * Recompute the printer type mask as needed...
  */

  if (c->num_printers > 0)
  {
    oldtype = c->type;
    type    = c->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT);
    c->type = ~CUPS_PRINTER_REMOTE;

    for (i = 0; i < c->num_printers; i ++)
      c->type &= c->printers[i]->type;

    c->type |= type;

   /*
    * Update the IPP attributes...
    */

    if (c->type != oldtype)
      cupsdSetPrinterAttrs(c);
  }
}


/*
 * 'cupsdDeletePrinterFromClasses()' - Delete a printer from all classes.
 */

void
cupsdDeletePrinterFromClasses(
    cupsd_printer_t *p)			/* I - Printer to delete */
{
  cupsd_printer_t	*c;		/* Pointer to current class */


 /*
  * Loop through the printer/class list and remove the printer
  * from each class listed...
  */

  for (c = (cupsd_printer_t *)cupsArrayFirst(Printers);
       c;
       c = (cupsd_printer_t *)cupsArrayNext(Printers))
    if (c->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT))
      cupsdDeletePrinterFromClass(c, p);

 /*
  * Then clean out any empty implicit classes...
  */

  for (c = (cupsd_printer_t *)cupsArrayFirst(ImplicitPrinters);
       c;
       c = (cupsd_printer_t *)cupsArrayNext(ImplicitPrinters))
    if (c->num_printers == 0)
    {
      cupsArrayRemove(ImplicitPrinters, c);
      cupsdDeletePrinter(c, 0);
    }
}


/*
 * 'cupsdDeleteAllClasses()' - Remove all classes from the system.
 */

void
cupsdDeleteAllClasses(void)
{
  cupsd_printer_t	*c;		/* Pointer to current printer/class */


  for (c = (cupsd_printer_t *)cupsArrayFirst(Printers);
       c;
       c = (cupsd_printer_t *)cupsArrayNext(Printers))
    if (c->type & CUPS_PRINTER_CLASS)
      cupsdDeletePrinter(c, 0);
}


/*
 * 'cupsdFindAvailablePrinter()' - Find an available printer in a class.
 */

cupsd_printer_t *			/* O - Available printer or NULL */
cupsdFindAvailablePrinter(
    const char *name)			/* I - Class to check */
{
  int			i;		/* Looping var */
  cupsd_printer_t	*c;		/* Printer class */


 /*
  * Find the class...
  */

  if ((c = cupsdFindClass(name)) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to find class \"%s\"!", name);
    return (NULL);
  }

  if (c->num_printers == 0)
    return (NULL);

 /*
  * Make sure that the last printer is also a valid index into the printer
  * array.  If not, reset the last printer to 0...
  */

  if (c->last_printer >= c->num_printers)
    c->last_printer = 0;

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

    if (c->printers[i]->accepting &&
        (c->printers[i]->state == IPP_PRINTER_IDLE ||
         ((c->printers[i]->type & CUPS_PRINTER_REMOTE) && !c->printers[i]->job)))
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
 * 'cupsdFindClass()' - Find the named class.
 */

cupsd_printer_t *			/* O - Matching class or NULL */
cupsdFindClass(const char *name)	/* I - Name of class */
{
  cupsd_printer_t	*c;		/* Current class/printer */


  if ((c = cupsdFindDest(name)) != NULL &&
      (c->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT)))
    return (c);
  else
    return (NULL);
}


/*
 * 'cupsdLoadAllClasses()' - Load classes from the classes.conf file.
 */

void
cupsdLoadAllClasses(void)
{
  cups_file_t		*fp;		/* classes.conf file */
  int			linenum;	/* Current line number */
  char			line[1024],	/* Line from file */
			*value,		/* Pointer to value */
			*valueptr;	/* Pointer into value */
  cupsd_printer_t	*p,		/* Current printer class */
			*temp;		/* Temporary pointer to printer */


 /*
  * Open the classes.conf file...
  */

  snprintf(line, sizeof(line), "%s/classes.conf", ServerRoot);
  if ((fp = cupsFileOpen(line, "r")) == NULL)
  {
    if (errno != ENOENT)
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to open %s - %s", line,
		      strerror(errno));
    return;
  }

 /*
  * Read class configurations until we hit EOF...
  */

  linenum = 0;
  p       = NULL;

  while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
  {
   /*
    * Decode the directive...
    */

    if (!strcasecmp(line, "<Class") ||
        !strcasecmp(line, "<DefaultClass"))
    {
     /*
      * <Class name> or <DefaultClass name>
      */

      if (p == NULL && value)
      {
        cupsdLogMessage(CUPSD_LOG_DEBUG, "Loading class %s...", value);

       /*
        * Since prior classes may have implicitly defined this class,
	* see if it already exists...
	*/

        if ((p = cupsdFindDest(value)) != NULL)
	{
	  p->type = CUPS_PRINTER_CLASS;
	  cupsdSetStringf(&p->uri, "ipp://%s:%d/classes/%s", ServerName,
	                  LocalPort, value);
	  cupsdSetString(&p->error_policy, "retry-job");
	}
	else
          p = cupsdAddClass(value);

	p->accepting = 1;
	p->state     = IPP_PRINTER_IDLE;

        if (!strcasecmp(line, "<DefaultClass"))
	  DefaultPrinter = p;
      }
      else
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of classes.conf.", linenum);
        break;
      }
    }
    else if (!strcasecmp(line, "</Class>"))
    {
      if (p != NULL)
      {
        cupsdSetPrinterAttrs(p);
        p = NULL;
      }
      else
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of classes.conf.", linenum);
        break;
      }
    }
    else if (!p)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Syntax error on line %d of classes.conf.", linenum);
      break;
    }
    else if (!strcasecmp(line, "AuthInfoRequired"))
    {
      if (!cupsdSetAuthInfoRequired(p, value, NULL))
	cupsdLogMessage(CUPSD_LOG_ERROR,
			"Bad AuthInfoRequired on line %d of classes.conf.",
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
    else if (!strcasecmp(line, "Option") && value)
    {
     /*
      * Option name value
      */

      for (valueptr = value; *valueptr && !isspace(*valueptr & 255); valueptr ++);

      if (!*valueptr)
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of classes.conf.", linenum);
      else
      {
        for (; *valueptr && isspace(*valueptr & 255); *valueptr++ = '\0');

        p->num_options = cupsAddOption(value, valueptr, p->num_options,
	                               &(p->options));
      }
    }
    else if (!strcasecmp(line, "Printer"))
    {
      if (!value)
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of classes.conf.", linenum);
	break;
      }
      else if ((temp = cupsdFindPrinter(value)) == NULL)
      {
	cupsdLogMessage(CUPSD_LOG_WARN,
	                "Unknown printer %s on line %d of classes.conf.",
	                value, linenum);

       /*
	* Add the missing remote printer...
	*/

	if ((temp = cupsdAddPrinter(value)) != NULL)
	{
	  cupsdSetString(&temp->make_model, "Remote Printer on unknown");

          temp->state       = IPP_PRINTER_STOPPED;
	  temp->type        |= CUPS_PRINTER_REMOTE;
	  temp->browse_time = 2147483647;

	  cupsdSetString(&temp->location, "Location Unknown");
	  cupsdSetString(&temp->info, "No Information Available");
	  temp->hostname[0] = '\0';

	  cupsdSetPrinterAttrs(temp);
	}
      }

      if (temp)
        cupsdAddPrinterToClass(p, temp);
    }
    else if (!strcasecmp(line, "State"))
    {
     /*
      * Set the initial queue state...
      */

      if (!strcasecmp(value, "idle"))
        p->state = IPP_PRINTER_IDLE;
      else if (!strcasecmp(value, "stopped"))
        p->state = IPP_PRINTER_STOPPED;
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of classes.conf.",
	                linenum);
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
	                "Syntax error on line %d of classes.conf.",
	                linenum);
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
	                "Syntax error on line %d of classes.conf.",
	                linenum);
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
	for (valueptr = value;
	     *valueptr && !isspace(*valueptr & 255);
	     valueptr ++);

	if (*valueptr)
          *valueptr++ = '\0';

	cupsdSetString(&p->job_sheets[0], value);

	while (isspace(*valueptr & 255))
          valueptr ++;

	if (*valueptr)
	{
          for (value = valueptr;
	       *valueptr && !isspace(*valueptr & 255);
	       valueptr ++);

	  if (*valueptr)
            *valueptr++ = '\0';

	  cupsdSetString(&p->job_sheets[1], value);
	}
      }
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of classes.conf.", linenum);
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
	                "Syntax error on line %d of classes.conf.", linenum);
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
	                "Syntax error on line %d of classes.conf.", linenum);
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
	                "Syntax error on line %d of classes.conf.", linenum);
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
	                "Syntax error on line %d of classes.conf.", linenum);
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
	                "Syntax error on line %d of classes.conf.", linenum);
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
	                  "Bad policy \"%s\" on line %d of classes.conf",
			  value, linenum);
      }
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of classes.conf.", linenum);
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
	                "Syntax error on line %d of classes.conf.", linenum);
	break;
      }
    }
    else
    {
     /*
      * Something else we don't understand...
      */

      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unknown configuration directive %s on line %d of classes.conf.",
	              line, linenum);
    }
  }

  cupsFileClose(fp);
}


/*
 * 'cupsdSaveAllClasses()' - Save classes to the classes.conf file.
 */

void
cupsdSaveAllClasses(void)
{
  cups_file_t		*fp;		/* classes.conf file */
  char			temp[1024];	/* Temporary string */
  char			backup[1024];	/* classes.conf.O file */
  cupsd_printer_t	*pclass;	/* Current printer class */
  int			i;		/* Looping var */
  time_t		curtime;	/* Current time */
  struct tm		*curdate;	/* Current date */
  cups_option_t		*option;	/* Current option */
  const char		*ptr;		/* Pointer into info/location */


 /*
  * Create the classes.conf file...
  */

  snprintf(temp, sizeof(temp), "%s/classes.conf", ServerRoot);
  snprintf(backup, sizeof(backup), "%s/classes.conf.O", ServerRoot);

  if (rename(temp, backup))
  {
    if (errno != ENOENT)
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to backup classes.conf - %s",
                      strerror(errno));
  }

  if ((fp = cupsFileOpen(temp, "w")) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to save classes.conf - %s",
                    strerror(errno));

    if (rename(backup, temp))
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to restore classes.conf - %s",
                      strerror(errno));
    return;
  }
  else
    cupsdLogMessage(CUPSD_LOG_INFO, "Saving classes.conf...");

 /*
  * Restrict access to the file...
  */

  fchown(cupsFileNumber(fp), RunUser, Group);
  fchmod(cupsFileNumber(fp), 0600);

 /*
  * Write a small header to the file...
  */

  curtime = time(NULL);
  curdate = localtime(&curtime);
  strftime(temp, sizeof(temp) - 1, "%Y-%m-%d %H:%M", curdate);

  cupsFilePuts(fp, "# Class configuration file for " CUPS_SVERSION "\n");
  cupsFilePrintf(fp, "# Written by cupsd on %s\n", temp);

 /*
  * Write each local class known to the system...
  */

  for (pclass = (cupsd_printer_t *)cupsArrayFirst(Printers);
       pclass;
       pclass = (cupsd_printer_t *)cupsArrayNext(Printers))
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
      cupsFilePrintf(fp, "<DefaultClass %s>\n", pclass->name);
    else
      cupsFilePrintf(fp, "<Class %s>\n", pclass->name);

    if (pclass->num_auth_info_required > 0)
    {
      cupsFilePrintf(fp, "AuthInfoRequired %s", pclass->auth_info_required[0]);
      for (i = 1; i < pclass->num_auth_info_required; i ++)
        cupsFilePrintf(fp, ",%s", pclass->auth_info_required[i]);
      cupsFilePutChar(fp, '\n');
    }

    if (pclass->info)
    {
      if ((ptr = strchr(pclass->info, '#')) != NULL)
      {
       /*
        * Need to quote the first # in the info string...
	*/

        cupsFilePuts(fp, "Info ");
	cupsFileWrite(fp, pclass->info, ptr - pclass->info);
	cupsFilePutChar(fp, '\\');
	cupsFilePuts(fp, ptr);
	cupsFilePutChar(fp, '\n');
      }
      else
        cupsFilePrintf(fp, "Info %s\n", pclass->info);
    }

    if (pclass->location)
    {
      if ((ptr = strchr(pclass->info, '#')) != NULL)
      {
       /*
        * Need to quote the first # in the location string...
	*/

        cupsFilePuts(fp, "Location ");
	cupsFileWrite(fp, pclass->location, ptr - pclass->location);
	cupsFilePutChar(fp, '\\');
	cupsFilePuts(fp, ptr);
	cupsFilePutChar(fp, '\n');
      }
      else
        cupsFilePrintf(fp, "Location %s\n", pclass->location);
    }

    if (pclass->state == IPP_PRINTER_STOPPED)
    {
      cupsFilePuts(fp, "State Stopped\n");
      cupsFilePrintf(fp, "StateMessage %s\n", pclass->state_message);
    }
    else
      cupsFilePuts(fp, "State Idle\n");

    cupsFilePrintf(fp, "StateTime %d\n", (int)pclass->state_time);

    if (pclass->accepting)
      cupsFilePuts(fp, "Accepting Yes\n");
    else
      cupsFilePuts(fp, "Accepting No\n");

    if (pclass->shared)
      cupsFilePuts(fp, "Shared Yes\n");
    else
      cupsFilePuts(fp, "Shared No\n");

    cupsFilePrintf(fp, "JobSheets %s %s\n", pclass->job_sheets[0],
                   pclass->job_sheets[1]);

    for (i = 0; i < pclass->num_printers; i ++)
      cupsFilePrintf(fp, "Printer %s\n", pclass->printers[i]->name);

    cupsFilePrintf(fp, "QuotaPeriod %d\n", pclass->quota_period);
    cupsFilePrintf(fp, "PageLimit %d\n", pclass->page_limit);
    cupsFilePrintf(fp, "KLimit %d\n", pclass->k_limit);

    for (i = 0; i < pclass->num_users; i ++)
      cupsFilePrintf(fp, "%sUser %s\n", pclass->deny_users ? "Deny" : "Allow",
        	     pclass->users[i]);

    if (pclass->op_policy)
      cupsFilePrintf(fp, "OpPolicy %s\n", pclass->op_policy);
    if (pclass->error_policy)
      cupsFilePrintf(fp, "ErrorPolicy %s\n", pclass->error_policy);

    for (i = pclass->num_options, option = pclass->options;
         i > 0;
	 i --, option ++)
      cupsFilePrintf(fp, "Option %s %s\n", option->name, option->value);

    cupsFilePuts(fp, "</Class>\n");
  }

  cupsFileClose(fp);
}


/*
 * 'cupsdUpdateImplicitClasses()' - Update the accepting state of implicit
 *                                  classes.
 */

void
cupsdUpdateImplicitClasses(void)
{
  int			i;		/* Looping var */
  cupsd_printer_t	*pclass;	/* Current class */
  int			accepting;	/* printer-is-accepting-jobs value */


  for (pclass = (cupsd_printer_t *)cupsArrayFirst(ImplicitPrinters);
       pclass;
       pclass = (cupsd_printer_t *)cupsArrayNext(ImplicitPrinters))
  {
   /*
    * Loop through the printers to come up with a composite state...
    */

    for (i = 0, accepting = 0; i < pclass->num_printers; i ++)
      if ((accepting = pclass->printers[i]->accepting) != 0)
	break;

    pclass->accepting = accepting;
  }
}


/*
 * End of "$Id$".
 */
