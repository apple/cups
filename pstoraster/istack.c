/* Copyright (C) 1992, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* istack.c */
/* Ghostscript expandable stack manager */
#include "memory_.h"
#include "ghost.h"
#include "gsstruct.h"
#include "gsutil.h"
#include "errors.h"
#include "ialloc.h"
#include "istack.h"
#include "istruct.h"		/* for gs_reloc_refs */
#include "iutil.h"
#include "ivmspace.h"		/* for local/global test */
#include "store.h"

/* Forward references */
private void init_block(P3(ref_stack *, ref *, uint));
int ref_stack_push_block(P3(ref_stack *, uint, uint));

/* GC procedures */
#define sptr ((ref_stack *)vptr)
private CLEAR_MARKS_PROC(ref_stack_clear_marks) {
	r_clear_attrs(&sptr->current, l_mark);
}
private ENUM_PTRS_BEGIN(ref_stack_enum_ptrs) return 0;
	case 0:
		*pep = &sptr->current;
		return ptr_ref_type;
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(ref_stack_reloc_ptrs) {
#if stacks_are_segmented
	/* In a segmented environment, the top block can't move, */
	/* so we don't need to relocate pointers to it. */
#else
	/* Note that the relocation must be a multiple of sizeof(ref_packed) */
	/* * align_packed_per_ref, but it need not be a multiple of */
	/* sizeof(ref).  Therefore, we must do the adjustments using */
	/* ref_packed pointers rather than ref pointers. */
	ref_packed *bot = (ref_packed *)sptr->current.value.refs;
	long reloc;
	gs_reloc_refs((ref_packed *)&sptr->current,
		      (ref_packed *)(&sptr->current + 1),
		      gcst);
	r_clear_attrs(&sptr->current, l_mark);
	reloc = bot - (ref_packed *)sptr->current.value.refs;
#define reloc_p(p)\
  sptr->p = (ref *)((ref_packed *)sptr->p - reloc);
	reloc_p(p);
	reloc_p(bot);
	reloc_p(top);
#undef reloc_p
#endif
} RELOC_PTRS_END
/* Structure type for a ref_stack. */
public_st_ref_stack();

/* Initialize a stack. */
void
ref_stack_init(register ref_stack *pstack, ref *psb,
  uint bot_guard, uint top_guard, ref *pguard, gs_ref_memory_t *mem)
{	uint size = r_size(psb);
	uint avail = size - (stack_block_refs + bot_guard + top_guard);
	ref_stack_block *pblock = (ref_stack_block *)psb->value.refs;
	s_ptr body = (s_ptr)(pblock + 1);
	pstack->bot = body + bot_guard;
	pstack->p = pstack->bot - 1;
	pstack->top = pstack->p + avail;
	pstack->current = *psb;
	pstack->extension_size = 0;
	pstack->extension_used = 0;

	make_int(&pstack->max_stack, avail);
	pstack->requested = 0;

	pstack->bot_guard = bot_guard;
	pstack->top_guard = top_guard;
	pstack->block_size = size - segmented_guard(bot_guard + top_guard);
	pstack->body_size = avail;
	if ( pguard != 0 )
	  pstack->guard_value = *pguard;
	else
	  make_tav(&pstack->guard_value, t__invalid, 0, intval, 0);
	pstack->underflow_error = -1;		/* bogus, caller must set */
	pstack->overflow_error = -1;		/* bogus, caller must set */
	pstack->allow_expansion = true;		/* default, caller may reset */
	pstack->memory = mem;
	init_block(pstack, psb, 0);
	refset_null(pstack->bot, avail);
	make_empty_array(&pblock->next, 0);
}

/* Set the maximum number of elements allowed on a stack. */
int
ref_stack_set_max_count(ref_stack *pstack, long nmax)
{	uint nmin = pstack->extension_size + (pstack->top - pstack->bot + 1);
	if ( nmax < nmin )
	  nmax = nmin;
	if ( nmax > max_uint / sizeof(ref) )
	  nmax = max_uint / sizeof(ref);
	if ( !pstack->allow_expansion )
	  {	uint ncur = pstack->body_size;
		if ( nmax > ncur )
		  nmax = ncur;
	  }
	pstack->max_stack.value.intval = nmax;
	return 0;
}

