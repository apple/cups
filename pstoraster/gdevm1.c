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

/*$Id: gdevm1.c,v 1.2.2.1 2001/05/13 18:38:31 mike Exp $ */
/* Monobit "memory" (stored bitmap) device */
#include "memory_.h"
#include "gx.h"
#include "gxdevice.h"
#include "gxdevmem.h"		/* semi-public definitions */
#include "gdevmem.h"		/* private definitions */

extern dev_proc_strip_copy_rop(mem_mono_strip_copy_rop);	/* in gdevmrop.c */

/* Optionally, use the slow RasterOp implementations for testing. */
/*#define USE_COPY_ROP */

#ifdef USE_COPY_ROP
#include "gsrop.h"
#endif

/* ================ Standard (byte-oriented) device ================ */

/* We went to a lot of trouble to optimize mem_mono_tile_rectangle. */
/* It has a substantial effect on the total time at high resolutions. */
/* However, it takes quite a lot of code, so we omit it on 16-bit systems. */
#define OPTIMIZE_TILE (arch_sizeof_int > 2)

/* Procedures */
private dev_proc_map_rgb_color(mem_mono_map_rgb_color);
private dev_proc_map_color_rgb(mem_mono_map_color_rgb);
private dev_proc_copy_mono(mem_mono_copy_mono);
private dev_proc_fill_rectangle(mem_mono_fill_rectangle);

#if OPTIMIZE_TILE
private dev_proc_strip_tile_rectangle(mem_mono_strip_tile_rectangle);

#else
#  define mem_mono_strip_tile_rectangle gx_default_strip_tile_rectangle
#endif

/* The device descriptor. */
/* The instance is public. */
const gx_device_memory mem_mono_device =
mem_full_alpha_device("image1", 0, 1, mem_open,
		      mem_mono_map_rgb_color, mem_mono_map_color_rgb,
	 mem_mono_copy_mono, gx_default_copy_color, mem_mono_fill_rectangle,
		      gx_default_map_cmyk_color, gx_no_copy_alpha,
		      mem_mono_strip_tile_rectangle, mem_mono_strip_copy_rop,
		      mem_get_bits_rectangle);

/* Map color to/from RGB.  This may be inverted. */
private gx_color_index
mem_mono_map_rgb_color(gx_device * dev, gx_color_value r, gx_color_value g,
		       gx_color_value b)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;

    return (gx_default_w_b_map_rgb_color(dev, r, g, b) ^
	    mdev->palette.data[0]) & 1;
}
private int
mem_mono_map_color_rgb(gx_device * dev, gx_color_index color,
		       gx_color_value prgb[3])
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;

    return gx_default_w_b_map_color_rgb(dev,
					(color ^ mdev->palette.data[0]) & 1,
					prgb);
}

/* Fill a rectangle with a color. */
private int
mem_mono_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
			gx_color_index color)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;

#ifdef USE_COPY_ROP
    return mem_mono_copy_rop(dev, NULL, 0, 0, gx_no_bitmap_id, NULL,
			     NULL, NULL,
			     x, y, w, h, 0, 0,
			     (color ? rop3_1 : rop3_0));
#else
    fit_fill(dev, x, y, w, h);
    bits_fill_rectangle(scan_line_base(mdev, y), x, mdev->raster,
			-(mono_fill_chunk) color, w, h);
    return 0;
#endif
}

/* Convert x coordinate to byte offset in scan line. */
#define x_to_byte(x) ((x) >> 3)

/* Copy a monochrome bitmap. */
#undef mono_masks
#define mono_masks mono_copy_masks

/*
 * Fetch a chunk from the source.
 *
 * Since source and destination are both always big-endian,
 * fetching an aligned chunk never requires byte swapping.
 */
#define CFETCH_ALIGNED(cptr)\
  (*(const chunk *)(cptr))

/*
 * Note that the macros always cast cptr,
 * so it doesn't matter what the type of cptr is.
 */
/* cshift = chunk_bits - shift. */
#undef chunk
#if arch_is_big_endian
#  define chunk uint
#  define CFETCH_RIGHT(cptr, shift, cshift)\
	(CFETCH_ALIGNED(cptr) >> shift)
