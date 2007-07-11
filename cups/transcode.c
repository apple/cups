/*
 * "$Id$"
 *
 *   Transcoding support for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   _cupsCharmapFlush() - Flush all character set maps out of cache.
 *   _cupsCharmapFree()  - Free a character set map.
 *   _cupsCharmapGet()   - Get a character set map.
 *   cupsCharsetToUTF8() - Convert legacy character set to UTF-8.
 *   cupsUTF8ToCharset() - Convert UTF-8 to legacy character set.
 *   cupsUTF8ToUTF32()   - Convert UTF-8 to UTF-32.
 *   cupsUTF32ToUTF8()   - Convert UTF-32 to UTF-8.
 *   compare_wide()      - Compare key for wide (VBCS) match.
 *   conv_sbcs_to_utf8() - Convert legacy SBCS to UTF-8.
 *   conv_utf8_to_sbcs() - Convert UTF-8 to legacy SBCS.
 *   conv_utf8_to_vbcs() - Convert UTF-8 to legacy DBCS/VBCS.
 *   conv_vbcs_to_utf8() - Convert legacy DBCS/VBCS to UTF-8.
 *   free_sbcs_charmap() - Free memory used by a single byte character set.
 *   free_vbcs_charmap() - Free memory used by a variable byte character set.
 *   get_charmap()       - Lookup or get a character set map (private).
 *   get_charmap_count() - Count lines in a charmap file.
 *   get_sbcs_charmap()  - Get SBCS Charmap.
 *   get_vbcs_charmap()  - Get DBCS/VBCS Charmap.
 */

/*
 * Include necessary headers...
 */

#include "globals.h"
#include "debug.h"
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>


/*
 * Local globals...
 */

#ifdef HAVE_PTHREAD_H
static pthread_mutex_t	map_mutex = PTHREAD_MUTEX_INITIALIZER;
					/* Mutex to control access to maps */
#endif /* HAVE_PTHREAD_H */
static _cups_cmap_t	*cmap_cache = NULL;
					/* SBCS Charmap Cache */
static _cups_vmap_t	*vmap_cache = NULL;
					/* VBCS Charmap Cache */


/*
 * Local functions...
 */

static int		compare_wide(const void *k1, const void *k2);
static int		conv_sbcs_to_utf8(cups_utf8_t *dest,
					  const cups_sbcs_t *src,
					  int maxout,
					  const cups_encoding_t encoding);
static int		conv_utf8_to_sbcs(cups_sbcs_t *dest,
					  const cups_utf8_t *src,
					  int maxout,
					  const cups_encoding_t encoding);
static int		conv_utf8_to_vbcs(cups_sbcs_t *dest,
					  const cups_utf8_t *src,
					  int maxout,
					  const cups_encoding_t encoding);
static int		conv_vbcs_to_utf8(cups_utf8_t *dest,
					  const cups_sbcs_t *src,
					  int maxout,
					  const cups_encoding_t encoding);
static void		free_sbcs_charmap(_cups_cmap_t *sbcs);
static void		free_vbcs_charmap(_cups_vmap_t *vbcs);
static void		*get_charmap(const cups_encoding_t encoding);
static int		get_charmap_count(cups_file_t *fp);
static _cups_cmap_t	*get_sbcs_charmap(const cups_encoding_t encoding,
				          const char *filename);
static _cups_vmap_t	*get_vbcs_charmap(const cups_encoding_t encoding,
				          const char *filename);


/*
 * '_cupsCharmapFlush()' - Flush all character set maps out of cache.
 */

void
_cupsCharmapFlush(void)
{
  _cups_cmap_t	*cmap,			/* Legacy SBCS / Unicode Charset Map */
		*cnext;			/* Next Legacy SBCS Charset Map */
  _cups_vmap_t	*vmap,			/* Legacy VBCS / Unicode Charset Map */
		*vnext;			/* Next Legacy VBCS Charset Map */


#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock(&map_mutex);
#endif /* HAVE_PTHREAD_H */

 /*
  * Loop through SBCS charset map cache, free all memory...
  */

  for (cmap = cmap_cache; cmap; cmap = cnext)
  {
    cnext = cmap->next;

    free_sbcs_charmap(cmap);
  }

  cmap_cache = NULL;

 /*
  * Loop through DBCS/VBCS charset map cache, free all memory...
  */

  for (vmap = vmap_cache; vmap; vmap = vnext)
  {
    vnext = vmap->next;

    free_vbcs_charmap(vmap);

    free(vmap);
  }

  vmap_cache = NULL;

#ifdef HAVE_PTHREAD_H
  pthread_mutex_unlock(&map_mutex);
#endif /* HAVE_PTHREAD_H */
}


/*
 * '_cupsCharmapFree()' - Free a character set map.
 *
 * This does not actually free; use '_cupsCharmapFlush()' for that.
 */

void
_cupsCharmapFree(
    const cups_encoding_t encoding)	/* I - Encoding */
{
  _cups_cmap_t	*cmap;			/* Legacy SBCS / Unicode Charset Map */
  _cups_vmap_t	*vmap;			/* Legacy VBCS / Unicode Charset Map */


 /*
  * See if we already have this SBCS charset map loaded...
  */

#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock(&map_mutex);
#endif /* HAVE_PTHREAD_H */

  for (cmap = cmap_cache; cmap; cmap = cmap->next)
  {
    if (cmap->encoding == encoding)
    {
      if (cmap->used > 0)
	cmap->used --;
      break;
    }
  }

 /*
  * See if we already have this DBCS/VBCS charset map loaded...
  */

  for (vmap = vmap_cache; vmap; vmap = vmap->next)
  {
    if (vmap->encoding == encoding)
    {
      if (vmap->used > 0)
	vmap->used --;
      break;
    }
  }

#ifdef HAVE_PTHREAD_H
  pthread_mutex_unlock(&map_mutex);
#endif /* HAVE_PTHREAD_H */
}


