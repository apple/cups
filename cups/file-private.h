/*
 * "$Id$"
 *
 *   Private file definitions for CUPS.
 *
 *   Since stdio files max out at 256 files on many systems, we have to
 *   write similar functions without this limit.  At the same time, using
 *   our own file functions allows us to provide transparent support of
 *   gzip'd print files, PPD files, etc.
 *
 *   Copyright 2007-2010 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */

#ifndef _CUPS_FILE_PRIVATE_H_
#  define _CUPS_FILE_PRIVATE_H_

/*
 * Include necessary headers...
 */

#  include "cups-private.h"
#  include <stdio.h>
#  include <stdlib.h>
#  include <stdarg.h>
#  include <fcntl.h>

#  ifdef HAVE_LIBZ
#    include <zlib.h>
#  endif /* HAVE_LIBZ */
#  ifdef WIN32
#    include <io.h>
#    include <sys/locking.h>
#  endif /* WIN32 */


/*
 * Some operating systems support large files via open flag O_LARGEFILE...
 */

#  ifndef O_LARGEFILE
#    define O_LARGEFILE 0
#  endif /* !O_LARGEFILE */


/*
 * Some operating systems don't define O_BINARY, which is used by Microsoft
 * and IBM to flag binary files...
 */

#  ifndef O_BINARY
#    define O_BINARY 0
#  endif /* !O_BINARY */


/*
 * Types and structures...
 */

struct _cups_file_s			/**** CUPS file structure... ****/

{
  int		fd;			/* File descriptor */
  char		mode,			/* Mode ('r' or 'w') */
		compressed,		/* Compression used? */
		is_stdio,		/* stdin/out/err? */
		eof,			/* End of file? */
		buf[4096],		/* Buffer */
		*ptr,			/* Pointer into buffer */
		*end;			/* End of buffer data */
  off_t		pos,			/* Position in file */
		bufpos;			/* File position for start of buffer */

#ifdef HAVE_LIBZ
  z_stream	stream;			/* (De)compression stream */
  Bytef		cbuf[4096];		/* (De)compression buffer */
  uLong		crc;			/* (De)compression CRC */
#endif /* HAVE_LIBZ */

  char		*printf_buffer;		/* cupsFilePrintf buffer */
  size_t	printf_size;		/* Size of cupsFilePrintf buffer */
};


#endif /* !_CUPS_FILE_PRIVATE_H_ */

/*
 * End of "$Id$".
 */
