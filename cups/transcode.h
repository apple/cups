/*
 * "$Id: transcode.h,v 1.1.2.1 2002/08/19 01:15:21 mike Exp $"
 *
 *   Transcoding support for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
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

typedef unsigned char  utf8_t;  /* UTF-8 Unicode/ISO-10646 code unit */
typedef unsigned short utf16_t; /* UTF-16 Unicode/ISO-10646 code unit */
typedef unsigned long  utf32_t; /* UTF-32 Unicode/ISO-10646 code unit */
typedef unsigned short ucs2_t;  /* UCS-2 Unicode/ISO-10646 code unit */
typedef unsigned long  ucs4_t;  /* UCS-4 Unicode/ISO-10646 code unit */
typedef unsigned char  sbcs_t;  /* SBCS Legacy 8-bit code unit */
typedef unsigned short dbcs_t;  /* DBCS Legacy 16-bit code unit */

/*
 * Structures...
 */

typedef struct cups_cmap_str    /**** SBCS Charmap Cache Structure ****/
{
  struct cups_cmap_str  *next;          /* Next charmap in cache */
  int                   used;           /* Number of times entry used */
  cups_encoding_t       encoding;       /* Legacy charset encoding */
  ucs2_t                char2uni[256];  /* Map Legacy SBCS -> UCS-2 */
  sbcs_t                *uni2char[256]; /* Map UCS-2 -> Legacy SBCS */
} cups_cmap_t;

#if 0
typedef struct cups_dmap_str    /**** DBCS Charmap Cache Structure ****/
{
  struct cups_dmap_str  *next;          /* Next charmap in cache */
  int                   used;           /* Number of times entry used */
  cups_encoding_t       encoding;       /* Legacy charset encoding */
  ucs2_t                *char2uni[256]; /* Map Legacy DBCS -> UCS-2 */
  dbcs_t                *uni2char[256]; /* Map UCS-2 -> Legacy DBCS */
} cups_dmap_t;
#endif

/*
 * Constants...
 */
#define CUPS_MAX_USTRING    1024    /* Maximum size of Unicode string */

/*
 * Globals...
 */

extern int      TcFixMapNames;  /* Fix map names to Unicode names */
extern int      TcStrictUtf8;   /* Non-shortest-form is illegal */
extern int      TcStrictUtf16;  /* Invalid surrogate pair is illegal */
extern int      TcStrictUtf32;  /* Greater than 0x10FFFF is illegal */
extern int      TcRequireBOM;   /* Require BOM for little/big-endian */
extern int      TcSupportBOM;   /* Support BOM for little/big-endian */
extern int      TcSupport8859;  /* Support ISO 8859-x repertoires */
extern int      TcSupportWin;   /* Support Windows-x repertoires */
extern int      TcSupportCJK;   /* Support CJK (Asian) repertoires */

/*
 * Prototypes...
 */

/*
 * Utility functions for character set maps
 */
extern void     *cupsCharmapGet(const cups_encoding_t encoding);
                                                /* I - Encoding */
extern void     cupsCharmapFree(const cups_encoding_t encoding);
                                                /* I - Encoding */
extern void     cupsCharmapFlush(void);

/*
 * Convert UTF-8 to and from legacy character set
 */
extern int      cupsUtf8ToCharset(char *dest,   /* O - Target string */
                    const utf8_t *src,          /* I - Source string */
                    const int maxout,           /* I - Max output */
                    cups_encoding_t encoding);  /* I - Encoding */
extern int      cupsCharsetToUtf8(utf8_t *dest, /* O - Target string */
                    const char *src,            /* I - Source string */
                    const int maxout,           /* I - Max output */
                    cups_encoding_t encoding);  /* I - Encoding */

/*
 * Convert UTF-8 to and from UTF-16
 */
extern int      cupsUtf8ToUtf16(utf16_t *dest,  /* O - Target string */
                    const utf8_t *src,          /* I - Source string */
                    const int maxout);          /* I - Max output */
extern int      cupsUtf16ToUtf8(utf8_t *dest,   /* O - Target string */
                    const utf16_t *src,         /* I - Source string */
                    const int maxout);          /* I - Max output */

/*
 * Convert UTF-8 to and from UTF-32
 */
extern int      cupsUtf8ToUtf32(utf32_t *dest,  /* O - Target string */
                    const utf8_t *src,          /* I - Source string */
                    const int maxout);          /* I - Max output */
extern int      cupsUtf32ToUtf8(utf8_t *dest,   /* O - Target string */
                    const utf32_t *src,         /* I - Source string */
                    const int maxout);          /* I - Max output */

/*
 * Convert UTF-16 to and from UTF-32
 */
extern int      cupsUtf16ToUtf32(utf32_t *dest, /* O - Target string */
                    const utf16_t *src,         /* I - Source string */
                    const int maxout);          /* I - Max output */
extern int      cupsUtf32ToUtf16(utf16_t *dest, /* O - Target string */
                    const utf32_t *src,         /* I - Source string */
                    const int maxout);          /* I - Max output */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_TRANSCODE_H_ */

/*
 * End of "$Id: transcode.h,v 1.1.2.1 2002/08/19 01:15:21 mike Exp $"
 */
