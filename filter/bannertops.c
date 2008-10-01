/*
 * "$Id$"
 *
 *   Banner to PostScript filter for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2008 by Apple Inc.
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
 */

/*
 * Include necessary headers...
 */

#include "pstext.h"
#include "image.h"
#include <cups/i18n.h>


/*
 * Constants...
 */

#define SHOW_IMAGEABLE_AREA		1	/* Show imageable area */
#define SHOW_JOB_BILLING		2	/* Show billing string */
#define SHOW_JOB_ID			4	/* Show job ID */
#define SHOW_JOB_NAME			8	/* Show job title */
#define SHOW_JOB_ORIGINATING_USER_NAME	16	/* Show owner of job */
#define SHOW_JOB_ORIGINATING_HOST_NAME	32	/* Show submitting system */
#define SHOW_JOB_UUID			64	/* Show job UUID */
#define SHOW_OPTIONS			128	/* Show print options */
#define SHOW_PAPER_NAME			256	/* Show paper size name */
#define SHOW_PAPER_SIZE			512	/* Show paper dimensions */
#define SHOW_PRINTER_DRIVER_NAME	1024	/* Show printer driver name */
#define SHOW_PRINTER_DRIVER_VERSION	2048	/* Show printer driver version */
#define SHOW_PRINTER_INFO		4096	/* Show printer description */
#define SHOW_PRINTER_LOCATION		8192	/* Show printer location */
#define SHOW_PRINTER_MAKE_AND_MODEL	16384	/* Show printer make and model */
#define SHOW_PRINTER_NAME		32768	/* Show printer queue ID */
#define SHOW_TIME_AT_CREATION		65536	/* Show date/time when submitted */
#define SHOW_TIME_AT_PROCESSING		131072	/* Show date/time when printed */


/*
 * Structures...
 */

typedef struct banner_file_s		/**** Banner file data ****/
{
  int		show;			/* What to show */
  char		*header,		/* Header text */
		*footer;		/* Footer text */
  cups_array_t	*notices,		/* Notices to show */
		*images;		/* Images to show */
} banner_file_t;


/*
 * Local functions...
 */

static banner_file_t	*load_banner(const char *filename);
static int		write_banner(banner_file_t *banner, ppd_file_t *ppd,
			             ps_text_t *fonts, int job_id,
				     const char *title, const char *username,
				     int num_options, cups_option_t *options);
static void		write_epilogue(int num_pages);
static ps_text_t	*write_prolog(const char *title, const char *user);


/*
 * 'main()' - Generate PostScript cover pages.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  banner_file_t	*banner;		/* Banner file data */
  int		num_options;		/* Number of print options */
  cups_option_t	*options;		/* Print options */
  ppd_file_t	*ppd;			/* PPD file */
  ps_text_t	*fonts;			/* Fonts for output */
  int		job_id;			/* Job ID from command-line */
  const char	*title,			/* Title from command-line */
		*username;		/* Username from command-line */
  int		num_pages;		/* Number of pages printed */


 /*
  * Make sure status messages are not buffered...
  */

  setbuf(stderr, NULL);

 /*
  * Check command-line...
  */

  if (argc < 6 || argc > 7)
  {
    _cupsLangPrintf(stderr,
                    _("Usage: %s job-id user title copies options [file]\n"),
                    "bannertops");
    return (1);
  }

 /*
  * Get stuff from command-line...
  */

  job_id      = atoi(argv[1]);
  username    = argv[2];
  title       = argv[3];
  options     = NULL;
  num_options = cupsParseOptions(argv[5], 0, &options);
  banner      = load_banner(argv[6]);

 /*
  * Set standard options and get the PPD file for this printer...
  */

  ppd = SetCommonOptions(num_options, options, 1);

 /*
  * Write a PostScript banner document and return...
  */

  fonts       = write_prolog(title, username);
  num_pages   = write_banner(banner, ppd, fonts, job_id, title, username,
                             num_options, options);

  write_epilogue(num_pages);

  return (0);
}


/*
 * 'load_banner()' - Load the banner file.
 */

