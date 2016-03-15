/*
 * "$Id: test1284.c 10996 2013-05-29 11:51:34Z msweet $"
 *
 *   IEEE-1284 support functions test program for CUPS.
 *
 *   Copyright 2007-2010 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   "LICENSE" which should have been included with this file.  If this
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main() - Test the device-ID functions.
 */

/*
 * Include necessary headers.
 */

#include <cups/string-private.h>
#ifdef WIN32
#  include <io.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#endif /* WIN32 */

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

    backendGetDeviceID(fd, device_id, sizeof(device_id), make_model,
                       sizeof(make_model), "test", uri, sizeof(uri));

    printf("    device_id=\"%s\"\n", device_id);
    printf("    make_model=\"%s\"\n", make_model);
    printf("    uri=\"%s\"\n", uri);

    close(fd);
  }

  return (0);
}


/*
 * End of "$Id: test1284.c 10996 2013-05-29 11:51:34Z msweet $".
 */
