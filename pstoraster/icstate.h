/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: icstate.h,v 1.1 2000/03/13 19:00:48 mike Exp $ */
/* Externally visible context state */
/* Requires iref.h */

#ifndef icstate_INCLUDED
#  define icstate_INCLUDED

#include "imemory.h"

/*
 * Define the externally visible state of an interpreter context.
 * If we aren't supporting Display PostScript features, there is only
 * a single context.
 */
#ifndef gs_context_state_t_DEFINED
#  define gs_context_state_t_DEFINED
typedef struct gs_context_state_s gs_context_state_t;
#endif
#ifndef ref_stack_DEFINED
#  define ref_stack_DEFINED
typedef struct ref_stack_s ref_stack;
#endif
struct gs_context_state_s {
    ref_stack *dstack;
    ref_stack *estack;
    ref_stack *ostack;
    gs_state *pgs;
    gs_dual_memory_t memory;
    ref array_packing;		/* t_boolean */
    ref binary_object_format;	/* t_integer */
    long rand_state;		/* (not in Red Book) */
    long usertime_total;	/* total accumulated usertime, */
    /* not counting current time if running */
    bool keep_usertime;		/* true if context ever executed usertime */
    /* View clipping is handled in the graphics state. */
    ref userparams;		/* t_dictionary */
    ref stdio[2];		/* t_file */
};

/*
 * We make st_context_state public because interp.c must allocate one,
 * and zcontext.c must subclass it.
 */
/*extern_st(st_context_state); *//* in icontext.h */
#define public_st_context_state()	/* in icontext.c */\
  gs_public_st_complex_only(st_context_state, gs_context_state_t,\
    "gs_context_state_t", context_state_clear_marks,\
    context_state_enum_ptrs, context_state_reloc_ptrs, 0)

#endif /* icstate_INCLUDED */
