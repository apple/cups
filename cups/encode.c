/*
 * "$Id$"
 *
 *   Option encoding routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products.
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsEncodeOptions()  - Encode printer options into IPP attributes.
 *   cupsEncodeOptions2() - Encode printer options into IPP attributes for
 *                          a group.
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include "ipp-private.h"
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
  ipp_tag_t	group_tag;
} _ipp_option_t;

static const _ipp_option_t ipp_options[] =
{
  { "blackplot",		IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { "brightness",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "columns",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "copies",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "document-format",		IPP_TAG_MIMETYPE,	IPP_TAG_OPERATION },
  { "finishings",		IPP_TAG_ENUM,		IPP_TAG_JOB },
  { "fitplot",			IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { "gamma",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "hue",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "job-k-limit",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "job-page-limit",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "job-priority",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "job-quota-period",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "landscape",		IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { "media",			IPP_TAG_KEYWORD,	IPP_TAG_JOB },
  { "mirror",			IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { "natural-scaling",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "notify-charset",		IPP_TAG_CHARSET,	IPP_TAG_SUBSCRIPTION },
  { "notify-events",		IPP_TAG_KEYWORD,	IPP_TAG_SUBSCRIPTION },
  { "notify-lease-time",	IPP_TAG_INTEGER,	IPP_TAG_SUBSCRIPTION },
  { "notify-natural-language",	IPP_TAG_LANGUAGE,	IPP_TAG_SUBSCRIPTION },
  { "notify-pull-method",	IPP_TAG_KEYWORD,	IPP_TAG_SUBSCRIPTION },
  { "notify-recipient",		IPP_TAG_URI,		IPP_TAG_SUBSCRIPTION },
  { "notify-time-interval",	IPP_TAG_INTEGER,	IPP_TAG_SUBSCRIPTION },
  { "notify-user-data",		IPP_TAG_STRING,		IPP_TAG_SUBSCRIPTION },
  { "number-up",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "orientation-requested",	IPP_TAG_ENUM,		IPP_TAG_JOB },
  { "page-bottom",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "page-left",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "page-ranges",		IPP_TAG_RANGE,		IPP_TAG_JOB },
  { "page-right",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "page-top",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "penwidth",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "ppi",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "prettyprint",		IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { "printer-resolution",	IPP_TAG_RESOLUTION,	IPP_TAG_JOB },
  { "printer-uri",		IPP_TAG_URI,		IPP_TAG_OPERATION },
  { "print-quality",		IPP_TAG_ENUM,		IPP_TAG_JOB },
  { "raw",			IPP_TAG_MIMETYPE,	IPP_TAG_OPERATION },
  { "saturation",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "scaling",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "sides",			IPP_TAG_KEYWORD,	IPP_TAG_JOB },
  { "wrap",			IPP_TAG_BOOLEAN,	IPP_TAG_JOB }
};


/*
 * 'cupsEncodeOptions()' - Encode printer options into IPP attributes.
 *
 * This function adds operation, job, and then subscription attributes,
 * in that order. Use the cupsEncodeOptions2() function to add attributes
 * for a single group.
 */

void
cupsEncodeOptions(ipp_t         *ipp,		/* I - Request to add to */
        	  int           num_options,	/* I - Number of options */
		  cups_option_t *options)	/* I - Options */
{
  DEBUG_printf(("cupsEncodeOptions(%p, %d, %p)\n", ipp, num_options, options));

 /*
  * Add the options in the proper groups & order...
  */

  cupsEncodeOptions2(ipp, num_options, options, IPP_TAG_OPERATION);
  cupsEncodeOptions2(ipp, num_options, options, IPP_TAG_JOB);
  cupsEncodeOptions2(ipp, num_options, options, IPP_TAG_SUBSCRIPTION);
}


/*
 * 'cupsEncodeOptions2()' - Encode printer options into IPP attributes for a group.
 *
 * This function only adds attributes for a single group. Call this
 * function multiple times for each group, or use cupsEncodeOptions()
 * to add the standard groups.
 *
 * @since CUPS 1.2@
 */

