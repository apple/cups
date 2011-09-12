/*
 * "$Id$"
 *
 *   Sample IPP/2.0 server for CUPS.
 *
 *   Copyright 2010-2011 by Apple Inc.
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
 *   copy_attribute()		  - Copy a single attribute.
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
 * Include necessary headers...
 */

#include <cups/cups-private.h>
#ifdef HAVE_DNSSD
#  include <dns_sd.h>
#endif /* HAVE_DNSSD */
#include <sys/stat.h>
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
			http_ref,	/* Bonjour HTTP service */
			printer_ref;	/* Bonjour LPD service */
  TXTRecordRef		ipp_txt;	/* Bonjour IPP TXT record */
  char			*dnssd_name;	/* printer-dnssd-name */
#endif /* HAVE_DNSSD */
  char			*name,		/* printer-name */
			*icon,		/* Icon filename */
			*directory,	/* Spool directory */
			*hostname,	/* Hostname */
			*uri;		/* printer-uri-supported */
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
  char			*name,		/* job-name */
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
  http_t		http;		/* HTTP connection */
  ipp_t			*request,	/* IPP request */
			*response;	/* IPP response */
  time_t		start;		/* Request start time */
  http_state_t		operation;	/* Request operation */
  ipp_op_t		operation_id;	/* IPP operation-id */
  char			uri[1024];	/* Request URI */
  http_addr_t		addr;		/* Client address */
  _ipp_printer_t	*printer;	/* Printer */
  _ipp_job_t		*job;		/* Current job, if any */
} _ipp_client_t;


/*
 * Local functions...
 */

static void		clean_jobs(_ipp_printer_t *printer);
static int		compare_jobs(_ipp_job_t *a, _ipp_job_t *b);
static ipp_attribute_t	*copy_attribute(ipp_t *to, ipp_attribute_t *attr,
		                        ipp_tag_t group_tag, int quickcopy);
static void		copy_attributes(ipp_t *to, ipp_t *from, cups_array_t *ra,
			                ipp_tag_t group_tag, int quickcopy);
static void		copy_job_attributes(_ipp_client_t *client,
			                    _ipp_job_t *job, cups_array_t *ra);
static _ipp_client_t	*create_client(_ipp_printer_t *printer, int sock);
static _ipp_job_t	*create_job(_ipp_client_t *client);
static int		create_listener(int family, int *port);
static ipp_t		*create_media_col(const char *media, const char *type,
					  int width, int length, int margins);
static _ipp_printer_t	*create_printer(const char *servername,
			                const char *name, const char *location,
			                const char *make, const char *model,
					const char *icon,
					const char *docformats, int ppm,
					int ppm_color, int duplex, int port,
#ifdef HAVE_DNSSD
					const char *regtype,
#endif /* HAVE_DNSSD */
					const char *directory);
static cups_array_t	*create_requested_array(_ipp_client_t *client);
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
		*servername = NULL,	/* Server host name */
		*name = NULL,		/* Printer name */
		*location = "",		/* Location of printer */
		*make = "Test",		/* Manufacturer */
		*model = "Printer",	/* Model */
		*icon = "printer.png",	/* Icon file */
		*formats = "application/pdf,image/jpeg";
	      				/* Supported formats */
#ifdef HAVE_DNSSD
  const char	*regtype = "_ipp._tcp";	/* Bonjour service type */
#endif /* HAVE_DNSSD */
  int		port = 8631,		/* Port number (0 = auto) TODO: FIX */
		duplex = 0,		/* Duplex mode */
		ppm = 10,		/* Pages per minute for mono */
		ppm_color = 0;		/* Pages per minute for color */
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
	  case 'r' : /* -r regtype */
	      i ++;
	      if (i >= argc)
	        usage(1);
	      regtype = argv[i];
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
                                formats, ppm, ppm_color, duplex, port,
#ifdef HAVE_DNSSD
				regtype,
#endif /* HAVE_DNSSD */
				directory)) == NULL)
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
 * 'copy_attribute()' - Copy a single attribute.
 */

static ipp_attribute_t *		/* O - New attribute */
copy_attribute(
    ipp_t           *to,		/* O - Destination request/response */
    ipp_attribute_t *attr,		/* I - Attribute to copy */
    ipp_tag_t       group_tag,		/* I - Group to put the copy in */
    int             quickcopy)		/* I - Do a quick copy? */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*toattr;	/* Destination attribute */


  if (Verbosity && attr->name)
  {
    char	buffer[2048];		/* Attribute value */

    _ippAttrString(attr, buffer, sizeof(buffer));

    fprintf(stderr, "Copying %s (%s%s) %s\n", attr->name,
	    attr->num_values > 1 ? "1setOf " : "",
	    ippTagString(attr->value_tag & ~IPP_TAG_COPY), buffer);
  }

  switch (attr->value_tag & ~IPP_TAG_COPY)
  {
    case IPP_TAG_ZERO :
        toattr = ippAddSeparator(to);
	break;

    case IPP_TAG_INTEGER :
    case IPP_TAG_ENUM :
        toattr = ippAddIntegers(to, group_tag, attr->value_tag,
	                        attr->name, attr->num_values, NULL);

        for (i = 0; i < attr->num_values; i ++)
	  toattr->values[i].integer = attr->values[i].integer;
        break;

    case IPP_TAG_BOOLEAN :
        toattr = ippAddBooleans(to, group_tag, attr->name,
	                        attr->num_values, NULL);

        for (i = 0; i < attr->num_values; i ++)
	  toattr->values[i].boolean = attr->values[i].boolean;
        break;

    case IPP_TAG_TEXT :
    case IPP_TAG_NAME :
    case IPP_TAG_KEYWORD :
    case IPP_TAG_URI :
    case IPP_TAG_URISCHEME :
    case IPP_TAG_CHARSET :
    case IPP_TAG_LANGUAGE :
    case IPP_TAG_MIMETYPE :
        toattr = ippAddStrings(to, group_tag,
	                       (ipp_tag_t)(attr->value_tag | quickcopy),
	                       attr->name, attr->num_values, NULL, NULL);

        if (quickcopy)
	{
          for (i = 0; i < attr->num_values; i ++)
	    toattr->values[i].string.text = attr->values[i].string.text;
        }
	else
	{
          for (i = 0; i < attr->num_values; i ++)
	    toattr->values[i].string.text =
	        _cupsStrAlloc(attr->values[i].string.text);
	}
        break;

    case IPP_TAG_DATE :
        toattr = ippAddDate(to, group_tag, attr->name,
	                    attr->values[0].date);
        break;

    case IPP_TAG_RESOLUTION :
        toattr = ippAddResolutions(to, group_tag, attr->name,
	                           attr->num_values, IPP_RES_PER_INCH,
				   NULL, NULL);

        for (i = 0; i < attr->num_values; i ++)
	{
	  toattr->values[i].resolution.xres  = attr->values[i].resolution.xres;
	  toattr->values[i].resolution.yres  = attr->values[i].resolution.yres;
	  toattr->values[i].resolution.units = attr->values[i].resolution.units;
	}
        break;

    case IPP_TAG_RANGE :
        toattr = ippAddRanges(to, group_tag, attr->name,
	                      attr->num_values, NULL, NULL);

        for (i = 0; i < attr->num_values; i ++)
	{
	  toattr->values[i].range.lower = attr->values[i].range.lower;
	  toattr->values[i].range.upper = attr->values[i].range.upper;
	}
        break;

    case IPP_TAG_TEXTLANG :
    case IPP_TAG_NAMELANG :
        toattr = ippAddStrings(to, group_tag,
	                       (ipp_tag_t)(attr->value_tag | quickcopy),
	                       attr->name, attr->num_values, NULL, NULL);

        if (quickcopy)
	{
          for (i = 0; i < attr->num_values; i ++)
	  {
            toattr->values[i].string.charset = attr->values[i].string.charset;
	    toattr->values[i].string.text    = attr->values[i].string.text;
          }
        }
	else
	{
          for (i = 0; i < attr->num_values; i ++)
	  {
	    if (!i)
              toattr->values[i].string.charset =
	          _cupsStrAlloc(attr->values[i].string.charset);
	    else
              toattr->values[i].string.charset =
	          toattr->values[0].string.charset;

	    toattr->values[i].string.text =
	        _cupsStrAlloc(attr->values[i].string.text);
          }
        }
        break;

    case IPP_TAG_BEGIN_COLLECTION :
        toattr = ippAddCollections(to, group_tag, attr->name,
	                           attr->num_values, NULL);

        for (i = 0; i < attr->num_values; i ++)
	{
	  toattr->values[i].collection = attr->values[i].collection;
	  attr->values[i].collection->use ++;
	}
        break;

    case IPP_TAG_STRING :
        if (quickcopy)
	{
	  toattr = ippAddOctetString(to, group_tag, attr->name, NULL, 0);
	  toattr->value_tag |= quickcopy;
	  toattr->values[0].unknown.data   = attr->values[0].unknown.data;
	  toattr->values[0].unknown.length = attr->values[0].unknown.length;
	}
	else
	  toattr = ippAddOctetString(to, attr->group_tag, attr->name,
				     attr->values[0].unknown.data,
				     attr->values[0].unknown.length);
        break;

    default :
        toattr = ippAddIntegers(to, group_tag, attr->value_tag,
	                        attr->name, attr->num_values, NULL);

        for (i = 0; i < attr->num_values; i ++)
	{
	  toattr->values[i].unknown.length = attr->values[i].unknown.length;

	  if (toattr->values[i].unknown.length > 0)
	  {
	    if ((toattr->values[i].unknown.data =
	             malloc(toattr->values[i].unknown.length)) == NULL)
	      toattr->values[i].unknown.length = 0;
	    else
	      memcpy(toattr->values[i].unknown.data,
		     attr->values[i].unknown.data,
		     toattr->values[i].unknown.length);
	  }
	}
        break; /* anti-compiler-warning-code */
  }

  return (toattr);
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

  for (fromattr = from->attrs; fromattr; fromattr = fromattr->next)
  {
   /*
    * Filter attributes as needed...
    */

    if ((group_tag != IPP_TAG_ZERO && fromattr->group_tag != group_tag &&
         fromattr->group_tag != IPP_TAG_ZERO) || !fromattr->name)
      continue;

    if (!ra || cupsArrayFind(ra, fromattr->name))
      copy_attribute(to, fromattr, fromattr->group_tag, quickcopy);
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
  copy_attributes(client->response, job->attrs, ra, IPP_TAG_ZERO, 0);

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
      case IPP_JOB_PENDING :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_TAG_KEYWORD | IPP_TAG_COPY, "job-state-reasons",
		       NULL, "none");
	  break;

      case IPP_JOB_HELD :
          if (job->fd >= 0)
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_TAG_KEYWORD | IPP_TAG_COPY, "job-state-reasons",
			 NULL, "job-incoming");
	  else if (ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_ZERO))
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_TAG_KEYWORD | IPP_TAG_COPY, "job-state-reasons",
			 NULL, "job-hold-until-specified");
          else
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_TAG_KEYWORD | IPP_TAG_COPY, "job-state-reasons",
			 NULL, "job-data-insufficient");
	  break;

      case IPP_JOB_PROCESSING :
	  if (job->cancel)
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_TAG_KEYWORD | IPP_TAG_COPY, "job-state-reasons",
			 NULL, "processing-to-stop-point");
	  else
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_TAG_KEYWORD | IPP_TAG_COPY, "job-state-reasons",
			 NULL, "job-printing");
	  break;

      case IPP_JOB_STOPPED :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_TAG_KEYWORD | IPP_TAG_COPY, "job-state-reasons",
		       NULL, "job-stopped");
	  break;

      case IPP_JOB_CANCELED :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_TAG_KEYWORD | IPP_TAG_COPY, "job-state-reasons",
		       NULL, "job-canceled-by-user");
	  break;

      case IPP_JOB_ABORTED :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_TAG_KEYWORD | IPP_TAG_COPY, "job-state-reasons",
		       NULL, "aborted-by-system");
	  break;

      case IPP_JOB_COMPLETED :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_TAG_KEYWORD | IPP_TAG_COPY, "job-state-reasons",
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
  int		val;			/* Parameter value */
  socklen_t	addrlen;		/* Length of address */


  if ((client = calloc(1, sizeof(_ipp_client_t))) == NULL)
  {
    perror("Unable to allocate memory for client");
    return (NULL);
  }

  client->printer         = printer;
  client->http.activity   = time(NULL);
  client->http.hostaddr   = &(client->addr);
  client->http.blocking   = 1;
  client->http.wait_value = 60000;

 /*
  * Accept the client and get the remote address...
  */

  addrlen = sizeof(http_addr_t);

  if ((client->http.fd = accept(sock, (struct sockaddr *)&(client->addr),
                                &addrlen)) < 0)
  {
    perror("Unable to accept client connection");

    free(client);

    return (NULL);
  }

  httpAddrString(&(client->addr), client->http.hostname,
		 sizeof(client->http.hostname));

  if (Verbosity)
    fprintf(stderr, "Accepted connection from %s (%s)\n", client->http.hostname,
	    client->http.hostaddr->addr.sa_family == AF_INET ? "IPv4" : "IPv6");

 /*
  * Using TCP_NODELAY improves responsiveness, especially on systems
  * with a slow loopback interface.  Since we write large buffers
  * when sending print files and requests, there shouldn't be any
  * performance penalty for this...
  */

  val = 1;
  setsockopt(client->http.fd, IPPROTO_TCP, TCP_NODELAY, (char *)&val,
             sizeof(val));

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
      client->printer->active_job->state < IPP_JOB_CANCELED)
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
  job->state      = IPP_JOB_HELD;
  job->fd         = -1;
  client->request = NULL;

 /*
  * Set all but the first two attributes to the job attributes group...
  */

  for (attr = job->attrs->attrs->next->next; attr; attr = attr->next)
    attr->group_tag = IPP_TAG_JOB;

 /*
  * Get the requesting-user-name, document format, and priority...
  */

  if ((attr = ippFindAttribute(job->attrs, "requesting-user-name",
                               IPP_TAG_NAME)) != NULL)
  {
    _cupsStrFree(attr->name);
    attr->name = _cupsStrAlloc("job-originating-user-name");
  }
  else
    attr = ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME | IPP_TAG_COPY,
                        "job-originating-user-name", NULL, "anonymous");

  if (attr)
    job->username = attr->values[0].string.text;
  else
    job->username = "anonymous";

  if ((attr = ippFindAttribute(job->attrs, "document-format",
                               IPP_TAG_MIMETYPE)) != NULL)
    job->format = attr->values[0].string.text;
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
  int		sock,			/* Listener socket */
		val;			/* Socket value */
  http_addr_t	address;		/* Listen address */
  socklen_t	addrlen;		/* Length of listen address */


  if ((sock = socket(family, SOCK_STREAM, 0)) < 0)
    return (-1);

  val = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

