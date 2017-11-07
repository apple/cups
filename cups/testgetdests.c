/*
 * CUPS cupsGetDests API test program for CUPS.
 *
 * Copyright 2017 by Apple Inc.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
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
