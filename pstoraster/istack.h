/* Copyright (C) 1992, 1995, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: istack.h,v 1.2 2000/03/08 23:15:16 mike Exp $ */
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
 * are implemented as a linked list of blocks.
 *
 * Since all operators exit cleanly in case of stack under- or overflow,
 * we handle all issues related to stack blocks in the top-level error
 * recovery code in interp.c.  A few situations require special treatment:
 * see ostack.h, estack.h, and dstack.h for details.
 */

typedef ref *s_ptr;
typedef const ref *const_s_ptr;

/*
 * Define the structure for a stack block.
 * In order to simplify allocation, stack blocks are implemented as
 * t_array objects, with the first few elements used for special purposes.
 * The actual layout is as follows:
 *              ref_stack_block structure
 *              bottom guard if any (see below)
 *              used elements of block
 *              unused elements of block
 *              top guard if any (see below)
 * The `next' member of the next higher stack block includes all of this.
 * The `used' member only includes the used elements of this block.
 * Notes:
 *      - In the top block, the size of the `used' member may not be correct.
 *      - In all blocks but the top, we fill the unused elements with nulls.
 */
typedef struct ref_stack_block_s {
    ref next;			/* t_array, next lower block on stack */
    ref used;			/* t_array, subinterval of this block */
    /* Actual stack starts here */
} ref_stack_block;

#define stack_block_refs (sizeof(ref_stack_block) / sizeof(ref))

/*
 * In order to detect under- and overflow with minimum overhead, we put
 * guard elements at the top and bottom of each stack block (see dstack.h,
 * estack.h, and ostack.h for details of the individual stacks).  Note that
 * the 'current' and 'next' arrays include the guard elements.
 */

/*
 * The garbage collector requires that the entire contents of every block
 * be 'clean', i.e., contain legitimate refs; we also need to ensure that
 * at GC time, pointers in unused areas of a block will not be followed
 * (since they may be dangling).  We ensure this as follows:
 *      - When allocating a new block, we set the entire body to nulls.
 *      This is necessary because the block may be freed before the next GC,
 *      and the GC must be able to scan (parse) refs even if they are free.
 *      - When adding a new block to the top of the stack, we set to nulls
 *      the unused area of the new next-to-top blocks.
 *      - At the beginning of garbage collection, we set to nulls the unused
 *      elements of the top block.
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
    s_ptr top;			/* topmost valid element = */
				/* bot + data_size */
    ref current;		/* t_array for current top block */
    uint extension_size;	/* total sizes of extn. blocks */
    uint extension_used;	/* total used sizes of extn. blocks */
    /* Following are updated rarely. */
    ref max_stack;		/* t_integer, Max...Stack user param */
    uint requested;		/* amount of last failing */
				/* push or pop request */
    uint margin;		/* # of slots to leave between limit */
				/* and top */
    uint body_size;		/* data_size - margin */
    /* Following are set at initialization. */
    uint bot_guard;		/* # of guard elements below bot */
    uint top_guard;		/* # of guard elements above top */
    uint block_size;		/* size of each block */
    uint data_size;		/* # of data slots in each block */
    ref guard_value;		/* t__invalid or t_operator, */
				/* bottom guard value */
    int underflow_error;	/* error code for underflow */
    int overflow_error;		/* error code for overflow */
    bool allow_expansion;	/* if false, don't expand */
    gs_ref_memory_t *memory;	/* allocator for blocks */
};
#define public_st_ref_stack()	/* in istack.c */\
  gs_public_st_complex_only(st_ref_stack, ref_stack, "ref_stack",\
    ref_stack_clear_marks, ref_stack_enum_ptrs, ref_stack_reloc_ptrs, 0)
#define st_ref_stack_num_ptrs 1	/* current */

/* ------ Procedural interface ------ */

/* Initialize a stack. */
void ref_stack_init(P6(ref_stack *, ref *, uint, uint, ref *,
		       gs_ref_memory_t *));

/* Set the maximum number of elements allowed on a stack. */
/* Note that the value is a long, not a uint or a ulong. */
int ref_stack_set_max_count(P2(ref_stack *, long));

/* Set the margin between the limit and the top of the stack. */
/* Note that this may require allocating a block. */
int ref_stack_set_margin(P2(ref_stack *, uint));

/* Return the number of elements on a stack. */
uint ref_stack_count(P1(const ref_stack *));

#define ref_stack_count_inline(pstk)\
  ((pstk)->p + 1 - (pstk)->bot + (pstk)->extension_used)

/* Return the maximum number of elements allowed on a stack. */
#define ref_stack_max_count(pstk) (uint)((pstk)->max_stack.value.intval)

/* Return a pointer to a given element from the stack, counting from */
/* 0 as the top element.  If the index is out of range, return 0. */
/* Note that the index is a long, not a uint or a ulong. */
ref *ref_stack_index(P2(const ref_stack *, long));

/* Count the number of elements down to and including the first mark. */
/* If no mark is found, return 0. */
uint ref_stack_counttomark(P1(const ref_stack *));

/*
 * Do the store check for storing 'count' elements of a stack, starting
 * 'skip' elements below the top, into an array.  Return 0 or e_invalidaccess.
 */
int ref_stack_store_check(P4(const ref_stack * pstack, ref * parray,
			     uint count, uint skip));

/*
 * Store 'count elements of a stack, starting 'skip' elements below the top,
 * into an array, with or without store/undo checking.
 * age=-1 for no check, 0 for old, 1 for new.
 * May return e_rangecheck or e_invalidaccess.
 */
int ref_stack_store(P7(const ref_stack * pstack, ref * parray, uint count,
		       uint skip, int age, bool check, client_name_t cname));

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

/*
 * Enumerate the blocks of a stack from top to bottom, as follows:

   ref_stack_enum_t rsenum;

   ref_stack_enum_begin(&rsenum, pstack);
   do {
      ... process rsenum.size refs starting at rsenum.ptr ...
   } while (ref_stack_enum_next(&rsenum));

 */
typedef struct ref_stack_enum_s {
    ref_stack_block *block;
    ref *ptr;
    uint size;
} ref_stack_enum_t;
void ref_stack_enum_begin(P2(ref_stack_enum_t *, const ref_stack *));
bool ref_stack_enum_next(P1(ref_stack_enum_t *));

/* Define a previous enumeration structure, for backward compatibility. */
#define STACK_LOOP_BEGIN(pstack, ptrv, sizev)\
{ ref_stack_enum_t enum_;\
  ref_stack_enum_begin(&enum_, pstack);\
  do {\
    ref *ptrv = enum_.ptr;\
    uint sizev = enum_.size;
#define STACK_LOOP_END(ptrv, sizev)\
  } while (ref_stack_enum_next(&enum_));\
}

/* Clean up a stack for garbage collection. */
void ref_stack_cleanup(P1(ref_stack *));

/*
 * Free the entire contents of a stack, including the bottom block.
 * The client must free the ref_stack itself.  Note that after calling
 * ref_stack_release, the stack is no longer usable.
 */
void ref_stack_release(P1(ref_stack *));

/*
 * Release a stack and then free the ref_stack object.
 */
void ref_stack_free(P3(ref_stack * pstack, gs_memory_t * mem,
		       client_name_t cname));

#endif /* istack_INCLUDED */
