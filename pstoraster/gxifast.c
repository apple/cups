/* Copyright (C) 1989, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
  to anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer
  to the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given
  to you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises supports the work of the GNU Project, but is not
  affiliated with the Free Software Foundation or the GNU Project.  GNU
  Ghostscript, as distributed by Aladdin Enterprises, does not require any
  GNU software to build or run it.
*/

/*$Id: gxifast.c,v 1.1 2000/03/08 23:15:01 mike Exp $ */
/* Fast monochrome image rendering */
#include "gx.h"
#include "memory_.h"
#include "gpcheck.h"
#include "gsbittab.h"
#include "gserrors.h"
#include "gxfixed.h"
#include "gxarith.h"
#include "gxmatrix.h"
#include "gsccolor.h"
#include "gspaint.h"
#include "gsutil.h"
#include "gxdevice.h"
#include "gxcmap.h"
#include "gxdcolor.h"
#include "gxistate.h"
#include "gzpath.h"
#include "gxdevmem.h"
#include "gdevmem.h"		/* for mem_mono_device */
#include "gxcpath.h"
#include "gximage.h"
#include "gzht.h"

/* Conditionally include statistics code. */
#ifdef DEBUG
#  define STATS
#endif

/* ------ Strategy procedure ------ */

/* Use special fast logic for portrait or landscape black-and-white images. */
private irender_proc(image_render_simple);
private irender_proc(image_render_landscape);
private irender_proc_t
image_strategy_simple(gx_image_enum * penum)
{
    irender_proc_t rproc;
    fixed ox = dda_current(penum->dda.pixel0.x);
    fixed oy = dda_current(penum->dda.pixel0.y);

    if (penum->use_rop || penum->spp != 1 || penum->bps != 1)
	return 0;
    switch (penum->posture) {
	case image_portrait:
	    {			/* Use fast portrait algorithm. */
		long dev_width =
		    fixed2long_pixround(ox + penum->x_extent.x) -
		    fixed2long_pixround(ox);

		if (dev_width != penum->rect.w) {
		    /*
		     * Add an extra align_bitmap_mod of padding so that
		     * we can align scaled rows with the device.
		     */
		    long line_size =
			bitmap_raster(any_abs(dev_width)) + align_bitmap_mod;

		    if (penum->adjust != 0 || line_size > max_uint)
			return 0;
		    /* Must buffer a scan line. */
		    penum->line_width = any_abs(dev_width);
		    penum->line_size = (uint) line_size;
		    penum->line = gs_alloc_bytes(penum->memory,
					    penum->line_size, "image line");
		    if (penum->line == 0) {
			gx_default_end_image(penum->dev,
					     (gx_image_enum_common_t *)penum,
					     false);
			return 0;
		    }
		}
		if_debug2('b', "[b]render=simple, unpack=copy; rect.w=%d, dev_width=%ld\n",
			  penum->rect.w, dev_width);
		rproc = image_render_simple;
		break;
	    }
	case image_landscape:
	    {			/* Use fast landscape algorithm. */
		long dev_width =
		    fixed2long_pixround(oy + penum->x_extent.y) -
		    fixed2long_pixround(oy);
		long line_size =
		    (dev_width = any_abs(dev_width),
		     bitmap_raster(dev_width) * 8 +
		     round_up(dev_width, 8) * align_bitmap_mod);

		if ((dev_width != penum->rect.w && penum->adjust != 0) ||
		    line_size > max_uint
		    )
		    return 0;
		/* Must buffer a group of 8N scan lines. */
		penum->line_width = dev_width;
		penum->line_size = (uint) line_size;
		penum->line = gs_alloc_bytes(penum->memory,
					     penum->line_size, "image line");
		if (penum->line == 0) {
		    gx_default_end_image(penum->dev,
					 (gx_image_enum_common_t *) penum,
					 false);
		    return 0;
		}
		penum->xi_next = penum->line_xy = fixed2int_var_rounded(ox);
		if_debug3('b', "[b]render=landscape, unpack=copy; rect.w=%d, dev_width=%ld, line_size=%ld\n",
			  penum->rect.w, dev_width, line_size);
		rproc = image_render_landscape;
		/* Precompute values needed for rasterizing. */
		penum->dxy =
		    float2fixed(penum->matrix.xy +
				fixed2float(fixed_epsilon) / 2);
		break;
	    }
	default:
	    return 0;
    }
    /* Precompute values needed for rasterizing. */
    penum->dxx =
	float2fixed(penum->matrix.xx + fixed2float(fixed_epsilon) / 2);
    /*
     * We don't want to spread the samples, but we have to reset unpack_bps
     * to prevent the buffer pointer from being incremented by 8 bytes per
     * input byte.
     */
    penum->unpack = sample_unpack_copy;
    penum->unpack_bps = 8;
    return rproc;
}

