/*
 * "$Id: image-pix.c,v 1.1 2000/04/30 22:03:57 mike Exp $"
 *
 *   Alias PIX image routines for the Common UNIX Printing System (CUPS).
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
 *   ImageReadPIX() - Read a PIX image file.
 *   read_short()   - Read a 16-bit integer.
 */

/*
 * Include necessary headers...
 */

#include "image.h"


/*
 * Local functions...
 */

static short	read_short(FILE *fp);


/*
 * 'ImageReadPIX()' - Read a PIX image file.
 */

int					/* O - Read status */
ImageReadPIX(image_t    *img,		/* IO - Image */
             FILE       *fp,		/* I - Image file */
             int        primary,	/* I - Primary choice for colorspace */
             int        secondary,	/* I - Secondary choice for colorspace */
             int        saturation,	/* I - Color saturation (%) */
             int        hue,		/* I - Color hue (degrees) */
	     const ib_t *lut)		/* I - Lookup table for gamma/brightness */
{
  short		width,			/* Width of image */
		height,			/* Height of image */
		depth;			/* Depth of image (bits) */
  int		count,			/* Repetition count */
		bpp,			/* Bytes per pixel */
		x, y;			/* Looping vars */
  ib_t		r, g, b;		/* Red, green/gray, blue values */
  ib_t		*in,			/* Input pixels */
		*out,			/* Output pixels */
		*ptr;			/* Pointer into pixels */


 /*
  * Get the image dimensions and setup the image...
  */

  width  = read_short(fp);
  height = read_short(fp);
  read_short(fp);
  read_short(fp);
  depth  = read_short(fp);

  if (depth == 8)
    img->colorspace = secondary;
  else
    img->colorspace = primary;

  img->xsize = width;
  img->ysize = height;

  ImageSetMaxTiles(img, 0);

  in  = malloc(img->xsize * (depth / 8));
  bpp = ImageGetDepth(img);
  out = malloc(img->xsize * bpp);

 /*
  * Read the image data...
  */

  if (depth == 8)
  {
    for (count = 0, y = 0; y < img->ysize; y ++)
    {
      if (img->colorspace == IMAGE_WHITE)
        ptr = out;
      else
        ptr = in;

      for (x = img->xsize; x > 0; x --, count --)
      {
        if (count == 0)
	{
          count = getc(fp);
	  g     = getc(fp);
	}

        *ptr++ = g;
      }

      if (img->colorspace != IMAGE_WHITE)
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
    for (count = 0, y = 0; y < img->ysize; y ++)
    {
      if (img->colorspace == IMAGE_RGB)
        ptr = out;
      else
        ptr = in;

      for (x = img->xsize; x > 0; x --, count --)
      {
        if (count == 0)
	{
          count = getc(fp);
	  b     = getc(fp);
	  g     = getc(fp);
	  r     = getc(fp);
	}

        *ptr++ = r;
        *ptr++ = g;
        *ptr++ = b;
      }

      if (img->colorspace == IMAGE_RGB)
      {
	if (saturation != 100 || hue != 0)
	  ImageRGBAdjust(out, img->xsize, saturation, hue);
      }
      else
      {
	if (saturation != 100 || hue != 0)
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
      }

      if (lut)
	ImageLut(out, img->xsize * bpp, lut);

      ImagePutRow(img, 0, y, img->xsize, out);
    }
  }

  fclose(fp);
  free(in);
  free(out);

  return (0);
}


/*
 * 'read_short()' - Read a 16-bit integer.
 */

static short			/* O - Value from file */
read_short(FILE *fp)		/* I - File to read from */
{
  int	ch;			/* Character from file */


  ch = getc(fp);
  return ((ch << 8) | getc(fp));
}


/*
 * End of "$Id: image-pix.c,v 1.1 2000/04/30 22:03:57 mike Exp $".
 */
