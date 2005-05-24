/*
 * "$Id$"
 *
 *   Internet Printing Protocol support functions for the Common UNIX
 *   Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
 *   ippErrorString() - Return a name for the given status code.
 *   ippErrorValue()  - Return a status code for the given name.
 *   ippOpString()    - Return a name for the given operation id.
 *   ippOpValue()     - Return an operation id for the given name.
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
static const char * const ipp_status_oks[] =	/* "OK" status codes */
		{
		  "successful-ok",
		  "successful-ok-ignored-or-substituted-attributes",
		  "successful-ok-conflicting-attributes",
		  "successful-ok-ignored-subscriptions",
		  "successful-ok-ignored-notifications",
		  "successful-ok-too-many-events",
		  "successful-ok-but-cancel-subscription"
		},
		* const ipp_status_400s[] =	/* Client errors */
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
		* const ipp_status_500s[] =		/* Server errors */
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
static char	* const ipp_std_ops[] =
		{
		  /* 0x0000 - 0x000f */
		  "", "", "Print-Job", "Print-URI",
		  "Validate-Job", "Create-Job", "Send-Document",
		  "Send-URI", "Cancel-Job", "Get-Job-Attributes",
		  "Get-Jobs", "Get-Printer-Attributes",
		  "Hold-Job", "Release-Job", "Restart-Job", "",

		  /* 0x0010 - 0x001f */
		  "Pause-Printer", "Resume-Printer",
		  "Purge-Jobs", "Set-Printer-Attributes",
		  "Set-Job-Attributes",
		  "Get-Printer-Supported-Values",
		  "Create-Printer-Subscription",
		  "Create-Job-Subscription",
		  "Get-Subscription-Attributes",
		  "Get-Subscriptions", "Renew-Subscriptions",
		  "Cancel-Subscription", "Get-Notifications",
		  "Send-Notifications", "", "",

		  /* 0x0020 - 0x002f */
		  "",
		  "Get-Printer-Support-Files",
		  "Enable-Printer",
		  "Disable-Printer",
		  "Pause-Printer-After-Current-Job",
		  "Hold-New-Jobs",
		  "Release-Held-New-Jobs",
		  "Deactivate-Printer",
		  "Activate-Printer",
		  "Restart-Printer",
		  "Shutdown-Printer",
		  "Startup-Printer",
		  "Reprocess-Job",
		  "Cancel-Current-Job",
		  "Suspend-Current-Job",
		  "Resume-Job",

		  /* 0x0030 - 0x0031 */
		  "Promote-Job",
		  "Schedule-Job-After"
		},
		* const ipp_cups_ops[] =
		{
		  "CUPS-Get-Default",
		  "CUPS-Get-Printers",
		  "CUPS-Add-Printer",
		  "CUPS-Delete-Printer",
		  "CUPS-Get-Classes",
		  "CUPS-Add-Class",
		  "CUPS-Delete-Class",
		  "CUPS-Accept-Jobs",
		  "CUPS-Reject-Jobs",
		  "CUPS-Set-Default",
		  "CUPS-Get-Devices",
		  "CUPS-Get-PPDs",
		  "CUPS-Move-Job",
		  "CUPS-Add-Device",
		  "CUPS-Delete-Device"
		};


/*
 * 'ippErrorString()' - Return a name for the given status code.
 */

const char *					/* O - Text string */
ippErrorString(ipp_status_t error)		/* I - Error status */
{
  static char	unknown[255];			/* Unknown error statuses */


 /*
  * See if the error code is a known value...
  */

  if (error >= IPP_OK && error <= IPP_OK_BUT_CANCEL_SUBSCRIPTION)
    return (ipp_status_oks[error]);
  else if (error == IPP_REDIRECTION_OTHER_SITE)
    return ("redirection-other-site");
  else if (error >= IPP_BAD_REQUEST && error <= IPP_PRINT_SUPPORT_FILE_NOT_FOUND)
    return (ipp_status_400s[error - IPP_BAD_REQUEST]);
  else if (error >= IPP_INTERNAL_ERROR && error <= IPP_PRINTER_IS_DEACTIVATED)
    return (ipp_status_500s[error - IPP_INTERNAL_ERROR]);

 /*
  * No, build an "unknown-xxxx" error string...
  */

  sprintf(unknown, "unknown-%04x", error);

  return (unknown);
}


/*
 * 'ippErrorValue()' - Return a status code for the given name.
 */

ipp_status_t				/* O - IPP status code */
ippErrorValue(const char *name)		/* I - Name */
{
  int		i;


  for (i = 0; i < (sizeof(ipp_status_oks) / sizeof(ipp_status_oks[0])); i ++)
    if (!strcasecmp(name, ipp_status_oks[i]))
      return ((ipp_status_t)i);

  if (!strcasecmp(name, "redirection-other-site"))
    return (IPP_REDIRECTION_OTHER_SITE);

  for (i = 0; i < (sizeof(ipp_status_400s) / sizeof(ipp_status_400s[0])); i ++)
    if (!strcasecmp(name, ipp_status_400s[i]))
      return ((ipp_status_t)(i + 0x400));

  for (i = 0; i < (sizeof(ipp_status_500s) / sizeof(ipp_status_500s[0])); i ++)
    if (!strcasecmp(name, ipp_status_500s[i]))
      return ((ipp_status_t)(i + 0x500));

  return ((ipp_status_t)-1);
}


/*
 * 'ippOpString()' - Return a name for the given operation id.
 */

const char *				/* O - Name */
ippOpString(ipp_op_t op)		/* I - Operation ID */
{
  static char	unknown[255];		/* Unknown error statuses */


 /*
  * See if the operation ID is a known value...
  */

  if (op >= IPP_PRINT_JOB && op <= IPP_SCHEDULE_JOB_AFTER)
    return (ipp_std_ops[op]);
  else if (op == IPP_PRIVATE)
    return ("windows-ext");
  else if (op >= CUPS_GET_DEFAULT && op <= CUPS_DELETE_DEVICE)
    return (ipp_cups_ops[op - CUPS_GET_DEFAULT]);

 /*
  * No, build an "unknown-xxxx" operation string...
  */

  sprintf(unknown, "unknown-%04x", op);

  return (unknown);
}


/*
 * 'ippOpValue()' - Return an operation id for the given name.
 */

ipp_op_t				/* O - Operation ID */
ippOpValue(const char *name)		/* I - Textual name */
{
  int		i;


  for (i = 0; i < (sizeof(ipp_std_ops) / sizeof(ipp_std_ops[0])); i ++)
    if (!strcasecmp(name, ipp_std_ops[i]))
      return ((ipp_op_t)i);

  if (!strcasecmp(name, "windows-ext"))
    return (IPP_PRIVATE);

  for (i = 0; i < (sizeof(ipp_cups_ops) / sizeof(ipp_cups_ops[0])); i ++)
    if (!strcasecmp(name, ipp_cups_ops[i]))
      return ((ipp_op_t)(i + 0x4001));

  return ((ipp_op_t)0);
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
 * End of "$Id$".
 */
