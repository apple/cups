/*
 * "$Id$"
 *
 *   Destination option/media support for CUPS.
 *
 *   Copyright 2012 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
﻿ *   cupsCheckDestSupported() - Check that the option and value are supported
 *				by the destination.
 *   cupsCopyDestConflicts()  - Get conflicts and resolutions for a new
 *				option/value pair.
 *   cupsCopyDestInfo()       - Get the supported values/capabilities for the
 *				destination.
 *   cupsFreeDestInfo()       - Free destination information obtained using
 *				@link cupsCopyDestInfo@.
 *   cupsGetDestMediaByName() - Get media names, dimensions, and margins.
 *   cupsGetDestMediaBySize() - Get media names, dimensions, and margins.
 *   cups_compare_media_db()  - Compare two media entries.
 *   cups_copy_media_db()     - Copy a media entry.
 *   cups_create_media_db()   - Create the media database.
 *   cups_free_media_cb()     - Free a media entry.
 *   cups_get_media_db()      - Lookup the media entry for a given size.
 *   cups_is_close_media_db() - Compare two media entries to see if they are
 *				close to the same size.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"


/*
 * Local functions...
 */

static int		cups_compare_media_db(_cups_media_db_t *a,
			                      _cups_media_db_t *b);
static _cups_media_db_t	*cups_copy_media_db(_cups_media_db_t *mdb);
static void		cups_create_media_db(cups_dinfo_t *dinfo);
static void		cups_free_media_db(_cups_media_db_t *mdb);
static int		cups_get_media_db(cups_dinfo_t *dinfo,
			                  _pwg_media_t *pwg, unsigned flags,
			                  cups_size_t *size);
static int		cups_is_close_media_db(_cups_media_db_t *a,
			                       _cups_media_db_t *b);

/*
 * 'cupsCheckDestSupported()' - Check that the option and value are supported
 *                              by the destination.
 *
 * Returns 1 if supported, 0 otherwise.
 *
 * @since CUPS 1.6@
 */

int					/* O - 1 if supported, 0 otherwise */
cupsCheckDestSupported(
    http_t       *http,			/* I - Connection to destination */
    cups_dest_t  *dest,			/* I - Destination */
    cups_dinfo_t *dinfo,		/* I - Destination information */
    const char   *option,		/* I - Option */
    const char   *value)		/* I - Value */
{
  int			i;		/* Looping var */
  char			temp[1024];	/* Temporary string */
  int			int_value;	/* Integer value */
  int			xres_value,	/* Horizontal resolution */
			yres_value;	/* Vertical resolution */
  ipp_res_t		units_value;	/* Resolution units */
  ipp_attribute_t	*attr;		/* Attribute */
  _ipp_value_t		*attrval;	/* Current attribute value */


 /*
  * Range check input...
  */

  if (!http || !dest || !dinfo || !option || !value)
    return (0);

 /*
  * Lookup the attribute...
  */

  if (strstr(option, "-supported"))
    attr = ippFindAttribute(dinfo->attrs, option, IPP_TAG_ZERO);
  else
  {
    snprintf(temp, sizeof(temp), "%s-supported", option);
    attr = ippFindAttribute(dinfo->attrs, temp, IPP_TAG_ZERO);
  }

  if (!attr)
    return (0);

 /*
  * Compare values...
  */

  if (!strcmp(option, "media") && !strncmp(value, "custom_", 7))
  {
   /*
    * Check range of custom media sizes...
    */

    _pwg_media_t	*pwg;		/* Current PWG media size info */
    int			min_width,	/* Minimum width */
			min_length,	/* Minimum length */
			max_width,	/* Maximum width */
			max_length;	/* Maximum length */

   /*
    * Get the minimum and maximum size...
    */

    min_width = min_length = INT_MAX;
    max_width = max_length = 0;

    for (i = attr->num_values, attrval = attr->values;
	 i > 0;
	 i --, attrval ++)
    {
      if (!strncmp(attrval->string.text, "custom_min_", 11) &&
          (pwg = _pwgMediaForPWG(attrval->string.text)) != NULL)
      {
        min_width  = pwg->width;
        min_length = pwg->length;
      }
      else if (!strncmp(attrval->string.text, "custom_max_", 11) &&
	       (pwg = _pwgMediaForPWG(attrval->string.text)) != NULL)
      {
        max_width  = pwg->width;
        max_length = pwg->length;
      }
    }

   /*
    * Check the range...
    */

    if (min_width < INT_MAX && max_width > 0 &&
        (pwg = _pwgMediaForPWG(value)) != NULL &&
        pwg->width >= min_width && pwg->width <= max_width &&
        pwg->length >= min_length && pwg->length <= max_length)
      return (1);
  }
  else
  {
   /*
    * Check literal values...
    */

    switch (attr->value_tag)
    {
      case IPP_TAG_INTEGER :
      case IPP_TAG_ENUM :
          int_value = atoi(value);

          for (i = 0; i < attr->num_values; i ++)
            if (attr->values[i].integer == int_value)
              return (1);
          break;

      case IPP_TAG_BOOLEAN :
          return (attr->values[0].boolean);

      case IPP_TAG_RESOLUTION :
          if (sscanf(value, "%dx%d%15s", &xres_value, &yres_value, temp) != 3)
          {
            if (sscanf(value, "%d%15s", &xres_value, temp) != 2)
              return (0);

            yres_value = xres_value;
          }

          if (!strcmp(temp, "dpi"))
            units_value = IPP_RES_PER_INCH;
          else if (!strcmp(temp, "dpc") || !strcmp(temp, "dpcm"))
            units_value = IPP_RES_PER_CM;
          else
            return (0);

          for (i = attr->num_values, attrval = attr->values;
               i > 0;
               i --, attrval ++)
          {
            if (attrval->resolution.xres == xres_value &&
                attrval->resolution.yres == yres_value &&
                attrval->resolution.units == units_value)
              return (1);
          }
          break;

      case IPP_TAG_TEXT :
      case IPP_TAG_NAME :
      case IPP_TAG_KEYWORD :
      case IPP_TAG_CHARSET :
      case IPP_TAG_URI :
      case IPP_TAG_URISCHEME :
      case IPP_TAG_MIMETYPE :
      case IPP_TAG_LANGUAGE :
      case IPP_TAG_TEXTLANG :
      case IPP_TAG_NAMELANG :
          for (i = 0; i < attr->num_values; i ++)
            if (!strcmp(attr->values[i].string.text, value))
              return (1);
          break;

      default :
          break;
    }
  }

 /*
  * If we get there the option+value is not supported...
  */

  return (0);
}


