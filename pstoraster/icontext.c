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

/*$Id: icontext.c,v 1.1 2000/03/08 23:15:09 mike Exp $ */
/* Context state operations */
#include "ghost.h"
#include "gsstruct.h"		/* for gxalloc.h */
#include "gxalloc.h"
#include "errors.h"
#include "stream.h"		/* for files.h */
#include "files.h"
#include "idict.h"
#include "igstate.h"
#include "icontext.h"
#include "interp.h"
#include "isave.h"
#include "dstack.h"
#include "estack.h"
#include "ostack.h"
#include "store.h"

/* Define the initial stack sizes. */
#define DSTACK_INITIAL 20
#define ESTACK_INITIAL 250
#define OSTACK_INITIAL 200

/* Per-context state stored in statics */
extern ref ref_array_packing;
extern ref ref_binary_object_format;
extern long zrand_state;

				/*extern ref ref_stdio[3]; *//* in files.h */

/* Initialization procedures */
void zrand_state_init(P1(long *));

/* GC descriptors */
extern_st(st_ref_stack);
#define pcst ((gs_context_state_t *)vptr)
private 
CLEAR_MARKS_PROC(context_state_clear_marks)
{
    r_clear_attrs(&pcst->userparams, l_mark);
}
private 
ENUM_PTRS_BEGIN(context_state_enum_ptrs) return 0;

ENUM_PTR3(0, gs_context_state_t, dstack, estack, ostack);
ENUM_PTR3(3, gs_context_state_t, pgs, stdio[0].value.pstruct,
	  stdio[1].value.pstruct);
case 6:
ENUM_RETURN_REF(&pcst->userparams);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(context_state_reloc_ptrs);
RELOC_PTR3(gs_context_state_t, dstack, estack, ostack);
RELOC_PTR3(gs_context_state_t, pgs, stdio[0].value.pstruct,
	   stdio[1].value.pstruct);
RELOC_REF_VAR(pcst->userparams);
r_clear_attrs(&pcst->userparams, l_mark);
RELOC_PTRS_END
#undef pcst
public_st_context_state();

/* Allocate the state of a context. */
     int
         context_state_alloc(gs_context_state_t ** ppcst,
			     const gs_dual_memory_t * dmem)
{
    gs_ref_memory_t *mem = dmem->space_local;
    gs_context_state_t *pcst = *ppcst;
    int code;
    int i;

    if (pcst == 0) {
	pcst = gs_alloc_struct((gs_memory_t *) mem, gs_context_state_t,
			       &st_context_state, "context_state_alloc");
	if (pcst == 0)
	    return_error(e_VMerror);
    }
    code = gs_interp_alloc_stacks(mem, pcst);
    if (code < 0)
	goto x0;
    pcst->pgs = int_gstate_alloc(mem);
    if (pcst->pgs == 0) {
	code = gs_note_error(e_VMerror);
	goto x1;
    }
    pcst->memory = *dmem;
    make_false(&pcst->array_packing);
    make_int(&pcst->binary_object_format, 0);
    zrand_state_init(&pcst->rand_state);
    pcst->usertime_total = 0;
    pcst->keep_usertime = false;
    {				/*
				 * Create an empty userparams dictionary of the right size.
				 * If we can't determine the size, pick an arbitrary one.
				 */
	ref *puserparams;
	uint size;

	if (dict_find_string(systemdict, "userparams", &puserparams) >= 0)
	    size = dict_length(puserparams);
	else
	    size = 20;
	code = dict_alloc(pcst->memory.space_local, size, &pcst->userparams);
	if (code < 0)
	    goto x2;
	/* PostScript code initializes the user parameters. */
    }
    /* The initial stdio values are bogus.... */
    make_file(&pcst->stdio[0], 0, 1, invalid_file_entry);
    make_file(&pcst->stdio[1], 0, 1, invalid_file_entry);
    for (i = countof(pcst->memory.spaces.indexed); --i >= 0;)
	if (dmem->spaces.indexed[i] != 0)
	    ++(dmem->spaces.indexed[i]->num_contexts);
    *ppcst = pcst;
    return 0;
  x2:gs_state_free(pcst->pgs);
  x1:gs_interp_free_stacks(mem, pcst);
  x0:if (*ppcst == 0)
	gs_free_object((gs_memory_t *) mem, pcst, "context_state_alloc");
    return code;
}

