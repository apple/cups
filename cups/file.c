/*
 * File functions for CUPS.
 *
 * Since stdio files max out at 256 files on many systems, we have to
 * write similar functions without this limit.  At the same time, using
 * our own file functions allows us to provide transparent support of
 * gzip'd print files, PPD files, etc.
 *
 * Copyright 2007-2015 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include "file-private.h"
#include <sys/stat.h>
#include <sys/types.h>


/*
 * Local functions...
 */

#ifdef HAVE_LIBZ
static ssize_t	cups_compress(cups_file_t *fp, const char *buf, size_t bytes);
#endif /* HAVE_LIBZ */
static ssize_t	cups_fill(cups_file_t *fp);
static int	cups_open(const char *filename, int mode);
static ssize_t	cups_read(cups_file_t *fp, char *buf, size_t bytes);
static ssize_t	cups_write(cups_file_t *fp, const char *buf, size_t bytes);


#ifndef WIN32
/*
 * '_cupsFileCheck()' - Check the permissions of the given filename.
 */

_cups_fc_result_t			/* O - Check result */
_cupsFileCheck(
    const char          *filename,	/* I - Filename to check */
    _cups_fc_filetype_t filetype,	/* I - Type of file checks? */
    int                 dorootchecks,	/* I - Check for root permissions? */
    _cups_fc_func_t     cb,		/* I - Callback function */
    void                *context)	/* I - Context pointer for callback */

{
  struct stat		fileinfo;	/* File information */
  char			message[1024],	/* Message string */
			temp[1024],	/* Parent directory filename */
			*ptr;		/* Pointer into parent directory */
  _cups_fc_result_t	result;		/* Check result */


 /*
  * Does the filename contain a relative path ("../")?
  */

  if (strstr(filename, "../"))
  {
   /*
    * Yes, fail it!
    */

    result = _CUPS_FILE_CHECK_RELATIVE_PATH;
    goto finishup;
  }

 /*
  * Does the program even exist and is it accessible?
  */

  if (stat(filename, &fileinfo))
  {
   /*
    * Nope...
    */

    result = _CUPS_FILE_CHECK_MISSING;
    goto finishup;
  }

 /*
  * Check the execute bit...
  */

  result = _CUPS_FILE_CHECK_OK;

  switch (filetype)
  {
    case _CUPS_FILE_CHECK_DIRECTORY :
        if (!S_ISDIR(fileinfo.st_mode))
	  result = _CUPS_FILE_CHECK_WRONG_TYPE;
        break;

    default :
        if (!S_ISREG(fileinfo.st_mode))
	  result = _CUPS_FILE_CHECK_WRONG_TYPE;
        break;
  }

  if (result)
    goto finishup;

 /*
  * Are we doing root checks?
  */

  if (!dorootchecks)
  {
   /*
    * Nope, so anything (else) goes...
    */

    goto finishup;
  }

 /*
  * Verify permission of the file itself:
  *
  * 1. Must be owned by root
  * 2. Must not be writable by group
  * 3. Must not be setuid
  * 4. Must not be writable by others
  */

  if (fileinfo.st_uid ||		/* 1. Must be owned by root */
      (fileinfo.st_mode & S_IWGRP)  ||	/* 2. Must not be writable by group */
      (fileinfo.st_mode & S_ISUID) ||	/* 3. Must not be setuid */
      (fileinfo.st_mode & S_IWOTH))	/* 4. Must not be writable by others */
  {
    result = _CUPS_FILE_CHECK_PERMISSIONS;
    goto finishup;
  }

  if (filetype == _CUPS_FILE_CHECK_DIRECTORY ||
      filetype == _CUPS_FILE_CHECK_FILE_ONLY)
    goto finishup;

 /*
  * Now check the containing directory...
  */

  strlcpy(temp, filename, sizeof(temp));
  if ((ptr = strrchr(temp, '/')) != NULL)
  {
    if (ptr == temp)
      ptr[1] = '\0';
    else
      *ptr = '\0';
  }

  if (stat(temp, &fileinfo))
  {
   /*
    * Doesn't exist?!?
    */

    result   = _CUPS_FILE_CHECK_MISSING;
    filetype = _CUPS_FILE_CHECK_DIRECTORY;
    filename = temp;

    goto finishup;
  }

  if (fileinfo.st_uid ||		/* 1. Must be owned by root */
      (fileinfo.st_mode & S_IWGRP) ||	/* 2. Must not be writable by group */
      (fileinfo.st_mode & S_ISUID) ||	/* 3. Must not be setuid */
      (fileinfo.st_mode & S_IWOTH))	/* 4. Must not be writable by others */
  {
    result   = _CUPS_FILE_CHECK_PERMISSIONS;
    filetype = _CUPS_FILE_CHECK_DIRECTORY;
    filename = temp;
  }

 /*
  * Common return point...
  */

  finishup:

  if (cb)
  {
    cups_lang_t *lang = cupsLangDefault();
					/* Localization information */

    switch (result)
    {
      case _CUPS_FILE_CHECK_OK :
	  if (filetype == _CUPS_FILE_CHECK_DIRECTORY)
	    snprintf(message, sizeof(message),
		     _cupsLangString(lang, _("Directory \"%s\" permissions OK "
					     "(0%o/uid=%d/gid=%d).")),
		     filename, fileinfo.st_mode, (int)fileinfo.st_uid,
		     (int)fileinfo.st_gid);
	  else
	    snprintf(message, sizeof(message),
		     _cupsLangString(lang, _("File \"%s\" permissions OK "
					     "(0%o/uid=%d/gid=%d).")),
		     filename, fileinfo.st_mode, (int)fileinfo.st_uid,
		     (int)fileinfo.st_gid);
          break;

      case _CUPS_FILE_CHECK_MISSING :
	  if (filetype == _CUPS_FILE_CHECK_DIRECTORY)
	    snprintf(message, sizeof(message),
		     _cupsLangString(lang, _("Directory \"%s\" not available: "
					     "%s")),
		     filename, strerror(errno));
	  else
	    snprintf(message, sizeof(message),
		     _cupsLangString(lang, _("File \"%s\" not available: %s")),
		     filename, strerror(errno));
          break;

      case _CUPS_FILE_CHECK_PERMISSIONS :
	  if (filetype == _CUPS_FILE_CHECK_DIRECTORY)
	    snprintf(message, sizeof(message),
		     _cupsLangString(lang, _("Directory \"%s\" has insecure "
					     "permissions "
					     "(0%o/uid=%d/gid=%d).")),
		     filename, fileinfo.st_mode, (int)fileinfo.st_uid,
		     (int)fileinfo.st_gid);
	  else
	    snprintf(message, sizeof(message),
		     _cupsLangString(lang, _("File \"%s\" has insecure "
		                             "permissions "
					     "(0%o/uid=%d/gid=%d).")),
		     filename, fileinfo.st_mode, (int)fileinfo.st_uid,
		     (int)fileinfo.st_gid);
          break;

      case _CUPS_FILE_CHECK_WRONG_TYPE :
	  if (filetype == _CUPS_FILE_CHECK_DIRECTORY)
	    snprintf(message, sizeof(message),
		     _cupsLangString(lang, _("Directory \"%s\" is a file.")),
		     filename);
	  else
	    snprintf(message, sizeof(message),
		     _cupsLangString(lang, _("File \"%s\" is a directory.")),
		     filename);
          break;

      case _CUPS_FILE_CHECK_RELATIVE_PATH :
	  if (filetype == _CUPS_FILE_CHECK_DIRECTORY)
	    snprintf(message, sizeof(message),
		     _cupsLangString(lang, _("Directory \"%s\" contains a "
					     "relative path.")), filename);
	  else
	    snprintf(message, sizeof(message),
		     _cupsLangString(lang, _("File \"%s\" contains a relative "
					     "path.")), filename);
          break;
    }

    (*cb)(context, result, message);
  }

  return (result);
}


/*
 * '_cupsFileCheckFilter()' - Report file check results as CUPS filter messages.
 */

