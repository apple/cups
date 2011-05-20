/*
 * "$Id: ipp-support.c 9371 2010-11-17 06:21:32Z mike $"
 *
 *   Internet Printing Protocol support functions for CUPS.
 *
 *   Copyright 2007-2011 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
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
 *   _ippAttrString() - Convert the attribute's value to a string.
 *   ippErrorString() - Return a name for the given status code.
 *   ippErrorValue()  - Return a status code for the given name.
 *   ippOpString()    - Return a name for the given operation id.
 *   ippOpValue()     - Return an operation id for the given name.
 *   ippPort()        - Return the default IPP port number.
 *   ippSetPort()     - Set the default port number.
 *   ippTagString()   - Return the tag name corresponding to a tag value.
 *   ippTagValue()    - Return the tag value corresponding to a tag name.
 *   ipp_col_string() - Convert a collection to a string.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"


/*
 * Local globals...
 */

static const char * const ipp_status_oks[] =	/* "OK" status codes */
		{
		  "successful-ok",
		  "successful-ok-ignored-or-substituted-attributes",
		  "successful-ok-conflicting-attributes",
		  "successful-ok-ignored-subscriptions",
		  "successful-ok-ignored-notifications",
		  "successful-ok-too-many-events",
		  "successful-ok-but-cancel-subscription",
		  "successful-ok-events-complete"
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
		},
		* const ipp_status_1000s[] =		/* CUPS internal */
		{
		  "cups-authorization-canceled",
		  "cups-pki-error",
		  "cups-upgrade-required"
		};
