/* Copyright (C) 1993, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* opcheck.h */
/* Operand checking for Ghostscript operators */
/* Requires ialloc.h (for imemory), iref.h, errors.h */

/*
 * Check the type of an object.  Operators almost always use check_type,
 * which is defined in oper.h; check_type_only is for checking
 * subsidiary objects obtained from places other than the stack.
 */
#define check_type_only(rf,typ)\
  if ( !r_has_type(&rf,typ) ) return_error(e_typecheck)
#define check_stype_only(rf,styp)\
  if ( !r_has_stype(&rf,imemory,styp) ) return_error(e_typecheck)
/* Check for array */
#define check_array_else(rf,errstat)\
  if ( !r_has_type(&rf, t_array) ) errstat
#define check_array_only(rf)\
  check_array_else(rf, return_error(e_typecheck))
/* Check for procedure.  check_proc_failed includes the stack underflow */
/* check, but it doesn't do any harm in the off-stack case. */
int check_proc_failed(P1(const ref *));
#define check_proc(rf)\
  if ( !r_is_proc(&rf) ) return_error(check_proc_failed(&rf));
#define check_proc_only(rf) check_proc(rf)

/* Check for read, write, or execute access. */
#define check_access(rf,acc1)\
  if ( !r_has_attr(&rf,acc1) ) return_error(e_invalidaccess)
#define check_read(rf) check_access(rf,a_read)
#define check_write(rf) check_access(rf,a_write)
#define check_execute(rf) check_access(rf,a_execute)
#define check_type_access_only(rf,typ,acc1)\
  if ( !r_has_type_attrs(&rf,typ,acc1) )\
    return_error((!r_has_type(&rf,typ) ? e_typecheck : e_invalidaccess))
#define check_read_type_only(rf,typ)\
  check_type_access_only(rf,typ,a_read)
#define check_write_type_only(rf,typ)\
  check_type_access_only(rf,typ,a_write)

/* Check for an integer value within an unsigned bound. */
#define check_int_leu(orf, u)\
  check_type(orf, t_integer);\
  if ( (ulong)(orf).value.intval > (u) ) return_error(e_rangecheck)
#define check_int_leu_only(rf, u)\
  check_type_only(rf, t_integer);\
  if ( (ulong)(rf).value.intval > (u) ) return_error(e_rangecheck)
#define check_int_ltu(orf, u)\
  check_type(orf, t_integer);\
  if ( (ulong)(orf).value.intval >= (u) ) return_error(e_rangecheck)