void
_cupsFileCheckFilter(
    void              *context,		/* I - Context pointer (unused) */
    _cups_fc_result_t result,		/* I - Result code */
    const char        *message)		/* I - Message text */
{
  const char	*prefix;		/* Messaging prefix */


  (void)context;

  switch (result)
  {
    default :
    case _CUPS_FILE_CHECK_OK :
        prefix = "DEBUG2";
	break;

    case _CUPS_FILE_CHECK_MISSING :
    case _CUPS_FILE_CHECK_WRONG_TYPE :
        prefix = "ERROR";
	fputs("STATE: +cups-missing-filter-warning\n", stderr);
	break;

    case _CUPS_FILE_CHECK_PERMISSIONS :
    case _CUPS_FILE_CHECK_RELATIVE_PATH :
        prefix = "ERROR";
	fputs("STATE: +cups-insecure-filter-warning\n", stderr);
	break;
  }

  fprintf(stderr, "%s: %s\n", prefix, message);
}
#endif /* !WIN32 */


/*
 * 'cupsFileClose()' - Close a CUPS file.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

int					/* O - 0 on success, -1 on error */
cupsFileClose(cups_file_t *fp)		/* I - CUPS file */
{
  int	fd;				/* File descriptor */
  char	mode;				/* Open mode */
  int	status;				/* Return status */
  int	is_stdio;			/* Is a stdio file? */


  DEBUG_printf(("cupsFileClose(fp=%p)", (void *)fp));

 /*
  * Range check...
  */

  if (!fp)
    return (-1);

 /*
  * Flush pending write data...
  */

  if (fp->mode == 'w')
    status = cupsFileFlush(fp);
  else
    status = 0;

#ifdef HAVE_LIBZ
  if (fp->compressed && status >= 0)
  {
    if (fp->mode == 'r')
    {
     /*
      * Free decompression data...
      */

      inflateEnd(&fp->stream);
    }
    else
    {
     /*
      * Flush any remaining compressed data...
      */

      unsigned char	trailer[8];	/* Trailer CRC and length */
      int		done;		/* Done writing... */


      fp->stream.avail_in = 0;

      for (done = 0;;)
      {
        if (fp->stream.next_out > fp->cbuf)
	{
	  if (cups_write(fp, (char *)fp->cbuf,
	                 (size_t)(fp->stream.next_out - fp->cbuf)) < 0)
	    status = -1;

	  fp->stream.next_out  = fp->cbuf;
	  fp->stream.avail_out = sizeof(fp->cbuf);
	}

        if (done || status < 0)
	  break;

        done = deflate(&fp->stream, Z_FINISH) == Z_STREAM_END &&
	       fp->stream.next_out == fp->cbuf;
      }

     /*
      * Write the CRC and length...
      */

      trailer[0] = (unsigned char)fp->crc;
      trailer[1] = (unsigned char)(fp->crc >> 8);
      trailer[2] = (unsigned char)(fp->crc >> 16);
      trailer[3] = (unsigned char)(fp->crc >> 24);
      trailer[4] = (unsigned char)fp->pos;
      trailer[5] = (unsigned char)(fp->pos >> 8);
      trailer[6] = (unsigned char)(fp->pos >> 16);
      trailer[7] = (unsigned char)(fp->pos >> 24);

      if (cups_write(fp, (char *)trailer, 8) < 0)
        status = -1;

     /*
      * Free all memory used by the compression stream...
      */

      deflateEnd(&(fp->stream));
    }
  }
#endif /* HAVE_LIBZ */

 /*
  * Save the file descriptor we used and free memory...
  */

  fd       = fp->fd;
  mode     = fp->mode;
  is_stdio = fp->is_stdio;

  if (fp->printf_buffer)
    free(fp->printf_buffer);

  free(fp);

 /*
  * Close the file, returning the close status...
  */

  if (mode == 's')
  {
    if (httpAddrClose(NULL, fd) < 0)
      status = -1;
  }
  else if (!is_stdio)
  {
    if (close(fd) < 0)
      status = -1;
  }

  return (status);
}


/*
 * 'cupsFileCompression()' - Return whether a file is compressed.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

int					/* O - @code CUPS_FILE_NONE@ or @code CUPS_FILE_GZIP@ */
cupsFileCompression(cups_file_t *fp)	/* I - CUPS file */
{
  return (fp ? fp->compressed : CUPS_FILE_NONE);
}


/*
 * 'cupsFileEOF()' - Return the end-of-file status.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

int					/* O - 1 on end of file, 0 otherwise */
cupsFileEOF(cups_file_t *fp)		/* I - CUPS file */
{
  return (fp ? fp->eof : 1);
}


/*
 * 'cupsFileFind()' - Find a file using the specified path.
 *
 * This function allows the paths in the path string to be separated by
 * colons (UNIX standard) or semicolons (Windows standard) and stores the
 * result in the buffer supplied.  If the file cannot be found in any of
 * the supplied paths, @code NULL@ is returned. A @code NULL@ path only
 * matches the current directory.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

const char *				/* O - Full path to file or @code NULL@ if not found */
cupsFileFind(const char *filename,	/* I - File to find */
             const char *path,		/* I - Colon/semicolon-separated path */
             int        executable,	/* I - 1 = executable files, 0 = any file/dir */
	     char       *buffer,	/* I - Filename buffer */
	     int        bufsize)	/* I - Size of filename buffer */
{
  char	*bufptr,			/* Current position in buffer */
	*bufend;			/* End of buffer */


 /*
  * Range check input...
  */

  DEBUG_printf(("cupsFileFind(filename=\"%s\", path=\"%s\", executable=%d, buffer=%p, bufsize=%d)", filename, path, executable, (void *)buffer, bufsize));

  if (!filename || !buffer || bufsize < 2)
    return (NULL);

  if (!path)
  {
   /*
    * No path, so check current directory...
    */

    if (!access(filename, 0))
    {
      strlcpy(buffer, filename, (size_t)bufsize);
      return (buffer);
    }
    else
      return (NULL);
  }

 /*
  * Now check each path and return the first match...
  */

  bufend = buffer + bufsize - 1;
  bufptr = buffer;

  while (*path)
  {
#ifdef WIN32
    if (*path == ';' || (*path == ':' && ((bufptr - buffer) > 1 || !isalpha(buffer[0] & 255))))
#else
    if (*path == ';' || *path == ':')
#endif /* WIN32 */
    {
      if (bufptr > buffer && bufptr[-1] != '/' && bufptr < bufend)
        *bufptr++ = '/';

      strlcpy(bufptr, filename, (size_t)(bufend - bufptr));

#ifdef WIN32
      if (!access(buffer, 0))
#else
      if (!access(buffer, executable ? X_OK : 0))
#endif /* WIN32 */
      {
        DEBUG_printf(("1cupsFileFind: Returning \"%s\"", buffer));
        return (buffer);
      }

      bufptr = buffer;
    }
    else if (bufptr < bufend)
      *bufptr++ = *path;

    path ++;
  }

 /*
  * Check the last path...
  */

  if (bufptr > buffer && bufptr[-1] != '/' && bufptr < bufend)
    *bufptr++ = '/';

  strlcpy(bufptr, filename, (size_t)(bufend - bufptr));

  if (!access(buffer, 0))
  {
    DEBUG_printf(("1cupsFileFind: Returning \"%s\"", buffer));
    return (buffer);
  }
  else
  {
    DEBUG_puts("1cupsFileFind: Returning NULL");
    return (NULL);
  }
}


/*
 * 'cupsFileFlush()' - Flush pending output.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

int					/* O - 0 on success, -1 on error */
cupsFileFlush(cups_file_t *fp)		/* I - CUPS file */
{
  ssize_t	bytes;			/* Bytes to write */


  DEBUG_printf(("cupsFileFlush(fp=%p)", (void *)fp));

 /*
  * Range check input...
  */

  if (!fp || fp->mode != 'w')
  {
    DEBUG_puts("1cupsFileFlush: Attempt to flush a read-only file...");
    return (-1);
  }

  bytes = (ssize_t)(fp->ptr - fp->buf);

  DEBUG_printf(("2cupsFileFlush: Flushing " CUPS_LLFMT " bytes...",
                CUPS_LLCAST bytes));

  if (bytes > 0)
  {
#ifdef HAVE_LIBZ
    if (fp->compressed)
      bytes = cups_compress(fp, fp->buf, (size_t)bytes);
    else
#endif /* HAVE_LIBZ */
      bytes = cups_write(fp, fp->buf, (size_t)bytes);

    if (bytes < 0)
      return (-1);

    fp->ptr = fp->buf;
  }

  return (0);
}