static banner_file_t *			/* O - Banner file data */
load_banner(const char *filename)	/* I - Filename or NULL for stdin */
{
  cups_file_t	*fp;			/* File */
  char		line[2048],		/* Line buffer */
		*ptr;			/* Pointer into line */
  int		linenum;		/* Current line number */
  banner_file_t	*banner;		/* Banner file data */
  const char	*cups_docroot;		/* CUPS_DOCROOT environment variable */


  fprintf(stderr, "DEBUG: load_banner(filename=\"%s\")\n",
          filename ? filename : "(stdin)");

 /*
  * Open the banner file...
  */

  if (filename)
    fp = cupsFileOpen(filename, "r");
  else
    fp = cupsFileStdin();

  if (!fp)
  {
    _cupsLangPrintf(stderr,
	            _("ERROR: Unable to open banner file \"%s\" - %s\n"),
                    filename ? filename : "(stdin)", strerror(errno));
    exit(1);
  }

 /*
  * Read the banner file...
  */

  if ((cups_docroot = getenv("CUPS_DOCROOT")) == NULL)
    cups_docroot = CUPS_DOCROOT;

  banner  = calloc(1, sizeof(banner_file_t));
  linenum = 0;

  while (cupsFileGets(fp, line, sizeof(line)))
  {
   /*
    * Skip blank and comment lines...
    */

    linenum ++;

    if (line[0] == '#' || !line[0])
      continue;

   /*
    * Break the line into keyword and value parts...
    */

    for (ptr = line; *ptr && !isspace(*ptr & 255); ptr ++);

    while (isspace(*ptr & 255))
      *ptr++ = '\0';

    if (!*ptr)
    {
      _cupsLangPrintf(stderr,
                      _("ERROR: Missing value on line %d of banner file!\n"),
		      linenum);
      continue;
    }

   /*
    * Save keyword values in the appropriate places...
    */

    if (!strcasecmp(line, "Footer"))
    {
      if (banner->footer)
        fprintf(stderr, "DEBUG: Extra \"Footer\" on line %d of banner file!\n",
		linenum);
      else
        banner->footer = strdup(ptr);
    }
    else if (!strcasecmp(line, "Header"))
    {
      if (banner->header)
        fprintf(stderr, "DEBUG: Extra \"Header\" on line %d of banner file!\n",
		linenum);
      else
        banner->header = strdup(ptr);
    }
    else if (!strcasecmp(line, "Image"))
    {
      char	imagefile[1024];	/* Image filename */


      if (ptr[0] == '/')
        strlcpy(imagefile, ptr, sizeof(imagefile));
      else
        snprintf(imagefile, sizeof(imagefile), "%s/%s", cups_docroot, ptr);

      if (access(imagefile, R_OK))
      {
        fprintf(stderr, "DEBUG: Image \"%s\" on line %d of banner file: %s\n",
	        ptr, linenum, strerror(errno));
      }
      else
      {
        if (!banner->images)
	  banner->images = cupsArrayNew(NULL, NULL);

        cupsArrayAdd(banner->images, strdup(imagefile));
      }
    }
    else if (!strcasecmp(line, "Notice"))
    {
      if (!banner->notices)
	banner->notices = cupsArrayNew(NULL, NULL);

      cupsArrayAdd(banner->notices, strdup(ptr));
    }
    else if (!strcasecmp(line, "Show"))
    {
      char	*value;			/* Current value */


      for (value = ptr; *value; value = ptr)
      {
       /*
	* Find the end of the current value
	*/

        while (*ptr && !isspace(*ptr & 255))
	  ptr ++;

        while (*ptr && isspace(*ptr & 255))
	  *ptr++ = '\0';

       /*
        * Add the value to the show flags...
	*/
	if (!strcasecmp(value, "imageable-area"))
	  banner->show |= SHOW_IMAGEABLE_AREA;
	else if (!strcasecmp(value, "job-billing"))
	  banner->show |= SHOW_JOB_BILLING;
	else if (!strcasecmp(value, "job-id"))
	  banner->show |= SHOW_JOB_ID;
	else if (!strcasecmp(value, "job-name"))
	  banner->show |= SHOW_JOB_NAME;
	else if (!strcasecmp(value, "job-originating-host-name"))
	  banner->show |= SHOW_JOB_ORIGINATING_HOST_NAME;
	else if (!strcasecmp(value, "job-originating-user-name"))
	  banner->show |= SHOW_JOB_ORIGINATING_USER_NAME;
	else if (!strcasecmp(value, "job-uuid"))
	  banner->show |= SHOW_JOB_UUID;
	else if (!strcasecmp(value, "options"))
	  banner->show |= SHOW_OPTIONS;
	else if (!strcasecmp(value, "paper-name"))
	  banner->show |= SHOW_PAPER_NAME;
	else if (!strcasecmp(value, "paper-size"))
	  banner->show |= SHOW_PAPER_SIZE;
	else if (!strcasecmp(value, "printer-driver-name"))
	  banner->show |= SHOW_PRINTER_DRIVER_NAME;
	else if (!strcasecmp(value, "printer-driver-version"))
	  banner->show |= SHOW_PRINTER_DRIVER_VERSION;
	else if (!strcasecmp(value, "printer-info"))
	  banner->show |= SHOW_PRINTER_INFO;
	else if (!strcasecmp(value, "printer-location"))
	  banner->show |= SHOW_PRINTER_LOCATION;
	else if (!strcasecmp(value, "printer-make-and-model"))
	  banner->show |= SHOW_PRINTER_MAKE_AND_MODEL;
	else if (!strcasecmp(value, "printer-name"))
	  banner->show |= SHOW_PRINTER_NAME;
	else if (!strcasecmp(value, "time-at-creation"))
	  banner->show |= SHOW_TIME_AT_CREATION;
	else if (!strcasecmp(value, "time-at-processing"))
	  banner->show |= SHOW_TIME_AT_PROCESSING;
	else
        {
	  fprintf(stderr,
	          "DEBUG: Unknown \"Show\" value \"%s\" on line %d of banner "
		  "file!\n", value, linenum);
	}
      }
    }
    else
      fprintf(stderr, "DEBUG: Unknown key \"%s\" on line %d of banner file!\n",
              line, linenum);
  }

  if (filename)
    cupsFileClose(fp);

  return (banner);
}


