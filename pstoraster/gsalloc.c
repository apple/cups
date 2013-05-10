/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gsalloc.c */
/* Standard memory allocator */
#include "gx.h"
#include "memory_.h"
#include "gsmdebug.h"
#include "gsstruct.h"
#include "gxalloc.h"

/*
 * This allocator produces tracing messages of the form
 *	[aNMOTS]...
 * where
 *   N is the VM space number,
 *   M is : for movable objects, | for immovable,
 *   O is {alloc = +, free = -, grow = >, shrink = <},
 *   T is {bytes = b, object = <, ref = $, string = >}, and
 *   S is {freelist = F, LIFO = space, own chunk = L, lost = #,
 *	lost own chunk = ~, other = .}.
 */

/* The structure descriptor for allocators.  Even though allocators */
/* are allocated outside GC space, they reference objects within it. */
public_st_ref_memory();
#define mptr ((gs_ref_memory_t *)vptr)
private ENUM_PTRS_BEGIN(ref_memory_enum_ptrs) return 0;
	ENUM_PTR(0, gs_ref_memory_t, changes);
	ENUM_PTR(1, gs_ref_memory_t, saved);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(ref_memory_reloc_ptrs) {
	RELOC_PTR(gs_ref_memory_t, changes);
	/* Don't relocate the pointer now -- see igc.c for details. */
	mptr->reloc_saved = gs_reloc_struct_ptr(mptr->saved, gcst);
} RELOC_PTRS_END

/* Forward references */
private obj_header_t *alloc_obj(P5(gs_ref_memory_t *, ulong, gs_memory_type_ptr_t, bool, client_name_t));
private chunk_t *alloc_add_chunk(P4(gs_ref_memory_t *, ulong, bool, client_name_t));
void alloc_close_chunk(P1(gs_ref_memory_t *));

#define imem ((gs_ref_memory_t *)mem)

/*
 * Define the standard implementation (with garbage collection)
 * of Ghostscript's memory manager interface.
 */
private gs_memory_proc_alloc_bytes(i_alloc_bytes);
private gs_memory_proc_alloc_bytes(i_alloc_bytes_immovable);
private gs_memory_proc_alloc_struct(i_alloc_struct);
private gs_memory_proc_alloc_struct(i_alloc_struct_immovable);
private gs_memory_proc_alloc_byte_array(i_alloc_byte_array);
private gs_memory_proc_alloc_byte_array(i_alloc_byte_array_immovable);
private gs_memory_proc_alloc_struct_array(i_alloc_struct_array);
private gs_memory_proc_alloc_struct_array(i_alloc_struct_array_immovable);
private gs_memory_proc_resize_object(i_resize_object);
private gs_memory_proc_object_size(i_object_size);
private gs_memory_proc_object_type(i_object_type);
private gs_memory_proc_free_object(i_free_object);
private gs_memory_proc_alloc_string(i_alloc_string);
private gs_memory_proc_alloc_string(i_alloc_string_immovable);
private gs_memory_proc_resize_string(i_resize_string);
private gs_memory_proc_free_string(i_free_string);
private gs_memory_proc_register_root(i_register_root);
private gs_memory_proc_unregister_root(i_unregister_root);
private gs_memory_proc_status(i_status);
private gs_memory_proc_enable_free(i_enable_free);
private const gs_memory_procs_t imemory_procs = {
	i_alloc_bytes,
	i_alloc_bytes_immovable,
	i_alloc_struct,
	i_alloc_struct_immovable,
	i_alloc_byte_array,
	i_alloc_byte_array_immovable,
	i_alloc_struct_array,
	i_alloc_struct_array_immovable,
	i_resize_object,
	i_object_size,
	i_object_type,
	i_free_object,
	i_alloc_string,
	i_alloc_string_immovable,
	i_resize_string,
	i_free_string,
	i_register_root,
	i_unregister_root,
	i_status,
	i_enable_free
};
/*
 * Allocate and mostly initialize the state of an allocator (system, global,
 * or local).  Does not initialize global or space.
 */
