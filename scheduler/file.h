/*
 * "$Id: file.h,v 1.1.2.2 2003/03/30 20:01:44 mike Exp $"
 *
 *   File definitions for the Common UNIX Printing System (CUPS).
 *
 *   Since stdio files max out at 256 files on many systems, we have to
 *   write similar functions without this limit.  At the same time, using
 *   our own file functions allows us to provide transparent support of
 *   gzip'd print files, PPD files, etc.
 *
 *   Copyright 1997-2003 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

#ifndef _CUPS_FILE_H
#  define _CUPS_FILE_H_

/*
 * Include necessary headers...
 */

#  ifdef HAVE_LIBZ
#    include <zlib.h>
#  endif /* HAVE_LIBZ */


/*
 * C++ magic...
 */

#  ifdef _cplusplus
extern "C" {
#  endif /* _cplusplus */


/*
 * CUPS file structure...
 */

typedef struct
{
  int		fd;			/* File descriptor */
  char		mode,			/* Mode ('r' or 'w') */
		compressed,		/* Compression used? */
		buf[2048],		/* Buffer */
		*ptr,			/* Pointer into buffer */
		*end;			/* End of buffer data */
  int		pos;			/* File position for start of buffer */

#  ifdef HAVE_LIBZ
  z_stream	stream;			/* Decompression stream */
  int		eof;			/* End of file? */
  unsigned char	cbuf[1024];		/* Decompression buffer */
#  endif /* HAVE_LIBZ */
} cups_file_t;


/*
 * Prototypes...
 */

extern int		cupsFileClose(cups_file_t *fp);
#define			cupsFileCompressed(fp) (fp)->compressed
extern int		cupsFileFlush(cups_file_t *fp);
extern int		cupsFileGetChar(cups_file_t *fp);
extern char		*cupsFileGets(cups_file_t *fp, char *buf, int buflen);
#define			cupsFileNumber(fp) (fp)->fd
extern cups_file_t	*cupsFileOpen(const char *filename, const char *mode);
extern int		cupsFilePrintf(cups_file_t *fp, const char *format, ...);
extern int		cupsFilePutChar(cups_file_t *fp, int c);
extern int		cupsFilePuts(cups_file_t *fp, const char *s);
extern int		cupsFileRead(cups_file_t *fp, char *buf, int bytes);
extern int		cupsFileSeek(cups_file_t *fp, int pos);
#define			cupsFileTell(fp) (fp)->pos
extern int		cupsFileWrite(cups_file_t *fp, const char *buf, int bytes);


#  ifdef _cplusplus
}
#  endif /* _cplusplus */
#endif /* !_CUPS_FILE_H_ */

/*
 * End of "$Id: file.h,v 1.1.2.2 2003/03/30 20:01:44 mike Exp $".
 */
