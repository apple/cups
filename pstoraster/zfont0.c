/* Copyright (C) 1991, 1992, 1993 Aladdin Enterprises.  All rights reserved.
  
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

/* zfont0.c */
/* Composite font creation operator */
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "gsstruct.h"
/*
 * The following lines used to say:
 *	#include "gsmatrix.h"
 *	#include "gxdevice.h"		/. for gxfont.h ./
 * Tony Li says the longer list is necessary to keep the GNU compiler
 * happy, but this is pretty hard to understand....
 */
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gzstate.h"		/* must precede gxdevice */
#include "gxdevice.h"		/* must precede gxfont */
#include "gschar.h"
#include "gxfont.h"
#include "gxfont0.h"
#include "bfont.h"
#include "ialloc.h"
#include "idict.h"
#include "igstate.h"
#include "store.h"

/* Composite font procedures */
extern font_proc_init_fstack(gs_type0_init_fstack);
extern font_proc_next_char(gs_type0_next_char);
extern font_proc_define_font(gs_type0_define_font);
extern font_proc_make_font(gs_type0_make_font);

/* Forward references */
private font_proc_define_font(ztype0_define_font);
private font_proc_make_font(ztype0_make_font);
private int ensure_char_entry(P4(os_ptr, const char _ds *, byte *, int));

