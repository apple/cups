/*
 * "$Id: ipp.c,v 1.3 1999/01/28 22:00:45 mike Exp $"
 *
 *   Internet Printing Protocol support functions for the Common UNIX
 *   Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   ippAddBoolean()    - Add a boolean attribute to an IPP request.
 *   ippAddBooleans()   - Add an array of boolean values.
 *   ippAddDate()       - Add a date attribute to an IPP request.
 *   ippAddInteger()    - Add a integer attribute to an IPP request.
 *   ippAddIntegers()   - Add an array of integer values.
 *   ippAddLString()    - Add a language-encoded string to an IPP request.
 *   ippAddLStrings()   - Add language-encoded strings to an IPP request.
 *   ippAddRange()      - Add a range of values to an IPP request.
 *   ippAddResolution() - Add a resolution value to an IPP request.
 *   ippAddString()     - Add an ASCII string to an IPP request.
 *   ippAddStrings()    - Add ASCII strings to an IPP request.
 *   ippDateToTime()    - Convert from RFC 1903 Date/Time format to UNIX time
 *   ippDelete()        - Delete an IPP request.
 *   ippFindAttribute() - Find a named attribute in a request...
 *   ippLength()        - Compute the length of an IPP request.
 *   ippRead()          - Read data for an IPP request.
 *   ipp_TimeToDate()   - Convert from UNIX time to RFC 1903 format.
 *   ippWrite()         - Write data for an IPP request.
 *   add_attr()         - Add a new attribute to the request.
 */

/*
 * Include necessary headers...
 */

#include "ipp.h"


/*
 * Local functions...
 */

static ipp_attribute_t	*add_attr(ipp_t *ipp, int num_values);


/*
 * 'ippAddBoolean()' - Add a boolean attribute to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddBoolean(ipp_t     *ipp,		/* I - IPP request */
              ipp_tag_t group,		/* I - IPP group */
              char      *name,		/* I - Name of attribute */
              char      value)		/* I - Value of attribute */
{
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = add_attr(ipp, 1)) == NULL)
    return (NULL);

  attr->name              = strdup(name);
  attr->group_tag         = group;
  attr->value_tag         = IPP_TAG_BOOLEAN;
  attr->values[0].boolean = value;

  return (attr);
}


/*
 * 'ippAddBooleans()' - Add an array of boolean values.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddBooleans(ipp_t     *ipp,		/* I - IPP request */
               ipp_tag_t group,		/* I - IPP group */
	       char      *name,		/* I - Name of attribute */
	       int       num_values,	/* I - Number of values */
	       char      *values)	/* I - Values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL || values == NULL)
    return (NULL);

  if ((attr = add_attr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name      = strdup(name);
  attr->group_tag = group;
  attr->value_tag = IPP_TAG_BOOLEAN;

  for (i = 0; i < num_values; i ++)
    attr->values[i].boolean = values[i];

  return (attr);
}


/*
 * 'ippAddDate()' - Add a date attribute to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddDate(ipp_t     *ipp,		/* I - IPP request */
           ipp_tag_t group,		/* I - IPP group */
	   char      *name,		/* I - Name of attribute */
	   uchar     *value)		/* I - Value */
{
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL || value == NULL)
    return (NULL);

  if ((attr = add_attr(ipp, 1)) == NULL)
    return (NULL);

  attr->name      = strdup(name);
  attr->group_tag = group;
  attr->value_tag = IPP_TAG_DATE;
  memcpy(attr->values[0].date, value, 11);

  return (attr);
}


/*
 * 'ippAddInteger()' - Add a integer attribute to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddInteger(ipp_t     *ipp,		/* I - IPP request */
              ipp_tag_t group,		/* I - IPP group */
              char      *name,		/* I - Name of attribute */
              int       value)		/* I - Value of attribute */
{
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = add_attr(ipp, 1)) == NULL)
    return (NULL);

  attr->name              = strdup(name);
  attr->group_tag         = group;
  attr->value_tag         = IPP_TAG_BOOLEAN;
  attr->values[0].integer = value;

  return (attr);
}


