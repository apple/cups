/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gstype42.c,v 1.4 2000/04/20 19:56:42 mike Exp $ */
/* Type 42 (TrueType) font library routines */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsccode.h"
#include "gsmatrix.h"
#include "gxfixed.h"		/* for gxpath.h */
#include "gxpath.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "gxistate.h"

/*
 * This Type 42 / TrueType rasterizer is about as primitive as it can be
 * and still produce useful output.  Here are some things it doesn't handle:
 *      - left side bearings;
 * and, of course, instructions (hints).
 */

/* Structure descriptor */
public_st_gs_font_type42();

/* Set up a pointer to a substring of the font data. */
/* Free variables: pfont, string_proc. */
#define access(base, length, vptr)\
  BEGIN\
    code = (*string_proc)(pfont, (ulong)(base), length, &vptr);\
    if ( code < 0 ) return code;\
  END

/* Get 2- or 4-byte quantities from a table. */
#define u8(p) ((uint)((p)[0]))
#define s8(p) (int)((u8(p) ^ 0x80) - 0x80)
#define u16(p) (((uint)((p)[0]) << 8) + (p)[1])
#define s16(p) (int)((u16(p) ^ 0x8000) - 0x8000)
#define u32(p) (((ulong)u16(p) << 16) + u16((p) + 2))
#define s32(p) (long)((u32(p) ^ 0x80000000) - 0x80000000)

/* Define the default implementation for getting the outline data for */
/* a glyph, using indexToLocFormat and the loca and glyf tables. */
/* Set pglyph->data = 0 if the glyph is empty. */
private int
default_get_outline(gs_font_type42 * pfont, uint glyph_index,
		    gs_const_string * pglyph)
{
    int (*string_proc) (P4(gs_font_type42 *, ulong, uint, const byte **)) =
    pfont->data.string_proc;
    const byte *ploca;
    ulong glyph_start;
    uint glyph_length;
    int code;

    /*
     * We can't assume that consecutive loca entries are stored
     * contiguously in memory: we have to access each entry
     * individually.
     */
    if (pfont->data.indexToLocFormat) {
	access(pfont->data.loca + glyph_index * 4, 4, ploca);
	glyph_start = u32(ploca);
	access(pfont->data.loca + glyph_index * 4 + 4, 4, ploca);
	glyph_length = u32(ploca) - glyph_start;
    } else {
	access(pfont->data.loca + glyph_index * 2, 2, ploca);
	glyph_start = (ulong) u16(ploca) << 1;
	access(pfont->data.loca + glyph_index * 2 + 2, 2, ploca);
	glyph_length = ((ulong) u16(ploca) << 1) - glyph_start;
    }
    pglyph->size = glyph_length;
    if (glyph_length == 0)
	pglyph->data = 0;
    else
	access(pfont->data.glyf + glyph_start, glyph_length, pglyph->data);
    return 0;
}