/*
 * 'cupsFileGetChar()' - Get a single character from a file.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

int					/* O - Character or -1 on end of file */
cupsFileGetChar(cups_file_t *fp)	/* I - CUPS file */
{
 /*
  * Range check input...
  */

  DEBUG_printf(("4cupsFileGetChar(fp=%p)", (void *)fp));

  if (!fp || (fp->mode != 'r' && fp->mode != 's'))
  {
    DEBUG_puts("5cupsFileGetChar: Bad arguments!");
    return (-1);
  }

 /*
  * If the input buffer is empty, try to read more data...
  */

  DEBUG_printf(("5cupsFileGetChar: fp->eof=%d, fp->ptr=%p, fp->end=%p", fp->eof, (void *)fp->ptr, (void *)fp->end));

  if (fp->ptr >= fp->end)
    if (cups_fill(fp) <= 0)
    {
      DEBUG_puts("5cupsFileGetChar: Unable to fill buffer!");
      return (-1);
    }

 /*
  * Return the next character in the buffer...
  */

  DEBUG_printf(("5cupsFileGetChar: Returning %d...", *(fp->ptr) & 255));

  fp->pos ++;

  DEBUG_printf(("6cupsFileGetChar: pos=" CUPS_LLFMT, CUPS_LLCAST fp->pos));

  return (*(fp->ptr)++ & 255);
}


/*
 * 'cupsFileGetConf()' - Get a line from a configuration file.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

char *					/* O  - Line read or @code NULL@ on end of file or error */
cupsFileGetConf(cups_file_t *fp,	/* I  - CUPS file */
                char        *buf,	/* O  - String buffer */
		size_t      buflen,	/* I  - Size of string buffer */
                char        **value,	/* O  - Pointer to value */
		int         *linenum)	/* IO - Current line number */
{
  char	*ptr;				/* Pointer into line */


 /*
  * Range check input...
  */

  DEBUG_printf(("2cupsFileGetConf(fp=%p, buf=%p, buflen=" CUPS_LLFMT
                ", value=%p, linenum=%p)", (void *)fp, (void *)buf, CUPS_LLCAST buflen, (void *)value, (void *)linenum));

  if (!fp || (fp->mode != 'r' && fp->mode != 's') ||
      !buf || buflen < 2 || !value)
  {
    if (value)
      *value = NULL;

    return (NULL);
  }

 /*
  * Read the next non-comment line...
  */

  *value = NULL;

  while (cupsFileGets(fp, buf, buflen))
  {
    (*linenum) ++;

   /*
    * Strip any comments...
    */

    if ((ptr = strchr(buf, '#')) != NULL)
    {
      if (ptr > buf && ptr[-1] == '\\')
      {
        // Unquote the #...
	_cups_strcpy(ptr - 1, ptr);
      }
      else
      {
        // Strip the comment and any trailing whitespace...
	while (ptr > buf)
	{
	  if (!_cups_isspace(ptr[-1]))
	    break;

	  ptr --;
	}

	*ptr = '\0';
      }
    }

   /*
    * Strip leading whitespace...
    */

    for (ptr = buf; _cups_isspace(*ptr); ptr ++);

    if (ptr > buf)
      _cups_strcpy(buf, ptr);

   /*
    * See if there is anything left...
    */

    if (buf[0])
    {
     /*
      * Yes, grab any value and return...
      */

      for (ptr = buf; *ptr; ptr ++)
        if (_cups_isspace(*ptr))
	  break;

      if (*ptr)
      {
       /*
        * Have a value, skip any other spaces...
	*/

        while (_cups_isspace(*ptr))
	  *ptr++ = '\0';

        if (*ptr)
	  *value = ptr;

       /*
        * Strip trailing whitespace and > for lines that begin with <...
	*/

        ptr += strlen(ptr) - 1;

        if (buf[0] == '<' && *ptr == '>')
	  *ptr-- = '\0';
	else if (buf[0] == '<' && *ptr != '>')
        {
	 /*
	  * Syntax error...
	  */

	  *value = NULL;
	  return (buf);
	}

        while (ptr > *value && _cups_isspace(*ptr))
	  *ptr-- = '\0';
      }

     /*
      * Return the line...
      */

      return (buf);
    }
  }

  return (NULL);
}


/*
 * 'cupsFileGetLine()' - Get a CR and/or LF-terminated line that may
 *                       contain binary data.
 *
 * This function differs from @link cupsFileGets@ in that the trailing CR
 * and LF are preserved, as is any binary data on the line. The buffer is
 * nul-terminated, however you should use the returned length to determine
 * the number of bytes on the line.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

size_t					/* O - Number of bytes on line or 0 on end of file */
cupsFileGetLine(cups_file_t *fp,	/* I - File to read from */
                char        *buf,	/* I - Buffer */
                size_t      buflen)	/* I - Size of buffer */
{
  int		ch;			/* Character from file */
  char		*ptr,			/* Current position in line buffer */
		*end;			/* End of line buffer */


 /*
  * Range check input...
  */

  DEBUG_printf(("2cupsFileGetLine(fp=%p, buf=%p, buflen=" CUPS_LLFMT ")", (void *)fp, (void *)buf, CUPS_LLCAST buflen));

  if (!fp || (fp->mode != 'r' && fp->mode != 's') || !buf || buflen < 3)
    return (0);

 /*
  * Now loop until we have a valid line...
  */

  for (ptr = buf, end = buf + buflen - 2; ptr < end ;)
  {
    if (fp->ptr >= fp->end)
      if (cups_fill(fp) <= 0)
        break;

    *ptr++ = ch = *(fp->ptr)++;
    fp->pos ++;

    if (ch == '\r')
    {
     /*
      * Check for CR LF...
      */

      if (fp->ptr >= fp->end)
	if (cups_fill(fp) <= 0)
          break;

      if (*(fp->ptr) == '\n')
      {
        *ptr++ = *(fp->ptr)++;
	fp->pos ++;
      }

      break;
    }
    else if (ch == '\n')
    {
     /*
      * Line feed ends a line...
      */

      break;
    }
  }

  *ptr = '\0';

  DEBUG_printf(("4cupsFileGetLine: pos=" CUPS_LLFMT, CUPS_LLCAST fp->pos));

  return ((size_t)(ptr - buf));
}


/*
 * 'cupsFileGets()' - Get a CR and/or LF-terminated line.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

char *					/* O - Line read or @code NULL@ on end of file or error */
cupsFileGets(cups_file_t *fp,		/* I - CUPS file */
             char        *buf,		/* O - String buffer */
	     size_t      buflen)	/* I - Size of string buffer */
{
  int		ch;			/* Character from file */
  char		*ptr,			/* Current position in line buffer */
		*end;			/* End of line buffer */


 /*
  * Range check input...
  */

  DEBUG_printf(("2cupsFileGets(fp=%p, buf=%p, buflen=" CUPS_LLFMT ")", (void *)fp, (void *)buf, CUPS_LLCAST buflen));

  if (!fp || (fp->mode != 'r' && fp->mode != 's') || !buf || buflen < 2)
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
    fp->pos ++;

    if (ch == '\r')
    {
     /*
      * Check for CR LF...
      */

      if (fp->ptr >= fp->end)
	if (cups_fill(fp) <= 0)
          break;

      if (*(fp->ptr) == '\n')
      {
        fp->ptr ++;
	fp->pos ++;
      }

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
      *ptr++ = (char)ch;
  }

  *ptr = '\0';

  DEBUG_printf(("4cupsFileGets: pos=" CUPS_LLFMT, CUPS_LLCAST fp->pos));

  return (buf);
}


/*
 * 'cupsFileLock()' - Temporarily lock access to a file.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

int					/* O - 0 on success, -1 on error */
cupsFileLock(cups_file_t *fp,		/* I - CUPS file */
             int         block)		/* I - 1 to wait for the lock, 0 to fail right away */
{
 /*
  * Range check...
  */

  if (!fp || fp->mode == 's')
    return (-1);

 /*
  * Try the lock...
  */

#ifdef WIN32
  return (_locking(fp->fd, block ? _LK_LOCK : _LK_NBLCK, 0));
#else
  return (lockf(fp->fd, block ? F_LOCK : F_TLOCK, 0));
#endif /* WIN32 */
}


/*
 * 'cupsFileNumber()' - Return the file descriptor associated with a CUPS file.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

int					/* O - File descriptor */
cupsFileNumber(cups_file_t *fp)		/* I - CUPS file */
{
  if (fp)
    return (fp->fd);
  else
    return (-1);
}


