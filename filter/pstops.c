/*
 * "$Id: pstops.c 12655 2015-05-22 17:26:40Z msweet $"
 *
 * PostScript filter for CUPS.
 *
 * Copyright 2007-2015 by Apple Inc.
 * Copyright 1993-2007 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include "common.h"
#include <limits.h>
#include <math.h>
#include <cups/file.h>
#include <cups/array.h>
#include <cups/language-private.h>
#include <signal.h>


/*
 * Constants...
 */

#define PSTOPS_BORDERNONE	0	/* No border or hairline border */
#define PSTOPS_BORDERTHICK	1	/* Think border */
#define PSTOPS_BORDERSINGLE	2	/* Single-line hairline border */
#define PSTOPS_BORDERSINGLE2	3	/* Single-line thick border */
#define PSTOPS_BORDERDOUBLE	4	/* Double-line hairline border */
#define PSTOPS_BORDERDOUBLE2	5	/* Double-line thick border */

#define PSTOPS_LAYOUT_LRBT	0	/* Left to right, bottom to top */
#define PSTOPS_LAYOUT_LRTB	1	/* Left to right, top to bottom */
#define PSTOPS_LAYOUT_RLBT	2	/* Right to left, bottom to top */
#define PSTOPS_LAYOUT_RLTB	3	/* Right to left, top to bottom */
#define PSTOPS_LAYOUT_BTLR	4	/* Bottom to top, left to right */
#define PSTOPS_LAYOUT_TBLR	5	/* Top to bottom, left to right */
#define PSTOPS_LAYOUT_BTRL	6	/* Bottom to top, right to left */
#define PSTOPS_LAYOUT_TBRL	7	/* Top to bottom, right to left */

#define PSTOPS_LAYOUT_NEGATEY	1	/* The bits for the layout */
#define PSTOPS_LAYOUT_NEGATEX	2	/* definitions above... */
#define PSTOPS_LAYOUT_VERTICAL	4


/*
 * Types...
 */

typedef struct				/**** Page information ****/
{
  char		*label;			/* Page label */
  int		bounding_box[4];	/* PageBoundingBox */
  off_t		offset;			/* Offset to start of page */
  ssize_t	length;			/* Number of bytes for page */
  int		num_options;		/* Number of options for this page */
  cups_option_t	*options;		/* Options for this page */
} pstops_page_t;

typedef struct				/**** Document information ****/
{
  int		page;			/* Current page */
  int		bounding_box[4];	/* BoundingBox from header */
  int		new_bounding_box[4];	/* New composite bounding box */
  int		num_options;		/* Number of document-wide options */
  cups_option_t	*options;		/* Document-wide options */
  int		normal_landscape,	/* Normal rotation for landscape? */
		saw_eof,		/* Saw the %%EOF comment? */
		slow_collate,		/* Collate copies by hand? */
		slow_duplex,		/* Duplex pages slowly? */
		slow_order,		/* Reverse pages slowly? */
		use_ESPshowpage;	/* Use ESPshowpage? */
  cups_array_t	*pages;			/* Pages in document */
  cups_file_t	*temp;			/* Temporary file, if any */
  char		tempfile[1024];		/* Temporary filename */
  int		job_id;			/* Job ID */
  const char	*user,			/* User name */
		*title;			/* Job name */
  int		copies;			/* Number of copies */
  const char	*ap_input_slot,		/* AP_FIRSTPAGE_InputSlot value */
		*ap_manual_feed,	/* AP_FIRSTPAGE_ManualFeed value */
		*ap_media_color,	/* AP_FIRSTPAGE_MediaColor value */
		*ap_media_type,		/* AP_FIRSTPAGE_MediaType value */
		*ap_page_region,	/* AP_FIRSTPAGE_PageRegion value */
		*ap_page_size;		/* AP_FIRSTPAGE_PageSize value */
  int		collate,		/* Collate copies? */
		emit_jcl,		/* Emit JCL commands? */
		fit_to_page;		/* Fit pages to media */
  const char	*input_slot,		/* InputSlot value */
		*manual_feed,		/* ManualFeed value */
		*media_color,		/* MediaColor value */
		*media_type,		/* MediaType value */
		*page_region,		/* PageRegion value */
		*page_size;		/* PageSize value */
  int		mirror,			/* doc->mirror/mirror pages */
		number_up,		/* Number of pages on each sheet */
		number_up_layout,	/* doc->number_up_layout of N-up pages */
		output_order,		/* Requested reverse output order? */
		page_border;		/* doc->page_border around pages */
  const char	*page_label,		/* page-label option, if any */
		*page_ranges,		/* page-ranges option, if any */
		*page_set;		/* page-set option, if any */
} pstops_doc_t;


/*
 * Convenience macros...
 */

#define	is_first_page(p)	(doc->number_up == 1 || \
				 ((p) % doc->number_up) == 1)
#define	is_last_page(p)		(doc->number_up == 1 || \
				 ((p) % doc->number_up) == 0)
#define is_not_last_page(p)	(doc->number_up > 1 && \
				 ((p) % doc->number_up) != 0)


/*
 * Local globals...
 */

static int		JobCanceled = 0;/* Set to 1 on SIGTERM */


/*
 * Local functions...
 */

static pstops_page_t	*add_page(pstops_doc_t *doc, const char *label);
static void		cancel_job(int sig);
static int		check_range(pstops_doc_t *doc, int page);
static void		copy_bytes(cups_file_t *fp, off_t offset,
			           size_t length);
static ssize_t		copy_comments(cups_file_t *fp, pstops_doc_t *doc,
			              ppd_file_t *ppd, char *line,
				      ssize_t linelen, size_t linesize);
static void		copy_dsc(cups_file_t *fp, pstops_doc_t *doc,
			         ppd_file_t *ppd, char *line, ssize_t linelen,
				 size_t linesize);
static void		copy_non_dsc(cups_file_t *fp, pstops_doc_t *doc,
			             ppd_file_t *ppd, char *line,
				     ssize_t linelen, size_t linesize);
static ssize_t		copy_page(cups_file_t *fp, pstops_doc_t *doc,
			          ppd_file_t *ppd, int number, char *line,
				  ssize_t linelen, size_t linesize);
static ssize_t		copy_prolog(cups_file_t *fp, pstops_doc_t *doc,
			            ppd_file_t *ppd, char *line,
				    ssize_t linelen, size_t linesize);
static ssize_t		copy_setup(cups_file_t *fp, pstops_doc_t *doc,
			           ppd_file_t *ppd, char *line,
				   ssize_t linelen, size_t linesize);
static ssize_t		copy_trailer(cups_file_t *fp, pstops_doc_t *doc,
			             ppd_file_t *ppd, int number, char *line,
				     ssize_t linelen, size_t linesize);
static void		do_prolog(pstops_doc_t *doc, ppd_file_t *ppd);
static void 		do_setup(pstops_doc_t *doc, ppd_file_t *ppd);
static void		doc_printf(pstops_doc_t *doc, const char *format, ...)
			__attribute__ ((__format__ (__printf__, 2, 3)));
static void		doc_puts(pstops_doc_t *doc, const char *s);
static void		doc_write(pstops_doc_t *doc, const char *s, size_t len);
static void		end_nup(pstops_doc_t *doc, int number);
static int		include_feature(ppd_file_t *ppd, const char *line,
			                int num_options,
					cups_option_t **options);
static char		*parse_text(const char *start, char **end, char *buffer,
			            size_t bufsize);
static void		set_pstops_options(pstops_doc_t *doc, ppd_file_t *ppd,
			                   char *argv[], int num_options,
			                   cups_option_t *options);
static ssize_t		skip_page(cups_file_t *fp, char *line, ssize_t linelen,
				  size_t linesize);
static void		start_nup(pstops_doc_t *doc, int number,
				  int show_border, const int *bounding_box);
static void		write_label_prolog(pstops_doc_t *doc, const char *label,
			                   float bottom, float top,
					   float width);
