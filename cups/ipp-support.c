/*
 * Internet Printing Protocol support functions for CUPS.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"


/*
 * Local globals...
 */

static const char * const ipp_states[] =
		{
		  "IPP_STATE_ERROR",
		  "IPP_STATE_IDLE",
		  "IPP_STATE_HEADER",
		  "IPP_STATE_ATTRIBUTE",
		  "IPP_STATE_DATA"
		};
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
		  "client-error-document-unprintable-error",
		  "client-error-account-info-needed",
		  "client-error-account-closed",
		  "client-error-account-limit-reached",
		  "client-error-account-authorization-failed",
		  "client-error-not-fetchable"
		},
		* const ipp_status_480s[] =	/* Vendor client errors */
		{
		  /* 0x0480 - 0x048F */
		  "0x0480",
		  "0x0481",
		  "0x0482",
		  "0x0483",
		  "0x0484",
		  "0x0485",
		  "0x0486",
		  "0x0487",
		  "0x0488",
		  "0x0489",
		  "0x048A",
		  "0x048B",
		  "0x048C",
		  "0x048D",
		  "0x048E",
		  "0x048F",
		  /* 0x0490 - 0x049F */
		  "0x0490",
		  "0x0491",
		  "0x0492",
		  "0x0493",
		  "0x0494",
		  "0x0495",
		  "0x0496",
		  "0x0497",
		  "0x0498",
		  "0x0499",
		  "0x049A",
		  "0x049B",
		  "cups-error-account-info-needed",
		  "cups-error-account-closed",
		  "cups-error-account-limit-reached",
		  "cups-error-account-authorization-failed"
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
		  "cups-authentication-canceled",
		  "cups-pki-error",
		  "cups-upgrade-required"
		};