void
cupsEncodeOptions2(
    ipp_t         *ipp,			/* I - Request to add to */
    int           num_options,		/* I - Number of options */
    cups_option_t *options,		/* I - Options */
    ipp_tag_t     group_tag)		/* I - Group to encode */
{
  int		i, j;			/* Looping vars */
  int		count;			/* Number of values */
  char		*s,			/* Pointer into option value */
		*val,			/* Pointer to option value */
		*copy,			/* Copy of option value */
		*sep;			/* Option separator */
  ipp_attribute_t *attr;		/* IPP attribute */
  ipp_tag_t	value_tag;		/* IPP value tag */


  DEBUG_printf(("cupsEncodeOptions2(ipp=%p, num_options=%d, options=%p, group_tag=%x)\n",
                ipp, num_options, options, group_tag));

 /*
  * Range check input...
  */

  if (ipp == NULL || num_options < 1 || options == NULL)
    return;

 /*
  * Do special handling for the document-format/raw options...
  */

  if (group_tag == IPP_TAG_OPERATION)
  {
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
  }

 /*
  * Then loop through the options...
  */

  for (i = 0; i < num_options; i ++)
  {
   /*
    * Skip document format options that are handled above...
    */

    if (!strcasecmp(options[i].name, "raw") ||
        !strcasecmp(options[i].name, "document-format") ||
	!options[i].name[0])
      continue;

   /*
    * Figure out the proper value and group tags for this option...
    */

    for (j = 0; j < (int)(sizeof(ipp_options) / sizeof(ipp_options[0])); j ++)
      if (!strcasecmp(options[i].name, ipp_options[j].name))
        break;

    if (j < (int)(sizeof(ipp_options) / sizeof(ipp_options[0])))
    {
      if (ipp_options[j].group_tag != group_tag)
        continue;

      value_tag = ipp_options[j].value_tag;
    }
    else if (group_tag != IPP_TAG_JOB)
      continue;
    else if (!strcasecmp(options[i].value, "true") ||
             !strcasecmp(options[i].value, "false"))
      value_tag = IPP_TAG_BOOLEAN;
    else
      value_tag = IPP_TAG_NAME;

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

    DEBUG_printf(("cupsEncodeOptions2: option = \'%s\', count = %d\n",
                  options[i].name, count));

   /*
    * Allocate memory for the attribute values...
    */

    if ((attr = _ipp_add_attr(ipp, count)) == NULL)
    {
     /*
      * Ran out of memory!
      */

      DEBUG_puts("cupsEncodeOptions2: Ran out of memory for attributes!");
      return;
    }

   /*
    * Now figure out what type of value we have...
    */

    attr->group_tag = group_tag;
    attr->value_tag = value_tag;

   /*
    * Copy the name over...
    */

    if ((attr->name = strdup(options[i].name)) == NULL)
    {
     /*
      * Ran out of memory!
      */

      DEBUG_puts("cupsEncodeOptions2: Ran out of memory for name!");
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

	DEBUG_puts("cupsEncodeOptions2: Ran out of memory for value copy!");
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

            DEBUG_printf(("cupsEncodeOptions2: Added integer option value %d...\n",
	                  attr->values[j].integer));
            break;

	case IPP_TAG_BOOLEAN :
	    if (!strcasecmp(val, "true") ||
	        !strcasecmp(val, "on") ||
	        !strcasecmp(val, "yes"))
	    {
	     /*
	      * Boolean value - true...
	      */

	      attr->values[j].boolean = 1;

              DEBUG_puts("cupsEncodeOptions2: Added boolean true value...");
	    }
	    else
	    {
	     /*
	      * Boolean value - false...
	      */

	      attr->values[j].boolean = 0;

              DEBUG_puts("cupsEncodeOptions2: Added boolean false value...");
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

	    DEBUG_printf(("cupsEncodeOptions2: Added range option value %d-%d...\n",
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

	    DEBUG_printf(("cupsEncodeOptions2: Added resolution option value %s...\n",
                	  val));
            break;

	case IPP_TAG_STRING :
           /*
	    * octet-string
	    */

            attr->values[j].unknown.length = strlen(val);
	    attr->values[j].unknown.data   = strdup(val);

            DEBUG_printf(("cupsEncodeOptions2: Added octet-string value \"%s\"...\n",
	                  attr->values[j].unknown.data));
            break;

	default :
            if ((attr->values[j].string.text = strdup(val)) == NULL)
	    {
	     /*
	      * Ran out of memory!
	      */

	      DEBUG_puts("cupsEncodeOptions2: Ran out of memory for string!");
	      return;
	    }

	    DEBUG_printf(("cupsEncodeOptions2: Added string value \"%s\"...\n",
	                  val));
            break;
      }
    }
  }
}


/*
 * End of "$Id$".
 */
