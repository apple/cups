/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gsparam.c */
/* Default implementation of parameter lists */
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsparam.h"
#include "gsstruct.h"

/* Forward references */
typedef union c_param_value_s c_param_value;
/*typedef struct gs_c_param_s gs_c_param;*/	/* in gsparam.h */

/* Define the GC type for a parameter list. */
private_st_c_param_list();

/* Define a union type for parameter values. */
union c_param_value_s {
	bool b;
	long l;
	float f;
	gs_param_string s;
	gs_param_int_array ia;
	gs_param_float_array fa;
	gs_param_string_array sa;
	gs_c_param_list d;
};

/*
 * Define the parameter list element types.
 * We would like to define:
 *	#define cpt(n, t) ((sizeof(t) << 4) + n)
 *	#define cpt_size(cpt) ((cpt) >> 4)
 * but a bug in the Borland C++ 3.1 compiler causes the enumerated type
 * never to get defined if we do this.  Instead, we use an ordinary
 * enumeration, and get the sizes from a table.
 */
typedef enum {
	cpt_null, cpt_bool, cpt_long, cpt_float,
	cpt_string, cpt_int_array,
	cpt_float_array, cpt_string_array,
	cpt_dict
} c_param_type;
private const byte c_param_type_sizes[] = {
	0, sizeof(bool), sizeof(long), sizeof(float),
	sizeof(gs_param_string), sizeof(gs_param_int_array),
	sizeof(gs_param_float_array), sizeof(gs_param_string_array),
	sizeof(gs_c_param_list)
};
#define cpt_size(cpt) (c_param_type_sizes[(int)cpt])

/* Define a parameter list element. */
struct gs_c_param_s {
	gs_c_param *next;
	gs_param_name key;
	c_param_value value;
	c_param_type type;
};
/* Parameter values aren't really simple, */
/* but since parameter lists are transient, it doesn't matter. */
gs_private_st_ptrs1(st_c_param, gs_c_param, "gs_c_param",
  c_param_enum_ptrs, c_param_reloc_ptrs, next);

/* ================ Utilities ================ */

#define cplist ((gs_c_param_list *)plist)

/* ================ Writing parameters to a list ================ */

private param_proc_xmit_null(c_param_write_null);
private param_proc_xmit_bool(c_param_write_bool);
private param_proc_xmit_int(c_param_write_int);
private param_proc_xmit_long(c_param_write_long);
private param_proc_xmit_float(c_param_write_float);
private param_proc_xmit_string(c_param_write_string);
private param_proc_xmit_int_array(c_param_write_int_array);
private param_proc_xmit_float_array(c_param_write_float_array);
private param_proc_xmit_string_array(c_param_write_string_array);
private param_proc_begin_xmit_dict(c_param_begin_write_dict);
private param_proc_end_xmit_dict(c_param_end_write_dict);
private param_proc_requested(c_param_requested);
private const gs_param_list_procs c_write_procs = {
	c_param_write_null,
	c_param_write_bool,
	c_param_write_int,
	c_param_write_long,
	c_param_write_float,
	c_param_write_string,
	c_param_write_string,		/* name = string */
	c_param_write_int_array,
	c_param_write_float_array,
	c_param_write_string_array,
	c_param_write_string_array,	/* name = string */
	c_param_begin_write_dict,
	c_param_end_write_dict,
	c_param_requested
};

/* Initialize a list for writing. */
void
gs_c_param_list_write(gs_c_param_list *plist, gs_memory_t *mem)
{	plist->procs = &c_write_procs;
	plist->memory = mem;
	plist->head = 0;
	plist->count = 0;
}

/* Release a list. */
void
gs_c_param_list_release(gs_c_param_list *plist)
{	gs_c_param *pparam;
	while ( (pparam = plist->head) != 0 )
	  { gs_c_param *next = pparam->next;
	    if ( pparam->type == cpt_dict )
	      gs_c_param_list_release(&pparam->value.d);
	    gs_free_object(plist->memory, pparam, "gs_c_param_list_release");
	    plist->head = next;
	    plist->count--;
	  }
}

