/*
 * "$Id: ipp.c,v 1.4 1999/02/05 17:40:52 mike Exp $"
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
 *   ippTimeToDate()    - Convert from UNIX time to RFC 1903 format.
 *   ippWrite()         - Write data for an IPP request.
 *   add_attr()         - Add a new attribute to the request.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

ipp_state_t			/* O - Current state */
ippRead(http_t *http,		/* I - HTTP data */
        ipp_t  *ipp)		/* I - IPP data */
{
  int			n;		/* Length of data */
  char			buffer[8192];	/* Data buffer */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_tag_t		tag;		/* Current tag */


  if (http == NULL || ipp == NULL)
    return (IPP_ERROR);

  switch (ipp->state)
  {
    case IPP_IDLE :
        break;

    case IPP_HEADER :
       /*
        * Get the request header...
	*/

        if (httpRead(http, buffer, 8) < 8)
	  return (IPP_ERROR);

       /*
        * Verify the major version number...
	*/

	if (buffer[0] != 1)
	  return (IPP_ERROR);

       /*
        * Then copy the request header over...
	*/

        ipp->request.any.version[0]  = buffer[0];
        ipp->request.any.version[1]  = buffer[1];
        ipp->request.any.op_status   = (buffer[2] << 8) | buffer[3];
        ipp->request.any.request_id  = (((((buffer[4] << 8) | buffer[5]) << 8) |
	                               buffer[6]) << 8) | buffer[7];

        ipp->state   = IPP_ATTRIBUTE;
	ipp->current = NULL;
	ipp->curtag  = IPP_TAG_ZERO;

       /*
        * If blocking is disabled, stop here...
	*/

        if (!http->blocking)
	  break;

    case IPP_ATTRIBUTE :
        while (httpRead(http, buffer, 1) > 0)
	{
	 /*
	  * Read this attribute...
	  */

          tag = (ipp_tag_t)buffer[0];

	  if (tag == IPP_TAG_END)
	  {
	   /*
	    * No more attributes left...
	    */

	    ipp->state = IPP_DATA;
	    break;
	  }
          else if (tag < IPP_TAG_UNSUPPORTED)
	  {
	   /*
	    * Group tag...  Set the current group and continue...
	    */

	    ipp->curtag  = tag;
	    ipp->current = NULL;
	    continue;
	  }

         /*
	  * Get the name...
	  */

          if (httpRead(http, buffer, 2) < 2)
	    return (IPP_ERROR);

          n = (buffer[0] << 8) | buffer[1];

          if (n == 0)
	  {
	   /*
	    * More values for current attribute...
	    */

            if (ipp->current == NULL)
	      return (IPP_ERROR);

            attr = ipp->current;

	    if (attr->num_values >= IPP_MAX_VALUES)
	      return (IPP_ERROR);
	  }
	  else
	  {
	   /*
	    * New attribute; read the name and add it...
	    */

	    if (httpRead(http, buffer, n) < n)
	      return (IPP_ERROR);

	    buffer[n] = '\0';
	    attr = ipp->current = add_attr(ipp, IPP_MAX_VALUES);

	    attr->group_tag = ipp->curtag;
	    attr->value_tag = tag;
	    attr->name      = strdup(buffer);
	  }

	  switch (tag)
	  {
	    case IPP_TAG_INTEGER :
	    case IPP_TAG_ENUM :
	        if (httpRead(http, buffer, 4) < 4)
		  return (IPP_ERROR);

		n = (((((buffer[0] << 8) | buffer[1]) << 8) | buffer[2]) << 8) |
		    buffer[3];

                attr->values[attr->num_values].integer = n;
	        break;
	    case IPP_TAG_BOOLEAN :
	        if (httpRead(http, buffer, 1) < 4)
		  return (IPP_ERROR);

                attr->values[attr->num_values].boolean = buffer[0];
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
	        if (httpRead(http, buffer, 2) < 2)
		  return (IPP_ERROR);

		n = (buffer[0] << 8) | buffer[1];

	        if (httpRead(http, buffer, n) < n)
		  return (IPP_ERROR);

                buffer[n] = '\0';

                attr->values[attr->num_values].string = strdup(buffer);
	        break;
	    case IPP_TAG_DATE :
	        if (httpRead(http, buffer, 11) < 11)
		  return (IPP_ERROR);

                memcpy(attr->values[attr->num_values].date, buffer, 11);
	        break;
	    case IPP_TAG_RESOLUTION :
	        if (httpRead(http, buffer, 9) < 9)
		  return (IPP_ERROR);

                attr->values[attr->num_values].resolution.xres =
		    (((((buffer[0] << 8) | buffer[1]) << 8) | buffer[2]) << 8) |
		    buffer[3];
                attr->values[attr->num_values].resolution.yres =
		    (((((buffer[4] << 8) | buffer[5]) << 8) | buffer[6]) << 8) |
		    buffer[7];
                attr->values[attr->num_values].resolution.units =
		    (ipp_res_t)buffer[8];
	        break;
	    case IPP_TAG_RANGE :
	        if (httpRead(http, buffer, 8) < 8)
		  return (IPP_ERROR);

                attr->values[attr->num_values].range.lower =
		    (((((buffer[0] << 8) | buffer[1]) << 8) | buffer[2]) << 8) |
		    buffer[3];
                attr->values[attr->num_values].range.upper =
		    (((((buffer[4] << 8) | buffer[5]) << 8) | buffer[6]) << 8) |
		    buffer[7];
	        break;
	    case IPP_TAG_TEXTLANG :
	    case IPP_TAG_NAMELANG :
	        if (httpRead(http, buffer, 2) < 2)
		  return (IPP_ERROR);

		n = (buffer[0] << 8) | buffer[1];

	        if (httpRead(http, buffer, n) < n)
		  return (IPP_ERROR);

                buffer[n] = '\0';

                attr->values[attr->num_values].lstring.charset = strdup(buffer);

	        if (httpRead(http, buffer, 2) < 2)
		  return (IPP_ERROR);

		n = (buffer[0] << 8) | buffer[1];

	        if (httpRead(http, buffer, n) < n)
		  return (IPP_ERROR);

                buffer[n] = '\0';

                attr->values[attr->num_values].lstring.string =
		    (unsigned char *)strdup(buffer);
	        break;
	  }

          attr->num_values ++;

	 /*
          * If blocking is disabled, stop here...
	  */

          if (!http->blocking)
	    break;
	}
        break;

    case IPP_DATA :
        break;
  }

  return (ipp->state);
}


