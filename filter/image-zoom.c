/*
 * "$Id: image-zoom.c,v 1.6 2000/01/04 13:45:45 mike Exp $"
 *
 *   Image zoom routines for the Common UNIX Printing System (CUPS).
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
 *   ImageZoomAlloc() - Allocate a pixel zoom record...
 *   ImageZoomFill()  - Fill a zoom record with image data utilizing bilinear
 *                      interpolation.
 *   ImageZoomQFill() - Fill a zoom record quickly using nearest-neighbor
 *                      sampling.
 *   ImageZoomFree()  - Free a zoom record...
 */

/*
 * Include necessary headers...
 */

#include "image.h"


/*
 * 'ZoomAlloc()' - Allocate a pixel zoom record...
 */

izoom_t *
ImageZoomAlloc(image_t *img,	/* I - Image to zoom */
               int     x0,	/* I - Upper-lefthand corner */
               int     y0,	/* I - ... */
               int     x1,	/* I - Lower-righthand corner */
               int     y1,	/* I - ... */
               int     xsize,	/* I - Final width of image */
               int     ysize,	/* I - Final height of image */
               int     rotated)	/* I - Non-zero if image is rotated 90 degs */
{
  izoom_t	*z;		/* New zoom record */


  if ((z = (izoom_t *)calloc(1, sizeof(izoom_t))) == NULL)
    return (NULL);

  z->img     = img;
  z->row     = 0;
  z->depth   = ImageGetDepth(img);
  z->rotated = rotated;

  if (rotated)
  {
    z->xorig   = x1;
    z->yorig   = y0;
    z->width   = y1 - y0 + 1;
    z->height  = x1 - x0 + 1;
    z->xsize   = xsize;
    z->ysize   = ysize;
    z->xmod    = z->width % z->xsize;
    z->xstep   = z->width / z->xsize;
    z->xincr   = 1;
    z->ymod    = z->height % z->ysize;
    z->ystep   = z->height / z->ysize;
    z->yincr   = 1;
    z->instep  = z->xstep * z->depth;
    z->inincr  = z->xincr * z->depth;

    if (z->width < img->ysize)
      z->xmax = z->width;
    else
      z->xmax = z->width - 1;

    if (z->height < img->xsize)
      z->ymax = z->height;
    else
      z->ymax = z->height - 1;
  }
  else
  {
    z->xorig   = x0;
    z->yorig   = y0;
    z->width   = x1 - x0 + 1;
    z->height  = y1 - y0 + 1;
    z->xsize   = xsize;
    z->ysize   = ysize;
    z->xmod    = z->width % z->xsize;
    z->xstep   = z->width / z->xsize;
    z->xincr   = 1;
    z->ymod    = z->height % z->ysize;
    z->ystep   = z->height / z->ysize;
    z->yincr   = 1;
    z->instep  = z->xstep * z->depth;
    z->inincr  = z->xincr * z->depth;

    if (z->width < img->xsize)
      z->xmax = z->width;
    else
      z->xmax = z->width - 1;

    if (z->height < img->ysize)
      z->ymax = z->height;
    else
      z->ymax = z->height - 1;
  }

  if ((z->rows[0] = (ib_t *)malloc(z->xsize * z->depth)) == NULL)
  {
    free(z);
    return (NULL);
  }

  if ((z->rows[1] = (ib_t *)malloc(z->xsize * z->depth)) == NULL)
  {
    free(z->rows[0]);
    free(z);
    return (NULL);
  }

  if ((z->in = (ib_t *)malloc(z->width * z->depth)) == NULL)
  {
    free(z->rows[0]);
    free(z->rows[1]);
    free(z);
    return (NULL);
  }

  return (z);
}


/*
 * 'ImageZoomFill()' - Fill a zoom record with image data utilizing bilinear
 *                     interpolation.
 */

