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
#include <dns_sd.h>
#include <sys/stat.h>


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


/*
 * Structures...
 */

typedef struct _ipp_printer_s		/**** Printer data ****/
{
  int			ipv4,		/* IPv4 listener */
			ipv6;		/* IPv6 listener */
  DNSServiceRef		ipp_ref,	/* Bonjour IPP service */
			printer_ref;	/* Bonjour LPD service */
  TXTRecordRef		ipp_txt;	/* Bonjour IPP TXT record */
  char			*name,		/* printer-name */
			*dnssd_name,	/* printer-dnssd-name */
			*directory,	/* Spool directory */
			*hostname;	/* Hostname */
  int			port;		/* Port */
  ipp_t			*attrs;		/* Static attributes */
  ipp_pstate_t		state;		/* printer-state value */
  _ipp_preasons_t	state_reasons;	/* printer-state-reasons values */  
  cups_array_t		*jobs;		/* Jobs */
  int			next_job_id;	/* Next job-id value */
  _cups_rwlock_t	rwlock;		/* Printer lock */
} _ipp_printer_t;

typedef struct _ipp_job_s		/**** Job data ****/
{
  int			id,		/* Job ID */
			use;		/* Use count */
  char			*name;		/* job-name */
  ipp_jstate_t		state;		/* job-state value */
  time_t		completed;	/* time-at-completed value */
  ipp_t			*attrs;		/* Static attributes */
  int			canceled;	/* Non-zero when job canceled */
  char			*filename;	/* Print file name */
  int			fd;		/* Print file descriptor */
  _ipp_printer_t	*printer;	/* Printer */
} _ipp_job_t;

typedef struct _ipp_client_s		/**** Client data ****/
{
  http_t		http;		/* HTTP connection */
  ipp_t			*request,	/* IPP request */
			*response;	/* IPP response */
  _ipp_printer_t	*printer;	/* Printer */
  _ipp_job_t		*job;		/* Current job, if any */
} _ipp_client_t;


/*
 * Local functions...
 */

static void		clean_jobs(_ipp_printer_t *printer);
static int		compare_jobs(_ipp_job_t *a, _ipp_job_t *b);
static ipp_attribute_t	*copy_attr(ipp_t *to, ipp_attribute_t *attr,
		                   ipp_tag_t group_tag, int quickcopy);
static void		copy_attrs(ipp_t *to, ipp_t *from, cups_array_t *ra,
			           ipp_tag_t group_tag, int quickcopy);
static void		copy_job_attrs(_ipp_client_t *client, _ipp_job_t *job,
			               cups_array_t *ra);
static _ipp_client_t	*create_client(_ipp_printer_t *printer, int fd);
static _ipp_job_t	*create_job(_ipp_client_t *client);
static int		create_listener(int family, int *port);
static _ipp_printer_t	*create_printer(const char *name, const char *location,
			                const char *make, const char *model,
					const char *icon, const char *formats,
					int ppm, int ppm_color, int duplex,
					int port, const char *regtype,
					const char *directory);
static cups_array_t	*create_requested_array(_ipp_client_t *client);
static void		delete_client(_ipp_client_t *client);
static void		delete_job(_ipp_job_t *job);
static void		delete_printer(_ipp_printer_t *printer);
static void		dnssd_callback(DNSServiceRef sdRef,
				       DNSServiceFlags flags,
				       DNSServiceErrorType errorCode,
				       const char *name,
				       const char *regtype,
				       const char *domain,
				       _ipp_printer_t *printer);
static void		ipp_cancel_job(_ipp_client_t *client);
static void		ipp_create_job(_ipp_client_t *client);
static void		ipp_get_job_attributes(_ipp_client_t *client);
static void		ipp_get_jobs(_ipp_client_t *client);
static void		ipp_get_printer_attributes(_ipp_client_t *client);
static void		ipp_print_job(_ipp_client_t *client);
static void		ipp_send_document(_ipp_client_t *client);
static void		ipp_validate_job(_ipp_client_t *client);
static void		*process_client(_ipp_client_t *client);
static int		register_printer(_ipp_printer_t *printer,
			                 const char *make, const char *model,
					 int color, int duplex,
					 const char *regtype);