private void *ialloc_solo(P3(gs_memory_t *, gs_memory_type_ptr_t, chunk_t **));
gs_ref_memory_t *
ialloc_alloc_state(gs_memory_t *parent, uint chunk_size)
{	chunk_t *cp;
	gs_ref_memory_t *iimem = ialloc_solo(parent, &st_ref_memory, &cp);

	if ( iimem == 0 )
	  return 0;
	iimem->procs = imemory_procs;
	iimem->parent = parent;
	iimem->chunk_size = chunk_size;
	iimem->large_size = ((chunk_size / 4) & -obj_align_mod) + 1;
	iimem->gc_status.vm_threshold = chunk_size * 3L;
	iimem->gc_status.max_vm = max_long;
	iimem->gc_status.psignal = NULL;
	iimem->gc_status.enabled = false;
	iimem->previous_status.allocated = 0;
	iimem->previous_status.used = 0;
	ialloc_reset(iimem);
	iimem->cfirst = iimem->clast = cp;
	ialloc_set_limit(iimem);
	iimem->cc.cbot = iimem->cc.ctop = 0;
	iimem->pcc = 0;
	iimem->roots = 0;
	iimem->num_contexts = 1;
	iimem->saved = 0;
	return iimem;
}
/* Allocate a 'solo' object with its own chunk. */
private void *
ialloc_solo(gs_memory_t *parent, gs_memory_type_ptr_t pstype, chunk_t **pcp)
{	/*
	 * We can't assume that the parent uses the same object header
	 * that we do, but the GC requires that allocators have
	 * such a header.  Therefore, we prepend one explicitly.
	 */
	chunk_t *cp = gs_alloc_struct_immovable(parent, chunk_t, &st_chunk,
						"ialloc_solo(chunk)");
	uint csize =
	  round_up(sizeof(chunk_head_t) + sizeof(obj_header_t) +
		     pstype->ssize,
		   obj_align_mod);
	byte *cdata = gs_alloc_bytes_immovable(parent, csize, "ialloc_solo");
	obj_header_t *obj = (obj_header_t *)(cdata + sizeof(chunk_head_t));

	if ( cp == 0 || cdata == 0 )
	  return 0;
	alloc_init_chunk(cp, cdata, cdata + csize, false, (chunk_t *)NULL);
	cp->cbot = cp->ctop;
	cp->cprev = cp->cnext = 0;
	/* Construct the object header "by hand". */
	obj->o_large = 0;
	obj->o_size = pstype->ssize;
	obj->o_type = pstype;
	*pcp = cp;
	return (void *)(obj + 1);
}
/* Initialize after a save. */
void
ialloc_reset(gs_ref_memory_t *mem)
{	mem->cfirst = 0;
	mem->clast = 0;
	mem->cc.rcur = 0;
	mem->cc.rtop = 0;
	mem->cc.has_refs = false;
	mem->allocated = 0;
	mem->inherited = 0;
	mem->changes = 0;
	ialloc_reset_free(mem);
}
/* Initialize after a save or GC. */
void
ialloc_reset_free(gs_ref_memory_t *mem)
{	int i;
	obj_header_t **p;
	mem->freed_lost = 0;
	mem->cfreed.cp = 0;
	for ( i = 0, p = &mem->freelists[0]; i < num_freelists; i++, p++ )
	  *p = 0;
}
/* Set the allocation limit after a change in one or more of */
/* vm_threshold, max_vm, or enabled, or after a GC. */
void
ialloc_set_limit(register gs_ref_memory_t *mem)
{	/*
	 * The following code is intended to set the limit so that
	 * we stop allocating when allocated + previous_status.allocated
	 * exceeds the lesser of max_vm or (if GC is enabled)
	 * gc_allocated + vm_threshold.
	 */
	ulong max_allocated =
	  (mem->gc_status.max_vm > mem->previous_status.allocated ?
	   mem->gc_status.max_vm - mem->previous_status.allocated :
	   0);
	if ( mem->gc_status.enabled )
	  {	ulong limit = mem->gc_allocated + mem->gc_status.vm_threshold;
		if ( limit < mem->previous_status.allocated )
		  mem->limit = 0;
		else
		  {	limit -= mem->previous_status.allocated;
			mem->limit = min(limit, max_allocated);
		  }
	  }
	else
	  mem->limit = max_allocated;
	if_debug7('0', "[0]space=%d, max_vm=%ld, prev.alloc=%ld, enabled=%d,\n\
      gc_alloc=%ld, threshold=%ld => limit=%ld\n",
		  mem->space, (long)mem->gc_status.max_vm,
		  (long)mem->previous_status.allocated,
		  mem->gc_status.enabled, (long)mem->gc_allocated,
		  (long)mem->gc_status.vm_threshold, (long)mem->limit);
}

/* ================ Accessors ================ */

/* Get the size of an object from the header. */
private uint
i_object_size(gs_memory_t *mem, const void /*obj_header_t*/ *obj)
{	return pre_obj_contents_size((const obj_header_t *)obj - 1);
}

/* Get the type of a structure from the header. */
private gs_memory_type_ptr_t
i_object_type(gs_memory_t *mem, const void /*obj_header_t*/ *obj)
{	return ((const obj_header_t *)obj - 1)->o_type;
}

/* Get the GC status of a memory. */
void
gs_memory_gc_status(const gs_ref_memory_t *mem, gs_memory_gc_status_t *pstat)
{	*pstat = mem->gc_status;
}

/* Set the GC status of a memory. */
void
gs_memory_set_gc_status(gs_ref_memory_t *mem, const gs_memory_gc_status_t *pstat)
{	mem->gc_status = *pstat;
	ialloc_set_limit(mem);
}

/* ================ Objects ================ */

