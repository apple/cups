/*
 * "$Id: ipp.h,v 1.2 1999/01/24 14:25:11 mike Exp $"
 *
 *   Internet Printing Protocol definitions for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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

/*
 * IPP version string...
 */

#define IPP_VERSION	"\001\000"	/* %x01.00 */

/*
 * Format tags for attribute formats...
 */

#define TAG_ZERO	0x00
#define TAG_OPERATION	0x01
#define TAG_JOB		0x02
#define TAG_PRINTER	0x03
#define TAG_END		0x04
#define TAG_UNSUPPORTED	0x05

#define TAG_INTEGER	0x21
#define TAG_BOOLEAN	0x22
#define TAG_ENUM	0x23

#define TAG_STRING	0x30
#define TAG_DATE	0x31
#define TAG_RESOLUTION	0x32
#define TAG_RANGE	0x33
#define TAG_TEXTLANG	0x35
#define TAG_NAMELANG	0x36

#define TAG_TEXT	0x41
#define TAG_NAME	0x42
#define TAG_KEYWORD	0x44
#define TAG_URI		0x45
#define TAG_URISCHEME	0x46
#define TAG_CHARSET	0x47
#define TAG_LANGUAGE	0x48
#define TAG_MIMETYPE	0x49

/*
 * Resolution units...
 */

#define RES_PER_INCH	3
#define RES_PER_CM	4

/*
 * IPP states...
 */

typedef enum
{
  IPP_IDLE,
  IPP_REQ_HEADER,
  IPP_REQ_ATTR,
  IPP_REQ_DATA,
  IPP_RES_HEADER,
  IPP_RES_ATTR,
  IPP_RES_DATA
} ipp_state_t;

/*
 * IPP operations...
 */

typedef enum
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

/*
 * IPP status codes...
 */

typedef enum
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

/*
 * IPP message formats...
 */

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;

typedef struct		/**** Request Header ****/
{
  uchar		version[2];	/* Protocol version number */
  ipp_op_t	operation_id;	/* Operation ID */
  int		request_id;	/* Request ID */
} ipp_request_t;

typedef struct		/**** Response Header ****/
{
  uchar		version[2];	/* Protocol version number */
  ipp_status_t	status_code;	/* Status code */
  int		request_id;	/* Request ID */
} ipp_response_t;

typedef union		/**** Attribute Value ****/
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

typedef struct		/**** Attribute ****/
{
  uchar		group_tag,	/* Job/Printer/Operation group tag */
		value_tag;	/* What type of value is it? */
  char		*name;		/* Name of attribute */
  int		num_values;	/* Number of values */
  ipp_value_t	*values;	/* Values */
} ipp_attribute_t;

typedef struct		/**** Request State ****/
{
  ipp_state_t	state;		/* State of request */
  ipp_request_t	request;	/* Request header */
  int		num_attrs;	/* Number of attributes */
  ipp_attribute_t *attrs;	/* Attributes */
  ipp_status_t	status;		/* Response status */
} ipp_client_t;


/*
 * Globals...
 */

VAR ipp_client_t	IPPClients[MAX_CLIENTS];


/*
 * Prototypes...
 */

extern int	ReadIPP(client_t *con, ipp_client_t *ipp);
extern int	WriteIPP(client_t *con, ipp_client_t *ipp);


/*
 * End of "$Id: ipp.h,v 1.2 1999/01/24 14:25:11 mike Exp $".
 */
