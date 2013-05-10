/* Copyright (C) 1993, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* igc.c */
/* Garbage collector for Ghostscript */
#include "memory_.h"
#include "ghost.h"
#include "errors.h"
#include "gsexit.h"
#include "gsmdebug.h"
#include "gsstruct.h"
#include "gsutil.h"
#include "iastate.h"
#include "isave.h"
#include "isstate.h"
#include "idict.h"
#include "ipacked.h"
#include "istruct.h"
#include "igc.h"
#include "igcstr.h"
#include "inamedef.h"
#include "opdef.h"			/* for marking oparray names */

/* Import preparation and cleanup routines. */
extern void name_gc_cleanup(P1(gc_state_t *));

/* Define an entry on the mark stack. */
typedef struct { void *ptr; uint index; bool is_refs; } ms_entry;

/* Define (a segment of) the mark stack. */
/* entries[0] has ptr = 0 to indicate the bottom of the stack. */
/* count additional entries follow this structure. */
typedef struct gc_mark_stack_s gc_mark_stack;
struct gc_mark_stack_s {
  gc_mark_stack *prev;
  gc_mark_stack *next;
  uint count;
  bool on_heap;		/* if true, allocated with gs_malloc */
  ms_entry entries[1];
};
/* Define the mark stack sizing parameters. */
#define ms_size_default 100	/* default, allocated on C stack */
/* This should probably be defined as a parameter somewhere.... */
#define ms_size_desired		/* for gs_malloc */\
 ((max_ushort - sizeof(gc_mark_stack)) / sizeof(ms_entry) - 10)
#define ms_size_min 50		/* min size for segment in free block */

/* Forward references */
private void gc_init_mark_stack(P2(gc_mark_stack *, uint));
private void gc_objects_clear_marks(P1(chunk_t *));
private void gc_unmark_names(P0());
private int gc_trace(P3(gs_gc_root_t *, gc_state_t *, gc_mark_stack *));
private int gc_trace_chunk(P3(chunk_t *, gc_state_t *, gc_mark_stack *));
private bool gc_trace_finish(P1(gc_state_t *));
private void gc_clear_reloc(P1(chunk_t *));
private void gc_objects_set_reloc(P1(chunk_t *));
private void gc_do_reloc(P3(chunk_t *, gs_ref_memory_t *, gc_state_t *));
private void gc_objects_compact(P2(chunk_t *, gc_state_t *));
private void gc_free_empty_chunks(P1(gs_ref_memory_t *));

/* Forward references for pointer types */
private ptr_proc_unmark(ptr_struct_unmark);
private ptr_proc_mark(ptr_struct_mark);
private ptr_proc_unmark(ptr_string_unmark);
private ptr_proc_mark(ptr_string_mark);
/*ptr_proc_unmark(ptr_ref_unmark);*/	/* in igc.h */
/*ptr_proc_mark(ptr_ref_mark);*/	/* in igc.h */
/*ptr_proc_reloc(gs_reloc_struct_ptr, void);*/	/* in gsstruct.h */
/*ptr_proc_reloc(gs_reloc_ref_ptr, ref_packed);*/	/* in istruct.h */

/* Pointer type descriptors. */
/* Note that the trace/mark routine has special knowledge of ptr_ref_type */
/* and ptr_struct_type -- it assumes that no other types have embedded */
/* pointers.  Note also that the reloc procedures for string and ref */
/* pointers are never called. */
typedef ptr_proc_reloc((*ptr_proc_reloc_t), void);
const gs_ptr_procs_t ptr_struct_procs =
 { ptr_struct_unmark, ptr_struct_mark, (ptr_proc_reloc_t)gs_reloc_struct_ptr };
const gs_ptr_procs_t ptr_string_procs =
 { ptr_string_unmark, ptr_string_mark, NULL };
const gs_ptr_procs_t ptr_const_string_procs =
 { ptr_string_unmark, ptr_string_mark, NULL };
const gs_ptr_procs_t ptr_ref_procs =
 { ptr_ref_unmark, ptr_ref_mark, NULL };

/* ------ Main program ------ */

