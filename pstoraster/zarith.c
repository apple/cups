/* Copyright (C) 1989, 1992, 1993, 1994, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zarith.c,v 1.2 2000/03/08 23:15:30 mike Exp $ */
/* Arithmetic operators */
#include "math_.h"
#include "ghost.h"
#include "oper.h"
#include "store.h"

/****** NOTE: none of the arithmetic operators  ******/
/****** currently check for floating exceptions ******/

/* Define max and min values for what will fit in value.intval. */
#define MIN_INTVAL min_long
#define MAX_INTVAL max_long
#define MAX_HALF_INTVAL ((1 << (size_of(long) / 2 - 1)) - 1)

/* <num1> <num2> add <sum> */
/* We make this into a separate procedure because */
/* the interpreter will almost always call it directly. */
int
zop_add(register os_ptr op)
{
    switch (r_type(op)) {
    default:
	return_op_typecheck(op);
    case t_real:
	switch (r_type(op - 1)) {
	default:
	    return_op_typecheck(op - 1);
	case t_real:
	    op[-1].value.realval += op->value.realval;
	    break;
	case t_integer:
	    make_real(op - 1, (double)op[-1].value.intval + op->value.realval);
	}
	break;
    case t_integer:
	switch (r_type(op - 1)) {
	default:
	    return_op_typecheck(op - 1);
	case t_real:
	    op[-1].value.realval += (double)op->value.intval;
	    break;
	case t_integer: {
	    long int2 = op->value.intval;

	    if (((op[-1].value.intval += int2) ^ int2) < 0 &&
		((op[-1].value.intval - int2) ^ int2) >= 0
		) {			/* Overflow, convert to real */
		make_real(op - 1, (double)(op[-1].value.intval - int2) + int2);
	    }
	}
	}
    }
    return 0;
}
int
zadd(os_ptr op)
{
    int code = zop_add(op);

    if (code == 0) {
	pop(1);
    }
    return code;
}

/* <num1> <num2> div <real_quotient> */
private int
zdiv(register os_ptr op)
{
    register os_ptr op1 = op - 1;

    /* We can't use the non_int_cases macro, */
    /* because we have to check explicitly for op == 0. */
    switch (r_type(op)) {
	default:
	    return_op_typecheck(op);
	case t_real:
	    if (op->value.realval == 0)
		return_error(e_undefinedresult);
	    switch (r_type(op1)) {
		default:
		    return_op_typecheck(op1);
		case t_real:
		    op1->value.realval /= op->value.realval;
		    break;
		case t_integer:
		    make_real(op1, (double)op1->value.intval / op->value.realval);
	    }
	    break;
	case t_integer:
	    if (op->value.intval == 0)
		return_error(e_undefinedresult);
	    switch (r_type(op1)) {
		default:
		    return_op_typecheck(op1);
		case t_real:
		    op1->value.realval /= (double)op->value.intval;
		    break;
		case t_integer:
		    make_real(op1, (double)op1->value.intval / (double)op->value.intval);
	    }
    }
    pop(1);
    return 0;
}

/* <num1> <num2> mul <product> */
private int
zmul(register os_ptr op)
{
    switch (r_type(op)) {
    default:
	return_op_typecheck(op);
    case t_real:
	switch (r_type(op - 1)) {
	default:
	    return_op_typecheck(op - 1);
	case t_real:
	    op[-1].value.realval *= op->value.realval;
	    break;
	case t_integer:
	    make_real(op - 1, (double)op[-1].value.intval * op->value.realval);
	}
	break;
    case t_integer:
	switch (r_type(op - 1)) {
	default:
	    return_op_typecheck(op - 1);
	case t_real:
	    op[-1].value.realval *= (double)op->value.intval;
	    break;
	case t_integer: {
	    long int1 = op[-1].value.intval;
	    long int2 = op->value.intval;
	    long abs1 = (int1 >= 0 ? int1 : -int1);
	    long abs2 = (int2 >= 0 ? int2 : -int2);
	    float fprod;

	    if ((abs1 > MAX_HALF_INTVAL || abs2 > MAX_HALF_INTVAL) &&
		/* At least one of the operands is very large. */
		/* Check for integer overflow. */
		abs1 != 0 &&
		abs2 > MAX_INTVAL / abs1 &&
		/* Check for the boundary case */
		(fprod = (float)int1 * int2,
		 (int1 * int2 != MIN_INTVAL ||
		  fprod != (float)MIN_INTVAL))
		)
		make_real(op - 1, fprod);
	    else
		op[-1].value.intval = int1 * int2;
	}
	}
    }
    pop(1);
    return 0;
}

