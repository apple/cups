/* Copyright (C) 1989, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* ztype.c */
/* Type, attribute, and conversion operators */
#include "math_.h"
#include "memory_.h"
#include "string_.h"
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "imemory.h"			/* for register_ref_root */
#include "idict.h"
#include "iname.h"
#include "stream.h"			/* for iscan.h */
#include "strimpl.h"			/* for sfilter.h for picky compilers */
#include "sfilter.h"			/* ditto */
#include "iscan.h"
#include "iutil.h"
#include "dstack.h"			/* for dict_set_top */
#include "store.h"

/* Forward references */
private int near access_check(P3(os_ptr, int, bool));
private int convert_to_string(P2(os_ptr, os_ptr));

/*
 * Max and min integer values expressed as reals.
 * Note that these are biased by 1 to allow for truncation.
 * They should be #defines rather than static consts, but
 * several of the SCO Unix compilers apparently can't handle this.
 * On the other hand, the DEC compiler can't handle casts in
 * constant expressions, so we can't use min_long and max_long.
 * What a nuisance!
 */
#define alt_min_long (-1L << (arch_sizeof_long * 8 - 1))
#define alt_max_long (~(alt_min_long))
private const double min_int_real = (alt_min_long * 1.0 - 1);
private const double max_int_real = (alt_max_long * 1.0 + 1);
#define real_can_be_int(v)\
  ((v) > min_int_real && (v) < max_int_real)
#define zcvi_possible(v) real_can_be_int(v)

/* Get the pointer to the access flags for a ref. */
#define access_ref(opp)\
  (r_has_type(opp, t_dictionary) ? dict_access_ref(opp) : opp)

/* Initialize the table of type names. */
/* We export the type names just in case they might be useful. */
ref type_names;			/* t_array */
private ref *type_names_p = &type_names;
private gs_gc_root_t type_names_root;
private void
ztype_init(void)
{	static const char _ds *tnames[] = { type_name_strings };
	int i;
	ialloc_ref_array(&type_names, a_readonly, t_next_index,
			 "type names");
	for ( i = 0; i < t_next_index; i++ )
	{	if ( tnames[i] == 0 )
		  make_null(&type_names.value.refs[i]);
		else
		  {	name_enter_string(tnames[i],
					  &type_names.value.refs[i]);
			r_set_attrs(&type_names.value.refs[i], a_executable);
		  }
	}
	gs_register_ref_root(imemory, &type_names_root,
			     (void **)&type_names_p, "type_names");
}

/* <obj> type <name> */
private int
ztype(register os_ptr op)
{	ref *ptref = &type_names.value.refs[r_btype(op)];
	if ( !r_has_type(ptref, t_name) )
	{	/* Must be either a stack underflow or a t_[a]struct. */
		check_op(1);
	  {	/* Get the type name from the structure. */
		const char *sname =
		  gs_struct_type_name_string(gs_object_type(imemory,
							  op->value.pstruct));
		int code = name_ref((const byte *)sname, strlen(sname),
				    (ref *)op, 0);
		if ( code < 0 )
			return code;
	  }
		r_set_attrs(op, a_executable);
	}
	else
	{	ref_assign(op, ptref);
	}
	return 0;
}

/* <obj> cvlit <obj> */
private int
zcvlit(register os_ptr op)
{	ref *aop;
	check_op(1);
	aop = access_ref(op);
	r_clear_attrs(aop, a_executable);
	return 0;
}

/* <obj> cvx <obj> */
int
zcvx(register os_ptr op)
{	ref *aop;
	uint opidx;
	check_op(1);
	/*
	 * If the object is an internal operator, we can't allow it to
	 * exist in executable form anywhere outside the e-stack.
	 */
	if ( r_has_type(op, t_operator) &&
	     ((opidx = op_index(op)) == 0 ||
	      op_def_is_internal(op_def_table[opidx]))
	   )
	  return_error(e_rangecheck);
	aop = access_ref(op);
	r_set_attrs(aop, a_executable);
	return 0;
}

/* <obj> xcheck <bool> */
private int
zxcheck(register os_ptr op)
{	check_op(1);
	make_bool(op, (r_has_attr(access_ref(op), a_executable) ? 1 : 0));
	return 0;
}

