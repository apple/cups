/* Copyright (C) 1990, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zdps1.c,v 1.2 2000/03/08 23:15:34 mike Exp $ */
/* Level 2 / Display PostScript graphics extensions */
#include "ghost.h"
#include "oper.h"
#include "gsmatrix.h"
#include "gspath.h"
#include "gspath2.h"
#include "gsstate.h"
#include "ialloc.h"
#include "igstate.h"
#include "ivmspace.h"
#include "store.h"
#include "stream.h"
#include "ibnum.h"

/* Forward references */
private int gstate_unshare(P1(os_ptr));

/* Structure descriptors */
public_st_igstate_obj();

/* Extend the `copy' operator to deal with gstates. */
/* This is done with a hack -- we know that gstates are the only */
/* t_astruct subtype that implements copy. */
private int
z1copy(register os_ptr op)
{
    int code = zcopy(op);

    if (code >= 0)
	return code;
    if (!r_has_type(op, t_astruct))
	return code;
    return zcopy_gstate(op);
}

/* ------ Graphics state ------ */

/* <bool> setstrokeadjust - */
private int
zsetstrokeadjust(register os_ptr op)
{
    check_type(*op, t_boolean);
    gs_setstrokeadjust(igs, op->value.boolval);
    pop(1);
    return 0;
}

/* - currentstrokeadjust <bool> */
private int
zcurrentstrokeadjust(register os_ptr op)
{
    push(1);
    make_bool(op, gs_currentstrokeadjust(igs));
    return 0;
}

/* ------ Graphics state objects ------ */

/* Check to make sure that all the elements of a graphics state */
/* can be stored in the given allocation space. */
/****** DOESN'T CHECK THE NON-REFS. ****** */
private int
gstate_check_space(int_gstate * isp, uint space)
{
#define gsref_check(p) store_check_space(space, p)
    int_gstate_map_refs(isp, gsref_check);
#undef gsref_check
    return 0;
}

/* - gstate <gstate> */
int
zgstate(register os_ptr op)
{
    int code = gstate_check_space(istate, icurrent_space);
    igstate_obj *pigo;
    gs_state *pnew;
    int_gstate *isp;

    if (code < 0)
	return code;
    pigo = ialloc_struct(igstate_obj, &st_igstate_obj, "gstate");
    if (pigo == 0)
	return_error(e_VMerror);
    pnew = gs_state_copy(igs, imemory);
    if (pnew == 0) {
	ifree_object(pigo, "gstate");
	return_error(e_VMerror);
    }
    isp = gs_int_gstate(pnew);
    int_gstate_map_refs(isp, ref_mark_new);
    push(1);
    /*
     * Since igstate_obj isn't a ref, but only contains a ref, save won't
     * clear its l_new bit automatically, and restore won't set it
     * automatically; we have to make sure this ref is on the changes chain.
     */
    make_iastruct(op, a_all, pigo);
    make_null(&pigo->gstate);
    ref_save(op, &pigo->gstate, "gstate");
    make_istruct_new(&pigo->gstate, 0, pnew);
    return 0;
}

/* copy for gstates */
int
zcopy_gstate(register os_ptr op)
{
    os_ptr op1 = op - 1;
    gs_state *pgs;
    gs_state *pgs1;
    int_gstate *pistate;
    gs_memory_t *mem;
    int code;

    check_stype(*op, st_igstate_obj);
    check_stype(*op1, st_igstate_obj);
    check_write(*op);
    code = gstate_unshare(op);
    if (code < 0)
	return code;
    pgs = igstate_ptr(op);
    pgs1 = igstate_ptr(op1);
    pistate = gs_int_gstate(pgs);
    code = gstate_check_space(gs_int_gstate(pgs1), r_space(op));
    if (code < 0)
	return code;
#define gsref_save(p) ref_save(op, p, "copygstate")
    int_gstate_map_refs(pistate, gsref_save);
#undef gsref_save
    mem = gs_state_swap_memory(pgs, imemory);
    code = gs_copygstate(pgs, pgs1);
    gs_state_swap_memory(pgs, mem);
    if (code < 0)
	return code;
    int_gstate_map_refs(pistate, ref_mark_new);
    *op1 = *op;
    pop(1);
    return 0;
}

