/*
 * "$Id$"
 *
 *   IEEE-1284 support functions test program for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE" which should have been included with this file.  If this
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 */

/*
 * Include necessary headers.
 */

#include <cups/string.h>
#include <stdlib.h>
#include <errno.h>
#ifdef WIN32
#  include <io.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#endif /* WIN32 */
#define DEBUG
#include "ieee1284.c"


/*
 * 'main()' - Test the device-ID functions.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int	i,				/* Looping var */
	fd;				/* File descriptor */
  char	device_id[1024],		/* 1284 device ID string */
	make_model[1024],		/* make-and-model string */
	uri[1024];			/* URI string */


  if (argc < 2)
  {
    puts("Usage: test1284 device-file [... device-file-N]");
    exit(1);
  }

  for (i = 1; i < argc; i ++)
  {
    if ((fd = open(argv[i], O_RDWR)) < 0)
    {
      perror(argv[i]);
      return (errno);
    }

    printf("%s:\n", argv[i]);

    get_device_id(fd, device_id, sizeof(device_id), make_model,
                  sizeof(make_model), "test", uri, sizeof(uri));

    printf("    device_id=\"%s\"\n", device_id);
    printf("    make_model=\"%s\"\n", make_model);
    printf("    uri=\"%s\"\n", uri);

    close(fd);
  }

  return (0);
}


/*
 * End of "$Id$".
 */