/*
 * '_cupsCharmapGet()' - Get a character set map.
 *
 * This code handles single-byte (SBCS), double-byte (DBCS), and
 * variable-byte (VBCS) character sets _without_ charset escapes...
 * This code does not handle multiple-byte character sets (MBCS)
 * (such as ISO-2022-JP) with charset switching via escapes...
 */

void *					/* O - Charset map pointer */
_cupsCharmapGet(
    const cups_encoding_t encoding)	/* I - Encoding */
{
  void	*charmap;			/* Charset map pointer */


  DEBUG_printf(("_cupsCharmapGet(encoding=%d)\n", encoding));

 /*
  * Check for valid arguments...
  */

  if (encoding < 0 || encoding >= CUPS_ENCODING_VBCS_END)
  {
    DEBUG_puts("    Bad encoding, returning NULL!");
    return (NULL);
  }

 /*
  * Lookup or get the charset map pointer and return...
  */

#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock(&map_mutex);
#endif /* HAVE_PTHREAD_H */

  charmap = get_charmap(encoding);

#ifdef HAVE_PTHREAD_H
  pthread_mutex_unlock(&map_mutex);
#endif /* HAVE_PTHREAD_H */

  return (charmap);
}


/*
 * 'cupsCharsetToUTF8()' - Convert legacy character set to UTF-8.
 *
 * This code handles single-byte (SBCS), double-byte (DBCS), and
 * variable-byte (VBCS) character sets _without_ charset escapes...
 * This code does not handle multiple-byte character sets (MBCS)
 * (such as ISO-2022-JP) with charset switching via escapes...
 */

int					/* O - Count or -1 on error */
cupsCharsetToUTF8(
    cups_utf8_t *dest,			/* O - Target string */
    const char *src,			/* I - Source string */
    const int maxout,			/* I - Max output */
    const cups_encoding_t encoding)	/* I - Encoding */
{
  int	bytes;				/* Number of bytes converted */


 /*
  * Check for valid arguments...
  */

  DEBUG_printf(("cupsCharsetToUTF8(dest=%p, src=\"%s\", maxout=%d, encoding=%d)\n",
	        dest, src, maxout, encoding));

  if (dest)
    *dest = '\0';

  if (!dest || !src || maxout < 1 || maxout > CUPS_MAX_USTRING)
  {
    DEBUG_puts("    Bad arguments, returning -1");
    return (-1);
  }

 /*
  * Handle identity conversions...
  */

  if (encoding == CUPS_UTF8 ||
      encoding < 0 || encoding >= CUPS_ENCODING_VBCS_END)
  {
    strlcpy((char *)dest, src, maxout);
    return ((int)strlen((char *)dest));
  }

 /*
  * Handle ISO-8859-1 to UTF-8 directly...
  */

  if (encoding == CUPS_ISO8859_1)
  {
    int		ch;			/* Character from string */
    cups_utf8_t	*destptr,		/* Pointer into UTF-8 buffer */
		*destend;		/* End of UTF-8 buffer */


    destptr = dest;
    destend = dest + maxout - 2;

    while (*src && destptr < destend)
    {
      ch = *src++ & 255;

      if (ch & 128)
      {
	*destptr++ = 0xc0 | (ch >> 6);
	*destptr++ = 0x80 | (ch & 0x3f);
      }
      else
	*destptr++ = ch;
    }

    *destptr = '\0';

    return ((int)(destptr - dest));
  }

 /*
  * Convert input legacy charset to UTF-8...
  */

#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock(&map_mutex);
#endif /* HAVE_PTHREAD_H */

  if (encoding < CUPS_ENCODING_SBCS_END)
    bytes = conv_sbcs_to_utf8(dest, (cups_sbcs_t *)src, maxout, encoding);
  else if (encoding < CUPS_ENCODING_VBCS_END)
    bytes = conv_vbcs_to_utf8(dest, (cups_sbcs_t *)src, maxout, encoding);
  else
  {
    DEBUG_puts("    Bad encoding, returning -1");
    bytes = -1;
  }

#ifdef HAVE_PTHREAD_H
  pthread_mutex_unlock(&map_mutex);
#endif /* HAVE_PTHREAD_H */

  return (bytes);
}


/*
 * 'cupsUTF8ToCharset()' - Convert UTF-8 to legacy character set.
 *
 * This code handles single-byte (SBCS), double-byte (DBCS), and
 * variable-byte (VBCS) character sets _without_ charset escapes...
 * This code does not handle multiple-byte character sets (MBCS)
 * (such as ISO-2022-JP) with charset switching via escapes...
 */

