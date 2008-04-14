/*
 * "$Id$"
 *
 *   PDF to PostScript filter front-end for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   main()       - Main entry for filter...
 *   cancel_job() - Flag the job as canceled.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/string.h>
#include <cups/i18n.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>


/*
 * Local functions...
 */

static void		cancel_job(int sig);


/*
 * Local globals...
 */

static int		job_canceled = 0;


/*
 * 'main()' - Main entry for filter...
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		fd;			/* Copy file descriptor */
  char		*filename,		/* PDF file to convert */
		tempfile[1024];		/* Temporary file */
  char		buffer[8192];		/* Copy buffer */
  int		bytes;			/* Bytes copied */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  const char	*val;			/* Option value */
  int		orientation;		/* Output orientation */
  ppd_file_t	*ppd;			/* PPD file */
  ppd_size_t	*size;			/* Current page size */
  int		pdfpid,			/* Process ID for pdftops */
		pdfwaitpid,		/* Process ID from wait() */
		pdfstatus,		/* Status from pdftops */
		pdfargc;		/* Number of args for pdftops */
  char		*pdfargv[100],		/* Arguments for pdftops/gs */
#ifdef HAVE_PDFTOPS
		pdfwidth[255],		/* Paper width */
		pdfheight[255];		/* Paper height */
#else
		pdfgeometry[255];	/* Paper width and height */
#endif /* HAVE_PDFTOPS */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Make sure status messages are not buffered...
  */

  setbuf(stderr, NULL);

 /*
  * Make sure we have the right number of arguments for CUPS!
  */

  if (argc < 6 || argc > 7)
  {
    _cupsLangPrintf(stderr,
                    _("Usage: %s job user title copies options [filename]\n"),
                    argv[0]);
    return (1);
  }

 /*
  * Register a signal handler to cleanly cancel a job.
  */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGTERM, cancel_job);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  action.sa_handler = cancel_job;
  sigaction(SIGTERM, &action, NULL);
#else
  signal(SIGTERM, cancel_job);
#endif /* HAVE_SIGSET */

 /*
  * Copy stdin if needed...
  */

  if (argc == 6)
  {
   /*
    * Copy stdin to a temp file...
    */

    if ((fd = cupsTempFd(tempfile, sizeof(tempfile))) < 0)
    {
      _cupsLangPrintError(_("ERROR: Unable to copy PDF file"));
      return (1);
    }

    fprintf(stderr, "DEBUG: pdftops - copying to temp print file \"%s\"\n",
            tempfile);

    while ((bytes = fread(buffer, 1, sizeof(buffer), stdin)) > 0)
      write(fd, buffer, bytes);

    close(fd);

    filename = tempfile;
  }
  else
  {
   /*
    * Use the filename on the command-line...
    */

    filename    = argv[6];
    tempfile[0] = '\0';
  }

 /*
  * Load the PPD file and mark options...
  */

  ppd         = ppdOpenFile(getenv("PPD"));
  num_options = cupsParseOptions(argv[5], 0, &options);

  ppdMarkDefaults(ppd);
  cupsMarkOptions(ppd, num_options, options);

 /*
  * Build the command-line for the pdftops or gs filter...
  */

#ifdef HAVE_PDFTOPS
  pdfargv[0] = (char *)"pdftops";
  pdfargc    = 1;
#else
  pdfargv[0] = (char *)"gs";
  pdfargv[1] = (char *)"-q";
  pdfargv[2] = (char *)"-dNOPAUSE";
  pdfargv[3] = (char *)"-dBATCH";
  pdfargv[4] = (char *)"-dSAFER";
  pdfargv[5] = (char *)"-sDEVICE=pswrite";
  pdfargv[6] = (char *)"-sOUTPUTFILE=%stdout";
  pdfargc    = 7;