/* Top level of garbage collector. */
#ifdef DEBUG
private void
end_phase(const char _ds *str)
{	if ( gs_debug_c('6') )
	  {	dprintf1("[6]---------------- end %s ----------------\n",
			 (const char *)str);
		fflush(dstderr);
	  }
}
#else
#  define end_phase(str) DO_NOTHING
#endif
void
gc_top_level(gs_dual_memory_t *dmem, bool global)
{
#define nspaces 3
	gs_ref_memory_t *spaces[nspaces];
	gs_gc_root_t space_roots[nspaces];
	int ntrace, ncollect, ispace;
	gs_ref_memory_t *mem;
	chunk_t *cp;
	gs_gc_root_t *rp;
	gc_state_t state;
	struct _msd {
	  gc_mark_stack stack;
	  ms_entry body[ms_size_default];
	} ms_default;
	gc_mark_stack *mark_stack = &ms_default.stack;

	/* Determine how many spaces we are collecting. */
	
	spaces[0] = dmem->space_local;
	spaces[1] = dmem->space_system;
	spaces[2] = dmem->space_global;
	if ( dmem->space_global != dmem->space_local )
	  ntrace = 3;
	else
	  ntrace = 2;
	ncollect = (global ? ntrace : 1);

#define for_spaces(i, n)\
  for ( i = 0; i < n; i++ )
#define for_space_mems(i, mem)\
  for ( mem = spaces[i]; mem != 0; mem = &mem->saved->state )
#define for_mem_chunks(mem, cp)\
  for ( cp = (mem)->cfirst; cp != 0; cp = cp->cnext )
#define for_space_chunks(i, mem, cp)\
  for_space_mems(i, mem) for_mem_chunks(mem, cp)
#define for_chunks(n, mem, cp)\
  for_spaces(ispace, n) for_space_chunks(ispace, mem, cp)
#define for_roots(n, mem, rp)\
  for_spaces(ispace, n)\
    for ( mem = spaces[ispace], rp = mem->roots; rp != 0; rp = rp->next )

	/* Initialize the state. */
	state.loc.memory = spaces[0];	/* either one will do */
	state.loc.cp = 0;
	state.space_local = spaces[0];
	state.space_system = spaces[1];
	state.space_global = spaces[2];

	/* Register the allocators themselves as roots, */
	/* so we mark and relocate the change and save lists properly. */

	for_spaces(ispace, ntrace)
	  gs_register_struct_root((gs_memory_t *)spaces[ispace],
				  &space_roots[ispace],
				  (void **)&spaces[ispace],
				  "gc_top_level");

	end_phase("register space roots");

#ifdef DEBUG

	/* Pre-validate the state.  This shouldn't be necessary.... */

	for_spaces(ispace, ntrace)
	  ialloc_validate_memory(spaces[ispace], &state);

	end_phase("pre-validate pointers");

#endif

	/* Clear marks in spaces to be collected; set them, */
	/* and clear relocation, in spaces that are only being traced. */

	for_chunks(ncollect, mem, cp)
	  {	gc_objects_clear_marks(cp);
		gc_strings_set_marks(cp, false);
	  }
	for ( ispace = ncollect; ispace < ntrace; ispace++ )
	  for_space_chunks(ispace, mem, cp)
		gc_clear_reloc(cp);

	end_phase("clear chunk marks");

	/* Clear the marks of roots.  We must do this explicitly, */
	/* since some roots are not in any chunk. */

	for_roots(ntrace, mem, rp)
	  {	void *vptr = *rp->p;
		if_debug_root('6', "[6]unmarking root", rp);
		(*rp->ptype->unmark)(vptr, &state);
	  }

	end_phase("clear root marks");

	gc_unmark_names();

	/* Initialize the (default) mark stack. */

	gc_init_mark_stack(&ms_default.stack, ms_size_default);
	ms_default.stack.prev = 0;
	ms_default.stack.on_heap = false;

	/* Add all large-enough free blocks to the mark stack. */
	/* Also initialize the rescan pointers. */

	{ gc_mark_stack *end = mark_stack;
	  for_chunks(ntrace, mem, cp)
	    {	uint avail = cp->ctop - cp->cbot;
		if ( avail >= sizeof(gc_mark_stack) + sizeof(ms_entry) *
		       ms_size_min &&
		     !cp->inner_count
		   )
		  { gc_mark_stack *pms = (gc_mark_stack *)cp->cbot;
		    gc_init_mark_stack(pms, (avail - sizeof(gc_mark_stack)) /
				       sizeof(ms_entry));
		    end->next = pms;
		    pms->prev = end;
		    pms->on_heap = false;
		    if_debug2('6', "[6]adding free 0x%lx(%u) to mark stack\n",
			      (ulong)pms, pms->count);
		  }
		cp->rescan_bot = cp->cend;
		cp->rescan_top = cp->cbase;
	    }
	}

	/* Mark from roots. */

	{	int more = 0;
		for_roots(ntrace, mem, rp)
		{	if_debug_root('6', "[6]marking root", rp);
			more |= gc_trace(rp, &state, mark_stack);
		}

		end_phase("mark");

		while ( more < 0 )		/* stack overflowed */
		  {	more = 0;
			for_chunks(ntrace, mem, cp)
			  more |= gc_trace_chunk(cp, &state, mark_stack);
		  }

		end_phase("mark overflow");
	}

	/* Free the mark stack. */

	{ gc_mark_stack *pms = mark_stack;
	  while ( pms->next )
	    pms = pms->next;
	  while ( pms )
	    { gc_mark_stack *prev = pms->prev;
	      uint size = sizeof(*pms) + sizeof(ms_entry) * pms->count;
	      if ( pms->on_heap )
		gs_free(pms, 1, size, "gc mark stack");
	      else
		gs_alloc_fill(pms, gs_alloc_fill_free, size);
	      pms = prev;
	    }
	}

	gc_trace_finish(&state);

	end_phase("finish trace");

	/* Set the relocation of roots outside any chunk to o_untraced, */
	/* so we won't try to relocate pointers to them. */
	/* (Currently, there aren't any.) */

	/* Disable freeing in the allocators of the spaces we are */
	/* collecting, so finalization procedures won't cause problems. */
	{ int i;
	  for_spaces(i, ncollect)
	    gs_enable_free((gs_memory_t *)spaces[i], false);
	}

	/* Compute relocation based on marks, in the spaces */
	/* we are going to compact.  Also finalize freed objects. */

	for_chunks(ncollect, mem, cp)
	{	gc_objects_set_reloc(cp);
		gc_strings_set_reloc(cp);
	}

	/* Re-enable freeing. */
	{ int i;
	  for_spaces(i, ncollect)
	    gs_enable_free((gs_memory_t *)spaces[i], true);
	}

	end_phase("set reloc");

	/* Remove unmarked names, and relocate name string pointers. */

	name_gc_cleanup(&state);

	end_phase("clean up names");

	/* Relocate pointers. */

	for_chunks(ntrace, mem, cp)
	  gc_do_reloc(cp, mem, &state);

	end_phase("relocate chunks");

	for_roots(ntrace, mem, rp)
	{	if_debug3('6', "[6]relocating root 0x%lx: 0x%lx -> 0x%lx\n",
			  (ulong)rp, (ulong)rp->p, (ulong)*rp->p);
		if ( rp->ptype == ptr_ref_type )
		{	ref *pref = (ref *)*rp->p;
			gs_reloc_refs((ref_packed *)pref,
				      (ref_packed *)(pref + 1),
				      &state);
		}
		else
			*rp->p = (*rp->ptype->reloc)(*rp->p, &state);
	}

	end_phase("relocate roots");

	/* Compact data.  We only do this for spaces we are collecting. */

	for_spaces(ispace, ncollect)
	  { for_space_mems(ispace, mem)
	      { for_mem_chunks(mem, cp)
		  { if_debug_chunk('6', "[6]compacting chunk", cp);
		    gc_objects_compact(cp, &state);
		    gc_strings_compact(cp);
		    if_debug_chunk('6', "[6]after compaction:", cp);
		    if ( mem->pcc == cp )
		      mem->cc = *cp;
		  }
		mem->saved = mem->reloc_saved;
		ialloc_reset_free(mem);
	      }
	  }

	end_phase("compact");

	/* Free empty chunks. */

	for_spaces(ispace, ncollect)
	  for_space_mems(ispace, mem)
	    gc_free_empty_chunks(mem);

	end_phase("free empty chunks");

	/*
	 * Update previous_status to reflect any freed chunks,
	 * and set inherited to the negative of allocated,
	 * so it has no effect.  We must update previous_status by
	 * working back-to-front along the save chain, using pointer reversal.
	 * (We could update inherited in any order, since it only uses
	 * information local to the individual save level.)
	 */

	for_spaces(ispace, ncollect)
	  {	/* Reverse the pointers. */
		alloc_save_t *curr;
		alloc_save_t *prev = 0;
		alloc_save_t *next;
		gs_memory_status_t total;
		for ( curr = spaces[ispace]->saved; curr != 0;
		      prev = curr, curr = next
		    )
		  { next = curr->state.saved;
		    curr->state.saved = prev;
		  }
		/* Now work the other way, accumulating the values. */
		total.allocated = 0, total.used = 0;
		for ( curr = prev, prev = 0; curr != 0;
		      prev = curr, curr = next
		    )
		  { mem = &curr->state;
		    next = mem->saved;
		    mem->saved = prev;
		    mem->previous_status = total;
		    if_debug3('6',
			      "[6]0x%lx previous allocated=%lu, used=%lu\n",
			      (ulong)mem, total.allocated, total.used);
		    gs_memory_status((gs_memory_t *)mem, &total);
		    mem->gc_allocated = mem->allocated + total.allocated;
		    mem->inherited = -mem->allocated;
		  }
		mem = spaces[ispace];
		mem->previous_status = total;
		mem->gc_allocated = mem->allocated + total.allocated;
		if_debug3('6', "[6]0x%lx previous allocated=%lu, used=%lu\n",
			  (ulong)mem, total.allocated, total.used);
	  }

	end_phase("update stats");

	/* Clear marks in spaces we didn't compact. */

	for ( ispace = ncollect; ispace < ntrace; ispace++ )
	  for_space_chunks(ispace, mem, cp)
	    gc_objects_clear_marks(cp);

	end_phase("post-clear marks");

	/* Unregister the allocator roots. */

	for_spaces(ispace, ntrace)
	  gs_unregister_root((gs_memory_t *)spaces[ispace],
			     &space_roots[ispace], "gc_top_level");

	end_phase("unregister space roots");

#ifdef DEBUG

	/* Validate the state.  This shouldn't be necessary.... */

	for_spaces(ispace, ntrace)
	  ialloc_validate_memory(spaces[ispace], &state);

	end_phase("validate pointers");

#endif
}

