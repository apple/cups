/*
 * "$Id: lp.c,v 1.6 1999/05/10 21:36:09 mike Exp $"
 *
 *   "lp" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
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
 * Contents:
 *
 *   main() - Parse options and send files for printing.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <cups/cups.h>


/*
 * 'main()' - Parse options and send files for printing.
 */

int
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  int		i;		/* Looping var */
  int		job_id;		/* Job ID */
  char		*dest;		/* Destination printer */
  char		*title;		/* Job title */
  int		priority;	/* Job priority (1-100) */
  int		num_copies;	/* Number of copies per file */
  int		num_files;	/* Number of files printed */
  int		num_options;	/* Number of options */
  cups_option_t	*options;	/* Options */
  int		silent;		/* Silent or verbose output? */
  char		tempfile[1024];	/* Temporary file for printing from stdin */
  char		buffer[8192];	/* Copy buffer */
  FILE		*temp;		/* Temporary file pointer */


  silent      = 0;
  dest        = cupsGetDefault();
  num_options = 0;
  options     = NULL;
  num_files   = 0;
  title       = NULL;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      switch (argv[i][1])
      {
        case 'c' : /* Copy to spool dir (always enabled) */
	    break;

        case 'd' : /* Destination printer or class */
	    if (argv[i][2] != '\0')
	      dest = argv[i] + 2;
	    else
	    {
	      i ++;
	      dest = argv[i];
	    }
	    break;

	case 'm' : /* Send email when job is done */
	case 'w' : /* Write to console or email */
	    break;

	case 'n' : /* Number of copies */
	    if (argv[i][2] != '\0')
	      num_copies = atoi(argv[i] + 2);
	    else
	    {
	      i ++;
	      num_copies = atoi(argv[i]);
	    }

	    if (num_copies < 1 || num_copies > 100)
	    {
	      fputs("lp: Number copies must be between 1 and 100.\n", stderr);
	      return (1);
	    }

            sprintf(buffer, "%d", num_copies);
            num_options = cupsAddOption("copies", buffer, num_options, &options);
	    break;

	case 'o' : /* Option */
	    if (argv[i][2] != '\0')
	      num_options = cupsParseOptions(argv[i] + 2, num_options, &options);
	    else
	    {
	      i ++;
	      num_options = cupsParseOptions(argv[i], num_options, &options);
	    }
	    break;

	case 'q' : /* Queue priority */
	    if (argv[i][2] != '\0')
	      priority = atoi(argv[i] + 2);
	    else
	    {
	      i ++;
	      priority = atoi(argv[i]);
	    }

	    if (priority < 1 || priority > 100)
	    {
	      fputs("lp: Priority must be between 1 and 100.\n", stderr);
	      return (1);
	    }

            sprintf(buffer, "%d", priority);
            num_options = cupsAddOption("job-priority", buffer, num_options, &options);
	    break;

	case 's' : /* Silent */
	    silent = 1;
	    break;

	case 't' : /* Title */
	    if (argv[i][2] != '\0')
	      title = argv[i] + 2;
	    else
	    {
	      i ++;
	      title = argv[i];
	    }
	    break;

	default :
	    fprintf(stderr, "lp: Unknown option \'%c\'!\n", argv[i][1]);
	    return (1);
      }
    else
    {
     /*
      * Print a file...
      */

      if (dest == NULL)
      {
	fputs("lp: error - no default destination available.\n", stderr);
	return (1);
      }

      num_files ++;
      if (title)
        job_id = cupsPrintFile(dest, argv[i], title, num_options, options);
      else
      {
        char *filename;

        if ((filename = strrchr(argv[i], '/')) != NULL)
	  filename ++;
	else
	  filename = argv[i];

        job_id = cupsPrintFile(dest, argv[i], filename, num_options, options);
      }

      if (job_id < 1)
      {
	fprintf(stderr, "lp: unable to print file \'%s\'.\n", argv[i]);
	return (1);
      }
      else if (!silent)
	fprintf(stderr, "request id is %s-%d (1 file(s))\n", dest, job_id);
    }

 /*
  * See if we printed anything; if not, print from stdin...
  */

  if (num_files == 0)
  {
    if (dest == NULL)
    {
      fputs("lp: error - no default destination available.\n", stderr);
      return (1);
    }

    temp = fopen(tmpnam(tempfile), "w");

    if (temp == NULL)
    {
      fputs("lp: unable to create temporary file.\n", stderr);
      return (1);
    }

    while ((i = fread(buffer, 1, sizeof(buffer), stdin)) > 0)
      fwrite(buffer, 1, i, temp);

    i = ftell(temp);
    fclose(temp);

    if (i == 0)
    {
      fputs("lp: stdin is empty, so no job has been sent.\n", stderr);
      return (1);
    }

    if (title)
      job_id = cupsPrintFile(dest, tempfile, title, num_options, options);
    else
      job_id = cupsPrintFile(dest, tempfile, "(stdin)", num_options, options);

    if (job_id < 1)
    {
      fprintf(stderr, "lp: unable to print file \'%s\'.\n", argv[i]);
      return (1);
    }
    else if (!silent)
      fprintf(stderr, "request id is %s-%d (1 file(s))\n", dest, job_id);
  }

  return (0);
}


/*
 * End of "$Id: lp.c,v 1.6 1999/05/10 21:36:09 mike Exp $".
 */
