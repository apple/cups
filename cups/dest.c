/*
 * "$Id: dest.c 9568 2011-02-25 06:13:56Z mike $"
 *
 *   User-defined destination (and option) support for CUPS.
 *
 *   Copyright 2007-2011 by Apple Inc.
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
 *   cupsAddDest()                  - Add a destination to the list of
 *                                    destinations.
 *   _cupsAppleCopyDefaultPaperID() - Get the default paper ID.
 *   _cupsAppleCopyDefaultPrinter() - Get the default printer at this location.
 *   _cupsAppleGetUseLastPrinter()  - Get whether to use the last used printer.
 *   _cupsAppleSetDefaultPaperID()  - Set the default paper id.
 *   _cupsAppleSetDefaultPrinter()  - Set the default printer for this location.
 *   _cupsAppleSetUseLastPrinter()  - Set whether to use the last used printer.
 *   cupsFreeDests()                - Free the memory used by the list of
 *                                    destinations.
 *   cupsGetDest()                  - Get the named destination from the list.
 *   _cupsGetDests()                - Get destinations from a server.
 *   cupsGetDests()                 - Get the list of destinations from the
 *                                    default server.
 *   cupsGetDests2()                - Get the list of destinations from the
 *                                    specified server.
 *   cupsGetNamedDest()             - Get options for the named destination.
 *   cupsRemoveDest()               - Remove a destination from the destination
 *                                    list.
 *   cupsSetDefaultDest()           - Set the default destination.
 *   cupsSetDests()                 - Save the list of destinations for the
 *                                    default server.
 *   cupsSetDests2()                - Save the list of destinations for the
 *                                    specified server.
 *   _cupsUserDefault()             - Get the user default printer from
 *                                    environment variables and location
 *                                    information.
 *   appleCopyLocations()           - Copy the location history array.
 *   appleCopyNetwork()             - Get the network ID for the current
 *                                    location.
 *   appleGetPaperSize()            - Get the default paper size.
 *   appleGetPrinter()              - Get a printer from the history array.
 *   cups_add_dest()                - Add a destination to the array.
 *   cups_compare_dests()           - Compare two destinations.
 *   cups_find_dest()               - Find a destination using a binary search.
 *   cups_get_default()             - Get the default destination from an
 *                                    lpoptions file.
 *   cups_get_dests()               - Get destinations from a file.
 *   cups_make_string()             - Make a comma-separated string of values
 *                                    from an IPP attribute.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include <sys/stat.h>

#ifdef HAVE_NOTIFY_H
#  include <notify.h>
#endif /* HAVE_NOTIFY_H */

#ifdef __APPLE__
#  include <SystemConfiguration/SystemConfiguration.h>
#  define kCUPSPrintingPrefs	CFSTR("org.cups.PrintingPrefs")
#  define kDefaultPaperIDKey	CFSTR("DefaultPaperID")
#  define kLastUsedPrintersKey	CFSTR("LastUsedPrinters")
#  define kLocationNetworkKey	CFSTR("Network")
#  define kLocationPrinterIDKey	CFSTR("PrinterID")
#  define kUseLastPrinter	CFSTR("UseLastPrinter")
#endif /* __APPLE__ */


/*
 * Local functions...
 */

#ifdef __APPLE__
static CFArrayRef appleCopyLocations(void);
static CFStringRef appleCopyNetwork(void);
static char	*appleGetPaperSize(char *name, int namesize);
static CFStringRef appleGetPrinter(CFArrayRef locations, CFStringRef network,
		                   CFIndex *locindex);
#endif /* __APPLE__ */
static cups_dest_t *cups_add_dest(const char *name, const char *instance,
		                  int *num_dests, cups_dest_t **dests);
static int	cups_compare_dests(cups_dest_t *a, cups_dest_t *b);
static int	cups_find_dest(const char *name, const char *instance,
			       int num_dests, cups_dest_t *dests, int prev,
			       int *rdiff);
static char	*cups_get_default(const char *filename, char *namebuf,
				  size_t namesize, const char **instance);
static int	cups_get_dests(const char *filename, const char *match_name,
		               const char *match_inst, int user_default_set,
			       int num_dests, cups_dest_t **dests);