#  define CFETCH_LEFT(cptr, shift, cshift)\
	(CFETCH_ALIGNED(cptr) << shift)
/* Fetch a chunk that straddles a chunk boundary. */
#  define CFETCH2(cptr, cskew, skew)\
    (CFETCH_LEFT(cptr, cskew, skew) +\
     CFETCH_RIGHT((const chunk *)(cptr) + 1, skew, cskew))
#else /* little-endian */
#  define chunk bits16
private const bits16 right_masks2[9] =
{
    0xffff, 0x7f7f, 0x3f3f, 0x1f1f, 0x0f0f, 0x0707, 0x0303, 0x0101, 0x0000
};
private const bits16 left_masks2[9] =
{
    0xffff, 0xfefe, 0xfcfc, 0xf8f8, 0xf0f0, 0xe0e0, 0xc0c0, 0x8080, 0x0000
};

#  define CCONT(cptr, off) (((const chunk *)(cptr))[off])
#  define CFETCH_RIGHT(cptr, shift, cshift)\
	((shift) < 8 ?\
	 ((CCONT(cptr, 0) >> (shift)) & right_masks2[shift]) +\
	  (CCONT(cptr, 0) << (cshift)) :\
	 ((chunk)*(const byte *)(cptr) << (cshift)) & 0xff00)
#  define CFETCH_LEFT(cptr, shift, cshift)\
	((shift) < 8 ?\
	 ((CCONT(cptr, 0) << (shift)) & left_masks2[shift]) +\
	  (CCONT(cptr, 0) >> (cshift)) :\
	 ((CCONT(cptr, 0) & 0xff00) >> (cshift)) & 0xff)
/* Fetch a chunk that straddles a chunk boundary. */
/* We can avoid testing the shift amount twice */
/* by expanding the CFETCH_LEFT/right macros in-line. */
#  define CFETCH2(cptr, cskew, skew)\
	((cskew) < 8 ?\
	 ((CCONT(cptr, 0) << (cskew)) & left_masks2[cskew]) +\
	  (CCONT(cptr, 0) >> (skew)) +\
	  (((chunk)(((const byte *)(cptr))[2]) << (cskew)) & 0xff00) :\
	 (((CCONT(cptr, 0) & 0xff00) >> (skew)) & 0xff) +\
	  ((CCONT(cptr, 1) >> (skew)) & right_masks2[skew]) +\
	   (CCONT(cptr, 1) << (cskew)))
#endif

typedef enum {
    COPY_OR = 0, COPY_STORE, COPY_AND, COPY_FUNNY
} copy_function;
typedef struct {
    uint invert;
    copy_function op;
} copy_mode;

/*
 * Map from <color0,color1> to copy_mode.
 * Logically, this is a 2-D array.
 * The indexing is (transparent, 0, 1, unused). */
private const copy_mode copy_modes[16] =
{
    {~0UL, COPY_FUNNY},		/* NN */
    {~0UL, COPY_AND},		/* N0 */
    {0, COPY_OR},		/* N1 */
    {0, 0},			/* unused */
    {0, COPY_AND},		/* 0N */
    {0, COPY_FUNNY},		/* 00 */
    {0, COPY_STORE},		/* 01 */
    {0, 0},			/* unused */
    {~0UL, COPY_OR},		/* 1N */
    {~0UL, COPY_STORE},		/* 10 */
    {0, COPY_FUNNY},		/* 11 */
    {0, 0},			/* unused */
    {0, 0},			/* unused */
    {0, 0},			/* unused */
    {0, 0},			/* unused */
    {0, 0},			/* unused */
};

/* Handle the funny cases that aren't supposed to happen. */
#define FUNNY_CASE()\
  (invert ? gs_note_error(-1) :\
   mem_mono_fill_rectangle(dev, x, y, w, h, color0))

private int
mem_mono_copy_mono(gx_device * dev,
 const byte * source_data, int source_x, int source_raster, gx_bitmap_id id,
   int x, int y, int w, int h, gx_color_index color0, gx_color_index color1)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;

