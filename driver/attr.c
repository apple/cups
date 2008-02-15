/*
 * "$Id$"
 *
 *   PPD attribute lookup routine for CUPS.
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1993-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cupsFindAttr() - Find a PPD attribute based on the colormodel,
 *                    media, and resolution.
 */

/*
 * Include necessary headers.
 */

#include "driver.h"
#include <cups/string.h>


/*
 * 'cupsFindAttr()' - Find a PPD attribute based on the colormodel,
 *                    media, and resolution.
 */

ppd_attr_t *				/* O - Matching attribute or NULL */
cupsFindAttr(ppd_file_t *ppd,		/* I - PPD file */
             const char *name,		/* I - Attribute name */
             const char *colormodel,	/* I - Color model */
             const char *media,		/* I - Media type */
             const char *resolution,	/* I - Resolution */
	     char       *spec,		/* O - Final selection string */
	     int        specsize)	/* I - Size of string buffer */
{
  ppd_attr_t	*attr;			/* Attribute */


 /*
  * Range check input...
  */

  if (!ppd || !name || !colormodel || !media || !resolution || !spec ||
      specsize < PPD_MAX_NAME)
    return (NULL);

 /*
  * Look for the attribute with the following keywords:
  *
  *     ColorModel.MediaType.Resolution
  *     ColorModel.Resolution
  *     ColorModel
  *     MediaType.Resolution
  *     MediaType
  *     Resolution
  *     ""
  */

  snprintf(spec, specsize, "%s.%s.%s", colormodel, media, resolution);
  fprintf(stderr, "DEBUG2: Looking for \"*%s %s\"...\n", name, spec);
  if ((attr = ppdFindAttr(ppd, name, spec)) != NULL && attr->value != NULL)
    return (attr);

  snprintf(spec, specsize, "%s.%s", colormodel, resolution);
  fprintf(stderr, "DEBUG2: Looking for \"*%s %s\"...\n", name, spec);
  if ((attr = ppdFindAttr(ppd, name, spec)) != NULL && attr->value != NULL)
    return (attr);

  strlcpy(spec, colormodel, specsize);
  fprintf(stderr, "DEBUG2: Looking for \"*%s %s\"...\n", name, spec);
  if ((attr = ppdFindAttr(ppd, name, spec)) != NULL && attr->value != NULL)
    return (attr);

  snprintf(spec, specsize, "%s.%s", media, resolution);
  fprintf(stderr, "DEBUG2: Looking for \"*%s %s\"...\n", name, spec);
  if ((attr = ppdFindAttr(ppd, name, spec)) != NULL && attr->value != NULL)
    return (attr);

  strlcpy(spec, media, specsize);
  fprintf(stderr, "DEBUG2: Looking for \"*%s %s\"...\n", name, spec);
  if ((attr = ppdFindAttr(ppd, name, spec)) != NULL && attr->value != NULL)
    return (attr);

  strlcpy(spec, resolution, specsize);
  fprintf(stderr, "DEBUG2: Looking for \"*%s %s\"...\n", name, spec);
  if ((attr = ppdFindAttr(ppd, name, spec)) != NULL && attr->value != NULL)
    return (attr);

  spec[0] = '\0';
  fprintf(stderr, "DEBUG2: Looking for \"*%s\"...\n", name);
  if ((attr = ppdFindAttr(ppd, name, spec)) != NULL && attr->value != NULL)
    return (attr);

  fprintf(stderr, "DEBUG2: No instance of \"*%s\" found...\n", name);

  return (NULL);
}


/*
 * End of "$Id$".
 */
