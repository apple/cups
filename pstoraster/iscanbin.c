/* Copyright (C) 1989, 1992, 1993, 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* iscanbin.c */
/* Ghostscript binary token scanner */
#include "math_.h"
#include "memory_.h"
#include "ghost.h"
#include "gsutil.h"
#include "stream.h"
#include "strimpl.h"		/* for sfilter.h */
#include "sfilter.h"		/* for iscan.h */
#include "errors.h"
#include "ialloc.h"
#include "idict.h"
#include "dstack.h"			/* for immediately evaluated names */
#include "ostack.h"		/* must precede iscan.h */
#include "iname.h"
#include "iscan.h"			/* for scan_Refill */
#include "iutil.h"
#include "ivmspace.h"
#include "store.h"
#include "bseq.h"
#include "btoken.h"
#include "ibnum.h"

/* Define the number of required initial bytes for binary tokens. */
const byte bin_token_bytes[] = { bin_token_bytes_values };

/* Import the system and user name tables */
extern ref system_names, user_names;

/* Forward references */
private int scan_bin_num_array_continue(P3(stream *, ref *, scanner_state *));
private int scan_bin_string_continue(P3(stream *, ref *, scanner_state *));
private int scan_bos_continue(P3(stream *, ref *, scanner_state *));
private byte *scan_bos_resize(P3(scanner_state *, uint, uint));
private int scan_bos_string_continue(P3(stream *, ref *, scanner_state *));

