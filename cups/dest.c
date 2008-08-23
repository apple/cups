/*
 * "$Id$"
 *
 *   User-defined destination (and option) support for the Common UNIX
 *   Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsAddDest()         - Add a destination to the list of destinations.
 *   cupsFreeDests()       - Free the memory used by the list of destinations.
 *   cupsGetDest()         - Get the named destination from the list.
 *   cupsGetDests()        - Get the list of destinations from the default
 *                           server.
 *   cupsGetDests2()       - Get the list of destinations from the specified
 *                           server.
 *   cupsGetNamedDest()    - Get options for the named destination.
 *   cupsRemoveDest()      - Remove a destination from the destination list.
 *   cupsSetDefaultDest()  - Set the default destination.
 *   cupsSetDests()        - Save the list of destinations for the default
 *                           server.
 *   cupsSetDests2()       - Save the list of destinations for the specified
 *                           server.
 *   appleCopyLocations()  - Get the location history array.
 *   appleCopyNetwork()    - Get the network ID for the current location.
 *   appleGetDefault()     - Get the default printer for this location.
 *   appleGetPrinter()     - Get a printer from the history array.
 *   appleSetDefault()     - Set the default printer for this location.
 *   appleUseLastPrinter() - Get the default printer preference value.
 *   cups_get_default()    - Get the default destination from an lpoptions file.
 *   cups_get_dests()      - Get destinations from a file.
 *   cups_get_sdests()     - Get destinations from a server.
 */

/*
 * Include necessary headers...
 */

#include "debug.h"
#include "globals.h"
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>

#ifdef HAVE_NOTIFY_H
#  include <notify.h>
#endif /* HAVE_NOTIFY_H */

#ifdef __APPLE__
#  include <sys/cdefs.h>
#  include <CoreFoundation/CoreFoundation.h>
#  include <SystemConfiguration/SystemConfiguration.h>
#  define kLocationHistoryArrayKey CFSTR("kLocationHistoryArrayKeyTMP")
#  define kLocationNetworkKey CFSTR("kLocationNetworkKey")
#  define kLocationPrinterIDKey CFSTR("kLocationPrinterIDKey")
#  define kPMPrintingPreferences CFSTR("com.apple.print.PrintingPrefs")
#  define kUseLastPrinterAsCurrentPrinterKey CFSTR("UseLastPrinterAsCurrentPrinter")
#endif /* __APPLE__ */


/*
 * Local functions...
 */

#ifdef __APPLE__
static CFArrayRef appleCopyLocations(void);
static CFStringRef appleCopyNetwork(void);
static char	*appleGetDefault(char *name, int namesize);
static CFStringRef appleGetPrinter(CFArrayRef locations, CFStringRef network,
		                   CFIndex *locindex);
static void	appleSetDefault(const char *name);
static int	appleUseLastPrinter(void);
#endif /* __APPLE__ */
static char	*cups_get_default(const char *filename, char *namebuf,
				    size_t namesize, const char **instance);
static int	cups_get_dests(const char *filename, const char *match_name,
		               const char *match_inst, int num_dests,
		               cups_dest_t **dests);
static int	cups_get_sdests(http_t *http, ipp_op_t op, const char *name,
		                int num_dests, cups_dest_t **dests);


/*
 * 'cupsAddDest()' - Add a destination to the list of destinations.
 *
 * This function cannot be used to add a new class or printer queue,
 * it only adds a new container of saved options for the named
 * destination or instance.
 *
 * If the named destination already exists, the destination list is
 * returned unchanged.  Adding a new instance of a destination creates
 * a copy of that destination's options.
 *
 * Use the @link cupsSaveDests@ function to save the updated list of
 * destinations to the user's lpoptions file.
 */

int					/* O  - New number of destinations */
cupsAddDest(const char  *name,		/* I  - Destination name */
            const char	*instance,	/* I  - Instance name or @code NULL@ for none/primary */
            int         num_dests,	/* I  - Number of destinations */
            cups_dest_t **dests)	/* IO - Destinations */
{
  int		i;			/* Looping var */
  cups_dest_t	*dest;			/* Destination pointer */
  cups_dest_t	*parent;		/* Parent destination */
  cups_option_t	*option;		/* Current option */


  if (!name || !dests)
    return (0);

  if (cupsGetDest(name, instance, num_dests, *dests))
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

 /*
  * Find where to insert the destination...
  */

  for (i = num_dests; i > 0; i --, dest ++)
    if (strcasecmp(name, dest->name) < 0)
      break;
    else if (!instance && dest->instance)
      break;
    else if (!strcasecmp(name, dest->name) &&
             instance  && dest->instance &&
             strcasecmp(instance, dest->instance) < 0)
      break;

  if (i > 0)
    memmove(dest + 1, dest, i * sizeof(cups_dest_t));

 /*
  * Initialize the destination...
  */

  dest->name        = _cupsStrAlloc(name);
  dest->is_default  = 0;
  dest->num_options = 0;
  dest->options     = (cups_option_t *)0;

  if (!instance)
    dest->instance = NULL;
  else
  {
   /*
    * Copy options from the primary instance...
    */

    dest->instance = _cupsStrAlloc(instance);

    if ((parent = cupsGetDest(name, NULL, num_dests + 1, *dests)) != NULL)
    {
      for (i = parent->num_options, option = parent->options;
           i > 0;
	   i --, option ++)
	dest->num_options = cupsAddOption(option->name, option->value,
	                                  dest->num_options,
					  &(dest->options));
    }
  }

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
    _cupsStrFree(dest->name);
    _cupsStrFree(dest->instance);

    cupsFreeOptions(dest->num_options, dest->options);
  }

  free(dests);
}


