/*
 * "$Id$"
 *
 *   PWG load/save API implementation for CUPS.
 *
 *   Copyright 2010 by Apple Inc.
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
 *   _pwgCreateWithFile() - Create PWG mapping data from a written file.
 *   _pwgDestroy()        - Free all memory used for PWG mapping data.
 *   _pwgWriteFile()      - Write PWG mapping data to a file.
 */

/*
 * Include necessary headers...
 */

#include "pwg-private.h"
#include "globals.h"
#include "string.h"
#include "debug.h"
#include <math.h>


/*
 * '_pwgCreateWithFile()' - Create PWG mapping data from a written file.
 *
 * Use the @link _pwgWriteFile@ function to write PWG mapping data to a file.
 */

_pwg_t *				/* O - PWG mapping data */
_pwgCreateWithFile(const char *filename)/* I - File to read */
{
  cups_file_t	*fp;			/* File */
  _pwg_t	*pwg;			/* PWG mapping data */
  _pwg_size_t	*size;			/* Current size */
  _pwg_map_t	*map;			/* Current map */
  int		linenum,		/* Current line number */
		num_sizes,		/* Number of sizes in file */
		num_sources,		/* Number of sources in file */
		num_types;		/* Number of types in file */
  char		line[512],		/* Current line */
		*value,			/* Pointer to value in line */
		pwg_keyword[128],	/* PWG keyword */
		ppd_keyword[PPD_MAX_NAME];
					/* PPD keyword */


  DEBUG_printf(("_pwgCreateWithFile(filename=\"%s\")", filename));

 /*
  * Range check input...
  */

  if (!filename)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, strerror(EINVAL), 0);
    return (NULL);
  }

 /*
  * Open the file...
  */

  if ((fp = cupsFileOpen(filename, "r")) == NULL)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, strerror(errno), 0);
    return (NULL);
  }

 /*
  * Allocate the mapping data structure...
  */

  if ((pwg = calloc(1, sizeof(_pwg_t))) == NULL)
  {
    DEBUG_puts("_pwgCreateWithFile: Unable to allocate pwg_t.");
    _cupsSetError(IPP_INTERNAL_ERROR, strerror(errno), 0);
    goto create_error;
  }

 /*
  * Read the file...
  */

  linenum     = 0;
  num_sizes   = 0;
  num_sources = 0;
  num_types   = 0;

  while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
  {
    DEBUG_printf(("_pwgCreateWithFile: line=\"%s\", value=\"%s\", linenum=%d",
		  line, value, linenum));

    if (!value)
    {
      DEBUG_printf(("_pwgCreateWithFile: Missing value on line %d.", linenum));
      _cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
      goto create_error;
    }
    else if (!strcasecmp(line, "NumSizes"))
    {
      if (num_sizes > 0)
      {
        DEBUG_puts("_pwgCreateWithFile: NumSizes listed multiple times.");
	_cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
	goto create_error;
      }

      if ((num_sizes = atoi(value)) <= 0 || num_sizes > 65536)
      {
        DEBUG_printf(("_pwgCreateWithFile: Bad NumSizes value %d on line %d.",
	              num_sizes, linenum));
	_cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
	goto create_error;
      }

      if ((pwg->sizes = calloc(num_sizes, sizeof(_pwg_size_t))) == NULL)
      {
        DEBUG_printf(("_pwgCreateWithFile: Unable to allocate %d sizes.",
	              num_sizes));
	_cupsSetError(IPP_INTERNAL_ERROR, strerror(errno), 0);
	goto create_error;
      }
    }
    else if (!strcasecmp(line, "Size"))
    {
      if (pwg->num_sizes >= num_sizes)
      {
        DEBUG_printf(("_pwgCreateWithFile: Too many Size's on line %d.",
	              linenum));
	_cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
	goto create_error;
      }

      size = pwg->sizes + pwg->num_sizes;

      if (sscanf(value, "%127s%40s%d%d%d%d%d%d", pwg_keyword, ppd_keyword,
		 &(size->width), &(size->length), &(size->left),
		 &(size->bottom), &(size->right), &(size->top)) != 8)
      {
        DEBUG_printf(("_pwgCreateWithFile: Bad Size on line %d.", linenum));
	_cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
	goto create_error;
      }

      size->map.pwg = _cupsStrAlloc(pwg_keyword);
      size->map.ppd = _cupsStrAlloc(ppd_keyword);

      pwg->num_sizes ++;
    }
    else if (!strcasecmp(line, "CustomSize"))
    {
      if (pwg->custom_max_width > 0)
      {
        DEBUG_printf(("_pwgCreateWithFile: Too many CustomSize's on line %d.",
	              linenum));
	_cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
	goto create_error;
      }

      if (sscanf(value, "%d%d%d%d%d%d%d%d", &(pwg->custom_max_width),
                 &(pwg->custom_max_length), &(pwg->custom_min_width),
		 &(pwg->custom_min_length), &(pwg->custom_size.left),
		 &(pwg->custom_size.bottom), &(pwg->custom_size.right),
		 &(pwg->custom_size.top)) != 8)
      {
        DEBUG_printf(("_pwgCreateWithFile: Bad CustomSize on line %d.", linenum));
	_cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
	goto create_error;
      }

      _pwgGenerateSize(pwg_keyword, sizeof(pwg_keyword), "custom", "max",
		       pwg->custom_max_width, pwg->custom_max_length);
      pwg->custom_max_keyword = _cupsStrAlloc(pwg_keyword);

      _pwgGenerateSize(pwg_keyword, sizeof(pwg_keyword), "custom", "min",
		       pwg->custom_min_width, pwg->custom_min_length);
      pwg->custom_min_keyword = _cupsStrAlloc(pwg_keyword);
    }
    else if (!strcasecmp(line, "NumSources"))
    {
      if (num_sources > 0)
      {
        DEBUG_puts("_pwgCreateWithFile: NumSources listed multiple times.");
	_cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
	goto create_error;
      }

      if ((num_sources = atoi(value)) <= 0 || num_sources > 65536)
      {
        DEBUG_printf(("_pwgCreateWithFile: Bad NumSources value %d on line %d.",
	              num_sources, linenum));
	_cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
	goto create_error;
      }

      if ((pwg->sources = calloc(num_sources, sizeof(_pwg_map_t))) == NULL)
      {
        DEBUG_printf(("_pwgCreateWithFile: Unable to allocate %d sources.",
	              num_sources));
	_cupsSetError(IPP_INTERNAL_ERROR, strerror(errno), 0);
	goto create_error;
      }
    }
    else if (!strcasecmp(line, "Source"))
    {
      if (sscanf(value, "%127s%40s", pwg_keyword, ppd_keyword) != 2)
      {
        DEBUG_printf(("_pwgCreateWithFile: Bad Source on line %d.", linenum));
	_cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
	goto create_error;
      }

      if (pwg->num_sources >= num_sources)
      {
        DEBUG_printf(("_pwgCreateWithFile: Too many Source's on line %d.",
	              linenum));
	_cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
	goto create_error;
      }

      map      = pwg->sources + pwg->num_sources;
      map->pwg = _cupsStrAlloc(pwg_keyword);
      map->ppd = _cupsStrAlloc(ppd_keyword);

      pwg->num_sources ++;
    }
    else if (!strcasecmp(line, "NumTypes"))
    {
      if (num_types > 0)
      {
        DEBUG_puts("_pwgCreateWithFile: NumTypes listed multiple times.");
	_cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
	goto create_error;
      }

      if ((num_types = atoi(value)) <= 0 || num_types > 65536)
      {
        DEBUG_printf(("_pwgCreateWithFile: Bad NumTypes value %d on line %d.",
	              num_types, linenum));
	_cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
	goto create_error;
      }

      if ((pwg->types = calloc(num_types, sizeof(_pwg_map_t))) == NULL)
      {
        DEBUG_printf(("_pwgCreateWithFile: Unable to allocate %d types.",
	              num_types));
	_cupsSetError(IPP_INTERNAL_ERROR, strerror(errno), 0);
	goto create_error;
      }
    }
    else if (!strcasecmp(line, "Type"))
    {
      if (sscanf(value, "%127s%40s", pwg_keyword, ppd_keyword) != 2)
      {
        DEBUG_printf(("_pwgCreateWithFile: Bad Type on line %d.", linenum));
	_cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
	goto create_error;
      }

      if (pwg->num_types >= num_types)
      {
        DEBUG_printf(("_pwgCreateWithFile: Too many Type's on line %d.",
	              linenum));
	_cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
	goto create_error;
      }

      map      = pwg->types + pwg->num_types;
      map->pwg = _cupsStrAlloc(pwg_keyword);
      map->ppd = _cupsStrAlloc(ppd_keyword);

      pwg->num_types ++;
    }
    else
    {
      DEBUG_printf(("_pwgCreateWithFile: Unknown %s on line %d.", line,
		    linenum));
      _cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
      goto create_error;
    }
  }

  if (pwg->num_sizes < num_sizes)
  {
    DEBUG_printf(("_pwgCreateWithFile: Not enough sizes (%d < %d).",
                  pwg->num_sizes, num_sizes));
    _cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
    goto create_error;
  }

  if (pwg->num_sources < num_sources)
  {
    DEBUG_printf(("_pwgCreateWithFile: Not enough sources (%d < %d).",
                  pwg->num_sources, num_sources));
    _cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
    goto create_error;
  }

  if (pwg->num_types < num_types)
  {
    DEBUG_printf(("_pwgCreateWithFile: Not enough types (%d < %d).",
                  pwg->num_types, num_types));
    _cupsSetError(IPP_INTERNAL_ERROR, _("Bad PWG mapping file."), 1);
    goto create_error;
  }

  cupsFileClose(fp);

  return (pwg);

 /*
  * If we get here the file was bad - free any data and return...
  */

  create_error:

  cupsFileClose(fp);
  _pwgDestroy(pwg);

  return (NULL);
}