static char	* const ipp_std_ops[] =
		{
		  /* 0x0000 - 0x000f */
		  "unknown-00",
		  "unknown-01",
		  "Print-Job",
		  "Print-URI",
		  "Validate-Job",
		  "Create-Job",
		  "Send-Document",
		  "Send-URI",
		  "Cancel-Job",
		  "Get-Job-Attributes",
		  "Get-Jobs",
		  "Get-Printer-Attributes",
		  "Hold-Job",
		  "Release-Job",
		  "Restart-Job",
		  "unknown-0f",

		  /* 0x0010 - 0x001f */
		  "Pause-Printer",
		  "Resume-Printer",
		  "Purge-Jobs",
		  "Set-Printer-Attributes",
		  "Set-Job-Attributes",
		  "Get-Printer-Supported-Values",
		  "Create-Printer-Subscription",
		  "Create-Job-Subscription",
		  "Get-Subscription-Attributes",
		  "Get-Subscriptions",
		  "Renew-Subscription",
		  "Cancel-Subscription",
		  "Get-Notifications",
		  "Send-Notifications",
		  "unknown-1e",
		  "unknown-1f",

		  /* 0x0020 - 0x002f */
		  "unknown-20",
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

		  /* 0x0030 - 0x003b */
		  "Promote-Job",
		  "Schedule-Job-After",
		  "unknown-32",
		  "Cancel-Document",
		  "Get-Document-Attributes",
		  "Get-Documents",
		  "Delete-Document",
		  "Set-Document-Attributes",
		  "Cancel-Jobs",
		  "Cancel-My-Jobs",
		  "Resubmit-Job",
		  "Close-Job"
		},
		* const ipp_cups_ops[] =
		{
		  "CUPS-Get-Default",
		  "CUPS-Get-Printers",
		  "CUPS-Add-Modify-Printer",
		  "CUPS-Delete-Printer",
		  "CUPS-Get-Classes",
		  "CUPS-Add-Modify-Class",
		  "CUPS-Delete-Class",
		  "CUPS-Accept-Jobs",
		  "CUPS-Reject-Jobs",
		  "CUPS-Set-Default",
		  "CUPS-Get-Devices",
		  "CUPS-Get-PPDs",
		  "CUPS-Move-Job",
		  "CUPS-Authenticate-Job",
		  "CUPS-Get-PPD"
		},
		* const ipp_cups_ops2[] =
		{
		  "CUPS-Get-Document"
		},
		* const ipp_tag_names[] =
		{			/* Value/group tag names */
		  "zero",		/* 0x00 */
		  "operation-attributes-tag",
					/* 0x01 */
		  "job-attributes-tag",	/* 0x02 */
		  "end-of-attributes-tag",
					/* 0x03 */
		  "printer-attributes-tag",
					/* 0x04 */
		  "unsupported-attributes-tag",
					/* 0x05 */
		  "subscription-attributes-tag",
					/* 0x06 */
		  "event-notification-attributes-tag",
					/* 0x07 */
		  "unknown-08",		/* 0x08 */
		  "unknown-09",		/* 0x09 */
		  "unknown-0a",		/* 0x0a */
		  "unknown-0b",		/* 0x0b */
		  "unknown-0c",		/* 0x0c */
		  "unknown-0d",		/* 0x0d */
		  "unknown-0e",		/* 0x0e */
		  "unknown-0f",		/* 0x0f */
		  "unsupported",	/* 0x10 */
		  "default",		/* 0x11 */
		  "unknown",		/* 0x12 */
		  "no-value",		/* 0x13 */
		  "unknown-14",		/* 0x14 */
		  "not-settable",	/* 0x15 */
		  "delete-attribute",	/* 0x16 */
		  "admin-define",	/* 0x17 */
		  "unknown-18",		/* 0x18 */
		  "unknown-19",		/* 0x19 */
		  "unknown-1a",		/* 0x1a */
		  "unknown-1b",		/* 0x1b */
		  "unknown-1c",		/* 0x1c */
		  "unknown-1d",		/* 0x1d */
		  "unknown-1e",		/* 0x1e */
		  "unknown-1f",		/* 0x1f */
		  "unknown-20",		/* 0x20 */
		  "integer",		/* 0x21 */
		  "boolean",		/* 0x22 */
		  "enum",		/* 0x23 */
		  "unknown-24",		/* 0x24 */
		  "unknown-25",		/* 0x25 */
		  "unknown-26",		/* 0x26 */
		  "unknown-27",		/* 0x27 */
		  "unknown-28",		/* 0x28 */
		  "unknown-29",		/* 0x29 */
		  "unknown-2a",		/* 0x2a */
		  "unknown-2b",		/* 0x2b */
		  "unknown-2c",		/* 0x2c */
		  "unknown-2d",		/* 0x2d */
		  "unknown-2e",		/* 0x2e */
		  "unknown-2f",		/* 0x2f */
		  "octetString",	/* 0x30 */
		  "dateTime",		/* 0x31 */
		  "resolution",		/* 0x32 */
		  "rangeOfInteger",	/* 0x33 */
		  "collection",		/* 0x34 */
		  "textWithLanguage",	/* 0x35 */
		  "nameWithLanguage",	/* 0x36 */
		  "endCollection",	/* 0x37 */
		  "unknown-38",		/* 0x38 */
		  "unknown-39",		/* 0x39 */
		  "unknown-3a",		/* 0x3a */
		  "unknown-3b",		/* 0x3b */
		  "unknown-3c",		/* 0x3c */
		  "unknown-3d",		/* 0x3d */
		  "unknown-3e",		/* 0x3e */
		  "unknown-3f",		/* 0x3f */
		  "unknown-40",		/* 0x40 */
		  "textWithoutLanguage",/* 0x41 */
		  "nameWithoutLanguage",/* 0x42 */
		  "unknown-43",		/* 0x43 */
		  "keyword",		/* 0x44 */
		  "uri",		/* 0x45 */
		  "uriScheme",		/* 0x46 */
		  "charset",		/* 0x47 */
		  "naturalLanguage",	/* 0x48 */
		  "mimeMediaType",	/* 0x49 */
		  "memberAttrName"	/* 0x4a */
		};
static const char * const job_states[] =
{					/* job-state enums */
  "pending",
  "pending-held",
  "processing",
  "processing-stopped",
  "canceled",
  "aborted",
  "completed"
};
static const char * const printer_states[] =
{					/* printer-state enums */
  "idle",
  "processing",
  "stopped",
};


/*
 * Local functions...
 */

static size_t	ipp_col_string(ipp_t *col, char *buffer, size_t bufsize);