/* <string|name> <font_dict> .buildfont0 <string|name> <font> */
/* Build a type 0 (composite) font. */
private int
zbuildfont0(os_ptr op)
{	ref *pfmaptype;
	gs_type0_data data;
	ref *pfdepvector;
	ref *pprefenc;
	ref *psubsvector;
	gs_font_type0 *pfont;
	font_data *pdata;
	int i;
	int code;
	check_type(*op, t_dictionary);
	if ( dict_find_string(op, "FMapType", &pfmaptype) <= 0 ||
	     !r_has_type(pfmaptype, t_integer) ||
	     pfmaptype->value.intval < (int)fmap_type_min ||
	     pfmaptype->value.intval > (int)fmap_type_max ||
	     dict_find_string(op, "FDepVector", &pfdepvector) <= 0 ||
	     !r_is_array(pfdepvector)
	   )
		return_error(e_invalidfont);
	data.FMapType = (fmap_type)pfmaptype->value.intval;
	/* Check that every element of the FDepVector is a font. */
	data.fdep_size = r_size(pfdepvector);
	for ( i = 0; i < data.fdep_size; i++ )
	{	ref fdep;
		ref *pfid;
		gs_font *psub;
		array_get(pfdepvector, i, &fdep);
		if ( !r_has_type(&fdep, t_dictionary) ||
		     dict_find_string(&fdep, "FID", &pfid) <= 0 ||
		     !r_has_type(pfid, t_fontID)
		   )
			return_error(e_invalidfont);
		/*
		 * Check the inheritance rules.  Allowed configurations
		 * (paths from root font) are defined by the regular
		 * expression:
		 *	(shift | double_escape escape* | escape*)
		 *	  non_modal* non_composite
		 */
		psub = r_ptr(pfid, gs_font);
		if ( psub->FontType == ft_composite )
		{
#define psub0 ((gs_font_type0 *)psub)
			fmap_type fmt = psub0->data.FMapType;
			if ( fmt == fmap_double_escape ||
			     fmt == fmap_shift ||
			     (fmt == fmap_escape &&
			      !(data.FMapType == fmap_escape ||
				data.FMapType == fmap_double_escape))
			   )
				return_error(e_invalidfont);
#undef psub0
		}
	}
	switch ( data.FMapType )
	{
	case fmap_escape: case fmap_double_escape:	/* need EscChar */
		code = ensure_char_entry(op, "EscChar", &data.EscChar, 255);
		break;
	case fmap_shift:			/* need ShiftIn & ShiftOut */
		code = ensure_char_entry(op, "ShiftIn", &data.ShiftIn, 15);
		if ( code >= 0 )
		  code = ensure_char_entry(op, "ShiftOut", &data.ShiftOut, 14);
		break;
	case fmap_SubsVector:			/* need SubsVector */
	{	uint svsize;
		if ( dict_find_string(op, "SubsVector", &psubsvector) <= 0 ||
		     !r_has_type(psubsvector, t_string) ||
		     (svsize = r_size(psubsvector)) == 0 ||
		     (data.subs_width = (int)*psubsvector->value.bytes + 1) > 4 ||
		     (svsize - 1) % data.subs_width != 0
		   )
			return_error(e_invalidfont);
		data.subs_size = (svsize - 1) / data.subs_width;
		data.SubsVector.data = psubsvector->value.bytes + 1;
		data.SubsVector.size = svsize - 1;
	}
	default:
		code = 0;
	}
	if ( code < 0 ) return code;
	{	build_proc_refs build;
		code = build_proc_name_refs(&build,
			"%Type0BuildChar", "%Type0BuildGlyph");
		if ( code < 0 ) return code;
		code = build_gs_font(op, (gs_font **)&pfont,
				     ft_composite, &st_gs_font_type0, &build);
	}
	if ( code != 0 ) return code;
	pfont->procs.init_fstack = gs_type0_init_fstack;
	pfont->procs.next_char = gs_type0_next_char;
	pfont->procs.define_font = ztype0_define_font;
	pfont->procs.make_font = ztype0_make_font;
	if ( dict_find_string(op, "PrefEnc", &pprefenc) <= 0 )
	{	ref nul;
		make_null_new(&nul);
		if ( (code = dict_put_string(op, "PrefEnc", &nul)) < 0 )
			return code;
	}
	/* Fill in the font data */
	pdata = pfont_data(pfont);
	data.encoding_size = r_size(&pdata->Encoding);
	data.Encoding =
	  (uint *)ialloc_byte_array(data.encoding_size, sizeof(uint),
				    "buildfont0(Encoding)");
	if ( data.Encoding == 0 )
		return_error(e_VMerror);
	/* Fill in the encoding vector, checking to make sure that */
	/* each element is an integer between 0 and fdep_size-1. */
	for ( i = 0; i < data.encoding_size; i++ )
	{	ref enc;
		array_get(&pdata->Encoding, i, &enc);
		check_int_leu_only(enc, data.fdep_size);
		data.Encoding[i] = (uint)enc.value.intval;
	}
	data.FDepVector =
	  ialloc_struct_array(data.fdep_size, gs_font *,
			      &st_gs_font_ptr_element,
			      "buildfont0(FDepVector)");
	if ( data.FDepVector == 0 )
		return_error(e_VMerror);
	for ( i = 0; i < data.fdep_size; i++ )
	{	ref fdep;
		ref *pfid;
		array_get(pfdepvector, i, &fdep);
		/* The lookup can't fail, because of the pre-check above. */
		dict_find_string(&fdep, "FID", &pfid);
		data.FDepVector[i] = r_ptr(pfid, gs_font);
	}
	pfont->data = data;
	return define_gs_font((gs_font *)pfont);
}
/* If a newly defined or scaled composite font had to scale */
/* any composite sub-fonts, adjust the parent font's FDepVector. */
/* This is called only if gs_type0_define/make_font */
/* actually changed the FDepVector. */
private int
ztype0_adjust_FDepVector(gs_font_type0 *pfont)
{	gs_font **pdep = pfont->data.FDepVector;
	ref newdep;
	uint fdep_size = pfont->data.fdep_size;
	ref *prdep;
	uint i;
	int code = ialloc_ref_array(&newdep, a_readonly, fdep_size,
				    "ztype0_adjust_matrix");
	if ( code < 0 )
		return code;
	for ( prdep = newdep.value.refs, i = 0; i < fdep_size; i++, prdep++ )
	{	const ref *pdict = pfont_dict(pdep[i]);
		ref_assign_new(prdep, pdict);
	}
	return dict_put_string(pfont_dict(pfont), "FDepVector", &newdep);
}
private int
ztype0_define_font(gs_font_dir *pdir, gs_font *pfont)
{
#define pfont0 ((gs_font_type0 *)pfont)
	gs_font **pdep = pfont0->data.FDepVector;
	int code = gs_type0_define_font(pdir, pfont);
	if ( code < 0 || pfont0->data.FDepVector == pdep )
		return code;
	return ztype0_adjust_FDepVector(pfont0);
#undef pfont0
}
private int
ztype0_make_font(gs_font_dir *pdir, const gs_font *pfont,
  const gs_matrix *pmat, gs_font **ppfont)
{
#define ppfont0 ((gs_font_type0 **)ppfont)
	gs_font **pdep = (*ppfont0)->data.FDepVector;
	int code;
	code = zdefault_make_font(pdir, pfont, pmat, ppfont);
	if ( code < 0 )
		return code;
	code = gs_type0_make_font(pdir, pfont, pmat, ppfont);
	if ( code < 0 )
		return code;
	if ( (*ppfont0)->data.FDepVector == pdep )
		return 0;
	return ztype0_adjust_FDepVector(*ppfont0);
#undef ppfont0
}

/* ------ Internal routines ------ */

/* Find or add a character entry in a font dictionary. */
private int
ensure_char_entry(os_ptr op, const char _ds *kstr,
  byte *pvalue, int default_value)
{	ref *pentry;
	if ( dict_find_string(op, kstr, &pentry) <= 0 )
	  {	ref ent;
		make_int(&ent, default_value);
		*pvalue = (byte)default_value;
		return dict_put_string(op, kstr, &ent);
	  }
	else
	  {	check_int_leu_only(*pentry, 255);
		*pvalue = (byte)pentry->value.intval;
		return 0;
	  }
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zfont0_op_defs) {
	{"2.buildfont0", zbuildfont0},
END_OP_DEFS(0) }
