/* Copyright (C) 1993, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* iparam.c */
/* Interpreter implementations of parameter dictionaries */
#include "memory_.h"
#include "string_.h"
#include "ghost.h"
#include "errors.h"
#include "opcheck.h"
#include "ialloc.h"
#include "idict.h"
#include "imemory.h"			/* for iutil.h */
#include "iname.h"
#include "istack.h"
#include "iparam.h"
#include "iutil.h"			/* for num_params */
#include "ivmspace.h"
#include "store.h"

/* ================ Utilities ================ */

#define iplist ((iparam_list *)plist)
#define ciplist ((const iparam_list *)plist)

/* Convert a key to a ref. */
private int near
ref_param_key(const iparam_list *plist, gs_param_name pkey, ref *pkref)
{	if ( plist->int_keys )
	  {	long key;
		if ( sscanf(pkey, "%ld", &key) != 1 )
		  return_error(e_rangecheck);
		make_int(pkref, key);
		return 0;
	  }
	else
	  {	return name_ref((const byte *)pkey, strlen(pkey), pkref, 0);
	  }
}

/* ================ Writing parameters to refs ================ */

/* ---------------- Generic writing procedures ---------------- */

private param_proc_xmit_null(ref_param_write_null);
private param_proc_xmit_bool(ref_param_write_bool);
private param_proc_xmit_int(ref_param_write_int);
private param_proc_xmit_long(ref_param_write_long);
private param_proc_xmit_float(ref_param_write_float);
private param_proc_xmit_string(ref_param_write_string);
private param_proc_xmit_name(ref_param_write_name);
private param_proc_xmit_int_array(ref_param_write_int_array);
private param_proc_xmit_float_array(ref_param_write_float_array);
private param_proc_xmit_string_array(ref_param_write_string_array);
private param_proc_xmit_name_array(ref_param_write_name_array);
private param_proc_begin_xmit_dict(ref_param_begin_write_dict);
private param_proc_end_xmit_dict(ref_param_end_write_dict);
private param_proc_requested(ref_param_requested);
private const gs_param_list_procs ref_write_procs = {
	ref_param_write_null,
	ref_param_write_bool,
	ref_param_write_int,
	ref_param_write_long,
	ref_param_write_float,
	ref_param_write_string,
	ref_param_write_name,
	ref_param_write_int_array,
	ref_param_write_float_array,
	ref_param_write_string_array,
	ref_param_write_name_array,
	ref_param_begin_write_dict,
	ref_param_end_write_dict,
	ref_param_requested
};
private int near ref_array_param_requested(P5(const gs_param_list *, gs_param_name, ref *, uint, client_name_t));
private int near ref_param_write(P3(iparam_list *, gs_param_name, const ref *));
private int near ref_param_write_string_value(P2(ref *,
  const gs_param_string *));
#define ref_param_write_name_value(pref, pvalue)\
  name_ref((pvalue)->data, (pvalue)->size, pref,\
	   ((pvalue)->persistent ? 0 : 1))

