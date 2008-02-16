/*
 * "$Id$"
 *
 *   RGB color separation code for CUPS.
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
 *   cupsRGBDelete() - Delete a color separation.
 *   cupsRGBDoGray() - Do a grayscale separation...
 *   cupsRGBDoRGB()  - Do a RGB separation...
 *   cupsRGBLoad()   - Load a RGB color profile from a PPD file.
 *   cupsRGBNew()    - Create a new RGB color separation.
 */

/*
 * Include necessary headers.
 */

#include "driver.h"


/*
 * 'cupsRGBDelete()' - Delete a color separation.
 */

void
cupsRGBDelete(cups_rgb_t *rgbptr)	/* I - Color separation */
{
  if (rgbptr == NULL)
    return;

  free(rgbptr->colors[0][0][0]);
  free(rgbptr->colors[0][0]);
  free(rgbptr->colors[0]);
  free(rgbptr->colors);
  free(rgbptr);
}


/*
 * 'cupsRGBDoGray()' - Do a grayscale separation...
 */

void
cupsRGBDoGray(cups_rgb_t          *rgbptr,
					/* I - Color separation */
	      const unsigned char *input,
					/* I - Input grayscale pixels */
	      unsigned char       *output,
					/* O - Output Device-N pixels */
	      int                 num_pixels)
					/* I - Number of pixels */
{
  int			i;		/* Looping var */
  int			lastgray;	/* Previous grayscale */
  int			xs, ys, zs,	/* Current RGB row offsets */
			g, gi, gm0, gm1;/* Current gray index and multipliers ... */
  const unsigned char	*color;		/* Current color data */
  int			tempg;		/* Current separation color */
  int			rgbsize;	/* Separation data size */


 /*
  * Range check input...
  */

  if (!rgbptr || !input || !output || num_pixels <= 0)
    return;

 /*
  * Initialize variables used for the duration of the separation...
  */

  lastgray = -1;
  rgbsize  = rgbptr->num_channels;
  xs       = rgbptr->cube_size * rgbptr->cube_size * rgbptr->num_channels;
  ys       = rgbptr->cube_size * rgbptr->num_channels;
  zs       = rgbptr->num_channels;

 /*
  * Loop through it all...
  */

  while (num_pixels > 0)
  {
   /*
    * See if the next pixel is a cached value...
    */

    num_pixels --;

    g = cups_srgb_lut[*input++];

    if (g == lastgray)
    {
     /*
      * Copy previous color and continue...
      */

      memcpy(output, output - rgbptr->num_channels, rgbsize);

      output += rgbptr->num_channels;
      continue;
    }
    else if (g == 0x00 && rgbptr->cache_init)
    {
     /*
      * Copy black color and continue...
      */

      memcpy(output, rgbptr->black, rgbsize);

      output += rgbptr->num_channels;
      continue;
    }
    else if (g == 0xff && rgbptr->cache_init)
    {
     /*
      * Copy white color and continue...
      */

      memcpy(output, rgbptr->white, rgbsize);

      output += rgbptr->num_channels;
      continue;
    }

   /*
    * Nope, figure this one out on our own...
    */

    gi  = rgbptr->cube_index[g];
    gm0 = rgbptr->cube_mult[g];
    gm1 = 256 - gm0;

    color = rgbptr->colors[gi][gi][gi];

    for (i = 0; i < rgbptr->num_channels; i ++, color ++)
    {
      tempg = (color[0] * gm0 + color[xs + ys + zs] * gm1) / 256;

      if (tempg > 255)
        *output++ = 255;
      else if (tempg < 0)
        *output++ = 0;
      else
        *output++ = tempg;
    }
  }
}


/*
 * 'cupsRGBDoRGB()' - Do a RGB separation...
 */

