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
 * Globals...
 */

/*
 * Local functions...
 */

static void	write_epilogue(int num_pages);
ps_text_t	*write_prolog(const char *title, const char *user);


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
