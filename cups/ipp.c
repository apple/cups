/*
 * "$Id$"
 *
 *   Internet Printing Protocol support functions for the Common UNIX
 *   Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
 *   ippAddBoolean()        - Add a boolean attribute to an IPP request.
 *   ippAddBooleans()       - Add an array of boolean values.
 *   ippAddDate()           - Add a date attribute to an IPP request.
 *   ippAddInteger()        - Add a integer attribute to an IPP request.
 *   ippAddIntegers()       - Add an array of integer values.
 *   ippAddString()         - Add a language-encoded string to an IPP request.
 *   ippAddStrings()        - Add language-encoded strings to an IPP request.
 *   ippAddRange()          - Add a range of values to an IPP request.
 *   ippAddRanges()         - Add ranges of values to an IPP request.
 *   ippAddResolution()     - Add a resolution value to an IPP request.
 *   ippAddResolutions()    - Add resolution values to an IPP request.
 *   ippAddSeparator()      - Add a group separator to an IPP request.
 *   ippDateToTime()        - Convert from RFC 1903 Date/Time format to
 *                            UNIX time in seconds.
 *   ippDelete()            - Delete an IPP request.
 *   ippDeleteAttribute()   - Delete a single attribute in an IPP request.
 *   ippFindAttribute()     - Find a named attribute in a request...
 *   ippFindNextAttribute() - Find the next named attribute in a request...
 *   ippLength()            - Compute the length of an IPP request.
 *   ippNew()               - Allocate a new IPP request.
 *   ippRead()              - Read data for an IPP request from a HTTP
 *                            connection.
 *   ippReadFile()          - Read data for an IPP request from a file.
 *   ippReadIO()            - Read data for an IPP request.
 *   ippTimeToDate()        - Convert from UNIX time to RFC 1903 format.
 *   ippWrite()             - Write data for an IPP request to a HTTP
 *                            connection.
 *   ippWriteFile()         - Write data for an IPP request to a file.
 *   ippWriteIO()           - Write data for an IPP request.
 *   _ipp_add_attr()        - Add a new attribute to the request.
 *   _ipp_free_attr()       - Free an attribute.
 *   ipp_length()           - Compute the length of an IPP request or
 *                            collection value.
 *   ipp_read_http()        - Semi-blocking read on a HTTP connection...
 *   ipp_read_file()        - Read IPP data from a file.
 *   ipp_write_file()       - Write IPP data to a file.
 */

/*
 * Include necessary headers...
 */

#include "globals.h"
#include "debug.h"
#include <stdlib.h>
#include <errno.h>
#ifdef WIN32
#  include <io.h>
#endif /* WIN32 */


/*
 * Local functions...
 */

static size_t		ipp_length(ipp_t *ipp, int collection);
static int		ipp_read_http(http_t *http, ipp_uchar_t *buffer, int length);
static int		ipp_read_file(int *fd, ipp_uchar_t *buffer, int length);
static int		ipp_write_file(int *fd, ipp_uchar_t *buffer, int length);


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
  ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("ippAddBooleans(%p, %02x, \'%s\', %d, %p)\n", ipp,
                group, name, num_values, values));

  if (ipp == NULL || name == NULL || num_values < 1)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name      = strdup(name);
  attr->group_tag = group;
  attr->value_tag = IPP_TAG_BOOLEAN;

  if (values != NULL)
    for (i = 0, value = attr->values;
	 i < num_values;
	 i ++, value ++)
      value->boolean = values[i];

  return (attr);
}


/*
 * 'ippAddCollection()' - Add a collection value.
 *
 * @since CUPS 1.1.19@
 */

ipp_attribute_t *			/* O - New attribute */
ippAddCollection(ipp_t      *ipp,	/* I - IPP request */
                 ipp_tag_t  group,	/* I - IPP group */
		 const char *name,	/* I - Name of attribute */
		 ipp_t      *value)	/* I - Value */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddCollection(%p, %02x, \'%s\', %p)\n", ipp, group, name,
                value));

  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, 1)) == NULL)
    return (NULL);

  attr->name                 = strdup(name);
  attr->group_tag            = group;
  attr->value_tag            = IPP_TAG_BEGIN_COLLECTION;
  attr->values[0].collection = value;

  return (attr);
}


/*
 * 'ippAddCollections()' - Add an array of collection values.
 *
 * @since CUPS 1.1.19@
 */

ipp_attribute_t *			/* O - New attribute */
ippAddCollections(
    ipp_t       *ipp,			/* I - IPP request */
    ipp_tag_t   group,			/* I - IPP group */
    const char  *name,			/* I - Name of attribute */
    int         num_values,		/* I - Number of values */
    const ipp_t **values)		/* I - Values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */
  ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("ippAddCollections(%p, %02x, \'%s\', %d, %p)\n", ipp,
                group, name, num_values, values));

  if (ipp == NULL || name == NULL || num_values < 1)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name      = strdup(name);
  attr->group_tag = group;
  attr->value_tag = IPP_TAG_BEGIN_COLLECTION;

  if (values != NULL)
    for (i = 0, value = attr->values;
	 i < num_values;
	 i ++, value ++)
      value->collection = (ipp_t *)values[i];

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
  ipp_value_t		*value;		/* Current value */


  if (ipp == NULL || name == NULL || num_values < 1)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name      = strdup(name);
  attr->group_tag = group;
  attr->value_tag = type;

  if (values != NULL)
    for (i = 0, value = attr->values;
	 i < num_values;
	 i ++, value ++)
      value->integer = values[i];

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

 /*
  * Force value to be English for the POSIX locale...
  */

  if (type == IPP_TAG_LANGUAGE && strcasecmp(value, "C") == 0)
    value = "en";

 /*
  * Initialize the attribute data...
  */

  attr->name                      = strdup(name);
  attr->group_tag                 = group;
  attr->value_tag                 = type;
  attr->values[0].string.charset  = ((int)type & IPP_TAG_COPY) ? (char *)charset :
                                    charset ? strdup(charset) : NULL;
  attr->values[0].string.text     = ((int)type & IPP_TAG_COPY) ? (char *)value :
                                    value ? strdup(value) : NULL;

 /*
  * Convert language values to lowercase and change _ to - as needed...
  */

  if ((type == IPP_TAG_LANGUAGE || type == IPP_TAG_CHARSET) &&
      attr->values[0].string.text)
  {
    char *p;


    for (p = attr->values[0].string.text; *p; p ++)
      if (*p == '_')
        *p = '-';
      else
        *p = tolower(*p & 255);
  }

  return (attr);
}


