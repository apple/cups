/*
 * "$Id: string.c 11889 2014-05-22 13:54:15Z msweet $"
 *
 * String functions for CUPS.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#define _CUPS_STRING_C_
#include "cups-private.h"
#include <stddef.h>
#include <limits.h>


/*
 * Local globals...
 */

static _cups_mutex_t	sp_mutex = _CUPS_MUTEX_INITIALIZER;
					/* Mutex to control access to pool */
static cups_array_t	*stringpool = NULL;
					/* Global string pool */


/*
 * Local functions...
 */

static int	compare_sp_items(_cups_sp_item_t *a, _cups_sp_item_t *b);


/*
 * '_cupsStrAlloc()' - Allocate/reference a string.
 */

char *					/* O - String pointer */
_cupsStrAlloc(const char *s)		/* I - String */
{
  size_t		slen;		/* Length of string */
  _cups_sp_item_t	*item,		/* String pool item */
			*key;		/* Search key */


 /*
  * Range check input...
  */

  if (!s)
    return (NULL);

 /*
  * Get the string pool...
  */

  _cupsMutexLock(&sp_mutex);

  if (!stringpool)
    stringpool = cupsArrayNew((cups_array_func_t)compare_sp_items, NULL);

  if (!stringpool)
  {
    _cupsMutexUnlock(&sp_mutex);

    return (NULL);
  }

 /*
  * See if the string is already in the pool...
  */

  key = (_cups_sp_item_t *)(s - offsetof(_cups_sp_item_t, str));

  if ((item = (_cups_sp_item_t *)cupsArrayFind(stringpool, key)) != NULL)
  {
   /*
    * Found it, return the cached string...
    */

    item->ref_count ++;

#ifdef DEBUG_GUARDS
    DEBUG_printf(("5_cupsStrAlloc: Using string %p(%s) for \"%s\", guard=%08x, "
                  "ref_count=%d", item, item->str, s, item->guard,
		  item->ref_count));

    if (item->guard != _CUPS_STR_GUARD)
      abort();
#endif /* DEBUG_GUARDS */

    _cupsMutexUnlock(&sp_mutex);

    return (item->str);
  }

 /*
  * Not found, so allocate a new one...
  */

  slen = strlen(s);
  item = (_cups_sp_item_t *)calloc(1, sizeof(_cups_sp_item_t) + slen);
  if (!item)
  {
    _cupsMutexUnlock(&sp_mutex);

    return (NULL);
  }

  item->ref_count = 1;
  memcpy(item->str, s, slen + 1);

#ifdef DEBUG_GUARDS
  item->guard = _CUPS_STR_GUARD;

  DEBUG_printf(("5_cupsStrAlloc: Created string %p(%s) for \"%s\", guard=%08x, "
		"ref_count=%d", item, item->str, s, item->guard,
		item->ref_count));
#endif /* DEBUG_GUARDS */

 /*
  * Add the string to the pool and return it...
  */

  cupsArrayAdd(stringpool, item);

  _cupsMutexUnlock(&sp_mutex);

  return (item->str);
}


/*
 * '_cupsStrDate()' - Return a localized date for a given time value.
 *
 * This function works around the locale encoding issues of strftime...
 */

char *					/* O - Buffer */
_cupsStrDate(char   *buf,		/* I - Buffer */
             size_t bufsize,		/* I - Size of buffer */
	     time_t timeval)		/* I - Time value */
{
  struct tm	*dateval;		/* Local date/time */
  char		temp[1024];		/* Temporary buffer */
  _cups_globals_t *cg = _cupsGlobals();	/* Per-thread globals */


  if (!cg->lang_default)
    cg->lang_default = cupsLangDefault();

  dateval = localtime(&timeval);

  if (cg->lang_default->encoding != CUPS_UTF8)
  {
    strftime(temp, sizeof(temp), "%c", dateval);
    cupsCharsetToUTF8((cups_utf8_t *)buf, temp, (int)bufsize, cg->lang_default->encoding);
  }
  else
    strftime(buf, bufsize, "%c", dateval);

  return (buf);
}


/*
 * '_cupsStrFlush()' - Flush the string pool.
 */

void
_cupsStrFlush(void)
{
  _cups_sp_item_t	*item;		/* Current item */


  DEBUG_printf(("4_cupsStrFlush: %d strings in array",
                cupsArrayCount(stringpool)));

  _cupsMutexLock(&sp_mutex);

  for (item = (_cups_sp_item_t *)cupsArrayFirst(stringpool);
       item;
       item = (_cups_sp_item_t *)cupsArrayNext(stringpool))
    free(item);

  cupsArrayDelete(stringpool);
  stringpool = NULL;

  _cupsMutexUnlock(&sp_mutex);
}


