/* Copyright (C) 1991, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* zcontext.c */
/* Display PostScript context operators */
#include "memory_.h"
#include "ghost.h"
#include "errors.h"
#include "gsstruct.h"
#include "gsutil.h"
#include "oper.h"
#include "idict.h"
#include "igstate.h"
#include "istruct.h"
#include "dstack.h"
#include "estack.h"
#include "store.h"

/****** THIS FILE IS NOT IN GOOD ENOUGH SHAPE TO USE YET. ******/
/* In particular, it hasn't been updated to handle expandable stacks. */
/****** DON'T TRY TO USE IT.  REALLY. ******/

/* Scheduling hooks in interp.c */
extern int (*gs_interp_reschedule_proc)(P0());
extern int (*gs_interp_time_slice_proc)(P0());
extern int gs_interp_time_slice_ticks;

/* Context structure */
typedef enum {
	cs_invalid,
	cs_active,
	cs_done
} ctx_status;
typedef struct gs_context_s gs_context;
struct gs_context_s {
	ctx_status status;
	ulong index;
	bool detach;			/* true if a detach has been */
					/* executed for this context */
	gs_context *next;		/* next context with same status */
					/* (active, waiting on same lock, */
					/* waiting on same condition) */
	gs_context *joiner;		/* context waiting on a join */
					/* for this one */
	gs_context *table_next;		/* hash table chain */
		/* Externally visible context state */
	ref stacks;			/* t_array */
#define default_stacksize 50
	uint ossize;
	uint essize;
	uint dssize;
	gs_state *pgs;
	/****** MORE STUFF HERE ******/
};

/* Context list structure */
typedef struct ctx_list_s {
	gs_context *head;
	gs_context *tail;
} ctx_list;

/* Condition structure */
typedef struct gs_condition_s {
	ctx_list waiting;	/* contexts waiting on this condition */
} gs_condition;
gs_private_st_ptrs2(st_condition, gs_condition, "conditiontype",
  condition_enum_ptrs, condition_reloc_ptrs, waiting.head, waiting.tail);

/* Lock structure */
typedef struct gs_lock_s {
	ctx_list waiting;		/* contexts waiting for this lock, */
					/* must be first for subclassing */
	gs_context *holder;		/* context holding the lock, if any */
} gs_lock;
gs_private_st_suffix_add1(st_lock, gs_lock, "locktype",
  lock_enum_ptrs, lock_reloc_ptrs, st_condition, holder);

/* GC procedures */
#define ptr ((gs_context *)vptr)
private CLEAR_MARKS_PROC(context_clear_marks) {
	r_clear_attrs(&ptr->stacks, l_mark);
}
private ENUM_PTRS_BEGIN(context_enum_ptrs) return 0;
	ENUM_PTR(0, gs_context, next);
	ENUM_PTR(1, gs_context, joiner);
	ENUM_PTR(2, gs_context, table_next);
	case 3:
		*pep = &ptr->stacks;
		return ptr_ref_type;
	ENUM_PTR(4, gs_context, pgs);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(context_reloc_ptrs) {
	ref *pstk = &ptr->stacks;
	RELOC_PTR(gs_context, next);
	RELOC_PTR(gs_context, joiner);
	RELOC_PTR(gs_context, table_next);
	gs_reloc_refs((ref_packed *)pstk, (ref_packed *)(pstk + 1), gcst);
	r_clear_attrs(pstk, l_mark);
	RELOC_PTR(gs_context, pgs);
} RELOC_PTRS_END
#undef ptr
/* Structure type */
gs_private_st_complex_only(st_context, gs_context, "context",
  context_clear_marks, context_enum_ptrs, context_reloc_ptrs, 0);

/* Global state */
private gs_context *ctx_current;
private ctx_list active;
#define ctx_table_size 19
private gs_context *ctx_table[ctx_table_size];

/* Forward references */
private int context_create(P2(uint, gs_context **));
private int context_param(P2(os_ptr, gs_context **));
#define check_context(op, vpc)\
  if ( (code = context_param(op, &vpc)) < 0 ) return code
private void context_destroy(P1(gs_context *));
private int lock_acquire(P1(os_ptr));
private int lock_release(P1(os_ptr));

/* List manipulation macros */
#define add_last(pl,pc)\
  (((pl)->head == 0 ? ((pl)->head = pc) : ((pl)->tail->next = pc)),\
   (pl)->tail = pc, (pc)->next = 0)
