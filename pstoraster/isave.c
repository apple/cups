/* Copyright (C) 1993, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: isave.c,v 1.2.2.1 2001/12/26 16:52:49 mike Exp $ */
/* Save/restore manager for Ghostscript interpreter */
#include "ghost.h"
#include "memory_.h"
#include "errors.h"
#include "gsexit.h"
#include "gsstruct.h"
#include "stream.h"		/* for linking for forgetsave */
#include "iastate.h"
#include "inamedef.h"
#include "iname.h"
#include "ipacked.h"
#include "isave.h"
#include "isstate.h"
#include "store.h"		/* for ref_assign */
#include "ivmspace.h"
#include "gsutil.h"		/* gs_next_ids prototype */

/* Imported save/restore routines */
extern void font_restore(P1(const alloc_save_t *));

/* Structure descriptor */
private_st_alloc_save();

/* Define the maximum amount of data we are willing to scan repeatedly -- */
/* see below for details. */
private const long max_repeated_scan = 100000;

/* Some compilers try to substitute macro args in string literals! */
#define print_save(str, spacen, sav)\
  if_debug5('u', "[u]%s space %u 0x%lx: cdata = 0x%lx, id = %lu\n",\
    str, spacen, (ulong)(sav), (ulong)(sav)->client_data, (ulong)(sav)->id);

/*
 * The logic for saving and restoring the state is complex.
 * Both the changes to individual objects, and the overall state
 * of the memory manager, must be saved and restored.
 */

/*
 * To save the state of the memory manager:
 *      Save the state of the current chunk in which we are allocating.
 *      Shrink the current chunk to its inner unallocated region.
 *      Save and reset the free block chains.
 * By doing this, we guarantee that no object older than the save
 * can be freed.
 *
 * To restore the state of the memory manager:
 *      Free all chunks newer than the save, and the descriptor for
 *        the inner chunk created by the save.
 *      Make current the chunk that was current at the time of the save.
 *      Restore the state of the current chunk.
 *
 * In addition to save ("start transaction") and restore ("abort transaction"),
 * we support forgetting a save ("commit transation").  To forget a save:
 *      Reassign to the next outer save all chunks newer than the save.
 *      Free the descriptor for the inner chunk, updating its outer chunk
 *        to reflect additional allocations in the inner chunk.
 *      Concatenate the free block chains with those of the outer save.
 */

/*
 * For saving changes to individual objects, we add an "attribute" bit
 * (l_new) that logically belongs to the slot where the ref is stored,
 * not to the ref itself.  The bit means "the contents of this slot
 * have been changed, or the slot was allocated, since the last save."
 * To keep track of changes since the save, we associate a chain of
 * <slot, old_contents> pairs that remembers the old contents of slots.
 *
 * When creating an object, if the save level is non-zero:
 *      Set l_new in all slots.
 *
 * When storing into a slot, if the save level is non-zero:
 *      If l_new isn't set, save the address and contents of the slot
 *        on the current contents chain.
 *      Set l_new after storing the new value.
 *
 * To do a save:
 *      If the save level is non-zero:
 *              Reset l_new in all slots on the contents chain, and in all
 *                objects created since the previous save.
 *      Push the head of the contents chain, and reset the chain to empty.
 *
 * To do a restore:
 *      Check all the stacks to make sure they don't contain references
 *        to objects created since the save.
 *      Restore all the slots on the contents chain.
 *      Pop the contents chain head.
 *      If the save level is now non-zero:
 *              Scan the newly restored contents chain, and set l_new in all
 *                the slots it references.
 *              Scan all objects created since the previous save, and set
 *                l_new in all the slots of each object.
 *
 * To forget a save:
 *      If the save level is greater than 1:
 *              Set l_new as for a restore, per the next outer save.
 *              Concatenate the next outer contents chain to the end of
 *                the current one.
 *      If the save level is 1:
 *              Reset l_new as for a save.
 *              Free the contents chain.
 */

/*
 * A consequence of the foregoing algorithms is that the cost of a save
 * is proportional to the total amount of data allocated since the previous
 * save.  If a PostScript program reads in a large amount of setup code
 * and then uses save/restore heavily, each save/restore will be expensive.
 * To mitigate this, we check to see how much data we are scanning at a save;
 * if it is large, we do a second, invisible save.  This greatly reduces
 * the cost of inner saves, at the expense of possibly saving some changes
 * twice that otherwise would only have to be saved once.
 */

