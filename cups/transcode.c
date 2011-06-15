/*
 * "$Id: transcode.c 9306 2010-09-16 21:43:57Z mike $"
 *
 *   Transcoding support for CUPS.
 *
 *   Copyright 2007-2010 by Apple Inc.
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
 *   cupsCharsetToUTF8() - Convert legacy character set to UTF-8.
 *   cupsUTF8ToCharset() - Convert UTF-8 to legacy character set.
 *   cupsUTF8ToUTF32()   - Convert UTF-8 to UTF-32.
 *   cupsUTF32ToUTF8()   - Convert UTF-32 to UTF-8.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include <limits.h>
#include <time.h>
#ifdef HAVE_ICONV_H
#  include <iconv.h>
#endif /* HAVE_ICONV_H */


/*
 * Local globals...
 */

#ifdef HAVE_ICONV_H
static _cups_mutex_t	map_mutex = _CUPS_MUTEX_INITIALIZER;
					/* Mutex to control access to maps */
static iconv_t		map_from_utf8 = (iconv_t)-1;
					/* Convert from UTF-8 to charset */
static iconv_t		map_to_utf8 = (iconv_t)-1;
					/* Convert from charset to UTF-8 */
static cups_encoding_t	map_encoding = CUPS_AUTO_ENCODING;
					/* Which charset is cached */
#endif /* HAVE_ICONV_H */


/*
 * '_cupsCharmapFlush()' - Flush all character set maps out of cache.
 */

void
_cupsCharmapFlush(void)
{
#ifdef HAVE_ICONV_H
  if (map_from_utf8 != (iconv_t)-1)
  {
    iconv_close(map_from_utf8);
    map_from_utf8 = (iconv_t)-1;
  }

  if (map_to_utf8 != (iconv_t)-1)
  {
    iconv_close(map_to_utf8);
    map_to_utf8 = (iconv_t)-1;
  }

  map_encoding = CUPS_AUTO_ENCODING;
#endif /* HAVE_ICONV_H */
}


/*
 * 'cupsCharsetToUTF8()' - Convert legacy character set to UTF-8.
 */

