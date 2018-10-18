/*
 * PPD custom option routines for CUPS.
 *
 * Copyright 2007-2015 by Apple Inc.
 * Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 *
 * PostScript is a trademark of Adobe Systems, Inc.
 */

/*
 * Include necessary headers.
 */

#include "cups-private.h"
#include "ppd-private.h"
#include "debug-internal.h"


/*
 * 'ppdFindCustomOption()' - Find a custom option.
 *
 * @since CUPS 1.2/macOS 10.5@
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
 * @since CUPS 1.2/macOS 10.5@
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
 * @since CUPS 1.2/macOS 10.5@
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
 * @since CUPS 1.2/macOS 10.5@
 */

ppd_cparam_t *				/* O - Custom parameter or NULL */
ppdNextCustomParam(ppd_coption_t *opt)	/* I - Custom option */
{
  if (!opt)
    return (NULL);

  return ((ppd_cparam_t *)cupsArrayNext(opt->params));
}
