/*
 * Internet Printing Protocol functions for CUPS.
 *
 * Copyright 2007-2017 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include <regex.h>
#ifdef WIN32
#  include <io.h>
#endif /* WIN32 */


/*
 * Local functions...
 */

static ipp_attribute_t	*ipp_add_attr(ipp_t *ipp, const char *name,
			              ipp_tag_t  group_tag, ipp_tag_t value_tag,
			              int num_values);
static void		ipp_free_values(ipp_attribute_t *attr, int element,
			                int count);
static char		*ipp_get_code(const char *locale, char *buffer,
			              size_t bufsize)
			              __attribute__((nonnull(1,2)));
static char		*ipp_lang_code(const char *locale, char *buffer,
			               size_t bufsize)
			               __attribute__((nonnull(1,2)));
static size_t		ipp_length(ipp_t *ipp, int collection);
static ssize_t		ipp_read_http(http_t *http, ipp_uchar_t *buffer,
			              size_t length);
static ssize_t		ipp_read_file(int *fd, ipp_uchar_t *buffer,
			              size_t length);
static void		ipp_set_error(ipp_status_t status, const char *format,
			              ...);
static _ipp_value_t	*ipp_set_value(ipp_t *ipp, ipp_attribute_t **attr,
			               int element);
static ssize_t		ipp_write_file(int *fd, ipp_uchar_t *buffer,
			               size_t length);


/*
 * '_cupsBufferGet()' - Get a read/write buffer.
 */

char *					/* O - Buffer */
_cupsBufferGet(size_t size)		/* I - Size required */
{
  _cups_buffer_t	*buffer;	/* Current buffer */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */


  for (buffer = cg->cups_buffers; buffer; buffer = buffer->next)
    if (!buffer->used && buffer->size >= size)
      break;

  if (!buffer)
  {
    if ((buffer = malloc(sizeof(_cups_buffer_t) + size - 1)) == NULL)
      return (NULL);

    buffer->next     = cg->cups_buffers;
    buffer->size     = size;
    cg->cups_buffers = buffer;
  }

  buffer->used = 1;

  return (buffer->d);
}


/*
 * '_cupsBufferRelease()' - Release a read/write buffer.
 */

void
_cupsBufferRelease(char *b)		/* I - Buffer to release */
{
  _cups_buffer_t	*buffer;	/* Buffer */


 /*
  * Mark this buffer as unused...
  */

  buffer       = (_cups_buffer_t *)(b - offsetof(_cups_buffer_t, d));
  buffer->used = 0;
}


/*
 * 'ippAddBoolean()' - Add a boolean attribute to an IPP message.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code group@ parameter specifies the IPP attribute group tag: none
 * (@code IPP_TAG_ZERO@, for member attributes), document (@code IPP_TAG_DOCUMENT@),
 * event notification (@code IPP_TAG_EVENT_NOTIFICATION@), operation
 * (@code IPP_TAG_OPERATION@), printer (@code IPP_TAG_PRINTER@), subscription
 * (@code IPP_TAG_SUBSCRIPTION@), or unsupported (@code IPP_TAG_UNSUPPORTED_GROUP@).
 */

ipp_attribute_t *			/* O - New attribute */
ippAddBoolean(ipp_t      *ipp,		/* I - IPP message */
              ipp_tag_t  group,		/* I - IPP group */
              const char *name,		/* I - Name of attribute */
              char       value)		/* I - Value of attribute */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddBoolean(ipp=%p, group=%02x(%s), name=\"%s\", value=%d)", (void *)ipp, group, ippTagString(group), name, value));

 /*
  * Range check input...
  */

  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE)
    return (NULL);

 /*
  * Create the attribute...
  */

  if ((attr = ipp_add_attr(ipp, name, group, IPP_TAG_BOOLEAN, 1)) == NULL)
    return (NULL);

  attr->values[0].boolean = value;

  return (attr);
}


/*
 * 'ippAddBooleans()' - Add an array of boolean values.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code group@ parameter specifies the IPP attribute group tag: none
 * (@code IPP_TAG_ZERO@, for member attributes), document (@code IPP_TAG_DOCUMENT@),
 * event notification (@code IPP_TAG_EVENT_NOTIFICATION@), operation
 * (@code IPP_TAG_OPERATION@), printer (@code IPP_TAG_PRINTER@), subscription
 * (@code IPP_TAG_SUBSCRIPTION@), or unsupported (@code IPP_TAG_UNSUPPORTED_GROUP@).
 */

ipp_attribute_t *			/* O - New attribute */
ippAddBooleans(ipp_t      *ipp,		/* I - IPP message */
               ipp_tag_t  group,	/* I - IPP group */
	       const char *name,	/* I - Name of attribute */
	       int        num_values,	/* I - Number of values */
	       const char *values)	/* I - Values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */
  _ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("ippAddBooleans(ipp=%p, group=%02x(%s), name=\"%s\", num_values=%d, values=%p)", (void *)ipp, group, ippTagString(group), name, num_values, (void *)values));

 /*
  * Range check input...
  */

  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE ||
      num_values < 1)
    return (NULL);

 /*
  * Create the attribute...
  */

  if ((attr = ipp_add_attr(ipp, name, group, IPP_TAG_BOOLEAN, num_values)) == NULL)
    return (NULL);

  if (values)
  {
    for (i = num_values, value = attr->values;
	 i > 0;
	 i --, value ++)
      value->boolean = *values++;
  }

  return (attr);
}


/*
 * 'ippAddCollection()' - Add a collection value.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code group@ parameter specifies the IPP attribute group tag: none
 * (@code IPP_TAG_ZERO@, for member attributes), document (@code IPP_TAG_DOCUMENT@),
 * event notification (@code IPP_TAG_EVENT_NOTIFICATION@), operation
 * (@code IPP_TAG_OPERATION@), printer (@code IPP_TAG_PRINTER@), subscription
 * (@code IPP_TAG_SUBSCRIPTION@), or unsupported (@code IPP_TAG_UNSUPPORTED_GROUP@).
 *
 * @since CUPS 1.1.19/macOS 10.3@
 */

ipp_attribute_t *			/* O - New attribute */
ippAddCollection(ipp_t      *ipp,	/* I - IPP message */
                 ipp_tag_t  group,	/* I - IPP group */
		 const char *name,	/* I - Name of attribute */
		 ipp_t      *value)	/* I - Value */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddCollection(ipp=%p, group=%02x(%s), name=\"%s\", value=%p)", (void *)ipp, group, ippTagString(group), name, (void *)value));

 /*
  * Range check input...
  */

  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE)
    return (NULL);

 /*
  * Create the attribute...
  */

  if ((attr = ipp_add_attr(ipp, name, group, IPP_TAG_BEGIN_COLLECTION, 1)) == NULL)
    return (NULL);

  attr->values[0].collection = value;

  if (value)
    value->use ++;

  return (attr);
}


/*
 * 'ippAddCollections()' - Add an array of collection values.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code group@ parameter specifies the IPP attribute group tag: none
 * (@code IPP_TAG_ZERO@, for member attributes), document (@code IPP_TAG_DOCUMENT@),
 * event notification (@code IPP_TAG_EVENT_NOTIFICATION@), operation
 * (@code IPP_TAG_OPERATION@), printer (@code IPP_TAG_PRINTER@), subscription
 * (@code IPP_TAG_SUBSCRIPTION@), or unsupported (@code IPP_TAG_UNSUPPORTED_GROUP@).
 *
 * @since CUPS 1.1.19/macOS 10.3@
 */

ipp_attribute_t *			/* O - New attribute */
ippAddCollections(
    ipp_t       *ipp,			/* I - IPP message */
    ipp_tag_t   group,			/* I - IPP group */
    const char  *name,			/* I - Name of attribute */
    int         num_values,		/* I - Number of values */
    const ipp_t **values)		/* I - Values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */
  _ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("ippAddCollections(ipp=%p, group=%02x(%s), name=\"%s\", num_values=%d, values=%p)", (void *)ipp, group, ippTagString(group), name, num_values, (void *)values));

 /*
  * Range check input...
  */

  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE ||
      num_values < 1)
    return (NULL);

 /*
  * Create the attribute...
  */

  if ((attr = ipp_add_attr(ipp, name, group, IPP_TAG_BEGIN_COLLECTION,
                           num_values)) == NULL)
    return (NULL);

  if (values)
  {
    for (i = num_values, value = attr->values;
	 i > 0;
	 i --, value ++)
    {
      value->collection = (ipp_t *)*values++;
      value->collection->use ++;
    }
  }

  return (attr);
}


/*
 * 'ippAddDate()' - Add a date attribute to an IPP message.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code group@ parameter specifies the IPP attribute group tag: none
 * (@code IPP_TAG_ZERO@, for member attributes), document (@code IPP_TAG_DOCUMENT@),
 * event notification (@code IPP_TAG_EVENT_NOTIFICATION@), operation
 * (@code IPP_TAG_OPERATION@), printer (@code IPP_TAG_PRINTER@), subscription
 * (@code IPP_TAG_SUBSCRIPTION@), or unsupported (@code IPP_TAG_UNSUPPORTED_GROUP@).
 */

ipp_attribute_t *			/* O - New attribute */
ippAddDate(ipp_t             *ipp,	/* I - IPP message */
           ipp_tag_t         group,	/* I - IPP group */
	   const char        *name,	/* I - Name of attribute */
	   const ipp_uchar_t *value)	/* I - Value */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddDate(ipp=%p, group=%02x(%s), name=\"%s\", value=%p)", (void *)ipp, group, ippTagString(group), name, (void *)value));

 /*
  * Range check input...
  */

  if (!ipp || !name || !value || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE)
    return (NULL);

 /*
  * Create the attribute...
  */

  if ((attr = ipp_add_attr(ipp, name, group, IPP_TAG_DATE, 1)) == NULL)
    return (NULL);

  memcpy(attr->values[0].date, value, 11);

  return (attr);
}


/*
 * 'ippAddInteger()' - Add a integer attribute to an IPP message.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code group@ parameter specifies the IPP attribute group tag: none
 * (@code IPP_TAG_ZERO@, for member attributes), document (@code IPP_TAG_DOCUMENT@),
 * event notification (@code IPP_TAG_EVENT_NOTIFICATION@), operation
 * (@code IPP_TAG_OPERATION@), printer (@code IPP_TAG_PRINTER@), subscription
 * (@code IPP_TAG_SUBSCRIPTION@), or unsupported (@code IPP_TAG_UNSUPPORTED_GROUP@).
 *
 * Supported values include enum (@code IPP_TAG_ENUM@) and integer
 * (@code IPP_TAG_INTEGER@).
 */

ipp_attribute_t *			/* O - New attribute */
ippAddInteger(ipp_t      *ipp,		/* I - IPP message */
              ipp_tag_t  group,		/* I - IPP group */
	      ipp_tag_t  value_tag,	/* I - Type of attribute */
              const char *name,		/* I - Name of attribute */
              int        value)		/* I - Value of attribute */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddInteger(ipp=%p, group=%02x(%s), type=%02x(%s), name=\"%s\", value=%d)", (void *)ipp, group, ippTagString(group), value_tag, ippTagString(value_tag), name, value));

  value_tag &= IPP_TAG_CUPS_MASK;

 /*
  * Special-case for legacy usage: map out-of-band attributes to new ippAddOutOfBand
  * function...
  */

  if (value_tag >= IPP_TAG_UNSUPPORTED_VALUE && value_tag <= IPP_TAG_ADMINDEFINE)
    return (ippAddOutOfBand(ipp, group, value_tag, name));

 /*
  * Range check input...
  */

#if 0
  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE ||
      (value_tag != IPP_TAG_INTEGER && value_tag != IPP_TAG_ENUM))
    return (NULL);
#else
  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE)
    return (NULL);
#endif /* 0 */

 /*
  * Create the attribute...
  */

  if ((attr = ipp_add_attr(ipp, name, group, value_tag, 1)) == NULL)
    return (NULL);

  attr->values[0].integer = value;

  return (attr);
}


/*
 * 'ippAddIntegers()' - Add an array of integer values.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code group@ parameter specifies the IPP attribute group tag: none
 * (@code IPP_TAG_ZERO@, for member attributes), document (@code IPP_TAG_DOCUMENT@),
 * event notification (@code IPP_TAG_EVENT_NOTIFICATION@), operation
 * (@code IPP_TAG_OPERATION@), printer (@code IPP_TAG_PRINTER@), subscription
 * (@code IPP_TAG_SUBSCRIPTION@), or unsupported (@code IPP_TAG_UNSUPPORTED_GROUP@).
 *
 * Supported values include enum (@code IPP_TAG_ENUM@) and integer
 * (@code IPP_TAG_INTEGER@).
 */

ipp_attribute_t *			/* O - New attribute */
ippAddIntegers(ipp_t      *ipp,		/* I - IPP message */
               ipp_tag_t  group,	/* I - IPP group */
	       ipp_tag_t  value_tag,	/* I - Type of attribute */
	       const char *name,	/* I - Name of attribute */
	       int        num_values,	/* I - Number of values */
	       const int  *values)	/* I - Values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */
  _ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("ippAddIntegers(ipp=%p, group=%02x(%s), type=%02x(%s), name=\"%s\", num_values=%d, values=%p)", (void *)ipp, group, ippTagString(group), value_tag, ippTagString(value_tag), name, num_values, (void *)values));

  value_tag &= IPP_TAG_CUPS_MASK;

 /*
  * Range check input...
  */

#if 0
  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE ||
      (value_tag != IPP_TAG_INTEGER && value_tag != IPP_TAG_ENUM) ||
      num_values < 1)
    return (NULL);
#else
  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE ||
      num_values < 1)
    return (NULL);
#endif /* 0 */

 /*
  * Create the attribute...
  */

  if ((attr = ipp_add_attr(ipp, name, group, value_tag, num_values)) == NULL)
    return (NULL);

  if (values)
  {
    for (i = num_values, value = attr->values;
	 i > 0;
	 i --, value ++)
      value->integer = *values++;
  }

  return (attr);
}


/*
 * 'ippAddOctetString()' - Add an octetString value to an IPP message.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code group@ parameter specifies the IPP attribute group tag: none
 * (@code IPP_TAG_ZERO@, for member attributes), document (@code IPP_TAG_DOCUMENT@),
 * event notification (@code IPP_TAG_EVENT_NOTIFICATION@), operation
 * (@code IPP_TAG_OPERATION@), printer (@code IPP_TAG_PRINTER@), subscription
 * (@code IPP_TAG_SUBSCRIPTION@), or unsupported (@code IPP_TAG_UNSUPPORTED_GROUP@).
 *
 * @since CUPS 1.2/macOS 10.5@
 */

ipp_attribute_t	*			/* O - New attribute */
ippAddOctetString(ipp_t      *ipp,	/* I - IPP message */
                  ipp_tag_t  group,	/* I - IPP group */
                  const char *name,	/* I - Name of attribute */
                  const void *data,	/* I - octetString data */
		  int        datalen)	/* I - Length of data in bytes */
{
  ipp_attribute_t	*attr;		/* New attribute */


  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE ||
      datalen < 0 || datalen > IPP_MAX_LENGTH)
    return (NULL);

  if ((attr = ipp_add_attr(ipp, name, group, IPP_TAG_STRING, 1)) == NULL)
    return (NULL);

 /*
  * Initialize the attribute data...
  */

  attr->values[0].unknown.length = datalen;

  if (data)
  {
    if ((attr->values[0].unknown.data = malloc((size_t)datalen)) == NULL)
    {
      ippDeleteAttribute(ipp, attr);
      return (NULL);
    }

    memcpy(attr->values[0].unknown.data, data, (size_t)datalen);
  }

 /*
  * Return the new attribute...
  */

  return (attr);
}


/*
 * 'ippAddOutOfBand()' - Add an out-of-band value to an IPP message.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code group@ parameter specifies the IPP attribute group tag: none
 * (@code IPP_TAG_ZERO@, for member attributes), document (@code IPP_TAG_DOCUMENT@),
 * event notification (@code IPP_TAG_EVENT_NOTIFICATION@), operation
 * (@code IPP_TAG_OPERATION@), printer (@code IPP_TAG_PRINTER@), subscription
 * (@code IPP_TAG_SUBSCRIPTION@), or unsupported (@code IPP_TAG_UNSUPPORTED_GROUP@).
 *
 * Supported out-of-band values include unsupported-value
 * (@code IPP_TAG_UNSUPPORTED_VALUE@), default (@code IPP_TAG_DEFAULT@), unknown
 * (@code IPP_TAG_UNKNOWN@), no-value (@code IPP_TAG_NOVALUE@), not-settable
 * (@code IPP_TAG_NOTSETTABLE@), delete-attribute (@code IPP_TAG_DELETEATTR@), and
 * admin-define (@code IPP_TAG_ADMINDEFINE@).
 *
 * @since CUPS 1.6/macOS 10.8@
 */

ipp_attribute_t	*			/* O - New attribute */
ippAddOutOfBand(ipp_t      *ipp,	/* I - IPP message */
                ipp_tag_t  group,	/* I - IPP group */
                ipp_tag_t  value_tag,	/* I - Type of attribute */
		const char *name)	/* I - Name of attribute */
{
  DEBUG_printf(("ippAddOutOfBand(ipp=%p, group=%02x(%s), value_tag=%02x(%s), name=\"%s\")", (void *)ipp, group, ippTagString(group), value_tag, ippTagString(value_tag), name));

  value_tag &= IPP_TAG_CUPS_MASK;

 /*
  * Range check input...
  */

  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE ||
      (value_tag != IPP_TAG_UNSUPPORTED_VALUE &&
       value_tag != IPP_TAG_DEFAULT &&
       value_tag != IPP_TAG_UNKNOWN &&
       value_tag != IPP_TAG_NOVALUE &&
       value_tag != IPP_TAG_NOTSETTABLE &&
       value_tag != IPP_TAG_DELETEATTR &&
       value_tag != IPP_TAG_ADMINDEFINE))
    return (NULL);

 /*
  * Create the attribute...
  */

  return (ipp_add_attr(ipp, name, group, value_tag, 1));
}


/*
 * 'ippAddRange()' - Add a range of values to an IPP message.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code group@ parameter specifies the IPP attribute group tag: none
 * (@code IPP_TAG_ZERO@, for member attributes), document (@code IPP_TAG_DOCUMENT@),
 * event notification (@code IPP_TAG_EVENT_NOTIFICATION@), operation
 * (@code IPP_TAG_OPERATION@), printer (@code IPP_TAG_PRINTER@), subscription
 * (@code IPP_TAG_SUBSCRIPTION@), or unsupported (@code IPP_TAG_UNSUPPORTED_GROUP@).
 *
 * The @code lower@ parameter must be less than or equal to the @code upper@ parameter.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddRange(ipp_t      *ipp,		/* I - IPP message */
            ipp_tag_t  group,		/* I - IPP group */
	    const char *name,		/* I - Name of attribute */
	    int        lower,		/* I - Lower value */
	    int        upper)		/* I - Upper value */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddRange(ipp=%p, group=%02x(%s), name=\"%s\", lower=%d, upper=%d)", (void *)ipp, group, ippTagString(group), name, lower, upper));

 /*
  * Range check input...
  */

  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE)
    return (NULL);

 /*
  * Create the attribute...
  */

  if ((attr = ipp_add_attr(ipp, name, group, IPP_TAG_RANGE, 1)) == NULL)
    return (NULL);

  attr->values[0].range.lower = lower;
  attr->values[0].range.upper = upper;

  return (attr);
}


/*
 * 'ippAddRanges()' - Add ranges of values to an IPP message.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code group@ parameter specifies the IPP attribute group tag: none
 * (@code IPP_TAG_ZERO@, for member attributes), document (@code IPP_TAG_DOCUMENT@),
 * event notification (@code IPP_TAG_EVENT_NOTIFICATION@), operation
 * (@code IPP_TAG_OPERATION@), printer (@code IPP_TAG_PRINTER@), subscription
 * (@code IPP_TAG_SUBSCRIPTION@), or unsupported (@code IPP_TAG_UNSUPPORTED_GROUP@).
 */

ipp_attribute_t *			/* O - New attribute */
ippAddRanges(ipp_t      *ipp,		/* I - IPP message */
             ipp_tag_t  group,		/* I - IPP group */
	     const char *name,		/* I - Name of attribute */
	     int        num_values,	/* I - Number of values */
	     const int  *lower,		/* I - Lower values */
	     const int  *upper)		/* I - Upper values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */
  _ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("ippAddRanges(ipp=%p, group=%02x(%s), name=\"%s\", num_values=%d, lower=%p, upper=%p)", (void *)ipp, group, ippTagString(group), name, num_values, (void *)lower, (void *)upper));

 /*
  * Range check input...
  */

  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE ||
      num_values < 1)
    return (NULL);

 /*
  * Create the attribute...
  */

  if ((attr = ipp_add_attr(ipp, name, group, IPP_TAG_RANGE, num_values)) == NULL)
    return (NULL);

  if (lower && upper)
  {
    for (i = num_values, value = attr->values;
	 i > 0;
	 i --, value ++)
    {
      value->range.lower = *lower++;
      value->range.upper = *upper++;
    }
  }

  return (attr);
}


/*
 * 'ippAddResolution()' - Add a resolution value to an IPP message.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code group@ parameter specifies the IPP attribute group tag: none
 * (@code IPP_TAG_ZERO@, for member attributes), document (@code IPP_TAG_DOCUMENT@),
 * event notification (@code IPP_TAG_EVENT_NOTIFICATION@), operation
 * (@code IPP_TAG_OPERATION@), printer (@code IPP_TAG_PRINTER@), subscription
 * (@code IPP_TAG_SUBSCRIPTION@), or unsupported (@code IPP_TAG_UNSUPPORTED_GROUP@).
 */

ipp_attribute_t *			/* O - New attribute */
ippAddResolution(ipp_t      *ipp,	/* I - IPP message */
        	 ipp_tag_t  group,	/* I - IPP group */
		 const char *name,	/* I - Name of attribute */
		 ipp_res_t  units,	/* I - Units for resolution */
		 int        xres,	/* I - X resolution */
		 int        yres)	/* I - Y resolution */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddResolution(ipp=%p, group=%02x(%s), name=\"%s\", units=%d, xres=%d, yres=%d)", (void *)ipp, group,
		ippTagString(group), name, units, xres, yres));

 /*
  * Range check input...
  */

  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE ||
      units < IPP_RES_PER_INCH || units > IPP_RES_PER_CM ||
      xres < 0 || yres < 0)
    return (NULL);

 /*
  * Create the attribute...
  */

  if ((attr = ipp_add_attr(ipp, name, group, IPP_TAG_RESOLUTION, 1)) == NULL)
    return (NULL);

  attr->values[0].resolution.xres  = xres;
  attr->values[0].resolution.yres  = yres;
  attr->values[0].resolution.units = units;

  return (attr);
}


/*
 * 'ippAddResolutions()' - Add resolution values to an IPP message.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code group@ parameter specifies the IPP attribute group tag: none
 * (@code IPP_TAG_ZERO@, for member attributes), document (@code IPP_TAG_DOCUMENT@),
 * event notification (@code IPP_TAG_EVENT_NOTIFICATION@), operation
 * (@code IPP_TAG_OPERATION@), printer (@code IPP_TAG_PRINTER@), subscription
 * (@code IPP_TAG_SUBSCRIPTION@), or unsupported (@code IPP_TAG_UNSUPPORTED_GROUP@).
 */