int					/* O - Count or -1 on error */
cupsCharsetToUTF8(
    cups_utf8_t           *dest,	/* O - Target string */
    const char            *src,		/* I - Source string */
    const int             maxout,	/* I - Max output */
    const cups_encoding_t encoding)	/* I - Encoding */
{
  cups_utf8_t	*destptr;		/* Pointer into UTF-8 buffer */
#ifdef HAVE_ICONV_H
  size_t	srclen,			/* Length of source string */
		outBytesLeft;		/* Bytes remaining in output buffer */
#endif /* HAVE_ICONV_H */


 /*
  * Check for valid arguments...
  */

  DEBUG_printf(("2cupsCharsetToUTF8(dest=%p, src=\"%s\", maxout=%d, encoding=%d)",
	        dest, src, maxout, encoding));

  if (!dest || !src || maxout < 1)
  {
    if (dest)
      *dest = '\0';

    DEBUG_puts("3cupsCharsetToUTF8: Bad arguments, returning -1");
    return (-1);
  }

 /*
  * Handle identity conversions...
  */

  if (encoding == CUPS_UTF8 || encoding <= CUPS_US_ASCII ||
      encoding >= CUPS_ENCODING_VBCS_END)
  {
    strlcpy((char *)dest, src, maxout);
    return ((int)strlen((char *)dest));
  }

 /*
  * Handle ISO-8859-1 to UTF-8 directly...
  */

  destptr = dest;

  if (encoding == CUPS_ISO8859_1)
  {
    int		ch;			/* Character from string */
    cups_utf8_t	*destend;		/* End of UTF-8 buffer */


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

#ifdef HAVE_ICONV_H
  _cupsMutexLock(&map_mutex);

  if (map_encoding != encoding)
  {
    _cupsCharmapFlush();

    map_from_utf8 = iconv_open(_cupsEncodingName(encoding), "UTF-8");
    map_to_utf8   = iconv_open("UTF-8", _cupsEncodingName(encoding));
    map_encoding     = encoding;
  }

  if (map_to_utf8 != (iconv_t)-1)
  {
    char *altdestptr = (char *)dest;	/* Silence bogus GCC type-punned */

    srclen       = strlen(src);
    outBytesLeft = maxout - 1;

    iconv(map_to_utf8, (char **)&src, &srclen, &altdestptr, &outBytesLeft);
    *altdestptr = '\0';

    _cupsMutexUnlock(&map_mutex);

    return ((int)(altdestptr - (char *)dest));
  }

  _cupsMutexUnlock(&map_mutex);
#endif /* HAVE_ICONV_H */

 /*
  * No iconv() support, so error out...
  */

  *destptr = '\0';

  return (-1);
}


/*
 * 'cupsUTF8ToCharset()' - Convert UTF-8 to legacy character set.
 */

int					/* O - Count or -1 on error */
cupsUTF8ToCharset(
    char		  *dest,	/* O - Target string */
    const cups_utf8_t	  *src,		/* I - Source string */
    const int		  maxout,	/* I - Max output */
    const cups_encoding_t encoding)	/* I - Encoding */
{
  char		*destptr;		/* Pointer into destination */
#ifdef HAVE_ICONV_H
  size_t	srclen,			/* Length of source string */
		outBytesLeft;		/* Bytes remaining in output buffer */
#endif /* HAVE_ICONV_H */


 /*
  * Check for valid arguments...
  */

  if (!dest || !src || maxout < 1)
  {
    if (dest)
      *dest = '\0';

    return (-1);
  }

 /*
  * Handle identity conversions...
  */

  if (encoding == CUPS_UTF8 ||
      encoding >= CUPS_ENCODING_VBCS_END)
  {
    strlcpy(dest, (char *)src, maxout);
    return ((int)strlen(dest));
  }

 /*
  * Handle UTF-8 to ISO-8859-1 directly...
  */

  destptr = dest;

  if (encoding == CUPS_ISO8859_1 || encoding <= CUPS_US_ASCII)
  {
    int		ch,			/* Character from string */
		maxch;			/* Maximum character for charset */
    char	*destend;		/* End of ISO-8859-1 buffer */

    maxch   = encoding == CUPS_ISO8859_1 ? 256 : 128;
    destend = dest + maxout - 1;

    while (*src && destptr < destend)
    {
      ch = *src++;

      if ((ch & 0xe0) == 0xc0)
      {
	ch = ((ch & 0x1f) << 6) | (*src++ & 0x3f);

	if (ch < maxch)
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

#ifdef HAVE_ICONV_H
 /*
  * Convert input UTF-8 to legacy charset...
  */

  _cupsMutexLock(&map_mutex);

  if (map_encoding != encoding)
  {
    _cupsCharmapFlush();

    map_from_utf8 = iconv_open(_cupsEncodingName(encoding), "UTF-8");
    map_to_utf8   = iconv_open("UTF-8", _cupsEncodingName(encoding));
    map_encoding  = encoding;
  }

  if (map_from_utf8 != (iconv_t)-1)
  {
    char *altsrc = (char *)src;		/* Silence bogus GCC type-punned */

    srclen       = strlen((char *)src);
    outBytesLeft = maxout - 1;

    iconv(map_from_utf8, &altsrc, &srclen, &destptr, &outBytesLeft);
    *destptr = '\0';

    _cupsMutexUnlock(&map_mutex);

    return ((int)(destptr - dest));
  }

  _cupsMutexUnlock(&map_mutex);
#endif /* HAVE_ICONV_H */

 /*
  * No iconv() support, so error out...
  */

  *destptr = '\0';

  return (-1);
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

  DEBUG_printf(("2cupsUTF8ToUTF32(dest=%p, src=\"%s\", maxout=%d)", dest,
                src, maxout));

  if (dest)
    *dest = 0;

  if (!dest || !src || maxout < 1 || maxout > CUPS_MAX_USTRING)
  {
    DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad arguments)");

    return (-1);
  }

 /*
  * Convert input UTF-8 to output UTF-32...
  */

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

      DEBUG_printf(("4cupsUTF8ToUTF32: %02x => %08X", src[-1], ch));
      continue;
    }
    else if ((ch & 0xe0) == 0xc0)
    {
     /*
      * Two-octet UTF-8 <= 2047 (Latin-x)...
      */

      next = *src++;
      if ((next & 0xc0) != 0x80)
      {
        DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

	return (-1);
      }

      ch32 = ((ch & 0x1f) << 6) | (next & 0x3f);

     /*
      * Check for non-shortest form (invalid UTF-8)...
      */

      if (ch32 < 0x80)
      {
        DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

	return (-1);
      }

      *dest++ = ch32;

      DEBUG_printf(("4cupsUTF8ToUTF32: %02x %02x => %08X",
                    src[-2], src[-1], (unsigned)ch32));
    }
    else if ((ch & 0xf0) == 0xe0)
    {
     /*
      * Three-octet UTF-8 <= 65535 (Plane 0 - BMP)...
      */

      next = *src++;
      if ((next & 0xc0) != 0x80)
      {
        DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

	return (-1);
      }

      ch32 = ((ch & 0x0f) << 6) | (next & 0x3f);

      next = *src++;
      if ((next & 0xc0) != 0x80)
      {
        DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

	return (-1);
      }

      ch32 = (ch32 << 6) | (next & 0x3f);

     /*
      * Check for non-shortest form (invalid UTF-8)...
      */

      if (ch32 < 0x800)
      {
        DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

	return (-1);
      }

      *dest++ = ch32;

      DEBUG_printf(("4cupsUTF8ToUTF32: %02x %02x %02x => %08X",
                    src[-3], src[-2], src[-1], (unsigned)ch32));
    }
    else if ((ch & 0xf8) == 0xf0)
    {
     /*
      * Four-octet UTF-8...
      */

      next = *src++;
      if ((next & 0xc0) != 0x80)
      {
        DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

	return (-1);
      }

      ch32 = ((ch & 0x07) << 6) | (next & 0x3f);

      next = *src++;
      if ((next & 0xc0) != 0x80)
      {
        DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

	return (-1);
      }

      ch32 = (ch32 << 6) | (next & 0x3f);

      next = *src++;
      if ((next & 0xc0) != 0x80)
      {
        DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

	return (-1);
      }

      ch32 = (ch32 << 6) | (next & 0x3f);

     /*
      * Check for non-shortest form (invalid UTF-8)...
      */

      if (ch32 < 0x10000)
      {
        DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

	return (-1);
      }

      *dest++ = ch32;

      DEBUG_printf(("4cupsUTF8ToUTF32: %02x %02x %02x %02x => %08X",
                    src[-4], src[-3], src[-2], src[-1], (unsigned)ch32));
    }
    else
    {
     /*
      * More than 4-octet (invalid UTF-8 sequence)...
      */

      DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

      return (-1);
    }

   /*
    * Check for UTF-16 surrogate (illegal UTF-8)...
    */

    if (ch32 >= 0xd800 && ch32 <= 0xdfff)
      return (-1);
  }

  *dest = 0;

  DEBUG_printf(("3cupsUTF8ToUTF32: Returning %d characters", maxout - 1 - i));

  return (maxout - 1 - i);
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

  DEBUG_printf(("2cupsUTF32ToUTF8(dest=%p, src=%p, maxout=%d)", dest, src,
                maxout));

  if (dest)
    *dest = '\0';

  if (!dest || !src || maxout < 1)
  {
    DEBUG_puts("3cupsUTF32ToUTF8: Returning -1 (bad args)");

    return (-1);
  }

 /*
  * Check for leading BOM in UTF-32 and inverted BOM...
  */

  start = dest;
  swap  = *src == 0xfffe0000;

  DEBUG_printf(("4cupsUTF32ToUTF8: swap=%d", swap));

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
    {
      DEBUG_puts("3cupsUTF32ToUTF8: Returning -1 (character out of range)");

      return (-1);
    }

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

      DEBUG_printf(("4cupsUTF32ToUTF8: %08x => %02x", (unsigned)ch, dest[-1]));
    }
    else if (ch < 0x800)
    {
     /*
      * Two-octet UTF-8 <= 2047 (Latin-x)...
      */

      if (i < 2)
      {
        DEBUG_puts("3cupsUTF32ToUTF8: Returning -1 (too long 2)");

        return (-1);
      }

      *dest++ = (cups_utf8_t)(0xc0 | ((ch >> 6) & 0x1f));
      *dest++ = (cups_utf8_t)(0x80 | (ch & 0x3f));
      i -= 2;

      DEBUG_printf(("4cupsUTF32ToUTF8: %08x => %02x %02x", (unsigned)ch,
                    dest[-2], dest[-1]));
    }
    else if (ch < 0x10000)
    {
     /*
      * Three-octet UTF-8 <= 65535 (Plane 0 - BMP)...
      */

      if (i < 3)
      {
        DEBUG_puts("3cupsUTF32ToUTF8: Returning -1 (too long 3)");

        return (-1);
      }

      *dest++ = (cups_utf8_t)(0xe0 | ((ch >> 12) & 0x0f));
      *dest++ = (cups_utf8_t)(0x80 | ((ch >> 6) & 0x3f));
      *dest++ = (cups_utf8_t)(0x80 | (ch & 0x3f));
      i -= 3;

      DEBUG_printf(("4cupsUTF32ToUTF8: %08x => %02x %02x %02x", (unsigned)ch,
                    dest[-3], dest[-2], dest[-1]));
    }
    else
    {
     /*
      * Four-octet UTF-8...
      */

      if (i < 4)
      {
        DEBUG_puts("3cupsUTF32ToUTF8: Returning -1 (too long 4)");

        return (-1);
      }

      *dest++ = (cups_utf8_t)(0xf0 | ((ch >> 18) & 0x07));
      *dest++ = (cups_utf8_t)(0x80 | ((ch >> 12) & 0x3f));
      *dest++ = (cups_utf8_t)(0x80 | ((ch >> 6) & 0x3f));
      *dest++ = (cups_utf8_t)(0x80 | (ch & 0x3f));
      i -= 4;

      DEBUG_printf(("4cupsUTF32ToUTF8: %08x => %02x %02x %02x %02x",
                    (unsigned)ch, dest[-4], dest[-3], dest[-2], dest[-1]));
    }
  }

  *dest = '\0';

  DEBUG_printf(("3cupsUTF32ToUTF8: Returning %d", (int)(dest - start)));

  return ((int)(dest - start));
}


/*
 * End of "$Id: transcode.c 9306 2010-09-16 21:43:57Z mike $"
 */
