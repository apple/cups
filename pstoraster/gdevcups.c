/*
 * "$Id: gdevcups.c,v 1.43.2.23 2004/06/29 13:15:10 mike Exp $"
 *
 *   GNU Ghostscript raster output driver for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 1993-2004 by Easy Software Products.
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
 *       Hollywood, Maryland 20636-3142 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org/
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
 *   cups_get_space_params() - Get space parameters from the RIP_CACHE env var.
 *   cups_map_color_rgb()    - Map a color index to an RGB color.
 *   cups_map_cielab()       - Map CIE Lab transformation...
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
#include <ctype.h>
#include <cups/raster.h>
#include <cups/ppd.h>
#include <math.h>

#undef private
#define private


/*
 * Check if we are compiling against CUPS 1.2.  If so, enable
 * certain extended attributes and use a different page header
 * structure and write function...
 */

#ifdef CUPS_RASTER_SYNCv1
#  define cups_page_header_t cups_page_header2_t
#  define cupsRasterWriteHeader cupsRasterWriteHeader2
#endif /* CUPS_RASTER_SYNCv1 */


/*
 * Newer versions of Ghostscript don't provide gs_exit() function anymore.
 * It has been renamed to gs_to_exit()...
 */

#ifdef dev_t_proc_encode_color
#  define gs_exit gs_to_exit
#endif /* dev_t_proc_encode_color */


/*
 * CIE XYZ color constants...
 */

#define D65_X	(0.412453 + 0.357580 + 0.180423)
#define D65_Y	(0.212671 + 0.715160 + 0.072169)
#define D65_Z	(0.019334 + 0.119193 + 0.950227)


/*
 * Size of a tile in pixels...
 */

#define CUPS_TILE_SIZE	256


/*
 * Size of profile LUTs...
 */

#ifdef dev_t_proc_encode_color
#  define CUPS_MAX_VALUE	frac_1
#else
#  define CUPS_MAX_VALUE	gx_max_color_value
#endif /* dev_t_proc_encode_color */


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
private dev_proc_open_device(cups_open);
private int cups_print_pages(gx_device_printer *, FILE *, int);
private int cups_put_params(gx_device *, gs_param_list *);
private void cups_set_color_info(gx_device *);
private dev_proc_sync_output(cups_sync_output);
private prn_dev_proc_get_space_params(cups_get_space_params);

#ifdef dev_t_proc_encode_color
private cm_map_proc_gray(cups_map_gray);
private cm_map_proc_rgb(cups_map_rgb);
private cm_map_proc_cmyk(cups_map_cmyk);
private dev_proc_decode_color(cups_decode_color);
private dev_proc_encode_color(cups_encode_color);
private dev_proc_get_color_mapping_procs(cups_get_color_mapping_procs);

static const gx_cm_color_map_procs cups_color_mapping_procs =
{
  cups_map_gray,
  cups_map_rgb,
  cups_map_cmyk
};
#else
private dev_proc_map_cmyk_color(cups_map_cmyk_color);
private dev_proc_map_color_rgb(cups_map_color_rgb);
private dev_proc_map_rgb_color(cups_map_rgb_color);
#endif /* dev_t_proc_encode_color */


/*
 * The device descriptors...
 */

typedef struct gx_device_cups_s
{
  gx_device_common;			/* Standard GhostScript device stuff */
  gx_prn_device_common;			/* Standard printer device stuff */
  int			page;		/* Page number */
  cups_raster_t		*stream;	/* Raster stream */
  cups_page_header_t	header;		/* PostScript page device info */
  int			landscape;	/* Non-zero if this is landscape */
} gx_device_cups;

private gx_device_procs	cups_procs =
{
   cups_open,
   cups_get_matrix,
   cups_sync_output,
   gdev_prn_output_page,
   cups_close,
#ifdef dev_t_proc_encode_color
   NULL,				/* map_rgb_color */
   NULL,				/* map_color_rgb */
#else
   cups_map_rgb_color,
   cups_map_color_rgb,
#endif /* dev_t_proc_encode_color */
   NULL,				/* fill_rectangle */
   NULL,				/* tile_rectangle */
   NULL,				/* copy_mono */
   NULL,				/* copy_color */
   NULL,				/* draw_line */
   gx_default_get_bits,
   cups_get_params,
   cups_put_params,
#ifdef dev_t_proc_encode_color
   NULL,				/* map_cmyk_color */
#else
   cups_map_cmyk_color,
#endif /* dev_t_proc_encode_color */
   NULL,				/* get_xfont_procs */
   NULL,				/* get_xfont_device */
   NULL,				/* map_rgb_alpha_color */
   gx_page_device_get_page_device,
   NULL,				/* get_alpha_bits */
   NULL,				/* copy_alpha */
   NULL,				/* get_band */
   NULL,				/* copy_rop */
   NULL,				/* fill_path */
   NULL,				/* stroke_path */
   NULL,				/* fill_mask */
   NULL,				/* fill_trapezoid */
   NULL,				/* fill_parallelogram */
   NULL,				/* fill_triangle */
   NULL,				/* draw_thin_line */
   NULL,				/* begin_image */
   NULL,				/* image_data */
   NULL,				/* end_image */
   NULL,				/* strip_tile_rectangle */
   NULL					/* strip_copy_rop */
#ifdef dev_t_proc_encode_color
   ,
   NULL,				/* get_clipping_box */
   NULL,				/* begin_typed_image */
   NULL,				/* get_bits_rectangle */
   NULL,				/* map_color_rgb_alpha */
   NULL,				/* create_compositor */
   NULL,				/* get_hardware_params */
   NULL,				/* text_begin */
   NULL,				/* finish_copydevice */
   NULL,				/* begin_transparency_group */
   NULL,				/* end_transparency_group */
   NULL,				/* begin_transparency_mask */
   NULL,				/* end_transparency_mask */
   NULL,				/* discard_transparency_layer */
   cups_get_color_mapping_procs,
   NULL,				/* get_color_comp_index */
   cups_encode_color,
   cups_decode_color
#endif /* dev_t_proc_encode_color */
};

#define prn_device_body_copies(dtype, procs, dname, w10, h10, xdpi, ydpi, lo, to, lm, bm, rm, tm, ncomp, depth, mg, mc, dg, dc, print_pages)\
	std_device_full_body_type(dtype, &procs, dname, &st_device_printer,\
	  (int)((long)(w10) * (xdpi) / 10),\
	  (int)((long)(h10) * (ydpi) / 10),\
	  xdpi, ydpi,\
	  ncomp, depth, mg, mc, dg, dc,\
	  -(lo) * (xdpi), -(to) * (ydpi),\
	  (lm) * 72.0, (bm) * 72.0,\
	  (rm) * 72.0, (tm) * 72.0\
	),\
	prn_device_body_copies_rest_(print_pages)

gx_device_cups	gs_cups_device =
{
  prn_device_body_copies(gx_device_cups,/* type */
                         cups_procs,	/* procedures */
			 "cups",	/* device name */
			 85,		/* initial width */
			 110,		/* initial height */
			 100,		/* initial x resolution */
			 100,		/* initial y resolution */
                         0,		/* initial left offset */
			 0,		/* initial top offset */
			 0,		/* initial left margin */
			 0,		/* initial bottom margin */
			 0,		/* initial right margin */
			 0,		/* initial top margin */
			 1,		/* number of color components */
			 1,		/* number of color bits */
			 1,		/* maximum gray value */
			 0,		/* maximum color value */
			 2,		/* number of gray values */
			 0,		/* number of color values */
			 cups_print_pages),
					/* print procedure */
  0,					/* page */
  NULL,					/* stream */
  {					/* header */
    "",					/* MediaClass */
    "",					/* MediaColor */
    "",					/* MediaType */
    "",					/* OutputType */
    0,					/* AdvanceDistance */
    CUPS_ADVANCE_NONE,			/* AdvanceMedia */
    CUPS_FALSE,				/* Collate */
    CUPS_CUT_NONE,			/* CutMedia */
    CUPS_FALSE,				/* Duplex */
    { 100, 100 },			/* HWResolution */
    { 0, 0, 612, 792 },			/* ImagingBoundingBox */
    CUPS_FALSE,				/* InsertSheet */
    CUPS_JOG_NONE,			/* Jog */
    CUPS_EDGE_TOP,			/* LeadingEdge */
    { 0, 0 },				/* Margins */
    CUPS_FALSE,				/* ManualFeed */
    0,					/* MediaPosition */
    0,					/* MediaWeight */
    CUPS_FALSE,				/* MirrorPrint */
    CUPS_FALSE,				/* NegativePrint */
    1,					/* NumCopies */
    CUPS_ORIENT_0,			/* Orientation */
    CUPS_FALSE,				/* OutputFaceUp */
    { 612, 792 },			/* PageSize */
    CUPS_FALSE,				/* Separations */
    CUPS_FALSE,				/* TraySwitch */
    CUPS_FALSE,				/* Tumble */
    850,				/* cupsWidth */
    1100,				/* cupsHeight */
    0,					/* cupsMediaType */
    1,					/* cupsBitsPerColor */
    1,					/* cupsBitsPerPixel */
    107,				/* cupsBytesPerLine */
    CUPS_ORDER_CHUNKED,			/* cupsColorOrder */
    CUPS_CSPACE_K,			/* cupsColorSpace */
    0,					/* cupsCompression */
    0,					/* cupsRowCount */
    0,					/* cupsRowFeed */
    0					/* cupsRowStep */
  }
};

/*
 * Globals...
 */

static gx_color_value	cupsDecodeLUT[256];
					/* Output color to RGB value LUT */
