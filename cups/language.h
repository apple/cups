/*
 * "$Id: language.h,v 1.18.2.12 2002/08/19 01:15:20 mike Exp $"
 *
 *   Multi-language support for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
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
  CUPS_MSG_JOB_STATE,
  CUPS_MSG_JOB_NAME,
  CUPS_MSG_USER_NAME,
  CUPS_MSG_PRIORITY,
  CUPS_MSG_COPIES,
  CUPS_MSG_FILE_SIZE,
  CUPS_MSG_PENDING,
  CUPS_MSG_OUTPUT_MODE,
  CUPS_MSG_RESOLUTION,
  CUPS_MSG_TEXT,
  CUPS_MSG_PRETTYPRINT,
  CUPS_MSG_MARGINS,
  CUPS_MSG_LEFT,
  CUPS_MSG_RIGHT,
  CUPS_MSG_BOTTOM,
  CUPS_MSG_TOP,
  CUPS_MSG_FILENAME,
  CUPS_MSG_PRINT,
  CUPS_MSG_OPTIONS_INSTALLED,
  CUPS_MSG_AUTO,
  /* Next message #111 */

  CUPS_MSG_HTTP_BASE = 200,
  CUPS_MSG_HTTP_END = 505,

  CUPS_MSG_IPP_OK = 512,
  CUPS_MSG_IPP_OK_SUBST,
  CUPS_MSG_IPP_OK_CONFLICT,
  CUPS_MSG_IPP_OK_IGNORED_SUBSCRIPTIONS,
  CUPS_MSG_IPP_OK_IGNORED_NOTIFICATIONS,
  CUPS_MSG_IPP_OK_TOO_MANY_EVENTS,
  CUPS_MSG_IPP_OK_BUT_CANCEL_SUBSCRIPTION,

  CUPS_MSG_IPP_REDIRECTION_OTHER_SITE,

  CUPS_MSG_IPP_BAD_REQUEST = 528,
  CUPS_MSG_IPP_FORBIDDEN,
  CUPS_MSG_IPP_NOT_AUTHENTICATED,
  CUPS_MSG_IPP_NOT_AUTHORIZED,
  CUPS_MSG_IPP_NOT_POSSIBLE,
  CUPS_MSG_IPP_TIMEOUT,
  CUPS_MSG_IPP_NOT_FOUND,
  CUPS_MSG_IPP_GONE,
  CUPS_MSG_IPP_REQUEST_ENTITY,
  CUPS_MSG_IPP_REQUEST_VALUE,
  CUPS_MSG_IPP_DOCUMENT_FORMAT,
  CUPS_MSG_IPP_ATTRIBUTES,
  CUPS_MSG_IPP_URI_SCHEME,
  CUPS_MSG_IPP_CHARSET,
  CUPS_MSG_IPP_CONFLICT,
  CUPS_MSG_IPP_COMPRESSION_NOT_SUPPORTED,
  CUPS_MSG_IPP_COMPRESSION_ERROR,
  CUPS_MSG_IPP_DOCUMENT_FORMAT_ERROR,
  CUPS_MSG_IPP_DOCUMENT_ACCESS_ERROR,
  CUPS_MSG_IPP_ATTRIBUTES_NOT_SETTABLE,
  CUPS_MSG_IPP_IGNORED_ALL_SUBSCRIPTIONS,
  CUPS_MSG_IPP_TOO_MANY_SUBSCRIPTIONS,
  CUPS_MSG_IPP_IGNORED_ALL_NOTIFICATIONS,
  CUPS_MSG_IPP_PRINT_SUPPORT_FILE_NOT_FOUND,

  CUPS_MSG_IPP_INTERNAL_ERROR = 560,
  CUPS_MSG_IPP_OPERATION_NOT_SUPPORTED,
  CUPS_MSG_IPP_SERVICE_UNAVAILABLE,
  CUPS_MSG_IPP_VERSION_NOT_SUPPORTED,
  CUPS_MSG_IPP_DEVICE_ERROR,
  CUPS_MSG_IPP_TEMPORARY_ERROR,
  CUPS_MSG_IPP_NOT_ACCEPTING,
  CUPS_MSG_IPP_PRINTER_BUSY,
  CUPS_MSG_IPP_ERROR_JOB_CANCELLED,
  CUPS_MSG_IPP_MULTIPLE_JOBS_NOT_SUPPORTED,
  CUPS_MSG_IPP_PRINTER_IS_DEACTIVATED,

  CUPS_MSG_MAX = 1024
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
  CUPS_UTF8,
  CUPS_ISO8859_13,
  CUPS_ISO8859_14,
  CUPS_ISO8859_15,
  CUPS_WINDOWS_874,
  CUPS_WINDOWS_1250,
  CUPS_WINDOWS_1251,
  CUPS_WINDOWS_1252,
  CUPS_WINDOWS_1253,
  CUPS_WINDOWS_1254,
  CUPS_WINDOWS_1255,
  CUPS_WINDOWS_1256,
  CUPS_WINDOWS_1257,
  CUPS_WINDOWS_1258,
  CUPS_KOI8_R,
  CUPS_KOI8_U
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

#  if defined(WIN32) || defined(__EMX__) || defined(__APPLE__)
#    define		cupsLangDefault() cupsLangGet(setlocale(LC_ALL, ""))
#  else
#    define		cupsLangDefault() cupsLangGet(setlocale(LC_MESSAGES, ""))
#  endif /* WIN32 || __EMX__ || __APPLE__ */

extern const char	*cupsEncodingName(cups_encoding_t encoding);
extern const char	*cupsLangEncoding(cups_lang_t *lang);
extern void		cupsLangFlush(void);
extern void		cupsLangFree(cups_lang_t *lang);
extern cups_lang_t	*cupsLangGet(const char *language);
extern int		cupsLangPrintf(FILE *fp, cups_lang_t *lang,
			               cups_msg_t msg, ...);
extern int		cupsLangPuts(FILE *fp, cups_lang_t *lang,
			             cups_msg_t msg);
#  define		cupsLangString(lang,msg) (lang)->messages[(msg)]

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_LANGUAGE_H_ */

/*
 * End of "$Id: language.h,v 1.18.2.12 2002/08/19 01:15:20 mike Exp $".
 */
