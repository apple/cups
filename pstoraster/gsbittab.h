/* Copyright (C) 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsbittab.h,v 1.2 2000/03/08 23:14:34 mike Exp $ */
/* Interface to tables for bit operations */

#ifndef gsbittab_INCLUDED
#  define gsbittab_INCLUDED

/*
 * Generate tables for transforming 2, 4, 6, or 8 bits.
 */
#define btab2_(v0,v2,v1)\
  v0,v1+v0,v2+v0,v2+v1+v0
#define bit_table_2(v0,v2,v1) btab2_(v0,v2,v1)
#define btab4_(v0,v8,v4,v2,v1)\
  btab2_(v0,v2,v1), btab2_(v4+v0,v2,v1),\
  btab2_(v8+v0,v2,v1), btab2_(v8+v4+v0,v2,v1)
#define bit_table_4(v0,v8,v4,v2,v1) btab4_(v0,v8,v4,v2,v1)
#define btab6_(v0,v20,v10,v8,v4,v2,v1)\
  btab4_(v0,v8,v4,v2,v1), btab4_(v10+v0,v8,v4,v2,v1),\
  btab4_(v20+v0,v8,v4,v2,v1), btab4_(v20+v10+v0,v8,v4,v2,v1)
#define bit_table_6(v0,v20,v10,v8,v4,v2,v1) btab6_(v0,v20,v10,v8,v4,v2,v1)
#define bit_table_8(v0,v80,v40,v20,v10,v8,v4,v2,v1)\
  btab6_(v0,v20,v10,v8,v4,v2,v1), btab6_(v40+v0,v20,v10,v8,v4,v2,v1),\
  btab6_(v80+v0,v20,v10,v8,v4,v2,v1), btab6_(v80+v40+v0,v20,v10,v8,v4,v2,v1)

/*
 * byte_reverse_bits[B] = the byte B with the order of bits reversed.
 */
extern const byte byte_reverse_bits[256];

/*
 * byte_right_mask[N] = a byte with N trailing 1s, 0 <= N <= 8.
 */
extern const byte byte_right_mask[9];

/*
 * byte_count_bits[B] = the number of 1-bits in a byte with value B.
 */
extern const byte byte_count_bits[256];

/*
 * byte_bit_run_length_N[B], for 0 <= N <= 7, gives the length of the
 * run of 1-bits starting at bit N in a byte with value B,
 * numbering the bits in the byte as 01234567.  If the run includes
 * the low-order bit (i.e., might be continued into a following byte),
 * the run length is increased by 8.
 */
extern const byte
      byte_bit_run_length_0[256], byte_bit_run_length_1[256], byte_bit_run_length_2[256],
      byte_bit_run_length_3[256], byte_bit_run_length_4[256], byte_bit_run_length_5[256],
      byte_bit_run_length_6[256], byte_bit_run_length_7[256];

/*
 * byte_bit_run_length[N] points to byte_bit_run_length_N.
 * byte_bit_run_length_neg[N] = byte_bit_run_length[-N & 7].
 */
extern const byte *const byte_bit_run_length[8];
extern const byte *const byte_bit_run_length_neg[8];

#endif /* gsbittab_INCLUDED */
