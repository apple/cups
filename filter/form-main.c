/*
 * "$Id: form-main.c,v 1.3.2.2 2002/03/01 19:55:16 mike Exp $"
 *
 *   CUPS form main entry for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
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
 * End of "$Id: form-main.c,v 1.3.2.2 2002/03/01 19:55:16 mike Exp $".
 */