/* Allocate a small object quickly if possible. */
/* The size must be substantially less than max_uint. */
/* ptr must be declared as obj_header_t *. */
/* pfl must be declared as obj_header_t **. */
#define IF_FREELIST_ALLOC(ptr, imem, size, pstype, pfl)\
	if ( size <= max_freelist_size &&\
	     *(pfl = &imem->freelists[(size + obj_align_mask) >> log2_obj_align_mod]) != 0\
	   )\
	{	ptr = *pfl;\
		*pfl = *(obj_header_t **)ptr;\
		ptr[-1].o_size = size;\
		ptr[-1].o_type = pstype;\
		/* If debugging, clear the block in an attempt to */\
		/* track down uninitialized data errors. */\
		gs_alloc_fill(ptr, gs_alloc_fill_alloc, size);
#define ELSEIF_LIFO_ALLOC(ptr, imem, size, pstype)\
	}\
	else if ( (imem->cc.ctop - (byte *)(ptr = (obj_header_t *)imem->cc.cbot))\
		>= size + (obj_align_mod + sizeof(obj_header_t) * 2) &&\
	     size < imem->large_size\
	   )\
	{	imem->cc.cbot = (byte *)ptr + obj_size_round(size);\
		ptr->o_large = 0;\
		ptr->o_size = size;\
		ptr->o_type = pstype;\
		ptr++;\
		/* If debugging, clear the block in an attempt to */\
		/* track down uninitialized data errors. */\
		gs_alloc_fill(ptr, gs_alloc_fill_alloc, size);
#define ELSE_ALLOC\
	}\
	else

