/* Copyright (C) 1993 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevxxf.c */
/* External font (xfont) implementation for X11. */
#include "math_.h"
#include "memory_.h"
#include "x_.h"
#include "gx.h"
#include "gxdevice.h"
#include "gdevx.h"
#include "gsstruct.h"
#include "gsutil.h"
#include "gserrors.h"
#include "gxxfont.h"

/* Define the smallest point size that we trust X to render reasonably well. */
#define min_X_font_size 6
/* Define the largest point size where X will do a better job than we can. */
#define max_X_font_size 35

extern gx_device_X gs_x11_device;

extern const byte gs_map_std_to_iso[256];
extern const byte gs_map_iso_to_std[256];

/* Declare the xfont procedures */
private xfont_proc_lookup_font(x_lookup_font);
private xfont_proc_char_xglyph(x_char_xglyph);
private xfont_proc_char_metrics(x_char_metrics);
private xfont_proc_render_char(x_render_char);
private xfont_proc_release(x_release);
private gx_xfont_procs x_xfont_procs = {
	x_lookup_font,
	x_char_xglyph,
	x_char_metrics,
	x_render_char,
	x_release
};

/* Return the xfont procedure record. */
gx_xfont_procs *
x_get_xfont_procs(gx_device *dev)
{
    return &x_xfont_procs;
}

/* Define a X11 xfont. */
typedef struct x_xfont_s x_xfont;
struct x_xfont_s {
      gx_xfont_common common;
      gx_device_X *xdev;
      XFontStruct *font;
      int encoding_index;
      int My;
      int angle;
};
gs_private_st_dev_ptrs1(st_x_xfont, x_xfont, "x_xfont",
  x_xfont_enum_ptrs, x_xfont_reloc_ptrs, xdev);

