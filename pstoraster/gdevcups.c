/*
 * "$Id: gdevcups.c,v 1.8 1999/07/22 20:58:34 mike Exp $"
 *
 *   GNU Ghostscript raster output driver for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 1993-1999 by Easy Software Products.
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

#include <stdlib.h>
#include <cups/raster.h>
#include <cups/ppd.h>
#include <math.h>


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
private dev_proc_map_color_rgb(cups_map_color_rgb);
private dev_proc_map_cmyk_color(cups_map_cmyk_color);
private dev_proc_map_rgb_color(cups_map_rgb_color);
private dev_proc_open_device(cups_open);
private int cups_print_pages(gx_device_printer *, FILE *, int);
private int cups_put_params(gx_device *, gs_param_list *);
private void cups_set_color_info(gx_device *);

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
   gx_default_sync_output,
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
   NULL,
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


/*
 * Local functions...
 */

static void	cups_print_chunked(gx_device_printer *, unsigned char *);
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
  fprintf(stderr, "DEBUG: cups_close(%08x)\n", pdev);
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
  fprintf(stderr, "DEBUG: cups_get_matrix(%08x, %08x)\n", pdev, pmat);
#endif /* DEBUG */

 /*
  * Set the raster width and height...
  */

  cups->header.cupsWidth  = cups->width;
  cups->header.cupsHeight = cups->height;

 /*
  * Set the transform matrix...
  */

  pmat->xx = (float)cups->header.HWResolution[0] / 72.0;
  pmat->xy = 0.0;
  pmat->yx = 0.0;
  pmat->yy = -(float)cups->header.HWResolution[1] / 72.0;
  pmat->tx = -(float)cups->header.HWResolution[0] * pdev->HWMargins[0] / 72.0;
  pmat->ty = (float)cups->header.HWResolution[1] *
             ((float)cups->header.PageSize[1] - pdev->HWMargins[3]) / 72.0;

#ifdef DEBUG
  fprintf(stderr, "DEBUG: width = %d, height = %d\n", cups->width,
          cups->height);
  fprintf(stderr, "DEBUG: PageSize = [ %d %d ], HWResolution = [ %d %d ]\n",
          cups->header.PageSize[0], cups->header.PageSize[1],
          cups->header.HWResolution[0], cups->header.HWResolution[1]);
  fprintf(stderr, "DEBUG: matrix = [ %.3f %.3f %.3f %.3f %.3f %.3f ]\n",
          pmat->xx, pmat->xy, pmat->yx, pmat->yy, pmat->tx, pmat->ty);
#endif /* DEBUG */
}


/*
 * 'cups_get_params()' - Get pagedevice parameters.
 */

private int				/* O - Error status */
cups_get_params(gx_device     *pdev,	/* I - Device info */
                gs_param_list *plist)	/* I - Parameter list */
{
  int	code;				/* Return code */


#ifdef DEBUG
  fprintf(stderr, "DEBUG: cups_get_params(%08x, %08x)\n", pdev, plist);
#endif /* DEBUG */

 /*
  * First process the "standard" page device parameters...
  */

  if ((code = gdev_prn_get_params(pdev, plist)) < 0)
    return (code);

 /*
  * Then write the CUPS-specific parameters...
  */

  if ((code = param_write_int(plist, "cupsWidth",
                              (int *)&(cups->header.cupsWidth))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsHeight",
                              (int *)&(cups->header.cupsHeight))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsMediaType",
                              (int *)&(cups->header.cupsMediaType))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsBitsPerColor",
                              (int *)&(cups->header.cupsBitsPerColor))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsBitsPerPixel",
                              (int *)&(cups->header.cupsBitsPerPixel))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsBytesPerLine",
                              (int *)&(cups->header.cupsBytesPerLine))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsColorOrder",
                              (int *)&(cups->header.cupsColorOrder))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsColorSpace",
                              (int *)&(cups->header.cupsColorSpace))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsCompression",
                              (int *)&(cups->header.cupsCompression))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsRowCount",
                              (int *)&(cups->header.cupsRowCount))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsRowFeed",
                              (int *)&(cups->header.cupsRowFeed))) < 0)
    return (code);

  if ((code = param_write_int(plist, "cupsRowStep",
                              (int *)&(cups->header.cupsRowStep))) < 0)
    return (code);

  return (0);
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
  fprintf(stderr, "DEBUG: cups_map_color_rgb(%08x, %d, %08x)\n", pdev,
          color, prgb);
