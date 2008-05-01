/*
 * "$Id$"
 *
 *   PostScript command filter for CUPS.
 *
 *   Copyright 2008 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *
 * Contents:
 *
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/string.h>
#include <cups/sidechannel.h>


/*
 * Local functions...
 */

static void	auto_configure(ppd_file_t *ppd, const char *user);
static void	print_self_test_page(ppd_file_t *ppd, const char *user);
static void	report_levels(ppd_file_t *ppd, const char *user);


/*
 * 'main()' - Process a CUPS command file.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  cups_file_t	*fp;			/* Command file */
  char		line[1024],		/* Line from file */
		*value;			/* Value on line */
  int		linenum;		/* Line number in file */
  ppd_file_t	*ppd;			/* PPD file */


 /*
  * Check for valid arguments...
  */

  if (argc < 6 || argc > 7)
  {
   /*
    * We don't have the correct number of arguments; write an error message
    * and return.
    */

    fputs("ERROR: commandtops job-id user title copies options [file]\n", stderr);
    return (1);
  }

 /*
  * Open the PPD file...
  */

  if ((ppd = ppdOpenFile(getenv("PPD"))) == NULL)
  {
    fputs("ERROR: Unable to open PPD file!\n", stderr);
    return (1);
  }

 /*
  * Open the command file as needed...
  */

  if (argc == 7)
  {
    if ((fp = cupsFileOpen(argv[6], "r")) == NULL)
    {
      perror("ERROR: Unable to open command file - ");
      return (1);
    }
  }
  else
    fp = cupsFileStdin();

 /*
  * Read the commands from the file and send the appropriate commands...
  */

  linenum = 0;

  while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
  {
   /*
    * Parse the command...
    */

    if (!strcasecmp(line, "AutoConfigure"))
      auto_configure(ppd, argv[2]);
    else if (!strcasecmp(line, "PrintSelfTestPage"))
      print_self_test_page(ppd, argv[2]);
    else if (!strcasecmp(line, "ReportLevels"))
      report_levels(ppd, argv[2]);
    else
      fprintf(stderr, "ERROR: Invalid printer command \"%s\"!\n", line);
  }

  return (0);
}


/*
 * 'auto_configure()' - Automatically configure the printer using PostScript
 *                      query commands and/or SNMP lookups.
 */

static void
auto_configure(ppd_file_t *ppd,		/* I - PPD file */
               const char *user)	/* I - Printing user */
{
  ppd_option_t	*option;		/* Current option in PPD */
  ppd_attr_t	*attr;			/* Query command attribute */
  char		buffer[1024],		/* String buffer */
		*bufptr;		/* Pointer into buffer */
  ssize_t	bytes;			/* Number of bytes read */
  int		datalen;		/* Side-channel data length */


 /*
  * See if the backend supports bidirectional I/O...
  */

  datalen = 1;
  if (cupsSideChannelDoRequest(CUPS_SC_CMD_GET_BIDI, buffer, &datalen,
                               30.0) != CUPS_SC_STATUS_OK ||
      buffer[0] != CUPS_SC_BIDI_SUPPORTED)
  {
    fputs("DEBUG: Unable to auto-configure PostScript Printer - no "
          "bidirectional I/O available!\n", stderr);
    return;
  }

 /*
  * Put the printer in PostScript mode...
  */

  if (ppd->jcl_begin)
  {
    fputs(ppd->jcl_begin, stdout);
    fputs(ppd->jcl_ps, stdout);
  }

  puts("%!");
  fflush(stdout);

 /*
  * Then loop through every option in the PPD file and ask for the current
  * value...
  */

  fputs("DEBUG: Auto-configuring PostScript printer...\n", stderr);

  for (option = ppdFirstOption(ppd); option; option = ppdNextOption(ppd))
  {
   /*
    * See if we have a query command for this option...
    */

    snprintf(buffer, sizeof(buffer), "?%s", option->keyword);

    if ((attr = ppdFindAttr(ppd, buffer, NULL)) == NULL || !attr->value)
    {
      fprintf(stderr, "DEBUG: Skipping %s option...\n", option->keyword);
      continue;
    }

   /*
    * Send the query code to the printer...
    */

    fprintf(stderr, "DEBUG: Querying %s...\n", option->keyword);
    fputs(attr->value, stdout);
    fflush(stdout);

    datalen = 0;
    cupsSideChannelDoRequest(CUPS_SC_CMD_DRAIN_OUTPUT, buffer, &datalen, 5.0);

   /*
    * Read the response data...
    */

    while ((bytes = cupsBackChannelRead(buffer, sizeof(buffer) - 1, 5.0)) > 0)
    {
     /*
      * Trim whitespace from both ends...
      */

      buffer[bytes] = '\0';

      for (bufptr = buffer + bytes - 1; bufptr >= buffer; bufptr --)
        if (isspace(*bufptr & 255))
	  *bufptr = '\0';
	else
	  break;

      for (bufptr = buffer; isspace(*bufptr & 255); bufptr ++);

     /*
      * Skip blank lines...
      */

      if (!*bufptr)
        continue;

     /*
      * Write out the result and move on to the next option...
      */

      fprintf(stderr, "DEBUG: Default%s=%s\n", option->keyword, bufptr);
      fprintf(stderr, "PPD: Default%s=%s\n", option->keyword, bufptr);
      break;
    }
  }

 /*
  * Finish the job...
  */

  if (ppd->jcl_begin)
    fputs(ppd->jcl_begin, stdout);
  else
    putchar(0x04);

  fflush(stdout);
}


/*
 * 'print_self_test_page()' - Print a self-test page.
 */

static void
print_self_test_page(ppd_file_t *ppd,	/* I - PPD file */
                     const char *user)	/* I - Printing user */
{
 /*
  * Put the printer in PostScript mode...
  */

  if (ppd->jcl_begin)
  {
    fputs(ppd->jcl_begin, stdout);
    fputs(ppd->jcl_ps, stdout);
  }

  puts("%!");

 /*
  * Send a simple file the draws a box around the imageable area and shows
  * the product/interpreter information...
  */

  puts("% You are using the wrong driver for your printer!\n"
       "0 setgray\n"
       "2 setlinewidth\n"
       "initclip newpath clippath gsave stroke grestore pathbbox\n"
       "exch pop exch pop exch 9 add exch 9 sub moveto\n"
       "/Courier findfont 12 scalefont setfont\n"
       "0 -12 rmoveto gsave product show grestore\n"
       "0 -12 rmoveto gsave version show ( ) show revision 20 string cvs show "
       "grestore\n"
       "0 -12 rmoveto gsave serialnumber 20 string cvs show grestore\n"
       "showpage");

 /*
  * Finish the job...
  */

  if (ppd->jcl_begin)
    fputs(ppd->jcl_begin, stdout);
  else
    putchar(0x04);

  fflush(stdout);
}


/*
 * 'report_levels()' - Report supply levels.
 */

static void
report_levels(ppd_file_t *ppd,		/* I - PPD file */
              const char *user)		/* I - Printing user */
{
 /*
  * Put the printer in PostScript mode...
  */

  if (ppd->jcl_begin)
  {
    fputs(ppd->jcl_begin, stdout);
    fputs(ppd->jcl_ps, stdout);
  }

  puts("%!");

 /*
  * Finish the job...
  */

  if (ppd->jcl_begin)
    fputs(ppd->jcl_begin, stdout);
  else
    putchar(0x04);

  fflush(stdout);
}


/*
 * End of "$Id$".
 */
