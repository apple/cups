/*
 * "$Id$"
 *
 *   Transcoding support for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
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
 *
 * Contents:
 *
 *   cupsCharmapGet()    - Get a character set map.
 *   cupsCharmapFree()   - Free a character set map.
 *   cupsCharmapFlush()  - Flush all character set maps out of cache.
 *   cupsUTF8ToCharset() - Convert UTF-8 to legacy character set.
 *   cupsCharsetToUTF8() - Convert legacy character set to UTF-8.
 *   cupsUTF8ToUTF16()   - Convert UTF-8 to UTF-16.
 *   cupsUTF16ToUTF8()   - Convert UTF-16 to UTF-8.
 *   cupsUTF8ToUTF32()   - Convert UTF-8 to UTF-32.
 *   cupsUTF32ToUTF8()   - Convert UTF-32 to UTF-8.
 *   cupsUTF16ToUTF32()  - Convert UTF-16 to UTF-32.
 *   cupsUTF32ToUTF16()  - Convert UTF-32 to UTF-16.
 *   get_charmap_count() - Count lines in a charmap file.
 *   get_sbcs_charmap()  - Get SBCS Charmap.
 *   get_vbcs_charmap()  - Get DBCS/VBCS Charmap.
 *   conv_utf8_to_sbcs() - Convert UTF-8 to legacy SBCS.
 *   conv_utf8_to_vbcs() - Convert UTF-8 to legacy DBCS/VBCS.
 *   conv_sbcs_to_utf8() - Convert legacy SBCS to UTF-8.
 *   conv_vbcs_to_utf8() - Convert legacy DBCS/VBCS to UTF-8.
 *   compare_wide()      - Compare key for wide (VBCS) match.
 */

/*
 * Include necessary headers...
 */

#include "globals.h"
#include <stdlib.h>
#include <errno.h>
#include <time.h>


/*
 * Prototypes...
 */

static int get_charmap_count(const char *filename);
static _cups_cmap_t *get_sbcs_charmap(const cups_encoding_t encoding,
                                     const char *filename);
static _cups_vmap_t *get_vbcs_charmap(const cups_encoding_t encoding,
                                     const char *filename);

static int conv_utf8_to_sbcs(char *dest,
                             const cups_utf8_t *src,
                             const int maxout,
                             const cups_encoding_t encoding);
static int conv_utf8_to_vbcs(char *dest,
                             const cups_utf8_t *src,
                             const int maxout,
                             const cups_encoding_t encoding);

static int conv_sbcs_to_utf8(cups_utf8_t *dest,
                             const char *src,
                             const int maxout,
                             const cups_encoding_t encoding);
static int conv_vbcs_to_utf8(cups_utf8_t *dest,
                             const char *src,
                             const int maxout,
                             const cups_encoding_t encoding);

static int compare_wide(const void *k1, const void *k2);

/*
 * 'cupsCharmapGet()' - Get a character set map.
 *
 * This code handles single-byte (SBCS), double-byte (DBCS), and
 * variable-byte (VBCS) character sets _without_ charset escapes...
 * This code does not handle multiple-byte character sets (MBCS)
 * (such as ISO-2022-JP) with charset switching via escapes...
 */
void *                                  /* O - Charset map pointer */
cupsCharmapGet(const cups_encoding_t encoding)
                                        /* I - Encoding */
{
  char          *datadir;       /* CUPS_DATADIR environment variable */
  char          mapname[80];    /* Name of charset map */
  char          filename[1024];  /* Filename for charset map file */

 /*
  * Check for valid arguments...
  */
  if ((encoding < 0) || (encoding >= CUPS_ENCODING_VBCS_END))
    return (NULL);

 /*
  * Get the data directory and charset map name...
  */
  if ((datadir = getenv("CUPS_DATADIR")) == NULL)
    datadir = CUPS_DATADIR;
  snprintf(mapname, sizeof(mapname), "%s.txt", cupsEncodingName(encoding));
  snprintf(filename, sizeof(filename), "%s/charmaps/%s",
           datadir, mapname);

 /*
  * Read charset map input file into cache...
  */
  if (encoding < CUPS_ENCODING_SBCS_END)
    return (get_sbcs_charmap(encoding, filename));
  else if (encoding < CUPS_ENCODING_VBCS_END)
    return (get_vbcs_charmap(encoding, filename));
  else
    return (NULL);
}

/*
 * 'cupsCharmapFree()' - Free a character set map.
 *
 * This does not actually free; use 'cupsCharmapFlush()' for that.
 */
void
cupsCharmapFree(const cups_encoding_t encoding)
                                        /* I - Encoding */
{
  _cups_cmap_t   *cmap;          /* Legacy SBCS / Unicode Charset Map */
  _cups_vmap_t   *vmap;          /* Legacy VBCS / Unicode Charset Map */
  _cups_globals_t *cg = _cupsGlobals();
				/* Pointer to library globals */

 /*
  * See if we already have this SBCS charset map loaded...
  */
  for (cmap = cg->cmap_cache; cmap != NULL; cmap = cmap->next)
  {
    if (cmap->encoding == encoding)
    {
      if (cmap->used > 0)
        cmap->used --;
      return;
    }
  }

 /*
  * See if we already have this DBCS/VBCS charset map loaded...
  */
  for (vmap = cg->vmap_cache; vmap != NULL; vmap = vmap->next)
  {
    if (vmap->encoding == encoding)
    {
      if (vmap->used > 0)
        vmap->used --;
      return;
    }
  }
  return;
}

/*
 * 'cupsCharmapFlush()' - Flush all character set maps out of cache.
 */
