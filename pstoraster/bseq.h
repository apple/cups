/* Copyright (C) 1990, 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* bseq.h */
/* Definitions for Level 2 binary object sequences */

/* Binary object sequence element types */
typedef enum {
  bs_null = 0,
  bs_integer = 1,
  bs_real = 2,
  bs_name = 3,
  bs_boolean = 4,
  bs_string = 5,
  bs_eval_name = 6,
  bs_array = 9,
  bs_mark = 10,
	/*
	 * We extend the PostScript language definition by allowing
	 * dictionaries in binary object sequences.  The data for
	 * a dictionary is like that for an array, with the following
	 * changes:
	 *	- If the size is an even number, the value is the index of
	 * the first of a series of alternating keys and values.
	 *	- If the size is 1, the value is the index of another
	 * object (which must also be a dictionary, and must not have
	 * size = 1); this object represents the same object as that one.
	 */
  bs_dictionary = 15
} bin_seq_type;
#define bs_executable 128

/* Definition of an object in a binary object sequence. */
typedef struct {
  byte tx;			/* type and executable flag */
  byte unused;
  union {
    bits16 w;
    byte b[2];
  } size;
  union {
    bits32 w;
    float f;
    byte b[4];
  } value;
} bin_seq_obj;