/* ------ Debugging utilities ------ */

/* Validate a pointer to an object header. */
#ifdef DEBUG
#  define debug_check_object(pre, cp, gcst)\
     ialloc_validate_object((pre) + 1, cp, gcst)
#else
#  define debug_check_object(pre, cp, gcst) DO_NOTHING
#endif

/* ------ Unmarking phase ------ */

/* Unmark a single struct. */
private void
ptr_struct_unmark(void *vptr, gc_state_t *ignored)
{	if ( vptr != 0 )
	  o_set_unmarked(((obj_header_t *)vptr - 1));
}

/* Unmark a single string. */
private void
ptr_string_unmark(void *vptr, gc_state_t *gcst)
{	discard(gc_string_mark(((gs_string *)vptr)->data,
			       ((gs_string *)vptr)->size,
			       false, gcst));
}

/* Unmark the objects in a chunk. */
private void
gc_objects_clear_marks(chunk_t *cp)
{	if_debug_chunk('6', "[6]unmarking chunk", cp);
	SCAN_CHUNK_OBJECTS(cp)
	  DO_ALL
		struct_proc_clear_marks((*proc)) =
			pre->o_type->clear_marks;
		debug_check_object(pre, cp, NULL);
		if_debug3('7', " [7](un)marking %s(%lu) 0x%lx\n",
			  struct_type_name_string(pre->o_type),
			  (ulong)size, (ulong)pre);
		o_set_unmarked(pre);
		if ( proc != 0 )
			(*proc)(pre + 1, size);
	END_OBJECTS_SCAN
}