void
cupsRGBDoRGB(cups_rgb_t          *rgbptr,
					/* I - Color separation */
	     const unsigned char *input,
					/* I - Input RGB pixels */
	     unsigned char       *output,
					/* O - Output Device-N pixels */
	     int                 num_pixels)
					/* I - Number of pixels */
{
  int			i;		/* Looping var */
  int			rgb,		/* Current RGB color */
			lastrgb;	/* Previous RGB color */
  int			r, ri, rm0, rm1, rs,
					/* Current red index, multipliexs, and row offset */
			g, gi, gm0, gm1, gs,
					/* Current green ... */
			b, bi, bm0, bm1, bs;
					/* Current blue ... */
  const unsigned char	*color;		/* Current color data */
  int			tempr,		/* Current separation colors */
			tempg,		/* ... */
			tempb ;		/* ... */
  int			rgbsize;	/* Separation data size */


 /*
  * Range check input...
  */

  if (!rgbptr || !input || !output || num_pixels <= 0)
    return;

 /*
  * Initialize variables used for the duration of the separation...
  */

  lastrgb = -1;
  rgbsize = rgbptr->num_channels;
  rs      = rgbptr->cube_size * rgbptr->cube_size * rgbptr->num_channels;
  gs      = rgbptr->cube_size * rgbptr->num_channels;
  bs      = rgbptr->num_channels;

 /*
  * Loop through it all...
  */

  while (num_pixels > 0)
  {
   /*
    * See if the next pixel is a cached value...
    */

    num_pixels --;

    r   = cups_srgb_lut[*input++];
    g   = cups_srgb_lut[*input++];
    b   = cups_srgb_lut[*input++];
    rgb = (((r << 8) | g) << 8) | b;

    if (rgb == lastrgb)
    {
     /*
      * Copy previous color and continue...
      */

      memcpy(output, output - rgbptr->num_channels, rgbsize);

      output += rgbptr->num_channels;
      continue;
    }
    else if (rgb == 0x000000 && rgbptr->cache_init)
    {
     /*
      * Copy black color and continue...
      */

      memcpy(output, rgbptr->black, rgbsize);

      output += rgbptr->num_channels;
      continue;
    }
    else if (rgb == 0xffffff && rgbptr->cache_init)
    {
     /*
      * Copy white color and continue...
      */

      memcpy(output, rgbptr->white, rgbsize);

      output += rgbptr->num_channels;
      continue;
    }

   /*
    * Nope, figure this one out on our own...
    */

    ri  = rgbptr->cube_index[r];
    rm0 = rgbptr->cube_mult[r];
    rm1 = 256 - rm0;

    gi  = rgbptr->cube_index[g];
    gm0 = rgbptr->cube_mult[g];
    gm1 = 256 - gm0;

    bi  = rgbptr->cube_index[b];
    bm0 = rgbptr->cube_mult[b];
    bm1 = 256 - bm0;

    color = rgbptr->colors[ri][gi][bi];

    for (i = rgbptr->num_channels; i > 0; i --, color ++)
    {
      tempb = (color[0] * bm0 + color[bs] * bm1) / 256;
      tempg = tempb  * gm0;
      tempb = (color[gs] * gm0 + color[gs + bs] * bm1) / 256;
      tempg = (tempg + tempb  * gm1) / 256;

      tempr = tempg * rm0;

      tempb = (color[rs] * bm0 + color[rs + bs] * bm1) / 256;
      tempg = tempb  * gm0;
      tempb = (color[rs + gs] * bm0 + color[rs + gs + bs] * bm1) / 256;
      tempg = (tempg + tempb  * gm1) / 256;

      tempr = (tempr + tempg * rm1) / 256;

      if (tempr > 255)
        *output++ = 255;
      else if (tempr < 0)
        *output++ = 0;
      else
        *output++ = tempr;
    }
  }
}


/*
 * 'cupsRGBLoad()' - Load a RGB color profile from a PPD file.
 */

