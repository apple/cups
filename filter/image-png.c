/*
 * "$Id: image-png.c,v 1.1 1998/02/19 20:43:33 mike Exp $"
 *
 *   PNG image routines for espPrint, a collection of printer drivers.
 *
 *   Copyright 1993-1998 by Easy Software Products
 *
 *   These coded instructions, statements, and computer programs contain
 *   unpublished proprietary information of Easy Software Products, and
 *   are protected by Federal copyright law.  They may not be disclosed
 *   to third parties or copied or duplicated in any form, in whole or
 *   in part, without the prior written consent of Easy Software Products.
 *
 * Contents:
 *
 * Revision History:
 *
 *   $Log: image-png.c,v $
 *   Revision 1.1  1998/02/19 20:43:33  mike
 *   Initial revision
 *
 */

/*
 * Include necessary headers...
 */

#include "image.h"
#include <png.h>	/* Portable Network Graphics (PNG) definitions */


int
ImageReadPNG(image_t *img,
             FILE    *fp,
             int     primary,
             int     secondary,
             int     saturation,
             int     hue)
{
  int		y;		/* Looping var */
  png_structp	pp;		/* PNG read pointer */
  png_infop	info;		/* PNG info pointers */
  int		bpp;		/* Bytes per pixel */
  ib_t		*in,		/* Input pixels */
		*out;		/* Output pixels */


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
   /*
    * Who the hell was the braniac that decided that an *integer*
    * pixels-per-meter measurement was good or even useful (~40ppi
    * per meter)!
    */

    img->xppi = (int)((float)info->x_pixels_per_unit / 0.0254);
    img->yppi = (int)((float)info->y_pixels_per_unit / 0.0254);
  };

  ImageSetMaxTiles(img, 0);

  if (info->bit_depth < 8)
  {
    png_set_packing(pp);
    png_set_expand(pp);

    if (info->valid & PNG_INFO_sBIT)
      png_set_shift(pp, &(info->sig_bit));
  }
  else if (info->bit_depth == 16)
    png_set_strip_16(pp);

  if (info->color_type == PNG_COLOR_TYPE_GRAY ||
      info->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    in = malloc(img->xsize);
  else
    in = malloc(img->xsize * 3);

  bpp = img->colorspace < 0 ? -img->colorspace : img->colorspace;
  out = malloc(img->xsize * bpp);

 /*
  * This doesn't work for interlaced PNG files... :(
  */

  for (y = 0; y < img->ysize; y ++)
  {
    if (info->color_type == PNG_COLOR_TYPE_GRAY ||
	 info->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    {
      if (img->colorspace == IMAGE_WHITE)
        png_read_row(pp, (png_bytep)out, NULL);
      else
      {
	png_read_row(pp, (png_bytep)in, NULL);

	switch (img->colorspace)
	{
	  case IMAGE_RGB :
	      ImageWhiteToRGB(in, out, img->xsize);
	      break;
	  case IMAGE_BLACK :
	      ImageWhiteToBlack(in, out, img->xsize);
	      break;
	  case IMAGE_CMY :
	      ImageWhiteToCMY(in, out, img->xsize);
	      break;
	  case IMAGE_CMYK :
	      ImageWhiteToCMYK(in, out, img->xsize);
	      break;
	};
      };
    }
    else
    {
      if (img->colorspace == IMAGE_RGB)
      {
        png_read_row(pp, (png_bytep)out, NULL);

	if (saturation != 100 || hue != 0)
	  ImageRGBAdjust(out, img->xsize, saturation, hue);
      }
      else
      {
	png_read_row(pp, (png_bytep)in, NULL);

	if ((saturation != 100 || hue != 0) && bpp > 1)
	  ImageRGBAdjust(in, img->xsize, saturation, hue);

	switch (img->colorspace)
	{
	  case IMAGE_WHITE :
	      ImageRGBToWhite(in, out, img->xsize);
	      break;
	  case IMAGE_BLACK :
	      ImageRGBToBlack(in, out, img->xsize);
	      break;
	  case IMAGE_CMY :
	      ImageRGBToCMY(in, out, img->xsize);
	      break;
	  case IMAGE_CMYK :
	      ImageRGBToCMYK(in, out, img->xsize);
	      break;
	};
      };
    };

    ImagePutRow(img, 0, y, img->xsize, out);
  };

  png_read_end(pp, info);
  png_read_destroy(pp, info, NULL);

  fclose(fp);

  return (0);
}


/*
 * End of "$Id: image-png.c,v 1.1 1998/02/19 20:43:33 mike Exp $".
 */
