/*
 * "$Id: mark.c,v 1.3 1999/01/24 14:18:43 mike Exp $"
 *
 *   Option marking routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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
 *   PostScript is a trademark of Adobe Systems, Inc.
 *
 * Contents:
 *
 *   ppdMarkDefaults() - Mark all default options in the PPD file.
 *   ppdMarkOption()   - Mark an option in a PPD file.
 */

/*
 * Include necessary headers...
 */

#include "ppd.h"


int
ppdConflicts(ppd_file_t *ppd)
{
}


int
ppdIsMarked(ppd_file_t *ppd,
            char       *keyword,
            char       *option)
{
}


/*
 * 'ppdMarkDefaults()' - Mark all default options in the PPD file.
 */

void
ppdMarkDefaults(ppd_file_t *ppd)	/* I - PPD file record */
{
}


/*
 * 'ppdMarkOption()' - Mark an option in a PPD file.
 *
 * Notes:
 *
 *   -1 is returned if the given option would conflict with any currently
 *   selected option.
 */

int					/* O - 0 on success, -1 on failure */
ppdMarkOption(ppd_file_t *ppd,		/* I - PPD file record */
              char       *keyword,	/* I - Keyword */
              char       *option)	/* I - Option name */
{
  return (0);
}


/*
 * End of "$Id: mark.c,v 1.3 1999/01/24 14:18:43 mike Exp $".
 */
