/*
 * "$Id: image-zoom.c,v 1.1 1998/02/02 20:20:02 mike Exp $"
 *
 *   Image zoom routines for espPrint, a collection of printer drivers.
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
 *   ImageZoomAlloc() - Allocate a pixel zoom record...
 *   ImageZoomFill()  - Fill a zoom record with image data utilizing bilinear
 *                      interpolation.
 *   ImageZoomQFill() - Fill a zoom record quickly using nearest-neighbor
 *                      sampling.
 *   ImageZoomFree()  - Free a zoom record...
 *
 * Revision History:
 *
 *   $Log: image-zoom.c,v $
 *   Revision 1.1  1998/02/02 20:20:02  mike
 *   Initial revision
 *
 */

/*
 * Include necessary headers...
 */

#include "image.h"


/*
 * 'ZoomAlloc()' - Allocate a pixel zoom record...
 */

izoom_t *
ImageZoomAlloc(image_t *ip,	/* I - Image to zoom */
               int     x0,	/* I - Upper-lefthand corner */
               int     y0,	/* I - ... */
               int     x1,	/* I - Lower-righthand corner */
               int     y1,	/* I - ... */
               int     xsize,	/* I - Final width of image */
               int     ysize,	/* I - Final height of image */
               int     rotated)	/* I - Non-zero if image is rotated 90 degs */
{
  izoom_t	*z;		/* New zoom record */
  int		width,		/* Width of image sub-region */
		height;		/* Height of ... */


  if ((z = (izoom_t *)calloc(1, sizeof(izoom_t))) == NULL)
    return (NULL);

  if ((z->rows[0] = (unsigned char *)calloc(xsize, ip->pdepth * ip->ldepth)) == NULL)
  {
    free(z);
    return (NULL);
  };

  if ((z->rows[1] = (unsigned char *)calloc(xsize, ip->pdepth * ip->ldepth)) == NULL)
  {
    free(z->rows[0]);
    free(z);
    return (NULL);
  };

  z->ip  = ip;
  z->row = 0;

  width  = x1 - x0 + 1;
  height = y1 - y0 + 1;

  if (rotated)
  {
    z->top_pixel = ip->pixels + x1 * ip->pdepth +
                   y0 * ip->width * ip->ldepth * ip->pdepth;

    z->xmult = ip->width * ip->pdepth * ip->ldepth;
    z->ymult = -ip->pdepth;

    z->xsize = xsize;
    z->ysize = ysize;

    z->xmod  = height % xsize;
    z->ymod  = width % ysize;
    z->xstep = height / xsize;
    z->ystep = width / ysize;
    z->pstep = z->xmult * (height / xsize);

    if (height < ip->height)
      z->xmax = height;
    else
      z->xmax = height - 1;

    if (width < ip->width)
      z->ymax = width;
    else
      z->ymax = width - 1;
  }
  else
  {
    z->top_pixel = ip->pixels + x0 * ip->pdepth +
                   y0 * ip->width * ip->ldepth * ip->pdepth;

    z->xmult = ip->pdepth;
    z->ymult = ip->width * ip->pdepth * ip->ldepth;
    z->dmult = ip->width * ip->pdepth;

    z->xsize = xsize;
    z->ysize = ysize;

    z->xmod  = width % xsize;
    z->ymod  = height % ysize;
    z->xstep = width / xsize;
    z->ystep = height / ysize;
    z->pstep = z->xmult * (width / xsize);

    if (width < ip->width)
      z->xmax = width;
    else
      z->xmax = width - 1;

    if (height < ip->height)
      z->ymax = height;
    else
      z->ymax = height - 1;
  };

  return (z);
}


/*
 * 'ImageZoomFill()' - Fill a zoom record with image data utilizing bilinear
 *                     interpolation.
 */