private int
ref_param_write_null(gs_param_list *plist, gs_param_name pkey)
{	ref value;
	make_null(&value);
	return ref_param_write(iplist, pkey, &value);
}
private int
ref_param_write_bool(gs_param_list *plist, gs_param_name pkey, bool *pvalue)
{	ref value;
	make_bool(&value, *pvalue);
	return ref_param_write(iplist, pkey, &value);
}
private int
ref_param_write_int(gs_param_list *plist, gs_param_name pkey, int *pvalue)
{	ref value;
	make_int(&value, *pvalue);
	return ref_param_write(iplist, pkey, &value);
}
private int
ref_param_write_long(gs_param_list *plist, gs_param_name pkey, long *pvalue)
{	ref value;
	make_int(&value, *pvalue);
	return ref_param_write(iplist, pkey, &value);
}
private int
ref_param_write_float(gs_param_list *plist, gs_param_name pkey, float *pvalue)
{	ref value;
	make_real(&value, *pvalue);
	return ref_param_write(iplist, pkey, &value);
}
private int
ref_param_write_string(gs_param_list *plist, gs_param_name pkey,
  gs_param_string *pvalue)
{	ref sref;
	int code;

	if ( !ref_param_requested(plist, pkey) )
	  return 0;
	code = ref_param_write_string_value(&sref, pvalue);
	if ( code < 0 )
	  return code;
	return ref_param_write(iplist, pkey, &sref);
}
private int
ref_param_write_name(gs_param_list *plist, gs_param_name pkey,
  gs_param_string *pvalue)
{	ref nref;
	int code;

	if ( !ref_param_requested(plist, pkey) )
	  return 0;
	code = ref_param_write_name_value(&nref, pvalue);
	if ( code < 0 )
	  return code;
	return ref_param_write(iplist, pkey, &nref);
}
private int
ref_param_write_int_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_int_array *pvalue)
{	ref value;
	const int *pdata = pvalue->data;
	uint n = pvalue->size;
	ref *pe;
	int code;

	if ( (code = ref_array_param_requested(plist, pkey, &value, n,
				"ref_param_write_int_array")) <= 0 )
	  return code;
	for ( pe = value.value.refs; n > 0; n--, pe++, pdata++ )
	  make_int_new(pe, *pdata);
	return ref_param_write(iplist, pkey, &value);
}
private int
ref_param_write_float_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_float_array *pvalue)
{	ref value;
	const float *pdata = pvalue->data;
	uint n = pvalue->size;
	int code;
	ref *pe;

	if ( (code = ref_array_param_requested(plist, pkey, &value, n,
				"ref_param_write_float_array")) <= 0 )
	  return code;
	for ( pe = value.value.refs; n > 0; n--, pe++, pdata++ )
	  make_real_new(pe, *pdata);
	return ref_param_write(iplist, pkey, &value);
}
private int
ref_param_write_string_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_string_array *pvalue)
{	ref value;
	const gs_param_string *pdata = pvalue->data;
	uint n = pvalue->size;
	int code;
	ref *pe;

	if ( (code = ref_array_param_requested(plist, pkey, &value, n,
				"ref_param_write_string_array")) <= 0 )
	  return code;
	for ( pe = value.value.refs; n > 0; n--, pe++, pdata++ )
	  {	code = ref_param_write_string_value(pe, pdata);
		if ( code < 0 )
		  {	/* Don't bother trying to release memory. */
			return code;
		  }
	  }
	return ref_param_write(iplist, pkey, &value);
}
private int
ref_param_write_name_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_string_array *pvalue)
{	ref value;
	const gs_param_string *pdata = pvalue->data;
	uint n = pvalue->size;
	int code;
	ref *pe;

	if ( (code = ref_array_param_requested(plist, pkey, &value, n,
				"ref_param_write_name_array")) <= 0 )
	  return code;
	for ( pe = value.value.refs; n > 0; n--, pe++, pdata++ )
	  {	code = ref_param_write_name_value(pe, pdata);
		if ( code < 0 )
		  {	/* Don't bother trying to release memory. */
			return code;
		  }
	  }
	return ref_param_write(iplist, pkey, &value);
}
private int
ref_param_begin_write_dict(gs_param_list *plist, gs_param_name pkey,
  gs_param_dict *pvalue, bool int_keys)
{	dict_param_list *dlist =
	  (dict_param_list *)ialloc_bytes(size_of(dict_param_list),
					  "ref_param_begin_write_dict");
	ref dref;
	int code;

	if ( dlist == 0 )
	  return_error(e_VMerror);
	code = dict_create(pvalue->size, &dref);
	if ( code < 0 )
	  {	ifree_object(dlist, "ref_param_begin_write_dict");
		return code;
	  }
	pvalue->list = (gs_param_list *)dlist;
	code = dict_param_list_write(dlist, &dref, NULL);
	if ( code < 0 )
	  return code;
	dlist->int_keys = int_keys;
	return 0;
}
private int
ref_param_end_write_dict(gs_param_list *plist, gs_param_name pkey,
  gs_param_dict *pvalue)
{	int code = ref_param_write(iplist, pkey,
				   &((dict_param_list *)pvalue->list)->dict);
	ifree_object(pvalue->list, "ref_param_end_write_dict");
	return code;
}

