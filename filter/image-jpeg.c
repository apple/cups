/*
 * "$Id: image-jpeg.c,v 1.11.2.6 2003/01/07 18:26:54 mike Exp $"
 *
 *   JPEG image routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-2003 by Easy Software Products.
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
 *   This file is subject to the Apple OS-Developed Software exception.
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
  char				header[16];
  					/* Photoshop JPEG header */
  int				psjpeg;	/* Non-zero if Photoshop JPEG */
  static const char		*cspaces[] =
				{	/* JPEG colorspaces... */
				  "JCS_UNKNOWN",
				  "JCS_GRAYSCALE",
				  "JCS_RGB",
				  "JCS_YCbCr",
				  "JCS_CMYK",
				  "JCS_YCCK"
				};


 /*
  * Read the first 16 bytes to determine if this is a Photoshop JPEG file...
  */

  fread(header, sizeof(header), 1, fp);
  rewind(fp);

  psjpeg = memcmp(header + 6, "Photoshop ", 10) == 0;

 /*
  * Read the JPEG header...
  */

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, fp);
  jpeg_read_header(&cinfo, 1);

  cinfo.quantize_colors = 0;

  fprintf(stderr, "DEBUG: num_components = %d\n", cinfo.num_components);
  fprintf(stderr, "DEBUG: jpeg_color_space = %s\n",
          cspaces[cinfo.jpeg_color_space]);

  if (cinfo.num_components == 1)
  {
    fputs("DEBUG: Converting image to grayscale...\n", stderr);

    cinfo.out_color_space      = JCS_GRAYSCALE;
    cinfo.out_color_components = 1;
    cinfo.output_components    = 1;

    img->colorspace = secondary;
  }
  else if (cinfo.num_components == 4)
  {
    fputs("DEBUG: Converting image to CMYK...\n", stderr);

    cinfo.out_color_space      = JCS_CMYK;
    cinfo.out_color_components = 4;
    cinfo.output_components    = 4;

    img->colorspace = (primary == IMAGE_RGB_CMYK) ? IMAGE_CMYK : primary;
  }
  else
  {
    fputs("DEBUG: Converting image to RGB...\n", stderr);

    cinfo.out_color_space      = JCS_RGB;
    cinfo.out_color_components = 3;
    cinfo.output_components    = 3;

    img->colorspace = (primary == IMAGE_RGB_CMYK) ? IMAGE_RGB : primary;
  }

  jpeg_calc_output_dimensions(&cinfo);

  if (cinfo.output_width <= 0 || cinfo.output_width > IMAGE_MAX_WIDTH ||
      cinfo.output_height <= 0 || cinfo.output_height > IMAGE_MAX_HEIGHT)
  {
    fprintf(stderr, "ERROR: Bad JPEG dimensions %dx%d!\n",
            cinfo.output_width, cinfo.output_height);

    jpeg_destroy_decompress(&cinfo);

    fclose(fp);
    return (1);
  }

  img->xsize      = cinfo.output_width;
  img->ysize      = cinfo.output_height;

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

    if (img->xppi == 0 || img->yppi == 0)
    {
      fprintf(stderr, "ERROR: Bad JPEG image resolution %dx%d PPI.\n",
              img->xppi, img->yppi);
      img->xppi = img->yppi = 128;
    }
  }

  fprintf(stderr, "DEBUG: JPEG image %dx%dx%d, %dx%d PPI\n",
          img->xsize, img->ysize, cinfo.output_components,
	  img->xppi, img->yppi);

  ImageSetMaxTiles(img, 0);

  in  = malloc(img->xsize * cinfo.output_components);
  out = malloc(img->xsize * ImageGetDepth(img));

  jpeg_start_decompress(&cinfo);

  while (cinfo.output_scanline < cinfo.output_height)
  {
    jpeg_read_scanlines(&cinfo, (JSAMPROW *)&in, (JDIMENSION)1);

    if (psjpeg && cinfo.output_components == 4)
    {
     /*
      * Invert CMYK data from Photoshop...
      */

      ib_t	*ptr;	/* Pointer into buffer */
      int	i;	/* Looping var */


      for (ptr = in, i = img->xsize * 4; i > 0; i --, ptr ++)
        *ptr = 255 - *ptr;
    }

    if ((saturation != 100 || hue != 0) && cinfo.output_components == 3)
      ImageRGBAdjust(in, img->xsize, saturation, hue);

    if ((img->colorspace == IMAGE_WHITE && cinfo.out_color_space == JCS_GRAYSCALE) ||
        (img->colorspace == IMAGE_RGB && cinfo.out_color_space == JCS_RGB) ||
	(img->colorspace == IMAGE_CMYK && cinfo.out_color_space == JCS_CMYK))
    {
#ifdef DEBUG
      int	i, j;
      ib_t	*ptr;


      fputs("DEBUG: Direct Data...\n", stderr);

      fputs("DEBUG:", stderr);

      for (i = 0, ptr = in; i < img->xsize; i ++)
      {
        putc(' ', stderr);
	for (j = 0; j < cinfo.output_components; j ++, ptr ++)
	  fprintf(stderr, "%02X", *ptr & 255);
      }

      putc('\n', stderr);
#endif /* DEBUG */

      if (lut)
        ImageLut(in, img->xsize * ImageGetDepth(img), lut);

      ImagePutRow(img, 0, cinfo.output_scanline - 1, img->xsize, in);
    }
    else if (cinfo.out_color_space == JCS_GRAYSCALE)
    {
      switch (img->colorspace)
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
    else if (cinfo.out_color_space == JCS_RGB)
    {
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
      }

      if (lut)
        ImageLut(out, img->xsize * ImageGetDepth(img), lut);

      ImagePutRow(img, 0, cinfo.output_scanline - 1, img->xsize, out);
    }
    else /* JCS_CMYK */
    {
      fputs("DEBUG: JCS_CMYK\n", stderr);

      switch (img->colorspace)
      {
        case IMAGE_WHITE :
            ImageCMYKToWhite(in, out, img->xsize);
            break;
        case IMAGE_BLACK :
            ImageCMYKToBlack(in, out, img->xsize);
            break;
        case IMAGE_CMY :
            ImageCMYKToCMY(in, out, img->xsize);
            break;
        case IMAGE_RGB :
            ImageCMYKToRGB(in, out, img->xsize);
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
 * End of "$Id: image-jpeg.c,v 1.11.2.6 2003/01/07 18:26:54 mike Exp $".
 */
