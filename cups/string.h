/*
 * "$Id$"
 *
 *   String definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _CUPS_STRING_H_
#  define _CUPS_STRING_H_

/*
 * Include necessary headers...
 */

#  include <stdio.h>
#  include <stdlib.h>
#  include <stdarg.h>
#  include <ctype.h>
#  include <locale.h>

#  include <config.h>

#  ifdef HAVE_STRING_H
#    include <string.h>
#  endif /* HAVE_STRING_H */

#  ifdef HAVE_STRINGS_H
#    include <strings.h>
#  endif /* HAVE_STRINGS_H */

#  ifdef HAVE_BSTRING_H
#    include <bstring.h>
#  endif /* HAVE_BSTRING_H */


/*
 * Stuff for WIN32 and OS/2...
 */

#  if defined(WIN32) || defined(__EMX__)
#    define strcasecmp	stricmp
#    define strncasecmp	strnicmp
#  endif /* WIN32 || __EMX__ */


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * String pool structures...
 */

typedef struct _cups_sp_item_s		/**** String Pool Item ****/
{
  char		*str;			/* String */
  unsigned int	ref_count;		/* Reference count */
} _cups_sp_item_t;


/*
 * Prototypes...
 */

extern void	_cups_strcpy(char *dst, const char *src);

#  ifndef HAVE_STRDUP
extern char	*_cups_strdup(const char *);
#    define strdup _cups_strdup
#  endif /* !HAVE_STRDUP */

#  ifndef HAVE_STRCASECMP
extern int	_cups_strcasecmp(const char *, const char *);
#    define strcasecmp _cups_strcasecmp
#  endif /* !HAVE_STRCASECMP */

#  ifndef HAVE_STRNCASECMP
extern int	_cups_strncasecmp(const char *, const char *, size_t n);
#    define strncasecmp _cups_strncasecmp
#  endif /* !HAVE_STRNCASECMP */

#  ifndef HAVE_STRLCAT
extern size_t _cups_strlcat(char *, const char *, size_t);
#    define strlcat _cups_strlcat
#  endif /* !HAVE_STRLCAT */

#  ifndef HAVE_STRLCPY
extern size_t _cups_strlcpy(char *, const char *, size_t);
#    define strlcpy _cups_strlcpy
#  endif /* !HAVE_STRLCPY */

#  ifndef HAVE_SNPRINTF
extern int	_cups_snprintf(char *, size_t, const char *, ...)
#    ifdef __GNUC__
__attribute__ ((__format__ (__printf__, 3, 4)))
#    endif /* __GNUC__ */
;
#    define snprintf _cups_snprintf
#  endif /* !HAVE_SNPRINTF */

#  ifndef HAVE_VSNPRINTF
extern int	_cups_vsnprintf(char *, size_t, const char *, va_list);
#    define vsnprintf _cups_vsnprintf
#  endif /* !HAVE_VSNPRINTF */

/*
 * String pool functions...
 */

extern char	*_cupsStrAlloc(const char *s);
extern void	_cupsStrFlush(void);
extern void	_cupsStrFree(const char *s);
extern char	*_cupsStrRetain(const char *s);
extern size_t	_cupsStrStatistics(size_t *alloc_bytes, size_t *total_bytes);


/*
 * Floating point number functions...
 */

extern char	*_cupsStrFormatd(char *buf, char *bufend, double number,
		                 struct lconv *loc);
extern double	_cupsStrScand(const char *buf, char **bufptr,
		              struct lconv *loc);


/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_STRING_H_ */

/*
 * End of "$Id$".
 */
