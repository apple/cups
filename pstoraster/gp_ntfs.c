/* Copyright (C) 1992, 1993, 1994, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gp_ntfs.c */
/* file system stuff for MS-Windows WIN32 and MS-Windows NT*/
/* hacked from gp_dosfs.c by Russell Lang */

#include "stdio_.h"
#include <fcntl.h>
#include "dos_.h"
#include "memory_.h"
#include "string_.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "gp.h"
#include "gsutil.h"
#include "windows_.h"

/* ------ Printer accessing ------ */

/* Put a printer file (which might be stdout) into binary or text mode. */
/* This is not a standard gp procedure, */
/* but all MS-DOS configurations need it. */
void
gp_set_printer_binary(int prnfno, int binary)
{
	/* UNIMPLEMENTED */
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
gp_file_name_is_absolute(const char *fname, uint len)
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
gp_file_name_concat_string(const char *prefix, uint plen,
  const char *fname, uint len)
{	if ( plen > 0 )
	  switch ( prefix[plen - 1] )
	   {	case ':': case '/': case '\\': return "";
	   };
	return "\\";
}

/* ------ File enumeration ------ */

struct file_enum_s {
	WIN32_FIND_DATA find_data;
	HANDLE find_handle;
	char *pattern;			/* orig pattern + modified pattern */
	int patlen;			/* orig pattern length */
	int pat_size;			/* allocate space for pattern */
	int head_size;			/* pattern length through last */
					/* :, / or \ */
	int first_time;
	gs_memory_t *memory;
};
gs_private_st_ptrs1(st_file_enum, struct file_enum_s, "file_enum",
  file_enum_enum_ptrs, file_enum_reloc_ptrs, pattern);

/* Initialize an enumeration.  Note that * and ? in a directory */
/* don't work, and \ is taken literally unless a second \ follows. */
file_enum *
gp_enumerate_files_init(const char *pat, uint patlen, gs_memory_t *mem)
{	file_enum *pfen = gs_alloc_struct(mem, file_enum, &st_file_enum, "gp_enumerate_files");
	int pat_size = 2 * patlen + 1;
	char *pattern;
	int hsize = 0;
	int i;
	if ( pfen == 0 ) return 0;

	/* pattern could be allocated as a string, */
	/* but it's simpler for GC and freeing to allocate it as bytes. */

	pattern = (char *)gs_alloc_bytes(mem, pat_size,
					 "gp_enumerate_files(pattern)");
	if ( pattern == 0 ) return 0;
	memcpy(pattern, pat, patlen);
	/* find directory name = header */
	for ( i = 0; i < patlen; i++ )
	{	switch ( pat[i] )
		{
		case '\\':
			if ( i + 1 < patlen && pat[i + 1] == '\\' )
				i++;
			/* falls through */
		case ':':
		case '/':
			hsize = i + 1;
		}
	}
	pattern[patlen] = 0;
	pfen->pattern = pattern;
	pfen->patlen = patlen;
	pfen->pat_size = pat_size;
	pfen->head_size = hsize;
	pfen->memory = mem;
	pfen->first_time = 1;
	memset(&pfen->find_data, 0, sizeof(pfen->find_data));
	pfen->find_handle = INVALID_HANDLE_VALUE;
	return pfen;
}

/* Enumerate the next file. */
uint
gp_enumerate_files_next(file_enum *pfen, char *ptr, uint maxlen)
{	int code = 0;
	uint len;
top:	if ( pfen->first_time )
	   {
		pfen->find_handle = FindFirstFile(pfen->pattern, &(pfen->find_data));
		if (pfen->find_handle == INVALID_HANDLE_VALUE)
		    code = -1;
	        pfen->first_time = 0;
	   }
	else
	   {
		if (!FindNextFile(pfen->find_handle, &(pfen->find_data)))
		    code = -1;
	   }
	if ( code != 0 )
	   {	/* All done, clean up. */
		gp_enumerate_files_close(pfen);
		return ~(uint)0;
	   }

	len = strlen(pfen->find_data.cFileName);

	if (pfen->head_size + len < maxlen) {
	    memcpy(ptr, pfen->pattern, pfen->head_size);
	    strcpy(ptr + pfen->head_size, pfen->find_data.cFileName);
	    return pfen->head_size + len;
	}

	if (pfen->head_size >= maxlen)
	    return 0;	/* no hope at all */

	memcpy(ptr, pfen->pattern, pfen->head_size);
	strncpy(ptr + pfen->head_size, pfen->find_data.cFileName, 
		maxlen - pfen->head_size - 1);
	return maxlen;
}

/* Clean up the file enumeration. */
void
gp_enumerate_files_close(file_enum *pfen)
{	gs_memory_t *mem = pfen->memory;
	if (pfen->find_handle != INVALID_HANDLE_VALUE)
	    FindClose(pfen->find_handle);
	gs_free_object(mem, pfen->pattern,
		       "gp_enumerate_files_close(pattern)");
	gs_free_object(mem, pfen, "gp_enumerate_files_close");
}
