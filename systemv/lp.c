/*
 * "$Id: lp.c,v 1.23 2000/11/10 21:25:51 mike Exp $"
 *
 *   "lp" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products.
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
 *   main()       - Parse options and send files for printing.
 *   sighandler() - Signal catcher for when we print from stdin...
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <cups/cups.h>
#include <cups/string.h>


#ifndef WIN32
#  include <signal.h>


/*
 * Local functions.
 */

void	sighandler(int);
#endif /* !WIN32 */
int	set_job_attrs(int job_id, int num_options, cups_option_t *options);


/*
 * Globals...
 */

char	tempfile[1024];		/* Temporary file for printing from stdin */


/*
 * 'main()' - Parse options and send files for printing.
 */

int
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  int		i, j;		/* Looping vars */
  int		job_id;		/* Job ID */
  char		*printer,	/* Printer name */
		*instance,	/* Instance name */ 
		*val,		/* Option value */
		*title;		/* Job title */
  int		priority;	/* Job priority (1-100) */
  int		num_copies;	/* Number of copies per file */
  int		num_files;	/* Number of files to print */
  const char	*files[1000];	/* Files to print */
  int		num_dests;	/* Number of destinations */
  cups_dest_t	*dests,		/* Destinations */
		*dest;		/* Selected destination */
  int		num_options;	/* Number of options */
  cups_option_t	*options;	/* Options */
  int		silent;		/* Silent or verbose output? */
  char		buffer[8192];	/* Copy buffer */
  FILE		*temp;		/* Temporary file pointer */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;	/* Signal action */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET*/


  silent      = 0;
  printer     = NULL;
  num_dests   = 0;
  dests       = NULL;
  num_options = 0;
  options     = NULL;
  num_files   = 0;
  title       = NULL;
  job_id      = 0;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      switch (argv[i][1])
      {
        case 'c' : /* Copy to spool dir (always enabled) */
	    break;

        case 'd' : /* Destination printer or class */
	    if (argv[i][2] != '\0')
	      printer = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        fputs("lp: Expected destination after -d option!\n", stderr);
		return (1);
              }

	      printer = argv[i];
	    }

            if ((instance = strrchr(printer, '/')) != NULL)
	      *instance++ = '\0';

	    if (num_dests == 0)
	      num_dests = cupsGetDests(&dests);

            if ((dest = cupsGetDest(printer, instance, num_dests, dests)) != NULL)
	    {
	      for (j = 0; j < dest->num_options; j ++)
	        if (cupsGetOption(dest->options[j].name, num_options, options) == NULL)
	          num_options = cupsAddOption(dest->options[j].name,
		                              dest->options[j].value,
					      num_options, &options);
	    }
	    break;

        case 'f' : /* Form */
	    if (!argv[i][2])
	    {
	      i ++;

	      if (i >= argc)
	      {
	        fputs("lp: Expected form after -f option!\n", stderr);
		return (1);
              }
	    }

	    fputs("lp: Warning - form option ignored!\n", stderr);
	    break;

        case 'h' : /* Destination host */
	    if (argv[i][2] != '\0')
	      cupsSetServer(argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        fputs("lp: Expected hostname after -h option!\n", stderr);
		return (1);
              }

	      cupsSetServer(argv[i]);
	    }
	    break;

        case 'i' : /* Change job */
	    if (argv[i][2])
	      val = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        fputs("lp: Expected job ID after -i option!\n", stderr);
		return (1);
              }

	      val = argv[i];
	    }

            if (num_files > 0)
	    {
	      fputs("lp: Error - cannot print files and alter jobs simultaneously!\n", stderr);
	      return (1);
	    }

            if (strrchr(val, '-') != NULL)
	      job_id = atoi(strrchr(val, '-') + 1);
	    else
	      job_id = atoi(val);

            if (job_id < 0)
	    {
	      fputs("lp: Error - bad job ID!\n", stderr);
	      break;
	    }
	    break;

	case 'm' : /* Send email when job is done */
#ifdef __sun
	case 'p' : /* Notify on completion */
#endif /* __sun */
	case 'w' : /* Write to console or email */
	    break;

	case 'n' : /* Number of copies */
	    if (argv[i][2] != '\0')
	      num_copies = atoi(argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        fputs("lp: Expected copies after -n option!\n", stderr);
		return (1);
              }

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

	      if (i >= argc)
	      {
	        fputs("lp: Expected option string after -o option!\n", stderr);
		return (1);
              }

	      num_options = cupsParseOptions(argv[i], num_options, &options);
	    }
	    break;

#ifndef __sun
	case 'p' : /* Queue priority */