/*
 * The presence of global and local VM complicates the situation further.
 * There is a separate save chain and contents chain for each VM space.
 * When multiple contexts are fully implemented, save and restore will have
 * the following effects, according to the privacy status of the current
 * context's global and local VM:
 *      Private global, private local:
 *              The outermost save saves both global and local VM;
 *                otherwise, save only saves local VM.
 *      Shared global, private local:
 *              Save only saves local VM.
 *      Shared global, shared local:
 *              Save only saves local VM, and suspends all other contexts
 *                sharing the same local VM until the matching restore.
 * Since we do not currently implement multiple contexts, only the first
 * case is relevant.
 *
 * Note that when saving the contents of a slot, the choice of chain
 * is determined by the VM space in which the slot is allocated,
 * not by the current allocation mode.
 */

/*
 * Structure for saved change chain for save/restore.  Because of the
 * garbage collector, we need to distinguish the cases where the change
 * is in a static object, a dynamic ref, or a dynamic struct.
 */
typedef struct alloc_change_s alloc_change_t;
struct alloc_change_s {
    alloc_change_t *next;
    ref_packed *where;
    ref contents;
#define ac_offset_static (-2)	/* static object */
#define ac_offset_ref (-1)	/* dynamic ref */
    short offset;		/* if >= 0, offset within struct */
};

#define ptr ((alloc_change_t *)vptr)
private 
CLEAR_MARKS_PROC(change_clear_marks)
{
    if (r_is_packed(&ptr->contents))
	r_clear_pmark((ref_packed *) & ptr->contents);
    else
	r_clear_attrs(&ptr->contents, l_mark);
}
private 
ENUM_PTRS_BEGIN(change_enum_ptrs) return 0;

ENUM_PTR(0, alloc_change_t, next);
case 1:
if (ptr->offset >= 0)
    ENUM_RETURN((byte *) ptr->where - ptr->offset);
else
    ENUM_RETURN_REF(ptr->where);
case 2:
ENUM_RETURN_REF(&ptr->contents);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(change_reloc_ptrs)
{
    RELOC_VAR(ptr->next);
    switch (ptr->offset) {
	case ac_offset_static:
	    break;
	case ac_offset_ref:
	    RELOC_REF_PTR_VAR(ptr->where);
	    break;
	default:
	    {
		byte *obj = (byte *) ptr->where - ptr->offset;

		RELOC_VAR(obj);
		ptr->where = (ref_packed *) (obj + ptr->offset);
	    }
	    break;
    }
    if (r_is_packed(&ptr->contents))
	r_clear_pmark((ref_packed *) & ptr->contents);
    else {
	RELOC_REF_VAR(ptr->contents);
	r_clear_attrs(&ptr->contents, l_mark);
    }
}
RELOC_PTRS_END
#undef ptr
gs_private_st_complex_only(st_alloc_change, alloc_change_t, "alloc_change",
		change_clear_marks, change_enum_ptrs, change_reloc_ptrs, 0);

/* Debugging printout */
#ifdef DEBUG
private void
alloc_save_print(alloc_change_t * cp, bool print_current)
{
    dprintf2(" 0x%lx: 0x%lx: ", (ulong) cp, (ulong) cp->where);
    if (r_is_packed(&cp->contents)) {
	if (print_current)
	    dprintf2("saved=%x cur=%x\n", *(ref_packed *) & cp->contents,
		     *cp->where);
	else
	    dprintf1("%x\n", *(ref_packed *) & cp->contents);
    } else {
	if (print_current)
	    dprintf6("saved=%x %x %lx cur=%x %x %lx\n",
		     r_type_attrs(&cp->contents), r_size(&cp->contents),
		     (ulong) cp->contents.value.intval,
		     r_type_attrs((ref *) cp->where),
		     r_size((ref *) cp->where),
		     (ulong) ((ref *) cp->where)->value.intval);
	else
	    dprintf3("%x %x %lx\n",
		     r_type_attrs(&cp->contents), r_size(&cp->contents),
		     (ulong) cp->contents.value.intval);
    }
}
#endif

/* Forward references */
private void restore_resources(P2(alloc_save_t *, gs_ref_memory_t *));
private void restore_free(P1(gs_ref_memory_t *));
private long save_set_new(P2(gs_ref_memory_t *, bool));
private void save_set_new_changes(P2(gs_ref_memory_t *, bool));

/* Initialize the save/restore machinery. */
void
alloc_save_init(gs_dual_memory_t * dmem)
{
    dmem->save_level = 0;
    alloc_set_not_in_save(dmem);
}