/*
 * '_cupsStrFormatd()' - Format a floating-point number.
 */

char *					/* O - Pointer to end of string */
_cupsStrFormatd(char         *buf,	/* I - String */
                char         *bufend,	/* I - End of string buffer */
		double       number,	/* I - Number to format */
                struct lconv *loc)	/* I - Locale data */
{
  char		*bufptr,		/* Pointer into buffer */
		temp[1024],		/* Temporary string */
		*tempdec,		/* Pointer to decimal point */
		*tempptr;		/* Pointer into temporary string */
  const char	*dec;			/* Decimal point */
  int		declen;			/* Length of decimal point */


 /*
  * Format the number using the "%.12f" format and then eliminate
  * unnecessary trailing 0's.
  */

  snprintf(temp, sizeof(temp), "%.12f", number);
  for (tempptr = temp + strlen(temp) - 1;
       tempptr > temp && *tempptr == '0';
       *tempptr-- = '\0');

 /*
  * Next, find the decimal point...
  */

  if (loc && loc->decimal_point)
  {
    dec    = loc->decimal_point;
    declen = (int)strlen(dec);
  }
  else
  {
    dec    = ".";
    declen = 1;
  }

  if (declen == 1)
    tempdec = strchr(temp, *dec);
  else
    tempdec = strstr(temp, dec);

 /*
  * Copy everything up to the decimal point...
  */

  if (tempdec)
  {
    for (tempptr = temp, bufptr = buf;
         tempptr < tempdec && bufptr < bufend;
	 *bufptr++ = *tempptr++);

    tempptr += declen;

    if (*tempptr && bufptr < bufend)
    {
      *bufptr++ = '.';

      while (*tempptr && bufptr < bufend)
        *bufptr++ = *tempptr++;
    }

    *bufptr = '\0';
  }
  else
  {
    strlcpy(buf, temp, (size_t)(bufend - buf + 1));
    bufptr = buf + strlen(buf);
  }

  return (bufptr);
}


/*
 * '_cupsStrFree()' - Free/dereference a string.
 */

void
_cupsStrFree(const char *s)		/* I - String to free */
{
  _cups_sp_item_t	*item,		/* String pool item */
			*key;		/* Search key */


 /*
  * Range check input...
  */

  if (!s)
    return;

 /*
  * Check the string pool...
  *
  * We don't need to lock the mutex yet, as we only want to know if
  * the stringpool is initialized.  The rest of the code will still
  * work if it is initialized before we lock...
  */

  if (!stringpool)
    return;

 /*
  * See if the string is already in the pool...
  */

  _cupsMutexLock(&sp_mutex);

  key = (_cups_sp_item_t *)(s - offsetof(_cups_sp_item_t, str));

#ifdef DEBUG_GUARDS
  if (key->guard != _CUPS_STR_GUARD)
  {
    DEBUG_printf(("5_cupsStrFree: Freeing string %p(%s), guard=%08x, "
                  "ref_count=%d", key, key->str, key->guard, key->ref_count));
    abort();
  }
#endif /* DEBUG_GUARDS */

  if ((item = (_cups_sp_item_t *)cupsArrayFind(stringpool, key)) != NULL &&
      item == key)
  {
   /*
    * Found it, dereference...
    */

    item->ref_count --;

    if (!item->ref_count)
    {
     /*
      * Remove and free...
      */

      cupsArrayRemove(stringpool, item);

      free(item);
    }
  }

  _cupsMutexUnlock(&sp_mutex);
}


/*
 * '_cupsStrRetain()' - Increment the reference count of a string.
 *
 * Note: This function does not verify that the passed pointer is in the
 *       string pool, so any calls to it MUST know they are passing in a
 *       good pointer.
 */

char *					/* O - Pointer to string */
_cupsStrRetain(const char *s)		/* I - String to retain */
{
  _cups_sp_item_t	*item;		/* Pointer to string pool item */


  if (s)
  {
    item = (_cups_sp_item_t *)(s - offsetof(_cups_sp_item_t, str));

#ifdef DEBUG_GUARDS
    if (item->guard != _CUPS_STR_GUARD)
    {
      DEBUG_printf(("5_cupsStrRetain: Retaining string %p(%s), guard=%08x, "
                    "ref_count=%d", item, s, item->guard, item->ref_count));
      abort();
    }
#endif /* DEBUG_GUARDS */

    _cupsMutexLock(&sp_mutex);

    item->ref_count ++;

    _cupsMutexUnlock(&sp_mutex);
  }

  return ((char *)s);
}