void
gs_gxifast_init(gs_memory_t * mem)
{
    image_strategies.simple = image_strategy_simple;
}

/* ------ Rendering procedures ------ */

/*
 * Scale (and possibly reverse) one scan line of a monobit image.
 * This is used for both portrait and landscape image processing.
 * We pass in an x offset (0 <= line_x < align_bitmap_mod * 8) so that
 * we can align the result with the eventual device X.
 *
 * To be precise, the input to this routine is the w bits starting at
 * bit data_x in buffer.  These w bits expand to abs(x_extent) bits,
 * either inverted (zero = 0xff) or not (zero = 0), starting at bit
 * line_x in line which corresponds to coordinate
 * fixed2int_pixround(xcur + min(x_extent, 0)).  Note that the entire
 * bytes containing the first and last output bits are affected: the
 * other bits in those bytes are set to zero (i.e., the value of the
 * 'zero' argument).
 */
#ifdef STATS
struct stats_image_fast_s {
    long
         calls, all0s, all1s, runs, lbit0, byte00, byte01, byte02, byte03,
         byte04, rbit0, lbit1, byte1, rbit1, thin, thin2, nwide, bwide,
         nfill, bfill;
} stats_image_fast;
#  define INCS(stat) ++stats_image_fast.stat
#  define ADDS(stat, n) stats_image_fast.stat += n
#else
#  define INCS(stat) DO_NOTHING
#  define ADDS(stat, n) DO_NOTHING
#endif
inline private void
fill_row(byte *line, int line_x, uint raster, int value)
{
    memset(line + (line_x >> 3), value, raster - (line_x >> 3));
}
private void
image_simple_expand(byte * line, int line_x, uint raster,
		    const byte * buffer, int data_x, uint w,
		    fixed xcur, fixed x_extent, byte zero /* 0 or 0xff */ )
{
    int dbitx = data_x & 7;
    byte sbit = 0x80 >> dbitx;
    byte sbitmask = 0xff >> dbitx;
    uint wx = dbitx + w;
    gx_dda_fixed xl;
    gx_dda_step_fixed dxx4, dxx8, dxx16, dxx24, dxx32;
    register const byte *psrc = buffer + (data_x >> 3);

    /*
     * The following 3 variables define the end of the input data row.
     * We would put them in a struct, except that no compiler that we
     * know of will optimize individual struct members as though they
     * were simple variables (e.g., by putting them in registers).
     *
     * endp points to the byte that contains the bit just beyond the
     * end of the row.  endx gives the bit number of this bit within
     * the byte, with 0 being the *least* significant bit.  endbit is
     * a mask for this bit.
     */
    const byte *endp = psrc + (wx >> 3);
    int endx = ~wx & 7;
    byte endbit = 1 << endx;

    /*
     * The following 3 variables do the same for start of the last run
     * of the input row (think of it as a pointer to just beyond the
     * end of the next-to-last run).
     */
    const byte *stop = endp;
    int stopx;
    byte stopbit = endbit;
    byte data;
    byte one = ~zero;
    fixed xl0;

    if (w == 0)
	return;
    INCS(calls);

    /* Scan backward for the last transition. */
    if (stopbit == 0x80)
	--stop, stopbit = 1;
    else
	stopbit <<= 1;
    /* Now (stop, stopbit) give the last bit of the row. */
    {
	byte stopmask = -stopbit << 1;
	byte last = *stop;

	if (stop == psrc)	/* only 1 input byte */
	    stopmask &= sbitmask;
	if (last & stopbit) {
	    /* The last bit is a 1: look for a 0-to-1 transition. */
	    if (~last & stopmask) {	/* Transition in last byte. */
		last |= stopbit - 1;
	    } else {		/* No transition in the last byte. */
		while (stop > psrc && stop[-1] == 0xff)
		    --stop;
		if (stop == psrc ||
		    (stop == psrc + 1 && !(~*psrc & sbitmask))
		    ) {
		    /* The input is all 1s.  Clear the row and exit. */
		    INCS(all1s);
		    fill_row(line, line_x, raster, one);
		    return;
		}
		last = *--stop;
	    }
	    stopx = byte_bit_run_length_0[byte_reverse_bits[last]] - 1;
	} else {
	    /* The last bit is a 0: look for a 1-to-0 transition. */
	    if (last & stopmask) {	/* Transition in last byte. */
		last &= -stopbit;
	    } else {		/* No transition in the last byte. */
		while (stop > psrc && stop[-1] == 0)
		    --stop;
		if (stop == psrc ||
		    (stop == psrc + 1 && !(*psrc & sbitmask))
		    ) {
		    /* The input is all 0s.  Clear the row and exit. */
		    INCS(all0s);
		    fill_row(line, line_x, raster, zero);
		    return;
		}
		last = *--stop;
	    }
	    stopx = byte_bit_run_length_0[byte_reverse_bits[last ^ 0xff]] - 1;
	}
	if (stopx < 0)
	    stopx = 7, ++stop;
	stopbit = 1 << stopx;
    }

    /* Pre-clear the row. */
    fill_row(line, line_x, raster, zero);

    /* Set up the DDAs. */
    xl0 =
	(x_extent >= 0 ?
	 fixed_fraction(fixed_pre_pixround(xcur)) :
	 fixed_fraction(fixed_pre_pixround(xcur + x_extent)) - x_extent);
    xl0 += int2fixed(line_x);
    dda_init(xl, xl0, x_extent, w);
    dxx4 = xl.step;
    dda_step_add(dxx4, xl.step);
    dda_step_add(dxx4, dxx4);
    dxx8 = dxx4;
    dda_step_add(dxx8, dxx4);
    dxx16 = dxx8;
    dda_step_add(dxx16, dxx8);
    dxx24 = dxx16;
    dda_step_add(dxx24, dxx8);
    dxx32 = dxx24;
    dda_step_add(dxx32, dxx8);

    /*
     * Loop invariants:
     *      data = *psrc;
     *      sbit = 1 << n, 0<=n<=7.
     */
    for (data = *psrc;;) {
	int x0, n, bit;
	byte *bp;
	static const byte lmasks[9] = {
	    0xff, 0x7f, 0x3f, 0x1f, 0xf, 7, 3, 1, 0
	};
	static const byte rmasks[9] = {
	    0, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff
	};

	INCS(runs);

	/* Scan a run of zeros. */
	data ^= 0xff;		/* invert */
	while (data & sbit) {
	    dda_next(xl);
	    sbit >>= 1;
	    INCS(lbit0);
	}
	if (!sbit) {		/* Scan a run of zero bytes. */
sw:	    if ((data = psrc[1]) != 0) {
		psrc++;
		INCS(byte00);
	    } else if ((data = psrc[2]) != 0) {
		dda_state_next(xl.state, dxx8);
		psrc += 2;
		INCS(byte01);
	    } else if ((data = psrc[3]) != 0) {
		dda_state_next(xl.state, dxx16);
		psrc += 3;
		INCS(byte02);
	    } else if ((data = psrc[4]) != 0) {
		dda_state_next(xl.state, dxx24);
		psrc += 4;
		INCS(byte03);
	    } else {
		dda_state_next(xl.state, dxx32);
		psrc += 4;
		INCS(byte04);
		goto sw;
	    }
	    if (data > 0xf)
		sbit = 0x80;
	    else {
		sbit = 0x08;
		dda_state_next(xl.state, dxx4);
	    }
	    data ^= 0xff;	/* invert */
	    while (data & sbit) {
		dda_next(xl);
		sbit >>= 1;
		INCS(rbit0);
	    }
	}
	x0 = dda_current_fixed2int(xl);
	if (psrc >= stop && sbit == stopbit) {
	    /*
	     * We've scanned the last run of 0s.
	     * Prepare to fill the final run of 1s.
	     */
	    n = fixed2int(xl0 + x_extent) - x0;
	} else {		/* Scan a run of ones. */
	    /* We know the current bit is a one. */
	    data ^= 0xff;	/* un-invert */
	    do {
		dda_next(xl);
		sbit >>= 1;
		INCS(lbit1);
	    }
	    while (data & sbit);
	    if (!sbit) {	/* Scan a run of 0xff bytes. */
		while ((data = *++psrc) == 0xff) {
		    dda_state_next(xl.state, dxx8);
		    INCS(byte1);
		}
		if (data < 0xf0)
		    sbit = 0x80;
		else {
		    sbit = 0x08;
		    dda_state_next(xl.state, dxx4);
		}
		while (data & sbit) {
		    dda_next(xl);
		    sbit >>= 1;
		    INCS(rbit1);
		}
	    }
	    n = dda_current_fixed2int(xl) - x0;
	}

	/* Fill the run in the scan line. */
	if (n < 0)
	    x0 += n, n = -n;
	bp = line + (x0 >> 3);
	bit = x0 & 7;
	if ((n += bit) <= 8) {
	    *bp ^= lmasks[bit] - lmasks[n];
	    INCS(thin);
	} else if ((n -= 8) <= 8) {
	    *bp ^= lmasks[bit];
	    bp[1] ^= rmasks[n];
	    INCS(thin2);
	} else {
	    *bp++ ^= lmasks[bit];
	    if (n >= 56) {
		int nb = n >> 3;

		memset(bp, one, nb);
		bp += nb;
		INCS(nwide);
		ADDS(bwide, nb);
	    } else {
		ADDS(bfill, n >> 3);
		while ((n -= 8) >= 0)
		    *bp++ = one;
		INCS(nfill);
	    }
	    *bp ^= rmasks[n & 7];
	}
	if (psrc >= stop && sbit == stopbit)
	    break;
    }
}

