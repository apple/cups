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

/* estack.h */
/* Definitions for the execution stack */
#include "istack.h"

/* Define the execution stack pointers. */
typedef s_ptr es_ptr;
typedef const_s_ptr const_es_ptr;
extern ref_stack e_stack;
#define esbot (e_stack.bot)
#define esp (e_stack.p)
#define estop (e_stack.top)

/*
 * To improve performance, we cache the currentfile pointer
 * (i.e., `shallow-bind' it in Lisp terminology).  The invariant is as
 * follows: either esfile points to the currentfile slot on the estack
 * (i.e., the topmost slot with an executable file), or it is 0.
 * To maintain the invariant, it is sufficient that whenever a routine
 * pushes or pops anything on the estack, if the object *might* be
 * an executable file, invoke esfile_clear_cache(); alternatively,
 * immediately after pushing an object, invoke esfile_check_cache().
 */
extern ref *esfile;
#define esfile_clear_cache() (esfile = 0)
#define esfile_set_cache(pref) (esfile = (pref))
#define esfile_check_cache()\
  if ( r_has_type_attrs(esp, t_file, a_executable) )\
    esfile_set_cache(esp)

/*
 * The execution stack is used for three purposes:
 *
 *	- Procedures being executed are held here.  They always have
 * type = t_array, t_mixedarray, or t_shortarray, with a_executable set.
 * More specifically, the e-stack holds the as yet unexecuted tail of the
 * procedure.
 *
 *	- if, ifelse, etc. push arguments to be executed here.
 * They may be any kind of object whatever.
 *
 *	- Control operators (filenameforall, for, repeat, loop, forall,
 * pathforall, run, stopped, ...) mark the stack by pushing
 * an object with type = t_null, attrs = a_executable, size = es_xxx
 * (see below), and value.opproc = a cleanup procedure that will get called
 * whenever the execution stack is about to get cut back beyond this point
 * (either for normal completion of the operator, or any kind of exit).
 * (Executable null objects can't ever appear on the e-stack otherwise:
 * if a control operator pushes one, it gets popped immediately.)
 * The cleanup procedure is called with esp pointing just BELOW the mark,
 * i.e., the mark has already been popped.
 *
 * The loop operators also push whatever state they need,
 * followed by an operator object that handles continuing the loop.
 *
 * Note that there are many internal looping operators -- for example,
 * all the 'show' operators can behave like loops, since they may call out
 * to BuildChar procedures.
 */

/* Macro for marking the execution stack */
#define make_mark_estack(ep, es_idx, proc)\
  make_tasv(ep, t_null, a_executable, es_idx, opproc, proc)
#define push_mark_estack(es_idx, proc)\
  (++esp, make_mark_estack(esp, es_idx, proc))
#define r_is_estack_mark(ep)\
  r_has_type_attrs(ep, t_null, a_executable)
#define estack_mark_index(ep) r_size(ep)

/* Macro for pushing an operator on the execution stack */
/* to represent a continuation procedure */
#define make_op_estack(ep, proc)\
  make_oper(ep, 0, proc)
#define push_op_estack(proc)\
  (++esp, make_op_estack(esp, proc))

/* Macro to ensure enough room on the execution stack */
#define check_estack(n)\
  if ( esp > estop - (n) )\
    { int es_code_ = ref_stack_extend(&e_stack, n);\
      if ( es_code_ < 0 ) return es_code_;\
    }

/* Macro to ensure enough entries on the execution stack */
#define check_esp(n)\
  if ( esp < esbot + ((n) - 1) )\
    { e_stack.requested = (n); return_error(e_ExecStackUnderflow); }

/* Define the various kinds of execution stack marks. */
#define es_other 0			/* internal use */
#define es_show 1			/* show operators */
#define es_for 2			/* iteration operators */
#define es_stopped 3			/* stopped operator */

/* Pop a given number of elements off the execution stack, */
/* executing cleanup procedures as necessary. */
void	pop_estack(P1(uint));

/*
 * The execution stack is implemented as a linked list of blocks;
 * operators that can push or pop an unbounded number of values, or that
 * access the entire o-stack, must take this into account.  These are:
 *	exit  .stop  .instopped  countexecstack  execstack  currentfile
 *	pop_estack(exit, stop, error recovery)
 *	gs_show_find(all the show operators)
 * In addition, for e-stack entries created by control operators, we must
 * ensure that the mark and its data are never separated.  We do this
 * by ensuring that when splitting the top block, at least N items
 * are kept in the new top block above the bottommost retained mark,
 * where N is the largest number of data items associated with a mark.
 * Finally, in order to avoid specific checks for underflowing a block,
 * we put a guard entry at the bottom of each block except the top one
 * that contains a procedure that returns an internal "exec stack block
 * underflow" error.
 */