/*
 * 'ippTimeToDate()' - Convert from UNIX time to RFC 1903 format.
 */

uchar *				/* O - RFC-1903 date/time data */
ippTimeToDate(time_t t)		/* I - UNIX time value */
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

ipp_state_t			/* O - Current state */
ippWrite(http_t *http,		/* I - HTTP data */
         ipp_t  *ipp)		/* I - IPP data */
{
  int			i;	/* Looping var */
  int			n;	/* Length of data */
  char			buffer[8192],	/* Data buffer */
			*bufptr;/* Pointer into buffer */
  ipp_attribute_t	*attr;	/* Current attribute */


  if (http == NULL || ipp == NULL)
    return (IPP_ERROR);

  switch (ipp->state)
  {
    case IPP_IDLE :
        break;

    case IPP_HEADER :
       /*
        * Send the request header...
	*/

        bufptr = buffer;

	*bufptr++ = 1;
	*bufptr++ = 0;
	*bufptr++ = ipp->request.any.op_status >> 8;
	*bufptr++ = ipp->request.any.op_status;
	*bufptr++ = ipp->request.any.request_id >> 24;
	*bufptr++ = ipp->request.any.request_id >> 16;
	*bufptr++ = ipp->request.any.request_id >> 8;
	*bufptr++ = ipp->request.any.request_id;

        if (httpWrite(http, buffer, bufptr - buffer) < 0)
	  return (IPP_ERROR);

        ipp->state   = IPP_ATTRIBUTE;
	ipp->current = ipp->attrs;
	ipp->curtag  = IPP_TAG_ZERO;

       /*
        * If blocking is disabled, stop here...
	*/

        if (!http->blocking)
	  break;

    case IPP_ATTRIBUTE :
        while (ipp->current != NULL)
	{
	 /*
	  * Write this attribute...
	  */

	  bufptr = buffer;
	  attr   = ipp->current;

	  ipp->current = ipp->current->next;

          if (ipp->curtag != attr->group_tag)
	  {
	   /*
	    * Send a group operation tag...
	    */

	    *bufptr++   = attr->group_tag;
	    ipp->curtag = attr->group_tag;
	  }

          n = strlen(attr->name);
          *bufptr++ = attr->value_tag;
	  *bufptr++ = n >> 8;
	  *bufptr++ = n;
	  memcpy(bufptr, attr->name, n);
	  bufptr += n;

	  switch (attr->value_tag)
	  {
	    case IPP_TAG_INTEGER :
	    case IPP_TAG_ENUM :
	        for (i = 0; i < attr->num_values; i ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

	          *bufptr++ = 0;
		  *bufptr++ = 4;
		  *bufptr++ = attr->values[i].integer >> 24;
		  *bufptr++ = attr->values[i].integer >> 16;
		  *bufptr++ = attr->values[i].integer >> 8;
		  *bufptr++ = attr->values[i].integer;
		}
		break;

	    case IPP_TAG_BOOLEAN :
	        for (i = 0; i < attr->num_values; i ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

	          *bufptr++ = 0;
		  *bufptr++ = 1;
		  *bufptr++ = attr->values[i].boolean;
		}
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
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

                  n = strlen(attr->values[i].string);
	          *bufptr++ = n >> 8;
		  *bufptr++ = n;
		  memcpy(bufptr, attr->values[i].string, n);
		  bufptr += n;
		}
		break;

	    case IPP_TAG_DATE :
	        for (i = 0; i < attr->num_values; i ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

	          *bufptr++ = 0;
		  *bufptr++ = 11;
		  memcpy(bufptr, attr->values[i].date, 11);
		  bufptr += 11;
		}
		break;

	    case IPP_TAG_RESOLUTION :
	        for (i = 0; i < attr->num_values; i ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

	          *bufptr++ = 0;
		  *bufptr++ = 9;
		  *bufptr++ = attr->values[i].resolution.xres >> 24;
		  *bufptr++ = attr->values[i].resolution.xres >> 16;
		  *bufptr++ = attr->values[i].resolution.xres >> 8;
		  *bufptr++ = attr->values[i].resolution.xres;
		  *bufptr++ = attr->values[i].resolution.yres >> 24;
		  *bufptr++ = attr->values[i].resolution.yres >> 16;
		  *bufptr++ = attr->values[i].resolution.yres >> 8;
		  *bufptr++ = attr->values[i].resolution.yres;
		  *bufptr++ = attr->values[i].resolution.units;
		}
		break;

	    case IPP_TAG_RANGE :
	        for (i = 0; i < attr->num_values; i ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

	          *bufptr++ = 0;
		  *bufptr++ = 8;
		  *bufptr++ = attr->values[i].range.lower >> 24;
		  *bufptr++ = attr->values[i].range.lower >> 16;
		  *bufptr++ = attr->values[i].range.lower >> 8;
		  *bufptr++ = attr->values[i].range.lower;
		  *bufptr++ = attr->values[i].range.upper >> 24;
		  *bufptr++ = attr->values[i].range.upper >> 16;
		  *bufptr++ = attr->values[i].range.upper >> 8;
		  *bufptr++ = attr->values[i].range.upper;
		}
		break;

	    case IPP_TAG_TEXTLANG :
	    case IPP_TAG_NAMELANG :
	        for (i = 0; i < attr->num_values; i ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

                  n = strlen(attr->values[i].lstring.charset);
	          *bufptr++ = n >> 8;
		  *bufptr++ = n;
		  memcpy(bufptr, attr->values[i].lstring.charset, n);
		  bufptr += n;

                  n = strlen((char *)attr->values[i].lstring.string);
	          *bufptr++ = n >> 8;
		  *bufptr++ = n;
		  memcpy(bufptr, attr->values[i].lstring.string, n);
		  bufptr += n;
		}
		break;
	  }

         /*
	  * Write the data out...
	  */

          if (httpWrite(http, buffer, bufptr - buffer) < 0)
	    return (IPP_ERROR);

	 /*
          * If blocking is disabled, stop here...
	  */

          if (!http->blocking)
	    break;
	}

	if (ipp->current == NULL)
	{
         /*
	  * Done with all of the attributes; add the end-of-attributes tag...
	  */

          buffer[0] = IPP_TAG_END;
	  if (httpWrite(http, buffer, 1) < 0)
	    return (IPP_ERROR);

	  ipp->state = IPP_DATA;
	}
        break;

    case IPP_DATA :
        break;
  }

  return (ipp->state);
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
 * End of "$Id: ipp.c,v 1.4 1999/02/05 17:40:52 mike Exp $".
 */