/* Check whether a given parameter was requested. */
private bool
ref_param_requested(const gs_param_list *plist, gs_param_name pkey)
{	ref kref;
	ref *ignore_value;
	if ( !r_has_type(&ciplist->u.w.wanted, t_dictionary) )
	  return true;
	if ( ref_param_key(ciplist, pkey, &kref) < 0 )
	  return true;		/* catch it later */
	return (dict_find(&ciplist->u.w.wanted, &kref, &ignore_value) > 0);
}
/* Check whether an array parameter is wanted, and allocate it if so. */
/* Return <0 on error, 0 if not wanted, 1 if wanted. */
private int near
ref_array_param_requested(const gs_param_list *plist, gs_param_name pkey,
  ref *pvalue, uint size, client_name_t cname)
{	int code;
	if ( !ref_param_requested(plist, pkey) )
	  return 0;
	code = ialloc_ref_array(pvalue, a_all, size, cname);
	return (code < 0 ? code : 1);
}

/* ---------------- Internal routines ---------------- */

/* Prepare to write a string value. */
private int near
ref_param_write_string_value(ref *pref, const gs_param_string *pvalue)
{	const byte *pdata = pvalue->data;
	uint n = pvalue->size;

	if ( pvalue->persistent )
		make_const_string(pref, a_readonly | avm_foreign, n, pdata);
	else
	{	byte *pstr = ialloc_string(n, "ref_param_write_string");
		if ( pstr == 0 )
		  return_error(e_VMerror);
		memcpy(pstr, pdata, n);
		make_string(pref, a_readonly | icurrent_space, n, pstr);
	}
	return 0;
}

/* Generic routine for writing a ref parameter. */
private int near
ref_param_write(iparam_list *plist, gs_param_name pkey, const ref *pvalue)
{	ref kref;
	int code;
	if ( !ref_param_requested((gs_param_list *)plist, pkey) )
	  return 0;
	code = ref_param_key(plist, pkey, &kref);
	if ( code < 0 )
	  return code;
	return (*plist->u.w.write)(plist, &kref, pvalue);
}

/* ---------------- Implementations ---------------- */

/* Initialize for writing parameters. */
private void near
ref_param_write_init(iparam_list *plist, const ref *pwanted)
{	if ( pwanted == 0 )
	  make_null(&plist->u.w.wanted);
	else
	  plist->u.w.wanted = *pwanted;
	plist->results = 0;
	plist->int_keys = false;
}

/* Implementation for getting parameters to a stack. */
private int
stack_param_write(iparam_list *plist, const ref *pkey, const ref *pvalue)
{
#define splist ((stack_param_list *)plist)
	ref_stack *pstack = splist->pstack;
	s_ptr p = pstack->p;
	if ( pstack->top - p < 2 )
	  { int code = ref_stack_push(pstack, 2);
	    if ( code < 0 )
	      return code;
	    *ref_stack_index(pstack, 1) = *pkey;
	    p = pstack->p;
	  }
	else
	  { pstack->p = p += 2;
	    p[-1] = *pkey;
	  }
	*p = *pvalue;
	splist->count++;
#undef splist
	return 0;
}
int
stack_param_list_write(stack_param_list *plist, ref_stack *pstack,
  const ref *pwanted)
{	plist->procs = &ref_write_procs;
	plist->u.w.write = stack_param_write;
	ref_param_write_init((iparam_list *)plist, pwanted);
	plist->pstack = pstack;
	/* plist->skip not used */
	plist->count = 0;
	return 0;
}	

