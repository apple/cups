/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: imainarg.c,v 1.5 2000/05/23 12:19:48 mike Exp $ */
/* Command line parsing and dispatching */
#include "ctype_.h"
#include "memory_.h"
#include "string_.h"
#include "ghost.h"
#include "gp.h"
#include "gsargs.h"
#include "gscdefs.h"
#include "gsmalloc.h"		/* for gs_malloc_limit */
#include "gsmdebug.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gsdevice.h"
#include "stream.h"
#include "errors.h"
#include "estack.h"
#include "ialloc.h"
#include "strimpl.h"		/* for sfilter.h */
#include "sfilter.h"		/* for iscan.h */
#include "ostack.h"		/* must precede iscan.h */
#include "iscan.h"
#include "imain.h"
#include "imainarg.h"
#include "iminst.h"
#include "iname.h"
#include "store.h"
#include "files.h"		/* requires stream.h */
#include "interp.h"
#include "iutil.h"
#include "ivmspace.h"

/* Import operator procedures */
extern int zflush(P1(os_ptr));
extern int zflushpage(P1(os_ptr));

#ifndef GS_LIB
#  define GS_LIB "GS_LIB"
#endif

#ifndef GS_OPTIONS
#  define GS_OPTIONS "GS_OPTIONS"
#endif

#ifndef GS_MAX_LIB_DIRS
#  define GS_MAX_LIB_DIRS 25
#endif

#ifndef GS_BUG_MAILBOX
#  define GS_BUG_MAILBOX "ghost@aladdin.com"
#endif

#define MAX_BUFFERED_SIZE 1024

/* Note: sscanf incorrectly defines its first argument as char * */
/* rather than const char *.  This accounts for the ugly casts below. */

/* Redefine puts to use fprintf, so it will work even without stdio. */
#undef puts
private void
fpputs(const char *str)
{
    fprintf(stderr, "%s\n", str);
}
#define puts(str) fpputs(str)

/* Other imported data */
extern const char *const gs_doc_directory;
extern const char *const gs_lib_default_path;
extern const ref gs_emulator_name_array[];

/* Forward references */
#define runInit 1
#define runFlush 2
#define runBuffer 4
private int swproc(P3(gs_main_instance *, const char *, arg_list *));
private void argproc(P2(gs_main_instance *, const char *));
private void run_buffered(P2(gs_main_instance *, const char *));
private int esc_strlen(P1(const char *));
private void esc_strcat(P2(char *, const char *));
private void runarg(P5(gs_main_instance *, const char *, const char *, const char *, int));
private void run_string(P3(gs_main_instance *, const char *, int));
private void run_finish(P3(int, int, ref *));

/* Forward references for help printout */
private void print_help(P1(gs_main_instance *));
private void print_revision(P0());
private void print_version(P0());
private void print_usage(P0());
private void print_devices(P0());
private void print_emulators(P0());
private void print_paths(P1(gs_main_instance *));
private void print_help_trailer(P0());

/* ------ Main program ------ */

