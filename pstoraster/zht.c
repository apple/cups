/* Copyright (C) 1989, 1991, 1993, 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* zht.c */
/* Halftone definition operators */
#include "ghost.h"
#include "memory_.h"
#include "errors.h"
#include "oper.h"
#include "estack.h"
#include "gsstruct.h"			/* must precede igstate.h, */
					/* because of #ifdef in gsht.h */
#include "ialloc.h"
#include "igstate.h"
#include "gsmatrix.h"
#include "gxdevice.h"			/* for gzht.h */
#include "gzht.h"
#include "gsstate.h"
#include "store.h"

/* Forward references */
int zscreen_params(P2(os_ptr, gs_screen_halftone *));	/* exported for zht1.c */
private int screen_sample(P1(os_ptr));
private int set_screen_continue(P1(os_ptr));
private int screen_cleanup(P1(os_ptr));

/* - .currenthalftone <dict> 0 */
/* - .currenthalftone <frequency> <angle> <proc> 1 */
/* - .currenthalftone <red_freq> ... <gray_proc> 2 */
private int
zcurrenthalftone(register os_ptr op)
{	gs_halftone ht;
	gs_currenthalftone(igs, &ht);
	switch ( ht.type )
	{
	case ht_type_screen:
		push(4);
		make_real(op - 3, ht.params.screen.frequency);
		make_real(op - 2, ht.params.screen.angle);
		op[-1] = istate->screen_procs.colored.gray;
		make_int(op, 1);
		break;
	case ht_type_colorscreen:
		push(13);
		{	int i;
			for ( i = 0; i < 4; i++ )
			{	os_ptr opc = op - 12 + i * 3;
				gs_screen_halftone *pht =
				  &ht.params.colorscreen.screens.indexed[i];
				make_real(opc, pht->frequency);
				make_real(opc + 1, pht->angle);
				opc[2] = istate->screen_procs.indexed[i];
			}
		}
		make_int(op, 2);
		break;
	default:		/* Screen was set by sethalftone. */
		push(2);
		op[-1] = istate->halftone;
		make_int(op, 0);
		break;
	}
	return 0;
}

/* - .currentscreenlevels <int> */
private int
zcurrentscreenlevels(register os_ptr op)
{	push(1);
	make_int(op, gs_currentscreenlevels(igs));
	return 0;
}

/* The setscreen operator is complex because it has to sample */
/* each pixel in the pattern cell, calling a procedure, and then */
/* sort the result into a whitening order. */

/* Layout of stuff pushed on estack: */
/*	Control mark, */
/*	[other stuff for other screen-setting operators], */
/*	finishing procedure (or 0), */
/*	spot procedure, */
/*	enumeration structure (as bytes). */
#define snumpush 4
#define sproc esp[-1]
#define senum r_ptr(esp, gs_screen_enum)

/* Forward references */
int zscreen_enum_init(P6(os_ptr, const gx_ht_order *, gs_screen_halftone *,
			 ref *, int, int (*)(P1(os_ptr))));
private int setscreen_finish(P1(os_ptr));

/* <frequency> <angle> <proc> setscreen - */
private int
zsetscreen(register os_ptr op)
{	gs_screen_halftone screen;
	gx_ht_order order;
	int code = zscreen_params(op, &screen);
	if ( code < 0 )
		return code;
	code = gs_screen_order_init(&order, igs, &screen,
				    gs_currentaccuratescreens());
	if ( code < 0 )
		return code;
	return zscreen_enum_init(op, &order, &screen, op, 3,
				 setscreen_finish);
}
/* We break out the body of this operator so it can be shared with */
/* the code for Type 1 halftones in sethalftone. */
int
zscreen_enum_init(os_ptr op, const gx_ht_order *porder,
  gs_screen_halftone *psp, ref *pproc, int npop,
  int (*finish_proc)(P1(os_ptr)))
{	gs_screen_enum *penum;
	int code;
	check_estack(snumpush + 1);
	penum = gs_screen_enum_alloc(imemory, "setscreen");
	if ( penum == 0 )
		return_error(e_VMerror);
	make_istruct(esp + snumpush, 0, penum);	/* do early for screen_cleanup in case of error */
	code = gs_screen_enum_init(penum, porder, igs, psp);
	if ( code < 0 )
	{	screen_cleanup(op);
		return code;
	}
	/* Push everything on the estack */
	make_mark_estack(esp + 1, es_other, screen_cleanup);
	esp += snumpush;
	make_op_estack(esp - 2, finish_proc);
	sproc = *pproc;
	push_op_estack(screen_sample);
	pop(npop);
	return o_push_estack;
}
/* Set up the next sample */
private int
screen_sample(register os_ptr op)
{	gs_screen_enum *penum = senum;
	gs_point pt;
	int code = gs_screen_currentpoint(penum, &pt);
	ref proc;
	switch ( code )
	{
	default:
		return code;
	case 1:
		/* All done */
		if ( real_opproc(esp - 2) != 0 )
			code = (*real_opproc(esp - 2))(op);
		esp -= snumpush;
		screen_cleanup(op);
		return (code < 0 ? code : o_pop_estack);
	case 0:
		;
	}
	push(2);
	make_real(op - 1, pt.x);
	make_real(op, pt.y);
	proc = sproc;
	push_op_estack(set_screen_continue);
	*++esp = proc;
	return o_push_estack;
}
/* Continuation procedure for processing sampled pixels. */
private int
set_screen_continue(register os_ptr op)
{	float value;
	int code = num_params(op, 1, &value);
	if ( code < 0 ) return code;
	code = gs_screen_next(senum, value);
	if ( code < 0 ) return code;
	pop(1);  op--;
	return screen_sample(op);
}
/* Finish setscreen. */
private int
setscreen_finish(os_ptr op)
{	gs_screen_install(senum);
	istate->screen_procs.colored.red = sproc;
	istate->screen_procs.colored.green = sproc;
	istate->screen_procs.colored.blue = sproc;
	istate->screen_procs.colored.gray = sproc;
	make_null(&istate->halftone);
	return 0;
}
/* Clean up after screen enumeration */
private int
screen_cleanup(os_ptr op)
{	ifree_object(esp[snumpush].value.pstruct, "screen_cleanup");
	return 0;
}

/* ------ Utility procedures ------ */

/* Get parameters for a single screen. */
int
zscreen_params(os_ptr op, gs_screen_halftone *phs)
{	float fa[2];
	int code = num_params(op - 1, 2, fa);
	if ( code < 0 )
		return code;
	check_proc(*op);
	phs->frequency = fa[0];
	phs->angle = fa[1];
	return 0;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zht_op_defs) {
	{"0.currenthalftone", zcurrenthalftone},
	{"0.currentscreenlevels", zcurrentscreenlevels},
	{"3setscreen", zsetscreen},
		/* Internal operators */
	{"0%screen_sample", screen_sample},
	{"1%set_screen_continue", set_screen_continue},
	{"0%setscreen_finish", setscreen_finish},
END_OP_DEFS(0) }
