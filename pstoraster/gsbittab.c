/* Copyright (C) 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsbittab.c,v 1.2 2000/03/08 23:14:34 mike Exp $ */
/* Tables for bit operations */
#include "stdpre.h"
#include "gsbittab.h"

/* ---------------- Byte processing tables ---------------- */

/*
 * byte_reverse_bits[B] = the byte B with the order of bits reversed.
 */
const byte byte_reverse_bits[256] =
{
    bit_table_8(0, 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80)
};

/*
 * byte_right_mask[N] = a byte with N trailing 1s, 0 <= N <= 8.
 */
const byte byte_right_mask[9] =
{
    0, 1, 3, 7, 0xf, 0x1f, 0x3f, 0x7f, 0xff
};

/*
 * byte_count_bits[B] = the number of 1-bits in a byte with value B.
 */
const byte byte_count_bits[256] =
{
    bit_table_8(0, 1, 1, 1, 1, 1, 1, 1, 1)
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
{
    r128(0), r64(1), r32(2), r16(3), r8(4), t8(5)
};
const byte byte_bit_run_length_1[256] =
{
    r64(0), r32(1), r16(2), r8(3), t8(4),
    r64(0), r32(1), r16(2), r8(3), t8(4)
};
const byte byte_bit_run_length_2[256] =
{
    r32(0), r16(1), r8(2), t8(3),
    r32(0), r16(1), r8(2), t8(3),
    r32(0), r16(1), r8(2), t8(3),
    r32(0), r16(1), r8(2), t8(3)
};
const byte byte_bit_run_length_3[256] =
{
    r16(0), r8(1), t8(2), r16(0), r8(1), t8(2),
    r16(0), r8(1), t8(2), r16(0), r8(1), t8(2),
    r16(0), r8(1), t8(2), r16(0), r8(1), t8(2),
    r16(0), r8(1), t8(2), r16(0), r8(1), t8(2)
};
const byte byte_bit_run_length_4[256] =
{
    r8(0), t8(1), r8(0), t8(1), r8(0), t8(1), r8(0), t8(1),
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
const byte byte_bit_run_length_5[256] =
{
    rr8(0, 0, 0, 0, 1, 1, 2, 11)
};
const byte byte_bit_run_length_6[256] =
{
    rr8(0, 0, 1, 10, 0, 0, 1, 10)
};
const byte byte_bit_run_length_7[256] =
{
    rr8(0, 9, 0, 9, 0, 9, 0, 9)
};

/* Pointer tables indexed by bit number. */

const byte *const byte_bit_run_length[8] =
{byte_bit_run_length_0, byte_bit_run_length_1,
 byte_bit_run_length_2, byte_bit_run_length_3,
 byte_bit_run_length_4, byte_bit_run_length_5,
 byte_bit_run_length_6, byte_bit_run_length_7
};
const byte *const byte_bit_run_length_neg[8] =
{byte_bit_run_length_0, byte_bit_run_length_7,
 byte_bit_run_length_6, byte_bit_run_length_5,
 byte_bit_run_length_4, byte_bit_run_length_3,
 byte_bit_run_length_2, byte_bit_run_length_1
};

/* Some C compilers insist on having executable code in every file.... */
void
gsbittab_dummy(void)
{
}
