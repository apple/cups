/*
 * "$Id: tempfile.c,v 1.1.2.1 2001/12/26 16:52:13 mike Exp $"
 *
 *   Temp file utilities for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2001 by Easy Software Products.
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
 *   cupsTempFd()   - Create a temporary file.
 *   cupsTempFile() - Generate a temporary filename.
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include "string.h"
#include "debug.h"
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#if defined(WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#endif /* WIN32 || __EMX__ */


/*
 * 'cupsTempFd()' - Create a temporary file.
 */

int					/* O - New file descriptor */
cupsTempFd(char *filename,		/* I - Pointer to buffer */
           int  len)			/* I - Size of buffer */
{
  int		fd;			/* File descriptor for temp file */
#ifdef WIN32
  char		tmpdir[1024];		/* Windows temporary directory */
  DWORD		curtime;		/* Current time */
#else
  char		*tmpdir;		/* TMPDIR environment var */
  struct timeval curtime;		/* Current time */
#endif /* WIN32 */
  static char	buf[1024] = "";		/* Buffer if you pass in NULL and 0 */


 /*
  * See if a filename was specified...
  */

  if (filename == NULL)
  {
    filename = buf;
    len      = sizeof(buf);
  }

 /*
  * See if TMPDIR is defined...
  */

#ifdef WIN32
  GetTempPath(sizeof(tmpdir), tmpdir);
#else
  if ((tmpdir = getenv("TMPDIR")) == NULL)
  {
   /*
    * Put root temp files in restricted temp directory...
    */

    if (getuid() == 0)
      tmpdir = CUPS_REQUESTS "/tmp";
    else
      tmpdir = "/var/tmp";
  }
#endif /* WIN32 */

 /*
  * Make the temporary name using the specified directory...
  */

  do
  {
#ifdef WIN32
   /*
    * Get the current time of day...
    */

    curtime = GetTickCount();

   /*
    * Format a string using the hex time values...
    */

    snprintf(filename, len - 1, "%s/%08lx", tmpdir, curtime);
#else
   /*
    * Get the current time of day...
    */

    gettimeofday(&curtime, NULL);

   /*
    * Format a string using the hex time values...
    */

    snprintf(filename, len - 1, "%s/%08lx%05lx", tmpdir,
             curtime.tv_sec, curtime.tv_usec);
#endif /* WIN32 */

   /*
    * Open the file in "exclusive" mode, making sure that we don't
    * stomp on an existing file or someone's symlink crack...
    */

#ifdef O_NOFOLLOW
    fd = open(filename, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
#else
    fd = open(filename, O_RDWR | O_CREAT | O_EXCL, 0600);
#endif /* O_NOFOLLOW */

    if (fd < 0 && (errno == EPERM || errno == ENOENT))
      break; /* Stop immediately if permission denied or the dir doesn't exist! */
  }
  while (fd < 0);

 /*
  * Return the file descriptor...
  */

  return (fd);
}


/*
 * 'cupsTempFile()' - Generate a temporary filename.
 */

char *					/* O - Filename */
cupsTempFile(char *filename,		/* I - Pointer to buffer */
             int  len)			/* I - Size of buffer */
{
  int		fd;			/* File descriptor for temp file */
  static char	buf[1024] = "";		/* Buffer if you pass in NULL and 0 */


 /*
  * See if a filename was specified...
  */

  if (filename == NULL)
  {
    filename = buf;
    len      = sizeof(buf);
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
 * End of "$Id: tempfile.c,v 1.1.2.1 2001/12/26 16:52:13 mike Exp $".
 */
