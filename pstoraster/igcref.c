/* Copyright (C) 1994, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* igcref.c */
/* ref garbage collector for Ghostscript */
#include "memory_.h"
#include "ghost.h"
#include "gsexit.h"
#include "gsstruct.h"			/* for gxalloc.h */
#include "iname.h"
#include "iastate.h"
#include "idebug.h"
#include "igc.h"
#include "ipacked.h"
#include "store.h"			/* for ref_assign_inline */

/*
 * Define the 'structure' type descriptor for refs.
 * This is special because it has different shared procs.
 */
private gc_proc_clear_reloc(refs_clear_reloc);
private gc_proc_set_reloc(refs_set_reloc);
private gc_proc_compact(refs_compact);
private const struct_shared_procs_t refs_shared_procs =
 { refs_clear_reloc, refs_set_reloc, refs_compact };
private struct_proc_clear_marks(refs_clear_marks);
private struct_proc_reloc_ptrs(refs_do_reloc);
const gs_memory_struct_type_t st_refs =
 { sizeof(ref), "refs", &refs_shared_procs, refs_clear_marks, 0, refs_do_reloc };

/*
 * Define the GC procedures for structs that actually contain refs.
 * These are special because the shared refs_* procedures
 * are never called.  Instead, we unmark the individual refs in clear_marks,
 * disregard refs_*_reloc (because we will never relocate a ptr_ref_type
 * pointer pointing into the structure), disregard refs_compact (because
 * compaction is never required), and remove the marks in reloc_ptrs.
 * See also the comment about ptr_ref_type in imemory.h.
 */
CLEAR_MARKS_PROC(ref_struct_clear_marks) {
	ref *pref = (ref *)vptr;
	ref *end = (ref *)((char *)vptr + size);
	for ( ; pref < end; pref++ )
		r_clear_attrs(pref, l_mark);
}
ENUM_PTRS_BEGIN_PROC(ref_struct_enum_ptrs) {
	if ( index >= size / sizeof(ref) )
	  return 0;
	*pep = (ref *)vptr + index;
	return ptr_ref_type;
ENUM_PTRS_END_PROC }
RELOC_PTRS_BEGIN(ref_struct_reloc_ptrs) {
	ref *beg = vptr;
	ref *end = (ref *)((char *)vptr + size);
	gs_reloc_refs((ref_packed *)beg, (ref_packed *)end, gcst);
	ref_struct_clear_marks(vptr, size);
} RELOC_PTRS_END

/* ------ Unmarking phase ------ */

/* Unmark a single ref. */
void
ptr_ref_unmark(void *vptr, gc_state_t *ignored)
{	if ( r_is_packed((ref *)vptr) )
	  r_clear_pmark((ref_packed *)vptr);
	else
	  r_clear_attrs((ref *)vptr, l_mark);
}

/* Unmarking routine for ref objects. */
private void
refs_clear_marks(void /*obj_header_t*/ *vptr, uint size)
{	gs_mark_refs((ref_packed *)vptr,
		     (ref *)((byte *)vptr + size),
		     false);
}
/* Mark or unmark a block of refs. */
/* The last ref must be full-size, and is never marked. */
void
gs_mark_refs(ref_packed *from, ref *to, bool mark)
{	ref_packed *rp = from;
	ushort pmark = (mark ? lp_mark : 0);
	ushort rmark = (mark ? l_mark : 0);
	/* Since the last ref is full-size, we only need to check for */
	/* the end of the block when we see one of those. */
	for ( ; ; )
	{	if ( r_is_packed(rp) )
		{
#ifdef DEBUG
if ( gs_debug_c('8') )
{			dprintf1("  [8]unmark packed 0x%lx ", (ulong)rp);
			debug_print_ref((const ref *)rp);
			dprintf("\n");
}
#endif
			r_store_pmark(rp, pmark);
			rp++;
		}
		else			/* full-size ref */
		{
#ifdef DEBUG
if ( gs_debug_c('8') )
{			dprintf1("  [8]unmark ref 0x%lx ", (ulong)rp);
			debug_print_ref((ref *)rp);
			dprintf("\n");
}
#endif
			r_store_attrs((ref *)rp, l_mark, rmark);
			rp += packed_per_ref;
			if ( rp >= (ref_packed *)to )
			{	/* Ensure the last ref is not marked. */
				r_clear_attrs((ref *)rp - 1, l_mark);
				break;
			}
		}
	}
}

