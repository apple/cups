/* Copyright (C) 1991, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zfont1.c,v 1.2 2000/03/08 23:15:37 mike Exp $ */
/* Type 1 and Type 4 font creation operator */
#include "ghost.h"
#include "oper.h"
#include "gxfixed.h"
#include "gsmatrix.h"
#include "gxdevice.h"
#include "gschar.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "bfont.h"
#include "ialloc.h"
#include "idict.h"
#include "idparam.h"
#include "store.h"

/*#define TEST*/

/* Type 1 auxiliary procedures (defined in zchar1.c) */
extern const gs_type1_data_procs_t z1_data_procs;

/* Default value of lenIV */
#define DEFAULT_LENIV_1 4
#define DEFAULT_LENIV_2 (-1)

/* Private utilities */
private uint
subr_bias(const ref * psubrs)
{
    uint size = r_size(psubrs);

    return (size < 1240 ? 107 : size < 33900 ? 1131 : 32768);
}
private void
find_zone_height(float *pmax_height, int count, const float *values)
{
    int i;
    float zone_height;

    for (i = 0; i < count; i += 2)
	if ((zone_height = values[i + 1] - values[i]) > *pmax_height)
	    *pmax_height = zone_height;
}

/* Build a Type 1 or Type 4 font. */
private int
buildfont1or4(os_ptr op, build_proc_refs * pbuild, font_type ftype,
	      build_font_options_t options)
{
    gs_type1_data data1;
    ref no_subrs;
    ref *pothersubrs = &no_subrs;
    ref *psubrs = &no_subrs;
    ref *pglobalsubrs = &no_subrs;
    ref *pprivate;
    gs_font_type1 *pfont;
    font_data *pdata;
    int code;

    check_type(*op, t_dictionary);
    if (dict_find_string(op, "Private", &pprivate) <= 0 ||
	!r_has_type(pprivate, t_dictionary)
	)
	return_error(e_invalidfont);
    make_empty_array(&no_subrs, 0);
    if (dict_find_string(pprivate, "OtherSubrs", &pothersubrs) > 0) {
	if (!r_is_array(pothersubrs))
	    return_error(e_typecheck);
    }
    if (dict_find_string(pprivate, "Subrs", &psubrs) > 0) {
	if (!r_is_array(psubrs))
	    return_error(e_typecheck);
    }
    if ((code = dict_int_param(op, "CharstringType", 1, 2, 1,
			       &data1.CharstringType)) < 0
	)
	return code;
    /* Get information specific to Type 2 charstrings. */
    if (data1.CharstringType == 2) {
	float dwx, nwx;

	data1.subroutineNumberBias = subr_bias(psubrs);
	if (dict_find_string(pprivate, "GlobalSubrs", &pglobalsubrs) > 0) {
	    if (!r_is_array(pglobalsubrs))
		return_error(e_typecheck);
	}
	data1.gsubrNumberBias = subr_bias(pglobalsubrs);
	if ((code = dict_uint_param(pprivate, "gsubrNumberBias",
				    0, max_uint, data1.gsubrNumberBias,
				    &data1.gsubrNumberBias)) < 0 ||
	    (code = dict_float_param(pprivate, "defaultWidthX", 0.0,
				     &dwx)) < 0 ||
	    (code = dict_float_param(pprivate, "nominalWidthX", 0.0,
				     &nwx)) < 0
	    )
	    return code;
	data1.defaultWidthX = float2fixed(dwx);
	data1.nominalWidthX = float2fixed(nwx);
	{
	    ref *pirs;

	    if (dict_find_string(pprivate, "initialRandomSeed", &pirs) <= 0)
		data1.initialRandomSeed = 0;
	    else if (!r_has_type(pirs, t_integer))
		return_error(e_typecheck);
	    else
		data1.initialRandomSeed = pirs->value.intval;
	}
	data1.lenIV = DEFAULT_LENIV_2;
    } else {
	data1.subroutineNumberBias = 0;
	data1.gsubrNumberBias = 0;
	data1.lenIV = DEFAULT_LENIV_1;
    }
    /* Get the rest of the information from the Private dictionary. */
    if ((code = dict_int_param(pprivate, "lenIV", -1, 255, data1.lenIV,
			       &data1.lenIV)) < 0 ||
	(code = dict_uint_param(pprivate, "subroutineNumberBias",
				0, max_uint, data1.subroutineNumberBias,
				&data1.subroutineNumberBias)) < 0 ||
	(code = dict_int_param(pprivate, "BlueFuzz", 0, 1999, 1,
			       &data1.BlueFuzz)) < 0 ||
	(code = dict_float_param(pprivate, "BlueScale", 0.039625,
				 &data1.BlueScale)) < 0 ||
	(code = dict_float_param(pprivate, "BlueShift", 7.0,
				 &data1.BlueShift)) < 0 ||
	(code = data1.BlueValues.count =
	 dict_float_array_param(pprivate, "BlueValues", max_BlueValues * 2,
				&data1.BlueValues.values[0], NULL)) < 0 ||
	(code = dict_float_param(pprivate, "ExpansionFactor", 0.06,
				 &data1.ExpansionFactor)) < 0 ||
	(code = data1.FamilyBlues.count =
	 dict_float_array_param(pprivate, "FamilyBlues", max_FamilyBlues * 2,
				&data1.FamilyBlues.values[0], NULL)) < 0 ||
	(code = data1.FamilyOtherBlues.count =
	 dict_float_array_param(pprivate,
				"FamilyOtherBlues", max_FamilyOtherBlues * 2,
			    &data1.FamilyOtherBlues.values[0], NULL)) < 0 ||
	(code = dict_bool_param(pprivate, "ForceBold", false,
				&data1.ForceBold)) < 0 ||
	(code = dict_int_param(pprivate, "LanguageGroup", 0, 1, 0,
			       &data1.LanguageGroup)) < 0 ||
	(code = data1.OtherBlues.count =
	 dict_float_array_param(pprivate, "OtherBlues", max_OtherBlues * 2,
				&data1.OtherBlues.values[0], NULL)) < 0 ||
	(code = dict_bool_param(pprivate, "RndStemUp", true,
				&data1.RndStemUp)) < 0 ||
	(code = data1.StdHW.count =
	 dict_float_array_param(pprivate, "StdHW", 1,
				&data1.StdHW.values[0], NULL)) < 0 ||
	(code = data1.StdVW.count =
	 dict_float_array_param(pprivate, "StdVW", 1,
				&data1.StdVW.values[0], NULL)) < 0 ||
	(code = data1.StemSnapH.count =
	 dict_float_array_param(pprivate, "StemSnapH", max_StemSnap,
				&data1.StemSnapH.values[0], NULL)) < 0 ||
	(code = data1.StemSnapV.count =
	 dict_float_array_param(pprivate, "StemSnapV", max_StemSnap,
				&data1.StemSnapV.values[0], NULL)) < 0 ||
    /* The WeightVector is in the font dictionary, not Private. */
	(code = data1.WeightVector.count =
	 dict_float_array_param(op, "WeightVector", max_WeightVector,
				data1.WeightVector.values, NULL)) < 0
	)
	return code;
    /*
     * According to section 5.6 of the "Adobe Type 1 Font Format",
     * there is a requirement that BlueScale times the maximum
     * alignment zone height must be less than 1.  Some fonts
     * produced by Fontographer have ridiculously large BlueScale
     * values, so we force BlueScale back into range here.
     */
    {
	float max_zone_height = 1.0;

#define SCAN_ZONE(z)\
    find_zone_height(&max_zone_height, data1.z.count, data1.z.values);

	SCAN_ZONE(BlueValues);
	SCAN_ZONE(OtherBlues);
	SCAN_ZONE(FamilyBlues);
	SCAN_ZONE(FamilyOtherBlues);

#undef SCAN_ZONE

	if (data1.BlueScale * max_zone_height > 1.0)
	    data1.BlueScale = 1.0 / max_zone_height;
    }
    /* Do the work common to primitive font types. */
    code = build_gs_primitive_font(op, (gs_font_base **) & pfont, ftype,
				   &st_gs_font_type1, pbuild, options);
    if (code != 0)
	return code;
    /* This is a new font, fill it in. */
    pdata = pfont_data(pfont);
    pfont->data = data1;
    ref_assign(&pdata->u.type1.OtherSubrs, pothersubrs);
    ref_assign(&pdata->u.type1.Subrs, psubrs);
    ref_assign(&pdata->u.type1.GlobalSubrs, pglobalsubrs);
    pfont->data.procs = &z1_data_procs;
    pfont->data.proc_data = (char *)pdata;
    return define_gs_font((gs_font *)pfont);
}

