/*
 * "$Id: http-support.c,v 1.1 2002/09/04 02:36:58 mike Exp $"
 *
 *   HTTP support routines for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-2002 by Easy Software Products, all rights reserved.
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
 *   httpSeparate() - Separate a Universal Resource Identifier into its
 *                    components.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include "string.h"

#include "http.h"
#include "ipp.h"


/*
 * 'httpSeparate()' - Separate a Universal Resource Identifier into its
 *                    components.
 */

void
httpSeparate(const char *uri,		/* I - Universal Resource Identifier */
             char       *method,	/* O - Method [32] (http, https, etc.) */
	     char       *username,	/* O - Username [32] */
	     char       *host,		/* O - Hostname [32] */
	     int        *port,		/* O - Port number to use */
             char       *resource)	/* O - Resource/filename [1024] */
{
  char		*ptr;			/* Pointer into string... */
  const char	*atsign,		/* @ sign */
		*slash;			/* Separator */
  char		safeuri[HTTP_MAX_URI];	/* "Safe" local copy of URI */
  char		quoted;			/* Quoted character */


 /*
  * Range check input...
  */

  if (uri == NULL || method == NULL || username == NULL || host == NULL ||
      port == NULL || resource == NULL)
    return;

 /*
  * Copy the URL to a local string to make sure we don't have a URL
  * longer than HTTP_MAX_URI characters long...
  */

  strlcpy(safeuri, uri, sizeof(safeuri));

  uri = safeuri;

 /*
  * Grab the method portion of the URI...
  */

  if (strncmp(uri, "//", 2) == 0)
  {
   /*
    * Workaround for HP IPP client bug...
    */

    strcpy(method, "ipp");
  }
  else
  {
   /*
    * Standard URI with method...
    */

    for (ptr = host; *uri != ':' && *uri != '\0'; uri ++)
      if (ptr < (host + HTTP_MAX_URI - 1))
        *ptr++ = *uri;

    *ptr = '\0';
    if (*uri == ':')
      uri ++;

   /*
    * If the method contains a period or slash, then it's probably
    * hostname/filename...
    */

    if (strchr(host, '.') != NULL || strchr(host, '/') != NULL || *uri == '\0')
    {
      if ((ptr = strchr(host, '/')) != NULL)
      {
	strlcpy(resource, ptr, HTTP_MAX_URI);
	*ptr = '\0';
      }
      else
	resource[0] = '\0';

      if (isdigit(*uri))
      {
       /*
	* OK, we have "hostname:port[/resource]"...
	*/

	*port = strtol(uri, (char **)&uri, 10);

	if (*uri == '/')
          strlcpy(resource, uri, HTTP_MAX_URI);
      }
      else
	*port = 631;

      strcpy(method, "http");
      username[0] = '\0';
      return;
    }
    else
      strlcpy(method, host, 32);
  }

 /*
  * If the method starts with less than 2 slashes then it is a local resource...
  */

  if (strncmp(uri, "//", 2) != 0)
  {
    strlcpy(resource, uri, HTTP_MAX_URI);

    username[0] = '\0';
    host[0]     = '\0';
    *port       = 0;
    return;
  }

 /*
  * Grab the username, if any...
  */

  while (*uri == '/')
    uri ++;

  if ((slash = strchr(uri, '/')) == NULL)
    slash = uri + strlen(uri);

  if ((atsign = strchr(uri, '@')) != NULL && atsign < slash)
  {
   /*
    * Got a username:password combo...
    */

    for (ptr = username; uri < atsign; uri ++)
      if (ptr < (username + HTTP_MAX_URI - 1))
      {
        if (*uri == '%' && isxdigit(uri[1]) && isxdigit(uri[2]))
	{
	 /*
	  * Grab a hex-encoded username and password...
	  */

          uri ++;
	  if (isalpha(*uri))
	    quoted = (tolower(*uri) - 'a' + 10) << 4;
	  else
	    quoted = (*uri - '0') << 4;

          uri ++;
	  if (isalpha(*uri))
	    quoted |= tolower(*uri) - 'a' + 10;
	  else
	    quoted |= *uri - '0';

          *ptr++ = quoted;
	}
	else
	  *ptr++ = *uri;
      }

    *ptr = '\0';

    uri = atsign + 1;
  }
  else
    username[0] = '\0';

 /*
  * Grab the hostname...
  */

  for (ptr = host; *uri != ':' && *uri != '/' && *uri != '\0'; uri ++)
    if (ptr < (host + HTTP_MAX_URI - 1))
      *ptr++ = *uri;

  *ptr = '\0';

  if (*uri != ':')
  {
    if (strcasecmp(method, "http") == 0)
      *port = 80;
    else if (strcasecmp(method, "https") == 0)
      *port = 443;
    else if (strcasecmp(method, "ipp") == 0)
      *port = ippPort();
    else if (strcasecmp(method, "socket") == 0)	/* Not registered yet... */
      *port = 9100;
    else
      *port = 0;
  }
  else
  {
   /*
    * Parse port number...
    */

    *port = 0;
    uri ++;
    while (isdigit(*uri))
    {
      *port = (*port * 10) + *uri - '0';
      uri ++;
    }
  }

  if (*uri == '\0')
  {
   /*
    * Hostname but no port or path...
    */

    resource[0] = '/';
    resource[1] = '\0';
    return;
  }

 /*
  * The remaining portion is the resource string...
  */

  strlcpy(resource, uri, HTTP_MAX_URI);
}


/*
 * End of "$Id: http-support.c,v 1.1 2002/09/04 02:36:58 mike Exp $".
 */