/* Initialize the cached values in a Type 42 font. */
/* Note that this initializes get_outline as well. */
int
gs_type42_font_init(gs_font_type42 * pfont)
{
    int (*string_proc) (P4(gs_font_type42 *, ulong, uint, const byte **)) =
    pfont->data.string_proc;
    const byte *OffsetTable;
    uint numTables;
    const byte *TableDirectory;
    uint i;
    int code;
    byte head_box[8];

    access(0, 12, OffsetTable);
    {
	static const byte version1_0[4] = {0, 1, 0, 0};
	static const byte * const version_true = (const byte *)"true";

	if (memcmp(OffsetTable, version1_0, 4) &&
	    memcmp(OffsetTable, version_true, 4))
	    return_error(gs_error_invalidfont);
    }
    numTables = u16(OffsetTable + 4);
    access(12, numTables * 16, TableDirectory);
    /* Clear optional entries. */
    pfont->data.numLongMetrics = 0;
    for (i = 0; i < numTables; ++i) {
	const byte *tab = TableDirectory + i * 16;
	ulong offset = u32(tab + 8);

	if (!memcmp(tab, "glyf", 4))
	    pfont->data.glyf = offset;
	else if (!memcmp(tab, "head", 4)) {
	    const byte *head;

	    access(offset, 54, head);
	    pfont->data.unitsPerEm = u16(head + 18);
	    memcpy(head_box, head + 36, 8);
	    pfont->data.indexToLocFormat = u16(head + 50);
	} else if (!memcmp(tab, "hhea", 4)) {
	    const byte *hhea;

	    access(offset, 36, hhea);
	    pfont->data.numLongMetrics = u16(hhea + 34);
	} else if (!memcmp(tab, "hmtx", 4))
	    pfont->data.hmtx = offset,
		pfont->data.hmtx_length = (uint) u32(tab + 12);
	else if (!memcmp(tab, "loca", 4))
	    pfont->data.loca = offset;
    }
    /*
     * If the font doesn't have a valid FontBBox, compute one from the
     * 'head' information.  Since the Adobe PostScript driver sometimes
     * outputs garbage FontBBox values, we use a "reasonableness" check
     * here.
     */
    if (pfont->FontBBox.p.x >= pfont->FontBBox.q.x ||
	pfont->FontBBox.p.y >= pfont->FontBBox.q.y ||
	pfont->FontBBox.p.x < -0.5 || pfont->FontBBox.p.x > 0.5 ||
	pfont->FontBBox.p.y < -0.5 || pfont->FontBBox.p.y > 0.5
	) {
	float upem = pfont->data.unitsPerEm;

	pfont->FontBBox.p.x = s16(head_box) / upem;
	pfont->FontBBox.p.y = s16(head_box + 2) / upem;
	pfont->FontBBox.q.x = s16(head_box + 4) / upem;
	pfont->FontBBox.q.y = s16(head_box + 6) / upem;
    }
    pfont->data.get_outline = default_get_outline;
    return 0;
}

/* Get the metrics of a glyph. */
int
gs_type42_get_metrics(gs_font_type42 * pfont, uint glyph_index,
		      float psbw[4])
{
    int (*string_proc) (P4(gs_font_type42 *, ulong, uint, const byte **)) =
    pfont->data.string_proc;
    float scale = pfont->data.unitsPerEm;
    uint widthx;
    int lsbx;
    int code;

    {
	uint num_metrics = pfont->data.numLongMetrics;
	const byte *hmetrics;

	if (glyph_index < num_metrics) {
	    access(pfont->data.hmtx + glyph_index * 4, 4, hmetrics);
	    widthx = u16(hmetrics);
	    lsbx = s16(hmetrics + 2);
	} else {
	    uint offset = pfont->data.hmtx + (num_metrics - 1) * 4;
	    const byte *lsb;

	    access(offset, 4, hmetrics);
	    widthx = u16(hmetrics);
	    offset += 4 + (glyph_index - num_metrics) * 2;
	    if (offset >= pfont->data.hmtx_length)
		offset = pfont->data.hmtx_length - 2;
	    access(offset, 2, lsb);
	    lsbx = s16(lsb);
	}
    }
    psbw[0] = lsbx / scale;
    psbw[1] = 0;
    psbw[2] = widthx / scale;
    psbw[3] = 0;
    return 0;
}

/* Define the bits in the glyph flags. */
#define gf_OnCurve 1
#define gf_xShort 2
#define gf_yShort 4
#define gf_Repeat 8
#define gf_xPos 16		/* xShort */
#define gf_xSame 16		/* !xShort */
#define gf_yPos 32		/* yShort */
#define gf_ySame 32		/* !yShort */

/* Define the bits in the component glyph flags. */
#define cg_argsAreWords 1
#define cg_argsAreXYValues 2
#define cg_haveScale 8
#define cg_moreComponents 32
#define cg_haveXYScale 64
#define cg_have2x2 128

/* Forward references */
private int append_outline(P4(uint glyph_index, const gs_matrix_fixed * pmat,
			      gx_path * ppath, gs_font_type42 * pfont));

/* Append a TrueType outline to a path. */
/* Note that this does not append the final moveto for the width. */
int
gs_type42_append(uint glyph_index, gs_imager_state * pis,
    gx_path * ppath, const gs_log2_scale_point * pscale, bool charpath_flag,
		 int paint_type, gs_font_type42 * pfont)
{
    float sbw[4];

    gs_type42_get_metrics(pfont, glyph_index, sbw);
    /*
     * This is where we should do something about the l.s.b., but I
     * can't figure out from the TrueType documentation what it should
     * be.
     */
    return append_outline(glyph_index, &pis->ctm, ppath, pfont);
}

