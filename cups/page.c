/*
 * "$Id: page.c,v 1.2 1998/06/12 20:33:20 mike Exp $"
 *
 *   Page size functions for the PostScript Printer Description (PPD) file
 *   library.
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
 *   ppdPageSize()   - Get the page size record for the given size.
 *   ppdPageWidth()  - Get the page width for the given size.
 *   ppdPageLength() - Get the page length for the given size.
 *
 * Revision History:
 *
 *   $Log: page.c,v $
 *   Revision 1.2  1998/06/12 20:33:20  mike
 *   First working version.
 *
 *   Revision 1.1  1998/06/11 20:56:29  mike
 *   Initial revision
 */

/*
 * Include necessary headers...
 */

#include "ppd.h"


/*
 * 'ppdPageSize()' - Get the page size record for the given size.
 */

ppd_size_t *			/* O - Size record for page or NULL */
ppdPageSize(ppd_file_t *ppd,	/* I - PPD file record */
            char       *name)	/* I - Size name */
{
  int	i;			/* Looping var */


  if (ppd == NULL || name == NULL)
    return (NULL);

  for (i = 0; i < ppd->num_sizes; i ++)
    if (strcmp(name, ppd->sizes[i].name) == 0)
      return (ppd->sizes + i);

  return (NULL);
}


/*
 * 'ppdPageWidth()' - Get the page width for the given size.
 */

float				/* O - Width of page in points or 0.0 */
ppdPageWidth(ppd_file_t *ppd,	/* I - PPD file record */
             char       *name)	/* I - Size name */
{
  ppd_size_t	*size;		/* Page size */


  if ((size = ppdPageSize(ppd, name)) == NULL)
    return (0.0);
  else
    return (size->width);
}


/*
 * 'ppdPageLength()' - Get the page length for the given size.
 */

float				/* O - Length of page in points or 0.0 */
ppdPageLength(ppd_file_t *ppd,	/* I - Size name */
              char       *name)	/* I - Size name */
{
  ppd_size_t	*size;		/* Page size */


  if ((size = ppdPageSize(ppd, name)) == NULL)
    return (0.0);
  else
    return (size->length);
}


/*
 * End of "$Id: page.c,v 1.2 1998/06/12 20:33:20 mike Exp $".
 */
