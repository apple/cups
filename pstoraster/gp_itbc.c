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

/* gp_itbc.c */
/* Intel processor, Turbo/Borland C-specific routines for Ghostscript */
#include "dos_.h"
#include <fcntl.h>
#include <io.h>
#include <signal.h>
#include "ctype_.h"				/* for tolower */
#include "string_.h"
#include "gx.h"
#include "gp.h"
#ifdef __OVERLAY__
#  include "overlay.h"
#endif

/* Library routines not declared in a standard header */
extern char *getenv(P1(const char *));

/* Define the size of the C stack. */
unsigned _stklen = 8000;		/* default is 4096, we need more */

/* Define the size of the overlay buffer, if relevant. */
#ifdef __OVERLAY__
unsigned _ovrbuffer = (1024L * OVLBUFK) / 16;
#endif

/* Forward declarations */
private void handle_FPE(P3(int, int, int *));

/* Do platform-dependent initialization. */
#if CPU_TYPE > 86
/* Internal routine to set flags and read them back. */
/* We use __emit__ so we don't require an assembler. */
private int
push_pop_flags(unsigned flags)
{	__emit__(0x8b, 0x46, 6);	/* mov ax,flags */
	__emit__(0x50, 0x9d);		/* push ax; popf */
	__emit__(0x9c, 0x58);		/* pushf; pop ax */
}
#endif
void
gp_init(void)
{	/*
	 * Detect the processor type using the following algorithms:
	 *	The 8088/8086 truncate shift counts mod 32,
	 *	  the 80186 and up do not.
	 *	The 80186 and below fix FLAGS bits 15-12 to 1,
	 *	  the 80286 and up do not.
	 *	The 80386 allows setting FLAGS bits 14-12,
	 *	  the 80286 and below do not.
	 * We currently can't tell an 80386 from an 80486.
	 * Note that this algorithm will identify an 80386
	 *   running in Virtual 8086 mode as an 80386.
	 *   This is acceptable, because Ghostscript doesn't actually
	 *   use 80286 or 80386 addressing modes, only the additional
	 *   instructions available on these processors.
	 * (This algorithm is derived from the Intel manuals.)
	 */
#if CPU_TYPE > 86
	/* We have to be careful not to turn interrupts off! */
	int result, type;
	result = push_pop_flags(0x202);
	if ( (result & 0xf000) == 0xf000 )
	   {	/* CPU is an 8088/8086/80186 */
		   {	int shc = 33;	/* force shift by variable */
			result = 0xffff << shc;
		   }
		type = (result == 0 ? 186 : 86);
	   }
	else
	   {	/* CPU is an 80286/80386/... */
		result = push_pop_flags(0x7202);
		type = ((result & 0x7000) == 0 ? 286 : 386);
	   }
	/* A 486 is the same as a 386. */
#define CPU_EQUIV (CPU_TYPE > 386 ? 386 : CPU_TYPE)
	if ( type < CPU_EQUIV )
	   {	eprintf1("This executable requires an 80%d or higher.\n",
			 CPU_EQUIV);
		exit(1);
	   }
#endif

#ifdef __OVERLAY__
	/* Initialize the overlay machinery. */
	   {	int code;
#  ifdef OVEMS
		code = _OvrInitEms(OVEMS_HANDLE, OVEMS_FIRST, OVEMS_PAGES);
		if ( code )
			eprintf("Attempt to use EMS memory for overlays failed.\n");
#  endif
#  ifdef OVEXT
		code = _OvrInitExt(OVEXT_START, OVEXT_LENGTH);
		if ( code )
			eprintf("Attempt to use extended memory for overlays failed.\n");
#  endif
	   }
#endif
	/* Set up the handler for numeric exceptions. */
	signal(SIGFPE, handle_FPE);
	gp_init_console();
}

/* Trap numeric exceptions.  Someday we will do something */
/* more appropriate with these. */
private void
handle_FPE(int sig, int subcode, int *regs)
{	unsigned char far *ip = MK_PTR(regs[10], regs[9]);
	eprintf1("Numeric exception %d:\n", subcode);
	fprintf(estderr,
"AX=%04x  BX=%04x  CX=%04x  DX=%04x  SI=%04x  DI=%04x  BP=%04x\n",
		regs[8], regs[7], regs[6], regs[5], regs[2], regs[1], regs[0]);
	fprintf(estderr,
"DS=%04x  ES=%04x  CS:IP=%04x:%04x",
		regs[3], regs[4], regs[10], regs[9]);
	fflush(estderr);		/* in case of trap */
	fprintf(estderr, " --> %02x %02x %02x %02x %02x >> %02x %02x %02x %02x %02x\n",
		ip[-5], ip[-4], ip[-3], ip[-2], ip[-1],
		ip[0], ip[1], ip[2], ip[3], ip[4]);
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
		pfile = stdprn;
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
}

/* ------ File naming and accessing ------ */

/* Create and open a scratch file with a given name prefix. */
/* Write the actual file name at fname. */
FILE *
gp_open_scratch_file(const char *prefix, char *fname, const char *mode)
{	char *temp;
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
	strcat(fname, "XXXXXX");
	mktemp(fname);
	return fopen(fname, mode);
}

/* Open a file with the given name, as a stream of uninterpreted bytes. */
FILE *
gp_fopen(const char *fname, const char *mode)
{	return fopen(fname, mode);
}
