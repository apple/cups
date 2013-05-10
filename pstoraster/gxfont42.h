/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gxfont42.h */
/* Type 42 font data definition */

/* This is the type-specific information for a Type 42 (TrueType) font. */

typedef struct gs_type42_data_s gs_type42_data;
#ifndef gs_font_type42_DEFINED
#  define gs_font_type42_DEFINED
typedef struct gs_font_type42_s gs_font_type42;
#endif
struct gs_type42_data_s {
		/* The following are set by the client. */
	int (*string_proc)(P4(gs_font_type42 *, ulong, uint, const byte **));
	void *proc_data;		/* data for string_proc */
		/* The following are cached values. */
	ulong glyf;			/* offset to glyf table */
	uint unitsPerEm;		/* from head */
	uint indexToLocFormat;		/* from head */
	uint numLongMetrics;		/* from hhea */
	ulong hmtx;			/* offset to hmtx table */
	uint hmtx_length;		/* length of hmtx table */
	ulong loca;			/* offset to loca table */
};
struct gs_font_type42_s {
	gs_font_base_common;
	gs_type42_data data;
};
extern_st(st_gs_font_type42);
#define public_st_gs_font_type42()	/* in gstype42.c */\
  gs_public_st_suffix_add1_final(st_gs_font_type42, gs_font_type42,\
    "gs_font_type42", font_type42_enum_ptrs, font_type42_reloc_ptrs,\
    gs_font_finalize, st_gs_font_base, data.proc_data)

/* Because a Type 42 font contains so many cached values, */
/* we provide a procedure to initialize them from the font data. */
int gs_type42_font_init(P1(gs_font_type42 *));
