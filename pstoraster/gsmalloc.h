/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsmalloc.h,v 1.1 2000/03/13 18:57:14 mike Exp $ */
/* Client interface to default (C heap) allocator */
/* Requires gsmemory.h */

#ifndef gsmalloc_INCLUDED
#  define gsmalloc_INCLUDED

/* Define a memory manager that allocates directly from the C heap. */
typedef struct gs_malloc_block_s gs_malloc_block_t;
typedef struct gs_malloc_memory_s {
    gs_memory_common;
    gs_malloc_block_t *allocated;
    long limit;
    long used;
    long max_used;
} gs_malloc_memory_t;

/* Allocate and initialize a malloc memory manager. */
gs_malloc_memory_t *gs_malloc_memory_init(P0());

/* Release all the allocated blocks, and free the memory manager. */
/* The cast is unfortunate, but unavoidable. */
#define gs_malloc_memory_release(mem)\
  gs_memory_free_all((gs_memory_t *)mem, FREE_ALL_EVERYTHING,\
		     "gs_malloc_memory_release")

/*
 * Define a default allocator that allocates from the C heap.
 * (We would really like to get rid of this.)
 */
extern gs_malloc_memory_t *gs_malloc_memory_default;
#define gs_memory_default (*(gs_memory_t *)gs_malloc_memory_default)

/*
 * The following procedures are historical artifacts that we hope to
 * get rid of someday.
 */
#define gs_malloc_init()\
  (gs_malloc_memory_default = gs_malloc_memory_init())
#define gs_malloc_release()\
  (gs_malloc_memory_release(gs_malloc_memory_default),\
   gs_malloc_memory_default = 0)
#define gs_malloc(nelts, esize, cname)\
  (void *)gs_alloc_byte_array(&gs_memory_default, nelts, esize, cname)
#define gs_free(data, nelts, esize, cname)\
  gs_free_object(&gs_memory_default, data, cname)

/* Define an accessor for the limit on the total allocated heap space. */
#define gs_malloc_limit (gs_malloc_memory_default->limit)

/* Define an accessor for the maximum amount ever allocated from the heap. */
#define gs_malloc_max (gs_malloc_memory_default->max_used)

#endif /* gsmalloc_INCLUDED */