/* Return the number of elements on a stack. */
uint
ref_stack_count(const ref_stack *pstack)
{	return pstack->extension_used + (pstack->p - pstack->bot + 1);
}

/* Retrieve a given element from the stack, counting from */
/* 0 as the top element. */
ref *
ref_stack_index(const ref_stack *pstack, long idx)
{	ref_stack_block *pblock;
	uint used = pstack->p + 1 - pstack->bot;
	if ( idx < 0 )
		return NULL;
	if ( idx < used )		/* common case */
		return pstack->p - (uint)idx;
	pblock = (ref_stack_block *)pstack->current.value.refs;
	do
	{	pblock = (ref_stack_block *)pblock->next.value.refs;
		if ( pblock == 0 )
			return NULL;
		idx -= used;
		used = r_size(&pblock->used);
	}
	while ( idx >= used );
	return pblock->used.value.refs + (used - 1 - (uint)idx);
}

/* Count the number of elements down to and including the first mark. */
/* If no mark is found, return 0. */
uint
ref_stack_counttomark(const ref_stack *pstack)
{	uint scanned = 0;
	STACK_LOOP_BEGIN(pstack, p, used)
	{	uint count = used;
		p += used - 1;
		for ( ; count; count--, p-- )
		  if ( r_has_type(p, t_mark) )
			return scanned + (used - count + 1);
		scanned += used;
	}
	STACK_LOOP_END(p, used)
	return 0;
}

/* Store the top elements of a stack into an array, */
/* with or without store/undo checking. */
/* May return e_rangecheck or e_invalidaccess. */
int
ref_stack_store(const ref_stack *pstack, ref *parray, uint count, uint skip,
  int age, bool check, client_name_t cname)
{	uint left, pass;
	ref *to;
	uint space = r_space(parray);
	if ( count > ref_stack_count(pstack) || count > r_size(parray) )
	  return_error(e_rangecheck);
	if ( check && space != avm_local )
	  {	/* Pre-check the elements being stored. */
		left = count, pass = skip;
		STACK_LOOP_BEGIN(pstack, ptr, size)
		  if ( size <= pass )
		    pass -= size;
		  else
		    {	int code;
			if ( pass != 0 )
			  size -= pass, pass = 0;
			ptr += size;
			if ( size > left )
			  size = left;
			left -= size;
			code = refs_check_space(ptr - size, size, space);
			if ( code < 0 )
			  return code;
			if ( left == 0 )
			  break;
		    }
		STACK_LOOP_END(ptr, size)
	  }
	to = parray->value.refs + count;
	left = count, pass = skip;
	STACK_LOOP_BEGIN(pstack, from, size)
	  if ( size <= pass )
	    pass -= size;
	  else
	    { if ( pass != 0 )
		size -= pass, pass = 0;
	      from += size;
	      if ( size > left )
		size = left;
	      left -= size;
	      switch ( age )
		{
		case -1:	/* not an array */
		  while ( size-- )
		    { from--, to--;
		      ref_assign(to, from);
		    }
		  break;
		case 0:		/* old array */
		  while ( size-- )
		    { from--, to--;
		      ref_assign_old(parray, to, from, cname);
		    }
		  break;
		case 1:		/* new array */
		  while ( size-- )
		    { from--, to--;
		      ref_assign_new(to, from);
		    }
		  break;
		}
	      if ( left == 0 )
		break;
	    }
	STACK_LOOP_END(from, size)
	r_set_size(parray, count);
	return 0;
}

/* Pop a given number of elements off a stack. */
/* The number must not exceed the number of elements in use. */
void
ref_stack_pop(register ref_stack *pstack, uint count)
{	uint used;
	while ( (used = pstack->p + 1 - pstack->bot) < count )
	{	count -= used;
		pstack->p = pstack->bot - 1;
		ref_stack_pop_block(pstack);
	}
	pstack->p -= count;
}

