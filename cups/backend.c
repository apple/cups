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
 */

/*
 * Include necessary headers...
 */

#include <stdlib.h>
#include "backend.h"
#include "string.h"


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


  if ((device_uri = getenv("DEVICE_URI")) != NULL)
    return (device_uri);

  if (!argv || !argv[0] || !strchr(argv[0], ':'))
    return (NULL);
  else
    return (argv[0]);
}


/*
 * End of "$Id$".
 */
