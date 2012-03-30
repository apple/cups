/*
 * "$Id: ipp-support.c 9371 2010-11-17 06:21:32Z mike $"
 *
 *   Internet Printing Protocol support functions for CUPS.
 *
 *   Copyright 2007-2012 by Apple Inc.
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
 *   ippAttributeString() - Convert the attribute's value to a string.
 *   ippEnumString()	  - Return a string corresponding to the enum value.
 *   ippEnumValue()	  - Return the value associated with a given enum
 *			    string.
 *   ippErrorString()	  - Return a name for the given status code.
 *   ippErrorValue()	  - Return a status code for the given name.
 *   ippOpString()	  - Return a name for the given operation id.
 *   ippOpValue()	  - Return an operation id for the given name.
 *   ippPort()		  - Return the default IPP port number.
 *   ippSetPort()	  - Set the default port number.
 *   ippTagString()	  - Return the tag name corresponding to a tag value.
 *   ippTagValue()	  - Return the tag value corresponding to a tag name.
 *   ipp_col_string()	  - Convert a collection to a string.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"


/*
 * Local globals...
 */

static const char * const ipp_status_oks[] =	/* "OK" status codes */
		{				/* (name) = abandoned standard value */
		  "successful-ok",
		  "successful-ok-ignored-or-substituted-attributes",
		  "successful-ok-conflicting-attributes",
		  "successful-ok-ignored-subscriptions",
		  "(successful-ok-ignored-notifications)",
		  "successful-ok-too-many-events",
		  "(successful-ok-but-cancel-subscription)",
		  "successful-ok-events-complete"
		},
		* const ipp_status_400s[] =	/* Client errors */
		{				/* (name) = abandoned standard value */
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
		  "(client-error-ignored-all-notifications)",
		  "(client-error-client-print-support-file-not-found)",
		  "client-error-document-password-error",
		  "client-error-document-permission-error",
		  "client-error-document-security-error",
		  "client-error-document-unprintable-error"
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
		  "server-error-printer-is-deactivated",
		  "server-error-too-many-jobs",
		  "server-error-too-many-documents"
		},
		* const ipp_status_1000s[] =		/* CUPS internal */
		{
		  "cups-authorization-canceled",
		  "cups-pki-error",
		  "cups-upgrade-required"
		};
static const char * const ipp_std_ops[] =
		{
		  /* 0x0000 - 0x000f */
		  "0x00",
		  "0x01",
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
		  "0x0f",

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
		  "(Send-Notifications)",
		  "(Get-Resource-Attributes)",
		  "(Get-Resource-Data)",

		  /* 0x0020 - 0x002f */
		  "(Get-Resources)",
		  "(Get-Printer-Support-Files)",
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

		  /* 0x0030 - 0x003d */
		  "Promote-Job",
		  "Schedule-Job-After",
		  "0x32",
		  "Cancel-Document",
		  "Get-Document-Attributes",
		  "Get-Documents",
		  "Delete-Document",
		  "Set-Document-Attributes",
		  "Cancel-Jobs",
		  "Cancel-My-Jobs",
		  "Resubmit-Job",
		  "Close-Job",
		  "Identify-Printer",
		  "Validate-Document"
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
		  "(resource-attributes-tag)",
		  			/* 0x08 */
		  "document-attributes-tag",
					/* 0x09 */
		  "0x0a",		/* 0x0a */
		  "0x0b",		/* 0x0b */
		  "0x0c",		/* 0x0c */
		  "0x0d",		/* 0x0d */
		  "0x0e",		/* 0x0e */
		  "0x0f",		/* 0x0f */
		  "unsupported",	/* 0x10 */
		  "default",		/* 0x11 */
		  "unknown",		/* 0x12 */
		  "no-value",		/* 0x13 */
		  "0x14",		/* 0x14 */
		  "not-settable",	/* 0x15 */
		  "delete-attribute",	/* 0x16 */
		  "admin-define",	/* 0x17 */
		  "0x18",		/* 0x18 */
		  "0x19",		/* 0x19 */
		  "0x1a",		/* 0x1a */
		  "0x1b",		/* 0x1b */
		  "0x1c",		/* 0x1c */
		  "0x1d",		/* 0x1d */
		  "0x1e",		/* 0x1e */
		  "0x1f",		/* 0x1f */
		  "0x20",		/* 0x20 */
		  "integer",		/* 0x21 */
		  "boolean",		/* 0x22 */
		  "enum",		/* 0x23 */
		  "0x24",		/* 0x24 */
		  "0x25",		/* 0x25 */
		  "0x26",		/* 0x26 */
		  "0x27",		/* 0x27 */
		  "0x28",		/* 0x28 */
		  "0x29",		/* 0x29 */
		  "0x2a",		/* 0x2a */
		  "0x2b",		/* 0x2b */
		  "0x2c",		/* 0x2c */
		  "0x2d",		/* 0x2d */
		  "0x2e",		/* 0x2e */
		  "0x2f",		/* 0x2f */
		  "octetString",	/* 0x30 */
		  "dateTime",		/* 0x31 */
		  "resolution",		/* 0x32 */
		  "rangeOfInteger",	/* 0x33 */
		  "collection",		/* 0x34 */
		  "textWithLanguage",	/* 0x35 */
		  "nameWithLanguage",	/* 0x36 */
		  "endCollection",	/* 0x37 */
		  "0x38",		/* 0x38 */
		  "0x39",		/* 0x39 */
		  "0x3a",		/* 0x3a */
		  "0x3b",		/* 0x3b */
		  "0x3c",		/* 0x3c */
		  "0x3d",		/* 0x3d */
		  "0x3e",		/* 0x3e */
		  "0x3f",		/* 0x3f */
		  "0x40",		/* 0x40 */
		  "textWithoutLanguage",/* 0x41 */
		  "nameWithoutLanguage",/* 0x42 */
		  "0x43",		/* 0x43 */
		  "keyword",		/* 0x44 */
		  "uri",		/* 0x45 */
		  "uriScheme",		/* 0x46 */
		  "charset",		/* 0x47 */
		  "naturalLanguage",	/* 0x48 */
		  "mimeMediaType",	/* 0x49 */
		  "memberAttrName"	/* 0x4a */
		};
