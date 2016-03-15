/*
 * "$Id: dnssd.c 12970 2015-11-13 20:02:51Z msweet $"
 *
 * DNS-SD discovery backend for CUPS.
 *
 * Copyright 2008-2015 by Apple Inc.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * "LICENSE" which should have been included with this file.  If this
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers.
 */

#include "backend-private.h"
#include <cups/array.h>
#ifdef HAVE_DNSSD
#  include <dns_sd.h>
#endif /* HAVE_DNSSD */
#ifdef HAVE_AVAHI
#  include <avahi-client/client.h>
#  include <avahi-client/lookup.h>
#  include <avahi-common/simple-watch.h>
#  include <avahi-common/domain.h>
#  include <avahi-common/error.h>
#  include <avahi-common/malloc.h>
#define kDNSServiceMaxDomainName AVAHI_DOMAIN_NAME_MAX
#endif /* HAVE_AVAHI */


/*
 * Device structure...
 */

typedef enum
{
  CUPS_DEVICE_PRINTER = 0,		/* lpd://... */
  CUPS_DEVICE_IPPS,			/* ipps://... */
  CUPS_DEVICE_IPP,			/* ipp://... */
  CUPS_DEVICE_FAX_IPP,			/* ipp://... */
  CUPS_DEVICE_PDL_DATASTREAM,		/* socket://... */
  CUPS_DEVICE_RIOUSBPRINT		/* riousbprint://... */
} cups_devtype_t;


typedef struct
{
#ifdef HAVE_DNSSD
  DNSServiceRef	ref;			/* Service reference for query */
#endif /* HAVE_DNSSD */
#ifdef HAVE_AVAHI
  AvahiRecordBrowser *ref;		/* Browser for query */
#endif /* HAVE_AVAHI */
  char		*name,			/* Service name */
		*domain,		/* Domain name */
		*fullName,		/* Full name */
		*make_and_model,	/* Make and model from TXT record */
		*device_id,		/* 1284 device ID from TXT record */
		*uuid;			/* UUID from TXT record */
  cups_devtype_t type;			/* Device registration type */
  int		priority,		/* Priority associated with type */
		cups_shared,		/* CUPS shared printer? */
		sent;			/* Did we list the device? */
} cups_device_t;


/*
 * Local globals...
 */

static int		job_canceled = 0;
					/* Set to 1 on SIGTERM */
#ifdef HAVE_AVAHI
static AvahiSimplePoll	*simple_poll = NULL;
					/* Poll information */
static int		got_data = 0;	/* Got data from poll? */
static int		browsers = 0;	/* Number of running browsers */
#endif /* HAVE_AVAHI */


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
#endif /* HAVE_DNSSD */
#ifdef HAVE_AVAHI
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

static int		compare_devices(cups_device_t *a, cups_device_t *b);
static void		exec_backend(char **argv) __attribute__((noreturn));
static cups_device_t	*get_device(cups_array_t *devices,
			            const char *serviceName,
			            const char *regtype,
				    const char *replyDomain)
				    __attribute__((nonnull(1,2,3,4)));
#ifdef HAVE_DNSSD
static void		query_callback(DNSServiceRef sdRef,
			               DNSServiceFlags flags,
				       uint32_t interfaceIndex,
				       DNSServiceErrorType errorCode,
				       const char *fullName, uint16_t rrtype,
				       uint16_t rrclass, uint16_t rdlen,
				       const void *rdata, uint32_t ttl,
				       void *context)
				       __attribute__((nonnull(1,5,9,11)));
#elif defined(HAVE_AVAHI)
static int		poll_callback(struct pollfd *pollfds,
			              unsigned int num_pollfds, int timeout,
			              void *context);
static void		query_callback(AvahiRecordBrowser *browser,
				       AvahiIfIndex interface,
				       AvahiProtocol protocol,
				       AvahiBrowserEvent event,
				       const char *name, uint16_t rrclass,
				       uint16_t rrtype, const void *rdata,
				       size_t rdlen,
				       AvahiLookupResultFlags flags,
				       void *context);
#endif /* HAVE_DNSSD */
static void		sigterm_handler(int sig);
static void		unquote(char *dst, const char *src, size_t dstsize)
			    __attribute__((nonnull(1,2)));


/*
 * 'main()' - Browse for printers.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  const char	*name;			/* Backend name */
  cups_array_t	*devices;		/* Device array */
  cups_device_t	*device;		/* Current device */
  char		uriName[1024];		/* Unquoted fullName for URI */
