/* Copyright (C) 1993, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsmemory.h,v 1.2 2000/03/08 23:14:44 mike Exp $ */
/* Client interface for memory allocation */

/*
 * The allocator knows about two basic kinds of memory: objects, which are
 * aligned and cannot have pointers to their interior, and strings, which
 * are not aligned and which can have interior references.
 *
 * The standard allocator is designed to interface to a garbage collector,
 * although it does not include or call one.  The allocator API recognizes
 * that the garbage collector may move objects, relocating pointers to them;
 * the API provides for allocating both movable (the default) and immovable
 * objects.  Clients must not attempt to resize immovable objects, and must
 * not create references to substrings of immovable strings.
 */

#ifndef gsmemory_INCLUDED
#  define gsmemory_INCLUDED

#include "gsmemraw.h"

/* Define the opaque type for a structure descriptor. */
typedef struct gs_memory_struct_type_s gs_memory_struct_type_t;
typedef const gs_memory_struct_type_t *gs_memory_type_ptr_t;

/* Define the opaque type for an allocator. */
/* (The actual structure is defined later in this file.) */
typedef struct gs_memory_s gs_memory_t;

/* Define the opaque type for a pointer type. */
typedef struct gs_ptr_procs_s gs_ptr_procs_t;
typedef const gs_ptr_procs_t *gs_ptr_type_t;

/* Define the opaque type for a GC root. */
typedef struct gs_gc_root_s gs_gc_root_t;

	/* Accessors for structure types. */

typedef client_name_t struct_name_t;

/* Get the size of a structure from the descriptor. */
uint gs_struct_type_size(P1(gs_memory_type_ptr_t));

/* Get the name of a structure from the descriptor. */
struct_name_t gs_struct_type_name(P1(gs_memory_type_ptr_t));

#define gs_struct_type_name_string(styp)\
  ((const char *)gs_struct_type_name(styp))

/*
 * Define the memory manager procedural interface.
 */