static void		write_labels(pstops_doc_t *doc, int orient);
static void		write_options(pstops_doc_t  *doc, ppd_file_t *ppd,
			              int num_options, cups_option_t *options);


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  pstops_doc_t	doc;			/* Document information */
  cups_file_t	*fp;			/* Print file */
  ppd_file_t	*ppd;			/* PPD file */
  int		num_options;		/* Number of print options */
  cups_option_t	*options;		/* Print options */
  char		line[8192];		/* Line buffer */
  ssize_t	len;			/* Length of line buffer */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Make sure status messages are not buffered...
  */

  setbuf(stderr, NULL);

 /*
  * Ignore broken pipe signals...
  */

  signal(SIGPIPE, SIG_IGN);

 /*
  * Check command-line...
  */

  if (argc < 6 || argc > 7)
  {
    _cupsLangPrintf(stderr,
                    _("Usage: %s job-id user title copies options [file]"),
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
  * If we have 7 arguments, print the file named on the command-line.
  * Otherwise, send stdin instead...
  */

  if (argc == 6)
    fp = cupsFileStdin();
  else
  {
   /*
    * Try to open the print file...
    */

    if ((fp = cupsFileOpen(argv[6], "r")) == NULL)
    {
      if (!JobCanceled)
      {
        fprintf(stderr, "DEBUG: Unable to open \"%s\".\n", argv[6]);
        _cupsLangPrintError("ERROR", _("Unable to open print file"));
      }

      return (1);
    }
  }

 /*
  * Read the first line to see if we have DSC comments...
  */

  if ((len = (ssize_t)cupsFileGetLine(fp, line, sizeof(line))) == 0)
  {
    fputs("DEBUG: The print file is empty.\n", stderr);
    return (1);
  }

 /*
  * Process command-line options...
  */

  options     = NULL;
  num_options = cupsParseOptions(argv[5], 0, &options);
  ppd         = SetCommonOptions(num_options, options, 1);

  set_pstops_options(&doc, ppd, argv, num_options, options);

 /*
  * Write any "exit server" options that have been selected...
  */

  ppdEmit(ppd, stdout, PPD_ORDER_EXIT);

 /*
  * Write any JCL commands that are needed to print PostScript code...
  */

  if (doc.emit_jcl)
    ppdEmitJCL(ppd, stdout, doc.job_id, doc.user, doc.title);

 /*
  * Start with a DSC header...
  */

  puts("%!PS-Adobe-3.0");

 /*
  * Skip leading PJL in the document...
  */

  while (!strncmp(line, "\033%-12345X", 9) || !strncmp(line, "@PJL ", 5))
  {
   /*
    * Yup, we have leading PJL fun, so skip it until we hit the line
    * with "ENTER LANGUAGE"...
    */

    fputs("DEBUG: Skipping PJL header...\n", stderr);

    while (strstr(line, "ENTER LANGUAGE") == NULL && strncmp(line, "%!", 2))
      if ((len = (ssize_t)cupsFileGetLine(fp, line, sizeof(line))) == 0)
        break;

    if (!strncmp(line, "%!", 2))
      break;

    if ((len = (ssize_t)cupsFileGetLine(fp, line, sizeof(line))) == 0)
      break;
  }

 /*
  * Now see if the document conforms to the Adobe Document Structuring
  * Conventions...
  */

  if (!strncmp(line, "%!PS-Adobe-", 11))
  {
   /*
    * Yes, filter the document...
    */

    copy_dsc(fp, &doc, ppd, line, len, sizeof(line));
  }
  else
  {
   /*
    * No, display an error message and treat the file as if it contains
    * a single page...
    */

    copy_non_dsc(fp, &doc, ppd, line, len, sizeof(line));
  }

 /*
  * Send %%EOF as needed...
  */

  if (!doc.saw_eof)
    puts("%%EOF");

 /*
  * End the job with the appropriate JCL command or CTRL-D...
  */

  if (doc.emit_jcl)
  {
    if (ppd && ppd->jcl_end)
      ppdEmitJCLEnd(ppd, stdout);
    else
      putchar(0x04);
  }

 /*
  * Close files and remove the temporary file if needed...
  */

  if (doc.temp)
  {
    cupsFileClose(doc.temp);
    unlink(doc.tempfile);
  }

  ppdClose(ppd);
  cupsFreeOptions(num_options, options);

  cupsFileClose(fp);

  return (0);
}


/*
 * 'add_page()' - Add a page to the pages array.
 */

static pstops_page_t *			/* O - New page info object */
add_page(pstops_doc_t *doc,		/* I - Document information */
         const char   *label)		/* I - Page label */
{
  pstops_page_t	*pageinfo;		/* New page info object */


  if (!doc->pages)
    doc->pages = cupsArrayNew(NULL, NULL);

  if (!doc->pages)
  {
    _cupsLangPrintError("EMERG", _("Unable to allocate memory for pages array"));
    exit(1);
  }

  if ((pageinfo = calloc(1, sizeof(pstops_page_t))) == NULL)
  {
    _cupsLangPrintError("EMERG", _("Unable to allocate memory for page info"));
    exit(1);
  }

  pageinfo->label  = strdup(label);
  pageinfo->offset = cupsFileTell(doc->temp);

  cupsArrayAdd(doc->pages, pageinfo);

  doc->page ++;

  return (pageinfo);
}


/*
 * 'cancel_job()' - Flag the job as canceled.
 */

static void
cancel_job(int sig)			/* I - Signal number (unused) */
{
  (void)sig;

  JobCanceled = 1;
}


/*
 * 'check_range()' - Check to see if the current page is selected for
 *                   printing.
 */

static int				/* O - 1 if selected, 0 otherwise */
check_range(pstops_doc_t *doc,		/* I - Document information */
            int          page)		/* I - Page number */
{
  const char	*range;			/* Pointer into range string */
  int		lower, upper;		/* Lower and upper page numbers */


  if (doc->page_set)
  {
   /*
    * See if we only print even or odd pages...
    */

    if (!_cups_strcasecmp(doc->page_set, "even") && (page & 1))
      return (0);

    if (!_cups_strcasecmp(doc->page_set, "odd") && !(page & 1))
      return (0);
  }

  if (!doc->page_ranges)
    return (1);				/* No range, print all pages... */

  for (range = doc->page_ranges; *range != '\0';)
  {
    if (*range == '-')
    {
      lower = 1;
      range ++;
      upper = (int)strtol(range, (char **)&range, 10);
    }
    else
    {
      lower = (int)strtol(range, (char **)&range, 10);

      if (*range == '-')
      {
        range ++;
	if (!isdigit(*range & 255))
	  upper = 65535;
	else
	  upper = (int)strtol(range, (char **)&range, 10);
      }
      else
        upper = lower;
    }

    if (page >= lower && page <= upper)
      return (1);

    if (*range == ',')
      range ++;
    else
      break;
  }

  return (0);
}


/*
 * 'copy_bytes()' - Copy bytes from the input file to stdout.
 */

static void
copy_bytes(cups_file_t *fp,		/* I - File to read from */
           off_t       offset,		/* I - Offset to page data */
           size_t      length)		/* I - Length of page data */
{
  char		buffer[8192];		/* Data buffer */
  ssize_t	nbytes;			/* Number of bytes read */
  size_t	nleft;			/* Number of bytes left/remaining */


  nleft = length;

  if (cupsFileSeek(fp, offset) < 0)
  {
    _cupsLangPrintError("ERROR", _("Unable to see in file"));
    return;
  }

  while (nleft > 0 || length == 0)
  {
    if (nleft > sizeof(buffer) || length == 0)
      nbytes = sizeof(buffer);
    else
      nbytes = (ssize_t)nleft;

    if ((nbytes = cupsFileRead(fp, buffer, (size_t)nbytes)) < 1)
      return;

    nleft -= (size_t)nbytes;

    fwrite(buffer, 1, (size_t)nbytes, stdout);
  }
}


/*
 * 'copy_comments()' - Copy all of the comments section.
 *
 * This function expects "line" to be filled with a comment line.
 * On return, "line" will contain the next line in the file, if any.
 */

static ssize_t				/* O - Length of next line */
copy_comments(cups_file_t  *fp,		/* I - File to read from */
              pstops_doc_t *doc,	/* I - Document info */
	      ppd_file_t   *ppd,	/* I - PPD file */
              char         *line,	/* I - Line buffer */
	      ssize_t      linelen,	/* I - Length of initial line */
	      size_t       linesize)	/* I - Size of line buffer */
{
  int	saw_bounding_box,		/* Saw %%BoundingBox: comment? */
	saw_for,			/* Saw %%For: comment? */
	saw_pages,			/* Saw %%Pages: comment? */
	saw_title;			/* Saw %%Title: comment? */


 /*
  * Loop until we see %%EndComments or a non-comment line...
  */

  saw_bounding_box = 0;
  saw_for          = 0;
  saw_pages        = 0;
  saw_title        = 0;

  while (line[0] == '%')
  {
   /*
    * Strip trailing whitespace...
    */

    while (linelen > 0)
    {
      linelen --;

      if (!isspace(line[linelen] & 255))
        break;
      else
        line[linelen] = '\0';
    }

   /*
    * Log the header...
    */

    fprintf(stderr, "DEBUG: %s\n", line);

   /*
    * Pull the headers out...
    */

    if (!strncmp(line, "%%Pages:", 8))
    {
      int	pages;			/* Number of pages */

      if (saw_pages)
	fputs("DEBUG: A duplicate %%Pages: comment was seen.\n", stderr);

      saw_pages = 1;

      if (Duplex && (pages = atoi(line + 8)) > 0 && pages <= doc->number_up)
      {
       /*
        * Since we will only be printing on a single page, disable duplexing.
	*/

	Duplex           = 0;
	doc->slow_duplex = 0;

	if (cupsGetOption("sides", doc->num_options, doc->options))
	  doc->num_options = cupsAddOption("sides", "one-sided",
	                                   doc->num_options, &(doc->options));

	if (cupsGetOption("Duplex", doc->num_options, doc->options))
	  doc->num_options = cupsAddOption("Duplex", "None",
	                                   doc->num_options, &(doc->options));

	if (cupsGetOption("EFDuplex", doc->num_options, doc->options))
	  doc->num_options = cupsAddOption("EFDuplex", "None",
	                                   doc->num_options, &(doc->options));

	if (cupsGetOption("EFDuplexing", doc->num_options, doc->options))
	  doc->num_options = cupsAddOption("EFDuplexing", "False",
	                                   doc->num_options, &(doc->options));

	if (cupsGetOption("KD03Duplex", doc->num_options, doc->options))
	  doc->num_options = cupsAddOption("KD03Duplex", "None",
	                                   doc->num_options, &(doc->options));

	if (cupsGetOption("JCLDuplex", doc->num_options, doc->options))
	  doc->num_options = cupsAddOption("JCLDuplex", "None",
	                                   doc->num_options, &(doc->options));

	ppdMarkOption(ppd, "Duplex", "None");
	ppdMarkOption(ppd, "EFDuplex", "None");
	ppdMarkOption(ppd, "EFDuplexing", "False");
	ppdMarkOption(ppd, "KD03Duplex", "None");
	ppdMarkOption(ppd, "JCLDuplex", "None");
      }
    }
    else if (!strncmp(line, "%%BoundingBox:", 14))
    {
      if (saw_bounding_box)
	fputs("DEBUG: A duplicate %%BoundingBox: comment was seen.\n", stderr);
      else if (strstr(line + 14, "(atend)"))
      {
       /*
        * Do nothing for now but use the default imageable area...
	*/
      }
      else if (sscanf(line + 14, "%d%d%d%d", doc->bounding_box + 0,
	              doc->bounding_box + 1, doc->bounding_box + 2,
		      doc->bounding_box + 3) != 4)
      {
	fputs("DEBUG: A bad %%BoundingBox: comment was seen.\n", stderr);

	doc->bounding_box[0] = (int)PageLeft;
	doc->bounding_box[1] = (int)PageBottom;
	doc->bounding_box[2] = (int)PageRight;
	doc->bounding_box[3] = (int)PageTop;
      }

      saw_bounding_box = 1;
    }
    else if (!strncmp(line, "%%For:", 6))
    {
      saw_for = 1;
      doc_printf(doc, "%s\n", line);
    }
    else if (!strncmp(line, "%%Title:", 8))
    {
      saw_title = 1;
      doc_printf(doc, "%s\n", line);
    }
    else if (!strncmp(line, "%cupsRotation:", 14))
    {
     /*
      * Reset orientation of document?
      */

      int orient = (atoi(line + 14) / 90) & 3;

      if (orient != Orientation)
      {
       /*
        * Yes, update things so that the pages come out right...
	*/

	Orientation = (4 - Orientation + orient) & 3;
	UpdatePageVars();
	Orientation = orient;
      }
    }
    else if (!strcmp(line, "%%EndComments"))
    {
      linelen = (ssize_t)cupsFileGetLine(fp, line, linesize);
      break;
    }
    else if (strncmp(line, "%!", 2) && strncmp(line, "%cups", 5))
      doc_printf(doc, "%s\n", line);

    if ((linelen = (ssize_t)cupsFileGetLine(fp, line, linesize)) == 0)
      break;
  }

  if (!saw_bounding_box)
    fputs("DEBUG: There wasn't a %%BoundingBox: comment in the header.\n",
          stderr);

  if (!saw_pages)
    fputs("DEBUG: There wasn't a %%Pages: comment in the header.\n", stderr);

  if (!saw_for)
    WriteTextComment("For", doc->user);

  if (!saw_title)
    WriteTextComment("Title", doc->title);

  if (doc->copies != 1 && (!doc->collate || !doc->slow_collate))
  {
   /*
    * Tell the document processor the copy and duplex options
    * that are required...
    */

    doc_printf(doc, "%%%%Requirements: numcopies(%d)%s%s\n", doc->copies,
               doc->collate ? " collate" : "",
	       Duplex ? " duplex" : "");

   /*
    * Apple uses RBI comments for various non-PPD options...
    */

    doc_printf(doc, "%%RBINumCopies: %d\n", doc->copies);
  }
  else
  {
   /*
    * Tell the document processor the duplex option that is required...
    */

    if (Duplex)
      doc_puts(doc, "%%Requirements: duplex\n");

   /*
    * Apple uses RBI comments for various non-PPD options...
    */

    doc_puts(doc, "%RBINumCopies: 1\n");
  }

  doc_puts(doc, "%%Pages: (atend)\n");
  doc_puts(doc, "%%BoundingBox: (atend)\n");
  doc_puts(doc, "%%EndComments\n");

  return (linelen);
}


/*
 * 'copy_dsc()' - Copy a DSC-conforming document.
 *
 * This function expects "line" to be filled with the %!PS-Adobe comment line.
 */

static void
copy_dsc(cups_file_t  *fp,		/* I - File to read from */
         pstops_doc_t *doc,		/* I - Document info */
         ppd_file_t   *ppd,		/* I - PPD file */
	 char         *line,		/* I - Line buffer */
	 ssize_t      linelen,		/* I - Length of initial line */
	 size_t       linesize)		/* I - Size of line buffer */
{
  int		number;			/* Page number */
  pstops_page_t	*pageinfo;		/* Page information */


 /*
  * Make sure we use ESPshowpage for EPS files...
  */

  if (strstr(line, "EPSF"))
  {
    doc->use_ESPshowpage = 1;
    doc->number_up       = 1;
  }

 /*
  * Start sending the document with any commands needed...
  */

  fprintf(stderr, "DEBUG: Before copy_comments - %s", line);
  linelen = copy_comments(fp, doc, ppd, line, linelen, linesize);

 /*
  * Now find the prolog section, if any...
  */

  fprintf(stderr, "DEBUG: Before copy_prolog - %s", line);
  linelen = copy_prolog(fp, doc, ppd, line, linelen, linesize);

 /*
  * Then the document setup section...
  */

  fprintf(stderr, "DEBUG: Before copy_setup - %s", line);
  linelen = copy_setup(fp, doc, ppd, line, linelen, linesize);

 /*
  * Copy until we see %%Page:...
  */

  while (strncmp(line, "%%Page:", 7) && strncmp(line, "%%Trailer", 9))
  {
    doc_write(doc, line, (size_t)linelen);

    if ((linelen = (ssize_t)cupsFileGetLine(fp, line, linesize)) == 0)
      break;
  }

 /*
  * Then process pages until we have no more...
  */

  number = 0;

  fprintf(stderr, "DEBUG: Before page loop - %s", line);
  while (!strncmp(line, "%%Page:", 7))
  {
    if (JobCanceled)
      break;

    number ++;

    if (check_range(doc, (number - 1) / doc->number_up + 1))
    {
      fprintf(stderr, "DEBUG: Copying page %d...\n", number);
      linelen = copy_page(fp, doc, ppd, number, line, linelen, linesize);
    }
    else
    {
      fprintf(stderr, "DEBUG: Skipping page %d...\n", number);
      linelen = skip_page(fp, line, linelen, linesize);
    }
  }

 /*
  * Finish up the last page(s)...
  */

  if (number && is_not_last_page(number) && cupsArrayLast(doc->pages) &&
      check_range(doc, (number - 1) / doc->number_up + 1))
  {
    pageinfo = (pstops_page_t *)cupsArrayLast(doc->pages);

    start_nup(doc, doc->number_up, 0, doc->bounding_box);
    doc_puts(doc, "showpage\n");
    end_nup(doc, doc->number_up);

    pageinfo->length = (ssize_t)(cupsFileTell(doc->temp) - pageinfo->offset);
  }

  if (doc->slow_duplex && (doc->page & 1))
  {
   /*
    * Make sure we have an even number of pages...
    */

    pageinfo = add_page(doc, "(filler)");

    if (!doc->slow_order)
    {
      if (!ppd || !ppd->num_filters)
	fprintf(stderr, "PAGE: %d %d\n", doc->page,
        	doc->slow_collate ? 1 : doc->copies);

      printf("%%%%Page: (filler) %d\n", doc->page);
    }

    start_nup(doc, doc->number_up, 0, doc->bounding_box);
    doc_puts(doc, "showpage\n");
    end_nup(doc, doc->number_up);

    pageinfo->length = (ssize_t)(cupsFileTell(doc->temp) - pageinfo->offset);
  }

 /*
  * Make additional copies as necessary...
  */

  number = doc->slow_order ? 0 : doc->page;

  if (doc->temp && !JobCanceled && cupsArrayCount(doc->pages) > 0)
  {
    int	copy;				/* Current copy */


   /*
    * Reopen the temporary file for reading...
    */

    cupsFileClose(doc->temp);

    doc->temp = cupsFileOpen(doc->tempfile, "r");

   /*
    * Make the copies...
    */

    if (doc->slow_collate)
      copy = !doc->slow_order;
    else
      copy = doc->copies - 1;

    for (; copy < doc->copies; copy ++)
    {
      if (JobCanceled)
	break;

     /*
      * Send end-of-job stuff followed by any start-of-job stuff required
      * for the JCL options...
      */

      if (number && doc->emit_jcl && ppd && ppd->jcl_end)
      {
       /*
        * Send the trailer...
	*/

        puts("%%Trailer");
	printf("%%%%Pages: %d\n", cupsArrayCount(doc->pages));
	if (doc->number_up > 1 || doc->fit_to_page)
	  printf("%%%%BoundingBox: %.0f %.0f %.0f %.0f\n",
		 PageLeft, PageBottom, PageRight, PageTop);
	else
	  printf("%%%%BoundingBox: %d %d %d %d\n",
		 doc->new_bounding_box[0], doc->new_bounding_box[1],
		 doc->new_bounding_box[2], doc->new_bounding_box[3]);
        puts("%%EOF");

       /*
        * Start a new document...
	*/

        ppdEmitJCLEnd(ppd, stdout);
        ppdEmitJCL(ppd, stdout, doc->job_id, doc->user, doc->title);

	puts("%!PS-Adobe-3.0");

	number = 0;
      }

     /*
      * Copy the prolog as needed...
      */

      if (!number)
      {
        pageinfo = (pstops_page_t *)cupsArrayFirst(doc->pages);
	copy_bytes(doc->temp, 0, (size_t)pageinfo->offset);
      }

     /*
      * Then copy all of the pages...
      */

      pageinfo = doc->slow_order ? (pstops_page_t *)cupsArrayLast(doc->pages) :
                                   (pstops_page_t *)cupsArrayFirst(doc->pages);

      while (pageinfo)
      {
        if (JobCanceled)
	  break;

        number ++;

	if (!ppd || !ppd->num_filters)
	  fprintf(stderr, "PAGE: %d %d\n", number,
	          doc->slow_collate ? 1 : doc->copies);

	if (doc->number_up > 1)
	{
	  printf("%%%%Page: (%d) %d\n", number, number);
	  printf("%%%%PageBoundingBox: %.0f %.0f %.0f %.0f\n",
		 PageLeft, PageBottom, PageRight, PageTop);
	}
	else
	{
          printf("%%%%Page: %s %d\n", pageinfo->label, number);
	  printf("%%%%PageBoundingBox: %d %d %d %d\n",
		 pageinfo->bounding_box[0], pageinfo->bounding_box[1],
		 pageinfo->bounding_box[2], pageinfo->bounding_box[3]);
	}

	copy_bytes(doc->temp, pageinfo->offset, (size_t)pageinfo->length);

	pageinfo = doc->slow_order ? (pstops_page_t *)cupsArrayPrev(doc->pages) :
                                     (pstops_page_t *)cupsArrayNext(doc->pages);
      }
    }
  }

 /*
  * Restore the old showpage operator as needed...
  */

  if (doc->use_ESPshowpage)
    puts("userdict/showpage/ESPshowpage load put\n");

 /*
  * Write/copy the trailer...
  */

  if (!JobCanceled)
    copy_trailer(fp, doc, ppd, number, line, linelen, linesize);
}


/*
 * 'copy_non_dsc()' - Copy a document that does not conform to the DSC.
 *
 * This function expects "line" to be filled with the %! comment line.
 */

static void
copy_non_dsc(cups_file_t  *fp,		/* I - File to read from */
             pstops_doc_t *doc,		/* I - Document info */
             ppd_file_t   *ppd,		/* I - PPD file */
	     char         *line,	/* I - Line buffer */
	     ssize_t      linelen,	/* I - Length of initial line */
	     size_t       linesize)	/* I - Size of line buffer */
{
  int		copy;			/* Current copy */
  char		buffer[8192];		/* Copy buffer */
  ssize_t	bytes;			/* Number of bytes copied */


  (void)linesize;

 /*
  * First let the user know that they are attempting to print a file
  * that may not print correctly...
  */

  fputs("DEBUG: This document does not conform to the Adobe Document "
        "Structuring Conventions and may not print correctly.\n", stderr);

 /*
  * Then write a standard DSC comment section...
  */

  printf("%%%%BoundingBox: %.0f %.0f %.0f %.0f\n", PageLeft, PageBottom,
         PageRight, PageTop);

  if (doc->slow_collate && doc->copies > 1)
    printf("%%%%Pages: %d\n", doc->copies);
  else
    puts("%%Pages: 1");

  WriteTextComment("For", doc->user);
  WriteTextComment("Title", doc->title);

  if (doc->copies != 1 && (!doc->collate || !doc->slow_collate))
  {
   /*
    * Tell the document processor the copy and duplex options
    * that are required...
    */

    printf("%%%%Requirements: numcopies(%d)%s%s\n", doc->copies,
           doc->collate ? " collate" : "",
	   Duplex ? " duplex" : "");

   /*
    * Apple uses RBI comments for various non-PPD options...
    */

    printf("%%RBINumCopies: %d\n", doc->copies);
  }
  else
  {
   /*
    * Tell the document processor the duplex option that is required...
    */

    if (Duplex)
      puts("%%Requirements: duplex");

   /*
    * Apple uses RBI comments for various non-PPD options...
    */

    puts("%RBINumCopies: 1");
  }

  puts("%%EndComments");

 /*
  * Then the prolog...
  */

  puts("%%BeginProlog");

  do_prolog(doc, ppd);

  puts("%%EndProlog");

 /*
  * Then the setup section...
  */

  puts("%%BeginSetup");

  do_setup(doc, ppd);

  puts("%%EndSetup");

 /*
  * Finally, embed a copy of the file inside a %%Page...
  */

  if (!ppd || !ppd->num_filters)
    fprintf(stderr, "PAGE: 1 %d\n", doc->temp ? 1 : doc->copies);

  puts("%%Page: 1 1");
  puts("%%BeginPageSetup");
  ppdEmit(ppd, stdout, PPD_ORDER_PAGE);
  puts("%%EndPageSetup");
  puts("%%BeginDocument: nondsc");

  fwrite(line, (size_t)linelen, 1, stdout);

  if (doc->temp)
    cupsFileWrite(doc->temp, line, (size_t)linelen);

  while ((bytes = cupsFileRead(fp, buffer, sizeof(buffer))) > 0)
  {
    fwrite(buffer, 1, (size_t)bytes, stdout);

    if (doc->temp)
      cupsFileWrite(doc->temp, buffer, (size_t)bytes);
  }

  puts("%%EndDocument");

  if (doc->use_ESPshowpage)
  {
    WriteLabels(Orientation);
    puts("ESPshowpage");
  }

  if (doc->temp && !JobCanceled)
  {
   /*
    * Reopen the temporary file for reading...
    */

    cupsFileClose(doc->temp);

    doc->temp = cupsFileOpen(doc->tempfile, "r");

   /*
    * Make the additional copies as needed...
    */

    for (copy = 1; copy < doc->copies; copy ++)
    {
      if (JobCanceled)
	break;

      if (!ppd || !ppd->num_filters)
	fputs("PAGE: 1 1\n", stderr);

      printf("%%%%Page: %d %d\n", copy + 1, copy + 1);
      puts("%%BeginPageSetup");
      ppdEmit(ppd, stdout, PPD_ORDER_PAGE);
      puts("%%EndPageSetup");
      puts("%%BeginDocument: nondsc");

      copy_bytes(doc->temp, 0, 0);

      puts("%%EndDocument");

      if (doc->use_ESPshowpage)
      {
	WriteLabels(Orientation);
        puts("ESPshowpage");
      }
    }
  }

 /*
  * Restore the old showpage operator as needed...
  */

  if (doc->use_ESPshowpage)
    puts("userdict/showpage/ESPshowpage load put\n");
}


/*
 * 'copy_page()' - Copy a page description.
 *
 * This function expects "line" to be filled with a %%Page comment line.
 * On return, "line" will contain the next line in the file, if any.
 */

static ssize_t				/* O - Length of next line */
copy_page(cups_file_t  *fp,		/* I - File to read from */
          pstops_doc_t *doc,		/* I - Document info */
          ppd_file_t   *ppd,		/* I - PPD file */
	  int          number,		/* I - Current page number */
	  char         *line,		/* I - Line buffer */
	  ssize_t      linelen,		/* I - Length of initial line */
	  size_t       linesize)	/* I - Size of line buffer */
{
  char		label[256],		/* Page label string */
		*ptr;			/* Pointer into line */
  int		level;			/* Embedded document level */
  pstops_page_t	*pageinfo;		/* Page information */
  int		first_page;		/* First page on N-up output? */
  int		has_page_setup = 0;	/* Does the page have %%Begin/EndPageSetup? */
  int		bounding_box[4];	/* PageBoundingBox */


 /*
  * Get the page label for this page...
  */

  first_page = is_first_page(number);

  if (!parse_text(line + 7, &ptr, label, sizeof(label)))
  {
    fputs("DEBUG: There was a bad %%Page: comment in the file.\n", stderr);
    label[0] = '\0';
    number   = doc->page;
  }
  else if (strtol(ptr, &ptr, 10) == LONG_MAX || !isspace(*ptr & 255))
  {
    fputs("DEBUG: There was a bad %%Page: comment in the file.\n", stderr);
    number = doc->page;
  }

 /*
  * Create or update the current output page...
  */

  if (first_page)
    pageinfo = add_page(doc, label);
  else
    pageinfo = (pstops_page_t *)cupsArrayLast(doc->pages);

 /*
  * Handle first page override...
  */

  if (doc->ap_input_slot || doc->ap_manual_feed)
  {
    if ((doc->page == 1 && (!doc->slow_order || !Duplex)) ||
        (doc->page == 2 && doc->slow_order && Duplex))
    {
     /*
      * First page/sheet gets AP_FIRSTPAGE_* options...
      */

      pageinfo->num_options = cupsAddOption("InputSlot", doc->ap_input_slot,
                                            pageinfo->num_options,
					    &(pageinfo->options));
      pageinfo->num_options = cupsAddOption("ManualFeed",
                                            doc->ap_input_slot ? "False" :
					        doc->ap_manual_feed,
                                            pageinfo->num_options,
					    &(pageinfo->options));
      pageinfo->num_options = cupsAddOption("MediaColor", doc->ap_media_color,
                                            pageinfo->num_options,
					    &(pageinfo->options));
      pageinfo->num_options = cupsAddOption("MediaType", doc->ap_media_type,
                                            pageinfo->num_options,
					    &(pageinfo->options));
      pageinfo->num_options = cupsAddOption("PageRegion", doc->ap_page_region,
                                            pageinfo->num_options,
					    &(pageinfo->options));
      pageinfo->num_options = cupsAddOption("PageSize", doc->ap_page_size,
                                            pageinfo->num_options,
					    &(pageinfo->options));
    }
    else if (doc->page == (Duplex + 2))
    {
     /*
      * Second page/sheet gets default options...
      */

      pageinfo->num_options = cupsAddOption("InputSlot", doc->input_slot,
                                            pageinfo->num_options,
					    &(pageinfo->options));
      pageinfo->num_options = cupsAddOption("ManualFeed",
                                            doc->input_slot ? "False" :
					        doc->manual_feed,
                                            pageinfo->num_options,
					    &(pageinfo->options));
      pageinfo->num_options = cupsAddOption("MediaColor", doc->media_color,
                                            pageinfo->num_options,
					    &(pageinfo->options));
      pageinfo->num_options = cupsAddOption("MediaType", doc->media_type,
                                            pageinfo->num_options,
					    &(pageinfo->options));
      pageinfo->num_options = cupsAddOption("PageRegion", doc->page_region,
                                            pageinfo->num_options,
					    &(pageinfo->options));
      pageinfo->num_options = cupsAddOption("PageSize", doc->page_size,
                                            pageinfo->num_options,
					    &(pageinfo->options));
    }
  }

 /*
  * Scan comments until we see something other than %%Page*: or
  * %%Include*...
  */

  memcpy(bounding_box, doc->bounding_box, sizeof(bounding_box));

  while ((linelen = (ssize_t)cupsFileGetLine(fp, line, linesize)) > 0)
  {
    if (!strncmp(line, "%%PageBoundingBox:", 18))
    {
     /*
      * %%PageBoundingBox: llx lly urx ury
      */

      if (sscanf(line + 18, "%d%d%d%d", bounding_box + 0,
                 bounding_box + 1, bounding_box + 2,
		 bounding_box + 3) != 4)
      {
	fputs("DEBUG: There was a bad %%PageBoundingBox: comment in the file.\n", stderr);
        memcpy(bounding_box, doc->bounding_box,
	       sizeof(bounding_box));
      }
      else if (doc->number_up == 1 && !doc->fit_to_page  && Orientation)
      {
        int	temp_bbox[4];		/* Temporary bounding box */


        memcpy(temp_bbox, bounding_box, sizeof(temp_bbox));

        fprintf(stderr, "DEBUG: Orientation = %d\n", Orientation);
        fprintf(stderr, "DEBUG: original bounding_box = [ %d %d %d %d ]\n",
		bounding_box[0], bounding_box[1],
		bounding_box[2], bounding_box[3]);
        fprintf(stderr, "DEBUG: PageWidth = %.1f, PageLength = %.1f\n",
	        PageWidth, PageLength);

        switch (Orientation)
	{
	  case 1 : /* Landscape */
	      bounding_box[0] = (int)(PageLength - temp_bbox[3]);
	      bounding_box[1] = temp_bbox[0];
	      bounding_box[2] = (int)(PageLength - temp_bbox[1]);
	      bounding_box[3] = temp_bbox[2];
              break;

	  case 2 : /* Reverse Portrait */
	      bounding_box[0] = (int)(PageWidth - temp_bbox[2]);
	      bounding_box[1] = (int)(PageLength - temp_bbox[3]);
	      bounding_box[2] = (int)(PageWidth - temp_bbox[0]);
	      bounding_box[3] = (int)(PageLength - temp_bbox[1]);
              break;

	  case 3 : /* Reverse Landscape */
	      bounding_box[0] = temp_bbox[1];
	      bounding_box[1] = (int)(PageWidth - temp_bbox[2]);
	      bounding_box[2] = temp_bbox[3];
	      bounding_box[3] = (int)(PageWidth - temp_bbox[0]);
              break;
	}

        fprintf(stderr, "DEBUG: updated bounding_box = [ %d %d %d %d ]\n",
		bounding_box[0], bounding_box[1],
		bounding_box[2], bounding_box[3]);
      }
    }
#if 0
    else if (!strncmp(line, "%%PageCustomColors:", 19) ||
             !strncmp(line, "%%PageMedia:", 12) ||
	     !strncmp(line, "%%PageOrientation:", 18) ||
	     !strncmp(line, "%%PageProcessColors:", 20) ||
	     !strncmp(line, "%%PageRequirements:", 18) ||
	     !strncmp(line, "%%PageResources:", 16))
    {
     /*
      * Copy literal...
      */
    }
#endif /* 0 */
    else if (!strncmp(line, "%%PageCustomColors:", 19))
    {
     /*
      * %%PageCustomColors: ...
      */
    }
    else if (!strncmp(line, "%%PageMedia:", 12))
    {
     /*
      * %%PageMedia: ...
      */
    }
    else if (!strncmp(line, "%%PageOrientation:", 18))
    {
     /*
      * %%PageOrientation: ...
      */
    }
    else if (!strncmp(line, "%%PageProcessColors:", 20))
    {
     /*
      * %%PageProcessColors: ...
      */
    }
    else if (!strncmp(line, "%%PageRequirements:", 18))
    {
     /*
      * %%PageRequirements: ...
      */
    }
    else if (!strncmp(line, "%%PageResources:", 16))
    {
     /*
      * %%PageResources: ...
      */
    }
    else if (!strncmp(line, "%%IncludeFeature:", 17))
    {
     /*
      * %%IncludeFeature: *MainKeyword OptionKeyword
      */

      if (doc->number_up == 1 &&!doc->fit_to_page)
	pageinfo->num_options = include_feature(ppd, line,
	                                        pageinfo->num_options,
                                        	&(pageinfo->options));
    }
    else if (!strncmp(line, "%%BeginPageSetup", 16))
    {
      has_page_setup = 1;
      break;
    }
    else
      break;
  }

  if (doc->number_up == 1)
  {
   /*
    * Update the document's composite and page bounding box...
    */

    memcpy(pageinfo->bounding_box, bounding_box,
           sizeof(pageinfo->bounding_box));

    if (bounding_box[0] < doc->new_bounding_box[0])
      doc->new_bounding_box[0] = bounding_box[0];
    if (bounding_box[1] < doc->new_bounding_box[1])
      doc->new_bounding_box[1] = bounding_box[1];
    if (bounding_box[2] > doc->new_bounding_box[2])
      doc->new_bounding_box[2] = bounding_box[2];
    if (bounding_box[3] > doc->new_bounding_box[3])
      doc->new_bounding_box[3] = bounding_box[3];
  }

 /*
  * Output the page header as needed...
  */

  if (!doc->slow_order && first_page)
  {
    if (!ppd || !ppd->num_filters)
      fprintf(stderr, "PAGE: %d %d\n", doc->page,
	      doc->slow_collate ? 1 : doc->copies);

    if (doc->number_up > 1)
    {
      printf("%%%%Page: (%d) %d\n", doc->page, doc->page);
      printf("%%%%PageBoundingBox: %.0f %.0f %.0f %.0f\n",
	     PageLeft, PageBottom, PageRight, PageTop);
    }
    else
    {
      printf("%%%%Page: %s %d\n", pageinfo->label, doc->page);
      printf("%%%%PageBoundingBox: %d %d %d %d\n",
	     pageinfo->bounding_box[0], pageinfo->bounding_box[1],
	     pageinfo->bounding_box[2], pageinfo->bounding_box[3]);
    }
  }

 /*
  * Copy any page setup commands...
  */

  if (first_page)
    doc_puts(doc, "%%BeginPageSetup\n");

  if (has_page_setup)
  {
    int	feature = 0;			/* In a Begin/EndFeature block? */

    while ((linelen = (ssize_t)cupsFileGetLine(fp, line, linesize)) > 0)
    {
      if (!strncmp(line, "%%EndPageSetup", 14))
	break;
      else if (!strncmp(line, "%%BeginFeature:", 15))
      {
	feature = 1;

	if (doc->number_up > 1 || doc->fit_to_page)
	  continue;
      }
      else if (!strncmp(line, "%%EndFeature", 12))
      {
	feature = 0;

	if (doc->number_up > 1 || doc->fit_to_page)
	  continue;
      }
      else if (!strncmp(line, "%%IncludeFeature:", 17))
      {
	pageinfo->num_options = include_feature(ppd, line,
						pageinfo->num_options,
						&(pageinfo->options));
	continue;
      }
      else if (!strncmp(line, "%%Include", 9))
	continue;

      if (line[0] != '%' && !feature)
        break;

      if (!feature || (doc->number_up == 1 && !doc->fit_to_page))
	doc_write(doc, line, (size_t)linelen);
    }

   /*
    * Skip %%EndPageSetup...
    */

    if (linelen > 0 && !strncmp(line, "%%EndPageSetup", 14))
      linelen = (ssize_t)cupsFileGetLine(fp, line, linesize);
  }

  if (first_page)
  {
    char	*page_setup;		/* PageSetup commands to send */


    if (pageinfo->num_options > 0)
      write_options(doc, ppd, pageinfo->num_options, pageinfo->options);

   /*
    * Output commands for the current page...
    */

    page_setup = ppdEmitString(ppd, PPD_ORDER_PAGE, 0);

    if (page_setup)
    {
      doc_puts(doc, page_setup);
      free(page_setup);
    }
  }

 /*
  * Prep for the start of the page description...
  */

  start_nup(doc, number, 1, bounding_box);

  if (first_page)
    doc_puts(doc, "%%EndPageSetup\n");

 /*
  * Read the rest of the page description...
  */

  level = 0;

  do
  {
    if (level == 0 &&
        (!strncmp(line, "%%Page:", 7) ||
	 !strncmp(line, "%%Trailer", 9) ||
	 !strncmp(line, "%%EOF", 5)))
      break;
    else if (!strncmp(line, "%%BeginDocument", 15) ||
	     !strncmp(line, "%ADO_BeginApplication", 21))
    {
      doc_write(doc, line, (size_t)linelen);

      level ++;
    }
    else if ((!strncmp(line, "%%EndDocument", 13) ||
	      !strncmp(line, "%ADO_EndApplication", 19)) && level > 0)
    {
      doc_write(doc, line, (size_t)linelen);

      level --;
    }
    else if (!strncmp(line, "%%BeginBinary:", 14) ||
             (!strncmp(line, "%%BeginData:", 12) &&
	      !strstr(line, "ASCII") && !strstr(line, "Hex")))
    {
     /*
      * Copy binary data...
      */

      int	bytes;			/* Bytes of data */


      doc_write(doc, line, (size_t)linelen);

      bytes = atoi(strchr(line, ':') + 1);

      while (bytes > 0)
      {
	if ((size_t)bytes > linesize)
	  linelen = cupsFileRead(fp, line, linesize);
	else
	  linelen = cupsFileRead(fp, line, (size_t)bytes);

	if (linelen < 1)
	{
	  line[0] = '\0';
	  perror("ERROR: Early end-of-file while reading binary data");
	  return (0);
	}

        doc_write(doc, line, (size_t)linelen);

	bytes -= linelen;
      }
    }
    else
      doc_write(doc, line, (size_t)linelen);
  }
  while ((linelen = (ssize_t)cupsFileGetLine(fp, line, linesize)) > 0);

 /*
  * Finish up this page and return...
  */

  end_nup(doc, number);

  pageinfo->length = (ssize_t)(cupsFileTell(doc->temp) - pageinfo->offset);

  return (linelen);
}


/*
 * 'copy_prolog()' - Copy the document prolog section.
 *
 * This function expects "line" to be filled with a %%BeginProlog comment line.
 * On return, "line" will contain the next line in the file, if any.
 */

static ssize_t				/* O - Length of next line */
copy_prolog(cups_file_t  *fp,		/* I - File to read from */
            pstops_doc_t *doc,		/* I - Document info */
            ppd_file_t   *ppd,		/* I - PPD file */
	    char         *line,		/* I - Line buffer */
	    ssize_t      linelen,	/* I - Length of initial line */
	    size_t       linesize)	/* I - Size of line buffer */
{
  while (strncmp(line, "%%BeginProlog", 13))
  {
    if (!strncmp(line, "%%BeginSetup", 12) || !strncmp(line, "%%Page:", 7))
      break;

    doc_write(doc, line, (size_t)linelen);

    if ((linelen = (ssize_t)cupsFileGetLine(fp, line, linesize)) == 0)
      break;
  }

  doc_puts(doc, "%%BeginProlog\n");

  do_prolog(doc, ppd);

  if (!strncmp(line, "%%BeginProlog", 13))
  {
    while ((linelen = (ssize_t)cupsFileGetLine(fp, line, linesize)) > 0)
    {
      if (!strncmp(line, "%%EndProlog", 11) ||
          !strncmp(line, "%%BeginSetup", 12) ||
          !strncmp(line, "%%Page:", 7))
        break;

      doc_write(doc, line, (size_t)linelen);
    }

    if (!strncmp(line, "%%EndProlog", 11))
      linelen = (ssize_t)cupsFileGetLine(fp, line, linesize);
    else
      fputs("DEBUG: The %%EndProlog comment is missing.\n", stderr);
  }

  doc_puts(doc, "%%EndProlog\n");

  return (linelen);
}


/*
 * 'copy_setup()' - Copy the document setup section.
 *
 * This function expects "line" to be filled with a %%BeginSetup comment line.
 * On return, "line" will contain the next line in the file, if any.
 */

static ssize_t				/* O - Length of next line */
copy_setup(cups_file_t  *fp,		/* I - File to read from */
           pstops_doc_t *doc,		/* I - Document info */
           ppd_file_t   *ppd,		/* I - PPD file */
	   char         *line,		/* I - Line buffer */
	   ssize_t      linelen,	/* I - Length of initial line */
	   size_t       linesize)	/* I - Size of line buffer */
{
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */


  while (strncmp(line, "%%BeginSetup", 12))
  {
    if (!strncmp(line, "%%Page:", 7))
      break;

    doc_write(doc, line, (size_t)linelen);

    if ((linelen = (ssize_t)cupsFileGetLine(fp, line, linesize)) == 0)
      break;
  }

  doc_puts(doc, "%%BeginSetup\n");

  do_setup(doc, ppd);

  num_options = 0;
  options     = NULL;

  if (!strncmp(line, "%%BeginSetup", 12))
  {
    while (strncmp(line, "%%EndSetup", 10))
    {
      if (!strncmp(line, "%%Page:", 7))
        break;
      else if (!strncmp(line, "%%IncludeFeature:", 17))
      {
       /*
	* %%IncludeFeature: *MainKeyword OptionKeyword
	*/

        if (doc->number_up == 1 && !doc->fit_to_page)
	  num_options = include_feature(ppd, line, num_options, &options);
      }
      else if (strncmp(line, "%%BeginSetup", 12))
        doc_write(doc, line, (size_t)linelen);

      if ((linelen = (ssize_t)cupsFileGetLine(fp, line, linesize)) == 0)
	break;
    }

    if (!strncmp(line, "%%EndSetup", 10))
      linelen = (ssize_t)cupsFileGetLine(fp, line, linesize);
    else
      fputs("DEBUG: The %%EndSetup comment is missing.\n", stderr);
  }

  if (num_options > 0)
  {
    write_options(doc, ppd, num_options, options);
    cupsFreeOptions(num_options, options);
  }

  doc_puts(doc, "%%EndSetup\n");

  return (linelen);
}


/*
 * 'copy_trailer()' - Copy the document trailer.
 *
 * This function expects "line" to be filled with a %%Trailer comment line.
 * On return, "line" will contain the next line in the file, if any.
 */

static ssize_t				/* O - Length of next line */
copy_trailer(cups_file_t  *fp,		/* I - File to read from */
             pstops_doc_t *doc,		/* I - Document info */
             ppd_file_t   *ppd,		/* I - PPD file */
	     int          number,	/* I - Number of pages */
	     char         *line,	/* I - Line buffer */
	     ssize_t      linelen,	/* I - Length of initial line */
	     size_t       linesize)	/* I - Size of line buffer */
{
 /*
  * Write the trailer comments...
  */

  (void)ppd;

  puts("%%Trailer");

  while (linelen > 0)
  {
    if (!strncmp(line, "%%EOF", 5))
      break;
    else if (strncmp(line, "%%Trailer", 9) &&
             strncmp(line, "%%Pages:", 8) &&
             strncmp(line, "%%BoundingBox:", 14))
      fwrite(line, 1, (size_t)linelen, stdout);

    linelen = (ssize_t)cupsFileGetLine(fp, line, linesize);
  }

  fprintf(stderr, "DEBUG: Wrote %d pages...\n", number);

  printf("%%%%Pages: %d\n", number);
  if (doc->number_up > 1 || doc->fit_to_page)
    printf("%%%%BoundingBox: %.0f %.0f %.0f %.0f\n",
	   PageLeft, PageBottom, PageRight, PageTop);
  else
    printf("%%%%BoundingBox: %d %d %d %d\n",
	   doc->new_bounding_box[0], doc->new_bounding_box[1],
	   doc->new_bounding_box[2], doc->new_bounding_box[3]);

  return (linelen);
}


/*
 * 'do_prolog()' - Send the necessary document prolog commands.
 */

static void
do_prolog(pstops_doc_t *doc,		/* I - Document information */
          ppd_file_t   *ppd)		/* I - PPD file */
{
  char	*ps;				/* PS commands */


 /*
  * Send the document prolog commands...
  */

  if (ppd && ppd->patches)
  {
    doc_puts(doc, "%%BeginFeature: *JobPatchFile 1\n");
    doc_puts(doc, ppd->patches);
    doc_puts(doc, "\n%%EndFeature\n");
  }

  if ((ps = ppdEmitString(ppd, PPD_ORDER_PROLOG, 0.0)) != NULL)
  {
    doc_puts(doc, ps);
    free(ps);
  }

 /*
  * Define ESPshowpage here so that applications that define their
  * own procedure to do a showpage pick it up...
  */

  if (doc->use_ESPshowpage)
    doc_puts(doc, "userdict/ESPshowpage/showpage load put\n"
	          "userdict/showpage{}put\n");
}


/*
 * 'do_setup()' - Send the necessary document setup commands.
 */

static void
do_setup(pstops_doc_t *doc,		/* I - Document information */
         ppd_file_t   *ppd)		/* I - PPD file */
{
  char	*ps;				/* PS commands */


 /*
  * Disable CTRL-D so that embedded files don't cause printing
  * errors...
  */

  doc_puts(doc, "% Disable CTRL-D as an end-of-file marker...\n");
  doc_puts(doc, "userdict dup(\\004)cvn{}put (\\004\\004)cvn{}put\n");

 /*
  * Mark job options...
  */

  cupsMarkOptions(ppd, doc->num_options, doc->options);

 /*
  * Send all the printer-specific setup commands...
  */

  if ((ps = ppdEmitString(ppd, PPD_ORDER_DOCUMENT, 0.0)) != NULL)
  {
    doc_puts(doc, ps);
    free(ps);
  }

  if ((ps = ppdEmitString(ppd, PPD_ORDER_ANY, 0.0)) != NULL)
  {
    doc_puts(doc, ps);
    free(ps);
  }

 /*
  * Set the number of copies for the job...
  */

  if (doc->copies != 1 && (!doc->collate || !doc->slow_collate))
  {
    doc_printf(doc, "%%RBIBeginNonPPDFeature: *NumCopies %d\n", doc->copies);
    doc_printf(doc,
               "%d/languagelevel where{pop languagelevel 2 ge}{false}ifelse\n"
               "{1 dict begin/NumCopies exch def currentdict end "
	       "setpagedevice}\n"
	       "{userdict/#copies 3 -1 roll put}ifelse\n", doc->copies);
    doc_puts(doc, "%RBIEndNonPPDFeature\n");
  }

 /*
  * If we are doing N-up printing, disable setpagedevice...
  */

  if (doc->number_up > 1)
  {
    doc_puts(doc, "userdict/CUPSsetpagedevice/setpagedevice load put\n");
    doc_puts(doc, "userdict/setpagedevice{pop}bind put\n");
  }

 /*
  * Make sure we have rectclip and rectstroke procedures of some sort...
  */

  doc_puts(doc,
           "% x y w h ESPrc - Clip to a rectangle.\n"
	   "userdict/ESPrc/rectclip where{pop/rectclip load}\n"
	   "{{newpath 4 2 roll moveto 1 index 0 rlineto 0 exch rlineto\n"
	   "neg 0 rlineto closepath clip newpath}bind}ifelse put\n");

  doc_puts(doc,
           "% x y w h ESPrf - Fill a rectangle.\n"
	   "userdict/ESPrf/rectfill where{pop/rectfill load}\n"
	   "{{gsave newpath 4 2 roll moveto 1 index 0 rlineto 0 exch rlineto\n"
	   "neg 0 rlineto closepath fill grestore}bind}ifelse put\n");

  doc_puts(doc,
           "% x y w h ESPrs - Stroke a rectangle.\n"
	   "userdict/ESPrs/rectstroke where{pop/rectstroke load}\n"
	   "{{gsave newpath 4 2 roll moveto 1 index 0 rlineto 0 exch rlineto\n"
	   "neg 0 rlineto closepath stroke grestore}bind}ifelse put\n");

 /*
  * Write the page and label prologs...
  */

  if (doc->number_up == 2 || doc->number_up == 6)
  {
   /*
    * For 2- and 6-up output, rotate the labels to match the orientation
    * of the pages...
    */

    if (Orientation & 1)
      write_label_prolog(doc, doc->page_label, PageBottom,
                         PageWidth - PageLength + PageTop, PageLength);
    else
      write_label_prolog(doc, doc->page_label, PageLeft, PageRight,
                         PageLength);
  }
  else
    write_label_prolog(doc, doc->page_label, PageBottom, PageTop, PageWidth);
}


/*
 * 'doc_printf()' - Send a formatted string to stdout and/or the temp file.
 *
 * This function should be used for all page-level output that is affected
 * by ordering, collation, etc.
 */

static void
doc_printf(pstops_doc_t *doc,		/* I - Document information */
           const char   *format,	/* I - Printf-style format string */
	   ...)				/* I - Additional arguments as needed */
{
  va_list	ap;			/* Pointer to arguments */
  char		buffer[1024];		/* Output buffer */
  ssize_t	bytes;			/* Number of bytes to write */


  va_start(ap, format);
  bytes = vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);

  if ((size_t)bytes > sizeof(buffer))
  {
    _cupsLangPrintFilter(stderr, "ERROR",
                         _("Buffer overflow detected, aborting."));
    exit(1);
  }

  doc_write(doc, buffer, (size_t)bytes);
}


