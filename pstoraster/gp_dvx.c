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

/* gp_dvx.c */
/* Desqview/X-specific routines for Ghostscript */
#include "string_.h"
#include "gx.h"
#include "gsexit.h"
#include "gp.h"
#include "time_.h"

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
/* All reasonable compilers support it. */
const char *
gp_strerror(int errnum)
{	return strerror(errnum);
}

/* ------ Date and time ------ */

/* Read the current time (in seconds since Jan. 1, 1970) */
/* and fraction (in nanoseconds). */
void
gp_get_realtime(long *pdt)
{	struct timeval tp;
	struct timezone tzp;

	if ( gettimeofday(&tp, &tzp) == -1 )
	   {	lprintf("Ghostscript: gettimeofday failed!\n");
		gs_exit(1);
	   }

	/* tp.tv_sec is #secs since Jan 1, 1970 */
	pdt[0] = tp.tv_sec;
	pdt[1] = tp.tv_usec * 1000;

#ifdef DEBUG_CLOCK
	printf("tp.tv_sec = %d  tp.tv_usec = %d  pdt[0] = %ld  pdt[1] = %ld\n",
		tp.tv_sec, tp.tv_usec, pdt[0], pdt[1]);
#endif
}

/* Read the current user CPU time (in seconds) */
/* and fraction (in nanoseconds).  */
void
gp_get_usertime(long *pdt)
{	gp_get_realtime(pdt);	/* Use an approximation for now.  */
}

/* ------ Printer accessing ------ */

/* Open a connection to a printer.  A null file name means use the */
/* standard printer connected to the machine, if any. */
/* Return NULL if the connection could not be opened. */
extern void gp_set_printer_binary(P2(int, int));
FILE *
gp_open_printer(char *fname, int binary_mode)
{       if ( strlen(fname) == 0 || !strcmp(fname, "PRN") )
        {       if ( binary_mode )
		  gp_set_printer_binary(fileno(stdprn), 1);
		stdprn->_flag = _IOWRT;	/* Make stdprn buffered to improve performance */
                return stdprn;
        }
        else
                return fopen(fname, (binary_mode ? "wb" : "w"));
}

/* Close the connection to the printer. */
void
gp_close_printer(FILE *pfile, const char *fname)
{       if ( pfile == stdprn )
	  fflush(pfile);
	else
	  fclose(pfile);
}