#ifdef USE_COPY_ROP
    return mem_mono_copy_rop(dev, source_data, source_x, source_raster,
			     id, NULL, NULL, NULL,
			     x, y, w, h, 0, 0,
			     ((color0 == gx_no_color_index ? rop3_D :
			       color0 == 0 ? rop3_0 : rop3_1) & ~rop3_S) |
			     ((color1 == gx_no_color_index ? rop3_D :
			       color1 == 0 ? rop3_0 : rop3_1) & rop3_S));
#else /* !USE_COPY_ROP */
    register const byte *bptr;	/* actually chunk * */
    int dbit, wleft;
    uint mask;
    copy_mode mode;

    DECLARE_SCAN_PTR_VARS(dbptr, byte *, dest_raster);
#define optr ((chunk *)dbptr)
    register int skew;
    register uint invert;

    fit_copy(dev, source_data, source_x, source_raster, id, x, y, w, h);
#if gx_no_color_index_value != -1	/* hokey! */
    if (color0 == gx_no_color_index)
	color0 = -1;
    if (color1 == gx_no_color_index)
	color1 = -1;
#endif
    mode = copy_modes[((int)color0 << 2) + (int)color1 + 5];
    invert = mode.invert;	/* load register */
    SETUP_RECT_VARS(dbptr, byte *, dest_raster);
    bptr = source_data + ((source_x & ~chunk_align_bit_mask) >> 3);
    dbit = x & chunk_align_bit_mask;
    skew = dbit - (source_x & chunk_align_bit_mask);

/* Macros for writing partial chunks. */
/* The destination pointer is always named optr, */
/* and must be declared as chunk *. */
/* CINVERT may be temporarily redefined. */
#define CINVERT(bits) ((bits) ^ invert)
#define WRITE_OR_MASKED(bits, mask, off)\
  optr[off] |= (CINVERT(bits) & mask)
#define WRITE_STORE_MASKED(bits, mask, off)\
  optr[off] = ((optr[off] & ~mask) | (CINVERT(bits) & mask))
#define WRITE_AND_MASKED(bits, mask, off)\
  optr[off] &= (CINVERT(bits) | ~mask)
/* Macros for writing full chunks. */
#define WRITE_OR(bits)  *optr |= CINVERT(bits)
#define WRITE_STORE(bits) *optr = CINVERT(bits)
#define WRITE_AND(bits) *optr &= CINVERT(bits)
/* Macro for incrementing to next chunk. */
#define NEXT_X_CHUNK()\
  bptr += chunk_bytes; dbptr += chunk_bytes
