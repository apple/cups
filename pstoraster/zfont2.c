/* Copyright (C) 1989, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zfont2.c */
/* Font creation utilities */
#include "memory_.h"
#include "string_.h"
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "gxfixed.h"
#include "gsmatrix.h"
#include "gxdevice.h"
#include "gschar.h"
#include "gxfont.h"
#include "bfont.h"
#include "ialloc.h"
#include "idict.h"
#include "idparam.h"
#include "ilevel.h"
#include "iname.h"
#include "ipacked.h"
#include "istruct.h"
#include "store.h"

/* Registered encodings.  See ifont.h for documentation. */
ref registered_Encodings;
private ref *registered_Encodings_p = &registered_Encodings;
private gs_gc_root_t registered_Encodings_root;

/* Structure descriptor */
public_st_font_data();

/* Initialize the font building operators */
private void
zfont2_init(void)
{	/* Initialize the registered Encodings. */
	{	int i;
		ialloc_ref_array(&registered_Encodings, a_readonly,
				 registered_Encodings_countof,
				 "registered_Encodings");
		for ( i = 0; i < registered_Encodings_countof; i++ )
		  make_empty_array(&registered_Encoding(i), 0);
	}
	gs_register_ref_root(imemory, &registered_Encodings_root,
			     (void **)&registered_Encodings_p,
			     "registered_Encodings");
}

/* <string|name> <font_dict> .buildfont3 <string|name> <font> */
/* Build a type 3 (user-defined) font. */
private int
zbuildfont3(os_ptr op)
{	int code;
	build_proc_refs build;
	gs_font_base *pfont;

	check_type(*op, t_dictionary);
	code = build_gs_font_procs(op, &build);
	if ( code < 0 )
	  return code;
	code = build_gs_simple_font(op, &pfont, ft_user_defined,
				    &st_gs_font_base, &build);
	if ( code < 0 )
	  return code;
	return define_gs_font((gs_font *)pfont);
}

/* <int> <array|shortarray> .registerencoding - */
private int
zregisterencoding(register os_ptr op)
{	long i;
	if ( !r_is_array(op) )
	  return_op_typecheck(op);
	check_read(*op);
	check_type(op[-1], t_integer);
	for ( i = r_size(op); i > 0; )
	  {	ref cname;
		array_get(op, --i, &cname);
		check_type_only(cname, t_name);
	  }
	i = op[-1].value.intval;
	if ( i >= 0 && i < registered_Encodings_countof )
	{	ref *penc = &registered_Encoding(i);
		ref_assign_old(&registered_Encodings, penc, op,
			       ".registerencoding");
	}
	pop(2);
	return 0;
}

/* Encode a character. */
/* (This is very inefficient right now; we can speed it up later.) */
private gs_glyph
zfont_encode_char(gs_show_enum *penum, gs_font *pfont, gs_char *pchr)
{	const ref *pencoding = &pfont_data(pfont)->Encoding;
	ulong index = *pchr;	/* work around VAX widening bug */
	ref cname;
	int code = array_get(pencoding, (long)index, &cname);
	if ( code < 0 || !r_has_type(&cname, t_name) )
	  return gs_no_glyph;
	return (gs_glyph)name_index(&cname);
}

/* Encode a character in a known encoding. */
private gs_glyph
zfont_known_encode(gs_char chr, int encoding_index)
{	ulong index = chr;	/* work around VAX widening bug */
	ref cname;
	int code;
	if ( encoding_index < 0 )
	  return gs_no_glyph;
	code = array_get(&registered_Encoding(encoding_index),
			 (long)index, &cname);
	if ( code < 0 || !r_has_type(&cname, t_name) )
	  return gs_no_glyph;
	return (gs_glyph)name_index(&cname);
}

/* Get the name of a glyph. */
/* The following typedef is needed to work around a bug in */
/* some AIX C compilers. */
typedef const char *const_chars;
private const_chars
zfont_glyph_name(gs_glyph index, uint *plen)
{	ref nref, sref;
	name_index_ref(index, &nref);
	name_string_ref(&nref, &sref);
	*plen = r_size(&sref);
	return (const char *)sref.value.const_bytes;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zfont2_op_defs) {
	{"2.buildfont3", zbuildfont3},
	{"2.registerencoding", zregisterencoding},
END_OP_DEFS(zfont2_init) }

/* ------ Subroutines ------ */