static char	*cups_make_string(ipp_attribute_t *attr, char *buffer,
		                  size_t bufsize);


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
  cups_dest_t	*parent = NULL;		/* Parent destination */
  cups_option_t	*doption,		/* Current destination option */
		*poption;		/* Current parent option */


  if (!name || !dests)
    return (0);

  if (!cupsGetDest(name, instance, num_dests, *dests))
  {
    if (instance && !cupsGetDest(name, NULL, num_dests, *dests))
      return (num_dests);

    dest = cups_add_dest(name, instance, &num_dests, dests);

   /*
    * Find the base dest again now the array has been realloc'd.
    */

    parent = cupsGetDest(name, NULL, num_dests, *dests);

    if (instance && parent && parent->num_options > 0)
    {
     /*
      * Copy options from parent...
      */

      dest->options = calloc(sizeof(cups_option_t), parent->num_options);

      if (dest->options)
      {
        dest->num_options = parent->num_options;

	for (i = dest->num_options, doption = dest->options,
	         poption = parent->options;
	     i > 0;
	     i --, doption ++, poption ++)
	{
	  doption->name  = _cupsStrRetain(poption->name);
	  doption->value = _cupsStrRetain(poption->value);
	}
      }
    }
  }

  return (num_dests);
}


#ifdef __APPLE__
/*
 * '_cupsAppleCopyDefaultPaperID()' - Get the default paper ID.
 */

CFStringRef				/* O - Default paper ID */
_cupsAppleCopyDefaultPaperID(void)
{
  return (CFPreferencesCopyAppValue(kDefaultPaperIDKey,
                                    kCUPSPrintingPrefs));
}


/*
 * '_cupsAppleCopyDefaultPrinter()' - Get the default printer at this location.
 */

CFStringRef				/* O - Default printer name */
_cupsAppleCopyDefaultPrinter(void)
{
  CFStringRef	network;		/* Network location */
  CFArrayRef	locations;		/* Location array */
  CFStringRef	locprinter;		/* Current printer */


 /*
  * Use location-based defaults only if "use last printer" is selected in the
  * system preferences...
  */

  if (!_cupsAppleGetUseLastPrinter())
  {
    DEBUG_puts("1_cupsAppleCopyDefaultPrinter: Not using last printer as "
	       "default.");
    return (NULL);
  }

 /*
  * Get the current location...
  */

  if ((network = appleCopyNetwork()) == NULL)
  {
    DEBUG_puts("1_cupsAppleCopyDefaultPrinter: Unable to get current "
               "network.");
    return (NULL);
  }

//#  ifdef DEBUG
//  CFStringGetCString(network, name, namesize, kCFStringEncodingUTF8);
//  DEBUG_printf(("2_cupsUserDefault: network=\"%s\"", name));
//#  endif /* DEBUG */

 /*
  * Lookup the network in the preferences...
  */

  if ((locations = appleCopyLocations()) == NULL)
  {
   /*
    * Missing or bad location array, so no location-based default...
    */

    DEBUG_puts("1_cupsAppleCopyDefaultPrinter: Missing or bad last used "
	       "printer array.");

    CFRelease(network);

    return (NULL);
  }

  DEBUG_printf(("1_cupsAppleCopyDefaultPrinter: Got locations, %d entries.",
                (int)CFArrayGetCount(locations)));

  if ((locprinter = appleGetPrinter(locations, network, NULL)) != NULL)
    CFRetain(locprinter);

  CFRelease(network);
  CFRelease(locations);

  return (locprinter);
}


/*
 * '_cupsAppleGetUseLastPrinter()' - Get whether to use the last used printer.
 */

int					/* O - 1 to use last printer, 0 otherwise */
_cupsAppleGetUseLastPrinter(void)
{
  Boolean	uselast,		/* Use last printer preference value */
		uselast_set;		/* Valid is set? */


  if (getenv("CUPS_DISABLE_APPLE_DEFAULT"))
    return (0);

  uselast = CFPreferencesGetAppBooleanValue(kUseLastPrinter,
                                            kCUPSPrintingPrefs,
					    &uselast_set);
  if (!uselast_set)
    return (1);
  else
    return (uselast);
}


/*
 * '_cupsAppleSetDefaultPaperID()' - Set the default paper id.
 */

void
_cupsAppleSetDefaultPaperID(
    CFStringRef name)			/* I - New paper ID */
{
  CFPreferencesSetAppValue(kDefaultPaperIDKey, name, kCUPSPrintingPrefs);
  CFPreferencesAppSynchronize(kCUPSPrintingPrefs);
  notify_post("com.apple.printerPrefsChange");
}


/*
 * '_cupsAppleSetDefaultPrinter()' - Set the default printer for this location.
 */

