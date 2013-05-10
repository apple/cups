/*
 * "$Id$"
 *
 *   PPD model-specific attribute routines for the Common UNIX Printing System
 *   (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
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
 *   ppdFindAttr()     - Find the first matching attribute.
 *   ppdFindNextAttr() - Find the next matching attribute.
 */

/*
 * Include necessary headers...
 */

#include "ppd-private.h"
#include "debug.h"
#include "string.h"
#include <stdlib.h>


/*
 * 'ppdFindAttr()' - Find the first matching attribute.
 *
 * @since CUPS 1.1.19@
 */

ppd_attr_t *				/* O - Attribute or @code NULL@ if not found */
ppdFindAttr(ppd_file_t *ppd,		/* I - PPD file data */
            const char *name,		/* I - Attribute name */
            const char *spec)		/* I - Specifier string or @code NULL@ */
{
  ppd_attr_t	key,			/* Search key */
		*attr;			/* Current attribute */


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

 /*
  * Return the first matching attribute, if any...
  */

  if ((attr = (ppd_attr_t *)cupsArrayFind(ppd->sorted_attrs, &key)) != NULL)
  {
    if (spec)
    {
     /*
      * Loop until we find the first matching attribute for "spec"...
      */

      while (attr && strcasecmp(spec, attr->spec))
      {
        if ((attr = (ppd_attr_t *)cupsArrayNext(ppd->sorted_attrs)) != NULL &&
	    strcasecmp(attr->name, name))
	  attr = NULL;
      }
    }
  }

  return (attr);
}


/*
 * 'ppdFindNextAttr()' - Find the next matching attribute.
 *
 * @since CUPS 1.1.19@
 */

ppd_attr_t *				/* O - Attribute or @code NULL@ if not found */
ppdFindNextAttr(ppd_file_t *ppd,	/* I - PPD file data */
                const char *name,	/* I - Attribute name */
		const char *spec)	/* I - Specifier string or @code NULL@ */
{
  ppd_attr_t	*attr;			/* Current attribute */


 /*
  * Range check input...
  */

  if (!ppd || !name || ppd->num_attrs == 0)
    return (NULL);

 /*
  * See if there are more attributes to return...
  */

  while ((attr = (ppd_attr_t *)cupsArrayNext(ppd->sorted_attrs)) != NULL)
  {
   /*
    * Check the next attribute to see if it is a match...
    */

    if (strcasecmp(attr->name, name))
    {
     /*
      * Nope, reset the current pointer to the end of the array...
      */

      cupsArrayIndex(ppd->sorted_attrs, cupsArrayCount(ppd->sorted_attrs));

      return (NULL);
    }

    if (!spec || !strcasecmp(attr->spec, spec))
      break;
  }

 /*
  * Return the next attribute's value...
  */

  return (attr);
}


/*
 * '_ppdGet1284Values()' - Get 1284 device ID keys and values.
 *
 * The returned dictionary is a CUPS option array that can be queried with
 * cupsGetOption and freed with cupsFreeOptions.
 */

int					/* O - Number of key/value pairs */
_ppdGet1284Values(
    const char *device_id,		/* I - IEEE-1284 device ID string */
    cups_option_t **values)		/* O - Array of key/value pairs */
{
  int		num_values;		/* Number of values */
  char		key[256],		/* Key string */
		value[256],		/* Value string */
		*ptr;			/* Pointer into key/value */


 /*
  * Range check input...
  */

  if (values)
    *values = NULL;

  if (!device_id || !values)
    return (0);

 /*
  * Parse the 1284 device ID value into keys and values.  The format is
  * repeating sequences of:
  *
  *   [whitespace]key:value[whitespace];
  */

  num_values = 0;
  while (*device_id)
  {
    while (isspace(*device_id & 255))
      device_id ++;

    if (!*device_id)
      break;

    for (ptr = key; *device_id && *device_id != ':'; device_id ++)
      if (ptr < (key + sizeof(key) - 1))
        *ptr++ = *device_id;

    if (!*device_id)
      break;

    while (ptr > key && isspace(ptr[-1] & 255))
      ptr --;

    *ptr = '\0';
    device_id ++;

    while (isspace(*device_id & 255))
      device_id ++;

    if (!*device_id)
      break;

    for (ptr = value; *device_id && *device_id != ';'; device_id ++)
      if (ptr < (value + sizeof(value) - 1))
        *ptr++ = *device_id;

    if (!*device_id)
      break;

    while (ptr > value && isspace(ptr[-1] & 255))
      ptr --;

    *ptr = '\0';
    device_id ++;

    num_values = cupsAddOption(key, value, num_values, values);
  }

  return (num_values);
}


