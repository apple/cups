/* Copyright (C) 1995, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxalloc.h,v 1.2 2000/03/08 23:14:50 mike Exp $ */
/* Requires gsmemory.h, gsstruct.h */

#ifndef gxalloc_INCLUDED
#  define gxalloc_INCLUDED

#ifndef gs_ref_memory_DEFINED
#  define gs_ref_memory_DEFINED
typedef struct gs_ref_memory_s gs_ref_memory_t;

#endif

#include "gsalloc.h"
#include "gxobj.h"

/* ================ Chunks ================ */

/*
 * We obtain memory from the operating system in `chunks'.  A chunk
 * may hold only a single large object (or string), or it may hold
 * many objects (allocated from the bottom up, always aligned)
 * and strings (allocated from the top down, not aligned).
 */

/*
 * Refs are allocated in the bottom-up section, along with struct objects.
 * In order to keep the overhead for refs small, we make consecutive
 * blocks of refs into a single allocator object of type st_refs.
 * To do this, we remember the start of the current ref object (if any),
 * and the end of the last block of allocated refs.  As long as
 * the latter is equal to the top of the allocated area, we can add
 * more refs to the current object; otherwise, we have to start a new one.
 * We assume that sizeof(ref) % obj_align_mod == 0; this means that if we
 * ever have to pad a block of refs, we never add as much as one entire ref.
 */

/*
 * When we do a save, we create a new 'inner' chunk out of the remaining
 * space in the currently active chunk.  Inner chunks must not be freed
 * by a restore.
 *
 * The garbage collector implements relocation for refs by scanning
 * forward to a free object.  Because of this, every ref object must end
 * with a dummy ref that can hold the relocation for the last block.
 * In order to put a reasonable upper bound on the scanning time, we
 * limit the length of the objects that contain runs of refs.
 */
#define max_size_st_refs (50 * sizeof(ref))

/*
 * Strings carry some additional overhead for use by the GC.
 * At the top of the chunk is a table of relocation values for
 * 16N-character blocks of strings, where N is sizeof(uint).
 * This table is aligned, by adding padding above it if necessary.
 * Just below it is a mark table for the strings.  This table is also aligned,
 * to improve GC performance. The actual string data start below
 * the mark table.  These tables are not needed for a chunk that holds
 * a single large (non-string) object, but they are needed for all other
 * chunks, including chunks created to hold a single large string.
 */

/*
 * Define the unit of data manipulation for marking strings.
 */
typedef uint string_mark_unit;

#define log2_sizeof_string_mark_unit arch_log2_sizeof_int
/*
 * Define the quantum of relocation for strings, which determines
 * the quantum for reserving space.  This value must be a power of 2,
 * must be at least sizeof(string_mark_unit) * 8, and (because of the
 * unrolled loops in igcstr.c) currently must be equal to either 32 or 64.
 */
typedef uint string_reloc_offset;

#define log2_string_data_quantum (arch_log2_sizeof_int + 4)
#define string_data_quantum (1 << log2_string_data_quantum)
/*
 * Define the quantum for reserving string space, including data,
 * marks, and relocation.
 */
#define string_space_quantum\
  (string_data_quantum + (string_data_quantum / 8) +\
   sizeof(string_reloc_offset))
/*
 * Compute the amount of space needed for a chunk that holds only
 * a string of a given size.
 */
#define string_chunk_space(nbytes)\
  (((nbytes) + (string_data_quantum - 1)) / string_data_quantum *\
   string_space_quantum)
/*
 * Compute the number of string space quanta in a given amount of storage.
 */
#define string_space_quanta(spacebytes)\
  ((spacebytes) / string_space_quantum)
/*
 * Compute the size of string marks for a given number of quanta.
 */
#define string_quanta_mark_size(nquanta)\
  ((nquanta) * (string_data_quantum / 8))

/*
 * To allow the garbage collector to combine chunks, we store in the
 * head of each chunk the address to which its contents will be moved.
 */
						/*typedef struct chunk_head_s chunk_head_t; *//* in gxobj.h */