static const char * const ipp_document_states[] =
		{			/* document-state-enums */
		  "pending",
		  "4",
		  "processing",
		  "6",
		  "canceled",
		  "aborted",
		  "completed"
		},
		* const ipp_finishings[] =
		{			/* finishings enums */
		  "none",
		  "staple",
		  "punch",
		  "cover",
		  "bind",
		  "saddle-stitch",
		  "edge-stitch",
		  "fold",
		  "trim",
		  "bale",
		  "booklet-maker",
		  "jog-offset",
		  "15",
		  "16",
		  "17",
		  "18",
		  "19",
		  "staple-top-left",
		  "staple-bottom-left",
		  "staple-top-right",
		  "staple-bottom-right",
		  "edge-stitch-left",
		  "edge-stitch-top",
		  "edge-stitch-right",
		  "edge-stitch-bottom",
		  "staple-dual-left",
		  "staple-dual-top",
		  "staple-dual-right",
		  "staple-dual-bottom",
		  "32",
		  "33",
		  "34",
		  "35",
		  "36",
		  "37",
		  "38",
		  "39",
		  "40",
		  "41",
		  "42",
		  "43",
		  "44",
		  "45",
		  "46",
		  "47",
		  "48",
		  "49",
		  "bind-left",
		  "bind-top",
		  "bind-right",
		  "bind-bottom",
		  "54",
		  "55",
		  "56",
		  "57",
		  "58",
		  "59",
		  "trim-after-pages",
		  "trim-after-documents",
		  "trim-after-copies",
		  "trim-after-job"
		},
		* const ipp_job_collation_types[] =
		{			/* job-collation-type enums */
		  "uncollated-sheets",
		  "collated-documents",
		  "uncollated-documents"
		},
		* const ipp_job_states[] =
		{			/* job-state enums */
		  "pending",
		  "pending-held",
		  "processing",
		  "processing-stopped",
		  "canceled",
		  "aborted",
		  "completed"
		},
		* const ipp_orientation_requesteds[] =
		{			/* orientation-requested enums */
		  "portrait",
		  "landscape",
		  "reverse-landscape",
		  "reverse-portrait"
		},
		* const ipp_print_qualities[] =
		{			/* print-quality enums */
		  "draft",
		  "normal",
		  "high"
		},
		* const ipp_printer_states[] =
		{			/* printer-state enums */
		  "idle",
		  "processing",
		  "stopped",
		};


/*
 * Local functions...
 */

static size_t	ipp_col_string(ipp_t *col, char *buffer, size_t bufsize);


/*
 * 'ippAttributeString()' - Convert the attribute's value to a string.
 *
 * Returns the number of bytes that would be written, not including the
 * trailing nul. The buffer pointer can be NULL to get the required length,
 * just like (v)snprintf.
 *
 * @since CUPS 1.6@
 */

