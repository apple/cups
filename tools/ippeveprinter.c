/*
 * IPP Everywhere printer application for CUPS.
 *
 * Copyright © 2010-2019 by Apple Inc.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.º
 *
 * Note: This program began life as the "ippserver" sample code that first
 * appeared in CUPS 1.4.  The name has been changed in order to distinguish it
 * from the PWG's much more ambitious "ippserver" program, which supports
 * different kinds of IPP services and multiple services per instance - the
 * "ippeveprinter" program exposes a single print service conforming to the
 * current IPP Everywhere specification, thus the new name.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups-private.h>
#include <cups/debug-private.h>
#if !CUPS_LITE
#  include <cups/ppd-private.h>
#endif /* !CUPS_LITE */

#include <limits.h>
#include <sys/stat.h>

#ifdef _WIN32
#  include <fcntl.h>
#  include <io.h>
#  include <process.h>
#  define WEXITSTATUS(s) (s)
#  include <winsock2.h>
typedef ULONG nfds_t;
#  define poll WSAPoll
#else
extern char **environ;

#  include <sys/fcntl.h>
#  include <sys/wait.h>
#  include <poll.h>
#endif /* _WIN32 */

#ifdef HAVE_DNSSD
#  include <dns_sd.h>
#elif defined(HAVE_AVAHI)
#  include <avahi-client/client.h>
#  include <avahi-client/publish.h>
#  include <avahi-common/error.h>
#  include <avahi-common/thread-watch.h>
#endif /* HAVE_DNSSD */
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

#include "printer-png.h"


/*
 * Constants...
 */

enum ippeve_preason_e			/* printer-state-reasons bit values */
{
  IPPEVE_PREASON_NONE = 0x0000,		/* none */
  IPPEVE_PREASON_OTHER = 0x0001,	/* other */
  IPPEVE_PREASON_COVER_OPEN = 0x0002,	/* cover-open */
  IPPEVE_PREASON_INPUT_TRAY_MISSING = 0x0004,
					/* input-tray-missing */
  IPPEVE_PREASON_MARKER_SUPPLY_EMPTY = 0x0008,
					/* marker-supply-empty */
  IPPEVE_PREASON_MARKER_SUPPLY_LOW = 0x0010,
					/* marker-supply-low */
  IPPEVE_PREASON_MARKER_WASTE_ALMOST_FULL = 0x0020,
					/* marker-waste-almost-full */
  IPPEVE_PREASON_MARKER_WASTE_FULL = 0x0040,
					/* marker-waste-full */
  IPPEVE_PREASON_MEDIA_EMPTY = 0x0080,	/* media-empty */
  IPPEVE_PREASON_MEDIA_JAM = 0x0100,	/* media-jam */
  IPPEVE_PREASON_MEDIA_LOW = 0x0200,	/* media-low */
  IPPEVE_PREASON_MEDIA_NEEDED = 0x0400,	/* media-needed */
  IPPEVE_PREASON_MOVING_TO_PAUSED = 0x0800,
					/* moving-to-paused */
  IPPEVE_PREASON_PAUSED = 0x1000,	/* paused */
  IPPEVE_PREASON_SPOOL_AREA_FULL = 0x2000,/* spool-area-full */
  IPPEVE_PREASON_TONER_EMPTY = 0x4000,	/* toner-empty */
  IPPEVE_PREASON_TONER_LOW = 0x8000	/* toner-low */
};
typedef unsigned int ippeve_preason_t;	/* Bitfield for printer-state-reasons */
static const char * const ippeve_preason_strings[] =
{					/* Strings for each bit */
  /* "none" is implied for no bits set */
  "other",
  "cover-open",
  "input-tray-missing",
  "marker-supply-empty",
  "marker-supply-low",
  "marker-waste-almost-full",
  "marker-waste-full",
  "media-empty",
  "media-jam",
  "media-low",
  "media-needed",
  "moving-to-paused",
  "paused",
  "spool-area-full",
  "toner-empty",
  "toner-low"
};


/*
 * URL scheme for web resources...
 */

#ifdef HAVE_SSL
#  define WEB_SCHEME "https"
#else
#  define WEB_SCHEME "http"
#endif /* HAVE_SSL */


/*
 * Structures...
 */

#ifdef HAVE_DNSSD
typedef DNSServiceRef ippeve_srv_t;	/* Service reference */
typedef TXTRecordRef ippeve_txt_t;	/* TXT record */

#elif defined(HAVE_AVAHI)
typedef AvahiEntryGroup *ippeve_srv_t;	/* Service reference */
typedef AvahiStringList *ippeve_txt_t;	/* TXT record */

#else
typedef void *ippeve_srv_t;		/* Service reference */
typedef void *ippeve_txt_t;		/* TXT record */
#endif /* HAVE_DNSSD */

typedef struct ippeve_filter_s		/**** Attribute filter ****/
{
  cups_array_t		*ra;		/* Requested attributes */
  ipp_tag_t		group_tag;	/* Group to copy */
} ippeve_filter_t;

typedef struct ippeve_job_s ippeve_job_t;

typedef struct ippeve_printer_s		/**** Printer data ****/
{
  /* TODO: One IPv4 and one IPv6 listener are really not sufficient */
  int			ipv4,		/* IPv4 listener */
			ipv6;		/* IPv6 listener */
  ippeve_srv_t		ipp_ref,	/* Bonjour IPP service */
			ipps_ref,	/* Bonjour IPPS service */
			http_ref,	/* Bonjour HTTP service */
			printer_ref;	/* Bonjour LPD service */
  char			*dnssd_name,	/* printer-dnssd-name */
			*name,		/* printer-name */
			*icon,		/* Icon filename */
			*directory,	/* Spool directory */
			*hostname,	/* Hostname */
			*uri,		/* printer-uri-supported */
			*device_uri,	/* Device URI (if any) */
			*output_format,	/* Output format */
#if !CUPS_LITE
			*ppdfile,	/* PPD file (if any) */
#endif /* !CUPS_LITE */
			*command;	/* Command to run with job file */
  int			port;		/* Port */
  int			web_forms;	/* Enable web interface forms? */
  size_t		urilen;		/* Length of printer URI */
  ipp_t			*attrs;		/* Static attributes */
  time_t		start_time;	/* Startup time */
  time_t		config_time;	/* printer-config-change-time */
  ipp_pstate_t		state;		/* printer-state value */
  ippeve_preason_t	state_reasons;	/* printer-state-reasons values */
  time_t		state_time;	/* printer-state-change-time */
  cups_array_t		*jobs;		/* Jobs */
  ippeve_job_t		*active_job;	/* Current active/pending job */
  int			next_job_id;	/* Next job-id value */
  _cups_rwlock_t	rwlock;		/* Printer lock */
} ippeve_printer_t;

struct ippeve_job_s			/**** Job data ****/
{
  int			id;		/* Job ID */
  const char		*name,		/* job-name */
			*username,	/* job-originating-user-name */
			*format;	/* document-format */
  ipp_jstate_t		state;		/* job-state value */
  char			*message;	/* job-state-message value */
  int			msglevel;	/* job-state-message log level (0=error, 1=info) */
  time_t		created,	/* time-at-creation value */
			processing,	/* time-at-processing value */
			completed;	/* time-at-completed value */
  int			impressions,	/* job-impressions value */
			impcompleted;	/* job-impressions-completed value */
  ipp_t			*attrs;		/* Static attributes */
  int			cancel;		/* Non-zero when job canceled */
  char			*filename;	/* Print file name */
  int			fd;		/* Print file descriptor */
  ippeve_printer_t	*printer;	/* Printer */
};

typedef struct ippeve_client_s		/**** Client data ****/
{
  http_t		*http;		/* HTTP connection */
  ipp_t			*request,	/* IPP request */
			*response;	/* IPP response */
  time_t		start;		/* Request start time */
  http_state_t		operation;	/* Request operation */
  ipp_op_t		operation_id;	/* IPP operation-id */
  char			uri[1024],	/* Request URI */
			*options;	/* URI options */
  http_addr_t		addr;		/* Client address */
  char			hostname[256];	/* Client hostname */
  ippeve_printer_t	*printer;	/* Printer */
  ippeve_job_t		*job;		/* Current job, if any */
} ippeve_client_t;


/*
 * Local functions...
 */

static void		clean_jobs(ippeve_printer_t *printer);
static int		compare_jobs(ippeve_job_t *a, ippeve_job_t *b);
static void		copy_attributes(ipp_t *to, ipp_t *from, cups_array_t *ra, ipp_tag_t group_tag, int quickcopy);
static void		copy_job_attributes(ippeve_client_t *client, ippeve_job_t *job, cups_array_t *ra);
static ippeve_client_t	*create_client(ippeve_printer_t *printer, int sock);
static ippeve_job_t	*create_job(ippeve_client_t *client);
static int		create_job_file(ippeve_job_t *job, char *fname, size_t fnamesize, const char *dir, const char *ext);
static int		create_listener(const char *name, int port, int family);
static ipp_t		*create_media_col(const char *media, const char *source, const char *type, int width, int length, int bottom, int left, int right, int top);
static ipp_t		*create_media_size(int width, int length);
static ippeve_printer_t	*create_printer(const char *servername, int serverport, const char *name, const char *location, const char *icon, cups_array_t *docformats, const char *subtypes, const char *directory, const char *command, const char *device_uri, const char *output_format, ipp_t *attrs);
static void		debug_attributes(const char *title, ipp_t *ipp, int response);
static void		delete_client(ippeve_client_t *client);
static void		delete_job(ippeve_job_t *job);
static void		delete_printer(ippeve_printer_t *printer);
#ifdef HAVE_DNSSD
static void DNSSD_API	dnssd_callback(DNSServiceRef sdRef, DNSServiceFlags flags, DNSServiceErrorType errorCode, const char *name, const char *regtype, const char *domain, ippeve_printer_t *printer);
#elif defined(HAVE_AVAHI)
static void		dnssd_callback(AvahiEntryGroup *p, AvahiEntryGroupState state, void *context);
static void		dnssd_client_cb(AvahiClient *c, AvahiClientState state, void *userdata);
#endif /* HAVE_DNSSD */
static void		dnssd_init(void);
static int		filter_cb(ippeve_filter_t *filter, ipp_t *dst, ipp_attribute_t *attr);
static ippeve_job_t	*find_job(ippeve_client_t *client);
static void		finish_document_data(ippeve_client_t *client, ippeve_job_t *job);
static void		finish_document_uri(ippeve_client_t *client, ippeve_job_t *job);
static void		html_escape(ippeve_client_t *client, const char *s, size_t slen);
static void		html_footer(ippeve_client_t *client);
static void		html_header(ippeve_client_t *client, const char *title, int refresh);
static void		html_printf(ippeve_client_t *client, const char *format, ...) _CUPS_FORMAT(2, 3);
static void		ipp_cancel_job(ippeve_client_t *client);
static void		ipp_close_job(ippeve_client_t *client);
static void		ipp_create_job(ippeve_client_t *client);
static void		ipp_get_job_attributes(ippeve_client_t *client);
static void		ipp_get_jobs(ippeve_client_t *client);
static void		ipp_get_printer_attributes(ippeve_client_t *client);
static void		ipp_identify_printer(ippeve_client_t *client);
static void		ipp_print_job(ippeve_client_t *client);
static void		ipp_print_uri(ippeve_client_t *client);
static void		ipp_send_document(ippeve_client_t *client);
static void		ipp_send_uri(ippeve_client_t *client);
static void		ipp_validate_job(ippeve_client_t *client);
static ipp_t		*load_ippserver_attributes(const char *servername, int serverport, const char *filename, cups_array_t *docformats);
static ipp_t		*load_legacy_attributes(const char *make, const char *model, int ppm, int ppm_color, int duplex, cups_array_t *docformats);
#if !CUPS_LITE
static ipp_t		*load_ppd_attributes(const char *ppdfile, cups_array_t *docformats);
#endif /* !CUPS_LITE */
static int		parse_options(ippeve_client_t *client, cups_option_t **options);
static void		process_attr_message(ippeve_job_t *job, char *message);
static void		*process_client(ippeve_client_t *client);
static int		process_http(ippeve_client_t *client);
static int		process_ipp(ippeve_client_t *client);
static void		*process_job(ippeve_job_t *job);
static void		process_state_message(ippeve_job_t *job, char *message);
static int		register_printer(ippeve_printer_t *printer, const char *subtypes);
static int		respond_http(ippeve_client_t *client, http_status_t code, const char *content_coding, const char *type, size_t length);
static void		respond_ipp(ippeve_client_t *client, ipp_status_t status, const char *message, ...) _CUPS_FORMAT(3, 4);
static void		respond_unsupported(ippeve_client_t *client, ipp_attribute_t *attr);
static void		run_printer(ippeve_printer_t *printer);
static int		show_media(ippeve_client_t *client);
static int		show_status(ippeve_client_t *client);
static int		show_supplies(ippeve_client_t *client);
static char		*time_string(time_t tv, char *buffer, size_t bufsize);
static void		usage(int status) _CUPS_NORETURN;
static int		valid_doc_attributes(ippeve_client_t *client);
static int		valid_job_attributes(ippeve_client_t *client);


/*
 * Globals...
 */

#ifdef HAVE_DNSSD
static DNSServiceRef	DNSSDMaster = NULL;
#elif defined(HAVE_AVAHI)
static AvahiThreadedPoll *DNSSDMaster = NULL;
static AvahiClient	*DNSSDClient = NULL;
#endif /* HAVE_DNSSD */

static int		KeepFiles = 0,	/* Keep spooled job files? */
			MaxVersion = 20,/* Maximum IPP version (20 = 2.0, 11 = 1.1, etc.) */
			Verbosity = 0;	/* Verbosity level */


/*
 * 'main()' - Main entry to the sample server.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  const char	*opt,			/* Current option character */
		*attrfile = NULL,	/* ippserver attributes file */
		*command = NULL,	/* Command to run with job files */
		*device_uri = NULL,	/* Device URI */
		*output_format = NULL,	/* Output format */
		*icon = NULL,		/* Icon file */
#ifdef HAVE_SSL
		*keypath = NULL,	/* Keychain path */
#endif /* HAVE_SSL */
		*location = "",		/* Location of printer */
		*make = "Example",	/* Manufacturer */
		*model = "Printer",	/* Model */
		*name = NULL,		/* Printer name */
#if !CUPS_LITE
		*ppdfile = NULL,	/* PPD file */
#endif /* !CUPS_LITE */
		*subtypes = "_print";	/* DNS-SD service subtype */
  int		legacy = 0,		/* Legacy mode? */
		duplex = 0,		/* Duplex mode */
		ppm = 10,		/* Pages per minute for mono */
		ppm_color = 0,		/* Pages per minute for color */
		web_forms = 1;		/* Enable web site forms? */
  ipp_t		*attrs = NULL;		/* Printer attributes */
  char		directory[1024] = "";	/* Spool directory */
  cups_array_t	*docformats = NULL;	/* Supported formats */
  const char	*servername = NULL;	/* Server host name */
  int		serverport = 0;		/* Server port number (0 = auto) */
  ippeve_printer_t *printer;		/* Printer object */


 /*
  * Parse command-line arguments...
  */

  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "--help"))
    {
      usage(0);
    }
    else if (!strcmp(argv[i], "--no-web-forms"))
    {
      web_forms = 0;
    }
    else if (!strcmp(argv[i], "--version"))
    {
      puts(CUPS_SVERSION);
      return (0);
    }
    else if (!strncmp(argv[i], "--", 2))
    {
      _cupsLangPrintf(stderr, _("%s: Unknown option \"%s\"."), argv[0], argv[i]);
      usage(1);
    }
    else if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
        switch (*opt)
	{
	  case '2' : /* -2 (enable 2-sided printing) */
	      duplex = 1;
	      legacy = 1;
	      break;

          case 'D' : /* -D device-uri */
	      i ++;
	      if (i >= argc)
	        usage(1);

	      device_uri = argv[i];
	      break;

          case 'F' : /* -F output/format */
	      i ++;
	      if (i >= argc)
	        usage(1);

	      output_format = argv[i];
	      break;

#ifdef HAVE_SSL
	  case 'K' : /* -K keypath */
	      i ++;
	      if (i >= argc)
	        usage(1);

	      keypath = argv[i];
	      break;
#endif /* HAVE_SSL */

	  case 'M' : /* -M manufacturer */
	      i ++;
	      if (i >= argc)
	        usage(1);

	      make   = argv[i];
	      legacy = 1;
	      break;

#if !CUPS_LITE
          case 'P' : /* -P filename.ppd */
	      i ++;
	      if (i >= argc)
	        usage(1);

              ppdfile = argv[i];
              break;
#endif /* !CUPS_LITE */

          case 'V' : /* -V max-version */
	      i ++;
	      if (i >= argc)
	        usage(1);

	      if (!strcmp(argv[i], "2.0"))
                MaxVersion = 20;
	      else if (!strcmp(argv[i], "1.1"))
                MaxVersion = 11;
	      else
	        usage(1);
              break;

	  case 'a' : /* -a attributes-file */
	      i ++;
	      if (i >= argc)
	        usage(1);

	      attrfile = argv[i];
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

	      docformats = _cupsArrayNewStrings(argv[i], ',');
	      legacy     = 1;
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

	      model  = argv[i];
	      legacy = 1;
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

	      serverport = atoi(argv[i]);
	      break;

	  case 'r' : /* -r subtype */
	      i ++;
	      if (i >= argc)
	        usage(1);

	      subtypes = argv[i];
	      break;

	  case 's' : /* -s speed[,color-speed] */
	      i ++;
	      if (i >= argc)
	        usage(1);

	      if (sscanf(argv[i], "%d,%d", &ppm, &ppm_color) < 1)
	        usage(1);

	      legacy = 1;
	      break;

	  case 'v' : /* -v (be verbose) */
	      Verbosity ++;
	      break;

          default : /* Unknown */
	      _cupsLangPrintf(stderr, _("%s: Unknown option \"-%c\"."), argv[0], *opt);
	      usage(1);
	}
      }
    }
    else if (!name)
    {
      name = argv[i];
    }
    else
    {
      _cupsLangPrintf(stderr, _("%s: Unknown option \"%s\"."), argv[0], argv[i]);
      usage(1);
    }
  }

  if (!name)
    usage(1);

#if CUPS_LITE
  if (attrfile != NULL && legacy)
    usage(1);
#else
  if (((ppdfile != NULL) + (attrfile != NULL) + legacy) > 1)
    usage(1);
#endif /* CUPS_LITE */

 /*
  * Apply defaults as needed...
  */

  if (!serverport)
  {
#ifdef _WIN32
   /*
    * Windows is almost always used as a single user system, so use a default
    * port number of 8631.
    */

    serverport = 8631;

#else
   /*
    * Use 8000 + UID mod 1000 for the default port number...
    */

    serverport = 8000 + ((int)getuid() % 1000);
#endif /* _WIN32 */

    _cupsLangPrintf(stderr, _("Listening on port %d."), serverport);
  }

  if (!directory[0])
  {
    const char *tmpdir;			/* Temporary directory */

#ifdef _WIN32
    if ((tmpdir = getenv("TEMP")) == NULL)
      tmpdir = "C:/TEMP";
#elif defined(__APPLE__) && TARGET_OS_OSX
    if ((tmpdir = getenv("TMPDIR")) == NULL)
      tmpdir = "/private/tmp";
#else
    if ((tmpdir = getenv("TMPDIR")) == NULL)
      tmpdir = "/tmp";
#endif /* _WIN32 */

    snprintf(directory, sizeof(directory), "%s/ippeveprinter.%d", tmpdir, (int)getpid());

    if (mkdir(directory, 0755) && errno != EEXIST)
    {
      _cupsLangPrintf(stderr, _("Unable to create spool directory \"%s\": %s"), directory, strerror(errno));
      usage(1);
    }

    if (Verbosity)
      _cupsLangPrintf(stderr, _("Using spool directory \"%s\"."), directory);
  }

 /*
  * Initialize DNS-SD...
  */

  dnssd_init();

 /*
  * Create the printer...
  */

  if (!docformats)
    docformats = _cupsArrayNewStrings(ppm_color > 0 ? "image/jpeg,image/pwg-raster,image/urf": "image/pwg-raster,image/urf", ',');

  if (attrfile)
    attrs = load_ippserver_attributes(servername, serverport, attrfile, docformats);
#if !CUPS_LITE
  else if (ppdfile)
  {
    attrs = load_ppd_attributes(ppdfile, docformats);

    if (!command)
      command = "ippeveps";

    if (!output_format)
      output_format = "application/postscript";
  }
#endif /* !CUPS_LITE */
  else
    attrs = load_legacy_attributes(make, model, ppm, ppm_color, duplex, docformats);

  if ((printer = create_printer(servername, serverport, name, location, icon, docformats, subtypes, directory, command, device_uri, output_format, attrs)) == NULL)
    return (1);

  printer->web_forms = web_forms;

#if !CUPS_LITE
  if (ppdfile)
    printer->ppdfile = strdup(ppdfile);
#endif /* !CUPS_LITE */

#ifdef HAVE_SSL
  cupsSetServerCredentials(keypath, printer->hostname, 1);
#endif /* HAVE_SSL */

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
clean_jobs(ippeve_printer_t *printer)	/* I - Printer */
{
  ippeve_job_t	*job;			/* Current job */
  time_t	cleantime;		/* Clean time */


  if (cupsArrayCount(printer->jobs) == 0)
    return;

  cleantime = time(NULL) - 60;

  _cupsRWLockWrite(&(printer->rwlock));
  for (job = (ippeve_job_t *)cupsArrayFirst(printer->jobs);
       job;
       job = (ippeve_job_t *)cupsArrayNext(printer->jobs))
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
compare_jobs(ippeve_job_t *a,		/* I - First job */
             ippeve_job_t *b)		/* I - Second job */
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
  ippeve_filter_t	filter;			/* Filter data */


  filter.ra        = ra;
  filter.group_tag = group_tag;

  ippCopyAttributes(to, from, quickcopy, (ipp_copycb_t)filter_cb, &filter);
}


/*
 * 'copy_job_attrs()' - Copy job attributes to the response.
 */

static void
copy_job_attributes(
    ippeve_client_t *client,		/* I - Client */
    ippeve_job_t    *job,			/* I - Job */
    cups_array_t  *ra)			/* I - requested-attributes */
{
  copy_attributes(client->response, job->attrs, ra, IPP_TAG_JOB, 0);

  if (!ra || cupsArrayFind(ra, "date-time-at-completed"))
  {
    if (job->completed)
      ippAddDate(client->response, IPP_TAG_JOB, "date-time-at-completed", ippTimeToDate(job->completed));
    else
      ippAddOutOfBand(client->response, IPP_TAG_JOB, IPP_TAG_NOVALUE, "date-time-at-completed");
  }

  if (!ra || cupsArrayFind(ra, "date-time-at-processing"))
  {
    if (job->processing)
      ippAddDate(client->response, IPP_TAG_JOB, "date-time-at-processing", ippTimeToDate(job->processing));
    else
      ippAddOutOfBand(client->response, IPP_TAG_JOB, IPP_TAG_NOVALUE, "date-time-at-processing");
  }

  if (!ra || cupsArrayFind(ra, "job-impressions"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-impressions", job->impressions);

  if (!ra || cupsArrayFind(ra, "job-impressions-completed"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-impressions-completed", job->impcompleted);

  if (!ra || cupsArrayFind(ra, "job-printer-up-time"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-printer-up-time", (int)(time(NULL) - client->printer->start_time));

  if (!ra || cupsArrayFind(ra, "job-state"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_ENUM, "job-state", (int)job->state);

  if (!ra || cupsArrayFind(ra, "job-state-message"))
  {
    if (job->message)
    {
      ippAddString(client->response, IPP_TAG_JOB, IPP_TAG_TEXT, "job-state-message", NULL, job->message);
    }
    else
    {
      switch (job->state)
      {
	case IPP_JSTATE_PENDING :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job pending.");
	    break;

	case IPP_JSTATE_HELD :
	    if (job->fd >= 0)
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job incoming.");
	    else if (ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_ZERO))
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job held.");
	    else
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job created.");
	    break;

	case IPP_JSTATE_PROCESSING :
	    if (job->cancel)
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job canceling.");
	    else
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job printing.");
	    break;

	case IPP_JSTATE_STOPPED :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job stopped.");
	    break;

	case IPP_JSTATE_CANCELED :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job canceled.");
	    break;

	case IPP_JSTATE_ABORTED :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job aborted.");
	    break;

	case IPP_JSTATE_COMPLETED :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job completed.");
	    break;
      }
    }
  }

  if (!ra || cupsArrayFind(ra, "job-state-reasons"))
  {
    switch (job->state)
    {
      case IPP_JSTATE_PENDING :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons",
		       NULL, "none");
	  break;

      case IPP_JSTATE_HELD :
          if (job->fd >= 0)
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_CONST_TAG(IPP_TAG_KEYWORD),
	                 "job-state-reasons", NULL, "job-incoming");
	  else if (ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_ZERO))
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_CONST_TAG(IPP_TAG_KEYWORD),
	                 "job-state-reasons", NULL, "job-hold-until-specified");
          else
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_CONST_TAG(IPP_TAG_KEYWORD),
	                 "job-state-reasons", NULL, "job-data-insufficient");
	  break;

      case IPP_JSTATE_PROCESSING :
	  if (job->cancel)
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_CONST_TAG(IPP_TAG_KEYWORD),
	                 "job-state-reasons", NULL, "processing-to-stop-point");
	  else
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_CONST_TAG(IPP_TAG_KEYWORD),
	                 "job-state-reasons", NULL, "job-printing");
	  break;

      case IPP_JSTATE_STOPPED :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons",
		       NULL, "job-stopped");
	  break;

      case IPP_JSTATE_CANCELED :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons",
		       NULL, "job-canceled-by-user");
	  break;

      case IPP_JSTATE_ABORTED :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons",
		       NULL, "aborted-by-system");
	  break;

      case IPP_JSTATE_COMPLETED :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons",
		       NULL, "job-completed-successfully");
	  break;
    }
  }

  if (!ra || cupsArrayFind(ra, "time-at-completed"))
    ippAddInteger(client->response, IPP_TAG_JOB,
                  job->completed ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE,
                  "time-at-completed", (int)(job->completed - client->printer->start_time));

  if (!ra || cupsArrayFind(ra, "time-at-processing"))
    ippAddInteger(client->response, IPP_TAG_JOB,
                  job->processing ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE,
                  "time-at-processing", (int)(job->processing - client->printer->start_time));
}


/*
 * 'create_client()' - Accept a new network connection and create a client
 *                     object.
 */

