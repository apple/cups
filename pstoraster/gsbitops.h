/* Copyright (C) 1991, 1995, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsbitops.h,v 1.2 2000/03/08 23:14:33 mike Exp $ */
/* Interface for bitmap operations */

#ifndef gsbitops_INCLUDED
#  define gsbitops_INCLUDED

/* ---------------- Pixel processing macros ---------------- */

/*
 * These macros support code that processes data pixel-by-pixel (or, to be
 * more accurate, packed arrays of values -- they may be complete pixels
 * or individual components of pixels).
 *
 * Supported #s of bits per value (bpv) are 1, 2, 4, 8, 12[, 16[, 24, 32]].
 * The suffix 12, 16, or 32 on a macro name indicates the maximum value
 * of bpv that the macro is prepared to handle.
 *
 * The setup macros number bits within a byte in big-endian order, i.e.,
 * 0x80 is bit 0, 0x01 is bit 7.  However, sbit/dbit may use a different
 * representation for better performance.  ****** NYI ******
 */

#define sample_end_\
  default: return_error(gs_error_rangecheck);\
  } END

/* Declare variables for loading. */
#define sample_load_declare(sptr, sbit)\
  const byte *sptr;\
  int sbit
#define sample_load_declare_setup(sptr, sbit, ptr, bitno, sbpv)\
  const byte *sptr = (ptr);\
  int sample_load_setup(sbit, bitno, sbpv)

/* Set up to load starting at a given bit number. */
#define sample_load_setup(sbit, bitno, sbpv)\
  sbit = (bitno)

