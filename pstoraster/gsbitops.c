/* Copyright (C) 1994, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gsbitops.c */
/* Bitmap filling, copying, and transforming operations */
#include "stdio_.h"
#include "memory_.h"
#include "gdebug.h"
#include "gstypes.h"
#include "gsbitops.h"

/*
 * Define a compile-time option to reverse nibble order in alpha maps.
 * Note that this does not reverse bit order within nibbles.
 * This option is here for a very specialized purpose and does not
 * interact well with the rest of the code.
 */
#ifndef ALPHA_LSB_FIRST
#  define ALPHA_LSB_FIRST 0
#endif

/* ---------------- Bit-oriented operations ---------------- */

/* Define masks for little-endian operation. */
/* masks[i] has the first i bits off and the rest on. */
#if !arch_is_big_endian
const bits16 mono_copy_masks[17] = {
	0xffff, 0xff7f, 0xff3f, 0xff1f,
	0xff0f, 0xff07, 0xff03, 0xff01,
	0xff00, 0x7f00, 0x3f00, 0x1f00,
	0x0f00, 0x0700, 0x0300, 0x0100,
	0x0000
};
#  if arch_sizeof_int > 2
const bits32 mono_fill_masks[33] = {
	0xffffffff, 0xffffff7f, 0xffffff3f, 0xffffff1f,
	0xffffff0f, 0xffffff07, 0xffffff03, 0xffffff01,
	0xffffff00, 0xffff7f00, 0xffff3f00, 0xffff1f00,
	0xffff0f00, 0xffff0700, 0xffff0300, 0xffff0100,
	0xffff0000, 0xff7f0000, 0xff3f0000, 0xff1f0000,
	0xff0f0000, 0xff070000, 0xff030000, 0xff010000,
	0xff000000, 0x7f000000, 0x3f000000, 0x1f000000,
	0x0f000000, 0x07000000, 0x03000000, 0x01000000,
	0x00000000
};
#  endif
#endif

/* Fill a rectangle of bits with an 8x1 pattern. */
/* The pattern argument must consist of the pattern in every byte, */
/* e.g., if the desired pattern is 0xaa, the pattern argument must */
/* have the value 0xaaaa (if ints are short) or 0xaaaaaaaa. */
#undef chunk
#define chunk mono_fill_chunk
#undef mono_masks
#define mono_masks mono_fill_masks
void
bits_fill_rectangle(byte *dest, int dest_bit, uint draster,
  mono_fill_chunk pattern, int width_bits, int height)
{	uint bit;
	chunk right_mask;

#define write_loop(stat)\
 { int line_count = height;\
   chunk *ptr = (chunk *)dest;\
   do { stat; inc_ptr(ptr, draster); }\
   while ( --line_count );\
 }

#define write_partial(msk)\
  switch ( (byte)pattern )\
  { case 0: write_loop(*ptr &= ~msk); break;\
    case 0xff: write_loop(*ptr |= msk); break;\
    default: write_loop(*ptr = (*ptr & ~msk) | (pattern & msk));\
  }

	dest += (dest_bit >> 3) & -chunk_align_bytes;
	bit = dest_bit & chunk_align_bit_mask;

#if 1			/* new code */

#define write_span(lmsk, stat0, statx, stat1, n, rmsk)\
  switch ( (byte)pattern )\
  { case 0: write_loop((*ptr &= ~lmsk, stat0, ptr[n] &= ~rmsk)); break;\
    case 0xff: write_loop((*ptr |= lmsk, stat1, ptr[n] |= rmsk)); break;\
    default: write_loop((*ptr = (*ptr & ~lmsk) | (pattern & lmsk), statx,\
			 ptr[n] = (ptr[n] & ~rmsk) | (pattern & rmsk)));\
  }

	{ int last_bit = width_bits + bit - (chunk_bits + 1);
	  if ( last_bit < 0 )		/* <=1 chunk */
	    { set_mono_thin_mask(right_mask, width_bits, bit);
	      write_partial(right_mask);
	    }
	  else
	    { chunk mask;
	      int last = last_bit >> chunk_log2_bits;
	      set_mono_left_mask(mask, bit);
	      set_mono_right_mask(right_mask, (last_bit & chunk_bit_mask) + 1);
	      switch ( last )
		{
		case 0:			/* 2 chunks */
		  { write_span(mask, 0, 0, 0, 1, right_mask);
		  } break;
		case 1:			/* 3 chunks */
		  { write_span(mask, ptr[1] = 0, ptr[1] = pattern,
			       ptr[1] = ~(chunk)0, 2, right_mask);
		  } break;
		default:		/* >3 chunks */
		  { uint byte_count = (last_bit >> 3) & -chunk_bytes;
		    write_span(mask, memset(ptr + 1, 0, byte_count),
			       memset(ptr + 1, (byte)pattern, byte_count),
			       memset(ptr + 1, 0xff, byte_count),
			       last + 1, right_mask);
		  } break;
		}
	    }
	}

#else			/* old code */

	if ( bit + width_bits <= chunk_bits )
	   {	/* Only one word. */
		set_mono_thin_mask(right_mask, width_bits, bit);
	   }
	else
	   {	int byte_count;
		if ( bit )
		   {	chunk mask;
			set_mono_left_mask(mask, bit);
			write_partial(mask);
			inc_ptr(dest, chunk_bytes);
			width_bits += bit - chunk_bits;
		   }
		set_mono_right_mask(right_mask, width_bits & chunk_bit_mask);
		if ( width_bits >= chunk_bits )
		  switch ( (byte_count = (width_bits >> 3) & -chunk_bytes) )
		{
		case chunk_bytes:
			write_loop(*ptr = pattern);
			inc_ptr(dest, chunk_bytes);
			break;
		case chunk_bytes * 2:
			write_loop(ptr[1] = *ptr = pattern);
			inc_ptr(dest, chunk_bytes * 2);
			break;
		default:
			write_loop(memset(ptr, (byte)pattern, byte_count));
			inc_ptr(dest, byte_count);
			break;
		}
	   }
	if ( right_mask )
		write_partial(right_mask);

#endif

}

