/* Copyright (C) 1994, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* igc.h */
/* Internal interfaces in Ghostscript GC */

/* Define the procedures shared among a "genus" of structures. */
/* Currently there are only two genera: refs, and all other structures. */
struct struct_shared_procs_s {

	/* Clear the relocation information in an object. */

#define gc_proc_clear_reloc(proc)\
  void proc(P2(obj_header_t *pre, uint size))
	gc_proc_clear_reloc((*clear_reloc));

	/* Compute any internal relocation for a marked object. */
	/* Return true if the object should be kept. */
	/* The reloc argument shouldn't be required, */
	/* but we need it for ref objects. */

#define gc_proc_set_reloc(proc)\
  bool proc(P3(obj_header_t *pre, uint reloc, uint size))
	gc_proc_set_reloc((*set_reloc));

	/* Compact an object. */

#define gc_proc_compact(proc)\
  void proc(P3(obj_header_t *pre, obj_header_t *dpre, uint size))
	gc_proc_compact((*compact));

};

/* Define the structure for holding GC state. */
/*typedef struct gc_state_s gc_state_t;*/	/* in gsstruct.h */
struct gc_state_s {
	chunk_locator_t loc;
	vm_spaces spaces;
};

/* Exported by igcref.c for igc.c */
void gs_mark_refs(P3(ref_packed *, ref *, bool));
void ptr_ref_unmark(P2(void *, gc_state_t *));
bool ptr_ref_mark(P2(void *, gc_state_t *));
/*ref_packed *gs_reloc_ref_ptr(P2(const ref_packed *, gc_state_t *));*/

/* Exported by ilocate.c for igc.c */
void ialloc_validate_memory(P2(const gs_ref_memory_t *, gc_state_t *));
void ialloc_validate_chunk(P2(const chunk_t *, gc_state_t *));
void ialloc_validate_object(P3(const obj_header_t *, const chunk_t *,
  gc_state_t *));

/* Macro for returning a relocated pointer */
#ifdef DEBUG
void *print_reloc_proc(P3(const void *obj, const char *cname, void *robj));
#  define print_reloc(obj, cname, nobj)\
	(gs_debug_c('9') ? print_reloc_proc(obj, cname, nobj) :\
	 (void *)(nobj))
#else
#  define print_reloc(obj, cname, nobj)\
	(void *)(nobj)
#endif