void
cupsCharmapFlush(void)
{
  int           i;              /* Looping variable */
  _cups_cmap_t   *cmap;          /* Legacy SBCS / Unicode Charset Map */
  _cups_vmap_t   *vmap;          /* Legacy VBCS / Unicode Charset Map */
  _cups_cmap_t   *cnext;         /* Next Legacy SBCS Charset Map */
  _cups_vmap_t   *vnext;         /* Next Legacy VBCS Charset Map */
  cups_ucs2_t   *crow;          /* Pointer to UCS-2 row in 'char2uni' */
  cups_sbcs_t   *srow;          /* Pointer to SBCS row in 'uni2char' */
  cups_vbcs_t   *vrow;          /* Pointer to VBCS row in 'uni2char' */
  _cups_globals_t *cg = _cupsGlobals();
  				/* Pointer to library globals */

 /*
  * Loop through SBCS charset map cache, free all memory...
  */
  for (cmap = cg->cmap_cache; cmap != NULL; cmap = cnext)
  {
    for (i = 0; i < 256; i ++)
    {
      if ((srow = cmap->uni2char[i]) != NULL)
        free(srow);
    }
    cnext = cmap->next;
    free(cmap);
  }
  cg->cmap_cache = NULL;

 /*
  * Loop through DBCS/VBCS charset map cache, free all memory...
  */
  for (vmap = cg->vmap_cache; vmap != NULL; vmap = vnext)
  {
    for (i = 0; i < 256; i ++)
    {
      if ((crow = vmap->char2uni[i]) != NULL)
        free(crow);
    }
    for (i = 0; i < 256; i ++)
    {
      if ((vrow = vmap->uni2char[i]) != NULL)
        free(vrow);
    }
    if (vmap->wide2uni)
      free(vmap->wide2uni);
    vnext = vmap->next;
    free(vmap);
  }
  cg->vmap_cache = NULL;
  return;
}

/*
 * 'cupsUTF8ToCharset()' - Convert UTF-8 to legacy character set.
 *
 * This code handles single-byte (SBCS), double-byte (DBCS), and
 * variable-byte (VBCS) character sets _without_ charset escapes...
 * This code does not handle multiple-byte character sets (MBCS)
 * (such as ISO-2022-JP) with charset switching via escapes...
 */
int                                     /* O - Count or -1 on error */
cupsUTF8ToCharset(char *dest,           /* O - Target string */
    const cups_utf8_t *src,             /* I - Source string */
    const int maxout,                   /* I - Max output */
    const cups_encoding_t encoding)     /* I - Encoding */
{
 /*
  * Check for valid arguments...
  */
  if ((dest == NULL)
  || (src == NULL)
  || (maxout < 1)
  || (maxout > CUPS_MAX_USTRING)
  || (encoding < 0)
  || (encoding == CUPS_UTF8)
  || (encoding >= CUPS_ENCODING_VBCS_END))
    return (-1);

 /*
  * Convert input UTF-8 to legacy charset...
  */
  if (encoding < CUPS_ENCODING_SBCS_END)
    return (conv_utf8_to_sbcs(dest, src, maxout, encoding));
  else if (encoding < CUPS_ENCODING_VBCS_END)
    return (conv_utf8_to_vbcs(dest, src, maxout, encoding));
  else
    return (-1);
}

/*
 * 'cupsCharsetToUTF8()' - Convert legacy character set to UTF-8.
 *
 * This code handles single-byte (SBCS), double-byte (DBCS), and
 * variable-byte (VBCS) character sets _without_ charset escapes...
 * This code does not handle multiple-byte character sets (MBCS)
 * (such as ISO-2022-JP) with charset switching via escapes...
 */
int                                     /* O - Count or -1 on error */
cupsCharsetToUTF8(cups_utf8_t *dest,    /* O - Target string */
    const char *src,                    /* I - Source string */
    const int maxout,                   /* I - Max output */
    const cups_encoding_t encoding)     /* I - Encoding */
{
 /*
  * Check for valid arguments...
  */
  if ((dest == NULL)
  || (src == NULL)
  || (maxout < 1)
  || (maxout > CUPS_MAX_USTRING)
  || (encoding < 0)
  || (encoding == CUPS_UTF8)
  || (encoding >= CUPS_ENCODING_VBCS_END))
    return (-1);

 /*
  * Convert input legacy charset to UTF-8...
  */
  if (encoding < CUPS_ENCODING_SBCS_END)
    return (conv_sbcs_to_utf8(dest, src, maxout, encoding));
  else if (encoding < CUPS_ENCODING_VBCS_END)
    return (conv_vbcs_to_utf8(dest, src, maxout, encoding));
  else
    return (-1);
}

/*
 * 'cupsUTF8ToUTF16()' - Convert UTF-8 to UTF-16.
 *
 * This code does not support Unicode beyond 16-bits (Plane 0)...
 */
int                                     /* O - Count or -1 on error */
cupsUTF8ToUTF16(cups_utf16_t *dest,     /* O - Target string */
    const cups_utf8_t *src,             /* I - Source string */
    const int maxout)                   /* I - Max output */
{
  int           worklen;        /* Internal UCS-4 string length */
  cups_utf32_t  work[CUPS_MAX_USTRING];
                                /* Internal UCS-4 string */

 /*
  * Check for valid arguments and clear output...
  */
  if ((dest == NULL)
  || (src == NULL)
  || (maxout < 1)
  || (maxout > CUPS_MAX_USTRING))
    return (-1);
  *dest = 0;

 /*
  * Convert input UTF-8 to internal UCS-4 (and insert BOM)...
  */
  worklen = cupsUTF8ToUTF32(work, src, CUPS_MAX_USTRING);
  if (worklen < 0)
    return (-1);

 /*
  * Convert internal UCS-4 to output UTF-16...
  */
  worklen = cupsUTF32ToUTF16(dest, work, maxout);
  return (worklen);
}

/*
 * 'cupsUTF16ToUTF8()' - Convert UTF-16 to UTF-8.
 *
 * This code does not support Unicode beyond 16-bits (Plane 0)...
 */
int                                     /* O - Count or -1 on error */
cupsUTF16ToUTF8(cups_utf8_t *dest,      /* O - Target string */
    const cups_utf16_t *src,            /* I - Source string */
    const int maxout)                   /* I - Max output */
{
  int           worklen;        /* Internal UCS-4 string length */
  cups_utf32_t  work[CUPS_MAX_USTRING];
                                /* Internal UCS-4 string */

 /*
  * Check for valid arguments and clear output...
  */
  if ((dest == NULL)
  || (src == NULL)
  || (maxout < 1)
  || (maxout > CUPS_MAX_USTRING))
    return (-1);
  *dest = 0;

 /*
  * Convert input UTF-16 to internal UCS-4 (and byte-swap)...
  */
  worklen = cupsUTF16ToUTF32(work, src, CUPS_MAX_USTRING);
  if (worklen < 0)
    return (-1);

 /*
  * Convert internal UCS-4 to output UTF-8 (and delete BOM)...
  */
  worklen = cupsUTF32ToUTF8(dest, work, maxout);
  return (worklen);
}

