/*
  Copyright 1993-2001 by Easy Software Products.
  Copyright 1989, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.

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

/*$Id: zcontrol.c,v 1.3 2001/01/22 15:03:57 mike Exp $ */
/* Control operators */
#include "string_.h"
#include "ghost.h"
#include "stream.h"
#include "oper.h"
#include "estack.h"
#include "files.h"
#include "ipacked.h"
#include "iutil.h"
#include "store.h"

/* Make an invalid file object. */
extern void make_invalid_file(P1(ref *));	/* in zfile.c */

/* Forward references */
private int no_cleanup(P1(os_ptr));
private uint count_exec_stack(P1(bool));
private uint count_to_stopped(P1(long));
private int unmatched_exit(P2(os_ptr, op_proc_p));

/* See the comment in opdef.h for an invariant which allows */
/* more efficient implementation of for, loop, and repeat. */

/* <[test0 body0 ...]> .cond - */
private int cond_continue(P1(os_ptr));
private int
zcond(register os_ptr op)
{
    es_ptr ep = esp;

    /* Push the array on the e-stack and call the continuation. */
    if (!r_is_array(op))
	return_op_typecheck(op);
    check_execute(*op);
    if ((r_size(op) & 1) != 0)
	return_error(e_rangecheck);
    if (r_size(op) == 0)
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
{
    es_ptr ep = esp;
    int code;

    /* The top element of the e-stack is the remaining tail of */
    /* the cond body.  The top element of the o-stack should be */
    /* the (boolean) result of the test that is the first element */
    /* of the tail. */
    check_type(*op, t_boolean);
    if (op->value.boolval) {	/* true */
	array_get(ep, 1L, ep);
	esfile_check_cache();
	code = o_pop_estack;
    } else if (r_size(ep) > 2) {	/* false */
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
    } else {			/* fall off end of cond */
	esp = ep - 1;
	code = o_pop_estack;
    }
    pop(1);			/* get rid of the boolean */
    return code;
}

/* <obj> exec - */
int
zexec(register os_ptr op)
{
    check_op(1);
    if (!r_has_attr(op, a_executable))
	return 0;		/* literal object just gets pushed back */
    check_estack(1);
    ++esp;
    ref_assign(esp, op);
    esfile_check_cache();
    pop(1);
    return o_push_estack;
}

/* <obj1> ... <objn> <n> .execn - */
int
zexecn(register os_ptr op)
{
    uint n, i;
    es_ptr esp_orig;

    check_int_leu(*op, max_uint - 1);
    n = (uint) op->value.intval;
    check_op(n + 1);
    check_estack(n);
    esp_orig = esp;
    for (i = 0; i < n; ++i) {
	const ref *rp = ref_stack_index(&o_stack, (long)(i + 1));

	/* Make sure this object is legal to execute. */
	if (ref_type_uses_access(r_type(rp))) {
	    if (!r_has_attr(rp, a_execute) &&
		r_has_attr(rp, a_executable)
		) {
		esp = esp_orig;
		return_error(e_invalidaccess);
	    }
	}
	/* Executable nulls have a special meaning on the e-stack, */
	/* so since they are no-ops, don't push them. */
	if (!r_has_type_attrs(rp, t_null, a_executable)) {
	    ++esp;
	    ref_assign(esp, rp);
	}
    }
    esfile_check_cache();
    pop(n + 1);
    return o_push_estack;
}

/* <obj> superexec - */
/* THIS IS NOT REALLY IMPLEMENTED YET. */
private int
zsuperexec(os_ptr op)
{
    return zexec(op);
}

/* <bool> <proc> if - */
int
zif(register os_ptr op)
{
    check_type(op[-1], t_boolean);
    check_proc(*op);
    if (op[-1].value.boolval) {
	check_estack(1);
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
{
    check_type(op[-2], t_boolean);
    check_proc(op[-1]);
    check_proc(*op);
    check_estack(1);
    ++esp;
    if (op[-2].value.boolval) {
	ref_assign(esp, op - 1);
    } else {
	ref_assign(esp, op);
    }
    esfile_check_cache();
    pop(3);
    return o_push_estack;
}

/* <init> <step> <limit> <proc> for - */
private int
    for_pos_int_continue(P1(os_ptr)), for_neg_int_continue(P1(os_ptr)),
    for_real_continue(P1(os_ptr));
int
zfor(register os_ptr op)
{
    register es_ptr ep;

    check_estack(7);
    ep = esp + 6;
    check_proc(*op);
    /* Push a mark, the control variable, the initial value, */
    /* the increment, the limit, and the procedure, */
    /* and invoke the continuation operator. */
    if (r_has_type(op - 3, t_integer) &&
	r_has_type(op - 2, t_integer)
	) {
	make_int(ep - 4, op[-3].value.intval);
	make_int(ep - 3, op[-2].value.intval);
	switch (r_type(op - 1)) {
	    case t_integer:
		make_int(ep - 2, op[-1].value.intval);
		break;
	    case t_real:
		make_int(ep - 2, (long)op[-1].value.realval);
		break;
	    default:
		return_op_typecheck(op - 1);
	}
	if (ep[-3].value.intval >= 0)
	    make_op_estack(ep, for_pos_int_continue);
	else
	    make_op_estack(ep, for_neg_int_continue);
    } else {
	float params[3];
	int code;

	if ((code = float_params(op - 1, 3, params)) < 0)
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
{
    register es_ptr ep = esp;
    long var = ep[-3].value.intval;

    if (var > ep[-1].value.intval) {
	esp -= 5;		/* pop everything */
	return o_pop_estack;
    }
    push(1);
    make_int(op, var);
    ep[-3].value.intval = var + ep[-2].value.intval;
    ref_assign_inline(ep + 2, ep);	/* saved proc */
    esp = ep + 2;
    return o_push_estack;
}
/* Continuation operator for negative integers. */
private int
for_neg_int_continue(register os_ptr op)
{
    register es_ptr ep = esp;
    long var = ep[-3].value.intval;

    if (var < ep[-1].value.intval) {
	esp -= 5;		/* pop everything */
	return o_pop_estack;
    }
    push(1);
    make_int(op, var);
    ep[-3].value.intval = var + ep[-2].value.intval;
    ref_assign(ep + 2, ep);	/* saved proc */
    esp = ep + 2;
    return o_push_estack;
}
/* Continuation operator for reals. */
private int
for_real_continue(register os_ptr op)
{
    es_ptr ep = esp;
    float var = ep[-3].value.realval;
    float incr = ep[-2].value.realval;

    if (incr >= 0 ? (var > ep[-1].value.realval) :
	(var < ep[-1].value.realval)
	) {
	esp -= 5;		/* pop everything */
	return o_pop_estack;
    }
    push(1);
    ref_assign(op, ep - 3);
    ep[-3].value.realval = var + incr;
    esp = ep + 2;
    ref_assign(ep + 2, ep);	/* saved proc */
    return o_push_estack;
}

/* Here we provide an internal variant of 'for' that enumerates the */
/* values 0, 1/N, 2/N, ..., 1 precisely.  The arguments must be */
/* the integers 0, 1, and N.  We need this for */
/* loading caches such as the transfer function cache. */
private int for_fraction_continue(P1(os_ptr));
int
zfor_fraction(register os_ptr op)
{
    int code = zfor(op);

    if (code < 0)
	return code;		/* shouldn't ever happen! */
    make_op_estack(esp, for_fraction_continue);
    return code;
}
/* Continuation procedure */
private int
for_fraction_continue(register os_ptr op)
{
    register es_ptr ep = esp;
    int code = for_pos_int_continue(op);

    if (code != o_push_estack)
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
{
    check_type(op[-1], t_integer);
    check_proc(*op);
    if (op[-1].value.intval < 0)
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
{
    es_ptr ep = esp;		/* saved proc */

    if (--(ep[-1].value.intval) >= 0) {		/* continue */
	esp += 2;
	ref_assign(esp, ep);
	return o_push_estack;
    } else {			/* done */
	esp -= 3;		/* pop mark, count, proc */
	return o_pop_estack;
    }
}

/* <proc> loop */
private int loop_continue(P1(os_ptr));
private int
zloop(register os_ptr op)
{
    check_proc(*op);
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
{
    register es_ptr ep = esp;	/* saved proc */

    ref_assign(ep + 2, ep);
    esp = ep + 2;
    return o_push_estack;
}

/* - exit - */
private int
zexit(register os_ptr op)
{
    ref_stack_enum_t rsenum;
    uint scanned = 0;

    ref_stack_enum_begin(&rsenum, &e_stack);
    do {
	uint used = rsenum.size;
	es_ptr ep = rsenum.ptr + used - 1;
	uint count = used;

	for (; count; count--, ep--)
	    if (r_is_estack_mark(ep))
		switch (estack_mark_index(ep)) {
		    case es_for:
			pop_estack(scanned + (used - count + 1));
			return o_pop_estack;
		    case es_stopped:
			return_error(e_invalidexit);	/* not a loop */
		}
	scanned += used;
    } while (ref_stack_enum_next(&rsenum));
    /* No mark, quit.  (per Adobe documentation) */
    push(2);
    return unmatched_exit(op, zexit);
}

/*
 * .stopped pushes the following on the e-stack:
 *      - A mark with type = es_stopped and procedure = no_cleanup.
 *      - The result to be pushed on a normal return.
 *      - The signal mask for .stop.
 *      - The procedure %stopped_push, to handle the normal return case.
 */

/* In the normal (no-error) case, pop the mask from the e-stack, */
/* and move the result to the o-stack. */
private int
stopped_push(register os_ptr op)
{
    push(1);
    *op = esp[-1];
    esp -= 3;
    return o_pop_estack;
}

/* - stop - */
/* Equivalent to true 1 .stop. */
/* This is implemented in C because if were a pseudo-operator, */
/* the stacks would get restored in case of an error. */
private int
zstop(register os_ptr op)
{
    uint count = count_to_stopped(1L);

    if (count) {
	/*
	 * If there are any t_oparrays on the e-stack, they will pop
	 * any new items from the o-stack.  Wait to push the 'true'
	 * until we have run all the unwind procedures.
	 */
	check_ostack(2);
	pop_estack(count);
	op = osp;
	push(1);
	make_true(op);
	return o_pop_estack;
    }
    /* No mark, quit.  (per Adobe documentation) */
    push(2);
    return unmatched_exit(op, zstop);
}

/* <result> <mask> .stop - */
private int
zzstop(register os_ptr op)
{
    uint count;

    check_type(*op, t_integer);
    count = count_to_stopped(op->value.intval);
    if (count) {
	/*
	 * If there are any t_oparrays on the e-stack, they will pop
	 * any new items from the o-stack.  Wait to push the result
	 * until we have run all the unwind procedures.
	 */
	ref save_result;

	check_op(2);
	save_result = op[-1];
	pop(2);
	pop_estack(count);
	op = osp;
	push(1);
	*op = save_result;
	return o_pop_estack;
    }
    /* No mark, quit.  (per Adobe documentation) */
    return unmatched_exit(op, zzstop);
}

/* <obj> stopped <stopped> */
/* Equivalent to false 1 .stopped. */
/* This is implemented in C because if were a pseudo-operator, */
/* the stacks would get restored in case of an error. */
private int
zstopped(register os_ptr op)
{
    check_op(1);
    /* Mark the execution stack, and push the default result */
    /* in case control returns normally. */
    check_estack(5);
    push_mark_estack(es_stopped, no_cleanup);
    ++esp;
    make_false(esp);		/* save the result */
    ++esp;
    make_int(esp, 1);		/* save the signal mask */
    push_op_estack(stopped_push);
    *++esp = *op;		/* execute the operand */
    esfile_check_cache();
    pop(1);
    return o_push_estack;
}

/* <obj> <result> <mask> .stopped <result> */
private int
zzstopped(register os_ptr op)
{
    check_type(*op, t_integer);
    check_op(3);
    /* Mark the execution stack, and push the default result */
    /* in case control returns normally. */
    check_estack(5);
    push_mark_estack(es_stopped, no_cleanup);
    *++esp = op[-1];		/* save the result */
    *++esp = *op;		/* save the signal mask */
    push_op_estack(stopped_push);
    *++esp = op[-2];		/* execute the operand */
    esfile_check_cache();
    pop(3);
    return o_push_estack;
}

/* <mask> .instopped false */
/* <mask> .instopped <result> true */
private int
zinstopped(register os_ptr op)
{
    uint count;

    check_type(*op, t_integer);
    count = count_to_stopped(op->value.intval);
    if (count) {
	push(1);
	op[-1] = *ref_stack_index(&e_stack, count - 2);		/* default result */
	make_true(op);
    } else
	make_false(op);
    return 0;
}

/* <include_marks> .countexecstack <int> */
/* - countexecstack <int> */
/* countexecstack is an operator solely for the sake of the Genoa tests. */
private int
zcountexecstack(register os_ptr op)
{
    push(1);
    make_int(op, count_exec_stack(false));
    return 0;
}
private int
zcountexecstack1(register os_ptr op)
{
    check_type(*op, t_boolean);
    make_int(op, count_exec_stack(op->value.boolval));
    return 0;
}

/* <array> <include_marks> .execstack <subarray> */
/* <array> execstack <subarray> */
/* execstack is an operator solely for the sake of the Genoa tests. */
private int execstack_continue(P1(os_ptr));
private int execstack2_continue(P1(os_ptr));
private int
push_execstack(os_ptr op1, bool include_marks, int (*cont)(P1(os_ptr)))
{
    uint size;
    /*
     * We can't do this directly, because the interpreter
     * might have cached some state.  To force the interpreter
     * to update the stored state, we push a continuation on
     * the exec stack; the continuation is executed immediately,
     * and does the actual transfer.
     */
    uint depth;

    check_write_type(*op1, t_array);
    size = r_size(op1);
    depth = count_exec_stack(include_marks);
    if (depth > size)
	return_error(e_rangecheck);
    {
	int code = ref_stack_store_check(&e_stack, op1, size, 0);

	if (code < 0)
	    return code;
    }
    check_estack(1);
    r_set_size(op1, depth);
    push_op_estack(cont);
    return o_push_estack;
}
private int
zexecstack(register os_ptr op)
{
    return push_execstack(op, false, execstack_continue);
}
private int
zexecstack2(register os_ptr op)
{
    check_type(*op, t_boolean);
    return push_execstack(op - 1, op->value.boolval, execstack2_continue);
}
/* Continuation operator to do the actual transfer. */
/* r_size(op1) was set just above. */
private int
do_execstack(os_ptr op, bool include_marks, os_ptr op1)
{
    ref *arefs = op1->value.refs;
    uint asize = r_size(op1);
    uint i;
    ref *rq;

    /*
     * Copy elements from the stack to the array,
     * optionally skipping executable nulls.
     * Clear the executable bit in any internal operators, and
     * convert t_structs and t_astructs (which can only appear
     * in connection with stack marks, which means that they will
     * probably be freed when unwinding) to something harmless.
     */
    for (i = 0, rq = arefs + asize; rq != arefs; ++i) {
	const ref *rp = ref_stack_index(&e_stack, (long)i);

	if (r_has_type_attrs(rp, t_null, a_executable) && !include_marks)
	    continue;
	--rq;
	ref_assign_old(op1, rq, rp, "execstack");
	switch (r_type(rq)) {
	    case t_operator: {
		uint opidx = op_index(rq);

		if (opidx == 0 || op_def_is_internal(op_def_table[opidx]))
		    r_clear_attrs(rq, a_executable);
		break;
	    }
	    case t_struct:
	    case t_astruct: {
		const char *tname =
		    gs_struct_type_name_string(
				gs_object_type(imemory, rq->value.pstruct));

		make_const_string(rq, a_readonly | avm_foreign,
				  strlen(tname), (const byte *)tname);
		break;
	    }
	    default:
		;
	}
    }
    pop(op - op1);
    return 0;
}
private int
execstack_continue(os_ptr op)
{
    return do_execstack(op, false, op);
}
private int
execstack2_continue(os_ptr op)
{
    return do_execstack(op, op->value.boolval, op - 1);
}

/* - .needinput - */
private int
zneedinput(register os_ptr op)
{
    return e_NeedInput;		/* interpreter will exit to caller */
}

/* <obj> <int> .quit - */
private int
zquit(register os_ptr op)
{
    check_op(2);
    check_type(*op, t_integer);
    return_error(e_Quit);	/* Interpreter will do the exit */
}

/* - currentfile <file> */
private ref *zget_current_file(P0());
private int
zcurrentfile(register os_ptr op)
{
    ref *fp;

    push(1);
    /* Check the cache first */
    if (esfile != 0) {
#ifdef DEBUG
	/* Check that esfile is valid. */
	ref *efp = zget_current_file();

	if (esfile != efp) {
	    lprintf2("currentfile: esfile=0x%lx, efp=0x%lx\n",
		     (ulong) esfile, (ulong) efp);
	    ref_assign(op, efp);
	} else
#endif
	    ref_assign(op, esfile);
    } else if ((fp = zget_current_file()) == 0) {	/* Return an invalid file object. */
	/* This doesn't make a lot of sense to me, */
	/* but it's what the PostScript manual specifies. */
	make_invalid_file(op);
    } else {
	ref_assign(op, fp);
	esfile_set_cache(fp);
    }
    /* Make the returned value literal. */
    r_clear_attrs(op, a_executable);
    return 0;
}
/* Get the current file from which the interpreter is reading. */
private ref *
zget_current_file(void)
{
    ref_stack_enum_t rsenum;

    ref_stack_enum_begin(&rsenum, &e_stack);
    do {
	uint count = rsenum.size;
	es_ptr ep = rsenum.ptr + count - 1;

	for (; count; count--, ep--)
	    if (r_has_type_attrs(ep, t_file, a_executable))
		return ep;
    } while (ref_stack_enum_next(&rsenum));
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zcontrol_op_defs[] =
{
    {"1.cond", zcond},
    {"0countexecstack", zcountexecstack},
    {"1.countexecstack", zcountexecstack1},
    {"0currentfile", zcurrentfile},
    {"1exec", zexec},
    {"1.execn", zexecn},
    {"1execstack", zexecstack},
    {"2.execstack", zexecstack2},
    {"0exit", zexit},
    {"2if", zif},
    {"3ifelse", zifelse},
    {"0.instopped", zinstopped},
    {"0.needinput", zneedinput},
    {"4for", zfor},
    {"1loop", zloop},
    {"2.quit", zquit},
    {"2repeat", zrepeat},
    {"0stop", zstop},
    {"1.stop", zzstop},
    {"1stopped", zstopped},
    {"2.stopped", zzstopped},
		/* Internal operators */
    {"1%cond_continue", cond_continue},
    {"1%execstack_continue", execstack_continue},
    {"2%execstack2_continue", execstack2_continue},
    {"0%for_pos_int_continue", for_pos_int_continue},
    {"0%for_neg_int_continue", for_neg_int_continue},
    {"0%for_real_continue", for_real_continue},
    {"4%for_fraction", zfor_fraction},
    {"0%for_fraction_continue", for_fraction_continue},
    {"0%loop_continue", loop_continue},
    {"0%repeat_continue", repeat_continue},
    {"0%stopped_push", stopped_push},
    {"1superexec", zsuperexec},
    op_def_end(0)
};

/* ------ Internal routines ------ */

/* Vacuous cleanup routine */
private int
no_cleanup(os_ptr op)
{
    return 0;
}

/*
 * Count the number of elements on the exec stack, with or without
 * the normally invisible elements (*op is a Boolean that indicates this).
 */
private uint
count_exec_stack(bool include_marks)
{
    uint count = ref_stack_count(&e_stack);

    if (!include_marks) {
	uint i;

	for (i = count; i--;)
	    if (r_has_type_attrs(ref_stack_index(&e_stack, (long)i),
				 t_null, a_executable))
		--count;
    }
    return count;
}

/*
 * Count the number of elements down to and including the first 'stopped'
 * mark on the e-stack with a given mask.  Return 0 if there is no 'stopped'
 * mark.
 */
private uint
count_to_stopped(long mask)
{
    ref_stack_enum_t rsenum;
    uint scanned = 0;

    ref_stack_enum_begin(&rsenum, &e_stack);
    do {
	uint used = rsenum.size;
	es_ptr ep = rsenum.ptr + used - 1;
	uint count = used;

	for (; count; count--, ep--)
	    if (r_is_estack_mark(ep) &&
		estack_mark_index(ep) == es_stopped &&
		(ep[2].value.intval & mask) != 0
		)
		return scanned + (used - count + 1);
	scanned += used;
    } while (ref_stack_enum_next(&rsenum));
    return 0;
}

/*
 * Pop the e-stack, executing cleanup procedures as needed.
 * We could make this more efficient using ref_stack_enum_*,
 * but it isn't used enough to make this worthwhile.
 */
void
pop_estack(uint count)
{
    uint idx = 0;
    uint popped = 0;

    esfile_clear_cache();
    for (; idx < count; idx++) {
	ref *ep = ref_stack_index(&e_stack, idx - popped);

	if (r_is_estack_mark(ep)) {
	    ref_stack_pop(&e_stack, idx + 1 - popped);
	    popped = idx + 1;
	    (*real_opproc(ep)) (osp);
	}
    }
    ref_stack_pop(&e_stack, count - popped);
}

/*
 * Execute a quit in the case of an exit or stop with no appropriate
 * enclosing control scope (loop or stopped).  The caller has already
 * ensured two free slots on the top of the o-stack.
 */
private int
unmatched_exit(os_ptr op, op_proc_p opproc)
{
    make_oper(op - 1, 0, opproc);
    make_int(op, e_invalidexit);
    return_error(e_Quit);
}