/*
 * 'doc_puts()' - Send a nul-terminated string to stdout and/or the temp file.
 *
 * This function should be used for all page-level output that is affected
 * by ordering, collation, etc.
 */

static void
doc_puts(pstops_doc_t *doc,		/* I - Document information */
         const char   *s)		/* I - String to send */
{
  doc_write(doc, s, strlen(s));
}


/*
 * 'doc_write()' - Send data to stdout and/or the temp file.
 */

static void
doc_write(pstops_doc_t *doc,		/* I - Document information */
          const char   *s,		/* I - Data to send */
	  size_t       len)		/* I - Number of bytes to send */
{
  if (!doc->slow_order)
    fwrite(s, 1, len, stdout);

  if (doc->temp)
    cupsFileWrite(doc->temp, s, len);
}


/*
 * 'end_nup()' - End processing for N-up printing.
 */

static void
end_nup(pstops_doc_t *doc,		/* I - Document information */
        int          number)		/* I - Page number */
{
  if (doc->number_up > 1)
    doc_puts(doc, "userdict/ESPsave get restore\n");

  switch (doc->number_up)
  {
    case 1 :
	if (doc->use_ESPshowpage)
	{
	  write_labels(doc, Orientation);
          doc_puts(doc, "ESPshowpage\n");
	}
	break;

    case 2 :
    case 6 :
	if (is_last_page(number) && doc->use_ESPshowpage)
	{
	  if (Orientation & 1)
	  {
	   /*
	    * Rotate the labels back to portrait...
	    */

	    write_labels(doc, Orientation - 1);
	  }
	  else if (Orientation == 0)
	  {
	   /*
	    * Rotate the labels to landscape...
	    */

	    write_labels(doc, doc->normal_landscape ? 1 : 3);
	  }
	  else
	  {
	   /*
	    * Rotate the labels to landscape...
	    */

	    write_labels(doc, doc->normal_landscape ? 3 : 1);
	  }

          doc_puts(doc, "ESPshowpage\n");
	}
        break;

    default :
	if (is_last_page(number) && doc->use_ESPshowpage)
	{
	  write_labels(doc, Orientation);
          doc_puts(doc, "ESPshowpage\n");
	}
        break;
  }

  fflush(stdout);
}