/*
 * '_ippAttrString()' - Convert the attribute's value to a string.
 *
 * Returns the number of bytes that would be written, not including the
 * trailing nul. The buffer pointer can be NULL to get the required length,
 * just like (v)snprintf.
 */

size_t					/* O - Number of bytes less nul */
_ippAttrString(ipp_attribute_t *attr,	/* I - Attribute */
               char            *buffer,	/* I - String buffer or NULL */
               size_t          bufsize)	/* I - Size of string buffer */
{
  int		i;			/* Looping var */
  char		*bufptr,		/* Pointer into buffer */
		*bufend,		/* End of buffer */
		temp[256];		/* Temporary string */
  const char	*ptr;			/* Pointer into string */
  ipp_value_t	*val;			/* Current value */


  if (!attr || !attr->name)
  {
    if (buffer)
      *buffer = '\0';

    return (0);
  }

  bufptr = buffer;
  if (buffer)
    bufend = buffer + bufsize - 1;
  else
    bufend = NULL;

  for (i = attr->num_values, val = attr->values; i > 0; i --, val ++)
  {
    if (val > attr->values)
    {
      if (buffer && bufptr < bufend)
        *bufptr++ = ',';
      else
        bufptr ++;
    }

    switch (attr->value_tag & ~IPP_TAG_COPY)
    {
      case IPP_TAG_ENUM :
          if (!strcmp(attr->name, "printer-state") &&
              val->integer >= IPP_PRINTER_IDLE &&
              val->integer <= IPP_PRINTER_STOPPED)
          {
            ptr = printer_states[val->integer - IPP_PRINTER_IDLE];

            if (buffer && bufptr < bufend)
              strlcpy(bufptr, ptr, bufend - bufptr + 1);

            bufptr += strlen(ptr);
            break;
          }
          else if (!strcmp(attr->name, "job-state") &&
		   val->integer >= IPP_JOB_PENDING &&
		   val->integer <= IPP_JOB_COMPLETED)
          {
            ptr = job_states[val->integer - IPP_JOB_PENDING];

            if (buffer && bufptr < bufend)
              strlcpy(bufptr, ptr, bufend - bufptr + 1);

            bufptr += strlen(ptr);
            break;
          }

      case IPP_TAG_INTEGER :
          if (buffer && bufptr < bufend)
            bufptr += snprintf(bufptr, bufend - bufptr + 1, "%d", val->integer);
          else
            bufptr += snprintf(temp, sizeof(temp), "%d", val->integer);
          break;

      case IPP_TAG_BOOLEAN :
          if (buffer && bufptr < bufend)
            strlcpy(bufptr, val->boolean ? "true" : "false",
                    bufend - bufptr + 1);

          bufptr += val->boolean ? 4 : 5;
          break;

      case IPP_TAG_RANGE :
          if (buffer && bufptr < bufend)
            bufptr += snprintf(bufptr, bufend - bufptr + 1, "%d-%d",
                               val->range.lower, val->range.upper);
          else
            bufptr += snprintf(temp, sizeof(temp), "%d-%d", val->range.lower,
                               val->range.upper);
          break;

      case IPP_TAG_RESOLUTION :
          if (buffer && bufptr < bufend)
            bufptr += snprintf(bufptr, bufend - bufptr + 1, "%dx%d%s",
                               val->resolution.xres, val->resolution.yres,
                               val->resolution.units == IPP_RES_PER_INCH ?
                                   "dpi" : "dpc");
          else
            bufptr += snprintf(temp, sizeof(temp), "%dx%d%s",
                               val->resolution.xres, val->resolution.yres,
                               val->resolution.units == IPP_RES_PER_INCH ?
                                   "dpi" : "dpc");
          break;

      case IPP_TAG_DATE :
          {
            unsigned year;		/* Year */

            year = (val->date[0] << 8) + val->date[1];

	    if (val->date[9] == 0 && val->date[10] == 0)
	      snprintf(temp, sizeof(temp), "%04u-%02u-%02uT%02u:%02u:%02uZ",
		       year, val->date[2], val->date[3], val->date[4],
		       val->date[5], val->date[6]);
	    else
	      snprintf(temp, sizeof(temp),
	               "%04u-%02u-%02uT%02u:%02u:%02u%c%02u%02u",
		       year, val->date[2], val->date[3], val->date[4],
		       val->date[5], val->date[6], val->date[8], val->date[9],
		       val->date[10]);

            if (buffer && bufptr < bufend)
              strlcpy(bufptr, temp, bufend - bufptr + 1);

            bufptr += strlen(temp);
          }
          break;

      case IPP_TAG_TEXT :
      case IPP_TAG_NAME :
      case IPP_TAG_KEYWORD :
      case IPP_TAG_CHARSET :
      case IPP_TAG_URI :
      case IPP_TAG_MIMETYPE :
      case IPP_TAG_LANGUAGE :
      case IPP_TAG_TEXTLANG :
      case IPP_TAG_NAMELANG :
	  if (!val->string.text)
	    break;

          for (ptr = val->string.text; *ptr; ptr ++)
          {
            if (*ptr == '\\' || *ptr == '\"')
            {
              if (buffer && bufptr < bufend)
                *bufptr = '\\';
              bufptr ++;
            }

            if (buffer && bufptr < bufend)
              *bufptr = *ptr;
            bufptr ++;
          }
          break;

      case IPP_TAG_BEGIN_COLLECTION :
          if (buffer && bufptr < bufend)
            bufptr += ipp_col_string(val->collection, bufptr,
                                     bufend - bufptr + 1);
          else
            bufptr += ipp_col_string(val->collection, NULL, 0);
          break;

      case IPP_TAG_STRING :
          for (ptr = val->string.text; *ptr; ptr ++)
          {
            if (*ptr == '\\' || _cups_isspace(*ptr))
            {
              if (buffer && bufptr < bufend)
                *bufptr = '\\';
              bufptr ++;

              if (buffer && bufptr < bufend)
                *bufptr = *ptr;
              bufptr ++;
            }
            else if (!isprint(*ptr & 255))
            {
              if (buffer && bufptr < bufend)
                bufptr += snprintf(bufptr, bufend - bufptr + 1, "\\%03o",
                                   *ptr & 255);
              else
                bufptr += snprintf(temp, sizeof(temp), "\\%03o",
                                   *ptr & 255);
            }
            else
            {
              if (buffer && bufptr < bufend)
                *bufptr = *ptr;
              bufptr ++;
            }
          }
          break;

      default :
          ptr = ippTagString(attr->value_tag);
          if (buffer && bufptr < bufend)
            strlcpy(bufptr, ptr, bufend - bufptr + 1);
          bufptr += strlen(ptr);
          break;
    }
  }

  if (buffer && bufptr < bufend)
    *bufptr = '\0';
  else if (bufend)
    *bufend = '\0';

  return (bufptr - buffer);
}