static void		respond_ipp(_ipp_client_t *client, ipp_status_t status,
			            const char *message, ...)
#ifdef __GNUC__
__attribute__ ((__format__ (__printf__, 3, 4)))
#endif /* __GNUC__ */
;
static void		run_printer(_ipp_printer_t *printer);
static void		usage(void);


/*
 * 'main()' - Main entry to the sample server.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  const char	*opt,			/* Current option character */
		*name = NULL,		/* Printer name */
		*location = "",		/* Location of printer */
		*make = "Test",		/* Manufacturer */
		*model = "Printer",	/* Model */
		*icon = "printer.png",	/* Icon file */
		*formats = "application/pdf,image/jpeg",
	      				/* Supported formats */
		*regtype = "_ipp._tcp";	/* Bonjour service type */
  int		port = 0,		/* Port number (0 = auto) */
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
	  case '2' : /* -2 */
	      duplex = 1;
	      break;

	  case 'M' : /* -M make */
	      i ++;
	      if (i >= argc)
	        usage();
	      make = argv[i];
	      break;

	  case 'd' : /* -d spool-directory */
	      i ++;
	      if (i >= argc)
	        usage();
	      strlcpy(directory, argv[i], sizeof(directory));
	      break;

	  case 'f' : /* -f type/subtype[,type/subtype,...] */
	      i ++;
	      if (i >= argc)
	        usage();
	      formats = argv[i];
	      break;

	  case 'i' : /* -i icon.png */
	      i ++;
	      if (i >= argc)
	        usage();
	      icon = argv[i];
	      break;

	  case 'l' : /* -l location */
	      i ++;
	      if (i >= argc)
	        usage();
	      location = argv[i];
	      break;

	  case 'm' : /* -m model */
	      i ++;
	      if (i >= argc)
	        usage();
	      model = argv[i];
	      break;

	  case 'p' : /* -p port */
	      i ++;
	      if (i >= argc || !isdigit(argv[i][0] & 255))
	        usage();
	      port = atoi(argv[i]);
	      break;

	  case 'r' : /* -r regtype */
	      i ++;
	      if (i >= argc)
	        usage();
	      regtype = argv[i];
	      break;

	  case 's' : /* -s speed[,color-speed] */
	      i ++;
	      if (i >= argc)
	        usage();
	      if (sscanf(argv[i], "%d,%d", &ppm, &ppm_color) < 1)
	        usage();
	      break;

          default : /* Unknown */
	      fprintf(stderr, "Unknown option \"-%c\".\n", *opt);
	      usage();
	}
    }
    else if (!name)
    {
      name = argv[i];
    }
    else
    {
      fprintf(stderr, "Unexpected command-line argument \"%s\"\n", argv[i]);
      usage();
    }

  if (!name)
    usage();

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
      usage();
    }

    printf("Using spool directory \"%s\".\n", directory);
  }

 /*
  * Create the printer...
  */

  if ((printer = create_printer(name, location, make, model, icon, formats, ppm,
                                ppm_color, duplex, port, regtype,
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
 * 'copy_attr()' - Copy a single attribute.
 */

static ipp_attribute_t *		/* O - New attribute */
copy_attr(ipp_t           *to,		/* O - Destination request/response */
          ipp_attribute_t *attr,	/* I - Attribute to copy */
          ipp_tag_t       group_tag,	/* I - Group to put the copy in */
          int             quickcopy)	/* I - Do a quick copy? */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*toattr;	/* Destination attribute */


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
 * 'copy_attrs()' - Copy attributes from one request to another.
 */

static void
copy_attrs(ipp_t        *to,		/* I - Destination request */
	   ipp_t        *from,		/* I - Source request */
	   cups_array_t *ra,		/* I - Requested attributes */
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
      copy_attr(to, fromattr, quickcopy, fromattr->group_tag);
  }
}


/*
 * 'copy_job_attrs()' - Copy job attributes to the response.
 */

static void
copy_job_attrs(_ipp_client_t *client,	/* I - Client */
               _ipp_job_t    *job,	/* I - Job */
	       cups_array_t  *ra)	/* I - requested-attributes */
{
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
	  ippAddString(client->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
		       "job-state-reasons", NULL, "none");
	  break;

      case IPP_JOB_HELD :
	  if (ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_ZERO))
	    ippAddString(client->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
			 "job-state-reasons", NULL, "job-hold-until-specified");
	  else
	    ippAddString(client->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
			 "job-state-reasons", NULL, "job-incoming");
	  break;

      case IPP_JOB_PROCESSING :
	  if (job->canceled)
	    ippAddString(client->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
			 "job-state-reasons", NULL, "processing-to-stop-point");
	  else
	    ippAddString(client->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
			 "job-state-reasons", NULL, "job-printing");
	  break;

      case IPP_JOB_STOPPED :
	  ippAddString(client->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
		       "job-state-reasons", NULL, "job-stopped");
	  break;

      case IPP_JOB_CANCELED :
	  ippAddString(client->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
		       "job-state-reasons", NULL, "job-canceled-by-user");
	  break;

      case IPP_JOB_ABORTED :
	  ippAddString(client->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
		       "job-state-reasons", NULL, "aborted-by-system");
	  break;

      case IPP_JOB_COMPLETED :
	  ippAddString(client->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
		       "job-state-reasons", NULL, "job-completed-successfully");
	  break;
    }
  }

  copy_attrs(client->response, job->attrs, ra, 0, IPP_TAG_ZERO);
}