void
ImageZoomFill(izoom_t *z,	/* I - Zoom record to fill */
              int     iy)	/* I - Zoom image row */
{
  ib_t	*r,			/* Row pointer */
	*inptr;			/* Pixel pointer */
  int	xerr0,			/* X error counter */
	xerr1;			/* ... */
  int	ix,
	x,
	count,
	z_depth,
	z_xstep,
	z_xincr,
	z_instep,
	z_inincr,
	z_xmax,
	z_xmod,
	z_xsize;


  if (iy > z->ymax)
    iy = z->ymax;

  z->row ^= 1;

  z_depth  = z->depth;
  z_xsize  = z->xsize;
  z_xmax   = z->xmax;
  z_xmod   = z->xmod;
  z_xstep  = z->xstep;
  z_xincr  = z->xincr;
  z_instep = z->instep;
  z_inincr = z->inincr;

  if (z->rotated)
    ImageGetCol(z->img, z->xorig - iy, z->yorig, z->width, z->in);
  else
    ImageGetRow(z->img, z->xorig, z->yorig + iy, z->width, z->in);

  if (z_inincr < 0)
    inptr = z->in + (z->width - 1) * z_depth;
  else
    inptr = z->in;

  for (x = z_xsize, xerr0 = z_xsize, xerr1 = 0, ix = 0, r = z->rows[z->row];
       x > 0;
       x --)
  {
    if (ix < z_xmax)
    {
      for (count = 0; count < z_depth; count ++)
        *r++ = (inptr[count] * xerr0 + inptr[z_depth + count] * xerr1) / z_xsize;
    }
    else
    {
      for (count = 0; count < z_depth; count ++)
        *r++ = inptr[count];
    }

    ix    += z_xstep;
    inptr += z_instep;
    xerr0 -= z_xmod;
    xerr1 += z_xmod;

    if (xerr0 <= 0)
    {
      xerr0 += z_xsize;
      xerr1 -= z_xsize;
      ix    += z_xincr;
      inptr += z_inincr;
    }
  }
}


/*
 * 'ImageZoomQFill()' - Fill a zoom record quickly using nearest-neighbor sampling.
 */

void
ImageZoomQFill(izoom_t *z,	/* I - Zoom record to fill */
               int     iy)	/* I - Zoom image row */
{
  ib_t	*r,			/* Row pointer */
	*inptr;			/* Pixel pointer */
  int	xerr0;			/* X error counter */
  int	ix,
	x,
	count,
	z_depth,
	z_xstep,
	z_xincr,
	z_instep,
	z_inincr,
	z_xmod,
	z_xsize;


  if (iy > z->ymax)
    iy = z->ymax;

  z->row ^= 1;

  z_depth  = z->depth;
  z_xsize  = z->xsize;
  z_xmod   = z->xmod;
  z_xstep  = z->xstep;
  z_xincr  = z->xincr;
  z_instep = z->instep;
  z_inincr = z->inincr;

  if (z->rotated)
    ImageGetCol(z->img, z->xorig - iy, z->yorig, z->width, z->in);
  else
    ImageGetRow(z->img, z->xorig, z->yorig + iy, z->width, z->in);

  if (z_inincr < 0)
    inptr = z->in + (z->width - 1) * z_depth;
  else
    inptr = z->in;

  for (x = z_xsize, xerr0 = z_xsize, ix = 0, r = z->rows[z->row];
       x > 0;
       x --)
  {
    for (count = 0; count < z_depth; count ++)
      *r++ = inptr[count];

    ix    += z_xstep;
    inptr += z_instep;
    xerr0 -= z_xmod;

    if (xerr0 <= 0)
    {
      xerr0 += z_xsize;
      ix    += z_xincr;
      inptr += z_inincr;
    }
  }
}


/*
 * 'ImageZoomFree()' - Free a zoom record...
 */

void
ImageZoomFree(izoom_t *z)	/* I - Zoom record to free */
{
  free(z->rows[0]);
  free(z->rows[1]);
  free(z->in);
  free(z);
}


/*
 * End of "$Id: image-zoom.c,v 1.6 2000/01/04 13:45:45 mike Exp $".
 */