#ifdef HAVE_DNSSD
  int		fd;			/* Main file descriptor */
  fd_set	input;			/* Input set for select() */
  struct timeval timeout;		/* Timeout for select() */
  DNSServiceRef	main_ref,		/* Main service reference */
		fax_ipp_ref,		/* IPP fax service reference */
		ipp_ref,		/* IPP service reference */
		ipp_tls_ref,		/* IPP w/TLS service reference */
		ipps_ref,		/* IPP service reference */
		local_fax_ipp_ref,	/* Local IPP fax service reference */
		local_ipp_ref,		/* Local IPP service reference */
		local_ipp_tls_ref,	/* Local IPP w/TLS service reference */
		local_ipps_ref,		/* Local IPP service reference */
		local_printer_ref,	/* Local LPD service reference */
		pdl_datastream_ref,	/* AppSocket service reference */
		printer_ref,		/* LPD service reference */
		riousbprint_ref;	/* Remote IO service reference */
#endif /* HAVE_DNSSD */
#ifdef HAVE_AVAHI
  AvahiClient	*client;		/* Client information */
  int		error;			/* Error code, if any */
#endif /* HAVE_AVAHI */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Don't buffer stderr, and catch SIGTERM...
  */

  setbuf(stderr, NULL);

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGTERM, sigterm_handler);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  action.sa_handler = sigterm_handler;
  sigaction(SIGTERM, &action, NULL);
#else
  signal(SIGTERM, sigterm_handler);
#endif /* HAVE_SIGSET */

 /*
  * Check command-line...
  */

  if (argc >= 6)
    exec_backend(argv);
  else if (argc != 1)
  {
    _cupsLangPrintf(stderr,
                    _("Usage: %s job-id user title copies options [file]"),
		    argv[0]);
    return (1);
  }

 /*
  * Only do discovery when run as "dnssd"...
  */

  if ((name = strrchr(argv[0], '/')) != NULL)
    name ++;
  else
    name = argv[0];

  if (strcmp(name, "dnssd"))
    return (0);

 /*
  * Create an array to track devices...
  */

  devices = cupsArrayNew((cups_array_func_t)compare_devices, NULL);

 /*
  * Browse for different kinds of printers...
  */

#ifdef HAVE_DNSSD
  if (DNSServiceCreateConnection(&main_ref) != kDNSServiceErr_NoError)
  {
    perror("ERROR: Unable to create service connection");
    return (1);
  }

  fd = DNSServiceRefSockFD(main_ref);

  fax_ipp_ref = main_ref;
  DNSServiceBrowse(&fax_ipp_ref, kDNSServiceFlagsShareConnection, 0,
                   "_fax-ipp._tcp", NULL, browse_callback, devices);

  ipp_ref = main_ref;
  DNSServiceBrowse(&ipp_ref, kDNSServiceFlagsShareConnection, 0,
                   "_ipp._tcp", NULL, browse_callback, devices);

  ipp_tls_ref = main_ref;
  DNSServiceBrowse(&ipp_tls_ref, kDNSServiceFlagsShareConnection, 0,
                   "_ipp-tls._tcp", NULL, browse_callback, devices);

  ipps_ref = main_ref;
  DNSServiceBrowse(&ipps_ref, kDNSServiceFlagsShareConnection, 0,
                   "_ipps._tcp", NULL, browse_callback, devices);

  local_fax_ipp_ref = main_ref;
  DNSServiceBrowse(&local_fax_ipp_ref, kDNSServiceFlagsShareConnection,
                   kDNSServiceInterfaceIndexLocalOnly,
		   "_fax-ipp._tcp", NULL, browse_local_callback, devices);

  local_ipp_ref = main_ref;
  DNSServiceBrowse(&local_ipp_ref, kDNSServiceFlagsShareConnection,
                   kDNSServiceInterfaceIndexLocalOnly,
		   "_ipp._tcp", NULL, browse_local_callback, devices);

  local_ipp_tls_ref = main_ref;
  DNSServiceBrowse(&local_ipp_tls_ref, kDNSServiceFlagsShareConnection,
                   kDNSServiceInterfaceIndexLocalOnly,
                   "_ipp-tls._tcp", NULL, browse_local_callback, devices);

  local_ipps_ref = main_ref;
  DNSServiceBrowse(&local_ipps_ref, kDNSServiceFlagsShareConnection,
                   kDNSServiceInterfaceIndexLocalOnly,
		   "_ipps._tcp", NULL, browse_local_callback, devices);

  local_printer_ref = main_ref;
  DNSServiceBrowse(&local_printer_ref, kDNSServiceFlagsShareConnection,
                   kDNSServiceInterfaceIndexLocalOnly,
                   "_printer._tcp", NULL, browse_local_callback, devices);

  pdl_datastream_ref = main_ref;
  DNSServiceBrowse(&pdl_datastream_ref, kDNSServiceFlagsShareConnection, 0,
                   "_pdl-datastream._tcp", NULL, browse_callback, devices);

  printer_ref = main_ref;
  DNSServiceBrowse(&printer_ref, kDNSServiceFlagsShareConnection, 0,
                   "_printer._tcp", NULL, browse_callback, devices);

  riousbprint_ref = main_ref;
  DNSServiceBrowse(&riousbprint_ref, kDNSServiceFlagsShareConnection, 0,
                   "_riousbprint._tcp", NULL, browse_callback, devices);
