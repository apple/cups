/*
 * "$Id$"
 *
 *   PostScript filter for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-2006 by Easy Software Products.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main()               - Main entry...
 *   add_page()           - Add a page to the pages array...
 *   check_range()        - Check to see if the current page is selected for
 *   copy_bytes()         - Copy bytes from the input file to stdout...
 *   copy_comments()      - Copy all of the comments section...
 *   copy_dsc()           - Copy a DSC-conforming document...
 *   copy_non_dsc()       - Copy a document that does not conform to the DSC...
 *   copy_page()          - Copy a page description...
 *   copy_prolog()        - Copy the document prolog section...
 *   copy_setup()         - Copy the document setup section...
 *   copy_trailer()       - Copy the document trailer...
 *   do_prolog()          - Send the necessary document prolog commands...
 *   do_setup()           - Send the necessary document setup commands...
 *   doc_printf()         - Send a formatted string to stdout and/or the temp
 *                          file.
 *   doc_puts()           - Send a nul-terminated string to stdout and/or the
 *                          temp file.
 *   doc_write()          - Send data to stdout and/or the temp file.
 *   end_nup()            - End processing for N-up printing...
 *   include_feature()    - Include a printer option/feature command.
 *   parse_text()         - Parse a text value in a comment...
 *   set_pstops_options() - Set pstops options...
 *   skip_page()          - Skip past a page that won't be printed...
 *   start_nup()          - Start processing for N-up printing...
 */

/*
 * Include necessary headers...
 */

#include "common.h"
#include <math.h>
#include <cups/file.h>
#include <cups/array.h>


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
		*ap_manual_feed;	/* AP_FIRSTPAGE_ManualFeed value */
  float		brightness;		/* brightness value */
  int		collate,		/* Collate copies? */
		emit_jcl,		/* Emit JCL commands? */
		fitplot;		/* Fit pages to media */
  float		gamma;			/* gamma value */
  const char	*input_slot,		/* InputSlot value */
		*manual_feed;		/* ManualFeed value */
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
 * Local functions...
 */

static pstops_page_t	*add_page(pstops_doc_t *doc, const char *label);
static int		check_range(pstops_doc_t *doc, int page);
static void		copy_bytes(cups_file_t *fp, off_t offset,
			           size_t length);
static size_t		copy_comments(cups_file_t *fp, pstops_doc_t *doc,
			              char *line, size_t linelen,
				      size_t linesize);
static void		copy_dsc(cups_file_t *fp, pstops_doc_t *doc,
			         ppd_file_t *ppd, char *line, size_t linelen,
				 size_t linesize);
static void		copy_non_dsc(cups_file_t *fp, pstops_doc_t *doc,
			             ppd_file_t *ppd, char *line,
				     size_t linelen, size_t linesize);
static size_t		copy_page(cups_file_t *fp, pstops_doc_t *doc,
			          ppd_file_t *ppd, int number, char *line,
				  size_t linelen, size_t linesize);
static size_t		copy_prolog(cups_file_t *fp, pstops_doc_t *doc,
			            ppd_file_t *ppd, char *line,
				    size_t linelen, size_t linesize);
static size_t		copy_setup(cups_file_t *fp, pstops_doc_t *doc,
			           ppd_file_t *ppd, char *line,
				   size_t linelen, size_t linesize);
static size_t		copy_trailer(cups_file_t *fp, pstops_doc_t *doc,
			             ppd_file_t *ppd, int number, char *line,
				     size_t linelen, size_t linesize);
static void		do_prolog(pstops_doc_t *doc, ppd_file_t *ppd);
static void 		do_setup(pstops_doc_t *doc, ppd_file_t *ppd);
static void		doc_printf(pstops_doc_t *doc, const char *format, ...);
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
static size_t		skip_page(cups_file_t *fp, char *line, size_t linelen,
				  size_t linesize);
static void		start_nup(pstops_doc_t *doc, int number,
				  int show_border, const int *bounding_box);


