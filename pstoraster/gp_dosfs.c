/* Copyright (C) 1992, 1993, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gp_dosfs.c */
/* Common routines for MS-DOS (any compiler) and DesqView/X, */
/* which has a MS-DOS-like file system. */
#include "dos_.h"
#include "gx.h"
#include "gp.h"

/* ------ Printer accessing ------ */

/* Put a printer file (which might be stdout) into binary or text mode. */
/* This is not a standard gp procedure, */
/* but all MS-DOS configurations need it. */
void
gp_set_printer_binary(int prnfno, int binary)
{	union REGS regs;
	regs.h.ah = 0x44;	/* ioctl */
	regs.h.al = 0;		/* get device info */
	regs.rshort.bx = prnfno;
	intdos(&regs, &regs);
	if ( regs.rshort.cflag != 0 || !(regs.h.dl & 0x80) )
		return;		/* error, or not a device */
	if ( binary )
		regs.h.dl |= 0x20;	/* binary (no ^Z intervention) */
	else
		regs.h.dl &= ~0x20;	/* text */
	regs.h.dh = 0;
	regs.h.ah = 0x44;	/* ioctl */
	regs.h.al = 1;		/* set device info */
	intdos(&regs, &regs);
}

/* ------ File names ------ */

/* Define the character used for separating file names in a list. */
const char gp_file_name_list_separator = ';';

/* Define the string to be concatenated with the file mode */
/* for opening files without end-of-line conversion. */
const char gp_fmode_binary_suffix[] = "b";
/* Define the file modes for binary reading or writing. */
const char gp_fmode_rb[] = "rb";
const char gp_fmode_wb[] = "wb";

/* Answer whether a file name contains a directory/device specification, */
/* i.e. is absolute (not directory- or device-relative). */
bool
gp_file_name_is_absolute(const char *fname, unsigned len)
{	/* A file name is absolute if it contains a drive specification */
	/* (second character is a :) or if it start with 0 or more .s */
	/* followed by a / or \. */
	if ( len >= 2 && fname[1] == ':' )
	  return true;
	while ( len && *fname == '.' )
	  ++fname, --len;
	return (len && (*fname == '/' || *fname == '\\'));
}

/* Answer the string to be used for combining a directory/device prefix */
/* with a base file name.  The file name is known to not be absolute. */
const char *
gp_file_name_concat_string(const char *prefix, unsigned plen,
  const char *fname, unsigned len)
{	if ( plen > 0 )
	  switch ( prefix[plen - 1] )
	   {	case ':': case '/': case '\\': return "";
	   };
	return "\\";
}