#endif /* HAVE_DNSSD */

#ifdef HAVE_AVAHI
  if ((simple_poll = avahi_simple_poll_new()) == NULL)
  {
    fputs("DEBUG: Unable to create Avahi simple poll object.\n", stderr);
    return (0);
  }

  avahi_simple_poll_set_func(simple_poll, poll_callback, NULL);

  client = avahi_client_new(avahi_simple_poll_get(simple_poll),
			    0, client_callback, simple_poll, &error);
  if (!client)
  {
    fputs("DEBUG: Unable to create Avahi client.\n", stderr);
    return (0);
  }

  browsers = 6;
  avahi_service_browser_new(client, AVAHI_IF_UNSPEC,
			    AVAHI_PROTO_UNSPEC,
			    "_fax-ipp._tcp", NULL, 0,
			    browse_callback, devices);
  avahi_service_browser_new(client, AVAHI_IF_UNSPEC,
			    AVAHI_PROTO_UNSPEC,
			    "_ipp._tcp", NULL, 0,
			    browse_callback, devices);
  avahi_service_browser_new(client, AVAHI_IF_UNSPEC,
			    AVAHI_PROTO_UNSPEC,
			    "_ipp-tls._tcp", NULL, 0,
			    browse_callback, devices);
  avahi_service_browser_new(client, AVAHI_IF_UNSPEC,
			    AVAHI_PROTO_UNSPEC,
			    "_ipps._tcp", NULL, 0,
			    browse_callback, devices);
  avahi_service_browser_new(client, AVAHI_IF_UNSPEC,
			    AVAHI_PROTO_UNSPEC,
			    "_pdl-datastream._tcp",
			    NULL, 0,
			    browse_callback,
			    devices);
  avahi_service_browser_new(client, AVAHI_IF_UNSPEC,
			    AVAHI_PROTO_UNSPEC,
			    "_printer._tcp", NULL, 0,
			    browse_callback, devices);