/*
 * 'main()' - Main entry...
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
  size_t	len;			/* Length of line buffer */


 /*
  * Make sure status messages are not buffered...
  */

  setbuf(stderr, NULL);

 /*
  * Check command-line...
  */

  if (argc < 6 || argc > 7)
  {
    fputs("ERROR: pstops job-id user title copies options [file]\n", stderr);
    return (1);
  }

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
      fprintf(stderr, "ERROR: Unable to open print file \"%s\" - %s\n",
              argv[6], strerror(errno));
      return (1);
    }
  }

 /*
  * Read the first line to see if we have DSC comments...
  */

  if ((len = cupsFileGetLine(fp, line, sizeof(line))) == 0)
  {
    fputs("ERROR: Empty print file!\n", stderr);
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

    while (strstr(line, "ENTER LANGUAGE") == NULL)
      if ((len = cupsFileGetLine(fp, line, sizeof(line))) == 0)
        break;

    if ((len = cupsFileGetLine(fp, line, sizeof(line))) == 0)
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
 * 'add_page()' - Add a page to the pages array...
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
    fprintf(stderr, "EMERG: Unable to allocate memory for pages array: %s\n",
            strerror(errno));
    exit(1);
  }

  if ((pageinfo = calloc(1, sizeof(pstops_page_t))) == NULL)
  {
    fprintf(stderr, "EMERG: Unable to allocate memory for page info: %s\n",
            strerror(errno));
    exit(1);
  }

  pageinfo->label  = strdup(label);
  pageinfo->offset = cupsFileTell(doc->temp);

  cupsArrayAdd(doc->pages, pageinfo);

  doc->page ++;

  return (pageinfo);
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

    if (!strcasecmp(doc->page_set, "even") &&
        ((page - 1) % (doc->number_up << 1)) < doc->number_up)
      return (0);

    if (!strcasecmp(doc->page_set, "odd") &&
        ((page - 1) % (doc->number_up << 1)) >= doc->number_up)
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
      upper = strtol(range, (char **)&range, 10);
    }
    else
    {
      lower = strtol(range, (char **)&range, 10);

      if (*range == '-')
      {
        range ++;
	if (!isdigit(*range & 255))
	  upper = 65535;
	else
	  upper = strtol(range, (char **)&range, 10);
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
 * 'copy_bytes()' - Copy bytes from the input file to stdout...
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
    fprintf(stderr, "ERROR: Unable to seek to offset " CUPS_LLFMT
                    " in file - %s\n",
            CUPS_LLCAST offset, strerror(errno));
    return;
  }

  while (nleft > 0 || length == 0)
  {
    if (nleft > sizeof(buffer) || length == 0)
      nbytes = sizeof(buffer);
    else
      nbytes = nleft;

    if ((nbytes = cupsFileRead(fp, buffer, nbytes)) < 1)
      return;

    nleft -= nbytes;

    fwrite(buffer, 1, nbytes, stdout);
  }
}


/*
 * 'copy_comments()' - Copy all of the comments section...
 *
 * This function expects "line" to be filled with a comment line.
 * On return, "line" will contain the next line in the file, if any.
 */

static size_t				/* O - Length of next line */
copy_comments(cups_file_t  *fp,		/* I - File to read from */
              pstops_doc_t *doc,	/* I - Document info */
              char         *line,	/* I - Line buffer */
	      size_t       linelen,	/* I - Length of initial line */
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
      if (saw_pages)
        fputs("ERROR: Duplicate %%Pages: comment seen!\n", stderr);

      saw_pages = 1;
    }
    else if (!strncmp(line, "%%BoundingBox:", 14))
    {
      if (saw_bounding_box)
        fputs("ERROR: Duplicate %%BoundingBox: comment seen!\n", stderr);
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
	fputs("ERROR: Bad %%BoundingBox: comment seen!\n", stderr);

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
      printf("%s\n", line);
    }
    else if (!strncmp(line, "%%Title:", 8))
    {
      saw_title = 1;
      printf("%s\n", line);
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
      linelen = cupsFileGetLine(fp, line, linesize);
      break;
    }
    else if (strncmp(line, "%!", 2) && strncmp(line, "%cups", 5))
      printf("%s\n", line);

    if ((linelen = cupsFileGetLine(fp, line, linesize)) == 0)
      break;
  }

  if (!saw_bounding_box)
    fputs("ERROR: No %%BoundingBox: comment in header!\n", stderr);

  if (!saw_pages)
    fputs("ERROR: No %%Pages: comment in header!\n", stderr);

  if (!saw_for)
    printf("%%%%For: %s\n", doc->user);

  if (!saw_title)
    printf("%%%%Title: %s\n", doc->title);

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

  puts("%%Pages: (atend)");
  puts("%%BoundingBox: (atend)");
  puts("%%EndComments");

  return (linelen);
}


/*
 * 'copy_dsc()' - Copy a DSC-conforming document...
 *
 * This function expects "line" to be filled with the %!PS-Adobe comment line.
 */

static void
copy_dsc(cups_file_t  *fp,		/* I - File to read from */
         pstops_doc_t *doc,		/* I - Document info */
         ppd_file_t   *ppd,		/* I - PPD file */
	 char         *line,		/* I - Line buffer */
	 size_t       linelen,		/* I - Length of initial line */
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
  linelen = copy_comments(fp, doc, line, linelen, linesize);

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
    fwrite(line, 1, linelen, stdout);

    if ((linelen = cupsFileGetLine(fp, line, linesize)) == 0)
      break;
  }

 /*
  * Then process pages until we have no more...
  */

  number = 0;

  fprintf(stderr, "DEBUG: Before page loop - %s", line);
  while (!strncmp(line, "%%Page:", 7))
  {
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

  if (number && is_not_last_page(number))
  {
    pageinfo = (pstops_page_t *)cupsArrayLast(doc->pages);

    start_nup(doc, doc->number_up, 0, doc->bounding_box);
    doc_puts(doc, "showpage\n");
    end_nup(doc, doc->number_up);

    pageinfo->length = cupsFileTell(doc->temp) - pageinfo->offset;
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

    pageinfo->length = cupsFileTell(doc->temp) - pageinfo->offset;
  }

 /*
  * Make additional copies as necessary...
  */

  number = doc->slow_order ? 0 : doc->page;

  if (doc->temp)
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

    for (copy = !doc->slow_order; copy < doc->copies; copy ++)
    {
      pageinfo = doc->slow_order ? (pstops_page_t *)cupsArrayLast(doc->pages) :
                                   (pstops_page_t *)cupsArrayFirst(doc->pages);

      while (pageinfo)
      {
        number ++;

	if (!ppd || !ppd->num_filters)
	  fprintf(stderr, "PAGE: %d 1\n", number);

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

	copy_bytes(doc->temp, pageinfo->offset, pageinfo->length);

	pageinfo = doc->slow_order ? (pstops_page_t *)cupsArrayPrev(doc->pages) :
                                     (pstops_page_t *)cupsArrayNext(doc->pages);
      }
    }
  }

 /*
  * Write/copy the trailer...
  */

  linelen = copy_trailer(fp, doc, ppd, number, line, linelen, linesize);
}