/* Mark 0- and 1-character names, and those referenced from the */
/* op_array_nx_table, and unmark all the rest. */
private void
gc_unmark_names()
{	register uint i;
	name_unmark_all();
	for ( i = 0; i < op_array_table_global.count; i++ )
	{	uint nidx = op_array_table_global.nx_table[i];
		name_index_ptr(nidx)->mark = 1;
	}
	for ( i = 0; i < op_array_table_local.count; i++ )
	{	uint nidx = op_array_table_local.nx_table[i];
		name_index_ptr(nidx)->mark = 1;
	}
}

/* ------ Marking phase ------ */

/* Initialize (a segment of) the mark stack. */
private void
gc_init_mark_stack(gc_mark_stack *pms, uint count)
{	pms->next = 0;
	pms->count = count;
	pms->entries[0].ptr = 0;
	pms->entries[0].index = 0;
	pms->entries[0].is_refs = false;
}

/* Mark starting from all marked objects in the interval of a chunk */
/* needing rescanning. */
private int
gc_trace_chunk(chunk_t *cp, gc_state_t *pstate, gc_mark_stack *pmstack)
{	byte *sbot = cp->rescan_bot;
	byte *stop = cp->rescan_top;
	gs_gc_root_t root;
	void *comp;
	int more = 0;

	if ( sbot > stop )
	  return 0;
	root.p = &comp;
	if_debug_chunk('6', "[6]marking from chunk", cp);
	cp->rescan_bot = cp->cend;
	cp->rescan_top = cp->cbase;
	SCAN_CHUNK_OBJECTS(cp)
	  DO_ALL
	    if ( (byte *)(pre + 1) + size < sbot )
	      ;
	    else if ( (byte *)(pre + 1) > stop )
	      return more;		/* 'break' won't work here */
	    else
	      {	if_debug2('7', " [7]scanning/marking 0x%lx(%lu)\n",
			  (ulong)pre, (ulong)size);
		if ( pre->o_type == &st_refs )
		  {	ref_packed *rp = (ref_packed *)(pre + 1);
			char *end = (char *)rp + size;
			root.ptype = ptr_ref_type;
			while ( (char *)rp < end )
			{	comp = rp;
				if ( r_is_packed(rp) )
				  { if ( r_has_pmark(rp) )
				      { r_clear_pmark(rp);
					more |= gc_trace(&root, pstate,
							 pmstack);
				      }
				    rp++;
				  }
				else
				  { if ( r_has_attr((ref *)rp, l_mark) )
				      { r_clear_attrs((ref *)rp, l_mark);
					more |= gc_trace(&root, pstate,
							 pmstack);
				      }
				    rp += packed_per_ref;
				  }
			}
		  }
		else if ( !o_is_unmarked(pre) )
		  {	struct_proc_clear_marks((*proc)) =
			  pre->o_type->clear_marks;
			root.ptype = ptr_struct_type;
			comp = pre + 1;
			if ( !o_is_untraced(pre) )
			  o_set_unmarked(pre);
			if ( proc != 0 )
			  (*proc)(comp, size);
			more |= gc_trace(&root, pstate, pmstack);
		  }
	  }
	END_OBJECTS_SCAN
	return more;
}

