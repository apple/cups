/* Copyright (C) 1989, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zcontrol.c */
/* Control operators */
#include "string_.h"
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "estack.h"
#include "ipacked.h"
#include "iutil.h"
#include "store.h"

/* Make an invalid file object. */
extern void make_invalid_file(P1(ref *)); /* in zfile.c */

/* Forward references */
int no_cleanup(P1(os_ptr));
private uint count_to_stopped(P0());

/* See the comment in opdef.h for an invariant which allows */
/* more efficient implementation of for, loop, and repeat. */

/* <[test0 body0 ...]> .cond - */
private int cond_continue(P1(os_ptr));
private int
zcond(register os_ptr op)
{	es_ptr ep = esp;
	/* Push the array on the e-stack and call the continuation. */
	if ( !r_is_array(op) )
	  return_op_typecheck(op);
	check_execute(*op);
	if ( (r_size(op) & 1) != 0)
	  return_error(e_rangecheck);
	if ( r_size(op) == 0 )
	  return zpop(op);
	check_estack(3);
	esp = ep += 3;
	ref_assign(ep - 2, op);	/* the cond body */
	make_op_estack(ep - 1, cond_continue);
	array_get(op, 0L, ep);
	esfile_check_cache();
	pop(1);
	return o_push_estack;
}
private int
cond_continue(register os_ptr op)
{	es_ptr ep = esp;
	int code;
	/* The top element of the e-stack is the remaining tail of */
	/* the cond body.  The top element of the o-stack should be */
	/* the (boolean) result of the test that is the first element */
	/* of the tail. */
	check_type(*op, t_boolean);
	if ( op->value.boolval )
	  {				/* true */
		array_get(ep, 1L, ep);
		esfile_check_cache();
		code = o_pop_estack;
	  }
	else if ( r_size(ep) > 2 )
	  {				/* false */
		const ref_packed *elts = ep->value.packed;
		check_estack(2);
		r_dec_size(ep, 2);
		elts = packed_next(elts);
		elts = packed_next(elts);
		ep->value.packed = elts;
		array_get(ep, 0L, ep + 2);
		make_op_estack(ep + 1, cond_continue);
		esp = ep + 2;
		esfile_check_cache();
		code = o_push_estack;
	  }
	else
	  {				/* fall off end of cond */
		esp = ep - 1;
		code = o_pop_estack;
	  }
	pop(1);			/* get rid of the boolean */
	return code;
}

/* <obj> exec - */
int
zexec(register os_ptr op)
{	check_op(1);
	if ( !r_has_attr(op, a_executable) )
	  return 0;		/* literal object just gets pushed back */
	check_estack(1);
	++esp;
	ref_assign(esp, op);
	esfile_check_cache();
	pop(1);
	return o_push_estack;
}

/* <obj> superexec - */
/* THIS IS NOT REALLY IMPLEMENTED YET. */
private int
zsuperexec(os_ptr op)
{	return zexec(op);
}

/* <bool> <proc> if - */
int
zif(register os_ptr op)
{	check_type(op[-1], t_boolean);
	check_proc(*op);
	if ( op[-1].value.boolval )
	   {	check_estack(1);
		++esp;
		ref_assign(esp, op);
		esfile_check_cache();
	   }
	pop(2);
	return o_push_estack;
}

/* <bool> <proc_true> <proc_false> ifelse - */
int
zifelse(register os_ptr op)
{	check_type(op[-2], t_boolean);
	check_proc(op[-1]);
	check_proc(*op);
	check_estack(1);
	++esp;
	if ( op[-2].value.boolval )
	   {	ref_assign(esp, op - 1);
	   }
	else
	   {	ref_assign(esp, op);
	   }
	esfile_check_cache();
	pop(3);
	return o_push_estack;
}

/* <init> <step> <limit> <proc> for - */
private int
  for_pos_int_continue(P1(os_ptr)),
  for_neg_int_continue(P1(os_ptr)),
  for_real_continue(P1(os_ptr));