ipp_attribute_t *			/* O - New attribute */
ippAddResolutions(ipp_t      *ipp,	/* I - IPP message */
        	  ipp_tag_t  group,	/* I - IPP group */
		  const char *name,	/* I - Name of attribute */
		  int        num_values,/* I - Number of values */
		  ipp_res_t  units,	/* I - Units for resolution */
		  const int  *xres,	/* I - X resolutions */
		  const int  *yres)	/* I - Y resolutions */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */
  _ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("ippAddResolutions(ipp=%p, group=%02x(%s), name=\"%s\", num_value=%d, units=%d, xres=%p, yres=%p)", (void *)ipp, group, ippTagString(group), name, num_values, units, (void *)xres, (void *)yres));

 /*
  * Range check input...
  */

  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE ||
      num_values < 1 ||
      units < IPP_RES_PER_INCH || units > IPP_RES_PER_CM)
    return (NULL);

 /*
  * Create the attribute...
  */

  if ((attr = ipp_add_attr(ipp, name, group, IPP_TAG_RESOLUTION, num_values)) == NULL)
    return (NULL);

  if (xres && yres)
  {
    for (i = num_values, value = attr->values;
	 i > 0;
	 i --, value ++)
    {
      value->resolution.xres  = *xres++;
      value->resolution.yres  = *yres++;
      value->resolution.units = units;
    }
  }

  return (attr);
}


/*
 * 'ippAddSeparator()' - Add a group separator to an IPP message.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddSeparator(ipp_t *ipp)		/* I - IPP message */
{
  DEBUG_printf(("ippAddSeparator(ipp=%p)", (void *)ipp));

 /*
  * Range check input...
  */

  if (!ipp)
    return (NULL);

 /*
  * Create the attribute...
  */

  return (ipp_add_attr(ipp, NULL, IPP_TAG_ZERO, IPP_TAG_ZERO, 0));
}


/*
 * 'ippAddString()' - Add a language-encoded string to an IPP message.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code group@ parameter specifies the IPP attribute group tag: none
 * (@code IPP_TAG_ZERO@, for member attributes), document (@code IPP_TAG_DOCUMENT@),
 * event notification (@code IPP_TAG_EVENT_NOTIFICATION@), operation
 * (@code IPP_TAG_OPERATION@), printer (@code IPP_TAG_PRINTER@), subscription
 * (@code IPP_TAG_SUBSCRIPTION@), or unsupported (@code IPP_TAG_UNSUPPORTED_GROUP@).
 *
 * Supported string values include charset (@code IPP_TAG_CHARSET@), keyword
 * (@code IPP_TAG_KEYWORD@), language (@code IPP_TAG_LANGUAGE@), mimeMediaType
 * (@code IPP_TAG_MIMETYPE@), name (@code IPP_TAG_NAME@), nameWithLanguage
 * (@code IPP_TAG_NAMELANG), text (@code IPP_TAG_TEXT@), textWithLanguage
 * (@code IPP_TAG_TEXTLANG@), uri (@code IPP_TAG_URI@), and uriScheme
 * (@code IPP_TAG_URISCHEME@).
 *
 * The @code language@ parameter must be non-@code NULL@ for nameWithLanguage and
 * textWithLanguage string values and must be @code NULL@ for all other string values.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddString(ipp_t      *ipp,		/* I - IPP message */
             ipp_tag_t  group,		/* I - IPP group */
	     ipp_tag_t  value_tag,	/* I - Type of attribute */
             const char *name,		/* I - Name of attribute */
             const char *language,	/* I - Language code */
             const char *value)		/* I - Value */
{
  ipp_tag_t		temp_tag;	/* Temporary value tag (masked) */
  ipp_attribute_t	*attr;		/* New attribute */
  char			code[IPP_MAX_LANGUAGE];
					/* Charset/language code buffer */


  DEBUG_printf(("ippAddString(ipp=%p, group=%02x(%s), value_tag=%02x(%s), name=\"%s\", language=\"%s\", value=\"%s\")", (void *)ipp, group, ippTagString(group), value_tag, ippTagString(value_tag), name, language, value));

 /*
  * Range check input...
  */

  temp_tag = (ipp_tag_t)((int)value_tag & IPP_TAG_CUPS_MASK);

#if 0
  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE ||
      (temp_tag < IPP_TAG_TEXT && temp_tag != IPP_TAG_TEXTLANG &&
       temp_tag != IPP_TAG_NAMELANG) || temp_tag > IPP_TAG_MIMETYPE)
    return (NULL);

  if ((temp_tag == IPP_TAG_TEXTLANG || temp_tag == IPP_TAG_NAMELANG)
          != (language != NULL))
    return (NULL);
#else
  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE)
    return (NULL);
#endif /* 0 */

 /*
  * See if we need to map charset, language, or locale values...
  */

  if (language && ((int)value_tag & IPP_TAG_CUPS_CONST) &&
      strcmp(language, ipp_lang_code(language, code, sizeof(code))))
    value_tag = temp_tag;		/* Don't do a fast copy */
  else if (value && value_tag == (ipp_tag_t)(IPP_TAG_CHARSET | IPP_TAG_CUPS_CONST) &&
           strcmp(value, ipp_get_code(value, code, sizeof(code))))
    value_tag = temp_tag;		/* Don't do a fast copy */
  else if (value && value_tag == (ipp_tag_t)(IPP_TAG_LANGUAGE | IPP_TAG_CUPS_CONST) &&
           strcmp(value, ipp_lang_code(value, code, sizeof(code))))
    value_tag = temp_tag;		/* Don't do a fast copy */

 /*
  * Create the attribute...
  */

  if ((attr = ipp_add_attr(ipp, name, group, value_tag, 1)) == NULL)
    return (NULL);

 /*
  * Initialize the attribute data...
  */

  if ((int)value_tag & IPP_TAG_CUPS_CONST)
  {
    attr->values[0].string.language = (char *)language;
    attr->values[0].string.text     = (char *)value;
  }
  else
  {
    if (language)
      attr->values[0].string.language = _cupsStrAlloc(ipp_lang_code(language, code,
						      sizeof(code)));

    if (value)
    {
      if (value_tag == IPP_TAG_CHARSET)
	attr->values[0].string.text = _cupsStrAlloc(ipp_get_code(value, code,
								 sizeof(code)));
      else if (value_tag == IPP_TAG_LANGUAGE)
	attr->values[0].string.text = _cupsStrAlloc(ipp_lang_code(value, code,
								  sizeof(code)));
      else
	attr->values[0].string.text = _cupsStrAlloc(value);
    }
  }

  return (attr);
}


/*
 * 'ippAddStringf()' - Add a formatted string to an IPP message.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code group@ parameter specifies the IPP attribute group tag: none
 * (@code IPP_TAG_ZERO@, for member attributes), document
 * (@code IPP_TAG_DOCUMENT@), event notification
 * (@code IPP_TAG_EVENT_NOTIFICATION@), operation (@code IPP_TAG_OPERATION@),
 * printer (@code IPP_TAG_PRINTER@), subscription (@code IPP_TAG_SUBSCRIPTION@),
 * or unsupported (@code IPP_TAG_UNSUPPORTED_GROUP@).
 *
 * Supported string values include charset (@code IPP_TAG_CHARSET@), keyword
 * (@code IPP_TAG_KEYWORD@), language (@code IPP_TAG_LANGUAGE@), mimeMediaType
 * (@code IPP_TAG_MIMETYPE@), name (@code IPP_TAG_NAME@), nameWithLanguage
 * (@code IPP_TAG_NAMELANG), text (@code IPP_TAG_TEXT@), textWithLanguage
 * (@code IPP_TAG_TEXTLANG@), uri (@code IPP_TAG_URI@), and uriScheme
 * (@code IPP_TAG_URISCHEME@).
 *
 * The @code language@ parameter must be non-@code NULL@ for nameWithLanguage
 * and textWithLanguage string values and must be @code NULL@ for all other
 * string values.
 *
 * The @code format@ parameter uses formatting characters compatible with the
 * printf family of standard functions.  Additional arguments follow it as
 * needed.  The formatted string is truncated as needed to the maximum length of
 * the corresponding value type.
 *
 * @since CUPS 1.7/macOS 10.9@
 */

ipp_attribute_t *			/* O - New attribute */
ippAddStringf(ipp_t      *ipp,		/* I - IPP message */
              ipp_tag_t  group,		/* I - IPP group */
	      ipp_tag_t  value_tag,	/* I - Type of attribute */
	      const char *name,		/* I - Name of attribute */
	      const char *language,	/* I - Language code (@code NULL@ for default) */
	      const char *format,	/* I - Printf-style format string */
	      ...)			/* I - Additional arguments as needed */
{
  ipp_attribute_t	*attr;		/* New attribute */
  va_list		ap;		/* Argument pointer */


  va_start(ap, format);
  attr = ippAddStringfv(ipp, group, value_tag, name, language, format, ap);
  va_end(ap);

  return (attr);
}


/*
 * 'ippAddStringfv()' - Add a formatted string to an IPP message.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code group@ parameter specifies the IPP attribute group tag: none
 * (@code IPP_TAG_ZERO@, for member attributes), document
 * (@code IPP_TAG_DOCUMENT@), event notification
 * (@code IPP_TAG_EVENT_NOTIFICATION@), operation (@code IPP_TAG_OPERATION@),
 * printer (@code IPP_TAG_PRINTER@), subscription (@code IPP_TAG_SUBSCRIPTION@),
 * or unsupported (@code IPP_TAG_UNSUPPORTED_GROUP@).
 *
 * Supported string values include charset (@code IPP_TAG_CHARSET@), keyword
 * (@code IPP_TAG_KEYWORD@), language (@code IPP_TAG_LANGUAGE@), mimeMediaType
 * (@code IPP_TAG_MIMETYPE@), name (@code IPP_TAG_NAME@), nameWithLanguage
 * (@code IPP_TAG_NAMELANG), text (@code IPP_TAG_TEXT@), textWithLanguage
 * (@code IPP_TAG_TEXTLANG@), uri (@code IPP_TAG_URI@), and uriScheme
 * (@code IPP_TAG_URISCHEME@).
 *
 * The @code language@ parameter must be non-@code NULL@ for nameWithLanguage
 * and textWithLanguage string values and must be @code NULL@ for all other
 * string values.
 *
 * The @code format@ parameter uses formatting characters compatible with the
 * printf family of standard functions.  Additional arguments are passed in the
 * stdarg pointer @code ap@.  The formatted string is truncated as needed to the
 * maximum length of the corresponding value type.
 *
 * @since CUPS 1.7/macOS 10.9@
 */

ipp_attribute_t *			/* O - New attribute */
ippAddStringfv(ipp_t      *ipp,		/* I - IPP message */
               ipp_tag_t  group,	/* I - IPP group */
	       ipp_tag_t  value_tag,	/* I - Type of attribute */
	       const char *name,	/* I - Name of attribute */
	       const char *language,	/* I - Language code (@code NULL@ for default) */
	       const char *format,	/* I - Printf-style format string */
	       va_list    ap)		/* I - Additional arguments */
{
  char		buffer[IPP_MAX_TEXT + 4];
					/* Formatted text string */
  ssize_t	bytes,			/* Length of formatted value */
		max_bytes;		/* Maximum number of bytes for value */


 /*
  * Range check input...
  */

  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE ||
      (value_tag < IPP_TAG_TEXT && value_tag != IPP_TAG_TEXTLANG &&
       value_tag != IPP_TAG_NAMELANG) || value_tag > IPP_TAG_MIMETYPE ||
      !format)
    return (NULL);

  if ((value_tag == IPP_TAG_TEXTLANG || value_tag == IPP_TAG_NAMELANG)
          != (language != NULL))
    return (NULL);

 /*
  * Format the string...
  */

  if (!strcmp(format, "%s"))
  {
   /*
    * Optimize the simple case...
    */

    const char *s = va_arg(ap, char *);

    if (!s)
      s = "(null)";

    bytes = (ssize_t)strlen(s);
    strlcpy(buffer, s, sizeof(buffer));
  }
  else
  {
   /*
    * Do a full formatting of the message...
    */

    if ((bytes = vsnprintf(buffer, sizeof(buffer), format, ap)) < 0)
      return (NULL);
  }

 /*
  * Limit the length of the string...
  */

  switch (value_tag)
  {
    default :
    case IPP_TAG_TEXT :
    case IPP_TAG_TEXTLANG :
        max_bytes = IPP_MAX_TEXT;
        break;

    case IPP_TAG_NAME :
    case IPP_TAG_NAMELANG :
        max_bytes = IPP_MAX_NAME;
        break;

    case IPP_TAG_CHARSET :
        max_bytes = IPP_MAX_CHARSET;
        break;

    case IPP_TAG_KEYWORD :
        max_bytes = IPP_MAX_KEYWORD;
        break;

    case IPP_TAG_LANGUAGE :
        max_bytes = IPP_MAX_LANGUAGE;
        break;

    case IPP_TAG_MIMETYPE :
        max_bytes = IPP_MAX_MIMETYPE;
        break;

    case IPP_TAG_URI :
        max_bytes = IPP_MAX_URI;
        break;

    case IPP_TAG_URISCHEME :
        max_bytes = IPP_MAX_URISCHEME;
        break;
  }

  if (bytes >= max_bytes)
  {
    char	*bufmax,		/* Buffer at max_bytes */
		*bufptr;		/* Pointer into buffer */

    bufptr = buffer + strlen(buffer) - 1;
    bufmax = buffer + max_bytes - 1;

    while (bufptr > bufmax)
    {
      if (*bufptr & 0x80)
      {
        while ((*bufptr & 0xc0) == 0x80 && bufptr > buffer)
          bufptr --;
      }

      bufptr --;
    }

    *bufptr = '\0';
  }

 /*
  * Add the formatted string and return...
  */

  return (ippAddString(ipp, group, value_tag, name, language, buffer));
}


/*
 * 'ippAddStrings()' - Add language-encoded strings to an IPP message.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code group@ parameter specifies the IPP attribute group tag: none
 * (@code IPP_TAG_ZERO@, for member attributes), document (@code IPP_TAG_DOCUMENT@),
 * event notification (@code IPP_TAG_EVENT_NOTIFICATION@), operation
 * (@code IPP_TAG_OPERATION@), printer (@code IPP_TAG_PRINTER@), subscription
 * (@code IPP_TAG_SUBSCRIPTION@), or unsupported (@code IPP_TAG_UNSUPPORTED_GROUP@).
 *
 * Supported string values include charset (@code IPP_TAG_CHARSET@), keyword
 * (@code IPP_TAG_KEYWORD@), language (@code IPP_TAG_LANGUAGE@), mimeMediaType
 * (@code IPP_TAG_MIMETYPE@), name (@code IPP_TAG_NAME@), nameWithLanguage
 * (@code IPP_TAG_NAMELANG), text (@code IPP_TAG_TEXT@), textWithLanguage
 * (@code IPP_TAG_TEXTLANG@), uri (@code IPP_TAG_URI@), and uriScheme
 * (@code IPP_TAG_URISCHEME@).
 *
 * The @code language@ parameter must be non-@code NULL@ for nameWithLanguage and
 * textWithLanguage string values and must be @code NULL@ for all other string values.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddStrings(
    ipp_t              *ipp,		/* I - IPP message */
    ipp_tag_t          group,		/* I - IPP group */
    ipp_tag_t          value_tag,	/* I - Type of attribute */
    const char         *name,		/* I - Name of attribute */
    int                num_values,	/* I - Number of values */
    const char         *language,	/* I - Language code (@code NULL@ for default) */
    const char * const *values)		/* I - Values */
{
  int			i;		/* Looping var */
  ipp_tag_t		temp_tag;	/* Temporary value tag (masked) */
  ipp_attribute_t	*attr;		/* New attribute */
  _ipp_value_t		*value;		/* Current value */
  char			code[32];	/* Language/charset value buffer */


  DEBUG_printf(("ippAddStrings(ipp=%p, group=%02x(%s), value_tag=%02x(%s), name=\"%s\", num_values=%d, language=\"%s\", values=%p)", (void *)ipp, group, ippTagString(group), value_tag, ippTagString(value_tag), name, num_values, language, (void *)values));

 /*
  * Range check input...
  */

  temp_tag = (ipp_tag_t)((int)value_tag & IPP_TAG_CUPS_MASK);

#if 0
  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE ||
      (temp_tag < IPP_TAG_TEXT && temp_tag != IPP_TAG_TEXTLANG &&
       temp_tag != IPP_TAG_NAMELANG) || temp_tag > IPP_TAG_MIMETYPE ||
      num_values < 1)
    return (NULL);

  if ((temp_tag == IPP_TAG_TEXTLANG || temp_tag == IPP_TAG_NAMELANG)
          != (language != NULL))
    return (NULL);
#else
  if (!ipp || !name || group < IPP_TAG_ZERO ||
      group == IPP_TAG_END || group >= IPP_TAG_UNSUPPORTED_VALUE ||
      num_values < 1)
    return (NULL);
#endif /* 0 */

 /*
  * See if we need to map charset, language, or locale values...
  */

  if (language && ((int)value_tag & IPP_TAG_CUPS_CONST) &&
      strcmp(language, ipp_lang_code(language, code, sizeof(code))))
    value_tag = temp_tag;		/* Don't do a fast copy */
  else if (values && value_tag == (ipp_tag_t)(IPP_TAG_CHARSET | IPP_TAG_CUPS_CONST))
  {
    for (i = 0; i < num_values; i ++)
      if (strcmp(values[i], ipp_get_code(values[i], code, sizeof(code))))
      {
	value_tag = temp_tag;		/* Don't do a fast copy */
        break;
      }
  }
  else if (values && value_tag == (ipp_tag_t)(IPP_TAG_LANGUAGE | IPP_TAG_CUPS_CONST))
  {
    for (i = 0; i < num_values; i ++)
      if (strcmp(values[i], ipp_lang_code(values[i], code, sizeof(code))))
      {
	value_tag = temp_tag;		/* Don't do a fast copy */
        break;
      }
  }

 /*
  * Create the attribute...
  */

  if ((attr = ipp_add_attr(ipp, name, group, value_tag, num_values)) == NULL)
    return (NULL);

 /*
  * Initialize the attribute data...
  */

  for (i = num_values, value = attr->values;
       i > 0;
       i --, value ++)
  {
    if (language)
    {
      if (value == attr->values)
      {
        if ((int)value_tag & IPP_TAG_CUPS_CONST)
          value->string.language = (char *)language;
        else
          value->string.language = _cupsStrAlloc(ipp_lang_code(language, code,
                                                               sizeof(code)));
      }
      else
	value->string.language = attr->values[0].string.language;
    }

    if (values)
    {
      if ((int)value_tag & IPP_TAG_CUPS_CONST)
        value->string.text = (char *)*values++;
      else if (value_tag == IPP_TAG_CHARSET)
	value->string.text = _cupsStrAlloc(ipp_get_code(*values++, code, sizeof(code)));
      else if (value_tag == IPP_TAG_LANGUAGE)
	value->string.text = _cupsStrAlloc(ipp_lang_code(*values++, code, sizeof(code)));
      else
	value->string.text = _cupsStrAlloc(*values++);
    }
  }

  return (attr);
}


/*
 * 'ippContainsInteger()' - Determine whether an attribute contains the
 *                          specified value or is within the list of ranges.
 *
 * Returns non-zero when the attribute contains either a matching integer or
 * enum value, or the value falls within one of the rangeOfInteger values for
 * the attribute.
 *
 * @since CUPS 1.7/macOS 10.9@
 */

int					/* O - 1 on a match, 0 on no match */
ippContainsInteger(
    ipp_attribute_t *attr,		/* I - Attribute */
    int             value)		/* I - Integer/enum value */
{
  int		i;			/* Looping var */
  _ipp_value_t	*avalue;		/* Current attribute value */


 /*
  * Range check input...
  */

  if (!attr)
    return (0);

  if (attr->value_tag != IPP_TAG_INTEGER && attr->value_tag != IPP_TAG_ENUM &&
      attr->value_tag != IPP_TAG_RANGE)
    return (0);

 /*
  * Compare...
  */

  if (attr->value_tag == IPP_TAG_RANGE)
  {
    for (i = attr->num_values, avalue = attr->values; i > 0; i --, avalue ++)
      if (value >= avalue->range.lower && value <= avalue->range.upper)
        return (1);
  }
  else
  {
    for (i = attr->num_values, avalue = attr->values; i > 0; i --, avalue ++)
      if (value == avalue->integer)
        return (1);
  }

  return (0);
}


/*
 * 'ippContainsString()' - Determine whether an attribute contains the
 *                         specified string value.
 *
 * Returns non-zero when the attribute contains a matching charset, keyword,
 * language, mimeMediaType, name, text, URI, or URI scheme value.
 *
 * @since CUPS 1.7/macOS 10.9@
 */

int					/* O - 1 on a match, 0 on no match */
ippContainsString(
    ipp_attribute_t *attr,		/* I - Attribute */
    const char      *value)		/* I - String value */
{
  int		i;			/* Looping var */
  _ipp_value_t	*avalue;		/* Current attribute value */


  DEBUG_printf(("ippContainsString(attr=%p, value=\"%s\")", (void *)attr, value));

 /*
  * Range check input...
  */

  if (!attr || !value)
  {
    DEBUG_puts("1ippContainsString: Returning 0 (bad input)");
    return (0);
  }

 /*
  * Compare...
  */

  DEBUG_printf(("1ippContainsString: attr %s, %s with %d values.",
		attr->name, ippTagString(attr->value_tag),
		attr->num_values));

  switch (attr->value_tag & IPP_TAG_CUPS_MASK)
  {
    case IPP_TAG_CHARSET :
    case IPP_TAG_KEYWORD :
    case IPP_TAG_LANGUAGE :
    case IPP_TAG_URI :
    case IPP_TAG_URISCHEME :
	for (i = attr->num_values, avalue = attr->values;
	     i > 0;
	     i --, avalue ++)
	{
	  DEBUG_printf(("1ippContainsString: value[%d]=\"%s\"",
	                attr->num_values - i, avalue->string.text));

	  if (!strcmp(value, avalue->string.text))
	  {
	    DEBUG_puts("1ippContainsString: Returning 1 (match)");
	    return (1);
	  }
        }

    case IPP_TAG_MIMETYPE :
    case IPP_TAG_NAME :
    case IPP_TAG_NAMELANG :
    case IPP_TAG_TEXT :
    case IPP_TAG_TEXTLANG :
	for (i = attr->num_values, avalue = attr->values;
	     i > 0;
	     i --, avalue ++)
	{
	  DEBUG_printf(("1ippContainsString: value[%d]=\"%s\"",
	                attr->num_values - i, avalue->string.text));

	  if (!_cups_strcasecmp(value, avalue->string.text))
	  {
	    DEBUG_puts("1ippContainsString: Returning 1 (match)");
	    return (1);
	  }
        }

    default :
        break;
  }

  DEBUG_puts("1ippContainsString: Returning 0 (no match)");

  return (0);
}


/*
 * 'ippCopyAttribute()' - Copy an attribute.
 *
 * The specified attribute, @code attr@, is copied to the destination IPP message.
 * When @code quickcopy@ is non-zero, a "shallow" reference copy of the attribute is
 * created - this should only be done as long as the original source IPP message will
 * not be freed for the life of the destination.
 *
 * @since CUPS 1.6/macOS 10.8@
 */


