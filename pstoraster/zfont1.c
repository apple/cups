/* Copyright (C) 1991, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zfont1.c */
/* Type 1 and Type 4 font creation operator */
#include "ghost.h"
#include "errors.h"
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

/* Type 1 auxiliary procedures (defined in zchar1.c) */
extern int z1_subr_proc(P3(gs_font_type1 *, int, gs_const_string *));
extern int z1_seac_proc(P3(gs_font_type1 *, int, gs_const_string *));
extern int z1_push_proc(P3(gs_font_type1 *, const fixed *, int));
extern int z1_pop_proc(P2(gs_font_type1 *, fixed *));

/* Default value of lenIV */
#define default_lenIV 4

/* Build a Type 1 or Type 4 font. */
private int
buildfont1or4(os_ptr op, build_proc_refs *pbuild, font_type ftype)
{	gs_type1_data data1;
	ref *pothersubrs;
	ref *psubrs;
	ref *pprivate;
	static ref no_subrs;
	gs_font_type1 *pfont;
	font_data *pdata;
	int code;

	check_type(*op, t_dictionary);
	if ( dict_find_string(op, "Private", &pprivate) <= 0 ||
	    !r_has_type(pprivate, t_dictionary)
	   )
	  return_error(e_invalidfont);
	make_empty_array(&no_subrs, 0);
	if ( dict_find_string(pprivate, "OtherSubrs", &pothersubrs) > 0 )
	{	if ( !r_is_array(pothersubrs) )  /* allow packed array */
		  return_error(e_invalidfont);
	}
	else
		pothersubrs = &no_subrs;
	if ( dict_find_string(pprivate, "Subrs", &psubrs) > 0 )
	{	if ( !r_is_array(psubrs) )	/* allow packed array */
		  return_error(e_invalidfont);
	}
	else
		psubrs = &no_subrs;
	/* Get the rest of the information from the Private dictionary. */
	if ( (code = dict_int_param(pprivate, "lenIV", 0, 255,
				    default_lenIV, &data1.lenIV)) < 0 ||
	     (code = dict_int_param(pprivate, "BlueFuzz", 0, 1999, 1,
				    &data1.BlueFuzz)) < 0 ||
	     (code = dict_float_param(pprivate, "BlueScale", 0.039625,
				      &data1.BlueScale)) < 0 ||
	     (code = dict_float_param(pprivate, "BlueShift", 7.0,
				    &data1.BlueShift)) < 0 ||
	     (code = data1.BlueValues.count = dict_float_array_param(pprivate,
		"BlueValues", max_BlueValues * 2,
		&data1.BlueValues.values[0], NULL)) < 0 ||
	     (code = dict_float_param(pprivate, "ExpansionFactor", 0.06,
				      &data1.ExpansionFactor)) < 0 ||
	     (code = data1.FamilyBlues.count = dict_float_array_param(pprivate,
		"FamilyBlues", max_FamilyBlues * 2,
		&data1.FamilyBlues.values[0], NULL)) < 0 ||
	     (code = data1.FamilyOtherBlues.count = dict_float_array_param(pprivate,
		"FamilyOtherBlues", max_FamilyOtherBlues * 2,
		&data1.FamilyOtherBlues.values[0], NULL)) < 0 ||
	     (code = dict_bool_param(pprivate, "ForceBold", false,
				     &data1.ForceBold)) < 0 ||
	     (code = dict_int_param(pprivate, "LanguageGroup", 0, 1, 0,
				    &data1.LanguageGroup)) < 0 ||
	     (code = data1.OtherBlues.count = dict_float_array_param(pprivate,
		"OtherBlues", max_OtherBlues * 2,
		&data1.OtherBlues.values[0], NULL)) < 0 ||
	     (code = dict_bool_param(pprivate, "RndStemUp", true,
				     &data1.RndStemUp)) < 0 ||
	     (code = data1.StdHW.count = dict_float_array_param(pprivate,
		"StdHW", 1, &data1.StdHW.values[0], NULL)) < 0 ||
	     (code = data1.StdVW.count = dict_float_array_param(pprivate,
		"StdVW", 1, &data1.StdVW.values[0], NULL)) < 0 ||
	     (code = data1.StemSnapH.count = dict_float_array_param(pprivate,
		"StemSnapH", max_StemSnap,
		&data1.StemSnapH.values[0], NULL)) < 0 ||
	     (code = data1.StemSnapV.count = dict_float_array_param(pprivate,
		"StemSnapV", max_StemSnap,
		&data1.StemSnapV.values[0], NULL)) < 0 ||
	/* The WeightVector is in the font dictionary, not Private. */
	     (code = dict_float_array_param(op, "WeightVector",
		max_WeightVector, &data1.WeightVector[0], NULL)) < 0
	   )
		return code;
	/*
	 * According to section 5.6 of the "Adobe Type 1 Font Format",
	 * there is a requirement that BlueScale times the maximum
	 * alignment zone height must be less than 1.  Some fonts
	 * produced by Fontographer have ridiculously large BlueScale
	 * values, so we force BlueScale back into range here.
	 */
	{ float max_zone_height = 1.0;
	  float zone_height;
	  int i;
#define scan_zone(z)\
  for ( i = 0; i < data1.z.count; i += 2 )\
    if ( (zone_height = data1.z.values[i+1] - data1.z.values[i]) > max_zone_height )\
      max_zone_height = zone_height
	  scan_zone(BlueValues);
	  scan_zone(OtherBlues);
	  scan_zone(FamilyBlues);
	  scan_zone(FamilyOtherBlues);
	  if ( data1.BlueScale * max_zone_height > 1.0 )
	    data1.BlueScale = 1.0 / max_zone_height;
	}
	/* Do the work common to primitive font types. */
	code = build_gs_primitive_font(op, (gs_font_base **)&pfont, ftype,
				       &st_gs_font_type1, pbuild);
	if ( code != 0 )
	  return code;
	/* This is a new font, fill it in. */
	pdata = pfont_data(pfont);
	pfont->data = data1;
	ref_assign(&pdata->u.type1.OtherSubrs, pothersubrs);
	ref_assign(&pdata->u.type1.Subrs, psubrs);
	pfont->data.subr_proc = z1_subr_proc;
	pfont->data.seac_proc = z1_seac_proc;
	pfont->data.push_proc = z1_push_proc;
	pfont->data.pop_proc = z1_pop_proc;
	pfont->data.proc_data = (char *)pdata;
	return define_gs_font((gs_font *)pfont);
}

/* <string|name> <font_dict> .buildfont1 <string|name> <font> */
/* Build a type 1 (Adobe encrypted) font. */
private int
zbuildfont1(os_ptr op)
{	build_proc_refs build;
	int code =
	  build_proc_name_refs(&build,
			       "%Type1BuildChar", "%Type1BuildGlyph");
	if ( code < 0 )
	  return code;
	return buildfont1or4(op, &build, ft_encrypted);
}

/* <string|name> <font_dict> .buildfont4 <string|name> <font> */
/* Build a type 4 (disk-based Adobe encrypted) font. */
private int
zbuildfont4(os_ptr op)
{	build_proc_refs build;
	int code = build_gs_font_procs(op, &build);
	if ( code < 0 )
	  return code;
	return buildfont1or4(op, &build, ft_disk_based);
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zfont1_op_defs) {
	{"2.buildfont1", zbuildfont1},
	{"2.buildfont4", zbuildfont4},
END_OP_DEFS(0) }
