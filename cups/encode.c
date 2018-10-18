/*
 * Option encoding routines for CUPS.
 *
 * Copyright © 2007-2018 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include "debug-internal.h"


/*
 * Local list of option names, the value tags they should use, and the list of
 * supported operations...
 *
 * **** THIS LIST MUST BE SORTED BY ATTRIBUTE NAME ****
 */

static const ipp_op_t ipp_job_creation[] =
{
  IPP_OP_PRINT_JOB,
  IPP_OP_PRINT_URI,
  IPP_OP_VALIDATE_JOB,
  IPP_OP_CREATE_JOB,
  IPP_OP_HOLD_JOB,
  IPP_OP_SET_JOB_ATTRIBUTES,
  IPP_OP_CUPS_NONE
};

static const ipp_op_t ipp_doc_creation[] =
{
  IPP_OP_PRINT_JOB,
  IPP_OP_PRINT_URI,
  IPP_OP_SEND_DOCUMENT,
  IPP_OP_SEND_URI,
  IPP_OP_SET_JOB_ATTRIBUTES,
  IPP_OP_SET_DOCUMENT_ATTRIBUTES,
  IPP_OP_CUPS_NONE
};

static const ipp_op_t ipp_sub_creation[] =
{
  IPP_OP_PRINT_JOB,
  IPP_OP_PRINT_URI,
  IPP_OP_CREATE_JOB,
  IPP_OP_CREATE_PRINTER_SUBSCRIPTIONS,
  IPP_OP_CREATE_JOB_SUBSCRIPTIONS,
  IPP_OP_CUPS_NONE
};

static const ipp_op_t ipp_all_print[] =
{
  IPP_OP_PRINT_JOB,
  IPP_OP_PRINT_URI,
  IPP_OP_VALIDATE_JOB,
  IPP_OP_CREATE_JOB,
  IPP_OP_SEND_DOCUMENT,
  IPP_OP_SEND_URI,
  IPP_OP_CUPS_NONE
};

static const ipp_op_t ipp_set_printer[] =
{
  IPP_OP_SET_PRINTER_ATTRIBUTES,
  IPP_OP_CUPS_ADD_MODIFY_PRINTER,
  IPP_OP_CUPS_ADD_MODIFY_CLASS,
  IPP_OP_CUPS_NONE
};

static const ipp_op_t cups_schemes[] =
{
  IPP_OP_CUPS_GET_DEVICES,
  IPP_OP_CUPS_GET_PPDS,
  IPP_OP_CUPS_NONE
};

static const ipp_op_t cups_get_ppds[] =
{
  IPP_OP_CUPS_GET_PPDS,
  IPP_OP_CUPS_NONE
};

static const ipp_op_t cups_ppd_name[] =
{
  IPP_OP_CUPS_ADD_MODIFY_PRINTER,
  IPP_OP_CUPS_GET_PPD,
  IPP_OP_CUPS_NONE
};