ipp_attribute_t *			/* O - New attribute */
ippCopyAttribute(
    ipp_t           *dst,		/* I - Destination IPP message */
    ipp_attribute_t *srcattr,		/* I - Attribute to copy */
    int             quickcopy)		/* I - 1 for a referenced copy, 0 for normal */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*dstattr;	/* Destination attribute */
  _ipp_value_t		*srcval,	/* Source value */
			*dstval;	/* Destination value */


  DEBUG_printf(("ippCopyAttribute(dst=%p, srcattr=%p, quickcopy=%d)", (void *)dst, (void *)srcattr, quickcopy));

 /*
  * Range check input...
  */

  if (!dst || !srcattr)
    return (NULL);

 /*
  * Copy it...
  */

  quickcopy = quickcopy ? IPP_TAG_CUPS_CONST : 0;

  switch (srcattr->value_tag & ~IPP_TAG_CUPS_CONST)
  {
    case IPP_TAG_ZERO :
        dstattr = ippAddSeparator(dst);
	break;

    case IPP_TAG_INTEGER :
    case IPP_TAG_ENUM :
        dstattr = ippAddIntegers(dst, srcattr->group_tag, srcattr->value_tag,
	                         srcattr->name, srcattr->num_values, NULL);
        if (!dstattr)
          break;

        for (i = srcattr->num_values, srcval = srcattr->values, dstval = dstattr->values;
             i > 0;
             i --, srcval ++, dstval ++)
	  dstval->integer = srcval->integer;
        break;

    case IPP_TAG_BOOLEAN :
        dstattr = ippAddBooleans(dst, srcattr->group_tag, srcattr->name,
	                        srcattr->num_values, NULL);
        if (!dstattr)
          break;

        for (i = srcattr->num_values, srcval = srcattr->values, dstval = dstattr->values;
             i > 0;
             i --, srcval ++, dstval ++)
	  dstval->boolean = srcval->boolean;
        break;

    case IPP_TAG_TEXT :
    case IPP_TAG_NAME :
    case IPP_TAG_KEYWORD :
    case IPP_TAG_URI :
    case IPP_TAG_URISCHEME :
    case IPP_TAG_CHARSET :
    case IPP_TAG_LANGUAGE :
    case IPP_TAG_MIMETYPE :
        dstattr = ippAddStrings(dst, srcattr->group_tag,
	                        (ipp_tag_t)(srcattr->value_tag | quickcopy),
	                        srcattr->name, srcattr->num_values, NULL, NULL);
        if (!dstattr)
          break;

        if (quickcopy)
	{
	  for (i = srcattr->num_values, srcval = srcattr->values,
	           dstval = dstattr->values;
	       i > 0;
	       i --, srcval ++, dstval ++)
	    dstval->string.text = srcval->string.text;
        }
	else if (srcattr->value_tag & IPP_TAG_CUPS_CONST)
	{
	  for (i = srcattr->num_values, srcval = srcattr->values,
	           dstval = dstattr->values;
	       i > 0;
	       i --, srcval ++, dstval ++)
	    dstval->string.text = _cupsStrAlloc(srcval->string.text);
	}
	else
	{
	  for (i = srcattr->num_values, srcval = srcattr->values,
	           dstval = dstattr->values;
	       i > 0;
	       i --, srcval ++, dstval ++)
	    dstval->string.text = _cupsStrRetain(srcval->string.text);
	}
        break;

    case IPP_TAG_DATE :
        if (srcattr->num_values != 1)
          return (NULL);

        dstattr = ippAddDate(dst, srcattr->group_tag, srcattr->name,
	                     srcattr->values[0].date);
        break;

    case IPP_TAG_RESOLUTION :
        dstattr = ippAddResolutions(dst, srcattr->group_tag, srcattr->name,
	                            srcattr->num_values, IPP_RES_PER_INCH,
				    NULL, NULL);
        if (!dstattr)
          break;

        for (i = srcattr->num_values, srcval = srcattr->values, dstval = dstattr->values;
             i > 0;
             i --, srcval ++, dstval ++)
	{
	  dstval->resolution.xres  = srcval->resolution.xres;
	  dstval->resolution.yres  = srcval->resolution.yres;
	  dstval->resolution.units = srcval->resolution.units;
	}
        break;

    case IPP_TAG_RANGE :
        dstattr = ippAddRanges(dst, srcattr->group_tag, srcattr->name,
	                       srcattr->num_values, NULL, NULL);
        if (!dstattr)
          break;

        for (i = srcattr->num_values, srcval = srcattr->values, dstval = dstattr->values;
             i > 0;
             i --, srcval ++, dstval ++)
	{
	  dstval->range.lower = srcval->range.lower;
	  dstval->range.upper = srcval->range.upper;
	}
        break;

    case IPP_TAG_TEXTLANG :
    case IPP_TAG_NAMELANG :
        dstattr = ippAddStrings(dst, srcattr->group_tag,
	                        (ipp_tag_t)(srcattr->value_tag | quickcopy),
	                        srcattr->name, srcattr->num_values, NULL, NULL);
        if (!dstattr)
          break;

        if (quickcopy)
	{
	  for (i = srcattr->num_values, srcval = srcattr->values,
	           dstval = dstattr->values;
	       i > 0;
	       i --, srcval ++, dstval ++)
	  {
            dstval->string.language = srcval->string.language;
	    dstval->string.text     = srcval->string.text;
          }
        }
	else if (srcattr->value_tag & IPP_TAG_CUPS_CONST)
	{
	  for (i = srcattr->num_values, srcval = srcattr->values,
	           dstval = dstattr->values;
	       i > 0;
	       i --, srcval ++, dstval ++)
	  {
	    if (srcval == srcattr->values)
              dstval->string.language = _cupsStrAlloc(srcval->string.language);
	    else
              dstval->string.language = dstattr->values[0].string.language;

	    dstval->string.text = _cupsStrAlloc(srcval->string.text);
          }
        }
	else
	{
	  for (i = srcattr->num_values, srcval = srcattr->values,
	           dstval = dstattr->values;
	       i > 0;
	       i --, srcval ++, dstval ++)
	  {
	    if (srcval == srcattr->values)
              dstval->string.language = _cupsStrRetain(srcval->string.language);
	    else
              dstval->string.language = dstattr->values[0].string.language;

	    dstval->string.text = _cupsStrRetain(srcval->string.text);
          }
        }
        break;

    case IPP_TAG_BEGIN_COLLECTION :
        dstattr = ippAddCollections(dst, srcattr->group_tag, srcattr->name,
	                            srcattr->num_values, NULL);
        if (!dstattr)
          break;

        for (i = srcattr->num_values, srcval = srcattr->values, dstval = dstattr->values;
             i > 0;
             i --, srcval ++, dstval ++)
	{
	  dstval->collection = srcval->collection;
	  srcval->collection->use ++;
	}
        break;

    case IPP_TAG_STRING :
    default :
        /* TODO: Implement quick copy for unknown/octetString values */
        dstattr = ippAddIntegers(dst, srcattr->group_tag, srcattr->value_tag,
	                         srcattr->name, srcattr->num_values, NULL);
        if (!dstattr)
          break;

        for (i = srcattr->num_values, srcval = srcattr->values, dstval = dstattr->values;
             i > 0;
             i --, srcval ++, dstval ++)
	{
	  dstval->unknown.length = srcval->unknown.length;

	  if (dstval->unknown.length > 0)
	  {
	    if ((dstval->unknown.data = malloc((size_t)dstval->unknown.length)) == NULL)
	      dstval->unknown.length = 0;
	    else
	      memcpy(dstval->unknown.data, srcval->unknown.data, (size_t)dstval->unknown.length);
	  }
	}
        break; /* anti-compiler-warning-code */
  }

  return (dstattr);
}


/*
 * 'ippCopyAttributes()' - Copy attributes from one IPP message to another.
 *
 * Zero or more attributes are copied from the source IPP message, @code src@, to the
 * destination IPP message, @code dst@. When @code quickcopy@ is non-zero, a "shallow"
 * reference copy of the attribute is created - this should only be done as long as the
 * original source IPP message will not be freed for the life of the destination.
 *
 * The @code cb@ and @code context@ parameters provide a generic way to "filter" the
 * attributes that are copied - the function must return 1 to copy the attribute or
 * 0 to skip it. The function may also choose to do a partial copy of the source attribute
 * itself.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O - 1 on success, 0 on error */
ippCopyAttributes(
    ipp_t        *dst,			/* I - Destination IPP message */
    ipp_t        *src,			/* I - Source IPP message */
    int          quickcopy,		/* I - 1 for a referenced copy, 0 for normal */
    ipp_copycb_t cb,			/* I - Copy callback or @code NULL@ for none */
    void         *context)		/* I - Context pointer */
{
  ipp_attribute_t	*srcattr;	/* Source attribute */


  DEBUG_printf(("ippCopyAttributes(dst=%p, src=%p, quickcopy=%d, cb=%p, context=%p)", (void *)dst, (void *)src, quickcopy, (void *)cb, context));

 /*
  * Range check input...
  */

  if (!dst || !src)
    return (0);

 /*
  * Loop through source attributes and copy as needed...
  */

  for (srcattr = src->attrs; srcattr; srcattr = srcattr->next)
    if (!cb || (*cb)(context, dst, srcattr))
      if (!ippCopyAttribute(dst, srcattr, quickcopy))
        return (0);

  return (1);
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


  if (!date)
    return (0);

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
 * 'ippDelete()' - Delete an IPP message.
 */

void
ippDelete(ipp_t *ipp)			/* I - IPP message */
{
  ipp_attribute_t	*attr,		/* Current attribute */
			*next;		/* Next attribute */


  DEBUG_printf(("ippDelete(ipp=%p)", (void *)ipp));

  if (!ipp)
    return;

  ipp->use --;
  if (ipp->use > 0)
    return;

  for (attr = ipp->attrs; attr != NULL; attr = next)
  {
    next = attr->next;

    ipp_free_values(attr, 0, attr->num_values);

    if (attr->name)
      _cupsStrFree(attr->name);

    free(attr);
  }

  free(ipp);
}


/*
 * 'ippDeleteAttribute()' - Delete a single attribute in an IPP message.
 *
 * @since CUPS 1.1.19/macOS 10.3@
 */

void
ippDeleteAttribute(
    ipp_t           *ipp,		/* I - IPP message */
    ipp_attribute_t *attr)		/* I - Attribute to delete */
{
  ipp_attribute_t	*current,	/* Current attribute */
			*prev;		/* Previous attribute */


  DEBUG_printf(("ippDeleteAttribute(ipp=%p, attr=%p(%s))", (void *)ipp, (void *)attr, attr ? attr->name : "(null)"));

 /*
  * Range check input...
  */

  if (!attr)
    return;

 /*
  * Find the attribute in the list...
  */

  if (ipp)
  {
    for (current = ipp->attrs, prev = NULL;
	 current;
	 prev = current, current = current->next)
      if (current == attr)
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

        break;
      }

    if (!current)
      return;
  }

 /*
  * Free memory used by the attribute...
  */

  ipp_free_values(attr, 0, attr->num_values);

  if (attr->name)
    _cupsStrFree(attr->name);

  free(attr);
}


/*
 * 'ippDeleteValues()' - Delete values in an attribute.
 *
 * The @code element@ parameter specifies the first value to delete, starting at
 * 0. It must be less than the number of values returned by @link ippGetCount@.
 *
 * The @code attr@ parameter may be modified as a result of setting the value.
 *
 * Deleting all values in an attribute deletes the attribute.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O  - 1 on success, 0 on failure */
ippDeleteValues(
    ipp_t           *ipp,		/* I  - IPP message */
    ipp_attribute_t **attr,		/* IO - Attribute */
    int             element,		/* I  - Index of first value to delete (0-based) */
    int             count)		/* I  - Number of values to delete */
{
 /*
  * Range check input...
  */

  if (!ipp || !attr || !*attr ||
      element < 0 || element >= (*attr)->num_values || count <= 0 ||
      (element + count) >= (*attr)->num_values)
    return (0);

 /*
  * If we are deleting all values, just delete the attribute entirely.
  */

  if (count == (*attr)->num_values)
  {
    ippDeleteAttribute(ipp, *attr);
    *attr = NULL;
    return (1);
  }

 /*
  * Otherwise free the values in question and return.
  */

  ipp_free_values(*attr, element, count);

  return (1);
}


/*
 * 'ippFindAttribute()' - Find a named attribute in a request.
 *
 * Starting with CUPS 2.0, the attribute name can contain a hierarchical list
 * of attribute and member names separated by slashes, for example
 * "media-col/media-size".
 */

ipp_attribute_t	*			/* O - Matching attribute */
ippFindAttribute(ipp_t      *ipp,	/* I - IPP message */
                 const char *name,	/* I - Name of attribute */
		 ipp_tag_t  type)	/* I - Type of attribute */
{
  DEBUG_printf(("2ippFindAttribute(ipp=%p, name=\"%s\", type=%02x(%s))", (void *)ipp, name, type, ippTagString(type)));

  if (!ipp || !name)
    return (NULL);

 /*
  * Reset the current pointer...
  */

  ipp->current = NULL;
  ipp->atend   = 0;

 /*
  * Search for the attribute...
  */

  return (ippFindNextAttribute(ipp, name, type));
}


/*
 * 'ippFindNextAttribute()' - Find the next named attribute in a request.
 *
 * Starting with CUPS 2.0, the attribute name can contain a hierarchical list
 * of attribute and member names separated by slashes, for example
 * "media-col/media-size".
 */

ipp_attribute_t	*			/* O - Matching attribute */
ippFindNextAttribute(ipp_t      *ipp,	/* I - IPP message */
                     const char *name,	/* I - Name of attribute */
		     ipp_tag_t  type)	/* I - Type of attribute */
{
  ipp_attribute_t	*attr,		/* Current atttribute */
			*childattr;	/* Child attribute */
  ipp_tag_t		value_tag;	/* Value tag */
  char			parent[1024],	/* Parent attribute name */
			*child = NULL;	/* Child attribute name */


  DEBUG_printf(("2ippFindNextAttribute(ipp=%p, name=\"%s\", type=%02x(%s))", (void *)ipp, name, type, ippTagString(type)));

  if (!ipp || !name)
    return (NULL);

  DEBUG_printf(("3ippFindNextAttribute: atend=%d", ipp->atend));

  if (ipp->atend)
    return (NULL);

  if (strchr(name, '/'))
  {
   /*
    * Search for child attribute...
    */

    strlcpy(parent, name, sizeof(parent));
    if ((child = strchr(parent, '/')) == NULL)
    {
      DEBUG_puts("3ippFindNextAttribute: Attribute name too long.");
      return (NULL);
    }

    *child++ = '\0';

    if (ipp->current && ipp->current->name && ipp->current->value_tag == IPP_TAG_BEGIN_COLLECTION && !strcmp(parent, ipp->current->name))
    {
      while (ipp->curindex < ipp->current->num_values)
      {
        if ((childattr = ippFindNextAttribute(ipp->current->values[ipp->curindex].collection, child, type)) != NULL)
          return (childattr);

        ipp->curindex ++;
        if (ipp->curindex < ipp->current->num_values && ipp->current->values[ipp->curindex].collection)
          ipp->current->values[ipp->curindex].collection->current = NULL;
      }

      ipp->prev     = ipp->current;
      ipp->current  = ipp->current->next;
      ipp->curindex = 0;

      if (!ipp->current)
      {
        ipp->atend = 1;
        return (NULL);
      }
    }

    if (!ipp->current)
    {
      ipp->prev     = NULL;
      ipp->current  = ipp->attrs;
      ipp->curindex = 0;
    }

    name = parent;
    attr = ipp->current;
  }
  else if (ipp->current)
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
    DEBUG_printf(("4ippFindAttribute: attr=%p, name=\"%s\"", (void *)attr, attr->name));

    value_tag = (ipp_tag_t)(attr->value_tag & IPP_TAG_CUPS_MASK);

    if (attr->name != NULL && _cups_strcasecmp(attr->name, name) == 0 &&
        (value_tag == type || type == IPP_TAG_ZERO || name == parent ||
	 (value_tag == IPP_TAG_TEXTLANG && type == IPP_TAG_TEXT) ||
	 (value_tag == IPP_TAG_NAMELANG && type == IPP_TAG_NAME)))
    {
      ipp->current = attr;

      if (name == parent && attr->value_tag == IPP_TAG_BEGIN_COLLECTION)
      {
        int i;				/* Looping var */

        for (i = 0; i < attr->num_values; i ++)
        {
	  if ((childattr = ippFindAttribute(attr->values[i].collection, child, type)) != NULL)
	  {
	    attr->values[0].collection->curindex = i;
	    return (childattr);
	  }
        }
      }
      else
        return (attr);
    }
  }

  ipp->current = NULL;
  ipp->prev    = NULL;
  ipp->atend   = 1;

  return (NULL);
}


/*
 * 'ippFirstAttribute()' - Return the first attribute in the message.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

ipp_attribute_t	*			/* O - First attribute or @code NULL@ if none */
ippFirstAttribute(ipp_t *ipp)		/* I - IPP message */
{
 /*
  * Range check input...
  */

  if (!ipp)
    return (NULL);

 /*
  * Return the first attribute...
  */

  return (ipp->current = ipp->attrs);
}


/*
 * 'ippGetBoolean()' - Get a boolean value for an attribute.
 *
 * The @code element@ parameter specifies which value to get from 0 to
 * @link ippGetCount(attr)@ - 1.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O - Boolean value or 0 on error */
ippGetBoolean(ipp_attribute_t *attr,	/* I - IPP attribute */
              int             element)	/* I - Value number (0-based) */
{
 /*
  * Range check input...
  */

  if (!attr || attr->value_tag != IPP_TAG_BOOLEAN ||
      element < 0 || element >= attr->num_values)
    return (0);

 /*
  * Return the value...
  */

  return (attr->values[element].boolean);
}


/*
 * 'ippGetCollection()' - Get a collection value for an attribute.
 *
 * The @code element@ parameter specifies which value to get from 0 to
 * @link ippGetCount(attr)@ - 1.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

ipp_t *					/* O - Collection value or @code NULL@ on error */
ippGetCollection(
    ipp_attribute_t *attr,		/* I - IPP attribute */
    int             element)		/* I - Value number (0-based) */
{
 /*
  * Range check input...
  */

  if (!attr || attr->value_tag != IPP_TAG_BEGIN_COLLECTION ||
      element < 0 || element >= attr->num_values)
    return (NULL);

 /*
  * Return the value...
  */

  return (attr->values[element].collection);
}


/*
 * 'ippGetCount()' - Get the number of values in an attribute.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O - Number of values or 0 on error */
ippGetCount(ipp_attribute_t *attr)	/* I - IPP attribute */
{
 /*
  * Range check input...
  */

  if (!attr)
    return (0);

 /*
  * Return the number of values...
  */

  return (attr->num_values);
}


/*
 * 'ippGetDate()' - Get a date value for an attribute.
 *
 * The @code element@ parameter specifies which value to get from 0 to
 * @link ippGetCount(attr)@ - 1.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

const ipp_uchar_t *			/* O - Date value or @code NULL@ */
ippGetDate(ipp_attribute_t *attr,	/* I - IPP attribute */
           int             element)	/* I - Value number (0-based) */
{
 /*
  * Range check input...
  */

  if (!attr || attr->value_tag != IPP_TAG_DATE ||
      element < 0 || element >= attr->num_values)
    return (NULL);

 /*
  * Return the value...
  */

  return (attr->values[element].date);
}


/*
 * 'ippGetGroupTag()' - Get the group associated with an attribute.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

ipp_tag_t				/* O - Group tag or @code IPP_TAG_ZERO@ on error */
ippGetGroupTag(ipp_attribute_t *attr)	/* I - IPP attribute */
{
 /*
  * Range check input...
  */

  if (!attr)
    return (IPP_TAG_ZERO);

 /*
  * Return the group...
  */

  return (attr->group_tag);
}


/*
 * 'ippGetInteger()' - Get the integer/enum value for an attribute.
 *
 * The @code element@ parameter specifies which value to get from 0 to
 * @link ippGetCount(attr)@ - 1.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O - Value or 0 on error */
ippGetInteger(ipp_attribute_t *attr,	/* I - IPP attribute */
              int             element)	/* I - Value number (0-based) */
{
 /*
  * Range check input...
  */

  if (!attr || (attr->value_tag != IPP_TAG_INTEGER && attr->value_tag != IPP_TAG_ENUM) ||
      element < 0 || element >= attr->num_values)
    return (0);

 /*
  * Return the value...
  */

  return (attr->values[element].integer);
}


/*
 * 'ippGetName()' - Get the attribute name.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

const char *				/* O - Attribute name or @code NULL@ for separators */
ippGetName(ipp_attribute_t *attr)	/* I - IPP attribute */
{
 /*
  * Range check input...
  */

  if (!attr)
    return (NULL);

 /*
  * Return the name...
  */

  return (attr->name);
}


/*
 * 'ippGetOctetString()' - Get an octetString value from an IPP attribute.
 *
 * The @code element@ parameter specifies which value to get from 0 to
 * @link ippGetCount(attr)@ - 1.
 *
 * @since CUPS 1.7/macOS 10.9@
 */

void *					/* O - Pointer to octetString data */
ippGetOctetString(
    ipp_attribute_t *attr,		/* I - IPP attribute */
    int             element,		/* I - Value number (0-based) */
    int             *datalen)		/* O - Length of octetString data */
{
 /*
  * Range check input...
  */

  if (!attr || attr->value_tag != IPP_TAG_STRING ||
      element < 0 || element >= attr->num_values)
  {
    if (datalen)
      *datalen = 0;

    return (NULL);
  }

 /*
  * Return the values...
  */

  if (datalen)
    *datalen = attr->values[element].unknown.length;

  return (attr->values[element].unknown.data);
}


/*
 * 'ippGetOperation()' - Get the operation ID in an IPP message.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

ipp_op_t				/* O - Operation ID or 0 on error */
ippGetOperation(ipp_t *ipp)		/* I - IPP request message */
{
 /*
  * Range check input...
  */

  if (!ipp)
    return ((ipp_op_t)0);

 /*
  * Return the value...
  */

  return (ipp->request.op.operation_id);
}


/*
 * 'ippGetRange()' - Get a rangeOfInteger value from an attribute.
 *
 * The @code element@ parameter specifies which value to get from 0 to
 * @link ippGetCount(attr)@ - 1.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O - Lower value of range or 0 */
ippGetRange(ipp_attribute_t *attr,	/* I - IPP attribute */
	    int             element,	/* I - Value number (0-based) */
	    int             *uppervalue)/* O - Upper value of range */
{
 /*
  * Range check input...
  */

  if (!attr || attr->value_tag != IPP_TAG_RANGE ||
      element < 0 || element >= attr->num_values)
  {
    if (uppervalue)
      *uppervalue = 0;

    return (0);
  }

 /*
  * Return the values...
  */

  if (uppervalue)
    *uppervalue = attr->values[element].range.upper;

  return (attr->values[element].range.lower);
}


/*
 * 'ippGetRequestId()' - Get the request ID from an IPP message.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O - Request ID or 0 on error */
ippGetRequestId(ipp_t *ipp)		/* I - IPP message */
{
 /*
  * Range check input...
  */

  if (!ipp)
    return (0);

 /*
  * Return the request ID...
  */

  return (ipp->request.any.request_id);
}


