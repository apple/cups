/*
 * "$Id: ipp.c,v 1.55 2001/03/30 19:21:30 mike Exp $"
 *
 *   Internet Printing Protocol support functions for the Common UNIX
 *   Printing System (CUPS).
 *
 *   Copyright 1997-2001 by Easy Software Products, all rights reserved.
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
 * Contents:
 *
 *   ippAddBoolean()     - Add a boolean attribute to an IPP request.
 *   ippAddBooleans()    - Add an array of boolean values.
 *   ippAddDate()        - Add a date attribute to an IPP request.
 *   ippAddInteger()     - Add a integer attribute to an IPP request.
 *   ippAddIntegers()    - Add an array of integer values.
 *   ippAddString()      - Add a language-encoded string to an IPP request.
 *   ippAddStrings()     - Add language-encoded strings to an IPP request.
 *   ippAddRange()       - Add a range of values to an IPP request.
 *   ippAddRanges()      - Add ranges of values to an IPP request.
 *   ippAddResolution()  - Add a resolution value to an IPP request.
 *   ippAddResolutions() - Add resolution values to an IPP request.
 *   ippAddSeparator()   - Add a group separator to an IPP request.
 *   ippDateToTime()     - Convert from RFC 1903 Date/Time format to UNIX time
 *   ippDelete()         - Delete an IPP request.
 *   ippErrorString()    - Return a textual message for the given error message.
 *   ippFindAttribute()  - Find a named attribute in a request...
 *   ippLength()         - Compute the length of an IPP request.
 *   ippNew()            - Allocate a new IPP request.
 *   ippPort()           - Return the default IPP port number.
 *   ippRead()           - Read data for an IPP request.
 *   ippSetPort()        - Set the default port number.
 *   ippTimeToDate()     - Convert from UNIX time to RFC 1903 format.
 *   ippWrite()          - Write data for an IPP request.
 *   _ipp_add_attr()     - Add a new attribute to the request.
 *   _ipp_free_attr()    - Free an attribute.
 *   ipp_read()          - Semi-blocking read on a HTTP connection...
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include "language.h"

#include "ipp.h"
#include "debug.h"
#include <ctype.h>


/*
 * Local globals...
 */

static int	ipp_port = 0;


/*
 * Local functions...
 */

static int	ipp_read(http_t *http, unsigned char *buffer, int length);


/*
 * 'ippAddBoolean()' - Add a boolean attribute to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddBoolean(ipp_t      *ipp,		/* I - IPP request */
              ipp_tag_t  group,		/* I - IPP group */
              const char *name,		/* I - Name of attribute */
              char       value)		/* I - Value of attribute */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddBoolean(%p, %02x, \'%s\', %d)\n", ipp, group, name, value));

  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, 1)) == NULL)
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
ippAddBooleans(ipp_t      *ipp,		/* I - IPP request */
               ipp_tag_t  group,	/* I - IPP group */
	       const char *name,	/* I - Name of attribute */
	       int        num_values,	/* I - Number of values */
	       const char *values)	/* I - Values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddBooleans(%p, %02x, \'%s\', %d, %p)\n", ipp,
                group, name, num_values, values));

  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name      = strdup(name);
  attr->group_tag = group;
  attr->value_tag = IPP_TAG_BOOLEAN;

  if (values != NULL)
    for (i = 0; i < num_values; i ++)
      attr->values[i].boolean = values[i];

  return (attr);
}


/*
 * 'ippAddDate()' - Add a date attribute to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddDate(ipp_t             *ipp,	/* I - IPP request */
           ipp_tag_t         group,	/* I - IPP group */
	   const char        *name,	/* I - Name of attribute */
	   const ipp_uchar_t *value)	/* I - Value */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddDate(%p, %02x, \'%s\', %p)\n", ipp, group, name,
                value));

  if (ipp == NULL || name == NULL || value == NULL)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, 1)) == NULL)
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
ippAddInteger(ipp_t      *ipp,		/* I - IPP request */
              ipp_tag_t  group,		/* I - IPP group */
	      ipp_tag_t  type,		/* I - Type of attribute */
              const char *name,		/* I - Name of attribute */
              int        value)		/* I - Value of attribute */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddInteger(%p, %d, \'%s\', %d)\n", ipp, group, name,
                value));

  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, 1)) == NULL)
    return (NULL);

  attr->name              = strdup(name);
  attr->group_tag         = group;
  attr->value_tag         = type;
  attr->values[0].integer = value;

  return (attr);
}


