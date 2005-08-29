/*
 * "$Id$"
 *
 *   Public directory routines for the Common UNIX Printing System (CUPS).
 *
 *   This set of APIs abstracts enumeration of directory entries.
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
 *
 * Contents:
 *
 */

/*
 * Include necessary headers...
 */

#include "dir.h"
#include "string.h"
#include <stdlib.h>


/*
 * Windows implementation...
 */

#ifdef WIN32
#  include <windows.h>

/*
 * Directory data structure...
 *
 * @private
 */

struct cups_dir_s
{
  char			directory[1024];/* Directory filename */
  HANDLE		dir;		/* Directory handle */
  cups_dir_entry_t	entry;		/* Directory entry */
};


/*
 * 'cups_dir_time()' - Convert a FILETIME value to a UNIX time value.
 */

time_t					/* O - UNIX time */
cups_dir_time(FILETIME ft)		/* I - File time */
{
  ULONGLONG	val;			/* File time in 0.1 usecs */


 /*
  * Convert file time (1/10 microseconds since Jan 1, 1601) to UNIX
  * time (seconds since Jan 1, 1970).  There are 11,644,732,800 seconds
  * between them...
  */

  val = ft.dwLowDateTime + ft.dwHighDateTime << 32;
  return (val / 10000000 - 11644732800);
}


/*
 * 'cupsDirClose()' - Close a directory.
 */

void
cupsDirClose(cups_dir_t *dp)		/* I - Directory */
{
 /*
  * Range check input...
  */

  if (!directory)
    return;

 /*
  * Close an open directory handle...
  */

  if (dp->dir != INVALID_HANDLE_VALUE)
    FindClose(dp->dir);

 /*
  * Free memory used...
  */

  free(dp);
}


/*
 * 'cupsDirOpen()' - Open a directory.
 */

cups_dir_t *				/* O - Directory */
cupsDirOpen(const char *directory)	/* I - Directory name */
{
  cups_dir_t	*dp;			/* Directory */


 /*
  * Range check input...
  */

  if (!directory)
    return (NULL);

 /*
  * Allocate memory for the directory structure...
  */

  dp = (cups_dir_t *)calloc(1, sizeof(cups_dir_t));
  if (!dp)
    return (NULL);

 /*
  * Copy the directory name for later use...
  */

  dp->dir = INVALID_HANDLE_VALUE;

  strlcpy(dp->directory, directory, sizeof(dp->directory));

 /*
  * Return the new directory structure...
  */

  return (dp);
}


/*
 * 'cupsDirRead()' - Read the next directory entry.
 */

cups_dir_entry_t *			/* O - Directory entry */
cupsDirRead(cups_dir_t *dp)		/* I - Directory */
{
  WIN32_FIND_DATA	entry;		/* Directory entry data */


 /*
  * Range check input...
  */

  if (!dp)
    return (NULL);

 /*
  * See if we have already started finding files...
  */

  if (dp->dir == INVALID_HANDLE_VALUE)
  {
   /*
    * No, find the first file...
    */

    dp->dir = FindFirstFile(dp->directory, &entry);
    if (dp->dir == INVALID_HANDLE_VALUE)
      return (NULL);
  }
  else if (!FindNextFile(dp->dir, &entry))
    return (NULL);

 /*
  * Copy the name over and convert the file information...
  */

  strlcpy(dp->entry.filename, entry.cFileName, sizeof(dp->entry.filename));

  if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    dp->entry.st_mode = 0755 | S_IFDIR;
  else
    dp->entry.st_mode = 0644;

  dp->entry.st_atime = cups_dir_time(entry.ftLastAccessTime);
  dp->entry.st_ctime = cups_dir_time(entry.ftCreationTime);
  dp->entry.st_mtime = cups_dir_time(entry.ftLastWriteTime);
  dp->entry.st_size  = entry.nFileSizeLow + entry.nFileSizeHigh << 32;

 /*
  * Return the entry...
  */

  return (&(dp->entry));
}


/*
 * 'cupsDir()' - .
 */

void
cupsDirRewind(cups_dir_t *dp)		/* I - Directory */
{
 /*
  * Range check input...
  */

  if (!dp)
    return;

 /*
  * Close an open directory handle...
  */

  if (dp->dir != INVALID_HANDLE_VALUE)
  {
    FindClose(dp->dir);
    dp->dir = INVALID_HANDLE_VALUE;
  }
}


#else

/*
 * POSIX implementation...
 */

#  include <sys/types.h>
#  include <dirent.h>

/*
 * Directory data structure...
 *
 * @private
 */

struct cups_dir_s
{
  char			directory[1024];/* Directory filename */
  DIR			*dir;		/* Directory file */
  cups_dir_entry_t	entry;		/* Directory entry */
};


/*
 * 'cupsDirClose()' - Close a directory.
 */

void
cupsDirClose(cups_dir_t *dp)		/* I - Directory */
{
 /*
  * Range check input...
  */

  if (!dp)
    return;

 /*
  * Close the directory and free memory...
  */

  closedir(dp->dir);
  free(dp);
}


/*
 * 'cupsDirOpen()' - Open a directory.
 */

cups_dir_t *				/* O - Directory */
cupsDirOpen(const char *directory)	/* I - Directory name */
{
  cups_dir_t	*dp;			/* Directory */


 /*
  * Range check input...
  */

  if (!directory)
    return (NULL);

 /*
  * Allocate memory for the directory structure...
  */

  dp = (cups_dir_t *)calloc(1, sizeof(cups_dir_t));
  if (!dp)
    return (NULL);

 /*
  * Open the directory...
  */

  dp->dir = opendir(directory);
  if (!dp->dir)
  {
    free(dp);
    return (NULL);
  }

 /*
  * Copy the directory name for later use...
  */

  strlcpy(dp->directory, directory, sizeof(dp->directory));

 /*
  * Return the new directory structure...
  */

  return (dp);
}


/*
 * 'cupsDirRead()' - Read the next directory entry.
 */

cups_dir_entry_t *			/* O - Directory entry */
cupsDirRead(cups_dir_t *dp)		/* I - Directory */
{
  char		buffer[sizeof(struct dirent) + PATH_MAX];
					/* Directory entry buffer */
  struct dirent	*entry;			/* Pointer to entry */
  char		filename[1024];		/* Full filename */


 /*
  * Range check input...
  */

  if (!dp)
    return (NULL);

 /*
  * Try reading an entry that is not "." or ".."...
  */

  do
  {
    if (!readdir_r(dp->dir, (struct dirent *)buffer, &entry))
      return (NULL);
  }
  while (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."));

 /*
  * Copy the name over and get the file information...
  */

  strlcpy(dp->entry.filename, entry->d_name, sizeof(dp->entry.filename));

  snprintf(filename, sizeof(filename), "%s/%s", dp->directory, entry->d_name);
  if (stat(filename, &(dp->entry.fileinfo)))
    return (NULL);

 /*
  * Return the entry...
  */

  return (&(dp->entry));
}


/*
 * 'cupsDirRewind()' - Rewind the directory to the beginning.
 */

void
cupsDirRewind(cups_dir_t *dp)		/* I - Directory */
{
 /*
  * Range check input...
  */

  if (!dp)
    return;

 /*
  * Rewind the directory...
  */

  rewinddir(dp->dir);
}


#endif /* WIN32 */

/*
 * End of "$Id$".
 */