/* Convert strings to executable names for build_proc_refs. */
int
build_proc_name_refs(build_proc_refs *pbuild,
  const char _ds *bcstr, const char _ds *bgstr)
{	int code;
	if ( (code = name_ref((const byte *)bcstr, strlen(bcstr), &pbuild->BuildChar, 0)) < 0 ||
	     (code = name_ref((const byte *)bgstr, strlen(bgstr), &pbuild->BuildGlyph, 0)) < 0
	   )
		return code;
	r_set_attrs(&pbuild->BuildChar, a_executable);
	r_set_attrs(&pbuild->BuildGlyph, a_executable);
	return 0;
}

/* Get the BuildChar and/or BuildGlyph routines from a (base) font. */
int
build_gs_font_procs(os_ptr op, build_proc_refs *pbuild)
{	int ccode, gcode;
	ref *pBuildChar;
	ref *pBuildGlyph;

	check_type(*op, t_dictionary);
	ccode = dict_find_string(op, "BuildChar", &pBuildChar);
	gcode = dict_find_string(op, "BuildGlyph", &pBuildGlyph);
	if ( ccode <= 0 )
	{	if ( gcode <= 0 )
		  return_error(e_invalidfont);
		make_null(&pbuild->BuildChar);
	}
	else
	{	check_proc(*pBuildChar);
		pbuild->BuildChar = *pBuildChar;
	}
	if ( gcode <= 0 )
	  make_null(&pbuild->BuildGlyph);
	else
	{	check_proc(*pBuildGlyph);
		pbuild->BuildGlyph = *pBuildGlyph;
	}
	return 0;
}

/* Do the common work for building a primitive font -- one whose execution */
/* algorithm is implemented in C (Type 1, Type 4, or Type 42). */
/* The caller guarantees that *op is a dictionary. */
int
build_gs_primitive_font(os_ptr op, gs_font_base **ppfont, font_type ftype,
  gs_memory_type_ptr_t pstype, const build_proc_refs *pbuild)
{	int painttype;
	float strokewidth;
	ref *pcharstrings;
	gs_font_base *pfont;
	font_data *pdata;
	int code;

	code = dict_int_param(op, "PaintType", 0, 3, 0, &painttype);
	if ( code < 0 )
	  return code;
	code = dict_float_param(op, "StrokeWidth", 0.0, &strokewidth);
	if ( code < 0 )
	  return code;
	if ( dict_find_string(op, "CharStrings", &pcharstrings) <= 0 ||
	    !r_has_type(pcharstrings, t_dictionary)
	   )
	  return_error(e_invalidfont);
	code = build_gs_simple_font(op, &pfont, ftype, pstype, pbuild);
	if ( code != 0 )
	  return code;
	pfont->PaintType = painttype;
	pfont->StrokeWidth = strokewidth;
	pdata = pfont_data(pfont);
	ref_assign(&pdata->CharStrings, pcharstrings);
	/* Check that the UniqueIDs match.  This is part of the */
	/* Adobe protection scheme, but we may as well emulate it. */
	if ( uid_is_valid(&pfont->UID) &&
	     !dict_check_uid_param(op, &pfont->UID)
	   )
	  uid_set_invalid(&pfont->UID);
	*ppfont = pfont;
	return 0;
}

/* Do the common work for building a font of any non-composite FontType. */
/* The caller guarantees that *op is a dictionary. */
int
build_gs_simple_font(os_ptr op, gs_font_base **ppfont, font_type ftype,
  gs_memory_type_ptr_t pstype, const build_proc_refs *pbuild)
{	float bbox[4];
	gs_uid uid;
	int code;
	gs_font_base *pfont;

	code = font_bbox_param(op, bbox);
	if ( code < 0 ) return code;
	code = dict_uid_param(op, &uid, 0, imemory);
	if ( code < 0 ) return code;
	code = build_gs_font(op, (gs_font **)ppfont, ftype, pstype, pbuild);
	if ( code != 0 ) return code;	/* invalid or scaled font */
	pfont = *ppfont;
	pfont->procs.init_fstack = gs_default_init_fstack;
	pfont->procs.next_char = gs_default_next_char;
	pfont->procs.define_font = gs_no_define_font;
	pfont->procs.make_font = zbase_make_font;
	pfont->FontBBox.p.x = bbox[0];
	pfont->FontBBox.p.y = bbox[1];
	pfont->FontBBox.q.x = bbox[2];
	pfont->FontBBox.q.y = bbox[3];
	pfont->UID = uid;
	lookup_gs_simple_font_encoding(pfont);
	return 0;
}

