/*
 * "$Id$"
 *
 *   Localized printf/puts functions for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 2002-2006 by Easy Software Products.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   _cupsLangPrintf() - Print a formatted message string to a file.
 *   _cupsLangPuts()   - Print a static message string to a file.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include "globals.h"


/*
 * '_cupsLangPrintf()' - Print a formatted message string to a file.
 */

int					/* O - Number of bytes written */
_cupsLangPrintf(FILE        *fp,	/* I - File to write to */
	        const char  *message,	/* I - Message string to use */
	        ...)			/* I - Additional arguments as needed */
{
  int		bytes;			/* Number of bytes formatted */
  char		buffer[2048],		/* Message buffer */
		output[8192];		/* Output buffer */
  va_list 	ap;			/* Pointer to additional arguments */
  _cups_globals_t *cg;			/* Global data */


 /*
  * Range check...
  */

  if (!fp || !message)
    return (-1);

  cg = _cupsGlobals();

  if (!cg->lang_default)
    cg->lang_default = cupsLangDefault();

 /*
  * Format the string...
  */

  va_start(ap, message);
  bytes = vsnprintf(buffer, sizeof(buffer),
                    _cupsLangString(cg->lang_default, message), ap);
  va_end(ap);

 /*
  * Transcode to the destination charset...
  */

  bytes = cupsUTF8ToCharset(output, (cups_utf8_t *)buffer, sizeof(output),
                            cg->lang_default->encoding);

 /*
  * Write the string and return the number of bytes written...
  */

  if (bytes > 0)
    return (fwrite(output, 1, bytes, fp));
  else
    return (bytes);
}


/*
 * '_cupsLangPuts()' - Print a static message string to a file.
 */

int					/* O - Number of bytes written */
_cupsLangPuts(FILE        *fp,		/* I - File to write to */
	      const char  *message)	/* I - Message string to use */
{
  int		bytes;			/* Number of bytes formatted */
  char		output[2048];		/* Message buffer */
  _cups_globals_t *cg;			/* Global data */


 /*
  * Range check...
  */

  if (!fp || !message)
    return (-1);

  cg = _cupsGlobals();

  if (!cg->lang_default)
    cg->lang_default = cupsLangDefault();

 /*
  * Transcode to the destination charset...
  */

  bytes = cupsUTF8ToCharset(output,
                            (cups_utf8_t *)_cupsLangString(cg->lang_default,
			                                   message),
			    sizeof(output), cg->lang_default->encoding);

 /*
  * Write the string and return the number of bytes written...
  */

  if (bytes > 0)
    return (fwrite(output, 1, bytes, fp));
  else
    return (bytes);
}


/*
 * End of "$Id$".
 */
