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

/*$Id: gsfunc.h,v 1.1 2000/03/13 18:57:55 mike Exp $ */
/* Generic definitions for Functions */

#ifndef gsfunc_INCLUDED
#  define gsfunc_INCLUDED

/* ---------------- Types and structures ---------------- */

/*
 * gs_function_type_t is defined as equivalent to int, rather than as an
 * enum type, because we can't enumerate all its possible values here in the
 * generic definitions.
 */
typedef int gs_function_type_t;

/*
 * Define information common to all Function types.
 * We separate the private part from the parameters so that
 * clients can create statically initialized parameter structures.
 */
#define gs_function_params_common\
    int m;			/* # of inputs */\
    const float *Domain;	/* 2 x m */\
    int n;			/* # of outputs */\
    const float *Range		/* 2 x n, optional except for type 0 */

/* Define a generic function, for use as the target type of pointers. */
typedef struct gs_function_params_s {
    gs_function_params_common;
} gs_function_params_t;
typedef struct gs_function_s gs_function_t;
typedef int (*fn_evaluate_proc_t)(P3(const gs_function_t * pfn,
				     const float *in, float *out));
typedef int (*fn_is_monotonic_proc_t)(P4(const gs_function_t * pfn,
					 const float *lower,
					 const float *upper,
					 bool must_know));
typedef void (*fn_free_params_proc_t)(P2(gs_function_params_t * params,
					 gs_memory_t * mem));
typedef void (*fn_free_proc_t)(P3(gs_function_t * pfn,
				  bool free_params, gs_memory_t * mem));
typedef struct gs_function_head_s {
    gs_function_type_t type;
    fn_evaluate_proc_t evaluate;
    fn_is_monotonic_proc_t is_monotonic;
    fn_free_params_proc_t free_params;
    fn_free_proc_t free;
} gs_function_head_t;
struct gs_function_s {
    gs_function_head_t head;
    gs_function_params_t params;
};

#define FunctionType(pfn) ((pfn)->head.type)

/*
 * Each specific function type has a definition in its own header file
 * for its parameter record.  In order to keep names from overflowing
 * various compilers' limits, we take the name of the function type and
 * reduce it to the first and last letter of each word, e.g., for
 * Sampled functions, XxYy is Sd.

typedef struct gs_function_XxYy_params_s {
     gs_function_params_common;
    << P additional members >>
} gs_function_XxYy_params_t;
#define private_st_function_XxYy()\
  gs_private_st_suffix_addP(st_function_XxYy, gs_function_XxYy_t,\
    "gs_function_XxYy_t", function_XxYy_enum_ptrs, function_XxYy_reloc_ptrs,\
    st_function, <<params.additional_members>>)

 */

/* ---------------- Procedures ---------------- */

/*
 * Each specific function type has a pair of procedures in its own
 * header file, one to allocate and initialize an instance of that type,
 * and one to free the parameters of that type.

int gs_function_XxYy_init(P3(gs_function_t **ppfn,
			     const gs_function_XxYy_params_t *params,
			     gs_memory_t *mem));

void gs_function_XxYy_free_params(P2(gs_function_XxYy_params_t *params,
				     gs_memory_t *mem));

 */

/* Evaluate a function. */
#define gs_function_evaluate(pfn, in, out)\
  (*(pfn)->head.evaluate)(pfn, in, out)

/*
 * Test whether a function is monotonic on a given (closed) interval.  If
 * must_know is true, returns 0 for false, 1 for true, gs_error_rangecheck
 * if any part of the interval is outside the function's domain; if
 * must_know is false, may also return gs_error_undefined to mean "can't
 * determine quickly".  If lower[i] > upper[i], the result is not defined.
 */
#define gs_function_is_monotonic(pfn, lower, upper, must_know)\
  (*(pfn)->head.is_monotonic)(pfn, lower, upper, must_know)

/* Free function parameters. */
#define gs_function_free_params(pfn, mem)\
  (*(pfn)->head.free_params)(&(pfn)->params, mem)

/* Free a function's implementation, optionally including its parameters. */
#define gs_function_free(pfn, free_params, mem)\
  (*(pfn)->head.free)(pfn, free_params, mem)

#endif /* gsfunc_INCLUDED */