/*
 * 'ippAddIntegers()' - Add an array of integer values.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddIntegers(ipp_t     *ipp,		/* I - IPP request */
               ipp_tag_t group,		/* I - IPP group */
	       char      *name,		/* I - Name of attribute */
	       int       num_values,	/* I - Number of values */
	       int       *values)	/* I - Values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL || values == NULL)
    return (NULL);

  if ((attr = add_attr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name      = strdup(name);
  attr->group_tag = group;
  attr->value_tag = IPP_TAG_BOOLEAN;

  for (i = 0; i < num_values; i ++)
    attr->values[i].integer = values[i];

  return (attr);
}


/*
 * 'ippAddLString()' - Add a language-encoded string to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddLString(ipp_t     *ipp,		/* I - IPP request */
              ipp_tag_t group,		/* I - IPP group */
	      char      *name,		/* I - Name of attribute */
	      char      *charset,	/* I - Character set */
	      uchar     *value)		/* I - Value */
{
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = add_attr(ipp, 1)) == NULL)
    return (NULL);

  attr->name                      = strdup(name);
  attr->group_tag                 = group;
  attr->value_tag                 = IPP_TAG_TEXTLANG;
  attr->values[0].lstring.charset = strdup(charset);
  attr->values[0].lstring.string  = (uchar *)strdup((char *)value);

  return (attr);
}


/*
 * 'ippAddLStrings()' - Add language-encoded strings to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddLStrings(ipp_t     *ipp,		/* I - IPP request */
               ipp_tag_t group,		/* I - IPP group */
	       char      *name,		/* I - Name of attribute */
	       int       num_values,	/* I - Number of values */
	       char      *charset,	/* I - Character set */
	       uchar     **values)	/* I - Values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = add_attr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name      = strdup(name);
  attr->group_tag = group;
  attr->value_tag = IPP_TAG_TEXTLANG;

  for (i = 0; i < num_values; i ++)
  {
    if (i == 0)
      attr->values[0].lstring.charset = strdup(charset);
    else
      attr->values[i].lstring.charset = attr->values[0].lstring.charset;

    attr->values[i].lstring.string = (uchar *)strdup((char *)values[i]);
  }

  return (attr);
}


/*
 * 'ippAddRange()' - Add a range of values to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddRange(ipp_t     *ipp,		/* I - IPP request */
            ipp_tag_t group,		/* I - IPP group */
	    char      *name,		/* I - Name of attribute */
	    int       lower,		/* I - Lower value */
	    int       upper)		/* I - Upper value */
{
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = add_attr(ipp, 1)) == NULL)
    return (NULL);

  attr->name                  = strdup(name);
  attr->group_tag             = group;
  attr->value_tag             = IPP_TAG_RANGE;
  attr->values[0].range.lower = lower;
  attr->values[0].range.upper = upper;

  return (attr);
}


/*
 * 'ippAddResolution()' - Add a resolution value to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddResolution(ipp_t     *ipp,	/* I - IPP request */
        	 ipp_tag_t group,	/* I - IPP group */
		 char      *name,	/* I - Name of attribute */
		 ipp_res_t units,	/* I - Units for resolution */
		 int       xres,	/* I - X resolution */
		 int       yres)	/* I - Y resolution */
{
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = add_attr(ipp, 1)) == NULL)
    return (NULL);

  attr->name                       = strdup(name);
  attr->group_tag                  = group;
  attr->value_tag                  = IPP_TAG_RESOLUTION;
  attr->values[0].resolution.xres  = xres;
  attr->values[0].resolution.yres  = yres;
  attr->values[0].resolution.units = units;

  return (attr);
}


/*
 * 'ippAddString()' - Add an ASCII string to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddString(ipp_t     *ipp,		/* I - IPP request */
             ipp_tag_t group,		/* I - IPP group */
	     char      *name,		/* I - Name of attribute */
	     char      *value)		/* I - Value */
{
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = add_attr(ipp, 1)) == NULL)
    return (NULL);

  attr->name             = strdup(name);
  attr->group_tag        = group;
  attr->value_tag        = IPP_TAG_STRING;
  attr->values[0].string = strdup(value);

  return (attr);
}


/*
 * 'ippAddStrings()' - Add ASCII strings to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddStrings(ipp_t     *ipp,		/* I - IPP request */
              ipp_tag_t group,		/* I - IPP group */
	      char      *name,		/* I - Name of attribute */
	      int       num_values,	/* I - Number of values */
	      char      **values)	/* I - Values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = add_attr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name      = strdup(name);
  attr->group_tag = group;
  attr->value_tag = IPP_TAG_STRING;

  for (i = 0; i < num_values; i ++)
    attr->values[i].string = strdup(values[i]);

  return (attr);
}


/*
 * 'ippDateToTime()' - Convert from RFC 1903 Date/Time format to UNIX time
 *                      in seconds.
 */

