/*
 * "$Id: util.c,v 1.1 1999/02/05 17:40:58 mike Exp $"
 *
 *   Printing utilities for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products.
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 */

/*
 * Include necessary headers...
 */

#include "cups.h"


int
cupsCancelJob(char *printer,
              int  job)
{
}


int
cupsGetClasses(char ***classes)
{
}


char *
cupsGetPPD(char *printer)
{
}


int
cupsGetPrinters(char ***printers)
{
}


int
cupsPrintFile(char          *printer,
              char          *filename,
              int           num_options,
	      cups_option_t *options)
{
}


/*
 * End of "$Id: util.c,v 1.1 1999/02/05 17:40:58 mike Exp $".
 */
