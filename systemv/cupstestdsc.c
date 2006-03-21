/*
 * "$Id$"
 *
 *   DSC test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2006 by Easy Software Products, all rights reserved.
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
 *   PostScript is a trademark of Adobe Systems, Inc.
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
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

static size_t	getline(cups_file_t *fp, char *buffer, size_t bufsize);
static void	usage(void);


/*
 * 'main()' - Main entry for test program.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  char		*filename;		/* File we are testing */
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
		saw_eof;		/* %%EOF seen? */


 /*
  * Collect command-line arguments...
  */

  for (i = 1, filename = NULL, fp = NULL; i < argc; i ++)
    if (argv[i][0] == '-')
    {
      usage();
    }
    else if (fp)
      usage();
    else if ((fp = cupsFileOpen(argv[i], "r")) == NULL)
    {
      perror(argv[i]);
      return (1);
    }
    else
      filename = argv[i];

  if (!fp)
    usage();

 /*
  * Scan the file...
  */

  binary           = 0;
  level            = 0;
  linenum          = 0;
  saw_begin_prolog = 0;
  saw_begin_setup  = 0;
  saw_bounding_box = 0;
  saw_end_comments = 0;
  saw_end_prolog   = 0;
  saw_end_setup    = 0;
  saw_eof          = 0;
  saw_page         = 0;
  saw_pages        = 0;
  saw_trailer      = 0;
  status           = 0;
  version          = 0.0f;

  printf("%s: ", filename);
  fflush(stdout);

  while ((bytes = getline(fp, line, sizeof(line))) > 0)
  {
    linenum ++;

    if (bytes > 255)
    {
      if (!status)
        puts("FAIL");

      status ++;
      printf("    Line %d is longer than 255 characters (%d)!\n", linenum,
             (int)bytes);
    }

    if (linenum == 1)
    {
      if (strncmp(line, "%!PS-Adobe-", 11))
      {
	if (!status)
          puts("FAIL");

	status ++;
	puts("    Missing %!PS-Adobe-3.0 on first line!");
	break;
      }
      else
        version = atof(line + 11);
    }
    else if (level > 0)
    {
      if (!strncmp(line, "%%BeginDocument:", 16) ||
          !strncmp(line, "%%BeginDocument ", 16) ||
					/* Adobe Acrobat BUG */
	  !strncmp(line, "%ADO_BeginApplication", 21))
        level ++;
      else if (!strncmp(line, "%%EndDocument", 13) ||
	       !strncmp(line, "%ADO_EndApplication", 19))
        level --;
    }
    else if (saw_trailer)
    {
      if (!strncmp(line, "%%Pages:", 8))
      {
        if (atoi(line + 8) <= 0)
	{
	  if (!status)
            puts("FAIL");

	  status ++;
	  printf("    Bad %%%%Pages: on line %d!\n", linenum);
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
            puts("FAIL");

	  status ++;
	  printf("    Bad %%%%BoundingBox: on line %d!\n", linenum);
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
            puts("FAIL");

	  status ++;
	  printf("    Bad %%%%Pages: on line %d!\n", linenum);
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
            puts("FAIL");

	  status ++;
	  printf("    Bad %%%%BoundingBox: on line %d!\n", linenum);
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
        if (sscanf(line + 7, "%255s%d", page_label, &page_number) != 2)
	{
	  if (!status)
            puts("FAIL");

	  status ++;
	  printf("    Bad %%%%Page: on line %d!\n", linenum);
	}
	else
	  saw_page = 1;
      }
      else if (!strncmp(line, "%%BeginProlog", 13))
        saw_begin_prolog = 1;
      else if (!strncmp(line, "%%BeginSetup", 12))
        saw_begin_setup = 1;
      else if (!strncmp(line, "%%BeginDocument:", 16) ||
               !strncmp(line, "%%BeginDocument ", 16) ||
					/* Adobe Acrobat BUG */
	       !strncmp(line, "%ADO_BeginApplication", 21))
        level ++;
      else if (!strncmp(line, "%%EndDocument", 13) ||
	       !strncmp(line, "%ADO_EndApplication", 19))
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
      puts("FAIL");

    status ++;
    puts("    Missing or bad %%BoundingBox: comment!");
  }

  if (saw_pages <= 0)
  {
    if (!status)
      puts("FAIL");

    status ++;
    puts("    Missing or bad %%Pages: comment!");
  }

  if (!saw_end_comments)
  {
    if (!status)
      puts("FAIL");

    status ++;
    puts("    Missing %%EndComments comment!");
  }

  if (!saw_page)
  {
    if (!status)
      puts("FAIL");

    status ++;
    puts("    Missing or bad %%Page: comments!");
  }

  if (level < 0)
  {
    if (!status)
      puts("FAIL");

    status ++;
    puts("    Too many %%EndDocument comments!");
  }
  else if (level > 0)
  {
    if (!status)
      puts("FAIL");

    status ++;
    puts("    Too many %%BeginDocument comments!");
  }

  if (!status)
    puts("PASS");

  if (binary)
    puts("    Warning: file contains binary data!");

  if (version < 3.0f)
    printf("    Warning: obsolete DSC version %.1f in file!\n", version);

  if (saw_end_comments < 0)
    puts("    Warning: no %%EndComments comment in file!");

  cupsFileClose(fp);

  return (status);
}


/*
 * 'getline()' - Get a line from a file.
 */

static size_t				/* O - Bytes in line */
getline(cups_file_t *fp,		/* I - File */
        char        *buffer,		/* I - Line buffer */
	size_t      bufsize)		/* I - Size of buffer */
{
  int	ch;				/* Character from file */
  char	*bufptr,			/* Pointer into buffer */
	*bufend;			/* End of buffer */


 /*
  * Scan the line ending with CR, LF, or CR LF.
  */

  bufptr = buffer;
  bufend = buffer + bufsize - 2;

  while ((ch = cupsFileGetChar(fp)) != EOF)
  {
    if (bufptr >= bufend)
      break;

    *bufptr++ = ch;

    if (ch == 0x0d)
    {
      if (cupsFilePeekChar(fp) == 0x0a)
        *bufptr++ = cupsFileGetChar(fp);

      break;
    }
    else if (ch == 0x0a)
      break;
  }

  *bufptr = '\0';
       
  return (bufptr - buffer);
}


/*
 * 'usage()' - Show program usage.
 */

static void
usage(void)
{
  puts("Usage: cupstestdsc filename.ps");
  exit(1);
}


/*
 * End of "$Id$".
 */