/*
 * '_pwgDestroy()' - Free all memory used for PWG mapping data.
 */

void
_pwgDestroy(_pwg_t *pwg)		/* I - PWG mapping data */
{
  int		i;			/* Looping var */
  _pwg_map_t	*map;			/* Current map */
  _pwg_size_t	*size;			/* Current size */


 /*
  * Range check input...
  */

  if (!pwg)
    return;

 /*
  * Free memory as needed...
  */

  if (pwg->sizes)
  {
    for (i = pwg->num_sizes, size = pwg->sizes; i > 0; i --, size ++)
    {
      _cupsStrFree(size->map.pwg);
      _cupsStrFree(size->map.ppd);
    }

    free(pwg->sizes);
  }

  if (pwg->sources)
  {
    for (i = pwg->num_sources, map = pwg->sources; i > 0; i --, map ++)
    {
      _cupsStrFree(map->pwg);
      _cupsStrFree(map->ppd);
    }

    free(pwg->sources);
  }

  if (pwg->types)
  {
    for (i = pwg->num_types, map = pwg->types; i > 0; i --, map ++)
    {
      _cupsStrFree(map->pwg);
      _cupsStrFree(map->ppd);
    }

    free(pwg->types);
  }

  if (pwg->custom_max_keyword)
    _cupsStrFree(pwg->custom_max_keyword);

  if (pwg->custom_min_keyword)
    _cupsStrFree(pwg->custom_min_keyword);

  free(pwg);
}