/* <string|name> <font_dict> .buildfont1 <string|name> <font> */
/* Build a type 1 (Adobe encrypted) font. */
private int
zbuildfont1(os_ptr op)
{
    build_proc_refs build;
    int code = build_proc_name_refs(&build,
				    "%Type1BuildChar", "%Type1BuildGlyph");

    if (code < 0)
	return code;
    return buildfont1or4(op, &build, ft_encrypted, bf_notdef_required);
}

/* <string|name> <font_dict> .buildfont4 <string|name> <font> */
/* Build a type 4 (disk-based Adobe encrypted) font. */
private int
zbuildfont4(os_ptr op)
{
    build_proc_refs build;
    int code = build_gs_font_procs(op, &build);

    if (code < 0)
	return code;
    return buildfont1or4(op, &build, ft_disk_based, bf_options_none);
}

#ifdef TEST

#include "igstate.h"
#include "stream.h"
#include "files.h"

/* <file> .printfont1 - */
private int
zprintfont1(os_ptr op)
{
    const gs_font *pfont = gs_currentfont(igs);
    stream *s;
    int code;

    if (pfont->FontType != ft_encrypted)
	return_error(e_rangecheck);
    check_write_file(s, op);
    code = psdf_embed_type1_font(s, (gs_font_type1 *) pfont);
    if (code >= 0)
	pop(1);
    return code;
}

#endif

/* ------ Initialization procedure ------ */

const op_def zfont1_op_defs[] =
{
    {"2.buildfont1", zbuildfont1},
    {"2.buildfont4", zbuildfont4},
#ifdef TEST
    {"2.printfont1", zprintfont1},
#endif
    op_def_end(0)
};
