/* Copyright (C) 1989, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: imain.c,v 1.2 2000/03/08 23:15:12 mike Exp $ */
/* Common support for interpreter front ends */
#include "memory_.h"
#include "string_.h"
/* Capture stdin/out/err before gs.h redefines them. */
#include <stdio.h>
void
gs_get_real_stdio(FILE * stdfiles[3])
{
    stdfiles[0] = stdin;
    stdfiles[1] = stdout;
    stdfiles[2] = stderr;
}
#include "ghost.h"
#include "gp.h"
#include "gslib.h"
#include "gsmatrix.h"		/* for gxdevice.h */
#include "gsutil.h"		/* for bytes_compare */
#include "gxdevice.h"
#include "errors.h"
#include "oper.h"
#include "idebug.h"
#include "idict.h"
#include "iname.h"		/* for name_init */
#include "dstack.h"
#include "estack.h"
#include "ostack.h"		/* put here for files.h */
#include "stream.h"		/* for files.h */
#include "files.h"
#include "ialloc.h"
#include "strimpl.h"		/* for sfilter.h */
#include "sfilter.h"		/* for iscan.h */
#include "iscan.h"
#include "main.h"
#include "store.h"
#include "isave.h"		/* for prototypes */
#include "interp.h"
#include "ivmspace.h"

/* ------ Exported data ------ */

/* Define the default instance of the interpreter. */
/* Currently, this is the *only possible* instance, because most of */
/* the places that need to take an explicit instance argument don't. */
private gs_main_instance the_gs_main_instance;
gs_main_instance *
gs_main_instance_default(void)
{				/* Determine whether the instance has been initialized. */
    if (the_gs_main_instance.memory_chunk_size == 0)
	the_gs_main_instance = gs_main_instance_init_values;
    return &the_gs_main_instance;
}

/* The only reason we export gs_exit_status is so that window systems */
/* with alert boxes can know whether to pause before exiting if */
/* the program terminates with an error.  There must be a better way .... */
int gs_exit_status;

/* Define the interpreter's name table.  We'll move it somewhere better */
/* eventually.... */
name_table *the_gs_name_table;

/* ------ Imported data ------ */

/* Configuration information imported from gconfig.c and iinit.c. */
extern const char *gs_init_file;
extern const byte gs_init_string[];
extern const uint gs_init_string_sizeof;
extern const ref gs_init_file_array[];
extern const ref gs_emulator_name_array[];

/* ------ Forward references ------ */

private int gs_run_init_file(P3(gs_main_instance *, int *, ref *));
private void print_resource_usage(P3(const gs_main_instance *,
				     gs_dual_memory_t *,
				     const char *));

/* ------ Initialization ------ */

/* A handy way to declare and execute an initialization procedure: */
#define call_init(proc)\
BEGIN extern void proc(P0()); proc(); END

/* Initialization to be done before anything else. */
void
gs_main_init0(gs_main_instance * minst, FILE * in, FILE * out, FILE * err,
	      int max_lib_paths)
{
    gs_memory_t *heap;

    /* Set our versions of stdin/out/err. */
    gs_stdin = minst->fstdin = in;
    gs_stdout = minst->fstdout = out;
    gs_stderr = minst->fstderr = err;
    /* Do platform-dependent initialization. */
    /* We have to do this as the very first thing, */
    /* because it detects attempts to run 80N86 executables (N>0) */
    /* on incompatible processors. */
    gp_init();
    gp_get_usertime(minst->base_time);
    /* Initialize the imager. */
    heap = gs_lib_init0(gs_stdout);
    minst->heap = heap;
    /* Initialize the file search paths. */
    make_array(&minst->lib_path.container, avm_foreign, max_lib_paths,
	       (ref *) gs_alloc_byte_array(heap, max_lib_paths, sizeof(ref),
					   "lib_path array"));
    make_array(&minst->lib_path.list, avm_foreign | a_readonly, 0,
	       minst->lib_path.container.value.refs);
    minst->lib_path.env = 0;
    minst->lib_path.final = 0;
    minst->lib_path.count = 0;
    minst->user_errors = 1;
    minst->init_done = 0;
}