static unsigned char	cupsEncodeLUT[gx_max_color_value + 1];
					/* RGB value to output color LUT */

static ppd_file_t	*cupsPPD = 0;	/* PPD file for this device */
static char		*cupsProfile = NULL;
					/* Current simple color profile string */
static int		cupsHaveProfile = 0;
					/* Has a color profile been defined? */
static int		cupsMatrix[3][3][CUPS_MAX_VALUE + 1];
					/* Color transform matrix LUT */
static int		cupsDensity[CUPS_MAX_VALUE + 1];
					/* Density LUT */
static unsigned char	cupsRevLower1[16] =
			{		/* Lower 1-bit reversal table */
			  0x00, 0x08, 0x04, 0x0c, 0x02, 0x0a, 0x06, 0x0e,
			  0x01, 0x09, 0x05, 0x0d, 0x03, 0x0b, 0x07, 0x0f
			},
			cupsRevUpper1[16] =
			{		/* Upper 1-bit reversal table */
			  0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
			  0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0
			},
			cupsRevLower2[16] =
			{		/* Lower 2-bit reversal table */
			  0x00, 0x04, 0x08, 0x0c, 0x01, 0x05, 0x09, 0x0d,
			  0x02, 0x06, 0x0a, 0x0e, 0x03, 0x07, 0x0b, 0x0f
			},
			cupsRevUpper2[16] =
			{		/* Upper 2-bit reversal table */
			  0x00, 0x40, 0x80, 0xc0, 0x10, 0x50, 0x90, 0xd0,
			  0x20, 0x60, 0xa0, 0xe0, 0x30, 0x70, 0xb0, 0xf0
			};


/*
 * Local functions...
 */

static double	cups_map_cielab(double, double);
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
cups_close(gx_device *pdev)		/* I - Device info */
{
  fprintf(stderr, "DEBUG2: cups_close(%p)\n", pdev);

  if (cups->stream != NULL)
  {
    cupsRasterClose(cups->stream);
    cups->stream = NULL;
  }

#if 0 /* Can't do this here because put_params() might close the device */
  if (cupsPPD != NULL)
  {
    ppdClose(cupsPPD);
    cupsPPD = NULL;
  }

  if (cupsProfile != NULL)
  {
    free(cupsProfile);
    cupsProfile = NULL;
  }
#endif /* 0 */

  return (gdev_prn_close(pdev));
}


#ifdef dev_t_proc_encode_color
/*
 * 'cups_decode_color()' - Decode a color value.
 */

private int				/* O - Status (0 = OK) */
cups_decode_color(gx_device      *pdev,	/* I - Device info */
                  gx_color_index ci,	/* I - Color index */
                  gx_color_value *cv)	/* O - Colors */
{
  int			i;		/* Looping var */
  int			shift;		/* Bits to shift */
  int			mask;		/* Bits to mask */


  if (cups->header.cupsColorSpace == CUPS_CSPACE_KCMYcm &&
      cups->header.cupsBitsPerColor == 1)
  {
   /*
    * KCMYcm data is represented internally by Ghostscript as CMYK...
    */

    cv[0] = (ci & 0x20) ? frac_1 : frac_0;
    cv[1] = (ci & 0x12) ? frac_1 : frac_0;
    cv[2] = (ci & 0x09) ? frac_1 : frac_0;
    cv[3] = (ci & 0x04) ? frac_1 : frac_0;
  }
  else
  {
    shift = cups->header.cupsBitsPerColor;
    mask  = (1 << shift) - 1;

    for (i = cups->color_info.num_components - 1; i > 0; i --, ci >>= shift)
      cv[i] = cupsDecodeLUT[ci & mask];

    cv[0] = cupsDecodeLUT[ci & mask];
  }

  return (0);
}


/*
 * 'cups_encode_color()' - Encode a color value.
 */

private gx_color_index			/* O - Color index */
cups_encode_color(gx_device            *pdev,
					/* I - Device info */
                  const gx_color_value *cv)
					/* I - Colors */
{
  int			i;		/* Looping var */
  gx_color_index	ci;		/* Color index */
  int			shift;		/* Bits to shift */


 /*
  * Encode the color index...
  */

  shift = cups->header.cupsBitsPerColor;

  for (ci = cupsEncodeLUT[cv[0]], i = 1;
       i < cups->color_info.num_components;
       i ++)
    ci = (ci << shift) | cupsEncodeLUT[cv[i]];

 /*
  * Handle 6-color output...
  */

  if (cups->header.cupsColorSpace == CUPS_CSPACE_KCMYcm &&
      cups->header.cupsBitsPerColor == 1)
  {
   /*
    * Welcome to hackville, where we map CMYK data to the
    * light inks in draft mode...  Map blue to light magenta and
    * cyan and green to light cyan and yellow...
    */

    ci <<= 2;				/* Leave room for light inks */

    if (ci == 0x18)			/* Blue */
      ci = 0x11;			/* == cyan + light magenta */
    else if (ci == 0x14)		/* Green */
      ci = 0x06;			/* == light cyan + yellow */
  }

 /*
  * Range check the return value...
  */

  if (ci == gx_no_color_index)
    ci --;

 /*
  * Return the color index...
  */

  return (ci);
}


/*
 * 'cups_get_color_mapping_procs()' - Get the list of color mapping procedures.
 */

private const gx_cm_color_map_procs *	/* O - List of device procedures */
cups_get_color_mapping_procs(const gx_device *pdev)
					/* I - Device info */
{
  return (&cups_color_mapping_procs);
}
#endif /* dev_t_proc_encode_color */


/*
 * 'cups_get_matrix()' - Generate the default page matrix.
 */

