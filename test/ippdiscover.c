/*
 * "$Id: ippdiscover.c 10983 2013-05-13 23:57:32Z msweet $"
 *
 *   ippdiscover command for CUPS.
 *
 *   Copyright 2007-2013 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
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
 */


/*
 * Include necessary headers.
 */

#include <cups/cups-private.h>
#ifdef HAVE_DNSSD
#  include <dns_sd.h>
#  ifdef WIN32
#    pragma comment(lib, "dnssd.lib")
#  endif /* WIN32 */
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
 * Local globals...
 */

#ifdef HAVE_AVAHI
static int		got_data = 0;	/* Got data from poll? */
static AvahiSimplePoll	*simple_poll = NULL;
					/* Poll information */
#endif /* HAVE_AVAHI */
static const char	*program = NULL;/* Program to run */


/*
 * Local functions...
 */

#ifdef HAVE_DNSSD
static void DNSSD_API	browse_callback(DNSServiceRef sdRef,
			                DNSServiceFlags flags,
				        uint32_t interfaceIndex,
				        DNSServiceErrorType errorCode,
				        const char *serviceName,
				        const char *regtype,
				        const char *replyDomain, void *context)
					__attribute__((nonnull(1,5,6,7,8)));
static void DNSSD_API	resolve_cb(DNSServiceRef sdRef,
				   DNSServiceFlags flags,
				   uint32_t interfaceIndex,
				   DNSServiceErrorType errorCode,
				   const char *fullName,
				   const char *hostTarget,
				   uint16_t port, uint16_t txtLen,
				   const unsigned char *txtRecord,
				   void *context);
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
static void		client_cb(AvahiClient *client, AvahiClientState state,
				  void *simple_poll);
static int		poll_cb(struct pollfd *pollfds, unsigned int num_pollfds,
				int timeout, void *context);
static void		resolve_cb(AvahiServiceResolver *resolver,
				   AvahiIfIndex interface,
				   AvahiProtocol protocol,
				   AvahiResolverEvent event,
				   const char *name, const char *type,
				   const char *domain, const char *host_name,
				   const AvahiAddress *address, uint16_t port,
				   AvahiStringList *txt,
				   AvahiLookupResultFlags flags, void *context);
#endif /* HAVE_AVAHI */

static void		resolve_and_run(const char *name, const char *type,
			                const char *domain);
static void		unquote(char *dst, const char *src, size_t dstsize);
static void		usage(void) __attribute__((noreturn));


/*
 * 'main()' - Browse for printers and run the specified command.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  const char	*opt,			/* Current option character */
		*name = NULL,		/* Service name */
		*type = "_ipp._tcp",	/* Service type */
		*domain = "local.";	/* Service domain */
#ifdef HAVE_DNSSD
  DNSServiceRef	ref;			/* Browsing service reference */
#endif /* HAVE_DNSSD */
#ifdef HAVE_AVAHI
  AvahiClient	*client;		/* Client information */
  int		error;			/* Error code, if any */
#endif /* HAVE_AVAHI */


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

      for (device = (cups_device_t *)cupsArrayFirst(devices);
           device;
	   device = (cups_device_t *)cupsArrayNext(devices))
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


      for (device = (cups_device_t *)cupsArrayFirst(devices), count = 0;
           device;
	   device = (cups_device_t *)cupsArrayNext(devices))
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

  for (device = (cups_device_t *)cupsArrayFirst(devices);
       device;
       device = (cups_device_t *)cupsArrayNext(devices))
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
#ifdef DEBUG
  fprintf(stderr, "browse_callback(sdRef=%p, flags=%x, "
                  "interfaceIndex=%d, errorCode=%d, serviceName=\"%s\", "
		  "regtype=\"%s\", replyDomain=\"%s\", context=%p)\n",
          sdRef, flags, interfaceIndex, errorCode,
	  serviceName ? serviceName : "(null)",
	  regtype ? regtype : "(null)",
	  replyDomain ? replyDomain : "(null)",
	  context);
#endif /* DEBUG */

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
 * 'compare_devices()' - Compare two devices.
 */

static int				/* O - Result of comparison */
compare_devices(cups_device_t *a,	/* I - First device */
                cups_device_t *b)	/* I - Second device */
{
  int retval = strcmp(a->name, b->name);

  if (retval)
    return (retval);
  else
    return (-strcmp(a->regtype, b->regtype));
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

  key.name    = (char *)serviceName;
  key.regtype = (char *)regtype;

  for (device = cupsArrayFind(devices, &key);
       device;
       device = cupsArrayNext(devices))
    if (strcasecmp(device->name, key.name))
      break;
    else
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

  device          = calloc(sizeof(cups_device_t), 1);
  device->name    = strdup(serviceName);
  device->domain  = strdup(replyDomain);
  device->regtype = strdup(regtype);

  cupsArrayAdd(devices, device);

 /*
  * Set the "full name" of this service, which is used for queries...
  */

  DNSServiceConstructFullName(fullName, serviceName, regtype, replyDomain);
  device->fullName = strdup(fullName);

#ifdef DEBUG
  fprintf(stderr, "get_device: fullName=\"%s\"...\n", fullName);
#endif /* DEBUG */

  return (device);
}