/* Initialization to be done before constructing any objects. */
void
gs_main_init1(gs_main_instance * minst)
{
    if (minst->init_done < 1) {
	{
	    extern bool gs_have_level2(P0());

	    ialloc_init((gs_raw_memory_t *) & gs_memory_default,
			minst->memory_chunk_size,
			gs_have_level2());
	    gs_lib_init1((gs_memory_t *) imemory_system);
	    alloc_save_init(idmemory);
	}
	{
	    gs_memory_t *mem = imemory_system;
	    name_table *nt = names_init(minst->name_table_size, mem);

	    if (nt == 0) {
		puts("name_init failed");
		gs_exit(1);
	    }
	    the_gs_name_table = nt;
	    gs_register_struct_root(mem, NULL, (void **)&the_gs_name_table,
				    "the_gs_name_table");
	}
	call_init(obj_init);	/* requires name_init */
	minst->init_done = 1;
    }
}

/* Initialization to be done before running any files. */
private void
init2_make_string_array(const ref * srefs, const char *aname)
{
    const ref *ifp = srefs;
    ref ifa;

    for (; ifp->value.bytes != 0; ifp++);
    make_tasv(&ifa, t_array, a_readonly | avm_foreign,
	      ifp - srefs, const_refs, srefs);
    initial_enter_name(aname, &ifa);
}
void
gs_main_init2(gs_main_instance * minst)
{
    gs_main_init1(minst);
    if (minst->init_done < 2) {
	int code, exit_code;
	ref error_object;

	call_init(igs_init);
	call_init(zop_init);
	{
	    extern void gs_iodev_init(P1(gs_memory_t *));

	    gs_iodev_init(imemory);
	}
	call_init(op_init);	/* requires obj_init */

	/* Set up the array of additional initialization files. */
	init2_make_string_array(gs_init_file_array, "INITFILES");
	/* Set up the array of emulator names. */
	init2_make_string_array(gs_emulator_name_array, "EMULATORS");
	/* Pass the search path. */
	initial_enter_name("LIBPATH", &minst->lib_path.list);

	/* Execute the standard initialization file. */
	code = gs_run_init_file(minst, &exit_code, &error_object);
	if (code < 0) {
	    if (code != e_Fatal)
		gs_debug_dump_stack(code, &error_object);
	    gs_exit_with_code((exit_code ? exit_code : 2), code);
	}
	minst->init_done = 2;
    }
    if (gs_debug_c(':'))
	print_resource_usage(minst, &gs_imemory, "Start");
}

/* ------ Search paths ------ */

/* Internal routine to add a set of directories to a search list. */
/* Returns 0 or an error code. */
private int
file_path_add(gs_file_path * pfp, const char *dirs)
{
    uint len = r_size(&pfp->list);
    const char *dpath = dirs;

    if (dirs == 0)
	return 0;
    for (;;) {			/* Find the end of the next directory name. */
	const char *npath = dpath;

	while (*npath != 0 && *npath != gp_file_name_list_separator)
	    npath++;
	if (npath > dpath) {
	    if (len == r_size(&pfp->container))
		return_error(e_limitcheck);
	    make_const_string(&pfp->container.value.refs[len],
			      avm_foreign | a_readonly,
			      npath - dpath, (const byte *)dpath);
	    ++len;
	}
	if (!*npath)
	    break;
	dpath = npath + 1;
    }
    r_set_size(&pfp->list, len);
    return 0;
}

/* Add a library search path to the list. */
void
gs_main_add_lib_path(gs_main_instance * minst, const char *lpath)
{				/* Account for the possibility that the first element */
    /* is gp_current_directory name added by set_lib_paths. */
    int first_is_here =
    (r_size(&minst->lib_path.list) != 0 &&
     minst->lib_path.container.value.refs[0].value.bytes ==
     (const byte *)gp_current_directory_name ? 1 : 0);

    r_set_size(&minst->lib_path.list, minst->lib_path.count +
	       first_is_here);
    file_path_add(&minst->lib_path, lpath);
    minst->lib_path.count = r_size(&minst->lib_path.list) - first_is_here;
    gs_main_set_lib_paths(minst);
}

