/*
 * "$Id$"
 *
 *   GZIP/raw pre-filter for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-2007 by Easy Software Products.
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
    fputs("ERROR: gziptoany job-id user title copies options file\n", stderr);
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
    fprintf(stderr, "ERROR: Unable to open file \"%s\": %s\n", argv[6],
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
	        "ERROR: Unable to write uncompressed document data: %s\n",
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
 * End of "$Id$".
 */
