/* Copyright (C) 1989, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zfont.c,v 1.2 2000/03/08 23:15:36 mike Exp $ */
/* Generic font operators */
#include "ghost.h"
#include "oper.h"
#include "gsstruct.h"		/* for registering root */
#include "gzstate.h"		/* must precede gxdevice */
#include "gxdevice.h"		/* must precede gxfont */
#include "gschar.h"
#include "gxfont.h"
#include "gxfcache.h"
#include "bfont.h"
#include "ialloc.h"
#include "idict.h"
#include "igstate.h"
#include "iname.h"		/* for name_mark_index */
#include "isave.h"
#include "store.h"
#include "ivmspace.h"

/* Forward references */
private int make_font(P2(os_ptr, const gs_matrix *));
private void make_uint_array(P3(os_ptr, const uint *, int));

/* The (global) font directory */
gs_font_dir *ifont_dir = 0;	/* needed for buildfont */

/* Mark a glyph as a PostScript name (if it isn't a CID). */
bool
zfont_mark_glyph_name(gs_glyph glyph, void *ignore_data)
{
    return (glyph >= gs_min_cid_glyph || glyph == gs_no_glyph ? false :
	    name_mark_index((uint) glyph));
}

/* Initialize the font operators */
private void
zfont_init(void)
{
    ifont_dir = gs_font_dir_alloc2(imemory, &gs_memory_default);
    ifont_dir->ccache.mark_glyph = zfont_mark_glyph_name;
    gs_register_struct_root(imemory, NULL, (void **)&ifont_dir,
			    "ifont_dir");
}

/* <font> <scale> scalefont <new_font> */
private int
zscalefont(register os_ptr op)
{
    int code;
    double scale;
    gs_matrix mat;

    if ((code = real_param(op, &scale)) < 0)
	return code;
    if ((code = gs_make_scaling(scale, scale, &mat)) < 0)
	return code;
    return make_font(op, &mat);
}

/* <font> <matrix> makefont <new_font> */
private int
zmakefont(register os_ptr op)
{
    int code;
    gs_matrix mat;

    if ((code = read_matrix(op, &mat)) < 0)
	return code;
    return make_font(op, &mat);
}

/* <font> setfont - */
int
zsetfont(register os_ptr op)
{
    gs_font *pfont;
    int code = font_param(op, &pfont);

    if (code < 0 || (code = gs_setfont(igs, pfont)) < 0)
	return code;
    pop(1);
    return code;
}

/* - currentfont <font> */
private int
zcurrentfont(register os_ptr op)
{
    push(1);
    *op = *pfont_dict(gs_currentfont(igs));
    return 0;
}

/* - cachestatus <mark> <bsize> <bmax> <msize> <mmax> <csize> <cmax> <blimit> */
private int
zcachestatus(register os_ptr op)
{
    uint status[7];

    gs_cachestatus(ifont_dir, status);
    push(7);
    make_uint_array(op - 6, status, 7);
    return 0;
}

/* <blimit> setcachelimit - */
private int
zsetcachelimit(register os_ptr op)
{
    check_int_leu(*op, max_uint);
    gs_setcachelimit(ifont_dir, (uint) op->value.intval);
    pop(1);
    return 0;
}

/* <mark> <size> <lower> <upper> setcacheparams - */
private int
zsetcacheparams(register os_ptr op)
{
    uint params[3];
    int i, code;
    os_ptr opp = op;

    for (i = 0; i < 3 && !r_has_type(opp, t_mark); i++, opp--) {
	check_int_leu(*opp, max_uint);
	params[i] = opp->value.intval;
    }
    switch (i) {
	case 3:
	    if ((code = gs_setcachesize(ifont_dir, params[2])) < 0)
		return code;
	case 2:
	    if ((code = gs_setcachelower(ifont_dir, params[1])) < 0)
		return code;
	case 1:
	    if ((code = gs_setcacheupper(ifont_dir, params[0])) < 0)
		return code;
	case 0:;
    }
    return zcleartomark(op);
}

