/*
 * "$Id: image-pnm.c,v 1.2 1999/03/06 18:11:35 mike Exp $"
 *
 *   Portable Any Map file routines for espPrint, a collection of printer
 *   drivers.
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
 *   $Log: image-pnm.c,v $
 *   Revision 1.2  1999/03/06 18:11:35  mike
 *   Checkin for CVS.
 *
 *   Revision 1.1  1998/02/19  20:43:33  mike
 *   Initial revision
 *
 *   Revision 1.1  1998/02/19  20:43:33  mike
 *   Initial revision
 *
 */

/*
 * Include necessary headers...
 */

#include "image.h"
#include <ctype.h>

int
ImageReadPNM(image_t *img,
             FILE    *fp,
             int     primary,
             int     secondary,
             int     saturation,
             int     hue)
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
  };

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
  };

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
    };
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
          };
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
            };
          };
          break;

      case 5 :
          fread(in, img->xsize, 1, fp);
          break;

      case 6 :
          fread(in, img->xsize, 3, fp);
          break;
    };

    switch (format)
    {
      case 1 :
      case 2 :
      case 4 :
      case 5 :
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
	  break;

      default :
	  if ((saturation != 100 || hue != 0) && bpp > 1)
	    ImageRGBAdjust(in, img->xsize, saturation, hue);

	  if (img->colorspace == IMAGE_RGB)
            ImagePutRow(img, 0, y, img->xsize, in);
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
	    };

            ImagePutRow(img, 0, y, img->xsize, out);
	  };
  	  break;
    };
  };

  free(in);
  free(out);

  fclose(fp);

  return (0);
}


/*
 * End of "$Id: image-pnm.c,v 1.2 1999/03/06 18:11:35 mike Exp $".
 */