/*
 * 'ippErrorString()' - Return a name for the given status code.
 */

const char *				/* O - Text string */
ippErrorString(ipp_status_t error)	/* I - Error status */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * See if the error code is a known value...
  */

  if (error >= IPP_OK && error <= IPP_OK_EVENTS_COMPLETE)
    return (ipp_status_oks[error]);
  else if (error == IPP_REDIRECTION_OTHER_SITE)
    return ("redirection-other-site");
  else if (error == CUPS_SEE_OTHER)
    return ("cups-see-other");
  else if (error >= IPP_BAD_REQUEST && error <= IPP_PRINT_SUPPORT_FILE_NOT_FOUND)
    return (ipp_status_400s[error - IPP_BAD_REQUEST]);
  else if (error >= IPP_INTERNAL_ERROR && error <= IPP_PRINTER_IS_DEACTIVATED)
    return (ipp_status_500s[error - IPP_INTERNAL_ERROR]);
  else if (error >= IPP_AUTHENTICATION_CANCELED && error <= IPP_UPGRADE_REQUIRED)
    return (ipp_status_1000s[error - IPP_AUTHENTICATION_CANCELED]);

 /*
  * No, build an "unknown-xxxx" error string...
  */

  sprintf(cg->ipp_unknown, "unknown-%04x", error);

  return (cg->ipp_unknown);
}


