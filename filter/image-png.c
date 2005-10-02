/*
 * "$Id$"
 *
 *   PNG image routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-2005 by Easy Software Products.
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
 *   _cupsImageReadPNG() - Read a PNG image file.
 */

/*
 * Include necessary headers...
 */

#include "image-private.h"

#if defined(HAVE_LIBPNG) && defined(HAVE_LIBZ)
#  include <png.h>	/* Portable Network Graphics (PNG) definitions */


/*
 * '_cupsImageReadPNG()' - Read a PNG image file.
 */

int					/* O - Read status */
_cupsImageReadPNG(
    cups_image_t    *img,		/* IO - cupsImage */
    FILE            *fp,		/* I - cupsImage file */
    cups_icspace_t  primary,		/* I - Primary choice for colorspace */
    cups_icspace_t  secondary,		/* I - Secondary choice for colorspace */
    int             saturation,		/* I - Color saturation (%) */
    int             hue,		/* I - Color hue (degrees) */
    const cups_ib_t *lut)		/* I - Lookup table for gamma/brightness */
{
  int		y;			/* Looping var */
  png_structp	pp;			/* PNG read pointer */
  png_infop	info;			/* PNG info pointers */
  int		bpp;			/* Bytes per pixel */
  int		pass,			/* Current pass */
		passes;			/* Number of passes required */
  cups_ib_t	*in,			/* Input pixels */
		*inptr,			/* Pointer into pixels */
		*out;			/* Output pixels */
  png_color_16	bg;			/* Background color */


 /*
  * Setup the PNG data structures...
  */

  pp   = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  info = png_create_info_struct(pp);

 /*
  * Initialize the PNG read "engine"...
  */

  png_init_io(pp, fp);

 /*
  * Get the image dimensions and load the output image...
  */

  png_read_info(pp, info);

  fprintf(stderr, "DEBUG: PNG image: %dx%dx%d, color_type=%x (%s%s%s)\n",
          (int)info->width, (int)info->height, info->bit_depth, info->color_type,
	  (info->color_type & PNG_COLOR_MASK_COLOR) ? "RGB" : "GRAYSCALE",
	  (info->color_type & PNG_COLOR_MASK_ALPHA) ? "+ALPHA" : "",
	  (info->color_type & PNG_COLOR_MASK_PALETTE) ? "+PALETTE" : "");

  if (info->color_type & PNG_COLOR_MASK_PALETTE)
    png_set_expand(pp);
  else if (info->bit_depth < 8)
  {
    png_set_packing(pp);
    png_set_expand(pp);
  }
  else if (info->bit_depth == 16)
    png_set_strip_16(pp);

  if (info->color_type & PNG_COLOR_MASK_COLOR)
    img->colorspace = (primary == CUPS_IMAGE_RGB_CMYK) ? CUPS_IMAGE_RGB : primary;
  else
    img->colorspace = secondary;

  if (info->width == 0 || info->width > CUPS_IMAGE_MAX_WIDTH ||
      info->height == 0 || info->height > CUPS_IMAGE_MAX_HEIGHT)
  {
    fprintf(stderr, "ERROR: PNG image has invalid dimensions %ux%u!\n",
            (unsigned)info->width, (unsigned)info->height);
    fclose(fp);
    return (1);
  }

  img->xsize = info->width;
  img->ysize = info->height;

  if (info->valid & PNG_INFO_pHYs &&
      info->phys_unit_type == PNG_RESOLUTION_METER)
  {
    img->xppi = (int)((float)info->x_pixels_per_unit * 0.0254);
    img->yppi = (int)((float)info->y_pixels_per_unit * 0.0254);

    if (img->xppi == 0 || img->yppi == 0)
    {
      fprintf(stderr, "ERROR: PNG image has invalid resolution %dx%d PPI\n",
              img->xppi, img->yppi);

      img->xppi = img->yppi = 128;
    }
  }

  cupsImageSetMaxTiles(img, 0);

  passes = png_set_interlace_handling(pp);

 /*
  * Handle transparency...
  */

  if (png_get_valid(pp, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(pp);

  bg.red   = 65535;
  bg.green = 65535;
  bg.blue  = 65535;

  png_set_background(pp, &bg, PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);

  if (passes == 1)
  {
   /*
    * Load one row at a time...
    */

    if (info->color_type == PNG_COLOR_TYPE_GRAY ||
	info->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
      in = malloc(img->xsize);
    else
      in = malloc(img->xsize * 3);
  }
  else
  {
   /*
    * Interlaced images must be loaded all at once...
    */

    if (info->color_type == PNG_COLOR_TYPE_GRAY ||
	info->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
      in = malloc(img->xsize * img->ysize);
    else
      in = malloc(img->xsize * img->ysize * 3);
  }

  bpp = cupsImageGetDepth(img);
  out = malloc(img->xsize * bpp);

 /*
  * Read the image, interlacing as needed...
  */

  for (pass = 1; pass <= passes; pass ++)
    for (inptr = in, y = 0; y < img->ysize; y ++)
    {
      png_read_row(pp, (png_bytep)inptr, NULL);

      if (pass == passes)
      {
       /*
        * Output this row...
	*/

	if (info->color_type & PNG_COLOR_MASK_COLOR)
	{
	  if ((saturation != 100 || hue != 0) && bpp > 1)
	    cupsImageRGBAdjust(inptr, img->xsize, saturation, hue);

	  switch (img->colorspace)
	  {
	    case CUPS_IMAGE_WHITE :
		cupsImageRGBToWhite(inptr, out, img->xsize);
		break;
	    case CUPS_IMAGE_RGB :
	    case CUPS_IMAGE_RGB_CMYK :
		memcpy(out, inptr, img->xsize * 3);
		break;
	    case CUPS_IMAGE_BLACK :
		cupsImageRGBToBlack(inptr, out, img->xsize);
		break;
	    case CUPS_IMAGE_CMY :
		cupsImageRGBToCMY(inptr, out, img->xsize);
		break;
	    case CUPS_IMAGE_CMYK :
		cupsImageRGBToCMYK(inptr, out, img->xsize);
		break;
	  }
	}
	else
	{
	  switch (img->colorspace)
	  {
	    case CUPS_IMAGE_WHITE :
		memcpy(out, inptr, img->xsize);
		break;
	    case CUPS_IMAGE_RGB :
	    case CUPS_IMAGE_RGB_CMYK :
		cupsImageWhiteToRGB(inptr, out, img->xsize);
		break;
	    case CUPS_IMAGE_BLACK :
		cupsImageWhiteToBlack(inptr, out, img->xsize);
		break;
	    case CUPS_IMAGE_CMY :
		cupsImageWhiteToCMY(inptr, out, img->xsize);
		break;
	    case CUPS_IMAGE_CMYK :
		cupsImageWhiteToCMYK(inptr, out, img->xsize);
		break;
	  }
	}

	if (lut)
	  cupsImageLut(out, img->xsize * bpp, lut);

	_cupsImagePutRow(img, 0, y, img->xsize, out);
      }

      if (passes > 1)
      {
	if (info->color_type & PNG_COLOR_MASK_COLOR)
          inptr += img->xsize * 3;
	else
          inptr += img->xsize;
      }
    }

  png_read_end(pp, info);
  png_read_destroy(pp, info, NULL);

  fclose(fp);
  free(in);
  free(out);

  return (0);
}
#endif /* HAVE_LIBPNG && HAVE_LIBZ */


/*
 * End of "$Id$".
 */