/*
 * 'include_feature()' - Include a printer option/feature command.
 */

static int				/* O  - New number of options */
include_feature(
    ppd_file_t    *ppd,			/* I  - PPD file */
    const char    *line,		/* I  - DSC line */
    int           num_options,		/* I  - Number of options */
    cups_option_t **options)		/* IO - Options */
{
  char		name[255],		/* Option name */
		value[255];		/* Option value */
  ppd_option_t	*option;		/* Option in file */


 /*
  * Get the "%%IncludeFeature: *Keyword OptionKeyword" values...
  */

  if (sscanf(line + 17, "%254s%254s", name, value) != 2)
  {
    fputs("DEBUG: The %%IncludeFeature: comment is not valid.\n", stderr);
    return (num_options);
  }

 /*
  * Find the option and choice...
  */

  if ((option = ppdFindOption(ppd, name + 1)) == NULL)
  {
    _cupsLangPrintFilter(stderr, "WARNING", _("Unknown option \"%s\"."),
                         name + 1);
    return (num_options);
  }

  if (option->section == PPD_ORDER_EXIT ||
      option->section == PPD_ORDER_JCL)
  {
    _cupsLangPrintFilter(stderr, "WARNING",
                         _("Option \"%s\" cannot be included via "
			   "%%%%IncludeFeature."), name + 1);
    return (num_options);
  }

  if (!ppdFindChoice(option, value))
  {
    _cupsLangPrintFilter(stderr, "WARNING",
			 _("Unknown choice \"%s\" for option \"%s\"."),
			 value, name + 1);
    return (num_options);
  }

 /*
  * Add the option to the option array and return...
  */

  return (cupsAddOption(name + 1, value, num_options, options));
}