void
ImageZoomFill(izoom_t *z,	/* I - Zoom record to fill */
              int  iy,		/* I - Zoom image row */
              int  idepth)	/* I - Image depth (for planar images) */
{
  unsigned char	*r,	/* Row pointer */
		*p;	/* Pixel pointer */
  int		xerr0,	/* X error counter */
		xerr1;	/* ... */
  int		ix, x, ldepth, z_xmult, z_ldepth, z_xmax, z_xsize, z_pdepth;


  if (iy > z->ymax)
    iy = z->ymax;

  z->row ^= 1;

  r = z->rows[z->row];

  z_xmax   = z->xmax;
  z_xsize  = z->xsize;
  z_xmult  = z->xmult;
  z_ldepth = z->ip->ldepth;
  z_pdepth = z->ip->pdepth;

  for (ldepth = 0; ldepth < z_ldepth; ldepth ++)
  {
    p = z->top_pixel + iy * z->ymult +
        ldepth * z->ip->width * z_pdepth +
        idepth * z->ip->width * z->ip->height * z_pdepth * z_ldepth;

    for (x = z_xsize, xerr0 = z_xsize, xerr1 = 0, ix = 0;
         x > 0;
         x --)
    {
      if (ix < z_xmax)
      {
        r[0] = (p[0] * xerr0 + p[z_xmult] * xerr1) / z_xsize;

        if (z_pdepth > 1)
        {
          r[1] = (p[1] * xerr0 + p[1 + z_xmult] * xerr1) / z_xsize;
          r[2] = (p[2] * xerr0 + p[2 + z_xmult] * xerr1) / z_xsize;
        };

        if (z_pdepth > 3)
          r[3] = (p[3] * xerr0 + p[3 + z_xmult] * xerr1) / z_xsize;
      }
      else
      {
        r[0] = p[0];

        if (z_pdepth > 1)
        {
          r[1] = p[1];
          r[2] = p[2];
        };

        if (z_pdepth > 3)
          r[3] = p[3];
      };

      r     += z_pdepth;
      ix    += z->xstep;
      p     += z->pstep;
      xerr0 -= z->xmod;
      xerr1 += z->xmod;

      if (xerr0 <= 0)
      {
        xerr0 += z_xsize;
        xerr1 -= z_xsize;
        ix ++;
        p     += z_xmult;
      };
    };
  };
}


/*
 * 'ImageZoomQFill()' - Fill a zoom record quickly using nearest-neighbor sampling.
 */

void
ImageZoomQFill(izoom_t *z,	/* I - Zoom record to fill */
               int  iy,		/* I - Image row */
               int  idepth)	/* I - Image depth (for planar images) */
{
  unsigned char	*r,		/* Row pointer */
		*p;		/* Pixel pointer */
  int		xerr0,		/* X error counter */
		xerr1;		/* ... */
  int		ix, x, ldepth, z_xmult, z_ldepth, z_xmax, z_xsize, z_pdepth;


  if (iy > z->ymax)
    iy = z->ymax;

  z->row = 0;

  r = z->rows[z->row];

  z_xmax   = z->xmax;
  z_xsize  = z->xsize;
  z_xmult  = z->xmult;
  z_ldepth = z->ip->ldepth;
  z_pdepth = z->ip->pdepth;

  for (ldepth = 0; ldepth < z_ldepth; ldepth ++)
  {
    p = z->top_pixel + iy * z->ymult +
        ldepth * z->ip->width * z_pdepth +
        idepth * z->ip->width * z->ip->height * z_pdepth * z_ldepth;

    for (x = z_xsize, xerr0 = z_xsize, xerr1 = 0, ix = 0;
         x > 0;
         x --)
    {
      r[0] = p[0];

      if (z_pdepth > 1)
      {
        r[1] = p[1];
        r[2] = p[2];
      };

      if (z_pdepth > 3)
        r[3] = p[3];

      r     += z_pdepth;
      ix    += z->xstep;
      p     += z->pstep;
      xerr0 -= z->xmod;
      xerr1 += z->xmod;

      if (xerr0 <= 0)
      {
        xerr0 += z_xsize;
        xerr1 -= z_xsize;
        ix ++;
        p     += z_xmult;
      };
    };
  };
}


/*
 * 'ImageZoomFree()' - Free a zoom record...
 */

void
ImageZoomFree(izoom_t *z)	/* I - Zoom record to free */
{
  free(z->rows[0]);
  free(z->rows[1]);
  free(z);
}


/*
 * End of "$Id: image-zoom.c,v 1.1 1998/02/02 20:20:02 mike Exp $".
 */