/* Copy one rendered scan line to the device. */
private int
copy_portrait(gx_image_enum * penum, const byte * data, int dx, int raster,
	      int x, int y, int w, int h, gx_device * dev)
{
    const gx_device_color *pdc0;
    const gx_device_color *pdc1;
    uint align = alignment_mod(data, align_bitmap_mod);

    /*
     * We know that the lookup table maps 1 bit to 1 bit,
     * so it can only have 2 states: straight-through or invert.
     */
    if (penum->map[0].table.lookup4x1to32[0])
	pdc0 = &penum->icolor1, pdc1 = &penum->icolor0;
    else
	pdc0 = &penum->icolor0, pdc1 = &penum->icolor1;
    data -= align;
    dx += align << 3;
    if (gx_dc_is_pure(pdc0) && gx_dc_is_pure(pdc1)) {
	/* Just use copy_mono. */
	dev_proc_copy_mono((*copy_mono)) =
	    (h == 1 || (raster & (align_bitmap_mod - 1)) == 0 ?
	     dev_proc(dev, copy_mono) : gx_copy_mono_unaligned);
	return (*copy_mono)
	    (dev, data, dx, raster, gx_no_bitmap_id,
	     x, y, w, h, pdc0->colors.pure, pdc1->colors.pure);
    }
    /*
     * At least one color isn't pure: if the other one is transparent, use
     * the opaque color's fill_masked procedure.  Note that we use a
     * slightly unusual representation for transparent here (per
     * gx_begin_image1): a pure color with pixel value gx_no_color_index.
     */
    {
	const gx_device_color *pdc;
	bool invert;

#define DC_IS_NULL(pdc)\
  (gx_dc_is_pure(pdc) && (pdc)->colors.pure == gx_no_color_index)

	if (DC_IS_NULL(pdc1)) {
	    pdc = pdc0;
	    invert = true;
	} else {
	    if (!DC_IS_NULL(pdc0)) {
		int code = gx_device_color_fill_rectangle
		    (pdc0, x, y, w, h, dev, lop_default, NULL);

		if (code < 0)
		    return code;
	    }
	    pdc = pdc1;
	    invert = false;
	}

#undef DC_IS_NULL

	return (*pdc->type->fill_masked)
	    (pdc, data, dx, raster, gx_no_bitmap_id, x, y, w, h,
	     dev, lop_default, invert);

    }
}

