/*
 * "$Id: image-photocd.c,v 1.3 1999/03/24 18:01:43 mike Exp $"
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

#ifdef HAVE_LIBPCD
#  include <pcd.h>


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
  int		y;		/* Looping vars */
  int		bpp;		/* Bytes per pixel */
  ib_t		*in,		/* Input pixels */
		*out;		/* Output pixels */


 /*
  * Setup the PhotoCD file...
  */

  if (pcd_fdopen(&pcd, fileno(fp)))
  {
    fclose(fp);
    return (-1);
  }

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
	}

        ImagePutRow(img, 0, y, img->xsize, out);
      }
    }
  }

  free(in);
  free(out);

  pcd_close(&pcd);

  return (0);
}


#endif /* HAVE_LIBPCD */


/*
 * End of "$Id: image-photocd.c,v 1.3 1999/03/24 18:01:43 mike Exp $".
 */