/* Common macro for the end of each scan line. */
#define END_Y_LOOP(sdelta, ddelta)\
  bptr += sdelta; dbptr += ddelta

    if ((wleft = w + dbit - chunk_bits) <= 0) {		/* The entire operation fits in one (destination) chunk. */
	set_mono_thin_mask(mask, w, dbit);

#define WRITE_SINGLE(wr_op, src)\
  for ( ; ; )\
   { wr_op(src, mask, 0);\
     if ( --h == 0 ) break;\
     END_Y_LOOP(source_raster, dest_raster);\
   }

#define WRITE1_LOOP(src)\
  switch ( mode.op ) {\
    case COPY_OR: WRITE_SINGLE(WRITE_OR_MASKED, src); break;\
    case COPY_STORE: WRITE_SINGLE(WRITE_STORE_MASKED, src); break;\
    case COPY_AND: WRITE_SINGLE(WRITE_AND_MASKED, src); break;\
    default: return FUNNY_CASE();\
  }

	if (skew >= 0) {	/* single -> single, right/no shift */
	    if (skew == 0) {	/* no shift */
		WRITE1_LOOP(CFETCH_ALIGNED(bptr));
	    } else {		/* right shift */
		int cskew = chunk_bits - skew;

		WRITE1_LOOP(CFETCH_RIGHT(bptr, skew, cskew));
	    }
	} else if (wleft <= skew) {	/* single -> single, left shift */
	    int cskew = chunk_bits + skew;

	    skew = -skew;
	    WRITE1_LOOP(CFETCH_LEFT(bptr, skew, cskew));
	} else {		/* double -> single */
	    int cskew = -skew;

	    skew += chunk_bits;
	    WRITE1_LOOP(CFETCH2(bptr, cskew, skew));
	}
#undef WRITE1_LOOP
#undef WRITE_SINGLE
    } else if (wleft <= skew) {	/* 1 source chunk -> 2 destination chunks. */
	/* This is an important special case for */
	/* both characters and halftone tiles. */
	uint rmask;
	int cskew = chunk_bits - skew;

	set_mono_left_mask(mask, dbit);
	set_mono_right_mask(rmask, wleft);
#undef CINVERT
#define CINVERT(bits) (bits)	/* pre-inverted here */

#if arch_is_big_endian		/* no byte swapping */
#  define WRITE_1TO2(wr_op)\
  for ( ; ; )\
   { register uint bits = CFETCH_ALIGNED(bptr) ^ invert;\
     wr_op(bits >> skew, mask, 0);\
     wr_op(bits << cskew, rmask, 1);\
     if ( --h == 0 ) break;\
     END_Y_LOOP(source_raster, dest_raster);\
   }
#else /* byte swapping */
#  define WRITE_1TO2(wr_op)\
  for ( ; ; )\
   { wr_op(CFETCH_RIGHT(bptr, skew, cskew) ^ invert, mask, 0);\
     wr_op(CFETCH_LEFT(bptr, cskew, skew) ^ invert, rmask, 1);\
     if ( --h == 0 ) break;\
     END_Y_LOOP(source_raster, dest_raster);\
   }
#endif

	switch (mode.op) {
	    case COPY_OR:
		WRITE_1TO2(WRITE_OR_MASKED);
		break;
	    case COPY_STORE:
		WRITE_1TO2(WRITE_STORE_MASKED);
		break;
	    case COPY_AND:
		WRITE_1TO2(WRITE_AND_MASKED);
		break;
	    default:
		return FUNNY_CASE();
	}
#undef CINVERT
#define CINVERT(bits) ((bits) ^ invert)
#undef WRITE_1TO2
    } else {			/* More than one source chunk and more than one */
	/* destination chunk are involved. */
	uint rmask;
	int words = (wleft & ~chunk_bit_mask) >> 3;
	uint sskip = source_raster - words;
	uint dskip = dest_raster - words;
	register uint bits;

	set_mono_left_mask(mask, dbit);
	set_mono_right_mask(rmask, wleft & chunk_bit_mask);
	if (skew == 0) {	/* optimize the aligned case */

#define WRITE_ALIGNED(wr_op, wr_op_masked)\
  for ( ; ; )\
   { int count = wleft;\
     /* Do first partial chunk. */\
     wr_op_masked(CFETCH_ALIGNED(bptr), mask, 0);\
     /* Do full chunks. */\
     while ( (count -= chunk_bits) >= 0 )\
      { NEXT_X_CHUNK(); wr_op(CFETCH_ALIGNED(bptr)); }\
     /* Do last chunk */\
     if ( count > -chunk_bits )\
      { wr_op_masked(CFETCH_ALIGNED(bptr + chunk_bytes), rmask, 1); }\
     if ( --h == 0 ) break;\
     END_Y_LOOP(sskip, dskip);\
   }

	    switch (mode.op) {
		case COPY_OR:
		    WRITE_ALIGNED(WRITE_OR, WRITE_OR_MASKED);
		    break;
		case COPY_STORE:
		    WRITE_ALIGNED(WRITE_STORE, WRITE_STORE_MASKED);
		    break;
		case COPY_AND:
		    WRITE_ALIGNED(WRITE_AND, WRITE_AND_MASKED);
		    break;
		default:
		    return FUNNY_CASE();
	    }
#undef WRITE_ALIGNED
	} else {		/* not aligned */
	    int cskew = -skew & chunk_bit_mask;
	    bool case_right =
	    (skew >= 0 ? true :
	     ((bptr += chunk_bytes), false));

	    skew &= chunk_bit_mask;

#define WRITE_UNALIGNED(wr_op, wr_op_masked)\
  /* Prefetch partial word. */\
  bits =\
    (case_right ? CFETCH_RIGHT(bptr, skew, cskew) :\
     CFETCH2(bptr - chunk_bytes, cskew, skew));\
  wr_op_masked(bits, mask, 0);\
  /* Do full chunks. */\
  while ( count >= chunk_bits )\
    { bits = CFETCH2(bptr, cskew, skew);\
      NEXT_X_CHUNK(); wr_op(bits); count -= chunk_bits;\
    }\
  /* Do last chunk */\
  if ( count > 0 )\
    { bits = CFETCH_LEFT(bptr, cskew, skew);\
      if ( count > skew ) bits += CFETCH_RIGHT(bptr + chunk_bytes, skew, cskew);\
      wr_op_masked(bits, rmask, 1);\
    }

	    switch (mode.op) {
		case COPY_OR:
		    for (;;) {
			int count = wleft;

			WRITE_UNALIGNED(WRITE_OR, WRITE_OR_MASKED);
			if (--h == 0)
			    break;
			END_Y_LOOP(sskip, dskip);
		    }
		    break;
		case COPY_STORE:
		    for (;;) {
			int count = wleft;

			WRITE_UNALIGNED(WRITE_STORE, WRITE_STORE_MASKED);
			if (--h == 0)
			    break;
			END_Y_LOOP(sskip, dskip);
		    }
		    break;
		case COPY_AND:
		    for (;;) {
			int count = wleft;

			WRITE_UNALIGNED(WRITE_AND, WRITE_AND_MASKED);
			if (--h == 0)
			    break;
			END_Y_LOOP(sskip, dskip);
		    }
		    break;
		default /*case COPY_FUNNY */ :
		    return FUNNY_CASE();
	    }
#undef WRITE_UNALIGNED
	}
    }