/*
 * 'cupsCopyDestConflicts()' - Get conflicts and resolutions for a new
 *                             option/value pair.
 *
 * "num_options" and "options" represent the currently selected options by the
 * user.  "new_option" and "new_value" are the setting the user has just
 * changed.
 *
 * Returns 1 if there is a conflict and 0 otherwise.
 *
 * If "num_conflicts" and "conflicts" are not NULL, they are set to contain the
 * list of conflicting option/value pairs.  Similarly, if "num_resolved" and
 * "resolved" are not NULL they will be set to the list of changes needed to
 * resolve the conflict.
 *
 * If cupsCopyDestConflicts returns 1 but "num_resolved" and "resolved" are set
 * to 0 and NULL, respectively, then the conflict cannot be resolved.
 *
 * @since CUPS 1.6@
 */

int					/* O - 1 if there is a conflict */
cupsCopyDestConflicts(
    http_t        *http,		/* I - Connection to destination */
    cups_dest_t   *dest,		/* I - Destination */
    cups_dinfo_t  *dinfo,		/* I - Destination information */
    int           num_options,		/* I - Number of current options */
    cups_option_t *options,		/* I - Current options */
    const char    *new_option,		/* I - New option */
    const char    *new_value,		/* I - New value */
    int           *num_conflicts,	/* O - Number of conflicting options */
    cups_option_t **conflicts,		/* O - Conflicting options */
    int           *num_resolved,	/* O - Number of options to resolve */
    cups_option_t **resolved)		/* O - Resolved options */
{
 /*
  * Clear returned values...
  */

  if (num_conflicts)
    *num_conflicts = 0;

  if (conflicts)
    *conflicts = NULL;

  if (num_resolved)
    *num_resolved = 0;

  if (resolved)
    *resolved = NULL;

 /*
  * Range check input...
  */

  if (!http || !dest || !dinfo || !new_option || !new_value ||
      (num_conflicts != NULL) != (conflicts != NULL) ||
      (num_resolved != NULL) != (resolved != NULL))
    return (0);

 /*
  * Check for an resolve any conflicts...
  */

  /* TODO: implement me! */

  return (0);
}


/*
 * 'cupsCopyDestInfo()' - Get the supported values/capabilities for the
 *                        destination.
 *
 * The caller is responsible for calling @link cupsFreeDestInfo@ on the return
 * value. @code NULL@ is returned on error.
 *
 * @since CUPS 1.6@
 */