int
zfor(register os_ptr op)
{	register es_ptr ep;
	check_estack(7);
	ep = esp + 6;
	check_proc(*op);
	/* Push a mark, the control variable, the initial value, */
	/* the increment, the limit, and the procedure, */
	/* and invoke the continuation operator. */
	if ( r_has_type(op - 3, t_integer) &&
	     r_has_type(op - 2, t_integer)
	   )
	  {	make_int(ep - 4, op[-3].value.intval);
		make_int(ep - 3, op[-2].value.intval);
		switch ( r_type(op - 1) )
		  {
		  case t_integer:
		    make_int(ep - 2, op[-1].value.intval);
		    break;
		  case t_real:
		    make_int(ep - 2, (long)op[-1].value.realval);
		    break;
		  default:
		    return_op_typecheck(op - 1);
		  }
		if ( ep[-3].value.intval >= 0 )
		  make_op_estack(ep, for_pos_int_continue);
		else
		  make_op_estack(ep, for_neg_int_continue);
	  }
	else
	  {	float params[3];
		int code;
		if ( (code = num_params(op - 1, 3, params)) < 0 )
		  return code;
		make_real(ep - 4, params[0]);
		make_real(ep - 3, params[1]);
		make_real(ep - 2, params[2]);
		make_op_estack(ep, for_real_continue);
	  }
	make_mark_estack(ep - 5, es_for, no_cleanup);
	ref_assign(ep - 1, op);
	esp = ep;
	pop(4);
	return o_push_estack;
}
/* Continuation operators for for, separate for positive integer, */
/* negative integer, and real. */
/* Execution stack contains mark, control variable, increment, */
/* limit, and procedure (procedure is topmost.) */
/* Continuation operator for positive integers. */
private int
for_pos_int_continue(register os_ptr op)
{	register es_ptr ep = esp;
	long var = ep[-3].value.intval;
	if ( var > ep[-1].value.intval )
	   {	esp -= 5;	/* pop everything */
		return o_pop_estack;
	   }
	push(1);
	make_int(op, var);
	ep[-3].value.intval = var + ep[-2].value.intval;
	ref_assign_inline(ep + 2, ep);		/* saved proc */
	esp = ep + 2;
	return o_push_estack;
}
/* Continuation operator for negative integers. */
private int
for_neg_int_continue(register os_ptr op)
{	register es_ptr ep = esp;
	long var = ep[-3].value.intval;
	if ( var < ep[-1].value.intval )
	   {	esp -= 5;	/* pop everything */
		return o_pop_estack;
	   }
	push(1);
	make_int(op, var);
	ep[-3].value.intval = var + ep[-2].value.intval;
	ref_assign(ep + 2, ep);		/* saved proc */
	esp = ep + 2;
	return o_push_estack;
}
/* Continuation operator for reals. */
private int
for_real_continue(register os_ptr op)
{	es_ptr ep = esp;
	float var = ep[-3].value.realval;
	float incr = ep[-2].value.realval;
	if ( incr >= 0 ? (var > ep[-1].value.realval) :
	    (var < ep[-1].value.realval)
	   )
	  {	esp -= 5;	/* pop everything */
		return o_pop_estack;
	  }
	push(1);
	ref_assign(op, ep - 3);
	ep[-3].value.realval = var + incr;
	esp = ep + 2;
	ref_assign(ep + 2, ep);		/* saved proc */
	return o_push_estack;
}

/* Here we provide an internal variant of 'for' that enumerates the */
/* values 0, 1/N, 2/N, ..., 1 precisely.  The arguments must be */
/* the integers 0, 1, and N.  We need this for */
/* loading caches such as the transfer function cache. */
private int for_fraction_continue(P1(os_ptr));
int
zfor_fraction(register os_ptr op)
{	int code = zfor(op);
	if ( code < 0 ) return code;	/* shouldn't ever happen! */
	make_op_estack(esp, for_fraction_continue);
	return code;
}
/* Continuation procedure */
private int
for_fraction_continue(register os_ptr op)
{	register es_ptr ep = esp;
	int code = for_pos_int_continue(op);
	if ( code != o_push_estack )
	  return code;
	/* We must use osp instead of op here, because */
	/* for_pos_int_continue pushes a value on the o-stack. */
	make_real(osp, (float)osp->value.intval / ep[-1].value.intval);
	return code;
}

/* <int> <proc> repeat - */
private int repeat_continue(P1(os_ptr));
private int
zrepeat(register os_ptr op)
{	check_type(op[-1], t_integer);
	check_proc(*op);
	if ( op[-1].value.intval < 0 )
		return_error(e_rangecheck);
	check_estack(5);
	/* Push a mark, the count, and the procedure, and invoke */
	/* the continuation operator. */
	push_mark_estack(es_for, no_cleanup);
	*++esp = op[-1];
	*++esp = *op;
	make_op_estack(esp + 1, repeat_continue);
	pop(2);
	return repeat_continue(op - 2);
}
/* Continuation operator for repeat */
private int
repeat_continue(register os_ptr op)
{	es_ptr ep = esp;		/* saved proc */
	if ( --(ep[-1].value.intval) >= 0 )	/* continue */
	   {	esp += 2;
		ref_assign(esp, ep);
		return o_push_estack;
	   }
	else				/* done */
	   {	esp -= 3;		/* pop mark, count, proc */
		return o_pop_estack;
	   }
}