static ippeve_client_t *			/* O - Client */
create_client(ippeve_printer_t *printer,	/* I - Printer */
              int            sock)	/* I - Listen socket */
{
  ippeve_client_t	*client;		/* Client */


  if ((client = calloc(1, sizeof(ippeve_client_t))) == NULL)
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

static ippeve_job_t *			/* O - Job */
create_job(ippeve_client_t *client)	/* I - Client */
{
  ippeve_job_t		*job;		/* Job */
  ipp_attribute_t	*attr;		/* Job attribute */
  char			uri[1024],	/* job-uri value */
			uuid[64];	/* job-uuid value */


  _cupsRWLockWrite(&(client->printer->rwlock));
  if (client->printer->active_job &&
      client->printer->active_job->state < IPP_JSTATE_CANCELED)
  {
   /*
    * Only accept a single job at a time...
    */

    _cupsRWUnlock(&(client->printer->rwlock));
    return (NULL);
  }

 /*
  * Allocate and initialize the job object...
  */

  if ((job = calloc(1, sizeof(ippeve_job_t))) == NULL)
  {
    perror("Unable to allocate memory for job");
    return (NULL);
  }

  job->printer    = client->printer;
  job->attrs      = ippNew();
  job->state      = IPP_JSTATE_HELD;
  job->fd         = -1;

 /*
  * Copy all of the job attributes...
  */

  copy_attributes(job->attrs, client->request, NULL, IPP_TAG_JOB, 0);

 /*
  * Get the requesting-user-name, document format, and priority...
  */

  if ((attr = ippFindAttribute(client->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
    job->username = ippGetString(attr, 0, NULL);
  else
    job->username = "anonymous";

  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-originating-user-name", NULL, job->username);

  if (ippGetOperation(client->request) != IPP_OP_CREATE_JOB)
  {
    if ((attr = ippFindAttribute(job->attrs, "document-format-detected", IPP_TAG_MIMETYPE)) != NULL)
      job->format = ippGetString(attr, 0, NULL);
    else if ((attr = ippFindAttribute(job->attrs, "document-format-supplied", IPP_TAG_MIMETYPE)) != NULL)
      job->format = ippGetString(attr, 0, NULL);
    else
      job->format = "application/octet-stream";
  }

  if ((attr = ippFindAttribute(client->request, "job-impressions", IPP_TAG_INTEGER)) != NULL)
    job->impressions = ippGetInteger(attr, 0);

  if ((attr = ippFindAttribute(client->request, "job-name", IPP_TAG_NAME)) != NULL)
    job->name = ippGetString(attr, 0, NULL);

 /*
  * Add job description attributes and add to the jobs array...
  */

  job->id = client->printer->next_job_id ++;

  snprintf(uri, sizeof(uri), "%s/%d", client->printer->uri, job->id);
  httpAssembleUUID(client->printer->hostname, client->printer->port, client->printer->name, job->id, uuid, sizeof(uuid));

  ippAddDate(job->attrs, IPP_TAG_JOB, "date-time-at-creation", ippTimeToDate(time(&job->created)));
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", job->id);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-uri", NULL, uri);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-uuid", NULL, uuid);
  if ((attr = ippFindAttribute(client->request, "printer-uri", IPP_TAG_URI)) != NULL)
    ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-printer-uri", NULL, ippGetString(attr, 0, NULL));
  else
    ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-printer-uri", NULL, client->printer->uri);
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "time-at-creation", (int)(job->created - client->printer->start_time));

  cupsArrayAdd(client->printer->jobs, job);
  client->printer->active_job = job;

  _cupsRWUnlock(&(client->printer->rwlock));

  return (job);
}


/*
 * 'create_job_file()' - Create a file for the document in a job.
 */

static int				/* O - File descriptor or -1 on error */
create_job_file(
    ippeve_job_t     *job,		/* I - Job */
    char             *fname,		/* I - Filename buffer */
    size_t           fnamesize,		/* I - Size of filename buffer */
    const char       *directory,	/* I - Directory to store in */
    const char       *ext)		/* I - Extension (`NULL` for default) */
{
  char			name[256],	/* "Safe" filename */
			*nameptr;	/* Pointer into filename */
  const char		*job_name;	/* job-name value */


 /*
  * Make a name from the job-name attribute...
  */

  if ((job_name = ippGetString(ippFindAttribute(job->attrs, "job-name", IPP_TAG_NAME), 0, NULL)) == NULL)
    job_name = "untitled";

  for (nameptr = name; *job_name && nameptr < (name + sizeof(name) - 1); job_name ++)
  {
    if (isalnum(*job_name & 255) || *job_name == '-')
    {
      *nameptr++ = (char)tolower(*job_name & 255);
    }
    else
    {
      *nameptr++ = '_';

      while (job_name[1] && !isalnum(job_name[1] & 255) && job_name[1] != '-')
        job_name ++;
    }
  }

  *nameptr = '\0';

 /*
  * Figure out the extension...
  */

  if (!ext)
  {
    if (!strcasecmp(job->format, "image/jpeg"))
      ext = "jpg";
    else if (!strcasecmp(job->format, "image/png"))
      ext = "png";
    else if (!strcasecmp(job->format, "image/pwg-raster"))
      ext = "pwg";
    else if (!strcasecmp(job->format, "image/urf"))
      ext = "urf";
    else if (!strcasecmp(job->format, "application/pdf"))
      ext = "pdf";
    else if (!strcasecmp(job->format, "application/postscript"))
      ext = "ps";
    else if (!strcasecmp(job->format, "application/vnd.hp-pcl"))
      ext = "pcl";
    else
      ext = "dat";
  }

 /*
  * Create a filename with the job-id, job-name, and document-format (extension)...
  */

  snprintf(fname, fnamesize, "%s/%d-%s.%s", directory, job->id, name, ext);

  return (open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0666));
}


/*
 * 'create_listener()' - Create a listener socket.
 */

static int				/* O - Listener socket or -1 on error */
create_listener(const char *name,	/* I - Host name (`NULL` for any address) */
                int        port,	/* I - Port number */
                int        family)	/* I - Address family */
{
  int			sock;		/* Listener socket */
  http_addrlist_t	*addrlist;	/* Listen address */
  char			service[255];	/* Service port */


  snprintf(service, sizeof(service), "%d", port);
  if ((addrlist = httpAddrGetList(name, family, service)) == NULL)
    return (-1);

  sock = httpAddrListen(&(addrlist->addr), port);

  httpAddrFreeList(addrlist);

  return (sock);
}


/*
 * 'create_media_col()' - Create a media-col value.
 */

static ipp_t *				/* O - media-col collection */
create_media_col(const char *media,	/* I - Media name */
		 const char *source,	/* I - Media source, if any */
		 const char *type,	/* I - Media type, if any */
		 int        width,	/* I - x-dimension in 2540ths */
		 int        length,	/* I - y-dimension in 2540ths */
		 int        bottom,	/* I - Bottom margin in 2540ths */
		 int        left,	/* I - Left margin in 2540ths */
		 int        right,	/* I - Right margin in 2540ths */
		 int        top)	/* I - Top margin in 2540ths */
{
  ipp_t		*media_col = ippNew(),	/* media-col value */
		*media_size = create_media_size(width, length);
					/* media-size value */
  char		media_key[256];		/* media-key value */
  const char	*media_key_suffix = "";	/* media-key suffix */


  if (bottom == 0 && left == 0 && right == 0 && top == 0)
    media_key_suffix = "_borderless";

  if (type && source)
    snprintf(media_key, sizeof(media_key), "%s_%s_%s%s", media, source, type, media_key_suffix);
  else if (type)
    snprintf(media_key, sizeof(media_key), "%s__%s%s", media, type, media_key_suffix);
  else if (source)
    snprintf(media_key, sizeof(media_key), "%s_%s%s", media, source, media_key_suffix);
  else
    snprintf(media_key, sizeof(media_key), "%s%s", media, media_key_suffix);

  ippAddString(media_col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-key", NULL, media_key);
  ippAddCollection(media_col, IPP_TAG_PRINTER, "media-size", media_size);
  ippAddString(media_col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-size-name", NULL, media);
  if (bottom >= 0)
    ippAddInteger(media_col, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-bottom-margin", bottom);
  if (left >= 0)
    ippAddInteger(media_col, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-left-margin", left);
  if (right >= 0)
    ippAddInteger(media_col, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-right-margin", right);
  if (top >= 0)
    ippAddInteger(media_col, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-top-margin", top);
  if (source)
    ippAddString(media_col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-source", NULL, source);
  if (type)
    ippAddString(media_col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-type", NULL, type);

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


  ippAddInteger(media_size, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "x-dimension", width);
  ippAddInteger(media_size, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "y-dimension", length);

  return (media_size);
}


/*
 * 'create_printer()' - Create, register, and listen for connections to a
 *                      printer object.
 */

static ippeve_printer_t *		/* O - Printer */
create_printer(
    const char   *servername,		/* I - Server hostname (NULL for default) */
    int          serverport,		/* I - Server port */
    const char   *name,			/* I - printer-name */
    const char   *location,		/* I - printer-location */
    const char   *icon,			/* I - printer-icons */
    cups_array_t *docformats,		/* I - document-format-supported */
    const char   *subtypes,		/* I - Bonjour service subtype(s) */
    const char   *directory,		/* I - Spool directory */
    const char   *command,		/* I - Command to run on job files, if any */
    const char   *device_uri,		/* I - Output device, if any */
    const char   *output_format,	/* I - Output format, if any */
    ipp_t        *attrs)		/* I - Capability attributes */
{
  ippeve_printer_t	*printer;	/* Printer */
  int			i;		/* Looping var */
#ifndef _WIN32
  char			path[1024];	/* Full path to command */
#endif /* !_WIN32 */
  char			uri[1024],	/* Printer URI */
#ifdef HAVE_SSL
			securi[1024],	/* Secure printer URI */
			*uris[2],	/* All URIs */
#endif /* HAVE_SSL */
			icons[1024],	/* printer-icons URI */
			adminurl[1024],	/* printer-more-info URI */
			supplyurl[1024],/* printer-supply-info-uri URI */
			uuid[128];	/* printer-uuid */
  int			k_supported;	/* Maximum file size supported */
  int			num_formats;	/* Number of supported document formats */
  const char		*formats[100],	/* Supported document formats */
			*format;	/* Current format */
  int			num_sup_attrs;	/* Number of supported attributes */
  const char		*sup_attrs[100];/* Supported attributes */
  char			xxx_supported[256];
					/* Name of -supported attribute */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global path values */
#ifdef HAVE_STATVFS
  struct statvfs	spoolinfo;	/* FS info for spool directory */
  double		spoolsize;	/* FS size */
#elif defined(HAVE_STATFS)
  struct statfs		spoolinfo;	/* FS info for spool directory */
  double		spoolsize;	/* FS size */
#endif /* HAVE_STATVFS */
  static const char * const versions[] =/* ipp-versions-supported values */
  {
    "1.1",
    "2.0"
  };
  static const char * const features[] =/* ipp-features-supported values */
  {
    "ipp-everywhere"
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
    IPP_OP_GET_PRINTER_ATTRIBUTES,
    IPP_OP_CANCEL_MY_JOBS,
    IPP_OP_CLOSE_JOB,
    IPP_OP_IDENTIFY_PRINTER
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
  static const char * const identify_actions[] =
  {
    "display",
    "sound"
  };
  static const char * const job_creation[] =
  {					/* job-creation-attributes-supported values */
    "copies",
    "document-access",
    "document-charset",
    "document-format",
    "document-message",
    "document-metadata",
    "document-name",
    "document-natural-language",
    "document-password",
    "finishings",
    "finishings-col",
    "ipp-attribute-fidelity",
    "job-account-id",
    "job-account-type",
    "job-accouunting-sheets",
    "job-accounting-user-id",
    "job-authorization-uri",
    "job-error-action",
    "job-error-sheet",
    "job-hold-until",
    "job-hold-until-time",
    "job-mandatory-attributes",
    "job-message-to-operator",
    "job-name",
    "job-pages-per-set",
    "job-password",
    "job-password-encryption",
    "job-phone-number",
    "job-priority",
    "job-recipient-name",
    "job-resource-ids",
    "job-sheet-message",
    "job-sheets",
    "job-sheets-col",
    "media",
    "media-col",
    "multiple-document-handling",
    "number-up",
    "orientation-requested",
    "output-bin",
    "output-device",
    "overrides",
    "page-delivery",
    "page-ranges",
    "presentation-direction-number-up",
    "print-color-mode",
    "print-content-optimize",
    "print-quality",
    "print-rendering-intent",
    "print-scaling",
    "printer-resolution",
    "proof-print",
    "separator-sheets",
    "sides",
    "x-image-position",
    "x-image-shift",
    "x-side1-image-shift",
    "x-side2-image-shift",
    "y-image-position",
    "y-image-shift",
    "y-side1-image-shift",
    "y-side2-image-shift"
  };
  static const char * const media_col_supported[] =
  {					/* media-col-supported values */
    "media-bottom-margin",
    "media-left-margin",
    "media-right-margin",
    "media-size",
    "media-size-name",
    "media-source",
    "media-top-margin",
    "media-type"
  };
  static const char * const multiple_document_handling[] =
  {					/* multiple-document-handling-supported values */
    "separate-documents-uncollated-copies",
    "separate-documents-collated-copies"
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
#ifdef HAVE_SSL
  static const char * const uri_authentication_supported[] =
  {					/* uri-authentication-supported values */
    "none",
    "none"
  };
  static const char * const uri_security_supported[] =
  {					/* uri-security-supported values */
    "none",
    "tls"
  };
#endif /* HAVE_SSL */
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


#ifndef _WIN32
 /*
  * If a command was specified, make sure it exists and is executable...
  */

  if (command)
  {
    if (*command == '/' || !strncmp(command, "./", 2))
    {
      if (access(command, X_OK))
      {
        _cupsLangPrintf(stderr, _("Unable to execute command \"%s\": %s"), command, strerror(errno));
	return (NULL);
      }
    }
    else
    {
      snprintf(path, sizeof(path), "%s/command/%s", cg->cups_serverbin, command);

      if (access(command, X_OK))
      {
        _cupsLangPrintf(stderr, _("Unable to execute command \"%s\": %s"), command, strerror(errno));
	return (NULL);
      }

      command = path;
    }
  }
#endif /* !_WIN32 */

 /*
  * Allocate memory for the printer...
  */

  if ((printer = calloc(1, sizeof(ippeve_printer_t))) == NULL)
  {
    _cupsLangPrintError(NULL, _("Unable to allocate memory for printer"));
    return (NULL);
  }

  printer->ipv4          = -1;
  printer->ipv6          = -1;
  printer->name          = strdup(name);
  printer->dnssd_name    = strdup(name);
  printer->command       = command ? strdup(command) : NULL;
  printer->device_uri    = device_uri ? strdup(device_uri) : NULL;
  printer->output_format = output_format ? strdup(output_format) : NULL;
  printer->directory     = strdup(directory);
  printer->icon          = icon ? strdup(icon) : NULL;
  printer->port          = serverport;
  printer->start_time    = time(NULL);
  printer->config_time   = printer->start_time;
  printer->state         = IPP_PSTATE_IDLE;
  printer->state_reasons = IPPEVE_PREASON_NONE;
  printer->state_time    = printer->start_time;
  printer->jobs          = cupsArrayNew((cups_array_func_t)compare_jobs, NULL);
  printer->next_job_id   = 1;

  if (servername)
  {
    printer->hostname = strdup(servername);
  }
  else
  {
    char	temp[1024];		/* Temporary string */

    printer->hostname = strdup(httpGetHostname(NULL, temp, sizeof(temp)));
  }

  _cupsRWInit(&(printer->rwlock));

 /*
  * Create the listener sockets...
  */

  if ((printer->ipv4 = create_listener(servername, printer->port, AF_INET)) < 0)
  {
    perror("Unable to create IPv4 listener");
    goto bad_printer;
  }

  if ((printer->ipv6 = create_listener(servername, printer->port, AF_INET6)) < 0)
  {
    perror("Unable to create IPv6 listener");
    goto bad_printer;
  }

 /*
  * Prepare URI values for the printer attributes...
  */

  httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL, printer->hostname, printer->port, "/ipp/print");
  printer->uri    = strdup(uri);
  printer->urilen = strlen(uri);

#ifdef HAVE_SSL
  httpAssembleURI(HTTP_URI_CODING_ALL, securi, sizeof(securi), "ipps", NULL, printer->hostname, printer->port, "/ipp/print");
#endif /* HAVE_SSL */

  httpAssembleURI(HTTP_URI_CODING_ALL, icons, sizeof(icons), WEB_SCHEME, NULL, printer->hostname, printer->port, "/icon.png");
  httpAssembleURI(HTTP_URI_CODING_ALL, adminurl, sizeof(adminurl), WEB_SCHEME, NULL, printer->hostname, printer->port, "/");
  httpAssembleURI(HTTP_URI_CODING_ALL, supplyurl, sizeof(supplyurl), WEB_SCHEME, NULL, printer->hostname, printer->port, "/supplies");
  httpAssembleUUID(printer->hostname, serverport, name, 0, uuid, sizeof(uuid));

  if (Verbosity)
  {
    fprintf(stderr, "printer-more-info=\"%s\"\n", adminurl);
    fprintf(stderr, "printer-supply-info-uri=\"%s\"\n", supplyurl);
#ifdef HAVE_SSL
    fprintf(stderr, "printer-uri=\"%s\",\"%s\"\n", uri, securi);
#else
    fprintf(stderr, "printer-uri=\"%s\"\n", uri);
#endif /* HAVE_SSL */
  }

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
  * Assemble the final list of document formats...
  */

  if (!cupsArrayFind(docformats, (void *)"application/octet-stream"))
    cupsArrayAdd(docformats, (void *)"application/octet-stream");

  for (num_formats = 0, format = (const char *)cupsArrayFirst(docformats); format && num_formats < (int)(sizeof(formats) / sizeof(formats[0])); format = (const char *)cupsArrayNext(docformats))
    formats[num_formats ++] = format;

 /*
  * Get the list of attributes that can be used when creating a job...
  */

  num_sup_attrs = 0;
  sup_attrs[num_sup_attrs ++] = "document-access";
  sup_attrs[num_sup_attrs ++] = "document-charset";
  sup_attrs[num_sup_attrs ++] = "document-format";
  sup_attrs[num_sup_attrs ++] = "document-message";
  sup_attrs[num_sup_attrs ++] = "document-metadata";
  sup_attrs[num_sup_attrs ++] = "document-name";
  sup_attrs[num_sup_attrs ++] = "document-natural-language";
  sup_attrs[num_sup_attrs ++] = "ipp-attribute-fidelity";
  sup_attrs[num_sup_attrs ++] = "job-name";
  sup_attrs[num_sup_attrs ++] = "job-priority";

  for (i = 0; i < (int)(sizeof(job_creation) / sizeof(job_creation[0])) && num_sup_attrs < (int)(sizeof(sup_attrs) / sizeof(sup_attrs[0])); i ++)
  {
    snprintf(xxx_supported, sizeof(xxx_supported), "%s-supported", job_creation[i]);
    if (ippFindAttribute(attrs, xxx_supported, IPP_TAG_ZERO))
      sup_attrs[num_sup_attrs ++] = job_creation[i];
  }

 /*
  * Fill out the rest of the printer attributes.
  */

  printer->attrs = attrs;

  /* charset-configured */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_CHARSET), "charset-configured", NULL, "utf-8");

  /* charset-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_CHARSET), "charset-supported", sizeof(charsets) / sizeof(charsets[0]), NULL, charsets);

  /* compression-supported */
  if (!ippFindAttribute(printer->attrs, "compression-supported", IPP_TAG_ZERO))
    ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "compression-supported", (int)(sizeof(compressions) / sizeof(compressions[0])), NULL, compressions);

  /* document-format-default */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "document-format-default", NULL, "application/octet-stream");

  /* document-format-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_MIMETYPE, "document-format-supported", num_formats, NULL, formats);

  /* generated-natural-language-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_LANGUAGE), "generated-natural-language-supported", NULL, "en");

  /* identify-actions-default */
  ippAddString (printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "identify-actions-default", NULL, "sound");

  /* identify-actions-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "identify-actions-supported", sizeof(identify_actions) / sizeof(identify_actions[0]), NULL, identify_actions);

  /* ipp-features-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "ipp-features-supported", sizeof(features) / sizeof(features[0]), NULL, features);

  /* ipp-versions-supported */
  if (MaxVersion == 11)
    ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "ipp-versions-supported", NULL, "1.1");
  else
    ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "ipp-versions-supported", (int)(sizeof(versions) / sizeof(versions[0])), NULL, versions);

  /* job-creation-attributes-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-creation-attributes-supported", num_sup_attrs, NULL, sup_attrs);

  /* job-ids-supported */
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "job-ids-supported", 1);

  /* job-k-octets-supported */
  ippAddRange(printer->attrs, IPP_TAG_PRINTER, "job-k-octets-supported", 0, k_supported);

  /* job-priority-default */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "job-priority-default", 50);

  /* job-priority-supported */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "job-priority-supported", 1);

  /* job-sheets-default */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_NAME), "job-sheets-default", NULL, "none");

  /* job-sheets-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_NAME), "job-sheets-supported", NULL, "none");

  /* media-col-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-col-supported", (int)(sizeof(media_col_supported) / sizeof(media_col_supported[0])), NULL, media_col_supported);

  /* multiple-document-handling-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "multiple-document-handling-supported", sizeof(multiple_document_handling) / sizeof(multiple_document_handling[0]), NULL, multiple_document_handling);

  /* multiple-document-jobs-supported */
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "multiple-document-jobs-supported", 0);

  /* multiple-operation-time-out */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "multiple-operation-time-out", 60);

  /* multiple-operation-time-out-action */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "multiple-operation-time-out-action", NULL, "abort-job");

  /* natural-language-configured */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_LANGUAGE), "natural-language-configured", NULL, "en");

  /* operations-supported */
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "operations-supported", sizeof(ops) / sizeof(ops[0]), ops);

  /* pdl-override-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pdl-override-supported", NULL, "attempted");

  /* preferred-attributes-supported */
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "preferred-attributes-supported", 0);

  /* printer-get-attributes-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-get-attributes-supported", NULL, "document-format");

  /* printer-geo-location */
  ippAddOutOfBand(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_UNKNOWN, "printer-geo-location");

  /* printer-icons */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-icons", NULL, icons);

  /* printer-is-accepting-jobs */
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "printer-is-accepting-jobs", 1);

  /* printer-info */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info", NULL, name);

  /* printer-location */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-location", NULL, location);

  /* printer-more-info */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-more-info", NULL, adminurl);

  /* printer-name */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-name", NULL, name);

  /* printer-organization */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_TEXT), "printer-organization", NULL, "");

  /* printer-organizational-unit */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_TEXT), "printer-organizational-unit", NULL, "");

  /* printer-supply-info-uri */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-supply-info-uri", NULL, supplyurl);

  /* printer-uri-supported */
#ifdef HAVE_SSL
  uris[0] = uri;
  uris[1] = securi;

  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri-supported", 2, NULL, (const char **)uris);

#else
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri-supported", NULL, uri);
#endif /* HAVE_SSL */

  /* printer-uuid */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uuid", NULL, uuid);

  /* reference-uri-scheme-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_URISCHEME), "reference-uri-schemes-supported", (int)(sizeof(reference_uri_schemes_supported) / sizeof(reference_uri_schemes_supported[0])), NULL, reference_uri_schemes_supported);

  /* uri-authentication-supported */
#ifdef HAVE_SSL
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-authentication-supported", 2, NULL, uri_authentication_supported);
#else
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-authentication-supported", NULL, "none");
#endif /* HAVE_SSL */

  /* uri-security-supported */