/* Scan a binary token.  Called from the main scanner */
/* when it encounters an ASCII code 128-159, */
/* if binary tokens are being recognized (object format != 0). */
#define pbs (&pstate->s_ss.binary)
int
scan_binary_token(stream *s, ref *pref, scanner_state *pstate)
{	s_declare_inline(s, p, rlimit);
#define return_skip(n) { p += n; s_end_inline(s, p, rlimit); return 0; }
	int num_format, code;
	uint arg;
	uint wanted;
	uint rcnt;
	s_begin_inline(s, p, rlimit);
	wanted = binary_token_bytes(*p) - 1;
	rcnt = rlimit - p;
#define return_refill()\
  { sputback_inline(s, p, rlimit); s_end_inline(s, p, rlimit);\
    pstate->s_scan_type = scanning_none; return scan_Refill;\
  }
	if ( rcnt < wanted )
		return_refill();
	switch ( *p )
	   {
	case bt_seq_IEEE_msb:
		num_format = num_msb + num_float_IEEE; goto bseq;
	case bt_seq_IEEE_lsb:
		num_format = num_lsb + num_float_IEEE; goto bseq;
	case bt_seq_native_msb:
		num_format = num_msb + num_float_native; goto bseq;
	case bt_seq_native_lsb:
		num_format = num_lsb + num_float_native;
bseq:		pbs->num_format = num_format;
	  {	uint top_size = p[1];
		uint hsize, size;
		if ( top_size == 0 )
		  {	/* Extended header (2-byte array size, */
			/* 4-byte length) */
			ulong lsize;
			if ( rcnt < 7 )
				return_refill();
			top_size = sdecodeshort(p + 2, num_format);
			lsize = sdecodelong(p + 4, num_format);
			if ( (size = lsize) != lsize )
				return_error(e_limitcheck);
			hsize = 8;
		  }
		else
		  {	/* Normal header (1-byte array size, 2-byte length). */
			/* We already checked rcnt >= 3. */
			size = sdecodeshort(p + 2, num_format);
			hsize = 4;
		  }
		if ( size < hsize )
			return_error(e_syntaxerror);
		/* Preallocate an array large enough for the worst case, */
		/* namely, all objects and no strings. */
		code = ialloc_ref_array(&pbs->bin_array,
					a_all + a_executable,
					size / sizeof(ref),
					"binary object sequence(objects)");
		if ( code < 0 )
			return code;
		p += hsize - 1;
		size -= hsize;
		s_end_inline(s, p, rlimit);
		pbs->max_array_index = pbs->top_size = top_size;
		pbs->min_string_index = pbs->size = size;
		pbs->index = 0;
	  }
		pstate->s_da.is_dynamic = false;
		pstate->s_da.base = pstate->s_da.next =
		  pstate->s_da.limit = pstate->s_da.buf;
		code = scan_bos_continue(s, pref, pstate);
		if ( code == scan_Refill || code < 0 )
		  {	/* Clean up array for GC. */
			uint index = pbs->index;
			refset_null(pbs->bin_array.value.refs + index,
				    r_size(&pbs->bin_array) - index);
			pbs->cont = scan_bos_continue;
		  }
		return code;
	case bt_int32_msb:
		num_format = num_msb + num_int32; goto num;
	case bt_int32_lsb:
		num_format = num_lsb + num_int32; goto num;
	case bt_int16_msb:
		num_format = num_msb + num_int16; goto num;
	case bt_int16_lsb:
		num_format = num_lsb + num_int16; goto num;
	case bt_int8:
		make_int(pref, (p[1] ^ 128) - 128);
		return_skip(1);
	case bt_fixed:
		num_format = p[1];
		if ( !num_is_valid(num_format) )
			return_error(e_syntaxerror);
		wanted = 1 + encoded_number_bytes(num_format);
		if ( rcnt < wanted )
			return_refill();
		code = sdecode_number(p + 2, num_format, pref);
		goto rnum;
	case bt_float_IEEE_msb:
		num_format = num_msb + num_float_IEEE; goto num;
	case bt_float_IEEE_lsb:
		num_format = num_lsb + num_float_IEEE; goto num;
	case bt_float_native:
		num_format = num_float_native;
num:		code = sdecode_number(p + 1, num_format, pref);
rnum:		switch ( code )
		   {
		case t_integer: case t_real:
			r_set_type(pref, code);
			break;
		case t_null:
			return_error(e_syntaxerror);
		default:
			return code;
		   }
		return_skip(wanted);
	case bt_boolean:
		arg = p[1];
		if ( arg & ~1 )
			return_error(e_syntaxerror);
		make_bool(pref, arg);
		return_skip(1);
	case bt_string_256:
		arg = p[1]; p++; goto str;
	case bt_string_64k_msb:
		arg = (p[1] << 8) + p[2]; p += 2; goto str;
	case bt_string_64k_lsb:
		arg = p[1] + (p[2] << 8); p += 2; goto str;
str:	   {	byte *str = ialloc_string(arg, "string token");
		if ( str == 0 )
			return_error(e_VMerror);
		s_end_inline(s, p, rlimit);
		pstate->s_da.base = pstate->s_da.next = str;
		pstate->s_da.limit = str + arg;
		code = scan_bin_string_continue(s, pref, pstate);
		if ( code == scan_Refill || code < 0 )
		  {	pstate->s_da.is_dynamic = true;
			make_null(&pbs->bin_array);	/* clean up for GC */
			pbs->cont = scan_bin_string_continue;
		  }
		return code;
	   }
	case bt_litname_system:
		code = array_get(&system_names, p[1], pref);
		goto lname;
	case bt_execname_system:
		code = array_get(&system_names, p[1], pref);
		goto xname;
	case bt_litname_user:
		code = array_get(&user_names, p[1], pref);
lname:		if ( code < 0 ) return code;
		if ( !r_has_type(pref, t_name) )
			return_error(e_undefined);
		return_skip(1);
	case bt_execname_user:
		code = array_get(&user_names, p[1], pref);
xname:		if ( code < 0 ) return code;
		if ( !r_has_type(pref, t_name) )
			return_error(e_undefined);
		r_set_attrs(pref, a_executable);
		return_skip(1);
	case bt_num_array:
		num_format = p[1];
		if ( !num_is_valid(num_format) )
			return_error(e_syntaxerror);
		arg = sdecodeshort(p + 2, num_format);
		code = ialloc_ref_array(&pbs->bin_array, a_all, arg,
					"number array token");
		if ( code < 0 )
			return code;
		pbs->num_format = num_format;
		pbs->index = 0;
		p += 3;
		s_end_inline(s, p, rlimit);
		code = scan_bin_num_array_continue(s, pref, pstate);
		if ( code == scan_Refill || code < 0 )
		  {	/* Make sure the array is clean for the GC. */
			refset_null(pbs->bin_array.value.refs + pbs->index,
				    arg - pbs->index);
			pbs->cont = scan_bin_num_array_continue;
		  }
		return code;
	   }
	return_error(e_syntaxerror);
}

