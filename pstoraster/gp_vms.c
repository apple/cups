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

/* gp_vms.c */
/* VAX/VMS specific routines for Ghostscript */
#include "string_.h"
#include "gx.h"
#include "gp.h"
#include <stat.h>
#include <stdlib.h>		/* for exit() */
#include <unixio.h>

extern char *getenv(P1(const char *));

/* Apparently gcc doesn't allow extra arguments for fopen: */
#ifdef VMS		/* DEC C */
#  define fopen_VMS fopen
#else			/* gcc */
#  define fopen_VMS(name, mode, m1, m2) fopen(name, mode)
#endif


/* VMS string descriptor structure */
#define DSC$K_DTYPE_T 14
#define DSC$K_CLASS_S  1
struct dsc$descriptor_s {
	unsigned short	dsc$w_length;
	unsigned char	dsc$b_dtype;
	unsigned char	dsc$b_class;
	char		*dsc$a_pointer;
};
typedef struct dsc$descriptor_s descrip;

/* VMS RMS constants */
#define RMS$_NMF    99018
#define RMS$_NORMAL 65537
#define NAM$C_MAXRSS  255

struct file_enum_s {
  uint context, length;
  descrip *pattern;
};

extern uint
  LIB$FIND_FILE(descrip *, descrip *, uint *, descrip *, descrip *,
		uint *, uint *),
  LIB$FIND_FILE_END(uint *),
  SYS$FILESCAN (descrip *, uint *, uint *),
  SYS$PUTMSG (uint *, int (*)(), descrip *, uint);

private uint
strlength(char *str, uint maxlen, char term)
{	uint i = 0;
	while ( i < maxlen && str[i] != term ) i++;
	return i;
}

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
{	/* The program returns exit_status = 0 for OK, 1 for failure; */
	/* VMS has different conventions. */
	switch ( exit_status )
	  {
	  case 0:
	    exit(exit_OK);
	  case 1:
	    exit(exit_FAILED);
	  }
	exit(exit_status);
}

/* ------ Date and time ------ */

/* Read the current time (in seconds since Jan. 1, 1980) */
/* and fraction (in nanoseconds). */
void
gp_get_realtime(long *pdt)
{	struct {uint _l0, _l1;} binary_date, now, difference;
	long lib$ediv(), lib$subx(), sys$bintim(), sys$gettim();
	long units_per_second = 10000000;
	char *jan_1_1980 = "1-JAN-1980 00:00:00.00";
	descrip str_desc;

	/* For those interested, Wednesday, November 17, 1858 is the base
	   of the Modified Julian Day system adopted by the Smithsonian
	   Astrophysical Observatory in 1957 for satellite tracking.  (The
	   year 1858 preceded the oldest star catalog in use at the
	   observatory.)  VMS uses quadword time stamps which are offsets
	   in 100 nanosecond units from November 17, 1858.  With a 63-bit
	   absolute time representation (sign bit must be clear), VMS will
	   have no trouble with time until 31-JUL-31086 02:48:05.47. */

	/* Convert January 1, 1980 into a binary absolute time */
	str_desc.dsc$w_length  = strlen(jan_1_1980);
	str_desc.dsc$a_pointer = jan_1_1980;
	(void) sys$bintim (&str_desc, &binary_date);

	/* Compute number of 100 nanosecond units since January 1, 1980.  */
	(void) sys$gettim (&now);
	(void) lib$subx (&now, &binary_date, &difference);

	/* Convert to seconds and nanoseconds.  */
	(void) lib$ediv (&units_per_second, &difference, &pdt[0], &pdt[1]);
	pdt[1] *= 100;
}

/* Read the current user CPU time (in seconds) */
/* and fraction (in nanoseconds).  */
void
gp_get_usertime(long *pdt)
{	gp_get_realtime(pdt);	/* Use an approximation for now.  */
}

/* ------ Screen management ------ */

/* Get the environment variable that specifies the display to use. */
const char *
gp_getenv_display(void)
{	return getenv("DECW$DISPLAY");
}

/* ------ Printer accessing ------ */

/* Open a connection to a printer.  A null file name means use the */
/* standard printer connected to the machine, if any. */
/* Return NULL if the connection could not be opened. */
FILE *
gp_open_printer(char *fname, int binary_mode)
{
 	if (strlen(fname) == 0)
	{	strcpy(fname, gp_scratch_file_name_prefix);
		strcat(fname, "XXXXXX");
		mktemp(fname);
	}
	if ( binary_mode )
	{	/*
		 * Printing must be done exactly byte to byte,
		 * using "passall".  However the standard VMS symbiont
		 * does not treat stream-LF files correctly in this respect,
		 * but throws away \n characters.  Giving the file
		 * the record type "undefined", but accessing it as a
		 * normal stream-LF file does the trick.
		 */
		return fopen_VMS(fname, "w", "rfm = udf", "ctx = stm");
	}
	else
	{	/* Open as a normal text stream file. */
		return fopen_VMS(fname, "w", "rfm = var", "rat = cr");
	}
}

