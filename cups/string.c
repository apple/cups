/*
 * "$Id$"
 *
 *   String functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products.
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
 *   _cups_sp_alloc()      - Allocate/reference a string.
 *   _cups_sp_flush()      - Flush the string pool...
 *   _cups_sp_free()       - Free/dereference a string.
 *   _cups_sp_statistics() - Return allocation statistics for string pool.
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
#include "debug.h"
#include "string.h"
#include "globals.h"


/*
 * Local functions...
 */

static int	compare_sp_items(_cups_sp_item_t *a, _cups_sp_item_t *b);


/*
 * '_cups_sp_alloc()' - Allocate/reference a string.
 */

char *					/* O - String pointer */
_cups_sp_alloc(const char *s)		/* I - String */
{
  _cups_globals_t	*cg;		/* Global data */
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

  cg = _cupsGlobals();

  if (!cg->stringpool)
    cg->stringpool = cupsArrayNew((cups_array_func_t)compare_sp_items, NULL);

  if (!cg->stringpool)
    return (NULL);

 /*
  * See if the string is already in the pool...
  */

  key.str = (char *)s;

  if ((item = (_cups_sp_item_t *)cupsArrayFind(cg->stringpool, &key)) != NULL)
  {
   /*
    * Found it, return the cached string...
    */

    item->ref_count ++;

    return (item->str);
  }

 /*
  * Not found, so allocate a new one...
  */

  item = (_cups_sp_item_t *)calloc(1, sizeof(_cups_sp_item_t));
  if (!item)
    return (NULL);

  item->ref_count = 1;
  item->str       = strdup(s);

  if (!item->str)
  {
    free(item);
    return (NULL);
  }

 /*
  * Add the string to the pool and return it...
  */

  cupsArrayAdd(cg->stringpool, item);

  return (item->str);
}


/*
 * '_cups_sp_flush()' - Flush the string pool...
 */

void
_cups_sp_flush(_cups_globals_t *cg)	/* I - Global data */
{
  _cups_sp_item_t	*item;		/* Current item */


  for (item = (_cups_sp_item_t *)cupsArrayFirst(cg->stringpool);
       item;
       item = (_cups_sp_item_t *)cupsArrayNext(cg->stringpool))
  {
    free(item->str);
    free(item);
  }

  cupsArrayDelete(cg->stringpool);
}


/*
 * '_cups_sp_free()' - Free/dereference a string.
 */

void
_cups_sp_free(const char *s)
{
  _cups_globals_t	*cg;		/* Global data */
  _cups_sp_item_t	*item,		/* String pool item */
			key;		/* Search key */


 /*
  * Range check input...
  */

  if (!s)
    return;

 /*
  * Get the string pool...
  */

  cg = _cupsGlobals();

  if (!cg->stringpool)
    return;

 /*
  * See if the string is already in the pool...
  */

  key.str = (char *)s;

  if ((item = (_cups_sp_item_t *)cupsArrayFind(cg->stringpool, &key)) != NULL)
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

      cupsArrayRemove(cg->stringpool, item);

      free(item->str);
      free(item);
    }
  }
}


/*
 * '_cups_sp_statistics()' - Return allocation statistics for string pool.
 */

size_t					/* O - Number of strings */
_cups_sp_statistics(size_t *alloc_bytes,/* O - Allocated bytes */
                    size_t *total_bytes)/* O - Total string bytes */
{
  size_t		count,		/* Number of strings */
			abytes,		/* Allocated string bytes */
			tbytes,		/* Total string bytes */
			len;		/* Length of string */
  _cups_sp_item_t	*item;		/* Current item */
  _cups_globals_t	*cg;		/* Global data */


 /*
  * Loop through strings in pool, counting everything up...
  */

  cg = _cupsGlobals();

  for (count = 0, abytes = 0, tbytes = 0,
           item = (_cups_sp_item_t *)cupsArrayFirst(cg->stringpool);
       item;
       item = (_cups_sp_item_t *)cupsArrayNext(cg->stringpool))
  {
   /*
    * Count allocated memory, using a 64-bit aligned buffer as a basis.
    */

    count  += item->ref_count;
    len    = (strlen(item->str) + 8) & ~7;
    abytes += sizeof(_cups_sp_item_t) + len;
    tbytes += item->ref_count * len;
  }

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
