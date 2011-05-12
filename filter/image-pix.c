/*
 * "$Id: image-pix.c 7221 2008-01-16 22:20:08Z mike $"
 *
 *   Alias PIX image routines for CUPS.
 *
 *   Copyright 2007-2011 by Apple Inc.
 *   Copyright 1993-2007 by Easy Software Products.
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
 *   _cupsImageReadPIX() - Read a PIX image file.
 *   read_short()        - Read a 16-bit integer.
 */

/*
 * Include necessary headers...
 */

#include "image-private.h"


/*
 * Local functions...
 */

static short	read_short(FILE *fp);


/*
 * '_cupsImageReadPIX()' - Read a PIX image file.
 */

int					/* O - Read status */
_cupsImageReadPIX(
    cups_image_t    *img,		/* IO - cupsImage */
    FILE            *fp,		/* I - cupsImage file */
    cups_icspace_t  primary,		/* I - Primary choice for colorspace */
    cups_icspace_t  secondary,		/* I - Secondary choice for colorspace */
    int             saturation,		/* I - Color saturation (%) */
    int             hue,		/* I - Color hue (degrees) */
    const cups_ib_t *lut)		/* I - Lookup table for gamma/brightness */
{
  short		width,			/* Width of image */
		height,			/* Height of image */
		depth;			/* Depth of image (bits) */
  int		count,			/* Repetition count */
		bpp,			/* Bytes per pixel */
		x, y;			/* Looping vars */
  cups_ib_t	r, g, b;		/* Red, green/gray, blue values */
  cups_ib_t	*in,			/* Input pixels */
		*out,			/* Output pixels */
		*ptr;			/* Pointer into pixels */


 /*
  * Get the image dimensions and setup the image...
  */

  width  = read_short(fp);
  height = read_short(fp);
  read_short(fp);
  read_short(fp);
  depth  = read_short(fp);

 /*
  * Check the dimensions of the image.  Since the short values used for the
  * width and height cannot exceed CUPS_IMAGE_MAX_WIDTH or
  * CUPS_IMAGE_MAX_HEIGHT, we just need to verify they are positive integers.
  */

  if (width <= 0 || height <= 0 ||
      (depth != 8 && depth != 24))
  {
    fprintf(stderr, "DEBUG: Bad PIX image dimensions %dx%dx%d\n",
            width, height, depth);
    fclose(fp);
    return (1);
  }

  if (depth == 8)
    img->colorspace = secondary;
  else
    img->colorspace = (primary == CUPS_IMAGE_RGB_CMYK) ? CUPS_IMAGE_RGB : primary;

  img->xsize = width;
  img->ysize = height;

  cupsImageSetMaxTiles(img, 0);

  bpp = cupsImageGetDepth(img);

  if ((in = malloc(img->xsize * (depth / 8))) == NULL)
  {
    fputs("DEBUG: Unable to allocate memory!\n", stderr);
    fclose(fp);
    return (1);
  }

  if ((out = malloc(img->xsize * bpp)) == NULL)
  {
    fputs("DEBUG: Unable to allocate memory!\n", stderr);
    fclose(fp);
    free(in);
    return (1);
  }

 /*
  * Read the image data...
  */

  if (depth == 8)
  {
    for (count = 0, y = 0, g = 0; y < img->ysize; y ++)
    {
      if (img->colorspace == CUPS_IMAGE_WHITE)
        ptr = out;
      else
        ptr = in;

      for (x = img->xsize; x > 0; x --, count --)
      {
        if (count == 0)
	{
          count = getc(fp);
	  g     = getc(fp);
	}

        *ptr++ = g;
      }

      if (img->colorspace != CUPS_IMAGE_WHITE)
	switch (img->colorspace)
	{
	  default :
	      cupsImageWhiteToRGB(in, out, img->xsize);
	      break;
	  case CUPS_IMAGE_BLACK :
	      cupsImageWhiteToBlack(in, out, img->xsize);
	      break;
	  case CUPS_IMAGE_CMY :
	      cupsImageWhiteToCMY(in, out, img->xsize);
	      break;
	  case CUPS_IMAGE_CMYK :
	      cupsImageWhiteToCMYK(in, out, img->xsize);
	      break;
	}

      if (lut)
	cupsImageLut(out, img->xsize * bpp, lut);

      _cupsImagePutRow(img, 0, y, img->xsize, out);
    }
  }
  else
  {
    for (count = 0, y = 0, r = 0, g = 0, b = 0; y < img->ysize; y ++)
    {
      ptr = in;

      for (x = img->xsize; x > 0; x --, count --)
      {
        if (count == 0)
	{
          count = getc(fp);
	  b     = getc(fp);
	  g     = getc(fp);
	  r     = getc(fp);
	}

        *ptr++ = r;
        *ptr++ = g;
        *ptr++ = b;
      }

      if (saturation != 100 || hue != 0)
	cupsImageRGBAdjust(in, img->xsize, saturation, hue);

      switch (img->colorspace)
      {
	default :
	    break;

	case CUPS_IMAGE_WHITE :
	    cupsImageRGBToWhite(in, out, img->xsize);
	    break;
	case CUPS_IMAGE_RGB :
	    cupsImageRGBToWhite(in, out, img->xsize);
	    break;
	case CUPS_IMAGE_BLACK :
	    cupsImageRGBToBlack(in, out, img->xsize);
	    break;
	case CUPS_IMAGE_CMY :
	    cupsImageRGBToCMY(in, out, img->xsize);
	    break;
	case CUPS_IMAGE_CMYK :
	    cupsImageRGBToCMYK(in, out, img->xsize);
	    break;
      }

      if (lut)
	cupsImageLut(out, img->xsize * bpp, lut);

      _cupsImagePutRow(img, 0, y, img->xsize, out);
    }
  }

  fclose(fp);
  free(in);
  free(out);

  return (0);
}


/*
 * 'read_short()' - Read a 16-bit integer.
 */

static short				/* O - Value from file */
read_short(FILE *fp)			/* I - File to read from */
{
  int	ch;				/* Character from file */


  ch = getc(fp);
  return ((ch << 8) | getc(fp));
}


/*
 * End of "$Id: image-pix.c 7221 2008-01-16 22:20:08Z mike $".
 */