/* Continue collecting a binary string. */
private int
scan_bin_string_continue(stream *s, ref *pref, scanner_state *pstate)
{	byte *q = pstate->s_da.next;
	uint wanted = pstate->s_da.limit - q;
	uint rcnt;
	sgets(s, q, wanted, &rcnt);
	if ( rcnt == wanted )
	  {	/* Finished collecting the string. */
		make_string(pref, a_all | icurrent_space,
			    pstate->s_da.limit - pstate->s_da.base,
			    pstate->s_da.base);
		return 0;
	  }
	pstate->s_da.next = q + rcnt;
	pstate->s_scan_type = scanning_binary;
	return scan_Refill;
}

/* Continue scanning a binary number array. */
private int
scan_bin_num_array_continue(stream *s, ref *pref, scanner_state *pstate)
{	uint index = pbs->index;
	ref *np = pbs->bin_array.value.refs + index;
	uint wanted = encoded_number_bytes(pbs->num_format);
	for ( ; index < r_size(&pbs->bin_array); index++, np++ )
	  {	int code;
		if ( sbufavailable(s) < wanted )
		  {	pbs->index = index;
			pstate->s_scan_type = scanning_binary;
			return scan_Refill;
		  }
		code = sdecode_number(sbufptr(s), pbs->num_format, np);
		switch ( code )
		  {
		case t_integer: case t_real:
			r_set_type(np, code);
			sbufskip(s, wanted);
			break;
		case t_null:
			return_error(e_syntaxerror);
		default:
			return code;
		  }
	  }
	*pref = pbs->bin_array;
	return 0;
}

/*
 * Continue scanning a binary object sequence.  We preallocated space for
 * the largest possible number of objects, but not for strings, since
 * the latter would probably be a gross over-estimate.  Instead,
 * we wait until we see the first string or name, and allocate string space
 * based on the hope that its string index is the smallest one we will see.
 * If this turns out to be wrong, we may have to reallocate, and adjust
 * all the pointers.
 */
