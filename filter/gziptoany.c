/*
 * "$Id: gziptoany.c 10996 2013-05-29 11:51:34Z msweet $"
 *
 *   GZIP/raw pre-filter for CUPS.
 *
 *   Copyright 2007-2012 by Apple Inc.
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

#include <cups/cups-private.h>


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
    _cupsLangPrintf(stderr,
                    _("Usage: %s job-id user title copies options [file]"),
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
    _cupsLangPrintError("ERROR", _("Unable to open print file"));
    return (1);
  }

 /*
  * Copy the file to stdout...
  */

  while (copies > 0)
  {
    if (!getenv("FINAL_CONTENT_TYPE"))
      fputs("PAGE: 1 1\n", stderr);

    cupsFileRewind(fp);

    while ((bytes = cupsFileRead(fp, buffer, sizeof(buffer))) > 0)
      if (write(1, buffer, bytes) < bytes)
      {
	_cupsLangPrintFilter(stderr, "ERROR",
			     _("Unable to write uncompressed print data: %s"),
			     strerror(errno));
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
 * End of "$Id: gziptoany.c 10996 2013-05-29 11:51:34Z msweet $".
 */
