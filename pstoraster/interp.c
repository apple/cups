/* Copyright (C) 1989, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: interp.c,v 1.2 2000/03/08 23:15:14 mike Exp $ */
/* Ghostscript language interpreter */
#include "memory_.h"
#include "string_.h"
#include "ghost.h"
#include "gsstruct.h"		/* for iastruct.h */
#include "stream.h"
#include "errors.h"
#include "estack.h"
#include "ialloc.h"
#include "iastruct.h"
#include "icontext.h"
#include "inamedef.h"
#include "iname.h"		/* for the_name_table */
#include "interp.h"
#include "ipacked.h"
#include "ostack.h"		/* must precede iscan.h */
#include "strimpl.h"		/* for sfilter.h */
#include "sfilter.h"		/* for iscan.h */
#include "iscan.h"
#include "idict.h"
#include "isave.h"
#include "istack.h"
#include "iutil.h"		/* for array_get */
#include "ivmspace.h"
#include "dstack.h"
#include "files.h"		/* for file_check_read */
#include "oper.h"
#include "store.h"

/*
 * We may or may not optimize the handling of the special fast operators
 * in packed arrays.  If we do this, they run much faster when packed, but
 * slightly slower when not packed.
 */
#define PACKED_SPECIAL_OPS 1

/*
 * Pseudo-operators (procedures of type t_oparray) record
 * the operand and dictionary stack pointers, and restore them if an error
 * occurs during the execution of the procedure and if the procedure hasn't
 * (net) decreased the depth of the stack.  While this obviously doesn't
 * do all the work of restoring the state if a pseudo-operator gets an
 * error, it's a big help.  The only downside is that pseudo-operators run
 * a little slower.
 */

/* Imported operator procedures */
extern int zop_add(P1(os_ptr));
extern int zop_def(P1(os_ptr));
extern int zop_sub(P1(os_ptr));

/* Other imported procedures */
extern int ztokenexec_continue(P1(os_ptr));

/* 
 * The procedure to call if an operator requests rescheduling.
 * This causes an error unless the context machinery has been installed.
 */
private int
no_reschedule(void)
{
    return_error(e_invalidcontext);
}
int (*gs_interp_reschedule_proc) (P0()) = no_reschedule;

/*
 * The procedure to call for time-slicing.
 * This is a no-op unless the context machinery has been installed.
 */
int
no_time_slice_proc(void)
{
    return 0;
}
int (*gs_interp_time_slice_proc) (P0()) = no_time_slice_proc;

/*
 * The number of interpreter "ticks" between calls on the time_slice_proc.
 * Currently, the clock ticks before each operator, and at each
 * procedure return.
 */
int gs_interp_time_slice_ticks = 0x7fff;

/*
 * Apply an operator.  When debugging, we route all operator calls
 * through a procedure.
 */
#ifdef DEBUG
private int
call_operator(int (*op_proc) (P1(os_ptr)), os_ptr op)
{
    int code = (*op_proc) (op);

    return code;
}
#else
#  define call_operator(proc, op) ((*(proc))(op))
#endif

/* Forward references */
private int estack_underflow(P1(os_ptr));
private int interp(P2(ref *, ref *));
private int interp_exit(P1(os_ptr));
private void set_gc_signal(P2(int *, int));
private int copy_stack(P2(const ref_stack *, ref *));
private int oparray_pop(P1(os_ptr));
private int oparray_cleanup(P1(os_ptr));

/* Stack sizes */

/* The maximum stack sizes may all be set in the makefile. */

/*
 * Define the initial maximum size of the operand stack (MaxOpStack
 * user parameter).
 */
#ifndef MAX_OSTACK
#  define MAX_OSTACK 800
#endif
/*
 * The minimum block size for extending the operand stack is the larger of:
 *      - the maximum number of parameters to an operator
 *      (currently setcolorscreen, with 12 parameters);
 *      - the maximum number of values pushed by an operator
 *      (currently setcolortransfer, which calls zcolor_remap_one 4 times
 *      and therefore pushes 16 values).
 */
#define MIN_BLOCK_OSTACK 16
const int gs_interp_max_op_num_args = MIN_BLOCK_OSTACK;		/* for iinit.c */

/*
 * Define the initial maximum size of the execution stack (MaxExecStack
 * user parameter).
 */
#ifndef MAX_ESTACK
#  define MAX_ESTACK 250
#endif
/*
 * The minimum block size for extending the execution stack is the largest
 * size of a contiguous block surrounding an e-stack mark, currently ???.
 * At least, that's what the minimum value would be if we supported
 * multi-block estacks, which we currently don't.
 */
#define MIN_BLOCK_ESTACK MAX_ESTACK

/*
 * Define the initial maximum size of the dictionary stack (MaxDictStack
 * user parameter).  Again, this is also currently the block size for
 * extending the d-stack.
 */
#ifndef MAX_DSTACK
#  define MAX_DSTACK 20
#endif
/*
 * The minimum block size for extending the dictionary stack is the number
 * of permanent entries on the dictionary stack, currently 3.
 */
#define MIN_BLOCK_DSTACK 3

/* Interpreter state variables */
ref ref_language_level;		/* 1 or 2, set by iinit.c */

/* See estack.h for a description of the execution stack. */

/* The logic for managing icount and iref below assumes that */
/* there are no control operators which pop and then push */
/* information on the execution stack. */

/* Stacks */
extern_st(st_ref_stack);
#define OS_GUARD_UNDER 10
#define OS_GUARD_OVER 10
#define OS_REFS_SIZE(body_size)\
  (stack_block_refs + OS_GUARD_UNDER + (body_size) + OS_GUARD_OVER)
op_stack_t iop_stack;

#define ES_GUARD_UNDER 1
#define ES_GUARD_OVER 10
#define ES_REFS_SIZE(body_size)\
  (stack_block_refs + ES_GUARD_UNDER + (body_size) + ES_GUARD_OVER)
exec_stack_t iexec_stack;

#define DS_REFS_SIZE(body_size)\
  (stack_block_refs + (body_size))
dict_stack_t idict_stack;

					  /*#define d_stack (idict_stack.stack) *//* in dstack.h */

/* Define a pointer to the current interpreter context state. */
gs_context_state_t *gs_interp_context_state_current;

/* Extended types.  The interpreter may replace the type of operators */
/* in procedures with these, to speed up the interpretation loop. */
/****** NOTE: If you add or change entries in this list, */
/****** you must change the three dispatches in the interpreter loop. */
/* The operator procedures are declared in opextern.h. */
#define tx_op t_next_index
private const op_proc_p special_ops[] =
{
    zadd, zdef, zdup, zexch, zif, zifelse, zindex, zpop, zroll, zsub
};
typedef enum {
    tx_op_add = tx_op,
    tx_op_def,
    tx_op_dup,
    tx_op_exch,
    tx_op_if,
    tx_op_ifelse,
    tx_op_index,
    tx_op_pop,
    tx_op_roll,
    tx_op_sub,
    tx_next_op
} special_op_types;

