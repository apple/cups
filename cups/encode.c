/*
 * "$Id: encode.c,v 1.1.2.13 2003/02/13 03:23:55 mike Exp $"
 *
 *   Option encoding routines for the Common UNIX Printing System (CUPS).
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
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsEncodeOptions() - Encode printer options into IPP attributes.
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include <stdlib.h>
#include <ctype.h>
#include "string.h"
#include "debug.h"


/*
 * Local list of option names and the value tags they should use...
 */

typedef struct
{
  const char	*name;
  ipp_tag_t	value_tag;
} ipp_option_t;

static const ipp_option_t ipp_options[] =
			{
			  { "blackplot",		IPP_TAG_BOOLEAN },
			  { "brightness",		IPP_TAG_INTEGER },
			  { "columns",			IPP_TAG_INTEGER },
			  { "copies",			IPP_TAG_INTEGER },
			  { "finishings",		IPP_TAG_ENUM },
			  { "fitplot",			IPP_TAG_BOOLEAN },
			  { "gamma",			IPP_TAG_INTEGER },
			  { "hue",			IPP_TAG_INTEGER },
			  { "job-k-limit",		IPP_TAG_INTEGER },
			  { "job-page-limit",		IPP_TAG_INTEGER },
			  { "job-priority",		IPP_TAG_INTEGER },
			  { "job-quota-period",		IPP_TAG_INTEGER },
			  { "landscape",		IPP_TAG_BOOLEAN },
			  { "media",			IPP_TAG_KEYWORD },
			  { "mirror",			IPP_TAG_BOOLEAN },
			  { "natural-scaling",		IPP_TAG_INTEGER },
			  { "number-up",		IPP_TAG_INTEGER },
			  { "orientation-requested",	IPP_TAG_ENUM },
			  { "page-bottom",		IPP_TAG_INTEGER },
			  { "page-left",		IPP_TAG_INTEGER },
			  { "page-ranges",		IPP_TAG_RANGE },
			  { "page-right",		IPP_TAG_INTEGER },
			  { "page-top",			IPP_TAG_INTEGER },
			  { "penwidth",			IPP_TAG_INTEGER },
			  { "ppi",			IPP_TAG_INTEGER },
			  { "prettyprint",		IPP_TAG_BOOLEAN },
			  { "printer-resolution",	IPP_TAG_RESOLUTION },
			  { "print-quality",		IPP_TAG_ENUM },
			  { "saturation",		IPP_TAG_INTEGER },
			  { "scaling",			IPP_TAG_INTEGER },
			  { "sides",			IPP_TAG_KEYWORD },
			  { "wrap",			IPP_TAG_BOOLEAN }
			};


/*
 * 'cupsEncodeOptions()' - Encode printer options into IPP attributes.
 */