/*
 * '_cupsStrScand()' - Scan a string for a floating-point number.
 *
 * This function handles the locale-specific BS so that a decimal
 * point is always the period (".")...
 */

double					/* O - Number */
_cupsStrScand(const char   *buf,	/* I - Pointer to number */
              char         **bufptr,	/* O - New pointer or NULL on error */
              struct lconv *loc)	/* I - Locale data */
{
  char	temp[1024],			/* Temporary buffer */
	*tempptr;			/* Pointer into temporary buffer */


 /*
  * Range check input...
  */

  if (!buf)
    return (0.0);

 /*
  * Skip leading whitespace...
  */

  while (_cups_isspace(*buf))
    buf ++;

 /*
  * Copy leading sign, numbers, period, and then numbers...
  */

  tempptr = temp;
  if (*buf == '-' || *buf == '+')
    *tempptr++ = *buf++;

  while (isdigit(*buf & 255))
    if (tempptr < (temp + sizeof(temp) - 1))
      *tempptr++ = *buf++;
    else
    {
      if (bufptr)
	*bufptr = NULL;

      return (0.0);
    }

  if (*buf == '.')
  {
   /*
    * Read fractional portion of number...
    */

    buf ++;

    if (loc && loc->decimal_point)
    {
      strlcpy(tempptr, loc->decimal_point, sizeof(temp) - (size_t)(tempptr - temp));
      tempptr += strlen(tempptr);
    }
    else if (tempptr < (temp + sizeof(temp) - 1))
      *tempptr++ = '.';
    else
    {
      if (bufptr)
        *bufptr = NULL;

      return (0.0);
    }

    while (isdigit(*buf & 255))
      if (tempptr < (temp + sizeof(temp) - 1))
	*tempptr++ = *buf++;
      else
      {
	if (bufptr)
	  *bufptr = NULL;

	return (0.0);
      }
  }

  if (*buf == 'e' || *buf == 'E')
  {
   /*
    * Read exponent...
    */

    if (tempptr < (temp + sizeof(temp) - 1))
      *tempptr++ = *buf++;
    else
    {
      if (bufptr)
	*bufptr = NULL;

      return (0.0);
    }

    if (*buf == '+' || *buf == '-')
    {
      if (tempptr < (temp + sizeof(temp) - 1))
	*tempptr++ = *buf++;
      else
      {
	if (bufptr)
	  *bufptr = NULL;

	return (0.0);
      }
    }

    while (isdigit(*buf & 255))
      if (tempptr < (temp + sizeof(temp) - 1))
	*tempptr++ = *buf++;
      else
      {
	if (bufptr)
	  *bufptr = NULL;

	return (0.0);
      }
  }

 /*
  * Nul-terminate the temporary string and return the value...
  */

  if (bufptr)
    *bufptr = (char *)buf;

  *tempptr = '\0';

  return (strtod(temp, NULL));
}


/*
 * '_cupsStrStatistics()' - Return allocation statistics for string pool.
 */

size_t					/* O - Number of strings */
_cupsStrStatistics(size_t *alloc_bytes,	/* O - Allocated bytes */
                   size_t *total_bytes)	/* O - Total string bytes */
{
  size_t		count,		/* Number of strings */
			abytes,		/* Allocated string bytes */
			tbytes,		/* Total string bytes */
			len;		/* Length of string */
  _cups_sp_item_t	*item;		/* Current item */


 /*
  * Loop through strings in pool, counting everything up...
  */

  _cupsMutexLock(&sp_mutex);

  for (count = 0, abytes = 0, tbytes = 0,
           item = (_cups_sp_item_t *)cupsArrayFirst(stringpool);
       item;
       item = (_cups_sp_item_t *)cupsArrayNext(stringpool))
  {
   /*
    * Count allocated memory, using a 64-bit aligned buffer as a basis.
    */

    count  += item->ref_count;
    len    = (strlen(item->str) + 8) & (size_t)~7;
    abytes += sizeof(_cups_sp_item_t) + len;
    tbytes += item->ref_count * len;
  }

  _cupsMutexUnlock(&sp_mutex);

 /*
  * Return values...
  */

  if (alloc_bytes)
    *alloc_bytes = abytes;

  if (total_bytes)
    *total_bytes = tbytes;

  return (count);
}


