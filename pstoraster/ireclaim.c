/* Copyright (C) 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: ireclaim.c,v 1.2 2000/03/08 23:15:15 mike Exp $ */
/* Interpreter's interface to garbage collector */
#include "ghost.h"
#include "errors.h"
#include "gsstruct.h"
#include "iastate.h"
#include "icontext.h"
#include "interp.h"
#include "isave.h"		/* for isstate.h */
#include "isstate.h"		/* for mem->saved->state */
#include "dstack.h"		/* for dsbot, dsp, dict_set_top */
#include "estack.h"		/* for esbot, esp */
#include "ostack.h"		/* for osbot, osp */
#include "opdef.h"		/* for defining init procedure */
#include "store.h"		/* for make_array */

/* Import preparation and cleanup routines. */
extern void ialloc_gc_prepare(P1(gs_ref_memory_t *));

/* Forward references */
private void gs_vmreclaim(P2(gs_dual_memory_t *, bool));

/* Initialize the GC hook in the allocator. */
private int ireclaim(P2(gs_dual_memory_t *, int));
private void
ireclaim_init(void)
{
    gs_imemory.reclaim = ireclaim;
}

/* GC hook called when the allocator returns a VMerror (space = -1), */
/* or for vmreclaim (space = the space to collect). */
private int
ireclaim(gs_dual_memory_t * dmem, int space)
{
    bool global;
    gs_ref_memory_t *mem;

    if (space < 0) {		/* Determine which allocator got the VMerror. */
	gs_memory_status_t stats;
	int i;

	mem = dmem->space_global;	/* just in case */
	for (i = 0; i < countof(dmem->spaces.indexed); ++i) {
	    mem = dmem->spaces.indexed[i];
	    if (mem == 0)
		continue;
	    if (mem->gc_status.requested > 0)
		break;
	}
	gs_memory_status((gs_memory_t *) mem, &stats);
	if (stats.allocated >= mem->gc_status.max_vm) {		/* We can't satisfy this request within max_vm. */
	    return_error(e_VMerror);
	}
    } else {
	mem = dmem->spaces.indexed[space >> r_space_shift];
    }
    if_debug3('0', "[0]GC called, space=%d, requestor=%d, requested=%ld\n",
	      space, mem->space, (long)mem->gc_status.requested);
    global = mem->space != avm_local;
    gs_vmreclaim(dmem, global);

    ialloc_set_limit(mem);
    ialloc_reset_requested(dmem);
    return 0;
}

/* Interpreter entry to garbage collector. */
private void
gs_vmreclaim(gs_dual_memory_t * dmem, bool global)
{
    gs_ref_memory_t *lmem = dmem->space_local;
    gs_ref_memory_t *gmem = dmem->space_global;
    gs_ref_memory_t *smem = dmem->space_system;
    int code = context_state_store(gs_interp_context_state_current);

/****** ABORT IF code < 0 ******/
    alloc_close_chunk(lmem);
    if (gmem != lmem)
	alloc_close_chunk(gmem);
    alloc_close_chunk(smem);

    /* Prune the file list so it won't retain potentially collectible */
    /* files. */

    {
	int i;

	for (i = (global ? i_vm_system : i_vm_local);
	     i < countof(dmem->spaces.indexed);
	     ++i
	    ) {
	    gs_ref_memory_t *mem = dmem->spaces.indexed[i];

	    if (mem == 0 || (i > 0 && mem == dmem->spaces.indexed[i - 1]))
		continue;
	    for (;; mem = &mem->saved->state) {
		ialloc_gc_prepare(mem);
		if (mem->saved == 0)
		    break;
	    }
	}
    }

    /* Do the actual collection. */

    gs_reclaim(&dmem->spaces, global);

    /* Reload the context state. */

    code = context_state_load(gs_interp_context_state_current);
/****** ABORT IF code < 0 ******/

    /* Update the cached value pointers in names. */

    dicts_gc_cleanup();

    /* Reopen the active chunks. */

    alloc_open_chunk(smem);
    if (gmem != lmem)
	alloc_open_chunk(gmem);
    alloc_open_chunk(lmem);

    /* Update caches not handled by context_state_load. */

    {
	uint dcount = ref_stack_count(&d_stack);

	*systemdict = *ref_stack_index(&d_stack, dcount - 1);
    }
}

/* ------ Initialization procedure ------ */

const op_def ireclaim_l2_op_defs[] =
{
    op_def_end(ireclaim_init)
};
