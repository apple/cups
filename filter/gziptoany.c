/*
 * "$Id: gziptoany.c,v 1.1.2.1 2003/03/30 21:49:14 mike Exp $"
 *
 *   GZIP pre-filter for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-2003 by Easy Software Products.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main() - Uncompress gzip'd files and send them to stdout...
 */

/*
 * Include necessary headers...
 */

#include <cups/string.h>
#include <errno.h>

#ifdef HAVE_LIBZ
#  include <zlib.h>
#endif /* HAVE_LIBZ */


/*
 * 'main()' - Uncompress gzip'd files and send them to stdout...
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
#ifdef HAVE_LIBZ
  gzFile	fp;			/* GZIP'd file */
  char		buffer[8192];		/* Data buffer */
  int		bytes;			/* Number of bytes read/written */


 /*
  * Check command-line...
  */

  if (argc != 7)
  {
    fputs("ERROR: gziptoany job-id user title copies options file\n", stderr);
    return (1);
  }

 /*
  * Open the gzip file...
  */

  if ((fp = gzopen(argv[6], "rb")) == NULL)
  {
    fprintf(stderr, "ERROR: Unable to open GZIP file: %s\n", strerror(errno));
    return (1);
  }

 /*
  * Copy the gzip file to stdout...
  */

  setbuf(stdout, NULL);

  while ((bytes = gzread(fp, buffer, sizeof(buffer))) > 0)
    if (fwrite(buffer, 1, bytes, stdout) < bytes)
    {
      fprintf(stderr, "ERROR: Unable to write uncompressed document data: %s\n",
              strerror(ferror(stdout)));
      gzclose(fp);

      return (1);
    }

 /*
  * Close the file and return...
  */

  gzclose(fp);

  return (0);
#else
  fputs("INFO: Hint: recompile CUPS with ZLIB.\n", stderr);
  fputs("ERROR: GZIP compression support not compiled in!\n", stderr);
  return (1);
#endif /* HAVE_LIBZ */
}


/*
 * End of "$Id: gziptoany.c,v 1.1.2.1 2003/03/30 21:49:14 mike Exp $".
 */
