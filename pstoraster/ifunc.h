/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: ifunc.h,v 1.1 2000/03/13 19:00:48 mike Exp $ */
/* Internal interpreter interfaces for Functions */

#ifndef ifunc_INCLUDED
#  define ifunc_INCLUDED

/* Define build procedures for the various function types. */
#define build_function_proc(proc)\
  int proc(P4(const_os_ptr op, const gs_function_params_t *params, int depth,\
	      gs_function_t **ppfn))
build_function_proc(build_function_undefined);

/* Define the table of build procedures, indexed by FunctionType. */
extern build_function_proc((*build_function_procs[5]));

/* Build a function structure from a PostScript dictionary. */
int fn_build_sub_function(P3(const ref * op, gs_function_t ** ppfn, int depth));

#define fn_build_function(op, ppfn)\
  fn_build_sub_function(op, ppfn, 0)

/* Allocate an array of function objects. */
int ialloc_function_array(P2(uint count, gs_function_t *** pFunctions));

/*
 * Collect a heap-allocated array of floats.  If the key is missing, set
 * *pparray = 0 and return 0; otherwise set *pparray and return the number
 * of elements.  Note that 0-length arrays are acceptable, so if the value
 * returned is 0, the caller must check whether *pparray == 0.
 */
int fn_build_float_array(P5(const ref * op, const char *kstr, bool required,
			    bool even, const float **pparray));

#endif /* ifunc_INCLUDED */