/* Close the connection to the printer. */
void
gp_close_printer(FILE *pfile, const char *fname)
{	fclose(pfile);
}

/* ------ File naming and accessing ------ */

/* Define the character used for separating file names in a list. */
const char gp_file_name_list_separator = ',';

/* Define the default scratch file name prefix. */
const char gp_scratch_file_name_prefix[] = "_temp_";

/* Define the name of the null output file. */
const char gp_null_file_name[] = "NLA0:";

/* Define the name that designates the current directory. */
const char gp_current_directory_name[] = "[]";

/* Define the string to be concatenated with the file mode */
/* for opening files without end-of-line conversion. */
const char gp_fmode_binary_suffix[] = "";
/* Define the file modes for binary reading or writing. */
const char gp_fmode_rb[] = "r";
const char gp_fmode_wb[] = "w";

/* Create and open a scratch file with a given name prefix. */
/* Write the actual file name at fname. */
FILE *
gp_open_scratch_file(const char *prefix, char *fname, const char *mode)
{	strcpy(fname, prefix);
	strcat(fname, "XXXXXX");
	mktemp(fname);
	return fopen(fname, mode);
}

/* Open a file with the given name, as a stream of uninterpreted bytes. */
/* We have to do something special if the file was FTP'ed in binary mode. */
/* Unfortunately, only DEC C supports the extra arguments to fopen. */
FILE *
gp_fopen(const char *fname, const char *mode)
{
#ifdef __DECC
#define FAB$C_FIX 1
        stat_t buffer;

        if ( stat((char *)fname, &buffer) == 0 )
	  if ( buffer.st_fab_rfm == FAB$C_FIX )
	    return fopen(fname, mode, "rfm=stmlf", "ctx=stm");
#endif
        return fopen(fname, mode);
}

/*  Answer whether a file name contains a directory/device specification, i.e.,
 *  is absolute (not directory- or device-relative).  Since for VMS, the concept
 *  of an "absolute" file reference has no meaning.  As Ghostscript is here
 *  merely checking to see if it will make sense to paste a path to the front
 *  of the file name, we use the VMS system service SYS$FILESCAN to check that
 *  the file name has no node, device, root, or directory specification: if all
 *  four of these items are missing from the file name then it is considered to
 *  a relative file name to which a path may be prefixed. (Roots are associated
 *  with rooted logical names.)
 */

bool
gp_file_name_is_absolute(const char *fname, uint len)
{
	descrip str_desc;
	/* SYS$FILESCAN takes a uint *, but we want to extract bits. */
	union {
		uint i;
		struct {
		 unsigned fscn$v_node : 1;
		 unsigned fscn$v_device : 1;
		 unsigned fscn$v_root : 1;
		 unsigned fscn$v_directory : 1;
		 unsigned fscn$v_name : 1;
		 unsigned fscn$v_type : 1;
		 unsigned fscn$v_version : 1;
		 unsigned fscn$v_fill_23 : 1;
		} s;
	} flags;
	uint zero = 0;

	str_desc.dsc$w_length  = len;
	str_desc.dsc$a_pointer = (char *)fname;
	SYS$FILESCAN (&str_desc, &zero, &flags.i);
	if ( flags.s.fscn$v_directory || flags.s.fscn$v_root ||
	     flags.s.fscn$v_device    || flags.s.fscn$v_node) return true;
	else return false;
}

/* Answer the string to be used for combining a directory/device prefix */
/* with a base file name.  The file name is known to not be absolute. */
const char *
gp_file_name_concat_string(const char *prefix, uint plen,
			   const char *fname, uint len)
{
	/*  Full VAX/VMS paths are of the form:
	 *
	 *    device:[root.][directory.subdirectory]filename.extension;version
	 *    logical:filename.extension;version
	 *
	 *  Roots are fairly rare and associated typically with rooted logical
	 *  names.
	 *
	 *  Examples:
	 *
	 *    DUA1:[GHOSTSCRIPT]GHOST.PS;1
	 *    THOR_DEC:[DOOF.A.B.C.D]FILE.DAT;-3
	 *    LOG:GHOST.PS  (LOG is a logical defined as DUA1:[GHOSTSCRIPT])
	 *    LOG:DOOF.DAT  (LOG is defined as DUA1, current directory is
	 *                   is used as the directory spec.)
	 *
	 */
	if ( plen > 0 )
	  switch ( prefix[plen - 1] )
	   {	case ':': case ']': return "";
	   };
	return ":";
}