/* <obj:array|packedarray|file|string> executeonly <obj> */
private int
zexecuteonly(register os_ptr op)
{	check_op(1);
	if ( r_has_type(op, t_dictionary) )
	  return_error(e_typecheck);
	return access_check(op, a_execute, true);
}

/* <obj:array|packedarray|dict|file|string> noaccess <obj> */
private int
znoaccess(register os_ptr op)
{	check_op(1);
	/* Don't allow removing read access to permanent dictionaries. */
	if ( r_has_type(op, t_dictionary) && dict_is_permanent_on_dstack(op) )
	  return_error(e_invalidaccess);
	return access_check(op, 0, true);
}

/* <obj:array|packedarray|dict|file|string> readonly <obj> */
int
zreadonly(register os_ptr op)
{	return access_check(op, a_readonly, true);
}

/* <array|packedarray|dict|file|string> rcheck <bool> */
private int
zrcheck(register os_ptr op)
{	int code = access_check(op, a_read, false);
	if ( code >= 0 ) make_bool(op, code), code = 0;
	return code;
}

/* <array|packedarray|dict|file|string> wcheck <bool> */
private int
zwcheck(register os_ptr op)
{	int code = access_check(op, a_write, false);
	if ( code >= 0 ) make_bool(op, code), code = 0;
	return code;
}

/* <num> cvi <int> */
/* <string> cvi <int> */
private int
zcvi(register os_ptr op)
{	float fval;
	switch ( r_type(op) )
	   {
	case t_integer:
		return 0;
	case t_real:
		fval = op->value.realval;
		break;
	default:
		return_op_typecheck(op);
	case t_string:
	   {	ref str, token;
		int code;
		ref_assign(&str, op);
		code = scan_string_token(&str, &token);
		switch ( code )
		  {
		  case scan_EOF:			/* no tokens */
		  case scan_BOS:			/* not allowed */
		    code = gs_note_error(e_syntaxerror);
		  default:
		    if ( code < 0 )
		      return code;
		  }
		switch ( r_type(&token) )
		  {
		  case t_integer:
		    *op = token;
		    return 0;
		  case t_real:
		    fval = token.value.realval;
		    break;
		  default:
		    return_error(e_typecheck);
		}
	   }
	   }
	/* Check if a real will fit into an integer value */
	if ( !zcvi_possible(fval) )
	  return_error(e_rangecheck);
	make_int(op, (long)fval);	/* truncates towards 0 */
	return 0;
}

/* <string> cvn <name> */
private int
zcvn(register os_ptr op)
{	check_read_type(*op, t_string);
	return name_from_string(op, op);
}

/* <num> cvr <real> */
/* <string> cvr <real> */
private int
zcvr(register os_ptr op)
{	switch ( r_type(op) )
	   {
	case t_integer:
		make_real(op, op->value.intval);
	case t_real:
		return 0;
	default:
		return_op_typecheck(op);
	case t_string:
	   {	ref str, token;
		int code;
		ref_assign(&str, op);
		code = scan_string_token(&str, &token);
		switch ( code )
		  {
		  case scan_EOF:			/* no tokens */
		  case scan_BOS:			/* not allowed */
		    code = gs_note_error(e_syntaxerror);
		  default:
		    if ( code < 0 )
		      return code;
		  }
		switch ( r_type(&token) )
		  {
		  case t_integer:
		    make_real(op, token.value.intval);
		    return 0;
		  case t_real:
		    *op = token;
		    return 0;
		  default:
		    return_error(e_typecheck);
		  }
	   }
	   }
}