/*
 * 'cupsFileOpen()' - Open a CUPS file.
 *
 * The "mode" parameter can be "r" to read, "w" to write, overwriting any
 * existing file, "a" to append to an existing file or create a new file,
 * or "s" to open a socket connection.
 *
 * When opening for writing ("w"), an optional number from 1 to 9 can be
 * supplied which enables Flate compression of the file.  Compression is
 * not supported for the "a" (append) mode.
 *
 * When opening a socket connection, the filename is a string of the form
 * "address:port" or "hostname:port". The socket will make an IPv4 or IPv6
 * connection as needed, generally preferring IPv6 connections when there is
 * a choice.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

cups_file_t *				/* O - CUPS file or @code NULL@ if the file or socket cannot be opened */
cupsFileOpen(const char *filename,	/* I - Name of file */
             const char *mode)		/* I - Open mode */
{
  cups_file_t	*fp;			/* New CUPS file */
  int		fd;			/* File descriptor */
  char		hostname[1024],		/* Hostname */
		*portname;		/* Port "name" (number or service) */
  http_addrlist_t *addrlist;		/* Host address list */


  DEBUG_printf(("cupsFileOpen(filename=\"%s\", mode=\"%s\")", filename,
                mode));

 /*
  * Range check input...
  */

  if (!filename || !mode ||
      (*mode != 'r' && *mode != 'w' && *mode != 'a' && *mode != 's') ||
      (*mode == 'a' && isdigit(mode[1] & 255)))
    return (NULL);

 /*
  * Open the file...
  */

  switch (*mode)
  {
    case 'a' : /* Append file */
        fd = cups_open(filename,
		       O_RDWR | O_CREAT | O_APPEND | O_LARGEFILE | O_BINARY);
        break;

    case 'r' : /* Read file */
	fd = open(filename, O_RDONLY | O_LARGEFILE | O_BINARY, 0);
	break;

    case 'w' : /* Write file */
        fd = cups_open(filename, O_WRONLY | O_LARGEFILE | O_BINARY);
	if (fd < 0 && errno == ENOENT)
	{
	  fd = cups_open(filename,
	                 O_WRONLY | O_CREAT | O_EXCL | O_LARGEFILE | O_BINARY);
	  if (fd < 0 && errno == EEXIST)
	    fd = cups_open(filename, O_WRONLY | O_LARGEFILE | O_BINARY);
	}

	if (fd >= 0)
#ifdef WIN32
	  _chsize(fd, 0);
#else
	  ftruncate(fd, 0);
#endif /* WIN32 */
        break;

    case 's' : /* Read/write socket */
        strlcpy(hostname, filename, sizeof(hostname));
	if ((portname = strrchr(hostname, ':')) != NULL)
	  *portname++ = '\0';
	else
	  return (NULL);

       /*
        * Lookup the hostname and service...
	*/

        if ((addrlist = httpAddrGetList(hostname, AF_UNSPEC, portname)) == NULL)
	  return (NULL);

       /*
	* Connect to the server...
	*/

        if (!httpAddrConnect(addrlist, &fd))
	{
	  httpAddrFreeList(addrlist);
	  return (NULL);
	}

	httpAddrFreeList(addrlist);
	break;

    default : /* Remove bogus compiler warning... */
        return (NULL);
  }

  if (fd < 0)
    return (NULL);

 /*
  * Create the CUPS file structure...
  */

  if ((fp = cupsFileOpenFd(fd, mode)) == NULL)
  {
    if (*mode == 's')
      httpAddrClose(NULL, fd);
    else
      close(fd);
  }

 /*
  * Return it...
  */

  return (fp);
}

/*
 * 'cupsFileOpenFd()' - Open a CUPS file using a file descriptor.
 *
 * The "mode" parameter can be "r" to read, "w" to write, "a" to append,
 * or "s" to treat the file descriptor as a bidirectional socket connection.
 *
 * When opening for writing ("w"), an optional number from 1 to 9 can be
 * supplied which enables Flate compression of the file.  Compression is
 * not supported for the "a" (append) mode.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

cups_file_t *				/* O - CUPS file or @code NULL@ if the file could not be opened */
cupsFileOpenFd(int        fd,		/* I - File descriptor */
	       const char *mode)	/* I - Open mode */
{
  cups_file_t	*fp;			/* New CUPS file */


  DEBUG_printf(("cupsFileOpenFd(fd=%d, mode=\"%s\")", fd, mode));

 /*
  * Range check input...
  */

  if (fd < 0 || !mode ||
      (*mode != 'r' && *mode != 'w' && *mode != 'a' && *mode != 's') ||
      (*mode == 'a' && isdigit(mode[1] & 255)))
    return (NULL);

 /*
  * Allocate memory...
  */

  if ((fp = calloc(1, sizeof(cups_file_t))) == NULL)
    return (NULL);

 /*
  * Open the file...
  */

  fp->fd = fd;

  switch (*mode)
  {
    case 'a' :
        fp->pos = lseek(fd, 0, SEEK_END);

    case 'w' :
	fp->mode = 'w';
	fp->ptr  = fp->buf;
	fp->end  = fp->buf + sizeof(fp->buf);

#ifdef HAVE_LIBZ
	if (mode[1] >= '1' && mode[1] <= '9')
	{
	 /*
	  * Open a compressed stream, so write the standard gzip file
	  * header...
	  */

          unsigned char header[10];	/* gzip file header */
	  time_t	curtime;	/* Current time */


          curtime   = time(NULL);
	  header[0] = 0x1f;
	  header[1] = 0x8b;
	  header[2] = Z_DEFLATED;
	  header[3] = 0;
	  header[4] = (unsigned char)curtime;
	  header[5] = (unsigned char)(curtime >> 8);
	  header[6] = (unsigned char)(curtime >> 16);
	  header[7] = (unsigned char)(curtime >> 24);
	  header[8] = 0;
	  header[9] = 0x03;

	  cups_write(fp, (char *)header, 10);

         /*
	  * Initialize the compressor...
	  */

          deflateInit2(&(fp->stream), mode[1] - '0', Z_DEFLATED, -15, 8,
	               Z_DEFAULT_STRATEGY);

	  fp->stream.next_out  = fp->cbuf;
	  fp->stream.avail_out = sizeof(fp->cbuf);
	  fp->compressed       = 1;
	  fp->crc              = crc32(0L, Z_NULL, 0);
	}
#endif /* HAVE_LIBZ */
        break;

    case 'r' :
	fp->mode = 'r';
	break;

    case 's' :
        fp->mode = 's';
	break;

    default : /* Remove bogus compiler warning... */
        return (NULL);
  }

 /*
  * Don't pass this file to child processes...
  */

#ifndef WIN32
  fcntl(fp->fd, F_SETFD, fcntl(fp->fd, F_GETFD) | FD_CLOEXEC);
#endif /* !WIN32 */

  return (fp);
}


/*
 * 'cupsFilePeekChar()' - Peek at the next character from a file.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

int					/* O - Character or -1 on end of file */
cupsFilePeekChar(cups_file_t *fp)	/* I - CUPS file */
{
 /*
  * Range check input...
  */

  if (!fp || (fp->mode != 'r' && fp->mode != 's'))
    return (-1);

 /*
  * If the input buffer is empty, try to read more data...
  */

  if (fp->ptr >= fp->end)
    if (cups_fill(fp) <= 0)
      return (-1);

 /*
  * Return the next character in the buffer...
  */

  return (*(fp->ptr) & 255);
}


