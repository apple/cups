/* Copyright (C) 1992, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* istack.h */
/* Definitions for expandable Ghostscript stacks */
/* Requires iref.h */

#ifndef istack_INCLUDED
#  define istack_INCLUDED

/* Define an opaque allocator type. */
#ifndef gs_ref_memory_DEFINED
#  define gs_ref_memory_DEFINED
typedef struct gs_ref_memory_s gs_ref_memory_t;
#endif

/*
 * The 3 principal Ghostscript stacks (operand, execution, and dictionary)
 * are implemented as a linked list of blocks.  On segmented MS-DOS and
 * MS Windows systems, where there is a substantial performance advantage
 * to keeping the top of the stack in the primary data segment, the top
 * block is stored there, and copied to and from blocks in the heap;
 * on systems with flat address spaces, the top block is stored in the heap
 * like other blocks.  Note that in environments with multiple PostScript
 * contexts, the MS-DOS approach requires some combination of keeping
 * multiple top blocks in the primary data segment, and copying top blocks
 * to and from the heap when switching contexts.
 *
 * Since all operators exit cleanly in case of stack under- or overflow,
 * we handle all issues related to stack blocks in the top-level error
 * recovery code in interp.c.  A few situations require special treatment:
 * see ostack.h, estack.h, and dstack.h for details.
 */

#define stacks_are_segmented arch_ptrs_are_segmented
#if stacks_are_segmented
#  define flat_guard(n) 0
#  define segmented_guard(n) (n)
#else
#  define flat_guard(n) (n)
#  define segmented_guard(n) 0
#endif

typedef ref _ds *s_ptr;
typedef const ref _ds *const_s_ptr;

/*
 * Define the structure for a stack block.
 * In order to simplify allocation, stack blocks are implemented as
 * t_array objects, with the first few elements used for special purposes.
 * The actual layout is as follows:
 *		ref_stack_block structure
 *		bottom guard if any (see below)
 *		used elements of block
 *		unused elements of block
 *		top guard if any (see below)
 * The `next' member of the next higher stack block includes all of this.
 * The `used' member only includes the used elements of this block.
 * Notes:
 *	- In the top block, the size of the `used' member may not be correct.
 *	- In all blocks but the top, we fill the unused elements with nulls.
 */
typedef struct ref_stack_block_s {
	ref next;		/* t_array, next lower block on stack */
	ref used;		/* t_array, subinterval of this block */
		/* Actual stack starts here */
} ref_stack_block;
#define stack_block_refs (sizeof(ref_stack_block) / sizeof(ref))

/*
 * In order to detect under- and overflow with minimum overhead, we may put
 * guard elements at the top and bottom of the stacks (see dstack.h,
 * estack.h, and ostack.h for details of the individual stacks).
 * On segmented systems, we only put guard elements around the top block;
 * on other systems, we put guard elements around every block.  Note that
 * in the latter case, the 'current' and  'next' arrays include the
 * guard elements.
 */

/*
 * The garbage collector requires that the entire contents of every block
 * be 'clean', i.e., contain legitimate refs; we also need to ensure that
 * at GC time, pointers in unused areas of a block will not be followed
 * (since they may be dangling).  We ensure this as follows:
 *	- When allocating a new block, we set the entire body to nulls.
 *	This is necessary because the block may be freed before the next GC,
 *	and the GC must be able to scan (parse) refs even if they are free.
 *	- When adding a new block to the top of the stack, we set to nulls
 *	the unused area of the new next-to-top blocks.
 *	- At the beginning of garbage collection, we set to nulls the unused
 *	elements of the top block.
 */

/*
 * Define the (statically allocated) state of a stack.
 * Note that the total size of a stack cannot exceed max_uint,
 * because it has to be possible to copy a stack to a PostScript array.
 */
