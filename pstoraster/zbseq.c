/* Copyright (C) 1990, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* zbseq.c */
/* Level 2 binary object sequence operators */
#include "memory_.h"
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "stream.h"
#include "strimpl.h"
#include "sfilter.h"
#include "ialloc.h"			/* for isave.h */
#include "idict.h"
#include "iname.h"
#include "isave.h"
#include "ibnum.h"
#include "btoken.h"
#include "bseq.h"
#include "store.h"

/* Current binary format (in iscan.c) */
extern ref ref_binary_object_format;

/* System and user name arrays. */
ref system_names, user_names;
private ref *system_names_p = &system_names;
private ref *user_names_p = &user_names;
private gs_gc_root_t system_names_root, user_names_root;

/* Import the Level 2 scanner extensions. */
typedef struct scanner_state_s scanner_state;
extern int scan_binary_token(P3(stream *, ref *, scanner_state *));
extern int (*scan_btoken_proc)(P3(stream *, ref *, scanner_state *));

/* Forward references */
private void store_short(P3(byte *, short, int));
private void store_long(P3(byte *, long, int));

/* Initialize the binary token machinery. */
private void
zbseq_init(void)
{	/* Initialize fake system and user name tables. */
	/* PostScript code will install the real ones. */
	make_empty_array(&system_names, a_readonly);
	gs_register_ref_root(imemory, &system_names_root,
			     (void **)&system_names_p, "system_names");
	make_empty_array(&user_names, a_all);
	gs_register_ref_root(imemory, &user_names_root,
			     (void **)&user_names_p, "user_names");
	/* Set up Level 2 scanning constants. */
	scan_btoken_proc = scan_binary_token;
}

/* <system_names> <user_names> .installnames - */
private int
zinstallnames(register os_ptr op)
{	check_read_type(op[-1], t_shortarray);
	check_type(*op, t_array);
	ref_assign_old(NULL, &system_names, op - 1, ".installnames");
	ref_assign_old(NULL, &user_names, op, ".installnames");
	pop(2);
	return 0;
}

/* - currentobjectformat <int> */
private int
zcurrentobjectformat(register os_ptr op)
{	push(1);
	*op = ref_binary_object_format;
	return 0;
}

/* <int> setobjectformat - */
private int
zsetobjectformat(register os_ptr op)
{	check_type(*op, t_integer);
	if ( op->value.intval < 0 || op->value.intval > 4 )
		return_error(e_rangecheck);
	ref_assign_old(NULL, &ref_binary_object_format, op, "setobjectformat");
	pop(1);
	return 0;
}

/*
 * The remaining operators in this file are conversion operators
 * that do the dirty work of printobject and writeobject.
 * The main control is in PostScript code, so that we don't have to worry
 * about interrupts or callouts in the middle of writing the various data
 * items.
 */

/* <top_length> <total_length> <string8> .bosheader <string4|8> */
private int
zbosheader(register os_ptr op)
{	int order = (int)ref_binary_object_format.value.intval - 1;
	long top, total;
	byte *p;
	if ( order < 0 )
		return_error(e_undefined);
	check_type(op[-2], t_integer);
	check_type(op[-1], t_integer);
	check_write_type(*op, t_string);
	if ( r_size(op) < 8 )
		return_error(e_rangecheck);
	top = op[-2].value.intval;
	total = op[-1].value.intval;
	p = op->value.bytes;
	p[0] = bt_seq + order;
	if ( top > 255 || total > 0xffff - 4 )	/* use long format */
	{	p[1] = 0;
		store_short(p + 2, top, order);
		store_long(p + 4, total + 8, order);
		r_set_size(op, 8);
	}
	else					/* use short format */
	{	p[1] = top;
		store_short(p + 2, total + 4, order);
		r_set_size(op, 4);
	}
	op[-2] = *op;
	pop(2);
	return 0;
}

