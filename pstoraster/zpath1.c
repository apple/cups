/* Copyright (C) 1989, 1995, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zpath1.c,v 1.2 2000/03/08 23:15:41 mike Exp $ */
/* PostScript Level 1 additional path operators */
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "estack.h"		/* for pathforall */
#include "ialloc.h"
#include "igstate.h"
#include "gsstruct.h"
#include "gspath.h"
#include "store.h"

/* Forward references */
private int common_arc(P2(os_ptr,
	  int (*)(P6(gs_state *, floatp, floatp, floatp, floatp, floatp))));
private int common_arct(P2(os_ptr, float *));

/* <x> <y> <r> <ang1> <ang2> arc - */
int
zarc(os_ptr op)
{
    return common_arc(op, gs_arc);
}

/* <x> <y> <r> <ang1> <ang2> arcn - */
int
zarcn(os_ptr op)
{
    return common_arc(op, gs_arcn);
}

/* Common code for arc[n] */
private int
common_arc(os_ptr op,
      int (*aproc)(P6(gs_state *, floatp, floatp, floatp, floatp, floatp)))
{
    double xyra[5];		/* x, y, r, ang1, ang2 */
    int code = num_params(op, 5, xyra);

    if (code < 0)
	return code;
    code = (*aproc)(igs, xyra[0], xyra[1], xyra[2], xyra[3], xyra[4]);
    if (code >= 0)
	pop(5);
    return code;
}

/* <x1> <y1> <x2> <y2> <r> arct - */
int
zarct(register os_ptr op)
{
    int code = common_arct(op, (float *)0);

    if (code < 0)
	return code;
    pop(5);
    return 0;
}

/* <x1> <y1> <x2> <y2> <r> arcto <xt1> <yt1> <xt2> <yt2> */
private int
zarcto(register os_ptr op)
{
    float tanxy[4];		/* xt1, yt1, xt2, yt2 */
    int code = common_arct(op, tanxy);

    if (code < 0)
	return code;
    make_real(op - 4, tanxy[0]);
    make_real(op - 3, tanxy[1]);
    make_real(op - 2, tanxy[2]);
    make_real(op - 1, tanxy[3]);
    pop(1);
    return 0;
}

/* Common code for arct[o] */
private int
common_arct(os_ptr op, float *tanxy)
{
    double args[5];		/* x1, y1, x2, y2, r */
    int code = num_params(op, 5, args);

    if (code < 0)
	return code;
    return gs_arcto(igs, args[0], args[1], args[2], args[3], args[4], tanxy);
}

/* - .dashpath - */
private int
zdashpath(register os_ptr op)
{
    return gs_dashpath(igs);
}

/* - flattenpath - */
private int
zflattenpath(register os_ptr op)
{
    return gs_flattenpath(igs);
}

/* - reversepath - */
private int
zreversepath(register os_ptr op)
{
    return gs_reversepath(igs);
}

/* - strokepath - */
private int
zstrokepath(register os_ptr op)
{
    return gs_strokepath(igs);
}

/* - clippath - */
private int
zclippath(register os_ptr op)
{
    return gs_clippath(igs);
}

/* <bool> .pathbbox <llx> <lly> <urx> <ury> */
private int
zpathbbox(register os_ptr op)
{
    gs_rect box;
    int code;

    check_type(*op, t_boolean);
    code = gs_upathbbox(igs, &box, op->value.boolval);
    if (code < 0)
	return code;
    push(3);
    make_real(op - 3, box.p.x);
    make_real(op - 2, box.p.y);
    make_real(op - 1, box.q.x);
    make_real(op, box.q.y);
    return 0;
}

/* <moveproc> <lineproc> <curveproc> <closeproc> pathforall - */
private int path_continue(P1(os_ptr));
private int path_cleanup(P1(os_ptr));
private int
zpathforall(register os_ptr op)
{
    gs_path_enum *penum;
    int code;

    check_proc(op[-3]);
    check_proc(op[-2]);
    check_proc(op[-1]);
    check_proc(*op);
    check_estack(8);
    if ((penum = gs_path_enum_alloc(imemory, "pathforall")) == 0)
	return_error(e_VMerror);
    code = gs_path_enum_init(penum, igs);
    if (code < 0) {
	ifree_object(penum, "path_cleanup");
	return code;
    }
    /* Push a mark, the four procedures, and the path enumerator. */
    push_mark_estack(es_for, path_cleanup);	/* iterator */
    memcpy(esp + 1, op - 3, 4 * sizeof(ref));	/* 4 procs */
    esp += 5;
    make_istruct(esp, 0, penum);
    push_op_estack(path_continue);
    pop(4);
    op -= 4;
    return o_push_estack;
}
/* Continuation procedure for pathforall */
private void pf_push(P3(gs_point *, int, os_ptr));
private int
path_continue(register os_ptr op)
{
    gs_path_enum *penum = r_ptr(esp, gs_path_enum);
    gs_point ppts[3];
    int code;

    /* Make sure we have room on the o-stack for the worst case */
    /* before we enumerate the next path element. */
    check_ostack(6);		/* 3 points for curveto */
    code = gs_path_enum_next(penum, ppts);
    switch (code) {
	case 0:		/* all done */
	    esp -= 6;
	    path_cleanup(op);
	    return o_pop_estack;
	default:		/* error */
	    return code;
	case gs_pe_moveto:
	    esp[2] = esp[-4];	/* moveto proc */
	    pf_push(ppts, 1, op);
	    break;
	case gs_pe_lineto:
	    esp[2] = esp[-3];	/* lineto proc */
	    pf_push(ppts, 1, op);
	    break;
	case gs_pe_curveto:
	    esp[2] = esp[-2];	/* curveto proc */
	    pf_push(ppts, 3, op);
	    break;
	case gs_pe_closepath:
	    esp[2] = esp[-1];	/* closepath proc */
	    break;
    }
    push_op_estack(path_continue);
    ++esp;			/* include pushed procedure */
    return o_push_estack;
}
/* Internal procedure to push one or more points */
private void
pf_push(gs_point * ppts, int n, os_ptr op)
{
    while (n--) {
	op += 2;
	make_real(op - 1, ppts->x);
	make_real(op, ppts->y);
	ppts++;
    }
    osp = op;
}
/* Clean up after a pathforall */
private int
path_cleanup(os_ptr op)
{
    gs_path_enum *penum = r_ptr(esp + 6, gs_path_enum);

    gs_path_enum_cleanup(penum);
    ifree_object(penum, "path_cleanup");
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zpath1_op_defs[] =
{
    {"5arc", zarc},
    {"5arcn", zarcn},
    {"5arct", zarct},
    {"5arcto", zarcto},
    {"0clippath", zclippath},
    {"0.dashpath", zdashpath},
    {"0flattenpath", zflattenpath},
    {"4pathforall", zpathforall},
    {"0reversepath", zreversepath},
    {"0strokepath", zstrokepath},
    {"0.pathbbox", zpathbbox},
		/* Internal operators */
    {"0%path_continue", path_continue},
    op_def_end(0)
};
