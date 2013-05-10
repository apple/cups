/* Copyright (C) 1989, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zdict.c */
/* Dictionary operators */
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "idict.h"
#include "dstack.h"
#include "ilevel.h"			/* for [count]dictstack */
#include "iname.h"			/* for dict_find_name */
#include "ipacked.h"			/* for inline dict lookup */
#include "ivmspace.h"
#include "store.h"

/* <int> dict <dict> */
int
zdict(register os_ptr op)
{	check_type(*op, t_integer);
	check_int_leu(*op, dict_max_size);
	return dict_create((uint)op->value.intval, op);
}

/* <dict> maxlength <int> */
private int
zmaxlength(register os_ptr op)
{	check_type(*op, t_dictionary);
	check_dict_read(*op);
	make_int(op, dict_maxlength(op));
	return 0;
}

/* <dict> begin - */
int
zbegin(register os_ptr op)
{	check_type(*op, t_dictionary);
	check_dict_read(*op);
	if ( dsp == dstop )
	  return_error(e_dictstackoverflow);
	++dsp;
	ref_assign(dsp, op);
	dict_set_top();
	pop(1);
	return 0;
}

/* - end - */
int
zend(register os_ptr op)
{	if ( ref_stack_count_inline(&d_stack) == min_dstack_size )
	  {	/* We would underflow the d-stack. */
		return_error(e_dictstackunderflow);
	  }
	while ( dsp == dsbot )
	  {	/* We would underflow the current block. */
		ref_stack_pop_block(&d_stack);
	  }
	dsp--;
	dict_set_top();
	return 0;
}

/* <key> <value> def - */
/* We make this into a separate procedure because */
/* the interpreter will almost always call it directly. */
int
zop_def(register os_ptr op)
{	register os_ptr op1 = op - 1;
	ref *pvslot;
	/* The following combines a check_op(2) with a type check. */
	switch ( r_type(op1) )
	{
	case t_name:
	{	/* We can use the fast single-probe lookup here. */
		uint nidx = name_index(op1);
		uint htemp;
		if_dict_find_name_by_index_top(nidx, htemp, pvslot)
		{	if ( dtop_can_store(op) )
			  goto ra;
		}
		break;			/* handle all slower cases */
	}
	case t_null:
		return_error(e_typecheck);
	case t__invalid:
		return_error(e_stackunderflow);
	}
	/* Combine the check for a writable top dictionary with */
	/* the global/local store check.  See dstack.h for details. */
	if ( !dtop_can_store(op) )
	{	int code;
		check_dict_write(*dsp);
		/*
		 * If the dictionary is writable, the problem must be
		 * an invalid store.  We need a special check to allow
		 * storing references to local objects in systemdict,
		 * or in dictionaries known in systemdict,
		 * during initialization (see ivmspace.h).
		 */
		if ( ialloc_is_in_save() )
		  return_error(e_invalidaccess);
		if ( dsp->value.pdict != systemdict->value.pdict )
		  {	/* See if systemdict is still writable, */
			/* i.e., we are still doing initialization. */
			int index;
			ref elt[2];		/* key, value */
			check_dict_write(*systemdict);
			/* See if this dictionary is known in systemdict. */
			for ( index = dict_first(systemdict);
			      (index = dict_next(systemdict, index, &elt[0])) >= 0;
			    )
			  if ( r_has_type(&elt[1], t_dictionary) &&
			       elt[1].value.pdict == dsp->value.pdict
			     )
			    break;
			if ( index < 0 )
			  return_error(e_invalidaccess);
		  }
		switch ( code = dict_find(dsp, op1, &pvslot) )
		{
		case 1:				/* found */
			goto ra;
		default:			/* some other error */
			return code;
		/*
		 * If we have to grow the dictionary, do it now, so that
		 * the allocator will allocate the copy in the correct space.
		 */
		case e_dictfull:
			if ( !dict_auto_expand )
			  return_error(e_dictfull);
			code = dict_grow(dsp);
			if ( code < 0 )
			  return code;
		case 0:
			;
		}
		/* Temporarily identify the dictionary as local, */
		/* so the store check in dict_put won't fail. */
		{	uint space = r_space(dsp);
			r_set_space(dsp, avm_local);
			code = dict_put(dsp, op1, op);
			r_set_space(dsp, space);
			return code;
		}
	}
	/* Save a level of procedure call in the common (redefinition) */
	/* case.  With the current interfaces, we pay a double lookup */
	/* in the uncommon case. */
	if ( dict_find(dsp, op1, &pvslot) <= 0 )
	  return dict_put(dsp, op1, op);
ra:	ref_assign_old_inline(&dsp->value.pdict->values, pvslot, op,
			      "dict_put(value)");
	return 0;
}
int
zdef(os_ptr op)
{	int code = zop_def(op);
	if ( code >= 0 ) { pop(2); }
	return code;
}