cups_rgb_t *				/* O - New color profile */
cupsRGBLoad(ppd_file_t *ppd,		/* I - PPD file */
            const char *colormodel,	/* I - Color model */
            const char *media,		/* I - Media type */
            const char *resolution)	/* I - Resolution */
{
  int		i,			/* Looping var */
		cube_size,		/* Size of color lookup cube */
		num_channels,		/* Number of color channels */
		num_samples;		/* Number of color samples */
  cups_sample_t	*samples;		/* Color samples */
  float		values[7];		/* Color sample values */
  char		spec[PPD_MAX_NAME];	/* Profile name */
  ppd_attr_t	*attr;			/* Attribute from PPD file */
  cups_rgb_t	*rgbptr;		/* RGB color profile */


 /*
  * Find the following attributes:
  *
  *    cupsRGBProfile  - Specifies the cube size, number of channels, and
  *                      number of samples
  *    cupsRGBSample   - Specifies an RGB to CMYK color sample
  */

  if ((attr = cupsFindAttr(ppd, "cupsRGBProfile", colormodel, media,
                           resolution, spec, sizeof(spec))) == NULL)
  {
    fputs("DEBUG2: No cupsRGBProfile attribute found for the current settings!\n", stderr);
    return (NULL);
  }

  if (!attr->value || sscanf(attr->value, "%d%d%d", &cube_size, &num_channels,
                             &num_samples) != 3)
  {
    fprintf(stderr, "ERROR: Bad cupsRGBProfile attribute \'%s\'!\n",
            attr->value ? attr->value : "(null)");
    return (NULL);
  }

  if (cube_size < 2 || cube_size > 16 ||
      num_channels < 1 || num_channels > CUPS_MAX_RGB ||
      num_samples != (cube_size * cube_size * cube_size))
  {
    fprintf(stderr, "ERROR: Bad cupsRGBProfile attribute \'%s\'!\n",
            attr->value);
    return (NULL);
  }

 /*
  * Allocate memory for the samples and read them...
  */

  if ((samples = calloc(num_samples, sizeof(cups_sample_t))) == NULL)
  {
    fputs("ERROR: Unable to allocate memory for RGB profile!\n", stderr);
    return (NULL);
  }

 /*
  * Read all of the samples...
  */

  for (i = 0; i < num_samples; i ++)
    if ((attr = ppdFindNextAttr(ppd, "cupsRGBSample", spec)) == NULL)
      break;
    else if (!attr->value)
    {
      fputs("ERROR: Bad cupsRGBSample value!\n", stderr);
      break;
    }
    else if (sscanf(attr->value, "%f%f%f%f%f%f%f", values + 0,
                    values + 1, values + 2, values + 3, values + 4, values + 5,
                    values + 6) != (3 + num_channels))
    {
      fputs("ERROR: Bad cupsRGBSample value!\n", stderr);
      break;
    }
    else
    {
      samples[i].rgb[0]    = (int)(255.0 * values[0] + 0.5);
      samples[i].rgb[1]    = (int)(255.0 * values[1] + 0.5);
      samples[i].rgb[2]    = (int)(255.0 * values[2] + 0.5);
      samples[i].colors[0] = (int)(255.0 * values[3] + 0.5);
      if (num_channels > 1)
	samples[i].colors[1] = (int)(255.0 * values[4] + 0.5);
      if (num_channels > 2)
	samples[i].colors[2] = (int)(255.0 * values[5] + 0.5);
      if (num_channels > 3)
	samples[i].colors[3] = (int)(255.0 * values[6] + 0.5);
    }

 /*
  * If everything went OK, create the color profile...
  */

  if (i == num_samples)
    rgbptr = cupsRGBNew(num_samples, samples, cube_size, num_channels);
  else
    rgbptr = NULL;

 /*
  * Free the temporary sample array and return...
  */

  free(samples);

  return (rgbptr);
}


/*
 * 'cupsRGBNew()' - Create a new RGB color separation.
 */