#endif /* HAVE_PDFTOPS */

  if (ppd)
  {
   /*
    * Set language level and TrueType font handling...
    */

    if (ppd->language_level == 1)
    {
#ifdef HAVE_PDFTOPS
      pdfargv[pdfargc++] = (char *)"-level1";
      pdfargv[pdfargc++] = (char *)"-noembtt";
#else
      pdfargv[pdfargc++] = (char *)"-dLanguageLevel=1";
#endif /* HAVE_PDFTOPS */
    }
    else if (ppd->language_level == 2)
    {
#ifdef HAVE_PDFTOPS
      pdfargv[pdfargc++] = (char *)"-level2";
      if (!ppd->ttrasterizer)
	pdfargv[pdfargc++] = (char *)"-noembtt";
#else
      pdfargv[pdfargc++] = (char *)"-dLanguageLevel=2";
#endif /* HAVE_PDFTOPS */
    }
    else
#ifdef HAVE_PDFTOPS
      pdfargv[pdfargc++] = (char *)"-level3";
#else
      pdfargv[pdfargc++] = (char *)"-dLanguageLevel=3";
#endif /* HAVE_PDFTOPS */

   /*
    * Set output page size...
    */

    size = ppdPageSize(ppd, NULL);
    if (size)
    {
     /*
      * Got the size, now get the orientation...
      */

      orientation = 0;

      if ((val = cupsGetOption("landscape", num_options, options)) != NULL)
      {
	if (strcasecmp(val, "no") != 0 && strcasecmp(val, "off") != 0 &&
	    strcasecmp(val, "false") != 0)
	  orientation = 1;
      }
      else if ((val = cupsGetOption("orientation-requested", num_options, options)) != NULL)
      {
       /*
	* Map IPP orientation values to 0 to 3:
	*
	*   3 = 0 degrees   = 0
	*   4 = 90 degrees  = 1
	*   5 = -90 degrees = 3
	*   6 = 180 degrees = 2
	*/

	orientation = atoi(val) - 3;
	if (orientation >= 2)
	  orientation ^= 1;
      }

#ifdef HAVE_PDFTOPS
      if (orientation & 1)
      {
	snprintf(pdfwidth, sizeof(pdfwidth), "%.0f", size->length);
	snprintf(pdfheight, sizeof(pdfheight), "%.0f", size->width);
      }
      else
      {
	snprintf(pdfwidth, sizeof(pdfwidth), "%.0f", size->width);
	snprintf(pdfheight, sizeof(pdfheight), "%.0f", size->length);
      }

      pdfargv[pdfargc++] = (char *)"-paperw";
      pdfargv[pdfargc++] = pdfwidth;
      pdfargv[pdfargc++] = (char *)"-paperh";
      pdfargv[pdfargc++] = pdfheight;
#else
      if (orientation & 1)
	snprintf(pdfgeometry, sizeof(pdfgeometry), "-g%.0fx%.0f", size->length,
	         size->width);
      else
	snprintf(pdfgeometry, sizeof(pdfgeometry), "-g%.0fx%.0f", size->width,
	         size->length);

      pdfargv[pdfargc++] = pdfgeometry;
#endif /* HAVE_PDFTOPS */
    }
  }

#ifdef HAVE_PDFTOPS
  if ((val = cupsGetOption("fitplot", num_options, options)) != NULL &&
      strcasecmp(val, "no") && strcasecmp(val, "off") &&
      strcasecmp(val, "false"))
    pdfargv[pdfargc++] = (char *)"-expand";

  pdfargv[pdfargc++] = filename;
  pdfargv[pdfargc++] = (char *)"-";
#else
  pdfargv[pdfargc++] = (char *)"-c";
  pdfargv[pdfargc++] = (char *)"save pop";
  pdfargv[pdfargc++] = (char *)"-f";
  pdfargv[pdfargc++] = filename;
#endif /* HAVE_PDFTOPS */

  pdfargv[pdfargc] = NULL;

  if ((pdfpid = fork()) == 0)
  {
   /*
    * Child comes here...
    */

#ifdef HAVE_PDFTOPS
    execv(CUPS_PDFTOPS, pdfargv);
    _cupsLangPrintError(_("ERROR: Unable to execute pdftops program"));
#else
    execv(CUPS_GHOSTSCRIPT, pdfargv);
    _cupsLangPrintError(_("ERROR: Unable to execute gs program"));
#endif /* HAVE_PDFTOPS */

    exit(1);
  }
  else if (pdfpid < 0)
  {
   /*
    * Unable to fork!
    */

#ifdef HAVE_PDFTOPS
    _cupsLangPrintError(_("ERROR: Unable to execute pdftops program"));
#else
    _cupsLangPrintError(_("ERROR: Unable to execute gs program"));
#endif /* HAVE_PDFTOPS */

    pdfstatus = 1;
  }
  else
  {
   /*
    * Parent comes here...
    */

    while ((pdfwaitpid = wait(&pdfstatus)) < 0 && errno == EINTR)
    {
     /*
      * Wait until we get a valid process ID or the job is canceled...
      */

      if (job_canceled)
	break;
    }

    if (pdfwaitpid != pdfpid)
    {
      kill(pdfpid, SIGTERM);
      pdfstatus = 1;
    }
    else if (pdfstatus)
    {
      if (WIFEXITED(pdfstatus))
      {
	pdfstatus = WEXITSTATUS(pdfstatus);

	_cupsLangPrintf(stderr,
			_("ERROR: pdftops filter exited with status %d!\n"),
			pdfstatus);
      }
      else
      {
	pdfstatus = WTERMSIG(pdfstatus);

	_cupsLangPrintf(stderr,
			_("ERROR: pdftops filter crashed on signal %d!\n"),
			pdfstatus);
      }
    }
  }

 /*
  * Cleanup and exit...
  */

  if (tempfile[0])
    unlink(tempfile);

  return (pdfstatus);
}


/*
 * 'cancel_job()' - Flag the job as canceled.
 */

static void
cancel_job(int sig)			/* I - Signal number (unused) */
{
  (void)sig;

  job_canceled = 1;
}


/*
 * End of "$Id$".
 */