/* ------ Marking phase ------ */

/* Mark a ref.  Return true if new mark. */
bool
ptr_ref_mark(void *vptr, gc_state_t *ignored)
{	if ( r_is_packed(vptr) )
	  { if ( r_has_pmark((ref_packed *)vptr) )
	      return false;
	    r_set_pmark((ref_packed *)vptr);
	  }
	else
	  { if ( r_has_attr((ref *)vptr, l_mark) )
	      return false;
	    r_set_attrs((ref *)vptr, l_mark);
	  }
	return true;
}

/* ------ Relocation planning phase ------ */

/*
 * We store relocation in the size field of refs that don't use it,
 * so that we don't have to scan all the way to an unmarked object.
 * We must avoid nulls, which sometimes have useful information
 * in their size fields, and the types above t_next_index, which are
 * actually operators in disguise and also use the size field.
 */
#define case_types_using_size\
  case t_null: case_types_with_size

/* Clear the relocation for a ref object. */
private void
refs_clear_reloc(obj_header_t *hdr, uint size)
{	register ref_packed *rp = (ref_packed *)(hdr + 1);
	ref_packed *end = (ref_packed *)((byte *)rp + size);
	while ( rp < end )
	{	if ( r_is_packed(rp) )
			rp++;
		else			/* full-size ref */
		  {	uint type;
			/* Store the relocation here if possible. */
			switch ( type = r_type((ref *)rp) )
			{	case_types_using_size:
					break;
				default:
					if ( type >= t_next_index )
					  break;
					if_debug1('8', "  [8]clearing reloc at 0x%lx\n",
						  (ulong)rp);
					r_set_size((ref *)rp, 0);
			}
			rp += packed_per_ref;
		}
	}
}

