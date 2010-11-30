/*
 * "$Id: tbcp.c 6800 2007-08-16 18:28:44Z mike $"
 *
 *   TBCP port monitor for CUPS.
 *
 *   Copyright 2007-2010 by Apple Inc.
 *   Copyright 1993-2006 by Easy Software Products.
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
 *   main()    - Main entry...
 *   psgets()  - Get a line from a file.
 *   pswrite() - Write data from a file.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups-private.h>
#include <cups/ppd.h>


/*
 * Local functions...
 */

static char		*psgets(char *buf, size_t *bytes, FILE *fp);
static size_t		pswrite(const char *buf, size_t bytes, FILE *fp);


/*
 * 'main()' - Main entry...
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  FILE		*fp;			/* File to print */
  int		copies;			/* Number of copies left */
  char		line[1024];		/* Line/buffer from stream/file */
  size_t	linelen;		/* Length of line */


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

  if (argc == 6)
  {
    copies = 1;
    fp     = stdin;
  }
  else
  {
    copies = atoi(argv[4]);
    fp     = fopen(argv[6], "rb");

    if (!fp)
    {
      perror(argv[6]);
      return (1);
    }
  }

 /*
  * Copy the print file to stdout...
  */

  while (copies > 0)
  {
    copies --;

   /*
    * Read the first line...
    */

    linelen = sizeof(line);
    if (psgets(line, &linelen, fp) == NULL)
    {
      fputs("ERROR: Empty print file!\n", stderr);
      return (1);
    }

   /*
    * Handle leading PJL fun...
    */

    if (!strncmp(line, "\033%-12345X", 9) || !strncmp(line, "@PJL ", 5))
    {
     /*
      * Yup, we have leading PJL fun, so copy it until we hit a line
      * with "ENTER LANGUAGE"...
      */

      while (strstr(line, "ENTER LANGUAGE") == NULL)
      {
        fwrite(line, 1, linelen, stdout);

	linelen = sizeof(line);
	if (psgets(line, &linelen, fp) == NULL)
          break;
      }
    }
    else
    {
     /*
      * No PJL stuff, just add the UEL...
      */

      fputs("\033%-12345X", stdout);
    }

   /*
    * Switch to TBCP mode...
    */

    fputs("\001M", stdout);

   /*
    * Loop until we see end-of-file...
    */

    while (pswrite(line, linelen, stdout) > 0)
    {
      linelen = sizeof(line);
      if (psgets(line, &linelen, fp) == NULL)
	break;
    }

    fflush(stdout);
  }

  return (0);
}


/*
 * 'psgets()' - Get a line from a file.
 *
 * Note:
 *
 *   This function differs from the gets() function in that it
 *   handles any combination of CR, LF, or CR LF to end input
 *   lines.
 */

static char *				/* O  - String or NULL if EOF */
psgets(char   *buf,			/* I  - Buffer to read into */
       size_t *bytes,			/* IO - Length of buffer */
       FILE   *fp)			/* I  - File to read from */
{
  char		*bufptr;		/* Pointer into buffer */
  int		ch;			/* Character from file */
  size_t	len;			/* Max length of string */


  len    = *bytes - 1;
  bufptr = buf;
  ch     = EOF;

  while ((bufptr - buf) < len)
  {
    if ((ch = getc(fp)) == EOF)
      break;

    if (ch == '\r')
    {
     /*
      * Got a CR; see if there is a LF as well...
      */

      ch = getc(fp);

      if (ch != EOF && ch != '\n')
      {
        ungetc(ch, fp);	/* Nope, save it for later... */
        ch = '\r';
      }
      else
        *bufptr++ = '\r';
      break;
    }
    else if (ch == '\n')
      break;
    else
      *bufptr++ = ch;
  }

 /*
  * Add a trailing newline if it is there...
  */

  if (ch == '\n' || ch == '\r')
  {
    if ((bufptr - buf) < len)
      *bufptr++ = ch;
    else
      ungetc(ch, fp);
  }

 /*
  * Nul-terminate the string and return it (or NULL for EOF).
  */

  *bufptr = '\0';
  *bytes  = bufptr - buf;

  if (ch == EOF && bufptr == buf)
    return (NULL);
  else
    return (buf);
}


/*
 * 'pswrite()' - Write data from a file.
 */

static size_t				/* O - Number of bytes written */
pswrite(const char *buf,		/* I - Buffer to write */
        size_t     bytes,		/* I - Bytes to write */
	FILE       *fp)			/* I - File to write to */
{
  size_t	count;			/* Remaining bytes */


  for (count = bytes; count > 0; count --, buf ++)
    switch (*buf)
    {
      case 0x04 : /* CTRL-D */
          if (bytes == 1)
	  {
	   /*
	    * Don't quote the last CTRL-D...
	    */

	    putchar(0x04);
	    break;
	  }

      case 0x01 : /* CTRL-A */
      case 0x03 : /* CTRL-C */
      case 0x05 : /* CTRL-E */
      case 0x11 : /* CTRL-Q */
      case 0x13 : /* CTRL-S */
      case 0x14 : /* CTRL-T */
      case 0x1b : /* CTRL-[ (aka ESC) */
      case 0x1c : /* CTRL-\ */
	  if (putchar(0x01) < 0)
	    return (-1);
	  if (putchar(*buf ^ 0x40) < 0)
	    return (-1);
	  break;

      default :
	  if (putchar(*buf) < 0)
	    return (-1);
	  break;
    }

  return (bytes);
}


/*
 * End of "$Id: tbcp.c 6800 2007-08-16 18:28:44Z mike $".
 */