size_t					/* O - Number of bytes less nul */
ippAttributeString(
    ipp_attribute_t *attr,		/* I - Attribute */
    char            *buffer,		/* I - String buffer or NULL */
    size_t          bufsize)		/* I - Size of string buffer */
{
  int		i;			/* Looping var */
  char		*bufptr,		/* Pointer into buffer */
		*bufend,		/* End of buffer */
		temp[256];		/* Temporary string */
  const char	*ptr;			/* Pointer into string */
  _ipp_value_t	*val;			/* Current value */


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
          ptr = ippEnumString(attr->name, val->integer);

          if (buffer && bufptr < bufend)
            strlcpy(bufptr, ptr, bufend - bufptr + 1);

          bufptr += strlen(ptr);
          break;

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
                                   "dpi" : "dpcm");
          else
            bufptr += snprintf(temp, sizeof(temp), "%dx%d%s",
                               val->resolution.xres, val->resolution.yres,
                               val->resolution.units == IPP_RES_PER_INCH ?
                                   "dpi" : "dpcm");
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
      case IPP_TAG_URISCHEME :
      case IPP_TAG_MIMETYPE :
      case IPP_TAG_LANGUAGE :
      case IPP_TAG_TEXTLANG :
      case IPP_TAG_NAMELANG :
	  if (!val->string.text)
	    break;

          for (ptr = val->string.text; *ptr; ptr ++)
          {
            if (*ptr == '\\' || *ptr == '\"' || *ptr == '[')
            {
              if (buffer && bufptr < bufend)
                *bufptr = '\\';
              bufptr ++;
            }

            if (buffer && bufptr < bufend)
              *bufptr = *ptr;
            bufptr ++;
          }

          if (val->string.language)
          {
           /*
            * Add "[language]" to end of string...
            */

            if (buffer && bufptr < bufend)
              *bufptr = '[';
            bufptr ++;

            if (buffer && bufptr < bufend)
              strlcpy(bufptr, val->string.language, bufend - bufptr);
            bufptr += strlen(val->string.language);

            if (buffer && bufptr < bufend)
              *bufptr = ']';
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
 * 'ippEnumString()' - Return a string corresponding to the enum value.
 */

const char *				/* O - Enum string */
ippEnumString(const char *attrname,	/* I - Attribute name */
              int        enumvalue)	/* I - Enum value */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * Check for standard enum values...
  */

  if (!strcmp(attrname, "document-state") &&
      enumvalue >= 3 &&
      enumvalue <= (3 + (int)(sizeof(ipp_document_states) /
                              sizeof(ipp_document_states[0]))))
    return (ipp_document_states[enumvalue - 3]);
  else if ((!strcmp(attrname, "finishings") ||
            !strcmp(attrname, "finishings-actual") ||
            !strcmp(attrname, "finishings-default") ||
            !strcmp(attrname, "finishings-ready") ||
            !strcmp(attrname, "finishings-supported")) &&
           enumvalue >= 3 &&
           enumvalue <= (3 + (int)(sizeof(ipp_finishings) / sizeof(ipp_finishings[0]))))
    return (ipp_finishings[enumvalue - 3]);
  else if ((!strcmp(attrname, "job-collation-type") ||
            !strcmp(attrname, "job-collation-type-actual")) &&
           enumvalue >= 3 &&
           enumvalue <= (3 + (int)(sizeof(ipp_job_collation_types) /
                                   sizeof(ipp_job_collation_types[0]))))
    return (ipp_job_collation_types[enumvalue - 3]);
  else if (!strcmp(attrname, "job-state") &&
	   enumvalue >= IPP_JOB_PENDING && enumvalue <= IPP_JOB_COMPLETED)
    return (ipp_job_states[enumvalue - IPP_JOB_PENDING]);
  else if (!strcmp(attrname, "operations-supported"))
    return (ippOpString((ipp_op_t)enumvalue));
  else if ((!strcmp(attrname, "orientation-requested") ||
            !strcmp(attrname, "orientation-requested-actual") ||
            !strcmp(attrname, "orientation-requested-default") ||
            !strcmp(attrname, "orientation-requested-supported")) &&
           enumvalue >= 3 &&
           enumvalue <= (3 + (int)(sizeof(ipp_orientation_requesteds) /
                                   sizeof(ipp_orientation_requesteds[0]))))
    return (ipp_orientation_requesteds[enumvalue - 3]);
  else if ((!strcmp(attrname, "print-quality") ||
            !strcmp(attrname, "print-quality-actual") ||
            !strcmp(attrname, "print-quality-default") ||
            !strcmp(attrname, "print-quality-supported")) &&
           enumvalue >= 3 &&
           enumvalue <= (3 + (int)(sizeof(ipp_print_qualities) /
                                   sizeof(ipp_print_qualities[0]))))
    return (ipp_print_qualities[enumvalue - 3]);
  else if (!strcmp(attrname, "printer-state") &&
           enumvalue >= IPP_PRINTER_IDLE && enumvalue <= IPP_PRINTER_STOPPED)
    return (ipp_printer_states[enumvalue - IPP_PRINTER_IDLE]);

 /*
  * Not a standard enum value, just return the decimal equivalent...
  */

  snprintf(cg->ipp_unknown, sizeof(cg->ipp_unknown), "%d", enumvalue);
  return (cg->ipp_unknown);
}