/*
 * 'cupsFilePrintf()' - Write a formatted string.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

int					/* O - Number of bytes written or -1 on error */
cupsFilePrintf(cups_file_t *fp,		/* I - CUPS file */
               const char  *format,	/* I - Printf-style format string */
	       ...)			/* I - Additional args as necessary */
{
  va_list	ap;			/* Argument list */
  ssize_t	bytes;			/* Formatted size */


  DEBUG_printf(("2cupsFilePrintf(fp=%p, format=\"%s\", ...)", (void *)fp, format));

  if (!fp || !format || (fp->mode != 'w' && fp->mode != 's'))
    return (-1);

  if (!fp->printf_buffer)
  {
   /*
    * Start with an 1k printf buffer...
    */

    if ((fp->printf_buffer = malloc(1024)) == NULL)
      return (-1);

    fp->printf_size = 1024;
  }

  va_start(ap, format);
  bytes = vsnprintf(fp->printf_buffer, fp->printf_size, format, ap);
  va_end(ap);

  if (bytes >= (ssize_t)fp->printf_size)
  {
   /*
    * Expand the printf buffer...
    */

    char	*temp;			/* Temporary buffer pointer */


    if (bytes > 65535)
      return (-1);

    if ((temp = realloc(fp->printf_buffer, (size_t)(bytes + 1))) == NULL)
      return (-1);

    fp->printf_buffer = temp;
    fp->printf_size   = (size_t)(bytes + 1);

    va_start(ap, format);
    bytes = vsnprintf(fp->printf_buffer, fp->printf_size, format, ap);
    va_end(ap);
  }

  if (fp->mode == 's')
  {
    if (cups_write(fp, fp->printf_buffer, (size_t)bytes) < 0)
      return (-1);

    fp->pos += bytes;

    DEBUG_printf(("4cupsFilePrintf: pos=" CUPS_LLFMT, CUPS_LLCAST fp->pos));

    return ((int)bytes);
  }

  if ((fp->ptr + bytes) > fp->end)
    if (cupsFileFlush(fp))
      return (-1);

  fp->pos += bytes;

  DEBUG_printf(("4cupsFilePrintf: pos=" CUPS_LLFMT, CUPS_LLCAST fp->pos));

  if ((size_t)bytes > sizeof(fp->buf))
  {
#ifdef HAVE_LIBZ
    if (fp->compressed)
      return ((int)cups_compress(fp, fp->printf_buffer, (size_t)bytes));
    else
#endif /* HAVE_LIBZ */
      return ((int)cups_write(fp, fp->printf_buffer, (size_t)bytes));
  }
  else
  {
    memcpy(fp->ptr, fp->printf_buffer, (size_t)bytes);
    fp->ptr += bytes;
    return ((int)bytes);
  }
}


/*
 * 'cupsFilePutChar()' - Write a character.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

int					/* O - 0 on success, -1 on error */
cupsFilePutChar(cups_file_t *fp,	/* I - CUPS file */
                int         c)		/* I - Character to write */
{
 /*
  * Range check input...
  */

  if (!fp || (fp->mode != 'w' && fp->mode != 's'))
    return (-1);

  if (fp->mode == 's')
  {
   /*
    * Send character immediately over socket...
    */

    char ch;				/* Output character */


    ch = (char)c;

    if (send(fp->fd, &ch, 1, 0) < 1)
      return (-1);
  }
  else
  {
   /*
    * Buffer it up...
    */

    if (fp->ptr >= fp->end)
      if (cupsFileFlush(fp))
	return (-1);

    *(fp->ptr) ++ = (char)c;
  }

  fp->pos ++;

  DEBUG_printf(("4cupsFilePutChar: pos=" CUPS_LLFMT, CUPS_LLCAST fp->pos));

  return (0);
}


/*
 * 'cupsFilePutConf()' - Write a configuration line.
 *
 * This function handles any comment escaping of the value.
 *
 * @since CUPS 1.4/macOS 10.6@
 */

ssize_t					/* O - Number of bytes written or -1 on error */
cupsFilePutConf(cups_file_t *fp,	/* I - CUPS file */
                const char *directive,	/* I - Directive */
		const char *value)	/* I - Value */
{
  ssize_t	bytes,			/* Number of bytes written */
		temp;			/* Temporary byte count */
  const char	*ptr;			/* Pointer into value */


  if (!fp || !directive || !*directive)
    return (-1);

  if ((bytes = cupsFilePuts(fp, directive)) < 0)
    return (-1);

  if (cupsFilePutChar(fp, ' ') < 0)
    return (-1);
  bytes ++;

  if (value && *value)
  {
    if ((ptr = strchr(value, '#')) != NULL)
    {
     /*
      * Need to quote the first # in the info string...
      */

      if ((temp = cupsFileWrite(fp, value, (size_t)(ptr - value))) < 0)
        return (-1);
      bytes += temp;

      if (cupsFilePutChar(fp, '\\') < 0)
        return (-1);
      bytes ++;

      if ((temp = cupsFilePuts(fp, ptr)) < 0)
        return (-1);
      bytes += temp;
    }
    else if ((temp = cupsFilePuts(fp, value)) < 0)
      return (-1);
    else
      bytes += temp;
  }

  if (cupsFilePutChar(fp, '\n') < 0)
    return (-1);
  else
    return (bytes + 1);
}


/*
 * 'cupsFilePuts()' - Write a string.
 *
 * Like the @code fputs@ function, no newline is appended to the string.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

int					/* O - Number of bytes written or -1 on error */
cupsFilePuts(cups_file_t *fp,		/* I - CUPS file */
             const char  *s)		/* I - String to write */
{
  ssize_t	bytes;			/* Bytes to write */


 /*
  * Range check input...
  */

  if (!fp || !s || (fp->mode != 'w' && fp->mode != 's'))
    return (-1);

 /*
  * Write the string...
  */

  bytes = (ssize_t)strlen(s);

  if (fp->mode == 's')
  {
    if (cups_write(fp, s, (size_t)bytes) < 0)
      return (-1);

    fp->pos += bytes;

    DEBUG_printf(("4cupsFilePuts: pos=" CUPS_LLFMT, CUPS_LLCAST fp->pos));

    return ((int)bytes);
  }

  if ((fp->ptr + bytes) > fp->end)
    if (cupsFileFlush(fp))
      return (-1);

  fp->pos += bytes;

  DEBUG_printf(("4cupsFilePuts: pos=" CUPS_LLFMT, CUPS_LLCAST fp->pos));

  if ((size_t)bytes > sizeof(fp->buf))
  {
#ifdef HAVE_LIBZ
    if (fp->compressed)
      return ((int)cups_compress(fp, s, (size_t)bytes));
    else
#endif /* HAVE_LIBZ */
      return ((int)cups_write(fp, s, (size_t)bytes));
  }
  else
  {
    memcpy(fp->ptr, s, (size_t)bytes);
    fp->ptr += bytes;
    return ((int)bytes);
  }
}


/*
 * 'cupsFileRead()' - Read from a file.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

ssize_t					/* O - Number of bytes read or -1 on error */
cupsFileRead(cups_file_t *fp,		/* I - CUPS file */
             char        *buf,		/* O - Buffer */
	     size_t      bytes)		/* I - Number of bytes to read */
{
  size_t	total;			/* Total bytes read */
  ssize_t	count;			/* Bytes read */


  DEBUG_printf(("2cupsFileRead(fp=%p, buf=%p, bytes=" CUPS_LLFMT ")", (void *)fp, (void *)buf, CUPS_LLCAST bytes));

 /*
  * Range check input...
  */

  if (!fp || !buf || (fp->mode != 'r' && fp->mode != 's'))
    return (-1);

  if (bytes == 0)
    return (0);

 /*
  * Loop until all bytes are read...
  */

  total = 0;
  while (bytes > 0)
  {
    if (fp->ptr >= fp->end)
      if (cups_fill(fp) <= 0)
      {
        DEBUG_printf(("4cupsFileRead: cups_fill() returned -1, total="
	              CUPS_LLFMT, CUPS_LLCAST total));

        if (total > 0)
          return ((ssize_t)total);
	else
	  return (-1);
      }

    count = (ssize_t)(fp->end - fp->ptr);
    if (count > (ssize_t)bytes)
      count = (ssize_t)bytes;

    memcpy(buf, fp->ptr,(size_t) count);
    fp->ptr += count;
    fp->pos += count;

    DEBUG_printf(("4cupsFileRead: pos=" CUPS_LLFMT, CUPS_LLCAST fp->pos));

   /*
    * Update the counts for the last read...
    */

    bytes -= (size_t)count;
    total += (size_t)count;
    buf   += count;
  }

 /*
  * Return the total number of bytes read...
  */

  DEBUG_printf(("3cupsFileRead: total=" CUPS_LLFMT, CUPS_LLCAST total));

  return ((ssize_t)total);
}


