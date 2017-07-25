#include <stdio.h>
#include "cups.h"
#include <sys/time.h>


int main(void)
{
  int num_dests;
  cups_dest_t *dests;
  struct timeval start, end;
  double secs;

  do
  {
    gettimeofday(&start, NULL);
    num_dests = cupsGetDests(&dests);
    gettimeofday(&end, NULL);
    secs = end.tv_sec - start.tv_sec + 0.000001 * (end.tv_usec - start.tv_usec);
    printf("Found %d printers in %.3f seconds...\n", num_dests, secs);
    if (num_dests > 0)
    {
      cupsFreeDests(num_dests, dests);
      sleep(1);
    }
  }
  while (num_dests > 0);

  return (0);
}