/*
 * 'parse_text()' - Parse a text value in a comment.
 *
 * This function parses a DSC text value as defined on page 36 of the
 * DSC specification.  Text values are either surrounded by parenthesis
 * or whitespace-delimited.
 *
 * The value returned is the literal characters for the entire text
 * string, including any parenthesis and escape characters.
 */

static char *				/* O - Value or NULL on error */
parse_text(const char *start,		/* I - Start of text value */
           char       **end,		/* O - End of text value */
	   char       *buffer,		/* I - Buffer */
           size_t     bufsize)		/* I - Size of buffer */
{
  char	*bufptr,			/* Pointer in buffer */
	*bufend;			/* End of buffer */
  int	level;				/* Parenthesis level */


 /*
  * Skip leading whitespace...
  */

  while (isspace(*start & 255))
    start ++;

 /*
  * Then copy the value...
  */

  level  = 0;
  bufptr = buffer;
  bufend = buffer + bufsize - 1;

  while (bufptr < bufend)
  {
    if (isspace(*start & 255) && !level)
      break;

    *bufptr++ = *start;

    if (*start == '(')
      level ++;
    else if (*start == ')')
    {
      if (!level)
      {
        start ++;
        break;
      }
      else
        level --;
    }
    else if (*start == '\\')
    {
     /*
      * Copy escaped character...
      */

      int	i;			/* Looping var */


      for (i = 1;
           i <= 3 && isdigit(start[i] & 255) && bufptr < bufend;
	   *bufptr++ = start[i], i ++);
    }

    start ++;
  }

  *bufptr = '\0';

 /*
  * Return the value and new pointer into the line...
  */

  if (end)
    *end = (char *)start;

  if (bufptr == bufend)
    return (NULL);
  else
    return (buffer);
}