/* Replicate a bitmap horizontally in place. */
void
bits_replicate_horizontally(byte *data, uint width, uint height,
  uint raster, uint replicated_width, uint replicated_raster)
{	/* The current algorithm is extremely inefficient. */
	uint y;
	for ( y = height; y-- > 0; )
	  {	const byte *orig_row = data + y * raster;
		byte *tile_row = data + y * replicated_raster;
		uint sx;
		if ( !(width & 7) )
		{ uint wbytes = width >> 3;
		  for ( sx = wbytes; sx-- > 0; )
		    {	byte sb = orig_row[sx];
			uint dx;
			for ( dx = sx + (replicated_width >> 3); dx >= wbytes; )
			  tile_row[dx -= wbytes] = sb;
		    }
		}
		else
		  for ( sx = width; sx-- > 0; )
		    {	byte sm = orig_row[sx >> 3] & (0x80 >> (sx & 7));
			uint dx;
			for ( dx = sx + replicated_width; dx >= width;
			    )
			  {	byte *dp =
				  (dx -= width, tile_row + (dx >> 3));
				byte dm = 0x80 >> (dx & 7);
				if ( sm ) *dp |= dm;
				else *dp &= ~dm;
			  }
		    }
	  }
}

/* Replicate a bitmap vertically in place. */
void
bits_replicate_vertically(byte *data, uint height, uint raster,
  uint replicated_height)
{	byte *dest = data;
	uint h = replicated_height;
	uint size = raster * height;
	while ( h >= height )
	  {	memcpy(dest + size, dest, size);
		dest += size;
		h -= height;
	  }
}

