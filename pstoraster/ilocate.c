/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* ilocate.c */
/* Object locating and validating for Ghostscript memory manager */
#include "ghost.h"
#include "memory_.h"
#include "errors.h"
#include "gsexit.h"
#include "gsstruct.h"
#include "iastate.h"
#include "igc.h"			/* for gc_state_t */
#include "igcstr.h"			/* for prototype */
#include "iname.h"
#include "ipacked.h"
#include "isstate.h"
#include "ivmspace.h"
#include "store.h"

/* ================ Locating ================ */

/* Locate a pointer in the chunks of a space being collected. */
/* This is only used for string garbage collection and for debugging. */
chunk_t *
gc_locate(const void *ptr, gc_state_t *gcst)
{	const gs_ref_memory_t *mem;
	if ( chunk_locate(ptr, &gcst->loc) )
	  return gcst->loc.cp;
	mem = gcst->loc.memory;
	/* Try the other space, if there is one. */
	if ( gcst->space_local != gcst->space_global )
	  { gcst->loc.memory =
	      (mem->space == avm_local ? gcst->space_global : gcst->space_local);
	    gcst->loc.cp = 0;
	    if ( chunk_locate(ptr, &gcst->loc) )
	      return gcst->loc.cp;
	    /* Try other save levels of this space. */
	    while ( gcst->loc.memory->saved != 0 )
	      { gcst->loc.memory = &gcst->loc.memory->saved->state;
		gcst->loc.cp = 0;
		if ( chunk_locate(ptr, &gcst->loc) )
		  return gcst->loc.cp;
	      }
	  }
	/* Try the system space.  This is simpler because it isn't */
	/* subject to save/restore. */
	if ( mem != gcst->space_system )
	{	gcst->loc.memory = gcst->space_system;
		gcst->loc.cp = 0;
		if ( chunk_locate(ptr, &gcst->loc) )
		  return gcst->loc.cp;
	}
	/* Try other save levels of the initial space, */
	/* or of global space if the original space was system space. */
	/* In the latter case, try all levels. */
	gcst->loc.memory =
	  (mem == gcst->space_system || mem->space == avm_global ?
	   gcst->space_global : gcst->space_local);
	for ( ; ; )
	  { if ( gcst->loc.memory != mem )	/* don't do twice */
	      { gcst->loc.cp = 0;
		if ( chunk_locate(ptr, &gcst->loc) )
		  return gcst->loc.cp;
	      }
	    if ( gcst->loc.memory->saved == 0 )
	      break;
	    gcst->loc.memory = &gcst->loc.memory->saved->state;
	  }
	/* Restore locator to a legal state. */
	gcst->loc.memory = mem;
	gcst->loc.cp = 0;
	return 0;
}

/* ================ Debugging ================ */

#ifdef DEBUG

/* Validate the contents of an allocator. */
void
ialloc_validate_spaces(const gs_dual_memory_t *dmem)
{	int i;
	gc_state_t state;
#define nspaces countof(dmem->spaces.indexed)
	chunk_t cc[nspaces];
	uint rsize[nspaces];
	ref rlast[nspaces];

	state.spaces = dmem->spaces;
	state.loc.memory = state.spaces.named.local;
	state.loc.cp = 0;

	/* Save everything we need to reset temporarily. */
	for ( i = 0; i < nspaces; i++ )
	  if ( dmem->spaces.indexed[i] != 0 )
	    {	gs_ref_memory_t *mem = dmem->spaces.indexed[i];
		chunk_t *pcc = mem->pcc;
		obj_header_t *rcur = mem->cc.rcur;
		if ( pcc != 0 )
		  {	cc[i] = *pcc;
			*pcc = mem->cc;
		  }
		if ( rcur != 0 )
		  {	rsize[i] = rcur[-1].o_size;
			rcur[-1].o_size = mem->cc.rtop - (byte *)rcur;
			/* Create the final ref, reserved for the GC. */
			rlast[i] = ((ref *)mem->cc.rtop)[-1];
			make_mark((ref *)mem->cc.rtop - 1);
		  }
	    }

	/* Validate memory. */
	for ( i = 0; i < nspaces; i++ )
	  if ( dmem->spaces.indexed[i] != 0 )
	    ialloc_validate_memory(dmem->spaces.indexed[i], &state);

	/* Undo temporary changes. */
	for ( i = 0; i < nspaces; i++ )
	  if ( dmem->spaces.indexed[i] != 0 )
	    {	gs_ref_memory_t *mem = dmem->spaces.indexed[i];
		chunk_t *pcc = mem->pcc;
		obj_header_t *rcur = mem->cc.rcur;
		if ( rcur != 0 )
		  {	rcur[-1].o_size = rsize[i];
			((ref *)mem->cc.rtop)[-1] = rlast[i];
		  }
		if ( pcc != 0 )
		  *pcc = cc[i];
	    }
}
void
ialloc_validate_memory(const gs_ref_memory_t *mem, gc_state_t *gcst)
{	const gs_ref_memory_t *smem;
	for ( smem = mem; smem != 0; smem = &smem->saved->state )
	  {	const chunk_t *cp;
		for ( cp = mem->cfirst; cp != 0; cp = cp->cnext )
		  ialloc_validate_chunk(cp, gcst);
	  };
}