/*
 * '_ppdNormalizeMakeAndModel()' - Normalize a product/make-and-model string.
 *
 * This function tries to undo the mistakes made by many printer manufacturers
 * to produce a clean make-and-model string we can use.
 */

char *					/* O - Normalized make-and-model string or NULL on error */
_ppdNormalizeMakeAndModel(
    const char *make_and_model,		/* I - Original make-and-model string */
    char       *buffer,			/* I - String buffer */
    size_t     bufsize)			/* I - Size of string buffer */
{
  char	*bufptr;			/* Pointer into buffer */


  if (!make_and_model || !buffer || bufsize < 1)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

 /*
  * Skip leading whitespace...
  */

  while (isspace(*make_and_model & 255))
    make_and_model ++;

 /*
  * Remove parenthesis and add manufacturers as needed...
  */

  if (make_and_model[0] == '(')
  {
    strlcpy(buffer, make_and_model + 1, bufsize);

    if ((bufptr = strrchr(buffer, ')')) != NULL)
      *bufptr = '\0';
  }
  else if (!strncasecmp(make_and_model, "XPrint", 6))
  {
   /*
    * Xerox XPrint...
    */

    snprintf(buffer, bufsize, "Xerox %s", make_and_model);
  }
  else if (!strncasecmp(make_and_model, "Eastman", 7))
  {
   /*
    * Kodak...
    */

    snprintf(buffer, bufsize, "Kodak %s", make_and_model + 7);
  }
  else if (!strncasecmp(make_and_model, "laserwriter", 11))
  {
   /*
    * Apple LaserWriter...
    */

    snprintf(buffer, bufsize, "Apple LaserWriter%s", make_and_model + 11);
  }
  else if (!strncasecmp(make_and_model, "colorpoint", 10))
  {
   /*
    * Seiko...
    */

    snprintf(buffer, bufsize, "Seiko %s", make_and_model);
  }
  else if (!strncasecmp(make_and_model, "fiery", 5))
  {
   /*
    * EFI...
    */

    snprintf(buffer, bufsize, "EFI %s", make_and_model);
  }
  else if (!strncasecmp(make_and_model, "ps ", 3) ||
	   !strncasecmp(make_and_model, "colorpass", 9))
  {
   /*
    * Canon...
    */

    snprintf(buffer, bufsize, "Canon %s", make_and_model);
  }
  else if (!strncasecmp(make_and_model, "primera", 7))
  {
   /*
    * Fargo...
    */

    snprintf(buffer, bufsize, "Fargo %s", make_and_model);
  }
  else if (!strncasecmp(make_and_model, "designjet", 9) ||
           !strncasecmp(make_and_model, "deskjet", 7))
  {
   /*
    * HP...
    */

    snprintf(buffer, bufsize, "HP %s", make_and_model);
  }
  else
    strlcpy(buffer, make_and_model, bufsize);

 /*
  * Clean up the make...
  */

  if (!strncasecmp(buffer, "agfa", 4))
  {
   /*
    * Replace with AGFA (all uppercase)...
    */

    buffer[0] = 'A';
    buffer[1] = 'G';
    buffer[2] = 'F';
    buffer[3] = 'A';
  }
  else if (!strncasecmp(buffer, "Hewlett-Packard hp ", 19))
  {
   /*
    * Just put "HP" on the front...
    */

    buffer[0] = 'H';
    buffer[1] = 'P';
    _cups_strcpy(buffer + 2, buffer + 18);
  }
  else if (!strncasecmp(buffer, "Hewlett-Packard ", 16))
  {
   /*
    * Just put "HP" on the front...
    */

    buffer[0] = 'H';
    buffer[1] = 'P';
    _cups_strcpy(buffer + 2, buffer + 15);
  }
  else if (!strncasecmp(buffer, "Lexmark International", 21))
  {
   /*
    * Strip "International"...
    */

    _cups_strcpy(buffer + 8, buffer + 21);
  }
  else if (!strncasecmp(buffer, "herk", 4))
  {
   /*
    * Replace with LHAG...
    */

    buffer[0] = 'L';
    buffer[1] = 'H';
    buffer[2] = 'A';
    buffer[3] = 'G';
  }
  else if (!strncasecmp(buffer, "linotype", 8))
  {
   /*
    * Replace with LHAG...
    */

    buffer[0] = 'L';
    buffer[1] = 'H';
    buffer[2] = 'A';
    buffer[3] = 'G';
    _cups_strcpy(buffer + 4, buffer + 8);
  }

 /*
  * Remove trailing whitespace and return...
  */

  for (bufptr = buffer + strlen(buffer) - 1;
       bufptr >= buffer && isspace(*bufptr & 255);
       bufptr --);

  bufptr[1] = '\0';

  return (buffer[0] ? buffer : NULL);
}


/*
 * End of "$Id$".
 */
