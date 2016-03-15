/*
 * "$Id: backend.c 10996 2013-05-29 11:51:34Z msweet $"
 *
 *   Backend functions for CUPS.
 *
 *   Copyright 2007-2012 by Apple Inc.
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

#include "cups-private.h"
#include "backend.h"


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
 *
 * @since CUPS 1.2/OS X 10.5@
 */

const char *				/* O - Device URI or @code NULL@ */
cupsBackendDeviceURI(char **argv)	/* I - Command-line arguments */
{
  const char	*device_uri,		/* Device URI */
		*auth_info_required;	/* AUTH_INFO_REQUIRED env var */
  _cups_globals_t *cg = _cupsGlobals();	/* Global info */
  int		options;		/* Resolve options */
  ppd_file_t	*ppd;			/* PPD file */
  ppd_attr_t	*ppdattr;		/* PPD attribute */


  if ((device_uri = getenv("DEVICE_URI")) == NULL)
  {
    if (!argv || !argv[0] || !strchr(argv[0], ':'))
      return (NULL);

    device_uri = argv[0];
  }

  options = _HTTP_RESOLVE_STDERR;
  if ((auth_info_required = getenv("AUTH_INFO_REQUIRED")) != NULL &&
      !strcmp(auth_info_required, "negotiate"))
    options |= _HTTP_RESOLVE_FQDN;

  if ((ppd = ppdOpenFile(getenv("PPD"))) != NULL)
  {
    if ((ppdattr = ppdFindAttr(ppd, "cupsIPPFaxOut", NULL)) != NULL &&
        !_cups_strcasecmp(ppdattr->value, "true"))
      options |= _HTTP_RESOLVE_FAXOUT;

    ppdClose(ppd);
  }

  return (_httpResolveURI(device_uri, cg->resolved_uri,
                          sizeof(cg->resolved_uri), options, NULL, NULL));
}


/*
 * 'cupsBackendReport()' - Write a device line from a backend.
 *
 * This function writes a single device line to stdout for a backend.
 * It handles quoting of special characters in the device-make-and-model,
 * device-info, device-id, and device-location strings.
 *
 * @since CUPS 1.4/OS X 10.6@
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

      if (((*s & 255) < ' ' && *s != '\t') || *s == 0x7f)
        putchar(' ');
      else
        putchar(*s);

      s ++;
    }
  }

  putchar('\"');
}


/*
 * End of "$Id: backend.c 10996 2013-05-29 11:51:34Z msweet $".
 */