private byte *
i_alloc_bytes(gs_memory_t *mem, uint size, client_name_t cname)
{	obj_header_t *obj;
	obj_header_t **pfl;
	IF_FREELIST_ALLOC(obj, imem, size, &st_bytes, pfl)
		if_debug4('A', "[a%d:+bF]%s -bytes-(%u) = 0x%lx\n",
			  imem->space,
			  client_name_string(cname), size, (ulong)obj);
	ELSEIF_LIFO_ALLOC(obj, imem, size, &st_bytes)
		if_debug4('A', "[a%d:+b ]%s -bytes-(%u) = 0x%lx\n",
			  imem->space,
			  client_name_string(cname), size, (ulong)obj);
	ELSE_ALLOC
	{	obj = alloc_obj(imem, size, &st_bytes, false, cname);
		if ( obj == 0 )
		  return 0;
		if_debug4('A', "[a%d:+b.]%s -bytes-(%u) = 0x%lx\n",
			  imem->space,
			  client_name_string(cname), size, (ulong)obj);
	}
	return (byte *)obj;
}
private byte *
i_alloc_bytes_immovable(gs_memory_t *mem, uint size, client_name_t cname)
{	obj_header_t *obj = alloc_obj(imem, size, &st_bytes, true, cname);
	if ( obj == 0 )
	  return 0;
	if_debug4('A', "[a%d|+b.]%s -bytes-(%u) = 0x%lx\n",
		  imem->space, client_name_string(cname), size, (ulong)obj);
	return (byte *)obj;
}
private void *
i_alloc_struct(gs_memory_t *mem, gs_memory_type_ptr_t pstype,
  client_name_t cname)
{	uint size = pstype->ssize;
	obj_header_t *obj;
	obj_header_t **pfl;
	IF_FREELIST_ALLOC(obj, imem, size, pstype, pfl)
		if_debug5('A', "[a%d:+<F]%s %s(%u) = 0x%lx\n",
			  imem->space, client_name_string(cname),
			  struct_type_name_string(pstype), size, (ulong)obj);
	ELSEIF_LIFO_ALLOC(obj, imem, size, pstype)
		if_debug5('A', "[a%d:+< ]%s %s(%u) = 0x%lx\n",
			  imem->space, client_name_string(cname),
			  struct_type_name_string(pstype), size, (ulong)obj);
	ELSE_ALLOC
	{	obj = alloc_obj(imem, size, pstype, false, cname);
		if ( obj == 0 )
		  return 0;
		if_debug5('A', "[a%d:+<.]%s %s(%u) = 0x%lx\n",
			  imem->space, client_name_string(cname),
			  struct_type_name_string(pstype), size, (ulong)obj);
	}
	return obj;
}
private void *
i_alloc_struct_immovable(gs_memory_t *mem, gs_memory_type_ptr_t pstype,
  client_name_t cname)
{	uint size = pstype->ssize;
	obj_header_t *obj = alloc_obj(imem, size, pstype, true, cname);
	if ( obj == 0 )
	  return 0;
	if_debug5('A', "[a%d|+<.]%s %s(%u) = 0x%lx\n",
		  imem->space, client_name_string(cname),
		  struct_type_name_string(pstype), size, (ulong)obj);
	return obj;
}
private byte *
i_alloc_byte_array(gs_memory_t *mem, uint num_elements, uint elt_size,
  client_name_t cname)
{	obj_header_t *obj = alloc_obj(imem, (ulong)num_elements * elt_size,
				      &st_bytes, false, cname);
	if_debug6('A', "[a%d:+b.]%s -bytes-*(%lu=%u*%u) = 0x%lx\n",
		  imem->space, client_name_string(cname),
		  (ulong)num_elements * elt_size,
		  num_elements, elt_size, (ulong)obj);
	return (byte *)obj;
}
private byte *
i_alloc_byte_array_immovable(gs_memory_t *mem, uint num_elements,
  uint elt_size, client_name_t cname)
{	obj_header_t *obj = alloc_obj(imem, (ulong)num_elements * elt_size,
				      &st_bytes, true, cname);
	if_debug6('A', "[a%d|+b.]%s -bytes-*(%lu=%u*%u) = 0x%lx\n",
		  imem->space, client_name_string(cname),
		  (ulong)num_elements * elt_size,
		  num_elements, elt_size, (ulong)obj);
	return (byte *)obj;
}
private void *
i_alloc_struct_array(gs_memory_t *mem, uint num_elements,
  gs_memory_type_ptr_t pstype, client_name_t cname)
{	obj_header_t *obj = alloc_obj(imem,
				(ulong)num_elements * pstype->ssize,
				pstype, false, cname);
	if_debug7('A', "[a%d:+<.]%s %s*(%lu=%u*%u) = 0x%lx\n",
		  imem->space,
		  client_name_string(cname), struct_type_name_string(pstype),
		  (ulong)num_elements * pstype->ssize,
		  num_elements, pstype->ssize, (ulong)obj);
	return (char *)obj;
}
private void *
i_alloc_struct_array_immovable(gs_memory_t *mem, uint num_elements,
  gs_memory_type_ptr_t pstype, client_name_t cname)
{	obj_header_t *obj = alloc_obj(imem,
				      (ulong)num_elements * pstype->ssize,
				      pstype, true, cname);
	if_debug7('A', "[a%d|+<.]%s %s*(%lu=%u*%u) = 0x%lx\n",
		  imem->space,
		  client_name_string(cname), struct_type_name_string(pstype),
		  (ulong)num_elements * pstype->ssize,
		  num_elements, pstype->ssize, (ulong)obj);
	return (char *)obj;
}
private void *
i_resize_object(gs_memory_t *mem, void *obj, uint new_num_elements,
  client_name_t cname)
{	obj_header_t *pp = (obj_header_t *)obj - 1;
	gs_memory_type_ptr_t pstype = pp->o_type;
	ulong old_size = pre_obj_contents_size(pp);
	ulong new_size = (ulong)pstype->ssize * new_num_elements;
	void *new_obj;

	if ( (byte *)obj + obj_align_round(old_size) == imem->cc.cbot &&
	     imem->cc.ctop - imem->cc.cbot < new_size + obj_align_mod
	   )
	  { imem->cc.cbot = (byte *)obj + obj_align_round(new_size);
	    if_debug8('A', "[a%d:%c%c ]%s %s(%lu=>%lu) 0x%lx\n",
		      imem->space,
		      (new_size > old_size ? '>' : '<'),
		      (pstype == &st_bytes ? 'b' : '<'),
		      client_name_string(cname),
		      struct_type_name_string(pstype),
		      old_size, new_size, (ulong)obj);
	    return obj;
	  }
	/* Punt. */
	new_obj = gs_alloc_struct_array(mem, new_num_elements, void,
					pstype, cname);
	if ( new_obj == 0 )
	  return 0;
	memcpy(new_obj, obj, min(old_size, new_size));
	gs_free_object(mem, obj, cname);
	return new_obj;
}
private void
i_free_object(gs_memory_t *mem, void *ptr, client_name_t cname)
{	obj_header_t *pp;
	struct_proc_finalize((*finalize));
	uint size;

	if ( ptr == 0 )
	  return;
	pp = (obj_header_t *)ptr - 1;
#ifdef DEBUG
	if ( gs_debug_c('?') )
	  {	if ( pp->o_type == &st_free )
		  {	lprintf2("%s: object 0x%lx already free!\n",
				 client_name_string(cname), (ulong)pp);
			return;/*gs_abort();*/
		  }
	  }
#endif
	size = pre_obj_contents_size(pp);
	finalize = pp->o_type->finalize;
	if ( finalize != 0 )
	  {	if_debug3('u', "[u]finalizing %s 0x%lx (%s)\n",
			  struct_type_name_string(pp->o_type),
			  (ulong)ptr, client_name_string(cname));
		(*finalize)(ptr);
	  }
	if ( (byte *)ptr + obj_align_round(size) == imem->cc.cbot )
	{	if_debug4('A', "[a%d:-o ]%s(%u) 0x%lx\n", imem->space,
			  client_name_string(cname), size, (ulong)pp);
		gs_alloc_fill(ptr, gs_alloc_fill_free, size);
		imem->cc.cbot = (byte *)pp;
		return;
	}
	if ( pp->o_large )
	{	/*
		 * We gave this object its own chunk.  Free the entire chunk,
		 * unless it belongs to an older save level, in which case
		 * we mustn't overwrite it.
		 */
		chunk_locator_t cl;
#ifdef DEBUG
		{ chunk_locator_t cld;
		  cld.memory = imem;
		  cld.cp = 0;
		  if_debug5('a', "[a%d:-o%c]%s(%u) 0x%lx\n", imem->space,
			    (chunk_locate_ptr(ptr, &cld) ? 'L' : '~'),
			    client_name_string(cname), size, (ulong)pp);
		}
#endif
		cl.memory = imem;
		cl.cp = 0;
		if ( chunk_locate_ptr(ptr, &cl) )
		  { alloc_free_chunk(cl.cp, imem);
		    return;
		  }
		/* Don't overwrite even if gs_alloc_debug is set. */
	}
	if ( size <= max_freelist_size &&
	     size >= sizeof(obj_header_t *)
	   )
	  {	/*
		 * Put the object on a freelist, unless it belongs to
		 * an older save level, in which case we mustn't
		 * overwrite it.
		 */
		imem->cfreed.memory = imem;
		if ( chunk_locate(ptr, &imem->cfreed) )
		  {	obj_header_t **pfl =
			  &imem->freelists[(size + obj_align_mask) >>
					   log2_obj_align_mod];
			pp->o_type = &st_free;	/* don't confuse GC */
			gs_alloc_fill(ptr, gs_alloc_fill_free, size);
			*(obj_header_t **)ptr = *pfl;
			*pfl = (obj_header_t *)ptr;
			if_debug4('A', "[a%d:-oF]%s(%u) 0x%lx\n",
				  imem->space, client_name_string(cname),
				  size, (ulong)pp);
			return;
		  }
		/* Don't overwrite even if gs_alloc_debug is set. */
	  }
	else
	  {	pp->o_type = &st_free;	/* don't confuse GC */
		gs_alloc_fill(ptr, gs_alloc_fill_free, size);
	  }
	if_debug4('A', "[a%d:-o#]%s(%u) 0x%lx\n", imem->space,
		  client_name_string(cname), size, (ulong)pp);
	imem->freed_lost += obj_size_round(size);
}
private byte *
i_alloc_string(gs_memory_t *mem, uint nbytes, client_name_t cname)
{	byte *str;
top:	if ( imem->cc.ctop - imem->cc.cbot > nbytes )
	{	if_debug4('A', "[a%d:+> ]%s(%u) = 0x%lx\n", imem->space,
			  client_name_string(cname), nbytes,
			  (ulong)(imem->cc.ctop - nbytes));
		str = imem->cc.ctop -= nbytes;
		gs_alloc_fill(str, gs_alloc_fill_alloc, nbytes);
		return str;
	}
	if ( nbytes > string_space_quanta(max_uint - sizeof(chunk_head_t)) *
	      string_data_quantum
	   )
	{	/* Can't represent the size in a uint! */
		return 0;
	}
	if ( nbytes >= imem->large_size )
	{	/* Give it a chunk all its own. */
		return i_alloc_string_immovable(mem, nbytes, cname);
	}
	else
	{	/* Add another chunk. */
		chunk_t *cp =
		  alloc_add_chunk(imem, (ulong)imem->chunk_size, true,
				  "chunk");
		if ( cp == 0 )
		  return 0;
		alloc_close_chunk(imem);
		imem->pcc = cp;
		imem->cc = *imem->pcc;
		goto top;
	}
}
private byte *
i_alloc_string_immovable(gs_memory_t *mem, uint nbytes, client_name_t cname)
{	byte *str;
	/* Give it a chunk all its own. */
	uint asize = string_chunk_space(nbytes) + sizeof(chunk_head_t);
	chunk_t *cp = alloc_add_chunk(imem, (ulong)asize, true,
				      "large string chunk");
	if ( cp == 0 )
	  return 0;
	str = cp->ctop = cp->climit - nbytes;
	if_debug4('a', "[a%d|+>L]%s(%u) = 0x%lx\n", imem->space,
		  client_name_string(cname), nbytes, (ulong)str);
	gs_alloc_fill(str, gs_alloc_fill_alloc, nbytes);
	return str;
}
private byte *
i_resize_string(gs_memory_t *mem, byte *data, uint old_num, uint new_num,
  client_name_t cname)
{	byte *ptr;
	if ( data == imem->cc.ctop &&
	       (new_num < old_num ||
		imem->cc.ctop - imem->cc.cbot > new_num - old_num)
	   )
	{	/* Resize in place. */
		register uint i;
		ptr = data + old_num - new_num;
		if_debug6('A', "[a%d:%c> ]%s(%u->%u) 0x%lx\n",
			  imem->space, (new_num > old_num ? '>' : '<'),
			  client_name_string(cname), old_num, new_num,
			  (ulong)ptr);
		imem->cc.ctop = ptr;
		if ( new_num < old_num )
		  for ( i = new_num; i > 0; ) --i, ptr[i] = data[i];
		else
		  for ( i = 0; i < old_num; ) ptr[i] = data[i], i++;
	}
	else
	{	/* Punt. */
		ptr = gs_alloc_string(mem, new_num, cname);
		if ( ptr == 0 )
		  return 0;
		memcpy(ptr, data, min(old_num, new_num));
		gs_free_string(mem, data, old_num, cname);
	}
	return ptr;
}
private void
i_free_string(gs_memory_t *mem, byte *data, uint nbytes,
  client_name_t cname)
{	if ( data == imem->cc.ctop )
	{	if_debug4('A', "[a%d:-> ]%s(%u) 0x%lx\n", imem->space,
			  client_name_string(cname), nbytes, (ulong)data);
		imem->cc.ctop += nbytes;
	}
	else
	{	if_debug4('A', "[a%d:->#]%s(%u) 0x%lx\n", imem->space,
			  client_name_string(cname), nbytes, (ulong)data);
		imem->freed_lost += nbytes;
	}
}
private void
i_status(gs_memory_t *mem, gs_memory_status_t *pstat)
{	ulong unused = imem->freed_lost;
	ulong inner = 0;
	alloc_close_chunk(imem);
	/* Add up unallocated space within each chunk. */
	/* Also keep track of space allocated to inner chunks, */
	/* which are included in previous_status.allocated. */
	  {	const chunk_t *cp = imem->cfirst;
		while ( cp != 0 )
		  {	unused += cp->ctop - cp->cbot;
			if ( cp->outer )
			  inner += cp->cend - (byte *)cp->chead;
			cp = cp->cnext;
		  }
	  }
	/* Add up space on free lists. */
	  {	int i;
		const obj_header_t *pfree;
		for ( i = 0; i < num_freelists; i++ )
		  {	uint free_size =
			  (i << log2_obj_align_mod) + sizeof(obj_header_t);
			for ( pfree = imem->freelists[i]; pfree != 0;
			      pfree = *(const obj_header_t * const *)pfree
			    )
			  unused += free_size;
		  }
	  }
	pstat->used = imem->allocated + inner - unused +
	  imem->previous_status.used;
	pstat->allocated = imem->allocated +
	  imem->previous_status.allocated;
}
private void
i_enable_free(gs_memory_t *mem, bool enable)
{	if ( enable )
	  mem->procs.free_object = i_free_object,
	  mem->procs.free_string = i_free_string;
	else
	  mem->procs.free_object = gs_ignore_free_object,
	  mem->procs.free_string = gs_ignore_free_string;
}

