/* Copyright (C) 1989, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* oper.h */
/* Definitions for Ghostscript operators */
#include "ostack.h"
#include "opdef.h"
#include "opextern.h"
#include "opcheck.h"
#include "iutil.h"

/*
 * In order to combine typecheck and stackunderflow error checking
 * into a single test, we guard the bottom of the o-stack with
 * additional entries of type t__invalid.  However, if a type check fails,
 * we must make an additional check to determine which error
 * should be reported.  In order not to have to make this check in-line
 * in every type check in every operator, we define a procedure that takes
 * an o-stack pointer and returns e_stackunderflow if it points to
 * a guard entry, e_typecheck otherwise.
 *
 * Note that we only need to do this for typecheck, not for any other
 * kind of error such as invalidaccess, since any operator that can
 * generate the latter will do a check_type or a check_op first.
 * (See ostack.h for more information.)
 *
 * We define the operand type of check_type_failed as const ref * rather than
 * const_os_ptr simply because there are a number of routines that might
 * be used either with a stack operand or with a general operand, and
 * the check for t__invalid is harmless when applied to off-stack refs.
 */
int check_type_failed(P1(const ref *));

/*
 * Check the type of an object.  Operators almost always use check_type,
 * which includes the stack underflow check described just above;
 * check_type_only is for checking subsidiary objects obtained from
 * places other than the stack.
 */
#define return_op_typecheck(op)\
  return_error(check_type_failed(op))
#define check_type(orf,typ)\
  if ( !r_has_type(&orf,typ) ) return_op_typecheck(&orf)
#define check_stype(orf,styp)\
  if ( !r_has_stype(&orf,imemory,styp) ) return_op_typecheck(&orf)
#define check_array(orf)\
  check_array_else(orf, return_op_typecheck(&orf))
#define check_type_access(orf,typ,acc1)\
  if ( !r_has_type_attrs(&orf,typ,acc1) )\
    return_error((!r_has_type(&orf,typ) ? check_type_failed(&orf) :\
		  e_invalidaccess))
#define check_read_type(orf,typ)\
  check_type_access(orf,typ,a_read)
#define check_write_type(orf,typ)\
  check_type_access(orf,typ,a_write)

/* Macro for as yet unimplemented operators. */
/* The if ( 1 ) is to prevent the compiler from complaining about */
/* unreachable code. */
#define NYI(msg) if ( 1 ) return_error(e_undefined)

/*
 * If an operator has popped or pushed something on the control stack,
 * it must return o_pop_estack or o_push_estack respectively,
 * rather than 0, to indicate success.
 * It is OK to return o_pop_estack if nothing has been popped,
 * but it is not OK to return o_push_estack if nothing has been pushed.
 *
 * If an operator has suspended the current context and wants the
 * interpreter to call the scheduler, it must return o_reschedule.
 * It may also have pushed or popped elements on the control stack.
 * (This is only used when the Display PostScript option is included.)
 *
 * These values must be greater than 1, and far enough apart from zero and
 * from each other not to tempt a compiler into implementing a 'switch'
 * on them using indexing rather than testing.
 */
#define o_push_estack 5
#define o_pop_estack 14
#define o_reschedule 22
