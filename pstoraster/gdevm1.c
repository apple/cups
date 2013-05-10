/* Copyright (C) 1989, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevm1.c */
/* Monobit "memory" (stored bitmap) device */
#include "memory_.h"
#include "gx.h"
#include "gxdevice.h"
#include "gxdevmem.h"			/* semi-public definitions */
#include "gdevmem.h"			/* private definitions */

extern dev_proc_strip_copy_rop(mem_mono_strip_copy_rop);  /* in gdevmrop.c */

/* Optionally, use the slow RasterOp implementations for testing. */
/*#define USE_COPY_ROP*/

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
const gx_device_memory far_data mem_mono_device =
  mem_full_alpha_device("image1", 0, 1, mem_open,
    mem_mono_map_rgb_color, mem_mono_map_color_rgb,
    mem_mono_copy_mono, gx_default_copy_color, mem_mono_fill_rectangle,
    mem_get_bits, gx_default_map_cmyk_color, gx_no_copy_alpha,
    mem_mono_strip_tile_rectangle, mem_mono_strip_copy_rop);

/* Map color to/from RGB.  This may be inverted. */
private gx_color_index
mem_mono_map_rgb_color(gx_device *dev, gx_color_value r, gx_color_value g,
  gx_color_value b)
{	return (gx_default_w_b_map_rgb_color(dev, r, g, b) ^
		mdev->palette.data[0]) & 1;
}
private int
mem_mono_map_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{	return gx_default_w_b_map_color_rgb(dev,
			(color ^ mdev->palette.data[0]) & 1,
			prgb);
}

/* Fill a rectangle with a color. */
private int
mem_mono_fill_rectangle(gx_device *dev, int x, int y, int w, int h,
  gx_color_index color)
{
#ifdef USE_COPY_ROP
	return mem_mono_copy_rop(dev, NULL, 0, 0, gx_no_bitmap_id, NULL,
				 NULL, NULL,
				 x, y, w, h, 0, 0,
				 (color ? rop3_1 : rop3_0));
#else
	fit_fill(dev, x, y, w, h);
	bits_fill_rectangle(scan_line_base(mdev, y), x, mdev->raster,
			    -(mono_fill_chunk)color, w, h);
	return 0;
#endif
}

/* Convert x coordinate to byte offset in scan line. */
#define x_to_byte(x) ((x) >> 3)

/* Copy a monochrome bitmap. */
#undef mono_masks
#define mono_masks mono_copy_masks

/* Fetch a chunk from the source. */
/* The source data are always stored big-endian. */
/* Note that the macros always cast cptr, */
/* so it doesn't matter what the type of cptr is. */
/* cshift = chunk_bits - shift. */
#undef chunk
#if arch_is_big_endian
#  define chunk uint
#  define cfetch_right(cptr, shift, cshift)\
	(cfetch_aligned(cptr) >> shift)
#  define cfetch_left(cptr, shift, cshift)\
	(cfetch_aligned(cptr) << shift)
/* Fetch a chunk that straddles a chunk boundary. */
#  define cfetch2(cptr, cskew, skew)\
    (cfetch_left(cptr, cskew, skew) +\
     cfetch_right((const chunk *)(cptr) + 1, skew, cskew))
#else				/* little-endian */
#  define chunk bits16
private const bits16 right_masks2[9] = {
	0xffff, 0x7f7f, 0x3f3f, 0x1f1f, 0x0f0f, 0x0707, 0x0303, 0x0101, 0x0000
};
private const bits16 left_masks2[9] = {
	0xffff, 0xfefe, 0xfcfc, 0xf8f8, 0xf0f0, 0xe0e0, 0xc0c0, 0x8080, 0x0000
};
#  define ccont(cptr, off) (((const chunk *)(cptr))[off])
#  define cfetch_right(cptr, shift, cshift)\
	((shift) < 8 ?\
	 ((ccont(cptr, 0) >> (shift)) & right_masks2[shift]) +\
	  (ccont(cptr, 0) << (cshift)) :\
	 ((chunk)*(const byte *)(cptr) << (cshift)) & 0xff00)
#  define cfetch_left(cptr, shift, cshift)\
	((shift) < 8 ?\
	 ((ccont(cptr, 0) << (shift)) & left_masks2[shift]) +\
	  (ccont(cptr, 0) >> (cshift)) :\
	 ((ccont(cptr, 0) & 0xff00) >> (cshift)) & 0xff)