#define sizeof_bin_seq_obj ((uint)sizeof(bin_seq_obj))
private int
scan_bos_continue(register stream *s, ref *pref, scanner_state *pstate)
{	s_declare_inline(s, p, rlimit);
	uint max_array_index = pbs->max_array_index;
	uint min_string_index = pbs->min_string_index;
	int format = pbs->num_format;
#if arch_is_big_endian
#  define must_swap_bytes num_is_lsb(format)
#else
#  define must_swap_bytes !num_is_lsb(format)
#endif
	uint index = pbs->index;
	uint size = pbs->size;
	ref *abase = pbs->bin_array.value.refs;
	int code;
	s_begin_inline(s, p, rlimit);
	for ( ; index < max_array_index; index++ )
	   {	bin_seq_obj ob;
		byte bt;
		ref *op = abase + index;
		uint atype, sattrs;
		s_end_inline(s, p, rlimit);	/* in case of error */
		if ( rlimit - p < sizeof_bin_seq_obj )
		  {	pbs->index = index;
			pbs->max_array_index = max_array_index;
			pbs->min_string_index = min_string_index;
			pstate->s_scan_type = scanning_binary;
			return scan_Refill;
		  }
		memcpy(&ob, p + 1, sizeof_bin_seq_obj);
		p += sizeof_bin_seq_obj;
#define do_swap_size()\
  bt = ob.size.b[0], ob.size.b[0] = ob.size.b[1], ob.size.b[1] = bt
#define swap_size()\
  if ( must_swap_bytes ) do_swap_size()
#define do_swap_value()\
  bt = ob.value.b[0], ob.value.b[0] = ob.value.b[3], ob.value.b[3] = bt,\
  bt = ob.value.b[1], ob.value.b[1] = ob.value.b[2], ob.value.b[2] = bt
#define swap_value()\
  if ( must_swap_bytes ) do_swap_value()
#define swap_size_also_value()\
  if ( must_swap_bytes ) do_swap_size(), do_swap_value()
		switch ( ob.tx & 0x7f )
		  {
		case bs_null:
			make_null(op); break;
		case bs_integer:
			swap_value();
			make_int(op, ob.value.w);
			break;
		case bs_real:
			if ( ob.size.w != 0 )	/* fixed-point number */
			   {	swap_size_also_value();
				ob.value.f = (float)ldexp((float)ob.value.w,
							  -ob.size.w);
			   }
			else if ( (format & ~(num_lsb | num_msb)) !=
				  num_float_native
				)
			   {	ob.value.f = sdecodefloat(ob.value.b, format);
			   }
			make_real(op, ob.value.f);
			break;
		case bs_boolean:
			swap_value();
			make_bool(op, (ob.value.w == 0 ? 0 : 1));
			break;
		case bs_string:
			swap_size_also_value();
			sattrs = (ob.tx < 128 ? a_all : a_all + a_executable);
str:			if ( ob.size.w == 0 )
			  {	/* For zero-length strings, the offset */
				/* doesn't matter, and may be zero. */
				make_empty_string(op, sattrs);
				break;
			  }
			if ( ob.value.w < max_array_index * sizeof_bin_seq_obj ||
			    ob.value.w + ob.size.w > size
			   )
			  return_error(e_syntaxerror);
			if ( ob.value.w < min_string_index )
			  {	/* We have to (re)allocate the strings. */
				uint str_size = size - ob.value.w;
				byte *sbase;
				if ( pstate->s_da.is_dynamic )
				  sbase = scan_bos_resize(pstate, str_size,
							  index);
				else
				  sbase = ialloc_string(str_size,
							"bos strings");
				if ( sbase == 0 )
				  return_error(e_VMerror);
				pstate->s_da.is_dynamic = true;
				pstate->s_da.base = pstate->s_da.next = sbase;
				pstate->s_da.limit = sbase + str_size;
				min_string_index = ob.value.w;
			  }
			make_string(op, sattrs | icurrent_space, ob.size.w,
				    pstate->s_da.base +
				      (ob.value.w - min_string_index));
			break;
		case bs_name:
			sattrs = (ob.tx < 128 ? 0 : a_executable);
			goto nam;
		case bs_eval_name:
			sattrs = a_readonly;
nam:			swap_size_also_value();
			switch ( ob.size.w )
			   {
			case 0:
				code = array_get(&user_names, ob.value.w, op);
				goto usn;
			case 0xffff:
				code = array_get(&system_names, ob.value.w, op);
usn:				if ( code < 0 )
					return code;
				if ( !r_has_type(op, t_name) )
					return_error(e_undefined);
				r_set_attrs(op, sattrs);
				break;
			default:
				goto str;
			  }
			break;
		case bs_array:
			swap_size_also_value();
			atype = t_array;
arr:			if ( ob.value.w + ob.size.w > min_string_index ||
			    ob.value.w & (sizeof_bin_seq_obj - 1)
			   )
			  return_error(e_syntaxerror);
		  {	uint aindex = ob.value.w / sizeof_bin_seq_obj;
			max_array_index =
			  max(max_array_index, aindex + ob.size.w);
			make_tasv_new(op, atype,
				      (ob.tx < 128 ? a_all :
				       a_all + a_executable) | icurrent_space,
				      ob.size.w, refs, abase + aindex);
		  }
			break;
		case bs_dictionary:		/* EXTENSION */
			swap_size_also_value();
			if ( (ob.size.w & 1) != 0 && ob.size.w != 1 )
			  return_error(e_syntaxerror);
			atype = t_mixedarray;	/* mark as dictionary */
			goto arr;
		case bs_mark:
			make_mark(op); break;
		default:
			return_error(e_syntaxerror);
		  }
	   }
	s_end_inline(s, p, rlimit);
	/* Shorten the objects to remove the space that turned out */
	/* to be used for strings. */
	iresize_ref_array(&pbs->bin_array, max_array_index,
			  "binary object sequence(objects)");
	code = scan_bos_string_continue(s, pref, pstate);
	if ( code == scan_Refill )
	  {	pbs->cont = scan_bos_string_continue;
	  }
	return code;
}

