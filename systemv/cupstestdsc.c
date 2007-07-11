/*
 * "$Id$"
 *
 *   DSC test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 2006 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   PostScript is a trademark of Adobe Systems, Inc.
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main()  - Main entry for test program.
 *   check() - Check a file for conformance.
 *   usage() - Show program usage.
 */

/*
 * Include necessary headers...
 */

#include <cups/string.h>
#include <cups/cups.h>
#include <cups/file.h>
#include <cups/i18n.h>
#include <errno.h>
#include <stdlib.h>


/*
 * Local functions...
 */

static int	check_file(const char *filename);
static void	usage(void);


/*
 * 'main()' - Main entry for test program.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  int		status;			/* Status of tests */
  int		num_files;		/* Number of files tested */


  _cupsSetLocale(argv);

 /*
  * Collect command-line arguments...
  */

  for (i = 1, num_files = 0, status = 0; i < argc; i ++)
    if (argv[i][0] == '-')
    {
      if (argv[i][1])
      {
       /*
        * Currently the only supported option is "-h" (help)...
	*/

        usage();
      }
      else
      {
        num_files ++;
	status += check_file("(stdin)");
      }
    }
    else
    {
      num_files ++;
      status += check_file(argv[i]);
    }

  if (!num_files)
    usage();

  return (status);
}


/*
 * 'check()' - Check a file for conformance.
 */