/*
 * 'ippAddIntegers()' - Add an array of integer values.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddIntegers(ipp_t      *ipp,		/* I - IPP request */
               ipp_tag_t  group,	/* I - IPP group */
	       ipp_tag_t  type,		/* I - Type of attribute */
	       const char *name,	/* I - Name of attribute */
	       int        num_values,	/* I - Number of values */
	       const int  *values)	/* I - Values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name      = strdup(name);
  attr->group_tag = group;
  attr->value_tag = type;

  if (values != NULL)
    for (i = 0; i < num_values; i ++)
      attr->values[i].integer = values[i];

  return (attr);
}


/*
 * 'ippAddString()' - Add a language-encoded string to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddString(ipp_t      *ipp,		/* I - IPP request */
             ipp_tag_t  group,		/* I - IPP group */
	     ipp_tag_t  type,		/* I - Type of attribute */
             const char *name,		/* I - Name of attribute */
             const char *charset,	/* I - Character set */
             const char *value)		/* I - Value */
{
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, 1)) == NULL)
    return (NULL);

  attr->name                      = strdup(name);
  attr->group_tag                 = group;
  attr->value_tag                 = type;
  attr->values[0].string.charset  = charset ? strdup(charset) : NULL;
  attr->values[0].string.text     = value ? strdup(value) : NULL;

  if ((type == IPP_TAG_LANGUAGE || type == IPP_TAG_CHARSET) &&
      attr->values[0].string.text)
  {
   /*
    * Convert to lowercase and change _ to - as needed...
    */

    char *p;


    for (p = attr->values[0].string.text; *p; p ++)
      if (*p == '_')
        *p = '-';
      else
        *p = tolower(*p);
  }

  return (attr);
}


/*
 * 'ippAddStrings()' - Add language-encoded strings to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddStrings(ipp_t      *ipp,		/* I - IPP request */
              ipp_tag_t  group,		/* I - IPP group */
	      ipp_tag_t  type,		/* I - Type of attribute */
	      const char *name,		/* I - Name of attribute */
	      int        num_values,	/* I - Number of values */
	      const char *charset,	/* I - Character set */
	      const char **values)	/* I - Values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name      = strdup(name);
  attr->group_tag = group;
  attr->value_tag = type;

  if (values != NULL)
    for (i = 0; i < num_values; i ++)
    {
      if (i == 0)
	attr->values[0].string.charset = charset ? strdup(charset) : NULL;
      else
	attr->values[i].string.charset = attr->values[0].string.charset;

      attr->values[i].string.text = strdup(values[i]);
    }

  return (attr);
}


/*
 * 'ippAddRange()' - Add a range of values to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddRange(ipp_t      *ipp,		/* I - IPP request */
            ipp_tag_t  group,		/* I - IPP group */
	    const char *name,		/* I - Name of attribute */
	    int        lower,		/* I - Lower value */
	    int        upper)		/* I - Upper value */
{
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, 1)) == NULL)
    return (NULL);

  attr->name                  = strdup(name);
  attr->group_tag             = group;
  attr->value_tag             = IPP_TAG_RANGE;
  attr->values[0].range.lower = lower;
  attr->values[0].range.upper = upper;

  return (attr);
}


/*
 * 'ippAddRanges()' - Add ranges of values to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddRanges(ipp_t      *ipp,		/* I - IPP request */
             ipp_tag_t  group,		/* I - IPP group */
	     const char *name,		/* I - Name of attribute */
	     int        num_values,	/* I - Number of values */
	     const int  *lower,		/* I - Lower values */
	     const int  *upper)		/* I - Upper values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name                  = strdup(name);
  attr->group_tag             = group;
  attr->value_tag             = IPP_TAG_RANGE;

  if (lower != NULL && upper != NULL)
    for (i = 0; i < num_values; i ++)
    {
      attr->values[i].range.lower = lower[i];
      attr->values[i].range.upper = upper[i];
    }

  return (attr);
}


/*
 * 'ippAddResolution()' - Add a resolution value to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddResolution(ipp_t      *ipp,	/* I - IPP request */
        	 ipp_tag_t  group,	/* I - IPP group */
		 const char *name,	/* I - Name of attribute */
		 ipp_res_t  units,	/* I - Units for resolution */
		 int        xres,	/* I - X resolution */
		 int        yres)	/* I - Y resolution */
{
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, 1)) == NULL)
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
 * 'ippAddResolutions()' - Add resolution values to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddResolutions(ipp_t      *ipp,	/* I - IPP request */
        	  ipp_tag_t  group,	/* I - IPP group */
		  const char *name,	/* I - Name of attribute */
		  int        num_values,/* I - Number of values */
		  ipp_res_t  units,	/* I - Units for resolution */
		  const int  *xres,	/* I - X resolutions */
		  const int  *yres)	/* I - Y resolutions */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name                       = strdup(name);
  attr->group_tag                  = group;
  attr->value_tag                  = IPP_TAG_RESOLUTION;

  if (xres != NULL && yres != NULL)
    for (i = 0; i < num_values; i ++)
    {
      attr->values[i].resolution.xres  = xres[i];
      attr->values[i].resolution.yres  = yres[i];
      attr->values[i].resolution.units = units;
    }

  return (attr);
}


