/*
 * "$Id: image-pnm.c,v 1.7.2.2 2002/03/01 19:55:18 mike Exp $"
 *
 *   Portable Any Map file routines for the Common UNIX Printing System (CUPS).
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   ImageReadPNM() - Read a PNM image file.
 */

/*
 * Include necessary headers...
 */

#include "image.h"
#include <ctype.h>


/*
 * 'ImageReadPNM()' - Read a PNM image file.
 */

int					/* O - Read status */
ImageReadPNM(image_t    *img,		/* IO - Image */
             FILE       *fp,		/* I - Image file */
             int        primary,	/* I - Primary choice for colorspace */
             int        secondary,	/* I - Secondary choice for colorspace */
             int        saturation,	/* I - Color saturation (%) */
             int        hue,		/* I - Color hue (degrees) */
	     const ib_t *lut)		/* I - Lookup table for gamma/brightness */
{
  int		x, y;		/* Looping vars */
  int		bpp;		/* Bytes per pixel */
  ib_t		*in,		/* Input pixels */
		*inptr,		/* Current input pixel */
		*out,		/* Output pixels */
		*outptr,	/* Current output pixel */
		bit;		/* Bit in input line */
  char		line[255],	/* Input line */
		*lineptr;	/* Pointer in line */
  int		format,		/* Format of PNM file */
		val,		/* Pixel value */
		maxval;		/* Maximum pixel value */


 /*
  * Read the file header in the format:
  *
  *   Pformat
  *   # comment1
  *   # comment2
  *   ...
  *   # commentN
  *   width
  *   height
  *   max sample
  */

  lineptr = fgets(line, sizeof(line), fp);
  lineptr ++;

  format = atoi(lineptr);
  while (isdigit(*lineptr))
    lineptr ++;

  while (lineptr != NULL && img->xsize == 0)
  {
    if (*lineptr == '\0' || *lineptr == '#')
      lineptr = fgets(line, sizeof(line), fp);
    else if (isdigit(*lineptr))
    {
      img->xsize = atoi(lineptr);
      while (isdigit(*lineptr))
	lineptr ++;
    }
    else
      lineptr ++;
  }

  while (lineptr != NULL && img->ysize == 0)
  {
    if (*lineptr == '\0' || *lineptr == '#')
      lineptr = fgets(line, sizeof(line), fp);
    else if (isdigit(*lineptr))
    {
      img->ysize = atoi(lineptr);
      while (isdigit(*lineptr))
	lineptr ++;
    }
    else
      lineptr ++;
  }

  if (format != 1 && format != 4)
  {
    maxval = 0;

    while (lineptr != NULL && maxval == 0)
    {
      if (*lineptr == '\0' || *lineptr == '#')
	lineptr = fgets(line, sizeof(line), fp);
      else if (isdigit(*lineptr))
      {
	maxval = atoi(lineptr);
	while (isdigit(*lineptr))
	  lineptr ++;
      }
      else
	lineptr ++;
    }
  }
  else
    maxval = 1;

  if (format == 1 || format == 2 || format == 4 || format == 5)
    img->colorspace = secondary;
  else
    img->colorspace = primary;

  ImageSetMaxTiles(img, 0);

  bpp = ImageGetDepth(img);
  in  = malloc(img->xsize * 3);
  out = malloc(img->xsize * bpp);

 /*
  * Read the image file...
  */

  for (y = 0; y < img->ysize; y ++)
  {
    switch (format)
    {
      case 1 :
      case 2 :
          for (x = img->xsize, inptr = in; x > 0; x --, inptr ++)
            if (fscanf(fp, "%d", &val) == 1)
              *inptr = 255 * val / maxval;
          break;

      case 3 :
          for (x = img->xsize, inptr = in; x > 0; x --, inptr += 3)
          {
            if (fscanf(fp, "%d", &val) == 1)
              inptr[0] = 255 * val / maxval;
            if (fscanf(fp, "%d", &val) == 1)
              inptr[1] = 255 * val / maxval;
            if (fscanf(fp, "%d", &val) == 1)
              inptr[2] = 255 * val / maxval;
          }
          break;

      case 4 :
          fread(out, (img->xsize + 7) / 8, 1, fp);
          for (x = img->xsize, inptr = in, outptr = out, bit = 128;
               x > 0;
               x --, inptr ++)
          {
            if (*outptr & bit)
              *inptr = 255;
            else
              *inptr = 0;

            if (bit > 1)
              bit >>= 1;
            else
            {
              bit = 128;
              inptr ++;
            }
          }
          break;

      case 5 :
          fread(in, img->xsize, 1, fp);
          break;

      case 6 :
          fread(in, img->xsize, 3, fp);
          break;
    }

    switch (format)
    {
      case 1 :
      case 2 :
      case 4 :
      case 5 :
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
	  break;

      default :
	  if ((saturation != 100 || hue != 0) && bpp > 1)
	    ImageRGBAdjust(in, img->xsize, saturation, hue);

	  if (img->colorspace == IMAGE_RGB)
	  {
	    if (lut)
	      ImageLut(in, img->xsize * 3, lut);

            ImagePutRow(img, 0, y, img->xsize, in);
	  }
	  else
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
	      ImageLut(out, img->xsize * bpp, lut);

            ImagePutRow(img, 0, y, img->xsize, out);
	  }
  	  break;
    }
  }

  free(in);
  free(out);

  fclose(fp);

  return (0);
}


/*
 * End of "$Id: image-pnm.c,v 1.7.2.2 2002/03/01 19:55:18 mike Exp $".
 */
