/*
 * "$Id: image.h,v 1.1 1995/07/04 22:16:59 mike Exp $"
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
 *   Revision 1.1  1995/07/04 22:16:59  mike
 *   Initial revision
 *
 */

#ifndef _IMAGE_H_
# define _IMAGE_H_

/*
 * Include necessary headers...
 */

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <il/ilCdefs.h>


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
  ilCoordSpace	type;		/* Colorspace of image */
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

extern ZOOM	*ZoomAlloc(IMAGE *ip, int xsize, int ysize, int rotated);
extern void	ZoomFill(ZOOM *z, int row, int idepth);
extern void	ZoomFree(ZOOM *z);

#endif /* !_IMAGE_H_ */

/*
 * End of "$Id: image.h,v 1.1 1995/07/04 22:16:59 mike Exp $".
 */