/*
 * 'cupsGetDest()' - Get the named destination from the list.
 *
 * Use the @link cupsGetDests@ or @link cupsGetDests2@ functions to get a
 * list of supported destinations for the current user.
 */

cups_dest_t *				/* O - Destination pointer or @code NULL@ */
cupsGetDest(const char  *name,		/* I - Destination name or @code NULL@ for the default destination */
            const char	*instance,	/* I - Instance name or @code NULL@ */
            int         num_dests,	/* I - Number of destinations */
            cups_dest_t *dests)		/* I - Destinations */
{
  int	comp;				/* Result of comparison */


  if (num_dests <= 0 || !dests)
    return (NULL);

  if (!name)
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
	if ((!instance && !dests->instance) ||
            (instance != NULL && dests->instance != NULL &&
	     !strcasecmp(instance, dests->instance)))
	  return (dests);
      }

      num_dests --;
      dests ++;
    }
  }

  return (NULL);
}


/*
 * 'cupsGetDests()' - Get the list of destinations from the default server.
 *
 * Starting with CUPS 1.2, the returned list of destinations include the
 * printer-info, printer-is-accepting-jobs, printer-is-shared,
 * printer-make-and-model, printer-state, printer-state-change-time,
 * printer-state-reasons, and printer-type attributes as options.
 *
 * Use the @link cupsFreeDests@ function to free the destination list and
 * the @link cupsGetDest@ function to find a particular destination.
 */

int					/* O - Number of destinations */
cupsGetDests(cups_dest_t **dests)	/* O - Destinations */
{
  return (cupsGetDests2(CUPS_HTTP_DEFAULT, dests));
}


/*
 * 'cupsGetDests2()' - Get the list of destinations from the specified server.
 *
 * Starting with CUPS 1.2, the returned list of destinations include the
 * printer-info, printer-is-accepting-jobs, printer-is-shared,
 * printer-make-and-model, printer-state, printer-state-change-time,
 * printer-state-reasons, and printer-type attributes as options.
 *
 * Use the @link cupsFreeDests@ function to free the destination list and
 * the @link cupsGetDest@ function to find a particular destination.
 *
 * @since CUPS 1.1.21@
 */

int					/* O - Number of destinations */
cupsGetDests2(http_t      *http,	/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
              cups_dest_t **dests)	/* O - Destinations */
{
  int		i;			/* Looping var */
  int		num_dests;		/* Number of destinations */
  cups_dest_t	*dest;			/* Destination pointer */
  const char	*home;			/* HOME environment variable */
  char		filename[1024];		/* Local ~/.cups/lpoptions file */
  const char	*defprinter;		/* Default printer */
  char		name[1024],		/* Copy of printer name */
		*instance;		/* Pointer to instance name */
  int		num_reals;		/* Number of real queues */
  cups_dest_t	*reals;			/* Real queues */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * Range check the input...
  */

  if (!dests)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, _("Bad NULL dests pointer"), 1);
    return (0);
  }

 /*
  * Initialize destination array...
  */

  num_dests = 0;
  *dests    = (cups_dest_t *)0;

 /*
  * Grab the printers and classes...
  */

  num_dests = cups_get_sdests(http, CUPS_GET_PRINTERS, NULL, num_dests, dests);
  num_dests = cups_get_sdests(http, CUPS_GET_CLASSES, NULL, num_dests, dests);

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

#ifdef __APPLE__
  if ((defprinter = appleGetDefault(name, sizeof(name))) == NULL)
