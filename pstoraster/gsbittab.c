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

/* gsbittab.c */
/* Tables for bit operations */
#include "stdpre.h"
#include "gsbittab.h"

/* ---------------- Byte processing tables ---------------- */

/* Reverse the bits of a byte. */
const byte byte_reverse_bits[256] =
{ 0, 128, 64, 192, 32, 160, 96, 224, 16, 144, 80, 208, 48, 176, 112, 240,
  8, 136, 72, 200, 40, 168, 104, 232, 24, 152, 88, 216, 56, 184, 120, 248,
  4, 132, 68, 196, 36, 164, 100, 228, 20, 148, 84, 212, 52, 180, 116, 244,
  12, 140, 76, 204, 44, 172, 108, 236, 28, 156, 92, 220, 60, 188, 124, 252,
  2, 130, 66, 194, 34, 162, 98, 226, 18, 146, 82, 210, 50, 178, 114, 242,
  10, 138, 74, 202, 42, 170, 106, 234, 26, 154, 90, 218, 58, 186, 122, 250,
  6, 134, 70, 198, 38, 166, 102, 230, 22, 150, 86, 214, 54, 182, 118, 246,
  14, 142, 78, 206, 46, 174, 110, 238, 30, 158, 94, 222, 62, 190, 126, 254,
  1, 129, 65, 193, 33, 161, 97, 225, 17, 145, 81, 209, 49, 177, 113, 241,
  9, 137, 73, 201, 41, 169, 105, 233, 25, 153, 89, 217, 57, 185, 121, 249,
  5, 133, 69, 197, 37, 165, 101, 229, 21, 149, 85, 213, 53, 181, 117, 245,
  13, 141, 77, 205, 45, 173, 109, 237, 29, 157, 93, 221, 61, 189, 125, 253,
  3, 131, 67, 195, 35, 163, 99, 227, 19, 147, 83, 211, 51, 179, 115, 243,
  11, 139, 75, 203, 43, 171, 107, 235, 27, 155, 91, 219, 59, 187, 123, 251,
  7, 135, 71, 199, 39, 167, 103, 231, 23, 151, 87, 215, 55, 183, 119, 247,
  15, 143, 79, 207, 47, 175, 111, 239, 31, 159, 95, 223, 63, 191, 127, 255
};

/* Define masks with a given number of trailing 1-bits. */
const byte byte_right_mask[9] =
{ 0, 1, 3, 7, 0xf, 0x1f, 0x3f, 0x7f, 0xff
};

/* ---------------- Scanning tables ---------------- */

/*
 * byte_bit_run_length_N[B], for 0 <= N <= 7, gives the length of the
 * run of 1-bits starting at bit N in a byte with value B,
 * numbering the bits in the byte as 01234567.  If the run includes
 * the low-order bit (i.e., might be continued into a following byte),
 * the run length is increased by 8.
 */

#define t8(n) n,n,n,n,n+1,n+1,n+2,n+11
#define r8(n) n,n,n,n,n,n,n,n
#define r16(n) r8(n),r8(n)
#define r32(n) r16(n),r16(n)
#define r64(n) r32(n),r32(n)
#define r128(n) r64(n),r64(n)
const byte byte_bit_run_length_0[256] =
{	r128(0), r64(1), r32(2), r16(3), r8(4), t8(5)
};
const byte far_data byte_bit_run_length_1[256] =
{	r64(0), r32(1), r16(2), r8(3), t8(4),
	r64(0), r32(1), r16(2), r8(3), t8(4)
};
const byte far_data byte_bit_run_length_2[256] =
{	r32(0), r16(1), r8(2), t8(3),
	r32(0), r16(1), r8(2), t8(3),
	r32(0), r16(1), r8(2), t8(3),
	r32(0), r16(1), r8(2), t8(3)
};
const byte far_data byte_bit_run_length_3[256] =
{	r16(0), r8(1), t8(2), r16(0), r8(1), t8(2),
	r16(0), r8(1), t8(2), r16(0), r8(1), t8(2),
	r16(0), r8(1), t8(2), r16(0), r8(1), t8(2),
	r16(0), r8(1), t8(2), r16(0), r8(1), t8(2)
};
const byte far_data byte_bit_run_length_4[256] =
{	r8(0), t8(1), r8(0), t8(1), r8(0), t8(1), r8(0), t8(1),
	r8(0), t8(1), r8(0), t8(1), r8(0), t8(1), r8(0), t8(1),
	r8(0), t8(1), r8(0), t8(1), r8(0), t8(1), r8(0), t8(1),
	r8(0), t8(1), r8(0), t8(1), r8(0), t8(1), r8(0), t8(1),
};
#define rr8(a,b,c,d,e,f,g,h)\
  a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h,\
  a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h,\
  a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h,\
  a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h,\
  a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h,\
  a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h,\
  a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h,\
  a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h, a,b,c,d,e,f,g,h
const byte far_data byte_bit_run_length_5[256] =
{	rr8(0,0,0,0,1,1,2,11)
};
const byte far_data byte_bit_run_length_6[256] =
{	rr8(0,0,1,10,0,0,1,10)
};
const byte far_data byte_bit_run_length_7[256] =
{	rr8(0,9,0,9,0,9,0,9)
};

/* Pointer tables indexed by bit number. */

const byte *byte_bit_run_length[8] =
{ byte_bit_run_length_0, byte_bit_run_length_1,
  byte_bit_run_length_2, byte_bit_run_length_3,
  byte_bit_run_length_4, byte_bit_run_length_5,
  byte_bit_run_length_6, byte_bit_run_length_7
};
const byte *byte_bit_run_length_neg[8] =
{ byte_bit_run_length_0, byte_bit_run_length_7,
  byte_bit_run_length_6, byte_bit_run_length_5,
  byte_bit_run_length_4, byte_bit_run_length_3,
  byte_bit_run_length_2, byte_bit_run_length_1
};

/* Some C compilers insist on having executable code in every file.... */
void
gsbittab_dummy(void)
{
}
