/*
 * "$Id: emit.c,v 1.2 1998/06/12 20:33:20 mike Exp $"
 *
 *   PPD code emission routines for the PostScript Printer Description (PPD)
 *   file library.
 *
 *   Copyright 1997-1998 by Easy Software Products.
 *
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
 *   This library is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU Library General Public License as published
 *   by the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 *   USA.
 *
 * Contents:
 *
 *   ppdEmit()     - Emit code for marked options to a file.
 *   ppdEmitFd()   - Emit code for marked options to a file.
 *
 * Revision History:
 *
 *   $Log: emit.c,v $
 *   Revision 1.2  1998/06/12 20:33:20  mike
 *   First working version.
 *
 *   Revision 1.1  1998/06/11 19:32:03  mike
 *   Initial revision
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
 * End of "$Id: emit.c,v 1.2 1998/06/12 20:33:20 mike Exp $".
 */
