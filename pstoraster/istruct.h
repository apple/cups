/* Copyright (C) 1994, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: istruct.h,v 1.2 2000/03/08 23:15:17 mike Exp $ */
/* Interpreter-level extension of gsstruct.h */

#ifndef istruct_INCLUDED
#  define istruct_INCLUDED

#include "gsstruct.h"

/* ================ Refs ================ */

/*
 * Define the pointer type for refs.  Note that if a structure contains refs,
 * both its clear_marks and its reloc_ptrs procedure must unmark them,
 * since the GC will never see the refs during the unmarking sweep.
 */
extern const gs_ptr_procs_t ptr_ref_procs;
#define ptr_ref_type (&ptr_ref_procs)

/* The structure type descriptor for (blocks of) refs. */
/* This is defined in igc.c and exported for isave.c. */
extern_st(st_refs);

/*
 * Extend the GC procedure vector to include refs.
 */
#define refs_proc_reloc(proc)\
  void proc(P3(ref_packed *from, ref_packed *to, gc_state_t *gcst))
typedef struct gc_procs_with_refs_s {
    gc_procs_common;
    /* Relocate a pointer to a ref[_packed]. */
    ptr_proc_reloc((*reloc_ref_ptr), ref_packed);
    /* Relocate a block of ref[_packed]s. */
    refs_proc_reloc((*reloc_refs));
} gc_procs_with_refs_t;

#undef gc_proc
#define gc_proc(gcst, proc) ((*(const gc_procs_with_refs_t **)(gcst))->proc)

/*
 * Define enumeration and relocation macros analogous to those for
 * structures and strings.  (We should go back and change the names of
 * those macros to be consistent which these, which are better, but it's
 * not worth the trouble.)
 */
#define ENUM_RETURN_REF(ptr)\
  BEGIN *pep = (const void *)(ptr); return ptr_ref_type; END
#define ENUM_RETURN_REF_MEMBER(typ, memb)\
  ENUM_RETURN_REF(&((typ *)vptr)->memb)
#define RELOC_REF_PTR_VAR(ptrvar)\
  ptrvar = (*gc_proc(gcst, reloc_ref_ptr))((const void *)(ptrvar), gcst)
#define RELOC_REF_PTR_MEMBER(typ, memb)\
  RELOC_REF_PTR_VAR(((typ *)vptr)->memb)
#define RELOC_REFS(from, upto)\
  (*gc_proc(gcst, reloc_refs))((ref_packed *)(from), (ref_packed *)(upto), gcst)
#define RELOC_REF_VAR(refvar)\
  RELOC_REFS(&(refvar), &(refvar) + 1)

/*
 * Define an object allocated as a struct, but actually containing refs.
 * Such objects are useful as the client_data of library structures
 * (currently only gstates and fonts).
 */
struct_proc_clear_marks(ref_struct_clear_marks);
struct_proc_enum_ptrs(ref_struct_enum_ptrs);
struct_proc_reloc_ptrs(ref_struct_reloc_ptrs);
#define gs__st_ref_struct(scope_st, stname, stype, sname)\
  gs__st_complex_only(scope_st, stname, stype, sname, ref_struct_clear_marks,\
    ref_struct_enum_ptrs, ref_struct_reloc_ptrs, 0)
#define gs_public_st_ref_struct(stname, stype, sname)\
  gs__st_ref_struct(public_st, stname, stype, sname)
#define gs_private_st_ref_struct(stname, stype, sname)\
  gs__st_ref_struct(private_st, stname, stype, sname)

#endif /* istruct_INCLUDED */
