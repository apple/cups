/*
 * "$Id: image-photocd.c,v 1.4 1999/03/25 20:39:06 mike Exp $"
 *
 *   PhotoCD routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-1999 by Easy Software Products.
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
 */

/*
 * Include necessary headers...
 */

#include "image.h"


/*
 * PhotoCD support is currently limited to the 768x512 base image, which
 * is only YCC encoded.  Support for the higher resolution images will
 * require a lot of extra code...
 */

int
ImageReadPhotoCD(image_t *img,
                 FILE    *fp,
                 int     primary,
                 int     secondary,
        	 int     saturation,
        	 int     hue)
{
  int		x, y;		/* Looping vars */
  int		bpp;		/* Bytes per pixel */
  int		pass;		/* Pass number */
  int		temp,		/* Adjusted luminance */
		temp2,		/* Red, green, and blue values */
		cb, cr;		/* Adjusted chroma values */
  ib_t		*in,		/* Input (YCC) pixels */
		*iy,		/* Luminance */
		*icb,		/* Blue chroma */
		*icr,		/* Red chroma */
		*rgb,		/* RGB */
		*rgbptr,	/* Pointer into RGB data */
		*out;		/* Output pixels */


  (void)secondary;

 /*
  * Seek to the start of the base image...
  */

  fseek(fp, 0x30000, SEEK_SET);

 /*
  * Allocate the initialize...
  */

  img->colorspace = primary;
  img->xsize      = 768;
  img->ysize      = 512;
  img->xppi       = 128;
  img->yppi       = 128;

  ImageSetMaxTiles(img, 0);

  bpp = ImageGetDepth(img);
  in  = malloc(768 * 3);
  out = malloc(img->xsize * bpp);

  if (bpp > 1)
    rgb = malloc(768 * 3);

 /*
  * Read the image file...
  */

  for (y = 0; y < img->ysize; y += 2)
  {
   /*
    * Grab the next two scanlines:
    *
    *     YYYYYYYYYYYYYYY...
    *     YYYYYYYYYYYYYYY...
    *     CbCbCb...CrCrCr...
    */

    if (fread(in, 1, 768 * 3, fp) < (768 * 3))
    {
     /*
      * Couldn't read a row of data - return an error!
      */

      free(in);
      free(out);

      return (-1);
    }

   /*
    * Process the two scanlines...
    */

    for (pass = 0, iy = in; pass < 2; pass ++)
    {
      if (bpp == 1)
      {
       /*
	* Just extract the luminance channel from the line and put it
	* in the image...
	*/

        if (primary == IMAGE_BLACK)
	{
          ImageWhiteToBlack(iy, out, img->xsize);
          ImagePutRow(img, 0, y + pass, img->xsize, out);
	}
	else
          ImagePutRow(img, 0, y + pass, img->xsize, iy);

        iy += 768;
      }
      else
      {
       /*
        * Convert YCbCr to RGB...  While every pixel gets a luminance
	* value, adjacent pixels share chroma information.
	*/

        for (x = 0, rgbptr = rgb, icb = in + 1536, icr = in + 1920;
	     x < img->xsize;
	     x ++, iy ++)
	{
	  if (!(x & 1))
	  {
	    cb = (float)(*icb - 156);
	    cr = (float)(*icr - 137);
	  }

          temp = 1.407488 * (float)(*iy);

          temp = 92241 * (*iy);

	  temp2 = (temp + 86706 * cr) / 65536;
	  if (temp2 < 0)
	    *rgbptr++ = 0;
	  else if (temp2 > 255)
	    *rgbptr++ = 255;
	  else
	    *rgbptr++ = temp2;

          temp2 = (temp - 25914 * cb - 44166 * cr) / 65536;
	  if (temp2 < 0)
	    *rgbptr++ = 0;
	  else if (temp2 > 255)
	    *rgbptr++ = 255;
	  else
	    *rgbptr++ = temp2;

          temp2 = (temp + 133434 * cb) / 65536;
	  if (temp2 < 0)
	    *rgbptr++ = 0;
	  else if (temp2 > 255)
	    *rgbptr++ = 255;
	  else
	    *rgbptr++ = temp2;

	  if (x & 1)
	  {
	    icb ++;
	    icr ++;
	  }
	}

       /*
        * Adjust the hue and saturation if needed...
	*/

	if (saturation != 100 || hue != 0)
	  ImageRGBAdjust(rgb, img->xsize, saturation, hue);

       /*
        * Then convert the RGB data to the appropriate colorspace and
	* put it in the image...
	*/

        if (img->colorspace == IMAGE_RGB)
          ImagePutRow(img, 0, y + pass, img->xsize, rgb);
	else
	{
	  switch (img->colorspace)
	  {
	    case IMAGE_CMY :
		ImageRGBToCMY(rgb, out, img->xsize);
		break;
	    case IMAGE_CMYK :
		ImageRGBToCMYK(rgb, out, img->xsize);
		break;
	  }

          ImagePutRow(img, 0, y + pass, img->xsize, out);
	}
      }
    }
  }

 /*
  * Free memory and return...
  */

  free(in);
  free(out);
  if (bpp > 1)
    free(rgb);

  return (0);
}


/*
 * End of "$Id: image-photocd.c,v 1.4 1999/03/25 20:39:06 mike Exp $".
 */
