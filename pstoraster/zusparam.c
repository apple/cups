/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zusparam.c */
/* User and system parameter operators */
#include "memory_.h"
#include "string_.h"
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "gscdefs.h"
#include "gsstruct.h"		/* for gxht.h */
#include "gsfont.h"		/* for user params */
#include "gxht.h"		/* for user params */
#include "gsutil.h"
#include "estack.h"
#include "ialloc.h"		/* for imemory for status */
#include "idict.h"
#include "idparam.h"
#include "iparam.h"
#include "dstack.h"
#include "iname.h"
#include "iutil2.h"
#include "store.h"

/* The (global) font directory */
extern gs_font_dir *ifont_dir;		/* in zfont.c */

/* Import the GC parameters from zvmem2.c. */
extern int set_vm_reclaim(P1(long));
extern int set_vm_threshold(P1(long));

/* The system passwords. */
private password StartJobPassword = NULL_PASSWORD;
password SystemParamsPassword = NULL_PASSWORD;	/* exported for ziodev2.c. */

/* Define an individual user or system parameter. */
/* Eventually this will be made public. */
#define param_def_common\
	const char _ds *pname
typedef struct param_def_s {
	param_def_common;
} param_def;
typedef struct long_param_def_s {
	param_def_common;
	long min_value, max_value;
	long (*current)(P0());
	int (*set)(P1(long));
} long_param_def;
#if arch_sizeof_long > arch_sizeof_int
#  define max_uint_param max_uint
#else
#  define max_uint_param max_long
#endif
typedef struct bool_param_def_s {
	param_def_common;
	bool (*current)(P0());
	int (*set)(P1(bool));
} bool_param_def;
typedef struct string_param_def_s {
	param_def_common;
	void (*current)(P1(gs_param_string *));
	int (*set)(P1(gs_param_string *));
} string_param_def;
/* Define a parameter set (user or system). */
typedef struct param_set_s {
	const long_param_def *long_defs;
	  uint long_count;
	const bool_param_def *bool_defs;
	  uint bool_count;
	const string_param_def *string_defs;
	  uint string_count;
} param_set;

/* Forward references */
private int setparams(P2(gs_param_list *, const param_set _ds *));
private int currentparams(P2(os_ptr, const param_set _ds *));

/* ------ Passwords ------ */

#define plist ((gs_param_list *)&list)

/* <string|int> .checkpassword <0|1|2> */
private int
zcheckpassword(register os_ptr op)
{	ref params[2];
	array_param_list list;
	int result = 0;
	int code = name_ref((const byte *)"Password", 8, &params[0], 0);
	if ( code < 0 )
	  return code;
	params[1] = *op;
	array_param_list_read(&list, params, 2, NULL, false);
	if ( param_check_password(plist, &StartJobPassword) == 0 )
		result = 1;
	if ( param_check_password(plist, &SystemParamsPassword) == 0 )
		result = 2;
	make_int(op, result);
	return 0;
}

#undef plist

/* ------ System parameters ------ */

/* Integer values */
private long
current_BuildTime(void)
{	return gs_buildtime;
}
private long
current_MaxFontCache(void)
{	return gs_currentcachesize(ifont_dir);
}
private int
set_MaxFontCache(long val)
{	return gs_setcachesize(ifont_dir,
			       (uint)(val < 0 ? 0 : val > max_uint ? max_uint :
				      val));
}
private long
current_CurFontCache(void)
{	uint cstat[7];
	gs_cachestatus(ifont_dir, cstat);
	return cstat[0];
}
private long
current_MaxGlobalVM(void)
{	gs_memory_gc_status_t stat;
	gs_memory_gc_status(iimemory_global, &stat);
	return stat.max_vm;
}
private int
set_MaxGlobalVM(long val)
{	gs_memory_gc_status_t stat;
	gs_memory_gc_status(iimemory_global, &stat);
	stat.max_vm = max(val, 0);
	gs_memory_set_gc_status(iimemory_global, &stat);
	return 0;
}
private long
current_Revision(void)
{	return gs_revision;
}
private const long_param_def system_long_params[] = {
	{"BuildTime", 0, max_uint_param, current_BuildTime, NULL},
	{"MaxFontCache", 0, max_uint_param, current_MaxFontCache, set_MaxFontCache},
	{"CurFontCache", 0, max_uint_param, current_CurFontCache, NULL},
	{"Revision", 0, max_uint_param, current_Revision, NULL},
		/* Extensions */
	{"MaxGlobalVM", 0, max_uint_param, current_MaxGlobalVM, set_MaxGlobalVM}
};
/* Boolean values */
private bool
current_ByteOrder(void)
{	return !arch_is_big_endian;
}
private const bool_param_def system_bool_params[] = {
	{"ByteOrder", current_ByteOrder, NULL}
};
/* String values */
private void
current_RealFormat(gs_param_string *pval)
{
#if arch_floats_are_IEEE
	static const char *rfs = "IEEE";
#else
	static const char *rfs = "not IEEE";
#endif
	pval->data = (const byte *)rfs;
	pval->size = strlen(rfs);
	pval->persistent = true;
}
private const string_param_def system_string_params[] = {
	{"RealFormat", current_RealFormat, NULL}
};

