/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gsalloc.h */
/* Memory allocator extensions for standard allocator */

#ifndef gsalloc_INCLUDED
#  define gsalloc_INCLUDED

/*
 * Define a structure and interface for GC-related allocator state.
 */
typedef struct gs_memory_gc_status_s {
		/* Set by client */
	long vm_threshold;		/* GC interval */
	long max_vm;			/* maximum allowed allocation */
	int *psignal;			/* if not NULL, store signal_value */
				/* here if we go over the vm_threshold */
	int signal_value;		/* value to store in *psignal */
	bool enabled;			/* auto GC enabled if true */
		/* Set by allocator */
	long requested;			/* amount of last failing request */
} gs_memory_gc_status_t;
void gs_memory_gc_status(P2(const gs_ref_memory_t *, gs_memory_gc_status_t *));
void gs_memory_set_gc_status(P2(gs_ref_memory_t *, const gs_memory_gc_status_t *));

/* ------ Internal routines ------ */

/* Initialize after a save. */
void ialloc_reset(P1(gs_ref_memory_t *));

/* Initialize after a save or GC. */
void ialloc_reset_free(P1(gs_ref_memory_t *));

/* Set the cached allocation limit of an alloctor from its GC parameters. */
void ialloc_set_limit(P1(gs_ref_memory_t *));

#endif					/* gsalloc_INCLUDED */
