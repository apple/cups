/*
 * "$Id$"
 *
 *   Utility to find IPP printers via Bonjour/DNS-SD and optionally run
 *   commands such as IPP and Bonjour conformance tests.  This tool is
 *   inspired by the UNIX "find" command, thus its name.
 *
 *   Copyright 2008-2013 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Usage:
 *
 *   ./ippfind [options] regtype[,subtype][.domain.] ... [expression]
 *   ./ippfind [options] name[.regtype[.domain.]] ... [expression]
 *   ./ippfind --help
 *   ./ippfind --version
 *
 *  Supported regtypes are:
 *
 *    _http._tcp                      - HTTP (RFC 2616)
 *    _https._tcp                     - HTTPS (RFC 2818)
 *    _ipp._tcp                       - IPP (RFC 2911)
 *    _ipps._tcp                      - IPPS (pending)
 *    _printer._tcp                   - LPD (RFC 1179)
 *
 *  Exit Codes:
 *
 *    0 if result for all processed expressions is true
 *    1 if result of any processed expression is false
 *    2 if browsing or any query or resolution failed
 *    3 if an undefined option or invalid expression was specified
 *
 *  Options:
 *
 *    --help                          - Show program help
 *    --version                       - Show program version
 *    -4                              - Use IPv4 when listing
 *    -6                              - Use IPv6 when listing
 *    -T seconds                      - Specify browse timeout (default 10
 *                                      seconds)
 *    -V version                      - Specify IPP version (1.1, 2.0, 2.1, 2.2)
 *
 *  "expression" is any of the following:
 *
 *   -d regex
 *   --domain regex                   - True if the domain matches the given
 *                                      regular expression.
 *
 *   -e utility [argument ...] ;
 *   --exec utility [argument ...] ;  - Executes the specified program; "{}"
 *                                      does a substitution (see below)
 *
 *   -l
 *   --ls                             - Lists attributes returned by
 *                                      Get-Printer-Attributes for IPP printers,
 *                                      ???? of HEAD request for HTTP URLs)
 *                                      True if resource is accessible, false
 *                                      otherwise.
 *
 *   --local                          - True if the service is local to this
 *                                      computer.
 *
 *   -n regex
 *   --name regex                     - True if the name matches the given
 *                                      regular expression.
 *
 *   --path regex                     - True if the URI resource path matches
 *                                      the given regular expression.
 *
 *   -p
 *   --print                          - Prints the URI of found printers (always
 *                                      true, default if -e, -l, -p, -q, or -s
 *                                      is not specified.
 *
 *   -q
 *   --quiet                          - Quiet mode (just return exit code)
 *
 *   -r
 *   --remote                         - True if the service is not local to this
 *                                      computer.
 *
 *   -s
 *   --print-name                     - Prints the service name of found
 *                                      printers.
 *
 *   -t key
 *   --txt key                        - True if the TXT record contains the
 *                                      named key
 *
 *   --txt-* regex                    - True if the TXT record contains the
 *                                      named key and matches the given regular
 *                                      expression.
 *
 *   -u regex
 *   --uri regex                      - True if the URI matches the given
 *                                      regular expression.
 *
 * Expressions may also contain modifiers:
 *
 *   ( expression )                  - Group the result of expressions.
 *
 *   ! expression
 *   --not expression                - Unary NOT
 *
 *   --false                         - Always false
 *   --true                          - Always true
 *
 *   expression expression
 *   expression --and expression
 *   expression -a expression        - Logical AND.
 *
 *   expression -o expression
 *   expression --or expression      - Logical OR.
 *
 * The substitutions for {} are:
 *
 *   {}                              - URI
 *   {service_domain}                - Domain name
 *   {service_hostname}              - FQDN
 *   {service_name}                  - Service name
 *   {service_port}                  - Port number
 *   {service_regtype}               - DNS-SD registration type
 *   {service_scheme}                - URI scheme for DNS-SD registration type
 *   {service_uri}                   - URI
 *   {txt_*}                         - Value of TXT record key
 *
 * These variables are also set in the environment for executed programs:
 *
 *   IPPFIND_SERVICE_DOMAIN          - Domain name
 *   IPPFIND_SERVICE_HOSTNAME        - FQDN
 *   IPPFIND_SERVICE_NAME            - Service name
 *   IPPFIND_SERVICE_PORT            - Port number
 *   IPPFIND_SERVICE_REGTYPE         - DNS-SD registration type
 *   IPPFIND_SERVICE_SCHEME          - URI scheme for DNS-SD registration type
 *   IPPFIND_SERVICE_URI             - URI
 *   IPPFIND_TXT_*                   - Values of TXT record key (uppercase)
 *
 * Contents:
 *
 */

/*
 * Include necessary headers.
 */

#include <cups/cups-private.h>
#include <regex.h>
#ifdef HAVE_DNSSD
#  include <dns_sd.h>
#elif defined(HAVE_AVAHI)
#  include <avahi-client/client.h>
#  include <avahi-client/lookup.h>
#  include <avahi-common/simple-watch.h>
#  include <avahi-common/domain.h>
#  include <avahi-common/error.h>
#  include <avahi-common/malloc.h>
#  define kDNSServiceMaxDomainName AVAHI_DOMAIN_NAME_MAX
#endif /* HAVE_DNSSD */