/*
 * 'cupsUTF8ToUTF32()' - Convert UTF-8 to UTF-32.
 *
 * 32-bit UTF-32 (actually 21-bit) maps to UTF-8 as follows...
 *
 *   UTF-32 char     UTF-8 char(s)
 *   --------------------------------------------------
 *        0 to 127 = 0xxxxxxx (US-ASCII)
 *     128 to 2047 = 110xxxxx 10yyyyyy
 *   2048 to 65535 = 1110xxxx 10yyyyyy 10zzzzzz
 *         > 65535 = 11110xxx 10yyyyyy 10zzzzzz 10xxxxxx
 *
 * UTF-32 prohibits chars beyond Plane 16 (> 0x10ffff) in UCS-4,
 * which would convert to five- or six-octet UTF-8 sequences...
 *
 * This code does not support Unicode beyond 16-bits (Plane 0)...
 */
int                                     /* O - Count or -1 on error */
cupsUTF8ToUTF32(cups_utf32_t *dest,     /* O - Target string */
    const cups_utf8_t *src,             /* I - Source string */
    const int maxout)                   /* I - Max output */
{
  cups_utf8_t   *first = (cups_utf8_t *) src;
  size_t        srclen;         /* Source string length */
  int           i;              /* Looping variable */
  cups_utf32_t  ch;             /* Character value */
  cups_utf32_t  next;           /* Next character value */
  cups_utf32_t  ch32;           /* UTF-32 character value */

 /*
  * Check for valid arguments and clear output...
  */
  if ((dest == NULL)
  || (src == NULL)
  || (maxout < 1)
  || (maxout > CUPS_MAX_USTRING))
    return (-1);
  *dest = 0;

 /*
  * Convert input UTF-8 to output UTF-32 (and insert BOM)...
  */
  *dest = 0xfeff;
  dest ++;
  srclen = strlen((char *) src);
  for (i = 1; i < (maxout - 1); src ++, dest ++)
  {
    ch = (cups_utf32_t) *src;
    ch &= 0xff;
    if (ch == 0)
      break;
    i ++;

   /*
    * Convert UTF-8 character(s) to UTF-32 character...
    */
    if ((ch & 0x7f) == ch)
    {
     /*
      * One-octet UTF-8 <= 127 (US-ASCII)...
      */
      *dest = ch;
    }
    else if ((ch & 0xe0) == 0xc0)
    {
     /*
      * Two-octet UTF-8 <= 2047 (Latin-x)...
      */
      src ++;
      next = (cups_utf32_t) *src;
      next &= 0xff;
      if (next == 0)
        return (-1);
      ch32 = ((ch & 0x1f) << 6) | (next & 0x3f);

     /*
      * Check for non-shortest form (invalid UTF-8)...
      */
      if (ch32 <= 127)
        return (-1);
      *dest = ch32;
    }
    else if ((ch & 0xf0) == 0xe0)
    {
     /*
      * Three-octet UTF-8 <= 65535 (Plane 0 - BMP)...
      */
      src ++;
      next = (cups_utf32_t) *src;
      next &= 0xff;
      if (next == 0)
        return (-1);
      ch32 = ((ch & 0x1f) << 6) | (next & 0x3f);
      src ++;
      next = (cups_utf32_t) *src;
      next &= 0xff;
      if (next == 0)
        return (-1);
      ch32 = ((ch32 << 6) | (next & 0x3f));

     /*
      * Check for non-shortest form (invalid UTF-8)...
      */
      if (ch32 <= 2047)
        return (-1);
      *dest = ch32;
    }
    else if ((ch & 0xf8) == 0xf0)
    {
     /*
      * Four-octet UTF-8 to Replacement Character...
      */
      if (((src - first) + 3) >= srclen)
        return (-1);
      src += 3;
      *dest = 0xfffd;
    }
    else if ((ch & 0xfc) == 0xf8)
    {
     /*
      * Five-octet UTF-8 (invalid strict UTF-32)...
      */
      return (-1);
    }
    else if ((ch & 0xfe) == 0xfc)
    {
     /*
      * Six-octet UTF-8 (invalid strict UTF-32)...
      */
      return (-1);
    }
    else
    {
     /*
      * More than six-octet (invalid UTF-8 sequence)...
      */
      return (-1);
    }

   /*
    * Check for UTF-16 surrogate (illegal UTF-8)...
    */
    if ((*dest >= 0xd800) && (*dest <= 0xdfff))
      return (-1);

   /*
    * Check for beyond Plane 16 (invalid UTF-8)...
    */
    if (*dest > 0x10ffff)
      return (-1);
  }
  *dest = 0;
  return (i);
}

/*
 * 'cupsUTF32ToUTF8()' - Convert UTF-32 to UTF-8.
 *
 * 32-bit UTF-32 (actually 21-bit) maps to UTF-8 as follows...
 *
 *   UTF-32 char     UTF-8 char(s)
 *   --------------------------------------------------
 *        0 to 127 = 0xxxxxxx (US-ASCII)
 *     128 to 2047 = 110xxxxx 10yyyyyy
 *   2048 to 65535 = 1110xxxx 10yyyyyy 10zzzzzz
 *         > 65535 = 11110xxx 10yyyyyy 10zzzzzz 10xxxxxx
 *
 * UTF-32 prohibits chars beyond Plane 16 (> 0x10ffff) in UCS-4,
 * which would convert to five- or six-octet UTF-8 sequences...
 *
 * This code does not support Unicode beyond 16-bits (Plane 0)...
 */
