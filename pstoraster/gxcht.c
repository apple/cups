/* Copyright (C) 1993, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxcht.c,v 1.2 2000/03/08 23:14:51 mike Exp $ */
/* Color halftone rendering for Ghostscript imaging library */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsutil.h"		/* for id generation */
#include "gxarith.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gxdevice.h"
#include "gxcmap.h"
#include "gxdcolor.h"
#include "gxistate.h"
#include "gzht.h"

/* Define the size of the tile buffer allocated on the stack. */
#define tile_longs_LARGE 256
#define tile_longs_SMALL 64
#if arch_small_memory
#  define tile_longs_allocated tile_longs_SMALL
#  define tile_longs tile_longs_SMALL
#else
#  define tile_longs_allocated tile_longs_LARGE
#  define tile_longs\
     (gs_debug_c('.') ? tile_longs_SMALL : tile_longs_LARGE)
#endif

/* Define the colored halftone device color type. */
gs_private_st_ptrs1(st_dc_ht_colored, gx_device_color, "dc_ht_colored",
    dc_ht_colored_enum_ptrs, dc_ht_colored_reloc_ptrs, colors.colored.c_ht);
private dev_color_proc_load(gx_dc_ht_colored_load);
private dev_color_proc_fill_rectangle(gx_dc_ht_colored_fill_rectangle);
private dev_color_proc_equal(gx_dc_ht_colored_equal);
const gx_device_color_type_t gx_dc_type_data_ht_colored = {
    &st_dc_ht_colored,
    gx_dc_ht_colored_load, gx_dc_ht_colored_fill_rectangle,
    gx_dc_default_fill_masked, gx_dc_ht_colored_equal
};
#undef gx_dc_type_ht_colored
const gx_device_color_type_t *const gx_dc_type_ht_colored =
    &gx_dc_type_data_ht_colored;
#define gx_dc_type_ht_colored (&gx_dc_type_data_ht_colored)

/* Forward references. */
private void set_ht_colors(P7(gx_color_index[16], gx_strip_bitmap *[4],
			      const gx_device_color *, gx_device *,
			      gx_ht_cache *[4], int, int *));
private void set_color_ht(P9(gx_strip_bitmap *, int, int, int, int, int, int,
			     const gx_color_index[16],
			     const gx_strip_bitmap *[4]));

/* Define a table for expanding 8x1 bits to 8x4. */
private const bits32 expand_8x1_to_8x4[256] = {
#define x16(c)\
  c+0, c+1, c+0x10, c+0x11, c+0x100, c+0x101, c+0x110, c+0x111,\
  c+0x1000, c+0x1001, c+0x1010, c+0x1011, c+0x1100, c+0x1101, c+0x1110, c+0x1111
    x16(0x00000000), x16(0x00010000), x16(0x00100000), x16(0x00110000),
    x16(0x01000000), x16(0x01010000), x16(0x01100000), x16(0x01110000),
    x16(0x10000000), x16(0x10010000), x16(0x10100000), x16(0x10110000),
    x16(0x11000000), x16(0x11010000), x16(0x11100000), x16(0x11110000)
#undef x16
};

/* Prepare to use a colored halftone, by loading the default cache. */
private int
gx_dc_ht_colored_load(gx_device_color * pdevc, const gs_imager_state * pis,
		      gx_device * ignore_dev, gs_color_select_t select)
{
    gx_device_halftone *pdht = pis->dev_ht;
    gx_ht_order *porder = &pdht->components[0].corder;
    gx_ht_cache *pcache = pis->ht_cache;

    if (pcache->order.bits != porder->bits)
	gx_ht_init_cache(pcache, porder);
    /* Set the cache pointers in the default order. */
    pdht->order.cache = porder->cache = pcache;
    return 0;
}