/* ------ Execution ------ */

/* Complete the list of library search paths. */
/* This may involve adding or removing the current directory */
/* as the first element. */
void
gs_main_set_lib_paths(gs_main_instance * minst)
{
    ref *paths = minst->lib_path.container.value.refs;
    int first_is_here =
    (r_size(&minst->lib_path.list) != 0 &&
    paths[0].value.bytes == (const byte *)gp_current_directory_name ? 1 : 0);
    int count = minst->lib_path.count;

    if (minst->search_here_first) {
	if (!(first_is_here ||
	      (r_size(&minst->lib_path.list) != 0 &&
	       !bytes_compare((const byte *)gp_current_directory_name,
			      strlen(gp_current_directory_name),
			      paths[0].value.bytes,
			      r_size(&paths[0]))))
	    ) {
	    memmove(paths + 1, paths, count * sizeof(*paths));
	    make_const_string(paths, avm_foreign | a_readonly,
			      strlen(gp_current_directory_name),
			      (const byte *)gp_current_directory_name);
	}
    } else {
	if (first_is_here)
	    memmove(paths, paths + 1, count * sizeof(*paths));
    }
    r_set_size(&minst->lib_path.list,
	       count + (minst->search_here_first ? 1 : 0));
    if (minst->lib_path.env != 0)
	file_path_add(&minst->lib_path, minst->lib_path.env);
    if (minst->lib_path.final != 0)
	file_path_add(&minst->lib_path, minst->lib_path.final);
}

/* Open a file, using the search paths. */
int
gs_main_lib_open(gs_main_instance * minst, const char *file_name, ref * pfile)
{				/* This is a separate procedure only to avoid tying up */
    /* extra stack space while running the file. */
#define maxfn 200
    byte fn[maxfn];
    uint len;

    return lib_file_open(file_name, strlen(file_name), fn, maxfn,
			 &len, pfile);
}

/* Open and execute a file. */
int
gs_main_run_file(gs_main_instance * minst, const char *file_name, int user_errors, int *pexit_code, ref * perror_object)
{
    ref initial_file;
    int code = gs_main_run_file_open(minst, file_name, &initial_file);

    if (code < 0)
	return code;
    return gs_interpret(&initial_file, user_errors, pexit_code, perror_object);
}
int
gs_main_run_file_open(gs_main_instance * minst, const char *file_name, ref * pfref)
{
    gs_main_set_lib_paths(minst);
    if (gs_main_lib_open(minst, file_name, pfref) < 0) {
	eprintf1("Can't find initialization file %s.\n", file_name);
	return_error(e_Fatal);
    }
    r_set_attrs(pfref, a_execute + a_executable);
    return 0;
}

/* Open and run the very first initialization file. */
private int
gs_run_init_file(gs_main_instance * minst, int *pexit_code, ref * perror_object)
{
    ref ifile;
    ref first_token;
    int code;
    scanner_state state;

    gs_main_set_lib_paths(minst);
    if (gs_init_string_sizeof == 0) {	/* Read from gs_init_file. */
	code = gs_main_run_file_open(minst, gs_init_file, &ifile);
    } else {			/* Read from gs_init_string. */
	code = file_read_string(gs_init_string, gs_init_string_sizeof,
				&ifile);
    }
    if (code < 0) {
	*pexit_code = 255;
	return code;
    }
    /* Check to make sure the first token is an integer */
    /* (for the version number check.) */
    scanner_state_init(&state, false);
    code = scan_token(ifile.value.pfile, &first_token, &state);
    if (code != 0 || !r_has_type(&first_token, t_integer)) {
	eprintf1("Initialization file %s does not begin with an integer.\n", gs_init_file);
	*pexit_code = 255;
	return_error(e_Fatal);
    }
    *++osp = first_token;
    r_set_attrs(&ifile, a_executable);
    return gs_interpret(&ifile, minst->user_errors,
			pexit_code, perror_object);
}