int                                     /* O - Count or -1 on error */
cupsUTF32ToUTF8(cups_utf8_t *dest,      /* O - Target string */
    const cups_utf32_t *src,            /* I - Source string */
    const int maxout)                   /* I - Max output */
{
  cups_utf32_t  *first = (cups_utf32_t *) src;
                                /* First source char */
  cups_utf8_t   *start = dest;  /* Start of destination string */
  int           i;              /* Looping variable */
  int           swap = 0;       /* Byte-swap input to output */
  cups_utf32_t  ch;             /* Character value */

 /*
  * Check for valid arguments and clear output...
  */
  if ((dest == NULL)
  || (src == NULL)
  || (maxout < 1))
    return (-1);
  *dest = '\0';

 /*
  * Check for leading BOM in UTF-32 and inverted BOM...
  */
  if (*src == 0xfffe0000)
    swap = 1;

 /*
  * Convert input UTF-32 to output UTF-8...
  */
  for (i = 0; i < (maxout - 1); src ++)
  {
    ch = *src;
    if (ch == 0)
      break;

   /*
    * Byte swap input UTF-32, if necessary...
    */
    if (swap)
      ch = ((ch >> 24) | ((ch >> 8) & 0xff00) | ((ch << 8) & 0xff0000));

   /*
    * Check for leading BOM (and delete from output)...
    */
    if ((src == first) && (ch == 0xfeff))
      continue;

   /*
    * Check for beyond Plane 16 (invalid UTF-32)...
    */
    if (ch > 0x10ffff)
      return (-1);

   /*
    * Convert beyond Plane 0 (BMP) to Replacement Character...
    */
    if (ch > 0xffff)
      ch = 0xfffd;

   /*
    * Convert UTF-32 character to UTF-8 character(s)...
    */
    if (ch <= 0x7f)
    {
     /*
      * One-octet UTF-8 <= 127 (US-ASCII)...
      */
      *dest = (cups_utf8_t) ch;
      dest ++;
      i ++;
    }
    else if (ch <= 0x7ff)
    {
     /*
      * Two-octet UTF-8 <= 2047 (Latin-x)...
      */
      if (i > (maxout - 2))
        break;
      *dest = (cups_utf8_t) (0xc0 | ((ch >> 6) & 0x1f));
      dest ++;
      i ++;
      *dest = (cups_utf8_t) (0x80 | (ch & 0x3f));
      dest ++;
      i ++;
    }
    else
    {
     /*
      * Three-octet UTF-8 <= 65535 (Plane 0 - BMP)...
      */
      if (i > (maxout - 3))
        break;
      *dest = (cups_utf8_t) (0xe0 | ((ch >> 12) & 0x0f));
      dest ++;
      i ++;
      *dest = (cups_utf8_t) (0x80 | ((ch >> 6) & 0x3f));
      dest ++;
      i ++;
      *dest = (cups_utf8_t) (0x80 | (ch & 0x3f));
      dest ++;
      i ++;
    }
  }
  *dest = '\0';
  i = (int) (dest - start);
  return (i);
}

/*
 * 'cupsUTF16ToUTF32()' - Convert UTF-16 to UTF-32.
 *
 * This code does not support Unicode beyond 16-bits (Plane 0)...
 */
int                                     /* O - Count or -1 on error */
cupsUTF16ToUTF32(cups_utf32_t *dest,    /* O - Target string */
    const cups_utf16_t *src,            /* I - Source string */
    const int maxout)                   /* I - Max output */
{
  int           i;              /* Looping variable */
  int           swap = 0;       /* Byte-swap input to output */
  int           surrogate = 0;  /* Expecting low-half surrogate */
  cups_utf32_t  ch;             /* Character value */

 /*
  * Check for valid arguments and clear output...
  */
  if ((dest == NULL)
  || (src == NULL)
  || (maxout < 1)
  || (maxout > CUPS_MAX_USTRING))
    return (-1);
  *dest = 0;

 /*
  * Check for leading BOM in UTF-16 and inverted BOM...
  */
  if (*src == 0xfffe)
    swap = 1;

 /*
  * Convert input UTF-16 to output UTF-32...
  */
  for (i = 0; i < (maxout - 1); src ++)
  {
    ch = (cups_utf32_t) (*src & 0xffff);
    if (ch == 0)
      break;
    i ++;

   /*
    * Byte swap input UTF-16, if necessary...
    */
    if (swap)
      ch = (cups_utf32_t) ((ch << 8) | (ch >> 8));

   /*
    * Discard expected UTF-16 low-half surrogate...
    */
    if ((ch >= 0xdc00) && (ch <= 0xdfff))
    {
      if (surrogate == 0)
        return (-1);
      surrogate = 0;
      continue;
    }

   /*
    * Convert UTF-16 high-half surrogate to Replacement Character...
    */
    if ((ch >= 0xd800) && (ch <= 0xdbff))
    {
      if (surrogate == 1)
        return (-1);
      surrogate = 1;
      ch = 0xfffd;
    }
    *dest = ch;
    dest ++;
  }
  *dest = 0;
  return (i);
}

/*
 * 'cupsUTF32ToUTF16()' - Convert UTF-32 to UTF-16.
 *
 * This code does not support Unicode beyond 16-bits (Plane 0)...
 */
int                                     /* O - Count or -1 on error */
cupsUTF32ToUTF16(cups_utf16_t *dest,    /* O - Target string */
    const cups_utf32_t *src,            /* I - Source string */
    const int maxout)                   /* I - Max output */
{
  int           i;              /* Looping variable */
  int           swap = 0;       /* Byte-swap input to output */
  cups_utf32_t  ch;             /* Character value */

 /*
  * Check for valid arguments and clear output...
  */
  if ((dest == NULL)
  || (src == NULL)
  || (maxout < 1)
  || (maxout > CUPS_MAX_USTRING))
    return (-1);
  *dest = 0;

 /*
  * Check for leading BOM in UTF-32 and inverted BOM...
  */
  if (*src == 0xfffe0000)
    swap = 1;

 /*
  * Convert input UTF-32 to output UTF-16 (w/out surrogate pairs)...
  */
  for (i = 0; i < (maxout - 1); src ++, dest ++)
  {
    ch = *src;
    if (ch == 0)
      break;
    i ++;

   /*
    * Byte swap input UTF-32, if necessary...
    */
    if (swap)
      ch = ((ch >> 24) | ((ch >> 8) & 0xff00) | ((ch << 8) & 0xff0000));

   /*
    * Check for UTF-16 surrogate (illegal UTF-32)...
    */
    if ((ch >= 0xd800) && (ch <= 0xdfff))
      return (-1);

   /*
    * Check for beyond Plane 16 (invalid UTF-32)...
    */
    if (ch > 0x10ffff)
      return (-1);

   /*
    * Convert beyond Plane 0 (BMP) to Replacement Character...
    */
    if (ch > 0xffff)
      ch = 0xfffd;
    *dest = (cups_utf16_t) ch;
  }
  *dest = 0;
  return (i);
}

/*
 * 'get_charmap_count()' - Count lines in a charmap file.
 */