void
_cupsAppleSetDefaultPrinter(
    CFStringRef name)			/* I - Default printer/class name */
{
  CFStringRef		network;	/* Current network */
  CFArrayRef		locations;	/* Old locations array */
  CFIndex		locindex;	/* Index in locations array */
  CFStringRef		locprinter;	/* Current printer */
  CFMutableArrayRef	newlocations;	/* New locations array */
  CFMutableDictionaryRef newlocation;	/* New location */


 /*
  * Get the current location...
  */

  if ((network = appleCopyNetwork()) == NULL)
  {
    DEBUG_puts("1_cupsAppleSetDefaultPrinter: Unable to get current network...");
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

  if (!locprinter || CFStringCompare(locprinter, name, 0) != kCFCompareEqualTo)
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
      newlocations = CFArrayCreateMutable(kCFAllocatorDefault, 0,
					  &kCFTypeArrayCallBacks);

    newlocation = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);

    if (newlocation && newlocations)
    {
     /*
      * Put the new location at the front of the array...
      */

      CFDictionaryAddValue(newlocation, kLocationNetworkKey, network);
      CFDictionaryAddValue(newlocation, kLocationPrinterIDKey, name);
      CFArrayInsertValueAtIndex(newlocations, 0, newlocation);

     /*
      * Limit the number of locations to 10...
      */

      while (CFArrayGetCount(newlocations) > 10)
        CFArrayRemoveValueAtIndex(newlocations, 10);

     /*
      * Push the changes out...
      */

      CFPreferencesSetAppValue(kLastUsedPrintersKey, newlocations,
                               kCUPSPrintingPrefs);
      CFPreferencesAppSynchronize(kCUPSPrintingPrefs);
      notify_post("com.apple.printerPrefsChange");
    }

    if (newlocations)
      CFRelease(newlocations);

    if (newlocation)
      CFRelease(newlocation);
  }

  if (locations)
    CFRelease(locations);

  CFRelease(network);
}


/*
 * '_cupsAppleSetUseLastPrinter()' - Set whether to use the last used printer.
 */

void
_cupsAppleSetUseLastPrinter(
    int uselast)			/* O - 1 to use last printer, 0 otherwise */
{
  CFPreferencesSetAppValue(kUseLastPrinter,
			   uselast ? kCFBooleanTrue : kCFBooleanFalse,
			   kCUPSPrintingPrefs);
  CFPreferencesAppSynchronize(kCUPSPrintingPrefs);
  notify_post("com.apple.printerPrefsChange");
}
#endif /* __APPLE__ */


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
  int	diff,				/* Result of comparison */
	match;				/* Matching index */


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

    match = cups_find_dest(name, instance, num_dests, dests, -1, &diff);

    if (!diff)
      return (dests + match);
  }

  return (NULL);
}


/*
 * '_cupsGetDests()' - Get destinations from a server.
 *
 * "op" is CUPS_GET_PRINTERS to get a full list, CUPS_GET_DEFAULT to get the
 * system-wide default printer, or IPP_GET_PRINTER_ATTRIBUTES for a known
 * printer.
 *
 * "name" is the name of an existing printer and is only used when "op" is
 * IPP_GET_PRINTER_ATTRIBUTES.
 *
 * "dest" is initialized to point to the array of destinations.
 *
 * 0 is returned if there are no printers, no default printer, or the named
 * printer does not exist, respectively.
 *
 * Free the memory used by the destination array using the @link cupsFreeDests@
 * function.
 *
 * Note: On Mac OS X this function also gets the default paper from the system
 * preferences (~/L/P/org.cups.PrintingPrefs.plist) and includes it in the
 * options array for each destination that supports it.
 */