/*
 * 'progress()' - Show query progress.
 */

static void
progress(void)
{
#ifndef DEBUG
  const char	*chars = "|/-\\";
  static int	count = 0;


  fprintf(stderr, "\rLooking for printers %c", chars[count]);
  fflush(stderr);
  count = (count + 1) & 3;
#endif /* !DEBUG */
}


/*
 * 'resolve_callback()' - Process resolve data.
 */

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
    void                *context)	/* I - Device */
{
  char		temp[257],		/* TXT key value */
		uri[1024];		/* Printer URI */
  const void	*value;			/* Value from TXT record */
  uint8_t	valueLen;		/* Length of value */
  cups_device_t	*device = (cups_device_t *)context;
					/* Device */


#ifdef DEBUG
  fprintf(stderr, "\rresolve_callback(sdRef=%p, flags=%x, "
                  "interfaceIndex=%d, errorCode=%d, fullName=\"%s\", "
		  "hostTarget=\"%s\", port=%d, txtLen=%u, txtRecord=%p, "
		  "context=%p)\n",
          sdRef, flags, interfaceIndex, errorCode,
	  fullName ? fullName : "(null)", hostTarget ? hostTarget : "(null)",
	  ntohs(port), txtLen, txtRecord, context);
#endif /* DEBUG */

 /*
  * Only process "add" data...
  */

  if (errorCode != kDNSServiceErr_NoError)
    return;

  device->got_resolve = 1;
  device->host        = strdup(hostTarget);
  device->port        = ntohs(port);

 /*
  * Extract the "remote printer" key from the TXT record and save the URI...
  */

  if ((value = TXTRecordGetValuePtr(txtLen, txtRecord, "rp",
                                    &valueLen)) != NULL)
  {
    if (((char *)value)[0] == '/')
    {
     /*
      * "rp" value (incorrectly) has a leading slash already...
      */

      memcpy(temp, value, valueLen);
      temp[valueLen] = '\0';
    }
    else
    {
     /*
      * Convert to resource by concatenating with a leading "/"...
      */

      temp[0] = '/';
      memcpy(temp + 1, value, valueLen);
      temp[valueLen + 1] = '\0';
    }
  }
  else
  {
   /*
    * Default "rp" value is blank, mapping to a path of "/"...
    */

    temp[0] = '/';
    temp[1] = '\0';
  }

  if (!strncmp(temp, "/printers/", 10))
    device->cups_shared = -1;

  httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp",
                  NULL, hostTarget, ntohs(port), temp);
  device->uri = strdup(uri);
  device->rp  = strdup(temp);

  if ((value = TXTRecordGetValuePtr(txtLen, txtRecord, "ty",
                                    &valueLen)) != NULL)
  {
    memcpy(temp, value, valueLen);
    temp[valueLen] = '\0';

    device->ty = strdup(temp);
  }

  if ((value = TXTRecordGetValuePtr(txtLen, txtRecord, "pdl",
                                    &valueLen)) != NULL)
  {
    memcpy(temp, value, valueLen);
    temp[valueLen] = '\0';

    device->pdl = strdup(temp);
  }

  if ((value = TXTRecordGetValuePtr(txtLen, txtRecord, "printer-type",
                                    &valueLen)) != NULL)
    device->cups_shared = 1;

  if (device->cups_shared)
    fprintf(stderr, "\rIgnoring CUPS printer %s\n", uri);
  else
    fprintf(stderr, "\rFound IPP printer %s\n", uri);

  progress();
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
 * 'usage()' - Show program usage and exit.
 */

static void
usage(void)
{
  _cupsLangPuts(stdout, _("Usage: ippdiscover [options] -a\n"
                          "       ippdiscover [options] \"service name\"\n"
                          "\n"
                          "Options:"));
  _cupsLangPuts(stdout, _("  -a                      Browse for all services."));
  _cupsLangPuts(stdout, _("  -d domain               Browse/resolve in specified domain."));
  _cupsLangPuts(stdout, _("  -p program              Run specified program for each service."));
  _cupsLangPuts(stdout, _("  -t type                 Browse/resolve with specified type."));

  exit(0);
}


/*
 * End of "$Id: ippdiscover.c 10983 2013-05-13 23:57:32Z msweet $".
 */
