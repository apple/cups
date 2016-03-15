/*
 * "$Id: ippserver.c 11097 2013-07-04 15:54:36Z msweet $"
 *
 *   Sample IPP/2.0 server for CUPS.
 *
 *   Copyright 2010-2013 by Apple Inc.
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
 *   main()			  - Main entry to the sample server.
 *   clean_jobs()		  - Clean out old (completed) jobs.
 *   compare_jobs()		  - Compare two jobs.
 *   copy_attributes()		  - Copy attributes from one request to
 *				    another.
 *   copy_job_attrs()		  - Copy job attributes to the response.
 *   create_client()		  - Accept a new network connection and create
 *				    a client object.
 *   create_job()		  - Create a new job object from a Print-Job or
 *				    Create-Job request.
 *   create_listener()		  - Create a listener socket.
 *   create_media_col() 	  - Create a media-col value.
 *   create_printer()		  - Create, register, and listen for
 *				    connections to a printer object.
 *   create_requested_array()	  - Create an array for requested-attributes.
 *   debug_attributes() 	  - Print attributes in a request or response.
 *   delete_client()		  - Close the socket and free all memory used
 *				    by a client object.
 *   delete_job()		  - Remove from the printer and free all memory
 *				    used by a job object.
 *   delete_printer()		  - Unregister, close listen sockets, and free
 *				    all memory used by a printer object.
 *   dnssd_callback()		  - Handle Bonjour registration events.
 *   find_job() 		  - Find a job specified in a request.
 *   html_escape()		  - Write a HTML-safe string.
 *   html_printf()		  - Send formatted text to the client, quoting
 *				    as needed.
 *   ipp_cancel_job()		  - Cancel a job.
 *   ipp_create_job()		  - Create a job object.
 *   ipp_get_job_attributes()	  - Get the attributes for a job object.
 *   ipp_get_jobs()		  - Get a list of job objects.
 *   ipp_get_printer_attributes() - Get the attributes for a printer object.
 *   ipp_print_job()		  - Create a job object with an attached
 *				    document.
 *   ipp_print_uri()		  - Create a job object with a referenced
 *				    document.
 *   ipp_send_document()	  - Add an attached document to a job object
 *				    created with Create-Job.
 *   ipp_send_uri()		  - Add a referenced document to a job object
 *				    created with Create-Job.
 *   ipp_validate_job() 	  - Validate job creation attributes.
 *   process_client()		  - Process client requests on a thread.
 *   process_http()		  - Process a HTTP request.
 *   process_ipp()		  - Process an IPP request.
 *   process_job()		  - Process a print job.
 *   register_printer() 	  - Register a printer object via Bonjour.
 *   respond_http()		  - Send a HTTP response.
 *   respond_ipp()		  - Send an IPP response.
 *   respond_unsupported()	  - Respond with an unsupported attribute.
 *   run_printer()		  - Run the printer service.
 *   usage()			  - Show program usage.
 *   valid_doc_attributes()	  - Determine whether the document attributes
 *				    are valid.
 *   valid_job_attributes()	  - Determine whether the job attributes are
 *				    valid.
 */

/*
 * Disable private and deprecated stuff so we can verify that the public API
 * is sufficient to implement a server.
 */

#define _IPP_PRIVATE_STRUCTURES 0	/* Disable private IPP stuff */
#define _CUPS_NO_DEPRECATED 1		/* Disable deprecated stuff */


/*
 * Include necessary headers...
 */

#include <cups/cups.h>			/* Public API */
#include <config.h>			/* CUPS configuration header */
#include <cups/string-private.h>	/* For string functions */
#include <cups/thread-private.h>	/* For multithreading functions */

#include <sys/wait.h>

#ifdef HAVE_DNSSD
#  include <dns_sd.h>
#endif /* HAVE_DNSSD */
#include <limits.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <poll.h>
#ifdef HAVE_SYS_MOUNT_H
#  include <sys/mount.h>
#endif /* HAVE_SYS_MOUNT_H */
#ifdef HAVE_SYS_STATFS_H
#  include <sys/statfs.h>
#endif /* HAVE_SYS_STATFS_H */
#ifdef HAVE_SYS_STATVFS_H
#  include <sys/statvfs.h>
#endif /* HAVE_SYS_STATVFS_H */
#ifdef HAVE_SYS_VFS_H
#  include <sys/vfs.h>
#endif /* HAVE_SYS_VFS_H */


/*
 * Constants...
 */

enum _ipp_preasons_e			/* printer-state-reasons bit values */
{
  _IPP_PSTATE_NONE = 0x0000,		/* none */
  _IPP_PSTATE_OTHER = 0x0001,		/* other */
  _IPP_PSTATE_COVER_OPEN = 0x0002,	/* cover-open */
  _IPP_PSTATE_INPUT_TRAY_MISSING = 0x0004,
					/* input-tray-missing */
  _IPP_PSTATE_MARKER_SUPPLY_EMPTY = 0x0008,
					/* marker-supply-empty */
  _IPP_PSTATE_MARKER_SUPPLY_LOW = 0x0010,
					/* marker-suply-low */
  _IPP_PSTATE_MARKER_WASTE_ALMOST_FULL = 0x0020,
					/* marker-waste-almost-full */
  _IPP_PSTATE_MARKER_WASTE_FULL = 0x0040,
					/* marker-waste-full */
  _IPP_PSTATE_MEDIA_EMPTY = 0x0080,	/* media-empty */
  _IPP_PSTATE_MEDIA_JAM = 0x0100,	/* media-jam */
  _IPP_PSTATE_MEDIA_LOW = 0x0200,	/* media-low */
  _IPP_PSTATE_MEDIA_NEEDED = 0x0400,	/* media-needed */
  _IPP_PSTATE_MOVING_TO_PAUSED = 0x0800,
					/* moving-to-paused */
  _IPP_PSTATE_PAUSED = 0x1000,		/* paused */
  _IPP_PSTATE_SPOOL_AREA_FULL = 0x2000,/* spool-area-full */
  _IPP_PSTATE_TONER_EMPTY = 0x4000,	/* toner-empty */
  _IPP_PSTATE_TONER_LOW = 0x8000	/* toner-low */
};
typedef unsigned int _ipp_preasons_t;	/* Bitfield for printer-state-reasons */

typedef enum _ipp_media_class_e
{
  _IPP_GENERAL,				/* General-purpose size */
  _IPP_PHOTO_ONLY,			/* Photo-only size */
  _IPP_ENV_ONLY				/* Envelope-only size */
} _ipp_media_class_t;

static const char * const media_supported[] =
{					/* media-supported values */
  "iso_a4_210x297mm",			/* A4 */
  "iso_a5_148x210mm",			/* A5 */
  "iso_a6_105x148mm",			/* A6 */
  "iso_dl_110x220mm",			/* DL */
  "na_legal_8.5x14in",			/* Legal */
  "na_letter_8.5x11in",			/* Letter */
  "na_number-10_4.125x9.5in",		/* #10 */
  "na_index-3x5_3x5in",			/* 3x5 */
  "oe_photo-l_3.5x5in",			/* L */
  "na_index-4x6_4x6in",			/* 4x6 */
  "na_5x7_5x7in"			/* 5x7 aka 2L */
};
static const int media_col_sizes[][3] =
{					/* media-col-database sizes */
  { 21000, 29700, _IPP_GENERAL },	/* A4 */
  { 14800, 21000, _IPP_PHOTO_ONLY },	/* A5 */
  { 10500, 14800, _IPP_PHOTO_ONLY },	/* A6 */
  { 11000, 22000, _IPP_ENV_ONLY },	/* DL */
  { 21590, 35560, _IPP_GENERAL },	/* Legal */
  { 21590, 27940, _IPP_GENERAL },	/* Letter */
  { 10477, 24130, _IPP_ENV_ONLY },	/* #10 */
  {  7630, 12700, _IPP_PHOTO_ONLY },	/* 3x5 */
  {  8890, 12700, _IPP_PHOTO_ONLY },	/* L */
  { 10160, 15240, _IPP_PHOTO_ONLY },	/* 4x6 */
  { 12700, 17780, _IPP_PHOTO_ONLY }	/* 5x7 aka 2L */
};
static const char * const media_type_supported[] =
				      /* media-type-supported values */
{
  "auto",
  "cardstock",
  "envelope",
  "labels",
  "other",
  "photographic-glossy",
  "photographic-high-gloss",
  "photographic-matte",
  "photographic-satin",
  "photographic-semi-gloss",
  "stationery",
  "stationery-letterhead",
  "transparency"
};


/*
 * Structures...
 */

typedef struct _ipp_job_s _ipp_job_t;

typedef struct _ipp_printer_s		/**** Printer data ****/
{
  int			ipv4,		/* IPv4 listener */
			ipv6;		/* IPv6 listener */
#ifdef HAVE_DNSSD
  DNSServiceRef		common_ref,	/* Shared service connection */
			ipp_ref,	/* Bonjour IPP service */
#  ifdef HAVE_SSL
			ipps_ref,	/* Bonjour IPPS service */
#  endif /* HAVE_SSL */
			http_ref,	/* Bonjour HTTP service */
			printer_ref;	/* Bonjour LPD service */
  TXTRecordRef		ipp_txt;	/* Bonjour IPP TXT record */
  char			*dnssd_name;	/* printer-dnssd-name */
#endif /* HAVE_DNSSD */
  char			*name,		/* printer-name */
			*icon,		/* Icon filename */
			*directory,	/* Spool directory */
			*hostname,	/* Hostname */
			*uri,		/* printer-uri-supported */
			*command;	/* Command to run with job file */
  int			port;		/* Port */
  size_t		urilen;		/* Length of printer URI */
  ipp_t			*attrs;		/* Static attributes */
  ipp_pstate_t		state;		/* printer-state value */
  _ipp_preasons_t	state_reasons;	/* printer-state-reasons values */
  cups_array_t		*jobs;		/* Jobs */
  _ipp_job_t		*active_job;	/* Current active/pending job */
  int			next_job_id;	/* Next job-id value */
  _cups_rwlock_t	rwlock;		/* Printer lock */
} _ipp_printer_t;

struct _ipp_job_s			/**** Job data ****/
{
  int			id;		/* Job ID */
  const char		*name,		/* job-name */
			*username,	/* job-originating-user-name */
			*format;	/* document-format */
  ipp_jstate_t		state;		/* job-state value */
  time_t		processing,	/* time-at-processing value */
			completed;	/* time-at-completed value */
  ipp_t			*attrs;		/* Static attributes */
  int			cancel;		/* Non-zero when job canceled */
  char			*filename;	/* Print file name */
  int			fd;		/* Print file descriptor */
  _ipp_printer_t	*printer;	/* Printer */
};

typedef struct _ipp_client_s		/**** Client data ****/
{
  http_t		*http;		/* HTTP connection */
  ipp_t			*request,	/* IPP request */
			*response;	/* IPP response */
  time_t		start;		/* Request start time */
  http_state_t		operation;	/* Request operation */
  ipp_op_t		operation_id;	/* IPP operation-id */
  char			uri[1024];	/* Request URI */
  http_addr_t		addr;		/* Client address */
  char			hostname[256];	/* Client hostname */
  _ipp_printer_t	*printer;	/* Printer */
  _ipp_job_t		*job;		/* Current job, if any */
} _ipp_client_t;


/*
 * Local functions...
 */

static void		clean_jobs(_ipp_printer_t *printer);
static int		compare_jobs(_ipp_job_t *a, _ipp_job_t *b);
static void		copy_attributes(ipp_t *to, ipp_t *from, cups_array_t *ra,
			                ipp_tag_t group_tag, int quickcopy);
static void		copy_job_attributes(_ipp_client_t *client,
			                    _ipp_job_t *job, cups_array_t *ra);
static _ipp_client_t	*create_client(_ipp_printer_t *printer, int sock);
static _ipp_job_t	*create_job(_ipp_client_t *client);
static int		create_listener(int family, int *port);
static ipp_t		*create_media_col(const char *media, const char *type,
					  int width, int length, int margins);
static ipp_t		*create_media_size(int width, int length);
static _ipp_printer_t	*create_printer(const char *servername,
			                const char *name, const char *location,
			                const char *make, const char *model,
					const char *icon,
					const char *docformats, int ppm,
					int ppm_color, int duplex, int port,
					int pin,
#ifdef HAVE_DNSSD
					const char *subtype,
#endif /* HAVE_DNSSD */
					const char *directory,
					const char *command);
static void		debug_attributes(const char *title, ipp_t *ipp,
			                 int response);
static void		delete_client(_ipp_client_t *client);
static void		delete_job(_ipp_job_t *job);
static void		delete_printer(_ipp_printer_t *printer);
#ifdef HAVE_DNSSD
static void		dnssd_callback(DNSServiceRef sdRef,
				       DNSServiceFlags flags,
				       DNSServiceErrorType errorCode,
				       const char *name,
				       const char *regtype,
				       const char *domain,
				       _ipp_printer_t *printer);
#endif /* HAVE_DNSSD */
static _ipp_job_t	*find_job(_ipp_client_t *client);
static void		html_escape(_ipp_client_t *client, const char *s,
			            size_t slen);
static void		html_printf(_ipp_client_t *client, const char *format,
			            ...) __attribute__((__format__(__printf__,
			                                           2, 3)));
static void		ipp_cancel_job(_ipp_client_t *client);
static void		ipp_create_job(_ipp_client_t *client);
static void		ipp_get_job_attributes(_ipp_client_t *client);
static void		ipp_get_jobs(_ipp_client_t *client);
static void		ipp_get_printer_attributes(_ipp_client_t *client);
static void		ipp_print_job(_ipp_client_t *client);
static void		ipp_print_uri(_ipp_client_t *client);
static void		ipp_send_document(_ipp_client_t *client);
static void		ipp_send_uri(_ipp_client_t *client);
static void		ipp_validate_job(_ipp_client_t *client);
static void		*process_client(_ipp_client_t *client);
static int		process_http(_ipp_client_t *client);
static int		process_ipp(_ipp_client_t *client);
static void		*process_job(_ipp_job_t *job);
#ifdef HAVE_DNSSD
static int		register_printer(_ipp_printer_t *printer,
			                 const char *location, const char *make,
					 const char *model, const char *formats,
					 const char *adminurl, int color,
					 int duplex, const char *regtype);
#endif /* HAVE_DNSSD */
static int		respond_http(_ipp_client_t *client, http_status_t code,
				     const char *content_coding,
				     const char *type, size_t length);
static void		respond_ipp(_ipp_client_t *client, ipp_status_t status,
			            const char *message, ...)
			__attribute__ ((__format__ (__printf__, 3, 4)));
static void		respond_unsupported(_ipp_client_t *client,
			                    ipp_attribute_t *attr);
static void		run_printer(_ipp_printer_t *printer);
static void		usage(int status) __attribute__((noreturn));
static int		valid_doc_attributes(_ipp_client_t *client);
static int		valid_job_attributes(_ipp_client_t *client);


/*
 * Globals...
 */

static int		KeepFiles = 0,
			Verbosity = 0;


/*
 * 'main()' - Main entry to the sample server.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  const char	*opt,			/* Current option character */
		*command = NULL,	/* Command to run with job files */
		*servername = NULL,	/* Server host name */
		*name = NULL,		/* Printer name */
		*location = "",		/* Location of printer */
		*make = "Test",		/* Manufacturer */
		*model = "Printer",	/* Model */
		*icon = "printer.png",	/* Icon file */
		*formats = "application/pdf,image/jpeg,image/pwg-raster";
	      				/* Supported formats */
#ifdef HAVE_DNSSD
  const char	*subtype = "_print";	/* Bonjour service subtype */