#endif /* __APPLE__ */
  defprinter = cupsGetDefault2(http);

  if (defprinter)
  {
   /*
    * Grab printer and instance name...
    */

#ifdef __APPLE__
    if (name != defprinter)
#endif /* __APPLE__ */
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
  * Load the /etc/cups/lpoptions and ~/.cups/lpoptions files...
  */

  snprintf(filename, sizeof(filename), "%s/lpoptions", cg->cups_serverroot);
  num_dests = cups_get_dests(filename, NULL, NULL, num_dests, dests);

  if ((home = getenv("HOME")) != NULL)
  {
    snprintf(filename, sizeof(filename), "%s/.cups/lpoptions", home);
    if (access(filename, 0))
      snprintf(filename, sizeof(filename), "%s/.lpoptions", home);

    num_dests = cups_get_dests(filename, NULL, NULL, num_dests, dests);
  }

 /*
  * Validate the current default destination - this prevents old
  * Default lines in /etc/cups/lpoptions and ~/.cups/lpoptions from
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

  if (num_dests > 0)
    _cupsSetError(IPP_OK, NULL, 0);

  return (num_dests);
}


/*
 * 'cupsGetNamedDest()' - Get options for the named destination.
 *
 * This function is optimized for retrieving a single destination and should
 * be used instead of @link cupsGetDests@ and @link cupsGetDest@ when you either
 * know the name of the destination or want to print to the default destination.
 * If @code NULL@ is returned, the destination does not exist or there is no
 * default destination.
 *
 * If "http" is @code CUPS_HTTP_DEFAULT@, the connection to the default print
 * server will be used.
 *
 * If "name" is @code NULL@, the default printer for the current user will be
 * returned.
 *
 * The returned destination must be freed using @link cupsFreeDests@ with a
 * "num_dests" value of 1.
 *
 * @since CUPS 1.4@
 */

cups_dest_t *				/* O - Destination or @code NULL@ */
cupsGetNamedDest(http_t     *http,	/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
                 const char *name,	/* I - Destination name or @code NULL@ for the default destination */
                 const char *instance)	/* I - Instance name or @code NULL@ */
{
  cups_dest_t	*dest;			/* Destination */
  char		filename[1024],		/* Path to lpoptions */
		defname[256];		/* Default printer name */
  const char	*home = getenv("HOME");	/* Home directory */
  ipp_op_t	op = IPP_GET_PRINTER_ATTRIBUTES;
					/* IPP operation to get server ops */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * If "name" is NULL, find the default destination...
  */

  if (!name)
  {
    if ((name = getenv("LPDEST")) == NULL)
      if ((name = getenv("PRINTER")) != NULL && !strcmp(name, "lp"))
        name = NULL;

    if (!name && home)
    {
     /*
      * No default in the environment, try the user's lpoptions files...
      */

      snprintf(filename, sizeof(filename), "%s/.cups/lpoptions", home);

      if ((name = cups_get_default(filename, defname, sizeof(defname),
				   &instance)) == NULL)
      {
	snprintf(filename, sizeof(filename), "%s/.lpoptions", home);
	name = cups_get_default(filename, defname, sizeof(defname),
				&instance);
      }
    }

    if (!name)
    {
     /*
      * Still not there?  Try the system lpoptions file...
      */

      snprintf(filename, sizeof(filename), "%s/lpoptions",
	       cg->cups_serverroot);
      name = cups_get_default(filename, defname, sizeof(defname), &instance);
    }

    if (!name)
    {
     /*
      * No locally-set default destination, ask the server...
      */

      op = CUPS_GET_DEFAULT;
    }
  }

 /*
  * Get the printer's attributes...
  */

  if (!cups_get_sdests(http, op, name, 0, &dest))
    return (NULL);

  if (instance)
    dest->instance = _cupsStrAlloc(instance);

 /*
  * Then add local options...
  */

  snprintf(filename, sizeof(filename), "%s/lpoptions", cg->cups_serverroot);
  cups_get_dests(filename, name, instance, 1, &dest);

  if (home)
  {
    snprintf(filename, sizeof(filename), "%s/.cups/lpoptions", home);

    if (access(filename, 0))
      snprintf(filename, sizeof(filename), "%s/.lpoptions", home);

    cups_get_dests(filename, name, instance, 1, &dest);
  }

 /*
  * Return the result...
  */

  return (dest);
}


/*
 * 'cupsRemoveDest()' - Remove a destination from the destination list.
 *
 * Removing a destination/instance does not delete the class or printer
 * queue, merely the lpoptions for that destination/instance.  Use the
 * @link cupsSetDests@ or @link cupsSetDests2@ functions to save the new
 * options for the user.
 *
 * @since CUPS 1.3@
 */

int					/* O  - New number of destinations */
cupsRemoveDest(const char  *name,	/* I  - Destination name */
               const char  *instance,	/* I  - Instance name or @code NULL@ */
	       int         num_dests,	/* I  - Number of destinations */
	       cups_dest_t **dests)	/* IO - Destinations */
{
  int		i;			/* Index into destinations */
  cups_dest_t	*dest;			/* Pointer to destination */


 /*
  * Find the destination...
  */

  if ((dest = cupsGetDest(name, instance, num_dests, *dests)) == NULL)
    return (num_dests);

 /*
  * Free memory...
  */

  _cupsStrFree(dest->name);
  _cupsStrFree(dest->instance);
  cupsFreeOptions(dest->num_options, dest->options);

 /*
  * Remove the destination from the array...
  */

  num_dests --;

  i = dest - *dests;

  if (i < num_dests)
    memmove(dest, dest + 1, (num_dests - i) * sizeof(cups_dest_t));

  return (num_dests);
}


/*
 * 'cupsSetDefaultDest()' - Set the default destination.
 *
 * @since CUPS 1.3@
 */

void
cupsSetDefaultDest(
    const char  *name,			/* I - Destination name */
    const char  *instance,		/* I - Instance name or @code NULL@ */
    int         num_dests,		/* I - Number of destinations */
    cups_dest_t *dests)			/* I - Destinations */
{
  int		i;			/* Looping var */
  cups_dest_t	*dest;			/* Current destination */


 /*
  * Range check input...
  */

  if (!name || num_dests <= 0 || !dests)
    return;

 /*
  * Loop through the array and set the "is_default" flag for the matching
  * destination...
  */

  for (i = num_dests, dest = dests; i > 0; i --, dest ++)
    dest->is_default = !strcasecmp(name, dest->name) &&
                       ((!instance && !dest->instance) ||
		        (instance && dest->instance &&
			 !strcasecmp(instance, dest->instance)));
}


/*
 * 'cupsSetDests()' - Save the list of destinations for the default server.
 *
 * This function saves the destinations to /etc/cups/lpoptions when run
 * as root and ~/.cups/lpoptions when run as a normal user.
 */

void
cupsSetDests(int         num_dests,	/* I - Number of destinations */
             cups_dest_t *dests)	/* I - Destinations */
{
  cupsSetDests2(CUPS_HTTP_DEFAULT, num_dests, dests);
}


/*
 * 'cupsSetDests2()' - Save the list of destinations for the specified server.
 *
 * This function saves the destinations to /etc/cups/lpoptions when run
 * as root and ~/.cups/lpoptions when run as a normal user.
 *
 * @since CUPS 1.1.21@
 */

int					/* O - 0 on success, -1 on error */
cupsSetDests2(http_t      *http,	/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
              int         num_dests,	/* I - Number of destinations */
              cups_dest_t *dests)	/* I - Destinations */
{
  int		i, j;			/* Looping vars */
  int		wrote;			/* Wrote definition? */
  cups_dest_t	*dest;			/* Current destination */
  cups_option_t	*option;		/* Current option */
  _ipp_option_t	*match;			/* Matching attribute for option */
  FILE		*fp;			/* File pointer */
#ifndef WIN32
  const char	*home;			/* HOME environment variable */
#endif /* WIN32 */
  char		filename[1024];		/* lpoptions file */
  int		num_temps;		/* Number of temporary destinations */
  cups_dest_t	*temps,			/* Temporary destinations */
		*temp;			/* Current temporary dest */
  const char	*val;			/* Value of temporary option */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * Range check the input...
  */

  if (!num_dests || !dests)
    return (-1);

 /*
  * Get the server destinations...
  */

  num_temps = cups_get_sdests(http, CUPS_GET_PRINTERS, NULL, 0, &temps);
  num_temps = cups_get_sdests(http, CUPS_GET_CLASSES, NULL, num_temps, &temps);

 /*
  * Figure out which file to write to...
  */

  snprintf(filename, sizeof(filename), "%s/lpoptions", cg->cups_serverroot);

#ifndef WIN32
  if (getuid())
  {
   /*
    * Merge in server defaults...
    */

    num_temps = cups_get_dests(filename, NULL, NULL, num_temps, &temps);

   /*
    * Point to user defaults...
    */

    if ((home = getenv("HOME")) != NULL)
    {
     /*
      * Remove the old ~/.lpoptions file...
      */

      snprintf(filename, sizeof(filename), "%s/.lpoptions", home);
      unlink(filename);

     /*
      * Create ~/.cups subdirectory...
      */

      snprintf(filename, sizeof(filename), "%s/.cups", home);
      if (access(filename, 0))
        mkdir(filename, 0700);

      snprintf(filename, sizeof(filename), "%s/.cups/lpoptions", home);
    }
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

#ifndef WIN32
 /*
  * Set the permissions to 0644 when saving to the /etc/cups/lpoptions
  * file...
  */

  if (!getuid())
    fchmod(fileno(fp), 0644);
#endif /* !WIN32 */

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
        * See if this option is a printer attribute; if so, skip it...
	*/

        if ((match = _ippFindOption(option->name)) != NULL &&
	    match->group_tag == IPP_TAG_PRINTER)
	  continue;

       /*
	* See if the server/global options match these; if so, don't
	* write 'em.
	*/

        if (temp &&
	    (val = cupsGetOption(option->name, temp->num_options,
	                         temp->options)) != NULL &&
            !strcasecmp(val, option->value))
	  continue;

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
	  if (strchr(option->value, ' ') ||
	      strchr(option->value, '\\') ||
	      strchr(option->value, '\"') ||
	      strchr(option->value, '\''))
	  {
	   /*
	    * Quote the value...
	    */

	    fprintf(fp, " %s=\"", option->name);

	    for (val = option->value; *val; val ++)
	    {
	      if (strchr("\"\'\\", *val))
	        putc('\\', fp);

              putc(*val, fp);
	    }

	    putc('\"', fp);
          }
	  else
	  {
	   /*
	    * Store the literal value...
	    */

	    fprintf(fp, " %s=%s", option->name, option->value);
          }
	}
	else
	  fprintf(fp, " %s", option->name);
      }

      if (wrote)
        fputs("\n", fp);
    }

 /*
  * Free the temporary destinations and close the file...
  */

  cupsFreeDests(num_temps, temps);

  fclose(fp);