/*
 * Structures...
 */

typedef enum ippfind_exit_e		/* Exit codes */
{
  IPPFIND_EXIT_OK = 0,			/* OK and result is true */
  IPPFIND_EXIT_FALSE,			/* OK but result is false*/
  IPPFIND_EXIT_BONJOUR,			/* Browse/resolve failure */
  IPPFIND_EXIT_SYNTAX			/* Bad option or syntax error */
} ippfind_exit_t;

typedef enum ippfind_op_e		/* Operations for expressions */
{
  IPPFIND_OP_NONE,			/* No operation */
  IPPFIND_OP_AND,			/* Logical AND of all children */
  IPPFIND_OP_OR,			/* Logical OR of all children */
  IPPFIND_OP_TRUE,			/* Always true */
  IPPFIND_OP_FALSE,			/* Always false */
  IPPFIND_OP_DOMAIN_REGEX,		/* Domain matches regular expression */
  IPPFIND_OP_NAME_REGEX,		/* Name matches regular expression */
  IPPFIND_OP_PATH_REGEX,		/* Path matches regular expression */
  IPPFIND_OP_TXT_EXISTS,		/* TXT record key exists */
  IPPFIND_OP_TXT_REGEX,			/* TXT record key matches regular expression */
  IPPFIND_OP_URI_REGEX,			/* URI matches regular expression */

  IPPFIND_OP_OUTPUT = 100,		/* Output operations */
  IPPFIND_OP_EXEC,			/* Execute when true */
  IPPFIND_OP_LIST,			/* List when true */
  IPPFIND_OP_PRINT_NAME,		/* Print URI when true */
  IPPFIND_OP_PRINT_URI,			/* Print name when true */
  IPPFIND_OP_QUIET,			/* No output when true */
} ippfind_op_t;

typedef struct ippfind_expr_s		/* Expression */
{
  struct ippfind_expr_s
		*prev,			/* Previous expression */
		*next,			/* Next expression */
		*parent,		/* Parent expressions */
		*child;			/* Child expressions */
  ippfind_op_t	op;			/* Operation code (see above) */
  int		invert;			/* Invert the result */
  char		*key;			/* TXT record key */
  regex_t	re;			/* Regular expression for matching */
} ippfind_expr_t;

typedef struct ippfind_srv_s		/* Service information */
{
#ifdef HAVE_DNSSD
  DNSServiceRef	ref;			/* Service reference for query */
#elif defined(HAVE_AVAHI)
  AvahiServiceResolver *ref;		/* Resolver */
#endif /* HAVE_DNSSD */
  char		*name,			/* Service name */
		*domain,		/* Domain name */
		*regtype,		/* Registration type */
		*fullName,		/* Full name */
		*host,			/* Hostname */
		*uri;			/* URI */
  int		num_txt;		/* Number of TXT record keys */
  cups_option_t	*txt;			/* TXT record keys */
  int		port,			/* Port number */
		is_local,		/* Is a local service? */
		is_processed,		/* Did we process the service? */
		is_resolved;		/* Got the resolve data? */
  time_t	resolve_time;		/* Time we started the resolve */
} ippfind_srv_t;


/*
 * Local globals...
 */

#ifdef HAVE_DNSSD
static DNSServiceRef dnssd_ref;		/* Master service reference */
#elif defined(HAVE_AVAHI)
static AvahiClient *avahi_client = NULL;/* Client information */
static int	avahi_got_data = 0;	/* Got data from poll? */
static AvahiSimplePoll *avahi_poll = NULL;
					/* Poll information */
#endif /* HAVE_DNSSD */

static int	address_family = AF_UNSPEC;
					/* Address family for LIST */
static int	bonjour_error = 0;	/* Error browsing/resolving? */
static int	ipp_version = 20;	/* IPP version for LIST */
static double	timeout = 10;		/* Timeout in seconds */


/*
 * Local functions...
 */

#ifdef HAVE_DNSSD
static void		browse_callback(DNSServiceRef sdRef,
			                DNSServiceFlags flags,
				        uint32_t interfaceIndex,
				        DNSServiceErrorType errorCode,
				        const char *serviceName,
				        const char *regtype,
				        const char *replyDomain, void *context)
					__attribute__((nonnull(1,5,6,7,8)));
static void		browse_local_callback(DNSServiceRef sdRef,
					      DNSServiceFlags flags,
					      uint32_t interfaceIndex,
					      DNSServiceErrorType errorCode,
					      const char *serviceName,
					      const char *regtype,
					      const char *replyDomain,
					      void *context)
					      __attribute__((nonnull(1,5,6,7,8)));
#elif defined(HAVE_AVAHI)
static void		browse_callback(AvahiServiceBrowser *browser,
					AvahiIfIndex interface,
					AvahiProtocol protocol,
					AvahiBrowserEvent event,
					const char *serviceName,
					const char *regtype,
					const char *replyDomain,
					AvahiLookupResultFlags flags,
					void *context);
static void		client_callback(AvahiClient *client,
					AvahiClientState state,
					void *context);
#endif /* HAVE_AVAHI */

