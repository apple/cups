/*
 * "$Id: file.c,v 1.1 2003/03/28 22:27:59 mike Exp $"
 *
 *   File functions for the Common UNIX Printing System (CUPS).
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
 *
 * Contents:
 *
 *   cupsFileClose()   - Close a CUPS file.
 *   cupsFileGetChar() - Get a single character from a file.
 *   cupsFileGets()    - Get a CR and/or LF-terminated line.
 *   cupsFileOpen()    - Open a CUPS file.
 *   cupsFilePrintf()  - Write a formatted string.
 *   cupsFilePuts()    - Write a string.
 *   cups_fill()       - Fill the input buffer...
 *   cups_read()       - Read from a file descriptor.
 *   cups_write()      - Write to a file descriptor.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <cups/string.h>
#include <errno.h>

#ifdef WIN32
#  include <io.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#endif /* WIN32 */

#include "file.h"


/*
 * Local functions...
 */

static int	cups_fill(cups_file_t *fp);
static int	cups_read(int fd, char *buf, int bytes);
static int	cups_write(int fd, const char *buf, int bytes);


/*
 * 'cupsFileClose()' - Close a CUPS file.
 */

int					/* O - 0 on success, -1 on error */
cupsFileClose(cups_file_t *fp)		/* I - CUPS file */
{
  int	fd;				/* File descriptor */


 /*
  * Range check...
  */

  if (!fp)
    return (-1);

#ifdef HAVE_LIBZ
 /*
  * Free decompression data as needed...
  */

  if (fp->compressed && fp->mode == 'r')
    inflateEnd(&fp->stream);
#endif /* HAVE_LIBZ */

 /*
  * Save the file descriptor we used and free memory...
  */

  fd = fp->fd;

  free(fp);

 /*
  * Close the file, returning the close status...
  */

  return (close(fd));
}


/*
 * 'cupsFileGetChar()' - Get a single character from a file.
 */

int					/* O - Character or -1 on EOF */
cupsFileGetChar(cups_file_t *fp)	/* I - CUPS file */
{
 /*
  * Range check input...
  */

  if (!fp || fp->mode != 'r')
    return (-1);

 /*
  * If the input buffer is empty, try to read more data...
  */

  if (fp->ptr >= fp->end)
    if (cups_fill(fp) < 0)
      return (-1);

 /*
  * Return the next character in the buffer...
  */

  return (*(fp->ptr)++ & 255);
}


/*
 * 'cupsFileGets()' - Get a CR and/or LF-terminated line.
 */

char *					/* O - Line read or NULL on eof/error */
cupsFileGets(cups_file_t *fp,		/* I - CUPS file */
             char        *buf,		/* O - String buffer */
	     int         buflen)	/* I - Size of string buffer */
{
  int		ch;			/* Character from file */
  char		*ptr,			/* Current position in line buffer */
		*end;			/* End of line buffer */


 /*
  * Range check input...
  */

  if (!fp || fp->mode != 'r' || !buf || buflen < 2)
    return (NULL);

 /*
  * Now loop until we have a valid line...
  */

  for (ptr = buf, end = buf + buflen - 1; ptr < end ;)
  {
    if (fp->ptr >= fp->end)
      if (cups_fill(fp) <= 0)
      {
        if (ptr == buf)
	  return (NULL);
	else
          break;
      }

    ch = *(fp->ptr)++;

    if (ch == '\r')
    {
     /*
      * Check for CR LF...
      */

      if (fp->ptr >= fp->end)
	if (cups_fill(fp) <= 0)
          break;

      if (*(fp->ptr) == '\n')
        fp->ptr ++;      

      break;
    }
    else if (ch == '\n')
    {
     /*
      * Line feed ends a line...
      */

      break;
    }
    else
      *ptr++ = ch;
  }

  *ptr = '\0';

  return (buf);
}


/*
 * 'cupsFileOpen()' - Open a CUPS file.
 */