/* ------ Internal procedures ------ */

/* Allocate an object.  This handles all but the fastest, simplest case. */
private obj_header_t *
alloc_obj(gs_ref_memory_t *mem, ulong lsize, gs_memory_type_ptr_t pstype,
  bool immovable, client_name_t cname)
{	obj_header_t *ptr;
	if ( lsize >= mem->large_size || immovable )
	{	ulong asize =
		  ((lsize + obj_align_mask) & -obj_align_mod) +
		    sizeof(obj_header_t);
		/* Give it a chunk all its own. */
		chunk_t *cp =
		  alloc_add_chunk(mem, asize + sizeof(chunk_head_t), false,
				  "large object chunk");
		if ( cp == 0 )
			return 0;
		ptr = (obj_header_t *)cp->cbot;
		cp->cbot += asize;
		if_debug5('a', "[a%d%c+ L]%s(%lu) = 0x%lx\n",
			  imem->space, (immovable ? '|' : ':'),
			  client_name_string(cname), lsize, (ulong)ptr);
		ptr->o_large = 1;
		pre_obj_set_large_size(ptr, lsize);
	}
	else
	{	uint asize = obj_size_round((uint)lsize);
		while ( mem->cc.ctop -
			 (byte *)(ptr = (obj_header_t *)mem->cc.cbot)
			  <= asize + sizeof(obj_header_t) )
		{	/* Add another chunk. */
			chunk_t *cp =
			  alloc_add_chunk(mem, (ulong)mem->chunk_size,
					  true, "chunk");
			if ( cp == 0 )
				return 0;
			alloc_close_chunk(mem);
			mem->pcc = cp;
			mem->cc = *mem->pcc;
		}
		mem->cc.cbot = (byte *)ptr + asize;
		ptr->o_large = 0;
		ptr->o_size = (uint)lsize;
	}
	ptr->o_type = pstype;
	ptr++;
        gs_alloc_fill(ptr, gs_alloc_fill_alloc, lsize);
	return ptr;
}