/* Rendering procedure for a monobit image with no */
/* skew or rotation and pure colors. */
private int
image_render_simple(gx_image_enum * penum, const byte * buffer, int data_x,
		    uint w, int h, gx_device * dev)
{
    dev_proc_copy_mono((*copy_mono)) = dev_proc(dev, copy_mono);
    const fixed dxx = penum->dxx;
    const byte *line;
    uint line_width, line_size;
    int line_x;
    fixed xcur = dda_current(penum->dda.pixel0.x);
    int ix = fixed2int_pixround(xcur);
    const int iy = penum->yci, ih = penum->hci;
    const gx_device_color * const pdc0 = &penum->icolor0;
    const gx_device_color * const pdc1 = &penum->icolor1;
    int dy;

    if (h == 0)
	return 0;
    if (penum->line == 0) {	/* A direct BitBlt is possible. */
	line = buffer;
	line_size = (w + 7) >> 3;
	line_width = w;
	line_x = 0;
    } else if (copy_mono == dev_proc(&mem_mono_device, copy_mono) &&
	       dxx > 0 && gx_dc_is_pure(pdc1) && gx_dc_is_pure(pdc0) &&
	/* We know the colors must be (0,1) or (1,0). */
	       (pdc0->colors.pure ^ pdc1->colors.pure) == 1 &&
	       !penum->clip_image
	) {
	/* Do the operation directly into the memory device bitmap. */
	int ixr = fixed2int_pixround(xcur + penum->x_extent.x) - 1;
	int line_ix;
	int ib_left = ix >> 3, ib_right = ixr >> 3;
	byte *scan_line = scan_line_base((gx_device_memory *) dev, iy);
	byte save_left, save_right, mask;

	line_x = ix & (align_bitmap_mod * 8 - 1);
	line_ix = ix - line_x;
	line_size = (ixr >> 3) + 1 - (line_ix >> 3);
	line_width = ixr + 1 - ix;
	/* We must save and restore any unmodified bits in */
	/* the two edge bytes. */
	save_left = scan_line[ib_left];
	save_right = scan_line[ib_right];
	image_simple_expand(scan_line + (line_ix >> 3), line_x,
			    line_size, buffer, data_x, w, xcur,
			    penum->x_extent.x,
			    ((pdc0->colors.pure == 0) !=
			     (penum->map[0].table.lookup4x1to32[0] == 0) ?
			     0xff : 0));
	if (ix & 7)
	    mask = (byte) (0xff00 >> (ix & 7)),
		scan_line[ib_left] =
		(save_left & mask) + (scan_line[ib_left] & ~mask);
	if ((ixr + 1) & 7)
	    mask = (byte) (0xff00 >> ((ixr + 1) & 7)),
		scan_line[ib_right] =
		(scan_line[ib_right] & mask) + (save_right & ~mask);
	if (ih <= 1)
	    return 1;
	/****** MAY BE UNALIGNED ******/
	line = scan_line + (line_ix >> 3);
	if (dxx < 0)
	    ix -= line_width;
	for (dy = 1; dy < ih; dy++) {
	    int code = (*copy_mono)
		(dev, line, line_x, line_size, gx_no_bitmap_id,
		 ix, iy + dy, line_width, 1,
		 (gx_color_index)0, (gx_color_index)1);

	    if (code < 0)
		return code;
	}
	return 0;
    } else {
	line = penum->line;
	line_size = penum->line_size;
	line_width = penum->line_width;
	line_x = ix & (align_bitmap_mod * 8 - 1);
	image_simple_expand(penum->line, line_x, line_size,
			    buffer, data_x, w, xcur,
			    penum->x_extent.x, 0);
    }

    /* Finally, transfer the scan line to the device. */
    if (dxx < 0)
	ix -= line_width;
    for (dy = 0; dy < ih; dy++) {
	int code = copy_portrait(penum, line, line_x, line_size,
				 ix, iy + dy, line_width, 1, dev);

	if (code < 0)
	    return code;
    }

    return 1;
}

