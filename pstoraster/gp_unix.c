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

/* gp_unix.c */
/* Unix-specific routines for Ghostscript */
#include "string_.h"
#include "gx.h"
#include "gsexit.h"
#include "gp.h"
#include "time_.h"

#if defined(SYSV) || defined(SVR4) || defined(CLK_TCK)
#include <sys/times.h>
#endif

/*
 * Because of inconsistent (and sometimes incorrect) header files,
 * we must omit the argument list for popen.
 */
extern FILE *popen( /* P2(const char *, const char *) */ );
extern int pclose(P1(FILE *));
/*
 * This is the only place in Ghostscript that calls 'exit'.  Including
 * <stdlib.h> is overkill, but that's where it's declared on ANSI systems.
 * We don't have any way of detecting whether we have a standard library
 * (some GNU compilers perversely define __STDC__ but don't provide
 * an ANSI-compliant library), so we check __PROTOTYPES__ and
 * hope for the best.  We pick up getenv at the same time.
 */
#ifdef __PROTOTYPES__
#  include <stdlib.h>		/* for exit and getenv */
#else
extern void exit(P1(int));
extern char *getenv(P1(const char *));
#endif

/* Do platform-dependent initialization. */
void
gp_init(void)
{
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

/* ------ Miscellaneous ------ */

/* Get the string corresponding to an OS error number. */
/* Unix systems support this so inconsistently that we don't attempt */
/* to figure out whether it's available. */
const char *
gp_strerror(int errnum)
{	return NULL;
}

/* ------ Date and time ------ */

/* Read the current time (in seconds since Jan. 1, 1970) */
/* and fraction (in nanoseconds). */
void
gp_get_realtime(long *pdt)
{	struct timeval tp;

#if gettimeofday_no_timezone			/* older versions of SVR4 */
	{	if ( gettimeofday(&tp) == -1 )
		  {	lprintf("Ghostscript: gettimeofday failed!\n");
			gs_exit(1);
		  }
	}
#else						/* All other systems */
	{	struct timezone tzp;
		if ( gettimeofday(&tp, &tzp) == -1 )
		  {	lprintf("Ghostscript: gettimeofday failed!\n");
			gs_exit(1);
		  }
	}
#endif

	/* tp.tv_sec is #secs since Jan 1, 1970 */
	pdt[0] = tp.tv_sec;

	/* Some Unix systems (e.g., Interactive 3.2 r3.0) return garbage */
	/* in tp.tv_usec.  Try to filter out the worst of it here. */
	pdt[1] = tp.tv_usec >= 0 && tp.tv_usec < 1000000 ? tp.tv_usec*1000 : 0;

#ifdef DEBUG_CLOCK
	printf("tp.tv_sec = %d  tp.tv_usec = %d  pdt[0] = %ld  pdt[1] = %ld\n",
		tp.tv_sec, tp.tv_usec, pdt[0], pdt[1]);
#endif
}

/* Read the current user CPU time (in seconds) */
/* and fraction (in nanoseconds).  */
void
gp_get_usertime(long *pdt)
{
#if use_times_for_usertime
	struct tms tms;
	long ticks;

	static long ticks_per_sec;
	if ( !ticks_per_sec )		/* not initialized yet */
	  ticks_per_sec = CLK_TCK;

	times(&tms);
	ticks = tms.tms_utime + tms.tms_stime + tms.tms_cutime + tms.tms_cstime;
	pdt[0] = ticks / ticks_per_sec;
	pdt[1] = (ticks % ticks_per_sec) * (1000000000 / ticks_per_sec);
#else
	gp_get_realtime(pdt);	/* Use an approximation on other hosts.  */
#endif
}

/* ------ Screen management ------ */

/* Get the environment variable that specifies the display to use. */
const char *
gp_getenv_display(void)
{	return getenv("DISPLAY");
}

/* ------ Printer accessing ------ */

/* Open a connection to a printer.  A null file name means use the */
/* standard printer connected to the machine, if any. */
/* "|command" opens an output pipe. */
/* Return NULL if the connection could not be opened. */
FILE *
gp_open_printer(char *fname, int binary_mode)
{	return
	  (strlen(fname) == 0 ?
	   gp_open_scratch_file(gp_scratch_file_name_prefix, fname, "w") :
	   fname[0] == '|' ?
	   popen(fname + 1, "w") :
	   fopen(fname, "w"));
}

/* Close the connection to the printer. */
void
gp_close_printer(FILE *pfile, const char *fname)
{	if ( fname[0] == '|' )
		pclose(pfile);
	else
		fclose(pfile);
}