#endif /* HAVE_AVAHI */

 /*
  * Loop until we are killed...
  */

  while (!job_canceled)
  {
    int announce = 0;			/* Announce printers? */

#ifdef HAVE_DNSSD
    FD_ZERO(&input);
    FD_SET(fd, &input);

    timeout.tv_sec  = 0;
    timeout.tv_usec = 500000;

    if (select(fd + 1, &input, NULL, NULL, &timeout) < 0)
      continue;

    if (FD_ISSET(fd, &input))
    {
     /*
      * Process results of our browsing...
      */

      DNSServiceProcessResult(main_ref);
    }
    else
      announce = 1;

#elif defined(HAVE_AVAHI)
    got_data = 0;

    if ((error = avahi_simple_poll_iterate(simple_poll, 500)) > 0)
    {
     /*
      * We've been told to exit the loop.  Perhaps the connection to
      * Avahi failed.
      */

      break;
    }

    if (!got_data)
      announce = 1;
#endif /* HAVE_DNSSD */

/*    fprintf(stderr, "DEBUG: announce=%d\n", announce);*/

    if (announce)
    {
     /*
      * Announce any devices we've found...
      */

#ifdef HAVE_DNSSD
      DNSServiceErrorType status;	/* DNS query status */
#endif /* HAVE_DNSSD */
      cups_device_t *best;		/* Best matching device */
      char	device_uri[1024];	/* Device URI */
      int	count;			/* Number of queries */
      int	sent;			/* Number of sent */

      for (device = (cups_device_t *)cupsArrayFirst(devices),
               best = NULL, count = 0, sent = 0;
           device;
	   device = (cups_device_t *)cupsArrayNext(devices))
      {
        if (device->sent)
	  sent ++;

        if (device->ref)
	  count ++;

        if (!device->ref && !device->sent)
	{
	 /*
	  * Found the device, now get the TXT record(s) for it...
	  */

          if (count < 50)
	  {
	    fprintf(stderr, "DEBUG: Querying \"%s\"...\n", device->fullName);

#ifdef HAVE_DNSSD
	    device->ref = main_ref;

	    status = DNSServiceQueryRecord(&(device->ref),
				           kDNSServiceFlagsShareConnection,
				           0, device->fullName,
					   kDNSServiceType_TXT,
				           kDNSServiceClass_IN, query_callback,
				           device);
            if (status != kDNSServiceErr_NoError)
	      fprintf(stderr,
	              "ERROR: Unable to query \"%s\" for TXT records: %d\n",
	              device->fullName, status);
	              			/* Users never see this */
	    else
	      count ++;

#else
	    if ((device->ref = avahi_record_browser_new(client, AVAHI_IF_UNSPEC,
	                                                AVAHI_PROTO_UNSPEC,
	                                                device->fullName,
	                                                AVAHI_DNS_CLASS_IN,
	                                                AVAHI_DNS_TYPE_TXT,
	                                                0,
				                        query_callback,
				                        device)) == NULL)
	      fprintf(stderr,
	              "ERROR: Unable to query \"%s\" for TXT records: %s\n",
	              device->fullName,
	              avahi_strerror(avahi_client_errno(client)));
	              			/* Users never see this */
	    else
	      count ++;
#endif /* HAVE_AVAHI */
          }
	}
	else if (!device->sent)
	{
#ifdef HAVE_DNSSD
	 /*
	  * Got the TXT records, now report the device...
	  */

	  DNSServiceRefDeallocate(device->ref);
#else
          avahi_record_browser_free(device->ref);
#endif /* HAVE_DNSSD */

	  device->ref = NULL;

          if (!best)
	    best = device;
	  else if (_cups_strcasecmp(best->name, device->name) ||
	           _cups_strcasecmp(best->domain, device->domain))
          {
	    unquote(uriName, best->fullName, sizeof(uriName));

            if (best->uuid)
	      httpAssembleURIf(HTTP_URI_CODING_ALL, device_uri,
	                       sizeof(device_uri), "dnssd", NULL, uriName, 0,
			       best->cups_shared ? "/cups?uuid=%s" : "/?uuid=%s",
			       best->uuid);
	    else
	      httpAssembleURI(HTTP_URI_CODING_ALL, device_uri,
	                      sizeof(device_uri), "dnssd", NULL, uriName, 0,
			      best->cups_shared ? "/cups" : "/");

	    cupsBackendReport("network", device_uri, best->make_and_model,
	                      best->name, best->device_id, NULL);
	    best->sent = 1;
	    best       = device;

	    sent ++;
	  }
	  else if (best->priority > device->priority ||
	           (best->priority == device->priority &&
		    best->type < device->type))
          {
	    best->sent = 1;
	    best       = device;

	    sent ++;
	  }
	  else
	  {
	    device->sent = 1;

	    sent ++;
	  }
        }
      }

      if (best)
      {
	unquote(uriName, best->fullName, sizeof(uriName));

	if (best->uuid)
	  httpAssembleURIf(HTTP_URI_CODING_ALL, device_uri,
			   sizeof(device_uri), "dnssd", NULL, uriName, 0,
			   best->cups_shared ? "/cups?uuid=%s" : "/?uuid=%s",
			   best->uuid);
	else
	  httpAssembleURI(HTTP_URI_CODING_ALL, device_uri,
			  sizeof(device_uri), "dnssd", NULL, uriName, 0,
			  best->cups_shared ? "/cups" : "/");

	cupsBackendReport("network", device_uri, best->make_and_model,
			  best->name, best->device_id, NULL);
	best->sent = 1;
	sent ++;
      }

      fprintf(stderr, "DEBUG: sent=%d, count=%d\n", sent, count);

#ifdef HAVE_AVAHI
      if (sent == cupsArrayCount(devices) && browsers == 0)
#else
      if (sent == cupsArrayCount(devices))
#endif /* HAVE_AVAHI */
	break;
    }
  }

  return (CUPS_BACKEND_OK);
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
    void                *context)	/* I - Devices array */
{
  fprintf(stderr, "DEBUG2: browse_callback(sdRef=%p, flags=%x, "
                  "interfaceIndex=%d, errorCode=%d, serviceName=\"%s\", "
		  "regtype=\"%s\", replyDomain=\"%s\", context=%p)\n",
          sdRef, flags, interfaceIndex, errorCode,
	  serviceName, regtype, replyDomain, context);

 /*
  * Only process "add" data...
  */

  if (errorCode != kDNSServiceErr_NoError || !(flags & kDNSServiceFlagsAdd))
    return;

 /*
  * Get the device...
  */

  get_device((cups_array_t *)context, serviceName, regtype, replyDomain);
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
    void                *context)	/* I - Devices array */
{
  cups_device_t	*device;		/* Device */


  fprintf(stderr, "DEBUG2: browse_local_callback(sdRef=%p, flags=%x, "
                  "interfaceIndex=%d, errorCode=%d, serviceName=\"%s\", "
		  "regtype=\"%s\", replyDomain=\"%s\", context=%p)\n",
          sdRef, flags, interfaceIndex, errorCode,
	  serviceName, regtype, replyDomain, context);

 /*
  * Only process "add" data...
  */

  if (errorCode != kDNSServiceErr_NoError || !(flags & kDNSServiceFlagsAdd))
    return;

 /*
  * Get the device...
  */

  device = get_device((cups_array_t *)context, serviceName, regtype,
                      replyDomain);

 /*
  * Hide locally-registered devices...
  */

  fprintf(stderr, "DEBUG: Hiding local printer \"%s\"...\n",
	  device->fullName);
  device->sent = 1;
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
    void                   *context)	/* I - Devices array */
{
  AvahiClient *client = avahi_service_browser_get_client(browser);
					/* Client information */


  (void)interface;
  (void)protocol;
  (void)context;

  switch (event)
  {
    case AVAHI_BROWSER_FAILURE:
	fprintf(stderr, "DEBUG: browse_callback: %s\n",
		avahi_strerror(avahi_client_errno(client)));
	avahi_simple_poll_quit(simple_poll);
	break;

    case AVAHI_BROWSER_NEW:
       /*
	* This object is new on the network.
	*/

	if (flags & AVAHI_LOOKUP_RESULT_LOCAL)
	{
	 /*
	  * This comes from the local machine so ignore it.
	  */

	  fprintf(stderr, "DEBUG: Ignoring local service %s.\n", name);
	}
	else
	{
	 /*
	  * Create a device entry for it if it doesn't yet exist.
	  */

	  get_device((cups_array_t *)context, name, type, domain);
	}
	break;

    case AVAHI_BROWSER_REMOVE:
    case AVAHI_BROWSER_CACHE_EXHAUSTED:
        break;

    case AVAHI_BROWSER_ALL_FOR_NOW:
	browsers--;
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
    avahi_simple_poll_quit(simple_poll);
  }
}
#endif /* HAVE_AVAHI */


