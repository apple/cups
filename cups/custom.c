/*
 * "$Id: custom.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   PPD custom option routines for CUPS.
 *
 *   Copyright 2007-2012 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
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

#include "cups-private.h"


/*
 * 'ppdFindCustomOption()' - Find a custom option.
 *
 * @since CUPS 1.2/OS X 10.5@
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
 * @since CUPS 1.2/OS X 10.5@
 */

ppd_cparam_t *				/* O - Custom parameter or NULL */
ppdFindCustomParam(ppd_coption_t *opt,	/* I - Custom option */
                   const char    *name)	/* I - Parameter name */
{
  ppd_cparam_t	*param;			/* Current custom parameter */


  if (!opt)
    return (NULL);

  for (param = (ppd_cparam_t *)cupsArrayFirst(opt->params);
       param;
       param = (ppd_cparam_t *)cupsArrayNext(opt->params))
    if (!_cups_strcasecmp(param->name, name))
      break;

  return (param);
}


/*
 * 'ppdFirstCustomParam()' - Return the first parameter for a custom option.
 *
 * @since CUPS 1.2/OS X 10.5@
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
 * @since CUPS 1.2/OS X 10.5@
 */

ppd_cparam_t *				/* O - Custom parameter or NULL */
ppdNextCustomParam(ppd_coption_t *opt)	/* I - Custom option */
{
  if (!opt)
    return (NULL);

  return ((ppd_cparam_t *)cupsArrayNext(opt->params));
}


/*
 * End of "$Id: custom.c 6649 2007-07-11 21:46:42Z mike $".
 */