static const char * const ipp_std_ops[] =
		{
		  /* 0x0000 - 0x000f */
		  "0x0000",
		  "0x0001",
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
		  "0x000f",

		  /* 0x0010 - 0x001f */
		  "Pause-Printer",
		  "Resume-Printer",
		  "Purge-Jobs",
		  "Set-Printer-Attributes",
		  "Set-Job-Attributes",
		  "Get-Printer-Supported-Values",
		  "Create-Printer-Subscriptions",
		  "Create-Job-Subscriptions",
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

		  /* 0x0030 - 0x003f */
		  "Promote-Job",
		  "Schedule-Job-After",
		  "0x0032",
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
		  "Validate-Document",
		  "Add-Document-Images",
		  "Acknowledge-Document",

		  /* 0x0040 - 0x004a */
		  "Acknowledge-Identify-Printer",
		  "Acknowledge-Job",
		  "Fetch-Document",
		  "Fetch-Job",
		  "Get-Output-Device-Attributes",
		  "Update-Active-Jobs",
		  "Deregister-Output-Device",
		  "Update-Document-Status",
		  "Update-Job-Status",
		  "Update-Output-Device-Attributes",
		  "Get-Next-Document-Data"
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
		  "CUPS-Get-Document",
		  "CUPS-Create-Local-Printer"
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
		  "processing-stopped",	/* IPPSIX */
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
		  "coat",		/* Finishings 2.0 */
		  "laminate",		/* Finishings 2.0 */
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
		  "staple-triple-left",	/* Finishings 2.0 */
		  "staple-triple-top",	/* Finishings 2.0 */
		  "staple-triple-right",/* Finishings 2.0 */
		  "staple-triple-bottom",/* Finishings 2.0 */
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
		  "trim-after-job",
		  "64",
		  "65",
		  "66",
		  "67",
		  "68",
		  "69",
		  "punch-top-left",	/* Finishings 2.0 */
		  "punch-bottom-left",	/* Finishings 2.0 */
		  "punch-top-right",	/* Finishings 2.0 */
		  "punch-bottom-right",	/* Finishings 2.0 */
		  "punch-dual-left",	/* Finishings 2.0 */
		  "punch-dual-top",	/* Finishings 2.0 */
		  "punch-dual-right",	/* Finishings 2.0 */
		  "punch-dual-bottom",	/* Finishings 2.0 */
		  "punch-triple-left",	/* Finishings 2.0 */
		  "punch-triple-top",	/* Finishings 2.0 */
		  "punch-triple-right",	/* Finishings 2.0 */
		  "punch-triple-bottom",/* Finishings 2.0 */
		  "punch-quad-left",	/* Finishings 2.0 */
		  "punch-quad-top",	/* Finishings 2.0 */
		  "punch-quad-right",	/* Finishings 2.0 */
		  "punch-quad-bottom",	/* Finishings 2.0 */
		  "punch-multiple-left",/* Finishings 2.1/Canon */
		  "punch-multiple-top",	/* Finishings 2.1/Canon */
		  "punch-multiple-right",/* Finishings 2.1/Canon */
		  "punch-multiple-bottom",/* Finishings 2.1/Canon */
		  "fold-accordian",	/* Finishings 2.0 */
		  "fold-double-gate",	/* Finishings 2.0 */
		  "fold-gate",		/* Finishings 2.0 */
		  "fold-half",		/* Finishings 2.0 */
		  "fold-half-z",	/* Finishings 2.0 */
		  "fold-left-gate",	/* Finishings 2.0 */
		  "fold-letter",	/* Finishings 2.0 */
		  "fold-parallel",	/* Finishings 2.0 */
		  "fold-poster",	/* Finishings 2.0 */
		  "fold-right-gate",	/* Finishings 2.0 */
		  "fold-z",		/* Finishings 2.0 */
                  "fold-engineering-z"	/* Finishings 2.1 */
		},
		* const ipp_finishings_vendor[] =
		{
		  /* 0x40000000 to 0x4000000F */
		  "0x40000000",
		  "0x40000001",
		  "0x40000002",
		  "0x40000003",
		  "0x40000004",
		  "0x40000005",
		  "0x40000006",
		  "0x40000007",
		  "0x40000008",
		  "0x40000009",
		  "0x4000000A",
		  "0x4000000B",
		  "0x4000000C",
		  "0x4000000D",
		  "0x4000000E",
		  "0x4000000F",
		  /* 0x40000010 to 0x4000001F */
		  "0x40000010",
		  "0x40000011",
		  "0x40000012",
		  "0x40000013",
		  "0x40000014",
		  "0x40000015",
		  "0x40000016",
		  "0x40000017",
		  "0x40000018",
		  "0x40000019",
		  "0x4000001A",
		  "0x4000001B",
		  "0x4000001C",
		  "0x4000001D",
		  "0x4000001E",
		  "0x4000001F",
		  /* 0x40000020 to 0x4000002F */
		  "0x40000020",
		  "0x40000021",
		  "0x40000022",
		  "0x40000023",
		  "0x40000024",
		  "0x40000025",
		  "0x40000026",
		  "0x40000027",
		  "0x40000028",
		  "0x40000029",
		  "0x4000002A",
		  "0x4000002B",
		  "0x4000002C",
		  "0x4000002D",
		  "0x4000002E",
		  "0x4000002F",
		  /* 0x40000030 to 0x4000003F */
		  "0x40000030",
		  "0x40000031",
		  "0x40000032",
		  "0x40000033",
		  "0x40000034",
		  "0x40000035",
		  "0x40000036",
		  "0x40000037",
		  "0x40000038",
		  "0x40000039",
		  "0x4000003A",
		  "0x4000003B",
		  "0x4000003C",
		  "0x4000003D",
		  "0x4000003E",
		  "0x4000003F",
		  /* 0x40000040 - 0x4000004F */
		  "0x40000040",
		  "0x40000041",
		  "0x40000042",
		  "0x40000043",
		  "0x40000044",
		  "0x40000045",
		  "cups-punch-top-left",
		  "cups-punch-bottom-left",
		  "cups-punch-top-right",
		  "cups-punch-bottom-right",
		  "cups-punch-dual-left",
		  "cups-punch-dual-top",
		  "cups-punch-dual-right",
		  "cups-punch-dual-bottom",
		  "cups-punch-triple-left",
		  "cups-punch-triple-top",
		  /* 0x40000050 - 0x4000005F */
		  "cups-punch-triple-right",
		  "cups-punch-triple-bottom",
		  "cups-punch-quad-left",
		  "cups-punch-quad-top",
		  "cups-punch-quad-right",
		  "cups-punch-quad-bottom",
		  "0x40000056",
		  "0x40000057",
		  "0x40000058",
		  "0x40000059",
		  "cups-fold-accordian",
		  "cups-fold-double-gate",
		  "cups-fold-gate",
		  "cups-fold-half",
		  "cups-fold-half-z",
		  "cups-fold-left-gate",
		  /* 0x40000060 - 0x40000064 */
		  "cups-fold-letter",
		  "cups-fold-parallel",
		  "cups-fold-poster",
		  "cups-fold-right-gate",
		  "cups-fold-z"
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
		  "reverse-portrait",
		  "none"
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
 * @since CUPS 1.6/macOS 10.8@
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
  const char	*ptr,			/* Pointer into string */
		*end;			/* Pointer to end of string */
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

    switch (attr->value_tag & ~IPP_TAG_CUPS_CONST)
    {
      case IPP_TAG_ENUM :
          ptr = ippEnumString(attr->name, val->integer);

          if (buffer && bufptr < bufend)
            strlcpy(bufptr, ptr, (size_t)(bufend - bufptr + 1));

          bufptr += strlen(ptr);
          break;

      case IPP_TAG_INTEGER :
          if (buffer && bufptr < bufend)
            bufptr += snprintf(bufptr, (size_t)(bufend - bufptr + 1), "%d", val->integer);
          else
            bufptr += snprintf(temp, sizeof(temp), "%d", val->integer);
          break;

      case IPP_TAG_BOOLEAN :
          if (buffer && bufptr < bufend)
            strlcpy(bufptr, val->boolean ? "true" : "false", (size_t)(bufend - bufptr + 1));

          bufptr += val->boolean ? 4 : 5;
          break;

      case IPP_TAG_RANGE :
          if (buffer && bufptr < bufend)
            bufptr += snprintf(bufptr, (size_t)(bufend - bufptr + 1), "%d-%d", val->range.lower, val->range.upper);
          else
            bufptr += snprintf(temp, sizeof(temp), "%d-%d", val->range.lower, val->range.upper);
          break;

      case IPP_TAG_RESOLUTION :
	  if (val->resolution.xres == val->resolution.yres)
	  {
	    if (buffer && bufptr < bufend)
	      bufptr += snprintf(bufptr, (size_t)(bufend - bufptr + 1), "%d%s", val->resolution.xres, val->resolution.units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
	    else
	      bufptr += snprintf(temp, sizeof(temp), "%d%s", val->resolution.xres, val->resolution.units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
	  }
	  else if (buffer && bufptr < bufend)
            bufptr += snprintf(bufptr, (size_t)(bufend - bufptr + 1), "%dx%d%s", val->resolution.xres, val->resolution.yres, val->resolution.units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
          else
            bufptr += snprintf(temp, sizeof(temp), "%dx%d%s", val->resolution.xres, val->resolution.yres, val->resolution.units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
          break;

      case IPP_TAG_DATE :
          {
            unsigned year;		/* Year */

            year = ((unsigned)val->date[0] << 8) + (unsigned)val->date[1];

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
              strlcpy(bufptr, temp, (size_t)(bufend - bufptr + 1));

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
              strlcpy(bufptr, val->string.language, (size_t)(bufend - bufptr));
            bufptr += strlen(val->string.language);

            if (buffer && bufptr < bufend)
              *bufptr = ']';
            bufptr ++;
          }
          break;

      case IPP_TAG_BEGIN_COLLECTION :
          if (buffer && bufptr < bufend)
            bufptr += ipp_col_string(val->collection, bufptr, (size_t)(bufend - bufptr + 1));
          else
            bufptr += ipp_col_string(val->collection, NULL, 0);
          break;

      case IPP_TAG_STRING :
          for (ptr = val->unknown.data, end = ptr + val->unknown.length;
               ptr < end; ptr ++)
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
                bufptr += snprintf(bufptr, (size_t)(bufend - bufptr + 1), "\\%03o", *ptr & 255);
              else
                bufptr += snprintf(temp, sizeof(temp), "\\%03o", *ptr & 255);
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
            strlcpy(bufptr, ptr, (size_t)(bufend - bufptr + 1));
          bufptr += strlen(ptr);
          break;
    }
  }

  if (buffer && bufptr < bufend)
    *bufptr = '\0';
  else if (bufend)
    *bufend = '\0';

  return ((size_t)(bufptr - buffer));
}


/*
 * 'ippCreateRequestedArray()' - Create a CUPS array of attribute names from the
 *                               given requested-attributes attribute.
 *
 * This function creates a (sorted) CUPS array of attribute names matching the
 * list of "requested-attribute" values supplied in an IPP request.  All IANA-
 * registered values are supported in addition to the CUPS IPP extension
 * attributes.
 *
 * The @code request@ parameter specifies the request message that was read from
 * the client.
 *
 * @code NULL@ is returned if all attributes should be returned.  Otherwise, the
 * result is a sorted array of attribute names, where @code cupsArrayFind(array,
 * "attribute-name")@ will return a non-NULL pointer.  The array must be freed
 * using the @code cupsArrayDelete@ function.
 *
 * @since CUPS 1.7/macOS 10.9@
 */

cups_array_t *				/* O - CUPS array or @code NULL@ if all */
ippCreateRequestedArray(ipp_t *request)	/* I - IPP request */
{
  int			i, j,		/* Looping vars */
			count,		/* Number of values */
			added;		/* Was name added? */
  ipp_attribute_t	*requested;	/* requested-attributes attribute */
  cups_array_t		*ra;		/* Requested attributes array */
  const char		*value;		/* Current value */
  /* The following lists come from the current IANA IPP registry of attributes */
  static const char * const document_description[] =
  {					/* document-description group */
    "compression",
    "copies-actual",
    "cover-back-actual",
    "cover-front-actual",
    "current-page-order",
    "date-time-at-completed",
    "date-time-at-creation",
    "date-time-at-processing",
    "detailed-status-messages",
    "document-access-errors",
    "document-charset",
    "document-digital-signature",
    "document-format",
    "document-format-details",
    "document-format-detected",
    "document-format-version",
    "document-format-version-detected",
    "document-job-id",
    "document-job-uri",
    "document-message",
    "document-metadata",
    "document-name",
    "document-natural-language",
    "document-number",
    "document-printer-uri",
    "document-state",
    "document-state-message",
    "document-state-reasons",
    "document-uri",
    "document-uuid",
    "errors-count",
    "finishings-actual",
    "finishings-col-actual",
    "force-front-side-actual",
    "imposition-template-actual",
    "impressions",
    "impressions-completed",
    "impressions-completed-current-copy",
    "insert-sheet-actual",
    "k-octets",
    "k-octets-processed",
    "last-document",
    "materials-col-actual",		/* IPP 3D */
    "media-actual",
    "media-col-actual",
    "media-input-tray-check-actual",
    "media-sheets",
    "media-sheets-completed",
    "more-info",
    "multiple-object-handling-actual",	/* IPP 3D */
    "number-up-actual",
    "orientation-requested-actual",
    "output-bin-actual",
    "output-device-assigned",
    "overrides-actual",
    "page-delivery-actual",
    "page-order-received-actual",
    "page-ranges-actual",
    "pages",
    "pages-completed",
    "pages-completed-current-copy",
    "platform-temperature-actual",	/* IPP 3D */
    "presentation-direction-number-up-actual",
    "print-accuracy-actual",		/* IPP 3D */
    "print-base-actual",		/* IPP 3D */
    "print-color-mode-actual",
    "print-content-optimize-actual",
    "print-objects-actual",		/* IPP 3D */
    "print-quality-actual",
    "print-rendering-intent-actual",
    "print-scaling-actual",		/* IPP Paid Printing */
    "print-supports-actual",		/* IPP 3D */
    "printer-resolution-actual",
    "printer-up-time",
    "separator-sheets-actual",
    "sheet-completed-copy-number",
    "sides-actual",
    "time-at-completed",
    "time-at-creation",
    "time-at-processing",
    "x-image-position-actual",
    "x-image-shift-actual",
    "x-side1-image-shift-actual",
    "x-side2-image-shift-actual",
    "y-image-position-actual",
    "y-image-shift-actual",
    "y-side1-image-shift-actual",
    "y-side2-image-shift-actual"
  };
  static const char * const document_template[] =
  {					/* document-template group */
    "copies",
    "copies-default",
    "copies-supported",
    "cover-back",
    "cover-back-default",
    "cover-back-supported",
    "cover-front",
    "cover-front-default",
    "cover-front-supported",
    "feed-orientation",
    "feed-orientation-default",
    "feed-orientation-supported",
    "finishings",
    "finishings-col",
    "finishings-col-default",
    "finishings-col-supported",
    "finishings-default",
    "finishings-supported",
    "font-name-requested",
    "font-name-requested-default",
    "font-name-requested-supported",
    "font-size-requested",
    "font-size-requested-default",
    "font-size-requested-supported",
    "force-front-side",
    "force-front-side-default",
    "force-front-side-supported",
    "imposition-template",
    "imposition-template-default",
    "imposition-template-supported",
    "insert-after-page-number-supported",
    "insert-count-supported",
    "insert-sheet",
    "insert-sheet-default",
    "insert-sheet-supported",
    "material-amount-units-supported",	/* IPP 3D */
    "material-diameter-supported",	/* IPP 3D */
    "material-purpose-supported",	/* IPP 3D */
    "material-rate-supported",		/* IPP 3D */
    "material-rate-units-supported",	/* IPP 3D */
    "material-shell-thickness-supported",/* IPP 3D */
    "material-temperature-supported",	/* IPP 3D */
    "material-type-supported",		/* IPP 3D */
    "materials-col",			/* IPP 3D */
    "materials-col-database",		/* IPP 3D */
    "materials-col-default",		/* IPP 3D */
    "materials-col-ready",		/* IPP 3D */
    "materials-col-supported",		/* IPP 3D */
    "max-materials-col-supported",	/* IPP 3D */
    "max-stitching-locations-supported",
    "media",
    "media-back-coating-supported",
    "media-bottom-margin-supported",
    "media-col",
    "media-col-default",
    "media-col-supported",
    "media-color-supported",
    "media-default",
    "media-front-coating-supported",
    "media-grain-supported",
    "media-hole-count-supported",
    "media-info-supported",
    "media-input-tray-check",
    "media-input-tray-check-default",
    "media-input-tray-check-supported",
    "media-key-supported",
    "media-left-margin-supported",
    "media-order-count-supported",
    "media-pre-printed-supported",
    "media-recycled-supported",
    "media-right-margin-supported",
    "media-size-supported",
    "media-source-supported",
    "media-supported",
    "media-thickness-supported",
    "media-top-margin-supported",
    "media-type-supported",
    "media-weight-metric-supported",
    "multiple-document-handling",
    "multiple-document-handling-default",
    "multiple-document-handling-supported",
    "multiple-object-handling",		/* IPP 3D */
    "multiple-object-handling-default",	/* IPP 3D */
    "multiple-object-handling-supported",/* IPP 3D */
    "number-up",
    "number-up-default",
    "number-up-supported",
    "orientation-requested",
    "orientation-requested-default",
    "orientation-requested-supported",
    "output-mode",			/* CUPS extension */
    "output-mode-default",		/* CUPS extension */
    "output-mode-supported",		/* CUPS extension */
    "overrides",
    "overrides-supported",
    "page-delivery",
    "page-delivery-default",
    "page-delivery-supported",
    "page-order-received",
    "page-order-received-default",
    "page-order-received-supported",
    "page-ranges",
    "page-ranges-supported",
    "pages-per-subset",
    "pages-per-subset-supported",
    "pdl-init-file",
    "pdl-init-file-default",
    "pdl-init-file-entry-supported",
    "pdl-init-file-location-supported",
    "pdl-init-file-name-subdirectory-supported",
    "pdl-init-file-name-supported",
    "pdl-init-file-supported",
    "platform-temperature",		/* IPP 3D */
    "platform-temperature-default",	/* IPP 3D */
    "platform-temperature-supported",	/* IPP 3D */
    "presentation-direction-number-up",
    "presentation-direction-number-up-default",
    "presentation-direction-number-up-supported",
    "print-accuracy",			/* IPP 3D */
    "print-accuracy-default",		/* IPP 3D */
    "print-accuracy-supported",		/* IPP 3D */
    "print-base",			/* IPP 3D */
    "print-base-default",		/* IPP 3D */
    "print-base-supported",		/* IPP 3D */
    "print-color-mode",
    "print-color-mode-default",
    "print-color-mode-supported",
    "print-content-optimize",
    "print-content-optimize-default",
    "print-content-optimize-supported",
    "print-objects",			/* IPP 3D */
    "print-objects-default",		/* IPP 3D */
    "print-objects-supported",		/* IPP 3D */
    "print-quality",
    "print-quality-default",
    "print-quality-supported",
    "print-rendering-intent",
    "print-rendering-intent-default",
    "print-rendering-intent-supported",
    "print-scaling",			/* IPP Paid Printing */
    "print-scaling-default",		/* IPP Paid Printing */
    "print-scaling-supported",		/* IPP Paid Printing */
    "print-supports",			/* IPP 3D */
    "print-supports-default",		/* IPP 3D */
    "print-supports-supported",		/* IPP 3D */
    "printer-resolution",
    "printer-resolution-default",
    "printer-resolution-supported",
    "separator-sheets",
    "separator-sheets-default",
    "separator-sheets-supported",
    "sheet-collate",
    "sheet-collate-default",
    "sheet-collate-supported",
    "sides",
    "sides-default",
    "sides-supported",
    "stitching-locations-supported",
    "stitching-offset-supported",
    "x-image-position",
    "x-image-position-default",
    "x-image-position-supported",
    "x-image-shift",
    "x-image-shift-default",
    "x-image-shift-supported",
    "x-side1-image-shift",
    "x-side1-image-shift-default",
    "x-side1-image-shift-supported",
    "x-side2-image-shift",
    "x-side2-image-shift-default",
    "x-side2-image-shift-supported",
    "y-image-position",
    "y-image-position-default",
    "y-image-position-supported",
    "y-image-shift",
    "y-image-shift-default",
    "y-image-shift-supported",
    "y-side1-image-shift",
    "y-side1-image-shift-default",
    "y-side1-image-shift-supported",
    "y-side2-image-shift",
    "y-side2-image-shift-default",
    "y-side2-image-shift-supported"
  };
  static const char * const job_description[] =
  {					/* job-description group */
    "compression-supplied",
    "copies-actual",
    "cover-back-actual",
    "cover-front-actual",
    "current-page-order",
    "date-time-at-completed",
    "date-time-at-creation",
    "date-time-at-processing",
    "destination-statuses",
    "document-charset-supplied",
    "document-digital-signature-supplied",
    "document-format-details-supplied",
    "document-format-supplied",
    "document-message-supplied",
    "document-metadata",
    "document-name-supplied",
    "document-natural-language-supplied",
    "document-overrides-actual",
    "errors-count",
    "finishings-actual",
    "finishings-col-actual",
    "force-front-side-actual",
    "imposition-template-actual",
    "impressions-completed-current-copy",
    "insert-sheet-actual",
    "job-account-id-actual",
    "job-accounting-sheets-actual",
    "job-accounting-user-id-actual",
    "job-attribute-fidelity",
    "job-charge-info",			/* CUPS extension */
    "job-collation-type",
    "job-collation-type-actual",
    "job-copies-actual",
    "job-cover-back-actual",
    "job-cover-front-actual",
    "job-detailed-status-message",
    "job-document-access-errors",
    "job-error-sheet-actual",
    "job-finishings-actual",
    "job-finishings-col-actual",
    "job-hold-until-actual",
    "job-id",
    "job-impressions",
    "job-impressions-completed",
    "job-k-octets",
    "job-k-octets-processed",
    "job-mandatory-attributes",
    "job-media-progress",		/* CUPS extension */
    "job-media-sheets",
    "job-media-sheets-completed",
    "job-message-from-operator",
    "job-more-info",
    "job-name",
    "job-originating-host-name",	/* CUPS extension */
    "job-originating-user-name",
    "job-originating-user-uri",
    "job-pages",
    "job-pages-completed",
    "job-pages-completed-current-copy",
    "job-printer-state-message",	/* CUPS extension */
    "job-printer-state-reasons",	/* CUPS extension */
    "job-printer-up-time",
    "job-printer-uri",
    "job-priority-actual",
    "job-save-printer-make-and-model",
    "job-sheet-message-actual",
    "job-sheets-actual",
    "job-sheets-col-actual",
    "job-state",
    "job-state-message",
    "job-state-reasons",
    "job-uri",
    "job-uuid",
    "materials-col-actual",		/* IPP 3D */
    "media-actual",
    "media-col-actual",
    "media-check-input-tray-actual",
    "multiple-document-handling-actual",
    "multiple-object-handling-actual",	/* IPP 3D */
    "number-of-documents",
    "number-of-intervening-jobs",
    "number-up-actual",
    "orientation-requested-actual",
    "original-requesting-user-name",
    "output-bin-actual",
    "output-device-assigned",
    "overrides-actual",
    "page-delivery-actual",
    "page-order-received-actual",
    "page-ranges-actual",
    "platform-temperature-actual",	/* IPP 3D */
    "presentation-direction-number-up-actual",
    "print-accuracy-actual",		/* IPP 3D */
    "print-base-actual",		/* IPP 3D */
    "print-color-mode-actual",
    "print-content-optimize-actual",
    "print-objects-actual",		/* IPP 3D */
    "print-quality-actual",
    "print-rendering-intent-actual",
    "print-scaling-actual",		/* IPP Paid Printing */
    "print-supports-actual",		/* IPP 3D */
    "printer-resolution-actual",
    "separator-sheets-actual",
    "sheet-collate-actual",
    "sheet-completed-copy-number",
    "sheet-completed-document-number",
    "sides-actual",
    "time-at-completed",
    "time-at-creation",
    "time-at-processing",
    "warnings-count",
    "x-image-position-actual",
    "x-image-shift-actual",
    "x-side1-image-shift-actual",
    "x-side2-image-shift-actual",
    "y-image-position-actual",
    "y-image-shift-actual",
    "y-side1-image-shift-actual",
    "y-side2-image-shift-actual"
  };
  static const char * const job_template[] =
  {					/* job-template group */
    "accuracy-units-supported",		/* IPP 3D */
    "confirmation-sheet-print",		/* IPP FaxOut */
    "confirmation-sheet-print-default",
    "copies",
    "copies-default",
    "copies-supported",
    "cover-back",
    "cover-back-default",
    "cover-back-supported",
    "cover-front",
    "cover-front-default",
    "cover-front-supported",
    "cover-sheet-info",			/* IPP FaxOut */
    "cover-sheet-info-default",
    "cover-sheet-info-supported",
    "destination-uri-schemes-supported",/* IPP FaxOut */
    "destination-uris",			/* IPP FaxOut */
    "destination-uris-supported",
    "feed-orientation",
    "feed-orientation-default",
    "feed-orientation-supported",
    "finishings",
    "finishings-col",
    "finishings-col-default",
    "finishings-col-supported",
    "finishings-default",
    "finishings-supported",
    "font-name-requested",
    "font-name-requested-default",
    "font-name-requested-supported",
    "font-size-requested",
    "font-size-requested-default",
    "font-size-requested-supported",
    "force-front-side",
    "force-front-side-default",
    "force-front-side-supported",
    "imposition-template",
    "imposition-template-default",
    "imposition-template-supported",
    "insert-after-page-number-supported",
    "insert-count-supported",
    "insert-sheet",
    "insert-sheet-default",
    "insert-sheet-supported",
    "job-account-id",
    "job-account-id-default",
    "job-account-id-supported",
    "job-accounting-sheets"
    "job-accounting-sheets-default"
    "job-accounting-sheets-supported"
    "job-accounting-user-id",
    "job-accounting-user-id-default",
    "job-accounting-user-id-supported",
    "job-copies",
    "job-copies-default",
    "job-copies-supported",
    "job-cover-back",
    "job-cover-back-default",
    "job-cover-back-supported",
    "job-cover-front",
    "job-cover-front-default",
    "job-cover-front-supported",
    "job-delay-output-until",
    "job-delay-output-until-default",
    "job-delay-output-until-supported",
    "job-delay-output-until-time",
    "job-delay-output-until-time-default",
    "job-delay-output-until-time-supported",
    "job-error-action",
    "job-error-action-default",
    "job-error-action-supported",
    "job-error-sheet",
    "job-error-sheet-default",
    "job-error-sheet-supported",
    "job-finishings",
    "job-finishings-col",
    "job-finishings-col-default",
    "job-finishings-col-supported",
    "job-finishings-default",
    "job-finishings-supported",
    "job-hold-until",
    "job-hold-until-default",
    "job-hold-until-supported",
    "job-hold-until-time",
    "job-hold-until-time-default",
    "job-hold-until-time-supported",
    "job-message-to-operator",
    "job-message-to-operator-default",
    "job-message-to-operator-supported",
    "job-phone-number",
    "job-phone-number-default",
    "job-phone-number-supported",
    "job-priority",
    "job-priority-default",
    "job-priority-supported",
    "job-recipient-name",
    "job-recipient-name-default",
    "job-recipient-name-supported",
    "job-save-disposition",
    "job-save-disposition-default",
    "job-save-disposition-supported",
    "job-sheets",
    "job-sheets-col",
    "job-sheets-col-default",
    "job-sheets-col-supported",
    "job-sheets-default",
    "job-sheets-supported",
    "logo-uri-schemes-supported",
    "material-amount-units-supported",	/* IPP 3D */
    "material-diameter-supported",	/* IPP 3D */
    "material-purpose-supported",	/* IPP 3D */
    "material-rate-supported",		/* IPP 3D */
    "material-rate-units-supported",	/* IPP 3D */
    "material-shell-thickness-supported",/* IPP 3D */
    "material-temperature-supported",	/* IPP 3D */
    "material-type-supported",		/* IPP 3D */
    "materials-col",			/* IPP 3D */
    "materials-col-database",		/* IPP 3D */
    "materials-col-default",		/* IPP 3D */
    "materials-col-ready",		/* IPP 3D */
    "materials-col-supported",		/* IPP 3D */
    "max-materials-col-supported",	/* IPP 3D */
    "max-save-info-supported",
    "max-stitching-locations-supported",
    "media",
    "media-back-coating-supported",
    "media-bottom-margin-supported",
    "media-col",
    "media-col-default",
    "media-col-supported",
    "media-color-supported",
    "media-default",
    "media-front-coating-supported",
    "media-grain-supported",
    "media-hole-count-supported",
    "media-info-supported",
    "media-input-tray-check",
    "media-input-tray-check-default",
    "media-input-tray-check-supported",
    "media-key-supported",
    "media-left-margin-supported",
    "media-order-count-supported",
    "media-pre-printed-supported",
    "media-recycled-supported",
    "media-right-margin-supported",
    "media-size-supported",
    "media-source-supported",
    "media-supported",
    "media-thickness-supported",
    "media-top-margin-supported",
    "media-type-supported",
    "media-weight-metric-supported",
    "multiple-document-handling",
    "multiple-document-handling-default",
    "multiple-document-handling-supported",
    "multiple-object-handling",		/* IPP 3D */
    "multiple-object-handling-default",	/* IPP 3D */
    "multiple-object-handling-supported",/* IPP 3D */
    "number-of-retries",		/* IPP FaxOut */
    "number-of-retries-default",
    "number-of-retries-supported",
    "number-up",
    "number-up-default",
    "number-up-supported",
    "orientation-requested",
    "orientation-requested-default",
    "orientation-requested-supported",
    "output-bin",
    "output-bin-default",
    "output-bin-supported",
    "output-device",
    "output-device-default",
    "output-device-supported",
    "output-mode",			/* CUPS extension */
    "output-mode-default",		/* CUPS extension */
    "output-mode-supported",		/* CUPS extension */
    "overrides",
    "overrides-supported",
    "page-delivery",
    "page-delivery-default",
    "page-delivery-supported",
    "page-order-received",
    "page-order-received-default",
    "page-order-received-supported",
    "page-ranges",
    "page-ranges-supported",
    "pages-per-subset",
    "pages-per-subset-supported",
    "pdl-init-file",
    "pdl-init-file-default",
    "pdl-init-file-entry-supported",
    "pdl-init-file-location-supported",
    "pdl-init-file-name-subdirectory-supported",
    "pdl-init-file-name-supported",
    "pdl-init-file-supported",
    "platform-temperature",		/* IPP 3D */
    "platform-temperature-default",	/* IPP 3D */
    "platform-temperature-supported",	/* IPP 3D */
    "presentation-direction-number-up",
    "presentation-direction-number-up-default",
    "presentation-direction-number-up-supported",
    "print-accuracy",			/* IPP 3D */
    "print-accuracy-default",		/* IPP 3D */
    "print-accuracy-supported",		/* IPP 3D */
    "print-base",			/* IPP 3D */
    "print-base-default",		/* IPP 3D */
    "print-base-supported",		/* IPP 3D */
    "print-color-mode",
    "print-color-mode-default",
    "print-color-mode-supported",
    "print-content-optimize",
    "print-content-optimize-default",
    "print-content-optimize-supported",
    "print-objects",			/* IPP 3D */
    "print-objects-default",		/* IPP 3D */
    "print-objects-supported",		/* IPP 3D */
    "print-quality",
    "print-quality-default",
    "print-quality-supported",
    "print-rendering-intent",
    "print-rendering-intent-default",
    "print-rendering-intent-supported",
    "print-scaling",			/* IPP Paid Printing */
    "print-scaling-default",		/* IPP Paid Printing */
    "print-scaling-supported",		/* IPP Paid Printing */
    "print-supports",			/* IPP 3D */
    "print-supports-default",		/* IPP 3D */
    "print-supports-supported",		/* IPP 3D */
    "printer-resolution",
    "printer-resolution-default",
    "printer-resolution-supported",
    "proof-print",
    "proof-print-default",
    "proof-print-supported",
    "retry-interval",			/* IPP FaxOut */
    "retry-interval-default",
    "retry-interval-supported",
    "retry-timeout",			/* IPP FaxOut */
    "retry-timeout-default",
    "retry-timeout-supported",
    "save-disposition-supported",
    "save-document-format-default",
    "save-document-format-supported",
    "save-location-default",
    "save-location-supported",
    "save-name-subdirectory-supported",
    "save-name-supported",
    "separator-sheets",
    "separator-sheets-default",
    "separator-sheets-supported",
    "sheet-collate",
    "sheet-collate-default",
    "sheet-collate-supported",
    "sides",
    "sides-default",
    "sides-supported",
    "stitching-locations-supported",
    "stitching-offset-supported",
    "x-image-position",
    "x-image-position-default",
    "x-image-position-supported",
    "x-image-shift",
    "x-image-shift-default",
    "x-image-shift-supported",
    "x-side1-image-shift",
    "x-side1-image-shift-default",
    "x-side1-image-shift-supported",
    "x-side2-image-shift",
    "x-side2-image-shift-default",
    "x-side2-image-shift-supported",
    "y-image-position",
    "y-image-position-default",
    "y-image-position-supported",
    "y-image-shift",
    "y-image-shift-default",
    "y-image-shift-supported",
    "y-side1-image-shift",
    "y-side1-image-shift-default",
    "y-side1-image-shift-supported",
    "y-side2-image-shift",
    "y-side2-image-shift-default",
    "y-side2-image-shift-supported"
  };
  static const char * const printer_description[] =
  {					/* printer-description group */
    "auth-info-required",		/* CUPS extension */
    "charset-configured",
    "charset-supported",
    "color-supported",
    "compression-supported",
    "device-service-count",
    "device-uri",			/* CUPS extension */
    "device-uuid",
    "document-charset-default",
    "document-charset-supported",
    "document-creation-attributes-supported",
    "document-digital-signature-default",
    "document-digital-signature-supported",
    "document-format-default",
    "document-format-details-default",
    "document-format-details-supported",
    "document-format-supported",
    "document-format-varying-attributes",
    "document-format-version-default",
    "document-format-version-supported",
    "document-natural-language-default",
    "document-natural-language-supported",
    "document-password-supported",
    "generated-natural-language-supported",
    "identify-actions-default",
    "identify-actions-supported",
    "input-source-supported",
    "ipp-features-supported",
    "ipp-versions-supported",
    "ippget-event-life",
    "job-authorization-uri-supported",	/* CUPS extension */
    "job-constraints-supported",
    "job-creation-attributes-supported",
    "job-finishings-col-ready",
    "job-finishings-ready",
    "job-ids-supported",
    "job-impressions-supported",
    "job-k-limit",			/* CUPS extension */
    "job-k-octets-supported",
    "job-media-sheets-supported",
    "job-page-limit",			/* CUPS extension */
    "job-password-encryption-supported",
    "job-password-supported",
    "job-quota-period",			/* CUPS extension */
    "job-resolvers-supported",
    "job-settable-attributes-supported",
    "job-spooling-supported",
    "jpeg-k-octets-supported",		/* CUPS extension */
    "jpeg-x-dimension-supported",	/* CUPS extension */
    "jpeg-y-dimension-supported",	/* CUPS extension */
    "landscape-orientation-requested-preferred",
					/* CUPS extension */
    "marker-change-time",		/* CUPS extension */
    "marker-colors",			/* CUPS extension */
    "marker-high-levels",		/* CUPS extension */
    "marker-levels",			/* CUPS extension */
    "marker-low-levels",		/* CUPS extension */
    "marker-message",			/* CUPS extension */
    "marker-names",			/* CUPS extension */
    "marker-types",			/* CUPS extension */
    "media-col-ready",
    "media-ready",
    "member-names",			/* CUPS extension */
    "member-uris",			/* CUPS extension */
    "multiple-destination-uris-supported",/* IPP FaxOut */
    "multiple-document-jobs-supported",
    "multiple-operation-time-out",
    "multiple-operation-time-out-action",
    "natural-language-configured",
    "operations-supported",
    "pages-per-minute",
    "pages-per-minute-color",
    "pdf-k-octets-supported",		/* CUPS extension */
    "pdf-features-supported",		/* IPP 3D */
    "pdf-versions-supported",		/* CUPS extension */
    "pdl-override-supported",
    "port-monitor",			/* CUPS extension */
    "port-monitor-supported",		/* CUPS extension */
    "preferred-attributes-supported",
    "printer-alert",
    "printer-alert-description",
    "printer-charge-info",
    "printer-charge-info-uri",
    "printer-commands",			/* CUPS extension */
    "printer-current-time",
    "printer-detailed-status-messages",
    "printer-device-id",
    "printer-dns-sd-name",		/* CUPS extension */
    "printer-driver-installer",
    "printer-fax-log-uri",		/* IPP FaxOut */
    "printer-fax-modem-info",		/* IPP FaxOut */
    "printer-fax-modem-name",		/* IPP FaxOut */
    "printer-fax-modem-number",		/* IPP FaxOut */
    "printer-firmware-name",		/* PWG 5110.1 */
    "printer-firmware-patches",		/* PWG 5110.1 */
    "printer-firmware-string-version",	/* PWG 5110.1 */
    "printer-firmware-version",		/* PWG 5110.1 */
    "printer-geo-location",
    "printer-get-attributes-supported",
    "printer-icc-profiles",
    "printer-icons",
    "printer-id",               	/* CUPS extension */
    "printer-info",
    "printer-input-tray",		/* IPP JPS3 */
    "printer-is-accepting-jobs",
    "printer-is-shared",		/* CUPS extension */
    "printer-is-temporary",		/* CUPS extension */
    "printer-kind",			/* IPP Paid Printing */
    "printer-location",
    "printer-make-and-model",
    "printer-mandatory-job-attributes",
    "printer-message-date-time",
    "printer-message-from-operator",
    "printer-message-time",
    "printer-more-info",
    "printer-more-info-manufacturer",
    "printer-name",
    "printer-native-formats",
    "printer-organization",
    "printer-organizational-unit",
    "printer-output-tray",		/* IPP JPS3 */
    "printer-queue-id",			/* CUPS extension */
    "printer-settable-attributes-supported",
    "printer-state",
    "printer-state-change-date-time",
    "printer-state-change-time",
    "printer-state-message",
    "printer-state-reasons",
    "printer-supply",
    "printer-supply-description",
    "printer-supply-info-uri",
    "printer-type",			/* CUPS extension */
    "printer-up-time",
    "printer-uri-supported",
    "printer-uuid",
    "printer-xri-supported",
    "pwg-raster-document-resolution-supported",
    "pwg-raster-document-sheet-back",
    "pwg-raster-document-type-supported",
    "queued-job-count",
    "reference-uri-schemes-supported",
    "repertoire-supported",
    "requesting-user-name-allowed",	/* CUPS extension */
    "requesting-user-name-denied",	/* CUPS extension */
    "requesting-user-uri-supported",
    "subordinate-printers-supported",
    "urf-supported",			/* CUPS extension */
    "uri-authentication-supported",
    "uri-security-supported",
    "user-defined-value-supported",
    "which-jobs-supported",
    "xri-authentication-supported",
    "xri-security-supported",
    "xri-uri-scheme-supported"
  };
  static const char * const subscription_description[] =
  {					/* subscription-description group */
    "notify-job-id",
    "notify-lease-expiration-time",
    "notify-printer-up-time",
    "notify-printer-uri",
    "notify-sequence-number",
    "notify-subscriber-user-name",
    "notify-subscriber-user-uri",
    "notify-subscription-id",
    "subscriptions-uuid"
  };
  static const char * const subscription_template[] =
  {					/* subscription-template group */
    "notify-attributes",
    "notify-attributes-supported",
    "notify-charset",
    "notify-events",
    "notify-events-default",
    "notify-events-supported",
    "notify-lease-duration",
    "notify-lease-duration-default",
    "notify-lease-duration-supported",
    "notify-max-events-supported",
    "notify-natural-language",
    "notify-pull-method",
    "notify-pull-method-supported",
    "notify-recipient-uri",
    "notify-schemes-supported",
    "notify-time-interval",
    "notify-user-data"
  };


 /*
  * Get the requested-attributes attribute...
  */

  if ((requested = ippFindAttribute(request, "requested-attributes",
                                    IPP_TAG_KEYWORD)) == NULL)
  {
   /*
    * The Get-Jobs operation defaults to "job-id" and "job-uri", all others
    * default to "all"...
    */

    if (ippGetOperation(request) == IPP_OP_GET_JOBS)
    {
      ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
      cupsArrayAdd(ra, "job-id");
      cupsArrayAdd(ra, "job-uri");

      return (ra);
    }
    else
      return (NULL);
  }

 /*
  * If the attribute contains a single "all" keyword, return NULL...
  */

  count = ippGetCount(requested);
  if (count == 1 && !strcmp(ippGetString(requested, 0, NULL), "all"))
    return (NULL);

 /*
  * Create an array using "strcmp" as the comparison function...
  */

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);

  for (i = 0; i < count; i ++)
  {
    added = 0;
    value = ippGetString(requested, i, NULL);

    if (!strcmp(value, "document-description") || !strcmp(value, "all"))
    {
      for (j = 0;
           j < (int)(sizeof(document_description) /
                     sizeof(document_description[0]));
           j ++)
        cupsArrayAdd(ra, (void *)document_description[j]);

      added = 1;
    }

    if (!strcmp(value, "document-template") || !strcmp(value, "all"))
    {
      for (j = 0;
           j < (int)(sizeof(document_template) / sizeof(document_template[0]));
           j ++)
        cupsArrayAdd(ra, (void *)document_template[j]);

      added = 1;
    }

    if (!strcmp(value, "job-description") || !strcmp(value, "all"))
    {
      for (j = 0;
           j < (int)(sizeof(job_description) / sizeof(job_description[0]));
           j ++)
        cupsArrayAdd(ra, (void *)job_description[j]);

      added = 1;
    }

    if (!strcmp(value, "job-template") || !strcmp(value, "all"))
    {
      for (j = 0;
           j < (int)(sizeof(job_template) / sizeof(job_template[0]));
           j ++)
        cupsArrayAdd(ra, (void *)job_template[j]);

      added = 1;
    }

    if (!strcmp(value, "printer-description") || !strcmp(value, "all"))
    {
      for (j = 0;
           j < (int)(sizeof(printer_description) /
                     sizeof(printer_description[0]));
           j ++)
        cupsArrayAdd(ra, (void *)printer_description[j]);

      added = 1;
    }

    if (!strcmp(value, "subscription-description") || !strcmp(value, "all"))
    {
      for (j = 0;
           j < (int)(sizeof(subscription_description) /
                     sizeof(subscription_description[0]));
           j ++)
        cupsArrayAdd(ra, (void *)subscription_description[j]);

      added = 1;
    }

    if (!strcmp(value, "subscription-template") || !strcmp(value, "all"))
    {
      for (j = 0;
           j < (int)(sizeof(subscription_template) /
                     sizeof(subscription_template[0]));
           j ++)
        cupsArrayAdd(ra, (void *)subscription_template[j]);

      added = 1;
    }

    if (!added)
      cupsArrayAdd(ra, (void *)value);
  }

  return (ra);
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
      enumvalue < (3 + (int)(sizeof(ipp_document_states) /
			     sizeof(ipp_document_states[0]))))
    return (ipp_document_states[enumvalue - 3]);
  else if (!strcmp(attrname, "finishings") ||
	   !strcmp(attrname, "finishings-actual") ||
	   !strcmp(attrname, "finishings-default") ||
	   !strcmp(attrname, "finishings-ready") ||
	   !strcmp(attrname, "finishings-supported") ||
	   !strcmp(attrname, "job-finishings") ||
	   !strcmp(attrname, "job-finishings-default") ||
	   !strcmp(attrname, "job-finishings-supported"))
  {
    if (enumvalue >= 3 &&
        enumvalue < (3 + (int)(sizeof(ipp_finishings) /
			       sizeof(ipp_finishings[0]))))
      return (ipp_finishings[enumvalue - 3]);
    else if (enumvalue >= 0x40000000 &&
             enumvalue <= (0x40000000 + (int)(sizeof(ipp_finishings_vendor) /
                                              sizeof(ipp_finishings_vendor[0]))))
      return (ipp_finishings_vendor[enumvalue - 0x40000000]);
  }
  else if ((!strcmp(attrname, "job-collation-type") ||
            !strcmp(attrname, "job-collation-type-actual")) &&
           enumvalue >= 3 &&
           enumvalue < (3 + (int)(sizeof(ipp_job_collation_types) /
				  sizeof(ipp_job_collation_types[0]))))
    return (ipp_job_collation_types[enumvalue - 3]);
  else if (!strcmp(attrname, "job-state") &&
	   enumvalue >= IPP_JSTATE_PENDING && enumvalue <= IPP_JSTATE_COMPLETED)
    return (ipp_job_states[enumvalue - IPP_JSTATE_PENDING]);
  else if (!strcmp(attrname, "operations-supported"))
    return (ippOpString((ipp_op_t)enumvalue));
  else if ((!strcmp(attrname, "orientation-requested") ||
            !strcmp(attrname, "orientation-requested-actual") ||
            !strcmp(attrname, "orientation-requested-default") ||
            !strcmp(attrname, "orientation-requested-supported")) &&
           enumvalue >= 3 &&
           enumvalue < (3 + (int)(sizeof(ipp_orientation_requesteds) /
				  sizeof(ipp_orientation_requesteds[0]))))
    return (ipp_orientation_requesteds[enumvalue - 3]);
  else if ((!strcmp(attrname, "print-quality") ||
            !strcmp(attrname, "print-quality-actual") ||
            !strcmp(attrname, "print-quality-default") ||
            !strcmp(attrname, "print-quality-supported")) &&
           enumvalue >= 3 &&
           enumvalue < (3 + (int)(sizeof(ipp_print_qualities) /
				  sizeof(ipp_print_qualities[0]))))
    return (ipp_print_qualities[enumvalue - 3]);
  else if (!strcmp(attrname, "printer-state") &&
           enumvalue >= IPP_PSTATE_IDLE && enumvalue <= IPP_PSTATE_STOPPED)
    return (ipp_printer_states[enumvalue - IPP_PSTATE_IDLE]);

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
    return ((int)strtol(enumstring, NULL, 0));

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
    for (i = 0;
         i < (int)(sizeof(ipp_finishings_vendor) /
                   sizeof(ipp_finishings_vendor[0]));
         i ++)
      if (!strcmp(enumstring, ipp_finishings_vendor[i]))
	return (i + 0x40000000);

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

  if (error >= IPP_STATUS_OK && error <= IPP_STATUS_OK_EVENTS_COMPLETE)
    return (ipp_status_oks[error]);
  else if (error == IPP_STATUS_REDIRECTION_OTHER_SITE)
    return ("redirection-other-site");
  else if (error == IPP_STATUS_CUPS_SEE_OTHER)
    return ("cups-see-other");
  else if (error >= IPP_STATUS_ERROR_BAD_REQUEST &&
           error <= IPP_STATUS_ERROR_ACCOUNT_AUTHORIZATION_FAILED)
    return (ipp_status_400s[error - IPP_STATUS_ERROR_BAD_REQUEST]);
  else if (error >= 0x480 &&
           error <= IPP_STATUS_ERROR_CUPS_ACCOUNT_AUTHORIZATION_FAILED)
    return (ipp_status_480s[error - 0x0480]);
  else if (error >= IPP_STATUS_ERROR_INTERNAL &&
           error <= IPP_STATUS_ERROR_TOO_MANY_DOCUMENTS)
    return (ipp_status_500s[error - IPP_STATUS_ERROR_INTERNAL]);
  else if (error >= IPP_STATUS_ERROR_CUPS_AUTHENTICATION_CANCELED &&
           error <= IPP_STATUS_ERROR_CUPS_UPGRADE_REQUIRED)
    return (ipp_status_1000s[error -
                             IPP_STATUS_ERROR_CUPS_AUTHENTICATION_CANCELED]);

 /*
  * No, build an "0xxxxx" error string...
  */

  sprintf(cg->ipp_unknown, "0x%04x", error);

  return (cg->ipp_unknown);
}


/*
 * 'ippErrorValue()' - Return a status code for the given name.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

ipp_status_t				/* O - IPP status code */
ippErrorValue(const char *name)		/* I - Name */
{
  size_t	i;			/* Looping var */


  for (i = 0; i < (sizeof(ipp_status_oks) / sizeof(ipp_status_oks[0])); i ++)
    if (!_cups_strcasecmp(name, ipp_status_oks[i]))
      return ((ipp_status_t)i);

  if (!_cups_strcasecmp(name, "redirection-other-site"))
    return (IPP_STATUS_REDIRECTION_OTHER_SITE);

  if (!_cups_strcasecmp(name, "cups-see-other"))
    return (IPP_STATUS_CUPS_SEE_OTHER);

  for (i = 0; i < (sizeof(ipp_status_400s) / sizeof(ipp_status_400s[0])); i ++)
    if (!_cups_strcasecmp(name, ipp_status_400s[i]))
      return ((ipp_status_t)(i + 0x400));

  for (i = 0; i < (sizeof(ipp_status_480s) / sizeof(ipp_status_480s[0])); i ++)
    if (!_cups_strcasecmp(name, ipp_status_480s[i]))
      return ((ipp_status_t)(i + 0x480));

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
 * @since CUPS 1.2/macOS 10.5@
 */

const char *				/* O - Name */
ippOpString(ipp_op_t op)		/* I - Operation ID */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * See if the operation ID is a known value...
  */

  if (op >= IPP_OP_PRINT_JOB && op < (ipp_op_t)(sizeof(ipp_std_ops) / sizeof(ipp_std_ops[0])))
    return (ipp_std_ops[op]);
  else if (op == IPP_OP_PRIVATE)
    return ("windows-ext");
  else if (op >= IPP_OP_CUPS_GET_DEFAULT && op <= IPP_OP_CUPS_GET_PPD)
    return (ipp_cups_ops[op - IPP_OP_CUPS_GET_DEFAULT]);
  else if (op >= IPP_OP_CUPS_GET_DOCUMENT && op <= IPP_OP_CUPS_CREATE_LOCAL_PRINTER)
    return (ipp_cups_ops2[op - IPP_OP_CUPS_GET_DOCUMENT]);

 /*
  * No, build an "0xxxxx" operation string...
  */

  sprintf(cg->ipp_unknown, "0x%04x", op);

  return (cg->ipp_unknown);
}


/*
 * 'ippOpValue()' - Return an operation id for the given name.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

ipp_op_t				/* O - Operation ID */
ippOpValue(const char *name)		/* I - Textual name */
{
  size_t	i;			/* Looping var */


  if (!strncmp(name, "0x", 2))
    return ((ipp_op_t)strtol(name + 2, NULL, 16));

  for (i = 0; i < (sizeof(ipp_std_ops) / sizeof(ipp_std_ops[0])); i ++)
    if (!_cups_strcasecmp(name, ipp_std_ops[i]))
      return ((ipp_op_t)i);

  if (!_cups_strcasecmp(name, "windows-ext"))
    return (IPP_OP_PRIVATE);

  for (i = 0; i < (sizeof(ipp_cups_ops) / sizeof(ipp_cups_ops[0])); i ++)
    if (!_cups_strcasecmp(name, ipp_cups_ops[i]))
      return ((ipp_op_t)(i + 0x4001));

  for (i = 0; i < (sizeof(ipp_cups_ops2) / sizeof(ipp_cups_ops2[0])); i ++)
    if (!_cups_strcasecmp(name, ipp_cups_ops2[i]))
      return ((ipp_op_t)(i + 0x4027));

  if (!_cups_strcasecmp(name, "Create-Job-Subscription"))
    return (IPP_OP_CREATE_JOB_SUBSCRIPTIONS);

  if (!_cups_strcasecmp(name, "Create-Printer-Subscription"))
    return (IPP_OP_CREATE_PRINTER_SUBSCRIPTIONS);

  if (!_cups_strcasecmp(name, "CUPS-Add-Class"))
    return (IPP_OP_CUPS_ADD_MODIFY_CLASS);

  if (!_cups_strcasecmp(name, "CUPS-Add-Printer"))
    return (IPP_OP_CUPS_ADD_MODIFY_PRINTER);

  return (IPP_OP_CUPS_INVALID);
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
 * 'ippStateString()' - Return the name corresponding to a state value.
 *
 * @since CUPS 2.0/OS 10.10@
 */

const char *				/* O - State name */
ippStateString(ipp_state_t state)	/* I - State value */
{
  if (state >= IPP_STATE_ERROR && state <= IPP_STATE_DATA)
    return (ipp_states[state - IPP_STATE_ERROR]);
  else
    return ("UNKNOWN");
}


/*
 * 'ippTagString()' - Return the tag name corresponding to a tag value.
 *
 * The returned names are defined in RFC 2911 and 3382.
 *
 * @since CUPS 1.4/macOS 10.6@
 */

const char *				/* O - Tag name */
ippTagString(ipp_tag_t tag)		/* I - Tag value */
{
  tag &= IPP_TAG_CUPS_MASK;

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
 * @since CUPS 1.4/macOS 10.6@
 */

ipp_tag_t				/* O - Tag value */
ippTagValue(const char *name)		/* I - Tag name */
{
  size_t	i;			/* Looping var */


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


  if (!col)
  {
    if (buffer)
      *buffer = '\0';

    return (0);
  }

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
      bufptr += snprintf(bufptr, (size_t)(bufend - bufptr + 1), "%s=", attr->name);
    else
      bufptr += strlen(attr->name) + 1;

    if (buffer && bufptr < bufend)
      bufptr += ippAttributeString(attr, bufptr, (size_t)(bufend - bufptr + 1));
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

  return ((size_t)(bufptr - buffer));
}
