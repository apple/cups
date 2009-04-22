/*
 * "$Id: image-private.h 7473 2008-04-21 17:51:58Z mike $"
 *
 *   Private image library definitions for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1993-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _CUPS_IMAGE_PRIVATE_H_
#  define _CUPS_IMAGE_PRIVATE_H_

/*
 * Include necessary headers...
 */

#  include "image.h"
#  include <cups/cups.h>
#  include <cups/debug.h>
#  include <cups/string.h>
#  include <stdlib.h>
#  include <string.h>
#  include <unistd.h>
#  include <errno.h>
#  include <math.h>


/*
 * Constants...
 */

#  define CUPS_IMAGE_MAX_WIDTH	0x07ffffff
					/* 2^27-1 to allow for 15-channel data */
#  define CUPS_IMAGE_MAX_HEIGHT	0x3fffffff
					/* 2^30-1 */

#  define CUPS_TILE_SIZE	256	/* 256x256 pixel tiles */
#  define CUPS_TILE_MINIMUM	10	/* Minimum number of tiles */


/*
 * min/max/abs macros...
 */

#  ifndef max
#    define 	max(a,b)	((a) > (b) ? (a) : (b))
#  endif /* !max */
#  ifndef min
#    define 	min(a,b)	((a) < (b) ? (a) : (b))
#  endif /* !min */
#  ifndef abs
#    define	abs(a)		((a) < 0 ? -(a) : (a))
#  endif /* !abs */


/*
 * Types and structures...
 */

typedef enum cups_iztype_e		/**** Image zoom type ****/
{
  CUPS_IZOOM_FAST,			/* Use nearest-neighbor sampling */
  CUPS_IZOOM_NORMAL,			/* Use bilinear interpolation */
  CUPS_IZOOM_BEST			/* Use bicubic interpolation */
} cups_iztype_t;

struct cups_ic_s;

typedef struct cups_itile_s		/**** Image tile ****/
{
  int			dirty;		/* True if tile is dirty */
  off_t			pos;		/* Position of tile on disk (-1 if not written) */
  struct cups_ic_s	*ic;		/* Pixel data */
} cups_itile_t;

typedef struct cups_ic_s		/**** Image tile cache ****/
{
  struct cups_ic_s	*prev,		/* Previous tile in cache */
			*next;		/* Next tile in cache */
  cups_itile_t		*tile;		/* Tile this is attached to */
  cups_ib_t		*pixels;	/* Pixel data */
} cups_ic_t;

struct cups_image_s			/**** Image file data ****/
{
  cups_icspace_t	colorspace;	/* Colorspace of image */
  unsigned		xsize,		/* Width of image in pixels */
			ysize,		/* Height of image in pixels */
			xppi,		/* X resolution in pixels-per-inch */
			yppi,		/* Y resolution in pixels-per-inch */
			num_ics,	/* Number of cached tiles */
			max_ics;	/* Maximum number of cached tiles */
  cups_itile_t		**tiles;	/* Tiles in image */
  cups_ic_t		*first,		/* First cached tile in image */
			*last;		/* Last cached tile in image */
  int			cachefile;	/* Tile cache file */
  char			cachename[256];	/* Tile cache filename */
};

struct cups_izoom_s			/**** Image zoom data ****/
{
  cups_image_t		*img;		/* Image to zoom */
  cups_iztype_t		type;		/* Type of zooming */
  unsigned		xorig,		/* X origin */
			yorig,		/* Y origin */
			width,		/* Width of input area */
			height,		/* Height of input area */
			depth,		/* Number of bytes per pixel */
			rotated,	/* Non-zero if image needs to be rotated */
			xsize,		/* Width of output image */
			ysize,		/* Height of output image */
			xmax,		/* Maximum input image X position */
			ymax,		/* Maximum input image Y position */
			xmod,		/* Threshold for Bresenheim rounding */
			ymod;		/* ... */
  int			xstep,		/* Amount to step for each pixel along X */
			xincr,
			instep,		/* Amount to step pixel pointer along X */
			inincr,
			ystep,		/* Amount to step for each pixel along Y */
			yincr,
			row;		/* Current row */
  cups_ib_t		*rows[2],	/* Horizontally scaled pixel data */
			*in;		/* Unscaled input pixel data */
};