/* Implementation for getting parameters to a dictionary. */
private int
dict_param_write(iparam_list *plist, const ref *pkey, const ref *pvalue)
{	int code = dict_put(&((dict_param_list *)plist)->dict, pkey, pvalue);
	return min(code, 0);
}
int
dict_param_list_write(dict_param_list *plist, ref *pdict,
  const ref *pwanted)
{	check_dict_write(*pdict);
	plist->procs = &ref_write_procs;
	plist->u.w.write = dict_param_write;
	ref_param_write_init((iparam_list *)plist, pwanted);
	plist->dict = *pdict;
	return 0;
}	

/* ================ Reading refs to parameters ================ */

/* ---------------- Generic reading procedures ---------------- */

private param_proc_xmit_null(ref_param_read_null);
private param_proc_xmit_bool(ref_param_read_bool);
private param_proc_xmit_int(ref_param_read_int);
private param_proc_xmit_long(ref_param_read_long);
private param_proc_xmit_float(ref_param_read_float);
private param_proc_xmit_string(ref_param_read_string);
private param_proc_xmit_int_array(ref_param_read_int_array);
private param_proc_xmit_float_array(ref_param_read_float_array);
private param_proc_xmit_string_array(ref_param_read_string_array);
private param_proc_begin_xmit_dict(ref_param_begin_read_dict);
private param_proc_end_xmit_dict(ref_param_end_read_dict);
private param_proc_get_policy(ref_param_read_get_policy);
private param_proc_signal_error(ref_param_read_signal_error);
private param_proc_commit(ref_param_read_commit);
private const gs_param_list_procs ref_read_procs = {
	ref_param_read_null,
	ref_param_read_bool,
	ref_param_read_int,
	ref_param_read_long,
	ref_param_read_float,
	ref_param_read_string,
	ref_param_read_string,		/* name = string */
	ref_param_read_int_array,
	ref_param_read_float_array,
	ref_param_read_string_array,
	ref_param_read_string_array,	/* name = string */
	ref_param_begin_read_dict,
	ref_param_end_read_dict,
	NULL,				/* requested */
	ref_param_read_get_policy,
	ref_param_read_signal_error,
	ref_param_read_commit
};
private int near ref_param_read(P4(iparam_list *, gs_param_name,
  iparam_loc *, int));
private int near ref_param_read_string_value(P2(const iparam_loc *,
  gs_param_string *));
private int near ref_param_read_array(P3(iparam_list *, gs_param_name,
  iparam_loc *));
#define iparam_return_error(loc, code)\
  return_error(*(loc).presult = code)
#define iparam_check_type(loc, typ)\
  if ( !r_has_type((loc).pvalue, typ) )\
    iparam_return_error(loc, e_typecheck)
#define iparam_check_read(loc)\
  if ( !r_has_attr((loc).pvalue, a_read) )\
    iparam_return_error(loc, e_invalidaccess)