/* Process the command line with a given instance. */
private FILE *
gs_main_arg_fopen(const char *fname, void *vminst)
{
    gs_main_set_lib_paths((gs_main_instance *) vminst);
    return lib_fopen(fname);
}
#define arg_heap_copy(str) arg_copy(str, &gs_memory_default)
int
gs_main_init_with_args(gs_main_instance * minst, int argc, char *argv[])
{
    const char *arg;
    arg_list args;
    FILE *stdfiles[3];

    gs_get_real_stdio(stdfiles);
    arg_init(&args, (const char **)argv, argc,
	     gs_main_arg_fopen, (void *)minst);
    gs_main_init0(minst, stdfiles[0], stdfiles[1], stdfiles[2],
		  GS_MAX_LIB_DIRS);
    {
	int len = 0;
	int code = gp_getenv(GS_LIB, (char *)0, &len);

	if (code < 0) {		/* key present, value doesn't fit */
	    char *path = (char *)gs_alloc_bytes(minst->heap, len, "GS_LIB");

	    gp_getenv(GS_LIB, path, &len);	/* can't fail */
	    minst->lib_path.env = path;
	}
    }
    minst->lib_path.final = gs_lib_default_path;
    gs_main_set_lib_paths(minst);
    /* Prescan the command line for --help and --version. */
    {
	int i;
	bool helping = false;

	for (i = 1; i < argc; ++i)
	    if (!strcmp(argv[i], "--")) {	/* A PostScript program will be interpreting all the */
		/* remaining switches, so stop scanning. */
		helping = false;
		break;
	    } else if (!strcmp(argv[i], "--help")) {
		print_help(minst);
		helping = true;
	    } else if (!strcmp(argv[i], "--version")) {
		print_version();
		puts("");	/* \n */
		helping = true;
	    }
	if (helping)
	    gs_exit(gs_exit_INFO);
    }
    /* Execute files named in the command line, */
    /* processing options along the way. */
    /* Wait until the first file name (or the end */
    /* of the line) to finish initialization. */
    minst->run_start = true;

    {
	int len = 0;
	int code = gp_getenv(GS_OPTIONS, (char *)0, &len);

	if (code < 0) {		/* key present, value doesn't fit */
	    char *opts =
	    (char *)gs_alloc_bytes(minst->heap, len, "GS_OPTIONS");

	    gp_getenv(GS_OPTIONS, opts, &len);	/* can't fail */
	    arg_push_memory_string(&args, opts, minst->heap);
	}
    }
    while ((arg = arg_next(&args)) != 0) {
	switch (*arg) {
	    case '-':
		if (swproc(minst, arg, &args) < 0)
		    fprintf(stderr,
			    "Unknown switch %s - ignoring\n", arg);
		break;
	    default:
		argproc(minst, arg);
	}
    }

    gs_main_init2(minst);

    return 0;
}

/* Run the 'start' procedure (after processing the command line). */
/* Note that this procedure exits rather than returning. */
void
gs_main_run_start(gs_main_instance * minst)
{
    run_string(minst, "systemdict /start get exec", runFlush);
}