/*
 * 'set_pstops_options()' - Set pstops options.
 */

static void
set_pstops_options(
    pstops_doc_t  *doc,			/* I - Document information */
    ppd_file_t    *ppd,			/* I - PPD file */
    char          *argv[],		/* I - Command-line arguments */
    int           num_options,		/* I - Number of options */
    cups_option_t *options)		/* I - Options */
{
  const char	*val;			/* Option value */
  int		intval;			/* Integer option value */
  ppd_attr_t	*attr;			/* PPD attribute */
  ppd_option_t	*option;		/* PPD option */
  ppd_choice_t	*choice;		/* PPD choice */
  const char	*content_type;		/* Original content type */
  int		max_copies;		/* Maximum number of copies supported */


 /*
  * Initialize document information structure...
  */

  memset(doc, 0, sizeof(pstops_doc_t));

  doc->job_id = atoi(argv[1]);
  doc->user   = argv[2];
  doc->title  = argv[3];
  doc->copies = atoi(argv[4]);

  if (ppd && ppd->landscape > 0)
    doc->normal_landscape = 1;

  doc->bounding_box[0] = (int)PageLeft;
  doc->bounding_box[1] = (int)PageBottom;
  doc->bounding_box[2] = (int)PageRight;
  doc->bounding_box[3] = (int)PageTop;

  doc->new_bounding_box[0] = INT_MAX;
  doc->new_bounding_box[1] = INT_MAX;
  doc->new_bounding_box[2] = INT_MIN;
  doc->new_bounding_box[3] = INT_MIN;

 /*
  * AP_FIRSTPAGE_* and the corresponding non-first-page options.
  */

  doc->ap_input_slot  = cupsGetOption("AP_FIRSTPAGE_InputSlot", num_options,
                                      options);
  doc->ap_manual_feed = cupsGetOption("AP_FIRSTPAGE_ManualFeed", num_options,
                                      options);
  doc->ap_media_color = cupsGetOption("AP_FIRSTPAGE_MediaColor", num_options,
                                      options);
  doc->ap_media_type  = cupsGetOption("AP_FIRSTPAGE_MediaType", num_options,
                                      options);
  doc->ap_page_region = cupsGetOption("AP_FIRSTPAGE_PageRegion", num_options,
                                      options);
  doc->ap_page_size   = cupsGetOption("AP_FIRSTPAGE_PageSize", num_options,
                                      options);

  if ((choice = ppdFindMarkedChoice(ppd, "InputSlot")) != NULL)
    doc->input_slot = choice->choice;
  if ((choice = ppdFindMarkedChoice(ppd, "ManualFeed")) != NULL)
    doc->manual_feed = choice->choice;
  if ((choice = ppdFindMarkedChoice(ppd, "MediaColor")) != NULL)
    doc->media_color = choice->choice;
  if ((choice = ppdFindMarkedChoice(ppd, "MediaType")) != NULL)
    doc->media_type = choice->choice;
  if ((choice = ppdFindMarkedChoice(ppd, "PageRegion")) != NULL)
    doc->page_region = choice->choice;
  if ((choice = ppdFindMarkedChoice(ppd, "PageSize")) != NULL)
    doc->page_size = choice->choice;

 /*
  * collate, multiple-document-handling
  */

  if ((val = cupsGetOption("multiple-document-handling", num_options, options)) != NULL)
  {
   /*
    * This IPP attribute is unnecessarily complicated...
    *
    *   single-document, separate-documents-collated-copies, and
    *   single-document-new-sheet all require collated copies.
    *
    *   separate-documents-uncollated-copies allows for uncollated copies.
    */

    doc->collate = _cups_strcasecmp(val, "separate-documents-uncollated-copies") != 0;
  }

  if ((val = cupsGetOption("Collate", num_options, options)) != NULL &&
      (!_cups_strcasecmp(val, "true") ||!_cups_strcasecmp(val, "on") ||
       !_cups_strcasecmp(val, "yes")))
    doc->collate = 1;

 /*
  * emit-jcl
  */

  if ((val = cupsGetOption("emit-jcl", num_options, options)) != NULL &&
      (!_cups_strcasecmp(val, "false") || !_cups_strcasecmp(val, "off") ||
       !_cups_strcasecmp(val, "no") || !strcmp(val, "0")))
    doc->emit_jcl = 0;
  else
    doc->emit_jcl = 1;

 /*
  * fit-to-page/ipp-attribute-fidelity
  *
  * (Only for original PostScript content)
  */

  if ((content_type = getenv("CONTENT_TYPE")) == NULL)
    content_type = "application/postscript";

  if (!_cups_strcasecmp(content_type, "application/postscript"))
  {
    if ((val = cupsGetOption("fit-to-page", num_options, options)) != NULL &&
	!_cups_strcasecmp(val, "true"))
      doc->fit_to_page = 1;
    else if ((val = cupsGetOption("ipp-attribute-fidelity", num_options,
                                  options)) != NULL &&
	     !_cups_strcasecmp(val, "true"))
      doc->fit_to_page = 1;
  }

 /*
  * mirror/MirrorPrint
  */

  if ((choice = ppdFindMarkedChoice(ppd, "MirrorPrint")) != NULL)
  {
    val = choice->choice;
    choice->marked = 0;
  }
  else
    val = cupsGetOption("mirror", num_options, options);

  if (val && (!_cups_strcasecmp(val, "true") || !_cups_strcasecmp(val, "on") ||
              !_cups_strcasecmp(val, "yes")))
    doc->mirror = 1;

 /*
  * number-up
  */

  if ((val = cupsGetOption("number-up", num_options, options)) != NULL)
  {
    switch (intval = atoi(val))
    {
      case 1 :
      case 2 :
      case 4 :
      case 6 :
      case 9 :
      case 16 :
          doc->number_up = intval;
	  break;
      default :
          _cupsLangPrintFilter(stderr, "ERROR",
	                       _("Unsupported number-up value %d, using "
				 "number-up=1."), intval);
          doc->number_up = 1;
	  break;
    }
  }
  else
    doc->number_up = 1;

 /*
  * number-up-layout
  */

  if ((val = cupsGetOption("number-up-layout", num_options, options)) != NULL)
  {
    if (!_cups_strcasecmp(val, "lrtb"))
      doc->number_up_layout = PSTOPS_LAYOUT_LRTB;
    else if (!_cups_strcasecmp(val, "lrbt"))
      doc->number_up_layout = PSTOPS_LAYOUT_LRBT;
    else if (!_cups_strcasecmp(val, "rltb"))
      doc->number_up_layout = PSTOPS_LAYOUT_RLTB;
    else if (!_cups_strcasecmp(val, "rlbt"))
      doc->number_up_layout = PSTOPS_LAYOUT_RLBT;
    else if (!_cups_strcasecmp(val, "tblr"))
      doc->number_up_layout = PSTOPS_LAYOUT_TBLR;
    else if (!_cups_strcasecmp(val, "tbrl"))
      doc->number_up_layout = PSTOPS_LAYOUT_TBRL;
    else if (!_cups_strcasecmp(val, "btlr"))
      doc->number_up_layout = PSTOPS_LAYOUT_BTLR;
    else if (!_cups_strcasecmp(val, "btrl"))
      doc->number_up_layout = PSTOPS_LAYOUT_BTRL;
    else
    {
      _cupsLangPrintFilter(stderr, "ERROR",
                           _("Unsupported number-up-layout value %s, using "
			     "number-up-layout=lrtb."), val);
      doc->number_up_layout = PSTOPS_LAYOUT_LRTB;
    }
  }
  else
    doc->number_up_layout = PSTOPS_LAYOUT_LRTB;

 /*
  * OutputOrder
  */

  if ((val = cupsGetOption("OutputOrder", num_options, options)) != NULL)
  {
    if (!_cups_strcasecmp(val, "Reverse"))
      doc->output_order = 1;
  }
  else if (ppd)
  {
   /*
    * Figure out the right default output order from the PPD file...
    */

    if ((choice = ppdFindMarkedChoice(ppd, "OutputBin")) != NULL &&
        (attr = ppdFindAttr(ppd, "PageStackOrder", choice->choice)) != NULL &&
	attr->value)
      doc->output_order = !_cups_strcasecmp(attr->value, "Reverse");
    else if ((attr = ppdFindAttr(ppd, "DefaultOutputOrder", NULL)) != NULL &&
             attr->value)
      doc->output_order = !_cups_strcasecmp(attr->value, "Reverse");
  }

 /*
  * page-border
  */

  if ((val = cupsGetOption("page-border", num_options, options)) != NULL)
  {
    if (!_cups_strcasecmp(val, "none"))
      doc->page_border = PSTOPS_BORDERNONE;
    else if (!_cups_strcasecmp(val, "single"))
      doc->page_border = PSTOPS_BORDERSINGLE;
    else if (!_cups_strcasecmp(val, "single-thick"))
      doc->page_border = PSTOPS_BORDERSINGLE2;
    else if (!_cups_strcasecmp(val, "double"))
      doc->page_border = PSTOPS_BORDERDOUBLE;
    else if (!_cups_strcasecmp(val, "double-thick"))
      doc->page_border = PSTOPS_BORDERDOUBLE2;
    else
    {
      _cupsLangPrintFilter(stderr, "ERROR",
                           _("Unsupported page-border value %s, using "
			     "page-border=none."), val);
      doc->page_border = PSTOPS_BORDERNONE;
    }
  }
  else
    doc->page_border = PSTOPS_BORDERNONE;

 /*
  * page-label
  */

  doc->page_label = cupsGetOption("page-label", num_options, options);

 /*
  * page-ranges
  */

  doc->page_ranges = cupsGetOption("page-ranges", num_options, options);

 /*
  * page-set
  */

  doc->page_set = cupsGetOption("page-set", num_options, options);

 /*
  * Now figure out if we have to force collated copies, etc.
  */

  if ((attr = ppdFindAttr(ppd, "cupsMaxCopies", NULL)) != NULL)
    max_copies = atoi(attr->value);
  else if (ppd && ppd->manual_copies)
    max_copies = 1;
  else
    max_copies = 9999;

  if (doc->copies > max_copies)
    doc->collate = 1;
  else if (ppd && ppd->manual_copies && Duplex && doc->copies > 1)
  {
   /*
    * Force collated copies when printing a duplexed document to
    * a non-PS printer that doesn't do hardware copy generation.
    * Otherwise the copies will end up on the front/back side of
    * each page.
    */

    doc->collate = 1;
  }

 /*
  * See if we have to filter the fast or slow way...
  */

  if (doc->collate && doc->copies > 1)
  {
   /*
    * See if we need to manually collate the pages...
    */

    doc->slow_collate = 1;

    if (doc->copies <= max_copies &&
        (choice = ppdFindMarkedChoice(ppd, "Collate")) != NULL &&
        !_cups_strcasecmp(choice->choice, "True"))
    {
     /*
      * Hardware collate option is selected, see if the option is
      * conflicting - if not, collate in hardware.  Otherwise,
      * turn the hardware collate option off...
      */

      if ((option = ppdFindOption(ppd, "Collate")) != NULL &&
          !option->conflicted)
	doc->slow_collate = 0;
      else
        ppdMarkOption(ppd, "Collate", "False");
    }
  }
  else
    doc->slow_collate = 0;

  if (!ppdFindOption(ppd, "OutputOrder") && doc->output_order)
    doc->slow_order = 1;
  else
    doc->slow_order = 0;

  if (Duplex &&
       (doc->slow_collate || doc->slow_order ||
        ((attr = ppdFindAttr(ppd, "cupsEvenDuplex", NULL)) != NULL &&
	 attr->value && !_cups_strcasecmp(attr->value, "true"))))
    doc->slow_duplex = 1;
  else
    doc->slow_duplex = 0;

 /*
  * Create a temporary file for page data if we need to filter slowly...
  */

  if (doc->slow_order || doc->slow_collate)
  {
    if ((doc->temp = cupsTempFile2(doc->tempfile,
                                   sizeof(doc->tempfile))) == NULL)
    {
      perror("DEBUG: Unable to create temporary file");
      exit(1);
    }
  }

 /*
  * Figure out if we should use ESPshowpage or not...
  */

  if (doc->page_label || getenv("CLASSIFICATION") || doc->number_up > 1 ||
      doc->page_border)
  {
   /*
    * Yes, use ESPshowpage...
    */

    doc->use_ESPshowpage = 1;
  }

  fprintf(stderr, "DEBUG: slow_collate=%d, slow_duplex=%d, slow_order=%d\n",
          doc->slow_collate, doc->slow_duplex, doc->slow_order);
}


