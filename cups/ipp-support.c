/*
 * "$Id: ipp-support.c,v 1.2.2.5 2004/06/29 13:15:08 mike Exp $"
 *
 *   Internet Printing Protocol support functions for the Common UNIX
 *   Printing System (CUPS).
 *
 *   Copyright 1997-2004 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636-3142 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   ippErrorString() - Return a textual message for the given error
 *                      message.
 *   ippPort()        - Return the default IPP port number.
 *   ippSetPort()     - Set the default port number.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include "language.h"

#include "ipp.h"
#include "debug.h"
#include <ctype.h>


/*
 * Local globals...
 */

static int	ipp_port = 0;


/*
 * 'ippErrorString()' - Return a textual message for the given error message.
 */

const char *					/* O - Text string */
ippErrorString(ipp_status_t error)		/* I - Error status */
{
  static char	unknown[255];			/* Unknown error statuses */
  static const char * const status_oks[] =	/* "OK" status codes */
		{
		  "successful-ok",
		  "successful-ok-ignored-or-substituted-attributes",
		  "successful-ok-conflicting-attributes",
		  "successful-ok-ignored-subscriptions",
		  "successful-ok-ignored-notifications",
		  "successful-ok-too-many-events",
		  "successful-ok-but-cancel-subscription"
		},
		* const status_400s[] =		/* Client errors */
		{
		  "client-error-bad-request",
		  "client-error-forbidden",
		  "client-error-not-authenticated",
		  "client-error-not-authorized",
		  "client-error-not-possible",
		  "client-error-timeout",
		  "client-error-not-found",
		  "client-error-gone",
		  "client-error-request-entity-too-large",
		  "client-error-request-value-too-long",
		  "client-error-document-format-not-supported",
		  "client-error-attributes-or-values-not-supported",
		  "client-error-uri-scheme-not-supported",
		  "client-error-charset-not-supported",
		  "client-error-conflicting-attributes",
		  "client-error-compression-not-supported",
		  "client-error-compression-error",
		  "client-error-document-format-error",
		  "client-error-document-access-error",
		  "client-error-attributes-not-settable",
		  "client-error-ignored-all-subscriptions",
		  "client-error-too-many-subscriptions",
		  "client-error-ignored-all-notifications",
		  "client-error-print-support-file-not-found"
		},
		* const status_500s[] =		/* Server errors */
		{
		  "server-error-internal-error",
		  "server-error-operation-not-supported",
		  "server-error-service-unavailable",
		  "server-error-version-not-supported",
		  "server-error-device-error",
		  "server-error-temporary-error",
		  "server-error-not-accepting-jobs",
		  "server-error-busy",
		  "server-error-job-canceled",
		  "server-error-multiple-document-jobs-not-supported",
		  "server-error-printer-is-deactivated"
		};


 /*
  * See if the error code is a known value...
  */

  if (error >= IPP_OK && error <= IPP_OK_BUT_CANCEL_SUBSCRIPTION)
    return (status_oks[error]);
  else if (error == IPP_REDIRECTION_OTHER_SITE)
    return ("redirection-other-site");
  else if (error >= IPP_BAD_REQUEST && error <= IPP_PRINT_SUPPORT_FILE_NOT_FOUND)
    return (status_400s[error - IPP_BAD_REQUEST]);
  else if (error >= IPP_INTERNAL_ERROR && error <= IPP_PRINTER_IS_DEACTIVATED)
    return (status_500s[error - IPP_INTERNAL_ERROR]);

 /*
  * No, build an "unknown-xxxx" error string...
  */

  sprintf(unknown, "unknown-%04x", error);

  return (unknown);
}


/*
 * 'ippPort()' - Return the default IPP port number.
 */

int						/* O - Port number */
ippPort(void)
{
  const char	*server_port;			/* SERVER_PORT environment variable */
  struct servent *port;				/* Port number info */  


  if (ipp_port)
    return (ipp_port);
  else if ((server_port = getenv("IPP_PORT")) != NULL)
    return (ipp_port = atoi(server_port));
  else if ((port = getservbyname("ipp", NULL)) == NULL)
    return (ipp_port = CUPS_DEFAULT_IPP_PORT);
  else
    return (ipp_port = ntohs(port->s_port));
}


/*
 * 'ippSetPort()' - Set the default port number.
 */

void
ippSetPort(int p)				/* I - Port number to use */
{
  ipp_port = p;
}


/*
 * End of "$Id: ipp-support.c,v 1.2.2.5 2004/06/29 13:15:08 mike Exp $".
 */