/* Look up a font. */
private gx_xfont *
x_lookup_font(gx_device *dev, const byte *fname, uint len,
	      int encoding_index, const gs_uid *puid, const gs_matrix *pmat,
	      gs_memory_t *mem)
{
    gx_device_X *xdev = (gx_device_X *) dev;
    x_xfont *xxf;
    char x11template[256];
    char *x11fontname = NULL;
    XFontStruct *x11font;
    x11fontmap *fmp;
    int i;
    double height;
    int size;
    int xwidth, xheight, angle;
    Boolean My;
    Boolean scalable_font = False;

    if (!xdev->useXFonts) return NULL;

    if (pmat->xy == 0 && pmat->yx == 0) {
	xwidth  = fabs(pmat->xx * 1000) + 0.5;
	xheight = fabs(pmat->yy * 1000) + 0.5;
	height  = fabs(pmat->yy * 1000);
	if (pmat->xx > 0) angle = 0;
	else angle = 180;
	My = (pmat->xx > 0 && pmat->yy > 0) || (pmat->xx < 0 && pmat->yy < 0);
    } else if (pmat->xx == 0 && pmat->yy == 0) {
	xwidth  = fabs(pmat->xy * 1000) + 0.5;
	xheight = fabs(pmat->yx * 1000) + 0.5;
	height  = fabs(pmat->yx * 1000);
	if (pmat->yx < 0) angle = 90;
	else angle = 270;
	My = (pmat->yx > 0 && pmat->xy < 0) || (pmat->yx < 0 && pmat->xy > 0);
    } else {
	return NULL;
    }

    /* Don't do very small fonts, where font metrics are way off */
    /* due to rounding and the server does a very bad job of scaling, */
    /* or very large fonts, where we can do just as good a job and */
    /* the server may lock up the entire window system while rasterizing */
    /* the whole font. */
    if ( xwidth < min_X_font_size || xwidth > max_X_font_size ||
	 xheight < min_X_font_size || xheight > max_X_font_size
       )
      return NULL;

    if (!xdev->useFontExtensions && (My || angle != 0)) return NULL;

    if (encoding_index == 0 || encoding_index == 1) {
	int tried_other_encoding = 0;

	fmp = xdev->regular_fonts;
	while (fmp) {
	    if (len == strlen(fmp->ps_name) &&
		strncmp(fmp->ps_name, (const char *)fname, len) == 0) break;
	    fmp = fmp->next;
	}
	if (fmp == NULL) return NULL;
	while (True) {
	    if (encoding_index == 0) {
		if (fmp->std_count == -1) {
		    sprintf(x11template, "%s%s", fmp->x11_name,
			    "-*-*-*-*-*-*-Adobe-fontspecific");
		    fmp->std_names = XListFonts(xdev->dpy, x11template, 32,
						&fmp->std_count);
		}
		if (fmp->std_count) {
		    for (i = 0; i < fmp->std_count; i++) {
			char *szp = fmp->std_names[i] + strlen(fmp->x11_name)+1;

			size = 0;
			while (*szp >= '0' && *szp <= '9')
			    size = size * 10 + *szp++ - '0';
			if (size == 0) {
			    scalable_font = True;
			    continue;
			}
			if (size == xheight) {
			    x11fontname = fmp->std_names[i];
			    break;
			}
		    }
		    if (!x11fontname && scalable_font &&
			xdev->useScalableFonts) {
			sprintf(x11template, "%s-%d%s", fmp->x11_name,
				xheight, "-0-0-0-*-0-Adobe-fontspecific");
			x11fontname = x11template;
		    }
		    if (x11fontname) break;
		}
		if (tried_other_encoding) return NULL;
		encoding_index = 1;
		tried_other_encoding = 1;
	    } else if (encoding_index == 1) {
		if (fmp->iso_count == -1) {
		    sprintf(x11template, "%s%s", fmp->x11_name,
			    "-*-*-*-*-*-*-ISO8859-1");
		    fmp->iso_names = XListFonts(xdev->dpy, x11template, 32,
						&fmp->iso_count);
		}
		if (fmp->iso_count) {
		    for (i = 0; i < fmp->iso_count; i++) {
			char *szp = fmp->iso_names[i] + strlen(fmp->x11_name)+1;

			size = 0;
			while (*szp >= '0' && *szp <= '9')
			    size = size * 10 + *szp++ - '0';
			if (size == 0) {
			    scalable_font = True;
			    continue;
			}
			if (size == xheight) {
			    x11fontname = fmp->iso_names[i];
			    break;
			}
		    }
		    if (!x11fontname && scalable_font &&
			xdev->useScalableFonts) {
			sprintf(x11template, "%s-%d%s", fmp->x11_name,
				xheight, "-0-0-0-*-0-ISO8859-1");
			x11fontname = x11template;
		    }
		    if (x11fontname) break;
		}
		if (tried_other_encoding) return NULL;
		encoding_index = 0;
		tried_other_encoding = 1;
	    }
	}
    } else if (encoding_index == 2 || encoding_index == 3) {
	if (encoding_index == 2) fmp = xdev->symbol_fonts;
	if (encoding_index == 3) fmp = xdev->dingbat_fonts;
	while (fmp) {
	    if (len == strlen(fmp->ps_name) &&
		strncmp(fmp->ps_name, (const char *)fname, len) == 0)
		break;
	    fmp = fmp->next;
	}
	if (fmp == NULL) return NULL;
	if (fmp->std_count == -1) {
	    sprintf(x11template, "%s%s", fmp->x11_name,
		    "-*-*-*-*-*-*-Adobe-fontspecific");
	    fmp->std_names = XListFonts(xdev->dpy, x11template, 32,
					&fmp->std_count);
	}
	if (fmp->std_count) {
	    for (i = 0; i < fmp->std_count; i++) {
		char *szp = fmp->std_names[i] + strlen(fmp->x11_name)+1;

		size = 0;
		while (*szp >= '0' && *szp <= '9')
		    size = size * 10 + *szp++ - '0';
		if (size == 0) {
		    scalable_font = True;
		    continue;
		}
		if (size == xheight) {
		    x11fontname = fmp->std_names[i];
		    break;
		}
	    }
	    if (!x11fontname && scalable_font && xdev->useScalableFonts) {
		sprintf(x11template, "%s-%d%s", fmp->x11_name, xheight,
			"-0-0-0-*-0-Adobe-fontspecific");
		x11fontname = x11template;
	    }
	    if (!x11fontname) return NULL;
	} else {
	    return NULL;
	}
    } else {
	return NULL;
    }

    if (xwidth != xheight || angle != 0 || My) {
	if (!xdev->useScalableFonts || !scalable_font) return NULL;
	sprintf(x11template, "%s%s+%d-%d+%d%s",
		fmp->x11_name, My ? "+My" : "",
		angle*64 , xheight, xwidth,
		encoding_index == 1 ? "-0-0-0-*-0-ISO8859-1" :
		"-0-0-0-*-0-Adobe-fontspecific");
	x11fontname = x11template;
    }

    x11font = XLoadQueryFont(xdev->dpy, x11fontname);
    if (x11font == NULL)
	return NULL;
    /* Don't bother with 16-bit or 2 byte fonts yet */
    if (x11font->min_byte1 || x11font->max_byte1) {
	XFreeFont(xdev->dpy, x11font);
	return NULL;
    }
    xxf = gs_alloc_struct(mem, x_xfont, &st_x_xfont, "x_lookup_font");
    if (xxf == NULL)
	return NULL;
    xxf->common.procs = &x_xfont_procs;
    xxf->xdev = xdev;
    xxf->font = x11font;
    xxf->encoding_index = encoding_index;
    xxf->My = My ? -1 : 1;
    xxf->angle = angle;
    if (xdev->logXFonts) {
	fprintf(stdout, "Using %s\n", x11fontname);
	fprintf(stdout, "  for %s at %g pixels.\n", fmp->ps_name, height);
	fflush(stdout);
    }
    return (gx_xfont *) xxf;
}