#ifdef __APPLE__
 /*
  * Set the default printer for this location - this allows command-line
  * and GUI applications to share the same default destination...
  */

  if ((dest = cupsGetDest(NULL, NULL, num_dests, dests)) != NULL)
    appleSetDefault(dest->name);
#endif /* __APPLE__ */

#ifdef HAVE_NOTIFY_POST
 /*
  * Send a notification so that MacOS X applications can know about the
  * change, too.
  */

  notify_post("com.apple.printerListChange");
#endif /* HAVE_NOTIFY_POST */

  return (0);
}


#ifdef __APPLE__
/*
 * 'appleCopyLocations()' - Copy the location history array.
 */

static CFArrayRef			/* O - Location array or NULL */
appleCopyLocations(void)
{
  CFArrayRef	locations;		/* Location array */


 /*
  * Look up the location array in the preferences...
  */

  if ((locations = CFPreferencesCopyAppValue(kLocationHistoryArrayKey,
                                             kPMPrintingPreferences)) == NULL)
    return (NULL);

  if (CFGetTypeID(locations) != CFArrayGetTypeID())
  {
    CFRelease(locations);
    return (NULL);
  }

  return (locations);
}


/*
 * 'appleCopyNetwork()' - Get the network ID for the current location.
 */

static CFStringRef			/* O - Network ID */
appleCopyNetwork(void)
{
  SCDynamicStoreRef	dynamicStore;	/* System configuration data */
  CFStringRef		key;		/* Current network configuration key */
  CFDictionaryRef	ip_dict;	/* Network configuration data */
  CFStringRef		network = NULL;	/* Current network ID */
  

  if ((dynamicStore = SCDynamicStoreCreate(NULL, CFSTR("Printing"), NULL,
                                           NULL)) != NULL)
  {
    if ((key = SCDynamicStoreKeyCreateNetworkGlobalEntity(
                   NULL, kSCDynamicStoreDomainState, kSCEntNetIPv4)) != NULL)
    {
      if ((ip_dict = SCDynamicStoreCopyValue(dynamicStore, key)) != NULL)
      {
	if ((network = CFDictionaryGetValue(ip_dict,
	                                    kSCPropNetIPv4Router)) != NULL)
          CFRetain(network);

        CFRelease(ip_dict);
      }

      CFRelease(key);
    }

    CFRelease(dynamicStore);
  }

  return (network);
}


