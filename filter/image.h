/*
 * "$Id: image.h,v 1.4 1998/07/28 20:48:30 mike Exp $"
 *
 *   Image library definitions for espPrint, a collection of printer drivers.
 *
 *   Copyright 1993-1998 by Easy Software Products
 *
 *   These coded instructions, statements, and computer programs contain
 *   unpublished proprietary information of Easy Software Products, and
 *   are protected by Federal copyright law.  They may not be disclosed
 *   to third parties or copied or duplicated in any form, in whole or
 *   in part, without the prior written consent of Easy Software Products.
 *
 * Revision History:
 *
 *   $Log: image.h,v $
 *   Revision 1.4  1998/07/28 20:48:30  mike
 *   Moved min, max, and abs macros from colorspace.c to here...
 *
 *   Revision 1.3  1998/02/19  20:44:58  mike
 *   Image file definitions.
 *
 */

#ifndef _IMAGE_H_
#  define _IMAGE_H_

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


/*
 * Colorspaces...
 */

#  define IMAGE_CMYK	-4		/* Cyan, magenta, yellow, and black */
#  define IMAGE_CMY	-3		/* Cyan, magenta, and yellow */
#  define IMAGE_BLACK	-1		/* Black */
#  define IMAGE_WHITE	1		/* White (luminance) */
#  define IMAGE_RGB	3		/* Red, green, and blue */

/*
 * Tile definitions...
 */

#  define TILE_SIZE	256		/* 256x256 pixel tiles */
#  define TILE_MINIMUM	10		/* Minimum number of tiles */

/*
 * min/max/abs macros...
 */

#ifndef max
#  define 	max(a,b)	((a) > (b) ? (a) : (b))
#endif /* !max */
#ifndef min
#  define 	min(a,b)	((a) < (b) ? (a) : (b))
#endif /* !min */
#ifndef abs
#  define	abs(a)		((a) < 0 ? -(a) : (a))
#endif /* !abs */


/*
 * Image byte type...
 */

typedef unsigned char ib_t;

/*
 * Tile cache structure...
 */

typedef struct ic_str
{
  struct ic_str	*prev,		/* Previous tile in cache */
		*next;		/* Next tile in cache */
  void		*tile;		/* Tile this is attached to */
  ib_t		*pixels;	/* Pixel data */
} ic_t;

/*
 * Tile structure...
 */

typedef struct
{
  int		dirty,		/* True if tile is dirty */
		pos;		/* Position of tile on disk (-1 if not written) */
  ic_t		*ic;		/* Pixel data */
} itile_t;

/*
 * Image structure...
 */

typedef struct
{
  int		colorspace,	/* Colorspace of image */
		xsize,		/* Width of image in pixels */
		ysize,		/* Height of image in pixels */
		xppi,		/* X resolution in pixels-per-inch */
		yppi,		/* Y resolution in pixels-per-inch */
		num_ics,	/* Number of cached tiles */
		max_ics;	/* Maximum number of cached tiles */
  itile_t	**tiles;	/* Tiles in image */
  ic_t		*first,		/* First cached tile in image */
		*last;		/* Last cached tile in image */
  FILE		*cachefile;	/* Tile cache file */
  char		cachename[256];	/* Tile cache filename */
} image_t;

/*
 * Image row zooming structure...
 */

typedef struct
{
  image_t	*img;		/* Image to zoom */
  int		xorig,
		yorig,
		width,		/* Width of input area */
		height,		/* Height of input area */
		depth,		/* Number of bytes per pixel */
		rotated,	/* Non-zero if image needs to be rotated */
		xsize,		/* Width of output image */
		ysize,		/* Height of output image */
		xmax,		/* Maximum input image X position */
		ymax,		/* Maximum input image Y position */
		xmod,		/* Threshold for Bresenheim rounding */
		ymod,		/* ... */
		xstep,		/* Amount to step for each pixel along X */
		xincr,
		instep,		/* Amount to step pixel pointer along X */
		inincr,
		ystep,		/* Amount to step for each pixel along Y */
		yincr,
		row;		/* Current row */
  ib_t		*rows[2],	/* Horizontally scaled pixel data */
		*in;		/* Unscaled input pixel data */
} izoom_t;


/*
 * Basic image functions...
 */

extern image_t	*ImageOpen(char *filename, int primary, int secondary,
		           int saturation, int hue);
extern void	ImageClose(image_t *img);
extern void	ImageSetMaxTiles(image_t *img, int max_tiles);

#define 	ImageGetDepth(img)	((img)->colorspace < 0 ? -(img)->colorspace : (img)->colorspace)
extern int	ImageGetCol(image_t *img, int x, int y, int height, ib_t *pixels);
extern int	ImageGetRow(image_t *img, int x, int y, int width, ib_t *pixels);
extern int	ImagePutCol(image_t *img, int x, int y, int height, ib_t *pixels);
extern int	ImagePutRow(image_t *img, int x, int y, int width, ib_t *pixels);

/*
 * File formats...
 */

extern int	ImageReadGIF(image_t *img, FILE *fp, int primary, int secondary,
		             int saturation, int hue);
extern int	ImageReadJPEG(image_t *img, FILE *fp, int primary, int secondary,
		              int saturation, int hue);
extern int	ImageReadPNG(image_t *img, FILE *fp, int primary, int secondary,
		             int saturation, int hue);
extern int	ImageReadPNM(image_t *img, FILE *fp, int primary, int secondary,
		             int saturation, int hue);
extern int	ImageReadPhotoCD(image_t *img, FILE *fp, int primary,
		                 int secondary, int saturation, int hue);
extern int	ImageReadSGI(image_t *img, FILE *fp, int primary, int secondary,
		             int saturation, int hue);
extern int	ImageReadSunRaster(image_t *img, FILE *fp, int primary,
		                   int secondary, int saturation, int hue);
extern int	ImageReadTIFF(image_t *img, FILE *fp, int primary, int secondary,
		              int saturation, int hue);

/*
 * Colorspace conversions...
 */

extern void	ImageWhiteToBlack(ib_t *in, ib_t *out, int count);
extern void	ImageWhiteToRGB(ib_t *in, ib_t *out, int count);
extern void	ImageWhiteToCMY(ib_t *in, ib_t *out, int count);
extern void	ImageWhiteToCMYK(ib_t *in, ib_t *out, int count);

extern void	ImageRGBToBlack(ib_t *in, ib_t *out, int count);
extern void	ImageRGBToWhite(ib_t *in, ib_t *out, int count);
extern void	ImageRGBToCMY(ib_t *in, ib_t *out, int count);
extern void	ImageRGBToCMYK(ib_t *in, ib_t *out, int count);

extern void	ImageRGBAdjust(ib_t *pixels, int count, int saturation, int hue);

extern void	ImageLut(ib_t *lut, ib_t *pixels, int count, int skip);

/*
 * Image scaling operations...
 */

extern izoom_t	*ImageZoomAlloc(image_t *img, int x0, int y0, int x1, int y1,
		                int xsize, int ysize, int rotated);
extern void	ImageZoomFill(izoom_t *z, int iy, ib_t *luts);
extern void	ImageZoomQFill(izoom_t *z, int iy, ib_t *luts);
extern void	ImageZoomFree(izoom_t *z);


#endif /* !_IMAGE_H_ */

/*
 * End of "$Id: image.h,v 1.4 1998/07/28 20:48:30 mike Exp $".
 */
