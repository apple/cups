/*
 * "$Id: ipp-var.c,v 1.1 2000/02/01 02:52:23 mike Exp $"
 *
 *   IPP variable routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products.
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
 *   ippSetCGIVars() - Set CGI variables from an IPP response.
 */

/*
 * Include necessary headers...
 */

#include "ipp-var.h"


/*
 * 'ippSetCGIVars()' - Set CGI variables from an IPP response.
 */

void
ippSetCGIVars(ipp_t *response)		/* I - Response data to be copied... */
{
  int			element;	/* Element in CGI array */
  ipp_attribute_t	*attr;		/* Attribute in response... */
  int			i;		/* Looping var */
  char			name[1024],	/* Name of attribute */
			value[16384],	/* Value(s) */
			*valptr;	/* Pointer into value */


  cgiSetVariable("SERVER_NAME", getenv("SERVER_NAME"));
  cgiSetVariable("REMOTE_USER", getenv("REMOTE_USER"));
  cgiSetVariable("CUPS_VERSION", CUPS_SVERSION);

  for (element = 0, attr = response->attrs;
       attr != NULL;
       attr = attr->next, element ++)
  {
   /*
    * Copy attributes to a separator...
    */

    for (; attr != NULL && attr->group_tag != IPP_TAG_ZERO; attr = attr->next)
    {
     /*
      * Copy the attribute name, substituting "_" for "-"...
      */

      if (attr->name == NULL)
        continue;

      for (i = 0; attr->name[i]; i ++)
        if (attr->name[i] == '-')
	  name[i] = '_';
	else
          name[i] = attr->name[i];

      name[i] = '\0';

     /*
      * Copy values...
      */

      value[0] = '\0';
      valptr   = value;

      for (i = 0; i < attr->num_values; i ++)
      {
	if (i)
	  strcat(valptr, ",");

	valptr += strlen(valptr);

	switch (attr->value_tag)
	{
	  case IPP_TAG_INTEGER :
	  case IPP_TAG_ENUM :
	      sprintf(valptr, "%d", attr->values[i].integer);
	      break;

	  case IPP_TAG_BOOLEAN :
	      if (!attr->values[i].boolean)
		strcat(valptr, "no");

	  case IPP_TAG_NOVALUE :
	      strcat(valptr, attr->name);
	      break;

	  case IPP_TAG_RANGE :
	      sprintf(valptr, "%d-%d", attr->values[i].range.lower,
		      attr->values[i].range.upper);
	      break;

	  case IPP_TAG_RESOLUTION :
	      sprintf(valptr, "%dx%d%s", attr->values[i].resolution.xres,
		      attr->values[i].resolution.yres,
		      attr->values[i].resolution.units == IPP_RES_PER_INCH ?
			  "dpi" : "dpc");
	      break;

          case IPP_TAG_STRING :
	  case IPP_TAG_TEXT :
	  case IPP_TAG_NAME :
	  case IPP_TAG_KEYWORD :
	  case IPP_TAG_CHARSET :
	  case IPP_TAG_LANGUAGE :
	      strcat(valptr, attr->values[i].string.text);
	      break;
	}
      }

     /*
      * Add the element...
      */

      cgiSetArray(name, element, value);
    }

    if (attr == NULL)
      break;
  }
}


/*
 * End of "$Id: ipp-var.c,v 1.1 2000/02/01 02:52:23 mike Exp $".
 */
