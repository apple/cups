/* Copyright (C) 1994, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: igcref.c,v 1.2 2000/03/08 23:15:11 mike Exp $ */
/* ref garbage collector for Ghostscript */
#include "memory_.h"
#include "ghost.h"
#include "gsexit.h"
#include "gsstruct.h"		/* for gxalloc.h included by iastate.h */
#include "iname.h"
#include "iastate.h"
#include "idebug.h"
#include "igc.h"
#include "ipacked.h"
#include "store.h"		/* for ref_assign_inline */

/* Define whether to trace every step of relocating ref pointers. */
#if 0
#  define rputc(c) dputc(c)
#else
#  define rputc(c) DO_NOTHING
#endif

/* Forward references */
ptr_proc_reloc(igc_reloc_ref_ptr, ref_packed);
refs_proc_reloc(igc_reloc_refs);

/*
 * Define the 'structure' type descriptor for refs.
 * This is special because it has different shared procs.
 */
private gc_proc_clear_reloc(refs_clear_reloc);
private gc_proc_set_reloc(refs_set_reloc);
private gc_proc_compact(refs_compact);
private const struct_shared_procs_t refs_shared_procs =
{refs_clear_reloc, refs_set_reloc, refs_compact};
private struct_proc_clear_marks(refs_clear_marks);
private struct_proc_reloc_ptrs(refs_do_reloc);
const gs_memory_struct_type_t st_refs =
{sizeof(ref), "refs", &refs_shared_procs, refs_clear_marks, 0, refs_do_reloc};

/*
 * Define the GC procedures for structs that actually contain refs.
 * These are special because the shared refs_* procedures
 * are never called.  Instead, we unmark the individual refs in clear_marks,
 * disregard refs_*_reloc (because we will never relocate a ptr_ref_type
 * pointer pointing into the structure), disregard refs_compact (because
 * compaction is never required), and remove the marks in reloc_ptrs.
 * See also the comment about ptr_ref_type in imemory.h.
 */
CLEAR_MARKS_PROC(ref_struct_clear_marks)
{
    ref *pref = (ref *) vptr;
    ref *end = (ref *) ((char *)vptr + size);

    for (; pref < end; pref++)
	r_clear_attrs(pref, l_mark);
}
ENUM_PTRS_BEGIN_PROC(ref_struct_enum_ptrs)
{
    if (index >= size / sizeof(ref))
	return 0;
    *pep = (ref *) vptr + index;
    return ptr_ref_type;
    ENUM_PTRS_END_PROC
}
RELOC_PTRS_BEGIN(ref_struct_reloc_ptrs)
{
    ref *beg = vptr;
    ref *end = (ref *) ((char *)vptr + size);

    igc_reloc_refs((ref_packed *) beg, (ref_packed *) end, gcst);
    ref_struct_clear_marks(vptr, size, pstype);
} RELOC_PTRS_END

/* ------ Unmarking phase ------ */

/* Unmark a single ref. */
void
ptr_ref_unmark(void *vptr, gc_state_t * ignored)
{
    if (r_is_packed((ref *) vptr))
	r_clear_pmark((ref_packed *) vptr);
    else
	r_clear_attrs((ref *) vptr, l_mark);
}

/* Unmarking routine for ref objects. */
private void
refs_clear_marks(void /*obj_header_t */ *vptr, uint size,
		 const gs_memory_struct_type_t * pstype)
{
    ref_packed *rp = (ref_packed *) vptr;
    ref_packed *end = (ref_packed *) ((byte *) vptr + size);

    /* Since the last ref is full-size, we only need to check for */
    /* the end of the block when we see one of those. */
    for (;;) {
	if (r_is_packed(rp)) {
#ifdef DEBUG
	    if (gs_debug_c('8')) {
		dlprintf1("  [8]unmark packed 0x%lx ", (ulong) rp);
		debug_print_ref((const ref *)rp);
		dputs("\n");
	    }
#endif
	    r_clear_pmark(rp);
	    rp++;
	} else {		/* full-size ref */
#ifdef DEBUG
	    if (gs_debug_c('8')) {
		dlprintf1("  [8]unmark ref 0x%lx ", (ulong) rp);
		debug_print_ref((ref *) rp);
		dputs("\n");
	    }
#endif
	    r_clear_attrs((ref *) rp, l_mark);
	    rp += packed_per_ref;
	    if (rp >= (ref_packed *) end)
		break;
	}
    }
}

/* ------ Marking phase ------ */

