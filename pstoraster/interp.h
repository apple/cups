/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* interp.h */
/* Internal interfaces to interp.c and iinit.c */

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
void gs_interp_make_oper(P3(ref *opref, op_proc_p, int index));

/* Get the name corresponding to an error number. */
int gs_errorname(P2(int, ref *));

/* Put a string in $error /errorinfo. */
int gs_errorinfo_put_string(P1(const char *));

/* Initialize the interpreter. */
void gs_interp_init(P0());

/* Reset the interpreter. */
void gs_interp_reset(P0());
