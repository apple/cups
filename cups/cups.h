/*
 * "$Id: cups.h,v 1.3 1999/01/28 22:00:44 mike Exp $"
 *
 *   API definitions for the Common UNIX Printing System (CUPS).
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

#ifndef _CUPS_CUPS_H_
#  define _CUPS_CUPS_H_

/*
 * Include necessary headers...
 */

#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#  include <time.h>

#  include <cups/ipp.h>
#  include <cups/mime.h>
#  include <cups/ppd.h>


/*
 * C++ magic...
 */

#  ifdef _cplusplus
extern "C" {
#  endif /* _cplusplus */


/*
 * Constants...
 */

#  define CUPS_VERSION		1.0
#  define CUPS_DATE_ANY		-1


/*
 * Types and structures...
 */

typedef enum				/**** Printer Status Bits ****/
{
  CUPS_PRINTER_IDLE = 0x00,		/* Printer is idle */
  CUPS_PRINTER_BUSY = 0x01,		/* Printer is busy */
  CUPS_PRINTER_FAULTED = 0x02,		/* Printer is faulted */
  CUPS_PRINTER_UNAVAILABLE = 0x03,	/* Printer is unavailable */
  CUPS_PRINTER_DISABLED = 0x04,		/* Printer is disabled */
  CUPS_PRINTER_REJECTING = 0x08		/* Printer is rejecting jobs */
} cups_pstatus_t;

typedef enum				/**** Printer Type/Capability Bits ****/
{
  CUPS_PRINTER_CLASS = 0x0001,		/* Printer class */
  CUPS_PRINTER_REMOTE = 0x0002,		/* Remote printer or class */
  CUPS_PRINTER_BW = 0x0004,		/* Can do B&W printing */
  CUPS_PRINTER_COLOR = 0x0008,		/* Can do color printing */
  CUPS_PRINTER_DUPLEX = 0x0010,		/* Can do duplexing */
  CUPS_PRINTER_STAPLE = 0x0020,		/* Can staple output */
  CUPS_PRINTER_COPIES = 0x0040,		/* Can do copies */
  CUPS_PRINTER_COLLATE = 0x0080,	/* Can collage copies */
  CUPS_PRINTER_PUNCH = 0x0100,		/* Can punch output */
  CUPS_PRINTER_COVER = 0x0200,		/* Can cover output */
  CUPS_PRINTER_BIND = 0x0400,		/* Can bind output */
  CUPS_PRINTER_SORT = 0x0800,		/* Can sort output */
  CUPS_PRINTER_SMALL = 0x1000,		/* Can do Letter/Legal/A4 */
  CUPS_PRINTER_MEDIUM = 0x2000,		/* Can do Tabloid/B/C/A3/A2 */
  CUPS_PRINTER_LARGE = 0x4000,		/* Can do D/E/A1/A0 */
  CUPS_PRINTER_VARIABLE = 0x8000	/* Can do variable sizes */
} cups_ptype_t;

typedef enum				/**** Job Priorities ****/
{
  CUPS_PRIORITY_ANY = -1,		/* Use the default priority */
  CUPS_PRIORITY_LOW,			/* Lowest priority */
  CUPS_PRIORITY_NORMAL = 50,		/* Default priority */
  CUPS_PRIORITY_HIGH = 100		/* Highest priority */
} cups_jpriority_t;

typedef enum				/**** Job Status ****/
{
  CUPS_JOB_PRINTING,
  CUPS_JOB_PENDING,
  CUPS_JOB_STOPPED
} cups_jstatus_t;

/*
 * Error codes...
 */

#  define CUPS_ERROR_NONE		0x00
#  define CUPS_ERROR_ACCESS		0x01
#  define CUPS_ERROR_NOT_AVAILABLE	0x02
#  define CUPS_ERROR_NOT_PRIVILEDGED	0x03

#  define CUPS_ERROR_BAD_PRINTER	0x10
#  define CUPS_ERROR_BAD_CLASS		0x11
#  define CUPS_ERROR_BAD_URI		0x12
#  define CUPS_ERROR_BAD_INTERFACE	0x13
#  define CUPS_ERROR_BAD_PPD		0x14
#  define CUPS_ERROR_BAD_LOCATION_CODE	0x15
#  define CUPS_ERROR_BAD_STATUS_CODE	0x16
#  define CUPS_ERROR_BAD_ACL		0x17
#  define CUPS_ERROR_BAD_JOB		0x18
#  define CUPS_ERROR_BAD_USER		0x19
#  define CUPS_ERROR_BAD_FILENAME	0x1a
#  define CUPS_ERROR_BAD_ID		0x1b
#  define CUPS_ERROR_BAD_COPIES		0x1c
#  define CUPS_ERROR_BAD_PRIORITY	0x1d
#  define CUPS_ERROR_BAD_OPTIONS	0x1e
#  define CUPS_ERROR_BAD_DATE		0x1f


/*
 * Types & structures...
 */

typedef struct				/**** Printer Information ****/
{
  char		printer_or_class[128],	/* Printer or class name */
		device_uri[128],	/* Universal resource identifier */
		description[64],	/* Printer or class description */
		location_code[32],	/* Location code */
		location_text[96],	/* Location text */
  short		type_code,		/* Printer type code */
		status_code;		/* Status code */
  unsigned char	status_text[60];	/* Status text */
} cups_printer_t;

typedef struct				/**** Job Information ****/
{
  char		printer_or_class[128],	/* Printer or class name */
		user[64],		/* Username (user@host) */
		super[MIME_MAX_SUPER],	/* Super-type of job file */
		type[MIME_MAX_TYPE],	/* Type of job file */
		description[128],	/* Job title or filename */
  int		id;			/* Job identifier */
  unsigned	size;			/* Size of file in bytes */
  int		status_code,		/* Status code */
		priority,		/* Job priority */
		num_copies,		/* Number of copies */
		num_options;		/* Number of job options */
  char		**options;		/* Job options (name=value) */
} cups_job_t;

typedef struct				/**** Page Information ****/
{
  char		printer_or_class[128],	/* Printer or class name */
		user[64];		/* Username (user@host) */
  unsigned char	description[128];	/* Job title or filename */
  int		id;			/* Job identifier */
  time_t	secs;			/* Time stamp */
  int		num_copies,		/* Number of copies */
		num_options;		/* Number of job options */
  char		**options;		/* Job options (name=value) */
} cups_page_t;

typedef struct				/**** Message Header ****/
{
  short		type,			/* Message type (see above) */
		count;			/* Number of records in message */
  unsigned	length;			/* Length of message in bytes */
} cups_header_t;


typedef struct				/**** Option Structure ****/
{
  char		*name;			/* Option name */
  char		*value;			/* Option value */
} cups_option_t;


/*
 * Functions...
 */

extern int		cupsOpen(void);
extern int		cupsClose(int fd);

extern unsigned char	*cupsErrorString(int code);
extern unsigned char	*cupsJobStatusString(int code);
extern unsigned char	*cupsPrinterStatusString(int code);

extern int		cupsPrinterUpdate(int fd, char *name, short status_code,
			                  unsigned char *status_text);
extern cups_msg_t	*cupsPrinterQuery(char *name);
extern cups_msg_t	*cupsPrinterFind(char *name, char *classes,
					 unsigned char *location_code,
					 unsigned char *location_text);

extern cups_msg_t	*cupsJobAdd(unsigned char *name, char *queue_name,
			            char *filename, int copies, int priority,
				    int copy_file, cups_array_t *options);
extern cups_msg_t	*cupsJobRemove(int id, char *queue_name);
extern cups_msg_t	*cupsJobUpdate(int id, char *queue_name,
			               int copies, int priority,
				       cups_array_t *options);
extern cups_msg_t	*cupsJobQuery(int id);
extern cups_msg_t	*cupsJobFind(int id, char *queue_name, int priority);

extern cups_msg_t	*cupsPageAdd(unsigned char *job_name,
			             char *printer_name, char *user,
				     char *filename, int copies, int priority,
				     time_t start_date, time_t end_date,
				     cups_array_t *options);
extern cups_msg_t	*cupsPageQuery(char *printer_name, char *user,
			               int copies);
extern cups_msg_t	*cupsPageFind(unsigned char *job_name,
			              char *printer_name, char *user,
				      char *filename, int priority,
				      time_t start_date, time_t end_date);
extern cups_msg_t	*cupsPageClear(unsigned char *job_name,
			               char *printer_name, char *user,
				       char *filename, int priority,
				       time_t start_date, time_t end_date);

extern int		*cupsParseOptions(char *arg, cups_option_t **options);

#  ifdef _cplusplus
}
#  endif /* _cplusplus */

#endif /* !_CUPS_CUPS_H_ */

/*
 * End of "$Id: cups.h,v 1.3 1999/01/28 22:00:44 mike Exp $".
 */