private void
cups_get_matrix(gx_device *pdev,	/* I - Device info */
                gs_matrix *pmat)	/* O - Physical transform matrix */
{
  fprintf(stderr, "DEBUG2: cups_get_matrix(%p, %p)\n", pdev, pmat);

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

  if (cupsPPD)
  {
    fprintf(stderr, "DEBUG: cupsPPD = %p\n", cupsPPD);
    fprintf(stderr, "DEBUG: cupsPPD->flip_duplex = %d\n", cupsPPD->flip_duplex);
  }

  if (cups->landscape)
  {
   /*
    * Do landscape orientation...
    */

    if (cups->header.Duplex && !cups->header.Tumble &&
	cupsPPD && cupsPPD->flip_duplex && !(cups->page & 1))
    {
      pmat->xx = 0.0;
      pmat->xy = (float)cups->header.HWResolution[0] / 72.0;
      pmat->yx = -(float)cups->header.HWResolution[1] / 72.0;
      pmat->yy = 0.0;
      pmat->tx = -(float)cups->header.HWResolution[0] * pdev->HWMargins[2] / 72.0;
      pmat->ty = (float)cups->header.HWResolution[1] *
                 ((float)cups->header.PageSize[0] - pdev->HWMargins[3]) / 72.0;
    }
    else
    {
      pmat->xx = 0.0;
      pmat->xy = (float)cups->header.HWResolution[0] / 72.0;
      pmat->yx = (float)cups->header.HWResolution[1] / 72.0;
      pmat->yy = 0.0;
      pmat->tx = -(float)cups->header.HWResolution[0] * pdev->HWMargins[0] / 72.0;
      pmat->ty = -(float)cups->header.HWResolution[1] * pdev->HWMargins[1] / 72.0;
    }
  }
  else if (cups->header.Duplex && !cups->header.Tumble &&
           cupsPPD && cupsPPD->flip_duplex && !(cups->page & 1))
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
#ifdef CUPS_RASTER_SYNCv1
  int			i;		/* Looping var */
  char			name[255];	/* Attribute name */
#endif /* CUPS_RASTER_SYNCv1 */
  int			code;		/* Return code */
  gs_param_string	s;		/* Temporary string value */
  bool			b;		/* Temporary boolean value */


  fprintf(stderr, "DEBUG2: cups_get_params(%p, %p)\n", pdev, plist);

 /*
  * First process the "standard" page device parameters...
  */

  fputs("DEBUG2: before gdev_prn_get_params()\n", stderr);

  if ((code = gdev_prn_get_params(pdev, plist)) < 0)
    return (code);

  fputs("DEBUG2: after gdev_prn_get_params()\n", stderr);

 /*
  * Then write the CUPS parameters...
  */

  fputs("DEBUG2: Adding MediaClass\n", stderr);

  param_string_from_string(s, cups->header.MediaClass);
  if ((code = param_write_string(plist, "MediaClass", &s)) < 0)
    return (code);

  fputs("DEBUG2: Adding AdvanceDistance\n", stderr);

  if ((code = param_write_int(plist, "AdvanceDistance",
                              (int *)&(cups->header.AdvanceDistance))) < 0)
    return (code);

  fputs("DEBUG2: Adding AdvanceDistance\n", stderr);

  if ((code = param_write_int(plist, "AdvanceMedia",
                              (int *)&(cups->header.AdvanceMedia))) < 0)
    return (code);

  fputs("DEBUG2: Adding Collate\n", stderr);

  b = cups->header.Collate;
  if ((code = param_write_bool(plist, "Collate", &b)) < 0)
    return (code);

  fputs("DEBUG2: Adding CutMedia\n", stderr);

  if ((code = param_write_int(plist, "CutMedia",
                              (int *)&(cups->header.CutMedia))) < 0)
    return (code);

  fputs("DEBUG2: Adding InsertSheet\n", stderr);

  b = cups->header.InsertSheet;
  if ((code = param_write_bool(plist, "InsertSheet", &b)) < 0)
    return (code);

  fputs("DEBUG2: Adding Jog\n", stderr);

  if ((code = param_write_int(plist, "Jog",
                              (int *)&(cups->header.Jog))) < 0)
    return (code);

  fputs("DEBUG2: Adding LeadingEdge\n", stderr);

  if ((code = param_write_int(plist, "LeadingEdge",
                              (int *)&(cups->header.LeadingEdge))) < 0)
    return (code);

  fputs("DEBUG2: Adding ManualFeed\n", stderr);

  b = cups->header.ManualFeed;
  if ((code = param_write_bool(plist, "ManualFeed", &b)) < 0)
    return (code);

  fputs("DEBUG2: Adding MediaPosition\n", stderr);

  if ((code = param_write_int(plist, "MediaPosition",
                              (int *)&(cups->header.MediaPosition))) < 0)
    return (code);

  fputs("DEBUG2: Adding MirrorPrint\n", stderr);

  b = cups->header.MirrorPrint;
  if ((code = param_write_bool(plist, "MirrorPrint", &b)) < 0)
    return (code);

  fputs("DEBUG2: Adding NegativePrint\n", stderr);

  b = cups->header.NegativePrint;
  if ((code = param_write_bool(plist, "NegativePrint", &b)) < 0)
    return (code);

  fputs("DEBUG2: Adding OutputFaceUp\n", stderr);

  b = cups->header.OutputFaceUp;
  if ((code = param_write_bool(plist, "OutputFaceUp", &b)) < 0)
    return (code);

  fputs("DEBUG2: Adding Separations\n", stderr);

  b = cups->header.Separations;
  if ((code = param_write_bool(plist, "Separations", &b)) < 0)
    return (code);

  fputs("DEBUG2: Adding TraySwitch\n", stderr);

  b = cups->header.TraySwitch;
  if ((code = param_write_bool(plist, "TraySwitch", &b)) < 0)
    return (code);

  fputs("DEBUG2: Adding Tumble\n", stderr);

  b = cups->header.Tumble;
  if ((code = param_write_bool(plist, "Tumble", &b)) < 0)
    return (code);

  fputs("DEBUG2: Adding cupsWidth\n", stderr);

  if ((code = param_write_int(plist, "cupsWidth",
                              (int *)&(cups->header.cupsWidth))) < 0)
    return (code);

  fputs("DEBUG2: Adding cupsHeight\n", stderr);

  if ((code = param_write_int(plist, "cupsHeight",
                              (int *)&(cups->header.cupsHeight))) < 0)
    return (code);

  fputs("DEBUG2: Adding cupsMediaType\n", stderr);

  if ((code = param_write_int(plist, "cupsMediaType",
                              (int *)&(cups->header.cupsMediaType))) < 0)
    return (code);

  fputs("DEBUG2: Adding cupsBitsPerColor\n", stderr);

  if ((code = param_write_int(plist, "cupsBitsPerColor",
                              (int *)&(cups->header.cupsBitsPerColor))) < 0)
    return (code);

  fputs("DEBUG2: Adding cupsBitsPerPixel\n", stderr);

  if ((code = param_write_int(plist, "cupsBitsPerPixel",
                              (int *)&(cups->header.cupsBitsPerPixel))) < 0)
    return (code);

  fputs("DEBUG2: Adding cupsBytesPerLine\n", stderr);

  if ((code = param_write_int(plist, "cupsBytesPerLine",
                              (int *)&(cups->header.cupsBytesPerLine))) < 0)
    return (code);

  fputs("DEBUG2: Adding cupsColorOrder\n", stderr);

  if ((code = param_write_int(plist, "cupsColorOrder",
                              (int *)&(cups->header.cupsColorOrder))) < 0)
    return (code);

  fputs("DEBUG2: Adding cupsColorSpace\n", stderr);

  if ((code = param_write_int(plist, "cupsColorSpace",
                              (int *)&(cups->header.cupsColorSpace))) < 0)
    return (code);

  fputs("DEBUG2: Adding cupsCompression\n", stderr);

  if ((code = param_write_int(plist, "cupsCompression",
                              (int *)&(cups->header.cupsCompression))) < 0)
    return (code);

  fputs("DEBUG2: Adding cupsRowCount\n", stderr);

  if ((code = param_write_int(plist, "cupsRowCount",
                              (int *)&(cups->header.cupsRowCount))) < 0)
    return (code);

  fputs("DEBUG2: Adding cupsRowFeed\n", stderr);

  if ((code = param_write_int(plist, "cupsRowFeed",
                              (int *)&(cups->header.cupsRowFeed))) < 0)
    return (code);

  fputs("DEBUG2: Adding cupsRowStep\n", stderr);

  if ((code = param_write_int(plist, "cupsRowStep",
                              (int *)&(cups->header.cupsRowStep))) < 0)
    return (code);

#ifdef CUPS_RASTER_SYNCv1
  fputs("DEBUG2: Adding cupsNumColors\n", stderr);

  if ((code = param_write_int(plist, "cupsNumColors",
                              (int *)&(cups->header.cupsNumColors))) < 0)
    return (code);

  fputs("DEBUG2: Adding cupsInteger\n", stderr);

  for (i = 0; i < 16; i ++)
  {
    sprintf(name, "cupsInteger%d", i);
    if ((code = param_write_int(plist, name,
                        	(int *)(cups->header.cupsInteger + i))) < 0)
      return (code);
  }

  fputs("DEBUG2: Adding cupsReal\n", stderr);

  for (i = 0; i < 16; i ++)
  {
    sprintf(name, "cupsReal%d", i);
    if ((code = param_write_float(plist, name,
                        	  cups->header.cupsReal + i)) < 0)
      return (code);
  }

  fputs("DEBUG2: Adding cupsString\n", stderr);

  for (i = 0; i < 16; i ++)
  {
    sprintf(name, "cupsReal%d", i);
    param_string_from_string(s, cups->header.cupsString[i]);
    if ((code = param_write_string(plist, name, &s)) < 0)
      return (code);
  }

  fputs("DEBUG2: Adding cupsMarkerType\n", stderr);

  param_string_from_string(s, cups->header.cupsMarkerType);
  if ((code = param_write_string(plist, "cupsMarkerType", &s)) < 0)
    return (code);

  fputs("DEBUG2: Adding cupsRenderingIntent\n", stderr);

  param_string_from_string(s, cups->header.cupsRenderingIntent);
  if ((code = param_write_string(plist, "cupsRenderingIntent", &s)) < 0)
    return (code);
#endif /* CUPS_RASTER_SYNCv1 */

  fputs("DEBUG2: Leaving cups_get_params()\n", stderr);

  return (0);
}


/*
 * 'cups_get_space_params()' - Get space parameters from the RIP_CACHE env var.
 */

void
cups_get_space_params(const gx_device_printer *pdev,
					/* I - Printer device */
                      gdev_prn_space_params   *space_params)
					/* O - Space parameters */
{
  float	cache_size;			/* Size of tile cache in bytes */
  char	*cache_env,			/* Cache size environment variable */
	cache_units[255];		/* Cache size units */


  fprintf(stderr, "DEBUG2: cups_get_space_params(%p, %p)\n", pdev, space_params);

  if ((cache_env = getenv("RIP_MAX_CACHE")) != NULL)
  {
    switch (sscanf(cache_env, "%f%254s", &cache_size, cache_units))
    {
      case 0 :
          cache_size = 8 * 1024 * 1024;
	  break;
      case 1 :
          cache_size *= 4 * CUPS_TILE_SIZE * CUPS_TILE_SIZE;
	  break;
      case 2 :
          if (tolower(cache_units[0]) == 'g')
	    cache_size *= 1024 * 1024 * 1024;
          else if (tolower(cache_units[0]) == 'm')
	    cache_size *= 1024 * 1024;
	  else if (tolower(cache_units[0]) == 'k')
	    cache_size *= 1024;
	  else if (tolower(cache_units[0]) == 't')
	    cache_size *= 4 * CUPS_TILE_SIZE * CUPS_TILE_SIZE;
	  break;
    }
  }
  else
    cache_size = 8 * 1024 * 1024;

  fprintf(stderr, "DEBUG: cache_size = %.0f\n", cache_size);

  space_params->MaxBitmap   = (int)cache_size;
  space_params->BufferSpace = (int)cache_size / 10;
}


/*
 * 'cups_map_cielab()' - Map CIE Lab transformation...
 */

static double				/* O - Adjusted color value */
cups_map_cielab(double x,		/* I - Raw color value */
                double xn)		/* I - Whitepoint color value */
{
  double x_xn;				/* Fraction of whitepoint */


  x_xn = x / xn;

  if (x_xn > 0.008856)
    return (cbrt(x_xn));
  else
    return (7.787 * x_xn + 16.0 / 116.0);
}


#ifdef dev_t_proc_encode_color
/*
 * 'cups_map_cmyk()' - Map a CMYK color value to device colors.
 */