/* Generic routine for writing a parameter to a list. */
private int
c_param_write(gs_c_param_list *plist, gs_param_name pkey, void *pvalue,
  c_param_type type)
{	gs_c_param *pparam =
	  gs_alloc_struct(plist->memory, gs_c_param, &st_c_param,
			  "c_param_write");
	if ( pparam == 0 )
	  return_error(gs_error_VMerror);
	pparam->next = plist->head;
	pparam->key = pkey;
	memcpy(&pparam->value, pvalue, cpt_size(type));
	pparam->type = type;
	plist->head = pparam;
	plist->count++;
	return 0;
}

/* Individual writing routines. */
#define cpw(pvalue, type)\
  c_param_write(cplist, pkey, pvalue, type)
private int
c_param_write_null(gs_param_list *plist, gs_param_name pkey)
{	return cpw(NULL, cpt_null);
}
private int
c_param_write_bool(gs_param_list *plist, gs_param_name pkey, bool *pvalue)
{	return cpw(pvalue, cpt_bool);
}
private int
c_param_write_int(gs_param_list *plist, gs_param_name pkey, int *pvalue)
{	long l = *pvalue;
	return cpw(&l, cpt_long);
}
private int
c_param_write_long(gs_param_list *plist, gs_param_name pkey, long *pvalue)
{	return cpw(pvalue, cpt_long);
}
private int
c_param_write_float(gs_param_list *plist, gs_param_name pkey, float *pvalue)
{	return cpw(pvalue, cpt_float);
}
private int
c_param_write_string(gs_param_list *plist, gs_param_name pkey,
  gs_param_string *pvalue)
{	return cpw(pvalue, cpt_string);
}
private int
c_param_write_int_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_int_array *pvalue)
{	return cpw(pvalue, cpt_int_array);
}
private int
c_param_write_float_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_float_array *pvalue)
{	return cpw(pvalue, cpt_float_array);
}
private int
c_param_write_string_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_string_array *pvalue)
{	return cpw(pvalue, cpt_string_array);
}
private int
c_param_begin_write_dict(gs_param_list *plist, gs_param_name pkey,
  gs_param_dict *pvalue, bool int_keys)
{	gs_c_param_list *dlist =
	  gs_alloc_struct(cplist->memory, gs_c_param_list, &st_c_param_list,
			  "c_param_begin_write_dict");
	if ( dlist == 0 )
	  return_error(gs_error_VMerror);
	gs_c_param_list_write(dlist, cplist->memory);
	pvalue->list = (gs_param_list *)dlist;
	return 0;
}
private int
c_param_end_write_dict(gs_param_list *plist, gs_param_name pkey,
  gs_param_dict *pvalue)
{	return cpw(pvalue->list, cpt_dict);
}

/* Other procedures */
private bool
c_param_requested(const gs_param_list *plist, gs_param_name pkey)
{	return true;
}

/* ================ Reading from a list to parameters ================ */

private param_proc_xmit_null(c_param_read_null);
private param_proc_xmit_bool(c_param_read_bool);
private param_proc_xmit_int(c_param_read_int);
private param_proc_xmit_long(c_param_read_long);
private param_proc_xmit_float(c_param_read_float);
private param_proc_xmit_string(c_param_read_string);
private param_proc_xmit_int_array(c_param_read_int_array);
private param_proc_xmit_float_array(c_param_read_float_array);
private param_proc_xmit_string_array(c_param_read_string_array);
private param_proc_begin_xmit_dict(c_param_begin_read_dict);
private param_proc_end_xmit_dict(c_param_end_read_dict);
private param_proc_get_policy(c_param_read_get_policy);
private param_proc_signal_error(c_param_read_signal_error);
private param_proc_commit(c_param_read_commit);
private const gs_param_list_procs c_read_procs = {
	c_param_read_null,
	c_param_read_bool,
	c_param_read_int,
	c_param_read_long,
	c_param_read_float,
	c_param_read_string,
	c_param_read_string,		/* name = string */
	c_param_read_int_array,
	c_param_read_float_array,
	c_param_read_string_array,
	c_param_read_string_array,	/* name = string */
	c_param_begin_read_dict,
	c_param_end_read_dict,
	NULL,				/* requested */
	c_param_read_get_policy,
	c_param_read_signal_error,
	c_param_read_commit
};