cups_dinfo_t *				/* O - Destination information */
cupsCopyDestInfo(
    http_t      *http,			/* I - Connection to destination */
    cups_dest_t *dest)			/* I - Destination */
{
  cups_dinfo_t	*dinfo;			/* Destination information */
  ipp_t		*request,		/* Get-Printer-Attributes request */
		*response;		/* Supported attributes */
  const char	*uri;			/* Printer URI */
  char		resource[1024];		/* Resource path */
  int		version;		/* IPP version */
  ipp_status_t	status;			/* Status of request */
  static const char * const requested_attrs[] =
  {					/* Requested attributes */
    "job-template",
    "media-col-database",
    "printer-description"
  };


  DEBUG_printf(("cupsCopyDestSupported(http=%p, dest=%p(%s))", http, dest,
                dest ? dest->name : ""));

 /*
  * Range check input...
  */

  if (!http || !dest)
    return (NULL);

 /*
  * Get the printer URI and resource path...
  */

  if ((uri = _cupsGetDestResource(dest, resource, sizeof(resource))) == NULL)
    return (NULL);

 /*
  * Get the supported attributes...
  */

  version = 20;

  do
  {
   /*
    * Send a Get-Printer-Attributes request...
    */

    request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL,
		 uri);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
		 NULL, cupsUser());
    ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
		  "requested-attributes",
		  (int)(sizeof(requested_attrs) / sizeof(requested_attrs[0])),
		  NULL, requested_attrs);
    response = cupsDoRequest(http, request, resource);
    status   = cupsLastError();

    if (status > IPP_OK_SUBST)
    {
      DEBUG_printf(("cupsCopyDestSupported: Get-Printer-Attributes for '%s' "
		    "returned %s (%s)", dest->name, ippErrorString(status),
		    cupsLastErrorString()));

      ippDelete(response);
      response = NULL;

      if (status == IPP_VERSION_NOT_SUPPORTED && version > 11)
        version = 11;
      else
        return (NULL);
    }
  }
  while (!response);

 /*
  * Allocate a cups_dinfo_t structure and return it...
  */

  if ((dinfo = calloc(1, sizeof(cups_dinfo_t))) == NULL)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, strerror(errno), 0);
    ippDelete(response);
    return (NULL);
  }

  dinfo->uri      = uri;
  dinfo->resource = _cupsStrAlloc(resource);
  dinfo->attrs    = response;

  return (dinfo);
}


/*
 * 'cupsFreeDestInfo()' - Free destination information obtained using
 *                        @link cupsCopyDestInfo@.
 */

void
cupsFreeDestInfo(cups_dinfo_t *dinfo)	/* I - Destination information */
{
 /*
  * Range check input...
  */

  if (!dinfo)
    return;

 /*
  * Free memory and return...
  */

  _cupsStrFree(dinfo->resource);

  ippDelete(dinfo->attrs);

  cupsArrayDelete(dinfo->constraints);

  cupsArrayDelete(dinfo->localizations);

  cupsArrayDelete(dinfo->media_db);

  free(dinfo);
}


/*
 * 'cupsGetDestMediaByName()' - Get media names, dimensions, and margins.
 *
 * The "media" string is a PWG media name, while "width" and "length" are the
 * dimensions in hundredths of millimeters.  "flags" provides some matching
 * guidance (multiple flags can be combined):
 *
 * CUPS_MEDIA_FLAGS_DEFAULT    = find the closest size supported by the printer
 * CUPS_MEDIA_FLAGS_BORDERLESS = find a borderless size
 * CUPS_MEDIA_FLAGS_DUPLEX     = find a size compatible with 2-sided printing
 * CUPS_MEDIA_FLAGS_EXACT      = find an exact match for the size
 * CUPS_MEDIA_FLAGS_READY      = if the printer supports media sensing, find the
 *                               size amongst the "ready" media.
 *
 * The matching result (if any) is returned in the "cups_size_t" structure.
 *
 * Returns 1 when there is a match and 0 if there is not a match.
 *
 * @since CUPS 1.6@
 */

int					/* O - 1 on match, 0 on failure */
cupsGetDestMediaByName(
    http_t       *http,			/* I - Connection to destination */
    cups_dest_t  *dest,			/* I - Destination */
    cups_dinfo_t *dinfo,		/* I - Destination information */
    const char   *media,		/* I - Media name */
    unsigned     flags,			/* I - Media matching flags */
    cups_size_t  *size)			/* O - Media size information */
{
  _pwg_media_t		*pwg;		/* PWG media info */


 /*
  * Range check input...
  */

  if (size)
    memset(size, 0, sizeof(cups_size_t));

  if (!http || !dest || !dinfo || !media || !size)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, strerror(EINVAL), 0);
    return (0);
  }

 /*
  * Lookup the media size name...
  */

  if ((pwg = _pwgMediaForPWG(media)) == NULL)
    if ((pwg = _pwgMediaForLegacy(media)) == NULL)
    {
      DEBUG_printf(("1cupsGetDestMediaByName: Unknown size '%s'.", media));
      _cupsSetError(IPP_INTERNAL_ERROR, _("Unknown media size name."), 1);
      return (0);
    }

 /*
  * Lookup the size...
  */

  return (cups_get_media_db(dinfo, pwg, flags, size));
}


