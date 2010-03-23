/*
 * "$Id: page.c 7791 2008-07-24 00:55:30Z mike $"
 *
 *   Page size functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2010 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   PostScript is a trademark of Adobe Systems, Inc.
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   ppdPageSize()       - Get the page size record for the given size.
 *   ppdPageSizeLimits() - Return the custom page size limits.
 *   ppdPageWidth()      - Get the page width for the given size.
 *   ppdPageLength()     - Get the page length for the given size.
 */

/*
 * Include necessary headers...
 */

#include "ppd.h"
#include "string.h"
#include <ctype.h>
#include "debug.h"


/*
 * 'ppdPageSize()' - Get the page size record for the given size.
 */

ppd_size_t *				/* O - Size record for page or NULL */
ppdPageSize(ppd_file_t *ppd,		/* I - PPD file record */
            const char *name)		/* I - Size name */
{
  int		i;			/* Looping var */
  ppd_size_t	*size;			/* Current page size */
  double	w, l;			/* Width and length of page */
  char		*nameptr;		/* Pointer into name */
  struct lconv	*loc;			/* Locale data */
  ppd_coption_t	*coption;		/* Custom option for page size */
  ppd_cparam_t	*cparam;		/* Custom option parameter */


  DEBUG_printf(("2ppdPageSize(ppd=%p, name=\"%s\")", ppd, name));

  if (!ppd)
  {
    DEBUG_puts("3ppdPageSize: Bad PPD pointer, returning NULL...");
    return (NULL);
  }

  if (name)
  {
    if (!strncmp(name, "Custom.", 7) && ppd->variable_sizes)
    {
     /*
      * Find the custom page size...
      */

      for (i = ppd->num_sizes, size = ppd->sizes; i > 0; i --, size ++)
	if (!strcmp("Custom", size->name))
          break;

      if (!i)
      {
	DEBUG_puts("3ppdPageSize: No custom sizes, returning NULL...");
        return (NULL);
      }

     /*
      * Variable size; size name can be one of the following:
      *
      *    Custom.WIDTHxLENGTHin    - Size in inches
      *    Custom.WIDTHxLENGTHft    - Size in feet
      *    Custom.WIDTHxLENGTHcm    - Size in centimeters
      *    Custom.WIDTHxLENGTHmm    - Size in millimeters
      *    Custom.WIDTHxLENGTHm     - Size in meters
      *    Custom.WIDTHxLENGTH[pt]  - Size in points
      */

      loc = localeconv();
      w   = _cupsStrScand(name + 7, &nameptr, loc);
      if (!nameptr || *nameptr != 'x')
        return (NULL);

      l = _cupsStrScand(nameptr + 1, &nameptr, loc);
      if (!nameptr)
        return (NULL);

      if (!strcasecmp(nameptr, "in"))
      {
        w *= 72.0;
	l *= 72.0;
      }
      else if (!strcasecmp(nameptr, "ft"))
      {
        w *= 12.0 * 72.0;
	l *= 12.0 * 72.0;
      }
      else if (!strcasecmp(nameptr, "mm"))
      {
        w *= 72.0 / 25.4;
        l *= 72.0 / 25.4;
      }
      else if (!strcasecmp(nameptr, "cm"))
      {
        w *= 72.0 / 2.54;
        l *= 72.0 / 2.54;
      }
      else if (!strcasecmp(nameptr, "m"))
      {
        w *= 72.0 / 0.0254;
        l *= 72.0 / 0.0254;
      }

      size->width  = (float)w;
      size->length = (float)l;
      size->left   = ppd->custom_margins[0];
      size->bottom = ppd->custom_margins[1];
      size->right  = (float)(w - ppd->custom_margins[2]);
      size->top    = (float)(l - ppd->custom_margins[3]);

     /*
      * Update the custom option records for the page size, too...
      */

      if ((coption = ppdFindCustomOption(ppd, "PageSize")) != NULL)
      {
        if ((cparam = ppdFindCustomParam(coption, "Width")) != NULL)
	  cparam->current.custom_points = w;

        if ((cparam = ppdFindCustomParam(coption, "Height")) != NULL)
	  cparam->current.custom_points = l;
      }

     /*
      * Return the page size...
      */

      DEBUG_printf(("3ppdPageSize: Returning %p (\"%s\", %gx%g)", size,
                    size->name, size->width, size->length));

      return (size);
    }
    else
    {
     /*
      * Lookup by name...
      */

      for (i = ppd->num_sizes, size = ppd->sizes; i > 0; i --, size ++)
	if (!strcasecmp(name, size->name))
	{
	  DEBUG_printf(("3ppdPageSize: Returning %p (\"%s\", %gx%g)", size,
			size->name, size->width, size->length));

          return (size);
	}
    }
  }
  else
  {
   /*
    * Find default...
    */

    for (i = ppd->num_sizes, size = ppd->sizes; i > 0; i --, size ++)
      if (size->marked)
      {
	DEBUG_printf(("3ppdPageSize: Returning %p (\"%s\", %gx%g)", size,
		      size->name, size->width, size->length));

        return (size);
      }
  }

  DEBUG_puts("3ppdPageSize: Size not found, returning NULL");

  return (NULL);
}


/*
 * 'ppdPageSizeLimits()' - Return the custom page size limits.
 *
 * This function returns the minimum and maximum custom page sizes and printable
 * areas based on the currently-marked (selected) options.
 *
 * If the specified PPD file does not support custom page sizes, both
 * "minimum" and "maximum" are filled with zeroes.
 *
 * @since CUPS 1.4/Mac OS X 10.6@
 */