private void
cups_map_cmyk(gx_device *pdev,		/* I - Device info */
              frac      c,		/* I - Cyan value */
	      frac      m,		/* I - Magenta value */
	      frac      y,		/* I - Yellow value */
	      frac      k,		/* I - Black value */
	      frac      *out)		/* O - Device colors */
{
  int	c0, c1, c2;			/* Temporary color values */
  float	rr, rg, rb,			/* Real RGB colors */
	ciex, ciey, ciez,		/* CIE XYZ colors */
	ciey_yn,			/* Normalized luminance */
	ciel, ciea, cieb;		/* CIE Lab colors */


  fprintf(stderr, "DEBUG2: cups_map_cmyk(%p, %d, %d, %d, %d, %p)\n",
          pdev, c, m, y, k, out);

 /*
  * Convert the CMYK color to the destination colorspace...
  */

  switch (cups->header.cupsColorSpace)
  {
    case CUPS_CSPACE_W :
        c0 = frac_1 - (c * 31 + m * 61 + y * 8) / 100 - k;

	if (c0 < 0)
	  out[0] = 0;
	else if (c0 > frac_1)
	  out[0] = (frac)cupsDensity[frac_1];
	else
	  out[0] = (frac)cupsDensity[c0];
        break;

    case CUPS_CSPACE_RGBA :
        out[3] = frac_1;

    case CUPS_CSPACE_RGB :
        c0 = frac_1 - c - k;
	c1 = frac_1 - m - k;
	c2 = frac_1 - y - k;

        if (c0 < 0)
	  out[0] = 0;
	else if (c0 > frac_1)
	  out[0] = (frac)cupsDensity[frac_1];
	else
	  out[0] = (frac)cupsDensity[c0];

        if (c1 < 0)
	  out[1] = 0;
	else if (c1 > frac_1)
	  out[1] = (frac)cupsDensity[frac_1];
	else
	  out[1] = (frac)cupsDensity[c1];

        if (c2 < 0)
	  out[2] = 0;
	else if (c2 > frac_1)
	  out[2] = (frac)cupsDensity[frac_1];
	else
	  out[2] = (frac)cupsDensity[c2];
        break;

    default :
    case CUPS_CSPACE_K :
        c0 = (c * 31 + m * 61 + y * 8) / 100 + k;

	if (c0 < 0)
	  out[0] = 0;
	else if (c0 > frac_1)
	  out[0] = (frac)cupsDensity[frac_1];
	else
	  out[0] = (frac)cupsDensity[c0];
        break;

    case CUPS_CSPACE_CMY :
        c0 = c + k;
	c1 = m + k;
	c2 = y + k;

        if (c0 < 0)
	  out[0] = 0;
	else if (c0 > frac_1)
	  out[0] = (frac)cupsDensity[frac_1];
	else
	  out[0] = (frac)cupsDensity[c0];

        if (c1 < 0)
	  out[1] = 0;
	else if (c1 > frac_1)
	  out[1] = (frac)cupsDensity[frac_1];
	else
	  out[1] = (frac)cupsDensity[c1];

        if (c2 < 0)
	  out[2] = 0;
	else if (c2 > frac_1)
	  out[2] = (frac)cupsDensity[frac_1];
	else
	  out[2] = (frac)cupsDensity[c2];
        break;

    case CUPS_CSPACE_YMC :
        c0 = y + k;
	c1 = m + k;
	c2 = c + k;

        if (c0 < 0)
	  out[0] = 0;
	else if (c0 > frac_1)
	  out[0] = (frac)cupsDensity[frac_1];
	else
	  out[0] = (frac)cupsDensity[c0];

        if (c1 < 0)
	  out[1] = 0;
	else if (c1 > frac_1)
	  out[1] = (frac)cupsDensity[frac_1];
	else
	  out[1] = (frac)cupsDensity[c1];

        if (c2 < 0)
	  out[2] = 0;
	else if (c2 > frac_1)
	  out[2] = (frac)cupsDensity[frac_1];
	else
	  out[2] = (frac)cupsDensity[c2];
        break;

    case CUPS_CSPACE_CMYK :
        if (c < 0)
	  out[0] = 0;
	else if (c > frac_1)
	  out[0] = (frac)cupsDensity[frac_1];
	else
	  out[0] = (frac)cupsDensity[c];

        if (m < 0)
	  out[1] = 0;
	else if (m > frac_1)
	  out[1] = (frac)cupsDensity[frac_1];
	else
	  out[1] = (frac)cupsDensity[m];

        if (y < 0)
	  out[2] = 0;
	else if (y > frac_1)
	  out[2] = (frac)cupsDensity[frac_1];
	else
	  out[2] = (frac)cupsDensity[y];

        if (k < 0)
	  out[3] = 0;
	else if (k > frac_1)
	  out[3] = (frac)cupsDensity[frac_1];
	else
	  out[3] = (frac)cupsDensity[k];
        break;

    case CUPS_CSPACE_YMCK :
    case CUPS_CSPACE_GMCK :
    case CUPS_CSPACE_GMCS :
        if (y < 0)
	  out[0] = 0;
	else if (y > frac_1)
	  out[0] = (frac)cupsDensity[frac_1];
	else
	  out[0] = (frac)cupsDensity[y];

        if (m < 0)
	  out[1] = 0;
	else if (m > frac_1)
	  out[1] = (frac)cupsDensity[frac_1];
	else
	  out[1] = (frac)cupsDensity[m];

        if (c < 0)
	  out[2] = 0;
	else if (c > frac_1)
	  out[2] = (frac)cupsDensity[frac_1];
	else
	  out[2] = (frac)cupsDensity[c];

        if (k < 0)
	  out[3] = 0;
	else if (k > frac_1)
	  out[3] = (frac)cupsDensity[frac_1];
	else
	  out[3] = (frac)cupsDensity[k];
        break;

    case CUPS_CSPACE_KCMYcm :
    case CUPS_CSPACE_KCMY :
        if (k < 0)
	  out[0] = 0;
	else if (k > frac_1)
	  out[0] = (frac)cupsDensity[frac_1];
	else
	  out[0] = (frac)cupsDensity[k];

        if (c < 0)
	  out[1] = 0;
	else if (c > frac_1)
	  out[1] = (frac)cupsDensity[frac_1];
	else
	  out[1] = (frac)cupsDensity[c];

        if (m < 0)
	  out[2] = 0;
	else if (m > frac_1)
	  out[2] = (frac)cupsDensity[frac_1];
	else
	  out[2] = (frac)cupsDensity[m];

        if (y < 0)
	  out[3] = 0;
	else if (y > frac_1)
	  out[3] = (frac)cupsDensity[frac_1];
	else
	  out[3] = (frac)cupsDensity[y];
        break;

#  ifdef CUPS_RASTER_HAVE_COLORIMETRIC
    case CUPS_CSPACE_CIEXYZ :
    case CUPS_CSPACE_CIELab :
    case CUPS_CSPACE_ICC1 :
    case CUPS_CSPACE_ICC2 :
    case CUPS_CSPACE_ICC3 :
    case CUPS_CSPACE_ICC4 :
    case CUPS_CSPACE_ICC5 :
    case CUPS_CSPACE_ICC6 :
    case CUPS_CSPACE_ICC7 :
    case CUPS_CSPACE_ICC8 :
    case CUPS_CSPACE_ICC9 :
    case CUPS_CSPACE_ICCA :
    case CUPS_CSPACE_ICCB :
    case CUPS_CSPACE_ICCC :
    case CUPS_CSPACE_ICCD :
    case CUPS_CSPACE_ICCE :
    case CUPS_CSPACE_ICCF :
       /*
        * Convert CMYK to sRGB...
	*/

        c0 = frac_1 - c - k;
	c1 = frac_1 - m - k;
	c2 = frac_1 - y - k;

        if (c0 < 0)
	  c0 = 0;
	else if (c0 > frac_1)
	  c0 = frac_1;

        if (c1 < 0)
	  c1 = 0;
	else if (c1 > frac_1)
	  c1 = frac_1;

        if (c2 < 0)
	  c2 = 0;
	else if (c2 > frac_1)
	  c2 = frac_1;

       /*
        * Convert sRGB to linear RGB...
	*/

	rr = pow((double)c0 / (double)frac_1, 0.58823529412);
	rg = pow((double)c1 / (double)frac_1, 0.58823529412);
	rb = pow((double)c2 / (double)frac_1, 0.58823529412);

       /*
        * Convert to CIE XYZ...
	*/

	ciex = 0.412453 * rr + 0.357580 * rg + 0.180423 * rb;
	ciey = 0.212671 * rr + 0.715160 * rg + 0.072169 * rb;
	ciez = 0.019334 * rr + 0.119193 * rg + 0.950227 * rb;

        if (cups->header.cupsColorSpace == CUPS_CSPACE_CIEXYZ)
	{
	 /*
	  * Convert to an integer XYZ color value...
	  */

          if (ciex > 1.0)
	    c0 = frac_1;
	  else if (ciex > 0.0)
	    c0 = (int)(ciex * frac_1);
	  else
	    c0 = 0;

          if (ciey > 1.0)
	    c1 = frac_1;
	  else if (ciey > 0.0)
	    c1 = (int)(ciey * frac_1);
	  else
	    c1 = 0;

          if (ciez > 1.0)
	    c2 = frac_1;
	  else if (ciez > 0.0)
	    c2 = (int)(ciez * frac_1);
	  else
	    c2 = 0;
	}
	else
	{
	 /*
	  * Convert CIE XYZ to Lab...
	  */

	  ciey_yn = ciey / D65_Y;

	  if (ciey_yn > 0.008856)
	    ciel = 116 * cbrt(ciey_yn) - 16;
	  else
	    ciel = 903.3 * ciey_yn;

	  ciea = 500 * (cups_map_cielab(ciex, D65_X) -
	                cups_map_cielab(ciey, D65_Y));
	  cieb = 200 * (cups_map_cielab(ciey, D65_Y) -
	                cups_map_cielab(ciez, D65_Z));

         /*
	  * Scale the L value and bias the a and b values by 128
	  * so that all values are in the range of 0 to 255.
	  */

	  ciel *= 2.55;
	  ciea += 128;
	  cieb += 128;

         /*
	  * Convert to frac values...
	  */

          if (ciel < 0.0)
	    c0 = 0;
	  else if (ciel < 255.0)
	    c0 = (int)(ciel * frac_1 / 255.0);
	  else
	    c0 = frac_1;

          if (ciea < 0.0)
	    c1 = 0;
	  else if (ciea < 255.0)
	    c1 = (int)(ciea * frac_1 / 255.0);
	  else
	    c1 = frac_1;

          if (cieb < 0.0)
	    c2 = 0;
	  else if (cieb < 255.0)
	    c2 = (int)(cieb * frac_1 / 255.0);
	  else
	    c2 = frac_1;
	}

       /*
        * Put the final color value together...
	*/

        out[0] = c0;
	out[1] = c1;
	out[2] = c2;
        break;
#  endif /* CUPS_RASTER_HAVE_COLORIMETRIC */
  }

  switch (cups->color_info.num_components)
  {
    default :
    case 1 :
        fprintf(stderr, "DEBUG2:   \\=== COLOR %d\n", out[0]);
	break;

    case 3 :
        fprintf(stderr, "DEBUG2:   \\=== COLOR %d, %d, %d\n",
	        out[0], out[1], out[2]);
	break;

    case 4 :
        fprintf(stderr, "DEBUG2:   \\=== COLOR %d, %d, %d, %d\n",
	        out[0], out[1], out[2], out[3]);
	break;
  }
}