int					/* O - Count or -1 on error */
cupsUTF8ToCharset(
    char		  *dest,	/* O - Target string */
    const cups_utf8_t	  *src,		/* I - Source string */
    const int		  maxout,	/* I - Max output */
    const cups_encoding_t encoding)	/* I - Encoding */
{
  int	bytes;				/* Number of bytes converted */


 /*
  * Check for valid arguments...
  */

  if (!dest || !src || maxout < 1 || maxout > CUPS_MAX_USTRING)
  {
    if (dest)
      *dest = '\0';

    return (-1);
  }

 /*
  * Handle identity conversions...
  */

  if (encoding == CUPS_UTF8 ||
      encoding < 0 || encoding >= CUPS_ENCODING_VBCS_END)
  {
    strlcpy(dest, (char *)src, maxout);
    return ((int)strlen(dest));
  }

 /*
  * Handle UTF-8 to ISO-8859-1 directly...
  */

  if (encoding == CUPS_ISO8859_1)
  {
    int		ch;			/* Character from string */
    char	*destptr,		/* Pointer into ISO-8859-1 buffer */
		*destend;		/* End of ISO-8859-1 buffer */


    destptr = dest;
    destend = dest + maxout - 1;

    while (*src && destptr < destend)
    {
      ch = *src++;

      if ((ch & 0xe0) == 0xc0)
      {
	ch = ((ch & 0x1f) << 6) | (*src++ & 0x3f);

	if (ch < 256)
          *destptr++ = ch;
	else
          *destptr++ = '?';
      }
      else if ((ch & 0xf0) == 0xe0 ||
               (ch & 0xf8) == 0xf0)
        *destptr++ = '?';
      else if (!(ch & 0x80))
	*destptr++ = ch;
    }

    *destptr = '\0';

    return ((int)(destptr - dest));
  }

 /*
  * Convert input UTF-8 to legacy charset...
  */

#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock(&map_mutex);
#endif /* HAVE_PTHREAD_H */

  if (encoding < CUPS_ENCODING_SBCS_END)
    bytes = conv_utf8_to_sbcs((cups_sbcs_t *)dest, src, maxout, encoding);
  else if (encoding < CUPS_ENCODING_VBCS_END)
    bytes = conv_utf8_to_vbcs((cups_sbcs_t *)dest, src, maxout, encoding);
  else
    bytes = -1;

#ifdef HAVE_PTHREAD_H
  pthread_mutex_unlock(&map_mutex);
#endif /* HAVE_PTHREAD_H */

  return (bytes);
}


/*
 * 'cupsUTF8ToUTF32()' - Convert UTF-8 to UTF-32.
 *
 * 32-bit UTF-32 (actually 21-bit) maps to UTF-8 as follows...
 *
 *   UTF-32 char     UTF-8 char(s)
 *   --------------------------------------------------
 *	  0 to 127 = 0xxxxxxx (US-ASCII)
 *     128 to 2047 = 110xxxxx 10yyyyyy
 *   2048 to 65535 = 1110xxxx 10yyyyyy 10zzzzzz
 *	   > 65535 = 11110xxx 10yyyyyy 10zzzzzz 10xxxxxx
 *
 * UTF-32 prohibits chars beyond Plane 16 (> 0x10ffff) in UCS-4,
 * which would convert to five- or six-octet UTF-8 sequences...
 */