/*
 * 'appleGetDefault()' - Get the default printer for this location.
 */

static char *				/* O - Name or NULL if no default */
appleGetDefault(char *name,		/* I - Name buffer */
                int  namesize)		/* I - Size of name buffer */
{
  CFStringRef		network;	/* Network location */
  CFArrayRef		locations;	/* Location array */
  CFStringRef		locprinter;	/* Current printer */


 /*
  * Use location-based defaults if "use last printer" is selected in the
  * system preferences...
  */

  if (!appleUseLastPrinter())
  {
    DEBUG_puts("appleGetDefault: Not using last printer as default...");
    return (NULL);
  }

 /*
  * Get the current location...
  */

  if ((network = appleCopyNetwork()) == NULL)
  {
    DEBUG_puts("appleGetDefault: Unable to get current network...");
    return (NULL);
  }

#ifdef DEBUG
  CFStringGetCString(network, name, namesize, kCFStringEncodingUTF8);
  DEBUG_printf(("appleGetDefault: network=\"%s\"\n", name));
#endif /* DEBUG */

 /*
  * Lookup the network in the preferences...
  */

  if ((locations = appleCopyLocations()) == NULL)
  {
   /*
    * Missing or bad location array, so no location-based default...
    */

    DEBUG_puts("appleGetDefault: Missing or bad location history array...");

    CFRelease(network);

    return (NULL);
  }
  
  DEBUG_printf(("appleGetDefault: Got location, %d entries...\n",
                (int)CFArrayGetCount(locations)));

  if ((locprinter = appleGetPrinter(locations, network, NULL)) != NULL)
    CFStringGetCString(locprinter, name, namesize, kCFStringEncodingUTF8);
  else
    name[0] = '\0';

  CFRelease(network);
  CFRelease(locations);

  DEBUG_printf(("appleGetDefault: Returning \"%s\"...\n", name));

  return (*name ? name : NULL);
}


/*
 * 'appleGetPrinter()' - Get a printer from the history array.
 */

static CFStringRef			/* O - Printer name or NULL */
appleGetPrinter(CFArrayRef  locations,	/* I - Location array */
                CFStringRef network,	/* I - Network name */
		CFIndex     *locindex)	/* O - Index in array */
{
  CFIndex		i,		/* Looping var */
			count;		/* Number of locations */
  CFDictionaryRef	location;	/* Current location */
  CFStringRef		locnetwork,	/* Current network */
			locprinter;	/* Current printer */


  for (i = 0, count = CFArrayGetCount(locations); i < count; i ++)
    if ((location = CFArrayGetValueAtIndex(locations, i)) != NULL &&
        CFGetTypeID(location) == CFDictionaryGetTypeID())
    {
      if ((locnetwork = CFDictionaryGetValue(location,
                                             kLocationNetworkKey)) != NULL &&
          CFGetTypeID(locnetwork) == CFStringGetTypeID() &&
	  CFStringCompare(network, locnetwork, 0) == kCFCompareEqualTo &&
	  (locprinter = CFDictionaryGetValue(location,
	                                     kLocationPrinterIDKey)) != NULL &&
	  CFGetTypeID(locprinter) == CFStringGetTypeID())
      {
        if (locindex)
	  *locindex = i;

	return (locprinter);
      }
    }

  return (NULL);
}


/*
 * 'appleSetDefault()' - Set the default printer for this location.
 */

static void
appleSetDefault(const char *name)	/* I - Default printer/class name */
{
  CFStringRef		network;	/* Current network */
  CFArrayRef		locations;	/* Old locations array */
  CFIndex		locindex;	/* Index in locations array */
  CFStringRef		locprinter;	/* Current printer */
  CFMutableArrayRef	newlocations;	/* New locations array */
  CFMutableDictionaryRef newlocation;	/* New location */
  CFStringRef		newprinter;	/* New printer */


 /*
  * Get the current location...
  */

  if ((network = appleCopyNetwork()) == NULL)
  {
    DEBUG_puts("appleSetDefault: Unable to get current network...");
    return;
  }

  if ((newprinter = CFStringCreateWithCString(kCFAllocatorDefault, name,
                                              kCFStringEncodingUTF8)) == NULL)
  {
    CFRelease(network);
    return;
  }

 /*
  * Lookup the network in the preferences...
  */

  if ((locations = appleCopyLocations()) != NULL)
    locprinter = appleGetPrinter(locations, network, &locindex);
  else
  {
    locprinter = NULL;
    locindex   = -1;
  }

  if (!locprinter ||
      CFStringCompare(locprinter, newprinter, 0) != kCFCompareEqualTo)
  {
   /*
    * Need to change the locations array...
    */

    if (locations)
    {
      newlocations = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0,
                                              locations);

      if (locprinter)
        CFArrayRemoveValueAtIndex(newlocations, locindex);
    }
    else
      newlocations = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);

    newlocation = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);

    if (newlocation && newlocations)
    {
     /*
      * Put the new location at the front of the array...
      */

      CFDictionaryAddValue(newlocation, kLocationNetworkKey, network);
      CFDictionaryAddValue(newlocation, kLocationPrinterIDKey, newprinter);
      CFArrayInsertValueAtIndex(newlocations, 0, newlocation);

     /*
      * Limit the number of locations to 10...
      */

      while (CFArrayGetCount(newlocations) > 10)
        CFArrayRemoveValueAtIndex(newlocations, 10);

     /*
      * Push the changes out...
      */

      CFPreferencesSetAppValue(kLocationHistoryArrayKey, newlocations,
                               kPMPrintingPreferences);
      CFPreferencesAppSynchronize(kPMPrintingPreferences);
    }

    if (newlocations)
      CFRelease(newlocations);

    if (newlocation)
      CFRelease(newlocation);
  }

  if (locations)
    CFRelease(locations);

  CFRelease(network);
  CFRelease(newprinter);
}


