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

/*$Id: zupath.c,v 1.2 2000/03/08 23:15:43 mike Exp $ */
/* Operators related to user paths */
#include "ghost.h"
#include "oper.h"
#include "idict.h"
#include "dstack.h"
#include "igstate.h"
#include "iname.h"
#include "iutil.h"
#include "store.h"
#include "stream.h"
#include "ibnum.h"
#include "gsmatrix.h"
#include "gsstate.h"
#include "gscoord.h"
#include "gspaint.h"
#include "gxfixed.h"
#include "gxdevice.h"
#include "gspath.h"
#include "gzpath.h"		/* for saving path */
#include "gzstate.h"		/* for accessing path */

/* Imported data */
extern const gx_device gs_hit_device;
extern const int gs_hit_detected;

/* Forward references */
private int upath_append(P2(os_ptr, os_ptr));
private int upath_stroke(P2(os_ptr, gs_matrix *));

/* ---------------- Insideness testing ---------------- */

/* Forward references */
private int in_test(P2(os_ptr, int (*)(P1(gs_state *))));
private int in_path(P3(os_ptr, os_ptr, gx_device *));
private int in_path_result(P3(os_ptr, int, int));
private int in_utest(P2(os_ptr, int (*)(P1(gs_state *))));
private int in_upath(P2(os_ptr, gx_device *));
private int in_upath_result(P3(os_ptr, int, int));

/* <x> <y> ineofill <bool> */
/* <userpath> ineofill <bool> */
private int
zineofill(os_ptr op)
{
    return in_test(op, gs_eofill);
}

/* <x> <y> infill <bool> */
/* <userpath> infill <bool> */
private int
zinfill(os_ptr op)
{
    return in_test(op, gs_fill);
}

/* <x> <y> instroke <bool> */
/* <userpath> instroke <bool> */
private int
zinstroke(os_ptr op)
{
    return in_test(op, gs_stroke);
}

/* <x> <y> <userpath> inueofill <bool> */
/* <userpath1> <userpath2> inueofill <bool> */
private int
zinueofill(os_ptr op)
{
    return in_utest(op, gs_eofill);
}

/* <x> <y> <userpath> inufill <bool> */
/* <userpath1> <userpath2> inufill <bool> */
private int
zinufill(os_ptr op)
{
    return in_utest(op, gs_fill);
}

/* <x> <y> <userpath> inustroke <bool> */
/* <x> <y> <userpath> <matrix> inustroke <bool> */
/* <userpath1> <userpath2> inustroke <bool> */
/* <userpath1> <userpath2> <matrix> inustroke <bool> */
private int
zinustroke(os_ptr op)
{	/* This is different because of the optional matrix operand. */
    int code = gs_gsave(igs);
    int spop, npop;
    gs_matrix mat;
    gx_device hdev;

    if (code < 0)
	return code;
    if ((spop = upath_stroke(op, &mat)) < 0) {
	gs_grestore(igs);
	return spop;
    }
    if ((npop = in_path(op - spop, op, &hdev)) < 0) {
	gs_grestore(igs);
	return npop;
    }
    if (npop > 1)		/* matrix was supplied */
	code = gs_concat(igs, &mat);
    if (code >= 0)
	code = gs_stroke(igs);
    return in_upath_result(op, npop + spop, code);
}

/* ------ Internal routines ------ */

/* Do the work of the non-user-path insideness operators. */
private int
in_test(os_ptr op, int (*paintproc)(P1(gs_state *)))
{
    gx_device hdev;
    int npop = in_path(op, op, &hdev);
    int code;

    if (npop < 0)
	return npop;
    code = (*paintproc)(igs);
    return in_path_result(op, npop, code);
}

/* Set up a clipping path and device for insideness testing. */
private int
in_path(os_ptr oppath, os_ptr op, gx_device * phdev)
{
    int code = gs_gsave(igs);
    int npop;
    double uxy[2];

    if (code < 0)
	return code;
    code = num_params(oppath, 2, uxy);
    if (code >= 0) {		/* Aperture is a single pixel. */
	gs_point dxy;
	gs_fixed_rect fr;

	gs_transform(igs, uxy[0], uxy[1], &dxy);
	fr.p.x = fixed_floor(float2fixed(dxy.x));
	fr.p.y = fixed_floor(float2fixed(dxy.y));
	fr.q.x = fr.p.x + fixed_1;
	fr.q.y = fr.p.y + fixed_1;
	code = gx_clip_to_rectangle(igs, &fr);
	npop = 2;
    } else {			/* Aperture is a user path. */
	/* We have to set the clipping path without disturbing */
	/* the current path. */
	gx_path *ipath = igs->path;
	gx_path save;

	gx_path_init_local(&save, imemory);
	gx_path_assign_preserve(&save, ipath);
	gs_newpath(igs);
	code = upath_append(oppath, op);
	if (code >= 0)
	    code = gx_clip_to_path(igs);
	gx_path_assign_free(igs->path, &save);
	npop = 1;
    }
    if (code < 0) {
	gs_grestore(igs);
	return code;
    }
    /* Install the hit detection device. */
    gx_set_device_color_1(igs);
    gx_device_init((gx_device *) phdev, (const gx_device *)&gs_hit_device,
		   NULL, true);
    phdev->width = phdev->height = max_int;
    gx_device_fill_in_procs(phdev);
    gx_set_device_only(igs, phdev);
    return npop;
}

