/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: idstack.h,v 1.1 2000/03/13 19:00:48 mike Exp $ */
/* Generic dictionary stack API */

#ifndef idstack_INCLUDED
#  define idstack_INCLUDED

#include "istack.h"

/* Define the dictionary stack structure. */
typedef struct dict_stack_s {

    ref_stack stack;		/* the actual stack of dictionaries */

/*
 * Switching between Level 1 and Level 2 involves inserting and removing
 * globaldict on the dictionary stack.  Instead of truly inserting and
 * removing entries, we replace globaldict by a copy of systemdict in
 * Level 1 mode.  min_dstack_size, the minimum number of entries, does not
 * change depending on language level; the countdictstack and dictstack
 * operators must take this into account.
 */
    uint min_size;		/* size of stack after clearing */

    int userdict_index;		/* index of userdict on stack */

/*
 * Cache a value for fast checking of def operations.
 * If the top entry on the dictionary stack is a writable dictionary,
 * dsspace is the space of the dictionary; if it is a non-writable
 * dictionary, dsspace = -1.  Then def is legal precisely if
 * r_space(pvalue) <= dsspace.  Note that in order for this trick to work,
 * the result of r_space must be a signed integer; some compilers treat
 * enums as unsigned, probably in violation of the ANSI standard.
 */
    int def_space;

/*
 * Cache values for fast name lookup.  If the top entry on the dictionary
 * stack is a readable dictionary with packed keys, dtop_keys, dtop_npairs,
 * and dtop_values are keys.value.packed, npairs, and values.value.refs
 * for that dictionary; otherwise, these variables point to a dummy
 * empty dictionary.
 */
    const ref_packed *top_keys;
    uint top_npairs;
    ref *top_values;

/*
 * Cache a copy of the bottom entry on the stack, which is never deleted.
 */
    ref system_dict;

} dict_stack_t;

/*
 * Reset the cached top values.  Every routine that alters the
 * dictionary stack (including changing the protection or size of the
 * top dictionary on the stack) must call this.
 */
void dstack_set_top(P1(dict_stack_t *));

/* Check whether a dictionary is one of the permanent ones on the d-stack. */
bool dstack_dict_is_permanent(P2(const dict_stack_t *, const ref *));

/* Define the type of pointers into the dictionary stack. */
typedef s_ptr ds_ptr;
typedef const_s_ptr const_ds_ptr;

/* Clean up a dictionary stack after a garbage collection. */
void dstack_gc_cleanup(P1(dict_stack_t *));

/*
 * Define a special fast entry for name lookup on a dictionary stack.
 * The key is known to be a name; search the entire dict stack.
 * Return the pointer to the value slot.
 * If the name isn't found, just return 0.
 */
ref *dstack_find_name_by_index(P2(dict_stack_t *, uint));

/*
 * Define an extra-fast macro for name lookup, optimized for
 * a single-probe lookup in the top dictionary on the stack.
 * Amazingly enough, this seems to hit over 90% of the time
 * (aside from operators, of course, which are handled either with
 * the special cache pointer or with 'bind').
 */
#define dstack_find_name_by_index_inline(pds,nidx,htemp)\
  ((pds)->top_keys[htemp = dict_hash_mod_inline(dict_name_index_hash(nidx),\
     (pds)->top_npairs) + 1] == pt_tag(pt_literal_name) + (nidx) ?\
   (pds)->top_values + htemp : dstack_find_name_by_index(pds, nidx))
/*
 * Define a similar macro that only checks the top dictionary on the stack.
 */
#define if_dstack_find_name_by_index_top(pds,nidx,htemp,pvslot)\
  if ( (((pds)->top_keys[htemp = dict_hash_mod_inline(dict_name_index_hash(nidx),\
	 (pds)->top_npairs) + 1] == pt_tag(pt_literal_name) + (nidx)) ?\
	((pvslot) = (pds)->top_values + (htemp), 1) :\
	0)\
     )

#endif /* idstack_INCLUDED */