/* <gstate> currentgstate <gstate> */
int
zcurrentgstate(register os_ptr op)
{
    gs_state *pgs;
    int_gstate *pistate;
    int code;
    gs_memory_t *mem;

    check_stype(*op, st_igstate_obj);
    check_write(*op);
    code = gstate_unshare(op);
    if (code < 0)
	return code;
    pgs = igstate_ptr(op);
    pistate = gs_int_gstate(pgs);
    code = gstate_check_space(istate, r_space(op));
    if (code < 0)
	return code;
#define gsref_save(p) ref_save(op, p, "currentgstate")
    int_gstate_map_refs(pistate, gsref_save);
#undef gsref_save
    mem = gs_state_swap_memory(pgs, imemory);
    code = gs_currentgstate(pgs, igs);
    gs_state_swap_memory(pgs, mem);
    if (code < 0)
	return code;
    int_gstate_map_refs(pistate, ref_mark_new);
    return 0;
}

/* <gstate> setgstate - */
int
zsetgstate(register os_ptr op)
{
    int code;

    check_stype(*op, st_igstate_obj);
    check_read(*op);
    code = gs_setgstate(igs, igstate_ptr(op));
    if (code < 0)
	return code;
    pop(1);
    return 0;
}

/* ------ Rectangles ------- */

/*
 * We preallocate a short list for rectangles, because
 * the rectangle operators usually will involve very few rectangles.
 */
#define MAX_LOCAL_RECTS 5
typedef struct local_rects_s {
    gs_rect *pr;
    uint count;
    gs_rect rl[MAX_LOCAL_RECTS];
} local_rects_t;

/* Forward references */
private int rect_get(P2(local_rects_t *, os_ptr));
private void rect_release(P1(local_rects_t *));

/* <x> <y> <width> <height> .rectappend - */
/* <numarray|numstring> .rectappend - */
private int
zrectappend(os_ptr op)
{
    local_rects_t lr;
    int npop = rect_get(&lr, op);
    int code;

    if (npop < 0)
	return npop;
    code = gs_rectappend(igs, lr.pr, lr.count);
    rect_release(&lr);
    if (code < 0)
	return code;
    pop(npop);
    return 0;
}

/* <x> <y> <width> <height> rectclip - */
/* <numarray|numstring> rectclip - */
private int
zrectclip(os_ptr op)
{
    local_rects_t lr;
    int npop = rect_get(&lr, op);
    int code;

    if (npop < 0)
	return npop;
    code = gs_rectclip(igs, lr.pr, lr.count);
    rect_release(&lr);
    if (code < 0)
	return code;
    pop(npop);
    return 0;
}

/* <x> <y> <width> <height> rectfill - */
/* <numarray|numstring> rectfill - */
private int
zrectfill(os_ptr op)
{
    local_rects_t lr;
    int npop = rect_get(&lr, op);
    int code;

    if (npop < 0)
	return npop;
    code = gs_rectfill(igs, lr.pr, lr.count);
    rect_release(&lr);
    if (code < 0)
	return code;
    pop(npop);
    return 0;
}

/* <x> <y> <width> <height> rectstroke - */
/* <numarray|numstring> rectstroke - */
private int
zrectstroke(os_ptr op)
{
    gs_matrix mat;
    local_rects_t lr;
    int npop, code;

    if (read_matrix(op, &mat) >= 0) {
	/* Concatenate the matrix to the CTM just before stroking the path. */
	npop = rect_get(&lr, op - 1);
	if (npop < 0)
	    return npop;
	code = gs_rectstroke(igs, lr.pr, lr.count, &mat);
	npop++;
    } else {
	/* No matrix. */
	npop = rect_get(&lr, op);
	if (npop < 0)
	    return npop;
	code = gs_rectstroke(igs, lr.pr, lr.count, (gs_matrix *) 0);
    }
    rect_release(&lr);
    if (code < 0)
	return code;
    pop(npop);
    return 0;
}

/* --- Internal routines --- */

