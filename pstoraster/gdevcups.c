/*
 * "$Id: gdevcups.c,v 1.43.2.1 2001/12/26 16:52:48 mike Exp $"
 *
 *   GNU Ghostscript raster output driver for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 1993-2001 by Easy Software Products.
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
 *   This code and any derivative of it may be used and distributed
 *   freely under the terms of the GNU General Public License when
 *   used with GNU Ghostscript or its derivatives.  Use of the code
 *   (or any derivative of it) with software other than GNU
 *   GhostScript (or its derivatives) is governed by the CUPS license
 *   agreement.
 *
 * Contents:
 *
 *   cups_close()            - Close the output file.
 *   cups_get_matrix()       - Generate the default page matrix.
 *   cups_get_params()       - Get pagedevice parameters.
 *   cups_map_color_rgb()    - Map a color index to an RGB color.
 *   cups_map_rgb_color()    - Map an RGB color to a color index.  We map the
 *                             RGB color to the output colorspace & bits (we
 *                             figure out the format when we output a page).
 *   cups_open()             - Open the output file and initialize things.
 *   cups_print_pages()      - Send one or more pages to the output file.
 *   cups_put_params()       - Set pagedevice parameters.
 *   cups_set_color_info()   - Set the color information structure based on
 *                             the required output.
 *   cups_print_chunked()    - Print a page of chunked pixels.
 *   cups_print_banded()     - Print a page of banded pixels.
 *   cups_print_planar()     - Print a page of planar pixels.
 */

/*
 * Include necessary headers...
 */

#include "std.h"                /* to stop stdlib.h redefining types */
#include "gdevprn.h"
#include "gsparam.h"
#include "gsexit.h"

#include <stdlib.h>
#include <filter/raster.h>
#include <cups/ppd.h>
#include <math.h>

#undef private
#define private


/*
 * Globals...
 */

extern const char	*cupsProfile;


/*
 * Macros...
 */

#define x_dpi		(pdev->HWResolution[0])
#define y_dpi		(pdev->HWResolution[1])
#define cups		((gx_device_cups *)pdev)

/*
 * Macros from <macros.h>; we can't include <macros.h> because it also
 * defines DEBUG, one of our flags to insert various debugging code.
 */

#ifndef max
#  define max(a,b)	((a)<(b) ? (b) : (a))
#endif /* !max */

#ifndef min
#  define min(a,b)	((a)>(b) ? (b) : (a))
#endif /* !min */

#ifndef abs
#  define abs(x)	((x)>=0 ? (x) : -(x))
#endif /* !abs */


/*
 * Procedures
 */

private dev_proc_close_device(cups_close);
private dev_proc_get_initial_matrix(cups_get_matrix);
private int cups_get_params(gx_device *, gs_param_list *);
private dev_proc_map_cmyk_color(cups_map_cmyk_color);
private dev_proc_map_color_rgb(cups_map_color_rgb);
private dev_proc_map_rgb_color(cups_map_rgb_color);
private dev_proc_open_device(cups_open);
private int cups_print_pages(gx_device_printer *, FILE *, int);
private int cups_put_params(gx_device *, gs_param_list *);
private void cups_set_color_info(gx_device *);
private dev_proc_sync_output(cups_sync_output);

/*
 * The device descriptors...
 */

typedef struct gx_device_cups_s
{
  gx_device_common;			/* Standard GhostScript device stuff */
  gx_prn_device_common;			/* Standard printer device stuff */
  int			page;		/* Page number */
  cups_raster_t		*stream;	/* Raster stream */
  ppd_file_t		*ppd;		/* PPD file for this printer */
  cups_page_header_t	header;		/* PostScript page device info */
} gx_device_cups;

private gx_device_procs	cups_procs =
{
   cups_open,
   cups_get_matrix,
   cups_sync_output,
   gdev_prn_output_page,
   cups_close,
   cups_map_rgb_color,
   cups_map_color_rgb,
   NULL,	/* fill_rectangle */
   NULL,	/* tile_rectangle */
   NULL,	/* copy_mono */
   NULL,	/* copy_color */
   NULL,	/* draw_line */
   gx_default_get_bits,
   cups_get_params,
   cups_put_params,
   NULL,	/* map_cmyk_color */
   NULL,	/* get_xfont_procs */
   NULL,	/* get_xfont_device */
   NULL,	/* map_rgb_alpha_color */
   gx_page_device_get_page_device,
   NULL,	/* get_alpha_bits */
   NULL,	/* copy_alpha */
   NULL,	/* get_band */
   NULL,	/* copy_rop */
   NULL,	/* fill_path */
   NULL,	/* stroke_path */
   NULL,	/* fill_mask */
   NULL,	/* fill_trapezoid */
   NULL,	/* fill_parallelogram */
   NULL,	/* fill_triangle */
   NULL,	/* draw_thin_line */
   NULL,	/* begin_image */
   NULL,	/* image_data */
   NULL,	/* end_image */
   NULL,	/* strip_tile_rectangle */
   NULL		/* strip_copy_rop */
};

gx_device_cups	gs_cups_device =
{
  prn_device_body_copies(gx_device_cups, cups_procs, "cups", 85, 110, 100, 100,
                         0, 0, 0, 0, 0, 1, 0, 0, 0, 0, cups_print_pages),
  0,				/* page */
  NULL,				/* stream */
  NULL,				/* ppd */
  {				/* header */
    "",				/* MediaClass */
    "",				/* MediaColor */
    "",				/* MediaType */
    "",				/* OutputType */
    0,				/* AdvanceDistance */
    CUPS_ADVANCE_NONE,		/* AdvanceMedia */
    CUPS_FALSE,			/* Collate */
    CUPS_CUT_NONE,		/* CutMedia */
    CUPS_FALSE,			/* Duplex */
    { 100, 100 },		/* HWResolution */
    { 0, 0, 612, 792 },		/* ImagingBoundingBox */
    CUPS_FALSE,			/* InsertSheet */
    CUPS_JOG_NONE,		/* Jog */
    CUPS_EDGE_TOP,		/* LeadingEdge */
    { 0, 0 },			/* Margins */
    CUPS_FALSE,			/* ManualFeed */
    0,				/* MediaPosition */
    0,				/* MediaWeight */
    CUPS_FALSE,			/* MirrorPrint */
    CUPS_FALSE,			/* NegativePrint */
    1,				/* NumCopies */
    CUPS_ORIENT_0,		/* Orientation */
    CUPS_FALSE,			/* OutputFaceUp */
    { 612, 792 },		/* PageSize */
    CUPS_FALSE,			/* Separations */
    CUPS_FALSE,			/* TraySwitch */
    CUPS_FALSE,			/* Tumble */
    850,			/* cupsWidth */
    1100,			/* cupsHeight */
    0,				/* cupsMediaType */
    1,				/* cupsBitsPerColor */
    1,				/* cupsBitsPerPixel */
    107,			/* cupsBytesPerLine */
    CUPS_ORDER_CHUNKED,		/* cupsColorOrder */
    CUPS_CSPACE_K,		/* cupsColorSpace */
    0,				/* cupsCompression */
    0,				/* cupsRowCount */
    0,				/* cupsRowFeed */
    0				/* cupsRowStep */
  }
};

/*
 * Color lookup tables...
 */

static gx_color_value	lut_color_rgb[256];
static unsigned char	lut_rgb_color[gx_max_color_value + 1];
static int		cupsHaveProfile = 0;
static int		cupsMatrix[3][3][gx_max_color_value + 1];
static int		cupsDensity[gx_max_color_value + 1];
static unsigned char	rev_lower1[16] =
			{
			  0x00, 0x08, 0x04, 0x0c, 0x02, 0x0a, 0x06, 0x0e,
			  0x01, 0x09, 0x05, 0x0d, 0x03, 0x0b, 0x07, 0x0f
			},
			rev_upper1[16] =
			{
			  0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
			  0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0
			},
			rev_lower2[16] = /* 2-bit colors */
			{
			  0x00, 0x04, 0x08, 0x0c, 0x01, 0x05, 0x09, 0x0d,
			  0x02, 0x06, 0x0a, 0x0e, 0x03, 0x07, 0x0b, 0x0f
			},
			rev_upper2[16] = /* 2-bit colors */
			{
			  0x00, 0x40, 0x80, 0xc0, 0x10, 0x50, 0x90, 0xd0,
			  0x20, 0x60, 0xa0, 0xe0, 0x30, 0x70, 0xb0, 0xf0
			};


/*
 * Local functions...
 */

static void	cups_print_chunked(gx_device_printer *, unsigned char *,
		                   unsigned char *, int);
static void	cups_print_banded(gx_device_printer *, unsigned char *,
		                  unsigned char *, int);
static void	cups_print_planar(gx_device_printer *, unsigned char *,
		                  unsigned char *, int);

/*static void	cups_set_margins(gx_device *);*/


/*
 * 'cups_close()' - Close the output file.
 */

private int
cups_close(gx_device *pdev)	/* I - Device info */
{
#ifdef DEBUG
  fprintf(stderr, "DEBUG: cups_close(%p)\n", pdev);
#endif /* DEBUG */

  if (cups->stream != NULL)
  {
    cupsRasterClose(cups->stream);
    cups->stream = NULL;
  }

#if 0 /* Can't do this here because put_params() might close the device */
  if (cups->ppd != NULL)
  {
    ppdClose(cups->ppd);
    cups->ppd = NULL;
  }
#endif /* 0 */

  return (gdev_prn_close(pdev));
}


/*
 * 'cups_get_matrix()' - Generate the default page matrix.
 */

private void
cups_get_matrix(gx_device *pdev,	/* I - Device info */
                gs_matrix *pmat)	/* O - Physical transform matrix */
{
#ifdef DEBUG
  fprintf(stderr, "DEBUG: cups_get_matrix(%p, %p)\n", pdev, pmat);
#endif /* DEBUG */

 /*
  * Set the raster width and height...
  */

  cups->header.cupsWidth  = cups->width;
  cups->header.cupsHeight = cups->height;

 /*
  * Set the transform matrix...
  */

  fprintf(stderr, "DEBUG: cups->header.Duplex = %d\n", cups->header.Duplex);
  fprintf(stderr, "DEBUG: cups->page = %d\n", cups->page);
  if (cups->ppd)
  {
    fprintf(stderr, "DEBUG: cups->ppd = %p\n", cups->ppd);
    fprintf(stderr, "DEBUG: cups->ppd->flip_duplex = %d\n", cups->ppd->flip_duplex);
  }

  if (cups->header.Duplex && !cups->header.Tumble &&
      cups->ppd && cups->ppd->flip_duplex && !(cups->page & 1))
  {
    pmat->xx = (float)cups->header.HWResolution[0] / 72.0;
    pmat->xy = 0.0;
    pmat->yx = 0.0;
    pmat->yy = (float)cups->header.HWResolution[1] / 72.0;
    pmat->tx = -(float)cups->header.HWResolution[0] * pdev->HWMargins[2] / 72.0;
    pmat->ty = -(float)cups->header.HWResolution[1] * pdev->HWMargins[3] / 72.0;
  }
  else
  {
    pmat->xx = (float)cups->header.HWResolution[0] / 72.0;
    pmat->xy = 0.0;
    pmat->yx = 0.0;
    pmat->yy = -(float)cups->header.HWResolution[1] / 72.0;
    pmat->tx = -(float)cups->header.HWResolution[0] * pdev->HWMargins[0] / 72.0;
    pmat->ty = (float)cups->header.HWResolution[1] *
               ((float)cups->header.PageSize[1] - pdev->HWMargins[3]) / 72.0;
  }

  fprintf(stderr, "DEBUG: width = %d, height = %d\n", cups->width,
          cups->height);
  fprintf(stderr, "DEBUG: PageSize = [ %d %d ], HWResolution = [ %d %d ]\n",
          cups->header.PageSize[0], cups->header.PageSize[1],
          cups->header.HWResolution[0], cups->header.HWResolution[1]);
  fprintf(stderr, "DEBUG: HWMargins = [ %.3f %.3f %.3f %.3f ]\n",
	  pdev->HWMargins[0], pdev->HWMargins[1], pdev->HWMargins[2],
	  pdev->HWMargins[3]);
  fprintf(stderr, "DEBUG: matrix = [ %.3f %.3f %.3f %.3f %.3f %.3f ]\n",
          pmat->xx, pmat->xy, pmat->yx, pmat->yy, pmat->tx, pmat->ty);
}