cups_file_t *				/* O - CUPS file or NULL */
cupsFileOpen(const char *filename,	/* I - Name of file */
             const char *mode)		/* I - Open mode */
{
  cups_file_t	*fp;			/* New CUPS file */
  int		o;			/* Open mode bits */


 /*
  * Range check input...
  */

  if (!filename || !mode || (*mode != 'r' && *mode != 'w'))
    return (NULL);

 /*
  * Allocate memory...
  */

  if ((fp = calloc(1, sizeof(cups_file_t))) == NULL)
    return (NULL);

 /*
  * Open the file...
  */

  if (*mode == 'r')
    o = O_RDONLY;
  else
    o = O_WRONLY | O_TRUNC | O_CREAT;

  if ((fp->fd = open(filename, o, 0644)) < 0)
  {
   /*
    * Can't open file!
    */

    free(fp);
    return (NULL);
  }

  fp->mode = *mode;

  return (fp);
}


/*
 * 'cupsFilePrintf()' - Write a formatted string.
 */

int					/* O - Number of bytes written or -1 */
cupsFilePrintf(cups_file_t *fp,		/* I - CUPS file */
               const char  *format,	/* I - Printf-style format string */
	       ...)			/* I - Additional args as necessary */
{
  va_list	ap;			/* Argument list */
  int		bytes;			/* Formatted size */


  if (!fp || !format || fp->mode != 'w')
    return (-1);

  va_start(ap, format);
  bytes = vsnprintf(fp->buf, sizeof(fp->buf), format, ap);
  va_end(ap);

  return (cups_write(fp->fd, fp->buf, bytes));
}


/*
 * 'cupsFilePuts()' - Write a string.
 */

int					/* O - Number of bytes written or -1 */
cupsFilePuts(cups_file_t *fp,		/* I - CUPS file */
             const char  *s)		/* I - String to write */
{
 /*
  * Range check input...
  */

  if (!fp || !s || fp->mode != 'w')
    return (-1);

 /*
  * Write the string...
  */

  return (cups_write(fp->fd, s, strlen(s)));
}


/*
 * 'cupsFileRead()' - Read from a file.
 */

int					/* O - Number of bytes read or -1 */
cupsFileRead(cups_file_t *fp,		/* I - CUPS file */
             char        *buf,		/* O - Buffer */
	     int         bytes)		/* I - Number of bytes to read */
{
  int	total,				/* Total bytes read */
	count;				/* Bytes read */


 /*
  * Range check input...
  */

  if (!fp || !buf || bytes <= 0 || fp->mode != 'r')
    return (-1);

 /*
  * Loop until all bytes are read...
  */

  total = 0;
  while (bytes > 0)
  {
    if (fp->ptr >= fp->end)
      if (cups_fill(fp) <= 0)
      {
        if (total > 0)
          return (total);
	else
	  return (-1);
      }

    count = fp->end - fp->ptr;
    if (count > bytes)
      count = bytes;

    memcpy(buf, fp->ptr, count);
    fp->ptr += count;

   /*
    * Update the counts for the last read...
    */

    bytes -= count;
    total += count;
    buf   += count;
  }

 /*
  * Return the total number of bytes read...
  */

  return (total);
}


/*
 * 'cupsFileSeek()' - Seek in a file.
 */

