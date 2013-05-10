/*
 * "$Id$"
 *
 *   Backend functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 2006 by Easy Software Products.
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
 *   cupsBackendDeviceURI() - Get the device URI for a backend.
 *   cupsBackendReport()    - Write a device line from a backend.
 *   quote_string()         - Write a quoted string to stdout, escaping \ and ".
 */

/*
 * Include necessary headers...
 */

#include <stdlib.h>
#include "backend.h"
#include "globals.h"


/*
 * Local functions...
 */

static void	quote_string(const char *s);


/*
 * 'cupsBackendDeviceURI()' - Get the device URI for a backend.
 *
 * The "argv" argument is the argv argument passed to main(). This
 * function returns the device URI passed in the DEVICE_URI environment
 * variable or the device URI passed in argv[0], whichever is found
 * first.
 */

const char *				/* O - Device URI or @code NULL@ */
cupsBackendDeviceURI(char **argv)	/* I - Command-line arguments */
{
  const char	*device_uri;		/* Device URI */
  _cups_globals_t *cg = _cupsGlobals();	/* Global info */


  if ((device_uri = getenv("DEVICE_URI")) == NULL)
  {
    if (!argv || !argv[0] || !strchr(argv[0], ':'))
      return (NULL);

    device_uri = argv[0];
  }

  return (_httpResolveURI(device_uri, cg->resolved_uri,
                          sizeof(cg->resolved_uri), 1));
}


/*
 * 'cupsBackendReport()' - Write a device line from a backend.
 *
 * This function writes a single device line to stdout for a backend.
 * It handles quoting of special characters in the device-make-and-model,
 * device-info, device-id, and device-location strings.
 */

void
cupsBackendReport(
    const char *device_scheme,		/* I - device-scheme string */
    const char *device_uri,		/* I - device-uri string */
    const char *device_make_and_model,	/* I - device-make-and-model string or @code NULL@ */
    const char *device_info,		/* I - device-info string or @code NULL@ */
    const char *device_id,		/* I - device-id string or @code NULL@ */
    const char *device_location)	/* I - device-location string or @code NULL@ */
{
  if (!device_scheme || !device_uri)
    return;

  printf("%s %s", device_scheme, device_uri);
  if (device_make_and_model && *device_make_and_model)
    quote_string(device_make_and_model);
  else
    quote_string("unknown");
  quote_string(device_info);
  quote_string(device_id);
  quote_string(device_location);
  putchar('\n');
  fflush(stdout);
}


/*
 * 'quote_string()' - Write a quoted string to stdout, escaping \ and ".
 */

static void
quote_string(const char *s)		/* I - String to write */
{
  fputs(" \"", stdout);

  if (s)
  {
    while (*s)
    {
      if (*s == '\\' || *s == '\"')
	putchar('\\');

      putchar(*s);

      s ++;
    }
  }

  putchar('\"');
}


/*
 * End of "$Id$".
 */
