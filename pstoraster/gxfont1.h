/* Copyright (C) 1994, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxfont1.h,v 1.2 2000/03/08 23:14:59 mike Exp $ */
/* Type 1 font data definition (including Type 2 charstrings) */

#ifndef gxfont1_INCLUDED
#  define gxfont1_INCLUDED


/*
 * This is the type-specific information for an Adobe Type 1 font.
 * It also includes the information for Type 2 charstrings, because
 * there isn't very much of it and it's less trouble to include here.
 */

#ifndef gs_font_type1_DEFINED
#  define gs_font_type1_DEFINED
typedef struct gs_font_type1_s gs_font_type1;

#endif

/*
 * The zone_table values should be ints, according to the Adobe
 * specification, but some fonts have arbitrary floats here.
 */
#define zone_table(size)\
	struct {\
		int count;\
		float values[(size)*2];\
	}
#define float_array(size)\
	struct {\
		int count;\
		float values[size];\
	}
#define stem_table(size)\
	float_array(size)

typedef struct gs_type1_data_s gs_type1_data;

typedef struct gs_type1_data_procs_s {

    /* Get the data for any glyph. */

    int (*glyph_data) (P3(gs_font_type1 * pfont, gs_glyph glyph,
			  gs_const_string * pgdata));

    /* Get the data for a Subr. */

    int (*subr_data) (P4(gs_font_type1 * pfont, int subr_num, bool global,
			 gs_const_string * psdata));

    /* Get the data for a seac character. */

    int (*seac_data) (P3(gs_font_type1 * pfont, int ccode,
			 gs_const_string * pcdata));

    /*
     * Get the next glyph.  index = 0 means return the first one; a
     * returned index of 0 means the enumeration is finished.
     */

    int (*next_glyph) (P3(gs_font_type1 * pfont, int *pindex,
			  gs_glyph * pglyph));

    /* Push (a) value(s) onto the client ('PostScript') stack. */

    int (*push) (P3(gs_font_type1 * pfont, const fixed * values, int count));

    /* Pop a value from the client stack. */

    int (*pop) (P2(gs_font_type1 * pfont, fixed * value));

} gs_type1_data_procs_t;

/*
 * The garbage collector really doesn't want the client data pointer
 * from a gs_type1_state to point to the gs_type1_data in the middle of
 * a gs_font_type1, so we make the client data pointer (which is passed
 * to the callback procedures) point to the gs_font_type1 itself.
 */
struct gs_type1_data_s {
    /*int PaintType; *//* in gs_font_common */
    int CharstringType;		/* 1 or 2 */
    const gs_type1_data_procs_t *procs;
    void *proc_data;		/* data for procs */
    int lenIV;			/* -1 means no encryption */
    /* (undocumented feature!) */
    uint subroutineNumberBias;	/* added to operand of callsubr */
    /* (undocumented feature!) */
    /* Type 2 charstring additions */
    uint gsubrNumberBias;	/* added to operand of callgsubr */
    long initialRandomSeed;
    fixed defaultWidthX;
    fixed nominalWidthX;
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
         float_array(max_WeightVector) WeightVector;
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

#endif /* gxfont1_INCLUDED */