#endif /* HAVE_DNSSD */
  int		port = 8631,		/* Port number (0 = auto) */
		duplex = 0,		/* Duplex mode */
		ppm = 10,		/* Pages per minute for mono */
		ppm_color = 0,		/* Pages per minute for color */
		pin = 0;		/* PIN printing mode? */
  char		directory[1024] = "";	/* Spool directory */
  _ipp_printer_t *printer;		/* Printer object */


 /*
  * Parse command-line arguments...
  */

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
        switch (*opt)
	{
	  case '2' : /* -2 (enable 2-sided printing) */
	      duplex = 1;
	      break;

	  case 'M' : /* -M manufacturer */
	      i ++;
	      if (i >= argc)
	        usage(1);
	      make = argv[i];
	      break;

          case 'P' : /* -P (PIN printing mode) */
              pin = 1;
              break;

          case 'c' : /* -c command */
              i ++;
	      if (i >= argc)
	        usage(1);

	      command = argv[i];
	      break;

	  case 'd' : /* -d spool-directory */
	      i ++;
	      if (i >= argc)
	        usage(1);
	      strlcpy(directory, argv[i], sizeof(directory));
	      break;

	  case 'f' : /* -f type/subtype[,...] */
	      i ++;
	      if (i >= argc)
	        usage(1);
	      formats = argv[i];
	      break;

          case 'h' : /* -h (show help) */
	      usage(0);
	      break;

	  case 'i' : /* -i icon.png */
	      i ++;
	      if (i >= argc)
	        usage(1);
	      icon = argv[i];
	      break;

	  case 'k' : /* -k (keep files) */
	      KeepFiles = 1;
	      break;

	  case 'l' : /* -l location */
	      i ++;
	      if (i >= argc)
	        usage(1);
	      location = argv[i];
	      break;

	  case 'm' : /* -m model */
	      i ++;
	      if (i >= argc)
	        usage(1);
	      model = argv[i];
	      break;

	  case 'n' : /* -n hostname */
	      i ++;
	      if (i >= argc)
	        usage(1);
	      servername = argv[i];
	      break;

	  case 'p' : /* -p port */
	      i ++;
	      if (i >= argc || !isdigit(argv[i][0] & 255))
	        usage(1);
	      port = atoi(argv[i]);
	      break;

#ifdef HAVE_DNSSD
	  case 'r' : /* -r subtype */
	      i ++;
	      if (i >= argc)
	        usage(1);
	      subtype = argv[i];
	      break;
#endif /* HAVE_DNSSD */

	  case 's' : /* -s speed[,color-speed] */
	      i ++;
	      if (i >= argc)
	        usage(1);
	      if (sscanf(argv[i], "%d,%d", &ppm, &ppm_color) < 1)
	        usage(1);
	      break;

	  case 'v' : /* -v (be verbose) */
	      Verbosity ++;
	      break;

          default : /* Unknown */
	      fprintf(stderr, "Unknown option \"-%c\".\n", *opt);
	      usage(1);
	      break;
	}
    }
    else if (!name)
    {
      name = argv[i];
    }
    else
    {
      fprintf(stderr, "Unexpected command-line argument \"%s\"\n", argv[i]);
      usage(1);
    }

  if (!name)
    usage(1);

 /*
  * Apply defaults as needed...
  */

  if (!directory[0])
  {
    snprintf(directory, sizeof(directory), "/tmp/ippserver.%d", (int)getpid());

    if (mkdir(directory, 0777) && errno != EEXIST)
    {
      fprintf(stderr, "Unable to create spool directory \"%s\": %s\n",
	      directory, strerror(errno));
      usage(1);
    }

    if (Verbosity)
      fprintf(stderr, "Using spool directory \"%s\".\n", directory);
  }

 /*
  * Create the printer...
  */

  if ((printer = create_printer(servername, name, location, make, model, icon,
                                formats, ppm, ppm_color, duplex, port, pin,
#ifdef HAVE_DNSSD
				subtype,
#endif /* HAVE_DNSSD */
				directory, command)) == NULL)
    return (1);

 /*
  * Run the print service...
  */

  run_printer(printer);

 /*
  * Destroy the printer and exit...
  */

  delete_printer(printer);

  return (0);
}


/*
 * 'clean_jobs()' - Clean out old (completed) jobs.
 */

static void
clean_jobs(_ipp_printer_t *printer)	/* I - Printer */
{
  _ipp_job_t	*job;			/* Current job */
  time_t	cleantime;		/* Clean time */


  if (cupsArrayCount(printer->jobs) == 0)
    return;

  cleantime = time(NULL) - 60;

  _cupsRWLockWrite(&(printer->rwlock));
  for (job = (_ipp_job_t *)cupsArrayFirst(printer->jobs);
       job;
       job = (_ipp_job_t *)cupsArrayNext(printer->jobs))
    if (job->completed && job->completed < cleantime)
    {
      cupsArrayRemove(printer->jobs, job);
      delete_job(job);
    }
    else
      break;
  _cupsRWUnlock(&(printer->rwlock));
}


/*
 * 'compare_jobs()' - Compare two jobs.
 */

static int				/* O - Result of comparison */
compare_jobs(_ipp_job_t *a,		/* I - First job */
             _ipp_job_t *b)		/* I - Second job */
{
  return (b->id - a->id);
}


/*
 * 'copy_attributes()' - Copy attributes from one request to another.
 */

static void
copy_attributes(ipp_t        *to,	/* I - Destination request */
	        ipp_t        *from,	/* I - Source request */
	        cups_array_t *ra,	/* I - Requested attributes */
	        ipp_tag_t    group_tag,	/* I - Group to copy */
	        int          quickcopy)	/* I - Do a quick copy? */
{
  ipp_attribute_t	*fromattr;	/* Source attribute */


  if (!to || !from)
    return;

  for (fromattr = ippFirstAttribute(from);
       fromattr;
       fromattr = ippNextAttribute(from))
  {
   /*
    * Filter attributes as needed...
    */

    ipp_tag_t fromgroup = ippGetGroupTag(fromattr);
    const char *fromname = ippGetName(fromattr);

    if ((group_tag != IPP_TAG_ZERO && fromgroup != group_tag &&
         fromgroup != IPP_TAG_ZERO) || !fromname)
      continue;

    if (!ra || cupsArrayFind(ra, (void *)fromname))
      ippCopyAttribute(to, fromattr, quickcopy);
  }
}


/*
 * 'copy_job_attrs()' - Copy job attributes to the response.
 */

static void
copy_job_attributes(
    _ipp_client_t *client,		/* I - Client */
    _ipp_job_t    *job,			/* I - Job */
    cups_array_t  *ra)			/* I - requested-attributes */
{
  copy_attributes(client->response, job->attrs, ra, IPP_TAG_JOB, 0);

  if (!ra || cupsArrayFind(ra, "job-printer-up-time"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER,
                  "job-printer-up-time", (int)time(NULL));

  if (!ra || cupsArrayFind(ra, "job-state"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_ENUM,
		  "job-state", job->state);

  if (!ra || cupsArrayFind(ra, "job-state-reasons"))
  {
    switch (job->state)
    {
      case IPP_JSTATE_PENDING :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST, "job-state-reasons",
		       NULL, "none");
	  break;

      case IPP_JSTATE_HELD :
          if (job->fd >= 0)
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
	                 "job-state-reasons", NULL, "job-incoming");
	  else if (ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_ZERO))
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
	                 "job-state-reasons", NULL, "job-hold-until-specified");
          else
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
	                 "job-state-reasons", NULL, "job-data-insufficient");
	  break;

      case IPP_JSTATE_PROCESSING :
	  if (job->cancel)
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
	                 "job-state-reasons", NULL, "processing-to-stop-point");
	  else
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
	                 "job-state-reasons", NULL, "job-printing");
	  break;

      case IPP_JSTATE_STOPPED :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST, "job-state-reasons",
		       NULL, "job-stopped");
	  break;

      case IPP_JSTATE_CANCELED :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST, "job-state-reasons",
		       NULL, "job-canceled-by-user");
	  break;

      case IPP_JSTATE_ABORTED :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST, "job-state-reasons",
		       NULL, "aborted-by-system");
	  break;

      case IPP_JSTATE_COMPLETED :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST, "job-state-reasons",
		       NULL, "job-completed-successfully");
	  break;
    }
  }

  if (!ra || cupsArrayFind(ra, "time-at-completed"))
    ippAddInteger(client->response, IPP_TAG_JOB,
                  job->completed ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE,
                  "time-at-completed", job->completed);

  if (!ra || cupsArrayFind(ra, "time-at-processing"))
    ippAddInteger(client->response, IPP_TAG_JOB,
                  job->processing ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE,
                  "time-at-processing", job->processing);
}


/*
 * 'create_client()' - Accept a new network connection and create a client
 *                     object.
 */

static _ipp_client_t *			/* O - Client */
create_client(_ipp_printer_t *printer,	/* I - Printer */
              int            sock)	/* I - Listen socket */
{
  _ipp_client_t	*client;		/* Client */


  if ((client = calloc(1, sizeof(_ipp_client_t))) == NULL)
  {
    perror("Unable to allocate memory for client");
    return (NULL);
  }

  client->printer = printer;

 /*
  * Accept the client and get the remote address...
  */

  if ((client->http = httpAcceptConnection(sock, 1)) == NULL)
  {
    perror("Unable to accept client connection");

    free(client);

    return (NULL);
  }

  httpGetHostname(client->http, client->hostname, sizeof(client->hostname));

  if (Verbosity)
    fprintf(stderr, "Accepted connection from %s\n", client->hostname);

  return (client);
}


/*
 * 'create_job()' - Create a new job object from a Print-Job or Create-Job
 *                  request.
 */

static _ipp_job_t *			/* O - Job */
create_job(_ipp_client_t *client)	/* I - Client */
{
  _ipp_job_t		*job;		/* Job */
  ipp_attribute_t	*attr;		/* Job attribute */
  char			uri[1024];	/* job-uri value */


  _cupsRWLockWrite(&(client->printer->rwlock));
  if (client->printer->active_job &&
      client->printer->active_job->state < IPP_JSTATE_CANCELED)
  {
   /*
    * Only accept a single job at a time...
    */

    _cupsRWLockWrite(&(client->printer->rwlock));
    return (NULL);
  }

 /*
  * Allocate and initialize the job object...
  */

  if ((job = calloc(1, sizeof(_ipp_job_t))) == NULL)
  {
    perror("Unable to allocate memory for job");
    return (NULL);
  }

  job->printer    = client->printer;
  job->attrs      = client->request;
  job->state      = IPP_JSTATE_HELD;
  job->fd         = -1;
  client->request = NULL;

 /*
  * Set all but the first two attributes to the job attributes group...
  */

  for (ippFirstAttribute(job->attrs),
           ippNextAttribute(job->attrs),
           attr = ippNextAttribute(job->attrs);
       attr;
       attr = ippNextAttribute(job->attrs))
    ippSetGroupTag(job->attrs, &attr, IPP_TAG_JOB);

 /*
  * Get the requesting-user-name, document format, and priority...
  */

  if ((attr = ippFindAttribute(job->attrs, "requesting-user-name",
                               IPP_TAG_NAME)) != NULL)
    ippSetName(job->attrs, &attr, "job-originating-user-name");
  else
    attr = ippAddString(job->attrs, IPP_TAG_JOB,
                        IPP_TAG_NAME | IPP_TAG_CUPS_CONST,
                        "job-originating-user-name", NULL, "anonymous");

  if (attr)
    job->username = ippGetString(attr, 0, NULL);
  else
    job->username = "anonymous";

  if ((attr = ippFindAttribute(job->attrs, "document-format",
                               IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else
    job->format = "application/octet-stream";

 /*
  * Add job description attributes and add to the jobs array...
  */

  job->id = client->printer->next_job_id ++;

  snprintf(uri, sizeof(uri), "%s/%d", client->printer->uri, job->id);

  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", job->id);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-uri", NULL, uri);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-printer-uri", NULL,
               client->printer->uri);
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "time-at-creation",
                (int)time(NULL));

  cupsArrayAdd(client->printer->jobs, job);
  client->printer->active_job = job;

  _cupsRWUnlock(&(client->printer->rwlock));

  return (job);
}


/*
 * 'create_listener()' - Create a listener socket.
 */

static int				/* O  - Listener socket or -1 on error */
create_listener(int family,		/* I  - Address family */
                int *port)		/* IO - Port number */
{
  int			sock;		/* Listener socket */
  http_addrlist_t	*addrlist;	/* Listen address */
  char			service[255];	/* Service port */


  if (!*port)
  {
    *port = 8000 + (getuid() % 1000);
    fprintf(stderr, "Listening on port %d.\n", *port);
  }

  snprintf(service, sizeof(service), "%d", *port);
  if ((addrlist = httpAddrGetList(NULL, family, service)) == NULL)
    return (-1);

  sock = httpAddrListen(&(addrlist->addr), *port);

  httpAddrFreeList(addrlist);

  return (sock);
}


/*
 * 'create_media_col()' - Create a media-col value.
 */

static ipp_t *				/* O - media-col collection */
create_media_col(const char *media,	/* I - Media name */
		 const char *type,	/* I - Nedua type */
		 int        width,	/* I - x-dimension in 2540ths */
		 int        length,	/* I - y-dimension in 2540ths */
		 int        margins)	/* I - Value for margins */
{
  ipp_t	*media_col = ippNew(),		/* media-col value */
	*media_size = create_media_size(width, length);
					/* media-size value */
  char	media_key[256];			/* media-key value */


  snprintf(media_key, sizeof(media_key), "%s_%s%s", media, type,
           margins == 0 ? "_borderless" : "");

  ippAddString(media_col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-key", NULL,
               media_key);
  ippAddCollection(media_col, IPP_TAG_PRINTER, "media-size", media_size);
  ippAddInteger(media_col, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "media-bottom-margin", margins);
  ippAddInteger(media_col, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "media-left-margin", margins);
  ippAddInteger(media_col, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "media-right-margin", margins);
  ippAddInteger(media_col, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "media-top-margin", margins);
  ippAddString(media_col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-type",
               NULL, type);

  ippDelete(media_size);

  return (media_col);
}


/*
 * 'create_media_size()' - Create a media-size value.
 */

static ipp_t *				/* O - media-col collection */
create_media_size(int width,		/* I - x-dimension in 2540ths */
		  int length)		/* I - y-dimension in 2540ths */
{
  ipp_t	*media_size = ippNew();		/* media-size value */


  ippAddInteger(media_size, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "x-dimension",
                width);
  ippAddInteger(media_size, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "y-dimension",
                length);

  return (media_size);
}


/*
 * 'create_printer()' - Create, register, and listen for connections to a
 *                      printer object.
 */