time_t
ippDateToTime(uchar *date)	/* RFC 1903 date info */
{
  struct tm	unixdate;	/* UNIX date/time info */
  time_t	t;		/* Computed time */


  memset(&unixdate, 0, sizeof(unixdate));

 /*
  * RFC-1903 date/time format is:
  *
  *    Byte(s)  Description
  *    -------  -----------
  *    0-1      Year (0 to 65535)
  *    2        Month (1 to 12)
  *    3        Day (1 to 31)
  *    4        Hours (0 to 23)
  *    5        Minutes (0 to 59)
  *    6        Seconds (0 to 60, 60 = "leap second")
  *    7        Deciseconds (0 to 9)
  *    8        +/- UTC
  *    9        UTC hours (0 to 11)
  *    10       UTC minutes (0 to 59)
  */

  unixdate.tm_year = ((date[0] << 8) | date[1]) - 1900;
  unixdate.tm_mon  = date[2] - 1;
  unixdate.tm_mday = date[3];
  unixdate.tm_hour = date[4];
  unixdate.tm_min  = date[5];
  unixdate.tm_sec  = date[6];

  t = mktime(&unixdate);

  if (date[8] == '-')
    t += date[9] * 3600 + date[10] * 60;
  else
    t -= date[9] * 3600 + date[10] * 60;

  return (t);
}


/*
 * 'ippDelete()' - Delete an IPP request.
 */

void
ippDelete(ipp_t *ipp)		/* I - IPP request */
{
  int			i;	/* Looping var */
  ipp_attribute_t	*attr,	/* Current attribute */
			*next;	/* Next attribute */


  if (ipp == NULL)
    return;

  for (attr = ipp->attrs; attr != NULL; attr = next)
  {
    switch (attr->value_tag)
    {
      case IPP_TAG_TEXT :
      case IPP_TAG_NAME :
      case IPP_TAG_KEYWORD :
      case IPP_TAG_STRING :
      case IPP_TAG_URI :
      case IPP_TAG_URISCHEME :
      case IPP_TAG_CHARSET :
      case IPP_TAG_LANGUAGE :
      case IPP_TAG_MIMETYPE :
          for (i = 0; i < attr->num_values; i ++)
	    free(attr->values[i].string);
	  break;

      case IPP_TAG_TEXTLANG :
      case IPP_TAG_NAMELANG :
          for (i = 0; i < attr->num_values; i ++)
	  {
	    free(attr->values[i].lstring.charset);
	    free(attr->values[i].lstring.string);
	  }
	  break;
    }

    next = attr->next;

    free(attr->name);
    free(attr);
  }

  free(ipp);
}


/*
 * 'ippFindAttribute()' - Find a named attribute in a request...
 */

ipp_attribute_t	*		/* O - Matching attribute */
ippFindAttribute(ipp_t *ipp,	/* I - IPP request */
                 char  *name)	/* I - Name of attribute */
{
  ipp_attribute_t	*attr;	/* Current atttribute */


  if (ipp == NULL || name == NULL)
    return (NULL);

  for (attr = ipp->attrs; attr != NULL; attr = attr->next)
    if (strcmp(attr->name, name) == 0)
      return (attr);

  return (NULL);
}


/*
 * 'ippLength()' - Compute the length of an IPP request.
 */

