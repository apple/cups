/*
 * "$Id$"
 *
 *   (Private) localization support for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
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
 */

#ifndef _CUPS_I18N_H_
#  define _CUPS_I18N_H_

/*
 * Include necessary headers...
 */

#  include "language.h"

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

/**** New in CUPS 1.1.20 ****/
extern void		_cupsRestoreLocale(int category, char *oldlocale);
extern char		*_cupsSaveLocale(int category, const char *locale);

/**** New in CUPS 1.2 ****/
extern const char	*_cupsEncodingName(cups_encoding_t encoding);
extern int		_cupsLangPrintf(FILE *fp, cups_lang_t *lang,
			                const char *message, ...)
#    ifdef __GNUC__
__attribute__ ((__format__ (__printf__, 3, 4)))
#    endif /* __GNUC__ */
;
extern int		_cupsLangPuts(FILE *fp, cups_lang_t *lang,
			              const char *message);
extern const char	*_cupsLangString(cups_lang_t *lang, const char *message);
extern void		_cupsMessageFree(cups_array_t *a);
extern cups_array_t	*_cupsMessageLoad(const char *filename);
extern const char	*_cupsMessageLookup(cups_array_t *a, const char *m);

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_I18N_H_ */

/*
 * End of "$Id$".
 */