/* Fill a rectangle with a colored halftone. */
/* Note that we treat this as "texture" for RasterOp. */
private int
gx_dc_ht_colored_fill_rectangle(const gx_device_color * pdevc,
				int x, int y, int w, int h,
				gx_device * dev, gs_logical_operation_t lop,
				const gx_rop_source_t * source)
{
    ulong tbits[tile_longs_allocated];
    const uint tile_bytes = tile_longs * size_of(long);
    gx_strip_bitmap tiles;
    gx_rop_source_t no_source;
    const gx_device_halftone *pdht = pdevc->colors.colored.c_ht;
    int depth = dev->color_info.depth;
    int nplanes = dev->color_info.num_components;
    gx_color_index colors[16];
    gx_strip_bitmap *sbits[4];
    gx_ht_cache *caches[4];
    int code = 0;
    int raster;
    uint size_x;
    int dw, dh;
    int lw = pdht->lcm_width, lh = pdht->lcm_height;
    int plane_mask;
    bool no_rop;

    if (w <= 0 || h <= 0)
	return 0;
    /* Colored halftone patterns are unconditionally opaque. */
    lop &= ~lop_T_transparent;
    tiles.data = (byte *) tbits;
    if (pdht->components == 0)
	caches[0] = caches[1] = caches[2] = caches[3] = pdht->order.cache;
    else {
	gx_ht_order_component *pocs = pdht->components;

	caches[0] = pocs[pdht->color_indices[0]].corder.cache;
	caches[1] = pocs[pdht->color_indices[1]].corder.cache;
	caches[2] = pocs[pdht->color_indices[2]].corder.cache;
	caches[3] = pocs[pdht->color_indices[3]].corder.cache;
    }
    set_ht_colors(colors, sbits, pdevc, dev, caches, nplanes, &plane_mask);
    if (!(plane_mask & (plane_mask - 1))) {
	/*
	 * At most one plane is not solid-color: we can treat this as a
	 * binary halftone (or, anomalously, a pure color).
	 */
	gx_device_color devc;

	if (plane_mask == 0 ) {
	    color_set_pure(&devc, colors[0]);
	} else {
	    int plane = small_exact_log2(plane_mask);
	    gx_ht_tile tile;

	    tile.tiles = *sbits[plane];	 /* already rendered */
	    tile.level = pdevc->colors.colored.c_level[plane];
	    color_set_binary_tile(&devc, colors[0], colors[plane_mask], &tile);
	    devc.phase = pdevc->phase;
	}
	return gx_device_color_fill_rectangle(&devc, x, y, w, h, dev, lop,
					      source);
    }

    no_rop = source == NULL && lop_no_S_is_T(lop);
    /*
     * If the LCM of the plane cell sizes is smaller than the rectangle
     * being filled, compute a single tile and let tile_rectangle do the
     * replication.
     */
    if ((w > lw || h > lh) &&
	(raster = bitmap_raster(lw * depth)) <= tile_bytes / lh
	) {
	/*
	 * The only reason we need to do fit_fill here is that if the
	 * device is a clipper, the caller might be counting on it to do
	 * all necessary clipping.  Actually, we should clip against the
	 * device's clipping box, not the default....
	 */
	fit_fill(dev, x, y, w, h);
	/* Check to make sure we still have a big rectangle. */
	if (w > lw || h > lh) {
	    tiles.raster = raster;
	    tiles.rep_width = tiles.size.x = lw;
	    tiles.rep_height = tiles.size.y = lh;
	    tiles.id = gs_next_ids(1);
	    tiles.rep_shift = tiles.shift = 0;
	    /* See below for why we need to cast bits. */
	    set_color_ht(&tiles, 0, 0, lw, lh,
			 depth, plane_mask, colors,
			 (const gx_strip_bitmap **)sbits);
	    if (no_rop)
		return (*dev_proc(dev, strip_tile_rectangle)) (dev, &tiles,
							       x, y, w, h,
				       gx_no_color_index, gx_no_color_index,
					    pdevc->phase.x, pdevc->phase.y);
	    if (source == NULL)
		set_rop_no_source(source, no_source, dev);
	    return (*dev_proc(dev, strip_copy_rop)) (dev, source->sdata,
			       source->sourcex, source->sraster, source->id,
			     (source->use_scolors ? source->scolors : NULL),
						     &tiles, NULL,
						     x, y, w, h,
					     pdevc->phase.x, pdevc->phase.y,
                                                lop);
	}
    }
    tiles.id = gx_no_bitmap_id;
    size_x = w * depth;
    raster = bitmap_raster(size_x);
    if (raster > tile_bytes) {
	/*
	 * We can't even do an entire line at once.  See above for
	 * why we do the X equivalent of fit_fill here.
	 */
	if (x < 0)
	    w += x, x = 0;
	if (x > dev->width - w)
	    w = dev->width - x;
	if (w <= 0)
	    return 0;
	size_x = w * depth;
	raster = bitmap_raster(size_x);
	if (raster > tile_bytes) {
	    /* We'll have to do a partial line. */
	    dw = tile_bytes * 8 / depth;
	    size_x = dw * depth;
	    raster = bitmap_raster(size_x);
	    dh = 1;
	    goto fit;
	}
    }
    /* Do as many lines as will fit. */
    dw = w;
    dh = tile_bytes / raster;
    if (dh > h)
	dh = h;
fit:				/* Now the tile will definitely fit. */
    tiles.raster = raster;
    tiles.rep_width = tiles.size.x = size_x / depth;
    tiles.rep_shift = tiles.shift = 0;
    while (w) {
	int cy = y, ch = dh, left = h;

	tiles.rep_height = tiles.size.y = ch;
	for (;;) {
	    /*
	     * The cast in the following statement is bogus,
	     * but some compilers won't accept an array type,
	     * and won't accept the ** type without a cast.
	     */
	    set_color_ht(&tiles, x + pdevc->phase.x, cy + pdevc->phase.y,
			 dw, ch, depth, plane_mask, colors,
			 (const gx_strip_bitmap **)sbits);
	    if (no_rop) {
		code = (*dev_proc(dev, copy_color))
		    (dev, tiles.data, 0, raster, gx_no_bitmap_id,
		     x, cy, dw, ch);
	    } else {
                if (source == NULL)
                    set_rop_no_source(source, no_source, dev);
	        return (*dev_proc(dev, strip_copy_rop))
		    (dev, source->sdata, source->sourcex, source->sraster,
		     source->id,
		     (source->use_scolors ? source->scolors : NULL),
		     &tiles, NULL, x, cy, dw, ch, 0, 0, lop);
	    }
	    if (code < 0)
		return code;
	    if (!(left -= ch))
		break;
	    cy += ch;
	    if (ch > left)
		tiles.rep_height = tiles.size.y = ch = left;
	}
	if (!(w -= dw))
	    break;
	x += dw;
	if (dw > w)
	    dw = w, tiles.rep_width = tiles.size.x = size_x / depth;
    }
    return code;
}

