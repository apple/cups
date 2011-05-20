/*
 * "$Id$"
 *
 *   CUPS API test program for CUPS.
 *
 *   Copyright 2007-2011 by Apple Inc.
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

#include "string-private.h"
#include "cups.h"
#include "ppd.h"
#include <stdlib.h>


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


  if (argc > 1)
  {
   /*
    * ./testcups printer file interval
    */

    int		interval,		/* Interval between writes */
		job_id;			/* Job ID */
    cups_file_t	*fp;			/* Print file */
    char	buffer[16384];		/* Read/write buffer */
    ssize_t	bytes;			/* Bytes read/written */


    if (argc != 4)
    {
      puts("Usage: ./testcups");
      puts("       ./testcups printer file interval");
      return (1);
    }

    if ((fp = cupsFileOpen(argv[2], "r")) == NULL)
    {
      printf("Unable to open \"%s\": %s\n", argv[2], strerror(errno));
      return (1);
    }

    if ((job_id = cupsCreateJob(CUPS_HTTP_DEFAULT, argv[1], "testcups", 0,
                                NULL)) <= 0)
    {
      printf("Unable to create print job on %s: %s\n", argv[1],
             cupsLastErrorString());
      return (1);
    }

    interval = atoi(argv[3]);

    if (cupsStartDocument(CUPS_HTTP_DEFAULT, argv[1], job_id, argv[2],
                          CUPS_FORMAT_AUTO, 1) != HTTP_CONTINUE)
    {
      puts("Unable to start document!");
      return (1);
    }

    while ((bytes = cupsFileRead(fp, buffer, sizeof(buffer))) > 0)
    {
      printf("Writing %d bytes...\n", (int)bytes);

      if (cupsWriteRequestData(CUPS_HTTP_DEFAULT, buffer,
			       bytes) != HTTP_CONTINUE)
      {
        puts("Unable to write bytes!");
	return (1);
      }

      sleep(interval);
    }

    cupsFileClose(fp);

    if (cupsFinishDocument(CUPS_HTTP_DEFAULT, argv[1]) != HTTP_OK)
    {
      puts("Unable to finish document!");
      return (1);
    }

    return (0);
  }

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
    puts("FAIL");
    return (1);
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

  if (cupsPrintFile(dest->name, "../data/testprint", "Test Page",
                    dest->num_options, dest->options) <= 0)
  {
    printf("FAIL (%s)\n", cupsLastErrorString());
    return (1);
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

  if (!a || !b)
    return (0);

  if (_cups_strcasecmp(a->name, b->name) ||
      (a->instance && !b->instance) ||
      (!a->instance && b->instance) ||
      (a->instance && _cups_strcasecmp(a->instance, b->instance)) ||
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

  if (_cups_strcasecmp(a->name, b->name))
    printf("    name                  %-20.20s  %-20.20s\n", a->name, b->name);

  if ((a->instance && !b->instance) ||
      (!a->instance && b->instance) ||
      (a->instance && _cups_strcasecmp(a->instance, b->instance)))
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
 * End of "$Id$".
 */
