/*
 * "$Id$"
 *
 *   PPD custom option routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
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
 *   PostScript is a trademark of Adobe Systems, Inc.
 *
 *   This code and any derivative of it may be used and distributed
 *   freely under the terms of the GNU General Public License when
 *   used with GNU Ghostscript or its derivatives.  Use of the code
 *   (or any derivative of it) with software other than GNU
 *   GhostScript (or its derivatives) is governed by the CUPS license
 *   agreement.
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   ppdFindCustomOption() - Find a custom option.
 *   ppdFindCustomParam()  - Find a parameter for a custom option.
 *   ppdFirstCustomParam() - Return the first parameter for a custom option.
 *   ppdNextCustomParam()  - Return the next parameter for a custom option.
 */

/*
 * Include necessary headers.
 */

#include "globals.h"
#include "debug.h"


/*
 * 'ppdFindCustomOption()' - Find a custom option.
 *
 * @since CUPS 1.2@
 */

ppd_coption_t *				/* O - Custom option or NULL */
ppdFindCustomOption(ppd_file_t *ppd,	/* I - PPD file */
                    const char *keyword)/* I - Custom option name */
{
  ppd_coption_t	key;			/* Custom option search key */


  if (!ppd)
    return (NULL);

  strlcpy(key.keyword, keyword, sizeof(key.keyword));
  return ((ppd_coption_t *)cupsArrayFind(ppd->coptions, &key));
}


/*
 * 'ppdFindCustomParam()' - Find a parameter for a custom option.
 *
 * @since CUPS 1.2@
 */

ppd_cparam_t *				/* O - Custom parameter or NULL */
ppdFindCustomParam(ppd_coption_t *opt,	/* I - Custom option */
                   const char    *name)	/* I - Parameter name */
{
  ppd_cparam_t	key;			/* Custom parameter search key */


  if (!opt)
    return (NULL);

  strlcpy(key.name, name, sizeof(key.name));
  return ((ppd_cparam_t *)cupsArrayFind(opt->params, &key));
}


/*
 * 'ppdFirstCustomParam()' - Return the first parameter for a custom option.
 *
 * @since CUPS 1.2@
 */

ppd_cparam_t *				/* O - Custom parameter or NULL */
ppdFirstCustomParam(ppd_coption_t *opt)	/* I - Custom option */
{
  if (!opt)
    return (NULL);

  return ((ppd_cparam_t *)cupsArrayFirst(opt->params));
}


/*
 * 'ppdNextCustomParam()' - Return the next parameter for a custom option.
 *
 * @since CUPS 1.2@
 */

ppd_cparam_t *				/* O - Custom parameter or NULL */
ppdNextCustomParam(ppd_coption_t *opt)	/* I - Custom option */
{
  if (!opt)
    return (NULL);

  return ((ppd_cparam_t *)cupsArrayNext(opt->params));
}


/*
 * End of "$Id$".
 */