/*
 * We construct color halftone tiles out of 3 or 4 "planes".
 * Each plane specifies halftoning for one component (R/G/B or C/M/Y/K).
 */

private const ulong ht_no_bitmap_data[] = {
    0, 0, 0, 0, 0, 0, 0, 0
};
private gx_strip_bitmap ht_no_bitmap;
private const gx_strip_bitmap ht_no_bitmap_init = {
    0, sizeof(ulong),
    {sizeof(ulong) * 8, countof(ht_no_bitmap_data)},
    gx_no_bitmap_id, 1, 1, 0, 0
};

void
gs_gxcht_init(gs_memory_t *mem)
{
    ht_no_bitmap = ht_no_bitmap_init;
    ht_no_bitmap.data = (byte *)ht_no_bitmap_data;	/* actually const */
}

/* Set up the colors and the individual plane halftone bitmaps. */
private void
set_ht_colors(gx_color_index colors[16], gx_strip_bitmap * sbits[4],
	      const gx_device_color * pdc, gx_device * dev,
	      gx_ht_cache * caches[4], int nplanes, int *pmask)
{
    gx_color_value v[2][4];
    gx_color_value max_color = dev->color_info.dither_colors - 1;
    int plane_mask = 0;
    /*
     * NB: the halftone orders are all set up for an additive color space.
     *     To use these work with a cmyk color space, it is necessary to
     *     invert both the color level and the color pair. Note that if the
     *     original color was provided an additive space, this will reverse
     *     (in an approximate sense) the color conversion performed to
     *     express the color in cmyk space.
     */
    bool invert = dev->color_info.num_components == 4;  /****** HACK ******/

#define set_plane_color(i)\
  BEGIN\
    uint q = pdc->colors.colored.c_base[i];\
    uint r = pdc->colors.colored.c_level[i];\
\
    v[0][i] = fractional_color(q, max_color);\
    if (r == 0)\
	v[1][i] = v[0][i], sbits[i] = &ht_no_bitmap;\
    else if (!invert) {\
	v[1][i] = fractional_color(q + 1, max_color);\
	sbits[i] = &gx_render_ht(caches[i], r)->tiles;\
	plane_mask |= 1 << (i);\
    } else {                                                        \
	const gx_device_halftone *  pdht = pdc->colors.colored.c_ht;\
	int                         nlevels = 0;                    \
                                                                    \
	nlevels = pdht->components[pdht->color_indices[i]].corder.num_levels;\
	v[1][i] = v[0][i];                                          \
	v[0][i] = fractional_color(q + 1, max_color);               \
	sbits[i] = &gx_render_ht(caches[i], nlevels - r)->tiles;    \
	plane_mask |= 1 << (i);                                     \
    }\
  END
#define map8(m) m(0), m(1), m(2), m(3), m(4), m(5), m(6), m(7)
    set_plane_color(0);
    set_plane_color(1);
    set_plane_color(2);
    if (nplanes == 3) {
#define map_rgb(i)\
  colors[i] = map1rgb(v[(i) & 1][0], v[((i) & 2) >> 1][1], v[(i) >> 2][2])
	gx_color_value alpha = pdc->colors.colored.alpha;

	if (alpha == gx_max_color_value) {
#ifdef DEBUG
#  define map1rgb(r, g, b) gx_map_rgb_color(dev, r, g, b)
#else
	    dev_proc_map_rgb_color((*map)) =
		dev_proc(dev, map_rgb_color);
#  define map1rgb(r, g, b) (*map)(dev, r, g, b)
#endif
	    map8(map_rgb);
#undef map1rgb
	} else {
#ifdef DEBUG
#  define map1rgb(r, g, b) gx_map_rgb_alpha_color(dev, r, g, b, alpha)
#else
	    dev_proc_map_rgb_alpha_color((*map)) =
		dev_proc(dev, map_rgb_alpha_color);
#  define map1rgb(r, g, b) (*map)(dev, r, g, b, alpha)
#endif
	    map8(map_rgb);
#undef map1rgb
	}
    } else {
#define map_cmyk(i)\
  colors[i] = map1cmyk(v[(i) & 1][0], v[((i) & 2) >> 1][1],\
		       v[((i) & 4) >> 2][2], v[(i) >> 3][3])
#ifdef DEBUG
#  define map1cmyk(r, g, b, w) gx_map_cmyk_color(dev, r, g, b, w)
#else
	dev_proc_map_cmyk_color((*map)) =
	    dev_proc(dev, map_cmyk_color);
#  define map1cmyk(r, g, b, w) (*map)(dev, r, g, b, w)
#endif
	set_plane_color(3);
	/*
	 * For CMYK output, especially if the input was RGB, it's
	 * common for one or more of the components to be zero.
	 * Each zero component can cut the cost of color mapping in
	 * half, so it's worth doing a little checking here.
	 */
#define m(i) map_cmyk(i)
	switch (plane_mask) {
	    case 15:
		m(15); m(14); m(13); m(12);
		m(11); m(10); m(9); m(8);
	    case 7:
		m(7); m(6); m(5); m(4);
c3:	    case 3:
		m(3); m(2);
c1:	    case 1:
		m(1);
		break;
	    case 14:
		m(14); m(12); m(10); m(8);
	    case 6:
		m(6); m(4);
c2:	    case 2:
		m(2);
		break;
	    case 13:
		m(13); m(12); m(9); m(8);
	    case 5:
		m(5); m(4);
		goto c1;
	    case 12:
		m(12); m(8);
	    case 4:
		m(4);
		break;
	    case 11:
		m(11); m(10); m(9); m(8);
		goto c3;
	    case 10:
		m(10); m(8);
		goto c2;
	    case 9:
		m(9); m(8);
		goto c1;
	    case 8:
		m(8);
		break;
	    case 0:;
	}
	m(0);
#undef m
#undef map1cmyk
    }
#undef map8
#undef set_plane_color
    *pmask = plane_mask;
}