/* Switch a list from writing to reading. */
void
gs_c_param_list_read(gs_c_param_list *plist)
{	plist->procs = &c_read_procs;
}

/* Generic routine for reading a parameter from a list. */
private gs_c_param *
c_param_find(gs_c_param_list *plist, gs_param_name pkey)
{	gs_c_param *pparam = plist->head;
	for ( ; pparam != 0; pparam = pparam->next )
	  if ( !strcmp(pparam->key, pkey) )
	    return pparam;
	return 0;
}
private int
c_param_read(gs_c_param_list *plist, gs_param_name pkey, void *pvalue,
  c_param_type type)
{	gs_c_param *pparam = c_param_find(plist, pkey);
	if ( pparam == 0 )
	  return 1;
	if ( pparam->type != type )
	  return_error(gs_error_typecheck);
	memcpy(pvalue, &pparam->value, cpt_size(type));
	return 0;
}

/* Individual reading routines. */
#define cpr(pvalue, type)\
  c_param_read(cplist, pkey, pvalue, type)
private int
c_param_read_null(gs_param_list *plist, gs_param_name pkey)
{	return cpr(NULL, cpt_null);
}
private int
c_param_read_bool(gs_param_list *plist, gs_param_name pkey, bool *pvalue)
{	return cpr(pvalue, cpt_bool);
}
private int
c_param_read_int(gs_param_list *plist, gs_param_name pkey, int *pvalue)
{	long l;
	int code = cpr(&l, cpt_long);
	if ( code == 0 )
	  { if ( l < min_int || l > max_int )
	      return_error(gs_error_rangecheck);
	  }
	return code;
}
private int
c_param_read_long(gs_param_list *plist, gs_param_name pkey, long *pvalue)
{	return cpr(pvalue, cpt_long);
}
private int
c_param_read_float(gs_param_list *plist, gs_param_name pkey, float *pvalue)
{	return cpr(pvalue, cpt_float);
}
private int
c_param_read_string(gs_param_list *plist, gs_param_name pkey,
  gs_param_string *pvalue)
{	return cpr(pvalue, cpt_string);
}
private int
c_param_read_int_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_int_array *pvalue)
{	return cpr(pvalue, cpt_int_array);
}
private int
c_param_read_float_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_float_array *pvalue)
{	return cpr(pvalue, cpt_float_array);
}
private int
c_param_read_string_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_string_array *pvalue)
{	return cpr(pvalue, cpt_string_array);
}
private int
c_param_begin_read_dict(gs_param_list *plist, gs_param_name pkey,
  gs_param_dict *pvalue, bool int_keys)
{	gs_c_param *pparam = c_param_find(cplist, pkey);
	if ( pparam == 0 )
	  return 1;
	if ( pparam->type != cpt_dict )
	  return_error(gs_error_typecheck);
	gs_c_param_list_read(&pparam->value.d);
	pvalue->list = (gs_param_list *)&pparam->value.d;
	pvalue->size = pparam->value.d.count;
	return 0;
}
private int
c_param_end_read_dict(gs_param_list *plist, gs_param_name pkey,
  gs_param_dict *pvalue)
{	return 0;
}

/* Other procedures */
private int
c_param_read_get_policy(gs_param_list *plist, gs_param_name pkey)
{	return gs_param_policy_ignore;
}
private int
c_param_read_signal_error(gs_param_list *plist, gs_param_name pkey, int code)
{	return code;
}
private int
c_param_read_commit(gs_param_list *plist)
{	return 0;
}