#undef END_Y_LOOP
#undef NEXT_X_CHUNK
    return 0;
#undef optr
#endif /* !USE_COPY_ROP */
}

#if OPTIMIZE_TILE		/**************** *************** */

/* Strip-tile with a monochrome halftone. */
/* This is a performance bottleneck for monochrome devices, */
/* so we re-implement it, even though it takes a lot of code. */
private int
mem_mono_strip_tile_rectangle(gx_device * dev,
			      register const gx_strip_bitmap * tiles,
int tx, int y, int tw, int th, gx_color_index color0, gx_color_index color1,
			      int px, int py)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;

#ifdef USE_COPY_ROP
    return mem_mono_strip_copy_rop(dev, NULL, 0, 0, tile->id, NULL,
				   tiles, NULL,
				   tx, y, tw, th, px, py,
				   ((color0 == gx_no_color_index ? rop3_D :
				 color0 == 0 ? rop3_0 : rop3_1) & ~rop3_T) |
				   ((color1 == gx_no_color_index ? rop3_D :
				  color1 == 0 ? rop3_0 : rop3_1) & rop3_T));
#else /* !USE_COPY_ROP */
    register uint invert;
    int source_raster;
    uint tile_bits_size;
    const byte *source_data;
    const byte *end;
    int x, rw, w, h;
    register const byte *bptr;	/* actually chunk * */
    int dbit, wleft;
    uint mask;
    byte *dbase;

    DECLARE_SCAN_PTR_VARS(dbptr, byte *, dest_raster);
#define optr ((chunk *)dbptr)
    register int skew;

    /* This implementation doesn't handle strips yet. */
    if (color0 != (color1 ^ 1) || tiles->shift != 0)
	return gx_default_strip_tile_rectangle(dev, tiles, tx, y, tw, th,
					       color0, color1, px, py);
    fit_fill(dev, tx, y, tw, th);
    invert = -(uint) color0;
    source_raster = tiles->raster;
    source_data = tiles->data + ((y + py) % tiles->rep_height) * source_raster;
    tile_bits_size = tiles->size.y * source_raster;
    end = tiles->data + tile_bits_size;