/* Finish an insideness test. */
private int
in_path_result(os_ptr op, int npop, int code)
{
    bool result;

    gs_grestore(igs);		/* matches gsave in in_path */
    if (code == gs_hit_detected)
	result = true;
    else if (code == 0)		/* completed painting without a hit */
	result = false;
    else			/* error */
	return code;
    npop--;
    pop(npop);
    op -= npop;
    make_bool(op, result);
    return 0;

}

/* Do the work of the user-path insideness operators. */
private int
in_utest(os_ptr op, int (*paintproc)(P1(gs_state *)))
{
    gx_device hdev;
    int npop = in_upath(op, &hdev);
    int code;

    if (npop < 0)
	return npop;
    code = (*paintproc)(igs);
    return in_upath_result(op, npop, code);
}

/* Set up a clipping path and device for insideness testing */
/* with a user path. */
private int
in_upath(os_ptr op, gx_device * phdev)
{
    int code = gs_gsave(igs);
    int npop;

    if (code < 0)
	return code;
    if ((code = upath_append(op, op)) < 0 ||
	(npop = in_path(op - 1, op, phdev)) < 0
	) {
	gs_grestore(igs);
	return code;
    }
    return npop + 1;
}

/* Finish an insideness test with a user path. */
private int
in_upath_result(os_ptr op, int npop, int code)
{
    gs_grestore(igs);		/* matches gsave in in_upath */
    return in_path_result(op, npop, code);
}

/* ---------------- User paths ---------------- */

/* User path operator codes */
typedef enum {
    upath_op_setbbox = 0,
    upath_op_moveto = 1,
    upath_op_rmoveto = 2,
    upath_op_lineto = 3,
    upath_op_rlineto = 4,
    upath_op_curveto = 5,
    upath_op_rcurveto = 6,
    upath_op_arc = 7,
    upath_op_arcn = 8,
    upath_op_arct = 9,
    upath_op_closepath = 10,
    upath_op_ucache = 11
} upath_op;

#define UPATH_MAX_OP 11
#define UPATH_REPEAT 32
static const byte up_nargs[UPATH_MAX_OP + 1] = {
    4, 2, 2, 2, 2, 6, 6, 5, 5, 5, 0, 0
};

/* Declare operator procedures not declared in opextern.h. */
int zsetbbox(P1(os_ptr));
int zarc(P1(os_ptr));
int zarcn(P1(os_ptr));
int zarct(P1(os_ptr));
private int zucache(P1(os_ptr));

#undef zp
static const op_proc_p up_ops[UPATH_MAX_OP + 1] = {
    zsetbbox, zmoveto, zrmoveto, zlineto, zrlineto,
    zcurveto, zrcurveto, zarc, zarcn, zarct,
    zclosepath, zucache
};

/* - ucache - */
private int
zucache(os_ptr op)
{				/* A no-op for now. */
    return 0;
}

/* <userpath> uappend - */
private int
zuappend(register os_ptr op)
{
    int code = gs_gsave(igs);

    if (code < 0)
	return code;
    if ((code = upath_append(op, op)) >= 0)
	code = gs_upmergepath(igs);
    gs_grestore(igs);
    if (code < 0)
	return code;
    pop(1);
    return 0;
}

/* <userpath> ueofill - */
private int
zueofill(register os_ptr op)
{
    int code = gs_gsave(igs);

    if (code < 0)
	return code;
    if ((code = upath_append(op, op)) >= 0)
	code = gs_eofill(igs);
    gs_grestore(igs);
    if (code < 0)
	return code;
    pop(1);
    return 0;
}

/* <userpath> ufill - */
private int
zufill(register os_ptr op)
{
    int code = gs_gsave(igs);

    if (code < 0)
	return code;
    if ((code = upath_append(op, op)) >= 0)
	code = gs_fill(igs);
    gs_grestore(igs);
    if (code < 0)
	return code;
    pop(1);
    return 0;
}

