/* Copyright (C) 1989, 1995 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility to
  anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer to
  the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given to
  you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises is not affiliated with the Free Software Foundation or
  the GNU Project.  GNU Ghostscript, as distributed by Aladdin Enterprises,
  does not depend on any other GNU software.
*/

/* gdevx.h */
/* Header file with X device structure */
/* Requires gxdevice.h and x_.h */

/* Define the type of an X pixel. */
typedef unsigned long x_pixel;

/* Define a rectangle structure for update bookkeeping */
typedef struct rect_s {
  int xo, yo, xe, ye;
} rect;

/* Define dynamic color hash table structure */
struct x11color_s;
typedef struct x11color_s x11color;
struct x11color_s {
    XColor color;
    x11color *next;
};

/* Define PostScript to X11 font name mapping */
struct x11fontmap_s;
typedef struct x11fontmap_s x11fontmap;
struct x11fontmap_s {
    char *ps_name;
    char *x11_name;
    char **std_names;
    char **iso_names;
    int std_count, iso_count;
    x11fontmap *next;
};

/* Define the X Windows device */
typedef struct gx_device_X_s {
	gx_device_common;
	bool IsPageDevice;

	/* An XImage object for writing bitmap images to the screen */
	XImage image;

	/* Global X state */
	Display *dpy;
	Screen *scr;
	XVisualInfo *vinfo;
	Colormap cmap;
	Window win;
	GC gc;

	/* An optional Window ID supplied as a device parameter */
	Window pwin;

	/* A backing pixmap so X will handle exposure automatically */
	Pixmap bpixmap;			/* 0 if useBackingPixmap is false, */
					/* or if it can't be allocated */
	int ghostview;		/* flag to tell if ghostview is in control */
	Window mwin;		/* window to receive ghostview messages */
/* Don't include standard colormap stuff for X11R3 and earlier */
#if HaveStdCMap
	XStandardColormap *std_cmap;	/* standard color map if available */
#endif
	gs_matrix initial_matrix;	/* the initial transformation */
	Atom NEXT, PAGE, DONE;	/* Atoms used to talk to ghostview */
	rect update;		/* region needing updating */
	long up_area;		/* total area of update */
				/* (always 0 if no backing pixmap) */
	int up_count;		/* # of updates since flush */
	Pixmap dest;		/* bpixmap if non-0, else win */
	x_pixel colors_or;	/* 'or' of all device colors used so far */
	x_pixel colors_and;	/* 'and' ditto */

	/* An intermediate pixmap for the stencil case of copy_mono */
	struct {
	  Pixmap pixmap;
	  GC gc;
	  int raster, height;
	} cp;

	/* Structure for dealing with the halftone tile. */
	/* Later this might become a multi-element cache. */
	struct {
	  Pixmap pixmap;
	  Pixmap no_pixmap;	/* kludge to get around X bug */
	  gx_bitmap_id id;
	  int width, height, raster;
	  x_pixel fore_c, back_c;
	} ht;

	/* Cache the function and fill style from the GC */
	int function;
	int fill_style;
	Font fid;

#define set_fill_style(style)\
  if ( xdev->fill_style != style )\
    XSetFillStyle(xdev->dpy, xdev->gc, (xdev->fill_style = style))
#define set_function(func)\
  if ( xdev->function != func )\
    XSetFunction(xdev->dpy, xdev->gc, (xdev->function = func))
#define set_font(font)\
  if ( xdev->fid != font )\
    XSetFont(xdev->dpy, xdev->gc, (xdev->fid = font))

	x_pixel back_color, fore_color;

	Pixel	background, foreground;
#define X_max_color_value 0xffff
#define cube_index(r,g,b) (((r) * xdev->color_info.dither_colors + (g)) * \
				  xdev->color_info.dither_colors + (b))
	x_pixel	*dither_colors;
	ushort	color_mask;
	int	num_rgb;
	x11color	*(*dynamic_colors)[];
	int	max_dynamic_colors, dynamic_size, dynamic_allocs;

#define note_color(pixel)\
  xdev->colors_or |= pixel,\
  xdev->colors_and &= pixel
#define set_back_color(pixel)\
  if ( xdev->back_color != pixel )\
   { xdev->back_color = pixel;\
     note_color(pixel);\
     XSetBackground(xdev->dpy, xdev->gc, pixel);\
   }
#define set_fore_color(pixel)\
  if ( xdev->fore_color != pixel )\
   { xdev->fore_color = pixel;\
     note_color(pixel);\
     XSetForeground(xdev->dpy, xdev->gc, pixel);\
   }

	/* Defautlts set by resources */
	Pixel borderColor;
	Dimension borderWidth;
	String geometry;
	int maxGrayRamp, maxRGBRamp;
	String palette;
	String regularFonts;
	String symbolFonts;
	String dingbatFonts;
	x11fontmap *regular_fonts;
	x11fontmap *symbol_fonts;
	x11fontmap *dingbat_fonts;
	Boolean useXFonts, useFontExtensions, useScalableFonts, logXFonts;
	float xResolution, yResolution;

	/* Flags work around various X server problems. */
	Boolean useBackingPixmap;
	Boolean useXPutImage;
	Boolean useXSetTile;

} gx_device_X;

/* function to keep track of screen updates */
void x_update_add(P5(gx_device *, int, int, int, int));
void gdev_x_clear_window(P1(gx_device_X *));
int x_catch_free_colors(P2(Display *, XErrorEvent *));

/* Number used to distinguish when resoultion was set from the command line */
#define FAKE_RES (16*72)