/* Reallocate the strings for a binary object sequence, */
/* adjusting all the pointers to them from objects. */
private byte *
scan_bos_resize(scanner_state *pstate, uint new_size, uint index)
{	uint old_size = da_size(&pstate->s_da);
	byte *old_base = pstate->s_da.base;
	byte *new_base = iresize_string(old_base, old_size, new_size,
					"scan_bos_resize");
	byte *relocated_base = new_base + (new_size - old_size);
	uint i;
	ref *aptr = pbs->bin_array.value.refs;
	if ( new_base == 0 )
	  return 0;
	/* Since the allocator normally extends strings downward, */
	/* it's quite possible that new and old addresses are the same. */
	if ( relocated_base != old_base )
	  for ( i = index; i != 0; i--, aptr++ )
	    if ( r_has_type(aptr, t_string) && r_size(aptr) != 0 )
	      aptr->value.bytes =
		aptr->value.bytes - old_base + relocated_base;
	return new_base;
}

/* Continue reading the strings for a binary object sequence. */
private int
scan_bos_string_continue(register stream *s, ref *pref, scanner_state *pstate)
{	ref rstr;
	ref *op = pbs->bin_array.value.refs;
	int code = scan_bin_string_continue(s, &rstr, pstate);
	uint space = ialloc_space(idmemory);
	bool rescan = false;
	uint i;

	if ( code != 0 )
		return code;
	/* Finally, fix up names and dictionaries. */
	for ( i = r_size(&pbs->bin_array); i != 0; i--, op++ )
	  switch ( r_type(op) )
	   {
	case t_string:
		if ( r_has_attr(op, a_write) )	/* a real string */
		  break;
		/* This is actually a name; look it up now. */
	  {	uint sattrs =
		  (r_has_attr(op, a_executable) ? a_executable : 0);
		code = name_ref(op->value.bytes, r_size(op), op, 1);
		if ( code < 0 )
		  return code;
		r_set_attrs(op, sattrs);
	  }
		/* falls through */
	case t_name:
		if ( r_has_attr(op, a_read) )	/* bs_eval_name */
		   {	ref *defp = dict_find_name(op);
			if ( defp == 0 )
				return_error(e_undefined);
			store_check_space(space, defp);
			ref_assign(op, defp);
		   }
		break;
	case t_mixedarray:		/* actually a dictionary */
		{	uint count = r_size(op);
			ref rdict;
			if ( count == 1 )
			  {	/* Indirect reference. */
				if ( op->value.refs < op )
				  ref_assign(&rdict, op->value.refs);
				else
				  {	rescan = true;
					continue;
				  }
			  }
			else
			  {	code = dict_create(count >> 1, &rdict);
				if ( code < 0 )
				  return code;
				while ( count )
				  {	count -= 2;
					code = dict_put(&rdict,
							&op->value.refs[count],
							&op->value.refs[count + 1]);
					if ( code < 0 )
					  return code;
				  }
			  }
			r_set_attrs(&rdict, a_all);
			r_copy_attrs(&rdict, a_executable, op);
			ref_assign(op, &rdict);
		}
		break;
	   }
	/* If there were any forward indirect references, */
	/* fix them up now. */
	if ( rescan )
	  for ( op = pbs->bin_array.value.refs, i = r_size(&pbs->bin_array);
	        i != 0; i--, op++
	      )
	    if ( r_has_type(op, t_mixedarray) )
	      {	const ref *piref = op->value.const_refs;
		ref rdict;
		if ( r_has_type(piref, t_mixedarray) )	/* ref to indirect */
		  return_error(e_syntaxerror);
		ref_assign(&rdict, piref);
		r_copy_attrs(&rdict, a_executable, op);
		ref_assign(op, &rdict);
	      }
	ref_assign(pref, &pbs->bin_array);
	r_set_size(pref, pbs->top_size);
	return scan_BOS;
}
