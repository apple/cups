/* Copyright (C) 1991, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gp_iwatc.c */
/* Intel processor, Watcom C-specific routines for Ghostscript */
#include "dos_.h"
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include "stat_.h"
#include "string_.h"
#include "gx.h"
#include "gp.h"

/* Library routines not declared in a standard header */
extern char *getenv(P1(const char *));

/* Define a substitute for stdprn (see below). */
private FILE *gs_stdprn;

/* Forward declarations */
private void handle_FPE(P1(int));

/* Do platform-dependent initialization. */
void
gp_init(void)
{	gs_stdprn = 0;
	/* Set up the handler for numeric exceptions. */
	signal(SIGFPE, handle_FPE);
	gp_init_console();
}

/* Trap numeric exceptions.  Someday we will do something */
/* more appropriate with these. */
private void
handle_FPE(int sig)
{	eprintf("Numeric exception:\n");
	exit(1);
}

/* Do platform-dependent cleanup. */
void
gp_exit(int exit_status, int code)
{
}

/* Exit the program. */
void
gp_do_exit(int exit_status)
{	exit(exit_status);
}

/* ------ Printer accessing ------ */

/* Open a connection to a printer.  A null file name means use the */
/* standard printer connected to the machine, if any. */
/* Return NULL if the connection could not be opened. */
extern void gp_set_printer_binary(P2(int, int));
FILE *
gp_open_printer(char *fname, int binary_mode)
{	FILE *pfile;
	if ( strlen(fname) == 0 || !strcmp(fname, "PRN") )
	{	if ( !binary_mode )
			return stdprn;
		if ( gs_stdprn == 0 )
		{	/* We have to effectively reopen the printer, */
			/* because the Watcom library does \n -> \r\n */
			/* substitution on the stdprn stream. */
			int fno = dup(fileno(stdprn));
			setmode(fno, O_BINARY);
			gs_stdprn = fdopen(fno, "wb");
		}
		pfile = gs_stdprn;
	}
	else
	{	pfile = fopen(fname, (binary_mode ? "wb" : "w"));
		if ( pfile == NULL )
			return NULL;
	}
	gp_set_printer_binary(fileno(pfile), binary_mode);
	return pfile;
}

/* Close the connection to the printer. */
void
gp_close_printer(FILE *pfile, const char *fname)
{	if ( pfile != stdprn )
		fclose(pfile);
	if ( pfile == gs_stdprn )
		gs_stdprn = 0;
}

/* ------ File naming and accessing ------ */

/* Create and open a scratch file with a given name prefix. */
/* Write the actual file name at fname. */
FILE *
gp_open_scratch_file(const char *prefix, char *fname, const char *mode)
{	/* Unfortunately, Watcom C doesn't provide mktemp, */
	/* so we have to simulate it ourselves. */
	char *temp;
	struct stat fst;
	char *end;
	if ( (temp = getenv("TEMP")) == NULL )
		*fname = 0;
	else
	{	char last = '\\';
		strcpy(fname, temp);
		/* Prevent X's in path from being converted by mktemp. */
		for ( temp = fname; *temp; temp++ )
			*temp = last = tolower(*temp);
		switch ( last )
		{
		default:
			strcat(fname, "\\");
		case ':': case '\\':
			;
		}
	}
	strcat(fname, prefix);
	strcat(fname, "AA.AAA");
	end = fname + strlen(fname) - 1;
	while ( stat(fname, &fst) == 0 )
	   {	char *inc = end;
		while ( *inc == 'Z' || *inc == '.' )
		   {	if ( *inc == 'Z' ) *inc = 'A';
			inc--;
			if ( end - inc == 6 ) return 0;
		   }
		++*inc;
	   }
	return fopen(fname, mode);
}

/* Open a file with the given name, as a stream of uninterpreted bytes. */
FILE *
gp_fopen(const char *fname, const char *mode)
{	return fopen(fname, mode);
}
