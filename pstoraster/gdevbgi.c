/* Copyright (C) 1989, 1990, 1991, 1993, 1994, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevbgi.c */
/* Driver for Borland Graphics Interface (BGI) */
#include <string.h>
#include <stdlib.h>
#include <conio.h>
#include <graphics.h>
#include <mem.h>
#include "gserrors.h"
#include "gx.h"
#include "gxdevice.h"
#include "gxxfont.h"

#ifndef BGI_LIB				/* may be set in makefile */
#  define BGI_LIB ""
#endif

/*
 * BGI supports these video cards:
 *   Hercules, CGA, MCGA, EGA, VGA, AT&T 400, IBM 8514, PC3270.
 * Highest resolution mode is used with all these video cards.
 * EGA and VGA display 16 colors, the rest are black-and-white only.
 * In addition, the environment variable BGIUSER may be used
 * to define a user-supplied Super VGA driver: see the use.doc file
 * for details.
 */
#define SUPER_VGA 999			/* bogus # for user-defined driver */

/* See gxdevice.h for the definitions of the procedures. */

dev_proc_open_device(bgi_open);
dev_proc_close_device(bgi_close);
dev_proc_map_rgb_color(bgi_map_rgb_color);
dev_proc_map_color_rgb(bgi_map_color_rgb);
dev_proc_fill_rectangle(bgi_fill_rectangle);
dev_proc_tile_rectangle(bgi_tile_rectangle);
dev_proc_copy_mono(bgi_copy_mono);
dev_proc_copy_color(bgi_copy_color);
dev_proc_draw_line(bgi_draw_line);
dev_proc_get_xfont_procs(bgi_get_xfont_procs);

/* The device descriptor */
typedef struct gx_device_bgi_s gx_device_bgi;
struct gx_device_bgi_s {
	gx_device_common;
	int display_mode;
	struct text_info text_mode;
};
#define bgi_dev ((gx_device_bgi *)dev)
private gx_device_procs bgi_procs = {
	bgi_open,
	NULL,			/* get_initial_matrix */
	NULL,			/* sync_output */
	NULL,			/* output_page */
	bgi_close,
	bgi_map_rgb_color,
	bgi_map_color_rgb,
	bgi_fill_rectangle,
	bgi_tile_rectangle,
	bgi_copy_mono,
	bgi_copy_color,
	bgi_draw_line,
	NULL,			/* get_bits */
	NULL,			/* get_props */
	NULL,			/* put_props */
	NULL,
	bgi_get_xfont_procs
};
gx_device_bgi far_data gs_bgi_device = {
	std_device_std_body(gx_device_bgi, &bgi_procs, "bgi",
	  0, 0,		/* width and height are set in bgi_open */
	  1, 1		/* density is set in bgi_open */
	)
};

/* Detection procedure for user-defined driver. */
private int huge
detectVGA(void)
{	return gs_bgi_device.display_mode;
}

