/*
 * "$Id$"
 *
 *   DNS-SD discovery backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2008 by Apple Inc.
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
 *   get_device()            - Create or update a device.
 *   query_callback()        - Process query data.
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
		*make_and_model;	/* Make and model from TXT record */
  cups_devtype_t type;			/* Device registration type */
  int		priority,		/* Priority associated with type */
		cups_shared,		/* CUPS shared printer? */
		sent;			/* Did we list the device? */
} cups_device_t;


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
static void		unquote(char *dst, const char *src, size_t dstsize);


/*
 * 'main()' - Browse for printers.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
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


 /*
  * Check command-line...
  */

  setbuf(stderr, NULL);

  if (argc >= 6)
    exec_backend(argv);
  else if (argc != 1)
  {
    fputs("Usage: mdns job user title copies options [filename(s)]\n", stderr);
    return (1);
  }

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

  for (;;)
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

      cups_device_t *best;		/* Best matching device */
      char	device_uri[1024];	/* Device URI */
      int	count;			/* Number of queries */
      static const char * const schemes[] =
      		{ "lpd", "ipp", "ipp", "socket", "riousbprint" };
					/* URI schemes for devices */


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

	    if (DNSServiceQueryRecord(&(device->ref),
				      kDNSServiceFlagsShareConnection,
				      0, device->fullName, kDNSServiceType_TXT,
				      kDNSServiceClass_IN, query_callback,
				      devices) != kDNSServiceErr_NoError)
	      fputs("ERROR: Unable to query for TXT records!\n", stderr);
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
	    httpAssembleURI(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri),
			    schemes[best->type], NULL, best->fullName, 0,
			    best->cups_shared ? "/cups" : "/");

	    printf("network %s \"%s\" \"%s\"\n", device_uri,
		   best->make_and_model ? best->make_and_model : "Unknown",
		   best->name);
	    fflush(stdout);

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
	httpAssembleURI(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri),
			schemes[best->type], NULL, best->fullName, 0,
			best->cups_shared ? "/cups" : "/");

	printf("network %s \"%s\" \"%s\"\n", device_uri,
	       best->make_and_model ? best->make_and_model : "Unknown",
	       best->name);
	fflush(stdout);

	best->sent = 1;
      }
    }
  }
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
  int result = strcmp(a->name, b->name);

  if (result)
    return (result);
  else
    return (strcmp(a->domain, b->domain));
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
  * Overwrite the device URIs and run the new backend...
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
  char		fullName[1024];		/* Full name for query */


 /*
  * See if this is a new device...
  */

  key.name   = (char *)serviceName;
  key.domain = (char *)replyDomain;

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
    if (strcasecmp(device->name, key.name) ||
        strcasecmp(device->domain, key.domain))
      break;
    else if (device->type == key.type)
      return (device);

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

  snprintf(fullName, sizeof(fullName), "%s.%s%s", serviceName, regtype,
           replyDomain);
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
		*ptr;			/* Pointer into name */
  cups_device_t	key,			/* Search key */
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

  devices  = (cups_array_t *)context;
  key.name = name;

  unquote(name, fullName, sizeof(name));

  if ((key.domain = strstr(name, "._tcp.")) != NULL)
    key.domain += 6;
  else
    key.domain = (char *)"local.";

  if ((ptr = strstr(name, "._")) != NULL)
    *ptr = '\0';

  if (strstr(fullName, "_ipp._tcp.") ||
      strstr(fullName, "_ipp-tls._tcp."))
    key.type = CUPS_DEVICE_IPP;
  else if (strstr(fullName, "_fax-ipp._tcp."))
    key.type = CUPS_DEVICE_FAX_IPP;
  else if (strstr(fullName, "_printer._tcp."))
    key.type = CUPS_DEVICE_PRINTER;
  else if (strstr(fullName, "_pdl-datastream._tcp."))
    key.type = CUPS_DEVICE_PDL_DATASTREAM;
  else
    key.type = CUPS_DEVICE_RIOUSBPRINT;

  for (device = cupsArrayFind(devices, &key);
       device;
       device = cupsArrayNext(devices))
  {
    if (strcasecmp(device->name, key.name) ||
        strcasecmp(device->domain, key.domain))
    {
      device = NULL;
      break;
    }
    else if (device->type == key.type)
    {
     /*
      * Found it, pull out the priority and make and model from the TXT
      * record and save it...
      */

      const void *value;		/* Pointer to value */
      uint8_t	valueLen;		/* Length of value (max 255) */
      char	make_and_model[512],	/* Manufacturer and model */
		model[256],		/* Model */
		priority[256];		/* Priority */


      value = TXTRecordGetValuePtr(rdlen, rdata, "priority", &valueLen);

      if (value && valueLen)
      {
	memcpy(priority, value, valueLen);
	priority[valueLen] = '\0';
	device->priority = atoi(priority);
      }

      if ((value = TXTRecordGetValuePtr(rdlen, rdata, "usb_MFG",
					&valueLen)) == NULL)
	value = TXTRecordGetValuePtr(rdlen, rdata, "usb_MANUFACTURER",
	                             &valueLen);

      if (value && valueLen)
      {
	memcpy(make_and_model, value, valueLen);
	make_and_model[valueLen] = '\0';
      }
      else
	make_and_model[0] = '\0';

      if ((value = TXTRecordGetValuePtr(rdlen, rdata, "usb_MDL",
					&valueLen)) == NULL)
	value = TXTRecordGetValuePtr(rdlen, rdata, "usb_MODEL", &valueLen);

      if (value && valueLen)
      {
	memcpy(model, value, valueLen);
	model[valueLen] = '\0';
      }
      else if ((value = TXTRecordGetValuePtr(rdlen, rdata, "product",
					     &valueLen)) != NULL && valueLen > 2)
      {
	if (((char *)value)[0] == '(')
	{
	 /*
	  * Strip parenthesis...
	  */

	  memcpy(model, value + 1, valueLen - 2);
	  model[valueLen - 2] = '\0';
	}
	else
	{
	  memcpy(model, value, valueLen);
	  model[valueLen] = '\0';
	}

	if (!strcasecmp(model, "GPL Ghostscript") ||
	    !strcasecmp(model, "GNU Ghostscript") ||
	    !strcasecmp(model, "ESP Ghostscript"))
	{
	  if ((value = TXTRecordGetValuePtr(rdlen, rdata, "ty",
					    &valueLen)) != NULL)
	  {
	    memcpy(model, value, valueLen);
	    model[valueLen] = '\0';

	    if ((ptr = strchr(model, ',')) != NULL)
	      *ptr = '\0';
	  }
	  else
	    strcpy(model, "Unknown");
	}
      }
      else
	strcpy(model, "Unknown");

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

      if ((device->type == CUPS_DEVICE_IPP ||
	   device->type == CUPS_DEVICE_PRINTER) &&
	  TXTRecordGetValuePtr(rdlen, rdata, "printer-type", &valueLen))
      {
       /*
	* This is a CUPS printer!
	*/

	device->cups_shared = 1;

	if (device->type == CUPS_DEVICE_PRINTER)
	  device->sent = 1;
      }

      break;
    }
  }

  if (!device)
    fprintf(stderr, "DEBUG: Ignoring TXT record for \"%s\"...\n", fullName);
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