#define num_special_ops ((int)tx_next_op - tx_op)
const int gs_interp_num_special_ops = num_special_ops;	/* for iinit.c */
const int tx_next_index = tx_next_op;

#define make_null_proc(pref)\
  make_empty_const_array(pref, a_executable + a_readonly)

/* Initialize the interpreter. */
void
gs_interp_init(void)
{				/* Create and initialize a ocntext state. */
    gs_context_state_t *pcst = 0;
    int code = context_state_alloc(&pcst, &gs_imemory);

    if (code < 0 || (code = context_state_load(pcst)) < 0) {
	lprintf1("Fatal error %d in gs_interp_init!", code);
/****** ABORT ******/
    }
    gs_interp_context_state_current = pcst;
    gs_register_struct_root(imemory_local, NULL,
			    (void **)&gs_interp_context_state_current,
			    "gs_interp_init(gs_icst_root)");
}
/*
 * Create initial stacks for the interpreter.
 * We export this for creating new contexts.
 */
int
gs_interp_alloc_stacks(gs_ref_memory_t * smem, gs_context_state_t * pcst)
{
    ref stk;

#define REFS_SIZE_OSTACK OS_REFS_SIZE(MAX_OSTACK)
#define REFS_SIZE_ESTACK ES_REFS_SIZE(MAX_ESTACK)
#define REFS_SIZE_DSTACK DS_REFS_SIZE(MAX_DSTACK)
    gs_alloc_ref_array(smem, &stk, 0,
		       REFS_SIZE_OSTACK + REFS_SIZE_ESTACK +
		       REFS_SIZE_DSTACK, "gs_interp_alloc_stacks");

    {
	ref_stack *pos = pcst->ostack =
	gs_alloc_struct((gs_memory_t *) smem, ref_stack, &st_ref_stack,
			"gs_interp_alloc_stacks(ostack)");

	r_set_size(&stk, REFS_SIZE_OSTACK);
	ref_stack_init(pos, &stk, OS_GUARD_UNDER, OS_GUARD_OVER, NULL,
		       smem);
	pos->underflow_error = e_stackunderflow;
	pos->overflow_error = e_stackoverflow;
	ref_stack_set_max_count(pos, MAX_OSTACK);
    }

    {
	ref_stack *pes = pcst->estack =
	gs_alloc_struct((gs_memory_t *) smem, ref_stack, &st_ref_stack,
			"gs_interp_alloc_stacks(estack)");
	ref euop;

	stk.value.refs += REFS_SIZE_OSTACK;
	r_set_size(&stk, REFS_SIZE_ESTACK);
	make_oper(&euop, 0, estack_underflow);
	ref_stack_init(pes, &stk, ES_GUARD_UNDER, ES_GUARD_OVER, &euop,
		       smem);
	pes->underflow_error = e_ExecStackUnderflow;
	pes->overflow_error = e_execstackoverflow;
/**************** E-STACK EXPANSION IS NYI. ****************/
	pes->allow_expansion = false;
	ref_stack_set_max_count(pes, MAX_ESTACK);
    }

    {
	ref_stack *pds = pcst->dstack =
	gs_alloc_struct((gs_memory_t *) smem, ref_stack, &st_ref_stack,
			"gs_interp_alloc_stacks(dstack)");

	stk.value.refs += REFS_SIZE_ESTACK;
	r_set_size(&stk, REFS_SIZE_DSTACK);
	ref_stack_init(pds, &stk, 0, 0, NULL, smem);
	pds->underflow_error = e_dictstackunderflow;
	pds->overflow_error = e_dictstackoverflow;
	ref_stack_set_max_count(pds, MAX_DSTACK);
    }

#undef REFS_SIZE_OSTACK
#undef REFS_SIZE_ESTACK
#undef REFS_SIZE_DSTACK
    return 0;
}
/*
 * Free the stacks when destroying a context.  This is the inverse of
 * create_stacks.
 */
void
gs_interp_free_stacks(gs_ref_memory_t * smem, gs_context_state_t * pcst)
{				/* Free the stacks in inverse order of allocation. */
    ref_stack_free(pcst->dstack, (gs_memory_t *) smem,
		   "gs_interp_free_stacks(dstack)");
    ref_stack_free(pcst->estack, (gs_memory_t *) smem,
		   "gs_interp_free_stacks(estack)");
    ref_stack_free(pcst->ostack, (gs_memory_t *) smem,
		   "gs_interp_free_stacks(ostack)");
}
void
gs_interp_reset(void)
{				/* Reset the stacks. */
    ref_stack_clear(&o_stack);
    ref_stack_clear(&e_stack);
    esp++;
    make_oper(esp, 0, interp_exit);
    ref_stack_pop_to(&d_stack, min_dstack_size);
    dict_set_top();
}
/* Report an e-stack block underflow.  The bottom guard slots of */
/* e-stack blocks contain a pointer to this procedure. */
private int
estack_underflow(os_ptr op)
{
    return e_ExecStackUnderflow;
}

/*
 * Create an operator during initialization.
 * If operator is hard-coded into the interpreter,
 * assign it a special type and index.
 */
void
gs_interp_make_oper(ref * opref, op_proc_p proc, int idx)
{
    register int i = num_special_ops;

    while (--i >= 0 && proc != special_ops[i]);
    if (i >= 0)
	make_tasv(opref, tx_op + i, a_executable, i + 1, opproc, proc);
    else
	make_tasv(opref, t_operator, a_executable, idx, opproc, proc);
}

/*
 * Invoke the interpreter.  If execution completes normally, return 0.
 * If an error occurs, the action depends on user_errors as follows:
 *    user_errors < 0: always return an error code.
 *    user_errors >= 0: let the PostScript machinery handle all errors.
 *      (This will eventually result in a fatal error if no 'stopped'
 *      is active.)
 * In case of a quit or a fatal error, also store the exit code.
 */