/*
 * 'cupsFileRewind()' - Set the current file position to the beginning of the
 *                      file.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

off_t					/* O - New file position or -1 on error */
cupsFileRewind(cups_file_t *fp)		/* I - CUPS file */
{
 /*
  * Range check input...
  */

  DEBUG_printf(("cupsFileRewind(fp=%p)", (void *)fp));
  DEBUG_printf(("2cupsFileRewind: pos=" CUPS_LLFMT, CUPS_LLCAST fp->pos));

  if (!fp || fp->mode != 'r')
    return (-1);

 /*
  * Handle special cases...
  */

  if (fp->bufpos == 0)
  {
   /*
    * No seeking necessary...
    */

    fp->pos = 0;

    if (fp->ptr)
    {
      fp->ptr = fp->buf;
      fp->eof = 0;
    }

    DEBUG_printf(("2cupsFileRewind: pos=" CUPS_LLFMT, CUPS_LLCAST fp->pos));

    return (0);
  }

 /*
  * Otherwise, seek in the file and cleanup any compression buffers...
  */

#ifdef HAVE_LIBZ
  if (fp->compressed)
  {
    inflateEnd(&fp->stream);
    fp->compressed = 0;
  }
#endif /* HAVE_LIBZ */

  if (lseek(fp->fd, 0, SEEK_SET))
  {
    DEBUG_printf(("1cupsFileRewind: lseek failed: %s", strerror(errno)));
    return (-1);
  }

  fp->bufpos = 0;
  fp->pos    = 0;
  fp->ptr    = NULL;
  fp->end    = NULL;
  fp->eof    = 0;

  DEBUG_printf(("2cupsFileRewind: pos=" CUPS_LLFMT, CUPS_LLCAST fp->pos));

  return (0);
}


/*
 * 'cupsFileSeek()' - Seek in a file.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

off_t					/* O - New file position or -1 on error */
cupsFileSeek(cups_file_t *fp,		/* I - CUPS file */
             off_t       pos)		/* I - Position in file */
{
  ssize_t	bytes;			/* Number bytes in buffer */


  DEBUG_printf(("cupsFileSeek(fp=%p, pos=" CUPS_LLFMT ")", (void *)fp, CUPS_LLCAST pos));
  DEBUG_printf(("2cupsFileSeek: fp->pos=" CUPS_LLFMT, CUPS_LLCAST fp->pos));
  DEBUG_printf(("2cupsFileSeek: fp->ptr=%p, fp->end=%p", (void *)fp->ptr, (void *)fp->end));

 /*
  * Range check input...
  */

  if (!fp || pos < 0 || fp->mode != 'r')
    return (-1);

 /*
  * Handle special cases...
  */

  if (pos == 0)
    return (cupsFileRewind(fp));

  if (fp->ptr)
  {
    bytes = (ssize_t)(fp->end - fp->buf);

    DEBUG_printf(("2cupsFileSeek: bytes=" CUPS_LLFMT, CUPS_LLCAST bytes));

    if (pos >= fp->bufpos && pos < (fp->bufpos + bytes))
    {
     /*
      * No seeking necessary...
      */

      fp->pos = pos;
      fp->ptr = fp->buf + pos - fp->bufpos;
      fp->eof = 0;

      return (pos);
    }
  }

#ifdef HAVE_LIBZ
  if (!fp->compressed && !fp->ptr)
  {
   /*
    * Preload a buffer to determine whether the file is compressed...
    */

    if (cups_fill(fp) <= 0)
      return (-1);
  }
#endif /* HAVE_LIBZ */

 /*
  * Seek forwards or backwards...
  */

  fp->eof = 0;

  if (pos < fp->bufpos)
  {
   /*
    * Need to seek backwards...
    */

    DEBUG_puts("2cupsFileSeek: SEEK BACKWARDS");

#ifdef HAVE_LIBZ
    if (fp->compressed)
    {
      inflateEnd(&fp->stream);

      lseek(fp->fd, 0, SEEK_SET);
      fp->bufpos = 0;
      fp->pos    = 0;
      fp->ptr    = NULL;
      fp->end    = NULL;

      while ((bytes = cups_fill(fp)) > 0)
        if (pos >= fp->bufpos && pos < (fp->bufpos + bytes))
	  break;

      if (bytes <= 0)
        return (-1);

      fp->ptr = fp->buf + pos - fp->bufpos;
      fp->pos = pos;
    }
    else
#endif /* HAVE_LIBZ */
    {
      fp->bufpos = lseek(fp->fd, pos, SEEK_SET);
      fp->pos    = fp->bufpos;
      fp->ptr    = NULL;
      fp->end    = NULL;

      DEBUG_printf(("2cupsFileSeek: lseek() returned " CUPS_LLFMT,
                    CUPS_LLCAST fp->pos));
    }
  }
  else
  {
   /*
    * Need to seek forwards...
    */

    DEBUG_puts("2cupsFileSeek: SEEK FORWARDS");

#ifdef HAVE_LIBZ
    if (fp->compressed)
    {
      while ((bytes = cups_fill(fp)) > 0)
      {
        if (pos >= fp->bufpos && pos < (fp->bufpos + bytes))
	  break;
      }

      if (bytes <= 0)
        return (-1);

      fp->ptr = fp->buf + pos - fp->bufpos;
      fp->pos = pos;
    }
    else
#endif /* HAVE_LIBZ */
    {
      fp->bufpos = lseek(fp->fd, pos, SEEK_SET);
      fp->pos    = fp->bufpos;
      fp->ptr    = NULL;
      fp->end    = NULL;

      DEBUG_printf(("2cupsFileSeek: lseek() returned " CUPS_LLFMT,
                    CUPS_LLCAST fp->pos));
    }
  }

  DEBUG_printf(("2cupsFileSeek: pos=" CUPS_LLFMT, CUPS_LLCAST fp->pos));

  return (fp->pos);
}


/*
 * 'cupsFileStderr()' - Return a CUPS file associated with stderr.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

cups_file_t *				/* O - CUPS file */
cupsFileStderr(void)
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals... */


 /*
  * Open file descriptor 2 as needed...
  */

  if (!cg->stdio_files[2])
  {
   /*
    * Flush any pending output on the stdio file...
    */

    fflush(stderr);

   /*
    * Open file descriptor 2...
    */

    if ((cg->stdio_files[2] = cupsFileOpenFd(2, "w")) != NULL)
      cg->stdio_files[2]->is_stdio = 1;
  }

  return (cg->stdio_files[2]);
}


/*
 * 'cupsFileStdin()' - Return a CUPS file associated with stdin.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

cups_file_t *				/* O - CUPS file */
cupsFileStdin(void)
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals... */


 /*
  * Open file descriptor 0 as needed...
  */

  if (!cg->stdio_files[0])
  {
   /*
    * Open file descriptor 0...
    */

    if ((cg->stdio_files[0] = cupsFileOpenFd(0, "r")) != NULL)
      cg->stdio_files[0]->is_stdio = 1;
  }

  return (cg->stdio_files[0]);
}


/*
 * 'cupsFileStdout()' - Return a CUPS file associated with stdout.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

cups_file_t *				/* O - CUPS file */
cupsFileStdout(void)
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals... */


 /*
  * Open file descriptor 1 as needed...
  */

  if (!cg->stdio_files[1])
  {
   /*
    * Flush any pending output on the stdio file...
    */

    fflush(stdout);

   /*
    * Open file descriptor 1...
    */

    if ((cg->stdio_files[1] = cupsFileOpenFd(1, "w")) != NULL)
      cg->stdio_files[1]->is_stdio = 1;
  }

  return (cg->stdio_files[1]);
}


/*
 * 'cupsFileTell()' - Return the current file position.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

off_t					/* O - File position */
cupsFileTell(cups_file_t *fp)		/* I - CUPS file */
{
  DEBUG_printf(("2cupsFileTell(fp=%p)", (void *)fp));
  DEBUG_printf(("3cupsFileTell: pos=" CUPS_LLFMT, CUPS_LLCAST (fp ? fp->pos : -1)));

  return (fp ? fp->pos : 0);
}


/*
 * 'cupsFileUnlock()' - Unlock access to a file.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

int					/* O - 0 on success, -1 on error */
cupsFileUnlock(cups_file_t *fp)		/* I - CUPS file */
{
 /*
  * Range check...
  */

  DEBUG_printf(("cupsFileUnlock(fp=%p)", (void *)fp));

  if (!fp || fp->mode == 's')
    return (-1);

 /*
  * Unlock...
  */

#ifdef WIN32
  return (_locking(fp->fd, _LK_UNLCK, 0));
#else
  return (lockf(fp->fd, F_ULOCK, 0));
#endif /* WIN32 */
}