/* Pop the top block off a stack. */
int
ref_stack_pop_block(register ref_stack *pstack)
{	s_ptr bot = pstack->bot;
	uint count = pstack->p + 1 - bot;
	ref_stack_block *pcur =
	  (ref_stack_block *)pstack->current.value.refs;
	register ref_stack_block *pnext =
	  (ref_stack_block *)pcur->next.value.refs;
	uint used;
	ref *body;
	ref next;
	if ( pnext == 0 )
	  return_error(pstack->underflow_error);
	used = r_size(&pnext->used);
	body = (ref *)(pnext + 1) + flat_guard(pstack->bot_guard);
	next = pcur->next;
	/*
	 * If we're on a segmented system, the top block does not move,
	 * so we move up the used part of the top block, copy the contents
	 * of the next block under it, and free the next block.
	 * We also do this on non-segmented systems if the contents of the
	 * two blocks won't fit in a single block; in this case we copy up
	 * as much as will fit.  On non-segmented systems where the contents
	 * of both blocks fit in a single block, we copy the used part
	 * of the top block to the top of the next block, and free
	 * the top block.
	 */
	if ( used + count > pstack->body_size )
	  {	/* Move as much into the top block as will fit. */
		uint moved = pstack->body_size - count;
		uint left;
		if ( moved == 0 )
		  return_error(e_Fatal);
		memmove(bot + moved, bot, count * sizeof(ref));
		left = used - moved;
		memcpy(bot, body + left, moved * sizeof(ref));
		refset_null(body + left, moved);
		r_dec_size(&pnext->used, moved);
		pstack->p = pstack->top;
		pstack->extension_used -= moved;
	  }
	else
	  {
#if stacks_are_segmented
	/* We know there are no guard elements in the next block. */
	memmove(bot + used, bot, count * sizeof(ref));
	memcpy(bot, body, used * sizeof(ref));
	pcur->next = pnext->next;
	gs_free_ref_array(pstack->memory, &next, "ref_stack_pop_block");
#else
	memcpy(body + used, bot, count * sizeof(ref));
	pstack->bot = bot = body;
	pstack->top = bot + pstack->body_size - 1;
	gs_free_ref_array(pstack->memory, &pstack->current,
			  "ref_stack_pop_block");
	pstack->current = next;
#endif
	pstack->p = bot + (used + count - 1);
	pstack->extension_size -= pstack->body_size;
	pstack->extension_used -= used;
	  }
	return 0;
}

/* Extend a stack to recover from an overflow condition. */
/* May return overflow_error or e_VMerror. */
int
ref_stack_extend(ref_stack *pstack, uint request)
{	uint keep = (pstack->top - pstack->bot + 1) / 3;
	uint count = pstack->p - pstack->bot + 1;

	if ( pstack->p < pstack->bot )
	  {	/* Adding another block can't help things. */
		return_error(pstack->overflow_error);
	  }
	if ( keep + request > pstack->body_size )
	  keep = pstack->body_size - request;
	if ( keep > count )
	  keep = count;		/* required by ref_stack_push_block */
	return ref_stack_push_block(pstack, keep, request);
}

/* Push N empty slots onto a stack.  These slots are not initialized; */
/* the caller must fill them immediately.  May return overflow_error */
/* (if max_stack would be exceeded, or the stack has no allocator) */
/* or e_VMerror. */
int
ref_stack_push(register ref_stack *pstack, uint count)
{	/* Don't bother to pre-check for overflow: we must be able to */
	/* back out in the case of a VMerror anyway, and */
	/* ref_stack_push_block will make the check itself. */
	uint needed = count;
	uint added;
	for ( ; (added = pstack->top - pstack->p) < needed; needed -= added )
	  {	int code;
		pstack->p = pstack->top;
		code =
		  ref_stack_push_block(pstack,
				       (pstack->top - pstack->bot + 1) / 3,
				       count);
		if ( code < 0 )
		  {	/* Back out. */
			ref_stack_pop(pstack, count - needed);
			pstack->requested = count;
			return code;
		  }
	  }
	pstack->p += needed;
	return 0;
}