/* Set the relocation for a ref object. */
private bool
refs_set_reloc(obj_header_t *hdr, uint reloc, uint size)
{	ref_packed *rp = (ref_packed *)(hdr + 1);
	ref_packed *end = (ref_packed *)((byte *)rp + size);
	uint freed = 0;
	/*
	 * We have to be careful to keep refs aligned properly.
	 * For the moment, we do this by either keeping or discarding
	 * an entire (aligned) block of align_packed_per_ref packed elements
	 * as a unit.  We know that align_packed_per_ref <= packed_per_ref,
	 * and we also know that packed refs are always allocated in blocks
	 * of align_packed_per_ref, so this makes things relatively easy.
	 */
	while ( rp < end )
	{	if ( r_is_packed(rp) )
		{
#if align_packed_per_ref == 1
			if ( r_has_pmark(rp) )
			{	if_debug1('8',
					  "  [8]packed ref 0x%lx is marked\n",
					  (ulong)rp);
				rp++;
			}
#else
			uint marked = *rp;
			int i;
			for ( i = 1; i < align_packed_per_ref; i++ )
				marked |= rp[i];
			if ( marked & lp_mark )
			{	/* At least one packed_ref in the block */
				/* is marked: Keep the whole block. */
				for ( ; i; i--, rp++ )
				{	r_set_pmark(rp);
					if_debug1('8',
					  "  [8]packed ref 0x%lx is marked\n",
					  (ulong)rp);
				}
			}
#endif
			else
			{	if_debug2('8', "  [8]%d packed ref(s) at 0x%lx are unmarked\n",
					  align_packed_per_ref, (ulong)rp);
				rp += align_packed_per_ref;
				freed += sizeof(ref_packed) * align_packed_per_ref;
			}
		}
		else			/* full-size ref */
		{	uint rel = reloc + freed;
			/* The following assignment is logically */
			/* unnecessary; we do it only for convenience */
			/* in debugging. */
			ref *pref = (ref *)rp;
			if ( !r_has_attr(pref, l_mark) )
			{	if_debug1('8', "  [8]ref 0x%lx is unmarked\n",
					  (ulong)pref);
				/* Change this to a mark so we can */
				/* store the relocation. */
				r_set_type(pref, t_mark);
				r_set_size(pref, rel);
				freed += sizeof(ref);
			}
			else
			{	uint type;
				if_debug1('8', "  [8]ref 0x%lx is marked\n",
					  (ulong)pref);
				/* Store the relocation here if possible. */
				switch ( type = r_type(pref) )
				  {
				  case_types_using_size:
					break;
				  default:
					if ( type >= t_next_index )
					  break;
					if_debug2('8', "  [8]storing reloc %u at 0x%lx\n",
						  rel, (ulong)pref);
					r_set_size(pref, rel);
				  }
			}
			rp += packed_per_ref;
		}
	}
	if_debug3('7', " [7]at end of refs 0x%lx, size = %u, freed = %u\n",
		  (ulong)(hdr + 1), size, freed);
	if ( freed == size )
	  return false;
#if arch_sizeof_int > arch_sizeof_short
	/*
	 * If the final relocation can't fit in the r_size field
	 * (which can't happen if the object shares a chunk with
	 * any other objects, so we know reloc = 0 in this case),
	 * we have to keep the entire object unless there are no
	 * references to any ref in it.
	 */
	if ( freed <= max_ushort )
	  return true;
	/*
	 * We have to mark all surviving refs, but we also must
	 * overwrite any non-surviving refs with something that
	 * doesn't contain any pointers.
	 */
	rp = (ref_packed *)(hdr + 1);
	while ( rp < end )
	  {	if ( r_is_packed(rp) )
		  {	if ( !r_has_pmark(rp) )
			  *rp = pt_tag(pt_integer) | lp_mark;
			++rp;
		  }
		else
		  {	/* The following assignment is logically */
			/* unnecessary; we do it only for convenience */
			/* in debugging. */
			ref *pref = (ref *)rp;
			if ( !r_has_attr(pref, l_mark) )
			  {	r_set_type_attrs(pref, t_mark, l_mark);
				r_set_size(pref, reloc);
			  }
			else
			  {	uint type;
				switch ( type = r_type(pref) )
				  {
				  case_types_using_size:
					break;
				  default:
					if ( type >= t_next_index )
					  break;
					r_set_size(pref, reloc);
				  }
			  }
			rp += packed_per_ref;
		  }
	  }
	/* The last ref has to remain unmarked. */
	r_clear_attrs((ref *)rp - 1, l_mark);
#endif
	return true;
}

/* ------ Relocation phase ------ */