static int		compare_services(ippfind_srv_t *a, ippfind_srv_t *b);
static ippfind_srv_t	*get_service(cups_array_t *services,
				     const char *serviceName,
				     const char *regtype,
				     const char *replyDomain)
				     __attribute__((nonnull(1,2,3,4)));
#ifdef HAVE_DNSSD
static void		resolve_callback(DNSServiceRef sdRef,
			                 DNSServiceFlags flags,
				         uint32_t interfaceIndex,
				         DNSServiceErrorType errorCode,
				         const char *fullName,
				         const char *hostTarget, uint16_t port,
				         uint16_t txtLen,
				         const unsigned char *txtRecord,
				         void *context);
				         __attribute__((nonnull(1,5,6,9, 10)));
#elif defined(HAVE_AVAHI)
static int		poll_callback(struct pollfd *pollfds,
			              unsigned int num_pollfds, int timeout,
			              void *context);
static void		resolve_callback(AvahiServiceResolver *res,
					 AvahiIfIndex interface,
					 AvahiProtocol protocol,
					 AvahiBrowserEvent event,
					 const char *serviceName,
					 const char *regtype,
					 const char *replyDomain,
					 const char *host_name,
					 uint16_t port,
					 AvahiStringList *txt,
					 AvahiLookupResultFlags flags,
					 void *context);
#endif /* HAVE_DNSSD */
static void		set_service_uri(ippfind_srv_t *service);
static void		show_usage(void) __attribute__((noreturn));
static void		show_version(void) __attribute__((noreturn));
static void		unquote(char *dst, const char *src, size_t dstsize)
			    __attribute__((nonnull(1,2)));


/*
 * 'main()' - Browse for printers.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		fd;			/* File descriptor for Bonjour */
  cups_array_t	*services;		/* Service array */
  ippfind_srv_t	*service;		/* Current service */


 /*
  * Initialize the locale...
  */

  _cupsSetLocale(argv);

 /*
  * Create an array to track services...
  */

  services = cupsArrayNew((cups_array_func_t)compare_services, NULL);

 /*
  * Start up masters for browsing/resolving...
  */

#ifdef HAVE_DNSSD
  if (DNSServiceCreateConnection(&dnssd_ref) != kDNSServiceErr_NoError)
  {
    perror("ERROR: Unable to create service connection");
    return (IPPFIND_EXIT_BONJOUR);
  }

  fd = DNSServiceRefSockFD(dnssd_ref);

#elif defined(HAVE_AVAHI)
#endif /* HAVE_DNSSD */

#if 0
  int		i;			/* Looping var */
  DNSServiceRef	main_ref,		/* Main service reference */
		ipp_ref;		/* IPP service reference */
  int		fd;			/* Main file descriptor */
  fd_set	input;			/* Input set for select() */
  struct timeval timeout;		/* Timeout for select() */
  cups_array_t	*devices;		/* Device array */
  ippfind_srv_t	*device;		/* Current device */
  http_t	*http;			/* Connection to printer */
  ipp_t		*request,		/* Get-Printer-Attributes request */
		*response;		/* Get-Printer-Attributes response */
  ipp_attribute_t *attr;		/* IPP attribute in response */
  const char	*version,		/* Version supported */
		*testfile;		/* Test file to use */
  int		ipponly = 0,		/* Do IPP tests only? */
		snmponly = 0;		/* Do SNMP walk only? */


  for (i = 1; i < argc; i ++)
    if (!strcmp(argv[i], "snmp"))
      snmponly = 1;
    else if (!strcmp(argv[i], "ipp"))
      ipponly = 1;
    else
    {
      puts("Usage: ./ipp-printers [{ipp | snmp}]");
      return (1);
    }

 /*
  * Create an array to track devices...
  */

  devices = cupsArrayNew((cups_array_func_t)compare_services, NULL);

 /*
  * Browse for different kinds of printers...
  */

  if (DNSServiceCreateConnection(&main_ref) != kDNSServiceErr_NoError)
  {
    perror("ERROR: Unable to create service connection");
    return (1);
  }

  fd = DNSServiceRefSockFD(main_ref);

  ipp_ref = main_ref;
  DNSServiceBrowse(&ipp_ref, kDNSServiceFlagsShareConnection, 0,
                   "_ipp._tcp", NULL, browse_callback, devices);

 /*
  * Loop until we are killed...
  */

  progress();

  for (;;)
  {
    FD_ZERO(&input);
    FD_SET(fd, &input);

    timeout.tv_sec  = 2;
    timeout.tv_usec = 500000;

    if (select(fd + 1, &input, NULL, NULL, &timeout) <= 0)
    {
      time_t curtime = time(NULL);

      for (device = (ippfind_srv_t *)cupsArrayFirst(devices);
           device;
	   device = (ippfind_srv_t *)cupsArrayNext(devices))
        if (!device->got_resolve)
        {
          if (!device->ref)
            break;

          if ((curtime - device->resolve_time) > 10)
          {
            device->got_resolve = -1;
	    fprintf(stderr, "\rUnable to resolve \"%s\": timeout\n",
		    device->name);
	    progress();
	  }
          else
            break;
        }

      if (!device)
        break;
    }

    if (FD_ISSET(fd, &input))
    {
     /*
      * Process results of our browsing...
      */

      progress();
      DNSServiceProcessResult(main_ref);
    }
    else
    {
     /*
      * Query any devices we've found...
      */

      DNSServiceErrorType	status;	/* DNS query status */
      int			count;	/* Number of queries */


      for (device = (ippfind_srv_t *)cupsArrayFirst(devices), count = 0;
           device;
	   device = (ippfind_srv_t *)cupsArrayNext(devices))
      {
        if (!device->ref && !device->sent)
	{
	 /*
	  * Found the device, now get the TXT record(s) for it...
	  */

          if (count < 50)
	  {
	    device->resolve_time = time(NULL);
	    device->ref          = main_ref;

	    status = DNSServiceResolve(&(device->ref),
				       kDNSServiceFlagsShareConnection,
				       0, device->name, device->regtype,
				       device->domain, resolve_callback,
				       device);
            if (status != kDNSServiceErr_NoError)
            {
	      fprintf(stderr, "\rUnable to resolve \"%s\": %d\n",
	              device->name, status);
	      progress();
	    }
	    else
	      count ++;
          }
	}
	else if (!device->sent && device->got_resolve)
	{
	 /*
	  * Got the TXT records, now report the device...
	  */

	  DNSServiceRefDeallocate(device->ref);
	  device->ref  = 0;
	  device->sent = 1;
        }
      }
    }
  }