/* Record that we are in a save. */
void
alloc_set_in_save(gs_dual_memory_t *dmem)
{
    dmem->test_mask = dmem->new_mask = l_new;
}

/* Record that we are not in a save. */
void
alloc_set_not_in_save(gs_dual_memory_t *dmem)
{
    dmem->test_mask = ~0;
    dmem->new_mask = 0;
}

/* Save the state. */
private alloc_save_t *alloc_save_space(P2(gs_ref_memory_t *,
					  gs_dual_memory_t *));

ulong
alloc_save_state(gs_dual_memory_t * dmem, void *cdata)
{
    gs_ref_memory_t *lmem = dmem->space_local;
    gs_ref_memory_t *gmem = dmem->space_global;
    ulong sid = gs_next_ids(2);

#define alloc_free_save(mem, s, scn, icn)\
  { chunk_t *inner = (mem)->pcc;\
    gs_free_object((gs_memory_t *)(mem), s, scn);\
    gs_free_object((mem)->parent, inner, icn);\
  }
    bool global =
    dmem->save_level == 0 && gmem != lmem &&
    gmem->num_contexts == 1;
    alloc_save_t *gsave =
    (global ? alloc_save_space(gmem, dmem) : (alloc_save_t *) 0);
    alloc_save_t *lsave = alloc_save_space(lmem, dmem);

    if (lsave == 0 || (global &&gsave == 0)) {
	if (lsave != 0)
	    alloc_free_save(lmem, lsave, "alloc_save_state(local save)",
			    "alloc_save_state(local inner)");
	if (gsave != 0)
	    alloc_free_save(gmem, gsave, "alloc_save_state(global save)",
			    "alloc_save_state(global inner)");
	return 0;
    }
#undef alloc_free_save
    if (gsave != 0) {
	gsave->id = sid + 1;
	gsave->client_data = 0;
	print_save("save", gmem->space, gsave);
	/* Restore names when we do the local restore. */
	lsave->restore_names = gsave->restore_names;
	gsave->restore_names = false;
    }
    lsave->id = sid;
    lsave->client_data = cdata;
    print_save("save", lmem->space, lsave);
    /* Reset the l_new attribute in all slots.  The only slots that */
    /* can have the attribute set are the ones on the changes chain, */
    /* and ones in objects allocated since the last save. */
    if (dmem->save_level != 0) {
	long scanned = save_set_new(&lsave->state, false);

	if (scanned > max_repeated_scan) {	/* Do a second, invisible save. */
	    alloc_save_t *rsave;

	    rsave = alloc_save_space(lmem, dmem);
	    if (rsave != 0) {
		rsave->id = sid;
		rsave->client_data = cdata;
		print_save("save", lmem->space, rsave);
		lsave->id = 0;	/* mark as invisible */
		lsave->client_data = 0;
		/* Inherit the allocated space count -- */
		/* we need this for triggering a GC. */
		rsave->state.inherited =
		    lsave->state.allocated +
		    lsave->state.inherited;
		lmem->inherited = rsave->state.inherited;
		print_save("save", lmem->space, lsave);
	    }
	}
    }
    dmem->save_level++;
    alloc_set_in_save(dmem);
    return sid;
}
/* Save the state of one space (global or local). */
private alloc_save_t *
alloc_save_space(gs_ref_memory_t * mem, gs_dual_memory_t * dmem)
{
    gs_ref_memory_t save_mem;
    alloc_save_t *save;
    chunk_t *inner = 0;

    if (mem->cc.ctop - mem->cc.cbot > sizeof(chunk_head_t)) {
	inner = gs_raw_alloc_struct_immovable(mem->parent, &st_chunk,
					      "alloc_save_space(inner)");
	if (inner == 0)
	    return 0;
    }
    save_mem = *mem;
    alloc_close_chunk(mem);
    gs_memory_status((gs_memory_t *) mem, &mem->previous_status);
    ialloc_reset(mem);
    mem->cc.cnext = mem->cc.cprev = 0;
    if (inner != 0) {		/* Create an inner chunk to cover only the unallocated part. */
	alloc_init_chunk(&mem->cc, mem->cc.cbot, mem->cc.ctop,
			 true, mem->pcc);
	*inner = mem->cc;
	mem->pcc = inner;
	mem->cfirst = mem->clast = inner;
    } else {			/* Not enough room to create an inner chunk. */
	mem->pcc = 0;
	mem->cfirst = mem->clast = 0;
	mem->cc.cbot = mem->cc.ctop = 0;
    }
    save = gs_alloc_struct((gs_memory_t *) mem, alloc_save_t,
			   &st_alloc_save, "alloc_save_space(save)");
#ifdef DEBUG
    if (inner != 0) {
	if_debug4('u',
		  "[u]save space %u at 0x%lx: cbot=0x%lx ctop=0x%lx\n",
		  mem->space, (ulong) save,
		  (ulong) inner->cbot, (ulong) inner->ctop);
    } else {
	if_debug2('u',
		  "[u]save space %u at 0x%lx (no inner)\n",
		  mem->space, (ulong) save);
    }
#endif
    if (save == 0) {
	gs_free_object(mem->parent, inner, "alloc_save_space(inner)");
	*mem = save_mem;
	return 0;
    }
    save->state = save_mem;
    save->dmem = dmem;
    save->restore_names = (name_memory() == (gs_memory_t *) mem);
    save->is_current = (dmem->current == mem);
    mem->saved = save;
    if_debug2('u', "[u%u]file_save 0x%lx\n",
	      mem->space, (ulong) mem->streams);
    mem->streams = 0;
    return save;
}