#ifdef HAVE_SSL
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-security-supported", 2, NULL, uri_security_supported);
#else
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-security-supported", NULL, "none");
#endif /* HAVE_SSL */

  /* which-jobs-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "which-jobs-supported", sizeof(which_jobs) / sizeof(which_jobs[0]), NULL, which_jobs);

  debug_attributes("Printer", printer->attrs, 0);

 /*
  * Register the printer with Bonjour...
  */

  if (!register_printer(printer, subtypes))
    goto bad_printer;

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
delete_client(ippeve_client_t *client)	/* I - Client */
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
delete_job(ippeve_job_t *job)		/* I - Job */
{
  if (Verbosity)
    fprintf(stderr, "[Job %d] Removing job from history.\n", job->id);

  ippDelete(job->attrs);

  if (job->message)
    free(job->message);

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
delete_printer(ippeve_printer_t *printer)	/* I - Printer */
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
  if (printer->ipps_ref)
    DNSServiceRefDeallocate(printer->ipps_ref);
  if (printer->http_ref)
    DNSServiceRefDeallocate(printer->http_ref);
#elif defined(HAVE_AVAHI)
  avahi_threaded_poll_lock(DNSSDMaster);

  if (printer->printer_ref)
    avahi_entry_group_free(printer->printer_ref);
  if (printer->ipp_ref)
    avahi_entry_group_free(printer->ipp_ref);
  if (printer->ipps_ref)
    avahi_entry_group_free(printer->ipps_ref);
  if (printer->http_ref)
    avahi_entry_group_free(printer->http_ref);

  avahi_threaded_poll_unlock(DNSSDMaster);
#endif /* HAVE_DNSSD */

  if (printer->dnssd_name)
    free(printer->dnssd_name);
  if (printer->name)
    free(printer->name);
  if (printer->icon)
    free(printer->icon);
  if (printer->command)
    free(printer->command);
  if (printer->device_uri)
    free(printer->device_uri);
#if !CUPS_LITE
  if (printer->ppdfile)
    free(printer->ppdfile);
#endif /* !CUPS_LITE */
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

static void DNSSD_API
dnssd_callback(
    DNSServiceRef       sdRef,		/* I - Service reference */
    DNSServiceFlags     flags,		/* I - Status flags */
    DNSServiceErrorType errorCode,	/* I - Error, if any */
    const char          *name,		/* I - Service name */
    const char          *regtype,	/* I - Service type */
    const char          *domain,	/* I - Domain for service */
    ippeve_printer_t      *printer)	/* I - Printer */
{
  (void)sdRef;
  (void)flags;
  (void)domain;

  if (errorCode)
  {
    fprintf(stderr, "DNSServiceRegister for %s failed with error %d.\n", regtype, (int)errorCode);
    return;
  }
  else if (strcasecmp(name, printer->dnssd_name))
  {
    if (Verbosity)
      fprintf(stderr, "Now using DNS-SD service name \"%s\".\n", name);

    /* No lock needed since only the main thread accesses/changes this */
    free(printer->dnssd_name);
    printer->dnssd_name = strdup(name);
  }
}


#elif defined(HAVE_AVAHI)
/*
 * 'dnssd_callback()' - Handle Bonjour registration events.
 */

static void
dnssd_callback(
    AvahiEntryGroup      *srv,		/* I - Service */
    AvahiEntryGroupState state,		/* I - Registration state */
    void                 *context)	/* I - Printer */
{
  (void)srv;
  (void)state;
  (void)context;
}


/*
 * 'dnssd_client_cb()' - Client callback for Avahi.
 *
 * Called whenever the client or server state changes...
 */

static void
dnssd_client_cb(
    AvahiClient      *c,		/* I - Client */
    AvahiClientState state,		/* I - Current state */
    void             *userdata)		/* I - User data (unused) */
{
  (void)userdata;

  if (!c)
    return;

  switch (state)
  {
    default :
        fprintf(stderr, "Ignored Avahi state %d.\n", state);
	break;

    case AVAHI_CLIENT_FAILURE:
	if (avahi_client_errno(c) == AVAHI_ERR_DISCONNECTED)
	{
	  fputs("Avahi server crashed, exiting.\n", stderr);
	  exit(1);
	}
	break;
  }
}
#endif /* HAVE_DNSSD */


/*
 * 'dnssd_init()' - Initialize the DNS-SD service connections...
 */

static void
dnssd_init(void)
{
#ifdef HAVE_DNSSD
  if (DNSServiceCreateConnection(&DNSSDMaster) != kDNSServiceErr_NoError)
  {
    fputs("Error: Unable to initialize Bonjour.\n", stderr);
    exit(1);
  }

#elif defined(HAVE_AVAHI)
  int error;			/* Error code, if any */

  if ((DNSSDMaster = avahi_threaded_poll_new()) == NULL)
  {
    fputs("Error: Unable to initialize Bonjour.\n", stderr);
    exit(1);
  }

  if ((DNSSDClient = avahi_client_new(avahi_threaded_poll_get(DNSSDMaster), AVAHI_CLIENT_NO_FAIL, dnssd_client_cb, NULL, &error)) == NULL)
  {
    fputs("Error: Unable to initialize Bonjour.\n", stderr);
    exit(1);
  }

  avahi_threaded_poll_start(DNSSDMaster);
#endif /* HAVE_DNSSD */
}


/*
 * 'filter_cb()' - Filter printer attributes based on the requested array.
 */

static int				/* O - 1 to copy, 0 to ignore */
filter_cb(ippeve_filter_t   *filter,	/* I - Filter parameters */
          ipp_t           *dst,		/* I - Destination (unused) */
	  ipp_attribute_t *attr)	/* I - Source attribute */
{
 /*
  * Filter attributes as needed...
  */

#ifndef _WIN32 /* Avoid MS compiler bug */
  (void)dst;
#endif /* !_WIN32 */

  ipp_tag_t group = ippGetGroupTag(attr);
  const char *name = ippGetName(attr);

  if ((filter->group_tag != IPP_TAG_ZERO && group != filter->group_tag && group != IPP_TAG_ZERO) || !name || (!strcmp(name, "media-col-database") && !cupsArrayFind(filter->ra, (void *)name)))
    return (0);

  return (!filter->ra || cupsArrayFind(filter->ra, (void *)name) != NULL);
}


/*
 * 'find_job()' - Find a job specified in a request.
 */

static ippeve_job_t *			/* O - Job or NULL */
find_job(ippeve_client_t *client)		/* I - Client */
{
  ipp_attribute_t	*attr;		/* job-id or job-uri attribute */
  ippeve_job_t		key,		/* Job search key */
			*job;		/* Matching job, if any */


  if ((attr = ippFindAttribute(client->request, "job-uri", IPP_TAG_URI)) != NULL)
  {
    const char *uri = ippGetString(attr, 0, NULL);

    if (!strncmp(uri, client->printer->uri, client->printer->urilen) &&
        uri[client->printer->urilen] == '/')
      key.id = atoi(uri + client->printer->urilen + 1);
    else
      return (NULL);
  }
  else if ((attr = ippFindAttribute(client->request, "job-id", IPP_TAG_INTEGER)) != NULL)
    key.id = ippGetInteger(attr, 0);

  _cupsRWLockRead(&(client->printer->rwlock));
  job = (ippeve_job_t *)cupsArrayFind(client->printer->jobs, &key);
  _cupsRWUnlock(&(client->printer->rwlock));

  return (job);
}


/*
 * 'finish_document()' - Finish receiving a document file and start processing.
 */

static void
finish_document_data(
    ippeve_client_t *client,		/* I - Client */
    ippeve_job_t    *job)		/* I - Job */
{
  char			filename[1024],	/* Filename buffer */
			buffer[4096];	/* Copy buffer */
  ssize_t		bytes;		/* Bytes read */
  cups_array_t		*ra;		/* Attributes to send in response */
  _cups_thread_t        t;              /* Thread */


 /*
  * Create a file for the request data...
  *
  * TODO: Update code to support piping large raster data to the print command.
  */

  if ((job->fd = create_job_file(job, filename, sizeof(filename), client->printer->directory, NULL)) < 0)
  {
    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL, "Unable to create print file: %s", strerror(errno));

    goto abort_job;
  }

  if (Verbosity)
    fprintf(stderr, "Created job file \"%s\", format \"%s\".\n", filename, job->format);

  while ((bytes = httpRead2(client->http, buffer, sizeof(buffer))) > 0)
  {
    if (write(job->fd, buffer, (size_t)bytes) < bytes)
    {
      int error = errno;		/* Write error */

      close(job->fd);
      job->fd = -1;

      unlink(filename);

      respond_ipp(client, IPP_STATUS_ERROR_INTERNAL, "Unable to write print file: %s", strerror(error));

      goto abort_job;
    }
  }

  if (bytes < 0)
  {
   /*
    * Got an error while reading the print data, so abort this job.
    */

    close(job->fd);
    job->fd = -1;

    unlink(filename);

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL, "Unable to read print file.");

    goto abort_job;
  }

  if (close(job->fd))
  {
    int error = errno;			/* Write error */

    job->fd = -1;

    unlink(filename);

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL, "Unable to write print file: %s", strerror(error));

    goto abort_job;
  }

  job->fd       = -1;
  job->filename = strdup(filename);
  job->state    = IPP_JSTATE_PENDING;

 /*
  * Process the job...
  */

  t = _cupsThreadCreate((_cups_thread_func_t)process_job, job);

  if (t)
  {
    _cupsThreadDetach(t);
  }
  else
  {
    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL, "Unable to process job.");
    goto abort_job;
  }

 /*
  * Return the job info...
  */

  respond_ipp(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-message");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
  return;

 /*
  * If we get here we had to abort the job...
  */

  abort_job:

  job->state     = IPP_JSTATE_ABORTED;
  job->completed = time(NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
}


/*
 * 'finish_uri()' - Finish fetching a document URI and start processing.
 */

static void
finish_document_uri(
    ippeve_client_t *client,		/* I - Client */
    ippeve_job_t    *job)		/* I - Job */
{
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


 /*
  * Do we have a file to print?
  */

  if (httpGetState(client->http) == HTTP_STATE_POST_RECV)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "Unexpected document data following request.");

    goto abort_job;
  }

 /*
  * Do we have a document URI?
  */

  if ((uri = ippFindAttribute(client->request, "document-uri", IPP_TAG_URI)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing document-uri.");

    goto abort_job;
  }

  if (ippGetCount(uri) != 1)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "Too many document-uri values.");

    goto abort_job;
  }

  uri_status = httpSeparateURI(HTTP_URI_CODING_ALL, ippGetString(uri, 0, NULL),
                               scheme, sizeof(scheme), userpass,
                               sizeof(userpass), hostname, sizeof(hostname),
                               &port, resource, sizeof(resource));
  if (uri_status < HTTP_URI_STATUS_OK)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "Bad document-uri: %s", httpURIStatusString(uri_status));

    goto abort_job;
  }

  if (strcmp(scheme, "file") &&
#ifdef HAVE_SSL
      strcmp(scheme, "https") &&
#endif /* HAVE_SSL */
      strcmp(scheme, "http"))
  {
    respond_ipp(client, IPP_STATUS_ERROR_URI_SCHEME, "URI scheme \"%s\" not supported.", scheme);

    goto abort_job;
  }

  if (!strcmp(scheme, "file") && access(resource, R_OK))
  {
    respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS, "Unable to access URI: %s", strerror(errno));

    goto abort_job;
  }

 /*
  * Get the document format for the job...
  */

  _cupsRWLockWrite(&(client->printer->rwlock));

  if ((attr = ippFindAttribute(job->attrs, "document-format", IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else
    job->format = "application/octet-stream";

 /*
  * Create a file for the request data...
  */

  if ((job->fd = create_job_file(job, filename, sizeof(filename), client->printer->directory, NULL)) < 0)
  {
    _cupsRWUnlock(&(client->printer->rwlock));

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL, "Unable to create print file: %s", strerror(errno));

    goto abort_job;
  }

  _cupsRWUnlock(&(client->printer->rwlock));

  if (!strcmp(scheme, "file"))
  {
    if ((infile = open(resource, O_RDONLY)) < 0)
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS, "Unable to access URI: %s", strerror(errno));

      goto abort_job;
    }

    do
    {
      if ((bytes = read(infile, buffer, sizeof(buffer))) < 0 &&
          (errno == EAGAIN || errno == EINTR))
      {
        bytes = 1;
      }
      else if (bytes > 0 && write(job->fd, buffer, (size_t)bytes) < bytes)
      {
	int error = errno;		/* Write error */

	close(job->fd);
	job->fd = -1;

	unlink(filename);
	close(infile);

	respond_ipp(client, IPP_STATUS_ERROR_INTERNAL, "Unable to write print file: %s", strerror(error));

        goto abort_job;
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

    if ((http = httpConnect2(hostname, port, NULL, AF_UNSPEC, encryption, 1, 30000, NULL)) == NULL)
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS, "Unable to connect to %s: %s", hostname, cupsLastErrorString());

      close(job->fd);
      job->fd = -1;

      unlink(filename);

      goto abort_job;
    }

    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_ACCEPT_LANGUAGE, "en");
    if (httpGet(http, resource))
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS, "Unable to GET URI: %s", strerror(errno));

      close(job->fd);
      job->fd = -1;

      unlink(filename);
      httpClose(http);

      goto abort_job;
    }

    while ((status = httpUpdate(http)) == HTTP_STATUS_CONTINUE);

    if (status != HTTP_STATUS_OK)
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS, "Unable to GET URI: %s", httpStatus(status));

      close(job->fd);
      job->fd = -1;

      unlink(filename);
      httpClose(http);

      goto abort_job;
    }

    while ((bytes = httpRead2(http, buffer, sizeof(buffer))) > 0)
    {
      if (write(job->fd, buffer, (size_t)bytes) < bytes)
      {
	int error = errno;		/* Write error */

	close(job->fd);
	job->fd = -1;

	unlink(filename);
	httpClose(http);

	respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
		    "Unable to write print file: %s", strerror(error));

        goto abort_job;
      }
    }

    httpClose(http);
  }

  if (close(job->fd))
  {
    int error = errno;		/* Write error */

    job->fd = -1;

    unlink(filename);

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL, "Unable to write print file: %s", strerror(error));

    goto abort_job;
  }

  _cupsRWLockWrite(&(client->printer->rwlock));

  job->fd       = -1;
  job->filename = strdup(filename);
  job->state    = IPP_JSTATE_PENDING;

  _cupsRWUnlock(&(client->printer->rwlock));

 /*
  * Process the job...
  */

  process_job(job);

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
  return;

 /*
  * If we get here we had to abort the job...
  */

  abort_job:

  job->state     = IPP_JSTATE_ABORTED;
  job->completed = time(NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
}


/*
 * 'html_escape()' - Write a HTML-safe string.
 */

static void
html_escape(ippeve_client_t *client,	/* I - Client */
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
        httpWrite2(client->http, start, (size_t)(s - start));

      if (*s == '&')
        httpWrite2(client->http, "&amp;", 5);
      else
        httpWrite2(client->http, "&lt;", 4);

      start = s + 1;
    }

    s ++;
  }

  if (s > start)
    httpWrite2(client->http, start, (size_t)(s - start));
}


/*
 * 'html_footer()' - Show the web interface footer.
 *
 * This function also writes the trailing 0-length chunk.
 */

static void
html_footer(ippeve_client_t *client)	/* I - Client */
{
  html_printf(client,
	      "</div>\n"
	      "</body>\n"
	      "</html>\n");
  httpWrite2(client->http, "", 0);
}


/*
 * 'html_header()' - Show the web interface header and title.
 */

static void
html_header(ippeve_client_t *client,	/* I - Client */
            const char    *title,	/* I - Title */
            int           refresh)	/* I - Refresh timer, if any */
{
  html_printf(client,
	      "<!doctype html>\n"
	      "<html>\n"
	      "<head>\n"
	      "<title>%s</title>\n"
	      "<link rel=\"shortcut icon\" href=\"/icon.png\" type=\"image/png\">\n"
	      "<link rel=\"apple-touch-icon\" href=\"/icon.png\" type=\"image/png\">\n"
	      "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=9\">\n", title);
  if (refresh > 0)
    html_printf(client, "<meta http-equiv=\"refresh\" content=\"%d\">\n", refresh);
  html_printf(client,
	      "<meta name=\"viewport\" content=\"width=device-width\">\n"
	      "<style>\n"
	      "body { font-family: sans-serif; margin: 0; }\n"
	      "div.body { padding: 0px 10px 10px; }\n"
	      "span.badge { background: #090; border-radius: 5px; color: #fff; padding: 5px 10px; }\n"
	      "span.bar { box-shadow: 0px 1px 5px #333; font-size: 75%%; }\n"
	      "table.form { border-collapse: collapse; margin-left: auto; margin-right: auto; margin-top: 10px; width: auto; }\n"
	      "table.form td, table.form th { padding: 5px 2px; }\n"
	      "table.form td.meter { border-right: solid 1px #ccc; padding: 0px; width: 400px; }\n"
	      "table.form th { text-align: right; }\n"
	      "table.striped { border-bottom: solid thin black; border-collapse: collapse; width: 100%%; }\n"
	      "table.striped tr:nth-child(even) { background: #fcfcfc; }\n"
	      "table.striped tr:nth-child(odd) { background: #f0f0f0; }\n"
	      "table.striped th { background: white; border-bottom: solid thin black; text-align: left; vertical-align: bottom; }\n"
	      "table.striped td { margin: 0; padding: 5px; vertical-align: top; }\n"
	      "table.nav { border-collapse: collapse; width: 100%%; }\n"
	      "table.nav td { margin: 0; text-align: center; }\n"
	      "td.nav a, td.nav a:active, td.nav a:hover, td.nav a:hover:link, td.nav a:hover:link:visited, td.nav a:link, td.nav a:link:visited, td.nav a:visited { background: inherit; color: inherit; font-size: 80%%; text-decoration: none; }\n"
	      "td.nav { background: #333; color: #fff; padding: 4px 8px; width: 33%%; }\n"
	      "td.nav.sel { background: #fff; color: #000; font-weight: bold; }\n"
	      "td.nav:hover { background: #666; color: #fff; }\n"
	      "td.nav:active { background: #000; color: #ff0; }\n"
	      "</style>\n"
	      "</head>\n"
	      "<body>\n"
	      "<table class=\"nav\"><tr>"
	      "<td class=\"nav%s\"><a href=\"/\">Status</a></td>"
	      "<td class=\"nav%s\"><a href=\"/supplies\">Supplies</a></td>"
	      "<td class=\"nav%s\"><a href=\"/media\">Media</a></td>"
	      "</tr></table>\n"
	      "<div class=\"body\">\n", !strcmp(client->uri, "/") ? " sel" : "", !strcmp(client->uri, "/supplies") ? " sel" : "", !strcmp(client->uri, "/media") ? " sel" : "");
}


/*
 * 'html_printf()' - Send formatted text to the client, quoting as needed.
 */

static void
html_printf(ippeve_client_t *client,	/* I - Client */
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
        httpWrite2(client->http, start, (size_t)(format - start));

      tptr    = tformat;
      *tptr++ = *format++;

      if (*format == '%')
      {
        httpWrite2(client->http, "%", 1);
        format ++;
	start = format;
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

	snprintf(tptr, sizeof(tformat) - (size_t)(tptr - tformat), "%d", width);
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

	  snprintf(tptr, sizeof(tformat) - (size_t)(tptr - tformat), "%d", prec);
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
	    if ((size_t)(width + 2) > sizeof(temp))
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
	    if ((size_t)(width + 2) > sizeof(temp))
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
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

	    sprintf(temp, tformat, va_arg(ap, void *));

            httpWrite2(client->http, temp, strlen(temp));
	    break;

        case 'c' : /* Character or character array */
            if (width <= 1)
            {
              temp[0] = (char)va_arg(ap, int);
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
    httpWrite2(client->http, start, (size_t)(format - start));

  va_end(ap);
}


/*
 * 'ipp_cancel_job()' - Cancel a job.
 */

static void
ipp_cancel_job(ippeve_client_t *client)	/* I - Client */
{
  ippeve_job_t		*job;		/* Job information */


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
 * 'ipp_close_job()' - Close an open job.
 */

static void
ipp_close_job(ippeve_client_t *client)	/* I - Client */
{
  ippeve_job_t		*job;		/* Job information */


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
		    "Job #%d is canceled - can\'t close.", job->id);
        break;

    case IPP_JSTATE_ABORTED :
	respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
		    "Job #%d is aborted - can\'t close.", job->id);
        break;

    case IPP_JSTATE_COMPLETED :
	respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
		    "Job #%d is completed - can\'t close.", job->id);
        break;

    case IPP_JSTATE_PROCESSING :
    case IPP_JSTATE_STOPPED :
	respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
		    "Job #%d is already closed.", job->id);
        break;

    default :
	respond_ipp(client, IPP_STATUS_OK, NULL);
        break;
  }
}


/*
 * 'ipp_create_job()' - Create a job object.
 */

