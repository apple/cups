/*
 * "$Id: langprintf.c,v 1.1.2.1 2002/06/06 02:01:38 mike Exp $"
 *
 *   Localized printf/puts functions for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 2002 by Easy Software Products.
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsLangPrintf() - Print a formatted message string to a file.
 *   cupsLangPuts()   - Print a static message string to a file.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include "string.h"
#include "language.h"


/*
 * 'cupsLangPrintf()' - Print a formatted message string to a file.
 */

int					/* O - Number of bytes written */
cupsLangPrintf(FILE        *fp,		/* I - File to write to */
               cups_lang_t *language,	/* I - Language to use */
	       cups_msg_t  msg,		/* I - Message string to use */
	       ...)			/* I - Additional arguments as needed */
{
  int		bytes;			/* Number of bytes formatted */
  char		buffer[2048];		/* Message buffer */
  va_list 	ap;			/* Pointer to additional arguments */


 /*
  * Range check...
  */

  if (fp == NULL || language == NULL || msg < CUPS_MSG_OK ||
      msg >= CUPS_MSG_MAX || cupsLangString(language, msg) == NULL)
    return (-1);

 /*
  * Format the string...
  */

  va_start(ap, msg);
  bytes = vsnprintf(buffer, sizeof(buffer), cupsLangString(language, msg), ap);
  va_end(ap);

 /*
  * IRA: Insert transcoding to destination charset here...
  */

 /*
  * Write the string and return the number of bytes written...
  */

  return (fwrite(buffer, 1, bytes, fp));
}


/*
 * 'cupsLangPuts()' - Print a static message string to a file.
 */

int					/* O - Number of bytes written */
cupsLangPuts(FILE        *fp,		/* I - File to write to */
             cups_lang_t *language,	/* I - Language to use */
	     cups_msg_t  msg)		/* I - Message string to use */
{
  int		bytes;			/* Number of bytes formatted */
  char		buffer[2048];		/* Message buffer */


 /*
  * Range check...
  */

  if (fp == NULL || language == NULL || msg < CUPS_MSG_OK ||
      msg >= CUPS_MSG_MAX || cupsLangString(language, msg) == NULL)
    return (-1);

 /*
  * Get length of message string...
  */

  strlcpy(buffer, cupsLangString(language, msg), sizeof(buffer));

  bytes = strlen(buffer);

 /*
  * IRA: Insert transcoding to destination charset here...
  */

 /*
  * Write the string and return the number of bytes written...
  */

  return (fwrite(buffer, 1, bytes, fp));
}


/*
 * End of "$Id: langprintf.c,v 1.1.2.1 2002/06/06 02:01:38 mike Exp $".
 */

