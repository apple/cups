/*
 * "$Id: ipp.h,v 1.18 1999/07/24 10:44:29 mike Exp $"
 *
 *   Internet Printing Protocol definitions for the Common UNIX Printing
 *   System (CUPS).
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
 */

#ifndef _CUPS_IPP_H_
#  define _CUPS_IPP_H_

/*
 * Include necessary headers...
 */

#  include <cups/http.h>


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * IPP version string...
 */

#  define IPP_VERSION		"\001\000"

/*
 * IPP registered port number...  This is the default value - applications
 * should use the ippPort() function so that you can customize things in
 * /etc/services if needed!
 */

#  define IPP_PORT		631

/*
 * Common limits...
 */

#  define IPP_MAX_NAME		256
#  define IPP_MAX_VALUES	100


/*
 * Types and structures...
 */

typedef enum			/**** Format tags for attribute formats... ****/
{
  IPP_TAG_ZERO = 0x00,
  IPP_TAG_OPERATION,
  IPP_TAG_JOB,
  IPP_TAG_END,
  IPP_TAG_PRINTER,
  IPP_TAG_EXTENSION,
  IPP_TAG_UNSUPPORTED = 0x10,
  IPP_TAG_DEFAULT,
  IPP_TAG_UNKNOWN,
  IPP_TAG_NOVALUE,
  IPP_TAG_INTEGER = 0x21,
  IPP_TAG_BOOLEAN,
  IPP_TAG_ENUM,
  IPP_TAG_STRING = 0x30,
  IPP_TAG_DATE,
  IPP_TAG_RESOLUTION,
  IPP_TAG_RANGE,
  IPP_TAG_COLLECTION,
  IPP_TAG_TEXTLANG,
  IPP_TAG_NAMELANG,
  IPP_TAG_TEXT = 0x41,
  IPP_TAG_NAME,
  IPP_TAG_KEYWORD = 0x44,
  IPP_TAG_URI,
  IPP_TAG_URISCHEME,
  IPP_TAG_CHARSET,
  IPP_TAG_LANGUAGE,
  IPP_TAG_MIMETYPE
} ipp_tag_t;

typedef enum			/**** Resolution units... ****/
{
  IPP_RES_PER_INCH = 3,
  IPP_RES_PER_CM
} ipp_res_t;

typedef enum			/**** Multiple Document Handling ****/
{
  IPP_DOC_SINGLE,
  IPP_DOC_UNCOLLATED,
  IPP_DOC_COLLATED,
  IPP_DOC_SEPARATE
} ipp_doc_t;

typedef enum			/**** Finishings... ****/
{
  IPP_FINISH_NONE = 3,
  IPP_FINISH_STAPLE,
  IPP_FINISH_PUNCH,
  IPP_FINISH_COVER,
  IPP_FINISH_BIND
} ipp_finish_t;

typedef enum			/**** Orientation... ****/
{
  IPP_PORTRAIT = 3,		/* No rotation */
  IPP_LANDSCAPE,		/* 90 degrees counter-clockwise */
  IPP_REVERSE_LANDSCAPE,	/* 90 degrees clockwise */
  IPP_REVERSE_PORTRAIT		/* 180 degrees */
} ipp_orient_t;

typedef enum			/**** Qualities... ****/
{
  IPP_QUALITY_DRAFT = 3,
  IPP_QUALITY_NORMAL,
  IPP_QUALITY_HIGH
} ipp_quality_t;

typedef enum			/**** Job States.... */
{
  IPP_JOB_PENDING = 3,
  IPP_JOB_HELD,
  IPP_JOB_PROCESSING,
  IPP_JOB_STOPPED,
  IPP_JOB_CANCELED,
  IPP_JOB_ABORTED,
  IPP_JOB_COMPLETED
} ipp_jstate_t;

typedef enum			/**** Printer States.... */
{
  IPP_PRINTER_IDLE = 3,
  IPP_PRINTER_PROCESSING,
  IPP_PRINTER_STOPPED
} ipp_pstate_t;