/*
 * 'cups_map_gray()' - Map a grayscale value to device colors.
 */

private void
cups_map_gray(gx_device *pdev,		/* I - Device info */
              frac      g,		/* I - Grayscale value */
	      frac      *out)		/* O - Device colors */
{
  fprintf(stderr, "DEBUG2: cups_map_gray(%p, %d, %p)\n",
          pdev, g, out);

 /*
  * Just use the CMYK mapper...
  */

  cups_map_cmyk(pdev, 0, 0, 0, frac_1 - g, out);
}


/*
 * 'cups_map_rgb()' - Map a RGB color value to device colors.
 */

private void
cups_map_rgb(gx_device             *pdev,
					/* I - Device info */
             const gs_imager_state *pis,/* I - Device state */
             frac                  r,	/* I - Red value */
	     frac                  g,	/* I - Green value */
	     frac                  b,	/* I - Blue value */
	     frac                  *out)/* O - Device colors */
{
  frac		c, m, y, k;		/* CMYK values */
  frac		mk;			/* Maximum K value */
  int		tc, tm, ty;		/* Temporary color values */


  fprintf(stderr, "DEBUG2: cups_map_rgb(%p, %p, %d, %d, %d, %p)\n",
          pdev, pis, r, g, b, out);

 /*
  * Compute CMYK values...
  */

  c = frac_1 - r;
  m = frac_1 - g;
  y = frac_1 - b;
  k = min(c, min(m, y));

  if ((mk = max(c, max(m, y))) > k)
    k = (int)((float)k * (float)k * (float)k / ((float)mk * (float)mk));

  c -= k;
  m -= k;
  y -= k;

 /*
  * Do color correction as needed...
  */

  if (cupsHaveProfile)
  {
   /*
    * Color correct CMY...
    */

    tc = cupsMatrix[0][0][c] +
         cupsMatrix[0][1][m] +
	 cupsMatrix[0][2][y];
    tm = cupsMatrix[1][0][c] +
         cupsMatrix[1][1][m] +
	 cupsMatrix[1][2][y];
    ty = cupsMatrix[2][0][c] +
         cupsMatrix[2][1][m] +
	 cupsMatrix[2][2][y];

    if (tc < 0)
      c = 0;
    else if (tc > frac_1)
      c = frac_1;
    else
      c = (frac)tc;

    if (tm < 0)
      m = 0;
    else if (tm > frac_1)
      m = frac_1;
    else
      m = (frac)tm;

    if (ty < 0)
      y = 0;
    else if (ty > frac_1)
      y = frac_1;
    else
      y = (frac)ty;
  }

 /*
  * Use the CMYK mapping function to produce the device colors...
  */

  cups_map_cmyk(pdev, c, m, y, k, out);
}
#else
/*
 * 'cups_map_cmyk_color()' - Map a CMYK color to a color index.
 *
 * This function is only called when a 4 or 6 color colorspace is
 * selected for output.  CMYK colors are *not* corrected but *are*
 * density adjusted.
 */

private gx_color_index			/* O - Color index */
cups_map_cmyk_color(gx_device      *pdev,
					/* I - Device info */
                    gx_color_value c,	/* I - Cyan value */
                    gx_color_value m,	/* I - Magenta value */
                    gx_color_value y,	/* I - Yellow value */
		    gx_color_value k)	/* I - Black value */
{
  gx_color_index	i;		/* Temporary index */
  gx_color_value	ic, im, iy, ik;	/* Integral CMYK values */


  fprintf(stderr, "DEBUG2: cups_map_cmyk_color(%p, %d, %d, %d, %d)\n", pdev,
          c, m, y, k);

 /*
  * Setup the color info data as needed...
  */

  if (pdev->color_info.num_components == 0)
    cups_set_color_info(pdev);

 /*
  * Density correct...
  */

  if (cupsHaveProfile)
  {
    c = cupsDensity[c];
    m = cupsDensity[m];
    y = cupsDensity[y];
    k = cupsDensity[k];
  }

  ic = cupsEncodeLUT[c];
  im = cupsEncodeLUT[m];
  iy = cupsEncodeLUT[y];
  ik = cupsEncodeLUT[k];

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

  fprintf(stderr, "DEBUG2: CMYK (%d,%d,%d,%d) -> CMYK %08x (%d,%d,%d,%d)\n",
          c, m, y, k, (unsigned)i, ic, im, iy, ik);

 /*
  * Make sure we don't get a CMYK color of 255, 255, 255, 255...
  */

  if (i == gx_no_color_index)
    i --;

  return (i);
}


/*
 * 'cups_map_color_rgb()' - Map a color index to an RGB color.
 */

private int
cups_map_color_rgb(gx_device      *pdev,/* I - Device info */
                   gx_color_index color,/* I - Color index */
		   gx_color_value prgb[3])
					/* O - RGB values */
{
  unsigned char		c0, c1, c2, c3;	/* Color index components */
  gx_color_value	k, divk;	/* Black & divisor */


  fprintf(stderr, "DEBUG2: cups_map_color_rgb(%p, %d, %p)\n", pdev,
          (unsigned)color, prgb);

 /*
  * Setup the color info data as needed...
  */

  if (pdev->color_info.num_components == 0)
    cups_set_color_info(pdev);

  fprintf(stderr, "DEBUG2: COLOR %08x = ", (unsigned)color);

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
        prgb[2] = cupsDecodeLUT[c3];
        break;

    case CUPS_CSPACE_W :
        prgb[0] =
        prgb[1] =
        prgb[2] = cupsDecodeLUT[c3];
        break;

    case CUPS_CSPACE_RGB :
        prgb[0] = cupsDecodeLUT[c1];
        prgb[1] = cupsDecodeLUT[c2];
        prgb[2] = cupsDecodeLUT[c3];
        break;

    case CUPS_CSPACE_RGBA :
        prgb[0] = cupsDecodeLUT[c0];
        prgb[1] = cupsDecodeLUT[c1];
        prgb[2] = cupsDecodeLUT[c2];
        break;

    case CUPS_CSPACE_CMY :
        prgb[0] = cupsDecodeLUT[c1];
        prgb[1] = cupsDecodeLUT[c2];
        prgb[2] = cupsDecodeLUT[c3];
        break;

    case CUPS_CSPACE_YMC :
        prgb[0] = cupsDecodeLUT[c3];
        prgb[1] = cupsDecodeLUT[c2];
        prgb[2] = cupsDecodeLUT[c1];
        break;

    case CUPS_CSPACE_KCMY :
    case CUPS_CSPACE_KCMYcm :
        k    = cupsDecodeLUT[c0];
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
        k    = cupsDecodeLUT[c3];
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
        k    = cupsDecodeLUT[c3];
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

#  ifdef CUPS_RASTER_HAVE_COLORIMETRIC
    case CUPS_CSPACE_CIEXYZ :
    case CUPS_CSPACE_CIELab :
    case CUPS_CSPACE_ICC1 :
    case CUPS_CSPACE_ICC2 :
    case CUPS_CSPACE_ICC3 :
    case CUPS_CSPACE_ICC4 :
    case CUPS_CSPACE_ICC5 :
    case CUPS_CSPACE_ICC6 :
    case CUPS_CSPACE_ICC7 :
    case CUPS_CSPACE_ICC8 :
    case CUPS_CSPACE_ICC9 :
    case CUPS_CSPACE_ICCA :
    case CUPS_CSPACE_ICCB :
    case CUPS_CSPACE_ICCC :
    case CUPS_CSPACE_ICCD :
    case CUPS_CSPACE_ICCE :
    case CUPS_CSPACE_ICCF :
        break;
#  endif /* CUPS_RASTER_HAVE_COLORIMETRIC */
  }

  fprintf(stderr, "%d,%d,%d\n", prgb[0], prgb[1], prgb[2]);

  return (0);
}


/*
 * 'cups_map_rgb_color()' - Map an RGB color to a color index.  We map the
 *                          RGB color to the output colorspace & bits (we
 *                          figure out the format when we output a page).
 */