/* Process switches */
private int
swproc(gs_main_instance * minst, const char *arg, arg_list * pal)
{
    char sw = arg[1];
    ref vtrue;

    make_true(&vtrue);
    arg += 2;			/* skip - and letter */
    switch (sw) {
	default:
	    return -1;
	case 0:		/* read stdin as a file */
	    minst->run_start = false;	/* don't run 'start' */
	    /* Set NOPAUSE so showpage won't try to read from stdin. */
	    swproc(minst, "-dNOPAUSE", pal);
	    gs_main_init2(minst);	/* Finish initialization */
	    /* We delete this only to make Ghostview work properly. */
/**************** This is WRONG. ****************/
	    /*gs_stdin_is_interactive = false; */
	    run_string(minst, ".runstdin", runFlush);
	    break;
	case '-':		/* run with command line args */
	case '+':
	    pal->expand_ats = false;
	case '@':		/* ditto with @-expansion */
	    {
		const char *psarg = arg_next(pal);

		if (psarg == 0) {
		    fprintf(stderr, "Usage: gs ... -%c file.ps arg1 ... argn\n", sw);
		    arg_finit(pal);
		    gs_exit(1);
		}
		psarg = arg_heap_copy(psarg);
		gs_main_init2(minst);
		run_string(minst, "userdict/ARGUMENTS[", 0);
		while ((arg = arg_next(pal)) != 0)
		    runarg(minst, "", arg_heap_copy(arg), "", runInit);
		runarg(minst, "]put", psarg, ".runfile", runInit | runFlush);
		gs_exit(0);
	    }
	case 'A':		/* trace allocator */
	    switch (*arg) {
		case 0:
		    gs_alloc_debug = 1;
		    break;
		case '-':
		    gs_alloc_debug = 0;
		    break;
		default:
		    puts("-A may only be followed by -");
		    gs_exit(1);
	    }
	    break;
	case 'B':		/* set run_string buffer size */
	    if (*arg == '-')
		minst->run_buffer_size = 0;
	    else {
		uint bsize;

		if (sscanf((const char *)arg, "%u", &bsize) != 1 ||
		    bsize <= 0 || bsize > MAX_BUFFERED_SIZE
		    ) {
		    fprintf(stderr, "-B must be followed by - or size between 1 and %u\n", MAX_BUFFERED_SIZE);
		    gs_exit(1);
		}
		minst->run_buffer_size = bsize;
	    }
	    break;
	case 'c':		/* code follows */
	    {
		bool ats = pal->expand_ats;

		gs_main_init2(minst);
		pal->expand_ats = false;
		while ((arg = arg_next(pal)) != 0) {
		    char *sarg;

		    if (arg[0] == '@' ||
			(arg[0] == '-' && !isdigit(arg[1]))
			)
			break;
		    sarg = arg_heap_copy(arg);
		    runarg(minst, "", sarg, ".runstring", 0);
		}
		if (arg != 0)
		    arg_push_string(pal, arg_heap_copy(arg));
		pal->expand_ats = ats;
		break;
	    }
	case 'E':		/* log errors */
	    switch (*arg) {
		case 0:
		    gs_log_errors = 1;
		    break;
		case '-':
		    gs_log_errors = 0;
		    break;
		default:
		    puts("-E may only be followed by -");
		    gs_exit(1);
	    }
	    break;
	case 'f':		/* run file of arbitrary name */
	    if (*arg != 0)
		argproc(minst, arg);
	    break;
	case 'F':		/* run file with buffer_size = 1 */
	    if (!*arg) {
		puts("-F requires a file name");
		gs_exit(1);
	    } {
		uint bsize = minst->run_buffer_size;

		minst->run_buffer_size = 1;
		argproc(minst, arg);
		minst->run_buffer_size = bsize;
	    }
	    break;
	case 'g':		/* define device geometry */
	    {
		long width, height;
		ref value;

		gs_main_init1(minst);
		if (sscanf((const char *)arg, "%ldx%ld", &width, &height) != 2) {
		    puts("-g must be followed by <width>x<height>");
		    gs_exit(1);
		}
		make_int(&value, width);
		initial_enter_name("DEVICEWIDTH", &value);
		make_int(&value, height);
		initial_enter_name("DEVICEHEIGHT", &value);
		initial_enter_name("FIXEDMEDIA", &vtrue);
		break;
	    }
	case 'h':		/* print help */
	case '?':		/* ditto */
	    print_help(minst);
	    gs_exit(gs_exit_INFO);
	case 'I':		/* specify search path */
	    gs_main_add_lib_path(minst, arg_heap_copy(arg));
	    break;
	case 'K':		/* set malloc limit */
	    {
		long msize = 0;

		sscanf((const char *)arg, "%ld", &msize);
		if (msize <= 0 || msize > max_long >> 10) {
		    fprintf(stderr, "-K<numK> must have 1 <= numK <= %ld\n",
			    max_long >> 10);
		    gs_exit(1);
		}
		gs_malloc_limit = msize << 10;
	    }
	    break;
	case 'M':		/* set memory allocation increment */
	    {
		unsigned msize = 0;

		sscanf((const char *)arg, "%u", &msize);
#if arch_ints_are_short
		if (msize <= 0 || msize >= 64) {
		    puts("-M must be between 1 and 63");
		    gs_exit(1);
		}
#endif
		minst->memory_chunk_size = msize << 10;
	    }
	    break;
	case 'N':		/* set size of name table */
	    {
		unsigned nsize = 0;

		sscanf((const char *)arg, "%d", &nsize);
#if arch_ints_are_short
		if (nsize < 2 || nsize > 64) {
		    puts("-N must be between 2 and 64");
		    gs_exit(1);
		}
#endif
		minst->name_table_size = (ulong) nsize << 10;
	    }
	    break;
	case 'P':		/* choose whether search '.' first */
	    if (!strcmp(arg, ""))
		minst->search_here_first = true;
	    else if (!strcmp(arg, "-"))
		minst->search_here_first = false;
	    else {
		puts("Only -P or -P- is allowed.");
		gs_exit(1);
	    }
	    break;
	case 'q':		/* quiet startup */
	    gs_main_init1(minst);
	    initial_enter_name("QUIET", &vtrue);
	    break;
	case 'r':		/* define device resolution */
	    {
		float xres, yres;
		ref value;

		gs_main_init1(minst);
		switch (sscanf((const char *)arg, "%fx%f", &xres, &yres)) {
		    default:
			puts("-r must be followed by <res> or <xres>x<yres>");
			gs_exit(1);
		    case 1:	/* -r<res> */
			yres = xres;
		    case 2:	/* -r<xres>x<yres> */
			make_real(&value, xres);
			initial_enter_name("DEVICEXRESOLUTION", &value);
			make_real(&value, yres);
			initial_enter_name("DEVICEYRESOLUTION", &value);
			initial_enter_name("FIXEDRESOLUTION", &vtrue);
		}
		break;
	    }
	case 'D':		/* define name */
	case 'd':
	case 'S':		/* define name as string */
	case 's':
	    {
		char *adef = arg_heap_copy(arg);
		char *eqp = strchr(adef, '=');
		bool isd = (sw == 'D' || sw == 'd');
		ref value;

		if (eqp == NULL)
		    eqp = strchr(adef, '#');
		/* Initialize the object memory, scanner, and */
		/* name table now if needed. */
		gs_main_init1(minst);
		if (eqp == adef) {
		    puts("Usage: -dname, -dname=token, -sname=string");
		    gs_exit(1);
		}
		if (eqp == NULL) {
		    if (isd)
			make_true(&value);
		    else
			make_empty_string(&value, a_readonly);
		} else {
		    int code;
		    uint space = icurrent_space;

		    *eqp++ = 0;
		    ialloc_set_space(idmemory, avm_system);
		    if (isd) {
			stream astream;
			scanner_state state;

			sread_string(&astream,
				     (const byte *)eqp, strlen(eqp));
			scanner_state_init(&state, false);
			code = scan_token(&astream, &value, &state);
			if (code) {
			    puts("-dname= must be followed by a valid token");
			    gs_exit(1);
			}
			if (r_has_type_attrs(&value, t_name,
					     a_executable)) {
			    ref nsref;

			    name_string_ref(&value, &nsref);
#define string_is(nsref, str, len)\
  (r_size(&(nsref)) == (len) &&\
   !strncmp((const char *)(nsref).value.const_bytes, str, (len)))
			    if (string_is(nsref, "null", 4))
				make_null(&value);
			    else if (string_is(nsref, "true", 4))
				make_true(&value);
			    else if (string_is(nsref, "false", 5))
				make_false(&value);
			    else {
				puts("-dvar=name requires name=null, true, or false");
				gs_exit(1);
			    }
#undef name_is_string
			}
		    } else {
			int len = strlen(eqp);
			char *str =
			(char *)gs_alloc_bytes(minst->heap,
					       (uint) len, "-s");

			if (str == 0) {
			    lprintf("Out of memory!\n");
			    gs_exit(1);
			}
			memcpy(str, eqp, len);
			make_const_string(&value,
					  a_readonly | avm_foreign,
					  len, (const byte *)str);
		    }
		    ialloc_set_space(idmemory, space);
		}
		/* Enter the name in systemdict. */
		initial_enter_name(adef, &value);
		break;
	    }
	case 'u':		/* undefine name */
	    if (!*arg) {
		puts("-u requires a name to undefine.");
		gs_exit(1);
	    }
	    gs_main_init1(minst);
	    initial_remove_name(arg);
	    break;
	case 'v':		/* print revision */
	    print_revision();
	    gs_exit(0);
/*#ifdef DEBUG */
	    /*
	     * Here we provide a place for inserting debugging code that can be
	     * run in place of the normal interpreter code.
	     */
	case 'X':
	    gs_main_init2(minst);
	    {
		int xec;	/* exit_code */
		ref xeo;	/* error_object */

#define start_x()\
  gs_main_run_string_begin(minst, 1, &xec, &xeo)
#define run_x(str)\
  gs_main_run_string_continue(minst, str, strlen(str), 1, &xec, &xeo)
#define stop_x()\
  gs_main_run_string_end(minst, 1, &xec, &xeo)
		start_x();
		run_x("\216\003abc");
		run_x("== flush\n");
		stop_x();
	    }
	    gs_exit(0);
/*#endif */
	case 'Z':
	    {
		byte value = (*arg == '-' ? (++arg, 0) : 0xff);

		while (*arg)
		    gs_debug[*arg++ & 127] = value;
	    }
	    break;
    }
    return 0;
}