#ifdef IPV6_V6ONLY
  if (family == AF_INET6)
    setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val));
#endif /* IPV6_V6ONLY */

  if (!*port)
  {
   /*
    * Get the auto-assigned port number for the IPv4 socket...
    */

    /* TODO: This code does not appear to work - port is always 0... */
    addrlen = sizeof(address);
    if (getsockname(sock, (struct sockaddr *)&address, &addrlen))
    {
      perror("getsockname() failed");
      *port = 8631;
    }
    else
      *port = _httpAddrPort(&address);

    fprintf(stderr, "Listening on port %d.\n", *port);
  }

  memset(&address, 0, sizeof(address));
  address.addr.sa_family = family;
  _httpAddrSetPort(&address, *port);

  if (bind(sock, (struct sockaddr *)&address, httpAddrLength(&address)))
  {
    close(sock);
    return (-1);
  }

  if (listen(sock, 5))
  {
    close(sock);
    return (-1);
  }

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
	*media_size = ippNew();		/* media-size value */
  char	media_key[256];			/* media-key value */


  ippAddInteger(media_size, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "x-dimension",
                width);
  ippAddInteger(media_size, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "y-dimension",
                length);

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
#ifdef HAVE_DNSSD
	       const char *regtype,	/* I - Bonjour service type */
#endif /* HAVE_DNSSD */
	       const char *directory)	/* I - Spool directory */
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
  ipp_attribute_t	*media_col_database;
					/* media-col-database value */
  ipp_t			*media_col_default;
					/* media-col-default value */
  ipp_value_t		*media_col_value;
					/* Current media-col-database value */
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
    IPP_PORTRAIT,
    IPP_LANDSCAPE,
    IPP_REVERSE_LANDSCAPE,
    IPP_REVERSE_PORTRAIT
  };
  static const char * const versions[] =/* ipp-versions-supported values */
  {
    "1.0",
    "1.1",
    "2.0"
  };
  static const int	ops[] =		/* operations-supported values */
  {
    IPP_PRINT_JOB,
    IPP_PRINT_URI,
    IPP_VALIDATE_JOB,
    IPP_CREATE_JOB,
    IPP_SEND_DOCUMENT,
    IPP_SEND_URI,
    IPP_CANCEL_JOB,
    IPP_GET_JOB_ATTRIBUTES,
    IPP_GET_JOBS,
    IPP_GET_PRINTER_ATTRIBUTES
  };
  static const char * const charsets[] =/* charset-supported values */
  {
    "us-ascii",
    "utf-8"
  };
  static const char * const job_creation[] =
  {					/* job-creation-attributes-supported values */
    "copies",
    "ipp-attribute-fidelity",
    "job-name",
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
  static const char * const referenced_uri_scheme_supported[] =
  {					/* referenced-uri-scheme-supported */
    "file",
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
  printer->name          = _cupsStrAlloc(name);
#ifdef HAVE_DNSSD
  printer->dnssd_name    = _cupsStrRetain(printer->name);
#endif /* HAVE_DNSSD */
  printer->directory     = _cupsStrAlloc(directory);
  printer->hostname      = _cupsStrAlloc(servername ? servername :
                                             httpGetHostname(NULL, hostname,
                                                             sizeof(hostname)));
  printer->port          = port;
  printer->state         = IPP_PRINTER_IDLE;
  printer->state_reasons = _IPP_PRINTER_NONE;
  printer->jobs          = cupsArrayNew((cups_array_func_t)compare_jobs, NULL);
  printer->next_job_id   = 1;

  httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
		  printer->hostname, printer->port, "/ipp");
  printer->uri    = _cupsStrAlloc(uri);
  printer->urilen = strlen(uri);

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
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_CHARSET | IPP_TAG_COPY,
               "charset-configured", NULL, "utf-8");

  /* charset-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_CHARSET | IPP_TAG_COPY,
                "charset-supported", sizeof(charsets) / sizeof(charsets[0]),
		NULL, charsets);

  /* color-supported */
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "color-supported",
                ppm_color > 0);

  /* compression-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD | IPP_TAG_COPY,
	       "compression-supported", NULL, "none");

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
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_LANGUAGE | IPP_TAG_COPY,
               "generated-natural-language-supported", NULL, "en");

  /* ipp-versions-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD | IPP_TAG_COPY,
                "ipp-versions-supported",
		sizeof(versions) / sizeof(versions[0]), NULL, versions);

  /* job-creation-attributes-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD | IPP_TAG_COPY,
                "job-creation-attributes-supported",
		sizeof(job_creation) / sizeof(job_creation[0]),
		NULL, job_creation);

  /* job-k-octets-supported */
  ippAddRange(printer->attrs, IPP_TAG_PRINTER, "job-k-octets-supported", 0,
	      k_supported);

  /* job-priority-default */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "job-priority-default", 50);

  /* job-priority-supported */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "job-priority-supported", 100);

  /* job-sheets-default */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME | IPP_TAG_COPY,
               "job-sheets-default", NULL, "none");

  /* job-sheets-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME | IPP_TAG_COPY,
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
  for (media_col_value = media_col_database->values, i = 0;
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

      media_col_value->collection =
          create_media_col(media_supported[i], media_type_supported[j],
	                   media_col_sizes[i][0], media_col_sizes[i][1],
			   media_xxx_margin_supported[1]);
      media_col_value ++;

      if (media_col_sizes[i][2] != _IPP_ENV_ONLY &&
	  (!strcmp(media_type_supported[j], "auto") ||
	   !strncmp(media_type_supported[j], "photographic-", 13)))
      {
       /*
        * Add borderless version for this combination...
	*/

	media_col_value->collection =
	    create_media_col(media_supported[i], media_type_supported[j],
			     media_col_sizes[i][0], media_col_sizes[i][1],
			     media_xxx_margin_supported[0]);
	media_col_value ++;
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
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD | IPP_TAG_COPY,
                "media-col-supported",
		(int)(sizeof(media_col_supported) /
		      sizeof(media_col_supported[0])), NULL,
		media_col_supported);

  /* media-default */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD | IPP_TAG_COPY,
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
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD | IPP_TAG_COPY,
                "media-supported",
		(int)(sizeof(media_supported) / sizeof(media_supported[0])),
		NULL, media_supported);

  /* media-top-margin-supported */
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                 "media-top-margin-supported",
		 (int)(sizeof(media_xxx_margin_supported) /
		       sizeof(media_xxx_margin_supported[0])),
		 media_xxx_margin_supported);

  /* multiple-document-handling-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD | IPP_TAG_COPY,
                "multiple-document-handling-supported",
                sizeof(multiple_document_handling) /
		    sizeof(multiple_document_handling[0]), NULL,
	        multiple_document_handling);

  /* multiple-document-jobs-supported */
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER,
                "multiple-document-jobs-supported", 0);

  /* natural-language-configured */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_LANGUAGE | IPP_TAG_COPY,
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
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD | IPP_TAG_COPY,
               "output-bin-default", NULL, "face-down");

  /* output-bin-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD | IPP_TAG_COPY,
               "output-bin-supported", NULL, "face-down");

  /* pages-per-minute */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "pages-per-minute", ppm);

  /* pages-per-minute-color */
  if (ppm_color > 0)
    ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                  "pages-per-minute-color", ppm_color);

  /* pdl-override-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD | IPP_TAG_COPY,
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

  /* referenced-uri-scheme-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER,
                IPP_TAG_URISCHEME | IPP_TAG_COPY,
                "referenced-uri-scheme-supported",
                (int)(sizeof(referenced_uri_scheme_supported) /
                      sizeof(referenced_uri_scheme_supported[0])),
                NULL, referenced_uri_scheme_supported);

  /* sides-default */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD | IPP_TAG_COPY,
               "sides-default", NULL, "one-sided");

  /* sides-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD | IPP_TAG_COPY,
                "sides-supported", duplex ? 3 : 1, NULL, sides_supported);

  /* uri-authentication-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD | IPP_TAG_COPY,
               "uri-authentication-supported", NULL, "none");

  /* uri-security-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD | IPP_TAG_COPY,
               "uri-security-supported", NULL, "none");

  /* which-jobs-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD | IPP_TAG_COPY,
                "which-jobs-supported",
                sizeof(which_jobs) / sizeof(which_jobs[0]), NULL, which_jobs);

  free(formats[0]);

  debug_attributes("Printer", printer->attrs, 0);

#ifdef HAVE_DNSSD
 /*
  * Register the printer with Bonjour...
  */

  if (!register_printer(printer, location, make, model, docformats, adminurl,
                        ppm_color > 0, duplex, regtype))
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
 * 'create_requested_array()' - Create an array for requested-attributes.
 */