static int				/* O - 0 on success, 1 on failure */
check_file(const char *filename)	/* I - File to read from */
{
  int		i;			/* Looping var */
  cups_file_t	*fp;			/* File */
  char		line[1024];		/* Line from file */
  int		ch;			/* Current character */
  size_t	bytes;			/* Length of line */
  int		status;			/* Status of test */
  int		linenum;		/* Line number */
  int		binary;			/* File contains binary data? */
  float		version;		/* DSC version */
  int		lbrt[4];		/* Bounding box */
  char		page_label[256];	/* Page label string */
  int		page_number;		/* Page number */
  int		last_page_number;	/* Last page number seen */
  int		level;			/* Embedded document level */
  int		saw_bounding_box,	/* %%BoundingBox seen? */
		saw_pages,		/* %%Pages seen? */
		saw_end_comments,	/* %%EndComments seen? */
		saw_begin_prolog,	/* %%BeginProlog seen? */
		saw_end_prolog,		/* %%EndProlog seen? */
		saw_begin_setup,	/* %%BeginSetup seen? */
		saw_end_setup,		/* %%EndSetup seen? */
		saw_page,		/* %%Page seen? */
		saw_trailer,		/* %%Trailer seen? */
		saw_long_line;		/* Saw long lines? */


 /*
  * Open the file...
  */

  if (!strcmp(filename, "(stdin)"))
    fp = cupsFileStdin();
  else
    fp = cupsFileOpen(filename, "r");

  if (!fp)
  {
    perror(filename);
    return (1);
  }

 /*
  * Scan the file...
  */

  binary           = 0;
  last_page_number = 0;
  level            = 0;
  linenum          = 0;
  saw_begin_prolog = 0;
  saw_begin_setup  = 0;
  saw_bounding_box = 0;
  saw_end_comments = 0;
  saw_end_prolog   = 0;
  saw_end_setup    = 0;
  saw_long_line    = 0;
  saw_page         = 0;
  saw_pages        = 0;
  saw_trailer      = 0;
  status           = 0;
  version          = 0.0f;

  _cupsLangPrintf(stdout, "%s: ", filename);
  fflush(stdout);

  while ((bytes = cupsFileGetLine(fp, line, sizeof(line))) > 0)
  {
    linenum ++;

    if (bytes > 255)
    {
      if (!saw_long_line)
      {
	if (!status)
          _cupsLangPuts(stdout, _("FAIL\n"));

	status ++;
	_cupsLangPrintf(stdout,
                	_("    Line %d is longer than 255 characters (%d)!\n"
		          "        REF: Page 25, Line Length\n"),
			linenum, (int)bytes);
      }

      saw_long_line ++;
    }

    if (linenum == 1)
    {
      if (strncmp(line, "%!PS-Adobe-", 11))
      {
	if (!status)
          _cupsLangPuts(stdout, _("FAIL\n"));

	status ++;
	_cupsLangPuts(stdout,
	              _("    Missing %!PS-Adobe-3.0 on first line!\n"
		        "        REF: Page 17, 3.1 Conforming Documents\n"));
	cupsFileClose(fp);
	return (1);
      }
      else
        version = atof(line + 11);
    }
    else if (level > 0)
    {
      if (!strncmp(line, "%%BeginDocument:", 16))
        level ++;
      else if (!strncmp(line, "%%EndDocument", 13))
        level --;
    }
    else if (saw_trailer)
    {
      if (!strncmp(line, "%%Pages:", 8))
      {
        if (atoi(line + 8) <= 0)
	{
	  if (!status)
            _cupsLangPuts(stdout, _("FAIL\n"));

	  status ++;
	  _cupsLangPrintf(stdout,
	                  _("    Bad %%%%Pages: on line %d!\n"
		            "        REF: Page 43, %%%%Pages:\n"),
			  linenum);
	}
	else
          saw_pages = 1;
      }
      else if (!strncmp(line, "%%BoundingBox:", 14))
      {
	if (sscanf(line + 14, "%d%d%d%d", lbrt + 0, lbrt + 1, lbrt + 2,
	           lbrt + 3) != 4)
        {
	  if (!status)
            _cupsLangPuts(stdout, _("FAIL\n"));

	  status ++;
	  _cupsLangPrintf(stdout, _("    Bad %%%%BoundingBox: on line %d!\n"
		        	    "        REF: Page 39, %%%%BoundingBox:\n"),
			  linenum);
	}
	else
          saw_bounding_box = 1;
      }
    }
    else if (!saw_end_comments)
    {
      if (!strncmp(line, "%%EndComments", 13))
        saw_end_comments = 1;
      else if (line[0] != '%')
        saw_end_comments = -1;
      else if (!strncmp(line, "%%Pages:", 8))
      {
        if (strstr(line + 8, "(atend)"))
	  saw_pages = -1;
	else if (atoi(line + 8) <= 0)
	{
	  if (!status)
            _cupsLangPuts(stdout, _("FAIL\n"));

	  status ++;
	  _cupsLangPrintf(stdout, _("    Bad %%%%Pages: on line %d!\n"
		        	    "        REF: Page 43, %%%%Pages:\n"),
			  linenum);
	}
	else
          saw_pages = 1;
      }
      else if (!strncmp(line, "%%BoundingBox:", 14))
      {
        if (strstr(line, "(atend)"))
	  saw_bounding_box = -1;
	else if (sscanf(line + 14, "%d%d%d%d", lbrt + 0, lbrt + 1, lbrt + 2,
	                lbrt + 3) != 4)
        {
	  if (!status)
            _cupsLangPuts(stdout, _("FAIL\n"));

	  status ++;
	  _cupsLangPrintf(stdout, _("    Bad %%%%BoundingBox: on line %d!\n"
		        	    "        REF: Page 39, %%%%BoundingBox:\n"),
			  linenum);
	}
	else
          saw_bounding_box = 1;
      }
    }
    else if (saw_begin_prolog && !saw_end_prolog)
    {
      if (!strncmp(line, "%%EndProlog", 11))
        saw_end_prolog = 1;
    }
    else if (saw_begin_setup && !saw_end_setup)
    {
      if (!strncmp(line, "%%EndSetup", 10))
        saw_end_setup = 1;
    }
    else if (saw_end_comments)
    {
      if (!strncmp(line, "%%Page:", 7))
      {
        if (sscanf(line + 7, "%255s%d", page_label, &page_number) != 2 ||
	    page_number != (last_page_number + 1) || page_number < 1)
	{
	  if (!status)
            _cupsLangPuts(stdout, _("FAIL\n"));

	  status ++;
	  _cupsLangPrintf(stdout, _("    Bad %%%%Page: on line %d!\n"
		        	    "        REF: Page 53, %%%%Page:\n"),
		          linenum);
	}
	else
	{
	  last_page_number = page_number;
	  saw_page         = 1;
	}
      }
      else if (!strncmp(line, "%%BeginProlog", 13))
        saw_begin_prolog = 1;
      else if (!strncmp(line, "%%BeginSetup", 12))
        saw_begin_setup = 1;
      else if (!strncmp(line, "%%BeginDocument:", 16))
        level ++;
      else if (!strncmp(line, "%%EndDocument", 13))
        level --;
      else if (!strncmp(line, "%%Trailer", 9))
        saw_trailer = 1;
    }

    for (i = 0; !binary && i < bytes; i ++)
    {
      ch = line[i];

      if ((ch < ' ' || (ch & 0x80)) && ch != '\n' && ch != '\r' && ch != '\t')
        binary = 1;
    }
  }

  if (saw_bounding_box <= 0)
  {
    if (!status)
      _cupsLangPuts(stdout, _("FAIL\n"));

    status ++;
    _cupsLangPuts(stdout, _("    Missing or bad %%BoundingBox: comment!\n"
		            "        REF: Page 39, %%BoundingBox:\n"));
  }

  if (saw_pages <= 0)
  {
    if (!status)
      _cupsLangPuts(stdout, _("FAIL\n"));

    status ++;
    _cupsLangPuts(stdout, _("    Missing or bad %%Pages: comment!\n"
		            "        REF: Page 43, %%Pages:\n"));
  }

  if (!saw_end_comments)
  {
    if (!status)
      _cupsLangPuts(stdout, _("FAIL\n"));

    status ++;
    _cupsLangPuts(stdout, _("    Missing %%EndComments comment!\n"
		            "        REF: Page 41, %%EndComments\n"));
  }

  if (!saw_page)
  {
    if (!status)
      _cupsLangPuts(stdout, _("FAIL\n"));

    status ++;
    _cupsLangPuts(stdout, _("    Missing or bad %%Page: comments!\n"
		            "        REF: Page 53, %%Page:\n"));
  }

  if (level < 0)
  {
    if (!status)
      _cupsLangPuts(stdout, _("FAIL\n"));

    status ++;
    _cupsLangPuts(stdout, _("    Too many %%EndDocument comments!\n"));
  }
  else if (level > 0)
  {
    if (!status)
      _cupsLangPuts(stdout, _("FAIL\n"));

    status ++;
    _cupsLangPuts(stdout, _("    Too many %%BeginDocument comments!\n"));
  }

  if (saw_long_line > 1)
    _cupsLangPrintf(stderr,
                    _("    Saw %d lines that exceeded 255 characters!\n"),
                    saw_long_line);

  if (!status)
    _cupsLangPuts(stdout, _("PASS\n"));

  if (binary)
    _cupsLangPuts(stdout, _("    Warning: file contains binary data!\n"));

  if (version < 3.0f)
    _cupsLangPrintf(stdout,
                    _("    Warning: obsolete DSC version %.1f in file!\n"),
		    version);

  if (saw_end_comments < 0)
    _cupsLangPuts(stdout, _("    Warning: no %%EndComments comment in file!\n"));

  cupsFileClose(fp);

  return (status);
}


/*
 * 'usage()' - Show program usage.
 */

static void
usage(void)
{
  _cupsLangPuts(stdout,
                _("Usage: cupstestdsc [options] filename.ps [... filename.ps]\n"
                  "       cupstestdsc [options] -\n"
		  "\n"
		  "Options:\n"
		  "\n"
		  "    -h       Show program usage\n"
		  "\n"
		  "    Note: this program only validates the DSC comments, "
		  "not the PostScript itself.\n"));
  
  exit(1);
}


/*
 * End of "$Id$".
 */