/*
 * 'ippGetResolution()' - Get a resolution value for an attribute.
 *
 * The @code element@ parameter specifies which value to get from 0 to
 * @link ippGetCount(attr)@ - 1.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O - Horizontal/cross feed resolution or 0 */
ippGetResolution(
    ipp_attribute_t *attr,		/* I - IPP attribute */
    int             element,		/* I - Value number (0-based) */
    int             *yres,		/* O - Vertical/feed resolution */
    ipp_res_t       *units)		/* O - Units for resolution */
{
 /*
  * Range check input...
  */

  if (!attr || attr->value_tag != IPP_TAG_RESOLUTION ||
      element < 0 || element >= attr->num_values)
  {
    if (yres)
      *yres = 0;

    if (units)
      *units = (ipp_res_t)0;

    return (0);
  }

 /*
  * Return the value...
  */

  if (yres)
    *yres = attr->values[element].resolution.yres;

  if (units)
    *units = attr->values[element].resolution.units;

  return (attr->values[element].resolution.xres);
}


/*
 * 'ippGetState()' - Get the IPP message state.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

ipp_state_t				/* O - IPP message state value */
ippGetState(ipp_t *ipp)			/* I - IPP message */
{
 /*
  * Range check input...
  */

  if (!ipp)
    return (IPP_STATE_IDLE);

 /*
  * Return the value...
  */

  return (ipp->state);
}


/*
 * 'ippGetStatusCode()' - Get the status code from an IPP response or event message.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

ipp_status_t				/* O - Status code in IPP message */
ippGetStatusCode(ipp_t *ipp)		/* I - IPP response or event message */
{
 /*
  * Range check input...
  */

  if (!ipp)
    return (IPP_STATUS_ERROR_INTERNAL);

 /*
  * Return the value...
  */

  return (ipp->request.status.status_code);
}


/*
 * 'ippGetString()' - Get the string and optionally the language code for an attribute.
 *
 * The @code element@ parameter specifies which value to get from 0 to
 * @link ippGetCount(attr)@ - 1.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

const char *
ippGetString(ipp_attribute_t *attr,	/* I - IPP attribute */
             int             element,	/* I - Value number (0-based) */
	     const char      **language)/* O - Language code (@code NULL@ for don't care) */
{
  ipp_tag_t	tag;			/* Value tag */


 /*
  * Range check input...
  */

  tag = ippGetValueTag(attr);

  if (!attr || element < 0 || element >= attr->num_values || (tag != IPP_TAG_TEXTLANG && tag != IPP_TAG_NAMELANG && (tag < IPP_TAG_TEXT || tag > IPP_TAG_MIMETYPE)))
    return (NULL);

 /*
  * Return the value...
  */

  if (language)
    *language = attr->values[element].string.language;

  return (attr->values[element].string.text);
}


/*
 * 'ippGetValueTag()' - Get the value tag for an attribute.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

ipp_tag_t				/* O - Value tag or @code IPP_TAG_ZERO@ on error */
ippGetValueTag(ipp_attribute_t *attr)	/* I - IPP attribute */
{
 /*
  * Range check input...
  */

  if (!attr)
    return (IPP_TAG_ZERO);

 /*
  * Return the value...
  */

  return (attr->value_tag & IPP_TAG_CUPS_MASK);
}


/*
 * 'ippGetVersion()' - Get the major and minor version number from an IPP message.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O - Major version number or 0 on error */
ippGetVersion(ipp_t *ipp,		/* I - IPP message */
              int   *minor)		/* O - Minor version number or @code NULL@ */
{
 /*
  * Range check input...
  */

  if (!ipp)
  {
    if (minor)
      *minor = 0;

    return (0);
  }

 /*
  * Return the value...
  */

  if (minor)
    *minor = ipp->request.any.version[1];

  return (ipp->request.any.version[0]);
}


/*
 * 'ippLength()' - Compute the length of an IPP message.
 */

size_t					/* O - Size of IPP message */
ippLength(ipp_t *ipp)			/* I - IPP message */
{
  return (ipp_length(ipp, 0));
}


/*
 * 'ippNextAttribute()' - Return the next attribute in the message.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

ipp_attribute_t *			/* O - Next attribute or @code NULL@ if none */
ippNextAttribute(ipp_t *ipp)		/* I - IPP message */
{
 /*
  * Range check input...
  */

  if (!ipp || !ipp->current)
    return (NULL);

 /*
  * Return the next attribute...
  */

  return (ipp->current = ipp->current->next);
}


/*
 * 'ippNew()' - Allocate a new IPP message.
 */

ipp_t *					/* O - New IPP message */
ippNew(void)
{
  ipp_t			*temp;		/* New IPP message */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */


  DEBUG_puts("ippNew()");

  if ((temp = (ipp_t *)calloc(1, sizeof(ipp_t))) != NULL)
  {
   /*
    * Set default version - usually 2.0...
    */

    if (cg->server_version == 0)
      _cupsSetDefaults();

    temp->request.any.version[0] = (ipp_uchar_t)(cg->server_version / 10);
    temp->request.any.version[1] = (ipp_uchar_t)(cg->server_version % 10);
    temp->use                    = 1;
  }

  DEBUG_printf(("1ippNew: Returning %p", (void *)temp));

  return (temp);
}


/*
 *  'ippNewRequest()' - Allocate a new IPP request message.
 *
 * The new request message is initialized with the attributes-charset and
 * attributes-natural-language attributes added. The
 * attributes-natural-language value is derived from the current locale.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

ipp_t *					/* O - IPP request message */
ippNewRequest(ipp_op_t op)		/* I - Operation code */
{
  ipp_t		*request;		/* IPP request message */
  cups_lang_t	*language;		/* Current language localization */
  static int	request_id = 0;		/* Current request ID */
  static _cups_mutex_t request_mutex = _CUPS_MUTEX_INITIALIZER;
					/* Mutex for request ID */


  DEBUG_printf(("ippNewRequest(op=%02x(%s))", op, ippOpString(op)));

 /*
  * Create a new IPP message...
  */

  if ((request = ippNew()) == NULL)
    return (NULL);

 /*
  * Set the operation and request ID...
  */

  _cupsMutexLock(&request_mutex);

  request->request.op.operation_id = op;
  request->request.op.request_id   = ++request_id;

  _cupsMutexUnlock(&request_mutex);

 /*
  * Use UTF-8 as the character set...
  */

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, "utf-8");

 /*
  * Get the language from the current locale...
  */

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

 /*
  * Return the new request...
  */

  return (request);
}


/*
 * 'ippNewResponse()' - Allocate a new IPP response message.
 *
 * The new response message is initialized with the same version-number,
 * request-id, attributes-charset, and attributes-natural-language as the
 * provided request message.  If the attributes-charset or
 * attributes-natural-language attributes are missing from the request,
 * "utf-8" and a value derived from the current locale are substituted,
 * respectively.
 *
 * @since CUPS 1.7/macOS 10.9@
 */

ipp_t *					/* O - IPP response message */
ippNewResponse(ipp_t *request)		/* I - IPP request message */
{
  ipp_t			*response;	/* IPP response message */
  ipp_attribute_t	*attr;		/* Current attribute */


 /*
  * Range check input...
  */

  if (!request)
    return (NULL);

 /*
  * Create a new IPP message...
  */

  if ((response = ippNew()) == NULL)
    return (NULL);

 /*
  * Copy the request values over to the response...
  */

  response->request.status.version[0] = request->request.op.version[0];
  response->request.status.version[1] = request->request.op.version[1];
  response->request.status.request_id = request->request.op.request_id;

 /*
  * The first attribute MUST be attributes-charset...
  */

  attr = request->attrs;

  if (attr && attr->name && !strcmp(attr->name, "attributes-charset") &&
      attr->group_tag == IPP_TAG_OPERATION &&
      attr->value_tag == IPP_TAG_CHARSET &&
      attr->num_values == 1)
  {
   /*
    * Copy charset from request...
    */

    ippAddString(response, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
		 "attributes-charset", NULL, attr->values[0].string.text);
  }
  else
  {
   /*
    * Use "utf-8" as the default...
    */

    ippAddString(response, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
		 "attributes-charset", NULL, "utf-8");
  }

 /*
  * Then attributes-natural-language...
  */

  if (attr)
    attr = attr->next;

  if (attr && attr->name &&
      !strcmp(attr->name, "attributes-natural-language") &&
      attr->group_tag == IPP_TAG_OPERATION &&
      attr->value_tag == IPP_TAG_LANGUAGE &&
      attr->num_values == 1)
  {
   /*
    * Copy language from request...
    */

    ippAddString(response, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
		 "attributes-natural-language", NULL,
		 attr->values[0].string.text);
  }
  else
  {
   /*
    * Use the language from the current locale...
    */

    cups_lang_t *language = cupsLangDefault();
					/* Current locale */

    ippAddString(response, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
		 "attributes-natural-language", NULL, language->language);
  }

  return (response);
}


/*
 * 'ippRead()' - Read data for an IPP message from a HTTP connection.
 */

ipp_state_t				/* O - Current state */
ippRead(http_t *http,			/* I - HTTP connection */
        ipp_t  *ipp)			/* I - IPP data */
{
  DEBUG_printf(("ippRead(http=%p, ipp=%p), data_remaining=" CUPS_LLFMT, (void *)http, (void *)ipp, CUPS_LLCAST (http ? http->data_remaining : -1)));

  if (!http)
    return (IPP_STATE_ERROR);

  DEBUG_printf(("2ippRead: http->state=%d, http->used=%d", http->state, http->used));

  return (ippReadIO(http, (ipp_iocb_t)ipp_read_http, http->blocking, NULL,
                    ipp));
}


/*
 * 'ippReadFile()' - Read data for an IPP message from a file.
 *
 * @since CUPS 1.1.19/macOS 10.3@
 */

ipp_state_t				/* O - Current state */
ippReadFile(int   fd,			/* I - HTTP data */
            ipp_t *ipp)			/* I - IPP data */
{
  DEBUG_printf(("ippReadFile(fd=%d, ipp=%p)", fd, (void *)ipp));

  return (ippReadIO(&fd, (ipp_iocb_t)ipp_read_file, 1, NULL, ipp));
}


/*
 * 'ippReadIO()' - Read data for an IPP message.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

ipp_state_t				/* O - Current state */
ippReadIO(void       *src,		/* I - Data source */
          ipp_iocb_t cb,		/* I - Read callback function */
	  int        blocking,		/* I - Use blocking IO? */
	  ipp_t      *parent,		/* I - Parent request, if any */
          ipp_t      *ipp)		/* I - IPP data */
{
  int			n;		/* Length of data */
  unsigned char		*buffer,	/* Data buffer */
			string[IPP_MAX_TEXT],
					/* Small string buffer */
			*bufptr;	/* Pointer into buffer */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_tag_t		tag;		/* Current tag */
  ipp_tag_t		value_tag;	/* Current value tag */
  _ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("ippReadIO(src=%p, cb=%p, blocking=%d, parent=%p, ipp=%p)", (void *)src, (void *)cb, blocking, (void *)parent, (void *)ipp));
  DEBUG_printf(("2ippReadIO: ipp->state=%d", ipp ? ipp->state : IPP_STATE_ERROR));

  if (!src || !ipp)
    return (IPP_STATE_ERROR);

  if ((buffer = (unsigned char *)_cupsBufferGet(IPP_BUF_SIZE)) == NULL)
  {
    DEBUG_puts("1ippReadIO: Unable to get read buffer.");
    return (IPP_STATE_ERROR);
  }

  switch (ipp->state)
  {
    case IPP_STATE_IDLE :
        ipp->state ++; /* Avoid common problem... */

    case IPP_STATE_HEADER :
        if (parent == NULL)
	{
	 /*
          * Get the request header...
	  */

          if ((*cb)(src, buffer, 8) < 8)
	  {
	    DEBUG_puts("1ippReadIO: Unable to read header.");
	    _cupsBufferRelease((char *)buffer);
	    return (IPP_STATE_ERROR);
	  }

	 /*
          * Then copy the request header over...
	  */

          ipp->request.any.version[0]  = buffer[0];
          ipp->request.any.version[1]  = buffer[1];
          ipp->request.any.op_status   = (buffer[2] << 8) | buffer[3];
          ipp->request.any.request_id  = (((((buffer[4] << 8) | buffer[5]) << 8) |
	                        	 buffer[6]) << 8) | buffer[7];

          DEBUG_printf(("2ippReadIO: version=%d.%d", buffer[0], buffer[1]));
	  DEBUG_printf(("2ippReadIO: op_status=%04x",
	                ipp->request.any.op_status));
	  DEBUG_printf(("2ippReadIO: request_id=%d",
	                ipp->request.any.request_id));
        }

        ipp->state   = IPP_STATE_ATTRIBUTE;
	ipp->current = NULL;
	ipp->curtag  = IPP_TAG_ZERO;
	ipp->prev    = ipp->last;

       /*
        * If blocking is disabled, stop here...
	*/

        if (!blocking)
	  break;

    case IPP_STATE_ATTRIBUTE :
        for (;;)
	{
	  if ((*cb)(src, buffer, 1) < 1)
	  {
	    DEBUG_puts("1ippReadIO: Callback returned EOF/error");
	    _cupsBufferRelease((char *)buffer);
	    return (IPP_STATE_ERROR);
	  }

	  DEBUG_printf(("2ippReadIO: ipp->current=%p, ipp->prev=%p", (void *)ipp->current, (void *)ipp->prev));

	 /*
	  * Read this attribute...
	  */

          tag = (ipp_tag_t)buffer[0];
          if (tag == IPP_TAG_EXTENSION)
          {
           /*
            * Read 32-bit "extension" tag...
            */

	    if ((*cb)(src, buffer, 4) < 1)
	    {
	      DEBUG_puts("1ippReadIO: Callback returned EOF/error");
	      _cupsBufferRelease((char *)buffer);
	      return (IPP_STATE_ERROR);
	    }

	    tag = (ipp_tag_t)((((((buffer[0] << 8) | buffer[1]) << 8) |
	                        buffer[2]) << 8) | buffer[3]);

            if (tag & IPP_TAG_CUPS_CONST)
            {
             /*
              * Fail if the high bit is set in the tag...
              */

	      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("IPP extension tag larger than 0x7FFFFFFF."), 1);
	      DEBUG_printf(("1ippReadIO: bad tag 0x%x.", tag));
	      _cupsBufferRelease((char *)buffer);
	      return (IPP_STATE_ERROR);
            }
          }

	  if (tag == IPP_TAG_END)
	  {
	   /*
	    * No more attributes left...
	    */

            DEBUG_puts("2ippReadIO: IPP_TAG_END.");

	    ipp->state = IPP_STATE_DATA;
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
	    DEBUG_printf(("2ippReadIO: group tag=%x(%s), ipp->prev=%p", tag, ippTagString(tag), (void *)ipp->prev));
	    continue;
	  }

          DEBUG_printf(("2ippReadIO: value tag=%x(%s)", tag,
	                ippTagString(tag)));

         /*
	  * Get the name...
	  */

          if ((*cb)(src, buffer, 2) < 2)
	  {
	    DEBUG_puts("1ippReadIO: unable to read name length.");
	    _cupsBufferRelease((char *)buffer);
	    return (IPP_STATE_ERROR);
	  }

          n = (buffer[0] << 8) | buffer[1];

          if (n >= IPP_BUF_SIZE)
	  {
	    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("IPP name larger than 32767 bytes."), 1);
	    DEBUG_printf(("1ippReadIO: bad name length %d.", n));
	    _cupsBufferRelease((char *)buffer);
	    return (IPP_STATE_ERROR);
	  }

          DEBUG_printf(("2ippReadIO: name length=%d", n));

          if (n == 0 && tag != IPP_TAG_MEMBERNAME &&
	      tag != IPP_TAG_END_COLLECTION)
	  {
	   /*
	    * More values for current attribute...
	    */

            if (ipp->current == NULL)
	    {
	      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("IPP attribute has no name."), 1);
	      DEBUG_puts("1ippReadIO: Attribute without name and no current.");
	      _cupsBufferRelease((char *)buffer);
	      return (IPP_STATE_ERROR);
	    }

            attr      = ipp->current;
	    value_tag = (ipp_tag_t)(attr->value_tag & IPP_TAG_CUPS_MASK);

	   /*
	    * Make sure we aren't adding a new value of a different
	    * type...
	    */

	    if (value_tag == IPP_TAG_ZERO)
	    {
	     /*
	      * Setting the value of a collection member...
	      */

	      attr->value_tag = tag;
	    }
	    else if (value_tag == IPP_TAG_TEXTLANG ||
	             value_tag == IPP_TAG_NAMELANG ||
		     (value_tag >= IPP_TAG_TEXT &&
		      value_tag <= IPP_TAG_MIMETYPE))
            {
	     /*
	      * String values can sometimes come across in different
	      * forms; accept sets of differing values...
	      */

	      if (tag != IPP_TAG_TEXTLANG && tag != IPP_TAG_NAMELANG &&
	          (tag < IPP_TAG_TEXT || tag > IPP_TAG_MIMETYPE) &&
		  tag != IPP_TAG_NOVALUE)
	      {
		_cupsSetError(IPP_STATUS_ERROR_INTERNAL,
		              _("IPP 1setOf attribute with incompatible value "
		                "tags."), 1);
		DEBUG_printf(("1ippReadIO: 1setOf value tag %x(%s) != %x(%s)",
			      value_tag, ippTagString(value_tag), tag,
			      ippTagString(tag)));
		_cupsBufferRelease((char *)buffer);
	        return (IPP_STATE_ERROR);
	      }

              if (value_tag != tag)
              {
                DEBUG_printf(("1ippReadIO: Converting %s attribute from %s to %s.",
                              attr->name, ippTagString(value_tag), ippTagString(tag)));
		ippSetValueTag(ipp, &attr, tag);
	      }
            }
	    else if (value_tag == IPP_TAG_INTEGER ||
	             value_tag == IPP_TAG_RANGE)
            {
	     /*
	      * Integer and rangeOfInteger values can sometimes be mixed; accept
	      * sets of differing values...
	      */

	      if (tag != IPP_TAG_INTEGER && tag != IPP_TAG_RANGE)
	      {
		_cupsSetError(IPP_STATUS_ERROR_INTERNAL,
		              _("IPP 1setOf attribute with incompatible value "
		                "tags."), 1);
		DEBUG_printf(("1ippReadIO: 1setOf value tag %x(%s) != %x(%s)",
			      value_tag, ippTagString(value_tag), tag,
			      ippTagString(tag)));
		_cupsBufferRelease((char *)buffer);
	        return (IPP_STATE_ERROR);
	      }

              if (value_tag == IPP_TAG_INTEGER && tag == IPP_TAG_RANGE)
              {
               /*
                * Convert integer values to rangeOfInteger values...
                */

		DEBUG_printf(("1ippReadIO: Converting %s attribute to "
		              "rangeOfInteger.", attr->name));
                ippSetValueTag(ipp, &attr, IPP_TAG_RANGE);
              }
            }
	    else if (value_tag != tag)
	    {
	      _cupsSetError(IPP_STATUS_ERROR_INTERNAL,
			    _("IPP 1setOf attribute with incompatible value "
			      "tags."), 1);
	      DEBUG_printf(("1ippReadIO: value tag %x(%s) != %x(%s)",
	                    value_tag, ippTagString(value_tag), tag,
			    ippTagString(tag)));
	      _cupsBufferRelease((char *)buffer);
	      return (IPP_STATE_ERROR);
            }

           /*
	    * Finally, reallocate the attribute array as needed...
	    */

	    if ((value = ipp_set_value(ipp, &attr, attr->num_values)) == NULL)
	    {
	      _cupsBufferRelease((char *)buffer);
	      return (IPP_STATE_ERROR);
	    }
	  }
	  else if (tag == IPP_TAG_MEMBERNAME)
	  {
	   /*
	    * Name must be length 0!
	    */

	    if (n)
	    {
	      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("IPP member name is not empty."), 1);
	      DEBUG_puts("1ippReadIO: member name not empty.");
	      _cupsBufferRelease((char *)buffer);
	      return (IPP_STATE_ERROR);
	    }

            if (ipp->current)
	      ipp->prev = ipp->current;

	    attr = ipp->current = ipp_add_attr(ipp, NULL, ipp->curtag, IPP_TAG_ZERO, 1);
	    if (!attr)
	    {
	      _cupsSetHTTPError(HTTP_STATUS_ERROR);
	      DEBUG_puts("1ippReadIO: unable to allocate attribute.");
	      _cupsBufferRelease((char *)buffer);
	      return (IPP_STATE_ERROR);
	    }

	    DEBUG_printf(("2ippReadIO: membername, ipp->current=%p, ipp->prev=%p", (void *)ipp->current, (void *)ipp->prev));

	    value = attr->values;
	  }
	  else if (tag != IPP_TAG_END_COLLECTION)
	  {
	   /*
	    * New attribute; read the name and add it...
	    */

	    if ((*cb)(src, buffer, (size_t)n) < n)
	    {
	      DEBUG_puts("1ippReadIO: unable to read name.");
	      _cupsBufferRelease((char *)buffer);
	      return (IPP_STATE_ERROR);
	    }

	    buffer[n] = '\0';

            if (ipp->current)
	      ipp->prev = ipp->current;

	    if ((attr = ipp->current = ipp_add_attr(ipp, (char *)buffer, ipp->curtag, tag,
	                                            1)) == NULL)
	    {
	      _cupsSetHTTPError(HTTP_STATUS_ERROR);
	      DEBUG_puts("1ippReadIO: unable to allocate attribute.");
	      _cupsBufferRelease((char *)buffer);
	      return (IPP_STATE_ERROR);
	    }

	    DEBUG_printf(("2ippReadIO: name=\"%s\", ipp->current=%p, ipp->prev=%p", buffer, (void *)ipp->current, (void *)ipp->prev));

	    value = attr->values;
	  }
	  else
	  {
	    attr  = NULL;
	    value = NULL;
	  }

	  if ((*cb)(src, buffer, 2) < 2)
	  {
	    DEBUG_puts("1ippReadIO: unable to read value length.");
	    _cupsBufferRelease((char *)buffer);
	    return (IPP_STATE_ERROR);
	  }

	  n = (buffer[0] << 8) | buffer[1];
          DEBUG_printf(("2ippReadIO: value length=%d", n));

	  if (n >= IPP_BUF_SIZE)
	  {
	    _cupsSetError(IPP_STATUS_ERROR_INTERNAL,
			  _("IPP value larger than 32767 bytes."), 1);
	    DEBUG_printf(("1ippReadIO: bad value length %d.", n));
	    _cupsBufferRelease((char *)buffer);
	    return (IPP_STATE_ERROR);
	  }

	  switch (tag)
	  {
	    case IPP_TAG_INTEGER :
	    case IPP_TAG_ENUM :
		if (n != 4)
		{
		  if (tag == IPP_TAG_INTEGER)
		    _cupsSetError(IPP_STATUS_ERROR_INTERNAL,
				  _("IPP integer value not 4 bytes."), 1);
		  else
		    _cupsSetError(IPP_STATUS_ERROR_INTERNAL,
				  _("IPP enum value not 4 bytes."), 1);
		  DEBUG_printf(("1ippReadIO: bad integer value length %d.", n));
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
		}

	        if ((*cb)(src, buffer, 4) < 4)
		{
	          DEBUG_puts("1ippReadIO: Unable to read integer value.");
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
		}

		n = (((((buffer[0] << 8) | buffer[1]) << 8) | buffer[2]) << 8) |
		    buffer[3];

                if (attr->value_tag == IPP_TAG_RANGE)
                  value->range.lower = value->range.upper = n;
                else
		  value->integer = n;
	        break;

	    case IPP_TAG_BOOLEAN :
		if (n != 1)
		{
		  _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("IPP boolean value not 1 byte."),
		                1);
		  DEBUG_printf(("1ippReadIO: bad boolean value length %d.", n));
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
		}

	        if ((*cb)(src, buffer, 1) < 1)
		{
	          DEBUG_puts("1ippReadIO: Unable to read boolean value.");
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
		}

                value->boolean = (char)buffer[0];
	        break;

            case IPP_TAG_NOVALUE :
	    case IPP_TAG_NOTSETTABLE :
	    case IPP_TAG_DELETEATTR :
	    case IPP_TAG_ADMINDEFINE :
	       /*
	        * These value types are not supposed to have values, however
		* some vendors (Brother) do not implement IPP correctly and so
		* we need to map non-empty values to text...
		*/

	        if (attr->value_tag == tag)
		{
		  if (n == 0)
		    break;

		  attr->value_tag = IPP_TAG_TEXT;
		}

	    case IPP_TAG_TEXT :
	    case IPP_TAG_NAME :
	    case IPP_TAG_KEYWORD :
	    case IPP_TAG_URI :
	    case IPP_TAG_URISCHEME :
	    case IPP_TAG_CHARSET :
	    case IPP_TAG_LANGUAGE :
	    case IPP_TAG_MIMETYPE :
	        if (n > 0)
	        {
		  if ((*cb)(src, buffer, (size_t)n) < n)
		  {
		    DEBUG_puts("1ippReadIO: unable to read string value.");
		    _cupsBufferRelease((char *)buffer);
		    return (IPP_STATE_ERROR);
		  }
		}

		buffer[n] = '\0';
		value->string.text = _cupsStrAlloc((char *)buffer);
		DEBUG_printf(("2ippReadIO: value=\"%s\"", value->string.text));
	        break;

	    case IPP_TAG_DATE :
		if (n != 11)
		{
		  _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("IPP date value not 11 bytes."), 1);
		  DEBUG_printf(("1ippReadIO: bad date value length %d.", n));
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
		}

	        if ((*cb)(src, value->date, 11) < 11)
		{
	          DEBUG_puts("1ippReadIO: Unable to read date value.");
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
		}
	        break;

	    case IPP_TAG_RESOLUTION :
		if (n != 9)
		{
		  _cupsSetError(IPP_STATUS_ERROR_INTERNAL,
		                _("IPP resolution value not 9 bytes."), 1);
		  DEBUG_printf(("1ippReadIO: bad resolution value length %d.", n));
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
		}

	        if ((*cb)(src, buffer, 9) < 9)
		{
	          DEBUG_puts("1ippReadIO: Unable to read resolution value.");
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
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
		if (n != 8)
		{
		  _cupsSetError(IPP_STATUS_ERROR_INTERNAL,
		                _("IPP rangeOfInteger value not 8 bytes."), 1);
		  DEBUG_printf(("1ippReadIO: bad rangeOfInteger value length "
		                "%d.", n));
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
		}

	        if ((*cb)(src, buffer, 8) < 8)
		{
	          DEBUG_puts("1ippReadIO: Unable to read range value.");
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
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
	        if (n < 4)
		{
		  if (tag == IPP_TAG_TEXTLANG)
		    _cupsSetError(IPP_STATUS_ERROR_INTERNAL,
		                  _("IPP textWithLanguage value less than "
		                    "minimum 4 bytes."), 1);
		  else
		    _cupsSetError(IPP_STATUS_ERROR_INTERNAL,
		                  _("IPP nameWithLanguage value less than "
		                    "minimum 4 bytes."), 1);
		  DEBUG_printf(("1ippReadIO: bad stringWithLanguage value "
		                "length %d.", n));
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
		}

	        if ((*cb)(src, buffer, (size_t)n) < n)
		{
	          DEBUG_puts("1ippReadIO: Unable to read string w/language "
		             "value.");
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
		}

                bufptr = buffer;

	       /*
	        * text-with-language and name-with-language are composite
		* values:
		*
		*    language-length
		*    language
		*    text-length
		*    text
		*/

		n = (bufptr[0] << 8) | bufptr[1];

		if ((bufptr + 2 + n) >= (buffer + IPP_BUF_SIZE) || n >= (int)sizeof(string))
		{
		  _cupsSetError(IPP_STATUS_ERROR_INTERNAL,
		                _("IPP language length overflows value."), 1);
		  DEBUG_printf(("1ippReadIO: bad language value length %d.",
		                n));
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
		}
		else if (n >= IPP_MAX_LANGUAGE)
		{
		  _cupsSetError(IPP_STATUS_ERROR_INTERNAL,
		                _("IPP language length too large."), 1);
		  DEBUG_printf(("1ippReadIO: bad language value length %d.",
		                n));
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
		}

		memcpy(string, bufptr + 2, (size_t)n);
		string[n] = '\0';

		value->string.language = _cupsStrAlloc((char *)string);

                bufptr += 2 + n;
		n = (bufptr[0] << 8) | bufptr[1];

		if ((bufptr + 2 + n) >= (buffer + IPP_BUF_SIZE))
		{
		  _cupsSetError(IPP_STATUS_ERROR_INTERNAL,
		                _("IPP string length overflows value."), 1);
		  DEBUG_printf(("1ippReadIO: bad string value length %d.", n));
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
		}

		bufptr[2 + n] = '\0';
                value->string.text = _cupsStrAlloc((char *)bufptr + 2);
	        break;

            case IPP_TAG_BEGIN_COLLECTION :
	       /*
	        * Oh, boy, here comes a collection value, so read it...
		*/

                value->collection = ippNew();

                if (n > 0)
		{
		  _cupsSetError(IPP_STATUS_ERROR_INTERNAL,
		                _("IPP begCollection value not 0 bytes."), 1);
	          DEBUG_puts("1ippReadIO: begCollection tag with value length "
		             "> 0.");
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
		}

		if (ippReadIO(src, cb, 1, ipp, value->collection) == IPP_STATE_ERROR)
		{
	          DEBUG_puts("1ippReadIO: Unable to read collection value.");
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
		}
                break;

            case IPP_TAG_END_COLLECTION :
		_cupsBufferRelease((char *)buffer);

                if (n > 0)
		{
		  _cupsSetError(IPP_STATUS_ERROR_INTERNAL,
		                _("IPP endCollection value not 0 bytes."), 1);
	          DEBUG_puts("1ippReadIO: endCollection tag with value length "
		             "> 0.");
		  return (IPP_STATE_ERROR);
		}

	        DEBUG_puts("1ippReadIO: endCollection tag...");
		return (ipp->state = IPP_STATE_DATA);

            case IPP_TAG_MEMBERNAME :
	       /*
	        * The value the name of the member in the collection, which
		* we need to carry over...
		*/

                if (!attr)
                {
		  _cupsSetError(IPP_STATUS_ERROR_INTERNAL,
		                _("IPP memberName with no attribute."), 1);
	          DEBUG_puts("1ippReadIO: Member name without attribute.");
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
                }
		else if (n == 0)
		{
		  _cupsSetError(IPP_STATUS_ERROR_INTERNAL,
		                _("IPP memberName value is empty."), 1);
	          DEBUG_puts("1ippReadIO: Empty member name value.");
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
		}
		else if ((*cb)(src, buffer, (size_t)n) < n)
		{
	          DEBUG_puts("1ippReadIO: Unable to read member name value.");
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
		}

		buffer[n] = '\0';
		attr->name = _cupsStrAlloc((char *)buffer);

               /*
	        * Since collection members are encoded differently than
		* regular attributes, make sure we don't start with an
		* empty value...
		*/

                attr->num_values --;

		DEBUG_printf(("2ippReadIO: member name=\"%s\"", attr->name));
		break;

            default : /* Other unsupported values */
                if (tag == IPP_TAG_STRING && n > IPP_MAX_LENGTH)
		{
		  _cupsSetError(IPP_STATUS_ERROR_INTERNAL,
		                _("IPP octetString length too large."), 1);
		  DEBUG_printf(("1ippReadIO: bad octetString value length %d.",
		                n));
		  _cupsBufferRelease((char *)buffer);
		  return (IPP_STATE_ERROR);
		}

                value->unknown.length = n;

	        if (n > 0)
		{
		  if ((value->unknown.data = malloc((size_t)n)) == NULL)
		  {
		    _cupsSetHTTPError(HTTP_STATUS_ERROR);
		    DEBUG_puts("1ippReadIO: Unable to allocate value");
		    _cupsBufferRelease((char *)buffer);
		    return (IPP_STATE_ERROR);
		  }

	          if ((*cb)(src, value->unknown.data, (size_t)n) < n)
		  {
	            DEBUG_puts("1ippReadIO: Unable to read unsupported value.");
		    _cupsBufferRelease((char *)buffer);
		    return (IPP_STATE_ERROR);
		  }
		}
		else
		  value->unknown.data = NULL;
	        break;
	  }

	 /*
          * If blocking is disabled, stop here...
	  */

          if (!blocking)
	    break;
	}
        break;

    case IPP_STATE_DATA :
        break;

    default :
        break; /* anti-compiler-warning-code */
  }

  DEBUG_printf(("1ippReadIO: returning ipp->state=%d.", ipp->state));
  _cupsBufferRelease((char *)buffer);

  return (ipp->state);
}