/*
 * 'copy_non_dsc()' - Copy a document that does not conform to the DSC...
 *
 * This function expects "line" to be filled with the %! comment line.
 */

static void
copy_non_dsc(cups_file_t  *fp,		/* I - File to read from */
             pstops_doc_t *doc,		/* I - Document info */
             ppd_file_t   *ppd,		/* I - PPD file */
	     char         *line,	/* I - Line buffer */
	     size_t       linelen,	/* I - Length of initial line */
	     size_t       linesize)	/* I - Size of line buffer */
{
  int	copy;				/* Current copy */
  char	buffer[8192];			/* Copy buffer */
  int	bytes;				/* Number of bytes copied */


 /*
  * First let the user know that they are attempting to print a file
  * that may not print correctly...
  */

  fputs("WARNING: This document does not conform to the Adobe Document "
        "Structuring Conventions and may not print correctly!\n", stderr);

 /*
  * Then write a standard DSC comment section...
  */

  printf("%%%%BoundingBox: %.0f %.0f %.0f %.0f\n", PageLeft, PageBottom,
         PageRight, PageTop);

  if (doc->slow_collate && doc->copies > 1)
    printf("%%%%Pages: %d\n", doc->copies);
  else
    puts("%%Pages: 1");

  printf("%%%%For: %s\n", doc->user);
  printf("%%%%Title: %s\n", doc->title);

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

  fwrite(line, linelen, 1, stdout);

  if (doc->temp)
    cupsFileWrite(doc->temp, line, linelen);

  while ((bytes = cupsFileRead(fp, buffer, sizeof(buffer))) > 0)
  {
    fwrite(buffer, 1, bytes, stdout);

    if (doc->temp)
      cupsFileWrite(doc->temp, buffer, bytes);
  }

  puts("%%EndDocument");

  if (doc->use_ESPshowpage)
  {
    WriteLabels(Orientation);
    puts("ESPshowpage");
  }

  if (doc->temp)
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
}


/*
 * 'copy_page()' - Copy a page description...
 *
 * This function expects "line" to be filled with a %%Page comment line.
 * On return, "line" will contain the next line in the file, if any.
 */

