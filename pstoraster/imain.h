/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* imain.h */
/* Interface to imain.c */
/* Requires <stdio.h>, stdpre.h, gsmemory.h, gstypes.h, iref.h */

#ifndef imain_INCLUDED
#  define imain_INCLUDED

#include "gsexit.h"			/* exported by imain.c */

/*
 * This file defines the intended API between client front ends
 * (such as imainarg.c, the command-line-driven front end)
 * and imain.c, which provides top-level control of the interpreter.
 */

/* ================ Types ================ */

/*
 * Currently, the interpreter has a lot of static variables, but
 * eventually it will have none, so that clients will be able to make
 * multiple instances of it.  In anticipation of this, many of the
 * top-level API calls take an interpreter instance (gs_main_instance *)
 * as their first argument.
 */
#ifndef gs_main_instance_DEFINED
#  define gs_main_instance_DEFINED
typedef struct gs_main_instance_s gs_main_instance;
#endif

/* ================ Exported procedures from imain.c ================ */

/* ---------------- Instance creation ---------------- */

/*
 * As noted above, multiple instances are not supported yet:
 */
/*gs_main_instance *gs_main_alloc_instance(P1(gs_memory_t *));*/
/*
 * Instead, we provide only a default instance:
 */
gs_main_instance *gs_main_instance_default(P0());

/* ---------------- Initialization ---------------- */

/*
 * The interpreter requires three initialization steps, called init0,
 * init1, and init2.  These steps must be done in that order, but
 * init1 may be omitted.
 */

/*
 * Since gsio.h (which is included in many other header files)
 * redefines stdin/out/err, callers need a way to get the "real"
 * stdio files to pass to init0 if they wish to do so.
 */
void gs_get_real_stdio(P1(FILE *stdfiles[3]));

/*
 * init0 records the files to be used for stdio, and initializes the
 * graphics library, the file search paths, and other instance data.
 */
void gs_main_init0(P5(gs_main_instance *minst, FILE *in, FILE *out, FILE *err,
		      int max_lib_paths));

/*
 * init1 initializes the memory manager and other internal data
 * structures such as the name table, the token scanner tables,
 * dictionaries such as systemdict, and the interpreter stacks.
 */
void gs_main_init1(P1(gs_main_instance *minst));

/*
 * init2 finishes preparing the interpreter for use by running
 * initialization files with PostScript procedure definitions.
 */
void gs_main_init2(P1(gs_main_instance *minst));

/*
 * The runlibfile operator uses a search path, as described in
 * use.doc, for looking up file names.  Each interpreter instance has
 * its own search path.  The following call adds a directory or set of
 * directories to the search path; it is equivalent to the -I command
 * line switch.  It may be called any time after init0.
 */
void gs_main_add_lib_path(P2(gs_main_instance *minst, const char *path));
/*
 * Under certain internal conditions, the search path may temporarily
 * be in an inconsistent state; gs_main_set_lib_paths takes care of
 * this.  Clients should never need to call this procedure, and
 * eventually it may be removed.
 */
void gs_main_set_lib_paths(P1(gs_main_instance *minst));

/*
 * Open a PostScript file using the search path.  Clients should
 * never need to call this procedure, since gs_main_run_file opens the
 * file itself, and eventually the procedure may be removed.
 */
int gs_main_lib_open(P3(gs_main_instance *minst, const char *fname,
			ref *pfile));

/*
 * Here we summarize the C API calls that correspond to some of the
 * most common command line switches documented in use.doc, to help
 * clients who are familiar with the command line and are starting to
 * use the API.
 *
 *	-d/D, -s/S (for setting device parameters like OutputFile)
 *		Use the C API for device parameters documented near the
 *		end of gsparam.h.
 *
 *	-d/D (for setting Boolean parameters like NOPAUSE)
 *		{ ref vtrue;
 *		  make_true(&vtrue);
 *		  dict_put_string(systemdict, "NOPAUSE", &vtrue);
 *		}
 *	-I
 *		Use gs_main_add_lib_path, documented above.
 *
 *	-A, -A-
 *		Set gs_debug['@'] = 1 or 0 respectively.
 *	-E, -E-
 *		Set gs_debug['#'] = 1 or 0 respectively.
 *	-Z..., -Z-...
 *		Set gs_debug[c] = 1 or 0 respectively for each character
 *		c in the string.
 */

/* ---------------- Execution ---------------- */

