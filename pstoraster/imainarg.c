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

/* imainarg.c */
/* Command line parsing and dispatching */
/* Define PROGRAM_NAME before we include std.h */
#define PROGRAM_NAME gs_product
#include "ctype_.h"
#include "memory_.h"
#include "string_.h"
#include "ghost.h"
#include "gp.h"
#include "gscdefs.h"
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
#include "files.h"				/* requires stream.h */
#include "interp.h"
#include "iutil.h"
#include "ivmspace.h"

/* Import operator procedures */
extern int zflush (P1(os_ptr));
extern int zflushpage (P1(os_ptr));

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
#  define GS_BUG_MAILBOX "print@easysw.com"
#endif

/* Library routines not declared in a standard header */
extern char *getenv(P1(const char *));
/* Note: sscanf incorrectly defines its first argument as char * */
/* rather than const char *.  This accounts for the ugly casts below. */

/* Redefine puts to use fprintf, so it will work even without stdio. */
#undef puts
private void near
fpputs(const char *str)
{	fprintf(stdout, "%s\n", str);
}
#define puts(str) fpputs(str)

/* Other imported data */
extern const char *gs_doc_directory;
extern const char *gs_lib_default_path;
extern ref gs_emulator_name_array[];

/* Forward references */
typedef struct arg_list_s arg_list;
private int swproc(P3(gs_main_instance *, const char *, arg_list *));
private void argproc(P2(gs_main_instance *, const char *));
private int esc_strlen(P1(const char *));
private void esc_strcat(P2(char *, const char *));
private void runarg(P6(gs_main_instance *, const char *, const char *, const char *, bool, bool));
private void run_string(P3(gs_main_instance *, const char *, bool));
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

/* ------ Argument management ------ */

/* We need to handle recursion into @-files. */
/* The following structures keep track of the state. */
typedef struct arg_source_s {
	int is_file;
	union _u {
		const char *str;
		FILE *file;
	} u;
} arg_source;
struct arg_list_s {
	bool expand_ats;	/* if true, expand @-files */
	const char **argp;
	int argn;
	int depth;		/* depth of @-files */
#define cstr_max 128
	char cstr[cstr_max + 1];
#define csource_max 10
	arg_source sources[csource_max];
};

/* Initialize an arg list. */
private void
arg_init(arg_list *pal, const char **argv, int argc)
{	pal->expand_ats = true;
	pal->argp = argv + 1;
	pal->argn = argc - 1;
	pal->depth = 0;
}

/* Push a string onto an arg list. */
/* (Only used at top level, so no need to check depth.) */
private void
arg_push_string(arg_list *pal, const char *str)
{	arg_source *pas = &pal->sources[pal->depth];
	pas->is_file = 0;
	pas->u.str = str;
	pal->depth++;
}

/* Clean up an arg list. */
private void
arg_finit(arg_list *pal)
{	while ( pal->depth )
	  if ( pal->sources[--(pal->depth)].is_file )
		fclose(pal->sources[pal->depth].u.file);
}

/* Get the next arg from a list. */
/* Note that these are not copied to the heap. */
private const char *
arg_next(gs_main_instance *minst, arg_list *pal)
{	arg_source *pas;
	FILE *f;
	const char *astr = 0;		/* initialized only to pacify gcc */
	char *cstr;
	const char *result;
	int endc;
	register int c;
	register int i;
	bool in_quote;

top:	pas = &pal->sources[pal->depth - 1];
	if ( pal->depth == 0 )
	{	if ( pal->argn <= 0 )		/* all done */
			return 0;
		pal->argn--;
		result = *(pal->argp++);
		goto at;
	}
	if ( pas->is_file )
		f = pas->u.file, endc = EOF;
	else
		astr = pas->u.str, f = NULL, endc = 0;
	result = cstr = pal->cstr;
#define cfsgetc() (f == NULL ? (*astr ? *astr++ : 0) : fgetc(f))
	while ( isspace(c = cfsgetc()) ) ;
	if ( c == endc )
	{	if ( f != NULL )
			fclose(f);
		pal->depth--;
		goto top;
	}
	in_quote = false;
	for ( i = 0; ; )
	{	if ( i == cstr_max - 1 )
		{	cstr[i] = 0;
			fprintf(stdout, "Command too long: %s\n", cstr);
			gs_exit(1);
		}
		/* If input is coming from an @-file, allow quotes */
		/* to protect whitespace. */
		if ( c == '"' && f != NULL )
		  in_quote = !in_quote;
		else
		  cstr[i++] = c;
		c = cfsgetc();
		if ( c == endc )
		  {	if ( in_quote )
			  { cstr[i] = 0;
			    fprintf(stdout,
				    "Unterminated quote in @-file: %s\n",
				    cstr);
			    gs_exit(1);
			  }
			break;
		  }
		if ( isspace(c) && !in_quote )
			break;
	}
	cstr[i] = 0;
	if ( f == NULL )
		pas->u.str = astr;
at:	if ( pal->expand_ats && result[0] == '@' )
	{	if ( pal->depth == csource_max )
		{	lprintf("Too much nesting of @-files.\n");
			gs_exit(1);
		}
		gs_main_set_lib_paths(minst);
		result++;		/* skip @ */
		f = lib_fopen(result);
		if ( f == NULL )
		{	fprintf(stdout, "Unable to open command line file %s\n", result);
			gs_exit(1);
		}
		pal->depth++;
		pas++;
		pas->is_file = 1;
		pas->u.file = f;
		goto top;
	}
	return result;
}