/* - currentcacheparams <mark> <size> <lower> <upper> */
private int
zcurrentcacheparams(register os_ptr op)
{
    uint params[3];

    params[0] = gs_currentcachesize(ifont_dir);
    params[1] = gs_currentcachelower(ifont_dir);
    params[2] = gs_currentcacheupper(ifont_dir);
    push(4);
    make_mark(op - 3);
    make_uint_array(op - 2, params, 3);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zfont_op_defs[] =
{
    {"0currentfont", zcurrentfont},
    {"2makefont", zmakefont},
    {"2scalefont", zscalefont},
    {"1setfont", zsetfont},
    {"0cachestatus", zcachestatus},
    {"1setcachelimit", zsetcachelimit},
    {"1setcacheparams", zsetcacheparams},
    {"0currentcacheparams", zcurrentcacheparams},
    op_def_end(zfont_init)
};

/* ------ Subroutines ------ */

/* Validate a font parameter. */
int
font_param(const ref * pfdict, gs_font ** ppfont)
{	/*
	 * Check that pfdict is a read-only dictionary, that it has a FID
	 * entry whose value is a fontID, and that the fontID points to a
	 * gs_font structure whose associated PostScript dictionary is
	 * pfdict.
	 */
    ref *pid;
    gs_font *pfont;
    const font_data *pdata;

    check_type(*pfdict, t_dictionary);
    if (dict_find_string(pfdict, "FID", &pid) <= 0 ||
	!r_has_type(pid, t_fontID)
	)
	return_error(e_invalidfont);
    pfont = r_ptr(pid, gs_font);
    pdata = pfont->client_data;
    if (!obj_eq(&pdata->dict, pfdict))
	return_error(e_invalidfont);
    *ppfont = pfont;
    if (pfont == 0)
	return_error(e_invalidfont);	/* unregistered font */
    return 0;
}

/* Add the FID entry to a font dictionary. */
int
add_FID(ref * fp /* t_dictionary */ , gs_font * pfont)
{
    ref fid;

    make_tav_new(&fid, t_fontID, a_readonly | icurrent_space,
		 pstruct, (void *)pfont);
    return dict_put_string(fp, "FID", &fid);
}

/* Make a transformed font (common code for makefont/scalefont). */
private int
make_font(os_ptr op, const gs_matrix * pmat)
{
    os_ptr fp = op - 1;
    gs_font *oldfont, *newfont;
    int code;
    ref *pencoding = 0;

    code = font_param(fp, &oldfont);
    if (code < 0)
	return code;
    {
	uint space = ialloc_space(idmemory);

	ialloc_set_space(idmemory, r_space(fp));
	if (dict_find_string(fp, "Encoding", &pencoding) > 0 &&
	    !r_is_array(pencoding)
	    )
	    code = gs_note_error(e_invalidfont);
	else {
		/*
		 * Temporarily substitute the new dictionary
		 * for the old one, in case the Encoding changed.
		 */
	    ref olddict;

	    olddict = *pfont_dict(oldfont);
	    *pfont_dict(oldfont) = *fp;
	    code = gs_makefont(ifont_dir, oldfont, pmat, &newfont);
	    *pfont_dict(oldfont) = olddict;
	}
	ialloc_set_space(idmemory, space);
    }
    if (code < 0)
	return code;
    /*
     * We have to allow for the possibility that the font's Encoding
     * is different from that of the base font.  Note that the
     * font_data of the new font was simply copied from the old one.
     */
    if (pencoding != 0 &&
	!obj_eq(pencoding, &pfont_data(newfont)->Encoding)
	) {
	if (newfont->FontType == ft_composite)
	    return_error(e_rangecheck);
	/* We should really do validity checking here.... */
	ref_assign(&pfont_data(newfont)->Encoding, pencoding);
	lookup_gs_simple_font_encoding((gs_font_base *) newfont);
    }
    *fp = *pfont_dict(newfont);
    pop(1);
    return 0;
}
/* Create the transformed font dictionary. */
/* This is the make_font completion procedure for all non-composite fonts */
/* created at the interpreter level (see build_gs_simple_font in zfont2.c.) */
int
zbase_make_font(gs_font_dir * pdir, const gs_font * oldfont,
		const gs_matrix * pmat, gs_font ** ppfont)
{
    /*
     * We must call gs_base_make_font so that the XUID gets copied
     * if necessary.
     */
    int code = gs_base_make_font(pdir, oldfont, pmat, ppfont);

    if (code < 0)
	return code;
    return zdefault_make_font(pdir, oldfont, pmat, ppfont);
}
int
zdefault_make_font(gs_font_dir * pdir, const gs_font * oldfont,
		   const gs_matrix * pmat, gs_font ** ppfont)
{
    gs_font *newfont = *ppfont;
    ref *fp = pfont_dict(oldfont);
    font_data *pdata;
    ref newdict, newmat, scalemat;
    uint dlen = dict_maxlength(fp);
    uint mlen = dict_length(fp) + 3;	/* FontID, OrigFont, ScaleMatrix */
    int code;

    if (dlen < mlen)
	dlen = mlen;
    if ((pdata = ialloc_struct(font_data, &st_font_data,
			       "make_font(font_data)")) == 0
	)
	return_error(e_VMerror);
    if ((code = dict_create(dlen, &newdict)) < 0 ||
	(code = dict_copy(fp, &newdict)) < 0 ||
    (code = ialloc_ref_array(&newmat, a_all, 12, "make_font(matrices)")) < 0
	)
	return code;
    refset_null(newmat.value.refs, 12);
    ref_assign(&scalemat, &newmat);
    r_set_size(&scalemat, 6);
    scalemat.value.refs += 6;
    /*
     * Create the scaling matrix.  We could do this several different
     * ways: by "dividing" the new FontMatrix by the base FontMatrix, by
     * multiplying the current scaling matrix by a ScaleMatrix kept in
     * the gs_font, or by multiplying the current scaling matrix by the
     * ScaleMatrix from the font dictionary.  We opt for the last of
     * these.
     */
    {
	gs_matrix scale, prev_scale;
	ref *ppsm;

	if (!(dict_find_string(fp, "ScaleMatrix", &ppsm) > 0 &&
	      read_matrix(ppsm, &prev_scale) >= 0 &&
	      gs_matrix_multiply(pmat, &prev_scale, &scale) >= 0)
	    )
	    scale = *pmat;
	write_matrix(&scalemat, &scale);
    }
    r_clear_attrs(&scalemat, a_write);
    r_set_size(&newmat, 6);
    write_matrix(&newmat, &newfont->FontMatrix);
    r_clear_attrs(&newmat, a_write);
    if ((code = dict_put_string(&newdict, "FontMatrix", &newmat)) < 0 ||
	(code = dict_put_string(&newdict, "OrigFont", pfont_dict(oldfont->base))) < 0 ||
	(code = dict_put_string(&newdict, "ScaleMatrix", &scalemat)) < 0 ||
	(code = add_FID(&newdict, newfont)) < 0
	)
	return code;
    newfont->client_data = pdata;
    *pdata = *pfont_data(oldfont);
    pdata->dict = newdict;
    r_clear_attrs(dict_access_ref(&newdict), a_write);
    return 0;
}

/* Convert an array of (unsigned) integers to stack form. */
private void
make_uint_array(register os_ptr op, const uint * intp, int count)
{
    int i;

    for (i = 0; i < count; i++, op++, intp++)
	make_int(op, *intp);
}

/* Remove scaled font and character cache entries that would be */
/* invalidated by a restore. */
private bool
purge_if_name_removed(cached_char * cc, void *vsave)
{
    return alloc_name_index_is_since_save(cc->code, vsave);
}
void
font_restore(const alloc_save_t * save)
{
    gs_font_dir *pdir = ifont_dir;

    if (pdir == 0)		/* not initialized yet */
	return;

    /* Purge original (unscaled) fonts. */

    {
	gs_font *pfont;

otop:
	for (pfont = pdir->orig_fonts; pfont != 0;
	     pfont = pfont->next
	    ) {
	    if (alloc_is_since_save((char *)pfont, save)) {
		gs_purge_font(pfont);
		goto otop;
	    }
	}
    }

    /* Purge cached scaled fonts. */

    {
	gs_font *pfont;

top:
	for (pfont = pdir->scaled_fonts; pfont != 0;
	     pfont = pfont->next
	    ) {
	    if (alloc_is_since_save((char *)pfont, save)) {
		gs_purge_font(pfont);
		goto top;
	    }
	}
    }

    /* Purge xfonts and uncached scaled fonts. */

    {
	cached_fm_pair *pair;
	uint n;

	for (pair = pdir->fmcache.mdata, n = pdir->fmcache.mmax;
	     n > 0; pair++, n--
	    )
	    if (!fm_pair_is_free(pair)) {
		if ((uid_is_XUID(&pair->UID) &&
		     alloc_is_since_save((char *)pair->UID.xvalues,
					 save))
		    ) {
		    gs_purge_fm_pair(pdir, pair, 0);
		    continue;
		}
		if (pair->font != 0 &&
		    alloc_is_since_save((char *)pair->font, save)
		    ) {
		    if (!uid_is_valid(&pair->UID)) {
			gs_purge_fm_pair(pdir, pair, 0);
			continue;
		    }
		    /* Don't discard pairs with a surviving UID. */
		    pair->font = 0;
		}
		if (pair->xfont != 0 &&
		    alloc_is_since_save((char *)pair->xfont, save)
		    )
		    gs_purge_fm_pair(pdir, pair, 1);
	    }
    }

    /* Purge characters with names about to be removed. */
    /* We only need to do this if any new names have been created */
    /* since the save. */

    if (alloc_any_names_since_save(save))
	gx_purge_selected_cached_chars(pdir, purge_if_name_removed,
				       (void *)save);

}
