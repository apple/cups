/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsgc.h,v 1.1 2000/03/13 18:57:55 mike Exp $ */
/* Library-level interface to garbage collector */

/*
 * This API is not strictly at the library level, since it references
 * gs_ref_memory_t and the 4 PostScript memory spaces; however, the former
 * concept already leaks into the library's standard allocator, and the
 * latter is relatively small and harmless.
 */

#ifndef gsgc_INCLUDED
#  define gsgc_INCLUDED

/*
 * Define the VM space numbers, in increasing order of dynamism.  Pointers
 * from a higher-numbered space to the same or a lower-numbered space are
 * always allowed, but not vice versa.  Foreign space (the most static) is
 * internal, the rest are visible to the programmer; the index of foreign
 * space must be 0, so that we don't have to set any space bits in scalar
 * refs (PostScript objects).
 */
typedef enum {
    i_vm_foreign = 0,		/* must be 0 */
    i_vm_system,
    i_vm_global,
    i_vm_local,
    i_vm_max = i_vm_local
} i_vm_space;

/* Define an array of allocators indexed by space. */
#ifndef gs_ref_memory_DEFINED
#  define gs_ref_memory_DEFINED
typedef struct gs_ref_memory_s gs_ref_memory_t;

#endif
/*
 * r_space_bits is only defined in PostScript interpreters, but if it is
 * defined, we want to make sure it's 2.
 */
#ifdef r_space_bits
#  if r_space_bits != 2
Error_r_space_bits_is_not_2;
#  endif
#endif
typedef union vm_spaces_s {
    gs_ref_memory_t *indexed[4 /*1 << r_space_bits */ ];
    struct _ssn {
	gs_ref_memory_t *foreign;
	gs_ref_memory_t *system;
	gs_ref_memory_t *global;
	gs_ref_memory_t *local;
    } named;
} vm_spaces;

/* By convention, the vm_spaces member of structures, and local variables */
/* of type vm_spaces, are named spaces. */
#define space_foreign spaces.named.foreign
#define space_system spaces.named.system
#define space_global spaces.named.global
#define space_local spaces.named.local

/*
 * Define the top-level entry to the garbage collector.
 */
void gs_reclaim(P2(vm_spaces * pspaces, bool global));

#endif /* gsgc_INCLUDED */
