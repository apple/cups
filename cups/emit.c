/*
 * "$Id: emit.c,v 1.3 1999/01/24 14:18:43 mike Exp $"
 *
 *   PPD code emission routines for the Common UNIX Printing System (CUPS).
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
 *   ppdEmit()     - Emit code for marked options to a file.
 *   ppdEmitFd()   - Emit code for marked options to a file.
 */

/*
 * Include necessary headers...
 */

#include "ppd.h"


/*
 * 'ppdEmit()' - Emit code for marked options to a file.
 */

int					/* O - 0 on success, -1 on failure */
ppdEmit(ppd_file_t    *ppd,		/* I - PPD file record */
        FILE          *fp,		/* I - File to write to */
        ppd_section_t section)		/* I - Section to write */
{
  return (0);
}


/*
 * 'ppdEmitFd()' - Emit code for marked options to a file.
 */

int					/* O - 0 on success, -1 on failure */
ppdEmitFd(ppd_file_t    *ppd,		/* I - PPD file record */
          int           fd,		/* I - File to write to */
          ppd_section_t section)	/* I - Section to write */
{
  return (0);
}


/*
 * End of "$Id: emit.c,v 1.3 1999/01/24 14:18:43 mike Exp $".
 */