/*
 * 'appleUseLastPrinter()' - Get the default printer preference value.
 */

static int				/* O - 1 to use last printer, 0 otherwise */
appleUseLastPrinter(void)
{
  CFPropertyListRef	uselast;	/* Use last printer preference value */


  if ((uselast = CFPreferencesCopyAppValue(kUseLastPrinterAsCurrentPrinterKey,
                                           kPMPrintingPreferences)) != NULL)
  {
    CFRelease(uselast);

    if (uselast == kCFBooleanFalse)
      return (0);
  }

  return (1);
}
#endif /* __APPLE__ */


/*
 * 'cups_get_default()' - Get the default destination from an lpoptions file.
 */

static char *				/* O - Default destination or NULL */
cups_get_default(const char *filename,	/* I - File to read */
                 char       *namebuf,	/* I - Name buffer */
		 size_t     namesize,	/* I - Size of name buffer */
		 const char **instance)	/* I - Instance */
{
  cups_file_t	*fp;			/* lpoptions file */
  char		line[8192],		/* Line from file */
		*value,			/* Value for line */
		*nameptr;		/* Pointer into name */
  int		linenum;		/* Current line */  


  *namebuf = '\0';

  if ((fp = cupsFileOpen(filename, "r")) != NULL)
  {
    linenum  = 0;

    while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
    {
      if (!strcasecmp(line, "default") && value)
      {
        strlcpy(namebuf, value, namesize);

	if ((nameptr = strchr(namebuf, ' ')) != NULL)
	  *nameptr = '\0';
	if ((nameptr = strchr(namebuf, '\t')) != NULL)
	  *nameptr = '\0';

	if ((nameptr = strchr(namebuf, '/')) != NULL)
	  *nameptr++ = '\0';

        *instance = nameptr;
	break;
      }
    }

    cupsFileClose(fp);
  }

  return (*namebuf ? namebuf : NULL);
}


/*
 * 'cups_get_dests()' - Get destinations from a file.
 */

static int				/* O - Number of destinations */
cups_get_dests(const char  *filename,	/* I - File to read from */
               const char  *match_name,	/* I - Destination name we want */
	       const char  *match_inst,	/* I - Instance name we want */
               int         num_dests,	/* I - Number of destinations */
               cups_dest_t **dests)	/* IO - Destinations */
{
  int		i;			/* Looping var */
  cups_dest_t	*dest;			/* Current destination */
  cups_file_t	*fp;			/* File pointer */
  char		line[8192],		/* Line from file */
		*lineptr,		/* Pointer into line */
		*name,			/* Name of destination/option */
		*instance;		/* Instance of destination */
  int		linenum;		/* Current line number */
  const char	*printer;		/* PRINTER or LPDEST */


  DEBUG_printf(("cups_get_dests(filename=\"%s\", match_name=\"%s\", "
                "match_inst=\"%s\", num_dests=%d, dests=%p)\n", filename,
		match_name ? match_name : "(null)",
		match_inst ? match_inst : "(null)", num_dests, dests));

 /*
  * Try to open the file...
  */

  if ((fp = cupsFileOpen(filename, "r")) == NULL)
    return (num_dests);

 /*
  * Check environment variables...
  */

  if ((printer = getenv("LPDEST")) == NULL)
    if ((printer = getenv("PRINTER")) != NULL)
      if (strcmp(printer, "lp") == 0)
        printer = NULL;

  DEBUG_printf(("cups_get_dests: printer=\"%s\"\n",
                printer ? printer : "(null)"));

 /*
  * Read each printer; each line looks like:
  *
  *    Dest name[/instance] options
  *    Default name[/instance] options
  */

  linenum = 0;

  while (cupsFileGetConf(fp, line, sizeof(line), &lineptr, &linenum))
  {
   /*
    * See what type of line it is...
    */

    DEBUG_printf(("cups_get_dests: linenum=%d line=\"%s\" lineptr=\"%s\"\n",
                  linenum, line, lineptr ? lineptr : "(null)"));

    if ((strcasecmp(line, "dest") && strcasecmp(line, "default")) || !lineptr)
    {
      DEBUG_puts("cups_get_dests: Not a dest or default line...");
      continue;
    }

    name = lineptr;

   /*
    * Search for an instance...
    */

    while (!isspace(*lineptr & 255) && *lineptr && *lineptr != '/')
      lineptr ++;

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

    if (*lineptr)
      *lineptr++ = '\0';

    DEBUG_printf(("cups_get_dests: name=\"%s\", instance=\"%s\"\n", name,
                  instance));

   /*
    * See if the primary instance of the destination exists; if not,
    * ignore this entry and move on...
    */

    if (match_name)
    {
      if (strcasecmp(name, match_name) ||
          (!instance && match_inst) ||
	  (instance && !match_inst) ||
	  (instance && strcasecmp(instance, match_inst)))
	continue;

      dest = *dests;
    }
    else if (cupsGetDest(name, NULL, num_dests, *dests) == NULL)
    {
      DEBUG_puts("cups_get_dests: Not found!");
      continue;
    }
    else
    {
     /*
      * Add the destination...
      */

      num_dests = cupsAddDest(name, instance, num_dests, dests);

      if ((dest = cupsGetDest(name, instance, num_dests, *dests)) == NULL)
      {
       /*
	* Out of memory!
	*/

        DEBUG_puts("cups_get_dests: Out of memory!");
        break;
      }
    }

   /*
    * Add options until we hit the end of the line...
    */

    dest->num_options = cupsParseOptions(lineptr, dest->num_options,
                                         &(dest->options));

   /*
    * If we found what we were looking for, stop now...
    */

    if (match_name)
      break;

   /*
    * Set this as default if needed...
    */

    if (!printer && !strcasecmp(line, "default"))
    {
      DEBUG_puts("cups_get_dests: Setting as default...");

      for (i = 0; i < num_dests; i ++)
        (*dests)[i].is_default = 0;

      dest->is_default = 1;
    }
  }

 /*
  * Close the file and return...
  */

  cupsFileClose(fp);      

  return (num_dests);
}