/* <proc> loop */
private int loop_continue(P1(os_ptr));
private int
zloop(register os_ptr op)
{	check_proc(*op);
	check_estack(4);
	/* Push a mark and the procedure, and invoke */
	/* the continuation operator. */
	push_mark_estack(es_for, no_cleanup);
	*++esp = *op;
	make_op_estack(esp + 1, loop_continue);
	pop(1);
	return loop_continue(op - 1);
}
/* Continuation operator for loop */
private int
loop_continue(register os_ptr op)
{	register es_ptr ep = esp;		/* saved proc */
	ref_assign(ep + 2, ep);
	esp = ep + 2;
	return o_push_estack;
}

/* - exit - */
private int
zexit(register os_ptr op)
{	uint scanned = 0;
	STACK_LOOP_BEGIN(&e_stack, ep, used)
	{	uint count = used;
		ep += used - 1;
		for ( ; count; count--, ep-- )
		  if ( r_is_estack_mark(ep) )
			switch ( estack_mark_index(ep) )
			{
			case es_for:
				pop_estack(scanned + (used - count + 1));
				return o_pop_estack;
			case es_stopped:
				return_error(e_invalidexit);	/* not a loop */
			}
		scanned += used;
	}
	STACK_LOOP_END(ep, used)
	/* Return e_invalidexit if there is no mark at all. */
	/* This is different from PostScript, which aborts. */
	/* It shouldn't matter in practice. */
	return_error(e_invalidexit);
}

/* <result> .stop - */
private int
zstop(register os_ptr op)
{	uint count = count_to_stopped();
	if ( count )
	{	pop_estack((uint)count);
		return o_pop_estack;
	}
	/* Return e_invalidexit if there is no mark at all. */
	/* This is different from PostScript, which aborts. */
	/* It shouldn't matter in practice. */
	return_error(e_invalidexit);
}

/* <obj> <result> .stopped <result> */
private int
zstopped(register os_ptr op)
{	check_op(2);
	/* Mark the execution stack, and push the default result */
	/* in case control returns normally. */
	check_estack(3);
	push_mark_estack(es_stopped, no_cleanup);
	*++esp = *op;			/* save the result */
	*++esp = op[-1];		/* execute the operand */
	esfile_check_cache();
	pop(2);
	return o_push_estack;
}

/* - .instopped false */
/* - .instopped <result> true */
private int
zinstopped(register os_ptr op)
{	uint count = count_to_stopped();
	if ( count )
	{	push(2);
		op[-1] = *ref_stack_index(&e_stack, count - 2);	/* default result */
		make_true(op);
	}
	else
	{	push(1);
		make_false(op);
	}
	return 0;
}

/* - countexecstack <int> */
private int
zcountexecstack(register os_ptr op)
{	push(1);
	make_int(op, ref_stack_count(&e_stack));
	return 0;
}

/* <array> execstack <subarray> */
private int execstack_continue(P1(os_ptr));
private int
zexecstack(register os_ptr op)
{	/*
	 * We can't do this directly, because the interpreter
	 * might have cached some state.  To force the interpreter
	 * to update the stored state, we push a continuation on
	 * the exec stack; the continuation is executed immediately,
	 * and does the actual transfer.
	 */
	uint depth = ref_stack_count(&e_stack);

	check_write_type(*op, t_array);
	if ( depth > r_size(op) )
	  return_error(e_rangecheck);
	check_estack(1);
	r_set_size(op, (uint)depth);
	push_op_estack(execstack_continue);
	return o_push_estack;
}
/* Continuation operator to do the actual transfer. */
/* r_size(op) was set just above. */
private int
execstack_continue(register os_ptr op)
{	int code =
	  ref_stack_store(&e_stack, op, r_size(op), 0, 0, true, "execstack");
	uint asize = r_size(op);
	uint i;
	ref *rp;

	if ( code < 0 )
	  return code;
	/*
	 * Clear the executable bit in any internal operators, and
	 * convert t_structs and t_astructs (which can only appear
	 * in connection with stack marks, which means that they will
	 * probably be freed when unwinding) to something harmless.
	 */
	for ( i = 0, rp = op->value.refs; i < asize; i++, rp++ )
	  switch ( r_type(rp) )
	    {
	    case t_operator:
	      {	uint opidx = op_index(rp);
		if ( opidx == 0 || op_def_is_internal(op_def_table[opidx]) )
		  r_clear_attrs(rp, a_executable);
		break;
	      }
	    case t_struct:
	    case t_astruct:
	      {	const char *tname =
		  gs_struct_type_name_string(gs_object_type(imemory,
							  rp->value.pstruct));
		make_const_string(rp, a_readonly | avm_foreign,
				  strlen(tname), (const byte *)tname);
		break;
	      }
	    }
	return 0;
}

