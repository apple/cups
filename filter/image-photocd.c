/*
 * "$Id: image-photocd.c,v 1.10.2.4 2003/01/07 18:26:54 mike Exp $"
 *
 *   PhotoCD routines for the Common UNIX Printing System (CUPS).
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
 *   ImageReadPhotoCD() - Read a PhotoCD image file.
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

/*
 * 'ImageReadPhotoCD()' - Read a PhotoCD image file.
 */

int					/* O - Read status */
ImageReadPhotoCD(image_t    *img,	/* IO - Image */
        	 FILE       *fp,	/* I - Image file */
        	 int        primary,	/* I - Primary choice for colorspace */
        	 int        secondary,	/* I - Secondary choice for colorspace */
        	 int        saturation,	/* I - Color saturation (%) */
        	 int        hue,	/* I - Color hue (degrees) */
		 const ib_t *lut)	/* I - Lookup table for gamma/brightness */
{
  int		x, y;		/* Looping vars */
  int		xdir,		/* X direction */
		xstart;		/* X starting point */
  int		bpp;		/* Bytes per pixel */
  int		pass;		/* Pass number */
  int		rotation;	/* 0 for 768x512, 1 for 512x768 */
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
  * Get the image orientation...
  */

  fseek(fp, 72, SEEK_SET);
  rotation = (getc(fp) & 63) != 8;

 /*
  * Seek to the start of the base image...
  */

  fseek(fp, 0x30000, SEEK_SET);

 /*
  * Allocate and initialize...
  */

  img->colorspace = (primary == IMAGE_RGB_CMYK) ? IMAGE_RGB : primary;
  img->xppi       = 128;
  img->yppi       = 128;

  if (rotation)
  {
    img->xsize = 512;
    img->ysize = 768;
  }
  else
  {
    img->xsize = 768;
    img->ysize = 512;
  }

  ImageSetMaxTiles(img, 0);

  bpp = ImageGetDepth(img);
  in  = malloc(768 * 3);
  out = malloc(768 * bpp);

  if (bpp > 1)
    rgb = malloc(768 * 3);
  else
    rgb = NULL;

  if (rotation)
  {
    xstart = 767 * bpp;
    xdir   = -2 * bpp;
  }
  else
  {
    xstart = 0;
    xdir   = 0;
  }

 /*
  * Read the image file...
  */

  for (y = 0; y < 512; y += 2)
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
	  if (rotation)
	  {
	    for (rgbptr = out + xstart, x = 0; x < 768; x ++)
	      *rgbptr-- = 255 - *iy++;

	    if (lut)
	      ImageLut(out, 768, lut);

            ImagePutCol(img, 511 - y - pass, 0, 768, out);
	  }
	  else
	  {
            ImageWhiteToBlack(iy, out, 768);

	    if (lut)
	      ImageLut(out, 768, lut);

            ImagePutRow(img, 0, y + pass, 768, out);
            iy += 768;
	  }
	}
	else if (rotation)
	{
	  for (rgbptr = out + xstart, x = 0; x < 768; x ++)
	    *rgbptr-- = 255 - *iy++;

	  if (lut)
	    ImageLut(out, 768, lut);

          ImagePutCol(img, 511 - y - pass, 0, 768, out);
	}
	else
	{
	  if (lut)
	    ImageLut(iy, 768, lut);

          ImagePutRow(img, 0, y + pass, 768, iy);
          iy += 768;
	}
      }
      else
      {
       /*
        * Convert YCbCr to RGB...  While every pixel gets a luminance
	* value, adjacent pixels share chroma information.
	*/

        cb = cr = 0.0f;

        for (x = 0, rgbptr = rgb + xstart, icb = in + 1536, icr = in + 1920;
	     x < 768;
	     x ++, iy ++, rgbptr += xdir)
	{
	  if (!(x & 1))
	  {
	    cb = (float)(*icb - 156);
	    cr = (float)(*icr - 137);
	  }

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
	  ImageRGBAdjust(rgb, 768, saturation, hue);

       /*
        * Then convert the RGB data to the appropriate colorspace and
	* put it in the image...
	*/

        if (img->colorspace == IMAGE_RGB)
	{
	  if (lut)
	    ImageLut(rgb, 768 * 3, lut);

	  if (rotation)
            ImagePutCol(img, 511 - y - pass, 0, 768, rgb);
	  else
            ImagePutRow(img, 0, y + pass, 768, rgb);
	}
	else
	{
	  switch (img->colorspace)
	  {
	    case IMAGE_CMY :
		ImageRGBToCMY(rgb, out, 768);
		break;
	    case IMAGE_CMYK :
		ImageRGBToCMYK(rgb, out, 768);
		break;
	  }

	  if (lut)
	    ImageLut(out, 768 * bpp, lut);

	  if (rotation)
            ImagePutCol(img, 511 - y - pass, 0, 768, out);
	  else
            ImagePutRow(img, 0, y + pass, 768, out);
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
 * End of "$Id: image-photocd.c,v 1.10.2.4 2003/01/07 18:26:54 mike Exp $".
 */
