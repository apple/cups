/*
 * "$Id: ipp-var.h,v 1.5.2.5 2003/07/20 03:49:45 mike Exp $"
 *
 *   IPP variable definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products.
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
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/debug.h>
#include <cups/language.h>
#include <cups/string.h>
#include "cgi.h"


/*
 * Definitions...
 */

#define TEMPLATES	ippGetTemplateDir()


/*
 * Prototype...
 */

extern void	ippGetAttributes(ipp_t *request, const char *directory,
		                 const char *tmpl, const char *lang);
extern char	*ippGetTemplateDir(void);
extern void	ippSetServerVersion(void);
extern void	ippSetCGIVars(ipp_t *, const char *, const char *,
		              const char *, int);


/*
 * End of "$Id: ipp-var.h,v 1.5.2.5 2003/07/20 03:49:45 mike Exp $".
 */