#define add_last_all(pl,pcl)		/* pcl->head != 0 */\
  (((pl)->head == 0 ? ((pl)->head = (pcl)->head) :\
    ((pl)->tail->next = (pcl)->head)),\
   (pl)->tail = (pcl)->tail, (pcl)->head = 0)

/* ------ Initialization ------ */

private int ctx_reschedule(P0());
private int ctx_time_slice(P0());
private void
zcontext_init(void)
{	ctx_current = 0;
	active.head = 0;
	memset(ctx_table, 0, sizeof(ctx_table));
	/* Create an initial context. */
	context_create(default_stacksize, &ctx_current);
	/* Hook into the interpreter. */
	gs_interp_reschedule_proc = ctx_reschedule;
	gs_interp_time_slice_proc = ctx_time_slice;
	gs_interp_time_slice_ticks = 100;
}

/* ------ Interpreter interface to scheduler ------ */

/* When an operator decides it is time to run a new context, */
/* it returns o_reschedule.  The interpreter saves all its state in */
/* memory, calls ctx_reschedule, and then loads the state from memory. */
private int
ctx_reschedule(void)
{	register gs_context *pctx;
	ref *stkp;
	uint ossize, essize, dssize;
	/* Save the state of the current context in ctx_current, */
	/* if any context is current at all. */
	pctx = ctx_current;
	if ( pctx != 0 )
	   {	uint stackneed;
		int code;
		ref newstacks;
		ossize = osp - osbot + 1;
		essize = esp - esbot + 1;
		dssize = dsp - dsbot + 1;
		stackneed = ossize + essize + dssize;
		if ( stackneed > r_size(&pctx->stacks) )
		   {	ifree_ref_array(&pctx->stacks, "ctx_reschedule");
			code = ialloc_ref_array(&newstacks, 0, stackneed,
						"ctx_reschedule");
			if ( code < 0 )
			   {	/* Punt. */
				lprintf("Can't allocate stacks!");
				return_error(e_Fatal);
			   }
			pctx->stacks = newstacks;
		   }
		stkp = pctx->stacks.value.refs;
#define save_stack(sbot, ssize)\
  memcpy(stkp, sbot, ssize * sizeof(ref));\
  pctx->ssize = ssize;\
  stkp += ssize
		save_stack(osbot, ossize);
		save_stack(esbot, essize);
		save_stack(dsbot, dssize);
#undef save_stack
		pctx->pgs = igs;
		/****** MORE TO DO HERE ******/
	   }
	/* Run the first ready context. */
	if ( active.head == 0 )
	   {	lprintf("No context to run!");
		return_error(e_Fatal);
	   }
	ctx_current = active.head;
	active.head = active.head->next;
	/* Load the state of the new current context. */
	pctx = ctx_current;
	stkp = pctx->stacks.value.refs;
#define reload_stack(sbot, ssize, sp)\
  ssize = pctx->ssize;\
  memcpy(sbot, stkp, ssize * sizeof(ref));\
  sp = sbot + (ssize - 1);\
  stkp += ssize
	reload_stack(osbot, ossize, osp);
	reload_stack(esbot, essize, esp);
	esfile_clear_cache();
	reload_stack(dsbot, dssize, dsp);
#undef reload_stack
	dict_set_top();		/* reload dict stack cache */
	igs = pctx->pgs;
	/****** MORE TO DO HERE ******/
	return 0;
}

/* If the interpreter wants to time-slice, it saves its state, */
/* calls ctx_time_slice, and reloads its state. */
private int
ctx_time_slice(void)
{	if ( active.head == 0 ) return 0;
	add_last(&active, ctx_current);
	return ctx_reschedule();
}

/* ------ Context operators ------ */

private int fork_done(P1(os_ptr));

/* - currentcontext <context> */
private int
zcurrentcontext(register os_ptr op)
{	push(1);
	make_int(op, ctx_current->index);
	return 0;
}

/* <context> detach - */
private int
zdetach(register os_ptr op)
{	gs_context *pctx;
	int code;
	check_context(op, pctx);
	if ( pctx->joiner != 0 || pctx->detach )
		return_error(e_invalidcontext);
	pop(1);
	switch ( pctx->status )
	   {
	case cs_active:
		pctx->detach = true;
		break;
	case cs_done:
		context_destroy(pctx);
		if ( pctx == ctx_current )
		   {	ctx_current = 0;
			return o_reschedule;
		   }
	   }
	return 0;
}

