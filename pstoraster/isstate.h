/* Copyright (C) 1993, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: isstate.h,v 1.2 2000/03/08 23:15:16 mike Exp $ */
/* Requires isave.h */

#ifndef isstate_INCLUDED
#  define isstate_INCLUDED

/* Saved state of allocator and other things as needed. */
						/*typedef struct alloc_save_s alloc_save_t; *//* in isave.h */
struct alloc_save_s {
    gs_ref_memory_t state;	/* must be first for subclassing */
    gs_dual_memory_t *dmem;
    bool restore_names;
    bool is_current;
    ulong id;
    void *client_data;
};

#define private_st_alloc_save()	/* in isave.c */\
  gs_private_st_suffix_add1(st_alloc_save, alloc_save_t, "alloc_save",\
    save_enum_ptrs, save_reloc_ptrs, st_ref_memory, client_data)

#endif /* isstate_INCLUDED */
