/*
 * "$Id: image.h,v 1.2 1995/10/10 01:20:26 mike Exp $"
 *
 *   Image file definitions for espPrint, a collection of printer/image
 *   software.
 *
 *   Copyright 1993-1995 by Easy Software Products
 *
 *   These coded instructions, statements, and computer  programs  contain
 *   unpublished  proprietary  information  of Easy Software Products, and
 *   are protected by Federal copyright law.  They may  not  be  disclosed
 *   to  third  parties  or  copied or duplicated in any form, in whole or
 *   in part, without the prior written consent of Easy Software Products.
 *
 * Revision History:
 *
 *   $Log: image.h,v $
 *   Revision 1.2  1995/10/10 01:20:26  mike
 *   Converted to C++, because everyone knows that C++ is better than ANSI C.
 *   (read LOTS of sarcasm there)
 *
 *   The C++ version doesn't require il_eoe.sw.c, which is not loaded by
 *   default.
 *
 *   Fixed the image scaling for rotated images.
 *
 *   Revision 1.1  1995/07/04  22:16:59  mike
 *   Initial revision
 */

#ifndef _IMAGE_H_
# define _IMAGE_H_

/*
 * Include necessary headers...
 */

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <il/ilDefs.h>
# include <il/ilGenericImgFile.h>
# include <il/ilCoord.h>
# include <il/ilColor.h>
# include <il/ilImage.h>
# include <il/ilRGBImg.h>
# include <il/ilCMYKImg.h>
# include <il/ilConfig.h>
# include <il/ilConfigure.h>


/*
 * Image buffer format...
 */

typedef struct
{
  int		width,		/* Width of image in pixels */
		height,		/* Height of image in pixels */
		pdepth,		/* Number of components per pixel */
		ldepth,		/* Number of components per line */
		idepth;		/* Number of components per image */
  ilColorModel	type;		/* Colorspace of image */
  unsigned char	*pixels;	/* Pixel data */
} IMAGE;


/*
 * Image row zooming structure...
 */

typedef struct
{
  IMAGE		*ip;
  unsigned char	*top_pixel;
  int		xmult,
		ymult,
		dmult,
		xsize,
		ysize,
		xmax,
		ymax,
		xmod,
		ymod,
		xstep,
		ystep,
		pstep,
		row;
  unsigned char	*rows[2];
} ZOOM;


/*
 * Prototypes...
 */

extern IMAGE	*ImageLoad(char *filename, ilColorModel type, ilOrder order,
		           int *channels);
extern void	ImageFree(IMAGE *ip);

extern ZOOM	*ZoomAlloc(IMAGE *ip, int x0, int y0, int x1, int y1,
		           int xsize, int ysize, int rotated);
extern void	ZoomFill(ZOOM *z, int row, int idepth);
extern void	ZoomQFill(ZOOM *z, int row, int idepth);
extern void	ZoomFree(ZOOM *z);

#endif /* !_IMAGE_H_ */

/*
 * End of "$Id: image.h,v 1.2 1995/10/10 01:20:26 mike Exp $".
 */
