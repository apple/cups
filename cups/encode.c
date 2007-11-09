/*
 * "$Id: encode.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   Option encoding routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsEncodeOptions()   - Encode printer options into IPP attributes.
 *   cupsEncodeOptions2()  - Encode printer options into IPP attributes for
 *                           a group.
 *   _ippFindOption()      - Find the attribute information for an option.
 *   compare_ipp_options() - Compare two IPP options.
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
 *
 * **** THIS LIST MUST BE SORTED ****
 */

static const _ipp_option_t ipp_options[] =
{
  { "auth-info",		IPP_TAG_TEXT,		IPP_TAG_JOB },
  { "auth-info-required",	IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { "blackplot",		IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { "blackplot-default",	IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { "brightness",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "brightness-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { "columns",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "columns-default",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { "copies",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "copies-default",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { "document-format",		IPP_TAG_MIMETYPE,	IPP_TAG_OPERATION },
  { "document-format-default",	IPP_TAG_MIMETYPE,	IPP_TAG_PRINTER },
  { "finishings",		IPP_TAG_ENUM,		IPP_TAG_JOB },
  { "finishings-default",	IPP_TAG_ENUM,		IPP_TAG_PRINTER },
  { "fitplot",			IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { "fitplot-default",		IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { "gamma",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "gamma-default",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { "hue",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "hue-default",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { "job-k-limit",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "job-page-limit",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "job-priority",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "job-quota-period",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "job-uuid",			IPP_TAG_URI,		IPP_TAG_JOB },
  { "landscape",		IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { "media",			IPP_TAG_KEYWORD,	IPP_TAG_JOB },
  { "mirror",			IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { "mirror-default",		IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { "natural-scaling",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "natural-scaling-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { "notify-charset",		IPP_TAG_CHARSET,	IPP_TAG_SUBSCRIPTION },
  { "notify-events",		IPP_TAG_KEYWORD,	IPP_TAG_SUBSCRIPTION },
  { "notify-events-default",	IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { "notify-lease-duration",	IPP_TAG_INTEGER,	IPP_TAG_SUBSCRIPTION },
  { "notify-lease-duration-default", IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { "notify-natural-language",	IPP_TAG_LANGUAGE,	IPP_TAG_SUBSCRIPTION },
  { "notify-pull-method",	IPP_TAG_KEYWORD,	IPP_TAG_SUBSCRIPTION },
  { "notify-recipient-uri",	IPP_TAG_URI,		IPP_TAG_SUBSCRIPTION },
  { "notify-time-interval",	IPP_TAG_INTEGER,	IPP_TAG_SUBSCRIPTION },
  { "notify-user-data",		IPP_TAG_STRING,		IPP_TAG_SUBSCRIPTION },
  { "number-up",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "number-up-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { "orientation-requested",	IPP_TAG_ENUM,		IPP_TAG_JOB },
  { "orientation-requested-default", IPP_TAG_ENUM,	IPP_TAG_PRINTER },
  { "page-bottom",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "page-bottom-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { "page-left",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "page-left-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { "page-ranges",		IPP_TAG_RANGE,		IPP_TAG_JOB },
  { "page-ranges-default",	IPP_TAG_RANGE,		IPP_TAG_PRINTER },
  { "page-right",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "page-right-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { "page-top",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "page-top-default",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { "penwidth",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "penwidth-default",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { "port-monitor",             IPP_TAG_NAME,           IPP_TAG_PRINTER },
  { "ppi",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "ppi-default",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { "prettyprint",		IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { "prettyprint-default",	IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { "print-quality",		IPP_TAG_ENUM,		IPP_TAG_JOB },
  { "print-quality-default",	IPP_TAG_ENUM,		IPP_TAG_PRINTER },
  { "printer-error-policy",	IPP_TAG_NAME,		IPP_TAG_PRINTER },
  { "printer-info",		IPP_TAG_TEXT,		IPP_TAG_PRINTER },
  { "printer-is-accepting-jobs",IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { "printer-is-shared",	IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { "printer-location",		IPP_TAG_TEXT,		IPP_TAG_PRINTER },
  { "printer-make-and-model",	IPP_TAG_TEXT,		IPP_TAG_PRINTER },
  { "printer-more-info",	IPP_TAG_URI,		IPP_TAG_PRINTER },
  { "printer-op-policy",	IPP_TAG_NAME,		IPP_TAG_PRINTER },
  { "printer-resolution",	IPP_TAG_RESOLUTION,	IPP_TAG_JOB },
  { "printer-state",		IPP_TAG_ENUM,		IPP_TAG_PRINTER },
  { "printer-state-change-time",IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { "printer-state-reasons",	IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { "printer-type",		IPP_TAG_ENUM,		IPP_TAG_PRINTER },
  { "printer-uri",		IPP_TAG_URI,		IPP_TAG_OPERATION },
  { "queued-job-count",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { "raw",			IPP_TAG_MIMETYPE,	IPP_TAG_OPERATION },
  { "requesting-user-name-allowed",	IPP_TAG_NAME,	IPP_TAG_PRINTER },
  { "requesting-user-name-denied",	IPP_TAG_NAME,	IPP_TAG_PRINTER },
  { "resolution",		IPP_TAG_RESOLUTION,	IPP_TAG_JOB },
  { "resolution-default",	IPP_TAG_RESOLUTION,	IPP_TAG_PRINTER },
  { "saturation",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "saturation-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { "scaling",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { "scaling-default",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { "sides",			IPP_TAG_KEYWORD,	IPP_TAG_JOB },
  { "sides-default",		IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { "wrap",			IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { "wrap-default",		IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER }
};


/*
 * Local functions...
 */

static int	compare_ipp_options(_ipp_option_t *a, _ipp_option_t *b);


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
  cups_option_t	*option;		/* Current option */


  DEBUG_printf(("cupsEncodeOptions2(ipp=%p, num_options=%d, options=%p, "
                "group_tag=%x)\n", ipp, num_options, options, group_tag));

 /*
  * Range check input...
  */

  if (!ipp || num_options < 1 || !options)
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

  for (i = num_options, option = options; i > 0; i --, option ++)
  {
    _ipp_option_t	*match;		/* Matching attribute */


   /*
    * Skip document format options that are handled above...
    */

    if (!strcasecmp(option->name, "raw") ||
        !strcasecmp(option->name, "document-format") ||
	!option->name[0])
      continue;

   /*
    * Figure out the proper value and group tags for this option...
    */

    if ((match = _ippFindOption(option->name)) != NULL)
    {
      if (match->group_tag != group_tag)
        continue;

      value_tag = match->value_tag;
    }
    else
    {
      int	namelen;		/* Length of name */


      namelen = (int)strlen(option->name);

      if (namelen < 9 || strcmp(option->name + namelen - 8, "-default"))
      {
	if (group_tag != IPP_TAG_JOB)
          continue;
      }
      else if (group_tag != IPP_TAG_PRINTER)
        continue;

      if (!strcasecmp(option->value, "true") ||
          !strcasecmp(option->value, "false"))
	value_tag = IPP_TAG_BOOLEAN;
      else
	value_tag = IPP_TAG_NAME;
    }

   /*
    * Count the number of values...
    */

    for (count = 1, sep = option->value; *sep; sep ++)
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
                  option->name, count));

   /*
    * Allocate memory for the attribute values...
    */

    if ((attr = _ippAddAttr(ipp, count)) == NULL)
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

    if ((attr->name = _cupsStrAlloc(option->name)) == NULL)
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

      if ((copy = strdup(option->value)) == NULL)
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

      val  = option->value;
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

	    if (!strcasecmp(s, "dpc"))
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

            attr->values[j].unknown.length = (int)strlen(val);
	    attr->values[j].unknown.data   = _cupsStrAlloc(val);

            DEBUG_printf(("cupsEncodeOptions2: Added octet-string value \"%s\"...\n",
	                  attr->values[j].unknown.data));
            break;

	default :
            if ((attr->values[j].string.text = _cupsStrAlloc(val)) == NULL)
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

    if (copy)
      free(copy);
  }
}


/*
 * '_ippFindOption()' - Find the attribute information for an option.
 */

_ipp_option_t *				/* O - Attribute information */
_ippFindOption(const char *name)	/* I - Option/attribute name */
{
  _ipp_option_t	key;			/* Search key */


 /*
  * Lookup the proper value and group tags for this option...
  */

  key.name = name;

  return ((_ipp_option_t *)bsearch(&key, ipp_options,
                                   sizeof(ipp_options) / sizeof(ipp_options[0]),
				   sizeof(ipp_options[0]),
				   (int (*)(const void *, const void *))
				       compare_ipp_options));
}


/*
 * 'compare_ipp_options()' - Compare two IPP options.
 */

static int				/* O - Result of comparison */
compare_ipp_options(_ipp_option_t *a,	/* I - First option */
                    _ipp_option_t *b)	/* I - Second option */
{
  return (strcmp(a->name, b->name));
}


/*
 * End of "$Id: encode.c 6649 2007-07-11 21:46:42Z mike $".
 */
