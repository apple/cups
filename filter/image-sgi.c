/*
 * "$Id: image-sgi.c,v 1.1 1998/02/19 20:43:33 mike Exp $"
 *
 *   SGI image file routines for espPrint, a collection of printer drivers.
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
 *   $Log: image-sgi.c,v $
 *   Revision 1.1  1998/02/19 20:43:33  mike
 *   Initial revision
 *
 */

/*
 * Include necessary headers...
 */

#include "image.h"
#include "sgi.h"


int
ImageReadSGI(image_t *img,
             FILE    *fp,
             int     primary,
             int     secondary,
             int     saturation,
             int     hue)
{
  int		i, y;		/* Looping vars */
  int		bpp;		/* Bytes per pixel */
  sgi_t		*sgip;		/* SGI image file */
  ib_t		*in,		/* Input pixels */
		*inptr,		/* Current input pixel */
		*out;		/* Output pixels */
  short		*rows[4],	/* Row pointers for image data */
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

  rows[0] = calloc(img->xsize * sgip->zsize, sizeof(short));
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
            };
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
            };
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
            };
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
	             blue = rows[2], inptr = in;
		 i >= 0;
		 i --)
            {
              *inptr++ = ((*red++) / 256 + 128) * (*alpha) / 32767;
              *inptr++ = ((*green++) / 256 + 128) * (*alpha) / 32767;
              *inptr++ = ((*blue++) / 256 + 128) * (*alpha++) / 32767;
            };
          break;
    };

    if (sgip->zsize < 3)
    {
      if (img->colorspace == IMAGE_WHITE)
        ImagePutRow(img, 0, y, img->xsize, in);
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
	};

        ImagePutRow(img, 0, y, img->xsize, out);
      };
    }
    else
    {
      if (img->colorspace == IMAGE_RGB)
      {
	if (saturation != 100 || hue != 0)
	  ImageRGBAdjust(in, img->xsize, saturation, hue);

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
	};

        ImagePutRow(img, 0, y, img->xsize, out);
      };
    };
  };

  free(in);
  free(out);
  free(rows[0]);

  sgiClose(sgip);

  return (0);
}


/*
 * End of "$Id: image-sgi.c,v 1.1 1998/02/19 20:43:33 mike Exp $".
 */
