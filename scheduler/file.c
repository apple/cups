/*
 * "$Id$"
 *
 *   File functions for the CUPS scheduler.
 *
 *   Copyright 2007-2011 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   "LICENSE" which should have been included with this file.  If this
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cupsdCleanFiles()           - Clean out old files.
 *   cupsdCloseCreatedConfFile() - Close a created configuration file and move
 *                                 into place.
 *   cupsdClosePipe()            - Close a pipe as necessary.
 *   cupsdCreateConfFile()       - Create a configuration file safely.
 *   cupsdOpenConfFile()         - Open a configuration file.
 *   cupsdOpenPipe()             - Create a pipe which is closed on exec.
 *   cupsdRemoveFile()           - Remove a file using the 7-pass US DoD method.
 *   overwrite_data()            - Overwrite the data in a file.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <cups/dir.h>
#include <fnmatch.h>
#ifdef HAVE_REMOVEFILE
#  include <removefile.h>
#else
static int	overwrite_data(int fd, const char *buffer, int bufsize,
		               int filesize);
#endif /* HAVE_REMOVEFILE */


/*
 * 'cupsdCleanFiles()' - Clean out old files.
 */
 
void
cupsdCleanFiles(const char *path,	/* I - Directory to clean */
                const char *pattern)	/* I - Filename pattern or NULL */
{
  cups_dir_t	*dir;			/* Directory */
  cups_dentry_t	*dent;			/* Directory entry */
  char		filename[1024];		/* Filename */
  int		status;			/* Status from unlink/rmdir */


  cupsdLogMessage(CUPSD_LOG_DEBUG,
                  "cupsdCleanFiles(path=\"%s\", pattern=\"%s\")", path,
		  pattern ? pattern : "(null)");

  if ((dir = cupsDirOpen(path)) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to open directory \"%s\" - %s",
		    path, strerror(errno));
    return;
  }

  cupsdLogMessage(CUPSD_LOG_INFO, "Cleaning out old files in \"%s\"...", path);

  while ((dent = cupsDirRead(dir)) != NULL)
  {
    if (pattern && fnmatch(pattern, dent->filename, 0))
      continue;

    snprintf(filename, sizeof(filename), "%s/%s", path, dent->filename);

    if (S_ISDIR(dent->fileinfo.st_mode))
    {
      cupsdCleanFiles(filename, pattern);

      status = rmdir(filename);
    }
    else
      status = unlink(filename);

    if (status)
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to remove \"%s\" - %s", filename,
		      strerror(errno));
    else
      cupsdLogMessage(CUPSD_LOG_DEBUG, "Removed \"%s\"...", filename);
  }

  cupsDirClose(dir);
}


/*
 * 'cupsdCloseCreatedConfFile()' - Close a created configuration file and move
 *                                 into place.
 */

int					/* O - 0 on success, -1 on error */
cupsdCloseCreatedConfFile(
    cups_file_t *fp,			/* I - File to close */
    const char  *filename)		/* I - Filename */
{
  char	newfile[1024],			/* filename.N */
	oldfile[1024];			/* filename.O */


 /*
  * First close the file...
  */

  if (cupsFileClose(fp))
    return (-1);

 /*
  * Then remove "filename.O", rename "filename" to "filename.O", and rename
  * "filename.N" to "filename".
  */

  snprintf(newfile, sizeof(newfile), "%s.N", filename);
  snprintf(oldfile, sizeof(oldfile), "%s.O", filename);

  if ((cupsdRemoveFile(oldfile) && errno != ENOENT) ||
      (rename(filename, oldfile) && errno != ENOENT) ||
      rename(newfile, filename))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to finalize \"%s\": %s",
                    filename, strerror(errno));
    return (-1);
  }

  return (0);
}


/*
 * 'cupsdClosePipe()' - Close a pipe as necessary.
 */

void
cupsdClosePipe(int *fds)		/* I - Pipe file descriptors (2) */
{
 /*
  * Close file descriptors as needed...
  */

  if (fds[0] >= 0)
  {
    close(fds[0]);
    fds[0] = -1;
  }

  if (fds[1] >= 0)
  {
    close(fds[1]);
    fds[1] = -1;
  }
}


/*
 * 'cupsdCreateConfFile()' - Create a configuration file safely.
 */

cups_file_t *				/* O - File pointer */
cupsdCreateConfFile(
    const char *filename,		/* I - Filename */
    mode_t     mode)			/* I - Permissions */
{
  cups_file_t	*fp;			/* File pointer */
  char		newfile[1024];		/* filename.N */


  snprintf(newfile, sizeof(newfile), "%s.N", filename);
  if ((fp = cupsFileOpen(newfile, "w")) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to create \"%s\": %s", newfile,
		    strerror(errno));
  }
  else
  {
    if (!getuid() && fchown(cupsFileNumber(fp), getuid(), Group))
      cupsdLogMessage(CUPSD_LOG_WARN, "Unable to change group for \"%s\": %s",
		      newfile, strerror(errno));

    if (fchmod(cupsFileNumber(fp), mode))
      cupsdLogMessage(CUPSD_LOG_WARN,
                      "Unable to change permissions for \"%s\": %s",
		      newfile, strerror(errno));
  }

  return (fp);
}


/*
 * 'cupsdOpenConfFile()' - Open a configuration file.
 *
 * This function looks for "filename.O" if "filename" does not exist and does
 * a rename as needed.
 */