/* Record a state change that must be undone for restore, */
/* and mark it as having been saved. */
int
alloc_save_change(gs_dual_memory_t * dmem, const ref * pcont,
		  ref_packed * where, client_name_t cname)
{
    gs_ref_memory_t *mem;
    register alloc_change_t *cp;

    if (dmem->save_level == 0)
	return 0;		/* no saving */
    mem = (pcont == NULL ? dmem->space_local :
	   dmem->spaces.indexed[r_space(pcont) >> r_space_shift]);
    cp = gs_alloc_struct((gs_memory_t *) mem, alloc_change_t,
			 &st_alloc_change, "alloc_save_change");
    if (cp == 0)
	return -1;
    cp->next = mem->changes;
    cp->where = where;
    if (pcont == NULL)
	cp->offset = ac_offset_static;
    else if (r_is_array(pcont) || r_has_type(pcont, t_dictionary))
	cp->offset = ac_offset_ref;
    else if (r_is_struct(pcont))
	cp->offset = (byte *) where - (byte *) pcont->value.pstruct;
    else {
	lprintf3("Bad type %u for save!  pcont = 0x%lx, where = 0x%lx\n",
		 r_type(pcont), (ulong) pcont, (ulong) where);
	gs_abort();
    }
    if (r_is_packed(where))
	*(ref_packed *)(&(cp->contents)) = *where;
    else {
/* MRS - the following inline assign didn't work...
	ref_assign_inline(&cp->contents, (ref *) where);*/
	ref_assign((&(cp->contents)), (ref *) where);
	r_set_attrs((ref *) where, l_new);
    }
    mem->changes = cp;
#ifdef DEBUG
    if (gs_debug_c('U')) {
	dlprintf1("[u]save(%s)", client_name_string(cname));
	alloc_save_print(cp, false);
    }
#endif
    return 0;
}

/* Return the current save level */
int
alloc_save_level(const gs_dual_memory_t * dmem)
{
    return dmem->save_level;
}

/* Return (the id of) the innermost externally visible save object, */
/* i.e., the innermost save with a non-zero ID. */
ulong
alloc_save_current_id(const gs_dual_memory_t * dmem)
{
    const alloc_save_t *save = dmem->space_local->saved;

    while (save != 0 && save->id == 0)
	save = save->state.saved;
    return save->id;
}
alloc_save_t *
alloc_save_current(const gs_dual_memory_t * dmem)
{
    return alloc_find_save(dmem, alloc_save_current_id(dmem));
}