/*
 * 'ippAddSeparator()' - Add a group separator to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddSeparator(ipp_t *ipp)		/* I - IPP request */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddSeparator(%p)\n", ipp));

  if (ipp == NULL)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, 0)) == NULL)
    return (NULL);

  attr->group_tag = IPP_TAG_ZERO;
  attr->value_tag = IPP_TAG_ZERO;

  return (attr);
}


/*
 * 'ippDateToTime()' - Convert from RFC 1903 Date/Time format to UNIX time
 *                      in seconds.
 */

time_t					/* O - UNIX time value */
ippDateToTime(const ipp_uchar_t *date)	/* I - RFC 1903 date info */
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
  ipp_attribute_t	*attr,	/* Current attribute */
			*next;	/* Next attribute */


  DEBUG_printf(("ippNew(): %p\n", ipp));

  if (ipp == NULL)
    return;

  for (attr = ipp->attrs; attr != NULL; attr = next)
  {
    next = attr->next;
    _ipp_free_attr(attr);
  }

  free(ipp);
}


/*
 * 'ippErrorString()' - Return a textual message for the given error message.
 */

const char *				/* O - Text string */
ippErrorString(ipp_status_t error)	/* I - Error status */
{
  static char	unknown[255];		/* Unknown error statuses */
  static const char *status_oks[] =	/* "OK" status codes */
		{
		  "successful-ok",
		  "successful-ok-ignored-or-substituted-attributes",
		  "successful-ok-conflicting-attributes",
		  "successful-ok-ignored-subscriptions",
		  "successful-ok-ignored-notifications",
		  "successful-ok-too-many-events",
		  "successful-ok-but-cancel-subscription"
		},
		*status_400s[] =	/* Client errors */
		{
		  "client-error-bad-request",
		  "client-error-forbidden",
		  "client-error-not-authenticated",
		  "client-error-not-authorized",
		  "client-error-not-possible",
		  "client-error-timeout",
		  "client-error-not-found",
		  "client-error-gone",
		  "client-error-request-entity-too-large",
		  "client-error-request-value-too-long",
		  "client-error-document-format-not-supported",
		  "client-error-attributes-or-values-not-supported",
		  "client-error-uri-scheme-not-supported",
		  "client-error-charset-not-supported",
		  "client-error-conflicting-attributes",
		  "client-error-compression-not-supported",
		  "client-error-compression-error",
		  "client-error-document-format-error",
		  "client-error-document-access-error",
		  "client-error-attributes-not-settable",
		  "client-error-ignored-all-subscriptions",
		  "client-error-too-many-subscriptions",
		  "client-error-ignored-all-notifications",
		  "client-error-print-support-file-not-found"
		},
		*status_500s[] =	/* Server errors */
		{
		  "server-error-internal-error",
		  "server-error-operation-not-supported",
		  "server-error-service-unavailable",
		  "server-error-version-not-supported",
		  "server-error-device-error",
		  "server-error-temporary-error",
		  "server-error-not-accepting-jobs",
		  "server-error-busy",
		  "server-error-job-canceled",
		  "server-error-multiple-document-jobs-not-supported",
		  "server-error-printer-is-deactivated"
		};


 /*
  * See if the error code is a known value...
  */

  if (error >= IPP_OK && error <= IPP_OK_BUT_CANCEL_SUBSCRIPTION)
    return (status_oks[error]);
  else if (error == IPP_REDIRECTION_OTHER_SITE)
    return ("redirection-other-site");
  else if (error >= IPP_BAD_REQUEST && error <= IPP_PRINT_SUPPORT_FILE_NOT_FOUND)
    return (status_400s[error - IPP_BAD_REQUEST]);
  else if (error >= IPP_INTERNAL_ERROR && error <= IPP_PRINTER_IS_DEACTIVATED)
    return (status_500s[error - IPP_INTERNAL_ERROR]);

 /*
  * No, build an "unknown-xxxx" error string...
  */

  sprintf(unknown, "unknown-%04x", error);

  return (unknown);
}


/*
 * 'ippFindAttribute()' - Find a named attribute in a request...
 */

