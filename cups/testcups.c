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
 *   main()        - Main entry.
 *   dests_equal() - Determine whether two destinations are equal.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include "cups.h"


/*
 * Local functions...
 */

static int	dests_equal(cups_dest_t *a, cups_dest_t *b);
static void	show_diffs(cups_dest_t *a, cups_dest_t *b);


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		status = 0,		/* Exit status */
		i,			/* Looping var */
		num_dests;		/* Number of destinations */
  cups_dest_t	*dests,			/* Destinations */
		*dest,			/* Current destination */
		*named_dest;		/* Current named destination */
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
  {
    printf("PASS (%d dests)\n", num_dests);

    for (i = num_dests, dest = dests; i > 0; i --, dest ++)
    {
      printf("    %s", dest->name);

      if (dest->instance)
        printf("    /%s", dest->instance);

      if (dest->is_default)
        puts(" ***DEFAULT***");
      else
        putchar('\n');
    }
  }

 /*
  * cupsGetDest(NULL)
  */

  fputs("cupsGetDest(NULL): ", stdout);
  fflush(stdout);

  if ((dest = cupsGetDest(NULL, NULL, num_dests, dests)) == NULL)
  {
    for (i = num_dests, dest = dests; i > 0; i --, dest ++)
      if (dest->is_default)
        break;

    if (i)
    {
      status = 1;
      puts("FAIL");
    }
    else
      puts("PASS (no default)");

    dest = NULL;
  }
  else
    printf("PASS (%s)\n", dest->name);

 /*
  * cupsGetNamedDest(NULL, NULL, NULL)
  */

  fputs("cupsGetNamedDest(NULL, NULL, NULL): ", stdout);
  fflush(stdout);

  if ((named_dest = cupsGetNamedDest(NULL, NULL, NULL)) == NULL ||
      !dests_equal(dest, named_dest))
  {
    if (!dest)
      puts("PASS (no default)");
    else if (named_dest)
    {
      puts("FAIL (different values)");
      show_diffs(dest, named_dest);
      status = 1;
    }
    else
    {
      puts("FAIL (no default)");
      status = 1;
    }
  }
  else
    printf("PASS (%s)\n", named_dest->name);

  if (named_dest)
    cupsFreeDests(1, named_dest);

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
  * cupsGetNamedDest(NULL, printer, instance)
  */

  printf("cupsGetNamedDest(NULL, \"%s\", \"%s\"): ", dest->name,
         dest->instance ? dest->instance : "(null)");
  fflush(stdout);

  if ((named_dest = cupsGetNamedDest(NULL, dest->name,
                                     dest->instance)) == NULL ||
      !dests_equal(dest, named_dest))
  {
    if (named_dest)
    {
      puts("FAIL (different values)");
      show_diffs(dest, named_dest);
    }
    else
      puts("FAIL (no destination)");


    status = 1;
  }
  else
    puts("PASS");

  if (named_dest)
    cupsFreeDests(1, named_dest);

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
 * 'dests_equal()' - Determine whether two destinations are equal.
 */

static int				/* O - 1 if equal, 0 if not equal */
dests_equal(cups_dest_t *a,		/* I - First destination */
            cups_dest_t *b)		/* I - Second destination */
{
  int		i;			/* Looping var */
  cups_option_t	*aoption;		/* Current option */
  const char	*bval;			/* Option value */


  if (a == b)
    return (1);

  if ((!a && b) || (a && !b))
    return (0);

  if (strcasecmp(a->name, b->name) ||
      (a->instance && !b->instance) ||
      (!a->instance && b->instance) ||
      (a->instance && strcasecmp(a->instance, b->instance)) ||
      a->num_options != b->num_options)
    return (0);

  for (i = a->num_options, aoption = a->options; i > 0; i --, aoption ++)
    if ((bval = cupsGetOption(aoption->name, b->num_options,
                              b->options)) == NULL ||
        strcmp(aoption->value, bval))
      return (0);

  return (1);
}


/*
 * 'show_diffs()' - Show differences between two destinations.
 */

static void
show_diffs(cups_dest_t *a,		/* I - First destination */
           cups_dest_t *b)		/* I - Second destination */
{
  int		i;			/* Looping var */
  cups_option_t	*aoption;		/* Current option */
  const char	*bval;			/* Option value */


  if (!a || !b)
    return;

  puts("    Item                  cupsGetDest           cupsGetNamedDest");
  puts("    --------------------  --------------------  --------------------");

  if (strcasecmp(a->name, b->name))
    printf("    name                  %-20.20s  %-20.20s\n", a->name, b->name);

  if ((a->instance && !b->instance) ||
      (!a->instance && b->instance) ||
      (a->instance && strcasecmp(a->instance, b->instance)))
    printf("    instance              %-20.20s  %-20.20s\n",
           a->instance ? a->instance : "(null)",
	   b->instance ? b->instance : "(null)");

  if (a->num_options != b->num_options)
    printf("    num_options           %-20d  %-20d\n", a->num_options,
           b->num_options);

  for (i = a->num_options, aoption = a->options; i > 0; i --, aoption ++)
    if ((bval = cupsGetOption(aoption->name, b->num_options,
                              b->options)) == NULL ||
        strcmp(aoption->value, bval))
      printf("    %-20.20s  %-20.20s  %-20.20s\n", aoption->name,
             aoption->value, bval ? bval : "(null)");
}


/*
 * End of "$Id: testfile.c 6192 2007-01-10 19:26:48Z mike $".
 */