/*
 * 'cupsFileWrite()' - Write to a file.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

ssize_t					/* O - Number of bytes written or -1 on error */
cupsFileWrite(cups_file_t *fp,		/* I - CUPS file */
              const char  *buf,		/* I - Buffer */
	      size_t      bytes)	/* I - Number of bytes to write */
{
 /*
  * Range check input...
  */

  DEBUG_printf(("2cupsFileWrite(fp=%p, buf=%p, bytes=" CUPS_LLFMT ")", (void *)fp, (void *)buf, CUPS_LLCAST bytes));

  if (!fp || !buf || (fp->mode != 'w' && fp->mode != 's'))
    return (-1);

  if (bytes == 0)
    return (0);

 /*
  * Write the buffer...
  */

  if (fp->mode == 's')
  {
    if (cups_write(fp, buf, bytes) < 0)
      return (-1);

    fp->pos += (off_t)bytes;

    DEBUG_printf(("4cupsFileWrite: pos=" CUPS_LLFMT, CUPS_LLCAST fp->pos));

    return ((ssize_t)bytes);
  }

  if ((fp->ptr + bytes) > fp->end)
    if (cupsFileFlush(fp))
      return (-1);

  fp->pos += (off_t)bytes;

  DEBUG_printf(("4cupsFileWrite: pos=" CUPS_LLFMT, CUPS_LLCAST fp->pos));

  if (bytes > sizeof(fp->buf))
  {
#ifdef HAVE_LIBZ
    if (fp->compressed)
      return (cups_compress(fp, buf, bytes));
    else
#endif /* HAVE_LIBZ */
      return (cups_write(fp, buf, bytes));
  }
  else
  {
    memcpy(fp->ptr, buf, bytes);
    fp->ptr += bytes;
    return ((ssize_t)bytes);
  }
}


#ifdef HAVE_LIBZ
/*
 * 'cups_compress()' - Compress a buffer of data.
 */

static ssize_t				/* O - Number of bytes written or -1 */
cups_compress(cups_file_t *fp,		/* I - CUPS file */
              const char  *buf,		/* I - Buffer */
	      size_t      bytes)	/* I - Number bytes */
{
  DEBUG_printf(("7cups_compress(fp=%p, buf=%p, bytes=" CUPS_LLFMT ")", (void *)fp, (void *)buf, CUPS_LLCAST bytes));

 /*
  * Update the CRC...
  */

  fp->crc = crc32(fp->crc, (const Bytef *)buf, (uInt)bytes);

 /*
  * Deflate the bytes...
  */

  fp->stream.next_in  = (Bytef *)buf;
  fp->stream.avail_in = (uInt)bytes;

  while (fp->stream.avail_in > 0)
  {
   /*
    * Flush the current buffer...
    */

    DEBUG_printf(("9cups_compress: avail_in=%d, avail_out=%d",
                  fp->stream.avail_in, fp->stream.avail_out));

    if (fp->stream.avail_out < (uInt)(sizeof(fp->cbuf) / 8))
    {
      if (cups_write(fp, (char *)fp->cbuf, (size_t)(fp->stream.next_out - fp->cbuf)) < 0)
        return (-1);

      fp->stream.next_out  = fp->cbuf;
      fp->stream.avail_out = sizeof(fp->cbuf);
    }

    deflate(&(fp->stream), Z_NO_FLUSH);
  }

  return ((ssize_t)bytes);
}
#endif /* HAVE_LIBZ */


/*
 * 'cups_fill()' - Fill the input buffer.
 */

static ssize_t				/* O - Number of bytes or -1 */
cups_fill(cups_file_t *fp)		/* I - CUPS file */
{
  ssize_t		bytes;		/* Number of bytes read */
#ifdef HAVE_LIBZ
  int			status;		/* Decompression status */
  const unsigned char	*ptr,		/* Pointer into buffer */
			*end;		/* End of buffer */
#endif /* HAVE_LIBZ */


  DEBUG_printf(("7cups_fill(fp=%p)", (void *)fp));
  DEBUG_printf(("9cups_fill: fp->ptr=%p, fp->end=%p, fp->buf=%p, fp->bufpos=" CUPS_LLFMT ", fp->eof=%d", (void *)fp->ptr, (void *)fp->end, (void *)fp->buf, CUPS_LLCAST fp->bufpos, fp->eof));

  if (fp->ptr && fp->end)
    fp->bufpos += fp->end - fp->buf;

#ifdef HAVE_LIBZ
  DEBUG_printf(("9cups_fill: fp->compressed=%d", fp->compressed));

  while (!fp->ptr || fp->compressed)
  {
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

     /*
      * Read the first bytes in the file to determine if we have a gzip'd
      * file...
      */

      if ((bytes = cups_read(fp, (char *)fp->buf, sizeof(fp->buf))) < 0)
      {
       /*
	* Can't read from file!
	*/

        DEBUG_printf(("9cups_fill: cups_read() returned " CUPS_LLFMT,
	              CUPS_LLCAST bytes));

        fp->eof = 1;

	return (-1);
      }

      if (bytes < 10 || fp->buf[0] != 0x1f ||
          (fp->buf[1] & 255) != 0x8b ||
          fp->buf[2] != 8 || (fp->buf[3] & 0xe0) != 0)
      {
       /*
	* Not a gzip'd file!
	*/

	fp->ptr = fp->buf;
	fp->end = fp->buf + bytes;

        DEBUG_printf(("9cups_fill: Returning " CUPS_LLFMT,
	              CUPS_LLCAST bytes));

	return (bytes);
      }

     /*
      * Parse header junk: extra data, original name, and comment...
      */

      ptr = (unsigned char *)fp->buf + 10;
      end = (unsigned char *)fp->buf + bytes;

      if (fp->buf[3] & 0x04)
      {
       /*
	* Skip extra data...
	*/

	if ((ptr + 2) > end)
	{
	 /*
	  * Can't read from file!
	  */

	  DEBUG_puts("9cups_fill: Extra gzip header data missing, returning -1.");

          fp->eof = 1;
	  errno   = EIO;

	  return (-1);
	}

	bytes = ((unsigned char)ptr[1] << 8) | (unsigned char)ptr[0];
	ptr   += 2 + bytes;

	if (ptr > end)
	{
	 /*
	  * Can't read from file!
	  */

	  DEBUG_puts("9cups_fill: Extra gzip header data does not fit in initial buffer, returning -1.");

          fp->eof = 1;
	  errno   = EIO;

	  return (-1);
	}
      }

      if (fp->buf[3] & 0x08)
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

	  DEBUG_puts("9cups_fill: Original filename in gzip header data does not fit in initial buffer, returning -1.");

          fp->eof = 1;
	  errno   = EIO;

	  return (-1);
	}
      }

      if (fp->buf[3] & 0x10)
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

	  DEBUG_puts("9cups_fill: Comment in gzip header data does not fit in initial buffer, returning -1.");

          fp->eof = 1;
	  errno   = EIO;

	  return (-1);
	}
      }

      if (fp->buf[3] & 0x02)
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

	  DEBUG_puts("9cups_fill: Header CRC in gzip header data does not fit in initial buffer, returning -1.");

          fp->eof = 1;
	  errno   = EIO;

	  return (-1);
	}
      }

     /*
      * Copy the flate-compressed data to the compression buffer...
      */

      if ((bytes = end - ptr) > 0)
        memcpy(fp->cbuf, ptr, (size_t)bytes);

     /*
      * Setup the decompressor data...
      */

      fp->stream.zalloc    = (alloc_func)0;
      fp->stream.zfree     = (free_func)0;
      fp->stream.opaque    = (voidpf)0;
      fp->stream.next_in   = (Bytef *)fp->cbuf;
      fp->stream.next_out  = NULL;
      fp->stream.avail_in  = (uInt)bytes;
      fp->stream.avail_out = 0;
      fp->crc              = crc32(0L, Z_NULL, 0);

      if ((status = inflateInit2(&(fp->stream), -15)) != Z_OK)
      {
	DEBUG_printf(("9cups_fill: inflateInit2 returned %d, returning -1.", status));

        fp->eof = 1;
        errno   = EIO;

	return (-1);
      }

      fp->compressed = 1;
    }

    if (fp->compressed)
    {
     /*
      * If we have reached end-of-file, return immediately...
      */

      if (fp->eof)
      {
        DEBUG_puts("9cups_fill: EOF, returning 0.");

	return (0);
      }

     /*
      * Fill the decompression buffer as needed...
      */

      if (fp->stream.avail_in == 0)
      {
	if ((bytes = cups_read(fp, (char *)fp->cbuf, sizeof(fp->cbuf))) <= 0)
	{
	  DEBUG_printf(("9cups_fill: cups_read error, returning %d.", (int)bytes));

	  fp->eof = 1;

          return (bytes);
	}

	fp->stream.next_in  = fp->cbuf;
	fp->stream.avail_in = (uInt)bytes;
      }

     /*
      * Decompress data from the buffer...
      */

      fp->stream.next_out  = (Bytef *)fp->buf;
      fp->stream.avail_out = sizeof(fp->buf);

      status = inflate(&(fp->stream), Z_NO_FLUSH);

      if (fp->stream.next_out > (Bytef *)fp->buf)
        fp->crc = crc32(fp->crc, (Bytef *)fp->buf,
	                (uInt)(fp->stream.next_out - (Bytef *)fp->buf));

      if (status == Z_STREAM_END)
      {
       /*
	* Read the CRC and length...
	*/

	unsigned char	trailer[8];	/* Trailer bytes */
	uLong		tcrc;		/* Trailer CRC */
	ssize_t		tbytes = 0;	/* Number of bytes */

	if (fp->stream.avail_in > 0)
	{
	  if (fp->stream.avail_in > sizeof(trailer))
	    tbytes = (ssize_t)sizeof(trailer);
	  else
	    tbytes = (ssize_t)fp->stream.avail_in;

	  memcpy(trailer, fp->stream.next_in, (size_t)tbytes);
	  fp->stream.next_in  += tbytes;
	  fp->stream.avail_in -= (size_t)tbytes;
	}

        if (tbytes < (ssize_t)sizeof(trailer))
	{
	  if (read(fp->fd, trailer + tbytes, sizeof(trailer) - (size_t)tbytes) < ((ssize_t)sizeof(trailer) - tbytes))
	  {
	   /*
	    * Can't get it, so mark end-of-file...
	    */

	    DEBUG_puts("9cups_fill: Unable to read gzip CRC trailer, returning -1.");

	    fp->eof = 1;
	    errno   = EIO;

	    return (-1);
	  }
	}

	tcrc = ((((((uLong)trailer[3] << 8) | (uLong)trailer[2]) << 8) |
		(uLong)trailer[1]) << 8) | (uLong)trailer[0];

	if (tcrc != fp->crc)
	{
	 /*
	  * Bad CRC, mark end-of-file...
	  */

	  DEBUG_printf(("9cups_fill: tcrc=%08x != fp->crc=%08x, returning -1.", (unsigned int)tcrc, (unsigned int)fp->crc));

	  fp->eof = 1;
	  errno   = EIO;

	  return (-1);
	}

       /*
	* Otherwise, reset the compressed flag so that we re-read the
	* file header...
	*/

	fp->compressed = 0;
      }
      else if (status < Z_OK)
      {
	DEBUG_printf(("9cups_fill: inflate returned %d, returning -1.", status));

        fp->eof = 1;
        errno   = EIO;

	return (-1);
      }

      bytes = (ssize_t)sizeof(fp->buf) - (ssize_t)fp->stream.avail_out;

     /*
      * Return the decompressed data...
      */

      fp->ptr = fp->buf;
      fp->end = fp->buf + bytes;

      if (bytes)
      {
        DEBUG_printf(("9cups_fill: Returning %d.", (int)bytes));
	return (bytes);
      }
    }
  }