int					/* O - 1 if custom sizes are supported, 0 otherwise */
ppdPageSizeLimits(ppd_file_t *ppd,	/* I - PPD file record */
                  ppd_size_t *minimum,	/* O - Minimum custom size */
		  ppd_size_t *maximum)	/* O - Maximum custom size */
{
  ppd_choice_t	*qualifier2,		/* Second media qualifier */
		*qualifier3;		/* Third media qualifier */
  ppd_attr_t	*attr;			/* Attribute */
  float		width,			/* Min/max width */
		length;			/* Min/max length */
  char		spec[PPD_MAX_NAME];	/* Selector for min/max */


 /*
  * Range check input...
  */

  if (!ppd || !ppd->variable_sizes || !minimum || !maximum)
  {
    if (minimum)
      memset(minimum, 0, sizeof(ppd_size_t));

    if (maximum)
      memset(maximum, 0, sizeof(ppd_size_t));

    return (0);
  }

 /*
  * See if we have the cupsMediaQualifier2 and cupsMediaQualifier3 attributes...
  */

  cupsArraySave(ppd->sorted_attrs);

  if ((attr = ppdFindAttr(ppd, "cupsMediaQualifier2", NULL)) != NULL &&
      attr->value)
    qualifier2 = ppdFindMarkedChoice(ppd, attr->value);
  else
    qualifier2 = NULL;

  if ((attr = ppdFindAttr(ppd, "cupsMediaQualifier3", NULL)) != NULL &&
      attr->value)
    qualifier3 = ppdFindMarkedChoice(ppd, attr->value);
  else
    qualifier3 = NULL;

 /*
  * Figure out the current minimum width and length...
  */

  width  = ppd->custom_min[0];
  length = ppd->custom_min[1];

  if (qualifier2)
  {
   /*
    * Try getting cupsMinSize...
    */

    if (qualifier3)
    {
      snprintf(spec, sizeof(spec), ".%s.%s", qualifier2->choice,
	       qualifier3->choice);
      attr = ppdFindAttr(ppd, "cupsMinSize", spec);
    }
    else
      attr = NULL;

    if (!attr)
    {
      snprintf(spec, sizeof(spec), ".%s.", qualifier2->choice);
      attr = ppdFindAttr(ppd, "cupsMinSize", spec);
    }

    if (!attr && qualifier3)
    {
      snprintf(spec, sizeof(spec), "..%s", qualifier3->choice);
      attr = ppdFindAttr(ppd, "cupsMinSize", spec);
    }

    if ((attr && attr->value &&
         sscanf(attr->value, "%f%f", &width, &length) != 2) || !attr)
    {
      width  = ppd->custom_min[0];
      length = ppd->custom_min[1];
    }
  }

  minimum->width  = width;
  minimum->length = length;
  minimum->left   = ppd->custom_margins[0];
  minimum->bottom = ppd->custom_margins[1];
  minimum->right  = width - ppd->custom_margins[2];
  minimum->top    = length - ppd->custom_margins[3];

 /*
  * Figure out the current maximum width and length...
  */

  width  = ppd->custom_max[0];
  length = ppd->custom_max[1];

  if (qualifier2)
  {
   /*
    * Try getting cupsMaxSize...
    */

    if (qualifier3)
    {
      snprintf(spec, sizeof(spec), ".%s.%s", qualifier2->choice,
	       qualifier3->choice);
      attr = ppdFindAttr(ppd, "cupsMaxSize", spec);
    }
    else
      attr = NULL;

    if (!attr)
    {
      snprintf(spec, sizeof(spec), ".%s.", qualifier2->choice);
      attr = ppdFindAttr(ppd, "cupsMaxSize", spec);
    }

    if (!attr && qualifier3)
    {
      snprintf(spec, sizeof(spec), "..%s", qualifier3->choice);
      attr = ppdFindAttr(ppd, "cupsMaxSize", spec);
    }

    if (!attr ||
        (attr->value && sscanf(attr->value, "%f%f", &width, &length) != 2))
    {
      width  = ppd->custom_max[0];
      length = ppd->custom_max[1];
    }
  }

  maximum->width  = width;
  maximum->length = length;
  maximum->left   = ppd->custom_margins[0];
  maximum->bottom = ppd->custom_margins[1];
  maximum->right  = width - ppd->custom_margins[2];
  maximum->top    = length - ppd->custom_margins[3];

 /*
  * Return the min and max...
  */

  cupsArrayRestore(ppd->sorted_attrs);

  return (1);
}


/*
 * 'ppdPageWidth()' - Get the page width for the given size.
 */

float				/* O - Width of page in points or 0.0 */
ppdPageWidth(ppd_file_t *ppd,	/* I - PPD file record */
             const char *name)	/* I - Size name */
{
  ppd_size_t	*size;		/* Page size */


  if ((size = ppdPageSize(ppd, name)) == NULL)
    return (0.0);
  else
    return (size->width);
}


/*
 * 'ppdPageLength()' - Get the page length for the given size.
 */

float				/* O - Length of page in points or 0.0 */
ppdPageLength(ppd_file_t *ppd,	/* I - PPD file */
              const char *name)	/* I - Size name */
{
  ppd_size_t	*size;		/* Page size */


  if ((size = ppdPageSize(ppd, name)) == NULL)
    return (0.0);
  else
    return (size->length);
}


/*
 * End of "$Id: page.c 7791 2008-07-24 00:55:30Z mike $".
 */