/* Fetch a chunk that straddles a chunk boundary. */
/* We can avoid testing the shift amount twice */
/* by expanding the cfetch_left/right macros in-line. */
#  define cfetch2(cptr, cskew, skew)\
	((cskew) < 8 ?\
	 ((ccont(cptr, 0) << (cskew)) & left_masks2[cskew]) +\
	  (ccont(cptr, 0) >> (skew)) +\
	  (((chunk)(((const byte *)(cptr))[2]) << (cskew)) & 0xff00) :\
	 (((ccont(cptr, 0) & 0xff00) >> (skew)) & 0xff) +\
	  ((ccont(cptr, 1) >> (skew)) & right_masks2[skew]) +\
	   (ccont(cptr, 1) << (cskew)))
#endif
/* Since source and destination are both always big-endian, */
/* fetching an aligned chunk never requires byte swapping. */
#  define cfetch_aligned(cptr)\
	(*(const chunk *)(cptr))

/* copy_function and copy_shift get added together for dispatch */
typedef enum {
	copy_or = 0, copy_store, copy_and, copy_funny
} copy_function;
/* copy_right/left is not an enum, because compilers complain about */
/* an enumeration clash when these are added to a copy_function. */
#define copy_right ((copy_function)0)
#define copy_left ((copy_function)4)
typedef struct {
	short invert;
	ushort op;			/* copy_function */
} copy_mode;
/* Map from <c0,c1> to copy_mode. */
#define cm(i,op) { i, (ushort)op }
private copy_mode copy_modes[9] = {
	cm(-1, copy_funny),		/* NN */
	cm(-1, copy_and),		/* N0 */
	cm(0, copy_or),			/* N1 */
	cm(0, copy_and),		/* 0N */
	cm(0, copy_funny),		/* 00 */
	cm(0, copy_store),		/* 01 */
	cm(-1, copy_or),		/* 1N */
	cm(-1, copy_store),		/* 10 */
	cm(0, copy_funny),		/* 11 */
};
private int
mem_mono_copy_mono(gx_device *dev,
  const byte *base, int sourcex, int sraster, gx_bitmap_id id,
  int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{
#ifdef USE_COPY_ROP
	return mem_mono_copy_rop(dev, base, sourcex, sraster, id, NULL,
				 NULL, NULL,
				 x, y, w, h, 0, 0,
				 ((zero == gx_no_color_index ? rop3_D :
				   zero == 0 ? rop3_0 : rop3_1) & ~rop3_S) |
				 ((one ==  gx_no_color_index ? rop3_D :
				   one == 0 ? rop3_0 : rop3_1) & rop3_S));
#else				/* !USE_COPY_ROP */
	register const byte *bptr;		/* actually chunk * */
	int dbit, wleft;
	uint mask;
	copy_mode mode;
#define function (copy_function)(mode.op)
	declare_scan_ptr_as(dbptr, byte *);
#define optr ((chunk *)dbptr)
	register int skew;
	register uint invert;
	fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
#if gx_no_color_index_value != -1		/* hokey! */
	if ( zero == gx_no_color_index ) zero = -1;
	if ( one == gx_no_color_index ) one = -1;
#endif
#define izero (int)zero
#define ione (int)one
	mode = copy_modes[izero + izero + izero + ione + 4];
#undef izero
#undef ione
	invert = (uint)(int)mode.invert;	/* load register */
	setup_rect_as(dbptr, byte *);
	bptr = base + ((sourcex & ~chunk_align_bit_mask) >> 3);
	dbit = x & chunk_align_bit_mask;
	skew = dbit - (sourcex & chunk_align_bit_mask);
/* Macros for writing partial chunks. */
/* The destination pointer is always named optr, */
/* and must be declared as chunk *. */
/* cinvert may be temporarily redefined. */
#define cinvert(bits) ((bits) ^ invert)
#define write_or_masked(bits, mask, off)\
  optr[off] |= (cinvert(bits) & mask)
#define write_store_masked(bits, mask, off)\
  optr[off] = ((optr[off] & ~mask) | (cinvert(bits) & mask))
#define write_and_masked(bits, mask, off)\
  optr[off] &= (cinvert(bits) | ~mask)
/* Macros for writing full chunks. */
#define write_or(bits)  *optr |= cinvert(bits)
#define write_store(bits) *optr = cinvert(bits)
#define write_and(bits) *optr &= cinvert(bits)
/* Macro for incrementing to next chunk. */
#define next_x_chunk\
  bptr += chunk_bytes; dbptr += chunk_bytes
/* Common macro for the end of each scan line. */
#define end_y_loop(sdelta, ddelta)\
  if ( --h == 0 ) break;\
  bptr += sdelta; dbptr += ddelta
	if ( (wleft = w + dbit - chunk_bits) <= 0 )
	   {	/* The entire operation fits in one (destination) chunk. */
		set_mono_thin_mask(mask, w, dbit);
#define write_single(wr_op, src)\
  for ( ; ; )\
   { wr_op(src, mask, 0);\
     end_y_loop(sraster, draster);\
   }
#define write1_loop(src)\
  switch ( function ) {\
    case copy_or: write_single(write_or_masked, src); break;\
    case copy_store: write_single(write_store_masked, src); break;\
    case copy_and: write_single(write_and_masked, src); break;\
    default: goto funny;\
  }
		if ( skew >= 0 )	/* single -> single, right/no shift */
		{	if ( skew == 0 )	/* no shift */
			{	write1_loop(cfetch_aligned(bptr));
			}
			else			/* right shift */
			{	int cskew = chunk_bits - skew;
				write1_loop(cfetch_right(bptr, skew, cskew));
			}
		}
		else if ( wleft <= skew )	/* single -> single, left shift */
		{	int cskew = chunk_bits + skew;
			skew = -skew;
			write1_loop(cfetch_left(bptr, skew, cskew));
		}
		else			/* double -> single */
		{	int cskew = -skew;
			skew += chunk_bits;
			write1_loop(cfetch2(bptr, cskew, skew));
		}
#undef write1_loop
#undef write_single
	   }
	else if ( wleft <= skew )
	   {	/* 1 source chunk -> 2 destination chunks. */
		/* This is an important special case for */
		/* both characters and halftone tiles. */
		uint rmask;
		int cskew = chunk_bits - skew;
		set_mono_left_mask(mask, dbit);
		set_mono_right_mask(rmask, wleft);
#undef cinvert
#define cinvert(bits) (bits)		/* pre-inverted here */
#if arch_is_big_endian			/* no byte swapping */
#  define write_1to2(wr_op)\
  for ( ; ; )\
   { register uint bits = cfetch_aligned(bptr) ^ invert;\
     wr_op(bits >> skew, mask, 0);\
     wr_op(bits << cskew, rmask, 1);\
     end_y_loop(sraster, draster);\
   }
#else					/* byte swapping */
#  define write_1to2(wr_op)\
  for ( ; ; )\
   { wr_op(cfetch_right(bptr, skew, cskew) ^ invert, mask, 0);\
     wr_op(cfetch_left(bptr, cskew, skew) ^ invert, rmask, 1);\
     end_y_loop(sraster, draster);\
   }
#endif
		switch ( function )
		   {
		case copy_or: write_1to2(write_or_masked); break;
		case copy_store: write_1to2(write_store_masked); break;
		case copy_and: write_1to2(write_and_masked); break;
		default: goto funny;
		   }
#undef cinvert
#define cinvert(bits) ((bits) ^ invert)
#undef write_1to2
	   }
	else
	   {	/* More than one source chunk and more than one */
		/* destination chunk are involved. */
		uint rmask;
		int words = (wleft & ~chunk_bit_mask) >> 3;
		uint sskip = sraster - words;
		uint dskip = draster - words;
		register uint bits;
		set_mono_left_mask(mask, dbit);
		set_mono_right_mask(rmask, wleft & chunk_bit_mask);
		if ( skew == 0 )	/* optimize the aligned case */
		   {
#define write_aligned(wr_op, wr_op_masked)\
  for ( ; ; )\
   { int count = wleft;\
     /* Do first partial chunk. */\
     wr_op_masked(cfetch_aligned(bptr), mask, 0);\
     /* Do full chunks. */\
     while ( (count -= chunk_bits) >= 0 )\
      { next_x_chunk; wr_op(cfetch_aligned(bptr)); }\
     /* Do last chunk */\
     if ( count > -chunk_bits )\
      { wr_op_masked(cfetch_aligned(bptr + chunk_bytes), rmask, 1); }\
     end_y_loop(sskip, dskip);\
   }
			switch ( function )
			  {
			  case copy_or:
			    write_aligned(write_or, write_or_masked);
			    break;
			  case copy_store:
			    write_aligned(write_store, write_store_masked);
			    break;
			  case copy_and:
			    write_aligned(write_and, write_and_masked);
			    break;
			  default:
			    goto funny;
			  }
#undef write_aligned
		   }
		else			/* not aligned */
		   {	int ccase =
			  (skew >= 0 ? copy_right :
			   ((bptr += chunk_bytes), copy_left))
			  + (int)function;
			int cskew = -skew & chunk_bit_mask;
			skew &= chunk_bit_mask;
			for ( ; ; )
			   {	int count = wleft;
#define prefetch_right\
  bits = cfetch_right(bptr, skew, cskew)
#define prefetch_left\
  bits = cfetch2(bptr - chunk_bytes, cskew, skew)
#define write_unaligned(wr_op, wr_op_masked)\
  wr_op_masked(bits, mask, 0);\
  /* Do full chunks. */\
  while ( count >= chunk_bits )\
    { bits = cfetch2(bptr, cskew, skew);\
      next_x_chunk; wr_op(bits); count -= chunk_bits;\
    }\
  /* Do last chunk */\
  if ( count > 0 )\
    { bits = cfetch_left(bptr, cskew, skew);\
      if ( count > skew ) bits += cfetch_right(bptr + chunk_bytes, skew, cskew);\
      wr_op_masked(bits, rmask, 1);\
    }
				switch ( ccase )
				  {
				  case copy_or + copy_left:
				    prefetch_left; goto uor;
				  case copy_or + copy_right:
				    prefetch_right;
uor:				    write_unaligned(write_or, write_or_masked);
				    break;
				  case copy_store + copy_left:
				    prefetch_left; goto ustore;
				  case copy_store + copy_right:
				    prefetch_right;
ustore:				    write_unaligned(write_store, write_store_masked);
				    break;
				  case copy_and + copy_left:
				    prefetch_left; goto uand;
				  case copy_and + copy_right:
				    prefetch_right;
uand:				    write_unaligned(write_and, write_and_masked);
				    break;
				  default:
				    goto funny;
				  }
				end_y_loop(sskip, dskip);
#undef write_unaligned
#undef prefetch_left
#undef prefetch_right
			   }
		   }
	   }
#undef end_y_loop
#undef next_x_chunk
	return 0;
	/* Handle the funny cases that aren't supposed to happen. */
funny:	return (invert ? -1 : mem_mono_fill_rectangle(dev, x, y, w, h, zero));
#undef optr
#endif				/* !USE_COPY_ROP */
}

#if OPTIMIZE_TILE		/**************** ****************/

/* Strip-tile with a monochrome halftone. */
/* This is a performance bottleneck for monochrome devices, */
/* so we re-implement it, even though it takes a lot of code. */
private int
mem_mono_strip_tile_rectangle(gx_device *dev,
  register const gx_strip_bitmap *tiles,
  int tx, int y, int tw, int th, gx_color_index color0, gx_color_index color1,
  int px, int py)
{
#ifdef USE_COPY_ROP
	return mem_mono_strip_copy_rop(dev, NULL, 0, 0, tile->id, NULL,
				tiles, NULL,
				tx, y, tw, th, px, py,
				((color0 == gx_no_color_index ? rop3_D :
				  color0 == 0 ? rop3_0 : rop3_1) & ~rop3_T) |
				((color1 ==  gx_no_color_index ? rop3_D :
				  color1 == 0 ? rop3_0 : rop3_1) & rop3_T));
#else				/* !USE_COPY_ROP */
	register uint invert;
	int sraster;
	uint tile_bits_size;
	const byte *base;
	const byte *end;
	int x, rw, w, h;
	register const byte *bptr;		/* actually chunk * */
	int dbit, wleft;
	uint mask;
	byte *dbase;
	declare_scan_ptr_as(dbptr, byte *);
#define optr ((chunk *)dbptr)
	register int skew;

	/* This implementation doesn't handle strips yet. */
	if ( color0 != (color1 ^ 1) || tiles->shift != 0 )
	  return gx_default_strip_tile_rectangle(dev, tiles, tx, y, tw, th,
						 color0, color1, px, py);
	fit_fill(dev, tx, y, tw, th);
	invert = -(uint)color0;
	sraster = tiles->raster;
	base = tiles->data + ((y + py) % tiles->rep_height) * sraster;
	tile_bits_size = tiles->size.y * sraster;
	end = tiles->data + tile_bits_size;
#undef end_y_loop
#define end_y_loop(sdelta, ddelta)\
  if ( --h == 0 ) break;\
  if ( end - bptr <= sdelta )	/* wrap around */\
    bptr -= tile_bits_size;\
  bptr += sdelta; dbptr += ddelta
	draster = mdev->raster;
	dbase = scan_line_base(mdev, y);
	x = tx;
	rw = tw;
	/*
	 * The outermost loop here works horizontally, one iteration per
	 * copy of the tile.  Note that all iterations except the first
	 * have sourcex = 0.
	 */
	{ int sourcex = (x + px) % tiles->rep_width;
	  w = tiles->size.x - sourcex;
	  bptr = base + ((sourcex & ~chunk_align_bit_mask) >> 3);
	  dbit = x & chunk_align_bit_mask;
	  skew = dbit - (sourcex & chunk_align_bit_mask);
	}
outer:	if ( w > rw )
	  w = rw;
	h = th;
	dbptr = dbase + ((x >> 3) & -chunk_align_bytes);
	if ( (wleft = w + dbit - chunk_bits) <= 0 )
	   {	/* The entire operation fits in one (destination) chunk. */
		set_mono_thin_mask(mask, w, dbit);
#define write1_loop(src)\
  for ( ; ; )\
   { write_store_masked(src, mask, 0);\
     end_y_loop(sraster, draster);\
   }
		if ( skew >= 0 )	/* single -> single, right/no shift */
		{	if ( skew == 0 )	/* no shift */
			{	write1_loop(cfetch_aligned(bptr));
			}
			else			/* right shift */
			{	int cskew = chunk_bits - skew;
				write1_loop(cfetch_right(bptr, skew, cskew));
			}
		}
		else if ( wleft <= skew )	/* single -> single, left shift */
		{	int cskew = chunk_bits + skew;
			skew = -skew;
			write1_loop(cfetch_left(bptr, skew, cskew));
		}
		else			/* double -> single */
		{	int cskew = -skew;
			skew += chunk_bits;
			write1_loop(cfetch2(bptr, cskew, skew));
		}
#undef write1_loop
	  }
	else if ( wleft <= skew )
	  {	/* 1 source chunk -> 2 destination chunks. */
		/* This is an important special case for */
		/* both characters and halftone tiles. */
		uint rmask;
		int cskew = chunk_bits - skew;
		set_mono_left_mask(mask, dbit);
		set_mono_right_mask(rmask, wleft);
#if arch_is_big_endian			/* no byte swapping */
#undef cinvert
#define cinvert(bits) (bits)		/* pre-inverted here */
		for ( ; ; )
		  { register uint bits = cfetch_aligned(bptr) ^ invert;
		    write_store_masked(bits >> skew, mask, 0);
		    write_store_masked(bits << cskew, rmask, 1);
		    end_y_loop(sraster, draster);
		  }
#undef cinvert
#define cinvert(bits) ((bits) ^ invert)
#else					/* byte swapping */
		for ( ; ; )
		  { write_store_masked(cfetch_right(bptr, skew, cskew), mask, 0);
		    write_store_masked(cfetch_left(bptr, cskew, skew), rmask, 1);
		    end_y_loop(sraster, draster);
		  }
#endif
	  }
	else
	   {	/* More than one source chunk and more than one */
		/* destination chunk are involved. */
		uint rmask;
		int words = (wleft & ~chunk_bit_mask) >> 3;
		uint sskip = sraster - words;
		uint dskip = draster - words;
		register uint bits;
#define next_x_chunk\
  bptr += chunk_bytes; dbptr += chunk_bytes

		set_mono_right_mask(rmask, wleft & chunk_bit_mask);
		if ( skew == 0 )	/* optimize the aligned case */
		   {	if ( dbit == 0 )
			  mask = 0;
			else
			  set_mono_left_mask(mask, dbit);
			for ( ; ; )
			  { int count = wleft;
			    /* Do first partial chunk. */
			    if ( mask )
			      write_store_masked(cfetch_aligned(bptr), mask, 0);
			    else
			      write_store(cfetch_aligned(bptr));
			    /* Do full chunks. */
			    while ( (count -= chunk_bits) >= 0 )
			      { next_x_chunk;
				write_store(cfetch_aligned(bptr));
			      }
			    /* Do last chunk */
			    if ( count > -chunk_bits )
			      { write_store_masked(cfetch_aligned(bptr + chunk_bytes), rmask, 1);
			      }
			    end_y_loop(sskip, dskip);
			  }
		   }
		else			/* not aligned */
		   {	bool case_right =
			  (skew >= 0 ? true :
			   ((bptr += chunk_bytes), false));
			int cskew = -skew & chunk_bit_mask;

			skew &= chunk_bit_mask;
			set_mono_left_mask(mask, dbit);
			for ( ; ; )
			   {	int count = wleft;
				if ( case_right )
				  bits = cfetch_right(bptr, skew, cskew);
				else
				  bits = cfetch2(bptr - chunk_bytes, cskew, skew);
				write_store_masked(bits, mask, 0);
				/* Do full chunks. */
				while ( count >= chunk_bits )
				  { bits = cfetch2(bptr, cskew, skew);
				    next_x_chunk;
				    write_store(bits);
				    count -= chunk_bits;
				  }
				/* Do last chunk */
				if ( count > 0 )
				  { bits = cfetch_left(bptr, cskew, skew);
				    if ( count > skew )
				      bits += cfetch_right(bptr + chunk_bytes, skew, cskew);
				    write_store_masked(bits, rmask, 1);
				  }
				end_y_loop(sskip, dskip);
			   }
		   }
	   }
#undef end_y_loop
#undef next_x_chunk
#undef optr
	if ( (rw -= w) > 0 )
	  {	x += w;
		w = tiles->size.x;
		bptr = base;
		skew = dbit = x & chunk_align_bit_mask;
		goto outer;
	  }
	return 0;
#endif				/* !USE_COPY_ROP */
}

#endif				/**************** ****************/

/* ================ "Word"-oriented device ================ */

/* Note that on a big-endian machine, this is the same as the */
/* standard byte-oriented-device. */

#if !arch_is_big_endian

/* Procedures */
private dev_proc_copy_mono(mem1_word_copy_mono);
private dev_proc_fill_rectangle(mem1_word_fill_rectangle);
#define mem1_word_strip_tile_rectangle gx_default_strip_tile_rectangle

/* Here is the device descriptor. */
const gx_device_memory far_data mem_mono_word_device =
  mem_full_alpha_device("image1w", 0, 1, mem_open,
    mem_mono_map_rgb_color, mem_mono_map_color_rgb,
    mem1_word_copy_mono, gx_default_copy_color, mem1_word_fill_rectangle,
    mem_word_get_bits, gx_default_map_cmyk_color, gx_no_copy_alpha,
    mem1_word_strip_tile_rectangle, gx_no_strip_copy_rop);

/* Fill a rectangle with a color. */
private int
mem1_word_fill_rectangle(gx_device *dev, int x, int y, int w, int h,
  gx_color_index color)
{	byte *base;
	uint raster;
	fit_fill(dev, x, y, w, h);
	base = scan_line_base(mdev, y);
	raster = mdev->raster;
	mem_swap_byte_rect(base, raster, x, w, h, true);
	bits_fill_rectangle(base, x, raster, -(mono_fill_chunk)color, w, h);
	mem_swap_byte_rect(base, raster, x, w, h, true);
	return 0;
}

/* Copy a bitmap. */
private int
mem1_word_copy_mono(gx_device *dev,
  const byte *base, int sourcex, int sraster, gx_bitmap_id id,
  int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{	byte *row;
	uint raster;
	bool store;
	fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
	row = scan_line_base(mdev, y);
	raster = mdev->raster;
	store = (zero != gx_no_color_index && one != gx_no_color_index);
	mem_swap_byte_rect(row, raster, x, w, h, store);
	mem_mono_copy_mono(dev, base, sourcex, sraster, id,
			   x, y, w, h, zero, one);
	mem_swap_byte_rect(row, raster, x, w, h, false);
	return 0;
}

#endif				/* !arch_is_big_endian */