static cups_array_t *			/* O - requested-attributes array */
create_requested_array(
    _ipp_client_t *client)		/* I - Client */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*requested;	/* requested-attributes attribute */
  cups_array_t		*ra;		/* Requested attributes array */
  char			*value;		/* Current value */


 /*
  * Get the requested-attributes attribute, and return NULL if we don't
  * have one...
  */

  if ((requested = ippFindAttribute(client->request, "requested-attributes",
                                    IPP_TAG_KEYWORD)) == NULL)
    return (NULL);

 /*
  * If the attribute contains a single "all" keyword, return NULL...
  */

  if (requested->num_values == 1 &&
      !strcmp(requested->values[0].string.text, "all"))
    return (NULL);

 /*
  * Create an array using "strcmp" as the comparison function...
  */

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);

  for (i = 0; i < requested->num_values; i ++)
  {
    value = requested->values[i].string.text;

    if (!strcmp(value, "job-template"))
    {
      cupsArrayAdd(ra, "copies");
      cupsArrayAdd(ra, "copies-default");
      cupsArrayAdd(ra, "copies-supported");
      cupsArrayAdd(ra, "finishings");
      cupsArrayAdd(ra, "finishings-default");
      cupsArrayAdd(ra, "finishings-supported");
      cupsArrayAdd(ra, "job-hold-until");
      cupsArrayAdd(ra, "job-hold-until-default");
      cupsArrayAdd(ra, "job-hold-until-supported");
      cupsArrayAdd(ra, "job-priority");
      cupsArrayAdd(ra, "job-priority-default");
      cupsArrayAdd(ra, "job-priority-supported");
      cupsArrayAdd(ra, "job-sheets");
      cupsArrayAdd(ra, "job-sheets-default");
      cupsArrayAdd(ra, "job-sheets-supported");
      cupsArrayAdd(ra, "media");
      cupsArrayAdd(ra, "media-col");
      cupsArrayAdd(ra, "media-col-default");
      cupsArrayAdd(ra, "media-col-supported");
      cupsArrayAdd(ra, "media-default");
      cupsArrayAdd(ra, "media-source-supported");
      cupsArrayAdd(ra, "media-supported");
      cupsArrayAdd(ra, "media-type-supported");
      cupsArrayAdd(ra, "multiple-document-handling");
      cupsArrayAdd(ra, "multiple-document-handling-default");
      cupsArrayAdd(ra, "multiple-document-handling-supported");
      cupsArrayAdd(ra, "number-up");
      cupsArrayAdd(ra, "number-up-default");
      cupsArrayAdd(ra, "number-up-supported");
      cupsArrayAdd(ra, "orientation-requested");
      cupsArrayAdd(ra, "orientation-requested-default");
      cupsArrayAdd(ra, "orientation-requested-supported");
      cupsArrayAdd(ra, "page-ranges");
      cupsArrayAdd(ra, "page-ranges-supported");
      cupsArrayAdd(ra, "printer-resolution");
      cupsArrayAdd(ra, "printer-resolution-default");
      cupsArrayAdd(ra, "printer-resolution-supported");
      cupsArrayAdd(ra, "print-quality");
      cupsArrayAdd(ra, "print-quality-default");
      cupsArrayAdd(ra, "print-quality-supported");
      cupsArrayAdd(ra, "sides");
      cupsArrayAdd(ra, "sides-default");
      cupsArrayAdd(ra, "sides-supported");
    }
    else if (!strcmp(value, "job-description"))
    {
      cupsArrayAdd(ra, "date-time-at-completed");
      cupsArrayAdd(ra, "date-time-at-creation");
      cupsArrayAdd(ra, "date-time-at-processing");
      cupsArrayAdd(ra, "job-detailed-status-message");
      cupsArrayAdd(ra, "job-document-access-errors");
      cupsArrayAdd(ra, "job-id");
      cupsArrayAdd(ra, "job-impressions");
      cupsArrayAdd(ra, "job-impressions-completed");
      cupsArrayAdd(ra, "job-k-octets");
      cupsArrayAdd(ra, "job-k-octets-processed");
      cupsArrayAdd(ra, "job-media-sheets");
      cupsArrayAdd(ra, "job-media-sheets-completed");
      cupsArrayAdd(ra, "job-message-from-operator");
      cupsArrayAdd(ra, "job-more-info");
      cupsArrayAdd(ra, "job-name");
      cupsArrayAdd(ra, "job-originating-user-name");
      cupsArrayAdd(ra, "job-printer-up-time");
      cupsArrayAdd(ra, "job-printer-uri");
      cupsArrayAdd(ra, "job-state");
      cupsArrayAdd(ra, "job-state-message");
      cupsArrayAdd(ra, "job-state-reasons");
      cupsArrayAdd(ra, "job-uri");
      cupsArrayAdd(ra, "number-of-documents");
      cupsArrayAdd(ra, "number-of-intervening-jobs");
      cupsArrayAdd(ra, "output-device-assigned");
      cupsArrayAdd(ra, "time-at-completed");
      cupsArrayAdd(ra, "time-at-creation");
      cupsArrayAdd(ra, "time-at-processing");
    }
    else if (!strcmp(value, "printer-description"))
    {
      cupsArrayAdd(ra, "charset-configured");
      cupsArrayAdd(ra, "charset-supported");
      cupsArrayAdd(ra, "color-supported");
      cupsArrayAdd(ra, "compression-supported");
      cupsArrayAdd(ra, "document-format-default");
      cupsArrayAdd(ra, "document-format-supported");
      cupsArrayAdd(ra, "generated-natural-language-supported");
      cupsArrayAdd(ra, "ipp-versions-supported");
      cupsArrayAdd(ra, "job-impressions-supported");
      cupsArrayAdd(ra, "job-k-octets-supported");
      cupsArrayAdd(ra, "job-media-sheets-supported");
      cupsArrayAdd(ra, "multiple-document-jobs-supported");
      cupsArrayAdd(ra, "multiple-operation-time-out");
      cupsArrayAdd(ra, "natural-language-configured");
      cupsArrayAdd(ra, "notify-attributes-supported");
      cupsArrayAdd(ra, "notify-lease-duration-default");
      cupsArrayAdd(ra, "notify-lease-duration-supported");
      cupsArrayAdd(ra, "notify-max-events-supported");
      cupsArrayAdd(ra, "notify-events-default");
      cupsArrayAdd(ra, "notify-events-supported");
      cupsArrayAdd(ra, "notify-pull-method-supported");
      cupsArrayAdd(ra, "notify-schemes-supported");
      cupsArrayAdd(ra, "operations-supported");
      cupsArrayAdd(ra, "pages-per-minute");
      cupsArrayAdd(ra, "pages-per-minute-color");
      cupsArrayAdd(ra, "pdl-override-supported");
      cupsArrayAdd(ra, "printer-alert");
      cupsArrayAdd(ra, "printer-alert-description");
      cupsArrayAdd(ra, "printer-current-time");
      cupsArrayAdd(ra, "printer-driver-installer");
      cupsArrayAdd(ra, "printer-info");
      cupsArrayAdd(ra, "printer-is-accepting-jobs");
      cupsArrayAdd(ra, "printer-location");
      cupsArrayAdd(ra, "printer-make-and-model");
      cupsArrayAdd(ra, "printer-message-from-operator");
      cupsArrayAdd(ra, "printer-more-info");
      cupsArrayAdd(ra, "printer-more-info-manufacturer");
      cupsArrayAdd(ra, "printer-name");
      cupsArrayAdd(ra, "printer-state");
      cupsArrayAdd(ra, "printer-state-message");
      cupsArrayAdd(ra, "printer-state-reasons");
      cupsArrayAdd(ra, "printer-up-time");
      cupsArrayAdd(ra, "printer-uri-supported");
      cupsArrayAdd(ra, "queued-job-count");
      cupsArrayAdd(ra, "reference-uri-schemes-supported");
      cupsArrayAdd(ra, "uri-authentication-supported");
      cupsArrayAdd(ra, "uri-security-supported");
    }
    else if (!strcmp(value, "printer-defaults"))
    {
      cupsArrayAdd(ra, "copies-default");
      cupsArrayAdd(ra, "document-format-default");
      cupsArrayAdd(ra, "finishings-default");
      cupsArrayAdd(ra, "job-hold-until-default");
      cupsArrayAdd(ra, "job-priority-default");
      cupsArrayAdd(ra, "job-sheets-default");
      cupsArrayAdd(ra, "media-default");
      cupsArrayAdd(ra, "media-col-default");
      cupsArrayAdd(ra, "number-up-default");
      cupsArrayAdd(ra, "orientation-requested-default");
      cupsArrayAdd(ra, "sides-default");
    }
    else if (!strcmp(value, "subscription-template"))
    {
      cupsArrayAdd(ra, "notify-attributes");
      cupsArrayAdd(ra, "notify-charset");
      cupsArrayAdd(ra, "notify-events");
      cupsArrayAdd(ra, "notify-lease-duration");
      cupsArrayAdd(ra, "notify-natural-language");
      cupsArrayAdd(ra, "notify-pull-method");
      cupsArrayAdd(ra, "notify-recipient-uri");
      cupsArrayAdd(ra, "notify-time-interval");
      cupsArrayAdd(ra, "notify-user-data");
    }
    else
      cupsArrayAdd(ra, value);
  }

  return (ra);
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


  if (Verbosity <= 1)
    return;

  fprintf(stderr, "%s:\n", title);
  fprintf(stderr, "  version=%d.%d\n", ipp->request.any.version[0],
          ipp->request.any.version[1]);
  if (type == 1)
    fprintf(stderr, "  operation-id=%s(%04x)\n",
            ippOpString(ipp->request.op.operation_id),
            ipp->request.op.operation_id);
  else if (type == 2)
    fprintf(stderr, "  status-code=%s(%04x)\n",
            ippErrorString(ipp->request.status.status_code),
            ipp->request.status.status_code);
  fprintf(stderr, "  request-id=%d\n\n", ipp->request.any.request_id);

  for (attr = ipp->attrs, group_tag = IPP_TAG_ZERO; attr; attr = attr->next)
  {
    if (attr->group_tag != group_tag)
    {
      group_tag = attr->group_tag;
      fprintf(stderr, "  %s\n", ippTagString(group_tag));
    }

    if (attr->name)
    {
      _ippAttrString(attr, buffer, sizeof(buffer));
      fprintf(stderr, "    %s (%s%s) %s\n", attr->name,
	      attr->num_values > 1 ? "1setOf " : "",
	      ippTagString(attr->value_tag), buffer);
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
    fprintf(stderr, "Closing connection from %s (%s)\n", client->http.hostname,
	    client->http.hostaddr->addr.sa_family == AF_INET ? "IPv4" : "IPv6");

 /*
  * Flush pending writes before closing...
  */

  httpFlushWrite(&(client->http));

  if (client->http.fd >= 0)
    close(client->http.fd);

 /*
  * Free memory...
  */

  httpClearCookie(&(client->http));
  httpClearFields(&(client->http));

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

  if (printer->http_ref)
    DNSServiceRefDeallocate(printer->http_ref);

  if (printer->common_ref)
    DNSServiceRefDeallocate(printer->common_ref);

  TXTRecordDeallocate(&(printer->ipp_txt));

  if (printer->dnssd_name)
    _cupsStrFree(printer->dnssd_name);
#endif /* HAVE_DNSSD */

  if (printer->name)
    _cupsStrFree(printer->name);
  if (printer->icon)
    _cupsStrFree(printer->icon);
  if (printer->directory)
    _cupsStrFree(printer->directory);
  if (printer->hostname)
    _cupsStrFree(printer->hostname);
  if (printer->uri)
    _cupsStrFree(printer->uri);

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
    _cupsStrFree(printer->dnssd_name);
    printer->dnssd_name = _cupsStrAlloc(name);
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
    if (!strncmp(attr->values[0].string.text, client->printer->uri,
                 client->printer->urilen) &&
        attr->values[0].string.text[client->printer->urilen] == '/')
      key.id = atoi(attr->values[0].string.text + client->printer->urilen + 1);
  }
  else if ((attr = ippFindAttribute(client->request, "job-id",
                                    IPP_TAG_INTEGER)) != NULL)
    key.id = attr->values[0].integer;

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
        httpWrite2(&(client->http), start, s - start);

      if (*s == '&')
        httpWrite2(&(client->http), "&amp;", 5);
      else
        httpWrite2(&(client->http), "&lt;", 4);

      start = s + 1;
    }

    s ++;
  }

  if (s > start)
    httpWrite2(&(client->http), start, s - start);
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
        httpWrite2(&(client->http), start, format - start);

      tptr    = tformat;
      *tptr++ = *format++;

      if (*format == '%')
      {
        httpWrite2(&(client->http), "%", 1);
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

            httpWrite2(&(client->http), temp, strlen(temp));
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

            httpWrite2(&(client->http), temp, strlen(temp));
	    break;

	case 'p' : /* Pointer value */
	    if ((width + 2) > sizeof(temp))
	      break;

	    sprintf(temp, tformat, va_arg(ap, void *));

            httpWrite2(&(client->http), temp, strlen(temp));
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
    httpWrite2(&(client->http), start, format - start);

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
    respond_ipp(client, IPP_NOT_FOUND, "Job does not exist.");
    return;
  }

 /*
  * See if the job is already completed, canceled, or aborted; if so,
  * we can't cancel...
  */

  switch (job->state)
  {
    case IPP_JOB_CANCELED :
	respond_ipp(client, IPP_NOT_POSSIBLE,
		    "Job #%d is already canceled - can\'t cancel.", job->id);
        break;

    case IPP_JOB_ABORTED :
	respond_ipp(client, IPP_NOT_POSSIBLE,
		    "Job #%d is already aborted - can\'t cancel.", job->id);
        break;

    case IPP_JOB_COMPLETED :
	respond_ipp(client, IPP_NOT_POSSIBLE,
		    "Job #%d is already completed - can\'t cancel.", job->id);
        break;

    default :
       /*
        * Cancel the job...
	*/

	_cupsRWLockWrite(&(client->printer->rwlock));

	if (job->state == IPP_JOB_PROCESSING ||
	    (job->state == IPP_JOB_HELD && job->fd >= 0))
          job->cancel = 1;
	else
	{
	  job->state     = IPP_JOB_CANCELED;
	  job->completed = time(NULL);
	}

	_cupsRWUnlock(&(client->printer->rwlock));

	respond_ipp(client, IPP_OK, NULL);
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
    httpFlush(&(client->http));
    return;
  }

 /*
  * Do we have a file to print?
  */

  if (client->http.state == HTTP_POST_RECV)
  {
    respond_ipp(client, IPP_BAD_REQUEST,
                "Unexpected document data following request.");
    return;
  }

 /*
  * Create the job...
  */

  if ((job = create_job(client)) == NULL)
  {
    respond_ipp(client, IPP_PRINTER_BUSY, "Currently printing another job.");
    return;
  }

 /*
  * Return the job info...
  */

  respond_ipp(client, IPP_OK, NULL);

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
    respond_ipp(client, IPP_NOT_FOUND, "Job not found.");
    return;
  }

  respond_ipp(client, IPP_OK, NULL);

  ra = create_requested_array(client);
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
    fprintf(stderr, "%s Get-Jobs which-jobs=%s", client->http.hostname,
            attr->values[0].string.text);

  if (!attr || !strcmp(attr->values[0].string.text, "not-completed"))
  {
    job_comparison = -1;
    job_state      = IPP_JOB_STOPPED;
  }
  else if (!strcmp(attr->values[0].string.text, "completed"))
  {
    job_comparison = 1;
    job_state      = IPP_JOB_CANCELED;
  }
  else if (!strcmp(attr->values[0].string.text, "aborted"))
  {
    job_comparison = 0;
    job_state      = IPP_JOB_ABORTED;
  }
  else if (!strcmp(attr->values[0].string.text, "all"))
  {
    job_comparison = 1;
    job_state      = IPP_JOB_PENDING;
  }
  else if (!strcmp(attr->values[0].string.text, "canceled"))
  {
    job_comparison = 0;
    job_state      = IPP_JOB_CANCELED;
  }
  else if (!strcmp(attr->values[0].string.text, "pending"))
  {
    job_comparison = 0;
    job_state      = IPP_JOB_PENDING;
  }
  else if (!strcmp(attr->values[0].string.text, "pending-held"))
  {
    job_comparison = 0;
    job_state      = IPP_JOB_HELD;
  }
  else if (!strcmp(attr->values[0].string.text, "processing"))
  {
    job_comparison = 0;
    job_state      = IPP_JOB_PROCESSING;
  }
  else if (!strcmp(attr->values[0].string.text, "processing-stopped"))
  {
    job_comparison = 0;
    job_state      = IPP_JOB_STOPPED;
  }
  else
  {
    respond_ipp(client, IPP_ATTRIBUTES,
                "The which-jobs value \"%s\" is not supported.",
                attr->values[0].string.text);
    ippAddString(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_KEYWORD,
                 "which-jobs", NULL, attr->values[0].string.text);
    return;
  }

 /*
  * See if they want to limit the number of jobs reported...
  */

  if ((attr = ippFindAttribute(client->request, "limit",
                               IPP_TAG_INTEGER)) != NULL)
  {
    limit = attr->values[0].integer;

    fprintf(stderr, "%s Get-Jobs limit=%d", client->http.hostname, limit);
  }
  else
    limit = 0;

  if ((attr = ippFindAttribute(client->request, "first-job-id",
                               IPP_TAG_INTEGER)) != NULL)
  {
    first_job_id = attr->values[0].integer;

    fprintf(stderr, "%s Get-Jobs first-job-id=%d", client->http.hostname,
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
    fprintf(stderr, "%s Get-Jobs my-jobs=%s\n", client->http.hostname,
            attr->values[0].boolean ? "true" : "false");

    if (attr->values[0].boolean)
    {
      if ((attr = ippFindAttribute(client->request, "requesting-user-name",
					IPP_TAG_NAME)) == NULL)
      {
	respond_ipp(client, IPP_BAD_REQUEST,
	            "Need requesting-user-name with my-jobs.");
	return;
      }

      username = attr->values[0].string.text;

      fprintf(stderr, "%s Get-Jobs requesting-user-name=\"%s\"\n",
              client->http.hostname, username);
    }
  }

 /*
  * OK, build a list of jobs for this printer...
  */

  if ((ra = create_requested_array(client)) == NULL &&
      !ippFindAttribute(client->request, "requested-attributes",
                        IPP_TAG_KEYWORD))
  {
   /*
    * IPP conformance - Get-Jobs has a default requested-attributes value of
    * "job-id" and "job-uri".
    */

    ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
    cupsArrayAdd(ra, "job-id");
    cupsArrayAdd(ra, "job-uri");
  }

  respond_ipp(client, IPP_OK, NULL);

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
	(username && job->username && _cups_strcasecmp(username, job->username)))
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

  ra      = create_requested_array(client);
  printer = client->printer;

  respond_ipp(client, IPP_OK, NULL);

  _cupsRWLockRead(&(printer->rwlock));

  copy_attributes(client->response, printer->attrs, ra, IPP_TAG_ZERO,
		  IPP_TAG_COPY);

  if (!ra || cupsArrayFind(ra, "printer-state"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                  "printer-state", printer->state);

  if (!ra || cupsArrayFind(ra, "printer-state-reasons"))
  {
    if (printer->state_reasons == _IPP_PRINTER_NONE)
      ippAddString(client->response, IPP_TAG_PRINTER,
                   IPP_TAG_KEYWORD | IPP_TAG_COPY, "printer-state-reasons",
		   NULL, "none");
    else
    {
      int			num_reasons = 0;/* Number of reasons */
      const char		*reasons[32];	/* Reason strings */

      if (printer->state_reasons & _IPP_PRINTER_OTHER)
	reasons[num_reasons ++] = "other";
      if (printer->state_reasons & _IPP_PRINTER_COVER_OPEN)
	reasons[num_reasons ++] = "cover-open";
      if (printer->state_reasons & _IPP_PRINTER_INPUT_TRAY_MISSING)
	reasons[num_reasons ++] = "input-tray-missing";
      if (printer->state_reasons & _IPP_PRINTER_MARKER_SUPPLY_EMPTY)
	reasons[num_reasons ++] = "marker-supply-empty-warning";
      if (printer->state_reasons & _IPP_PRINTER_MARKER_SUPPLY_LOW)
	reasons[num_reasons ++] = "marker-supply-low-report";
      if (printer->state_reasons & _IPP_PRINTER_MARKER_WASTE_ALMOST_FULL)
	reasons[num_reasons ++] = "marker-waste-almost-full-report";
      if (printer->state_reasons & _IPP_PRINTER_MARKER_WASTE_FULL)
	reasons[num_reasons ++] = "marker-waste-full-warning";
      if (printer->state_reasons & _IPP_PRINTER_MEDIA_EMPTY)
	reasons[num_reasons ++] = "media-empty-warning";
      if (printer->state_reasons & _IPP_PRINTER_MEDIA_JAM)
	reasons[num_reasons ++] = "media-jam-warning";
      if (printer->state_reasons & _IPP_PRINTER_MEDIA_LOW)
	reasons[num_reasons ++] = "media-low-report";
      if (printer->state_reasons & _IPP_PRINTER_MEDIA_NEEDED)
	reasons[num_reasons ++] = "media-needed-report";
      if (printer->state_reasons & _IPP_PRINTER_MOVING_TO_PAUSED)
	reasons[num_reasons ++] = "moving-to-paused";
      if (printer->state_reasons & _IPP_PRINTER_PAUSED)
	reasons[num_reasons ++] = "paused";
      if (printer->state_reasons & _IPP_PRINTER_SPOOL_AREA_FULL)
	reasons[num_reasons ++] = "spool-area-full";
      if (printer->state_reasons & _IPP_PRINTER_TONER_EMPTY)
	reasons[num_reasons ++] = "toner-empty-warning";
      if (printer->state_reasons & _IPP_PRINTER_TONER_LOW)
	reasons[num_reasons ++] = "toner-low-report";

      ippAddStrings(client->response, IPP_TAG_PRINTER,
                    IPP_TAG_KEYWORD | IPP_TAG_COPY,  "printer-state-reasons",
		    num_reasons, NULL, reasons);
    }
  }

  if (!ra || cupsArrayFind(ra, "printer-up-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                  "printer-up-time", (int)time(NULL));

  if (!ra || cupsArrayFind(ra, "queued-job-count"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                  "queued-job-count",
		  printer->active_job &&
		      printer->active_job->state < IPP_JOB_CANCELED);

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
    httpFlush(&(client->http));
    return;
  }

 /*
  * Do we have a file to print?
  */

  if (client->http.state == HTTP_POST_SEND)
  {
    respond_ipp(client, IPP_BAD_REQUEST, "No file in request.");
    return;
  }

 /*
  * Print the job...
  */

  if ((job = create_job(client)) == NULL)
  {
    respond_ipp(client, IPP_PRINTER_BUSY, "Currently printing another job.");
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
    job->state = IPP_JOB_ABORTED;

    respond_ipp(client, IPP_INTERNAL_ERROR,
                "Unable to create print file: %s", strerror(errno));
    return;
  }

  while ((bytes = httpRead2(&(client->http), buffer, sizeof(buffer))) > 0)
  {
    if (write(job->fd, buffer, bytes) < bytes)
    {
      int error = errno;		/* Write error */

      job->state = IPP_JOB_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);

      respond_ipp(client, IPP_INTERNAL_ERROR,
                  "Unable to write print file: %s", strerror(error));
      return;
    }
  }

  if (bytes < 0)
  {
   /*
    * Got an error while reading the print data, so abort this job.
    */

    job->state = IPP_JOB_ABORTED;

    close(job->fd);
    job->fd = -1;

    unlink(filename);

    respond_ipp(client, IPP_INTERNAL_ERROR, "Unable to read print file.");
    return;
  }

  if (close(job->fd))
  {
    int error = errno;		/* Write error */

    job->state = IPP_JOB_ABORTED;
    job->fd    = -1;

    unlink(filename);

    respond_ipp(client, IPP_INTERNAL_ERROR, "Unable to write print file: %s",
                strerror(error));
    return;
  }

  job->fd       = -1;
  job->filename = strdup(filename);
  job->state    = IPP_JOB_PENDING;

 /*
  * Process the job...
  */

#if 0
  if (!_cupsThreadCreate((_cups_thread_func_t)process_job, job))
  {
    job->state = IPP_JOB_ABORTED;
    respond_ipp(client, IPP_INTERNAL_ERROR, "Unable to process job.");
    return;
  }

#else
  process_job(job);
#endif /* 0 */

 /*
  * Return the job info...
  */

  respond_ipp(client, IPP_OK, NULL);

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
    httpFlush(&(client->http));
    return;
  }

 /*
  * Do we have a file to print?
  */

  if (client->http.state == HTTP_POST_RECV)
  {
    respond_ipp(client, IPP_BAD_REQUEST,
                "Unexpected document data following request.");
    return;
  }

 /*
  * Do we have a document URI?
  */

  if ((uri = ippFindAttribute(client->request, "document-uri",
                              IPP_TAG_URI)) == NULL)
  {
    respond_ipp(client, IPP_BAD_REQUEST, "Missing document-uri.");
    return;
  }

  if (uri->num_values != 1)
  {
    respond_ipp(client, IPP_BAD_REQUEST, "Too many document-uri values.");
    return;
  }

  uri_status = httpSeparateURI(HTTP_URI_CODING_ALL, uri->values[0].string.text,
                               scheme, sizeof(scheme), userpass,
                               sizeof(userpass), hostname, sizeof(hostname),
                               &port, resource, sizeof(resource));
  if (uri_status < HTTP_URI_OK)
  {
    respond_ipp(client, IPP_BAD_REQUEST, "Bad document-uri: %s",
                uri_status_strings[uri_status - HTTP_URI_OVERFLOW]);
    return;
  }

  if (strcmp(scheme, "file") &&
#ifdef HAVE_SSL
      strcmp(scheme, "https") &&
#endif /* HAVE_SSL */
      strcmp(scheme, "http"))
  {
    respond_ipp(client, IPP_URI_SCHEME, "URI scheme \"%s\" not supported.",
                scheme);
    return;
  }

  if (!strcmp(scheme, "file") && access(resource, R_OK))
  {
    respond_ipp(client, IPP_DOCUMENT_ACCESS_ERROR, "Unable to access URI: %s",
                strerror(errno));
    return;
  }

 /*
  * Print the job...
  */

  if ((job = create_job(client)) == NULL)
  {
    respond_ipp(client, IPP_PRINTER_BUSY, "Currently printing another job.");
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
    job->state = IPP_JOB_ABORTED;

    respond_ipp(client, IPP_INTERNAL_ERROR,
                "Unable to create print file: %s", strerror(errno));
    return;
  }

  if (!strcmp(scheme, "file"))
  {
    if ((infile = open(resource, O_RDONLY)) < 0)
    {
      respond_ipp(client, IPP_DOCUMENT_ACCESS_ERROR, "Unable to access URI: %s",
		  strerror(errno));
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

	job->state = IPP_JOB_ABORTED;

	close(job->fd);
	job->fd = -1;

	unlink(filename);
	close(infile);

	respond_ipp(client, IPP_INTERNAL_ERROR,
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
      encryption = HTTP_ENCRYPT_ALWAYS;
    else
#endif /* HAVE_SSL */
    encryption = HTTP_ENCRYPT_IF_REQUESTED;

    if ((http = httpConnectEncrypt(hostname, port, encryption)) == NULL)
    {
      respond_ipp(client, IPP_DOCUMENT_ACCESS_ERROR,
                  "Unable to connect to %s: %s", hostname,
		  cupsLastErrorString());
      job->state = IPP_JOB_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);
      return;
    }

    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_ACCEPT_LANGUAGE, "en");
    if (httpGet(http, resource))
    {
      respond_ipp(client, IPP_DOCUMENT_ACCESS_ERROR, "Unable to GET URI: %s",
		  strerror(errno));

      job->state = IPP_JOB_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);
      httpClose(http);
      return;
    }

    while ((status = httpUpdate(http)) == HTTP_CONTINUE);

    if (status != HTTP_OK)
    {
      respond_ipp(client, IPP_DOCUMENT_ACCESS_ERROR, "Unable to GET URI: %s",
		  httpStatus(status));

      job->state = IPP_JOB_ABORTED;

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

	job->state = IPP_JOB_ABORTED;

	close(job->fd);
	job->fd = -1;

	unlink(filename);
	httpClose(http);

	respond_ipp(client, IPP_INTERNAL_ERROR,
		    "Unable to write print file: %s", strerror(error));
	return;
      }
    }

    httpClose(http);
  }

  if (close(job->fd))
  {
    int error = errno;		/* Write error */

    job->state = IPP_JOB_ABORTED;
    job->fd    = -1;

    unlink(filename);

    respond_ipp(client, IPP_INTERNAL_ERROR, "Unable to write print file: %s",
		strerror(error));
    return;
  }

  job->fd       = -1;
  job->filename = strdup(filename);
  job->state    = IPP_JOB_PENDING;

 /*
  * Process the job...
  */