/* Open the BGI driver for graphics mode */
int
bgi_open(gx_device *dev)
{	int driver, mode;
	char *bgi_user = getenv("BGIUSER");
	char *bgi_path = getenv("BGIPATH");

	gettextinfo(&bgi_dev->text_mode);

	if ( bgi_path == NULL )
		bgi_path = BGI_LIB;
	if ( bgi_user != NULL )
	   {	/* A user-supplied driver is specified as "mode.dname", */
		/* where mode is a hex number and dname is the name */
		/* of the driver file. */
		char dname[40];
		if ( strlen(bgi_user) > sizeof(dname) ||
		     sscanf(bgi_user, "%x.%s", &mode, dname) != 2
		   )
		   {	eprintf("BGIUSER not in form nn.dname.\n");
			exit(1);
		   }
		gs_bgi_device.display_mode = mode;	/* sigh.... */
		installuserdriver(dname, detectVGA);
		driver = DETECT;
		initgraph(&driver, &mode, bgi_path);
		driver = SUPER_VGA;
	   }
	else				/* not user-defined driver */
	   {	/* We include the CGA driver in the executable, */
		/* so end-users don't have to have the BGI files. */
		if ( registerfarbgidriver(CGA_driver_far) < 0 )
		   {	eprintf("BGI: Can't register CGA driver!\n");
			exit(1);
		   }

		detectgraph(&driver, &mode);
		if ( driver < 0 )
		   {	eprintf("BGI: No graphics hardware detected!\n");
			exit(1);
		   }

		if ( driver == EGA64 )
		   {	/* Select 16 color video mode if video card is EGA with 64 Kb of memory */
			mode = EGA64LO;
		   }

		/* Initialize graphics mode. */

		/* Following patch for AT&T 6300 is courtesy of */
		/* Allan Wax, Xerox Corp. */
		if ( driver == CGA )
		   {	/* The actual hardware might be an AT&T 6300. */
			/* Try initializing it that way. */
			int save_mode = mode;
			driver = ATT400, mode = ATT400HI;
			initgraph(&driver, &mode, bgi_path);
			if ( graphresult() != grOk )
			   {	/* Nope, it was a real CGA. */
				closegraph();
				driver = CGA, mode = save_mode;
				initgraph(&driver, &mode, bgi_path);
			   }
		   }
		else
			initgraph(&driver, &mode, bgi_path);
	   }

	   {	int code = graphresult();
		if ( code != grOk )
		   {	eprintf1("Error initializing BGI driver: %s\n",
				 grapherrormsg(code));
			exit(1);
		   }
	   }

	setactivepage(1);
	setvisualpage(1);
	/* Set parameters that were unknown before opening device */

	/* Size and nominal density of screen. */
	/* The following algorithm maps an appropriate fraction of */
	/* the display screen to an 8.5" x 11" coordinate space. */
	/* This may or may not be what is desired! */
	if ( dev->width == 0 )
		dev->width = getmaxx() + 1;
	if ( dev->height == 0 )
		dev->height = getmaxy() + 1;
	if ( dev->y_pixels_per_inch == 1 )
	   {	/* Get the aspect ratio from the driver. */
		int arx, ary;
		getaspectratio(&arx, &ary);
		dev->y_pixels_per_inch = dev->height / 11.0;
		dev->x_pixels_per_inch =
			dev->y_pixels_per_inch * ((float)ary / arx);
	   }

	/* Find out if the device supports color */
	/* (default initialization is monochrome). */
	/* We only recognize 16-color devices right now. */
	if ( getmaxcolor() > 1 )
	   {	static gx_device_color_info bgi_16color = dci_color(4, 2, 3);
		dev->color_info = bgi_16color;
	   }
	return 0;
}

/* Close the BGI driver */
int
bgi_close(gx_device *dev)
{	closegraph();
	textmode(bgi_dev->text_mode.currmode);
	return 0;
}

/* Map a r-g-b color to the 16 colors available with an EGA/VGA video card. */
gx_color_index
bgi_map_rgb_color(gx_device *dev, gx_color_value r, gx_color_value g,
  gx_color_value b)
{	return (gx_color_index)
		((r > gx_max_color_value / 4 ? 4 : 0) +
		 (g > gx_max_color_value / 4 ? 2 : 0) +
		 (b > gx_max_color_value / 4 ? 1 : 0) +
		 (r > gx_max_color_value / 4 * 3 ||
		  g > gx_max_color_value / 4 * 3 ? 8 : 0));
}

/* Map a color code to r-g-b.  Surprisingly enough, this is algorithmic. */
int
bgi_map_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{
#define icolor (int)color
	gx_color_value one =
		(icolor & 8 ? gx_max_color_value : gx_max_color_value / 3);
	prgb[0] = (icolor & 4 ? one : 0);
	prgb[1] = (icolor & 2 ? one : 0);
	prgb[2] = (icolor & 1 ? one : 0);
	return 0;
#undef icolor
}

