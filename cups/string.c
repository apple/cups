/*
 * "$Id: string.c,v 1.5.2.6 2002/05/16 14:00:00 mike Exp $"
 *
 *   String functions for the Common UNIX Printing System (CUPS).
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
 *
 * Contents:
 *
 *   cups_strdup()      - Duplicate a string.
 *   cups_strcasecmp()  - Do a case-insensitive comparison.
 *   cups_strncasecmp() - Do a case-insensitive comparison on up to N chars.
 *   cups_strlcat()     - Safely concatenate two strings.
 *   cups_strlcpy()     - Safely copy two strings.
 */

/*
 * Include necessary headers...
 */

#include "string.h"


/*
 * 'cups_strdup()' - Duplicate a string.
 */

#  ifndef HAVE_STRDUP
char *				/* O - New string pointer */
cups_strdup(const char *s)	/* I - String to duplicate */
{
  char	*t;			/* New string pointer */


  if (s == NULL)
    return (NULL);

  if ((t = malloc(strlen(s) + 1)) == NULL)
    return (NULL);

  return (strcpy(t, s));
}
#  endif /* !HAVE_STRDUP */


/*
 * 'cups_strcasecmp()' - Do a case-insensitive comparison.
 */

#  ifndef HAVE_STRCASECMP
int				/* O - Result of comparison (-1, 0, or 1) */
cups_strcasecmp(const char *s,	/* I - First string */
               const char *t)	/* I - Second string */
{
  while (*s != '\0' && *t != '\0')
  {
    if (tolower(*s) < tolower(*t))
      return (-1);
    else if (tolower(*s) > tolower(*t))
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
#  endif /* !HAVE_STRCASECMP */

/*
 * 'cups_strncasecmp()' - Do a case-insensitive comparison on up to N chars.
 */

#  ifndef HAVE_STRNCASECMP
int				/* O - Result of comparison (-1, 0, or 1) */
cups_strncasecmp(const char *s,	/* I - First string */
                const char *t,	/* I - Second string */
	        size_t     n)	/* I - Maximum number of characters to compare */
{
  while (*s != '\0' && *t != '\0' && n > 0)
  {
    if (tolower(*s) < tolower(*t))
      return (-1);
    else if (tolower(*s) > tolower(*t))
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
 * 'cups_strlcat()' - Safely concatenate two strings.
 */

size_t				/* O - Length of string */
cups_strlcat(char       *dst,	/* O - Destination string */
             const char *src,	/* I - Source string */
	     size_t     size)	/* I - Size of destination string buffer */
{
  size_t	srclen;		/* Length of source string */
  size_t	dstlen;		/* Length of destination string */


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
 * 'cups_strlcpy()' - Safely copy two strings.
 */

size_t				/* O - Length of string */
cups_strlcpy(char       *dst,	/* O - Destination string */
             const char *src,	/* I - Source string */
	     size_t      size)	/* I - Size of destination string buffer */
{
  size_t	srclen;		/* Length of source string */


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
 * End of "$Id: string.c,v 1.5.2.6 2002/05/16 14:00:00 mike Exp $".
 */
