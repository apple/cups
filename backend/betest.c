/*
 * "$Id: betest.c,v 1.3.2.3 2003/01/07 18:26:14 mike Exp $"
 *
 *   Backend test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
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
 * End of "$Id: betest.c,v 1.3.2.3 2003/01/07 18:26:14 mike Exp $".
 */