/*
 * 'ippErrorValue()' - Return a status code for the given name.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

ipp_status_t				/* O - IPP status code */
ippErrorValue(const char *name)		/* I - Name */
{
  int		i;


  for (i = 0; i < (sizeof(ipp_status_oks) / sizeof(ipp_status_oks[0])); i ++)
    if (!_cups_strcasecmp(name, ipp_status_oks[i]))
      return ((ipp_status_t)i);

  if (!_cups_strcasecmp(name, "redirection-other-site"))
    return (IPP_REDIRECTION_OTHER_SITE);

  if (!_cups_strcasecmp(name, "cups-see-other"))
    return (CUPS_SEE_OTHER);

  for (i = 0; i < (sizeof(ipp_status_400s) / sizeof(ipp_status_400s[0])); i ++)
    if (!_cups_strcasecmp(name, ipp_status_400s[i]))
      return ((ipp_status_t)(i + 0x400));

  for (i = 0; i < (sizeof(ipp_status_500s) / sizeof(ipp_status_500s[0])); i ++)
    if (!_cups_strcasecmp(name, ipp_status_500s[i]))
      return ((ipp_status_t)(i + 0x500));

  for (i = 0; i < (sizeof(ipp_status_1000s) / sizeof(ipp_status_1000s[0])); i ++)
    if (!_cups_strcasecmp(name, ipp_status_1000s[i]))
      return ((ipp_status_t)(i + 0x1000));

  return ((ipp_status_t)-1);
}


/*
 * 'ippOpString()' - Return a name for the given operation id.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

const char *				/* O - Name */
ippOpString(ipp_op_t op)		/* I - Operation ID */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * See if the operation ID is a known value...
  */

  if (op >= IPP_PRINT_JOB && op <= IPP_CLOSE_JOB)
    return (ipp_std_ops[op]);
  else if (op == IPP_PRIVATE)
    return ("windows-ext");
  else if (op >= CUPS_GET_DEFAULT && op <= CUPS_GET_PPD)
    return (ipp_cups_ops[op - CUPS_GET_DEFAULT]);
  else if (op == CUPS_GET_DOCUMENT)
    return (ipp_cups_ops2[0]);

 /*
  * No, build an "unknown-xxxx" operation string...
  */

  sprintf(cg->ipp_unknown, "unknown-%04x", op);

  return (cg->ipp_unknown);
}


/*
 * 'ippOpValue()' - Return an operation id for the given name.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

ipp_op_t				/* O - Operation ID */
ippOpValue(const char *name)		/* I - Textual name */
{
  int		i;


  for (i = 0; i < (sizeof(ipp_std_ops) / sizeof(ipp_std_ops[0])); i ++)
    if (!_cups_strcasecmp(name, ipp_std_ops[i]))
      return ((ipp_op_t)i);

  if (!_cups_strcasecmp(name, "windows-ext"))
    return (IPP_PRIVATE);

  for (i = 0; i < (sizeof(ipp_cups_ops) / sizeof(ipp_cups_ops[0])); i ++)
    if (!_cups_strcasecmp(name, ipp_cups_ops[i]))
      return ((ipp_op_t)(i + 0x4001));

  for (i = 0; i < (sizeof(ipp_cups_ops2) / sizeof(ipp_cups_ops2[0])); i ++)
    if (!_cups_strcasecmp(name, ipp_cups_ops2[i]))
      return ((ipp_op_t)(i + 0x4027));

  if (!_cups_strcasecmp(name, "CUPS-Add-Class"))
    return (CUPS_ADD_MODIFY_CLASS);

  if (!_cups_strcasecmp(name, "CUPS-Add-Printer"))
    return (CUPS_ADD_MODIFY_PRINTER);

  return ((ipp_op_t)-1);
}


/*
 * 'ippPort()' - Return the default IPP port number.
 */

int					/* O - Port number */
ippPort(void)
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  DEBUG_puts("ippPort()");

  if (!cg->ipp_port)
    _cupsSetDefaults();

  DEBUG_printf(("1ippPort: Returning %d...", cg->ipp_port));

  return (cg->ipp_port);
}


/*
 * 'ippSetPort()' - Set the default port number.
 */