/* Relocate all the pointers in a block of refs. */
private void
refs_do_reloc(void /*obj_header_t*/ *vptr, uint size, gc_state_t *gcst)
{	gs_reloc_refs((ref_packed *)vptr,
		      (ref_packed *)((char *)vptr + size),
		      gcst);
}
/* Relocate the contents of a block of refs. */
void
gs_reloc_refs(ref_packed *from, ref_packed *to, gc_state_t *gcst)
{	ref_packed *rp = from;
	while ( rp < to )
	{	ref *pref;
		if ( r_is_packed(rp) )
		{	rp++;
			continue;
		}
		/* The following assignment is logically unnecessary; */
		/* we do it only for convenience in debugging. */
		pref = (ref *)rp;
		if_debug3('8', "  [8]relocating %s %d ref at 0x%lx\n",
			  (r_has_attr(pref, l_mark) ? "marked" : "unmarked"),
			  r_btype(pref), (ulong)pref);
		if ( r_has_attr(pref, l_mark) && !r_is_foreign(pref) )
		  switch ( r_type(pref) )
		{
		/* Struct cases */
#define ref_case(v, t)\
  pref->value.v =\
    (t *)gs_reloc_struct_ptr((obj_header_t *)pref->value.v, gcst)
		case t_file:
			ref_case(pfile, struct stream_s); break;
		case t_device:
			ref_case(pdevice, struct gx_device_s); break;
		case t_fontID:
		case t_struct:
		case t_astruct:
			ref_case(pstruct, void); break;
#undef ref_case
		/* Non-trivial non-struct cases */
		case t_dictionary:
			pref->value.pdict =
			  (dict *)gs_reloc_ref_ptr((ref_packed *)pref->value.pdict, gcst);
			break;
		case t_array:
			if ( r_size(pref) != 0 ) /* value.refs might be NULL */
			  pref->value.refs =
			    (ref *)gs_reloc_ref_ptr((ref_packed *)pref->value.refs, gcst);
			break;
		case t_mixedarray: case t_shortarray:
			if ( r_size(pref) != 0 ) /* value.refs might be NULL */
			  pref->value.packed =
			    gs_reloc_ref_ptr(pref->value.packed, gcst);
			break;
		case t_name:
		  {	void *psub = name_ref_sub_table(pref);
			void *rsub = gs_reloc_struct_ptr(psub, gcst);
			pref->value.pname = (name *)
			  ((char *)rsub + ((char *)pref->value.pname - (char *)psub));
		  }	break;
		case t_string:
		  {	gs_string str;
			str.data = pref->value.bytes;
			str.size = r_size(pref);
			gs_reloc_string(&str, gcst);
			pref->value.bytes = str.data;
		  }	break;
		case t_oparray:
			pref->value.const_refs =
			  (const ref *)gs_reloc_ref_ptr((const ref_packed *)pref->value.const_refs, gcst);
			break;
		}
		rp += packed_per_ref;
	}
}

/* Relocate a pointer to a ref. */
/* See gsmemory.h for why the argument is const and the result is not. */
ref_packed *
gs_reloc_ref_ptr(const ref_packed *prp, gc_state_t *ignored)
{	/* Search forward for relocation. */
	/* This algorithm is intrinsically very inefficient; */
	/* we hope eventually to replace it with a better one. */
	register const ref_packed *rp = prp;
	uint dec = 0;

	for ( ; ; )
	{	uint type;
		if ( r_is_packed(rp) )
		{	/* For each unmarked packed ref we pass over, */
			/* we have to decrement the final relocation. */
			if ( r_is_packed(rp + 1) )
			{	/* Almost all packed refs are marked, */
				/* so test both at the same time. */
				if ( !(*rp & rp[1] & lp_mark) )
				{	if ( (*rp | rp[1]) & lp_mark )
						dec += sizeof(ref_packed);
					else
						dec += sizeof(ref_packed) * 2;
				}
				rp += 2;
				continue;
			}
			else if ( !r_has_pmark(rp) )
				dec += sizeof(ref_packed);
			rp++;		/* fall through */
		}
		switch ( type = r_type((const ref *)rp) )
		{
		default:		/* reloc is in r_size */
			if ( type < t_next_index )
			  {	/* These refs might be in a space */
				/* that isn't being compacted.  If so, */
				/* the relocation value here will be zero. */
				return print_reloc(prp, "ref",
					(ref_packed *)
					  (r_size((const ref *)rp) == 0 ? prp :
					   (const ref_packed *)((const char *)prp - r_size((const ref *)rp) + dec)));
			  }
			/* falls through */
		case_types_using_size:
			rp += packed_per_ref;
		}
	}
}

/* ------ Compaction phase ------ */

