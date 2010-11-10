/*
 * "$Id$"
 *
 *   Sample IPP server for CUPS.
 *
 *   Copyright 2010 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 */

/*
 * Include necessary headers...
 */

#include <cups/cups-private.h>


/*
 * Constants...
 */

enum					/* printer-state-reasons values */
{
  _IPP_PRINTER_NONE = 0x0000,		/* none */
  _IPP_PRINTER_OTHER = 0x0001,		/* other */
  _IPP_PRINTER_COVER_OPEN = 0x0002,	/* cover-open */
  _IPP_PRINTER_INPUT_TRAY_MISSING = 0x0004,
					/* input-tray-missing */
  _IPP_PRINTER_MARKER_SUPPLY_EMPTY = 0x0008,
					/* marker-supply-empty */
  _IPP_PRINTER_MARKER_SUPPLY_LOW = 0x0010,
					/* marker-suply-low */
  _IPP_PRINTER_MARKER_WASTE_ALMOST_FULL = 0x0020,
					/* marker-waste-almost-full */
  _IPP_PRINTER_MARKER_WASTE_FULL = 0x0040,
					/* marker-waste-full */
  _IPP_PRINTER_MEDIA_EMPTY = 0x0080,	/* media-empty */
  _IPP_PRINTER_MEDIA_JAM = 0x0100,	/* media-jam */
  _IPP_PRINTER_MEDIA_LOW = 0x0200,	/* media-low */
  _IPP_PRINTER_MEDIA_NEEDED = 0x0400,	/* media-needed */
  _IPP_PRINTER_MOVING_TO_PAUSED = 0x0800,
					/* moving-to-paused */
  _IPP_PRINTER_PAUSED = 0x1000,		/* paused */
  _IPP_PRINTER_SPOOL_AREA_FULL = 0x2000,/* spool-area-full */
  _IPP_PRINTER_TONER_EMPTY = 0x4000,	/* toner-empty */
  _IPP_PRINTER_TONER_LOW = 0x8000	/* toner-low */
};

enum					/* job-state-reasons values */
{
  _IPP_JOB_NONE = 0x0000,		/* none */
  _IPP_JOB_ABORTED_BY_SYSTEM = 0x0001,	/* aborted-by-system */
  _IPP_JOB_CANCELED_AT_DEVICE = 0x0002,	/* job-canceled-at-device */
  _IPP_JOB_CANCELED_BY_USER = 0x0004,	/* job-canceled-by-user */
  _IPP_JOB_COMPLETED_SUCCESSFULLY = 0x0008,
					/* job-completed-successfully */
  _IPP_JOB_COMPLETED_WITH_ERRORS = 0x0010,
					/* job-completed-with-errors */
  _IPP_JOB_COMPLETED_WITH_WARNINGS = 0x0020,
					/* job-completed-with-warnings */
  _IPP_JOB_DATA_INSUFFICIENT = 0x0040,	/* job-data-insufficient */
  _IPP_JOB_HOLD_UNTIL_SPECIFIED = 0x0080,
					/* job-hold-until-specified */
  _IPP_JOB_INCOMING = 0x0100,		/* job-incoming*/
  _IPP_JOB_PRINTER_STOPPED = 0x0200,	/* printer-stopped */
  _IPP_JOB_PRINTING = 0x0400,		/* job-printing */
  _IPP_JOB_PROCESSING_TO_STOP_POINT = 0x0800,
					/* processing-to-stop-point */
  _IPP_JOB_QUEUED = 0x1000,		/* job-queued */
  _IPP_JOB_RESTARTABLE = 0x2000		/* job-restartable */
};


/*
 * Structures...
 */

typedef struct _ipp_printer_s		/**** Printer data ****/
{
  int			ipv4,		/* IPv4 listener */
			ipv6;		/* IPv6 listener */
  DNSServiceRef		ipp_ref,	/* Bonjour IPP service */
			printer_ref;	/* Bonjour LPD service */
  TXTRef		ipp_txt,	/* Bonjour IPP TXT record */
			printer_txt;	/* Bonjour LPD TXT record */
  ipp_attribute_t	*attrs;		/* Static attributes */
  char			*name;		/* printer-name */
  ipp_pstate_t		state;		/* printer-state value */
  unsigned		state_reasons;	/* printer-state-reasons values */  
  cups_array_t		*clients;	/* Clients */
  cups_array_t		*jobs;		/* Jobs */
} _ipp_printer_t;

typedef struct _ipp_job_s		/**** Job data ****/
{
  int			id;		/* Job ID */
  char			*name;		/* job-name */
  ipp_jstate_t		state;		/* job-state value */
  unsigned		state_reasons;	/* job-state-reasons values */  
  ipp_attribute_t	*attrs;		/* Static attributes */
  _ipp_printer_t	*printer;	/* Printer */
  char			filename[1024];	/* Print file name */
  int			fd;		/* Print file descriptor */
} _ipp_job_t;

typedef struct _ipp_client_s		/**** Client data ****/
{
  http_t		http;		/* HTTP connection */
  ipp_t			*request,	/* IPP request */
			*response;	/* IPP response */
  _ipp_printer_t	*printer;	/* Printer */
  _ipp_job_t		*job;		/* Current job */
} _ipp_client_t;


/*
 * Local functions...
 */


/*
 * 'main()' - Main entry to the sample server.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  return (0);
}


/*
 * End of "$Id$".
 */