/*
 * 'skip_page()' - Skip past a page that won't be printed.
 */

static ssize_t				/* O - Length of next line */
skip_page(cups_file_t *fp,		/* I - File to read from */
          char        *line,		/* I - Line buffer */
	  ssize_t     linelen,		/* I - Length of initial line */
          size_t      linesize)		/* I - Size of line buffer */
{
  int	level;				/* Embedded document level */


  level = 0;

  while ((linelen = (ssize_t)cupsFileGetLine(fp, line, linesize)) > 0)
  {
    if (level == 0 &&
        (!strncmp(line, "%%Page:", 7) || !strncmp(line, "%%Trailer", 9)))
      break;
    else if (!strncmp(line, "%%BeginDocument", 15) ||
	     !strncmp(line, "%ADO_BeginApplication", 21))
      level ++;
    else if ((!strncmp(line, "%%EndDocument", 13) ||
	      !strncmp(line, "%ADO_EndApplication", 19)) && level > 0)
      level --;
    else if (!strncmp(line, "%%BeginBinary:", 14) ||
             (!strncmp(line, "%%BeginData:", 12) &&
	      !strstr(line, "ASCII") && !strstr(line, "Hex")))
    {
     /*
      * Skip binary data...
      */

      ssize_t	bytes;			/* Bytes of data */

      bytes = atoi(strchr(line, ':') + 1);

      while (bytes > 0)
      {
	if ((size_t)bytes > linesize)
	  linelen = (ssize_t)cupsFileRead(fp, line, linesize);
	else
	  linelen = (ssize_t)cupsFileRead(fp, line, (size_t)bytes);

	if (linelen < 1)
	{
	  line[0] = '\0';
	  perror("ERROR: Early end-of-file while reading binary data");
	  return (0);
	}

	bytes -= linelen;
      }
    }
  }

  return (linelen);
}


/*
 * 'start_nup()' - Start processing for N-up printing.
 */

static void
start_nup(pstops_doc_t *doc,		/* I - Document information */
          int          number,		/* I - Page number */
	  int          show_border,	/* I - Show the border? */
	  const int    *bounding_box)	/* I - BoundingBox value */
{
  int		pos;			/* Position on page */
  int		x, y;			/* Relative position of subpage */
  double	w, l,			/* Width and length of subpage */
		tx, ty;			/* Translation values for subpage */
  double	pagew,			/* Printable width of page */
		pagel;			/* Printable height of page */
  int		bboxx,			/* BoundingBox X origin */
		bboxy,			/* BoundingBox Y origin */
		bboxw,			/* BoundingBox width */
		bboxl;			/* BoundingBox height */
  double	margin = 0;		/* Current margin for border */


  if (doc->number_up > 1)
    doc_puts(doc, "userdict/ESPsave save put\n");

  pos   = (number - 1) % doc->number_up;
  pagew = PageRight - PageLeft;
  pagel = PageTop - PageBottom;

  if (doc->fit_to_page)
  {
    bboxx = bounding_box[0];
    bboxy = bounding_box[1];
    bboxw = bounding_box[2] - bounding_box[0];
    bboxl = bounding_box[3] - bounding_box[1];
  }
  else
  {
    bboxx = 0;
    bboxy = 0;
    bboxw = (int)PageWidth;
    bboxl = (int)PageLength;
  }

  fprintf(stderr, "DEBUG: pagew = %.1f, pagel = %.1f\n", pagew, pagel);
  fprintf(stderr, "DEBUG: bboxx = %d, bboxy = %d, bboxw = %d, bboxl = %d\n",
          bboxx, bboxy, bboxw, bboxl);
  fprintf(stderr, "DEBUG: PageLeft = %.1f, PageRight = %.1f\n",
          PageLeft, PageRight);
  fprintf(stderr, "DEBUG: PageTop = %.1f, PageBottom = %.1f\n",
          PageTop, PageBottom);
  fprintf(stderr, "DEBUG: PageWidth = %.1f, PageLength = %.1f\n",
          PageWidth, PageLength);

  switch (Orientation)
  {
    case 1 : /* Landscape */
        doc_printf(doc, "%.1f 0.0 translate 90 rotate\n", PageLength);
        break;
    case 2 : /* Reverse Portrait */
        doc_printf(doc, "%.1f %.1f translate 180 rotate\n", PageWidth,
	           PageLength);
        break;
    case 3 : /* Reverse Landscape */
        doc_printf(doc, "0.0 %.1f translate -90 rotate\n", PageWidth);
        break;
  }

 /*
  * Mirror the page as needed...
  */

  if (doc->mirror)
    doc_printf(doc, "%.1f 0.0 translate -1 1 scale\n", PageWidth);

 /*
  * Offset and scale as necessary for fit_to_page/fit-to-page/number-up...
  */

  if (Duplex && doc->number_up > 1 && ((number / doc->number_up) & 1))
    doc_printf(doc, "%.1f %.1f translate\n", PageWidth - PageRight, PageBottom);
  else if (doc->number_up > 1 || doc->fit_to_page)
    doc_printf(doc, "%.1f %.1f translate\n", PageLeft, PageBottom);

  switch (doc->number_up)
  {
    default :
        if (doc->fit_to_page)
	{
          w = pagew;
          l = w * bboxl / bboxw;

          if (l > pagel)
          {
            l = pagel;
            w = l * bboxw / bboxl;
          }

          tx = 0.5 * (pagew - w);
          ty = 0.5 * (pagel - l);

	  doc_printf(doc, "%.1f %.1f translate %.3f %.3f scale\n", tx, ty,
	             w / bboxw, l / bboxl);
	}
	else
          w = PageWidth;
	break;

    case 2 :
        if (Orientation & 1)
	{
          x = pos & 1;

          if (doc->number_up_layout & PSTOPS_LAYOUT_NEGATEY)
	    x = 1 - x;

          w = pagel;
          l = w * bboxl / bboxw;

          if (l > (pagew * 0.5))
          {
            l = pagew * 0.5;
            w = l * bboxw / bboxl;
          }

          tx = 0.5 * (pagew * 0.5 - l);
          ty = 0.5 * (pagel - w);

          if (doc->normal_landscape)
            doc_printf(doc, "0.0 %.1f translate -90 rotate\n", pagel);
	  else
	    doc_printf(doc, "%.1f 0.0 translate 90 rotate\n", pagew);

          doc_printf(doc, "%.1f %.1f translate %.3f %.3f scale\n",
                     ty, tx + pagew * 0.5 * x, w / bboxw, l / bboxl);
        }
	else
	{
          x = pos & 1;

          if (doc->number_up_layout & PSTOPS_LAYOUT_NEGATEX)
	    x = 1 - x;

          l = pagew;
          w = l * bboxw / bboxl;

          if (w > (pagel * 0.5))
          {
            w = pagel * 0.5;
            l = w * bboxl / bboxw;
          }

          tx = 0.5 * (pagel * 0.5 - w);
          ty = 0.5 * (pagew - l);

          if (doc->normal_landscape)
	    doc_printf(doc, "%.1f 0.0 translate 90 rotate\n", pagew);
	  else
            doc_printf(doc, "0.0 %.1f translate -90 rotate\n", pagel);

          doc_printf(doc, "%.1f %.1f translate %.3f %.3f scale\n",
                     tx + pagel * 0.5 * x, ty, w / bboxw, l / bboxl);
        }
        break;

    case 4 :
        if (doc->number_up_layout & PSTOPS_LAYOUT_VERTICAL)
	{
	  x = (pos / 2) & 1;
          y = pos & 1;
        }
	else
	{
          x = pos & 1;
	  y = (pos / 2) & 1;
        }

        if (doc->number_up_layout & PSTOPS_LAYOUT_NEGATEX)
	  x = 1 - x;

	if (doc->number_up_layout & PSTOPS_LAYOUT_NEGATEY)
	  y = 1 - y;

        w = pagew * 0.5;
	l = w * bboxl / bboxw;

	if (l > (pagel * 0.5))
	{
	  l = pagel * 0.5;
	  w = l * bboxw / bboxl;
	}

        tx = 0.5 * (pagew * 0.5 - w);
        ty = 0.5 * (pagel * 0.5 - l);

	doc_printf(doc, "%.1f %.1f translate %.3f %.3f scale\n",
	           tx + x * pagew * 0.5, ty + y * pagel * 0.5,
	           w / bboxw, l / bboxl);
        break;

    case 6 :
        if (Orientation & 1)
	{
	  if (doc->number_up_layout & PSTOPS_LAYOUT_VERTICAL)
	  {
	    x = pos / 3;
	    y = pos % 3;

            if (doc->number_up_layout & PSTOPS_LAYOUT_NEGATEX)
	      x = 1 - x;

            if (doc->number_up_layout & PSTOPS_LAYOUT_NEGATEY)
	      y = 2 - y;
	  }
	  else
	  {
	    x = pos & 1;
	    y = pos / 2;

            if (doc->number_up_layout & PSTOPS_LAYOUT_NEGATEX)
	      x = 1 - x;

            if (doc->number_up_layout & PSTOPS_LAYOUT_NEGATEY)
	      y = 2 - y;
	  }

          w = pagel * 0.5;
          l = w * bboxl / bboxw;

          if (l > (pagew * 0.333))
          {
            l = pagew * 0.333;
            w = l * bboxw / bboxl;
          }

          tx = 0.5 * (pagel - 2 * w);
          ty = 0.5 * (pagew - 3 * l);

          if (doc->normal_landscape)
            doc_printf(doc, "0 %.1f translate -90 rotate\n", pagel);
	  else
	    doc_printf(doc, "%.1f 0 translate 90 rotate\n", pagew);

          doc_printf(doc, "%.1f %.1f translate %.3f %.3f scale\n",
                     tx + x * w, ty + y * l, l / bboxl, w / bboxw);
        }
	else
	{
	  if (doc->number_up_layout & PSTOPS_LAYOUT_VERTICAL)
	  {
	    x = pos / 2;
	    y = pos & 1;

            if (doc->number_up_layout & PSTOPS_LAYOUT_NEGATEX)
	      x = 2 - x;

            if (doc->number_up_layout & PSTOPS_LAYOUT_NEGATEY)
	      y = 1 - y;
	  }
	  else
	  {
	    x = pos % 3;
	    y = pos / 3;

            if (doc->number_up_layout & PSTOPS_LAYOUT_NEGATEX)
	      x = 2 - x;

            if (doc->number_up_layout & PSTOPS_LAYOUT_NEGATEY)
	      y = 1 - y;
	  }

          l = pagew * 0.5;
          w = l * bboxw / bboxl;

          if (w > (pagel * 0.333))
          {
            w = pagel * 0.333;
            l = w * bboxl / bboxw;
          }

	  tx = 0.5 * (pagel - 3 * w);
	  ty = 0.5 * (pagew - 2 * l);

          if (doc->normal_landscape)
	    doc_printf(doc, "%.1f 0 translate 90 rotate\n", pagew);
	  else
            doc_printf(doc, "0 %.1f translate -90 rotate\n", pagel);

          doc_printf(doc, "%.1f %.1f translate %.3f %.3f scale\n",
                     tx + w * x, ty + l * y, w / bboxw, l / bboxl);

        }
        break;

    case 9 :
        if (doc->number_up_layout & PSTOPS_LAYOUT_VERTICAL)
	{
	  x = (pos / 3) % 3;
          y = pos % 3;
        }
	else
	{
          x = pos % 3;
	  y = (pos / 3) % 3;
        }

        if (doc->number_up_layout & PSTOPS_LAYOUT_NEGATEX)
	  x = 2 - x;

	if (doc->number_up_layout & PSTOPS_LAYOUT_NEGATEY)
	  y = 2 - y;

        w = pagew * 0.333;
	l = w * bboxl / bboxw;

	if (l > (pagel * 0.333))
	{
	  l = pagel * 0.333;
	  w = l * bboxw / bboxl;
	}

        tx = 0.5 * (pagew * 0.333 - w);
        ty = 0.5 * (pagel * 0.333 - l);

	doc_printf(doc, "%.1f %.1f translate %.3f %.3f scale\n",
	           tx + x * pagew * 0.333, ty + y * pagel * 0.333,
	           w / bboxw, l / bboxl);
        break;

    case 16 :
        if (doc->number_up_layout & PSTOPS_LAYOUT_VERTICAL)
	{
	  x = (pos / 4) & 3;
          y = pos & 3;
        }
	else
	{
          x = pos & 3;
	  y = (pos / 4) & 3;
        }

        if (doc->number_up_layout & PSTOPS_LAYOUT_NEGATEX)
	  x = 3 - x;

	if (doc->number_up_layout & PSTOPS_LAYOUT_NEGATEY)
	  y = 3 - y;

        w = pagew * 0.25;
	l = w * bboxl / bboxw;

	if (l > (pagel * 0.25))
	{
	  l = pagel * 0.25;
	  w = l * bboxw / bboxl;
	}

        tx = 0.5 * (pagew * 0.25 - w);
        ty = 0.5 * (pagel * 0.25 - l);

	doc_printf(doc, "%.1f %.1f translate %.3f %.3f scale\n",
	           tx + x * pagew * 0.25, ty + y * pagel * 0.25,
	           w / bboxw, l / bboxl);
        break;
  }

 /*
  * Draw borders as necessary...
  */

  if (doc->page_border && show_border)
  {
    int		rects;			/* Number of border rectangles */
    double	fscale;			/* Scaling value for points */


    rects  = (doc->page_border & PSTOPS_BORDERDOUBLE) ? 2 : 1;
    fscale = PageWidth / w;
    margin = 2.25 * fscale;

   /*
    * Set the line width and color...
    */

    doc_puts(doc, "gsave\n");
    doc_printf(doc, "%.3f setlinewidth 0 setgray newpath\n",
               (doc->page_border & PSTOPS_BORDERTHICK) ? 0.5 * fscale :
	                                                 0.24 * fscale);

   /*
    * Draw border boxes...
    */

    for (; rects > 0; rects --, margin += 2 * fscale)
      if (doc->number_up > 1)
	doc_printf(doc, "%.1f %.1f %.1f %.1f ESPrs\n",
		   margin,
		   margin,
		   bboxw - 2 * margin,
		   bboxl - 2 * margin);
      else
	doc_printf(doc, "%.1f %.1f %.1f %.1f ESPrs\n",
        	   PageLeft + margin,
		   PageBottom + margin,
		   PageRight - PageLeft - 2 * margin,
		   PageTop - PageBottom - 2 * margin);

   /*
    * Restore pen settings...
    */

    doc_puts(doc, "grestore\n");
  }

  if (doc->fit_to_page)
  {
   /*
    * Offset the page by its bounding box...
    */

    doc_printf(doc, "%d %d translate\n", -bounding_box[0],
               -bounding_box[1]);
  }

  if (doc->fit_to_page || doc->number_up > 1)
  {
   /*
    * Clip the page to the page's bounding box...
    */

    doc_printf(doc, "%.1f %.1f %.1f %.1f ESPrc\n",
               bboxx + margin, bboxy + margin,
               bboxw - 2 * margin, bboxl - 2 * margin);
  }
}