static _ipp_printer_t *			/* O - Printer */
create_printer(const char *servername,	/* I - Server hostname (NULL for default) */
               const char *name,	/* I - printer-name */
	       const char *location,	/* I - printer-location */
	       const char *make,	/* I - printer-make-and-model */
	       const char *model,	/* I - printer-make-and-model */
	       const char *icon,	/* I - printer-icons */
	       const char *docformats,	/* I - document-format-supported */
	       int        ppm,		/* I - Pages per minute in grayscale */
	       int        ppm_color,	/* I - Pages per minute in color (0 for gray) */
	       int        duplex,	/* I - 1 = duplex, 0 = simplex */
	       int        port,		/* I - Port for listeners or 0 for auto */
	       int        pin,		/* I - Require PIN printing */
#ifdef HAVE_DNSSD
	       const char *subtype,	/* I - Bonjour service subtype */
#endif /* HAVE_DNSSD */
	       const char *directory,	/* I - Spool directory */
	       const char *command)	/* I - Command to run on job files */
{
  int			i, j;		/* Looping vars */
  _ipp_printer_t	*printer;	/* Printer */
  char			hostname[256],	/* Hostname */
			uri[1024],	/* Printer URI */
			icons[1024],	/* printer-icons URI */
			adminurl[1024],	/* printer-more-info URI */
			device_id[1024],/* printer-device-id */
			make_model[128];/* printer-make-and-model */
  int			num_formats;	/* Number of document-format-supported values */
  char			*defformat,	/* document-format-default value */
			*formats[100],	/* document-format-supported values */
			*ptr;		/* Pointer into string */
  const char		*prefix;	/* Prefix string */
  int			num_database;	/* Number of database values */
  ipp_attribute_t	*media_col_database,
					/* media-col-database value */
			*media_size_supported;
					/* media-size-supported value */
  ipp_t			*media_col_default;
					/* media-col-default value */
  int			media_col_index;/* Current media-col-database value */
  int			k_supported;	/* Maximum file size supported */
#ifdef HAVE_STATVFS
  struct statvfs	spoolinfo;	/* FS info for spool directory */
  double		spoolsize;	/* FS size */
#elif defined(HAVE_STATFS)
  struct statfs		spoolinfo;	/* FS info for spool directory */
  double		spoolsize;	/* FS size */
#endif /* HAVE_STATVFS */
  static const int	orients[4] =	/* orientation-requested-supported values */
  {
    IPP_ORIENT_PORTRAIT,
    IPP_ORIENT_LANDSCAPE,
    IPP_ORIENT_REVERSE_LANDSCAPE,
    IPP_ORIENT_REVERSE_PORTRAIT
  };
  static const char * const versions[] =/* ipp-versions-supported values */
  {
    "1.0",
    "1.1",
    "2.0"
  };
  static const int	ops[] =		/* operations-supported values */
  {
    IPP_OP_PRINT_JOB,
    IPP_OP_PRINT_URI,
    IPP_OP_VALIDATE_JOB,
    IPP_OP_CREATE_JOB,
    IPP_OP_SEND_DOCUMENT,
    IPP_OP_SEND_URI,
    IPP_OP_CANCEL_JOB,
    IPP_OP_GET_JOB_ATTRIBUTES,
    IPP_OP_GET_JOBS,
    IPP_OP_GET_PRINTER_ATTRIBUTES
  };
  static const char * const charsets[] =/* charset-supported values */
  {
    "us-ascii",
    "utf-8"
  };
  static const char * const compressions[] =/* compression-supported values */
  {
#ifdef HAVE_LIBZ
    "deflate",
    "gzip",
#endif /* HAVE_LIBZ */
    "none"
  };
  static const char * const job_creation[] =
  {					/* job-creation-attributes-supported values */
    "copies",
    "ipp-attribute-fidelity",
    "job-account-id",
    "job-accounting-user-id",
    "job-name",
    "job-password",
    "job-priority",
    "media",
    "media-col",
    "multiple-document-handling",
    "orientation-requested",
    "print-quality",
    "sides"
  };
  static const char * const media_col_supported[] =
  {					/* media-col-supported values */
    "media-bottom-margin",
    "media-left-margin",
    "media-right-margin",
    "media-size",
    "media-top-margin",
    "media-type"
  };
  static const int	media_xxx_margin_supported[] =
  {					/* media-xxx-margin-supported values */
    0,
    635
  };
  static const char * const multiple_document_handling[] =
  {					/* multiple-document-handling-supported values */
    "separate-documents-uncollated-copies",
    "separate-documents-collated-copies"
  };
  static const int	print_quality_supported[] =
  {					/* print-quality-supported values */
    IPP_QUALITY_DRAFT,
    IPP_QUALITY_NORMAL,
    IPP_QUALITY_HIGH
  };
  static const int	pwg_raster_document_resolution_supported[] =
  {
    150,
    300,
    600
  };
  static const char * const pwg_raster_document_type_supported[] =
  {
    "black-1",
    "cmyk-8",
    "sgray-8",
    "srgb-8",
    "srgb-16"
  };
  static const char * const reference_uri_schemes_supported[] =
  {					/* reference-uri-schemes-supported */
    "file",
    "ftp",
    "http"
#ifdef HAVE_SSL
    , "https"
#endif /* HAVE_SSL */
  };
  static const char * const sides_supported[] =
  {					/* sides-supported values */
    "one-sided",
    "two-sided-long-edge",
    "two-sided-short-edge"
  };
  static const char * const which_jobs[] =
  {					/* which-jobs-supported values */
    "completed",
    "not-completed",
    "aborted",
    "all",
    "canceled",
    "pending",
    "pending-held",
    "processing",
    "processing-stopped"
  };


 /*
  * Allocate memory for the printer...
  */

  if ((printer = calloc(1, sizeof(_ipp_printer_t))) == NULL)
  {
    perror("Unable to allocate memory for printer");
    return (NULL);
  }

  printer->ipv4          = -1;
  printer->ipv6          = -1;
  printer->name          = strdup(name);
#ifdef HAVE_DNSSD
  printer->dnssd_name    = strdup(printer->name);
#endif /* HAVE_DNSSD */
  printer->command       = command ? strdup(command) : NULL;
  printer->directory     = strdup(directory);
  printer->hostname      = strdup(servername ? servername :
                                             httpGetHostname(NULL, hostname,
                                                             sizeof(hostname)));
  printer->port          = port;
  printer->state         = IPP_PSTATE_IDLE;
  printer->state_reasons = _IPP_PSTATE_NONE;
  printer->jobs          = cupsArrayNew((cups_array_func_t)compare_jobs, NULL);
  printer->next_job_id   = 1;

  httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
		  printer->hostname, printer->port, "/ipp/print");
  printer->uri    = strdup(uri);
  printer->urilen = strlen(uri);

  if (icon)
    printer->icon = strdup(icon);

  _cupsRWInit(&(printer->rwlock));

 /*
  * Create the listener sockets...
  */

  if ((printer->ipv4 = create_listener(AF_INET, &(printer->port))) < 0)
  {
    perror("Unable to create IPv4 listener");
    goto bad_printer;
  }

  if ((printer->ipv6 = create_listener(AF_INET6, &(printer->port))) < 0)
  {
    perror("Unable to create IPv6 listener");
    goto bad_printer;
  }

 /*
  * Prepare values for the printer attributes...
  */

  httpAssembleURI(HTTP_URI_CODING_ALL, icons, sizeof(icons), "http", NULL,
                  printer->hostname, printer->port, "/icon.png");
  httpAssembleURI(HTTP_URI_CODING_ALL, adminurl, sizeof(adminurl), "http", NULL,
                  printer->hostname, printer->port, "/");

  if (Verbosity)
  {
    fprintf(stderr, "printer-more-info=\"%s\"\n", adminurl);
    fprintf(stderr, "printer-uri=\"%s\"\n", uri);
  }

  snprintf(make_model, sizeof(make_model), "%s %s", make, model);

  num_formats = 1;
  formats[0]  = strdup(docformats);
  defformat   = formats[0];
  for (ptr = strchr(formats[0], ','); ptr; ptr = strchr(ptr, ','))
  {
    *ptr++ = '\0';
    formats[num_formats++] = ptr;

    if (!_cups_strcasecmp(ptr, "application/octet-stream"))
      defformat = ptr;
  }

  snprintf(device_id, sizeof(device_id), "MFG:%s;MDL:%s;", make, model);
  ptr    = device_id + strlen(device_id);
  prefix = "CMD:";
  for (i = 0; i < num_formats; i ++)
  {
    if (!_cups_strcasecmp(formats[i], "application/pdf"))
      snprintf(ptr, sizeof(device_id) - (ptr - device_id), "%sPDF", prefix);
    else if (!_cups_strcasecmp(formats[i], "application/postscript"))
      snprintf(ptr, sizeof(device_id) - (ptr - device_id), "%sPS", prefix);
    else if (!_cups_strcasecmp(formats[i], "application/vnd.hp-PCL"))
      snprintf(ptr, sizeof(device_id) - (ptr - device_id), "%sPCL", prefix);
    else if (!_cups_strcasecmp(formats[i], "image/jpeg"))
      snprintf(ptr, sizeof(device_id) - (ptr - device_id), "%sJPEG", prefix);
    else if (!_cups_strcasecmp(formats[i], "image/png"))
      snprintf(ptr, sizeof(device_id) - (ptr - device_id), "%sPNG", prefix);
    else if (_cups_strcasecmp(formats[i], "application/octet-stream"))
      snprintf(ptr, sizeof(device_id) - (ptr - device_id), "%s%s", prefix,
               formats[i]);

    ptr += strlen(ptr);
    prefix = ",";
  }
  strlcat(device_id, ";", sizeof(device_id));

 /*
  * Get the maximum spool size based on the size of the filesystem used for
  * the spool directory.  If the host OS doesn't support the statfs call
  * or the filesystem is larger than 2TiB, always report INT_MAX.
  */

#ifdef HAVE_STATVFS
  if (statvfs(printer->directory, &spoolinfo))
    k_supported = INT_MAX;
  else if ((spoolsize = (double)spoolinfo.f_frsize *
                        spoolinfo.f_blocks / 1024) > INT_MAX)
    k_supported = INT_MAX;
  else
    k_supported = (int)spoolsize;

#elif defined(HAVE_STATFS)
  if (statfs(printer->directory, &spoolinfo))
    k_supported = INT_MAX;
  else if ((spoolsize = (double)spoolinfo.f_bsize *
                        spoolinfo.f_blocks / 1024) > INT_MAX)
    k_supported = INT_MAX;
  else
    k_supported = (int)spoolsize;

#else
  k_supported = INT_MAX;
#endif /* HAVE_STATVFS */

 /*
  * Create the printer attributes.  This list of attributes is sorted to improve
  * performance when the client provides a requested-attributes attribute...
  */

  printer->attrs = ippNew();

  /* charset-configured */
  ippAddString(printer->attrs, IPP_TAG_PRINTER,
               IPP_TAG_CHARSET | IPP_TAG_CUPS_CONST,
               "charset-configured", NULL, "utf-8");

  /* charset-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER,
                IPP_TAG_CHARSET | IPP_TAG_CUPS_CONST,
                "charset-supported", sizeof(charsets) / sizeof(charsets[0]),
		NULL, charsets);

  /* color-supported */
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "color-supported",
                ppm_color > 0);

  /* compression-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER,
                IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
	        "compression-supported",
	        (int)(sizeof(compressions) / sizeof(compressions[0])), NULL,
	        compressions);

  /* copies-default */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "copies-default", 1);

  /* copies-supported */
  ippAddRange(printer->attrs, IPP_TAG_PRINTER, "copies-supported", 1, 999);

  /* document-format-default */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_MIMETYPE,
               "document-format-default", NULL, defformat);

  /* document-format-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_MIMETYPE,
                "document-format-supported", num_formats, NULL,
		(const char * const *)formats);

  /* finishings-default */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                "finishings-default", IPP_FINISHINGS_NONE);

  /* finishings-supported */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                "finishings-supported", IPP_FINISHINGS_NONE);

  /* generated-natural-language-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER,
               IPP_TAG_LANGUAGE | IPP_TAG_CUPS_CONST,
               "generated-natural-language-supported", NULL, "en");

  /* ipp-versions-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER,
                IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
                "ipp-versions-supported",
		sizeof(versions) / sizeof(versions[0]), NULL, versions);

  /* job-account-id-supported */
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "job-account-id-supported", 1);

  /* job-accounting-user-id-supported */
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER,
                "job-accounting-user-id-supported", 1);

  /* job-creation-attributes-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER,
                IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
                "job-creation-attributes-supported",
		sizeof(job_creation) / sizeof(job_creation[0]),
		NULL, job_creation);

  /* job-k-octets-supported */
  ippAddRange(printer->attrs, IPP_TAG_PRINTER, "job-k-octets-supported", 0,
	      k_supported);

  /* job-password-supported */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "job-password-supported", 4);

  /* job-priority-default */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "job-priority-default", 50);

  /* job-priority-supported */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "job-priority-supported", 100);

  /* job-sheets-default */
  ippAddString(printer->attrs, IPP_TAG_PRINTER,
               IPP_TAG_NAME | IPP_TAG_CUPS_CONST,
               "job-sheets-default", NULL, "none");

  /* job-sheets-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER,
               IPP_TAG_NAME | IPP_TAG_CUPS_CONST,
               "job-sheets-supported", NULL, "none");

  /* media-bottom-margin-supported */
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                 "media-bottom-margin-supported",
		 (int)(sizeof(media_xxx_margin_supported) /
		       sizeof(media_xxx_margin_supported[0])),
		 media_xxx_margin_supported);

  /* media-col-database */
  for (num_database = 0, i = 0;
       i < (int)(sizeof(media_col_sizes) / sizeof(media_col_sizes[0]));
       i ++)
  {
    if (media_col_sizes[i][2] == _IPP_ENV_ONLY)
      num_database += 2;		/* auto + envelope */
    else if (media_col_sizes[i][2] == _IPP_PHOTO_ONLY)
      num_database += 12;		/* auto + photographic-* + borderless */
    else
      num_database += (int)(sizeof(media_type_supported) /
                            sizeof(media_type_supported[0])) + 6;
					/* All types + borderless */
  }

  media_col_database = ippAddCollections(printer->attrs, IPP_TAG_PRINTER,
                                         "media-col-database", num_database,
					 NULL);
  for (media_col_index = 0, i = 0;
       i < (int)(sizeof(media_col_sizes) / sizeof(media_col_sizes[0]));
       i ++)
  {
    for (j = 0;
         j < (int)(sizeof(media_type_supported) /
	           sizeof(media_type_supported[0]));
         j ++)
    {
      if (media_col_sizes[i][2] == _IPP_ENV_ONLY &&
          strcmp(media_type_supported[j], "auto") &&
	  strcmp(media_type_supported[j], "envelope"))
	continue;
      else if (media_col_sizes[i][2] == _IPP_PHOTO_ONLY &&
               strcmp(media_type_supported[j], "auto") &&
	       strncmp(media_type_supported[j], "photographic-", 13))
	continue;

      ippSetCollection(printer->attrs, &media_col_database, media_col_index,
                       create_media_col(media_supported[i],
                                        media_type_supported[j],
					media_col_sizes[i][0],
					media_col_sizes[i][1],
					media_xxx_margin_supported[1]));
      media_col_index ++;

      if (media_col_sizes[i][2] != _IPP_ENV_ONLY &&
	  (!strcmp(media_type_supported[j], "auto") ||
	   !strncmp(media_type_supported[j], "photographic-", 13)))
      {
       /*
        * Add borderless version for this combination...
	*/

	ippSetCollection(printer->attrs, &media_col_database, media_col_index,
                         create_media_col(media_supported[i],
                                          media_type_supported[j],
					  media_col_sizes[i][0],
					  media_col_sizes[i][1],
					  media_xxx_margin_supported[0]));
	media_col_index ++;
      }
    }
  }

  /* media-col-default */
  media_col_default = create_media_col(media_supported[0],
                                       media_type_supported[0],
				       media_col_sizes[0][0],
				       media_col_sizes[0][1],
				       media_xxx_margin_supported[1]);

  ippAddCollection(printer->attrs, IPP_TAG_PRINTER, "media-col-default",
                   media_col_default);
  ippDelete(media_col_default);

  /* media-col-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER,
                IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
                "media-col-supported",
		(int)(sizeof(media_col_supported) /
		      sizeof(media_col_supported[0])), NULL,
		media_col_supported);

  /* media-default */
  ippAddString(printer->attrs, IPP_TAG_PRINTER,
               IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
               "media-default", NULL, media_supported[0]);

  /* media-left-margin-supported */
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                 "media-left-margin-supported",
		 (int)(sizeof(media_xxx_margin_supported) /
		       sizeof(media_xxx_margin_supported[0])),
		 media_xxx_margin_supported);

  /* media-right-margin-supported */
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                 "media-right-margin-supported",
		 (int)(sizeof(media_xxx_margin_supported) /
		       sizeof(media_xxx_margin_supported[0])),
		 media_xxx_margin_supported);

  /* media-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER,
                IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
                "media-supported",
		(int)(sizeof(media_supported) / sizeof(media_supported[0])),
		NULL, media_supported);

  /* media-size-supported */
  media_size_supported = ippAddCollections(printer->attrs, IPP_TAG_PRINTER,
                                           "media-size-supported",
                                           (int)(sizeof(media_col_sizes) /
                                                 sizeof(media_col_sizes[0])),
                                           NULL);
  for (i = 0;
       i < (int)(sizeof(media_col_sizes) / sizeof(media_col_sizes[0]));
       i ++)
    ippSetCollection(printer->attrs, &media_size_supported, i,
		     create_media_size(media_col_sizes[i][0],
		                       media_col_sizes[i][1]));

  /* media-top-margin-supported */
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                 "media-top-margin-supported",
		 (int)(sizeof(media_xxx_margin_supported) /
		       sizeof(media_xxx_margin_supported[0])),
		 media_xxx_margin_supported);

  /* media-type-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER,
                IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
                "media-type-supported",
		(int)(sizeof(media_type_supported) /
		      sizeof(media_type_supported[0])),
		NULL, media_type_supported);

  /* multiple-document-handling-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER,
                IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
                "multiple-document-handling-supported",
                sizeof(multiple_document_handling) /
		    sizeof(multiple_document_handling[0]), NULL,
	        multiple_document_handling);

  /* multiple-document-jobs-supported */
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER,
                "multiple-document-jobs-supported", 0);

  /* natural-language-configured */
  ippAddString(printer->attrs, IPP_TAG_PRINTER,
               IPP_TAG_LANGUAGE | IPP_TAG_CUPS_CONST,
               "natural-language-configured", NULL, "en");

  /* number-up-default */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "number-up-default", 1);

  /* number-up-supported */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "number-up-supported", 1);

  /* operations-supported */
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM,
		 "operations-supported", sizeof(ops) / sizeof(ops[0]), ops);

  /* orientation-requested-default */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_NOVALUE,
                "orientation-requested-default", 0);

  /* orientation-requested-supported */
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                 "orientation-requested-supported", 4, orients);

  /* output-bin-default */
  ippAddString(printer->attrs, IPP_TAG_PRINTER,
               IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
               "output-bin-default", NULL, "face-down");

  /* output-bin-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER,
               IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
               "output-bin-supported", NULL, "face-down");

  /* pages-per-minute */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "pages-per-minute", ppm);

  /* pages-per-minute-color */
  if (ppm_color > 0)
    ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                  "pages-per-minute-color", ppm_color);

  /* pdl-override-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER,
               IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
               "pdl-override-supported", NULL, "attempted");

  /* print-quality-default */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                "print-quality-default", IPP_QUALITY_NORMAL);

  /* print-quality-supported */
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                 "print-quality-supported",
		 (int)(sizeof(print_quality_supported) /
		       sizeof(print_quality_supported[0])),
		 print_quality_supported);

  /* printer-device-id */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
	       "printer-device-id", NULL, device_id);

  /* printer-icons */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI,
               "printer-icons", NULL, icons);

  /* printer-is-accepting-jobs */
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "printer-is-accepting-jobs",
                1);

  /* printer-info */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info",
               NULL, name);

  /* printer-location */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
               "printer-location", NULL, location);

  /* printer-make-and-model */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
               "printer-make-and-model", NULL, make_model);

  /* printer-mandatory-job-attributes */
  if (pin)
  {
    static const char * const names[] =
    {
      "job-accounting-user-id",
      "job-password"
    };

    ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                  "printer-mandatory-job-attributes",
                  (int)(sizeof(names) / sizeof(names[0])), NULL, names);
  }

  /* printer-more-info */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI,
               "printer-more-info", NULL, adminurl);

  /* printer-name */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-name",
               NULL, name);

  /* printer-resolution-default */
  ippAddResolution(printer->attrs, IPP_TAG_PRINTER,
                   "printer-resolution-default", IPP_RES_PER_INCH, 600, 600);

  /* printer-resolution-supported */
  ippAddResolution(printer->attrs, IPP_TAG_PRINTER,
                   "printer-resolution-supported", IPP_RES_PER_INCH, 600, 600);

  /* printer-uri-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI,
               "printer-uri-supported", NULL, uri);

  /* pwg-raster-document-xxx-supported */
  for (i = 0; i < num_formats; i ++)
    if (!_cups_strcasecmp(formats[i], "image/pwg-raster"))
      break;

  if (i < num_formats)
  {
    ippAddResolutions(printer->attrs, IPP_TAG_PRINTER,
                      "pwg-raster-document-resolution-supported",
                      (int)(sizeof(pwg_raster_document_resolution_supported) /
                            sizeof(pwg_raster_document_resolution_supported[0])),
                      IPP_RES_PER_INCH,
                      pwg_raster_document_resolution_supported,
                      pwg_raster_document_resolution_supported);
    ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                 "pwg-raster-document-sheet-back", NULL, "normal");
    ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                  "pwg-raster-document-type-supported",
                  (int)(sizeof(pwg_raster_document_type_supported) /
                        sizeof(pwg_raster_document_type_supported[0])), NULL,
                  pwg_raster_document_type_supported);
  }

  /* reference-uri-scheme-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER,
                IPP_TAG_URISCHEME | IPP_TAG_CUPS_CONST,
                "reference-uri-schemes-supported",
                (int)(sizeof(reference_uri_schemes_supported) /
                      sizeof(reference_uri_schemes_supported[0])),
                NULL, reference_uri_schemes_supported);

  /* sides-default */
  ippAddString(printer->attrs, IPP_TAG_PRINTER,
               IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
               "sides-default", NULL, "one-sided");

  /* sides-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER,
                IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
                "sides-supported", duplex ? 3 : 1, NULL, sides_supported);

  /* uri-authentication-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER,
               IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
               "uri-authentication-supported", NULL, "none");

  /* uri-security-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER,
               IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
               "uri-security-supported", NULL, "none");

  /* which-jobs-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER,
                IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
                "which-jobs-supported",
                sizeof(which_jobs) / sizeof(which_jobs[0]), NULL, which_jobs);

  free(formats[0]);

  debug_attributes("Printer", printer->attrs, 0);

#ifdef HAVE_DNSSD
 /*
  * Register the printer with Bonjour...
  */

  if (!register_printer(printer, location, make, model, docformats, adminurl,
                        ppm_color > 0, duplex, subtype))
    goto bad_printer;