int					/* O - New file position or -1 */
cupsFileSeek(cups_file_t *fp,		/* I - CUPS file */
             int         pos)		/* I - Position in file */
{
  int	bytes;				/* Number bytes in buffer */


 /*
  * Range check input...
  */

  if (!fp || pos < 0 || fp->mode != 'r')
    return (-1);

 /*
  * Figure out the number of bytes in the current buffer, and then
  * see if we are outside of it...
  */

  bytes = fp->end - fp->buf;

  if (pos < fp->pos)
  {
   /*
    * Need to seek backwards...
    */

#ifdef HAVE_LIBZ
    if (fp->compressed)
    {
      inflateEnd(&fp->stream);

      lseek(fp->fd, 0, SEEK_SET);
      fp->pos = 0;
      fp->ptr = NULL;
      fp->end = NULL;

      while ((bytes = cups_fill(fp)) > 0)
        if (pos >= fp->pos && pos < (fp->pos + bytes))
	  break;

      if (bytes <= 0)
        return (-1);
    }
    else
#endif /* HAVE_LIBZ */
    {
      lseek(fp->fd, pos, SEEK_SET);
      fp->pos = pos;
      fp->ptr = NULL;
      fp->end = NULL;
    }
  }
  else if (pos >= (fp->pos + bytes))
  {
   /*
    * Need to seek forwards...
    */

#ifdef HAVE_LIBZ
    if (fp->compressed)
    {
      while ((bytes = cups_fill(fp)) > 0)
        if (pos >= fp->pos && pos < (fp->pos + bytes))
	  break;

      if (bytes <= 0)
        return (-1);
    }
    else
#endif /* HAVE_LIBZ */
    {
      lseek(fp->fd, pos, SEEK_SET);
      fp->pos = pos;
      fp->ptr = NULL;
      fp->end = NULL;
    }
  }
  else
  {
   /*
    * Just reposition the current pointer, since we have the right
    * range...
    */

    fp->ptr = fp->buf + pos - fp->pos;
  }

  return (pos);
}


/*
 * 'cups_fill()' - Fill the input buffer...
 */

static int				/* O - Number of bytes or -1 */
cups_fill(cups_file_t *fp)		/* I - CUPS file */
{
  int			bytes;		/* Number of bytes read */
  const unsigned char	*ptr,		/* Pointer into buffer */
			*end;		/* End of buffer */


 /*
  * Update the "pos" element as needed...
  */

  if (fp->ptr)
    fp->pos += fp->end - fp->buf;

#ifdef HAVE_LIBZ
 /*
  * Check to see if we have read any data yet; if not, see if we have a
  * compressed file...
  */

  if (!fp->ptr)
  {
   /*
    * Reset the file position in case we are seeking...
    */

    fp->compressed = 0;
    fp->pos        = 0;

   /*
    * Read the first bytes in the file to determine if we have a gzip'd
    * file...
    */

    if ((bytes = cups_read(fp->fd, (char *)fp->cbuf, sizeof(fp->cbuf))) < 0)
    {
     /*
      * Can't read from file!
      */

      return (-1);
    }

    if (bytes < 10 || fp->cbuf[0] != 0x1f || fp->cbuf[1] != 0x8b ||
        fp->cbuf[2] != 8 || (fp->cbuf[3] & 0xe0) != 0)
    {
     /*
      * Not a gzip'd file!
      */

      memcpy(fp->buf, fp->cbuf, bytes);

      fp->ptr = fp->buf;
      fp->end = fp->buf + bytes;

      return (bytes);
    }

   /*
    * Parse header junk: extra data, original name, and comment...
    */

    ptr = fp->cbuf + 10;
    end = fp->cbuf + bytes;

    if (fp->cbuf[3] & 0x04)
    {
     /*
      * Skip extra data...
      */

      if ((ptr + 2) > end)
      {
       /*
	* Can't read from file!
	*/

	return (-1);
      }

      bytes = ((unsigned char)ptr[1] << 8) | (unsigned char)ptr[0];
      ptr   += 2 + bytes;

      if (ptr > end)
      {
       /*
	* Can't read from file!
	*/

	return (-1);
      }
    }

    if (fp->cbuf[3] & 0x08)
    {
     /*
      * Skip original name data...
      */

      while (ptr < end && *ptr)
        ptr ++;

      if (ptr < end)
        ptr ++;
      else
      {
       /*
	* Can't read from file!
	*/

	return (-1);
      }
    }

    if (fp->cbuf[3] & 0x10)
    {
     /*
      * Skip comment data...
      */

      while (ptr < end && *ptr)
        ptr ++;

      if (ptr < end)
        ptr ++;
      else
      {
       /*
	* Can't read from file!
	*/

	return (-1);
      }
    }

    if (fp->cbuf[3] & 0x02)
    {
     /*
      * Skip header CRC data...
      */

      ptr += 2;

      if (ptr > end)
      {
       /*
	* Can't read from file!
	*/

	return (-1);
      }
    }

   /*
    * Setup the decompressor data...
    */

    fp->stream.zalloc    = (alloc_func)0;
    fp->stream.zfree     = (free_func)0;
    fp->stream.opaque    = (voidpf)0;
    fp->stream.next_in   = (Bytef *)ptr;
    fp->stream.next_out  = NULL;
    fp->stream.avail_in  = end - ptr;
    fp->stream.avail_out = 0;

    if (inflateInit2(&(fp->stream), -15) != Z_OK)
      return (-1);

    fp->compressed = 1;
  }

  if (fp->compressed)
  {
   /*
    * If we have reached end-of-file, return immediately...
    */

    if (fp->eof)
      return (-1);

   /*
    * Fill the decompression buffer as needed...
    */

    if (fp->stream.avail_in == 0)
    {
      if ((bytes = cups_read(fp->fd, (char *)fp->cbuf, sizeof(fp->cbuf))) <= 0)
        return (-1);

      fp->stream.next_in  = fp->cbuf;
      fp->stream.avail_in = bytes;
    }

   /*
    * Decompress data from the buffer...
    */

    fp->stream.next_out  = fp->buf;
    fp->stream.avail_out = sizeof(fp->buf);

    if (inflate(&(fp->stream), Z_NO_FLUSH) == Z_STREAM_END)
    {
     /*
      * Mark end-of-file; note: we do not support concatenated gzip files
      * like gunzip does...
      */

      fp->eof = 1;
    }

    bytes = sizeof(fp->buf) - fp->stream.avail_out;

   /*
    * Return the decompressed data...
    */

    fp->ptr = fp->buf;
    fp->end = fp->buf + bytes;

    return (bytes);
  }
#endif /* HAVE_LIBZ */

 /*
  * Read a buffer's full of data...
  */

  if ((bytes = cups_read(fp->fd, fp->buf, sizeof(fp->buf))) <= 0)
  {
   /*
    * Can't read from file!
    */

    fp->ptr = fp->buf;
    fp->end = fp->buf;

    return (-1);
  }

 /*
  * Return the bytes we read...
  */

  fp->ptr = fp->buf;
  fp->end = fp->buf + bytes;

  return (bytes);
}