#ifndef DEBUG
  fprintf(stderr, "\rFound %d printers. Now querying for capabilities...\n",
          cupsArrayCount(devices));
#endif /* !DEBUG */

  puts("#!/bin/sh -x");
  puts("test -d results && rm -rf results");
  puts("mkdir results");
  puts("CUPS_DEBUG_LEVEL=6; export CUPS_DEBUG_LEVEL");
  puts("CUPS_DEBUG_FILTER='^(ipp|http|_ipp|_http|cupsGetResponse|cupsSend|"
       "cupsWrite|cupsDo).*'; export CUPS_DEBUG_FILTER");

  for (device = (ippfind_srv_t *)cupsArrayFirst(devices);
       device;
       device = (ippfind_srv_t *)cupsArrayNext(devices))
  {
    if (device->got_resolve <= 0 || device->cups_shared)
      continue;

#ifdef DEBUG
    fprintf(stderr, "Checking \"%s\" (got_resolve=%d, cups_shared=%d, uri=%s)\n",
            device->name, device->got_resolve, device->cups_shared, device->uri);
#else
    fprintf(stderr, "Checking \"%s\"...\n", device->name);
#endif /* DEBUG */

    if ((http = httpConnect(device->host, device->port)) == NULL)
    {
      fprintf(stderr, "Failed to connect to \"%s\": %s\n", device->name,
              cupsLastErrorString());
      continue;
    }

    request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL,
                 device->uri);

    response = cupsDoRequest(http, request, device->rp);

    if (cupsLastError() > IPP_OK_SUBST)
      fprintf(stderr, "Failed to query \"%s\": %s\n", device->name,
              cupsLastErrorString());
    else
    {
      if ((attr = ippFindAttribute(response, "ipp-versions-supported",
				   IPP_TAG_KEYWORD)) != NULL)
      {
	version = attr->values[0].string.text;

	for (i = 1; i < attr->num_values; i ++)
	  if (strcmp(attr->values[i].string.text, version) > 0)
	    version = attr->values[i].string.text;
      }
      else
	version = "1.0";

      testfile = NULL;

      if ((attr = ippFindAttribute(response, "document-format-supported",
                                   IPP_TAG_MIMETYPE)) != NULL)
      {
       /*
        * Figure out the test file for printing, preferring PDF and PostScript
        * over JPEG and plain text...
        */

        for (i = 0; i < attr->num_values; i ++)
        {
          if (!strcasecmp(attr->values[i].string.text, "application/pdf"))
          {
            testfile = "testfile.pdf";
            break;
          }
          else if (!strcasecmp(attr->values[i].string.text,
                               "application/postscript"))
            testfile = "testfile.ps";
          else if (!strcasecmp(attr->values[i].string.text, "image/jpeg") &&
                   !testfile)
            testfile = "testfile.jpg";
          else if (!strcasecmp(attr->values[i].string.text, "text/plain") &&
                   !testfile)
            testfile = "testfile.txt";
          else if (!strcasecmp(attr->values[i].string.text,
                               "application/vnd.hp-PCL") && !testfile)
            testfile = "testfile.pcl";
        }

        if (!testfile)
        {
          fprintf(stderr,
                  "Printer \"%s\" reports the following IPP file formats:\n",
                  device->name);
          for (i = 0; i < attr->num_values; i ++)
            fprintf(stderr, "    \"%s\"\n", attr->values[i].string.text);
        }
      }

      if (!testfile && device->pdl)
      {
	char	*pdl,			/* Copy of pdl string */
		*start, *end;		/* Pointers into pdl string */


        pdl = strdup(device->pdl);
	for (start = device->pdl; start && *start; start = end)
	{
	  if ((end = strchr(start, ',')) != NULL)
	    *end++ = '\0';

	  if (!strcasecmp(start, "application/pdf"))
	  {
	    testfile = "testfile.pdf";
	    break;
	  }
	  else if (!strcasecmp(start, "application/postscript"))
	    testfile = "testfile.ps";
	  else if (!strcasecmp(start, "image/jpeg") && !testfile)
	    testfile = "testfile.jpg";
	  else if (!strcasecmp(start, "text/plain") && !testfile)
	    testfile = "testfile.txt";
	  else if (!strcasecmp(start, "application/vnd.hp-PCL") && !testfile)
	    testfile = "testfile.pcl";
	}
	free(pdl);

        if (testfile)
        {
	  fprintf(stderr,
		  "Using \"%s\" for printer \"%s\" based on TXT record pdl "
		  "info.\n", testfile, device->name);
        }
        else
        {
	  fprintf(stderr,
		  "Printer \"%s\" reports the following TXT file formats:\n",
		  device->name);
	  fprintf(stderr, "    \"%s\"\n", device->pdl);
	}
      }

      if (!device->ty &&
	  (attr = ippFindAttribute(response, "printer-make-and-model",
				   IPP_TAG_TEXT)) != NULL)
	device->ty = strdup(attr->values[0].string.text);

      if (strcmp(version, "1.0") && testfile && device->ty)
      {
	char		filename[1024],	/* Filename */
			*fileptr;	/* Pointer into filename */
	const char	*typtr;		/* Pointer into ty */

        if (!strncasecmp(device->ty, "DeskJet", 7) ||
            !strncasecmp(device->ty, "DesignJet", 9) ||
            !strncasecmp(device->ty, "OfficeJet", 9) ||
            !strncasecmp(device->ty, "Photosmart", 10))
          strlcpy(filename, "HP_", sizeof(filename));
        else
          filename[0] = '\0';

	fileptr = filename + strlen(filename);

        if (!strncasecmp(device->ty, "Lexmark International Lexmark", 29))
          typtr = device->ty + 22;
        else
          typtr = device->ty;

	while (*typtr && fileptr < (filename + sizeof(filename) - 1))
	{
	  if (isalnum(*typtr & 255) || *typtr == '-')
	    *fileptr++ = *typtr++;
	  else
	  {
	    *fileptr++ = '_';
	    typtr++;
	  }
	}

	*fileptr = '\0';

        printf("# %s\n", device->name);
        printf("echo \"Testing %s...\"\n", device->name);

        if (!ipponly)
        {
	  printf("echo \"snmpwalk -c public -v 1 -Cc %s 1.3.6.1.2.1.25 "
	         "1.3.6.1.2.1.43 1.3.6.1.4.1.2699.1\" > results/%s.snmpwalk\n",
	         device->host, filename);
	  printf("snmpwalk -c public -v 1 -Cc %s 1.3.6.1.2.1.25 "
	         "1.3.6.1.2.1.43 1.3.6.1.4.1.2699.1 | "
	         "tee -a results/%s.snmpwalk\n",
	         device->host, filename);
        }

        if (!snmponly)
        {
	  printf("echo \"./ipptool-static -tIf %s -T 30 -d NOPRINT=1 -V %s %s "
	         "ipp-%s.test\" > results/%s.log\n", testfile, version,
	         device->uri, version, filename);
	  printf("CUPS_DEBUG_LOG=results/%s.debug_log "
	         "./ipptool-static -tIf %s -T 30 -d NOPRINT=1 -V %s %s "
	         "ipp-%s.test | tee -a results/%s.log\n", filename,
	         testfile, version, device->uri,
	         version, filename);
        }

	puts("");
      }
      else if (!device->ty)
	fprintf(stderr,
		"Ignoring \"%s\" since it doesn't provide a make and model.\n",
		device->name);
      else if (!testfile)
	fprintf(stderr,
	        "Ignoring \"%s\" since it does not support a common format.\n",
		device->name);
      else
	fprintf(stderr, "Ignoring \"%s\" since it only supports IPP/1.0.\n",
		device->name);
    }

    ippDelete(response);
    httpClose(http);
  }

  return (0);
