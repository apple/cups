/* Copyright (C) 1990, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gstype1.h,v 1.2 2000/03/08 23:14:48 mike Exp $ */
/* Client interface to Adobe Type 1 font routines */

#ifndef gstype1_INCLUDED
#  define gstype1_INCLUDED

/* ------ Normal client interface ------ */

#define crypt_charstring_seed 4330
typedef struct gs_type1_state_s gs_type1_state;

#ifndef gx_path_DEFINED
#  define gx_path_DEFINED
typedef struct gx_path_s gx_path;

#endif
#ifndef gs_show_enum_s_DEFINED
struct gs_show_enum_s;

#endif
#ifndef gs_font_type1_DEFINED
#  define gs_font_type1_DEFINED
typedef struct gs_font_type1_s gs_font_type1;

#endif
#ifndef gs_type1_data_s_DEFINED
struct gs_type1_data_s;

#endif
int gs_type1_interp_init(P7(gs_type1_state * pcis, gs_imager_state * pis,
			gx_path * ppath, const gs_log2_scale_point * pscale,
			    bool charpath_flag, int paint_type,
			    gs_font_type1 * pfont));
void gs_type1_set_lsb(P2(gs_type1_state * pcis, const gs_point * psbpt));
void gs_type1_set_width(P2(gs_type1_state * pcis, const gs_point * pwpt));

/* Backward compatibility */
#define gs_type1_init(pcis, penum, psbpt, charpath_flag, paint_type, pfont)\
  (gs_type1_interp_init(pcis, (gs_imager_state *)((penum)->pgs),\
			(penum)->pgs->path, &(penum)->log2_current_scale,\
			charpath_flag, paint_type, pfont) |\
   ((psbpt) == 0 ? 0 : (gs_type1_set_lsb(pcis, psbpt), 0)))
/*
 * Continue interpreting a Type 1 CharString.  If str != 0, it is taken as
 * the byte string to interpret.  Return 0 on successful completion, <0 on
 * error, or >0 when client intervention is required (or allowed).  The int*
 * argument is where the othersubr # is stored for callothersubr.
 */
#define type1_result_sbw 1	/* allow intervention after [h]sbw */
#define type1_result_callothersubr 2

int gs_type1_interpret(P3(gs_type1_state *, const gs_const_string *, int *));

/* ------ CharString number representation ------ */

/* Define the representation of integers used by both Type 1 and Type 2. */
typedef enum {

    /* Values from 32 to 246 represent small integers. */
    c_num1 = 32,
#define c_value_num1(ch) ((int)(byte)(ch) - 139)

    /* The next 4 values represent 2-byte positive integers. */
    c_pos2_0 = 247,
    c_pos2_1 = 248,
    c_pos2_2 = 249,
    c_pos2_3 = 250,
#define c_value_pos2(c1,c2)\
  (((int)(byte)((c1) - (int)c_pos2_0) << 8) + (int)(byte)(c2) + 108)

    /* The next 4 values represent 2-byte negative integers. */
    c_neg2_0 = 251,
    c_neg2_1 = 252,
    c_neg2_2 = 253,
    c_neg2_3 = 254
#define c_value_neg2(c1,c2)\
  -(((int)(byte)((c1) - (int)c_neg2_0) << 8) + (int)(byte)(c2) + 108)

} char_num_command;

/* ------ Type 1 & Type 2 CharString representation ------ */

/*
 * We define both the Type 1 and Type 2 operators here, because they
 * overlap so much.
 */