/* ================ Roots ================ */

/* Register a root. */
private void
i_register_root(gs_memory_t *mem, gs_gc_root_t *rp, gs_ptr_type_t ptype,
  void **up, client_name_t cname)
{	if_debug3('8', "[8]register root(%s) 0x%lx -> 0x%lx\n",
		 client_name_string(cname), (ulong)rp, (ulong)up);
	rp->ptype = ptype, rp->p = up;
	rp->next = imem->roots, imem->roots = rp;
}

/* Unregister a root. */
private void
i_unregister_root(gs_memory_t *mem, gs_gc_root_t *rp, client_name_t cname)
{	gs_gc_root_t **rpp = &imem->roots;
	if_debug2('8', "[8]unregister root(%s) 0x%lx\n",
		client_name_string(cname), (ulong)rp);
	while ( *rpp != rp ) rpp = &(*rpp)->next;
	*rpp = (*rpp)->next;
}

/* ================ Chunks ================ */

public_st_chunk();

/* Insert a chunk in the chain.  This is exported for the GC and for */
/* the forget_save operation. */
void
alloc_link_chunk(chunk_t *cp, gs_ref_memory_t *mem)
{	byte *cdata = cp->cbase;
	chunk_t *icp;
	chunk_t *prev;
	for ( icp = mem->cfirst; icp != 0 && ptr_ge(cdata, icp->ctop);
	      icp = icp->cnext
	    )
		;
	cp->cnext = icp;
	if ( icp == 0 )			/* add at end of chain */
	{	prev = imem->clast;
		imem->clast = cp;
	}
	else				/* insert before icp */
	{	prev = icp->cprev;
		icp->cprev = cp;
	}
	cp->cprev = prev;
	if ( prev == 0 )
		imem->cfirst = cp;
	else
		prev->cnext= cp;
	if ( imem->pcc != 0 )
	{	imem->cc.cnext = imem->pcc->cnext;
		imem->cc.cprev = imem->pcc->cprev;
	}
}

