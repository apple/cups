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

/*$Id: zgstate.c,v 1.2 2000/03/08 23:15:38 mike Exp $ */
/* Graphics state operators */
#include "math_.h"
#include "ghost.h"
#include "oper.h"
#include "ialloc.h"
#include "idict.h"
#include "istruct.h"
#include "igstate.h"
#include "gsmatrix.h"
#include "store.h"

/* Forward references */
private int num_param(P2(const_os_ptr, int (*)(P2(gs_state *, floatp))));

/* Structure descriptors */
private_st_int_gstate();

/* ------ Operations on the entire graphics state ------ */

/* The current graphics state */
gs_state *igs;

/* "Client" procedures */
private void *gs_istate_alloc(P1(gs_memory_t * mem));
private int gs_istate_copy(P2(void *to, const void *from));
private void gs_istate_free(P2(void *old, gs_memory_t * mem));
private const gs_state_client_procs istate_procs =
{
    gs_istate_alloc,
    gs_istate_copy,
    gs_istate_free
};

/* Initialize the graphics stack. */
void
igs_init(void)
{
    gs_register_struct_root(imemory, NULL, (void **)&igs, "igs");
    igs = int_gstate_alloc(iimemory);
}
gs_state *
int_gstate_alloc(gs_ref_memory_t * mem)
{
    int_gstate *iigs;
    ref proc0;
    gs_state *pgs = gs_state_alloc((gs_memory_t *) mem);

    iigs = gs_alloc_struct((gs_memory_t *) mem, int_gstate, &st_int_gstate,
			   "int_gstate_alloc(int_gstate)");
    int_gstate_map_refs(iigs, make_null);
    make_empty_array(&iigs->dash_pattern, a_all);
    gs_alloc_ref_array(mem, &proc0, a_readonly + a_executable, 2,
		       "int_gstate_alloc(proc0)");
    make_oper(proc0.value.refs, 0, zpop);
    make_real(proc0.value.refs + 1, 0.0);
    iigs->black_generation = proc0;
    iigs->undercolor_removal = proc0;
    clear_pagedevice(iigs);
    gs_state_set_client(pgs, iigs, &istate_procs);
    /* PostScript code wants limit clamping enabled. */
    gs_setlimitclamp(pgs, true);
    /*
     * gsave and grestore only work properly
     * if there are always at least 2 entries on the stack.
     * We count on the PostScript initialization code to do a gsave.
     */
    return pgs;
}

/* - gsave - */
int
zgsave(register os_ptr op)
{
    return gs_gsave(igs);
}

/* - grestore - */
int
zgrestore(register os_ptr op)
{
    return gs_grestore(igs);
}

/* - grestoreall - */
int
zgrestoreall(register os_ptr op)
{
    return gs_grestoreall(igs);
}

/* - initgraphics - */
private int
zinitgraphics(register os_ptr op)
{
    /* gs_initgraphics does a setgray; we must clear the interpreter's */
    /* cached copy of the color space object. */
    int code = gs_initgraphics(igs);

    if (code >= 0)
	make_null(&istate->colorspace.array);
    return code;
}

/* ------ Operations on graphics state elements ------ */

/* <num> setlinewidth - */
private int
zsetlinewidth(register os_ptr op)
{	/*
	 * The Red Book doesn't say anything about this, but Adobe
	 * interpreters return (or perhaps store) the absolute value
	 * of the width.
	 */
    double width;
    int code = real_param(op, &width);

    if (code < 0)
	return_op_typecheck(op);
    code = gs_setlinewidth(igs, fabs(width));
    if (code >= 0)
	pop(1);
    return code;
}

/* - currentlinewidth <num> */
private int
zcurrentlinewidth(register os_ptr op)
{
    push(1);
    make_real(op, gs_currentlinewidth(igs));
    return 0;
}

/* <cap_int> .setlinecap - */
private int
zsetlinecap(register os_ptr op)
{
    int param;
    int code = int_param(op, max_int, &param);

    if (code < 0 || (code = gs_setlinecap(igs, (gs_line_cap) param)) < 0)
	return code;
    pop(1);
    return 0;
}

/* - currentlinecap <cap_int> */
private int
zcurrentlinecap(register os_ptr op)
{
    push(1);
    make_int(op, (int)gs_currentlinecap(igs));
    return 0;
}

/* <join_int> .setlinejoin - */
private int
zsetlinejoin(register os_ptr op)
{
    int param;
    int code = int_param(op, max_int, &param);

    if (code < 0 || (code = gs_setlinejoin(igs, (gs_line_join) param)) < 0)
	return code;
    pop(1);
    return 0;
}

/* - currentlinejoin <join_int> */
private int
zcurrentlinejoin(register os_ptr op)
{
    push(1);
    make_int(op, (int)gs_currentlinejoin(igs));
    return 0;
}

/* <num> setmiterlimit - */
private int
zsetmiterlimit(register os_ptr op)
{
    return num_param(op, gs_setmiterlimit);
}

/* - currentmiterlimit <num> */
private int
zcurrentmiterlimit(register os_ptr op)
{
    push(1);
    make_real(op, gs_currentmiterlimit(igs));
    return 0;
}