/* Append a simple glyph outline. */
private int
append_simple(const byte * glyph, const gs_matrix_fixed * pmat, gx_path * ppath,
	      gs_font_type42 * pfont)
{
    int numContours = s16(glyph);
    const byte *pends = glyph + 10;
    const byte *pinstr = pends + numContours * 2;
    const byte *pflags;
    uint npoints;
    const byte *pxc, *pyc;
    int code;

    if (numContours == 0)
	return 0;
    /*
     * It appears that the only way to find the X and Y coordinate
     * tables is to parse the flags.  If this is true, it is an
     * incredible piece of bad design.
     */
    {
	const byte *pf = pflags = pinstr + 2 + u16(pinstr);
	uint xbytes = npoints = u16(pinstr - 2) + 1;
	uint np = npoints;

	while (np > 0) {
	    byte flags = *pf++;
	    uint reps = (flags & gf_Repeat ? *pf++ + 1 : 1);

	    if (!(flags & gf_xShort)) {
		if (flags & gf_xSame)
		    xbytes -= reps;
		else
		    xbytes += reps;
	    }
	    np -= reps;
	}
	pxc = pf;
	pyc = pxc + xbytes;
    }

    /* Interpret the contours. */

    {
	uint i, np;
	gs_fixed_point pt;
	float scale = pfont->data.unitsPerEm;
	uint reps = 0;
	byte flags;

	gs_point_transform2fixed(pmat, 0.0, 0.0, &pt);
	for (i = 0, np = 0; i < numContours; ++i) {
	    bool move = true;
	    uint last_point = u16(pends + i * 2);
	    float dx, dy;
	    int off_curve = 0;
	    gs_fixed_point start;
	    gs_fixed_point cpoints[3];

	    for (; np <= last_point; --reps, ++np) {
		gs_fixed_point dpt;

		if (reps == 0) {
		    flags = *pflags++;
		    reps = (flags & gf_Repeat ? *pflags++ + 1 : 1);
		}
		if (flags & gf_xShort)
		    dx = (flags & gf_xPos ? *pxc++ : -(int)*pxc++) / scale;
		else if (!(flags & gf_xSame))
		    dx = s16(pxc) / scale, pxc += 2;
		else
		    dx = 0;
		if (flags & gf_yShort)
		    dy = (flags & gf_yPos ? *pyc++ : -(int)*pyc++) / scale;
		else if (!(flags & gf_ySame))
		    dy = s16(pyc) / scale, pyc += 2;
		else
		    dy = 0;
		code = gs_distance_transform2fixed(pmat, dx, dy, &dpt);
		if (code < 0)
		    return code;
		pt.x += dpt.x, pt.y += dpt.y;
#define control1(xy) cpoints[1].xy
#define control2(xy) cpoints[2].xy
#define control3off(xy) ((cpoints[1].xy + pt.xy) / 2)
#define control4off(xy) ((cpoints[0].xy + 2 * cpoints[1].xy) / 3)
#define control5off(xy) ((2 * cpoints[1].xy + cpoints[2].xy) / 3)
#define control6off(xy) ((2 * cpoints[1].xy + pt.xy) / 3)
#define control7off(xy) ((2 * cpoints[1].xy + start.xy) / 3)
		if (move) {
		    if_debug2('1', "[1t]start (%g,%g)\n",
			      fixed2float(pt.x), fixed2float(pt.y));
		    start = pt;
		    code = gx_path_add_point(ppath, pt.x, pt.y);
		    cpoints[0] = pt;
		    move = false;
		} else if (flags & gf_OnCurve) {
		    if_debug2('1', "[1t]ON (%g,%g)\n",
			      fixed2float(pt.x), fixed2float(pt.y));
		    if (off_curve)
			code = gx_path_add_curve(ppath, control4off(x),
					     control4off(y), control6off(x),
						 control6off(y), pt.x, pt.y);
		    else
			code = gx_path_add_line(ppath, pt.x, pt.y);
		    cpoints[0] = pt;
		    off_curve = 0;
		} else {
		    if_debug2('1', "[1t]...off (%g,%g)\n",
			      fixed2float(pt.x), fixed2float(pt.y));
		    switch (off_curve++) {
			default:	/* >= 1 */
			    control2(x) = control3off(x);
			    control2(y) = control3off(y);
			    code = gx_path_add_curve(ppath,
					     control4off(x), control4off(y),
					     control5off(x), control5off(y),
						  control2(x), control2(y));
			    cpoints[0] = cpoints[2];
			    off_curve = 1;
			    /* falls through */
			case 0:
			    cpoints[1] = pt;
		    }
		}
		if (code < 0)
		    return code;
	    }
	    if (off_curve)
		code = gx_path_add_curve(ppath, control4off(x), control4off(y),
					 control7off(x), control7off(y),
					 start.x, start.y);
	    code = gx_path_close_subpath(ppath);
	    if (code < 0)
		return code;
	}
    }
    return 0;
}

