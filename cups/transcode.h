/*
 * "$Id$"
 *
 *   Transcoding support for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are
 *   the property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the
 *   file "LICENSE.txt" which should have been included with this file.
 *   If this file is missing or damaged please contact Easy Software
 *   Products at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

#ifndef _CUPS_TRANSCODE_H_
#  define _CUPS_TRANSCODE_H_

/*
 * Include necessary headers...
 */

#  include "language.h"

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Types...
 */

typedef unsigned char  cups_utf8_t;  /* UTF-8 Unicode/ISO-10646 unit */
typedef unsigned short cups_utf16_t; /* UTF-16 Unicode/ISO-10646 unit */
typedef unsigned long  cups_utf32_t; /* UTF-32 Unicode/ISO-10646 unit */
typedef unsigned short cups_ucs2_t;  /* UCS-2 Unicode/ISO-10646 unit */
typedef unsigned long  cups_ucs4_t;  /* UCS-4 Unicode/ISO-10646 unit */
typedef unsigned char  cups_sbcs_t;  /* SBCS Legacy 8-bit unit */
typedef unsigned short cups_dbcs_t;  /* DBCS Legacy 16-bit unit */
typedef unsigned long  cups_vbcs_t;  /* VBCS Legacy 32-bit unit */
                                     /* EUC uses 8, 16, 24, 32-bit */


/*
 * Structures...
 */

typedef struct _cups_cmap_s		/**** SBCS Charmap Struct ****/
{
  struct _cups_cmap_s  *next;          /* Next charmap in cache */
  int                   used;           /* Number of times entry used */
  cups_encoding_t       encoding;       /* Legacy charset encoding */
  cups_ucs2_t           char2uni[256];  /* Map Legacy SBCS -> UCS-2 */
  cups_sbcs_t           *uni2char[256]; /* Map UCS-2 -> Legacy SBCS */
} _cups_cmap_t;

typedef struct _cups_wide2uni_s		/**** Wide to Unicode ****/
{
  cups_vbcs_t           widechar;       /* VBCS 32-bit Char (EUC) */
  cups_ucs2_t           unichar;        /* UCS-2 Char */
} _cups_wide2uni_t;

typedef struct _cups_vmap_s		/**** VBCS Charmap Struct ****/
{
  struct _cups_vmap_s  *next;          /* Next charmap in cache */
  int                   used;           /* Number of times entry used */
  cups_encoding_t       encoding;       /* Legacy charset encoding */
  cups_ucs2_t           *char2uni[256]; /* Map 16-bit Char -> UCS-2 */
  int                   charcount;      /* Count of 16-bit VBCS Chars */
  _cups_wide2uni_t       *wide2uni;      /* Map 32-bit Char -> UCS-2 */
  int                   widecount;      /* Count of 32-bit VBCS Chars */
  cups_vbcs_t           *uni2char[256]; /* Map UCS-2 -> 32-bit VBCS */
  cups_sbcs_t           lead2char[256]; /* Legacy Lead Char - 2-byte */
  cups_sbcs_t           lead3char[256]; /* Legacy Lead Char - 3-byte */
  cups_sbcs_t           lead4char[256]; /* Legacy Lead Char - 4-byte */
} _cups_vmap_t;


/*
 * Constants...
 */

#  define CUPS_MAX_USTRING		8192	/* Max size of Unicode string */
#  define CUPS_MAX_CHARMAP_LINES	100000	/* Max lines in charmap file */


/*
 * Prototypes...
 */

extern void     cupsCharmapFlush(void);
extern void     cupsCharmapFree(const cups_encoding_t encoding);
extern void     *cupsCharmapGet(const cups_encoding_t encoding);
extern int      cupsCharsetToUTF8(cups_utf8_t *dest,
                                  const char *src,
                                  const int maxout,
                                  const cups_encoding_t encoding);
extern int      cupsUTF8ToCharset(char *dest,
                                  const cups_utf8_t *src,
                                  const int maxout,
                                  const cups_encoding_t encoding);
extern int      cupsUTF8ToUTF32(cups_utf32_t *dest,
                                const cups_utf8_t *src,
                                const int maxout);
extern int      cupsUTF32ToUTF8(cups_utf8_t *dest,
                                const cups_utf32_t *src,
                                const int maxout);

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_TRANSCODE_H_ */


/*
 * End of "$Id$"
 */