/*
 * 'ippSetBoolean()' - Set a boolean value in an attribute.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code attr@ parameter may be modified as a result of setting the value.
 *
 * The @code element@ parameter specifies which value to set from 0 to
 * @link ippGetCount(attr)@.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O  - 1 on success, 0 on failure */
ippSetBoolean(ipp_t           *ipp,	/* I  - IPP message */
              ipp_attribute_t **attr,	/* IO - IPP attribute */
              int             element,	/* I  - Value number (0-based) */
              int             boolvalue)/* I  - Boolean value */
{
  _ipp_value_t	*value;			/* Current value */


 /*
  * Range check input...
  */

  if (!ipp || !attr || !*attr || (*attr)->value_tag != IPP_TAG_BOOLEAN ||
      element < 0 || element > (*attr)->num_values)
    return (0);

 /*
  * Set the value and return...
  */

  if ((value = ipp_set_value(ipp, attr, element)) != NULL)
    value->boolean = (char)boolvalue;

  return (value != NULL);
}


/*
 * 'ippSetCollection()' - Set a collection value in an attribute.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code attr@ parameter may be modified as a result of setting the value.
 *
 * The @code element@ parameter specifies which value to set from 0 to
 * @link ippGetCount(attr)@.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O  - 1 on success, 0 on failure */
ippSetCollection(
    ipp_t           *ipp,		/* I  - IPP message */
    ipp_attribute_t **attr,		/* IO - IPP attribute */
    int             element,		/* I  - Value number (0-based) */
    ipp_t           *colvalue)		/* I  - Collection value */
{
  _ipp_value_t	*value;			/* Current value */


 /*
  * Range check input...
  */

  if (!ipp || !attr || !*attr || (*attr)->value_tag != IPP_TAG_BEGIN_COLLECTION ||
      element < 0 || element > (*attr)->num_values || !colvalue)
    return (0);

 /*
  * Set the value and return...
  */

  if ((value = ipp_set_value(ipp, attr, element)) != NULL)
  {
    if (value->collection)
      ippDelete(value->collection);

    value->collection = colvalue;
    colvalue->use ++;
  }

  return (value != NULL);
}


/*
 * 'ippSetDate()' - Set a date value in an attribute.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code attr@ parameter may be modified as a result of setting the value.
 *
 * The @code element@ parameter specifies which value to set from 0 to
 * @link ippGetCount(attr)@.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O  - 1 on success, 0 on failure */
ippSetDate(ipp_t             *ipp,	/* I  - IPP message */
           ipp_attribute_t   **attr,	/* IO - IPP attribute */
           int               element,	/* I  - Value number (0-based) */
           const ipp_uchar_t *datevalue)/* I  - Date value */
{
  _ipp_value_t	*value;			/* Current value */


 /*
  * Range check input...
  */

  if (!ipp || !attr || !*attr || (*attr)->value_tag != IPP_TAG_DATE ||
      element < 0 || element > (*attr)->num_values || !datevalue)
    return (0);

 /*
  * Set the value and return...
  */

  if ((value = ipp_set_value(ipp, attr, element)) != NULL)
    memcpy(value->date, datevalue, sizeof(value->date));

  return (value != NULL);
}


/*
 * 'ippSetGroupTag()' - Set the group tag of an attribute.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code attr@ parameter may be modified as a result of setting the value.
 *
 * The @code group@ parameter specifies the IPP attribute group tag: none
 * (@code IPP_TAG_ZERO@, for member attributes), document (@code IPP_TAG_DOCUMENT@),
 * event notification (@code IPP_TAG_EVENT_NOTIFICATION@), operation
 * (@code IPP_TAG_OPERATION@), printer (@code IPP_TAG_PRINTER@), subscription
 * (@code IPP_TAG_SUBSCRIPTION@), or unsupported (@code IPP_TAG_UNSUPPORTED_GROUP@).
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O  - 1 on success, 0 on failure */
ippSetGroupTag(
    ipp_t           *ipp,		/* I  - IPP message */
    ipp_attribute_t **attr,		/* IO - Attribute */
    ipp_tag_t       group_tag)		/* I  - Group tag */
{
 /*
  * Range check input - group tag must be 0x01 to 0x0F, per RFC 2911...
  */

  if (!ipp || !attr || !*attr ||
      group_tag < IPP_TAG_ZERO || group_tag == IPP_TAG_END ||
      group_tag >= IPP_TAG_UNSUPPORTED_VALUE)
    return (0);

 /*
  * Set the group tag and return...
  */

  (*attr)->group_tag = group_tag;

  return (1);
}


/*
 * 'ippSetInteger()' - Set an integer or enum value in an attribute.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code attr@ parameter may be modified as a result of setting the value.
 *
 * The @code element@ parameter specifies which value to set from 0 to
 * @link ippGetCount(attr)@.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O  - 1 on success, 0 on failure */
ippSetInteger(ipp_t           *ipp,	/* I  - IPP message */
              ipp_attribute_t **attr,	/* IO - IPP attribute */
              int             element,	/* I  - Value number (0-based) */
              int             intvalue)	/* I  - Integer/enum value */
{
  _ipp_value_t	*value;			/* Current value */


 /*
  * Range check input...
  */

  if (!ipp || !attr || !*attr ||
      ((*attr)->value_tag != IPP_TAG_INTEGER && (*attr)->value_tag != IPP_TAG_ENUM) ||
      element < 0 || element > (*attr)->num_values)
    return (0);

 /*
  * Set the value and return...
  */

  if ((value = ipp_set_value(ipp, attr, element)) != NULL)
    value->integer = intvalue;

  return (value != NULL);
}


/*
 * 'ippSetName()' - Set the name of an attribute.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code attr@ parameter may be modified as a result of setting the value.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O  - 1 on success, 0 on failure */
ippSetName(ipp_t           *ipp,	/* I  - IPP message */
	   ipp_attribute_t **attr,	/* IO - IPP attribute */
	   const char      *name)	/* I  - Attribute name */
{
  char	*temp;				/* Temporary name value */


 /*
  * Range check input...
  */

  if (!ipp || !attr || !*attr)
    return (0);

 /*
  * Set the value and return...
  */

  if ((temp = _cupsStrAlloc(name)) != NULL)
  {
    if ((*attr)->name)
      _cupsStrFree((*attr)->name);

    (*attr)->name = temp;
  }

  return (temp != NULL);
}


/*
 * 'ippSetOctetString()' - Set an octetString value in an IPP attribute.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code attr@ parameter may be modified as a result of setting the value.
 *
 * The @code element@ parameter specifies which value to set from 0 to
 * @link ippGetCount(attr)@.
 *
 * @since CUPS 1.7/macOS 10.9@
 */

int					/* O  - 1 on success, 0 on failure */
ippSetOctetString(
    ipp_t           *ipp,		/* I  - IPP message */
    ipp_attribute_t **attr,		/* IO - IPP attribute */
    int             element,		/* I  - Value number (0-based) */
    const void      *data,		/* I  - Pointer to octetString data */
    int             datalen)		/* I  - Length of octetString data */
{
  _ipp_value_t	*value;			/* Current value */


 /*
  * Range check input...
  */

  if (!ipp || !attr || !*attr || (*attr)->value_tag != IPP_TAG_STRING ||
      element < 0 || element > (*attr)->num_values ||
      datalen < 0 || datalen > IPP_MAX_LENGTH)
    return (0);

 /*
  * Set the value and return...
  */

  if ((value = ipp_set_value(ipp, attr, element)) != NULL)
  {
    if ((int)((*attr)->value_tag) & IPP_TAG_CUPS_CONST)
    {
     /*
      * Just copy the pointer...
      */

      value->unknown.data   = (void *)data;
      value->unknown.length = datalen;
    }
    else
    {
     /*
      * Copy the data...
      */

      if (value->unknown.data)
      {
       /*
	* Free previous data...
	*/

	free(value->unknown.data);

	value->unknown.data   = NULL;
        value->unknown.length = 0;
      }

      if (datalen > 0)
      {
	void	*temp;			/* Temporary data pointer */

	if ((temp = malloc((size_t)datalen)) != NULL)
	{
	  memcpy(temp, data, (size_t)datalen);

	  value->unknown.data   = temp;
	  value->unknown.length = datalen;
	}
	else
	  return (0);
      }
    }
  }

  return (value != NULL);
}


/*
 * 'ippSetOperation()' - Set the operation ID in an IPP request message.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O - 1 on success, 0 on failure */
ippSetOperation(ipp_t    *ipp,		/* I - IPP request message */
                ipp_op_t op)		/* I - Operation ID */
{
 /*
  * Range check input...
  */

  if (!ipp)
    return (0);

 /*
  * Set the operation and return...
  */

  ipp->request.op.operation_id = op;

  return (1);
}


/*
 * 'ippSetRange()' - Set a rangeOfInteger value in an attribute.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code attr@ parameter may be modified as a result of setting the value.
 *
 * The @code element@ parameter specifies which value to set from 0 to
 * @link ippGetCount(attr)@.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O  - 1 on success, 0 on failure */
ippSetRange(ipp_t           *ipp,	/* I  - IPP message */
            ipp_attribute_t **attr,	/* IO - IPP attribute */
            int             element,	/* I  - Value number (0-based) */
	    int             lowervalue,	/* I  - Lower bound for range */
	    int             uppervalue)	/* I  - Upper bound for range */
{
  _ipp_value_t	*value;			/* Current value */


 /*
  * Range check input...
  */

  if (!ipp || !attr || !*attr || (*attr)->value_tag != IPP_TAG_RANGE ||
      element < 0 || element > (*attr)->num_values || lowervalue > uppervalue)
    return (0);

 /*
  * Set the value and return...
  */

  if ((value = ipp_set_value(ipp, attr, element)) != NULL)
  {
    value->range.lower = lowervalue;
    value->range.upper = uppervalue;
  }

  return (value != NULL);
}


/*
 * 'ippSetRequestId()' - Set the request ID in an IPP message.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code request_id@ parameter must be greater than 0.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O - 1 on success, 0 on failure */
ippSetRequestId(ipp_t *ipp,		/* I - IPP message */
                int   request_id)	/* I - Request ID */
{
 /*
  * Range check input; not checking request_id values since ipptool wants to send
  * invalid values for conformance testing and a bad request_id does not affect the
  * encoding of a message...
  */

  if (!ipp)
    return (0);

 /*
  * Set the request ID and return...
  */

  ipp->request.any.request_id = request_id;

  return (1);
}


/*
 * 'ippSetResolution()' - Set a resolution value in an attribute.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code attr@ parameter may be modified as a result of setting the value.
 *
 * The @code element@ parameter specifies which value to set from 0 to
 * @link ippGetCount(attr)@.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O  - 1 on success, 0 on failure */
ippSetResolution(
    ipp_t           *ipp,		/* I  - IPP message */
    ipp_attribute_t **attr,		/* IO - IPP attribute */
    int             element,		/* I  - Value number (0-based) */
    ipp_res_t       unitsvalue,		/* I  - Resolution units */
    int             xresvalue,		/* I  - Horizontal/cross feed resolution */
    int             yresvalue)		/* I  - Vertical/feed resolution */
{
  _ipp_value_t	*value;			/* Current value */


 /*
  * Range check input...
  */

  if (!ipp || !attr || !*attr || (*attr)->value_tag != IPP_TAG_RESOLUTION ||
      element < 0 || element > (*attr)->num_values || xresvalue <= 0 || yresvalue <= 0 ||
      unitsvalue < IPP_RES_PER_INCH || unitsvalue > IPP_RES_PER_CM)
    return (0);

 /*
  * Set the value and return...
  */

  if ((value = ipp_set_value(ipp, attr, element)) != NULL)
  {
    value->resolution.units = unitsvalue;
    value->resolution.xres  = xresvalue;
    value->resolution.yres  = yresvalue;
  }

  return (value != NULL);
}


/*
 * 'ippSetState()' - Set the current state of the IPP message.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O - 1 on success, 0 on failure */
ippSetState(ipp_t       *ipp,		/* I - IPP message */
            ipp_state_t state)		/* I - IPP state value */
{
 /*
  * Range check input...
  */

  if (!ipp)
    return (0);

 /*
  * Set the state and return...
  */

  ipp->state   = state;
  ipp->current = NULL;

  return (1);
}


/*
 * 'ippSetStatusCode()' - Set the status code in an IPP response or event message.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O - 1 on success, 0 on failure */
ippSetStatusCode(ipp_t        *ipp,	/* I - IPP response or event message */
                 ipp_status_t status)	/* I - Status code */
{
 /*
  * Range check input...
  */

  if (!ipp)
    return (0);

 /*
  * Set the status code and return...
  */

  ipp->request.status.status_code = status;

  return (1);
}


/*
 * 'ippSetString()' - Set a string value in an attribute.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code attr@ parameter may be modified as a result of setting the value.
 *
 * The @code element@ parameter specifies which value to set from 0 to
 * @link ippGetCount(attr)@.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O  - 1 on success, 0 on failure */
ippSetString(ipp_t           *ipp,	/* I  - IPP message */
             ipp_attribute_t **attr,	/* IO - IPP attribute */
             int             element,	/* I  - Value number (0-based) */
	     const char      *strvalue)	/* I  - String value */
{
  char		*temp;			/* Temporary string */
  _ipp_value_t	*value;			/* Current value */


 /*
  * Range check input...
  */

  if (!ipp || !attr || !*attr ||
      ((*attr)->value_tag != IPP_TAG_TEXTLANG &&
      (*attr)->value_tag != IPP_TAG_NAMELANG &&
       ((*attr)->value_tag < IPP_TAG_TEXT ||
        (*attr)->value_tag > IPP_TAG_MIMETYPE)) ||
      element < 0 || element > (*attr)->num_values || !strvalue)
    return (0);

 /*
  * Set the value and return...
  */

  if ((value = ipp_set_value(ipp, attr, element)) != NULL)
  {
    if (element > 0)
      value->string.language = (*attr)->values[0].string.language;

    if ((int)((*attr)->value_tag) & IPP_TAG_CUPS_CONST)
      value->string.text = (char *)strvalue;
    else if ((temp = _cupsStrAlloc(strvalue)) != NULL)
    {
      if (value->string.text)
        _cupsStrFree(value->string.text);

      value->string.text = temp;
    }
    else
      return (0);
  }

  return (value != NULL);
}