/* Mark a ref.  Return true if new mark. */
bool
ptr_ref_mark(void *vptr, gc_state_t * ignored)
{
    if (r_is_packed(vptr)) {
	if (r_has_pmark((ref_packed *) vptr))
	    return false;
	r_set_pmark((ref_packed *) vptr);
    } else {
	if (r_has_attr((ref *) vptr, l_mark))
	    return false;
	r_set_attrs((ref *) vptr, l_mark);
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

/* Clear the relocation for a ref object. */
private void
refs_clear_reloc(obj_header_t * hdr, uint size)
{
    register ref_packed *rp = (ref_packed *) (hdr + 1);
    ref_packed *end = (ref_packed *) ((byte *) rp + size);

    while (rp < end) {
	if (r_is_packed(rp))
	    rp++;
	else {			/* full-size ref *//* Store the relocation here if possible. */
	    if (!ref_type_uses_size_or_null(r_type((ref *) rp))) {
		if_debug1('8',
			  "  [8]clearing reloc at 0x%lx\n",
			  (ulong) rp);
		r_set_size((ref *) rp, 0);
	    }
	    rp += packed_per_ref;
	}
    }
}

/* Set the relocation for a ref object. */
private bool
refs_set_reloc(obj_header_t * hdr, uint reloc, uint size)
{
    ref_packed *rp = (ref_packed *) (hdr + 1);
    ref_packed *end = (ref_packed *) ((byte *) rp + size);
    uint freed = 0;

    /*
     * We have to be careful to keep refs aligned properly.
     * For the moment, we do this by either keeping or discarding
     * an entire (aligned) block of align_packed_per_ref packed elements
     * as a unit.  We know that align_packed_per_ref <= packed_per_ref,
     * and we also know that packed refs are always allocated in blocks
     * of align_packed_per_ref, so this makes things relatively easy.
     */
    while (rp < end) {
	if (r_is_packed(rp)) {
#if align_packed_per_ref == 1
	    if (r_has_pmark(rp)) {
		if_debug1('8',
			  "  [8]packed ref 0x%lx is marked\n",
			  (ulong) rp);
		rp++;
	    } else {
#else
	    int i;

	    /*
	     * Note: align_packed_per_ref is typically
	     * 2 or 4 for 32-bit processors.
	     */
#define all_marked (align_packed_per_ref * lp_mark)
# if align_packed_per_ref == 2
#  if arch_sizeof_long == arch_sizeof_short * 2
#    undef all_marked
#    define all_marked ( ((long)lp_mark << (sizeof(short) * 8)) + lp_mark )
#    define marked (*(long *)rp & all_marked)
#  else
#    define marked ((*rp & lp_mark) + (rp[1] & lp_mark))
#  endif
# else
#  if align_packed_per_ref == 4
#    define marked ((*rp & lp_mark) + (rp[1] & lp_mark) +\
		    (rp[2] & lp_mark) + (rp[3] & lp_mark))
#  else
	    uint marked = *rp & lp_mark;

	    for (i = 1; i < align_packed_per_ref; i++)
		marked += rp[i] & lp_mark;
#  endif
# endif
	    /*
	     * Now marked is lp_mark * the number of marked
	     * packed refs in the aligned block, except for
	     * a couple of special cases above.
	     */
	    switch (marked) {
		case all_marked:
		    if_debug2('8',
			      "  [8]packed refs 0x%lx..0x%lx are marked\n",
			      (ulong) rp,
			      (ulong) (rp + (align_packed_per_ref - 1)));
		    rp += align_packed_per_ref;
		    break;
		default:
		    /* At least one packed ref in the block */
		    /* is marked: Keep the whole block. */
		    for (i = align_packed_per_ref; i--; rp++) {
			r_set_pmark(rp);
			if_debug1('8',
				  "  [8]packed ref 0x%lx is marked\n",
				  (ulong) rp);
		    }
		    break;
		case 0:
#endif
		    if_debug2('8', "  [8]%d packed ref(s) at 0x%lx are unmarked\n",
			      align_packed_per_ref, (ulong) rp);
		    {
			uint rel = reloc + freed;

			/* Change this to an integer so we can */
			/* store the relocation here. */
			*rp = pt_tag(pt_integer) +
			    min(rel, packed_max_value);
		    }
		    rp += align_packed_per_ref;
		    freed += sizeof(ref_packed) * align_packed_per_ref;
	    }
	} else {		/* full-size ref */
	    uint rel = reloc + freed;

	    /* The following assignment is logically */
	    /* unnecessary; we do it only for convenience */
	    /* in debugging. */
	    ref *pref = (ref *) rp;

	    if (!r_has_attr(pref, l_mark)) {
		if_debug1('8', "  [8]ref 0x%lx is unmarked\n",
			  (ulong) pref);
		/* Change this to a mark so we can */
		/* store the relocation. */
		r_set_type(pref, t_mark);
		r_set_size(pref, rel);
		freed += sizeof(ref);
	    } else {
		if_debug1('8', "  [8]ref 0x%lx is marked\n",
			  (ulong) pref);
		/* Store the relocation here if possible. */
		if (!ref_type_uses_size_or_null(r_type(pref))) {
		    if_debug2('8', "  [8]storing reloc %u at 0x%lx\n",
			      rel, (ulong) pref);
		    r_set_size(pref, rel);
		}
	    }
	    rp += packed_per_ref;
	}
    }
    if_debug3('7', " [7]at end of refs 0x%lx, size = %u, freed = %u\n",
	      (ulong) (hdr + 1), size, freed);
    if (freed == size)
	return false;
#if arch_sizeof_int > arch_sizeof_short
    /*
     * If the final relocation can't fit in the r_size field
     * (which can't happen if the object shares a chunk with
     * any other objects, so we know reloc = 0 in this case),
     * we have to keep the entire object unless there are no
     * references to any ref in it.
     */
    if (freed <= max_ushort)
	return true;
    /*
     * We have to mark all surviving refs, but we also must
     * overwrite any non-surviving refs with something that
     * doesn't contain any pointers.
     */
    rp = (ref_packed *) (hdr + 1);
    while (rp < end) {
	if (r_is_packed(rp)) {
	    if (!r_has_pmark(rp))
		*rp = pt_tag(pt_integer) | lp_mark;
	    ++rp;
	} else {		/* The following assignment is logically */
	    /* unnecessary; we do it only for convenience */
	    /* in debugging. */
	    ref *pref = (ref *) rp;

	    if (!r_has_attr(pref, l_mark)) {
		r_set_type_attrs(pref, t_mark, l_mark);
		r_set_size(pref, reloc);
	    } else {
		if (!ref_type_uses_size_or_null(r_type(pref)))
		    r_set_size(pref, reloc);
	    }
	    rp += packed_per_ref;
	}
    }
    /* The last ref has to remain unmarked. */
    r_clear_attrs((ref *) rp - 1, l_mark);
#endif
    return true;
}

/* ------ Relocation phase ------ */

/* Relocate all the pointers in a block of refs. */
private void
refs_do_reloc(void /*obj_header_t */ *vptr, uint size,
	      const gs_memory_struct_type_t * pstype, gc_state_t * gcst)
{
    igc_reloc_refs((ref_packed *) vptr,
		   (ref_packed *) ((char *)vptr + size),
		   gcst);
}
/* Relocate the contents of a block of refs. */
/* If gcst->relocating_untraced is true, we are relocating pointers from an */
/* untraced space, so relocate all refs, not just marked ones. */
void
igc_reloc_refs(ref_packed * from, ref_packed * to, gc_state_t * gcst)
{
    int min_trace = gcst->min_collect;
    ref_packed *rp = from;
    bool do_all = gcst->relocating_untraced;

    while (rp < to) {
	ref *pref;

	if (r_is_packed(rp)) {
	    rp++;
	    continue;
	}
	/* The following assignment is logically unnecessary; */
	/* we do it only for convenience in debugging. */
	pref = (ref *) rp;
	if_debug3('8', "  [8]relocating %s %d ref at 0x%lx\n",
		  (r_has_attr(pref, l_mark) ? "marked" : "unmarked"),
		  r_btype(pref), (ulong) pref);
	if ((r_has_attr(pref, l_mark) || do_all) &&
	    r_space(pref) >= min_trace
	    )
	    switch (r_type(pref)) {
		    /* Struct cases */
		case t_file:
		    RELOC_VAR(pref->value.pfile);
		    break;
		case t_device:
		    RELOC_VAR(pref->value.pdevice);
		    break;
		case t_fontID:
		case t_struct:
		case t_astruct:
		    RELOC_VAR(pref->value.pstruct);
		    break;
		    /* Non-trivial non-struct cases */
		case t_dictionary:
		    rputc('d');
		    pref->value.pdict =
			(dict *) igc_reloc_ref_ptr((ref_packed *) pref->value.pdict, gcst);
		    break;
		case t_array:
		    {
			uint size = r_size(pref);

			if (size != 0) {	/* value.refs might be NULL *//*
						 * If the array is large, we allocated it in its
						 * own object (at least originally -- this might
						 * be a pointer to a subarray.)  In this case,
						 * we know it is the only object in its
						 * containing st_refs object, so we know that
						 * the mark containing the relocation appears
						 * just after it.
						 */
			    if (size < max_size_st_refs / sizeof(ref)) {
				rputc('a');
				pref->value.refs =
				    (ref *) igc_reloc_ref_ptr(
				     (ref_packed *) pref->value.refs, gcst);
			    } else {
				rputc('A');
				/*
				 * See the t_shortarray case below for why we
				 * decrement size.
				 */
				--size;
				pref->value.refs =
				    (ref *) igc_reloc_ref_ptr(
				   (ref_packed *) (pref->value.refs + size),
							       gcst) - size;
			    }
			}
		    }
		    break;
		case t_mixedarray:
		    if (r_size(pref) != 0) {	/* value.refs might be NULL */
			rputc('m');
			pref->value.packed =
			    igc_reloc_ref_ptr(pref->value.packed, gcst);
		    }
		    break;
		case t_shortarray:
		    {
			uint size = r_size(pref);

			/*
			 * Since we know that igc_reloc_ref_ptr works by
			 * scanning forward, and we know that all the
			 * elements of this array itself are marked, we can
			 * save some scanning time by relocating the pointer
			 * to the end of the array rather than the
			 * beginning.
			 */
			if (size != 0) {	/* value.refs might be NULL */
			    rputc('s');
			    /*
			     * igc_reloc_ref_ptr has to be able to determine
			     * whether the pointer points into a space that
			     * isn't being collected.  It does this by
			     * checking whether the referent of the pointer
			     * is marked.  For this reason, we have to pass
			     * a pointer to the last real element of the
			     * array, rather than just beyond it.
			     */
			    --size;
			    pref->value.packed =
				igc_reloc_ref_ptr(pref->value.packed + size,
						  gcst) - size;
			}
		    }
		    break;
		case t_name:
		    {
			void *psub = name_ref_sub_table(pref);
			void *rsub = RELOC_OBJ(psub); /* gcst implicit */

			pref->value.pname = (name *)
			    ((char *)rsub + ((char *)pref->value.pname - (char *)psub));
		    } break;
		case t_string:
		    {
			gs_string str;

			str.data = pref->value.bytes;
			str.size = r_size(pref);

			RELOC_STRING_VAR(str);
			pref->value.bytes = str.data;
		    }
		    break;
		case t_oparray:
		    rputc('o');
		    pref->value.const_refs =
			(const ref *)igc_reloc_ref_ptr((const ref_packed *)pref->value.const_refs, gcst);
		    break;
	    }
	rp += packed_per_ref;
    }
}

/* Relocate a pointer to a ref. */
/* See gsmemory.h for why the argument is const and the result is not. */
ref_packed *
igc_reloc_ref_ptr(const ref_packed * prp, gc_state_t * ignored)
{				/*
				 * Search forward for relocation.  This algorithm is intrinsically
				 * very inefficient; we hope eventually to replace it with a better
				 * one.
				 */
    register const ref_packed *rp = prp;
    uint dec = 0;

    /*
     * Iff this pointer points into a space that wasn't traced,
     * the referent won't be marked.  In this case, we shouldn't
     * do any relocation.  Check for this first.
     */
    if (r_is_packed(rp)) {
	if (!r_has_pmark(rp))
	    return (ref_packed *) rp;
    } else {
	if (!r_has_attr((const ref *)rp, l_mark))
	    return (ref_packed *) rp;
    }
    for (;;) {
	if (r_is_packed(rp)) {	/*
				 * Normally, an unmarked packed ref will be an
				 * integer whose value is the amount of relocation.
				 * However, the relocation value might have been
				 * too large to fit.  If this is the case, for
				 * each such unmarked packed ref we pass over,
				 * we have to decrement the final relocation.
				 */
	    rputc((*rp & lp_mark ? '1' : '0'));
	    if (!(*rp & lp_mark)) {
		if (*rp != pt_tag(pt_integer) + packed_max_value) {	/* This is a stored relocation value. */
		    rputc('\n');
		    return print_reloc(prp, "ref",
				       (ref_packed *)
				       ((const char *)prp -
					(*rp & packed_value_mask) + dec));
		}
		/*
		 * We know this is the first of an aligned block
		 * of packed refs.  Skip over the entire block,
		 * decrementing the final relocation.
		 */
		dec += sizeof(ref_packed) * align_packed_per_ref;
		rp += align_packed_per_ref;
	    } else
		rp++;
	    continue;
	}
	if (!ref_type_uses_size_or_null(r_type((const ref *)rp))) {	/* reloc is in r_size */
	    rputc('\n');
	    return print_reloc(prp, "ref",
			       (ref_packed *)
			       (r_size((const ref *)rp) == 0 ? prp :
				(const ref_packed *)((const char *)prp - r_size((const ref *)rp) + dec)));
	}
	rputc('u');
	rp += packed_per_ref;
    }
}

/* ------ Compaction phase ------ */

/* Compact a ref object. */
/* Remove the marks at the same time. */
private void
refs_compact(obj_header_t * pre, obj_header_t * dpre, uint size)
{
    ref_packed *dest;
    ref_packed *src;
    ref_packed *end;
    uint new_size;

    src = (ref_packed *) (pre + 1);
    end = (ref_packed *) ((byte *) src + size);
    /*
     * We know that a block of refs always ends with an unmarked
     * full-size ref, so we only need to check for reaching the end
     * of the block when we see one of those.
     */
    if (dpre == pre)		/* Loop while we don't need to copy. */
	for (;;) {
	    if (r_is_packed(src)) {
		if (!r_has_pmark(src))
		    break;
		if_debug1('8', "  [8]packed ref 0x%lx \"copied\"\n",
			  (ulong) src);
		*src &= ~lp_mark;
		src++;
	    } else {		/* full-size ref */
		if (!r_has_attr((ref *) src, l_mark))
		    break;
		if_debug1('8', "  [8]ref 0x%lx \"copied\"\n",
			  (ulong) src);
		r_clear_attrs((ref *) src, l_mark);
		src += packed_per_ref;
	    }
    } else
	*dpre = *pre;
    dest = (ref_packed *) ((char *)dpre + ((char *)src - (char *)pre));
    for (;;) {
	if (r_is_packed(src)) {
	    if (r_has_pmark(src)) {
		if_debug2('8', "  [8]packed ref 0x%lx copied to 0x%lx\n",
			  (ulong) src, (ulong) dest);
		*dest++ = *src & ~lp_mark;
	    }
	    src++;
	} else {		/* full-size ref */
	    if (r_has_attr((ref *) src, l_mark)) {
		ref rtemp;

		if_debug2('8', "  [8]ref 0x%lx copied to 0x%lx\n",
			  (ulong) src, (ulong) dest);
		/* We can't just use ref_assign_inline, */
		/* because the source and destination */
		/* might overlap! */
		ref_assign_inline(&rtemp, (ref *) src);
		r_clear_attrs(&rtemp, l_mark);
		ref_assign_inline((ref *) dest, &rtemp);
		dest += packed_per_ref;
		src += packed_per_ref;
	    } else {		/* check for end of block */
		src += packed_per_ref;
		if (src >= end)
		    break;
	    }
	}
    }
    new_size = (byte *) dest - (byte *) (dpre + 1) + sizeof(ref);
#ifdef DEBUG
    /* Check that the relocation came out OK. */
    /* NOTE: this check only works within a single chunk. */
    if ((byte *) src - (byte *) dest != r_size((ref *) src - 1) + sizeof(ref)) {
	lprintf3("Reloc error for refs 0x%lx: reloc = %lu, stored = %u\n",
		 (ulong) dpre, (ulong) ((byte *) src - (byte *) dest),
		 (uint) r_size((ref *) src - 1));
	gs_exit(1);
    }
#endif
    /* Pad to a multiple of sizeof(ref). */
    while (new_size & (sizeof(ref) - 1))
	*dest++ = pt_tag(pt_integer),
	    new_size += sizeof(ref_packed);
    /* We want to make the newly freed space into a free block, */
    /* but we can only do this if we have enough room. */
    if (size - new_size < sizeof(obj_header_t)) {	/* Not enough room.  Pad to original size. */
	while (new_size < size)
	    *dest++ = pt_tag(pt_integer),
		new_size += sizeof(ref_packed);
    } else {
	obj_header_t *pfree = (obj_header_t *) ((ref *) dest + 1);

	pfree->o_large = 0;
	pfree->o_size = size - new_size - sizeof(obj_header_t);
	pfree->o_type = &st_bytes;
    }
    /* Re-create the final ref. */
    r_set_type((ref *) dest, t_integer);
    dpre->o_size = new_size;
}