/* Push a block onto the stack, specifying how many elements of */
/* the current top block should remain in the top block and also */
/* how many elements we are trying to add. */
/* May return overflow_error or e_VMerror. */
/* Must have keep <= count. */
int
ref_stack_push_block(register ref_stack *pstack, uint keep, uint add)
{	uint count = pstack->p - pstack->bot + 1;
	uint move = count - keep;
	ref_stack_block *pcur = (ref_stack_block *)pstack->current.value.refs;
	ref next;
	ref_stack_block *pnext;
	ref *body;
	int code;
	if ( keep > count )
	  return_error(e_Fatal);
	/* Check for overflowing the maximum size, */
	/* or expansion not allowed.  */
	if ( pstack->memory == 0 ||
	     pstack->extension_used + (pstack->top - pstack->bot) + add >=
	       pstack->max_stack.value.intval ||
	     !pstack->allow_expansion
	   )
	  return_error(pstack->overflow_error);
	code = gs_alloc_ref_array(pstack->memory, &next, 0,
				  pstack->block_size, "ref_stack_push_block");
	if ( code < 0 )
	  return code;
	pnext = (ref_stack_block *)next.value.refs;
	body = (ref *)(pnext + 1);
#if stacks_are_segmented
	/* Copy all but the top keep elements into the new block, */
	/* and move the top elements down. */
	/* We know there are no guard elements in the new block. */
	memcpy(body, pstack->bot, move * sizeof(ref));
	/* Clear the elements above the top of the new block. */
	refset_null(body + move, pstack->body_size - move);
	if ( keep <= move )
	{	/* No overlap, memcpy is safe. */
		memcpy(pstack->bot, pstack->bot + move, keep * sizeof(ref));
	}
	else
	{	uint i;
		s_ptr bot = pstack->bot;
		s_ptr up = bot + move;
		for ( i = 0; i < keep; i++ )
			bot[i] = up[i];
	}
	pnext->next = pcur->next;
	pnext->used = next;
	pcur->next = next;
	pnext->used.value.refs = body;
	r_set_size(&pnext->used, move);
#else
	/* Copy the top keep elements into the new block, */
	/* and make the new block the top block. */
	init_block(pstack, &next, keep);
	body += pstack->bot_guard;
	memcpy(body, pstack->bot + move, keep * sizeof(ref));
	/* Clear the elements above the top of the new block. */
	refset_null(body + keep, pstack->body_size - keep);
	/* Clear the elements above the top of the old block. */
	refset_null(pstack->bot + move, keep);
	pnext->next = pstack->current;
	pcur->used.value.refs = pstack->bot;
	r_set_size(&pcur->used, move);
	pstack->current = next;
	pstack->bot = body;
	pstack->top = pstack->bot + pstack->body_size - 1;
#endif
	pstack->p = pstack->bot + keep - 1;
	pstack->extension_size += pstack->body_size;
	pstack->extension_used += move;
	return 0;
}

/* Clean up a stack for garbage collection. */
void
ref_stack_cleanup(ref_stack *pstack)
{	ref_stack_block *pblock =
	  (ref_stack_block *)pstack->current.value.refs;
	refset_null(pstack->p + 1, pstack->top - pstack->p);
	pblock->used = pstack->current;		/* set attrs */
	pblock->used.value.refs = pstack->bot;
	r_set_size(&pblock->used, pstack->p + 1 - pstack->bot);
}

/* ------ Internal routines ------ */

/* Initialize the guards and body of a stack block. */
/* Note that this always initializes the guards, so it should not be used */
/* for extension blocks in a segmented environments. */
private void
init_block(ref_stack *pstack, ref *psb, uint used)
{	ref *brefs = psb->value.refs;
#define pblock ((ref_stack_block *)brefs)
	register uint i;
	register ref *p;
	for ( i = pstack->bot_guard, p = brefs + stack_block_refs;
	      i != 0; i--, p++
	    )
		ref_assign(p, &pstack->guard_value);
	/* The top guard elements will never be read, */
	/* but we need to initialize them for the sake of the GC. */
	/* We can use refset_null for this, because even though it uses */
	/* make_null_new and stack elements must not be marked new, */
	/* these slots will never actually be read or written. */
	if ( pstack->top_guard )
	  {	ref *top = brefs + r_size(psb);
		int top_guard = pstack->top_guard;
		refset_null(top - top_guard, top_guard);
	  }
	pblock->used = *psb;
	pblock->used.value.refs = brefs + stack_block_refs + pstack->bot_guard;
	r_set_size(&pblock->used, 0);
}