/* Run a string. */
int
gs_main_run_string(gs_main_instance * minst, const char *str, int user_errors,
		   int *pexit_code, ref * perror_object)
{
    return gs_main_run_string_with_length(minst, str, (uint) strlen(str),
					  user_errors,
					  pexit_code, perror_object);
}
int
gs_main_run_string_with_length(gs_main_instance * minst, const char *str,
	 uint length, int user_errors, int *pexit_code, ref * perror_object)
{
    int code;

    code = gs_main_run_string_begin(minst, user_errors,
				    pexit_code, perror_object);
    if (code < 0)
	return code;
    code = gs_main_run_string_continue(minst, str, length, user_errors,
				       pexit_code, perror_object);
    if (code != e_NeedInput)
	return code;
    return gs_main_run_string_end(minst, user_errors,
				  pexit_code, perror_object);
}

/* Set up for a suspendable run_string. */
int
gs_main_run_string_begin(gs_main_instance * minst, int user_errors,
			 int *pexit_code, ref * perror_object)
{
    const char *setup = ".runstringbegin";
    ref rstr;
    int code;

    gs_main_set_lib_paths(minst);
    make_const_string(&rstr, avm_foreign | a_readonly | a_executable,
		      strlen(setup), (const byte *)setup);
    code = gs_interpret(&rstr, user_errors, pexit_code, perror_object);
    return (code == e_NeedInput ? 0 : code == 0 ? e_Fatal : code);
}
/* Continue running a string with the option of suspending. */
int
gs_main_run_string_continue(gs_main_instance * minst, const char *str,
	 uint length, int user_errors, int *pexit_code, ref * perror_object)
{
    ref rstr;

    if (length == 0)
	return 0;		/* empty string signals EOF */
    make_const_string(&rstr, avm_foreign | a_readonly, length,
		      (const byte *)str);
    return gs_interpret(&rstr, user_errors, pexit_code, perror_object);
}
/* Signal EOF when suspended. */
int
gs_main_run_string_end(gs_main_instance * minst, int user_errors,
		       int *pexit_code, ref * perror_object)
{
    ref rstr;

    make_empty_const_string(&rstr, avm_foreign | a_readonly);
    return gs_interpret(&rstr, user_errors, pexit_code, perror_object);
}

/* ------ Operand stack access ------ */

/* These are built for comfort, not for speed. */

private int
push_value(ref * pvalue)
{
    int code = ref_stack_push(&o_stack, 1);

    if (code < 0)
	return code;
    *ref_stack_index(&o_stack, 0L) = *pvalue;
    return 0;
}

int
gs_push_boolean(gs_main_instance * minst, bool value)
{
    ref vref;

    make_bool(&vref, value);
    return push_value(&vref);
}

int
gs_push_integer(gs_main_instance * minst, long value)
{
    ref vref;

    make_int(&vref, value);
    return push_value(&vref);
}

int
gs_push_real(gs_main_instance * minst, floatp value)
{
    ref vref;

    make_real(&vref, value);
    return push_value(&vref);
}

int
gs_push_string(gs_main_instance * minst, byte * chars, uint length,
	       bool read_only)
{
    ref vref;

    make_string(&vref, avm_foreign | (read_only ? a_readonly : a_all),
		length, (byte *) chars);
    return push_value(&vref);
}

private int
pop_value(ref * pvalue)
{
    if (!ref_stack_count(&o_stack))
	return_error(e_stackunderflow);
    *pvalue = *ref_stack_index(&o_stack, 0L);
    return 0;
}

int
gs_pop_boolean(gs_main_instance * minst, bool * result)
{
    ref vref;
    int code = pop_value(&vref);

    if (code < 0)
	return code;
    check_type_only(vref, t_boolean);
    *result = vref.value.boolval;
    ref_stack_pop(&o_stack, 1);
    return 0;
}

