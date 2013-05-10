/* Copyright (C) 1990, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gxtype1.h */
/* Private Adobe Type 1 font definitions for Ghostscript library */
#include "gscrypt1.h"
#include "gstype1.h"

/* This file defines the structures for the state of a Type 1 interpreter. */

/*
 * Because of oversampling, one pixel in the Type 1 interpreter may
 * correspond to several device pixels.  This is also true of the hint data,
 * since the CTM reflects the transformation to the oversampled space.
 * To help keep the font level hints separated from the character level hints,
 * we store the scaling factor separately with each set of hints.
 */
typedef struct pixel_scale_s {
	fixed unit;		/* # of pixels per device pixel */
	fixed half;		/* unit / 2 */
	int log2_unit;		/* log2(unit / fixed_1) */
} pixel_scale;
typedef struct point_scale_s {
	pixel_scale x, y;
} point_scale;
#define set_pixel_scale(pps, log2)\
  (pps)->unit = ((pps)->half = fixed_half << ((pps)->log2_unit = log2)) << 1
#define scaled_rounded(v, pps)\
  (((v) + (pps)->half) & -(pps)->unit)

/* ------ Font level hints ------ */

/* Define the standard stem width tables. */
/* Each table is sorted, since the StemSnap arrays are sorted. */
#define max_snaps (1 + max_StemSnap)
typedef struct {
	int count;
	fixed data[max_snaps];
} stem_snap_table;

/* Define the alignment zone structure. */
/* These are in device coordinates also. */
#define max_a_zones (max_BlueValues + max_OtherBlues)
typedef struct {
	int is_top_zone;
	fixed v0, v1;			/* range for testing */
	fixed flat;			/* flat position */
} alignment_zone;

/* Define the structure for hints that depend only on the font and CTM, */
/* not on the individual character.  Eventually these should be cached */
/* with the font/matrix pair. */
typedef struct font_hints_s {
	bool axes_swapped;		/* true if x & y axes interchanged */
					/* (only set if using hints) */
	bool x_inverted, y_inverted;	/* true if axis is inverted */
	bool use_x_hints;		/* true if we should use hints */
					/* for char space x coords (vstem) */
	bool use_y_hints;		/* true if we should use hints */
					/* for char space y coords (hstem) */
	point_scale scale;		/* oversampling scale */
	stem_snap_table snap_h;		/* StdHW, StemSnapH */
	stem_snap_table snap_v;		/* StdVW, StemSnapV */
	fixed blue_fuzz, blue_shift;	/* alignment zone parameters */
					/* in device pixels */
	bool suppress_overshoot;	/* (computed from BlueScale) */
	int a_zone_count;		/* # of alignment zones */
	alignment_zone a_zones[max_a_zones];	/* the alignment zones */
} font_hints;

/* ------ Character level hints ------ */

/* Define the stem hint tables. */
/* Each stem hint table is kept sorted. */
/* Stem hints are in device coordinates. */
/* We must set max_stems large enough to handle complex Asian characters. */
#define max_stems 16			/* arbitrary */
typedef struct {
	fixed v0, v1;			/* coordinates (widened a little) */
	fixed dv0, dv1;			/* adjustment values */
} stem_hint;
typedef struct {
	int count;
	int current;			/* cache cursor for search */
	stem_hint data[max_stems];
} stem_hint_table;

/* ------ Interpreter state ------ */

/* Define the control state of the interpreter. */
/* This is what must be saved and restored */
/* when calling a CharString subroutine. */
typedef struct {
	const byte *ip;
	crypt_state dstate;
	gs_const_string char_string;	/* original CharString or Subr, */
					/* for GC */
} ip_state;

#ifndef gx_path_DEFINED
#  define gx_path_DEFINED
typedef struct gx_path_s gx_path;
#endif

#ifndef segment_DEFINED
#  define segment_DEFINED
typedef struct segment_s segment;
#endif