ipp_attribute_t	*			/* O - Matching attribute */
ippFindAttribute(ipp_t      *ipp,	/* I - IPP request */
                 const char *name,	/* I - Name of attribute */
		 ipp_tag_t  type)	/* I - Type of attribute */
{
  ipp_attribute_t	*attr;		/* Current atttribute */
  ipp_tag_t		value_tag;	/* Value tag */


  DEBUG_printf(("ippFindAttribute(%p, \'%s\')\n", ipp, name));

  if (ipp == NULL || name == NULL)
    return (NULL);

  for (attr = ipp->attrs; attr != NULL; attr = attr->next)
  {
    DEBUG_printf(("ippFindAttribute: attr = %p, name = \'%s\'\n", attr,
                  attr->name));

    value_tag = attr->value_tag & ~IPP_TAG_COPY;

    if (attr->name != NULL && strcasecmp(attr->name, name) == 0 &&
        (value_tag == type || type == IPP_TAG_ZERO ||
	 (value_tag == IPP_TAG_TEXTLANG && type == IPP_TAG_TEXT) ||
	 (value_tag == IPP_TAG_NAMELANG && type == IPP_TAG_NAME)))
      return (attr);
  }

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
      group = attr->group_tag;
      if (group == IPP_TAG_ZERO)
	continue;

      bytes ++;	/* Group tag */
    }

    DEBUG_printf(("attr->name = %s, attr->num_values = %d, bytes = %d\n",
                  attr->name, attr->num_values, bytes));

    bytes += strlen(attr->name);	/* Name */
    bytes += attr->num_values;		/* Value tag for each value */
    bytes += 2 * attr->num_values;	/* Name lengths */
    bytes += 2 * attr->num_values;	/* Value lengths */

    switch (attr->value_tag & ~IPP_TAG_COPY)
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
	    bytes += strlen(attr->values[i].string.text);
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
          bytes += 4 * attr->num_values;/* Charset + text length */
          for (i = 0; i < attr->num_values; i ++)
	    bytes += strlen(attr->values[i].string.charset) +
	             strlen(attr->values[i].string.text);
	  break;

      default :
          for (i = 0; i < attr->num_values; i ++)
            bytes += attr->values[0].unknown.length;
	  break;
    }
  }

 /*
  * Finally, add 1 byte for the "end of attributes" tag and return...
  */

  DEBUG_printf(("bytes = %d\n", bytes + 1));

  return (bytes + 1);
}


/*
 * 'ippNew()' - Allocate a new IPP request.
 */

ipp_t *				/* O - New IPP request */
ippNew(void)
{
  ipp_t	*temp;			/* New IPP request */


  if ((temp = (ipp_t *)calloc(1, sizeof(ipp_t))) != NULL)
  {
   /*
    * Default to IPP 1.1...
    */

    temp->request.any.version[0] = 1;
    temp->request.any.version[1] = 1;
  }

  DEBUG_printf(("ippNew(): %p\n", temp));

  return (temp);
}


/*
 * 'ippRead()' - Read data for an IPP request.
 */

