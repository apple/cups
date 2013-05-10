/*
 * "$Id$"
 *
 *   String functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2007 by Easy Software Products.
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
 *
 * Contents:
 *
 *   _cupsStrAlloc()       - Allocate/reference a string.
 *   _cupsStrFlush()       - Flush the string pool...
 *   _cupsStrFormatd()     - Format a floating-point number.
 *   _cupsStrFree()        - Free/dereference a string.
 *   _cupsStrScand()       - Scan a string for a floating-point number.
 *   _cupsStrStatistics()  - Return allocation statistics for string pool.
 *   _cups_strcpy()        - Copy a string allowing for overlapping strings.
 *   _cups_strdup()        - Duplicate a string.
 *   _cups_strcasecmp()    - Do a case-insensitive comparison.
 *   _cups_strncasecmp()   - Do a case-insensitive comparison on up to N chars.
 *   _cups_strlcat()       - Safely concatenate two strings.
 *   _cups_strlcpy()       - Safely copy two strings.
 *   compare_sp_items()    - Compare two string pool items...
 */

/*
 * Include necessary headers...
 */

#include <stdlib.h>
#include <limits.h>
#include "array.h"
#include "debug.h"
#include "string.h"
#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#endif /* HAVE_PTHREAD_H */


/*
 * Local globals...
 */

#ifdef HAVE_PTHREAD_H
static pthread_mutex_t	sp_mutex = PTHREAD_MUTEX_INITIALIZER;
					/* Mutex to control access to pool */
#endif /* HAVE_PTHREAD_H */
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
  _cups_sp_item_t	*item,		/* String pool item */
			key;		/* Search key */


 /*
  * Range check input...
  */

  if (!s)
    return (NULL);

 /*
  * Get the string pool...
  */

#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock(&sp_mutex);
#endif /* HAVE_PTHREAD_H */

  if (!stringpool)
    stringpool = cupsArrayNew((cups_array_func_t)compare_sp_items, NULL);

  if (!stringpool)
  {
#ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock(&sp_mutex);
#endif /* HAVE_PTHREAD_H */

    return (NULL);
  }

 /*
  * See if the string is already in the pool...
  */

  key.str = (char *)s;

  if ((item = (_cups_sp_item_t *)cupsArrayFind(stringpool, &key)) != NULL)
  {
   /*
    * Found it, return the cached string...
    */

    item->ref_count ++;

#ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock(&sp_mutex);
#endif /* HAVE_PTHREAD_H */

    return (item->str);
  }

 /*
  * Not found, so allocate a new one...
  */

  item = (_cups_sp_item_t *)calloc(1, sizeof(_cups_sp_item_t));
  if (!item)
  {
#ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock(&sp_mutex);
#endif /* HAVE_PTHREAD_H */

    return (NULL);
  }

  item->ref_count = 1;
  item->str       = strdup(s);

  if (!item->str)
  {
    free(item);

#ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock(&sp_mutex);
#endif /* HAVE_PTHREAD_H */

    return (NULL);
  }

 /*
  * Add the string to the pool and return it...
  */

  cupsArrayAdd(stringpool, item);

#ifdef HAVE_PTHREAD_H
  pthread_mutex_unlock(&sp_mutex);
#endif /* HAVE_PTHREAD_H */

  return (item->str);
}


/*
 * '_cupsStrFlush()' - Flush the string pool...
 */

void
_cupsStrFlush(void)
{
  _cups_sp_item_t	*item;		/* Current item */


  DEBUG_printf(("_cupsStrFlush(cg=%p)\n", cg));
  DEBUG_printf(("    %d strings in array\n", cupsArrayCount(stringpool)));

#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock(&sp_mutex);
#endif /* HAVE_PTHREAD_H */

  for (item = (_cups_sp_item_t *)cupsArrayFirst(stringpool);
       item;
       item = (_cups_sp_item_t *)cupsArrayNext(stringpool))
  {
    free(item->str);
    free(item);
  }

  cupsArrayDelete(stringpool);
  stringpool = NULL;

#ifdef HAVE_PTHREAD_H
  pthread_mutex_unlock(&sp_mutex);
#endif /* HAVE_PTHREAD_H */
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
    strlcpy(buf, temp, bufend - buf + 1);
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
			key;		/* Search key */


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

#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock(&sp_mutex);
#endif /* HAVE_PTHREAD_H */

  key.str = (char *)s;

  if ((item = (_cups_sp_item_t *)cupsArrayFind(stringpool, &key)) != NULL &&
      item->str == s)
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

      free(item->str);
      free(item);
    }
  }

#ifdef HAVE_PTHREAD_H
  pthread_mutex_unlock(&sp_mutex);
#endif /* HAVE_PTHREAD_H */
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

  while (isspace(*buf & 255))
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
      strlcpy(tempptr, loc->decimal_point, sizeof(temp) - (tempptr - temp));
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

#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock(&sp_mutex);
#endif /* HAVE_PTHREAD_H */

  for (count = 0, abytes = 0, tbytes = 0,
           item = (_cups_sp_item_t *)cupsArrayFirst(stringpool);
       item;
       item = (_cups_sp_item_t *)cupsArrayNext(stringpool))
  {
   /*
    * Count allocated memory, using a 64-bit aligned buffer as a basis.
    */

    count  += item->ref_count;
    len    = (strlen(item->str) + 8) & ~7;
    abytes += sizeof(_cups_sp_item_t) + len;
    tbytes += item->ref_count * len;
  }

#ifdef HAVE_PTHREAD_H
  pthread_mutex_unlock(&sp_mutex);
#endif /* HAVE_PTHREAD_H */

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
  char	*t;				/* New string pointer */


  if (s == NULL)
    return (NULL);

  if ((t = malloc(strlen(s) + 1)) == NULL)
    return (NULL);

  return (strcpy(t, s));
}
#endif /* !HAVE_STRDUP */


/*
 * '_cups_strcasecmp()' - Do a case-insensitive comparison.
 */

#ifndef HAVE_STRCASECMP
int				/* O - Result of comparison (-1, 0, or 1) */
_cups_strcasecmp(const char *s,	/* I - First string */
                const char *t)	/* I - Second string */
{
  while (*s != '\0' && *t != '\0')
  {
    if (tolower(*s & 255) < tolower(*t & 255))
      return (-1);
    else if (tolower(*s & 255) > tolower(*t & 255))
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
#endif /* !HAVE_STRCASECMP */

/*
 * '_cups_strncasecmp()' - Do a case-insensitive comparison on up to N chars.
 */

#ifndef HAVE_STRNCASECMP
int					/* O - Result of comparison (-1, 0, or 1) */
_cups_strncasecmp(const char *s,	/* I - First string */
                 vconst char *t,	/* I - Second string */
		  size_t     n)		/* I - Maximum number of characters to compare */
{
  while (*s != '\0' && *t != '\0' && n > 0)
  {
    if (tolower(*s & 255) < tolower(*t & 255))
      return (-1);
    else if (tolower(*s & 255) > tolower(*t & 255))
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
#endif /* !HAVE_STRNCASECMP */


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

  memcpy(dst + dstlen, src, srclen);
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

  memcpy(dst, src, srclen);
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
 * End of "$Id$".
 */
