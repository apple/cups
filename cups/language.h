/*
 * "$Id: language.h,v 1.7 1999/06/15 20:39:59 mike Exp $"
 *
 *   Multi-language support for the Common UNIX Printing System (CUPS).
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

#ifndef _CUPS_LANGUAGE_H_
#  define _CUPS_LANGUAGE_H_

/*
 * Include necessary headers...
 */

#  include <locale.h>

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */

/*
 * Messages...
 */

typedef enum			/**** Message Indices ****/
{
  CUPS_MSG_OK,
  CUPS_MSG_CANCEL,
  CUPS_MSG_HELP,
  CUPS_MSG_QUIT,
  CUPS_MSG_CLOSE,
  CUPS_MSG_YES,
  CUPS_MSG_NO,
  CUPS_MSG_ON,
  CUPS_MSG_OFF,
  CUPS_MSG_SAVE,
  CUPS_MSG_DISCARD,
  CUPS_MSG_DEFAULT,
  CUPS_MSG_OPTIONS,
  CUPS_MSG_MORE_INFO,
  CUPS_MSG_BLACK,
  CUPS_MSG_COLOR,
  CUPS_MSG_CYAN,
  CUPS_MSG_MAGENTA,
  CUPS_MSG_YELLOW,
  CUPS_MSG_COPYRIGHT,
  CUPS_MSG_GENERAL,
  CUPS_MSG_PRINTER,
  CUPS_MSG_IMAGE,
  CUPS_MSG_HPGL2,
  CUPS_MSG_EXTRA,
  CUPS_MSG_DOCUMENT,
  CUPS_MSG_OTHER,
  CUPS_MSG_PRINT_PAGES,
  CUPS_MSG_ENTIRE_DOCUMENT,
  CUPS_MSG_PAGE_RANGE,
  CUPS_MSG_REVERSE_ORDER,
  CUPS_MSG_PAGE_FORMAT,
  CUPS_MSG_1_UP,
  CUPS_MSG_2_UP,
  CUPS_MSG_4_UP,
  CUPS_MSG_IMAGE_SCALING,
  CUPS_MSG_USE_NATURAL_IMAGE_SIZE,
  CUPS_MSG_ZOOM_BY_PERCENT,
  CUPS_MSG_ZOOM_BY_PPI,
  CUPS_MSG_MIRROR_IMAGE,
  CUPS_MSG_COLOR_SATURATION,
  CUPS_MSG_COLOR_HUE,
  CUPS_MSG_FIT_TO_PAGE,
  CUPS_MSG_SHADING,
  CUPS_MSG_DEFAULT_PEN_WIDTH,
  CUPS_MSG_GAMMA_CORRECTION,
  CUPS_MSG_BRIGHTNESS,
  CUPS_MSG_ADD,
  CUPS_MSG_DELETE,
  CUPS_MSG_MODIFY,
  CUPS_MSG_PRINTER_URI,
  CUPS_MSG_PRINTER_NAME,
  CUPS_MSG_PRINTER_LOCATION,
  CUPS_MSG_PRINTER_INFO,
  CUPS_MSG_PRINTER_MAKE_AND_MODEL,
  CUPS_MSG_DEVICE_URI,
  CUPS_MSG_FORMATTING_PAGE,
  CUPS_MSG_PRINTING_PAGE,
  CUPS_MSG_INITIALIZING_PRINTER,
  CUPS_MSG_PRINTER_STATE,
  CUPS_MSG_ACCEPTING_JOBS,
  CUPS_MSG_NOT_ACCEPTING_JOBS,
  CUPS_MSG_PRINT_JOBS,
  CUPS_MSG_CLASS,
  CUPS_MSG_LOCAL,
  CUPS_MSG_REMOTE,
  CUPS_MSG_DUPLEXING,
  CUPS_MSG_STAPLING,
  CUPS_MSG_FAST_COPIES,
  CUPS_MSG_COLLATED_COPIES,
  CUPS_MSG_PUNCHING,
  CUPS_MSG_COVERING,
  CUPS_MSG_BINDING,
  CUPS_MSG_SORTING,
  CUPS_MSG_SMALL,
  CUPS_MSG_MEDIUM,
  CUPS_MSG_LARGE,
  CUPS_MSG_VARIABLE,
  CUPS_MSG_IDLE,
  CUPS_MSG_PROCESSING,
  CUPS_MSG_STOPPED,
  CUPS_MSG_ALL,
  CUPS_MSG_ODD,
  CUPS_MSG_EVEN_PAGES,
  CUPS_MSG_DARKER_LIGHTER,
  CUPS_MSG_MEDIA_SIZE,
  CUPS_MSG_MEDIA_TYPE,
  CUPS_MSG_MEDIA_SOURCE,
  CUPS_MSG_ORIENTATION,
  CUPS_MSG_PORTRAIT,
  CUPS_MSG_LANDSCAPE,
  CUPS_MSG_HTTP_BASE = 200,
  CUPS_MSG_HTTP_END = 505,
  CUPS_MSG_MAX
} cups_msg_t;

typedef enum			/**** Language Encodings ****/
{
  CUPS_US_ASCII,
  CUPS_ISO8859_1,
  CUPS_ISO8859_2,
  CUPS_ISO8859_3,
  CUPS_ISO8859_4,
  CUPS_ISO8859_5,
  CUPS_ISO8859_6,
  CUPS_ISO8859_7,
  CUPS_ISO8859_8,
  CUPS_ISO8859_9,
  CUPS_ISO8859_10,
  CUPS_UTF8
} cups_encoding_t;

typedef struct cups_lang_str	/**** Language Cache Structure ****/
{
  struct cups_lang_str	*next;		/* Next language in cache */
  int			used;		/* Number of times this entry has been used. */
  cups_encoding_t	encoding;	/* Text encoding */
  char			language[16];	/* Language/locale name */
  char			*messages[CUPS_MSG_MAX];
					/* Message array */
} cups_lang_t;


/*
 * Prototypes...
 */

#  define		cupsLangDefault() cupsLangGet(setlocale(LC_ALL, ""))
extern char		*cupsLangEncoding(cups_lang_t *lang);
extern void		cupsLangFlush(void);
extern void		cupsLangFree(cups_lang_t *lang);
extern cups_lang_t	*cupsLangGet(char *language);
#  define		cupsLangString(lang,msg) (lang)->messages[(msg)]

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_LANGUAGE_H_ */

/*
 * End of "$Id: language.h,v 1.7 1999/06/15 20:39:59 mike Exp $".
 */