/* <num> <radix_int> <string> cvrs <substring> */
private int
zcvrs(register os_ptr op)
{	int radix;
	check_type(op[-1], t_integer);
	if ( op[-1].value.intval < 2 || op[-1].value.intval > 36 )
		return_error(e_rangecheck);
	radix = op[-1].value.intval;
	check_write_type(*op, t_string);
	if ( radix == 10 )
	   {	switch ( r_type(op - 2) )
		   {
		case t_integer: case t_real:
		   {	int code = convert_to_string(op - 2, op);
			if ( code < 0 ) return code;
			pop(2);
			return 0;
		   }
		default:
			return_op_typecheck(op - 2);
		   }
	   }
	else
	   {	ulong ival;
		byte digits[sizeof(ulong) * 8];
		byte *endp = &digits[countof(digits)];
		byte *dp = endp;
		switch ( r_type(op - 2) )
		   {
		case t_integer:
			ival = (ulong)op[-2].value.intval;
			break;
		case t_real:
		   {	float fval = op[-2].value.realval;
			if ( !real_can_be_int(fval) )
				return_error(e_rangecheck);
			ival = (ulong)(long)fval;
		   }	break;
		default:
			return_op_typecheck(op - 2);
		   }
		do
		   {	int dit = ival % radix;
			*--dp = dit + (dit < 10 ? '0' : ('A' - 10));
			ival /= radix;
		   }
		while ( ival );
		if ( endp - dp > r_size(op) ) return_error(e_rangecheck);
		memcpy(op->value.bytes, dp, (uint)(endp - dp));
		r_set_size(op, endp - dp);
	   }
	op[-2] = *op;
	pop(2);
	return 0;
}

/* <any> <string> cvs <substring> */
private int
zcvs(register os_ptr op)
{	int code;
	check_op(2);
	check_write_type(*op, t_string);
	code = convert_to_string(op - 1, op);
	if ( code >= 0 ) pop(1);
	return code;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(ztype_op_defs) {
	{"1cvi", zcvi},
	{"1cvlit", zcvlit},
	{"1cvn", zcvn},
	{"1cvr", zcvr},
	{"3cvrs", zcvrs},
	{"2cvs", zcvs},
	{"1cvx", zcvx},
	{"1executeonly", zexecuteonly},
	{"1noaccess", znoaccess},
	{"1rcheck", zrcheck},
	{"1readonly", zreadonly},
	{"1type", ztype},
	{"1wcheck", zwcheck},
	{"1xcheck", zxcheck},
END_OP_DEFS(ztype_init) }

/* ------ Internal routines ------ */

/* Test or modify the access of an object. */
/* If modify = true, restrict to the selected access and return 0; */
/* if modify = false, do not change the access, and return 1 */
/* if the object had the access. */
/* Return an error code if the object is not of appropriate type, */
/* or if the object did not have the access already when modify=1. */
private int near
access_check(os_ptr op,
    int access,				/* mask for attrs */
    bool modify)				/* if true, reduce access */
{	ref *aop;
	switch ( r_type(op) )
	{
	case t_dictionary:
		aop = dict_access_ref(op);
		if ( modify )
		{	if ( !r_has_attrs(aop, access) )
				return_error(e_invalidaccess);
			ref_save(op, aop, "access_check(modify)");
			r_clear_attrs(aop, a_all);
			r_set_attrs(aop, access);
			dict_set_top();
			return 0;
		}
		break;
	case t_array: case t_file: case t_string:
	case t_mixedarray: case t_shortarray:
	case t_astruct: case t_device: ;
		if ( modify )
		{	if ( !r_has_attrs(op, access) )
			  return_error(e_invalidaccess);
			r_clear_attrs(op, a_all);
			r_set_attrs(op, access);
			return 0;
		}
		aop = op;
		break;
	default:
		return_op_typecheck(op);
	}
	return (r_has_attrs(aop, access)? 1 : 0);
}

/* Do all the work of cvs.  The destination has been checked, but not */
/* the source.  This is a separate procedure so that */
/* cvrs can use it when the radix is 10. */
private int
convert_to_string(os_ptr op1, os_ptr op)
{	uint len;
	const byte *pstr = 0;
	int code = obj_cvs(op1, op->value.bytes, r_size(op), &len, &pstr);

	if ( code < 0 )
	  {	/* Some common downloaded error handlers assume that */
		/* operator names don't exceed a certain fixed size. */
		/* To work around this bit of bad design, we implement */
		/* a special hack here: if we got a rangecheck, and */
		/* the object is an operator whose name begins with */
		/* %, ., or @, we just truncate the name. */
		if ( code == e_rangecheck )
		  switch ( r_btype(op1) )
		    {
		    case t_oparray:
		    case t_operator:
		      if ( pstr != 0 )
			switch ( *pstr )
			  {
			  case '%':
			  case '.':
			  case '@':
			    len = r_size(op);
			    memcpy(op->value.bytes, pstr, len);
			    goto ok;
			  }
		    }
		return code;
	  }
ok:	*op1 = *op;
	r_set_size(op1, len);
	return 0;
}