void
ippSetPort(int p)			/* I - Port number to use */
{
  DEBUG_printf(("ippSetPort(p=%d)", p));

  _cupsGlobals()->ipp_port = p;
}


/*
 * 'ippTagString()' - Return the tag name corresponding to a tag value.
 *
 * The returned names are defined in RFC 2911 and 3382.
 *
 * @since CUPS 1.4/Mac OS X 10.6@
 */

const char *				/* O - Tag name */
ippTagString(ipp_tag_t tag)		/* I - Tag value */
{
  tag &= IPP_TAG_MASK;

  if (tag < (ipp_tag_t)(sizeof(ipp_tag_names) / sizeof(ipp_tag_names[0])))
    return (ipp_tag_names[tag]);
  else
    return ("UNKNOWN");
}


/*
 * 'ippTagValue()' - Return the tag value corresponding to a tag name.
 *
 * The tag names are defined in RFC 2911 and 3382.
 *
 * @since CUPS 1.4/Mac OS X 10.6@
 */

ipp_tag_t				/* O - Tag value */
ippTagValue(const char *name)		/* I - Tag name */
{
  int	i;				/* Looping var */


  for (i = 0; i < (sizeof(ipp_tag_names) / sizeof(ipp_tag_names[0])); i ++)
    if (!_cups_strcasecmp(name, ipp_tag_names[i]))
      return ((ipp_tag_t)i);

  if (!_cups_strcasecmp(name, "operation"))
    return (IPP_TAG_OPERATION);
  else if (!_cups_strcasecmp(name, "job"))
    return (IPP_TAG_JOB);
  else if (!_cups_strcasecmp(name, "printer"))
    return (IPP_TAG_PRINTER);
  else if (!_cups_strcasecmp(name, "unsupported"))
    return (IPP_TAG_UNSUPPORTED_GROUP);
  else if (!_cups_strcasecmp(name, "subscription"))
    return (IPP_TAG_SUBSCRIPTION);
  else if (!_cups_strcasecmp(name, "event"))
    return (IPP_TAG_EVENT_NOTIFICATION);
  else if (!_cups_strcasecmp(name, "language"))
    return (IPP_TAG_LANGUAGE);
  else if (!_cups_strcasecmp(name, "mimetype"))
    return (IPP_TAG_MIMETYPE);
  else if (!_cups_strcasecmp(name, "name"))
    return (IPP_TAG_NAME);
  else if (!_cups_strcasecmp(name, "text"))
    return (IPP_TAG_TEXT);
  else if (!_cups_strcasecmp(name, "begCollection"))
    return (IPP_TAG_BEGIN_COLLECTION);
  else
    return (IPP_TAG_ZERO);
}


/*
 * 'ipp_col_string()' - Convert a collection to a string.
 */

static size_t				/* O - Number of bytes */
ipp_col_string(ipp_t  *col,		/* I - Collection attribute */
               char   *buffer,		/* I - Buffer or NULL */
               size_t bufsize)		/* I - Size of buffer */
{
  char			*bufptr,	/* Position in buffer */
			*bufend,	/* End of buffer */
			temp[256];	/* Temporary string */
  ipp_attribute_t	*attr;		/* Current member attribute */


  bufptr = buffer;
  bufend = buffer + bufsize - 1;

  if (buffer && bufptr < bufend)
    *bufptr = '{';
  bufptr ++;

  for (attr = col->attrs; attr; attr = attr->next)
  {
    if (!attr->name)
      continue;

    if (buffer && bufptr < bufend)
      bufptr += snprintf(bufptr, bufend - bufptr + 1, "%s=", attr->name);
    else
      bufptr += strlen(attr->name) + 1;

    if (buffer && bufptr < bufend)
      bufptr += _ippAttrString(attr, bufptr, bufend - bufptr + 1);
    else
      bufptr += _ippAttrString(attr, temp, sizeof(temp));
  }

  if (buffer && bufptr < bufend)
    *bufptr = '}';
  bufptr ++;

  return (bufptr - buffer);
}


/*
 * End of "$Id: ipp-support.c 9371 2010-11-17 06:21:32Z mike $".
 */