/* Define the bookkeeping structure for each plane of halftone rendering. */
typedef struct tile_cursor_s {
    int tile_shift;		/* X shift per copy of tile */
    int xoffset;
    int xshift;
    uint xbytes;
    int xbits;
    const byte *row;
    const byte *tdata;
    uint raster;
    const byte *data;
    int bit_shift;
} tile_cursor_t;

/* Initialize one plane cursor. */
private void
init_tile_cursor(int i, tile_cursor_t *ptc, const gx_strip_bitmap *btile,
		 int endx, int lasty)
{
    int tw = btile->size.x;
    int bx = ((ptc->tile_shift = btile->shift) == 0 ? endx :
	      endx + lasty / btile->size.y * ptc->tile_shift) % tw;
    int by = lasty % btile->size.y;

    ptc->xoffset = bx >> 3;
    ptc->xshift = 8 - (bx & 7);
    ptc->xbytes = (tw - 1) >> 3;
    ptc->xbits = ((tw - 1) & 7) + 1;
    ptc->tdata = btile->data;
    ptc->raster = btile->raster;
    ptc->row = ptc->tdata + by * ptc->raster;
    if_debug5('h', "[h]plane %d: size=%d,%d bx=%d by=%dn",
	      i, tw, btile->size.y, bx, by);
}