/* <array> <offset> setdash - */
private int
zsetdash(register os_ptr op)
{
    os_ptr op1 = op - 1;
    double offset;
    int code = real_param(op, &offset);
    uint i, n;
    gs_memory_t *mem = imemory;
    float *pattern;

    if (code < 0)
	return_op_typecheck(op);
    if (!r_is_array(op1))
	return_op_typecheck(op1);
    /* Adobe interpreters apparently don't check the array for */
    /* read access, so we won't either. */
    /*check_read(*op1); */
    /* Unpack the dash pattern and check it */
    n = r_size(op1);
    pattern =
	(float *)gs_alloc_byte_array(mem, n, sizeof(float), "setdash");

    if (pattern == 0)
	return_error(e_VMerror);
    for (i = 0, code = 0; i < n && code >= 0; ++i) {
	ref element;

	array_get(op1, (long)i, &element);
	code = float_param(&element, &pattern[i]);
    }
    if (code >= 0)
	code = gs_setdash(igs, pattern, n, offset);
    gs_free_object(mem, pattern, "setdash");	/* gs_setdash copies this */
    if (code < 0)
	return code;
    ref_assign(&istate->dash_pattern, op1);
    pop(2);
    return code;
}

/* - currentdash <array> <offset> */
private int
zcurrentdash(register os_ptr op)
{
    push(2);
    ref_assign(op - 1, &istate->dash_pattern);
    make_real(op, gs_currentdash_offset(igs));
    return 0;
}

/* <num> setflat - */
private int
zsetflat(register os_ptr op)
{
    return num_param(op, gs_setflat);
}

/* - currentflat <num> */
private int
zcurrentflat(register os_ptr op)
{
    push(1);
    make_real(op, gs_currentflat(igs));
    return 0;
}

/* ------ Extensions ------ */

/* <bool> .setaccuratecurves - */
private int
zsetaccuratecurves(register os_ptr op)
{
    check_type(*op, t_boolean);
    gs_setaccuratecurves(igs, op->value.boolval);
    pop(1);
    return 0;
}

/* - .currentaccuratecurves <bool> */
private int
zcurrentaccuratecurves(register os_ptr op)
{
    push(1);
    make_bool(op, gs_currentaccuratecurves(igs));
    return 0;
}

/* <adjust.x> <adjust.y> .setfilladjust2 - */
private int
zsetfilladjust2(register os_ptr op)
{
    double adjust[2];
    int code = num_params(op, 2, adjust);

    if (code < 0)
	return code;
    code = gs_setfilladjust(igs, adjust[0], adjust[1]);
    if (code < 0)
	return code;
    pop(2);
    return 0;
}

/* - .currentfilladjust2 <adjust.x> <adjust.y> */
private int
zcurrentfilladjust2(register os_ptr op)
{
    gs_point adjust;

    push(2);
    gs_currentfilladjust(igs, &adjust);
    make_real(op - 1, adjust.x);
    make_real(op, adjust.y);
    return 0;
}

/* <bool> .setdashadapt - */
private int
zsetdashadapt(register os_ptr op)
{
    check_type(*op, t_boolean);
    gs_setdashadapt(igs, op->value.boolval);
    pop(1);
    return 0;
}

/* - .currentdashadapt <bool> */
private int
zcurrentdashadapt(register os_ptr op)
{
    push(1);
    make_bool(op, gs_currentdashadapt(igs));
    return 0;
}

/* <num> <bool> .setdotlength - */
private int
zsetdotlength(register os_ptr op)
{
    double length;
    int code = real_param(op - 1, &length);

    if (code < 0)
	return code;
    check_type(*op, t_boolean);
    code = gs_setdotlength(igs, length, op->value.boolval);
    if (code < 0)
	return code;
    pop(2);
    return 0;
}

/* - .currentdotlength <num> <bool> */
private int
zcurrentdotlength(register os_ptr op)
{
    push(2);
    make_real(op - 1, gs_currentdotlength(igs));
    make_bool(op, gs_currentdotlength_absolute(igs));
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zgstate_op_defs[] =
{
    {"0.currentaccuratecurves", zcurrentaccuratecurves},
    {"0currentdash", zcurrentdash},
    {"0.currentdashadapt", zcurrentdashadapt},
    {"0.currentdotlength", zcurrentdotlength},
    {"0.currentfilladjust2", zcurrentfilladjust2},
    {"0currentflat", zcurrentflat},
    {"0currentlinecap", zcurrentlinecap},
    {"0currentlinejoin", zcurrentlinejoin},
    {"0currentlinewidth", zcurrentlinewidth},
    {"0currentmiterlimit", zcurrentmiterlimit},
    {"0grestore", zgrestore},
    {"0grestoreall", zgrestoreall},
    {"0gsave", zgsave},
    {"0initgraphics", zinitgraphics},
    {"1.setaccuratecurves", zsetaccuratecurves},
    {"2setdash", zsetdash},
    {"1.setdashadapt", zsetdashadapt},
    {"2.setdotlength", zsetdotlength},
    {"2.setfilladjust2", zsetfilladjust2},
    {"1setflat", zsetflat},
    {"1.setlinecap", zsetlinecap},
    {"1.setlinejoin", zsetlinejoin},
    {"1setlinewidth", zsetlinewidth},
    {"1setmiterlimit", zsetmiterlimit},
    op_def_end(0)
};

/* ------ Internal routines ------ */

/* Allocate the interpreter's part of a graphics state. */
private void *
gs_istate_alloc(gs_memory_t * mem)
{
    return gs_alloc_struct(mem, int_gstate, &st_int_gstate, "int_gsave");
}

/* Copy the interpreter's part of a graphics state. */
private int
gs_istate_copy(void *to, const void *from)
{
    *(int_gstate *) to = *(const int_gstate *)from;
    return 0;
}

/* Free the interpreter's part of a graphics state. */
private void
gs_istate_free(void *old, gs_memory_t * mem)
{
    gs_free_object(mem, old, "int_grestore");
}

/* Get a numeric parameter */
private int
num_param(const_os_ptr op, int (*pproc)(P2(gs_state *, floatp)))
{
    double param;
    int code = real_param(op, &param);

    if (code < 0)
	return_op_typecheck(op);
    code = (*pproc)(igs, param);
    if (!code)
	pop(1);
    return code;
}