cups_file_t *				/* O - File pointer */
cupsdOpenConfFile(const char *filename)	/* I - Filename */
{
  cups_file_t	*fp;			/* File pointer */


  if ((fp = cupsFileOpen(filename, "r")) == NULL)
  {
    if (errno == ENOENT)
    {
     /*
      * Try opening the backup file...
      */

      char	oldfile[1024];		/* filename.O */

      snprintf(oldfile, sizeof(oldfile), "%s.O", filename);
      fp = cupsFileOpen(oldfile, "r");
    }
    else
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to open \"%s\": %s", filename,
		      strerror(errno));
  }

  return (fp);
}


/*
 * 'cupsdOpenPipe()' - Create a pipe which is closed on exec.
 */

int					/* O - 0 on success, -1 on error */
cupsdOpenPipe(int *fds)			/* O - Pipe file descriptors (2) */
{
 /*
  * Create the pipe...
  */

  if (pipe(fds))
  {
    fds[0] = -1;
    fds[1] = -1;

    return (-1);
  }

 /*
  * Set the "close on exec" flag on each end of the pipe...
  */

  if (fcntl(fds[0], F_SETFD, fcntl(fds[0], F_GETFD) | FD_CLOEXEC))
  {
    close(fds[0]);
    close(fds[1]);

    fds[0] = -1;
    fds[1] = -1;

    return (-1);
  }

  if (fcntl(fds[1], F_SETFD, fcntl(fds[1], F_GETFD) | FD_CLOEXEC))
  {
    close(fds[0]);
    close(fds[1]);

    fds[0] = -1;
    fds[1] = -1;

    return (-1);
  }

 /*
  * Return 0 indicating success...
  */

  return (0);
}


/*
 * 'cupsdRemoveFile()' - Remove a file using the 7-pass US DoD method.
 */

int					/* O - 0 on success, -1 on error */
cupsdRemoveFile(const char *filename)	/* I - File to remove */
{
#ifdef HAVE_REMOVEFILE
  return (removefile(filename, NULL, REMOVEFILE_SECURE_7_PASS));

#else
  int			fd;		/* File descriptor */
  struct stat		info;		/* File information */
  char			buffer[512];	/* Data buffer */
  int			i;		/* Looping var */


 /*
  * First open the file for writing in exclusive mode.
  */

  if ((fd = open(filename, O_WRONLY | O_EXCL)) < 0)
    return (-1);

 /*
  * Delete the file now - it will still be around as long as the file is
  * open...
  */

  if (unlink(filename))
  {
    close(fd);
    return (-1);
  }

 /*
  * Then get the file size...
  */

  if (fstat(fd, &info))
  {
    close(fd);
    return (-1);
  }

 /*
  * Overwrite the file 7 times with 0xF6, 0x00, 0xFF, random, 0x00, 0xFF,
  * and more random data.
  */

  memset(buffer, 0xF6, sizeof(buffer));
  if (overwrite_data(fd, buffer, sizeof(buffer), (int)info.st_size))
  {
    close(fd);
    return (-1);
  }

  memset(buffer, 0x00, sizeof(buffer));
  if (overwrite_data(fd, buffer, sizeof(buffer), (int)info.st_size))
  {
    close(fd);
    return (-1);
  }

  memset(buffer, 0xFF, sizeof(buffer));
  if (overwrite_data(fd, buffer, sizeof(buffer), (int)info.st_size))
  {
    close(fd);
    return (-1);
  }

  CUPS_SRAND(time(NULL));

  for (i = 0; i < sizeof(buffer); i ++)
    buffer[i] = CUPS_RAND();
  if (overwrite_data(fd, buffer, sizeof(buffer), (int)info.st_size))
  {
    close(fd);
    return (-1);
  }

  memset(buffer, 0x00, sizeof(buffer));
  if (overwrite_data(fd, buffer, sizeof(buffer), (int)info.st_size))
  {
    close(fd);
    return (-1);
  }

  memset(buffer, 0xFF, sizeof(buffer));
  if (overwrite_data(fd, buffer, sizeof(buffer), (int)info.st_size))
  {
    close(fd);
    return (-1);
  }

  for (i = 0; i < sizeof(buffer); i ++)
    buffer[i] = CUPS_RAND();
  if (overwrite_data(fd, buffer, sizeof(buffer), (int)info.st_size))
  {
    close(fd);
    return (-1);
  }

 /*
  * Whew!  Close the file (which will lead to the actual deletion) and
  * return success...
  */

  close(fd);
  return (0);
#endif /* HAVE_REMOVEFILE */
}


#ifndef HAVE_REMOVEFILE
/*
 * 'overwrite_data()' - Overwrite the data in a file.
 */

static int				/* O - 0 on success, -1 on error */
overwrite_data(int        fd,		/* I - File descriptor */
               const char *buffer,	/* I - Buffer to write */
	       int        bufsize,	/* I - Size of buffer */
               int        filesize)	/* I - Size of file */
{
  int	bytes;				/* Bytes to write/written */


 /*
  * Start at the beginning of the file...
  */

  if (lseek(fd, 0, SEEK_SET) < 0)
    return (-1);

 /*
  * Fill the file with the provided data...
  */

  while (filesize > 0)
  {
    if (filesize > bufsize)
      bytes = bufsize;
    else
      bytes = filesize;

    if ((bytes = write(fd, buffer, bytes)) < 0)
      return (-1);

    filesize -= bytes;
  }

 /*
  * Force the changes to disk...
  */

  return (fsync(fd));
}
#endif /* HAVE_REMOVEFILE */


/*
 * End of "$Id$".
 */
