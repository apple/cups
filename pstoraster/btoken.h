/* Copyright (C) 1990 Aladdin Enterprises.  All rights reserved.
  
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

/* btoken.h */
/* Definitions for Level 2 binary tokens */

/* Binary token types */
typedef enum {
  bt_seq = 128,
    bt_seq_IEEE_msb = 128,	/* binary object sequence, */
				/* IEEE floats, big-endian */
    bt_seq_IEEE_lsb = 129,	/* ditto, little-endian */
    bt_seq_native_msb = 130,	/* ditto, native floats, big-endian */
    bt_seq_native_lsb = 131,	/* ditto, little-endian */
  bt_int32_msb = 132,
  bt_int32_lsb = 133,
  bt_int16_msb = 134,
  bt_int16_lsb = 135,
  bt_int8 = 136,
  bt_fixed = 137,
  bt_float_IEEE_msb = 138,
  bt_float_IEEE_lsb = 139,
  bt_float_native = 140,
  bt_boolean = 141,
  bt_string_256 = 142,
  bt_string_64k_msb = 143,
  bt_string_64k_lsb = 144,
  bt_litname_system = 145,
  bt_execname_system = 146,
  bt_litname_user = 147,
  bt_execname_user = 148,
  bt_num_array = 149
} bt_char;
#define bt_char_min 128
#define bt_char_max 159

/* Define the number of required initial bytes for binary tokens */
/* (including the token type byte). */
extern const byte bin_token_bytes[];	/* in iscan2.c */
#define bin_token_bytes_values\
  4, 4, 4, 4, 5, 5, 3, 3, 2, 2, 5, 5, 5,\
  2, 2, 3, 3, 2, 2, 2, 2, 4,\
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1	/* undefined */
#define binary_token_bytes(btchar)\
  (bin_token_bytes[(btchar) - bt_char_min])