/*
 * 'ippEnumValue()' - Return the value associated with a given enum string.
 */

int					/* O - Enum value or -1 if unknown */
ippEnumValue(const char *attrname,	/* I - Attribute name */
             const char *enumstring)	/* I - Enum string */
{
  int		i,			/* Looping var */
		num_strings;		/* Number of strings to compare */
  const char * const *strings;		/* Strings to compare */


 /*
  * If the string is just a number, return it...
  */

  if (isdigit(*enumstring & 255))
    return (strtol(enumstring, NULL, 0));

 /*
  * Otherwise look up the string...
  */

  if (!strcmp(attrname, "document-state"))
  {
    num_strings = (int)(sizeof(ipp_document_states) / sizeof(ipp_document_states[0]));
    strings     = ipp_document_states;
  }
  else if (!strcmp(attrname, "finishings") ||
	   !strcmp(attrname, "finishings-actual") ||
	   !strcmp(attrname, "finishings-default") ||
	   !strcmp(attrname, "finishings-ready") ||
	   !strcmp(attrname, "finishings-supported"))
  {
    num_strings = (int)(sizeof(ipp_finishings) / sizeof(ipp_finishings[0]));
    strings     = ipp_finishings;
  }
  else if (!strcmp(attrname, "job-collation-type") ||
           !strcmp(attrname, "job-collation-type-actual"))
  {
    num_strings = (int)(sizeof(ipp_job_collation_types) /
                        sizeof(ipp_job_collation_types[0]));
    strings     = ipp_job_collation_types;
  }
  else if (!strcmp(attrname, "job-state"))
  {
    num_strings = (int)(sizeof(ipp_job_states) / sizeof(ipp_job_states[0]));
    strings     = ipp_job_states;
  }
  else if (!strcmp(attrname, "operations-supported"))
    return (ippOpValue(enumstring));
  else if (!strcmp(attrname, "orientation-requested") ||
           !strcmp(attrname, "orientation-requested-actual") ||
           !strcmp(attrname, "orientation-requested-default") ||
           !strcmp(attrname, "orientation-requested-supported"))
  {
    num_strings = (int)(sizeof(ipp_orientation_requesteds) /
                        sizeof(ipp_orientation_requesteds[0]));
    strings     = ipp_orientation_requesteds;
  }
  else if (!strcmp(attrname, "print-quality") ||
           !strcmp(attrname, "print-quality-actual") ||
           !strcmp(attrname, "print-quality-default") ||
           !strcmp(attrname, "print-quality-supported"))
  {
    num_strings = (int)(sizeof(ipp_print_qualities) / sizeof(ipp_print_qualities[0]));
    strings     = ipp_print_qualities;
  }
  else if (!strcmp(attrname, "printer-state"))
  {
    num_strings = (int)(sizeof(ipp_printer_states) / sizeof(ipp_printer_states[0]));
    strings     = ipp_printer_states;
  }
  else
    return (-1);

  for (i = 0; i < num_strings; i ++)
    if (!strcmp(enumstring, strings[i]))
      return (i + 3);

  return (-1);
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
  * No, build an "0xxxxx" error string...
  */

  sprintf(cg->ipp_unknown, "0x%04x", error);

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
  * No, build an "0xxxxx" operation string...
  */

  sprintf(cg->ipp_unknown, "0x%04x", op);

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


  if (!strncmp(name, "0x", 2))
    return ((ipp_op_t)strtol(name + 2, NULL, 16));

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
			prefix = '{',	/* Prefix character */
			temp[256];	/* Temporary string */
  ipp_attribute_t	*attr;		/* Current member attribute */


  bufptr = buffer;
  bufend = buffer + bufsize - 1;

  for (attr = col->attrs; attr; attr = attr->next)
  {
    if (!attr->name)
      continue;

    if (buffer && bufptr < bufend)
      *bufptr = prefix;
    bufptr ++;
    prefix = ' ';

    if (buffer && bufptr < bufend)
      bufptr += snprintf(bufptr, bufend - bufptr + 1, "%s=", attr->name);
    else
      bufptr += strlen(attr->name) + 1;

    if (buffer && bufptr < bufend)
      bufptr += ippAttributeString(attr, bufptr, bufend - bufptr + 1);
    else
      bufptr += ippAttributeString(attr, temp, sizeof(temp));
  }

  if (prefix == '{')
  {
    if (buffer && bufptr < bufend)
      *bufptr = prefix;
    bufptr ++;
  }

  if (buffer && bufptr < bufend)
    *bufptr = '}';
  bufptr ++;

  return (bufptr - buffer);
}


/*
 * End of "$Id: ipp-support.c 9371 2010-11-17 06:21:32Z mike $".
 */
