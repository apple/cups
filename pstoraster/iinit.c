/* Copyright (C) 1989, 1995, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
  to anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer
  to the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given
  to you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises supports the work of the GNU Project, but is not
  affiliated with the Free Software Foundation or the GNU Project.  GNU
  Ghostscript, as distributed by Aladdin Enterprises, does not require any
  GNU software to build or run it.
*/

/*$Id: iinit.c,v 1.3 2000/03/08 23:15:12 mike Exp $ */
/* Initialize internally known objects for Ghostscript interpreter */
#include "string_.h"
#include "ghost.h"
#include "gscdefs.h"
#include "gsexit.h"
#include "gsstruct.h"
#include "errors.h"
#include "ialloc.h"
#include "idict.h"
#include "dstack.h"
#include "ilevel.h"
#include "iname.h"
#include "interp.h"
#include "ipacked.h"
#include "iparray.h"
#include "iutil.h"
#include "ivmspace.h"
#include "opdef.h"
#include "store.h"

/* Implementation parameters. */
/*
 * Define the (initial) sizes of the various system dictionaries.  We want
 * the sizes to be prime numbers large enough to cover all the operators,
 * plus everything in the init files, even if all the optional features are
 * selected.  Note that these sizes must be large enough to get us through
 * initialization, since we start up in Level 1 mode where dictionaries
 * don't expand automatically.
 */
/* The size of systemdict can be set in the makefile. */
#ifndef SYSTEMDICT_SIZE
#  define SYSTEMDICT_SIZE 601
#endif
#ifndef SYSTEMDICT_LEVEL2_SIZE
#  define SYSTEMDICT_LEVEL2_SIZE 941
#endif
/* The size of level2dict, if applicable, can be set in the makefile. */
#ifndef LEVEL2DICT_SIZE
#  define LEVEL2DICT_SIZE 233
#endif
/* Ditto the size of ll3dict. */
#ifndef LL3DICT_SIZE
#  define LL3DICT_SIZE 43
#endif
/* Ditto the size of filterdict. */
#ifndef FILTERDICT_SIZE
#  define FILTERDICT_SIZE 43
#endif
/* Define an arbitrary size for the operator procedure tables. */
#ifndef OP_ARRAY_TABLE_SIZE
#  define OP_ARRAY_TABLE_SIZE 180
#endif
#ifndef OP_ARRAY_TABLE_GLOBAL_SIZE
#  define OP_ARRAY_TABLE_GLOBAL_SIZE OP_ARRAY_TABLE_SIZE
#endif
#ifndef OP_ARRAY_TABLE_LOCAL_SIZE
#  define OP_ARRAY_TABLE_LOCAL_SIZE (OP_ARRAY_TABLE_SIZE / 2)
#endif
#define OP_ARRAY_TABLE_TOTAL_SIZE\
  (OP_ARRAY_TABLE_GLOBAL_SIZE + OP_ARRAY_TABLE_LOCAL_SIZE)

/* Define the list of error names. */
const char *const gs_error_names[] =
{
    ERROR_NAMES
};

/* The operator tables */
const op_def **op_def_table;
uint op_def_count;
op_array_table op_array_table_global, op_array_table_local;	/* definitions of `operator' procedures */

/* Enter a name and value into a dictionary. */
void
initial_enter_name_in(const char *nstr, const ref * pref, ref * pdict)
{
    int code = dict_put_string(pdict, nstr, pref);

    if (code < 0) {
	lprintf4("initial_enter failed (%d), entering /%s in -dict:%u/%u-\n",
		 code, nstr, dict_length(pdict), dict_maxlength(pdict));
	gs_exit(1);
    }
}
void
initial_enter_name(const char *nstr, const ref * pref)
{
    initial_enter_name_in(nstr, pref, systemdict);
}

/* Remove a name from systemdict. */
void
initial_remove_name(const char *nstr)
{
    ref nref;

    if (name_ref((const byte *)nstr, strlen(nstr), &nref, -1) >= 0)
	dict_undef(systemdict, &nref);
}

