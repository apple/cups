/*
 * "$Id$"
 *
 *   Public file definitions for the Common UNIX Printing System (CUPS).
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

#ifndef _CUPS_FILE_H_
#  define _CUPS_FILE_H_


/*
 * Include necessary headers...
 */

#  include <sys/types.h>


/*
 * C++ magic...
 */

#  ifdef _cplusplus
extern "C" {
#  endif /* _cplusplus */


/*
 * CUPS file definitions...
 */

#  define CUPS_FILE_NONE	0	/* No compression */
#  define CUPS_FILE_GZIP	1	/* GZIP compression */


/*
 * CUPS file type...
 */

typedef struct cups_file_s cups_file_t;


/*
 * Prototypes...
 */

extern int		cupsFileClose(cups_file_t *fp);
extern int		cupsFileCompression(cups_file_t *fp);
extern int		cupsFileEOF(cups_file_t *fp);
extern int		cupsFileFlush(cups_file_t *fp);
extern int		cupsFileGetChar(cups_file_t *fp);
extern char		*cupsFileGetConf(cups_file_t *fp, char *buf, size_t buflen,
			                 char **value, int *linenum);
extern char		*cupsFileGets(cups_file_t *fp, char *buf, size_t buflen);
extern int		cupsFileNumber(cups_file_t *fp);
extern cups_file_t	*cupsFileOpen(const char *filename, const char *mode);
extern cups_file_t	*cupsFileOpenFd(int fd, const char *mode);
extern int		cupsFilePrintf(cups_file_t *fp, const char *format, ...);
extern int		cupsFilePutChar(cups_file_t *fp, int c);
extern int		cupsFilePuts(cups_file_t *fp, const char *s);
extern ssize_t		cupsFileRead(cups_file_t *fp, char *buf, size_t bytes);
extern off_t		cupsFileRewind(cups_file_t *fp);
extern off_t		cupsFileSeek(cups_file_t *fp, off_t pos);
extern off_t		cupsFileTell(cups_file_t *fp);
extern ssize_t		cupsFileWrite(cups_file_t *fp, const char *buf, size_t bytes);


#  ifdef _cplusplus
}
#  endif /* _cplusplus */
#endif /* !_CUPS_FILE_H_ */

/*
 * End of "$Id$".
 */
