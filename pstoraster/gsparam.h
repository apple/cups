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

/* gsparam.h */
/* Client interface to parameter dictionaries */

#ifndef gsparam_INCLUDED
#  define gsparam_INCLUDED

/*
 * Several interfaces use parameter dictionaries to communicate sets of
 * (key, value) pairs from a client to an object with complex state.
 * (Several of these correspond directly to similar interfaces in the
 * PostScript language.) This file defines the API for parameter dictionaries.
 */

#ifndef gs_param_list_DEFINED
#  define gs_param_list_DEFINED
typedef struct gs_param_list_s gs_param_list;
#endif
typedef const char *gs_param_name;

/*
 * Define a structure for representing a variable-size value
 * (string/name, integer array, or floating point array).
 * The size is the number of elements, not the size in bytes.
 * A value is persistent if it is defined as static const,
 * or if it is allocated in garbage-collectable space and never freed.
 */

#define _param_array_struct(sname,etype)\
  struct sname { const etype *data; uint size; bool persistent; }
typedef _param_array_struct(gs_param_string_s, byte) gs_param_string;
typedef _param_array_struct(gs_param_int_array_s, int) gs_param_int_array;
typedef _param_array_struct(gs_param_float_array_s, float) gs_param_float_array;
typedef _param_array_struct(gs_param_string_array_s, gs_param_string) gs_param_string_array;

#define param_string_from_string(ps, str)\
  (ps).data = (const byte *)(str), (ps).size = strlen((const char *)(ps).data),\
  (ps).persistent = true

/* Define the type for dictionary and mixed-type-array values. */
typedef struct gs_param_collection_s {
	gs_param_list *list;
	uint size;
} gs_param_collection;
typedef gs_param_collection gs_param_dict;
typedef gs_param_collection gs_param_array;

/* Define the 'policies' for handling out-of-range parameter values. */
/* This is not an enum, because some parameters may recognize other values. */
#define gs_param_policy_signal_error 0
#define gs_param_policy_ignore 1
#define gs_param_policy_consult_user 2

/*
 * Define the object procedures.  Note that the same interface is used
 * both for getting and for setting parameter values.  (This is a bit
 * of a hack, and we might change it someday.)  The procedures return
 * as follows:
 *	- 'reading' procedures ('put' operations from the client's viewpoint)
 * return 1 for a missing parameter, 0 for a valid parameter, <0 on error.
 *	- 'writing' procedures ('get' operations from the client's viewpoint)
 * return 0 or 1 if successful, <0 on error.
 */

/*
 * Transmitting variable-size objects requires some extra care.
 *	- When writing an array, string, name, or dictionary, the
 * implementation (not the client) sets all the fields of the value.
 *	- When reading an array, string, or name, the client must set
 * all the fields of the value.
 *	- When reading a dictionary, the client must set the size field
 * before calling begin_write_dict; the implementation of begin_write_dict
 * allocates the list.
 */

/*
 * Setting parameters must use a "two-phase commit" policy.  Specifically,
 * any put_params procedure must observe the following discipline:

	1. For each parameter known to the device, ask the parameter list if
there is a new value, and if so, make all necessary validity checks.  If any
check fails, call param_signal_error for that parameter, but continue to
check further parameters.  Normally, this step should not alter the state of
the device; however, if the device allows changing any parameters that are
read-only by default (for example, BitsPerPixel or ProcessColorModel), or if
it replaces the default put_params behavior for any parameter (for example,
if it handles MediaSize or Resolution itself to forestall the normal closing
of the device when these are set), step 1 of put_params must change the
parameters in the device state, and step 2 must undo the changes if
returning an error.

	2. Call the "superclass" put_params routine.  For printer devices,
this is gdev_prn_put_params; for other devices, it is gx_default_put_params.
Note that this must be done even if errors were detected in step 1.  If this
routine returns an error code, or if step 1 detected an error, undo any
changes that step 1 made in the device state, and return the error code.

	3. Install the new parameter values in the device.  If necessary,
close the device first; a higher-level routine (gs_putdeviceparams) will
reopen the device if necessary.

 */

