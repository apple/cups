/* Copyright (C) 1992, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* ccfont.h */
/* Header for fonts compiled into C. */

#ifndef ccfont_INCLUDED
#  define ccfont_INCLUDED

/* Include all the things a compiled font needs. */
#include "std.h"
#include "gsmemory.h"
#include "iref.h"
#include "ivmspace.h"		/* for avm_foreign */
#include "store.h"

/* Define type-specific refs for initializing arrays. */
#define ref_(t) struct { struct tas_s tas; t value; }
#define boolean_v(b) { {t_boolean<<r_type_shift}, (ushort)(b) }
#define integer_v(i) { {t_integer<<r_type_shift}, (long)(i) }
#define null_v() { {t_null<<r_type_shift} }
#define real_v(v) { {t_real<<r_type_shift}, (float)(v) }

/* Define other initialization structures. */
typedef struct { byte encx, charx; } charindex;
/*
 * We represent mostly-string arrays by byte strings.  Each element
 * starts with length bytes.  If the first length byte is not 255,
 * it and the following byte define a big-endian length of a string or name.
 * If the first two bytes are (255,255), this element is null.
 * Otherwise, the initial 255 is followed by a 2-byte big-endian length
 * of a string that must be scanned as a token.
 */
typedef const char *cfont_string_array;

/* Support routines in iccfont.c */
typedef struct {
	const charindex *enc_keys;	/* keys from encoding vectors */
	uint num_enc_keys;
	uint num_str_keys;
	uint extra_slots;		/* (need extra for fonts) */
	uint dict_attrs;		/* protection for dictionary */
	uint value_attrs;		/* protection for values */
					/* (only used for string dicts) */
} cfont_dict_keys;
/* We pass a procedure vector to the font initialization routine */
/* to avoid having externs, which compromise sharability. */
/* On MS-DOS, each compiled font has its own data segment, */
/* so all of these procedures must be declared 'huge' for Borland C. */
typedef struct cfont_procs_s {
	int huge (*ref_dict_create)(P4(ref *, const cfont_dict_keys *,
				       cfont_string_array, const ref *));
	int huge (*string_dict_create)(P4(ref *, const cfont_dict_keys *,
					  cfont_string_array,
					  cfont_string_array));
	int huge (*num_dict_create)(P5(ref *, const cfont_dict_keys *,
				       cfont_string_array, const ref *,
				       const char *));
	int huge (*name_array_create)(P3(ref *, cfont_string_array, int));
	int huge (*string_array_create)(P4(ref *, cfont_string_array,
					   int /*size*/,
					   uint	/*protection*/));
	int huge (*name_create)(P2(ref *, const char *));
	int huge (*ref_from_string)(P3(ref *, const char *, uint));
} cfont_procs;

/*
 * In order to make it possible for third parties to compile fonts (into
 * a shared library, on systems that support such things), we define
 * a tiny procedural interface for getting access to the compiled font table.
 */
typedef huge int ccfont_fproc(P2(const cfont_procs *, ref *));
/* There should be some consts in the *** below, but a number of */
/* C compilers don't handle const properly in such situations. */
extern int ccfont_fprocs(P2(int *, ccfont_fproc ***));
#define ccfont_version 17       /* for checking against libraries */

#endif					/* ccfont_INCLUDED */
