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

/*$Id: idstack.c,v 1.1 2000/03/08 23:15:10 mike Exp $ */
/* Implementation of dictionary stacks */
#include "ghost.h"
#include "idict.h"
#include "idictdef.h"
#include "idstack.h"
#include "inamedef.h"
#include "iname.h"
#include "ipacked.h"
#include "iutil.h"
#include "ivmspace.h"

/* Debugging statistics */
#ifdef DEBUG
#include "idebug.h"
long ds_lookups;		/* total lookups */
long ds_1probe;			/* successful lookups on only 1 probe */
long ds_2probe;			/* successful lookups on 2 probes */

/* Wrapper for dstack_find_name_by_index */
ref *real_dstack_find_name_by_index(P2(dict_stack_t * pds, uint nidx));
ref *
dstack_find_name_by_index(dict_stack_t * pds, uint nidx)
{
    ref *pvalue = real_dstack_find_name_by_index(pds, nidx);
    dict *pdict = pds->stack.p->value.pdict;

    ds_lookups++;
    if (dict_is_packed(pdict)) {
	uint hash =
	dict_hash_mod(dict_name_index_hash(nidx), npairs(pdict)) + 1;

	if (pdict->keys.value.packed[hash] ==
	    pt_tag(pt_literal_name) + nidx
	    )
	    ds_1probe++;
	else if (pdict->keys.value.packed[hash - 1] ==
		 pt_tag(pt_literal_name) + nidx
	    )
	    ds_2probe++;
    }
    /* Do the cheap flag test before the expensive remainder test. */
    if (gs_debug_c('d') && !(ds_lookups % 1000))
	dlprintf3("[d]lookups=%ld 1probe=%ld 2probe=%ld\n",
		  ds_lookups, ds_1probe, ds_2probe);
    return pvalue;
}
#define dstack_find_name_by_index real_dstack_find_name_by_index
#endif

/* Check whether a dictionary is one of the permanent ones on the d-stack. */
bool
dstack_dict_is_permanent(const dict_stack_t * pds, const ref * pdref)
{
    dict *pdict = pdref->value.pdict;
    int i;

    if (pds->stack.extension_size == 0) {	/* Only one block of d-stack. */
	for (i = 0; i < pds->min_size; ++i)
	    if (pds->stack.bot[i].value.pdict == pdict)
		return true;
    } else {			/* More than one block of d-stack. */
	uint count = ref_stack_count(&pds->stack);

	for (i = count - pds->min_size; i < count; ++i)
	    if (ref_stack_index(&pds->stack, i)->value.pdict == pdict)
		return true;
    }
    return false;
}

/*
 * Look up a name on the dictionary stack.
 * Return the pointer to the value if found, 0 if not.
 */