/* Render the combined halftone. */
private void
     set_color_ht(
		     gx_strip_bitmap * ctiles,	/* the output tile; data, raster, size are set */
		     int px,	/* the initial phase of the output tile */
		     int py,
		     int w,	/* how much of the tile to set */
		     int h,
		     int depth,	/* depth of tile (4, 8, 16, 24, 32) */
		     int plane_mask,	/* which planes are halftoned */
		     const gx_color_index colors[16],	/* the actual colors for the tile, */
				/* actually [1 << nplanes] */
		     const gx_strip_bitmap * sbits[4]	/* the bitmaps for the planes, */
				/* actually [nplanes] */
) {
    /* Note that the planes are specified in the order RGB or CMYK, but */
    /* the indices used for the internal colors array are BGR or KYMC. */

    int x, y;
    tile_cursor_t cursor[4];
    int dbytes = depth >> 3;
    uint dest_raster = ctiles->raster;
    byte *dest_row =
	ctiles->data + dest_raster * (h - 1) + (w * depth) / 8;

    if_debug6('h',
	      "[h]color_ht: x=%d y=%d w=%d h=%d plane_mask=%d depth=%d\n",
	      px, py, w, h, plane_mask, depth);

    /* Do one-time cursor initialization. */
    {
	int endx = w + px;
	int lasty = h - 1 + py;

	if (plane_mask & 1)
	    init_tile_cursor(0, &cursor[0], sbits[0], endx, lasty);
	if (plane_mask & 2)
	    init_tile_cursor(1, &cursor[1], sbits[1], endx, lasty);
	if (plane_mask & 4)
	    init_tile_cursor(2, &cursor[2], sbits[2], endx, lasty);
	if (plane_mask & 8)
	    init_tile_cursor(3, &cursor[3], sbits[3], endx, lasty);
    }

    /* Now compute the actual tile. */
    for (y = h; ; dest_row -= dest_raster) {
	byte *dest = dest_row;

#define set_row(c)\
    (c.data = c.row + c.xoffset,\
     c.bit_shift = c.xshift)
	if (plane_mask & 1)
	    set_row(cursor[0]);
	if (plane_mask & 2)
	    set_row(cursor[1]);
	if (plane_mask & 4)
	    set_row(cursor[2]);
	if (plane_mask & 8)
	    set_row(cursor[3]);
#undef set_row
	--y;
	for (x = w; x > 0;) {
	    bits32 indices;
	    int nx, i;
	    register uint bits;

/* Get the next byte's worth of bits.  Note that there may be */
/* excess bits set beyond the 8th. */
#define next_bits(c)\
{	if ( c.data > c.row )\
	{	bits = ((c.data[-1] << 8) | *c.data) >> c.bit_shift;\
		c.data--;\
	}\
	else\
	{	bits = *c.data >> c.bit_shift;\
		c.data += c.xbytes;\
		if ( (c.bit_shift -= c.xbits) < 0 )\
		{	bits |= *c.data << -c.bit_shift;\
			c.bit_shift += 8;\
		}\
		else\
		{	bits |= ((c.data[-1] << 8) | *c.data) >> c.bit_shift;\
			c.data--;\
		}\
	}\
}
	    if (plane_mask & 1) {
		next_bits(cursor[0]);
		indices = expand_8x1_to_8x4[bits & 0xff];
	    } else
		indices = 0;
	    if (plane_mask & 2) {
		next_bits(cursor[1]);
		indices |= expand_8x1_to_8x4[bits & 0xff] << 1;
	    }
	    if (plane_mask & 4) {
		next_bits(cursor[2]);
		indices |= expand_8x1_to_8x4[bits & 0xff] << 2;
	    }
	    if (plane_mask & 8) {
		next_bits(cursor[3]);
		indices |= expand_8x1_to_8x4[bits & 0xff] << 3;
	    }
#undef next_bits
	    nx = min(x, 8);	/* 1 <= nx <= 8 */
	    x -= nx;
	    switch (dbytes) {
		case 0:	/* 4 */
		    i = nx;
		    if ((x + nx) & 1) {
			/* First pixel is even nibble. */
			*dest = (*dest & 0xf) +
			    ((byte)colors[indices & 0xf] << 4);
			indices >>= 4;
			--i;
		    }
		    /* Now 0 <= i <= 8. */
		    for (; (i -= 2) >= 0; indices >>= 8)
			*--dest =
			    (byte)colors[indices & 0xf] +
			    ((byte)colors[(indices >> 4) & 0xf]
			     << 4);
		    /* Check for final odd nibble. */
		    if (i & 1)
			*--dest = (byte)colors[indices & 0xf];
		    break;
		case 4:	/* 32 */
		    for (i = nx; --i >= 0; indices >>= 4) {
			bits32 tcolor = (bits32)colors[indices & 0xf];

			dest -= 4;
			dest[3] = (byte)tcolor;
			dest[2] = (byte)(tcolor >> 8);
			tcolor >>= 16;
			dest[1] = (byte)tcolor;
			dest[0] = (byte)(tcolor >> 8);
		    }
		    break;
		case 3:	/* 24 */
		    for (i = nx; --i >= 0; indices >>= 4) {
			bits32 tcolor = (bits32)colors[indices & 0xf];

			dest -= 3;
			dest[2] = (byte) tcolor;
			dest[1] = (byte)(tcolor >> 8);
			dest[0] = (byte)(tcolor >> 16);
		    }
		    break;
		case 2:	/* 16 */
		    for (i = nx; --i >= 0; indices >>= 4) {
			uint tcolor =
			    (uint)colors[indices & 0xf];

			dest -= 2;
			dest[1] = (byte)tcolor;
			dest[0] = (byte)(tcolor >> 8);
		    }
		    break;
		case 1:	/* 8 */
		    for (i = nx; --i >= 0; indices >>= 4)
			*--dest = (byte)colors[indices & 0xf];
		    break;
	    }
	}
	if (y == 0)
	    break;

#define step_row(c, i)\
  BEGIN\
  if ( c.row > c.tdata )\
    c.row -= c.raster;\
  else	/* wrap around to end of tile, taking shift into account */\
    { c.row += c.raster * (sbits[i]->size.y - 1);\
      if ( c.tile_shift )\
	{ if ( (c.xshift += c.tile_shift) >= 8 )\
	    { if ( (c.xoffset -= c.xshift >> 3) < 0 )\
		{ /* wrap around in X */\
		  int bx = (c.xoffset << 3) + 8 - (c.xshift & 7) +\
		    sbits[i]->size.x;\
		  c.xoffset = bx >> 3;\
		  c.xshift = 8 - (bx & 7);\
		}\
	      else\
		c.xshift &= 7;\
	    }\
	}\
    }\
    END

	if (plane_mask & 1)
	    step_row(cursor[0], 0);
	if (plane_mask & 2)
	    step_row(cursor[1], 1);
	if (plane_mask & 4)
	    step_row(cursor[2], 2);
	if (plane_mask & 8)
	    step_row(cursor[3], 3);
#undef step_row
    }
}

/* Compare two colored halftones for equality. */
private bool
gx_dc_ht_colored_equal(const gx_device_color * pdevc1,
		       const gx_device_color * pdevc2)
{
    uint num_comp;

    if (pdevc2->type != pdevc1->type ||
	pdevc1->colors.colored.c_ht != pdevc2->colors.colored.c_ht ||
	pdevc1->colors.colored.alpha != pdevc2->colors.colored.alpha ||
	pdevc1->phase.x != pdevc2->phase.x ||
	pdevc1->phase.y != pdevc2->phase.y
	)
	return false;
    num_comp = pdevc1->colors.colored.c_ht->num_comp;
    return
	!memcmp(pdevc1->colors.colored.c_base,
		pdevc2->colors.colored.c_base,
		num_comp * sizeof(pdevc1->colors.colored.c_base[0])) &&
	!memcmp(pdevc1->colors.colored.c_level,
		pdevc2->colors.colored.c_level,
		num_comp * sizeof(pdevc1->colors.colored.c_level[0]));
}