/* Allocate a chunk.  If we would exceed MaxLocalVM (if relevant), */
/* or if we would exceed the VMThreshold and psignal is NULL, */
/* return 0; if we would exceed the VMThreshold but psignal is valid, */
/* just set the signal and return successfully. */
private chunk_t *
alloc_add_chunk(gs_ref_memory_t *mem, ulong csize, bool has_strings,
  client_name_t cname)
{	gs_memory_t *parent = mem->parent;
	chunk_t *cp = gs_alloc_struct_immovable(parent, chunk_t, &st_chunk,
						cname);
	byte *cdata;
	/* If csize is larger than max_uint, */
	/* we have to fake it using gs_alloc_byte_array. */
	ulong elt_size = csize;
	uint num_elts = 1;
	if ( (ulong)(mem->allocated + mem->inherited) >= mem->limit )
	  {	mem->gc_status.requested += csize;
		if ( mem->limit >= mem->gc_status.max_vm ||
		     mem->gc_status.psignal == 0
		   )
			return 0;
		if_debug4('0', "[0]signaling space=%d, allocated=%ld, limit=%ld, requested=%ld\n",
			  mem->space, (long)mem->allocated,
			  (long)mem->limit, (long)mem->gc_status.requested);
		*mem->gc_status.psignal = mem->gc_status.signal_value;
	  }
	while ( (uint)elt_size != elt_size )
	  elt_size = (elt_size + 1) >> 1,
	  num_elts <<= 1;
	cdata = gs_alloc_byte_array_immovable(parent, num_elts, elt_size,
					      cname);
	if ( cp == 0 || cdata == 0 )
	{	gs_free_object(parent, cdata, cname);
		gs_free_object(parent, cp, cname);
		mem->gc_status.requested = csize;
		return 0;
	}
	alloc_init_chunk(cp, cdata, cdata + csize, has_strings, (chunk_t *)0);
	alloc_link_chunk(cp, mem);
	mem->allocated +=
	  gs_object_size(parent, cdata) + gs_object_size(parent, cp);
	return cp;
}

/* Initialize the pointers in a chunk.  This is exported for save/restore. */
/* The bottom pointer must be aligned, but the top pointer need not */
/* be aligned. */
void
alloc_init_chunk(chunk_t *cp, byte *bot, byte *top, bool has_strings,
  chunk_t *outer)
{	byte *cdata = bot;
	if ( outer != 0 )
	  outer->inner_count++;
	cp->chead = (chunk_head_t *)cdata;
	cdata += sizeof(chunk_head_t);
	cp->cbot = cp->cbase = cdata;
	cp->cend = top;
	cp->rcur = 0;
	cp->rtop = 0;
	cp->outer = outer;
	cp->inner_count = 0;
	cp->has_refs = false;
	cp->sbase = cdata;
	if ( has_strings && top - cdata >= string_space_quantum + sizeof(long) - 1)
	{	/*
		 * We allocate a large enough string marking and reloc table
		 * to cover the entire chunk.
		 */
		uint nquanta = string_space_quanta(top - cdata);
		cp->climit = cdata + nquanta * string_data_quantum;
		cp->smark = cp->climit;
		cp->smark_size = string_quanta_mark_size(nquanta);
		cp->sreloc =
		  (string_reloc_offset *)(cp->smark + cp->smark_size);
	}
	else
	{	/* No strings, don't need the string GC tables. */
		cp->climit = cp->cend;
		cp->smark = 0;
		cp->smark_size = 0;
		cp->sreloc = 0;
	}
	cp->ctop = cp->climit;
}

/* Close up the current chunk. */
/* This is exported for save/restore and the GC. */
void
alloc_close_chunk(gs_ref_memory_t *mem)
{	if ( mem->pcc != 0 )
	{	*mem->pcc = mem->cc;
#ifdef DEBUG
		if ( gs_debug_c('a') )
		  {	dprintf1("[a%d]", mem->space);
			dprintf_chunk("closing chunk", mem->pcc);
		  }
#endif
	}
}

/* Reopen the current chunk after a GC or restore. */
void
alloc_open_chunk(gs_ref_memory_t *mem)
{	if ( mem->pcc != 0 )
	{	mem->cc = *mem->pcc;
#ifdef DEBUG
		if ( gs_debug_c('a') )
		  {	dprintf1("[a%d]", mem->space);
			dprintf_chunk("opening chunk", mem->pcc);
		  }
#endif
	}
}