size_t				/* O - Size of IPP request */
ippLength(ipp_t *ipp)		/* I - IPP request */
{
  int			i;	/* Looping var */
  int			bytes;	/* Number of bytes */
  ipp_attribute_t	*attr;	/* Current attribute */
  ipp_tag_t		group;	/* Current group */


  if (ipp == NULL)
    return (0);

 /*
  * Start with 8 bytes for the IPP request or status header...
  */

  bytes = 8;

 /*
  * Then add the lengths of each attribute...
  */

  group = IPP_TAG_ZERO;

  for (attr = ipp->attrs; attr != NULL; attr = attr->next)
  {
    if (attr->group_tag != group)
    {
      bytes ++;	/* Group tag */
      group = attr->group_tag;
    };

    bytes ++;				/* Value tag */
    bytes += strlen(attr->name);	/* Name */
    bytes += 4 * attr->num_values;	/* Name and value length */

    switch (attr->value_tag)
    {
      case IPP_TAG_INTEGER :
      case IPP_TAG_ENUM :
          bytes += 4 * attr->num_values;
	  break;

      case IPP_TAG_BOOLEAN :
          bytes += attr->num_values;
	  break;

      case IPP_TAG_TEXT :
      case IPP_TAG_NAME :
      case IPP_TAG_KEYWORD :
      case IPP_TAG_STRING :
      case IPP_TAG_URI :
      case IPP_TAG_URISCHEME :
      case IPP_TAG_CHARSET :
      case IPP_TAG_LANGUAGE :
      case IPP_TAG_MIMETYPE :
          for (i = 0; i < attr->num_values; i ++)
	    bytes += strlen(attr->values[i].string);
	  break;

      case IPP_TAG_DATE :
          bytes += 11 * attr->num_values;
	  break;

      case IPP_TAG_RESOLUTION :
          bytes += 9 * attr->num_values;
	  break;

      case IPP_TAG_RANGE :
          bytes += 8 * attr->num_values;
	  break;

      case IPP_TAG_TEXTLANG :
      case IPP_TAG_NAMELANG :
          bytes += 2 * attr->num_values;/* Charset length */
          for (i = 0; i < attr->num_values; i ++)
	    bytes += strlen(attr->values[i].lstring.charset) +
	             strlen((char *)attr->values[i].lstring.string);
	  break;
    }
  }

 /*
  * Finally, add 1 byte for the "end of attributes" tag and return...
  */

  return (bytes + 1);
}


ipp_t *				/* O - New IPP request */
ippNew(void)
{
  return ((ipp_t *)calloc(sizeof(ipp_t), 1));
}


/*
 * 'ippRead()' - Read data for an IPP request.
 */

int
ippRead(http_t *http,
        ipp_t  *ipp)
{
  return (1);
}


/*
 * 'ipp_TimeToDate()' - Convert from UNIX time to RFC 1903 format.
 */

uchar *				/* O - RFC-1903 date/time data */
ipp_TimeToDate(time_t t)	/* I - UNIX time value */
{
  struct tm	*unixdate;	/* UNIX unixdate/time info */
  static uchar	date[11];	/* RFC-1903 date/time data */


 /*
  * RFC-1903 date/time format is:
  *
  *    Byte(s)  Description
  *    -------  -----------
  *    0-1      Year (0 to 65535)
  *    2        Month (1 to 12)
  *    3        Day (1 to 31)
  *    4        Hours (0 to 23)
  *    5        Minutes (0 to 59)
  *    6        Seconds (0 to 60, 60 = "leap second")
  *    7        Deciseconds (0 to 9)
  *    8        +/- UTC
  *    9        UTC hours (0 to 11)
  *    10       UTC minutes (0 to 59)
  */

  unixdate = gmtime(&t);
  unixdate->tm_year += 1900;

  date[0]  = unixdate->tm_year >> 8;
  date[1]  = unixdate->tm_year;
  date[2]  = unixdate->tm_mon + 1;
  date[3]  = unixdate->tm_mday;
  date[4]  = unixdate->tm_hour;
  date[5]  = unixdate->tm_min;
  date[6]  = unixdate->tm_sec;
  date[7]  = 0;
  date[8]  = '+';
  date[9]  = 0;
  date[10] = 0;

  return (date);
}


/*
 * 'ippWrite()' - Write data for an IPP request.
 */

int
ippWrite(http_t *http,
         ipp_t  *ipp)
{
  return (1);
}


/*
 * 'add_attr()' - Add a new attribute to the request.
 */

static ipp_attribute_t *		/* O - New attribute */
add_attr(ipp_t *ipp,			/* I - IPP request */
         int   num_values)		/* I - Number of values */
{
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || num_values < 1)
    return (NULL);

  attr = calloc(sizeof(ipp_attribute_t) +
                (num_values - 1) * sizeof(ipp_value_t), 1);

  if (attr == NULL)
    return (NULL);

  if (ipp->last == NULL)
    ipp->attrs = attr;
  else
    ipp->last->next = attr;

  ipp->last = attr;

  return (attr);
}


/*
 * End of "$Id: ipp.c,v 1.3 1999/01/28 22:00:45 mike Exp $".
 */