/* <key> load <value> */
private int
zload(register os_ptr op)
{	ref *pvalue;
	switch ( r_type(op) )
	{
	case t_name:
		/* Use the fast lookup. */
		if ( (pvalue = dict_find_name(op)) == 0 )
		  return_error(e_undefined);
		ref_assign(op, pvalue);
		return 0;
	case t_null:
		return_error(e_typecheck);
	case t__invalid:
		return_error(e_stackunderflow);
	default:
	{	/* Use an explicit loop. */
		uint size = ref_stack_count(&d_stack);
		uint i;
		for ( i = 0; i < size; i++ )
		  {	ref *dp = ref_stack_index(&d_stack, i);
			check_dict_read(*dp);
			if ( dict_find(dp, op, &pvalue) > 0 )
			  {	ref_assign(op, pvalue);
				return 0;
			  }
		  }
		return_error(e_undefined);
	}
	}
}

/* get - implemented in zgeneric.c */

/* put - implemented in zgeneric.c */

/* <dict> <key> .undef - */
/* <dict> <key> undef - */
private int
zundef(register os_ptr op)
{	check_type(op[-1], t_dictionary);
	check_dict_write(op[-1]);
	dict_undef(op - 1, op);		/* ignore undefined error */
	pop(2);
	return 0;
}

/* <dict> <key> known <bool> */
private int
zknown(register os_ptr op)
{	register os_ptr op1 = op - 1;
	ref *pvalue;
	check_type(*op1, t_dictionary);
	check_dict_read(*op1);
	make_bool(op1, (dict_find(op1, op, &pvalue) > 0 ? 1 : 0));
	pop(1);
	return 0;
}

/* <key> where <dict> true */
/* <key> where false */
int
zwhere(register os_ptr op)
{	check_op(1);
	STACK_LOOP_BEGIN(&d_stack, bot, size)
	  {	const ref *pdref = bot + size;
		ref *pvalue;
		while ( pdref-- > bot )
		  {	check_dict_read(*pdref);
			if ( dict_find(pdref, op, &pvalue) > 0 )
			  {	push(1);
				ref_assign(op - 1, pdref);
				make_true(op);
				return 0;
			  }
		  }
	   }
	STACK_LOOP_END(bot, size)
	make_false(op);
	return 0;
}

/* copy for dictionaries -- called from zcopy in zgeneric.c. */
/* Only the type of *op has been checked. */
int
zcopy_dict(register os_ptr op)
{	os_ptr op1 = op - 1;
	int code;
	check_type(*op1, t_dictionary);
	check_dict_read(*op1);
	check_dict_write(*op);
	if ( !dict_auto_expand &&
	     (dict_length(op) != 0 || dict_maxlength(op) < dict_length(op1))
	   )
	  return_error(e_rangecheck);
	code = dict_copy(op1, op);
	if ( code < 0 )
	  return code;
	/*
	 * In Level 1 systems, we must copy the access attributes too.
	 * The only possible effect this can have is to make the
	 * copy read-only if the original dictionary is read-only.
	 */
	if ( !level2_enabled )
	  r_copy_attrs(dict_access_ref(op), a_write, dict_access_ref(op1));
	ref_assign(op1, op);
	pop(1);
	return 0;
}

/* - currentdict <dict> */
private int
zcurrentdict(register os_ptr op)
{	push(1);
	ref_assign(op, dsp);
	return 0;
}

/* - countdictstack <int> */
private int
zcountdictstack(register os_ptr op)
{	uint count = ref_stack_count(&d_stack);
	push(1);
	if ( !level2_enabled )
	  count--;		/* see dstack.h */
	make_int(op, count);
	return 0;
}

/* <array> dictstack <subarray> */
private int
zdictstack(register os_ptr op)
{	uint count = ref_stack_count(&d_stack);
	check_write_type(*op, t_array);
	if ( !level2_enabled )
	  count--;		/* see dstack.h */
	return ref_stack_store(&d_stack, op, count, 0, 0, true, "dictstack");
}

/* - cleardictstack - */
private int
zcleardictstack(os_ptr op)
{	while ( zend(op) >= 0 ) ;
	return 0;
}

/* ------ Extensions ------ */

/* <dict1> <dict2> .dictcopynew <dict2> */
private int
zdictcopynew(register os_ptr op)
{	os_ptr op1 = op - 1;
	int code;
	check_type(*op1, t_dictionary);
	check_dict_read(*op1);
	check_type(*op, t_dictionary);
	check_dict_write(*op);
	/* This is only recognized in Level 2 mode. */
	if ( !dict_auto_expand )
	  return_error(e_undefined);
	code = dict_copy_new(op1, op);
	if ( code < 0 )
	  return code;
	ref_assign(op1, op);
	pop(1);
	return 0;
}

