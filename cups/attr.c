/*
 * "$Id$"
 *
 *   PPD model-specific attribute routines for the Common UNIX Printing System
 *   (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   ppdFindAttr()     - Find the first matching attribute...
 *   ppdFindNextAttr() - Find the next matching attribute...
 */

/*
 * Include necessary headers...
 */

#include "ppd.h"
#include "debug.h"
#include "string.h"
#include <stdlib.h>


/*
 * 'ppdFindAttr()' - Find the first matching attribute...
 *
 * @since CUPS 1.1.19@
 */

ppd_attr_t *				/* O - Attribute or NULL if not found */
ppdFindAttr(ppd_file_t *ppd,		/* I - PPD file data */
            const char *name,		/* I - Attribute name */
            const char *spec)		/* I - Specifier string or NULL */
{
  ppd_attr_t	key,			/* Search key */
		*attr;			/* Current attribute */
  int		diff;			/* Current difference */


  DEBUG_printf(("ppdFindAttr(ppd=%p, name=\"%s\", spec=\"%s\")\n", ppd,
                name ? name : "(null)", spec ? spec : "(null)"));

 /*
  * Range check input...
  */

  if (!ppd || !name || ppd->num_attrs == 0)
    return (NULL);

 /*
  * Search for a matching attribute...
  */

  memset(&key, 0, sizeof(key));
  strlcpy(key.name, name, sizeof(key.name));
  if (spec)
    strlcpy(key.spec, spec, sizeof(key.spec));

 /*
  * Return the first matching attribute, if any...
  */

  if ((attr = (ppd_attr_t *)cupsArrayFind(ppd->sorted_attrs, &key)) != NULL)
    return (attr);
  else if (spec)
    return (NULL);

 /*
  * No match found, loop through the sorted attributes to see if we can
  * find a "wildcard" match for the attribute...
  */

  for (attr = (ppd_attr_t *)cupsArrayFirst(ppd->sorted_attrs);
       attr;
       attr = (ppd_attr_t *)cupsArrayNext(ppd->sorted_attrs))
  {
    if ((diff = strcasecmp(attr->name, name)) == 0)
      break;

    if (diff > 0)
    {
     /*
      * All remaining attributes are > than the one we are trying to find...
      */

      cupsArrayIndex(ppd->sorted_attrs, cupsArrayCount(ppd->sorted_attrs));

      return (NULL);
    }
  }

  return (attr);
}


/*
 * 'ppdFindNextAttr()' - Find the next matching attribute...
 *
 * @since CUPS 1.1.19@
 */

ppd_attr_t *				/* O - Attribute or NULL if not found */
ppdFindNextAttr(ppd_file_t *ppd,	/* I - PPD file data */
                const char *name,	/* I - Attribute name */
		const char *spec)	/* I - Specifier string or NULL */
{
  ppd_attr_t	*attr;			/* Current attribute */


 /*
  * Range check input...
  */

  if (!ppd || !name || ppd->num_attrs == 0 ||
      !cupsArrayCurrent(ppd->sorted_attrs))
    return (NULL);

 /*
  * See if there are more attributes to return...
  */

  if ((attr = (ppd_attr_t *)cupsArrayNext(ppd->sorted_attrs)) == NULL)
    return (NULL);

 /*
  * Check the next attribute to see if it is a match...
  */

  if (strcasecmp(attr->name, name) || (spec && strcasecmp(attr->spec, spec)))
  {
   /*
    * Nope, reset the current pointer to the end of the array...
    */

    cupsArrayIndex(ppd->sorted_attrs, cupsArrayCount(ppd->sorted_attrs));

    return (NULL);
  }
  
 /*
  * Return the next attribute's value...
  */

  return (attr);
}


/*
 * End of "$Id$".
 */