/* Convert a character name or index to an xglyph code. */
private gx_xglyph
x_char_xglyph(gx_xfont *xf, gs_char chr, int encoding_index,
	      gs_glyph glyph, gs_proc_glyph_name_t glyph_name_proc)
{
    x_xfont *xxf = (x_xfont *) xf;

    if (chr == gs_no_char)
	return gx_no_xglyph;	/* can't look up names yet */
    if (encoding_index != xxf->encoding_index) {
	if (encoding_index == 0 && xxf->encoding_index == 1)
	    chr = gs_map_std_to_iso[chr];
	else if (encoding_index == 1 && xxf->encoding_index == 0)
	    chr = gs_map_iso_to_std[chr];
	else
	    return gx_no_xglyph;
	if (chr == 0) return gx_no_xglyph;
    }
    if (chr < xxf->font->min_char_or_byte2 ||
	chr > xxf->font->max_char_or_byte2)
	return gx_no_xglyph;
    if (xxf->font->per_char) {
	int i = chr - xxf->font->min_char_or_byte2;
	XCharStruct *xc = &(xxf->font->per_char[i]);

	if ((xc->lbearing == 0) && (xc->rbearing == 0) &&
	    (xc->ascent == 0) && (xc->descent == 0))
	    return gx_no_xglyph;
    }
    return (gx_xglyph) chr;
}

/* Get the metrics for a character. */
private int
x_char_metrics(gx_xfont *xf, gx_xglyph xg, int wmode,
	       gs_point *pwidth, gs_int_rect *pbbox)
{
    x_xfont *xxf = (x_xfont *) xf;

    if (wmode != 0)
	return gs_error_undefined;
    if (xxf->font->per_char == NULL) {
	if (xxf->angle == 0) {
	    pwidth->x = xxf->font->max_bounds.width;
	    pwidth->y = 0;
	} else if (xxf->angle == 90) {
	    pwidth->x = 0;
	    pwidth->y = -xxf->My * xxf->font->max_bounds.width;
	} else if (xxf->angle == 180) {
	    pwidth->x = -xxf->font->max_bounds.width;
	    pwidth->y = 0;
	} else if (xxf->angle == 270) {
	    pwidth->x = 0;
	    pwidth->y = xxf->My * xxf->font->max_bounds.width;
	}
	pbbox->p.x = xxf->font->max_bounds.lbearing;
	pbbox->q.x = xxf->font->max_bounds.rbearing;
	pbbox->p.y = -xxf->font->max_bounds.ascent;
	pbbox->q.y = xxf->font->max_bounds.descent;
    } else {
	int i = xg - xxf->font->min_char_or_byte2;

	if (xxf->angle == 0) {
	    pwidth->x = xxf->font->per_char[i].width;
	    pwidth->y = 0;
	} else if (xxf->angle == 90) {
	    pwidth->x = 0;
	    pwidth->y = -xxf->My * xxf->font->per_char[i].width;
	} else if (xxf->angle == 180) {
	    pwidth->x = -xxf->font->per_char[i].width;
	    pwidth->y = 0;
	} else if (xxf->angle == 270) {
	    pwidth->x = 0;
	    pwidth->y = xxf->My * xxf->font->per_char[i].width;
	}
	pbbox->p.x = xxf->font->per_char[i].lbearing;
	pbbox->q.x = xxf->font->per_char[i].rbearing;
	pbbox->p.y = -xxf->font->per_char[i].ascent;
	pbbox->q.y = xxf->font->per_char[i].descent;
    }
    return 0;
}