static int                              /* O - Count or -1 on error */
get_charmap_count(const char *filename) /* I - Charmap Filename */
{
  int           i;              /* Looping variable */
  cups_file_t	*fp;            /* Map input file pointer */
  char          *s;             /* Line parsing pointer */
  char          line[256];      /* Line from input map file */
  cups_utf32_t  unichar;        /* Unicode character value */

 /*
  * Open map input file...
  */
  if ((filename == NULL) || (*filename == '\0'))
    return (-1);
  fp = cupsFileOpen(filename, "r");
  if (fp == NULL)
    return (-1);

 /*
  * Count lines in map input file...
  */
  for (i = 0; i < CUPS_MAX_CHARMAP_LINES;)
  {
    s = cupsFileGets(fp, line, sizeof(line));
    if (s == NULL)
      break;
    if ((*s == '#') || (*s == '\n') || (*s == '\0'))
      continue;
    while ((*s != 0) && (*s != ' ') && (*s != '\t'))
        s ++;
    while ((*s == ' ') || (*s == '\t'))
        s ++;
    if (strncmp (s, "0x", 2) == 0)
      s += 2;
    if ((sscanf(s, "%lx", &unichar) != 1)
    || (unichar > 0xffff))
    {
      cupsFileClose(fp);
      return (-1);
    }
    i ++;
  }
  if (i == 0)
    i = -1;

 /*
  * Close file and return charmap count (non-comment line count)...
  */
  cupsFileClose(fp);
  return (i);
}

/*
 * 'get_sbcs_charmap()' - Get SBCS Charmap.
 */
static _cups_cmap_t *                    /* O - Charmap or 0 on error */
get_sbcs_charmap(const cups_encoding_t encoding,
                                        /* I - Charmap Encoding */
                 const char *filename)  /* I - Charmap Filename */
{
  int           i;              /* Loop variable */
  unsigned long legchar;        /* Legacy character value */
  cups_utf32_t  unichar;        /* Unicode character value */
  _cups_cmap_t   *cmap;          /* Legacy SBCS / Unicode Charset Map */
  cups_file_t   *fp;            /* Charset map file pointer */
  char          *s;             /* Line parsing pointer */
  cups_ucs2_t   *crow;          /* Pointer to UCS-2 row in 'char2uni' */
  cups_sbcs_t   *srow;          /* Pointer to SBCS row in 'uni2char' */
  char          line[256];      /* Line from charset map file */
  _cups_globals_t *cg = _cupsGlobals();
				/* Pointer to library globals */

 /*
  * Check for valid arguments...
  */
  if ((encoding < 0) || (filename == NULL))
    return (NULL);

 /*
  * See if we already have this SBCS charset map loaded...
  */
  for (cmap = cg->cmap_cache; cmap != NULL; cmap = cmap->next)
  {
    if (cmap->encoding == encoding)
    {
      cmap->used ++;
      return ((void *) cmap);
    }
  }

 /*
  * Open SBCS charset map input file...
  */
  fp = cupsFileOpen(filename, "r");
  if (fp == NULL)
    return (NULL);

 /*
  * Allocate memory for SBCS charset map and add to cache...
  */
  cmap = (_cups_cmap_t *) calloc(1, sizeof(_cups_cmap_t));
  if (cmap == NULL)
  {
    cupsFileClose(fp);
    return (NULL);
  }
  cmap->next = cg->cmap_cache;
  cg->cmap_cache = cmap;
  cmap->used ++;
  cmap->encoding = encoding;

 /*
  * Save SBCS charset map into memory for transcoding...
  */
  for (i = 0; i < CUPS_MAX_CHARMAP_LINES;)
  {
    s = cupsFileGets(fp, line, sizeof(line));
    if (s == NULL)
      break;
    if ((*s == '#') || (*s == '\n') || (*s == '\0'))
      continue;
    if (strncmp (s, "0x", 2) == 0)
      s += 2;
    if ((sscanf(s, "%lx", &legchar) != 1)
    || (legchar > 0xff))
    {
      cupsFileClose(fp);
      cupsCharmapFlush();
      return (NULL);
    }
    while ((*s != 0) && (*s != ' ') && (*s != '\t'))
      s ++;
    while ((*s == ' ') || (*s == '\t'))
      s ++;
    if (strncmp (s, "0x", 2) == 0)
      s += 2;
    if (sscanf(s, "%lx", &unichar) != 1)
    {
      cupsFileClose(fp);
      cupsCharmapFlush();
      return (NULL);
    }
    i ++;

   /*
    * Convert beyond Plane 0 (BMP) to Replacement Character...
    */
    if (unichar > 0xffff)
      unichar = 0xfffd;

   /*
    * Save legacy to Unicode mapping in direct lookup table...
    */
    crow = &cmap->char2uni[(int) legchar];
    *crow = (cups_ucs2_t) (unichar & 0xffff);

   /*
    * Save Unicode to legacy mapping in indirect lookup table...
    */
    srow = cmap->uni2char[(int) ((unichar >> 8) & 0xff)];
    if (srow == NULL)
    {
      srow = (cups_sbcs_t *) calloc(256, sizeof(cups_sbcs_t));
      if (srow == NULL)
      {
        cupsFileClose(fp);
        cupsCharmapFlush();
        return (NULL);
      }
      cmap->uni2char[(int) ((unichar >> 8) & 0xff)] = srow;
    }
    srow += (int) (unichar & 0xff);

   /*
    * Convert Replacement Character to visible replacement...
    */
    if (unichar == 0xfffd)
      legchar = (unsigned long) '?';

   /*
    * First (oldest) legacy character uses Unicode mapping cell...
    */
    if (*srow == 0)
      *srow = (cups_sbcs_t) legchar;
  }
  cupsFileClose(fp);
  return (cmap);
}

/*
 * 'get_vbcs_charmap()' - Get DBCS/VBCS Charmap.
 */