/* Copy a monochrome bitmap.  The colors are given explicitly. */
/* Color = gx_no_color_index means transparent (no effect on the image). */
int
bgi_copy_mono(gx_device *dev,
  const byte *base, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h,
  gx_color_index zero, gx_color_index one)
{	const byte *ptr_line = base + (sourcex >> 3);
	int left_bit = 0x80 >> (sourcex & 7);
	int dest_y = y, end_x = x + w;
	int invert = 0;
	int color;

	if ( zero == gx_no_color_index )
	   {	if ( one == gx_no_color_index ) return 0;
		color = (int)one;
	   }
	else
	   {	if ( one == gx_no_color_index )
		   {	color = (int)zero;
			invert = -1;
		   }
		else
		   {	/* Pre-clear the rectangle to zero */
			setfillstyle(SOLID_FILL, (int)zero);
			bar(x, y, x + w - 1, y + h - 1);
			color = (int)one;
		   }
	   }

	while ( h-- )              /* for each line */
	   {	const byte *ptr_source = ptr_line;
		register int dest_x = x;
		register int bit = left_bit;
		while ( dest_x < end_x )     /* for each bit in the line */
		   {	if ( (*ptr_source ^ invert) & bit )
				putpixel(dest_x, dest_y, color);
			dest_x++;
			if ( (bit >>= 1) == 0 )
				bit = 0x80, ptr_source++;
		   }
		dest_y++;
		ptr_line += raster;
	   }
	return 0;
}


/* Copy a color pixel map.  This is just like a bitmap, except that */
/* each pixel takes 4 bits instead of 1 when device driver has color. */
int
bgi_copy_color(gx_device *dev,
  const byte *base, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h)
{	if ( gx_device_has_color(dev) )
	   {	/* color device, four bits per pixel */
		const byte *line = base + (sourcex >> 1);
		int dest_y = y, end_x = x + w;

		if ( w <= 0 ) return 0;
		while ( h-- )              /* for each line */
		   {	const byte *source = line;
			register int dest_x = x;
			if ( sourcex & 1 )    /* odd nibble first */
			   {	int color =  *source++ & 0xf;
				putpixel(dest_x, dest_y, color);
				dest_x++;
			   }
			/* Now do full bytes */
			while ( dest_x < end_x )
			   {	int color = *source >> 4;
				putpixel(dest_x, dest_y, color);
				dest_x++;
				if ( dest_x < end_x )
				   {	color =  *source++ & 0xf;
					putpixel(dest_x, dest_y, color);
					dest_x++;
				   }
			   }
			dest_y++;
			line += raster;
		   }
	   }
	else /* monochrome device: one bit per pixel */
	   {	/* bitmap is the same as bgi_copy_mono: one bit per pixel */
		bgi_copy_mono(dev, base, sourcex, raster, id, x, y, w, h,
			(gx_color_index)BLACK, (gx_color_index)WHITE);
	   }
	return 0;
}


/* Fill a rectangle. */
int
bgi_fill_rectangle(gx_device *dev, int x, int y, int w, int h,
  gx_color_index color)
{	setfillstyle(SOLID_FILL, (int)color);
	bar(x, y, x + w - 1, y + h - 1);
	return 0;
}