#endif /* 0 */
}


#ifdef HAVE_DNSSD
/*
 * 'browse_callback()' - Browse devices.
 */

static void
browse_callback(
    DNSServiceRef       sdRef,		/* I - Service reference */
    DNSServiceFlags     flags,		/* I - Option flags */
    uint32_t            interfaceIndex,	/* I - Interface number */
    DNSServiceErrorType errorCode,	/* I - Error, if any */
    const char          *serviceName,	/* I - Name of service/device */
    const char          *regtype,	/* I - Type of service */
    const char          *replyDomain,	/* I - Service domain */
    void                *context)	/* I - Services array */
{
 /*
  * Only process "add" data...
  */

  if (errorCode != kDNSServiceErr_NoError || !(flags & kDNSServiceFlagsAdd))
    return;

 /*
  * Get the device...
  */

  get_service((cups_array_t *)context, serviceName, regtype, replyDomain);
}


/*
 * 'browse_local_callback()' - Browse local devices.
 */

static void
browse_local_callback(
    DNSServiceRef       sdRef,		/* I - Service reference */
    DNSServiceFlags     flags,		/* I - Option flags */
    uint32_t            interfaceIndex,	/* I - Interface number */
    DNSServiceErrorType errorCode,	/* I - Error, if any */
    const char          *serviceName,	/* I - Name of service/device */
    const char          *regtype,	/* I - Type of service */
    const char          *replyDomain,	/* I - Service domain */
    void                *context)	/* I - Services array */
{
  ippfind_srv_t	*service;		/* Service */


 /*
  * Only process "add" data...
  */

  if (errorCode != kDNSServiceErr_NoError || !(flags & kDNSServiceFlagsAdd))
    return;

 /*
  * Get the device...
  */

  service = get_service((cups_array_t *)context, serviceName, regtype,
                        replyDomain);
  service->is_local = 1;
}
#endif /* HAVE_DNSSD */