#endif /* HAVE_DNSSD */

 /*
  * Return it!
  */

  return (printer);


 /*
  * If we get here we were unable to create the printer...
  */

  bad_printer:

  delete_printer(printer);
  return (NULL);
}


/*
 * 'debug_attributes()' - Print attributes in a request or response.
 */

static void
debug_attributes(const char *title,	/* I - Title */
                 ipp_t      *ipp,	/* I - Request/response */
                 int        type)	/* I - 0 = object, 1 = request, 2 = response */
{
  ipp_tag_t		group_tag;	/* Current group */
  ipp_attribute_t	*attr;		/* Current attribute */
  char			buffer[2048];	/* String buffer for value */
  int			major, minor;	/* Version */


  if (Verbosity <= 1)
    return;

  fprintf(stderr, "%s:\n", title);
  major = ippGetVersion(ipp, &minor);
  fprintf(stderr, "  version=%d.%d\n", major, minor);
  if (type == 1)
    fprintf(stderr, "  operation-id=%s(%04x)\n",
            ippOpString(ippGetOperation(ipp)), ippGetOperation(ipp));
  else if (type == 2)
    fprintf(stderr, "  status-code=%s(%04x)\n",
            ippErrorString(ippGetStatusCode(ipp)), ippGetStatusCode(ipp));
  fprintf(stderr, "  request-id=%d\n\n", ippGetRequestId(ipp));

  for (attr = ippFirstAttribute(ipp), group_tag = IPP_TAG_ZERO;
       attr;
       attr = ippNextAttribute(ipp))
  {
    if (ippGetGroupTag(attr) != group_tag)
    {
      group_tag = ippGetGroupTag(attr);
      fprintf(stderr, "  %s\n", ippTagString(group_tag));
    }

    if (ippGetName(attr))
    {
      ippAttributeString(attr, buffer, sizeof(buffer));
      fprintf(stderr, "    %s (%s%s) %s\n", ippGetName(attr),
	      ippGetCount(attr) > 1 ? "1setOf " : "",
	      ippTagString(ippGetValueTag(attr)), buffer);
    }
  }
}


/*
 * 'delete_client()' - Close the socket and free all memory used by a client
 *                     object.
 */

static void
delete_client(_ipp_client_t *client)	/* I - Client */
{
  if (Verbosity)
    fprintf(stderr, "Closing connection from %s\n", client->hostname);

 /*
  * Flush pending writes before closing...
  */

  httpFlushWrite(client->http);

 /*
  * Free memory...
  */

  httpClose(client->http);

  ippDelete(client->request);
  ippDelete(client->response);

  free(client);
}


/*
 * 'delete_job()' - Remove from the printer and free all memory used by a job
 *                  object.
 */

static void
delete_job(_ipp_job_t *job)		/* I - Job */
{
  if (Verbosity)
    fprintf(stderr, "Removing job #%d from history.\n", job->id);

  ippDelete(job->attrs);

  if (job->filename)
  {
    if (!KeepFiles)
      unlink(job->filename);

    free(job->filename);
  }

  free(job);
}


/*
 * 'delete_printer()' - Unregister, close listen sockets, and free all memory
 *                      used by a printer object.
 */

static void
delete_printer(_ipp_printer_t *printer)	/* I - Printer */
{
  if (printer->ipv4 >= 0)
    close(printer->ipv4);

  if (printer->ipv6 >= 0)
    close(printer->ipv6);

#if HAVE_DNSSD
  if (printer->printer_ref)
    DNSServiceRefDeallocate(printer->printer_ref);

  if (printer->ipp_ref)
    DNSServiceRefDeallocate(printer->ipp_ref);

#  ifdef HAVE_SSL
  if (printer->ipps_ref)
    DNSServiceRefDeallocate(printer->ipps_ref);
#  endif /* HAVE_SSL */
  if (printer->http_ref)
    DNSServiceRefDeallocate(printer->http_ref);

  if (printer->common_ref)
    DNSServiceRefDeallocate(printer->common_ref);

  TXTRecordDeallocate(&(printer->ipp_txt));

  if (printer->dnssd_name)
    free(printer->dnssd_name);
#endif /* HAVE_DNSSD */

  if (printer->name)
    free(printer->name);
  if (printer->icon)
    free(printer->icon);
  if (printer->command)
    free(printer->command);
  if (printer->directory)
    free(printer->directory);
  if (printer->hostname)
    free(printer->hostname);
  if (printer->uri)
    free(printer->uri);

  ippDelete(printer->attrs);
  cupsArrayDelete(printer->jobs);

  free(printer);
}


#ifdef HAVE_DNSSD
/*
 * 'dnssd_callback()' - Handle Bonjour registration events.
 */

static void
dnssd_callback(
    DNSServiceRef       sdRef,		/* I - Service reference */
    DNSServiceFlags     flags,		/* I - Status flags */
    DNSServiceErrorType errorCode,	/* I - Error, if any */
    const char          *name,		/* I - Service name */
    const char          *regtype,	/* I - Service type */
    const char          *domain,	/* I - Domain for service */
    _ipp_printer_t      *printer)	/* I - Printer */
{
  if (errorCode)
  {
    fprintf(stderr, "DNSServiceRegister for %s failed with error %d.\n",
            regtype, (int)errorCode);
    return;
  }
  else if (_cups_strcasecmp(name, printer->dnssd_name))
  {
    if (Verbosity)
      fprintf(stderr, "Now using DNS-SD service name \"%s\".\n", name);

    /* No lock needed since only the main thread accesses/changes this */
    free(printer->dnssd_name);
    printer->dnssd_name = strdup(name);
  }
}
#endif /* HAVE_DNSSD */


/*
 * 'find_job()' - Find a job specified in a request.
 */

static _ipp_job_t *			/* O - Job or NULL */
find_job(_ipp_client_t *client)		/* I - Client */
{
  ipp_attribute_t	*attr;		/* job-id or job-uri attribute */
  _ipp_job_t		key,		/* Job search key */
			*job;		/* Matching job, if any */


  key.id = 0;

  if ((attr = ippFindAttribute(client->request, "job-uri",
                               IPP_TAG_URI)) != NULL)
  {
    const char *uri = ippGetString(attr, 0, NULL);

    if (!strncmp(uri, client->printer->uri, client->printer->urilen) &&
        uri[client->printer->urilen] == '/')
      key.id = atoi(uri + client->printer->urilen + 1);
  }
  else if ((attr = ippFindAttribute(client->request, "job-id",
                                    IPP_TAG_INTEGER)) != NULL)
    key.id = ippGetInteger(attr, 0);

  _cupsRWLockRead(&(client->printer->rwlock));
  job = (_ipp_job_t *)cupsArrayFind(client->printer->jobs, &key);
  _cupsRWUnlock(&(client->printer->rwlock));

  return (job);
}


/*
 * 'html_escape()' - Write a HTML-safe string.
 */

static void
html_escape(_ipp_client_t *client,	/* I - Client */
	    const char    *s,		/* I - String to write */
	    size_t        slen)		/* I - Number of characters to write */
{
  const char	*start,			/* Start of segment */
		*end;			/* End of string */


  start = s;
  end   = s + (slen > 0 ? slen : strlen(s));

  while (*s && s < end)
  {
    if (*s == '&' || *s == '<')
    {
      if (s > start)
        httpWrite2(client->http, start, s - start);

      if (*s == '&')
        httpWrite2(client->http, "&amp;", 5);
      else
        httpWrite2(client->http, "&lt;", 4);

      start = s + 1;
    }

    s ++;
  }

  if (s > start)
    httpWrite2(client->http, start, s - start);
}


/*
 * 'html_printf()' - Send formatted text to the client, quoting as needed.
 */

static void
html_printf(_ipp_client_t *client,	/* I - Client */
	    const char    *format,	/* I - Printf-style format string */
	    ...)			/* I - Additional arguments as needed */
{
  va_list	ap;			/* Pointer to arguments */
  const char	*start;			/* Start of string */
  char		size,			/* Size character (h, l, L) */
		type;			/* Format type character */
  int		width,			/* Width of field */
		prec;			/* Number of characters of precision */
  char		tformat[100],		/* Temporary format string for sprintf() */
		*tptr,			/* Pointer into temporary format */
		temp[1024];		/* Buffer for formatted numbers */
  char		*s;			/* Pointer to string */


 /*
  * Loop through the format string, formatting as needed...
  */

  va_start(ap, format);
  start = format;

  while (*format)
  {
    if (*format == '%')
    {
      if (format > start)
        httpWrite2(client->http, start, format - start);

      tptr    = tformat;
      *tptr++ = *format++;

      if (*format == '%')
      {
        httpWrite2(client->http, "%", 1);
        format ++;
	continue;
      }
      else if (strchr(" -+#\'", *format))
        *tptr++ = *format++;

      if (*format == '*')
      {
       /*
        * Get width from argument...
	*/

	format ++;
	width = va_arg(ap, int);

	snprintf(tptr, sizeof(tformat) - (tptr - tformat), "%d", width);
	tptr += strlen(tptr);
      }
      else
      {
	width = 0;

	while (isdigit(*format & 255))
	{
	  if (tptr < (tformat + sizeof(tformat) - 1))
	    *tptr++ = *format;

	  width = width * 10 + *format++ - '0';
	}
      }

      if (*format == '.')
      {
	if (tptr < (tformat + sizeof(tformat) - 1))
	  *tptr++ = *format;

        format ++;

        if (*format == '*')
	{
         /*
	  * Get precision from argument...
	  */

	  format ++;
	  prec = va_arg(ap, int);

	  snprintf(tptr, sizeof(tformat) - (tptr - tformat), "%d", prec);
	  tptr += strlen(tptr);
	}
	else
	{
	  prec = 0;

	  while (isdigit(*format & 255))
	  {
	    if (tptr < (tformat + sizeof(tformat) - 1))
	      *tptr++ = *format;

	    prec = prec * 10 + *format++ - '0';
	  }
	}
      }

      if (*format == 'l' && format[1] == 'l')
      {
        size = 'L';

	if (tptr < (tformat + sizeof(tformat) - 2))
	{
	  *tptr++ = 'l';
	  *tptr++ = 'l';
	}

	format += 2;
      }
      else if (*format == 'h' || *format == 'l' || *format == 'L')
      {
	if (tptr < (tformat + sizeof(tformat) - 1))
	  *tptr++ = *format;

        size = *format++;
      }
      else
        size = 0;


      if (!*format)
      {
        start = format;
        break;
      }

      if (tptr < (tformat + sizeof(tformat) - 1))
        *tptr++ = *format;

      type  = *format++;
      *tptr = '\0';
      start = format;

      switch (type)
      {
	case 'E' : /* Floating point formats */
	case 'G' :
	case 'e' :
	case 'f' :
	case 'g' :
	    if ((width + 2) > sizeof(temp))
	      break;

	    sprintf(temp, tformat, va_arg(ap, double));

            httpWrite2(client->http, temp, strlen(temp));
	    break;

        case 'B' : /* Integer formats */
	case 'X' :
	case 'b' :
        case 'd' :
	case 'i' :
	case 'o' :
	case 'u' :
	case 'x' :
	    if ((width + 2) > sizeof(temp))
	      break;

#  ifdef HAVE_LONG_LONG
            if (size == 'L')
	      sprintf(temp, tformat, va_arg(ap, long long));
	    else
#  endif /* HAVE_LONG_LONG */
            if (size == 'l')
	      sprintf(temp, tformat, va_arg(ap, long));
	    else
	      sprintf(temp, tformat, va_arg(ap, int));

            httpWrite2(client->http, temp, strlen(temp));
	    break;

	case 'p' : /* Pointer value */
	    if ((width + 2) > sizeof(temp))
	      break;

	    sprintf(temp, tformat, va_arg(ap, void *));

            httpWrite2(client->http, temp, strlen(temp));
	    break;

        case 'c' : /* Character or character array */
            if (width <= 1)
            {
              temp[0] = va_arg(ap, int);
              temp[1] = '\0';
              html_escape(client, temp, 1);
            }
            else
              html_escape(client, va_arg(ap, char *), (size_t)width);
	    break;

	case 's' : /* String */
	    if ((s = va_arg(ap, char *)) == NULL)
	      s = "(null)";

            html_escape(client, s, strlen(s));
	    break;
      }
    }
    else
      format ++;
  }

  if (format > start)
    httpWrite2(client->http, start, format - start);

  va_end(ap);
}


/*
 * 'ipp_cancel_job()' - Cancel a job.
 */

static void
ipp_cancel_job(_ipp_client_t *client)	/* I - Client */
{
  _ipp_job_t		*job;		/* Job information */


 /*
  * Get the job...
  */

  if ((job = find_job(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    return;
  }

 /*
  * See if the job is already completed, canceled, or aborted; if so,
  * we can't cancel...
  */

  switch (job->state)
  {
    case IPP_JSTATE_CANCELED :
	respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
		    "Job #%d is already canceled - can\'t cancel.", job->id);
        break;

    case IPP_JSTATE_ABORTED :
	respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
		    "Job #%d is already aborted - can\'t cancel.", job->id);
        break;

    case IPP_JSTATE_COMPLETED :
	respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
		    "Job #%d is already completed - can\'t cancel.", job->id);
        break;

    default :
       /*
        * Cancel the job...
	*/

	_cupsRWLockWrite(&(client->printer->rwlock));

	if (job->state == IPP_JSTATE_PROCESSING ||
	    (job->state == IPP_JSTATE_HELD && job->fd >= 0))
          job->cancel = 1;
	else
	{
	  job->state     = IPP_JSTATE_CANCELED;
	  job->completed = time(NULL);
	}

	_cupsRWUnlock(&(client->printer->rwlock));

	respond_ipp(client, IPP_STATUS_OK, NULL);
        break;
  }
}


/*
 * 'ipp_create_job()' - Create a job object.
 */