private gx_color_index			/* O - Color index */
cups_map_rgb_color(gx_device      *pdev,/* I - Device info */
                   gx_color_value r,	/* I - Red value */
                   gx_color_value g,	/* I - Green value */
                   gx_color_value b)	/* I - Blue value */
{
  gx_color_index	i;		/* Temporary index */
  gx_color_value	ic, im, iy, ik;	/* Integral CMYK values */
  gx_color_value	mk;		/* Maximum K value */
  int			tc, tm, ty;	/* Temporary color values */
  float			rr, rg, rb,	/* Real RGB colors */
			ciex, ciey, ciez,
					/* CIE XYZ colors */
			ciey_yn,	/* Normalized luminance */
			ciel, ciea, cieb;
					/* CIE Lab colors */


  fprintf(stderr, "DEBUG2: cups_map_rgb_color(%p, %d, %d, %d)\n", pdev, r, g, b);

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
        i = cupsEncodeLUT[(r * 31 + g * 61 + b * 8) / 100];
        break;

    case CUPS_CSPACE_RGB :
        ic = cupsEncodeLUT[r];
        im = cupsEncodeLUT[g];
        iy = cupsEncodeLUT[b];

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
        ic = cupsEncodeLUT[r];
        im = cupsEncodeLUT[g];
        iy = cupsEncodeLUT[b];

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
        i = cupsEncodeLUT[gx_max_color_value - (r * 31 + g * 61 + b * 8) / 100];
        break;

    case CUPS_CSPACE_CMY :
        ic = cupsEncodeLUT[gx_max_color_value - r];
        im = cupsEncodeLUT[gx_max_color_value - g];
        iy = cupsEncodeLUT[gx_max_color_value - b];

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
        ic = cupsEncodeLUT[gx_max_color_value - r];
        im = cupsEncodeLUT[gx_max_color_value - g];
        iy = cupsEncodeLUT[gx_max_color_value - b];

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

        ic = cupsEncodeLUT[ic - ik];
        im = cupsEncodeLUT[im - ik];
        iy = cupsEncodeLUT[iy - ik];
        ik = cupsEncodeLUT[ik];

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

	fprintf(stderr, "DEBUG2: CMY (%d,%d,%d) -> CMYK %08x (%d,%d,%d,%d)\n",
	        r, g, b, (unsigned)i, ic, im, iy, ik);
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

        ic = cupsEncodeLUT[ic - ik];
        im = cupsEncodeLUT[im - ik];
        iy = cupsEncodeLUT[iy - ik];
        ik = cupsEncodeLUT[ik];

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

          ic = cupsEncodeLUT[ic - ik];
          im = cupsEncodeLUT[im - ik];
          iy = cupsEncodeLUT[iy - ik];
          ik = cupsEncodeLUT[ik];
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

        ic = cupsEncodeLUT[ic - ik];
        im = cupsEncodeLUT[im - ik];
        iy = cupsEncodeLUT[iy - ik];
        ik = cupsEncodeLUT[ik];

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

#  ifdef CUPS_RASTER_HAVE_COLORIMETRIC
    case CUPS_CSPACE_CIEXYZ :
    case CUPS_CSPACE_CIELab :
    case CUPS_CSPACE_ICC1 :
    case CUPS_CSPACE_ICC2 :
    case CUPS_CSPACE_ICC3 :
    case CUPS_CSPACE_ICC4 :
    case CUPS_CSPACE_ICC5 :
    case CUPS_CSPACE_ICC6 :
    case CUPS_CSPACE_ICC7 :
    case CUPS_CSPACE_ICC8 :
    case CUPS_CSPACE_ICC9 :
    case CUPS_CSPACE_ICCA :
    case CUPS_CSPACE_ICCB :
    case CUPS_CSPACE_ICCC :
    case CUPS_CSPACE_ICCD :
    case CUPS_CSPACE_ICCE :
    case CUPS_CSPACE_ICCF :
       /*
        * Convert sRGB to linear RGB...
	*/

	rr = pow((double)r / (double)gx_max_color_value, 0.58823529412);
	rg = pow((double)g / (double)gx_max_color_value, 0.58823529412);
	rb = pow((double)b / (double)gx_max_color_value, 0.58823529412);

       /*
        * Convert to CIE XYZ...
	*/

	ciex = 0.412453 * rr + 0.357580 * rg + 0.180423 * rb;
	ciey = 0.212671 * rr + 0.715160 * rg + 0.072169 * rb;
	ciez = 0.019334 * rr + 0.119193 * rg + 0.950227 * rb;

        if (cups->header.cupsColorSpace == CUPS_CSPACE_CIEXYZ)
	{
	 /*
	  * Convert to an integer XYZ color value...
	  */

          if (ciex > 1.0)
	    ic = 255;
	  else if (ciex > 0.0)
	    ic = (int)(ciex * 255.0);
	  else
	    ic = 0;

          if (ciey > 1.0)
	    im = 255;
	  else if (ciey > 0.0)
	    im = (int)(ciey * 255.0);
	  else
	    im = 0;

          if (ciez > 1.0)
	    iy = 255;
	  else if (ciez > 0.0)
	    iy = (int)(ciez * 255.0);
	  else
	    iy = 0;
	}
	else
	{
	 /*
	  * Convert CIE XYZ to Lab...
	  */

	  ciey_yn = ciey / D65_Y;

	  if (ciey_yn > 0.008856)
	    ciel = 116 * cbrt(ciey_yn) - 16;
	  else
	    ciel = 903.3 * ciey_yn;

	  ciea = 500 * (cups_map_cielab(ciex, D65_X) -
	                cups_map_cielab(ciey, D65_Y));
	  cieb = 200 * (cups_map_cielab(ciey, D65_Y) -
	                cups_map_cielab(ciez, D65_Z));

         /*
	  * Scale the L value and bias the a and b values by 128
	  * so that all values are in the range of 0 to 255.
	  */

	  ciel *= 2.55;
	  ciea += 128;
	  cieb += 128;

         /*
	  * Convert to 8-bit values...
	  */

          if (ciel < 0.0)
	    ic = 0;
	  else if (ciel < 255.0)
	    ic = ciel;
	  else
	    ic = 255;

          if (ciea < 0.0)
	    im = 0;
	  else if (ciea < 255.0)
	    im = ciea;
	  else
	    im = 255;

          if (cieb < 0.0)
	    iy = 0;
	  else if (cieb < 255.0)
	    iy = cieb;
	  else
	    iy = 255;
	}

       /*
        * Put the final color value together...
	*/

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
#  endif /* CUPS_RASTER_HAVE_COLORIMETRIC */
  }

  fprintf(stderr, "DEBUG2: RGB %d,%d,%d = %08x\n", r, g, b, (unsigned)i);

  return (i);
}
#endif /* dev_t_proc_encode_color */


/*
 * 'cups_open()' - Open the output file and initialize things.
 */

private int				/* O - Error status */
cups_open(gx_device *pdev)		/* I - Device info */
{
  int	code;				/* Return status */


  fprintf(stderr, "DEBUG2: cups_open(%p)\n", pdev);

  cups->printer_procs.get_space_params = cups_get_space_params;

  if (cups->page == 0)
  {
    fputs("INFO: Processing page 1...\n", stderr);
    cups->page = 1;
  }

  cups_set_color_info(pdev);

  if ((code = gdev_prn_open(pdev)) != 0)
    return (code);

  if (cupsPPD == NULL)
    cupsPPD = ppdOpenFile(getenv("PPD"));

  return (0);
}


/*
 * 'cups_print_pages()' - Send one or more pages to the output file.
 */

