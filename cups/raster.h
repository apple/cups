/*
 * "$Id: raster.h,v 1.4 1999/03/29 22:05:11 mike Exp $"
 *
 *   Raster file definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights for the CUPS Raster source
 *   files are outlined in the GNU Library General Public License, located
 *   in the "pstoraster" directory.  If this file is missing or damaged
 *   please contact Easy Software Products at:
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
 *   This code and any derivative of it may be used and distributed
 *   freely under the terms of the GNU General Public License when
 *   used with GNU Ghostscript or its derivatives.  Use of the code
 *   (or any derivative of it) with software other than GNU
 *   GhostScript (or its derivatives) is governed by the CUPS license
 *   agreement.
 */

#ifndef _CUPS_RASTER_H_
#  define _CUPS_RASTER_H_

/*
 * Every non-PostScript printer driver that supports raster images should
 * use the application/vnd.cups-raster image file format.  Since both the
 * PostScript RIP (pstoraster, based on GNU Ghostscript 4.03) and Image RIP
 * (imagetoraster, located in the filter directory) use it, using this format
 * saves you a lot of work.  Also, the PostScript RIP passes any printer
 * options that are in a PS file to your driver this way as well...
 */

/*
 * Constants...
 */

#  define CUPS_RASTER_SYNC	0x52615374	/* RaSt */
#  define CUPS_RASTER_REVSYNC	0x74536152	/* tSaR */


/*
 * Types...
 */

typedef enum
{
  CUPS_RASTER_READ,			/* Open stream for reading */
  CUPS_RASTER_WRITE			/* Open stream for writing */
} cups_mode_t;

typedef enum
{
  CUPS_FALSE,				/* Logical false */
  CUPS_TRUE				/* Logical true */
} cups_bool_t;

typedef enum
{
  CUPS_JOG_NONE,			/* Never move pages */
  CUPS_JOG_FILE,			/* Move pages after this file */
  CUPS_JOG_JOB,				/* Move pages after this job */
  CUPS_JOG_SET				/* Move pages after this set */
} cups_jog_t;

typedef enum
{
  CUPS_ORIENT_0,			/* Don't rotate the page */
  CUPS_ORIENT_90,			/* Rotate the page counter-clockwise */
  CUPS_ORIENT_180,			/* Turn the page upside down */
  CUPS_ORIENT_270			/* Rotate the page clockwise */
} cups_orient_t;

typedef enum
{
  CUPS_CUT_NONE,			/* Never cut the roll */
  CUPS_CUT_FILE,			/* Cut the roll after this file */
  CUPS_CUT_JOB,				/* Cut the roll after this job */
  CUPS_CUT_SET,				/* Cut the roll after this set */
  CUPS_CUT_PAGE				/* Cut the roll after this page */
} cups_cut_t;

typedef enum
{
  CUPS_ADVANCE_NONE,			/* Never advance the roll */
  CUPS_ADVANCE_FILE,			/* Advance the roll after this file */
  CUPS_ADVANCE_JOB,			/* Advance the roll after this job */
  CUPS_ADVANCE_SET,			/* Advance the roll after this set */
  CUPS_ADVANCE_PAGE			/* Advance the roll after this page */
} cups_adv_t;

typedef enum
{
  CUPS_EDGE_TOP,			/* Leading edge is the top of the page */
  CUPS_EDGE_RIGHT,			/* Leading edge is the right of the page */
  CUPS_EDGE_BOTTOM,			/* Leading edge is the bottom of the page */
  CUPS_EDGE_LEFT			/* Leading edge is the left of the page */
} cups_edge_t;

typedef enum
{
  CUPS_ORDER_CHUNKED,			/* CMYK CMYK CMYK ... */
  CUPS_ORDER_BANDED,			/* CCC MMM YYY KKK ... */
  CUPS_ORDER_PLANAR			/* CCC ... MMM ... YYY ... KKK ... */
} cups_order_t;