/* Validate all the objects in a chunk. */
void
ialloc_validate_chunk(const chunk_t *cp, gc_state_t *gcst)
{	if_debug_chunk('6', "[6]validating chunk", cp);
	SCAN_CHUNK_OBJECTS(cp);
	  DO_ALL
		ialloc_validate_object(pre + 1, cp, gcst);
		if_debug3('7', " [7]validating %s(%lu) 0x%lx\n",
			  struct_type_name_string(pre->o_type),
			  (ulong)size, (ulong)pre);
		if ( pre->o_type == &st_refs )
		  {	const ref_packed *rp = (const ref_packed *)(pre + 1);
			const char *end = (const char *)rp + size;

			while ( (const char *)rp < end )
			  if ( r_is_packed(rp) )
			    rp++;
			  else
			  { const void *optr = 0;
#define pref ((const ref *)rp)
			    if ( r_space(pref) != avm_foreign )
			      switch ( r_type(pref) )
			    {
			    case t_file:
				optr = pref->value.pfile;
				goto cks;
			    case t_device:
				optr = pref->value.pdevice;
				goto cks;
			    case t_fontID:
			    case t_struct:
			    case t_astruct:
				optr = pref->value.pstruct;
cks:				if ( optr != 0 )
				  ialloc_validate_object(optr, NULL, gcst);
				break;
			    case t_name:
				if ( name_index_ptr(r_size(pref)) !=
				     pref->value.pname
				   )
				  {	lprintf3("At 0x%lx, bad name %u, pname = 0x%lx\n",
						 (ulong)pref,
						 (uint)r_size(pref),
						 (ulong)pref->value.pname);
					break;
				  }
				  {	ref sref;
					name_string_ref(pref, &sref);
					if ( r_space(&sref) != avm_foreign &&
					     !gc_locate(sref.value.const_bytes, gcst)
					   )
					  {	lprintf4("At 0x%lx, bad name %u, pname = 0x%lx, string 0x%lx not in any chunk\n",
							 (ulong)pref,
							 (uint)r_size(pref),
							 (ulong)pref->value.pname,
							 (ulong)sref.value.const_bytes);
						break;
					  }
				  }
				break;
			    case t_string:
				if ( r_size(pref) != 0 &&
				     !gc_locate(pref->value.bytes, gcst)
				   )
				  {	lprintf3("At 0x%lx, string ptr 0x%lx[%u] not in any chunk\n",
						 (ulong)pref,
						 (ulong)pref->value.bytes,
						 (uint)r_size(pref));
				  }
				break;
			    /****** SHOULD ALSO CHECK: ******/
			    /****** arrays, dict ******/
			    }
#undef pref
			    rp += packed_per_ref;
			  }
		  }
		else
		  {	struct_proc_enum_ptrs((*proc)) =
			  pre->o_type->enum_ptrs;
			uint index = 0;
			void *ptr;
			gs_ptr_type_t ptype;
			if ( proc != 0 )
			  for ( ; (ptype = (*proc)(pre + 1, size, index, &ptr)) != 0; ++index )
			    if ( ptr != 0 && ptype == ptr_struct_type )
			      ialloc_validate_object(ptr, NULL, gcst);
		  }
	END_OBJECTS_SCAN
}

/* Validate an object. */
void
ialloc_validate_object(const obj_header_t *ptr, const chunk_t *cp,
  gc_state_t *gcst)
{	const obj_header_t *pre = ptr - 1;
	ulong size = pre_obj_contents_size(pre);
	gs_memory_type_ptr_t otype = pre->o_type;
	const char *oname;

	if ( !gs_debug_c('?') )
		return;			/* no check */
	if ( cp == 0 && gcst != 0 )
	{	gc_state_t st;
		st = *gcst;		/* no side effects! */
		if ( !(cp = gc_locate(pre, &st)) )
		{	lprintf1("Object 0x%lx not in any chunk!\n",
				 (ulong)pre);
			return;/*gs_abort();*/
		}
	}
	if ( (cp != 0 &&
	      !(pre->o_large ? (const byte *)pre == cp->cbase :
		size <= cp->ctop - (const byte *)(pre + 1))) ||
	     otype->ssize == 0 ||
	     size % otype->ssize != 0 ||
	     (oname = struct_type_name_string(otype),
	      *oname < 33 || *oname > 126)
	   )
	{	lprintf4("Bad object 0x%lx(%lu), ssize = %u, in chunk 0x%lx!\n",
			 (ulong)pre, (ulong)size, otype->ssize, (ulong)cp);
		gs_abort();
	}
}

#else				/* !DEBUG */

void
ialloc_validate_spaces(const gs_dual_memory_t *dmem)
{
}

void
ialloc_validate_memory(const gs_ref_memory_t *mem, gc_state_t *gcst)
{
}

void
ialloc_validate_chunk(const chunk_t *cp, gc_state_t *gcst)
{
}

void
ialloc_validate_object(const obj_header_t *ptr, const chunk_t *cp,
  gc_state_t *gcst)
{
}

#endif				/* (!)DEBUG */
