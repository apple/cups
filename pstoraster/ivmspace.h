/* Copyright (C) 1992, 1993, 1994, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: ivmspace.h,v 1.2 2000/03/08 23:15:17 mike Exp $ */
/* Local/global space management */
/* Requires iref.h */

#ifndef ivmspace_INCLUDED
#  define ivmspace_INCLUDED

#include "gsgc.h"

/*
 * r_space_bits and r_space_shift, which define the bits in a ref
 * that carry VM space information, are defined in iref.h.
 * r_space_bits must be at least 2.
 */
#define a_space (((1 << r_space_bits) - 1) << r_space_shift)
/*
 * The i_vm_xxx values are defined in gsgc.h.
 */
typedef enum {
    avm_foreign = (i_vm_foreign << r_space_shift),
    avm_system = (i_vm_system << r_space_shift),
    avm_global = (i_vm_global << r_space_shift),
    avm_local = (i_vm_local << r_space_shift),
    avm_max = avm_local
} avm_space;

#define r_space(rp) (avm_space)(r_type_attrs(rp) & a_space)
#define r_space_index(rp) ((int)r_space(rp) >> r_space_shift)
#define r_set_space(rp,space) r_store_attrs(rp, a_space, (uint)space)

/*
 * According to the PostScript language specification, attempting to store
 * a reference to a local object into a global object must produce an
 * invalidaccess error.  However, systemdict must be able to refer to
 * a number of local dictionaries such as userdict and errordict.
 * Therefore, we implement a special hack in 'def' that allows such stores
 * if the dictionary being stored into is systemdict (which is normally
 * only writable during initialization) or a dictionary that appears
 * in systemdict (such as level2dict), and the current save level is zero
 * (to guarantee that we can't get dangling pointers).
 * We could allow this for any global dictionary, except that the garbage
 * collector must treat any such dictionaries as roots when collecting
 * local VM without collecting global VM.
 * We make a similar exception for .makeglobaloperator; this requires
 * treating the operator table as a GC root as well.
 *
 * We extend the local-into-global store check because we have four VM
 * spaces (local, global, system, and foreign), and we allow PostScript
 * programs to create objects in any of the first three.  If we define
 * the "generation" of an object as foreign = 0, system = 1, global = 2,
 * and local = 3, then a store is legal iff the generation of the object
 * into which a pointer is being stored is greater than or equal to
 * the generation of the object into which the store is occurring.
 *
 * We must check for local-into-global stores in three categories of places:
 *
 *      - The scanner, when it encounters a //name inside {}.
 *
 *      - All operators that allocate ref-containing objects and also
 *      store into them:
 *              packedarray  gstate  makepattern?
 *              makefont  scalefont  definefont  filter
 *
 *      - All operators that store refs into existing objects
 *      ("operators" marked with * are actually PostScript procedures):
 *              put(array)  putinterval(array)  astore  copy(to array)
 *              def  store*  put(dict)  copy(dict)
 *              dictstack  execstack  .make(global)operator
 *              currentgstate  defineusername
 */

/* Test whether an object is in local space, */
/* which implies that we need not check when storing into it. */
#define r_is_local(rp) (r_space(rp) == avm_local)
/* Test whether an object is foreign, i.e., outside known space. */
#define r_is_foreign(rp) (r_space(rp) == avm_foreign)
/* Check whether a store is allowed. */
#define store_check_space(destspace,rpnew)\
  if ( r_space(rpnew) > (destspace) )\
    return_error(e_invalidaccess)
#define store_check_dest(rpdest,rpnew)\
  store_check_space(r_space(rpdest), rpnew)
/* BACKWARD COMPATIBILITY (not used by any Ghostscript code per se) */
#define check_store_space(rdest,rnewcont)\
  store_check_dest(&(rdest),&(rnewcont))

#endif /* ivmspace_INCLUDED */