/* Test whether a reference would be invalidated by a restore. */
bool
alloc_is_since_save(const void *vptr, const alloc_save_t * save)
{
#define ptr ((const char *)vptr)

    /* A reference postdates a save iff it is in a chunk allocated */
    /* since the save (including the carried-over inner chunk). */

    const gs_dual_memory_t *dmem = save->dmem;
    register const gs_ref_memory_t *mem = dmem->space_local;

    if_debug2('U', "[U]is_since_save 0x%lx, 0x%lx:\n",
	      (ulong) ptr, (ulong) save);
    if (mem->saved == 0) {	/* This is a special case, the final 'restore' from */
	/* alloc_restore_all. */
	return true;
    }
    /* Check against chunks allocated since the save. */
    /* (There may have been intermediate saves as well.) */
    for (;; mem = &mem->saved->state) {
	const chunk_t *cp;

	if_debug1('U', "[U]checking mem=0x%lx\n", (ulong) mem);
	for (cp = mem->cfirst; cp != 0; cp = cp->cnext) {
	    if (ptr_is_within_chunk(ptr, cp)) {
		if_debug3('U', "[U+]in new chunk 0x%lx: 0x%lx, 0x%lx\n",
			  (ulong) cp,
			  (ulong) cp->cbase, (ulong) cp->cend);
		return true;
	    }
	    if_debug1('U', "[U-]not in 0x%lx\n", (ulong) cp);
	}
	if (mem->saved == save) {	/* We've checked all the more recent saves, */
	    /* must be OK. */
	    break;
	}
    }

    /*
     * If we're about to do a global restore (save level = 1),
     * and there is only one context using this global VM
     * (the normal case, in which global VM is saved by the
     * outermost save), we also have to check the global save.
     * Global saves can't be nested, which makes things easy.
     */
    if (dmem->save_level == 1 &&
	(mem = dmem->space_global) != dmem->space_local &&
	dmem->space_global->num_contexts == 1
	) {
	const chunk_t *cp;

	if_debug1('U', "[U]checking global mem=0x%lx\n", (ulong) mem);
	for (cp = mem->cfirst; cp != 0; cp = cp->cnext)
	    if (ptr_is_within_chunk(ptr, cp)) {
		if_debug3('U', "[U+]  new chunk 0x%lx: 0x%lx, 0x%lx\n",
			  (ulong) cp, (ulong) cp->cbase, (ulong) cp->cend);
		return true;
	    }
    }
    return false;

#undef ptr
}

/* Test whether a name would be invalidated by a restore. */
bool
alloc_name_is_since_save(const ref * pnref, const alloc_save_t * save)
{
    const name *pname;

    if (!save->restore_names)
	return false;
    pname = pnref->value.pname;
    if (pname->foreign_string)
	return false;
    return alloc_is_since_save(pname->string_bytes, save);
}
bool
alloc_name_index_is_since_save(uint nidx, const alloc_save_t * save)
{
    ref nref;

    nref.value.pname = name_index_ptr(nidx);
    return alloc_name_is_since_save(&nref, save);
}

/* Check whether any names have been created since a given save */
/* that might be released by the restore. */
bool
alloc_any_names_since_save(const alloc_save_t * save)
{
    return save->restore_names;
}

/* Get the saved state with a given ID. */
alloc_save_t *
alloc_find_save(const gs_dual_memory_t * dmem, ulong sid)
{
    alloc_save_t *sprev = dmem->space_local->saved;

    if (sid == 0)
	return 0;		/* invalid id */
    while (sprev != 0) {
	if (sprev->id == sid)
	    return sprev;
	sprev = sprev->state.saved;
    }
    return 0;
}

/* Get the client data from a saved state. */
void *
alloc_save_client_data(const alloc_save_t * save)
{
    return save->client_data;
}

/*
 * Do one step of restoring the state.  The client is responsible for
 * calling alloc_find_save to get the save object, and for ensuring that
 * there are no surviving pointers for which alloc_is_since_save is true.
 * Return true if the argument was the innermost save, in which case
 * this is the last (or only) step.
 * Note that "one step" may involve multiple internal steps,
 * if this is the outermost restore (which requires restoring both local
 * and global VM) or if we created extra save levels to reduce scanning.
 */
private void restore_finalize(P1(gs_ref_memory_t *));
private void restore_space(P1(gs_ref_memory_t *));

