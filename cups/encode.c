/*
 * "$Id: encode.c,v 1.1.2.3 2002/02/14 16:18:03 mike Exp $"
 *
 *   Option encoding routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
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
 * 'cupsEncodeOptions()' - Encode printer options into IPP attributes.
 */

void
cupsEncodeOptions(ipp_t         *ipp,		/* I - Request to add to */
        	  int           num_options,	/* I - Number of options */
		  cups_option_t *options)	/* I - Options */
{
  int		i, j, k;			/* Looping vars */
  int		count;				/* Number of values */
  int		n;				/* Attribute value */
  int		numbers;			/* 1 if all number values */
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

    for (count = 1, sep = options[i].value, numbers = 1; *sep; sep ++)
    {
      if (*sep == '\'')
      {
       /*
        * Skip quoted option value...
	*/

        sep ++;
	numbers = 0;

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
	numbers = 0;

        while (*sep && *sep != '\"')
	  sep ++;

	if (!*sep)
	  sep --;
      }
      else if (*sep == ',')
        count ++;
      else if (!isdigit(*sep) && *sep != '-')
      {
       /*
        * Isn't a standard numeric value, check for "NxMdpi" values...
	*/
        if (*sep != 'x' ||
	    (strstr(sep, "dpc") == NULL && strstr(sep, "dpi") == NULL))
	  numbers = 0;
      }
    }

    DEBUG_printf(("cupsEncodeOptions: option = \'%s\', count = %d\n",
                  options[i].name, count));

    if ((attr = _ipp_add_attr(ipp, count)) == NULL)
    {
     /*
      * Ran out of memory!
      */

      DEBUG_puts("cupsEncodeOptions: Ran out of memory for attributes!");
      return;
    }

    attr->group_tag = IPP_TAG_JOB;

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
    * See what the option value is; for compatibility with older interface
    * scripts, we have to support single-argument options as well as
    * option=value, option=low-high, option=MxN, and option=val1,val2,...,valN.
    */

    if (*val == '\0')
    {
     /*
      * Old-style System V boolean value...
      */

      attr->value_tag = IPP_TAG_BOOLEAN;

      if (strncasecmp(attr->name, "no", 2) == 0)
      {
        DEBUG_puts("cupsEncodeOptions: Added boolean false value...");
        strcpy(attr->name, attr->name + 2);
	attr->values[0].boolean = 0;
      }
      else
      {
        DEBUG_puts("cupsEncodeOptions: Added boolean true value...");
	attr->values[0].boolean = 1;
      }
    }
    else
    {
     /*
      * Scan the value string for values...
      */

      for (j = 0; *val != '\0'; val = sep, j ++)
      {
       /*
        * Find the end of this value and mark it if needed...
	*/

        if ((sep = strchr(val, ',')) != NULL)
	  *sep++ = '\0';
	else
	  sep = val + strlen(val);

       /*
        * See what kind of value it is...
	*/

	if (strcasecmp(val, "true") == 0 ||
            strcasecmp(val, "on") == 0 ||
	    strcasecmp(val, "yes") == 0)
	{
	 /*
	  * Boolean value - true...
	  */

          attr->value_tag         = IPP_TAG_BOOLEAN;
	  attr->values[j].boolean = 1;

          DEBUG_puts("cupsEncodeOptions: Added boolean true value...");
	}
	else if (strcasecmp(val, "false") == 0 ||
        	 strcasecmp(val, "off") == 0 ||
		 strcasecmp(val, "no") == 0)
	{
	 /*
	  * Boolean value - false...
	  */

          attr->value_tag         = IPP_TAG_BOOLEAN;
	  attr->values[j].boolean = 0;

          DEBUG_puts("cupsEncodeOptions: Added boolean false value...");
	}
	else
	{
	 /*
	  * Number, range, resolution, or string...
	  */

	  n = strtol(val, &s, 0);

	  if (!numbers)
	  {
	   /*
	    * String value(s)...
	    */

            if ((attr->values[j].string.text = strdup(val)) == NULL)
	    {
	     /*
	      * Ran out of memory!
	      */

	      DEBUG_puts("cupsEncodeOptions: Ran out of memory for string!");
	      return;
	    }

            attr->value_tag = IPP_TAG_NAME;

	    DEBUG_printf(("cupsEncodeOptions: Added string value \'%s\'...\n", val));
	  }
	  else if (*s == '-')
	  {
	    if (j > 0 && attr->value_tag == IPP_TAG_INTEGER)
	    {
	     /*
	      * Reset previous integer values to N-N ranges...
	      */

              for (k = 0; k < j; k ++)
	        attr->values[k].range.upper = attr->values[k].range.lower;
	    }

            attr->value_tag             = IPP_TAG_RANGE;
	    attr->values[j].range.lower = n;
	    attr->values[j].range.upper = strtol(s + 1, NULL, 0);

	    DEBUG_printf(("cupsEncodeOptions: Added range option value %d-%d...\n",
                	  n, attr->values[j].range.upper));
	  }
	  else if (*s == 'x')
	  {
            attr->value_tag                 = IPP_TAG_RESOLUTION;
	    attr->values[j].resolution.xres = n;
	    attr->values[j].resolution.yres = strtol(s + 1, &s, 0);

	    if (strcasecmp(s, "dpc") == 0)
              attr->values[j].resolution.units = IPP_RES_PER_CM;
            else if (strcasecmp(s, "dpi") == 0)
              attr->values[j].resolution.units = IPP_RES_PER_INCH;
            else
	    {
              if ((attr->values[j].string.text = strdup(val)) == NULL)
	      {
	       /*
		* Ran out of memory!
		*/

		DEBUG_puts("cupsEncodeOptions: Ran out of memory for string!");
		return;
	      }

              attr->value_tag = IPP_TAG_NAME;

	      DEBUG_printf(("cupsEncodeOptions: Added string value \'%s\'...\n", val));
	      continue;
            }

	    DEBUG_printf(("cupsEncodeOptions: Adding resolution option value %s...\n",
                	  val));
	  }
	  else
	  {
	    if (j && attr->value_tag == IPP_TAG_RANGE)
	    {
	     /*
	      * Set this value as a range...
	      */

	      attr->values[j].range.lower = n;
	      attr->values[j].range.upper = n;
	    }
	    else
	    {
	     /*
	      * Set this value as an integer...
	      */

              attr->value_tag         = IPP_TAG_INTEGER;
	      attr->values[j].integer = n;
            }

	    DEBUG_printf(("cupsEncodeOptions: Adding integer option value %d...\n", n));
	  }
        }
      }
    }
  }
}


/*
 * End of "$Id: encode.c,v 1.1.2.3 2002/02/14 16:18:03 mike Exp $".
 */
