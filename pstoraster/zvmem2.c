/* Copyright (C) 1992, 1993, 1994, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zvmem2.c,v 1.2 2000/03/08 23:15:43 mike Exp $ */
/* Level 2 "Virtual memory" operators */
#include "ghost.h"
#include "oper.h"
#include "estack.h"
#include "ialloc.h"		/* for ivmspace.h */
#include "ivmspace.h"
#include "store.h"

/* Garbage collector control parameters. */
#define DEFAULT_VM_THRESHOLD_SMALL 20000
#define DEFAULT_VM_THRESHOLD_LARGE 250000
#define MIN_VM_THRESHOLD 1
#define MAX_VM_THRESHOLD max_long

/* ------ Local/global VM control ------ */

/* <bool> .setglobal - */
private int
zsetglobal(register os_ptr op)
{
    check_type(*op, t_boolean);
    ialloc_set_space(idmemory,
		     (op->value.boolval ? avm_global : avm_local));
    pop(1);
    return 0;
}

/* <bool> .currentglobal - */
private int
zcurrentglobal(register os_ptr op)
{
    push(1);
    make_bool(op, ialloc_space(idmemory) != avm_local);
    return 0;
}

/* <any> gcheck/scheck <bool> */
private int
zgcheck(register os_ptr op)
{
    check_op(1);
    make_bool(op, (r_is_local(op) ? false : true));
    return 0;
}

/* ------ Garbage collector control ------ */

/* These routines are exported for setuserparams. */

/*
 * <int> setvmthreshold -
 *
 * This is implemented as a PostScript procedure that calls setuserparams.
 */
int
set_vm_threshold(long val)
{
    gs_memory_gc_status_t stat;

    if (val < -1)
	return_error(e_rangecheck);
    else if (val == -1)
	val = (gs_debug_c('.') ? DEFAULT_VM_THRESHOLD_SMALL :
	       DEFAULT_VM_THRESHOLD_LARGE);
    else if (val < MIN_VM_THRESHOLD)
	val = MIN_VM_THRESHOLD;
    else if (val > MAX_VM_THRESHOLD)
	val = MAX_VM_THRESHOLD;
    gs_memory_gc_status(idmemory->space_global, &stat);
    stat.vm_threshold = val;
    gs_memory_set_gc_status(idmemory->space_global, &stat);
    gs_memory_gc_status(idmemory->space_local, &stat);
    stat.vm_threshold = val;
    gs_memory_set_gc_status(idmemory->space_local, &stat);
    return 0;
}

/*
 * <int> .vmreclaim -
 *
 * This implements only immediate garbage collection: enabling and
 * disabling GC is implemented by calling setuserparams.
 */
int
set_vm_reclaim(long val)
{
    if (val >= -2 && val <= 0) {
	gs_memory_gc_status_t stat;

	gs_memory_gc_status(idmemory->space_system, &stat);
	stat.enabled = val >= -1;
	gs_memory_set_gc_status(idmemory->space_system, &stat);
	gs_memory_gc_status(idmemory->space_global, &stat);
	stat.enabled = val >= -1;
	gs_memory_set_gc_status(idmemory->space_global, &stat);
	gs_memory_gc_status(idmemory->space_local, &stat);
	stat.enabled = val == 0;
	gs_memory_set_gc_status(idmemory->space_local, &stat);
	return 0;
    } else
	return_error(e_rangecheck);
}
private int
zvmreclaim(register os_ptr op)
{
    check_type(*op, t_integer);
    if (op->value.intval == 1 || op->value.intval == 2) {
	/* Force the interpreter to store its state and exit. */
	/* The interpreter's caller will do the actual GC. */
	return_error(e_VMreclaim);
    }
    return_error(e_rangecheck);
}

/* ------ Initialization procedure ------ */

/* The VM operators are defined even if the initial language level is 1, */
/* because we need them during initialization. */
const op_def zvmem2_op_defs[] =
{
    {"0.currentglobal", zcurrentglobal},
    {"1.gcheck", zgcheck},
    {"1.setglobal", zsetglobal},
		/* The rest of the operators are defined only in Level 2. */
    op_def_begin_level2(),
    {"1.vmreclaim", zvmreclaim},
    op_def_end(0)
};