bool
alloc_restore_state_step(alloc_save_t * save)
{
    gs_dual_memory_t *dmem = save->dmem;
    gs_ref_memory_t *mem = dmem->space_local;
    alloc_save_t *sprev;

    /* Do one (externally visible) step of restoring the state. */
    do {
	ulong sid;

	sprev = mem->saved;
	sid = sprev->id;
	restore_finalize(mem);	/* finalize objects */
	restore_resources(sprev, mem);	/* release other resources */
	restore_space(mem);	/* release memory */
	if (sid != 0) {
	    dmem->save_level--;
	    break;
	}
    }
    while (sprev != save);

    if (dmem->save_level == 0) {	/* This is the outermost save, which might also */
	/* need to restore global VM. */
	mem = dmem->space_global;
	if (mem != dmem->space_local && mem->saved != 0) {
	    restore_finalize(mem);
	    restore_resources(mem->saved, mem);
	    restore_space(mem);
	}
	alloc_set_not_in_save(dmem);
    } else {			/* Set the l_new attribute in all slots that are now new. */
	save_set_new(mem, true);
    }

    return sprev == save;
}
/* Restore the memory of one space, by undoing changes and freeing */
/* memory allocated since the save. */
private void
restore_space(gs_ref_memory_t * mem)
{
    alloc_save_t *save = mem->saved;
    alloc_save_t saved;

    print_save("restore", mem->space, save);

    /* Undo changes since the save. */
    {
	register alloc_change_t *cp = mem->changes;

	while (cp) {
#ifdef DEBUG
	    if (gs_debug_c('U')) {
		dlputs("[U]restore");
		alloc_save_print(cp, true);
	    }
#endif
	    if (r_is_packed(&cp->contents))
		*cp->where = *(ref_packed *) & cp->contents;
	    else
		ref_assign_inline((ref *) cp->where, &cp->contents);
	    cp = cp->next;
	}
    }

    /* Free memory allocated since the save. */
    /* Note that this frees all chunks except the inner one */
    /* belonging to this level. */
    saved = *save;
    restore_free(mem);

    /* Restore the allocator state. */
    {
	int num_contexts = mem->num_contexts;	/* don't restore */

	*mem = saved.state;
	mem->num_contexts = num_contexts;
    }
    alloc_open_chunk(mem);

    /* Make the allocator current if it was current before the save. */
    if (saved.is_current) {
	gs_dual_memory_t *dmem = saved.dmem;

	dmem->current = mem;
	dmem->current_space = mem->space;
    }
}

/* Restore to the initial state, releasing all resources. */
/* The allocator is no longer usable after calling this routine! */
void
alloc_restore_all(gs_dual_memory_t * dmem)
{
    /* Restore to a state outside any saves. */
    while (dmem->save_level != 0)
	discard(alloc_restore_state_step(dmem->space_local->saved));

    /* Finalize memory. */
    restore_finalize(dmem->space_local);
    {
	gs_ref_memory_t *mem = dmem->space_global;

	if (mem != dmem->space_local && mem->num_contexts == 1)
	    restore_finalize(mem);
    }
    restore_finalize(dmem->space_system);

    /* Release resources other than memory, using fake */
    /* save and memory objects. */
    {
	alloc_save_t empty_save;

	empty_save.dmem = dmem;
	empty_save.restore_names = false;	/* don't bother to release */
	restore_resources(&empty_save, NULL);
    }

    /* Finally, release memory. */
    restore_free(dmem->space_local);
    {
	gs_ref_memory_t *mem = dmem->space_global;

	if (mem != dmem->space_local) {
	    if (!--(mem->num_contexts))
		restore_free(mem);
	}
    }
    restore_free(dmem->space_system);

}

/*
 * Finalize objects that will be freed by a restore.
 * Note that we must temporarily disable the freeing operations
 * of the allocator while doing this.
 */
private void
restore_finalize(gs_ref_memory_t * mem)
{
    chunk_t *cp;

    alloc_close_chunk(mem);
    gs_enable_free((gs_memory_t *) mem, false);
    for (cp = mem->clast; cp != 0; cp = cp->cprev) {
	SCAN_CHUNK_OBJECTS(cp)
	    DO_ALL
	    struct_proc_finalize((*finalize)) =
	    pre->o_type->finalize;
	if (finalize != 0) {
	    if_debug2('u', "[u]restore finalizing %s 0x%lx\n",
		      struct_type_name_string(pre->o_type),
		      (ulong) (pre + 1));
	    (*finalize) (pre + 1);
	}
	END_OBJECTS_SCAN
    }
    gs_enable_free((gs_memory_t *) mem, true);
}

/* Release resources for a restore */
private void
restore_resources(alloc_save_t * sprev, gs_ref_memory_t * mem)
{
#ifdef DEBUG
    if (mem) {
	/* Note restoring of the file list. */
	if_debug4('u', "[u%u]file_restore 0x%lx => 0x%lx for 0x%lx\n",
		  mem->space, (ulong)mem->streams,
		  (ulong)sprev->state.streams, (ulong) sprev);
    }
#endif

    /* Remove entries from font and character caches. */
    font_restore(sprev);

    /* Adjust the name table. */
    if (sprev->restore_names)
	names_restore(the_gs_name_table, sprev);
}