/* Create a name.  Fatal error if it fails. */
private void
name_enter(const char *str, ref * pref)
{
    if (name_enter_string(str, pref) != 0) {
	lprintf1("name_enter failed - %s\n", str);
	gs_exit(1);
    }
}

/* Define the names and sizes of the initial dictionaries. */
/* The names are used to create references in systemdict. */
const struct {
    const char *name;
    uint size;
    bool local;
} initial_dictionaries[] = {
#ifdef INITIAL_DICTIONARIES
    INITIAL_DICTIONARIES
#else
    /* systemdict is created and named automagically */
    {
	"level2dict", LEVEL2DICT_SIZE, false
    },
    {
	"ll3dict", LL3DICT_SIZE, false
    },
    {
	"globaldict", 0, false
    },
    {
	"userdict", 0, true
    },
    {
	"filterdict", FILTERDICT_SIZE, false
    },
#endif
};
/* systemdict and globaldict are magically inserted at the bottom */
const char *const initial_dstack[] =
{
#ifdef INITIAL_DSTACK
    INITIAL_DSTACK
#else
    "userdict"
#endif
};

#define MIN_DSTACK_SIZE (countof(initial_dstack) + 1)	/* +1 for systemdict */


/* Detect whether we have any Level 2 operators. */
/* We export this for gs_init1 in imain.c. */
/* This is very slow, but we only call it a couple of times. */
bool
gs_have_level2(void)
{
    const op_def *const *tptr;

    for (tptr = op_defs_all; *tptr != 0; ++tptr) {
	const op_def *def;

	for (def = *tptr; def->oname != 0; ++def)
	    if (op_def_is_begin_dict(def) &&
		!strcmp(def->oname, "level2dict")
		)
		return true;
    }
    return false;
}

/* Create an initial dictionary if necessary. */
private ref *
make_initial_dict(const char *iname, ref idicts[])
{
    int i;

    /* systemdict was created specially. */
    if (!strcmp(iname, "systemdict"))
	return systemdict;
    for (i = 0; i < countof(initial_dictionaries); i++) {
	const char *dname = initial_dictionaries[i].name;
	const int dsize = initial_dictionaries[i].size;

	if (!strcmp(iname, dname)) {
	    ref *dref = &idicts[i];

	    if (r_has_type(dref, t_null)) {
		gs_ref_memory_t *mem =
		(initial_dictionaries[i].local ?
		 iimemory_local : iimemory_global);
		int code = dict_alloc(mem, dsize, dref);

		if (code < 0)
		    return 0;	/* disaster */
	    }
	    return dref;
	}
    }

    /*
     * Name mentioned in some op_def, but not in initial_dictionaries.
     * Punt.
     */
    return 0;
}

