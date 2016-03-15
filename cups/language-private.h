/*
 * "$Id: language-private.h 10996 2013-05-29 11:51:34Z msweet $"
 *
 *   Private localization support for CUPS.
 *
 *   Copyright 2007-2010 by Apple Inc.
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

#ifndef _CUPS_LANGUAGE_PRIVATE_H_
#  define _CUPS_LANGUAGE_PRIVATE_H_

/*
 * Include necessary headers...
 */

#  include <stdio.h>
#  include <cups/transcode.h>

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Macro for localized text...
 */

#  define _(x) x


/*
 * Types...
 */

typedef struct _cups_message_s		/**** Message catalog entry ****/
{
  char	*id,				/* Original string */
	*str;				/* Localized string */
} _cups_message_t;


/*
 * Prototypes...
 */

#  ifdef __APPLE__
extern const char	*_cupsAppleLanguage(const char *locale, char *language,
			                    size_t langsize);
#  endif /* __APPLE__ */
extern void		_cupsCharmapFlush(void);
extern const char	*_cupsEncodingName(cups_encoding_t encoding);
extern void		_cupsLangPrintError(const char *prefix,
			                    const char *message);
extern int		_cupsLangPrintFilter(FILE *fp, const char *prefix,
			                     const char *message, ...)
			__attribute__ ((__format__ (__printf__, 3, 4)));
extern int		_cupsLangPrintf(FILE *fp, const char *message, ...)
			__attribute__ ((__format__ (__printf__, 2, 3)));
extern int		_cupsLangPuts(FILE *fp, const char *message);
extern const char	*_cupsLangString(cups_lang_t *lang,
			                 const char *message);
extern void		_cupsMessageFree(cups_array_t *a);
extern cups_array_t	*_cupsMessageLoad(const char *filename, int unquote);
extern const char	*_cupsMessageLookup(cups_array_t *a, const char *m);
extern cups_array_t	*_cupsMessageNew(void *context);
extern void		_cupsSetLocale(char *argv[]);


#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_LANGUAGE_PRIVATE_H_ */

/*
 * End of "$Id: language-private.h 10996 2013-05-29 11:51:34Z msweet $".
 */
