/*
 * "$Id$"
 *
 *   Destination localization support for CUPS.
 *
 *   Copyright 2012 by Apple Inc.
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
﻿ *   cupsLocalizeDestOption() - Get the localized string for a destination
 *				option.
 *   cupsLocalizeDestValue()  - Get the localized string for a destination
 *				option+value pair.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"


/*
 * 'cupsLocalizeDestOption()' - Get the localized string for a destination
 *                              option.
 *
 * The returned string is stored in the localization array and will become
 * invalid if the localization array is deleted.
 *
 * @since CUPS 1.6/OS X 10.8@
 */

const char *				/* O - Localized string */
cupsLocalizeDestOption(
    http_t       *http,			/* I - Connection to destination */
    cups_dest_t  *dest,			/* I - Destination */
    cups_dinfo_t *dinfo,		/* I - Destination information */
    const char   *option)		/* I - Option to localize */
{
  return (option);
}


/*
 * 'cupsLocalizeDestValue()' - Get the localized string for a destination
 *                             option+value pair.
 *
 * The returned string is stored in the localization array and will become
 * invalid if the localization array is deleted.
 *
 * @since CUPS 1.6/OS X 10.8@
 */

const char *				/* O - Localized string */
cupsLocalizeDestValue(
    http_t       *http,			/* I - Connection to destination */
    cups_dest_t  *dest,			/* I - Destination */
    cups_dinfo_t *dinfo,		/* I - Destination information */
    const char   *option,		/* I - Option to localize */
    const char   *value)		/* I - Value to localize */
{
  return (value);
}


/*
 * End of "$Id$".
 */