private int gs_call_interp(P4(ref *, int, int *, ref *));
int
gs_interpret(ref * pref, int user_errors, int *pexit_code, ref * perror_object)
{
    gs_gc_root_t error_root;
    int code;

    gs_register_ref_root(imemory_system, &error_root,
			 (void **)&perror_object, "gs_interpret");
    /* Initialize the error object in case of GC. */
    make_null(perror_object);
    code = gs_call_interp(pref, user_errors, pexit_code, perror_object);
    gs_unregister_root(imemory_system, &error_root, "gs_interpret");
    /* Avoid a dangling reference to a stack-allocated GC signal. */
    set_gc_signal(NULL, 0);
    return code;
}
private int
gs_call_interp(ref * pref, int user_errors, int *pexit_code, ref * perror_object)
{
    ref *epref = pref;
    ref doref;
    ref *perrordict;
    ref error_name;
    int code, ccode;
    ref saref;
    int gc_signal = 0;

    *pexit_code = 0;
    ialloc_reset_requested(idmemory);
  again:o_stack.requested = e_stack.requested = d_stack.requested = 0;
    while (gc_signal) {		/* Some routine below triggered a GC. */
	gs_gc_root_t epref_root;

	gc_signal = 0;
	/* Make sure that doref will get relocated properly if */
	/* a garbage collection happens with epref == &doref. */
	gs_register_ref_root(imemory_system, &epref_root,
			     (void **)&epref,
			     "gs_call_interpret(epref)");
	code = (*idmemory->reclaim) (idmemory, -1);
	gs_unregister_root(imemory_system, &epref_root,
			   "gs_call_interpret(epref)");
	if (code < 0)
	    return code;
    }
    code = interp(epref, perror_object);
    /* Prevent a dangling reference to the GC signal in ticks_left */
    /* in the frame of interp, but be prepared to do a GC if */
    /* an allocation in this routine asks for it. */
    set_gc_signal(&gc_signal, 1);
    if (esp < esbot)		/* popped guard entry */
	esp = esbot;
    switch (code) {
	case e_Fatal:
	    *pexit_code = 255;
	    return code;
	case e_Quit:
	    *perror_object = osp[-1];
	    *pexit_code = code = osp->value.intval;
	    osp -= 2;
	    return
		(code == 0 ? e_Quit :
		 code < 0 && code > -100 ? code : e_Fatal);
	case e_InterpreterExit:
	    return 0;
	case e_ExecStackUnderflow:
/****** WRONG -- must keep mark blocks intact ******/
	    ref_stack_pop_block(&e_stack);
	    doref = *perror_object;
	    epref = &doref;
	    goto again;
	case e_VMreclaim:
	    /* Do the GC and continue. */
	    code = (*idmemory->reclaim) (idmemory,
					 (osp->value.intval == 2 ?
					  avm_global : avm_local));
/****** What if code < 0? ******/
	    make_oper(&doref, 0, zpop);
	    epref = &doref;
	    goto again;
	case e_NeedInput:
	    return code;
    }
    /* Adjust osp in case of operand stack underflow */
    if (osp < osbot - 1)
	osp = osbot - 1;
    /* We have to handle stack over/underflow specially, because */
    /* we might be able to recover by adding or removing a block. */
    switch (code) {
	case e_dictstackoverflow:
	    if (ref_stack_extend(&d_stack, d_stack.requested) >= 0) {
		dict_set_top();
		doref = *perror_object;
		epref = &doref;
		goto again;
	    }
	    if (osp >= ostop) {
		if ((ccode = ref_stack_extend(&o_stack, 1)) < 0)
		    return ccode;
	    }
	    ccode = copy_stack(&d_stack, &saref);
	    if (ccode < 0)
		return ccode;
	    ref_stack_pop_to(&d_stack, min_dstack_size);
	    dict_set_top();
	    *++osp = saref;
	    break;
	case e_dictstackunderflow:
	    if (ref_stack_pop_block(&d_stack) >= 0) {
		dict_set_top();
		doref = *perror_object;
		epref = &doref;
		goto again;
	    }
	    break;
	case e_execstackoverflow:
	    /* We don't have to handle this specially: */
	    /* The only places that could generate it */
	    /* use check_estack, which does a ref_stack_extend, */
	    /* so if we get this error, it's a real one. */
	    if (osp >= ostop) {
		if ((ccode = ref_stack_extend(&o_stack, 1)) < 0)
		    return ccode;
	    }
	    ccode = copy_stack(&e_stack, &saref);
	    if (ccode < 0)
		return ccode;
	    {
		uint count = ref_stack_count(&e_stack);
		long limit = ref_stack_max_count(&e_stack) - 10;

		if (count > limit)
		    pop_estack(count - limit);
	    }
	    *++osp = saref;
	    break;
	case e_stackoverflow:
	    if (ref_stack_extend(&o_stack, o_stack.requested) >= 0) {	/* We can't just re-execute the object, because */
		/* it might be a procedure being pushed as a */
		/* literal.  We check for this case specially. */
		doref = *perror_object;
		if (r_is_proc(&doref)) {
		    *++osp = doref;
		    make_null_proc(&doref);
		}
		epref = &doref;
		goto again;
	    }
	    ccode = copy_stack(&o_stack, &saref);
	    if (ccode < 0)
		return ccode;
	    ref_stack_clear(&o_stack);
	    *++osp = saref;
	    break;
	case e_stackunderflow:
	    if (ref_stack_pop_block(&o_stack) >= 0) {
		doref = *perror_object;
		epref = &doref;
		goto again;
	    }
	    break;
    }
    if (user_errors < 0)
	return code;
    if (gs_errorname(code, &error_name) < 0)
	return code;		/* out-of-range error code! */
    if (dict_find_string(systemdict, "errordict", &perrordict) <= 0 ||
	dict_find(perrordict, &error_name, &epref) <= 0
	)
	return code;		/* error name not in errordict??? */
    doref = *epref;
    epref = &doref;
    /* Push the error object on the operand stack if appropriate. */
    if (!error_is_interrupt(code))
	*++osp = *perror_object;
    goto again;
}
private int
interp_exit(os_ptr op)
{
    return e_InterpreterExit;
}

/* Set the GC signal for all VMs. */
private void
set_gc_signal(int *psignal, int value)
{
    gs_memory_gc_status_t stat;
    int i;

    for (i = 0; i < countof(idmemory->spaces.indexed); i++) {
	gs_ref_memory_t *mem = idmemory->spaces.indexed[i];

	if (mem != 0) {
	    gs_memory_gc_status(mem, &stat);
	    stat.psignal = psignal;
	    stat.signal_value = value;
	    gs_memory_set_gc_status(mem, &stat);
	}
    }
}

/* Copy the contents of an overflowed stack into a (local) array. */
private int
copy_stack(const ref_stack * pstack, ref * arr)
{
    uint size = ref_stack_count(pstack);
    uint save_space = ialloc_space(idmemory);
    int code;

    ialloc_set_space(idmemory, avm_local);
    code = ialloc_ref_array(arr, a_all, size, "copy_stack");
    if (code >= 0)
	code = ref_stack_store(pstack, arr, size, 0, 1, true, "copy_stack");
    ialloc_set_space(idmemory, save_space);
    return code;
}

