/*
 * "$Id: dest.c,v 1.18.2.17 2004/06/29 03:46:29 mike Exp $"
 *
 *   User-defined destination (and option) support for the Common UNIX
 *   Printing System (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products.
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
 *       Hollywood, Maryland 20636-3142 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsAddDest()     - Add a destination to the list of destinations.
 *   cupsFreeDests()   - Free the memory used by the list of destinations.
 *   cupsGetDest()     - Get the named destination from the list.
 *   cupsGetDests()    - Get the list of destinations.
 *   cupsGetDests2()   - Get the list of destinations using a HTTP connection.
 *   cupsSetDests()    - Set the list of destinations.
 *   cups_get_dests()  - Get destinations from a file.
 *   cups_get_sdests() - Get destinations from a server.
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include "language.h"
#include "string.h"
#include <stdlib.h>
#include <ctype.h>


/*
 * Local functions...
 */

static int	cups_get_dests(const char *filename, int num_dests,
		               cups_dest_t **dests);
static int	cups_get_sdests(http_t *http, ipp_op_t op, int num_dests,
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
    else if (strcasecmp(name, dest->name) == 0 &&
             instance != NULL && dest->instance != NULL &&
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
  int		num_dests;		/* Number of destinations */
  http_t	*http;			/* HTTP connection */


 /*
  * Connect to the CUPS server and get the destination list and options...
  */

  http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption());

  num_dests = cupsGetDests2(http, dests);

  if (http)
    httpClose(http);

  return (num_dests);
}


/*
 * 'cupsGetDests2()' - Get the list of destinations.
 */

int					/* O - Number of destinations */
cupsGetDests2(http_t      *http,	/* I - HTTP connection */
              cups_dest_t **dests)	/* O - Destinations */
{
  int		i;			/* Looping var */
  int		num_dests;		/* Number of destinations */
  cups_dest_t	*dest;			/* Destination pointer */
  const char	*home;			/* HOME environment variable */
  char		filename[1024];		/* Local ~/.lpoptions file */
  const char	*defprinter;		/* Default printer */
  char		name[1024],		/* Copy of printer name */
		*instance;		/* Pointer to instance name */
  int		num_reals;		/* Number of real queues */
  cups_dest_t	*reals;			/* Real queues */


 /*
  * Range check the input...
  */

  if (!http || !dests)
    return (0);

 /*
  * Initialize destination array...
  */

  num_dests = 0;
  *dests    = (cups_dest_t *)0;

 /*
  * Grab the printers and classes...
  */

  num_dests = cups_get_sdests(http, CUPS_GET_PRINTERS, num_dests, dests);
  num_dests = cups_get_sdests(http, CUPS_GET_CLASSES, num_dests, dests);

 /*
  * Make a copy of the "real" queues for a later sanity check...
  */

  if (num_dests > 0)
  {
    num_reals = num_dests;
    reals     = calloc(num_reals, sizeof(cups_dest_t));

    if (reals)
      memcpy(reals, *dests, num_reals * sizeof(cups_dest_t));
    else
      num_reals = 0;
  }
  else
  {
    num_reals = 0;
    reals     = NULL;
  }

 /*
  * Grab the default destination...
  */

  if ((defprinter = cupsGetDefault2(http)) != NULL)
  {
   /*
    * Grab printer and instance name...
    */

    strlcpy(name, defprinter, sizeof(name));

    if ((instance = strchr(name, '/')) != NULL)
      *instance++ = '\0';

   /*
    * Lookup the printer and instance and make it the default...
    */

    if ((dest = cupsGetDest(name, instance, num_dests, *dests)) != NULL)
      dest->is_default = 1;
  }
  else
  {
   /*
    * This initialization of "instance" is unnecessary, but avoids a
    * compiler warning...
    */

    instance = NULL;
  }

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
  * Validate the current default destination - this prevents old
  * Default lines in /etc/cups/lpoptions and ~/.lpoptions from
  * pointing to a non-existent printer or class...
  */

  if (num_reals)
  {
   /*
    * See if we have a default printer...
    */

    if ((dest = cupsGetDest(NULL, NULL, num_dests, *dests)) != NULL)
    {
     /*
      * Have a default; see if it is real...
      */

      dest = cupsGetDest(dest->name, NULL, num_reals, reals);
    }

   /*
    * If dest is NULL, then no default (that exists) is set, so we
    * need to set a default if one exists...
    */

    if (dest == NULL && defprinter != NULL)
    {
      for (i = 0; i < num_dests; i ++)
        (*dests)[i].is_default = 0;

      if ((dest = cupsGetDest(name, instance, num_dests, *dests)) != NULL)
	dest->is_default = 1;
    }

   /*
    * Free memory...
    */

    free(reals);
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
  http_t	*http;			/* HTTP connection */


 /*
  * Connect to the CUPS server and save the destination list and options...
  */

  http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption());

  cupsSetDests2(http, num_dests, dests);

  if (http)
    httpClose(http);
}