#if 0
  if (!_cupsThreadCreate((_cups_thread_func_t)process_job, job))
  {
    job->state = IPP_JOB_ABORTED;
    respond_ipp(client, IPP_INTERNAL_ERROR, "Unable to process job.");
    return;
  }

#else
  process_job(job);
#endif /* 0 */

 /*
  * Return the job info...
  */

  respond_ipp(client, IPP_OK, NULL);

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
    respond_ipp(client, IPP_NOT_FOUND, "Job does not exist.");
    httpFlush(&(client->http));
    return;
  }

 /*
  * See if we already have a document for this job or the job has already
  * in a non-pending state...
  */

  if (job->state > IPP_JOB_HELD)
  {
    respond_ipp(client, IPP_NOT_POSSIBLE, "Job is not in a pending state.");
    httpFlush(&(client->http));
    return;
  }
  else if (job->filename || job->fd >= 0)
  {
    respond_ipp(client, IPP_MULTIPLE_JOBS_NOT_SUPPORTED,
                "Multiple document jobs are not supported.");
    httpFlush(&(client->http));
    return;
  }

  if ((attr = ippFindAttribute(client->request, "last-document",
                               IPP_TAG_ZERO)) == NULL)
  {
    respond_ipp(client, IPP_BAD_REQUEST,
                "Missing required last-document attribute.");
    httpFlush(&(client->http));
    return;
  }
  else if (attr->value_tag != IPP_TAG_BOOLEAN || attr->num_values != 1 ||
           !attr->values[0].boolean)
  {
    respond_unsupported(client, attr);
    httpFlush(&(client->http));
    return;
  }

 /*
  * Validate document attributes...
  */

  if (!valid_doc_attributes(client))
  {
    httpFlush(&(client->http));
    return;
  }

 /*
  * Get the document format for the job...
  */

  _cupsRWLockWrite(&(client->printer->rwlock));

  if ((attr = ippFindAttribute(job->attrs, "document-format",
                               IPP_TAG_MIMETYPE)) != NULL)
    job->format = attr->values[0].string.text;
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
    job->state = IPP_JOB_ABORTED;

    respond_ipp(client, IPP_INTERNAL_ERROR,
                "Unable to create print file: %s", strerror(errno));
    return;
  }

  while ((bytes = httpRead2(&(client->http), buffer, sizeof(buffer))) > 0)
  {
    if (write(job->fd, buffer, bytes) < bytes)
    {
      int error = errno;		/* Write error */

      job->state = IPP_JOB_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);

      respond_ipp(client, IPP_INTERNAL_ERROR,
                  "Unable to write print file: %s", strerror(error));
      return;
    }
  }

  if (bytes < 0)
  {
   /*
    * Got an error while reading the print data, so abort this job.
    */

    job->state = IPP_JOB_ABORTED;

    close(job->fd);
    job->fd = -1;

    unlink(filename);

    respond_ipp(client, IPP_INTERNAL_ERROR, "Unable to read print file.");
    return;
  }

  if (close(job->fd))
  {
    int error = errno;		/* Write error */

    job->state = IPP_JOB_ABORTED;
    job->fd    = -1;

    unlink(filename);

    respond_ipp(client, IPP_INTERNAL_ERROR, "Unable to write print file: %s",
                strerror(error));
    return;
  }

  _cupsRWLockWrite(&(client->printer->rwlock));

  job->fd       = -1;
  job->filename = strdup(filename);
  job->state    = IPP_JOB_PENDING;

  _cupsRWUnlock(&(client->printer->rwlock));

 /*
  * Process the job...
  */

