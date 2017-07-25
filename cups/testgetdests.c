/*
 * CUPS cupsGetDests API test program for CUPS.
 *
 * Copyright 2017 by Apple Inc.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include "cups.h"
#include <sys/time.h>


/*
 * 'main()' - Loop calling cupsGetDests.
 */

int                                     /* O - Exit status */
main(void)
{
  int           num_dests;              /* Number of destinations */
  cups_dest_t   *dests;                 /* Destinations */
  struct timeval start, end;            /* Start and stop time */
  double        secs;                   /* Total seconds to run cupsGetDests */


  for (;;)
  {
    gettimeofday(&start, NULL);
    num_dests = cupsGetDests(&dests);
    gettimeofday(&end, NULL);
    secs = end.tv_sec - start.tv_sec + 0.000001 * (end.tv_usec - start.tv_usec);

    printf("Found %d printers in %.3f seconds...\n", num_dests, secs);

    cupsFreeDests(num_dests, dests);
    sleep(1);
  }

  return (0);
}