#undef END_Y_LOOP
#define END_Y_LOOP(sdelta, ddelta)\
  if ( end - bptr <= sdelta )	/* wrap around */\
    bptr -= tile_bits_size;\
  bptr += sdelta; dbptr += ddelta
    dest_raster = mdev->raster;
    dbase = scan_line_base(mdev, y);
    x = tx;
    rw = tw;
    /*
     * The outermost loop here works horizontally, one iteration per
     * copy of the tile.  Note that all iterations except the first
     * have source_x = 0.
     */
    {
	int source_x = (x + px) % tiles->rep_width;

	w = tiles->size.x - source_x;
	bptr = source_data + ((source_x & ~chunk_align_bit_mask) >> 3);
	dbit = x & chunk_align_bit_mask;
	skew = dbit - (source_x & chunk_align_bit_mask);
    }
  outer:if (w > rw)
	w = rw;
    h = th;
    dbptr = dbase + ((x >> 3) & -chunk_align_bytes);
    if ((wleft = w + dbit - chunk_bits) <= 0) {		/* The entire operation fits in one (destination) chunk. */
	set_mono_thin_mask(mask, w, dbit);
#define WRITE1_LOOP(src)\
  for ( ; ; )\
   { WRITE_STORE_MASKED(src, mask, 0);\
     if ( --h == 0 ) break;\
     END_Y_LOOP(source_raster, dest_raster);\
   }
	if (skew >= 0) {	/* single -> single, right/no shift */
	    if (skew == 0) {	/* no shift */
		WRITE1_LOOP(CFETCH_ALIGNED(bptr));
	    } else {		/* right shift */
		int cskew = chunk_bits - skew;

		WRITE1_LOOP(CFETCH_RIGHT(bptr, skew, cskew));
	    }
	} else if (wleft <= skew) {	/* single -> single, left shift */
	    int cskew = chunk_bits + skew;

	    skew = -skew;
	    WRITE1_LOOP(CFETCH_LEFT(bptr, skew, cskew));
	} else {		/* double -> single */
	    int cskew = -skew;

	    skew += chunk_bits;
	    WRITE1_LOOP(CFETCH2(bptr, cskew, skew));
	}
#undef WRITE1_LOOP
    } else if (wleft <= skew) {	/* 1 source chunk -> 2 destination chunks. */
	/* This is an important special case for */
	/* both characters and halftone tiles. */
	uint rmask;
	int cskew = chunk_bits - skew;

	set_mono_left_mask(mask, dbit);
	set_mono_right_mask(rmask, wleft);
#if arch_is_big_endian		/* no byte swapping */
#undef CINVERT
#define CINVERT(bits) (bits)	/* pre-inverted here */
	for (;;) {
	    register uint bits = CFETCH_ALIGNED(bptr) ^ invert;

	    WRITE_STORE_MASKED(bits >> skew, mask, 0);
	    WRITE_STORE_MASKED(bits << cskew, rmask, 1);
	    if (--h == 0)
		break;
	    END_Y_LOOP(source_raster, dest_raster);
	}
#undef CINVERT
#define CINVERT(bits) ((bits) ^ invert)
#else /* byte swapping */
	for (;;) {
	    WRITE_STORE_MASKED(CFETCH_RIGHT(bptr, skew, cskew), mask, 0);
	    WRITE_STORE_MASKED(CFETCH_LEFT(bptr, cskew, skew), rmask, 1);
	    if (--h == 0)
		break;
	    END_Y_LOOP(source_raster, dest_raster);
	}
#endif
    } else {			/* More than one source chunk and more than one */
	/* destination chunk are involved. */
	uint rmask;
	int words = (wleft & ~chunk_bit_mask) >> 3;
	uint sskip = source_raster - words;
	uint dskip = dest_raster - words;
	register uint bits;

#define NEXT_X_CHUNK()\
  bptr += chunk_bytes; dbptr += chunk_bytes

	set_mono_right_mask(rmask, wleft & chunk_bit_mask);
	if (skew == 0) {	/* optimize the aligned case */
	    if (dbit == 0)
		mask = 0;
	    else
		set_mono_left_mask(mask, dbit);
	    for (;;) {
		int count = wleft;

		/* Do first partial chunk. */
		if (mask)
		    WRITE_STORE_MASKED(CFETCH_ALIGNED(bptr), mask, 0);
		else
		    WRITE_STORE(CFETCH_ALIGNED(bptr));
		/* Do full chunks. */
		while ((count -= chunk_bits) >= 0) {
		    NEXT_X_CHUNK();
		    WRITE_STORE(CFETCH_ALIGNED(bptr));
		}
		/* Do last chunk */
		if (count > -chunk_bits) {
		    WRITE_STORE_MASKED(CFETCH_ALIGNED(bptr + chunk_bytes), rmask, 1);
		}
		if (--h == 0)
		    break;
		END_Y_LOOP(sskip, dskip);
	    }
	} else {		/* not aligned */
	    bool case_right =
	    (skew >= 0 ? true :
	     ((bptr += chunk_bytes), false));
	    int cskew = -skew & chunk_bit_mask;

	    skew &= chunk_bit_mask;
	    set_mono_left_mask(mask, dbit);
	    for (;;) {
		int count = wleft;

		if (case_right)
		    bits = CFETCH_RIGHT(bptr, skew, cskew);
		else
		    bits = CFETCH2(bptr - chunk_bytes, cskew, skew);
		WRITE_STORE_MASKED(bits, mask, 0);
		/* Do full chunks. */
		while (count >= chunk_bits) {
		    bits = CFETCH2(bptr, cskew, skew);
		    NEXT_X_CHUNK();
		    WRITE_STORE(bits);
		    count -= chunk_bits;
		}
		/* Do last chunk */
		if (count > 0) {
		    bits = CFETCH_LEFT(bptr, cskew, skew);
		    if (count > skew)
			bits += CFETCH_RIGHT(bptr + chunk_bytes, skew, cskew);
		    WRITE_STORE_MASKED(bits, rmask, 1);
		}
		if (--h == 0)
		    break;
		END_Y_LOOP(sskip, dskip);
	    }
	}
    }