cups_rgb_t *				/* O - New color separation or NULL */
cupsRGBNew(int           num_samples,	/* I - Number of samples */
	   cups_sample_t *samples,	/* I - Samples */
	   int           cube_size,	/* I - Size of LUT cube */
           int           num_channels)	/* I - Number of color components */
{
  cups_rgb_t		*rgbptr;	/* New color separation */
  int			i;		/* Looping var */
  int			r, g, b;	/* Current RGB */
  int			tempsize;	/* Sibe of main arrays */
  unsigned char		*tempc;		/* Pointer for C arrays */
  unsigned char		**tempb ;	/* Pointer for Z arrays */
  unsigned char		***tempg;	/* Pointer for Y arrays */
  unsigned char		****tempr;	/* Pointer for X array */
  unsigned char		rgb[3];		/* Temporary RGB value */


 /*
  * Range-check the input...
  */

  if (!samples || num_samples != (cube_size * cube_size * cube_size) ||
      num_channels <= 0 || num_channels > CUPS_MAX_RGB)
    return (NULL);

 /*
  * Allocate memory for the separation...
  */

  if ((rgbptr = calloc(1, sizeof(cups_rgb_t))) == NULL)
    return (NULL);

 /*
  * Allocate memory for the samples and the LUT cube...
  */

  tempsize = cube_size * cube_size * cube_size;	/* FUTURE: num_samples < cs^3 */

  tempc = calloc(tempsize, num_channels);
  tempb = calloc(tempsize, sizeof(unsigned char *));
  tempg = calloc(cube_size * cube_size, sizeof(unsigned char **));
  tempr = calloc(cube_size, sizeof(unsigned char ***));

  if (tempc == NULL || tempb  == NULL || tempg == NULL || tempr == NULL)
  {
    free(rgbptr);

    if (tempc)
      free(tempc);

    if (tempb)
      free(tempb);

    if (tempg)
      free(tempg);

    if (tempr)
      free(tempr);

    return (NULL);
  }

 /*
  * Fill in the arrays...
  */

  for (i = 0, r = 0; r < cube_size; r ++)
  {
    tempr[r] = tempg + r * cube_size;

    for (g = 0; g < cube_size; g ++)
    {
      tempr[r][g] = tempb + i;

      for (b = 0; b < cube_size; b ++, i ++)
        tempr[r][g][b] = tempc + i * num_channels;
    }
  }

  for (i = 0; i < num_samples; i ++)
  {
    r = samples[i].rgb[0] * (cube_size - 1) / 255;
    g = samples[i].rgb[1] * (cube_size - 1) / 255;
    b = samples[i].rgb[2] * (cube_size - 1) / 255;

    memcpy(tempr[r][g][b], samples[i].colors, num_channels);
  }

  rgbptr->cube_size    = cube_size;
  rgbptr->num_channels = num_channels;
  rgbptr->colors       = tempr;

 /*
  * Generate the lookup tables for the cube indices and multipliers...
  */

  for (i = 0; i < 256; i ++)
  {
    rgbptr->cube_index[i] = i * (cube_size - 1) / 256;

    if (i == 0)
      rgbptr->cube_mult[i] = 256;
    else
      rgbptr->cube_mult[i] = 255 - ((i * (cube_size - 1)) & 255);
  }

 /*
  * Generate the black and white cache values for the separation...
  */

  rgb[0] = 0;
  rgb[1] = 0;
  rgb[2] = 0;

  cupsRGBDoRGB(rgbptr, rgb, rgbptr->black, 1);

  rgb[0] = 255;
  rgb[1] = 255;
  rgb[2] = 255;

  cupsRGBDoRGB(rgbptr, rgb, rgbptr->white, 1);

  rgbptr->cache_init = 1;

 /*
  * Return the separation...
  */

  return (rgbptr);
}


/*
 * End of "$Id$".
 */