/* Copy an argument string to the heap. */
private char *
arg_copy(const char *str)
{	char *sstr = gs_malloc(strlen(str) + 1, 1, "arg_copy");
	if ( sstr == 0 )
	{	lprintf("Out of memory!\n");
		gs_exit(1);
	}
	strcpy(sstr, str);
	return sstr;
}

/* ------ Main program ------ */

/* Process the command line with a given instance. */
int
gs_main_init_with_args(gs_main_instance *minst, int argc, char *argv[])
{	const char *arg;
	arg_list args;
	FILE *stdfiles[3];

	gs_get_real_stdio(stdfiles);
	arg_init(&args, (const char **)argv, argc);
	gs_main_init0(minst, stdfiles[0], stdfiles[1], stdfiles[2],
		      GS_MAX_LIB_DIRS);
	   {	char *lib = getenv(GS_LIB);
		if ( lib != 0 ) 
		   {	int len = strlen(lib);
			char *path = gs_malloc(len + 1, 1, "GS_LIB");
			strcpy(path, lib);
			minst->lib_path.env = path;
		   }
	   }
	minst->lib_path.final = gs_lib_default_path;
	gs_main_set_lib_paths(minst);
	/* Prescan the command line for --help and --version. */
	{ int i;
	  bool helping = false;
	  for ( i = 1; i < argc; ++i )
	    if ( !strcmp(argv[i], "--" ) )
	      { /* A PostScript program will be interpreting all the */
		/* remaining switches, so stop scanning. */
		helping = false;
		break;
	      }
	    else if ( !strcmp(argv[i], "--help") )
	      { print_help(minst);
		helping = true;
	      }
	    else if ( !strcmp(argv[i], "--version") )
	      { print_version();
		puts("");		/* \n */
		helping = true;
	      }
	  if ( helping )
	    gs_exit(1);
	}
	/* Execute files named in the command line, */
	/* processing options along the way. */
	/* Wait until the first file name (or the end */
	/* of the line) to finish initialization. */
	minst->run_start = true;
	{	const char *opts = getenv(GS_OPTIONS);
		if ( opts != 0 )
			arg_push_string(&args, opts);
	}
	while ( (arg = arg_next(minst, &args)) != 0 )
	   {	switch ( *arg )
		{
		case '-':
			if ( swproc(minst, arg, &args) < 0 )
			  fprintf(stdout,
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
gs_main_run_start(gs_main_instance *minst)
{	run_string(minst, "systemdict /start get exec", true);
}

/* Process switches */
private int
swproc(gs_main_instance *minst, const char *arg, arg_list *pal)
{	char sw = arg[1];
	ref vtrue;

	make_true(&vtrue);
	arg += 2;		/* skip - and letter */
	switch ( sw )
	   {
	default:
		return -1;
	case 0:				/* read stdin as a file */
		minst->run_start = false;	/* don't run 'start' */
		/* Set NOPAUSE so showpage won't try to read from stdin. */
		swproc(minst, "-dNOPAUSE", pal);
		gs_main_init2(minst);		/* Finish initialization */
		/* We delete this only to make Ghostview work properly. */
		/**************** This is WRONG. ****************/
		/*gs_stdin_is_interactive = false;*/
		run_string(minst, ".runstdin", true);
		break;
	case '-':			/* run with command line args */
	case '+':
		pal->expand_ats = false;
	case '@':			/* ditto with @-expansion */
	   {	const char *psarg = arg_next(minst, pal);
		if ( psarg == 0 )
		{	fprintf(stdout, "Usage: gs ... -%c file.ps arg1 ... argn\n", sw);
			arg_finit(pal);
			gs_exit(1);
		}
		psarg = arg_copy(psarg);
		gs_main_init2(minst);
		run_string(minst, "userdict/ARGUMENTS[", false);
		while ( (arg = arg_next(minst, pal)) != 0 )
		  runarg(minst, "", arg_copy(arg), "", true, false);
		runarg(minst, "]put", psarg, ".runfile", true, true);
		gs_exit(0);
	   }
	case 'A':			/* trace allocator */
		switch ( *arg )
		{
		case 0: gs_alloc_debug = 1; break;
		case '-': gs_alloc_debug = 0; break;
		default: puts("-A may only be followed by -"); gs_exit(1);
		}
		break;
	case 'c':			/* code follows */
	  {	bool ats = pal->expand_ats;
		gs_main_init2(minst);
		pal->expand_ats = false;
		while ( (arg = arg_next(minst, pal)) != 0 )
		  {	char *sarg;
			if ( arg[0] == '@' ||
			     (arg[0] == '-' && !isdigit(arg[1]))
			   )
			  break;
			sarg = arg_copy(arg);
			runarg(minst, "", sarg, ".runstring", false, false);
		  }
		if ( arg != 0 )
		  arg_push_string(pal, arg_copy(arg));
		pal->expand_ats = ats;
		break;
	  }
	case 'E':			/* log errors */
		switch ( *arg )
		{
		case 0: gs_log_errors = 1; break;
		case '-': gs_log_errors = 0; break;
		default: puts("-E may only be followed by -"); gs_exit(1);
		}
		break;
	case 'f':			/* run file of arbitrary name */
		if ( *arg != 0 )
		  argproc(minst, arg);
		break;
	case 'g':			/* define device geometry */
	   {	long width, height;
		ref value;
		gs_main_init1(minst);
		if ( sscanf((const char *)arg, "%ldx%ld", &width, &height) != 2 )
		   {	puts("-g must be followed by <width>x<height>");
			gs_exit(1);
		   }
		make_int(&value, width);
		initial_enter_name("DEVICEWIDTH", &value);
		make_int(&value, height);
		initial_enter_name("DEVICEHEIGHT", &value);
		initial_enter_name("FIXEDMEDIA", &vtrue);
		break;
	   }
	case 'h':			/* print help */
	case '?':			/* ditto */
		print_help(minst);
		gs_exit(1);
	case 'I':			/* specify search path */
		gs_main_add_lib_path(minst, arg_copy(arg));
		break;
	case 'M':			/* set memory allocation increment */
	   {	unsigned msize = 0;
		sscanf((const char *)arg, "%d", &msize);
#if arch_ints_are_short
		if ( msize <= 0 || msize >= 64 )
		   {	puts("-M must be between 1 and 63");
			gs_exit(1);
		   }
#endif
		minst->memory_chunk_size = msize << 10;
	   }
		break;
	case 'N':			/* set size of name table */
	   {	unsigned nsize = 0;
		sscanf((const char *)arg, "%d", &nsize);
#if arch_ints_are_short
		if ( nsize < 2 || nsize > 64 )
		   {	puts("-N must be between 2 and 64");
			gs_exit(1);
		   }
#endif
		minst->name_table_size = (ulong)nsize << 10;
	   }
		break;
	case 'P':			/* choose whether search '.' first */
		if ( !strcmp(arg, "") )
		  minst->search_here_first = true;
		else if ( !strcmp(arg, "-") )
		  minst->search_here_first = false;
		else
		  {	puts("Only -P or -P- is allowed.");
			gs_exit(1);
		  }
		break;
	case 'q':			/* quiet startup */
		gs_main_init1(minst);
		initial_enter_name("QUIET", &vtrue);
		break;
	case 'r':			/* define device resolution */
	   {	float xres, yres;
		ref value;
		gs_main_init1(minst);
		switch ( sscanf((const char *)arg, "%fx%f", &xres, &yres) )
		   {
		default:
			puts("-r must be followed by <res> or <xres>x<yres>");
			gs_exit(1);
		case 1:			/* -r<res> */
			yres = xres;
		case 2:			/* -r<xres>x<yres> */
			make_real(&value, xres);
			initial_enter_name("DEVICEXRESOLUTION", &value);
			make_real(&value, yres);
			initial_enter_name("DEVICEYRESOLUTION", &value);
			initial_enter_name("FIXEDRESOLUTION", &vtrue);
		   }
		break;
	   }
	case 'D':			/* define name */
	case 'd':
	case 'S':			/* define name as string */
	case 's':
	   {	char *adef = arg_copy(arg);
		char *eqp = strchr(adef, '=');
		bool isd = (sw == 'D' || sw == 'd');
		ref value;

		if ( eqp == NULL )
			eqp = strchr(adef, '#');
		/* Initialize the object memory, scanner, and */
		/* name table now if needed. */
		gs_main_init1(minst);
		if ( eqp == adef )
		   {	puts("Usage: -dname, -dname=token, -sname=string");
			gs_exit(1);
		   }
		if ( eqp == NULL )
		   {	if ( isd )
				make_true(&value);
			else
				make_empty_string(&value, a_readonly);
		   }
		else
		   {	int code;
			uint space = icurrent_space;

			*eqp++ = 0;
			ialloc_set_space(idmemory, avm_system);
			if ( isd )
			   {	stream astream;
				scanner_state state;

				sread_string(&astream,
					     (const byte *)eqp, strlen(eqp));
				scanner_state_init(&state, false);
				code = scan_token(&astream, &value, &state);
				if ( code )
				   {	puts("-dname= must be followed by a valid token");
					gs_exit(1);
				   }
				if ( r_has_type_attrs(&value, t_name,
						      a_executable) )
				  { ref nsref;
				    name_string_ref(&value, &nsref);
#define string_is(nsref, str, len)\
  (r_size(&(nsref)) == (len) &&\
   !strncmp((const char *)(nsref).value.const_bytes, str, (len)))
				    if ( string_is(nsref, "null", 4) )
				      make_null(&value);
				    else if ( string_is(nsref, "true", 4) )
				      make_true(&value);
				    else if ( string_is(nsref, "false", 5) )
				      make_false(&value);
				    else
				      { puts("-dvar=name requires name=null, true, or false");
				        gs_exit(1);
				      }
#undef name_is_string
				  }
			   }
			else
			   {	int len = strlen(eqp);
				char *str = gs_malloc((uint)len, 1, "-s");

				if ( str == 0 )
				   {	lprintf("Out of memory!\n");
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
	case 'u':			/* undefine name */
		if ( !*arg )
		  { puts("-u requires a name to undefine.");
		    gs_exit(1);
		  }
		gs_main_init1(minst);
		initial_remove_name(arg);
		break;
	case 'v':			/* print revision */
		print_revision();
		gs_exit(0);
/*#ifdef DEBUG*/
	/*
	 * Here we provide a place for inserting debugging code that can be
	 * run in place of the normal interpreter code.
	 */
	case 'X':
		gs_main_init2(minst);
	  {	int xec;		/* exit_code */
		ref xeo;		/* error_object */
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
/*#endif*/
	case 'Z':
		{ byte value = (*arg == '-' ? (++arg, 0) : 0xff);
		  while ( *arg )
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
{	return strlen(str) * 2 + 2;
}
private void
esc_strcat(char *dest, const char *src)
{	char *d = dest + strlen(dest);
	const char *p;
	static const char *hex = "0123456789abcdef";
	*d++ = '<';
	for ( p = src; *p; p++ )
	{	byte c = (byte)*p;
		*d++ = hex[c >> 4];
		*d++ = hex[c & 0xf];
	}
	*d++ = '>';
	*d = 0;
}

/* Process file names */
private void
argproc(gs_main_instance *minst, const char *arg)
{	runarg(minst, "", arg, ".runfile", true, true);
}
private void
runarg(gs_main_instance *minst, const char *pre, const char *arg,
  const char *post, bool init, bool flush)
{	int len = strlen(pre) + esc_strlen(arg) + strlen(post) + 1;
	char *line;
	if ( init )
	  gs_main_init2(minst);	/* Finish initialization */
	line = gs_malloc(len, 1, "argproc");
	if ( line == 0 )
	{	lprintf("Out of memory!\n");
		gs_exit(1);
	}
	strcpy(line, pre);
	esc_strcat(line, arg);
	strcat(line, post);
	run_string(minst, line, flush);
}
private void
run_string(gs_main_instance *minst, const char *str, bool flush)
{	int exit_code;
	ref error_object;
	int code = gs_main_run_string(minst, str, minst->user_errors,
				      &exit_code, &error_object);
	if ( flush || code != 0 )
	{	zflush(osp);		/* flush stdout */
		zflushpage(osp);	/* force display update */
	}
	run_finish(code, exit_code, &error_object);
}
private void
run_finish(int code, int exit_code, ref *perror_object)
{	switch ( code )
	{
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
private const char far_data help_usage1[] = "\
Usage: gs [switches] [file1.ps file2.ps ...]\n\
Most frequently used switches: (you can use # in place of =)\n\
 -dNOPAUSE           no pause after page   | -q       `quiet', fewer messages\n\
 -g<width>x<height>  page size in pixels   | -r<res>  pixels/inch resolution\n";
private const char far_data help_usage2[] = "\
 -sDEVICE=<devname>  select device         | -c quit  (as the last switch)\n\
                                           |            exit after last file\n\
 -sOutputFile=<file> select output file: - for stdout, |command for pipe,\n\
                                         embed %d or %ld for page #\n";
private const char far_data help_trailer[] = "\
For more information, see %s%suse.txt.\n\
Report bugs to %s; use the form in new-user.txt.\n";
private const char far_data help_devices[] = "Available devices:";
private const char far_data help_emulators[] = "Input formats:";
private const char far_data help_paths[] = "Search path:";

/* Print the standard help message. */
private void
print_help(gs_main_instance *minst)
{	print_revision();
	print_usage();
	print_emulators();
	print_devices();
	print_paths(minst);
	print_help_trailer();
}

/* Print the revision, revision date, and copyright. */
private void
print_revision(void)
{	fprintf(stdout, "%s ", gs_product);
	print_version();
	fprintf(stdout, " (%d-%d-%d)\n%s\n",
		(int)(gs_revisiondate / 10000),
		(int)(gs_revisiondate / 100 % 100),
		(int)(gs_revisiondate % 100),
		gs_copyright);
}

/* Print the version number. */
private void
print_version(void)
{	fprintf(stdout, "%d.%02d",
		(int)(gs_revision / 100),
		(int)(gs_revision % 100));
}

/* Print usage information. */
private void
print_usage(void)
{	fprintf(stdout, "%s", help_usage1);
	fprintf(stdout, "%s", help_usage2);
}

/* Print the list of available devices. */
private void
print_devices(void)
{	fprintf(stdout, "%s", help_devices);
	{	int i;
		int pos = 100;
		const gx_device *pdev;

		for ( i = 0; (pdev = gs_getdevice(i)) != 0; i++ )
		  { const char *dname = gs_devicename(pdev);
		    int len = strlen(dname);

		    if ( pos + 1 + len > 76 )
		      fprintf(stdout, "\n  "), pos = 2;
		    fprintf(stdout, " %s", dname);
		    pos += 1 + len;
		  }
	}
	fprintf(stdout, "\n");
}

/* Print the list of language emulators. */
private void
print_emulators(void)
{	fprintf(stdout, "%s", help_emulators);
	{	const ref *pes;
		for ( pes = gs_emulator_name_array;
		      pes->value.const_bytes != 0; pes++
		    )
		  fprintf(stdout, " %s", pes->value.const_bytes);
	}
	fprintf(stdout, "\n");
}

/* Print the search paths. */
private void
print_paths(gs_main_instance *minst)
{	fprintf(stdout, "%s", help_paths);
	gs_main_set_lib_paths(minst);
	{	uint count = r_size(&minst->lib_path.list);
		uint i;
		int pos = 100;
		char fsepr[3];

		fsepr[0] = ' ', fsepr[1] = gp_file_name_list_separator,
		  fsepr[2] = 0;
		for ( i = 0; i < count; ++i )
		{	const ref *prdir =
			  minst->lib_path.list.value.refs + i;
			uint len = r_size(prdir);
			const char *sepr = (i == count - 1 ? "" : fsepr);

			if ( 1 + pos + strlen(sepr) + len > 76 )
			  fprintf(stdout, "\n  "), pos = 2;
			fprintf(stdout, " ");
			/*
			 * This is really ugly, but it's necessary.
			 * We wish we could just do:
				fwrite(prdir->value.bytes, 1, len, stdout);
			 */
			{ const char *p = (const char *)prdir->value.bytes;
			  uint j;
			  for ( j = len; j; j-- )
			    fprintf(stdout, "%c", *p++);
			}
			fprintf(stdout, sepr);
			pos += 1 + len + strlen(sepr);
		}
	}
	fprintf(stdout, "\n");
}

/* Print the help trailer. */
private void
print_help_trailer(void)
{	fprintf(stdout, help_trailer, gs_doc_directory,
		gp_file_name_concat_string(gs_doc_directory,
					   strlen(gs_doc_directory),
					   "use.txt", 7),
		GS_BUG_MAILBOX);
}