/* <ref_offset> <char_offset> <obj> <string8> .bosobject */
/*   <ref_offset'> <char_offset'> <string8> */
/* This converts a single object to its binary object sequence */
/* representation.  Note that this may or may not modify the 'unused' field. */
private int
zbosobject(os_ptr op)
{	register os_ptr op1 = op - 1;
	int order = (int)ref_binary_object_format.value.intval - 1;
	bin_seq_obj ob;
	ref nstr;
	check_type(op[-3], t_integer);
	check_type(op[-2], t_integer);
	check_write_type(*op, t_string);
	if ( r_size(op) < 8 )
		return_error(e_rangecheck);
#define swap_t(a, b) t = a, a = b, b = t
#if arch_is_big_endian
#  define must_swap() (order & 1)
#else
#  define must_swap() (!(order & 1))
#endif
	switch ( r_type(op1) )
	{
	case t_null:
		ob.tx = (byte)bs_null;
		break;
	case t_mark:
		ob.tx = (byte)bs_mark;
		break;
	case t_integer:
		ob.tx = (byte)bs_integer;
		ob.value.w = op1->value.intval;
num:		ob.size.w = 0;	/* (matters for reals) */
swb:		/* swap bytes of value if needed */
		if ( must_swap() )
		{ byte t;
		  swap_t(ob.value.b[0], ob.value.b[3]);
		  swap_t(ob.value.b[1], ob.value.b[2]);
		}
		break;
	case t_real:
		ob.tx = (byte)bs_real;
		ob.value.f = op1->value.realval;
		/***** handle non-IEEE native *****/
		goto num;
	case t_boolean:
		ob.tx = (byte)bs_boolean;
		ob.value.w = op1->value.boolval;
		goto num;
	case t_array:
		ob.tx = (byte)bs_array;
		if ( r_has_attr(op1, a_executable) )
		  ob.tx += (byte)bs_executable;
		ob.size.w = r_size(op1);
		ob.value.w = op[-3].value.intval;
		op[-3].value.intval += ob.size.w * (ulong)sizeof(bin_seq_obj);
		goto nsa;
	case t_dictionary:		/* EXTENSION */
		ob.tx = (byte)bs_dictionary;
		if ( r_has_attr(op1, a_executable) )
		  ob.tx += (byte)bs_executable;
		ob.size.w = dict_length(op1) << 1;
		ob.value.w = op[-3].value.intval;
		op[-3].value.intval += ob.size.w * (ulong)sizeof(bin_seq_obj);
		goto nsa;
	case t_string:
		ob.tx = (byte)bs_string;
		if ( r_has_attr(op1, a_executable) )
		  ob.tx += (byte)bs_executable;
		ob.size.w = r_size(op1);
nos:		ob.value.w = op[-2].value.intval;
		op[-2].value.intval += ob.size.w;
nsa:		if ( must_swap() )
		{ byte t;
		  swap_t(ob.size.b[0], ob.size.b[1]);
		}
		goto swb;
	case t_name:
		ob.tx = (byte)bs_name;
		name_string_ref(op1, &nstr);
		ob.size.w = r_size(&nstr);
		goto nos;
	}
	memcpy(op->value.bytes, (byte *)&ob, sizeof(bin_seq_obj));
	op[-1] = *op;
	r_set_size(op - 1, 8);
	pop(1);
	return 0;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zbseq_l2_op_defs) {
		op_def_begin_level2(),
	{"2.installnames", zinstallnames},
	{"0currentobjectformat", zcurrentobjectformat},
	{"1setobjectformat", zsetobjectformat},
	{"3.bosheader", zbosheader},
	{"4.bosobject", zbosobject},
END_OP_DEFS(zbseq_init) }

/* ------ Internal routines ------ */

/* Put a short (16 bits). */
private void
store_short(register byte *p, short num, int order)
{	byte a = num & 0xff;
	byte b = (byte)(num >> 8);
	if ( order & 1 )
	  p[0] = a, p[1] = b;
	else
	  p[0] = b, p[1] = a;
}

/* Put a long (32 bits). */
private void
store_long(register byte *p, long num, int order)
{	byte a = num & 0xff;
	byte b = (byte)(num >> 8);
	byte c = (byte)(num >> 16);
	byte d = (byte)(num >> 24);
	if ( order & 1 )
	  p[0] = a, p[1] = b, p[2] = c, p[3] = d;
	else
	  p[0] = d, p[1] = c, p[2] = b, p[3] = a;
}