/*
 * 'write_banner()' - Write a banner page...
 */

static int				/* O - Number of pages */
write_banner(banner_file_t *banner,	/* I - Banner file */
             ppd_file_t    *ppd,	/* I - PPD file */
	     ps_text_t     *fonts,	/* I - Fonts */
	     int           job_id,	/* I - Job ID */
	     const char    *title,	/* I - Title of job */
	     const char    *username,	/* I - Owner of job */
	     int           num_options,	/* I - Number of options */
	     cups_option_t *options)	/* I - Options */
{
  char		*notice;		/* Current notice */
  char		*imagefile;		/* Current image file */
  cups_array_t	*images;		/* Images */
  const char	*option;		/* Option value */
  int		i, j;			/* Looping vars */
  float		x,			/* Current X position */
		y;			/* Current Y position */
  cups_lang_t	*language;		/* Default language */
  int		showlines;		/* Number of lines to show */
  float		fontsize;		/* Font size to use */
  int		num_pages;		/* Number of pages */
  float		print_width,		/* Printable width of page */
		print_height,		/* Printable height of page */
		info_top,		/* Top of info fields */
		info_height,		/* Height of info fields */
		line_height,		/* Height of info lines */
		notices_height,		/* Height of all notices */
		images_width,		/* Width of all images */
		images_height,		/* Height of all images */
		total_height;		/* Height of all content */
  char		text[1024];		/* Formatted field text */


 /*
  * Figure out how many lines of text will be shown...
  */

  showlines = 0;
  if (banner->show & SHOW_IMAGEABLE_AREA)
    showlines += 2;
  if (banner->show & SHOW_JOB_BILLING)
    showlines ++;
  if (banner->show & SHOW_JOB_ID)
    showlines ++;
  if (banner->show & SHOW_JOB_NAME)
    showlines ++;
  if (banner->show & SHOW_JOB_ORIGINATING_USER_NAME)
    showlines ++;
  if (banner->show & SHOW_JOB_ORIGINATING_HOST_NAME)
    showlines ++;
  if (banner->show & SHOW_JOB_UUID)
    showlines ++;
  if (banner->show & SHOW_OPTIONS)
  {
    for (j = 0; j < num_options; j ++)
    {
      if (strcasecmp("media", options[j].name) &&
	  strcasecmp("PageSize", options[j].name) &&
	  strcasecmp("PageRegion", options[j].name) &&
	  strcasecmp("InputSlot", options[j].name) &&
	  strcasecmp("MediaType", options[j].name) &&
	  strcasecmp("finishings", options[j].name) &&
	  strcasecmp("sides", options[j].name) &&
	  strcasecmp("Duplex", options[j].name) &&
	  strcasecmp("orientation-requested", options[j].name) &&
	  strcasecmp("landscape", options[j].name) &&
	  strcasecmp("number-up", options[j].name) &&
	  strcasecmp("OutputOrder", options[j].name))
      continue;

      showlines ++;
    }
  }
  if (banner->show & SHOW_PAPER_NAME)
    showlines ++;
  if (banner->show & SHOW_PAPER_SIZE)
    showlines += 2;
  if (banner->show & SHOW_PRINTER_DRIVER_NAME)
    showlines ++;
  if (banner->show & SHOW_PRINTER_DRIVER_VERSION)
    showlines ++;
  if (banner->show & SHOW_PRINTER_INFO)
    showlines ++;
  if (banner->show & SHOW_PRINTER_LOCATION)
    showlines ++;
  if (banner->show & SHOW_PRINTER_MAKE_AND_MODEL)
    showlines ++;
  if (banner->show & SHOW_PRINTER_NAME)
    showlines ++;
  if (banner->show & SHOW_TIME_AT_CREATION)
    showlines ++;
  if (banner->show & SHOW_TIME_AT_PROCESSING)
    showlines ++;

 /*
  * Figure out the dimensions and positions of everything...
  */

  print_width    = PageRight - PageLeft;
  print_height   = PageTop - PageBottom;
  fontsize       = print_height / 60;	/* Nominally 12pts */
  line_height    = 1.2 * fontsize;
  info_height    = showlines * line_height;
  notices_height = cupsArrayCount(banner->notices) * line_height;

  if (cupsArrayCount(banner->images))
    images_height = print_height / 10;	/* Nominally 1" */
  else
    images_height = 0;

  total_height = info_height + notices_height + images_height;
  if (cupsArrayCount(banner->notices) && showlines)
    total_height += 2 * line_height;
  if (cupsArrayCount(banner->images) &&
      (showlines || cupsArrayCount(banner->notices)))
    total_height += 2 * line_height;

  info_top = 0.5 * (print_height + total_height);

 /*
  * Write the page(s)...
  */

  language  = cupsLangDefault();
  num_pages = Duplex ? 2 : 1;

  for (i = 1; i <= num_pages; i ++)
  {
   /*
    * Start the page...
    */

    printf("%%Page: %s %d\n", i == 1 ? "coverpage" : "coverback", i);
    puts("gsave");
    printf("%.1f %.1f translate\n", PageLeft, PageBottom);
    puts("0 setgray");

    y = info_top;

   /*
    * Information...
    */

    if (banner->show)
    {
      x = 0.33 * print_width;

      if (banner->show & SHOW_PRINTER_NAME)
      {
	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
	psTextUTF8(fonts, fontsize, PS_BOLD, PS_RIGHT,
	           _cupsLangString(language, _("Printer Name: ")));
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT, getenv("PRINTER_NAME"));
      }
      if (banner->show & SHOW_JOB_ID)
      {
        snprintf(text, sizeof(text), "%s-%d", getenv("PRINTER_NAME"), job_id);
	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
	psTextUTF8(fonts, fontsize, PS_BOLD, PS_RIGHT,
	           _cupsLangString(language, _("Job ID: ")));
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT, text);
      }
      if (banner->show & SHOW_JOB_UUID)
      {
	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
	psTextUTF8(fonts, fontsize, PS_BOLD, PS_RIGHT,
	           _cupsLangString(language, _("Job UUID: ")));
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT,
		   cupsGetOption("job-uuid", num_options, options));
      }
      if (banner->show & SHOW_JOB_NAME)
      {
	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
	psTextUTF8(fonts, fontsize, PS_BOLD, PS_RIGHT,
	           _cupsLangString(language, _("Title: ")));
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT, title);
      }
      if (banner->show & SHOW_JOB_ORIGINATING_USER_NAME)
      {
	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
	psTextUTF8(fonts, fontsize, PS_BOLD, PS_RIGHT,
	           _cupsLangString(language, _("Printed For: ")));
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT, username);
      }
      if (banner->show & SHOW_JOB_ORIGINATING_HOST_NAME)
      {
	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
	psTextUTF8(fonts, fontsize, PS_BOLD, PS_RIGHT,
	           _cupsLangString(language, _("Printed From: ")));
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT,
	           cupsGetOption("job-originating-host-name", num_options,
		                 options));
      }
      if (banner->show & SHOW_JOB_BILLING)
      {
	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
	psTextUTF8(fonts, fontsize, PS_BOLD, PS_RIGHT,
	           _cupsLangString(language, _("Billing Information: ")));
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT,
	           cupsGetOption("job-billing", num_options, options));
      }
      if (banner->show & SHOW_OPTIONS)
      {
	printf("%.1f %.1f moveto", x, y);
	psTextUTF8(fonts, fontsize, PS_BOLD, PS_RIGHT,
	           _cupsLangString(language, _("Options: ")));

        for (j = 0; j < num_options; j ++)
	{
	  if (strcasecmp("media", options[j].name) &&
	      strcasecmp("PageSize", options[j].name) &&
	      strcasecmp("PageRegion", options[j].name) &&
	      strcasecmp("InputSlot", options[j].name) &&
	      strcasecmp("MediaType", options[j].name) &&
	      strcasecmp("finishings", options[j].name) &&
	      strcasecmp("sides", options[j].name) &&
	      strcasecmp("Duplex", options[j].name) &&
	      strcasecmp("orientation-requested", options[j].name) &&
	      strcasecmp("landscape", options[j].name) &&
	      strcasecmp("number-up", options[j].name) &&
	      strcasecmp("OutputOrder", options[j].name))
          continue;

          if (!strcasecmp("landscape", options[j].name))
	    strlcpy(text, "orientation-requested=landscape", sizeof(text));
	  else if (!strcasecmp("orientation-requested", options[j].name))
	  {
	    switch (atoi(options[j].value))
	    {
	      default :
	      case IPP_PORTRAIT :
	          strlcpy(text, "orientation-requested=portrait",
		          sizeof(text));
		  break;

	      case IPP_LANDSCAPE :
	          strlcpy(text, "orientation-requested=landscape",
		          sizeof(text));
		  break;

	      case IPP_REVERSE_PORTRAIT :
	          strlcpy(text, "orientation-requested=reverse-portrait",
		          sizeof(text));
		  break;

	      case IPP_REVERSE_LANDSCAPE :
	          strlcpy(text, "orientation-requested=reverse-landscape",
		          sizeof(text));
		  break;
	    }
	  }
	  else
	    snprintf(text, sizeof(text), "%s=%s", options[j].name,
	             options[j].value);

	  printf("%.1f %.1f moveto", x, y);
	  y -= line_height;
	  psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT, text);
        }
      }

      if (banner->show & SHOW_PRINTER_INFO)
      {
	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
	psTextUTF8(fonts, fontsize, PS_BOLD, PS_RIGHT,
	           _cupsLangString(language, _("Description: ")));
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT,
	           getenv("PRINTER_INFO"));
      }
      if (banner->show & SHOW_PRINTER_LOCATION)
      {
	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
	psTextUTF8(fonts, fontsize, PS_BOLD, PS_RIGHT,
	           _cupsLangString(language, _("Location: ")));
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT,
	           getenv("PRINTER_LOCATION"));
      }
      if (banner->show & SHOW_PRINTER_MAKE_AND_MODEL)
      {
	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
	psTextUTF8(fonts, fontsize, PS_BOLD, PS_RIGHT,
	           _cupsLangString(language, _("Make and Model: ")));
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT,
	           ppd ? ppd->nickname : NULL);
      }

      if (banner->show & SHOW_PAPER_NAME)
      {
        if ((option = cupsGetOption("media", num_options, options)) == NULL)
	  if ((option = cupsGetOption("PageSize", num_options, options)) == NULL)
	    if ((option = cupsGetOption("PageRegion", num_options,
	                                options)) == NULL)
	      option = "Default";

	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
	psTextUTF8(fonts, fontsize, PS_BOLD, PS_RIGHT,
	           _cupsLangString(language, _("Media Name: ")));
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT, option);
      }
      if (banner->show & SHOW_PAPER_SIZE)
      {
        snprintf(text, sizeof(text),
	         _cupsLangString(language, _("%.2f x %.2f inches")),
		 PageWidth / 72.0, PageLength / 72.0);
	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
	psTextUTF8(fonts, fontsize, PS_BOLD, PS_RIGHT,
	           _cupsLangString(language, _("Media Dimensions: ")));
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT, text);

        snprintf(text, sizeof(text),
	         _cupsLangString(language, _("%.0f x %.0f millimeters")),
	         PageWidth * 25.4 / 72.0, PageLength * 25.4 / 72.0);
	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT, text);
      }
      if (banner->show & SHOW_IMAGEABLE_AREA)
      {
        snprintf(text, sizeof(text),
	         _cupsLangString(language,
		                 _("%.2f x %.2f to %.2f x %.2f inches")),
	         PageLeft / 72.0, PageBottom / 72.0,
		 PageRight / 72.0, PageTop / 72.0);
	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
	psTextUTF8(fonts, fontsize, PS_BOLD, PS_RIGHT,
	           _cupsLangString(language, _("Media Limits: ")));
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT, text);

        snprintf(text, sizeof(text),
	         _cupsLangString(language,
		                 _("%.0f x %.0f to %.0f x %.0f millimeters")),
	         PageLeft * 25.4 / 72.0, PageBottom * 25.4 / 72.0,
		 PageRight * 25.4 / 72.0, PageTop * 25.4 / 72.0);
	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT, text);

	printf("gsave 2 setlinewidth 1 1 %.1f %.1f rectstroke grestore\n",
	       print_width - 2.0, print_height - 2.0);
      }
      if (banner->show & SHOW_PRINTER_DRIVER_NAME)
      {
	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
	psTextUTF8(fonts, fontsize, PS_BOLD, PS_RIGHT,
	           _cupsLangString(language, _("Driver Name: ")));
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT,
	           ppd ? ppd->pcfilename : NULL);
      }
      if (banner->show & SHOW_PRINTER_DRIVER_VERSION)
      {
        ppd_attr_t  *file_version = ppdFindAttr(ppd, "FileVersion", NULL);

	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
	psTextUTF8(fonts, fontsize, PS_BOLD, PS_RIGHT,
	           _cupsLangString(language, _("Driver Version: ")));
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT,
	           file_version ? file_version->value : NULL);
      }
      if (banner->show & SHOW_TIME_AT_CREATION)
      {
        if ((option = cupsGetOption("time-at-creation", num_options,
	                            options)) != NULL)
        {
	  time_t	curtime;	/* Current time */
	  struct tm	*curdate;	/* Current date */

          curtime = (time_t)atoi(option);
	  curdate = localtime(&curtime);

          strftime(text, sizeof(text), "%c", curdate);
	}
	else
	  strlcpy(text, "?", sizeof(text));

	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
	psTextUTF8(fonts, fontsize, PS_BOLD, PS_RIGHT,
	           _cupsLangString(language, _("Created On: ")));
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT, text);
      }
      if (banner->show & SHOW_TIME_AT_PROCESSING)
      {
        if ((option = cupsGetOption("time-at-processing", num_options,
	                            options)) != NULL)
        {
	  time_t	curtime;	/* Current time */
	  struct tm	*curdate;	/* Current date */

          curtime = (time_t)atoi(option);
	  curdate = localtime(&curtime);

          strftime(text, sizeof(text), "%c", curdate);
	}
	else
	  strlcpy(text, "?", sizeof(text));

	printf("%.1f %.1f moveto", x, y);
	y -= line_height;
	psTextUTF8(fonts, fontsize, PS_BOLD, PS_RIGHT,
	           _cupsLangString(language, _("Printed On: ")));
        psTextUTF8(fonts, fontsize, PS_NORMAL, PS_LEFT, text);
      }
    }

   /*
    * Notices...
    */

    if (cupsArrayCount(banner->notices))
    {
      if (banner->show)
        y -= 2 * line_height;

      x = 0.5 * print_width;

      for (notice = (char *)cupsArrayFirst(banner->notices);
           notice;
	   notice = (char *)cupsArrayNext(banner->notices))
      {
        printf("%.1f %.1f moveto", x, y);
	y -= line_height;
	psTextUTF8(fonts, fontsize, PS_NORMAL, PS_CENTER, notice);
      }
    }

   /*
    * Images...
    */

    /* TODO: write images */

   /*
    * Header and footer...
    */

    x = 0.5 * print_width;

    if (banner->header)
    {
      printf("%.1f %.1f moveto", x, print_height - 2 * fontsize);
      psTextUTF8(fonts, 2 * fontsize, PS_BOLD, PS_CENTER, banner->header);
    }

    if (banner->footer)
    {
      printf("%.1f %.1f moveto", x, fontsize);
      psTextUTF8(fonts, 2 * fontsize, PS_BOLD, PS_CENTER, banner->footer);
    }

   /*
    * Show the page...
    */

    puts("grestore");
    puts("showpage");
  }

  return (num_pages);
}