/* Define versions of strlen and strcat that encode strings in hex. */
/* This is so we can enter escaped characters regardless of whether */
/* the Level 1 convention of ignoring \s in strings-within-strings */
/* is being observed (sigh). */
private int
esc_strlen(const char *str)
{
    return strlen(str) * 2 + 2;
}
private void
esc_strcat(char *dest, const char *src)
{
    char *d = dest + strlen(dest);
    const char *p;
    static const char *const hex = "0123456789abcdef";

    *d++ = '<';
    for (p = src; *p; p++) {
	byte c = (byte) * p;

	*d++ = hex[c >> 4];
	*d++ = hex[c & 0xf];
    }
    *d++ = '>';
    *d = 0;
}

/* Process file names */
private void
argproc(gs_main_instance * minst, const char *arg)
{
    if (minst->run_buffer_size) {
	/* Run file with run_string. */
	run_buffered(minst, arg);
    } else {
	/* Run file directly in the normal way. */
	runarg(minst, "", arg, ".runfile", runInit | runFlush);
    }
}
private void
run_buffered(gs_main_instance * minst, const char *arg)
{
    FILE *in = gp_fopen(arg, gp_fmode_rb);
    int exit_code;
    ref error_object;
    int code;

    if (in == 0) {
	fprintf(stderr, "Unable to open %s for reading", arg);
	gs_exit(1);
    }
    gs_main_init2(minst);
    code = gs_main_run_string_begin(minst, minst->user_errors,
				    &exit_code, &error_object);
    if (!code) {
	char buf[MAX_BUFFERED_SIZE];
	int count;

	code = e_NeedInput;
	while ((count = fread(buf, 1, minst->run_buffer_size, in)) > 0) {
	    code = gs_main_run_string_continue(minst, buf, count,
					       minst->user_errors,
					       &exit_code, &error_object);
	    if (code != e_NeedInput)
		break;
	}
	if (code == e_NeedInput) {
	    code = gs_main_run_string_end(minst, minst->user_errors,
					  &exit_code, &error_object);
	}
    }
    fclose(in);
    zflush(osp);
    zflushpage(osp);
    run_finish(code, exit_code, &error_object);
}
private void
runarg(gs_main_instance * minst, const char *pre, const char *arg,
       const char *post, int options)
{
    int len = strlen(pre) + esc_strlen(arg) + strlen(post) + 1;
    char *line;

    if (options & runInit)
	gs_main_init2(minst);	/* Finish initialization */
    line = (char *)gs_alloc_bytes(minst->heap, len, "argproc");
    if (line == 0) {
	lprintf("Out of memory!\n");
	gs_exit(1);
    }
    strcpy(line, pre);
    esc_strcat(line, arg);
    strcat(line, post);
    run_string(minst, line, options);
}
private void
run_string(gs_main_instance * minst, const char *str, int options)
{
    int exit_code;
    ref error_object;
    int code = gs_main_run_string(minst, str, minst->user_errors,
				  &exit_code, &error_object);

    if ((options & runFlush) || code != 0) {
	zflush(osp);		/* flush stdout */
	zflushpage(osp);	/* force display update */
    }
    run_finish(code, exit_code, &error_object);
}
private void
run_finish(int code, int exit_code, ref * perror_object)
{
    switch (code) {
	case 0:
	    break;
	case e_Quit:
	    gs_exit(0);
	case e_Fatal:
	    eprintf1("Unrecoverable error, exit code %d\n", exit_code);
	    gs_exit(exit_code);
	default:
	    gs_debug_dump_stack(code, perror_object);
	    gs_exit_with_code(255, code);
    }
}