/* Get the name corresponding to an error number. */
int
gs_errorname(int code, ref * perror_name)
{
    ref *perrordict, *pErrorNames;

    if (dict_find_string(systemdict, "errordict", &perrordict) <= 0 ||
	dict_find_string(systemdict, "ErrorNames", &pErrorNames) <= 0
	)
	return_error(e_undefined);	/* errordict or ErrorNames not found?! */
    return array_get(pErrorNames, (long)(-code - 1), perror_name);
}

/* Store an error string in $error.errorinfo. */
/* This routine is here because of the proximity to the error handler. */
int
gs_errorinfo_put_string(const char *str)
{
    ref rstr;
    ref *pderror;
    int code = string_to_ref(str, &rstr, iimemory, "gs_errorinfo_put_string");

    if (code < 0)
	return code;
    if (dict_find_string(systemdict, "$error", &pderror) <= 0 ||
	!r_has_type(pderror, t_dictionary) ||
	dict_put_string(pderror, "errorinfo", &rstr) < 0
	)
	return_error(e_Fatal);
    return 0;
}

/* Main interpreter. */
/* If execution terminates normally, return e_InterpreterExit. */
/* If an error occurs, leave the current object in *perror_object */
/* and return a (negative) error code. */
private int
interp(ref * pref /* object to interpret */ , ref * perror_object)
{				/*
				 * Note that iref is declared as a ref *, but it may actually be
				 * a ref_packed *.
				 */
    register const ref *iref = pref;
    register int icount = 0;	/* # of consecutive tokens at iref */
    register os_ptr iosp = osp;	/* private copy of osp */
    register es_ptr iesp = esp;	/* private copy of esp */
    int code;
    ref token;			/* token read from file or string, */

    /* must be declared in this scope */
    register const ref *pvalue;
    os_ptr whichp;

    /*
     * We have to make the error information into a struct;
     * otherwise, the Watcom compiler will assign it to registers
     * strictly on the basis of textual frequency.
     * We also have to use ref_assign_inline everywhere, and
     * avoid direct assignments of refs, so that esi and edi
     * will remain available on Intel processors.
     */
    struct interp_error_s {
	int code;
	int line;
	const ref *obj;
	ref full;
    } ierror;

    /*
     * Get a pointer to the name table so that we can use the
     * inline version of name_index_ref.
     */
    const name_table *const int_nt = the_name_table();

#define set_error(ecode)\
  { ierror.code = ecode; ierror.line = __LINE__; }
#define return_with_error(ecode, objp)\
  { set_error(ecode); ierror.obj = objp; goto rwe; }
#define return_with_error_iref(ecode)\
  { set_error(ecode); goto rwei; }
#define return_with_code_iref()\
  { ierror.line = __LINE__; goto rweci; }
#define return_with_error_code_op(nargs)\
  return_with_code_iref()
#define return_with_stackoverflow(objp)\
  { o_stack.requested = 1; return_with_error(e_stackoverflow, objp); }
#define return_with_stackoverflow_iref()\
  { o_stack.requested = 1; return_with_error_iref(e_stackoverflow); }
    int ticks_left = gs_interp_time_slice_ticks;

    /*
     * If we exceed the VMThreshold, set ticks_left to -1
     * to alert the interpreter that we need to garbage collect.
     */
    set_gc_signal(&ticks_left, -100);

    esfile_clear_cache();
    /*
     * From here on, if icount > 0, iref and icount correspond
     * to the top entry on the execution stack: icount is the count
     * of sequential entries remaining AFTER the current one.
     */
#define add1_short(pref) (const ref *)((const ushort *)(pref) + 1)
#define add1_either(pref) (r_is_packed(pref) ? add1_short(pref) : (pref) + 1)
#define store_state(ep)\
  ( icount > 0 ? (ep->value.const_refs = iref + 1, r_set_size(ep, icount)) : 0 )
#define store_state_short(ep)\
  ( icount > 0 ? (ep->value.const_refs = add1_short(iref), r_set_size(ep, icount)) : 0 )
#define store_state_either(ep)\
  ( icount > 0 ? (ep->value.const_refs = add1_either(iref), r_set_size(ep, icount)) : 0 )
#define next()\
  if ( --icount > 0 ) { iref++; goto top; } else goto out
#define next_short()\
  if ( --icount <= 0 ) { if ( icount < 0 ) goto up; iesp--; }\
  iref = add1_short(iref); goto top
#define next_either()\
  if ( --icount <= 0 ) { if ( icount < 0 ) goto up; iesp--; }\
  iref = add1_either(iref); goto top

#if !PACKED_SPECIAL_OPS
#  undef next_either
#  define next_either() next()
#  undef store_state_either
#  define store_state_either(ep) store_state(ep)
#endif

    /* We want to recognize executable arrays here, */
    /* so we push the argument on the estack and enter */
    /* the loop at the bottom. */
    if (iesp >= estop)
	return_with_error(e_execstackoverflow, pref);
    ++iesp;
    ref_assign_inline(iesp, pref);
    goto bot;
  top:				/*
				 * This is the top of the interpreter loop.
				 * iref points to the ref being interpreted.
				 * Note that this might be an element of a packed array,
				 * not a real ref: we carefully arranged the first 16 bits of
				 * a ref and of a packed array element so they could be distinguished
				 * from each other.  (See ghost.h and packed.h for more detail.)
				 */
#ifdef DEBUG
    /* Do a little validation on the top o-stack entry. */
    if (iosp >= osbot &&
	(r_type(iosp) == t__invalid || r_type(iosp) >= tx_next_op)
	) {
	lprintf("Invalid value on o-stack!\n");
	return_with_error_iref(e_Fatal);
    }
    if (gs_debug['I'] ||
	(gs_debug['i'] &&
	 (r_is_packed(iref) ?
	  r_packed_is_name((const ref_packed *)iref) :
	  r_has_type(iref, t_name)))
	) {
	void debug_print_ref(P1(const ref *));
	os_ptr save_osp = osp;	/* avoid side-effects */
	es_ptr save_esp = esp;
	int edepth;
	char depth[10];

	osp = iosp;
	esp = iesp;
	edepth = ref_stack_count(&e_stack);
	sprintf(depth, "%2d", edepth);
	dputs(depth);
	for (edepth -= strlen(depth); edepth >= 5; edepth -= 5)
	    dputc('*');		/* indent */
	for (; edepth > 0; --edepth)
	    dputc('.');
	dlprintf3("0x%lx(%d)<%d>: ",
		  (ulong) iref, icount, ref_stack_count(&o_stack));
	debug_print_ref(iref);
	if (iosp >= osbot) {
	    dputs(" // ");
	    debug_print_ref(iosp);
	}
	dputc('\n');
	osp = save_osp;
	esp = save_esp;
	fflush(dstderr);
    }
#endif
/* Objects that have attributes (arrays, dictionaries, files, and strings) */
/* use lit and exec; other objects use plain and plain_exec. */
#define lit(t) type_xe_value(t, a_execute)
#define exec(t) type_xe_value(t, a_execute + a_executable)
#define nox(t) type_xe_value(t, 0)
#define nox_exec(t) type_xe_value(t, a_executable)
#define plain(t) type_xe_value(t, 0)
#define plain_exec(t) type_xe_value(t, a_executable)
    /*
     * We have to populate enough cases of the switch statement to force
     * some compilers to use a dispatch rather than a testing loop.
     * What a nuisance!
     */
    switch (r_type_xe(iref)) {
	    /* Access errors. */
#define cases_invalid()\
  case plain(t__invalid): case plain_exec(t__invalid)
	  cases_invalid():
	    return_with_error_iref(e_Fatal);
#define cases_nox()\
  case nox_exec(t_array): case nox_exec(t_dictionary):\
  case nox_exec(t_file): case nox_exec(t_string):\
  case nox_exec(t_mixedarray): case nox_exec(t_shortarray)
	  cases_nox():
	    return_with_error_iref(e_invalidaccess);
	    /*
	     * Literal objects.  We have to enumerate all the types.
	     * In fact, we have to include some extra plain_exec entries
	     * just to populate the switch.  We break them up into groups
	     * to avoid overflowing some preprocessors.
	     */
#define cases_lit_1()\
  case lit(t_array): case nox(t_array):\
  case plain(t_boolean): case plain_exec(t_boolean):\
  case lit(t_dictionary): case nox(t_dictionary)
#define cases_lit_2()\
  case lit(t_file): case nox(t_file):\
  case plain(t_fontID): case plain_exec(t_fontID):\
  case plain(t_integer): case plain_exec(t_integer):\
  case plain(t_mark): case plain_exec(t_mark)
#define cases_lit_3()\
  case plain(t_name):\
  case plain(t_null):\
  case plain(t_oparray):\
  case plain(t_operator)
#define cases_lit_4()\
  case plain(t_real): case plain_exec(t_real):\
  case plain(t_save): case plain_exec(t_save):\
  case lit(t_string): case nox(t_string)
#define cases_lit_5()\
  case lit(t_mixedarray): case nox(t_mixedarray):\
  case lit(t_shortarray): case nox(t_shortarray):\
  case plain(t_device): case plain_exec(t_device):\
  case plain(t_struct): case plain_exec(t_struct):\
  case plain(t_astruct): case plain_exec(t_astruct)
	    /* Executable arrays are treated as literals in direct execution. */
#define cases_lit_array()\
  case exec(t_array): case exec(t_mixedarray): case exec(t_shortarray)
	  cases_lit_1():
	  cases_lit_2():
	  cases_lit_3():
	  cases_lit_4():
	  cases_lit_5():
	  cases_lit_array():
	    break;
	    /* Special operators. */
	case plain_exec(tx_op_add):
	  x_add:if ((code = zop_add(iosp)) < 0)
		return_with_error_code_op(2);
	    iosp--;
	    next_either();
	case plain_exec(tx_op_def):
	  x_def:if ((code = zop_def(iosp)) < 0)
		return_with_error_code_op(2);
	    iosp -= 2;
	    next_either();
	case plain_exec(tx_op_dup):
	  x_dup:if (iosp < osbot)
		return_with_error_iref(e_stackunderflow);
	    if (iosp >= ostop)
		return_with_stackoverflow_iref();
	    iosp++;
	    ref_assign_inline(iosp, iosp - 1);
	    next_either();
	case plain_exec(tx_op_exch):
	  x_exch:if (iosp <= osbot)
		return_with_error_iref(e_stackunderflow);
	    ref_assign_inline(&token, iosp);
	    ref_assign_inline(iosp, iosp - 1);
	    ref_assign_inline(iosp - 1, &token);
	    next_either();
	case plain_exec(tx_op_if):
	  x_if:if (!r_has_type(iosp - 1, t_boolean))
		return_with_error_iref((iosp <= osbot ?
					e_stackunderflow : e_typecheck));
	    if (!r_is_proc(iosp))
		return_with_error_iref(check_proc_failed(iosp));
	    if (!iosp[-1].value.boolval) {
		iosp -= 2;
		next_either();
	    }
	    if (iesp >= estop)
		return_with_error_iref(e_execstackoverflow);
	    store_state_either(iesp);
	    whichp = iosp;
	    iosp -= 2;
	    goto ifup;
	case plain_exec(tx_op_ifelse):
	  x_ifelse:if (!r_has_type(iosp - 2, t_boolean))
		return_with_error_iref((iosp < osbot + 2 ?
					e_stackunderflow : e_typecheck));
	    if (!r_is_proc(iosp - 1))
		return_with_error_iref(check_proc_failed(iosp - 1));
	    if (!r_is_proc(iosp))
		return_with_error_iref(check_proc_failed(iosp));
	    if (iesp >= estop)
		return_with_error_iref(e_execstackoverflow);
	    store_state_either(iesp);
	    whichp = (iosp[-2].value.boolval ? iosp - 1 : iosp);
	    iosp -= 3;
	    /* Open code "up" for the array case(s) */
	  ifup:if ((icount = r_size(whichp) - 1) <= 0) {
		if (icount < 0)
		    goto up;	/* 0-element proc */
		iref = whichp->value.refs;	/* 1-element proc */
		if (--ticks_left > 0)
		    goto top;
	    }
	    ++iesp;
	    /* Do a ref_assign, but also set iref. */
	    iesp->tas = whichp->tas;
	    iref = iesp->value.refs = whichp->value.refs;
	    if (--ticks_left > 0)
		goto top;
	    goto slice;
	case plain_exec(tx_op_index):
	  x_index:osp = iosp;	/* zindex references o_stack */
	    if ((code = zindex(iosp)) < 0)
		return_with_error_code_op(1);
	    next_either();
	case plain_exec(tx_op_pop):
	  x_pop:if (iosp < osbot)
		return_with_error_iref(e_stackunderflow);
	    iosp--;
	    next_either();
	case plain_exec(tx_op_roll):
	  x_roll:osp = iosp;	/* zroll references o_stack */
	    if ((code = zroll(iosp)) < 0)
		return_with_error_code_op(2);
	    iosp -= 2;
	    next_either();
	case plain_exec(tx_op_sub):
	  x_sub:if ((code = zop_sub(iosp)) < 0)
		return_with_error_code_op(2);
	    iosp--;
	    next_either();
	    /* Executable types. */
	case plain_exec(t_null):
	    goto bot;
	case plain_exec(t_oparray):
	    /* Replace with the definition and go again. */
	    pvalue = (const ref *)iref->value.const_refs;
	  opst:		/* Prepare to call a t_oparray procedure in *pvalue. */
	    store_state(iesp);
	  oppr:		/* Record the stack depths in case of failure. */
	    if (iesp >= estop - 3)
		return_with_error_iref(e_execstackoverflow);
	    iesp += 4;
	    osp = iosp;		/* ref_stack_count_inline needs this */
	    make_mark_estack(iesp - 3, es_other, oparray_cleanup);
	    make_int(iesp - 2, ref_stack_count_inline(&o_stack));
	    make_int(iesp - 1, ref_stack_count_inline(&d_stack));
	    make_op_estack(iesp, oparray_pop);
	    goto pr;
	  prst:		/* Prepare to call the procedure (array) in *pvalue. */
	    store_state(iesp);
	  pr:			/* Call the array in *pvalue.  State has been stored. */
	    if ((icount = r_size(pvalue) - 1) <= 0) {
		if (icount < 0)
		    goto up;	/* 0-element proc */
		iref = pvalue->value.refs;	/* 1-element proc */
		if (--ticks_left > 0)
		    goto top;
	    }
	    if (iesp >= estop)
		return_with_error_iref(e_execstackoverflow);
	    ++iesp;
	    /* Do a ref_assign, but also set iref. */
	    iesp->tas = pvalue->tas;
	    iref = iesp->value.refs = pvalue->value.refs;
	    if (--ticks_left > 0)
		goto top;
	    goto slice;
	case plain_exec(t_operator):
	    if (--ticks_left <= 0) {	/* The following doesn't work, */
		/* and I can't figure out why. */
/****** goto sst; ******/
	    }
	    esp = iesp;		/* save for operator */
	    osp = iosp;		/* ditto */
	    /* Operator routines take osp as an argument. */
	    /* This is just a convenience, since they adjust */
	    /* osp themselves to reflect the results. */
	    /* Operators that (net) push information on the */
	    /* operand stack must check for overflow: */
	    /* this normally happens automatically through */
	    /* the push macro (in oper.h). */
	    /* Operators that do not typecheck their operands, */
	    /* or take a variable number of arguments, */
	    /* must check explicitly for stack underflow. */
	    /* (See oper.h for more detail.) */
	    /* Note that each case must set iosp = osp: */
	    /* this is so we can switch on code without having to */
	    /* store it and reload it (for dumb compilers). */
	    switch (code = call_operator(real_opproc(iref), iosp)) {
		case 0:	/* normal case */
		case 1:	/* alternative success case */
		    iosp = osp;
		    next();
		case o_push_estack:	/* store the state and go to up */
		    store_state(iesp);
		  opush:iosp = osp;
		    iesp = esp;
		    if (--ticks_left > 0)
			goto up;
		    goto slice;
		case o_pop_estack:	/* just go to up */
		  opop:iosp = osp;
		    if (esp == iesp)
			goto bot;
		    iesp = esp;
		    goto up;
		case o_reschedule:
		    store_state(iesp);
		    goto res;
		case e_InsertProc:
		    store_state(iesp);
		  oeinsert:ref_assign_inline(iesp + 1, iref);
		    /* esp = iesp + 2; *esp = the procedure */
		    iesp = esp;
		    goto up;
	    }
	    iosp = osp;
	    iesp = esp;
	    return_with_code_iref();
	case plain_exec(t_name):
	    pvalue = iref->value.pname->pvalue;
	    if (!pv_valid(pvalue)) {
		uint nidx = names_index(int_nt, iref);
		uint htemp;

		if ((pvalue = dict_find_name_by_index_inline(nidx, htemp)) == 0)
		    return_with_error_iref(e_undefined);
	    }
	    /* Dispatch on the type of the value. */
	    /* Again, we have to over-populate the switch. */
	    switch (r_type_xe(pvalue)) {
		  cases_invalid():
		    return_with_error_iref(e_Fatal);
		  cases_nox():	/* access errors */
		    return_with_error_iref(e_invalidaccess);
		  cases_lit_1():
		  cases_lit_2():
		  cases_lit_3():
		  cases_lit_4():
		  cases_lit_5():
		    /* Just push the value */
		    if (iosp >= ostop)
			return_with_stackoverflow(pvalue);
		    ++iosp;
		    ref_assign_inline(iosp, pvalue);
		    next();
		case exec(t_array):
		case exec(t_mixedarray):
		case exec(t_shortarray):
		    /* This is an executable procedure, execute it. */
		    goto prst;
		case plain_exec(tx_op_add):
		    goto x_add;
		case plain_exec(tx_op_def):
		    goto x_def;
		case plain_exec(tx_op_dup):
		    goto x_dup;
		case plain_exec(tx_op_exch):
		    goto x_exch;
		case plain_exec(tx_op_if):
		    goto x_if;
		case plain_exec(tx_op_ifelse):
		    goto x_ifelse;
		case plain_exec(tx_op_index):
		    goto x_index;
		case plain_exec(tx_op_pop):
		    goto x_pop;
		case plain_exec(tx_op_roll):
		    goto x_roll;
		case plain_exec(tx_op_sub):
		    goto x_sub;
		case plain_exec(t_null):
		    goto bot;
		case plain_exec(t_oparray):
		    pvalue = (const ref *)pvalue->value.const_refs;
		    goto opst;
		case plain_exec(t_operator):
		    {		/* Shortcut for operators. */
			/* See above for the logic. */
			if (--ticks_left <= 0) {	/* The following doesn't work, */
			    /* and I can't figure out why. */
/****** goto sst; ******/
			}
			esp = iesp;
			osp = iosp;
			switch (code = call_operator(real_opproc(pvalue), iosp)) {
			    case 0:	/* normal case */
			    case 1:	/* alternative success case */
				iosp = osp;
				next();
			    case o_push_estack:
				store_state(iesp);
				goto opush;
			    case o_pop_estack:
				goto opop;
			    case o_reschedule:
				store_state(iesp);
				goto res;
			    case e_InsertProc:
				store_state(iesp);
				goto oeinsert;
			}
			iosp = osp;
			iesp = esp;
			return_with_error(code, pvalue);
		    }
		case plain_exec(t_name):
		case exec(t_file):
		case exec(t_string):
		default:
		    /* Not a procedure, reinterpret it. */
		    store_state(iesp);
		    icount = 0;
		    iref = pvalue;
		    goto top;
	    }
	case exec(t_file):
	    {			/* Executable file.  Read the next token and interpret it. */
		stream *s;
		scanner_state sstate;

		check_read_known_file(s, iref, return_with_error_iref);
	      rt:if (iosp >= ostop)	/* check early */
		    return_with_stackoverflow_iref();
		osp = iosp;	/* scan_token uses ostack */
		scanner_state_init(&sstate, false);
	      again:code = scan_token(s, &token, &sstate);
		iosp = osp;	/* ditto */
		switch (code) {
		    case 0:	/* read a token */
			/* It's worth checking for literals, which make up */
			/* the majority of input tokens, before storing the */
			/* state on the e-stack.  Note that because of //, */
			/* the token may have *any* type and attributes. */
			/* Note also that executable arrays aren't executed */
			/* at the top level -- they're treated as literals. */
			if (!r_has_attr(&token, a_executable) ||
			    r_is_array(&token)
			    ) {	/* If scan_token used the o-stack, */
			    /* we know we can do a push now; if not, */
			    /* the pre-check is still valid. */
			    iosp++;
			    ref_assign_inline(iosp, &token);
			    goto rt;
			}
			store_state(iesp);
			/* Push the file on the e-stack */
			if (iesp >= estop)
			    return_with_error_iref(e_execstackoverflow);
			esfile_set_cache(++iesp);
			ref_assign_inline(iesp, iref);
			iref = &token;
			icount = 0;
			goto top;
		    case scan_EOF:	/* end of file */
			esfile_clear_cache();
			goto bot;
		    case scan_BOS:
			/* Binary object sequences */
			/* ARE executed at the top level. */
			store_state(iesp);
			/* Push the file on the e-stack */
			if (iesp >= estop)
			    return_with_error_iref(e_execstackoverflow);
			esfile_set_cache(++iesp);
			ref_assign_inline(iesp, iref);
			pvalue = &token;
			goto pr;
		    case scan_Refill:
			store_state(iesp);
			/* iref may point into the exec stack; */
			/* save its referent now. */
			ref_assign_inline(&token, iref);
			/* Push the file on the e-stack */
			if (iesp >= estop)
			    return_with_error_iref(e_execstackoverflow);
			++iesp;
			ref_assign_inline(iesp, &token);
			esp = iesp;
			osp = iosp;
			code = scan_handle_refill(&token, &sstate, true, true,
						  ztokenexec_continue);
			iosp = osp;
			iesp = esp;
			switch (code) {
			    case 0:
				iesp--;		/* don't push the file */
				goto again;	/* stacks are unchanged */
			    case o_push_estack:
				esfile_clear_cache();
				if (--ticks_left > 0)
				    goto up;
				goto slice;
			}
			/* must be an error */
			iesp--;	/* don't push the file */
		    default:	/* error */
			return_with_code_iref();
		}
	    }
	case exec(t_string):
	    {			/* Executable string.  Read a token and interpret it. */
		stream ss;
		scanner_state sstate;

		scanner_state_init(&sstate, true);
		sread_string(&ss, iref->value.bytes, r_size(iref));
		osp = iosp;	/* scan_token uses ostack */
		code = scan_token(&ss, &token, &sstate);
		iosp = osp;	/* ditto */
		switch (code) {
		    case 0:	/* read a token */
		    case scan_BOS:	/* binary object sequence */
			store_state(iesp);
			/* If the updated string isn't empty, push it back */
			/* on the e-stack. */
			{
			    uint size = sbufavailable(&ss);

			    if (size) {
				if (iesp >= estop)
				    return_with_error_iref(e_execstackoverflow);
				++iesp;
				iesp->tas.type_attrs = iref->tas.type_attrs;
				iesp->value.const_bytes = sbufptr(&ss);
				r_set_size(iesp, size);
			    }
			}
			if (code == 0) {
			    iref = &token;
			    icount = 0;
			    goto top;
			}
			/* Handle BOS specially */
			pvalue = &token;
			goto pr;
		    case scan_EOF:	/* end of string */
			goto bot;
		    case scan_Refill:	/* error */
			code = gs_note_error(e_syntaxerror);
		    default:	/* error */
			return_with_code_iref();
		}
	    }
	    /* Handle packed arrays here by re-dispatching. */
	    /* This also picks up some anomalous cases of non-packed arrays. */
	default:
	    {
		uint index;

		switch (*(const ushort *)iref >> r_packed_type_shift) {
		    case pt_full_ref:
		    case pt_full_ref + 1:
			if (iosp >= ostop)
			    return_with_stackoverflow_iref();
			/* We know this can't be an executable object */
			/* requiring special handling, so we just push it. */
			++iosp;
			/* We know that refs are properly aligned: */
			/* see packed.h for details. */
			ref_assign_inline(iosp, iref);
			next();
		    case pt_executable_operator:
			index = *(const ushort *)iref & packed_value_mask;
			if (--ticks_left <= 0) {	/* The following doesn't work, */
			    /* and I can't figure out why. */
/****** goto sst_short; ******/
			}
			if (!op_index_is_operator(index)) {
			    store_state_short(iesp);
			    /* Call the operator procedure. */
			    index -= op_def_count;
			    pvalue = (const ref *)
				(index < r_size(&op_array_table_global.table) ?
			      op_array_table_global.table.value.const_refs +
				 index :
			       op_array_table_local.table.value.const_refs +
			    (index - r_size(&op_array_table_global.table)));
			    goto oppr;
			}
			/* See the main plain_exec(t_operator) case */
			/* for details of what happens here. */
#if PACKED_SPECIAL_OPS
			/*
			 * We arranged in iinit.c that the special ops
			 * have operator indices starting at 1.
			 *
			 * The (int) cast in the next line is required
			 * because some compilers don't allow arithmetic
			 * involving two different enumerated types.
			 */
#  define case_xop(xop) case xop - (int)tx_op + 1
			switch (index) {
			      case_xop(tx_op_add):goto x_add;
			      case_xop(tx_op_def):goto x_def;
			      case_xop(tx_op_dup):goto x_dup;
			      case_xop(tx_op_exch):goto x_exch;
			      case_xop(tx_op_if):goto x_if;
			      case_xop(tx_op_ifelse):goto x_ifelse;
			      case_xop(tx_op_index):goto x_index;
			      case_xop(tx_op_pop):goto x_pop;
			      case_xop(tx_op_roll):goto x_roll;
			      case_xop(tx_op_sub):goto x_sub;
			    case 0:	/* for dumb compilers */
			    default:
				;
			}
#  undef case_xop
#endif
			esp = iesp;
			osp = iosp;
			switch (code = call_operator(op_index_proc(index), iosp)) {
			    case 0:
			    case 1:
				iosp = osp;
				next_short();
			    case o_push_estack:
				store_state_short(iesp);
				goto opush;
			    case o_pop_estack:
				iosp = osp;
				if (esp == iesp) {
				    next_short();
				}
				iesp = esp;
				goto up;
			    case o_reschedule:
				store_state_short(iesp);
				goto res;
			    case e_InsertProc:
				store_state_short(iesp);
				packed_get((const ref_packed *)iref, iesp + 1);
				/* esp = iesp + 2; *esp = the procedure */
				iesp = esp;
				goto up;
			}
			iosp = osp;
			iesp = esp;
			return_with_code_iref();
		    case pt_integer:
			if (iosp >= ostop)
			    return_with_stackoverflow_iref();
			++iosp;
			make_int(iosp,
				 (*(const short *)iref & packed_int_mask) +
				 packed_min_intval);
			next_short();
		    case pt_literal_name:
			{
			    uint nidx = *(const ushort *)iref & packed_value_mask;

			    if (iosp >= ostop)
				return_with_stackoverflow_iref();
			    ++iosp;
			    name_index_ref_inline(int_nt, nidx, iosp);
			    next_short();
			}
		    case pt_executable_name:
			{
			    uint nidx =
			    (uint) * (const ushort *)iref & packed_value_mask;

			    pvalue = name_index_ptr_inline(int_nt, nidx)->pvalue;
			    if (!pv_valid(pvalue)) {
				uint htemp;

				if ((pvalue = dict_find_name_by_index_inline(nidx, htemp)) == 0) {
				    names_index_ref(int_nt, nidx, &token);
				    return_with_error(e_undefined, &token);
				}
			    }
			    if (r_has_masked_attrs(pvalue, a_execute, a_execute + a_executable)) {	/* Literal, push it. */
				if (iosp >= ostop)
				    return_with_stackoverflow_iref();
				++iosp;
				ref_assign_inline(iosp, pvalue);
				next_short();
			    }
			    if (r_is_proc(pvalue)) {	/* This is an executable procedure, */
				/* execute it. */
				store_state_short(iesp);
				goto pr;
			    }
			    /* Not a literal or procedure, reinterpret it. */
			    store_state_short(iesp);
			    icount = 0;
			    iref = pvalue;
			    goto top;
			}
			/* default can't happen here */
		}
	    }
    }
    /* Literal type, just push it. */
    if (iosp >= ostop)
	return_with_stackoverflow_iref();
    ++iosp;
    ref_assign_inline(iosp, iref);
  bot:next();
  out:				/* At most 1 more token in the current procedure. */
    /* (We already decremented icount.) */
    if (!icount) {		/* Pop the execution stack for tail recursion. */
	iesp--;
	iref++;
	goto top;
    }
  up:if (--ticks_left < 0)
	goto slice;
    /* See if there is anything left on the execution stack. */
    if (!r_is_proc(iesp)) {
	iref = iesp--;
	icount = 0;
	goto top;
    }
    iref = iesp->value.refs;	/* next element of array */
    icount = r_size(iesp) - 1;
    if (icount <= 0) {		/* <= 1 more elements */
	iesp--;			/* pop, or tail recursion */
	if (icount < 0)
	    goto up;
    }
    goto top;
  res:				/* Some operator has asked for context rescheduling. */
    /* We've done a store_state. */
    code = (*gs_interp_reschedule_proc) ();
  sched:			/* We've just called a scheduling procedure. */
    /* The interpreter state is in memory; iref is not current. */
    if (code < 0) {
	set_error(code);
	/*
	 * We need a real object to return as the error object.
	 * (It only has to last long enough to store in
	 * *perror_object.)
	 */
	make_null_proc(&ierror.full);
	ierror.obj = iref = &ierror.full;
	goto error_exit;
    }
    /* Reload state information from memory. */
    iosp = osp;
    iesp = esp;
    goto up;
#if 0				/****** ****** ***** */
  sst:				/* Time-slice, but push the current object first. */
    store_state(iesp);
    if (iesp >= estop)
	return_with_error_iref(e_execstackoverflow);
    iesp++;
    ref_assign_inline(iesp, iref);
#endif /****** ****** ***** */
  slice:			/* It's time to time-slice or garbage collect. */
    /* iref is not live, so we don't need to do a store_state. */
    osp = iosp;
    esp = iesp;
    /* If ticks_left <= -100, we need to GC now. */
    if (ticks_left <= -100) {	/* We need to garbage collect now. */
	code = (*idmemory->reclaim) (idmemory, -1);
    } else
	code = (*gs_interp_time_slice_proc) ();
    ticks_left = gs_interp_time_slice_ticks;
    goto sched;

    /* Error exits. */

  rweci:
    ierror.code = code;
  rwei:
    ierror.obj = iref;
  rwe:
    if (!r_is_packed(iref))
	store_state(iesp);
    else {			/*
				 * We need a real object to return as the error object.
				 * (It only has to last long enough to store in
				 * *perror_object.)
				 */
	packed_get((const ref_packed *)ierror.obj, &ierror.full);
	store_state_short(iesp);
	if (iref == ierror.obj)
	    iref = &ierror.full;
	ierror.obj = &ierror.full;
    }
  error_exit:
    if (error_is_interrupt(ierror.code)) {	/* We must push the current object being interpreted */
	/* back on the e-stack so it will be re-executed. */
	/* Currently, this is always an executable operator, */
	/* but it might be something else someday if we check */
	/* for interrupts in the interpreter loop itself. */
	if (iesp >= estop)
	    code = e_execstackoverflow;
	else {
	    iesp++;
	    ref_assign_inline(iesp, iref);
	}
    }
    esp = iesp;
    osp = iosp;
    ref_assign_inline(perror_object, ierror.obj);
    return gs_log_error(ierror.code, __FILE__, ierror.line);

}

/* Pop the bookkeeping information for a normal exit from a t_oparray. */
private int
oparray_pop(os_ptr op)
{
    esp -= 3;
    return o_pop_estack;
}

/* Restore the stack pointers after an error inside a t_oparray procedure. */
/* This procedure is called only from pop_estack. */
private int
oparray_cleanup(os_ptr op)
{				/* esp points just below the cleanup procedure. */
    es_ptr ep = esp;
    uint ocount_old = (uint) ep[2].value.intval;
    uint dcount_old = (uint) ep[3].value.intval;
    uint ocount = ref_stack_count(&o_stack);
    uint dcount = ref_stack_count(&d_stack);

    if (ocount > ocount_old)
	ref_stack_pop(&o_stack, ocount - ocount_old);
    if (dcount > dcount_old) {
	ref_stack_pop(&d_stack, dcount - dcount_old);
	dict_set_top();
    }
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def interp_op_defs[] =
{
    /* Internal operators */
    {"0%interp_exit", interp_exit},
    {"0%oparray_pop", oparray_pop},
    op_def_end(0)
};