/* -mark- <key0> <value0> <key1> <value1> ... .dicttomark <dict> */
/* This is the Level 2 >> operator. */
private int
zdicttomark(register os_ptr op)
{	uint count2 = ref_stack_counttomark(&o_stack);
	ref rdict;
	int code;
	uint idx;

	if ( count2 == 0 )
	  return_error(e_unmatchedmark);
	count2--;
	if ( (count2 & 1) != 0 )
	  return_error(e_rangecheck);
	if ( count2 >> 1 > dict_max_size )
	  return_error(e_rangecheck);
	code = dict_create(count2 >> 1, &rdict);
	if ( code < 0 )
	  return code;
	/* << /a 1 /a 2 >> => << /a 1 >>, i.e., */
	/* we must enter the keys in top-to-bottom order. */
	for ( idx = 0; idx < count2; idx += 2 )
	  { code = dict_put(&rdict,
			    ref_stack_index (&o_stack, idx + 1),
			    ref_stack_index (&o_stack, idx));
	    if ( code < 0 )
	      { /* There's no way to free the dictionary -- too bad. */
		return code;
	      }
	  }
	ref_stack_pop(&o_stack, count2);
	ref_assign(osp, &rdict);
	return code;
}

/* <dict> <key> <value> .forceput - */
/*
 * This forces a "put" even if the dictionary is not writable, and (if
 * the dictionary is systemdict) even if the value is in local VM.
 * It is meant to be used only for replacing the value of FontDirectory
 * in systemdict when switching between local and global VM,
 * and a few similar applications.  After initialization, this operator
 * should no longer be accessible by name.
 */
private int
zforceput(register os_ptr op)
{	os_ptr odp = op - 2;
	int code;
	check_type(*odp, t_dictionary);
	if ( odp->value.pdict == systemdict->value.pdict )
	  {	uint space = r_space(odp);
		r_set_space(odp, avm_local);
		code = dict_put(odp, op - 1, op);
		r_set_space(odp, space);
	  }
	else
	  code = dict_put(odp, op - 1, op);
	if ( code < 0 )
	  return code;
	pop(3);
	return 0;
}

/* <dict> <key> .knownget <value> true */
/* <dict> <key> .knownget false */
private int
zknownget(register os_ptr op)
{	register os_ptr op1 = op - 1;
	ref *pvalue;
	check_type(*op1, t_dictionary);
	check_dict_read(*op1);
	if ( dict_find(op1, op, &pvalue) <= 0 )
	{	make_false(op1);
		pop(1);
	}
	else
	{	ref_assign(op1, pvalue);
		make_true(op);
	}
	return 0;
}

/* <dict> <key> .knownundef <bool> */
private int
zknownundef(register os_ptr op)
{	os_ptr op1 = op - 1;
	int code;
	check_type(*op1, t_dictionary);
	check_dict_write(*op1);
	code = dict_undef(op1, op);
	make_bool(op1, code == 0);
	pop(1);
	return 0;
}

/* <dict> <int> .setmaxlength - */
private int
zsetmaxlength(register os_ptr op)
{	uint new_size;
	int code;
	os_ptr op1 = op - 1;
	check_type(*op1, t_dictionary);
	check_dict_write(*op1);
	check_type(*op, t_integer);
	check_int_leu(*op, dict_max_size);
	new_size = (uint)op->value.intval;
	if ( dict_length(op - 1) > new_size )
	  return_error(e_dictfull);
	code = dict_resize(op - 1, new_size);
	if ( code >= 0 )
	  pop(2);
	return code;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zdict_op_defs) {
	{"0cleardictstack", zcleardictstack},
	{"1begin", zbegin},
	{"0countdictstack", zcountdictstack},
	{"0currentdict", zcurrentdict},
	{"2def", zdef},
	{"1dict", zdict},
	{"0dictstack", zdictstack},
	{"0end", zend},
	{"2known", zknown},
	{"1load", zload},
	{"1maxlength", zmaxlength},
	{"2.undef", zundef},		/* we need this even in Level 1 */
	{"1where", zwhere},
		/* Extensions */
	{"2.dictcopynew", zdictcopynew},
	{"1.dicttomark", zdicttomark},
	{"3.forceput", zforceput},
	{"2.knownget", zknownget},
	{"1.knownundef", zknownundef},
	{"2.setmaxlength", zsetmaxlength},
END_OP_DEFS(0) }