/*
 * 'ippAddStrings()' - Add language-encoded strings to an IPP request.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddStrings(
    ipp_t              *ipp,		/* I - IPP request */
    ipp_tag_t          group,		/* I - IPP group */
    ipp_tag_t          type,		/* I - Type of attribute */
    const char         *name,		/* I - Name of attribute */
    int                num_values,	/* I - Number of values */
    const char         *charset,	/* I - Character set */
    const char * const *values)		/* I - Values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */
  ipp_value_t		*value;		/* Current value */


  if (ipp == NULL || name == NULL || num_values < 1)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, num_values)) == NULL)
    return (NULL);

 /*
  * Initialize the attribute data...
  */

  attr->name      = strdup(name);
  attr->group_tag = group;
  attr->value_tag = type;

  for (i = 0, value = attr->values;
       i < num_values;
       i ++, value ++)
  {
    if (i == 0)
      value->string.charset = ((int)type & IPP_TAG_COPY) ? (char *)charset :
                                   charset ? strdup(charset) : NULL;
    else
      value->string.charset = attr->values[0].string.charset;

    if (values != NULL)
    {
     /*
      * Force language to be English for the POSIX locale...
      */

      if (type == IPP_TAG_LANGUAGE && strcasecmp(values[i], "C") == 0)
	value->string.text = ((int)type & IPP_TAG_COPY) ? "en" :
                                      strdup("en");
      else
	value->string.text = ((int)type & IPP_TAG_COPY) ? (char *)values[i] :
                                      strdup(values[i]);
    }
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
  ipp_value_t		*value;		/* Current value */


  if (ipp == NULL || name == NULL || num_values < 1)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name      = strdup(name);
  attr->group_tag = group;
  attr->value_tag = IPP_TAG_RANGE;

  if (lower != NULL && upper != NULL)
    for (i = 0, value = attr->values;
	 i < num_values;
	 i ++, value ++)
    {
      value->range.lower = lower[i];
      value->range.upper = upper[i];
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
  ipp_value_t		*value;		/* Current value */


  if (ipp == NULL || name == NULL || num_values < 1)
    return (NULL);

  if ((attr = _ipp_add_attr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name      = strdup(name);
  attr->group_tag = group;
  attr->value_tag = IPP_TAG_RESOLUTION;

  if (xres != NULL && yres != NULL)
    for (i = 0, value = attr->values;
	 i < num_values;
	 i ++, value ++)
    {
      value->resolution.xres  = xres[i];
      value->resolution.yres  = yres[i];
      value->resolution.units = units;
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
 *                     in seconds.
 */

time_t					/* O - UNIX time value */
ippDateToTime(const ipp_uchar_t *date)	/* I - RFC 1903 date info */
{
  struct tm	unixdate;		/* UNIX date/time info */
  time_t	t;			/* Computed time */


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
ippDelete(ipp_t *ipp)			/* I - IPP request */
{
  ipp_attribute_t	*attr,		/* Current attribute */
			*next;		/* Next attribute */


  DEBUG_printf(("ippDelete(): %p\n", ipp));

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
 * 'ippDeleteAttribute()' - Delete a single attribute in an IPP request.
 *
 * @since CUPS 1.1.19@
 */

void
ippDeleteAttribute(
    ipp_t           *ipp,		/* I - IPP request */
    ipp_attribute_t *attr)		/* I - Attribute to delete */
{
  ipp_attribute_t	*current,	/* Current attribute */
			*prev;		/* Previous attribute */


 /*
  * Find the attribute in the list...
  */

  for (current = ipp->attrs, prev = NULL;
       current != NULL && current != attr;
       prev = current, current = current->next);

  if (current)
  {
   /*
    * Found it, remove the attribute from the list...
    */

    if (prev)
      prev->next = current->next;
    else
      ipp->attrs = current->next;

    if (current == ipp->last)
      ipp->last = prev;

   /*
    * Free memory used by the attribute...
    */

    _ipp_free_attr(current);
  }
}


/*
 * 'ippFindAttribute()' - Find a named attribute in a request...
 */

ipp_attribute_t	*			/* O - Matching attribute */
ippFindAttribute(ipp_t      *ipp,	/* I - IPP request */
                 const char *name,	/* I - Name of attribute */
		 ipp_tag_t  type)	/* I - Type of attribute */
{
  DEBUG_printf(("ippFindAttribute(%p, \'%s\')\n", ipp, name));

  if (ipp == NULL || name == NULL)
    return (NULL);

 /*
  * Reset the current pointer...
  */

  ipp->current = NULL;

 /*
  * Search for the attribute...
  */

  return (ippFindNextAttribute(ipp, name, type));
}


/*
 * 'ippFindNextAttribute()' - Find the next named attribute in a request...
 */

ipp_attribute_t	*			/* O - Matching attribute */
ippFindNextAttribute(ipp_t      *ipp,	/* I - IPP request */
                     const char *name,	/* I - Name of attribute */
		     ipp_tag_t  type)	/* I - Type of attribute */
{
  ipp_attribute_t	*attr;		/* Current atttribute */
  ipp_tag_t		value_tag;	/* Value tag */


  DEBUG_printf(("ippFindNextAttribute(%p, \'%s\')\n", ipp, name));

  if (ipp == NULL || name == NULL)
    return (NULL);

  if (ipp->current)
  {
    ipp->prev = ipp->current;
    attr      = ipp->current->next;
  }
  else
  {
    ipp->prev = NULL;
    attr      = ipp->attrs;
  }

  for (; attr != NULL; ipp->prev = attr, attr = attr->next)
  {
    DEBUG_printf(("ippFindAttribute: attr = %p, name = \'%s\'\n", attr,
                  attr->name));

    value_tag = (ipp_tag_t)(attr->value_tag & IPP_TAG_MASK);

    if (attr->name != NULL && strcasecmp(attr->name, name) == 0 &&
        (value_tag == type || type == IPP_TAG_ZERO ||
	 (value_tag == IPP_TAG_TEXTLANG && type == IPP_TAG_TEXT) ||
	 (value_tag == IPP_TAG_NAMELANG && type == IPP_TAG_NAME)))
    {
      ipp->current = attr;

      return (attr);
    }
  }

  ipp->current = NULL;
  ipp->prev    = NULL;

  return (NULL);
}


/*
 * 'ippLength()' - Compute the length of an IPP request.
 */

size_t					/* O - Size of IPP request */
ippLength(ipp_t *ipp)			/* I - IPP request */
{
  return (ipp_length(ipp, 0));
}


/*
 * 'ippNew()' - Allocate a new IPP request.
 */

ipp_t *					/* O - New IPP request */
ippNew(void)
{
  ipp_t	*temp;				/* New IPP request */


  DEBUG_puts("ippNew()");

  if ((temp = (ipp_t *)calloc(1, sizeof(ipp_t))) != NULL)
  {
   /*
    * Default to IPP 1.1...
    */

    temp->request.any.version[0] = 1;
    temp->request.any.version[1] = 1;
  }

  DEBUG_printf(("ippNew: %p\n", temp));

  return (temp);
}


/*
 * 'ippRead()' - Read data for an IPP request from a HTTP connection.
 */

ipp_state_t				/* O - Current state */
ippRead(http_t *http,			/* I - HTTP connection */
        ipp_t  *ipp)			/* I - IPP data */
{
  DEBUG_printf(("ippRead(http=%p, ipp=%p), data_remaining=" CUPS_LLFMT "\n",
                http, ipp, CUPS_LLCAST (http ? http->data_remaining : -1)));

  if (http == NULL)
    return (IPP_ERROR);

  DEBUG_printf(("http->state = %d\n", http->state));

  return (ippReadIO(http, (ipp_iocb_t)ipp_read_http,
                    http->blocking || http->used != 0, NULL, ipp));
}


/*
 * 'ippReadFile()' - Read data for an IPP request from a file.
 *
 * @since CUPS 1.1.19@
 */

ipp_state_t				/* O - Current state */
ippReadFile(int   fd,			/* I - HTTP data */
            ipp_t *ipp)			/* I - IPP data */
{
  DEBUG_printf(("ippReadFile(%d, %p)\n", fd, ipp));

  return (ippReadIO(&fd, (ipp_iocb_t)ipp_read_file, 1, NULL, ipp));
}


/*
 * 'ippReadIO()' - Read data for an IPP request.
 *
 * @since CUPS 1.1.19@
 */

ipp_state_t				/* O - Current state */
ippReadIO(void       *src,		/* I - Data source */
          ipp_iocb_t cb,		/* I - Read callback function */
	  int        blocking,		/* I - Use blocking IO? */
	  ipp_t      *parent,		/* I - Parent request, if any */
          ipp_t      *ipp)		/* I - IPP data */
{
  int			n;		/* Length of data */
  unsigned char		buffer[32768],	/* Data buffer */
			*bufptr;	/* Pointer into buffer */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_tag_t		tag;		/* Current tag */
  ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("ippReadIO(%p, %p, %d, %p, %p)\n", src, cb, blocking,
                parent, ipp));

  if (src == NULL || ipp == NULL)
    return (IPP_ERROR);

  switch (ipp->state)
  {
    case IPP_IDLE :
        ipp->state ++; /* Avoid common problem... */

    case IPP_HEADER :
        if (parent == NULL)
	{
	 /*
          * Get the request header...
	  */

          if ((n = (*cb)(src, buffer, 8)) < 8)
	  {
	    DEBUG_printf(("ippReadIO: Unable to read header (%d bytes read)!\n", n));
	    return (n == 0 ? IPP_IDLE : IPP_ERROR);
	  }

	 /*
          * Verify the major version number...
	  */

	  if (buffer[0] != 1)
	  {
	    DEBUG_printf(("ippReadIO: version number (%d.%d) is bad.\n", buffer[0],
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

          DEBUG_printf(("ippReadIO: version=%d.%d\n", buffer[0], buffer[1]));
	  DEBUG_printf(("ippReadIO: op_status=%04x\n",
	                ipp->request.any.op_status));
	  DEBUG_printf(("ippReadIO: request_id=%d\n",
	                ipp->request.any.request_id));
        }

        ipp->state   = IPP_ATTRIBUTE;
	ipp->current = NULL;
	ipp->curtag  = IPP_TAG_ZERO;
	ipp->prev    = ipp->last;

       /*
        * If blocking is disabled, stop here...
	*/

        if (!blocking)
	  break;

    case IPP_ATTRIBUTE :
        while ((*cb)(src, buffer, 1) > 0)
	{
	  DEBUG_printf(("ippReadIO: ipp->current=%p, ipp->prev=%p\n",
	                ipp->current, ipp->prev));

	 /*
	  * Read this attribute...
	  */

          tag = (ipp_tag_t)buffer[0];

	  if (tag == IPP_TAG_END)
	  {
	   /*
	    * No more attributes left...
	    */

            DEBUG_puts("ippReadIO: IPP_TAG_END!");

	    ipp->state = IPP_DATA;
	    break;
	  }
          else if (tag < IPP_TAG_UNSUPPORTED_VALUE)
	  {
	   /*
	    * Group tag...  Set the current group and continue...
	    */

            if (ipp->curtag == tag)
	      ipp->prev = ippAddSeparator(ipp);
            else if (ipp->current)
	      ipp->prev = ipp->current;

	    ipp->curtag  = tag;
	    ipp->current = NULL;
	    DEBUG_printf(("ippReadIO: group tag = %x, ipp->prev=%p\n", tag,
	                  ipp->prev));
	    continue;
	  }

          DEBUG_printf(("ippReadIO: value tag = %x\n", tag));

         /*
	  * Get the name...
	  */

          if ((*cb)(src, buffer, 2) < 2)
	  {
	    DEBUG_puts("ippReadIO: unable to read name length!");
	    return (IPP_ERROR);
	  }

          n = (buffer[0] << 8) | buffer[1];

          if (n > (sizeof(buffer) - 1))
	  {
	    DEBUG_printf(("ippReadIO: bad name length %d!\n", n));
	    return (IPP_ERROR);
	  }

          DEBUG_printf(("ippReadIO: name length = %d\n", n));

          if (n == 0 && tag != IPP_TAG_MEMBERNAME &&
	      tag != IPP_TAG_END_COLLECTION)
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

	    if (attr->value_tag == IPP_TAG_ZERO)
	    {
	     /*
	      * Setting the value of a collection member...
	      */

	      attr->value_tag = tag;
	    }
	    else if (attr->value_tag == IPP_TAG_STRING ||
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
	    * Finally, reallocate the attribute array as needed...
	    */

	    if (attr->num_values == 1 ||
	        (attr->num_values > 0 &&
	         (attr->num_values & (IPP_MAX_VALUES - 1)) == 0))
	    {
	      ipp_attribute_t	*temp;	/* Pointer to new buffer */


              DEBUG_printf(("ippReadIO: reallocating for up to %d values...\n",
	                    attr->num_values + IPP_MAX_VALUES));

             /*
	      * Reallocate memory...
	      */

              if ((temp = realloc(attr, sizeof(ipp_attribute_t) +
	                                (attr->num_values + IPP_MAX_VALUES - 1) *
					sizeof(ipp_value_t))) == NULL)
	        return (IPP_ERROR);

              if (temp != attr)
	      {
               /*
		* Reset pointers in the list...
		*/

        	if (ipp->prev)
	          ipp->prev->next = temp;
		else
	          ipp->attrs = temp;

        	attr = ipp->current = ipp->last = temp;
	      }
	    }
	  }
	  else if (tag == IPP_TAG_MEMBERNAME)
	  {
	   /*
	    * Name must be length 0!
	    */

	    if (n)
	    {
	      DEBUG_puts("ippReadIO: member name not empty!");
	      return (IPP_ERROR);
	    }

            if (ipp->current)
	      ipp->prev = ipp->current;

	    attr = ipp->current = _ipp_add_attr(ipp, 1);

	    DEBUG_printf(("ippReadIO: membername, ipp->current=%p, ipp->prev=%p\n",
	                  ipp->current, ipp->prev));

	    attr->group_tag  = ipp->curtag;
	    attr->value_tag  = IPP_TAG_ZERO;
	    attr->num_values = 0;
	  }
	  else if (tag != IPP_TAG_END_COLLECTION)
	  {
	   /*
	    * New attribute; read the name and add it...
	    */

	    if ((*cb)(src, buffer, n) < n)
	    {
	      DEBUG_puts("ippReadIO: unable to read name!");
	      return (IPP_ERROR);
	    }

	    buffer[n] = '\0';

            if (ipp->current)
	      ipp->prev = ipp->current;

	    attr = ipp->current = _ipp_add_attr(ipp, 1);

	    DEBUG_printf(("ippReadIO: name=\'%s\', ipp->current=%p, ipp->prev=%p\n",
	                  buffer, ipp->current, ipp->prev));

	    attr->group_tag  = ipp->curtag;
	    attr->value_tag  = tag;
	    attr->name       = strdup((char *)buffer);
	    attr->num_values = 0;
	  }
	  else
	    attr = NULL;

          if (tag != IPP_TAG_END_COLLECTION)
            value = attr->values + attr->num_values;
	  else
	    value = NULL;

	  if ((*cb)(src, buffer, 2) < 2)
	  {
	    DEBUG_puts("ippReadIO: unable to read value length!");
	    return (IPP_ERROR);
	  }

	  n = (buffer[0] << 8) | buffer[1];
          DEBUG_printf(("ippReadIO: value length = %d\n", n));

	  switch (tag)
	  {
	    case IPP_TAG_INTEGER :
	    case IPP_TAG_ENUM :
	        if ((*cb)(src, buffer, 4) < 4)
		{
	          DEBUG_puts("ippReadIO: Unable to read integer value!");
		  return (IPP_ERROR);
		}

		n = (((((buffer[0] << 8) | buffer[1]) << 8) | buffer[2]) << 8) |
		    buffer[3];

                value->integer = n;
	        break;
	    case IPP_TAG_BOOLEAN :
	        if ((*cb)(src, buffer, 1) < 1)
		{
	          DEBUG_puts("ippReadIO: Unable to read boolean value!");
		  return (IPP_ERROR);
		}

                value->boolean = buffer[0];
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
                value->string.text = calloc(n + 1, 1);

	        if ((*cb)(src, (ipp_uchar_t *)value->string.text, n) < n)
		{
	          DEBUG_puts("ippReadIO: Unable to read string value!");
		  return (IPP_ERROR);
		}

		DEBUG_printf(("ippReadIO: value = \'%s\'\n",
		              value->string.text));
	        break;
	    case IPP_TAG_DATE :
	        if ((*cb)(src, value->date, 11) < 11)
		{
	          DEBUG_puts("ippReadIO: Unable to date integer value!");
		  return (IPP_ERROR);
		}
	        break;
	    case IPP_TAG_RESOLUTION :
	        if ((*cb)(src, buffer, 9) < 9)
		{
	          DEBUG_puts("ippReadIO: Unable to read resolution value!");
		  return (IPP_ERROR);
		}

                value->resolution.xres =
		    (((((buffer[0] << 8) | buffer[1]) << 8) | buffer[2]) << 8) |
		    buffer[3];
                value->resolution.yres =
		    (((((buffer[4] << 8) | buffer[5]) << 8) | buffer[6]) << 8) |
		    buffer[7];
                value->resolution.units =
		    (ipp_res_t)buffer[8];
	        break;
	    case IPP_TAG_RANGE :
	        if ((*cb)(src, buffer, 8) < 8)
		{
	          DEBUG_puts("ippReadIO: Unable to read range value!");
		  return (IPP_ERROR);
		}

                value->range.lower =
		    (((((buffer[0] << 8) | buffer[1]) << 8) | buffer[2]) << 8) |
		    buffer[3];
                value->range.upper =
		    (((((buffer[4] << 8) | buffer[5]) << 8) | buffer[6]) << 8) |
		    buffer[7];
	        break;
	    case IPP_TAG_TEXTLANG :
	    case IPP_TAG_NAMELANG :
	        if (n > sizeof(buffer) || n < 4)
		{
		  DEBUG_printf(("ippReadIO: bad value length %d!\n", n));
		  return (IPP_ERROR);
		}

	        if ((*cb)(src, buffer, n) < n)
		{
	          DEBUG_puts("ippReadIO: Unable to read string w/language value!");
		  return (IPP_ERROR);
		}

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

                value->string.charset = calloc(n + 1, 1);

		memcpy(value->string.charset,
		       bufptr + 2, n);

                bufptr += 2 + n;
		n = (bufptr[0] << 8) | bufptr[1];

                value->string.text = calloc(n + 1, 1);

		memcpy(value->string.text,
		       bufptr + 2, n);
	        break;

            case IPP_TAG_BEGIN_COLLECTION :
	       /*
	        * Oh, boy, here comes a collection value, so read it...
		*/

                value->collection = ippNew();

                if (n > 0)
		{
	          DEBUG_puts("ippReadIO: begCollection tag with value length > 0!");
		  return (IPP_ERROR);
		}

		if (ippReadIO(src, cb, 1, ipp, value->collection) == IPP_ERROR)
		{
	          DEBUG_puts("ippReadIO: Unable to read collection value!");
		  return (IPP_ERROR);
		}
                break;

            case IPP_TAG_END_COLLECTION :
                if (n > 0)
		{
	          DEBUG_puts("ippReadIO: endCollection tag with value length > 0!");
		  return (IPP_ERROR);
		}

	        DEBUG_puts("ippReadIO: endCollection tag...");

		return (ipp->state = IPP_DATA);

            case IPP_TAG_MEMBERNAME :
	       /*
	        * The value is the name of the member in the collection,
		* which we need to carry over...
		*/

		attr->name = calloc(n + 1, 1);

	        if ((*cb)(src, (ipp_uchar_t *)attr->name, n) < n)
		{
	          DEBUG_puts("ippReadIO: Unable to read member name value!");
		  return (IPP_ERROR);
		}

               /*
	        * Since collection members are encoded differently than
		* regular attributes, make sure we don't start with an
		* empty value...
		*/

                attr->num_values --;

		DEBUG_printf(("ippReadIO: member name = \"%s\"\n", attr->name));
		break;

            default : /* Other unsupported values */
                value->unknown.length = n;
	        if (n > 0)
		{
		  value->unknown.data = malloc(n);
	          if ((*cb)(src, value->unknown.data, n) < n)
		  {
	            DEBUG_puts("ippReadIO: Unable to read unsupported value!");
		    return (IPP_ERROR);
		  }
		}
		else
		  value->unknown.data = NULL;
	        break;
	  }

          attr->num_values ++;

	 /*
          * If blocking is disabled, stop here...
	  */

          if (!blocking)
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
  struct tm	*unixdate;		/* UNIX unixdate/time info */
  ipp_uchar_t	*date = _cupsGlobals()->ipp_date;
					/* RFC-1903 date/time data */


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
 * 'ippWrite()' - Write data for an IPP request to a HTTP connection.
 */

ipp_state_t				/* O - Current state */
ippWrite(http_t *http,			/* I - HTTP connection */
         ipp_t  *ipp)			/* I - IPP data */
{
  DEBUG_printf(("ippWrite(%p, %p)\n", http, ipp));

  if (http == NULL)
    return (IPP_ERROR);

  return (ippWriteIO(http, (ipp_iocb_t)httpWrite,
                     http->blocking, NULL, ipp));
}


/*
 * 'ippWriteFile()' - Write data for an IPP request to a file.
 *
 * @since CUPS 1.1.19@
 */

ipp_state_t				/* O - Current state */
ippWriteFile(int   fd,			/* I - HTTP data */
             ipp_t *ipp)		/* I - IPP data */
{
  DEBUG_printf(("ippWriteFile(%d, %p)\n", fd, ipp));

  ipp->state = IPP_IDLE;

  return (ippWriteIO(&fd, (ipp_iocb_t)ipp_write_file, 1, NULL, ipp));
}


/*
 * 'ippWriteIO()' - Write data for an IPP request.
 *
 * @since CUPS 1.1.19@
 */

ipp_state_t				/* O - Current state */
ippWriteIO(void       *dst,		/* I - Destination */
           ipp_iocb_t cb,		/* I - Write callback function */
	   int        blocking,		/* I - Use blocking IO? */
	   ipp_t      *parent,		/* I - Parent IPP request */
           ipp_t      *ipp)		/* I - IPP data */
{
  int			i;		/* Looping var */
  int			n;		/* Length of data */
  unsigned char		buffer[32768],	/* Data buffer */
			*bufptr;	/* Pointer into buffer */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("ippWriteIO(%p, %p, %d, %p, %p)\n", dst, cb, blocking,
                parent, ipp));

  if (dst == NULL || ipp == NULL)
    return (IPP_ERROR);

  switch (ipp->state)
  {
    case IPP_IDLE :
        ipp->state ++; /* Avoid common problem... */

    case IPP_HEADER :
        if (parent == NULL)
	{
	 /*
	  * Send the request header:
	  *
	  *                 Version = 2 bytes
	  *   Operation/Status Code = 2 bytes
	  *              Request ID = 4 bytes
	  *                   Total = 8 bytes
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

          if ((*cb)(dst, buffer, bufptr - buffer) < 0)
	  {
	    DEBUG_puts("ippWrite: Could not write IPP header...");
	    return (IPP_ERROR);
	  }
	}

       /*
	* Reset the state engine to point to the first attribute
	* in the request/response, with no current group.
	*/

        ipp->state   = IPP_ATTRIBUTE;
	ipp->current = ipp->attrs;
	ipp->curtag  = IPP_TAG_ZERO;

        DEBUG_printf(("ippWrite: version=%d.%d\n", buffer[0], buffer[1]));
	DEBUG_printf(("ippWrite: op_status=%04x\n", ipp->request.any.op_status));
	DEBUG_printf(("ippWrite: request_id=%d\n", ipp->request.any.request_id));

       /*
        * If blocking is disabled, stop here...
	*/

        if (!blocking)
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

          if (ipp->curtag != attr->group_tag && parent == NULL)
	  {
	   /*
	    * Send a group tag byte...
	    */

	    ipp->curtag = attr->group_tag;

            if (attr->group_tag == IPP_TAG_ZERO)
	      continue;

            DEBUG_printf(("ippWrite: wrote group tag = %x\n", attr->group_tag));
	    *bufptr++ = attr->group_tag;
	  }

         /*
	  * Write the attribute tag and name.  The current implementation
	  * does not support the extension value tags above 0x7f, so all
	  * value tags are 1 byte.
	  *
	  * The attribute name length does not include the trailing nul
	  * character in the source string.
	  *
	  * Collection values (parent != NULL) are written differently...
	  */

          if (parent == NULL)
	  {
           /*
	    * Get the length of the attribute name, and make sure it won't
	    * overflow the buffer...
	    */

            if ((n = strlen(attr->name)) > (sizeof(buffer) - 4))
	      return (IPP_ERROR);

           /*
	    * Write the value tag, name length, and name string...
	    */

            DEBUG_printf(("ippWrite: writing value tag = %x\n", attr->value_tag));
            DEBUG_printf(("ippWrite: writing name = %d, \'%s\'\n", n, attr->name));

            *bufptr++ = attr->value_tag;
	    *bufptr++ = n >> 8;
	    *bufptr++ = n;
	    memcpy(bufptr, attr->name, n);
	    bufptr += n;
          }
	  else
	  {
           /*
	    * Get the length of the attribute name, and make sure it won't
	    * overflow the buffer...
	    */

            if ((n = strlen(attr->name)) > (sizeof(buffer) - 7))
	      return (IPP_ERROR);

           /*
	    * Write the member name tag, name length, name string, value tag,
	    * and empty name for the collection member attribute...
	    */

            DEBUG_printf(("ippWrite: writing value tag = %x\n",
	                  IPP_TAG_MEMBERNAME));
            DEBUG_printf(("ippWrite: writing name = %d, \'%s\'\n", n, attr->name));
            DEBUG_printf(("ippWrite: writing value tag = %x\n", attr->value_tag));
            DEBUG_puts("ippWrite: writing name = 0, \'\'\n");

            *bufptr++ = IPP_TAG_MEMBERNAME;
	    *bufptr++ = 0;
	    *bufptr++ = 0;
	    *bufptr++ = n >> 8;
	    *bufptr++ = n;
	    memcpy(bufptr, attr->name, n);
	    bufptr += n;

            *bufptr++ = attr->value_tag;
            *bufptr++ = 0;
            *bufptr++ = 0;
	  }

         /*
	  * Now write the attribute value(s)...
	  */

	  switch (attr->value_tag & ~IPP_TAG_COPY)
	  {
	    case IPP_TAG_INTEGER :
	    case IPP_TAG_ENUM :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
                  if ((sizeof(buffer) - (bufptr - buffer)) < 9)
		  {
                    if ((*cb)(dst, buffer, bufptr - buffer) < 0)
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

		 /*
	          * Integers and enumerations are both 4-byte signed
		  * (twos-complement) values.
		  *
		  * Put the 2-byte length and 4-byte value into the buffer...
		  */

	          *bufptr++ = 0;
		  *bufptr++ = 4;
		  *bufptr++ = value->integer >> 24;
		  *bufptr++ = value->integer >> 16;
		  *bufptr++ = value->integer >> 8;
		  *bufptr++ = value->integer;
		}
		break;

	    case IPP_TAG_BOOLEAN :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
                  if ((sizeof(buffer) - (bufptr - buffer)) < 6)
		  {
                    if ((*cb)(dst, buffer, bufptr - buffer) < 0)
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

                 /*
		  * Boolean values are 1-byte; 0 = false, 1 = true.
		  *
		  * Put the 2-byte length and 1-byte value into the buffer...
		  */

	          *bufptr++ = 0;
		  *bufptr++ = 1;
		  *bufptr++ = value->boolean;
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
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
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
                      if ((*cb)(dst, buffer, bufptr - buffer) < 0)
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

                  if (value->string.text != NULL)
                    n = strlen(value->string.text);
		  else
		    n = 0;

                  if (n > (sizeof(buffer) - 2))
		    return (IPP_ERROR);

                  DEBUG_printf(("ippWrite: writing string = %d, \'%s\'\n", n,
		                value->string.text));

                  if ((int)(sizeof(buffer) - (bufptr - buffer)) < (n + 2))
		  {
                    if ((*cb)(dst, buffer, bufptr - buffer) < 0)
	            {
	              DEBUG_puts("ippWrite: Could not write IPP attribute...");
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

		 /*
		  * All simple strings consist of the 2-byte length and
		  * character data without the trailing nul normally found
		  * in C strings.  Also, strings cannot be longer than 32767
		  * bytes since the 2-byte length is a signed (twos-complement)
		  * value.
		  *
		  * Put the 2-byte length and string characters in the buffer.
		  */

	          *bufptr++ = n >> 8;
		  *bufptr++ = n;

		  if (n > 0)
		  {
		    memcpy(bufptr, value->string.text, n);
		    bufptr += n;
		  }
		}
		break;

	    case IPP_TAG_DATE :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
                  if ((sizeof(buffer) - (bufptr - buffer)) < 16)
		  {
                    if ((*cb)(dst, buffer, bufptr - buffer) < 0)
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

                 /*
		  * Date values consist of a 2-byte length and an
		  * 11-byte date/time structure defined by RFC 1903.
		  *
		  * Put the 2-byte length and 11-byte date/time
		  * structure in the buffer.
		  */

	          *bufptr++ = 0;
		  *bufptr++ = 11;
		  memcpy(bufptr, value->date, 11);
		  bufptr += 11;
		}
		break;

	    case IPP_TAG_RESOLUTION :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
                  if ((sizeof(buffer) - (bufptr - buffer)) < 14)
		  {
                    if ((*cb)(dst, buffer, bufptr - buffer) < 0)
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

                 /*
		  * Resolution values consist of a 2-byte length,
		  * 4-byte horizontal resolution value, 4-byte vertical
		  * resolution value, and a 1-byte units value.
		  *
		  * Put the 2-byte length and resolution value data
		  * into the buffer.
		  */

	          *bufptr++ = 0;
		  *bufptr++ = 9;
		  *bufptr++ = value->resolution.xres >> 24;
		  *bufptr++ = value->resolution.xres >> 16;
		  *bufptr++ = value->resolution.xres >> 8;
		  *bufptr++ = value->resolution.xres;
		  *bufptr++ = value->resolution.yres >> 24;
		  *bufptr++ = value->resolution.yres >> 16;
		  *bufptr++ = value->resolution.yres >> 8;
		  *bufptr++ = value->resolution.yres;
		  *bufptr++ = value->resolution.units;
		}
		break;

	    case IPP_TAG_RANGE :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
                  if ((sizeof(buffer) - (bufptr - buffer)) < 13)
		  {
                    if ((*cb)(dst, buffer, bufptr - buffer) < 0)
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

                 /*
		  * Range values consist of a 2-byte length,
		  * 4-byte lower value, and 4-byte upper value.
		  *
		  * Put the 2-byte length and range value data
		  * into the buffer.
		  */

	          *bufptr++ = 0;
		  *bufptr++ = 8;
		  *bufptr++ = value->range.lower >> 24;
		  *bufptr++ = value->range.lower >> 16;
		  *bufptr++ = value->range.lower >> 8;
		  *bufptr++ = value->range.lower;
		  *bufptr++ = value->range.upper >> 24;
		  *bufptr++ = value->range.upper >> 16;
		  *bufptr++ = value->range.upper >> 8;
		  *bufptr++ = value->range.upper;
		}
		break;

	    case IPP_TAG_TEXTLANG :
	    case IPP_TAG_NAMELANG :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    if ((sizeof(buffer) - (bufptr - buffer)) < 3)
		    {
                      if ((*cb)(dst, buffer, bufptr - buffer) < 0)
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

                 /*
		  * textWithLanguage and nameWithLanguage values consist
		  * of a 2-byte length for both strings and their
		  * individual lengths, a 2-byte length for the
		  * character string, the character string without the
		  * trailing nul, a 2-byte length for the character
		  * set string, and the character set string without
		  * the trailing nul.
		  */

                  n = 4;

		  if (value->string.charset != NULL)
                    n += strlen(value->string.charset);

		  if (value->string.text != NULL)
                    n += strlen(value->string.text);

                  if (n > (sizeof(buffer) - 2))
		    return (IPP_ERROR);

                  if ((int)(sizeof(buffer) - (bufptr - buffer)) < (n + 2))
		  {
                    if ((*cb)(dst, buffer, bufptr - buffer) < 0)
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
		  if (value->string.charset != NULL)
		    n = strlen(value->string.charset);
		  else
		    n = 0;

	          *bufptr++ = n >> 8;
		  *bufptr++ = n;

                 /* Charset */
		  if (n > 0)
		  {
		    memcpy(bufptr, value->string.charset, n);
		    bufptr += n;
		  }

                 /* Length of text */
                  if (value->string.text != NULL)
		    n = strlen(value->string.text);
		  else
		    n = 0;

	          *bufptr++ = n >> 8;
		  *bufptr++ = n;

                 /* Text */
		  if (n > 0)
		  {
		    memcpy(bufptr, value->string.text, n);
		    bufptr += n;
		  }
		}
		break;

            case IPP_TAG_BEGIN_COLLECTION :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
		 /*
		  * Collections are written with the begin-collection
		  * tag first with a value of 0 length, followed by the
		  * attributes in the collection, then the end-collection
		  * value...
		  */

                  if ((sizeof(buffer) - (bufptr - buffer)) < 5)
		  {
                    if ((*cb)(dst, buffer, bufptr - buffer) < 0)
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

                 /*
		  * Write a data length of 0 and flush the buffer...
		  */

	          *bufptr++ = 0;
		  *bufptr++ = 0;

                  if ((*cb)(dst, buffer, bufptr - buffer) < 0)
	          {
	            DEBUG_puts("ippWrite: Could not write IPP attribute...");
	            return (IPP_ERROR);
	          }

		  bufptr = buffer;

                 /*
		  * Then write the collection attribute...
		  */

                  value->collection->state = IPP_IDLE;

		  if (ippWriteIO(dst, cb, 1, ipp, value->collection) == IPP_ERROR)
		    return (IPP_ERROR);
		}
		break;

            default :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    if ((sizeof(buffer) - (bufptr - buffer)) < 3)
		    {
                      if ((*cb)(dst, buffer, bufptr - buffer) < 0)
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

                 /*
		  * An unknown value might some new value that a
		  * vendor has come up with. It consists of a
		  * 2-byte length and the bytes in the unknown
		  * value buffer.
		  */

                  n = value->unknown.length;

                  if (n > (sizeof(buffer) - 2))
		    return (IPP_ERROR);

                  if ((int)(sizeof(buffer) - (bufptr - buffer)) < (n + 2))
		  {
                    if ((*cb)(dst, buffer, bufptr - buffer) < 0)
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
		    memcpy(bufptr, value->unknown.data, n);
		    bufptr += n;
		  }
		}
		break;
	  }

         /*
	  * Write the data out...
	  */

          if ((*cb)(dst, buffer, bufptr - buffer) < 0)
	  {
	    DEBUG_puts("ippWrite: Could not write IPP attribute...");
	    return (IPP_ERROR);
	  }

          DEBUG_printf(("ippWrite: wrote %d bytes\n", bufptr - buffer));

	 /*
          * If blocking is disabled, stop here...
	  */

          if (!blocking)
	    break;
	}

	if (ipp->current == NULL)
	{
         /*
	  * Done with all of the attributes; add the end-of-attributes
	  * tag or end-collection attribute...
	  */

          if (parent == NULL)
	  {
            buffer[0] = IPP_TAG_END;
	    n         = 1;
	  }
	  else
	  {
            buffer[0] = IPP_TAG_END_COLLECTION;
	    buffer[1] = 0; /* empty name */
	    buffer[2] = 0;
	    buffer[3] = 0; /* empty value */
	    buffer[4] = 0;
	    n         = 5;
	  }

	  if ((*cb)(dst, buffer, n) < 0)
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
 * '_ipp_add_attr()' - Add a new attribute to the request.
 */

ipp_attribute_t *			/* O - New attribute */
_ipp_add_attr(ipp_t *ipp,		/* I - IPP request */
              int   num_values)		/* I - Number of values */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("_ipp_add_attr(%p, %d)\n", ipp, num_values));

  if (ipp == NULL || num_values < 0)
    return (NULL);

  attr = calloc(sizeof(ipp_attribute_t) +
                (num_values - 1) * sizeof(ipp_value_t), 1);

  attr->num_values = num_values;

  if (attr != NULL)
  {
    if (ipp->last == NULL)
      ipp->attrs = attr;
    else
      ipp->last->next = attr;

    ipp->last = attr;
  }

  DEBUG_printf(("_ipp_add_attr(): %p\n", attr));

  return (attr);
}


/*
 * '_ipp_free_attr()' - Free an attribute.
 */

void
_ipp_free_attr(ipp_attribute_t *attr)	/* I - Attribute to free */
{
  int		i;			/* Looping var */
  ipp_value_t	*value;			/* Current value */


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
	for (i = 0, value = attr->values;
	     i < attr->num_values;
	     i ++, value ++)
	  free(value->string.text);
	break;

    case IPP_TAG_TEXTLANG :
    case IPP_TAG_NAMELANG :
	for (i = 0, value = attr->values;
	     i < attr->num_values;
	     i ++, value ++)
	{
	  if (value->string.charset && i == 0)
	    free(value->string.charset);
	  free(value->string.text);
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
 * 'ipp_length()' - Compute the length of an IPP request or collection value.
 */

static size_t				/* O - Size of IPP request */
ipp_length(ipp_t *ipp,			/* I - IPP request or collection */
           int   collection)		/* I - 1 if a collection, 0 otherwise */
{
  int			i;		/* Looping var */
  int			bytes;		/* Number of bytes */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_tag_t		group;		/* Current group */
  ipp_value_t		*value;		/* Current value */


  if (ipp == NULL)
    return (0);

 /*
  * Start with 8 bytes for the IPP request or status header...
  */

  bytes = collection ? 0 : 8;

 /*
  * Then add the lengths of each attribute...
  */

  group = IPP_TAG_ZERO;

  for (attr = ipp->attrs; attr != NULL; attr = attr->next)
  {
    if (attr->group_tag != group && !collection)
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

    if (collection)
      bytes += 5;			/* Add membername overhead */

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
	  for (i = 0, value = attr->values;
	       i < attr->num_values;
	       i ++, value ++)
	    if (value->string.text != NULL)
	      bytes += strlen(value->string.text);
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

	  for (i = 0, value = attr->values;
	       i < attr->num_values;
	       i ++, value ++)
	  {
	    if (value->string.charset != NULL)
	      bytes += strlen(value->string.charset);

	    if (value->string.text != NULL)
	      bytes += strlen(value->string.text);
	  }
	  break;

      case IPP_TAG_BEGIN_COLLECTION :
	  for (i = 0, value = attr->values;
	       i < attr->num_values;
	       i ++, value ++)
            bytes += ipp_length(attr->values[i].collection, 1);
	  break;

      default :
	  for (i = 0, value = attr->values;
	       i < attr->num_values;
	       i ++, value ++)
            bytes += attr->values[0].unknown.length;
	  break;
    }
  }

 /*
  * Finally, add 1 byte for the "end of attributes" tag or 5 bytes
  * for the "end of collection" tag and return...
  */

  if (collection)
    bytes += 5;
  else
    bytes ++;

  DEBUG_printf(("bytes = %d\n", bytes));

  return (bytes);
}


/*
 * 'ipp_read_http()' - Semi-blocking read on a HTTP connection...
 */

static int				/* O - Number of bytes read */
ipp_read_http(http_t      *http,	/* I - Client connection */
              ipp_uchar_t *buffer,	/* O - Buffer for data */
	      int         length)	/* I - Total length */
{
  int		tbytes,			/* Total bytes read */
		bytes;			/* Bytes read this pass */
  char		len[32];		/* Length string */
  

  DEBUG_printf(("ipp_read_http(http=%p, buffer=%p, length=%d)\n",
                http, buffer, length));

 /*
  * Loop until all bytes are read...
  */

  for (tbytes = 0, bytes = 0; tbytes < length; tbytes += bytes, buffer += bytes)
  {
    DEBUG_printf(("tbytes = %d, http->state = %d\n", tbytes, http->state));

    if (http->state == HTTP_WAITING)
      break;

    if (http->used > 0 && http->data_encoding == HTTP_ENCODE_LENGTH)
    {
     /*
      * Do "fast read" from HTTP buffer directly...
      */

      if (http->used > (length - tbytes))
        bytes = length - tbytes;
      else
        bytes = http->used;

      if (bytes == 1)
	buffer[0] = http->buffer[0];
      else
	memcpy(buffer, http->buffer, bytes);

      http->used           -= bytes;
      http->data_remaining -= bytes;

      if (http->data_remaining <= INT_MAX)
	http->_data_remaining = (int)http->data_remaining;
      else
	http->_data_remaining = INT_MAX;

      if (http->used > 0)
	memmove(http->buffer, http->buffer + bytes, http->used);

      if (http->data_remaining == 0)
      {
	if (http->data_encoding == HTTP_ENCODE_CHUNKED)
	{
	 /*
	  * Get the trailing CR LF after the chunk...
	  */

	  if (!httpGets(len, sizeof(len), http))
	    return (-1);
	}

	if (http->data_encoding != HTTP_ENCODE_CHUNKED)
	{
	  if (http->state == HTTP_POST_RECV)
	    http->state ++;
	  else
	    http->state = HTTP_WAITING;
	}
      }
    }
    else
    {
     /*
      * Wait a maximum of 1 second for data...
      */

      if (!http->blocking)
      {
       /*
        * Wait up to 1 second for more data on non-blocking sockets...
	*/

	if (!httpWait(http, 1000))
	{
	 /*
          * Signal no data...
	  */

          bytes = -1;
	  break;
	}
      }

      if ((bytes = httpRead(http, (char *)buffer, length - tbytes)) <= 0)
        break;
    }
  }

 /*
  * Return the number of bytes read...
  */

  if (tbytes == 0 && bytes < 0)
    tbytes = -1;

  DEBUG_printf(("returning %d bytes...\n", tbytes));

  return (tbytes);
}


/*
 * 'ipp_read_file()' - Read IPP data from a file.
 */

static int				/* O - Number of bytes read */
ipp_read_file(int         *fd,		/* I - File descriptor */
              ipp_uchar_t *buffer,	/* O - Read buffer */
	      int         length)	/* I - Number of bytes to read */
{
  return (read(*fd, buffer, length));
}


/*
 * 'ipp_write_file()' - Write IPP data to a file.
 */

static int				/* O - Number of bytes written */
ipp_write_file(int         *fd,		/* I - File descriptor */
               ipp_uchar_t *buffer,	/* I - Data to write */
               int         length)	/* I - Number of bytes to write */
{
  return (write(*fd, buffer, length));
}


/*
 * End of "$Id$".
 */