#undef END_Y_LOOP
#undef NEXT_X_CHUNK
#undef optr
    if ((rw -= w) > 0) {
	x += w;
	w = tiles->size.x;
	bptr = source_data;
	skew = dbit = x & chunk_align_bit_mask;
	goto outer;
    }
    return 0;
#endif /* !USE_COPY_ROP */
}

#endif /**************** *************** */

/* ================ "Word"-oriented device ================ */

/* Note that on a big-endian machine, this is the same as the */
/* standard byte-oriented-device. */

#if !arch_is_big_endian

/* Procedures */
private dev_proc_copy_mono(mem1_word_copy_mono);
private dev_proc_fill_rectangle(mem1_word_fill_rectangle);

#define mem1_word_strip_tile_rectangle gx_default_strip_tile_rectangle

/* Here is the device descriptor. */
const gx_device_memory mem_mono_word_device =
mem_full_alpha_device("image1w", 0, 1, mem_open,
		      mem_mono_map_rgb_color, mem_mono_map_color_rgb,
       mem1_word_copy_mono, gx_default_copy_color, mem1_word_fill_rectangle,
		      gx_default_map_cmyk_color, gx_no_copy_alpha,
		      mem1_word_strip_tile_rectangle, gx_no_strip_copy_rop,
		      mem_word_get_bits_rectangle);

/* Fill a rectangle with a color. */
private int
mem1_word_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
			 gx_color_index color)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    byte *base;
    uint raster;

    fit_fill(dev, x, y, w, h);
    base = scan_line_base(mdev, y);
    raster = mdev->raster;
    mem_swap_byte_rect(base, raster, x, w, h, true);
    bits_fill_rectangle(base, x, raster, -(mono_fill_chunk) color, w, h);
    mem_swap_byte_rect(base, raster, x, w, h, true);
    return 0;
}

/* Copy a bitmap. */
private int
mem1_word_copy_mono(gx_device * dev,
 const byte * source_data, int source_x, int source_raster, gx_bitmap_id id,
   int x, int y, int w, int h, gx_color_index color0, gx_color_index color1)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    byte *row;
    uint raster;
    bool store;

    fit_copy(dev, source_data, source_x, source_raster, id, x, y, w, h);
    row = scan_line_base(mdev, y);
    raster = mdev->raster;
    store = (color0 != gx_no_color_index && color1 != gx_no_color_index);
    mem_swap_byte_rect(row, raster, x, w, h, store);
    mem_mono_copy_mono(dev, source_data, source_x, source_raster, id,
		       x, y, w, h, color0, color1);
    mem_swap_byte_rect(row, raster, x, w, h, false);
    return 0;
}

#endif /* !arch_is_big_endian */
