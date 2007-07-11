/*
 * "$Id$"
 *
 *   Localized printf/puts functions for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 2002-2007 by Easy Software Products.
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
 *   _cupsLangPrintf() - Print a formatted message string to a file.
 *   _cupsLangPuts()   - Print a static message string to a file.
 *   _cupsSetLocale()  - Set the current locale and transcode the command-line.
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
    return ((int)fwrite(output, 1, bytes, fp));
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
    return ((int)fwrite(output, 1, bytes, fp));
  else
    return (bytes);
}


/*
 * '_cupsSetLocale()' - Set the current locale and transcode the command-line.
 */

void
_cupsSetLocale(char *argv[])		/* IO - Command-line arguments */
{
  int		i;			/* Looping var */
  char		buffer[8192];		/* Command-line argument buffer */
  _cups_globals_t *cg;			/* Global data */
#ifdef LC_TIME
  const char	*lc_time;		/* Current LC_TIME value */
  char		new_lc_time[255],	/* New LC_TIME value */
		*charset;		/* Pointer to character set */
#endif /* LC_TIME */


 /*
  * Set the locale so that times, etc. are displayed properly.
  *
  * Unfortunately, while we need the localized time value, we *don't*
  * want to use the localized charset for the time value, so we need
  * to set LC_TIME to the locale name with .UTF-8 on the end (if
  * the locale includes a character set specifier...)
  */

  setlocale(LC_ALL, "");

#ifdef LC_TIME
  if ((lc_time = setlocale(LC_TIME, NULL)) == NULL)
    lc_time = setlocale(LC_ALL, NULL);

  if (lc_time)
  {
    strlcpy(new_lc_time, lc_time, sizeof(new_lc_time));
    if ((charset = strchr(new_lc_time, '.')) == NULL)
      charset = new_lc_time + strlen(new_lc_time);

    strlcpy(charset, ".UTF-8", sizeof(new_lc_time) - (charset - new_lc_time));
  }
  else
    strcpy(new_lc_time, "C");

  setlocale(LC_TIME, new_lc_time);
#endif /* LC_TIME */

 /*
  * Initialize the default language info...
  */

  cg = _cupsGlobals();

  if (!cg->lang_default)
    cg->lang_default = cupsLangDefault();

 /*
  * Transcode the command-line arguments from the locale charset to
  * UTF-8...
  */

  if (cg->lang_default->encoding != CUPS_US_ASCII &&
      cg->lang_default->encoding != CUPS_UTF8)
  {
    for (i = 1; argv[i]; i ++)
    {
     /*
      * Try converting from the locale charset to UTF-8...
      */

      if (cupsCharsetToUTF8((cups_utf8_t *)buffer, argv[i], sizeof(buffer),
                            cg->lang_default->encoding) < 0)
        continue;

     /*
      * Save the new string if it differs from the original...
      */

      if (strcmp(buffer, argv[i]))
        argv[i] = strdup(buffer);
    }
  }
}


/*
 * End of "$Id$".
 */