/* Tile a rectangle.  If neither color is transparent, */
/* pre-clear the rectangle to color0 and just tile with color1. */
/* This is faster because of how bgi_copy_mono is implemented. */
/* Note that this also does the right thing for colored tiles. */
int
bgi_tile_rectangle(gx_device *dev, const gx_tile_bitmap *tile,
  int x, int y, int w, int h, gx_color_index czero, gx_color_index cone,
  int px, int py)
{	int tw = tile->size.x, th = tile->size.y;
	int rw, rh, tx, ty;
	int code;
	byte image[4+4+256];
	if ( w >> 1 < tw || h >> 1 < th ||
	     czero == gx_no_color_index || cone == gx_no_color_index ||
	     imagesize(x, y, x + tw - 1, y + th - 1) > sizeof(image)
	   )
	{	if ( czero != gx_no_color_index && cone != gx_no_color_index )
		{	bgi_fill_rectangle(dev, x, y, w, h, czero);
			czero = gx_no_color_index;
		}
		return gx_default_tile_rectangle(dev, tile, x, y, w, h, czero, cone, px, py);
	}
	/* Handle edge strips.  We know w and h are both large. */
	rh = h % th;
	if ( rh )
	{	code = gx_default_tile_rectangle(dev, tile, x, y + h - rh, w, rh, czero, cone, px, py);
		if ( code < 0 ) return code;
		h -= rh;
	}
	rw = w % tw;
	if ( rw )
	{	code = gx_default_tile_rectangle(dev, tile, x + w - rw, y, rw, h, czero, cone, px, py);
		if ( code < 0 ) return code;
		w -= rw;
	}
	/* Now w and h are multiples of tw and th respectively, */
	/* and greater than 1.  Start by doing one tile the slow way. */
	code = gx_default_tile_rectangle(dev, tile, x, y, tw, th, czero, cone, px, py);
	if ( code < 0 ) return code;
	/* Now replicate the tile. */
	getimage(x, y, x + tw - 1, y + th - 1, &image[0]);
	for ( ty = h; (ty -= th) >= 0; )
	{	for ( tx = w; (tx -= tw) >= 0; )
			putimage(x + tx, y + ty, &image[0], COPY_PUT);
	}
	return 0;
}


/* Draw a line */
int
bgi_draw_line(gx_device *dev, int x0, int y0, int x1, int y1,
  gx_color_index color)
{	setcolor((int)color);
	setlinestyle(SOLID_LINE, 0, NORM_WIDTH);  /* solid, one pixel wide */
	line(x0, y0, x1, y1);
	return 0;
}

/* ------ Platform font procedures ------ */

/*
 * Note: Stroked BGI fonts lie about their height and baseline:
 * the textheight is actually the position of the baseline,
 * and the only way to find the actual height is to scan the bits.
 */

/* Define xfont procedures. */
private xfont_proc_lookup_font(bgi_lookup_font);
private xfont_proc_char_xglyph(bgi_char_xglyph);
private xfont_proc_char_metrics(bgi_char_metrics);
private xfont_proc_render_char(bgi_render_char);
private xfont_proc_release(bgi_release);
private gx_xfont_procs bgi_xfont_procs = {
	bgi_lookup_font,
	bgi_char_xglyph,
	bgi_char_metrics,
	bgi_render_char,
	bgi_release
};

/* Return the xfont procedure record. */
gx_xfont_procs *
bgi_get_xfont_procs(gx_device *dev)
{	return &bgi_xfont_procs;
}

/* Define a BGI xfont. */
typedef struct bgi_xfont_s {
	gx_xfont_common common;
	const char _ds *fname;
	int index;			/* BGI font index */
	gs_int_point ratio;		/* scaling ratio for character */
	int base_size;			/* height of 4x 'A' */
	int baseline;
} bgi_xfont;
gs_private_st_simple(st_bgi_xfont, bgi_xfont, "bgi_xfont");

/* Different versions of Turbo C and Borland C++ have different fonts.... */
private bgi_xfont all_fonts[] = {
	{ { &bgi_xfont_procs}, "Courier", DEFAULT_FONT },
#ifdef SANSSERIF_FONT
	{ { &bgi_xfont_procs}, "Helvetica", SANSSERIF_FONT },
#else
# ifdef SANS_SERIF_FONT
	{ { &bgi_xfont_procs}, "Helvetica", SANS_SERIF_FONT },
# endif
#endif
#ifdef SIMPLEX_FONT
	{ { &bgi_xfont_procs}, "Times-Roman", SIMPLEX_FONT },
#endif
#ifdef BOLD_FONT
	{ { &bgi_xfont_procs}, "Times-Bold", BOLD_FONT }
#endif
};