/* Recursively mark from a (root) pointer. */
/* Return -1 if we overflowed the mark stack, */
/* 0 if we completed successfully without marking any new objects, */
/* 1 if we completed and marked some new objects. */
private int
gc_trace(gs_gc_root_t *rp, gc_state_t *pstate, gc_mark_stack *pmstack)
{	gc_mark_stack *pms = pmstack;
	ms_entry *sp = pms->entries + 1;
	/* We stop the mark stack 1 entry early, because we store into */
	/* the entry beyond the top. */
	ms_entry *stop = sp + pms->count - 2;
#ifdef DEBUG
	ulong prev_depth = 0;
#endif
	int new = 0;
	void *nptr = *rp->p;
#define mark_name(nidx, pname)\
  { if ( !pname->mark )\
     {  pname->mark = 1;\
	new |= 1;\
	if_debug2('8', "  [8]marked name 0x%lx(%u)\n", (ulong)pname, nidx);\
     }\
  }

	if ( nptr == 0 )
	  return 0;

	/* Initialize the stack */
	sp->ptr = nptr;
	if ( rp->ptype == ptr_ref_type )
		sp->index = 1, sp->is_refs = true;
	else
	{	sp->index = 0, sp->is_refs = false;
		if ( (*rp->ptype->mark)(nptr, pstate) )
		  new |= 1;
	}
	for ( ; ; )
	{	gs_ptr_type_t ptp;
#ifdef DEBUG
		static const char *dots = "..........";
		ulong dot_depth;
#define depth_dots\
  ((dot_depth = sp - pms->entries - 1 + prev_depth) >= 10 ? dots :\
   dots + 10 - dot_depth)
#endif
		if ( !sp->is_refs )	/* struct */
		{	obj_header_t *ptr = sp->ptr;
			ulong osize;
			struct_proc_enum_ptrs((*mproc));

			if ( ptr == 0 )
			  { /* We've reached the bottom of a stack segment. */
			    pms = pms->prev;
			    if ( pms == 0 )
			      break;		/* all done */
#ifdef DEBUG
			    prev_depth -= pms->count - 1;
#endif
			    stop = pms->entries + pms->count - 1;
			    sp = stop;
			    continue;
			  }
			debug_check_object(ptr - 1, NULL, NULL);
			osize = pre_obj_contents_size(ptr - 1);
			if_debug4('7', " [7]%smarking %s 0x%lx[%u]",
				  depth_dots,
				  struct_type_name_string(ptr[-1].o_type),
				  (ulong)ptr, sp->index);
			mproc = ptr[-1].o_type->enum_ptrs;
			ptp = (mproc == 0 ? (gs_ptr_type_t)0 :
				(*mproc)(ptr, osize, sp->index++, &nptr));
			if ( ptp == NULL )	/* done with structure */
			{	if_debug0('7', " - done\n");
				sp--;
				continue;
			}
			if_debug1('7', " = 0x%lx\n", (ulong)nptr);
			/* Descend into nptr, whose pointer type is ptp. */
			if ( ptp == ptr_ref_type )
			  { sp[1].index = 1;
			    sp[1].is_refs = true;
			  }
			else if ( ptp != ptr_struct_type )
			  { /* We assume this is some non-pointer- */
			    /* containing type. */
			    if ( (*ptp->mark)(nptr, pstate) )
			      new |= 1;
			    continue;
			  }
			else
			  { sp[1].index = 0;
			    sp[1].is_refs = false;
			  }
		}
		else			/* refs */
		{	ref_packed *pptr = sp->ptr;

			if ( !sp->index )
			  { --sp;
			    continue;
			  }
			--(sp->index);
			if_debug3('8', "  [8]%smarking refs 0x%lx[%u]\n",
				  depth_dots, (ulong)pptr, sp->index);
#define rptr ((ref *)pptr)
			if ( r_is_packed(rptr) )
			{	sp->ptr = pptr + 1;
				if ( r_has_pmark(pptr) )
				  continue;
				r_set_pmark(pptr);
				new |= 1;
				if ( r_packed_is_name(pptr) )
				{	uint nidx = packed_name_index(pptr);
					name *pname = name_index_ptr(nidx);
					mark_name(nidx, pname);
				}
				continue;
			}
			sp->ptr = rptr + 1;
			if ( r_has_attr(rptr, l_mark) )
			  continue;
			r_set_attrs(rptr, l_mark);
			new |= 1;
			switch ( r_type(rptr) )
			   {
			/* Struct cases */
			case t_file:
				nptr = rptr->value.pfile;
rs:				if ( r_is_foreign(rptr) )
				  continue;
				sp[1].is_refs = false;
				sp[1].index = 0;
				ptp = ptr_struct_type;
				break;
			case t_device:
				nptr = rptr->value.pdevice; goto rs;
			case t_fontID:
			case t_struct:
			case t_astruct:
				nptr = rptr->value.pstruct; goto rs;
			/* Non-trivial non-struct cases */
			case t_dictionary:
				nptr = rptr->value.pdict;
				sp[1].index = sizeof(dict) / sizeof(ref);
				goto rrp;
			case t_array:
				nptr = rptr->value.refs;
rr:				if ( (sp[1].index = r_size(rptr)) == 0 )
				{	/* Set the base pointer to 0, */
					/* so we never try to relocate it. */
					rptr->value.refs = 0;
					continue;
				}
rrp:				if ( r_is_foreign(rptr) )
					continue;
rrc:				sp[1].is_refs = true;
				break;
			case t_mixedarray: case t_shortarray:
				nptr = (void *)rptr->value.packed; /* discard const */
				goto rr;
			case t_name:
				mark_name(name_index(rptr), rptr->value.pname);
				continue;
			case t_string:
				if ( r_is_foreign(rptr) )
				  continue;
				if ( gc_string_mark(rptr->value.bytes, r_size(rptr), true, pstate) )
				  new |= 1;
				continue;
			case t_oparray:
				nptr = (void *)rptr->value.const_refs;	/* discard const */
				sp[1].index = 1;
				goto rrc;
			default:		/* includes packed refs */
				continue;
			   }
#undef rptr
		}
		if ( sp == stop )
		  {	/* The current segment is full. */
			if ( pms->next == 0 )
			  { /* Try to allocate another segment. */
			    uint count;
			    for ( count = ms_size_desired;
				  count >= ms_size_min;
				  count >>= 1
				)
			      { pms->next =
				  gs_malloc(1, sizeof(gc_mark_stack) +
					    sizeof(ms_entry) * count,
					    "gc mark stack");
				if ( pms->next != 0 )
				  break;
			      }
			    if ( pms->next == 0 )
			      { /* The mark stack overflowed. */
				byte *cptr = sp->ptr;	/* container */
				chunk_t *cp = gc_locate(cptr, pstate);

				if ( cp == 0 )
				  { /* We were tracing outside collectible */
				    /* storage.  This can't happen. */
				    lprintf1("mark stack overflowed while outside collectible space at 0x%lx!\n",
					     (ulong)cptr);
				    gs_abort();
				  }
				if ( cptr < cp->rescan_bot )
				  cp->rescan_bot = cptr, new = -1;
				if ( cptr > cp->rescan_top )
				  cp->rescan_top = cptr, new = -1;
				new |= 1;
				continue;
			      }
			    gc_init_mark_stack(pms->next, count);
			    pms->next->prev = pms;
			    pms->next->on_heap = true;
			  }
#ifdef DEBUG
			prev_depth += pms->count - 1;
#endif
			pms = pms->next;
			stop = pms->entries + pms->count - 1;
			pms->entries[1] = sp[1];
			sp = pms->entries;
		  }
		/* index and is_refs are already set */
		if ( !sp[1].is_refs )
		  { if ( !(*ptp->mark)(nptr, pstate) )
		      continue;
		    new |= 1;
		  }
		(++sp)->ptr = nptr;
	}
	return new;
}

