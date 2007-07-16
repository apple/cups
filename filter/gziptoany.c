/*
 * "$Id: gziptoany.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   GZIP/raw pre-filter for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1993-2007 by Easy Software Products.
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
 *   main() - Copy (and uncompress) files to stdout.
 */

/*
 * Include necessary headers...
 */

#include <cups/file.h>
#include <cups/string.h>
#include <cups/i18n.h>
#include <stdlib.h>
#include <errno.h>


/*
 * 'main()' - Copy (and uncompress) files to stdout.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  cups_file_t	*fp;			/* File */
  char		buffer[8192];		/* Data buffer */
  int		bytes;			/* Number of bytes read/written */
  int		copies;			/* Number of copies */


 /*
  * Check command-line...
  */

  if (argc != 7)
  {
    fprintf(stderr, _("Usage: %s job-id user title copies options file\n"),
            argv[0]);
    return (1);
  }

 /*
  * Get the copy count; if we have no final content type, this is a
  * raw queue or raw print file, so we need to make copies...
  */

  if (!getenv("FINAL_CONTENT_TYPE"))
    copies = atoi(argv[4]);
  else
    copies = 1;

 /*
  * Open the file...
  */

  if ((fp = cupsFileOpen(argv[6], "r")) == NULL)
  {
    fprintf(stderr, _("ERROR: Unable to open file \"%s\": %s\n"), argv[6],
            strerror(errno));
    return (1);
  }

 /*
  * Copy the file to stdout...
  */

  setbuf(stdout, NULL);

  while (copies > 0)
  {
    if (!getenv("FINAL_CONTENT_TYPE"))
      fputs("PAGE: 1 1\n", stderr);

    cupsFileRewind(fp);

    while ((bytes = cupsFileRead(fp, buffer, sizeof(buffer))) > 0)
      if (fwrite(buffer, 1, bytes, stdout) < bytes)
      {
	fprintf(stderr,
	        _("ERROR: Unable to write uncompressed document data: %s\n"),
        	strerror(ferror(stdout)));
	cupsFileClose(fp);

	return (1);
      }

    copies --;
  }

 /*
  * Close the file and return...
  */

  cupsFileClose(fp);

  return (0);
}


/*
 * End of "$Id: gziptoany.c 6649 2007-07-11 21:46:42Z mike $".
 */
