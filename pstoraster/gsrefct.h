/* Copyright (C) 1993, 1994, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsrefct.h,v 1.2 2000/03/08 23:14:46 mike Exp $ */
/* Reference counting definitions */

#ifndef gsrefct_INCLUDED
#  define gsrefct_INCLUDED

/*
 * A reference-counted object must include the following header:
 *      rc_header rc;
 * The header need not be the first element of the object.
 */
typedef struct rc_header_s rc_header;
struct rc_header_s {
    long ref_count;
    gs_memory_t *memory;
#define rc_free_proc(proc)\
  void proc(P3(gs_memory_t *, void *, client_name_t))
                rc_free_proc((*free));
};

/* ------ Allocate/free ------ */

rc_free_proc(rc_free_struct_only);
/* rc_init[_free] is only used to initialize stack-allocated structures. */
#define rc_init_free(vp, mem, rcinit, proc)\
  ((vp)->rc.ref_count = rcinit,\
   (vp)->rc.memory = mem,\
   (vp)->rc.free = proc)
#define rc_init(vp, mem, rcinit)\
  rc_init_free(vp, mem, rcinit, rc_free_struct_only)

#define rc_alloc_struct_n(vp, typ, pstyp, mem, errstat, cname, rcinit)\
  BEGIN\
    if ( ((vp) = gs_alloc_struct(mem, typ, pstyp, cname)) == 0 ) {\
      errstat;\
    } else {\
      rc_init(vp, mem, rcinit);\
    }\
  END
#define rc_alloc_struct_0(vp, typ, pstype, mem, errstat, cname)\
  rc_alloc_struct_n(vp, typ, pstype, mem, errstat, cname, 0)
#define rc_alloc_struct_1(vp, typ, pstype, mem, errstat, cname)\
  rc_alloc_struct_n(vp, typ, pstype, mem, errstat, cname, 1)

#define rc_free_struct(vp, cname)\
  (*(vp)->rc.free)((vp)->rc.memory, (void *)(vp), cname)

/* ------ Reference counting ------ */

/* Increment a reference count. */
#define rc_increment(vp)\
  BEGIN if ( (vp) != 0 ) (vp)->rc.ref_count++; END

/* Increment a reference count, allocating the structure if necessary. */
#define rc_allocate_struct(vp, typ, pstype, mem, errstat, cname)\
  BEGIN\
    if ( (vp) != 0 )\
      (vp)->rc.ref_count++;\
    else\
      rc_alloc_struct_1(vp, typ, pstype, mem, errstat, cname);\
  END

/* Guarantee that a structure is allocated and is not shared. */
#define rc_unshare_struct(vp, typ, pstype, mem, errstat, cname)\
  BEGIN\
    if ( (vp) == 0 || (vp)->rc.ref_count > 1 || (vp)->rc.memory != (mem) ) {\
      typ *new;\
      rc_alloc_struct_1(new, typ, pstype, mem, errstat, cname);\
      if ( vp ) (vp)->rc.ref_count--;\
      (vp) = new;\
    }\
  END

/* Adjust a reference count either up or down. */
#ifdef DEBUG
#  define rc_check_(vp)\
     BEGIN\
       if ( gs_debug_c('?') && (vp) != 0 && (vp)->rc.ref_count < 0 )\
	 lprintf2("0x%lx has ref_count of %ld!\n", (ulong)(vp),\
		  (vp)->rc.ref_count);\
     END
#else
#  define rc_check_(vp) DO_NOTHING
#endif
#define rc_adjust_(vp, delta, cname, body)\
  BEGIN\
    if ( (vp) != 0 && !((vp)->rc.ref_count += delta) ) {\
      rc_free_struct(vp, cname);\
      body;\
    } else\
      rc_check_(vp);\
  END
#define rc_adjust(vp, delta, cname)\
  rc_adjust_(vp, delta, cname, (vp) = 0)
#define rc_adjust_only(vp, delta, cname)\
  rc_adjust_(vp, delta, cname, DO_NOTHING)
#define rc_adjust_const(vp, delta, cname)\
  rc_adjust_only(vp, delta, cname)
#define rc_decrement(vp, cname)\
  rc_adjust(vp, -1, cname)
#define rc_decrement_only(vp, cname)\
  rc_adjust_only(vp, -1, cname)

/* Assign a pointer, adjusting reference counts. */
#define rc_assign(vpto, vpfrom, cname)\
  BEGIN\
    if ( (vpto) != (vpfrom) ) {\
      rc_decrement_only(vpto, cname);\
      (vpto) = (vpfrom);\
      rc_increment(vpto);\
    }\
  END
/* Adjust reference counts for assigning a pointer, */
/* but don't do the assignment.  We use this before assigning */
/* an entire structure containing reference-counted pointers. */
#define rc_pre_assign(vpto, vpfrom, cname)\
  BEGIN\
    if ( (vpto) != (vpfrom) ) {\
      rc_decrement_only(vpto, cname);\
      rc_increment(vpfrom);\
    }\
  END

#endif /* gsrefct_INCLUDED */