/* Load the interpreter state from a context. */
int
context_state_load(const gs_context_state_t * pcst)
{
    gs_ref_memory_t *lmem = iimemory_local;
    uint space = r_space(systemdict);
    int code;

    d_stack = *pcst->dstack;
    e_stack = *pcst->estack;
    o_stack = *pcst->ostack;
    igs = pcst->pgs;
    gs_imemory = pcst->memory;
    ref_array_packing = pcst->array_packing;
    ref_binary_object_format = pcst->binary_object_format;
    zrand_state = pcst->rand_state;
    /*
     * Set systemdict.userparams to the saved copy, and then
     * set the actual user parameters.  Be careful to disable both
     * space checking and save checking while we do this.
     */
    r_set_space(systemdict, avm_max);
    alloc_set_not_in_save(idmemory);
    code = dict_put_string(systemdict, "userparams", &pcst->userparams);
    if (code >= 0)
	code = set_user_params(&pcst->userparams);
    ref_stdio[0] = pcst->stdio[0];
    ref_stdio[1] = pcst->stdio[1];
    if (iimemory_local != lmem) {
	/*
	 * Switch references in systemdict to local objects.
	 * userdict.localdicts holds these objects.
	 */
	const ref *puserdict =
	ref_stack_index(&d_stack, ref_stack_count(&d_stack) - 1 -
			dstack_userdict_index);
	ref *plocaldicts;

	if (dict_find_string(puserdict, "localdicts", &plocaldicts) > 0 &&
	    r_has_type(plocaldicts, t_dictionary)
	    ) {
	    dict_copy(plocaldicts, systemdict);
	}
    }
    r_set_space(systemdict, space);
    if (idmemory->save_level > 0)
	alloc_set_in_save(idmemory);
    esfile_clear_cache();
    dict_set_top();		/* reload dict stack cache */
    return code;
}

/* Store the interpreter state in a context. */
int
context_state_store(gs_context_state_t * pcst)
{
    ref_stack_cleanup(&d_stack);
    ref_stack_cleanup(&e_stack);
    ref_stack_cleanup(&o_stack);
    *pcst->dstack = d_stack;
    *pcst->estack = e_stack;
    *pcst->ostack = o_stack;
    pcst->pgs = igs;
    pcst->memory = gs_imemory;
    pcst->array_packing = ref_array_packing;
    pcst->binary_object_format = ref_binary_object_format;
    pcst->rand_state = zrand_state;
    /*
     * The user parameters in systemdict.userparams are kept
     * up to date by PostScript code, but we still need to save
     * systemdict.userparams to get the correct l_new flag.
     */
    {
	ref *puserparams;

	if (dict_find_string(systemdict, "userparams", &puserparams) < 0)
	    return_error(e_Fatal);
	pcst->userparams = *puserparams;
    }
    pcst->stdio[0] = ref_stdio[0];
    pcst->stdio[1] = ref_stdio[1];
    return 0;
}

/* Free the state of a context. */
bool
context_state_free(gs_context_state_t * pcst)
{
    gs_ref_memory_t *mem = pcst->memory.space_local;
    int freed = 0;
    int i;

    /*
     * If this context is the last one referencing a particular VM
     * (local / global / system), free the entire VM space;
     * otherwise, just free the context-related structures.
     */
    for (i = countof(pcst->memory.spaces.indexed); --i >= 0;) {
	if (pcst->memory.spaces.indexed[i] != 0 &&
	    !--(pcst->memory.spaces.indexed[i]->num_contexts)
	    ) {
/****** FREE THE ENTIRE SPACE ******/
	    freed |= 1 << i;
	}
    }
    /*
     * If we freed any spaces at all, we must have freed the local
     * VM where the context structure and its substructures were
     * allocated.
     */
    if (freed)
	return freed;
    {
	gs_state *pgs = pcst->pgs;

	gs_grestoreall(pgs);
	/* Patch the saved pointer so we can do the last grestore. */
	{
	    gs_state *saved = gs_state_saved(pgs);

	    gs_state_swap_saved(saved, saved);
	}
	gs_grestore(pgs);
	gs_state_swap_saved(pgs, (gs_state *) 0);
	gs_state_free(pgs);
    }
/****** FREE USERPARAMS ******/
    gs_interp_free_stacks(mem, pcst);
    return false;
}