/*
 * 'ippSetStringf()' - Set a formatted string value of an attribute.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code attr@ parameter may be modified as a result of setting the value.
 *
 * The @code element@ parameter specifies which value to set from 0 to
 * @link ippGetCount(attr)@.
 *
 * The @code format@ parameter uses formatting characters compatible with the
 * printf family of standard functions.  Additional arguments follow it as
 * needed.  The formatted string is truncated as needed to the maximum length of
 * the corresponding value type.
 *
 * @since CUPS 1.7/macOS 10.9@
 */

int					/* O  - 1 on success, 0 on failure */
ippSetStringf(ipp_t           *ipp,	/* I  - IPP message */
              ipp_attribute_t **attr,	/* IO - IPP attribute */
              int             element,	/* I  - Value number (0-based) */
	      const char      *format,	/* I  - Printf-style format string */
	      ...)			/* I  - Additional arguments as needed */
{
  int		ret;			/* Return value */
  va_list	ap;			/* Pointer to additional arguments */


  va_start(ap, format);
  ret = ippSetStringfv(ipp, attr, element, format, ap);
  va_end(ap);

  return (ret);
}


/*
 * 'ippSetStringf()' - Set a formatted string value of an attribute.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code attr@ parameter may be modified as a result of setting the value.
 *
 * The @code element@ parameter specifies which value to set from 0 to
 * @link ippGetCount(attr)@.
 *
 * The @code format@ parameter uses formatting characters compatible with the
 * printf family of standard functions.  Additional arguments follow it as
 * needed.  The formatted string is truncated as needed to the maximum length of
 * the corresponding value type.
 *
 * @since CUPS 1.7/macOS 10.9@
 */

int					/* O  - 1 on success, 0 on failure */
ippSetStringfv(ipp_t           *ipp,	/* I  - IPP message */
               ipp_attribute_t **attr,	/* IO - IPP attribute */
               int             element,	/* I  - Value number (0-based) */
	       const char      *format,	/* I  - Printf-style format string */
	       va_list         ap)	/* I  - Pointer to additional arguments */
{
  ipp_tag_t	value_tag;		/* Value tag */
  char		buffer[IPP_MAX_TEXT + 4];
					/* Formatted text string */
  ssize_t	bytes,			/* Length of formatted value */
		max_bytes;		/* Maximum number of bytes for value */


 /*
  * Range check input...
  */

  if (attr && *attr)
    value_tag = (*attr)->value_tag & IPP_TAG_CUPS_MASK;
  else
    value_tag = IPP_TAG_ZERO;

  if (!ipp || !attr || !*attr ||
      (value_tag < IPP_TAG_TEXT && value_tag != IPP_TAG_TEXTLANG &&
       value_tag != IPP_TAG_NAMELANG) || value_tag > IPP_TAG_MIMETYPE ||
      !format)
    return (0);

 /*
  * Format the string...
  */

  if (!strcmp(format, "%s"))
  {
   /*
    * Optimize the simple case...
    */

    const char *s = va_arg(ap, char *);

    if (!s)
      s = "(null)";

    bytes = (ssize_t)strlen(s);
    strlcpy(buffer, s, sizeof(buffer));
  }
  else
  {
   /*
    * Do a full formatting of the message...
    */

    if ((bytes = vsnprintf(buffer, sizeof(buffer), format, ap)) < 0)
      return (0);
  }

 /*
  * Limit the length of the string...
  */

  switch (value_tag)
  {
    default :
    case IPP_TAG_TEXT :
    case IPP_TAG_TEXTLANG :
        max_bytes = IPP_MAX_TEXT;
        break;

    case IPP_TAG_NAME :
    case IPP_TAG_NAMELANG :
        max_bytes = IPP_MAX_NAME;
        break;

    case IPP_TAG_CHARSET :
        max_bytes = IPP_MAX_CHARSET;
        break;

    case IPP_TAG_KEYWORD :
        max_bytes = IPP_MAX_KEYWORD;
        break;

    case IPP_TAG_LANGUAGE :
        max_bytes = IPP_MAX_LANGUAGE;
        break;

    case IPP_TAG_MIMETYPE :
        max_bytes = IPP_MAX_MIMETYPE;
        break;

    case IPP_TAG_URI :
        max_bytes = IPP_MAX_URI;
        break;

    case IPP_TAG_URISCHEME :
        max_bytes = IPP_MAX_URISCHEME;
        break;
  }

  if (bytes >= max_bytes)
  {
    char	*bufmax,		/* Buffer at max_bytes */
		*bufptr;		/* Pointer into buffer */

    bufptr = buffer + strlen(buffer) - 1;
    bufmax = buffer + max_bytes - 1;

    while (bufptr > bufmax)
    {
      if (*bufptr & 0x80)
      {
        while ((*bufptr & 0xc0) == 0x80 && bufptr > buffer)
          bufptr --;
      }

      bufptr --;
    }

    *bufptr = '\0';
  }

 /*
  * Set the formatted string and return...
  */

  return (ippSetString(ipp, attr, element, buffer));
}


/*
 * 'ippSetValueTag()' - Set the value tag of an attribute.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The @code attr@ parameter may be modified as a result of setting the value.
 *
 * Integer (@code IPP_TAG_INTEGER@) values can be promoted to rangeOfInteger
 * (@code IPP_TAG_RANGE@) values, the various string tags can be promoted to name
 * (@code IPP_TAG_NAME@) or nameWithLanguage (@code IPP_TAG_NAMELANG@) values, text
 * (@code IPP_TAG_TEXT@) values can be promoted to textWithLanguage
 * (@code IPP_TAG_TEXTLANG@) values, and all values can be demoted to the various
 * out-of-band value tags such as no-value (@code IPP_TAG_NOVALUE@). All other changes
 * will be rejected.
 *
 * Promoting a string attribute to nameWithLanguage or textWithLanguage adds the language
 * code in the "attributes-natural-language" attribute or, if not present, the language
 * code for the current locale.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O  - 1 on success, 0 on failure */
ippSetValueTag(
    ipp_t          *ipp,		/* I  - IPP message */
    ipp_attribute_t **attr,		/* IO - IPP attribute */
    ipp_tag_t       value_tag)		/* I  - Value tag */
{
  int		i;			/* Looping var */
  _ipp_value_t	*value;			/* Current value */
  int		integer;		/* Current integer value */
  cups_lang_t	*language;		/* Current language */
  char		code[32];		/* Language code */
  ipp_tag_t	temp_tag;		/* Temporary value tag */


 /*
  * Range check input...
  */

  if (!ipp || !attr || !*attr)
    return (0);

 /*
  * If there is no change, return immediately...
  */

  if (value_tag == (*attr)->value_tag)
    return (1);

 /*
  * Otherwise implement changes as needed...
  */

  temp_tag = (ipp_tag_t)((int)((*attr)->value_tag) & IPP_TAG_CUPS_MASK);

  switch (value_tag)
  {
    case IPP_TAG_UNSUPPORTED_VALUE :
    case IPP_TAG_DEFAULT :
    case IPP_TAG_UNKNOWN :
    case IPP_TAG_NOVALUE :
    case IPP_TAG_NOTSETTABLE :
    case IPP_TAG_DELETEATTR :
    case IPP_TAG_ADMINDEFINE :
       /*
        * Free any existing values...
        */

        if ((*attr)->num_values > 0)
          ipp_free_values(*attr, 0, (*attr)->num_values);

       /*
        * Set out-of-band value...
        */

        (*attr)->value_tag = value_tag;
        break;

    case IPP_TAG_RANGE :
        if (temp_tag != IPP_TAG_INTEGER)
          return (0);

        for (i = (*attr)->num_values, value = (*attr)->values;
             i > 0;
             i --, value ++)
        {
          integer            = value->integer;
          value->range.lower = value->range.upper = integer;
        }

        (*attr)->value_tag = IPP_TAG_RANGE;
        break;

    case IPP_TAG_NAME :
        if (temp_tag != IPP_TAG_KEYWORD && temp_tag != IPP_TAG_URI &&
            temp_tag != IPP_TAG_URISCHEME && temp_tag != IPP_TAG_LANGUAGE &&
            temp_tag != IPP_TAG_MIMETYPE)
          return (0);

        (*attr)->value_tag = (ipp_tag_t)(IPP_TAG_NAME | ((*attr)->value_tag & IPP_TAG_CUPS_CONST));
        break;

    case IPP_TAG_NAMELANG :
    case IPP_TAG_TEXTLANG :
        if (value_tag == IPP_TAG_NAMELANG &&
            (temp_tag != IPP_TAG_NAME && temp_tag != IPP_TAG_KEYWORD &&
             temp_tag != IPP_TAG_URI && temp_tag != IPP_TAG_URISCHEME &&
             temp_tag != IPP_TAG_LANGUAGE && temp_tag != IPP_TAG_MIMETYPE))
          return (0);

        if (value_tag == IPP_TAG_TEXTLANG && temp_tag != IPP_TAG_TEXT)
          return (0);

        if (ipp->attrs && ipp->attrs->next && ipp->attrs->next->name &&
            !strcmp(ipp->attrs->next->name, "attributes-natural-language"))
        {
         /*
          * Use the language code from the IPP message...
          */

	  (*attr)->values[0].string.language =
	      _cupsStrAlloc(ipp->attrs->next->values[0].string.text);
        }
        else
        {
         /*
          * Otherwise, use the language code corresponding to the locale...
          */

	  language = cupsLangDefault();
	  (*attr)->values[0].string.language = _cupsStrAlloc(ipp_lang_code(language->language,
									code,
									sizeof(code)));
        }

        for (i = (*attr)->num_values - 1, value = (*attr)->values + 1;
             i > 0;
             i --, value ++)
          value->string.language = (*attr)->values[0].string.language;

        if ((int)(*attr)->value_tag & IPP_TAG_CUPS_CONST)
        {
         /*
          * Make copies of all values...
          */

	  for (i = (*attr)->num_values, value = (*attr)->values;
	       i > 0;
	       i --, value ++)
	    value->string.text = _cupsStrAlloc(value->string.text);
        }

        (*attr)->value_tag = IPP_TAG_NAMELANG;
        break;

    case IPP_TAG_KEYWORD :
        if (temp_tag == IPP_TAG_NAME || temp_tag == IPP_TAG_NAMELANG)
          break;			/* Silently "allow" name -> keyword */

    default :
        return (0);
  }

  return (1);
}


/*
 * 'ippSetVersion()' - Set the version number in an IPP message.
 *
 * The @code ipp@ parameter refers to an IPP message previously created using
 * the @link ippNew@, @link ippNewRequest@, or  @link ippNewResponse@ functions.
 *
 * The valid version numbers are currently 1.0, 1.1, 2.0, 2.1, and 2.2.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

int					/* O - 1 on success, 0 on failure */
ippSetVersion(ipp_t *ipp,		/* I - IPP message */
              int   major,		/* I - Major version number (major.minor) */
              int   minor)		/* I - Minor version number (major.minor) */
{
 /*
  * Range check input...
  */

  if (!ipp || major < 0 || minor < 0)
    return (0);

 /*
  * Set the version number...
  */

  ipp->request.any.version[0] = (ipp_uchar_t)major;
  ipp->request.any.version[1] = (ipp_uchar_t)minor;

  return (1);
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

  date[0]  = (ipp_uchar_t)(unixdate->tm_year >> 8);
  date[1]  = (ipp_uchar_t)(unixdate->tm_year);
  date[2]  = (ipp_uchar_t)(unixdate->tm_mon + 1);
  date[3]  = (ipp_uchar_t)unixdate->tm_mday;
  date[4]  = (ipp_uchar_t)unixdate->tm_hour;
  date[5]  = (ipp_uchar_t)unixdate->tm_min;
  date[6]  = (ipp_uchar_t)unixdate->tm_sec;
  date[7]  = 0;
  date[8]  = '+';
  date[9]  = 0;
  date[10] = 0;

  return (date);
}


/*
 * 'ippValidateAttribute()' - Validate the contents of an attribute.
 *
 * This function validates the contents of an attribute based on the name and
 * value tag.  1 is returned if the attribute is valid, 0 otherwise.  On
 * failure, cupsLastErrorString() is set to a human-readable message.
 *
 * @since CUPS 1.7/macOS 10.9@
 */

int					/* O - 1 if valid, 0 otherwise */
ippValidateAttribute(
    ipp_attribute_t *attr)		/* I - Attribute */
{
  int		i;			/* Looping var */
  char		scheme[64],		/* Scheme from URI */
		userpass[256],		/* Username/password from URI */
		hostname[256],		/* Hostname from URI */
		resource[1024];		/* Resource from URI */
  int		port,			/* Port number from URI */
		uri_status;		/* URI separation status */
  const char	*ptr;			/* Pointer into string */
  ipp_attribute_t *colattr;		/* Collection attribute */
  regex_t	re;			/* Regular expression */
  ipp_uchar_t	*date;			/* Current date value */
  static const char * const uri_status_strings[] =
  {					/* URI status strings */
    "URI too large",
    "Bad arguments to function",
    "Bad resource in URI",
    "Bad port number in URI",
    "Bad hostname/address in URI",
    "Bad username in URI",
    "Bad scheme in URI",
    "Bad/empty URI",
    "OK",
    "Missing scheme in URI",
    "Unknown scheme in URI",
    "Missing resource in URI"
  };


 /*
  * Skip separators.
  */

  if (!attr->name)
    return (1);

 /*
  * Validate the attribute name.
  */

  for (ptr = attr->name; *ptr; ptr ++)
    if (!isalnum(*ptr & 255) && *ptr != '-' && *ptr != '.' && *ptr != '_')
      break;

  if (*ptr || ptr == attr->name)
  {
    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
                  _("\"%s\": Bad attribute name - invalid character "
		    "(RFC 2911 section 4.1.3)."), attr->name);
    return (0);
  }

  if ((ptr - attr->name) > 255)
  {
    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
                  _("\"%s\": Bad attribute name - bad length %d "
		    "(RFC 2911 section 4.1.3)."), attr->name,
		  (int)(ptr - attr->name));
    return (0);
  }

  switch (attr->value_tag)
  {
    case IPP_TAG_INTEGER :
        break;

    case IPP_TAG_BOOLEAN :
        for (i = 0; i < attr->num_values; i ++)
	{
	  if (attr->values[i].boolean != 0 &&
	      attr->values[i].boolean != 1)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
                          _("\"%s\": Bad boolen value %d "
			    "(RFC 2911 section 4.1.11)."), attr->name,
			  attr->values[i].boolean);
	    return (0);
	  }
	}
        break;

    case IPP_TAG_ENUM :
        for (i = 0; i < attr->num_values; i ++)
	{
	  if (attr->values[i].integer < 1)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad enum value %d - out of range "
			    "(RFC 2911 section 4.1.4)."), attr->name,
			    attr->values[i].integer);
            return (0);
	  }
	}
        break;

    case IPP_TAG_STRING :
        for (i = 0; i < attr->num_values; i ++)
	{
	  if (attr->values[i].unknown.length > IPP_MAX_OCTETSTRING)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad octetString value - bad length %d "
			    "(RFC 2911 section 4.1.10)."), attr->name,
			    attr->values[i].unknown.length);
	    return (0);
	  }
	}
        break;

    case IPP_TAG_DATE :
        for (i = 0; i < attr->num_values; i ++)
	{
	  date = attr->values[i].date;

          if (date[2] < 1 || date[2] > 12)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad dateTime month %u "
			    "(RFC 2911 section 4.1.14)."), attr->name, date[2]);
	    return (0);
	  }

          if (date[3] < 1 || date[3] > 31)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad dateTime day %u "
			    "(RFC 2911 section 4.1.14)."), attr->name, date[3]);
	    return (0);
	  }

          if (date[4] > 23)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad dateTime hours %u "
			    "(RFC 2911 section 4.1.14)."), attr->name, date[4]);
	    return (0);
	  }

          if (date[5] > 59)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad dateTime minutes %u "
			    "(RFC 2911 section 4.1.14)."), attr->name, date[5]);
	    return (0);
	  }

          if (date[6] > 60)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad dateTime seconds %u "
			    "(RFC 2911 section 4.1.14)."), attr->name, date[6]);
	    return (0);
	  }

          if (date[7] > 9)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad dateTime deciseconds %u "
			    "(RFC 2911 section 4.1.14)."), attr->name, date[7]);
	    return (0);
	  }

          if (date[8] != '-' && date[8] != '+')
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad dateTime UTC sign '%c' "
			    "(RFC 2911 section 4.1.14)."), attr->name, date[8]);
	    return (0);
	  }

          if (date[9] > 11)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad dateTime UTC hours %u "
			    "(RFC 2911 section 4.1.14)."), attr->name, date[9]);
	    return (0);
	  }

          if (date[10] > 59)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad dateTime UTC minutes %u "
			    "(RFC 2911 section 4.1.14)."), attr->name, date[10]);
	    return (0);
	  }
	}
        break;

    case IPP_TAG_RESOLUTION :
        for (i = 0; i < attr->num_values; i ++)
	{
	  if (attr->values[i].resolution.xres <= 0)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad resolution value %dx%d%s - cross "
			    "feed resolution must be positive "
			    "(RFC 2911 section 4.1.15)."), attr->name,
			  attr->values[i].resolution.xres,
			  attr->values[i].resolution.yres,
			  attr->values[i].resolution.units ==
			      IPP_RES_PER_INCH ? "dpi" :
			      attr->values[i].resolution.units ==
				  IPP_RES_PER_CM ? "dpcm" : "unknown");
	    return (0);
	  }

	  if (attr->values[i].resolution.yres <= 0)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad resolution value %dx%d%s - feed "
			    "resolution must be positive "
			    "(RFC 2911 section 4.1.15)."), attr->name,
			  attr->values[i].resolution.xres,
			  attr->values[i].resolution.yres,
			  attr->values[i].resolution.units ==
			      IPP_RES_PER_INCH ? "dpi" :
			      attr->values[i].resolution.units ==
				  IPP_RES_PER_CM ? "dpcm" : "unknown");
            return (0);
	  }

	  if (attr->values[i].resolution.units != IPP_RES_PER_INCH &&
	      attr->values[i].resolution.units != IPP_RES_PER_CM)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad resolution value %dx%d%s - bad "
			    "units value (RFC 2911 section 4.1.15)."),
			  attr->name, attr->values[i].resolution.xres,
			  attr->values[i].resolution.yres,
			  attr->values[i].resolution.units ==
			      IPP_RES_PER_INCH ? "dpi" :
			      attr->values[i].resolution.units ==
				  IPP_RES_PER_CM ? "dpcm" : "unknown");
	    return (0);
	  }
	}
        break;

    case IPP_TAG_RANGE :
        for (i = 0; i < attr->num_values; i ++)
	{
	  if (attr->values[i].range.lower > attr->values[i].range.upper)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad rangeOfInteger value %d-%d - lower "
			    "greater than upper (RFC 2911 section 4.1.13)."),
			  attr->name, attr->values[i].range.lower,
			  attr->values[i].range.upper);
	    return (0);
	  }
	}
        break;

    case IPP_TAG_BEGIN_COLLECTION :
        for (i = 0; i < attr->num_values; i ++)
	{
	  for (colattr = attr->values[i].collection->attrs;
	       colattr;
	       colattr = colattr->next)
	  {
	    if (!ippValidateAttribute(colattr))
	      return (0);
	  }
	}
        break;

    case IPP_TAG_TEXT :
    case IPP_TAG_TEXTLANG :
        for (i = 0; i < attr->num_values; i ++)
	{
	  for (ptr = attr->values[i].string.text; *ptr; ptr ++)
	  {
	    if ((*ptr & 0xe0) == 0xc0)
	    {
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	    }
	    else if ((*ptr & 0xf0) == 0xe0)
	    {
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	    }
	    else if ((*ptr & 0xf8) == 0xf0)
	    {
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	    }
	    else if (*ptr & 0x80)
	      break;
	  }

	  if (*ptr)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad text value \"%s\" - bad UTF-8 "
			    "sequence (RFC 2911 section 4.1.1)."), attr->name,
			  attr->values[i].string.text);
	    return (0);
	  }

	  if ((ptr - attr->values[i].string.text) > (IPP_MAX_TEXT - 1))
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad text value \"%s\" - bad length %d "
			    "(RFC 2911 section 4.1.1)."), attr->name,
			  attr->values[i].string.text,
			  (int)(ptr - attr->values[i].string.text));
	    return (0);
	  }
	}
        break;

    case IPP_TAG_NAME :
    case IPP_TAG_NAMELANG :
        for (i = 0; i < attr->num_values; i ++)
	{
	  for (ptr = attr->values[i].string.text; *ptr; ptr ++)
	  {
	    if ((*ptr & 0xe0) == 0xc0)
	    {
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	    }
	    else if ((*ptr & 0xf0) == 0xe0)
	    {
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	    }
	    else if ((*ptr & 0xf8) == 0xf0)
	    {
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	    }
	    else if (*ptr & 0x80)
	      break;
	  }

	  if (*ptr)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad name value \"%s\" - bad UTF-8 "
			    "sequence (RFC 2911 section 4.1.2)."), attr->name,
			  attr->values[i].string.text);
	    return (0);
	  }

	  if ((ptr - attr->values[i].string.text) > (IPP_MAX_NAME - 1))
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad name value \"%s\" - bad length %d "
			    "(RFC 2911 section 4.1.2)."), attr->name,
			  attr->values[i].string.text,
			  (int)(ptr - attr->values[i].string.text));
	    return (0);
	  }
	}
        break;

    case IPP_TAG_KEYWORD :
        for (i = 0; i < attr->num_values; i ++)
	{
	  for (ptr = attr->values[i].string.text; *ptr; ptr ++)
	    if (!isalnum(*ptr & 255) && *ptr != '-' && *ptr != '.' &&
	        *ptr != '_')
	      break;

	  if (*ptr || ptr == attr->values[i].string.text)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad keyword value \"%s\" - invalid "
			    "character (RFC 2911 section 4.1.3)."),
			  attr->name, attr->values[i].string.text);
	    return (0);
	  }

	  if ((ptr - attr->values[i].string.text) > (IPP_MAX_KEYWORD - 1))
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad keyword value \"%s\" - bad "
			    "length %d (RFC 2911 section 4.1.3)."),
			  attr->name, attr->values[i].string.text,
			  (int)(ptr - attr->values[i].string.text));
	    return (0);
	  }
	}
        break;

    case IPP_TAG_URI :
        for (i = 0; i < attr->num_values; i ++)
	{
	  uri_status = httpSeparateURI(HTTP_URI_CODING_ALL,
	                               attr->values[i].string.text,
				       scheme, sizeof(scheme),
				       userpass, sizeof(userpass),
				       hostname, sizeof(hostname),
				       &port, resource, sizeof(resource));

	  if (uri_status < HTTP_URI_STATUS_OK)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad URI value \"%s\" - %s "
			    "(RFC 2911 section 4.1.5)."), attr->name,
			  attr->values[i].string.text,
			  uri_status_strings[uri_status -
					     HTTP_URI_STATUS_OVERFLOW]);
	    return (0);
	  }

	  if (strlen(attr->values[i].string.text) > (IPP_MAX_URI - 1))
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad URI value \"%s\" - bad length %d "
			    "(RFC 2911 section 4.1.5)."), attr->name,
			  attr->values[i].string.text,
			  (int)strlen(attr->values[i].string.text));
	  }
	}
        break;

    case IPP_TAG_URISCHEME :
        for (i = 0; i < attr->num_values; i ++)
	{
	  ptr = attr->values[i].string.text;
	  if (islower(*ptr & 255))
	  {
	    for (ptr ++; *ptr; ptr ++)
	      if (!islower(*ptr & 255) && !isdigit(*ptr & 255) &&
	          *ptr != '+' && *ptr != '-' && *ptr != '.')
                break;
	  }

	  if (*ptr || ptr == attr->values[i].string.text)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad uriScheme value \"%s\" - bad "
			    "characters (RFC 2911 section 4.1.6)."),
			  attr->name, attr->values[i].string.text);
	    return (0);
	  }

	  if ((ptr - attr->values[i].string.text) > (IPP_MAX_URISCHEME - 1))
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad uriScheme value \"%s\" - bad "
			    "length %d (RFC 2911 section 4.1.6)."),
			  attr->name, attr->values[i].string.text,
			  (int)(ptr - attr->values[i].string.text));
	    return (0);
	  }
	}
        break;

    case IPP_TAG_CHARSET :
        for (i = 0; i < attr->num_values; i ++)
	{
	  for (ptr = attr->values[i].string.text; *ptr; ptr ++)
	    if (!isprint(*ptr & 255) || isupper(*ptr & 255) ||
	        isspace(*ptr & 255))
	      break;

	  if (*ptr || ptr == attr->values[i].string.text)
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad charset value \"%s\" - bad "
			    "characters (RFC 2911 section 4.1.7)."),
			  attr->name, attr->values[i].string.text);
	    return (0);
	  }

	  if ((ptr - attr->values[i].string.text) > (IPP_MAX_CHARSET - 1))
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad charset value \"%s\" - bad "
			    "length %d (RFC 2911 section 4.1.7)."),
			  attr->name, attr->values[i].string.text,
			  (int)(ptr - attr->values[i].string.text));
	    return (0);
	  }
	}
        break;

    case IPP_TAG_LANGUAGE :
       /*
        * The following regular expression is derived from the ABNF for
	* language tags in RFC 4646.  All I can say is that this is the
	* easiest way to check the values...
	*/

        if ((i = regcomp(&re,
			 "^("
			 "(([a-z]{2,3}(-[a-z][a-z][a-z]){0,3})|[a-z]{4,8})"
								/* language */
			 "(-[a-z][a-z][a-z][a-z]){0,1}"		/* script */
			 "(-([a-z][a-z]|[0-9][0-9][0-9])){0,1}"	/* region */
			 "(-([a-z]{5,8}|[0-9][0-9][0-9]))*"	/* variant */
			 "(-[a-wy-z](-[a-z0-9]{2,8})+)*"	/* extension */
			 "(-x(-[a-z0-9]{1,8})+)*"		/* privateuse */
			 "|"
			 "x(-[a-z0-9]{1,8})+"			/* privateuse */
			 "|"
			 "[a-z]{1,3}(-[a-z][0-9]{2,8}){1,2}"	/* grandfathered */
			 ")$",
			 REG_NOSUB | REG_EXTENDED)) != 0)
        {
          char	temp[256];		/* Temporary error string */

          regerror(i, &re, temp, sizeof(temp));
	  ipp_set_error(IPP_STATUS_ERROR_INTERNAL,
			_("Unable to compile naturalLanguage regular "
			  "expression: %s."), temp);
	  return (0);
        }

        for (i = 0; i < attr->num_values; i ++)
	{
	  if (regexec(&re, attr->values[i].string.text, 0, NULL, 0))
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad naturalLanguage value \"%s\" - bad "
			    "characters (RFC 2911 section 4.1.8)."),
			  attr->name, attr->values[i].string.text);
	    regfree(&re);
	    return (0);
	  }

	  if (strlen(attr->values[i].string.text) > (IPP_MAX_LANGUAGE - 1))
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad naturalLanguage value \"%s\" - bad "
			    "length %d (RFC 2911 section 4.1.8)."),
			  attr->name, attr->values[i].string.text,
			  (int)strlen(attr->values[i].string.text));
	    regfree(&re);
	    return (0);
	  }
	}

	regfree(&re);
        break;

    case IPP_TAG_MIMETYPE :
       /*
        * The following regular expression is derived from the ABNF for
	* MIME media types in RFC 2045 and 4288.  All I can say is that this is
	* the easiest way to check the values...
	*/

        if ((i = regcomp(&re,
			 "^"
			 "[-a-zA-Z0-9!#$&.+^_]{1,127}"		/* type-name */
			 "/"
			 "[-a-zA-Z0-9!#$&.+^_]{1,127}"		/* subtype-name */
			 "(;[-a-zA-Z0-9!#$&.+^_]{1,127}="	/* parameter= */
			 "([-a-zA-Z0-9!#$&.+^_]{1,127}|\"[^\"]*\"))*"
			 					/* value */
			 "$",
			 REG_NOSUB | REG_EXTENDED)) != 0)
        {
          char	temp[256];		/* Temporary error string */

          regerror(i, &re, temp, sizeof(temp));
	  ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			_("Unable to compile mimeMediaType regular "
			  "expression: %s."), temp);
	  return (0);
        }

        for (i = 0; i < attr->num_values; i ++)
	{
	  if (regexec(&re, attr->values[i].string.text, 0, NULL, 0))
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad mimeMediaType value \"%s\" - bad "
			    "characters (RFC 2911 section 4.1.9)."),
			  attr->name, attr->values[i].string.text);
	    regfree(&re);
	    return (0);
	  }

	  if (strlen(attr->values[i].string.text) > (IPP_MAX_MIMETYPE - 1))
	  {
	    ipp_set_error(IPP_STATUS_ERROR_BAD_REQUEST,
			  _("\"%s\": Bad mimeMediaType value \"%s\" - bad "
			    "length %d (RFC 2911 section 4.1.9)."),
			  attr->name, attr->values[i].string.text,
			  (int)strlen(attr->values[i].string.text));
	    regfree(&re);
	    return (0);
	  }
	}

	regfree(&re);
        break;

    default :
        break;
  }

  return (1);
}