private int
ref_param_read_null(gs_param_list *plist, gs_param_name pkey)
{	iparam_loc loc;
	return ref_param_read(iplist, pkey, &loc, t_null);
}
private int
ref_param_read_bool(gs_param_list *plist, gs_param_name pkey, bool *pvalue)
{	iparam_loc loc;
	int code = ref_param_read(iplist, pkey, &loc, t_boolean);
	if ( code != 0 )
	  return code;
	*pvalue = loc.pvalue->value.boolval;
	return 0;
}
private int
ref_param_read_int(gs_param_list *plist, gs_param_name pkey, int *pvalue)
{	iparam_loc loc;
	int code = ref_param_read(iplist, pkey, &loc, t_integer);
	if ( code != 0 )
	  return code;
#if arch_sizeof_int < arch_sizeof_long
	if ( loc.pvalue->value.intval != (int)loc.pvalue->value.intval )
	  return_error(e_rangecheck);
#endif
	*pvalue = (int)loc.pvalue->value.intval;
	return 0;
}
private int
ref_param_read_long(gs_param_list *plist, gs_param_name pkey, long *pvalue)
{	iparam_loc loc;
	int code = ref_param_read(iplist, pkey, &loc, t_integer);
	if ( code != 0 )
	  return code;
	*pvalue = loc.pvalue->value.intval;
	return 0;
}
private int
ref_param_read_float(gs_param_list *plist, gs_param_name pkey, float *pvalue)
{	iparam_loc loc;
	int code = ref_param_read(iplist, pkey, &loc, -1);
	if ( code != 0 )
	  return code;
	switch ( r_type(loc.pvalue) )
	{
	case t_integer:
		*pvalue = loc.pvalue->value.intval;
		break;
	case t_real:
		*pvalue = loc.pvalue->value.realval;
		break;
	default:
		iparam_return_error(loc, e_typecheck);
	}
	return 0;
}
private int
ref_param_read_string(gs_param_list *plist, gs_param_name pkey,
  gs_param_string *pvalue)
{	iparam_loc loc;
	int code = ref_param_read(iplist, pkey, &loc, -1);
	if ( code != 0 )
	  return code;
	return ref_param_read_string_value(&loc, pvalue);
}
private int
ref_param_read_int_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_int_array *pvalue)
{	iparam_loc loc;
	int code = ref_param_read_array(iplist, pkey, &loc);
	int *piv;
	uint size;
	uint i;

	if ( code != 0 )
	  return code;
	size = r_size(loc.pvalue);
	piv = (int *)ialloc_byte_array(size, sizeof(int),
				       "ref_param_read_int_array");
	if ( piv == 0 )
	  return_error(e_VMerror);
	for ( i = 0; i < size; i++ )
	{	const ref *pe = loc.pvalue->value.const_refs + i;
		if ( !r_has_type(pe, t_integer) )
		  { code = gs_note_error(e_typecheck); break; }
#if arch_sizeof_int < arch_sizeof_long
		if ( pe->value.intval != (int)pe->value.intval )
		  { code = gs_note_error(e_rangecheck); break; }
#endif
		piv[i] = (int)pe->value.intval;
	}
	if ( code < 0 )
	  {	ifree_object(piv, "ref_param_read_int_array");
		return (*loc.presult = code);
	  }
	pvalue->data = piv;
	pvalue->size = size;
	pvalue->persistent = true;
	return 0;
}
private int
ref_param_read_float_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_float_array *pvalue)
{	iparam_loc loc;
	int code = ref_param_read_array(iplist, pkey, &loc);
	float *pfv;
	uint size;

	if ( code != 0 )
	  return code;
	size = r_size(loc.pvalue);
	pfv = (float *)ialloc_byte_array(size, sizeof(float),
					 "ref_param_read_float_array");
	if ( pfv == 0 )
	  return_error(e_VMerror);
	code = num_params(loc.pvalue->value.const_refs + size - 1, size, pfv);
	if ( code < 0 )
	{	ifree_object(pfv, "ref_read_float_array_param");
		return (*loc.presult = code);
	}
	pvalue->data = pfv;
	pvalue->size = size;
	pvalue->persistent = true;
	return 0;
}
private int
ref_param_read_string_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_string_array *pvalue)
{	iparam_loc loc;
	int code = ref_param_read_array(iplist, pkey, &loc);
	gs_param_string *psv;
	ref *prefs;
	uint size;
	uint i;

	if ( code != 0 )
	  return code;
	prefs = loc.pvalue->value.refs;
	size = r_size(loc.pvalue);
	psv = (gs_param_string *)ialloc_byte_array(size, sizeof(gs_param_string),
						   "ref_param_read_string_array");
	if ( psv == 0 )
	  return_error(e_VMerror);
	for ( i = 0; code >= 0 && i < size; i++ )
	{	loc.pvalue = prefs + i;
		code = ref_param_read_string_value(&loc, psv + i);
	}
	if ( code < 0 )
	{	ifree_object(psv, "ref_param_read_string_array");
		return (*loc.presult = code);
	}
	pvalue->data = psv;
	pvalue->size = size;
	pvalue->persistent = true;
	return 0;
}
private int
ref_param_begin_read_dict(gs_param_list *plist, gs_param_name pkey,
  gs_param_dict *pvalue, bool int_keys)
{	iparam_loc loc;
	dict_param_list *dlist =
	  (dict_param_list *)ialloc_bytes(size_of(dict_param_list),
					  "ref_param_begin_write_dict");
	int code = ref_param_read(iplist, pkey, &loc, t_dictionary);

	if ( code != 0 )
	  return code;
	code = dict_param_list_read(dlist, loc.pvalue, NULL, false);
	if ( code < 0 )
	  iparam_return_error(loc, code);
	dlist->int_keys = int_keys;
	pvalue->list = (gs_param_list *)dlist;
	pvalue->size = dict_length(loc.pvalue);
	return 0;
}
private int
ref_param_end_read_dict(gs_param_list *plist, gs_param_name pkey,
  gs_param_dict *pvalue)
{	iparam_list_release((dict_param_list *)pvalue->list);
	ifree_object(pvalue->list, "ref_param_end_read_dict");
	return 0;
}
private int
ref_param_read_get_policy(gs_param_list *plist, gs_param_name pkey)
{	ref kname;
	int code;
	ref *pvalue;
	/* We can't use dict_find_string directly here, because */
	/* pkey might not be a _ds string. */
	if ( !(r_has_type(&iplist->u.r.policies, t_dictionary) &&
	       (code = name_ref((const byte *)pkey, strlen(pkey), &kname, -1)) >= 0 &&
	       dict_find(&iplist->u.r.policies, &kname, &pvalue) > 0 &&
	       r_has_type(pvalue, t_integer))
	   )
	  return gs_param_policy_ignore;
	return (int)pvalue->value.intval;
}
private int
ref_param_read_signal_error(gs_param_list *plist, gs_param_name pkey, int code)
{	iparam_loc loc;
	ref_param_read(iplist, pkey, &loc, -1);	/* can't fail */
	*loc.presult = code;
	switch ( ref_param_read_get_policy(plist, pkey) )
	  {
	  case gs_param_policy_ignore:
	    return 0;
	  case gs_param_policy_consult_user:
	    return_error(e_configurationerror);
	  default:
	    return code;
	  }
}
private int
ref_param_read_commit(gs_param_list *plist)
{	int i;
	int ecode = 0;
	if ( !iplist->u.r.require_all )
	  return 0;
	/* Check to make sure that all parameters were actually read. */
	for ( i = 0; i < iplist->count; ++i )
	  if ( iplist->results[i] == 0 )
	    iplist->results[i] = ecode = gs_note_error(e_undefined);
	return ecode;
}

