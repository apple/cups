/*
 * "$Id: dest.c,v 1.9 2000/06/28 16:40:52 mike Exp $"
 *
 *   User-defined destination (and option) support for the Common UNIX
 *   Printing System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products.
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
 *   cupsAddDest()    - Add a destination to the list of destinations.
 *   cupsFreeDests()  - Free the memory used by the list of destinations.
 *   cupsGetDest()    - Get the named destination from the list.
 *   cupsGetDests()   - Get the list of destinations.
 *   cupsSetDests()   - Set the list of destinations.
 *   cups_get_dests() - Get destinations from a file.
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include "string.h"
#include <stdlib.h>
#include <ctype.h>


/*
 * Local functions...
 */

static int	cups_get_dests(const char *filename, int num_dests,
		               cups_dest_t **dests);


/*
 * 'cupsAddDest()' - Add a destination to the list of destinations.
 */

int					/* O - New number of destinations */
cupsAddDest(const char  *name,		/* I - Name of destination */
            const char	*instance,	/* I - Instance of destination */
            int         num_dests,	/* I - Number of destinations */
            cups_dest_t **dests)	/* IO - Destinations */
{
  int		i;			/* Looping var */
  cups_dest_t	*dest;			/* Destination pointer */


  if (name == NULL || dests == NULL)
    return (0);

  if ((dest = cupsGetDest(name, instance, num_dests, *dests)) != NULL)
    return (num_dests);

 /*
  * Add new destination...
  */

  if (num_dests == 0)
    dest = malloc(sizeof(cups_dest_t));
  else
    dest = realloc(*dests, sizeof(cups_dest_t) * (num_dests + 1));

  if (dest == NULL)
    return (num_dests);

  *dests = dest;

  for (i = num_dests; i > 0; i --, dest ++)
    if (strcasecmp(name, dest->name) < 0)
      break;
    else if (instance == NULL && dest->instance != NULL)
      break;
    else if (instance != NULL && dest->instance != NULL &&
             strcasecmp(instance, dest->instance) < 0)
      break;

  if (i > 0)
    memmove(dest + 1, dest, i * sizeof(cups_dest_t));

  dest->name        = strdup(name);
  dest->is_default  = 0;
  dest->num_options = 0;
  dest->options     = (cups_option_t *)0;

  if (instance == NULL)
    dest->instance = NULL;
  else
    dest->instance = strdup(instance);

  return (num_dests + 1);
}


/*
 * 'cupsFreeDests()' - Free the memory used by the list of destinations.
 */

void
cupsFreeDests(int         num_dests,	/* I - Number of destinations */
              cups_dest_t *dests)	/* I - Destinations */
{
  int		i;			/* Looping var */
  cups_dest_t	*dest;			/* Current destination */


  if (num_dests == 0 || dests == NULL)
    return;

  for (i = num_dests, dest = dests; i > 0; i --, dest ++)
  {
    free(dest->name);

    if (dest->instance)
      free(dest->instance);

    cupsFreeOptions(dest->num_options, dest->options);
  }

  free(dests);
}


/*
 * 'cupsGetDest()' - Get the named destination from the list.
 */

cups_dest_t *				/* O - Destination pointer or NULL */
cupsGetDest(const char  *name,		/* I - Name of destination */
            const char	*instance,	/* I - Instance of destination */
            int         num_dests,	/* I - Number of destinations */
            cups_dest_t *dests)		/* I - Destinations */
{
  int	comp;				/* Result of comparison */


  if (num_dests == 0 || dests == NULL)
    return (NULL);

  if (name == NULL)
  {
   /*
    * NULL name for default printer.
    */

    while (num_dests > 0)
    {
      if (dests->is_default)
        return (dests);

      num_dests --;
      dests ++;
    }
  }
  else
  {
   /*
    * Lookup name and optionally the instance...
    */

    while (num_dests > 0)
    {
      if ((comp = strcasecmp(name, dests->name)) < 0)
	return (NULL);
      else if (comp == 0)
      {
	if ((instance == NULL && dests->instance == NULL) ||
            (instance != NULL && dests->instance != NULL &&
	     strcasecmp(instance, dests->instance) == 0))
	  return (dests);
      }

      num_dests --;
      dests ++;
    }
  }

  return (NULL);
}


/*
 * 'cupsGetDests()' - Get the list of destinations.
 */

int					/* O - Number of destinations */
cupsGetDests(cups_dest_t **dests)	/* O - Destinations */
{
  int		i;			/* Looping var */
  int		num_dests;		/* Number of destinations */
  int		count;			/* Number of printers/classes */
  char		**names;		/* Printer/class names */
  cups_dest_t	*dest;			/* Destination pointer */
  const char	*home;			/* HOME environment variable */
  char		filename[1024];		/* Local ~/.lpoptions file */


 /*
  * Initialize destination array...
  */

  num_dests = 0;
  *dests    = (cups_dest_t *)0;

 /*
  * Grab all available printers...
  */

  if ((count = cupsGetPrinters(&names)) > 0)
  {
    for (i = 0; i < count; i ++)
    {
      num_dests = cupsAddDest(names[i], NULL, num_dests, dests);
      free(names[i]);
    }

    free(names);
  }

 /*
  * Grab all available classes...
  */
      
  if ((count = cupsGetClasses(&names)) > 0)
  {
    for (i = 0; i < count; i ++)
    {
      num_dests = cupsAddDest(names[i], NULL, num_dests, dests);
      free(names[i]);
    }

    free(names);
  }

 /*
  * Grab the default destination...
  */

  if ((dest = cupsGetDest(cupsGetDefault(), NULL, num_dests, *dests)) != NULL)
    dest->is_default = 1;

 /*
  * Load the /etc/cups/lpoptions and ~/.lpoptions files...
  */

  if ((home = getenv("CUPS_SERVERROOT")) != NULL)
  {
    snprintf(filename, sizeof(filename), "%s/lpoptions", home);
    num_dests = cups_get_dests(filename, num_dests, dests);
  }
  else
    num_dests = cups_get_dests(CUPS_SERVERROOT "/lpoptions", num_dests, dests);

  if ((home = getenv("HOME")) != NULL)
  {
    snprintf(filename, sizeof(filename), "%s/.lpoptions", home);
    num_dests = cups_get_dests(filename, num_dests, dests);
  }

 /*
  * Return the number of destinations...
  */

  return (num_dests);
}