static void
ipp_create_job(ippeve_client_t *client)	/* I - Client */
{
  ippeve_job_t		*job;		/* New job */
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
  cupsArrayAdd(ra, "job-state-message");
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
    ippeve_client_t *client)		/* I - Client */
{
  ippeve_job_t	*job;			/* Job */
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
ipp_get_jobs(ippeve_client_t *client)	/* I - Client */
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
  ippeve_job_t		*job;		/* Current job pointer */
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

    fprintf(stderr, "%s Get-Jobs first-job-id=%d", client->hostname, first_job_id);
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

    fprintf(stderr, "%s Get-Jobs my-jobs=%s\n", client->hostname, my_jobs ? "true" : "false");

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

      fprintf(stderr, "%s Get-Jobs requesting-user-name=\"%s\"\n", client->hostname, username);
    }
  }

 /*
  * OK, build a list of jobs for this printer...
  */

  ra = ippCreateRequestedArray(client->request);

  respond_ipp(client, IPP_STATUS_OK, NULL);

  _cupsRWLockRead(&(client->printer->rwlock));

  for (count = 0, job = (ippeve_job_t *)cupsArrayFirst(client->printer->jobs);
       (limit <= 0 || count < limit) && job;
       job = (ippeve_job_t *)cupsArrayNext(client->printer->jobs))
  {
   /*
    * Filter out jobs that don't match...
    */

    if ((job_comparison < 0 && job->state > job_state) ||
	(job_comparison == 0 && job->state != job_state) ||
	(job_comparison > 0 && job->state < job_state) ||
	job->id < first_job_id ||
	(username && job->username &&
	 strcasecmp(username, job->username)))
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
    ippeve_client_t *client)		/* I - Client */
{
  cups_array_t		*ra;		/* Requested attributes array */
  ippeve_printer_t	*printer;	/* Printer */


 /*
  * Send the attributes...
  */

  ra      = ippCreateRequestedArray(client->request);
  printer = client->printer;

  respond_ipp(client, IPP_STATUS_OK, NULL);

  _cupsRWLockRead(&(printer->rwlock));

  copy_attributes(client->response, printer->attrs, ra, IPP_TAG_ZERO,
		  IPP_TAG_CUPS_CONST);

  if (!ra || cupsArrayFind(ra, "printer-config-change-date-time"))
    ippAddDate(client->response, IPP_TAG_PRINTER, "printer-config-change-date-time", ippTimeToDate(printer->config_time));

  if (!ra || cupsArrayFind(ra, "printer-config-change-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-config-change-time", (int)(printer->config_time - printer->start_time));

  if (!ra || cupsArrayFind(ra, "printer-current-time"))
    ippAddDate(client->response, IPP_TAG_PRINTER, "printer-current-time", ippTimeToDate(time(NULL)));


  if (!ra || cupsArrayFind(ra, "printer-state"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state", (int)printer->state);

  if (!ra || cupsArrayFind(ra, "printer-state-change-date-time"))
    ippAddDate(client->response, IPP_TAG_PRINTER, "printer-state-change-date-time", ippTimeToDate(printer->state_time));

  if (!ra || cupsArrayFind(ra, "printer-state-change-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-state-change-time", (int)(printer->state_time - printer->start_time));

  if (!ra || cupsArrayFind(ra, "printer-state-message"))
  {
    static const char * const messages[] = { "Idle.", "Printing.", "Stopped." };

    ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_TEXT), "printer-state-message", NULL, messages[printer->state - IPP_PSTATE_IDLE]);
  }

  if (!ra || cupsArrayFind(ra, "printer-state-reasons"))
  {
    if (printer->state_reasons == IPPEVE_PREASON_NONE)
    {
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-state-reasons", NULL, "none");
    }
    else
    {
      ipp_attribute_t	*attr = NULL;		/* printer-state-reasons */
      ippeve_preason_t	bit;			/* Reason bit */
      int		i;			/* Looping var */
      char		reason[32];		/* Reason string */

      for (i = 0, bit = 1; i < (int)(sizeof(ippeve_preason_strings) / sizeof(ippeve_preason_strings[0])); i ++, bit *= 2)
      {
        if (printer->state_reasons & bit)
	{
	  snprintf(reason, sizeof(reason), "%s-%s", ippeve_preason_strings[i], printer->state == IPP_PSTATE_IDLE ? "report" : printer->state == IPP_PSTATE_PROCESSING ? "warning" : "error");
	  if (attr)
	    ippSetString(client->response, &attr, ippGetCount(attr), reason);
	  else
	    attr = ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "printer-state-reasons", NULL, reason);
	}
      }
    }
  }

  if (!ra || cupsArrayFind(ra, "printer-up-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-up-time", (int)(time(NULL) - printer->start_time));

  if (!ra || cupsArrayFind(ra, "queued-job-count"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "queued-job-count", printer->active_job && printer->active_job->state < IPP_JSTATE_CANCELED);

  _cupsRWUnlock(&(printer->rwlock));

  cupsArrayDelete(ra);
}


/*
 * 'ipp_identify_printer()' - Beep or display a message.
 */

static void
ipp_identify_printer(
    ippeve_client_t *client)		/* I - Client */
{
  ipp_attribute_t	*actions,	/* identify-actions */
			*message;	/* message */


  actions = ippFindAttribute(client->request, "identify-actions", IPP_TAG_KEYWORD);
  message = ippFindAttribute(client->request, "message", IPP_TAG_TEXT);

  if (!actions || ippContainsString(actions, "sound"))
  {
    putchar(0x07);
    fflush(stdout);
  }

  if (ippContainsString(actions, "display"))
    printf("IDENTIFY from %s: %s\n", client->hostname, message ? ippGetString(message, 0, NULL) : "No message supplied");

  respond_ipp(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_print_job()' - Create a job object with an attached document.
 */

static void
ipp_print_job(ippeve_client_t *client)	/* I - Client */
{
  ippeve_job_t		*job;		/* New job */


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
  * Create the job...
  */

  if ((job = create_job(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BUSY, "Currently printing another job.");
    return;
  }

 /*
  * Then finish getting the document data and process things...
  */

  finish_document_data(client, job);
}


/*
 * 'ipp_print_uri()' - Create a job object with a referenced document.
 */

static void
ipp_print_uri(ippeve_client_t *client)	/* I - Client */
{
  ippeve_job_t		*job;		/* New job */


 /*
  * Validate print job attributes...
  */

  if (!valid_job_attributes(client))
  {
    httpFlush(client->http);
    return;
  }

 /*
  * Create the job...
  */

  if ((job = create_job(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BUSY, "Currently printing another job.");
    return;
  }

 /*
  * Then finish getting the document data and process things...
  */

  finish_document_uri(client, job);
}


/*
 * 'ipp_send_document()' - Add an attached document to a job object created with
 *                         Create-Job.
 */

static void
ipp_send_document(
    ippeve_client_t *client)		/* I - Client */
{
  ippeve_job_t		*job;		/* Job information */
  ipp_attribute_t	*attr;		/* Current attribute */


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
    respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job is not in a pending state.");
    httpFlush(client->http);
    return;
  }
  else if (job->filename || job->fd >= 0)
  {
    respond_ipp(client, IPP_STATUS_ERROR_MULTIPLE_JOBS_NOT_SUPPORTED, "Multiple document jobs are not supported.");
    httpFlush(client->http);
    return;
  }

 /*
  * Make sure we have the "last-document" operation attribute...
  */

  if ((attr = ippFindAttribute(client->request, "last-document", IPP_TAG_ZERO)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required last-document attribute.");
    httpFlush(client->http);
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "The last-document attribute is not in the operation group.");
    httpFlush(client->http);
    return;
  }
  else if (ippGetValueTag(attr) != IPP_TAG_BOOLEAN || ippGetCount(attr) != 1 || !ippGetBoolean(attr, 0))
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
  * Then finish getting the document data and process things...
  */

  _cupsRWLockWrite(&(client->printer->rwlock));

  copy_attributes(job->attrs, client->request, NULL, IPP_TAG_JOB, 0);

  if ((attr = ippFindAttribute(job->attrs, "document-format-detected", IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else if ((attr = ippFindAttribute(job->attrs, "document-format-supplied", IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else
    job->format = "application/octet-stream";

  _cupsRWUnlock(&(client->printer->rwlock));

  finish_document_data(client, job);
}


/*
 * 'ipp_send_uri()' - Add a referenced document to a job object created with
 *                    Create-Job.
 */

static void
ipp_send_uri(ippeve_client_t *client)	/* I - Client */
{
  ippeve_job_t		*job;		/* Job information */
  ipp_attribute_t	*attr;		/* Current attribute */


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
    respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job is not in a pending state.");
    httpFlush(client->http);
    return;
  }
  else if (job->filename || job->fd >= 0)
  {
    respond_ipp(client, IPP_STATUS_ERROR_MULTIPLE_JOBS_NOT_SUPPORTED, "Multiple document jobs are not supported.");
    httpFlush(client->http);
    return;
  }

  if ((attr = ippFindAttribute(client->request, "last-document", IPP_TAG_ZERO)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required last-document attribute.");
    httpFlush(client->http);
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "The last-document attribute is not in the operation group.");
    httpFlush(client->http);
    return;
  }
  else if (ippGetValueTag(attr) != IPP_TAG_BOOLEAN || ippGetCount(attr) != 1 || !ippGetBoolean(attr, 0))
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
  * Then finish getting the document data and process things...
  */

  _cupsRWLockWrite(&(client->printer->rwlock));

  copy_attributes(job->attrs, client->request, NULL, IPP_TAG_JOB, 0);

  if ((attr = ippFindAttribute(job->attrs, "document-format-detected", IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else if ((attr = ippFindAttribute(job->attrs, "document-format-supplied", IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else
    job->format = "application/octet-stream";

  _cupsRWUnlock(&(client->printer->rwlock));

  finish_document_uri(client, job);
}


/*
 * 'ipp_validate_job()' - Validate job creation attributes.
 */

static void
ipp_validate_job(ippeve_client_t *client)	/* I - Client */
{
  if (valid_job_attributes(client))
    respond_ipp(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ippserver_attr_cb()' - Determine whether an attribute should be loaded.
 */

static int				/* O - 1 to use, 0 to ignore */
ippserver_attr_cb(
    _ipp_file_t    *f,			/* I - IPP file */
    void           *user_data,		/* I - User data pointer (unused) */
    const char     *attr)		/* I - Attribute name */
{
  int		i,			/* Current element */
		result;			/* Result of comparison */
  static const char * const ignored[] =
  {					/* Ignored attributes */
    "attributes-charset",
    "attributes-natural-language",
    "charset-configured",
    "charset-supported",
    "device-service-count",
    "device-uuid",
    "document-format-varying-attributes",
    "generated-natural-language-supported",
    "identify-actions-default",
    "identify-actions-supported",
    "ipp-features-supported",
    "ipp-versions-supproted",
    "ippget-event-life",
    "job-hold-until-supported",
    "job-hold-until-time-supported",
    "job-ids-supported",
    "job-k-octets-supported",
    "job-settable-attributes-supported",
    "multiple-document-jobs-supported",
    "multiple-operation-time-out",
    "multiple-operation-time-out-action",
    "natural-language-configured",
    "notify-attributes-supported",
    "notify-events-default",
    "notify-events-supported",
    "notify-lease-duration-default",
    "notify-lease-duration-supported",
    "notify-max-events-supported",
    "notify-pull-method-supported",
    "operations-supported",
    "printer-alert",
    "printer-alert-description",
    "printer-camera-image-uri",
    "printer-charge-info",
    "printer-charge-info-uri",
    "printer-config-change-date-time",
    "printer-config-change-time",
    "printer-current-time",
    "printer-detailed-status-messages",
    "printer-dns-sd-name",
    "printer-fax-log-uri",
    "printer-get-attributes-supported",
    "printer-icons",
    "printer-id",
    "printer-info",
    "printer-is-accepting-jobs",
    "printer-message-date-time",
    "printer-message-from-operator",
    "printer-message-time",
    "printer-more-info",
    "printer-service-type",
    "printer-settable-attributes-supported",
    "printer-state",
    "printer-state-message",
    "printer-state-reasons",
    "printer-static-resource-directory-uri",
    "printer-static-resource-k-octets-free",
    "printer-static-resource-k-octets-supported",
    "printer-strings-languages-supported",
    "printer-strings-uri",
    "printer-supply-info-uri",
    "printer-up-time",
    "printer-uri-supported",
    "printer-xri-supported",
    "queued-job-count",
    "reference-uri-scheme-supported",
    "uri-authentication-supported",
    "uri-security-supported",
    "which-jobs-supported",
    "xri-authentication-supported",
    "xri-security-supported",
    "xri-uri-scheme-supported"
  };


  (void)f;
  (void)user_data;

  for (i = 0, result = 1; i < (int)(sizeof(ignored) / sizeof(ignored[0])); i ++)
  {
    if ((result = strcmp(attr, ignored[i])) <= 0)
      break;
  }

  return (result != 0);
}


/*
 * 'ippserver_error_cb()' - Log an error message.
 */

static int				/* O - 1 to continue, 0 to stop */
ippserver_error_cb(
    _ipp_file_t    *f,			/* I - IPP file data */
    void           *user_data,		/* I - User data pointer (unused) */
    const char     *error)		/* I - Error message */
{
  (void)f;
  (void)user_data;

  _cupsLangPrintf(stderr, "%s\n", error);

  return (1);
}


/*
 * 'ippserver_token_cb()' - Process ippserver-specific config file tokens.
 */

static int				/* O - 1 to continue, 0 to stop */
ippserver_token_cb(
    _ipp_file_t    *f,			/* I - IPP file data */
    _ipp_vars_t    *vars,		/* I - IPP variables */
    void           *user_data,		/* I - User data pointer (unused) */
    const char     *token)		/* I - Current token */
{
  (void)vars;
  (void)user_data;

  if (!token)
  {
   /*
    * NULL token means do the initial setup - create an empty IPP message and
    * return...
    */

    f->attrs     = ippNew();
    f->group_tag = IPP_TAG_PRINTER;
  }
  else
  {
    _cupsLangPrintf(stderr, _("Unknown directive \"%s\" on line %d of \"%s\" ignored."), token, f->linenum, f->filename);
  }

  return (1);
}


/*
 * 'load_ippserver_attributes()' - Load IPP attributes from an ippserver file.
 */

static ipp_t *				/* O - IPP attributes or `NULL` on error */
load_ippserver_attributes(
    const char   *servername,		/* I - Server name or `NULL` for default */
    int          serverport,		/* I - Server port number */
    const char   *filename,		/* I - ippserver attribute filename */
    cups_array_t *docformats)		/* I - document-format-supported values */
{
  ipp_t		*attrs;			/* IPP attributes */
  _ipp_vars_t	vars;			/* IPP variables */
  char		temp[256];		/* Temporary string */


  (void)docformats; /* for now */

 /*
  * Setup callbacks and variables for the printer configuration file...
  *
  * The following additional variables are supported:
  *
  * - SERVERNAME: The host name of the server.
  * - SERVERPORT: The default port of the server.
  */

  _ippVarsInit(&vars, (_ipp_fattr_cb_t)ippserver_attr_cb, (_ipp_ferror_cb_t)ippserver_error_cb, (_ipp_ftoken_cb_t)ippserver_token_cb);

  if (servername)
  {
    _ippVarsSet(&vars, "SERVERNAME", servername);
  }
  else
  {
    httpGetHostname(NULL, temp, sizeof(temp));
    _ippVarsSet(&vars, "SERVERNAME", temp);
  }

  snprintf(temp, sizeof(temp), "%d", serverport);
  _ippVarsSet(&vars, "SERVERPORT", temp);

 /*
  * Load attributes and values for the printer...
  */

  attrs = _ippFileParse(&vars, filename, NULL);

 /*
  * Free memory and return...
  */

  _ippVarsDeinit(&vars);

  return (attrs);
}


/*
 * 'load_legacy_attributes()' - Load IPP attributes using the old ippserver
 *                              options.
 */

static ipp_t *				/* O - IPP attributes or `NULL` on error */
load_legacy_attributes(
    const char   *make,			/* I - Manufacturer name */
    const char   *model,		/* I - Model name */
    int          ppm,			/* I - pages-per-minute */
    int          ppm_color,		/* I - pages-per-minute-color */
    int          duplex,		/* I - Duplex support? */
    cups_array_t *docformats)		/* I - document-format-supported values */
{
  int			i;		/* Looping var */
  ipp_t			*attrs,		/* IPP attributes */
			*col;		/* Collection value */
  ipp_attribute_t	*attr;		/* Current attribute */
  char			device_id[1024],/* printer-device-id */
			*ptr,		/* Pointer into device ID */
			make_model[128];/* printer-make-and-model */
  const char		*format,	/* Current document format */
			*prefix;	/* Prefix for device ID */
  int			num_media;	/* Number of media */
  const char * const	*media;		/* List of media */
  int			num_ready;	/* Number of loaded media */
  const char * const	*ready;		/* List of loaded media */
  pwg_media_t		*pwg;		/* PWG media size information */
  static const char * const media_supported[] =
  {					/* media-supported values */
    "na_letter_8.5x11in",		/* Letter */
    "na_legal_8.5x14in",		/* Legal */
    "iso_a4_210x297mm",			/* A4 */
    "na_number-10_4.125x9.5in",		/* #10 Envelope */
    "iso_dl_110x220mm"			/* DL Envelope */
  };
  static const char * const media_supported_color[] =
  {					/* media-supported values */
    "na_letter_8.5x11in",		/* Letter */
    "na_legal_8.5x14in",		/* Legal */
    "iso_a4_210x297mm",			/* A4 */
    "na_number-10_4.125x9.5in",		/* #10 Envelope */
    "iso_dl_110x220mm",			/* DL Envelope */
    "na_index-3x5_3x5in",		/* Photo 3x5 */
    "oe_photo-l_3.5x5in",		/* Photo L */
    "na_index-4x6_4x6in",		/* Photo 4x6 */
    "iso_a6_105x148mm",			/* A6 */
    "na_5x7_5x7in"			/* Photo 5x7 aka 2L */
    "iso_a5_148x210mm",			/* A5 */
  };
  static const char * const media_ready[] =
  {					/* media-ready values */
    "na_letter_8.5x11in",		/* Letter */
    "na_number-10_4.125x9.5in"		/* #10 */
  };
  static const char * const media_ready_color[] =
  {					/* media-ready values */
    "na_letter_8.5x11in",		/* Letter */
    "na_index-4x6_4x6in"		/* Photo 4x6 */
  };
  static const char * const media_source_supported[] =
  {					/* media-source-supported values */
    "auto",
    "main",
    "manual",
    "by-pass-tray"			/* AKA multi-purpose tray */
  };
  static const char * const media_source_supported_color[] =
  {					/* media-source-supported values */
    "auto",
    "main",
    "photo"
  };
  static const char * const media_type_supported[] =
  {					/* media-type-supported values */
    "auto",
    "cardstock",
    "envelope",
    "labels",
    "other",
    "stationery",
    "stationery-letterhead",
    "transparency"
  };
  static const char * const media_type_supported_color[] =
  {					/* media-type-supported values */
    "auto",
    "cardstock",
    "envelope",
    "labels",
    "other",
    "stationery",
    "stationery-letterhead",
    "transparency",
    "photographic-glossy",
    "photographic-high-gloss",
    "photographic-matte",
    "photographic-satin",
    "photographic-semi-gloss"
  };
  static const int	media_bottom_margin_supported[] =
  {					/* media-bottom-margin-supported values */
    635					/* 1/4" */
  };
  static const int	media_bottom_margin_supported_color[] =
  {					/* media-bottom/top-margin-supported values */
    0,					/* Borderless */
    1168				/* 0.46" (common HP inkjet bottom margin) */
  };
  static const int	media_lr_margin_supported[] =
  {					/* media-left/right-margin-supported values */
    340,				/* 3.4mm (historical HP PCL A4 margin) */
    635					/* 1/4" */
  };
  static const int	media_lr_margin_supported_color[] =
  {					/* media-left/right-margin-supported values */
    0,					/* Borderless */
    340,				/* 3.4mm (historical HP PCL A4 margin) */
    635					/* 1/4" */
  };
  static const int	media_top_margin_supported[] =
  {					/* media-top-margin-supported values */
    635					/* 1/4" */
  };
  static const int	media_top_margin_supported_color[] =
  {					/* media-top/top-margin-supported values */
    0,					/* Borderless */
    102					/* 0.04" (common HP inkjet top margin */
  };
  static const int	orientation_requested_supported[4] =
  {					/* orientation-requested-supported values */
    IPP_ORIENT_PORTRAIT,
    IPP_ORIENT_LANDSCAPE,
    IPP_ORIENT_REVERSE_LANDSCAPE,
    IPP_ORIENT_REVERSE_PORTRAIT
  };
  static const char * const overrides_supported[] =
  {					/* overrides-supported values */
    "document-numbers",
    "media",
    "media-col",
    "orientation-requested",
    "pages"
  };
  static const char * const print_color_mode_supported[] =
  {					/* print-color-mode-supported values */
    "monochrome"
  };
  static const char * const print_color_mode_supported_color[] =
  {					/* print-color-mode-supported values */
    "auto",
    "color",
    "monochrome"
  };
  static const int	print_quality_supported[] =
  {					/* print-quality-supported values */
    IPP_QUALITY_DRAFT,
    IPP_QUALITY_NORMAL,
    IPP_QUALITY_HIGH
  };
  static const char * const printer_input_tray[] =
  {					/* printer-input-tray values */
    "type=sheetFeedAutoRemovableTray;mediafeed=0;mediaxfeed=0;maxcapacity=-2;level=-2;status=0;name=auto",
    "type=sheetFeedAutoRemovableTray;mediafeed=0;mediaxfeed=0;maxcapacity=250;level=100;status=0;name=main",
    "type=sheetFeedManual;mediafeed=0;mediaxfeed=0;maxcapacity=1;level=-2;status=0;name=manual",
    "type=sheetFeedAutoNonRemovableTray;mediafeed=0;mediaxfeed=0;maxcapacity=25;level=-2;status=0;name=by-pass-tray"
  };
  static const char * const printer_input_tray_color[] =
  {					/* printer-input-tray values */
    "type=sheetFeedAutoRemovableTray;mediafeed=0;mediaxfeed=0;maxcapacity=-2;level=-2;status=0;name=auto",
    "type=sheetFeedAutoRemovableTray;mediafeed=0;mediaxfeed=0;maxcapacity=250;level=-2;status=0;name=main",
    "type=sheetFeedAutoRemovableTray;mediafeed=0;mediaxfeed=0;maxcapacity=25;level=-2;status=0;name=photo"
  };
  static const char * const printer_supply[] =
  {					/* printer-supply values */
    "index=1;class=receptacleThatIsFilled;type=wasteToner;unit=percent;"
        "maxcapacity=100;level=25;colorantname=unknown;",
    "index=2;class=supplyThatIsConsumed;type=toner;unit=percent;"
        "maxcapacity=100;level=75;colorantname=black;"
  };
  static const char * const printer_supply_color[] =
  {					/* printer-supply values */
    "index=1;class=receptacleThatIsFilled;type=wasteInk;unit=percent;"
        "maxcapacity=100;level=25;colorantname=unknown;",
    "index=2;class=supplyThatIsConsumed;type=ink;unit=percent;"
        "maxcapacity=100;level=75;colorantname=black;",
    "index=3;class=supplyThatIsConsumed;type=ink;unit=percent;"
        "maxcapacity=100;level=50;colorantname=cyan;",
    "index=4;class=supplyThatIsConsumed;type=ink;unit=percent;"
        "maxcapacity=100;level=33;colorantname=magenta;",
    "index=5;class=supplyThatIsConsumed;type=ink;unit=percent;"
        "maxcapacity=100;level=67;colorantname=yellow;"
  };
  static const char * const printer_supply_description[] =
  {					/* printer-supply-description values */
    "Toner Waste Tank",
    "Black Toner"
  };
  static const char * const printer_supply_description_color[] =
  {					/* printer-supply-description values */
    "Ink Waste Tank",
    "Black Ink",
    "Cyan Ink",
    "Magenta Ink",
    "Yellow Ink"
  };
  static const int	pwg_raster_document_resolution_supported[] =
  {
    300,
    600
  };
  static const char * const pwg_raster_document_type_supported[] =
  {
    "black_1",
    "sgray_8"
  };
  static const char * const pwg_raster_document_type_supported_color[] =
  {
    "black_1",
    "sgray_8",
    "srgb_8",
    "srgb_16"
  };
  static const char * const sides_supported[] =
  {					/* sides-supported values */
    "one-sided",
    "two-sided-long-edge",
    "two-sided-short-edge"
  };
  static const char * const urf_supported[] =
  {					/* urf-supported values */
    "CP1",
    "IS1-4-5-19",
    "MT1-2-3-4-5-6",
    "RS600",
    "V1.4",
    "W8"
  };
  static const char * const urf_supported_color[] =
  {					/* urf-supported values */
    "CP1",
    "IS1-4-5-7-19",
    "MT1-2-3-4-5-6-8-9-10-11-12-13",
    "RS600",
    "SRGB24",
    "V1.4",
    "W8"
  };
  static const char * const urf_supported_color_duplex[] =
  {					/* urf-supported values */
    "CP1",
    "IS1-4-5-7-19",
    "MT1-2-3-4-5-6-8-9-10-11-12-13",
    "RS600",
    "SRGB24",
    "V1.4",
    "W8",
    "DM3"
  };
  static const char * const urf_supported_duplex[] =
  {					/* urf-supported values */
    "CP1",
    "IS1-4-5-19",
    "MT1-2-3-4-5-6",
    "RS600",
    "V1.4",
    "W8",
    "DM1"
  };


  attrs = ippNew();

  if (ppm_color > 0)
  {
    num_media = (int)(sizeof(media_supported_color) / sizeof(media_supported_color[0]));
    media     = media_supported_color;
    num_ready = (int)(sizeof(media_ready_color) / sizeof(media_ready_color[0]));
    ready     = media_ready_color;
  }
  else
  {
    num_media = (int)(sizeof(media_supported) / sizeof(media_supported[0]));
    media     = media_supported;
    num_ready = (int)(sizeof(media_ready) / sizeof(media_ready[0]));
    ready     = media_ready;
  }

  /* color-supported */
  ippAddBoolean(attrs, IPP_TAG_PRINTER, "color-supported", ppm_color > 0);

  /* copies-default */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "copies-default", 1);

  /* copies-supported */
  ippAddRange(attrs, IPP_TAG_PRINTER, "copies-supported", 1, (cupsArrayFind(docformats, (void *)"application/pdf") != NULL || cupsArrayFind(docformats, (void *)"image/jpeg") != NULL) ? 999 : 1);

  /* document-password-supported */
  if (cupsArrayFind(docformats, (void *)"application/pdf"))
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "document-password-supported", 1023);

  /* finishings-default */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "finishings-default", IPP_FINISHINGS_NONE);

  /* finishings-supported */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "finishings-supported", IPP_FINISHINGS_NONE);

  /* media-bottom-margin-supported */
  if (ppm_color > 0)
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-bottom-margin-supported", (int)(sizeof(media_bottom_margin_supported) / sizeof(media_bottom_margin_supported[0])), media_bottom_margin_supported);
  else
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-bottom-margin-supported", (int)(sizeof(media_bottom_margin_supported_color) / sizeof(media_bottom_margin_supported_color[0])), media_bottom_margin_supported_color);

  /* media-col-database and media-col-default */
  attr = ippAddCollections(attrs, IPP_TAG_PRINTER, "media-col-database", num_media, NULL);
  for (i = 0; i < num_media; i ++)
  {
    int		bottom, left,		/* media-xxx-margins */
		right, top;
    const char	*source;		/* media-source, if any */

    pwg = pwgMediaForPWG(media[i]);

    if (pwg->width < 21000 && pwg->length < 21000)
    {
      source = "photo";			/* Photo size media from photo tray */
      bottom =				/* Borderless margins */
      left   =
      right  =
      top    = 0;
    }
    else if (pwg->width < 21000)
    {
      source = "by-pass-tray";		/* Envelopes from multi-purpose tray */
      bottom = ppm_color > 0 ? media_bottom_margin_supported_color[1] : media_bottom_margin_supported[0];
      left   =				/* Left/right margins are standard */
      right  = media_lr_margin_supported[1];
      top    = ppm_color > 0 ? media_top_margin_supported_color[1] : media_top_margin_supported[0];
    }
    else if (pwg->width == 21000)
    {
      source = NULL;			/* A4 from any tray */
      bottom = ppm_color > 0 ? media_bottom_margin_supported_color[1] : media_bottom_margin_supported[0];
      left   =				/* Left/right margins are reduced */
      right  = media_lr_margin_supported[0];
      top    = ppm_color > 0 ? media_top_margin_supported_color[1] : media_top_margin_supported[0];
    }
    else
    {
      source = NULL;			/* Other size media from any tray */
      bottom = ppm_color > 0 ? media_bottom_margin_supported_color[1] : media_bottom_margin_supported[0];
      left   =				/* Left/right margins are standard */
      right  = media_lr_margin_supported[1];
      top    = ppm_color > 0 ? media_top_margin_supported_color[1] : media_top_margin_supported[0];
    }

    col = create_media_col(media[i], source, NULL, pwg->width, pwg->length, bottom, left, right, top);
    ippSetCollection(attrs, &attr, i, col);

    ippDelete(col);
  }

  /* media-col-default */
  pwg = pwgMediaForPWG(ready[0]);

  if (pwg->width == 21000)
    col = create_media_col(ready[0], "main", "stationery", pwg->width, pwg->length, ppm_color > 0 ? media_bottom_margin_supported_color[1] : media_bottom_margin_supported[0], media_lr_margin_supported[0], media_lr_margin_supported[0], ppm_color > 0 ? media_top_margin_supported_color[1] : media_top_margin_supported[0]);
  else
    col = create_media_col(ready[0], "main", "stationery", pwg->width, pwg->length, ppm_color > 0 ? media_bottom_margin_supported_color[1] : media_bottom_margin_supported[0], media_lr_margin_supported[1], media_lr_margin_supported[1], ppm_color > 0 ? media_top_margin_supported_color[1] : media_top_margin_supported[0]);

  ippAddCollection(attrs, IPP_TAG_PRINTER, "media-col-default", col);

  ippDelete(col);

  /* media-col-ready */
  attr = ippAddCollections(attrs, IPP_TAG_PRINTER, "media-col-ready", num_ready, NULL);
  for (i = 0; i < num_ready; i ++)
  {
    int		bottom, left,		/* media-xxx-margins */
		right, top;
    const char	*source,		/* media-source */
		*type;			/* media-type */

    pwg = pwgMediaForPWG(ready[i]);

    if (pwg->width < 21000 && pwg->length < 21000)
    {
      source = "photo";			/* Photo size media from photo tray */
      type   = "photographic-glossy";	/* Glossy photo paper */
      bottom =				/* Borderless margins */
      left   =
      right  =
      top    = 0;
    }
    else if (pwg->width < 21000)
    {
      source = "by-pass-tray";		/* Envelopes from multi-purpose tray */
      type   = "envelope";		/* Envelope */
      bottom = ppm_color > 0 ? media_bottom_margin_supported_color[1] : media_bottom_margin_supported[0];
      left   =				/* Left/right margins are standard */
      right  = media_lr_margin_supported[1];
      top    = ppm_color > 0 ? media_top_margin_supported_color[1] : media_top_margin_supported[0];
    }
    else if (pwg->width == 21000)
    {
      source = "main";			/* A4 from main tray */
      type   = "stationery";		/* Plain paper */
      bottom = ppm_color > 0 ? media_bottom_margin_supported_color[1] : media_bottom_margin_supported[0];
      left   =				/* Left/right margins are reduced */
      right  = media_lr_margin_supported[0];
      top    = ppm_color > 0 ? media_top_margin_supported_color[1] : media_top_margin_supported[0];
    }
    else
    {
      source = "main";			/* A4 from main tray */
      type   = "stationery";		/* Plain paper */
      bottom = ppm_color > 0 ? media_bottom_margin_supported_color[1] : media_bottom_margin_supported[0];
      left   =				/* Left/right margins are standard */
      right  = media_lr_margin_supported[1];
      top    = ppm_color > 0 ? media_top_margin_supported_color[1] : media_top_margin_supported[0];
    }

    col = create_media_col(ready[i], source, type, pwg->width, pwg->length, bottom, left, right, top);
    ippSetCollection(attrs, &attr, i, col);
    ippDelete(col);
  }

  /* media-default */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-default", NULL, media[0]);

  /* media-left/right-margin-supported */
  if (ppm_color > 0)
  {
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-left-margin-supported", (int)(sizeof(media_lr_margin_supported_color) / sizeof(media_lr_margin_supported_color[0])), media_lr_margin_supported_color);
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-right-margin-supported", (int)(sizeof(media_lr_margin_supported_color) / sizeof(media_lr_margin_supported_color[0])), media_lr_margin_supported_color);
  }
  else
  {
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-left-margin-supported", (int)(sizeof(media_lr_margin_supported) / sizeof(media_lr_margin_supported[0])), media_lr_margin_supported);
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-right-margin-supported", (int)(sizeof(media_lr_margin_supported) / sizeof(media_lr_margin_supported[0])), media_lr_margin_supported);
  }

  /* media-ready */
  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-ready", num_ready, NULL, ready);

  /* media-supported */
  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-supported", num_media, NULL, media);

  /* media-size-supported */
  attr = ippAddCollections(attrs, IPP_TAG_PRINTER, "media-size-supported", num_media, NULL);
  for (i = 0; i < num_media; i ++)
  {
    pwg = pwgMediaForPWG(media[i]);
    col = create_media_size(pwg->width, pwg->length);

    ippSetCollection(attrs, &attr, i, col);
    ippDelete(col);
  }

  /* media-source-supported */
  if (ppm_color > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-source-supported", (int)(sizeof(media_source_supported_color) / sizeof(media_source_supported_color[0])), NULL, media_source_supported_color);
  else
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-source-supported", (int)(sizeof(media_source_supported) / sizeof(media_source_supported[0])), NULL, media_source_supported);

  /* media-top-margin-supported */
  if (ppm_color > 0)
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-top-margin-supported", (int)(sizeof(media_top_margin_supported) / sizeof(media_top_margin_supported[0])), media_top_margin_supported);
  else
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-top-margin-supported", (int)(sizeof(media_top_margin_supported_color) / sizeof(media_top_margin_supported_color[0])), media_top_margin_supported_color);

  /* media-type-supported */
  if (ppm_color > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-type-supported", (int)(sizeof(media_type_supported_color) / sizeof(media_type_supported_color[0])), NULL, media_type_supported_color);
  else
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-type-supported", (int)(sizeof(media_type_supported) / sizeof(media_type_supported[0])), NULL, media_type_supported);

  /* orientation-requested-default */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-default", IPP_ORIENT_PORTRAIT);

  /* orientation-requested-supported */
  if (cupsArrayFind(docformats, (void *)"application/pdf") || cupsArrayFind(docformats, (void *)"image/jpeg"))
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-supported", (int)(sizeof(orientation_requested_supported) / sizeof(orientation_requested_supported[0])), orientation_requested_supported);
  else
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-supported", IPP_ORIENT_PORTRAIT);

  /* output-bin-default */
  if (ppm_color > 0)
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-default", NULL, "face-up");
  else
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-default", NULL, "face-down");

  /* output-bin-supported */
  if (ppm_color > 0)
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-supported", NULL, "face-up");
  else
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-supported", NULL, "face-down");

  /* overrides-supported */
  if (cupsArrayFind(docformats, (void *)"application/pdf"))
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "overrides-supported", (int)(sizeof(overrides_supported) / sizeof(overrides_supported[0])), NULL, overrides_supported);

  /* page-ranges-supported */
  ippAddBoolean(attrs, IPP_TAG_PRINTER, "page-ranges-supported", cupsArrayFind(docformats, (void *)"application/pdf") != NULL);

  /* pages-per-minute */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "pages-per-minute", ppm);

  /* pages-per-minute-color */
  if (ppm_color > 0)
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "pages-per-minute-color", ppm_color);

  /* print-color-mode-default */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-default", NULL, ppm_color > 0 ? "auto" : "monochrome");

  /* print-color-mode-supported */
  if (ppm_color > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-supported", (int)(sizeof(print_color_mode_supported_color) / sizeof(print_color_mode_supported_color[0])), NULL, print_color_mode_supported_color);
  else
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-supported", (int)(sizeof(print_color_mode_supported) / sizeof(print_color_mode_supported[0])), NULL, print_color_mode_supported);

  /* print-content-optimize-default */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-content-optimize-default", NULL, "auto");

  /* print-content-optimize-supported */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-content-optimize-supported", NULL, "auto");

  /* print-quality-default */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-default", IPP_QUALITY_NORMAL);

  /* print-quality-supported */
  ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-supported", (int)(sizeof(print_quality_supported) / sizeof(print_quality_supported[0])), print_quality_supported);

  /* print-rendering-intent-default */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-rendering-intent-default", NULL, "auto");

  /* print-rendering-intent-supported */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-rendering-intent-supported", NULL, "auto");

  /* printer-device-id */
  snprintf(device_id, sizeof(device_id), "MFG:%s;MDL:%s;", make, model);
  ptr    = device_id + strlen(device_id);
  prefix = "CMD:";
  for (format = (const char *)cupsArrayFirst(docformats); format; format = (const char *)cupsArrayNext(docformats))
  {
    if (!strcasecmp(format, "application/pdf"))
      snprintf(ptr, sizeof(device_id) - (size_t)(ptr - device_id), "%sPDF", prefix);
    else if (!strcasecmp(format, "application/postscript"))
      snprintf(ptr, sizeof(device_id) - (size_t)(ptr - device_id), "%sPS", prefix);
    else if (!strcasecmp(format, "application/vnd.hp-PCL"))
      snprintf(ptr, sizeof(device_id) - (size_t)(ptr - device_id), "%sPCL", prefix);
    else if (!strcasecmp(format, "image/jpeg"))
      snprintf(ptr, sizeof(device_id) - (size_t)(ptr - device_id), "%sJPEG", prefix);
    else if (!strcasecmp(format, "image/png"))
      snprintf(ptr, sizeof(device_id) - (size_t)(ptr - device_id), "%sPNG", prefix);
    else if (!strcasecmp(format, "image/pwg-raster"))
      snprintf(ptr, sizeof(device_id) - (size_t)(ptr - device_id), "%sPWG", prefix);
    else if (!strcasecmp(format, "image/urf"))
      snprintf(ptr, sizeof(device_id) - (size_t)(ptr - device_id), "%sURF", prefix);
    else
      continue;

    ptr += strlen(ptr);
    prefix = ",";
  }
  if (ptr < (device_id + sizeof(device_id) - 1))
  {
    *ptr++ = ';';
    *ptr = '\0';
  }
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-device-id", NULL, device_id);

  /* printer-input-tray */
  if (ppm_color > 0)
  {
    attr = ippAddOctetString(attrs, IPP_TAG_PRINTER, "printer-input-tray", printer_input_tray_color[0], (int)strlen(printer_input_tray_color[0]));
    for (i = 1; i < (int)(sizeof(printer_input_tray_color) / sizeof(printer_input_tray_color[0])); i ++)
      ippSetOctetString(attrs, &attr, i, printer_input_tray_color[i], (int)strlen(printer_input_tray_color[i]));
  }
  else
  {
    attr = ippAddOctetString(attrs, IPP_TAG_PRINTER, "printer-input-tray", printer_input_tray[0], (int)strlen(printer_input_tray[0]));
    for (i = 1; i < (int)(sizeof(printer_input_tray) / sizeof(printer_input_tray[0])); i ++)
      ippSetOctetString(attrs, &attr, i, printer_input_tray[i], (int)strlen(printer_input_tray[i]));
  }

  /* printer-make-and-model */
  snprintf(make_model, sizeof(make_model), "%s %s", make, model);
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-make-and-model", NULL, make_model);

  /* printer-resolution-default */
  ippAddResolution(attrs, IPP_TAG_PRINTER, "printer-resolution-default", IPP_RES_PER_INCH, 600, 600);

  /* printer-resolution-supported */
  ippAddResolution(attrs, IPP_TAG_PRINTER, "printer-resolution-supported", IPP_RES_PER_INCH, 600, 600);

  /* printer-supply and printer-supply-description */
  if (ppm_color > 0)
  {
    attr = ippAddOctetString(attrs, IPP_TAG_PRINTER, "printer-supply", printer_supply_color[0], (int)strlen(printer_supply_color[0]));
    for (i = 1; i < (int)(sizeof(printer_supply_color) / sizeof(printer_supply_color[0])); i ++)
      ippSetOctetString(attrs, &attr, i, printer_supply_color[i], (int)strlen(printer_supply_color[i]));

    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_TEXT), "printer-supply-description", (int)(sizeof(printer_supply_description_color) / sizeof(printer_supply_description_color[0])), NULL, printer_supply_description_color);
  }
  else
  {
    attr = ippAddOctetString(attrs, IPP_TAG_PRINTER, "printer-supply", printer_supply[0], (int)strlen(printer_supply[0]));
    for (i = 1; i < (int)(sizeof(printer_supply) / sizeof(printer_supply[0])); i ++)
      ippSetOctetString(attrs, &attr, i, printer_supply[i], (int)strlen(printer_supply[i]));

    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_TEXT), "printer-supply-description", (int)(sizeof(printer_supply_description) / sizeof(printer_supply_description[0])), NULL, printer_supply_description);
  }

  /* pwg-raster-document-xxx-supported */
  if (cupsArrayFind(docformats, (void *)"image/pwg-raster"))
  {
    ippAddResolutions(attrs, IPP_TAG_PRINTER, "pwg-raster-document-resolution-supported", (int)(sizeof(pwg_raster_document_resolution_supported) / sizeof(pwg_raster_document_resolution_supported[0])), IPP_RES_PER_INCH, pwg_raster_document_resolution_supported, pwg_raster_document_resolution_supported);

    if (ppm_color > 0 && duplex)
      ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pwg-raster-document-sheet-back", NULL, "rotated");
    else if (duplex)
      ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pwg-raster-document-sheet-back", NULL, "normal");

    if (ppm_color > 0)
      ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pwg-raster-document-type-supported", (int)(sizeof(pwg_raster_document_type_supported_color) / sizeof(pwg_raster_document_type_supported_color[0])), NULL, pwg_raster_document_type_supported_color);
    else
      ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pwg-raster-document-type-supported", (int)(sizeof(pwg_raster_document_type_supported) / sizeof(pwg_raster_document_type_supported[0])), NULL, pwg_raster_document_type_supported);
  }

  /* sides-default */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "sides-default", NULL, "one-sided");

  /* sides-supported */
  if (duplex)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "sides-supported", (int)(sizeof(sides_supported) / sizeof(sides_supported[0])), NULL, sides_supported);
  else
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "sides-supported", NULL, "one-sided");

  /* urf-supported */
  if (cupsArrayFind(docformats, (void *)"image/urf"))
  {
    if (ppm_color > 0)
    {
      if (duplex)
	ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "urf-supported", (int)(sizeof(urf_supported_color_duplex) / sizeof(urf_supported_color_duplex[0])), NULL, urf_supported_color_duplex);
      else
	ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "urf-supported", (int)(sizeof(urf_supported_color) / sizeof(urf_supported_color[0])), NULL, urf_supported_color);
    }
    else if (duplex)
    {
      ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "urf-supported", (int)(sizeof(urf_supported_duplex) / sizeof(urf_supported_duplex[0])), NULL, urf_supported_duplex);
    }
    else
    {
      ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "urf-supported", (int)(sizeof(urf_supported) / sizeof(urf_supported[0])), NULL, urf_supported);
    }
  }

  return (attrs);
}


#if !CUPS_LITE
/*
 * 'load_ppd_attributes()' - Load IPP attributes from a PPD file.
 */

static ipp_t *				/* O - IPP attributes or `NULL` on error */
load_ppd_attributes(
    const char   *ppdfile,		/* I - PPD filename */
    cups_array_t *docformats)		/* I - document-format-supported values */
{
  int		i, j;			/* Looping vars */
  ipp_t		*attrs;			/* Attributes */
  ipp_attribute_t *attr;		/* Current attribute */
  ipp_t		*col;			/* Current collection value */
  ppd_file_t	*ppd;			/* PPD data */
  ppd_attr_t	*ppd_attr;		/* PPD attribute */
  ppd_choice_t	*ppd_choice;		/* PPD choice */
  ppd_size_t	*ppd_size;		/* Default PPD size */
  pwg_size_t	*pwg_size,		/* Current PWG size */
		*default_size = NULL;	/* Default PWG size */
  const char	*default_source = NULL,	/* Default media source */
		*default_type = NULL;	/* Default media type */
  pwg_map_t	*pwg_map;		/* Mapping from PWG to PPD keywords */
  _ppd_cache_t	*pc;			/* PPD cache */
  _pwg_finishings_t *finishings;	/* Current finishings value */
  const char	*template;		/* Current finishings-template value */
  int		num_margins;		/* Number of media-xxx-margin-supported values */
  int		margins[10];		/* media-xxx-margin-supported values */
  int		xres,			/* Default horizontal resolution */
		yres;			/* Default vertical resolution */
  int		num_urf;		/* Number of urf-supported values */
  const char	*urf[10];		/* urf-supported values */
  char		urf_rs[32];		/* RS value */
  static const int	orientation_requested_supported[4] =
  {					/* orientation-requested-supported values */
    IPP_ORIENT_PORTRAIT,
    IPP_ORIENT_LANDSCAPE,
    IPP_ORIENT_REVERSE_LANDSCAPE,
    IPP_ORIENT_REVERSE_PORTRAIT
  };
  static const char * const overrides_supported[] =
  {					/* overrides-supported */
    "document-numbers",
    "media",
    "media-col",
    "orientation-requested",
    "pages"
  };
  static const char * const print_color_mode_supported[] =
  {					/* print-color-mode-supported values */
    "monochrome"
  };
  static const char * const print_color_mode_supported_color[] =
  {					/* print-color-mode-supported values */
    "auto",
    "color",
    "monochrome"
  };
  static const int	print_quality_supported[] =
  {					/* print-quality-supported values */
    IPP_QUALITY_DRAFT,
    IPP_QUALITY_NORMAL,
    IPP_QUALITY_HIGH
  };
  static const char * const printer_supply[] =
  {					/* printer-supply values */
    "index=1;class=receptacleThatIsFilled;type=wasteToner;unit=percent;"
        "maxcapacity=100;level=25;colorantname=unknown;",
    "index=2;class=supplyThatIsConsumed;type=toner;unit=percent;"
        "maxcapacity=100;level=75;colorantname=black;"
  };
  static const char * const printer_supply_color[] =
  {					/* printer-supply values */
    "index=1;class=receptacleThatIsFilled;type=wasteInk;unit=percent;"
        "maxcapacity=100;level=25;colorantname=unknown;",
    "index=2;class=supplyThatIsConsumed;type=ink;unit=percent;"
        "maxcapacity=100;level=75;colorantname=black;",
    "index=3;class=supplyThatIsConsumed;type=ink;unit=percent;"
        "maxcapacity=100;level=50;colorantname=cyan;",
    "index=4;class=supplyThatIsConsumed;type=ink;unit=percent;"
        "maxcapacity=100;level=33;colorantname=magenta;",
    "index=5;class=supplyThatIsConsumed;type=ink;unit=percent;"
        "maxcapacity=100;level=67;colorantname=yellow;"
  };
  static const char * const printer_supply_description[] =
  {					/* printer-supply-description values */
    "Toner Waste Tank",
    "Black Toner"
  };
  static const char * const printer_supply_description_color[] =
  {					/* printer-supply-description values */
    "Ink Waste Tank",
    "Black Ink",
    "Cyan Ink",
    "Magenta Ink",
    "Yellow Ink"
  };
  static const char * const pwg_raster_document_type_supported[] =
  {
    "black_1",
    "sgray_8"
  };
  static const char * const pwg_raster_document_type_supported_color[] =
  {
    "black_1",
    "sgray_8",
    "srgb_8",
    "srgb_16"
  };
  static const char * const sides_supported[] =
  {					/* sides-supported values */
    "one-sided",
    "two-sided-long-edge",
    "two-sided-short-edge"
  };


 /*
  * Open the PPD file...
  */

  if ((ppd = ppdOpenFile(ppdfile)) == NULL)
  {
    ppd_status_t	status;		/* Load error */

    status = ppdLastError(&i);
    _cupsLangPrintf(stderr, _("ippeveprinter: Unable to open \"%s\": %s on line %d."), ppdfile, ppdErrorString(status), i);
    return (NULL);
  }

  ppdMarkDefaults(ppd);

  pc = _ppdCacheCreateWithPPD(ppd);

  if ((ppd_size = ppdPageSize(ppd, NULL)) != NULL)
  {
   /*
    * Look up default size...
    */

    for (i = 0, pwg_size = pc->sizes; i < pc->num_sizes; i ++, pwg_size ++)
    {
      if (!strcmp(pwg_size->map.ppd, ppd_size->name))
      {
        default_size = pwg_size;
        break;
      }
    }
  }

  if (!default_size)
  {
   /*
    * Default to A4 or Letter...
    */

    for (i = 0, pwg_size = pc->sizes; i < pc->num_sizes; i ++, pwg_size ++)
    {
      if (!strcmp(pwg_size->map.ppd, "Letter") || !strcmp(pwg_size->map.ppd, "A4"))
      {
        default_size = pwg_size;
        break;
      }
    }

    if (!default_size)
      default_size = pc->sizes;		/* Last resort: first size */
  }

  if ((ppd_choice = ppdFindMarkedChoice(ppd, "InputSlot")) != NULL)
    default_source = _ppdCacheGetSource(pc, ppd_choice->choice);

  if ((ppd_choice = ppdFindMarkedChoice(ppd, "MediaType")) != NULL)
    default_source = _ppdCacheGetType(pc, ppd_choice->choice);

  if ((ppd_attr = ppdFindAttr(ppd, "DefaultResolution", NULL)) != NULL)
  {
   /*
    * Use the PPD-defined default resolution...
    */

    if ((i = sscanf(ppd_attr->value, "%dx%d", &xres, &yres)) == 1)
      yres = xres;
    else if (i < 0)
      xres = yres = 300;
  }
  else
  {
   /*
    * Use default of 300dpi...
    */

    xres = yres = 300;
  }

  snprintf(urf_rs, sizeof(urf_rs), "RS%d", yres < xres ? yres : xres);

  num_urf = 0;
  urf[num_urf ++] = "V1.4";
  urf[num_urf ++] = "CP1";
  urf[num_urf ++] = urf_rs;
  urf[num_urf ++] = "W8";
  if (pc->sides_2sided_long)
    urf[num_urf ++] = "DM1";
  if (ppd->color_device)
    urf[num_urf ++] = "SRGB24";

 /*
  * PostScript printers accept PDF via one of the CUPS PDF to PostScript
  * filters, along with PostScript (of course) and JPEG...
  */

  cupsArrayAdd(docformats, "application/pdf");
  cupsArrayAdd(docformats, "application/postscript");
  cupsArrayAdd(docformats, "image/jpeg");

 /*
  * Create the attributes...
  */

  attrs = ippNew();

  /* color-supported */
  ippAddBoolean(attrs, IPP_TAG_PRINTER, "color-supported", (char)ppd->color_device);

  /* copies-default */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "copies-default", 1);

  /* copies-supported */
  ippAddRange(attrs, IPP_TAG_PRINTER, "copies-supported", 1, 999);

  /* document-password-supported */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "document-password-supported", 127);

  /* finishing-template-supported */
  attr = ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "finishing-template-supported", cupsArrayCount(pc->templates) + 1, NULL, NULL);
  ippSetString(attrs, &attr, 0, "none");
  for (i = 1, template = (const char *)cupsArrayFirst(pc->templates); template; i ++, template = (const char *)cupsArrayNext(pc->templates))
    ippSetString(attrs, &attr, i, template);

  /* finishings-col-database */
  attr = ippAddCollections(attrs, IPP_TAG_PRINTER, "finishings-col-database", cupsArrayCount(pc->templates) + 1, NULL);

  col = ippNew();
  ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "finishing-template", NULL, "none");
  ippSetCollection(attrs, &attr, 0, col);
  ippDelete(col);

  for (i = 1, template = (const char *)cupsArrayFirst(pc->templates); template; i ++, template = (const char *)cupsArrayNext(pc->templates))
  {
    col = ippNew();
    ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "finishing-template", NULL, template);
    ippSetCollection(attrs, &attr, i, col);
    ippDelete(col);
  }

  /* finishings-col-default */
  col = ippNew();
  ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "finishing-template", NULL, "none");
  ippAddCollection(attrs, IPP_TAG_PRINTER, "finishings-col-default", col);
  ippDelete(col);

  /* finishings-col-ready */
  attr = ippAddCollections(attrs, IPP_TAG_PRINTER, "finishings-col-ready", cupsArrayCount(pc->templates) + 1, NULL);

  col = ippNew();
  ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "finishing-template", NULL, "none");
  ippSetCollection(attrs, &attr, 0, col);
  ippDelete(col);

  for (i = 1, template = (const char *)cupsArrayFirst(pc->templates); template; i ++, template = (const char *)cupsArrayNext(pc->templates))
  {
    col = ippNew();
    ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "finishing-template", NULL, template);
    ippSetCollection(attrs, &attr, i, col);
    ippDelete(col);
  }

  /* finishings-col-supported */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "finishings-col-supported", NULL, "finishing-template");

  /* finishings-default */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "finishings-default", IPP_FINISHINGS_NONE);

  /* finishings-ready */
  attr = ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "finishings-ready", cupsArrayCount(pc->finishings) + 1, NULL);
  ippSetInteger(attrs, &attr, 0, IPP_FINISHINGS_NONE);
  for (i = 1, finishings = (_pwg_finishings_t *)cupsArrayFirst(pc->finishings); finishings; i ++, finishings = (_pwg_finishings_t *)cupsArrayNext(pc->finishings))
    ippSetInteger(attrs, &attr, i, (int)finishings->value);

  /* finishings-supported */
  attr = ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "finishings-supported", cupsArrayCount(pc->finishings) + 1, NULL);
  ippSetInteger(attrs, &attr, 0, IPP_FINISHINGS_NONE);
  for (i = 1, finishings = (_pwg_finishings_t *)cupsArrayFirst(pc->finishings); finishings; i ++, finishings = (_pwg_finishings_t *)cupsArrayNext(pc->finishings))
    ippSetInteger(attrs, &attr, i, (int)finishings->value);

  /* media-bottom-margin-supported */
  for (i = 0, num_margins = 0, pwg_size = pc->sizes; i < pc->num_sizes && num_margins < (int)(sizeof(margins) / sizeof(margins[0])); i ++, pwg_size ++)
  {
    for (j = 0; j < num_margins; j ++)
    {
      if (margins[j] == pwg_size->bottom)
        break;
    }

    if (j >= num_margins)
      margins[num_margins ++] = pwg_size->bottom;
  }

  for (i = 0; i < (num_margins - 1); i ++)
  {
    for (j = i + 1; j < num_margins; j ++)
    {
      if (margins[i] > margins[j])
      {
        int mtemp = margins[i];

        margins[i] = margins[j];
        margins[j] = mtemp;
      }
    }
  }

  ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-bottom-margin-supported", num_margins, margins);

  /* media-col-database */
  attr = ippAddCollections(attrs, IPP_TAG_PRINTER, "media-col-database", pc->num_sizes, NULL);
  for (i = 0, pwg_size = pc->sizes; i < pc->num_sizes; i ++, pwg_size ++)
  {
    col = create_media_col(pwg_size->map.pwg, NULL, NULL, pwg_size->width, pwg_size->length, pwg_size->bottom, pwg_size->left, pwg_size->right, pwg_size->top);
    ippSetCollection(attrs, &attr, i, col);
    ippDelete(col);
  }

  /* media-col-default */
  col = create_media_col(default_size->map.pwg, default_source, default_type, default_size->width, default_size->length, default_size->bottom, default_size->left, default_size->right, default_size->top);
  ippAddCollection(attrs, IPP_TAG_PRINTER, "media-col-default", col);
  ippDelete(col);

  /* media-col-ready */
  col = create_media_col(default_size->map.pwg, default_source, default_type, default_size->width, default_size->length, default_size->bottom, default_size->left, default_size->right, default_size->top);
  ippAddCollection(attrs, IPP_TAG_PRINTER, "media-col-ready", col);
  ippDelete(col);

  /* media-default */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-default", NULL, default_size->map.pwg);

  /* media-left-margin-supported */
  for (i = 0, num_margins = 0, pwg_size = pc->sizes; i < pc->num_sizes && num_margins < (int)(sizeof(margins) / sizeof(margins[0])); i ++, pwg_size ++)
  {
    for (j = 0; j < num_margins; j ++)
    {
      if (margins[j] == pwg_size->left)
        break;
    }

    if (j >= num_margins)
      margins[num_margins ++] = pwg_size->left;
  }

  for (i = 0; i < (num_margins - 1); i ++)
  {
    for (j = i + 1; j < num_margins; j ++)
    {
      if (margins[i] > margins[j])
      {
        int mtemp = margins[i];

        margins[i] = margins[j];
        margins[j] = mtemp;
      }
    }
  }

  ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-left-margin-supported", num_margins, margins);

  /* media-ready */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-ready", NULL, default_size->map.pwg);

  /* media-right-margin-supported */
  for (i = 0, num_margins = 0, pwg_size = pc->sizes; i < pc->num_sizes && num_margins < (int)(sizeof(margins) / sizeof(margins[0])); i ++, pwg_size ++)
  {
    for (j = 0; j < num_margins; j ++)
    {
      if (margins[j] == pwg_size->right)
        break;
    }

    if (j >= num_margins)
      margins[num_margins ++] = pwg_size->right;
  }

  for (i = 0; i < (num_margins - 1); i ++)
  {
    for (j = i + 1; j < num_margins; j ++)
    {
      if (margins[i] > margins[j])
      {
        int mtemp = margins[i];

        margins[i] = margins[j];
        margins[j] = mtemp;
      }
    }
  }

  ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-right-margin-supported", num_margins, margins);

  /* media-supported */
  attr = ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-supported", pc->num_sizes, NULL, NULL);
  for (i = 0, pwg_size = pc->sizes; i < pc->num_sizes; i ++, pwg_size ++)
    ippSetString(attrs, &attr, i, pwg_size->map.pwg);

  /* media-size-supported */
  attr = ippAddCollections(attrs, IPP_TAG_PRINTER, "media-size-supported", pc->num_sizes, NULL);
  for (i = 0, pwg_size = pc->sizes; i < pc->num_sizes; i ++, pwg_size ++)
  {
    col = create_media_size(pwg_size->width, pwg_size->length);
    ippSetCollection(attrs, &attr, i, col);
    ippDelete(col);
  }

  /* media-source-supported */
  if (pc->num_sources > 0)
  {
    attr = ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-source-supported", pc->num_sources, NULL,  NULL);
    for (i = 0, pwg_map = pc->sources; i < pc->num_sources; i ++, pwg_map ++)
      ippSetString(attrs, &attr, i, pwg_map->pwg);
  }
  else
  {
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-source-supported", NULL, "auto");
  }

  /* media-top-margin-supported */
  for (i = 0, num_margins = 0, pwg_size = pc->sizes; i < pc->num_sizes && num_margins < (int)(sizeof(margins) / sizeof(margins[0])); i ++, pwg_size ++)
  {
    for (j = 0; j < num_margins; j ++)
    {
      if (margins[j] == pwg_size->top)
        break;
    }

    if (j >= num_margins)
      margins[num_margins ++] = pwg_size->top;
  }

  for (i = 0; i < (num_margins - 1); i ++)
  {
    for (j = i + 1; j < num_margins; j ++)
    {
      if (margins[i] > margins[j])
      {
        int mtemp = margins[i];

        margins[i] = margins[j];
        margins[j] = mtemp;
      }
    }
  }

  ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-top-margin-supported", num_margins, margins);

  /* media-type-supported */
  if (pc->num_types > 0)
  {
    attr = ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-type-supported", pc->num_types, NULL,  NULL);
    for (i = 0, pwg_map = pc->types; i < pc->num_types; i ++, pwg_map ++)
      ippSetString(attrs, &attr, i, pwg_map->pwg);
  }
  else
  {
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-type-supported", NULL, "auto");
  }

  /* orientation-requested-default */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-default", IPP_ORIENT_PORTRAIT);

  /* orientation-requested-supported */
  ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-supported", (int)(sizeof(orientation_requested_supported) / sizeof(orientation_requested_supported[0])), orientation_requested_supported);

  /* output-bin-default */
  if (pc->num_bins > 0)
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "output-bin-default", NULL, pc->bins->pwg);
  else
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-default", NULL, "face-down");

  /* output-bin-supported */
  if (pc->num_bins > 0)
  {
    attr = ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "output-bin-supported", pc->num_bins, NULL,  NULL);
    for (i = 0, pwg_map = pc->bins; i < pc->num_bins; i ++, pwg_map ++)
      ippSetString(attrs, &attr, i, pwg_map->pwg);
  }
  else
  {
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-supported", NULL, "face-down");
  }

  /* overrides-supported */
  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "overrides-supported", (int)(sizeof(overrides_supported) / sizeof(overrides_supported[0])), NULL, overrides_supported);

  /* page-ranges-supported */
  ippAddBoolean(attrs, IPP_TAG_PRINTER, "page-ranges-supported", 1);

  /* pages-per-minute */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "pages-per-minute", ppd->throughput);

  /* pages-per-minute-color */
  if (ppd->color_device)
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "pages-per-minute-color", ppd->throughput);

  /* print-color-mode-default */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-default", NULL, ppd->color_device ? "auto" : "monochrome");

  /* print-color-mode-supported */
  if (ppd->color_device)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-supported", (int)(sizeof(print_color_mode_supported_color) / sizeof(print_color_mode_supported_color[0])), NULL, print_color_mode_supported_color);
  else
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-supported", (int)(sizeof(print_color_mode_supported) / sizeof(print_color_mode_supported[0])), NULL, print_color_mode_supported);

  /* print-content-optimize-default */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-content-optimize-default", NULL, "auto");

  /* print-content-optimize-supported */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-content-optimize-supported", NULL, "auto");

  /* print-quality-default */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-default", IPP_QUALITY_NORMAL);

  /* print-quality-supported */
  ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-supported", (int)(sizeof(print_quality_supported) / sizeof(print_quality_supported[0])), print_quality_supported);

  /* print-rendering-intent-default */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-rendering-intent-default", NULL, "auto");

  /* print-rendering-intent-supported */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-rendering-intent-supported", NULL, "auto");

  /* printer-device-id */
  if ((ppd_attr = ppdFindAttr(ppd, "1284DeviceId", NULL)) != NULL)
  {
   /*
    * Use the device ID string from the PPD...
    */

    ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-device-id", NULL, ppd_attr->value);
  }
  else
  {
   /*
    * Synthesize a device ID string...
    */

    char	device_id[1024];		/* Device ID string */

    snprintf(device_id, sizeof(device_id), "MFG:%s;MDL:%s;CMD:PS;", ppd->manufacturer, ppd->modelname);

    ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-device-id", NULL, device_id);
  }

  /* printer-input-tray */
  if (pc->num_sources > 0)
  {
    for (i = 0, attr = NULL; i < pc->num_sources; i ++)
    {
      char	input_tray[1024];	/* printer-input-tray value */

      if (!strcmp(pc->sources[i].pwg, "manual") || strstr(pc->sources[i].pwg, "-man") != NULL)
        snprintf(input_tray, sizeof(input_tray), "type=sheetFeedManual;mediafeed=0;mediaxfeed=0;maxcapacity=1;level=-2;status=0;name=%s", pc->sources[i].pwg);
      else
        snprintf(input_tray, sizeof(input_tray), "type=sheetFeedAutoRemovableTray;mediafeed=0;mediaxfeed=0;maxcapacity=250;level=125;status=0;name=%s", pc->sources[i].pwg);

      if (attr)
        ippSetOctetString(attrs, &attr, i, input_tray, (int)strlen(input_tray));
      else
        attr = ippAddOctetString(attrs, IPP_TAG_PRINTER, "printer-input-tray", input_tray, (int)strlen(input_tray));
    }
  }
  else
  {
    static const char *printer_input_tray = "type=sheetFeedAutoRemovableTray;mediafeed=0;mediaxfeed=0;maxcapacity=-2;level=-2;status=0;name=auto";

    ippAddOctetString(attrs, IPP_TAG_PRINTER, "printer-input-tray", printer_input_tray, (int)strlen(printer_input_tray));
  }

  /* printer-make-and-model */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-make-and-model", NULL, ppd->nickname);

  /* printer-resolution-default */
  ippAddResolution(attrs, IPP_TAG_PRINTER, "printer-resolution-default", IPP_RES_PER_INCH, xres, yres);

  /* printer-resolution-supported */
  ippAddResolution(attrs, IPP_TAG_PRINTER, "printer-resolution-supported", IPP_RES_PER_INCH, xres, yres);

  /* printer-supply and printer-supply-description */
  if (ppd->color_device)
  {
    attr = ippAddOctetString(attrs, IPP_TAG_PRINTER, "printer-supply", printer_supply_color[0], (int)strlen(printer_supply_color[0]));
    for (i = 1; i < (int)(sizeof(printer_supply_color) / sizeof(printer_supply_color[0])); i ++)
      ippSetOctetString(attrs, &attr, i, printer_supply_color[i], (int)strlen(printer_supply_color[i]));

    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_TEXT), "printer-supply-description", (int)(sizeof(printer_supply_description_color) / sizeof(printer_supply_description_color[0])), NULL, printer_supply_description_color);
  }
  else
  {
    attr = ippAddOctetString(attrs, IPP_TAG_PRINTER, "printer-supply", printer_supply[0], (int)strlen(printer_supply[0]));
    for (i = 1; i < (int)(sizeof(printer_supply) / sizeof(printer_supply[0])); i ++)
      ippSetOctetString(attrs, &attr, i, printer_supply[i], (int)strlen(printer_supply[i]));

    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_TEXT), "printer-supply-description", (int)(sizeof(printer_supply_description) / sizeof(printer_supply_description[0])), NULL, printer_supply_description);
  }

  /* pwg-raster-document-xxx-supported */
  if (cupsArrayFind(docformats, (void *)"image/pwg-raster"))
  {
    ippAddResolution(attrs, IPP_TAG_PRINTER, "pwg-raster-document-resolution-supported", IPP_RES_PER_INCH, xres, yres);

    if (pc->sides_2sided_long)
      ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pwg-raster-document-sheet-back", NULL, "normal");

    if (ppd->color_device)
      ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pwg-raster-document-type-supported", (int)(sizeof(pwg_raster_document_type_supported_color) / sizeof(pwg_raster_document_type_supported_color[0])), NULL, pwg_raster_document_type_supported_color);
    else
      ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pwg-raster-document-type-supported", (int)(sizeof(pwg_raster_document_type_supported) / sizeof(pwg_raster_document_type_supported[0])), NULL, pwg_raster_document_type_supported);
  }

  /* sides-default */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "sides-default", NULL, "one-sided");

  /* sides-supported */
  if (pc->sides_2sided_long)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "sides-supported", (int)(sizeof(sides_supported) / sizeof(sides_supported[0])), NULL, sides_supported);
  else
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "sides-supported", NULL, "one-sided");

  /* urf-supported */
  if (cupsArrayFind(docformats, (void *)"image/urf"))
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "urf-supported", num_urf, NULL, urf);

 /*
  * Free the PPD file and return the attributes...
  */

  _ppdCacheDestroy(pc);

  ppdClose(ppd);

  return (attrs);
}
#endif /* !CUPS_LITE */


