/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gsbittab.h */
/* Interface to tables for bit operations */

/*
 * byte_reverse_bits[B] = the byte B with the order of bits reversed.
 */
extern const byte byte_reverse_bits[256];

/*
 * byte_right_mask[N] = a byte with N trailing 1s, 0 <= N <= 8.
 */
extern const byte byte_right_mask[9];

/*
 * byte_bit_run_length_N[B], for 0 <= N <= 7, gives the length of the
 * run of 1-bits starting at bit N in a byte with value B,
 * numbering the bits in the byte as 01234567.  If the run includes
 * the low-order bit (i.e., might be continued into a following byte),
 * the run length is increased by 8.
 */
extern const byte
  byte_bit_run_length_0[256], byte_bit_run_length_1[256],
  byte_bit_run_length_2[256], byte_bit_run_length_3[256],
  byte_bit_run_length_4[256], byte_bit_run_length_5[256],
  byte_bit_run_length_6[256], byte_bit_run_length_7[256];

/*
 * byte_bit_run_length[N] points to byte_bit_run_length_N.
 * byte_bit_run_length_neg[N] = byte_bit_run_length[-N & 7].
 */
extern const byte *byte_bit_run_length[8];
extern const byte *byte_bit_run_length_neg[8];