ipp_state_t			/* O - Current state */
ippRead(http_t *http,		/* I - HTTP data */
        ipp_t  *ipp)		/* I - IPP data */
{
  int			n;		/* Length of data */
  unsigned char		buffer[8192],	/* Data buffer */
			*bufptr;	/* Pointer into buffer */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_tag_t		tag;		/* Current tag */


  DEBUG_printf(("ippRead(%p, %p)\n", http, ipp));

  if (http == NULL || ipp == NULL)
    return (IPP_ERROR);

  switch (ipp->state)
  {
    case IPP_IDLE :
        ipp->state ++; /* Avoid common problem... */

    case IPP_HEADER :
       /*
        * Get the request header...
	*/

        if ((n = ipp_read(http, buffer, 8)) < 8)
	{
	  DEBUG_printf(("ippRead: Unable to read header (%d bytes read)!\n", n));
	  return (n == 0 ? IPP_IDLE : IPP_ERROR);
	}

       /*
        * Verify the major version number...
	*/

	if (buffer[0] != 1)
	{
	  DEBUG_printf(("ippRead: version number (%d.%d) is bad.\n", buffer[0],
	                buffer[1]));
	  return (IPP_ERROR);
	}

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

        DEBUG_printf(("ippRead: version=%d.%d\n", buffer[0], buffer[1]));
	DEBUG_printf(("ippRead: op_status=%04x\n", ipp->request.any.op_status));
	DEBUG_printf(("ippRead: request_id=%d\n", ipp->request.any.request_id));

       /*
        * If blocking is disabled, stop here...
	*/

        if (!http->blocking && http->used == 0)
	  break;

    case IPP_ATTRIBUTE :
        while (ipp_read(http, buffer, 1) > 0)
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

            DEBUG_puts("ippRead: IPP_TAG_END!");

	    ipp->state = IPP_DATA;
	    break;
	  }
          else if (tag < IPP_TAG_UNSUPPORTED_VALUE)
	  {
	   /*
	    * Group tag...  Set the current group and continue...
	    */

            if (ipp->curtag == tag)
	      ippAddSeparator(ipp);

	    ipp->curtag  = tag;
	    ipp->current = NULL;
	    DEBUG_printf(("ippRead: group tag = %x\n", tag));
	    continue;
	  }

          DEBUG_printf(("ippRead: value tag = %x\n", tag));

         /*
	  * Get the name...
	  */

          if (ipp_read(http, buffer, 2) < 2)
	  {
	    DEBUG_puts("ippRead: unable to read name length!");
	    return (IPP_ERROR);
	  }

          n = (buffer[0] << 8) | buffer[1];

          DEBUG_printf(("ippRead: name length = %d\n", n));

          if (n == 0)
	  {
	   /*
	    * More values for current attribute...
	    */

            if (ipp->current == NULL)
	      return (IPP_ERROR);

            attr = ipp->current;

	   /*
	    * Make sure we aren't adding a new value of a different
	    * type...
	    */

	    if (attr->value_tag == IPP_TAG_STRING ||
    		(attr->value_tag >= IPP_TAG_TEXTLANG &&
		 attr->value_tag <= IPP_TAG_MIMETYPE))
            {
	     /*
	      * String values can sometimes come across in different
	      * forms; accept sets of differing values...
	      */

	      if (tag != IPP_TAG_STRING &&
    		  (tag < IPP_TAG_TEXTLANG || tag > IPP_TAG_MIMETYPE))
	        return (IPP_ERROR);
            }
	    else if (attr->value_tag != tag)
	      return (IPP_ERROR);

           /*
	    * Finally, make sure we don't have too many elements in the
	    * attribute array...
	    */

	    if (attr->num_values >= IPP_MAX_VALUES)
	      return (IPP_ERROR);
	  }
	  else
	  {
	   /*
	    * New attribute; read the name and add it...
	    */

	    if (ipp_read(http, buffer, n) < n)
	    {
	      DEBUG_puts("ippRead: unable to read name!");
	      return (IPP_ERROR);
	    }

	    buffer[n] = '\0';
	    DEBUG_printf(("ippRead: name = \'%s\'\n", buffer));

	    attr = ipp->current = _ipp_add_attr(ipp, IPP_MAX_VALUES);

	    attr->group_tag  = ipp->curtag;
	    attr->value_tag  = tag;
	    attr->name       = strdup((char *)buffer);
	    attr->num_values = 0;
	  }

	  if (ipp_read(http, buffer, 2) < 2)
	  {
	    DEBUG_puts("ippRead: unable to read value length!");
	    return (IPP_ERROR);
	  }

	  n = (buffer[0] << 8) | buffer[1];
          DEBUG_printf(("ippRead: value length = %d\n", n));

	  switch (tag)
	  {
	    case IPP_TAG_INTEGER :
	    case IPP_TAG_ENUM :
	        if (ipp_read(http, buffer, 4) < 4)
		  return (IPP_ERROR);

		n = (((((buffer[0] << 8) | buffer[1]) << 8) | buffer[2]) << 8) |
		    buffer[3];

                attr->values[attr->num_values].integer = n;
	        break;
	    case IPP_TAG_BOOLEAN :
	        if (ipp_read(http, buffer, 1) < 1)
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
	        if (ipp_read(http, buffer, n) < n)
		  return (IPP_ERROR);

                buffer[n] = '\0';
		DEBUG_printf(("ippRead: value = \'%s\'\n", buffer));

                attr->values[attr->num_values].string.text = strdup((char *)buffer);
	        break;
	    case IPP_TAG_DATE :
	        if (ipp_read(http, buffer, 11) < 11)
		  return (IPP_ERROR);

                memcpy(attr->values[attr->num_values].date, buffer, 11);
	        break;
	    case IPP_TAG_RESOLUTION :
	        if (ipp_read(http, buffer, 9) < 9)
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
	        if (ipp_read(http, buffer, 8) < 8)
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
	        if (ipp_read(http, buffer, n) < n)
		  return (IPP_ERROR);

                bufptr = buffer;

	       /*
	        * text-with-language and name-with-language are composite
		* values:
		*
		*    charset-length
		*    charset
		*    text-length
		*    text
		*/

		n = (bufptr[0] << 8) | bufptr[1];

                attr->values[attr->num_values].string.charset = calloc(n + 1, 1);

		memcpy(attr->values[attr->num_values].string.charset,
		       bufptr + 2, n);

                bufptr += 2 + n;
		n = (bufptr[0] << 8) | bufptr[1];

                attr->values[attr->num_values].string.text = calloc(n + 1, 1);

		memcpy(attr->values[attr->num_values].string.text,
		       bufptr + 2, n);

	        break;

            default : /* Other unsupported values */
                attr->values[attr->num_values].unknown.length = n;
	        if (n > 0)
		{
		  attr->values[attr->num_values].unknown.data = malloc(n);
	          if (ipp_read(http, attr->values[attr->num_values].unknown.data, n) < n)
		    return (IPP_ERROR);
		}
		else
		  attr->values[attr->num_values].unknown.data = NULL;
	        break;
	  }

          attr->num_values ++;

	 /*
          * If blocking is disabled, stop here...
	  */

          if (!http->blocking && http->used == 0)
	    break;
	}
        break;

    case IPP_DATA :
        break;

    default :
        break; /* anti-compiler-warning-code */
  }

  return (ipp->state);
}


