/*
 * "$Id: string.h,v 1.2 1999/06/18 18:36:11 mike Exp $"
 *
 *   String definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products.
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
 */

#ifndef _CUPS_STRING_H_
#  define _CUPS_STRING_H_

/*
 * Include necessary headers...
 */

#  include <config.h>
#  include <string.h>


/*
 * Stuff for WIN32 and OS/2...
 */

#  if defined(WIN32) || defined(__EMX__)
#    define strcasecmp	stricmp
#    define strncasecmp	strnicmp
#  endif /* WIN32 || __EMX__ */


/*
 * Prototypes...
 */

#  ifndef HAVE_STRDUP
extern char	*strdup(const char *);
#  endif /* !HAVE_STRDUP */

#  ifndef HAVE_STRCASECMP
extern int	strcasecmp(const char *, const char *);
#  endif /* !HAVE_STRCASECMP */

#  ifndef HAVE_STRNCASECMP
extern int	strncasecmp(const char *, const char *, size_t n);
#  endif /* !HAVE_STRNCASECMP */

#endif /* !_CUPS_STRING_H_ */

/*
 * End of "$Id: string.h,v 1.2 1999/06/18 18:36:11 mike Exp $".
 */
