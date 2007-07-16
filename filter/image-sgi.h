/*
 * "$Id: image-sgi.h 6649 2007-07-11 21:46:42Z mike $"
 *
 *   SGI image file format library definitions for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1993-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _SGI_H_
#  define _SGI_H_

#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>

#  ifdef __cplusplus
extern "C" {
#  endif


/*
 * Constants...
 */

#  define SGI_MAGIC	474	/* Magic number in image file */

#  define SGI_READ	0	/* Read from an SGI image file */
#  define SGI_WRITE	1	/* Write to an SGI image file */

#  define SGI_COMP_NONE	0	/* No compression */
#  define SGI_COMP_RLE	1	/* Run-length encoding */
#  define SGI_COMP_ARLE	2	/* Agressive run-length encoding */


/*
 * Image structure...
 */

typedef struct
{
  FILE			*file;		/* Image file */
  int			mode,		/* File open mode */
			bpp,		/* Bytes per pixel/channel */
			comp;		/* Compression */
  unsigned short	xsize,		/* Width in pixels */
			ysize,		/* Height in pixels */
			zsize;		/* Number of channels */
  long			firstrow,	/* File offset for first row */
			nextrow,	/* File offset for next row */
			**table,	/* Offset table for compression */
			**length;	/* Length table for compression */
  unsigned short	*arle_row;	/* Advanced RLE compression buffer */
  long			arle_offset,	/* Advanced RLE buffer offset */
			arle_length;	/* Advanced RLE buffer length */
} sgi_t;


/*
 * Prototypes...
 */

extern int	sgiClose(sgi_t *sgip);
extern int	sgiGetRow(sgi_t *sgip, unsigned short *row, int y, int z);
extern sgi_t	*sgiOpen(const char *filename, int mode, int comp, int bpp,
		         int xsize, int ysize, int zsize);
extern sgi_t	*sgiOpenFile(FILE *file, int mode, int comp, int bpp,
		             int xsize, int ysize, int zsize);
extern int	sgiPutRow(sgi_t *sgip, unsigned short *row, int y, int z);

#  ifdef __cplusplus
}
#  endif
#endif /* !_SGI_H_ */

/*
 * End of "$Id: image-sgi.h 6649 2007-07-11 21:46:42Z mike $".
 */