/* Get rectangles from the stack. */
/* Return the number of elements to pop (>0) if OK, <0 if error. */
private int
rect_get(local_rects_t * plr, os_ptr op)
{
    int format, code;
    uint n, count;
    gs_rect *pr;
    double rv[4];

    switch (r_type(op)) {
	case t_array:
	case t_mixedarray:
	case t_shortarray:
	case t_string:
	    code = num_array_format(op);
	    if (code < 0)
		return code;
	    format = code;
	    count = num_array_size(op, format);
	    if (count % 4)
		return_error(e_rangecheck);
	    count /= 4;
	    break;
	default:		/* better be 4 numbers */
	    code = num_params(op, 4, rv);
	    if (code < 0)
		return code;
	    plr->pr = plr->rl;
	    plr->count = 1;
	    plr->rl[0].q.x = (plr->rl[0].p.x = rv[0]) + rv[2];
	    plr->rl[0].q.y = (plr->rl[0].p.y = rv[1]) + rv[3];
	    return 4;
    }
    plr->count = count;
    if (count <= MAX_LOCAL_RECTS)
	pr = plr->rl;
    else {
	pr = (gs_rect *) ialloc_byte_array(count, sizeof(gs_rect),
					   "rect_get");
	if (pr == 0)
	    return_error(e_VMerror);
    }
    plr->pr = pr;
    for (n = 0; n < count; n++, pr++) {
	ref rnum;
	int i;

	for (i = 0; i < 4; i++) {
	    code = num_array_get((const ref *)op, format,
				 (n << 2) + i, &rnum);
	    switch (code) {
		case t_integer:
		    rv[i] = rnum.value.intval;
		    break;
		case t_real:
		    rv[i] = rnum.value.realval;
		    break;
		default:	/* code < 0 */
		    return code;
	    }
	}
	pr->q.x = (pr->p.x = rv[0]) + rv[2];
	pr->q.y = (pr->p.y = rv[1]) + rv[3];
    }
    return 1;
}

/* Release the rectangle list if needed. */
private void
rect_release(local_rects_t * plr)
{
    if (plr->pr != plr->rl)
	ifree_object(plr->pr, "rect_release");
}

/* ------ Graphics state ------ */

/* <llx> <lly> <urx> <ury> setbbox - */
int
zsetbbox(register os_ptr op)
{
    double box[4];

    int code = num_params(op, 4, box);

    if (code < 0)
	return code;
    if ((code = gs_setbbox(igs, box[0], box[1], box[2], box[3])) < 0)
	return code;
    pop(4);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zdps1_l2_op_defs[] =
{
    op_def_begin_level2(),
		/* Graphics state */
    {"0currentstrokeadjust", zcurrentstrokeadjust},
    {"1setstrokeadjust", zsetstrokeadjust},
		/* Graphics state objects */
    {"1copy", z1copy},
    {"1currentgstate", zcurrentgstate},
    {"0gstate", zgstate},
    {"1setgstate", zsetgstate},
		/* Rectangles */
    {"1.rectappend", zrectappend},
    {"1rectclip", zrectclip},
    {"1rectfill", zrectfill},
    {"1rectstroke", zrectstroke},
		/* Graphics state components */
    {"4setbbox", zsetbbox},
    op_def_end(0)
};

/* ------ Internal routines ------ */

/* Ensure that a gstate is not shared with an outer save level. */
/* *op is of type t_astruct(igstate_obj). */
private int
gstate_unshare(os_ptr op)
{
    ref *pgsref = &r_ptr(op, igstate_obj)->gstate;
    gs_state *pgs = r_ptr(pgsref, gs_state);
    gs_state *pnew;
    int_gstate *isp;

    if (!ref_must_save(pgsref))
	return 0;
    /* Copy the gstate. */
    pnew = gs_gstate(pgs);
    if (pnew == 0)
	return_error(e_VMerror);
    isp = gs_int_gstate(pnew);
    int_gstate_map_refs(isp, ref_mark_new);
    ref_do_save(op, pgsref, "gstate_unshare");
    make_istruct_new(pgsref, 0, pnew);
    return 0;
}
