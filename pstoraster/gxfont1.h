/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* gxfont1.h */
/* Type 1 font data definition */

/* This is the type-specific information for a type 1 (encrypted) gs_font. */
/* The zone_table values should be ints, according to the Adobe */
/* specification, but some fonts have arbitrary floats here. */
#define zone_table(size)\
	struct {\
		int count;\
		float values[(size)*2];\
	}
#define stem_table(size)\
	struct {\
		int count;\
		float values[size];\
	}
typedef struct gs_type1_data_s gs_type1_data;
/* The garbage collector really doesn't want the client data pointer */
/* from a gs_type1_state to point to the gs_type1_data in the middle of */
/* a gs_font_type1, so we make the client data pointer (which is passed */
/* to the callback procedures) point to the gs_font_type1 itself. */
typedef struct gs_font_type1_s gs_font_type1;
struct gs_type1_data_s {
	/*int PaintType;*/		/* in gs_font_common */
	int (*subr_proc)(P3(gs_font_type1 *, int, gs_const_string *));
	int (*seac_proc)(P3(gs_font_type1 *, int, gs_const_string *));
	int (*push_proc)(P3(gs_font_type1 *, const fixed *, int));
	int (*pop_proc)(P2(gs_font_type1 *, fixed *));
	void *proc_data;		/* data for subr_proc & seac_proc */
	int lenIV;
	/* For a description of the following hint information, */
	/* see chapter 5 of the "Adobe Type 1 Font Format" book. */
	int BlueFuzz;
	float BlueScale;
	float BlueShift;
#define max_BlueValues 7
	zone_table(max_BlueValues) BlueValues;
	float ExpansionFactor;
	bool ForceBold;
#define max_FamilyBlues 7
	zone_table(max_FamilyBlues) FamilyBlues;
#define max_FamilyOtherBlues 5
	zone_table(max_FamilyOtherBlues) FamilyOtherBlues;
	int LanguageGroup;
#define max_OtherBlues 5
	zone_table(max_OtherBlues) OtherBlues;
	bool RndStemUp;
	stem_table(1) StdHW;
	stem_table(1) StdVW;
#define max_StemSnap 12
	stem_table(max_StemSnap) StemSnapH;
	stem_table(max_StemSnap) StemSnapV;
		/* Additional information for Multiple Master fonts */
#define max_WeightVector 16
	float WeightVector[max_WeightVector];
};
#define gs_type1_data_s_DEFINED

struct gs_font_type1_s {
	gs_font_base_common;
	gs_type1_data data;
};
extern_st(st_gs_font_type1);
#define public_st_gs_font_type1()	/* in gstype1.c */\
  gs_public_st_suffix_add1_final(st_gs_font_type1, gs_font_type1,\
    "gs_font_type1", font_type1_enum_ptrs, font_type1_reloc_ptrs,\
    gs_font_finalize, st_gs_font_base, data.proc_data)