/*
 * 'compare_devices()' - Compare two devices.
 */

static int				/* O - Result of comparison */
compare_devices(cups_device_t *a,	/* I - First device */
                cups_device_t *b)	/* I - Second device */
{
  return (strcmp(a->name, b->name));
}


/*
 * 'exec_backend()' - Execute the backend that corresponds to the
 *                    resolved service name.
 */

static void
exec_backend(char **argv)		/* I - Command-line arguments */
{
  const char	*resolved_uri,		/* Resolved device URI */
		*cups_serverbin;	/* Location of programs */
  char		scheme[1024],		/* Scheme from URI */
		*ptr,			/* Pointer into scheme */
		filename[1024];		/* Backend filename */


 /*
  * Resolve the device URI...
  */

  job_canceled = -1;

  while ((resolved_uri = cupsBackendDeviceURI(argv)) == NULL)
  {
    _cupsLangPrintFilter(stderr, "INFO", _("Unable to locate printer."));
    sleep(10);

    if (getenv("CLASS") != NULL)
      exit(CUPS_BACKEND_FAILED);
  }

 /*
  * Extract the scheme from the URI...
  */

  strlcpy(scheme, resolved_uri, sizeof(scheme));
  if ((ptr = strchr(scheme, ':')) != NULL)
    *ptr = '\0';

 /*
  * Get the filename of the backend...
  */

  if ((cups_serverbin = getenv("CUPS_SERVERBIN")) == NULL)
    cups_serverbin = CUPS_SERVERBIN;

  snprintf(filename, sizeof(filename), "%s/backend/%s", cups_serverbin, scheme);

 /*
  * Overwrite the device URI and run the new backend...
  */

  setenv("DEVICE_URI", resolved_uri, 1);

  argv[0] = (char *)resolved_uri;

  fprintf(stderr, "DEBUG: Executing backend \"%s\"...\n", filename);

  execv(filename, argv);

  fprintf(stderr, "ERROR: Unable to execute backend \"%s\": %s\n", filename,
          strerror(errno));
  exit(CUPS_BACKEND_STOP);
}


/*
 * 'device_type()' - Get DNS-SD type enumeration from string.
 */