/* <userpath> ustroke - */
/* <userpath> <matrix> ustroke - */
private int
zustroke(register os_ptr op)
{
    int code = gs_gsave(igs);
    int npop;

    if (code < 0)
	return code;
    if ((code = npop = upath_stroke(op, NULL)) >= 0)
	code = gs_stroke(igs);
    gs_grestore(igs);
    if (code < 0)
	return code;
    pop(npop);
    return 0;
}

/* <userpath> ustrokepath - */
/* <userpath> <matrix> ustrokepath - */
private int
zustrokepath(register os_ptr op)
{
    gx_path save;
    int code, npop;

    /* Save and reset the path. */
    gx_path_init_local(&save, imemory);
    gx_path_assign_preserve(&save, igs->path);
    if ((code = npop = upath_stroke(op, NULL)) < 0 ||
	(code = gs_strokepath(igs)) < 0
	) {
	gx_path_assign_free(igs->path, &save);
	return code;
    }
    gx_path_free(&save, "ustrokepath");
    pop(npop);
    return 0;
}

/* <with_ucache> upath <userpath> */
/* We do all the work in a procedure that is also used to construct */
/* the UnpaintedPath user path for ImageType 2 images. */
int make_upath(P4(ref * rupath, gs_state * pgs, gx_path * ppath,
		  bool with_ucache));
private int
zupath(register os_ptr op)
{
    check_type(*op, t_boolean);
    return make_upath(op, igs, igs->path, op->value.boolval);
}
int
make_upath(ref * rupath, gs_state * pgs, gx_path * ppath, bool with_ucache)
{
    int size = (with_ucache ? 6 : 5);
    gs_path_enum penum;
    int op;
    ref *next;
    int code;

    /* Compute the size of the user path array. */
    {
	gs_fixed_point pts[3];

	gx_path_enum_init(&penum, ppath);
	while ((op = gx_path_enum_next(&penum, pts)) != 0) {
	    switch (op) {
		case gs_pe_moveto:
		case gs_pe_lineto:
		    size += 3;
		    continue;
		case gs_pe_curveto:
		    size += 7;
		    continue;
		case gs_pe_closepath:
		    size += 1;
		    continue;
		default:
		    return_error(e_unregistered);
	    }
	}
    }
    code = ialloc_ref_array(rupath, a_all | a_executable, size,
			    "make_upath");
    if (code < 0)
	return code;
    /* Construct the path. */
    next = rupath->value.refs;
    if (with_ucache) {
	if ((code = name_enter_string("ucache", next)) < 0)
	    return code;
	r_set_attrs(next, a_executable | l_new);
	++next;
    } {
	gs_rect bbox;

	gs_upathbbox(pgs, &bbox, true);
	make_real_new(next, bbox.p.x);
	make_real_new(next + 1, bbox.p.y);
	make_real_new(next + 2, bbox.q.x);
	make_real_new(next + 3, bbox.q.y);
	next += 4;
	if ((code = name_enter_string("setbbox", next)) < 0)
	    return code;
	r_set_attrs(next, a_executable | l_new);
	++next;
    }
    {
	gs_point pts[3];

	/* Patch the path in the gstate to set up the enumerator. */
	gx_path *save_path = pgs->path;

	pgs->path = ppath;
	gs_path_enum_copy_init(&penum, pgs, false);
	pgs->path = save_path;
	while ((op = gs_path_enum_next(&penum, pts)) != 0) {
	    const char *opstr;

	    switch (op) {
		case gs_pe_moveto:
		    opstr = "moveto";
		    goto ml;
		case gs_pe_lineto:
		    opstr = "lineto";
		  ml:make_real_new(next, pts[0].x);
		    make_real_new(next + 1, pts[0].y);
		    next += 2;
		    break;
		case gs_pe_curveto:
		    opstr = "curveto";
		    make_real_new(next, pts[0].x);
		    make_real_new(next + 1, pts[0].y);
		    make_real_new(next + 2, pts[1].x);
		    make_real_new(next + 3, pts[1].y);
		    make_real_new(next + 4, pts[2].x);
		    make_real_new(next + 5, pts[2].y);
		    next += 6;
		    break;
		case gs_pe_closepath:
		    opstr = "closepath";
		    break;
		default:
		    return_error(e_unregistered);
	    }
	    if ((code = name_enter_string(opstr, next)) < 0)
		return code;
	    r_set_attrs(next, a_executable);
	    ++next;
	}
    }
    return 0;
}

/* ------ Internal routines ------ */