#ifndef ref_stack_DEFINED
typedef struct ref_stack_s ref_stack;	/* also defined in idebug.h */
#  define ref_stack_DEFINED
#endif
struct ref_stack_s {
		/* Following are updated dynamically. */
	s_ptr p;			/* current top element */
		/* Following are updated when adding or deleting blocks. */
	s_ptr bot;			/* bottommost valid element */
	s_ptr top;			/* topmost valid element */
	ref current;			/* t_array for current top block */
	uint extension_size;		/* total sizes of extn. blocks */
	uint extension_used;		/* total used sizes of extn. blocks */
		/* Following are updated rarely. */
	ref max_stack;			/* t_integer, Max...Stack user param */
	uint requested;			/* amount of last failing */
					/* push or pop request */
		/* Following are set at initialization. */
	uint bot_guard;			/* # of guard elements below bot */
	uint top_guard;			/* # of guard elements above top */
	uint block_size;		/* size of each block */
	uint body_size;			/* # of data slots in each block */
	ref guard_value;		/* t__invalid or t_operator, */
					/* bottom guard value */
	int underflow_error;		/* error code for underflow */
	int overflow_error;		/* error code for overflow */
	bool allow_expansion;		/* if false, don't expand */
	gs_ref_memory_t *memory;	/* allocator for blocks */
};
#define public_st_ref_stack()	/* in istack.c */\
  gs_public_st_complex_only(st_ref_stack, ref_stack, "ref_stack",\
    ref_stack_clear_marks, ref_stack_enum_ptrs, ref_stack_reloc_ptrs, 0)

/*
 * Define the loop control for enumerating the elements of a stack,
 * as follows:

	STACK_LOOP_BEGIN(pstack, ptr, size)
	...
	STACK_LOOP_END(ptr, size)

 * Each time through the loop, we set ptr to the bottom of a block
 * and size to the size of the block.
 */
#define STACK_LOOP_BEGIN(pstack, ptr, size)\
{ ref_stack_block *pblock_ = (ref_stack_block *)(pstack)->current.value.refs;\
  ref *ptr = (pstack)->bot; uint size = (pstack)->p + 1 - (pstack)->bot;\
  for ( ; ; ) {
#define STACK_LOOP_END(ptr, size)\
    pblock_ = (ref_stack_block *)pblock_->next.value.refs;\
    if ( pblock_ == 0 ) break;\
    ptr = pblock_->used.value.refs; size = r_size(&pblock_->used);\
  }\
}

/* ------ Procedural interface ------ */

/* Initialize a stack.  Note that on segmented systems, */
/* the body of the stack (the elements of the array given by the first ref) */
/* must be in the _ds segment. */
void ref_stack_init(P6(ref_stack *, ref *, uint, uint, ref *,
  gs_ref_memory_t *));

/* Set the maximum number of elements allowed on a stack. */
/* Note that the value is a long, not a uint or a ulong. */
int ref_stack_set_max_count(P2(ref_stack *, long));

/* Return the number of elements on a stack. */
uint ref_stack_count(P1(const ref_stack *));
#define ref_stack_count_inline(pstk)\
  ((pstk)->extension_size == 0 ?\
   (pstk)->p + 1 - (pstk)->bot : ref_stack_count(pstk))

/* Return the maximum number of elements allowed on a stack. */
#define ref_stack_max_count(pstk) (uint)((pstk)->max_stack.value.intval)

/* Return a pointer to a given element from the stack, counting from */
/* 0 as the top element.  If the index is out of range, return 0. */
/* Note that the index is a long, not a uint or a ulong. */
ref *ref_stack_index(P2(const ref_stack *, long));

/* Count the number of elements down to and including the first mark. */
/* If no mark is found, return 0. */
uint ref_stack_counttomark(P1(const ref_stack *));

/* Store N elements of a stack, starting M elements below the top, */
/*  into an array, with or without store/undo checking. */
/*  age=-1 for no check, 0 for old, 1 for new. */
/* May return e_rangecheck or e_invalidaccess. */
int ref_stack_store(P7(const ref_stack *, ref *, uint, uint, int, bool, client_name_t));

/* Pop the top N elements off a stack. */
/* The number must not exceed the number of elements in use. */
void ref_stack_pop(P2(ref_stack *, uint));
#define ref_stack_clear(pstk) ref_stack_pop(pstk, ref_stack_count(pstk))
#define ref_stack_pop_to(pstk, depth)\
  ref_stack_pop(pstk, ref_stack_count(pstk) - (depth))

/* Pop the top block off a stack. */
/* May return underflow_error. */
int ref_stack_pop_block(P1(ref_stack *));

/* Extend a stack to recover from an overflow condition. */
/* Uses the requested value to decide what to do. */
/* May return overflow_error or e_VMerror. */
int ref_stack_extend(P2(ref_stack *, uint));

/* Push N empty slots onto a stack.  These slots are not initialized; */
/* the caller must immediately fill them.  May return overflow_error */
/* (if max_stack would be exceeded, or the stack has no allocator) */
/* or e_VMerror. */
int ref_stack_push(P2(ref_stack *, uint));

/* Clean up a stack for garbage collection. */
void ref_stack_cleanup(P1(ref_stack *));

#endif					/* istack_INCLUDED */