static cups_devtype_t			/* O - Device type */
device_type(const char *regtype)	/* I - Service registration type */
{
#ifdef HAVE_AVAHI
  if (!strcmp(regtype, "_ipp._tcp"))
    return (CUPS_DEVICE_IPP);
  else if (!strcmp(regtype, "_ipps._tcp") ||
	   !strcmp(regtype, "_ipp-tls._tcp"))
    return (CUPS_DEVICE_IPPS);
  else if (!strcmp(regtype, "_fax-ipp._tcp"))
    return (CUPS_DEVICE_FAX_IPP);
  else if (!strcmp(regtype, "_printer._tcp"))
    return (CUPS_DEVICE_PDL_DATASTREAM);
#else
  if (!strcmp(regtype, "_ipp._tcp."))
    return (CUPS_DEVICE_IPP);
  else if (!strcmp(regtype, "_ipps._tcp.") ||
	   !strcmp(regtype, "_ipp-tls._tcp."))
    return (CUPS_DEVICE_IPPS);
  else if (!strcmp(regtype, "_fax-ipp._tcp."))
    return (CUPS_DEVICE_FAX_IPP);
  else if (!strcmp(regtype, "_printer._tcp."))
    return (CUPS_DEVICE_PRINTER);
  else if (!strcmp(regtype, "_pdl-datastream._tcp."))
    return (CUPS_DEVICE_PDL_DATASTREAM);
#endif /* HAVE_AVAHI */

  return (CUPS_DEVICE_RIOUSBPRINT);
}


/*
 * 'get_device()' - Create or update a device.
 */