#endif /* !__sun */
	case 'q' : /* Queue priority */
	    if (argv[i][2] != '\0')
	      priority = atoi(argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        fprintf(stderr, "lp: Expected priority after -%c option!\n",
		        argv[i][1]);
		return (1);
              }

	      priority = atoi(argv[i]);
	    }

           /*
	    * For 100% Solaris compatibility, need to add:
	    *
	    *   priority = 99 * (39 - priority) / 39 + 1;
	    *
	    * However, to keep CUPS lp the same across all platforms
	    * we will break compatibility this far...
	    */

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

	      if (i >= argc)
	      {
	        fputs("lp: Expected title after -t option!\n", stderr);
		return (1);
              }

	      title = argv[i];
	    }
	    break;

        case 'y' : /* mode-list */
	    if (!argv[i][2])
	    {
	      i ++;

	      if (i >= argc)
	      {
	        fputs("lp: Expected mode list after -y option!\n", stderr);
		return (1);
              }
	    }

	    fputs("lp: Warning - mode option ignored!\n", stderr);
	    break;

        case 'H' : /* Hold job */
	    if (argv[i][2])
	      val = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        fputs("lp: Expected hold name after -H option!\n", stderr);
		return (1);
              }

	      val = argv[i];
	    }

	    if (strcmp(val, "hold") == 0)
              num_options = cupsAddOption("job-hold-until", "indefinite",
	                                  num_options, &options);
	    if (strcmp(val, "resume") == 0)
              num_options = cupsAddOption("job-hold-until", "none",
	                                  num_options, &options);
	    else if (strcmp(val, "immediate") == 0)
              num_options = cupsAddOption("job-priority", "100",
	                                  num_options, &options);
	    else
              num_options = cupsAddOption("job-hold-until", val,
	                                  num_options, &options);
	    break;

        case 'P' : /* Page list */
	    if (argv[i][2])
	      val = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        fputs("lp: Expected page list after -P option!\n", stderr);
		return (1);
              }

	      val = argv[i];
	    }

            num_options = cupsAddOption("page-ranges", val, num_options,
	                                &options);
            break;

        case 'S' : /* character set */
	    if (!argv[i][2])
	    {
	      i ++;

	      if (i >= argc)
	      {
	        fputs("lp: Expected character set after -S option!\n", stderr);
		return (1);
              }
	    }

	    fputs("lp: Warning - character set option ignored!\n", stderr);
	    break;

        case 'T' : /* Content-Type */
	    if (!argv[i][2])
	    {
	      i ++;

	      if (i >= argc)
	      {
	        fputs("lp: Expected content type after -T option!\n", stderr);
		return (1);
              }
	    }

	    fputs("lp: Warning - content type option ignored!\n", stderr);
	    break;

	default :
	    fprintf(stderr, "lp: Unknown option \'%c\'!\n", argv[i][1]);
	    return (1);
      }
    else if (num_files < 1000 && job_id == 0)
    {
     /*
      * Print a file...
      */

      files[num_files] = argv[i];
      num_files ++;

      if (title == NULL)
      {
        if ((title = strrchr(argv[i], '/')) != NULL)
	  title ++;
	else
          title = argv[i];
      }
    }
    else
      fprintf(stderr, "lp: Too many files - \"%s\"\n", argv[i]);

 /*
  * See if we are altering an existing job...
  */

  if (job_id)
    return (set_job_attrs(job_id, num_options, options));

 /*
  * See if we have any files to print; if not, print from stdin...
  */

  if (printer == NULL)
  {
    if (num_dests == 0)
      num_dests = cupsGetDests(&dests);

    for (j = 0, dest = dests; j < num_dests; j ++, dest ++)
      if (dest->is_default)
      {
	printer = dests[j].name;

	for (j = 0; j < dest->num_options; j ++)
	  if (cupsGetOption(dest->options[j].name, num_options, options) == NULL)
	    num_options = cupsAddOption(dest->options[j].name,
		                        dest->options[j].value,
					num_options, &options);
        break;
      }
  }

  if (printer == NULL)
  {
    fputs("lp: error - no default destination available.\n", stderr);
    return (1);
  }

  if (num_files > 0)
    job_id = cupsPrintFiles(printer, num_files, files, title, num_options, options);
  else
  {
    num_files = 1;

#ifndef WIN32
#  if defined(HAVE_SIGSET)
    sigset(SIGHUP, sighandler);
    sigset(SIGINT, sighandler);
    sigset(SIGTERM, sighandler);
#  elif defined(HAVE_SIGACTION)
    memset(&action, 0, sizeof(action));
    action.sa_handler = sighandler;

    sigaction(SIGHUP, &action, NULL);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
#  else
    signal(SIGHUP, sighandler);
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
#  endif
#endif /* !WIN32 */

    temp = fopen(cupsTempFile(tempfile, sizeof(tempfile)), "w");

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
      job_id = cupsPrintFile(printer, tempfile, title, num_options, options);
    else
      job_id = cupsPrintFile(printer, tempfile, "(stdin)", num_options, options);

    unlink(tempfile);
  }

  if (job_id < 1)
  {
    fprintf(stderr, "lp: unable to print file: %s\n",
	    ippErrorString(cupsLastError()));
    return (1);
  }
  else if (!silent)
    fprintf(stderr, "request id is %s-%d (%d file(s))\n", printer, job_id,
            num_files);

  return (0);
}


/*
 * 'set_job_attrs()' - Set job attributes.
 */

int					/* O - Exit status */
set_job_attrs(int           job_id,	/* I - Job ID */
              int           num_options,/* I - Number of options */
	      cups_option_t *options)	/* I - Options */
{
  http_t	*http;			/* HTTP connection to server */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  cups_lang_t	*language;		/* Language for request */
  char		uri[HTTP_MAX_URI];	/* URI for job */


  http = httpConnect(cupsServer(), ippPort());

}


#ifndef WIN32
/*
 * 'sighandler()' - Signal catcher for when we print from stdin...
 */

void
sighandler(int s)	/* I - Signal number */
{
 /*
  * Remove the temporary file we're using to print from stdin...
  */

  unlink(tempfile);

 /*
  * Exit...
  */

  exit(s);
}
#endif /* !WIN32 */


/*
 * End of "$Id: lp.c,v 1.23 2000/11/10 21:25:51 mike Exp $".
 */