/* Compact a ref object. */
/* Remove the marks at the same time. */
private void
refs_compact(obj_header_t *pre, obj_header_t *dpre, uint size)
{	ref_packed *dest;
	ref_packed *src;
	ref_packed *end;
	uint new_size;
	src = (ref_packed *)(pre + 1);
	end = (ref_packed *)((byte *)src + size);
	/*
	 * We know that a block of refs always ends with an unmarked
	 * full-size ref, so we only need to check for reaching the end
	 * of the block when we see one of those.
	 */
	if ( dpre == pre )	/* Loop while we don't need to copy. */
	  for ( ; ; )
	{	if ( r_is_packed(src) )
		{	if ( !r_has_pmark(src) )
				break;
			if_debug1('8', "  [8]packed ref 0x%lx \"copied\"\n",
					  (ulong)src);
			*src &= ~lp_mark;
			src++;
		}
		else			/* full-size ref */
		{	if ( !r_has_attr((ref *)src, l_mark) )
				break;
			if_debug1('8', "  [8]ref 0x%lx \"copied\"\n",
					  (ulong)src);
			r_clear_attrs((ref *)src, l_mark);
			src += packed_per_ref;
		}
	}
	else
		*dpre = *pre;
	dest = (ref_packed *)((char *)dpre + ((char *)src - (char *)pre));
	for ( ; ; )
	{	if ( r_is_packed(src) )
		{	if ( r_has_pmark(src) )
			{	if_debug2('8', "  [8]packed ref 0x%lx copied to 0x%lx\n",
					  (ulong)src, (ulong)dest);
				*dest++ = *src & ~lp_mark;
			}
			src++;
		}
		else			/* full-size ref */
		{	if ( r_has_attr((ref *)src, l_mark) )
			{	ref rtemp;
				if_debug2('8', "  [8]ref 0x%lx copied to 0x%lx\n",
					  (ulong)src, (ulong)dest);
				/* We can't just use ref_assign_inline, */
				/* because the source and destination */
				/* might overlap! */
				ref_assign_inline(&rtemp, (ref *)src);
				r_clear_attrs(&rtemp, l_mark);
				ref_assign_inline((ref *)dest, &rtemp);
				dest += packed_per_ref;
				src += packed_per_ref;
			}
			else		/* check for end of block */
			{	src += packed_per_ref;
				if ( src >= end )
					break;
			}
		}
	}
	new_size = (byte *)dest - (byte *)(dpre + 1) + sizeof(ref);
#ifdef DEBUG
	/* Check that the relocation came out OK. */
	/* NOTE: this check only works within a single chunk. */
	if ( (byte *)src - (byte *)dest != r_size((ref *)src - 1) + sizeof(ref) )
	{	lprintf3("Reloc error for refs 0x%lx: reloc = %lu, stored = %u\n",
			 (ulong)dpre, (ulong)((byte *)src - (byte *)dest),
			 (uint)r_size((ref *)src - 1));
		gs_exit(1);
	}
#endif
	/* Pad to a multiple of sizeof(ref). */
	while ( new_size & (sizeof(ref) - 1) )
		*dest++ = pt_tag(pt_integer),
		new_size += sizeof(ref_packed);
	/* We want to make the newly freed space into a free block, */
	/* but we can only do this if we have enough room. */
	if ( size - new_size < sizeof(obj_header_t) )
	  {	/* Not enough room.  Pad to original size. */
		while ( new_size < size )
		  *dest++ = pt_tag(pt_integer),
		  new_size += sizeof(ref_packed);
	  }
	else
	  {	obj_header_t *pfree = (obj_header_t *)((ref *)dest + 1);
		pfree->o_large = 0;
		pfree->o_size = size - new_size - sizeof(obj_header_t);
		pfree->o_type = &st_bytes;
	  }
	/* Re-create the final ref. */
	r_set_type((ref *)dest, t_integer);
	dpre->o_size = new_size;
}