/* Initialize objects other than operators.  In particular, */
/* initialize the dictionaries that hold operator definitions. */
void
obj_init(void)
{
    bool level2 = gs_have_level2();

    /* Initialize the language level. */
    make_int(&ref_language_level, 1);

    /*
     * Create systemdict.  The context machinery requires that
     * we do this before initializing the interpreter.
     */
    dict_alloc(iimemory_global,
	       (level2 ? SYSTEMDICT_LEVEL2_SIZE : SYSTEMDICT_SIZE),
	       systemdict);

    /* Initialize the interpreter. */
    gs_interp_init();

    {
#define icount countof(initial_dictionaries)
	ref idicts[icount];
	int i;
	const op_def *const *tptr;

	min_dstack_size = MIN_DSTACK_SIZE;

	refset_null(idicts, icount);

	/* Put systemdict on the dictionary stack. */
	if (level2) {
	    dsp += 2;
	    /*
	     * For the moment, let globaldict be an alias for systemdict.
	     */
	    dsp[-1] = *systemdict;
	    min_dstack_size++;
	} else {
	    ++dsp;
	}
	*dsp = *systemdict;

	/* Create dictionaries which are to be homes for operators. */
	for (tptr = op_defs_all; *tptr != 0; tptr++) {
	    const op_def *def;

	    for (def = *tptr; def->oname != 0; def++)
		if (op_def_is_begin_dict(def))
		    make_initial_dict(def->oname, idicts);
	}

	/* Set up the initial dstack. */
	for (i = 0; i < countof(initial_dstack); i++) {
	    const char *dname = initial_dstack[i];

	    ++dsp;
	    if (!strcmp(dname, "userdict"))
		dstack_userdict_index = dsp - dsbot;
	    ref_assign(dsp, make_initial_dict(dname, idicts));
	}

	/* Enter names of referenced initial dictionaries into systemdict. */
	initial_enter_name("systemdict", systemdict);
	for (i = 0; i < icount; i++) {
	    ref *idict = &idicts[i];

	    if (!r_has_type(idict, t_null)) {	/*
						 * Note that we enter the dictionary in systemdict
						 * even if it is in local VM.  There is a special
						 * provision in the garbage collector for this:
						 * see ivmspace.h for more information.
						 * In order to do this, we must temporarily
						 * identify systemdict as local, so that the
						 * store check in dict_put won't fail.
						 */
		uint save_space = r_space(systemdict);

		r_set_space(systemdict, avm_local);
		initial_enter_name(initial_dictionaries[i].name,
				   idict);
		r_set_space(systemdict, save_space);
	    }
	}
#undef icount
    }

    gs_interp_reset();

    {
	ref vtemp;

	make_null(&vtemp);
	initial_enter_name("null", &vtemp);
	make_true(&vtemp);
	initial_enter_name("true", &vtemp);
	make_false(&vtemp);
	initial_enter_name("false", &vtemp);
    }

    /* Create the error name table */
    {
	int n = countof(gs_error_names) - 1;
	int i;
	ref era;

	ialloc_ref_array(&era, a_readonly, n, "ErrorNames");
	for (i = 0; i < n; i++)
	    name_enter((const char *)gs_error_names[i],
		       era.value.refs + i);
	initial_enter_name("ErrorNames", &era);
    }
}

/* Run the initialization procedures of the individual operator files. */
void
zop_init(void)
{
    const op_def *const *tptr;

    /* Because of a bug in Sun's SC1.0 compiler, */
    /* we have to spell out the typedef for op_def_ptr here: */
    const op_def *def;

    for (tptr = op_defs_all; *tptr != 0; tptr++) {
	for (def = *tptr; def->oname != 0; def++)
	    DO_NOTHING;
	if (def->proc != 0)
	    ((void (*)(P0()))(def->proc)) ();
    }

    /* Initialize the predefined names other than operators. */
    /* Do this here in case op_init changed any of them. */
    {
	ref vtemp;

	make_const_string(&vtemp, a_readonly | avm_foreign,
			  strlen(gs_copyright),
			  (const byte *)gs_copyright);
	initial_enter_name("copyright", &vtemp);
	make_const_string(&vtemp, a_readonly | avm_foreign,
			  strlen(gs_product),
			  (const byte *)gs_product);
	initial_enter_name("product", &vtemp);
	make_int(&vtemp, gs_revision);
	initial_enter_name("revision", &vtemp);
	make_int(&vtemp, gs_revisiondate);
	initial_enter_name("revisiondate", &vtemp);
    }
}

/* Create an op_array table. */
private int
alloc_op_array_table(uint size, uint space, op_array_table * opt)
{
    uint save_space = ialloc_space(idmemory);
    int code;

    ialloc_set_space(idmemory, space);
    code = ialloc_ref_array(&opt->table, a_readonly, size,
			    "op_array table");
    ialloc_set_space(idmemory, save_space);
    if (code < 0)
	return code;
    refset_null(opt->table.value.refs, size);
    opt->nx_table =
	(ushort *) ialloc_byte_array(size, sizeof(ushort),
				     "op_array nx_table");
    if (opt->nx_table == 0)
	return_error(e_VMerror);
    opt->count = 0;
    opt->root_p = &opt->table;
    opt->attrs = space | a_executable;
    return 0;
}