/* Append a user path to the current path. */
private int
upath_append(os_ptr oppath, os_ptr op)
{
    check_read(*oppath);
    gs_newpath(igs);
/****** ROUND tx AND ty ******/
    if (r_has_type(oppath, t_array) && r_size(oppath) == 2 &&
	r_has_type(oppath->value.refs + 1, t_string)
	) {			/* 1st element is operators, 2nd is operands */
	const ref *operands = oppath->value.refs;
	int code, format;
	int repcount = 1;
	const byte *opp;
	uint ocount, i = 0;

	code = num_array_format(operands);
	if (code < 0)
	    return code;
	format = code;
	opp = oppath->value.refs[1].value.bytes;
	ocount = r_size(&oppath->value.refs[1]);
	while (ocount--) {
	    byte opx = *opp++;

	    if (opx > UPATH_REPEAT)
		repcount = opx - UPATH_REPEAT;
	    else if (opx > UPATH_MAX_OP)
		return_error(e_rangecheck);
	    else {		/* operator */
		do {
		    byte opargs = up_nargs[opx];

		    while (opargs--) {
			push(1);
			code = num_array_get(operands, format, i++, op);
			switch (code) {
			    case t_integer:
				r_set_type_attrs(op, t_integer, 0);
				break;
			    case t_real:
				r_set_type_attrs(op, t_real, 0);
				break;
			    default:
				return_error(e_typecheck);
			}
		    }
		    code = (*up_ops[opx])(op);
		    if (code < 0)
			return code;
		    op = osp;	/* resync */
		}
		while (--repcount);
		repcount = 1;
	    }
	}
    } else if (r_is_array(oppath)) {	/* Ordinary executable array. */
	const ref *arp = oppath;
	uint ocount = r_size(oppath);
	long index = 0;
	int argcount = 0;
	int (*oproc)(P1(os_ptr));
	int opx, code;

	for (; index < ocount; index++) {
	    ref rup;
	    ref *defp;

	    array_get(arp, index, &rup);
	    switch (r_type(&rup)) {
		case t_integer:
		case t_real:
		    argcount++;
		    push(1);
		    *op = rup;
		    break;
		case t_name:
		    if (!r_has_attr(&rup, a_executable))
			return_error(e_typecheck);
		    if (dict_find(systemdict, &rup, &defp) <= 0)
			return_error(e_undefined);
		    if (r_btype(defp) != t_operator)
			return_error(e_typecheck);
		    goto xop;
		case t_operator:
		    defp = &rup;
		  xop:if (!r_has_attr(defp, a_executable))
			return_error(e_typecheck);
		    oproc = real_opproc(defp);
		    for (opx = 0; opx <= UPATH_MAX_OP; opx++)
			if (oproc == up_ops[opx])
			    break;
		    if (opx > UPATH_MAX_OP || argcount != up_nargs[opx])
			return_error(e_typecheck);
		    code = (*oproc)(op);
		    if (code < 0)
			return code;
		    op = osp;	/* resync ostack pointer */
		    argcount = 0;
		    break;
		default:
		    return_error(e_typecheck);
	    }
	}
	if (argcount)
	    return_error(e_typecheck);	/* leftover args */
    } else
	return_error(e_typecheck);
    return 0;
}

/* Append a user path to the current path, and then apply or return */
/* a transformation if one is supplied. */
private int
upath_stroke(os_ptr op, gs_matrix *pmat)
{
    int code, npop;
    gs_matrix mat;

    if ((code = read_matrix(op, &mat)) >= 0) {
	if ((code = upath_append(op - 1, op)) >= 0) {
	    if (pmat)
		*pmat = mat;
	    else
		code = gs_concat(igs, &mat);
	}
	npop = 2;
    } else {
	if ((code = upath_append(op, op)) >= 0)
	    if (pmat)
		gs_make_identity(pmat);
	npop = 1;
    }
    return (code < 0 ? code : npop);
}

/* ---------------- Initialization procedure ---------------- */

const op_def zupath_l2_op_defs[] =
{
    op_def_begin_level2(),
		/* Insideness testing */
    {"1ineofill", zineofill},
    {"1infill", zinfill},
    {"1instroke", zinstroke},
    {"2inueofill", zinueofill},
    {"2inufill", zinufill},
    {"2inustroke", zinustroke},
		/* User paths */
    {"1uappend", zuappend},
    {"0ucache", zucache},
    {"1ueofill", zueofill},
    {"1ufill", zufill},
    {"1upath", zupath},
    {"1ustroke", zustroke},
    {"1ustrokepath", zustrokepath},
    op_def_end(0)
};
