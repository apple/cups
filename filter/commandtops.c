/*
 * "$Id: commandtops.c 11558 2014-02-06 18:33:34Z msweet $"
 *
 * PostScript command filter for CUPS.
 *
 * Copyright 2008-2014 by Apple Inc.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Include necessary headers...
 */

#include <cups/cups-private.h>
#include <cups/ppd.h>
#include <cups/sidechannel.h>


/*
 * Local functions...
 */

static int	auto_configure(ppd_file_t *ppd, const char *user);
static void	begin_ps(ppd_file_t *ppd, const char *user);
static void	end_ps(ppd_file_t *ppd);
static void	print_self_test_page(ppd_file_t *ppd, const char *user);
static void	report_levels(ppd_file_t *ppd, const char *user);


/*
 * 'main()' - Process a CUPS command file.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		status = 0;		/* Exit status */
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

    _cupsLangPrintf(stderr,
                    _("Usage: %s job-id user title copies options [file]"),
                    argv[0]);
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

    if (!_cups_strcasecmp(line, "AutoConfigure"))
      status |= auto_configure(ppd, argv[2]);
    else if (!_cups_strcasecmp(line, "PrintSelfTestPage"))
      print_self_test_page(ppd, argv[2]);
    else if (!_cups_strcasecmp(line, "ReportLevels"))
      report_levels(ppd, argv[2]);
    else
    {
      _cupsLangPrintFilter(stderr, "ERROR",
                           _("Invalid printer command \"%s\"."), line);
      status = 1;
    }
  }

  return (status);
}


/*
 * 'auto_configure()' - Automatically configure the printer using PostScript
 *                      query commands and/or SNMP lookups.
 */