/* ---------------- Internal routines ---------------- */

/* Read a string value. */
private int near
ref_param_read_string_value(const iparam_loc *ploc, gs_param_string *pvalue)
{	const ref *pref = ploc->pvalue;
	ref nref;
	switch ( r_type(pref) )
	{
	case t_name:
		name_string_ref(pref, &nref);
		pref = &nref;
		pvalue->persistent = true;
		goto s;
	case t_string:
		iparam_check_read(*ploc);
		pvalue->persistent = false;
s:		pvalue->data = pref->value.const_bytes;
		pvalue->size = r_size(pref);
		break;
	default:
		iparam_return_error(*ploc, e_typecheck);
	}
	return 0;
}

/* Read an array parameter. */
private int near
ref_param_read_array(iparam_list *plist, gs_param_name pkey, iparam_loc *ploc)
{	int code = ref_param_read(plist, pkey, ploc, t_array);
	if ( code != 0 )
	  return code;
	iparam_check_read(*ploc);
	return 0;
}

/* Generic routine for reading a ref parameter. */
private int near
ref_param_read(iparam_list *plist, gs_param_name pkey, iparam_loc *ploc,
  int type)
{	ref kref;
	int code = ref_param_key(plist, pkey, &kref);
	if ( code < 0 )
	  return code;
	code = (*plist->u.r.read)(iplist, &kref, ploc);
	if ( code != 0 )
	  return code;
	if ( type >= 0 )
	  iparam_check_type(*ploc, type);
	return 0;
}