/* Append a glyph outline. */
private int
append_outline(uint glyph_index, const gs_matrix_fixed * pmat, gx_path * ppath,
	       gs_font_type42 * pfont)
{
    gs_const_string glyph_string;

#define glyph glyph_string.data
    int numContours;
    int code;

    code = (*pfont->data.get_outline) (pfont, glyph_index, &glyph_string);
    if (code < 0)
	return code;
    if (glyph == 0 || glyph_string.size == 0)	/* empty glyph */
	return 0;
    numContours = s16(glyph);
    if (numContours >= 0)
	return append_simple(glyph, pmat, ppath, pfont);
    if (numContours != -1)
	return_error(gs_error_rangecheck);
    /* This is a component glyph.  Things get messy.  */
    {
	uint flags;
	float scale = pfont->data.unitsPerEm;

	glyph += 10;
	do {
	    uint comp_index = u16(glyph + 2);
	    gs_matrix_fixed mat;
	    gs_matrix scale_mat;

	    flags = u16(glyph);
	    glyph += 4;
	    mat = *pmat;
	    if (flags & cg_argsAreXYValues) {
		int arg1, arg2;
		gs_fixed_point pt;

		if (flags & cg_argsAreWords)
		    arg1 = s16(glyph), arg2 = s16(glyph + 2), glyph += 4;
		else
		    arg1 = s8(glyph), arg2 = s8(glyph + 1), glyph += 2;
		gs_point_transform2fixed(pmat, arg1 / scale,
					 arg2 / scale, &pt);
/****** HACK: WE KNOW ABOUT FIXED MATRICES ******/
		mat.tx = fixed2float(mat.tx_fixed = pt.x);
		mat.ty = fixed2float(mat.ty_fixed = pt.y);
	    } else {
/****** WE DON'T HANDLE POINT MATCHING YET ******/
		glyph += (flags & cg_argsAreWords ? 4 : 2);
	    }
#define s2_14(p) (s16(p) / 16384.0)
	    if (flags & cg_haveScale) {
		scale_mat.xx = scale_mat.yy = s2_14(glyph);
		scale_mat.xy = scale_mat.yx = 0;
		glyph += 2;
	    } else if (flags & cg_haveXYScale) {
		scale_mat.xx = s2_14(glyph);
		scale_mat.yy = s2_14(glyph + 2);
		scale_mat.xy = scale_mat.yx = 0;
		glyph += 4;
	    } else if (flags & cg_have2x2) {
		scale_mat.xx = s2_14(glyph);
		scale_mat.xy = s2_14(glyph + 2);
		scale_mat.yx = s2_14(glyph + 4);
		scale_mat.yy = s2_14(glyph + 6);
		glyph += 8;
	    } else
		goto no_scale;
#undef s2_14
	    scale_mat.tx = 0;
	    scale_mat.ty = 0;
	    /* The scale doesn't affect mat.t{x,y}, so we don't */
	    /* need to update the fixed components. */
	    gs_matrix_multiply(&scale_mat, (const gs_matrix *)&mat,
			       (gs_matrix *) & mat);
	  no_scale:code = append_outline(comp_index, &mat, ppath, pfont);
	    if (code < 0)
		return code;
	}
	while (flags & cg_moreComponents);
    }
    return 0;
#undef glyph
}
