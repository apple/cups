/*
 * "$Id: image-jpeg.c,v 1.10 2000/04/24 20:47:49 mike Exp $"
 *
 *   JPEG image routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-2000 by Easy Software Products.
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
 *   ImageReadJPEG() - Read a JPEG image file.
 */

/*
 * Include necessary headers...
 */

#include "image.h"

#ifdef HAVE_LIBJPEG
#  include <jpeglib.h>	/* JPEG/JFIF image definitions */


/*
 * 'ImageReadJPEG()' - Read a JPEG image file.
 */

int					/* O - Read status */
ImageReadJPEG(image_t    *img,		/* IO - Image */
              FILE       *fp,		/* I - Image file */
              int        primary,	/* I - Primary choice for colorspace */
              int        secondary,	/* I - Secondary choice for colorspace */
              int        saturation,	/* I - Color saturation (%) */
              int        hue,		/* I - Color hue (degrees) */
	      const ib_t *lut)		/* I - Lookup table for gamma/brightness */
{
  struct jpeg_decompress_struct	cinfo;	/* Decompressor info */
  struct jpeg_error_mgr		jerr;	/* Error handler info */
  ib_t				*in,	/* Input pixels */
				*out;	/* Output pixels */


  (void)secondary;

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, fp);
  jpeg_read_header(&cinfo, 1);

  cinfo.quantize_colors = 0;

  if (cinfo.num_components == 1)
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
  }

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
      img->xppi = (int)((float)cinfo.X_density * 2.54);
      img->yppi = (int)((float)cinfo.Y_density * 2.54);
    }
  }

  fprintf(stderr, "DEBUG: JPEG image %dx%dx%d, %dx%d PPI\n",
          img->xsize, img->colorspace, cinfo.output_components,
	  img->xppi, img->yppi);

  ImageSetMaxTiles(img, 0);

  in = malloc(img->xsize * cinfo.output_components);
  if (primary < 0)
    out = malloc(-img->xsize * primary);
  else
    out = malloc(img->xsize * primary);

  jpeg_start_decompress(&cinfo);

  while (cinfo.output_scanline < cinfo.output_height)
  {
    jpeg_read_scanlines(&cinfo, (JSAMPROW *)&in, (JDIMENSION)1);

    if ((saturation != 100 || hue != 0) && cinfo.output_components > 1)
      ImageRGBAdjust(in, img->xsize, saturation, hue);

    if ((primary == IMAGE_WHITE && cinfo.out_color_space == JCS_GRAYSCALE) ||
        (primary == IMAGE_RGB && cinfo.out_color_space == JCS_RGB))
    {
      if (lut)
        ImageLut(in, img->xsize * ImageGetDepth(img), lut);

      ImagePutRow(img, 0, cinfo.output_scanline - 1, img->xsize, in);
    }
    else if (cinfo.out_color_space == JCS_GRAYSCALE)
    {
      switch (primary)
      {
        case IMAGE_BLACK :
            ImageWhiteToBlack(in, out, img->xsize);
            break;
        case IMAGE_RGB :
            ImageWhiteToRGB(in, out, img->xsize);
            break;
        case IMAGE_CMY :
            ImageWhiteToCMY(in, out, img->xsize);
            break;
        case IMAGE_CMYK :
            ImageWhiteToCMYK(in, out, img->xsize);
            break;
      }

      if (lut)
        ImageLut(out, img->xsize * ImageGetDepth(img), lut);

      ImagePutRow(img, 0, cinfo.output_scanline - 1, img->xsize, out);
    }
    else
    {
      switch (primary)
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
      }

      if (lut)
        ImageLut(out, img->xsize * ImageGetDepth(img), lut);

      ImagePutRow(img, 0, cinfo.output_scanline - 1, img->xsize, out);
    }
  }

  free(in);
  free(out);

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);

  fclose(fp);

  return (0);
}


#endif /* HAVE_LIBJPEG */


/*
 * End of "$Id: image-jpeg.c,v 1.10 2000/04/24 20:47:49 mike Exp $".
 */
