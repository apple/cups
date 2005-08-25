/*
 * "$Id$"
 *
 *   On-line help CGI for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
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
 * Contents:
 *
 */

/*
 * Include necessary headers...
 */

#include "ipp-var.h"


/*
 * 'main()' - Main entry for CGI.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{


 /*
  * Get any form variables...
  */

  cgiInitialize();

 /*
  * Set the web interface section...
  */

  cgiSetVariable("SECTION", "help");

 /*
  * Send a standard header...
  */

  cgiStartHTML("Help");

 /*
  * Send a standard trailer...
  */

  cgiEndHTML();

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * End of "$Id$".
 */