int
gs_pop_integer(gs_main_instance * minst, long *result)
{
    ref vref;
    int code = pop_value(&vref);

    if (code < 0)
	return code;
    check_type_only(vref, t_integer);
    *result = vref.value.intval;
    ref_stack_pop(&o_stack, 1);
    return 0;
}

int
gs_pop_real(gs_main_instance * minst, float *result)
{
    ref vref;
    int code = pop_value(&vref);

    if (code < 0)
	return code;
    switch (r_type(&vref)) {
	case t_real:
	    *result = vref.value.realval;
	    break;
	case t_integer:
	    *result = vref.value.intval;
	    break;
	default:
	    return_error(e_typecheck);
    }
    ref_stack_pop(&o_stack, 1);
    return 0;
}

int
gs_pop_string(gs_main_instance * minst, gs_string * result)
{
    ref vref;
    int code = pop_value(&vref);

    if (code < 0)
	return code;
    switch (r_type(&vref)) {
	case t_name:
	    name_string_ref(&vref, &vref);
	    code = 1;
	    goto rstr;
	case t_string:
	    code = (r_has_attr(&vref, a_write) ? 0 : 1);
	  rstr:result->data = vref.value.bytes;
	    result->size = r_size(&vref);
	    break;
	default:
	    return_error(e_typecheck);
    }
    ref_stack_pop(&o_stack, 1);
    return code;
}

/* ------ Termination ------ */

/* Free all resources and exit. */
void
gs_main_finit(gs_main_instance * minst, int exit_status, int code)
{	/*
	 * Previous versions of this code closed the devices in the
	 * device list here.  Since these devices are now prototypes,
	 * they cannot be opened, so they do not need to be closed;
	 * alloc_restore_all will close dynamically allocated devices.
	 */
    gs_exit_status = exit_status;	/* see above */

    if (gs_debug_c(':'))
	print_resource_usage(minst, &gs_imemory, "Final");
    /* Do the equivalent of a restore "past the bottom". */
    /* This will release all memory, close all open files, etc. */
    if (minst->init_done >= 1)
	alloc_restore_all(idmemory);
    gs_lib_finit(exit_status, code);
}
void
gs_exit_with_code(int exit_status, int code)
{
    gs_finit(exit_status, code);
    gp_do_exit(exit_status);
}
void
gs_exit(int exit_status)
{
    gs_exit_with_code(exit_status, 0);
}

/* ------ Debugging ------ */

/* Print resource usage statistics. */
private void
print_resource_usage(const gs_main_instance * minst, gs_dual_memory_t * dmem,
		     const char *msg)
{
    ulong allocated = 0, used = 0;
    long utime[2];

    gp_get_usertime(utime);
    {
	int i;

	for (i = 0; i < countof(dmem->spaces.indexed); ++i) {
	    gs_ref_memory_t *mem = dmem->spaces.indexed[i];

	    if (mem != 0 && (i == 0 || mem != dmem->spaces.indexed[i - 1])) {
		gs_memory_status_t status;

		gs_memory_status((gs_memory_t *) mem, &status);
		allocated += status.allocated;
		used += status.used;
	    }
	}
    }
    dprintf4("%% %s time = %g, memory allocated = %lu, used = %lu\n",
	     msg, utime[0] - minst->base_time[0] +
	     (utime[1] - minst->base_time[1]) / 1000000000.0,
	     allocated, used);
}

/* Dump the stacks after interpretation */
void
gs_debug_dump_stack(int code, ref * perror_object)
{
    zflush(osp);		/* force out buffered output */
    dprintf1("\nUnexpected interpreter error %d.\n", code);
    if (perror_object != 0) {
	dputs("Error object: ");
	debug_print_ref(perror_object);
	dputc('\n');
    }
    debug_dump_stack(&o_stack, "Operand stack");
    debug_dump_stack(&e_stack, "Execution stack");
    debug_dump_stack(&d_stack, "Dictionary stack");
}
