/*
 * "$Id: ipp.h,v 1.2 1999/01/24 14:18:43 mike Exp $"
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
 *       44145 Airport View Drive, Suite 204
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
 * Types and structures...
 */

typedef enum			/**** Format tags for attribute formats... ****/
{
  IPP_TAG_ZERO = 0x00,
  IPP_TAG_OPERATION,
  IPP_TAG_JOB,
  IPP_TAG_PRINTER,
  IPP_TAG_END,
  IPP_TAG_UNSUPPORTED,
  IPP_TAG_INTEGER = 0x21,
  IPP_TAG_BOOLEAN,
  IPP_TAG_ENUM,
  IPP_TAG_STRING = 0x30,
  IPP_TAG_DATE,
  IPP_TAG_RESOLUTION,
  IPP_TAG_RANGE,
  IPP_TAG_TEXTLANG,
  IPP_TAG_NAMELANG,
  IPP_TAG_TEXT = 0x41,
  IPP_TAG_NAME,
  IPP_TAG_KEYWORD,
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

typedef enum			/**** IPP states... ****/
{
  IPP_IDLE,
  IPP_REQUEST_HEADER,
  IPP_REQUEST_ATTR,
  IPP_REQUEST_DATA,
  IPP_RESPONSE_HEADER,
  IPP_RESPONSE_ATTR,
  IPP_RESPONSE_DATA
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
  IPP_PRIVATE = 0x4000
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

typedef unsigned char uchar;	/**** Unsigned 8-bit integer/character ****/
typedef unsigned short ushort;	/**** Unsigned 16-bit integer ****/
typedef unsigned int uint;	/**** Unsigned 32-bit integer ****/

typedef struct			/**** Request Header ****/
{
  uchar		version[2];	/* Protocol version number */
  ipp_op_t	operation_id;	/* Operation ID */
  int		request_id;	/* Request ID */
} ipp_request_t;

typedef struct			/**** Response Header ****/
{
  uchar		version[2];	/* Protocol version number */
  ipp_status_t	status_code;	/* Status code */
  int		request_id;	/* Request ID */
} ipp_response_t;

typedef union			/**** Attribute Value ****/
{
  int		integer;	/* Integer value */

  char		boolean;	/* Boolean value */

  int		enumeration;	/* Enumerated value */

  uchar		*string;	/* String value */

  uchar		date[11];	/* Date/time value */

  struct
  {
    int		xres,		/* Horizontal resolution */
		yres;		/* Vertical resolution */
    char	unit;		/* Resolution units */
  }		resolution;	/* Resolution value */

  struct
  {
    int		lower,		/* Lower value */
		upper;		/* Upper value */
  }		range;		/* Range of integers value */

  struct
  {
    char	*charset;	/* Character set */
    uchar	*string;	/* String */
  }		lstring;	/* String with language value */
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
  ipp_attribute_t *attrs;	/* Attributes */
		*last;		/* Last attribute in list */
  ipp_status_t	status;		/* Response status */
} ipp_t;


/*
 * Prototypes...
 */

extern void	ippAddBoolean(ipp_t *ipp, uchar group, char *name, char value);
extern void	ippAddBooleans(ipp_t *ipp, uchar group, char *name, int num_values, char *values);
extern void	ippAddDate(ipp_t *ipp, uchar group, char *name, uchar *value);
extern void	ippAddEnum(ipp_t *ipp, uchar group, char *name, int value);
extern void	ippAddEnums(ipp_t *ipp, uchar group, char *name, int num_values, int *values);
extern void	ippAddInteger(ipp_t *ipp, uchar group, char *name, int value);
extern void	ippAddIntegers(ipp_t *ipp, uchar group, char *name, int num_values, int *values);
extern void	ippAddLString(ipp_t *ipp, uchar group, char *name, char *charset, uchar *value);
extern void	ippAddLStrings(ipp_t *ipp, uchar group, char *name, int num_values, char *charset, uchar **values);
extern void	ippAddRange(ipp_t *ipp, uchar group, char *name, int lower, int upper);
extern void	ippAddResolution(ipp_t *ipp, uchar group, char *name, int units, int xres, int yres);
extern void	ippAddString(ipp_t *ipp, uchar group, char *name, uchar *value);
extern void	ippAddStrings(ipp_t *ipp, uchar group, char *name, int num_values, uchar **values);
extern time_t	ippDateToTime(uchar *date);
extern void	ippDelete(ipp_t *ipp);
extern ipp_t	*ippNew(void);
extern int	ippRead(http_t *http, ipp_t *ipp);
extern uchar	*ippTimeToDate(time_t t);
extern int	ippWrite(http_t *http, ipp_t *ipp);

/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_IPP_H_ */

/*
 * End of "$Id: ipp.h,v 1.2 1999/01/24 14:18:43 mike Exp $".
 */