#ifdef HAVE_AVAHI
/*
 * 'browse_callback()' - Browse devices.
 */

static void
browse_callback(
    AvahiServiceBrowser    *browser,	/* I - Browser */
    AvahiIfIndex           interface,	/* I - Interface index (unused) */
    AvahiProtocol          protocol,	/* I - Network protocol (unused) */
    AvahiBrowserEvent      event,	/* I - What happened */
    const char             *name,	/* I - Service name */
    const char             *type,	/* I - Registration type */
    const char             *domain,	/* I - Domain */
    AvahiLookupResultFlags flags,	/* I - Flags */
    void                   *context)	/* I - Services array */
{
  AvahiClient	*client = avahi_service_browser_get_client(browser);
					/* Client information */
  ippfind_srv_t	*service;		/* Service information */


  (void)interface;
  (void)protocol;
  (void)context;

  switch (event)
  {
    case AVAHI_BROWSER_FAILURE:
	fprintf(stderr, "DEBUG: browse_callback: %s\n",
		avahi_strerror(avahi_client_errno(client)));
	bonjour_error = 1;
	avahi_simple_poll_quit(simple_poll);
	break;

    case AVAHI_BROWSER_NEW:
       /*
	* This object is new on the network. Create a device entry for it if
	* it doesn't yet exist.
	*/

	service = get_service((cups_array_t *)context, name, type, domain);

	if (flags & AVAHI_LOOKUP_RESULT_LOCAL)
	  service->is_local = 1;
	break;

    case AVAHI_BROWSER_REMOVE:
    case AVAHI_BROWSER_ALL_FOR_NOW:
    case AVAHI_BROWSER_CACHE_EXHAUSTED:
        break;
  }
}


/*
 * 'client_callback()' - Avahi client callback function.
 */

static void
client_callback(
    AvahiClient      *client,		/* I - Client information (unused) */
    AvahiClientState state,		/* I - Current state */
    void             *context)		/* I - User data (unused) */
{
  (void)client;
  (void)context;

 /*
  * If the connection drops, quit.
  */

  if (state == AVAHI_CLIENT_FAILURE)
  {
    fputs("DEBUG: Avahi connection failed.\n", stderr);
    bonjour_error = 1;
    avahi_simple_poll_quit(avahi_poll);
  }
}
#endif /* HAVE_AVAHI */


/*
 * 'compare_services()' - Compare two devices.
 */

static int				/* O - Result of comparison */
compare_services(ippfind_srv_t *a,	/* I - First device */
                 ippfind_srv_t *b)	/* I - Second device */
{
  return (strcmp(a->name, b->name));
}


/*
 * 'get_service()' - Create or update a device.
 */

static ippfind_srv_t *			/* O - Service */
get_service(cups_array_t *services,	/* I - Service array */
	    const char   *serviceName,	/* I - Name of service/device */
	    const char   *regtype,	/* I - Type of service */
	    const char   *replyDomain)	/* I - Service domain */
{
  ippfind_srv_t	key,			/* Search key */
		*service;		/* Service */
  char		fullName[kDNSServiceMaxDomainName];
					/* Full name for query */


 /*
  * See if this is a new device...
  */

  key.name    = (char *)serviceName;
  key.regtype = (char *)regtype;

  for (service = cupsArrayFind(services, &key);
       service;
       service = cupsArrayNext(services))
    if (_cups_strcasecmp(service->name, key.name))
      break;
    else if (!strcmp(service->regtype, key.regtype))
      return (service);

 /*
  * Yes, add the service...
  */

  service           = calloc(sizeof(ippfind_srv_t), 1);
  service->name     = strdup(serviceName);
  service->domain   = strdup(replyDomain);
  service->regtype  = strdup(regtype);

  cupsArrayAdd(services, service);

 /*
  * Set the "full name" of this service, which is used for queries and
  * resolves...
  */

#ifdef HAVE_DNSSD
  DNSServiceConstructFullName(fullName, serviceName, regtype, replyDomain);
#else /* HAVE_AVAHI */
  avahi_service_name_join(fullName, kDNSServiceMaxDomainName, serviceName,
                          regtype, replyDomain);
#endif /* HAVE_DNSSD */

  service->fullName = strdup(fullName);

  return (service);
}


#ifdef HAVE_AVAHI
/*
 * 'poll_callback()' - Wait for input on the specified file descriptors.
 *
 * Note: This function is needed because avahi_simple_poll_iterate is broken
 *       and always uses a timeout of 0 (!) milliseconds.
 *       (Avahi Ticket #364)
 */

