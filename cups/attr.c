/*
 * "$Id: attr.c,v 1.1.2.3 2003/01/31 20:09:51 mike Exp $"
 *
 *   PPD model-specific attribute routines for the Common UNIX Printing System
 *   (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products.
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
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
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
 * Private function...
 */

extern int	_ppd_attr_compare(ppd_attr_t **a, ppd_attr_t **b);


/*
 * 'ppdFindAttr()' - Find the first matching attribute...
 */

const char *			/* O - Value or NULL if not found */
ppdFindAttr(ppd_file_t *ppd,	/* I - PPD file data */
            const char *name,	/* I - Attribute name */
            const char *spec)	/* I - Specifier string or NULL */
{
  ppd_attr_t	key,		/* Search key */
		*keyptr,	/* Pointer to key */
		**match;	/* Matching attribute */


 /*
  * Range check input...
  */

  if (ppd == NULL || name == NULL || ppd->num_attrs == 0)
    return (NULL);

 /*
  * Do a binary search for a matching attribute...
  */

  memset(&key, 0, sizeof(key));
  strncpy(key.name, name, sizeof(key.name) - 1);
  if (spec)
    strncpy(key.spec, spec, sizeof(key.spec) - 1);

  keyptr = &key;

  match = bsearch(&keyptr, ppd->attrs, ppd->num_attrs, sizeof(ppd_attr_t *),
                  (int (*)(const void *, const void *))_ppd_attr_compare);

  if (match == NULL)
  {
   /* 
    * No match!
    */

    ppd->cur_attr = -1;
    return (NULL);
  }

  if (match > ppd->attrs && spec == NULL)
  {
   /*
    * Find the first attribute with the same name...
    */

    while (match > ppd->attrs)
    {
      if (strcmp(match[-1]->name, name) != 0)
        break;

      match --;
    }
  }

 /*
  * Save the current attribute and return its value...
  */

  ppd->cur_attr = match - ppd->attrs;

  if ((*match)->value)
    return ((*match)->value);
  else
    return ("");
}


/*
 * 'ppdFindNextAttr()' - Find the next matching attribute...
 */

const char *				/* O - Value or NULL if not found */
ppdFindNextAttr(ppd_file_t *ppd,	/* I - PPD file data */
                const char *name,	/* I - Attribute name */
		const char *spec)	/* I - Specifier string or NULL */
{
  ppd_attr_t	**match;		/* Matching attribute */


 /*
  * Range check input...
  */

  if (ppd == NULL || name == NULL || ppd->num_attrs == 0 || ppd->cur_attr < 0)
    return (NULL);

 /*
  * See if there are more attributes to return...
  */

  ppd->cur_attr ++;

  if (ppd->cur_attr >= ppd->num_attrs)
  {
   /*
    * Nope...
    */

    ppd->cur_attr = -1;
    return (NULL);
  }

 /*
  * Check the next attribute to see if it is a match...
  */

  match = ppd->attrs + ppd->cur_attr;

  if (strcmp((*match)->name, name) != 0 ||
      (spec != NULL && strcmp((*match)->spec, spec) != 0))
  {
   /*
    * Nope...
    */

    ppd->cur_attr = -1;
    return (NULL);
  }
  
 /*
  * Return the next attribute's value...
  */

  if ((*match)->value)
    return ((*match)->value);
  else
    return ("");
}


/*
 * End of "$Id: attr.c,v 1.1.2.3 2003/01/31 20:09:51 mike Exp $".
 */
