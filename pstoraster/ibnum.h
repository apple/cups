/* Copyright (C) 1990, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* ibnum.h */
/* Interface to Level 2 number readers */
/* Requires stream.h */

/* Define the byte that begins an encoded number string. */
/* (This is the same as the value of bt_num_array in btoken.h.) */
#define bt_num_array_value 149

/* Homogenous number array formats. */
/* The default for numbers is big-endian. */
#define num_int32 0			/* [0..31] */
#define num_int16 32			/* [32..47] */
#define num_float 48
#define num_float_IEEE num_float
#define num_float_native (num_float + 1)
#define num_msb 0
#define num_lsb 128
#define num_is_lsb(format) ((format) >= num_lsb)
#define num_is_valid(format) (((format) & 127) <= 49)
/* Special "format" for reading from an array. */
/* num_msb/lsb is not used in this case. */
#define num_array 256
/* Define the number of bytes for a given format of encoded number. */
extern const byte enc_num_bytes[]; /* in ibnum.c */
#define enc_num_bytes_values\
  4, 4, 2, 4, 0, 0, 0, 0,\
  4, 4, 2, 4, 0, 0, 0, 0,\
  sizeof(ref)
#define encoded_number_bytes(format)\
  (enc_num_bytes[(format) >> 4])

/* Read from an array or encoded number string. */
int	num_array_format(P1(const ref *));	/* returns format or error */
uint	num_array_size(P2(const ref *, int));
int	num_array_get(P4(const ref *, int, uint, ref *));

/* Decode a number from a string with appropriate byte swapping. */
int	sdecode_number(P3(const byte *, int, ref *));
short	sdecodeshort(P2(const byte *, int));
long	sdecodelong(P2(const byte *, int));
float	sdecodefloat(P2(const byte *, int));