static int				/* O - Number of file descriptors matching */
poll_callback(
    struct pollfd *pollfds,		/* I - File descriptors */
    unsigned int  num_pollfds,		/* I - Number of file descriptors */
    int           timeout,		/* I - Timeout in milliseconds (unused) */
    void          *context)		/* I - User data (unused) */
{
  int	val;				/* Return value */


  (void)timeout;
  (void)context;

  val = poll(pollfds, num_pollfds, 500);

  if (val < 0)
    fprintf(stderr, "DEBUG: poll_callback: %s\n", strerror(errno));
  else if (val > 0)
    got_data = 1;

  return (val);
}
#endif /* HAVE_AVAHI */


/*
 * 'resolve_callback()' - Process resolve data.
 */

#ifdef HAVE_DNSSD
static void
resolve_callback(
    DNSServiceRef       sdRef,		/* I - Service reference */
    DNSServiceFlags     flags,		/* I - Data flags */
    uint32_t            interfaceIndex,	/* I - Interface */
    DNSServiceErrorType errorCode,	/* I - Error, if any */
    const char          *fullName,	/* I - Full service name */
    const char          *hostTarget,	/* I - Hostname */
    uint16_t            port,		/* I - Port number (network byte order) */
    uint16_t            txtLen,		/* I - Length of TXT record data */
    const unsigned char *txtRecord,	/* I - TXT record data */
    void                *context)	/* I - Service */
{
  char			key[256],	/* TXT key value */
			*value;		/* Value from TXT record */
  const unsigned char	*txtEnd;	/* End of TXT record */
  uint8_t		valueLen;	/* Length of value */
  ippfind_srv_t		*service = (ippfind_srv_t *)context;
					/* Service */


 /*
  * Only process "add" data...
  */

  if (errorCode != kDNSServiceErr_NoError)
    return;

  service->is_resolved = 1;
  service->host        = strdup(hostTarget);
  service->port        = ntohs(port);

 /*
  * Loop through the TXT key/value pairs and add them to an array...
  */

  for (txtEnd = txtRecord + txtLen; txtRecord < txtEnd; txtRecord += valueLen)
  {
   /*
    * Ignore bogus strings...
    */

    valueLen = *txtRecord++;

    memcpy(key, txtRecord, valueLen);
    key[valueLen] = '\0';

    if ((value = strchr(key, '=')) == NULL)
      continue;

    *value++ = '\0';

   /*
    * Add to array of TXT values...
    */

    service->num_txt = cupsAddOption(key, value, service->num_txt,
                                     &(service->txt));
  }

  set_service_uri(service);
}


#elif defined(HAVE_AVAHI)
static void
resolve_callback(
    AvahiServiceResolver   *resolver,	/* I - Resolver */
    AvahiIfIndex           interface,	/* I - Interface */
    AvahiProtocol          protocol,	/* I - Address protocol */
    AvahiBrowserEvent      event,	/* I - Event */
    const char             *serviceName,/* I - Service name */
    const char             *regtype,	/* I - Registration type */
    const char             *replyDomain,/* I - Domain name */
    const char             *hostTarget,	/* I - FQDN */
    uint16_t               port,	/* I - Port number */
    AvahiStringList        *txt,	/* I - TXT records */
    AvahiLookupResultFlags flags,	/* I - Lookup flags */
    void                   *context)	/* I - Service */
{
  char		uri[1024];		/* URI */
		key[256],		/* TXT key */
		*value;			/* TXT value */
  ippfind_srv_t	*service = (ippfind_srv_t *)context;
					/* Service */
  AvahiStringList *current;		/* Current TXT key/value pair */


  if (event != AVAHI_RESOLVER_FOUND)
  {
    bonjour_error = 1;

    avahi_service_resolver_free(resolver);
    avahi_simple_poll_quit(uribuf->poll);
    return;
  }

  service->is_resolved = 1;
  service->host        = strdup(hostTarget);
  service->port        = ntohs(port);

 /*
  * Loop through the TXT key/value pairs and add them to an array...
  */

  for (current = txt; current; current = current->next)
  {
   /*
    * Ignore bogus strings...
    */

    if (current->size > (sizeof(key) - 1))
      continue;

    memcpy(key, current->text, current->size);
    key[current->size] = '\0';

    if ((value = strchr(key, '=')) == NULL)
      continue;

    *value++ = '\0';

   /*
    * Add to array of TXT values...
    */

    service->num_txt = cupsAddOption(key, value, service->num_txt,
                                     &(service->txt));
  }

  set_service_uri(service);
}
#endif /* HAVE_DNSSD */


/*
 * 'set_service_uri()' - Set the URI of the service.
 */