/* Rendering procedure for a 90 degree rotated monobit image */
/* with pure colors.  We buffer and then flip 8 scan lines at a time. */
private int copy_landscape(P5(gx_image_enum *, int, int, bool, gx_device *));
private int
image_render_landscape(gx_image_enum * penum, const byte * buffer, int data_x,
		       uint w, int h, gx_device * dev)
{
    byte *line = penum->line;
    uint raster = bitmap_raster(penum->line_width);
    int ix = penum->xci, iw = penum->wci;
    int xinc, xmod;
    byte *row;
    const byte *orig_row = 0;
    bool y_neg = penum->dxy < 0;

    if (is_fneg(penum->matrix.yx))
	ix += iw, iw = -iw, xinc = -1;
    else
	xinc = 1;
    /*
     * Because of clipping, there may be discontinuous jumps in the values
     * of ix (xci).  If this happens, or if we are at the end of the data or
     * a client has requested flushing, flush the flipping buffer.
     */
    if (ix != penum->xi_next || h == 0) {
	int xi = penum->xi_next;
	int code =
	    (xinc > 0 ?
	     copy_landscape(penum, penum->line_xy, xi, y_neg, dev) :
	     copy_landscape(penum, xi, penum->line_xy, y_neg, dev));

	if (code < 0)
	    return code;
	penum->line_xy = ix;
	if (h == 0)
	    return code;
    }
    for (; iw != 0; iw -= xinc) {
	if (xinc < 0)
	    --ix;
	xmod = ix & 7;
	row = line + xmod * raster;
	if (orig_row == 0) {
	    image_simple_expand(row, 0, raster,
				buffer, data_x, w,
				dda_current(penum->dda.pixel0.y),
				penum->x_extent.y, 0);
	    orig_row = row;
	} else
	    memcpy(row, orig_row, raster);
	if (xinc > 0) {
	    ++ix;
	    if (xmod == 7) {
		int code =
		    copy_landscape(penum, penum->line_xy, ix, y_neg, dev);

		if (code < 0)
		    return code;
		orig_row = 0;
		penum->line_xy = ix;
	    }
	} else {
	    if (xmod == 0) {
		int code =
		    copy_landscape(penum, ix, penum->line_xy, y_neg, dev);

		if (code < 0)
		    return code;
		orig_row = 0;
		penum->line_xy = ix;
	    }
	}
    }
    penum->xi_next = ix;
    return 0;
}

/* Flip and copy one group of scan lines. */
private int
copy_landscape(gx_image_enum * penum, int x0, int x1, bool y_neg,
	       gx_device * dev)
{
    byte *line = penum->line;
    uint line_width = penum->line_width;
    uint raster = bitmap_raster(line_width);
    byte *flipped = line + raster * 8;
    int w = x1 - x0;
    int y = fixed2int_pixround(dda_current(penum->dda.pixel0.y));

    if (w == 0 || line_width == 0)
	return 0;
    /* Flip the buffered data from raster x 8 to align_bitmap_mod x */
    /* line_width. */
    if (line_width > 0) {
	int i;

	for (i = (line_width - 1) >> 3; i >= 0; --i)
	    memflip8x8(line + i, raster,
		       flipped + (i << (log2_align_bitmap_mod + 3)),
		       align_bitmap_mod);
    }
    /* Transfer the scan lines to the device. */
    if (w < 0)
	x0 = x1, w = -w;
    if (y_neg)
	y -= line_width;
    return copy_portrait(penum, flipped, x0 & 7, align_bitmap_mod,
			 x0, y, w, line_width, dev);
}