int					/* O - Count or -1 on error */
cupsUTF8ToUTF32(
    cups_utf32_t      *dest,		/* O - Target string */
    const cups_utf8_t *src,		/* I - Source string */
    const int         maxout)		/* I - Max output */
{
  int		i;			/* Looping variable */
  cups_utf8_t	ch;			/* Character value */
  cups_utf8_t	next;			/* Next character value */
  cups_utf32_t	ch32;			/* UTF-32 character value */


 /*
  * Check for valid arguments and clear output...
  */

  if (dest)
    *dest = 0;

  if (!dest || !src || maxout < 1 || maxout > CUPS_MAX_USTRING)
    return (-1);

 /*
  * Convert input UTF-8 to output UTF-32 (and insert BOM)...
  */

  *dest++ = 0xfeff;

  for (i = maxout - 1; *src && i > 0; i --)
  {
    ch = *src++;

   /*
    * Convert UTF-8 character(s) to UTF-32 character...
    */

    if (!(ch & 0x80))
    {
     /*
      * One-octet UTF-8 <= 127 (US-ASCII)...
      */

      *dest++ = ch;
      continue;
    }
    else if ((ch & 0xe0) == 0xc0)
    {
     /*
      * Two-octet UTF-8 <= 2047 (Latin-x)...
      */

      next = *src++;
      if (!next)
	return (-1);

      ch32 = ((ch & 0x1f) << 6) | (next & 0x3f);

     /*
      * Check for non-shortest form (invalid UTF-8)...
      */

      if (ch32 < 0x80)
	return (-1);

      *dest++ = ch32;
    }
    else if ((ch & 0xf0) == 0xe0)
    {
     /*
      * Three-octet UTF-8 <= 65535 (Plane 0 - BMP)...
      */

      next = *src++;
      if (!next)
	return (-1);

      ch32 = ((ch & 0x0f) << 6) | (next & 0x3f);

      next = *src++;
      if (!next)
	return (-1);

      ch32 = (ch32 << 6) | (next & 0x3f);

     /*
      * Check for non-shortest form (invalid UTF-8)...
      */

      if (ch32 < 0x800)
	return (-1);

      *dest++ = ch32;
    }
    else if ((ch & 0xf8) == 0xf0)
    {
     /*
      * Four-octet UTF-8...
      */

      next = *src++;
      if (!next)
	return (-1);

      ch32 = ((ch & 0x07) << 6) | (next & 0x3f);

      next = *src++;
      if (!next)
	return (-1);

      ch32 = (ch32 << 6) | (next & 0x3f);

      next = *src++;
      if (!next)
	return (-1);

      ch32 = (ch32 << 6) | (next & 0x3f);

     /*
      * Check for non-shortest form (invalid UTF-8)...
      */

      if (ch32 < 0x10000)
	return (-1);

      *dest++ = ch32;
    }
    else
    {
     /*
      * More than 4-octet (invalid UTF-8 sequence)...
      */

      return (-1);
    }

   /*
    * Check for UTF-16 surrogate (illegal UTF-8)...
    */

    if (ch32 >= 0xd800 && ch32 <= 0xdfff)
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
 *	  0 to 127 = 0xxxxxxx (US-ASCII)
 *     128 to 2047 = 110xxxxx 10yyyyyy
 *   2048 to 65535 = 1110xxxx 10yyyyyy 10zzzzzz
 *	   > 65535 = 11110xxx 10yyyyyy 10zzzzzz 10xxxxxx
 *
 * UTF-32 prohibits chars beyond Plane 16 (> 0x10ffff) in UCS-4,
 * which would convert to five- or six-octet UTF-8 sequences...
 */

int					/* O - Count or -1 on error */
cupsUTF32ToUTF8(
    cups_utf8_t        *dest,		/* O - Target string */
    const cups_utf32_t *src,		/* I - Source string */
    const int          maxout)		/* I - Max output */
{
  cups_utf8_t	*start;			/* Start of destination string */
  int		i;			/* Looping variable */
  int		swap;			/* Byte-swap input to output */
  cups_utf32_t	ch;			/* Character value */


 /*
  * Check for valid arguments and clear output...
  */

  if (dest)
    *dest = '\0';

  if (!dest || !src || maxout < 1)
    return (-1);

 /*
  * Check for leading BOM in UTF-32 and inverted BOM...
  */

  start = dest;
  swap  = *src == 0xfffe0000;

  if (*src == 0xfffe0000 || *src == 0xfeff)
    src ++;

 /*
  * Convert input UTF-32 to output UTF-8...
  */

  for (i = maxout - 1; *src && i > 0;)
  {
    ch = *src++;

   /*
    * Byte swap input UTF-32, if necessary...
    * (only byte-swapping 24 of 32 bits)
    */

    if (swap)
      ch = ((ch >> 24) | ((ch >> 8) & 0xff00) | ((ch << 8) & 0xff0000));

   /*
    * Check for beyond Plane 16 (invalid UTF-32)...
    */

    if (ch > 0x10ffff)
      return (-1);

   /*
    * Convert UTF-32 character to UTF-8 character(s)...
    */

    if (ch < 0x80)
    {
     /*
      * One-octet UTF-8 <= 127 (US-ASCII)...
      */

      *dest++ = (cups_utf8_t)ch;
      i --;
    }
    else if (ch < 0x800)
    {
     /*
      * Two-octet UTF-8 <= 2047 (Latin-x)...
      */

      if (i < 2)
        return (-1);

      *dest++ = (cups_utf8_t)(0xc0 | ((ch >> 6) & 0x1f));
      *dest++ = (cups_utf8_t)(0x80 | (ch & 0x3f));
      i -= 2;
    }
    else if (ch < 0x10000)
    {
     /*
      * Three-octet UTF-8 <= 65535 (Plane 0 - BMP)...
      */

      if (i < 3)
        return (-1);

      *dest++ = (cups_utf8_t)(0xe0 | ((ch >> 12) & 0x0f));
      *dest++ = (cups_utf8_t)(0x80 | ((ch >> 6) & 0x3f));
      *dest++ = (cups_utf8_t)(0x80 | (ch & 0x3f));
      i -= 3;
    }
    else
    {
     /*
      * Four-octet UTF-8...
      */

      if (i < 4)
        return (-1);

      *dest++ = (cups_utf8_t)(0xf0 | ((ch >> 18) & 0x07));
      *dest++ = (cups_utf8_t)(0x80 | ((ch >> 12) & 0x3f));
      *dest++ = (cups_utf8_t)(0x80 | ((ch >> 6) & 0x3f));
      *dest++ = (cups_utf8_t)(0x80 | (ch & 0x3f));
      i -= 4;
    }
  }

  *dest = '\0';

  return ((int)(dest - start));
}


/*
 * 'compare_wide()' - Compare key for wide (VBCS) match.
 */

static int
compare_wide(const void *k1,		/* I - Key char */
             const void *k2)		/* I - Map char */
{
  cups_vbcs_t	key;			/* Legacy key character */
  cups_vbcs_t	map;			/* Legacy map character */


  key = *((cups_vbcs_t *)k1);
  map = ((_cups_wide2uni_t *)k2)->widechar;

  return ((int)(key - map));
}


/*
 * 'conv_sbcs_to_utf8()' - Convert legacy SBCS to UTF-8.
 */

static int				/* O - Count or -1 on error */
conv_sbcs_to_utf8(
    cups_utf8_t           *dest,	/* O - Target string */
    const cups_sbcs_t     *src,		/* I - Source string */
    int                   maxout,	/* I - Max output */
    const cups_encoding_t encoding)	/* I - Encoding */
{
  _cups_cmap_t	*cmap;			/* Legacy SBCS / Unicode Charset Map */
  cups_ucs2_t	*crow;			/* Pointer to UCS-2 row in 'char2uni' */
  cups_sbcs_t	legchar;		/* Legacy character value */
  cups_utf32_t	work[CUPS_MAX_USTRING],	/* Internal UCS-4 string */
		*workptr;		/* Pointer into string */


 /*
  * Find legacy charset map in cache...
  */

  if ((cmap = (_cups_cmap_t *)get_charmap(encoding)) == NULL)
    return (-1);

 /*
  * Convert input legacy charset to internal UCS-4 (and insert BOM)...
  */

  work[0] = 0xfeff;
  for (workptr = work + 1; *src && workptr < (work + CUPS_MAX_USTRING - 1);)
  {
    legchar = *src++;

   /*
    * Convert ASCII verbatim (optimization)...
    */

    if (legchar < 0x80)
      *workptr++ = (cups_utf32_t)legchar;
    else
    {
     /*
      * Convert unknown character to Replacement Character...
      */

      crow = cmap->char2uni + legchar;

      if (!*crow)
	*workptr++ = 0xfffd;
      else
	*workptr++ = (cups_utf32_t)*crow;
    }
  }

  *workptr = 0;

 /*
  * Convert internal UCS-4 to output UTF-8 (and delete BOM)...
  */

  cmap->used --;

  return (cupsUTF32ToUTF8(dest, work, maxout));
}


/*
 * 'conv_utf8_to_sbcs()' - Convert UTF-8 to legacy SBCS.
 */

static int				/* O - Count or -1 on error */
conv_utf8_to_sbcs(
    cups_sbcs_t           *dest,	/* O - Target string */
    const cups_utf8_t     *src,		/* I - Source string */
    int                   maxout,	/* I - Max output */
    const cups_encoding_t encoding)	/* I - Encoding */
{
  cups_sbcs_t	*start;			/* Start of destination string */
  _cups_cmap_t	*cmap;			/* Legacy SBCS / Unicode Charset Map */
  cups_sbcs_t	*srow;			/* Pointer to SBCS row in 'uni2char' */
  cups_utf32_t	unichar;		/* Character value */
  cups_utf32_t	work[CUPS_MAX_USTRING],	/* Internal UCS-4 string */
		*workptr;		/* Pointer into string */


 /*
  * Find legacy charset map in cache...
  */

  if ((cmap = (_cups_cmap_t *)get_charmap(encoding)) == NULL)
    return (-1);

 /*
  * Convert input UTF-8 to internal UCS-4 (and insert BOM)...
  */

  if (cupsUTF8ToUTF32(work, src, CUPS_MAX_USTRING) < 0)
    return (-1);

 /*
  * Convert internal UCS-4 to SBCS legacy charset (and delete BOM)...
  */

  for (workptr = work + 1, start = dest; *workptr && maxout > 1; maxout --)
  {
    unichar = *workptr++;
    if (!unichar)
      break;

   /*
    * Convert ASCII verbatim (optimization)...
    */

    if (unichar < 0x80)
    {
      *dest++ = (cups_sbcs_t)unichar;
      continue;
    }

   /*
    * Convert unknown character to visible replacement...
    */

    srow = cmap->uni2char[(int)((unichar >> 8) & 0xff)];

    if (srow)
      srow += (int)(unichar & 0xff);

    if (!srow || !*srow)
      *dest++ = '?';
    else
      *dest++ = *srow;
  }

  *dest = '\0';

  cmap->used --;

  return ((int)(dest - start));
}


/*
 * 'conv_utf8_to_vbcs()' - Convert UTF-8 to legacy DBCS/VBCS.
 */

static int				/* O - Count or -1 on error */
conv_utf8_to_vbcs(
    cups_sbcs_t           *dest,	/* O - Target string */
    const cups_utf8_t     *src,		/* I - Source string */
    int                   maxout,	/* I - Max output */
    const cups_encoding_t encoding)	/* I - Encoding */
{
  cups_sbcs_t	*start;			/* Start of destination string */
  _cups_vmap_t	*vmap;			/* Legacy DBCS / Unicode Charset Map */
  cups_vbcs_t	*vrow;			/* Pointer to VBCS row in 'uni2char' */
  cups_utf32_t	unichar;		/* Character value */
  cups_vbcs_t	legchar;		/* Legacy character value */
  cups_utf32_t	work[CUPS_MAX_USTRING],	/* Internal UCS-4 string */
		*workptr;		/* Pointer into string */


 /*
  * Find legacy charset map in cache...
  */

  if ((vmap = (_cups_vmap_t *)get_charmap(encoding)) == NULL)
    return (-1);

 /*
  * Convert input UTF-8 to internal UCS-4 (and insert BOM)...
  */

  if (cupsUTF8ToUTF32(work, src, CUPS_MAX_USTRING) < 0)
    return (-1);

 /*
  * Convert internal UCS-4 to VBCS legacy charset (and delete BOM)...
  */

  for (start = dest, workptr = work + 1; *workptr && maxout > 1; maxout --)
  {
    unichar = *workptr++;
    if (!unichar)
      break;

   /*
    * Convert ASCII verbatim (optimization)...
    */

    if (unichar < 0x80)
    {
      *dest++ = (cups_sbcs_t)unichar;
      continue;
    }

   /*
    * Convert unknown character to visible replacement...
    */

    vrow = vmap->uni2char[(int)((unichar >> 8) & 0xff)];

    if (vrow)
      vrow += (int)(unichar & 0xff);

    if (!vrow || !*vrow)
      legchar = (cups_vbcs_t)'?';
    else
      legchar = (cups_vbcs_t)*vrow;

   /*
    * Save n-byte legacy character...
    */

    if (legchar > 0xffffff)
    {
      if (maxout < 5)
        return (-1);

      *dest++ = (cups_sbcs_t)(legchar >> 24);
      *dest++ = (cups_sbcs_t)(legchar >> 16);
      *dest++ = (cups_sbcs_t)(legchar >> 8);
      *dest++ = (cups_sbcs_t)legchar;

      maxout -= 3;
    }
    else if (legchar > 0xffff)
    {
      if (maxout < 4)
        return (-1);

      *dest++ = (cups_sbcs_t)(legchar >> 16);
      *dest++ = (cups_sbcs_t)(legchar >> 8);
      *dest++ = (cups_sbcs_t)legchar;

      maxout -= 2;
    }
    else if (legchar > 0xff)
    {
      *dest++ = (cups_sbcs_t)(legchar >> 8);
      *dest++ = (cups_sbcs_t)legchar;

      maxout --;
    }
  }

  *dest = '\0';

  vmap->used --;

  return ((int)(dest - start));
}


/*
 * 'conv_vbcs_to_utf8()' - Convert legacy DBCS/VBCS to UTF-8.
 */

static int				/* O - Count or -1 on error */
conv_vbcs_to_utf8(
    cups_utf8_t           *dest,	/* O - Target string */
    const cups_sbcs_t     *src,		/* I - Source string */
    int                   maxout,	/* I - Max output */
    const cups_encoding_t encoding)	/* I - Encoding */
{
  _cups_vmap_t	*vmap;			/* Legacy VBCS / Unicode Charset Map */
  cups_ucs2_t	*crow;			/* Pointer to UCS-2 row in 'char2uni' */
  _cups_wide2uni_t *wide2uni;		/* Pointer to row in 'wide2uni' */
  cups_sbcs_t	leadchar;		/* Lead char of n-byte legacy char */
  cups_vbcs_t	legchar;		/* Legacy character value */
  cups_utf32_t	work[CUPS_MAX_USTRING],	/* Internal UCS-4 string */
		*workptr;		/* Pointer into string */


 /*
  * Find legacy charset map in cache...
  */

  if ((vmap = (_cups_vmap_t *)get_charmap(encoding)) == NULL)
    return (-1);

 /*
  * Convert input legacy charset to internal UCS-4 (and insert BOM)...
  */

  work[0] = 0xfeff;
  for (workptr = work + 1; *src && workptr < (work + CUPS_MAX_USTRING - 1);)
  {
    legchar  = *src++;
    leadchar = (cups_sbcs_t)legchar;

   /*
    * Convert ASCII verbatim (optimization)...
    */

    if (legchar < 0x80)
    {
      *workptr++ = (cups_utf32_t)legchar;
      continue;
    }

   /*
    * Convert 2-byte legacy character...
    */

    if (vmap->lead2char[(int)leadchar] == leadchar)
    {
      if (!*src)
	return (-1);

      legchar = (legchar << 8) | *src++;
  
     /*
      * Convert unknown character to Replacement Character...
      */

      crow = vmap->char2uni[(int)((legchar >> 8) & 0xff)];
      if (crow)
	crow += (int) (legchar & 0xff);

      if (!crow || !*crow)
	*workptr++ = 0xfffd;
      else
	*workptr++ = (cups_utf32_t)*crow;
      continue;
    }

   /*
    * Fetch 3-byte or 4-byte legacy character...
    */

    if (vmap->lead3char[(int)leadchar] == leadchar)
    {
      if (!*src || !src[1])
	return (-1);

      legchar = (legchar << 8) | *src++;
      legchar = (legchar << 8) | *src++;
    }
    else if (vmap->lead4char[(int)leadchar] == leadchar)
    {
      if (!*src || !src[1] || !src[2])
	return (-1);

      legchar = (legchar << 8) | *src++;
      legchar = (legchar << 8) | *src++;
      legchar = (legchar << 8) | *src++;
    }
    else
      return (-1);

   /*
    * Find 3-byte or 4-byte legacy character...
    */

    wide2uni = (_cups_wide2uni_t *)bsearch(&legchar,
					   vmap->wide2uni,
					   vmap->widecount,
					   sizeof(_cups_wide2uni_t),
					   compare_wide);

   /*
    * Convert unknown character to Replacement Character...
    */

    if (!wide2uni || !wide2uni->unichar)
      *workptr++ = 0xfffd;
    else
      *workptr++ = wide2uni->unichar;
  }

  *workptr = 0;

  vmap->used --;

 /*
  * Convert internal UCS-4 to output UTF-8 (and delete BOM)...
  */

  return (cupsUTF32ToUTF8(dest, work, maxout));
}


/*
 * 'free_sbcs_charmap()' - Free memory used by a single byte character set.
 */

static void
free_sbcs_charmap(_cups_cmap_t *cmap)	/* I - Character set */
{
  int		i;			/* Looping variable */


  for (i = 0; i < 256; i ++)
    if (cmap->uni2char[i])
      free(cmap->uni2char[i]);

  free(cmap);
}


/*
 * 'free_vbcs_charmap()' - Free memory used by a variable byte character set.
 */

static void
free_vbcs_charmap(_cups_vmap_t *vmap)	/* I - Character set */
{
  int		i;			/* Looping variable */


  for (i = 0; i < 256; i ++)
    if (vmap->char2uni[i])
      free(vmap->char2uni[i]);

  for (i = 0; i < 256; i ++)
    if (vmap->uni2char[i])
      free(vmap->uni2char[i]);

  if (vmap->wide2uni)
    free(vmap->wide2uni);

  free(vmap);
}


/*
 * 'get_charmap()' - Lookup or get a character set map (private).
 *
 * This code handles single-byte (SBCS), double-byte (DBCS), and
 * variable-byte (VBCS) character sets _without_ charset escapes...
 * This code does not handle multiple-byte character sets (MBCS)
 * (such as ISO-2022-JP) with charset switching via escapes...
 */


static void *				/* O - Charset map pointer */
get_charmap(
    const cups_encoding_t encoding)	/* I - Encoding */
{
  char		filename[1024];		/* Filename for charset map file */
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */


 /*
  * Get the data directory and charset map name...
  */

  snprintf(filename, sizeof(filename), "%s/charmaps/%s.txt",
	   cg->cups_datadir, _cupsEncodingName(encoding));

  DEBUG_printf(("    filename=\"%s\"\n", filename));

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
 * 'get_charmap_count()' - Count lines in a charmap file.
 */

static int				/* O - Count or -1 on error */
get_charmap_count(cups_file_t *fp)	/* I - File to read from */
{
  int	count;				/* Number of lines */
  char	line[256];			/* Line from input map file */


 /*
  * Count lines in map input file...
  */

  count = 0;

  while (cupsFileGets(fp, line, sizeof(line)))
    if (line[0] == '0')
      count ++;

 /*
  * Return the number of lines...
  */

  if (count > 0)
    return (count);
  else
    return (-1);
}


/*
 * 'get_sbcs_charmap()' - Get SBCS Charmap.
 */

static _cups_cmap_t *			 /* O - Charmap or 0 on error */
get_sbcs_charmap(
    const cups_encoding_t encoding,	/* I - Charmap Encoding */
    const char            *filename)	/* I - Charmap Filename */
{
  unsigned long legchar;		/* Legacy character value */
  cups_utf32_t	unichar;		/* Unicode character value */
  _cups_cmap_t	 *cmap;			/* Legacy SBCS / Unicode Charset Map */
  cups_file_t	*fp;			/* Charset map file pointer */
  char		*s;			/* Line parsing pointer */
  cups_ucs2_t	*crow;			/* Pointer to UCS-2 row in 'char2uni' */
  cups_sbcs_t	*srow;			/* Pointer to SBCS row in 'uni2char' */
  char		line[256];		/* Line from charset map file */


 /*
  * See if we already have this SBCS charset map loaded...
  */

  for (cmap = cmap_cache; cmap; cmap = cmap->next)
  {
    if (cmap->encoding == encoding)
    {
      cmap->used ++;
      DEBUG_printf(("    returning existing cmap=%p\n", cmap));

      return ((void *)cmap);
    }
  }

 /*
  * Open SBCS charset map input file...
  */

  if ((fp = cupsFileOpen(filename, "r")) == NULL)
    return (NULL);

 /*
  * Allocate memory for SBCS charset map...
  */

  if ((cmap = (_cups_cmap_t *)calloc(1, sizeof(_cups_cmap_t))) == NULL)
  {
    cupsFileClose(fp);
    DEBUG_puts("    Unable to allocate memory!");

    return (NULL);
  }

  cmap->used ++;
  cmap->encoding = encoding;

 /*
  * Save SBCS charset map into memory for transcoding...
  */

  while (cupsFileGets(fp, line, sizeof(line)))
  {
    if (line[0] != '0')
      continue;

    legchar = strtol(line, &s, 16);
    if (legchar < 0 || legchar > 0xff)
      goto sbcs_error;

    unichar = strtol(s, NULL, 16);
    if (unichar < 0 || unichar > 0xffff)
      goto sbcs_error;

   /*
    * Save legacy to Unicode mapping in direct lookup table...
    */

    crow  = cmap->char2uni + legchar;
    *crow = (cups_ucs2_t)(unichar & 0xffff);

   /*
    * Save Unicode to legacy mapping in indirect lookup table...
    */

    srow = cmap->uni2char[(unichar >> 8) & 0xff];
    if (!srow)
    {
      srow = (cups_sbcs_t *)calloc(256, sizeof(cups_sbcs_t));
      if (!srow)
        goto sbcs_error;

      cmap->uni2char[(unichar >> 8) & 0xff] = srow;
    }

    srow += unichar & 0xff;

   /*
    * Convert Replacement Character to visible replacement...
    */

    if (unichar == 0xfffd)
      legchar = (unsigned long)'?';

   /*
    * First (oldest) legacy character uses Unicode mapping cell...
    */

    if (!*srow)
      *srow = (cups_sbcs_t)legchar;
  }

  cupsFileClose(fp);
      
 /*
  * Add it to the cache and return...
  */

  cmap->next = cmap_cache;
  cmap_cache = cmap;

  DEBUG_printf(("    returning new cmap=%p\n", cmap));

  return (cmap);

 /*
  * If we get here, there was an error in the cmap file...
  */

  sbcs_error:

  free_sbcs_charmap(cmap);

  cupsFileClose(fp);

  DEBUG_puts("    Error, returning NULL!");

  return (NULL);
}


/*
 * 'get_vbcs_charmap()' - Get DBCS/VBCS Charmap.
 */

static _cups_vmap_t *			/* O - Charmap or 0 on error */
get_vbcs_charmap(
    const cups_encoding_t encoding,	/* I - Charmap Encoding */
    const char            *filename)	/* I - Charmap Filename */
{
  _cups_vmap_t	*vmap;			/* Legacy VBCS / Unicode Charset Map */
  cups_ucs2_t	*crow;			/* Pointer to UCS-2 row in 'char2uni' */
  cups_vbcs_t	*vrow;			/* Pointer to VBCS row in 'uni2char' */
  _cups_wide2uni_t *wide2uni;		/* Pointer to row in 'wide2uni' */
  cups_sbcs_t	leadchar;		/* Lead char of 2-byte legacy char */
  unsigned long	legchar;		/* Legacy character value */
  cups_utf32_t	unichar;		/* Unicode character value */
  int		mapcount;		/* Count of lines in charmap file */
  cups_file_t	*fp;			/* Charset map file pointer */
  char		*s;			/* Line parsing pointer */
  char		line[256];		/* Line from charset map file */
  int		i;			/* Loop variable */
  int		legacy;			/* 32-bit legacy char */


  DEBUG_printf(("get_vbcs_charmap(encoding=%d, filename=\"%s\")\n",
                encoding, filename));

 /*
  * See if we already have this DBCS/VBCS charset map loaded...
  */

  for (vmap = vmap_cache; vmap; vmap = vmap->next)
  {
    if (vmap->encoding == encoding)
    {
      vmap->used ++;
      DEBUG_printf(("    returning existing vmap=%p\n", vmap));

      return ((void *)vmap);
    }
  }

 /*
  * Open VBCS charset map input file...
  */

  if ((fp = cupsFileOpen(filename, "r")) == NULL)
  {
    DEBUG_printf(("    Unable to open file: %s\n", strerror(errno)));

    return (NULL);
  }

 /*
  * Count lines in charmap file...
  */

  if ((mapcount = get_charmap_count(fp)) <= 0)
  {
    DEBUG_puts("    Unable to get charmap count!");

    return (NULL);
  }

  DEBUG_printf(("    mapcount=%d\n", mapcount));

 /*
  * Allocate memory for DBCS/VBCS charset map...
  */

  if ((vmap = (_cups_vmap_t *)calloc(1, sizeof(_cups_vmap_t))) == NULL)
  {
    cupsFileClose(fp);
    DEBUG_puts("    Unable to allocate memory!");

    return (NULL);
  }

  vmap->used ++;
  vmap->encoding = encoding;

 /*
  * Save DBCS/VBCS charset map into memory for transcoding...
  */

  leadchar = 0;
  wide2uni = NULL;

  cupsFileRewind(fp);

  i      = 0;
  legacy = 0;

  while (cupsFileGets(fp, line, sizeof(line)))
  {
    if (line[0] != '0')
      continue;

    legchar = strtoul(line, &s, 16);
    if (legchar == ULONG_MAX)
      goto vbcs_error;

    unichar = strtol(s, NULL, 16);
    if (unichar < 0 || unichar > 0xffff)
      goto vbcs_error;

    i ++;

/*    DEBUG_printf(("    i=%d, legchar=0x%08lx, unichar=0x%04x\n", i,
                  legchar, (unsigned)unichar)); */

   /*
    * Save lead char of 2/3/4-byte legacy char...
    */

    if (legchar > 0xff && legchar <= 0xffff)
    {
      leadchar                  = (cups_sbcs_t)(legchar >> 8);
      vmap->lead2char[leadchar] = leadchar;
    }

    if (legchar > 0xffff && legchar <= 0xffffff)
    {
      leadchar                  = (cups_sbcs_t)(legchar >> 16);
      vmap->lead3char[leadchar] = leadchar;
    }

    if (legchar > 0xffffff)
    {
      leadchar                  = (cups_sbcs_t)(legchar >> 24);
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

      crow = vmap->char2uni[(int)leadchar];
      if (!crow)
      {
	crow = (cups_ucs2_t *)calloc(256, sizeof(cups_ucs2_t));
	if (!crow)
          goto vbcs_error;

	vmap->char2uni[(int)leadchar] = crow;
      }

      crow[(int)(legchar & 0xff)] = (cups_ucs2_t)unichar;
    }
    else
    {
     /*
      * Save VBCS 32-bit to Unicode mapping in sorted list table...
      */

      if (!legacy)
      {
	legacy          = 1;
	vmap->widecount = (mapcount - i + 1);
	wide2uni        = (_cups_wide2uni_t *)calloc(vmap->widecount,
	                                             sizeof(_cups_wide2uni_t));
	if (!wide2uni)
          goto vbcs_error;

	vmap->wide2uni = wide2uni;
      }

      wide2uni->widechar = (cups_vbcs_t)legchar;
      wide2uni->unichar  = (cups_ucs2_t)unichar;
      wide2uni ++;
    }

   /*
    * Save Unicode to legacy mapping in indirect lookup table...
    */

    vrow = vmap->uni2char[(int)((unichar >> 8) & 0xff)];
    if (!vrow)
    {
      vrow = (cups_vbcs_t *)calloc(256, sizeof(cups_vbcs_t));
      if (!vrow)
        goto vbcs_error;

      vmap->uni2char[(int) ((unichar >> 8) & 0xff)] = vrow;
    }

    vrow += (int)(unichar & 0xff);

   /*
    * Convert Replacement Character to visible replacement...
    */

    if (unichar == 0xfffd)
      legchar = (unsigned long)'?';

   /*
    * First (oldest) legacy character uses Unicode mapping cell...
    */

    if (!*vrow)
      *vrow = (cups_vbcs_t)legchar;
  }

  vmap->charcount = (i - vmap->widecount);

  cupsFileClose(fp);

 /*
  * Add it to the cache and return...
  */

  vmap->next     = vmap_cache;
  vmap_cache = vmap;

  DEBUG_printf(("    returning new vmap=%p\n", vmap));

  return (vmap);

 /*
  * If we get here, the file contains errors...
  */

  vbcs_error:

  free_vbcs_charmap(vmap);

  cupsFileClose(fp);

  DEBUG_puts("    Error, returning NULL!");

  return (NULL);
}


/*
 * End of "$Id$"
 */