/* Release memory for a restore. */
private void
restore_free(gs_ref_memory_t * mem)
{
    /* Free chunks allocated since the save. */
    gs_free_all((gs_memory_t *) mem);
}

/* Forget a save, by merging this level with the next outer one. */
private void file_forget_save(P1(gs_ref_memory_t *));
private void combine_space(P1(gs_ref_memory_t *));
private void forget_changes(P1(gs_ref_memory_t *));
void
alloc_forget_save(alloc_save_t * save)
{
    gs_dual_memory_t *dmem = save->dmem;
    gs_ref_memory_t *mem = dmem->space_local;
    alloc_save_t *sprev;

    print_save("forget_save", mem->space, save);

    /* Iteratively combine the current level with the previous one. */
    do {
	sprev = mem->saved;
	if (sprev->id != 0)
	    dmem->save_level--;
	if (dmem->save_level != 0) {
	    alloc_change_t *chp = mem->changes;

	    save_set_new(&sprev->state, true);
	    /* Concatenate the changes chains. */
	    if (chp == 0)
		mem->changes = sprev->state.changes;
	    else {
		while (chp->next != 0)
		    chp = chp->next;
		chp->next = sprev->state.changes;
	    }
	    file_forget_save(mem);
	    combine_space(mem);	/* combine memory */
	} else {
	    forget_changes(mem);
	    save_set_new(mem, false);
	    file_forget_save(mem);
	    combine_space(mem);	/* combine memory */
	    /* This is the outermost save, which might also */
	    /* need to combine global VM. */
	    mem = dmem->space_global;
	    if (mem != dmem->space_local && mem->saved != 0) {
		forget_changes(mem);
		save_set_new(mem, false);
		file_forget_save(mem);
		combine_space(mem);
	    }
	    alloc_set_not_in_save(dmem);
	    break;		/* must be outermost */
	}
    }
    while (sprev != save);
}
/* Combine the chunks of the next outer level with those of the current one, */
/* and free the bookkeeping structures. */
private void
combine_space(gs_ref_memory_t * mem)
{
    alloc_save_t *saved = mem->saved;
    gs_ref_memory_t *omem = &saved->state;
    chunk_t *cp;
    chunk_t *csucc;

    alloc_close_chunk(mem);
    for (cp = mem->cfirst; cp != 0; cp = csucc) {
	csucc = cp->cnext;	/* save before relinking */
	if (cp->outer == 0)
	    alloc_link_chunk(cp, omem);
	else {
	    chunk_t *outer = cp->outer;

	    outer->inner_count--;
	    if (mem->pcc == cp)
		mem->pcc = outer;
	    if (mem->cfreed.cp == cp)
		mem->cfreed.cp = outer;
	    /* "Free" the header of the inner chunk, */
	    /* and any immediately preceding gap left by */
	    /* the GC having compacted the outer chunk. */
	    {
		obj_header_t *hp = (obj_header_t *) outer->cbot;

		hp->o_large = 0;
		hp->o_size = (char *)(cp->chead + 1)
		    - (char *)(hp + 1);
		hp->o_type = &st_bytes;
		/* The following call is probably not safe. */
#if 0				/* **************** */
		gs_free_object((gs_memory_t *) mem,
			       hp + 1, "combine_space(header)");
#endif /* **************** */
	    }
	    /* Update the outer chunk's allocation pointers. */
	    outer->cbot = cp->cbot;
	    outer->rcur = cp->rcur;
	    outer->rtop = cp->rtop;
	    outer->ctop = cp->ctop;
	    outer->has_refs |= cp->has_refs;
	    gs_free_object(mem->parent, cp,
			   "combine_space(inner)");
	}
    }
    /* Update relevant parts of allocator state. */
    mem->cfirst = omem->cfirst;
    mem->clast = omem->clast;
    mem->allocated += omem->allocated;
    mem->gc_allocated += omem->allocated;
    mem->lost.objects += omem->lost.objects;
    mem->lost.refs += omem->lost.refs;
    mem->lost.strings += omem->lost.strings;
    mem->saved = omem->saved;
    mem->previous_status = omem->previous_status;
    {				/* Concatenate free lists. */
	int i;

	for (i = 0; i < num_freelists; i++) {
	    obj_header_t *olist = omem->freelists[i];
	    obj_header_t *list = mem->freelists[i];

	    if (olist == 0);
	    else if (list == 0)
		mem->freelists[i] = olist;
	    else {
		while (*(obj_header_t **) list != 0)
		    list = *(obj_header_t **) list;
		*(obj_header_t **) list = olist;
	    }
	}
    }
    gs_free_object((gs_memory_t *) mem, saved, "combine_space(saved)");
    alloc_open_chunk(mem);
}
/* Free the changes chain for a level 0 .forgetsave, */
/* resetting the l_new flag in the changed refs. */
private void
forget_changes(gs_ref_memory_t * mem)
{
    register alloc_change_t *chp = mem->changes;
    alloc_change_t *next;

    for (; chp; chp = next) {
	ref_packed *prp = chp->where;

	if_debug1('U', "[U]forgetting change 0x%lx\n", (ulong) chp);
	if (!r_is_packed(prp))
	    r_clear_attrs((ref *) prp, l_new);
	next = chp->next;
	gs_free_object((gs_memory_t *) mem, chp, "forget_changes");
    }
    mem->changes = 0;
}
/* Update the streams list when forgetting a save. */
private void
file_forget_save(gs_ref_memory_t * mem)
{
    const alloc_save_t *save = mem->saved;
    stream *streams = mem->streams;
    stream *saved_streams = save->state.streams;

    if_debug4('u', "[u%d]file_forget_save 0x%lx + 0x%lx for 0x%lx\n",
	      mem->space, (ulong) streams, (ulong) saved_streams,
	      (ulong) save);
    if (streams == 0)
	mem->streams = saved_streams;
    else if (saved_streams != 0) {
	while (streams->next != 0)
	    streams = streams->next;
	streams->next = saved_streams;
	saved_streams->prev = streams;
    }
}

