/*
 * "$Id: image-sgi.c,v 1.7 2000/03/21 04:03:26 mike Exp $"
 *
 *   SGI image file routines for the Common UNIX Printing System (CUPS).
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
 *   ImageReadSGI() - Read a SGI image file.
 */

/*
 * Include necessary headers...
 */

#include "image.h"
#include "image-sgi.h"


/*
 * 'ImageReadSGI()' - Read a SGI image file.
 */

int					/* O - Read status */
ImageReadSGI(image_t    *img,		/* IO - Image */
             FILE       *fp,		/* I - Image file */
             int        primary,	/* I - Primary choice for colorspace */
             int        secondary,	/* I - Secondary choice for colorspace */
             int        saturation,	/* I - Color saturation (%) */
             int        hue,		/* I - Color hue (degrees) */
	     const ib_t *lut)		/* I - Lookup table for gamma/brightness */
{
  int		i, y;		/* Looping vars */
  int		bpp;		/* Bytes per pixel */
  sgi_t		*sgip;		/* SGI image file */
  ib_t		*in,		/* Input pixels */
		*inptr,		/* Current input pixel */
		*out;		/* Output pixels */
  unsigned short *rows[4],	/* Row pointers for image data */
		*red,
		*green,
		*blue,
		*gray,
		*alpha;


 /*
  * Setup the SGI file...
  */

  sgip = sgiOpenFile(fp, SGI_READ, 0, 0, 0, 0, 0);

 /*
  * Get the image dimensions and load the output image...
  */

  if (sgip->zsize < 3)
    img->colorspace = secondary;
  else
    img->colorspace = primary;

  img->xsize = sgip->xsize;
  img->ysize = sgip->ysize;

  ImageSetMaxTiles(img, 0);

  bpp = ImageGetDepth(img);
  in  = malloc(img->xsize * sgip->zsize);
  out = malloc(img->xsize * bpp);

  rows[0] = calloc(img->xsize * sgip->zsize, sizeof(unsigned short));
  for (i = 1; i < sgip->zsize; i ++)
    rows[i] = rows[0] + i * img->xsize;

 /*
  * Read the SGI image file...
  */

  for (y = 0; y < img->ysize; y ++)
  {
    for (i = 0; i < sgip->zsize; i ++)
      sgiGetRow(sgip, rows[i], img->ysize - 1 - y, i);

    switch (sgip->zsize)
    {
      case 1 :
          if (sgip->bpp == 1)
	    for (i = img->xsize - 1, gray = rows[0], inptr = in;
		 i >= 0;
		 i --)
            {
              *inptr++ = *gray++;
            }
          else
	    for (i = img->xsize - 1, gray = rows[0], inptr = in;
		 i >= 0;
		 i --)
            {
              *inptr++ = (*gray++) / 256 + 128;
            }
          break;
      case 2 :
          if (sgip->bpp == 1)
	    for (i = img->xsize - 1, gray = rows[0], alpha = rows[1], inptr = in;
		 i >= 0;
		 i --)
            {
              *inptr++ = (*gray++) * (*alpha++) / 255;
            }
          else
	    for (i = img->xsize - 1, gray = rows[0], alpha = rows[1], inptr = in;
		 i >= 0;
		 i --)
            {
              *inptr++ = ((*gray++) / 256 + 128) * (*alpha++) / 32767;
            }
          break;
      case 3 :
          if (sgip->bpp == 1)
	    for (i = img->xsize - 1, red = rows[0], green = rows[1],
	             blue = rows[2], inptr = in;
		 i >= 0;
		 i --)
            {
              *inptr++ = *red++;
              *inptr++ = *green++;
              *inptr++ = *blue++;
            }
          else
	    for (i = img->xsize - 1, red = rows[0], green = rows[1],
	             blue = rows[2], inptr = in;
		 i >= 0;
		 i --)
            {
              *inptr++ = (*red++) / 256 + 128;
              *inptr++ = (*green++) / 256 + 128;
              *inptr++ = (*blue++) / 256 + 128;
            }
          break;
      case 4 :
          if (sgip->bpp == 1)
	    for (i = img->xsize - 1, red = rows[0], green = rows[1],
	             blue = rows[2], alpha = rows[3], inptr = in;
		 i >= 0;
		 i --)
            {
              *inptr++ = (*red++) * (*alpha) / 255;
              *inptr++ = (*green++) * (*alpha) / 255;
              *inptr++ = (*blue++) * (*alpha++) / 255;
            }
          else
	    for (i = img->xsize - 1, red = rows[0], green = rows[1],
	             blue = rows[2], alpha = rows[3], inptr = in;
		 i >= 0;
		 i --)
            {
              *inptr++ = ((*red++) / 256 + 128) * (*alpha) / 32767;
              *inptr++ = ((*green++) / 256 + 128) * (*alpha) / 32767;
              *inptr++ = ((*blue++) / 256 + 128) * (*alpha++) / 32767;
            }
          break;
    }

    if (sgip->zsize < 3)
    {
      if (img->colorspace == IMAGE_WHITE)
      {
        if (lut)
	  ImageLut(in, img->xsize, lut);

        ImagePutRow(img, 0, y, img->xsize, in);
      }
      else
      {
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
	}

        if (lut)
	  ImageLut(out, img->xsize * bpp, lut);

        ImagePutRow(img, 0, y, img->xsize, out);
      }
    }
    else
    {
      if (img->colorspace == IMAGE_RGB)
      {
	if (saturation != 100 || hue != 0)
	  ImageRGBAdjust(in, img->xsize, saturation, hue);

        if (lut)
	  ImageLut(in, img->xsize * 3, lut);

        ImagePutRow(img, 0, y, img->xsize, in);
      }
      else
      {
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
	}

        if (lut)
	  ImageLut(out, img->xsize * bpp, lut);

        ImagePutRow(img, 0, y, img->xsize, out);
      }
    }
  }

  free(in);
  free(out);
  free(rows[0]);

  sgiClose(sgip);

  return (0);
}


/*
 * End of "$Id: image-sgi.c,v 1.7 2000/03/21 04:03:26 mike Exp $".
 */
