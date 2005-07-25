/*
 * "$Id$"
 *
 *   Private file definitions for the Common UNIX Printing System (CUPS).
 *
 *   Since stdio files max out at 256 files on many systems, we have to
 *   write similar functions without this limit.  At the same time, using
 *   our own file functions allows us to provide transparent support of
 *   gzip'd print files, PPD files, etc.
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

#ifndef _CUPS_FILE_PRIVATE_H_
#  define _CUPS_FILE_PRIVATE_H_

/*
 * Include necessary headers...
 */

#  include "file.h"
#  include <config.h>
#  ifdef HAVE_LIBZ
#    include <zlib.h>
#  endif /* HAVE_LIBZ */


/*
 * CUPS file structure...
 */

struct cups_file_s
{
  int		fd;			/* File descriptor */
  char		mode,			/* Mode ('r' or 'w') */
		compressed,		/* Compression used? */
		buf[2048],		/* Buffer */
		*ptr,			/* Pointer into buffer */
		*end;			/* End of buffer data */
  off_t		pos;			/* File position for start of buffer */
  int		eof;			/* End of file? */

#  ifdef HAVE_LIBZ
  z_stream	stream;			/* Decompression stream */
  unsigned char	cbuf[1024];		/* Decompression buffer */
#  endif /* HAVE_LIBZ */
};


#endif /* !_CUPS_FILE_PRIVATE_H_ */

/*
 * End of "$Id$".
 */