/*
 * 'cupsSetDests()' - Set the list of destinations.
 */

int					/* O - 0 on success, -1 on error */
cupsSetDests2(http_t      *http,	/* I - HTTP connection */
              int         num_dests,	/* I - Number of destinations */
              cups_dest_t *dests)	/* I - Destinations */
{
  int		i, j;			/* Looping vars */
  int		wrote;			/* Wrote definition? */
  cups_dest_t	*dest;			/* Current destination */
  cups_option_t	*option;		/* Current option */
  FILE		*fp;			/* File pointer */
  const char	*home;			/* HOME environment variable */
  char		filename[1024];		/* lpoptions file */
  int		num_temps;		/* Number of temporary destinations */
  cups_dest_t	*temps,			/* Temporary destinations */
		*temp;			/* Current temporary dest */
  const char	*val;			/* Value of temporary option */


 /*
  * Range check the input...
  */

  if (!http || !num_dests || !dests)
    return (-1);

 /*
  * Get the server destinations...
  */

  num_temps = cups_get_sdests(http, CUPS_GET_PRINTERS, 0, &temps);
  num_temps = cups_get_sdests(http, CUPS_GET_CLASSES, num_temps, &temps);

 /*
  * Figure out which file to write to...
  */

  if ((home = getenv("CUPS_SERVERROOT")) != NULL)
    snprintf(filename, sizeof(filename), "%s/lpoptions", home);
  else
    strcpy(filename, CUPS_SERVERROOT "/lpoptions");

#ifndef WIN32
  if (getuid())
  {
   /*
    * Merge in server defaults...
    */

    num_temps = cups_get_dests(filename, num_temps, &temps);

   /*
    * Point to user defaults...
    */

    if ((home = getenv("HOME")) != NULL)
      snprintf(filename, sizeof(filename), "%s/.lpoptions", home);
  }
#endif /* !WIN32 */

 /*
  * Try to open the file...
  */

  if ((fp = fopen(filename, "w")) == NULL)
  {
    cupsFreeDests(num_temps, temps);
    return (-1);
  }

 /*
  * Write each printer; each line looks like:
  *
  *    Dest name[/instance] options
  *    Default name[/instance] options
  */

  for (i = num_dests, dest = dests; i > 0; i --, dest ++)
    if (dest->instance != NULL || dest->num_options != 0 || dest->is_default)
    {
      if (dest->is_default)
      {
	fprintf(fp, "Default %s", dest->name);
	if (dest->instance)
	  fprintf(fp, "/%s", dest->instance);

        wrote = 1;
      }
      else
        wrote = 0;

      if ((temp = cupsGetDest(dest->name, dest->instance, num_temps, temps)) == NULL)
        temp = cupsGetDest(dest->name, NULL, num_temps, temps);

      for (j = dest->num_options, option = dest->options; j > 0; j --, option ++)
      {
       /*
	* See if the server/global options match these; if so, don't
	* write 'em.
	*/

        if (temp && (val = cupsGetOption(option->name, temp->num_options,
	                                 temp->options)) != NULL)
	{
	  if (strcasecmp(val, option->value) == 0)
	    continue;
	}

       /*
        * Options don't match, write to the file...
	*/

        if (!wrote)
	{
	  fprintf(fp, "Dest %s", dest->name);
	  if (dest->instance)
	    fprintf(fp, "/%s", dest->instance);
          wrote = 1;
	}
        
        if (option->value[0])
	{
	  if (strchr(option->value, ' ') != NULL)
	    fprintf(fp, " %s=\"%s\"", option->name, option->value);
          else
	    fprintf(fp, " %s=%s", option->name, option->value);
	}
	else
	  fprintf(fp, " %s", option->name);
      }

      if (wrote)
        fputs("\n", fp);
    }

 /*
  * Free the temporary destinations...
  */

  cupsFreeDests(num_temps, temps);

 /*
  * Close the file and return...
  */

  fclose(fp);

  return (0);
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
  const char	*printer;		/* PRINTER or LPDEST */


 /*
  * Check environment variables...
  */

  if ((printer = getenv("LPDEST")) == NULL)
    if ((printer = getenv("PRINTER")) != NULL)
      if (strcmp(printer, "lp") == 0)
        printer = NULL;

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

    if (strncasecmp(line, "dest", 4) == 0 && isspace(line[4] & 255))
      lineptr = line + 4;
    else if (strncasecmp(line, "default", 7) == 0 && isspace(line[7] & 255))
      lineptr = line + 7;
    else
      continue;

   /*
    * Skip leading whitespace...
    */

    while (isspace(*lineptr & 255))
      lineptr ++;

    if (!*lineptr)
      continue;

    name = lineptr;

   /*
    * Search for an instance...
    */

    while (!isspace(*lineptr & 255) && *lineptr && *lineptr != '/')
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

      while (!isspace(*lineptr & 255) && *lineptr)
	lineptr ++;
    }
    else
      instance = NULL;

    *lineptr++ = '\0';

   /*
    * See if the primary instance of the destination exists; if not,
    * ignore this entry and move on...
    */

    if (cupsGetDest(name, NULL, num_dests, *dests) == NULL)
      continue;

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

    dest->num_options = cupsParseOptions(lineptr, dest->num_options,
                                         &(dest->options));

   /*
    * Set this as default if needed...
    */

    if (strncasecmp(line, "default", 7) == 0 && printer == NULL)
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
 * 'cups_get_sdests()' - Get destinations from a server.
 */