static void
ipp_create_job(_ipp_client_t *client)	/* I - Client */
{
  _ipp_job_t		*job;		/* New job */
  cups_array_t		*ra;		/* Attributes to send in response */


 /*
  * Validate print job attributes...
  */

  if (!valid_job_attributes(client))
  {
    httpFlush(client->http);
    return;
  }

 /*
  * Do we have a file to print?
  */

  if (httpGetState(client->http) == HTTP_STATE_POST_RECV)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "Unexpected document data following request.");
    return;
  }

 /*
  * Create the job...
  */

  if ((job = create_job(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BUSY,
                "Currently printing another job.");
    return;
  }

 /*
  * Return the job info...
  */

  respond_ipp(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
}


/*
 * 'ipp_get_job_attributes()' - Get the attributes for a job object.
 */

static void
ipp_get_job_attributes(
    _ipp_client_t *client)		/* I - Client */
{
  _ipp_job_t	*job;			/* Job */
  cups_array_t	*ra;			/* requested-attributes */


  if ((job = find_job(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Job not found.");
    return;
  }

  respond_ipp(client, IPP_STATUS_OK, NULL);

  ra = ippCreateRequestedArray(client->request);
  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
}


/*
 * 'ipp_get_jobs()' - Get a list of job objects.
 */

static void
ipp_get_jobs(_ipp_client_t *client)	/* I - Client */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  const char		*which_jobs = NULL;
					/* which-jobs values */
  int			job_comparison;	/* Job comparison */
  ipp_jstate_t		job_state;	/* job-state value */
  int			first_job_id,	/* First job ID */
			limit,		/* Maximum number of jobs to return */
			count;		/* Number of jobs that match */
  const char		*username;	/* Username */
  _ipp_job_t		*job;		/* Current job pointer */
  cups_array_t		*ra;		/* Requested attributes array */


 /*
  * See if the "which-jobs" attribute have been specified...
  */

  if ((attr = ippFindAttribute(client->request, "which-jobs",
                               IPP_TAG_KEYWORD)) != NULL)
  {
    which_jobs = ippGetString(attr, 0, NULL);
    fprintf(stderr, "%s Get-Jobs which-jobs=%s", client->hostname, which_jobs);
  }

  if (!which_jobs || !strcmp(which_jobs, "not-completed"))
  {
    job_comparison = -1;
    job_state      = IPP_JSTATE_STOPPED;
  }
  else if (!strcmp(which_jobs, "completed"))
  {
    job_comparison = 1;
    job_state      = IPP_JSTATE_CANCELED;
  }
  else if (!strcmp(which_jobs, "aborted"))
  {
    job_comparison = 0;
    job_state      = IPP_JSTATE_ABORTED;
  }
  else if (!strcmp(which_jobs, "all"))
  {
    job_comparison = 1;
    job_state      = IPP_JSTATE_PENDING;
  }
  else if (!strcmp(which_jobs, "canceled"))
  {
    job_comparison = 0;
    job_state      = IPP_JSTATE_CANCELED;
  }
  else if (!strcmp(which_jobs, "pending"))
  {
    job_comparison = 0;
    job_state      = IPP_JSTATE_PENDING;
  }
  else if (!strcmp(which_jobs, "pending-held"))
  {
    job_comparison = 0;
    job_state      = IPP_JSTATE_HELD;
  }
  else if (!strcmp(which_jobs, "processing"))
  {
    job_comparison = 0;
    job_state      = IPP_JSTATE_PROCESSING;
  }
  else if (!strcmp(which_jobs, "processing-stopped"))
  {
    job_comparison = 0;
    job_state      = IPP_JSTATE_STOPPED;
  }
  else
  {
    respond_ipp(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES,
                "The which-jobs value \"%s\" is not supported.", which_jobs);
    ippAddString(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_KEYWORD,
                 "which-jobs", NULL, which_jobs);
    return;
  }

 /*
  * See if they want to limit the number of jobs reported...
  */

  if ((attr = ippFindAttribute(client->request, "limit",
                               IPP_TAG_INTEGER)) != NULL)
  {
    limit = ippGetInteger(attr, 0);

    fprintf(stderr, "%s Get-Jobs limit=%d", client->hostname, limit);
  }
  else
    limit = 0;

  if ((attr = ippFindAttribute(client->request, "first-job-id",
                               IPP_TAG_INTEGER)) != NULL)
  {
    first_job_id = ippGetInteger(attr, 0);

    fprintf(stderr, "%s Get-Jobs first-job-id=%d", client->hostname,
            first_job_id);
  }
  else
    first_job_id = 1;

 /*
  * See if we only want to see jobs for a specific user...
  */

  username = NULL;

  if ((attr = ippFindAttribute(client->request, "my-jobs",
                               IPP_TAG_BOOLEAN)) != NULL)
  {
    int my_jobs = ippGetBoolean(attr, 0);

    fprintf(stderr, "%s Get-Jobs my-jobs=%s\n", client->hostname,
            my_jobs ? "true" : "false");

    if (my_jobs)
    {
      if ((attr = ippFindAttribute(client->request, "requesting-user-name",
					IPP_TAG_NAME)) == NULL)
      {
	respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
	            "Need requesting-user-name with my-jobs.");
	return;
      }

      username = ippGetString(attr, 0, NULL);

      fprintf(stderr, "%s Get-Jobs requesting-user-name=\"%s\"\n",
              client->hostname, username);
    }
  }

 /*
  * OK, build a list of jobs for this printer...
  */

  ra = ippCreateRequestedArray(client->request);

  respond_ipp(client, IPP_STATUS_OK, NULL);

  _cupsRWLockRead(&(client->printer->rwlock));

  for (count = 0, job = (_ipp_job_t *)cupsArrayFirst(client->printer->jobs);
       (limit <= 0 || count < limit) && job;
       job = (_ipp_job_t *)cupsArrayNext(client->printer->jobs))
  {
   /*
    * Filter out jobs that don't match...
    */

    if ((job_comparison < 0 && job->state > job_state) ||
	(job_comparison == 0 && job->state != job_state) ||
	(job_comparison > 0 && job->state < job_state) ||
	job->id < first_job_id ||
	(username && job->username &&
	 _cups_strcasecmp(username, job->username)))
      continue;

    if (count > 0)
      ippAddSeparator(client->response);

    count ++;
    copy_job_attributes(client, job, ra);
  }

  cupsArrayDelete(ra);

  _cupsRWUnlock(&(client->printer->rwlock));
}


/*
 * 'ipp_get_printer_attributes()' - Get the attributes for a printer object.
 */

static void
ipp_get_printer_attributes(
    _ipp_client_t *client)		/* I - Client */
{
  cups_array_t		*ra;		/* Requested attributes array */
  _ipp_printer_t	*printer;	/* Printer */


 /*
  * Send the attributes...
  */

  ra      = ippCreateRequestedArray(client->request);
  printer = client->printer;

  respond_ipp(client, IPP_STATUS_OK, NULL);

  _cupsRWLockRead(&(printer->rwlock));

  copy_attributes(client->response, printer->attrs, ra, IPP_TAG_ZERO,
		  IPP_TAG_CUPS_CONST);

  if (!ra || cupsArrayFind(ra, "printer-state"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                  "printer-state", printer->state);

  if (!ra || cupsArrayFind(ra, "printer-state-reasons"))
  {
    if (printer->state_reasons == _IPP_PSTATE_NONE)
      ippAddString(client->response, IPP_TAG_PRINTER,
                   IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
                   "printer-state-reasons", NULL, "none");
    else
    {
      int			num_reasons = 0;/* Number of reasons */
      const char		*reasons[32];	/* Reason strings */

      if (printer->state_reasons & _IPP_PSTATE_OTHER)
	reasons[num_reasons ++] = "other";
      if (printer->state_reasons & _IPP_PSTATE_COVER_OPEN)
	reasons[num_reasons ++] = "cover-open";
      if (printer->state_reasons & _IPP_PSTATE_INPUT_TRAY_MISSING)
	reasons[num_reasons ++] = "input-tray-missing";
      if (printer->state_reasons & _IPP_PSTATE_MARKER_SUPPLY_EMPTY)
	reasons[num_reasons ++] = "marker-supply-empty-warning";
      if (printer->state_reasons & _IPP_PSTATE_MARKER_SUPPLY_LOW)
	reasons[num_reasons ++] = "marker-supply-low-report";
      if (printer->state_reasons & _IPP_PSTATE_MARKER_WASTE_ALMOST_FULL)
	reasons[num_reasons ++] = "marker-waste-almost-full-report";
      if (printer->state_reasons & _IPP_PSTATE_MARKER_WASTE_FULL)
	reasons[num_reasons ++] = "marker-waste-full-warning";
      if (printer->state_reasons & _IPP_PSTATE_MEDIA_EMPTY)
	reasons[num_reasons ++] = "media-empty-warning";
      if (printer->state_reasons & _IPP_PSTATE_MEDIA_JAM)
	reasons[num_reasons ++] = "media-jam-warning";
      if (printer->state_reasons & _IPP_PSTATE_MEDIA_LOW)
	reasons[num_reasons ++] = "media-low-report";
      if (printer->state_reasons & _IPP_PSTATE_MEDIA_NEEDED)
	reasons[num_reasons ++] = "media-needed-report";
      if (printer->state_reasons & _IPP_PSTATE_MOVING_TO_PAUSED)
	reasons[num_reasons ++] = "moving-to-paused";
      if (printer->state_reasons & _IPP_PSTATE_PAUSED)
	reasons[num_reasons ++] = "paused";
      if (printer->state_reasons & _IPP_PSTATE_SPOOL_AREA_FULL)
	reasons[num_reasons ++] = "spool-area-full";
      if (printer->state_reasons & _IPP_PSTATE_TONER_EMPTY)
	reasons[num_reasons ++] = "toner-empty-warning";
      if (printer->state_reasons & _IPP_PSTATE_TONER_LOW)
	reasons[num_reasons ++] = "toner-low-report";

      ippAddStrings(client->response, IPP_TAG_PRINTER,
                    IPP_TAG_KEYWORD | IPP_TAG_CUPS_CONST,
                    "printer-state-reasons", num_reasons, NULL, reasons);
    }
  }

  if (!ra || cupsArrayFind(ra, "printer-up-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                  "printer-up-time", (int)time(NULL));

  if (!ra || cupsArrayFind(ra, "queued-job-count"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                  "queued-job-count",
		  printer->active_job &&
		      printer->active_job->state < IPP_JSTATE_CANCELED);

  _cupsRWUnlock(&(printer->rwlock));

  cupsArrayDelete(ra);
}


/*
 * 'ipp_print_job()' - Create a job object with an attached document.
 */

static void
ipp_print_job(_ipp_client_t *client)	/* I - Client */
{
  _ipp_job_t		*job;		/* New job */
  char			filename[1024],	/* Filename buffer */
			buffer[4096];	/* Copy buffer */
  ssize_t		bytes;		/* Bytes read */
  cups_array_t		*ra;		/* Attributes to send in response */


 /*
  * Validate print job attributes...
  */

  if (!valid_job_attributes(client))
  {
    httpFlush(client->http);
    return;
  }

 /*
  * Do we have a file to print?
  */

  if (httpGetState(client->http) == HTTP_STATE_POST_SEND)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "No file in request.");
    return;
  }

 /*
  * Print the job...
  */

  if ((job = create_job(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BUSY,
                "Currently printing another job.");
    return;
  }

 /*
  * Create a file for the request data...
  */

  if (!_cups_strcasecmp(job->format, "image/jpeg"))
    snprintf(filename, sizeof(filename), "%s/%d.jpg",
             client->printer->directory, job->id);
  else if (!_cups_strcasecmp(job->format, "image/png"))
    snprintf(filename, sizeof(filename), "%s/%d.png",
             client->printer->directory, job->id);
  else if (!_cups_strcasecmp(job->format, "image/pwg-raster"))
    snprintf(filename, sizeof(filename), "%s/%d.ras",
             client->printer->directory, job->id);
  else if (!_cups_strcasecmp(job->format, "application/pdf"))
    snprintf(filename, sizeof(filename), "%s/%d.pdf",
             client->printer->directory, job->id);
  else if (!_cups_strcasecmp(job->format, "application/postscript"))
    snprintf(filename, sizeof(filename), "%s/%d.ps",
             client->printer->directory, job->id);
  else
    snprintf(filename, sizeof(filename), "%s/%d.prn",
             client->printer->directory, job->id);

  if ((job->fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0)
  {
    job->state = IPP_JSTATE_ABORTED;

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to create print file: %s", strerror(errno));
    return;
  }

  while ((bytes = httpRead2(client->http, buffer, sizeof(buffer))) > 0)
  {
    if (write(job->fd, buffer, bytes) < bytes)
    {
      int error = errno;		/* Write error */

      job->state = IPP_JSTATE_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);

      respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                  "Unable to write print file: %s", strerror(error));
      return;
    }
  }

  if (bytes < 0)
  {
   /*
    * Got an error while reading the print data, so abort this job.
    */

    job->state = IPP_JSTATE_ABORTED;

    close(job->fd);
    job->fd = -1;

    unlink(filename);

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to read print file.");
    return;
  }

  if (close(job->fd))
  {
    int error = errno;		/* Write error */

    job->state = IPP_JSTATE_ABORTED;
    job->fd    = -1;

    unlink(filename);

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to write print file: %s", strerror(error));
    return;
  }

  job->fd       = -1;
  job->filename = strdup(filename);
  job->state    = IPP_JSTATE_PENDING;

 /*
  * Process the job...
  */

#if 0
  if (!_cupsThreadCreate((_cups_thread_func_t)process_job, job))
  {
    job->state = IPP_JSTATE_ABORTED;
    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL, "Unable to process job.");
    return;
  }

#else
  process_job(job);
#endif /* 0 */

 /*
  * Return the job info...
  */

  respond_ipp(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
}


/*
 * 'ipp_print_uri()' - Create a job object with a referenced document.
 */

static void
ipp_print_uri(_ipp_client_t *client)	/* I - Client */
{
  _ipp_job_t		*job;		/* New job */
  ipp_attribute_t	*uri;		/* document-uri */
  char			scheme[256],	/* URI scheme */
			userpass[256],	/* Username and password info */
			hostname[256],	/* Hostname */
			resource[1024];	/* Resource path */
  int			port;		/* Port number */
  http_uri_status_t	uri_status;	/* URI decode status */
  http_encryption_t	encryption;	/* Encryption to use, if any */
  http_t		*http;		/* Connection for http/https URIs */
  http_status_t		status;		/* Access status for http/https URIs */
  int			infile;		/* Input file for local file URIs */
  char			filename[1024],	/* Filename buffer */
			buffer[4096];	/* Copy buffer */
  ssize_t		bytes;		/* Bytes read */
  cups_array_t		*ra;		/* Attributes to send in response */
  static const char * const uri_status_strings[] =
  {					/* URI decode errors */
    "URI too large.",
    "Bad arguments to function.",
    "Bad resource in URI.",
    "Bad port number in URI.",
    "Bad hostname in URI.",
    "Bad username in URI.",
    "Bad scheme in URI.",
    "Bad/empty URI."
  };


 /*
  * Validate print job attributes...
  */

  if (!valid_job_attributes(client))
  {
    httpFlush(client->http);
    return;
  }

 /*
  * Do we have a file to print?
  */

  if (httpGetState(client->http) == HTTP_STATE_POST_RECV)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "Unexpected document data following request.");
    return;
  }

 /*
  * Do we have a document URI?
  */

  if ((uri = ippFindAttribute(client->request, "document-uri",
                              IPP_TAG_URI)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing document-uri.");
    return;
  }

  if (ippGetCount(uri) != 1)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "Too many document-uri values.");
    return;
  }

  uri_status = httpSeparateURI(HTTP_URI_CODING_ALL, ippGetString(uri, 0, NULL),
                               scheme, sizeof(scheme), userpass,
                               sizeof(userpass), hostname, sizeof(hostname),
                               &port, resource, sizeof(resource));
  if (uri_status < HTTP_URI_STATUS_OK)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "Bad document-uri: %s",
                uri_status_strings[uri_status - HTTP_URI_STATUS_OVERFLOW]);
    return;
  }

  if (strcmp(scheme, "file") &&
#ifdef HAVE_SSL
      strcmp(scheme, "https") &&
#endif /* HAVE_SSL */
      strcmp(scheme, "http"))
  {
    respond_ipp(client, IPP_STATUS_ERROR_URI_SCHEME,
                "URI scheme \"%s\" not supported.", scheme);
    return;
  }

  if (!strcmp(scheme, "file") && access(resource, R_OK))
  {
    respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                "Unable to access URI: %s", strerror(errno));
    return;
  }

 /*
  * Print the job...
  */

  if ((job = create_job(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BUSY,
                "Currently printing another job.");
    return;
  }

 /*
  * Create a file for the request data...
  */

  if (!_cups_strcasecmp(job->format, "image/jpeg"))
    snprintf(filename, sizeof(filename), "%s/%d.jpg",
             client->printer->directory, job->id);
  else if (!_cups_strcasecmp(job->format, "image/png"))
    snprintf(filename, sizeof(filename), "%s/%d.png",
             client->printer->directory, job->id);
  else if (!_cups_strcasecmp(job->format, "application/pdf"))
    snprintf(filename, sizeof(filename), "%s/%d.pdf",
             client->printer->directory, job->id);
  else if (!_cups_strcasecmp(job->format, "application/postscript"))
    snprintf(filename, sizeof(filename), "%s/%d.ps",
             client->printer->directory, job->id);
  else
    snprintf(filename, sizeof(filename), "%s/%d.prn",
             client->printer->directory, job->id);

  if ((job->fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0)
  {
    job->state = IPP_JSTATE_ABORTED;

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to create print file: %s", strerror(errno));
    return;
  }

  if (!strcmp(scheme, "file"))
  {
    if ((infile = open(resource, O_RDONLY)) < 0)
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                  "Unable to access URI: %s", strerror(errno));
      return;
    }

    do
    {
      if ((bytes = read(infile, buffer, sizeof(buffer))) < 0 &&
          (errno == EAGAIN || errno == EINTR))
        bytes = 1;
      else if (bytes > 0 && write(job->fd, buffer, bytes) < bytes)
      {
	int error = errno;		/* Write error */

	job->state = IPP_JSTATE_ABORTED;

	close(job->fd);
	job->fd = -1;

	unlink(filename);
	close(infile);

	respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
		    "Unable to write print file: %s", strerror(error));
	return;
      }
    }
    while (bytes > 0);

    close(infile);
  }
  else
  {
#ifdef HAVE_SSL
    if (port == 443 || !strcmp(scheme, "https"))
      encryption = HTTP_ENCRYPTION_ALWAYS;
    else
#endif /* HAVE_SSL */
    encryption = HTTP_ENCRYPTION_IF_REQUESTED;

    if ((http = httpConnect2(hostname, port, NULL, AF_UNSPEC, encryption,
                             1, 30000, NULL)) == NULL)
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                  "Unable to connect to %s: %s", hostname,
		  cupsLastErrorString());
      job->state = IPP_JSTATE_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);
      return;
    }

    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_ACCEPT_LANGUAGE, "en");
    if (httpGet(http, resource))
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                  "Unable to GET URI: %s", strerror(errno));

      job->state = IPP_JSTATE_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);
      httpClose(http);
      return;
    }

    while ((status = httpUpdate(http)) == HTTP_STATUS_CONTINUE);

    if (status != HTTP_STATUS_OK)
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                  "Unable to GET URI: %s", httpStatus(status));

      job->state = IPP_JSTATE_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);
      httpClose(http);
      return;
    }

    while ((bytes = httpRead2(http, buffer, sizeof(buffer))) > 0)
    {
      if (write(job->fd, buffer, bytes) < bytes)
      {
	int error = errno;		/* Write error */

	job->state = IPP_JSTATE_ABORTED;

	close(job->fd);
	job->fd = -1;

	unlink(filename);
	httpClose(http);

	respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
		    "Unable to write print file: %s", strerror(error));
	return;
      }
    }

    httpClose(http);
  }

  if (close(job->fd))
  {
    int error = errno;		/* Write error */

    job->state = IPP_JSTATE_ABORTED;
    job->fd    = -1;

    unlink(filename);

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to write print file: %s", strerror(error));
    return;
  }

  job->fd       = -1;
  job->filename = strdup(filename);
  job->state    = IPP_JSTATE_PENDING;

 /*
  * Process the job...
  */

