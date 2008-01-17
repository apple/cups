/*
 * "$Id: image-pnm.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   Portable Any Map file routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1993-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   _cupsImageReadPNM() - Read a PNM image file.
 */

/*
 * Include necessary headers...
 */

#include "image-private.h"


/*
 * '_cupsImageReadPNM()' - Read a PNM image file.
 */

int					/* O - Read status */
_cupsImageReadPNM(
    cups_image_t    *img,		/* IO - cupsImage */
    FILE            *fp,		/* I - cupsImage file */
    cups_icspace_t  primary,		/* I - Primary choice for colorspace */
    cups_icspace_t  secondary,		/* I - Secondary choice for colorspace */
    int             saturation,		/* I - Color saturation (%) */
    int             hue,		/* I - Color hue (degrees) */
    const cups_ib_t *lut)		/* I - Lookup table for gamma/brightness */
{
  int		x, y;			/* Looping vars */
  int		bpp;			/* Bytes per pixel */
  cups_ib_t	*in,			/* Input pixels */
		*inptr,			/* Current input pixel */
		*out,			/* Output pixels */
		*outptr,		/* Current output pixel */
		bit;			/* Bit in input line */
  char		line[255],		/* Input line */
		*lineptr;		/* Pointer in line */
  int		format,			/* Format of PNM file */
		val,			/* Pixel value */
		maxval;			/* Maximum pixel value */


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

  if ((lineptr = fgets(line, sizeof(line), fp)) == NULL)
  {
    fputs("DEBUG: Bad PNM header!\n", stderr);
    fclose(fp);
    return (1);
  }

  lineptr ++;

  format = atoi(lineptr);
  while (isdigit(*lineptr & 255))
    lineptr ++;

  while (lineptr != NULL && img->xsize == 0)
  {
    if (*lineptr == '\0' || *lineptr == '#')
      lineptr = fgets(line, sizeof(line), fp);
    else if (isdigit(*lineptr & 255))
    {
      img->xsize = atoi(lineptr);
      while (isdigit(*lineptr & 255))
	lineptr ++;
    }
    else
      lineptr ++;
  }

  while (lineptr != NULL && img->ysize == 0)
  {
    if (*lineptr == '\0' || *lineptr == '#')
      lineptr = fgets(line, sizeof(line), fp);
    else if (isdigit(*lineptr & 255))
    {
      img->ysize = atoi(lineptr);
      while (isdigit(*lineptr & 255))
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
      else if (isdigit(*lineptr & 255))
      {
	maxval = atoi(lineptr);
	while (isdigit(*lineptr & 255))
	  lineptr ++;
      }
      else
	lineptr ++;
    }
  }
  else
    maxval = 1;

  if (img->xsize == 0 || img->xsize > CUPS_IMAGE_MAX_WIDTH ||
      img->ysize == 0 || img->ysize > CUPS_IMAGE_MAX_HEIGHT)
  {
    fprintf(stderr, "DEBUG: Bad PNM dimensions %dx%d!\n",
            img->xsize, img->ysize);
    fclose(fp);
    return (1);
  }

  if (maxval == 0)
  {
    fprintf(stderr, "DEBUG: Bad PNM max value %d!\n", maxval);
    fclose(fp);
    return (1);
  }

  if (format == 1 || format == 2 || format == 4 || format == 5)
    img->colorspace = secondary;
  else
    img->colorspace = (primary == CUPS_IMAGE_RGB_CMYK) ? CUPS_IMAGE_RGB : primary;

  cupsImageSetMaxTiles(img, 0);

  bpp = cupsImageGetDepth(img);

  if ((in = malloc(img->xsize * 3)) == NULL)
  {
    fputs("DEBUG: Unable to allocate memory!\n", stderr);
    fclose(fp);
    return (1);
  }

  if ((out = malloc(img->xsize * bpp)) == NULL)
  {
    fputs("DEBUG: Unable to allocate memory!\n", stderr);
    fclose(fp);
    free(in);
    return (1);
  }

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
              outptr ++;
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
          if (img->colorspace == CUPS_IMAGE_WHITE)
	  {
	    if (lut)
	      cupsImageLut(in, img->xsize, lut);

            _cupsImagePutRow(img, 0, y, img->xsize, in);
	  }
	  else
	  {
	    switch (img->colorspace)
	    {
              default :
		  break;

	      case CUPS_IMAGE_RGB :
		  cupsImageWhiteToRGB(in, out, img->xsize);
		  break;
	      case CUPS_IMAGE_BLACK :
		  cupsImageWhiteToBlack(in, out, img->xsize);
		  break;
	      case CUPS_IMAGE_CMY :
		  cupsImageWhiteToCMY(in, out, img->xsize);
		  break;
	      case CUPS_IMAGE_CMYK :
		  cupsImageWhiteToCMYK(in, out, img->xsize);
		  break;
	    }

	    if (lut)
	      cupsImageLut(out, img->xsize * bpp, lut);

            _cupsImagePutRow(img, 0, y, img->xsize, out);
	  }
	  break;

      default :
	  if ((saturation != 100 || hue != 0) && bpp > 1)
	    cupsImageRGBAdjust(in, img->xsize, saturation, hue);

	  switch (img->colorspace)
	  {
            default :
		break;

	    case CUPS_IMAGE_WHITE :
		cupsImageRGBToWhite(in, out, img->xsize);
		break;
	    case CUPS_IMAGE_RGB :
		cupsImageRGBToRGB(in, out, img->xsize);
		break;
	    case CUPS_IMAGE_BLACK :
		cupsImageRGBToBlack(in, out, img->xsize);
		break;
	    case CUPS_IMAGE_CMY :
		cupsImageRGBToCMY(in, out, img->xsize);
		break;
	    case CUPS_IMAGE_CMYK :
		cupsImageRGBToCMYK(in, out, img->xsize);
		break;
	  }

	  if (lut)
	    cupsImageLut(out, img->xsize * bpp, lut);

          _cupsImagePutRow(img, 0, y, img->xsize, out);
  	  break;
    }
  }

  free(in);
  free(out);

  fclose(fp);

  return (0);
}


/*
 * End of "$Id: image-pnm.c 6649 2007-07-11 21:46:42Z mike $".
 */