#if 0
  if (!_cupsThreadCreate((_cups_thread_func_t)process_job, job))
  {
    job->state = IPP_JOB_ABORTED;
    respond_ipp(client, IPP_INTERNAL_ERROR, "Unable to process job.");
    return;
  }

#else
  process_job(job);
#endif /* 0 */

 /*
  * Return the job info...
  */

  respond_ipp(client, IPP_OK, NULL);

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
    respond_ipp(client, IPP_NOT_FOUND, "Job does not exist.");
    httpFlush(&(client->http));
    return;
  }

 /*
  * See if we already have a document for this job or the job has already
  * in a non-pending state...
  */

  if (job->state > IPP_JOB_HELD)
  {
    respond_ipp(client, IPP_NOT_POSSIBLE, "Job is not in a pending state.");
    httpFlush(&(client->http));
    return;
  }
  else if (job->filename || job->fd >= 0)
  {
    respond_ipp(client, IPP_MULTIPLE_JOBS_NOT_SUPPORTED,
                "Multiple document jobs are not supported.");
    httpFlush(&(client->http));
    return;
  }

  if ((attr = ippFindAttribute(client->request, "last-document",
                               IPP_TAG_ZERO)) == NULL)
  {
    respond_ipp(client, IPP_BAD_REQUEST,
                "Missing required last-document attribute.");
    httpFlush(&(client->http));
    return;
  }
  else if (attr->value_tag != IPP_TAG_BOOLEAN || attr->num_values != 1 ||
           !attr->values[0].boolean)
  {
    respond_unsupported(client, attr);
    httpFlush(&(client->http));
    return;
  }

 /*
  * Validate document attributes...
  */

  if (!valid_doc_attributes(client))
  {
    httpFlush(&(client->http));
    return;
  }

 /*
  * Do we have a file to print?
  */

  if (client->http.state == HTTP_POST_RECV)
  {
    respond_ipp(client, IPP_BAD_REQUEST,
                "Unexpected document data following request.");
    return;
  }

 /*
  * Do we have a document URI?
  */

  if ((uri = ippFindAttribute(client->request, "document-uri",
                              IPP_TAG_URI)) == NULL)
  {
    respond_ipp(client, IPP_BAD_REQUEST, "Missing document-uri.");
    return;
  }

  if (uri->num_values != 1)
  {
    respond_ipp(client, IPP_BAD_REQUEST, "Too many document-uri values.");
    return;
  }

  uri_status = httpSeparateURI(HTTP_URI_CODING_ALL, uri->values[0].string.text,
                               scheme, sizeof(scheme), userpass,
                               sizeof(userpass), hostname, sizeof(hostname),
                               &port, resource, sizeof(resource));
  if (uri_status < HTTP_URI_OK)
  {
    respond_ipp(client, IPP_BAD_REQUEST, "Bad document-uri: %s",
                uri_status_strings[uri_status - HTTP_URI_OVERFLOW]);
    return;
  }

  if (strcmp(scheme, "file") &&
#ifdef HAVE_SSL
      strcmp(scheme, "https") &&
#endif /* HAVE_SSL */
      strcmp(scheme, "http"))
  {
    respond_ipp(client, IPP_URI_SCHEME, "URI scheme \"%s\" not supported.",
                scheme);
    return;
  }

  if (!strcmp(scheme, "file") && access(resource, R_OK))
  {
    respond_ipp(client, IPP_DOCUMENT_ACCESS_ERROR, "Unable to access URI: %s",
                strerror(errno));
    return;
  }

 /*
  * Get the document format for the job...
  */

  _cupsRWLockWrite(&(client->printer->rwlock));

  if ((attr = ippFindAttribute(job->attrs, "document-format",
                               IPP_TAG_MIMETYPE)) != NULL)
    job->format = attr->values[0].string.text;
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
    job->state = IPP_JOB_ABORTED;

    respond_ipp(client, IPP_INTERNAL_ERROR,
                "Unable to create print file: %s", strerror(errno));
    return;
  }

  if (!strcmp(scheme, "file"))
  {
    if ((infile = open(resource, O_RDONLY)) < 0)
    {
      respond_ipp(client, IPP_DOCUMENT_ACCESS_ERROR, "Unable to access URI: %s",
		  strerror(errno));
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

	job->state = IPP_JOB_ABORTED;

	close(job->fd);
	job->fd = -1;

	unlink(filename);
	close(infile);

	respond_ipp(client, IPP_INTERNAL_ERROR,
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
      encryption = HTTP_ENCRYPT_ALWAYS;
    else
#endif /* HAVE_SSL */
    encryption = HTTP_ENCRYPT_IF_REQUESTED;

    if ((http = httpConnectEncrypt(hostname, port, encryption)) == NULL)
    {
      respond_ipp(client, IPP_DOCUMENT_ACCESS_ERROR,
                  "Unable to connect to %s: %s", hostname,
		  cupsLastErrorString());
      job->state = IPP_JOB_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);
      return;
    }

    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_ACCEPT_LANGUAGE, "en");
    if (httpGet(http, resource))
    {
      respond_ipp(client, IPP_DOCUMENT_ACCESS_ERROR, "Unable to GET URI: %s",
		  strerror(errno));

      job->state = IPP_JOB_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);
      httpClose(http);
      return;
    }

    while ((status = httpUpdate(http)) == HTTP_CONTINUE);

    if (status != HTTP_OK)
    {
      respond_ipp(client, IPP_DOCUMENT_ACCESS_ERROR, "Unable to GET URI: %s",
		  httpStatus(status));

      job->state = IPP_JOB_ABORTED;

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

	job->state = IPP_JOB_ABORTED;

	close(job->fd);
	job->fd = -1;

	unlink(filename);
	httpClose(http);

	respond_ipp(client, IPP_INTERNAL_ERROR,
		    "Unable to write print file: %s", strerror(error));
	return;
      }
    }

    httpClose(http);
  }

  if (close(job->fd))
  {
    int error = errno;		/* Write error */

    job->state = IPP_JOB_ABORTED;
    job->fd    = -1;

    unlink(filename);

    respond_ipp(client, IPP_INTERNAL_ERROR, "Unable to write print file: %s",
		strerror(error));
    return;
  }

  _cupsRWLockWrite(&(client->printer->rwlock));

  job->fd       = -1;
  job->filename = strdup(filename);
  job->state    = IPP_JOB_PENDING;

  _cupsRWUnlock(&(client->printer->rwlock));

 /*
  * Process the job...
  */