private int				/* O - 0 if everything is OK */
cups_print_pages(gx_device_printer *pdev,
					/* I - Device info */
                 FILE              *fp,	/* I - Output file */
		 int               num_copies)
					/* I - Number of copies */
{
  int		copy;			/* Copy number */
  int		srcbytes;		/* Byte width of scanline */
  unsigned char	*src,			/* Scanline data */
		*dst;			/* Bitmap data */


  (void)fp; /* reference unused file pointer to prevent compiler warning */

  fprintf(stderr, "DEBUG2: cups_print_pages(%p, %p, %d)\n", pdev, fp,
          num_copies);

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

  fprintf(stderr, "DEBUG2: cupsBitsPerPixel = %d, cupsWidth = %d, cupsBytesPerLine = %d, srcbytes = %d\n",
          cups->header.cupsBitsPerPixel, cups->header.cupsWidth,
	  cups->header.cupsBytesPerLine, srcbytes);

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
    if ((cups->stream = cupsRasterOpen(fileno(cups->file),
                                       CUPS_RASTER_WRITE)) == NULL)
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

  if (cupsPPD != NULL && !cupsPPD->manual_copies)
  {
    cups->header.NumCopies = num_copies;
    num_copies = 1;
  }

  fprintf(stderr, "DEBUG2: cupsWidth = %d, cupsHeight = %d, cupsBytesPerLine = %d\n",
          cups->header.cupsWidth, cups->header.cupsHeight,
	  cups->header.cupsBytesPerLine);

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
#ifdef CUPS_RASTER_SYNCv1
  char			name[255];	/* Name of attribute */
#endif /* CUPS_RASTER_SYNCv1 */
  float			margins[4];	/* Physical margins of print */
  ppd_size_t		*size;		/* Page size */
  int			code;		/* Error code */
  int			intval;		/* Integer value */
  bool			boolval;	/* Boolean value */
  float			floatval;	/* Floating point value */
  gs_param_string	stringval;	/* String value */
  gs_param_float_array	arrayval;	/* Float array value */
  int			size_set;	/* Was the size set? */
  int			color_set;	/* Were the color attrs set? */
  gdev_prn_space_params	sp;		/* Space parameter data */
  int			width,		/* New width of page */
			height;		/* New height of page */


  fprintf(stderr, "DEBUG2: cups_put_params(%p, %p)\n", pdev, plist);

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

  size_set  = param_read_float_array(plist, ".MediaSize", &arrayval) == 0 ||
              param_read_float_array(plist, "PageSize", &arrayval) == 0;
  color_set = param_read_int(plist, "cupsColorSpace", &intval) == 0 ||
              param_read_int(plist, "cupsBitsPerColor", &intval) == 0;

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

#ifdef CUPS_RASTER_SYNCv1
  for (i = 0; i < 16; i ++)
  {
    sprintf(name, "cupsInteger%d", i);
    intoption(cupsInteger[i], name, unsigned)
  }

  for (i = 0; i < 16; i ++)
  {
    sprintf(name, "cupsReal%d", i);
    floatoption(cupsReal[i], name)
  }

  for (i = 0; i < 16; i ++)
  {
    sprintf(name, "cupsString%d", i);
    stringoption(cupsString[i], name)
  }

  stringoption(cupsMarkerType, "cupsMarkerType");
  stringoption(cupsRenderingIntent, "cupsRenderingIntent");
#endif /* CUPS_RASTER_SYNCv1 */

  if ((code = param_read_string(plist, "cupsProfile", &stringval)) < 0)
  {
    param_signal_error(plist, "cupsProfile", code);
    return (code);
  }
  else if (code == 0)
  {
    if (cupsProfile != NULL)
      free(cupsProfile);

    cupsProfile = strdup(stringval.data);
  }

  cups_set_color_info(pdev);

 /*
  * Then process standard page device options...
  */

  if ((code = gdev_prn_put_params(pdev, plist)) < 0)
    return (code);

 /*
  * Update margins/sizes as needed...
  */

  if (size_set)
  {
   /*
    * Compute the page margins...
    */

    fprintf(stderr, "DEBUG: Updating PageSize to [%.0f %.0f]...\n",
            cups->MediaSize[0], cups->MediaSize[1]);

    memset(margins, 0, sizeof(margins));

    cups->landscape = 0;

    if (cupsPPD != NULL)
    {
     /*
      * Find the matching page size...
      */

      for (i = cupsPPD->num_sizes, size = cupsPPD->sizes;
           i > 0;
           i --, size ++)
	if (fabs(cups->MediaSize[1] - size->length) < 5.0 &&
            fabs(cups->MediaSize[0] - size->width) < 5.0)
	  break;

      if (i > 0)
      {
       /*
	* Standard size...
	*/

	fprintf(stderr, "DEBUG: size = %s\n", size->name);

	gx_device_set_media_size(pdev, size->width, size->length);

	margins[0] = size->left / 72.0;
	margins[1] = size->bottom / 72.0;
	margins[2] = (size->width - size->right) / 72.0;
	margins[3] = (size->length - size->top) / 72.0;
      }
      else
      {
       /*
	* No matching portrait size; look for a matching size in
	* landscape orientation...
	*/

	for (i = cupsPPD->num_sizes, size = cupsPPD->sizes;
             i > 0;
             i --, size ++)
	  if (fabs(cups->MediaSize[0] - size->length) < 5.0 &&
              fabs(cups->MediaSize[1] - size->width) < 5.0)
	    break;

	if (i > 0)
	{
	 /*
	  * Standard size in landscape orientation...
	  */

	  fprintf(stderr, "DEBUG: landscape size = %s\n", size->name);

	  gx_device_set_media_size(pdev, size->length, size->width);

          cups->landscape = 1;

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
            margins[i] = cupsPPD->custom_margins[i] / 72.0;
	}
      }

      fprintf(stderr, "DEBUG: margins[] = [ %f %f %f %f ]\n",
	      margins[0], margins[1], margins[2], margins[3]);
    }

   /*
    * Set the margins to update the bitmap size...
    */

    gx_device_set_margins(pdev, margins, false);
  }

 /*
  * Set CUPS raster header values...
  */

  cups->header.HWResolution[0] = pdev->HWResolution[0];
  cups->header.HWResolution[1] = pdev->HWResolution[1];

  cups->header.Margins[0] = pdev->HWMargins[0];
  cups->header.Margins[1] = pdev->HWMargins[1];

  cups->header.PageSize[0] = pdev->MediaSize[0];
  cups->header.PageSize[1] = pdev->MediaSize[1];

  cups->header.ImagingBoundingBox[0] = pdev->HWMargins[0];
  cups->header.ImagingBoundingBox[1] = pdev->HWMargins[3];
  cups->header.ImagingBoundingBox[2] = pdev->MediaSize[0] - pdev->HWMargins[2];
  cups->header.ImagingBoundingBox[3] = pdev->MediaSize[1] - pdev->HWMargins[1];

 /*
  * Reallocate memory if the size or color depth was changed...
  */

  if (color_set || size_set)
  {
   /*
    * Make sure the page image is the correct size - current Ghostscript
    * does not keep track of the margins in the bitmap size...
    */

    if (cups->landscape)
    {
      width  = (pdev->MediaSize[1] - pdev->HWMargins[0] - pdev->HWMargins[2]) *
               pdev->HWResolution[0] / 72.0f + 0.499f;
      height = (pdev->MediaSize[0] - pdev->HWMargins[1] - pdev->HWMargins[3]) *
               pdev->HWResolution[1] / 72.0f + 0.499f;
    }
    else
    {
      width  = (pdev->MediaSize[0] - pdev->HWMargins[0] - pdev->HWMargins[2]) *
               pdev->HWResolution[0] / 72.0f + 0.499f;
      height = (pdev->MediaSize[1] - pdev->HWMargins[1] - pdev->HWMargins[3]) *
               pdev->HWResolution[1] / 72.0f + 0.499f;
    }

   /*
    * Don't reallocate memory unless the device has been opened...
    */

    if (pdev->is_open)
    {
     /*
      * Device is open, so reallocate...
      */

      fprintf(stderr, "DEBUG: Reallocating memory, [%.0f %.0f] = %dx%d pixels...\n",
              pdev->MediaSize[0], pdev->MediaSize[1], width, height);

      sp = ((gx_device_printer *)pdev)->space_params;

      if ((code = gdev_prn_reallocate_memory(pdev, &sp, width, height)) < 0)
	return (code);
    }
    else
    {
     /*
      * Device isn't yet open, so just save the new width and height...
      */

      fprintf(stderr, "DEBUG: Setting initial media size, [%.0f %.0f] = %dx%d pixels...\n",
              pdev->MediaSize[0], pdev->MediaSize[1], width, height);

      pdev->width  = width;
      pdev->height = height;
    }
  }

  fprintf(stderr, "DEBUG2: ppd = %p\n", cupsPPD);
  fprintf(stderr, "DEBUG2: PageSize = [ %.3f %.3f ]\n",
          pdev->MediaSize[0], pdev->MediaSize[1]);
  fprintf(stderr, "DEBUG2: margins = [ %.3f %.3f %.3f %.3f ]\n",
          margins[0], margins[1], margins[2], margins[3]);
  fprintf(stderr, "DEBUG2: HWResolution = [ %.3f %.3f ]\n",
          pdev->HWResolution[0], pdev->HWResolution[1]);
  fprintf(stderr, "DEBUG2: width = %d, height = %d\n",
          pdev->width, pdev->height);
  fprintf(stderr, "DEBUG2: HWMargins = [ %.3f %.3f %.3f %.3f ]\n",
          pdev->HWMargins[0], pdev->HWMargins[1],
	  pdev->HWMargins[2], pdev->HWMargins[3]);

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
  int		max_lut;		/* Maximum LUT value */
  float		d, g;			/* Density and gamma correction */
  float		m[3][3];		/* Color correction matrix */
  char		resolution[41];		/* Resolution string */
  ppd_profile_t	*profile;		/* Color profile information */


  fprintf(stderr, "DEBUG2: cups_set_color_info(%p)\n", pdev);

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

#ifdef CUPS_RASTER_HAVE_COLORIMETRIC
    case CUPS_CSPACE_CIEXYZ :
    case CUPS_CSPACE_CIELab :
    case CUPS_CSPACE_ICC1 :
    case CUPS_CSPACE_ICC2 :
    case CUPS_CSPACE_ICC3 :
    case CUPS_CSPACE_ICC4 :
    case CUPS_CSPACE_ICC5 :
    case CUPS_CSPACE_ICC6 :
    case CUPS_CSPACE_ICC7 :
    case CUPS_CSPACE_ICC8 :
    case CUPS_CSPACE_ICC9 :
    case CUPS_CSPACE_ICCA :
    case CUPS_CSPACE_ICCB :
    case CUPS_CSPACE_ICCC :
    case CUPS_CSPACE_ICCD :
    case CUPS_CSPACE_ICCE :
    case CUPS_CSPACE_ICCF :
       /*
	* Colorimetric color spaces currently are implemented as 24-bit
	* mapping to XYZ or Lab, which are then converted as needed to
	* the final representation...
	*
	* This code enforces a minimum output depth of 8 bits per
	* component...
	*/

	if (cups->header.cupsBitsPerColor < 8)
          cups->header.cupsBitsPerColor = 8;

	if (cups->header.cupsColorOrder != CUPS_ORDER_CHUNKED)
          cups->header.cupsBitsPerPixel = cups->header.cupsBitsPerColor;
	else
          cups->header.cupsBitsPerPixel = 3 * cups->header.cupsBitsPerColor;

	cups->color_info.depth          = 24;
	cups->color_info.num_components = 3;
	break;
#endif /* CUPS_RASTER_HAVE_COLORIMETRIC */
  }

