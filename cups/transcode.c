/*
 * "$Id: transcode.c,v 1.1.2.2 2002/08/20 12:41:53 mike Exp $"
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
 *	 Attn: CUPS Licensing Information
 *	 Easy Software Products
 *	 44141 Airport View Drive, Suite 204
 *	 Hollywood, Maryland 20636-3111 USA
 *
 *	 Voice: (301) 373-9603
 *	 EMail: cups-info@cups.org
 *	   WWW: http://www.cups.org
 *
 * Contents:
 *
 *   cupsCharmapGet()	 - Get a character set map.
 *   cupsCharmapFree()	 - Free a character set map.
 *   cupsCharmapFlush()	 - Flush all character set maps out of cache.
 *   cupsUTF8ToCharset() - Convert UTF-8 to legacy character set.
 *   cupsCharsetToUTF8() - Convert legacy character set to UTF-8.
 *   cupsUTF8ToUTF16()	 - Convert UTF-8 to UTF-16.
 *   cupsUTF16ToUTF8()	 - Convert UTF-16 to UTF-8.
 *   cupsUTF8ToUTF32()	 - Convert UTF-8 to UTF-32.
 *   cupsUTF32ToUTF8()	 - Convert UTF-32 to UTF-8.
 *   cupsUTF16ToUTF32()	 - Convert UTF-16 to UTF-32.
 *   cupsUTF32ToUTF16()	 - Convert UTF-32 to UTF-16.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#include "language.h"
#include "string.h"
#include "transcode.h"

/*
 * Globals...
 */

int	_cupsFixMapNames = 1;  /* Fix map names to Unicode names */
int	_cupsStrictUTF8 = 0;   /* Non-shortest-form is illegal */
int	_cupsStrictUTF16 = 0;  /* Invalid surrogate pair is illegal */
int	_cupsStrictUTF32 = 0;  /* Greater than 0x10ffff is illegal */
int	_cupsRequireBOM = 0;   /* Require BOM for little/big-endian */
int	_cupsSupportBOM = 0;   /* Support BOM for little/big-endian */
int	_cupsSupport8859 = 1;  /* Support ISO 8859-x repertoires */
int	_cupsSupportWin = 1;   /* Support Windows-x repertoires */
int	_cupsSupportCJK = 0;   /* Support CJK Asian repertoires */

/*
 * Local Globals...
 */

static cups_cmap_t	*cmap_cache = NULL;    /* SBCS Charmap Cache */
#if 0
static cups_dmap_t	*dmap_cache = NULL;    /* DBCS Charmap Cache */
#endif

/*
 * 'cupsCharmapGet()' - Get a character set map.
 *
 * This code only handles single-byte character sets (SBCS)...
 * This code does not handle double-byte character sets (DBCS) or
 * multiple-byte sets (MBCS) with charset switching via escapes...
 */