static int				/* O - Exit status */
auto_configure(ppd_file_t *ppd,		/* I - PPD file */
               const char *user)	/* I - Printing user */
{
  int		status = 0;		/* Exit status */
  ppd_option_t	*option;		/* Current option in PPD */
  ppd_attr_t	*attr;			/* Query command attribute */
  const char	*valptr;		/* Pointer into attribute value */
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
    return (1);
  }

 /*
  * Put the printer in PostScript mode...
  */

  begin_ps(ppd, user);

 /*
  * (STR #4028)
  *
  * As a lot of PPDs contain bad PostScript query code, we need to prevent one
  * bad query sequence from affecting all auto-configuration.  The following
  * error handler allows us to log PostScript errors to cupsd.
  */

  puts("/cups_handleerror {\n"
       "  $error /newerror false put\n"
       "  (:PostScript error in \") print cups_query_keyword print (\": ) "
       "print\n"
       "  $error /errorname get 128 string cvs print\n"
       "  (; offending command:) print $error /command get 128 string cvs "
       "print (\n) print flush\n"
       "} bind def\n"
       "errordict /timeout {} put\n"
       "/cups_query_keyword (?Unknown) def\n");
  fflush(stdout);

 /*
  * Wait for the printer to become connected...
  */

  do
  {
    sleep(1);
    datalen = 1;
  }
  while (cupsSideChannelDoRequest(CUPS_SC_CMD_GET_CONNECTED, buffer, &datalen,
                                  5.0) == CUPS_SC_STATUS_OK && !buffer[0]);

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

    for (bufptr = buffer, valptr = attr->value; *valptr; valptr ++)
    {
     /*
      * Log the query code, breaking at newlines...
      */

      if (*valptr == '\n')
      {
        *bufptr = '\0';
        fprintf(stderr, "DEBUG: %s\\n\n", buffer);
        bufptr = buffer;
      }
      else if (*valptr < ' ')
      {
        if (bufptr >= (buffer + sizeof(buffer) - 4))
        {
	  *bufptr = '\0';
	  fprintf(stderr, "DEBUG: %s\n", buffer);
	  bufptr = buffer;
        }

        if (*valptr == '\r')
        {
          *bufptr++ = '\\';
          *bufptr++ = 'r';
        }
        else if (*valptr == '\t')
        {
          *bufptr++ = '\\';
          *bufptr++ = 't';
        }
        else
        {
          *bufptr++ = '\\';
          *bufptr++ = '0' + ((*valptr / 64) & 7);
          *bufptr++ = '0' + ((*valptr / 8) & 7);
          *bufptr++ = '0' + (*valptr & 7);
        }
      }
      else
      {
        if (bufptr >= (buffer + sizeof(buffer) - 1))
        {
	  *bufptr = '\0';
	  fprintf(stderr, "DEBUG: %s\n", buffer);
	  bufptr = buffer;
        }

	*bufptr++ = *valptr;
      }
    }

    if (bufptr > buffer)
    {
      *bufptr = '\0';
      fprintf(stderr, "DEBUG: %s\n", buffer);
    }

    printf("/cups_query_keyword (?%s) def\n", option->keyword);
					/* Set keyword for error reporting */
    fputs("{ (", stdout);
    for (valptr = attr->value; *valptr; valptr ++)
    {
      if (*valptr == '(' || *valptr == ')' || *valptr == '\\')
        putchar('\\');
      putchar(*valptr);
    }
    fputs(") cvx exec } stopped { cups_handleerror } if clear\n", stdout);
    					/* Send query code */
    fflush(stdout);

    datalen = 0;
    cupsSideChannelDoRequest(CUPS_SC_CMD_DRAIN_OUTPUT, buffer, &datalen, 5.0);

   /*
    * Read the response data...
    */

    bufptr    = buffer;
    buffer[0] = '\0';
    while ((bytes = cupsBackChannelRead(bufptr, sizeof(buffer) - (size_t)(bufptr - buffer) - 1, 10.0)) > 0)
    {
     /*
      * No newline at the end? Go on reading ...
      */

      bufptr += bytes;
      *bufptr = '\0';

      if (bytes == 0 ||
          (bufptr > buffer && bufptr[-1] != '\r' && bufptr[-1] != '\n'))
	continue;

     /*
      * Trim whitespace and control characters from both ends...
      */

      bytes = bufptr - buffer;

      for (bufptr --; bufptr >= buffer; bufptr --)
        if (isspace(*bufptr & 255) || iscntrl(*bufptr & 255))
	  *bufptr = '\0';
	else
	  break;

      for (bufptr = buffer; isspace(*bufptr & 255) || iscntrl(*bufptr & 255);
	   bufptr ++);

      if (bufptr > buffer)
      {
        _cups_strcpy(buffer, bufptr);
	bufptr = buffer;
      }

      fprintf(stderr, "DEBUG: Got %d bytes.\n", (int)bytes);

     /*
      * Skip blank lines...
      */

      if (!buffer[0])
        continue;

     /*
      * Check the response...
      */

      if ((bufptr = strchr(buffer, ':')) != NULL)
      {
       /*
        * PostScript code for this option in the PPD is broken; show the
        * interpreter's error message that came back...
        */

	fprintf(stderr, "DEBUG%s\n", bufptr);
	break;
      }

     /*
      * Verify the result is a valid option choice...
      */

      if (!ppdFindChoice(option, buffer))
      {
	if (!strcasecmp(buffer, "Unknown"))
	  break;

	bufptr    = buffer;
	buffer[0] = '\0';
        continue;
      }

     /*
      * Write out the result and move on to the next option...
      */

      fprintf(stderr, "PPD: Default%s=%s\n", option->keyword, buffer);
      break;
    }

   /*
    * Printer did not answer this option's query
    */

    if (bytes <= 0)
    {
      fprintf(stderr,
	      "DEBUG: No answer to query for option %s within 10 seconds.\n",
	      option->keyword);
      status = 1;
    }
  }

 /*
  * Finish the job...
  */

  fflush(stdout);
  end_ps(ppd);

 /*
  * Return...
  */

  if (status)
    _cupsLangPrintFilter(stderr, "WARNING",
                         _("Unable to configure printer options."));

  return (0);
}


/*
 * 'begin_ps()' - Send the standard PostScript prolog.
 */

static void
begin_ps(ppd_file_t *ppd,		/* I - PPD file */
         const char *user)		/* I - Username */
{
  (void)user;

  if (ppd->jcl_begin)
  {
    fputs(ppd->jcl_begin, stdout);
    fputs(ppd->jcl_ps, stdout);
  }

  puts("%!");
  puts("userdict dup(\\004)cvn{}put (\\004\\004)cvn{}put\n");

  fflush(stdout);
}


/*
 * 'end_ps()' - Send the standard PostScript trailer.
 */

static void
end_ps(ppd_file_t *ppd)			/* I - PPD file */
{
  if (ppd->jcl_end)
    fputs(ppd->jcl_end, stdout);
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

  begin_ps(ppd, user);

 /*
  * Send a simple file the draws a box around the imageable area and shows
  * the product/interpreter information...
  */

  puts("\r%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
       "%%%%%%%%%%%%%\n"
       "\r%%%% If you can read this, you are using the wrong driver for your "
       "printer. %%%%\n"
       "\r%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
       "%%%%%%%%%%%%%\n"
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

  end_ps(ppd);
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

  begin_ps(ppd, user);

 /*
  * Don't bother sending any additional PostScript commands, since we just
  * want the backend to have enough time to collect the supply info.
  */

 /*
  * Finish the job...
  */

  end_ps(ppd);
}


/*
 * End of "$Id: commandtops.c 11558 2014-02-06 18:33:34Z msweet $".
 */
