/*
 * "$Id: ppd-cache.c 11833 2014-04-24 15:04:15Z msweet $"
 *
 *   PPD cache implementation for CUPS.
 *
 *   Copyright 2010-2013 by Apple Inc.
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
 *   _ppdCacheCreateWithFile() - Create PPD cache and mapping data from a
 *                               written file.
 *   _ppdCacheCreateWithPPD()  - Create PWG mapping data from a PPD file.
 *   _ppdCacheDestroy()        - Free all memory used for PWG mapping data.
 *   _ppdCacheGetBin()         - Get the PWG output-bin keyword associated with
 *                               a PPD OutputBin.
 *   _ppdCacheGetInputSlot()   - Get the PPD InputSlot associated with the job
 *                               attributes or a keyword string.
 *   _ppdCacheGetMediaType()   - Get the PPD MediaType associated with the job
 *                               attributes or a keyword string.
 *   _ppdCacheGetOutputBin()   - Get the PPD OutputBin associated with the
 *                               keyword string.
 *   _ppdCacheGetPageSize()    - Get the PPD PageSize associated with the job
 *                               attributes or a keyword string.
 *   _ppdCacheGetSize()        - Get the PWG size associated with a PPD
 *                               PageSize.
 *   _ppdCacheGetSource()      - Get the PWG media-source associated with a PPD
 *                               InputSlot.
 *   _ppdCacheGetType()        - Get the PWG media-type associated with a PPD
 *                               MediaType.
 *   _ppdCacheWriteFile()      - Write PWG mapping data to a file.
 *   _pwgInputSlotForSource()  - Get the InputSlot name for the given PWG
 *                               media-source.
 *   _pwgMediaTypeForType()    - Get the MediaType name for the given PWG
 *                               media-type.
 *   _pwgPageSizeForMedia()    - Get the PageSize name for the given media.
 *   pwg_ppdize_name()         - Convert an IPP keyword to a PPD keyword.
 *   pwg_unppdize_name()       - Convert a PPD keyword to a lowercase IPP
 *                               keyword.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include <math.h>


/*
 * Macro to test for two almost-equal PWG measurements.
 */

#define _PWG_EQUIVALENT(x, y)	(abs((x)-(y)) < 2)


/*
 * Local functions...
 */

static int	pwg_compare_finishings(_pwg_finishings_t *a,
		                       _pwg_finishings_t *b);
static void	pwg_free_finishings(_pwg_finishings_t *f);
static void	pwg_ppdize_name(const char *ipp, char *name, size_t namesize);
static void	pwg_unppdize_name(const char *ppd, char *name, size_t namesize,
		                  const char *dashchars);


/*
 * '_ppdCacheCreateWithFile()' - Create PPD cache and mapping data from a
 *                               written file.
 *
 * Use the @link _ppdCacheWriteFile@ function to write PWG mapping data to a
 * file.
 */

