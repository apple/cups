/* Copyright (C) 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zfont42.c,v 1.2 2000/03/08 23:15:37 mike Exp $ */
/* Type 42 font creation operator */
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gsccode.h"
#include "gsmatrix.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "bfont.h"
#include "idict.h"
#include "idparam.h"
#include "store.h"

/* Forward references */
private int z42_string_proc(P4(gs_font_type42 *, ulong, uint, const byte **));
private int z42_gdir_get_outline(P3(gs_font_type42 *, uint, gs_const_string *));

/* <string|name> <font_dict> .buildfont11/42 <string|name> <font> */
/* Build a type 11 (TrueType CID-keyed) or 42 (TrueType) font. */
int
build_gs_TrueType_font(os_ptr op, font_type ftype, const char *bcstr,
		       const char *bgstr, build_font_options_t options)
{
    build_proc_refs build;
    ref sfnts, sfnts0, GlyphDirectory;
    gs_font_type42 *pfont;
    font_data *pdata;
    int code;

    code = build_proc_name_refs(&build, bcstr, bgstr);
    if (code < 0)
	return code;
    check_type(*op, t_dictionary);
    {
	ref *psfnts;
	ref *pGlyphDirectory;

	if (dict_find_string(op, "sfnts", &psfnts) <= 0)
	    return_error(e_invalidfont);
	if ((code = array_get(psfnts, 0L, &sfnts0)) < 0)
	    return code;
	if (!r_has_type(&sfnts0, t_string))
	    return_error(e_typecheck);
	if (dict_find_string(op, "GlyphDirectory", &pGlyphDirectory) <= 0)
	    make_null(&GlyphDirectory);
	else if (!r_has_type(pGlyphDirectory, t_dictionary))
	    return_error(e_typecheck);
	else
	    GlyphDirectory = *pGlyphDirectory;
	/*
	 * Since build_gs_primitive_font may resize the dictionary and cause
	 * pointers to become invalid, save sfnts.
	 */
	sfnts = *psfnts;
    }
    code = build_gs_primitive_font(op, (gs_font_base **) & pfont, ftype,
				   &st_gs_font_type42, &build, options);
    if (code != 0)
	return code;
    pdata = pfont_data(pfont);
    ref_assign(&pdata->u.type42.sfnts, &sfnts);
    ref_assign(&pdata->u.type42.GlyphDirectory, &GlyphDirectory);
    pfont->data.string_proc = z42_string_proc;
    pfont->data.proc_data = (char *)pdata;
    code = gs_type42_font_init(pfont);
    if (code < 0)
	return code;
    /*
     * Some versions of the Adobe PostScript Windows driver have a bug
     * that causes them to output the FontBBox for Type 42 fonts in the
     * 2048- or 4096-unit character space rather than a 1-unit space.
     * Work around this here.
     */
    if (pfont->FontBBox.q.x - pfont->FontBBox.p.x > 100 ||
	pfont->FontBBox.q.y - pfont->FontBBox.p.y > 100
	) {
	float upem = pfont->data.unitsPerEm;

	pfont->FontBBox.p.x /= upem;
	pfont->FontBBox.p.y /= upem;
	pfont->FontBBox.q.x /= upem;
	pfont->FontBBox.q.y /= upem;
    }
    /*
     * Apparently Adobe versions 2015 and later use an alternate
     * method of accessing character outlines: instead of loca and glyf,
     * they use a dictionary called GlyphDirectory.  In this case,
     * we use an alternate get_outline procedure.
     */
    if (!r_has_type(&GlyphDirectory, t_null))
	pfont->data.get_outline = z42_gdir_get_outline;
    return define_gs_font((gs_font *) pfont);
}
private int
zbuildfont42(os_ptr op)
{
    return build_gs_TrueType_font(op, ft_TrueType, "%Type42BuildChar",
				  "%Type42BuildGlyph", bf_options_none);
}

/* ------ Initialization procedure ------ */

const op_def zfont42_op_defs[] =
{
    {"2.buildfont42", zbuildfont42},
    op_def_end(0)
};

/* Get an outline from GlyphDirectory instead of loca / glyf. */
private int
z42_gdir_get_outline(gs_font_type42 * pfont, uint glyph_index,
		     gs_const_string * pgstr)
{
    const font_data *pfdata = pfont_data(pfont);
    const ref *pgdir = &pfdata->u.type42.GlyphDirectory;
    ref iglyph;
    ref *pgdef;

    make_int(&iglyph, glyph_index);
    if (dict_find(pgdir, &iglyph, &pgdef) <= 0) {
	pgstr->data = 0;
	pgstr->size = 0;
    } else if (!r_has_type(pgdef, t_string)) {
	return_error(e_typecheck);
    } else {
	pgstr->data = pgdef->value.const_bytes;
	pgstr->size = r_size(pgdef);
    }
    return 0;
}

/* Procedure for accessing the sfnts array. */
private int
z42_string_proc(gs_font_type42 * pfont, ulong offset, uint length,
		const byte ** pdata)
{
    const font_data *pfdata = pfont_data(pfont);
    ulong left = offset;
    uint index = 0;

    for (;; ++index) {
	ref rstr;
	int code = array_get(&pfdata->u.type42.sfnts, index, &rstr);
	uint size;

	if (code < 0)
	    return code;
	if (!r_has_type(&rstr, t_string))
	    return_error(e_typecheck);
	/*
	 * NOTE: According to the Adobe documentation, each sfnts
	 * string should have even length.  If the length is odd,
	 * the additional byte is padding and should be ignored.
	 */
	size = r_size(&rstr) & ~1;
	if (left < size) {
	    if (left + length > size)
		return_error(e_rangecheck);
	    *pdata = rstr.value.const_bytes + left;
	    return 0;
	}
	left -= size;
    }
}