/*
 * 'cupsGetDestMediaBySize()' - Get media names, dimensions, and margins.
 *
 * The "media" string is a PWG media name, while "width" and "length" are the
 * dimensions in hundredths of millimeters.  "flags" provides some matching
 * guidance (multiple flags can be combined):
 *
 * CUPS_MEDIA_FLAGS_DEFAULT    = find the closest size supported by the printer
 * CUPS_MEDIA_FLAGS_BORDERLESS = find a borderless size
 * CUPS_MEDIA_FLAGS_DUPLEX     = find a size compatible with 2-sided printing
 * CUPS_MEDIA_FLAGS_EXACT      = find an exact match for the size
 * CUPS_MEDIA_FLAGS_READY      = if the printer supports media sensing, find the
 *                               size amongst the "ready" media.
 *
 * The matching result (if any) is returned in the "cups_size_t" structure.
 *
 * Returns 1 when there is a match and 0 if there is not a match.
 *
 * @since CUPS 1.6@
 */

int					/* O - 1 on match, 0 on failure */
cupsGetDestMediaBySize(
    http_t       *http,			/* I - Connection to destination */
    cups_dest_t  *dest,			/* I - Destination */
    cups_dinfo_t *dinfo,		/* I - Destination information */
    int         width,			/* I - Media width in hundredths of
					 *     of millimeters */
    int         length,			/* I - Media length in hundredths of
					 *     of millimeters */
    unsigned     flags,			/* I - Media matching flags */
    cups_size_t  *size)			/* O - Media size information */
{
  _pwg_media_t		*pwg;		/* PWG media info */


 /*
  * Range check input...
  */

  if (size)
    memset(size, 0, sizeof(cups_size_t));

  if (!http || !dest || !dinfo || width <= 0 || length <= 0 || !size)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, strerror(EINVAL), 0);
    return (0);
  }

 /*
  * Lookup the media size name...
  */

  if ((pwg = _pwgMediaForSize(width, length)) == NULL)
  {
    DEBUG_printf(("1cupsGetDestMediaBySize: Invalid size %dx%d.", width,
                  length));
    _cupsSetError(IPP_INTERNAL_ERROR, _("Invalid media size."), 1);
    return (0);
  }

 /*
  * Lookup the size...
  */

  return (cups_get_media_db(dinfo, pwg, flags, size));
}


/*
 * 'cups_compare_media_db()' - Compare two media entries.
 */

static int				/* O - Result of comparison */
cups_compare_media_db(
    _cups_media_db_t *a,		/* I - First media entries */
    _cups_media_db_t *b)		/* I - Second media entries */
{
  int	result;				/* Result of comparison */


  if ((result = a->width - b->width) == 0)
    result = a->length - b->length;

  return (result);
}


/*
 * 'cups_copy_media_db()' - Copy a media entry.
 */

static _cups_media_db_t *		/* O - New media entry */
cups_copy_media_db(
    _cups_media_db_t *mdb)		/* I - Media entry to copy */
{
  _cups_media_db_t *temp;		/* New media entry */


  if ((temp = calloc(1, sizeof(_cups_media_db_t))) == NULL)
    return (NULL);

  if (mdb->color)
    temp->color = _cupsStrAlloc(mdb->color);
  if (mdb->key)
    temp->key = _cupsStrAlloc(mdb->key);
  if (mdb->info)
    temp->info = _cupsStrAlloc(mdb->info);
  if (mdb->size_name)
    temp->size_name = _cupsStrAlloc(mdb->size_name);
  if (mdb->source)
    temp->source = _cupsStrAlloc(mdb->source);
  if (mdb->type)
    temp->type = _cupsStrAlloc(mdb->type);

  temp->width  = mdb->width;
  temp->length = mdb->length;
  temp->bottom = mdb->bottom;
  temp->left   = mdb->left;
  temp->right  = mdb->right;
  temp->top    = mdb->top;

  return (temp);
}


/*
 * 'cups_create_media_db()' - Create the media database.
 */