/*
 * 'parse_options()' - Parse URL options into CUPS options.
 *
 * The client->options string is destroyed by this function.
 */

static int				/* O - Number of options */
parse_options(ippeve_client_t *client,	/* I - Client */
              cups_option_t   **options)/* O - Options */
{
  char	*name,				/* Name */
	*value,				/* Value */
	*next;				/* Next name=value pair */
  int	num_options = 0;		/* Number of options */


  *options = NULL;

  for (name = client->options; name && *name; name = next)
  {
    if ((value = strchr(name, '=')) == NULL)
      break;

    *value++ = '\0';
    if ((next = strchr(value, '&')) != NULL)
      *next++ = '\0';

    num_options = cupsAddOption(name, value, num_options, options);
  }

  return (num_options);
}


/*
 * 'process_attr_message()' - Process an ATTR: message from a command.
 */

static void
process_attr_message(
    ippeve_job_t *job,			/* I - Job */
    char       *message)		/* I - Message */
{
  int		i,			/* Looping var */
		num_options = 0;	/* Number of name=value pairs */
  cups_option_t	*options = NULL,	/* name=value pairs from message */
		*option;		/* Current option */
  ipp_attribute_t *attr;		/* Current attribute */


 /*
  * Grab attributes from the message line...
  */

  num_options = cupsParseOptions(message + 5, num_options, &options);

 /*
  * Loop through the options and record them in the printer or job objects...
  */

  for (i = num_options, option = options; i > 0; i --, option ++)
  {
    if (!strcmp(option->name, "job-impressions"))
    {
     /*
      * Update job-impressions attribute...
      */

      job->impressions = atoi(option->value);
    }
    else if (!strcmp(option->name, "job-impressions-completed"))
    {
     /*
      * Update job-impressions-completed attribute...
      */

      job->impcompleted = atoi(option->value);
    }
    else if (!strncmp(option->name, "marker-", 7) || !strcmp(option->name, "printer-alert") || !strcmp(option->name, "printer-alert-description") || !strcmp(option->name, "printer-supply") || !strcmp(option->name, "printer-supply-description"))
    {
     /*
      * Update Printer Status attribute...
      */

      _cupsRWLockWrite(&job->printer->rwlock);

      if ((attr = ippFindAttribute(job->printer->attrs, option->name, IPP_TAG_ZERO)) != NULL)
        ippDeleteAttribute(job->printer->attrs, attr);

      cupsEncodeOption(job->printer->attrs, IPP_TAG_PRINTER, option->name, option->value);

      _cupsRWUnlock(&job->printer->rwlock);
    }
    else
    {
     /*
      * Something else that isn't currently supported...
      */

      fprintf(stderr, "[Job %d] Ignoring update of attribute \"%s\" with value \"%s\".\n", job->id, option->name, option->value);
    }
  }

  cupsFreeOptions(num_options, options);
}


