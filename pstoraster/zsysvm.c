/* Copyright (C) 1994, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zsysvm.c,v 1.2 2000/03/08 23:15:42 mike Exp $ */
/* System VM and VM-specific operators */
#include "ghost.h"
#include "oper.h"
#include "ialloc.h"
#include "ivmspace.h"
#include "store.h"		/* for make_bool */

/*
 * These operators allow creation of objects in a specific VM --
 * local, global, or system.  System VM, which is not a standard PostScript
 * facility, is not subject to save and restore; objects in system VM
 * may only refer to simple objects or to other (composite) objects
 * in system VM.
 */

/* Execute an operator with a specific VM selected as current VM. */
private int
specific_vm_op(os_ptr op, int (*opproc)(P1(os_ptr)), uint space)
{
    uint save_space = icurrent_space;
    int code;

    ialloc_set_space(idmemory, space);
    code = (*opproc)(op);
    ialloc_set_space(idmemory, save_space);
    return code;
}

/* <int> .globalvmarray <array> */
private int
zglobalvmarray(os_ptr op)
{
    return specific_vm_op(op, zarray, avm_global);
}

/* <int> .globalvmdict <dict> */
private int
zglobalvmdict(os_ptr op)
{
    return specific_vm_op(op, zdict, avm_global);
}

/* <obj_0> ... <obj_n-1> <n> .globalvmpackedarray <packedarray> */
private int
zglobalvmpackedarray(os_ptr op)
{
    return specific_vm_op(op, zpackedarray, avm_global);
}

/* <int> .globalvmstring <string> */
private int
zglobalvmstring(os_ptr op)
{
    return specific_vm_op(op, zstring, avm_global);
}

/* <int> .localvmarray <array> */
private int
zlocalvmarray(os_ptr op)
{
    return specific_vm_op(op, zarray, avm_local);
}

/* <int> .localvmdict <dict> */
private int
zlocalvmdict(os_ptr op)
{
    return specific_vm_op(op, zdict, avm_local);
}

/* <obj_0> ... <obj_n-1> <n> .localvmpackedarray <packedarray> */
private int
zlocalvmpackedarray(os_ptr op)
{
    return specific_vm_op(op, zpackedarray, avm_local);
}

/* <int> .localvmstring <string> */
private int
zlocalvmstring(os_ptr op)
{
    return specific_vm_op(op, zstring, avm_local);
}

/* <int> .systemvmarray <array> */
private int
zsystemvmarray(os_ptr op)
{
    return specific_vm_op(op, zarray, avm_system);
}

/* <int> .systemvmdict <dict> */
private int
zsystemvmdict(os_ptr op)
{
    return specific_vm_op(op, zdict, avm_system);
}

/* <obj_0> ... <obj_n-1> <n> .systemvmpackedarray <packedarray> */
private int
zsystemvmpackedarray(os_ptr op)
{
    return specific_vm_op(op, zpackedarray, avm_system);
}

/* <int> .systemvmstring <string> */
private int
zsystemvmstring(os_ptr op)
{
    return specific_vm_op(op, zstring, avm_system);
}

/* <any> .systemvmcheck <bool> */
private int
zsystemvmcheck(register os_ptr op)
{
    make_bool(op, (r_space(op) == avm_system ? true : false));
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zsysvm_op_defs[] =
{
    {"1.globalvmarray", zglobalvmarray},
    {"1.globalvmdict", zglobalvmdict},
    {"1.globalvmpackedarray", zglobalvmpackedarray},
    {"1.globalvmstring", zglobalvmstring},
    {"1.localvmarray", zlocalvmarray},
    {"1.localvmdict", zlocalvmdict},
    {"1.localvmpackedarray", zlocalvmpackedarray},
    {"1.localvmstring", zlocalvmstring},
    {"1.systemvmarray", zsystemvmarray},
    {"1.systemvmcheck", zsystemvmcheck},
    {"1.systemvmdict", zsystemvmdict},
    {"1.systemvmpackedarray", zsystemvmpackedarray},
    {"1.systemvmstring", zsystemvmstring},
    op_def_end(0)
};
