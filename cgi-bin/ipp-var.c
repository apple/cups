/*
 * "$Id: ipp-var.c,v 1.22 2001/02/20 17:32:59 mike Exp $"
 *
 *   IPP variable routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2001 by Easy Software Products.
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
 *   ippGetTemplateDir()   - Get the templates directory...
 *   ippSetServerVersion() - Set the server name and CUPS version...
 *   ippSetCGIVars()       - Set CGI variables from an IPP response.
 */

/*
 * Include necessary headers...
 */

#include "ipp-var.h"


/*
 * 'ippGetTemplateDir()' - Get the templates directory...
 */

char *					/* O - Template directory */
ippGetTemplateDir(void)
{
  const char	*datadir;		/* CUPS_DATADIR env var */
  static char	templates[1024] = "";	/* Template directory */


  if (!templates[0])
  {
   /*
    * Build the template directory pathname...
    */

    if ((datadir = getenv("CUPS_DATADIR")) == NULL)
      datadir = CUPS_DATADIR;

    snprintf(templates, sizeof(templates), "%s/templates", datadir);
  }

  return (templates);
}


/*
 * 'ippSetServerVersion()' - Set the server name and CUPS version...
 */

void
ippSetServerVersion(void)
{
  cgiSetVariable("SERVER_NAME", getenv("SERVER_NAME"));
  cgiSetVariable("REMOTE_USER", getenv("REMOTE_USER"));
  cgiSetVariable("CUPS_VERSION", CUPS_SVERSION);
}


/*
 * 'ippSetCGIVars()' - Set CGI variables from an IPP response.
 */

void
ippSetCGIVars(ipp_t      *response,	/* I - Response data to be copied... */
              const char *filter_name,	/* I - Filter name */
	      const char *filter_value)	/* I - Filter value */
{
  int			element;	/* Element in CGI array */
  ipp_attribute_t	*attr,		/* Attribute in response... */
			*filter;	/* Filtering attribute */
  int			i;		/* Looping var */
  char			name[1024],	/* Name of attribute */
			value[16384],	/* Value(s) */
			*valptr;	/* Pointer into value */
  char			method[HTTP_MAX_URI],
			username[HTTP_MAX_URI],
			hostname[HTTP_MAX_URI],
			resource[HTTP_MAX_URI],
			uri[HTTP_MAX_URI];
  int			port;		/* URI data */
  const char		*server;	/* Name of server */
  struct tm		*date;		/* Date information */


  ippSetServerVersion();

  server = getenv("SERVER_NAME");

  for (attr = response->attrs;
       attr && attr->group_tag == IPP_TAG_OPERATION;
       attr = attr->next);

  for (element = 0; attr != NULL; attr = attr->next, element ++)
  {
   /*
    * Copy attributes to a separator...
    */

    if (filter_name)
    {
      for (filter = attr;
           filter != NULL && filter->group_tag != IPP_TAG_ZERO;
           filter = filter->next)
        if (filter->name && strcmp(filter->name, filter_name) == 0 &&
	    (filter->value_tag == IPP_TAG_STRING ||
	     (filter->value_tag >= IPP_TAG_TEXTLANG &&
	      filter->value_tag <= IPP_TAG_MIMETYPE)) &&
	    filter->values[0].string.text != NULL &&
	    strcasecmp(filter->values[0].string.text, filter_value) == 0)
	  break;

      if (!filter)
        return;

      if (filter->group_tag == IPP_TAG_ZERO)
      {
        attr = filter;
	element --;
	continue;
      }
    }

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
      * Add "job_printer_name" variable if we have a "job_printer_uri"
      * attribute...
      */

      if (strcmp(name, "job_printer_uri") == 0)
      {
        if ((valptr = strrchr(attr->values[0].string.text, '/')) == NULL)
	  valptr = "unknown";
	else
	  valptr ++;

        cgiSetArray("job_printer_name", element, valptr);
      }

     /*
      * Copy values...
      */

      value[0]                 = '\0';	/* Initially an empty string */
      value[sizeof(value) - 1] = '\0';	/* In case string gets full */
      valptr                   = value; /* Start at the beginning */

      for (i = 0; i < attr->num_values; i ++)
      {
	if (i)
	  strncat(valptr, ",", sizeof(value) - (valptr - value) - 1);

	valptr += strlen(valptr);

	switch (attr->value_tag)
	{
	  case IPP_TAG_INTEGER :
	  case IPP_TAG_ENUM :
	      if (strncmp(name, "time_at_", 8) == 0)
	      {
	        date = localtime((time_t *)&(attr->values[i].integer));
		strftime(valptr, sizeof(value) - (valptr - value),
		         CUPS_STRFTIME_FORMAT, date);
	      }
	      else
	        snprintf(valptr, sizeof(value) - (valptr - value),
		         "%d", attr->values[i].integer);
	      break;

	  case IPP_TAG_BOOLEAN :
	      snprintf(valptr, sizeof(value) - (valptr - value),
	               "%d", attr->values[i].boolean);
	      break;

	  case IPP_TAG_NOVALUE :
	      strncat(valptr, "novalue", sizeof(value) - (valptr - value) - 1);
	      break;

	  case IPP_TAG_RANGE :
	      snprintf(valptr, sizeof(value) - (valptr - value),
	               "%d-%d", attr->values[i].range.lower,
		       attr->values[i].range.upper);
	      break;

	  case IPP_TAG_RESOLUTION :
	      snprintf(valptr, sizeof(value) - (valptr - value),
	               "%dx%d%s", attr->values[i].resolution.xres,
		       attr->values[i].resolution.yres,
		       attr->values[i].resolution.units == IPP_RES_PER_INCH ?
			   "dpi" : "dpc");
	      break;

	  case IPP_TAG_URI :
	      if (strchr(attr->values[i].string.text, ':') != NULL)
	      {
		httpSeparate(attr->values[i].string.text, method, username,
		             hostname, &port, resource);

        	if (strcmp(method, "ipp") == 0 ||
	            strcmp(method, "http") == 0)
        	{
        	 /*
		  * Map localhost access to localhost...
		  */

        	  if (strcasecmp(hostname, server) == 0 &&
	              (strcmp(getenv("REMOTE_HOST"), "127.0.0.1") == 0 ||
		       strcmp(getenv("REMOTE_HOST"), "localhost") == 0 ||
		       strcmp(getenv("REMOTE_HOST"), server) == 0))
		    strcpy(hostname, "localhost");

        	 /*
		  * Rewrite URI with HTTP address...
		  */

		  if (username[0])
		    snprintf(uri, sizeof(uri), "http://%s@%s:%d%s", username,
		             hostname, port, resource);
        	  else
		    snprintf(uri, sizeof(uri), "http://%s:%d%s", hostname, port,
		             resource);

		  strncat(valptr, uri, sizeof(value) - (valptr - value) - 1);
        	  break;
        	}
              }

          case IPP_TAG_STRING :
	  case IPP_TAG_TEXT :
	  case IPP_TAG_NAME :
	  case IPP_TAG_KEYWORD :
	  case IPP_TAG_CHARSET :
	  case IPP_TAG_LANGUAGE :
	      strncat(valptr, attr->values[i].string.text,
	              sizeof(value) - (valptr - value) - 1);
	      break;

          default :
	      break; /* anti-compiler-warning-code */
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
 * End of "$Id: ipp-var.c,v 1.22 2001/02/20 17:32:59 mike Exp $".
 */