/*
 * 'cups_read()' - Read from a file descriptor.
 */

int					/* O - Number of bytes read or -1 */
cups_read(int  fd,			/* I - File descriptor */
          char *buf,			/* I - Buffer */
	  int  bytes)			/* I - Number bytes */
{
  int	total;				/* Total bytes read */


 /*
  * Loop until we read at least 0 bytes...
  */

  while ((total = read(fd, buf, bytes)) < 0)
  {
   /*
    * Reads can be interrupted by signals and unavailable resources...
    */

    if (errno == EAGAIN || errno == EINTR)
      continue;
    else
      return (-1);
  }

 /*
  * Return the total number of bytes read...
  */

  return (total);
}


/*
 * 'cups_write()' - Write to a file descriptor.
 */

int					/* O - Number of bytes written or -1 */
cups_write(int        fd,		/* I - File descriptor */
           const char *buf,		/* I - Buffer */
	   int        bytes)		/* I - Number bytes */
{
  int	total,				/* Total bytes written */
	count;				/* Count this time */


 /*
  * Loop until all bytes are written...
  */

  total = 0;
  while (bytes > 0)
  {
    if ((count = write(fd, buf, bytes)) < 0)
    {
     /*
      * Writes can be interrupted by signals and unavailable resources...
      */

      if (errno == EAGAIN || errno == EINTR)
        continue;
      else
        return (-1);
    }

   /*
    * Update the counts for the last write call...
    */

    bytes -= count;
    total += count;
    buf   += count;
  }

 /*
  * Return the total number of bytes written...
  */

  return (total);
}


/*
 * End of "$Id: file.c,v 1.1 2003/03/28 22:27:59 mike Exp $".
 */
