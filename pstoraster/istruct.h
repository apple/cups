/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* istruct.h */
/* Interpreter-level extension of gsstruct.h */

#ifndef istruct_INCLUDED
#  define istruct_INCLUDED

#include "gsstruct.h"

/* ================ Refs ================ */

/* The structure type descriptor for (blocks of) refs. */
/* This is defined in igc.c and exported for isave.c. */
extern_st(st_refs);

/* Relocate a pointer to a ref[_packed]. */
ptr_proc_reloc(gs_reloc_ref_ptr, ref_packed);

/* Relocate a block of ref[_packed]s. */
void gs_reloc_refs(P3(ref_packed *from, ref_packed *to, gc_state_t *gcst));

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

#endif					/* istruct_INCLUDED */