/* Load a value from memory, without incrementing. */
#define sample_load12_(value, sptr, sbit, sbpv)\
  BEGIN\
  switch ( (sbpv) >> 2 ) {\
  case 0: value = (*(sptr) >> (8 - (sbit) - (sbpv))) & ((sbpv) - 1); break;\
  case 1: value = (*(sptr) >> (4 - (sbit))) & 0xf; break;\
  case 2: value = *(sptr); break;\
  case 3:\
    value = ((sbit) ? ((*(sptr) & 0xf) << 8) | (sptr)[1] :\
	      (*(sptr) << 4) | ((sptr)[1] >> 4));\
    break;
#define sample_load12(value, sptr, sbit, sbpv)\
  sample_load12_(value, sptr, sbit, sbpv)\
  sample_end_
#define sample_load_next12(value, sptr, sbit, sbpv)\
  sample_load12(value, sptr, sbit, sbpv);\
  sample_next(sptr, sbit, sbpv)
#define sample_load16_(value, sptr, sbit, sbpv)\
  sample_load12_(value, sptr, sbit, sbpv)\
  case 4: value = (*(sptr) << 8) | (sptr)[1]; break;
#define sample_load16(value, sptr, sbit, sbpv)\
  sample_load16_(value, sptr, sbit, sbpv)\
  sample_end_
#define sample_load_next16(value, sptr, sbit, sbpv)\
  sample_load16(value, sptr, sbit, sbpv);\
  sample_next(sptr, sbit, sbpv)
#define sample_load32(value, sptr, sbit, sbpv)\
  sample_load16_(value, sptr, sbit, sbpv)\
  case 6: value = (*(sptr) << 16) | ((sptr)[1] << 8) | (sptr)[2]; break;\
  case 8:\
    value = (*(sptr) << 24) | ((sptr)[1] << 16) | ((sptr)[2] << 8) | sptr[3];\
    break;\
  sample_end_
#define sample_load_next32(value, sptr, sbit, sbpv)\
  sample_load32(value, sptr, sbit, sbpv);\
  sample_next(sptr, sbit, sbpv)

/* Declare variables for storing. */
#define sample_store_declare(dptr, dbit, dbbyte)\
  byte *dptr;\
  int dbit;\
  byte dbbyte			/* maybe should be uint? */
#define sample_store_declare_setup(dptr, dbit, dbbyte, ptr, bitno, dbpv)\
  byte *dptr = (ptr);\
  int sample_store_setup(dbit, bitno, dbpv);\
  byte /* maybe should be uint? */\
    sample_store_preload(dbbyte, dptr, dbit, dbpv)

/* Set up to store starting at a given bit number. */
#define sample_store_setup(dbit, bitno, dbpv)\
  dbit = (bitno)

/* Prepare for storing by preloading any partial byte. */
#define sample_store_preload(dbbyte, dptr, dbit, dbpv)\
  dbbyte = ((dbit) ? (byte)(*(dptr) & (0xff00 >> (dbit))) : 0)

/* Store a value and increment the pointer. */
#define sample_store_next12_(value, dptr, dbit, dbpv, dbbyte)\
  BEGIN\
  switch ( (dbpv) >> 2 ) {\
  case 0:\
    if ( (dbit += (dbpv)) == 8 )\
       *(dptr)++ = dbbyte | (value), dbbyte = 0, dbit = 0;\
    else dbbyte |= (value) << (8 - dbit);\
    break;\
  case 1:\
    if ( dbit ^= 4 ) dbbyte = (byte)((value) << 4);\
    else *(dptr)++ = dbbyte | (value);\
    break;\
  /* case 2 is deliberately omitted */\
  case 3:\
    if ( dbit ^= 4 ) *(dptr)++ = (value) >> 4, dbbyte = (byte)((value) << 4);\
    else\
      *(dptr) = dbbyte | ((value) >> 8), (dptr)[1] = (byte)(value), dptr += 2;\
    break;
#define sample_store_next12(value, dptr, dbit, dbpv, dbbyte)\
  sample_store_next12_(value, dptr, dbit, dbpv, dbbyte)\
  case 2: *(dptr)++ = (byte)(value); break;\
  sample_end_
#define sample_store_next16(value, dptr, dbit, dbpv, dbbyte)\
  sample_store_next12_(value, dptr, dbit, dbpv, dbbyte)\
  case 4: *(dptr)++ = (byte)((value) >> 8);\
  case 2: *(dptr)++ = (byte)(value); break;\
  sample_end_
#define sample_store_next32(value, dptr, dbit, dbpv, dbbyte)\
  sample_store_next12_(value, dptr, dbit, dbpv, dbbyte)\
  case 8: *(dptr)++ = (byte)((value) >> 24);\
  case 6: *(dptr)++ = (byte)((value) >> 16);\
  case 4: *(dptr)++ = (byte)((value) >> 8);\
  case 2: *(dptr)++ = (byte)(value); break;\
  sample_end_

/* Skip over storing one sample.  This may or may not store into the */
/* skipped region. */
#define sample_store_skip_next(dptr, dbit, dbpv, dbbyte)\
  if ( (dbpv) < 8 ) {\
    sample_store_flush(dptr, dbit, dbpv, dbbyte);\
    sample_next(dptr, dbit, dbpv);\
  } else dptr += ((dbpv) >> 3)

/* Finish storing by flushing any partial byte. */
#define sample_store_flush(dptr, dbit, dbpv, dbbyte)\
  if ( (dbit) != 0 )\
    *(dptr) = dbbyte | (*(dptr) & (0xff >> (dbit)));

/* Increment a pointer to the next sample. */
#define sample_next(ptr, bit, bpv)\
  BEGIN bit += (bpv); ptr += bit >> 3; bit &= 7; END

/* ---------------- Definitions ---------------- */

/*
 * Define the chunk size for monobit filling operations.
 * This is always uint, regardless of byte order.
 */
#define mono_fill_chunk uint
#define mono_fill_chunk_bytes arch_sizeof_int

/* ---------------- Procedures ---------------- */

/* Fill a rectangle of bits with an 8x1 pattern. */
/* The pattern argument must consist of the pattern in every byte, */
/* e.g., if the desired pattern is 0xaa, the pattern argument must */
/* have the value 0xaaaa (if ints are short) or 0xaaaaaaaa. */
#if mono_fill_chunk_bytes == 2
#  define mono_fill_make_pattern(byt) (uint)((uint)(byt) * 0x0101)
#else
#  define mono_fill_make_pattern(byt) (uint)((uint)(byt) * 0x01010101)
#endif
void bits_fill_rectangle(P6(byte * dest, int dest_bit, uint raster,
		      mono_fill_chunk pattern, int width_bits, int height));

/* Replicate a bitmap horizontally in place. */
void bits_replicate_horizontally(P6(byte * data, uint width, uint height,
	       uint raster, uint replicated_width, uint replicated_raster));

/* Replicate a bitmap vertically in place. */
void bits_replicate_vertically(P4(byte * data, uint height, uint raster,
				  uint replicated_height));

/* Find the bounding box of a bitmap. */
void bits_bounding_box(P4(const byte * data, uint height, uint raster,
			  gs_int_rect * pbox));

/* Compress an oversampled image, possibly in place. */
/* The width and height must be multiples of the respective scale factors. */
/* The source must be an aligned bitmap, as usual. */
void bits_compress_scaled(P9(const byte * src, int srcx, uint width,
		       uint height, uint sraster, byte * dest, uint draster,
	       const gs_log2_scale_point * plog2_scale, int log2_out_bits));

/* Fill a rectangle of bytes. */
void bytes_fill_rectangle(P5(byte * dest, uint raster,
			     byte value, int width_bytes, int height));

/* Copy a rectangle of bytes. */
void bytes_copy_rectangle(P6(byte * dest, uint dest_raster,
	   const byte * src, uint src_raster, int width_bytes, int height));

#endif /* gsbitops_INCLUDED */