static size_t				/* O - Length of next line */
copy_page(cups_file_t  *fp,		/* I - File to read from */
          pstops_doc_t *doc,		/* I - Document info */
          ppd_file_t   *ppd,		/* I - PPD file */
	  int          number,		/* I - Current page number */
	  char         *line,		/* I - Line buffer */
	  size_t       linelen,		/* I - Length of initial line */
	  size_t       linesize)	/* I - Size of line buffer */
{
  char		label[256],		/* Page label string */
		*ptr;			/* Pointer into line */
  int		level;			/* Embedded document level */
  pstops_page_t	*pageinfo;		/* Page information */
  int		first_page;		/* First page on N-up output? */
  int		bounding_box[4];	/* PageBoundingBox */


 /*
  * Get the page label for this page...
  */

  first_page = is_first_page(number);

  if (!parse_text(line + 7, &ptr, label, sizeof(label)))
  {
    fputs("ERROR: Bad %%Page: comment in file!\n", stderr);
    label[0] = '\0';
    number   = doc->page;
  }
  else if (strtol(ptr, &ptr, 10) == LONG_MAX || !isspace(*ptr & 255))
  {
    fputs("ERROR: Bad %%Page: comment in file!\n", stderr);
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
    if (doc->page == 1)
    {
     /*
      * First page/sheet gets AP_FIRSTPAGE_* options...
      */

      pageinfo->num_options = cupsAddOption("InputSlot", doc->ap_input_slot,
                                            pageinfo->num_options,
					    &(pageinfo->options));
      pageinfo->num_options = cupsAddOption("ManualFeed", doc->ap_manual_feed,
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
      pageinfo->num_options = cupsAddOption("ManualFeed", doc->manual_feed,
                                            pageinfo->num_options,
					    &(pageinfo->options));
    }
  }

 /*
  * Scan comments until we see something other than %%Page*: or
  * %%Include*...
  */

  memcpy(bounding_box, doc->bounding_box, sizeof(bounding_box));

  while ((linelen = cupsFileGetLine(fp, line, linesize)) > 0)
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
        fputs("ERROR: Bad %%PageBoundingBox: comment in file!\n", stderr);
        memcpy(bounding_box, doc->bounding_box,
	       sizeof(bounding_box));
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

      if (doc->number_up == 1 &&!doc->fitplot)
	pageinfo->num_options = include_feature(ppd, line,
	                                        pageinfo->num_options,
                                        	&(pageinfo->options));
    }
    else if (strncmp(line, "%%Include", 9))
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

  if (!strncmp(line, "%%BeginPageSetup", 16))
  {
   /*
    * Copy page setup commands...
    */

    doc_write(doc, line, linelen);

    while ((linelen = cupsFileGetLine(fp, line, linesize)) > 0)
    {
      if (!strncmp(line, "%%EndPageSetup", 14))
        break;
      else if (!strncmp(line, "%%Include", 9))
        continue;

      if (doc->number_up == 1 && !doc->fitplot)
	doc_write(doc, line, linelen);
    }

   /*
    * Skip %%EndPageSetup...
    */

    if (linelen > 0)
      linelen = cupsFileGetLine(fp, line, linesize);

    if (pageinfo->num_options == 0)
      doc_puts(doc, "%%EndPageSetup\n");
  }
  else if (first_page && pageinfo->num_options > 0)
    doc_puts(doc, "%%BeginPageSetup\n");

  if (first_page && pageinfo->num_options > 0)
  {
    int		i;			/* Looping var */
    ppd_option_t *option;		/* PPD option */
    int		min_order;		/* Minimum OrderDependency value */
    char	*doc_setup,		/* DocumentSetup commands to send */
		*any_setup;		/* AnySetup commands to send */


   /*
    * Yes, figure out the minimum OrderDependency value...
    */

    if ((option = ppdFindOption(ppd, "PageRegion")) != NULL)
      min_order = option->order;
    else
      min_order = 999.0f;

    for (i = 0; i < pageinfo->num_options; i ++)
      if ((option = ppdFindOption(ppd, pageinfo->options[i].name)) != NULL &&
          option->order < min_order)
        min_order = option->order;

   /*
    * Mark and extract them...
    */

    cupsMarkOptions(ppd, pageinfo->num_options, pageinfo->options);

    doc_setup = ppdEmitString(ppd, PPD_ORDER_DOCUMENT, min_order);
    any_setup = ppdEmitString(ppd, PPD_ORDER_ANY, min_order);

   /*
    * Then send them out...
    */

    if (doc_setup)
      doc_puts(doc, doc_setup);

    if (any_setup)
      doc_puts(doc, any_setup);

   /*
    * Free the command strings...
    */

    if (doc_setup)
      free(doc_setup);

    if (any_setup)
      free(any_setup);

    doc_puts(doc, "%%EndPageSetup\n");
  }

 /*
  * Prep for the start of the page description...
  */

  start_nup(doc, number, 1, bounding_box);

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
      doc_write(doc, line, linelen);

      level ++;
    }
    else if ((!strncmp(line, "%%EndDocument", 13) ||
	      !strncmp(line, "%ADO_EndApplication", 19)) && level > 0)
    {
      doc_write(doc, line, linelen);

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


      doc_write(doc, line, linelen);

      bytes = atoi(strchr(line, ':') + 1);

      while (bytes > 0)
      {
	if (bytes > linesize)
	  linelen = cupsFileRead(fp, line, linesize);
	else
	  linelen = cupsFileRead(fp, line, bytes);

	if (linelen < 1)
	{
	  line[0] = '\0';
	  perror("ERROR: Early end-of-file while reading binary data");
	  return (0);
	}

        doc_write(doc, line, linelen);

	bytes -= linelen;
      }
    }
    else
      doc_write(doc, line, linelen);
  }
  while ((linelen = cupsFileGetLine(fp, line, linesize)) > 0);

 /*
  * Finish up this page and return...
  */

  end_nup(doc, number);

  pageinfo->length = cupsFileTell(doc->temp) - pageinfo->offset;

  return (linelen);
}


/*
 * 'copy_prolog()' - Copy the document prolog section...
 *
 * This function expects "line" to be filled with a %%BeginProlog comment line.
 * On return, "line" will contain the next line in the file, if any.
 */

static size_t				/* O - Length of next line */
copy_prolog(cups_file_t  *fp,		/* I - File to read from */
            pstops_doc_t *doc,		/* I - Document info */
            ppd_file_t   *ppd,		/* I - PPD file */
	    char         *line,		/* I - Line buffer */
	    size_t       linelen,	/* I - Length of initial line */
	    size_t       linesize)	/* I - Size of line buffer */
{
  while (strncmp(line, "%%BeginProlog", 13))
  {
    if (!strncmp(line, "%%BeginSetup", 12) || !strncmp(line, "%%Page:", 7))
      break;

    fwrite(line, 1, linelen, stdout);

    if ((linelen = cupsFileGetLine(fp, line, linesize)) == 0)
      break;
  }

  puts("%%BeginProlog");

  do_prolog(doc, ppd);

  if (!strncmp(line, "%%BeginProlog", 13))
  {
    while ((linelen = cupsFileGetLine(fp, line, linesize)) > 0)
    {
      if (!strncmp(line, "%%EndProlog", 11) ||
          !strncmp(line, "%%BeginSetup", 12) ||
          !strncmp(line, "%%Page:", 7))
        break;

      fwrite(line, 1, linelen, stdout);
    }

    if (!strncmp(line, "%%EndProlog", 11))
      linelen = cupsFileGetLine(fp, line, linesize);
    else
      fputs("ERROR: Missing %%EndProlog!\n", stderr);
  }

  puts("%%EndProlog");

  return (linelen);
}