/* <mark> <obj1> ... <objN> <proc> fork <context> */
private int
zfork(register os_ptr op)
{	os_ptr mp = op - 1;
	gs_context *pctx;
	uint ossize, essize, dssize, stacksize;
	int code;
	ref *stkp;
	check_proc(*op);
	while ( !r_has_type(mp, t_mark) )
	{	if ( mp <= osbot )
			return_error(e_unmatchedmark);
		mp--;
	}
	ossize = op - mp - 1;
	essize = 2;
	dssize = dsp - dsbot + 1;
	stacksize = ossize + essize + dssize + 10;
	code = context_create(stacksize, &pctx);
	if ( code < 0 ) return code;
	stkp = pctx->stacks.value.refs;
	pctx->ossize = ossize;
	memcpy(stkp, mp + 1, ossize * sizeof(ref));
	stkp += ossize;
	pctx->essize = essize;
	make_oper(stkp, 0, fork_done);
	stkp++;
	*stkp = *op;
	stkp++;
	pctx->dssize = dssize;
	memcpy(stkp, dsbot, dssize * sizeof(ref));
	pctx->pgs = igs;		/* ****** WRONG, MUST COPY ****** */
	/****** MORE INIT HERE? ******/
	add_last(&active, pctx);
	osp = mp;
	make_int(mp, pctx->index);
	return 0;
}
/* This gets executed when a context terminates normally. */
/****** HOW TO GET IT EXECUTED ON ERROR TERMINATION? ******/
private int
fork_done(os_ptr op)
{	if ( ctx_current->detach )
	   {	context_destroy(ctx_current);
		ctx_current = 0;
	   }
	else
	   {	gs_context *pctx = ctx_current->joiner;
		ctx_current->status = cs_done;
		/* Schedule the context waiting to join this one, if any. */
		if ( pctx != 0 ) add_last(&active, pctx);
	   }
	return o_reschedule;
}

/* <context> join <mark> <obj1> ... <objN> */
private int
zjoin(register os_ptr op)
{	gs_context *pctx;
	int code;
	check_context(op, pctx);
	if ( pctx->joiner != 0 || pctx == ctx_current || pctx->detach )
		return_error(e_invalidcontext);
	switch ( pctx->status )
	   {
	case cs_active:
		pctx->joiner = ctx_current;
		return o_reschedule;
	case cs_done:
	   {	uint count = pctx->ossize;
		os_ptr mp = op;
		push(count);
		make_mark(mp);
		memcpy(++mp, pctx->stacks.value.refs, count * sizeof(ref));
		context_destroy(pctx);
	   }
	   }
	return 0;
}

/* - yield - */
private int
zyield(register os_ptr op)
{	if ( active.head == 0 ) return 0;
	add_last(&active, ctx_current);
	return o_reschedule;
}

/* ------ Condition and lock operators ------ */

private int
  monitor_release(P1(os_ptr)),
  await_lock(P1(os_ptr));

/* - condition <condition> */
private int
zcondition(register os_ptr op)
{	gs_condition *pcond =
		ialloc_struct(gs_condition, &st_condition, "zcondition");
	if ( pcond == 0 )
		return_error(e_VMerror);
	pcond->waiting.head = 0;
	push(1);
	make_istruct(op, a_all, pcond);
	return 0;
}

/* - lock <lock> */
private int
zlock(register os_ptr op)
{	gs_lock *plock = ialloc_struct(gs_lock, &st_lock, "zlock");
	if ( plock == 0 )
		return_error(e_VMerror);
	plock->holder = 0;
	plock->waiting.head = 0;
	push(1);
	make_istruct(op, a_all, plock);
	return 0;
}

/* <lock> <proc> monitor - */
private int
zmonitor(register os_ptr op)
{	gs_lock *plock;
	int code;
	check_stype(op[-1], st_lock);
	check_proc(*op);
	plock = r_ptr(op - 1, gs_lock);
	check_estack(2);
	if ( plock->holder == ctx_current )
		return_error(e_invalidcontext);
	code = lock_acquire(op - 1);
	/****** HOW TO GUARANTEE RELEASE IF CONTEXT DIES? ******/
	push_op_estack(monitor_release);
	*++esp = op[-1];
	pop(2);
	return code;
}
/* Release the monitor lock when the procedure completes. */
private int
monitor_release(os_ptr op)
{	es_ptr ep = esp--;
	return lock_release(ep);
}