#endif /* DEBUG */

 /*
  * Setup the color info data as needed...
  */

  if (pdev->color_info.num_components == 0)
    cups_set_color_info(pdev);

#ifdef DEBUG
  fprintf(stderr, "DEBUG: COLOR %08x = ", color);
#endif /* DEBUG */

 /*
  * Extract the color components from the color index...
  */

  switch (cups->header.cupsBitsPerColor)
  {
    case 1 :
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
        c2 = color & 3;
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
  gx_color_value	ic, im, iy, ik;		/* Integral CMYKcm values */
  float			divk,			/* Black "divisor" */
			diff;			/* Average color difference */
  int			tc, tm, ty, tk;		/* Temporary color values */


#ifdef DEBUG
  fprintf(stderr, "DEBUG: cups_map_rgb_color(%08x, %d, %d, %d)\n", pdev, r, g, b);
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
          case 1 :
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

    case CUPS_CSPACE_K :
    case CUPS_CSPACE_WHITE :
    case CUPS_CSPACE_GOLD :
    case CUPS_CSPACE_SILVER :
        i = lut_rgb_color[gx_max_color_value - (r * 31 + g * 61 + b * 8) / 100];
        break;

    case CUPS_CSPACE_CMY :
        if (cups->header.cupsBitsPerColor == 1)
        {
          ic = lut_rgb_color[gx_max_color_value - r];
          im = lut_rgb_color[gx_max_color_value - g];
          iy = lut_rgb_color[gx_max_color_value - b];
        }
        else
        {
	  ic = gx_max_color_value - r;
	  im = gx_max_color_value - g;
	  iy = gx_max_color_value - b;
	  ik = min(ic, min(im, iy));

	  ic = lut_rgb_color[(gx_color_value)((float)(gx_max_color_value - g / 4) /
	                     (float)gx_max_color_value * (float)(ic - ik)) + ik];
	  im = lut_rgb_color[(gx_color_value)((float)(gx_max_color_value - b / 4) /
	                     (float)gx_max_color_value * (float)(im - ik)) + ik];
	  iy = lut_rgb_color[(gx_color_value)((float)(gx_max_color_value - r / 4) /
	                     (float)gx_max_color_value * (float)(iy - ik)) + ik];
        }

        switch (cups->header.cupsBitsPerColor)
        {
          case 1 :
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
        if (cups->header.cupsBitsPerColor == 1)
        {
          ic = lut_rgb_color[gx_max_color_value - r];
          im = lut_rgb_color[gx_max_color_value - g];
          iy = lut_rgb_color[gx_max_color_value - b];
        }
        else
        {
	  ic = gx_max_color_value - r;
	  im = gx_max_color_value - g;
	  iy = gx_max_color_value - b;
	  ik = min(ic, min(im, iy));

	  ic = lut_rgb_color[(gx_color_value)((float)(gx_max_color_value - g / 4) /
	                     (float)gx_max_color_value * (float)(ic - ik)) + ik];
	  im = lut_rgb_color[(gx_color_value)((float)(gx_max_color_value - b / 4) /
	                     (float)gx_max_color_value * (float)(im - ik)) + ik];
	  iy = lut_rgb_color[(gx_color_value)((float)(gx_max_color_value - r / 4) /
	                     (float)gx_max_color_value * (float)(iy - ik)) + ik];
        }

        switch (cups->header.cupsBitsPerColor)
        {
          case 1 :
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

        if (ik > 0)
	{
	  diff = 1.0 - (float)(max(ic, max(im, iy)) - ik) /
	               (float)gx_max_color_value;
          ik   = (int)(diff * (float)ik);
	}

        if (ik == gx_max_color_value)
        {
          ik = lut_rgb_color[ik];
          ic = 0;
          im = 0;
          iy = 0;
        }
        else if (cups->header.cupsBitsPerColor == 1)
        {
          ic = lut_rgb_color[ic - ik];
          im = lut_rgb_color[im - ik];
          iy = lut_rgb_color[iy - ik];
          ik = lut_rgb_color[ik];
        }
        else
        {
          divk = (float)gx_max_color_value / (float)(gx_max_color_value - ik);
	  tc   = (float)(ic - ik) * divk;
	  tm   = (float)(im - ik) * divk;
	  ty   = (float)(iy - ik) * divk;

          if (tc >= gx_max_color_value)
	    ic = lut_rgb_color[gx_max_color_value];
	  else
	    ic = lut_rgb_color[tc];

          if (tm >= gx_max_color_value)
	    im = lut_rgb_color[gx_max_color_value];
	  else
	    im = lut_rgb_color[tm];

          if (ty >= gx_max_color_value)
	    iy = lut_rgb_color[gx_max_color_value];
	  else
	    iy = lut_rgb_color[ty];

          ik = lut_rgb_color[ik];
        }

        switch (cups->header.cupsBitsPerColor)
        {
          case 1 :
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
	  fprintf(stderr, "DEBUG: CMY (%d,%d,%d) -> CMYK %08.8x (%d,%d,%d,%d)\n",
	          r, g, b, i, ic, im, iy, ik);
        break;

    case CUPS_CSPACE_YMCK :
    case CUPS_CSPACE_GMCK :
    case CUPS_CSPACE_GMCS :
	ic = gx_max_color_value - r;
	im = gx_max_color_value - g;
	iy = gx_max_color_value - b;
        ik = min(ic, min(im, iy));

        if (ik > 0)
	{
	  diff = 1.0 - (float)(max(ic, max(im, iy)) - ik) /
	               (float)gx_max_color_value;
          ik   = (int)(diff * (float)ik);
	}

        if (ik == gx_max_color_value)
        {
          ik = lut_rgb_color[ik];
          ic = 0;
          im = 0;
          iy = 0;
        }
        else if (cups->header.cupsBitsPerColor == 1)
        {
          ic = lut_rgb_color[ic - ik];
          im = lut_rgb_color[im - ik];
          iy = lut_rgb_color[iy - ik];
          ik = lut_rgb_color[ik];
        }
        else
        {
          divk = (float)gx_max_color_value / (float)(gx_max_color_value - ik);
	  tc   = (float)(ic - ik) * divk;
	  tm   = (float)(im - ik) * divk;
	  ty   = (float)(iy - ik) * divk;

          if (tc >= gx_max_color_value)
	    ic = lut_rgb_color[gx_max_color_value];
	  else
	    ic = lut_rgb_color[tc];

          if (tm >= gx_max_color_value)
	    im = lut_rgb_color[gx_max_color_value];
	  else
	    im = lut_rgb_color[tm];

          if (ty >= gx_max_color_value)
	    iy = lut_rgb_color[gx_max_color_value];
	  else
	    iy = lut_rgb_color[ty];

          ik = lut_rgb_color[ik];
        }

        switch (cups->header.cupsBitsPerColor)
        {
          case 1 :
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

          if (ik > 0)
	  {
	    diff = 1.0 - (float)(max(ic, max(im, iy)) - ik) / (float)ik;
            ik   = (int)(diff * (float)ik);
	  }

          ic = lut_rgb_color[ic - ik];
          im = lut_rgb_color[im - ik];
          iy = lut_rgb_color[iy - ik];
          ik = lut_rgb_color[ik];
	  if (ik)
	    i = 32;
	  else if (ic && im)
	    i = 3;
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

        if (ik > 0)
	{
	  diff = 1.0 - (float)(max(ic, max(im, iy)) - ik) /
	               (float)gx_max_color_value;
          ik   = (int)(diff * (float)ik);
	}

        if (ik == gx_max_color_value)
        {
          ik = lut_rgb_color[ik];
          ic = 0;
          im = 0;
          iy = 0;
        }
        else if (cups->header.cupsBitsPerColor == 1)
        {
          ic = lut_rgb_color[ic - ik];
          im = lut_rgb_color[im - ik];
          iy = lut_rgb_color[iy - ik];
          ik = lut_rgb_color[ik];
        }
        else
        {
          divk = (float)gx_max_color_value / (float)(gx_max_color_value - ik);
	  tc   = (float)(ic - ik) * divk;
	  tm   = (float)(im - ik) * divk;
	  ty   = (float)(iy - ik) * divk;

          if (tc >= gx_max_color_value)
	    ic = lut_rgb_color[gx_max_color_value];
	  else
	    ic = lut_rgb_color[tc];

          if (tm >= gx_max_color_value)
	    im = lut_rgb_color[gx_max_color_value];
	  else
	    im = lut_rgb_color[tm];

          if (ty >= gx_max_color_value)
	    iy = lut_rgb_color[gx_max_color_value];
	  else
	    iy = lut_rgb_color[ty];

          ik = lut_rgb_color[ik];
        }

        switch (cups->header.cupsBitsPerColor)
        {
          case 1 :
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
  fprintf(stderr, "DEBUG: RGB %d,%d,%d = %08x\n", r, g, b, i);
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
  fprintf(stderr, "DEBUG: cups_open(%08x)\n", pdev);
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
  int		x, y;		/* Current position in image */
  unsigned char	*src,		/* Scanline data */
		*dst;		/* Bitmap data */


#ifdef DEBUG
  fprintf(stderr, "DEBUG: cups_print_pages(%08x, %08x, %d)\n", pdev, fp,
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
        cups->header.cupsBytesPerLine = (cups->header.cupsBitsPerPixel *
                                         cups->header.cupsWidth + 7) / 8 *
				        cups->color_info.num_components;
        break;

    case CUPS_ORDER_PLANAR :
        cups->header.cupsBytesPerLine = (cups->header.cupsBitsPerPixel *
	                                 cups->header.cupsWidth + 7) / 8;
        break;
  }

 /*
  * Compute the width of a scanline and allocate input/output buffers...
  */

  srcbytes = gdev_prn_raster(pdev);
  src       = (unsigned char *)gs_malloc(srcbytes, 1, "cups_print_pages");

  if (src == NULL)	/* can't allocate input buffer */
    return_error(gs_error_VMerror);

  if (cups->header.cupsColorOrder != CUPS_ORDER_CHUNKED)
  {
   /*
    * Need an output buffer, too...
    */

    dst = (unsigned char *)gs_malloc(cups->header.cupsBytesPerLine, 1,
                                     "cups_print_pages");

    if (dst == NULL)	/* can't allocate working area */
      return_error(gs_error_VMerror);
  }
  else
    dst = NULL;

 /*
  * See if the stream has been initialized yet...
  */

  if (cups->stream == NULL)
  {
    if (fp == NULL)
      cups->stream = cupsRasterOpen(1, CUPS_RASTER_WRITE);
    else
      cups->stream = cupsRasterOpen(fileno(fp), CUPS_RASTER_WRITE);

    if (cups->stream == NULL)
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
      cups_print_chunked(pdev, src);
    else
      switch (cups->header.cupsColorOrder)
      {
	case CUPS_ORDER_CHUNKED :
            cups_print_chunked(pdev, src);
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
  if (dst)
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
  int			olddepth;	/* Old depth value */
  int			code;		/* Error code */
  int			intval;		/* Integer value */
  bool			boolval;	/* Boolean value */
  float			floatval;	/* Floating point value */
  gs_param_string	stringval;	/* String value */
  gs_param_float_array	arrayval;	/* Float array value */


#ifdef DEBUG
  fprintf(stderr, "DEBUG: cups_put_params(%08x, %08x)\n", pdev, plist);
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
    cups->header.name = (type)intval;

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
  intoption(MediaPosition, "cupsMediaPosition", unsigned)
  floatoption(MediaWeight, "MediaWeight")
  booloption(MirrorPrint, "MirrorPrint")
  booloption(NegativePrint, "NegativePrint")
  intoption(NumCopies, "NumCopies", unsigned)
  intoption(Orientation, "Orientation", cups_orient_t)
  booloption(OutputFaceUp, "OutputFaceUp")
  booloption(Separations, "Separations")
  booloption(TraySwitch, "TraySwitch")
  booloption(Tumble, "Tumble")
  intoption(cupsWidth, "cupsWidth", unsigned)
  intoption(cupsHeight, "cupsHeight", unsigned)
  intoption(cupsMediaType, "cupsMediaType", unsigned)
  intoption(cupsBitsPerColor, "cupsBitsPerColor", unsigned)
  intoption(cupsBitsPerPixel, "cupsBitsPerPixel", unsigned)
  intoption(cupsBytesPerLine, "cupsBytesPerLine", unsigned)
  intoption(cupsColorOrder, "cupsColorOrder", cups_order_t)
  intoption(cupsColorSpace, "cupsColorSpace", cups_cspace_t)
  intoption(cupsCompression, "cupsCompression", unsigned)
  intoption(cupsRowCount, "cupsRowCount", unsigned)
  intoption(cupsRowFeed, "cupsRowFeed", unsigned)
  intoption(cupsRowStep, "cupsRowStep", unsigned)

 /*
  * Then process standard page device options...
  */

  if ((code = gdev_prn_put_params(pdev, plist)) < 0)
    return (code);

  cups->header.HWResolution[0] = pdev->HWResolution[0];
  cups->header.HWResolution[1] = pdev->HWResolution[1];

  cups->header.PageSize[0] = pdev->MediaSize[0];
  cups->header.PageSize[1] = pdev->MediaSize[1];

 /*
  * Check for a change in color depth...
  */

  olddepth = pdev->color_info.depth;
  cups_set_color_info(pdev);

  if (olddepth != pdev->color_info.depth && pdev->is_open)
    gs_closedevice(pdev);

 /*
  * Compute the page margins...
  */

  if (cups->ppd != NULL)
  {
   /*
    * Set the margins from the PPD file...
    */

    for (i = cups->ppd->num_sizes, size = cups->ppd->sizes;
         i > 0;
	 i --, size ++)
      if (size->width == cups->header.PageSize[0] &&
          size->length == cups->header.PageSize[1])
	break;

    if (i == 0)
    {
     /*
      * Pull margins from custom page size (0 or whatever is defined
      * by the PPD file...
      */

      margins[0] = cups->ppd->custom_margins[0] / 72.0;
      margins[1] = cups->ppd->custom_margins[1] / 72.0;
      margins[2] = cups->ppd->custom_margins[2] / 72.0;
      margins[3] = cups->ppd->custom_margins[3] / 72.0;
    }
    else
    {
     /*
      * Pull the margins from the size entry; since the margins are not
      * like the bounding box we have to adjust the top and right values
      * accordingly.
      */

      margins[0] = size->left / 72.0;
      margins[1] = size->bottom / 72.0;
      margins[2] = (size->width - size->right) / 72.0;
      margins[3] = (size->length - size->top) / 72.0;
    }
  }
  else
  {
   /*
    * Set default margins of 0.0...
    */

    memset(margins, 0, sizeof(margins));
  }

#ifdef DEBUG
  fprintf(stderr, "DEBUG: ppd = %08x\n", cups->ppd);
  fprintf(stderr, "DEBUG: MediaSize = [ %.3f %.3f ]\n",
          pdev->MediaSize[0], pdev->MediaSize[1]);
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

 /*
  * Set the margins and update the bitmap size...
  */

  gx_device_set_margins(pdev, margins, false);

  if ((code = gdev_prn_put_params(pdev, plist)) < 0)
    return (code);

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
  char		resolution[41];		/* Resolution string */
  ppd_profile_t	*profile;		/* Color profile information */


#ifdef DEBUG
  fprintf(stderr, "DEBUG: cups_set_color_info(%08x)\n", pdev);
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
	  cups->color_info.num_components = 6;
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

  if (cups->ppd != NULL && cups->header.cupsBitsPerColor == 8)
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

      for (i = 0; i < 3; i ++)
	for (j = 0; j < 3; j ++)
	  for (k = 0; k <= gx_max_color_value; k ++)
	  {
            cupsMatrix[i][j][k] = (int)((float)k * profile->matrix[i][j] + 0.5);

#ifdef DEBUG
            if ((k & 4095) == 0)
              fprintf(stderr, "DEBUG: cupsMatrix[%d][%d][%d] = %d\n",
	              i, j, k, cupsMatrix[i][j][k]);
#endif /* DEBUG */
          }

      d = profile->density;
      g = 1.0 / (profile->density * profile->density);

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
}


/*
 * 'cups_print_chunked()' - Print a page of chunked pixels.
 */

static void
cups_print_chunked(gx_device_printer *pdev,	/* I - Printer device */
                   unsigned char     *src)	/* I - Scanline buffer */
{
  int		y;				/* Looping var */
  unsigned char	*srcptr;			/* Pointer to data */


 /*
  * Loop through the page bitmap and write chunked pixels (the format
  * is identical to GhostScript's...
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

   /*
    * Write the scanline data to the raster stream...
    */

    cupsRasterWritePixels(cups->stream, srcptr, cups->header.cupsBytesPerLine);
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


 /*
  * Loop through the page bitmap and write banded pixels...  We have
  * to separate each chunked color as needed...
  */

  bandbytes = cups->header.cupsBytesPerLine / pdev->color_info.num_components;

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
      switch (cups->header.cupsBitsPerColor)
      {
	case 1 :
            memset(dst, 0, cups->header.cupsBytesPerLine);

            switch (cups->header.cupsColorSpace)
	    {
	      case CUPS_CSPACE_RGB :
	      case CUPS_CSPACE_CMY :
	      case CUPS_CSPACE_YMC :
	          for (x = cups->width, cptr = dst, mptr = cptr + bandbytes,
		           yptr = mptr + bandbytes, bit = 128;
		       x > 0;
		       x --, srcptr ++)
		  {
		    if (*srcptr & 0x40)
		      *cptr |= bit;
		    if (*srcptr & 0x20)
		      *mptr |= bit;
		    if (*srcptr & 0x10)
		      *yptr |= bit;

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

                    if (bit > 1)
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
	      case CUPS_CSPACE_CMYK :
	      case CUPS_CSPACE_YMCK :
	      case CUPS_CSPACE_KCMY :
	          for (x = cups->width, cptr = dst, mptr = cptr + bandbytes,
		           yptr = mptr + bandbytes, kptr = yptr + bandbytes,
			   bit = 128;
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

                    if (bit > 1)
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
	          for (x = cups->width, cptr = dst, mptr = cptr + bandbytes,
		           yptr = mptr + bandbytes, kptr = yptr + bandbytes,
			   lcptr = kptr + bandbytes, lmptr = lcptr + bandbytes,
			   bit = 128;
		       x > 0;
		       x --, srcptr ++)
		  {
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

                    if (bit > 1)
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
	      case CUPS_CSPACE_RGB :
	      case CUPS_CSPACE_CMY :
	      case CUPS_CSPACE_YMC :
	          for (x = cups->width, cptr = dst, mptr = cptr + bandbytes,
		           yptr = mptr + bandbytes, bit = 0xc0;
		       x > 0;
		       x --, srcptr ++)
		    switch (bit)
		    {
		      case 0xc0 :
			  if (temp = *srcptr & 0x30)
			    *cptr |= temp << 2;
			  if (temp = *srcptr & 0x0c)
			    *mptr |= temp << 4;
			  if (temp = *srcptr & 0x03)
			    *yptr |= temp << 6;

			  bit = 0x30;
			  break;
		      case 0x30 :
			  if (temp = *srcptr & 0x30)
			    *cptr |= temp;
			  if (temp = *srcptr & 0x0c)
			    *mptr |= temp << 2;
			  if (temp = *srcptr & 0x03)
			    *yptr |= temp << 4;

			  bit = 0x0c;
			  break;
		      case 0x0c :
			  if (temp = *srcptr & 0x30)
			    *cptr |= temp >> 2;
			  if (temp = *srcptr & 0x0c)
			    *mptr |= temp;
			  if (temp = *srcptr & 0x03)
			    *yptr |= temp << 2;

			  bit = 0x03;
			  break;
		      case 0x03 :
			  if (temp = *srcptr & 0x30)
			    *cptr |= temp >> 4;
			  if (temp = *srcptr & 0x0c)
			    *mptr |= temp >> 2;
			  if (temp = *srcptr & 0x03)
			    *yptr |= temp;

			  bit = 0xc0;
			  cptr ++;
			  mptr ++;
			  yptr ++;
			  break;
                    }
	          break;
	      case CUPS_CSPACE_CMYK :
	      case CUPS_CSPACE_YMCK :
	      case CUPS_CSPACE_KCMY :
	          for (x = cups->width, cptr = dst, mptr = cptr + bandbytes,
		           yptr = mptr + bandbytes, kptr = yptr + bandbytes,
			   bit = 0xc0;
		       x > 0;
		       x --, srcptr ++)
		    switch (bit)
		    {
		      case 0xc0 :
		          if (temp = *srcptr & 0xc0)
			    *cptr |= temp;
			  if (temp = *srcptr & 0x30)
			    *mptr |= temp << 2;
			  if (temp = *srcptr & 0x0c)
			    *yptr |= temp << 4;
			  if (temp = *srcptr & 0x03)
			    *kptr |= temp << 6;

			  bit = 0x30;
			  break;
		      case 0x30 :
		          if (temp = *srcptr & 0xc0)
			    *cptr |= temp >> 2;
			  if (temp = *srcptr & 0x30)
			    *mptr |= temp;
			  if (temp = *srcptr & 0x0c)
			    *yptr |= temp << 2;
			  if (temp = *srcptr & 0x03)
			    *kptr |= temp << 4;

			  bit = 0x0c;
			  break;
		      case 0x0c :
		          if (temp = *srcptr & 0xc0)
			    *cptr |= temp >> 4;
			  if (temp = *srcptr & 0x30)
			    *mptr |= temp >> 2;
			  if (temp = *srcptr & 0x0c)
			    *yptr |= temp;
			  if (temp = *srcptr & 0x03)
			    *kptr |= temp << 2;

			  bit = 0x03;
			  break;
		      case 0x03 :
		          if (temp = *srcptr & 0xc0)
			    *cptr |= temp >> 6;
			  if (temp = *srcptr & 0x30)
			    *mptr |= temp >> 4;
			  if (temp = *srcptr & 0x0c)
			    *yptr |= temp >> 2;
			  if (temp = *srcptr & 0x03)
			    *kptr |= temp;

			  bit = 0xc0;
			  cptr ++;
			  mptr ++;
			  yptr ++;
			  kptr ++;
			  break;
                    }
	          break;
	    }
            break;

	case 4 :
            memset(dst, 0, cups->header.cupsBytesPerLine);

            switch (cups->header.cupsColorSpace)
	    {
	      case CUPS_CSPACE_RGB :
	      case CUPS_CSPACE_CMY :
	      case CUPS_CSPACE_YMC :
	          for (x = cups->width, cptr = dst, mptr = cptr + bandbytes,
		           yptr = mptr + bandbytes, bit = 0xf0;
		       x > 0;
		       x --, srcptr += 2)
		    switch (bit)
		    {
		      case 0xf0 :
			  if (temp = srcptr[0] & 0x0f)
			    *cptr |= temp << 4;
			  if (temp = srcptr[1] & 0xf0)
			    *mptr |= temp;
			  if (temp = srcptr[1] & 0x0f)
			    *yptr |= temp << 4;

			  bit = 0x0f;
			  break;
		      case 0x0f :
			  if (temp = srcptr[0] & 0x0f)
			    *cptr |= temp;
			  if (temp = srcptr[1] & 0xf0)
			    *mptr |= temp >> 4;
			  if (temp = srcptr[1] & 0x0f)
			    *yptr |= temp;

			  bit = 0xf0;
			  cptr ++;
			  mptr ++;
			  yptr ++;
			  kptr ++;
			  break;
                    }
	          break;
	      case CUPS_CSPACE_CMYK :
	      case CUPS_CSPACE_YMCK :
	      case CUPS_CSPACE_KCMY :
	      case CUPS_CSPACE_KCMYcm :
	          for (x = cups->width, cptr = dst, mptr = cptr + bandbytes,
		           yptr = mptr + bandbytes, kptr = yptr + bandbytes,
			   bit = 0xf0;
		       x > 0;
		       x --, srcptr += 2)
		    switch (bit)
		    {
		      case 0xf0 :
		          if (temp = srcptr[0] & 0xf0)
			    *cptr |= temp;
			  if (temp = srcptr[0] & 0x0f)
			    *mptr |= temp << 4;
			  if (temp = srcptr[1] & 0xf0)
			    *yptr |= temp;
			  if (temp = srcptr[1] & 0x0f)
			    *kptr |= temp << 4;

			  bit = 0x0f;
			  break;
		      case 0x0f :
		          if (temp = srcptr[0] & 0xf0)
			    *cptr |= temp >> 4;
			  if (temp = srcptr[0] & 0x0f)
			    *mptr |= temp;
			  if (temp = srcptr[1] & 0xf0)
			    *yptr |= temp >> 4;
			  if (temp = srcptr[1] & 0x0f)
			    *kptr |= temp;

			  bit = 0xf0;
			  cptr ++;
			  mptr ++;
			  yptr ++;
			  kptr ++;
			  break;
                    }
	          break;
	    }
            break;

	case 8 :
            switch (cups->header.cupsColorSpace)
	    {
	      case CUPS_CSPACE_RGB :
	      case CUPS_CSPACE_CMY :
	      case CUPS_CSPACE_YMC :
	          for (x = cups->width, cptr = dst, mptr = cptr + bandbytes,
		           yptr = mptr + bandbytes;
		       x > 0;
		       x --)
		  {
		    *cptr++ = *srcptr++;
		    *mptr++ = *srcptr++;
		    *yptr++ = *srcptr++;
		  }
	          break;
	      case CUPS_CSPACE_CMYK :
	      case CUPS_CSPACE_YMCK :
	      case CUPS_CSPACE_KCMY :
	      case CUPS_CSPACE_KCMYcm :
	          for (x = cups->width, cptr = dst, mptr = cptr + bandbytes,
		           yptr = mptr + bandbytes, kptr = yptr + bandbytes;
		       x > 0;
		       x --)
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
          case 1 :
	      memset(dst, 0, cups->header.cupsBytesPerLine);

	      switch (cups->header.cupsColorSpace)
	      {
		case CUPS_CSPACE_RGB :
		case CUPS_CSPACE_CMY :
		case CUPS_CSPACE_YMC :
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
		case CUPS_CSPACE_RGB :
		case CUPS_CSPACE_CMY :
		case CUPS_CSPACE_YMC :
	            for (dstptr = dst, x = cups->width, srcbit = 48 >> (z * 2),
		             dstbit = 0xc0;
			 x > 0;
			 x --, srcptr ++)
		    {
		      if (temp = *srcptr & srcbit)
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
		case CUPS_CSPACE_CMYK :
		case CUPS_CSPACE_YMCK :
		case CUPS_CSPACE_KCMY :
		case CUPS_CSPACE_KCMYcm :
	            for (dstptr = dst, x = cups->width, srcbit = 192 >> (z * 2),
		             dstbit = 0xc0;
			 x > 0;
			 x --, srcptr ++)
		    {
		      if (temp = *srcptr & srcbit)
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
		case CUPS_CSPACE_RGB :
		case CUPS_CSPACE_CMY :
		case CUPS_CSPACE_YMC :
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
		      if (temp = *srcptr & srcbit)
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
		      if (temp = *srcptr & srcbit)
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
 * End of "$Id: gdevcups.c,v 1.8 1999/07/22 20:58:34 mike Exp $".
 */