typedef enum
{
  CUPS_CSPACE_W,			/* Luminance */
  CUPS_CSPACE_RGB,			/* Red, green, blue */
  CUPS_CSPACE_RGBA,			/* Red, green, blue, alpha */
  CUPS_CSPACE_K,			/* Black */
  CUPS_CSPACE_CMY,			/* Cyan, magenta, yellow */
  CUPS_CSPACE_YMC,			/* Yellow, magenta, cyan */
  CUPS_CSPACE_CMYK,			/* Cyan, magenta, yellow, black */
  CUPS_CSPACE_YMCK,			/* Yellow, magenta, cyan, black */
  CUPS_CSPACE_KCMY,			/* Black, cyan, magenta, yellow */
  CUPS_CSPACE_KCMYcm			/* Black, cyan, magenta, yellow, *
					 * light-cyan, light-magenta     */
} cups_cspace_t;


/*
 * The page header structure contains the standard PostScript page device
 * dictionary, along with some CUPS-specific parameters that are provided
 * by the RIPs...
 */

typedef struct
{
  /**** Standard Page Device Dictionary String Values ****/
  char		MediaClass[64];		/* MediaClass string */
  char		MediaColor[64];		/* MediaColor string */
  char		MediaType[64];		/* MediaType string */
  char		OutputType[64];		/* OutputType string */

  /**** Standard Page Device Dictionary Integer Values ****/
  unsigned	AdvanceDistance;	/* AdvanceDistance value in pixels */
  cups_adv_t	AdvanceMedia;		/* AdvanceMedia value (see above) */
  cups_bool_t	Collate;		/* Collated copies value */
  cups_cut_t	CutMedia;		/* CutMedia value (see above) */
  cups_bool_t	Duplex;			/* Duplexed (double-sided) value */
  unsigned	HWResolution[2];	/* Resolution in dots-per-inch */
  unsigned	ImagingBoundingBox[4];	/* Pixel region that is painted */
  cups_bool_t	InsertSheet;		/* InsertSheet value */
  cups_jog_t	Jog;			/* Jog value (see above) */
  cups_edge_t	LeadingEdge;		/* LeadingEdge value (see above) */
  unsigned	Margins[2];		/* Lower-lefthand margins in pixels */
  cups_bool_t	ManualFeed;		/* ManualFeed value */
  unsigned	MediaPosition;		/* MediaPosition value */
  unsigned	MediaWeight;		/* MediaWeight value in grams/m^2 */
  cups_bool_t	MirrorPrint;		/* MirrorPrint value */
  cups_bool_t	NegativePrint;		/* NegativePrint value */
  unsigned	NumCopies;		/* Number of copies to produce */
  cups_orient_t	Orientation;		/* Orientation value (see above) */
  cups_bool_t	OutputFaceUp;		/* OutputFaceUp value */
  cups_bool_t	Separations;		/* Separations value */
  cups_bool_t	TraySwitch;		/* TraySwitch value */
  cups_bool_t	Tumble;			/* Tumble value */
  unsigned	PageSize[2];		/* Width and length of page in pixels */

  /**** CUPS Page Device Dictionary Values ****/
  unsigned	cupsBitsPerColor;	/* Number of bits for each color */
  unsigned	cupsBitsPerPixel;	/* Number of bits for each pixel */
  unsigned	cupsBytesPerLine;	/* Number of bytes per line */
  cups_order_t	cupsColorOrder;		/* Order of colors */
  cups_cspace_t	cupsColorSpace;		/* True colorspace */
} cups_page_header_t;


/*
 * The raster structure maintains information about a raster data
 * stream...
 */

typedef struct
{
  unsigned	sync;			/* Sync word from start of stream */
  int		fd;			/* File descriptor */
  cups_mode_t	mode;			/* Read/write mode */
} cups_raster_t;


/*
 * Prototypes...
 */

extern void		cupsRasterClose(cups_raster_t *r);
extern cups_raster_t	*cupsRasterOpen(int fd, cups_mode_t mode);
extern unsigned		cupsRasterReadHeader(cups_raster_t *r,
			                     cups_page_header_t *h);
extern unsigned		cupsRasterReadPixels(cups_raster_t *r,
			                     unsigned char *p, unsigned len);
extern unsigned		cupsRasterWriteHeader(cups_raster_t *r,
			                      cups_page_header_t *h);
extern unsigned		cupsRasterWritePixels(cups_raster_t *r,
			                      unsigned char *p, unsigned len);

#endif /* !_CUPS_RASTER_H_ */

/*
 * End of "$Id: raster.h,v 1.4 1999/03/29 22:05:11 mike Exp $".
 */
