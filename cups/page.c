/*
 * "$Id: page.c,v 1.3 1999/01/24 14:18:43 mike Exp $"
 *
 *   Page size functions for the Common UNIX Printing System (CUPS).
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
 *   ppdPageSize()   - Get the page size record for the given size.
 *   ppdPageWidth()  - Get the page width for the given size.
 *   ppdPageLength() - Get the page length for the given size.
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
 * End of "$Id: page.c,v 1.3 1999/01/24 14:18:43 mike Exp $".
 */
