/* Copyright (C) 1989, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gs.c */
/* 'main' program for Ghostscript */
#include "ghost.h"
#include "imain.h"
#include "imainarg.h"
#include "iminst.h"

/* Define an optional array of strings for testing. */
/*#define RUN_STRINGS*/
#ifdef RUN_STRINGS
private const char *run_strings[] = {
  "2 vmreclaim /SAVE save def 2 vmreclaim",
  "(saved\n) print flush",
  "SAVE restore (restored\n) print flush 2 vmreclaim",
  "(done\n) print flush quit",
  0
};
#endif

int
main(int argc, char *argv[])
{	gs_main_instance *minst = gs_main_instance_default();
	gs_main_init_with_args(minst, argc, argv);

#ifdef RUN_STRINGS
	{	/* Run a list of strings (for testing). */
		const char **pstr = run_strings;
		for ( ; *pstr; ++pstr )
		  {	int exit_code;
			ref error_object;
			int code;
			fprintf(stdout, "{%s} =>\n", *pstr);
			fflush(stdout);
			code = gs_main_run_string(minst, *pstr, 0,
						  &exit_code, &error_object);
			zflush(osp);
			fprintf(stdout, " => code = %d\n", code);
			fflush(stdout);
			if ( code < 0 )
			  gs_exit(1);
		  }
	}
#endif

	if ( minst->run_start )
	  gs_main_run_start(minst);

	gs_exit(0);			/* exit */
	/* NOTREACHED */
}