/*
 * 'copy_setup()' - Copy the document setup section...
 *
 * This function expects "line" to be filled with a %%BeginSetup comment line.
 * On return, "line" will contain the next line in the file, if any.
 */

static size_t				/* O - Length of next line */
copy_setup(cups_file_t  *fp,		/* I - File to read from */
           pstops_doc_t *doc,		/* I - Document info */
           ppd_file_t   *ppd,		/* I - PPD file */
	   char         *line,		/* I - Line buffer */
	   size_t       linelen,	/* I - Length of initial line */
	   size_t       linesize)	/* I - Size of line buffer */
{
  while (strncmp(line, "%%BeginSetup", 12))
  {
    if (!strncmp(line, "%%Page:", 7))
      break;

    fwrite(line, 1, linelen, stdout);

    if ((linelen = cupsFileGetLine(fp, line, linesize)) == 0)
      break;
  }

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

        if (doc->number_up == 1 && !doc->fitplot)
	  doc->num_options = include_feature(ppd, line, doc->num_options,
                                             &(doc->options));
      }
      else
        fwrite(line, 1, linelen, stdout);

      if ((linelen = cupsFileGetLine(fp, line, linesize)) == 0)
	break;
    }

    if (!strncmp(line, "%%EndSetup", 10))
      linelen = cupsFileGetLine(fp, line, linesize);
    else
      fputs("ERROR: Missing %%EndSetup!\n", stderr);
  }
  else
    puts("%%BeginSetup");

  do_setup(doc, ppd);

  puts("%%EndSetup");

  return (linelen);
}


/*
 * 'copy_trailer()' - Copy the document trailer...
 *
 * This function expects "line" to be filled with a %%Trailer comment line.
 * On return, "line" will contain the next line in the file, if any.
 */

static size_t				/* O - Length of next line */
copy_trailer(cups_file_t  *fp,		/* I - File to read from */
             pstops_doc_t *doc,		/* I - Document info */
             ppd_file_t   *ppd,		/* I - PPD file */
	     int          number,	/* I - Number of pages */
	     char         *line,	/* I - Line buffer */
	     size_t       linelen,	/* I - Length of initial line */
	     size_t       linesize)	/* I - Size of line buffer */
{
 /*
  * Write the trailer comments...
  */

  puts("%%Trailer");

  while (linelen > 0)
  {
    if (!strncmp(line, "%%EOF", 5))
      break;
    else if (strncmp(line, "%%Trailer", 9) &&
             strncmp(line, "%%Pages:", 8) &&
             strncmp(line, "%%BoundingBox:", 14))
      fwrite(line, 1, linelen, stdout);

    linelen = cupsFileGetLine(fp, line, linesize);
  }

  fprintf(stderr, "DEBUG: Wrote %d pages...\n", number);

  printf("%%%%Pages: %d\n", number);
  if (doc->number_up > 1 || doc->fitplot)
    printf("%%%%BoundingBox: %.0f %.0f %.0f %.0f\n",
	   PageLeft, PageBottom, PageRight, PageTop);
  else
    printf("%%%%BoundingBox: %d %d %d %d\n",
	   doc->new_bounding_box[0], doc->new_bounding_box[1],
	   doc->new_bounding_box[2], doc->new_bounding_box[3]);

  return (linelen);
}


/*
 * 'do_prolog()' - Send the necessary document prolog commands...
 */

static void
do_prolog(pstops_doc_t *doc,		/* I - Document information */
          ppd_file_t   *ppd)		/* I - PPD file */
{
 /*
  * Send the document prolog commands...
  */

  if (ppd && ppd->patches)
  {
    puts("%%BeginFeature: *JobPatchFile 1");
    puts(ppd->patches);
    puts("%%EndFeature");
  }

  ppdEmit(ppd, stdout, PPD_ORDER_PROLOG);

 /*
  * Define ESPshowpage here so that applications that define their
  * own procedure to do a showpage pick it up...
  */

  if (doc->use_ESPshowpage)
    puts("userdict/ESPshowpage/showpage load put\n"
	 "userdict/showpage{}put");

}


/*
 * 'do_setup()' - Send the necessary document setup commands...
 */