/* Initialize the operator table. */
void
op_init(void)
{
    int count = 1;
    const op_def *const *tptr;
    const op_def *def;
    const char *nstr;

    /* Do a first pass just to count the operators. */

    for (tptr = op_defs_all; *tptr != 0; tptr++) {
	for (def = *tptr; def->oname != 0; def++)
	    if (!op_def_is_begin_dict(def))
		count++;
    }

    /* Do a second pass to construct the operator table, */
    /* and enter the operators into the appropriate dictionary. */

    /* Because of a bug in Sun's SC1.0 compiler, */
    /* we have to spell out the typedef for op_def_ptr here: */
    op_def_table =
	(const op_def **)ialloc_byte_array(count, sizeof(const op_def *),
					   "op_init(op_def_table)");

    op_def_count = count;
    for (count = 0; count <= gs_interp_num_special_ops; count++)
	op_def_table[count] = 0;
    count = gs_interp_num_special_ops + 1;	/* leave space for magic entries */
    for (tptr = op_defs_all; *tptr != 0; tptr++) {
	ref *pdict = systemdict;

	for (def = *tptr; (nstr = def->oname) != 0; def++)
	    if (op_def_is_begin_dict(def)) {
		ref nref;
		int code = name_ref((const byte *)nstr, strlen(nstr),
				    &nref, -1);

		if (code != 0)
		    gs_abort();
		if (!dict_find(systemdict, &nref, &pdict))
		    gs_abort();
		if (!r_has_type(pdict, t_dictionary))
		    gs_abort();
	    } else {
		ref oper;
		uint opidx;

		gs_interp_make_oper(&oper, def->proc, count);
		opidx = r_size(&oper);
		/* The first character of the name is a digit */
		/* giving the minimum acceptable number of operands. */
		/* Check to make sure it's within bounds. */
		if (*nstr - '0' > gs_interp_max_op_num_args)
		    gs_abort();
		nstr++;
		/* Don't enter internal operators into */
		/* the dictionary. */
		if (*nstr != '%')
		    initial_enter_name_in(nstr, &oper, pdict);
		op_def_table[opidx] = def;
		if (opidx == count)
		    count++;
	    }
    }
    /* All of the built-ins had better be defined somewhere, */
    /* or things like op_find_index will choke. */
    for (count = 1; count <= gs_interp_num_special_ops; count++)
	if (op_def_table[count] == 0)
	    gs_abort();
    gs_register_struct_root(imemory, NULL, (void **)&op_def_table,
			    "op_def_table");

    /* Allocate the tables for `operator' procedures. */
    /* Make one of them local so we can have local operators. */

    if (alloc_op_array_table(OP_ARRAY_TABLE_GLOBAL_SIZE,
			     avm_global, &op_array_table_global) < 0)
	gs_abort();
    op_array_table_global.base_index = op_def_count;
    gs_register_ref_root(imemory, NULL,
			 (void **)&op_array_table_global.root_p,
			 "op_array_table(global)");
    gs_register_struct_root(imemory, NULL,
			    (void **)&op_array_table_global.nx_table,
			    "op_array nx_table(global)");

    if (alloc_op_array_table(OP_ARRAY_TABLE_LOCAL_SIZE,
			     avm_local, &op_array_table_local) < 0)
	gs_abort();
    op_array_table_local.base_index =
	op_array_table_global.base_index +
	r_size(&op_array_table_global.table);
    gs_register_ref_root(imemory, NULL,
			 (void **)&op_array_table_local.root_p,
			 "op_array_table(local)");
    gs_register_struct_root(imemory, NULL,
			    (void **)&op_array_table_local.nx_table,
			    "op_array nx_table(local)");

}
