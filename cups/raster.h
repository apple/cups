/*
 * "$Id: raster.h,v 1.1 1999/03/11 19:59:25 mike Exp $"
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

#ifndef _CUPS_RASTER_H_
#  define _CUPS_RASTER_H_

/*
 * Every non-PostScript printer driver that supports raster images should
 * use the cups/raster image file format.  Since both the PostScript RIP
 * (pstoraster, based on GNU GhostScript 4.03) and Image RIP (imagetoraster,
 * located in the filter directory) use it, using this format saves you a
 * lot of work.  Also, the PostScript RIP passes any printer options that
 * are in a PS file to your driver this way as well...
 */

/*
 * Constants...
 */

#  define CUPS_RASTER_SYNC	0x52615374	/* RaSt */


/*
 * Types...
 */

typedef enum
{
  CUPS_FALSE,
  CUPS_TRUE
} cups_bool_t;

typedef enum
{
  CUPS_JOG_NONE,
  CUPS_JOG_FILE,
  CUPS_JOG_JOB,
  CUPS_JOG_SET
} cups_jog_t;

typedef enum
{
  CUPS_ORIENT_0,
  CUPS_ORIENT_90,
  CUPS_ORIENT_180,
  CUPS_ORIENT_270
} cups_orient_t;

typedef enum
{
  CUPS_CUT_NONE,
  CUPS_CUT_FILE,
  CUPS_CUT_JOB,
  CUPS_CUT_SET,
  CUPS_CUT_PAGE
} cups_cut_t;

typedef enum
{
  CUPS_ADVANCE_NONE,
  CUPS_ADVANCE_FILE,
  CUPS_ADVANCE_JOB,
  CUPS_ADVANCE_SET,
  CUPS_ADVANCE_PAGE
} cups_adv_t;

typedef enum
{
  CUPS_COLOR_CMYK = -4,
  CUPS_COLOR_CMY,
  CUPS_COLOR_GRAY = -1,
  CUPS_COLOR_RGB = 3
} cups_color_t;


/*
 * The page header structure contains the standard PostScript page device
 * dictionary, along with some CUPS-specific parameters that are provided
 * by the RIPs...
 */

typedef struct
{
  /**** Standard Page Device Dictionary Values ****/
  unsigned	AdvanceDistance;	/* AdvanceDistance value in pixels */
  cups_adv_t	AdvanceMedia;		/* AdvanceMedia value (see above) */
  cups_bool_t	Collate;		/* Collated copies value */
  cups_cut_t	CutMedia;		/* CutMedia value (see above) */
  cups_bool_t	Duplex;			/* Duplexed (double-sided) value */
  cups_jog_t	Jog;			/* Jog value (see above) */
  unsigned	Margins[2];		/* Lower-lefthand margins in pixels */
  cups_bool_t	ManualFeed;		/* ManualFeed value */
  char		MediaColor[64];		/* MediaColor string */
  unsigned	MediaPosition;		/* MediaPosition value */
  char		MediaType[64];		/* MediaType string */
  unsigned	MediaWeight;		/* MediaWeight value in grams/m^2 */
  cups_bool_t	MirrorPrint;		/* MirrorPrint value */
  cups_bool_t	NegativePrint;		/* NegativePrint value */
  unsigned	NumCopies;		/* Number of copies to produce */
  cups_orient_t	Orientation;		/* Orientation value (see above) */
  cups_bool_t	OutputFaceUp;		/* OutputFaceUp value */
  char		OutputType[64];		/* OutputType string */
  unsigned	ImagingBoundingBox[4];	/* Pixel region that is painted */
  unsigned	HWResolution[2];	/* Resolution in dots-per-inch */
  cups_bool_t	Separations;		/* Separations value */
  cups_bool_t	TraySwitch;		/* TraySwitch value */
  cups_bool_t	Tumble;			/* Tumble value */
  unsigned	PageSize[2];		/* Width and length of page in pixels */
  cups_color_t	ProcessColorModel;	/* ProcessColorModel value (see above) */

  /**** CUPS Page Device Dictionary Values ****/
  unsigned	
} cups_page_header_t;


#endif /* !_CUPS_RASTER_H_ */

/*
 * End of "$Id: raster.h,v 1.1 1999/03/11 19:59:25 mike Exp $".
 */