/* Mark a struct.  Return true if new mark. */
private bool
ptr_struct_mark(void *vptr, gc_state_t *ignored)
{	obj_header_t *ptr = vptr;
	if ( vptr == 0 )
		return false;
	ptr--;			/* point to header */
	if ( !o_is_unmarked(ptr) )
		return false;
	o_mark(ptr);
	return true;
}

/* Mark a string.  Return true if new mark. */
private bool
ptr_string_mark(void *vptr, gc_state_t *gcst)
{	return gc_string_mark(((gs_string *)vptr)->data,
			      ((gs_string *)vptr)->size,
			      true, gcst);
}

/* Finish tracing by marking names. */
private bool
gc_trace_finish(gc_state_t *pstate)
{	uint nidx = 0;
	bool marked = false;
	while ( (nidx = name_next_valid_index(nidx)) != 0 )
	{	name *pname = name_index_ptr(nidx);
		if ( pname->mark )
		{	if ( !pname->foreign_string && gc_string_mark(pname->string_bytes, pname->string_size, true, pstate) )
			  marked = true;
			marked |= ptr_struct_mark(name_index_ptr_sub_table(nidx, pname), pstate);
		}
	}
	return marked;
}

/* ------ Relocation planning phase ------ */

/* Initialize the relocation information in the chunk header. */
private void
gc_init_reloc(chunk_t *cp)
{	chunk_head_t *chead = cp->chead;
	chead->dest = cp->cbase;
	chead->free.o_back =
	  offset_of(chunk_head_t, free) >> obj_back_shift;
	chead->free.o_size = sizeof(obj_header_t);
	chead->free.o_nreloc = 0;
}

