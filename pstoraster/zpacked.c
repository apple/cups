/* Copyright (C) 1990, 1992, 1993 Aladdin Enterprises.  All rights reserved.
  
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

/* zpacked.c */
/* Packed array operators */
#include "ghost.h"
#include "errors.h"
#include "ialloc.h"
#include "idict.h"
#include "iname.h"
#include "istack.h"		/* for iparray.h */
#include "ipacked.h"
#include "iparray.h"
#include "ivmspace.h"
#include "oper.h"
#include "store.h"

/* Import the array packing flag */
extern ref ref_array_packing;

/* - currentpacking <bool> */
private int
zcurrentpacking(register os_ptr op)
{	push(1);
	ref_assign(op, &ref_array_packing);
	return 0;
}

/* <obj_0> ... <obj_n-1> <n> packedarray <packedarray> */
int
zpackedarray(register os_ptr op)
{	int code;
	ref parr;
	check_type(*op, t_integer);
	if ( op->value.intval < 0 ||
	     (op->value.intval > op - osbot &&
	      op->value.intval >= ref_stack_count(&o_stack))
	   )
		return_error(e_rangecheck);
	osp--;
	code = make_packed_array(&parr, &o_stack, (uint)op->value.intval,
				 "packedarray");
	osp++;
	if ( code >= 0 )
	  *osp = parr;
	return code;
}

/* <bool> setpacking - */
private int
zsetpacking(register os_ptr op)
{	check_type(*op, t_boolean);
	ref_assign_old(NULL, &ref_array_packing, op, "setpacking");
	pop(1);
	return 0;
}

/* ------ Non-operator routines ------ */

/* Make a packed array.  See the comment in packed.h about */
/* ensuring that refs in mixed arrays are properly aligned. */
int
make_packed_array(ref *parr, ref_stack *pstack, uint size, client_name_t cname)
{	uint i;
	const ref *pref;
	uint idest = 0, ishort = 0;
	ref_packed *pbody, *pdest;
	ref_packed *pshort;		/* points to start of */
					/* last run of short elements */
	uint space = ialloc_space(idmemory);
	int skip = 0, pad;
	ref rtemp;
	int code;

	/* Do a first pass to calculate the size of the array, */
	/* and to detect local-into-global stores. */

	for ( i = size; i != 0; i-- )
	{	pref = ref_stack_index(pstack, i - 1);
		switch ( r_btype(pref) )  /* not r_type, opers are special */
		{
		case t_name:
			if ( name_index(pref) >= packed_name_max_index )
				break;	/* can't pack */
			idest++;
			continue;
		case t_integer:
			if ( pref->value.intval < packed_min_intval ||
			     pref->value.intval > packed_max_intval
			   )
				break;
			idest++;
			continue;
		case t_oparray:
			/* Check for local-into-global store. */
			store_check_space(space, pref);
			/* falls through */
		case t_operator:
		{	uint oidx;
			if ( !r_has_attr(pref, a_executable) )
				break;
			oidx = op_index(pref);
			if ( oidx == 0 || oidx > packed_int_mask )
				break;
		}	idest++;
			continue;
		default:
			/* Check for local-into-global store. */
			store_check_space(space, pref);
		}
		/* Can't pack this element, use a full ref. */
		/* We may have to unpack up to align_packed_per_ref - 1 */
		/* preceding short elements. */
		/* If we are at the beginning of the array, however, */
		/* we can just move the elements up. */
		{	int i = (idest - ishort) & (align_packed_per_ref - 1);
			if ( ishort == 0 )		/* first time */
			  idest += skip = -i & (align_packed_per_ref - 1);
			else
			  idest += (packed_per_ref - 1) * i;
		}
		ishort = idest += packed_per_ref;
	}
	pad = -idest & (packed_per_ref - 1);	/* padding at end */

	/* Now we can allocate the array. */

	code = ialloc_ref_array(&rtemp, 0, (idest + pad) / packed_per_ref,
				cname);
	if ( code < 0 )
	  return code;
	pbody = (ref_packed *)rtemp.value.refs;

	/* Make sure any initial skipped elements contain legal packed */
	/* refs, so that the garbage collector can scan storage. */

	pshort = pbody;
	for ( ; skip; skip-- )
		*pbody++ = pt_tag(pt_integer);
	pdest = pbody;

	for ( i = size; i != 0; i-- )
	{	pref = ref_stack_index(pstack, i - 1);
		switch ( r_btype(pref) )	/* not r_type, opers are special */
		{
		case t_name:
		{	uint nidx = name_index(pref);
			if ( nidx >= packed_name_max_index )
				break;	/* can't pack */
			*pdest++ = nidx +
			  (r_has_attr(pref, a_executable) ?
			   pt_tag(pt_executable_name) :
			   pt_tag(pt_literal_name));
		}	continue;
		case t_integer:
			if ( pref->value.intval < packed_min_intval ||
			     pref->value.intval > packed_max_intval
			   )
				break;
			*pdest++ = pt_tag(pt_integer) +
			  ((short)pref->value.intval - packed_min_intval);
			continue;
		case t_oparray:
		case t_operator:
		{	uint oidx;
			if ( !r_has_attr(pref, a_executable) )
				break;
			oidx = op_index(pref);
			if ( oidx == 0 || oidx > packed_int_mask )
				break;
			*pdest++ = pt_tag(pt_executable_operator) + oidx;
		}	continue;
		}
		/* Can't pack this element, use a full ref. */
		/* We may have to unpack up to align_packed_per_ref - 1 */
		/* preceding short elements. */
		/* Note that if we are at the beginning of the array, */
		/* 'skip' already ensures that we don't need to do this. */
		{	int i = (pdest - pshort) & (align_packed_per_ref - 1);
			const ref_packed *psrc = pdest;
			ref *pmove =
			  (ref *)(pdest += (packed_per_ref - 1) * i);
			ref_assign_new(pmove, pref);
			while ( --i >= 0 )
			{	--psrc;
				--pmove;
				packed_get(psrc, pmove);
			}
		}
		pshort = pdest += packed_per_ref;
	}

	{	int atype =
		  (pdest == pbody + size ? t_shortarray : t_mixedarray);

		/* Pad with legal packed refs so that the garbage collector */
		/* can scan storage. */

		for ( ; pad; pad-- )
			*pdest++ = pt_tag(pt_integer);

		/* Finally, make the array. */

		ref_stack_pop(pstack, size);
		make_tasv_new(parr, atype, a_readonly | space, size,
			      packed, pbody + skip);
	}
	return 0;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zpacked_op_defs) {
	{"0currentpacking", zcurrentpacking},
	{"1packedarray", zpackedarray},
	{"1setpacking", zsetpacking},
END_OP_DEFS(0) }
