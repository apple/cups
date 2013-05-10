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

/* zstring.c */
/* String operators */
#include "memory_.h"
#include "ghost.h"
#include "errors.h"
#include "gsutil.h"
#include "ialloc.h"
#include "iname.h"
#include "ivmspace.h"
#include "oper.h"
#include "store.h"

/* The generic operators (copy, get, put, getinterval, putinterval, */
/* length, and forall) are implemented in zgeneric.c. */

/* <int> string <string> */
int
zstring(register os_ptr op)
{	byte *sbody;
	uint size;
	check_int_leu(*op, max_string_size);
	size = op->value.intval;
	sbody = ialloc_string(size, "string");
	if ( sbody == 0 )
	  return_error(e_VMerror);
	make_string(op, a_all | icurrent_space, size, sbody);
	memset(sbody, 0, size);
	return 0;
}

/* <name> .namestring <string> */
private int
znamestring(register os_ptr op)
{	check_type(*op, t_name);
	name_string_ref(op, op);
	return 0;
}

/* <string> <pattern> anchorsearch <post> <match> -true- */
/* <string> <pattern> anchorsearch <string> -false- */
private int
zanchorsearch(register os_ptr op)
{	os_ptr op1 = op - 1;
	uint size = r_size(op);
	check_read_type(*op1, t_string);
	check_read_type(*op, t_string);
	if ( size <= r_size(op1) && !memcmp(op1->value.bytes, op->value.bytes, size) )
	{	os_ptr op0 = op;
		push(1);
		*op0 = *op1;
		r_set_size(op0, size);
		op1->value.bytes += size;
		r_dec_size(op1, size);
		make_true(op);
	}
	else
		make_false(op);
	return 0;
}

/* <string> <pattern> search <post> <match> <pre> -true- */
/* <string> <pattern> search <string> -false- */
private int
zsearch(register os_ptr op)
{	os_ptr op1 = op - 1;
	uint size = r_size(op);
	uint count;
	byte *pat;
	byte *ptr;
	byte ch;

	check_read_type(*op1, t_string);
	check_read_type(*op, t_string);
	if ( size > r_size(op1) )		/* can't match */
	   {	make_false(op);
		return 0;
	   }
	count = r_size(op1) - size;
	ptr = op1->value.bytes;
	if ( size == 0 )
	  goto found;
	pat = op->value.bytes;
	ch = pat[0];
	do
	   {	if ( *ptr == ch && (size == 1 || !memcmp(ptr, pat, size)) )
		  goto found;
		ptr++;
	   }
	while ( count-- );
	/* No match */
	make_false(op);
	return 0;
found:	op->tas.type_attrs = op1->tas.type_attrs;
	op->value.bytes = ptr;
	r_set_size(op, size);
	push(2);
	op[-1] = *op1;
	r_set_size(op - 1, ptr - op[-1].value.bytes);
	op1->value.bytes = ptr + size;
	r_set_size(op1, count);
	make_true(op);
	return 0;
}

/* <obj> <pattern> .stringmatch <bool> */
private int
zstringmatch(register os_ptr op)
{	os_ptr op1 = op - 1;
	bool result;
	check_read_type(*op, t_string);
	switch ( r_type(op1) )
	   {
	case t_string:
		check_read(*op1);
		goto cmp;
	case t_name:
		name_string_ref(op1, op1);		/* can't fail */
cmp:		result = string_match(op1->value.const_bytes, r_size(op1),
				      op->value.const_bytes, r_size(op),
				      NULL);
		break;
	default:
		result = (r_size(op) == 1 && *op->value.bytes == '*');
	   }
	make_bool(op1, result);
	pop(1);
	return 0;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zstring_op_defs) {
	{"2anchorsearch", zanchorsearch},
	{"1.namestring", znamestring},
	{"2search", zsearch},
	{"1string", zstring},
	{"2.stringmatch", zstringmatch},
END_OP_DEFS(0) }