/* The system parameter set */
private const param_set system_param_set = {
	system_long_params, countof(system_long_params),
	system_bool_params, countof(system_bool_params),
	system_string_params, countof(system_string_params)
};


/* <dict> setsystemparams - */
private int
zsetsystemparams(register os_ptr op)
{	int code;
	dict_param_list list;
	password pass;

	check_type(*op, t_dictionary);
	code = dict_param_list_read(&list, op, NULL, false);
	if ( code < 0 )
	  return code;
#define plist ((gs_param_list *)&list)
	code = param_check_password(plist, &SystemParamsPassword);
	if ( code != 0 )
	  return_error(code < 0 ? code : e_invalidaccess);
	code = param_read_password(plist, "StartJobPassword", &pass);
	switch ( code )
	{
	default:			/* invalid */
		return code;
	case 1:				/* missing */
		break;
	case 0:
		StartJobPassword = pass;
	}
	code = param_read_password(plist, "SystemParamsPassword", &pass);
	switch ( code )
	{
	default:			/* invalid */
		return code;
	case 1:				/* missing */
		break;
	case 0:
		SystemParamsPassword = pass;
	}
	code = setparams(plist, &system_param_set);
	if ( code < 0 )
	  return code;
#undef plist
	pop(1);
	return 0;
}

/* - .currentsystemparams <name1> <value1> ... */
private int
zcurrentsystemparams(os_ptr op)
{	return currentparams(op, &system_param_set);
}

/* ------ User parameters ------ */

/* Integer values */
private long
current_JobTimeout(void)
{	return 0;
}
private int
set_JobTimeout(long val)
{	return 0;
}
private long
current_MaxFontItem(void)
{	return gs_currentcacheupper(ifont_dir);
}
private int
set_MaxFontItem(long val)
{	return gs_setcacheupper(ifont_dir, val);
}
private long
current_MinFontCompress(void)
{	return gs_currentcachelower(ifont_dir);
}
private int
set_MinFontCompress(long val)
{	return gs_setcachelower(ifont_dir, val);
}
private long
current_MaxOpStack(void)
{	return ref_stack_max_count(&o_stack);
}
private int
set_MaxOpStack(long val)
{	return ref_stack_set_max_count(&o_stack, val);
}
private long
current_MaxDictStack(void)
{	return ref_stack_max_count(&d_stack);
}
private int
set_MaxDictStack(long val)
{	return ref_stack_set_max_count(&d_stack, val);
}
private long
current_MaxExecStack(void)
{	return ref_stack_max_count(&e_stack);
}
private int
set_MaxExecStack(long val)
{	return ref_stack_set_max_count(&e_stack, val);
}
private long
current_MaxLocalVM(void)
{	gs_memory_gc_status_t stat;
	gs_memory_gc_status(iimemory_local, &stat);
	return stat.max_vm;
}
private int
set_MaxLocalVM(long val)
{	gs_memory_gc_status_t stat;
	gs_memory_gc_status(iimemory_local, &stat);
	stat.max_vm = max(val, 0);
	gs_memory_set_gc_status(iimemory_local, &stat);
	return 0;
}
private long
current_VMReclaim(void)
{	gs_memory_gc_status_t gstat, lstat;
	gs_memory_gc_status(iimemory_global, &gstat);
	gs_memory_gc_status(iimemory_local, &lstat);
	return (!gstat.enabled ? -2 : !lstat.enabled ? -1 : 0);
}
private long
current_VMThreshold(void)
{	gs_memory_gc_status_t stat;
	gs_memory_gc_status(iimemory_local, &stat);
	return stat.vm_threshold;
}
#define current_WaitTimeout current_JobTimeout
#define set_WaitTimeout set_JobTimeout
private const long_param_def user_long_params[] = {
	{"JobTimeout", 0, max_uint_param,
	   current_JobTimeout, set_JobTimeout},
	{"MaxFontItem", 0, max_uint_param,
	   current_MaxFontItem, set_MaxFontItem},
	{"MinFontCompress", 0, max_uint_param,
	   current_MinFontCompress, set_MinFontCompress},
	{"MaxOpStack", 0, max_uint_param,
	   current_MaxOpStack, set_MaxOpStack},
	{"MaxDictStack", 0, max_uint_param,
	   current_MaxDictStack, set_MaxDictStack},
	{"MaxExecStack", 0, max_uint_param,
	   current_MaxExecStack, set_MaxExecStack},
	{"MaxLocalVM", 0, max_uint_param,
	   current_MaxLocalVM, set_MaxLocalVM},
	{"VMReclaim", -2, 0,
	   current_VMReclaim, set_vm_reclaim},
	{"VMThreshold", 0, max_uint_param,
	   current_VMThreshold, set_vm_threshold},
	{"WaitTimeout", 0, max_uint_param,
	   current_WaitTimeout, set_WaitTimeout}
};
/* Boolean values */
private bool
current_AccurateScreens(void)
{	return gs_currentaccuratescreens();
}
private int
set_AccurateScreens(bool val)
{	gs_setaccuratescreens(val);
	return 0;
}
private const bool_param_def user_bool_params[] = {
	{"AccurateScreens", current_AccurateScreens, set_AccurateScreens}
};
/* String values */
private void
current_JobName(gs_param_string *pval)
{	pval->data = 0;
	pval->size = 0;
	pval->persistent = true;
}
private int
set_JobName(gs_param_string *val)
{	return 0;
}
private const string_param_def user_string_params[] = {
	{"JobName", current_JobName, set_JobName}
};

