/*
 * "$Id: pstoraster.c,v 1.13 2000/03/13 18:30:14 mike Exp $"
 *
 *   PostScript RIP filter main entry for the Common UNIX Printing System
 *   (CUPS).
 *
 *   Copyright 1993-2000 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This code and any derivative of it may be used and distributed
 *   freely under the terms of the GNU General Public License when
 *   used with GNU Ghostscript or its derivatives.  Use of the code
 *   (or any derivative of it) with software other than GNU
 *   GhostScript (or its derivatives) is governed by the CUPS license
 *   agreement.
 *
 * Contents:
 *
 *   main()          - Main entry for pstoraster.
 *   define_string() - Define a string value...
 */

/*
 * Include necessary headers...
 */

#define bool bool_              /* (maybe not needed) */
#define uchar uchar_
#define uint uint_
#define ushort ushort_
#define ulong ulong_

#include <cups/cups.h>
#include <cups/string.h>
#include <stdlib.h>

#undef bool
#undef uchar
#undef uint
#undef ushort
#undef ulong

#include "ghost.h"
#include "imain.h"
#include "iminst.h"
#include "istack.h"
#include "interp.h"
#include "ostack.h"
#include "opextern.h"
#include "gscdefs.h"
#include "store.h"


/*
 * Globals...
 */

const char	*cupsProfile = NULL;


/*
 * Local functions...
 */

static void	define_string(char *, char *);


/*
 * 'main()' - Main entry for pstoraster.
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  FILE			*stdfiles[3];	/* Copies of stdin, stdout, and stderr */
  gs_main_instance	*minst;		/* Interpreter instance */
  ref			vtrue;		/* True value */
  int			exit_code;	/* Exit code */
  ref			error_object;	/* Error object */
  char			*content_type;	/* CONTENT_TYPE environment variable */
  int			num_options;	/* Number of job options */
  cups_option_t		*options;	/* Job options */


 /*
  * Force the locale to "C" to avoid bugs...
  */

  putenv("LANG=C");

 /*
  * Create a PostScript interpreter instance...
  */

  minst = gs_main_instance_default();

 /*
  * Grab the old stdin/stdout/stderr...
  */

  gs_get_real_stdio(stdfiles);

 /*
  * Grab any job options...
  */

  num_options = cupsParseOptions(argv[5], 0, &options);

  cupsProfile = cupsGetOption("profile", num_options, options);

 /*
  * Initialize basic interpreter stuff and read from the named file or
  * from stdin...
  */

  if (argc > 6)
    gs_main_init0(minst, fopen(argv[6], "r"), stdfiles[1], stdfiles[2], 8);
  else
    gs_main_init0(minst, stdfiles[0], stdfiles[1], stdfiles[2], 8);

 /*
  * Tell the interpreter where to find its files...
  */

  minst->lib_path.final = gs_lib_default_path;
  gs_main_set_lib_paths(minst);

 /*
  * Set interpreter options...
  */

  make_true(&vtrue);
  gs_main_init1(minst);
  initial_enter_name("QUIET", &vtrue);
  initial_enter_name("NOPAUSE", &vtrue);
  initial_enter_name("BATCH", &vtrue);

  if ((content_type = getenv("CONTENT_TYPE")) != NULL &&
      strcmp(content_type, "application/pdf") == 0)
  {
    fputs("INFO: Converting PDF file to PostScript...\n", stderr);
    define_string("DEVICE", "pswrite");
  }

  define_string("OutputFile", "-");
  define_string("FONTPATH", CUPS_DATADIR "/fonts");

 /*
  * Start the interpreter...
  */

  gs_main_init2(minst);
  gs_main_run_string(minst, ".runstdin", minst->user_errors, &exit_code,
                     &error_object);

 /*
  * Make sure that the last page was printed...
  */

  zflush(osp);
  zflushpage(osp);

 /*
  * And the exit when we're done...
  */

  gs_exit(exit_code);

  return (0);
}


/*
 * 'define_string()' - Define a string value...
 */

static void
define_string(char *name,	/* I - Variable to set */
              char *s)		/* I - Value */
{
  int	len;			/* Length of string */
  char	*copy;			/* Copy of string */
  ref	value;			/* Value object */


 /*
  * Get the string value and copy it using strdup().  Note that this uses
  * the malloc() function, but since we are only running gsrip on "real"
  * operating systems (no Windows/PC build), this is not a problem...
  */

  if (s == NULL)
  {
    len  = 0;
    copy = strdup("");
  }
  else
  {
    len  = strlen(s);
    copy = strdup(s);
  };
  
 /*
  * Enter the name in systemdict...
  */

  make_const_string(&value, a_readonly | avm_foreign, len, (const byte *)copy);
  initial_enter_name(name, &value);
}


/*
 * End of "$Id: pstoraster.c,v 1.13 2000/03/13 18:30:14 mike Exp $".
 */