/* Keep a record of a character temporarily rendered to the screen. */
typedef struct char_image_s {
	char str[2];
	gs_int_point size;
	uint image_size;
	char *image;
} char_image;

/* Forward references */
private bgi_xfont *char_set_font(P1(gx_xfont *));
private int char_set_image(P2(byte, char_image *));
private void char_bbox(P3(bgi_xfont *, char_image *, gs_int_rect *));
private void char_restore_image(P1(char_image *));

/* Look up a font. */
gx_xfont *
bgi_lookup_font(gx_device *dev, const byte *fname, uint len,
  int encoding_index, const gs_uid *puid, const gs_matrix *pmat,
  gs_memory_t *mem)
{	float px, py;
	int i;
	if ( pmat->xy != 0 || pmat->yx != 0 || pmat->xx <= 0 || pmat->yy >= 0 )
		return NULL;	/* unsupported transformation */
	px = pmat->xx * 1000;
	py = pmat->yy * -1000;
	for ( i = 0; i < countof(all_fonts); i++ )
	{	bgi_xfont *pf = &all_fonts[i];
		if ( strlen(pf->fname) == len && !memcmp(pf->fname, fname, len) )
		{	long rx, ry;
			bgi_xfont *spf;
			settextstyle(pf->index, HORIZ_DIR, 1);
			if ( pf->base_size == 0 )	/* not set yet */
			{	setusercharsize(1, 1, 1, 1);
				pf->base_size = textheight("A");
			}
			rx = px * 64 / pf->base_size;
			ry = py * 64 / pf->base_size;
			if ( rx <= 0 || ry <= 0 )
				return NULL;
			spf = gs_alloc_struct(mem, bgi_xfont,
					&st_bgi_xfont, "bgi_lookup_font");
			if ( spf == 0 )
				return NULL;
			*spf = *pf;
			spf->ratio.x = rx;
			spf->ratio.y = ry;
			char_set_font((gx_xfont *)spf);
			spf->baseline = textheight("A");  /* (see above) */
			return (gx_xfont *)spf;
		}
	}
	return NULL;		/* unsupported font name */
}

/* Convert a character name or index to an xglyph code. */
gx_xglyph
bgi_char_xglyph(gx_xfont *xf, gs_char chr, int encoding_index,
  gs_glyph glyph, gs_proc_glyph_name_t glyph_name_proc)
{	if ( (encoding_index & ~1) != 0 ||  /* Standard & ISOLatin1 only */
	     chr < 32 || chr > 126	/* ASCII only */
	   )
		return gx_no_xglyph;
	return chr;
}

/* Get the metrics for a character. */
int
bgi_char_metrics(gx_xfont *xf, gx_xglyph xg, int wmode,
  gs_point *pwidth, gs_int_rect *pbbox)
{	bgi_xfont *pf = char_set_font(xf);
	char_image ci;
	if ( wmode != 0 )
		return gs_error_undefined;
	if ( char_set_image((byte)xg, &ci) < 0 )
		return gs_error_ioerror;
	char_bbox(pf, &ci, pbbox);
	pwidth->x = textwidth(ci.str);
	pwidth->y = 0;
	if ( pwidth->x == pbbox->q.x && pbbox->p.x == 0 )
	{	/* This is a badly designed font with no inter-character */
		/* spacing.  Add 1 pixel. */
		pwidth->x++;
	}
	char_restore_image(&ci);
	return 0;
}