typedef struct gs_param_list_procs_s {

	/* Transmit a null value. */
	/* Note that this is the only 'transmit' operation */
	/* that does not actually pass any data. */

#define param_proc_xmit_null(proc)\
  int proc(P2(gs_param_list *, gs_param_name))
	param_proc_xmit_null((*xmit_null));
#define param_read_null(plist, pkey)\
  (*(plist)->procs->xmit_null)(plist, pkey)
#define param_write_null(plist, pkey)\
  (*(plist)->procs->xmit_null)(plist, pkey)

	/* Transmit a Boolean value. */

#define param_proc_xmit_bool(proc)\
  int proc(P3(gs_param_list *, gs_param_name, bool *))
	param_proc_xmit_bool((*xmit_bool));
#define param_read_bool(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_bool)(plist, pkey, pvalue)
#define param_write_bool(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_bool)(plist, pkey, pvalue)

	/* Transmit an integer value. */

#define param_proc_xmit_int(proc)\
  int proc(P3(gs_param_list *, gs_param_name, int *))
	param_proc_xmit_int((*xmit_int));
#define param_read_int(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_int)(plist, pkey, pvalue)
#define param_write_int(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_int)(plist, pkey, pvalue)

	/* Transmit a long value. */

#define param_proc_xmit_long(proc)\
  int proc(P3(gs_param_list *, gs_param_name, long *))
	param_proc_xmit_long((*xmit_long));
#define param_read_long(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_long)(plist, pkey, pvalue)
#define param_write_long(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_long)(plist, pkey, pvalue)

	/* Transmit a float value. */

#define param_proc_xmit_float(proc)\
  int proc(P3(gs_param_list *, gs_param_name, float *))
	param_proc_xmit_float((*xmit_float));
#define param_read_float(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_float)(plist, pkey, pvalue)
#define param_write_float(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_float)(plist, pkey, pvalue)

	/* Transmit a string value. */

#define param_proc_xmit_string(proc)\
  int proc(P3(gs_param_list *, gs_param_name, gs_param_string *))
	param_proc_xmit_string((*xmit_string));
#define param_read_string(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_string)(plist, pkey, pvalue)
#define param_write_string(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_string)(plist, pkey, pvalue)

	/* Transmit a name value. */

#define param_proc_xmit_name(proc)\
  int proc(P3(gs_param_list *, gs_param_name, gs_param_string *))
	param_proc_xmit_name((*xmit_name));
#define param_read_name(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_name)(plist, pkey, pvalue)
#define param_write_name(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_name)(plist, pkey, pvalue)

	/* Transmit an integer array value. */

#define param_proc_xmit_int_array(proc)\
  int proc(P3(gs_param_list *, gs_param_name, gs_param_int_array *))
	param_proc_xmit_int_array((*xmit_int_array));
#define param_read_int_array(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_int_array)(plist, pkey, pvalue)
#define param_write_int_array(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_int_array)(plist, pkey, pvalue)

	/* Transmit a float array value. */

#define param_proc_xmit_float_array(proc)\
  int proc(P3(gs_param_list *, gs_param_name, gs_param_float_array *))
	param_proc_xmit_float_array((*xmit_float_array));
#define param_read_float_array(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_float_array)(plist, pkey, pvalue)
#define param_write_float_array(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_float_array)(plist, pkey, pvalue)

	/* Transmit a string array value. */

#define param_proc_xmit_string_array(proc)\
  int proc(P3(gs_param_list *, gs_param_name, gs_param_string_array *))
	param_proc_xmit_string_array((*xmit_string_array));
#define param_read_string_array(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_string_array)(plist, pkey, pvalue)
#define param_write_string_array(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_string_array)(plist, pkey, pvalue)

	/* Transmit a name array value. */

#define param_proc_xmit_name_array(proc)\
  int proc(P3(gs_param_list *, gs_param_name, gs_param_string_array *))
	param_proc_xmit_name_array((*xmit_name_array));
#define param_read_name_array(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_name_array)(plist, pkey, pvalue)
#define param_write_name_array(plist, pkey, pvalue)\
  (*(plist)->procs->xmit_name_array)(plist, pkey, pvalue)

	/* Start transmitting a dictionary value. */
	/* If int_keys is true, the keys are actually integers */
	/* (although still presented as strings). */

#define param_proc_begin_xmit_dict(proc)\
  int proc(P4(gs_param_list *, gs_param_name, gs_param_dict *, bool))
	param_proc_begin_xmit_dict((*begin_xmit_dict));
#define param_begin_read_dict(plist, pkey, pvalue, int_keys)\
  (*(plist)->procs->begin_xmit_dict)(plist, pkey, pvalue, int_keys)
#define param_begin_write_dict(plist, pkey, pvalue, int_keys)\
  (*(plist)->procs->begin_xmit_dict)(plist, pkey, pvalue, int_keys)

	/* Finish transmitting a dictionary value. */

#define param_proc_end_xmit_dict(proc)\
  int proc(P3(gs_param_list *, gs_param_name, gs_param_dict *))
	param_proc_end_xmit_dict((*end_xmit_dict));
#define param_end_read_dict(plist, pkey, pvalue)\
  (*(plist)->procs->end_xmit_dict)(plist, pkey, pvalue)
#define param_end_write_dict(plist, pkey, pvalue)\
  (*(plist)->procs->end_xmit_dict)(plist, pkey, pvalue)

	/* Determine whether a given key has been requested. */
	/* (Only used when writing.) */

#define param_proc_requested(proc)\
  bool proc(P2(const gs_param_list *, gs_param_name))
	param_proc_requested((*requested));
#define param_requested(plist, pkey)\
  (*(plist)->procs->requested)(plist, pkey)

	/* Get the 'policy' associated with an out-of-range parameter value. */
	/* (Only used when reading.) */

#define param_proc_get_policy(proc)\
  int proc(P2(gs_param_list *, gs_param_name))
	param_proc_get_policy((*get_policy));
#define param_get_policy(plist, pkey)\
  (*(plist)->procs->get_policy)(plist, pkey)

	/*
	 * Signal an error.  (Only used when reading.)
	 * The procedure may return a different error code,
	 * or may return 0 indicating that the error is to be ignored.
	 */

#define param_proc_signal_error(proc)\
  int proc(P3(gs_param_list *, gs_param_name, int))
	param_proc_signal_error((*signal_error));
#define param_signal_error(plist, pkey, code)\
  (*(plist)->procs->signal_error)(plist, pkey, code)
#define param_return_error(plist, pkey, code)\
  return_error(param_signal_error(plist, pkey, code))

	/*
	 * "Commit" a set of changes.  (Only used when reading.)
	 * This is called at the end of the first phase.
	 */

#define param_proc_commit(proc)\
  int proc(P1(gs_param_list *))
	param_proc_commit((*commit));
#define param_commit(plist)\
  (*(plist)->procs->commit)(plist)

} gs_param_list_procs;

