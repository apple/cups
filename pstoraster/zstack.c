/* Copyright (C) 1989, 1991, 1992, 1994 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zstack.c,v 1.2 2000/03/08 23:15:42 mike Exp $ */
/* Operand stack operators */
#include "memory_.h"
#include "ghost.h"
#include "ialloc.h"
#include "istack.h"
#include "oper.h"
#include "store.h"

/* <obj> pop - */
int
zpop(register os_ptr op)
{
    check_op(1);
    pop(1);
    return 0;
}

/* <obj1> <obj2> exch <obj2> <obj1> */
int
zexch(register os_ptr op)
{
    ref next;

    check_op(2);
    ref_assign_inline(&next, op - 1);
    ref_assign_inline(op - 1, op);
    ref_assign_inline(op, &next);
    return 0;
}

/* <obj> dup <obj> <obj> */
int
zdup(register os_ptr op)
{
    check_op(1);
    push(1);
    ref_assign_inline(op, op - 1);
    return 0;
}

/* <obj_n> ... <obj_0> <n> index <obj_n> ... <obj_0> <obj_n> */
int
zindex(register os_ptr op)
{
    register os_ptr opn;

    check_type(*op, t_integer);
    if ((ulong)op->value.intval >= op - osbot) {
	/* Might be in an older stack block. */
	ref *elt;

	if (op->value.intval < 0)
	    return_error(e_rangecheck);
	elt = ref_stack_index(&o_stack, op->value.intval + 1);
	if (elt == 0)
	    return_error(e_rangecheck);
	ref_assign(op, elt);
	return 0;
    }
    opn = op + ~(int)op->value.intval;
    ref_assign_inline(op, opn);
    return 0;
}

/* <obj_n-1> ... <obj_0> <n> <i> roll */
/*      <obj_(i-1)_mod_ n> ... <obj_0> <obj_n-1> ... <obj_i_mod_n> */
int
zroll(register os_ptr op)
{
    os_ptr op1 = op - 1;
    int count, mod;
    register os_ptr from, to;
    register int n;

    check_type(*op1, t_integer);
    check_type(*op, t_integer);
    if ((ulong) op1->value.intval > op1 - osbot) {
	/*
	 * The data might span multiple stack blocks.
	 * There are efficient ways to handle this situation,
	 * but they're more complicated than seems worth implementing;
	 * for now, do something very simple and inefficient.
	 */
	int left, i;

	if (op1->value.intval < 0 ||
	    op1->value.intval + 2 > ref_stack_count(&o_stack)
	    )
	    return_error(e_rangecheck);
	count = op1->value.intval;
	if (count <= 1) {
	    pop(2);
	    return 0;
	}
	mod = op->value.intval;
	if (mod >= count)
	    mod %= count;
	else if (mod < 0) {
	    mod %= count;
	    if (mod < 0)
		mod += count;	/* can't assume % means mod! */
	}
	/* Use the chain rotation algorithm mentioned below. */
	for (i = 0, left = count; left; i++) {
	    ref *elt = ref_stack_index(&o_stack, i + 2);
	    ref save;
	    int j, k;
	    ref *next;

	    save = *elt;
	    for (j = i, left--;; j = k, elt = next, left--) {
		k = (j + mod) % count;
		if (k == i)
		    break;
		next = ref_stack_index(&o_stack, k + 2);
		ref_assign(elt, next);
	    }
	    *elt = save;
	}
	pop(2);
	return 0;
    }
    count = op1->value.intval;
    if (count <= 1) {
	pop(2);
	return 0;
    }
    mod = op->value.intval;
    /*
     * The elegant approach, requiring no extra space, would be to
     * rotate the elements in chains separated by mod elements.
     * Instead, we simply check to make sure there is enough space
     * above op to do the roll in two block moves.
     * Unfortunately, we can't count on memcpy doing the right thing
     * in *either* direction.
     */
    switch (mod) {
	case 1:		/* common special case */
	    pop(2);
	    op -= 2;
	    {
		ref top;

		ref_assign_inline(&top, op);
		for (from = op, n = count; --n; from--)
		    ref_assign_inline(from, from - 1);
		ref_assign_inline(from, &top);
	    }
	    return 0;
	case -1:		/* common special case */
	    pop(2);
	    op -= 2;
	    {
		ref bot;

		to = op - count + 1;
		ref_assign_inline(&bot, to);
		for (n = count; --n; to++)
		    ref_assign(to, to + 1);
		ref_assign_inline(to, &bot);
	    }
	    return 0;
    }
    if (mod < 0) {
	mod += count;
	if (mod < 0) {
	    mod %= count;
	    if (mod < 0)
		mod += count;	/* can't assume % means mod! */
	}
    } else if (mod >= count)
	mod %= count;
    if (mod <= count >> 1) {
	/* Move everything up, then top elements down. */
	if (mod >= ostop - op) {
	    o_stack.requested = mod;
	    return_error(e_stackoverflow);
	}
	pop(2);
	op -= 2;
	for (to = op + mod, from = op, n = count; n--; to--, from--)
	    ref_assign(to, from);
	memcpy((char *)(from + 1), (char *)(op + 1), mod * sizeof(ref));
    } else {
	/* Move bottom elements up, then everything down. */
	mod = count - mod;
	if (mod >= ostop - op) {
	    o_stack.requested = mod;
	    return_error(e_stackoverflow);
	}
	pop(2);
	op -= 2;
	to = op - count + 1;
	memcpy((char *)(op + 1), (char *)to, mod * sizeof(ref));
	for (from = to + mod, n = count; n--; to++, from++)
	    ref_assign(to, from);
    }
    return 0;
}

/* |- ... clear |- */
/* The function name is changed, because the IRIS library has */
/* a function called zclear. */
private int
zclear_stack(os_ptr op)
{
    ref_stack_clear(&o_stack);
    return 0;
}

/* |- <obj_n-1> ... <obj_0> count <obj_n-1> ... <obj_0> <n> */
private int
zcount(register os_ptr op)
{
    push(1);
    make_int(op, ref_stack_count(&o_stack) - 1);
    return 0;
}

/* - mark <mark> */
private int
zmark(register os_ptr op)
{
    push(1);
    make_mark(op);
    return 0;
}

/* <mark> ... cleartomark */
int
zcleartomark(register os_ptr op)
{
    uint count = ref_stack_counttomark(&o_stack);

    if (count == 0)
	return_error(e_unmatchedmark);
    ref_stack_pop(&o_stack, count);
    return 0;
}

/* <mark> <obj_n-1> ... <obj_0> counttomark */
/*      <mark> <obj_n-1> ... <obj_0> <n> */
private int
zcounttomark(os_ptr op)
{
    uint count = ref_stack_counttomark(&o_stack);

    if (count == 0)
	return_error(e_unmatchedmark);
    push(1);
    make_int(op, count - 1);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zstack_op_defs[] =
{
    {"0clear", zclear_stack},
    {"0cleartomark", zcleartomark},
    {"0count", zcount},
    {"0counttomark", zcounttomark},
    {"1dup", zdup},
    {"2exch", zexch},
    {"2index", zindex},
    {"0mark", zmark},
    {"1pop", zpop},
    {"2roll", zroll},
    op_def_end(0)
};