/* - .needinput - */
private int
zneedinput(register os_ptr op)
{	return e_NeedInput;	/* interpreter will exit to caller */
}

/* <obj> <int> .quit - */
private int
zquit(register os_ptr op)
{	check_op(2);
	check_type(*op, t_integer);
	return e_Quit;		/* Interpreter will do the exit */
}

/* - currentfile <file> */
private ref *zget_current_file(P0());
private int
zcurrentfile(register os_ptr op)
{	ref *fp;
	push(1);
	/* Check the cache first */
	if ( esfile != 0 )
	{
#ifdef DEBUG
		/* Check that esfile is valid range. */
		ref *efp = zget_current_file();
		if ( esfile != efp )
		  { lprintf2("currentfile: esfile=0x%lx, efp=0x%lx\n",
			     (ulong)esfile, (ulong)efp);
		    ref_assign(op, efp);
		  }
		else
#endif
		ref_assign(op, esfile);
	}
	else if ( (fp = zget_current_file()) == 0 )
	{	/* Return an invalid file object. */
		/* This doesn't make a lot of sense to me, */
		/* but it's what the PostScript manual specifies. */
		make_invalid_file(op);
	}
	else
	{	ref_assign(op, fp);
		esfile_set_cache(fp);
	}
	/* Make the returned value literal. */
	r_clear_attrs(op, a_executable);
	return 0;
}
/* Get the current file from which the interpreter is reading. */
private ref *
zget_current_file(void)
{	STACK_LOOP_BEGIN(&e_stack, ep, used)
	{	uint count = used;
		ep += used - 1;
		for ( ; count; count--, ep-- )
		  if ( r_has_type_attrs(ep, t_file, a_executable) )
		    return ep;
	}
	STACK_LOOP_END(ep, used)
	return 0;
}

/* ------ Non-operator routines ------ */

/* Test whether we are inside a `stopped'. */
/* The top level of the interpreter uses this. */
int
in_stopped(void)
{	return count_to_stopped() != 0;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zcontrol_op_defs) {
	{"1.cond", zcond},
	{"0countexecstack", zcountexecstack},
	{"0currentfile", zcurrentfile},
	{"1exec", zexec},
	{"0execstack", zexecstack},
	{"0exit", zexit},
	{"2if", zif},
	{"3ifelse", zifelse},
	{"0.instopped", zinstopped},
	{"0.needinput", zneedinput},
	{"4for", zfor},
	{"1loop", zloop},
	{"2.quit", zquit},
	{"2repeat", zrepeat},
	{"1.stop", zstop},
	{"2.stopped", zstopped},
		/* Internal operators */
	{"1%cond_continue", cond_continue},
	{"0%execstack_continue", execstack_continue},
	{"0%for_pos_int_continue", for_pos_int_continue},
	{"0%for_neg_int_continue", for_neg_int_continue},
	{"0%for_real_continue", for_real_continue},
	{"4%for_fraction", zfor_fraction},
	{"0%for_fraction_continue", for_fraction_continue},
	{"0%loop_continue", loop_continue},
	{"0%repeat_continue", repeat_continue},
		/* Operators defined in internaldict */
		op_def_begin_dict("internaldict"),
	{"1superexec", zsuperexec},
END_OP_DEFS(0) }

/* ------ Internal routines ------ */

/* Vacuous cleanup routine */
int
no_cleanup(os_ptr op)
{	return 0;
}

/* Count the number of elements down to and including the first 'stopped' */
/* mark on the e-stack.  Return 0 if there is no 'stopped' mark. */
private uint
count_to_stopped(void)
{	uint scanned = 0;
	STACK_LOOP_BEGIN(&e_stack, ep, used)
	{	uint count = used;
		ep += used - 1;
		for ( ; count; count--, ep-- )
		  if ( r_is_estack_mark(ep) &&
		       estack_mark_index(ep) == es_stopped
		     )
			return scanned + (used - count + 1);
		scanned += used;
	}
	STACK_LOOP_END(ep, used)
	return 0;
}

/* Pop the e-stack, executing cleanup procedures as needed. */
/* We could make this more efficient using the STACK_LOOP macros, */
/* but it isn't used enough to make this worthwhile. */
void
pop_estack(uint count)
{	uint idx = 0;
	uint popped = 0;
	esfile_clear_cache();
	for ( ; idx < count; idx++ )
	{	ref *ep = ref_stack_index(&e_stack, idx - popped);
		if ( r_is_estack_mark(ep) )
		{	ref_stack_pop(&e_stack, idx + 1 - popped);
			popped = idx + 1;
			(*real_opproc(ep))(osp);
		}
	}
	ref_stack_pop(&e_stack, count - popped);
}