/* Define an abstract parameter dictionary.  Implementations are */
/* concrete subclasses. */
#define gs_param_list_common\
	const gs_param_list_procs _ds *procs
struct gs_param_list_s {
	gs_param_list_common;
};

/*
 * Define a default implementation, intended to be usable easily
 * from C code.  The intended usage pattern is:
	gs_c_param_list list;
	[... other code here ...]
	gs_c_param_list_write(&list, mem);
	[As many as needed:]
		code = param_write_XXX(&list, "ParamName", &param_value);
		[Check code for <0]
	gs_c_param_list_read(&list);
	code = gs_putdeviceparams(dev, &list);
	gs_c_param_list_release(&list);
	[Check code for <0]
	if ( code == 1 )
	{ code = (*dev_proc(dev, open_device))(dev);
	  [Check code for <0]
	}
 */

typedef struct gs_c_param_s gs_c_param;		/* opaque here */
typedef struct gs_c_param_list_s {
	gs_param_list_common;
	gs_memory_t *memory;
	gs_c_param *head;
	uint count;
} gs_c_param_list;
#define private_st_c_param_list()	/* in gsparam.c */\
  gs_private_st_ptrs1(st_c_param_list, gs_c_param_list, "c_param_list",\
    c_param_list_enum_ptrs, c_param_list_reloc_ptrs, head)
/* Clients normally allocate the gs_c_param_list on the stack. */
void gs_c_param_list_write(P2(gs_c_param_list *, gs_memory_t *));
void gs_c_param_list_read(P1(gs_c_param_list *));	/* switch to reading */
void gs_c_param_list_release(P1(gs_c_param_list *));

#endif					/* gsparam_INCLUDED */
