/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: interp.h,v 1.2 2000/03/08 23:15:14 mike Exp $ */
/* Internal interfaces to interp.c and iinit.c */

#ifndef interp_INCLUDED
#  define interp_INCLUDED

/* ------ iinit.c ------ */

/* Enter a name and value into systemdict. */
void initial_enter_name(P2(const char *, const ref *));

/* Remove a name from systemdict. */
void initial_remove_name(P1(const char *));

/* ------ interp.c ------ */

/*
 * Maximum number of arguments (and results) for an operator,
 * determined by operand stack block size.
 */
extern const int gs_interp_max_op_num_args;

/*
 * Number of slots to reserve at the start of op_def_table for
 * operators which are hard-coded into the interpreter loop.
 */
extern const int gs_interp_num_special_ops;

/*
 * Create an operator during initialization.
 * If operator is hard-coded into the interpreter,
 * assign it a special type and index.
 */
void gs_interp_make_oper(P3(ref * opref, op_proc_p, int index));

/* Get the name corresponding to an error number. */
int gs_errorname(P2(int, ref *));

/* Put a string in $error /errorinfo. */
int gs_errorinfo_put_string(P1(const char *));

/* Initialize the interpreter. */
void gs_interp_init(P0());

#ifndef gs_context_state_t_DEFINED
#  define gs_context_state_t_DEFINED
typedef struct gs_context_state_s gs_context_state_t;

#endif

/* Define a pointer to the current interpreter context state. */
extern gs_context_state_t *gs_interp_context_state_current;

/*
 * Create initial stacks for the interpreter.
 * We export this for creating new contexts.
 */
int gs_interp_alloc_stacks(P2(gs_ref_memory_t * smem,
			      gs_context_state_t * pcst));

/*
 * Free the stacks when destroying a context.  This is the inverse of
 * create_stacks.
 */
void gs_interp_free_stacks(P2(gs_ref_memory_t * smem,
			      gs_context_state_t * pcst));

/* Reset the interpreter. */
void gs_interp_reset(P0());

#endif /* interp_INCLUDED */
