/*
 * "$Id: page.c,v 1.10 1999/12/21 02:26:45 mike Exp $"
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
 *       44141 Airport View Drive, Suite 204
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
#include "string.h"
#include <ctype.h>


/*
 * 'ppdPageSize()' - Get the page size record for the given size.
 */

ppd_size_t *			/* O - Size record for page or NULL */
ppdPageSize(ppd_file_t *ppd,	/* I - PPD file record */
            const char *name)	/* I - Size name */
{
  int	i;			/* Looping var */
  float	w, l;			/* Width and length of page */
  char	units[255];		/* Page size units... */


  if (ppd == NULL)
    return (NULL);

  if (name != NULL)
  {
    if (strncmp(name, "Custom.", 7) == 0 && ppd->variable_sizes)
    {
     /*
      * Find the custom page size...
      */

      for (i = 0; i < ppd->num_sizes; i ++)
	if (strcmp("Custom", ppd->sizes[i].name) == 0)
          break;

      if (i == ppd->num_sizes)
        return (NULL);

     /*
      * Variable size; size name can be one of the following:
      *
      *    Custom.WIDTHxLENGTHin    - Size in inches
      *    Custom.WIDTHxLENGTHcm    - Size in centimeters
      *    Custom.WIDTHxLENGTHmm    - Size in millimeters
      *    Custom.WIDTHxLENGTH[pt]  - Size in points
      */

      units[0] = '\0';
      if (sscanf(name + 7, "%fx%f%254s", &w, &l, units) < 2)
        return (NULL);

      if (strcasecmp(units, "in") == 0)
      {
        ppd->sizes[i].width  = w * 72.0f;
	ppd->sizes[i].length = l * 72.0f;
	ppd->sizes[i].left   = ppd->custom_margins[0];
	ppd->sizes[i].bottom = ppd->custom_margins[1];
	ppd->sizes[i].right  = w * 72.0f - ppd->custom_margins[2];
	ppd->sizes[i].top    = l * 72.0f - ppd->custom_margins[3];
      }
      else if (strcasecmp(units, "cm") == 0)
      {
        ppd->sizes[i].width  = w / 2.54f * 72.0f;
	ppd->sizes[i].length = l / 2.54f * 72.0f;
	ppd->sizes[i].left   = ppd->custom_margins[0];
	ppd->sizes[i].bottom = ppd->custom_margins[1];
	ppd->sizes[i].right  = w / 2.54f * 72.0f - ppd->custom_margins[2];
	ppd->sizes[i].top    = l / 2.54f * 72.0f - ppd->custom_margins[3];
      }
      else if (strcasecmp(units, "mm") == 0)
      {
        ppd->sizes[i].width  = w / 25.4f * 72.0f;
	ppd->sizes[i].length = l / 25.4f * 72.0f;
	ppd->sizes[i].left   = ppd->custom_margins[0];
	ppd->sizes[i].bottom = ppd->custom_margins[1];
	ppd->sizes[i].right  = w / 25.4f * 72.0f - ppd->custom_margins[2];
	ppd->sizes[i].top    = l / 25.4f * 72.0f - ppd->custom_margins[3];
      }
      else
      {
        ppd->sizes[i].width  = w;
	ppd->sizes[i].length = l;
	ppd->sizes[i].left   = ppd->custom_margins[0];
	ppd->sizes[i].bottom = ppd->custom_margins[1];
	ppd->sizes[i].right  = w - ppd->custom_margins[2];
	ppd->sizes[i].top    = l - ppd->custom_margins[3];
      }

      return (ppd->sizes + i);
    }
    else
    {
     /*
      * Lookup by name...
      */

      for (i = 0; i < ppd->num_sizes; i ++)
	if (strcmp(name, ppd->sizes[i].name) == 0)
          return (ppd->sizes + i);
    }
  }
  else
  {
   /*
    * Find default...
    */

    for (i = 0; i < ppd->num_sizes; i ++)
      if (ppd->sizes[i].marked)
        return (ppd->sizes + i);
  }

  return (NULL);
}


/*
 * 'ppdPageWidth()' - Get the page width for the given size.
 */

float				/* O - Width of page in points or 0.0 */
ppdPageWidth(ppd_file_t *ppd,	/* I - PPD file record */
             const char *name)	/* I - Size name */
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
ppdPageLength(ppd_file_t *ppd,	/* I - PPD file */
              const char *name)	/* I - Size name */
{
  ppd_size_t	*size;		/* Page size */


  if ((size = ppdPageSize(ppd, name)) == NULL)
    return (0.0);
  else
    return (size->length);
}


/*
 * End of "$Id: page.c,v 1.10 1999/12/21 02:26:45 mike Exp $".
 */