ref *
dstack_find_name_by_index(dict_stack_t * pds, uint nidx)
{
    ds_ptr pdref = pds->stack.p;

/* Since we know the hash function is the identity function, */
/* there's no point in allocating a separate variable for it. */
#define hash dict_name_index_hash(nidx)
    ref_packed kpack = packed_name_key(nidx);

    do {
	dict *pdict = pdref->value.pdict;
	uint size = npairs(pdict);

#ifdef DEBUG
	if (gs_debug_c('D')) {
	    ref dnref;

	    name_index_ref(nidx, &dnref);
	    dlputs("[D]lookup ");
	    debug_print_name(&dnref);
	    dprintf3(" in 0x%lx(%u/%u)\n",
		     (ulong) pdict, dict_length(pdref),
		     dict_maxlength(pdref));
	}
#endif
	if (dict_is_packed(pdict)) {
	    packed_search_1(DO_NOTHING,
			    return packed_search_value_pointer,
			    DO_NOTHING, goto miss);
	    packed_search_2(DO_NOTHING,
			    return packed_search_value_pointer,
			    DO_NOTHING, break);
	  miss:;
	} else {
	    ref *kbot = pdict->keys.value.refs;
	    register ref *kp;
	    int wrap = 0;

	    /* Search the dictionary */
	    for (kp = kbot + dict_hash_mod(hash, size) + 2;;) {
		--kp;
		if (r_has_type(kp, t_name)) {
		    if (name_index(kp) == nidx)
			return pdict->values.value.refs +
			    (kp - kbot);
		} else if (r_has_type(kp, t_null)) {	/* Empty, deleted, or wraparound. */
		    /* Figure out which. */
		    if (!r_has_attr(kp, a_executable))
			break;
		    if (kp == kbot) {	/* wrap */
			if (wrap++)
			    break;	/* 2 wraps */
			kp += size + 1;
		    }
		}
	    }
	}
    }
    while (pdref-- > pds->stack.bot);
    /* The name isn't in the top dictionary block. */
    /* If there are other blocks, search them now (more slowly). */
    if (!pds->stack.extension_size)	/* no more blocks */
	return (ref *) 0;
    {				/* We could use the STACK_LOOP macros, but for now, */
	/* we'll do things the simplest way. */
	ref key;
	uint i = pds->stack.p + 1 - pds->stack.bot;
	uint size = ref_stack_count(&pds->stack);
	ref *pvalue;

	name_index_ref(nidx, &key);
	for (; i < size; i++) {
	    if (dict_find(ref_stack_index(&pds->stack, i),
			  &key, &pvalue) > 0
		)
		return pvalue;
	}
    }
    return (ref *) 0;
#undef hash
}

/* Set the cached values computed from the top entry on the dstack. */
/* See idstack.h for details. */
private const ref_packed no_packed_keys[2] =
{packed_key_deleted, packed_key_empty};
void
dstack_set_top(dict_stack_t * pds)
{
    ds_ptr dsp = pds->stack.p;
    dict *pdict = dsp->value.pdict;

    if_debug3('d', "[d]dsp = 0x%lx -> 0x%lx, key array type = %d\n",
	      (ulong) dsp, (ulong) pdict, r_type(&pdict->keys));
    if (dict_is_packed(pdict) &&
	r_has_attr(dict_access_ref(dsp), a_read)
	) {
	pds->top_keys = pdict->keys.value.packed;
	pds->top_npairs = npairs(pdict);
	pds->top_values = pdict->values.value.refs;
    } else {
	pds->top_keys = no_packed_keys;
	pds->top_npairs = 1;
    }
    if (!r_has_attr(dict_access_ref(dsp), a_write))
	pds->def_space = -1;
    else
	pds->def_space = r_space(dsp);
}

/* After a garbage collection, scan the permanent dictionaries and */
/* update the cached value pointers in names. */
void
dstack_gc_cleanup(dict_stack_t * pds)
{
    uint count = ref_stack_count(&pds->stack);
    uint dsi;

    for (dsi = pds->min_size; dsi > 0; --dsi) {
	const dict *pdict =
	ref_stack_index(&pds->stack, count - dsi)->value.pdict;
	uint size = nslots(pdict);
	ref *pvalue = pdict->values.value.refs;
	uint i;

	for (i = 0; i < size; ++i, ++pvalue) {
	    ref key;
	    ref *old_pvalue;

	    array_get(&pdict->keys, (long)i, &key);
	    if (r_has_type(&key, t_name) &&
		pv_valid(old_pvalue = key.value.pname->pvalue)
		) {		/*
				 * The name only has a single definition,
				 * so it must be this one.  Check to see if
				 * no relocation is actually needed; if so,
				 * we can skip the entire dictionary.
				 */
		if (old_pvalue == pvalue) {
		    if_debug1('d', "[d]skipping dstack entry %d\n",
			      dsi - 1);
		    break;
		}
		/* Update the value pointer. */
		key.value.pname->pvalue = pvalue;
	    }
	}
    }
}