static cups_device_t *			/* O - Device */
get_device(cups_array_t *devices,	/* I - Device array */
           const char   *serviceName,	/* I - Name of service/device */
           const char   *regtype,	/* I - Type of service */
           const char   *replyDomain)	/* I - Service domain */
{
  cups_device_t	key,			/* Search key */
		*device;		/* Device */
  char		fullName[kDNSServiceMaxDomainName];
					/* Full name for query */


 /*
  * See if this is a new device...
  */

  key.name = (char *)serviceName;
  key.type = device_type(regtype);

  for (device = cupsArrayFind(devices, &key);
       device;
       device = cupsArrayNext(devices))
    if (_cups_strcasecmp(device->name, key.name))
      break;
    else if (device->type == key.type)
    {
      if (!_cups_strcasecmp(device->domain, "local.") &&
          _cups_strcasecmp(device->domain, replyDomain))
      {
       /*
        * Update the .local listing to use the "global" domain name instead.
	* The backend will try local lookups first, then the global domain name.
	*/

        free(device->domain);
	device->domain = strdup(replyDomain);

#ifdef HAVE_DNSSD
	DNSServiceConstructFullName(fullName, device->name, regtype,
	                            replyDomain);
#else /* HAVE_AVAHI */
	avahi_service_name_join(fullName, kDNSServiceMaxDomainName,
				serviceName, regtype, replyDomain);
#endif /* HAVE_DNSSD */

	free(device->fullName);
	device->fullName = strdup(fullName);
      }

      return (device);
    }

 /*
  * Yes, add the device...
  */

  device           = calloc(sizeof(cups_device_t), 1);
  device->name     = strdup(serviceName);
  device->domain   = strdup(replyDomain);
  device->type     = key.type;
  device->priority = 50;

  cupsArrayAdd(devices, device);

 /*
  * Set the "full name" of this service, which is used for queries...
  */

#ifdef HAVE_DNSSD
  DNSServiceConstructFullName(fullName, serviceName, regtype, replyDomain);
#else /* HAVE_AVAHI */
  avahi_service_name_join(fullName, kDNSServiceMaxDomainName, serviceName, regtype, replyDomain);
#endif /* HAVE_DNSSD */

  device->fullName = strdup(fullName);

  return (device);
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


#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
#  ifdef HAVE_DNSSD
/*
 * 'query_callback()' - Process query data.
 */

static void
query_callback(
    DNSServiceRef       sdRef,		/* I - Service reference */
    DNSServiceFlags     flags,		/* I - Data flags */
    uint32_t            interfaceIndex,	/* I - Interface */
    DNSServiceErrorType errorCode,	/* I - Error, if any */
    const char          *fullName,	/* I - Full service name */
    uint16_t            rrtype,		/* I - Record type */
    uint16_t            rrclass,	/* I - Record class */
    uint16_t            rdlen,		/* I - Length of record data */
    const void          *rdata,		/* I - Record data */
    uint32_t            ttl,		/* I - Time-to-live */
    void                *context)	/* I - Device */
{
#  else
/*
 * 'query_callback()' - Process query data.
 */

static void
query_callback(
    AvahiRecordBrowser     *browser,	/* I - Record browser */
    AvahiIfIndex           interfaceIndex,
					/* I - Interface index (unused) */
    AvahiProtocol          protocol,	/* I - Network protocol (unused) */
    AvahiBrowserEvent      event,	/* I - What happened? */
    const char             *fullName,	/* I - Service name */
    uint16_t               rrclass,	/* I - Record class */
    uint16_t               rrtype,	/* I - Record type */
    const void             *rdata,	/* I - TXT record */
    size_t                 rdlen,	/* I - Length of TXT record */
    AvahiLookupResultFlags flags,	/* I - Flags */
    void                   *context)	/* I - Device */
{
  AvahiClient		*client = avahi_record_browser_get_client(browser);
					/* Client information */
#  endif /* HAVE_DNSSD */
  char		*ptr;			/* Pointer into string */
  cups_device_t	*device = (cups_device_t *)context;
					/* Device */
  const uint8_t	*data,			/* Pointer into data */
		*datanext,		/* Next key/value pair */
		*dataend;		/* End of entire TXT record */
  uint8_t	datalen;		/* Length of current key/value pair */
  char		key[256],		/* Key string */
		value[256],		/* Value string */
		make_and_model[512],	/* Manufacturer and model */
		model[256],		/* Model */
		pdl[256],		/* PDL */
		device_id[2048];	/* 1284 device ID */


#  ifdef HAVE_DNSSD
  fprintf(stderr, "DEBUG2: query_callback(sdRef=%p, flags=%x, "
                  "interfaceIndex=%d, errorCode=%d, fullName=\"%s\", "
		  "rrtype=%u, rrclass=%u, rdlen=%u, rdata=%p, ttl=%u, "
		  "context=%p)\n",
          sdRef, flags, interfaceIndex, errorCode,
	  fullName ? fullName : "(null)", rrtype, rrclass, rdlen, rdata, ttl,
	  context);

 /*
  * Only process "add" data...
  */

  if (errorCode != kDNSServiceErr_NoError || !(flags & kDNSServiceFlagsAdd))
    return;

#  else
  fprintf(stderr, "DEBUG2: query_callback(browser=%p, interfaceIndex=%d, "
                  "protocol=%d, event=%d, fullName=\"%s\", rrclass=%u, "
		  "rrtype=%u, rdata=%p, rdlen=%u, flags=%x, context=%p)\n",
          browser, interfaceIndex, protocol, event,
	  fullName ? fullName : "(null)", rrclass, rrtype, rdata,
	  (unsigned)rdlen, flags, context);

 /*
  * Only process "add" data...
  */

  if (event != AVAHI_BROWSER_NEW)
  {
    if (event == AVAHI_BROWSER_FAILURE)
      fprintf(stderr, "ERROR: %s\n",
	      avahi_strerror(avahi_client_errno(client)));

    return;
  }
#  endif /* HAVE_DNSSD */

 /*
  * Pull out the priority and make and model from the TXT
  * record and save it...
  */

  device_id[0]      = '\0';
  make_and_model[0] = '\0';
  pdl[0]            = '\0';

  strlcpy(model, "Unknown", sizeof(model));

  for (data = rdata, dataend = data + rdlen;
       data < dataend;
       data = datanext)
  {
   /*
    * Read a key/value pair starting with an 8-bit length.  Since the
    * length is 8 bits and the size of the key/value buffers is 256, we
    * don't need to check for overflow...
    */

    datalen = *data++;

    if (!datalen || (data + datalen) > dataend)
      break;

    datanext = data + datalen;

    for (ptr = key; data < datanext && *data != '='; data ++)
      *ptr++ = (char)*data;
    *ptr = '\0';

    if (data < datanext && *data == '=')
    {
      data ++;

      if (data < datanext)
	memcpy(value, data, (size_t)(datanext - data));
      value[datanext - data] = '\0';

      fprintf(stderr, "DEBUG2: query_callback: \"%s=%s\".\n",
	      key, value);
    }
    else
    {
      fprintf(stderr, "DEBUG2: query_callback: \"%s\" with no value.\n",
	      key);
      continue;
    }

    if (!_cups_strncasecmp(key, "usb_", 4))
    {
     /*
      * Add USB device ID information...
      */

      ptr = device_id + strlen(device_id);
      snprintf(ptr, sizeof(device_id) - (size_t)(ptr - device_id), "%s:%s;", key + 4, value);
    }

    if (!_cups_strcasecmp(key, "usb_MFG") || !_cups_strcasecmp(key, "usb_MANU") ||
	!_cups_strcasecmp(key, "usb_MANUFACTURER"))
      strlcpy(make_and_model, value, sizeof(make_and_model));
    else if (!_cups_strcasecmp(key, "usb_MDL") || !_cups_strcasecmp(key, "usb_MODEL"))
      strlcpy(model, value, sizeof(model));
    else if (!_cups_strcasecmp(key, "product") && !strstr(value, "Ghostscript"))
    {
      if (value[0] == '(')
      {
       /*
	* Strip parenthesis...
	*/

	if ((ptr = value + strlen(value) - 1) > value && *ptr == ')')
	  *ptr = '\0';

	strlcpy(model, value + 1, sizeof(model));
      }
      else
	strlcpy(model, value, sizeof(model));
    }
    else if (!_cups_strcasecmp(key, "ty"))
    {
      strlcpy(model, value, sizeof(model));

      if ((ptr = strchr(model, ',')) != NULL)
	*ptr = '\0';
    }
    else if (!_cups_strcasecmp(key, "pdl"))
      strlcpy(pdl, value, sizeof(pdl));
    else if (!_cups_strcasecmp(key, "priority"))
      device->priority = atoi(value);
    else if ((device->type == CUPS_DEVICE_IPP ||
	      device->type == CUPS_DEVICE_IPPS ||
	      device->type == CUPS_DEVICE_PRINTER) &&
	     !_cups_strcasecmp(key, "printer-type"))
    {
     /*
      * This is a CUPS printer!
      */

      device->cups_shared = 1;

      if (device->type == CUPS_DEVICE_PRINTER)
	device->sent = 1;
    }
    else if (!_cups_strcasecmp(key, "UUID"))
      device->uuid = strdup(value);
  }

  if (device->device_id)
    free(device->device_id);

  if (!device_id[0] && strcmp(model, "Unknown"))
  {
    if (make_and_model[0])
      snprintf(device_id, sizeof(device_id), "MFG:%s;MDL:%s;",
	       make_and_model, model);
    else if (!_cups_strncasecmp(model, "designjet ", 10))
      snprintf(device_id, sizeof(device_id), "MFG:HP;MDL:%s", model + 10);
    else if (!_cups_strncasecmp(model, "stylus ", 7))
      snprintf(device_id, sizeof(device_id), "MFG:EPSON;MDL:%s", model + 7);
    else if ((ptr = strchr(model, ' ')) != NULL)
    {
     /*
      * Assume the first word is the make...
      */

      memcpy(make_and_model, model, (size_t)(ptr - model));
      make_and_model[ptr - model] = '\0';

      snprintf(device_id, sizeof(device_id), "MFG:%s;MDL:%s",
	       make_and_model, ptr + 1);
    }
  }

  if (device_id[0] &&
      !strstr(device_id, "CMD:") &&
      !strstr(device_id, "COMMAND SET:") &&
      (strstr(pdl, "application/pdf") ||
       strstr(pdl, "application/postscript") ||
       strstr(pdl, "application/vnd.hp-PCL") ||
       strstr(pdl, "image/")))
  {
    value[0] = '\0';
    if (strstr(pdl, "application/pdf"))
      strlcat(value, ",PDF", sizeof(value));
    if (strstr(pdl, "application/postscript"))
      strlcat(value, ",PS", sizeof(value));
    if (strstr(pdl, "application/vnd.hp-PCL"))
      strlcat(value, ",PCL", sizeof(value));
    for (ptr = strstr(pdl, "image/"); ptr; ptr = strstr(ptr, "image/"))
    {
      char *valptr = value + strlen(value);
      					/* Pointer into value */

      if (valptr < (value + sizeof(value) - 1))
        *valptr++ = ',';

      ptr += 6;
      while (isalnum(*ptr & 255) || *ptr == '-' || *ptr == '.')
      {
        if (isalnum(*ptr & 255) && valptr < (value + sizeof(value) - 1))
          *valptr++ = (char)toupper(*ptr++ & 255);
        else
          break;
      }

      *valptr = '\0';
    }

    ptr = device_id + strlen(device_id);
    snprintf(ptr, sizeof(device_id) - (size_t)(ptr - device_id), "CMD:%s;", value + 1);
  }

  if (device_id[0])
    device->device_id = strdup(device_id);
  else
    device->device_id = NULL;

  if (device->make_and_model)
    free(device->make_and_model);

  if (make_and_model[0])
  {
    strlcat(make_and_model, " ", sizeof(make_and_model));
    strlcat(make_and_model, model, sizeof(make_and_model));

    device->make_and_model = strdup(make_and_model);
  }
  else
    device->make_and_model = strdup(model);
}
#endif /* HAVE_DNSSD || HAVE_AVAHI */


/*
 * 'sigterm_handler()' - Handle termination signals.
 */

static void
sigterm_handler(int sig)		/* I - Signal number (unused) */
{
  (void)sig;

  if (job_canceled)
    _exit(CUPS_BACKEND_OK);
  else
    job_canceled = 1;
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
 * End of "$Id: dnssd.c 12970 2015-11-13 20:02:51Z msweet $".
 */