typedef enum {

    /* Commands with identical functions in Type 1 and Type 2 */
    /* charstrings. */

    c_undef0 = 0,
    c_undef2 = 2,
    c_callsubr = 10,
    c_return = 11,
    c_undoc15 = 15,		/* An obsolete and undocumented */
    /* command used in some very old */
    /* Adobe fonts. */
    c_undef17 = 17,

    /* Commands with similar but not identical functions */
    /* in Type 1 and Type 2 charstrings. */

    cx_hstem = 1,
    cx_vstem = 3,
    cx_vmoveto = 4,
    cx_rlineto = 5,
    cx_hlineto = 6,
    cx_vlineto = 7,
    cx_rrcurveto = 8,
    cx_escape = 12,		/* extends the command set */
    cx_endchar = 14,
    cx_rmoveto = 21,
    cx_hmoveto = 22,
    cx_vhcurveto = 30,
    cx_hvcurveto = 31,

    cx_num4 = 255,		/* 4-byte numbers */

    /* Commands recognized only in Type 1 charstrings. */

    c1_closepath = 9,
    c1_hsbw = 13,

    /* Commands not recognized in Type 1 charstrings. */

#define case_c1_undefs\
	case 16: case 18: case 19:\
	case 20: case 23: case 24:\
	case 25: case 26: case 27: case 28: case 29

    /* Commands only recognized in Type 2 charstrings. */

    c2_blend = 16,
    c2_hstemhm = 18,
    c2_hintmask = 19,
    c2_cntrmask = 20,
    c2_vstemhm = 23,
    c2_rcurveline = 24,
    c2_rlinecurve = 25,
    c2_vvcurveto = 26,
    c2_hhcurveto = 27,
    c2_shortint = 28,
    c2_callgsubr = 29

    /* Commands not recognized in Type 2 charstrings. */

#define case_c2_undefs\
	case 9: case 13

} char_command;

#define char1_command_names\
  0, "hstem", 0, "vstem", "vmoveto",\
  "rlineto", "hlineto", "vlineto", "rrcurveto", "closepath",\
  "callsubr", "return", "(escape)", "hsbw", "endchar",\
  "undoc15", 0, 0, 0, 0,\
  0, "rmoveto", "hmoveto", 0, 0,\
  0, 0, 0, 0, 0,\
  "vhcurveto", "hvcurveto"
#define char2_command_names\
  0, "hstem", 0, "vstem", "vmoveto",\
  "rlineto", "hlineto", "vlineto", "rrcurveto", 0,\
  "callsubr", "return", "(escape)", 0, "endchar",\
  "undoc15", "blend", 0, "hstemhm", "hintmask",\
  "cntrmask", "rmoveto", "hmoveto", "vstemhm", "rcurveline",\
  "rlinecurve", "vvcurveto", "hhcurveto", "shortint", "callgsubr",\
  "vhcurveto", "hvcurveto"

/*
 * Extended (escape) commands in Type 1 charstrings.
 */
typedef enum {
    ce1_dotsection = 0,
    ce1_vstem3 = 1,
    ce1_hstem3 = 2,
    ce1_seac = 6,
    ce1_sbw = 7,
    ce1_div = 12,
    ce1_undoc15 = 15,		/* An obsolete and undocumented */
    /* command used in some very old */
    /* Adobe fonts. */
    ce1_callothersubr = 16,
    ce1_pop = 17,
    ce1_setcurrentpoint = 33
} char1_extended_command;

#define char1_extended_command_count 34
#define char1_extended_command_names\
  "dotsection", "vstem3", "hstem3", 0, 0,\
  0, "seac", "sbw", 0, 0,\
  0, 0, "div", 0, 0,\
  "undoc15", "callothersubr", "pop", 0, 0,\
  0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0,\
  0, 0, 0, "setcurrentpoint"

/*
 * Extended (escape) commands in Type 2 charstrings.
 */
typedef enum {
    ce2_and = 3,
    ce2_or = 4,
    ce2_not = 5,
    ce2_store = 8,
    ce2_abs = 9,
    ce2_add = 10,
    ce2_sub = 11,
    ce2_div = 12,		/* same as ce1_div */
    ce2_load = 13,
    ce2_neg = 14,
    ce2_eq = 15,
    ce2_drop = 18,
    ce2_put = 20,
    ce2_get = 21,
    ce2_ifelse = 22,
    ce2_random = 23,
    ce2_mul = 24,
    ce2_sqrt = 26,
    ce2_dup = 27,
    ce2_exch = 28,
    ce2_index = 29,
    ce2_roll = 30,
    ce2_hflex = 34,
    ce2_flex = 35,
    ce2_hflex1 = 36,
    ce2_flex1 = 37
} char2_extended_command;

#define char2_extended_command_count 38
#define char2_extended_command_names\
  0, 0, 0, "and", "or",\
  "not", 0, 0, "store", "abs",\
  "add", "sub", "div", "load", "neg",\
  "eq", 0, 0, "drop", 0,\
  "put", "get", "ifelse", "random", "mul",\
  0, "sqrt", "dup", "exch", "index",\
  "roll", 0, 0, 0, "hflex",\
  "flex", "hflex1", "flex1"

#endif /* gstype1_INCLUDED */