/* ---------------- Print information ---------------- */

/*
 * Help strings.  We have to break them up into parts, because
 * the Watcom compiler has a limit of 510 characters for a single token.
 * For PC displays, we want to limit the strings to 24 lines.
 */
private const char help_usage1[] = "\
Usage: gs [switches] [file1.ps file2.ps ...]\n\
Most frequently used switches: (you can use # in place of =)\n\
 -dNOPAUSE           no pause after page   | -q       `quiet', fewer messages\n\
 -g<width>x<height>  page size in pixels   | -r<res>  pixels/inch resolution\n";
private const char help_usage2[] = "\
 -sDEVICE=<devname>  select device         | -dBATCH  exit after last file\n\
 -sOutputFile=<file> select output file: - for stdout, |command for pipe,\n\
                                         embed %d or %ld for page #\n";
private const char help_trailer[] = "\
For more information, see %s%sUse.htm.\n\
Report bugs to %s, using the form in Bug-form.htm.\n";
private const char help_devices[] = "Available devices:";
private const char help_emulators[] = "Input formats:";
private const char help_paths[] = "Search path:";

/* Print the standard help message. */
private void
print_help(gs_main_instance * minst)
{
    print_revision();
    print_usage();
    print_emulators();
    print_devices();
    print_paths(minst);
    print_help_trailer();
}

