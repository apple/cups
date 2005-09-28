/*
 * "$Id$"
 *
 *   Mini-daemon utility functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
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
 * Contents:
 *
 *   cupsdCompareNames()   - Compare two names.
 *   cupsdSendIPPGroup()   - Send a group tag.
 *   cupsdSendIPPHeader()  - Send the IPP response header.
 *   cupsdSendIPPInteger() - Send an integer attribute.
 *   cupsdSendIPPString()  - Send a string attribute.
 *   cupsdSendIPPTrailer() - Send the end-of-message tag.
 */

/*
 * Include necessary headers...
 */

#include "util.h"


/*
 * 'cupsdCompareNames()' - Compare two names.
 *
 * This function basically does a strcasecmp() of the two strings,
 * but is also aware of numbers so that "a2" < "a100".
 */

int					/* O - Result of comparison */
cupsdCompareNames(const char *s,	/* I - First string */
                  const char *t)	/* I - Second string */
{
  int		diff,			/* Difference between digits */
		digits;			/* Number of digits */


 /*
  * Loop through both names, returning only when a difference is
  * seen.  Also, compare whole numbers rather than just characters, too!
  */

  while (*s && *t)
  {
    if (isdigit(*s & 255) && isdigit(*t & 255))
    {
     /*
      * Got a number; start by skipping leading 0's...
      */

      while (*s == '0')
        s ++;
      while (*t == '0')
        t ++;

     /*
      * Skip equal digits...
      */

      while (isdigit(*s & 255) && *s == *t)
      {
        s ++;
	t ++;
      }

     /*
      * Bounce out if *s and *t aren't both digits...
      */

      if (isdigit(*s & 255) && !isdigit(*t & 255))
        return (1);
      else if (!isdigit(*s & 255) && isdigit(*t & 255))
        return (-1);
      else if (!isdigit(*s & 255) || !isdigit(*t & 255))
        continue;     

      if (*s < *t)
        diff = -1;
      else
        diff = 1;

     /*
      * Figure out how many more digits there are...
      */

      digits = 0;
      s ++;
      t ++;

      while (isdigit(*s & 255))
      {
        digits ++;
	s ++;
      }

      while (isdigit(*t & 255))
      {
        digits --;
	t ++;
      }

     /*
      * Return if the number or value of the digits is different...
      */

      if (digits < 0)
        return (-1);
      else if (digits > 0)
        return (1);
      else if (diff)
        return (diff);
    }
    else if (tolower(*s) < tolower(*t))
      return (-1);
    else if (tolower(*s) > tolower(*t))
      return (1);
    else
    {
      s ++;
      t ++;
    }
  }

 /*
  * Return the results of the final comparison...
  */

  if (*s)
    return (1);
  else if (*t)
    return (-1);
  else
    return (0);
}


/*
 * 'cupsdSendIPPGroup()' - Send a group tag.
 */

void
cupsdSendIPPGroup(ipp_tag_t group_tag)	/* I - Group tag */
{
 /*
  * Send IPP group tag (1 byte)...
  */

  putchar(group_tag);
}


/*
 * 'cupsdSendIPPHeader()' - Send the IPP response header.
 */

void
cupsdSendIPPHeader(
    ipp_status_t status_code,		/* I - Status code */
    int          request_id)		/* I - Request ID */
{
 /*
  * Send IPP/1.1 response header: version number (2 bytes), status code
  * (2 bytes), and request ID (4 bytes)...
  */

  putchar(1);
  putchar(1);

  putchar(status_code >> 8);
  putchar(status_code);

  putchar(request_id >> 24);
  putchar(request_id >> 16);
  putchar(request_id >> 8);
  putchar(request_id);
}


/*
 * 'cupsdSendIPPInteger()' - Send an integer attribute.
 */

void
cupsdSendIPPInteger(
    ipp_tag_t  value_tag,		/* I - Value tag */
    const char *name,			/* I - Attribute name */
    int        value)			/* I - Attribute value */
{
  size_t	len;			/* Length of attribute name */


 /*
  * Send IPP integer value: value tag (1 byte), name length (2 bytes),
  * name string (without nul), and value (4 bytes)...
  */

  putchar(value_tag);

  len = strlen(name);
  putchar(len >> 8);
  putchar(len);

  fputs(name, stdout);

  putchar(value >> 24);
  putchar(value >> 16);
  putchar(value >> 8);
  putchar(value);
}


/*
 * 'cupsdSendIPPString()' - Send a string attribute.
 */

void
cupsdSendIPPString(
    ipp_tag_t  value_tag,		/* I - Value tag */
    const char *name,			/* I - Attribute name */
    const char *value)			/* I - Attribute value */
{
  size_t	len;			/* Length of attribute name */


 /*
  * Send IPP string value: value tag (1 byte), name length (2 bytes),
  * name string (without nul), value length (2 bytes), and value string
  * (without nul)...
  */

  putchar(value_tag);

  len = strlen(name);
  putchar(len >> 8);
  putchar(len);

  fputs(name, stdout);

  len = strlen(value);
  putchar(len >> 8);
  putchar(len);

  fputs(value, stdout);
}


/*
 * 'cupsdSendIPPTrailer()' - Send the end-of-message tag.
 */

void
cupsdSendIPPTrailer(void)
{
  putchar(IPP_TAG_END);
  fflush(stdout);
}


/*
 * End of "$Id$".
 */
