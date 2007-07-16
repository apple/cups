/*
 * "$Id: form-main.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   CUPS form main entry for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2005 by Easy Software Products.
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
 *   main() - Load the specified form file and output PostScript.
 */

/*
 * Include necessary headers...
 */

#include "form.h"


/*
 * Globals...
 */

int		NumOptions;	/* Number of command-line options */
cups_option_t	*Options;	/* Command-line options */
ppd_file_t	*PPD;		/* PPD file */


/*
 * 'main()' - Load the specified form file and output PostScript.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{

  return (0);
}


/*
 * End of "$Id: form-main.c 6649 2007-07-11 21:46:42Z mike $".
 */