#if 0
  if (!_cupsThreadCreate((_cups_thread_func_t)process_job, job))
  {
    job->state = IPP_JOB_ABORTED;
    respond_ipp(client, IPP_INTERNAL_ERROR, "Unable to process job.");
    return;
  }

#else
  process_job(job);
#endif /* 0 */

 /*
  * Return the job info...
  */

  respond_ipp(client, IPP_OK, NULL);

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
    respond_ipp(client, IPP_OK, NULL);
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

  while (httpWait(&(client->http), 30000))
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
  char			line[4096],	/* Line from client... */
			operation[64],	/* Operation code from socket */
			uri[1024],	/* URI */
			version[64],	/* HTTP version number string */
			*ptr;		/* Pointer into strings */
  int			major, minor;	/* HTTP version numbers */
  http_status_t		status;		/* Transfer status */
  ipp_state_t		state;		/* State of IPP transfer */


 /*
  * Abort if we have an error on the connection...
  */

  if (client->http.error)
    return (0);

 /*
  * Clear state variables...
  */

  httpClearFields(&(client->http));
  ippDelete(client->request);
  ippDelete(client->response);

  client->http.activity       = time(NULL);
  client->http.version        = HTTP_1_1;
  client->http.keep_alive     = HTTP_KEEPALIVE_OFF;
  client->http.data_encoding  = HTTP_ENCODE_LENGTH;
  client->http.data_remaining = 0;
  client->request             = NULL;
  client->response            = NULL;
  client->operation           = HTTP_WAITING;

 /*
  * Read a request from the connection...
  */

  while ((ptr = httpGets(line, sizeof(line) - 1, &(client->http))) != NULL)
    if (*ptr)
      break;

  if (!ptr)
    return (0);

 /*
  * Parse the request line...
  */

  fprintf(stderr, "%s %s\n", client->http.hostname, line);

  switch (sscanf(line, "%63s%1023s%63s", operation, uri, version))
  {
    case 1 :
	fprintf(stderr, "%s Bad request line.\n", client->http.hostname);
	respond_http(client, HTTP_BAD_REQUEST, NULL, 0);
	return (0);

    case 2 :
	client->http.version = HTTP_0_9;
	break;

    case 3 :
	if (sscanf(version, "HTTP/%d.%d", &major, &minor) != 2)
	{
	  fprintf(stderr, "%s Bad HTTP version.\n", client->http.hostname);
	  respond_http(client, HTTP_BAD_REQUEST, NULL, 0);
	  return (0);
	}

	if (major < 2)
	{
	  client->http.version = (http_version_t)(major * 100 + minor);
	  if (client->http.version == HTTP_1_1)
	    client->http.keep_alive = HTTP_KEEPALIVE_ON;
	  else
	    client->http.keep_alive = HTTP_KEEPALIVE_OFF;
	}
	else
	{
	  respond_http(client, HTTP_NOT_SUPPORTED, NULL, 0);
	  return (0);
	}
	break;
  }

 /*
  * Handle full URLs in the request line...
  */

  if (!strncmp(client->uri, "http:", 5) || !strncmp(client->uri, "ipp:", 4))
  {
    char	scheme[32],		/* Method/scheme */
		userpass[128],		/* Username:password */
		hostname[HTTP_MAX_HOST];/* Hostname */
    int		port;			/* Port number */

   /*
    * Separate the URI into its components...
    */

    if (httpSeparateURI(HTTP_URI_CODING_MOST, uri, scheme, sizeof(scheme),
		        userpass, sizeof(userpass),
		        hostname, sizeof(hostname), &port,
		        client->uri, sizeof(client->uri)) < HTTP_URI_OK)
    {
      fprintf(stderr, "%s Bad URI \"%s\".\n", client->http.hostname, uri);
      respond_http(client, HTTP_BAD_REQUEST, NULL, 0);
      return (0);
    }
  }
  else
  {
   /*
    * Decode URI
    */

    if (!_httpDecodeURI(client->uri, uri, sizeof(client->uri)))
    {
      fprintf(stderr, "%s Bad URI \"%s\".\n", client->http.hostname, uri);
      respond_http(client, HTTP_BAD_REQUEST, NULL, 0);
      return (0);
    }
  }

 /*
  * Process the request...
  */

  if (!strcmp(operation, "GET"))
    client->http.state = HTTP_GET;
  else if (!strcmp(operation, "POST"))
    client->http.state = HTTP_POST;
  else if (!strcmp(operation, "OPTIONS"))
    client->http.state = HTTP_OPTIONS;
  else if (!strcmp(operation, "HEAD"))
    client->http.state = HTTP_HEAD;
  else
  {
    fprintf(stderr, "%s Bad operation \"%s\".\n", client->http.hostname,
            operation);
    respond_http(client, HTTP_BAD_REQUEST, NULL, 0);
    return (0);
  }

  client->start       = time(NULL);
  client->operation   = client->http.state;
  client->http.status = HTTP_OK;

 /*
  * Parse incoming parameters until the status changes...
  */

  while ((status = httpUpdate(&(client->http))) == HTTP_CONTINUE);

  if (status != HTTP_OK)
  {
    respond_http(client, HTTP_BAD_REQUEST, NULL, 0);
    return (0);
  }

  if (!client->http.fields[HTTP_FIELD_HOST][0] &&
      client->http.version >= HTTP_1_1)
  {
   /*
    * HTTP/1.1 and higher require the "Host:" field...
    */

    respond_http(client, HTTP_BAD_REQUEST, NULL, 0);
    return (0);
  }

 /*
  * Handle HTTP Upgrade...
  */

  if (!_cups_strcasecmp(client->http.fields[HTTP_FIELD_CONNECTION], "Upgrade"))
  {
    if (!respond_http(client, HTTP_NOT_IMPLEMENTED, NULL, 0))
      return (0);
  }

 /*
  * Handle HTTP Expect...
  */

  if (client->http.expect &&
      (client->operation == HTTP_POST || client->operation == HTTP_PUT))
  {
    if (client->http.expect == HTTP_CONTINUE)
    {
     /*
      * Send 100-continue header...
      */

      if (!respond_http(client, HTTP_CONTINUE, NULL, 0))
	return (0);
    }
    else
    {
     /*
      * Send 417-expectation-failed header...
      */

      if (!respond_http(client, HTTP_EXPECTATION_FAILED, NULL, 0))
	return (0);

      httpPrintf(&(client->http), "Content-Length: 0\r\n");
      httpPrintf(&(client->http), "\r\n");
      httpFlushWrite(&(client->http));
      client->http.data_encoding = HTTP_ENCODE_LENGTH;
    }
  }

 /*
  * Handle new transfers...
  */

  switch (client->operation)
  {
    case HTTP_OPTIONS :
       /*
	* Do HEAD/OPTIONS command...
	*/

	return (respond_http(client, HTTP_OK, NULL, 0));

    case HTTP_HEAD :
        if (!strcmp(client->uri, "/icon.png"))
	  return (respond_http(client, HTTP_OK, "image/png", 0));
	else if (!strcmp(client->uri, "/"))
	  return (respond_http(client, HTTP_OK, "text/html", 0));
	else
	  return (respond_http(client, HTTP_NOT_FOUND, NULL, 0));
	break;

    case HTTP_GET :
        if (!strcmp(client->uri, "/icon.png"))
	{
	 /*
	  * Send PNG icon file.
	  */

          int		fd;		/* Icon file */
	  struct stat	fileinfo;	/* Icon file information */
	  char		buffer[4096];	/* Copy buffer */
	  ssize_t	bytes;		/* Bytes */

          if (!stat(client->printer->icon, &fileinfo) &&
	      (fd = open(client->printer->icon, O_RDONLY)) >= 0)
	  {
	    if (!respond_http(client, HTTP_OK, "image/png", fileinfo.st_size))
	    {
	      close(fd);
	      return (0);
	    }

	    while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
	      httpWrite2(&(client->http), buffer, bytes);

	    httpFlushWrite(&(client->http));

	    close(fd);
	  }
	  else
	    return (respond_http(client, HTTP_NOT_FOUND, NULL, 0));
	}
	else if (!strcmp(client->uri, "/"))
	{
	 /*
	  * Show web status page...
	  */

          if (!respond_http(client, HTTP_OK, "text/html", 0))
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
		      "<h1>%s</h1>\n"
		      "<p>%s, %d job(s).</p>\n"
		      "</body>\n"
		      "</html>\n",
		      client->printer->name, client->printer->name,
		      client->printer->state == IPP_PRINTER_IDLE ? "Idle" :
		          client->printer->state == IPP_PRINTER_PROCESSING ?
			  "Printing" : "Stopped",
		      cupsArrayCount(client->printer->jobs));
          httpWrite2(&(client->http), "", 0);

	  return (1);
	}
	else
	  return (respond_http(client, HTTP_NOT_FOUND, NULL, 0));
	break;

    case HTTP_POST :
	if (client->http.data_remaining < 0 ||
	    (!client->http.fields[HTTP_FIELD_CONTENT_LENGTH][0] &&
	     client->http.data_encoding == HTTP_ENCODE_LENGTH))
	{
	 /*
	  * Negative content lengths are invalid...
	  */

	  return (respond_http(client, HTTP_BAD_REQUEST, NULL, 0));
	}

	if (strcmp(client->http.fields[HTTP_FIELD_CONTENT_TYPE],
	           "application/ipp"))
        {
	 /*
	  * Not an IPP request...
	  */

	  return (respond_http(client, HTTP_BAD_REQUEST, NULL, 0));
	}

       /*
        * Read the IPP request...
	*/

	client->request = ippNew();

        while ((state = ippRead(&(client->http), client->request)) != IPP_DATA)
	  if (state == IPP_ERROR)
	  {
            fprintf(stderr, "%s IPP read error (%s).\n", client->http.hostname,
	            cupsLastErrorString());
	    respond_http(client, HTTP_BAD_REQUEST, NULL, 0);
	    return (0);
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


  debug_attributes("Request", client->request, 1);

 /*
  * First build an empty response message for this request...
  */

  client->operation_id = client->request->request.op.operation_id;
  client->response     = ippNew();

  client->response->request.status.version[0] =
      client->request->request.op.version[0];
  client->response->request.status.version[1] =
      client->request->request.op.version[1];
  client->response->request.status.request_id =
      client->request->request.op.request_id;

 /*
  * Then validate the request header and required attributes...
  */

  if (client->request->request.any.version[0] < 1 ||
      client->request->request.any.version[0] > 2)
  {
   /*
    * Return an error, since we only support IPP 1.x and 2.x.
    */

    respond_ipp(client, IPP_VERSION_NOT_SUPPORTED,
                "Bad request version number %d.%d.",
		client->request->request.any.version[0],
	        client->request->request.any.version[1]);
  }
  else if (client->request->request.any.request_id <= 0)
    respond_ipp(client, IPP_BAD_REQUEST, "Bad request-id %d.",
                client->request->request.any.request_id);
  else if (!client->request->attrs)
    respond_ipp(client, IPP_BAD_REQUEST, "No attributes in request.");
  else
  {
   /*
    * Make sure that the attributes are provided in the correct order and
    * don't repeat groups...
    */

    for (attr = client->request->attrs, group = attr->group_tag;
	 attr;
	 attr = attr->next)
      if (attr->group_tag < group && attr->group_tag != IPP_TAG_ZERO)
      {
       /*
	* Out of order; return an error...
	*/

	respond_ipp(client, IPP_BAD_REQUEST,
		       "Attribute groups are out of order (%x < %x).",
		       attr->group_tag, group);
	break;
      }
      else
	group = attr->group_tag;

    if (!attr)
    {
     /*
      * Then make sure that the first three attributes are:
      *
      *     attributes-charset
      *     attributes-natural-language
      *     printer-uri/job-uri
      */

      attr = client->request->attrs;
      if (attr && attr->name &&
          !strcmp(attr->name, "attributes-charset") &&
	  (attr->value_tag & IPP_TAG_MASK) == IPP_TAG_CHARSET)
	charset = attr;
      else
	charset = NULL;

      if (attr)
        attr = attr->next;

      if (attr && attr->name &&
          !strcmp(attr->name, "attributes-natural-language") &&
	  (attr->value_tag & IPP_TAG_MASK) == IPP_TAG_LANGUAGE)
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

      ippAddString(client->response, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
		   "attributes-charset", NULL,
		   charset ? charset->values[0].string.text : "utf-8");

      ippAddString(client->response, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
		   "attributes-natural-language", NULL,
		   language ? language->values[0].string.text : "en");

      if (charset &&
          _cups_strcasecmp(charset->values[0].string.text, "us-ascii") &&
          _cups_strcasecmp(charset->values[0].string.text, "utf-8"))
      {
       /*
        * Bad character set...
	*/

	respond_ipp(client, IPP_BAD_REQUEST,
	            "Unsupported character set \"%s\".",
	            charset->values[0].string.text);
      }
      else if (!charset || !language || !uri)
      {
       /*
	* Return an error, since attributes-charset,
	* attributes-natural-language, and printer-uri/job-uri are required
	* for all operations.
	*/

	respond_ipp(client, IPP_BAD_REQUEST, "Missing required attributes.");
      }
      else if (strcmp(uri->values[0].string.text, client->printer->uri) &&
               strncmp(uri->values[0].string.text, client->printer->uri,
	               client->printer->urilen))
      {
        respond_ipp(client, IPP_NOT_FOUND, "%s %s not found.", uri->name,
	            uri->values[0].string.text);
      }
      else
      {
       /*
        * Try processing the operation...
	*/

        if (client->http.expect == HTTP_CONTINUE)
	{
	 /*
	  * Send 100-continue header...
	  */

	  if (!respond_http(client, HTTP_CONTINUE, NULL, 0))
	    return (0);
	}

	switch (client->request->request.op.operation_id)
	{
	  case IPP_PRINT_JOB :
              ipp_print_job(client);
              break;

	  case IPP_PRINT_URI :
              ipp_print_uri(client);
              break;

	  case IPP_VALIDATE_JOB :
              ipp_validate_job(client);
              break;

          case IPP_CREATE_JOB :
              ipp_create_job(client);
              break;

          case IPP_SEND_DOCUMENT :
              ipp_send_document(client);
              break;

          case IPP_SEND_URI :
              ipp_send_uri(client);
              break;

	  case IPP_CANCEL_JOB :
              ipp_cancel_job(client);
              break;

	  case IPP_GET_JOB_ATTRIBUTES :
              ipp_get_job_attributes(client);
              break;

	  case IPP_GET_JOBS :
              ipp_get_jobs(client);
              break;

	  case IPP_GET_PRINTER_ATTRIBUTES :
              ipp_get_printer_attributes(client);
              break;

	  default :
	      respond_ipp(client, IPP_OPERATION_NOT_SUPPORTED,
	                  "Operation not supported.");
	      break;
	}
      }
    }
  }

 /*
  * Send the HTTP header and return...
  */

  if (client->http.state != HTTP_POST_SEND)
    httpFlush(&(client->http));		/* Flush trailing (junk) data */

  return (respond_http(client, HTTP_OK, "application/ipp",
                       ippLength(client->response)));
}