typedef enum			/**** IPP states... ****/
{
  IPP_ERROR = -1,		/* An error occurred */
  IPP_IDLE,			/* Nothing is happening/request completed */
  IPP_HEADER,			/* The request header needs to be sent/received */
  IPP_ATTRIBUTE,		/* One or more attributes need to be sent/received */
  IPP_DATA			/* IPP request data needs to be sent/received */
} ipp_state_t;

typedef enum			/**** IPP operations... ****/
{
  IPP_PRINT_JOB = 0x0002,
  IPP_PRINT_URI,
  IPP_VALIDATE_JOB,
  IPP_CREATE_JOB,
  IPP_SEND_DOCUMENT,
  IPP_SEND_URI,
  IPP_CANCEL_JOB,
  IPP_GET_JOB_ATTRIBUTES,
  IPP_GET_JOBS,
  IPP_GET_PRINTER_ATTRIBUTES,
  IPP_HOLD_JOB = 0x000c,
  IPP_RELEASE_JOB,
  IPP_RESTART_JOB,
  IPP_PAUSE_PRINTER = 0x0010,
  IPP_RESUME_PRINTER,
  IPP_PURGE_JOBS,
  IPP_PRIVATE = 0x4000,
  CUPS_GET_DEFAULT,
  CUPS_GET_PRINTERS,
  CUPS_ADD_PRINTER,
  CUPS_DELETE_PRINTER,
  CUPS_GET_CLASSES,
  CUPS_ADD_CLASS,
  CUPS_DELETE_CLASS,
  CUPS_ACCEPT_JOBS,
  CUPS_REJECT_JOBS,
  CUPS_SET_DEFAULT
} ipp_op_t;

typedef enum			/**** IPP status codes... ****/
{
  IPP_OK = 0x0000,
  IPP_OK_SUBST,
  IPP_OK_CONFLICT,
  IPP_BAD_REQUEST = 0x0400,
  IPP_FORBIDDEN,
  IPP_NOT_AUTHENTICATED,
  IPP_NOT_AUTHORIZED,
  IPP_NOT_POSSIBLE,
  IPP_TIMEOUT,
  IPP_NOT_FOUND,
  IPP_GONE,
  IPP_REQUEST_ENTITY,
  IPP_REQUEST_VALUE,
  IPP_DOCUMENT_FORMAT,
  IPP_ATTRIBUTES,
  IPP_URI_SCHEME,
  IPP_CHARSET,
  IPP_CONFLICT,
  IPP_INTERNAL_ERROR = 0x0500,
  IPP_OPERATION_NOT_SUPPORTED,
  IPP_SERVICE_UNAVAILABLE,
  IPP_VERSION_NOT_SUPPORTED,
  IPP_DEVICE_UNAVAILABLE,
  IPP_TEMPORARY_ERROR,
  IPP_NOT_ACCEPTING,
  IPP_PRINTER_BUSY
} ipp_status_t;

typedef unsigned char ipp_uchar_t;/**** Unsigned 8-bit integer/character ****/

typedef union			/**** Request Header ****/
{
  struct			/* Any Header */
  {
    ipp_uchar_t	version[2];	/* Protocol version number */
    int		op_status;	/* Operation ID or status code*/
    int		request_id;	/* Request ID */
  }		any;

  struct			/* Operation Header */
  {
    ipp_uchar_t	version[2];	/* Protocol version number */
    ipp_op_t	operation_id;	/* Operation ID */
    int		request_id;	/* Request ID */
  }		op;

  struct			/* Status Header */
  {
    ipp_uchar_t	version[2];	/* Protocol version number */
    ipp_status_t status_code;	/* Status code */
    int		request_id;	/* Request ID */
  }		status;
} ipp_request_t;