void *				/* O - Charset map pointer */
cupsCharmapGet(const cups_encoding_t encoding)	/* I - Encoding */
{
  int		i, j;		/* Looping variables */
  int		legchar;	/* Legacy character value */
  cups_utf32_t	unichar;	/* Unicode character value */
  cups_cmap_t	*cmap;		/* Legacy SBCS / Unicode Charset Map */
  char		*datadir;	/* CUPS_DATADIR environment variable */
  char		mapname[80];	/* Name of charset map */
  char		filename[256];	/* Filename for charset map file */
  FILE		*fp;		/* Charset map file pointer */
  char		*s;		/* Line parsing pointer */
  cups_sbcs_t	*row;		/* Pointer to row in 'uni2char' */
  char		line[256];	/* Line from charset map file */

 /*
  * See if we already have this charset map loaded...
  */
  for (cmap = cmap_cache; cmap != NULL; cmap = cmap->next)
  {
    if (cmap->encoding == encoding)
    {
      cmap->used ++;
      return ((void *) cmap);
    }
  }

 /*
  * Get the data directory and encoding name...
  */
  if ((datadir = getenv("CUPS_DATADIR")) == NULL)
    datadir = CUPS_DATADIR;

  snprintf(mapname, sizeof(mapname), "%s.txt", cupsEncodingName(encoding));

 /*
  * Open charset map input file...
  */
  snprintf(filename, sizeof(filename), "%s/charmaps/%s", datadir, mapname);
  fp = fopen(filename, "r");
  if (fp == NULL)
    return (NULL);

 /*
  * Allocate memory for SBCS charset map and add to cache...
  */
  cmap = (cups_cmap_t *) calloc(1, sizeof(cups_cmap_t));
  if (cmap == NULL)
  {
    fclose(fp);
    return (NULL);
  }
  cmap->next = cmap_cache;
  cmap_cache = cmap;
  cmap->used ++;
  cmap->encoding = encoding;

 /*
  * Save charset map into memory for transcoding...
  */
  for (i = j = 0; (i < 256) && (j < 40);)
  {
    s = fgets(&line[0], sizeof(line), fp);
    if (s == NULL)
      break;
    if ((*s == '#') || (*s == '\n') || (*s == '\0'))
      continue;
    i ++;
    if (strncmp (s, "0x", 2) == 0)
      s += 2;
    if (sscanf(s, "%x", &legchar) != 1)
      break;
    if ((legchar < 0) || (legchar > 255))
      break;
    while ((*s != 0) && (*s != ' ') && (*s != '\t'))
      s ++;
    while ((*s != 0) && ((*s == ' ') || (*s == '\t')))
      s ++;
    if (strncmp (s, "0x", 2) == 0)
      s += 2;
    if (sscanf(s, "%lx", &unichar) != 1)
      break;

   /*
    * Convert beyond Plane 0 (BMP) to Replacement Character...
    */
    if (unichar > 0xffff)
      unichar = 0xfffd;

   /*
    * Save legacy to Unicode mapping in direct lookup table...
    */
    cmap->char2uni[legchar] = (cups_ucs2_t) (unichar & 0xffff);

   /*
    * Save Unicode to legacy mapping in indirect lookup table...
    */
    row = cmap->uni2char[(((int) unichar) >> 8) & 0xff];
    if (row == NULL)
    {
      row = (cups_sbcs_t *) calloc(256, sizeof(cups_sbcs_t));
      if (row == NULL)
      {
	cupsCharmapFlush();
	return (NULL);
      }
      cmap->uni2char[(((int) unichar) >> 8) & 0xff] = row;
      j ++;
    }
    row += (int) (unichar & 0xff);

   /*
    * Convert Replacement Character to visible replacement...
    */
    if (unichar == 0xfffd)
      *row = '?';
    else
      *row = (cups_sbcs_t) legchar;
  }
  fclose(fp);
  return ((void *) cmap);
}

/*
 * 'cupsCharmapFree()' - Free a character set map.
 *
 * This does not actually free; use 'cupsCharmapFlush()' for that.
 */