/*
 * Prototypes...
 */

extern int		_cupsImagePutCol(cups_image_t *img, int x, int y,
			                 int height, const cups_ib_t *pixels);
extern int		_cupsImagePutRow(cups_image_t *img, int x, int y,
			                 int width, const cups_ib_t *pixels);
extern int		_cupsImageReadBMP(cups_image_t *img, FILE *fp,
			                  cups_icspace_t primary,
					  cups_icspace_t secondary,
			                  int saturation, int hue,
					  const cups_ib_t *lut);
extern int		_cupsImageReadFPX(cups_image_t *img, FILE *fp,
			                  cups_icspace_t primary,
					  cups_icspace_t secondary,
			                  int saturation, int hue,
					  const cups_ib_t *lut);
extern int		_cupsImageReadGIF(cups_image_t *img, FILE *fp,
			                  cups_icspace_t primary,
					  cups_icspace_t secondary,
			                  int saturation, int hue,
					  const cups_ib_t *lut);
extern int		_cupsImageReadJPEG(cups_image_t *img, FILE *fp,
			                   cups_icspace_t primary,
					   cups_icspace_t secondary,
			                   int saturation, int hue,
					   const cups_ib_t *lut);
extern int		_cupsImageReadPIX(cups_image_t *img, FILE *fp,
			                  cups_icspace_t primary,
					  cups_icspace_t secondary,
			                  int saturation, int hue,
					  const cups_ib_t *lut);
extern int		_cupsImageReadPNG(cups_image_t *img, FILE *fp,
			                  cups_icspace_t primary,
					  cups_icspace_t secondary,
			                  int saturation, int hue,
					  const cups_ib_t *lut);
extern int		_cupsImageReadPNM(cups_image_t *img, FILE *fp,
			                  cups_icspace_t primary,
					  cups_icspace_t secondary,
			                  int saturation, int hue,
					  const cups_ib_t *lut);
extern int		_cupsImageReadPhotoCD(cups_image_t *img, FILE *fp,
			                      cups_icspace_t primary,
			                      cups_icspace_t secondary,
					      int saturation, int hue,
					      const cups_ib_t *lut);
extern int		_cupsImageReadSGI(cups_image_t *img, FILE *fp,
			                  cups_icspace_t primary,
					  cups_icspace_t secondary,
			                  int saturation, int hue,
					  const cups_ib_t *lut);
extern int		_cupsImageReadSunRaster(cups_image_t *img, FILE *fp,
			                        cups_icspace_t primary,
			                        cups_icspace_t secondary,
						int saturation, int hue,
					        const cups_ib_t *lut);
extern int		_cupsImageReadTIFF(cups_image_t *img, FILE *fp,
			                   cups_icspace_t primary,
					   cups_icspace_t secondary,
			                   int saturation, int hue,
					   const cups_ib_t *lut);
extern void		_cupsImageZoomDelete(cups_izoom_t *z);
extern void		_cupsImageZoomFill(cups_izoom_t *z, int iy);
extern cups_izoom_t	*_cupsImageZoomNew(cups_image_t *img, int xc0, int yc0,
			                   int xc1, int yc1, int xsize,
					   int ysize, int rotated,
					   cups_iztype_t type);

extern int		_cupsRasterExecPS(cups_page_header2_t *h,
			                  int *preferred_bits,
			                  const char *code);
extern void		_cupsRasterAddError(const char *f, ...);
extern void		_cupsRasterClearError(void);

#endif /* !_CUPS_IMAGE_PRIVATE_H_ */

/*
 * End of "$Id: image-private.h 7473 2008-04-21 17:51:58Z mike $".
 */
