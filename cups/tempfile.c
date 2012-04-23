/*
 * "$Id: tempfile.c 7337 2008-02-22 04:44:04Z mike $"
 *
 *   Temp file utilities for CUPS.
 *
 *   Copyright 2007-2012 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsTempFd()    - Creates a temporary file.
 *   cupsTempFile()  - Generates a temporary filename.
 *   cupsTempFile2() - Creates a temporary CUPS file.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#if defined(WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#endif /* WIN32 || __EMX__ */


/*
 * 'cupsTempFd()' - Creates a temporary file.
 *
 * The temporary filename is returned in the filename buffer.
 * The temporary file is opened for reading and writing.
 */

int					/* O - New file descriptor or -1 on error */
cupsTempFd(char *filename,		/* I - Pointer to buffer */
           int  len)			/* I - Size of buffer */
{
  int		fd;			/* File descriptor for temp file */
  int		tries;			/* Number of tries */
  const char	*tmpdir;		/* TMPDIR environment var */
#ifdef WIN32
  char		tmppath[1024];		/* Windows temporary directory */
  DWORD		curtime;		/* Current time */
#else
  struct timeval curtime;		/* Current time */
#endif /* WIN32 */


 /*
  * See if TMPDIR is defined...
  */

#ifdef WIN32
  if ((tmpdir = getenv("TEMP")) == NULL)
  {
    GetTempPath(sizeof(tmppath), tmppath);
    tmpdir = tmppath;
  }
#else
 /*
  * Previously we put root temporary files in the default CUPS temporary
  * directory under /var/spool/cups.  However, since the scheduler cleans
  * out temporary files there and runs independently of the user apps, we
  * don't want to use it unless specifically told to by cupsd.
  */

  if ((tmpdir = getenv("TMPDIR")) == NULL)
#  ifdef __APPLE__
    tmpdir = "/private/tmp";		/* /tmp is a symlink to /private/tmp */
#  else
    tmpdir = "/tmp";
#  endif /* __APPLE__ */
#endif /* WIN32 */

 /*
  * Make the temporary name using the specified directory...
  */

  tries = 0;

  do
  {
#ifdef WIN32
   /*
    * Get the current time of day...
    */

    curtime =  GetTickCount() + tries;

   /*
    * Format a string using the hex time values...
    */

    snprintf(filename, len - 1, "%s/%05lx%08lx", tmpdir,
             GetCurrentProcessId(), curtime);
#else
   /*
    * Get the current time of day...
    */

    gettimeofday(&curtime, NULL);

   /*
    * Format a string using the hex time values...
    */

    snprintf(filename, len - 1, "%s/%05x%08x", tmpdir, (unsigned)getpid(),
             (unsigned)(curtime.tv_sec + curtime.tv_usec + tries));
#endif /* WIN32 */

   /*
    * Open the file in "exclusive" mode, making sure that we don't
    * stomp on an existing file or someone's symlink crack...
    */

#ifdef WIN32
    fd = open(filename, _O_CREAT | _O_RDWR | _O_TRUNC | _O_BINARY,
              _S_IREAD | _S_IWRITE);
#elif defined(O_NOFOLLOW)
    fd = open(filename, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
#else
    fd = open(filename, O_RDWR | O_CREAT | O_EXCL, 0600);
#endif /* WIN32 */

    if (fd < 0 && errno != EEXIST)
      break;

    tries ++;
  }
  while (fd < 0 && tries < 1000);

 /*
  * Return the file descriptor...
  */

  return (fd);
}


/*
 * 'cupsTempFile()' - Generates a temporary filename.
 *
 * The temporary filename is returned in the filename buffer.
 * This function is deprecated - use @link cupsTempFd@ or
 * @link cupsTempFile2@ instead.
 *
 * @deprecated@
 */

char *					/* O - Filename or @code NULL@ on error */
cupsTempFile(char *filename,		/* I - Pointer to buffer */
             int  len)			/* I - Size of buffer */
{
  int		fd;			/* File descriptor for temp file */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * See if a filename was specified...
  */

  if (filename == NULL)
  {
    filename = cg->tempfile;
    len      = sizeof(cg->tempfile);
  }

 /*
  * Create the temporary file...
  */

  if ((fd = cupsTempFd(filename, len)) < 0)
    return (NULL);

 /*
  * Close the temp file - it'll be reopened later as needed...
  */

  close(fd);

 /*
  * Return the temp filename...
  */

  return (filename);
}


/*
 * 'cupsTempFile2()' - Creates a temporary CUPS file.
 *
 * The temporary filename is returned in the filename buffer.
 * The temporary file is opened for writing.
 *
 * @since CUPS 1.2/OS X 10.5@
 */

cups_file_t *				/* O - CUPS file or @code NULL@ on error */
cupsTempFile2(char *filename,		/* I - Pointer to buffer */
              int  len)			/* I - Size of buffer */
{
  cups_file_t	*file;			/* CUPS file */
  int		fd;			/* File descriptor */


  if ((fd = cupsTempFd(filename, len)) < 0)
    return (NULL);
  else if ((file = cupsFileOpenFd(fd, "w")) == NULL)
  {
    close(fd);
    unlink(filename);
    return (NULL);
  }
  else
    return (file);
}


/*
 * End of "$Id: tempfile.c 7337 2008-02-22 04:44:04Z mike $".
 */