/* Structure for a chunk. */
typedef struct chunk_s chunk_t;
struct chunk_s {
    chunk_head_t *chead;	/* chunk head, bottom of chunk; */
    /* csbase is an alias for chead */
#define csbase(cp) ((byte *)(cp)->chead)
    /* Note that allocation takes place both from the bottom up */
    /* (aligned objects) and from the top down (strings). */
    byte *cbase;		/* bottom of chunk data area */
    byte *cbot;			/* bottom of free area */
    /* (top of aligned objects) */
    obj_header_t *rcur;		/* current refs object, 0 if none */
    byte *rtop;			/* top of rcur */
    byte *ctop;			/* top of free area */
    /* (bottom of strings) */
    byte *climit;		/* top of strings */
    byte *cend;			/* top of chunk */
    chunk_t *cprev;		/* chain chunks together, */
    chunk_t *cnext;		/*   sorted by address */
    chunk_t *outer;		/* the chunk of which this is */
    /*   an inner chunk, if any */
    uint inner_count;		/* number of chunks of which this is */
    /*   the outer chunk, if any */
    bool has_refs;		/* true if any refs in chunk */
    /*
     * Free lists for single bytes in blocks of 1-3 bytes,
     * one per 256 bytes in [csbase..climit).  The chain
     * pointer is a (1-byte) self-relative offset,
     * terminated by a 0; obviously, the chain is sorted by
     * increasing address.  The free list pointers themselves
     * are offsets relative to csbase.
     *
     * Note that these lists overlay the GC relocation table.
     */
    ushort *sfree1;
    /*
     * Free list for blocks of >= 4 bytes.  Each block begins
     * with a 2-byte size and a 2-byte next block pointer,
     * both big-endian.  This too is sorted in increasing address order.
     */
    ushort sfree;
    /* The remaining members are for the GC. */
    byte *odest;		/* destination for objects */
    byte *smark;		/* mark bits for strings */
    uint smark_size;
    byte *sbase;		/* base for computing smark offsets */
    string_reloc_offset *sreloc;	/* relocation for string blocks */
    byte *sdest;		/* destination for (top of) strings */
    byte *rescan_bot;		/* bottom of rescanning range if */
    /* the GC mark stack overflows */
    byte *rescan_top;		/* top of range ditto */
};

/* The chunk descriptor is exported only for isave.c. */
extern_st(st_chunk);
#define public_st_chunk()	/* in ialloc.c */\
  gs_public_st_ptrs2(st_chunk, chunk_t, "chunk_t",\
    chunk_enum_ptrs, chunk_reloc_ptrs, cprev, cnext)

/*
 * Macros for scanning a chunk linearly, with the following schema:
 *      SCAN_CHUNK_OBJECTS(cp)                  << declares pre, size >>
 *              << code for all objects -- size not set yet >>
 *      DO_LARGE
 *              << code for large objects >>
 *      DO_SMALL
 *              << code for small objects >>
 *      END_OBJECTS_SCAN
 * If large and small objects are treated alike, one can use DO_ALL instead
 * of DO_LARGE and DO_SMALL.
 */
#define SCAN_CHUNK_OBJECTS(cp)\
	{	obj_header_t *pre = (obj_header_t *)((cp)->cbase);\
		obj_header_t *end = (obj_header_t *)((cp)->cbot);\
		ulong size;		/* long because of large objects */\
		for ( ; pre < end;\
			pre = (obj_header_t *)((char *)pre + obj_size_round(size))\
		    )\
		{
#define DO_LARGE\
			if ( pre->o_large )\
			{	size = pre_obj_large_size(pre);\
				{
#define DO_SMALL\
				}\
			} else\
			{	size = pre_obj_small_size(pre);\
				{
#define DO_ALL\
			{	size = pre_obj_contents_size(pre);\
				{
#ifdef DEBUG
#  define END_OBJECTS_SCAN\
				}\
			}\
		}\
		if ( pre != end )\
		{	lprintf2("Chunk parsing error, 0x%lx != 0x%lx\n",\
				 (ulong)pre, (ulong)end);\
			gs_exit(1);\
		}\
	}
#else
#  define END_OBJECTS_SCAN\
				}\
			}\
		}\
	}
#endif

/* Initialize a chunk. */
/* This is exported for save/restore. */
void alloc_init_chunk(P5(chunk_t *, byte *, byte *, bool, chunk_t *));

/* Initialize the string freelists in a chunk. */
void alloc_init_free_strings(P1(chunk_t *));

/* Find the chunk for a pointer. */
/* Note that ptr_is_within_chunk returns true even if the pointer */
/* is in an inner chunk of the chunk being tested. */
#define ptr_is_within_chunk(ptr, cp)\
  ptr_between((const byte *)(ptr), (cp)->cbase, (cp)->cend)
#define ptr_is_in_inner_chunk(ptr, cp)\
  ((cp)->inner_count != 0 &&\
   ptr_between((const byte *)(ptr), (cp)->cbot, (cp)->ctop))
#define ptr_is_in_chunk(ptr, cp)\
  (ptr_is_within_chunk(ptr, cp) && !ptr_is_in_inner_chunk(ptr, cp))
typedef struct chunk_locator_s {
    const gs_ref_memory_t *memory;	/* for head & tail of chain */
    chunk_t *cp;		/* one-element cache */
} chunk_locator_t;
bool chunk_locate_ptr(P2(const void *, chunk_locator_t *));

#define chunk_locate(ptr, clp)\
  (((clp)->cp != 0 && ptr_is_in_chunk(ptr, (clp)->cp)) ||\
   chunk_locate_ptr(ptr, clp))

/* Close up the current chunk. */
/* This is exported for save/restore and for the GC. */
void alloc_close_chunk(P1(gs_ref_memory_t * mem));

/* Reopen the current chunk after a GC. */
void alloc_open_chunk(P1(gs_ref_memory_t * mem));

/* Insert or remove a chunk in the address-ordered chain. */
/* These are exported for the GC. */
void alloc_link_chunk(P2(chunk_t *, gs_ref_memory_t *));
void alloc_unlink_chunk(P2(chunk_t *, gs_ref_memory_t *));