/* ------ Wild card file search procedures ------ */

private void
gp_free_enumeration(file_enum *pfen)
{
	if (pfen) {
	  LIB$FIND_FILE_END(&pfen->context);
	  gs_free(pfen->pattern->dsc$a_pointer, pfen->length, 1,
		  "GP_ENUM(pattern)");
	  gs_free((char *)pfen->pattern, sizeof(descrip), 1,
		  "GP_ENUM(descriptor)");
	  gs_free((char *)pfen, sizeof(file_enum), 1,
		  "GP_ENUM(file_enum)");
	}
}

/* Begin an enumeration.  See gp.h for details. */

file_enum *
gp_enumerate_files_init(const char *pat, uint patlen,
  gs_memory_t *memory)
{
	file_enum *pfen;
	uint i, len;
	char *c, *newpat;

	pfen = (file_enum *)gs_malloc(sizeof (file_enum), 1,
				      "GP_ENUM(file_enum)");
	pfen->pattern = (descrip *)gs_malloc(sizeof (descrip), 1,
					     "GP_ENUM(descriptor)");
	newpat = (char *)gs_malloc(patlen, 1, "GP_ENUM(pattern)");

	/*  Copy the pattern removing backslash quoting characters and
	 *  transforming unquoted question marks, '?', to percent signs, '%'.
	 *  (VAX/VMS uses the wildcard '%' to represent exactly one character
	 *  and '*' to represent zero or more characters.  Any combination and
	 *  number of interspersed wildcards is permitted.)
	 */
	c = newpat;
	for ( i = 0; i < patlen; pat++, i++ )
	  switch (*pat) {
	    case '?'  :
		*c++ = '%'; break;
	    case '\\' :
		i++;
		if (i < patlen) *c++ = *++pat;
		break;
	    default   :
		*c++ = *pat; break;
	  }
	len = c - newpat;

	/* Pattern may not exceed 255 characters */
	if (len > 255) {
	  gs_free(newpat, patlen, 1, "GP_ENUM(pattern)");
	  gs_free((char *)pfen->pattern, sizeof (descrip), 1,
		  "GP_ENUM(descriptor)");
	  gs_free((char *)pfen, sizeof (file_enum), 1, "GP_ENUM(file_enum)");
	  return (file_enum *)0;
	}

	pfen->context = 0;
	pfen->length = patlen;
	pfen->pattern->dsc$w_length  = len;
	pfen->pattern->dsc$b_dtype   = DSC$K_DTYPE_T;
	pfen->pattern->dsc$b_class   = DSC$K_CLASS_S;
	pfen->pattern->dsc$a_pointer = newpat;

	return pfen;
}

/* Return the next file name in the enumeration.  The client passes in */
/* a scratch string and a max length.  If the name of the next file fits, */
/* the procedure returns the length.  If it doesn't fit, the procedure */
/* returns max length +1.  If there are no more files, the procedure */
/* returns -1. */

uint
gp_enumerate_files_next(file_enum *pfen, char *ptr, uint maxlen)
{
	char *c, filnam[NAM$C_MAXRSS];
	descrip result = {NAM$C_MAXRSS, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
	uint i, len;
  
	result.dsc$a_pointer = filnam;

	/* Find the next file which matches the pattern */
	i = LIB$FIND_FILE(pfen->pattern, &result, &pfen->context,
			  (descrip *)0, (descrip *)0, (uint *)0, (uint *)0);

	/* Check the return status */
	if (i == RMS$_NMF) {
	  gp_free_enumeration (pfen);
	  return (uint)-1;
	}
	else if (i != RMS$_NORMAL) return 0;
	else if ((len = strlength (filnam, NAM$C_MAXRSS, ' ')) > maxlen)
	  return maxlen+1;

	/* Copy the returned filename over to the input string ptr */
	c = ptr;
	for (i = 0; i < len; i++) *c++ = filnam[i];

	return len;
}

/* Clean up a file enumeration.  This is only called to abandon */
/* an enumeration partway through: ...next should do it if there are */
/* no more files to enumerate.  This should deallocate the file_enum */
/* structure and any subsidiary structures, strings, buffers, etc. */

void
gp_enumerate_files_close(file_enum *pfen)
{	gp_free_enumeration (pfen);
}

const char *
gp_strerror(int errnum)
{	return NULL;
}