/*
 * 'ippValidateAttributes()' - Validate all attributes in an IPP message.
 *
 * This function validates the contents of the IPP message, including each
 * attribute.  Like @link ippValidateAttribute@, cupsLastErrorString() is set
 * to a human-readable message on failure.
 *
 * @since CUPS 1.7/macOS 10.9@
 */

int					/* O - 1 if valid, 0 otherwise */
ippValidateAttributes(ipp_t *ipp)	/* I - IPP message */
{
  ipp_attribute_t	*attr;		/* Current attribute */


  if (!ipp)
    return (1);

  for (attr = ipp->attrs; attr; attr = attr->next)
    if (!ippValidateAttribute(attr))
      return (0);

  return (1);
}


/*
 * 'ippWrite()' - Write data for an IPP message to a HTTP connection.
 */

ipp_state_t				/* O - Current state */
ippWrite(http_t *http,			/* I - HTTP connection */
         ipp_t  *ipp)			/* I - IPP data */
{
  DEBUG_printf(("ippWrite(http=%p, ipp=%p)", (void *)http, (void *)ipp));

  if (!http)
    return (IPP_STATE_ERROR);

  return (ippWriteIO(http, (ipp_iocb_t)httpWrite2, http->blocking, NULL, ipp));
}


/*
 * 'ippWriteFile()' - Write data for an IPP message to a file.
 *
 * @since CUPS 1.1.19/macOS 10.3@
 */

ipp_state_t				/* O - Current state */
ippWriteFile(int   fd,			/* I - HTTP data */
             ipp_t *ipp)		/* I - IPP data */
{
  DEBUG_printf(("ippWriteFile(fd=%d, ipp=%p)", fd, (void *)ipp));

  ipp->state = IPP_STATE_IDLE;

  return (ippWriteIO(&fd, (ipp_iocb_t)ipp_write_file, 1, NULL, ipp));
}


/*
 * 'ippWriteIO()' - Write data for an IPP message.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

ipp_state_t				/* O - Current state */
ippWriteIO(void       *dst,		/* I - Destination */
           ipp_iocb_t cb,		/* I - Write callback function */
	   int        blocking,		/* I - Use blocking IO? */
	   ipp_t      *parent,		/* I - Parent IPP message */
           ipp_t      *ipp)		/* I - IPP data */
{
  int			i;		/* Looping var */
  int			n;		/* Length of data */
  unsigned char		*buffer,	/* Data buffer */
			*bufptr;	/* Pointer into buffer */
  ipp_attribute_t	*attr;		/* Current attribute */
  _ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("ippWriteIO(dst=%p, cb=%p, blocking=%d, parent=%p, ipp=%p)", (void *)dst, (void *)cb, blocking, (void *)parent, (void *)ipp));

  if (!dst || !ipp)
    return (IPP_STATE_ERROR);

  if ((buffer = (unsigned char *)_cupsBufferGet(IPP_BUF_SIZE)) == NULL)
  {
    DEBUG_puts("1ippWriteIO: Unable to get write buffer");
    return (IPP_STATE_ERROR);
  }

  switch (ipp->state)
  {
    case IPP_STATE_IDLE :
        ipp->state ++; /* Avoid common problem... */

    case IPP_STATE_HEADER :
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
	  *bufptr++ = (ipp_uchar_t)(ipp->request.any.op_status >> 8);
	  *bufptr++ = (ipp_uchar_t)ipp->request.any.op_status;
	  *bufptr++ = (ipp_uchar_t)(ipp->request.any.request_id >> 24);
	  *bufptr++ = (ipp_uchar_t)(ipp->request.any.request_id >> 16);
	  *bufptr++ = (ipp_uchar_t)(ipp->request.any.request_id >> 8);
	  *bufptr++ = (ipp_uchar_t)ipp->request.any.request_id;

	  DEBUG_printf(("2ippWriteIO: version=%d.%d", buffer[0], buffer[1]));
	  DEBUG_printf(("2ippWriteIO: op_status=%04x",
			ipp->request.any.op_status));
	  DEBUG_printf(("2ippWriteIO: request_id=%d",
			ipp->request.any.request_id));

          if ((*cb)(dst, buffer, (size_t)(bufptr - buffer)) < 0)
	  {
	    DEBUG_puts("1ippWriteIO: Could not write IPP header...");
	    _cupsBufferRelease((char *)buffer);
	    return (IPP_STATE_ERROR);
	  }
	}

       /*
	* Reset the state engine to point to the first attribute
	* in the request/response, with no current group.
	*/

        ipp->state   = IPP_STATE_ATTRIBUTE;
	ipp->current = ipp->attrs;
	ipp->curtag  = IPP_TAG_ZERO;

	DEBUG_printf(("1ippWriteIO: ipp->current=%p", (void *)ipp->current));

       /*
        * If blocking is disabled, stop here...
	*/

        if (!blocking)
	  break;

    case IPP_STATE_ATTRIBUTE :
        while (ipp->current != NULL)
	{
	 /*
	  * Write this attribute...
	  */

	  bufptr = buffer;
	  attr   = ipp->current;

	  ipp->current = ipp->current->next;

          if (!parent)
	  {
	    if (ipp->curtag != attr->group_tag)
	    {
	     /*
	      * Send a group tag byte...
	      */

	      ipp->curtag = attr->group_tag;

	      if (attr->group_tag == IPP_TAG_ZERO)
		continue;

	      DEBUG_printf(("2ippWriteIO: wrote group tag=%x(%s)",
			    attr->group_tag, ippTagString(attr->group_tag)));
	      *bufptr++ = (ipp_uchar_t)attr->group_tag;
	    }
	    else if (attr->group_tag == IPP_TAG_ZERO)
	      continue;
	  }

	  DEBUG_printf(("1ippWriteIO: %s (%s%s)", attr->name,
	                attr->num_values > 1 ? "1setOf " : "",
			ippTagString(attr->value_tag)));

         /*
	  * Write the attribute tag and name.
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

            if ((n = (int)strlen(attr->name)) > (IPP_BUF_SIZE - 8))
	    {
	      DEBUG_printf(("1ippWriteIO: Attribute name too long (%d)", n));
	      _cupsBufferRelease((char *)buffer);
	      return (IPP_STATE_ERROR);
	    }

           /*
	    * Write the value tag, name length, and name string...
	    */

            DEBUG_printf(("2ippWriteIO: writing value tag=%x(%s)",
	                  attr->value_tag, ippTagString(attr->value_tag)));
            DEBUG_printf(("2ippWriteIO: writing name=%d,\"%s\"", n,
	                  attr->name));

            if (attr->value_tag > 0xff)
            {
              *bufptr++ = IPP_TAG_EXTENSION;
	      *bufptr++ = (ipp_uchar_t)(attr->value_tag >> 24);
	      *bufptr++ = (ipp_uchar_t)(attr->value_tag >> 16);
	      *bufptr++ = (ipp_uchar_t)(attr->value_tag >> 8);
	      *bufptr++ = (ipp_uchar_t)attr->value_tag;
            }
            else
	      *bufptr++ = (ipp_uchar_t)attr->value_tag;

	    *bufptr++ = (ipp_uchar_t)(n >> 8);
	    *bufptr++ = (ipp_uchar_t)n;
	    memcpy(bufptr, attr->name, (size_t)n);
	    bufptr += n;
          }
	  else
	  {
           /*
	    * Get the length of the attribute name, and make sure it won't
	    * overflow the buffer...
	    */

            if ((n = (int)strlen(attr->name)) > (IPP_BUF_SIZE - 12))
	    {
	      DEBUG_printf(("1ippWriteIO: Attribute name too long (%d)", n));
	      _cupsBufferRelease((char *)buffer);
	      return (IPP_STATE_ERROR);
	    }

           /*
	    * Write the member name tag, name length, name string, value tag,
	    * and empty name for the collection member attribute...
	    */

            DEBUG_printf(("2ippWriteIO: writing value tag=%x(memberName)",
	                  IPP_TAG_MEMBERNAME));
            DEBUG_printf(("2ippWriteIO: writing name=%d,\"%s\"", n,
	                  attr->name));
            DEBUG_printf(("2ippWriteIO: writing value tag=%x(%s)",
	                  attr->value_tag, ippTagString(attr->value_tag)));
            DEBUG_puts("2ippWriteIO: writing name=0,\"\"");

            *bufptr++ = IPP_TAG_MEMBERNAME;
	    *bufptr++ = 0;
	    *bufptr++ = 0;
	    *bufptr++ = (ipp_uchar_t)(n >> 8);
	    *bufptr++ = (ipp_uchar_t)n;
	    memcpy(bufptr, attr->name, (size_t)n);
	    bufptr += n;

            if (attr->value_tag > 0xff)
            {
              *bufptr++ = IPP_TAG_EXTENSION;
	      *bufptr++ = (ipp_uchar_t)(attr->value_tag >> 24);
	      *bufptr++ = (ipp_uchar_t)(attr->value_tag >> 16);
	      *bufptr++ = (ipp_uchar_t)(attr->value_tag >> 8);
	      *bufptr++ = (ipp_uchar_t)attr->value_tag;
            }
            else
	      *bufptr++ = (ipp_uchar_t)attr->value_tag;

            *bufptr++ = 0;
            *bufptr++ = 0;
	  }

         /*
	  * Now write the attribute value(s)...
	  */

	  switch (attr->value_tag & ~IPP_TAG_CUPS_CONST)
	  {
	    case IPP_TAG_UNSUPPORTED_VALUE :
	    case IPP_TAG_DEFAULT :
	    case IPP_TAG_UNKNOWN :
	    case IPP_TAG_NOVALUE :
	    case IPP_TAG_NOTSETTABLE :
	    case IPP_TAG_DELETEATTR :
	    case IPP_TAG_ADMINDEFINE :
		*bufptr++ = 0;
		*bufptr++ = 0;
	        break;

	    case IPP_TAG_INTEGER :
	    case IPP_TAG_ENUM :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
                  if ((IPP_BUF_SIZE - (bufptr - buffer)) < 9)
		  {
                    if ((*cb)(dst, buffer, (size_t)(bufptr - buffer)) < 0)
	            {
	              DEBUG_puts("1ippWriteIO: Could not write IPP "
		                 "attribute...");
		      _cupsBufferRelease((char *)buffer);
	              return (IPP_STATE_ERROR);
	            }

		    bufptr = buffer;
		  }

		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = (ipp_uchar_t)attr->value_tag;
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
		  *bufptr++ = (ipp_uchar_t)(value->integer >> 24);
		  *bufptr++ = (ipp_uchar_t)(value->integer >> 16);
		  *bufptr++ = (ipp_uchar_t)(value->integer >> 8);
		  *bufptr++ = (ipp_uchar_t)value->integer;
		}
		break;

	    case IPP_TAG_BOOLEAN :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
                  if ((IPP_BUF_SIZE - (bufptr - buffer)) < 6)
		  {
                    if ((*cb)(dst, buffer, (size_t)(bufptr - buffer)) < 0)
	            {
	              DEBUG_puts("1ippWriteIO: Could not write IPP "
		                 "attribute...");
		      _cupsBufferRelease((char *)buffer);
	              return (IPP_STATE_ERROR);
	            }

		    bufptr = buffer;
		  }

		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = (ipp_uchar_t)attr->value_tag;
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
		  *bufptr++ = (ipp_uchar_t)value->boolean;
		}
		break;

	    case IPP_TAG_TEXT :
	    case IPP_TAG_NAME :
	    case IPP_TAG_KEYWORD :
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

        	    DEBUG_printf(("2ippWriteIO: writing value tag=%x(%s)",
		                  attr->value_tag,
				  ippTagString(attr->value_tag)));
        	    DEBUG_printf(("2ippWriteIO: writing name=0,\"\""));

                    if ((IPP_BUF_SIZE - (bufptr - buffer)) < 3)
		    {
                      if ((*cb)(dst, buffer, (size_t)(bufptr - buffer)) < 0)
	              {
	        	DEBUG_puts("1ippWriteIO: Could not write IPP "
			           "attribute...");
			_cupsBufferRelease((char *)buffer);
	        	return (IPP_STATE_ERROR);
	              }

		      bufptr = buffer;
		    }

                    *bufptr++ = (ipp_uchar_t)attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

                  if (value->string.text != NULL)
                    n = (int)strlen(value->string.text);
		  else
		    n = 0;

                  if (n > (IPP_BUF_SIZE - 2))
		  {
		    DEBUG_printf(("1ippWriteIO: String too long (%d)", n));
		    _cupsBufferRelease((char *)buffer);
		    return (IPP_STATE_ERROR);
		  }

                  DEBUG_printf(("2ippWriteIO: writing string=%d,\"%s\"", n,
		                value->string.text));

                  if ((int)(IPP_BUF_SIZE - (bufptr - buffer)) < (n + 2))
		  {
                    if ((*cb)(dst, buffer, (size_t)(bufptr - buffer)) < 0)
	            {
	              DEBUG_puts("1ippWriteIO: Could not write IPP "
		                 "attribute...");
		      _cupsBufferRelease((char *)buffer);
	              return (IPP_STATE_ERROR);
	            }

		    bufptr = buffer;
		  }

		 /*
		  * All simple strings consist of the 2-byte length and
		  * character data without the trailing nul normally found
		  * in C strings.  Also, strings cannot be longer than IPP_MAX_LENGTH
		  * bytes since the 2-byte length is a signed (twos-complement)
		  * value.
		  *
		  * Put the 2-byte length and string characters in the buffer.
		  */

	          *bufptr++ = (ipp_uchar_t)(n >> 8);
		  *bufptr++ = (ipp_uchar_t)n;

		  if (n > 0)
		  {
		    memcpy(bufptr, value->string.text, (size_t)n);
		    bufptr += n;
		  }
		}
		break;

	    case IPP_TAG_DATE :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
                  if ((IPP_BUF_SIZE - (bufptr - buffer)) < 16)
		  {
                    if ((*cb)(dst, buffer, (size_t)(bufptr - buffer)) < 0)
	            {
	              DEBUG_puts("1ippWriteIO: Could not write IPP "
		                 "attribute...");
		      _cupsBufferRelease((char *)buffer);
	              return (IPP_STATE_ERROR);
	            }

		    bufptr = buffer;
		  }

		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = (ipp_uchar_t)attr->value_tag;
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
                  if ((IPP_BUF_SIZE - (bufptr - buffer)) < 14)
		  {
                    if ((*cb)(dst, buffer, (size_t)(bufptr - buffer)) < 0)
	            {
	              DEBUG_puts("1ippWriteIO: Could not write IPP "
		                 "attribute...");
		      _cupsBufferRelease((char *)buffer);
		      return (IPP_STATE_ERROR);
	            }

		    bufptr = buffer;
		  }

		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = (ipp_uchar_t)attr->value_tag;
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
		  *bufptr++ = (ipp_uchar_t)(value->resolution.xres >> 24);
		  *bufptr++ = (ipp_uchar_t)(value->resolution.xres >> 16);
		  *bufptr++ = (ipp_uchar_t)(value->resolution.xres >> 8);
		  *bufptr++ = (ipp_uchar_t)value->resolution.xres;
		  *bufptr++ = (ipp_uchar_t)(value->resolution.yres >> 24);
		  *bufptr++ = (ipp_uchar_t)(value->resolution.yres >> 16);
		  *bufptr++ = (ipp_uchar_t)(value->resolution.yres >> 8);
		  *bufptr++ = (ipp_uchar_t)value->resolution.yres;
		  *bufptr++ = (ipp_uchar_t)value->resolution.units;
		}
		break;

	    case IPP_TAG_RANGE :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
                  if ((IPP_BUF_SIZE - (bufptr - buffer)) < 13)
		  {
                    if ((*cb)(dst, buffer, (size_t)(bufptr - buffer)) < 0)
	            {
	              DEBUG_puts("1ippWriteIO: Could not write IPP "
		                 "attribute...");
		      _cupsBufferRelease((char *)buffer);
	              return (IPP_STATE_ERROR);
	            }

		    bufptr = buffer;
		  }

		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = (ipp_uchar_t)attr->value_tag;
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
		  *bufptr++ = (ipp_uchar_t)(value->range.lower >> 24);
		  *bufptr++ = (ipp_uchar_t)(value->range.lower >> 16);
		  *bufptr++ = (ipp_uchar_t)(value->range.lower >> 8);
		  *bufptr++ = (ipp_uchar_t)value->range.lower;
		  *bufptr++ = (ipp_uchar_t)(value->range.upper >> 24);
		  *bufptr++ = (ipp_uchar_t)(value->range.upper >> 16);
		  *bufptr++ = (ipp_uchar_t)(value->range.upper >> 8);
		  *bufptr++ = (ipp_uchar_t)value->range.upper;
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

                    if ((IPP_BUF_SIZE - (bufptr - buffer)) < 3)
		    {
                      if ((*cb)(dst, buffer, (size_t)(bufptr - buffer)) < 0)
	              {
	        	DEBUG_puts("1ippWriteIO: Could not write IPP "
		                   "attribute...");
			_cupsBufferRelease((char *)buffer);
	        	return (IPP_STATE_ERROR);
	              }

		      bufptr = buffer;
		    }

                    *bufptr++ = (ipp_uchar_t)attr->value_tag;
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

		  if (value->string.language != NULL)
                    n += (int)strlen(value->string.language);

		  if (value->string.text != NULL)
                    n += (int)strlen(value->string.text);

                  if (n > (IPP_BUF_SIZE - 2))
		  {
		    DEBUG_printf(("1ippWriteIO: text/nameWithLanguage value "
		                  "too long (%d)", n));
		    _cupsBufferRelease((char *)buffer);
		    return (IPP_STATE_ERROR);
                  }

                  if ((int)(IPP_BUF_SIZE - (bufptr - buffer)) < (n + 2))
		  {
                    if ((*cb)(dst, buffer, (size_t)(bufptr - buffer)) < 0)
	            {
	              DEBUG_puts("1ippWriteIO: Could not write IPP "
		                 "attribute...");
		      _cupsBufferRelease((char *)buffer);
	              return (IPP_STATE_ERROR);
	            }

		    bufptr = buffer;
		  }

                 /* Length of entire value */
	          *bufptr++ = (ipp_uchar_t)(n >> 8);
		  *bufptr++ = (ipp_uchar_t)n;

                 /* Length of language */
		  if (value->string.language != NULL)
		    n = (int)strlen(value->string.language);
		  else
		    n = 0;

	          *bufptr++ = (ipp_uchar_t)(n >> 8);
		  *bufptr++ = (ipp_uchar_t)n;

                 /* Language */
		  if (n > 0)
		  {
		    memcpy(bufptr, value->string.language, (size_t)n);
		    bufptr += n;
		  }

                 /* Length of text */
                  if (value->string.text != NULL)
		    n = (int)strlen(value->string.text);
		  else
		    n = 0;

	          *bufptr++ = (ipp_uchar_t)(n >> 8);
		  *bufptr++ = (ipp_uchar_t)n;

                 /* Text */
		  if (n > 0)
		  {
		    memcpy(bufptr, value->string.text, (size_t)n);
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

                  if ((IPP_BUF_SIZE - (bufptr - buffer)) < 5)
		  {
                    if ((*cb)(dst, buffer, (size_t)(bufptr - buffer)) < 0)
	            {
	              DEBUG_puts("1ippWriteIO: Could not write IPP "
		                 "attribute...");
		      _cupsBufferRelease((char *)buffer);
	              return (IPP_STATE_ERROR);
	            }

		    bufptr = buffer;
		  }

		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = (ipp_uchar_t)attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

                 /*
		  * Write a data length of 0 and flush the buffer...
		  */

	          *bufptr++ = 0;
		  *bufptr++ = 0;

                  if ((*cb)(dst, buffer, (size_t)(bufptr - buffer)) < 0)
	          {
	            DEBUG_puts("1ippWriteIO: Could not write IPP "
		               "attribute...");
		    _cupsBufferRelease((char *)buffer);
	            return (IPP_STATE_ERROR);
	          }

		  bufptr = buffer;

                 /*
		  * Then write the collection attribute...
		  */

                  value->collection->state = IPP_STATE_IDLE;

		  if (ippWriteIO(dst, cb, 1, ipp,
		                 value->collection) == IPP_STATE_ERROR)
		  {
		    DEBUG_puts("1ippWriteIO: Unable to write collection value");
		    _cupsBufferRelease((char *)buffer);
		    return (IPP_STATE_ERROR);
		  }
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

                    if ((IPP_BUF_SIZE - (bufptr - buffer)) < 3)
		    {
                      if ((*cb)(dst, buffer, (size_t)(bufptr - buffer)) < 0)
	              {
	        	DEBUG_puts("1ippWriteIO: Could not write IPP "
		                   "attribute...");
			_cupsBufferRelease((char *)buffer);
	        	return (IPP_STATE_ERROR);
	              }

		      bufptr = buffer;
		    }

                    *bufptr++ = (ipp_uchar_t)attr->value_tag;
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

                  if (n > (IPP_BUF_SIZE - 2))
		  {
		    DEBUG_printf(("1ippWriteIO: Data length too long (%d)",
		                  n));
		    _cupsBufferRelease((char *)buffer);
		    return (IPP_STATE_ERROR);
		  }

                  if ((int)(IPP_BUF_SIZE - (bufptr - buffer)) < (n + 2))
		  {
                    if ((*cb)(dst, buffer, (size_t)(bufptr - buffer)) < 0)
	            {
	              DEBUG_puts("1ippWriteIO: Could not write IPP "
		                 "attribute...");
		      _cupsBufferRelease((char *)buffer);
	              return (IPP_STATE_ERROR);
	            }

		    bufptr = buffer;
		  }

                 /* Length of unknown value */
	          *bufptr++ = (ipp_uchar_t)(n >> 8);
		  *bufptr++ = (ipp_uchar_t)n;

                 /* Value */
		  if (n > 0)
		  {
		    memcpy(bufptr, value->unknown.data, (size_t)n);
		    bufptr += n;
		  }
		}
		break;
	  }

         /*
	  * Write the data out...
	  */

	  if (bufptr > buffer)
	  {
	    if ((*cb)(dst, buffer, (size_t)(bufptr - buffer)) < 0)
	    {
	      DEBUG_puts("1ippWriteIO: Could not write IPP attribute...");
	      _cupsBufferRelease((char *)buffer);
	      return (IPP_STATE_ERROR);
	    }

	    DEBUG_printf(("2ippWriteIO: wrote %d bytes",
			  (int)(bufptr - buffer)));
	  }

	 /*
          * If blocking is disabled and we aren't at the end of the attribute
          * list, stop here...
	  */

          if (!blocking && ipp->current)
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

	  if ((*cb)(dst, buffer, (size_t)n) < 0)
	  {
	    DEBUG_puts("1ippWriteIO: Could not write IPP end-tag...");
	    _cupsBufferRelease((char *)buffer);
	    return (IPP_STATE_ERROR);
	  }

	  ipp->state = IPP_STATE_DATA;
	}
        break;

    case IPP_STATE_DATA :
        break;

    default :
        break; /* anti-compiler-warning-code */
  }

  _cupsBufferRelease((char *)buffer);

  return (ipp->state);
}