static int				/* O - Number of destinations */
cups_get_sdests(http_t      *http,	/* I - HTTP connection */
                ipp_op_t    op,		/* I - get-printers or get-classes */
                int         num_dests,	/* I - Number of destinations */
                cups_dest_t **dests)	/* IO - Destinations */
{
  cups_dest_t	*dest;			/* Current destination */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  cups_lang_t	*language;		/* Default language */
  const char	*name;			/* printer-name attribute */
  char		job_sheets[1024];	/* job-sheets option */
  static const char * const pattrs[] =	/* Attributes we're interested in */
		{
		  "printer-name",
		  "job-sheets-default"
		};


 /*
  * Build a CUPS_GET_PRINTERS or CUPS_GET_CLASSES request, which require
  * the following attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNew();

  request->request.op.operation_id = op;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  cupsLangFree(language);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(pattrs) / sizeof(pattrs[0]),
		NULL, pattrs);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a printer...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this job...
      */

      name = NULL;

      strcpy(job_sheets, "");

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (strcmp(attr->name, "printer-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  name = attr->values[0].string.text;

        if (strcmp(attr->name, "job-sheets-default") == 0 &&
	    (attr->value_tag == IPP_TAG_KEYWORD ||
	     attr->value_tag == IPP_TAG_NAME))
        {
	  if (attr->num_values == 2)
	    snprintf(job_sheets, sizeof(job_sheets), "%s,%s",
	             attr->values[0].string.text, attr->values[1].string.text);
	  else
	    strcpy(job_sheets, attr->values[0].string.text);
        }

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (!name)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

      num_dests = cupsAddDest(name, NULL, num_dests, dests);

      if ((dest = cupsGetDest(name, NULL, num_dests, *dests)) != NULL)
        if (job_sheets[0])
          dest->num_options = cupsAddOption("job-sheets", job_sheets, 0,
	                                    &(dest->options));

      if (attr == NULL)
	break;
    }

    ippDelete(response);
  }

 /*
  * Return the count...
  */

  return (num_dests);
}


/*
 * End of "$Id: dest.c,v 1.18.2.17 2004/06/29 03:46:29 mike Exp $".
 */