/* Set marks and clear relocation for chunks that won't be compacted. */
private void
gc_clear_reloc(chunk_t *cp)
{	gc_init_reloc(cp);
	SCAN_CHUNK_OBJECTS(cp)
	  DO_ALL
		const struct_shared_procs_t _ds *procs =
			pre->o_type->shared;
		if ( procs != 0 )
		  (*procs->clear_reloc)(pre, size);
		o_set_untraced(pre);
	END_OBJECTS_SCAN
	gc_strings_set_marks(cp, true);
	gc_strings_clear_reloc(cp);
}

/* Set the relocation for the objects in a chunk. */
/* This will never be called for a chunk with any o_untraced objects. */
private void
gc_objects_set_reloc(chunk_t *cp)
{	uint reloc = 0;
	chunk_head_t *chead = cp->chead;
	byte *pfree = (byte *)&chead->free;	/* most recent free object */
	if_debug_chunk('6', "[6]setting reloc for chunk", cp);
	gc_init_reloc(cp);
	SCAN_CHUNK_OBJECTS(cp)
	  DO_ALL
		struct_proc_finalize((*finalize));
		const struct_shared_procs_t _ds *procs =
		  pre->o_type->shared;
		if ( (procs == 0 ? o_is_unmarked(pre) :
		      !(*procs->set_reloc)(pre, reloc, size))
		   )
		  {	/* Free object */
			reloc += sizeof(obj_header_t) + obj_align_round(size);
			if ( (finalize = pre->o_type->finalize) != 0 )
			  {	if_debug2('u', "[u]GC finalizing %s 0x%lx\n",
					  struct_type_name_string(pre->o_type),
					  (ulong)(pre + 1));
				(*finalize)(pre + 1);
			  }
			if ( pre->o_large )
			  {	/* We should chop this up into small */
				/* free blocks, but there's no value */
				/* in doing this right now. */
				o_set_unmarked_large(pre);
			  }
			else
			  {	pfree = (byte *)pre;
				pre->o_back =
				  (pfree - (byte *)chead) >> obj_back_shift;
				pre->o_nreloc = reloc;
			  }
			if_debug3('7', " [7]at 0x%lx, unmarked %lu, new reloc = %u\n",
				  (ulong)pre, (ulong)size, reloc);
		  }
		else
		  {	/* Useful object */
			debug_check_object(pre, cp, NULL);
			if ( pre->o_large )
			  {	if ( o_is_unmarked_large(pre) )
				  o_mark_large(pre);
			  }
			else
			  pre->o_back =
			    ((byte *)pre - pfree) >> obj_back_shift;
		  }
	END_OBJECTS_SCAN
#ifdef DEBUG
	if ( reloc != 0 )
	{ if_debug1('6', "[6]freed %u", reloc);
	  if_debug_chunk('6', " in", cp);
	}
#endif
}

/* ------ Relocation phase ------ */

/* Relocate the pointers in all the objects in a chunk. */
private void
gc_do_reloc(chunk_t *cp, gs_ref_memory_t *mem, gc_state_t *pstate)
{	chunk_head_t *chead = cp->chead;
	if_debug_chunk('6', "[6]relocating in chunk", cp);
	SCAN_CHUNK_OBJECTS(cp)
	  DO_ALL
		/* We need to relocate the pointers in an object iff */
		/* it is o_untraced, or it is a useful object. */
		/* An object is free iff its back pointer points to */
		/* the chunk_head structure. */
		if ( o_is_untraced(pre) ||
		     (pre->o_large ? !o_is_unmarked(pre) :
		      pre->o_back << obj_back_shift !=
		        (byte *)pre - (byte *)chead)
		   )
		  {	struct_proc_reloc_ptrs((*proc)) =
				pre->o_type->reloc_ptrs;
			if_debug3('7',
				  " [7]relocating ptrs in %s(%lu) 0x%lx\n",
				  struct_type_name_string(pre->o_type),
				  (ulong)size, (ulong)pre);
			if ( proc != 0 )
				(*proc)(pre + 1, size, pstate);
		  }
	END_OBJECTS_SCAN
}