typedef struct gs_memory_procs_s {

    gs_raw_memory_procs(gs_memory_t);	/* defined in gsmemraw.h */

    /* Redefine inherited procedures with the new allocator type. */

#define gs_memory_proc_alloc_bytes(proc)\
  gs_memory_t_proc_alloc_bytes(proc, gs_memory_t)
#define gs_memory_proc_resize_object(proc)\
  gs_memory_t_proc_resize_object(proc, gs_memory_t)
#define gs_memory_proc_free_object(proc)\
  gs_memory_t_proc_free_object(proc, gs_memory_t)
#define gs_memory_proc_status(proc)\
  gs_memory_t_proc_status(proc, gs_memory_t)
#define gs_memory_proc_free_all(proc)\
  gs_memory_t_proc_free_all(proc, gs_memory_t)
#define gs_memory_proc_consolidate_free(proc)\
  gs_memory_t_proc_consolidate_free(proc, gs_memory_t)

    /*
     * Allocate possibly movable bytes.  (We inherit allocating immovable
     * bytes from the raw memory allocator.)
     */

#define gs_alloc_bytes(mem, nbytes, cname)\
  (*(mem)->procs.alloc_bytes)(mem, nbytes, cname)
    gs_memory_proc_alloc_bytes((*alloc_bytes));

    /*
     * Allocate a structure.
     */

#define gs_memory_proc_alloc_struct(proc)\
  void *proc(P3(gs_memory_t *mem, gs_memory_type_ptr_t pstype,\
    client_name_t cname))
#define gs_alloc_struct(mem, typ, pstype, cname)\
  (typ *)(*(mem)->procs.alloc_struct)(mem, pstype, cname)
    gs_memory_proc_alloc_struct((*alloc_struct));
#define gs_alloc_struct_immovable(mem, typ, pstype, cname)\
  (typ *)(*(mem)->procs.alloc_struct_immovable)(mem, pstype, cname)
    gs_memory_proc_alloc_struct((*alloc_struct_immovable));

    /*
     * Allocate an array of bytes.
     */

#define gs_memory_proc_alloc_byte_array(proc)\
  byte *proc(P4(gs_memory_t *mem, uint num_elements, uint elt_size,\
    client_name_t cname))
#define gs_alloc_byte_array(mem, nelts, esize, cname)\
  (*(mem)->procs.alloc_byte_array)(mem, nelts, esize, cname)
    gs_memory_proc_alloc_byte_array((*alloc_byte_array));
#define gs_alloc_byte_array_immovable(mem, nelts, esize, cname)\
  (*(mem)->procs.alloc_byte_array_immovable)(mem, nelts, esize, cname)
    gs_memory_proc_alloc_byte_array((*alloc_byte_array_immovable));

    /*
     * Allocate an array of structures.
     */

#define gs_memory_proc_alloc_struct_array(proc)\
  void *proc(P4(gs_memory_t *mem, uint num_elements,\
    gs_memory_type_ptr_t pstype, client_name_t cname))
#define gs_alloc_struct_array(mem, nelts, typ, pstype, cname)\
  (typ *)(*(mem)->procs.alloc_struct_array)(mem, nelts, pstype, cname)
    gs_memory_proc_alloc_struct_array((*alloc_struct_array));
#define gs_alloc_struct_array_immovable(mem, nelts, typ, pstype, cname)\
 (typ *)(*(mem)->procs.alloc_struct_array_immovable)(mem, nelts, pstype, cname)
    gs_memory_proc_alloc_struct_array((*alloc_struct_array_immovable));

    /*
     * Get the size of an object (anything except a string).
     */

#define gs_memory_proc_object_size(proc)\
  uint proc(P2(gs_memory_t *mem, const void *obj))
#define gs_object_size(mem, obj)\
  (*(mem)->procs.object_size)(mem, obj)
    gs_memory_proc_object_size((*object_size));

    /*
     * Get the type of an object (anything except a string).
     * The value returned for byte objects is useful only for
     * printing.
     */

#define gs_memory_proc_object_type(proc)\
  gs_memory_type_ptr_t proc(P2(gs_memory_t *mem, const void *obj))
#define gs_object_type(mem, obj)\
  (*(mem)->procs.object_type)(mem, obj)
    gs_memory_proc_object_type((*object_type));

    /*
     * Allocate a string (unaligned bytes).
     */

#define gs_memory_proc_alloc_string(proc)\
  byte *proc(P3(gs_memory_t *mem, uint nbytes, client_name_t cname))
#define gs_alloc_string(mem, nbytes, cname)\
  (*(mem)->procs.alloc_string)(mem, nbytes, cname)
    gs_memory_proc_alloc_string((*alloc_string));
#define gs_alloc_string_immovable(mem, nbytes, cname)\
  (*(mem)->procs.alloc_string_immovable)(mem, nbytes, cname)
    gs_memory_proc_alloc_string((*alloc_string_immovable));

    /*
     * Resize a string.
     */

#define gs_memory_proc_resize_string(proc)\
  byte *proc(P5(gs_memory_t *mem, byte *data, uint old_num, uint new_num,\
    client_name_t cname))
#define gs_resize_string(mem, data, oldn, newn, cname)\
  (*(mem)->procs.resize_string)(mem, data, oldn, newn, cname)
    gs_memory_proc_resize_string((*resize_string));

    /*
     * Free a string.
     */

#define gs_memory_proc_free_string(proc)\
  void proc(P4(gs_memory_t *mem, byte *data, uint nbytes,\
    client_name_t cname))
#define gs_free_string(mem, data, nbytes, cname)\
  (*(mem)->procs.free_string)(mem, data, nbytes, cname)
    gs_memory_proc_free_string((*free_string));

    /*
     * Register a root for the garbage collector.  root = NULL
     * asks the memory manager to allocate the root object
     * itself (immovable, in the manager's parent): this is the usual
     * way to call this procedure.
     */

#define gs_memory_proc_register_root(proc)\
  int proc(P5(gs_memory_t *mem, gs_gc_root_t *root, gs_ptr_type_t ptype,\
    void **pp, client_name_t cname))
#define gs_register_root(mem, root, ptype, pp, cname)\
  (*(mem)->procs.register_root)(mem, root, ptype, pp, cname)
    gs_memory_proc_register_root((*register_root));

    /*
     * Unregister a root.  The root object itself will be freed iff
     * it was allocated by gs_register_root.
     */

#define gs_memory_proc_unregister_root(proc)\
  void proc(P3(gs_memory_t *mem, gs_gc_root_t *root, client_name_t cname))
#define gs_unregister_root(mem, root, cname)\
  (*(mem)->procs.unregister_root)(mem, root, cname)
    gs_memory_proc_unregister_root((*unregister_root));

    /*
     * Enable or disable the freeing operations: when disabled,
     * these operations return normally but do nothing.  The
     * garbage collector and the PostScript interpreter
     * 'restore' operator need to temporarily disable the
     * freeing functions of (an) allocator(s) while running
     * finalization procedures.
     */

#define gs_memory_proc_enable_free(proc)\
  void proc(P2(gs_memory_t *mem, bool enable))
#define gs_enable_free(mem, enable)\
  (*(mem)->procs.enable_free)(mem, enable)
    gs_memory_proc_enable_free((*enable_free));

} gs_memory_procs_t;

/* Register a structure root.  This just calls gs_register_root. */
int gs_register_struct_root(P4(gs_memory_t *mem, gs_gc_root_t *root,
			       void **pp, client_name_t cname));

/* Define no-op freeing procedures for use by enable_free. */
gs_memory_proc_free_object(gs_ignore_free_object);
gs_memory_proc_free_string(gs_ignore_free_string);

/* Define a no-op consolidation procedure. */
gs_memory_proc_consolidate_free(gs_ignore_consolidate_free);

/*
 * Allocate a structure using a "raw memory" allocator.  Note that this does
 * not retain the identity of the structure.  Note also that it returns a
 * void *, and does not take the type of the returned pointer as a
 * parameter.
 */
void *gs_raw_alloc_struct_immovable(P3(gs_raw_memory_t * rmem,
				       gs_memory_type_ptr_t pstype,
				       client_name_t cname));

/*
 * Define an abstract allocator instance.
 * Subclasses may have state as well.
 */
#define gs_memory_common\
	gs_memory_procs_t procs
struct gs_memory_s {
    gs_memory_common;
};

#endif /* gsmemory_INCLUDED */