/*
 * 'ipp_add_attr()' - Add a new attribute to the message.
 */

static ipp_attribute_t *		/* O - New attribute */
ipp_add_attr(ipp_t      *ipp,		/* I - IPP message */
             const char *name,		/* I - Attribute name or NULL */
             ipp_tag_t  group_tag,	/* I - Group tag or IPP_TAG_ZERO */
             ipp_tag_t  value_tag,	/* I - Value tag or IPP_TAG_ZERO */
             int        num_values)	/* I - Number of values */
{
  int			alloc_values;	/* Number of values to allocate */
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("4ipp_add_attr(ipp=%p, name=\"%s\", group_tag=0x%x, value_tag=0x%x, num_values=%d)", (void *)ipp, name, group_tag, value_tag, num_values));

 /*
  * Range check input...
  */

  if (!ipp || num_values < 0)
    return (NULL);

 /*
  * Allocate memory, rounding the allocation up as needed...
  */

  if (num_values <= 1)
    alloc_values = 1;
  else
    alloc_values = (num_values + IPP_MAX_VALUES - 1) & ~(IPP_MAX_VALUES - 1);

  attr = calloc(sizeof(ipp_attribute_t) +
                (size_t)(alloc_values - 1) * sizeof(_ipp_value_t), 1);

  if (attr)
  {
   /*
    * Initialize attribute...
    */

    if (name)
      attr->name = _cupsStrAlloc(name);

    attr->group_tag  = group_tag;
    attr->value_tag  = value_tag;
    attr->num_values = num_values;

   /*
    * Add it to the end of the linked list...
    */

    if (ipp->last)
      ipp->last->next = attr;
    else
      ipp->attrs = attr;

    ipp->prev = ipp->last;
    ipp->last = ipp->current = attr;
  }

  DEBUG_printf(("5ipp_add_attr: Returning %p", (void *)attr));

  return (attr);
}


/*
 * 'ipp_free_values()' - Free attribute values.
 */

static void
ipp_free_values(ipp_attribute_t *attr,	/* I - Attribute to free values from */
                int             element,/* I - First value to free */
                int             count)	/* I - Number of values to free */
{
  int		i;			/* Looping var */
  _ipp_value_t	*value;			/* Current value */


  DEBUG_printf(("4ipp_free_values(attr=%p, element=%d, count=%d)", (void *)attr, element, count));

  if (!(attr->value_tag & IPP_TAG_CUPS_CONST))
  {
   /*
    * Free values as needed...
    */

    switch (attr->value_tag)
    {
      case IPP_TAG_TEXTLANG :
      case IPP_TAG_NAMELANG :
	  if (element == 0 && count == attr->num_values &&
	      attr->values[0].string.language)
	  {
	    _cupsStrFree(attr->values[0].string.language);
	    attr->values[0].string.language = NULL;
	  }
	  /* Fall through to other string values */

      case IPP_TAG_TEXT :
      case IPP_TAG_NAME :
      case IPP_TAG_RESERVED_STRING :
      case IPP_TAG_KEYWORD :
      case IPP_TAG_URI :
      case IPP_TAG_URISCHEME :
      case IPP_TAG_CHARSET :
      case IPP_TAG_LANGUAGE :
      case IPP_TAG_MIMETYPE :
	  for (i = count, value = attr->values + element;
	       i > 0;
	       i --, value ++)
	  {
	    _cupsStrFree(value->string.text);
	    value->string.text = NULL;
	  }
	  break;

      case IPP_TAG_DEFAULT :
      case IPP_TAG_UNKNOWN :
      case IPP_TAG_NOVALUE :
      case IPP_TAG_NOTSETTABLE :
      case IPP_TAG_DELETEATTR :
      case IPP_TAG_ADMINDEFINE :
      case IPP_TAG_INTEGER :
      case IPP_TAG_ENUM :
      case IPP_TAG_BOOLEAN :
      case IPP_TAG_DATE :
      case IPP_TAG_RESOLUTION :
      case IPP_TAG_RANGE :
	  break;

      case IPP_TAG_BEGIN_COLLECTION :
	  for (i = count, value = attr->values + element;
	       i > 0;
	       i --, value ++)
	  {
	    ippDelete(value->collection);
	    value->collection = NULL;
	  }
	  break;

      case IPP_TAG_STRING :
      default :
	  for (i = count, value = attr->values + element;
	       i > 0;
	       i --, value ++)
	  {
	    if (value->unknown.data)
	    {
	      free(value->unknown.data);
	      value->unknown.data = NULL;
	    }
	  }
	  break;
    }
  }

 /*
  * If we are not freeing values from the end, move the remaining values up...
  */

  if ((element + count) < attr->num_values)
    memmove(attr->values + element, attr->values + element + count,
            (size_t)(attr->num_values - count - element) * sizeof(_ipp_value_t));

  attr->num_values -= count;
}


/*
 * 'ipp_get_code()' - Convert a C locale/charset name into an IPP language/charset code.
 *
 * This typically converts strings of the form "ll_CC", "ll-REGION", and "CHARSET_NUMBER"
 * to "ll-cc", "ll-region", and "charset-number", respectively.
 */

static char *				/* O - Language code string */
ipp_get_code(const char *value,		/* I - Locale/charset string */
             char       *buffer,	/* I - String buffer */
             size_t     bufsize)	/* I - Size of string buffer */
{
  char	*bufptr,			/* Pointer into buffer */
	*bufend;			/* End of buffer */


 /*
  * Convert values to lowercase and change _ to - as needed...
  */

  for (bufptr = buffer, bufend = buffer + bufsize - 1;
       *value && bufptr < bufend;
       value ++)
    if (*value == '_')
      *bufptr++ = '-';
    else
      *bufptr++ = (char)_cups_tolower(*value);

  *bufptr = '\0';

 /*
  * Return the converted string...
  */

  return (buffer);
}


/*
 * 'ipp_lang_code()' - Convert a C locale name into an IPP language code.
 *
 * This typically converts strings of the form "ll_CC" and "ll-REGION" to "ll-cc" and
 * "ll-region", respectively.  It also converts the "C" (POSIX) locale to "en".
 */

static char *				/* O - Language code string */
ipp_lang_code(const char *locale,	/* I - Locale string */
              char       *buffer,	/* I - String buffer */
              size_t     bufsize)	/* I - Size of string buffer */
{
 /*
  * Map POSIX ("C") locale to generic English, otherwise convert the locale string as-is.
  */

  if (!_cups_strcasecmp(locale, "c"))
  {
    strlcpy(buffer, "en", bufsize);
    return (buffer);
  }
  else
    return (ipp_get_code(locale, buffer, bufsize));
}


/*
 * 'ipp_length()' - Compute the length of an IPP message or collection value.
 */

static size_t				/* O - Size of IPP message */
ipp_length(ipp_t *ipp,			/* I - IPP message or collection */
           int   collection)		/* I - 1 if a collection, 0 otherwise */
{
  int			i;		/* Looping var */
  size_t		bytes;		/* Number of bytes */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_tag_t		group;		/* Current group */
  _ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("3ipp_length(ipp=%p, collection=%d)", (void *)ipp, collection));

  if (!ipp)
  {
    DEBUG_puts("4ipp_length: Returning 0 bytes");
    return (0);
  }

 /*
  * Start with 8 bytes for the IPP message header...
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

    if (!attr->name)
      continue;

    DEBUG_printf(("5ipp_length: attr->name=\"%s\", attr->num_values=%d, "
                  "bytes=" CUPS_LLFMT, attr->name, attr->num_values, CUPS_LLCAST bytes));

    if ((attr->value_tag & ~IPP_TAG_CUPS_CONST) < IPP_TAG_EXTENSION)
      bytes += (size_t)attr->num_values;/* Value tag for each value */
    else
      bytes += (size_t)(5 * attr->num_values);
					/* Value tag for each value */
    bytes += (size_t)(2 * attr->num_values);
					/* Name lengths */
    bytes += strlen(attr->name);	/* Name */
    bytes += (size_t)(2 * attr->num_values);
					/* Value lengths */

    if (collection)
      bytes += 5;			/* Add membername overhead */

    switch (attr->value_tag & ~IPP_TAG_CUPS_CONST)
    {
      case IPP_TAG_UNSUPPORTED_VALUE :
      case IPP_TAG_DEFAULT :
      case IPP_TAG_UNKNOWN :
      case IPP_TAG_NOVALUE :
      case IPP_TAG_NOTSETTABLE :
      case IPP_TAG_DELETEATTR :
      case IPP_TAG_ADMINDEFINE :
          break;

      case IPP_TAG_INTEGER :
      case IPP_TAG_ENUM :
          bytes += (size_t)(4 * attr->num_values);
	  break;

      case IPP_TAG_BOOLEAN :
          bytes += (size_t)attr->num_values;
	  break;

      case IPP_TAG_TEXT :
      case IPP_TAG_NAME :
      case IPP_TAG_KEYWORD :
      case IPP_TAG_URI :
      case IPP_TAG_URISCHEME :
      case IPP_TAG_CHARSET :
      case IPP_TAG_LANGUAGE :
      case IPP_TAG_MIMETYPE :
	  for (i = 0, value = attr->values;
	       i < attr->num_values;
	       i ++, value ++)
	    if (value->string.text)
	      bytes += strlen(value->string.text);
	  break;

      case IPP_TAG_DATE :
          bytes += (size_t)(11 * attr->num_values);
	  break;

      case IPP_TAG_RESOLUTION :
          bytes += (size_t)(9 * attr->num_values);
	  break;

      case IPP_TAG_RANGE :
          bytes += (size_t)(8 * attr->num_values);
	  break;

      case IPP_TAG_TEXTLANG :
      case IPP_TAG_NAMELANG :
          bytes += (size_t)(4 * attr->num_values);
					/* Charset + text length */

	  for (i = 0, value = attr->values;
	       i < attr->num_values;
	       i ++, value ++)
	  {
	    if (value->string.language)
	      bytes += strlen(value->string.language);

	    if (value->string.text)
	      bytes += strlen(value->string.text);
	  }
	  break;

      case IPP_TAG_BEGIN_COLLECTION :
	  for (i = 0, value = attr->values;
	       i < attr->num_values;
	       i ++, value ++)
            bytes += ipp_length(value->collection, 1);
	  break;

      default :
	  for (i = 0, value = attr->values;
	       i < attr->num_values;
	       i ++, value ++)
            bytes += (size_t)value->unknown.length;
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

  DEBUG_printf(("4ipp_length: Returning " CUPS_LLFMT " bytes", CUPS_LLCAST bytes));

  return (bytes);
}


/*
 * 'ipp_read_http()' - Semi-blocking read on a HTTP connection...
 */

static ssize_t				/* O - Number of bytes read */
ipp_read_http(http_t      *http,	/* I - Client connection */
              ipp_uchar_t *buffer,	/* O - Buffer for data */
	      size_t      length)	/* I - Total length */
{
  ssize_t	tbytes,			/* Total bytes read */
		bytes;			/* Bytes read this pass */


  DEBUG_printf(("7ipp_read_http(http=%p, buffer=%p, length=%d)", (void *)http, (void *)buffer, (int)length));

 /*
  * Loop until all bytes are read...
  */

  for (tbytes = 0, bytes = 0;
       tbytes < (int)length;
       tbytes += bytes, buffer += bytes)
  {
    DEBUG_printf(("9ipp_read_http: tbytes=" CUPS_LLFMT ", http->state=%d", CUPS_LLCAST tbytes, http->state));

    if (http->state == HTTP_STATE_WAITING)
      break;

    if (http->used == 0 && !http->blocking)
    {
     /*
      * Wait up to 10 seconds for more data on non-blocking sockets...
      */

      if (!httpWait(http, 10000))
      {
       /*
	* Signal no data...
	*/

	bytes = -1;
	break;
      }
    }
    else if (http->used == 0 && http->timeout_value > 0)
    {
     /*
      * Wait up to timeout seconds for more data on blocking sockets...
      */

      if (!httpWait(http, (int)(1000 * http->timeout_value)))
      {
       /*
	* Signal no data...
	*/

	bytes = -1;
	break;
      }
    }

    if ((bytes = httpRead2(http, (char *)buffer, length - (size_t)tbytes)) < 0)
    {
#ifdef WIN32
      break;
#else
      if (errno != EAGAIN && errno != EINTR)
	break;

      bytes = 0;
#endif /* WIN32 */
    }
    else if (bytes == 0)
      break;
  }

 /*
  * Return the number of bytes read...
  */

  if (tbytes == 0 && bytes < 0)
    tbytes = -1;

  DEBUG_printf(("8ipp_read_http: Returning " CUPS_LLFMT " bytes", CUPS_LLCAST tbytes));

  return (tbytes);
}


/*
 * 'ipp_read_file()' - Read IPP data from a file.
 */

static ssize_t				/* O - Number of bytes read */
ipp_read_file(int         *fd,		/* I - File descriptor */
              ipp_uchar_t *buffer,	/* O - Read buffer */
	      size_t      length)	/* I - Number of bytes to read */
{
#ifdef WIN32
  return ((ssize_t)read(*fd, buffer, (unsigned)length));
#else
  return (read(*fd, buffer, length));
#endif /* WIN32 */
}


/*
 * 'ipp_set_error()' - Set a formatted, localized error string.
 */

static void
ipp_set_error(ipp_status_t status,	/* I - Status code */
              const char   *format,	/* I - Printf-style error string */
	      ...)			/* I - Additional arguments as needed */
{
  va_list	ap;			/* Pointer to additional args */
  char		buffer[2048];		/* Message buffer */
  cups_lang_t	*lang = cupsLangDefault();
					/* Current language */


  va_start(ap, format);
  vsnprintf(buffer, sizeof(buffer), _cupsLangString(lang, format), ap);
  va_end(ap);

  _cupsSetError(status, buffer, 0);
}


/*
 * 'ipp_set_value()' - Get the value element from an attribute, expanding it as
 *                     needed.
 */

static _ipp_value_t *			/* O  - IPP value element or NULL on error */
ipp_set_value(ipp_t           *ipp,	/* IO - IPP message */
              ipp_attribute_t **attr,	/* IO - IPP attribute */
              int             element)	/* I  - Value number (0-based) */
{
  ipp_attribute_t	*temp,		/* New attribute pointer */
			*current,	/* Current attribute in list */
			*prev;		/* Previous attribute in list */
  int			alloc_values;	/* Allocated values */


 /*
  * If we are setting an existing value element, return it...
  */

  temp = *attr;

  if (temp->num_values <= 1)
    alloc_values = 1;
  else
    alloc_values = (temp->num_values + IPP_MAX_VALUES - 1) &
                   ~(IPP_MAX_VALUES - 1);

  if (element < alloc_values)
  {
    if (element >= temp->num_values)
      temp->num_values = element + 1;

    return (temp->values + element);
  }

 /*
  * Otherwise re-allocate the attribute - we allocate in groups of IPP_MAX_VALUE
  * values when num_values > 1.
  */

  if (alloc_values < IPP_MAX_VALUES)
    alloc_values = IPP_MAX_VALUES;
  else
    alloc_values += IPP_MAX_VALUES;

  DEBUG_printf(("4ipp_set_value: Reallocating for up to %d values.",
                alloc_values));

 /*
  * Reallocate memory...
  */

  if ((temp = realloc(temp, sizeof(ipp_attribute_t) + (size_t)(alloc_values - 1) * sizeof(_ipp_value_t))) == NULL)
  {
    _cupsSetHTTPError(HTTP_STATUS_ERROR);
    DEBUG_puts("4ipp_set_value: Unable to resize attribute.");
    return (NULL);
  }

 /*
  * Zero the new memory...
  */

  memset(temp->values + temp->num_values, 0, (size_t)(alloc_values - temp->num_values) * sizeof(_ipp_value_t));

  if (temp != *attr)
  {
   /*
    * Reset pointers in the list...
    */

    if (ipp->current == *attr && ipp->prev)
    {
     /*
      * Use current "previous" pointer...
      */

      prev = ipp->prev;
    }
    else
    {
     /*
      * Find this attribute in the linked list...
      */

      for (prev = NULL, current = ipp->attrs;
	   current && current != *attr;
	   prev = current, current = current->next);

      if (!current)
      {
       /*
	* This is a serious error!
	*/

	*attr = temp;
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL,
	              _("IPP attribute is not a member of the message."), 1);
	DEBUG_puts("4ipp_set_value: Unable to find attribute in message.");
	return (NULL);
      }
    }

    if (prev)
      prev->next = temp;
    else
      ipp->attrs = temp;

    ipp->current = temp;
    ipp->prev    = prev;

    if (ipp->last == *attr)
      ipp->last = temp;

    *attr = temp;
  }

 /*
  * Return the value element...
  */

  if (element >= temp->num_values)
    temp->num_values = element + 1;

  return (temp->values + element);
}


/*
 * 'ipp_write_file()' - Write IPP data to a file.
 */

static ssize_t				/* O - Number of bytes written */
ipp_write_file(int         *fd,		/* I - File descriptor */
               ipp_uchar_t *buffer,	/* I - Data to write */
               size_t      length)	/* I - Number of bytes to write */
{
#ifdef WIN32
  return ((ssize_t)write(*fd, buffer, (unsigned)length));
#else
  return (write(*fd, buffer, length));
#endif /* WIN32 */
}
