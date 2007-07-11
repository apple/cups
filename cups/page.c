/*
 * "$Id$"
 *
 *   Page size functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
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
 *   ppdPageSize()   - Get the page size record for the given size.
 *   ppdPageWidth()  - Get the page width for the given size.
 *   ppdPageLength() - Get the page length for the given size.
 */

/*
 * Include necessary headers...
 */

#include "ppd.h"
#include "string.h"
#include <ctype.h>


/*
 * 'ppdPageSize()' - Get the page size record for the given size.
 */

ppd_size_t *				/* O - Size record for page or NULL */
ppdPageSize(ppd_file_t *ppd,		/* I - PPD file record */
            const char *name)		/* I - Size name */
{
  int		i;			/* Looping var */
  ppd_size_t	*size;			/* Current page size */
  float		w, l;			/* Width and length of page */
  char		*nameptr;		/* Pointer into name */
  struct lconv	*loc;			/* Locale data */
  ppd_coption_t	*coption;		/* Custom option for page size */
  ppd_cparam_t	*cparam;		/* Custom option parameter */


  if (!ppd)
    return (NULL);

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
        return (NULL);

     /*
      * Variable size; size name can be one of the following:
      *
      *    Custom.WIDTHxLENGTHin    - Size in inches
      *    Custom.WIDTHxLENGTHcm    - Size in centimeters
      *    Custom.WIDTHxLENGTHmm    - Size in millimeters
      *    Custom.WIDTHxLENGTH[pt]  - Size in points
      */

      loc = localeconv();
      w   = (float)_cupsStrScand(name + 7, &nameptr, loc);
      if (!nameptr || *nameptr != 'x')
        return (NULL);

      l = (float)_cupsStrScand(nameptr + 1, &nameptr, loc);
      if (!nameptr)
        return (NULL);

      if (!strcasecmp(nameptr, "in"))
      {
        w *= 72.0f;
	l *= 72.0f;
      }
      else if (!strcasecmp(nameptr, "ft"))
      {
        w *= 12.0f * 72.0f;
	l *= 12.0f * 72.0f;
      }
      else if (!strcasecmp(nameptr, "mm"))
      {
        w *= 72.0f / 25.4f;
        l *= 72.0f / 25.4f;
      }
      else if (!strcasecmp(nameptr, "cm"))
      {
        w *= 72.0f / 2.54f;
        l *= 72.0f / 2.54f;
      }
      else if (!strcasecmp(nameptr, "m"))
      {
        w *= 72.0f / 0.0254f;
        l *= 72.0f / 0.0254f;
      }

      size->width  = w;
      size->length = l;
      size->left   = ppd->custom_margins[0];
      size->bottom = ppd->custom_margins[1];
      size->right  = w - ppd->custom_margins[2];
      size->top    = l - ppd->custom_margins[3];

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

      return (size);
    }
    else
    {
     /*
      * Lookup by name...
      */

      for (i = ppd->num_sizes, size = ppd->sizes; i > 0; i --, size ++)
	if (!strcmp(name, size->name))
          return (size);
    }
  }
  else
  {
   /*
    * Find default...
    */

    for (i = ppd->num_sizes, size = ppd->sizes; i > 0; i --, size ++)
      if (size->marked)
        return (size);
  }

  return (NULL);
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
 * End of "$Id$".
 */
