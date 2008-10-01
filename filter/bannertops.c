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
static int		write_banner(ppd_file_t *ppd, ps_text_t *fonts,
				     banner_file_t *banner);
static void		write_epilogue(int num_pages);
static ps_text_t	*write_prolog(const char *title, const char *user);


/*
 * 'main()' - Generate PostScript cover pages.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		num_options;		/* Number of print options */
  cups_option_t	*options;		/* Print options */
  ppd_file_t	*ppd;			/* PPD file */
  ps_text_t	*fonts;			/* Fonts for output */
  cups_lang_t	*language;		/* Default language */
  cups_file_t	*fp;			/* Print file */
  char		line[2048],		/* Line from file */
		*ptr;			/* Pointer into line */
  float		fontsize;		/* Font size to use */


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
  * Get standard options and the PPD file for this printer...
  */

  options     = NULL;
  num_options = cupsParseOptions(argv[5], 0, &options);
  ppd         = SetCommonOptions(num_options, options, 1);
  fonts       = write_prolog(argv[3], argv[2]);
  language    = cupsLangDefault();

 /*
  * Write the page...
  */

  puts("%%Page: 1 1");
  puts("0 setgray");

  puts("306 144 moveto");
  psTextUTF8(fonts, 12.0, PS_BOLD, PS_RIGHT,
             _cupsLangString(language, _("Job ID: ")));
  snprintf(line, sizeof(line), "%s-%s", getenv("PRINTER"), argv[1]);
  psTextUTF8(fonts, 12.0, PS_NORMAL, PS_LEFT, line);

  puts("306 130 moveto");
  psTextUTF8(fonts, 12.0, PS_BOLD, PS_RIGHT,
             _cupsLangString(language, _("Title: ")));
  psTextUTF8(fonts, 12.0, PS_NORMAL, PS_LEFT, argv[3]);

  puts("showpage");
  write_epilogue(1);

  return (0);

#if 0
  int	line;			/* Current line */


  NumPages ++;
  printf("%%%%Page: %d %d\n", NumPages, NumPages);

  puts("gsave");

  if (PrettyPrint)
    printf("%d H\n", NumPages);

  for (line = 0; line < SizeLines; line ++)
    write_line(line, Page[line]);

  puts("grestore");
  puts("showpage");

  memset(Page[0], 0, sizeof(lchar_t) * SizeColumns * SizeLines);
#endif /* 0 */
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


 /*
  * Open the banner file...
  */

  if (filename)
    fp = cupsFileStdin();
  else
    fp = cupsFileOpen(filename, "r");

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
write_banner(ppd_file_t    *ppd,	/* I - PPD file */
             ps_text_t     *fonts,	/* I - Fonts */
	     banner_file_t *banner)	/* I - Banner file */
{
  return (1);
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
	     const char *user)		/* I - Username */
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
  WriteTextComment("Title", title);
  WriteTextComment("For", user);
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
