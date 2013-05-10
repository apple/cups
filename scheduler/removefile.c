/*
 * "$Id$"
 *
 *   "Secure" file removal function for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
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
 *   cupsdRemoveFile() - Remove a file using the 7-pass US DoD method.
 *   overwrite_data()  - Overwrite the data in a file.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#ifdef HAVE_REMOVEFILE
#  include <removefile.h>
#else
static int	overwrite_data(int fd, const char *buffer, int bufsize,
		               int filesize);
#endif /* HAVE_REMOVEFILE */


/*
 * 'cupsdRemoveFile()' - Remove a file using the 7-pass US DoD method.
 */

int					/* O - 0 on success, -1 on error */
cupsdRemoveFile(const char *filename)	/* I - File to remove */
{
#ifdef HAVE_REMOVEFILE
  int			ret;		/* Return value */
  removefile_state_t	s;		/* Remove state variable */


  s   = removefile_state_alloc();
  ret = removefile(filename, s, REMOVEFILE_SECURE_7_PASS);

  removefile_state_free(s);

  return (ret);

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

  unlink(filename);

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

  for (i = 0; i < sizeof(buffer); i ++)
    buffer[i] = rand();
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
    buffer[i] = rand();
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


#ifdef TEST
#  define testmain main
int
testmain(void)
{
  FILE	*fp;


  fp = fopen("testfile.secure", "w");
  fputs("Hello, World!\n", fp);
  fputs("Now is the time for all good men to come to the aid of their "
        "country.\n", fp);
  fclose(fp);

  if (cupsdRemoveFile("testfile.secure"))
  {
    printf("cupsdRemoveFile: FAIL (%s)\n", strerror(errno));
    return (1);
  }
  else
  {
    puts("cupsdRemoveFile: PASS");
    return (0);
  }
}
#endif /* TEST */


/*
 * End of "$Id$".
 */