/* The user parameter set */
private const param_set user_param_set = {
	user_long_params, countof(user_long_params),
	user_bool_params, countof(user_bool_params),
	user_string_params, countof(user_string_params)
};

/* <dict> setuserparams - */
private int
zsetuserparams(register os_ptr op)
{	dict_param_list list;
	int code;

	check_type(*op, t_dictionary);
	code = dict_param_list_read(&list, op, NULL, false);
	if ( code < 0 )
	  return code;
	code = setparams((gs_param_list *)&list, &user_param_set);
	if ( code < 0 )
	  return code;
	pop(1);
	return 0;
}

/* - .currentuserparams <name1> <value1> ... */
private int
zcurrentuserparams(os_ptr op)
{	return currentparams(op, &user_param_set);
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zusparam_op_defs) {
		/* User and system parameters are readable even in Level 1. */
	{"0.currentsystemparams", zcurrentsystemparams},
	{"0.currentuserparams", zcurrentuserparams},
		/* The rest of the operators are defined only in Level 2. */
		op_def_begin_level2(),
	{"1.checkpassword", zcheckpassword},
	{"1setsystemparams", zsetsystemparams},
	{"1setuserparams", zsetuserparams},
END_OP_DEFS(0) }

/* ------ Internal procedures ------ */

/* Set the values of a parameter set from a parameter list. */
/* We don't attempt to back out if anything fails. */
private int
setparams(gs_param_list *plist, const param_set _ds *pset)
{	int i, code;
	for ( i = 0; i < pset->long_count; i++ )
	  {	const long_param_def *pdef = &pset->long_defs[i];
		long val;
		if ( pdef->set == NULL )
		  continue;
		code = param_read_long(plist, pdef->pname, &val);
		switch ( code )
		  {
		  default:			/* invalid */
			return code;
		  case 1:			/* missing */
			break;
		  case 0:
			if ( val < pdef->min_value || val > pdef->max_value )
			  return_error(e_rangecheck);
			code = (*pdef->set)(val);
			if ( code < 0 )
			  return code;
		  }
	  }
	for ( i = 0; i < pset->bool_count; i++ )
	  {	const bool_param_def *pdef = &pset->bool_defs[i];
		bool val;
		if ( pdef->set == NULL )
		  continue;
		code = param_read_bool(plist, pdef->pname, &val);
		if ( code == 0 )
		  code = (*pdef->set)(val);
		if ( code < 0 )
		  return code;
	  }
	/****** WE SHOULD DO STRINGS AND STRING ARRAYS, BUT WE DON'T YET ******/
	return 0;
}

/* Get the current values of a parameter set to the stack. */
private int
currentparams(os_ptr op, const param_set _ds *pset)
{	stack_param_list list;
	int i;
	stack_param_list_write(&list, &o_stack, NULL);
	for ( i = 0; i < pset->long_count; i++ )
	{	long val = (*pset->long_defs[i].current)();
		int code = param_write_long((gs_param_list *)&list,
					    pset->long_defs[i].pname, &val);

		if ( code < 0 )
		  return code;
	}
	for ( i = 0; i < pset->bool_count; i++ )
	{	bool val = (*pset->bool_defs[i].current)();
		int code = param_write_bool((gs_param_list *)&list,
					    pset->bool_defs[i].pname, &val);

		if ( code < 0 )
		  return code;
	}
	for ( i = 0; i < pset->string_count; i++ )
	{	gs_param_string val;
		int code;

		(*pset->string_defs[i].current)(&val);
		code = param_write_string((gs_param_list *)&list,
					  pset->string_defs[i].pname, &val);
		if ( code < 0 )
		  return code;
	}
	return 0;
}