#if 0
  if (!_cupsThreadCreate((_cups_thread_func_t)process_job, job))
  {
    job->state = IPP_JSTATE_ABORTED;
    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL, "Unable to process job.");
    return;
  }

#else
  process_job(job);
#endif /* 0 */

 /*
  * Return the job info...
  */

  respond_ipp(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
}


/*
 * 'ipp_send_document()' - Add an attached document to a job object created with
 *                         Create-Job.
 */

static void
ipp_send_document(_ipp_client_t *client)/* I - Client */
{
  _ipp_job_t		*job;		/* Job information */
  char			filename[1024],	/* Filename buffer */
			buffer[4096];	/* Copy buffer */
  ssize_t		bytes;		/* Bytes read */
  ipp_attribute_t	*attr;		/* Current attribute */
  cups_array_t		*ra;		/* Attributes to send in response */


 /*
  * Get the job...
  */

  if ((job = find_job(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    httpFlush(client->http);
    return;
  }

 /*
  * See if we already have a document for this job or the job has already
  * in a non-pending state...
  */

  if (job->state > IPP_JSTATE_HELD)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
                "Job is not in a pending state.");
    httpFlush(client->http);
    return;
  }
  else if (job->filename || job->fd >= 0)
  {
    respond_ipp(client, IPP_STATUS_ERROR_MULTIPLE_JOBS_NOT_SUPPORTED,
                "Multiple document jobs are not supported.");
    httpFlush(client->http);
    return;
  }

  if ((attr = ippFindAttribute(client->request, "last-document",
                               IPP_TAG_ZERO)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "Missing required last-document attribute.");
    httpFlush(client->http);
    return;
  }
  else if (ippGetValueTag(attr) != IPP_TAG_BOOLEAN || ippGetCount(attr) != 1 ||
           !ippGetBoolean(attr, 0))
  {
    respond_unsupported(client, attr);
    httpFlush(client->http);
    return;
  }

 /*
  * Validate document attributes...
  */

  if (!valid_doc_attributes(client))
  {
    httpFlush(client->http);
    return;
  }

 /*
  * Get the document format for the job...
  */

  _cupsRWLockWrite(&(client->printer->rwlock));

  if ((attr = ippFindAttribute(job->attrs, "document-format",
                               IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else
    job->format = "application/octet-stream";

 /*
  * Create a file for the request data...
  */

  if (!_cups_strcasecmp(job->format, "image/jpeg"))
    snprintf(filename, sizeof(filename), "%s/%d.jpg",
             client->printer->directory, job->id);
  else if (!_cups_strcasecmp(job->format, "image/png"))
    snprintf(filename, sizeof(filename), "%s/%d.png",
             client->printer->directory, job->id);
  else if (!_cups_strcasecmp(job->format, "application/pdf"))
    snprintf(filename, sizeof(filename), "%s/%d.pdf",
             client->printer->directory, job->id);
  else if (!_cups_strcasecmp(job->format, "application/postscript"))
    snprintf(filename, sizeof(filename), "%s/%d.ps",
             client->printer->directory, job->id);
  else
    snprintf(filename, sizeof(filename), "%s/%d.prn",
             client->printer->directory, job->id);

  job->fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);

  _cupsRWUnlock(&(client->printer->rwlock));

  if (job->fd < 0)
  {
    job->state = IPP_JSTATE_ABORTED;

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to create print file: %s", strerror(errno));
    return;
  }

  while ((bytes = httpRead2(client->http, buffer, sizeof(buffer))) > 0)
  {
    if (write(job->fd, buffer, bytes) < bytes)
    {
      int error = errno;		/* Write error */

      job->state = IPP_JSTATE_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);

      respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                  "Unable to write print file: %s", strerror(error));
      return;
    }
  }

  if (bytes < 0)
  {
   /*
    * Got an error while reading the print data, so abort this job.
    */

    job->state = IPP_JSTATE_ABORTED;

    close(job->fd);
    job->fd = -1;

    unlink(filename);

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to read print file.");
    return;
  }

  if (close(job->fd))
  {
    int error = errno;			/* Write error */

    job->state = IPP_JSTATE_ABORTED;
    job->fd    = -1;

    unlink(filename);

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to write print file: %s", strerror(error));
    return;
  }

  _cupsRWLockWrite(&(client->printer->rwlock));

  job->fd       = -1;
  job->filename = strdup(filename);
  job->state    = IPP_JSTATE_PENDING;

  _cupsRWUnlock(&(client->printer->rwlock));

 /*
  * Process the job...
  */

#if 0
  if (!_cupsThreadCreate((_cups_thread_func_t)process_job, job))
  {
    job->state = IPP_JSTATE_ABORTED;
    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL, "Unable to process job.");
    return;
  }

#else
  process_job(job);
#endif /* 0 */

 /*
  * Return the job info...
  */

  respond_ipp(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
}


/*
 * 'ipp_send_uri()' - Add a referenced document to a job object created with
 *                    Create-Job.
 */

static void
ipp_send_uri(_ipp_client_t *client)	/* I - Client */
{
  _ipp_job_t		*job;		/* Job information */
  ipp_attribute_t	*uri;		/* document-uri */
  char			scheme[256],	/* URI scheme */
			userpass[256],	/* Username and password info */
			hostname[256],	/* Hostname */
			resource[1024];	/* Resource path */
  int			port;		/* Port number */
  http_uri_status_t	uri_status;	/* URI decode status */
  http_encryption_t	encryption;	/* Encryption to use, if any */
  http_t		*http;		/* Connection for http/https URIs */
  http_status_t		status;		/* Access status for http/https URIs */
  int			infile;		/* Input file for local file URIs */
  char			filename[1024],	/* Filename buffer */
			buffer[4096];	/* Copy buffer */
  ssize_t		bytes;		/* Bytes read */
  ipp_attribute_t	*attr;		/* Current attribute */
  cups_array_t		*ra;		/* Attributes to send in response */
  static const char * const uri_status_strings[] =
  {					/* URI decode errors */
    "URI too large.",
    "Bad arguments to function.",
    "Bad resource in URI.",
    "Bad port number in URI.",
    "Bad hostname in URI.",
    "Bad username in URI.",
    "Bad scheme in URI.",
    "Bad/empty URI."
  };


 /*
  * Get the job...
  */

  if ((job = find_job(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    httpFlush(client->http);
    return;
  }

 /*
  * See if we already have a document for this job or the job has already
  * in a non-pending state...
  */

  if (job->state > IPP_JSTATE_HELD)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
                "Job is not in a pending state.");
    httpFlush(client->http);
    return;
  }
  else if (job->filename || job->fd >= 0)
  {
    respond_ipp(client, IPP_STATUS_ERROR_MULTIPLE_JOBS_NOT_SUPPORTED,
                "Multiple document jobs are not supported.");
    httpFlush(client->http);
    return;
  }

  if ((attr = ippFindAttribute(client->request, "last-document",
                               IPP_TAG_ZERO)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "Missing required last-document attribute.");
    httpFlush(client->http);
    return;
  }
  else if (ippGetValueTag(attr) != IPP_TAG_BOOLEAN || ippGetCount(attr) != 1 ||
           !ippGetBoolean(attr, 0))
  {
    respond_unsupported(client, attr);
    httpFlush(client->http);
    return;
  }

 /*
  * Validate document attributes...
  */

  if (!valid_doc_attributes(client))
  {
    httpFlush(client->http);
    return;
  }

 /*
  * Do we have a file to print?
  */

  if (httpGetState(client->http) == HTTP_STATE_POST_RECV)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "Unexpected document data following request.");
    return;
  }

 /*
  * Do we have a document URI?
  */

  if ((uri = ippFindAttribute(client->request, "document-uri",
                              IPP_TAG_URI)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing document-uri.");
    return;
  }

  if (ippGetCount(uri) != 1)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "Too many document-uri values.");
    return;
  }

  uri_status = httpSeparateURI(HTTP_URI_CODING_ALL, ippGetString(uri, 0, NULL),
                               scheme, sizeof(scheme), userpass,
                               sizeof(userpass), hostname, sizeof(hostname),
                               &port, resource, sizeof(resource));
  if (uri_status < HTTP_URI_STATUS_OK)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "Bad document-uri: %s",
                uri_status_strings[uri_status - HTTP_URI_STATUS_OVERFLOW]);
    return;
  }

  if (strcmp(scheme, "file") &&
#ifdef HAVE_SSL
      strcmp(scheme, "https") &&
#endif /* HAVE_SSL */
      strcmp(scheme, "http"))
  {
    respond_ipp(client, IPP_STATUS_ERROR_URI_SCHEME,
                "URI scheme \"%s\" not supported.", scheme);
    return;
  }

  if (!strcmp(scheme, "file") && access(resource, R_OK))
  {
    respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                "Unable to access URI: %s", strerror(errno));
    return;
  }

 /*
  * Get the document format for the job...
  */

  _cupsRWLockWrite(&(client->printer->rwlock));

  if ((attr = ippFindAttribute(job->attrs, "document-format",
                               IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else
    job->format = "application/octet-stream";

 /*
  * Create a file for the request data...
  */

  if (!_cups_strcasecmp(job->format, "image/jpeg"))
    snprintf(filename, sizeof(filename), "%s/%d.jpg",
             client->printer->directory, job->id);
  else if (!_cups_strcasecmp(job->format, "image/png"))
    snprintf(filename, sizeof(filename), "%s/%d.png",
             client->printer->directory, job->id);
  else if (!_cups_strcasecmp(job->format, "application/pdf"))
    snprintf(filename, sizeof(filename), "%s/%d.pdf",
             client->printer->directory, job->id);
  else if (!_cups_strcasecmp(job->format, "application/postscript"))
    snprintf(filename, sizeof(filename), "%s/%d.ps",
             client->printer->directory, job->id);
  else
    snprintf(filename, sizeof(filename), "%s/%d.prn",
             client->printer->directory, job->id);

  job->fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);

  _cupsRWUnlock(&(client->printer->rwlock));

  if (job->fd < 0)
  {
    job->state = IPP_JSTATE_ABORTED;

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to create print file: %s", strerror(errno));
    return;
  }

  if (!strcmp(scheme, "file"))
  {
    if ((infile = open(resource, O_RDONLY)) < 0)
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                  "Unable to access URI: %s", strerror(errno));
      return;
    }

    do
    {
      if ((bytes = read(infile, buffer, sizeof(buffer))) < 0 &&
          (errno == EAGAIN || errno == EINTR))
        bytes = 1;
      else if (bytes > 0 && write(job->fd, buffer, bytes) < bytes)
      {
	int error = errno;		/* Write error */

	job->state = IPP_JSTATE_ABORTED;

	close(job->fd);
	job->fd = -1;

	unlink(filename);
	close(infile);

	respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
		    "Unable to write print file: %s", strerror(error));
	return;
      }
    }
    while (bytes > 0);

    close(infile);
  }
  else
  {
#ifdef HAVE_SSL
    if (port == 443 || !strcmp(scheme, "https"))
      encryption = HTTP_ENCRYPTION_ALWAYS;
    else
#endif /* HAVE_SSL */
    encryption = HTTP_ENCRYPTION_IF_REQUESTED;

    if ((http = httpConnect2(hostname, port, NULL, AF_UNSPEC, encryption,
                             1, 30000, NULL)) == NULL)
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                  "Unable to connect to %s: %s", hostname,
		  cupsLastErrorString());
      job->state = IPP_JSTATE_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);
      return;
    }

    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_ACCEPT_LANGUAGE, "en");
    if (httpGet(http, resource))
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                  "Unable to GET URI: %s", strerror(errno));

      job->state = IPP_JSTATE_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);
      httpClose(http);
      return;
    }

    while ((status = httpUpdate(http)) == HTTP_STATUS_CONTINUE);

    if (status != HTTP_STATUS_OK)
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                  "Unable to GET URI: %s", httpStatus(status));

      job->state = IPP_JSTATE_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);
      httpClose(http);
      return;
    }

    while ((bytes = httpRead2(http, buffer, sizeof(buffer))) > 0)
    {
      if (write(job->fd, buffer, bytes) < bytes)
      {
	int error = errno;		/* Write error */

	job->state = IPP_JSTATE_ABORTED;

	close(job->fd);
	job->fd = -1;

	unlink(filename);
	httpClose(http);

	respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
		    "Unable to write print file: %s", strerror(error));
	return;
      }
    }

    httpClose(http);
  }

  if (close(job->fd))
  {
    int error = errno;		/* Write error */

    job->state = IPP_JSTATE_ABORTED;
    job->fd    = -1;

    unlink(filename);

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to write print file: %s", strerror(error));
    return;
  }

  _cupsRWLockWrite(&(client->printer->rwlock));

  job->fd       = -1;
  job->filename = strdup(filename);
  job->state    = IPP_JSTATE_PENDING;

  _cupsRWUnlock(&(client->printer->rwlock));

 /*
  * Process the job...
  */

#if 0
  if (!_cupsThreadCreate((_cups_thread_func_t)process_job, job))
  {
    job->state = IPP_JSTATE_ABORTED;
    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL, "Unable to process job.");
    return;
  }

#else
  process_job(job);
#endif /* 0 */

 /*
  * Return the job info...
  */

  respond_ipp(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
}


/*
 * 'ipp_validate_job()' - Validate job creation attributes.
 */

static void
ipp_validate_job(_ipp_client_t *client)	/* I - Client */
{
  if (valid_job_attributes(client))
    respond_ipp(client, IPP_STATUS_OK, NULL);
}


/*
 * 'process_client()' - Process client requests on a thread.
 */

static void *				/* O - Exit status */
process_client(_ipp_client_t *client)	/* I - Client */
{
 /*
  * Loop until we are out of requests or timeout (30 seconds)...
  */

  while (httpWait(client->http, 30000))
    if (!process_http(client))
      break;

 /*
  * Close the conection to the client and return...
  */

  delete_client(client);

  return (NULL);
}


/*
 * 'process_http()' - Process a HTTP request.
 */