int					/* O - Number of destinations */
_cupsGetDests(http_t      *http,	/* I - Connection to server or CUPS_HTTP_DEFAULT */
	      ipp_op_t    op,		/* I - IPP operation */
	      const char  *name,	/* I - Name of destination */
	      cups_dest_t **dests)	/* IO - Destinations */
{
  int		num_dests = 0;		/* Number of destinations */
  cups_dest_t	*dest;			/* Current destination */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  const char	*printer_name;		/* printer-name attribute */
  char		uri[1024];		/* printer-uri value */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
#ifdef __APPLE__
  char		media_default[41];	/* Default paper size */
#endif /* __APPLE__ */
  char		optname[1024],		/* Option name */
		value[2048],		/* Option value */
		*ptr;			/* Pointer into name/value */
  static const char * const pattrs[] =	/* Attributes we're interested in */
		{
		  "auth-info-required",
		  "device-uri",
		  "job-sheets-default",
		  "marker-change-time",
		  "marker-colors",
		  "marker-high-levels",
		  "marker-levels",
		  "marker-low-levels",
		  "marker-message",
		  "marker-names",
		  "marker-types",
#ifdef __APPLE__
		  "media-supported",
#endif /* __APPLE__ */
		  "printer-commands",
		  "printer-defaults",
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
		  "printer-uri-supported"
		};


#ifdef __APPLE__
 /*
  * Get the default paper size...
  */

  appleGetPaperSize(media_default, sizeof(media_default));
#endif /* __APPLE__ */

 /*
  * Build a CUPS_GET_PRINTERS or IPP_GET_PRINTER_ATTRIBUTES request, which
  * require the following attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requesting-user-name
  *    printer-uri [for IPP_GET_PRINTER_ATTRIBUTES]
  */

  request = ippNewRequest(op);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(pattrs) / sizeof(pattrs[0]),
		NULL, pattrs);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, cupsUser());

  if (name && op != CUPS_GET_DEFAULT)
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

      printer_name = NULL;
      num_options  = 0;
      options      = NULL;

      for (; attr && attr->group_tag == IPP_TAG_PRINTER; attr = attr->next)
      {
	if (attr->value_tag != IPP_TAG_INTEGER &&
	    attr->value_tag != IPP_TAG_ENUM &&
	    attr->value_tag != IPP_TAG_BOOLEAN &&
	    attr->value_tag != IPP_TAG_TEXT &&
	    attr->value_tag != IPP_TAG_TEXTLANG &&
	    attr->value_tag != IPP_TAG_NAME &&
	    attr->value_tag != IPP_TAG_NAMELANG &&
	    attr->value_tag != IPP_TAG_KEYWORD &&
	    attr->value_tag != IPP_TAG_RANGE &&
	    attr->value_tag != IPP_TAG_URI)
          continue;

        if (!strcmp(attr->name, "auth-info-required") ||
	    !strcmp(attr->name, "device-uri") ||
	    !strcmp(attr->name, "marker-change-time") ||
	    !strcmp(attr->name, "marker-colors") ||
	    !strcmp(attr->name, "marker-high-levels") ||
	    !strcmp(attr->name, "marker-levels") ||
	    !strcmp(attr->name, "marker-low-levels") ||
	    !strcmp(attr->name, "marker-message") ||
	    !strcmp(attr->name, "marker-names") ||
	    !strcmp(attr->name, "marker-types") ||
	    !strcmp(attr->name, "printer-commands") ||
	    !strcmp(attr->name, "printer-info") ||
	    !strcmp(attr->name, "printer-is-shared") ||
	    !strcmp(attr->name, "printer-make-and-model") ||
	    !strcmp(attr->name, "printer-state") ||
	    !strcmp(attr->name, "printer-state-change-time") ||
	    !strcmp(attr->name, "printer-type") ||
            !strcmp(attr->name, "printer-is-accepting-jobs") ||
            !strcmp(attr->name, "printer-location") ||
            !strcmp(attr->name, "printer-state-reasons") ||
	    !strcmp(attr->name, "printer-uri-supported"))
        {
	 /*
	  * Add a printer description attribute...
	  */

          num_options = cupsAddOption(attr->name,
	                              cups_make_string(attr, value,
				                       sizeof(value)),
				      num_options, &options);
	}
#ifdef __APPLE__
	else if (!strcmp(attr->name, "media-supported"))
	{
	 /*
	  * See if we can set a default media size...
	  */

          int	i;			/* Looping var */

	  for (i = 0; i < attr->num_values; i ++)
	    if (!_cups_strcasecmp(media_default, attr->values[i].string.text))
	    {
	      num_options = cupsAddOption("media", media_default, num_options,
	                                  &options);
              break;
	    }
	}
#endif /* __APPLE__ */
        else if (!strcmp(attr->name, "printer-name") &&
	         attr->value_tag == IPP_TAG_NAME)
	  printer_name = attr->values[0].string.text;
        else if (strncmp(attr->name, "notify-", 7) &&
	         (attr->value_tag == IPP_TAG_BOOLEAN ||
		  attr->value_tag == IPP_TAG_ENUM ||
		  attr->value_tag == IPP_TAG_INTEGER ||
		  attr->value_tag == IPP_TAG_KEYWORD ||
		  attr->value_tag == IPP_TAG_NAME ||
		  attr->value_tag == IPP_TAG_RANGE) &&
		 (ptr = strstr(attr->name, "-default")) != NULL)
	{
	 /*
	  * Add a default option...
	  */

          strlcpy(optname, attr->name, sizeof(optname));
	  optname[ptr - attr->name] = '\0';

	  if (_cups_strcasecmp(optname, "media") ||
	      !cupsGetOption("media", num_options, options))
	    num_options = cupsAddOption(optname,
					cups_make_string(attr, value,
							 sizeof(value)),
					num_options, &options);
	}
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

      if ((dest = cups_add_dest(printer_name, NULL, &num_dests, dests)) != NULL)
      {
        dest->num_options = num_options;
	dest->options     = options;
      }
      else
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
 * 'cupsGetDests()' - Get the list of destinations from the default server.
 *
 * Starting with CUPS 1.2, the returned list of destinations include the
 * printer-info, printer-is-accepting-jobs, printer-is-shared,
 * printer-make-and-model, printer-state, printer-state-change-time,
 * printer-state-reasons, and printer-type attributes as options.  CUPS 1.4
 * adds the marker-change-time, marker-colors, marker-high-levels,
 * marker-levels, marker-low-levels, marker-message, marker-names,
 * marker-types, and printer-commands attributes as well.
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
 * printer-state-reasons, and printer-type attributes as options.  CUPS 1.4
 * adds the marker-change-time, marker-colors, marker-high-levels,
 * marker-levels, marker-low-levels, marker-message, marker-names,
 * marker-types, and printer-commands attributes as well.
 *
 * Use the @link cupsFreeDests@ function to free the destination list and
 * the @link cupsGetDest@ function to find a particular destination.
 *
 * @since CUPS 1.1.21/Mac OS X 10.4@
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
		*instance,		/* Pointer to instance name */
		*user_default;		/* User default printer */
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
  * Grab the printers and classes...
  */

  *dests    = (cups_dest_t *)0;
  num_dests = _cupsGetDests(http, CUPS_GET_PRINTERS, NULL, dests);

  if (cupsLastError() >= IPP_REDIRECTION_OTHER_SITE)
  {
    cupsFreeDests(num_dests, *dests);
    *dests = (cups_dest_t *)0;
    return (0);
  }

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

  if ((user_default = _cupsUserDefault(name, sizeof(name))) != NULL)
    defprinter = name;
  else if ((defprinter = cupsGetDefault2(http)) != NULL)
  {
    strlcpy(name, defprinter, sizeof(name));
    defprinter = name;
  }

  if (defprinter)
  {
   /*
    * Separate printer and instance name...
    */

    if ((instance = strchr(name, '/')) != NULL)
      *instance++ = '\0';

   /*
    * Lookup the printer and instance and make it the default...
    */

    if ((dest = cupsGetDest(name, instance, num_dests, *dests)) != NULL)
      dest->is_default = 1;
  }
  else
    instance = NULL;

 /*
  * Load the /etc/cups/lpoptions and ~/.cups/lpoptions files...
  */

  snprintf(filename, sizeof(filename), "%s/lpoptions", cg->cups_serverroot);
  num_dests = cups_get_dests(filename, NULL, NULL, user_default != NULL,
                             num_dests, dests);

  if ((home = getenv("HOME")) != NULL)
  {
    snprintf(filename, sizeof(filename), "%s/.cups/lpoptions", home);

    num_dests = cups_get_dests(filename, NULL, NULL, user_default != NULL,
                               num_dests, dests);
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
 * @since CUPS 1.4/Mac OS X 10.6@
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
  int		set_as_default = 0;	/* Set returned destination as default */
  ipp_op_t	op = IPP_GET_PRINTER_ATTRIBUTES;
					/* IPP operation to get server ops */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * If "name" is NULL, find the default destination...
  */

  if (!name)
  {
    set_as_default = 1;
    name           = _cupsUserDefault(defname, sizeof(defname));

    if (name)
    {
      char	*ptr;			/* Temporary pointer... */

      if ((ptr = strchr(defname, '/')) != NULL)
      {
        *ptr++   = '\0';
	instance = ptr;
      }
      else
        instance = NULL;
    }
    else if (home)
    {
     /*
      * No default in the environment, try the user's lpoptions files...
      */

      snprintf(filename, sizeof(filename), "%s/.cups/lpoptions", home);

      name = cups_get_default(filename, defname, sizeof(defname), &instance);
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

  if (!_cupsGetDests(http, op, name, &dest))
  {
    if (op == CUPS_GET_DEFAULT || (name && !set_as_default))
      return (NULL);

   /*
    * The default printer from environment variables or from a
    * configuration file does not exist.  Find out the real default.
    */

    if (!_cupsGetDests(http, CUPS_GET_DEFAULT, NULL, &dest))
      return (NULL);
  }

  if (instance)
    dest->instance = _cupsStrAlloc(instance);

  if (set_as_default)
    dest->is_default = 1;

 /*
  * Then add local options...
  */

  snprintf(filename, sizeof(filename), "%s/lpoptions", cg->cups_serverroot);
  cups_get_dests(filename, name, instance, 1, 1, &dest);

  if (home)
  {
    snprintf(filename, sizeof(filename), "%s/.cups/lpoptions", home);

    cups_get_dests(filename, name, instance, 1, 1, &dest);
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
 * @since CUPS 1.3/Mac OS X 10.5@
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
 * @since CUPS 1.3/Mac OS X 10.5@
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
    dest->is_default = !_cups_strcasecmp(name, dest->name) &&
                       ((!instance && !dest->instance) ||
		        (instance && dest->instance &&
			 !_cups_strcasecmp(instance, dest->instance)));
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
 * @since CUPS 1.1.21/Mac OS X 10.4@
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

  num_temps = _cupsGetDests(http, CUPS_GET_PRINTERS, NULL, &temps);

  if (cupsLastError() >= IPP_REDIRECTION_OTHER_SITE)
  {
    cupsFreeDests(num_temps, temps);
    return (-1);
  }

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

    num_temps = cups_get_dests(filename, NULL, NULL, 0, num_temps, &temps);

   /*
    * Point to user defaults...
    */

    if ((home = getenv("HOME")) != NULL)
    {
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
            !_cups_strcasecmp(val, option->value))
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
  {
    CFStringRef name = CFStringCreateWithCString(kCFAllocatorDefault,
                                                 dest->name,
                                                 kCFStringEncodingUTF8);
					/* Default printer name */

    if (name)
    {
      _cupsAppleSetDefaultPrinter(name);
      CFRelease(name);
    }
  }
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


/*
 * '_cupsUserDefault()' - Get the user default printer from environment
 *                        variables and location information.
 */

char *					/* O - Default printer or NULL */
_cupsUserDefault(char   *name,		/* I - Name buffer */
                 size_t namesize)	/* I - Size of name buffer */
{
  const char	*env;			/* LPDEST or PRINTER env variable */
#ifdef __APPLE__
  CFStringRef	locprinter;		/* Last printer as this location */
#endif /* __APPLE__ */


  if ((env = getenv("LPDEST")) == NULL)
    if ((env = getenv("PRINTER")) != NULL && !strcmp(env, "lp"))
      env = NULL;

  if (env)
  {
    strlcpy(name, env, namesize);
    return (name);
  }

#ifdef __APPLE__
 /*
  * Use location-based defaults if "use last printer" is selected in the
  * system preferences...
  */

  if ((locprinter = _cupsAppleCopyDefaultPrinter()) != NULL)
  {
    CFStringGetCString(locprinter, name, namesize, kCFStringEncodingUTF8);
    CFRelease(locprinter);
  }
  else
    name[0] = '\0';

  DEBUG_printf(("1_cupsUserDefault: Returning \"%s\".", name));

  return (*name ? name : NULL);

#else
 /*
  * No location-based defaults on this platform...
  */

  name[0] = '\0';
  return (NULL);
#endif /* __APPLE__ */
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

  if ((locations = CFPreferencesCopyAppValue(kLastUsedPrintersKey,
                                             kCUPSPrintingPrefs)) == NULL)
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


  if ((dynamicStore = SCDynamicStoreCreate(NULL, CFSTR("libcups"), NULL,
                                           NULL)) != NULL)
  {
   /*
    * First use the IPv6 router address, if available, since that will generally
    * be a globally-unique link-local address.
    */

    if ((key = SCDynamicStoreKeyCreateNetworkGlobalEntity(
                   NULL, kSCDynamicStoreDomainState, kSCEntNetIPv6)) != NULL)
    {
      if ((ip_dict = SCDynamicStoreCopyValue(dynamicStore, key)) != NULL)
      {
	if ((network = CFDictionaryGetValue(ip_dict,
	                                    kSCPropNetIPv6Router)) != NULL)
          CFRetain(network);

        CFRelease(ip_dict);
      }

      CFRelease(key);
    }

   /*
    * If that doesn't work, try the IPv4 router address. This isn't as unique
    * and will likely be a 10.x.y.z or 192.168.y.z address...
    */

    if (!network)
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
    }

    CFRelease(dynamicStore);
  }

  return (network);
}


/*
 * 'appleGetPaperSize()' - Get the default paper size.
 */

char *					/* O - Default paper size */
appleGetPaperSize(char *name,		/* I - Paper size name buffer */
                  int  namesize)	/* I - Size of buffer */
{
  CFStringRef	defaultPaperID;		/* Default paper ID */
  _pwg_media_t	*pwgmedia;		/* PWG media size */


  defaultPaperID = _cupsAppleCopyDefaultPaperID();
  if (!defaultPaperID ||
      CFGetTypeID(defaultPaperID) != CFStringGetTypeID() ||
      !CFStringGetCString(defaultPaperID, name, namesize,
			  kCFStringEncodingUTF8))
    name[0] = '\0';
  else if ((pwgmedia = _pwgMediaForLegacy(name)) != NULL)
    strlcpy(name, pwgmedia->pwg, namesize);

  if (defaultPaperID)
    CFRelease(defaultPaperID);

  return (name);
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
#endif /* __APPLE__ */


/*
 * 'cups_add_dest()' - Add a destination to the array.
 *
 * Unlike cupsAddDest(), this function does not check for duplicates.
 */

static cups_dest_t *			/* O  - New destination */
cups_add_dest(const char  *name,	/* I  - Name of destination */
              const char  *instance,	/* I  - Instance or NULL */
              int         *num_dests,	/* IO - Number of destinations */
	      cups_dest_t **dests)	/* IO - Destinations */
{
  int		insert,			/* Insertion point */
		diff;			/* Result of comparison */
  cups_dest_t	*dest;			/* Destination pointer */


 /*
  * Add new destination...
  */

  if (*num_dests == 0)
    dest = malloc(sizeof(cups_dest_t));
  else
    dest = realloc(*dests, sizeof(cups_dest_t) * (*num_dests + 1));

  if (!dest)
    return (NULL);

  *dests = dest;

 /*
  * Find where to insert the destination...
  */

  if (*num_dests == 0)
    insert = 0;
  else
  {
    insert = cups_find_dest(name, instance, *num_dests, *dests, *num_dests - 1,
                            &diff);

    if (diff > 0)
      insert ++;
  }

 /*
  * Move the array elements as needed...
  */

  if (insert < *num_dests)
    memmove(*dests + insert + 1, *dests + insert,
            (*num_dests - insert) * sizeof(cups_dest_t));

  (*num_dests) ++;

 /*
  * Initialize the destination...
  */

  dest              = *dests + insert;
  dest->name        = _cupsStrAlloc(name);
  dest->instance    = _cupsStrAlloc(instance);
  dest->is_default  = 0;
  dest->num_options = 0;
  dest->options     = (cups_option_t *)0;

  return (dest);
}


/*
 * 'cups_compare_dests()' - Compare two destinations.
 */

static int				/* O - Result of comparison */
cups_compare_dests(cups_dest_t *a,	/* I - First destination */
                   cups_dest_t *b)	/* I - Second destination */
{
  int	diff;				/* Difference */


  if ((diff = _cups_strcasecmp(a->name, b->name)) != 0)
    return (diff);
  else if (a->instance && b->instance)
    return (_cups_strcasecmp(a->instance, b->instance));
  else
    return ((a->instance && !b->instance) - (!a->instance && b->instance));
}


/*
 * 'cups_find_dest()' - Find a destination using a binary search.
 */

static int				/* O - Index of match */
cups_find_dest(const char  *name,	/* I - Destination name */
               const char  *instance,	/* I - Instance or NULL */
               int         num_dests,	/* I - Number of destinations */
	       cups_dest_t *dests,	/* I - Destinations */
	       int         prev,	/* I - Previous index */
	       int         *rdiff)	/* O - Difference of match */
{
  int		left,			/* Low mark for binary search */
		right,			/* High mark for binary search */
		current,		/* Current index */
		diff;			/* Result of comparison */
  cups_dest_t	key;			/* Search key */


  key.name     = (char *)name;
  key.instance = (char *)instance;

  if (prev >= 0)
  {
   /*
    * Start search on either side of previous...
    */

    if ((diff = cups_compare_dests(&key, dests + prev)) == 0 ||
        (diff < 0 && prev == 0) ||
	(diff > 0 && prev == (num_dests - 1)))
    {
      *rdiff = diff;
      return (prev);
    }
    else if (diff < 0)
    {
     /*
      * Start with previous on right side...
      */

      left  = 0;
      right = prev;
    }
    else
    {
     /*
      * Start wih previous on left side...
      */

      left  = prev;
      right = num_dests - 1;
    }
  }
  else
  {
   /*
    * Start search in the middle...
    */

    left  = 0;
    right = num_dests - 1;
  }

  do
  {
    current = (left + right) / 2;
    diff    = cups_compare_dests(&key, dests + current);

    if (diff == 0)
      break;
    else if (diff < 0)
      right = current;
    else
      left = current;
  }
  while ((right - left) > 1);

  if (diff != 0)
  {
   /*
    * Check the last 1 or 2 elements...
    */

    if ((diff = cups_compare_dests(&key, dests + left)) <= 0)
      current = left;
    else
    {
      diff    = cups_compare_dests(&key, dests + right);
      current = right;
    }
  }

 /*
  * Return the closest destination and the difference...
  */

  *rdiff = diff;

  return (current);
}


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
      if (!_cups_strcasecmp(line, "default") && value)
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
cups_get_dests(
    const char  *filename,		/* I - File to read from */
    const char  *match_name,		/* I - Destination name we want */
    const char  *match_inst,		/* I - Instance name we want */
    int         user_default_set,	/* I - User default printer set? */
    int         num_dests,		/* I - Number of destinations */
    cups_dest_t **dests)		/* IO - Destinations */
{
  int		i;			/* Looping var */
  cups_dest_t	*dest;			/* Current destination */
  cups_file_t	*fp;			/* File pointer */
  char		line[8192],		/* Line from file */
		*lineptr,		/* Pointer into line */
		*name,			/* Name of destination/option */
		*instance;		/* Instance of destination */
  int		linenum;		/* Current line number */


  DEBUG_printf(("7cups_get_dests(filename=\"%s\", match_name=\"%s\", "
                "match_inst=\"%s\", user_default_set=%d, num_dests=%d, "
		"dests=%p)", filename, match_name, match_inst,
		user_default_set, num_dests, dests));

 /*
  * Try to open the file...
  */

  if ((fp = cupsFileOpen(filename, "r")) == NULL)
    return (num_dests);

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

    DEBUG_printf(("9cups_get_dests: linenum=%d line=\"%s\" lineptr=\"%s\"",
                  linenum, line, lineptr));

    if ((_cups_strcasecmp(line, "dest") && _cups_strcasecmp(line, "default")) || !lineptr)
    {
      DEBUG_puts("9cups_get_dests: Not a dest or default line...");
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

    DEBUG_printf(("9cups_get_dests: name=\"%s\", instance=\"%s\"", name,
                  instance));

   /*
    * See if the primary instance of the destination exists; if not,
    * ignore this entry and move on...
    */

    if (match_name)
    {
      if (_cups_strcasecmp(name, match_name) ||
          (!instance && match_inst) ||
	  (instance && !match_inst) ||
	  (instance && _cups_strcasecmp(instance, match_inst)))
	continue;

      dest = *dests;
    }
    else if (cupsGetDest(name, NULL, num_dests, *dests) == NULL)
    {
      DEBUG_puts("9cups_get_dests: Not found!");
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

        DEBUG_puts("9cups_get_dests: Out of memory!");
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

    if (!user_default_set && !_cups_strcasecmp(line, "default"))
    {
      DEBUG_puts("9cups_get_dests: Setting as default...");

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
 * 'cups_make_string()' - Make a comma-separated string of values from an IPP
 *                        attribute.
 */

static char *				/* O - New string */
cups_make_string(
    ipp_attribute_t *attr,		/* I - Attribute to convert */
    char            *buffer,		/* I - Buffer */
    size_t          bufsize)		/* I - Size of buffer */
{
  int		i;			/* Looping var */
  char		*ptr,			/* Pointer into buffer */
		*end,			/* Pointer to end of buffer */
		*valptr;		/* Pointer into string attribute */


 /*
  * Return quickly if we have a single string value...
  */

  if (attr->num_values == 1 &&
      attr->value_tag != IPP_TAG_INTEGER &&
      attr->value_tag != IPP_TAG_ENUM &&
      attr->value_tag != IPP_TAG_BOOLEAN &&
      attr->value_tag != IPP_TAG_RANGE)
    return (attr->values[0].string.text);

 /*
  * Copy the values to the string, separating with commas and escaping strings
  * as needed...
  */

  end = buffer + bufsize - 1;

  for (i = 0, ptr = buffer; i < attr->num_values && ptr < end; i ++)
  {
    if (i)
      *ptr++ = ',';

    switch (attr->value_tag)
    {
      case IPP_TAG_INTEGER :
      case IPP_TAG_ENUM :
	  snprintf(ptr, end - ptr + 1, "%d", attr->values[i].integer);
	  break;

      case IPP_TAG_BOOLEAN :
	  if (attr->values[i].boolean)
	    strlcpy(ptr, "true", end - ptr + 1);
	  else
	    strlcpy(ptr, "false", end - ptr + 1);
	  break;

      case IPP_TAG_RANGE :
	  if (attr->values[i].range.lower == attr->values[i].range.upper)
	    snprintf(ptr, end - ptr + 1, "%d", attr->values[i].range.lower);
	  else
	    snprintf(ptr, end - ptr + 1, "%d-%d", attr->values[i].range.lower,
		     attr->values[i].range.upper);
	  break;

      default :
	  for (valptr = attr->values[i].string.text;
	       *valptr && ptr < end;)
	  {
	    if (strchr(" \t\n\\\'\"", *valptr))
	    {
	      if (ptr >= (end - 1))
	        break;

	      *ptr++ = '\\';
	    }

	    *ptr++ = *valptr++;
	  }

	  *ptr = '\0';
	  break;
    }

    ptr += strlen(ptr);
  }

  *ptr = '\0';

  return (buffer);
}


/*
 * End of "$Id: dest.c 9568 2011-02-25 06:13:56Z mike $".
 */