void
cupsCharmapFree(const cups_encoding_t encoding) /* I - Encoding */
{
  cups_cmap_t	*cmap;		/* Legacy SBCS / Unicode Charset Map */

 /*
  * See if we already have this charset map loaded...
  */
  for (cmap = cmap_cache; cmap != NULL; cmap = cmap->next)
  {
    if (cmap->encoding == encoding)
    {
      if (cmap->used > 0)
	cmap->used --;
      break;
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
  int		i;		/* Looping variable */
  cups_cmap_t	*cmap;		/* Legacy SBCS / Unicode Charset Map */
  cups_cmap_t	*next;		/* Next Legacy Charset Map */
  cups_sbcs_t	*row;		/* Pointer to row in 'uni2char' */

 /*
  * Loop through charset map cache, free all memory...
  */
  for (cmap = cmap_cache; cmap != NULL; cmap = next)
  {
    for (i = 0; i < 256; i ++)
    {
      if ((row = cmap->uni2char[i]) != NULL)
	free(row);
    }
    next = cmap->next;
    free(cmap);
  }
  cmap_cache = NULL;
  return;
}

/*
 * 'cupsUTF8ToCharset()' - Convert UTF-8 to legacy character set.
 *
 * This code only handles single-byte character sets (SBCS)...
 * This code does not handle double-byte character sets (DBCS) or
 * multiple-byte sets (MBCS) with charset switching via escapes...
 */
int				/* O - Count or -1 on error */
cupsUTF8ToCharset(char *dest,	/* O - Target string */
    const cups_utf8_t *src,	     /* I - Source string */
    const int maxout,		/* I - Max output */
    cups_encoding_t encoding)	/* I - Encoding */
{
  char		*start = dest;	/* Start of destination string */
  cups_cmap_t	*cmap;		/* Legacy SBCS / Unicode Charset Map */
  cups_utf32_t	     ch;	     /* Character value */
  int		i;		/* Looping variable */
  int		worklen;	/* Internal UCS-4 string length */
  cups_utf32_t	     work[CUPS_MAX_USTRING];
				/* Internal UCS-4 string */
  cups_sbcs_t	     *row;	     /* Pointer to row in 'uni2char' */

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
  cmap = (cups_cmap_t *) cupsCharmapGet(encoding);
  if (cmap == NULL)
    return (-1);

 /*
  * Convert input UTF-8 to internal UCS-4 (and insert BOM)...
  */
  worklen = cupsUTF8ToUTF32(work, src, CUPS_MAX_USTRING);
  if (worklen < 0)
    return (-1);

 /*
  * Convert internal UCS-4 to output legacy charset (and delete BOM)...
  */
  for (i = 0; i < worklen;)
  {
    ch = work[i];
    if (ch == 0)
      break;
    i ++;

   /*
    * Check for leading BOM (and delete from output)...
    */
    if ((i == 1) && (ch == 0xfeff))
      continue;

   /*
    * Convert ASCII verbatim (optimization)...
    */
    if (ch < 0x7f)
    {
      *dest = (char) ch;
      dest ++;
      continue;
    }

   /*
    * Convert unknown character to visible replacement...
    */
    row = cmap->uni2char[(int) ((ch >> 8) & 0xff)];
    if (row)
      row += (int) (ch & 0xff);
    if ((row == NULL) || (*row == 0))
      *dest = '?';
    else
      *dest = (char) (*row);
    dest ++;
  }
  *dest = '\0';
  worklen = (int) (dest - start);
  cupsCharmapFree(encoding);
  return (worklen);
}

/*
 * 'cupsCharsetToUTF8()' - Convert legacy character set to UTF-8.
 *
 * This code only handles single-byte character sets (SBCS)...
 * This code does not handle double-byte character sets (DBCS) or
 * multiple-byte sets (MBCS) with charset switching via escapes...
 */
int				/* O - Count or -1 on error */
cupsCharsetToUTF8(cups_utf8_t *dest, /* O - Target string */
    const char *src,		/* I - Source string */
    const int maxout,		/* I - Max output */
    cups_encoding_t encoding)	/* I - Encoding */
{
  cups_cmap_t	*cmap;		/* Legacy SBCS / Unicode Charset Map */
  int		i;		/* Looping variable */
  int		worklen;	/* Internal UCS-4 string length */
  cups_utf32_t	     work[CUPS_MAX_USTRING];
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
  cmap = (cups_cmap_t *) cupsCharmapGet(encoding);
  if (cmap == NULL)
    return (-1);

 /*
  * Convert input legacy charset to internal UCS-4 (and insert BOM)...
  */
  i = 0;
  if (_cupsRequireBOM)
  {
    work[0] = 0xfeff;
    i ++;
  }
  for (; i < (CUPS_MAX_USTRING - 1); src ++)
  {
    if (*src == '\0')
      break;
    if (cmap->char2uni[(cups_sbcs_t) *src] == 0)
      work[i] = 0xfffd;
    else
      work[i] = (cups_utf32_t) cmap->char2uni[(cups_sbcs_t) *src];
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
 * 'cupsUTF8ToUTF16()' - Convert UTF-8 to UTF-16.
 *
 * This code does not support Unicode beyond 16-bits (Plane 0)...
 */
int				/* O - Count or -1 on error */
cupsUTF8ToUTF16(cups_utf16_t *dest,  /* O - Target string */
    const cups_utf8_t *src,	     /* I - Source string */
    const int maxout)		/* I - Max output */
{
  int		worklen;	/* Internal UCS-4 string length */
  cups_utf32_t	     work[CUPS_MAX_USTRING];
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
int				/* O - Count or -1 on error */
cupsUTF16ToUTF8(cups_utf8_t *dest,   /* O - Target string */
    const cups_utf16_t *src,	     /* I - Source string */
    const int maxout)		/* I - Max output */
{
  int		worklen;	/* Internal UCS-4 string length */
  cups_utf32_t	     work[CUPS_MAX_USTRING];
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
 *	  0 to 127 = 0xxxxxxx (US-ASCII)
 *     128 to 2047 = 110xxxxx 10yyyyyy
 *   2048 to 65535 = 1110xxxx 10yyyyyy 10zzzzzz
 *	   > 65535 = 11110xxx 10yyyyyy 10zzzzzz 10xxxxxx
 *
 * UTF-32 prohibits chars beyond Plane 16 (> 0x10ffff) in UCS-4,
 * which would convert to five- or six-octet UTF-8 sequences...
 *
 * This code does not support Unicode beyond 16-bits (Plane 0)...
 */
int				/* O - Count or -1 on error */
cupsUTF8ToUTF32(cups_utf32_t *dest,  /* O - Target string */
    const cups_utf8_t *src,	     /* I - Source string */
    const int maxout)		/* I - Max output */
{
  cups_utf8_t	     *first = (cups_utf8_t *) src;
  int		srclen;		/* Source string length */
  int		i;		/* Looping variable */
  cups_utf32_t	     ch;	     /* Character value */
  cups_utf32_t	     next;	     /* Next character value */
  cups_utf32_t	     ch32;	     /* UTF-32 character value */

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
  i = 0;
  if (_cupsRequireBOM)
  {
    *dest = 0xfeff;
    dest ++;
    i ++;
  }
  srclen = strlen((char *) src);
  for (; i < (maxout - 1); src ++, dest ++)
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
      if ((_cupsStrictUTF8) && (ch32 <= 127))
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
      if ((_cupsStrictUTF8) && (ch32 <= 2047))
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
      * Five-octet UTF-8 to Replacement Character...
      */
      if (_cupsStrictUTF32)
	return (-1);
      if (((src - first) + 4) >= srclen)
	return (-1);
      src += 4;
      *dest = 0xfffd;
    }
    else if ((ch & 0xfe) == 0xfc)
    {
     /*
      * Six-octet UTF-8 to Replacement Character...
      */
      if (_cupsStrictUTF32)
	return (-1);
      if (((src - first) + 5) >= srclen)
	return (-1);
      src += 5;
      *dest = 0xfffd;
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
    if ((_cupsStrictUTF8) && (*dest > 0x10ffff))
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
 *
 * This code does not support Unicode beyond 16-bits (Plane 0)...
 */
int				/* O - Count or -1 on error */
cupsUTF32ToUTF8(cups_utf8_t *dest,   /* O - Target string */
    const cups_utf32_t *src,	     /* I - Source string */
    const int maxout)		/* I - Max output */
{
  cups_utf32_t	     *first = (cups_utf32_t *) src;
				/* First source char */
  cups_utf8_t	     *start = dest;  /* Start of destination string */
  int		i;		/* Looping variable */
  int		swap = 0;	/* Byte-swap input to output */
  cups_utf32_t	     ch;	     /* Character value */

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
  if ((_cupsSupportBOM | _cupsRequireBOM)
  && (*src == 0xfffe0000))
    swap = 1;

 /*
  * Check for leading BOM in UTF-32 and unsupported...
  */
  if ((_cupsSupportBOM == 0)
  && ((*src == 0xfeff) || (*src == 0xfffe0000)))
    return (-1);

 /*
  * Check for missing BOM in UTF-32 and required...
  */
  if ((_cupsRequireBOM)
  && ((*src != 0xfeff) && (*src != 0xfffe0000)))
    return (-1);

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
    if ((_cupsStrictUTF32) && (ch > 0x10ffff))
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
int				/* O - Count or -1 on error */
cupsUTF16ToUTF32(cups_utf32_t *dest, /* O - Target string */
    const cups_utf16_t *src,	     /* I - Source string */
    const int maxout)		/* I - Max output */
{
  int		i;		/* Looping variable */
  int		swap = 0;	/* Byte-swap input to output */
  int		surrogate = 0;	/* Expecting low-half surrogate */
  cups_utf32_t	     ch;	     /* Character value */

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
  if ((_cupsSupportBOM | _cupsRequireBOM)
  && (*src == 0xfffe))
    swap = 1;

 /*
  * Check for leading BOM in UTF-16 and unsupported...
  */
  if ((_cupsSupportBOM == 0)
  && ((*src == 0xfeff) || (*src == 0xfffe)))
    return (-1);

 /*
  * Check for missing BOM in UTF-16 and required...
  */
  if ((_cupsRequireBOM)
  && ((*src != 0xfeff) && (*src != 0xfffe)))
    return (-1);

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
      if ((_cupsStrictUTF16) && (surrogate == 0))
	return (-1);
      surrogate = 0;
      continue;
    }

   /*
    * Convert UTF-16 high-half surrogate to Replacement Character...
    */
    if ((ch >= 0xd800) && (ch <= 0xdbff))
    {
      if ((_cupsStrictUTF16) && (surrogate == 1))
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
int				/* O - Count or -1 on error */
cupsUTF32ToUTF16(cups_utf16_t *dest, /* O - Target string */
    const cups_utf32_t *src,	     /* I - Source string */
    const int maxout)		/* I - Max output */
{
  int		i;		/* Looping variable */
  int		swap = 0;	/* Byte-swap input to output */
  cups_utf32_t	     ch;	     /* Character value */

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
  if ((_cupsSupportBOM | _cupsRequireBOM)
  && (*src == 0xfffe0000))
    swap = 1;

 /*
  * Check for leading BOM in UTF-32 and unsupported...
  */
  if ((_cupsSupportBOM == 0)
  && ((*src == 0xfeff) || (*src == 0xfffe0000)))
    return (-1);

 /*
  * Check for missing BOM in UTF-32 and required...
  */
  if ((_cupsRequireBOM)
  && ((*src != 0xfeff) && (*src != 0xfffe0000)))
    return (-1);

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
    if ((_cupsStrictUTF32) && (ch > 0x10ffff))
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
 * End of "$Id: transcode.c,v 1.1.2.2 2002/08/20 12:41:53 mike Exp $"
 */