/*
 * 'create_client()' - Accept a new network connection and create a client
 *                     object.
 */

static _ipp_client_t *			/* O - Client */
create_client(_ipp_printer_t *printer,	/* I - Printer */
              int            fd)	/* I - Listen socket */
{
}


/*
 * 'create_job()' - Create a new job object from a Print-Job or Create-Job
 *                  request.
 */

static _ipp_job_t *			/* O - Job */
create_job(_ipp_client_t *client)	/* I - Client */
{
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

  if (!port)
  {
   /*
    * Get the auto-assigned port number for the IPv4 socket...
    */

    addrlen = sizeof(address);
    if (getsockname(sock, (struct sockaddr *)&address, &addrlen))
      *port = 8631;
    else if (family == AF_INET)
      *port = ntohl(address.ipv4.sin_port);
    else
      *port = ntohl(address.ipv6.sin6_port);
  }

  memset(&address, 0, sizeof(address));
  if (family == AF_INET)
  {
    address.ipv4.sin_family = family;
    address.ipv4.sin_port   = htonl(*port);
  }
  else
  {
    address.ipv6.sin6_family = family;
    address.ipv6.sin6_port   = htonl(*port);
  }

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
 * 'create_printer()' - Create, register, and listen for connections to a
 *                      printer object.
 */

static _ipp_printer_t *			/* O - Printer */
create_printer(const char *name,	/* I - printer-name */
	       const char *location,	/* I - printer-location */
	       const char *make,	/* I - printer-make-and-model */
	       const char *model,	/* I - printer-make-and-model */
	       const char *icon,	/* I - printer-icons */
	       const char *formats,	/* I - document-format-supported */
	       int        ppm,		/* I - Pages per minute in grayscale */
	       int        ppm_color,	/* I - Pages per minute in color (0 for gray) */
	       int        duplex,	/* I - 1 = duplex, 0 = simplex */
	       int        port,		/* I - Port for listeners or 0 for auto */
	       const char *regtype,	/* I - Bonjour service type */
	       const char *directory)	/* I - Spool directory */
{
  _ipp_printer_t	*printer;	/* Printer */
  char			hostname[256],	/* Hostname */
			uri[1024];	/* Printer URI */


 /*
  * Allocate memory for the printer...
  */

  if ((printer = calloc(1, sizeof(_ipp_printer_t))) == NULL)
    return (NULL);

  printer->name          = _cupsStrAlloc(name);
  printer->dnssd_name    = _cupsStrRetain(printer->name);
  printer->directory     = _cupsStrAlloc(directory);
  printer->hostname      = _cupsStrAlloc(httpGetHostname(NULL, hostname,
                                                         sizeof(hostname)));
  printer->port          = port;
  printer->state         = IPP_PRINTER_IDLE;
  printer->state_reasons = _IPP_PRINTER_NONE;
  printer->jobs          = cupsArrayNew((cups_array_func_t)compare_jobs, NULL);
  printer->next_job_id   = 1;

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
  * Register the printer with Bonjour...
  */

  if (register_printer(printer, make, model, ppm_color != 0, duplex, regtype))
  {
    fputs("Unable to register printer with Bonjour.\n", stderr);
    goto bad_printer;
  }

 /*
  * Create the printer attributes...
  */

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
 * 'delete_client()' - Close the socket and free all memory used by a client
 *                     object.
 */

static void
delete_client(_ipp_client_t *client)	/* I - Client */
{
}


/*
 * 'delete_job()' - Remove from the printer and free all memory used by a job
 *                  object.
 */

static void
delete_job(_ipp_job_t *job)		/* I - Job */
{
}


/*
 * 'delete_printer()' - Unregister, close listen sockets, and free all memory
 *                      used by a printer object.
 */

static void
delete_printer(_ipp_printer_t *printer)	/* I - Printer */
{
}


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
}


/*
 * 'ipp_cancel_job()' - Cancel a job.
 */

static void
ipp_cancel_job(_ipp_client_t *client)	/* I - Client */
{
}


/*
 * 'ipp_create_job()' - Create a job object.
 */

static void
ipp_create_job(_ipp_client_t *client)	/* I - Client */
{
}


/*
 * 'ipp_get_job_attributes()' - Get the attributes for a job object.
 */

static void
ipp_get_job_attributes(
    _ipp_client_t *client)		/* I - Client */
{
}


/*
 * 'ipp_get_jobs()' - Get a list of job objects.
 */

static void
ipp_get_jobs(_ipp_client_t *client)	/* I - Client */
{
}


/*
 * 'ipp_get_printer_attributes()' - Get the attributes for a printer object.
 */

static void
ipp_get_printer_attributes(
    _ipp_client_t *client)		/* I - Client */
{
}


/*
 * 'ipp_print_job()' - Create a job object with an attached document.
 */

static void
ipp_print_job(_ipp_client_t *client)	/* I - Client */
{
}


/*
 * 'ipp_send_document()' - Add an attached document to a job object created with
 *                         Create-Job.
 */

static void
ipp_send_document(_ipp_client_t *client)/* I - Client */
{
}


/*
 * 'ipp_validate_job()' - Validate job creation attributes.
 */

static void
ipp_validate_job(_ipp_client_t *client)	/* I - Client */
{
}


/*
 * 'process_client()' - Process client requests on a thread.
 */

static void *				/* O - Exit status */
process_client(_ipp_client_t *client)	/* I - Client */
{
}


/*
 * 'register_printer()' - Register a printer object via Bonjour.
 */

static int				/* O - 0 on success, -1 on error */
register_printer(
    _ipp_printer_t *printer,		/* I - Printer */
    const char     *make,		/* I - Manufacturer */
    const char     *model,		/* I - Model name */
    int            color,		/* I - 1 = color, 0 = monochrome */
    int            duplex,		/* I - 1 = duplex, 0 = simplex */
    const char     *regtype)		/* I - Service type */
{
}


/*
 * 'respond_ipp()' - Respond to an IPP request.
 */

static void
respond_ipp(_ipp_client_t *client,	/* I - Client */
            ipp_status_t  status,	/* I - status-code */
	    const char    *message,	/* I - printf-style status-message */
	    ...)			/* I - Additional args as needed */
{
}


/*
 * 'run_printer()' - Run the printer service.
 */

static void
run_printer(_ipp_printer_t *printer)	/* I - Printer */
{
}


/*
 * 'usage()' - Show program usage.
 */

static void
usage(void)
{
  puts("Usage: ippserver [options] \"name\"");
  puts("");
  puts("Options:");
  puts("-p port           Port number (default=auto)");
  puts("-r regtype        Bonjour service type (default=_ipp._tcp)");
  puts("-i iconfile.png   PNG icon file (default=printer.png)");
  puts("-");
  puts("-");
  puts("-");
  puts("-");
  puts("-");
  puts("-");
  puts("-");
  puts("-");

  exit(1);
}


/*
 * End of "$Id$".
 */