/* Print the revision, revision date, and copyright. */
private void
print_revision(void)
{
    fprintf(stderr, "%s ", gs_product);
    print_version();
    fprintf(stderr, " (%d-%d-%d)\n%s\n",
	    (int)(gs_revisiondate / 10000),
	    (int)(gs_revisiondate / 100 % 100),
	    (int)(gs_revisiondate % 100),
	    gs_copyright);
}

/* Print the version number. */
private void
print_version(void)
{
    fprintf(stderr, "%d.%02d",
	    (int)(gs_revision / 100),
	    (int)(gs_revision % 100));
}

/* Print usage information. */
private void
print_usage(void)
{
    fprintf(stderr, "%s", help_usage1);
    fprintf(stderr, "%s", help_usage2);
}

/* Print the list of available devices. */
private void
print_devices(void)
{
    fprintf(stderr, "%s", help_devices);
    {
	int i;
	int pos = 100;
	const gx_device *pdev;

	for (i = 0; (pdev = gs_getdevice(i)) != 0; i++) {
	    const char *dname = gs_devicename(pdev);
	    int len = strlen(dname);

	    if (pos + 1 + len > 76)
		fprintf(stderr, "\n  "), pos = 2;
	    fprintf(stderr, " %s", dname);
	    pos += 1 + len;
	}
    }
    fprintf(stderr, "\n");
}

/* Print the list of language emulators. */
private void
print_emulators(void)
{
    fprintf(stderr, "%s", help_emulators);
    {
	const ref *pes;

	for (pes = gs_emulator_name_array;
	     pes->value.const_bytes != 0; pes++
	    )
	    fprintf(stderr, " %s", pes->value.const_bytes);
    }
    fprintf(stderr, "\n");
}

/* Print the search paths. */
private void
print_paths(gs_main_instance * minst)
{
    fprintf(stderr, "%s", help_paths);
    gs_main_set_lib_paths(minst);
    {
	uint count = r_size(&minst->lib_path.list);
	uint i;
	int pos = 100;
	char fsepr[3];

	fsepr[0] = ' ', fsepr[1] = gp_file_name_list_separator,
	    fsepr[2] = 0;
	for (i = 0; i < count; ++i) {
	    const ref *prdir =
	    minst->lib_path.list.value.refs + i;
	    uint len = r_size(prdir);
	    const char *sepr = (i == count - 1 ? "" : fsepr);

	    if (1 + pos + strlen(sepr) + len > 76)
		fprintf(stderr, "\n  "), pos = 2;
	    fprintf(stderr, " ");
	    /*
	     * This is really ugly, but it's necessary.
	     * We wish we could just do:
	     fwrite(prdir->value.bytes, 1, len, stdout);
	     */
	    {
		const char *p = (const char *)prdir->value.bytes;
		uint j;

		for (j = len; j; j--)
		    fprintf(stderr, "%c", *p++);
	    }
	    fprintf(stderr, sepr);
	    pos += 1 + len + strlen(sepr);
	}
    }
    fprintf(stderr, "\n");
}

/* Print the help trailer. */
private void
print_help_trailer(void)
{
    fprintf(stderr, help_trailer, gs_doc_directory,
	    gp_file_name_concat_string(gs_doc_directory,
				       strlen(gs_doc_directory),
				       "Use.htm", 7),
	    GS_BUG_MAILBOX);
}