/*
 * 'process_client()' - Process client requests on a thread.
 */

static void *				/* O - Exit status */
process_client(ippeve_client_t *client)	/* I - Client */
{
 /*
  * Loop until we are out of requests or timeout (30 seconds)...
  */

#ifdef HAVE_SSL
  int first_time = 1;			/* First time request? */
#endif /* HAVE_SSL */

  while (httpWait(client->http, 30000))
  {
#ifdef HAVE_SSL
    if (first_time)
    {
     /*
      * See if we need to negotiate a TLS connection...
      */

      char buf[1];			/* First byte from client */

      if (recv(httpGetFd(client->http), buf, 1, MSG_PEEK) == 1 && (!buf[0] || !strchr("DGHOPT", buf[0])))
      {
        fprintf(stderr, "%s Starting HTTPS session.\n", client->hostname);

	if (httpEncryption(client->http, HTTP_ENCRYPTION_ALWAYS))
	{
	  fprintf(stderr, "%s Unable to encrypt connection: %s\n", client->hostname, cupsLastErrorString());
	  break;
        }

        fprintf(stderr, "%s Connection now encrypted.\n", client->hostname);
      }

      first_time = 0;
    }
#endif /* HAVE_SSL */

    if (!process_http(client))
      break;
  }

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
process_http(ippeve_client_t *client)	/* I - Client connection */
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
      fprintf(stderr, "%s Bad request line (%s).\n", client->hostname, strerror(httpError(client->http)));

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

  fprintf(stderr, "%s %s %s\n", client->hostname, http_states[http_state], uri);

 /*
  * Separate the URI into its components...
  */

  if (httpSeparateURI(HTTP_URI_CODING_MOST, uri, scheme, sizeof(scheme),
		      userpass, sizeof(userpass),
		      hostname, sizeof(hostname), &port,
		      client->uri, sizeof(client->uri)) < HTTP_URI_STATUS_OK &&
      (http_state != HTTP_STATE_OPTIONS || strcmp(uri, "*")))
  {
    fprintf(stderr, "%s Bad URI \"%s\".\n", client->hostname, uri);
    respond_http(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }

  if ((client->options = strchr(client->uri, '?')) != NULL)
    *(client->options)++ = '\0';

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

  if (!strcasecmp(httpGetField(client->http, HTTP_FIELD_CONNECTION),
                        "Upgrade"))
  {
#ifdef HAVE_SSL
    if (strstr(httpGetField(client->http, HTTP_FIELD_UPGRADE), "TLS/") != NULL && !httpIsEncrypted(client->http))
    {
      if (!respond_http(client, HTTP_STATUS_SWITCHING_PROTOCOLS, NULL, NULL, 0))
        return (0);

      fprintf(stderr, "%s Upgrading to encrypted connection.\n", client->hostname);

      if (httpEncryption(client->http, HTTP_ENCRYPTION_REQUIRED))
      {
        fprintf(stderr, "%s Unable to encrypt connection: %s\n", client->hostname, cupsLastErrorString());
	return (0);
      }

      fprintf(stderr, "%s Connection now encrypted.\n", client->hostname);
    }
    else
#endif /* HAVE_SSL */

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

  switch (client->operation)
  {
    case HTTP_STATE_OPTIONS :
       /*
	* Do OPTIONS command...
	*/

	return (respond_http(client, HTTP_STATUS_OK, NULL, NULL, 0));

    case HTTP_STATE_HEAD :
        if (!strcmp(client->uri, "/icon.png"))
	  return (respond_http(client, HTTP_STATUS_OK, NULL, "image/png", 0));
	else if (!strcmp(client->uri, "/") || !strcmp(client->uri, "/media") || !strcmp(client->uri, "/supplies"))
	  return (respond_http(client, HTTP_STATUS_OK, NULL, "text/html", 0));
	else
	  return (respond_http(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0));

    case HTTP_STATE_GET :
        if (!strcmp(client->uri, "/icon.png"))
	{
	 /*
	  * Send PNG icon file.
	  */

          if (client->printer->icon)
          {
	    int		fd;		/* Icon file */
	    struct stat	fileinfo;	/* Icon file information */
	    char	buffer[4096];	/* Copy buffer */
	    ssize_t	bytes;		/* Bytes */

	    fprintf(stderr, "Icon file is \"%s\".\n", client->printer->icon);

	    if (!stat(client->printer->icon, &fileinfo) && (fd = open(client->printer->icon, O_RDONLY)) >= 0)
	    {
	      if (!respond_http(client, HTTP_STATUS_OK, NULL, "image/png", (size_t)fileinfo.st_size))
	      {
		close(fd);
		return (0);
	      }

	      while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
		httpWrite2(client->http, buffer, (size_t)bytes);

	      httpFlushWrite(client->http);

	      close(fd);
	    }
	    else
	      return (respond_http(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0));
	  }
	  else
	  {
	    fputs("Icon file is internal printer.png.\n", stderr);

	    if (!respond_http(client, HTTP_STATUS_OK, NULL, "image/png", sizeof(printer_png)))
	      return (0);

            httpWrite2(client->http, (const char *)printer_png, sizeof(printer_png));
	    httpFlushWrite(client->http);
	  }
	}
	else if (!strcmp(client->uri, "/"))
	{
	 /*
	  * Show web status page...
	  */

          return (show_status(client));
	}
	else if (!strcmp(client->uri, "/media"))
	{
	 /*
	  * Show web media page...
	  */

          return (show_media(client));
	}
	else if (!strcmp(client->uri, "/supplies"))
	{
	 /*
	  * Show web supplies page...
	  */

          return (show_supplies(client));
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
            fprintf(stderr, "%s IPP read error (%s).\n", client->hostname, cupsLastErrorString());
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
process_ipp(ippeve_client_t *client)	/* I - Client */
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

    respond_ipp(client, IPP_STATUS_ERROR_VERSION_NOT_SUPPORTED, "Bad request version number %d.%d.", major, minor);
  }
  else if ((major * 10 + minor) > MaxVersion)
  {
    if (httpGetState(client->http) != HTTP_STATE_POST_SEND)
      httpFlush(client->http);		/* Flush trailing (junk) data */

    respond_http(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }
  else if (ippGetRequestId(client->request) <= 0)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "Bad request-id %d.", ippGetRequestId(client->request));
  }
  else if (!ippFirstAttribute(client->request))
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "No attributes in request.");
  }
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
          strcasecmp(ippGetString(charset, 0, NULL), "us-ascii") &&
          strcasecmp(ippGetString(charset, 0, NULL), "utf-8"))
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

	    case IPP_OP_CLOSE_JOB :
	        ipp_close_job(client);
		break;

	    case IPP_OP_IDENTIFY_PRINTER :
	        ipp_identify_printer(client);
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
process_job(ippeve_job_t *job)		/* I - Job */
{
  job->state          = IPP_JSTATE_PROCESSING;
  job->printer->state = IPP_PSTATE_PROCESSING;
  job->processing     = time(NULL);

  while (job->printer->state_reasons & IPPEVE_PREASON_MEDIA_EMPTY)
  {
    job->printer->state_reasons |= IPPEVE_PREASON_MEDIA_NEEDED;

    sleep(1);
  }

  job->printer->state_reasons &= (ippeve_preason_t)~IPPEVE_PREASON_MEDIA_NEEDED;

  if (job->printer->command)
  {
   /*
    * Execute a command with the job spool file and wait for it to complete...
    */

    int 		pid,		/* Process ID */
			status;		/* Exit status */
    struct timeval	start,		/* Start time */
			end;		/* End time */
    char		*myargv[3],	/* Command-line arguments */
			*myenvp[400];	/* Environment variables */
    int			myenvc;		/* Number of environment variables */
    ipp_attribute_t	*attr;		/* Job attribute */
    char		val[1280],	/* IPP_NAME=value */
			*valptr;	/* Pointer into string */
#ifndef _WIN32
    int			mystdout = -1;	/* File for stdout */
    int			mypipe[2];	/* Pipe for stderr */
    char		line[2048],	/* Line from stderr */
			*ptr,		/* Pointer into line */
			*endptr;	/* End of line */
    ssize_t		bytes;		/* Bytes read */
#endif /* !_WIN32 */

    fprintf(stderr, "[Job %d] Running command \"%s %s\".\n", job->id, job->printer->command, job->filename);
    gettimeofday(&start, NULL);

   /*
    * Setup the command-line arguments...
    */

    myargv[0] = job->printer->command;
    myargv[1] = job->filename;
    myargv[2] = NULL;

   /*
    * Copy the current environment, then add environment variables for every
    * Job attribute and Printer -default attributes...
    */

    for (myenvc = 0; environ[myenvc] && myenvc < (int)(sizeof(myenvp) / sizeof(myenvp[0]) - 1); myenvc ++)
      myenvp[myenvc] = strdup(environ[myenvc]);

    if (myenvc > (int)(sizeof(myenvp) / sizeof(myenvp[0]) - 32))
    {
      fprintf(stderr, "[Job %d] Too many environment variables to process job.\n", job->id);
      job->state = IPP_JSTATE_ABORTED;
      goto error;
    }

    snprintf(val, sizeof(val), "CONTENT_TYPE=%s", job->format);
    myenvp[myenvc ++] = strdup(val);

    if (job->printer->device_uri)
    {
      snprintf(val, sizeof(val), "DEVICE_URI=%s", job->printer->device_uri);
      myenvp[myenvc ++] = strdup(val);
    }

    if (job->printer->output_format)
    {
      snprintf(val, sizeof(val), "OUTPUT_TYPE=%s", job->printer->output_format);
      myenvp[myenvc ++] = strdup(val);
    }

#if !CUPS_LITE
    if (job->printer->ppdfile)
    {
      snprintf(val, sizeof(val), "PPD=%s", job->printer->ppdfile);
      myenvp[myenvc++] = strdup(val);
    }
#endif /* !CUPS_LITE */

    for (attr = ippFirstAttribute(job->printer->attrs); attr && myenvc < (int)(sizeof(myenvp) / sizeof(myenvp[0]) - 1); attr = ippNextAttribute(job->printer->attrs))
    {
     /*
      * Convert "attribute-name-default" to "IPP_ATTRIBUTE_NAME_DEFAULT=" and
      * "pwg-xxx" to "IPP_PWG_XXX", then add the value(s) from the attribute.
      */

      const char	*name = ippGetName(attr),
					/* Attribute name */
			*suffix = strstr(name, "-default");
					/* Suffix on attribute name */

      if (strncmp(name, "pwg-", 4) && (!suffix || suffix[8]))
        continue;

      valptr = val;
      *valptr++ = 'I';
      *valptr++ = 'P';
      *valptr++ = 'P';
      *valptr++ = '_';
      while (*name && valptr < (val + sizeof(val) - 2))
      {
        if (*name == '-')
	  *valptr++ = '_';
	else
	  *valptr++ = (char)toupper(*name & 255);

	name ++;
      }
      *valptr++ = '=';
      ippAttributeString(attr, valptr, sizeof(val) - (size_t)(valptr - val));

      myenvp[myenvc++] = strdup(val);
    }

    for (attr = ippFirstAttribute(job->attrs); attr && myenvc < (int)(sizeof(myenvp) / sizeof(myenvp[0]) - 1); attr = ippNextAttribute(job->attrs))
    {
     /*
      * Convert "attribute-name" to "IPP_ATTRIBUTE_NAME=" and then add the
      * value(s) from the attribute.
      */

      const char *name = ippGetName(attr);
					/* Attribute name */

      if (!name)
        continue;

      valptr = val;
      *valptr++ = 'I';
      *valptr++ = 'P';
      *valptr++ = 'P';
      *valptr++ = '_';
      while (*name && valptr < (val + sizeof(val) - 2))
      {
        if (*name == '-')
	  *valptr++ = '_';
	else
	  *valptr++ = (char)toupper(*name & 255);

	name ++;
      }
      *valptr++ = '=';
      ippAttributeString(attr, valptr, sizeof(val) - (size_t)(valptr - val));

      myenvp[myenvc++] = strdup(val);
    }

    if (attr)
    {
      fprintf(stderr, "[Job %d] Too many environment variables to process job.\n", job->id);
      job->state = IPP_JSTATE_ABORTED;
      goto error;
    }

    myenvp[myenvc] = NULL;

   /*
    * Now run the program...
    */

#ifdef _WIN32
    status = _spawnvpe(_P_WAIT, job->printer->command, myargv, myenvp);

#else
    if (job->printer->device_uri)
    {
      char	scheme[32],		/* URI scheme */
		userpass[256],		/* username:password (unused) */
		host[256],		/* Hostname or IP address */
		resource[256];		/* Resource path */
      int	port;			/* Port number */


      if (httpSeparateURI(HTTP_URI_CODING_ALL, job->printer->device_uri, scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
      {
        fprintf(stderr, "[Job %d] Bad device URI \"%s\".\n", job->id, job->printer->device_uri);
      }
      else if (!strcmp(scheme, "file"))
      {
        struct stat	fileinfo;	/* See if this is a file or directory... */

        if (stat(resource, &fileinfo))
        {
          if (errno == ENOENT)
          {
            if ((mystdout = open(resource, O_WRONLY | O_CREAT | O_TRUNC, 0666)) >= 0)
	      fprintf(stderr, "[Job %d] Saving print command output to \"%s\".\n", job->id, resource);
	    else
	      fprintf(stderr, "[Job %d] Unable to create \"%s\": %s\n", job->id, resource, strerror(errno));
          }
          else
            fprintf(stderr, "[Job %d] Unable to access \"%s\": %s\n", job->id, resource, strerror(errno));
        }
        else if (S_ISDIR(fileinfo.st_mode))
        {
          if ((mystdout = create_job_file(job, line, sizeof(line), resource, "prn")) >= 0)
	    fprintf(stderr, "[Job %d] Saving print command output to \"%s\".\n", job->id, line);
          else
            fprintf(stderr, "[Job %d] Unable to create \"%s\": %s\n", job->id, line, strerror(errno));
        }
	else if (!S_ISREG(fileinfo.st_mode))
	{
	  if ((mystdout = open(resource, O_WRONLY | O_CREAT | O_TRUNC, 0666)) >= 0)
	    fprintf(stderr, "[Job %d] Saving print command output to \"%s\".\n", job->id, resource);
	  else
            fprintf(stderr, "[Job %d] Unable to create \"%s\": %s\n", job->id, resource, strerror(errno));
	}
        else if ((mystdout = open(resource, O_WRONLY)) >= 0)
	  fprintf(stderr, "[Job %d] Saving print command output to \"%s\".\n", job->id, resource);
	else
	  fprintf(stderr, "[Job %d] Unable to open \"%s\": %s\n", job->id, resource, strerror(errno));
      }
      else if (!strcmp(scheme, "socket"))
      {
        http_addrlist_t	*addrlist;	/* List of addresses */
        char		service[32];	/* Service number */

        snprintf(service, sizeof(service), "%d", port);

        if ((addrlist = httpAddrGetList(host, AF_UNSPEC, service)) == NULL)
          fprintf(stderr, "[Job %d] Unable to find \"%s\": %s\n", job->id, host, cupsLastErrorString());
        else if (!httpAddrConnect2(addrlist, &mystdout, 30000, &(job->cancel)))
          fprintf(stderr, "[Job %d] Unable to connect to \"%s\": %s\n", job->id, host, cupsLastErrorString());

        httpAddrFreeList(addrlist);
      }
      else
      {
        fprintf(stderr, "[Job %d] Unsupported device URI scheme \"%s\".\n", job->id, scheme);
      }
    }
    else if ((mystdout = create_job_file(job, line, sizeof(line), job->printer->directory, "prn")) >= 0)
    {
      fprintf(stderr, "[Job %d] Saving print command output to \"%s\".\n", job->id, line);
    }

    if (mystdout < 0)
      mystdout = open("/dev/null", O_WRONLY);

    if (pipe(mypipe))
    {
      fprintf(stderr, "[Job %d] Unable to create pipe for stderr: %s\n", job->id, strerror(errno));
      mypipe[0] = mypipe[1] = -1;
    }

    if ((pid = fork()) == 0)
    {
     /*
      * Child comes here...
      */

      close(1);
      dup2(mystdout, 1);
      close(mystdout);

      close(2);
      dup2(mypipe[1], 2);
      close(mypipe[0]);
      close(mypipe[1]);

      execve(job->printer->command, myargv, myenvp);
      exit(errno);
    }
    else if (pid < 0)
    {
     /*
      * Unable to fork process...
      */

      fprintf(stderr, "[Job %d] Unable to start job processing command: %s\n", job->id, strerror(errno));
      status = -1;

      close(mystdout);
      close(mypipe[0]);
      close(mypipe[1]);

     /*
      * Free memory used for environment...
      */

      while (myenvc > 0)
	free(myenvp[-- myenvc]);
    }
    else
    {
     /*
      * Free memory used for environment...
      */

      while (myenvc > 0)
	free(myenvp[-- myenvc]);

     /*
      * Close the output file in the parent process...
      */

      close(mystdout);

     /*
      * If the pipe exists, read from it until EOF...
      */

      if (mypipe[0] >= 0)
      {
	close(mypipe[1]);

	endptr = line;
	while ((bytes = read(mypipe[0], endptr, sizeof(line) - (size_t)(endptr - line) - 1)) > 0)
	{
	  endptr += bytes;
	  *endptr = '\0';

          while ((ptr = strchr(line, '\n')) != NULL)
	  {
	    int level = 3;		/* Message log level */

	    *ptr++ = '\0';

	    if (!strncmp(line, "ATTR:", 5))
	    {
	     /*
	      * Process job/printer attribute updates.
	      */

	      process_attr_message(job, line);
	    }
	    else if (!strncmp(line, "DEBUG:", 6))
	    {
	     /*
	      * Debug message...
	      */

              level = 2;
	    }
	    else if (!strncmp(line, "ERROR:", 6))
	    {
	     /*
	      * Error message...
	      */

              level         = 0;
              job->message  = strdup(line + 6);
              job->msglevel = 0;
	    }
	    else if (!strncmp(line, "INFO:", 5))
	    {
	     /*
	      * Informational/progress message...
	      */

              level = 1;
              if (job->msglevel)
              {
                job->message  = strdup(line + 5);
                job->msglevel = 1;
	      }
	    }
	    else if (!strncmp(line, "STATE:", 6))
	    {
	     /*
	      * Process printer-state-reasons keywords.
	      */

	      process_state_message(job, line);
	    }

	    if (Verbosity >= level)
	      fprintf(stderr, "[Job %d] Command - %s\n", job->id, line);

	    bytes = ptr - line;
            if (ptr < endptr)
	      memmove(line, ptr, (size_t)(endptr - ptr));
	    endptr -= bytes;
	    *endptr = '\0';
	  }
	}

	close(mypipe[0]);
      }

     /*
      * Wait for child to complete...
      */

#  ifdef HAVE_WAITPID
      while (waitpid(pid, &status, 0) < 0);
#  else
      while (wait(&status) < 0);
#  endif /* HAVE_WAITPID */
    }
#endif /* _WIN32 */

    if (status)
    {
#ifndef _WIN32
      if (WIFEXITED(status))
#endif /* !_WIN32 */
	fprintf(stderr, "[Job %d] Command \"%s\" exited with status %d.\n", job->id,  job->printer->command, WEXITSTATUS(status));
#ifndef _WIN32
      else
	fprintf(stderr, "[Job %d] Command \"%s\" terminated with signal %d.\n", job->id, job->printer->command, WTERMSIG(status));
#endif /* !_WIN32 */
      job->state = IPP_JSTATE_ABORTED;
    }
    else if (status < 0)
      job->state = IPP_JSTATE_ABORTED;
    else
      fprintf(stderr, "[Job %d] Command \"%s\" completed successfully.\n", job->id, job->printer->command);

   /*
    * Report the total processing time...
    */

    gettimeofday(&end, NULL);

    fprintf(stderr, "[Job %d] Processing time was %.3f seconds.\n", job->id, end.tv_sec - start.tv_sec + 0.000001 * (end.tv_usec - start.tv_usec));
  }
  else
  {
   /*
    * Sleep for a random amount of time to simulate job processing.
    */

    sleep((unsigned)(5 + (CUPS_RAND() % 11)));
  }

  if (job->cancel)
    job->state = IPP_JSTATE_CANCELED;
  else if (job->state == IPP_JSTATE_PROCESSING)
    job->state = IPP_JSTATE_COMPLETED;

  error:

  job->completed           = time(NULL);
  job->printer->state      = IPP_PSTATE_IDLE;
  job->printer->active_job = NULL;

  return (NULL);
}


/*
 * 'process_state_message()' - Process a STATE: message from a command.
 */

static void
process_state_message(
    ippeve_job_t *job,			/* I - Job */
    char       *message)		/* I - Message */
{
  int		i;			/* Looping var */
  ippeve_preason_t state_reasons,		/* printer-state-reasons values */
		bit;			/* Current reason bit */
  char		*ptr,			/* Pointer into message */
		*next;			/* Next keyword in message */
  int		remove;			/* Non-zero if we are removing keywords */


 /*
  * Skip leading "STATE:" and any whitespace...
  */

  for (message += 6; *message; message ++)
    if (*message != ' ' && *message != '\t')
      break;

 /*
  * Support the following forms of message:
  *
  * "keyword[,keyword,...]" to set the printer-state-reasons value(s).
  *
  * "-keyword[,keyword,...]" to remove keywords.
  *
  * "+keyword[,keyword,...]" to add keywords.
  *
  * Keywords may or may not have a suffix (-report, -warning, -error) per
  * RFC 8011.
  */

  if (*message == '-')
  {
    remove        = 1;
    state_reasons = job->printer->state_reasons;
    message ++;
  }
  else if (*message == '+')
  {
    remove        = 0;
    state_reasons = job->printer->state_reasons;
    message ++;
  }
  else
  {
    remove        = 0;
    state_reasons = IPPEVE_PREASON_NONE;
  }

  while (*message)
  {
    if ((next = strchr(message, ',')) != NULL)
      *next++ = '\0';

    if ((ptr = strstr(message, "-error")) != NULL)
      *ptr = '\0';
    else if ((ptr = strstr(message, "-report")) != NULL)
      *ptr = '\0';
    else if ((ptr = strstr(message, "-warning")) != NULL)
      *ptr = '\0';

    for (i = 0, bit = 1; i < (int)(sizeof(ippeve_preason_strings) / sizeof(ippeve_preason_strings[0])); i ++, bit *= 2)
    {
      if (!strcmp(message, ippeve_preason_strings[i]))
      {
        if (remove)
	  state_reasons &= ~bit;
	else
	  state_reasons |= bit;
      }
    }

    if (next)
      message = next;
    else
      break;
  }

  job->printer->state_reasons = state_reasons;
}


/*
 * 'register_printer()' - Register a printer object via Bonjour.
 */

static int				/* O - 1 on success, 0 on error */
register_printer(
    ippeve_printer_t *printer,		/* I - Printer */
    const char       *subtypes)		/* I - Service subtype(s) */
{
#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
  ippeve_txt_t		ipp_txt;	/* Bonjour IPP TXT record */
  int			i,		/* Looping var */
			count;		/* Number of values */
  ipp_attribute_t	*color_supported,
			*document_format_supported,
			*printer_location,
			*printer_make_and_model,
			*printer_more_info,
			*printer_uuid,
			*sides_supported,
			*urf_supported;	/* Printer attributes */
  const char		*value;		/* Value string */
  char			formats[252],	/* List of supported formats */
			urf[252],	/* List of supported URF values */
			*ptr;		/* Pointer into string */

  color_supported           = ippFindAttribute(printer->attrs, "color-supported", IPP_TAG_BOOLEAN);
  document_format_supported = ippFindAttribute(printer->attrs, "document-format-supported", IPP_TAG_MIMETYPE);
  printer_location          = ippFindAttribute(printer->attrs, "printer-location", IPP_TAG_TEXT);
  printer_make_and_model    = ippFindAttribute(printer->attrs, "printer-make-and-model", IPP_TAG_TEXT);
  printer_more_info         = ippFindAttribute(printer->attrs, "printer-more-info", IPP_TAG_URI);
  printer_uuid              = ippFindAttribute(printer->attrs, "printer-uuid", IPP_TAG_URI);
  sides_supported           = ippFindAttribute(printer->attrs, "sides-supported", IPP_TAG_KEYWORD);
  urf_supported             = ippFindAttribute(printer->attrs, "urf-supported", IPP_TAG_KEYWORD);

  for (i = 0, count = ippGetCount(document_format_supported), ptr = formats; i < count; i ++)
  {
    value = ippGetString(document_format_supported, i, NULL);

    if (!strcasecmp(value, "application/octet-stream"))
      continue;

    if (ptr > formats && ptr < (formats + sizeof(formats) - 1))
      *ptr++ = ',';

    strlcpy(ptr, value, sizeof(formats) - (size_t)(ptr - formats));
    ptr += strlen(ptr);

    if (ptr >= (formats + sizeof(formats) - 1))
      break;
  }

  urf[0] = '\0';
  for (i = 0, count = ippGetCount(urf_supported), ptr = urf; i < count; i ++)
  {
    value = ippGetString(urf_supported, i, NULL);

    if (ptr > urf && ptr < (urf + sizeof(urf) - 1))
      *ptr++ = ',';

    strlcpy(ptr, value, sizeof(urf) - (size_t)(ptr - urf));
    ptr += strlen(ptr);

    if (ptr >= (urf + sizeof(urf) - 1))
      break;
  }

#endif /* HAVE_DNSSD || HAVE_AVAHI */
#ifdef HAVE_DNSSD
  DNSServiceErrorType	error;		/* Error from Bonjour */
  char			regtype[256];	/* Bonjour service type */


 /*
  * Build the TXT record for IPP...
  */

  TXTRecordCreate(&ipp_txt, 1024, NULL);
  TXTRecordSetValue(&ipp_txt, "rp", 9, "ipp/print");
  if ((value = ippGetString(printer_make_and_model, 0, NULL)) != NULL)
    TXTRecordSetValue(&ipp_txt, "ty", (uint8_t)strlen(value), value);
  if ((value = ippGetString(printer_more_info, 0, NULL)) != NULL)
    TXTRecordSetValue(&ipp_txt, "adminurl", (uint8_t)strlen(value), value);
  if ((value = ippGetString(printer_location, 0, NULL)) != NULL)
    TXTRecordSetValue(&ipp_txt, "note", (uint8_t)strlen(value), value);
  TXTRecordSetValue(&ipp_txt, "pdl", (uint8_t)strlen(formats), formats);
  TXTRecordSetValue(&ipp_txt, "Color", 1, ippGetBoolean(color_supported, 0) ? "T" : "F");
  TXTRecordSetValue(&ipp_txt, "Duplex", 1, ippGetCount(sides_supported) > 1 ? "T" : "F");
  if ((value = ippGetString(printer_uuid, 0, NULL)) != NULL)
    TXTRecordSetValue(&ipp_txt, "UUID", (uint8_t)strlen(value) - 9, value + 9);
#  ifdef HAVE_SSL
  TXTRecordSetValue(&ipp_txt, "TLS", 3, "1.2");
#  endif /* HAVE_SSL */
  if (urf[0])
    TXTRecordSetValue(&ipp_txt, "URF", (uint8_t)strlen(urf), urf);
  TXTRecordSetValue(&ipp_txt, "txtvers", 1, "1");
  TXTRecordSetValue(&ipp_txt, "qtotal", 1, "1");

 /*
  * Register the _printer._tcp (LPD) service type with a port number of 0 to
  * defend our service name but not actually support LPD...
  */

  printer->printer_ref = DNSSDMaster;

  if ((error = DNSServiceRegister(&(printer->printer_ref), kDNSServiceFlagsShareConnection, 0 /* interfaceIndex */, printer->dnssd_name, "_printer._tcp", NULL /* domain */, NULL /* host */, 0 /* port */, 0 /* txtLen */, NULL /* txtRecord */, (DNSServiceRegisterReply)dnssd_callback, printer)) != kDNSServiceErr_NoError)
  {
    _cupsLangPrintf(stderr, _("Unable to register \"%s.%s\": %d"), printer->dnssd_name, "_printer._tcp", error);
    return (0);
  }

 /*
  * Then register the _ipp._tcp (IPP) service type with the real port number to
  * advertise our IPP printer...
  */

  printer->ipp_ref = DNSSDMaster;

  if (subtypes && *subtypes)
    snprintf(regtype, sizeof(regtype), "_ipp._tcp,%s", subtypes);
  else
    strlcpy(regtype, "_ipp._tcp", sizeof(regtype));

  if ((error = DNSServiceRegister(&(printer->ipp_ref), kDNSServiceFlagsShareConnection, 0 /* interfaceIndex */, printer->dnssd_name, regtype, NULL /* domain */, NULL /* host */, htons(printer->port), TXTRecordGetLength(&ipp_txt), TXTRecordGetBytesPtr(&ipp_txt), (DNSServiceRegisterReply)dnssd_callback, printer)) != kDNSServiceErr_NoError)
  {
    _cupsLangPrintf(stderr, _("Unable to register \"%s.%s\": %d"), printer->dnssd_name, regtype, error);
    return (0);
  }

#  ifdef HAVE_SSL
 /*
  * Then register the _ipps._tcp (IPP) service type with the real port number to
  * advertise our IPPS printer...
  */

  printer->ipps_ref = DNSSDMaster;

  if (subtypes && *subtypes)
    snprintf(regtype, sizeof(regtype), "_ipps._tcp,%s", subtypes);
  else
    strlcpy(regtype, "_ipps._tcp", sizeof(regtype));

  if ((error = DNSServiceRegister(&(printer->ipps_ref), kDNSServiceFlagsShareConnection, 0 /* interfaceIndex */, printer->dnssd_name, regtype, NULL /* domain */, NULL /* host */, htons(printer->port), TXTRecordGetLength(&ipp_txt), TXTRecordGetBytesPtr(&ipp_txt), (DNSServiceRegisterReply)dnssd_callback, printer)) != kDNSServiceErr_NoError)
  {
    _cupsLangPrintf(stderr, _("Unable to register \"%s.%s\": %d"), printer->dnssd_name, regtype, error);
    return (0);
  }
#  endif /* HAVE_SSL */

 /*
  * Similarly, register the _http._tcp,_printer (HTTP) service type with the
  * real port number to advertise our IPP printer...
  */

  printer->http_ref = DNSSDMaster;

  if ((error = DNSServiceRegister(&(printer->http_ref), kDNSServiceFlagsShareConnection, 0 /* interfaceIndex */, printer->dnssd_name, "_http._tcp,_printer", NULL /* domain */, NULL /* host */, htons(printer->port), 0 /* txtLen */, NULL /* txtRecord */, (DNSServiceRegisterReply)dnssd_callback, printer)) != kDNSServiceErr_NoError)
  {
    _cupsLangPrintf(stderr, _("Unable to register \"%s.%s\": %d"), printer->dnssd_name, "_http._tcp,_printer", error);
    return (0);
  }

  TXTRecordDeallocate(&ipp_txt);

#elif defined(HAVE_AVAHI)
  char		temp[256];		/* Subtype service string */

 /*
  * Create the TXT record...
  */

  ipp_txt = NULL;
  ipp_txt = avahi_string_list_add_printf(ipp_txt, "rp=ipp/print");
  if ((value = ippGetString(printer_make_and_model, 0, NULL)) != NULL)
    ipp_txt = avahi_string_list_add_printf(ipp_txt, "ty=%s", value);
  if ((value = ippGetString(printer_more_info, 0, NULL)) != NULL)
    ipp_txt = avahi_string_list_add_printf(ipp_txt, "adminurl=%s", value);
  if ((value = ippGetString(printer_location, 0, NULL)) != NULL)
    ipp_txt = avahi_string_list_add_printf(ipp_txt, "note=%s", value);
  ipp_txt = avahi_string_list_add_printf(ipp_txt, "pdl=%s", formats);
  ipp_txt = avahi_string_list_add_printf(ipp_txt, "Color=%s", ippGetBoolean(color_supported, 0) ? "T" : "F");
  ipp_txt = avahi_string_list_add_printf(ipp_txt, "Duplex=%s", ippGetCount(sides_supported) > 1 ? "T" : "F");
  if ((value = ippGetString(printer_uuid, 0, NULL)) != NULL)
    ipp_txt = avahi_string_list_add_printf(ipp_txt, "UUID=%s", value + 9);
#  ifdef HAVE_SSL
  ipp_txt = avahi_string_list_add_printf(ipp_txt, "TLS=1.2");
#  endif /* HAVE_SSL */
  if (urf[0])
    ipp_txt = avahi_string_list_add_printf(ipp_txt, "URF=%s", urf);
  ipp_txt = avahi_string_list_add_printf(ipp_txt, "txtvers=1");
  ipp_txt = avahi_string_list_add_printf(ipp_txt, "qtotal=1");

 /*
  * Register _printer._tcp (LPD) with port 0 to reserve the service name...
  */

  avahi_threaded_poll_lock(DNSSDMaster);

  printer->ipp_ref = avahi_entry_group_new(DNSSDClient, dnssd_callback, NULL);

  avahi_entry_group_add_service_strlst(printer->ipp_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dnssd_name, "_printer._tcp", NULL, NULL, 0, NULL);

 /*
  * Then register the _ipp._tcp (IPP)...
  */

  avahi_entry_group_add_service_strlst(printer->ipp_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dnssd_name, "_ipp._tcp", NULL, NULL, printer->port, ipp_txt);
  if (subtypes && *subtypes)
  {
    char *temptypes = strdup(subtypes), *start, *end;

    for (start = temptypes; *start; start = end)
    {
      if ((end = strchr(start, ',')) != NULL)
        *end++ = '\0';
      else
        end = start + strlen(start);

      snprintf(temp, sizeof(temp), "%s._sub._ipp._tcp", start);
      avahi_entry_group_add_service_subtype(printer->ipp_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dnssd_name, "_ipp._tcp", NULL, temp);
    }

    free(temptypes);
  }

#ifdef HAVE_SSL
 /*
  * _ipps._tcp (IPPS) for secure printing...
  */

  avahi_entry_group_add_service_strlst(printer->ipp_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dnssd_name, "_ipps._tcp", NULL, NULL, printer->port, ipp_txt);
  if (subtypes && *subtypes)
  {
    char *temptypes = strdup(subtypes), *start, *end;

    for (start = temptypes; *start; start = end)
    {
      if ((end = strchr(start, ',')) != NULL)
        *end++ = '\0';
      else
        end = start + strlen(start);

      snprintf(temp, sizeof(temp), "%s._sub._ipps._tcp", start);
      avahi_entry_group_add_service_subtype(printer->ipp_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dnssd_name, "_ipps._tcp", NULL, temp);
    }

    free(temptypes);
  }
#endif /* HAVE_SSL */

 /*
  * Finally _http.tcp (HTTP) for the web interface...
  */

  avahi_entry_group_add_service_strlst(printer->ipp_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dnssd_name, "_http._tcp", NULL, NULL, printer->port, NULL);
  avahi_entry_group_add_service_subtype(printer->ipp_ref, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, printer->dnssd_name, "_http._tcp", NULL, "_printer._sub._http._tcp");

 /*
  * Commit it...
  */

  avahi_entry_group_commit(printer->ipp_ref);
  avahi_threaded_poll_unlock(DNSSDMaster);

  avahi_string_list_free(ipp_txt);
#endif /* HAVE_DNSSD */

  return (1);
}


/*
 * 'respond_http()' - Send a HTTP response.
 */

int					/* O - 1 on success, 0 on failure */
respond_http(
    ippeve_client_t *client,		/* I - Client */
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

  if (!type && !length && code != HTTP_STATUS_OK && code != HTTP_STATUS_SWITCHING_PROTOCOLS)
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
respond_ipp(ippeve_client_t *client,	/* I - Client */
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
    if ((attr = ippFindAttribute(client->response, "status-message", IPP_TAG_TEXT)) != NULL)
      ippSetStringfv(client->response, &attr, 0, message, ap);
    else
      attr = ippAddStringfv(client->response, IPP_TAG_OPERATION, IPP_TAG_TEXT, "status-message", NULL, message, ap);
    va_end(ap);

    formatted = ippGetString(attr, 0, NULL);
  }

  if (formatted)
    fprintf(stderr, "%s %s %s (%s)\n", client->hostname, ippOpString(client->operation_id), ippErrorString(status), formatted);
  else
    fprintf(stderr, "%s %s %s\n", client->hostname, ippOpString(client->operation_id), ippErrorString(status));
}


/*
 * 'respond_unsupported()' - Respond with an unsupported attribute.
 */

static void
respond_unsupported(
    ippeve_client_t   *client,		/* I - Client */
    ipp_attribute_t *attr)		/* I - Atribute */
{
  ipp_attribute_t	*temp;		/* Copy of attribute */


  respond_ipp(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported %s %s%s value.", ippGetName(attr), ippGetCount(attr) > 1 ? "1setOf " : "", ippTagString(ippGetValueTag(attr)));

  temp = ippCopyAttribute(client->response, attr, 0);
  ippSetGroupTag(client->response, &temp, IPP_TAG_UNSUPPORTED_GROUP);
}


/*
 * 'run_printer()' - Run the printer service.
 */

static void
run_printer(ippeve_printer_t *printer)	/* I - Printer */
{
  int		num_fds;		/* Number of file descriptors */
  struct pollfd	polldata[3];		/* poll() data */
  int		timeout;		/* Timeout for poll() */
  ippeve_client_t	*client;		/* New client */


 /*
  * Setup poll() data for the Bonjour service socket and IPv4/6 listeners...
  */

  polldata[0].fd     = printer->ipv4;
  polldata[0].events = POLLIN;

  polldata[1].fd     = printer->ipv6;
  polldata[1].events = POLLIN;

  num_fds = 2;

#ifdef HAVE_DNSSD
  polldata[num_fds   ].fd     = DNSServiceRefSockFD(DNSSDMaster);
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

    if (poll(polldata, (nfds_t)num_fds, timeout) < 0 && errno != EINTR)
    {
      perror("poll() failed");
      break;
    }

    if (polldata[0].revents & POLLIN)
    {
      if ((client = create_client(printer, printer->ipv4)) != NULL)
      {
        _cups_thread_t t = _cupsThreadCreate((_cups_thread_func_t)process_client, client);

        if (t)
        {
          _cupsThreadDetach(t);
        }
        else
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
        _cups_thread_t t = _cupsThreadCreate((_cups_thread_func_t)process_client, client);

        if (t)
        {
          _cupsThreadDetach(t);
        }
        else
	{
	  perror("Unable to create client thread");
	  delete_client(client);
	}
      }
    }

#ifdef HAVE_DNSSD
    if (polldata[2].revents & POLLIN)
      DNSServiceProcessResult(DNSSDMaster);
#endif /* HAVE_DNSSD */

   /*
    * Clean out old jobs...
    */

    clean_jobs(printer);
  }
}


/*
 * 'show_media()' - Show media load state.
 */

static int				/* O - 1 on success, 0 on failure */
show_media(ippeve_client_t  *client)	/* I - Client connection */
{
  ippeve_printer_t *printer = client->printer;
					/* Printer */
  int			i, j,		/* Looping vars */
                        num_ready,	/* Number of ready media */
                        num_sizes,	/* Number of media sizes */
			num_sources,	/* Number of media sources */
                        num_types;	/* Number of media types */
  ipp_attribute_t	*media_col_ready,/* media-col-ready attribute */
                        *media_ready,	/* media-ready attribute */
                        *media_sizes,	/* media-supported attribute */
                        *media_sources,	/* media-source-supported attribute */
                        *media_types,	/* media-type-supported attribute */
                        *input_tray;	/* printer-input-tray attribute */
  ipp_t			*media_col;	/* media-col value */
  const char            *media_size,	/* media value */
                        *media_source,	/* media-source value */
                        *media_type,	/* media-type value */
                        *ready_size,	/* media-col-ready media-size[-name] value */
                        *ready_source,	/* media-col-ready media-source value */
                        *ready_tray,	/* printer-input-tray value */
                        *ready_type;	/* media-col-ready media-type value */
  char			tray_str[1024],	/* printer-input-tray string value */
			*tray_ptr;	/* Pointer into value */
  int			tray_len;	/* Length of printer-input-tray value */
  int			ready_sheets;	/* printer-input-tray sheets value */
  int			num_options = 0;/* Number of form options */
  cups_option_t		*options = NULL;/* Form options */
  static const int	sheets[] =	/* Number of sheets */
  {
    250,
    125,
    50,
    25,
    5,
    0,
    -2
  };


  if (!respond_http(client, HTTP_STATUS_OK, NULL, "text/html", 0))
    return (0);

  html_header(client, printer->name, 0);

  if ((media_col_ready = ippFindAttribute(printer->attrs, "media-col-ready", IPP_TAG_BEGIN_COLLECTION)) == NULL)
  {
    html_printf(client, "<p>Error: No media-col-ready defined for printer.</p>\n");
    html_footer(client);
    return (1);
  }

  media_ready = ippFindAttribute(printer->attrs, "media-ready", IPP_TAG_ZERO);

  if ((media_sizes = ippFindAttribute(printer->attrs, "media-supported", IPP_TAG_ZERO)) == NULL)
  {
    html_printf(client, "<p>Error: No media-supported defined for printer.</p>\n");
    html_footer(client);
    return (1);
  }

  if ((media_sources = ippFindAttribute(printer->attrs, "media-source-supported", IPP_TAG_ZERO)) == NULL)
  {
    html_printf(client, "<p>Error: No media-source-supported defined for printer.</p>\n");
    html_footer(client);
    return (1);
  }

  if ((media_types = ippFindAttribute(printer->attrs, "media-type-supported", IPP_TAG_ZERO)) == NULL)
  {
    html_printf(client, "<p>Error: No media-type-supported defined for printer.</p>\n");
    html_footer(client);
    return (1);
  }

  if ((input_tray = ippFindAttribute(printer->attrs, "printer-input-tray", IPP_TAG_STRING)) == NULL)
  {
    html_printf(client, "<p>Error: No printer-input-tray defined for printer.</p>\n");
    html_footer(client);
    return (1);
  }

  num_ready   = ippGetCount(media_col_ready);
  num_sizes   = ippGetCount(media_sizes);
  num_sources = ippGetCount(media_sources);
  num_types   = ippGetCount(media_types);

  if (num_sources != ippGetCount(input_tray))
  {
    html_printf(client, "<p>Error: Different number of trays in media-source-supported and printer-input-tray defined for printer.</p>\n");
    html_footer(client);
    return (1);
  }

 /*
  * Process form data if present...
  */

  if (printer->web_forms)
    num_options = parse_options(client, &options);

  if (num_options > 0)
  {
   /*
    * WARNING: A real printer/server implementation MUST NOT implement
    * media updates via a GET request - GET requests are supposed to be
    * idempotent (without side-effects) and we obviously are not
    * authenticating access here.  This form is provided solely to
    * enable testing and development!
    */

    char	name[255];		/* Form name */
    const char	*val;			/* Form value */
    pwg_media_t	*media;			/* Media info */

    _cupsRWLockWrite(&printer->rwlock);

    ippDeleteAttribute(printer->attrs, media_col_ready);
    media_col_ready = NULL;

    if (media_ready)
    {
      ippDeleteAttribute(printer->attrs, media_ready);
      media_ready = NULL;
    }

    printer->state_reasons &= (ippeve_preason_t)~(IPPEVE_PREASON_MEDIA_LOW | IPPEVE_PREASON_MEDIA_EMPTY | IPPEVE_PREASON_MEDIA_NEEDED);

    for (i = 0; i < num_sources; i ++)
    {
      media_source = ippGetString(media_sources, i, NULL);

      if (!strcmp(media_source, "auto") || !strcmp(media_source, "manual") || strstr(media_source, "-man") != NULL)
	continue;

      snprintf(name, sizeof(name), "size%d", i);
      if ((media_size = cupsGetOption(name, num_options, options)) != NULL && (media = pwgMediaForPWG(media_size)) != NULL)
      {
        snprintf(name, sizeof(name), "type%d", i);
        if ((media_type = cupsGetOption(name, num_options, options)) != NULL && !*media_type)
          media_type = NULL;

        if (media_ready)
          ippSetString(printer->attrs, &media_ready, ippGetCount(media_ready), media_size);
        else
          media_ready = ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-ready", NULL, media_size);

        media_col = create_media_col(media_size, media_source, media_type, media->width, media->length, -1, -1, -1, -1);

        if (media_col_ready)
          ippSetCollection(printer->attrs, &media_col_ready, ippGetCount(media_col_ready), media_col);
        else
          media_col_ready = ippAddCollection(printer->attrs, IPP_TAG_PRINTER, "media-col-ready", media_col);
        ippDelete(media_col);
      }
      else
        media = NULL;

      snprintf(name, sizeof(name), "level%d", i);
      if ((val = cupsGetOption(name, num_options, options)) != NULL)
        ready_sheets = atoi(val);
      else
        ready_sheets = 0;

      snprintf(tray_str, sizeof(tray_str), "type=sheetFeedAuto%sRemovableTray;mediafeed=%d;mediaxfeed=%d;maxcapacity=%d;level=%d;status=0;name=%s;", !strcmp(media_source, "by-pass-tray") ? "Non" : "", media ? media->length : 0, media ? media->width : 0, strcmp(media_source, "by-pass-tray") ? 250 : 25, ready_sheets, media_source);

      ippSetOctetString(printer->attrs, &input_tray, i, tray_str, (int)strlen(tray_str));

      if (ready_sheets == 0)
      {
        printer->state_reasons |= IPPEVE_PREASON_MEDIA_EMPTY;
        if (printer->active_job)
          printer->state_reasons |= IPPEVE_PREASON_MEDIA_NEEDED;
      }
      else if (ready_sheets < 25 && ready_sheets > 0)
        printer->state_reasons |= IPPEVE_PREASON_MEDIA_LOW;
    }

    if (!media_col_ready)
      media_col_ready = ippAddOutOfBand(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_NOVALUE, "media-col-ready");

    if (!media_ready)
      media_ready = ippAddOutOfBand(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_NOVALUE, "media-ready");

    _cupsRWUnlock(&printer->rwlock);
  }

  if (printer->web_forms)
    html_printf(client, "<form method=\"GET\" action=\"/media\">\n");

  html_printf(client, "<table class=\"form\" summary=\"Media\">\n");
  for (i = 0; i < num_sources; i ++)
  {
    media_source = ippGetString(media_sources, i, NULL);

    if (!strcmp(media_source, "auto") || !strcmp(media_source, "manual") || strstr(media_source, "-man") != NULL)
      continue;

    for (j = 0, ready_size = NULL, ready_type = NULL; j < num_ready; j ++)
    {
      media_col    = ippGetCollection(media_col_ready, j);
      ready_size   = ippGetString(ippFindAttribute(media_col, "media-size-name", IPP_TAG_ZERO), 0, NULL);
      ready_source = ippGetString(ippFindAttribute(media_col, "media-source", IPP_TAG_ZERO), 0, NULL);
      ready_type   = ippGetString(ippFindAttribute(media_col, "media-type", IPP_TAG_ZERO), 0, NULL);

      if (ready_source && !strcmp(ready_source, media_source))
        break;

      ready_source = NULL;
      ready_size   = NULL;
      ready_type   = NULL;
    }

    html_printf(client, "<tr><th>%s:</th>", media_source);

   /*
    * Media size...
    */

    if (printer->web_forms)
    {
      html_printf(client, "<td><select name=\"size%d\"><option value=\"\">None</option>", i);
      for (j = 0; j < num_sizes; j ++)
      {
	media_size = ippGetString(media_sizes, j, NULL);

	html_printf(client, "<option%s>%s</option>", (ready_size && !strcmp(ready_size, media_size)) ? " selected" : "", media_size);
      }
      html_printf(client, "</select>");
    }
    else
      html_printf(client, "<td>%s", ready_size);

   /*
    * Media type...
    */

    if (printer->web_forms)
    {
      html_printf(client, " <select name=\"type%d\"><option value=\"\">None</option>", i);
      for (j = 0; j < num_types; j ++)
      {
	media_type = ippGetString(media_types, j, NULL);

	html_printf(client, "<option%s>%s</option>", (ready_type && !strcmp(ready_type, media_type)) ? " selected" : "", media_type);
      }
      html_printf(client, "</select>");
    }
    else if (ready_type)
      html_printf(client, ", %s", ready_type);

   /*
    * Level/sheets loaded...
    */

    if ((ready_tray = ippGetOctetString(input_tray, i, &tray_len)) != NULL)
    {
      if (tray_len > (int)(sizeof(tray_str) - 1))
        tray_len = (int)sizeof(tray_str) - 1;
      memcpy(tray_str, ready_tray, (size_t)tray_len);
      tray_str[tray_len] = '\0';

      if ((tray_ptr = strstr(tray_str, "level=")) != NULL)
        ready_sheets = atoi(tray_ptr + 6);
      else
        ready_sheets = 0;
    }
    else
      ready_sheets = 0;

    if (printer->web_forms)
    {
      html_printf(client, " <select name=\"level%d\">", i);
      for (j = 0; j < (int)(sizeof(sheets) / sizeof(sheets[0])); j ++)
      {
	if (!strcmp(media_source, "by-pass-tray") && sheets[j] > 25)
	  continue;

	if (sheets[j] < 0)
	  html_printf(client, "<option value=\"%d\"%s>Unknown</option>", sheets[j], sheets[j] == ready_sheets ? " selected" : "");
	else
	  html_printf(client, "<option value=\"%d\"%s>%d sheets</option>", sheets[j], sheets[j] == ready_sheets ? " selected" : "", sheets[j]);
      }
      html_printf(client, "</select></td></tr>\n");
    }
    else if (ready_sheets == 1)
      html_printf(client, ", 1 sheet</td></tr>\n");
    else if (ready_sheets > 0)
      html_printf(client, ", %d sheets</td></tr>\n", ready_sheets);
    else
      html_printf(client, "</td></tr>\n");
  }

  if (printer->web_forms)
  {
    html_printf(client, "<tr><td></td><td><input type=\"submit\" value=\"Update Media\">");
    if (num_options > 0)
      html_printf(client, " <span class=\"badge\" id=\"status\">Media updated.</span>\n");
    html_printf(client, "</td></tr></table></form>\n");

    if (num_options > 0)
      html_printf(client, "<script>\n"
			  "setTimeout(hide_status, 3000);\n"
			  "function hide_status() {\n"
			  "  var status = document.getElementById('status');\n"
			  "  status.style.display = 'none';\n"
			  "}\n"
			  "</script>\n");
  }
  else
    html_printf(client, "</table>\n");

  html_footer(client);

  return (1);
}


/*
 * 'show_status()' - Show printer/system state.
 */

static int				/* O - 1 on success, 0 on failure */
show_status(ippeve_client_t  *client)	/* I - Client connection */
{
  ippeve_printer_t *printer = client->printer;
					/* Printer */
  ippeve_job_t		*job;		/* Current job */
  int			i;		/* Looping var */
  ippeve_preason_t	reason;		/* Current reason */
  static const char * const reasons[] =	/* Reason strings */
  {
    "Other",
    "Cover Open",
    "Input Tray Missing",
    "Marker Supply Empty",
    "Marker Supply Low",
    "Marker Waste Almost Full",
    "Marker Waste Full",
    "Media Empty",
    "Media Jam",
    "Media Low",
    "Media Needed",
    "Moving to Paused",
    "Paused",
    "Spool Area Full",
    "Toner Empty",
    "Toner Low"
  };
  static const char * const state_colors[] =
  {					/* State colors */
    "#0C0",				/* Idle */
    "#EE0",				/* Processing */
    "#C00"				/* Stopped */
  };


  if (!respond_http(client, HTTP_STATUS_OK, NULL, "text/html", 0))
    return (0);

  html_header(client, printer->name, printer->state == IPP_PSTATE_PROCESSING ? 5 : 15);
  html_printf(client, "<h1><img style=\"background: %s; border-radius: 10px; float: left; margin-right: 10px; padding: 10px;\" src=\"/icon.png\" width=\"64\" height=\"64\">%s Jobs</h1>\n", state_colors[printer->state - IPP_PSTATE_IDLE], printer->name);
  html_printf(client, "<p>%s, %d job(s).", printer->state == IPP_PSTATE_IDLE ? "Idle" : printer->state == IPP_PSTATE_PROCESSING ? "Printing" : "Stopped", cupsArrayCount(printer->jobs));
  for (i = 0, reason = 1; i < (int)(sizeof(reasons) / sizeof(reasons[0])); i ++, reason <<= 1)
    if (printer->state_reasons & reason)
      html_printf(client, "\n<br>&nbsp;&nbsp;&nbsp;&nbsp;%s", reasons[i]);
  html_printf(client, "</p>\n");

  if (cupsArrayCount(printer->jobs) > 0)
  {
    _cupsRWLockRead(&(printer->rwlock));

    html_printf(client, "<table class=\"striped\" summary=\"Jobs\"><thead><tr><th>Job #</th><th>Name</th><th>Owner</th><th>Status</th></tr></thead><tbody>\n");
    for (job = (ippeve_job_t *)cupsArrayFirst(printer->jobs); job; job = (ippeve_job_t *)cupsArrayNext(printer->jobs))
    {
      char	when[256],		/* When job queued/started/finished */
	      hhmmss[64];		/* Time HH:MM:SS */

      switch (job->state)
      {
	case IPP_JSTATE_PENDING :
	case IPP_JSTATE_HELD :
	    snprintf(when, sizeof(when), "Queued at %s", time_string(job->created, hhmmss, sizeof(hhmmss)));
	    break;
	case IPP_JSTATE_PROCESSING :
	case IPP_JSTATE_STOPPED :
	    snprintf(when, sizeof(when), "Started at %s", time_string(job->processing, hhmmss, sizeof(hhmmss)));
	    break;
	case IPP_JSTATE_ABORTED :
	    snprintf(when, sizeof(when), "Aborted at %s", time_string(job->completed, hhmmss, sizeof(hhmmss)));
	    break;
	case IPP_JSTATE_CANCELED :
	    snprintf(when, sizeof(when), "Canceled at %s", time_string(job->completed, hhmmss, sizeof(hhmmss)));
	    break;
	case IPP_JSTATE_COMPLETED :
	    snprintf(when, sizeof(when), "Completed at %s", time_string(job->completed, hhmmss, sizeof(hhmmss)));
	    break;
      }

      html_printf(client, "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td></tr>\n", job->id, job->name, job->username, when);
    }
    html_printf(client, "</tbody></table>\n");

    _cupsRWUnlock(&(printer->rwlock));
  }

  html_footer(client);

  return (1);
}


/*
 * 'show_supplies()' - Show printer supplies.
 */

static int				/* O - 1 on success, 0 on failure */
show_supplies(
    ippeve_client_t  *client)		/* I - Client connection */
{
  ippeve_printer_t *printer = client->printer;
					/* Printer */
  int		i,			/* Looping var */
		num_supply;		/* Number of supplies */
  ipp_attribute_t *supply,		/* printer-supply attribute */
		*supply_desc;		/* printer-supply-description attribute */
  int		num_options = 0;	/* Number of form options */
  cups_option_t	*options = NULL;	/* Form options */
  int		supply_len,		/* Length of supply value */
		level;			/* Supply level */
  const char	*supply_value;		/* Supply value */
  char		supply_text[1024],	/* Supply string */
		*supply_ptr;		/* Pointer into supply string */
  static const char * const printer_supply[] =
  {					/* printer-supply values */
    "index=1;class=receptacleThatIsFilled;type=wasteToner;unit=percent;"
        "maxcapacity=100;level=%d;colorantname=unknown;",
    "index=2;class=supplyThatIsConsumed;type=toner;unit=percent;"
        "maxcapacity=100;level=%d;colorantname=black;",
    "index=3;class=supplyThatIsConsumed;type=toner;unit=percent;"
        "maxcapacity=100;level=%d;colorantname=cyan;",
    "index=4;class=supplyThatIsConsumed;type=toner;unit=percent;"
        "maxcapacity=100;level=%d;colorantname=magenta;",
    "index=5;class=supplyThatIsConsumed;type=toner;unit=percent;"
        "maxcapacity=100;level=%d;colorantname=yellow;"
  };
  static const char * const backgrounds[] =
  {					/* Background colors for the supply-level bars */
    "#777 linear-gradient(#333,#777)",
    "#000 linear-gradient(#666,#000)",
    "#0FF linear-gradient(#6FF,#0FF)",
    "#F0F linear-gradient(#F6F,#F0F)",
    "#CC0 linear-gradient(#EE6,#EE0)"
  };
  static const char * const colors[] =	/* Text colors for the supply-level bars */
  {
    "#fff",
    "#fff",
    "#000",
    "#000",
    "#000"
  };


  if (!respond_http(client, HTTP_STATUS_OK, NULL, "text/html", 0))
    return (0);

  html_header(client, printer->name, 0);

  if ((supply = ippFindAttribute(printer->attrs, "printer-supply", IPP_TAG_STRING)) == NULL)
  {
    html_printf(client, "<p>Error: No printer-supply defined for printer.</p>\n");
    html_footer(client);
    return (1);
  }

  num_supply = ippGetCount(supply);

  if ((supply_desc = ippFindAttribute(printer->attrs, "printer-supply-description", IPP_TAG_TEXT)) == NULL)
  {
    html_printf(client, "<p>Error: No printer-supply-description defined for printer.</p>\n");
    html_footer(client);
    return (1);
  }

  if (num_supply != ippGetCount(supply_desc))
  {
    html_printf(client, "<p>Error: Different number of values for printer-supply and printer-supply-description defined for printer.</p>\n");
    html_footer(client);
    return (1);
  }

  if (printer->web_forms)
    num_options = parse_options(client, &options);

  if (num_options > 0)
  {
   /*
    * WARNING: A real printer/server implementation MUST NOT implement
    * supply updates via a GET request - GET requests are supposed to be
    * idempotent (without side-effects) and we obviously are not
    * authenticating access here.  This form is provided solely to
    * enable testing and development!
    */

    char	name[64];		/* Form field */
    const char	*val;			/* Form value */

    _cupsRWLockWrite(&printer->rwlock);

    ippDeleteAttribute(printer->attrs, supply);
    supply = NULL;

    printer->state_reasons &= (ippeve_preason_t)~(IPPEVE_PREASON_MARKER_SUPPLY_EMPTY | IPPEVE_PREASON_MARKER_SUPPLY_LOW | IPPEVE_PREASON_MARKER_WASTE_ALMOST_FULL | IPPEVE_PREASON_MARKER_WASTE_FULL | IPPEVE_PREASON_TONER_EMPTY | IPPEVE_PREASON_TONER_LOW);

    for (i = 0; i < num_supply; i ++)
    {
      snprintf(name, sizeof(name), "supply%d", i);
      if ((val = cupsGetOption(name, num_options, options)) != NULL)
      {
        level = atoi(val);      /* New level */

        snprintf(supply_text, sizeof(supply_text), printer_supply[i], level);
        if (supply)
          ippSetOctetString(printer->attrs, &supply, ippGetCount(supply), supply_text, (int)strlen(supply_text));
        else
          supply = ippAddOctetString(printer->attrs, IPP_TAG_PRINTER, "printer-supply", supply_text, (int)strlen(supply_text));

        if (i == 0)
        {
          if (level == 100)
            printer->state_reasons |= IPPEVE_PREASON_MARKER_WASTE_FULL;
          else if (level > 90)
            printer->state_reasons |= IPPEVE_PREASON_MARKER_WASTE_ALMOST_FULL;
        }
        else
        {
          if (level == 0)
            printer->state_reasons |= IPPEVE_PREASON_TONER_EMPTY;
          else if (level < 10)
            printer->state_reasons |= IPPEVE_PREASON_TONER_LOW;
        }
      }
    }

    _cupsRWUnlock(&printer->rwlock);
  }

  if (printer->web_forms)
    html_printf(client, "<form method=\"GET\" action=\"/supplies\">\n");

  html_printf(client, "<table class=\"form\" summary=\"Supplies\">\n");
  for (i = 0; i < num_supply; i ++)
  {
    supply_value = ippGetOctetString(supply, i, &supply_len);
    if (supply_len > (int)(sizeof(supply_text) - 1))
      supply_len = (int)sizeof(supply_text) - 1;

    memcpy(supply_text, supply_value, (size_t)supply_len);
    supply_text[supply_len] = '\0';

    if ((supply_ptr = strstr(supply_text, "level=")) != NULL)
      level = atoi(supply_ptr + 6);
    else
      level = 50;

    if (printer->web_forms)
      html_printf(client, "<tr><th>%s:</th><td><input name=\"supply%d\" size=\"3\" value=\"%d\"></td>", ippGetString(supply_desc, i, NULL), i, level);
    else
      html_printf(client, "<tr><th>%s:</th>", ippGetString(supply_desc, i, NULL));

    if (level < 10)
      html_printf(client, "<td class=\"meter\"><span class=\"bar\" style=\"background: %s; padding: 5px %dpx;\"></span>&nbsp;%d%%</td></tr>\n", backgrounds[i], level * 2, level);
    else
      html_printf(client, "<td class=\"meter\"><span class=\"bar\" style=\"background: %s; color: %s; padding: 5px %dpx;\">%d%%</span></td></tr>\n", backgrounds[i], colors[i], level * 2, level);
  }

  if (printer->web_forms)
  {
    html_printf(client, "<tr><td></td><td colspan=\"2\"><input type=\"submit\" value=\"Update Supplies\">");
    if (num_options > 0)
      html_printf(client, " <span class=\"badge\" id=\"status\">Supplies updated.</span>\n");
    html_printf(client, "</td></tr>\n</table>\n</form>\n");

    if (num_options > 0)
      html_printf(client, "<script>\n"
			  "setTimeout(hide_status, 3000);\n"
			  "function hide_status() {\n"
			  "  var status = document.getElementById('status');\n"
			  "  status.style.display = 'none';\n"
			  "}\n"
			  "</script>\n");
  }
  else
    html_printf(client, "</table>\n");

  html_footer(client);

  return (1);
}


/*
 * 'time_string()' - Return the local time in hours, minutes, and seconds.
 */

static char *
time_string(time_t tv,			/* I - Time value */
            char   *buffer,		/* I - Buffer */
	    size_t bufsize)		/* I - Size of buffer */
{
  struct tm	date;			/* Local time and date */

  localtime_r(&tv, &date);

  strftime(buffer, bufsize, "%X", &date);

  return (buffer);
}


/*
 * 'usage()' - Show program usage.
 */

static void
usage(int status)			/* O - Exit status */
{
  _cupsLangPuts(stdout, _("Usage: ippeveprinter [options] \"name\""));
  _cupsLangPuts(stdout, _("Options:"));
  _cupsLangPuts(stderr, _("--help                  Show program help"));
  _cupsLangPuts(stderr, _("--no-web-forms          Disable web forms for media and supplies"));
  _cupsLangPuts(stderr, _("--version               Show program version"));
  _cupsLangPuts(stdout, _("-2                      Set 2-sided printing support (default=1-sided)"));
  _cupsLangPuts(stdout, _("-D device-uri           Set the device URI for the printer"));
  _cupsLangPuts(stdout, _("-F output-type/subtype  Set the output format for the printer"));
#ifdef HAVE_SSL
  _cupsLangPuts(stdout, _("-K keypath              Set location of server X.509 certificates and keys."));
#endif /* HAVE_SSL */
  _cupsLangPuts(stdout, _("-M manufacturer         Set manufacturer name (default=Test)"));
  _cupsLangPuts(stdout, _("-P filename.ppd         Load printer attributes from PPD file"));
  _cupsLangPuts(stdout, _("-V version              Set default IPP version"));
  _cupsLangPuts(stdout, _("-a filename.conf        Load printer attributes from conf file"));
  _cupsLangPuts(stdout, _("-c command              Set print command"));
  _cupsLangPuts(stdout, _("-d spool-directory      Set spool directory"));
  _cupsLangPuts(stdout, _("-f type/subtype[,...]   Set supported file types"));
  _cupsLangPuts(stdout, _("-i iconfile.png         Set icon file"));
  _cupsLangPuts(stdout, _("-k                      Keep job spool files"));
  _cupsLangPuts(stdout, _("-l location             Set location of printer"));
  _cupsLangPuts(stdout, _("-m model                Set model name (default=Printer)"));
  _cupsLangPuts(stdout, _("-n hostname             Set hostname for printer"));
  _cupsLangPuts(stdout, _("-p port                 Set port number for printer"));
  _cupsLangPuts(stdout, _("-r subtype,[subtype]    Set DNS-SD service subtype"));
  _cupsLangPuts(stdout, _("-s speed[,color-speed]  Set speed in pages per minute"));
  _cupsLangPuts(stderr, _("-v                      Be verbose"));

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
    ippeve_client_t *client)		/* I - Client */
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

  if ((attr = ippFindAttribute(client->request, "compression", IPP_TAG_ZERO)) != NULL)
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
      fprintf(stderr, "%s %s compression=\"%s\"\n", client->hostname, op_name, compression);

      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "compression-supplied", NULL, compression);

      if (strcmp(compression, "none"))
      {
	if (Verbosity)
	  fprintf(stderr, "Receiving job file with \"%s\" compression.\n", compression);
        httpSetField(client->http, HTTP_FIELD_CONTENT_ENCODING, compression);
      }
    }
  }

 /*
  * Is it a format we support?
  */

  if ((attr = ippFindAttribute(client->request, "document-format", IPP_TAG_ZERO)) != NULL)
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

      fprintf(stderr, "%s %s document-format=\"%s\"\n", client->hostname, op_name, format);

      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format-supplied", NULL, format);
    }
  }
  else
  {
    format = ippGetString(ippFindAttribute(client->printer->attrs, "document-format-default", IPP_TAG_MIMETYPE), 0, NULL);
    if (!format)
      format = "application/octet-stream"; /* Should never happen */

    attr = ippAddString(client->request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, format);
  }

  if (format && !strcmp(format, "application/octet-stream") && (ippGetOperation(client->request) == IPP_OP_PRINT_JOB || ippGetOperation(client->request) == IPP_OP_SEND_DOCUMENT))
  {
   /*
    * Auto-type the file using the first 8 bytes of the file...
    */

    unsigned char	header[8];	/* First 8 bytes of file */

    memset(header, 0, sizeof(header));
    httpPeek(client->http, (char *)header, sizeof(header));

    if (!memcmp(header, "%PDF", 4))
      format = "application/pdf";
    else if (!memcmp(header, "%!", 2))
      format = "application/postscript";
    else if (!memcmp(header, "\377\330\377", 3) && header[3] >= 0xe0 && header[3] <= 0xef)
      format = "image/jpeg";
    else if (!memcmp(header, "\211PNG", 4))
      format = "image/png";
    else if (!memcmp(header, "RAS2", 4))
      format = "image/pwg-raster";
    else if (!memcmp(header, "UNIRAST", 8))
      format = "image/urf";
    else
      format = NULL;

    if (format)
    {
      fprintf(stderr, "%s %s Auto-typed document-format=\"%s\"\n", client->hostname, op_name, format);

      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format-detected", NULL, format);
    }
  }

  if (op != IPP_OP_CREATE_JOB && (supported = ippFindAttribute(client->printer->attrs, "document-format-supported", IPP_TAG_MIMETYPE)) != NULL && !ippContainsString(supported, format))
  {
    respond_unsupported(client, attr);
    valid = 0;
  }

 /*
  * document-name
  */

  if ((attr = ippFindAttribute(client->request, "document-name", IPP_TAG_NAME)) != NULL)
    ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_NAME, "document-name-supplied", NULL, ippGetString(attr, 0, NULL));

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
    ippeve_client_t *client)		/* I - Client */
{
  int			i,		/* Looping var */
			count,		/* Number of values */
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

  if ((attr = ippFindAttribute(client->request, "copies", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER ||
        ippGetInteger(attr, 0) < 1 || ippGetInteger(attr, 0) > 999)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "ipp-attribute-fidelity", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_BOOLEAN)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-hold-until", IPP_TAG_ZERO)) != NULL)
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

  if ((attr = ippFindAttribute(client->request, "job-impressions", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 0)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-name", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 ||
        (ippGetValueTag(attr) != IPP_TAG_NAME &&
	 ippGetValueTag(attr) != IPP_TAG_NAMELANG))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }

    ippSetGroupTag(client->request, &attr, IPP_TAG_JOB);
  }
  else
    ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL, "Untitled");

  if ((attr = ippFindAttribute(client->request, "job-priority", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER ||
        ippGetInteger(attr, 0) < 1 || ippGetInteger(attr, 0) > 100)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-sheets", IPP_TAG_ZERO)) != NULL)
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

  if ((attr = ippFindAttribute(client->request, "media", IPP_TAG_ZERO)) != NULL)
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
      supported = ippFindAttribute(client->printer->attrs, "media-supported", IPP_TAG_KEYWORD);

      if (!ippContainsString(supported, ippGetString(attr, 0, NULL)))
      {
	respond_unsupported(client, attr);
	valid = 0;
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "media-col", IPP_TAG_ZERO)) != NULL)
  {
    ipp_t		*col,		/* media-col collection */
			*size;		/* media-size collection */
    ipp_attribute_t	*member,	/* Member attribute */
			*x_dim,		/* x-dimension */
			*y_dim;		/* y-dimension */
    int			x_value,	/* y-dimension value */
			y_value;	/* x-dimension value */

    if (ippGetCount(attr) != 1 ||
        ippGetValueTag(attr) != IPP_TAG_BEGIN_COLLECTION)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }

    col = ippGetCollection(attr, 0);

    if ((member = ippFindAttribute(col, "media-size-name", IPP_TAG_ZERO)) != NULL)
    {
      if (ippGetCount(member) != 1 ||
	  (ippGetValueTag(member) != IPP_TAG_NAME &&
	   ippGetValueTag(member) != IPP_TAG_NAMELANG &&
	   ippGetValueTag(member) != IPP_TAG_KEYWORD))
      {
	respond_unsupported(client, attr);
	valid = 0;
      }
      else
      {
	supported = ippFindAttribute(client->printer->attrs, "media-supported", IPP_TAG_KEYWORD);

	if (!ippContainsString(supported, ippGetString(member, 0, NULL)))
	{
	  respond_unsupported(client, attr);
	  valid = 0;
	}
      }
    }
    else if ((member = ippFindAttribute(col, "media-size", IPP_TAG_BEGIN_COLLECTION)) != NULL)
    {
      if (ippGetCount(member) != 1)
      {
	respond_unsupported(client, attr);
	valid = 0;
      }
      else
      {
	size = ippGetCollection(member, 0);

	if ((x_dim = ippFindAttribute(size, "x-dimension", IPP_TAG_INTEGER)) == NULL || ippGetCount(x_dim) != 1 ||
	    (y_dim = ippFindAttribute(size, "y-dimension", IPP_TAG_INTEGER)) == NULL || ippGetCount(y_dim) != 1)
	{
	  respond_unsupported(client, attr);
	  valid = 0;
	}
	else
	{
	  x_value   = ippGetInteger(x_dim, 0);
	  y_value   = ippGetInteger(y_dim, 0);
	  supported = ippFindAttribute(client->printer->attrs, "media-size-supported", IPP_TAG_BEGIN_COLLECTION);
	  count     = ippGetCount(supported);

	  for (i = 0; i < count ; i ++)
	  {
	    size  = ippGetCollection(supported, i);
	    x_dim = ippFindAttribute(size, "x-dimension", IPP_TAG_ZERO);
	    y_dim = ippFindAttribute(size, "y-dimension", IPP_TAG_ZERO);

	    if (ippContainsInteger(x_dim, x_value) && ippContainsInteger(y_dim, y_value))
	      break;
	  }

	  if (i >= count)
	  {
	    respond_unsupported(client, attr);
	    valid = 0;
	  }
	}
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "multiple-document-handling", IPP_TAG_ZERO)) != NULL)
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

  if ((attr = ippFindAttribute(client->request, "orientation-requested", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_ENUM ||
        ippGetInteger(attr, 0) < IPP_ORIENT_PORTRAIT ||
        ippGetInteger(attr, 0) > IPP_ORIENT_REVERSE_PORTRAIT)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "page-ranges", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetValueTag(attr) != IPP_TAG_RANGE)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-quality", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_ENUM ||
        ippGetInteger(attr, 0) < IPP_QUALITY_DRAFT ||
        ippGetInteger(attr, 0) > IPP_QUALITY_HIGH)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "printer-resolution", IPP_TAG_ZERO)) != NULL)
  {
    supported = ippFindAttribute(client->printer->attrs, "printer-resolution-supported", IPP_TAG_RESOLUTION);

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_RESOLUTION ||
        !supported)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else
    {
      int	xdpi,			/* Horizontal resolution for job template attribute */
		ydpi,			/* Vertical resolution for job template attribute */
		sydpi;			/* Vertical resolution for supported value */
      ipp_res_t	units,			/* Units for job template attribute */
		sunits;			/* Units for supported value */

      xdpi  = ippGetResolution(attr, 0, &ydpi, &units);
      count = ippGetCount(supported);

      for (i = 0; i < count; i ++)
      {
        if (xdpi == ippGetResolution(supported, i, &sydpi, &sunits) && ydpi == sydpi && units == sunits)
          break;
      }

      if (i >= count)
      {
	respond_unsupported(client, attr);
	valid = 0;
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "sides", IPP_TAG_ZERO)) != NULL)
  {
    const char *sides = ippGetString(attr, 0, NULL);
					/* "sides" value... */

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else if ((supported = ippFindAttribute(client->printer->attrs, "sides-supported", IPP_TAG_KEYWORD)) != NULL)
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