static void
cups_create_media_db(
    cups_dinfo_t *dinfo)		/* I - Destination information */
{
  int			i;		/* Looping var */
  _ipp_value_t		*val;		/* Current value */
  ipp_attribute_t	*media_col_db,	/* media-col-database */
			*media_attr,	/* media-xxx */
			*x_dimension,	/* x-dimension */
			*y_dimension;	/* y-dimension */
  _pwg_media_t		*pwg;		/* PWG media info */
  _cups_media_db_t	mdb;		/* Media entry */


  dinfo->media_db = cupsArrayNew3((cups_array_func_t)cups_compare_media_db,
                                  NULL, NULL, 0,
                                  (cups_acopy_func_t)cups_copy_media_db,
                                  (cups_afree_func_t)cups_free_media_db);
  dinfo->min_size.width  = INT_MAX;
  dinfo->min_size.length = INT_MAX;
  dinfo->max_size.width  = 0;
  dinfo->max_size.length = 0;

  if ((media_col_db = ippFindAttribute(dinfo->attrs, "media-col-database",
                                       IPP_TAG_BEGIN_COLLECTION)) != NULL)
  {
    _ipp_value_t	*custom = NULL;	/* Custom size range value */

    for (i = media_col_db->num_values, val = media_col_db->values;
         i > 0;
         i --, val ++)
    {
      memset(&mdb, 0, sizeof(mdb));

      if ((media_attr = ippFindAttribute(val->collection, "media-size",
                                         IPP_TAG_BEGIN_COLLECTION)) != NULL)
      {
        ipp_t	*media_size = media_attr->values[0].collection;
					/* media-size collection value */

        if ((x_dimension = ippFindAttribute(media_size, "x-dimension",
                                          IPP_TAG_INTEGER)) != NULL &&
	    (y_dimension = ippFindAttribute(media_size, "y-dimension",
					    IPP_TAG_INTEGER)) != NULL)
	{
	  mdb.width  = x_dimension->values[0].integer;
	  mdb.length = y_dimension->values[0].integer;
	}
        else if ((x_dimension = ippFindAttribute(media_size, "x-dimension",
					       IPP_TAG_RANGE)) != NULL &&
		 (y_dimension = ippFindAttribute(media_size, "y-dimension",
						 IPP_TAG_RANGE)) != NULL)
	{
	 /*
	  * Custom size range; save this as the custom size value with default
	  * margins, then continue; we'll capture the real margins below...
	  */

	  custom = val;

	  dinfo->min_size.width  = x_dimension->values[0].range.lower;
	  dinfo->min_size.length = y_dimension->values[0].range.lower;
	  dinfo->min_size.left   =
	  dinfo->min_size.right  = 635; /* Default 1/4" side margins */
	  dinfo->min_size.top    =
	  dinfo->min_size.bottom = 1270; /* Default 1/2" top/bottom margins */

	  dinfo->max_size.width  = x_dimension->values[0].range.upper;
	  dinfo->max_size.length = y_dimension->values[0].range.upper;
	  dinfo->max_size.left   =
	  dinfo->max_size.right  = 635; /* Default 1/4" side margins */
	  dinfo->max_size.top    =
	  dinfo->max_size.bottom = 1270; /* Default 1/2" top/bottom margins */

	  continue;
	}
      }

      if ((media_attr = ippFindAttribute(val->collection, "media-color",
                                         IPP_TAG_ZERO)) != NULL &&
          (media_attr->value_tag == IPP_TAG_NAME ||
           media_attr->value_tag == IPP_TAG_NAMELANG ||
           media_attr->value_tag == IPP_TAG_KEYWORD))
        mdb.color = media_attr->values[0].string.text;

      if ((media_attr = ippFindAttribute(val->collection, "media-info",
                                         IPP_TAG_TEXT)) != NULL)
        mdb.info = media_attr->values[0].string.text;

      if ((media_attr = ippFindAttribute(val->collection, "media-key",
                                         IPP_TAG_ZERO)) != NULL &&
          (media_attr->value_tag == IPP_TAG_NAME ||
           media_attr->value_tag == IPP_TAG_NAMELANG ||
           media_attr->value_tag == IPP_TAG_KEYWORD))
        mdb.key = media_attr->values[0].string.text;

      if ((media_attr = ippFindAttribute(val->collection, "media-size-name",
                                         IPP_TAG_ZERO)) != NULL &&
          (media_attr->value_tag == IPP_TAG_NAME ||
           media_attr->value_tag == IPP_TAG_NAMELANG ||
           media_attr->value_tag == IPP_TAG_KEYWORD))
        mdb.size_name = media_attr->values[0].string.text;

      if ((media_attr = ippFindAttribute(val->collection, "media-source",
                                         IPP_TAG_ZERO)) != NULL &&
          (media_attr->value_tag == IPP_TAG_NAME ||
           media_attr->value_tag == IPP_TAG_NAMELANG ||
           media_attr->value_tag == IPP_TAG_KEYWORD))
        mdb.source = media_attr->values[0].string.text;

      if ((media_attr = ippFindAttribute(val->collection, "media-type",
                                         IPP_TAG_ZERO)) != NULL &&
          (media_attr->value_tag == IPP_TAG_NAME ||
           media_attr->value_tag == IPP_TAG_NAMELANG ||
           media_attr->value_tag == IPP_TAG_KEYWORD))
        mdb.type = media_attr->values[0].string.text;

      if ((media_attr = ippFindAttribute(val->collection, "media-bottom-margin",
                                         IPP_TAG_INTEGER)) != NULL)
        mdb.bottom = media_attr->values[0].integer;

      if ((media_attr = ippFindAttribute(val->collection, "media-left-margin",
                                         IPP_TAG_INTEGER)) != NULL)
        mdb.left = media_attr->values[0].integer;

      if ((media_attr = ippFindAttribute(val->collection, "media-right-margin",
                                         IPP_TAG_INTEGER)) != NULL)
        mdb.right = media_attr->values[0].integer;

      if ((media_attr = ippFindAttribute(val->collection, "media-top-margin",
                                         IPP_TAG_INTEGER)) != NULL)
        mdb.top = media_attr->values[0].integer;

      cupsArrayAdd(dinfo->media_db, &mdb);
    }

    if (custom)
    {
      if ((media_attr = ippFindAttribute(custom->collection,
                                         "media-bottom-margin",
                                         IPP_TAG_INTEGER)) != NULL)
      {
        dinfo->min_size.top =
        dinfo->max_size.top = media_attr->values[0].integer;
      }

      if ((media_attr = ippFindAttribute(custom->collection,
                                         "media-left-margin",
                                         IPP_TAG_INTEGER)) != NULL)
      {
        dinfo->min_size.left =
        dinfo->max_size.left = media_attr->values[0].integer;
      }

      if ((media_attr = ippFindAttribute(custom->collection,
                                         "media-right-margin",
                                         IPP_TAG_INTEGER)) != NULL)
      {
        dinfo->min_size.right =
        dinfo->max_size.right = media_attr->values[0].integer;
      }

      if ((media_attr = ippFindAttribute(custom->collection,
                                         "media-top-margin",
                                         IPP_TAG_INTEGER)) != NULL)
      {
        dinfo->min_size.top =
        dinfo->max_size.top = media_attr->values[0].integer;
      }
    }
  }
  else if ((media_attr = ippFindAttribute(dinfo->attrs, "media-supported",
                                          IPP_TAG_ZERO)) != NULL &&
           (media_attr->value_tag == IPP_TAG_NAME ||
            media_attr->value_tag == IPP_TAG_NAMELANG ||
            media_attr->value_tag == IPP_TAG_KEYWORD))
  {
    memset(&mdb, 0, sizeof(mdb));

    mdb.left   =
    mdb.right  = 635; /* Default 1/4" side margins */
    mdb.top    =
    mdb.bottom = 1270; /* Default 1/2" top/bottom margins */

    for (i = media_col_db->num_values, val = media_col_db->values;
         i > 0;
         i --, val ++)
    {
      if ((pwg = _pwgMediaForPWG(val->string.text)) == NULL)
        if ((pwg = _pwgMediaForLegacy(val->string.text)) == NULL)
	{
	  DEBUG_printf(("3cups_create_media_db: Ignoring unknown size '%s'.",
			val->string.text));
	  continue;
	}

      mdb.width  = pwg->width;
      mdb.length = pwg->length;

      if (!strncmp(val->string.text, "custom_min_", 11))
      {
        mdb.size_name   = NULL;
        dinfo->min_size = mdb;
      }
      else if (!strncmp(val->string.text, "custom_max_", 11))
      {
        mdb.size_name   = NULL;
        dinfo->max_size = mdb;
      }
      else
      {
        mdb.size_name = val->string.text;

        cupsArrayAdd(dinfo->media_db, &mdb);
      }
    }
  }
}


