/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gxclio.h */
/* I/O interface for command lists */

#ifndef gxclio_INCLUDED
#  define gxclio_INCLUDED

/*
 * We intend that there be two implementations of the I/O interface for
 * command lists -- one suitable for embedded systems, which stores the
 * "files" in RAM, and one suitable for other systems, which uses an
 * external file system -- with the choice made at compile/link time.
 * This header file defines the API between the command list code proper
 * and its I/O interface.
 */

typedef void *clist_file_ptr;	/* We can't do any better than this. */

/* ---------------- Open/close/unlink ---------------- */

int clist_open_scratch(P4(char *fname, clist_file_ptr *pcf, gs_memory_t *mem,
			  bool ok_to_compress));

void clist_fclose_and_unlink(P2(clist_file_ptr cf, const char *fname));

/* ---------------- Writing ---------------- */

/* clist_space_available returns min(requested, available). */
long clist_space_available(P1(long requested));

int clist_fwrite_chars(P3(const void *data, uint len, clist_file_ptr cf));

/* ---------------- Reading ---------------- */

int clist_fread_chars(P3(void *data, uint len, clist_file_ptr cf));

/* ---------------- Position/status ---------------- */

/* clist_ferror_code returns an error code per gserrors.h, not a Boolean. */
int clist_ferror_code(P1(clist_file_ptr cf));

long clist_ftell(P1(clist_file_ptr cf));

void clist_rewind(P2(clist_file_ptr cf, bool discard_data));

int clist_fseek(P3(clist_file_ptr cf, long offset, int mode));

#endif					/* gxclio_INCLUDED */
