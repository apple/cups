/* Copyright (C) 1993, 1994, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gsrefct.h */
/* Reference counting definitions */

#ifndef gsrefct_INCLUDED
#  define gsrefct_INCLUDED

/*
 * In many places below, a do {...} while (0) avoids problems with a possible
 * enclosing 'if'.
 */

/*
 * A reference-counted object must include the following header:
 *	rc_header rc;
 * The header need not be the first element of the object.
 */
typedef struct rc_header_s rc_header;
struct rc_header_s {
	long ref_count;
#define rc_free_proc(proc)\
  void proc(P3(gs_memory_t *, void *, client_name_t))
	rc_free_proc((*free));
};

/* ------ Allocate/free ------ */

rc_free_proc(rc_free_struct_only);
#define rc_alloc_struct_n(vp, typ, pstyp, mem, errstat, cname, rcinit)\
  do\
   {	if ( (vp = gs_alloc_struct(mem, typ, pstyp, cname)) == 0 )\
	   errstat;\
	vp->rc.ref_count = rcinit;\
	vp->rc.free = rc_free_struct_only;\
   }\
  while (0)
#define rc_alloc_struct_0(vp, typ, pstype, mem, errstat, cname)\
  rc_alloc_struct_n(vp, typ, pstype, mem, errstat, cname, 0)
#define rc_alloc_struct_1(vp, typ, pstype, mem, errstat, cname)\
  rc_alloc_struct_n(vp, typ, pstype, mem, errstat, cname, 1)

#define rc_free_struct(vp, mem, cname)\
  (*vp->rc.free)(mem, (void *)vp, cname)

/* ------ Reference counting ------ */

/* Increment a reference count. */
#define rc_increment(vp)\
  do { if ( vp != 0 ) vp->rc.ref_count++; } while (0)

/* Increment a reference count, allocating the structure if necessary. */
#define rc_allocate_struct(vp, typ, pstype, mem, errstat, cname)\
  do\
   { if ( vp != 0 )\
       vp->rc.ref_count++;\
     else\
       rc_alloc_struct_1(vp, typ, pstype, mem, errstat, cname);\
   }\
  while (0)

/* Guarantee that a structure is allocated and is not shared. */
#define rc_unshare_struct(vp, typ, pstype, mem, errstat, cname)\
  do\
   { if ( vp == 0 || vp->rc.ref_count > 1 )\
      {	typ *new;\
	rc_alloc_struct_1(new, typ, pstype, mem, errstat, cname);\
	if ( vp ) vp->rc.ref_count--;\
	vp = new;\
      }\
   }\
  while (0)

/* Adjust a reference count either up or down. */
#define rc_adjust_(vp, delta, mem, cname, body)\
  do\
   { if ( vp != 0 && !(vp->rc.ref_count += delta) )\
      {	rc_free_struct(vp, mem, cname);\
	body;\
      }\
   }\
  while (0)
#define rc_adjust(vp, delta, mem, cname)\
  rc_adjust_(vp, delta, mem, cname, vp = 0)
#define rc_adjust_only(vp, delta, mem, cname)\
  rc_adjust_(vp, delta, mem, cname, DO_NOTHING)
#define rc_adjust_const(vp, delta, mem, cname)\
  rc_adjust_only(vp, delta, mem, cname)
#define rc_decrement(vp, mem, cname)\
  rc_adjust(vp, -1, mem, cname)
#define rc_decrement_only(vp, mem, cname)\
  rc_adjust_only(vp, -1, mem, cname)

/* Assign a pointer, adjusting reference counts. */
#define rc_assign(vpto, vpfrom, mem, cname)\
  do\
   { if ( vpto != vpfrom )\
      {	rc_decrement_only(vpto, mem, cname);\
	vpto = vpfrom;\
	rc_increment(vpto);\
      }\
   }\
  while (0)
/* Adjust reference counts for assigning a pointer, */
/* but don't do the assignment.  We use this before assigning */
/* an entire structure containing reference-counted pointers. */
#define rc_pre_assign(vpto, vpfrom, mem, cname)\
  do\
   { if ( vpto != vpfrom )\
      {	rc_decrement_only(vpto, mem, cname);\
	rc_increment(vpfrom);\
      }\
   }\
  while (0)

#endif					/* gsrefct_INCLUDED */
