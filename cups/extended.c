/*
 * "$Id: extended.c,v 1.1.2.1 2002/08/10 00:05:45 mike Exp $"
 *
 *   Extended option routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products, all rights reserved.
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   ppdFindExtOption() - Return a pointer to the extended option.
 *   ppdMarkNumeric()   - Mark an extended numeric option.
 *   ppdMarkText()      - Mark an extended text option.
 */

/*
 * Include necessary headers...
 */

#include "ppd.h"
#include "string.h"
#include "debug.h"


/*
 * 'ppdFindExtOption()' - Return a pointer to the extended option.
 */

ppd_ext_option_t *			/* O - Pointer to option or NULL */
ppdFindExtOption(ppd_file_t *ppd,	/* I - PPD file data */
                 const char *option)	/* I - Option/Keyword name */
{
  int			i;		/* Looping var */
  ppd_ext_option_t	**o;		/* Pointer to option */


  if (ppd == NULL || option == NULL)
    return (NULL);

  for (i = ppd->num_extended, o = ppd->extended; i > 0; i --, o ++)
    if (strcasecmp(o[0]->keyword, option) == 0)
      return (*o);

  return (NULL);
}


/*
 * 'ppdMarkNumeric()' - Mark an extended numeric option.
 */

int					/* O - Number of conflicts */
ppdMarkNumeric(ppd_file_t *ppd,		/* I - PPD file */
               const char *keyword,	/* I - Option name */
               int        num_values,	/* I - Number of values */
	       float      *values)	/* I - Values */
{
  return (0);
}


/*
 * 'ppdMarkText()' - Mark an extended text option.
 */

int					/* O - Number of conflicts */
ppdMarkText(ppd_file_t *ppd,		/* I - PPD file */
            const char *keyword,	/* I - Option name */
            const char *value)		/* I - Option value */
{
  return (0);
}


/*
 * End of "$Id: extended.c,v 1.1.2.1 2002/08/10 00:05:45 mike Exp $".
 */
