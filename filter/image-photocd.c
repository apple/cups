/*
 * "$Id: image-photocd.c,v 1.1 1998/02/19 20:43:33 mike Exp $"
 *
 *   PhotoCD routines for espPrint, a collection of printer drivers.
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
 *   $Log: image-photocd.c,v $
 *   Revision 1.1  1998/02/19 20:43:33  mike
 *   Initial revision
 *
 */

/*
 * Include necessary headers...
 */

#include "image.h"
#include <pcd.h>


int
ImageReadPhotoCD(image_t *img,
                 FILE    *fp,
                 int     primary,
                 int     secondary,
        	 int     saturation,
        	 int     hue)
{
  struct PCD_IMAGE	pcd;	/* PhotoCD image structure */
  int		rot,		/* Image rotation */
		res,		/* Image resolution */
		left, top,	/* Left and top of image area */
		width, height;	/* Width and height of image */
  int		i, y;		/* Looping vars */
  int		bpp;		/* Bytes per pixel */
  ib_t		*in,		/* Input pixels */
		*inptr,		/* Current input pixel */
		*out;		/* Output pixels */


 /*
  * Setup the PhotoCD file...
  */

  if (pcd_fdopen(&pcd, fileno(fp)))
  {
    fclose(fp);
    return (-1);
  };

 /*
  * Get the image dimensions and load the output image...
  */

  rot    = pcd_get_rot(&pcd, 0);
  res    = pcd_get_maxres(&pcd);
  res    = res > 3 ? 3 : res;
  left   = 0;
  top    = 0;
  width  = PCD_WIDTH(res, rot);
  height = PCD_HEIGHT(res, rot);

  if (primary == IMAGE_WHITE || primary == IMAGE_BLACK)
    pcd_select(&pcd, res, 0, 1, 0, rot, &left, &top, &width, &height);
  else
    pcd_select(&pcd, res, 0, 0, 0, rot, &left, &top, &width, &height);

  img->colorspace = primary;
  img->xsize      = width;
  img->ysize      = height;
  img->xppi       = (rot & 1) ? width / 4 : width / 6;
  img->yppi       = img->xppi;

  ImageSetMaxTiles(img, 0);

  bpp = ImageGetDepth(img);
  in  = malloc(img->xsize * (pcd.gray ? 1 : 3));
  out = malloc(img->xsize * bpp);

 /*
  * Read the image file...
  */

  for (y = 0; y < img->ysize; y ++)
  {
    if (pcd.gray)
    {
      pcd_get_image_line(&pcd, y, in, PCD_TYPE_GRAY, 0);

      if (primary == IMAGE_BLACK)
      {
        ImageWhiteToBlack(in, out, img->xsize);
        ImagePutRow(img, 0, y, img->xsize, out);
      }
      else
        ImagePutRow(img, 0, y, img->xsize, in);
    }
    else
    {
      pcd_get_image_line(&pcd, y, in, PCD_TYPE_RGB, 0);

      if (saturation != 100 || hue != 0)
	ImageRGBAdjust(in, img->xsize, saturation, hue);

      if (primary == IMAGE_RGB)
        ImagePutRow(img, 0, y, img->xsize, in);
      else
      {
	switch (img->colorspace)
	{
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

  pcd_close(&pcd);

  return (0);
}


/*
 * End of "$Id: image-photocd.c,v 1.1 1998/02/19 20:43:33 mike Exp $".
 */