/* Free a chunk.  This is exported for save/restore and for the GC. */
void alloc_free_chunk(P2(chunk_t *, gs_ref_memory_t *));

/* Print a chunk debugging message. */
/* Unfortunately, the ANSI C preprocessor doesn't allow us to */
/* define the list of variables being printed as a macro. */
#define dprintf_chunk_format\
  "%s 0x%lx (0x%lx..0x%lx, 0x%lx..0x%lx..0x%lx)\n"
#define dprintf_chunk(msg, cp)\
  dprintf7(dprintf_chunk_format,\
	   msg, (ulong)(cp), (ulong)(cp)->cbase, (ulong)(cp)->cbot,\
	   (ulong)(cp)->ctop, (ulong)(cp)->climit, (ulong)(cp)->cend)
#define if_debug_chunk(c, msg, cp)\
  if_debug7(c, dprintf_chunk_format,\
	    msg, (ulong)(cp), (ulong)(cp)->cbase, (ulong)(cp)->cbot,\
	    (ulong)(cp)->ctop, (ulong)(cp)->climit, (ulong)(cp)->cend)

/* ================ Allocator state ================ */

/* Structures for save/restore (not defined here). */
struct alloc_save_s;
struct alloc_change_s;

/* Stream structure, only needed for the streams member of the state. */
#ifndef stream_DEFINED
#  define stream_DEFINED
typedef struct stream_s stream;

#endif

/* Define the number of freelists.  The index in the freelist array */
/* is the ceiling of the size of the object contents (i.e., not including */
/* the header) divided by obj_align_mod. */
#define max_freelist_size 800	/* big enough for gstate & contents */
#define num_freelists\
  ((max_freelist_size + obj_align_mod - 1) / obj_align_mod + 1)

/* Define the memory manager subclass for this allocator. */
struct gs_ref_memory_s {
    /* The following are set at initialization time. */
    gs_memory_common;
    gs_raw_memory_t *parent;	/* for allocating chunks */
    uint chunk_size;
    uint large_size;		/* min size to give large object */
				/* its own chunk: must be */
				/* 1 mod obj_align_mod */
    gs_ref_memory_t *global;	/* global VM for this allocator */
				/* (may point to itself) */
    uint space;			/* a_local, a_global, a_system */
    /* Callers can change the following dynamically */
    /* (through a procedural interface). */
    gs_memory_gc_status_t gc_status;
    /* The following are updated dynamically. */
    bool is_controlled;		/* if true, this allocator doesn't manage */
				/* its own chunks */
    ulong limit;		/* signal a VMerror when total */
				/* allocated exceeds this */
    chunk_t *cfirst;		/* head of chunk list */
    chunk_t *clast;		/* tail of chunk list */
    chunk_t cc;			/* current chunk */
    chunk_t *pcc;		/* where to store cc */
    chunk_locator_t cfreed;	/* chunk where last object freed */
    ulong allocated;		/* total size of all chunks */
				/* allocated at this save level */
    long inherited;		/* chunks allocated at outer save */
				/* levels that should be counted */
				/* towards the GC threshold */
				/* (may be negative, but allocated + */
				/* inherited >= 0 always) */
    ulong gc_allocated;		/* value of (allocated + */
				/* previous_status.allocated) after last GC */
    struct lost_ {		/* space freed and 'lost' */
	ulong objects;
	ulong refs;
	ulong strings;
    } lost;
    /*
     * The following are for the interpreter's convenience: the
     * library initializes them to 0 and then never touches them.
     */
    stream *streams;
    /* Garbage collector information */
    gs_gc_root_t *roots;	/* roots for GC */
    /* Sharing / saved state information */
    int num_contexts;		/* # of contexts sharing this VM */
    struct alloc_change_s *changes;
    struct alloc_save_s *saved;
    struct alloc_save_s *reloc_saved;	/* for GC */
    gs_memory_status_t previous_status;		/* total allocated & used */
				/* in outer save levels */
				/* We put the freelists last to keep the */
                                /* scalar offsets small. */
    obj_header_t *freelists[num_freelists];
};

/* The descriptor for gs_ref_memory_t is exported only for */
/* the alloc_save_t subclass; otherwise, it should be private. */
extern_st(st_ref_memory);
#define public_st_ref_memory()	/* in gsalloc.c */\
  gs_public_st_composite(st_ref_memory, gs_ref_memory_t,\
    "gs_ref_memory", ref_memory_enum_ptrs, ref_memory_reloc_ptrs)
#define st_ref_memory_max_ptrs 3	/* streams, changes, saved */

/* Define the procedures for the standard allocator. */
/* We export this for subclasses. */
extern const gs_memory_procs_t gs_ref_memory_procs;

/*
 * Scan the chunks of an allocator:
 *      SCAN_MEM_CHUNKS(mem, cp)
 *              << code to process chunk cp >>
 *      END_CHUNKS_SCAN
 */
#define SCAN_MEM_CHUNKS(mem, cp)\
	{	chunk_t *cp = (mem)->cfirst;\
		for ( ; cp != 0; cp = cp->cnext )\
		{
#define END_CHUNKS_SCAN\
		}\
	}

#endif /* gxalloc_INCLUDED */