/*
 * 'ippTimeToDate()' - Convert from UNIX time to RFC 1903 format.
 */

const ipp_uchar_t *			/* O - RFC-1903 date/time data */
ippTimeToDate(time_t t)			/* I - UNIX time value */
{
  struct tm		*unixdate;	/* UNIX unixdate/time info */
  static ipp_uchar_t	date[11];	/* RFC-1903 date/time data */


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
  int			i;		/* Looping var */
  int			n;		/* Length of data */
  unsigned char		buffer[8192],	/* Data buffer */
			*bufptr;	/* Pointer into buffer */
  ipp_attribute_t	*attr;		/* Current attribute */


  if (http == NULL || ipp == NULL)
    return (IPP_ERROR);

  switch (ipp->state)
  {
    case IPP_IDLE :
        ipp->state ++; /* Avoid common problem... */

    case IPP_HEADER :
       /*
        * Send the request header...
	*/

        bufptr = buffer;

	*bufptr++ = ipp->request.any.version[0];
	*bufptr++ = ipp->request.any.version[1];
	*bufptr++ = ipp->request.any.op_status >> 8;
	*bufptr++ = ipp->request.any.op_status;
	*bufptr++ = ipp->request.any.request_id >> 24;
	*bufptr++ = ipp->request.any.request_id >> 16;
	*bufptr++ = ipp->request.any.request_id >> 8;
	*bufptr++ = ipp->request.any.request_id;

        if (httpWrite(http, (char *)buffer, bufptr - buffer) < 0)
	{
	  DEBUG_puts("ippWrite: Could not write IPP header...");
	  return (IPP_ERROR);
	}

        ipp->state   = IPP_ATTRIBUTE;
	ipp->current = ipp->attrs;
	ipp->curtag  = IPP_TAG_ZERO;

        DEBUG_printf(("ippWrite: version=%d.%d\n", buffer[0], buffer[1]));
	DEBUG_printf(("ippWrite: op_status=%04x\n", ipp->request.any.op_status));
	DEBUG_printf(("ippWrite: request_id=%d\n", ipp->request.any.request_id));

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

	    ipp->curtag = attr->group_tag;

            if (attr->group_tag == IPP_TAG_ZERO)
	      continue;

            DEBUG_printf(("ippWrite: wrote group tag = %x\n", attr->group_tag));
	    *bufptr++ = attr->group_tag;
	  }

          if ((n = strlen(attr->name)) > (sizeof(buffer) - 3))
	    return (IPP_ERROR);

          DEBUG_printf(("ippWrite: writing value tag = %x\n", attr->value_tag));
          DEBUG_printf(("ippWrite: writing name = %d, \'%s\'\n", n, attr->name));

          *bufptr++ = attr->value_tag;
	  *bufptr++ = n >> 8;
	  *bufptr++ = n;
	  memcpy(bufptr, attr->name, n);
	  bufptr += n;

	  switch (attr->value_tag & ~IPP_TAG_COPY)
	  {
	    case IPP_TAG_INTEGER :
	    case IPP_TAG_ENUM :
	        for (i = 0; i < attr->num_values; i ++)
		{
                  if ((sizeof(buffer) - (bufptr - buffer)) < 9)
		  {
                    if (httpWrite(http, (char *)buffer, bufptr - buffer) < 0)
	            {
	              DEBUG_puts("ippWrite: Could not write IPP attribute...");
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

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
                  if ((sizeof(buffer) - (bufptr - buffer)) < 6)
		  {
                    if (httpWrite(http, (char *)buffer, bufptr - buffer) < 0)
	            {
	              DEBUG_puts("ippWrite: Could not write IPP attribute...");
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

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

        	    DEBUG_printf(("ippWrite: writing value tag = %x\n",
		                  attr->value_tag));
        	    DEBUG_printf(("ippWrite: writing name = 0, \'\'\n"));

                    if ((sizeof(buffer) - (bufptr - buffer)) < 3)
		    {
                      if (httpWrite(http, (char *)buffer, bufptr - buffer) < 0)
	              {
	        	DEBUG_puts("ippWrite: Could not write IPP attribute...");
	        	return (IPP_ERROR);
	              }

		      bufptr = buffer;
		    }

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

                  n = strlen(attr->values[i].string.text);

                  if (n > sizeof(buffer))
		    return (IPP_ERROR);

                  DEBUG_printf(("ippWrite: writing string = %d, \'%s\'\n", n,
		                attr->values[i].string.text));

                  if ((sizeof(buffer) - (bufptr - buffer)) < (n + 2))
		  {
                    if (httpWrite(http, (char *)buffer, bufptr - buffer) < 0)
	            {
	              DEBUG_puts("ippWrite: Could not write IPP attribute...");
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

	          *bufptr++ = n >> 8;
		  *bufptr++ = n;
		  memcpy(bufptr, attr->values[i].string.text, n);
		  bufptr += n;
		}
		break;

	    case IPP_TAG_DATE :
	        for (i = 0; i < attr->num_values; i ++)
		{
                  if ((sizeof(buffer) - (bufptr - buffer)) < 16)
		  {
                    if (httpWrite(http, (char *)buffer, bufptr - buffer) < 0)
	            {
	              DEBUG_puts("ippWrite: Could not write IPP attribute...");
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

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
                  if ((sizeof(buffer) - (bufptr - buffer)) < 14)
		  {
                    if (httpWrite(http, (char *)buffer, bufptr - buffer) < 0)
	            {
	              DEBUG_puts("ippWrite: Could not write IPP attribute...");
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

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
                  if ((sizeof(buffer) - (bufptr - buffer)) < 13)
		  {
                    if (httpWrite(http, (char *)buffer, bufptr - buffer) < 0)
	            {
	              DEBUG_puts("ippWrite: Could not write IPP attribute...");
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

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

                    if ((sizeof(buffer) - (bufptr - buffer)) < 3)
		    {
                      if (httpWrite(http, (char *)buffer, bufptr - buffer) < 0)
	              {
	        	DEBUG_puts("ippWrite: Could not write IPP attribute...");
	        	return (IPP_ERROR);
	              }

		      bufptr = buffer;
		    }

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

                  n = strlen(attr->values[i].string.charset) +
		      strlen(attr->values[i].string.text) +
		      4;

                  if (n > sizeof(buffer))
		    return (IPP_ERROR);

                  if ((sizeof(buffer) - (bufptr - buffer)) < (n + 2))
		  {
                    if (httpWrite(http, (char *)buffer, bufptr - buffer) < 0)
	            {
	              DEBUG_puts("ippWrite: Could not write IPP attribute...");
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

                 /* Length of entire value */
	          *bufptr++ = n >> 8;
		  *bufptr++ = n;

                 /* Length of charset */
                  n = strlen(attr->values[i].string.charset);
	          *bufptr++ = n >> 8;
		  *bufptr++ = n;

                 /* Charset */
		  memcpy(bufptr, attr->values[i].string.charset, n);
		  bufptr += n;

                 /* Length of text */
                  n = strlen(attr->values[i].string.text);
	          *bufptr++ = n >> 8;
		  *bufptr++ = n;

                 /* Text */
		  memcpy(bufptr, attr->values[i].string.text, n);
		  bufptr += n;
		}
		break;

            default :
	        for (i = 0; i < attr->num_values; i ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    if ((sizeof(buffer) - (bufptr - buffer)) < 3)
		    {
                      if (httpWrite(http, (char *)buffer, bufptr - buffer) < 0)
	              {
	        	DEBUG_puts("ippWrite: Could not write IPP attribute...");
	        	return (IPP_ERROR);
	              }

		      bufptr = buffer;
		    }

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

                  n = attr->values[i].unknown.length;

                  if (n > sizeof(buffer))
		    return (IPP_ERROR);

                  if ((sizeof(buffer) - (bufptr - buffer)) < (n + 2))
		  {
                    if (httpWrite(http, (char *)buffer, bufptr - buffer) < 0)
	            {
	              DEBUG_puts("ippWrite: Could not write IPP attribute...");
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

                 /* Length of unknown value */
	          *bufptr++ = n >> 8;
		  *bufptr++ = n;

                 /* Value */
		  if (n > 0)
		  {
		    memcpy(bufptr, attr->values[i].unknown.data, n);
		    bufptr += n;
		  }
		}
		break;
	  }

         /*
	  * Write the data out...
	  */

          if (httpWrite(http, (char *)buffer, bufptr - buffer) < 0)
	  {
	    DEBUG_puts("ippWrite: Could not write IPP attribute...");
	    return (IPP_ERROR);
	  }

          DEBUG_printf(("ippWrite: wrote %d bytes\n", bufptr - buffer));

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
	  if (httpWrite(http, (char *)buffer, 1) < 0)
	  {
	    DEBUG_puts("ippWrite: Could not write IPP end-tag...");
	    return (IPP_ERROR);
	  }

	  ipp->state = IPP_DATA;
	}
        break;

    case IPP_DATA :
        break;

    default :
        break; /* anti-compiler-warning-code */
  }

  return (ipp->state);
}


/*
 * 'ippPort()' - Return the default IPP port number.
 */

int				/* O - Port number */
ippPort(void)
{
  const char	*server_port;	/* SERVER_PORT environment variable */
  struct servent *port;		/* Port number info */  


  if (ipp_port)
    return (ipp_port);
  else if ((server_port = getenv("IPP_PORT")) != NULL)
    return (ipp_port = atoi(server_port));
  else if ((port = getservbyname("ipp", NULL)) == NULL)
    return (ipp_port = IPP_PORT);
  else
    return (ipp_port = ntohs(port->s_port));
}


/*
 * 'ippSetPort()' - Set the default port number.
 */

void
ippSetPort(int p)		/* I - Port number to use */
{
  ipp_port = p;
}


/*
 * '_ipp_add_attr()' - Add a new attribute to the request.
 */

ipp_attribute_t *		/* O - New attribute */
_ipp_add_attr(ipp_t *ipp,	/* I - IPP request */
              int   num_values)	/* I - Number of values */
{
  ipp_attribute_t	*attr;	/* New attribute */


  DEBUG_printf(("_ipp_add_attr(%p, %d)\n", ipp, num_values));

  if (ipp == NULL || num_values < 0)
    return (NULL);

  attr = calloc(sizeof(ipp_attribute_t) +
                (num_values - 1) * sizeof(ipp_value_t), 1);

  attr->num_values = num_values;

  if (attr == NULL)
    return (NULL);

  if (ipp->last == NULL)
    ipp->attrs = attr;
  else
    ipp->last->next = attr;

  ipp->last = attr;

  DEBUG_printf(("_ipp_add_attr(): %p\n", attr));

  return (attr);
}


/*
 * '_ipp_free_attr()' - Free an attribute.
 */

void
_ipp_free_attr(ipp_attribute_t *attr)	/* I - Attribute to free */
{
  int	i;				/* Looping var */


  DEBUG_printf(("_ipp_free_attr(): %p\n", attr));

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
	  free(attr->values[i].string.text);
	break;

    case IPP_TAG_TEXTLANG :
    case IPP_TAG_NAMELANG :
        for (i = 0; i < attr->num_values; i ++)
	{
	  if (attr->values[i].string.charset && i == 0)
	    free(attr->values[i].string.charset);
	  free(attr->values[i].string.text);
	}
	break;

    default :
        break; /* anti-compiler-warning-code */
  }

  if (attr->name != NULL)
    free(attr->name);

  free(attr);
}


/*
 * 'ipp_read()' - Semi-blocking read on a HTTP connection...
 */

static int			/* O - Number of bytes read */
ipp_read(http_t        *http,	/* I - Client connection */
         unsigned char *buffer,	/* O - Buffer for data */
	 int           length)	/* I - Total length */
{
  int	tbytes,			/* Total bytes read */
	bytes;			/* Bytes read this pass */


 /*
  * Loop until all bytes are read...
  */

  for (tbytes = 0, bytes = 0; tbytes < length; tbytes += bytes, buffer += bytes)
    if ((bytes = httpRead(http, (char *)buffer, length - tbytes)) <= 0)
      break;

 /*
  * Return the number of bytes read...
  */

  if (tbytes == 0 && bytes < 0)
    return (-1);
  else
    return (tbytes);
}


/*
 * End of "$Id: ipp.c,v 1.55 2001/03/30 19:21:30 mike Exp $".
 */