/* Remove a chunk from the chain.  This is exported for the GC. */
void
alloc_unlink_chunk(chunk_t *cp, gs_ref_memory_t *mem)
{
#ifdef DEBUG
	if ( gs_alloc_debug )
	  {	/* Check to make sure this chunk belongs to this allocator. */
		const chunk_t *ap = mem->cfirst;
		while ( ap != 0 && ap != cp )
		  ap = ap->cnext;
		if ( ap != cp )
		  {	lprintf2("unlink_chunk 0x%lx not owned by memory 0x%lx!\n",
				 (ulong)cp, (ulong)mem);
			return;/*gs_abort();*/
		  }
	  }
#endif
	if ( cp->cprev == 0 )
	  mem->cfirst = cp->cnext;
	else
	  cp->cprev->cnext = cp->cnext;
	if ( cp->cnext == 0 )
	  mem->clast = cp->cprev;
	else
	  cp->cnext->cprev = cp->cprev;
	if ( mem->pcc != 0 )
	  {	mem->cc.cnext = mem->pcc->cnext;
		mem->cc.cprev = mem->pcc->cprev;
		if ( mem->pcc == cp )
		  {	mem->pcc = 0;
			mem->cc.cbot = mem->cc.ctop = 0;
		  }
	  }
}

/* Free a chunk.  This is exported for save/restore and for the GC. */
void
alloc_free_chunk(chunk_t *cp, gs_ref_memory_t *mem)
{	gs_memory_t *parent = mem->parent;
	alloc_unlink_chunk(cp, mem);
	if ( mem->cfreed.cp == cp )
	  mem->cfreed.cp = 0;
	if ( cp->outer == 0 )
	  {	byte *cdata = (byte *)cp->chead;
		mem->allocated -= gs_object_size(parent, cdata);
		gs_free_object(parent, cdata, "alloc_free_chunk(data)");
	  }
	else
	  cp->outer->inner_count--;
	mem->allocated -= gs_object_size(parent, cp);
	gs_free_object(parent, cp, "alloc_free_chunk(chunk struct)");
}

/* Find the chunk for a pointer. */
/* Note that this only searches the current save level. */
/* Since a given save level can't contain both a chunk and an inner chunk */
/* of that chunk, we can stop when is_within_chunk succeeds, and just test */
/* is_in_inner_chunk then. */
bool
chunk_locate_ptr(const void *vptr, chunk_locator_t *clp)
{	register chunk_t *cp = clp->cp;
	if ( cp == 0 )
	{	cp = clp->memory->cfirst;
		if ( cp == 0 )
			return false;
	}
#define ptr (const byte *)vptr
	if ( ptr_lt(ptr, cp->cbase) )
	{	do
		{	cp = cp->cprev;
			if ( cp == 0 )
				return false;
		}
		while ( ptr_lt(ptr, cp->cbase) );
		if ( ptr_ge(ptr, cp->cend) )
		  return false;
	}
	else
	{	while ( ptr_ge(ptr, cp->cend) )
		{	cp = cp->cnext;
			if ( cp == 0 )
				return false;
		}
		if ( ptr_lt(ptr, cp->cbase) )
		  return false;
	}
	clp->cp = cp;
	return !ptr_is_in_inner_chunk(ptr, cp);
#undef ptr
}

/* ------ Debugging printout ------ */

#ifdef DEBUG

void
debug_print_chunk(const chunk_t *cp)
{	dprintf1("chunk at 0x%lx:\n", (ulong)cp);
	dprintf3("   chead=0x%lx  cbase=0x%lx sbase=0x%lx\n",
		 (ulong)cp->chead, (ulong)cp->cbase, (ulong)cp->sbase);
	dprintf3("    rcur=0x%lx   rtop=0x%lx  cbot=0x%lx\n",
		 (ulong)cp->rcur, (ulong)cp->rtop, (ulong)cp->cbot);
	dprintf4("    ctop=0x%lx climit=0x%lx smark=0x%lx, size=%u\n",
		 (ulong)cp->ctop, (ulong)cp->climit, (ulong)cp->smark,
		 cp->smark_size);
	dprintf2("  sreloc=0x%lx   cend=0x%lx\n",
		 (ulong)cp->sreloc, (ulong)cp->cend);
	dprintf5("cprev=0x%lx cnext=0x%lx outer=0x%lx inner_count=%u has_refs=%s\n",
		 (ulong)cp->cprev, (ulong)cp->cnext, (ulong)cp->outer,
		 cp->inner_count, (cp->has_refs? "true" : "false"));

	SCAN_CHUNK_OBJECTS(cp)
	  DO_ALL
	    dprintf3("    pre=0x%lx type=0x%lx size=%lu  ",
		     (ulong)pre, (ulong)pre->o_type, size);
	    if ( pre->o_large )
	      dprintf2("lmark=%d large size=%u\n",
		       pre->o_lmark, pre->o_lsize);
	    else
	      dprintf2("smark/back=%u (0x%x)\n", pre->o_smark, pre->o_smark);
/* Temporarily redefine gs_exit so a chunk parsing error */
/* won't actually exit. */
#define gs_exit(n) DO_NOTHING
	END_OBJECTS_SCAN
#undef gs_exit
}

#endif					/* DEBUG */