static void
do_setup(pstops_doc_t *doc,		/* I - Document information */
         ppd_file_t   *ppd)		/* I - PPD file */
{
 /*
  * Mark any options from %%IncludeFeature: comments...
  */

  cupsMarkOptions(ppd, doc->num_options, doc->options);

 /*
  * Send all the printer-specific setup commands...
  */

  ppdEmit(ppd, stdout, PPD_ORDER_DOCUMENT);
  ppdEmit(ppd, stdout, PPD_ORDER_ANY);

 /*
  * Set the number of copies for the job...
  */

  if (doc->copies != 1 && (!doc->collate || !doc->slow_collate))
  {
    printf("%%RBIBeginNonPPDFeature: *NumCopies %d\n", doc->copies);
    printf("%d/languagelevel where{pop languagelevel 2 ge}{false}ifelse\n"
           "{1 dict begin/NumCopies exch def currentdict end setpagedevice}\n"
	   "{userdict/#copies 3 -1 roll put}ifelse\n", doc->copies);
    puts("%RBIEndNonPPDFeature");
  }

 /*
  * If we are doing N-up printing, disable setpagedevice...
  */

  if (doc->number_up > 1)
    puts("userdict/setpagedevice{pop}bind put");

 /*
  * Changes to the transfer function must be made AFTER any
  * setpagedevice code...
  */

  if (doc->gamma != 1.0f || doc->brightness != 1.0f)
    printf("{ neg 1 add dup 0 lt { pop 1 } { %.3f exp neg 1 add } "
	   "ifelse %.3f mul } bind settransfer\n", doc->gamma,
	   doc->brightness);

 /*
  * Make sure we have rectclip and rectstroke procedures of some sort...
  */

  WriteCommon();

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
      WriteLabelProlog(doc->page_label, PageBottom,
                       PageWidth - PageLength + PageTop, PageLength);
    else
      WriteLabelProlog(doc->page_label, PageLeft, PageRight, PageLength);
  }
  else
    WriteLabelProlog(doc->page_label, PageBottom, PageTop, PageWidth);
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
  size_t	bytes;			/* Number of bytes to write */


  va_start(ap, format);
  bytes = vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);

  if (bytes > sizeof(buffer))
  {
    fprintf(stderr,
            "ERROR: doc_printf overflow (%d bytes) detected, aborting!\n",
            (int)bytes);
    exit(1);
  }

  doc_write(doc, buffer, bytes);
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
 * 'end_nup()' - End processing for N-up printing...
 */