/* Compare the encoding of a simple font with the registered encodings. */
void
lookup_gs_simple_font_encoding(gs_font_base *pfont)
{	const ref *pfe = &pfont_data(pfont)->Encoding;
	int index;
	for ( index = registered_Encodings_countof; --index >= 0; )
	  if ( obj_eq(pfe, &registered_Encoding(index)) )
	    break;
	pfont->encoding_index = index;
	if ( index < 0 )
	{	/* Look for an encoding that's "close". */
		int near_index = -1;
		uint esize = r_size(pfe);
		uint best = esize / 3;	/* must match at least this many */
		for ( index = registered_Encodings_countof; --index >= 0; )
		{	const ref *pre = &registered_Encoding(index);
			bool r_packed = r_has_type(pre, t_shortarray);
			bool f_packed = !r_has_type(pfe, t_array);
			uint match = esize;
			int i;
			ref fchar, rchar;
			const ref *pfchar = &fchar;
			if ( r_size(pre) != esize )
			  continue;
			for ( i = esize; --i >= 0; )
			{	uint rnidx;
				if ( r_packed )
				  rnidx = packed_name_index(pre->value.packed + i);
				else
				  { array_get(pre, (long)i, &rchar);
				    rnidx = name_index(&rchar);
				  }
				if ( f_packed )
				  array_get(pfe, (long)i, &fchar);
				else
				  pfchar = pfe->value.const_refs + i;
				if ( !r_has_type(pfchar, t_name) ||
				     name_index(pfchar) != rnidx
				   )
				  if ( --match <= best )
				    break;
			}
			if ( match > best )
			  best = match,
			  near_index = index;
		}
		index = near_index;
	}
	pfont->nearest_encoding_index = index;
}