_ppd_cache_t *				/* O  - PPD cache and mapping data */
_ppdCacheCreateWithFile(
    const char *filename,		/* I  - File to read */
    ipp_t      **attrs)			/* IO - IPP attributes, if any */
{
  cups_file_t	*fp;			/* File */
  _ppd_cache_t	*pc;			/* PWG mapping data */
  pwg_size_t	*size;			/* Current size */
  pwg_map_t	*map;			/* Current map */
  _pwg_finishings_t *finishings;	/* Current finishings option */
  int		linenum,		/* Current line number */
		num_bins,		/* Number of bins in file */
		num_sizes,		/* Number of sizes in file */
		num_sources,		/* Number of sources in file */
		num_types;		/* Number of types in file */
  char		line[2048],		/* Current line */
		*value,			/* Pointer to value in line */
		*valueptr,		/* Pointer into value */
		pwg_keyword[128],	/* PWG keyword */
		ppd_keyword[PPD_MAX_NAME];
					/* PPD keyword */
  _pwg_print_color_mode_t print_color_mode;
					/* Print color mode for preset */
  _pwg_print_quality_t print_quality;	/* Print quality for preset */


  DEBUG_printf(("_ppdCacheCreateWithFile(filename=\"%s\")", filename));

 /*
  * Range check input...
  */

  if (attrs)
    *attrs = NULL;

  if (!filename)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    return (NULL);
  }

 /*
  * Open the file...
  */

  if ((fp = cupsFileOpen(filename, "r")) == NULL)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
    return (NULL);
  }

 /*
  * Read the first line and make sure it has "#CUPS-PPD-CACHE-version" in it...
  */

  if (!cupsFileGets(fp, line, sizeof(line)))
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
    DEBUG_puts("_ppdCacheCreateWithFile: Unable to read first line.");
    cupsFileClose(fp);
    return (NULL);
  }

  if (strncmp(line, "#CUPS-PPD-CACHE-", 16))
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
    DEBUG_printf(("_ppdCacheCreateWithFile: Wrong first line \"%s\".", line));
    cupsFileClose(fp);
    return (NULL);
  }

  if (atoi(line + 16) != _PPD_CACHE_VERSION)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Out of date PPD cache file."), 1);
    DEBUG_printf(("_ppdCacheCreateWithFile: Cache file has version %s, "
                  "expected %d.", line + 16, _PPD_CACHE_VERSION));
    cupsFileClose(fp);
    return (NULL);
  }

 /*
  * Allocate the mapping data structure...
  */

  if ((pc = calloc(1, sizeof(_ppd_cache_t))) == NULL)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
    DEBUG_puts("_ppdCacheCreateWithFile: Unable to allocate _ppd_cache_t.");
    goto create_error;
  }

  pc->max_copies = 9999;

 /*
  * Read the file...
  */

  linenum     = 0;
  num_bins    = 0;
  num_sizes   = 0;
  num_sources = 0;
  num_types   = 0;

  while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
  {
    DEBUG_printf(("_ppdCacheCreateWithFile: line=\"%s\", value=\"%s\", "
                  "linenum=%d", line, value, linenum));

    if (!value)
    {
      DEBUG_printf(("_ppdCacheCreateWithFile: Missing value on line %d.",
                    linenum));
      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
      goto create_error;
    }
    else if (!_cups_strcasecmp(line, "Filter"))
    {
      if (!pc->filters)
        pc->filters = cupsArrayNew3(NULL, NULL, NULL, 0,
	                            (cups_acopy_func_t)_cupsStrAlloc,
				    (cups_afree_func_t)_cupsStrFree);

      cupsArrayAdd(pc->filters, value);
    }
    else if (!_cups_strcasecmp(line, "PreFilter"))
    {
      if (!pc->prefilters)
        pc->prefilters = cupsArrayNew3(NULL, NULL, NULL, 0,
	                               (cups_acopy_func_t)_cupsStrAlloc,
				       (cups_afree_func_t)_cupsStrFree);

      cupsArrayAdd(pc->prefilters, value);
    }
    else if (!_cups_strcasecmp(line, "Product"))
    {
      pc->product = _cupsStrAlloc(value);
    }
    else if (!_cups_strcasecmp(line, "SingleFile"))
    {
      pc->single_file = !_cups_strcasecmp(value, "true");
    }
    else if (!_cups_strcasecmp(line, "IPP"))
    {
      off_t	pos = cupsFileTell(fp),	/* Position in file */
		length = strtol(value, NULL, 10);
					/* Length of IPP attributes */

      if (attrs && *attrs)
      {
        DEBUG_puts("_ppdCacheCreateWithFile: IPP listed multiple times.");
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }
      else if (length <= 0)
      {
        DEBUG_puts("_ppdCacheCreateWithFile: Bad IPP length.");
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      if (attrs)
      {
       /*
        * Read IPP attributes into the provided variable...
	*/

        *attrs = ippNew();

        if (ippReadIO(fp, (ipp_iocb_t)cupsFileRead, 1, NULL,
		      *attrs) != IPP_STATE_DATA)
	{
	  DEBUG_puts("_ppdCacheCreateWithFile: Bad IPP data.");
	  _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	  goto create_error;
	}
      }
      else
      {
       /*
        * Skip the IPP data entirely...
	*/

        cupsFileSeek(fp, pos + length);
      }

      if (cupsFileTell(fp) != (pos + length))
      {
        DEBUG_puts("_ppdCacheCreateWithFile: Bad IPP data.");
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }
    }
    else if (!_cups_strcasecmp(line, "NumBins"))
    {
      if (num_bins > 0)
      {
        DEBUG_puts("_ppdCacheCreateWithFile: NumBins listed multiple times.");
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      if ((num_bins = atoi(value)) <= 0 || num_bins > 65536)
      {
        DEBUG_printf(("_ppdCacheCreateWithFile: Bad NumBins value %d on line "
		      "%d.", num_sizes, linenum));
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      if ((pc->bins = calloc(num_bins, sizeof(pwg_map_t))) == NULL)
      {
        DEBUG_printf(("_ppdCacheCreateWithFile: Unable to allocate %d bins.",
	              num_sizes));
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
	goto create_error;
      }
    }
    else if (!_cups_strcasecmp(line, "Bin"))
    {
      if (sscanf(value, "%127s%40s", pwg_keyword, ppd_keyword) != 2)
      {
        DEBUG_printf(("_ppdCacheCreateWithFile: Bad Bin on line %d.", linenum));
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      if (pc->num_bins >= num_bins)
      {
        DEBUG_printf(("_ppdCacheCreateWithFile: Too many Bin's on line %d.",
	              linenum));
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      map      = pc->bins + pc->num_bins;
      map->pwg = _cupsStrAlloc(pwg_keyword);
      map->ppd = _cupsStrAlloc(ppd_keyword);

      pc->num_bins ++;
    }
    else if (!_cups_strcasecmp(line, "NumSizes"))
    {
      if (num_sizes > 0)
      {
        DEBUG_puts("_ppdCacheCreateWithFile: NumSizes listed multiple times.");
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      if ((num_sizes = atoi(value)) < 0 || num_sizes > 65536)
      {
        DEBUG_printf(("_ppdCacheCreateWithFile: Bad NumSizes value %d on line "
	              "%d.", num_sizes, linenum));
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      if (num_sizes > 0)
      {
	if ((pc->sizes = calloc(num_sizes, sizeof(pwg_size_t))) == NULL)
	{
	  DEBUG_printf(("_ppdCacheCreateWithFile: Unable to allocate %d sizes.",
			num_sizes));
	  _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
	  goto create_error;
	}
      }
    }
    else if (!_cups_strcasecmp(line, "Size"))
    {
      if (pc->num_sizes >= num_sizes)
      {
        DEBUG_printf(("_ppdCacheCreateWithFile: Too many Size's on line %d.",
	              linenum));
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      size = pc->sizes + pc->num_sizes;

      if (sscanf(value, "%127s%40s%d%d%d%d%d%d", pwg_keyword, ppd_keyword,
		 &(size->width), &(size->length), &(size->left),
		 &(size->bottom), &(size->right), &(size->top)) != 8)
      {
        DEBUG_printf(("_ppdCacheCreateWithFile: Bad Size on line %d.",
	              linenum));
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      size->map.pwg = _cupsStrAlloc(pwg_keyword);
      size->map.ppd = _cupsStrAlloc(ppd_keyword);

      pc->num_sizes ++;
    }
    else if (!_cups_strcasecmp(line, "CustomSize"))
    {
      if (pc->custom_max_width > 0)
      {
        DEBUG_printf(("_ppdCacheCreateWithFile: Too many CustomSize's on line "
	              "%d.", linenum));
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      if (sscanf(value, "%d%d%d%d%d%d%d%d", &(pc->custom_max_width),
                 &(pc->custom_max_length), &(pc->custom_min_width),
		 &(pc->custom_min_length), &(pc->custom_size.left),
		 &(pc->custom_size.bottom), &(pc->custom_size.right),
		 &(pc->custom_size.top)) != 8)
      {
        DEBUG_printf(("_ppdCacheCreateWithFile: Bad CustomSize on line %d.",
	              linenum));
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      pwgFormatSizeName(pwg_keyword, sizeof(pwg_keyword), "custom", "max",
		        pc->custom_max_width, pc->custom_max_length, NULL);
      pc->custom_max_keyword = _cupsStrAlloc(pwg_keyword);

      pwgFormatSizeName(pwg_keyword, sizeof(pwg_keyword), "custom", "min",
		        pc->custom_min_width, pc->custom_min_length, NULL);
      pc->custom_min_keyword = _cupsStrAlloc(pwg_keyword);
    }
    else if (!_cups_strcasecmp(line, "SourceOption"))
    {
      pc->source_option = _cupsStrAlloc(value);
    }
    else if (!_cups_strcasecmp(line, "NumSources"))
    {
      if (num_sources > 0)
      {
        DEBUG_puts("_ppdCacheCreateWithFile: NumSources listed multiple "
	           "times.");
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      if ((num_sources = atoi(value)) <= 0 || num_sources > 65536)
      {
        DEBUG_printf(("_ppdCacheCreateWithFile: Bad NumSources value %d on "
	              "line %d.", num_sources, linenum));
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      if ((pc->sources = calloc(num_sources, sizeof(pwg_map_t))) == NULL)
      {
        DEBUG_printf(("_ppdCacheCreateWithFile: Unable to allocate %d sources.",
	              num_sources));
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
	goto create_error;
      }
    }
    else if (!_cups_strcasecmp(line, "Source"))
    {
      if (sscanf(value, "%127s%40s", pwg_keyword, ppd_keyword) != 2)
      {
        DEBUG_printf(("_ppdCacheCreateWithFile: Bad Source on line %d.",
	              linenum));
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      if (pc->num_sources >= num_sources)
      {
        DEBUG_printf(("_ppdCacheCreateWithFile: Too many Source's on line %d.",
	              linenum));
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      map      = pc->sources + pc->num_sources;
      map->pwg = _cupsStrAlloc(pwg_keyword);
      map->ppd = _cupsStrAlloc(ppd_keyword);

      pc->num_sources ++;
    }
    else if (!_cups_strcasecmp(line, "NumTypes"))
    {
      if (num_types > 0)
      {
        DEBUG_puts("_ppdCacheCreateWithFile: NumTypes listed multiple times.");
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      if ((num_types = atoi(value)) <= 0 || num_types > 65536)
      {
        DEBUG_printf(("_ppdCacheCreateWithFile: Bad NumTypes value %d on "
	              "line %d.", num_types, linenum));
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      if ((pc->types = calloc(num_types, sizeof(pwg_map_t))) == NULL)
      {
        DEBUG_printf(("_ppdCacheCreateWithFile: Unable to allocate %d types.",
	              num_types));
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
	goto create_error;
      }
    }
    else if (!_cups_strcasecmp(line, "Type"))
    {
      if (sscanf(value, "%127s%40s", pwg_keyword, ppd_keyword) != 2)
      {
        DEBUG_printf(("_ppdCacheCreateWithFile: Bad Type on line %d.",
	              linenum));
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      if (pc->num_types >= num_types)
      {
        DEBUG_printf(("_ppdCacheCreateWithFile: Too many Type's on line %d.",
	              linenum));
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      map      = pc->types + pc->num_types;
      map->pwg = _cupsStrAlloc(pwg_keyword);
      map->ppd = _cupsStrAlloc(ppd_keyword);

      pc->num_types ++;
    }
    else if (!_cups_strcasecmp(line, "Preset"))
    {
     /*
      * Preset output-mode print-quality name=value ...
      */

      print_color_mode = (_pwg_print_color_mode_t)strtol(value, &valueptr, 10);
      print_quality    = (_pwg_print_quality_t)strtol(valueptr, &valueptr, 10);

      if (print_color_mode < _PWG_PRINT_COLOR_MODE_MONOCHROME ||
          print_color_mode >= _PWG_PRINT_COLOR_MODE_MAX ||
	  print_quality < _PWG_PRINT_QUALITY_DRAFT ||
	  print_quality >= _PWG_PRINT_QUALITY_MAX ||
	  valueptr == value || !*valueptr)
      {
        DEBUG_printf(("_ppdCacheCreateWithFile: Bad Preset on line %d.",
	              linenum));
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
	goto create_error;
      }

      pc->num_presets[print_color_mode][print_quality] =
          cupsParseOptions(valueptr, 0,
	                   pc->presets[print_color_mode] + print_quality);
    }
    else if (!_cups_strcasecmp(line, "SidesOption"))
      pc->sides_option = _cupsStrAlloc(value);
    else if (!_cups_strcasecmp(line, "Sides1Sided"))
      pc->sides_1sided = _cupsStrAlloc(value);
    else if (!_cups_strcasecmp(line, "Sides2SidedLong"))
      pc->sides_2sided_long = _cupsStrAlloc(value);
    else if (!_cups_strcasecmp(line, "Sides2SidedShort"))
      pc->sides_2sided_short = _cupsStrAlloc(value);
    else if (!_cups_strcasecmp(line, "Finishings"))
    {
      if (!pc->finishings)
	pc->finishings =
	    cupsArrayNew3((cups_array_func_t)pwg_compare_finishings,
			  NULL, NULL, 0, NULL,
			  (cups_afree_func_t)pwg_free_finishings);

      if ((finishings = calloc(1, sizeof(_pwg_finishings_t))) == NULL)
        goto create_error;

      finishings->value       = strtol(value, &valueptr, 10);
      finishings->num_options = cupsParseOptions(valueptr, 0,
                                                 &(finishings->options));

      cupsArrayAdd(pc->finishings, finishings);
    }
    else if (!_cups_strcasecmp(line, "MaxCopies"))
      pc->max_copies = atoi(value);
    else if (!_cups_strcasecmp(line, "ChargeInfoURI"))
      pc->charge_info_uri = _cupsStrAlloc(value);
    else if (!_cups_strcasecmp(line, "JobAccountId"))
      pc->account_id = !_cups_strcasecmp(value, "true");
    else if (!_cups_strcasecmp(line, "JobAccountingUserId"))
      pc->accounting_user_id = !_cups_strcasecmp(value, "true");
    else if (!_cups_strcasecmp(line, "JobPassword"))
      pc->password = _cupsStrAlloc(value);
    else if (!_cups_strcasecmp(line, "Mandatory"))
    {
      if (pc->mandatory)
        _cupsArrayAddStrings(pc->mandatory, value, ' ');
      else
        pc->mandatory = _cupsArrayNewStrings(value, ' ');
    }
    else if (!_cups_strcasecmp(line, "SupportFile"))
    {
      if (!pc->support_files)
        pc->support_files = cupsArrayNew3(NULL, NULL, NULL, 0,
                                          (cups_acopy_func_t)_cupsStrAlloc,
                                          (cups_afree_func_t)_cupsStrFree);

      cupsArrayAdd(pc->support_files, value);
    }
    else
    {
      DEBUG_printf(("_ppdCacheCreateWithFile: Unknown %s on line %d.", line,
		    linenum));
    }
  }

  if (pc->num_sizes < num_sizes)
  {
    DEBUG_printf(("_ppdCacheCreateWithFile: Not enough sizes (%d < %d).",
                  pc->num_sizes, num_sizes));
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
    goto create_error;
  }

  if (pc->num_sources < num_sources)
  {
    DEBUG_printf(("_ppdCacheCreateWithFile: Not enough sources (%d < %d).",
                  pc->num_sources, num_sources));
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
    goto create_error;
  }

  if (pc->num_types < num_types)
  {
    DEBUG_printf(("_ppdCacheCreateWithFile: Not enough types (%d < %d).",
                  pc->num_types, num_types));
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad PPD cache file."), 1);
    goto create_error;
  }

  cupsFileClose(fp);

  return (pc);

 /*
  * If we get here the file was bad - free any data and return...
  */

  create_error:

  cupsFileClose(fp);
  _ppdCacheDestroy(pc);

  if (attrs)
  {
    ippDelete(*attrs);
    *attrs = NULL;
  }

  return (NULL);
}


/*
 * '_ppdCacheCreateWithPPD()' - Create PWG mapping data from a PPD file.
 */

_ppd_cache_t *				/* O - PPD cache and mapping data */
_ppdCacheCreateWithPPD(ppd_file_t *ppd)	/* I - PPD file */
{
  int			i, j, k;	/* Looping vars */
  _ppd_cache_t		*pc;		/* PWG mapping data */
  ppd_option_t		*input_slot,	/* InputSlot option */
			*media_type,	/* MediaType option */
			*output_bin,	/* OutputBin option */
			*color_model,	/* ColorModel option */
			*duplex;	/* Duplex option */
  ppd_choice_t		*choice;	/* Current InputSlot/MediaType */
  pwg_map_t		*map;		/* Current source/type map */
  ppd_attr_t		*ppd_attr;	/* Current PPD preset attribute */
  int			num_options;	/* Number of preset options and props */
  cups_option_t		*options;	/* Preset options and properties */
  ppd_size_t		*ppd_size;	/* Current PPD size */
  pwg_size_t		*pwg_size;	/* Current PWG size */
  char			pwg_keyword[3 + PPD_MAX_NAME + 1 + 12 + 1 + 12 + 3],
					/* PWG keyword string */
			ppd_name[PPD_MAX_NAME];
					/* Normalized PPD name */
  const char		*pwg_name;	/* Standard PWG media name */
  pwg_media_t		*pwg_media;	/* PWG media data */
  _pwg_print_color_mode_t pwg_print_color_mode;
					/* print-color-mode index */
  _pwg_print_quality_t	pwg_print_quality;
					/* print-quality index */
  int			similar;	/* Are the old and new size similar? */
  pwg_size_t		*old_size;	/* Current old size */
  int			old_imageable,	/* Old imageable length in 2540ths */
			old_borderless,	/* Old borderless state */
			old_known_pwg;	/* Old PWG name is well-known */
  int			new_width,	/* New width in 2540ths */
			new_length,	/* New length in 2540ths */
			new_left,	/* New left margin in 2540ths */
			new_bottom,	/* New bottom margin in 2540ths */
			new_right,	/* New right margin in 2540ths */
			new_top,	/* New top margin in 2540ths */
			new_imageable,	/* New imageable length in 2540ths */
			new_borderless,	/* New borderless state */
			new_known_pwg;	/* New PWG name is well-known */
  pwg_size_t		*new_size;	/* New size to add, if any */
  const char		*filter;	/* Current filter */
  _pwg_finishings_t	*finishings;	/* Current finishings value */


  DEBUG_printf(("_ppdCacheCreateWithPPD(ppd=%p)", ppd));

 /*
  * Range check input...
  */

  if (!ppd)
    return (NULL);

 /*
  * Allocate memory...
  */

  if ((pc = calloc(1, sizeof(_ppd_cache_t))) == NULL)
  {
    DEBUG_puts("_ppdCacheCreateWithPPD: Unable to allocate _ppd_cache_t.");
    goto create_error;
  }

 /*
  * Copy and convert size data...
  */

  if (ppd->num_sizes > 0)
  {
    if ((pc->sizes = calloc(ppd->num_sizes, sizeof(pwg_size_t))) == NULL)
    {
      DEBUG_printf(("_ppdCacheCreateWithPPD: Unable to allocate %d "
		    "pwg_size_t's.", ppd->num_sizes));
      goto create_error;
    }

    for (i = ppd->num_sizes, pwg_size = pc->sizes, ppd_size = ppd->sizes;
	 i > 0;
	 i --, ppd_size ++)
    {
     /*
      * Don't copy over custom size...
      */

      if (!_cups_strcasecmp(ppd_size->name, "Custom"))
	continue;

     /*
      * Convert the PPD size name to the corresponding PWG keyword name.
      */

      if ((pwg_media = pwgMediaForPPD(ppd_size->name)) != NULL)
      {
       /*
	* Standard name, do we have conflicts?
	*/

	for (j = 0; j < pc->num_sizes; j ++)
	  if (!strcmp(pc->sizes[j].map.pwg, pwg_media->pwg))
	  {
	    pwg_media = NULL;
	    break;
	  }
      }

      if (pwg_media)
      {
       /*
	* Standard name and no conflicts, use it!
	*/

	pwg_name      = pwg_media->pwg;
	new_known_pwg = 1;
      }
      else
      {
       /*
	* Not a standard name; convert it to a PWG vendor name of the form:
	*
	*     pp_lowerppd_WIDTHxHEIGHTuu
	*/

	pwg_name      = pwg_keyword;
	new_known_pwg = 0;

	pwg_unppdize_name(ppd_size->name, ppd_name, sizeof(ppd_name), "_.");
	pwgFormatSizeName(pwg_keyword, sizeof(pwg_keyword), NULL, ppd_name,
			  PWG_FROM_POINTS(ppd_size->width),
			  PWG_FROM_POINTS(ppd_size->length), NULL);
      }

     /*
      * If we have a similar paper with non-zero margins then we only want to
      * keep it if it has a larger imageable area length.  The NULL check is for
      * dimensions that are <= 0...
      */

      if ((pwg_media = pwgMediaForSize(PWG_FROM_POINTS(ppd_size->width),
					PWG_FROM_POINTS(ppd_size->length))) == NULL)
	continue;

      new_width      = pwg_media->width;
      new_length     = pwg_media->length;
      new_left       = PWG_FROM_POINTS(ppd_size->left);
      new_bottom     = PWG_FROM_POINTS(ppd_size->bottom);
      new_right      = PWG_FROM_POINTS(ppd_size->width - ppd_size->right);
      new_top        = PWG_FROM_POINTS(ppd_size->length - ppd_size->top);
      new_imageable  = new_length - new_top - new_bottom;
      new_borderless = new_bottom == 0 && new_top == 0 &&
		       new_left == 0 && new_right == 0;

      for (k = pc->num_sizes, similar = 0, old_size = pc->sizes, new_size = NULL;
	   k > 0 && !similar;
	   k --, old_size ++)
      {
	old_imageable  = old_size->length - old_size->top - old_size->bottom;
	old_borderless = old_size->left == 0 && old_size->bottom == 0 &&
			 old_size->right == 0 && old_size->top == 0;
	old_known_pwg  = strncmp(old_size->map.pwg, "oe_", 3) &&
			 strncmp(old_size->map.pwg, "om_", 3);

	similar = old_borderless == new_borderless &&
		  _PWG_EQUIVALENT(old_size->width, new_width) &&
		  _PWG_EQUIVALENT(old_size->length, new_length);

	if (similar &&
	    (new_known_pwg || (!old_known_pwg && new_imageable > old_imageable)))
	{
	 /*
	  * The new paper has a larger imageable area so it could replace
	  * the older paper.  Regardless of the imageable area, we always
	  * prefer the size with a well-known PWG name.
	  */

	  new_size = old_size;
	  _cupsStrFree(old_size->map.ppd);
	  _cupsStrFree(old_size->map.pwg);
	}
      }

      if (!similar)
      {
       /*
	* The paper was unique enough to deserve its own entry so add it to the
	* end.
	*/

	new_size = pwg_size ++;
	pc->num_sizes ++;
      }

      if (new_size)
      {
       /*
	* Save this size...
	*/

	new_size->map.ppd = _cupsStrAlloc(ppd_size->name);
	new_size->map.pwg = _cupsStrAlloc(pwg_name);
	new_size->width   = new_width;
	new_size->length  = new_length;
	new_size->left    = new_left;
	new_size->bottom  = new_bottom;
	new_size->right   = new_right;
	new_size->top     = new_top;
      }
    }
  }

  if (ppd->variable_sizes)
  {
   /*
    * Generate custom size data...
    */

    pwgFormatSizeName(pwg_keyword, sizeof(pwg_keyword), "custom", "max",
		      PWG_FROM_POINTS(ppd->custom_max[0]),
		      PWG_FROM_POINTS(ppd->custom_max[1]), NULL);
    pc->custom_max_keyword = _cupsStrAlloc(pwg_keyword);
    pc->custom_max_width   = PWG_FROM_POINTS(ppd->custom_max[0]);
    pc->custom_max_length  = PWG_FROM_POINTS(ppd->custom_max[1]);

    pwgFormatSizeName(pwg_keyword, sizeof(pwg_keyword), "custom", "min",
		      PWG_FROM_POINTS(ppd->custom_min[0]),
		      PWG_FROM_POINTS(ppd->custom_min[1]), NULL);
    pc->custom_min_keyword = _cupsStrAlloc(pwg_keyword);
    pc->custom_min_width   = PWG_FROM_POINTS(ppd->custom_min[0]);
    pc->custom_min_length  = PWG_FROM_POINTS(ppd->custom_min[1]);

    pc->custom_size.left   = PWG_FROM_POINTS(ppd->custom_margins[0]);
    pc->custom_size.bottom = PWG_FROM_POINTS(ppd->custom_margins[1]);
    pc->custom_size.right  = PWG_FROM_POINTS(ppd->custom_margins[2]);
    pc->custom_size.top    = PWG_FROM_POINTS(ppd->custom_margins[3]);
  }

 /*
  * Copy and convert InputSlot data...
  */

  if ((input_slot = ppdFindOption(ppd, "InputSlot")) == NULL)
    input_slot = ppdFindOption(ppd, "HPPaperSource");

  if (input_slot)
  {
    pc->source_option = _cupsStrAlloc(input_slot->keyword);

    if ((pc->sources = calloc(input_slot->num_choices,
                               sizeof(pwg_map_t))) == NULL)
    {
      DEBUG_printf(("_ppdCacheCreateWithPPD: Unable to allocate %d "
                    "pwg_map_t's for InputSlot.", input_slot->num_choices));
      goto create_error;
    }

    pc->num_sources = input_slot->num_choices;

    for (i = input_slot->num_choices, choice = input_slot->choices,
             map = pc->sources;
	 i > 0;
	 i --, choice ++, map ++)
    {
      if (!_cups_strncasecmp(choice->choice, "Auto", 4) ||
          !_cups_strcasecmp(choice->choice, "Default"))
        pwg_name = "auto";
      else if (!_cups_strcasecmp(choice->choice, "Cassette"))
        pwg_name = "main";
      else if (!_cups_strcasecmp(choice->choice, "PhotoTray"))
        pwg_name = "photo";
      else if (!_cups_strcasecmp(choice->choice, "CDTray"))
        pwg_name = "disc";
      else if (!_cups_strncasecmp(choice->choice, "Multipurpose", 12) ||
               !_cups_strcasecmp(choice->choice, "MP") ||
               !_cups_strcasecmp(choice->choice, "MPTray"))
        pwg_name = "by-pass-tray";
      else if (!_cups_strcasecmp(choice->choice, "LargeCapacity"))
        pwg_name = "large-capacity";
      else if (!_cups_strncasecmp(choice->choice, "Lower", 5))
        pwg_name = "bottom";
      else if (!_cups_strncasecmp(choice->choice, "Middle", 6))
        pwg_name = "middle";
      else if (!_cups_strncasecmp(choice->choice, "Upper", 5))
        pwg_name = "top";
      else if (!_cups_strncasecmp(choice->choice, "Side", 4))
        pwg_name = "side";
      else if (!_cups_strcasecmp(choice->choice, "Roll"))
        pwg_name = "main-roll";
      else
      {
       /*
        * Convert PPD name to lowercase...
	*/

        pwg_name = pwg_keyword;
	pwg_unppdize_name(choice->choice, pwg_keyword, sizeof(pwg_keyword),
	                  "_");
      }

      map->pwg = _cupsStrAlloc(pwg_name);
      map->ppd = _cupsStrAlloc(choice->choice);
    }
  }

 /*
  * Copy and convert MediaType data...
  */

  if ((media_type = ppdFindOption(ppd, "MediaType")) != NULL)
  {
    if ((pc->types = calloc(media_type->num_choices,
                             sizeof(pwg_map_t))) == NULL)
    {
      DEBUG_printf(("_ppdCacheCreateWithPPD: Unable to allocate %d "
                    "pwg_map_t's for MediaType.", media_type->num_choices));
      goto create_error;
    }

    pc->num_types = media_type->num_choices;

    for (i = media_type->num_choices, choice = media_type->choices,
             map = pc->types;
	 i > 0;
	 i --, choice ++, map ++)
    {
      if (!_cups_strncasecmp(choice->choice, "Auto", 4) ||
          !_cups_strcasecmp(choice->choice, "Any") ||
          !_cups_strcasecmp(choice->choice, "Default"))
        pwg_name = "auto";
      else if (!_cups_strncasecmp(choice->choice, "Card", 4))
        pwg_name = "cardstock";
      else if (!_cups_strncasecmp(choice->choice, "Env", 3))
        pwg_name = "envelope";
      else if (!_cups_strncasecmp(choice->choice, "Gloss", 5))
        pwg_name = "photographic-glossy";
      else if (!_cups_strcasecmp(choice->choice, "HighGloss"))
        pwg_name = "photographic-high-gloss";
      else if (!_cups_strcasecmp(choice->choice, "Matte"))
        pwg_name = "photographic-matte";
      else if (!_cups_strncasecmp(choice->choice, "Plain", 5))
        pwg_name = "stationery";
      else if (!_cups_strncasecmp(choice->choice, "Coated", 6))
        pwg_name = "stationery-coated";
      else if (!_cups_strcasecmp(choice->choice, "Inkjet"))
        pwg_name = "stationery-inkjet";
      else if (!_cups_strcasecmp(choice->choice, "Letterhead"))
        pwg_name = "stationery-letterhead";
      else if (!_cups_strncasecmp(choice->choice, "Preprint", 8))
        pwg_name = "stationery-preprinted";
      else if (!_cups_strcasecmp(choice->choice, "Recycled"))
        pwg_name = "stationery-recycled";
      else if (!_cups_strncasecmp(choice->choice, "Transparen", 10))
        pwg_name = "transparency";
      else
      {
       /*
        * Convert PPD name to lowercase...
	*/

        pwg_name = pwg_keyword;
	pwg_unppdize_name(choice->choice, pwg_keyword, sizeof(pwg_keyword),
	                  "_");
      }

      map->pwg = _cupsStrAlloc(pwg_name);
      map->ppd = _cupsStrAlloc(choice->choice);
    }
  }

 /*
  * Copy and convert OutputBin data...
  */

  if ((output_bin = ppdFindOption(ppd, "OutputBin")) != NULL)
  {
    if ((pc->bins = calloc(output_bin->num_choices,
                           sizeof(pwg_map_t))) == NULL)
    {
      DEBUG_printf(("_ppdCacheCreateWithPPD: Unable to allocate %d "
                    "pwg_map_t's for OutputBin.", output_bin->num_choices));
      goto create_error;
    }

    pc->num_bins = output_bin->num_choices;

    for (i = output_bin->num_choices, choice = output_bin->choices,
             map = pc->bins;
	 i > 0;
	 i --, choice ++, map ++)
    {
      pwg_unppdize_name(choice->choice, pwg_keyword, sizeof(pwg_keyword), "_");

      map->pwg = _cupsStrAlloc(pwg_keyword);
      map->ppd = _cupsStrAlloc(choice->choice);
    }
  }

  if ((ppd_attr = ppdFindAttr(ppd, "APPrinterPreset", NULL)) != NULL)
  {
   /*
    * Copy and convert APPrinterPreset (output-mode + print-quality) data...
    */

    const char	*quality,		/* com.apple.print.preset.quality value */
		*output_mode,		/* com.apple.print.preset.output-mode value */
		*color_model_val,	/* ColorModel choice */
		*graphicsType,		/* com.apple.print.preset.graphicsType value */
		*media_front_coating;	/* com.apple.print.preset.media-front-coating value */

    do
    {
      num_options = _ppdParseOptions(ppd_attr->value, 0, &options,
                                     _PPD_PARSE_ALL);

      if ((quality = cupsGetOption("com.apple.print.preset.quality",
                                   num_options, options)) != NULL)
      {
       /*
        * Get the print-quality for this preset...
	*/

	if (!strcmp(quality, "low"))
	  pwg_print_quality = _PWG_PRINT_QUALITY_DRAFT;
	else if (!strcmp(quality, "high"))
	  pwg_print_quality = _PWG_PRINT_QUALITY_HIGH;
	else
	  pwg_print_quality = _PWG_PRINT_QUALITY_NORMAL;

       /*
	* Ignore graphicsType "Photo" presets that are not high quality.
	*/

	graphicsType = cupsGetOption("com.apple.print.preset.graphicsType",
				      num_options, options);

	if (pwg_print_quality != _PWG_PRINT_QUALITY_HIGH && graphicsType &&
	    !strcmp(graphicsType, "Photo"))
	  continue;

       /*
	* Ignore presets for normal and draft quality where the coating
	* isn't "none" or "autodetect".
	*/

	media_front_coating = cupsGetOption(
	                          "com.apple.print.preset.media-front-coating",
			          num_options, options);

        if (pwg_print_quality != _PWG_PRINT_QUALITY_HIGH &&
	    media_front_coating &&
	    strcmp(media_front_coating, "none") &&
	    strcmp(media_front_coating, "autodetect"))
	  continue;

       /*
        * Get the output mode for this preset...
	*/

        output_mode     = cupsGetOption("com.apple.print.preset.output-mode",
	                                num_options, options);
        color_model_val = cupsGetOption("ColorModel", num_options, options);

        if (output_mode)
	{
	  if (!strcmp(output_mode, "monochrome"))
	    pwg_print_color_mode = _PWG_PRINT_COLOR_MODE_MONOCHROME;
	  else
	    pwg_print_color_mode = _PWG_PRINT_COLOR_MODE_COLOR;
	}
	else if (color_model_val)
	{
	  if (!_cups_strcasecmp(color_model_val, "Gray"))
	    pwg_print_color_mode = _PWG_PRINT_COLOR_MODE_MONOCHROME;
	  else
	    pwg_print_color_mode = _PWG_PRINT_COLOR_MODE_COLOR;
	}
	else
	  pwg_print_color_mode = _PWG_PRINT_COLOR_MODE_COLOR;

       /*
        * Save the options for this combination as needed...
	*/

        if (!pc->num_presets[pwg_print_color_mode][pwg_print_quality])
	  pc->num_presets[pwg_print_color_mode][pwg_print_quality] =
	      _ppdParseOptions(ppd_attr->value, 0,
	                       pc->presets[pwg_print_color_mode] +
			           pwg_print_quality, _PPD_PARSE_OPTIONS);
      }

      cupsFreeOptions(num_options, options);
    }
    while ((ppd_attr = ppdFindNextAttr(ppd, "APPrinterPreset", NULL)) != NULL);
  }

  if (!pc->num_presets[_PWG_PRINT_COLOR_MODE_MONOCHROME][_PWG_PRINT_QUALITY_DRAFT] &&
      !pc->num_presets[_PWG_PRINT_COLOR_MODE_MONOCHROME][_PWG_PRINT_QUALITY_NORMAL] &&
      !pc->num_presets[_PWG_PRINT_COLOR_MODE_MONOCHROME][_PWG_PRINT_QUALITY_HIGH])
  {
   /*
    * Try adding some common color options to create grayscale presets.  These
    * are listed in order of popularity...
    */

    const char	*color_option = NULL,	/* Color control option */
		*gray_choice = NULL;	/* Choice to select grayscale */

    if ((color_model = ppdFindOption(ppd, "ColorModel")) != NULL &&
        ppdFindChoice(color_model, "Gray"))
    {
      color_option = "ColorModel";
      gray_choice  = "Gray";
    }
    else if ((color_model = ppdFindOption(ppd, "HPColorMode")) != NULL &&
             ppdFindChoice(color_model, "grayscale"))
    {
      color_option = "HPColorMode";
      gray_choice  = "grayscale";
    }
    else if ((color_model = ppdFindOption(ppd, "BRMonoColor")) != NULL &&
             ppdFindChoice(color_model, "Mono"))
    {
      color_option = "BRMonoColor";
      gray_choice  = "Mono";
    }
    else if ((color_model = ppdFindOption(ppd, "CNIJSGrayScale")) != NULL &&
             ppdFindChoice(color_model, "1"))
    {
      color_option = "CNIJSGrayScale";
      gray_choice  = "1";
    }
    else if ((color_model = ppdFindOption(ppd, "HPColorAsGray")) != NULL &&
             ppdFindChoice(color_model, "True"))
    {
      color_option = "HPColorAsGray";
      gray_choice  = "True";
    }

    if (color_option && gray_choice)
    {
     /*
      * Copy and convert ColorModel (output-mode) data...
      */

      cups_option_t	*coption,	/* Color option */
			  *moption;	/* Monochrome option */

      for (pwg_print_quality = _PWG_PRINT_QUALITY_DRAFT;
	   pwg_print_quality < _PWG_PRINT_QUALITY_MAX;
	   pwg_print_quality ++)
      {
	if (pc->num_presets[_PWG_PRINT_COLOR_MODE_COLOR][pwg_print_quality])
	{
	 /*
	  * Copy the color options...
	  */

	  num_options = pc->num_presets[_PWG_PRINT_COLOR_MODE_COLOR]
					[pwg_print_quality];
	  options     = calloc(sizeof(cups_option_t), num_options);

	  if (options)
	  {
	    for (i = num_options, moption = options,
		     coption = pc->presets[_PWG_PRINT_COLOR_MODE_COLOR]
					   [pwg_print_quality];
		 i > 0;
		 i --, moption ++, coption ++)
	    {
	      moption->name  = _cupsStrRetain(coption->name);
	      moption->value = _cupsStrRetain(coption->value);
	    }

	    pc->num_presets[_PWG_PRINT_COLOR_MODE_MONOCHROME][pwg_print_quality] =
		num_options;
	    pc->presets[_PWG_PRINT_COLOR_MODE_MONOCHROME][pwg_print_quality] =
		options;
	  }
	}
	else if (pwg_print_quality != _PWG_PRINT_QUALITY_NORMAL)
	  continue;

       /*
	* Add the grayscale option to the preset...
	*/

	pc->num_presets[_PWG_PRINT_COLOR_MODE_MONOCHROME][pwg_print_quality] =
	    cupsAddOption(color_option, gray_choice,
			  pc->num_presets[_PWG_PRINT_COLOR_MODE_MONOCHROME]
					  [pwg_print_quality],
			  pc->presets[_PWG_PRINT_COLOR_MODE_MONOCHROME] +
			      pwg_print_quality);
      }
    }
  }

 /*
  * Copy and convert Duplex (sides) data...
  */

  if ((duplex = ppdFindOption(ppd, "Duplex")) == NULL)
    if ((duplex = ppdFindOption(ppd, "JCLDuplex")) == NULL)
      if ((duplex = ppdFindOption(ppd, "EFDuplex")) == NULL)
        if ((duplex = ppdFindOption(ppd, "EFDuplexing")) == NULL)
	  duplex = ppdFindOption(ppd, "KD03Duplex");

  if (duplex)
  {
    pc->sides_option = _cupsStrAlloc(duplex->keyword);

    for (i = duplex->num_choices, choice = duplex->choices;
         i > 0;
	 i --, choice ++)
    {
      if ((!_cups_strcasecmp(choice->choice, "None") ||
	   !_cups_strcasecmp(choice->choice, "False")) && !pc->sides_1sided)
        pc->sides_1sided = _cupsStrAlloc(choice->choice);
      else if ((!_cups_strcasecmp(choice->choice, "DuplexNoTumble") ||
	        !_cups_strcasecmp(choice->choice, "LongEdge") ||
	        !_cups_strcasecmp(choice->choice, "Top")) && !pc->sides_2sided_long)
        pc->sides_2sided_long = _cupsStrAlloc(choice->choice);
      else if ((!_cups_strcasecmp(choice->choice, "DuplexTumble") ||
	        !_cups_strcasecmp(choice->choice, "ShortEdge") ||
	        !_cups_strcasecmp(choice->choice, "Bottom")) &&
	       !pc->sides_2sided_short)
        pc->sides_2sided_short = _cupsStrAlloc(choice->choice);
    }
  }

 /*
  * Copy filters and pre-filters...
  */

  pc->filters = cupsArrayNew3(NULL, NULL, NULL, 0,
			      (cups_acopy_func_t)_cupsStrAlloc,
			      (cups_afree_func_t)_cupsStrFree);

  cupsArrayAdd(pc->filters,
               "application/vnd.cups-raw application/octet-stream 0 -");

  if ((ppd_attr = ppdFindAttr(ppd, "cupsFilter2", NULL)) != NULL)
  {
    do
    {
      cupsArrayAdd(pc->filters, ppd_attr->value);
    }
    while ((ppd_attr = ppdFindNextAttr(ppd, "cupsFilter2", NULL)) != NULL);
  }
  else if (ppd->num_filters > 0)
  {
    for (i = 0; i < ppd->num_filters; i ++)
      cupsArrayAdd(pc->filters, ppd->filters[i]);
  }
  else
    cupsArrayAdd(pc->filters, "application/vnd.cups-postscript 0 -");

 /*
  * See if we have a command filter...
  */

  for (filter = (const char *)cupsArrayFirst(pc->filters);
       filter;
       filter = (const char *)cupsArrayNext(pc->filters))
    if (!_cups_strncasecmp(filter, "application/vnd.cups-command", 28) &&
        _cups_isspace(filter[28]))
      break;

  if (!filter &&
      ((ppd_attr = ppdFindAttr(ppd, "cupsCommands", NULL)) == NULL ||
       _cups_strcasecmp(ppd_attr->value, "none")))
  {
   /*
    * No command filter and no cupsCommands keyword telling us not to use one.
    * See if this is a PostScript printer, and if so add a PostScript command
    * filter...
    */

    for (filter = (const char *)cupsArrayFirst(pc->filters);
	 filter;
	 filter = (const char *)cupsArrayNext(pc->filters))
      if (!_cups_strncasecmp(filter, "application/vnd.cups-postscript", 31) &&
	  _cups_isspace(filter[31]))
	break;

    if (filter)
      cupsArrayAdd(pc->filters,
                   "application/vnd.cups-command application/postscript 100 "
                   "commandtops");
  }

  if ((ppd_attr = ppdFindAttr(ppd, "cupsPreFilter", NULL)) != NULL)
  {
    pc->prefilters = cupsArrayNew3(NULL, NULL, NULL, 0,
				   (cups_acopy_func_t)_cupsStrAlloc,
				   (cups_afree_func_t)_cupsStrFree);

    do
    {
      cupsArrayAdd(pc->prefilters, ppd_attr->value);
    }
    while ((ppd_attr = ppdFindNextAttr(ppd, "cupsPreFilter", NULL)) != NULL);
  }

  if ((ppd_attr = ppdFindAttr(ppd, "cupsSingleFile", NULL)) != NULL)
    pc->single_file = !_cups_strcasecmp(ppd_attr->value, "true");

 /*
  * Copy the product string, if any...
  */

  if (ppd->product)
    pc->product = _cupsStrAlloc(ppd->product);

 /*
  * Copy finishings mapping data...
  */

  if ((ppd_attr = ppdFindAttr(ppd, "cupsIPPFinishings", NULL)) != NULL)
  {
    pc->finishings = cupsArrayNew3((cups_array_func_t)pwg_compare_finishings,
                                   NULL, NULL, 0, NULL,
                                   (cups_afree_func_t)pwg_free_finishings);

    do
    {
      if ((finishings = calloc(1, sizeof(_pwg_finishings_t))) == NULL)
        goto create_error;

      finishings->value       = atoi(ppd_attr->spec);
      finishings->num_options = _ppdParseOptions(ppd_attr->value, 0,
                                                 &(finishings->options),
                                                 _PPD_PARSE_OPTIONS);

      cupsArrayAdd(pc->finishings, finishings);
    }
    while ((ppd_attr = ppdFindNextAttr(ppd, "cupsIPPFinishings",
                                       NULL)) != NULL);
  }

 /*
  * Max copies...
  */

  if ((ppd_attr = ppdFindAttr(ppd, "cupsMaxCopies", NULL)) != NULL)
    pc->max_copies = atoi(ppd_attr->value);
  else if (ppd->manual_copies)
    pc->max_copies = 1;
  else
    pc->max_copies = 9999;

 /*
  * cupsChargeInfoURI, cupsJobAccountId, cupsJobAccountingUserId,
  * cupsJobPassword, and cupsMandatory.
  */

  if ((ppd_attr = ppdFindAttr(ppd, "cupsChargeInfoURI", NULL)) != NULL)
    pc->charge_info_uri = _cupsStrAlloc(ppd_attr->value);

  if ((ppd_attr = ppdFindAttr(ppd, "cupsJobAccountId", NULL)) != NULL)
    pc->account_id = !_cups_strcasecmp(ppd_attr->value, "true");

  if ((ppd_attr = ppdFindAttr(ppd, "cupsJobAccountingUserId", NULL)) != NULL)
    pc->accounting_user_id = !_cups_strcasecmp(ppd_attr->value, "true");

  if ((ppd_attr = ppdFindAttr(ppd, "cupsJobPassword", NULL)) != NULL)
    pc->password = _cupsStrAlloc(ppd_attr->value);

  if ((ppd_attr = ppdFindAttr(ppd, "cupsMandatory", NULL)) != NULL)
    pc->mandatory = _cupsArrayNewStrings(ppd_attr->value, ' ');

 /*
  * Support files...
  */

  pc->support_files = cupsArrayNew3(NULL, NULL, NULL, 0,
				    (cups_acopy_func_t)_cupsStrAlloc,
				    (cups_afree_func_t)_cupsStrFree);

  for (ppd_attr = ppdFindAttr(ppd, "cupsICCProfile", NULL);
       ppd_attr;
       ppd_attr = ppdFindNextAttr(ppd, "cupsICCProfile", NULL))
    cupsArrayAdd(pc->support_files, ppd_attr->value);

  if ((ppd_attr = ppdFindAttr(ppd, "APPrinterIconPath", NULL)) != NULL)
    cupsArrayAdd(pc->support_files, ppd_attr->value);

 /*
  * Return the cache data...
  */

  return (pc);

 /*
  * If we get here we need to destroy the PWG mapping data and return NULL...
  */

  create_error:

  _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Out of memory."), 1);
  _ppdCacheDestroy(pc);

  return (NULL);
}


/*
 * '_ppdCacheDestroy()' - Free all memory used for PWG mapping data.
 */

void
_ppdCacheDestroy(_ppd_cache_t *pc)	/* I - PPD cache and mapping data */
{
  int		i;			/* Looping var */
  pwg_map_t	*map;			/* Current map */
  pwg_size_t	*size;			/* Current size */


 /*
  * Range check input...
  */

  if (!pc)
    return;

 /*
  * Free memory as needed...
  */

  if (pc->bins)
  {
    for (i = pc->num_bins, map = pc->bins; i > 0; i --, map ++)
    {
      _cupsStrFree(map->pwg);
      _cupsStrFree(map->ppd);
    }

    free(pc->bins);
  }

  if (pc->sizes)
  {
    for (i = pc->num_sizes, size = pc->sizes; i > 0; i --, size ++)
    {
      _cupsStrFree(size->map.pwg);
      _cupsStrFree(size->map.ppd);
    }

    free(pc->sizes);
  }

  if (pc->source_option)
    _cupsStrFree(pc->source_option);

  if (pc->sources)
  {
    for (i = pc->num_sources, map = pc->sources; i > 0; i --, map ++)
    {
      _cupsStrFree(map->pwg);
      _cupsStrFree(map->ppd);
    }

    free(pc->sources);
  }

  if (pc->types)
  {
    for (i = pc->num_types, map = pc->types; i > 0; i --, map ++)
    {
      _cupsStrFree(map->pwg);
      _cupsStrFree(map->ppd);
    }

    free(pc->types);
  }

  if (pc->custom_max_keyword)
    _cupsStrFree(pc->custom_max_keyword);

  if (pc->custom_min_keyword)
    _cupsStrFree(pc->custom_min_keyword);

  _cupsStrFree(pc->product);
  cupsArrayDelete(pc->filters);
  cupsArrayDelete(pc->prefilters);
  cupsArrayDelete(pc->finishings);

  _cupsStrFree(pc->charge_info_uri);
  _cupsStrFree(pc->password);

  cupsArrayDelete(pc->mandatory);

  cupsArrayDelete(pc->support_files);

  free(pc);
}


/*
 * '_ppdCacheGetBin()' - Get the PWG output-bin keyword associated with a PPD
 *                  OutputBin.
 */

const char *				/* O - output-bin or NULL */
_ppdCacheGetBin(
    _ppd_cache_t *pc,			/* I - PPD cache and mapping data */
    const char   *output_bin)		/* I - PPD OutputBin string */
{
  int	i;				/* Looping var */


 /*
  * Range check input...
  */

  if (!pc || !output_bin)
    return (NULL);

 /*
  * Look up the OutputBin string...
  */


  for (i = 0; i < pc->num_bins; i ++)
    if (!_cups_strcasecmp(output_bin, pc->bins[i].ppd))
      return (pc->bins[i].pwg);

  return (NULL);
}


/*
 * '_ppdCacheGetFinishingOptions()' - Get PPD finishing options for the given
 *                                    IPP finishings value(s).
 */

int					/* O  - New number of options */
_ppdCacheGetFinishingOptions(
    _ppd_cache_t     *pc,		/* I  - PPD cache and mapping data */
    ipp_t            *job,		/* I  - Job attributes or NULL */
    ipp_finishings_t value,		/* I  - IPP finishings value of IPP_FINISHINGS_NONE */
    int              num_options,	/* I  - Number of options */
    cups_option_t    **options)		/* IO - Options */
{
  int			i;		/* Looping var */
  _pwg_finishings_t	*f,		/* PWG finishings options */
			key;		/* Search key */
  ipp_attribute_t	*attr;		/* Finishings attribute */
  cups_option_t		*option;	/* Current finishings option */


 /*
  * Range check input...
  */

  if (!pc || cupsArrayCount(pc->finishings) == 0 || !options ||
      (!job && value == IPP_FINISHINGS_NONE))
    return (num_options);

 /*
  * Apply finishing options...
  */

  if (job && (attr = ippFindAttribute(job, "finishings", IPP_TAG_ENUM)) != NULL)
  {
    int	num_values = ippGetCount(attr);	/* Number of values */

    for (i = 0; i < num_values; i ++)
    {
      key.value = ippGetInteger(attr, i);

      if ((f = cupsArrayFind(pc->finishings, &key)) != NULL)
      {
        int	j;			/* Another looping var */

        for (j = f->num_options, option = f->options; j > 0; j --, option ++)
          num_options = cupsAddOption(option->name, option->value,
                                      num_options, options);
      }
    }
  }
  else if (value != IPP_FINISHINGS_NONE)
  {
    key.value = value;

    if ((f = cupsArrayFind(pc->finishings, &key)) != NULL)
    {
      int	j;			/* Another looping var */

      for (j = f->num_options, option = f->options; j > 0; j --, option ++)
	num_options = cupsAddOption(option->name, option->value,
				    num_options, options);
    }
  }

  return (num_options);
}


/*
 * '_ppdCacheGetFinishingValues()' - Get IPP finishings value(s) from the given
 *                                   PPD options.
 */

int					/* O - Number of finishings values */
_ppdCacheGetFinishingValues(
    _ppd_cache_t  *pc,			/* I - PPD cache and mapping data */
    int           num_options,		/* I - Number of options */
    cups_option_t *options,		/* I - Options */
    int           max_values,		/* I - Maximum number of finishings values */
    int           *values)		/* O - Finishings values */
{
  int			i,		/* Looping var */
			num_values = 0;	/* Number of values */
  _pwg_finishings_t	*f;		/* Current finishings option */
  cups_option_t		*option;	/* Current option */
  const char		*val;		/* Value for option */


 /*
  * Range check input...
  */

  if (!pc || !pc->finishings || num_options < 1 || max_values < 1 || !values)
    return (0);

 /*
  * Go through the finishings options and see what is set...
  */

  for (f = (_pwg_finishings_t *)cupsArrayFirst(pc->finishings);
       f;
       f = (_pwg_finishings_t *)cupsArrayNext(pc->finishings))
  {
    for (i = f->num_options, option = f->options; i > 0; i --, option ++)
      if ((val = cupsGetOption(option->name, num_options, options)) == NULL ||
          _cups_strcasecmp(option->value, val))
        break;

    if (i == 0)
    {
      values[num_values ++] = f->value;

      if (num_values >= max_values)
        break;
    }
  }

  return (num_values);
}


/*
 * '_ppdCacheGetInputSlot()' - Get the PPD InputSlot associated with the job
 *                        attributes or a keyword string.
 */

const char *				/* O - PPD InputSlot or NULL */
_ppdCacheGetInputSlot(
    _ppd_cache_t *pc,			/* I - PPD cache and mapping data */
    ipp_t        *job,			/* I - Job attributes or NULL */
    const char   *keyword)		/* I - Keyword string or NULL */
{
 /*
  * Range check input...
  */

  if (!pc || pc->num_sources == 0 || (!job && !keyword))
    return (NULL);

  if (job && !keyword)
  {
   /*
    * Lookup the media-col attribute and any media-source found there...
    */

    ipp_attribute_t	*media_col,	/* media-col attribute */
			*media_source;	/* media-source attribute */
    pwg_size_t		size;		/* Dimensional size */
    int			margins_set;	/* Were the margins set? */

    media_col = ippFindAttribute(job, "media-col", IPP_TAG_BEGIN_COLLECTION);
    if (media_col &&
        (media_source = ippFindAttribute(ippGetCollection(media_col, 0),
                                         "media-source",
	                                 IPP_TAG_KEYWORD)) != NULL)
    {
     /*
      * Use the media-source value from media-col...
      */

      keyword = ippGetString(media_source, 0, NULL);
    }
    else if (pwgInitSize(&size, job, &margins_set))
    {
     /*
      * For media <= 5x7, look for a photo tray...
      */

      if (size.width <= (5 * 2540) && size.length <= (7 * 2540))
        keyword = "photo";
    }
  }

  if (keyword)
  {
    int	i;				/* Looping var */

    for (i = 0; i < pc->num_sources; i ++)
      if (!_cups_strcasecmp(keyword, pc->sources[i].pwg))
        return (pc->sources[i].ppd);
  }

  return (NULL);
}


/*
 * '_ppdCacheGetMediaType()' - Get the PPD MediaType associated with the job
 *                        attributes or a keyword string.
 */

const char *				/* O - PPD MediaType or NULL */
_ppdCacheGetMediaType(
    _ppd_cache_t *pc,			/* I - PPD cache and mapping data */
    ipp_t        *job,			/* I - Job attributes or NULL */
    const char   *keyword)		/* I - Keyword string or NULL */
{
 /*
  * Range check input...
  */

  if (!pc || pc->num_types == 0 || (!job && !keyword))
    return (NULL);

  if (job && !keyword)
  {
   /*
    * Lookup the media-col attribute and any media-source found there...
    */

    ipp_attribute_t	*media_col,	/* media-col attribute */
			*media_type;	/* media-type attribute */

    media_col = ippFindAttribute(job, "media-col", IPP_TAG_BEGIN_COLLECTION);
    if (media_col)
    {
      if ((media_type = ippFindAttribute(media_col->values[0].collection,
                                         "media-type",
	                                 IPP_TAG_KEYWORD)) == NULL)
	media_type = ippFindAttribute(media_col->values[0].collection,
				      "media-type", IPP_TAG_NAME);

      if (media_type)
	keyword = media_type->values[0].string.text;
    }
  }

  if (keyword)
  {
    int	i;				/* Looping var */

    for (i = 0; i < pc->num_types; i ++)
      if (!_cups_strcasecmp(keyword, pc->types[i].pwg))
        return (pc->types[i].ppd);
  }

  return (NULL);
}


/*
 * '_ppdCacheGetOutputBin()' - Get the PPD OutputBin associated with the keyword
 *                        string.
 */

const char *				/* O - PPD OutputBin or NULL */
_ppdCacheGetOutputBin(
    _ppd_cache_t *pc,			/* I - PPD cache and mapping data */
    const char   *output_bin)		/* I - Keyword string */
{
  int	i;				/* Looping var */


 /*
  * Range check input...
  */

  if (!pc || !output_bin)
    return (NULL);

 /*
  * Look up the OutputBin string...
  */


  for (i = 0; i < pc->num_bins; i ++)
    if (!_cups_strcasecmp(output_bin, pc->bins[i].pwg))
      return (pc->bins[i].ppd);

  return (NULL);
}


/*
 * '_ppdCacheGetPageSize()' - Get the PPD PageSize associated with the job
 *                       attributes or a keyword string.
 */

const char *				/* O - PPD PageSize or NULL */
_ppdCacheGetPageSize(
    _ppd_cache_t *pc,			/* I - PPD cache and mapping data */
    ipp_t        *job,			/* I - Job attributes or NULL */
    const char   *keyword,		/* I - Keyword string or NULL */
    int          *exact)		/* O - 1 if exact match, 0 otherwise */
{
  int		i;			/* Looping var */
  pwg_size_t	*size,			/* Current size */
		*closest,		/* Closest size */
		jobsize;		/* Size data from job */
  int		margins_set,		/* Were the margins set? */
		dwidth,			/* Difference in width */
		dlength,		/* Difference in length */
		dleft,			/* Difference in left margins */
		dright,			/* Difference in right margins */
		dbottom,		/* Difference in bottom margins */
		dtop,			/* Difference in top margins */
		dmin,			/* Minimum difference */
		dclosest;		/* Closest difference */
  const char	*ppd_name;		/* PPD media name */


  DEBUG_printf(("_ppdCacheGetPageSize(pc=%p, job=%p, keyword=\"%s\", exact=%p)",
	        pc, job, keyword, exact));

 /*
  * Range check input...
  */

  if (!pc || (!job && !keyword))
    return (NULL);

  if (exact)
    *exact = 0;

  ppd_name = keyword;

  if (job)
  {
   /*
    * Try getting the PPD media name from the job attributes...
    */

    ipp_attribute_t	*attr;		/* Job attribute */

    if ((attr = ippFindAttribute(job, "PageSize", IPP_TAG_ZERO)) == NULL)
      if ((attr = ippFindAttribute(job, "PageRegion", IPP_TAG_ZERO)) == NULL)
        attr = ippFindAttribute(job, "media", IPP_TAG_ZERO);

#ifdef DEBUG
    if (attr)
      DEBUG_printf(("1_ppdCacheGetPageSize: Found attribute %s (%s)",
                    attr->name, ippTagString(attr->value_tag)));
    else
      DEBUG_puts("1_ppdCacheGetPageSize: Did not find media attribute.");
#endif /* DEBUG */

    if (attr && (attr->value_tag == IPP_TAG_NAME ||
                 attr->value_tag == IPP_TAG_KEYWORD))
      ppd_name = attr->values[0].string.text;
  }

  DEBUG_printf(("1_ppdCacheGetPageSize: ppd_name=\"%s\"", ppd_name));

  if (ppd_name)
  {
   /*
    * Try looking up the named PPD size first...
    */

    for (i = pc->num_sizes, size = pc->sizes; i > 0; i --, size ++)
    {
      DEBUG_printf(("2_ppdCacheGetPageSize: size[%d]=[\"%s\" \"%s\"]",
                    (int)(size - pc->sizes), size->map.pwg, size->map.ppd));

      if (!_cups_strcasecmp(ppd_name, size->map.ppd) ||
          !_cups_strcasecmp(ppd_name, size->map.pwg))
      {
	if (exact)
	  *exact = 1;

        DEBUG_printf(("1_ppdCacheGetPageSize: Returning \"%s\"", ppd_name));

        return (size->map.ppd);
      }
    }
  }

  if (job && !keyword)
  {
   /*
    * Get the size using media-col or media, with the preference being
    * media-col.
    */

    if (!pwgInitSize(&jobsize, job, &margins_set))
      return (NULL);
  }
  else
  {
   /*
    * Get the size using a media keyword...
    */

    pwg_media_t	*media;		/* Media definition */


    if ((media = pwgMediaForPWG(keyword)) == NULL)
      if ((media = pwgMediaForLegacy(keyword)) == NULL)
        if ((media = pwgMediaForPPD(keyword)) == NULL)
	  return (NULL);

    jobsize.width  = media->width;
    jobsize.length = media->length;
    margins_set    = 0;
  }

 /*
  * Now that we have the dimensions and possibly the margins, look at the
  * available sizes and find the match...
  */

  closest  = NULL;
  dclosest = 999999999;

  if (!ppd_name || _cups_strncasecmp(ppd_name, "Custom.", 7) ||
      _cups_strncasecmp(ppd_name, "custom_", 7))
  {
    for (i = pc->num_sizes, size = pc->sizes; i > 0; i --, size ++)
    {
     /*
      * Adobe uses a size matching algorithm with an epsilon of 5 points, which
      * is just about 176/2540ths...
      */

      dwidth  = size->width - jobsize.width;
      dlength = size->length - jobsize.length;

      if (dwidth <= -176 || dwidth >= 176 || dlength <= -176 || dlength >= 176)
	continue;

      if (margins_set)
      {
       /*
	* Use a tighter epsilon of 1 point (35/2540ths) for margins...
	*/

	dleft   = size->left - jobsize.left;
	dright  = size->right - jobsize.right;
	dtop    = size->top - jobsize.top;
	dbottom = size->bottom - jobsize.bottom;

	if (dleft <= -35 || dleft >= 35 || dright <= -35 || dright >= 35 ||
	    dtop <= -35 || dtop >= 35 || dbottom <= -35 || dbottom >= 35)
	{
	  dleft   = dleft < 0 ? -dleft : dleft;
	  dright  = dright < 0 ? -dright : dright;
	  dbottom = dbottom < 0 ? -dbottom : dbottom;
	  dtop    = dtop < 0 ? -dtop : dtop;
	  dmin    = dleft + dright + dbottom + dtop;

	  if (dmin < dclosest)
	  {
	    dclosest = dmin;
	    closest  = size;
	  }

	  continue;
	}
      }

      if (exact)
	*exact = 1;

      DEBUG_printf(("1_ppdCacheGetPageSize: Returning \"%s\"", size->map.ppd));

      return (size->map.ppd);
    }
  }

  if (closest)
  {
    DEBUG_printf(("1_ppdCacheGetPageSize: Returning \"%s\" (closest)",
                  closest->map.ppd));

    return (closest->map.ppd);
  }

 /*
  * If we get here we need to check for custom page size support...
  */

  if (jobsize.width >= pc->custom_min_width &&
      jobsize.width <= pc->custom_max_width &&
      jobsize.length >= pc->custom_min_length &&
      jobsize.length <= pc->custom_max_length)
  {
   /*
    * In range, format as Custom.WWWWxLLLL (points).
    */

    snprintf(pc->custom_ppd_size, sizeof(pc->custom_ppd_size), "Custom.%dx%d",
             (int)PWG_TO_POINTS(jobsize.width), (int)PWG_TO_POINTS(jobsize.length));

    if (margins_set && exact)
    {
      dleft   = pc->custom_size.left - jobsize.left;
      dright  = pc->custom_size.right - jobsize.right;
      dtop    = pc->custom_size.top - jobsize.top;
      dbottom = pc->custom_size.bottom - jobsize.bottom;

      if (dleft > -35 && dleft < 35 && dright > -35 && dright < 35 &&
          dtop > -35 && dtop < 35 && dbottom > -35 && dbottom < 35)
	*exact = 1;
    }
    else if (exact)
      *exact = 1;

    DEBUG_printf(("1_ppdCacheGetPageSize: Returning \"%s\" (custom)",
                  pc->custom_ppd_size));

    return (pc->custom_ppd_size);
  }

 /*
  * No custom page size support or the size is out of range - return NULL.
  */

  DEBUG_puts("1_ppdCacheGetPageSize: Returning NULL");

  return (NULL);
}


/*
 * '_ppdCacheGetSize()' - Get the PWG size associated with a PPD PageSize.
 */

pwg_size_t *				/* O - PWG size or NULL */
_ppdCacheGetSize(
    _ppd_cache_t *pc,			/* I - PPD cache and mapping data */
    const char   *page_size)		/* I - PPD PageSize */
{
  int		i;			/* Looping var */
  pwg_media_t	*media;			/* Media */
  pwg_size_t	*size;			/* Current size */


 /*
  * Range check input...
  */

  if (!pc || !page_size)
    return (NULL);

  if (!_cups_strncasecmp(page_size, "Custom.", 7))
  {
   /*
    * Custom size; size name can be one of the following:
    *
    *    Custom.WIDTHxLENGTHin    - Size in inches
    *    Custom.WIDTHxLENGTHft    - Size in feet
    *    Custom.WIDTHxLENGTHcm    - Size in centimeters
    *    Custom.WIDTHxLENGTHmm    - Size in millimeters
    *    Custom.WIDTHxLENGTHm     - Size in meters
    *    Custom.WIDTHxLENGTH[pt]  - Size in points
    */

    double		w, l;		/* Width and length of page */
    char		*ptr;		/* Pointer into PageSize */
    struct lconv	*loc;		/* Locale data */

    loc = localeconv();
    w   = (float)_cupsStrScand(page_size + 7, &ptr, loc);
    if (!ptr || *ptr != 'x')
      return (NULL);

    l = (float)_cupsStrScand(ptr + 1, &ptr, loc);
    if (!ptr)
      return (NULL);

    if (!_cups_strcasecmp(ptr, "in"))
    {
      w *= 2540.0;
      l *= 2540.0;
    }
    else if (!_cups_strcasecmp(ptr, "ft"))
    {
      w *= 12.0 * 2540.0;
      l *= 12.0 * 2540.0;
    }
    else if (!_cups_strcasecmp(ptr, "mm"))
    {
      w *= 100.0;
      l *= 100.0;
    }
    else if (!_cups_strcasecmp(ptr, "cm"))
    {
      w *= 1000.0;
      l *= 1000.0;
    }
    else if (!_cups_strcasecmp(ptr, "m"))
    {
      w *= 100000.0;
      l *= 100000.0;
    }
    else
    {
      w *= 2540.0 / 72.0;
      l *= 2540.0 / 72.0;
    }

    pc->custom_size.width  = (int)w;
    pc->custom_size.length = (int)l;

    return (&(pc->custom_size));
  }

 /*
  * Not a custom size - look it up...
  */

  for (i = pc->num_sizes, size = pc->sizes; i > 0; i --, size ++)
    if (!_cups_strcasecmp(page_size, size->map.ppd) ||
        !_cups_strcasecmp(page_size, size->map.pwg))
      return (size);

 /*
  * Look up standard sizes...
  */

  if ((media = pwgMediaForPPD(page_size)) == NULL)
    if ((media = pwgMediaForLegacy(page_size)) == NULL)
      media = pwgMediaForPWG(page_size);

  if (media)
  {
    pc->custom_size.width  = media->width;
    pc->custom_size.length = media->length;

    return (&(pc->custom_size));
  }

  return (NULL);
}


/*
 * '_ppdCacheGetSource()' - Get the PWG media-source associated with a PPD
 *                          InputSlot.
 */

const char *				/* O - PWG media-source keyword */
_ppdCacheGetSource(
    _ppd_cache_t *pc,			/* I - PPD cache and mapping data */
    const char   *input_slot)		/* I - PPD InputSlot */
{
  int		i;			/* Looping var */
  pwg_map_t	*source;		/* Current source */


 /*
  * Range check input...
  */

  if (!pc || !input_slot)
    return (NULL);

  for (i = pc->num_sources, source = pc->sources; i > 0; i --, source ++)
    if (!_cups_strcasecmp(input_slot, source->ppd))
      return (source->pwg);

  return (NULL);
}


/*
 * '_ppdCacheGetType()' - Get the PWG media-type associated with a PPD
 *                        MediaType.
 */

const char *				/* O - PWG media-type keyword */
_ppdCacheGetType(
    _ppd_cache_t *pc,			/* I - PPD cache and mapping data */
    const char   *media_type)		/* I - PPD MediaType */
{
  int		i;			/* Looping var */
  pwg_map_t	*type;			/* Current type */


 /*
  * Range check input...
  */

  if (!pc || !media_type)
    return (NULL);

  for (i = pc->num_types, type = pc->types; i > 0; i --, type ++)
    if (!_cups_strcasecmp(media_type, type->ppd))
      return (type->pwg);

  return (NULL);
}


/*
 * '_ppdCacheWriteFile()' - Write PWG mapping data to a file.
 */

int					/* O - 1 on success, 0 on failure */
_ppdCacheWriteFile(
    _ppd_cache_t *pc,			/* I - PPD cache and mapping data */
    const char   *filename,		/* I - File to write */
    ipp_t        *attrs)		/* I - Attributes to write, if any */
{
  int			i, j, k;	/* Looping vars */
  cups_file_t		*fp;		/* Output file */
  pwg_size_t		*size;		/* Current size */
  pwg_map_t		*map;		/* Current map */
  _pwg_finishings_t	*f;		/* Current finishing option */
  cups_option_t		*option;	/* Current option */
  const char		*value;		/* Filter/pre-filter value */
  char			newfile[1024];	/* New filename */


 /*
  * Range check input...
  */

  if (!pc || !filename)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    return (0);
  }

 /*
  * Open the file and write with compression...
  */

  snprintf(newfile, sizeof(newfile), "%s.N", filename);
  if ((fp = cupsFileOpen(newfile, "w9")) == NULL)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
    return (0);
  }

 /*
  * Standard header...
  */

  cupsFilePrintf(fp, "#CUPS-PPD-CACHE-%d\n", _PPD_CACHE_VERSION);

 /*
  * Output bins...
  */

  if (pc->num_bins > 0)
  {
    cupsFilePrintf(fp, "NumBins %d\n", pc->num_bins);
    for (i = pc->num_bins, map = pc->bins; i > 0; i --, map ++)
      cupsFilePrintf(fp, "Bin %s %s\n", map->pwg, map->ppd);
  }

 /*
  * Media sizes...
  */

  cupsFilePrintf(fp, "NumSizes %d\n", pc->num_sizes);
  for (i = pc->num_sizes, size = pc->sizes; i > 0; i --, size ++)
    cupsFilePrintf(fp, "Size %s %s %d %d %d %d %d %d\n", size->map.pwg,
		   size->map.ppd, size->width, size->length, size->left,
		   size->bottom, size->right, size->top);
  if (pc->custom_max_width > 0)
    cupsFilePrintf(fp, "CustomSize %d %d %d %d %d %d %d %d\n",
                   pc->custom_max_width, pc->custom_max_length,
		   pc->custom_min_width, pc->custom_min_length,
		   pc->custom_size.left, pc->custom_size.bottom,
		   pc->custom_size.right, pc->custom_size.top);

 /*
  * Media sources...
  */

  if (pc->source_option)
    cupsFilePrintf(fp, "SourceOption %s\n", pc->source_option);

  if (pc->num_sources > 0)
  {
    cupsFilePrintf(fp, "NumSources %d\n", pc->num_sources);
    for (i = pc->num_sources, map = pc->sources; i > 0; i --, map ++)
      cupsFilePrintf(fp, "Source %s %s\n", map->pwg, map->ppd);
  }

 /*
  * Media types...
  */

  if (pc->num_types > 0)
  {
    cupsFilePrintf(fp, "NumTypes %d\n", pc->num_types);
    for (i = pc->num_types, map = pc->types; i > 0; i --, map ++)
      cupsFilePrintf(fp, "Type %s %s\n", map->pwg, map->ppd);
  }

 /*
  * Presets...
  */

  for (i = _PWG_PRINT_COLOR_MODE_MONOCHROME; i < _PWG_PRINT_COLOR_MODE_MAX; i ++)
    for (j = _PWG_PRINT_QUALITY_DRAFT; j < _PWG_PRINT_QUALITY_MAX; j ++)
      if (pc->num_presets[i][j])
      {
	cupsFilePrintf(fp, "Preset %d %d", i, j);
	for (k = pc->num_presets[i][j], option = pc->presets[i][j];
	     k > 0;
	     k --, option ++)
	  cupsFilePrintf(fp, " %s=%s", option->name, option->value);
	cupsFilePutChar(fp, '\n');
      }

 /*
  * Duplex/sides...
  */

  if (pc->sides_option)
    cupsFilePrintf(fp, "SidesOption %s\n", pc->sides_option);

  if (pc->sides_1sided)
    cupsFilePrintf(fp, "Sides1Sided %s\n", pc->sides_1sided);

  if (pc->sides_2sided_long)
    cupsFilePrintf(fp, "Sides2SidedLong %s\n", pc->sides_2sided_long);

  if (pc->sides_2sided_short)
    cupsFilePrintf(fp, "Sides2SidedShort %s\n", pc->sides_2sided_short);

 /*
  * Product, cupsFilter, cupsFilter2, and cupsPreFilter...
  */

  if (pc->product)
    cupsFilePutConf(fp, "Product", pc->product);

  for (value = (const char *)cupsArrayFirst(pc->filters);
       value;
       value = (const char *)cupsArrayNext(pc->filters))
    cupsFilePutConf(fp, "Filter", value);

  for (value = (const char *)cupsArrayFirst(pc->prefilters);
       value;
       value = (const char *)cupsArrayNext(pc->prefilters))
    cupsFilePutConf(fp, "PreFilter", value);

  cupsFilePrintf(fp, "SingleFile %s\n", pc->single_file ? "true" : "false");

 /*
  * Finishing options...
  */

  for (f = (_pwg_finishings_t *)cupsArrayFirst(pc->finishings);
       f;
       f = (_pwg_finishings_t *)cupsArrayNext(pc->finishings))
  {
    cupsFilePrintf(fp, "Finishings %d", f->value);
    for (i = f->num_options, option = f->options; i > 0; i --, option ++)
      cupsFilePrintf(fp, " %s=%s", option->name, option->value);
    cupsFilePutChar(fp, '\n');
  }

 /*
  * Max copies...
  */

  cupsFilePrintf(fp, "MaxCopies %d\n", pc->max_copies);

 /*
  * Accounting/quota/PIN/managed printing values...
  */

  if (pc->charge_info_uri)
    cupsFilePutConf(fp, "ChargeInfoURI", pc->charge_info_uri);

  cupsFilePrintf(fp, "AccountId %s\n", pc->account_id ? "true" : "false");
  cupsFilePrintf(fp, "AccountingUserId %s\n",
                 pc->accounting_user_id ? "true" : "false");

  if (pc->password)
    cupsFilePutConf(fp, "Password", pc->password);

  for (value = (char *)cupsArrayFirst(pc->mandatory);
       value;
       value = (char *)cupsArrayNext(pc->mandatory))
    cupsFilePutConf(fp, "Mandatory", value);

 /*
  * Support files...
  */

  for (value = (char *)cupsArrayFirst(pc->support_files);
       value;
       value = (char *)cupsArrayNext(pc->support_files))
    cupsFilePutConf(fp, "SupportFile", value);

 /*
  * IPP attributes, if any...
  */

  if (attrs)
  {
    cupsFilePrintf(fp, "IPP " CUPS_LLFMT "\n", CUPS_LLCAST ippLength(attrs));

    attrs->state = IPP_STATE_IDLE;
    ippWriteIO(fp, (ipp_iocb_t)cupsFileWrite, 1, NULL, attrs);
  }

 /*
  * Close and return...
  */

  if (cupsFileClose(fp))
  {
    unlink(newfile);
    return (0);
  }

  unlink(filename);
  return (!rename(newfile, filename));
}


/*
 * '_pwgInputSlotForSource()' - Get the InputSlot name for the given PWG
 *                              media-source.
 */

const char *				/* O - InputSlot name */
_pwgInputSlotForSource(
    const char *media_source,		/* I - PWG media-source */
    char       *name,			/* I - Name buffer */
    size_t     namesize)		/* I - Size of name buffer */
{
 /*
  * Range check input...
  */

  if (!media_source || !name || namesize < PPD_MAX_NAME)
    return (NULL);

  if (_cups_strcasecmp(media_source, "main"))
    strlcpy(name, "Cassette", namesize);
  else if (_cups_strcasecmp(media_source, "alternate"))
    strlcpy(name, "Multipurpose", namesize);
  else if (_cups_strcasecmp(media_source, "large-capacity"))
    strlcpy(name, "LargeCapacity", namesize);
  else if (_cups_strcasecmp(media_source, "bottom"))
    strlcpy(name, "Lower", namesize);
  else if (_cups_strcasecmp(media_source, "middle"))
    strlcpy(name, "Middle", namesize);
  else if (_cups_strcasecmp(media_source, "top"))
    strlcpy(name, "Upper", namesize);
  else if (_cups_strcasecmp(media_source, "rear"))
    strlcpy(name, "Rear", namesize);
  else if (_cups_strcasecmp(media_source, "side"))
    strlcpy(name, "Side", namesize);
  else if (_cups_strcasecmp(media_source, "envelope"))
    strlcpy(name, "Envelope", namesize);
  else if (_cups_strcasecmp(media_source, "main-roll"))
    strlcpy(name, "Roll", namesize);
  else if (_cups_strcasecmp(media_source, "alternate-roll"))
    strlcpy(name, "Roll2", namesize);
  else
    pwg_ppdize_name(media_source, name, namesize);

  return (name);
}


/*
 * '_pwgMediaTypeForType()' - Get the MediaType name for the given PWG
 *                            media-type.
 */

const char *				/* O - MediaType name */
_pwgMediaTypeForType(
    const char *media_type,		/* I - PWG media-type */
    char       *name,			/* I - Name buffer */
    size_t     namesize)		/* I - Size of name buffer */
{
 /*
  * Range check input...
  */

  if (!media_type || !name || namesize < PPD_MAX_NAME)
    return (NULL);

  if (_cups_strcasecmp(media_type, "auto"))
    strlcpy(name, "Auto", namesize);
  else if (_cups_strcasecmp(media_type, "cardstock"))
    strlcpy(name, "Cardstock", namesize);
  else if (_cups_strcasecmp(media_type, "envelope"))
    strlcpy(name, "Envelope", namesize);
  else if (_cups_strcasecmp(media_type, "photographic-glossy"))
    strlcpy(name, "Glossy", namesize);
  else if (_cups_strcasecmp(media_type, "photographic-high-gloss"))
    strlcpy(name, "HighGloss", namesize);
  else if (_cups_strcasecmp(media_type, "photographic-matte"))
    strlcpy(name, "Matte", namesize);
  else if (_cups_strcasecmp(media_type, "stationery"))
    strlcpy(name, "Plain", namesize);
  else if (_cups_strcasecmp(media_type, "stationery-coated"))
    strlcpy(name, "Coated", namesize);
  else if (_cups_strcasecmp(media_type, "stationery-inkjet"))
    strlcpy(name, "Inkjet", namesize);
  else if (_cups_strcasecmp(media_type, "stationery-letterhead"))
    strlcpy(name, "Letterhead", namesize);
  else if (_cups_strcasecmp(media_type, "stationery-preprinted"))
    strlcpy(name, "Preprinted", namesize);
  else if (_cups_strcasecmp(media_type, "transparency"))
    strlcpy(name, "Transparency", namesize);
  else
    pwg_ppdize_name(media_type, name, namesize);

  return (name);
}


/*
 * '_pwgPageSizeForMedia()' - Get the PageSize name for the given media.
 */

const char *				/* O - PageSize name */
_pwgPageSizeForMedia(
    pwg_media_t *media,		/* I - Media */
    char         *name,			/* I - PageSize name buffer */
    size_t       namesize)		/* I - Size of name buffer */
{
  const char	*sizeptr,		/* Pointer to size in PWG name */
		*dimptr;		/* Pointer to dimensions in PWG name */


 /*
  * Range check input...
  */

  if (!media || !name || namesize < PPD_MAX_NAME)
    return (NULL);

 /*
  * Copy or generate a PageSize name...
  */

  if (media->ppd)
  {
   /*
    * Use a standard Adobe name...
    */

    strlcpy(name, media->ppd, namesize);
  }
  else if (!media->pwg || !strncmp(media->pwg, "custom_", 7) ||
           (sizeptr = strchr(media->pwg, '_')) == NULL ||
	   (dimptr = strchr(sizeptr + 1, '_')) == NULL ||
	   (size_t)(dimptr - sizeptr) > namesize)
  {
   /*
    * Use a name of the form "wNNNhNNN"...
    */

    snprintf(name, namesize, "w%dh%d", (int)PWG_TO_POINTS(media->width),
             (int)PWG_TO_POINTS(media->length));
  }
  else
  {
   /*
    * Copy the size name from class_sizename_dimensions...
    */

    memcpy(name, sizeptr + 1, dimptr - sizeptr - 1);
    name[dimptr - sizeptr - 1] = '\0';
  }

  return (name);
}


/*
 * 'pwg_compare_finishings()' - Compare two finishings values.
 */

static int				/* O- Result of comparison */
pwg_compare_finishings(
    _pwg_finishings_t *a,		/* I - First finishings value */
    _pwg_finishings_t *b)		/* I - Second finishings value */
{
  return (b->value - a->value);
}


/*
 * 'pwg_free_finishings()' - Free a finishings value.
 */

static void
pwg_free_finishings(
    _pwg_finishings_t *f)		/* I - Finishings value */
{
  cupsFreeOptions(f->num_options, f->options);
  free(f);
}


/*
 * 'pwg_ppdize_name()' - Convert an IPP keyword to a PPD keyword.
 */

static void
pwg_ppdize_name(const char *ipp,	/* I - IPP keyword */
                char       *name,	/* I - Name buffer */
		size_t     namesize)	/* I - Size of name buffer */
{
  char	*ptr,				/* Pointer into name buffer */
	*end;				/* End of name buffer */


  *name = toupper(*ipp++);

  for (ptr = name + 1, end = name + namesize - 1; *ipp && ptr < end;)
  {
    if (*ipp == '-' && _cups_isalpha(ipp[1]))
    {
      ipp ++;
      *ptr++ = toupper(*ipp++ & 255);
    }
    else
      *ptr++ = *ipp++;
  }

  *ptr = '\0';
}


/*
 * 'pwg_unppdize_name()' - Convert a PPD keyword to a lowercase IPP keyword.
 */

static void
pwg_unppdize_name(const char *ppd,	/* I - PPD keyword */
		  char       *name,	/* I - Name buffer */
                  size_t     namesize,	/* I - Size of name buffer */
                  const char *dashchars)/* I - Characters to be replaced by dashes */
{
  char	*ptr,				/* Pointer into name buffer */
	*end;				/* End of name buffer */


  for (ptr = name, end = name + namesize - 1; *ppd && ptr < end; ppd ++)
  {
    if (_cups_isalnum(*ppd) || *ppd == '-')
      *ptr++ = tolower(*ppd & 255);
    else if (strchr(dashchars, *ppd))
      *ptr++ = '-';
    else
      *ptr++ = *ppd;

    if (!_cups_isupper(*ppd) && _cups_isalnum(*ppd) &&
	_cups_isupper(ppd[1]) && ptr < end)
      *ptr++ = '-';
    else if (!isdigit(*ppd & 255) && isdigit(ppd[1] & 255))
      *ptr++ = '-';
  }

  *ptr = '\0';
}


/*
 * End of "$Id: ppd-cache.c 11833 2014-04-24 15:04:15Z msweet $".
 */