/* Render a character. */
int
bgi_render_char(gx_xfont *xf, gx_xglyph xg, gx_device *target,
  int xo, int yo, gx_color_index color, int required)
{	bgi_xfont *pf = char_set_font(xf);
	char_image ci;
	gs_int_rect bbox;
	int xi, yi;
	if ( target->dname == gs_bgi_device.dname )
	{	/* Write directly to a BGI device. */
		ci.str[0] = (char)xg;
		ci.str[1] = 0;
		setcolor((int)color);
		outtextxy(xo, yo - pf->baseline, ci.str);
		return 0;
	}
	if ( !required )
		return gs_error_ioerror;	/* any error will do */
	if ( char_set_image((byte)xg, &ci) < 0 )
		return gs_error_ioerror;
	char_bbox(pf, &ci, &bbox);
	for ( yi = bbox.p.y; yi < bbox.q.y; yi++ )
	{	for ( xi = bbox.p.x; xi < bbox.q.x; xi++ )
		  if ( getpixel(xi, yi + pf->baseline) != WHITE )
		{	(*dev_proc(target, fill_rectangle))(target,
				xi + xo, yi + yo, 1, 1, color);
		}
	}
	char_restore_image(&ci);
	return 0;
}

/* Release an xfont. */
private int
bgi_release(gx_xfont *xf, gs_memory_t *mem)
{	if ( mem != NULL )
		gs_free_object(mem, xf, "bgi_release");
	return 0;
}

/* ------ Font utilities ------ */

/* Set up a font. */
private bgi_xfont *
char_set_font(gx_xfont *xf)
{	bgi_xfont *pf = (bgi_xfont *)xf;
	settextstyle(pf->index, HORIZ_DIR, 0);
	setusercharsize(pf->ratio.x, 64, pf->ratio.y, 64);
	return pf;
}

/* Write a character onto the screen. */
private int
char_set_image(byte ch, char_image *pci)
{	char *str = pci->str;
	int w, h;
	uint size;
	char *image;
	str[0] = ch;
	str[1] = 0;
	w = textwidth(str);
	h = textheight(str) << 1;	/* (see above) */
	if ( w != 0 && h != 0 )
	{	size = imagesize(0, 0, w - 1, h - 1);
		image = malloc(size);
		if ( image == 0 ) return -1;	/* allocation failed */
		getimage(0, 0, w - 1, h - 1, image);
		setfillstyle(SOLID_FILL, WHITE);
		bar(0, 0, w - 1, h - 1);		/* clear */
		setcolor(BLACK);
		outtextxy(0, 0, str);
	}
	else
	{	size = 0;
		image = 0;
	}
	pci->size.x = w;
	pci->size.y = h;
	pci->image_size = size;
	pci->image = image;
	return 0;
}

/* Find the bounding box of a character. */
/* The character is already on the screen. */
private void
char_bbox(bgi_xfont *pf, char_image *pci, gs_int_rect *pbbox)
{	int x0 = pci->size.x, y0 = pci->size.y, x1 = -1, y1 = 0;
	int x, y;
	int base = pf->baseline;
	for ( y = pci->size.y; --y >= 0; )
	  for ( x = pci->size.x; --x >= 0; )
	    if ( getpixel(x, y) != WHITE )
	{	if ( x < x0 ) x0 = x;
		if ( x > x1 ) x1 = x;
		if ( y < y0 ) y0 = y;
		if ( y > y1 ) y1 = y;
	}
	if ( x0 > x1 )		/* blank */
		pbbox->p.x = pbbox->q.x = pbbox->p.y = pbbox->q.y = 0;
	else
	{	pbbox->p.x = x0;
		pbbox->p.y = y0 - base;
		pbbox->q.x = x1 + 1;
		pbbox->q.y = y1 + 1 - base;
	}
}

/* Restore the image under the character. */
private void
char_restore_image(char_image *pci)
{	if ( pci->image != 0 )		/* might have been empty */
	{	putimage(0, 0, pci->image, COPY_PUT);
		free(pci->image);
	}
}
