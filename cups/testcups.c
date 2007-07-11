/*
 * "$Id$"
 *
 *   CUPS API test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 2007 by Easy Software Products.
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
 *   main() - Main entry.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include "cups.h"


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		status = 0,		/* Exit status */
		num_dests;		/* Number of destinations */
  cups_dest_t	*dests,			/* Destinations */
		*dest;			/* Current destination */
  const char	*ppdfile;		/* PPD file */
  ppd_file_t	*ppd;			/* PPD file data */
  int		num_jobs;		/* Number of jobs for queue */
  cups_job_t	*jobs;			/* Jobs for queue */


 /*
  * cupsGetDests()
  */

  fputs("cupsGetDests: ", stdout);
  fflush(stdout);

  num_dests = cupsGetDests(&dests);

  if (num_dests == 0)
  {
    puts("FAIL");
    return (1);
  }
  else
    puts("PASS");

 /*
  * cupsGetDest(printer)
  */

  printf("cupsGetDest(\"%s\"): ", dests[num_dests / 2].name);
  fflush(stdout);

  if ((dest = cupsGetDest(dests[num_dests / 2].name, NULL, num_dests,
                          dests)) == NULL)
  {
    status = 1;
    puts("FAIL");
  }
  else
    puts("PASS");

 /*
  * cupsGetDest(NULL)
  */

  fputs("cupsGetDest(NULL): ", stdout);
  fflush(stdout);

  if ((dest = cupsGetDest(NULL, NULL, num_dests, dests)) == NULL)
  {
    status = 1;
    puts("FAIL");
  }
  else
    puts("PASS");

 /*
  * cupsPrintFile()
  */

  fputs("cupsPrintFile: ", stdout);
  fflush(stdout);

  if (cupsPrintFile(dest->name, "../data/testprint.ps", "Test Page",
                    dest->num_options, dest->options) <= 0)
  {
    status = 1;
    puts("FAIL");
  }
  else
    puts("PASS");

 /*
  * cupsGetPPD(printer)
  */

  fputs("cupsGetPPD(): ", stdout);
  fflush(stdout);

  if ((ppdfile = cupsGetPPD(dest->name)) == NULL)
  {
    status = 1;
    puts("FAIL");
  }
  else
  {
    puts("PASS");

   /*
    * ppdOpenFile()
    */

    fputs("ppdOpenFile(): ", stdout);
    fflush(stdout);

    if ((ppd = ppdOpenFile(ppdfile)) == NULL)
    {
      puts("FAIL");
      return (1);
    }
    else
      puts("PASS");

    ppdClose(ppd);
    unlink(ppdfile);
  }

 /*
  * cupsGetJobs()
  */

  fputs("cupsGetJobs: ", stdout);
  fflush(stdout);

  num_jobs = cupsGetJobs(&jobs, NULL, 0, -1);

  if (num_jobs == 0)
  {
    puts("FAIL");
    return (1);
  }
  else
    puts("PASS");

  cupsFreeJobs(num_jobs, jobs);
  cupsFreeDests(num_dests, dests);

  return (status);
}


/*
 * End of "$Id: testfile.c 6192 2007-01-10 19:26:48Z mike $".
 */
