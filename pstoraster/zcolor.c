/* Copyright (C) 1989, 1992, 1993, 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* zcolor.c */
/* Color operators */
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "estack.h"
#include "ialloc.h"
#include "igstate.h"
#include "iutil.h"
#include "store.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gzstate.h"
#include "gxdevice.h"
#include "gxcmap.h"
#include "icolor.h"

/* Import the 'for' operator */
extern int
  zfor_fraction(P1(os_ptr));

/* Imported from gsht.c */
void gx_set_effective_transfer(P1(gs_state *));

/* Define the generic transfer function for the library layer. */
/* This just returns what's already in the map. */
float
gs_mapped_transfer(floatp value, const gx_transfer_map *pmap)
{	return gx_map_color_float(pmap, value);
}

/* Define the number of stack slots needed for zcolor_remap_one. */
const int zcolor_remap_one_ostack = 4;
const int zcolor_remap_one_estack = 3;

/* - currentalpha <alpha> */
private int
zcurrentalpha(register os_ptr op)
{	push(1);
	make_real(op, gs_currentalpha(igs));
	return 0;
}

/* - currentgray <gray> */
private int
zcurrentgray(register os_ptr op)
{	push(1);
	make_real(op, gs_currentgray(igs));
	return 0;
}

/* - currentrgbcolor <red> <green> <blue> */
private int
zcurrentrgbcolor(register os_ptr op)
{	float par[3];
	gs_currentrgbcolor(igs, par);
	push(3);
	make_reals(op - 2, par, 3);
	return 0;
}

/* - currenttransfer <proc> */
private int
zcurrenttransfer(register os_ptr op)
{	push(1);
	*op = istate->transfer_procs.colored.gray;
	return 0;
}

/* - processcolors <int> - */
/* Note: this is an undocumented operator that is not supported */
/* in Level 2. */
private int
zprocesscolors(register os_ptr op)
{	push(1);
	make_int(op, gs_currentdevice(igs)->color_info.num_components);
	return 0;
}

/* <alpha> setalpha - */
private int
zsetalpha(register os_ptr op)
{	float alpha;
	int code;
	if ( real_param(op, &alpha) < 0 )
	  return_op_typecheck(op);
	if ( (code = gs_setalpha(igs, alpha)) < 0 )
	  return code;
	pop(1);
	return 0;
}

/* <gray> setgray - */
private int
zsetgray(register os_ptr op)
{	float gray;
	int code;
	if ( real_param(op, &gray) < 0 )
	  return_op_typecheck(op);
	if ( (code = gs_setgray(igs, gray)) < 0 )
	  return code;
	make_null(&istate->colorspace.array);
	pop(1);
	return 0;
}

/* <red> <green> <blue> setrgbcolor - */
private int
zsetrgbcolor(register os_ptr op)
{	float par[3];
	int code;
	if (	(code = num_params(op, 3, par)) < 0 ||
		(code = gs_setrgbcolor(igs, par[0], par[1], par[2])) < 0
	   )
		return code;
	make_null(&istate->colorspace.array);
	pop(3);
	return 0;
}

/* <proc> settransfer - */
private int
zsettransfer(register os_ptr op)
{	int code;
	check_proc(*op);
	check_ostack(zcolor_remap_one_ostack - 1);
	check_estack(1 + zcolor_remap_one_estack);
	istate->transfer_procs.colored.red =
	  istate->transfer_procs.colored.green =
	  istate->transfer_procs.colored.blue =
	  istate->transfer_procs.colored.gray = *op;
	code = gs_settransfer_remap(igs, gs_mapped_transfer, false);
	if ( code < 0 ) return code;
	push_op_estack(zcolor_reset_transfer);
	pop(1);  op--;
	return zcolor_remap_one(&istate->transfer_procs.colored.gray, op,
				igs->set_transfer.colored.gray, igs,
				zcolor_remap_one_finish);
}

/* ------ Internal routines ------ */

/* Prepare to remap one color component */
/* (also used for black generation and undercolor removal). */
/* Use the 'for' operator to gather the values. */
/* The caller must have done the necessary check_ostack and check_estack. */
int
zcolor_remap_one(const ref *pproc, register os_ptr op, gx_transfer_map *pmap,
  const gs_state *pgs, int (*finish_proc)(P1(os_ptr)))
{	osp = op += 4;
	make_int(op - 3, 0);
	make_int(op - 2, 1);
	make_int(op - 1, transfer_map_size - 1);
	*op = *pproc;
	++esp;
	make_struct(esp, imemory_space((gs_ref_memory_t *)pgs->memory),
		    pmap);
	push_op_estack(finish_proc);
	push_op_estack(zfor_fraction);
	return o_push_estack;
}

/* Store the result of remapping a component. */
private int
zcolor_remap_one_store(os_ptr op, floatp min_value)
{	int i;
	gx_transfer_map *pmap = r_ptr(esp, gx_transfer_map);
	if ( ref_stack_count(&o_stack) < transfer_map_size )
	  return_error(e_stackunderflow);
	for ( i = 0; i < transfer_map_size; i++ )
	{	float v;
		int code =
		  real_param(ref_stack_index(&o_stack,
					     transfer_map_size - 1 - i),
			     &v);
		if ( code < 0 ) return code;
		pmap->values[i] =
			(v < min_value ? float2frac(min_value) :
			 v >= 1.0 ? frac_1 :
			 float2frac(v));
	}
	ref_stack_pop(&o_stack, transfer_map_size);
	esp--;				/* pop pointer to transfer map */
	return o_pop_estack;
}
int
zcolor_remap_one_finish(os_ptr op)
{	return zcolor_remap_one_store(op, 0.0);
}
int
zcolor_remap_one_signed_finish(os_ptr op)
{	return zcolor_remap_one_store(op, -1.0);
}

/* Finally, reset the effective transfer functions and */
/* invalidate the current color. */
int
zcolor_reset_transfer(os_ptr op)
{	gx_set_effective_transfer(igs);
	return zcolor_remap_color(op);
}
int
zcolor_remap_color(os_ptr op)
{	gx_unset_dev_color(igs);
	return 0;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zcolor_op_defs) {
	{"0currentalpha", zcurrentalpha},
	{"0currentgray", zcurrentgray},
	{"0currentrgbcolor", zcurrentrgbcolor},
	{"0currenttransfer", zcurrenttransfer},
	{"0processcolors", zprocesscolors},
	{"1setalpha", zsetalpha},
	{"1setgray", zsetgray},
	{"3setrgbcolor", zsetrgbcolor},
	{"1settransfer", zsettransfer},
		/* Internal operators */
	{"1%zcolor_remap_one_finish", zcolor_remap_one_finish},
	{"1%zcolor_remap_one_signed_finish", zcolor_remap_one_signed_finish},
	{"0%zcolor_reset_transfer", zcolor_reset_transfer},
	{"0%zcolor_remap_color", zcolor_remap_color},
END_OP_DEFS(0) }