/*
 * 'write_label_prolog()' - Write the prolog with the classification
 *                          and page label.
 */

static void
write_label_prolog(pstops_doc_t *doc,	/* I - Document info */
                   const char   *label,	/* I - Page label */
		   float        bottom,	/* I - Bottom position in points */
		   float        top,	/* I - Top position in points */
		   float        width)	/* I - Width in points */
{
  const char	*classification;	/* CLASSIFICATION environment variable */
  const char	*ptr;			/* Temporary string pointer */


 /*
  * First get the current classification...
  */

  if ((classification = getenv("CLASSIFICATION")) == NULL)
    classification = "";
  if (strcmp(classification, "none") == 0)
    classification = "";

 /*
  * If there is nothing to show, bind an empty 'write labels' procedure
  * and return...
  */

  if (!classification[0] && (label == NULL || !label[0]))
  {
    doc_puts(doc, "userdict/ESPwl{}bind put\n");
    return;
  }

 /*
  * Set the classification + page label string...
  */

  doc_puts(doc, "userdict");
  if (!strcmp(classification, "confidential"))
    doc_puts(doc, "/ESPpl(CONFIDENTIAL");
  else if (!strcmp(classification, "classified"))
    doc_puts(doc, "/ESPpl(CLASSIFIED");
  else if (!strcmp(classification, "secret"))
    doc_puts(doc, "/ESPpl(SECRET");
  else if (!strcmp(classification, "topsecret"))
    doc_puts(doc, "/ESPpl(TOP SECRET");
  else if (!strcmp(classification, "unclassified"))
    doc_puts(doc, "/ESPpl(UNCLASSIFIED");
  else
  {
    doc_puts(doc, "/ESPpl(");

    for (ptr = classification; *ptr; ptr ++)
    {
      if (*ptr < 32 || *ptr > 126)
        doc_printf(doc, "\\%03o", *ptr);
      else if (*ptr == '_')
        doc_puts(doc, " ");
      else if (*ptr == '(' || *ptr == ')' || *ptr == '\\')
	doc_printf(doc, "\\%c", *ptr);
      else
        doc_printf(doc, "%c", *ptr);
    }
  }

  if (label)
  {
    if (classification[0])
      doc_puts(doc, " - ");

   /*
    * Quote the label string as needed...
    */

    for (ptr = label; *ptr; ptr ++)
    {
      if (*ptr < 32 || *ptr > 126)
        doc_printf(doc, "\\%03o", *ptr);
      else if (*ptr == '(' || *ptr == ')' || *ptr == '\\')
	doc_printf(doc, "\\%c", *ptr);
      else
        doc_printf(doc, "%c", *ptr);
    }
  }

  doc_puts(doc, ")put\n");

 /*
  * Then get a 14 point Helvetica-Bold font...
  */

  doc_puts(doc, "userdict/ESPpf /Helvetica-Bold findfont 14 scalefont put\n");

 /*
  * Finally, the procedure to write the labels on the page...
  */

  doc_puts(doc, "userdict/ESPwl{\n");
  doc_puts(doc, "  ESPpf setfont\n");
  doc_printf(doc, "  ESPpl stringwidth pop dup 12 add exch -0.5 mul %.0f add\n",
             width * 0.5f);
  doc_puts(doc, "  1 setgray\n");
  doc_printf(doc, "  dup 6 sub %.0f 3 index 20 ESPrf\n", bottom - 2.0);
  doc_printf(doc, "  dup 6 sub %.0f 3 index 20 ESPrf\n", top - 18.0);
  doc_puts(doc, "  0 setgray\n");
  doc_printf(doc, "  dup 6 sub %.0f 3 index 20 ESPrs\n", bottom - 2.0);
  doc_printf(doc, "  dup 6 sub %.0f 3 index 20 ESPrs\n", top - 18.0);
  doc_printf(doc, "  dup %.0f moveto ESPpl show\n", bottom + 2.0);
  doc_printf(doc, "  %.0f moveto ESPpl show\n", top - 14.0);
  doc_puts(doc, "pop\n");
  doc_puts(doc, "}bind put\n");
}


/*
 * 'write_labels()' - Write the actual page labels.
 *
 * This function is a copy of the one in common.c since we need to
 * use doc_puts/doc_printf instead of puts/printf...
 */

static void
write_labels(pstops_doc_t *doc,		/* I - Document information */
             int          orient)	/* I - Orientation of the page */
{
  float	width,				/* Width of page */
	length;				/* Length of page */


  doc_puts(doc, "gsave\n");

  if ((orient ^ Orientation) & 1)
  {
    width  = PageLength;
    length = PageWidth;
  }
  else
  {
    width  = PageWidth;
    length = PageLength;
  }

  switch (orient & 3)
  {
    case 1 : /* Landscape */
        doc_printf(doc, "%.1f 0.0 translate 90 rotate\n", length);
        break;
    case 2 : /* Reverse Portrait */
        doc_printf(doc, "%.1f %.1f translate 180 rotate\n", width, length);
        break;
    case 3 : /* Reverse Landscape */
        doc_printf(doc, "0.0 %.1f translate -90 rotate\n", width);
        break;
  }

  doc_puts(doc, "ESPwl\n");
  doc_puts(doc, "grestore\n");
}


/*
 * 'write_options()' - Write options provided via %%IncludeFeature.
 */

static void
write_options(
    pstops_doc_t  *doc,		/* I - Document */
    ppd_file_t    *ppd,		/* I - PPD file */
    int           num_options,	/* I - Number of options */
    cups_option_t *options)	/* I - Options */
{
  int		i;		/* Looping var */
  ppd_option_t	*option;	/* PPD option */
  float		min_order;	/* Minimum OrderDependency value */
  char		*doc_setup,	/* DocumentSetup commands to send */
		*any_setup;	/* AnySetup commands to send */


 /*
  * Figure out the minimum OrderDependency value...
  */

  if ((option = ppdFindOption(ppd, "PageRegion")) != NULL)
    min_order = option->order;
  else
    min_order = 999.0f;

  for (i = 0; i < num_options; i ++)
    if ((option = ppdFindOption(ppd, options[i].name)) != NULL &&
	option->order < min_order)
      min_order = option->order;

 /*
  * Mark and extract them...
  */

  cupsMarkOptions(ppd, num_options, options);

  doc_setup = ppdEmitString(ppd, PPD_ORDER_DOCUMENT, min_order);
  any_setup = ppdEmitString(ppd, PPD_ORDER_ANY, min_order);

 /*
  * Then send them out...
  */

  if (doc->number_up > 1)
  {
   /*
    * Temporarily restore setpagedevice so we can set the options...
    */

    doc_puts(doc, "userdict/setpagedevice/CUPSsetpagedevice load put\n");
  }

  if (doc_setup)
  {
    doc_puts(doc, doc_setup);
    free(doc_setup);
  }

  if (any_setup)
  {
    doc_puts(doc, any_setup);
    free(any_setup);
  }

  if (doc->number_up > 1)
  {
   /*
    * Disable setpagedevice again...
    */

    doc_puts(doc, "userdict/setpagedevice{pop}bind put\n");
  }
}


/*
 * End of "$Id: pstops.c 12655 2015-05-22 17:26:40Z msweet $".
 */