void
cupsEncodeOptions(ipp_t         *ipp,		/* I - Request to add to */
        	  int           num_options,	/* I - Number of options */
		  cups_option_t *options)	/* I - Options */
{
  int		i, j;				/* Looping vars */
  int		count;				/* Number of values */
  char		*s,				/* Pointer into option value */
		*val,				/* Pointer to option value */
		*copy,				/* Copy of option value */
		*sep;				/* Option separator */
  ipp_attribute_t *attr;			/* IPP job-id attribute */


  DEBUG_printf(("cupsEncodeOptions(%p, %d, %p)\n", ipp, num_options, options));

  if (ipp == NULL || num_options < 1 || options == NULL)
    return;

 /*
  * Handle the document format stuff first...
  */

  if ((val = (char *)cupsGetOption("document-format", num_options, options)) != NULL)
    ippAddString(ipp, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format",
        	 NULL, val);
  else if (cupsGetOption("raw", num_options, options))
    ippAddString(ipp, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format",
        	 NULL, "application/vnd.cups-raw");
  else
    ippAddString(ipp, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format",
        	 NULL, "application/octet-stream");

 /*
  * Then add all other options...
  */

  for (i = 0; i < num_options; i ++)
  {
   /*
    * Skip document format options - handled above...
    */

    if (strcasecmp(options[i].name, "raw") == 0 ||
        strcasecmp(options[i].name, "document-format") == 0 ||
	!options[i].name[0])
      continue;

   /*
    * Count the number of values...
    */

    for (count = 1, sep = options[i].value; *sep; sep ++)
    {
      if (*sep == '\'')
      {
       /*
        * Skip quoted option value...
	*/

        sep ++;

        while (*sep && *sep != '\'')
	  sep ++;

	if (!*sep)
	  sep --;
      }
      else if (*sep == '\"')
      {
       /*
        * Skip quoted option value...
	*/

        sep ++;

        while (*sep && *sep != '\"')
	  sep ++;

	if (!*sep)
	  sep --;
      }
      else if (*sep == ',')
        count ++;
      else if (*sep == '\\' && sep[1])
        sep ++;
    }

    DEBUG_printf(("cupsEncodeOptions: option = \'%s\', count = %d\n",
                  options[i].name, count));

   /*
    * Allocate memory for the attribute values...
    */

    if ((attr = _ipp_add_attr(ipp, count)) == NULL)
    {
     /*
      * Ran out of memory!
      */

      DEBUG_puts("cupsEncodeOptions: Ran out of memory for attributes!");
      return;
    }

   /*
    * Now figure out what type of value we have...
    */

    attr->group_tag = IPP_TAG_JOB;

    if (strcasecmp(options[i].value, "true") == 0 ||
        strcasecmp(options[i].value, "false") == 0)
      attr->value_tag = IPP_TAG_BOOLEAN;
    else
      attr->value_tag = IPP_TAG_NAME;

    for (j = 0; j < (int)(sizeof(ipp_options) / sizeof(ipp_options[0])); j ++)
      if (strcasecmp(options[i].name, ipp_options[j].name) == 0)
      {
        attr->value_tag = ipp_options[j].value_tag;
	break;
      }

   /*
    * Copy the name over...
    */

    if ((attr->name = strdup(options[i].name)) == NULL)
    {
     /*
      * Ran out of memory!
      */

      DEBUG_puts("cupsEncodeOptions: Ran out of memory for name!");
      return;
    }

    if (count > 1)
    {
     /*
      * Make a copy of the value we can fiddle with...
      */

      if ((copy = strdup(options[i].value)) == NULL)
      {
       /*
	* Ran out of memory!
	*/

	DEBUG_puts("cupsEncodeOptions: Ran out of memory for value copy!");
	return;
      }

      val = copy;
    }
    else
    {
     /*
      * Since we have a single value, use the value directly...
      */

      val  = options[i].value;
      copy = NULL;
    }

   /*
    * Scan the value string for values...
    */

    for (j = 0; j < count; val = sep, j ++)
    {
     /*
      * Find the end of this value and mark it if needed...
      */

      if ((sep = strchr(val, ',')) != NULL)
	*sep++ = '\0';
      else
	sep = val + strlen(val);

     /*
      * Copy the option value(s) over as needed by the type...
      */

      switch (attr->value_tag)
      {
	case IPP_TAG_INTEGER :
	case IPP_TAG_ENUM :
	   /*
	    * Integer/enumeration value...
	    */

            attr->values[j].integer = strtol(val, &s, 0);

            DEBUG_printf(("cupsEncodeOptions: Adding integer option value %d...\n",
	                  attr->values[j].integer));
            break;

	case IPP_TAG_BOOLEAN :
	    if (strcasecmp(val, "true") == 0)
	    {
	     /*
	      * Boolean value - true...
	      */

	      attr->values[j].boolean = 1;

              DEBUG_puts("cupsEncodeOptions: Added boolean true value...");
	    }
	    else
	    {
	     /*
	      * Boolean value - false...
	      */

	      attr->values[j].boolean = 0;

              DEBUG_puts("cupsEncodeOptions: Added boolean false value...");
	    }
            break;

	case IPP_TAG_RANGE :
	   /*
	    * Range...
	    */

            if (*val == '-')
	    {
	      attr->values[j].range.lower = 1;
	      s = val;
	    }
	    else
	      attr->values[j].range.lower = strtol(val, &s, 0);

	    if (*s == '-')
	    {
	      if (s[1])
		attr->values[j].range.upper = strtol(s + 1, NULL, 0);
	      else
		attr->values[j].range.upper = 2147483647;
            }
	    else
	      attr->values[j].range.upper = attr->values[j].range.lower;

	    DEBUG_printf(("cupsEncodeOptions: Added range option value %d-%d...\n",
                	  attr->values[j].range.lower,
			  attr->values[j].range.upper));
            break;

	case IPP_TAG_RESOLUTION :
	   /*
	    * Resolution...
	    */

	    attr->values[j].resolution.xres = strtol(val, &s, 0);

	    if (*s == 'x')
	      attr->values[j].resolution.yres = strtol(s + 1, &s, 0);
	    else
	      attr->values[j].resolution.yres = attr->values[j].resolution.xres;

	    if (strcasecmp(s, "dpc") == 0)
              attr->values[j].resolution.units = IPP_RES_PER_CM;
            else
              attr->values[j].resolution.units = IPP_RES_PER_INCH;

	    DEBUG_printf(("cupsEncodeOptions: Adding resolution option value %s...\n",
                	  val));
            break;

	default :
            if ((attr->values[j].string.text = strdup(val)) == NULL)
	    {
	     /*
	      * Ran out of memory!
	      */

	      DEBUG_puts("cupsEncodeOptions: Ran out of memory for string!");
	      return;
	    }

	    DEBUG_printf(("cupsEncodeOptions: Added string value \'%s\'...\n",
	                  val));
            break;
      }
    }
  }
}


/*
 * End of "$Id: encode.c,v 1.1.2.13 2003/02/13 03:23:55 mike Exp $".
 */