static void
end_nup(pstops_doc_t *doc,		/* I - Document information */
        int          number)		/* I - Page number */
{
  if (doc->mirror || Orientation || doc->number_up > 1)
    puts("userdict /ESPsave get restore");

  switch (doc->number_up)
  {
    case 1 :
	if (doc->use_ESPshowpage)
	{
	  WriteLabels(Orientation);
          puts("ESPshowpage");
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

	    WriteLabels(Orientation - 1);
	  }
	  else if (Orientation == 0)
	  {
	   /*
	    * Rotate the labels to landscape...
	    */

	    WriteLabels(doc->normal_landscape ? 1 : 3);
	  }
	  else
	  {
	   /*
	    * Rotate the labels to landscape...
	    */

	    WriteLabels(doc->normal_landscape ? 3 : 1);
	  }

          puts("ESPshowpage");
	}
        break;

    default :
	if (is_last_page(number) && doc->use_ESPshowpage)
	{
	  WriteLabels(Orientation);
          puts("ESPshowpage");
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
  ppd_choice_t	*choice;		/* Choice */


 /*
  * Get the "%%IncludeFeature: *Keyword OptionKeyword" values...
  */

  if (sscanf(line + 17, "%254s%254s", name, value) != 2)
  {
    fputs("ERROR: Bad %%IncludeFeature: comment!\n", stderr);
    return (num_options);
  }

 /*
  * Find the option and choice...
  */

  if ((option = ppdFindOption(ppd, name + 1)) == NULL)
  {
    fprintf(stderr, "WARNING: Unknown option \"%s\"!\n", name + 1);
    return (num_options);
  }

  if (option->section == PPD_ORDER_EXIT ||
      option->section == PPD_ORDER_JCL)
  {
    fprintf(stderr, "WARNING: Option \"%s\" cannot be included via "
                    "IncludeFeature!\n", name + 1);
    return (num_options);
  }

  if ((choice = ppdFindChoice(option, value)) == NULL)
  {
    fprintf(stderr, "WARNING: Unknown choice \"%s\" for option \"%s\"!\n",
            value, name + 1);
    return (num_options);
  }

 /*
  * Add the option to the option array and return...
  */

  return (cupsAddOption(name + 1, value, num_options, options));
}


/*
 * 'parse_text()' - Parse a text value in a comment...
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
 * 'set_pstops_options()' - Set pstops options...
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
  * AP_FIRSTPAGE_InputSlot
  */

  doc->ap_input_slot = cupsGetOption("AP_FIRSTPAGE_InputSlot", num_options,
                                     options);

 /*
  * AP_FIRSTPAGE_ManualFeed
  */

  doc->ap_manual_feed = cupsGetOption("AP_FIRSTPAGE_ManualFeed", num_options,
                                      options);

 /*
  * brightness
  */

  if ((val = cupsGetOption("brightness", num_options, options)) != NULL)
  {
   /*
    * Get brightness value from 10 to 1000.
    */

    intval = atoi(val);

    if (intval < 10 || intval > 1000)
    {
      fprintf(stderr, "ERROR: Unsupported brightness value %s, using "
                      "brightness=100!\n", val);
      doc->brightness = 1.0f;
    }
    else
      doc->brightness = intval * 0.01f;
  }
  else
    doc->brightness = 1.0f;

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

    doc->collate = strcasecmp(val, "separate-documents-uncollated-copies") != 0;
  }

  if ((val = cupsGetOption("Collate", num_options, options)) != NULL &&
      (!strcasecmp(val, "true") ||!strcasecmp(val, "on") ||
       !strcasecmp(val, "yes")))
    doc->collate = 1;

 /*
  * emit-jcl
  */

  if ((val = cupsGetOption("emit-jcl", num_options, options)) != NULL &&
      (!strcasecmp(val, "false") || !strcasecmp(val, "off") ||
       !strcasecmp(val, "no") || !strcmp(val, "0")))
    doc->emit_jcl = 0;
  else
    doc->emit_jcl = 1;

 /*
  * fitplot
  */

  if ((val = cupsGetOption("fitplot", num_options, options)) != NULL &&
      (!strcasecmp(val, "true") || !strcasecmp(val, "on") ||
       !strcasecmp(val, "yes")))
    doc->fitplot = 1;

 /*
  * gamma
  */

  if ((val = cupsGetOption("gamma", num_options, options)) != NULL)
  {
   /*
    * Get gamma value from 1 to 10000...
    */

    intval = atoi(val);

    if (intval < 1 || intval > 10000)
    {
      fprintf(stderr, "ERROR: Unsupported gamma value %s, using "
                      "gamma=1000!\n", val);
      doc->gamma = 1.0f;
    }
    else
      doc->gamma = intval * 0.001f;
  }
  else
    doc->gamma = 1.0f;

 /*
  * InputSlot
  */

  if ((choice = ppdFindMarkedChoice(ppd, "InputSlot")) != NULL)
    doc->input_slot = choice->choice;

 /*
  * ManualFeed
  */

  if ((choice = ppdFindMarkedChoice(ppd, "ManualFeed")) != NULL)
    doc->manual_feed = choice->choice;

  if ((val = cupsGetOption("mirror", num_options, options)) != NULL &&
      (!strcasecmp(val, "true") || !strcasecmp(val, "on") ||
       !strcasecmp(val, "yes")))
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
          fprintf(stderr,
	          "ERROR: Unsupported number-up value %d, using number-up=1!\n",
		  intval);
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
    if (!strcasecmp(val, "lrtb"))
      doc->number_up_layout = PSTOPS_LAYOUT_LRTB;
    else if (!strcasecmp(val, "lrbt"))
      doc->number_up_layout = PSTOPS_LAYOUT_LRBT;
    else if (!strcasecmp(val, "rltb"))
      doc->number_up_layout = PSTOPS_LAYOUT_RLTB;
    else if (!strcasecmp(val, "rlbt"))
      doc->number_up_layout = PSTOPS_LAYOUT_RLBT;
    else if (!strcasecmp(val, "tblr"))
      doc->number_up_layout = PSTOPS_LAYOUT_TBLR;
    else if (!strcasecmp(val, "tbrl"))
      doc->number_up_layout = PSTOPS_LAYOUT_TBRL;
    else if (!strcasecmp(val, "btlr"))
      doc->number_up_layout = PSTOPS_LAYOUT_BTLR;
    else if (!strcasecmp(val, "btrl"))
      doc->number_up_layout = PSTOPS_LAYOUT_BTRL;
    else
    {
      fprintf(stderr, "ERROR: Unsupported number-up-layout value %s, using "
                      "number-up-layout=lrtb!\n", val);
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
    if (!strcasecmp(val, "Reverse"))
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
      doc->output_order = !strcasecmp(attr->value, "Reverse");
    else if ((attr = ppdFindAttr(ppd, "DefaultOutputOrder", NULL)) != NULL &&
             attr->value)
      doc->output_order = !strcasecmp(attr->value, "Reverse");
  }

 /*
  * page-border
  */

  if ((val = cupsGetOption("page-border", num_options, options)) != NULL)
  {
    if (!strcasecmp(val, "none"))
      doc->page_border = PSTOPS_BORDERNONE;
    else if (!strcasecmp(val, "single"))
      doc->page_border = PSTOPS_BORDERSINGLE;
    else if (!strcasecmp(val, "single-thick"))
      doc->page_border = PSTOPS_BORDERSINGLE2;
    else if (!strcasecmp(val, "double"))
      doc->page_border = PSTOPS_BORDERDOUBLE;
    else if (!strcasecmp(val, "double-thick"))
      doc->page_border = PSTOPS_BORDERDOUBLE2;
    else
    {
      fprintf(stderr, "ERROR: Unsupported page-border value %s, using "
                      "page-border=none!\n", val);
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

  if (ppd && ppd->manual_copies && Duplex && doc->copies > 1)
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

    if ((choice = ppdFindMarkedChoice(ppd, "Collate")) != NULL &&
        !strcasecmp(choice->choice, "True"))
    {
     /*
      * Hardware collate option is selected, see if the option is
      * conflicting - if not, collate in hardware.  Otherwise,
      * turn the hardware collate option off...
      */

      if ((option = ppdFindOption(ppd, "Option")) != NULL &&
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

  if ((doc->slow_collate || doc->slow_order) && Duplex)
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
      fprintf(stderr, "ERROR: Unable to create temporary file: %s\n",
              strerror(errno));
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
 * 'skip_page()' - Skip past a page that won't be printed...
 */

static size_t				/* O - Length of next line */
skip_page(cups_file_t *fp,		/* I - File to read from */
          char        *line,		/* I - Line buffer */
	  size_t      linelen,		/* I - Length of initial line */
          size_t      linesize)		/* I - Size of line buffer */
{
  int	level;				/* Embedded document level */


  level = 0;

  while ((linelen = cupsFileGetLine(fp, line, linesize)) > 0)
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

      int	bytes;			/* Bytes of data */


      bytes = atoi(strchr(line, ':') + 1);

      while (bytes > 0)
      {
	if (bytes > linesize)
	  linelen = cupsFileRead(fp, line, linesize);
	else
	  linelen = cupsFileRead(fp, line, bytes);

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
 * 'start_nup()' - Start processing for N-up printing...
 */

static void
start_nup(pstops_doc_t *doc,		/* I - Document information */
          int          number,		/* I - Page number */
	  int          show_border,	/* I - Show the border? */
	  const int    *bounding_box)	/* I - BoundingBox value */
{
  int		pos;			/* Position on page */
  int		x, y;			/* Relative position of subpage */
  float		w, l,			/* Width and length of subpage */
		tx, ty;			/* Translation values for subpage */
  float		pagew,			/* Printable width of page */
		pagel;			/* Printable height of page */
  int		bboxw,			/* BoundingBox width */
		bboxl;			/* BoundingBox height */


  if (doc->mirror || Orientation || doc->number_up > 1)
    doc_puts(doc, "userdict/ESPsave save put\n");

  if (doc->mirror)
    doc_printf(doc, "%.1f 0.0 translate -1 1 scale\n", PageWidth);

  pos   = (number - 1) % doc->number_up;
  pagew = PageRight - PageLeft;
  pagel = PageTop - PageBottom;

  if (doc->fitplot)
  {
    bboxw = bounding_box[2] - bounding_box[0];
    bboxl = bounding_box[3] - bounding_box[1];
  }
  else
  {
    bboxw = PageWidth;
    bboxl = PageLength;
  }

  fprintf(stderr, "DEBUG: pagew = %.1f, pagel = %.1f\n", pagew, pagel);
  fprintf(stderr, "DEBUG: bboxw = %d, bboxl = %d\n", bboxw, bboxl);
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

  if (Duplex && doc->number_up > 1 && ((number / doc->number_up) & 1))
    doc_printf(doc, "%.1f %.1f translate\n", PageWidth - PageRight, PageBottom);
  else if (doc->number_up > 1 || doc->fitplot)
    doc_printf(doc, "%.1f %.1f translate\n", PageLeft, PageBottom);

  switch (doc->number_up)
  {
    default :
        if (doc->fitplot)
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
	{
          w = PageWidth;
	  l = PageLength;
	}
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
            doc_printf(doc, "0.0 %.1f translate -90 rotate\n", pagel);
	  else
	    doc_printf(doc, "%.1f 0.0 translate 90 rotate\n", pagew);

          doc_printf(doc, "%.1f %.1f translate %.3f %.3f scale\n",
                     tx + x * y * pagel * 0.5, ty + pagew * 0.333,
		     w / bboxw, l / bboxl);
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
	    doc_printf(doc, "%.1f 0.0 translate 90 rotate\n", pagew);
	  else
            doc_printf(doc, "0.0 %.1f translate -90 rotate\n", pagel);

          doc_printf(doc, "%.1f %.1f translate %.3f %.3f scale\n",
                     tx + x * pagel * 0.333, ty + y * pagew * 0.5,
		     w / bboxw, l / bboxl);
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
    float	fscale,			/* Scaling value for points */
		margin;			/* Current margin for borders */


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
		   margin - 2.25 * fscale,
		   margin - 2.25 * fscale,
		   bboxw + 4.5 * fscale - 2 * margin,
		   bboxl + 4.5 * fscale - 2 * margin);
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

  if (doc->fitplot)
  {
   /*
    * Clip the page that follows to the bounding box of the page...
    */

    doc_printf(doc, "%d %d translate\n", -bounding_box[0],
               -bounding_box[1]);
    doc_printf(doc, "%d %d %d %d ESPrc\n", bounding_box[0], bounding_box[1],
               bboxw, bboxl);
  }
  else if (doc->number_up > 1)
  {
   /*
    * Clip the page that follows to the default page size...
    */

    doc_printf(doc, "0 0 %d %d ESPrc\n", bboxw, bboxl);
  }
}


/*
 * End of "$Id$".
 */
