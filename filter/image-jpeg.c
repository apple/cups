/*
 * "$Id: image-jpeg.c,v 1.1 1998/02/19 20:43:33 mike Exp $"
 *
 *   JPEG image routines for espPrint, a collection of printer drivers.
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
 *   $Log: image-jpeg.c,v $
 *   Revision 1.1  1998/02/19 20:43:33  mike
 *   Initial revision
 *
 */

/*
 * Include necessary headers...
 */

#include "image.h"
#include <jpeglib.h>	/* JPEG/JFIF image definitions */


int
ImageReadJPEG(image_t *img,
              FILE    *fp,
              int     primary,
              int     secondary,
              int     saturation,
              int     hue)
{
  struct jpeg_decompress_struct	cinfo;		/* Decompressor info */
  struct jpeg_error_mgr		jerr;		/* Error handler info */
  ib_t				*in,		/* Input pixels */
				*out;		/* Output pixels */


  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, fp);
  jpeg_read_header(&cinfo, 1);

  cinfo.quantize_colors = 0;

  if (primary == IMAGE_BLACK || primary == IMAGE_WHITE)
  {
    cinfo.out_color_space      = JCS_GRAYSCALE;
    cinfo.out_color_components = 1;
    cinfo.output_components    = 1;
  }
  else
  {
    cinfo.out_color_space      = JCS_RGB;
    cinfo.out_color_components = 3;
    cinfo.output_components    = 3;
  };

  jpeg_calc_output_dimensions(&cinfo);

  img->xsize      = cinfo.output_width;
  img->ysize      = cinfo.output_height;
  img->colorspace = primary;

  if (cinfo.X_density > 0 && cinfo.Y_density > 0 && cinfo.density_unit > 0)
  {
    if (cinfo.density_unit == 1)
    {
      img->xppi = cinfo.X_density;
      img->yppi = cinfo.Y_density;
    }
    else
    {
      img->xppi = (int)((float)cinfo.X_density / 2.54);
      img->yppi = (int)((float)cinfo.Y_density / 2.54);
    };
  };

  ImageSetMaxTiles(img, 0);

  in = malloc(img->xsize * cinfo.output_components);
  if (primary < 0)
    out = malloc(-img->xsize * primary);

  jpeg_start_decompress(&cinfo);

  while (cinfo.output_scanline < cinfo.output_height)
  {
    jpeg_read_scanlines(&cinfo, (JSAMPROW *)&in, (JDIMENSION)1);

    if ((saturation != 100 || hue != 0) && cinfo.output_components > 1)
      ImageRGBAdjust(in, img->xsize, saturation, hue);

    if (primary > 0)
      ImagePutRow(img, 0, cinfo.output_scanline - 1, img->xsize, in);
    else
    {
      switch (primary)
      {
        case IMAGE_BLACK :
            ImageWhiteToBlack(in, out, img->xsize);
            break;
        case IMAGE_CMY :
            ImageRGBToCMY(in, out, img->xsize);
            break;
        case IMAGE_CMYK :
            ImageRGBToCMYK(in, out, img->xsize);
            break;
      };

      ImagePutRow(img, 0, cinfo.output_scanline - 1, img->xsize, out);
    };
  };

  free(in);
  if (primary < 0)
    free(out);

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);

  fclose(fp);

  return (0);
}


/*
 * End of "$Id: image-jpeg.c,v 1.1 1998/02/19 20:43:33 mike Exp $".
 */