/*
 * 'cups_get_sdests()' - Get destinations from a server.
 */

static int				/* O - Number of destinations */
cups_get_sdests(http_t      *http,	/* I - Connection to server or CUPS_HTTP_DEFAULT */
                ipp_op_t    op,		/* I - IPP operation */
		const char  *name,	/* I - Name of destination */
                int         num_dests,	/* I - Number of destinations */
                cups_dest_t **dests)	/* IO - Destinations */
{
  int		i;			/* Looping var */
  cups_dest_t	*dest;			/* Current destination */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  int		accepting,		/* printer-is-accepting-jobs attribute */
		shared,			/* printer-is-shared attribute */
		state,			/* printer-state attribute */
		change_time,		/* printer-state-change-time attribute */
		type;			/* printer-type attribute */
  const char	*info,			/* printer-info attribute */
		*location,		/* printer-location attribute */
		*make_model,		/* printer-make-and-model attribute */
		*printer_name;		/* printer-name attribute */
  char		uri[1024],		/* printer-uri value */
		job_sheets[1024],	/* job-sheets-default attribute */
		auth_info_req[1024],	/* auth-info-required attribute */
		reasons[1024];		/* printer-state-reasons attribute */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  char		optname[1024],		/* Option name */
		value[2048],		/* Option value */
		*ptr;			/* Pointer into name/value */
  static const char * const pattrs[] =	/* Attributes we're interested in */
		{
		  "auth-info-required",
		  "job-sheets-default",
		  "printer-info",
		  "printer-is-accepting-jobs",
		  "printer-is-shared",
		  "printer-location",
		  "printer-make-and-model",
		  "printer-name",
		  "printer-state",
		  "printer-state-change-time",
		  "printer-state-reasons",
		  "printer-type",
		  "printer-defaults"
		};


 /*
  * Build a CUPS_GET_PRINTERS or CUPS_GET_CLASSES request, which require
  * the following attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requesting-user-name
  */

  request = ippNewRequest(op);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(pattrs) / sizeof(pattrs[0]),
		NULL, pattrs);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, cupsUser());

  if (name)
  {
    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                     "localhost", ippPort(), "/printers/%s", name);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL,
                 uri);
  }

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
      * Pull the needed attributes from this printer...
      */

      accepting    = 0;
      change_time  = 0;
      info         = NULL;
      location     = NULL;
      make_model   = NULL;
      printer_name = NULL;
      num_options  = 0;
      options      = NULL;
      shared       = 1;
      state        = IPP_PRINTER_IDLE;
      type         = CUPS_PRINTER_LOCAL;

      auth_info_req[0] = '\0';
      job_sheets[0]    = '\0';
      reasons[0]       = '\0';

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (!strcmp(attr->name, "auth-info-required") &&
	    attr->value_tag == IPP_TAG_KEYWORD)
        {
	  strlcpy(auth_info_req, attr->values[0].string.text,
		  sizeof(auth_info_req));

	  for (i = 1, ptr = auth_info_req + strlen(auth_info_req);
	       i < attr->num_values;
	       i ++)
	  {
	    snprintf(ptr, sizeof(auth_info_req) - (ptr - auth_info_req), ",%s",
	             attr->values[i].string.text);
	    ptr += strlen(ptr);
	  }
        }
        else if (!strcmp(attr->name, "job-sheets-default") &&
	         (attr->value_tag == IPP_TAG_KEYWORD ||
	          attr->value_tag == IPP_TAG_NAME))
        {
	  if (attr->num_values == 2)
	    snprintf(job_sheets, sizeof(job_sheets), "%s,%s",
	             attr->values[0].string.text, attr->values[1].string.text);
	  else
	    strlcpy(job_sheets, attr->values[0].string.text,
	            sizeof(job_sheets));
        }
        else if (!strcmp(attr->name, "printer-info") &&
	         attr->value_tag == IPP_TAG_TEXT)
	  info = attr->values[0].string.text;
	else if (!strcmp(attr->name, "printer-is-accepting-jobs") &&
	         attr->value_tag == IPP_TAG_BOOLEAN)
          accepting = attr->values[0].boolean;
	else if (!strcmp(attr->name, "printer-is-shared") &&
	         attr->value_tag == IPP_TAG_BOOLEAN)
          shared = attr->values[0].boolean;
        else if (!strcmp(attr->name, "printer-location") &&
	         attr->value_tag == IPP_TAG_TEXT)
	  location = attr->values[0].string.text;
        else if (!strcmp(attr->name, "printer-make-and-model") &&
	         attr->value_tag == IPP_TAG_TEXT)
	  make_model = attr->values[0].string.text;
        else if (!strcmp(attr->name, "printer-name") &&
	         attr->value_tag == IPP_TAG_NAME)
	  printer_name = attr->values[0].string.text;
	else if (!strcmp(attr->name, "printer-state") &&
	         attr->value_tag == IPP_TAG_ENUM)
          state = attr->values[0].integer;
	else if (!strcmp(attr->name, "printer-state-change-time") &&
	         attr->value_tag == IPP_TAG_INTEGER)
          change_time = attr->values[0].integer;
        else if (!strcmp(attr->name, "printer-state-reasons") &&
	         attr->value_tag == IPP_TAG_KEYWORD)
	{
	  strlcpy(reasons, attr->values[0].string.text, sizeof(reasons));
	  for (i = 1, ptr = reasons + strlen(reasons);
	       i < attr->num_values;
	       i ++)
	  {
	    snprintf(ptr, sizeof(reasons) - (ptr - reasons), ",%s",
	             attr->values[i].string.text);
	    ptr += strlen(ptr);
	  }
	}
	else if (!strcmp(attr->name, "printer-type") &&
	         attr->value_tag == IPP_TAG_ENUM)
          type = attr->values[0].integer;
        else if (strncmp(attr->name, "notify-", 7) &&
	         (attr->value_tag == IPP_TAG_BOOLEAN ||
		  attr->value_tag == IPP_TAG_ENUM ||
		  attr->value_tag == IPP_TAG_INTEGER ||
		  attr->value_tag == IPP_TAG_KEYWORD ||
		  attr->value_tag == IPP_TAG_NAME ||
		  attr->value_tag == IPP_TAG_RANGE) &&
		 strstr(attr->name, "-default"))
	{
	  char	*valptr;		/* Pointer into attribute value */


	 /*
	  * Add a default option...
	  */

          strlcpy(optname, attr->name, sizeof(optname));
	  if ((ptr = strstr(optname, "-default")) != NULL)
	    *ptr = '\0';

          value[0] = '\0';
	  for (i = 0, ptr = value; i < attr->num_values; i ++)
	  {
	    if (ptr >= (value + sizeof(value) - 1))
	      break;

            if (i)
	      *ptr++ = ',';

            switch (attr->value_tag)
	    {
	      case IPP_TAG_INTEGER :
	      case IPP_TAG_ENUM :
	          snprintf(ptr, sizeof(value) - (ptr - value), "%d",
		           attr->values[i].integer);
	          break;

	      case IPP_TAG_BOOLEAN :
	          if (attr->values[i].boolean)
		    strlcpy(ptr, "true", sizeof(value) - (ptr - value));
		  else
		    strlcpy(ptr, "false", sizeof(value) - (ptr - value));
	          break;

	      case IPP_TAG_RANGE :
	          if (attr->values[i].range.lower ==
		          attr->values[i].range.upper)
	            snprintf(ptr, sizeof(value) - (ptr - value), "%d",
		             attr->values[i].range.lower);
		  else
	            snprintf(ptr, sizeof(value) - (ptr - value), "%d-%d",
		             attr->values[i].range.lower,
			     attr->values[i].range.upper);
	          break;

	      default :
		  for (valptr = attr->values[i].string.text;
		       *valptr && ptr < (value + sizeof(value) - 2);)
		  {
	            if (strchr(" \t\n\\\'\"", *valptr))
		      *ptr++ = '\\';

		    *ptr++ = *valptr++;
		  }

		  *ptr = '\0';
	          break;
	    }

	    ptr += strlen(ptr);
          }

	  num_options = cupsAddOption(optname, value, num_options, &options);
	}

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (!printer_name)
      {
        cupsFreeOptions(num_options, options);

        if (attr == NULL)
	  break;
	else
          continue;
      }

      num_dests = cupsAddDest(printer_name, NULL, num_dests, dests);

      if ((dest = cupsGetDest(printer_name, NULL, num_dests, *dests)) != NULL)
      {
        dest->num_options = num_options;
	dest->options     = options;

        num_options = 0;
	options     = NULL;

        if (auth_info_req[0])
          dest->num_options = cupsAddOption("auth-info-required", auth_info_req,
	                                    dest->num_options,
	                                    &(dest->options));

        if (job_sheets[0])
          dest->num_options = cupsAddOption("job-sheets", job_sheets,
	                                    dest->num_options,
	                                    &(dest->options));

        if (info)
          dest->num_options = cupsAddOption("printer-info", info,
	                                    dest->num_options,
	                                    &(dest->options));

        sprintf(value, "%d", accepting);
	dest->num_options = cupsAddOption("printer-is-accepting-jobs", value,
					  dest->num_options,
					  &(dest->options));

        sprintf(value, "%d", shared);
	dest->num_options = cupsAddOption("printer-is-shared", value,
					  dest->num_options,
					  &(dest->options));

        if (location)
          dest->num_options = cupsAddOption("printer-location",
	                                    location, dest->num_options,
	                                    &(dest->options));

        if (make_model)
          dest->num_options = cupsAddOption("printer-make-and-model",
	                                    make_model, dest->num_options,
	                                    &(dest->options));

        sprintf(value, "%d", state);
	dest->num_options = cupsAddOption("printer-state", value,
					  dest->num_options,
					  &(dest->options));

        if (change_time)
	{
	  sprintf(value, "%d", change_time);
	  dest->num_options = cupsAddOption("printer-state-change-time", value,
					    dest->num_options,
					    &(dest->options));
        }

        if (reasons[0])
          dest->num_options = cupsAddOption("printer-state-reasons", reasons,
					    dest->num_options,
	                                    &(dest->options));

        sprintf(value, "%d", type);
	dest->num_options = cupsAddOption("printer-type", value,
					  dest->num_options,
					  &(dest->options));
      }

      cupsFreeOptions(num_options, options);

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
 * End of "$Id$".
 */