int					/* O - 1 on success, 0 on failure */
process_http(_ipp_client_t *client)	/* I - Client connection */
{
  char			uri[1024];	/* URI */
  http_state_t		http_state;	/* HTTP state */
  http_status_t		http_status;	/* HTTP status */
  ipp_state_t		ipp_state;	/* State of IPP transfer */
  char			scheme[32],	/* Method/scheme */
			userpass[128],	/* Username:password */
			hostname[HTTP_MAX_HOST];
					/* Hostname */
  int			port;		/* Port number */
  const char		*encoding;	/* Content-Encoding value */
  static const char * const http_states[] =
  {					/* Strings for logging HTTP method */
    "WAITING",
    "OPTIONS",
    "GET",
    "GET_SEND",
    "HEAD",
    "POST",
    "POST_RECV",
    "POST_SEND",
    "PUT",
    "PUT_RECV",
    "DELETE",
    "TRACE",
    "CONNECT",
    "STATUS",
    "UNKNOWN_METHOD",
    "UNKNOWN_VERSION"
  };


 /*
  * Clear state variables...
  */

  ippDelete(client->request);
  ippDelete(client->response);

  client->request   = NULL;
  client->response  = NULL;
  client->operation = HTTP_STATE_WAITING;

 /*
  * Read a request from the connection...
  */

  while ((http_state = httpReadRequest(client->http, uri,
                                       sizeof(uri))) == HTTP_STATE_WAITING)
    usleep(1);

 /*
  * Parse the request line...
  */

  if (http_state == HTTP_STATE_ERROR)
  {
    if (httpError(client->http) == EPIPE)
      fprintf(stderr, "%s Client closed connection.\n", client->hostname);
    else
      fprintf(stderr, "%s Bad request line (%s).\n", client->hostname,
              strerror(httpError(client->http)));

    return (0);
  }
  else if (http_state == HTTP_STATE_UNKNOWN_METHOD)
  {
    fprintf(stderr, "%s Bad/unknown operation.\n", client->hostname);
    respond_http(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }
  else if (http_state == HTTP_STATE_UNKNOWN_VERSION)
  {
    fprintf(stderr, "%s Bad HTTP version.\n", client->hostname);
    respond_http(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }

  fprintf(stderr, "%s %s %s\n", client->hostname, http_states[http_state],
          uri);

 /*
  * Separate the URI into its components...
  */

  if (httpSeparateURI(HTTP_URI_CODING_MOST, uri, scheme, sizeof(scheme),
		      userpass, sizeof(userpass),
		      hostname, sizeof(hostname), &port,
		      client->uri, sizeof(client->uri)) < HTTP_URI_STATUS_OK)
  {
    fprintf(stderr, "%s Bad URI \"%s\".\n", client->hostname, uri);
    respond_http(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }

 /*
  * Process the request...
  */

  client->start     = time(NULL);
  client->operation = httpGetState(client->http);

 /*
  * Parse incoming parameters until the status changes...
  */

  while ((http_status = httpUpdate(client->http)) == HTTP_STATUS_CONTINUE);

  if (http_status != HTTP_STATUS_OK)
  {
    respond_http(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }

  if (!httpGetField(client->http, HTTP_FIELD_HOST)[0] &&
      httpGetVersion(client->http) >= HTTP_VERSION_1_1)
  {
   /*
    * HTTP/1.1 and higher require the "Host:" field...
    */

    respond_http(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }

 /*
  * Handle HTTP Upgrade...
  */

  if (!_cups_strcasecmp(httpGetField(client->http, HTTP_FIELD_CONNECTION),
                        "Upgrade"))
  {
    if (!respond_http(client, HTTP_STATUS_NOT_IMPLEMENTED, NULL, NULL, 0))
      return (0);
  }

 /*
  * Handle HTTP Expect...
  */

  if (httpGetExpect(client->http) &&
      (client->operation == HTTP_STATE_POST ||
       client->operation == HTTP_STATE_PUT))
  {
    if (httpGetExpect(client->http) == HTTP_STATUS_CONTINUE)
    {
     /*
      * Send 100-continue header...
      */

      if (!respond_http(client, HTTP_STATUS_CONTINUE, NULL, NULL, 0))
	return (0);
    }
    else
    {
     /*
      * Send 417-expectation-failed header...
      */

      if (!respond_http(client, HTTP_STATUS_EXPECTATION_FAILED, NULL, NULL, 0))
	return (0);
    }
  }

 /*
  * Handle new transfers...
  */

  encoding = httpGetContentEncoding(client->http);

  switch (client->operation)
  {
    case HTTP_STATE_OPTIONS :
       /*
	* Do HEAD/OPTIONS command...
	*/

	return (respond_http(client, HTTP_STATUS_OK, NULL, NULL, 0));

    case HTTP_STATE_HEAD :
        if (!strcmp(client->uri, "/icon.png"))
	  return (respond_http(client, HTTP_STATUS_OK, NULL, "image/png", 0));
	else if (!strcmp(client->uri, "/"))
	  return (respond_http(client, HTTP_STATUS_OK, NULL, "text/html", 0));
	else
	  return (respond_http(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0));
	break;

    case HTTP_STATE_GET :
        if (!strcmp(client->uri, "/icon.png"))
	{
	 /*
	  * Send PNG icon file.
	  */

          int		fd;		/* Icon file */
	  struct stat	fileinfo;	/* Icon file information */
	  char		buffer[4096];	/* Copy buffer */
	  ssize_t	bytes;		/* Bytes */

          fprintf(stderr, "Icon file is \"%s\".\n", client->printer->icon);

          if (!stat(client->printer->icon, &fileinfo) &&
	      (fd = open(client->printer->icon, O_RDONLY)) >= 0)
	  {
	    if (!respond_http(client, HTTP_STATUS_OK, NULL, "image/png",
	                      fileinfo.st_size))
	    {
	      close(fd);
	      return (0);
	    }

	    while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
	      httpWrite2(client->http, buffer, bytes);

	    httpFlushWrite(client->http);

	    close(fd);
	  }
	  else
	    return (respond_http(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0));
	}
	else if (!strcmp(client->uri, "/"))
	{
	 /*
	  * Show web status page...
	  */

          if (!respond_http(client, HTTP_STATUS_OK, encoding, "text/html", 0))
	    return (0);

          html_printf(client,
	              "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" "
		      "\"http://www.w3.org/TR/html4/strict.dtd\">\n"
		      "<html>\n"
		      "<head>\n"
		      "<title>%s</title>\n"
		      "<link rel=\"SHORTCUT ICON\" href=\"/icon.png\" "
		      "type=\"image/png\">\n"
		      "</head>\n"
		      "<body>\n"
		      "</body>\n"
		      "<h1><img align=\"right\" src=\"/icon.png\">%s</h1>\n"
		      "<p>%s, %d job(s).</p>\n"
		      "</body>\n"
		      "</html>\n",
		      client->printer->name, client->printer->name,
		      client->printer->state == IPP_PSTATE_IDLE ? "Idle" :
		          client->printer->state == IPP_PSTATE_PROCESSING ?
			  "Printing" : "Stopped",
		      cupsArrayCount(client->printer->jobs));
          httpWrite2(client->http, "", 0);

	  return (1);
	}
	else
	  return (respond_http(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0));
	break;

    case HTTP_STATE_POST :
	if (strcmp(httpGetField(client->http, HTTP_FIELD_CONTENT_TYPE),
	           "application/ipp"))
        {
	 /*
	  * Not an IPP request...
	  */

	  return (respond_http(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0));
	}

       /*
        * Read the IPP request...
	*/

	client->request = ippNew();

        while ((ipp_state = ippRead(client->http,
                                    client->request)) != IPP_STATE_DATA)
	{
	  if (ipp_state == IPP_STATE_ERROR)
	  {
            fprintf(stderr, "%s IPP read error (%s).\n", client->hostname,
	            cupsLastErrorString());
	    respond_http(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
	    return (0);
	  }
	}

       /*
        * Now that we have the IPP request, process the request...
	*/

        return (process_ipp(client));

    default :
        break; /* Anti-compiler-warning-code */
  }

  return (1);
}


/*
 * 'process_ipp()' - Process an IPP request.
 */

static int				/* O - 1 on success, 0 on error */
process_ipp(_ipp_client_t *client)	/* I - Client */
{
  ipp_tag_t		group;		/* Current group tag */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_attribute_t	*charset;	/* Character set attribute */
  ipp_attribute_t	*language;	/* Language attribute */
  ipp_attribute_t	*uri;		/* Printer URI attribute */
  int			major, minor;	/* Version number */
  const char		*name;		/* Name of attribute */


  debug_attributes("Request", client->request, 1);

 /*
  * First build an empty response message for this request...
  */

  client->operation_id = ippGetOperation(client->request);
  client->response     = ippNewResponse(client->request);

 /*
  * Then validate the request header and required attributes...
  */

  major = ippGetVersion(client->request, &minor);

  if (major < 1 || major > 2)
  {
   /*
    * Return an error, since we only support IPP 1.x and 2.x.
    */

    respond_ipp(client, IPP_STATUS_ERROR_VERSION_NOT_SUPPORTED,
                "Bad request version number %d.%d.", major, minor);
  }
  else if (ippGetRequestId(client->request) <= 0)
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "Bad request-id %d.",
                ippGetRequestId(client->request));
  else if (!ippFirstAttribute(client->request))
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "No attributes in request.");
  else
  {
   /*
    * Make sure that the attributes are provided in the correct order and
    * don't repeat groups...
    */

    for (attr = ippFirstAttribute(client->request),
             group = ippGetGroupTag(attr);
	 attr;
	 attr = ippNextAttribute(client->request))
    {
      if (ippGetGroupTag(attr) < group && ippGetGroupTag(attr) != IPP_TAG_ZERO)
      {
       /*
	* Out of order; return an error...
	*/

	respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
		    "Attribute groups are out of order (%x < %x).",
		    ippGetGroupTag(attr), group);
	break;
      }
      else
	group = ippGetGroupTag(attr);
    }

    if (!attr)
    {
     /*
      * Then make sure that the first three attributes are:
      *
      *     attributes-charset
      *     attributes-natural-language
      *     printer-uri/job-uri
      */

      attr = ippFirstAttribute(client->request);
      name = ippGetName(attr);
      if (attr && name && !strcmp(name, "attributes-charset") &&
	  ippGetValueTag(attr) == IPP_TAG_CHARSET)
	charset = attr;
      else
	charset = NULL;

      attr = ippNextAttribute(client->request);
      name = ippGetName(attr);

      if (attr && name && !strcmp(name, "attributes-natural-language") &&
	  ippGetValueTag(attr) == IPP_TAG_LANGUAGE)
	language = attr;
      else
	language = NULL;

      if ((attr = ippFindAttribute(client->request, "printer-uri",
                                   IPP_TAG_URI)) != NULL)
	uri = attr;
      else if ((attr = ippFindAttribute(client->request, "job-uri",
                                        IPP_TAG_URI)) != NULL)
	uri = attr;
      else
	uri = NULL;

      if (charset &&
          _cups_strcasecmp(ippGetString(charset, 0, NULL), "us-ascii") &&
          _cups_strcasecmp(ippGetString(charset, 0, NULL), "utf-8"))
      {
       /*
        * Bad character set...
	*/

	respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
	            "Unsupported character set \"%s\".",
	            ippGetString(charset, 0, NULL));
      }
      else if (!charset || !language || !uri)
      {
       /*
	* Return an error, since attributes-charset,
	* attributes-natural-language, and printer-uri/job-uri are required
	* for all operations.
	*/

	respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
	            "Missing required attributes.");
      }
      else
      {
        char		scheme[32],	/* URI scheme */
			userpass[32],	/* Username/password in URI */
			host[256],	/* Host name in URI */
			resource[256];	/* Resource path in URI */
	int		port;		/* Port number in URI */

        name = ippGetName(uri);

        if (httpSeparateURI(HTTP_URI_CODING_ALL, ippGetString(uri, 0, NULL),
                            scheme, sizeof(scheme),
                            userpass, sizeof(userpass),
                            host, sizeof(host), &port,
                            resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
	  respond_ipp(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES,
	              "Bad %s value '%s'.", name, ippGetString(uri, 0, NULL));
        else if ((!strcmp(name, "job-uri") &&
                  strncmp(resource, "/ipp/print/", 11)) ||
                 (!strcmp(name, "printer-uri") &&
                  strcmp(resource, "/ipp/print")))
	  respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "%s %s not found.",
		      name, ippGetString(uri, 0, NULL));
	else
	{
	 /*
	  * Try processing the operation...
	  */

	  switch (ippGetOperation(client->request))
	  {
	    case IPP_OP_PRINT_JOB :
		ipp_print_job(client);
		break;

	    case IPP_OP_PRINT_URI :
		ipp_print_uri(client);
		break;

	    case IPP_OP_VALIDATE_JOB :
		ipp_validate_job(client);
		break;

	    case IPP_OP_CREATE_JOB :
		ipp_create_job(client);
		break;

	    case IPP_OP_SEND_DOCUMENT :
		ipp_send_document(client);
		break;

	    case IPP_OP_SEND_URI :
		ipp_send_uri(client);
		break;

	    case IPP_OP_CANCEL_JOB :
		ipp_cancel_job(client);
		break;

	    case IPP_OP_GET_JOB_ATTRIBUTES :
		ipp_get_job_attributes(client);
		break;

	    case IPP_OP_GET_JOBS :
		ipp_get_jobs(client);
		break;

	    case IPP_OP_GET_PRINTER_ATTRIBUTES :
		ipp_get_printer_attributes(client);
		break;

	    default :
		respond_ipp(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED,
			    "Operation not supported.");
		break;
	  }
	}
      }
    }
  }

 /*
  * Send the HTTP header and return...
  */

  if (httpGetState(client->http) != HTTP_STATE_POST_SEND)
    httpFlush(client->http);		/* Flush trailing (junk) data */

  return (respond_http(client, HTTP_STATUS_OK, NULL, "application/ipp",
                       ippLength(client->response)));
}


/*
 * 'process_job()' - Process a print job.
 */

static void *				/* O - Thread exit status */
process_job(_ipp_job_t *job)		/* I - Job */
{
  job->state          = IPP_JSTATE_PROCESSING;
  job->printer->state = IPP_PSTATE_PROCESSING;

  if (job->printer->command)
  {
   /*
    * Execute a command with the job spool file and wait for it to complete...
    */

    int 	pid,			/* Process ID */
		status;			/* Exit status */
    time_t	start,			/* Start time */
		end;			/* End time */

    fprintf(stderr, "Running command \"%s %s\".\n", job->printer->command,
            job->filename);
    time(&start);

    if ((pid = fork()) == 0)
    {
     /*
      * Child comes here...
      */

      execlp(job->printer->command, job->printer->command, job->filename,
             (void *)NULL);
      exit(errno);
    }
    else if (pid < 0)
    {
     /*
      * Unable to fork process...
      */

      perror("Unable to start job processing command");
    }
    else
    {
     /*
      * Wait for child to complete...
      */

#ifdef HAVE_WAITPID
      while (waitpid(pid, &status, 0) < 0);
#else
      while (wait(&status) < 0);
#endif /* HAVE_WAITPID */

      if (status)
      {
        if (WIFEXITED(status))
	  fprintf(stderr, "Command \"%s\" exited with status %d.\n",
	          job->printer->command, WEXITSTATUS(status));
        else
	  fprintf(stderr, "Command \"%s\" terminated with signal %d.\n",
	          job->printer->command, WTERMSIG(status));
      }
      else
	fprintf(stderr, "Command \"%s\" completed successfully.\n",
		job->printer->command);
    }

   /*
    * Make sure processing takes at least 5 seconds...
    */

    time(&end);
    if ((end - start) < 5)
      sleep(5);
  }
  else
  {
   /*
    * Sleep for a random amount of time to simulate job processing.
    */

    sleep(5 + (rand() % 11));
  }

  if (job->cancel)
    job->state = IPP_JSTATE_CANCELED;
  else
    job->state = IPP_JSTATE_COMPLETED;

  job->completed           = time(NULL);
  job->printer->state      = IPP_PSTATE_IDLE;
  job->printer->active_job = NULL;

  return (NULL);
}


#ifdef HAVE_DNSSD
/*
 * 'register_printer()' - Register a printer object via Bonjour.
 */

static int				/* O - 1 on success, 0 on error */
register_printer(
    _ipp_printer_t *printer,		/* I - Printer */
    const char     *location,		/* I - Location */
    const char     *make,		/* I - Manufacturer */
    const char     *model,		/* I - Model name */
    const char     *formats,		/* I - Supported formats */
    const char     *adminurl,		/* I - Web interface URL */
    int            color,		/* I - 1 = color, 0 = monochrome */
    int            duplex,		/* I - 1 = duplex, 0 = simplex */
    const char     *subtype)		/* I - Service subtype */
{
  DNSServiceErrorType	error;		/* Error from Bonjour */
  char			make_model[256],/* Make and model together */
			product[256],	/* Product string */
			regtype[256];	/* Bonjour service type */


 /*
  * Build the TXT record for IPP...
  */

  snprintf(make_model, sizeof(make_model), "%s %s", make, model);
  snprintf(product, sizeof(product), "(%s)", model);

  TXTRecordCreate(&(printer->ipp_txt), 1024, NULL);
  TXTRecordSetValue(&(printer->ipp_txt), "rp", 9, "ipp/print");
  TXTRecordSetValue(&(printer->ipp_txt), "ty", (uint8_t)strlen(make_model),
                    make_model);
  TXTRecordSetValue(&(printer->ipp_txt), "adminurl", (uint8_t)strlen(adminurl),
                    adminurl);
  if (*location)
    TXTRecordSetValue(&(printer->ipp_txt), "note", (uint8_t)strlen(location),
		      location);
  TXTRecordSetValue(&(printer->ipp_txt), "product", (uint8_t)strlen(product),
                    product);
  TXTRecordSetValue(&(printer->ipp_txt), "pdl", (uint8_t)strlen(formats),
                    formats);
  TXTRecordSetValue(&(printer->ipp_txt), "Color", 1, color ? "T" : "F");
  TXTRecordSetValue(&(printer->ipp_txt), "Duplex", 1, duplex ? "T" : "F");
  TXTRecordSetValue(&(printer->ipp_txt), "usb_MFG", (uint8_t)strlen(make),
                    make);
  TXTRecordSetValue(&(printer->ipp_txt), "usb_MDL", (uint8_t)strlen(model),
                    model);

 /*
  * Create a shared service reference for Bonjour...
  */

  if ((error = DNSServiceCreateConnection(&(printer->common_ref)))
          != kDNSServiceErr_NoError)
  {
    fprintf(stderr, "Unable to create mDNSResponder connection: %d\n", error);
    return (0);
  }

 /*
  * Register the _printer._tcp (LPD) service type with a port number of 0 to
  * defend our service name but not actually support LPD...
  */

  printer->printer_ref = printer->common_ref;

  if ((error = DNSServiceRegister(&(printer->printer_ref),
                                  kDNSServiceFlagsShareConnection,
                                  0 /* interfaceIndex */, printer->dnssd_name,
				  "_printer._tcp", NULL /* domain */,
				  NULL /* host */, 0 /* port */, 0 /* txtLen */,
				  NULL /* txtRecord */,
			          (DNSServiceRegisterReply)dnssd_callback,
			          printer)) != kDNSServiceErr_NoError)
  {
    fprintf(stderr, "Unable to register \"%s._printer._tcp\": %d\n",
            printer->dnssd_name, error);
    return (0);
  }

 /*
  * Then register the _ipp._tcp (IPP) service type with the real port number to
  * advertise our IPP printer...
  */

  printer->ipp_ref = printer->common_ref;

  if (subtype && *subtype)
    snprintf(regtype, sizeof(regtype), "_ipp._tcp,%s", subtype);
  else
    strlcpy(regtype, "_ipp._tcp", sizeof(regtype));

  if ((error = DNSServiceRegister(&(printer->ipp_ref),
                                  kDNSServiceFlagsShareConnection,
                                  0 /* interfaceIndex */, printer->dnssd_name,
				  regtype, NULL /* domain */,
				  NULL /* host */, htons(printer->port),
				  TXTRecordGetLength(&(printer->ipp_txt)),
				  TXTRecordGetBytesPtr(&(printer->ipp_txt)),
			          (DNSServiceRegisterReply)dnssd_callback,
			          printer)) != kDNSServiceErr_NoError)
  {
    fprintf(stderr, "Unable to register \"%s.%s\": %d\n",
            printer->dnssd_name, regtype, error);
    return (0);
  }

#  if 0 /* ifdef HAVE_SSL */
 /*
  * Then register the _ipps._tcp (IPP) service type with the real port number to
  * advertise our IPP printer...
  */

  printer->ipps_ref = printer->common_ref;

  if (subtype && *subtype)
    snprintf(regtype, sizeof(regtype), "_ipps._tcp,%s", subtype);
  else
    strlcpy(regtype, "_ipps._tcp", sizeof(regtype));

  if ((error = DNSServiceRegister(&(printer->ipps_ref),
                                  kDNSServiceFlagsShareConnection,
                                  0 /* interfaceIndex */, printer->dnssd_name,
				  regtype, NULL /* domain */,
				  NULL /* host */, htons(printer->port),
				  TXTRecordGetLength(&(printer->ipp_txt)),
				  TXTRecordGetBytesPtr(&(printer->ipp_txt)),
			          (DNSServiceRegisterReply)dnssd_callback,
			          printer)) != kDNSServiceErr_NoError)
  {
    fprintf(stderr, "Unable to register \"%s.%s\": %d\n",
            printer->dnssd_name, regtype, error);
    return (0);
  }
#  endif /* HAVE_SSL */

 /*
  * Similarly, register the _http._tcp,_printer (HTTP) service type with the
  * real port number to advertise our IPP printer...
  */

  printer->http_ref = printer->common_ref;

  if ((error = DNSServiceRegister(&(printer->http_ref),
                                  kDNSServiceFlagsShareConnection,
                                  0 /* interfaceIndex */, printer->dnssd_name,
				  "_http._tcp,_printer", NULL /* domain */,
				  NULL /* host */, htons(printer->port),
				  0 /* txtLen */, NULL, /* txtRecord */
			          (DNSServiceRegisterReply)dnssd_callback,
			          printer)) != kDNSServiceErr_NoError)
  {
    fprintf(stderr, "Unable to register \"%s.%s\": %d\n",
            printer->dnssd_name, regtype, error);
    return (0);
  }

  return (1);
}
#endif /* HAVE_DNSSD */


/*
 * 'respond_http()' - Send a HTTP response.
 */

int					/* O - 1 on success, 0 on failure */
respond_http(
    _ipp_client_t *client,		/* I - Client */
    http_status_t code,			/* I - HTTP status of response */
    const char    *content_encoding,	/* I - Content-Encoding of response */
    const char    *type,		/* I - MIME media type of response */
    size_t        length)		/* I - Length of response */
{
  char	message[1024];			/* Text message */


  fprintf(stderr, "%s %s\n", client->hostname, httpStatus(code));

  if (code == HTTP_STATUS_CONTINUE)
  {
   /*
    * 100-continue doesn't send any headers...
    */

    return (httpWriteResponse(client->http, HTTP_STATUS_CONTINUE) == 0);
  }

 /*
  * Format an error message...
  */

  if (!type && !length && code != HTTP_STATUS_OK)
  {
    snprintf(message, sizeof(message), "%d - %s\n", code, httpStatus(code));

    type   = "text/plain";
    length = strlen(message);
  }
  else
    message[0] = '\0';

 /*
  * Send the HTTP response header...
  */

  httpClearFields(client->http);

  if (code == HTTP_STATUS_METHOD_NOT_ALLOWED ||
      client->operation == HTTP_STATE_OPTIONS)
    httpSetField(client->http, HTTP_FIELD_ALLOW, "GET, HEAD, OPTIONS, POST");

  if (type)
  {
    if (!strcmp(type, "text/html"))
      httpSetField(client->http, HTTP_FIELD_CONTENT_TYPE,
                   "text/html; charset=utf-8");
    else
      httpSetField(client->http, HTTP_FIELD_CONTENT_TYPE, type);

    if (content_encoding)
      httpSetField(client->http, HTTP_FIELD_CONTENT_ENCODING, content_encoding);
  }

  httpSetLength(client->http, length);

  if (httpWriteResponse(client->http, code) < 0)
    return (0);

 /*
  * Send the response data...
  */

  if (message[0])
  {
   /*
    * Send a plain text message.
    */

    if (httpPrintf(client->http, "%s", message) < 0)
      return (0);

    if (httpWrite2(client->http, "", 0) < 0)
      return (0);
  }
  else if (client->response)
  {
   /*
    * Send an IPP response...
    */

    debug_attributes("Response", client->response, 2);

    ippSetState(client->response, IPP_STATE_IDLE);

    if (ippWrite(client->http, client->response) != IPP_STATE_DATA)
      return (0);
  }

  return (1);
}


/*
 * 'respond_ipp()' - Send an IPP response.
 */

static void
respond_ipp(_ipp_client_t *client,	/* I - Client */
            ipp_status_t  status,	/* I - status-code */
	    const char    *message,	/* I - printf-style status-message */
	    ...)			/* I - Additional args as needed */
{
  const char	*formatted = NULL;	/* Formatted message */


  ippSetStatusCode(client->response, status);

  if (message)
  {
    va_list		ap;		/* Pointer to additional args */
    ipp_attribute_t	*attr;		/* New status-message attribute */

    va_start(ap, message);
    if ((attr = ippFindAttribute(client->response, "status-message",
				 IPP_TAG_TEXT)) != NULL)
      ippSetStringfv(client->response, &attr, 0, message, ap);
    else
      attr = ippAddStringfv(client->response, IPP_TAG_OPERATION, IPP_TAG_TEXT,
			    "status-message", NULL, message, ap);
    va_end(ap);

    formatted = ippGetString(attr, 0, NULL);
  }

  if (formatted)
    fprintf(stderr, "%s %s %s (%s)\n", client->hostname,
	    ippOpString(client->operation_id), ippErrorString(status),
	    formatted);
  else
    fprintf(stderr, "%s %s %s\n", client->hostname,
	    ippOpString(client->operation_id), ippErrorString(status));
}


/*
 * 'respond_unsupported()' - Respond with an unsupported attribute.
 */

static void
respond_unsupported(
    _ipp_client_t   *client,		/* I - Client */
    ipp_attribute_t *attr)		/* I - Atribute */
{
  ipp_attribute_t	*temp;		/* Copy of attribute */


  respond_ipp(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES,
              "Unsupported %s %s%s value.", ippGetName(attr),
              ippGetCount(attr) > 1 ? "1setOf " : "",
	      ippTagString(ippGetValueTag(attr)));

  temp = ippCopyAttribute(client->response, attr, 0);
  ippSetGroupTag(client->response, &temp, IPP_TAG_UNSUPPORTED_GROUP);
}


/*
 * 'run_printer()' - Run the printer service.
 */

static void
run_printer(_ipp_printer_t *printer)	/* I - Printer */
{
  int		num_fds;		/* Number of file descriptors */
  struct pollfd	polldata[3];		/* poll() data */
  int		timeout;		/* Timeout for poll() */
  _ipp_client_t	*client;		/* New client */


 /*
  * Setup poll() data for the Bonjour service socket and IPv4/6 listeners...
  */

  polldata[0].fd     = printer->ipv4;
  polldata[0].events = POLLIN;

  polldata[1].fd     = printer->ipv6;
  polldata[1].events = POLLIN;

  num_fds = 2;

#ifdef HAVE_DNSSD
  polldata[num_fds   ].fd     = DNSServiceRefSockFD(printer->common_ref);
  polldata[num_fds ++].events = POLLIN;
#endif /* HAVE_DNSSD */

 /*
  * Loop until we are killed or have a hard error...
  */

  for (;;)
  {
    if (cupsArrayCount(printer->jobs))
      timeout = 10;
    else
      timeout = -1;

    if (poll(polldata, num_fds, timeout) < 0 && errno != EINTR)
    {
      perror("poll() failed");
      break;
    }

    if (polldata[0].revents & POLLIN)
    {
      if ((client = create_client(printer, printer->ipv4)) != NULL)
      {
	if (!_cupsThreadCreate((_cups_thread_func_t)process_client, client))
	{
	  perror("Unable to create client thread");
	  delete_client(client);
	}
      }
    }

    if (polldata[1].revents & POLLIN)
    {
      if ((client = create_client(printer, printer->ipv6)) != NULL)
      {
	if (!_cupsThreadCreate((_cups_thread_func_t)process_client, client))
	{
	  perror("Unable to create client thread");
	  delete_client(client);
	}
      }
    }

#ifdef HAVE_DNSSD
    if (polldata[2].revents & POLLIN)
      DNSServiceProcessResult(printer->common_ref);
#endif /* HAVE_DNSSD */

   /*
    * Clean out old jobs...
    */

    clean_jobs(printer);
  }
}


/*
 * 'usage()' - Show program usage.
 */

static void
usage(int status)			/* O - Exit status */
{
  if (!status)
  {
    puts(CUPS_SVERSION " - Copyright 2010-2013 by Apple Inc. All rights "
         "reserved.");
    puts("");
  }

  puts("Usage: ippserver [options] \"name\"");
  puts("");
  puts("Options:");
  puts("-2                      Supports 2-sided printing (default=1-sided)");
  puts("-M manufacturer         Manufacturer name (default=Test)");
  puts("-P                      PIN printing mode");
  puts("-c command              Run command for every print job");
  printf("-d spool-directory      Spool directory "
         "(default=/tmp/ippserver.%d)\n", (int)getpid());
  puts("-f type/subtype[,...]   List of supported types "
       "(default=application/pdf,image/jpeg)");
  puts("-h                      Show program help");
  puts("-i iconfile.png         PNG icon file (default=printer.png)");
  puts("-k                      Keep job spool files");
  puts("-l location             Location of printer (default=empty string)");
  puts("-m model                Model name (default=Printer)");
  puts("-n hostname             Hostname for printer");
  puts("-p port                 Port number (default=auto)");
#ifdef HAVE_DNSSD
  puts("-r subtype              Bonjour service subtype (default=_print)");
#endif /* HAVE_DNSSD */
  puts("-s speed[,color-speed]  Speed in pages per minute (default=10,0)");
  puts("-v[vvv]                 Be (very) verbose");

  exit(status);
}


/*
 * 'valid_doc_attributes()' - Determine whether the document attributes are
 *                            valid.
 *
 * When one or more document attributes are invalid, this function adds a
 * suitable response and attributes to the unsupported group.
 */

static int				/* O - 1 if valid, 0 if not */
valid_doc_attributes(
    _ipp_client_t *client)		/* I - Client */
{
  int			valid = 1;	/* Valid attributes? */
  ipp_op_t		op = ippGetOperation(client->request);
					/* IPP operation */
  const char		*op_name = ippOpString(op);
					/* IPP operation name */
  ipp_attribute_t	*attr,		/* Current attribute */
			*supported;	/* xxx-supported attribute */
  const char		*compression = NULL,
					/* compression value */
			*format = NULL;	/* document-format value */


 /*
  * Check operation attributes...
  */

  if ((attr = ippFindAttribute(client->request, "compression",
                               IPP_TAG_ZERO)) != NULL)
  {
   /*
    * If compression is specified, only accept a supported value in a Print-Job
    * or Send-Document request...
    */

    compression = ippGetString(attr, 0, NULL);
    supported   = ippFindAttribute(client->printer->attrs,
                                   "compression-supported", IPP_TAG_KEYWORD);

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD ||
        ippGetGroupTag(attr) != IPP_TAG_OPERATION ||
        (op != IPP_OP_PRINT_JOB && op != IPP_OP_SEND_DOCUMENT &&
         op != IPP_OP_VALIDATE_JOB) ||
        !ippContainsString(supported, compression))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else
    {
      fprintf(stderr, "%s %s compression=\"%s\"\n",
              client->hostname, op_name, compression);

      if (strcmp(compression, "none"))
        httpSetField(client->http, HTTP_FIELD_CONTENT_ENCODING, compression);
    }
  }

 /*
  * Is it a format we support?
  */

  if ((attr = ippFindAttribute(client->request, "document-format",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_MIMETYPE ||
        ippGetGroupTag(attr) != IPP_TAG_OPERATION)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else
    {
      format = ippGetString(attr, 0, NULL);

      fprintf(stderr, "%s %s document-format=\"%s\"\n",
	      client->hostname, op_name, format);
    }
  }
  else
  {
    format = ippGetString(ippFindAttribute(client->printer->attrs,
                                           "document-format-default",
                                           IPP_TAG_MIMETYPE), 0, NULL);
    if (!format)
      format = "application/octet-stream"; /* Should never happen */

    attr = ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE,
			"document-format", NULL, format);
  }

  if (!strcmp(format, "application/octet-stream") &&
      (ippGetOperation(client->request) == IPP_OP_PRINT_JOB ||
       ippGetOperation(client->request) == IPP_OP_SEND_DOCUMENT))
  {
   /*
    * Auto-type the file using the first 4 bytes of the file...
    */

    unsigned char	header[4];	/* First 4 bytes of file */

    memset(header, 0, sizeof(header));
    httpPeek(client->http, (char *)header, sizeof(header));

    if (!memcmp(header, "%PDF", 4))
      format = "application/pdf";
    else if (!memcmp(header, "%!", 2))
      format = "application/postscript";
    else if (!memcmp(header, "\377\330\377", 3) &&
	     header[3] >= 0xe0 && header[3] <= 0xef)
      format = "image/jpeg";
    else if (!memcmp(header, "\211PNG", 4))
      format = "image/png";

    if (format)
      fprintf(stderr, "%s %s Auto-typed document-format=\"%s\"\n",
	      client->hostname, op_name, format);

    if (!attr)
      attr = ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE,
                          "document-format", NULL, format);
    else
      ippSetString(client->request, &attr, 0, format);
  }

  if (op != IPP_OP_CREATE_JOB &&
      (supported = ippFindAttribute(client->printer->attrs,
                                    "document-format-supported",
			            IPP_TAG_MIMETYPE)) != NULL &&
      !ippContainsString(supported, format))
  {
    respond_unsupported(client, attr);
    valid = 0;
  }

  return (valid);
}