static _cups_vmap_t *                    /* O - Charmap or 0 on error */
get_vbcs_charmap(const cups_encoding_t encoding,
                                        /* I - Charmap Encoding */
                 const char *filename)  /* I - Charmap Filename */
{
  _cups_vmap_t       *vmap;      /* Legacy VBCS / Unicode Charset Map */
  cups_ucs2_t       *crow;      /* Pointer to UCS-2 row in 'char2uni' */
  cups_vbcs_t       *vrow;      /* Pointer to VBCS row in 'uni2char' */
  _cups_wide2uni_t   *wide2uni;  /* Pointer to row in 'wide2uni' */
  cups_sbcs_t       leadchar;   /* Lead char of 2-byte legacy char */
  unsigned long     legchar;    /* Legacy character value */
  cups_utf32_t      unichar;    /* Unicode character value */
  int               mapcount;   /* Count of lines in charmap file */
  cups_file_t       *fp;        /* Charset map file pointer */
  char              *s;         /* Line parsing pointer */
  char              line[256];  /* Line from charset map file */
  int               i;          /* Loop variable */
  int               wide;       /* 32-bit legacy char */
  _cups_globals_t    *cg = _cupsGlobals();
				/* Pointer to library globals */

 /*
  * Check for valid arguments...
  */
  if ((encoding < 0) || (filename == NULL))
    return (NULL);

 /*
  * See if we already have this DBCS/VBCS charset map loaded...
  */
  for (vmap = cg->vmap_cache; vmap != NULL; vmap = vmap->next)
  {
    if (vmap->encoding == encoding)
    {
      vmap->used ++;
      return ((void *) vmap);
    }
  }

 /*
  * Count lines in charmap file...
  */
  mapcount = get_charmap_count(filename);
  if (mapcount <= 0)
    return (NULL);

 /*
  * Open VBCS charset map input file...
  */
  fp = cupsFileOpen(filename, "r");
  if (fp == NULL)
    return (NULL);

 /*
  * Allocate memory for DBCS/VBCS charset map and add to cache...
  */
  vmap = (_cups_vmap_t *) calloc(1, sizeof(_cups_vmap_t));
  if (vmap == NULL)
  {
    cupsFileClose(fp);
    return (NULL);
  }
  vmap->next = cg->vmap_cache;
  cg->vmap_cache = vmap;
  vmap->used ++;
  vmap->encoding = encoding;

 /*
  * Save DBCS/VBCS charset map into memory for transcoding...
  */
  leadchar = 0;
  wide2uni = NULL;

  for (i = 0, wide = 0; i < mapcount; )
  {
    s = cupsFileGets(fp, line, sizeof(line));
    if (s == NULL)
      break;
    if ((*s == '#') || (*s == '\n') || (*s == '\0'))
      continue;
    if (strncmp (s, "0x", 2) == 0)
      s += 2;
    if ((sscanf(s, "%lx", &legchar) != 1)
    || ((legchar > 0xffff) && (encoding < CUPS_ENCODING_DBCS_END)))
    {
      cupsFileClose(fp);
      cupsCharmapFlush();
      return (NULL);
    }
    while ((*s != 0) && (*s != ' ') && (*s != '\t'))
      s ++;
    while ((*s == ' ') || (*s == '\t'))
      s ++;
    if (strncmp (s, "0x", 2) == 0)
      s += 2;
    if (sscanf(s, "%lx", &unichar) != 1)
    {
      cupsFileClose(fp);
      cupsCharmapFlush();
      return (NULL);
    }
    i ++;

   /*
    * Convert beyond Plane 0 (BMP) to Replacement Character...
    */
    if (unichar > 0xffff)
      unichar = 0xfffd;

   /*
    * Save lead char of 2/3/4-byte legacy char...
    */
    if ((legchar > 0xff) && (legchar <= 0xffff))
    {
      leadchar = (cups_sbcs_t) (legchar >> 8);
      vmap->lead2char[leadchar] = leadchar;
    }
    if ((legchar > 0xffff) && (legchar <= 0xffffff))
    {
      leadchar = (cups_sbcs_t) (legchar >> 16);
      vmap->lead3char[leadchar] = leadchar;
    }
    if (legchar > 0xffffff)
    {
      leadchar = (cups_sbcs_t) (legchar >> 24);
      vmap->lead4char[leadchar] = leadchar;
    }

   /*
    * Save Legacy to Unicode mapping...
    */
    if (legchar <= 0xffff)
    {
     /*
      * Save DBCS 16-bit to Unicode mapping in indirect lookup table...
      */
      crow = vmap->char2uni[(int) leadchar];
      if (crow == NULL)
      {
        crow = (cups_ucs2_t *) calloc(256, sizeof(cups_ucs2_t));
        if (crow == NULL)
        {
          cupsFileClose(fp);
          cupsCharmapFlush();
          return (NULL);
        }
        vmap->char2uni[(int) leadchar] = crow;
      }
      crow += (int) (legchar & 0xff);
      *crow = (cups_ucs2_t) unichar;
    }
    else
    {
     /*
      * Save VBCS 32-bit to Unicode mapping in sorted list table...
      */
      if (wide == 0)
      {
        wide = 1;
        vmap->widecount = (mapcount - i + 1);
        wide2uni = (_cups_wide2uni_t *)
          calloc(vmap->widecount, sizeof(_cups_wide2uni_t));
        if (wide2uni == NULL)
        {
          cupsFileClose(fp);
          cupsCharmapFlush();
          return (NULL);
        }
        vmap->wide2uni = wide2uni;
      }
      wide2uni->widechar = (cups_vbcs_t) legchar;
      wide2uni->unichar = (cups_ucs2_t)unichar;
      wide2uni ++;
    }

   /*
    * Save Unicode to legacy mapping in indirect lookup table...
    */
    vrow = vmap->uni2char[(int) ((unichar >> 8) & 0xff)];
    if (vrow == NULL)
    {
      vrow = (cups_vbcs_t *) calloc(256, sizeof(cups_vbcs_t));
      if (vrow == NULL)
      {
        cupsFileClose(fp);
        cupsCharmapFlush();
        return (NULL);
      }
      vmap->uni2char[(int) ((unichar >> 8) & 0xff)] = vrow;
    }
    vrow += (int) (unichar & 0xff);

   /*
    * Convert Replacement Character to visible replacement...
    */
    if (unichar == 0xfffd)
      legchar = (unsigned long) '?';

   /*
    * First (oldest) legacy character uses Unicode mapping cell...
    */
    if (*vrow == 0)
      *vrow = (cups_vbcs_t) legchar;
  }
  vmap->charcount = (i - vmap->widecount);
  cupsFileClose(fp);
  return (vmap);
}

/*
 * 'conv_utf8_to_sbcs()' - Convert UTF-8 to legacy SBCS.
 */