/* Do the common work for building a font of any FontType. */
/* The caller guarantees that *op is a dictionary. */
/* op[-1] must be the key under which the font is being registered */
/* in FontDirectory, normally a name or string. */
/* Return 0 for a new font, 1 for a font made by makefont or scalefont, */
/* or a negative error code. */
private void get_font_name(P2(ref *, const ref *));
private void copy_font_name(P2(gs_font_name *, const ref *));
int
build_gs_font(os_ptr op, gs_font **ppfont, font_type ftype,
  gs_memory_type_ptr_t pstype, const build_proc_refs *pbuild)
{	ref kname, fname;		/* t_string */
	ref *pftype;
	ref *pfontname;
	ref *pmatrix;
	gs_matrix mat;
	ref *pencoding;
	bool bitmapwidths;
	int exactsize, inbetweensize, transformedchar;
	int wmode;
	int code;
	gs_font *pfont;
	ref *pfid;
	ref *aop = dict_access_ref(op);
	get_font_name(&kname, op - 1);
	if ( dict_find_string(op, "FontType", &pftype) <= 0 ||
	    !r_has_type(pftype, t_integer) ||
	    pftype->value.intval != (int)ftype ||
	    dict_find_string(op, "FontMatrix", &pmatrix) <= 0 ||
	    read_matrix(pmatrix, &mat) < 0 ||
	    dict_find_string(op, "Encoding", &pencoding) <= 0 ||
	    !r_is_array(pencoding)
	   )
		return_error(e_invalidfont);
	if ( dict_find_string(op, "FontName", &pfontname) > 0 )
		get_font_name(&fname, pfontname);
	else
		make_empty_string(&fname, a_readonly);
	if ( (code = dict_int_param(op, "WMode", 0, 1, 0, &wmode)) < 0 ||
	     (code = dict_bool_param(op, "BitmapWidths", false, &bitmapwidths)) < 0 ||
	     (code = dict_int_param(op, "ExactSize", 0, 2, fbit_use_bitmaps, &exactsize)) < 0 ||
	     (code = dict_int_param(op, "InBetweenSize", 0, 2, fbit_use_outlines, &inbetweensize)) < 0 ||
	     (code = dict_int_param(op, "TransformedChar", 0, 2, fbit_use_outlines, &transformedchar)) < 0
	   )
		return code;
	code = dict_find_string(op, "FID", &pfid);
	if ( code > 0 )
	{	if ( !r_has_type(pfid, t_fontID) )
		  return_error(e_invalidfont);
		/*
		 * If this font has a FID entry already, it might be
		 * a scaled font made by makefont or scalefont;
		 * in a Level 2 environment, it might be an existing font
		 * being registered under a second name, or a re-encoded
		 * font (which is questionable PostScript, but dvips
		 * is known to do this).
		 */
		pfont = r_ptr(pfid, gs_font);
		if ( pfont->base == pfont )	/* original font */
		  {	if ( !level2_enabled )
			  return_error(e_invalidfont);
			if ( obj_eq(pfont_dict(pfont), op) )
			  {	*ppfont = pfont;
				return 0;
			  }
			/*
			 * This is a re-encoded font, or some other
			 * questionable situation in which the FID
			 * was preserved.  Pretend the FID wasn't there.
			 */
		  }
		else
		  {	/* This was made by makefont or scalefont. */
			/* Just insert the new name. */
			code = 1;
			goto set_name;
		  }
	}
	/* This is a new font. */
	if ( !r_has_attr(aop, a_write) )
	  return_error(e_invalidaccess);
	{	font_data *pdata;
		/* Make sure that we allocate the font data */
		/* in the same VM as the font dictionary. */
		uint space = ialloc_space(idmemory);
		ialloc_set_space(idmemory, r_space(op));
		pfont = ialloc_struct(gs_font, pstype,
				      "buildfont(font)");
		pdata = ialloc_struct(font_data, &st_font_data,
				      "buildfont(data)");
		if ( pfont == 0 || pdata == 0 )
		  code = gs_note_error(e_VMerror);
		else
		  code = add_FID(op, pfont);
		if ( code < 0 )
		  {	ifree_object(pdata, "buildfont(data)");
			ifree_object(pfont, "buildfont(font)");
			ialloc_set_space(idmemory, space);
			return code;
		  }
		refset_null((ref *)pdata, sizeof(font_data) / sizeof(ref));
		ref_assign_new(&pdata->dict, op);
		ref_assign_new(&pdata->BuildChar, &pbuild->BuildChar);
		ref_assign_new(&pdata->BuildGlyph, &pbuild->BuildGlyph);
		ref_assign_new(&pdata->Encoding, pencoding);
		pfont->base = pfont;
		pfont->memory = imemory;
		pfont->dir = 0;
		pfont->client_data = pdata;
		pfont->FontType = ftype;
		pfont->FontMatrix = mat;
		pfont->BitmapWidths = bitmapwidths;
		pfont->ExactSize = exactsize;
		pfont->InBetweenSize = inbetweensize;
		pfont->TransformedChar = transformedchar;
		pfont->WMode = wmode;
		pfont->PaintType = 0;
		pfont->StrokeWidth = 0.0;
		pfont->procs.build_char = gs_no_build_char;
		pfont->procs.encode_char = zfont_encode_char;
		pfont->procs.callbacks.glyph_name = zfont_glyph_name;
		pfont->procs.callbacks.known_encode = zfont_known_encode;
		ialloc_set_space(idmemory, space);
	}
	code = 0;
set_name:
	copy_font_name(&pfont->key_name, &kname);
	copy_font_name(&pfont->font_name, &fname);
	*ppfont = pfont;
	return code;
}

/* Get the string corresponding to a font name. */
/* If the font name isn't a name or a string, return an empty string. */
private void
get_font_name(ref *pfname, const ref *op)
{	switch ( r_type(op) )
	{
	case t_string:
		*pfname = *op;
		break;
	case t_name:
		name_string_ref(op, pfname);
		break;
	default:
		/* This is weird, but legal.... */
		make_empty_string(pfname, a_readonly);
	}
}

/* Copy a font name into the gs_font structure. */
private void
copy_font_name(gs_font_name *pfstr, const ref *pfname)
{	uint size = r_size(pfname);
	if ( size > gs_font_name_max )
		size = gs_font_name_max;
	memcpy(&pfstr->chars[0], pfname->value.const_bytes, size);
	/* Following is only for debugging printout. */
	pfstr->chars[size] = 0;
	pfstr->size = size;
}

/* Finish building a font, by calling gs_definefont if needed. */
int
define_gs_font(gs_font *pfont)
{	return (pfont->base == pfont && pfont->dir == 0 ?	/* i.e., unregistered original font */
		gs_definefont(ifont_dir, pfont) :
		0);
}