#ifdef dev_t_proc_encode_color
  switch (cups->header.cupsColorSpace)
  {
    default :
        cups->color_info.gray_index = GX_CINFO_COMP_NO_INDEX;
	break;

    case CUPS_CSPACE_W :
    case CUPS_CSPACE_WHITE :
    case CUPS_CSPACE_K :
    case CUPS_CSPACE_GOLD :
    case CUPS_CSPACE_SILVER :
    case CUPS_CSPACE_KCMYcm :
    case CUPS_CSPACE_KCMY :
        cups->color_info.gray_index = 0;
	break;

    case CUPS_CSPACE_CMYK :
    case CUPS_CSPACE_YMCK :
    case CUPS_CSPACE_GMCK :
    case CUPS_CSPACE_GMCS :
        cups->color_info.gray_index = 3;
	break;
  }

  switch (cups->header.cupsColorSpace)
  {
    default :
    case CUPS_CSPACE_W :
    case CUPS_CSPACE_WHITE :
    case CUPS_CSPACE_RGB :
#  ifdef CUPS_RASTER_HAVE_COLORIMETRIC
    case CUPS_CSPACE_CIEXYZ :
    case CUPS_CSPACE_CIELab :
    case CUPS_CSPACE_ICC1 :
    case CUPS_CSPACE_ICC2 :
    case CUPS_CSPACE_ICC3 :
    case CUPS_CSPACE_ICC4 :
    case CUPS_CSPACE_ICC5 :
    case CUPS_CSPACE_ICC6 :
    case CUPS_CSPACE_ICC7 :
    case CUPS_CSPACE_ICC8 :
    case CUPS_CSPACE_ICC9 :
    case CUPS_CSPACE_ICCA :
    case CUPS_CSPACE_ICCB :
    case CUPS_CSPACE_ICCC :
    case CUPS_CSPACE_ICCD :
    case CUPS_CSPACE_ICCE :
    case CUPS_CSPACE_ICCF :
#  endif /* CUPS_RASTER_HAVE_COLORIMETRIC */
        cups->color_info.polarity = GX_CINFO_POLARITY_ADDITIVE;
        break;

    case CUPS_CSPACE_K :
    case CUPS_CSPACE_GOLD :
    case CUPS_CSPACE_SILVER :
    case CUPS_CSPACE_CMY :
    case CUPS_CSPACE_YMC :
    case CUPS_CSPACE_KCMYcm :
    case CUPS_CSPACE_CMYK :
    case CUPS_CSPACE_YMCK :
    case CUPS_CSPACE_KCMY :
    case CUPS_CSPACE_GMCK :
    case CUPS_CSPACE_GMCS :
        cups->color_info.polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
        break;
  }

  cups->color_info.separable_and_linear = GX_CINFO_SEP_LIN_NONE;
#endif /* dev_t_proc_encode_color */

  if ((i = cups->header.cupsBitsPerColor) > 8)
    i = 8;

  max_lut = (1 << i) - 1;

  switch (cups->color_info.num_components)
  {
    default :
    case 1 :
	cups->color_info.max_gray      = max_lut;
	cups->color_info.max_color     = 0;
	cups->color_info.dither_grays  = max_lut + 1;
	cups->color_info.dither_colors = 0;
        break;

    case 3 :
	cups->color_info.max_gray      = 0;
	cups->color_info.max_color     = max_lut;
	cups->color_info.dither_grays  = 0;
	cups->color_info.dither_colors = max_lut + 1;
	break;

    case 4 :
	cups->color_info.max_gray      = max_lut;
	cups->color_info.max_color     = max_lut;
	cups->color_info.dither_grays  = max_lut + 1;
	cups->color_info.dither_colors = max_lut + 1;
	break;
  }

 /*
  * Enable/disable CMYK color support...
  */

#ifdef dev_t_proc_encode_color
  cups->color_info.max_components = cups->color_info.num_components;
#endif /* dev_t_proc_encode_color */

 /*
  * Tell Ghostscript to forget any colors it has cached...
  */

  gx_device_decache_colors(pdev);

 /*
  * Compute the lookup tables...
  */

  for (i = 0; i <= gx_max_color_value; i ++)
  {
    cupsEncodeLUT[i] = (max_lut * i + gx_max_color_value / 2) /
                       gx_max_color_value;

    if (i == 0 || cupsEncodeLUT[i] != cupsEncodeLUT[i - 1])
      fprintf(stderr, "DEBUG2: cupsEncodeLUT[%d] = %d\n", i, cupsEncodeLUT[i]);
  }

  for (i = 0; i < cups->color_info.dither_grays; i ++)
    cupsDecodeLUT[i] = gx_max_color_value * i / max_lut;

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

 /*
  * Set the color profile as needed...
  */

  cupsHaveProfile = 0;

#ifdef dev_t_proc_encode_color
  if (cupsProfile)
#else
  if (cupsProfile && cups->header.cupsBitsPerColor == 8)
#endif /* dev_t_proc_encode_color */
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
#ifdef dev_t_proc_encode_color
  else if (cupsPPD)
#else
  else if (cupsPPD && cups->header.cupsBitsPerColor == 8)
#endif /* dev_t_proc_encode_color */
  {
   /*
    * Find the appropriate color profile...
    */

    if (pdev->HWResolution[0] != pdev->HWResolution[1])
      sprintf(resolution, "%.0fx%.0fdpi", pdev->HWResolution[0],
              pdev->HWResolution[1]);
    else
      sprintf(resolution, "%.0fdpi", pdev->HWResolution[0]);

    for (i = 0, profile = cupsPPD->profiles;
         i < cupsPPD->num_profiles;
	 i ++, profile ++)
      if ((strcmp(profile->resolution, resolution) == 0 ||
           profile->resolution[0] == '-') &&
          (strcmp(profile->media_type, cups->header.MediaType) == 0 ||
           profile->media_type[0] == '-'))
	break;

   /*
    * If we found a color profile, use it!
    */

    if (i < cupsPPD->num_profiles)
    {
      fputs("DEBUG: Using color profile in PPD file!\n", stderr);

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
	for (k = 0; k <= CUPS_MAX_VALUE; k ++)
	{
          cupsMatrix[i][j][k] = (int)((float)k * m[i][j] + 0.5);

          if ((k & 4095) == 0)
            fprintf(stderr, "DEBUG2: cupsMatrix[%d][%d][%d] = %d\n",
	            i, j, k, cupsMatrix[i][j][k]);
        }


    for (k = 0; k <= CUPS_MAX_VALUE; k ++)
    {
      cupsDensity[k] = (int)((float)CUPS_MAX_VALUE * d *
	                     pow((float)k / (float)CUPS_MAX_VALUE, g) +
			     0.5);

      if ((k & 4095) == 0)
        fprintf(stderr, "DEBUG2: cupsDensity[%d] = %d\n", k, cupsDensity[k]);
    }
  }
  else
  {
    for (k = 0; k <= CUPS_MAX_VALUE; k ++)
      cupsDensity[k] = k;
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
cups_print_chunked(gx_device_printer *pdev,
					/* I - Printer device */
                   unsigned char     *src,
					/* I - Scanline buffer */
		   unsigned char     *dst,
					/* I - Bitmap buffer */
		   int               srcbytes)
					/* I - Number of bytes in src */
{
  int		y;			/* Looping var */
  unsigned char	*srcptr,		/* Pointer to data */
		*dstptr;		/* Pointer to bits */
  int		count;			/* Count for loop */
  int		flip;			/* Flip scanline? */


  if (cups->header.Duplex && !cups->header.Tumble &&
      cupsPPD && cupsPPD->flip_duplex && !(cups->page & 1))
    flip = 1;
  else
    flip = 0;

  fprintf(stderr, "DEBUG: cups_print_chunked - flip = %d, height = %d\n",
          flip, cups->height);

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
	        *dstptr = cupsRevUpper1[*srcptr & 15] |
		          cupsRevLower1[*srcptr >> 4];
              }
	      break;

	  case 2 : /* 2-bit grayscale */
	      for (srcptr += srcbytes - 1;
	           count > 0;
		   count --, srcptr --, dstptr ++)
	      {
	        *dstptr = cupsRevUpper2[*srcptr & 15] |
		          cupsRevLower2[*srcptr >> 4];
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
cups_print_banded(gx_device_printer *pdev,
					/* I - Printer device */
                  unsigned char     *src,
					/* I - Scanline buffer */
		  unsigned char     *dst,
					/* I - Bitmap buffer */
		  int               srcbytes)
					/* I - Number of bytes in src */
{
  int		x;			/* Looping var */
  int		y;			/* Looping var */
  int		bandbytes;		/* Bytes per band */
  unsigned char	bit;			/* Current bit */
  unsigned char	temp;			/* Temporary variable */
  unsigned char	*srcptr;		/* Pointer to data */
  unsigned char	*cptr, *mptr, *yptr,	/* Pointer to components */
		*kptr, *lcptr, *lmptr;	/* ... */
  int		flip;			/* Flip scanline? */


  if (cups->header.Duplex && !cups->header.Tumble &&
      cupsPPD && cupsPPD->flip_duplex && !(cups->page & 1))
    flip = 1;
  else
    flip = 0;

  fprintf(stderr, "DEBUG: cups_print_banded - flip = %d, height = %d\n",
          flip, cups->height);

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
cups_print_planar(gx_device_printer *pdev,
					/* I - Printer device */
                  unsigned char     *src,
					/* I - Scanline buffer */
		  unsigned char     *dst,
					/* I - Bitmap buffer */
		  int               srcbytes)
					/* I - Number of bytes in src */
{
  int		x;			/* Looping var */
  int		y;			/* Looping var */
  int		z;			/* Looping var */
  unsigned char	srcbit;			/* Current source bit */
  unsigned char	dstbit;			/* Current destination bit */
  unsigned char	temp;			/* Temporary variable */
  unsigned char	*srcptr;		/* Pointer to data */
  unsigned char	*dstptr;		/* Pointer to bitmap */


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
 * End of "$Id: gdevcups.c,v 1.43.2.23 2004/06/29 13:15:10 mike Exp $".
 */