/* <num1> <num2> sub <difference> */
/* We make this into a separate procedure because */
/* the interpreter will almost always call it directly. */
int
zop_sub(register os_ptr op)
{
    switch (r_type(op)) {
    default:
	return_op_typecheck(op);
    case t_real:
	switch (r_type(op - 1)) {
	default:
	    return_op_typecheck(op - 1);
	case t_real:
	    op[-1].value.realval -= op->value.realval;
	    break;
	case t_integer:
	    make_real(op - 1, (double)op[-1].value.intval - op->value.realval);
	}
	break;
    case t_integer:
	switch (r_type(op - 1)) {
	default:
	    return_op_typecheck(op - 1);
	case t_real:
	    op[-1].value.realval -= (double)op->value.intval;
	    break;
	case t_integer: {
	    long int1 = op[-1].value.intval;

	    if ((int1 ^ (op[-1].value.intval = int1 - op->value.intval)) < 0 &&
		(int1 ^ op->value.intval) < 0
		) {			/* Overflow, convert to real */
		make_real(op - 1, (float)int1 - op->value.intval);
	    }
	}
	}
    }
    return 0;
}
int
zsub(os_ptr op)
{
    int code = zop_sub(op);

    if (code == 0) {
	pop(1);
    }
    return code;
}

/* <num1> <num2> idiv <int_quotient> */
private int
zidiv(register os_ptr op)
{
    check_type(*op, t_integer);
    check_type(op[-1], t_integer);
    if (op->value.intval == 0)
	return_error(e_undefinedresult);
    if ((op[-1].value.intval /= op->value.intval) ==
	MIN_INTVAL && op->value.intval == -1
	) {			/* Anomalous boundary case, fail. */
	return_error(e_rangecheck);
    }
    pop(1);
    return 0;
}

/* <int1> <int2> mod <remainder> */
private int
zmod(register os_ptr op)
{
    check_type(*op, t_integer);
    check_type(op[-1], t_integer);
    if (op->value.intval == 0)
	return_error(e_undefinedresult);
    op[-1].value.intval %= op->value.intval;
    pop(1);
    return 0;
}

/* <num1> neg <num2> */
private int
zneg(register os_ptr op)
{
    switch (r_type(op)) {
	default:
	    return_op_typecheck(op);
	case t_real:
	    op->value.realval = -op->value.realval;
	    break;
	case t_integer:
	    if (op->value.intval == MIN_INTVAL)
		make_real(op, -(float)MIN_INTVAL);
	    else
		op->value.intval = -op->value.intval;
    }
    return 0;
}

/* <num1> ceiling <num2> */
private int
zceiling(register os_ptr op)
{
    switch (r_type(op)) {
	default:
	    return_op_typecheck(op);
	case t_real:
	    op->value.realval = ceil(op->value.realval);
	case t_integer:;
    }
    return 0;
}

/* <num1> floor <num2> */
private int
zfloor(register os_ptr op)
{
    switch (r_type(op)) {
	default:
	    return_op_typecheck(op);
	case t_real:
	    op->value.realval = floor(op->value.realval);
	case t_integer:;
    }
    return 0;
}

/* <num1> round <num2> */
private int
zround(register os_ptr op)
{
    switch (r_type(op)) {
	default:
	    return_op_typecheck(op);
	case t_real:
	    op->value.realval = floor(op->value.realval + 0.5);
	case t_integer:;
    }
    return 0;
}

/* <num1> truncate <num2> */
private int
ztruncate(register os_ptr op)
{
    switch (r_type(op)) {
	default:
	    return_op_typecheck(op);
	case t_real:
	    op->value.realval =
		(op->value.realval < 0.0 ?
		 ceil(op->value.realval) :
		 floor(op->value.realval));
	case t_integer:;
    }
    return 0;
}

/* Non-standard operators */

/* <int1> <int2> .bitadd <sum> */
private int
zbitadd(register os_ptr op)
{
    check_type(*op, t_integer);
    check_type(op[-1], t_integer);
    op[-1].value.intval += op->value.intval;
    pop(1);
    return 0;
}

/* ------ Initialization table ------ */

const op_def zarith_op_defs[] =
{
    {"2add", zadd},
    {"2.bitadd", zbitadd},
    {"1ceiling", zceiling},
    {"2div", zdiv},
    {"2idiv", zidiv},
    {"1floor", zfloor},
    {"2mod", zmod},
    {"2mul", zmul},
    {"1neg", zneg},
    {"1round", zround},
    {"2sub", zsub},
    {"1truncate", ztruncate},
    op_def_end(0)
};