/*
 * 'cups_free_media_cb()' - Free a media entry.
 */

static void
cups_free_media_db(
    _cups_media_db_t *mdb)		/* I - Media entry to free */
{
  if (mdb->color)
    _cupsStrFree(mdb->color);
  if (mdb->key)
    _cupsStrFree(mdb->key);
  if (mdb->info)
    _cupsStrFree(mdb->info);
  if (mdb->size_name)
    _cupsStrFree(mdb->size_name);
  if (mdb->source)
    _cupsStrFree(mdb->source);
  if (mdb->type)
    _cupsStrFree(mdb->type);

  free(mdb);
}


/*
 * 'cups_get_media_db()' - Lookup the media entry for a given size.
 */

static int				/* O - 1 on match, 0 on failure */
cups_get_media_db(cups_dinfo_t *dinfo,	/* I - Destination information */
                  _pwg_media_t *pwg,	/* I - PWG media info */
                  unsigned     flags,	/* I - Media matching flags */
                  cups_size_t *size)	/* O - Media size/margin/name info */
{
  _cups_media_db_t	*mdb,		/* Current media database entry */
			*best = NULL,	/* Best matching entry */
			key;		/* Search key */


 /*
  * Create the media database as needed...
  */

  if (!dinfo->media_db)
    cups_create_media_db(dinfo);

 /*
  * Find a match...
  */

  memset(&key, 0, sizeof(key));
  key.width  = pwg->width;
  key.length = pwg->length;

  if ((mdb = cupsArrayFind(dinfo->media_db, &key)) != NULL)
  {
   /*
    * Found an exact match, let's figure out the best margins for the flags
    * supplied...
    */

    best = mdb;

    if (flags & CUPS_MEDIA_FLAGS_BORDERLESS)
    {
     /*
      * Look for the smallest margins...
      */

      if (best->left != 0 || best->right != 0 || best->top != 0 ||
          best->bottom != 0)
      {
	for (mdb = (_cups_media_db_t *)cupsArrayNext(dinfo->media_db);
	     mdb && !cups_compare_media_db(mdb, &key);
	     mdb = (_cups_media_db_t *)cupsArrayNext(dinfo->media_db))
	{
	  if (mdb->left <= best->left && mdb->right <= best->right &&
	      mdb->top <= best->top && mdb->bottom <= best->bottom)
	  {
	    best = mdb;
	    if (mdb->left == 0 && mdb->right == 0 && mdb->bottom == 0 &&
		mdb->top == 0)
	      break;
	  }
	}
      }

     /*
      * If we need an exact match, return no-match if the size is not
      * borderless.
      */

      if ((flags & CUPS_MEDIA_FLAGS_EXACT) &&
          (best->left || best->right || best->top || best->bottom))
        return (0);
    }
    else if (flags & CUPS_MEDIA_FLAGS_DUPLEX)
    {
     /*
      * Look for the largest margins...
      */

      for (mdb = (_cups_media_db_t *)cupsArrayNext(dinfo->media_db);
	   mdb && !cups_compare_media_db(mdb, &key);
	   mdb = (_cups_media_db_t *)cupsArrayNext(dinfo->media_db))
      {
	if (mdb->left >= best->left && mdb->right >= best->right &&
	    mdb->top >= best->top && mdb->bottom >= best->bottom)
	  best = mdb;
      }
    }
    else
    {
     /*
      * Look for the smallest non-zero margins...
      */

      for (mdb = (_cups_media_db_t *)cupsArrayNext(dinfo->media_db);
	   mdb && !cups_compare_media_db(mdb, &key);
	   mdb = (_cups_media_db_t *)cupsArrayNext(dinfo->media_db))
      {
	if (((mdb->left > 0 && mdb->left <= best->left) || best->left == 0) &&
	    ((mdb->right > 0 && mdb->right <= best->right) ||
	     best->right == 0) &&
	    ((mdb->top > 0 && mdb->top <= best->top) || best->top == 0) &&
	    ((mdb->bottom > 0 && mdb->bottom <= best->bottom) ||
	     best->bottom == 0))
	  best = mdb;
      }
    }
  }
  else if (flags & CUPS_MEDIA_FLAGS_EXACT)
  {
   /*
    * See if we can do this as a custom size...
    */

    if (pwg->width < dinfo->min_size.width ||
        pwg->width > dinfo->max_size.width ||
        pwg->length < dinfo->min_size.length ||
        pwg->length > dinfo->max_size.length)
      return (0);			/* Out of range */

    if ((flags & CUPS_MEDIA_FLAGS_BORDERLESS) &&
        (dinfo->min_size.left > 0 || dinfo->min_size.right > 0 ||
         dinfo->min_size.top > 0 || dinfo->min_size.bottom > 0))
      return (0);			/* Not borderless */

    key.size_name = (char *)pwg->pwg;
    key.bottom    = dinfo->min_size.bottom;
    key.left      = dinfo->min_size.left;
    key.right     = dinfo->min_size.right;
    key.top       = dinfo->min_size.top;

    best = &key;
  }
  else if (pwg->width >= dinfo->min_size.width &&
	   pwg->width <= dinfo->max_size.width &&
	   pwg->length >= dinfo->min_size.length &&
	   pwg->length <= dinfo->max_size.length)
  {
   /*
    * Map to custom size...
    */

    key.size_name = (char *)pwg->pwg;
    key.bottom    = dinfo->min_size.bottom;
    key.left      = dinfo->min_size.left;
    key.right     = dinfo->min_size.right;
    key.top       = dinfo->min_size.top;

    best = &key;
  }
  else
  {
   /*
    * Find a close size...
    */

    for (mdb = (_cups_media_db_t *)cupsArrayFirst(dinfo->media_db);
         mdb;
         mdb = (_cups_media_db_t *)cupsArrayNext(dinfo->media_db))
      if (cups_is_close_media_db(mdb, &key))
        break;

    if (!mdb)
      return (0);

    best = mdb;

    if (flags & CUPS_MEDIA_FLAGS_BORDERLESS)
    {
     /*
      * Look for the smallest margins...
      */

      if (best->left != 0 || best->right != 0 || best->top != 0 ||
          best->bottom != 0)
      {
	for (mdb = (_cups_media_db_t *)cupsArrayNext(dinfo->media_db);
	     mdb && cups_is_close_media_db(mdb, &key);
	     mdb = (_cups_media_db_t *)cupsArrayNext(dinfo->media_db))
	{
	  if (mdb->left <= best->left && mdb->right <= best->right &&
	      mdb->top <= best->top && mdb->bottom <= best->bottom)
	  {
	    best = mdb;
	    if (mdb->left == 0 && mdb->right == 0 && mdb->bottom == 0 &&
		mdb->top == 0)
	      break;
	  }
	}
      }
    }
    else if (flags & CUPS_MEDIA_FLAGS_DUPLEX)
    {
     /*
      * Look for the largest margins...
      */

      for (mdb = (_cups_media_db_t *)cupsArrayNext(dinfo->media_db);
	   mdb && cups_is_close_media_db(mdb, &key);
	   mdb = (_cups_media_db_t *)cupsArrayNext(dinfo->media_db))
      {
	if (mdb->left >= best->left && mdb->right >= best->right &&
	    mdb->top >= best->top && mdb->bottom >= best->bottom)
	  best = mdb;
      }
    }
    else
    {
     /*
      * Look for the smallest non-zero margins...
      */

      for (mdb = (_cups_media_db_t *)cupsArrayNext(dinfo->media_db);
	   mdb && cups_is_close_media_db(mdb, &key);
	   mdb = (_cups_media_db_t *)cupsArrayNext(dinfo->media_db))
      {
	if (((mdb->left > 0 && mdb->left <= best->left) || best->left == 0) &&
	    ((mdb->right > 0 && mdb->right <= best->right) ||
	     best->right == 0) &&
	    ((mdb->top > 0 && mdb->top <= best->top) || best->top == 0) &&
	    ((mdb->bottom > 0 && mdb->bottom <= best->bottom) ||
	     best->bottom == 0))
	  best = mdb;
      }
    }
  }

  if (best)
  {
   /*
    * Return the matching size...
    */

    if (best->size_name)
      strlcpy(size->media, best->size_name, sizeof(size->media));
    else if (best->key)
      strlcpy(size->media, best->key, sizeof(size->media));
    else
      strlcpy(size->media, pwg->pwg, sizeof(size->media));

    size->width  = best->width;
    size->length = best->length;
    size->bottom = best->bottom;
    size->left   = best->left;
    size->right  = best->right;
    size->top    = best->top;

    return (1);
  }

  return (0);
}


/*
 * 'cups_is_close_media_db()' - Compare two media entries to see if they are
 *                              close to the same size.
 *
 * Currently we use 5 points (from PostScript) as the matching range...
 */

static int				/* O - 1 if the sizes are close */
cups_is_close_media_db(
    _cups_media_db_t *a,		/* I - First media entries */
    _cups_media_db_t *b)		/* I - Second media entries */
{
  int	dwidth,				/* Difference in width */
	dlength;			/* Difference in length */


  dwidth  = a->width - b->width;
  dlength = a->length - b->length;

  return (dwidth >= -176 && dwidth <= 176 &&
          dlength >= -176 && dlength <= 176);
}


/*
 * End of "$Id$".
 */