typedef union			/**** Attribute Value ****/
{
  int		integer;	/* Integer/enumerated value */

  char		boolean;	/* Boolean value */

  ipp_uchar_t	date[11];	/* Date/time value */

  struct
  {
    int		xres,		/* Horizontal resolution */
		yres;		/* Vertical resolution */
    ipp_res_t	units;		/* Resolution units */
  }		resolution;	/* Resolution value */

  struct
  {
    int		lower,		/* Lower value */
		upper;		/* Upper value */
  }		range;		/* Range of integers value */

  struct
  {
    char	*charset;	/* Character set */
    char	*text;		/* String */
  }		string;		/* String with language value */
} ipp_value_t;

typedef struct ipp_attribute_s	/**** Attribute ****/
{
  struct ipp_attribute_s	*next;
  				/* Next atrtribute in list */
  ipp_tag_t	group_tag,	/* Job/Printer/Operation group tag */
		value_tag;	/* What type of value is it? */
  char		*name;		/* Name of attribute */
  int		num_values;	/* Number of values */
  ipp_value_t	values[1];	/* Values */
} ipp_attribute_t;

typedef struct			/**** Request State ****/
{
  ipp_state_t	state;		/* State of request */
  ipp_request_t	request;	/* Request header */
  ipp_attribute_t *attrs,	/* Attributes */
		*last,		/* Last attribute in list */
		*current;	/* Current attribute (for read/write) */
  ipp_tag_t	curtag;		/* Current attribute group tag */
} ipp_t;


/*
 * Prototypes...
 */

extern time_t		ippDateToTime(const ipp_uchar_t *date);
extern ipp_attribute_t	*ippAddBoolean(ipp_t *ipp, ipp_tag_t group, const char *name, char value);
extern ipp_attribute_t	*ippAddBooleans(ipp_t *ipp, ipp_tag_t group, const char *name, int num_values, const char *values);
extern ipp_attribute_t	*ippAddDate(ipp_t *ipp, ipp_tag_t group, const char *name, const ipp_uchar_t *value);
extern ipp_attribute_t	*ippAddInteger(ipp_t *ipp, ipp_tag_t group, ipp_tag_t type, const char *name, int value);
extern ipp_attribute_t	*ippAddIntegers(ipp_t *ipp, ipp_tag_t group, ipp_tag_t type, const char *name, int num_values, const int *values);
extern ipp_attribute_t	*ippAddRange(ipp_t *ipp, ipp_tag_t group, const char *name, int lower, int upper);
extern ipp_attribute_t	*ippAddRanges(ipp_t *ipp, ipp_tag_t group, const char *name, int num_values, const int *lower, const int *upper);
extern ipp_attribute_t	*ippAddResolution(ipp_t *ipp, ipp_tag_t group, const char *name, ipp_res_t units, int xres, int yres);
extern ipp_attribute_t	*ippAddResolutions(ipp_t *ipp, ipp_tag_t group, const char *name, int num_values, ipp_res_t units, const int *xres, const int *yres);
extern ipp_attribute_t	*ippAddSeparator(ipp_t *ipp);
extern ipp_attribute_t	*ippAddString(ipp_t *ipp, ipp_tag_t group, ipp_tag_t type, const char *name, const char *charset, const char *value);
extern ipp_attribute_t	*ippAddStrings(ipp_t *ipp, ipp_tag_t group, ipp_tag_t type, const char *name, int num_values, const char *charset, const char **values);
extern void		ippDelete(ipp_t *ipp);
extern ipp_attribute_t	*ippFindAttribute(ipp_t *ipp, const char *name, ipp_tag_t type);
extern size_t		ippLength(ipp_t *ipp);
extern ipp_t		*ippNew(void);
extern ipp_state_t	ippRead(http_t *http, ipp_t *ipp);
extern const ipp_uchar_t *ippTimeToDate(time_t t);
extern ipp_state_t	ippWrite(http_t *http, ipp_t *ipp);
extern int		ippPort(void);

/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_IPP_H_ */

/*
 * End of "$Id: ipp.h,v 1.18 1999/07/24 10:44:29 mike Exp $".
 */
