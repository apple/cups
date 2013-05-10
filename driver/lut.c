/*
 * "$Id$"
 *
 *   Lookup table routines for CUPS.
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1993-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cupsLutDelete() - Free the memory used by a lookup table.
 *   cupsLutLoad()   - Load a LUT from a PPD file.
 *   cupsLutNew()    - Make a lookup table from a list of pixel values.
 */

/*
 * Include necessary headers.
 */

#include "driver.h"
#include <math.h>


/*
 * 'cupsLutDelete()' - Free the memory used by a lookup table.
 */

void
cupsLutDelete(cups_lut_t *lut)		/* I - Lookup table to free */
{
  if (lut != NULL)
    free(lut);
}


/*
 * 'cupsLutLoad()' - Load a LUT from a PPD file.
 */

cups_lut_t *				/* O - New lookup table */
cupsLutLoad(ppd_file_t *ppd,		/* I - PPD file */
            const char *colormodel,	/* I - Color model */
            const char *media,		/* I - Media type */
            const char *resolution,	/* I - Resolution */
	    const char *ink)		/* I - Ink name */
{
  char		name[PPD_MAX_NAME],	/* Attribute name */
		spec[PPD_MAX_NAME];	/* Attribute spec */
  ppd_attr_t	*attr;			/* Attribute */
  int		nvals;			/* Number of values */
  float		vals[4];		/* Values */


 /*
  * Range check input...
  */

  if (!ppd || !colormodel || !media || !resolution || !ink)
    return (NULL);

 /*
  * Try to find the LUT values...
  */

  snprintf(name, sizeof(name), "cups%sDither", ink);

  if ((attr = cupsFindAttr(ppd, name, colormodel, media, resolution, spec,
                           sizeof(spec))) == NULL)
    attr = cupsFindAttr(ppd, "cupsAllDither", colormodel, media,
                        resolution, spec, sizeof(spec));

  if (!attr)
    return (NULL);

  vals[0] = 0.0;
  vals[1] = 0.0;
  vals[2] = 0.0;
  vals[3] = 0.0;
  nvals   = sscanf(attr->value, "%f%f%f", vals + 1, vals + 2, vals + 3) + 1;

  fprintf(stderr, "DEBUG: Loaded LUT %s from PPD with values [%.3f %.3f %.3f %.3f]\n",
          name, vals[0], vals[1], vals[2], vals[3]);

  return (cupsLutNew(nvals, vals));
}


/*
 * 'cupsLutNew()' - Make a lookup table from a list of pixel values.
 *
 * Returns a pointer to the lookup table on success, NULL on failure.
 */

cups_lut_t *				/* O - New lookup table */
cupsLutNew(int         num_values,	/* I - Number of values */
	   const float *values)		/* I - Lookup table values */
{
  int		pixel;			/* Pixel value */
  cups_lut_t	*lut;			/* Lookup table */
  int		start,			/* Start value */
		end,			/* End value */
		maxval;			/* Maximum value */


 /*
  * Range check...
  */

  if (!num_values || !values)
    return (NULL);

 /*
  * Allocate memory for the lookup table...
  */

  if ((lut = (cups_lut_t *)calloc((CUPS_MAX_LUT + 1),
                                  sizeof(cups_lut_t))) == NULL)
    return (NULL);

 /*
  * Generate the dither lookup table.  The pixel values are roughly
  * defined by a piecewise linear curve that has an intensity value
  * at each output pixel.  This isn't perfectly accurate, but it's
  * close enough for jazz.
  */

  maxval = CUPS_MAX_LUT / values[num_values - 1];

  for (start = 0; start <= CUPS_MAX_LUT; start ++)
    lut[start].intensity = start * maxval / CUPS_MAX_LUT;

  for (pixel = 0; pixel < num_values; pixel ++)
  {
   /*
    * Select start and end values for this pixel...
    */

    if (pixel == 0)
      start = 0;
    else
      start = (int)(0.5 * maxval * (values[pixel - 1] +
                                    values[pixel])) + 1;

    if (start < 0)
      start = 0;
    else if (start > CUPS_MAX_LUT)
      start = CUPS_MAX_LUT;

    if (pixel == (num_values - 1))
      end = CUPS_MAX_LUT;
    else
      end = (int)(0.5 * maxval * (values[pixel] + values[pixel + 1]));

    if (end < 0)
      end = 0;
    else if (end > CUPS_MAX_LUT)
      end = CUPS_MAX_LUT;

    if (start == end)
      break;

   /*
    * Generate lookup values and errors for each pixel.
    */

    while (start <= end)
    {
      lut[start].pixel = pixel;
      if (start == 0)
        lut[0].error = 0;
      else
        lut[start].error = start - maxval * values[pixel];

      start ++;
    }
  }

 /*
  * Show the lookup table...
  */

  for (start = 0; start <= CUPS_MAX_LUT; start += CUPS_MAX_LUT / 15)
    fprintf(stderr, "DEBUG: %d = %d/%d/%d\n", start, lut[start].intensity,
            lut[start].pixel, lut[start].error);

 /*
  * Return the lookup table...
  */

  return (lut);
}


/*
 * End of "$Id$".
 */
