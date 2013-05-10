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

/* gxclfile.c */
/* File-based command list implementation */
#include "stdio_.h"
#include "string_.h"
#include "gserror.h"
#include "gserrors.h"
#include "gsmemory.h"
#include "gp.h"
#include "gxclio.h"

/* This is an implementation of the command list I/O interface */
/* that uses the file system for storage. */

/* ------ Open/close/unlink ------ */

int
clist_open_scratch(char *fname, clist_file_ptr *pcf, gs_memory_t *mem,
  bool ok_to_compress)
{	char fmode[4];
	strcpy(fmode, "w+");
	strcat(fmode, gp_fmode_binary_suffix);
	*pcf =
	  (clist_file_ptr)gp_open_scratch_file(gp_scratch_file_name_prefix,
					       fname, fmode);
	if ( *pcf == NULL )
	   {	eprintf1("Could not open the scratch file %s.\n", fname);
		return_error(gs_error_invalidfileaccess);
	   }
	return 0;
}

void
clist_fclose_and_unlink(clist_file_ptr cf, const char *fname)
{	fclose((FILE *)cf);
	unlink(fname);
}

/* ------ Writing ------ */

long
clist_space_available(long requested)
{	return requested;
}

int
clist_fwrite_chars(const void *data, uint len, clist_file_ptr cf)
{	return fwrite(data, 1, len, (FILE *)cf);
}

/* ------ Reading ------ */

int
clist_fread_chars(void *data, uint len, clist_file_ptr cf)
{	FILE *f = (FILE *)cf;
	byte *str = data;
	/* The typical implementation of fread */
	/* is extremely inefficient for small counts, */
	/* so we just use straight-line code instead. */
	switch ( len )
	   {
	default: return fread(str, 1, len, f);
	case 8: *str++ = (byte)getc(f);
	case 7: *str++ = (byte)getc(f);
	case 6: *str++ = (byte)getc(f);
	case 5: *str++ = (byte)getc(f);
	case 4: *str++ = (byte)getc(f);
	case 3: *str++ = (byte)getc(f);
	case 2: *str++ = (byte)getc(f);
	case 1: *str = (byte)getc(f);
	   }
	return len;
}

/* ------ Position/status ------ */

int
clist_ferror_code(clist_file_ptr cf)
{	return (ferror((FILE *)cf) ? gs_error_ioerror : 0);
}

long
clist_ftell(clist_file_ptr cf)
{	return ftell((FILE *)cf);
}

void
clist_rewind(clist_file_ptr cf, bool discard_data)
{	rewind((FILE *)cf);
}

int
clist_fseek(clist_file_ptr cf, long offset, int mode)
{	return fseek((FILE *)cf, offset, mode);
}