/*
 * 'cups_get_params()' - Get pagedevice parameters.
 */

private int				/* O - Error status */
cups_get_params(gx_device     *pdev,	/* I - Device info */
                gs_param_list *plist)	/* I - Parameter list */
{
  int			code;		/* Return code */
  gs_param_string	s;		/* Temporary string value */
  bool			b;		/* Temporary boolean value */


#ifdef DEBUG
  fprintf(stderr, "DEBUG: cups_get_params(%p, %p)\n", pdev, plist);
#endif /* DEBUG */

 /*
  * First process the "standard" page device parameters...
  */

#ifdef DEBUG
  fputs("DEBUG: before gdev_prn_get_params()\n", stderr);
#endif /* DEBUG */

  if ((code = gdev_prn_get_params(pdev, plist)) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: after gdev_prn_get_params()\n", stderr);
#endif /* DEBUG */

 /*
  * Then write the CUPS parameters...
  */

#ifdef DEBUG
  fputs("DEBUG: MediaClass\n", stderr);
#endif /* DEBUG */

  param_string_from_string(s, cups->header.MediaClass);
  if ((code = param_write_string(plist, "MediaClass", &s)) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: AdvanceDistance\n", stderr);
#endif /* DEBUG */

  if ((code = param_write_int(plist, "AdvanceDistance",
                              (int *)&(cups->header.AdvanceDistance))) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: AdvanceDistance\n", stderr);
#endif /* DEBUG */

  if ((code = param_write_int(plist, "AdvanceMedia",
                              (int *)&(cups->header.AdvanceMedia))) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: Collate\n", stderr);
#endif /* DEBUG */

  b = cups->header.Collate;
  if ((code = param_write_bool(plist, "Collate", &b)) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: CutMedia\n", stderr);
#endif /* DEBUG */

  if ((code = param_write_int(plist, "CutMedia",
                              (int *)&(cups->header.CutMedia))) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: InsertSheet\n", stderr);
#endif /* DEBUG */

  b = cups->header.InsertSheet;
  if ((code = param_write_bool(plist, "InsertSheet", &b)) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: Jog\n", stderr);
#endif /* DEBUG */

  if ((code = param_write_int(plist, "Jog",
                              (int *)&(cups->header.Jog))) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: LeadingEdge\n", stderr);
#endif /* DEBUG */

  if ((code = param_write_int(plist, "LeadingEdge",
                              (int *)&(cups->header.LeadingEdge))) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: ManualFeed\n", stderr);
#endif /* DEBUG */

  b = cups->header.ManualFeed;
  if ((code = param_write_bool(plist, "ManualFeed", &b)) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: MediaPosition\n", stderr);
#endif /* DEBUG */

  if ((code = param_write_int(plist, "MediaPosition",
                              (int *)&(cups->header.MediaPosition))) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: MirrorPrint\n", stderr);
#endif /* DEBUG */

  b = cups->header.MirrorPrint;
  if ((code = param_write_bool(plist, "MirrorPrint", &b)) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: NegativePrint\n", stderr);
#endif /* DEBUG */

  b = cups->header.NegativePrint;
  if ((code = param_write_bool(plist, "NegativePrint", &b)) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: OutputFaceUp\n", stderr);
#endif /* DEBUG */

  b = cups->header.OutputFaceUp;
  if ((code = param_write_bool(plist, "OutputFaceUp", &b)) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: Separations\n", stderr);
#endif /* DEBUG */

  b = cups->header.Separations;
  if ((code = param_write_bool(plist, "Separations", &b)) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: TraySwitch\n", stderr);
#endif /* DEBUG */

  b = cups->header.TraySwitch;
  if ((code = param_write_bool(plist, "TraySwitch", &b)) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: Tumble\n", stderr);
#endif /* DEBUG */

  b = cups->header.Tumble;
  if ((code = param_write_bool(plist, "Tumble", &b)) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: cupsWidth\n", stderr);
#endif /* DEBUG */

  if ((code = param_write_int(plist, "cupsWidth",
                              (int *)&(cups->header.cupsWidth))) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: cupsHeight\n", stderr);
#endif /* DEBUG */

  if ((code = param_write_int(plist, "cupsHeight",
                              (int *)&(cups->header.cupsHeight))) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: cupsMediaType\n", stderr);
#endif /* DEBUG */

  if ((code = param_write_int(plist, "cupsMediaType",
                              (int *)&(cups->header.cupsMediaType))) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: cupsBitsPerColor\n", stderr);
#endif /* DEBUG */

  if ((code = param_write_int(plist, "cupsBitsPerColor",
                              (int *)&(cups->header.cupsBitsPerColor))) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: cupsBitsPerPixel\n", stderr);
#endif /* DEBUG */

  if ((code = param_write_int(plist, "cupsBitsPerPixel",
                              (int *)&(cups->header.cupsBitsPerPixel))) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: cupsBytesPerLine\n", stderr);
#endif /* DEBUG */

  if ((code = param_write_int(plist, "cupsBytesPerLine",
                              (int *)&(cups->header.cupsBytesPerLine))) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: cupsColorOrder\n", stderr);
#endif /* DEBUG */

  if ((code = param_write_int(plist, "cupsColorOrder",
                              (int *)&(cups->header.cupsColorOrder))) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: cupsColorSpace\n", stderr);
#endif /* DEBUG */

  if ((code = param_write_int(plist, "cupsColorSpace",
                              (int *)&(cups->header.cupsColorSpace))) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: cupsCompression\n", stderr);
#endif /* DEBUG */

  if ((code = param_write_int(plist, "cupsCompression",
                              (int *)&(cups->header.cupsCompression))) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: cupsRowCount\n", stderr);
#endif /* DEBUG */

  if ((code = param_write_int(plist, "cupsRowCount",
                              (int *)&(cups->header.cupsRowCount))) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: cupsRowFeed\n", stderr);
#endif /* DEBUG */

  if ((code = param_write_int(plist, "cupsRowFeed",
                              (int *)&(cups->header.cupsRowFeed))) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: cupsRowStep\n", stderr);
#endif /* DEBUG */

  if ((code = param_write_int(plist, "cupsRowStep",
                              (int *)&(cups->header.cupsRowStep))) < 0)
    return (code);

#ifdef DEBUG
  fputs("DEBUG: Leaving cups_get_params()\n", stderr);
#endif /* DEBUG */

  return (0);
}


/*
 * 'cups_map_cmyk_color()' - Map a CMYK color to a color index.
 *
 * This function is only called when a 4 or 6 color colorspace is
 * selected for output.  CMYK colors are *not* corrected but *are*
 * density adjusted.
 */

private gx_color_index				/* O - Color index */
cups_map_cmyk_color(gx_device      *pdev,	/* I - Device info */
                    gx_color_value c,		/* I - Cyan value */
                    gx_color_value m,		/* I - Magenta value */
                    gx_color_value y,		/* I - Yellow value */
		    gx_color_value k)		/* I - Black value */
{
  gx_color_index	i;			/* Temporary index */
  gx_color_value	ic, im, iy, ik;		/* Integral CMYK values */


#ifdef DEBUG
  fprintf(stderr, "DEBUG: cups_map_cmyk_color(%p, %d, %d, %d, %d)\n", pdev,
          c, m, y, k);
#endif /* DEBUG */

 /*
  * Setup the color info data as needed...
  */

  if (pdev->color_info.num_components == 0)
    cups_set_color_info(pdev);

 /*
  * Density correct...
  */

  c  = cupsDensity[c];
  m  = cupsDensity[m];
  y  = cupsDensity[y];
  k  = cupsDensity[k];

  ic = lut_rgb_color[c];
  im = lut_rgb_color[m];
  iy = lut_rgb_color[y];
  ik = lut_rgb_color[k];

 /*
  * Convert the CMYK color to a color index...
  */

  switch (cups->header.cupsColorSpace)
  {
    default :
        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((((ic << 1) | im) << 1) | iy) << 1) | ik;
              break;
          case 2 :
              i = (((((ic << 2) | im) << 2) | iy) << 2) | ik;
              break;
          case 4 :
              i = (((((ic << 4) | im) << 4) | iy) << 4) | ik;
              break;
          case 8 :
              i = (((((ic << 8) | im) << 8) | iy) << 8) | ik;
              break;
        }
        break;

    case CUPS_CSPACE_YMCK :
    case CUPS_CSPACE_GMCK :
    case CUPS_CSPACE_GMCS :
        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((((iy << 1) | im) << 1) | ic) << 1) | ik;
              break;
          case 2 :
              i = (((((iy << 2) | im) << 2) | ic) << 2) | ik;
              break;
          case 4 :
              i = (((((iy << 4) | im) << 4) | ic) << 4) | ik;
              break;
          case 8 :
              i = (((((iy << 8) | im) << 8) | ic) << 8) | ik;
              break;
        }
        break;

    case CUPS_CSPACE_KCMYcm :
        if (cups->header.cupsBitsPerColor == 1)
	{
	  if (ik)
	    i = 32;
	  else
	    i = 0;

	  if (ic && im)
	    i |= 17;
	  else if (ic && iy)
	    i |= 6;
	  else if (im && iy)
	    i |= 12;
	  else if (ic)
	    i |= 16;
	  else if (im)
	    i |= 8;
	  else if (iy)
	    i |= 4;
	  break;
	}

    case CUPS_CSPACE_KCMY :
        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((((ik << 1) | ic) << 1) | im) << 1) | iy;
              break;
          case 2 :
              i = (((((ik << 2) | ic) << 2) | im) << 2) | iy;
              break;
          case 4 :
              i = (((((ik << 4) | ic) << 4) | im) << 4) | iy;
              break;
          case 8 :
              i = (((((ik << 8) | ic) << 8) | im) << 8) | iy;
              break;
        }
        break;
  }

  if (gs_log_errors > 1)
    fprintf(stderr, "DEBUG: CMYK (%d,%d,%d,%d) -> CMYK %8x (%d,%d,%d,%d)\n",
	    c, m, y, k, i, ic, im, iy, ik);

  return (i);
}
    

/*
 * 'cups_map_color_rgb()' - Map a color index to an RGB color.
 */

private int
cups_map_color_rgb(gx_device      *pdev,	/* I - Device info */
                   gx_color_index color,	/* I - Color index */
		   gx_color_value prgb[3])	/* O - RGB values */
{
  unsigned char		c0, c1, c2, c3;		/* Color index components */
  gx_color_value	k, divk;		/* Black & divisor */


#ifdef DEBUG
  fprintf(stderr, "DEBUG: cups_map_color_rgb(%p, %d, %8x)\n", pdev,
          color, prgb);
#endif /* DEBUG */

 /*
  * Setup the color info data as needed...
  */

  if (pdev->color_info.num_components == 0)
    cups_set_color_info(pdev);

#ifdef DEBUG
  fprintf(stderr, "DEBUG: COLOR %8x = ", color);
#endif /* DEBUG */

 /*
  * Extract the color components from the color index...
  */

  switch (cups->header.cupsBitsPerColor)
  {
    default :
        c3 = color & 1;
        color >>= 1;
        c2 = color & 1;
        color >>= 1;
        c1 = color & 1;
        color >>= 1;
        c0 = color;
        break;
    case 2 :
        c3 = color & 3;
        color >>= 2;
        c2 = color & 3;
        color >>= 2;
        c1 = color & 3;
        color >>= 2;
        c0 = color;
        break;
    case 4 :
        c3 = color & 15;
        color >>= 4;
        c2 = color & 15;
        color >>= 4;
        c1 = color & 15;
        color >>= 4;
        c0 = color;
        break;
    case 8 :
        c3 = color & 255;
        color >>= 8;
        c2 = color & 255;
        color >>= 8;
        c1 = color & 255;
        color >>= 8;
        c0 = color;
        break;
  }

 /*
  * Convert the color components to RGB...
  */

  switch (cups->header.cupsColorSpace)
  {
    case CUPS_CSPACE_K :
    case CUPS_CSPACE_WHITE :
    case CUPS_CSPACE_GOLD :
    case CUPS_CSPACE_SILVER :
        prgb[0] =
        prgb[1] =
        prgb[2] = lut_color_rgb[c3];
        break;

    case CUPS_CSPACE_W :
        prgb[0] =
        prgb[1] =
        prgb[2] = lut_color_rgb[c3];
        break;

    case CUPS_CSPACE_RGB :
        prgb[0] = lut_color_rgb[c1];
        prgb[1] = lut_color_rgb[c2];
        prgb[2] = lut_color_rgb[c3];
        break;

    case CUPS_CSPACE_RGBA :
        prgb[0] = lut_color_rgb[c0];
        prgb[1] = lut_color_rgb[c1];
        prgb[2] = lut_color_rgb[c2];
        break;

    case CUPS_CSPACE_CMY :
        prgb[0] = lut_color_rgb[c1];
        prgb[1] = lut_color_rgb[c2];
        prgb[2] = lut_color_rgb[c3];
        break;

    case CUPS_CSPACE_YMC :
        prgb[0] = lut_color_rgb[c3];
        prgb[1] = lut_color_rgb[c2];
        prgb[2] = lut_color_rgb[c1];
        break;

    case CUPS_CSPACE_KCMY :
    case CUPS_CSPACE_KCMYcm :
        k    = lut_color_rgb[c0];
        divk = gx_max_color_value - k;
        if (divk == 0)
        {
          prgb[0] = 0;
          prgb[1] = 0;
          prgb[2] = 0;
        }
        else
        {
          prgb[0] = gx_max_color_value + divk -
                    gx_max_color_value * c1 / divk;
          prgb[1] = gx_max_color_value + divk -
                    gx_max_color_value * c2 / divk;
          prgb[2] = gx_max_color_value + divk -
                    gx_max_color_value * c3 / divk;
        }
        break;

    case CUPS_CSPACE_CMYK :
        k    = lut_color_rgb[c3];
        divk = gx_max_color_value - k;
        if (divk == 0)
        {
          prgb[0] = 0;
          prgb[1] = 0;
          prgb[2] = 0;
        }
        else
        {
          prgb[0] = gx_max_color_value + divk -
                    gx_max_color_value * c0 / divk;
          prgb[1] = gx_max_color_value + divk -
                    gx_max_color_value * c1 / divk;
          prgb[2] = gx_max_color_value + divk -
                    gx_max_color_value * c2 / divk;
        }
        break;

    case CUPS_CSPACE_YMCK :
    case CUPS_CSPACE_GMCK :
    case CUPS_CSPACE_GMCS :
        k    = lut_color_rgb[c3];
        divk = gx_max_color_value - k;
        if (divk == 0)
        {
          prgb[0] = 0;
          prgb[1] = 0;
          prgb[2] = 0;
        }
        else
        {
          prgb[0] = gx_max_color_value + divk -
                    gx_max_color_value * c2 / divk;
          prgb[1] = gx_max_color_value + divk -
                    gx_max_color_value * c1 / divk;
          prgb[2] = gx_max_color_value + divk -
                    gx_max_color_value * c0 / divk;
        }
        break;
  }

#ifdef DEBUG
  fprintf(stderr, "%d,%d,%d\n", prgb[0], prgb[1], prgb[2]);
#endif /* DEBUG */

  return (0);
}


/*
 * 'cups_map_rgb_color()' - Map an RGB color to a color index.  We map the
 *                          RGB color to the output colorspace & bits (we
 *                          figure out the format when we output a page).
 */

private gx_color_index				/* O - Color index */
cups_map_rgb_color(gx_device      *pdev,	/* I - Device info */
                   gx_color_value r,		/* I - Red value */
                   gx_color_value g,		/* I - Green value */
                   gx_color_value b)		/* I - Blue value */
{
  gx_color_index	i;			/* Temporary index */
  gx_color_value	ic, im, iy, ik;		/* Integral CMYK values */
  gx_color_value	mk;			/* Maximum K value */
  int			tc, tm, ty;		/* Temporary color values */


#ifdef DEBUG
  fprintf(stderr, "DEBUG: cups_map_rgb_color(%p, %d, %d, %d)\n", pdev, r, g, b);
#endif /* DEBUG */

 /*
  * Setup the color info data as needed...
  */

  if (pdev->color_info.num_components == 0)
    cups_set_color_info(pdev);

 /*
  * Do color correction as needed...
  */

  if (cupsHaveProfile)
  {
   /*
    * Compute CMYK values...
    */

    ic = gx_max_color_value - r;
    im = gx_max_color_value - g;
    iy = gx_max_color_value - b;
    ik = min(ic, min(im, iy));

    if ((mk = max(ic, max(im, iy))) > ik)
      ik = (int)((float)ik * (float)ik * (float)ik / ((float)mk * (float)mk));

    ic -= ik;
    im -= ik;
    iy -= ik;

   /*
    * Color correct CMY...
    */

    tc = cupsMatrix[0][0][ic] +
         cupsMatrix[0][1][im] +
	 cupsMatrix[0][2][iy] +
	 ik;
    tm = cupsMatrix[1][0][ic] +
         cupsMatrix[1][1][im] +
	 cupsMatrix[1][2][iy] +
	 ik;
    ty = cupsMatrix[2][0][ic] +
         cupsMatrix[2][1][im] +
	 cupsMatrix[2][2][iy] +
	 ik;

   /*
    * Density correct combined CMYK...
    */

    if (tc < 0)
      r = gx_max_color_value;
    else if (tc > gx_max_color_value)
      r = gx_max_color_value - cupsDensity[gx_max_color_value];
    else
      r = gx_max_color_value - cupsDensity[tc];

    if (tm < 0)
      g = gx_max_color_value;
    else if (tm > gx_max_color_value)
      g = gx_max_color_value - cupsDensity[gx_max_color_value];
    else
      g = gx_max_color_value - cupsDensity[tm];

    if (ty < 0)
      b = gx_max_color_value;
    else if (ty > gx_max_color_value)
      b = gx_max_color_value - cupsDensity[gx_max_color_value];
    else
      b = gx_max_color_value - cupsDensity[ty];
  }
    
 /*
  * Convert the RGB color to a color index...
  */

  switch (cups->header.cupsColorSpace)
  {
    case CUPS_CSPACE_W :
        i = lut_rgb_color[(r * 31 + g * 61 + b * 8) / 100];
        break;

    case CUPS_CSPACE_RGB :
        ic = lut_rgb_color[r];
        im = lut_rgb_color[g];
        iy = lut_rgb_color[b];

        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((ic << 1) | im) << 1) | iy;
              break;
          case 2 :
              i = (((ic << 2) | im) << 2) | iy;
              break;
          case 4 :
              i = (((ic << 4) | im) << 4) | iy;
              break;
          case 8 :
              i = (((ic << 8) | im) << 8) | iy;
              break;
        }
        break;

    case CUPS_CSPACE_RGBA :
        ic = lut_rgb_color[r];
        im = lut_rgb_color[g];
        iy = lut_rgb_color[b];

        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((((ic << 1) | im) << 1) | iy) << 1) | 0x01;
              break;
          case 2 :
              i = (((((ic << 2) | im) << 2) | iy) << 2) | 0x03;
              break;
          case 4 :
              i = (((((ic << 4) | im) << 4) | iy) << 4) | 0x0f;
              break;
          case 8 :
              i = (((((ic << 8) | im) << 8) | iy) << 8) | 0xff;
              break;
        }
        break;

    default :
        i = lut_rgb_color[gx_max_color_value - (r * 31 + g * 61 + b * 8) / 100];
        break;

    case CUPS_CSPACE_CMY :
        ic = lut_rgb_color[gx_max_color_value - r];
        im = lut_rgb_color[gx_max_color_value - g];
        iy = lut_rgb_color[gx_max_color_value - b];

        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((ic << 1) | im) << 1) | iy;
              break;
          case 2 :
              i = (((ic << 2) | im) << 2) | iy;
              break;
          case 4 :
              i = (((ic << 4) | im) << 4) | iy;
              break;
          case 8 :
              i = (((ic << 8) | im) << 8) | iy;
              break;
        }
        break;

    case CUPS_CSPACE_YMC :
        ic = lut_rgb_color[gx_max_color_value - r];
        im = lut_rgb_color[gx_max_color_value - g];
        iy = lut_rgb_color[gx_max_color_value - b];

        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((iy << 1) | im) << 1) | ic;
              break;
          case 2 :
              i = (((iy << 2) | im) << 2) | ic;
              break;
          case 4 :
              i = (((iy << 4) | im) << 4) | ic;
              break;
          case 8 :
              i = (((iy << 8) | im) << 8) | ic;
              break;
        }
        break;

    case CUPS_CSPACE_CMYK :
	ic = gx_max_color_value - r;
	im = gx_max_color_value - g;
	iy = gx_max_color_value - b;
        ik = min(ic, min(im, iy));

	if ((mk = max(ic, max(im, iy))) > ik)
	  ik = (int)((float)ik * (float)ik * (float)ik /
	             ((float)mk * (float)mk));

        ic = lut_rgb_color[ic - ik];
        im = lut_rgb_color[im - ik];
        iy = lut_rgb_color[iy - ik];
        ik = lut_rgb_color[ik];

        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((((ic << 1) | im) << 1) | iy) << 1) | ik;
              break;
          case 2 :
              i = (((((ic << 2) | im) << 2) | iy) << 2) | ik;
              break;
          case 4 :
              i = (((((ic << 4) | im) << 4) | iy) << 4) | ik;
              break;
          case 8 :
              i = (((((ic << 8) | im) << 8) | iy) << 8) | ik;
              break;
        }

        if (gs_log_errors > 1)
	  fprintf(stderr, "DEBUG: CMY (%d,%d,%d) -> CMYK %8x (%d,%d,%d,%d)\n",
	          r, g, b, i, ic, im, iy, ik);
        break;

    case CUPS_CSPACE_YMCK :
    case CUPS_CSPACE_GMCK :
    case CUPS_CSPACE_GMCS :
	ic = gx_max_color_value - r;
	im = gx_max_color_value - g;
	iy = gx_max_color_value - b;
        ik = min(ic, min(im, iy));

	if ((mk = max(ic, max(im, iy))) > ik)
	  ik = (int)((float)ik * (float)ik * (float)ik /
	             ((float)mk * (float)mk));

        ic = lut_rgb_color[ic - ik];
        im = lut_rgb_color[im - ik];
        iy = lut_rgb_color[iy - ik];
        ik = lut_rgb_color[ik];

        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((((iy << 1) | im) << 1) | ic) << 1) | ik;
              break;
          case 2 :
              i = (((((iy << 2) | im) << 2) | ic) << 2) | ik;
              break;
          case 4 :
              i = (((((iy << 4) | im) << 4) | ic) << 4) | ik;
              break;
          case 8 :
              i = (((((iy << 8) | im) << 8) | ic) << 8) | ik;
              break;
        }
        break;

    case CUPS_CSPACE_KCMYcm :
        if (cups->header.cupsBitsPerColor == 1)
	{
	  ic = gx_max_color_value - r;
	  im = gx_max_color_value - g;
	  iy = gx_max_color_value - b;
          ik = min(ic, min(im, iy));

	  if ((mk = max(ic, max(im, iy))) > ik)
	    ik = (int)((float)ik * (float)ik * (float)ik /
	               ((float)mk * (float)mk));

          ic = lut_rgb_color[ic - ik];
          im = lut_rgb_color[im - ik];
          iy = lut_rgb_color[iy - ik];
          ik = lut_rgb_color[ik];
	  if (ik)
	    i = 32;
	  else if (ic && im)
	    i = 17;
	  else if (ic && iy)
	    i = 6;
	  else if (im && iy)
	    i = 12;
	  else if (ic)
	    i = 16;
	  else if (im)
	    i = 8;
	  else if (iy)
	    i = 4;
	  else
	    i = 0;
	  break;
	}

    case CUPS_CSPACE_KCMY :
	ic = gx_max_color_value - r;
	im = gx_max_color_value - g;
	iy = gx_max_color_value - b;
        ik = min(ic, min(im, iy));

	if ((mk = max(ic, max(im, iy))) > ik)
	  ik = (int)((float)ik * (float)ik * (float)ik /
	             ((float)mk * (float)mk));

        ic = lut_rgb_color[ic - ik];
        im = lut_rgb_color[im - ik];
        iy = lut_rgb_color[iy - ik];
        ik = lut_rgb_color[ik];

        switch (cups->header.cupsBitsPerColor)
        {
          default :
              i = (((((ik << 1) | ic) << 1) | im) << 1) | iy;
              break;
          case 2 :
              i = (((((ik << 2) | ic) << 2) | im) << 2) | iy;
              break;
          case 4 :
              i = (((((ik << 4) | ic) << 4) | im) << 4) | iy;
              break;
          case 8 :
              i = (((((ik << 8) | ic) << 8) | im) << 8) | iy;
              break;
        }
        break;
  }

#ifdef DEBUG
  fprintf(stderr, "DEBUG: RGB %d,%d,%d = %8x\n", r, g, b, i);
#endif /* DEBUG */

  return (i);
}
    

/*
 * 'cups_open()' - Open the output file and initialize things.
 */

private int			/* O - Error status */
cups_open(gx_device *pdev)	/* I - Device info */
{
  int	code;			/* Return status */


#ifdef DEBUG
  fprintf(stderr, "DEBUG: cups_open(%p)\n", pdev);
#endif /* DEBUG */

  if (cups->page == 0)
  {
    fputs("INFO: Processing page 1...\n", stderr);
    cups->page = 1;
  }

  if (pdev->color_info.num_components == 0)
    cups_set_color_info(pdev);

  if ((code = gdev_prn_open(pdev)) != 0)
    return (code);

  if (cups->ppd == NULL)
    cups->ppd = ppdOpenFile(getenv("PPD"));

  return (0);
}


/*
 * 'cups_print_pages()' - Send one or more pages to the output file.
 */

private int					/* O - 0 if everything is OK */
cups_print_pages(gx_device_printer *pdev,	/* I - Device info */
                 FILE              *fp,		/* I - Output file */
		 int               num_copies)	/* I - Number of copies */
{
  int		copy;		/* Copy number */
  int		srcbytes;	/* Byte width of scanline */
  unsigned char	*src,		/* Scanline data */
		*dst;		/* Bitmap data */


  (void)fp; /* reference unused file pointer to prevent compiler warning */

#ifdef DEBUG
  fprintf(stderr, "DEBUG: cups_print_pages(%p, %p, %d)\n", pdev, fp,
          num_copies);
#endif /* DEBUG */

 /*
  * Figure out the number of bytes per line...
  */

  switch (cups->header.cupsColorOrder)
  {
    case CUPS_ORDER_CHUNKED :
        cups->header.cupsBytesPerLine = (cups->header.cupsBitsPerPixel *
	                                 cups->header.cupsWidth + 7) / 8;
        break;

    case CUPS_ORDER_BANDED :
        if (cups->header.cupsColorSpace == CUPS_CSPACE_KCMYcm &&
	    cups->header.cupsBitsPerColor == 1)
          cups->header.cupsBytesPerLine = (cups->header.cupsBitsPerColor *
                                           cups->header.cupsWidth + 7) / 8 * 6;
        else
          cups->header.cupsBytesPerLine = (cups->header.cupsBitsPerColor *
                                           cups->header.cupsWidth + 7) / 8 *
				          cups->color_info.num_components;
        break;

    case CUPS_ORDER_PLANAR :
        cups->header.cupsBytesPerLine = (cups->header.cupsBitsPerColor *
	                                 cups->header.cupsWidth + 7) / 8;
        break;
  }

 /*
  * Compute the width of a scanline and allocate input/output buffers...
  */

  srcbytes = gdev_prn_raster(pdev);

#ifdef DEBUG
  fprintf(stderr, "DEBUG: cupsBitsPerPixel = %d, cupsWidth = %d, cupsBytesPerLine = %d, srcbytes = %d\n",
          cups->header.cupsBitsPerPixel, cups->header.cupsWidth,
	  cups->header.cupsBytesPerLine, srcbytes);
#endif /* DEBUG */

  src = (unsigned char *)gs_malloc(srcbytes, 1, "cups_print_pages");

  if (src == NULL)	/* can't allocate input buffer */
    return_error(gs_error_VMerror);

 /*
  * Need an output buffer, too...
  */

  dst = (unsigned char *)gs_malloc(cups->header.cupsBytesPerLine, 2,
                                   "cups_print_pages");

  if (dst == NULL)	/* can't allocate working area */
    return_error(gs_error_VMerror);

 /*
  * See if the stream has been initialized yet...
  */

  if (cups->stream == NULL)
  {
    if ((cups->stream = cupsRasterOpen(1, CUPS_RASTER_WRITE)) == NULL)
    {
      perror("ERROR: Unable to open raster stream - ");
      gs_exit(0);
    }
  }

 /*
  * Output a page of graphics...
  */

  if (num_copies < 1)
    num_copies = 1;

  if (cups->ppd != NULL && !cups->ppd->manual_copies)
  {
    cups->header.NumCopies = num_copies;
    num_copies = 1;
  }

#ifdef DEBUG
  fprintf(stderr, "DEBUG: cupsWidth = %d, cupsHeight = %d, cupsBytesPerLine = %d\n",
          cups->header.cupsWidth, cups->header.cupsHeight,
	  cups->header.cupsBytesPerLine);
#endif /* DEBUG */

  for (copy = num_copies; copy > 0; copy --)
  {
    cupsRasterWriteHeader(cups->stream, &(cups->header));

    if (pdev->color_info.num_components == 1)
      cups_print_chunked(pdev, src, dst, srcbytes);
    else
      switch (cups->header.cupsColorOrder)
      {
	case CUPS_ORDER_CHUNKED :
            cups_print_chunked(pdev, src, dst, srcbytes);
	    break;
	case CUPS_ORDER_BANDED :
            cups_print_banded(pdev, src, dst, srcbytes);
	    break;
	case CUPS_ORDER_PLANAR :
            cups_print_planar(pdev, src, dst, srcbytes);
	    break;
      }
  }

 /*
  * Free temporary storage and return...
  */

  gs_free((char *)src, srcbytes, 1, "cups_print_pages");
  gs_free((char *)dst, cups->header.cupsBytesPerLine, 1, "cups_print_pages");

  cups->page ++;
  fprintf(stderr, "INFO: Processing page %d...\n", cups->page);

  return (0);
}


/*
 * 'cups_put_params()' - Set pagedevice parameters.
 */

private int				/* O - Error status */
cups_put_params(gx_device     *pdev,	/* I - Device info */
                gs_param_list *plist)	/* I - Parameter list */
{
  int			i;		/* Looping var */
  float			margins[4];	/* Physical margins of print */
  ppd_size_t		*size;		/* Page size */
  int			code;		/* Error code */
  int			intval;		/* Integer value */
  bool			boolval;	/* Boolean value */
  float			floatval;	/* Floating point value */
  gs_param_string	stringval;	/* String value */
  gs_param_float_array	arrayval;	/* Float array value */
  int			old_depth;	/* Old color depth */
  int			size_set;	/* Was the size set? */
  gdev_prn_space_params	sp;		/* Space parameter data */


#ifdef DEBUG
  fprintf(stderr, "DEBUG: cups_put_params(%p, %p)\n", pdev, plist);
#endif /* DEBUG */

 /*
  * Process other options for CUPS...
  */

#define stringoption(name, sname) \
  if ((code = param_read_string(plist, sname, &stringval)) < 0) \
  { \
    param_signal_error(plist, sname, code); \
    return (code); \
  } \
  else if (code == 0) \
  { \
    strncpy(cups->header.name, (const char *)stringval.data, \
            stringval.size); \
    cups->header.name[stringval.size] = '\0'; \
  }

#define intoption(name, sname, type) \
  if ((code = param_read_int(plist, sname, &intval)) < 0) \
  { \
    param_signal_error(plist, sname, code); \
    return (code); \
  } \
  else if (code == 0) \
  { \
    fprintf(stderr, "DEBUG: Setting %s to %d...\n", sname, intval); \
    cups->header.name = (type)intval; \
  }

#define floatoption(name, sname) \
  if ((code = param_read_float(plist, sname, &floatval)) < 0) \
  { \
    param_signal_error(plist, sname, code); \
    return (code); \
  } \
  else if (code == 0) \
    cups->header.name = (unsigned)floatval;

#define booloption(name, sname) \
  if ((code = param_read_bool(plist, sname, &boolval)) < 0) \
  { \
    if ((code = param_read_null(plist, sname)) < 0) \
    { \
      param_signal_error(plist, sname, code); \
      return (code); \
    } \
    if (code == 0) \
      cups->header.name = CUPS_FALSE; \
  } \
  else if (code == 0) \
    cups->header.name = (cups_bool_t)boolval;

#define arrayoption(name, sname, count) \
  if ((code = param_read_float_array(plist, sname, &arrayval)) < 0) \
  { \
    if ((code = param_read_null(plist, sname)) < 0) \
    { \
      param_signal_error(plist, sname, code); \
      return (code); \
    } \
    if (code == 0) \
      for (i = 0; i < count; i ++) \
	cups->header.name[i] = 0; \
  } \
  else if (code == 0) \
  { \
    for (i = 0; i < count; i ++) \
      cups->header.name[i] = (unsigned)arrayval.data[i]; \
  }

  old_depth = pdev->color_info.depth;
  size_set  = param_read_float_array(plist, "PageSize", &arrayval) == 0;

  stringoption(MediaClass, "MediaClass")
  stringoption(MediaColor, "MediaColor")
  stringoption(MediaType, "MediaType")
  stringoption(OutputType, "OutputType")
  floatoption(AdvanceDistance, "AdvanceDistance")
  intoption(AdvanceMedia, "AdvanceMedia", cups_adv_t)
  booloption(Collate, "Collate")
  intoption(CutMedia, "CutMedia", cups_cut_t)
  booloption(Duplex, "Duplex")
  arrayoption(ImagingBoundingBox, "ImagingBoundingBox", 4)
  booloption(InsertSheet, "InsertSheet")
  intoption(Jog, "Jog", cups_jog_t)
  intoption(LeadingEdge, "LeadingEdge", cups_edge_t)
  arrayoption(Margins, "Margins", 2)
  booloption(ManualFeed, "ManualFeed")
  intoption(MediaPosition, "cupsMediaPosition", unsigned) /* Compatibility */
  intoption(MediaPosition, "MediaPosition", unsigned)
  floatoption(MediaWeight, "MediaWeight")
  booloption(MirrorPrint, "MirrorPrint")
  booloption(NegativePrint, "NegativePrint")
  intoption(NumCopies, "NumCopies", unsigned)
  intoption(Orientation, "Orientation", cups_orient_t)
  booloption(OutputFaceUp, "OutputFaceUp")
  booloption(Separations, "Separations")
  booloption(TraySwitch, "TraySwitch")
  booloption(Tumble, "Tumble")
  intoption(cupsMediaType, "cupsMediaType", unsigned)
  intoption(cupsBitsPerColor, "cupsBitsPerColor", unsigned)
  intoption(cupsColorOrder, "cupsColorOrder", cups_order_t)
  intoption(cupsColorSpace, "cupsColorSpace", cups_cspace_t)
  intoption(cupsCompression, "cupsCompression", unsigned)
  intoption(cupsRowCount, "cupsRowCount", unsigned)
  intoption(cupsRowFeed, "cupsRowFeed", unsigned)
  intoption(cupsRowStep, "cupsRowStep", unsigned)

  cups_set_color_info(pdev);

  if (old_depth != pdev->color_info.depth)
  {
    fputs("DEBUG: Reallocating memory for new color depth...\n", stderr);
    sp = ((gx_device_printer *)pdev)->space_params;

    if ((code = gdev_prn_reallocate_memory(pdev, &sp, pdev->width,
                                           pdev->height)) < 0)
      return (code);
  }

 /*
  * Compute the page margins...
  */

  if (cups->ppd != NULL)
  {
   /*
    * Pull the margins from the first size entry; since the margins are not
    * like the bounding box we have to adjust the top and right values
    * accordingly.
    */

    for (i = cups->ppd->num_sizes, size = cups->ppd->sizes;
         i > 0;
         i --, size ++)
      if ((fabs(cups->PageSize[1] - size->length) < 18.0 &&
           fabs(cups->PageSize[0] - size->width) < 18.0) ||
          (fabs(cups->PageSize[0] - size->length) < 18.0 &&
           fabs(cups->PageSize[1] - size->width) < 18.0))
	break;

    if (i == 0 && !cups->ppd->variable_sizes)
    {
      i    = 1;
      size = cups->ppd->sizes;
    }

    if (i > 0)
    {
     /*
      * Standard size...
      */

      fprintf(stderr, "DEBUG: size = %s\n", size->name);

      margins[0] = size->left / 72.0;
      margins[1] = size->bottom / 72.0;
      margins[2] = (size->width - size->right) / 72.0;
      margins[3] = (size->length - size->top) / 72.0;
    }
    else
    {
     /*
      * Custom size...
      */

      fputs("DEBUG: size = Custom\n", stderr);

      for (i = 0; i < 4; i ++)
        margins[i] = cups->ppd->custom_margins[i] / 72.0;
    }

    fprintf(stderr, "DEBUG: margins[] = [ %f %f %f %f ]\n",
	    margins[0], margins[1], margins[2], margins[3]);
  }
  else
  {
   /*
    * Set default margins of 0.0...
    */

    memset(margins, 0, sizeof(margins));
  }

 /*
  * Set the margins to update the bitmap size...
  */

  gx_device_set_margins(pdev, margins, false);

 /*
  * Then process standard page device options...
  */

  if ((code = gdev_prn_put_params(pdev, plist)) < 0)
    return (code);

  cups->header.HWResolution[0] = pdev->HWResolution[0];
  cups->header.HWResolution[1] = pdev->HWResolution[1];

  cups->header.PageSize[0] = pdev->PageSize[0];
  cups->header.PageSize[1] = pdev->PageSize[1];

 /*
  * Reallocate memory if the size or color depth was changed...
  */

  if (old_depth != pdev->color_info.depth || size_set)
  {
    fputs("DEBUG: Reallocating memory...\n", stderr);
    sp = ((gx_device_printer *)pdev)->space_params;

    if ((code = gdev_prn_reallocate_memory(pdev, &sp, pdev->width,
                                           pdev->height)) < 0)
      return (code);
  }

#ifdef DEBUG
  fprintf(stderr, "DEBUG: ppd = %8x\n", cups->ppd);
  fprintf(stderr, "DEBUG: PageSize = [ %.3f %.3f ]\n",
          pdev->PageSize[0], pdev->PageSize[1]);
  fprintf(stderr, "DEBUG: margins = [ %.3f %.3f %.3f %.3f ]\n",
          margins[0], margins[1], margins[2], margins[3]);
  fprintf(stderr, "DEBUG: HWResolution = [ %.3f %.3f ]\n",
          pdev->HWResolution[0], pdev->HWResolution[1]);
  fprintf(stderr, "DEBUG: width = %d, height = %d\n",
          pdev->width, pdev->height);
  fprintf(stderr, "DEBUG: HWMargins = [ %.3f %.3f %.3f %.3f ]\n",
          pdev->HWMargins[0], pdev->HWMargins[1],
	  pdev->HWMargins[2], pdev->HWMargins[3]);
#endif /* DEBUG */

  return (0);
}


/*
 * 'cups_set_color_info()' - Set the color information structure based on
 *                           the required output.
 */

private void
cups_set_color_info(gx_device *pdev)	/* I - Device info */
{
  int		i, j, k;		/* Looping vars */
  float		d, g;			/* Density and gamma correction */
  float		m[3][3];		/* Color correction matrix */
  char		resolution[41];		/* Resolution string */
  ppd_profile_t	*profile;		/* Color profile information */


#ifdef DEBUG
  fprintf(stderr, "DEBUG: cups_set_color_info(%p)\n", pdev);
#endif /* DEBUG */

  switch (cups->header.cupsColorSpace)
  {
    default :
    case CUPS_CSPACE_W :
    case CUPS_CSPACE_K :
    case CUPS_CSPACE_WHITE :
    case CUPS_CSPACE_GOLD :
    case CUPS_CSPACE_SILVER :
        cups->header.cupsBitsPerPixel   = cups->header.cupsBitsPerColor;
        cups->color_info.depth          = cups->header.cupsBitsPerPixel;
        cups->color_info.num_components = 1;
        break;

    case CUPS_CSPACE_CMY :
    case CUPS_CSPACE_YMC :
    case CUPS_CSPACE_RGB :
        if (cups->header.cupsColorOrder != CUPS_ORDER_CHUNKED)
          cups->header.cupsBitsPerPixel = cups->header.cupsBitsPerColor;
	else if (cups->header.cupsBitsPerColor < 8)
	  cups->header.cupsBitsPerPixel = 4 * cups->header.cupsBitsPerColor;
	else
	  cups->header.cupsBitsPerPixel = 3 * cups->header.cupsBitsPerColor;

	if (cups->header.cupsBitsPerColor < 8)
	  cups->color_info.depth = 4 * cups->header.cupsBitsPerColor;
	else
	  cups->color_info.depth = 3 * cups->header.cupsBitsPerColor;

        cups->color_info.num_components = 3;
        break;

    case CUPS_CSPACE_KCMYcm :
        if (cups->header.cupsBitsPerColor == 1)
	{
	  cups->header.cupsBitsPerPixel   = 8;
	  cups->color_info.depth          = 8;
	  cups->color_info.num_components = 4;
	  break;
	}

    case CUPS_CSPACE_CMYK :
    case CUPS_CSPACE_YMCK :
    case CUPS_CSPACE_KCMY :
    case CUPS_CSPACE_GMCK :
    case CUPS_CSPACE_GMCS :
        if (cups->header.cupsColorOrder != CUPS_ORDER_CHUNKED)
          cups->header.cupsBitsPerPixel = cups->header.cupsBitsPerColor;
	else
	  cups->header.cupsBitsPerPixel = 4 * cups->header.cupsBitsPerColor;

        cups->color_info.depth          = 4 * cups->header.cupsBitsPerColor;
        cups->color_info.num_components = 4;
        break;
  }

  if (cups->color_info.num_components > 1)
  {
    cups->color_info.max_gray      = (1 << cups->header.cupsBitsPerColor) - 1;
    cups->color_info.max_color     = (1 << cups->header.cupsBitsPerColor) - 1;
    cups->color_info.dither_grays  = (1 << cups->header.cupsBitsPerColor);
    cups->color_info.dither_colors = (1 << cups->header.cupsBitsPerColor);
  }
  else
  {
    cups->color_info.max_gray      = (1 << cups->header.cupsBitsPerColor) - 1;
    cups->color_info.max_color     = 0;
    cups->color_info.dither_grays  = (1 << cups->header.cupsBitsPerColor);
    cups->color_info.dither_colors = 0;
  }

 /*
  * Enable/disable CMYK color support...
  */

  if (cups->color_info.num_components == 4)
    cups->procs.map_cmyk_color = cups_map_cmyk_color;
  else
    cups->procs.map_cmyk_color = NULL;

 /*
  * Compute the lookup tables...
  */

  for (i = 0; i <= gx_max_color_value; i ++)
    lut_rgb_color[i] = cups->color_info.max_gray * i / gx_max_color_value;

  for (i = 0; i < cups->color_info.dither_grays; i ++)
    lut_color_rgb[i] = gx_max_color_value * i / cups->color_info.max_gray;

#ifdef DEBUG
  fprintf(stderr, "DEBUG: num_components = %d, depth = %d\n",
          cups->color_info.num_components, cups->color_info.depth);
  fprintf(stderr, "DEBUG: cupsColorSpace = %d, cupsColorOrder = %d\n",
          cups->header.cupsColorSpace, cups->header.cupsColorOrder);
  fprintf(stderr, "DEBUG: cupsBitsPerPixel = %d, cupsBitsPerColor = %d\n",
          cups->header.cupsBitsPerPixel, cups->header.cupsBitsPerColor);
  fprintf(stderr, "DEBUG: max_gray = %d, dither_grays = %d\n",
          cups->color_info.max_gray, cups->color_info.dither_grays);
  fprintf(stderr, "DEBUG: max_color = %d, dither_colors = %d\n",
          cups->color_info.max_color, cups->color_info.dither_colors);
#endif /* DEBUG */

 /*
  * Set the color profile as needed...
  */

  cupsHaveProfile = 0;

  if (cupsProfile && cups->header.cupsBitsPerColor == 8)
  {
    fprintf(stderr, "DEBUG: Using user-defined profile \"%s\"...\n", cupsProfile);

    if (sscanf(cupsProfile, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f", &d, &g,
               m[0] + 0, m[0] + 1, m[0] + 2,
               m[1] + 0, m[1] + 1, m[1] + 2,
               m[2] + 0, m[2] + 1, m[2] + 2) != 11)
      fputs("DEBUG: User-defined profile does not contain 11 integers!\n", stderr);
    else
    {
      cupsHaveProfile = 1;

      d       *= 0.001f;
      g       *= 0.001f;
      m[0][0] *= 0.001f;
      m[0][1] *= 0.001f;
      m[0][2] *= 0.001f;
      m[1][0] *= 0.001f;
      m[1][1] *= 0.001f;
      m[1][2] *= 0.001f;
      m[2][0] *= 0.001f;
      m[2][1] *= 0.001f;
      m[2][2] *= 0.001f;
    }
  }
  else if (cups->ppd != NULL && cups->header.cupsBitsPerColor == 8)
  {
   /*
    * Find the appropriate color profile...
    */

    if (pdev->HWResolution[0] != pdev->HWResolution[1])
      sprintf(resolution, "%.0fx%.0fdpi", pdev->HWResolution[0],
              pdev->HWResolution[1]);
    else
      sprintf(resolution, "%.0fdpi", pdev->HWResolution[0]);

    for (i = 0, profile = cups->ppd->profiles;
         i < cups->ppd->num_profiles;
	 i ++, profile ++)
      if ((strcmp(profile->resolution, resolution) == 0 ||
           profile->resolution[0] == '-') &&
          (strcmp(profile->media_type, cups->header.MediaType) == 0 ||
           profile->media_type[0] == '-'))
	break;

   /*
    * If we found a color profile, use it!
    */

    if (i < cups->ppd->num_profiles)
    {
#ifdef DEBUG
      fputs("DEBUG: Using color profile!\n", stderr);
#endif /* DEBUG */

      cupsHaveProfile = 1;

      d = profile->density;
      g = profile->gamma;
      
      memcpy(m, profile->matrix, sizeof(m));
    }
  }

  if (cupsHaveProfile)
  {
    for (i = 0; i < 3; i ++)
      for (j = 0; j < 3; j ++)
	for (k = 0; k <= gx_max_color_value; k ++)
	{
          cupsMatrix[i][j][k] = (int)((float)k * m[i][j] + 0.5);

#ifdef DEBUG
          if ((k & 4095) == 0)
            fprintf(stderr, "DEBUG: cupsMatrix[%d][%d][%d] = %d\n",
	            i, j, k, cupsMatrix[i][j][k]);
#endif /* DEBUG */
        }


    for (k = 0; k <= gx_max_color_value; k ++)
    {
      cupsDensity[k] = (int)((float)gx_max_color_value * d *
	                     pow((float)k / (float)gx_max_color_value, g) +
			     0.5);

#ifdef DEBUG
      if ((k & 4095) == 0)
        fprintf(stderr, "DEBUG: cupsDensity[%d] = %d\n", k, cupsDensity[k]);
#endif /* DEBUG */
    }
  }
}


/*
 * 'cups_sync_output()' - Keep the user informed of our status...
 */

private int				/* O - Error status */
cups_sync_output(gx_device *pdev)	/* I - Device info */
{
  fprintf(stderr, "INFO: Processing page %d...\n", cups->page);

  return (0);
}


/*
 * 'cups_print_chunked()' - Print a page of chunked pixels.
 */

static void
cups_print_chunked(gx_device_printer *pdev,	/* I - Printer device */
                   unsigned char     *src,	/* I - Scanline buffer */
		   unsigned char     *dst,	/* I - Bitmap buffer */
		   int               srcbytes)	/* I - Number of bytes in src */
{
  int		y;				/* Looping var */
  unsigned char	*srcptr,			/* Pointer to data */
		*dstptr;			/* Pointer to bits */
  int		count;				/* Count for loop */
  int		flip;				/* Flip scanline? */


  if (cups->header.Duplex && !cups->header.Tumble &&
      cups->ppd && cups->ppd->flip_duplex && !(cups->page & 1))
    flip = 1;
  else
    flip = 0;

  fprintf(stderr, "DEBUG: cups_print_chunked - flip = %d\n", flip);

 /*
  * Loop through the page bitmap and write chunked pixels, reversing as
  * needed...
  */

  for (y = 0; y < cups->height; y ++)
  {
   /*
    * Grab the scanline data...
    */

    if (gdev_prn_get_bits((gx_device_printer *)pdev, y, src, &srcptr) < 0)
    {
      fprintf(stderr, "ERROR: Unable to get scanline %d!\n", y);
      gs_exit(1);
    }

    if (flip)
    {
     /*
      * Flip the raster data before writing it...
      */

      if (srcptr[0] == 0 && memcmp(srcptr, srcptr + 1, srcbytes - 1) == 0)
	memset(dst, 0, cups->header.cupsBytesPerLine);
      else
      {
        dstptr = dst;
	count  = srcbytes;

	switch (cups->color_info.depth)
	{
          case 1 : /* B&W bitmap */
	      for (srcptr += srcbytes - 1;
	           count > 0;
		   count --, srcptr --, dstptr ++)
	      {
	        *dstptr = rev_upper1[*srcptr & 15] |
		          rev_lower1[*srcptr >> 4];
              }
	      break;

	  case 2 : /* 2-bit grayscale */
	      for (srcptr += srcbytes - 1;
	           count > 0;
		   count --, srcptr --, dstptr ++)
	      {
	        *dstptr = rev_upper2[*srcptr & 15] |
		          rev_lower2[*srcptr >> 4];
              }
	      break;

	  case 4 : /* 4-bit grayscale, or RGB, CMY, or CMYK bitmap */
	      for (srcptr += srcbytes - 1;
	           count > 0;
		   count --, srcptr --, dstptr ++)
	        *dstptr = (*srcptr >> 4) | (*srcptr << 4);
	      break;

          case 8 : /* 8-bit grayscale, or 2-bit RGB, CMY, or CMYK image */
	      for (srcptr += srcbytes - 1;
	           count > 0;
		   count --, srcptr --, dstptr ++)
	        *dstptr = *srcptr;
	      break;

          case 16 : /* 4-bit RGB, CMY or CMYK image */
	      for (srcptr += srcbytes - 2;
	           count > 0;
		   count -= 2, srcptr -= 2, dstptr += 2)
	      {
	        dstptr[0] = srcptr[0];
	        dstptr[1] = srcptr[1];
              }
	      break;

          case 24 : /* 8-bit RGB or CMY image */
	      for (srcptr += srcbytes - 3;
	           count > 0;
		   count -= 3, srcptr -= 3, dstptr += 3)
	      {
	        dstptr[0] = srcptr[0];
	        dstptr[1] = srcptr[1];
	        dstptr[2] = srcptr[2];
              }
	      break;

          case 32 : /* 4-bit RGB, CMY or CMYK bitmap */
	      for (srcptr += srcbytes - 4;
	           count > 0;
		   count -= 4, srcptr -= 4, dstptr += 4)
	      {
	        dstptr[0] = srcptr[0];
	        dstptr[1] = srcptr[1];
	        dstptr[2] = srcptr[2];
	        dstptr[3] = srcptr[3];
              }
	      break;
        }
      }

     /*
      * Write the bitmap data to the raster stream...
      */

      cupsRasterWritePixels(cups->stream, dst, cups->header.cupsBytesPerLine);
    }
    else
    {
     /*
      * Write the scanline data to the raster stream...
      */

      cupsRasterWritePixels(cups->stream, srcptr, cups->header.cupsBytesPerLine);
    }
  }
}


/*
 * 'cups_print_banded()' - Print a page of banded pixels.
 */

static void
cups_print_banded(gx_device_printer *pdev,	/* I - Printer device */
                  unsigned char     *src,	/* I - Scanline buffer */
		  unsigned char     *dst,	/* I - Bitmap buffer */
		  int               srcbytes)	/* I - Number of bytes in src */
{
  int		x;				/* Looping var */
  int		y;				/* Looping var */
  int		bandbytes;			/* Bytes per band */
  unsigned char	bit;				/* Current bit */
  unsigned char	temp;				/* Temporary variable */
  unsigned char	*srcptr;			/* Pointer to data */
  unsigned char	*cptr, *mptr, *yptr, *kptr;	/* Pointer to components */
  unsigned char	*lcptr, *lmptr;			/* ... */
  int		flip;				/* Flip scanline? */


  if (cups->header.Duplex && !cups->header.Tumble &&
      cups->ppd && cups->ppd->flip_duplex && !(cups->page & 1))
    flip = 1;
  else
    flip = 0;

  fprintf(stderr, "DEBUG: cups_print_banded - flip = %d\n", flip);

 /*
  * Loop through the page bitmap and write banded pixels...  We have
  * to separate each chunked color as needed...
  */

  bandbytes = (cups->header.cupsWidth * cups->header.cupsBitsPerColor + 7) / 8;

  for (y = 0; y < cups->height; y ++)
  {
   /*
    * Grab the scanline data...
    */

    if (gdev_prn_get_bits((gx_device_printer *)pdev, y, src, &srcptr) < 0)
    {
      fprintf(stderr, "ERROR: Unable to get scanline %d!\n", y);
      gs_exit(1);
    }

   /*
    * Separate the chunked colors into their components...
    */

    if (srcptr[0] == 0 && memcmp(srcptr, srcptr + 1, srcbytes - 1) == 0)
      memset(dst, 0, cups->header.cupsBytesPerLine);
    else
    {
      if (flip)
        cptr = dst + bandbytes - 1;
      else
        cptr = dst;

      mptr  = cptr + bandbytes;
      yptr  = mptr + bandbytes;
      kptr  = yptr + bandbytes;
      lcptr = yptr + bandbytes;
      lmptr = lcptr + bandbytes;

      switch (cups->header.cupsBitsPerColor)
      {
	default :
            memset(dst, 0, cups->header.cupsBytesPerLine);

            switch (cups->header.cupsColorSpace)
	    {
	      default :
	          for (x = cups->width, bit = flip ? 1 << (x & 7) : 128;
		       x > 0;
		       x --, srcptr ++)
		  {
		    if (*srcptr & 0x40)
		      *cptr |= bit;
		    if (*srcptr & 0x20)
		      *mptr |= bit;
		    if (*srcptr & 0x10)
		      *yptr |= bit;

                    if (flip)
		    {
		      if (bit < 128)
			bit <<= 1;
		      else
		      {
			cptr --;
			mptr --;
			yptr --;
			bit = 1;
		      }
		    }
		    else
		      bit >>= 1;

		    x --;
		    if (x == 0)
		      break;

		    if (*srcptr & 0x4)
		      *cptr |= bit;
		    if (*srcptr & 0x2)
		      *mptr |= bit;
		    if (*srcptr & 0x1)
		      *yptr |= bit;

                    if (flip)
		    {
		      if (bit < 128)
			bit <<= 1;
		      else
		      {
			cptr --;
			mptr --;
			yptr --;
			bit = 1;
		      }
		    }
		    else if (bit > 1)
		      bit >>= 1;
		    else
		    {
		      cptr ++;
		      mptr ++;
		      yptr ++;
		      bit = 128;
		    }
		  }
	          break;
	      case CUPS_CSPACE_GMCK :
	      case CUPS_CSPACE_GMCS :
	      case CUPS_CSPACE_RGBA :
	      case CUPS_CSPACE_CMYK :
	      case CUPS_CSPACE_YMCK :
	      case CUPS_CSPACE_KCMY :
	          for (x = cups->width, bit = flip ? 1 << (x & 7) : 128;
		       x > 0;
		       x --, srcptr ++)
		  {
		    if (*srcptr & 0x80)
		      *cptr |= bit;
		    if (*srcptr & 0x40)
		      *mptr |= bit;
		    if (*srcptr & 0x20)
		      *yptr |= bit;
		    if (*srcptr & 0x10)
		      *kptr |= bit;

                    if (flip)
		    {
		      if (bit < 128)
			bit <<= 1;
		      else
		      {
			cptr --;
			mptr --;
			yptr --;
			kptr --;
			bit = 1;
		      }
		    }
		    else
		      bit >>= 1;

		    x --;
		    if (x == 0)
		      break;

		    if (*srcptr & 0x8)
		      *cptr |= bit;
		    if (*srcptr & 0x4)
		      *mptr |= bit;
		    if (*srcptr & 0x2)
		      *yptr |= bit;
		    if (*srcptr & 0x1)
		      *kptr |= bit;

                    if (flip)
		    {
		      if (bit < 128)
			bit <<= 1;
		      else
		      {
			cptr --;
			mptr --;
			yptr --;
			kptr --;
			bit = 1;
		      }
		    }
		    else if (bit > 1)
		      bit >>= 1;
		    else
		    {
		      cptr ++;
		      mptr ++;
		      yptr ++;
		      kptr ++;
		      bit = 128;
		    }
		  }
	          break;
	      case CUPS_CSPACE_KCMYcm :
	          for (x = cups->width, bit = flip ? 1 << (x & 7) : 128;
		       x > 0;
		       x --, srcptr ++)
		  {
                   /*
                    * Note: Because of the way the pointers are setup,
                    *       the following code is correct even though
                    *       the names don't match...
                    */

		    if (*srcptr & 0x20)
		      *cptr |= bit;
		    if (*srcptr & 0x10)
		      *mptr |= bit;
		    if (*srcptr & 0x08)
		      *yptr |= bit;
		    if (*srcptr & 0x04)
		      *kptr |= bit;
		    if (*srcptr & 0x02)
		      *lcptr |= bit;
		    if (*srcptr & 0x01)
		      *lmptr |= bit;

                    if (flip)
		    {
		      if (bit < 128)
			bit <<= 1;
		      else
		      {
			cptr --;
			mptr --;
			yptr --;
			kptr --;
			lcptr --;
			lmptr --;
			bit = 1;
		      }
		    }
		    else if (bit > 1)
		      bit >>= 1;
		    else
		    {
		      cptr ++;
		      mptr ++;
		      yptr ++;
		      kptr ++;
		      lcptr ++;
		      lmptr ++;
		      bit = 128;
		    }
		  }
	          break;
	    }
            break;

	case 2 :
            memset(dst, 0, cups->header.cupsBytesPerLine);

            switch (cups->header.cupsColorSpace)
	    {
	      default :
	          for (x = cups->width, bit = flip ? 3 << (2 * (x & 3)) : 0xc0;
		       x > 0;
		       x --, srcptr ++)
		    switch (bit)
		    {
		      case 0xc0 :
			  if ((temp = *srcptr & 0x30) != 0)
			    *cptr |= temp << 2;
			  if ((temp = *srcptr & 0x0c) != 0)
			    *mptr |= temp << 4;
			  if ((temp = *srcptr & 0x03) != 0)
			    *yptr |= temp << 6;

                          if (flip)
			  {
			    bit = 0x03;
			    cptr --;
			    mptr --;
			    yptr --;
			  }
			  else
			    bit = 0x30;
			  break;
		      case 0x30 :
			  if ((temp = *srcptr & 0x30) != 0)
			    *cptr |= temp;
			  if ((temp = *srcptr & 0x0c) != 0)
			    *mptr |= temp << 2;
			  if ((temp = *srcptr & 0x03) != 0)
			    *yptr |= temp << 4;

			  if (flip)
			    bit = 0xc0;
			  else
			    bit = 0x0c;
			  break;
		      case 0x0c :
			  if ((temp = *srcptr & 0x30) != 0)
			    *cptr |= temp >> 2;
			  if ((temp = *srcptr & 0x0c) != 0)
			    *mptr |= temp;
			  if ((temp = *srcptr & 0x03) != 0)
			    *yptr |= temp << 2;

			  if (flip)
			    bit = 0x30;
			  else
			    bit = 0x03;
			  break;
		      case 0x03 :
			  if ((temp = *srcptr & 0x30) != 0)
			    *cptr |= temp >> 4;
			  if ((temp = *srcptr & 0x0c) != 0)
			    *mptr |= temp >> 2;
			  if ((temp = *srcptr & 0x03) != 0)
			    *yptr |= temp;

			  if (flip)
			    bit = 0x0c;
			  else
			  {
			    bit = 0xc0;
			    cptr ++;
			    mptr ++;
			    yptr ++;
			  }
			  break;
                    }
	          break;
	      case CUPS_CSPACE_GMCK :
	      case CUPS_CSPACE_GMCS :
	      case CUPS_CSPACE_RGBA :
	      case CUPS_CSPACE_CMYK :
	      case CUPS_CSPACE_YMCK :
	      case CUPS_CSPACE_KCMY :
	      case CUPS_CSPACE_KCMYcm :
	          for (x = cups->width, bit = flip ? 3 << (2 * (x & 3)) : 0xc0;
		       x > 0;
		       x --, srcptr ++)
		    switch (bit)
		    {
		      case 0xc0 :
		          if ((temp = *srcptr & 0xc0) != 0)
			    *cptr |= temp;
			  if ((temp = *srcptr & 0x30) != 0)
			    *mptr |= temp << 2;
			  if ((temp = *srcptr & 0x0c) != 0)
			    *yptr |= temp << 4;
			  if ((temp = *srcptr & 0x03) != 0)
			    *kptr |= temp << 6;

                          if (flip)
			  {
			    bit = 0x03;
			    cptr --;
			    mptr --;
			    yptr --;
			    kptr --;
			  }
			  else
			    bit = 0x30;
			  break;
		      case 0x30 :
		          if ((temp = *srcptr & 0xc0) != 0)
			    *cptr |= temp >> 2;
			  if ((temp = *srcptr & 0x30) != 0)
			    *mptr |= temp;
			  if ((temp = *srcptr & 0x0c) != 0)
			    *yptr |= temp << 2;
			  if ((temp = *srcptr & 0x03) != 0)
			    *kptr |= temp << 4;

			  if (flip)
			    bit = 0xc0;
			  else
			    bit = 0x0c;
			  break;
		      case 0x0c :
		          if ((temp = *srcptr & 0xc0) != 0)
			    *cptr |= temp >> 4;
			  if ((temp = *srcptr & 0x30) != 0)
			    *mptr |= temp >> 2;
			  if ((temp = *srcptr & 0x0c) != 0)
			    *yptr |= temp;
			  if ((temp = *srcptr & 0x03) != 0)
			    *kptr |= temp << 2;

			  if (flip)
			    bit = 0x30;
			  else
			    bit = 0x03;
			  break;
		      case 0x03 :
		          if ((temp = *srcptr & 0xc0) != 0)
			    *cptr |= temp >> 6;
			  if ((temp = *srcptr & 0x30) != 0)
			    *mptr |= temp >> 4;
			  if ((temp = *srcptr & 0x0c) != 0)
			    *yptr |= temp >> 2;
			  if ((temp = *srcptr & 0x03) != 0)
			    *kptr |= temp;

			  if (flip)
			    bit = 0x0c;
			  else
			  {
			    bit = 0xc0;
			    cptr ++;
			    mptr ++;
			    yptr ++;
			    kptr ++;
			  }
			  break;
                    }
	          break;
	    }
            break;

	case 4 :
            memset(dst, 0, cups->header.cupsBytesPerLine);

            switch (cups->header.cupsColorSpace)
	    {
	      default :
	          for (x = cups->width, bit = flip && (x & 1) ? 0xf0 : 0x0f;
		       x > 0;
		       x --, srcptr += 2)
		    switch (bit)
		    {
		      case 0xf0 :
			  if ((temp = srcptr[0] & 0x0f) != 0)
			    *cptr |= temp << 4;
			  if ((temp = srcptr[1] & 0xf0) != 0)
			    *mptr |= temp;
			  if ((temp = srcptr[1] & 0x0f) != 0)
			    *yptr |= temp << 4;

			  bit = 0x0f;

                          if (flip)
			  {
			    cptr --;
			    mptr --;
			    yptr --;
			  }
			  break;
		      case 0x0f :
			  if ((temp = srcptr[0] & 0x0f) != 0)
			    *cptr |= temp;
			  if ((temp = srcptr[1] & 0xf0) != 0)
			    *mptr |= temp >> 4;
			  if ((temp = srcptr[1] & 0x0f) != 0)
			    *yptr |= temp;

			  bit = 0xf0;

                          if (!flip)
			  {
			    cptr ++;
			    mptr ++;
			    yptr ++;
			  }
			  break;
                    }
	          break;
	      case CUPS_CSPACE_GMCK :
	      case CUPS_CSPACE_GMCS :
	      case CUPS_CSPACE_RGBA :
	      case CUPS_CSPACE_CMYK :
	      case CUPS_CSPACE_YMCK :
	      case CUPS_CSPACE_KCMY :
	      case CUPS_CSPACE_KCMYcm :
	          for (x = cups->width, bit = flip && (x & 1) ? 0xf0 : 0x0f;
		       x > 0;
		       x --, srcptr += 2)
		    switch (bit)
		    {
		      case 0xf0 :
		          if ((temp = srcptr[0] & 0xf0) != 0)
			    *cptr |= temp;
			  if ((temp = srcptr[0] & 0x0f) != 0)
			    *mptr |= temp << 4;
			  if ((temp = srcptr[1] & 0xf0) != 0)
			    *yptr |= temp;
			  if ((temp = srcptr[1] & 0x0f) != 0)
			    *kptr |= temp << 4;

			  bit = 0x0f;

                          if (flip)
			  {
			    cptr --;
			    mptr --;
			    yptr --;
			    kptr --;
			  }
			  break;
		      case 0x0f :
		          if ((temp = srcptr[0] & 0xf0) != 0)
			    *cptr |= temp >> 4;
			  if ((temp = srcptr[0] & 0x0f) != 0)
			    *mptr |= temp;
			  if ((temp = srcptr[1] & 0xf0) != 0)
			    *yptr |= temp >> 4;
			  if ((temp = srcptr[1] & 0x0f) != 0)
			    *kptr |= temp;

			  bit = 0xf0;

                          if (!flip)
			  {
			    cptr ++;
			    mptr ++;
			    yptr ++;
			    kptr ++;
			  }
			  break;
                    }
	          break;
	    }
            break;

	case 8 :
            switch (cups->header.cupsColorSpace)
	    {
	      default :
	          if (flip)
	            for (x = cups->width; x > 0; x --)
		    {
		      *cptr-- = *srcptr++;
		      *mptr-- = *srcptr++;
		      *yptr-- = *srcptr++;
		    }
		  else
	            for (x = cups->width; x > 0; x --)
		    {
		      *cptr++ = *srcptr++;
		      *mptr++ = *srcptr++;
		      *yptr++ = *srcptr++;
		    }
	          break;
	      case CUPS_CSPACE_GMCK :
	      case CUPS_CSPACE_GMCS :
	      case CUPS_CSPACE_RGBA :
	      case CUPS_CSPACE_CMYK :
	      case CUPS_CSPACE_YMCK :
	      case CUPS_CSPACE_KCMY :
	      case CUPS_CSPACE_KCMYcm :
	          if (flip)
	            for (x = cups->width; x > 0; x --)
		    {
		      *cptr-- = *srcptr++;
		      *mptr-- = *srcptr++;
		      *yptr-- = *srcptr++;
		      *kptr-- = *srcptr++;
		    }
		  else
	            for (x = cups->width; x > 0; x --)
		    {
		      *cptr++ = *srcptr++;
		      *mptr++ = *srcptr++;
		      *yptr++ = *srcptr++;
		      *kptr++ = *srcptr++;
		    }
	          break;
	    }
            break;
      }
    }

   /*
    * Write the bitmap data to the raster stream...
    */

    cupsRasterWritePixels(cups->stream, dst, cups->header.cupsBytesPerLine);
  }
}


/*
 * 'cups_print_planar()' - Print a page of planar pixels.
 */

static void
cups_print_planar(gx_device_printer *pdev,	/* I - Printer device */
                  unsigned char     *src,	/* I - Scanline buffer */
		  unsigned char     *dst,	/* I - Bitmap buffer */
		  int               srcbytes)	/* I - Number of bytes in src */
{
  int		x;				/* Looping var */
  int		y;				/* Looping var */
  int		z;				/* Looping var */
  unsigned char	srcbit;				/* Current source bit */
  unsigned char	dstbit;				/* Current destination bit */
  unsigned char	temp;				/* Temporary variable */
  unsigned char	*srcptr;			/* Pointer to data */
  unsigned char	*dstptr;			/* Pointer to bitmap */


 /**** NOTE: Currently planar output doesn't support flipped duplex!!! ****/

 /*
  * Loop through the page bitmap and write planar pixels...  We have
  * to separate each chunked color as needed...
  */

  for (z = 0; z < pdev->color_info.num_components; z ++)
    for (y = 0; y < cups->height; y ++)
    {
     /*
      * Grab the scanline data...
      */

      if (gdev_prn_get_bits((gx_device_printer *)pdev, y, src, &srcptr) < 0)
      {
	fprintf(stderr, "ERROR: Unable to get scanline %d!\n", y);
	gs_exit(1);
      }

     /*
      * Pull the individual color planes out of the pixels...
      */

      if (srcptr[0] == 0 && memcmp(srcptr, srcptr + 1, srcbytes - 1) == 0)
	memset(dst, 0, cups->header.cupsBytesPerLine);
      else
	switch (cups->header.cupsBitsPerColor)
	{
          default :
	      memset(dst, 0, cups->header.cupsBytesPerLine);

	      switch (cups->header.cupsColorSpace)
	      {
		default :
	            for (dstptr = dst, x = cups->width, srcbit = 64 >> z,
		             dstbit = 128;
			 x > 0;
			 x --)
		    {
		      if (*srcptr & srcbit)
			*dstptr |= dstbit;

                      if (srcbit >= 16)
			srcbit >>= 4;
		      else
		      {
			srcbit = 64 >> z;
			srcptr ++;
		      }

                      if (dstbit > 1)
			dstbit >>= 1;
		      else
		      {
			dstbit = 128;
			dstptr ++;
		      }
		    }
	            break;
		case CUPS_CSPACE_GMCK :
		case CUPS_CSPACE_GMCS :
		case CUPS_CSPACE_RGBA :
		case CUPS_CSPACE_CMYK :
		case CUPS_CSPACE_YMCK :
		case CUPS_CSPACE_KCMY :
	            for (dstptr = dst, x = cups->width, srcbit = 128 >> z,
		             dstbit = 128;
			 x > 0;
			 x --)
		    {
		      if (*srcptr & srcbit)
			*dstptr |= dstbit;

                      if (srcbit >= 16)
			srcbit >>= 4;
		      else
		      {
			srcbit = 128 >> z;
			srcptr ++;
		      }

                      if (dstbit > 1)
			dstbit >>= 1;
		      else
		      {
			dstbit = 128;
			dstptr ++;
		      }
		    }
	            break;
		case CUPS_CSPACE_KCMYcm :
	            for (dstptr = dst, x = cups->width, srcbit = 32 >> z,
		             dstbit = 128;
			 x > 0;
			 x --, srcptr ++)
		    {
		      if (*srcptr & srcbit)
			*dstptr |= dstbit;

                      if (dstbit > 1)
			dstbit >>= 1;
		      else
		      {
			dstbit = 128;
			dstptr ++;
		      }
		    }
	            break;
              }
	      break;

	  case 2 :
	      memset(dst, 0, cups->header.cupsBytesPerLine);

	      switch (cups->header.cupsColorSpace)
	      {
		default :
	            for (dstptr = dst, x = cups->width, srcbit = 48 >> (z * 2),
		             dstbit = 0xc0;
			 x > 0;
			 x --, srcptr ++)
		    {
		      if ((temp = *srcptr & srcbit) != 0)
		      {
			if (srcbit == dstbit)
		          *dstptr |= temp;
	        	else
			{
		          switch (srcbit)
			  {
			    case 0x30 :
				temp >>= 4;
				break;
			    case 0x0c :
				temp >>= 2;
				break;
                          }

		          switch (dstbit)
			  {
			    case 0xc0 :
				*dstptr |= temp << 6;
				break;
			    case 0x30 :
				*dstptr |= temp << 4;
				break;
			    case 0x0c :
				*dstptr |= temp << 2;
				break;
			    case 0x03 :
				*dstptr |= temp;
				break;
                          }
			}
		      }

		      if (dstbit > 0x03)
			dstbit >>= 2;
		      else
		      {
			dstbit = 0xc0;
			dstptr ++;
		      }
		    }
	            break;
		case CUPS_CSPACE_GMCK :
		case CUPS_CSPACE_GMCS :
		case CUPS_CSPACE_RGBA :
		case CUPS_CSPACE_CMYK :
		case CUPS_CSPACE_YMCK :
		case CUPS_CSPACE_KCMY :
		case CUPS_CSPACE_KCMYcm :
	            for (dstptr = dst, x = cups->width, srcbit = 192 >> (z * 2),
		             dstbit = 0xc0;
			 x > 0;
			 x --, srcptr ++)
		    {
		      if ((temp = *srcptr & srcbit) != 0)
		      {
			if (srcbit == dstbit)
		          *dstptr |= temp;
	        	else
			{
		          switch (srcbit)
			  {
			    case 0xc0 :
				temp >>= 6;
				break;
			    case 0x30 :
				temp >>= 4;
				break;
			    case 0x0c :
				temp >>= 2;
				break;
                          }

		          switch (dstbit)
			  {
			    case 0xc0 :
				*dstptr |= temp << 6;
				break;
			    case 0x30 :
				*dstptr |= temp << 4;
				break;
			    case 0x0c :
				*dstptr |= temp << 2;
				break;
			    case 0x03 :
				*dstptr |= temp;
				break;
                          }
			}
		      }

		      if (dstbit > 0x03)
			dstbit >>= 2;
		      else
		      {
			dstbit = 0xc0;
			dstptr ++;
		      }
		    }
	            break;
              }
	      break;

	  case 4 :
	      memset(dst, 0, cups->header.cupsBytesPerLine);

	      switch (cups->header.cupsColorSpace)
	      {
		default :
	            if (z > 0)
		      srcptr ++;

		    if (z == 1)
		      srcbit = 0xf0;
		    else
		      srcbit = 0x0f;

	            for (dstptr = dst, x = cups->width, dstbit = 0xf0;
			 x > 0;
			 x --, srcptr += 2)
		    {
		      if ((temp = *srcptr & srcbit) != 0)
		      {
			if (srcbit == dstbit)
		          *dstptr |= temp;
	        	else
			{
		          if (srcbit == 0xf0)
	                    temp >>= 4;

		          if (dstbit == 0xf0)
   			    *dstptr |= temp << 4;
			  else
			    *dstptr |= temp;
			}
		      }

		      if (dstbit == 0xf0)
			dstbit = 0x0f;
		      else
		      {
			dstbit = 0xf0;
			dstptr ++;
		      }
		    }
	            break;
		case CUPS_CSPACE_GMCK :
		case CUPS_CSPACE_GMCS :
		case CUPS_CSPACE_RGBA :
		case CUPS_CSPACE_CMYK :
		case CUPS_CSPACE_YMCK :
		case CUPS_CSPACE_KCMY :
		case CUPS_CSPACE_KCMYcm :
	            if (z > 1)
		      srcptr ++;

		    if (z & 1)
		      srcbit = 0x0f;
		    else
		      srcbit = 0xf0;

	            for (dstptr = dst, x = cups->width, dstbit = 0xf0;
			 x > 0;
			 x --, srcptr += 2)
		    {
		      if ((temp = *srcptr & srcbit) != 0)
		      {
			if (srcbit == dstbit)
		          *dstptr |= temp;
	        	else
			{
		          if (srcbit == 0xf0)
	                    temp >>= 4;

		          if (dstbit == 0xf0)
   			    *dstptr |= temp << 4;
			  else
			    *dstptr |= temp;
			}
		      }

		      if (dstbit == 0xf0)
			dstbit = 0x0f;
		      else
		      {
			dstbit = 0xf0;
			dstptr ++;
		      }
		    }
	            break;
              }
	      break;

	  case 8 :
	      for (srcptr += z, dstptr = dst, x = cups->header.cupsBytesPerLine;
	           x > 0;
		   srcptr += pdev->color_info.num_components, x --)
		*dstptr++ = *srcptr;
	      break;
	}

     /*
      * Write the bitmap data to the raster stream...
      */

      cupsRasterWritePixels(cups->stream, dst, cups->header.cupsBytesPerLine);
    }
}


/*
 * End of "$Id: gdevcups.c,v 1.43.2.1 2001/12/26 16:52:48 mike Exp $".
 */