/* Print pointer relocation if debugging. */
/* We have to provide this procedure even if DEBUG is not defined, */
/* in case one of the other GC modules was compiled with DEBUG. */
void *
print_reloc_proc(const void *obj, const char *cname, void *robj)
{	if_debug3('9', "  [9]relocate %s * 0x%lx to 0x%lx\n",
		  cname, (ulong)obj, (ulong)robj);
	return robj;
}

/* Relocate a pointer to an (aligned) object. */
/* See gsmemory.h for why the argument is const and the result is not. */
void /*obj_header_t*/ *
gs_reloc_struct_ptr(const void /*obj_header_t*/ *obj, gc_state_t *gcst)
{	const void *robj;

	if ( obj == 0 )
	  return print_reloc(obj, "NULL", 0);
#define optr ((const obj_header_t *)obj)
	debug_check_object(optr - 1, NULL, gcst);
	/* The following should be a conditional expression, */
	/* but Sun's cc compiler can't handle it. */
	if ( optr[-1].o_large )
	  robj = obj;
	else
	  {	uint back = optr[-1].o_back;
		if ( back == o_untraced )
		  robj = obj;
		else
		  {
#ifdef DEBUG
			/* Do some sanity checking. */
			if ( back > gcst->space_local->chunk_size >> obj_back_shift )
			  {	lprintf2("Invalid back pointer %u at 0x%lx!\n",
					 back, (ulong)obj);
				gs_abort();
			  }
#endif
		    {	const obj_header_t *pfree = (const obj_header_t *)
			  ((const char *)(optr - 1) -
			    (back << obj_back_shift));
			const chunk_head_t *chead = (const chunk_head_t *)
			  ((const char *)pfree -
			    (pfree->o_back << obj_back_shift));
			robj = chead->dest +
			  ((const char *)obj - (const char *)(chead + 1) -
			    pfree->o_nreloc);
		    }
		  }
	  }
	return print_reloc(obj,
			   struct_type_name_string(optr[-1].o_type),
			   (void *)robj);	/* discard const */
#undef optr
}

/* ------ Compaction phase ------ */

/* Compact the objects in a chunk. */
/* This will never be called for a chunk with any o_untraced objects. */
private void
gc_objects_compact(chunk_t *cp, gc_state_t *gcst)
{	chunk_head_t *chead = cp->chead;
	obj_header_t *dpre = (obj_header_t *)chead->dest;
	SCAN_CHUNK_OBJECTS(cp)
	  DO_ALL
		/* An object is free iff its back pointer points to */
		/* the chunk_head structure. */
		if ( (pre->o_large ? !o_is_unmarked(pre) :
		      pre->o_back << obj_back_shift !=
		        (byte *)pre - (byte *)chead)
		   )
		  {	const struct_shared_procs_t _ds *procs =
			  pre->o_type->shared;
			debug_check_object(pre, cp, gcst);
			if_debug4('7',
				  " [7]compacting %s 0x%lx(%lu) to 0x%lx\n",
				  struct_type_name_string(pre->o_type),
				  (ulong)pre, (ulong)size, (ulong)dpre);
			if ( procs == 0 )
			  { if ( dpre != pre )
			      memmove(dpre, pre,
				      sizeof(obj_header_t) + size);
			  }
			else
			  (*procs->compact)(pre, dpre, size);
			dpre = (obj_header_t *)
			  ((byte *)dpre + obj_size_round(size));
		  }
	END_OBJECTS_SCAN
	if ( cp->outer == 0 && chead->dest != cp->cbase )
	  dpre = (obj_header_t *)cp->cbase; /* compacted this chunk into another */
	gs_alloc_fill(dpre, gs_alloc_fill_collected, cp->cbot - (byte *)dpre);
	cp->cbot = (byte *)dpre;
	cp->rcur = 0;
	cp->rtop = 0;		/* just to be sure */
}

/* ------ Cleanup ------ */

/* Free empty chunks. */
private void
gc_free_empty_chunks(gs_ref_memory_t *mem)
{	chunk_t *cp;
	chunk_t *csucc;
	/* Free the chunks in reverse order, */
	/* to encourage LIFO behavior. */
	for ( cp = mem->clast; cp != 0; cp = csucc )
	{	/* Make sure this isn't an inner chunk, */
		/* or a chunk that has inner chunks. */
		csucc = cp->cprev; 	/* save before freeing */
		if ( cp->cbot == cp->cbase && cp->ctop == cp->climit &&
		     cp->outer == 0 && cp->inner_count == 0
		   )
		  {	alloc_free_chunk(cp, mem);
			if ( mem->pcc == cp )
			  mem->pcc = 0;
		  }
	}
}