/*
 * '_cups_strcpy()' - Copy a string allowing for overlapping strings.
 */

void
_cups_strcpy(char       *dst,		/* I - Destination string */
             const char *src)		/* I - Source string */
{
  while (*src)
    *dst++ = *src++;

  *dst = '\0';
}


/*
 * '_cups_strdup()' - Duplicate a string.
 */

#ifndef HAVE_STRDUP
char 	*				/* O - New string pointer */
_cups_strdup(const char *s)		/* I - String to duplicate */
{
  char		*t;			/* New string pointer */
  size_t	slen;			/* Length of string */


  if (!s)
    return (NULL);

  slen = strlen(s);
  if ((t = malloc(slen + 1)) == NULL)
    return (NULL);

  return (memcpy(t, s, slen + 1));
}
#endif /* !HAVE_STRDUP */


/*
 * '_cups_strcasecmp()' - Do a case-insensitive comparison.
 */

int				/* O - Result of comparison (-1, 0, or 1) */
_cups_strcasecmp(const char *s,	/* I - First string */
                 const char *t)	/* I - Second string */
{
  while (*s != '\0' && *t != '\0')
  {
    if (_cups_tolower(*s) < _cups_tolower(*t))
      return (-1);
    else if (_cups_tolower(*s) > _cups_tolower(*t))
      return (1);

    s ++;
    t ++;
  }

  if (*s == '\0' && *t == '\0')
    return (0);
  else if (*s != '\0')
    return (1);
  else
    return (-1);
}

/*
 * '_cups_strncasecmp()' - Do a case-insensitive comparison on up to N chars.
 */

int					/* O - Result of comparison (-1, 0, or 1) */
_cups_strncasecmp(const char *s,	/* I - First string */
                  const char *t,	/* I - Second string */
		  size_t     n)		/* I - Maximum number of characters to compare */
{
  while (*s != '\0' && *t != '\0' && n > 0)
  {
    if (_cups_tolower(*s) < _cups_tolower(*t))
      return (-1);
    else if (_cups_tolower(*s) > _cups_tolower(*t))
      return (1);

    s ++;
    t ++;
    n --;
  }

  if (n == 0)
    return (0);
  else if (*s == '\0' && *t == '\0')
    return (0);
  else if (*s != '\0')
    return (1);
  else
    return (-1);
}


#ifndef HAVE_STRLCAT
/*
 * '_cups_strlcat()' - Safely concatenate two strings.
 */

size_t					/* O - Length of string */
_cups_strlcat(char       *dst,		/* O - Destination string */
              const char *src,		/* I - Source string */
	      size_t     size)		/* I - Size of destination string buffer */
{
  size_t	srclen;			/* Length of source string */
  size_t	dstlen;			/* Length of destination string */


 /*
  * Figure out how much room is left...
  */

  dstlen = strlen(dst);
  size   -= dstlen + 1;

  if (!size)
    return (dstlen);		/* No room, return immediately... */

 /*
  * Figure out how much room is needed...
  */

  srclen = strlen(src);

 /*
  * Copy the appropriate amount...
  */

  if (srclen > size)
    srclen = size;

  memmove(dst + dstlen, src, srclen);
  dst[dstlen + srclen] = '\0';

  return (dstlen + srclen);
}
#endif /* !HAVE_STRLCAT */


#ifndef HAVE_STRLCPY
/*
 * '_cups_strlcpy()' - Safely copy two strings.
 */

size_t					/* O - Length of string */
_cups_strlcpy(char       *dst,		/* O - Destination string */
              const char *src,		/* I - Source string */
	      size_t      size)		/* I - Size of destination string buffer */
{
  size_t	srclen;			/* Length of source string */


 /*
  * Figure out how much room is needed...
  */

  size --;

  srclen = strlen(src);

 /*
  * Copy the appropriate amount...
  */

  if (srclen > size)
    srclen = size;

  memmove(dst, src, srclen);
  dst[srclen] = '\0';

  return (srclen);
}
#endif /* !HAVE_STRLCPY */


/*
 * 'compare_sp_items()' - Compare two string pool items...
 */

static int				/* O - Result of comparison */
compare_sp_items(_cups_sp_item_t *a,	/* I - First item */
                 _cups_sp_item_t *b)	/* I - Second item */
{
  return (strcmp(a->str, b->str));
}


/*
 * End of "$Id: string.c 11889 2014-05-22 13:54:15Z msweet $".
 */