/*
 * 'process_job()' - Process a print job.
 */

static void *				/* O - Thread exit status */
process_job(_ipp_job_t *job)		/* I - Job */
{
  job->state          = IPP_JOB_PROCESSING;
  job->printer->state = IPP_PRINTER_PROCESSING;

  sleep(5);

  if (job->cancel)
    job->state = IPP_JOB_CANCELED;
  else
    job->state = IPP_JOB_COMPLETED;

  job->completed           = time(NULL);
  job->printer->state      = IPP_PRINTER_IDLE;
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
    const char     *regtype)		/* I - Service type */
{
  DNSServiceErrorType	error;		/* Error from Bonjour */
  char			make_model[256],/* Make and model together */
			product[256];	/* Product string */


 /*
  * Build the TXT record for IPP...
  */

  snprintf(make_model, sizeof(make_model), "%s %s", make, model);
  snprintf(product, sizeof(product), "(%s)", model);

  TXTRecordCreate(&(printer->ipp_txt), 1024, NULL);
  TXTRecordSetValue(&(printer->ipp_txt), "txtvers", 1, "1");
  TXTRecordSetValue(&(printer->ipp_txt), "qtotal", 1, "1");
  TXTRecordSetValue(&(printer->ipp_txt), "rp", 3, "ipp");
  TXTRecordSetValue(&(printer->ipp_txt), "ty", (uint8_t)strlen(make_model),
                    make_model);
  TXTRecordSetValue(&(printer->ipp_txt), "adminurl", (uint8_t)strlen(adminurl),
                    adminurl);
  TXTRecordSetValue(&(printer->ipp_txt), "note", (uint8_t)strlen(location),
                    location);
  TXTRecordSetValue(&(printer->ipp_txt), "priority", 1, "0");
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
  TXTRecordSetValue(&(printer->ipp_txt), "air", 4, "none");

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
respond_http(_ipp_client_t *client,	/* I - Client */
	     http_status_t code,	/* I - HTTP status of response */
	     const char    *type,	/* I - MIME type of response */
	     size_t        length)	/* I - Length of response */
{
  char	message[1024];			/* Text message */


  fprintf(stderr, "%s %s\n", client->http.hostname, httpStatus(code));

  if (code == HTTP_CONTINUE)
  {
   /*
    * 100-continue doesn't send any headers...
    */

    return (httpPrintf(&(client->http), "HTTP/%d.%d 100 Continue\r\n\r\n",
		       client->http.version / 100,
		       client->http.version % 100) > 0);
  }

 /*
  * Format an error message...
  */

  if (!type && !length && code != HTTP_OK)
  {
    snprintf(message, sizeof(message), "%d - %s\n", code, httpStatus(code));

    type   = "text/plain";
    length = strlen(message);
  }
  else
    message[0] = '\0';

 /*
  * Send the HTTP status header...
  */

  httpFlushWrite(&(client->http));

  client->http.data_encoding = HTTP_ENCODE_FIELDS;

  if (httpPrintf(&(client->http), "HTTP/%d.%d %d %s\r\n", client->http.version / 100,
                 client->http.version % 100, code, httpStatus(code)) < 0)
    return (0);

 /*
  * Follow the header with the response fields...
  */

  if (httpPrintf(&(client->http), "Date: %s\r\n", httpGetDateString(time(NULL))) < 0)
    return (0);

  if (client->http.keep_alive && client->http.version >= HTTP_1_0)
  {
    if (httpPrintf(&(client->http),
                   "Connection: Keep-Alive\r\n"
                   "Keep-Alive: timeout=10\r\n") < 0)
      return (0);
  }

  if (code == HTTP_METHOD_NOT_ALLOWED || client->operation == HTTP_OPTIONS)
  {
    if (httpPrintf(&(client->http), "Allow: GET, HEAD, OPTIONS, POST\r\n") < 0)
      return (0);
  }

  if (type)
  {
    if (!strcmp(type, "text/html"))
    {
      if (httpPrintf(&(client->http),
                     "Content-Type: text/html; charset=utf-8\r\n") < 0)
	return (0);
    }
    else if (httpPrintf(&(client->http), "Content-Type: %s\r\n", type) < 0)
      return (0);
  }

  if (length == 0 && !message[0])
  {
    if (httpPrintf(&(client->http), "Transfer-Encoding: chunked\r\n\r\n") < 0)
      return (0);
  }
  else if (httpPrintf(&(client->http), "Content-Length: " CUPS_LLFMT "\r\n\r\n",
                      CUPS_LLCAST length) < 0)
    return (0);

  if (httpFlushWrite(&(client->http)) < 0)
    return (0);

 /*
  * Send the response data...
  */

  if (message[0])
  {
   /*
    * Send a plain text message.
    */

    if (httpPrintf(&(client->http), "%s", message) < 0)
      return (0);
  }
  else if (client->response)
  {
   /*
    * Send an IPP response...
    */

    debug_attributes("Response", client->response, 2);

    client->http.data_encoding  = HTTP_ENCODE_LENGTH;
    client->http.data_remaining = (off_t)ippLength(client->response);
    client->response->state     = IPP_IDLE;

    if (ippWrite(&(client->http), client->response) != IPP_DATA)
      return (0);
  }
  else
    client->http.data_encoding = HTTP_ENCODE_CHUNKED;

 /*
  * Flush the data and return...
  */

  return (httpFlushWrite(&(client->http)) >= 0);
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
  va_list	ap;			/* Pointer to additional args */
  char		formatted[1024];	/* Formatted errror message */


  client->response->request.status.status_code = status;

  if (!client->response->attrs)
  {
    ippAddString(client->response, IPP_TAG_OPERATION,
                 IPP_TAG_CHARSET | IPP_TAG_COPY, "attributes-charset", NULL,
		 "utf-8");
    ippAddString(client->response, IPP_TAG_OPERATION,
                 IPP_TAG_LANGUAGE | IPP_TAG_COPY, "attributes-natural-language",
		 NULL, "en-us");
  }

  if (message)
  {
    va_start(ap, message);
    vsnprintf(formatted, sizeof(formatted), message, ap);
    va_end(ap);

    ippAddString(client->response, IPP_TAG_OPERATION, IPP_TAG_TEXT,
		 "status-message", NULL, formatted);
  }
  else
    formatted[0] = '\0';

  fprintf(stderr, "%s %s %s (%s)\n", client->http.hostname,
          ippOpString(client->operation_id), ippErrorString(status), formatted);
}


/*
 * 'respond_unsupported()' - Respond with an unsupported attribute.
 */

static void
respond_unsupported(
    _ipp_client_t   *client,		/* I - Client */
    ipp_attribute_t *attr)		/* I - Atribute */
{
  if (!client->response->attrs)
    respond_ipp(client, IPP_ATTRIBUTES, "Unsupported %s %s%s value.",
		attr->name, attr->num_values > 1 ? "1setOf " : "",
		ippTagString(attr->value_tag));

  copy_attribute(client->response, attr, IPP_TAG_UNSUPPORTED_GROUP, 0);
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
    puts(CUPS_SVERSION " - Copyright 2010 by Apple Inc. All rights reserved.");
    puts("");
  }

  puts("Usage: ippserver [options] \"name\"");
  puts("");
  puts("Options:");
  puts("-2                      Supports 2-sided printing (default=1-sided)");
  puts("-M manufacturer         Manufacturer name (default=Test)");
  printf("-d spool-directory      Spool directory "
         "(default=/tmp/ippserver.%d)\n", (int)getpid());
  puts("-f type/subtype[,...]   List of supported types "
       "(default=application/pdf,image/jpeg)");
  puts("-h                      Show program help");
  puts("-i iconfile.png         PNG icon file (default=printer.png)");
  puts("-l location             Location of printer (default=empty string)");
  puts("-m model                Model name (default=Printer)");
  puts("-n hostname             Hostname for printer");
  puts("-p port                 Port number (default=auto)");
  puts("-r regtype              Bonjour service type (default=_ipp._tcp)");
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
  int			i;		/* Looping var */
  ipp_attribute_t	*attr,		/* Current attribute */
			*supported;	/* document-format-supported */
  const char		*format = NULL;	/* document-format value */


 /*
  * Check operation attributes...
  */

  if ((attr = ippFindAttribute(client->request, "compression",
                               IPP_TAG_ZERO)) != NULL)
  {
   /*
    * If compression is specified, only accept "none"...
    */

    if (attr->num_values != 1 || attr->value_tag != IPP_TAG_KEYWORD ||
        strcmp(attr->values[0].string.text, "none"))
      respond_unsupported(client, attr);
    else
      fprintf(stderr, "%s %s compression=\"%s\"\n",
              client->http.hostname,
              ippOpString(client->request->request.op.operation_id),
              attr->values[0].string.text);
  }

 /*
  * Is it a format we support?
  */

  if ((attr = ippFindAttribute(client->request, "document-format",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (attr->num_values != 1 || attr->value_tag != IPP_TAG_MIMETYPE)
      respond_unsupported(client, attr);
    else
    {
      format = attr->values[0].string.text;

      fprintf(stderr, "%s %s document-format=\"%s\"\n",
	      client->http.hostname,
	      ippOpString(client->request->request.op.operation_id), format);
    }
  }
  else
    format = "application/octet-stream";

  if (!strcmp(format, "application/octet-stream") &&
      (client->request->request.op.operation_id == IPP_PRINT_JOB ||
       client->request->request.op.operation_id == IPP_SEND_DOCUMENT))
  {
   /*
    * Auto-type the file using the first 4 bytes of the file...
    */

    unsigned char	header[4];	/* First 4 bytes of file */

    memset(header, 0, sizeof(header));
    _httpPeek(&(client->http), (char *)header, sizeof(header));

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
	      client->http.hostname,
	      ippOpString(client->request->request.op.operation_id), format);

    if (!attr)
      attr = ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE,
                          "document-format", NULL, format);
    else
    {
      _cupsStrFree(attr->values[0].string.text);
      attr->values[0].string.text = _cupsStrAlloc(format);
    }
  }

  if (client->request->request.op.operation_id != IPP_CREATE_JOB &&
      (supported = ippFindAttribute(client->printer->attrs,
                                    "document-format-supported",
			            IPP_TAG_MIMETYPE)) != NULL)
  {
    for (i = 0; i < supported->num_values; i ++)
      if (!_cups_strcasecmp(format, supported->values[i].string.text))
	break;

    if (i >= supported->num_values && attr)
      respond_unsupported(client, attr);
  }

  return (!client->response->attrs ||
          !client->response->attrs->next ||
          !client->response->attrs->next->next);
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
  int			i;		/* Looping var */
  ipp_attribute_t	*attr,		/* Current attribute */
			*supported;	/* xxx-supported attribute */


 /*
  * Check operation attributes...
  */

  valid_doc_attributes(client);

 /*
  * Check the various job template attributes...
  */

  if ((attr = ippFindAttribute(client->request, "copies",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (attr->num_values != 1 || attr->value_tag != IPP_TAG_INTEGER ||
        attr->values[0].integer < 1 || attr->values[0].integer > 999)
    {
      respond_unsupported(client, attr);
    }
  }

  if ((attr = ippFindAttribute(client->request, "ipp-attribute-fidelity",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (attr->num_values != 1 || attr->value_tag != IPP_TAG_BOOLEAN)
    {
      respond_unsupported(client, attr);
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-hold-until",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (attr->num_values != 1 ||
        (attr->value_tag != IPP_TAG_NAME &&
	 attr->value_tag != IPP_TAG_NAMELANG &&
	 attr->value_tag != IPP_TAG_KEYWORD) ||
	strcmp(attr->values[0].string.text, "no-hold"))
    {
      respond_unsupported(client, attr);
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-name",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (attr->num_values != 1 ||
        (attr->value_tag != IPP_TAG_NAME &&
	 attr->value_tag != IPP_TAG_NAMELANG))
    {
      respond_unsupported(client, attr);
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-priority",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (attr->num_values != 1 || attr->value_tag != IPP_TAG_INTEGER ||
        attr->values[0].integer < 1 || attr->values[0].integer > 100)
    {
      respond_unsupported(client, attr);
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-sheets",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (attr->num_values != 1 ||
        (attr->value_tag != IPP_TAG_NAME &&
	 attr->value_tag != IPP_TAG_NAMELANG &&
	 attr->value_tag != IPP_TAG_KEYWORD) ||
	strcmp(attr->values[0].string.text, "none"))
    {
      respond_unsupported(client, attr);
    }
  }

  if ((attr = ippFindAttribute(client->request, "media",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (attr->num_values != 1 ||
        (attr->value_tag != IPP_TAG_NAME &&
	 attr->value_tag != IPP_TAG_NAMELANG &&
	 attr->value_tag != IPP_TAG_KEYWORD))
    {
      respond_unsupported(client, attr);
    }
    else
    {
      for (i = 0;
           i < (int)(sizeof(media_supported) / sizeof(media_supported[0]));
	   i ++)
        if (!strcmp(attr->values[0].string.text, media_supported[i]))
	  break;

      if (i >= (int)(sizeof(media_supported) / sizeof(media_supported[0])))
      {
	respond_unsupported(client, attr);
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "media-col",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (attr->num_values != 1 || attr->value_tag != IPP_TAG_BEGIN_COLLECTION)
    {
      respond_unsupported(client, attr);
    }
    /* TODO: check for valid media-col */
  }

  if ((attr = ippFindAttribute(client->request, "multiple-document-handling",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (attr->num_values != 1 || attr->value_tag != IPP_TAG_KEYWORD ||
        (strcmp(attr->values[0].string.text,
		"separate-documents-uncollated-copies") &&
	 strcmp(attr->values[0].string.text,
		"separate-documents-collated-copies")))
    {
      respond_unsupported(client, attr);
    }
  }

  if ((attr = ippFindAttribute(client->request, "orientation-requested",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (attr->num_values != 1 || attr->value_tag != IPP_TAG_ENUM ||
        attr->values[0].integer < IPP_PORTRAIT ||
        attr->values[0].integer > IPP_REVERSE_PORTRAIT)
    {
      respond_unsupported(client, attr);
    }
  }

  if ((attr = ippFindAttribute(client->request, "page-ranges",
                               IPP_TAG_ZERO)) != NULL)
  {
    respond_unsupported(client, attr);
  }

  if ((attr = ippFindAttribute(client->request, "print-quality",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (attr->num_values != 1 || attr->value_tag != IPP_TAG_ENUM ||
        attr->values[0].integer < IPP_QUALITY_DRAFT ||
        attr->values[0].integer > IPP_QUALITY_HIGH)
    {
      respond_unsupported(client, attr);
    }
  }

  if ((attr = ippFindAttribute(client->request, "printer-resolution",
                               IPP_TAG_ZERO)) != NULL)
  {
    respond_unsupported(client, attr);
  }

  if ((attr = ippFindAttribute(client->request, "sides",
                               IPP_TAG_ZERO)) != NULL)
  {
    if (attr->num_values != 1 || attr->value_tag != IPP_TAG_KEYWORD)
    {
      respond_unsupported(client, attr);
    }

    if ((supported = ippFindAttribute(client->printer->attrs, "sides",
                                      IPP_TAG_KEYWORD)) != NULL)
    {
      for (i = 0; i < supported->num_values; i ++)
        if (!strcmp(attr->values[0].string.text,
	            supported->values[i].string.text))
	  break;

      if (i >= supported->num_values)
      {
	respond_unsupported(client, attr);
      }
    }
    else
    {
      respond_unsupported(client, attr);
    }
  }

  return (!client->response->attrs ||
          !client->response->attrs->next ||
          !client->response->attrs->next->next);
}


/*
 * End of "$Id$".
 */
