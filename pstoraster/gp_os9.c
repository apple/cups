/* Copyright (C) 1989, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gp_os9.c */
/* OSK-specific routines for Ghostscript */
#include "string_.h"
#include "gx.h"
#include "gp.h"
#include "time_.h"
#include <signal.h>
#include <stdlib.h>	/* for exit */
#include <sys/param.h>	/* for MAXPATHLEN */

/* popen isn't POSIX-standard, so we declare it here. */
extern FILE *popen();
extern int pclose();

int interrupted;

/* Forward declarations */
private void signalhandler(P1(int));
private FILE *rbfopen(P2(char*, char*));

/* Do platform-dependent initialization */
void
gp_init(void)
{	intercept(signalhandler);
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

private void
signalhandler(int sig)
{	clearerr(stdin);
	switch(sig) {
		case SIGINT:
		case SIGQUIT:
			interrupted = 1;
			break;
		case SIGFPE:
			interrupted = 2;
			break;
		default:
			break;
	}
}

/* ------ Date and time ------ */

/* Read the current time (in seconds since Jan. 1, 1980) */
/* and fraction (in nanoseconds). */
#define PS_YEAR_0 80
#define PS_MONTH_0 1
#define PS_DAY_0 1
void
gp_get_realtime(long *pdt)
{
	long date, time, pstime, psdate, tick;
	short day;

	_sysdate(0, &time, &date, &day, &tick);
	_julian(&time, &date);
        
	pstime = 0;
	psdate = (PS_YEAR_0 << 16) + (PS_MONTH_0 << 8) + PS_DAY_0;
	_julian(&pstime, &psdate);
        
	pdt[0] = (date - psdate)*86400 + time;
	pdt[1] = 0;
	
#ifdef DEBUG_CLOCK
	printf("pdt[0] = %ld  pdt[1] = %ld\n", pdt[0], pdt[1]);
#endif
}

/* Read the current user CPU time (in seconds) */
/* and fraction (in nanoseconds).  */
void
gp_get_usertime(long *pdt)
{	return gp_get_realtime(pdt);	/* not yet implemented */
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
	   rbfopen(fname, "w"));
}

FILE *
rbfopen(char *fname, char *perm)
{	FILE *file = fopen(fname, perm);
	file->_flag |= _RBF;
	return file;
}

/* Close the connection to the printer. */
void
gp_close_printer(FILE *pfile, const char *fname)
{	if ( fname[0] == '|' )
		pclose(pfile);
	else
		fclose(pfile);
}
