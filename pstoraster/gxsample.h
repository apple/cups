/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxsample.h,v 1.1 2000/03/13 18:58:46 mike Exp $ */
/* Sample lookup and expansion */

#ifndef gxsample_INCLUDED
#  define gxsample_INCLUDED

/*
 * The following union implements the expansion of sample
 * values from N bits to 8, and a possible linear transformation.
 */
typedef union sample_lookup_s {
    bits32 lookup4x1to32[16];	/* 1 bit/sample, not spreading */
    bits16 lookup2x2to16[16];	/* 2 bits/sample, not spreading */
    byte lookup8[256];		/* 1 bit/sample, spreading [2] */
    /* 2 bits/sample, spreading [4] */
    /* 4 bits/sample [16] */
    /* 8 bits/sample [256] */
} sample_lookup_t;

/*
 * Define identity and inverted expansion lookups for 1-bit input values.
 * These can be cast to a const sample_lookup_t.
 */
extern const bits32 lookup4x1to32_identity[16];

#define sample_lookup_1_identity\
  ((const sample_lookup_t *)lookup4x1to32_identity)
extern const bits32 lookup4x1to32_inverted[16];

#define sample_lookup_1_inverted\
  ((const sample_lookup_t *)lookup4x1to32_inverted)

/*
 * Define procedures to unpack and shuffle image data samples.  The Unix C
 * compiler can't handle typedefs for procedure (as opposed to
 * pointer-to-procedure) types, so we have to do it with macros instead.
 *
 * The original data start at sample data_x relative to data.
 * bptr points to the buffer normally used to deliver the unpacked data.
 * The unpacked data are at sample *pdata_x relative to the return value.
 *
 * Note that this procedure may return either a pointer to the buffer, or
 * a pointer to the original data.
 */
#define sample_unpack_proc(proc)\
  const byte *proc(P7(byte *bptr, int *pdata_x, const byte *data, int data_x,\
		      uint dsize, const sample_lookup_t *ptab, int spread))
typedef sample_unpack_proc((*sample_unpack_proc_t));

/*
 * Declare the 1-for-1 unpacking procedure.
 */
sample_unpack_proc(sample_unpack_copy);
/*
 * Declare unpacking procedures for 1, 2, 4, and 8 bits per pixel,
 * with optional spreading of the result.
 */
sample_unpack_proc(sample_unpack_1);
sample_unpack_proc(sample_unpack_2);
sample_unpack_proc(sample_unpack_4);
sample_unpack_proc(sample_unpack_8);

#endif /* gxsample_INCLUDED */
