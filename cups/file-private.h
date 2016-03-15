/*
 * "$Id: file-private.h 11627 2014-02-20 16:15:09Z msweet $"
 *
 * Private file definitions for CUPS.
 *
 * Since stdio files max out at 256 files on many systems, we have to
 * write similar functions without this limit.  At the same time, using
 * our own file functions allows us to provide transparent support of
 * gzip'd print files, PPD files, etc.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
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


#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Types and structures...
 */

typedef enum				/**** _cupsFileCheck return values ****/
{
  _CUPS_FILE_CHECK_OK = 0,		/* Everything OK */
  _CUPS_FILE_CHECK_MISSING = 1,		/* File is missing */
  _CUPS_FILE_CHECK_PERMISSIONS = 2,	/* File (or parent dir) has bad perms */
  _CUPS_FILE_CHECK_WRONG_TYPE = 3,	/* File has wrong type */
  _CUPS_FILE_CHECK_RELATIVE_PATH = 4	/* File contains a relative path */
} _cups_fc_result_t;

typedef enum				/**** _cupsFileCheck file type values ****/
{
  _CUPS_FILE_CHECK_FILE = 0,		/* Check the file and parent directory */
  _CUPS_FILE_CHECK_PROGRAM = 1,		/* Check the program and parent directory */
  _CUPS_FILE_CHECK_FILE_ONLY = 2,	/* Check the file only */
  _CUPS_FILE_CHECK_DIRECTORY = 3	/* Check the directory */
} _cups_fc_filetype_t;

typedef void (*_cups_fc_func_t)(void *context, _cups_fc_result_t result,
				const char *message);

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


/*
 * Prototypes...
 */

extern _cups_fc_result_t	_cupsFileCheck(const char *filename,
					       _cups_fc_filetype_t filetype,
				               int dorootchecks,
					       _cups_fc_func_t cb,
					       void *context);
extern void			_cupsFileCheckFilter(void *context,
						     _cups_fc_result_t result,
						     const char *message);

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_FILE_PRIVATE_H_ */

/*
 * End of "$Id: file-private.h 11627 2014-02-20 16:15:09Z msweet $".
 */