/* ---------------- Implementations ---------------- */

/* Initialize for reading parameters. */
private int
ref_param_read_init(iparam_list *plist, uint count, const ref *ppolicies,
  bool require_all)
{	if ( ppolicies == 0 )
	  make_null(&plist->u.r.policies);
	else
	  plist->u.r.policies = *ppolicies;
	plist->u.r.require_all = require_all;
	plist->count = count;
	plist->results =
	  (int *)ialloc_byte_array(count, sizeof(int), "ref_param_read_init");
	if ( plist->results == 0 )
	  return_error(e_VMerror);
	memset(plist->results, 0, count * sizeof(int));
	plist->int_keys = false;
	return 0;
}

/* Implementation for putting parameters from an array. */
private int
array_param_read(iparam_list *plist, const ref *pkey, iparam_loc *ploc)
{	ref *bot = ((array_param_list *)plist)->bot;
	ref *ptr = bot;
	ref *top = ((array_param_list *)plist)->top;
	for ( ; ptr < top; ptr += 2 )
	{	if ( r_has_type(ptr, t_name) && name_eq(ptr, pkey) )
		{	ploc->pvalue = ptr + 1;
			ploc->presult = &plist->results[ptr - bot];
			*ploc->presult = 1;
			return 0;
		}
	}
	return 1;
}
int
array_param_list_read(array_param_list *plist, ref *bot, uint count,
  const ref *ppolicies, bool require_all)
{	if ( count & 1 )
	  return_error(e_rangecheck);
	plist->procs = &ref_read_procs;
	plist->u.r.read = array_param_read;
	plist->bot = bot;
	plist->top = bot + count;
	return ref_param_read_init(iplist, count, ppolicies, require_all);
}

/* Implementation for putting parameters from a stack. */
private int
stack_param_read(iparam_list *plist, const ref *pkey, iparam_loc *ploc)
{
#define splist ((stack_param_list *)plist)
	ref_stack *pstack = splist->pstack;
	/* This implementation is slow, but it probably doesn't matter. */
	uint index = splist->skip + 1;
	uint count = splist->count;
	for ( ; count; count--, index += 2 )
	  {	const ref *p = ref_stack_index(pstack, index);
		if ( r_has_type(p, t_name) && name_eq(p, pkey) )
		  {	ploc->pvalue = ref_stack_index(pstack, index - 1);
			ploc->presult = &plist->results[count - 1];
			*ploc->presult = 1;
			return 0;
		  }
	  }
#undef splist
	return 1;
}
int
stack_param_list_read(stack_param_list *plist, ref_stack *pstack, uint skip,
  const ref *ppolicies, bool require_all)
{	uint count = ref_stack_counttomark(pstack);
	if ( count == 0 )
	  return_error(e_unmatchedmark);
	count -= skip + 1;
	if ( count & 1 )
	  return_error(e_rangecheck);
	plist->procs = &ref_read_procs;
	plist->u.r.read = stack_param_read;
	plist->pstack = pstack;
	plist->skip = skip;
	return ref_param_read_init(iplist, count >> 1, ppolicies, require_all);
}	

/* Implementation for putting parameters from a dictionary. */
private int
dict_param_read(iparam_list *plist, const ref *pkey, iparam_loc *ploc)
{
#define spdict (&((dict_param_list *)plist)->dict)
	int code = dict_find(spdict, pkey, &ploc->pvalue);
	if ( code != 1 )
	  return 1;
	ploc->presult = &plist->results[dict_value_index(spdict, ploc->pvalue)];
#undef spdict
	*ploc->presult = 1;
	return 0;
}
int
dict_param_list_read(dict_param_list *plist, const ref *pdict,
  const ref *ppolicies, bool require_all)
{	check_dict_read(*pdict);
	plist->procs = &ref_read_procs;
	plist->u.r.read = dict_param_read;
	plist->dict = *pdict;
	return ref_param_read_init(iplist, dict_maxlength(pdict), ppolicies,
				   require_all);
}	