/*
 * 'cupsSetDests()' - Set the list of destinations.
 */

void
cupsSetDests(int         num_dests,	/* I - Number of destinations */
             cups_dest_t *dests)	/* I - Destinations */
{
  int		i, j;			/* Looping vars */
  cups_dest_t	*dest;			/* Current destination */
  cups_option_t	*option;		/* Current option */
  FILE		*fp;			/* File pointer */
  const char	*home;			/* HOME environment variable */
  char		filename[1024];		/* lpoptions file */


 /*
  * Figure out which file to write to...
  */

  if (getuid() == 0)
  {
    if ((home = getenv("CUPS_SERVERROOT")) != NULL)
      snprintf(filename, sizeof(filename), "%s/lpoptions", home);
    else
      strcpy(filename, CUPS_SERVERROOT "/lpoptions");
  }
  else if ((home = getenv("HOME")) != NULL)
    snprintf(filename, sizeof(filename), "%s/.lpoptions", home);
  else
    return;

 /*
  * Try to open the file...
  */

  if ((fp = fopen(filename, "w")) == NULL)
    return;

 /*
  * Write each printer; each line looks like:
  *
  *    Dest name[/instance] options
  *    Default name[/instance] options
  */

  for (i = num_dests, dest = dests; i > 0; i --, dest ++)
    if (dest->instance != NULL || dest->num_options != 0 || dest->is_default)
    {
      fprintf(fp, "%s %s", dest->is_default ? "Default" : "Dest",
              dest->name);
      if (dest->instance)
	fprintf(fp, "/%s", dest->instance);

      for (j = dest->num_options, option = dest->options; j > 0; j --, option ++)
        if (option->value[0])
	  fprintf(fp, " %s=%s", option->name, option->value);
	else
	  fprintf(fp, " %s", option->name);

      fputs("\n", fp);
    }

 /*
  * Close the file and return...
  */

  fclose(fp);      
}


/*
 * 'cups_get_dests()' - Get destinations from a file.
 */

static int				/* O - Number of destinations */
cups_get_dests(const char  *filename,	/* I - File to read from */
               int         num_dests,	/* I - Number of destinations */
               cups_dest_t **dests)	/* IO - Destinations */
{
  int		i;			/* Looping var */
  cups_dest_t	*dest;			/* Current destination */
  FILE		*fp;			/* File pointer */
  char		line[8192],		/* Line from file */
		*lineptr,		/* Pointer into line */
		*name,			/* Name of destination/option */
		*instance;		/* Instance of destination */


 /*
  * Try to open the file...
  */

  if ((fp = fopen(filename, "r")) == NULL)
    return (num_dests);

 /*
  * Read each printer; each line looks like:
  *
  *    Dest name[/instance] options
  *    Default name[/instance] options
  */

  while (fgets(line, sizeof(line), fp) != NULL)
  {
   /*
    * See what type of line it is...
    */

    if (strncasecmp(line, "dest", 4) == 0 && isspace(line[4]))
      lineptr = line + 4;
    else if (strncasecmp(line, "default", 7) == 0 && isspace(line[7]))
      lineptr = line + 7;
    else
      continue;

   /*
    * Skip leading whitespace...
    */

    while (isspace(*lineptr))
      lineptr ++;

    if (!*lineptr)
      continue;

    name = lineptr;

   /*
    * Search for an instance...
    */

    while (!isspace(*lineptr) && *lineptr && *lineptr != '/')
      lineptr ++;

    if (!*lineptr)
      continue;

    if (*lineptr == '/')
    {
     /*
      * Found an instance...
      */

      *lineptr++ = '\0';
      instance = lineptr;

     /*
      * Search for an instance...
      */

      while (!isspace(*lineptr) && *lineptr)
	lineptr ++;
    }
    else
      instance = NULL;

    *lineptr++ = '\0';

   /*
    * Add the destination...
    */

    num_dests = cupsAddDest(name, instance, num_dests, dests);

    if ((dest = cupsGetDest(name, instance, num_dests, *dests)) == NULL)
    {
     /*
      * Out of memory!
      */

      fclose(fp);
      return (num_dests);
    }

   /*
    * Add options until we hit the end of the line...
    */

    if (dest->num_options)
    {
     /*
      * Free old options...
      */

      cupsFreeOptions(dest->num_options, dest->options);

      dest->num_options = 0;
      dest->options     = (cups_option_t *)0;
    }

    dest->num_options = cupsParseOptions(lineptr, dest->num_options,
                                         &(dest->options));

   /*
    * Set this as default if needed...
    */

    if (strncasecmp(line, "default", 7) == 0)
    {
      for (i = 0; i < num_dests; i ++)
        (*dests)[i].is_default = 0;

      dest->is_default = 1;
    }
  }

 /*
  * Close the file and return...
  */

  fclose(fp);      

  return (num_dests);
}


/*
 * End of "$Id: dest.c,v 1.9 2000/06/28 16:40:52 mike Exp $".
 */