/* <condition> notify - */
private int
znotify(register os_ptr op)
{	gs_condition *pcond;
	check_stype(*op, st_condition);
	pcond = r_ptr(op, gs_condition);
	pop(1); op--;
	if ( pcond->waiting.head == 0 ) return 0;	/* nothing to do */
	add_last_all(&active, &pcond->waiting);
	return zyield(op);
}

/* <lock> <condition> wait - */
private int
zwait(register os_ptr op)
{	gs_condition *pcond;
	check_stype(op[-1], st_lock);
	check_stype(*op, st_condition);
	pcond = r_ptr(op, gs_condition);
	check_estack(1);
	lock_release(op - 1);
	add_last(&pcond->waiting, ctx_current);
	push_op_estack(await_lock);
	return o_reschedule;
}
/* When the condition is signaled, wait for acquiring the lock. */
private int
await_lock(os_ptr op)
{	int code = lock_acquire(op - 1);
	pop(2);
	return code;
}

/* ------ Internal routines ------ */

/* Create a context. */
private int
context_create(uint stacksize, gs_context **ppctx)
{	gs_context *pctx;
	int code;
	long ctx_index;
	gs_context **pte;
	pctx = ialloc_struct(gs_context, &st_context, "context");
	if ( pctx == 0 )
		return_error(e_VMerror);
	if ( stacksize < default_stacksize ) stacksize = default_stacksize;
	code = ialloc_ref_array(&pctx->stacks, 0, stacksize,
				"context(stacks)");
	if ( code < 0 ) return code;
	ctx_index = gs_next_ids(1);
	pctx->status = cs_active;
	pctx->index = ctx_index;
	pctx->detach = false;
	pctx->next = 0;
	pctx->joiner = 0;
	pte = &ctx_table[ctx_index % ctx_table_size];
	pctx->table_next = *pte;
	*pte = pctx;
	*ppctx = pctx;
	return 0;
}

/* Check a context ID.  Note that we do not check for context validity. */
private int
context_param(os_ptr op, gs_context **ppctx)
{	gs_context *pctx;
	long index;
	check_type(*op, t_integer);
	index = op->value.intval;
	if ( index < 0 )
		return_error(e_invalidcontext);
	pctx = ctx_table[index % ctx_table_size];
	for ( ; ; pctx = pctx->table_next )
	{	if ( pctx == 0 )
			return_error(e_invalidcontext);
		if ( pctx->index == index ) break;
	}
	*ppctx = pctx;
	return 0;
}

/* Destroy a context. */
private void
context_destroy(gs_context *pctx)
{	gs_context **ppctx = &ctx_table[pctx->index % ctx_table_size];
	while ( *ppctx != pctx )
		ppctx = &(*ppctx)->table_next;
	*ppctx = (*ppctx)->table_next;
	ifree_ref_array(&pctx->stacks, "context_destroy");
	ifree_object(pctx, "context_destroy");
}

/* Acquire a lock.  Return 0 if acquired, o_reschedule if not. */
private int
lock_acquire(os_ptr op)
{	gs_lock *plock = r_ptr(op, gs_lock);
	if ( plock->holder == 0 )
	   {	plock->holder = ctx_current;
		return 0;
	   }
	add_last(&plock->waiting, ctx_current);
	return o_reschedule;
}

/* Release a lock.  Return 0 if OK, e_invalidcontext if not. */
private int
lock_release(os_ptr op)
{	gs_lock *plock = r_ptr(op, gs_lock);
	if ( plock->holder == ctx_current )
	   {	plock->holder = 0;
		add_last_all(&active, &plock->waiting);
		return 0;
	   }
	return_error(e_invalidcontext);
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zcontext_l2_op_defs) {
		op_def_begin_level2(),
	{"0condition", zcondition},
	{"0currentcontext", zcurrentcontext},
	{"1detach", zdetach},
	{"2fork", zfork},
	{"1join", zjoin},
	{"0lock", zlock},
	{"2monitor", zmonitor},
	{"1notify", znotify},
	{"2wait", zwait},
	{"0yield", zyield},
		/* Internal operators */
	{"0%fork_done", fork_done},
	{"2%monitor_release", monitor_release},
	{"2%await_lock", await_lock},
END_OP_DEFS(zcontext_init) }