/* ------ Internal routines ------ */

/* Set or reset the l_new attribute in every relevant slot. */
/* This includes every slot on the current change chain, */
/* and every (ref) slot allocated at this save level. */
/* Return the number of bytes of data scanned. */
private long
save_set_new(gs_ref_memory_t * mem, bool to_new)
{
    long scanned = 0;

    /* Handle the change chain. */
    save_set_new_changes(mem, to_new);

    /* Handle newly allocated ref objects. */
    SCAN_MEM_CHUNKS(mem, cp) {
	if (cp->has_refs) {
	    bool has_refs = false;

	    SCAN_CHUNK_OBJECTS(cp)
		DO_ALL
		if_debug3('U', "[U]set_new scan(0x%lx(%lu), %d)\n",
			  (ulong) pre, size, to_new);
	    if (pre->o_type == &st_refs) {	/* These are refs, scan them. */
		ref_packed *prp = (ref_packed *) (pre + 1);
		ref_packed *next = (ref_packed *) ((char *)prp + size);

		if_debug2('U', "[U]refs 0x%lx to 0x%lx\n",
			  (ulong) prp, (ulong) next);
		has_refs = true;
		scanned += size;
		/* We know that every block of refs ends with */
		/* a full-size ref, so we only need the end check */
		/* when we encounter one of those. */
		if (to_new)
		    while (1) {
			if (r_is_packed(prp))
			    prp++;
			else {
			    ((ref *) prp)->tas.type_attrs |= l_new;
			    prp += packed_per_ref;
			    if (prp >= next)
				break;
			}
		} else
		    while (1) {
			if (r_is_packed(prp))
			    prp++;
			else {
			    ((ref *) prp)->tas.type_attrs &= ~l_new;
			    prp += packed_per_ref;
			    if (prp >= next)
				break;
			}
		    }
	    } else
		scanned += sizeof(obj_header_t);
	    END_OBJECTS_SCAN
		cp->has_refs = has_refs;
	}
    }
    END_CHUNKS_SCAN
	if_debug2('u', "[u]set_new (%s) scanned %ld\n",
		  (to_new ? "restore" : "save"), scanned);
    return scanned;
}

/* Set or reset the l_new attribute on the changes chain. */
private void
save_set_new_changes(gs_ref_memory_t * mem, bool to_new)
{
    register alloc_change_t *chp = mem->changes;
    register uint new = (to_new ? l_new : 0);

    for (; chp; chp = chp->next) {
	ref_packed *prp = chp->where;

	if_debug2('U', "[U]set_new(0x%lx, %d)\n",
		  (ulong) prp, new);
	if (!r_is_packed(prp)) {
	    ref *const rp = (ref *) prp;

	    rp->tas.type_attrs =
		(rp->tas.type_attrs & ~l_new) + new;
	}
    }
}