static int                              /* O - Count or -1 on error */
conv_utf8_to_sbcs(char *dest,           /* O - Target string */
    const cups_utf8_t *src,             /* I - Source string */
    const int maxout,                   /* I - Max output */
    const cups_encoding_t encoding)     /* I - Encoding */
{
  char          *start = dest;  /* Start of destination string */
  _cups_cmap_t   *cmap;          /* Legacy SBCS / Unicode Charset Map */
  cups_sbcs_t   *srow;          /* Pointer to SBCS row in 'uni2char' */
  cups_utf32_t  unichar;        /* Character value */
  int           worklen;        /* Internal UCS-4 string length */
  cups_utf32_t  work[CUPS_MAX_USTRING];
                                /* Internal UCS-4 string */
  int           i;              /* Looping variable */

 /*
  * Check for valid arguments and clear output...
  */
  if ((dest == NULL)
  || (src == NULL)
  || (maxout < 1)
  || (maxout > CUPS_MAX_USTRING)
  || (encoding == CUPS_UTF8))
    return (-1);
  *dest = '\0';

 /*
  * Find legacy charset map in cache...
  */
  cmap = (_cups_cmap_t *) cupsCharmapGet(encoding);
  if (cmap == NULL)
    return (-1);

 /*
  * Convert input UTF-8 to internal UCS-4 (and insert BOM)...
  */
  worklen = cupsUTF8ToUTF32(work, src, CUPS_MAX_USTRING);
  if (worklen < 0)
    return (-1);

 /*
  * Convert internal UCS-4 to SBCS legacy charset (and delete BOM)...
  */
  for (i = 0; i < worklen;)
  {
    unichar = work[i];
    if (unichar == 0)
      break;
    i ++;

   /*
    * Check for leading BOM (and delete from output)...
    */
    if ((i == 1) && (unichar == 0xfeff))
      continue;

   /*
    * Convert ASCII verbatim (optimization)...
    */
    if (unichar <= 0x7f)
    {
      *dest = (char) unichar;
      dest ++;
      continue;
    }

   /*
    * Convert unknown character to visible replacement...
    */
    srow = cmap->uni2char[(int) ((unichar >> 8) & 0xff)];
    if (srow)
      srow += (int) (unichar & 0xff);
    if ((srow == NULL) || (*srow == 0))
      *dest = '?';
    else
      *dest = (char) (*srow);
    dest ++;
  }
  *dest = '\0';
  worklen = (int) (dest - start);
  cupsCharmapFree(encoding);
  return (worklen);
}

/*
 * 'conv_utf8_to_vbcs()' - Convert UTF-8 to legacy DBCS/VBCS.
 */
static int                              /* O - Count or -1 on error */
conv_utf8_to_vbcs(char *dest,           /* O - Target string */
    const cups_utf8_t *src,             /* I - Source string */
    const int maxout,                   /* I - Max output */
    const cups_encoding_t encoding)     /* I - Encoding */
{
  char          *start = dest;  /* Start of destination string */
  _cups_vmap_t   *vmap;          /* Legacy DBCS / Unicode Charset Map */
  cups_vbcs_t   *vrow;          /* Pointer to VBCS row in 'uni2char' */
  cups_utf32_t  unichar;        /* Character value */
  cups_vbcs_t   legchar;        /* Legacy character value */
  int           worklen;        /* Internal UCS-4 string length */
  cups_utf32_t  work[CUPS_MAX_USTRING];
                                /* Internal UCS-4 string */
  int           i;              /* Looping variable */

 /*
  * Check for valid arguments and clear output...
  */
  if ((dest == NULL)
  || (src == NULL)
  || (maxout < 1)
  || (maxout > CUPS_MAX_USTRING)
  || (encoding == CUPS_UTF8))
    return (-1);
  *dest = '\0';

 /*
  * Find legacy charset map in cache...
  */
  vmap = (_cups_vmap_t *) cupsCharmapGet(encoding);
  if (vmap == NULL)
    return (-1);

 /*
  * Convert input UTF-8 to internal UCS-4 (and insert BOM)...
  */
  worklen = cupsUTF8ToUTF32(work, src, CUPS_MAX_USTRING);
  if (worklen < 0)
    return (-1);

 /*
  * Convert internal UCS-4 to VBCS legacy charset (and delete BOM)...
  */
  for (i = 0; i < worklen;)
  {
    unichar = work[i];
    if (unichar == 0)
      break;
    i ++;

   /*
    * Check for leading BOM (and delete from output)...
    */
    if ((i == 1) && (unichar == 0xfeff))
      continue;

   /*
    * Convert ASCII verbatim (optimization)...
    */
    if (unichar <= 0x7f)
    {
      *dest = (char) unichar;
      dest ++;
      continue;
    }

   /*
    * Convert unknown character to visible replacement...
    */
    vrow = vmap->uni2char[(int) ((unichar >> 8) & 0xff)];
    if (vrow)
      vrow += (int) (unichar & 0xff);
    if ((vrow == NULL) || (*vrow == 0))
      legchar = (cups_vbcs_t) '?';
    else
      legchar = (cups_vbcs_t) *vrow;

   /*
    * Save n-byte legacy character...
    */
    if (legchar > 0xffffff)
    {
      *dest = (char) ((legchar >> 24) & 0xff);
      dest++;
    }
    if (legchar > 0xffff)
    {
      *dest = (char) ((legchar >> 16) & 0xff);
      dest++;
    }
    if (legchar > 0xff)
    {
      *dest = (char) ((legchar >> 8) & 0xff);
      dest++;
    }
    *dest = (char) (legchar & 0xff);
    dest ++;
  }
  *dest = '\0';
  worklen = (int) (dest - start);
  cupsCharmapFree(encoding);
  return (worklen);
}

/*
 * 'conv_sbcs_to_utf8()' - Convert legacy SBCS to UTF-8.
 */
static int                              /* O - Count or -1 on error */
conv_sbcs_to_utf8(cups_utf8_t *dest,    /* O - Target string */
    const char *src,                    /* I - Source string */
    const int maxout,                   /* I - Max output */
    const cups_encoding_t encoding)     /* I - Encoding */
{
  _cups_cmap_t   *cmap;          /* Legacy SBCS / Unicode Charset Map */
  cups_ucs2_t   *crow;          /* Pointer to UCS-2 row in 'char2uni' */
  unsigned long legchar;        /* Legacy character value */
  cups_utf32_t  unichar;        /* Unicode character value */
  int           worklen;        /* Internal UCS-4 string length */
  cups_utf32_t  work[CUPS_MAX_USTRING];
                                /* Internal UCS-4 string */
  int           i;              /* Looping variable */

 /*
  * Check for valid arguments and clear output...
  */
  if ((dest == NULL)
  || (src == NULL)
  || (maxout < 1)
  || (maxout > CUPS_MAX_USTRING)
  || (encoding == CUPS_UTF8))
    return (-1);
  *dest = '\0';

 /*
  * Find legacy charset map in cache...
  */
  cmap = (_cups_cmap_t *) cupsCharmapGet(encoding);
  if (cmap == NULL)
    return (-1);

 /*
  * Convert input legacy charset to internal UCS-4 (and insert BOM)...
  */
  work[0] = 0xfeff;
  for (i = 1; i < (CUPS_MAX_USTRING - 1); src ++)
  {
    if (*src == '\0')
      break;
    legchar = (unsigned long) *src;

   /*
    * Convert ASCII verbatim (optimization)...
    */
    if (legchar <= 0x7f)
    {
      work[i] = (cups_utf32_t) legchar;
      i ++;
      continue;
    }

   /*
    * Convert unknown character to Replacement Character...
    */
    crow = &cmap->char2uni[0];
    crow += (int) legchar;
    if (*crow == 0)
      unichar = 0xfffd;
    else
      unichar = (cups_utf32_t) *crow;
    work[i] = unichar;
    i ++;
  }
  work[i] = 0;

 /*
  * Convert internal UCS-4 to output UTF-8 (and delete BOM)...
  */
  worklen = cupsUTF32ToUTF8(dest, work, maxout);
  cupsCharmapFree(encoding);
  return (worklen);
}