/* Find the bounding box of a bitmap. */
/* Assume bits beyond the width are zero. */
void
bits_bounding_box(const byte *data, uint height, uint raster,
  gs_int_rect *pbox)
{	register const ulong *lp;
	static const byte first_1[16] =
	  { 4, 3, 2,2, 1,1,1,1, 0,0,0,0,0,0,0,0 };
	static const byte last_1[16] =
	  { 0,4,3,4,2,4,3,4,1,4,3,4,2,4,3,4 };

	/* Count trailing blank rows. */
	/* Since the raster is a multiple of sizeof(long), */
	/* we don't need to scan by bytes, only by longs. */

	lp = (const ulong *)(data + raster * height);
	while ( (const byte *)lp > data && !lp[-1] )
	  --lp;
	if ( (const byte *)lp == data )
	  {	pbox->p.x = pbox->q.x = pbox->p.y = pbox->q.y = 0;
		return;
	  }
	pbox->q.y = height = ((const byte *)lp - data + raster - 1) / raster;

	/* Count leading blank rows. */

	lp = (const ulong *)data;
	while ( !*lp )
	  ++lp;
	{ uint n = ((const byte *)lp - data) / raster;
	  pbox->p.y = n;
	  if ( n )
	    height -= n, data += n * raster;
	}

	/* Find the left and right edges. */
	/* We know that the first and last rows are non-blank. */

	{	uint raster_longs = raster >> arch_log2_sizeof_long;
		uint left = raster_longs - 1, right = 0;
		ulong llong = 0, rlong = 0;
		const byte *q;
		uint h, n;

		for ( q = data, h = height; h-- > 0; q += raster )
		  {	/* Work from the left edge by longs. */
			for ( lp = (const ulong *)q, n = 0;
			      n < left && !*lp; lp++, n++
			    )
			  ;
			if ( n < left )
			  left = n, llong = *lp;
			else
			  llong |= *lp;
			/* Work from the right edge by longs. */
			for ( lp = (const ulong *)(q + raster - sizeof(long)),
			        n = raster_longs - 1;
			      n > right && !*lp; lp--, n--
			    )
			  ;
			if ( n > right )
			  right = n, rlong = *lp;
			else
			  rlong |= *lp;
		  }

		/* Do binary subdivision on edge longs.  We assume that */
		/* sizeof(long) = 4 or 8. */
#if arch_sizeof_long > 8
		Error_longs_are_too_large();
#endif

#if arch_is_big_endian
#  define last_bits(n) ((1L << (n)) - 1)
#  define shift_out_last(x,n) ((x) >>= (n))
#  define right_justify_last(x,n) DO_NOTHING
#else
#  define last_bits(n) (-1L << ((arch_sizeof_long * 8) - (n)))
#  define shift_out_last(x,n) ((x) <<= (n))
#  define right_justify_last(x,n) (x) >>= ((arch_sizeof_long * 8) - (n))
#endif

		left <<= arch_log2_sizeof_long + 3;
#if arch_sizeof_long == 8
		if ( llong & ~last_bits(32) ) shift_out_last(llong, 32);
		else left += 32;
#endif
		if ( llong & ~last_bits(16) ) shift_out_last(llong, 16);
		else left += 16;
		if ( llong & ~last_bits(8) ) shift_out_last(llong, 8);
		else left += 8;
		right_justify_last(llong, 8);
		if ( llong & 0xf0 )
		  left += first_1[(byte)llong >> 4];
		else
		  left += first_1[(byte)llong] + 4;

		right <<= arch_log2_sizeof_long + 3;
#if arch_sizeof_long == 8
		if ( !(rlong & last_bits(32)) ) shift_out_last(rlong, 32);
		else right += 32;
#endif
		if ( !(rlong & last_bits(16)) ) shift_out_last(rlong, 16);
		else right += 16;
		if ( !(rlong & last_bits(8)) ) shift_out_last(rlong, 8);
		else right += 8;
		right_justify_last(rlong, 8);
		if ( !(rlong & 0xf) )
		  right += last_1[(byte)rlong >> 4];
		else
		  right += last_1[(uint)rlong & 0xf] + 4;

		pbox->p.x = left;
		pbox->q.x = right;
	}
}	

/* Count the number of 1-bits in a half-byte. */
static const byte half_byte_1s[16] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};
/* Count the number of trailing 1s in an up-to-5-bit value, -1. */
static const byte bits5_trailing_1s[32] =
{ 0,0,0,1,0,0,0,2,0,0,0,1,0,0,0,3,0,0,0,1,0,0,0,2,0,0,0,1,0,0,0,4 };
/* Count the number of leading 1s in an up-to-5-bit value, -1. */
static const byte bits5_leading_1s[32] =
{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,2,2,3,4 };

/*
 * Compress a value between 0 and 2^M to a value between 0 and 2^N-1.
 * Possible values of M are 1, 2, 3, or 4; of N are 1, 2, and 4.
 * The name of the table is compress_count_M_N.
 * As noted below, we require that N <= M.
 */
static const byte compress_1_1[3] =
	{ 0, 1, 1 };
static const byte compress_2_1[5] =
	{ 0, 0, 1, 1, 1 };
static const byte compress_2_2[5] =
	{ 0, 1, 2, 2, 3 };