/* Render a character. */
private int
x_render_char(gx_xfont *xf, gx_xglyph xg, gx_device *dev,
	      int xo, int yo, gx_color_index color, int required)
{
    x_xfont *xxf = (x_xfont *) xf;
    gx_device_X *xdev = xxf->xdev;
    char chr = (char)xg;
    gs_point wxy;
    gs_int_rect bbox;
    int x, y, w, h;
    int code;

    if (dev->dname == gs_x11_device.dname) {
	code = (*xf->common.procs->char_metrics) (xf, xg, 0, &wxy, &bbox);
	if (code < 0) return code;
	set_fill_style(FillSolid);
	set_fore_color(color);
	set_function(GXcopy);
	set_font(xxf->font->fid);
	XDrawString(xdev->dpy, xdev->dest, xdev->gc, xo, yo, &chr, 1);
	if (xdev->bpixmap != (Pixmap) 0) {
	    x = xo + bbox.p.x;
	    y = yo + bbox.p.y;
	    w = bbox.q.x - bbox.p.x;
	    h = bbox.q.y - bbox.p.y;
	    fit_fill(dev, x, y, w, h);
	    x_update_add(dev, x, y, w, h);
	}
	return 0;
    } else if (!required)
	return -1;	/* too hard */
    else {
	/* Display on an intermediate bitmap, then copy the bits. */
	int wbm, raster;
	int i;
	XImage *xim;
	Pixmap xpm;
	GC fgc;
	byte *bits;

	dev_proc_copy_mono((*copy_mono)) = dev_proc(dev, copy_mono);

	code = (*xf->common.procs->char_metrics) (xf, xg, 0, &wxy, &bbox);
	if (code < 0) return code;
	w = bbox.q.x - bbox.p.x;
	h = bbox.q.y - bbox.p.y;
	wbm = round_up(w, align_bitmap_mod * 8);
	raster = wbm >> 3;
	bits = (byte *) gs_malloc(h, raster, "x_render_char");
	if (bits == 0) return gs_error_limitcheck;
	xpm = XCreatePixmap(xdev->dpy, xdev->win, w, h, 1);
	fgc = XCreateGC(xdev->dpy, xpm, None, NULL);
	XSetForeground(xdev->dpy, fgc, 0);
	XFillRectangle(xdev->dpy, xpm, fgc, 0, 0, w, h);
	XSetForeground(xdev->dpy, fgc, 1);
	XSetFont(xdev->dpy, fgc, xxf->font->fid);
	XDrawString(xdev->dpy, xpm, fgc, -bbox.p.x, -bbox.p.y, &chr, 1);
	xim = XGetImage(xdev->dpy, xpm, 0, 0, w, h, 1, ZPixmap);
	i = 0;
	for (y = 0; y < h; y++) {
	    char b = 0;

	    for (x = 0; x < wbm; x++) {
		b = b << 1;
		if (x < w)
		    b += XGetPixel(xim, x, y);
		if ((x & 7) == 7)
		    bits[i++] = b;
	    }
	}
	code = (*copy_mono) (dev, bits, 0, raster, gx_no_bitmap_id,
			     xo + bbox.p.x, yo + bbox.p.y, w, h,
			     gx_no_color_index, color);
	gs_free((char *)bits, h, raster, "x_render_char");
	XFreePixmap(xdev->dpy, xpm);
	XFreeGC(xdev->dpy, fgc);
	XDestroyImage(xim);
	return (code < 0 ? code : 0);
    }
}

/* Release an xfont. */
private int
x_release(gx_xfont *xf, gs_memory_t *mem)
{
#if 0
    /* The device may not be open.  Cannot reliably free the font. */
    x_xfont *xxf = (x_xfont *) xf;
    XFreeFont(xxf->xdev->dpy, xxf->font);
#endif
    if (mem != NULL)
	gs_free_object(mem, xf, "x_release");
    return 0;
}