static const _ipp_option_t ipp_options[] =
{
  { 1, "auth-info",		IPP_TAG_TEXT,		IPP_TAG_JOB },
  { 1, "auth-info-default",	IPP_TAG_TEXT,		IPP_TAG_PRINTER },
  { 1, "auth-info-required",	IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 0, "blackplot",		IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { 0, "blackplot-default",	IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { 0, "brightness",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "brightness-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "columns",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "columns-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "compression",		IPP_TAG_KEYWORD,	IPP_TAG_OPERATION,
							IPP_TAG_ZERO,
							ipp_doc_creation },
  { 0, "copies",		IPP_TAG_INTEGER,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "copies-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "date-time-at-completed",IPP_TAG_DATE,		IPP_TAG_ZERO }, /* never send as option */
  { 0, "date-time-at-creation",	IPP_TAG_DATE,		IPP_TAG_ZERO }, /* never send as option */
  { 0, "date-time-at-processing",IPP_TAG_DATE,		IPP_TAG_ZERO }, /* never send as option */
  { 0, "device-uri",		IPP_TAG_URI,		IPP_TAG_PRINTER },
  { 1, "document-copies",	IPP_TAG_RANGE,		IPP_TAG_JOB,
							IPP_TAG_DOCUMENT,
							ipp_doc_creation },
  { 0, "document-format",	IPP_TAG_MIMETYPE,	IPP_TAG_OPERATION,
							IPP_TAG_ZERO,
							ipp_doc_creation },
  { 0, "document-format-default", IPP_TAG_MIMETYPE,	IPP_TAG_PRINTER },
  { 1, "document-numbers",	IPP_TAG_RANGE,		IPP_TAG_JOB,
							IPP_TAG_DOCUMENT,
							ipp_all_print },
  { 1, "exclude-schemes",	IPP_TAG_NAME,		IPP_TAG_OPERATION,
							IPP_TAG_ZERO,
							cups_schemes },
  { 1, "finishings",		IPP_TAG_ENUM,		IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 1, "finishings-col",	IPP_TAG_BEGIN_COLLECTION, IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 1, "finishings-col-default", IPP_TAG_BEGIN_COLLECTION, IPP_TAG_PRINTER },
  { 1, "finishings-default",	IPP_TAG_ENUM,		IPP_TAG_PRINTER },
  { 0, "fit-to-page",		IPP_TAG_BOOLEAN,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "fit-to-page-default",	IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { 0, "fitplot",		IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { 0, "fitplot-default",	IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { 0, "gamma",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "gamma-default",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "hue",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "hue-default",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 1, "include-schemes",	IPP_TAG_NAME,		IPP_TAG_OPERATION,
							IPP_TAG_ZERO,
							cups_schemes },
  { 0, "ipp-attribute-fidelity", IPP_TAG_BOOLEAN,	IPP_TAG_OPERATION },
  { 0, "job-account-id",        IPP_TAG_NAME,           IPP_TAG_JOB },
  { 0, "job-account-id-default",IPP_TAG_NAME,           IPP_TAG_PRINTER },
  { 0, "job-accounting-user-id", IPP_TAG_NAME,          IPP_TAG_JOB },
  { 0, "job-accounting-user-id-default", IPP_TAG_NAME,  IPP_TAG_PRINTER },
  { 0, "job-authorization-uri",	IPP_TAG_URI,		IPP_TAG_OPERATION },
  { 0, "job-cancel-after",	IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "job-cancel-after-default", IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "job-hold-until",	IPP_TAG_KEYWORD,	IPP_TAG_JOB },
  { 0, "job-hold-until-default", IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 0, "job-id",		IPP_TAG_INTEGER,	IPP_TAG_ZERO }, /* never send as option */
  { 0, "job-impressions",	IPP_TAG_INTEGER,	IPP_TAG_OPERATION },
  { 0, "job-impressions-completed", IPP_TAG_INTEGER,	IPP_TAG_ZERO }, /* never send as option */
  { 0, "job-k-limit",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "job-k-octets",		IPP_TAG_INTEGER,	IPP_TAG_OPERATION },
  { 0, "job-k-octets-completed",IPP_TAG_INTEGER,	IPP_TAG_ZERO }, /* never send as option */
  { 0, "job-media-sheets",	IPP_TAG_INTEGER,	IPP_TAG_OPERATION },
  { 0, "job-media-sheets-completed", IPP_TAG_INTEGER,	IPP_TAG_ZERO }, /* never send as option */
  { 0, "job-name",		IPP_TAG_NAME,		IPP_TAG_OPERATION,
							IPP_TAG_JOB },
  { 0, "job-page-limit",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "job-pages",		IPP_TAG_INTEGER,	IPP_TAG_OPERATION },
  { 0, "job-pages-completed",	IPP_TAG_INTEGER,	IPP_TAG_ZERO }, /* never send as option */
  { 0, "job-password",          IPP_TAG_STRING,         IPP_TAG_OPERATION,
							IPP_TAG_ZERO,
							ipp_job_creation },
  { 0, "job-password-encryption", IPP_TAG_KEYWORD,      IPP_TAG_OPERATION,
							IPP_TAG_ZERO,
							ipp_job_creation },
  { 0, "job-priority",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "job-priority-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "job-quota-period",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 1, "job-sheets",		IPP_TAG_NAME,		IPP_TAG_JOB },
  { 1, "job-sheets-default",	IPP_TAG_NAME,		IPP_TAG_PRINTER },
  { 0, "job-state",		IPP_TAG_ENUM,		IPP_TAG_ZERO }, /* never send as option */
  { 0, "job-state-message",	IPP_TAG_TEXT,		IPP_TAG_ZERO }, /* never send as option */
  { 0, "job-state-reasons",	IPP_TAG_KEYWORD,	IPP_TAG_ZERO }, /* never send as option */
  { 0, "job-uuid",		IPP_TAG_URI,		IPP_TAG_JOB },
  { 0, "landscape",		IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { 1, "marker-change-time",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 1, "marker-colors",		IPP_TAG_NAME,		IPP_TAG_PRINTER },
  { 1, "marker-high-levels",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 1, "marker-levels",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 1, "marker-low-levels",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "marker-message",	IPP_TAG_TEXT,		IPP_TAG_PRINTER },
  { 1, "marker-names",		IPP_TAG_NAME,		IPP_TAG_PRINTER },
  { 1, "marker-types",		IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 1, "media",			IPP_TAG_KEYWORD,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "media-bottom-margin",	IPP_TAG_INTEGER,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "media-col",		IPP_TAG_BEGIN_COLLECTION, IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "media-col-default",	IPP_TAG_BEGIN_COLLECTION, IPP_TAG_PRINTER },
  { 0, "media-color",		IPP_TAG_KEYWORD,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 1, "media-default",		IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 0, "media-key",		IPP_TAG_KEYWORD,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "media-left-margin",	IPP_TAG_INTEGER,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "media-right-margin",	IPP_TAG_INTEGER,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "media-size",		IPP_TAG_BEGIN_COLLECTION, IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "media-size-name",	IPP_TAG_KEYWORD,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "media-source",		IPP_TAG_KEYWORD,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "media-top-margin",	IPP_TAG_INTEGER,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "media-type",		IPP_TAG_KEYWORD,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "mirror",		IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { 0, "mirror-default",	IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { 0, "multiple-document-handling", IPP_TAG_KEYWORD,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "multiple-document-handling-default", IPP_TAG_KEYWORD, IPP_TAG_PRINTER },
  { 0, "natural-scaling",	IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "natural-scaling-default", IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "notify-charset",	IPP_TAG_CHARSET,	IPP_TAG_SUBSCRIPTION },
  { 1, "notify-events",		IPP_TAG_KEYWORD,	IPP_TAG_SUBSCRIPTION },
  { 1, "notify-events-default",	IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 0, "notify-lease-duration",	IPP_TAG_INTEGER,	IPP_TAG_SUBSCRIPTION },
  { 0, "notify-lease-duration-default", IPP_TAG_INTEGER, IPP_TAG_PRINTER },
  { 0, "notify-natural-language", IPP_TAG_LANGUAGE,	IPP_TAG_SUBSCRIPTION },
  { 0, "notify-pull-method",	IPP_TAG_KEYWORD,	IPP_TAG_SUBSCRIPTION },
  { 0, "notify-recipient-uri",	IPP_TAG_URI,		IPP_TAG_SUBSCRIPTION },
  { 0, "notify-time-interval",	IPP_TAG_INTEGER,	IPP_TAG_SUBSCRIPTION },
  { 0, "notify-user-data",	IPP_TAG_STRING,		IPP_TAG_SUBSCRIPTION },
  { 0, "number-up",		IPP_TAG_INTEGER,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "number-up-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "number-up-layout",	IPP_TAG_KEYWORD,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "number-up-layout-default", IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 0, "orientation-requested",	IPP_TAG_ENUM,		IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "orientation-requested-default", IPP_TAG_ENUM,	IPP_TAG_PRINTER },
  { 0, "output-bin",		IPP_TAG_KEYWORD,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "output-bin-default",	IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 1, "overrides",		IPP_TAG_BEGIN_COLLECTION, IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "page-bottom",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "page-bottom-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "page-delivery",		IPP_TAG_KEYWORD,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "page-delivery-default",	IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 0, "page-left",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "page-left-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 1, "page-ranges",		IPP_TAG_RANGE,		IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "page-right",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "page-right-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "page-top",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "page-top-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 1, "pages",			IPP_TAG_RANGE,		IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "penwidth",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "penwidth-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "port-monitor",		IPP_TAG_NAME,		IPP_TAG_PRINTER },
  { 0, "ppd-device-id",		IPP_TAG_TEXT,		IPP_TAG_OPERATION,
							IPP_TAG_ZERO,
							cups_get_ppds },
  { 0, "ppd-make",		IPP_TAG_TEXT,		IPP_TAG_OPERATION,
							IPP_TAG_ZERO,
							cups_get_ppds },
  { 0, "ppd-make-and-model",	IPP_TAG_TEXT,		IPP_TAG_OPERATION,
							IPP_TAG_ZERO,
							cups_get_ppds },
  { 0, "ppd-model-number",	IPP_TAG_INTEGER,	IPP_TAG_OPERATION,
							IPP_TAG_ZERO,
							cups_get_ppds },
  { 0, "ppd-name",		IPP_TAG_NAME,		IPP_TAG_OPERATION,
							IPP_TAG_ZERO,
							cups_ppd_name },
  { 0, "ppd-natural-language",	IPP_TAG_LANGUAGE,	IPP_TAG_OPERATION,
							IPP_TAG_ZERO,
							cups_get_ppds },
  { 0, "ppd-product",		IPP_TAG_TEXT,		IPP_TAG_OPERATION,
							IPP_TAG_ZERO,
							cups_get_ppds },
  { 0, "ppd-psversion",		IPP_TAG_TEXT,		IPP_TAG_OPERATION,
							IPP_TAG_ZERO,
							cups_get_ppds },
  { 0, "ppd-type",		IPP_TAG_KEYWORD,	IPP_TAG_OPERATION,
							IPP_TAG_ZERO,
							cups_get_ppds },
  { 0, "ppi",			IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "ppi-default",		IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "prettyprint",		IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { 0, "prettyprint-default",	IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { 0, "print-color-mode",	IPP_TAG_KEYWORD,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "print-color-mode-default", IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 0, "print-content-optimize", IPP_TAG_KEYWORD,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "print-content-optimize-default", IPP_TAG_KEYWORD, IPP_TAG_PRINTER },
  { 0, "print-quality",		IPP_TAG_ENUM,		IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "print-quality-default",	IPP_TAG_ENUM,		IPP_TAG_PRINTER },
  { 0, "print-rendering-intent", IPP_TAG_KEYWORD,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "print-rendering-intent-default", IPP_TAG_KEYWORD, IPP_TAG_PRINTER },
  { 0, "print-scaling",		IPP_TAG_KEYWORD,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "print-scaling-default",	IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 1, "printer-alert",		IPP_TAG_STRING,		IPP_TAG_PRINTER },
  { 1, "printer-alert-description", IPP_TAG_TEXT,	IPP_TAG_PRINTER },
  { 1, "printer-commands",	IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 0, "printer-error-policy",	IPP_TAG_NAME,		IPP_TAG_PRINTER },
  { 1, "printer-finisher",	IPP_TAG_STRING,		IPP_TAG_PRINTER },
  { 1, "printer-finisher-description", IPP_TAG_TEXT,	IPP_TAG_PRINTER },
  { 1, "printer-finisher-supplies", IPP_TAG_STRING,	IPP_TAG_PRINTER },
  { 1, "printer-finisher-supplies-description", IPP_TAG_TEXT, IPP_TAG_PRINTER },
  { 0, "printer-geo-location",	IPP_TAG_URI,		IPP_TAG_PRINTER },
  { 0, "printer-info",		IPP_TAG_TEXT,		IPP_TAG_PRINTER },
  { 1, "printer-input-tray",	IPP_TAG_STRING,		IPP_TAG_PRINTER },
  { 0, "printer-is-accepting-jobs", IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { 0, "printer-is-shared",	IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { 0, "printer-is-temporary",	IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { 0, "printer-location",	IPP_TAG_TEXT,		IPP_TAG_PRINTER },
  { 0, "printer-make-and-model", IPP_TAG_TEXT,		IPP_TAG_PRINTER },
  { 0, "printer-more-info",	IPP_TAG_URI,		IPP_TAG_PRINTER },
  { 0, "printer-op-policy",	IPP_TAG_NAME,		IPP_TAG_PRINTER },
  { 1, "printer-output-tray",	IPP_TAG_STRING,		IPP_TAG_PRINTER },
  { 0, "printer-resolution",	IPP_TAG_RESOLUTION,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "printer-resolution-default", IPP_TAG_RESOLUTION, IPP_TAG_PRINTER },
  { 0, "printer-state",		IPP_TAG_ENUM,		IPP_TAG_PRINTER },
  { 0, "printer-state-change-time", IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 1, "printer-state-reasons",	IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 1, "printer-supply",	IPP_TAG_STRING,		IPP_TAG_PRINTER },
  { 1, "printer-supply-description", IPP_TAG_TEXT,	IPP_TAG_PRINTER },
  { 0, "printer-type",		IPP_TAG_ENUM,		IPP_TAG_PRINTER },
  { 0, "printer-uri",		IPP_TAG_URI,		IPP_TAG_OPERATION },
  { 1, "printer-uri-supported",	IPP_TAG_URI,		IPP_TAG_PRINTER },
  { 0, "queued-job-count",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "raw",			IPP_TAG_MIMETYPE,	IPP_TAG_OPERATION },
  { 1, "requested-attributes",	IPP_TAG_NAME,		IPP_TAG_OPERATION },
  { 1, "requesting-user-name-allowed", IPP_TAG_NAME,	IPP_TAG_PRINTER },
  { 1, "requesting-user-name-denied", IPP_TAG_NAME,	IPP_TAG_PRINTER },
  { 0, "resolution",		IPP_TAG_RESOLUTION,	IPP_TAG_JOB },
  { 0, "resolution-default",	IPP_TAG_RESOLUTION,	IPP_TAG_PRINTER },
  { 0, "saturation",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "saturation-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "scaling",		IPP_TAG_INTEGER,	IPP_TAG_JOB },
  { 0, "scaling-default",	IPP_TAG_INTEGER,	IPP_TAG_PRINTER },
  { 0, "sides",			IPP_TAG_KEYWORD,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "sides-default",		IPP_TAG_KEYWORD,	IPP_TAG_PRINTER },
  { 0, "time-at-completed",	IPP_TAG_INTEGER,	IPP_TAG_ZERO }, /* never send as option */
  { 0, "time-at-creation",	IPP_TAG_INTEGER,	IPP_TAG_ZERO }, /* never send as option */
  { 0, "time-at-processing",	IPP_TAG_INTEGER,	IPP_TAG_ZERO }, /* never send as option */
  { 0, "wrap",			IPP_TAG_BOOLEAN,	IPP_TAG_JOB },
  { 0, "wrap-default",		IPP_TAG_BOOLEAN,	IPP_TAG_PRINTER },
  { 0, "x-dimension",		IPP_TAG_INTEGER,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT },
  { 0, "y-dimension",		IPP_TAG_INTEGER,	IPP_TAG_JOB,
							IPP_TAG_DOCUMENT }
};


/*
 * Local functions...
 */

static int	compare_ipp_options(_ipp_option_t *a, _ipp_option_t *b);


/*
 * '_cupsEncodeOption()' - Encode a single option as an IPP attribute.
 */

ipp_attribute_t *			/* O - New attribute or @code NULL@ on error */
_cupsEncodeOption(
    ipp_t         *ipp,			/* I - IPP request/response/collection */
    ipp_tag_t     group_tag,		/* I - Group tag */
    _ipp_option_t *map,			/* I - Option mapping, if any */
    const char    *name,		/* I - Attribute name */
    const char    *value)		/* I - Value */
{
  int			i,		/* Looping var */
			count;		/* Number of values */
  char			*s,		/* Pointer into option value */
			*val,		/* Pointer to option value */
			*copy,		/* Copy of option value */
			*sep,		/* Option separator */
			quote;		/* Quote character */
  ipp_attribute_t	*attr;		/* IPP attribute */
  ipp_tag_t		value_tag;	/* IPP value tag */
  ipp_t			*collection;	/* Collection value */
  int			num_cols;	/* Number of collection values */
  cups_option_t		*cols;		/* Collection values */


  DEBUG_printf(("_cupsEncodeOption(ipp=%p(%s), group=%s, map=%p, name=\"%s\", value=\"%s\")", (void *)ipp, ipp ? ippOpString(ippGetOperation(ipp)) : "", ippTagString(group_tag), (void *)map, name, value));

 /*
  * Figure out the attribute syntax for encoding...
  */

  if (!map)
    map = _ippFindOption(name);

  if (map)
    value_tag = map->value_tag;
  else if (!_cups_strcasecmp(value, "true") || !_cups_strcasecmp(value, "false"))
    value_tag = IPP_TAG_BOOLEAN;
  else if (value[0] == '{')
    value_tag = IPP_TAG_BEGIN_COLLECTION;
  else
    value_tag = IPP_TAG_NAME;

 /*
  * Count the number of values...
  */

  if (map && map->multivalue)
  {
    for (count = 1, sep = (char *)value, quote = 0; *sep; sep ++)
    {
      if (*sep == quote)
	quote = 0;
      else if (!quote && (*sep == '\'' || *sep == '\"'))
      {
       /*
	* Skip quoted option value...
	*/

	quote = *sep;
      }
      else if (*sep == ',' && !quote)
	count ++;
      else if (*sep == '\\' && sep[1])
	sep ++;
    }
  }
  else
    count = 1;

  DEBUG_printf(("2_cupsEncodeOption: value_tag=%s, count=%d", ippTagString(value_tag), count));

 /*
  * Allocate memory for the attribute values...
  */

  if ((attr = ippAddStrings(ipp, group_tag, value_tag, name, count, NULL, NULL)) == NULL)
  {
   /*
    * Ran out of memory!
    */

    DEBUG_puts("1_cupsEncodeOption: Ran out of memory for attributes.");
    return (NULL);
  }

  if (count > 1)
  {
   /*
    * Make a copy of the value we can fiddle with...
    */

    if ((copy = strdup(value)) == NULL)
    {
     /*
      * Ran out of memory!
      */

      DEBUG_puts("1_cupsEncodeOption: Ran out of memory for value copy.");
      ippDeleteAttribute(ipp, attr);
      return (NULL);
    }

    val = copy;
  }
  else
  {
   /*
    * Since we have a single value, use the value directly...
    */

    val  = (char *)value;
    copy = NULL;
  }

 /*
  * Scan the value string for values...
  */

  for (i = 0, sep = val; i < count; val = sep, i ++)
  {
   /*
    * Find the end of this value and mark it if needed...
    */

    if (count > 1)
    {
      for (quote = 0; *sep; sep ++)
      {
	if (*sep == quote)
	{
	 /*
	  * Finish quoted value...
	  */

	  quote = 0;
	}
	else if (!quote && (*sep == '\'' || *sep == '\"'))
	{
	 /*
	  * Handle quoted option value...
	  */

	  quote = *sep;
	}
	else if (*sep == ',' && count > 1)
	  break;
	else if (*sep == '\\' && sep[1])
	{
	 /*
	  * Skip quoted character...
	  */

	  memmove(sep, sep + 1, strlen(sep));
	}
      }

      if (*sep == ',')
	*sep++ = '\0';
    }

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

	  ippSetInteger(ipp, &attr, i, (int)strtol(val, &s, 10));
	  break;

      case IPP_TAG_BOOLEAN :
	  if (!_cups_strcasecmp(val, "true") || !_cups_strcasecmp(val, "on") || !_cups_strcasecmp(val, "yes"))
	  {
	   /*
	    * Boolean value - true...
	    */

            ippSetBoolean(ipp, &attr, i, 1);
	  }
	  else
	  {
	   /*
	    * Boolean value - false...
	    */

            ippSetBoolean(ipp, &attr, i, 0);
	  }
	  break;

      case IPP_TAG_RANGE :
          {
	   /*
	    * Range...
	    */

	    int lower, upper;		/* Lower and upper ranges... */

	    if (*val == '-')
	    {
	      lower = 1;
	      s     = val;
	    }
	    else
	      lower = (int)strtol(val, &s, 10);

	    if (*s == '-')
	    {
	      if (s[1])
		upper = (int)strtol(s + 1, NULL, 10);
	      else
		upper = 2147483647;
	    }
	    else
	      upper = lower;

            ippSetRange(ipp, &attr, i, lower, upper);
	  }
	  break;

      case IPP_TAG_RESOLUTION :
          {
	   /*
	    * Resolution...
	    */
	    int		xres, yres;	/* Resolution values */
	    ipp_res_t	units;		/* Resolution units */

	    xres = (int)strtol(val, &s, 10);

	    if (*s == 'x')
	      yres = (int)strtol(s + 1, &s, 10);
	    else
	      yres = xres;

	    if (!_cups_strcasecmp(s, "dpc") || !_cups_strcasecmp(s, "dpcm"))
	      units = IPP_RES_PER_CM;
	    else
	      units = IPP_RES_PER_INCH;

	    ippSetResolution(ipp, &attr, i, units, xres, yres);
          }
	  break;

      case IPP_TAG_STRING :
	 /*
	  * octetString
	  */

          ippSetOctetString(ipp, &attr, i, val, (int)strlen(val));
	  break;

      case IPP_TAG_BEGIN_COLLECTION :
	 /*
	  * Collection value
	  */

	  num_cols = cupsParseOptions(val, 0, &cols);
	  if ((collection = ippNew()) == NULL)
	  {
	    cupsFreeOptions(num_cols, cols);

	    if (copy)
	      free(copy);

	    ippDeleteAttribute(ipp, attr);
	    return (NULL);
	  }

	  ippSetCollection(ipp, &attr, i, collection);
	  cupsEncodeOptions2(collection, num_cols, cols, IPP_TAG_JOB);
	  cupsFreeOptions(num_cols, cols);
	  break;

      default :
	  ippSetString(ipp, &attr, i, val);
	  break;
    }
  }

  if (copy)
    free(copy);

  return (attr);
}


/*
 * 'cupsEncodeOption()' - Encode a single option into an IPP attribute.
 *
 * @since CUPS 2.3@
 */

ipp_attribute_t	*			/* O - New attribute or @code NULL@ on error */
cupsEncodeOption(ipp_t      *ipp,	/* I - IPP request/response */
                 ipp_tag_t  group_tag,	/* I - Attribute group */
                 const char *name,	/* I - Option name */
                 const char *value)	/* I - Option string value */
{
  return (_cupsEncodeOption(ipp, group_tag, _ippFindOption(name), name, value));
}


/*
 * 'cupsEncodeOptions()' - Encode printer options into IPP attributes.
 *
 * This function adds operation, job, and then subscription attributes,
 * in that order. Use the @link cupsEncodeOptions2@ function to add attributes
 * for a single group.
 */

void
cupsEncodeOptions(ipp_t         *ipp,		/* I - IPP request/response */
        	  int           num_options,	/* I - Number of options */
		  cups_option_t *options)	/* I - Options */
{
  DEBUG_printf(("cupsEncodeOptions(%p, %d, %p)", (void *)ipp, num_options, (void *)options));

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
 * function multiple times for each group, or use @link cupsEncodeOptions@
 * to add the standard groups.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

void
cupsEncodeOptions2(
    ipp_t         *ipp,			/* I - IPP request/response */
    int           num_options,		/* I - Number of options */
    cups_option_t *options,		/* I - Options */
    ipp_tag_t     group_tag)		/* I - Group to encode */
{
  int			i;		/* Looping var */
  char			*val;		/* Pointer to option value */
  cups_option_t		*option;	/* Current option */
  ipp_op_t		op;		/* Operation for this request */
  const ipp_op_t	*ops;		/* List of allowed operations */


  DEBUG_printf(("cupsEncodeOptions2(ipp=%p(%s), num_options=%d, options=%p, group_tag=%x)", (void *)ipp, ipp ? ippOpString(ippGetOperation(ipp)) : "", num_options, (void *)options, group_tag));

 /*
  * Range check input...
  */

  if (!ipp || num_options < 1 || !options)
    return;

 /*
  * Do special handling for the document-format/raw options...
  */

  op = ippGetOperation(ipp);

  if (group_tag == IPP_TAG_OPERATION && (op == IPP_OP_PRINT_JOB || op == IPP_OP_PRINT_URI || op == IPP_OP_SEND_DOCUMENT || op == IPP_OP_SEND_URI))
  {
   /*
    * Handle the document format stuff first...
    */

    if ((val = (char *)cupsGetOption("document-format", num_options, options)) != NULL)
      ippAddString(ipp, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, val);
    else if (cupsGetOption("raw", num_options, options))
      ippAddString(ipp, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, "application/vnd.cups-raw");
    else
      ippAddString(ipp, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, "application/octet-stream");
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

    if (!_cups_strcasecmp(option->name, "raw") || !_cups_strcasecmp(option->name, "document-format") || !option->name[0])
      continue;

   /*
    * Figure out the proper value and group tags for this option...
    */

    if ((match = _ippFindOption(option->name)) != NULL)
    {
      if (match->group_tag != group_tag && match->alt_group_tag != group_tag)
        continue;

      if (match->operations)
        ops = match->operations;
      else if (group_tag == IPP_TAG_JOB)
        ops = ipp_job_creation;
      else if (group_tag == IPP_TAG_DOCUMENT)
        ops = ipp_doc_creation;
      else if (group_tag == IPP_TAG_SUBSCRIPTION)
        ops = ipp_sub_creation;
      else if (group_tag == IPP_TAG_PRINTER)
        ops = ipp_set_printer;
      else
      {
	DEBUG_printf(("2cupsEncodeOptions2: Skipping \"%s\".", option->name));
        continue;
      }
    }
    else
    {
      int	namelen;		/* Length of name */

      namelen = (int)strlen(option->name);

      if (namelen < 10 || (strcmp(option->name + namelen - 8, "-default") && strcmp(option->name + namelen - 10, "-supported")))
      {
	if (group_tag != IPP_TAG_JOB && group_tag != IPP_TAG_DOCUMENT)
	{
	  DEBUG_printf(("2cupsEncodeOptions2: Skipping \"%s\".", option->name));
          continue;
        }
      }
      else if (group_tag != IPP_TAG_PRINTER)
      {
	DEBUG_printf(("2cupsEncodeOptions2: Skipping \"%s\".", option->name));
        continue;
      }

      if (group_tag == IPP_TAG_JOB)
        ops = ipp_job_creation;
      else if (group_tag == IPP_TAG_DOCUMENT)
        ops = ipp_doc_creation;
      else
        ops = ipp_set_printer;
    }

   /*
    * Verify that we send this attribute for this operation...
    */

    while (*ops != IPP_OP_CUPS_NONE)
      if (op == *ops)
        break;
      else
        ops ++;

    if (*ops == IPP_OP_CUPS_NONE && op != IPP_OP_CUPS_NONE)
    {
      DEBUG_printf(("2cupsEncodeOptions2: Skipping \"%s\".", option->name));
      continue;
    }

    _cupsEncodeOption(ipp, group_tag, match, option->name, option->value);
  }
}


#ifdef DEBUG
/*
 * '_ippCheckOptions()' - Validate that the option array is sorted properly.
 */

const char *				/* O - First out-of-order option or NULL */
_ippCheckOptions(void)
{
  int	i;				/* Looping var */


  for (i = 0; i < (int)(sizeof(ipp_options) / sizeof(ipp_options[0]) - 1); i ++)
    if (strcmp(ipp_options[i].name, ipp_options[i + 1].name) >= 0)
      return (ipp_options[i + 1].name);

  return (NULL);
}
#endif /* DEBUG */


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