#endif /* HAVE_LIBZ */

 /*
  * Read a buffer's full of data...
  */

  if ((bytes = cups_read(fp, fp->buf, sizeof(fp->buf))) <= 0)
  {
   /*
    * Can't read from file!
    */

    fp->eof = 1;
    fp->ptr = fp->buf;
    fp->end = fp->buf;
  }
  else
  {
   /*
    * Return the bytes we read...
    */

    fp->eof = 0;
    fp->ptr = fp->buf;
    fp->end = fp->buf + bytes;
  }

  DEBUG_printf(("9cups_fill: Not gzip, returning %d.", (int)bytes));

  return (bytes);
}


/*
 * 'cups_open()' - Safely open a file for writing.
 *
 * We don't allow appending to directories or files that are hard-linked or
 * symlinked.
 */

static int				/* O - File descriptor or -1 otherwise */
cups_open(const char *filename,		/* I - Filename */
	  int        mode)		/* I - Open mode */
{
  int		fd;			/* File descriptor */
  struct stat	fileinfo;		/* File information */
#ifndef WIN32
  struct stat	linkinfo;		/* Link information */
#endif /* !WIN32 */


 /*
  * Open the file...
  */

  if ((fd = open(filename, mode, 0666)) < 0)
    return (-1);

 /*
  * Then verify that the file descriptor doesn't point to a directory or hard-
  * linked file.
  */

  if (fstat(fd, &fileinfo))
  {
    close(fd);
    return (-1);
  }

  if (fileinfo.st_nlink != 1)
  {
    close(fd);
    errno = EPERM;
    return (-1);
  }

#ifdef WIN32
  if (fileinfo.st_mode & _S_IFDIR)
#else
  if (S_ISDIR(fileinfo.st_mode))
#endif /* WIN32 */
  {
    close(fd);
    errno = EISDIR;
    return (-1);
  }

#ifndef WIN32
 /*
  * Then use lstat to determine whether the filename is a symlink...
  */

  if (lstat(filename, &linkinfo))
  {
    close(fd);
    return (-1);
  }

  if (S_ISLNK(linkinfo.st_mode) ||
      fileinfo.st_dev != linkinfo.st_dev ||
      fileinfo.st_ino != linkinfo.st_ino ||
#ifdef HAVE_ST_GEN
      fileinfo.st_gen != linkinfo.st_gen ||
#endif /* HAVE_ST_GEN */
      fileinfo.st_nlink != linkinfo.st_nlink ||
      fileinfo.st_mode != linkinfo.st_mode)
  {
   /*
    * Yes, don't allow!
    */

    close(fd);
    errno = EPERM;
    return (-1);
  }
#endif /* !WIN32 */

  return (fd);
}


/*
 * 'cups_read()' - Read from a file descriptor.
 */

static ssize_t				/* O - Number of bytes read or -1 */
cups_read(cups_file_t *fp,		/* I - CUPS file */
          char        *buf,		/* I - Buffer */
	  size_t      bytes)		/* I - Number bytes */
{
  ssize_t	total;			/* Total bytes read */


  DEBUG_printf(("7cups_read(fp=%p, buf=%p, bytes=" CUPS_LLFMT ")", (void *)fp, (void *)buf, CUPS_LLCAST bytes));

 /*
  * Loop until we read at least 0 bytes...
  */

  for (;;)
  {
#ifdef WIN32
    if (fp->mode == 's')
      total = (ssize_t)recv(fp->fd, buf, (unsigned)bytes, 0);
    else
      total = (ssize_t)read(fp->fd, buf, (unsigned)bytes);
#else
    if (fp->mode == 's')
      total = recv(fp->fd, buf, bytes, 0);
    else
      total = read(fp->fd, buf, bytes);
#endif /* WIN32 */

    DEBUG_printf(("9cups_read: total=" CUPS_LLFMT, CUPS_LLCAST total));

    if (total >= 0)
      break;

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

static ssize_t				/* O - Number of bytes written or -1 */
cups_write(cups_file_t *fp,		/* I - CUPS file */
           const char  *buf,		/* I - Buffer */
	   size_t      bytes)		/* I - Number bytes */
{
  size_t	total;			/* Total bytes written */
  ssize_t	count;			/* Count this time */


  DEBUG_printf(("7cups_write(fp=%p, buf=%p, bytes=" CUPS_LLFMT ")", (void *)fp, (void *)buf, CUPS_LLCAST bytes));

 /*
  * Loop until all bytes are written...
  */

  total = 0;
  while (bytes > 0)
  {
#ifdef WIN32
    if (fp->mode == 's')
      count = (ssize_t)send(fp->fd, buf, (unsigned)bytes, 0);
    else
      count = (ssize_t)write(fp->fd, buf, (unsigned)bytes);
#else
    if (fp->mode == 's')
      count = send(fp->fd, buf, bytes, 0);
    else
      count = write(fp->fd, buf, bytes);
#endif /* WIN32 */

    DEBUG_printf(("9cups_write: count=" CUPS_LLFMT, CUPS_LLCAST count));

    if (count < 0)
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

    bytes -= (size_t)count;
    total += (size_t)count;
    buf   += count;
  }

 /*
  * Return the total number of bytes written...
  */

  return ((ssize_t)total);
}