/*
 * After initializing the interpreter, clients may pass it files or
 * strings to be interpreted.  There are four ways to do this:
 *	- Pass a file name (gs_main_run_file);
 *	- Pass a C string (gs_main_run_string);
 *	- Pass a string defined by pointer and length
 *		(gs_main_run_string_with_length);
 *	- Pass strings piece-by-piece
 *		(gs_main_run_string_begin/continue/end).
 *
 * The value returned by the first three of these calls is
 * 0 if the interpreter ran to completion, e_Quit for a normal quit,
 * or e_Fatal for a non-zero quit or a fatal error.
 * e_Fatal stores the exit code in the third argument.
 * The str argument of gs_main_run_string[_with_length] must be allocated
 * in non-garbage-collectable space (e.g., by malloc or gs_malloc,
 * or statically).
 */
int gs_main_run_file(P5(gs_main_instance *minst,
			const char *fname,
			int user_errors, int *pexit_code,
			ref *perror_object));
int gs_main_run_string(P5(gs_main_instance *minst,
			  const char *str,
			  int user_errors, int *pexit_code,
			  ref *perror_object));
int gs_main_run_string_with_length(P6(gs_main_instance *minst,
				      const char *str, uint length,
				      int user_errors, int *pexit_code,
				      ref *perror_object));

/*
 * Open the file for gs_main_run_file.  This is an internal routine
 * that is only exported for some special clients.
 */
int gs_main_run_file_open(P3(gs_main_instance *minst,
			     const char *file_name, ref *pfref));

/*
 * The next 3 procedures provide for feeding input to the interpreter
 * in arbitrary chunks, unlike run_string, which requires that each string
 * be a properly formed PostScript program fragment.  To use them:
 *	Call run_string_begin.
 *	Call run_string_continue as many times as desired,
 *	  stopping if it returns anything other than e_NeedInput.
 *	If run_string_continue didn't indicate an error or a quit
 *	  (i.e., a return value other than e_NeedInput), call run_string_end
 *	  to provide an EOF indication.
 * Note that run_string_continue takes a pointer and a length, like
 * run_string_with_length.
 */
int gs_main_run_string_begin(P4(gs_main_instance *minst, int user_errors,
				int *pexit_code, ref *perror_object));
int gs_main_run_string_continue(P6(gs_main_instance *minst,
				   const char *str, uint length,
				   int user_errors, int *pexit_code,
				   ref *perror_object));
int gs_main_run_string_end(P4(gs_main_instance *minst, int user_errors,
			      int *pexit_code, ref *perror_object));

/* ---------------- Operand stack access ---------------- */

/*
 * The following procedures are not used in normal operation;
 * they exist only to allow clients driving the interpreter through the
 * gs_main_run_xxx procedures to push parameters quickly and to get results
 * back.  The push procedures return 0, e_stackoverflow, or e_VMerror;
 * the pop procedures return 0, e_stackunderflow, or e_typecheck.
 *
 * Procedures to push values on the operand stack:
 */
int gs_push_boolean(P2(gs_main_instance *minst, bool value));
int gs_push_integer(P2(gs_main_instance *minst, long value));
int gs_push_real(P2(gs_main_instance *minst, floatp value));
int gs_push_string(P4(gs_main_instance *minst, byte *chars, uint length,
		      bool read_only));
/*
 * Procedures to pop values from the operand stack:
 */
int gs_pop_boolean(P2(gs_main_instance *minst, bool *result));
int gs_pop_integer(P2(gs_main_instance *minst, long *result));
int gs_pop_real(P2(gs_main_instance *minst, float *result));
/* gs_pop_string returns 1 if the string is read-only. */
int gs_pop_string(P2(gs_main_instance *minst, gs_string *result));

/* ---------------- Debugging ---------------- */

/*
 * Print an error mesage including the error code, error object (if any),
 * and operand and execution stacks in hex.  Clients will probably
 * never call this.
 */
void gs_debug_dump_stack(P2(int code, ref *perror_object));

/* ---------------- Termination ---------------- */

/*
 * Terminate the interpreter by closing all devices and releasing all
 * allocated memory.  Currently, because of some technical problems
 * with statically initialized data, it is not possible to reinitialize
 * the interpreter after terminating it; we plan to fix this as soon as
 * possible.
 *
 * Note that calling gs_exit (defined in gsexit.h) automatically calls
 * gs_main_finit for the default instance.
 */
void gs_main_finit(P3(gs_main_instance *minst, int exit_status, int code));

/* ================ Other exported procedures ================ */

/*
 * Define an internal interface to the interpreter.  Clients do not
 * normally use this.
 */
int gs_interpret(P4(ref *pref, int user_errors, int *pexit_code,
		    ref *perror_object));

#endif					/* imain_INCLUDED */
