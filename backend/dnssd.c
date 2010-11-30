/*
 * "$Id$"
 *
 *   DNS-SD discovery backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2008-2009 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   "LICENSE" which should have been included with this file.  If this
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main()                  - Browse for printers.
 *   browse_callback()       - Browse devices.
 *   browse_local_callback() - Browse local devices.
 *   compare_devices()       - Compare two devices.
 *   exec_backend()          - Execute the backend that corresponds to the
 *                             resolved service name.
 *   get_device()            - Create or update a device.
 *   query_callback()        - Process query data.
 *   sigterm_handler()       - Handle termination signals...
 *   unquote()               - Unquote a name string.
 */

/*
 * Include necessary headers.
 */

#include "backend-private.h"
#include <cups/array.h>
#include <dns_sd.h>


/*
 * Device structure...
 */

typedef enum
{
  CUPS_DEVICE_PRINTER = 0,		/* lpd://... */
  CUPS_DEVICE_IPP,			/* ipp://... */
  CUPS_DEVICE_FAX_IPP,			/* ipp://... */
  CUPS_DEVICE_PDL_DATASTREAM,		/* socket://... */
  CUPS_DEVICE_RIOUSBPRINT		/* riousbprint://... */
} cups_devtype_t;


typedef struct
{
  DNSServiceRef	ref;			/* Service reference for resolve */
  char		*name,			/* Service name */
		*domain,		/* Domain name */
		*fullName,		/* Full name */
		*make_and_model,	/* Make and model from TXT record */
		*device_id;		/* 1284 device ID from TXT record */
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


/*
 * Local functions...
 */

static void		browse_callback(DNSServiceRef sdRef,
			                DNSServiceFlags flags,
				        uint32_t interfaceIndex,
				        DNSServiceErrorType errorCode,
				        const char *serviceName,
				        const char *regtype,
				        const char *replyDomain, void *context);
static void		browse_local_callback(DNSServiceRef sdRef,
					      DNSServiceFlags flags,
					      uint32_t interfaceIndex,
					      DNSServiceErrorType errorCode,
					      const char *serviceName,
					      const char *regtype,
					      const char *replyDomain,
					      void *context);
static int		compare_devices(cups_device_t *a, cups_device_t *b);
static void		exec_backend(char **argv);
static cups_device_t	*get_device(cups_array_t *devices,
			            const char *serviceName,
			            const char *regtype,
				    const char *replyDomain);
static void		query_callback(DNSServiceRef sdRef,
			               DNSServiceFlags flags,
				       uint32_t interfaceIndex,
				       DNSServiceErrorType errorCode,
				       const char *fullName, uint16_t rrtype,
				       uint16_t rrclass, uint16_t rdlen,
				       const void *rdata, uint32_t ttl,
				       void *context);
static void		sigterm_handler(int sig);
static void		unquote(char *dst, const char *src, size_t dstsize);


/*
 * 'main()' - Browse for printers.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  const char	*name;			/* Backend name */
  DNSServiceRef	main_ref,		/* Main service reference */
		fax_ipp_ref,		/* IPP fax service reference */
		ipp_ref,		/* IPP service reference */
		ipp_tls_ref,		/* IPP w/TLS service reference */
		local_fax_ipp_ref,	/* Local IPP fax service reference */
		local_ipp_ref,		/* Local IPP service reference */
		local_ipp_tls_ref,	/* Local IPP w/TLS service reference */
		local_printer_ref,	/* Local LPD service reference */
		pdl_datastream_ref,	/* AppSocket service reference */
		printer_ref,		/* LPD service reference */
		riousbprint_ref;	/* Remote IO service reference */
  int		fd;			/* Main file descriptor */
  fd_set	input;			/* Input set for select() */
  struct timeval timeout;		/* Timeout for select() */
  cups_array_t	*devices;		/* Device array */
  cups_device_t	*device;		/* Current device */
  char		uriName[1024];		/* Unquoted fullName for URI */
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

 /*
  * Loop until we are killed...
  */

  while (!job_canceled)
  {
    FD_ZERO(&input);
    FD_SET(fd, &input);

    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;

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
    {
     /*
      * Announce any devices we've found...
      */

      DNSServiceErrorType status;	/* DNS query status */
      cups_device_t *best;		/* Best matching device */
      char	device_uri[1024];	/* Device URI */
      int	count;			/* Number of queries */


      for (device = (cups_device_t *)cupsArrayFirst(devices),
               best = NULL, count = 0;
           device;
	   device = (cups_device_t *)cupsArrayNext(devices))
        if (!device->ref && !device->sent)
	{
	 /*
	  * Found the device, now get the TXT record(s) for it...
	  */

          if (count < 10)
	  {
	    device->ref = main_ref;

	    fprintf(stderr, "DEBUG: Querying \"%s\"...\n", device->fullName);

	    status = DNSServiceQueryRecord(&(device->ref),
				           kDNSServiceFlagsShareConnection,
				           0, device->fullName,
					   kDNSServiceType_TXT,
				           kDNSServiceClass_IN, query_callback,
				           devices);
            if (status != kDNSServiceErr_NoError)
	    {
	      fputs("ERROR: Unable to query for TXT records!\n", stderr);
	      fprintf(stderr, "DEBUG: DNSServiceQueryRecord returned %d\n",
	              status);
            }
	    else
	      count ++;
          }
	}
	else if (!device->sent)
	{
	 /*
	  * Got the TXT records, now report the device...
	  */

	  DNSServiceRefDeallocate(device->ref);
	  device->ref = 0;

          if (!best)
	    best = device;
	  else if (strcasecmp(best->name, device->name) ||
	           strcasecmp(best->domain, device->domain))
          {
	    unquote(uriName, best->fullName, sizeof(uriName));

	    httpAssembleURI(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri),
			    "dnssd", NULL, uriName, 0,
			    best->cups_shared ? "/cups" : "/");

	    cupsBackendReport("network", device_uri, best->make_and_model,
	                      best->name, best->device_id, NULL);
	    best->sent = 1;
	    best       = device;
	  }
	  else if (best->priority > device->priority ||
	           (best->priority == device->priority &&
		    best->type < device->type))
          {
	    best->sent = 1;
	    best       = device;
	  }
	  else
	    device->sent = 1;
        }

      if (best)
      {
	unquote(uriName, best->fullName, sizeof(uriName));

	httpAssembleURI(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri),
			"dnssd", NULL, uriName, 0,
			best->cups_shared ? "/cups" : "/");

	cupsBackendReport("network", device_uri, best->make_and_model,
			  best->name, best->device_id, NULL);
	best->sent = 1;
      }
    }
  }

  return (CUPS_BACKEND_OK);
}


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
	  serviceName ? serviceName : "(null)",
	  regtype ? regtype : "(null)",
	  replyDomain ? replyDomain : "(null)",
	  context);

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
	  serviceName ? serviceName : "(null)",
	  regtype ? regtype : "(null)",
	  replyDomain ? replyDomain : "(null)",
	  context);

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

  if ((resolved_uri = cupsBackendDeviceURI(argv)) == NULL)
    exit(CUPS_BACKEND_FAILED);

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

  if (!strcmp(regtype, "_ipp._tcp.") ||
      !strcmp(regtype, "_ipp-tls._tcp."))
    key.type = CUPS_DEVICE_IPP;
  else if (!strcmp(regtype, "_fax-ipp._tcp."))
    key.type = CUPS_DEVICE_FAX_IPP;
  else if (!strcmp(regtype, "_printer._tcp."))
    key.type = CUPS_DEVICE_PRINTER;
  else if (!strcmp(regtype, "_pdl-datastream._tcp."))
    key.type = CUPS_DEVICE_PDL_DATASTREAM;
  else
    key.type = CUPS_DEVICE_RIOUSBPRINT;

  for (device = cupsArrayFind(devices, &key);
       device;
       device = cupsArrayNext(devices))
    if (strcasecmp(device->name, key.name))
      break;
    else if (device->type == key.type)
    {
      if (!strcasecmp(device->domain, "local.") &&
          strcasecmp(device->domain, replyDomain))
      {
       /*
        * Update the .local listing to use the "global" domain name instead.
	* The backend will try local lookups first, then the global domain name.
	*/

        free(device->domain);
	device->domain = strdup(replyDomain);

	DNSServiceConstructFullName(fullName, device->name, regtype,
	                            replyDomain);
	free(device->fullName);
	device->fullName = strdup(fullName);
      }

      return (device);
    }

 /*
  * Yes, add the device...
  */

  fprintf(stderr, "DEBUG: Found \"%s.%s%s\"...\n", serviceName, regtype,
	  replyDomain);

  device           = calloc(sizeof(cups_device_t), 1);
  device->name     = strdup(serviceName);
  device->domain   = strdup(replyDomain);
  device->type     = key.type;
  device->priority = 50;

  cupsArrayAdd(devices, device);

 /*
  * Set the "full name" of this service, which is used for queries...
  */

  DNSServiceConstructFullName(fullName, serviceName, regtype, replyDomain);
  device->fullName = strdup(fullName);

  return (device);
}


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
    void                *context)	/* I - Devices array */
{
  cups_array_t	*devices;		/* Device array */
  char		name[1024],		/* Service name */
		*ptr;			/* Pointer into string */
  cups_device_t	dkey,			/* Search key */
		*device;		/* Device */


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

 /*
  * Lookup the service in the devices array.
  */

  devices   = (cups_array_t *)context;
  dkey.name = name;

  unquote(name, fullName, sizeof(name));

  if ((dkey.domain = strstr(name, "._tcp.")) != NULL)
    dkey.domain += 6;
  else
    dkey.domain = (char *)"local.";

  if ((ptr = strstr(name, "._")) != NULL)
    *ptr = '\0';

  if (strstr(fullName, "_ipp._tcp.") ||
      strstr(fullName, "_ipp-tls._tcp."))
    dkey.type = CUPS_DEVICE_IPP;
  else if (strstr(fullName, "_fax-ipp._tcp."))
    dkey.type = CUPS_DEVICE_FAX_IPP;
  else if (strstr(fullName, "_printer._tcp."))
    dkey.type = CUPS_DEVICE_PRINTER;
  else if (strstr(fullName, "_pdl-datastream._tcp."))
    dkey.type = CUPS_DEVICE_PDL_DATASTREAM;
  else
    dkey.type = CUPS_DEVICE_RIOUSBPRINT;

  for (device = cupsArrayFind(devices, &dkey);
       device;
       device = cupsArrayNext(devices))
  {
    if (strcasecmp(device->name, dkey.name) ||
        strcasecmp(device->domain, dkey.domain))
    {
      device = NULL;
      break;
    }
    else if (device->type == dkey.type)
    {
     /*
      * Found it, pull out the priority and make and model from the TXT
      * record and save it...
      */

      const uint8_t	*data,		/* Pointer into data */
			*datanext,	/* Next key/value pair */
			*dataend;	/* End of entire TXT record */
      uint8_t		datalen;	/* Length of current key/value pair */
      char		key[256],	/* Key string */
			value[256],	/* Value string */
			make_and_model[512],
				      	/* Manufacturer and model */
			model[256],	/* Model */
			device_id[2048];/* 1284 device ID */


      device_id[0]      = '\0';
      make_and_model[0] = '\0';

      strcpy(model, "Unknown");

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

        if (!datalen || (data + datalen) >= dataend)
	  break;

        datanext = data + datalen;

        for (ptr = key; data < datanext && *data != '='; data ++)
	  *ptr++ = *data;
	*ptr = '\0';

	if (data < datanext && *data == '=')
	{
	  data ++;

	  if (data < datanext)
	    memcpy(value, data, datanext - data);
	  value[datanext - data] = '\0';
	}
	else
	  continue;

        if (!strncasecmp(key, "usb_", 4))
	{
	 /*
	  * Add USB device ID information...
	  */

	  ptr = device_id + strlen(device_id);
	  snprintf(ptr, sizeof(device_id) - (ptr - device_id), "%s:%s;",
	           key + 4, value);
        }

        if (!strcasecmp(key, "usb_MFG") || !strcasecmp(key, "usb_MANU") ||
	    !strcasecmp(key, "usb_MANUFACTURER"))
	  strcpy(make_and_model, value);
        else if (!strcasecmp(key, "usb_MDL") || !strcasecmp(key, "usb_MODEL"))
	  strcpy(model, value);
	else if (!strcasecmp(key, "product") && !strstr(value, "Ghostscript"))
	{
	  if (value[0] == '(')
	  {
	   /*
	    * Strip parenthesis...
	    */

            if ((ptr = value + strlen(value) - 1) > value && *ptr == ')')
	      *ptr = '\0';

	    strcpy(model, value + 1);
	  }
	  else
	    strcpy(model, value);
        }
	else if (!strcasecmp(key, "ty"))
	{
          strcpy(model, value);

	  if ((ptr = strchr(model, ',')) != NULL)
	    *ptr = '\0';
	}
	else if (!strcasecmp(key, "priority"))
	  device->priority = atoi(value);
	else if ((device->type == CUPS_DEVICE_IPP ||
	          device->type == CUPS_DEVICE_PRINTER) &&
		 !strcasecmp(key, "printer-type"))
	{
	 /*
	  * This is a CUPS printer!
	  */

	  device->cups_shared = 1;

	  if (device->type == CUPS_DEVICE_PRINTER)
	    device->sent = 1;
	}
      }

      if (device->device_id)
        free(device->device_id);

      if (!device_id[0] && strcmp(model, "Unknown"))
      {
        if (make_and_model[0])
	  snprintf(device_id, sizeof(device_id), "MFG:%s;MDL:%s;",
	           make_and_model, model);
        else if (!strncasecmp(model, "designjet ", 10))
	  snprintf(device_id, sizeof(device_id), "MFG:HP;MDL:%s", model + 10);
        else if (!strncasecmp(model, "stylus ", 7))
	  snprintf(device_id, sizeof(device_id), "MFG:EPSON;MDL:%s", model + 7);
        else if ((ptr = strchr(model, ' ')) != NULL)
	{
	 /*
	  * Assume the first word is the make...
	  */

          memcpy(make_and_model, model, ptr - model);
	  make_and_model[ptr - model] = '\0';

	  snprintf(device_id, sizeof(device_id), "MFG:%s;MDL:%s",
		   make_and_model, ptr + 1);
        }
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
      break;
    }
  }

  if (!device)
    fprintf(stderr, "DEBUG: Ignoring TXT record for \"%s\"...\n", fullName);
}


/*
 * 'sigterm_handler()' - Handle termination signals...
 */

static void
sigterm_handler(int sig)		/* I - Signal number (unused) */
{
  if (job_canceled)
    exit(CUPS_BACKEND_OK);
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
 * End of "$Id$".
 */