/*
 * '_pwgWriteFile()' - Write PWG mapping data to a file.
 */

int					/* O - 1 on success, 0 on failure */
_pwgWriteFile(_pwg_t     *pwg,		/* I - PWG mapping data */
              const char *filename)	/* I - File to write */
{
  int		i;			/* Looping var */
  cups_file_t	*fp;			/* Output file */
  _pwg_size_t	*size;			/* Current size */
  _pwg_map_t	*map;			/* Current map */


 /*
  * Range check input...
  */

  if (!pwg || !filename)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, strerror(EINVAL), 0);
    return (0);
  }

 /*
  * Open the file and write with compression...
  */

  if ((fp = cupsFileOpen(filename, "w9")) == NULL)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, strerror(errno), 0);
    return (0);
  }

 /*
  * Standard header...
  */

  cupsFilePuts(fp, "#CUPS-PWGPPD\n");

 /*
  * Media sizes...
  */

  cupsFilePrintf(fp, "NumSizes %d\n", pwg->num_sizes);
  for (i = pwg->num_sizes, size = pwg->sizes; i > 0; i --, size ++)
    cupsFilePrintf(fp, "Size %s %s %d %d %d %d %d %d\n", size->map.pwg,
		   size->map.ppd, size->width, size->length, size->left,
		   size->bottom, size->right, size->top);
  if (pwg->custom_max_width > 0)
    cupsFilePrintf(fp, "CustomSize %d %d %d %d %d %d %d %d\n",
                   pwg->custom_max_width, pwg->custom_max_length,
		   pwg->custom_min_width, pwg->custom_min_length,
		   pwg->custom_size.left, pwg->custom_size.bottom,
		   pwg->custom_size.right, pwg->custom_size.top);

 /*
  * Media sources...
  */

  if (pwg->num_sources > 0)
  {
    cupsFilePrintf(fp, "NumSources %d\n", pwg->num_sources);
    for (i = pwg->num_sources, map = pwg->sources; i > 0; i --, map ++)
      cupsFilePrintf(fp, "Source %s %s\n", map->pwg, map->ppd);
  }

 /*
  * Media types...
  */

  if (pwg->num_types > 0)
  {
    cupsFilePrintf(fp, "NumTypes %d\n", pwg->num_types);
    for (i = pwg->num_types, map = pwg->types; i > 0; i --, map ++)
      cupsFilePrintf(fp, "Type %s %s\n", map->pwg, map->ppd);
  }

 /*
  * Close and return...
  */

  return (!cupsFileClose(fp));
}


/*
 * End of "$Id$".
 */