/* This is the full state of the Type 1 interpreter. */
#define ostack_size 24			/* per documentation */
#define ipstack_size 10			/* per documentation */
struct gs_type1_state_s {
		/* The following are set at initialization */
	gs_font_type1 *pfont;		/* font-specific data */
	gs_imager_state *pis;		/* imager state */
	gx_path *path;			/* path for appending */
	bool charpath_flag;		/* false if show, true if charpath */
	int paint_type;			/* 0/3 for fill, 1/2 for stroke */
	fixed_coeff fc;			/* cached fixed coefficients */
	float flatness;			/* flatness for character curves */
	point_scale scale;		/* oversampling scale */
	font_hints fh;			/* font-level hints */
	gs_fixed_point origin;		/* character origin */
		/* The following are updated dynamically */
	fixed ostack[ostack_size];	/* the Type 1 operand stack */
	int os_count;			/* # of occupied stack entries */
	ip_state ipstack[ipstack_size+1];	/* control stack */
	int ips_count;			/* # of occupied entries */
	int init_done;			/* -1 if not done & not needed, */
					/* 0 if not done & needed, 1 if done */
	bool sb_set;			/* true if lsb is preset */
	bool width_set;			/* true if width is set (for */
					/* seac components) */
	gs_fixed_point lsb;		/* left side bearing (char coords) */
	gs_fixed_point width;		/* character width (char coords) */
	int seac_base;			/* base character code for seac, */
					/* or -1 */
	gs_fixed_point adxy;		/* seac accent displacement, */
					/* needed to adjust currentpoint */
	fixed asb_diff;			/* seac asb - accented char lsb.x, */
					/* needed to adjust Flex endpoint */
	gs_fixed_point position;	/* save unadjusted position */
					/* when returning temporarily */
					/* to caller */
	int flex_path_was_open;		/* record whether path was open */
					/* at start of Flex section */
#define flex_max 8
	gs_fixed_point flex_points[flex_max];	/* points for Flex */
	int flex_count;
	int ignore_pops;		/* # of pops to ignore (after */
					/* a known othersubr call) */
		/* The following are set dynamically. */
#define dotsection_in 0
#define dotsection_out (-1)
	int dotsection_flag;		/* 0 if inside dotsection, */
					/* -1 if outside */
	bool vstem3_set;		/* true if vstem3 seen */
	gs_fixed_point vs_offset;	/* device space offset for centering */
					/* middle stem of vstem3 */
	int hints_initial;		/* hints applied to initial point */
					/* of subpath */
	gs_fixed_point unmoved_start;	/* original initial point of subpath */
	segment *hint_next;		/* last segment where hints have */
					/* been applied, 0 means none of */
					/* current subpath has been hinted */
	int hints_pending;		/* hints applied to end of hint_next */
	gs_fixed_point unmoved_end;	/* original hint_next->pt */
	stem_hint_table hstem_hints;	/* horizontal stem hints */
	stem_hint_table vstem_hints;	/* vertical stem hints */
};
extern_st(st_gs_type1_state);
#define public_st_gs_type1_state() /* in gstype1.c */\
  gs_public_st_composite(st_gs_type1_state, gs_type1_state, "gs_type1_state",\
    gs_type1_state_enum_ptrs, gs_type1_state_reloc_ptrs)


/* ------ Interface between main Type 1 interpreter and hint routines ------ */

/* Font level hints */
void	reset_font_hints(P2(font_hints *, const gs_log2_scale_point *));
void	compute_font_hints(P4(font_hints *, const gs_matrix_fixed *,
			      const gs_log2_scale_point *,
			      const gs_type1_data *));
/* Character level hints */
void	reset_stem_hints(P1(gs_type1_state *)),
	update_stem_hints(P1(gs_type1_state *)),
	type1_apply_path_hints(P3(gs_type1_state *, bool, gx_path *)),
#define apply_path_hints(pcis, closing)\
  type1_apply_path_hints(pcis, closing, pcis->path)
	type1_do_hstem(P4(gs_type1_state *, fixed, fixed,
			  const gs_matrix_fixed *)),
#define type1_hstem(pcis, y, dy)\
  type1_do_hstem(pcis, y, dy, &(pcis)->pis->ctm)
	type1_do_vstem(P4(gs_type1_state *, fixed, fixed,
			  const gs_matrix_fixed *)),
#define type1_vstem(pcis, x, dx)\
  type1_do_vstem(pcis, x, dx, &(pcis)->pis->ctm)
	type1_do_center_vstem(P4(gs_type1_state *, fixed, fixed,
				 const gs_matrix_fixed *));
#define center_vstem(pcis, x0, dx)\
  type1_do_center_vstem(pcis, x0, dx, &(pcis)->pis->ctm)
