/* Copyright (C) 1989, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: ialloc.h,v 1.2 2000/03/08 23:15:07 mike Exp $ */
/* Interface to Ghostscript interpreter memory allocator */

#ifndef ialloc_INCLUDED
#  define ialloc_INCLUDED

#include "imemory.h"

/*
 * Define the interpreter memory manager instance.
 */
extern gs_dual_memory_t gs_imemory;

#define idmemory (&gs_imemory)
#define iimemory (gs_imemory.current)
#define imemory ((gs_memory_t *)iimemory)
#define iimemory_local (gs_imemory.space_local)
#define imemory_local ((gs_memory_t *)iimemory_local)
#define iimemory_global (gs_imemory.space_global)
#define imemory_global ((gs_memory_t *)iimemory_global)
#define iimemory_system (gs_imemory.space_system)
#define imemory_system ((gs_memory_t *)iimemory_system)

/*
 * Aliases for invoking the standard allocator interface.
 */
#define ialloc_bytes(nbytes, cname)\
  gs_alloc_bytes(imemory, nbytes, cname)
#define ialloc_struct(typ, pstype, cname)\
  gs_alloc_struct(imemory, typ, pstype, cname)
#define ialloc_byte_array(nelts, esize, cname)\
  gs_alloc_byte_array(imemory, nelts, esize, cname)
#define ialloc_struct_array(nelts, typ, pstype, cname)\
  gs_alloc_struct_array(imemory, nelts, typ, pstype, cname)
#define ifree_object(data, cname)\
  gs_free_object(imemory, data, cname)
#define ialloc_string(nbytes, cname)\
  gs_alloc_string(imemory, nbytes, cname)
#define ifree_string(data, nbytes, cname)\
  gs_free_string(imemory, data, nbytes, cname)

/* Initialize the interpreter's allocator. */
void ialloc_init(P3(gs_raw_memory_t *, uint, bool));

/* Resize a string. */
#define iresize_string(data, oldn, newn, cname)\
  gs_resize_string(imemory, data, oldn, newn, cname)

/* ------ Internal routines ------ */

/* Reset the request values that identify the cause of a GC. */
void ialloc_reset_requested(P1(gs_dual_memory_t *));

/* Validate the contents of memory. */
void ialloc_validate_spaces(P1(const gs_dual_memory_t *));

#define ivalidate_spaces() ialloc_validate_spaces(idmemory)

/*
 * Local/global VM management.
 */

/* Get the space attribute of the current allocator. */
#define ialloc_space(dmem) ((dmem)->current_space)
#define icurrent_space ialloc_space(idmemory)
extern uint imemory_space(P1(gs_ref_memory_t *));

/* Select the allocation space. */
void ialloc_set_space(P2(gs_dual_memory_t *, uint));

/*
 * Ref-related facilities.
 */

#ifdef r_type			/* i.e., we know about refs */

/* Allocate and free ref arrays. */
#define ialloc_ref_array(paref, attrs, nrefs, cname)\
  gs_alloc_ref_array(iimemory, paref, attrs, nrefs, cname)
#define iresize_ref_array(paref, nrefs, cname)\
  gs_resize_ref_array(iimemory, paref, nrefs, cname)
#define ifree_ref_array(paref, cname)\
  gs_free_ref_array(iimemory, paref, cname)

/* Allocate a string ref. */
#define ialloc_string_ref(psref, attrs, nbytes, cname)\
  gs_alloc_string_ref(iimemory, psref, attrs, nbytes, cname)

/* Make a ref for a newly allocated structure. */
#define make_istruct(pref,attrs,ptr)\
  make_struct(pref, (attrs) | icurrent_space, ptr)
#define make_istruct_new(pref,attrs,ptr)\
  make_struct_new(pref, (attrs) | icurrent_space, ptr)
#define make_iastruct(pref,attrs,ptr)\
  make_astruct(pref, (attrs) | icurrent_space, ptr)
#define make_iastruct_new(pref,attrs,ptr)\
  make_astruct_new(pref, (attrs) | icurrent_space, ptr)

#endif /* ifdef r_type */

#endif /* ialloc_INCLUDED */