static const byte compress_3_1[9] =
	{ 0, 0, 0, 0, 1, 1, 1, 1, 1 };
static const byte compress_3_2[9] =
	{ 0, 0, 1, 1, 2, 2, 2, 3, 3 };
static const byte compress_4_1[17] =
	{ 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static const byte compress_4_2[17] =
	{ 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3 };
static const byte compress_4_4[17] =
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 9,10,11,12,13,14,15 };
/* The table of tables is indexed by log2(N) and then by M-1. */
static const byte _ds *compress_tables[4][4] =
{	{ compress_1_1, compress_2_1, compress_3_1, compress_4_1 },
	{ 0, compress_2_2, compress_3_2, compress_4_2 },
	{ 0, 0, 0, compress_4_4 }
};

/*
 * Compress an XxY-oversampled bitmap to Nx1 by counting 1-bits.  The X and
 * Y oversampling factors are 1, 2, or 4, but may be different.  N, the
 * resulting number of (alpha) bits per pixel, may be 1, 2, or 4; we allow
 * compression in place, in which case N must not exceed the X oversampling
 * factor.  Width and height are the source dimensions, and hence reflect
 * the oversampling; both are multiples of the relevant scale factor.  The
 * same is true for srcx.
 */
void
bits_compress_scaled(const byte *src, int srcx, uint width, uint height,
  uint sraster, byte *dest, uint draster,
  const gs_log2_scale_point *plog2_scale, int log2_out_bits)
{	int log2_x = plog2_scale->x, log2_y = plog2_scale->y;
	int xscale = 1 << log2_x;
	int yscale = 1 << log2_y;
	int out_bits = 1 << log2_out_bits;
	int input_byte_out_bits;
	byte input_byte_out_mask;
	const byte _ds *table =
	  compress_tables[log2_out_bits][log2_x + log2_y - 1];
	uint sskip = sraster << log2_y;
	uint dwidth = (width >> log2_x) << log2_out_bits;
	uint dskip = draster - ((dwidth + 7) >> 3);
	uint mask = (1 << xscale) - 1;
	uint count_max = 1 << (log2_x + log2_y);
	/*
	 * For right now, we don't attempt to take advantage of the fact
	 * that the input is aligned.
	 */
	const byte *srow = src + (srcx >> 3);
	int in_shift_initial = 8 - xscale - (srcx & 7);
	int in_shift_check = (out_bits <= xscale ? 8 - xscale : -1);
	byte *d = dest;
	uint h;

	if ( out_bits <= xscale )
	  input_byte_out_bits = out_bits << (3 - log2_x),
	  input_byte_out_mask = (1 << input_byte_out_bits) - 1;
	for ( h = height; h; srow += sskip, h -= yscale )
	  {	const byte *s = srow;
#if ALPHA_LSB_FIRST
#  define out_shift_initial 0
#  define out_shift_update(out_shift, nbits) ((out_shift += (nbits)) >= 8)
#else
#  define out_shift_initial (8 - out_bits)
#  define out_shift_update(out_shift, nbits) ((out_shift -= (nbits)) < 0)
#endif
		int out_shift = out_shift_initial;
		byte out = 0;
		int in_shift = in_shift_initial;
		int dw = 8 - (srcx & 7);
		int w;

		/* Loop over source bytes. */
		for ( w = width; w > 0; w -= dw, dw = 8 )
		  { int index;
		    int in_shift_final =
		      (w >= dw ? 0 : dw - w);
		    /*
		     * Check quickly for all-0s or all-1s, but only if each
		     * input byte generates no more than one output byte,
		     * we're at an input byte boundary, and we're processing
		     * an entire input byte (i.e., this isn't a final
		     * partial byte.)
		     */
		    if ( in_shift == in_shift_check && in_shift_final == 0 )
		      switch ( *s )
		      {
		      case 0:
			for ( index = sraster; index != sskip; index += sraster )
			  if ( s[index] != 0 )
			    goto p;
			if ( out_shift_update(out_shift, input_byte_out_bits) )
			  *d++ = out, out_shift &= 7, out = 0;
			s++;
			continue;
#if !ALPHA_LSB_FIRST			/* too messy to make it work */
		      case 0xff:
			for ( index = sraster; index != sskip; index += sraster )
			  if ( s[index] != 0xff )
			    goto p;
			{ int shift =
			    (out_shift -= input_byte_out_bits) + out_bits;
			  if ( shift > 0 )
			    out |= input_byte_out_mask << shift;
			  else
			    { out |= input_byte_out_mask >> -shift;
			      *d++ = out;
			      out_shift += 8;
			      out = input_byte_out_mask << (8 + shift);
			    }
			}
			s++;
			continue;
#endif
		      default:
			;
		      }
p:		    /* Loop over source pixels within a byte. */
		    do
		      {	uint count;
			for ( index = 0, count = 0; index != sskip;
			      index += sraster
			    )
			  count += half_byte_1s[(s[index] >> in_shift) & mask];
			if ( count != 0 && table[count] == 0 )
			  { /* Look at adjacent cells to help prevent */
			    /* dropouts. */
			    uint orig_count = count;
			    uint shifted_mask = mask << in_shift;
			    byte in;
			    if_debug3('B', "[B]count(%d,%d)=%d\n",
				      (width - w) / xscale,
				      (height - h) / yscale, count);
			    if ( yscale > 1 )
			      { /* Look at the next "lower" cell. */
				if ( h < height && (in = s[0] & shifted_mask) != 0 )
				  { uint lower;
				    for ( index = 0, lower = 0;
					  -(index -= sraster) <= sskip &&
					  (in &= s[index]) != 0;
					)
				      lower += half_byte_1s[in >> in_shift];
				    if_debug1('B', "[B]  lower adds %d\n",
					      lower);
				    if ( lower <= orig_count )
				      count += lower;
				  }
				/* Look at the next "higher" cell. */
				if ( h > yscale && (in = s[sskip - sraster] & shifted_mask) != 0 )
				  { uint upper;
				    for ( index = sskip, upper = 0;
					  index < sskip << 1 &&
					  (in &= s[index]) != 0;
					  index += sraster
					)
				      upper += half_byte_1s[in >> in_shift];
				    if_debug1('B', "[B]  upper adds %d\n",
					      upper);
				    if ( upper < orig_count )
				      count += upper;
				  }
			      }
			    if ( xscale > 1 )
			      { uint mask1 = (mask << 1) + 1;
				/* Look at the next cell to the left. */
				if ( w < width )
				  { int lshift = in_shift + xscale - 1;
				    uint left;
				    for ( index = 0, left = 0;
					  index < sskip; index += sraster
					)
				      { uint bits =
					  ((s[index - 1] << 8) +
					   s[index]) >> lshift;
					left += bits5_trailing_1s[bits & mask1];
				      }
				    if_debug1('B', "[B]  left adds %d\n",
					      left);
				    if ( left < orig_count )
				      count += left;
				  }
				/* Look at the next cell to the right. */
				if ( w > xscale )
				  { int rshift = in_shift - xscale + 8;
				    uint right;
				    for ( index = 0, right = 0;
					  index < sskip; index += sraster
					)
				      { uint bits =
					  ((s[index] << 8) +
					   s[index + 1]) >> rshift;
					right += bits5_leading_1s[(bits & mask1) << (4 - xscale)];
				      }
				    if_debug1('B', "[B]  right adds %d\n",
					      right);
				    if ( right <= orig_count )
				      count += right;
				  }
			      }
			    if ( count > count_max )
			      count = count_max;
			  }
			out += table[count] << out_shift;
			if ( out_shift_update(out_shift, out_bits) )
			  *d++ = out, out_shift &= 7, out = 0;
		      }
		    while ( (in_shift -= xscale) >= in_shift_final );
		    s++, in_shift += 8;
		  }
		if ( out_shift != out_shift_initial )
		  *d++ = out;
		for ( w = dskip; w != 0; w-- )
		  *d++ = 0;
#undef out_shift_initial
#undef out_shift_update
	  }
}

/* ---------------- Byte-oriented operations ---------------- */

/* Fill a rectangle of bytes. */
void
bytes_fill_rectangle(byte *dest, uint raster,
  byte value, int width_bytes, int height)
{	while ( height-- > 0 )
	  {	memset(dest, value, width_bytes);
		dest += raster;
	  }
}

/* Copy a rectangle of bytes. */
void
bytes_copy_rectangle(byte *dest, uint dest_raster,
  const byte *src, uint src_raster, int width_bytes, int height)
{	while ( height-- > 0 )
	  {	memcpy(dest, src, width_bytes);
		src += src_raster;
		dest += dest_raster;
	  }
}