/*
 * 'valid_job_attributes()' - Determine whether the job attributes are valid.
 *
 * When one or more job attributes are invalid, this function adds a suitable
 * response and attributes to the unsupported group.
 */

static int				/* O - 1 if valid, 0 if not */
valid_job_attributes(
    _ipp_client_t *client)		/* I - Client */
{
  int			i,		/* Looping var */
			valid = 1;	/* Valid attributes? */
  ipp_attribute_t	*attr,		/* Current attribute */
			*supported;	/* xxx-supported attribute */


 /*
  * Check operation attributes...
  */

  valid = valid_doc_attributes(client);

 /*
  * Check the various job template attributes...
  */

  if ((attr = ippFindAttribute(client->request, "copies",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER ||
        ippGetInteger(attr, 0) < 1 || ippGetInteger(attr, 0) > 999)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "ipp-attribute-fidelity",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_BOOLEAN)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-hold-until",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 ||
        (ippGetValueTag(attr) != IPP_TAG_NAME &&
	 ippGetValueTag(attr) != IPP_TAG_NAMELANG &&
	 ippGetValueTag(attr) != IPP_TAG_KEYWORD) ||
	strcmp(ippGetString(attr, 0, NULL), "no-hold"))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-name",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 ||
        (ippGetValueTag(attr) != IPP_TAG_NAME &&
	 ippGetValueTag(attr) != IPP_TAG_NAMELANG))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-priority",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER ||
        ippGetInteger(attr, 0) < 1 || ippGetInteger(attr, 0) > 100)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-sheets",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 ||
        (ippGetValueTag(attr) != IPP_TAG_NAME &&
	 ippGetValueTag(attr) != IPP_TAG_NAMELANG &&
	 ippGetValueTag(attr) != IPP_TAG_KEYWORD) ||
	strcmp(ippGetString(attr, 0, NULL), "none"))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "media",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 ||
        (ippGetValueTag(attr) != IPP_TAG_NAME &&
	 ippGetValueTag(attr) != IPP_TAG_NAMELANG &&
	 ippGetValueTag(attr) != IPP_TAG_KEYWORD))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else
    {
      for (i = 0;
           i < (int)(sizeof(media_supported) / sizeof(media_supported[0]));
	   i ++)
        if (!strcmp(ippGetString(attr, 0, NULL), media_supported[i]))
	  break;

      if (i >= (int)(sizeof(media_supported) / sizeof(media_supported[0])))
      {
	respond_unsupported(client, attr);
	valid = 0;
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "media-col",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 ||
        ippGetValueTag(attr) != IPP_TAG_BEGIN_COLLECTION)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    /* TODO: check for valid media-col */
  }

  if ((attr = ippFindAttribute(client->request, "multiple-document-handling",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD ||
        (strcmp(ippGetString(attr, 0, NULL),
		"separate-documents-uncollated-copies") &&
	 strcmp(ippGetString(attr, 0, NULL),
		"separate-documents-collated-copies")))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "orientation-requested",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_ENUM ||
        ippGetInteger(attr, 0) < IPP_ORIENT_PORTRAIT ||
        ippGetInteger(attr, 0) > IPP_ORIENT_REVERSE_PORTRAIT)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "page-ranges",
                               IPP_TAG_ZERO)) != NULL)
  {
    respond_unsupported(client, attr);
      valid = 0;
  }

  if ((attr = ippFindAttribute(client->request, "print-quality",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_ENUM ||
        ippGetInteger(attr, 0) < IPP_QUALITY_DRAFT ||
        ippGetInteger(attr, 0) > IPP_QUALITY_HIGH)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "printer-resolution",
                               IPP_TAG_ZERO)) != NULL)
  {
    respond_unsupported(client, attr);
    valid = 0;
  }

  if ((attr = ippFindAttribute(client->request, "sides",
                               IPP_TAG_ZERO)) != NULL)
  {
    const char *sides = NULL;		/* "sides" value... */

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }

    sides = ippGetString(attr, 0, NULL);

    if ((supported = ippFindAttribute(client->printer->attrs, "sides-supported",
                                      IPP_TAG_KEYWORD)) != NULL)
    {
      if (!ippContainsString(supported, sides))
      {
	respond_unsupported(client, attr);
	valid = 0;
      }
    }
    else if (strcmp(sides, "one-sided"))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  return (valid);
}


/*
 * End of "$Id: ippserver.c 11097 2013-07-04 15:54:36Z msweet $".
 */