/*
 * 'write_epilogue()' - Write the PostScript file epilogue.
 */

static void
write_epilogue(int num_pages)		/* I - Number of pages */
{
  puts("%%Trailer");
  printf("%%%%Pages: %d\n", num_pages);
  puts("%%EOF");
}


/*
 * 'write_prolog()' - Write the PostScript file prolog with options.
 */

ps_text_t *				/* O - Fonts */
write_prolog(const char *title,		/* I - Title of job */
	     const char *username)	/* I - Username */
{
  time_t	curtime;		/* Current time */
  struct tm	*curtm;			/* Current date */
  char		curdate[255];		/* Current date (text format) */
  ps_text_t	*fonts;			/* Fonts */


 /*
  * Get the fonts we'll need...
  */

  fonts = psTextInitialize();

 /*
  * Output the DSC header...
  */

  curtime = time(NULL);
  curtm   = localtime(&curtime);
  strftime(curdate, sizeof(curdate), "%c", curtm);

  puts("%!PS-Adobe-3.0");
  printf("%%%%BoundingBox: %.0f %.0f %.0f %.0f\n", PageLeft, PageBottom,
         PageRight, PageTop);
  printf("%%cupsRotation: %d\n", (Orientation & 3) * 90);
  puts("%%Creator: bannertops/" CUPS_SVERSION);
  printf("%%%%CreationDate: %s\n", curdate);
  puts("%%LanguageLevel: 2");
  puts("%%DocumentData: Clean7Bit");
  WriteTextComment("Title", title);
  WriteTextComment("For", username);
  printf("%%%%Pages: %d\n", Duplex ? 2 : 1);
  psTextListFonts(fonts);
  puts("%%EndComments");
  puts("%%BeginProlog");
  psTextEmbedFonts(fonts);
  puts("%%EndProlog");

  return (fonts);
}


/*
 * End of "$Id$".
 */