/*
 * 'conv_vbcs_to_utf8()' - Convert legacy DBCS/VBCS to UTF-8.
 */
static int                              /* O - Count or -1 on error */
conv_vbcs_to_utf8(cups_utf8_t *dest,    /* O - Target string */
    const char *src,                    /* I - Source string */
    const int maxout,                   /* I - Max output */
    const cups_encoding_t encoding)     /* I - Encoding */
{
  _cups_vmap_t       *vmap;      /* Legacy VBCS / Unicode Charset Map */
  cups_ucs2_t       *crow;      /* Pointer to UCS-2 row in 'char2uni' */
  _cups_wide2uni_t   *wide2uni;  /* Pointer to row in 'wide2uni' */
  cups_sbcs_t       leadchar;   /* Lead char of n-byte legacy char */
  cups_vbcs_t       legchar;    /* Legacy character value */
  cups_utf32_t      unichar;    /* Unicode character value */
  int               i;          /* Looping variable */
  int               worklen;    /* Internal UCS-4 string length */
  cups_utf32_t      work[CUPS_MAX_USTRING];
                                /* Internal UCS-4 string */

 /*
  * Check for valid arguments and clear output...
  */
  if ((dest == NULL)
  || (src == NULL)
  || (maxout < 1)
  || (maxout > CUPS_MAX_USTRING)
  || (encoding == CUPS_UTF8))
    return (-1);
  *dest = '\0';

 /*
  * Find legacy charset map in cache...
  */
  vmap = (_cups_vmap_t *) cupsCharmapGet(encoding);
  if (vmap == NULL)
    return (-1);

 /*
  * Convert input legacy charset to internal UCS-4 (and insert BOM)...
  */
  work[0] = 0xfeff;
  for (i = 1; i < (CUPS_MAX_USTRING - 1); src ++)
  {
    if (*src == '\0')
      break;
    legchar = (cups_vbcs_t) *src;
    leadchar = (cups_sbcs_t) *src;

   /*
    * Convert ASCII verbatim (optimization)...
    */
    if (legchar <= 0x7f)
    {
      work[i] = (cups_utf32_t) legchar;
      i ++;
      continue;
    }

   /*
    * Convert 2-byte legacy character...
    */
    if (vmap->lead2char[(int) leadchar] == leadchar)
    {
      src ++;
      if (*src == '\0')
        return (-1);
      legchar = (legchar << 8) | (cups_vbcs_t) *src;
  
     /*
      * Convert unknown character to Replacement Character...
      */
      crow = vmap->char2uni[(int) ((legchar >> 8) & 0xff)];
      if (crow)
        crow += (int) (legchar & 0xff);
      if ((crow == NULL) || (*crow == 0))
        unichar = 0xfffd;
      else
        unichar = (cups_utf32_t) *crow;
      work[i] = unichar;
      i ++;
      continue;
    }

   /*
    * Fetch 3-byte or 4-byte legacy character...
    */
    if (vmap->lead3char[(int) leadchar] == leadchar)
    {
      src ++;
      if (*src == '\0')
        return (-1);
      legchar = (legchar << 8) | (cups_vbcs_t) *src;
      src ++;
      if (*src == '\0')
        return (-1);
      legchar = (legchar << 8) | (cups_vbcs_t) *src;
    }
    else if (vmap->lead4char[(int) leadchar] == leadchar)
    {
      src ++;
      if (*src == '\0')
        return (-1);
      legchar = (legchar << 8) | (cups_vbcs_t) *src;
      src ++;
      if (*src == '\0')
        return (-1);
      legchar = (legchar << 8) | (cups_vbcs_t) *src;
      src ++;
      if (*src == '\0')
        return (-1);
      legchar = (legchar << 8) | (cups_vbcs_t) *src;
    }
    else
      return (-1);

   /*
    * Find 3-byte or 4-byte legacy character...
    */
    wide2uni = vmap->wide2uni;
    wide2uni = (_cups_wide2uni_t *) bsearch(&legchar,
                                           vmap->wide2uni,
                                           vmap->widecount,
                                           sizeof(_cups_wide2uni_t),
                                           compare_wide);

   /*
    * Convert unknown character to Replacement Character...
    */
    if ((wide2uni == NULL) || (wide2uni->unichar == 0))
      unichar = 0xfffd;
    else
      unichar = wide2uni->unichar;
    work[i] = unichar;
    i ++;
  }
  work[i] = 0;

 /*
  * Convert internal UCS-4 to output UTF-8 (and delete BOM)...
  */
  worklen = cupsUTF32ToUTF8(dest, work, maxout);
  cupsCharmapFree(encoding);
  return (worklen);
}

/*
 * 'compare_wide()' - Compare key for wide (VBCS) match.
 */
static int
compare_wide(const void *k1,            /* I - Key char */
    const void *k2)                     /* I - Map char */
{
  cups_vbcs_t       *kp = (cups_vbcs_t *) k1;
                                /* Key char pointer */
  _cups_wide2uni_t   *mp = (_cups_wide2uni_t *) k2;
                                /* Map char pointer */
  cups_vbcs_t       key;        /* Legacy key character */
  cups_vbcs_t       map;        /* Legacy map character */
  int               result;     /* Result Value */

  key = *kp;
  map = mp->widechar;
  if (key >= map)
    result = (int) (key - map);
  else
    result = -1 * ((int) (map - key));
  return (result);
}

/*
 * End of "$Id$"
 */