static void
set_service_uri(ippfind_srv_t *service)	/* I - Service */
{
  char		uri[1024];		/* URI */
  const char	*path,			/* Resource path */
		*scheme;		/* URI scheme */


  if (!strncmp(service->regtype, "_http.", 6))
  {
    scheme = "http";
    path   = cupsGetOption("path", service->num_txt, service->txt);
  }
  else if (!strncmp(service->regtype, "_https.", 7))
  {
    scheme = "https";
    path   = cupsGetOption("path", service->num_txt, service->txt);
  }
  else if (!strncmp(service->regtype, "_ipp.", 5))
  {
    scheme = "ipp";
    path   = cupsGetOption("rp", service->num_txt, service->txt);
  }
  else if (!strncmp(service->regtype, "_ipps.", 6))
  {
    scheme = "ipps";
    path   = cupsGetOption("rp", service->num_txt, service->txt);
  }
  else if (!strncmp(service->regtype, "_printer.", 9))
  {
    scheme = "lpd";
    path   = cupsGetOption("rp", service->num_txt, service->txt);
  }
  else
    return;

  if (!path || !*path)
    path = "/";

  if (*path == '/')
    httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(URI), scheme, NULL,
                    service->host, service->port, path);
  else
    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(URI), scheme, NULL,
                     service->host, service->port, "/%s", path);

  service->uri = strdup(uri);
}


/*
 * 'usage()' - Show program usage.
 */

static void
usage(void)
{
  _cupsLangPuts(stderr, _("Usage: ippfind [options] regtype[,subtype]"
                          "[.domain.] ... [expression]\n"
                          "       ippfind [options] name[.regtype[.domain.]] "
                          "... [expression]\n"
                          "       ippfind --help\n"
                          "       ippfind --version"));
  _cupsLangPuts(stderr, "");
  _cupsLangPuts(stderr, _("Options:"));
  _cupsLangPuts(stderr, _("  -4                      Connect using IPv4."));
  _cupsLangPuts(stderr, _("  -6                      Connect using IPv6."));
  _cupsLangPuts(stderr, _("  -T seconds              Set the browse timeout in "
                          "seconds."));
  _cupsLangPuts(stderr, _("  -V version              Set default IPP "
                          "version."));
  _cupsLangPuts(stderr, _("  --help                  Show this help."));
  _cupsLangPuts(stderr, _("  --version               Show program version."));
  _cupsLangPuts(stderr, "");
  _cupsLangPuts(stderr, _("Expressions:"));
  _cupsLangPuts(stderr, _("  -d regex                Match domain to regular expression."));
  _cupsLangPuts(stderr, _("  -e utility [argument ...] ;\n"
                          "                          Execute program if true."));
  _cupsLangPuts(stderr, _("  -l                      List attributes."));
  _cupsLangPuts(stderr, _("  --local                 True if service is local."));
  _cupsLangPuts(stderr, _("  -n regex                Match service name to regular expression."));
  _cupsLangPuts(stderr, _("  --path regex            Match resource path to regular expression."));
  _cupsLangPuts(stderr, _("  -p                      Print URI if true."));
  _cupsLangPuts(stderr, _("  -q                      Quietly report match via exit code."));
  _cupsLangPuts(stderr, _("  -r                      True if service is remote."));

  _cupsLangPuts(stderr, _("  -n regex                Match service name to regular expression."));
  _cupsLangPuts(stderr, _("  -n regex                Match service name to regular expression."));
  _cupsLangPuts(stderr, _("  -n regex                Match service name to regular expression."));
  _cupsLangPuts(stderr, _("  -n regex                Match service name to regular expression."));
  _cupsLangPuts(stderr, _("  -n regex                Match service name to regular expression."));
  _cupsLangPuts(stderr, _("  -n regex                Match service name to regular expression."));
  _cupsLangPuts(stderr, _("  -n regex                Match service name to regular expression."));
  _cupsLangPuts(stderr, _("  -d name=value           Set named variable to "
                          "value."));
  _cupsLangPuts(stderr, _("  -f filename             Set default request "
                          "filename."));
  _cupsLangPuts(stderr, _("  -i seconds              Repeat the last file with "
                          "the given time interval."));
  _cupsLangPuts(stderr, _("  -n count                Repeat the last file the "
                          "given number of times."));
  _cupsLangPuts(stderr, _("  -q                      Run silently."));
  _cupsLangPuts(stderr, _("  -t                      Produce a test report."));
  _cupsLangPuts(stderr, _("  -v                      Be verbose."));

  exit(IPPFIND_EXIT_OK);
}


/*
 * 'show_version()' - Show program version.
 */

static void
show_version(void)
{
  _cupsLangPuts(stderr, CUPS_SVERSION);

  exit(IPPFIND_EXIT_OK);
}


/*
 * 'unquote()' - Unquote a name string.
 */

static void
unquote(char       *dst,		/* I - Destination buffer */
        const char *src,		/* I - Source string */
	size_t     dstsize)		/* I - Size of destination buffer */
{
  char	*dstend = dst + dstsize - 1;	/* End of destination buffer */


  while (*src && dst < dstend)
  {
    if (*src == '\\')
    {
      src ++;
      if (isdigit(src[0] & 255) && isdigit(src[1] & 255) &&
          isdigit(src[2] & 255))
      {
        *dst++ = ((((src[0] - '0') * 10) + src[1] - '0') * 10) + src[2] - '0';
	src += 3;
      }
      else
        *dst++ = *src++;
    }
    else
      *dst++ = *src ++;
  }

  *dst = '\0';
}


/*
 * End of "$Id$".
 */
