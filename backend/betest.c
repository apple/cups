/*
 * "$Id$"
 *
 *   Backend test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
 *   main() - Run the named backend.
 */

/*
 * Include necessary headers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <cups/string.h>
#include <unistd.h>


/*
 * 'main()' - Run the named backend.
 *
 * Usage:
 *
 *    betest device-uri job-id user title copies options [file]
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments (7 or 8) */
     char *argv[])	/* I - Command-line arguments */
{
  char		backend[255];	/* Method in URI */


  if (argc < 7 || argc > 8)
  {
    fputs("Usage: betest device-uri job-id user title copies options [file]\n",
          stderr);
    return (1);
  }

 /*
  * Extract the method from the device-uri - that's the program we want to
  * execute.
  */

  if (sscanf(argv[1], "%254[^:]", backend) != 1)
  {
    fputs("betest: Bad device-uri - no colon!\n", stderr);
    return (1);
  }

 /*
  * Execute and return
  */

  execl(backend, argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7],
        NULL);

  return (1);
}


/*
 * End of "$Id$".
 */
