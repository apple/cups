/*
 * "$Id: image-png.c,v 1.11.2.1 2002/01/02 18:04:45 mike Exp $"
 *
 *   PNG image routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-2002 by Easy Software Products.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   ImageReadPNG() - Read a PNG image file.
 */

/*
 * Include necessary headers...
 */

#include "image.h"

#if defined(HAVE_LIBPNG) && defined(HAVE_LIBZ)
#include <png.h>	/* Portable Network Graphics (PNG) definitions */


/*
 * 'ImageReadPNG()' - Read a PNG image file.
 */

int					/* O - Read status */
ImageReadPNG(image_t    *img,		/* IO - Image */
             FILE       *fp,		/* I - Image file */
             int        primary,	/* I - Primary choice for colorspace */
             int        secondary,	/* I - Secondary choice for colorspace */
             int        saturation,	/* I - Color saturation (%) */
             int        hue,		/* I - Color hue (degrees) */
	     const ib_t *lut)		/* I - Lookup table for gamma/brightness */
{
  int		y;			/* Looping var */
  png_structp	pp;			/* PNG read pointer */
  png_infop	info;			/* PNG info pointers */
  int		bpp;			/* Bytes per pixel */
  int		pass,			/* Current pass */
		passes;			/* Number of passes required */
  ib_t		*in,			/* Input pixels */
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

  if (info->color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_expand(pp);

  if (info->color_type == PNG_COLOR_TYPE_GRAY ||
      info->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    img->colorspace = secondary;
  else
    img->colorspace = primary;

  img->xsize = info->width;
  img->ysize = info->height;

  if (info->valid & PNG_INFO_pHYs &&
      info->phys_unit_type == PNG_RESOLUTION_METER)
  {
    img->xppi = (int)((float)info->x_pixels_per_unit * 0.0254);
    img->yppi = (int)((float)info->y_pixels_per_unit * 0.0254);
  }

  ImageSetMaxTiles(img, 0);

  if (info->bit_depth < 8)
  {
    png_set_packing(pp);

    if (info->valid & PNG_INFO_sBIT)
      png_set_shift(pp, &(info->sig_bit));
  }
  else if (info->bit_depth == 16)
    png_set_strip_16(pp);

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

  bpp = ImageGetDepth(img);
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

	if (info->color_type == PNG_COLOR_TYPE_GRAY ||
	     info->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
	{
	  switch (img->colorspace)
	  {
	    case IMAGE_WHITE :
		memcpy(out, inptr, img->xsize);
		break;
	    case IMAGE_RGB :
		ImageWhiteToRGB(inptr, out, img->xsize);
		break;
	    case IMAGE_BLACK :
		ImageWhiteToBlack(inptr, out, img->xsize);
		break;
	    case IMAGE_CMY :
		ImageWhiteToCMY(inptr, out, img->xsize);
		break;
	    case IMAGE_CMYK :
		ImageWhiteToCMYK(inptr, out, img->xsize);
		break;
	  }
	}
	else
	{
	  if ((saturation != 100 || hue != 0) && bpp > 1)
	    ImageRGBAdjust(inptr, img->xsize, saturation, hue);

	  switch (img->colorspace)
	  {
	    case IMAGE_WHITE :
		ImageRGBToWhite(inptr, out, img->xsize);
		break;
	    case IMAGE_RGB :
		memcpy(out, inptr, img->xsize * 3);
		break;
	    case IMAGE_BLACK :
		ImageRGBToBlack(inptr, out, img->xsize);
		break;
	    case IMAGE_CMY :
		ImageRGBToCMY(inptr, out, img->xsize);
		break;
	    case IMAGE_CMYK :
		ImageRGBToCMYK(inptr, out, img->xsize);
		break;
	  }
	}

	if (lut)
	  ImageLut(out, img->xsize * bpp, lut);

	ImagePutRow(img, 0, y, img->xsize, out);
      }

      if (passes > 1)
      {
	if (info->color_type == PNG_COLOR_TYPE_GRAY ||
		 info->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
          inptr += img->xsize;
	else
          inptr += img->xsize * 3;
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
 * End of "$Id: image-png.c,v 1.11.2.1 2002/01/02 18:04:45 mike Exp $".
 */
