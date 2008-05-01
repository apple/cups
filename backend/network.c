/*
 * "$Id$"
 *
 *   Common network APIs for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 2006-2007 by Easy Software Products, all rights reserved.
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
 */

/*
 * Include necessary headers.
 */

#include "backend-private.h"
#include <limits.h>
#ifdef __hpux
#  include <sys/time.h>
#else
#  include <sys/select.h>
#endif /* __hpux */
#ifdef HAVE_DNSSD
#  include <dns_sd.h>
#endif /* HAVE_DNSSD */


/*
 * Local functions...
 */

#ifdef HAVE_DNSSD
static void	resolve_callback(DNSServiceRef sdRef, DNSServiceFlags flags,
				 uint32_t interfaceIndex,
				 DNSServiceErrorType errorCode,
				 const char *fullName, const char *hostTarget,
				 uint16_t port, uint16_t txtLen,
				 const unsigned char *txtRecord, void *context);
#endif /* HAVE_DNSSD */


/*
 * 'backendCheckSideChannel()' - Check the side-channel for pending requests.
 */


void
backendCheckSideChannel(
    int         snmp_fd,		/* I - SNMP socket */
    http_addr_t *addr)			/* I - Address of device */
{
  fd_set	input;			/* Select input set */
  struct timeval timeout;		/* Select timeout */


  FD_ZERO(&input);
  FD_SET(CUPS_SC_FD, &input);

  timeout.tv_sec = timeout.tv_usec = 0;

  if (select(CUPS_SC_FD + 1, &input, NULL, NULL, &timeout) > 0)
    backendNetworkSideCB(-1, -1, snmp_fd, addr, 0);
}


/*
 * 'backendNetworkSideCB()' - Handle common network side-channel commands.
 */

void
backendNetworkSideCB(
    int         print_fd,		/* I - Print file or -1 */
    int         device_fd,		/* I - Device file or -1 */
    int         snmp_fd,		/* I - SNMP socket */
    http_addr_t *addr,			/* I - Address of device */
    int         use_bc)			/* I - Use back-channel data? */
{
  cups_sc_command_t	command;	/* Request command */
  cups_sc_status_t	status;		/* Request/response status */
  char			data[2048];	/* Request/response data */
  int			datalen;	/* Request/response data size */
  const char		*device_id;	/* 1284DEVICEID env var */


  datalen = sizeof(data);

  if (cupsSideChannelRead(&command, &status, data, &datalen, 1.0))
  {
    _cupsLangPuts(stderr, _("WARNING: Failed to read side-channel request!\n"));
    return;
  }

  switch (command)
  {
    case CUPS_SC_CMD_DRAIN_OUTPUT :
       /*
        * Our sockets disable the Nagle algorithm and data is sent immediately.
	*/

        if (device_fd < 0)
	  status = CUPS_SC_STATUS_NOT_IMPLEMENTED;
	else if (backendDrainOutput(print_fd, device_fd))
	  status = CUPS_SC_STATUS_IO_ERROR;
	else 
          status = CUPS_SC_STATUS_OK;

	datalen = 0;
        break;

    case CUPS_SC_CMD_GET_BIDI :
        data[0] = use_bc;
        datalen = 1;
        break;

    case CUPS_SC_CMD_GET_DEVICE_ID :
        if (snmp_fd >= 0)
	{
	  cups_snmp_t	packet;		/* Packet from printer */
	  static const int ppmPrinterIEEE1284DeviceId[] =
	  		{ CUPS_OID_ppmPrinterIEEE1284DeviceId,1,-1 };

          if (_cupsSNMPWrite(snmp_fd, addr, 1, _cupsSNMPDefaultCommunity(),
	                     CUPS_ASN1_GET_REQUEST, 1,
			     ppmPrinterIEEE1284DeviceId))
          {
	    if (_cupsSNMPRead(snmp_fd, &packet, 1.0) &&
	        packet.object_type == CUPS_ASN1_OCTET_STRING)
	    {
	      strlcpy(data, packet.object_value.string, sizeof(data));
	      datalen = (int)strlen(data);
	      break;
	    }
	  }
        }

	if ((device_id = getenv("1284DEVICEID")) != NULL)
	{
	  strlcpy(data, device_id, sizeof(data));
	  datalen = (int)strlen(data);
	  break;
	}

    default :
        status  = CUPS_SC_STATUS_NOT_IMPLEMENTED;
	datalen = 0;
	break;
  }

  cupsSideChannelWrite(command, status, data, datalen, 1.0);
}


/*
 * 'backendResolveURI()' - Get the device URI, resolving as needed.
 */

const char *				/* O - Device URI */
backendResolveURI(char **argv)		/* I - Command-line arguments */
{
  const char		*uri;		/* Device URI */
  char			scheme[32],	/* URI components... */
			userpass[256],
			hostname[1024],
			resource[1024];
  int			port;
  http_uri_status_t	status;		/* URI decode status */

 /*
  * Get the device URI...
  */

  uri = cupsBackendDeviceURI(argv);

  if ((status = httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme,
                                sizeof(scheme), userpass, sizeof(userpass),
				hostname, sizeof(hostname), &port,
				resource, sizeof(resource))) < HTTP_URI_OK)
  {
    fprintf(stderr, "ERROR: Bad device URI \"%s\" (%d)!\n", uri, status);
    exit (CUPS_BACKEND_STOP);
  }

 /*
  * Resolve it as needed...
  */

  if (strstr(hostname, "._tcp"))
  {
#ifdef HAVE_DNSSD
    DNSServiceRef	ref;		/* DNS-SD service reference */
    char		*regtype,	/* Pointer to type in hostname */
			*domain;	/* Pointer to domain in hostname */
    static char		resolved_uri[HTTP_MAX_URI];
    					/* Resolved device URI */

   /*
    * Separate the hostname into service name, registration type, and domain...
    */

    regtype = strchr(hostname, '.');
    *regtype++ = '\0';

    domain = regtype + strlen(regtype) - 1;
    if (domain > regtype && *domain == '.')
      *domain = '\0';

    for (domain = strchr(regtype, '.');
         domain;
	 domain = strchr(domain + 1, '.'))
      if (domain[1] != '_')
        break;

    if (domain)
      *domain++ = '\0';

    fprintf(stderr,
            "DEBUG: Resolving service \"%s\", regtype \"%s\", domain \"%s\"\n",
	    hostname, regtype, domain ? domain : "(null)");

    if (DNSServiceResolve(&ref, 0, 0, hostname, regtype, domain,
			  resolve_callback,
			  resolved_uri) == kDNSServiceErr_NoError)
    {
      if (DNSServiceProcessResult(ref) != kDNSServiceErr_NoError)
        uri = NULL;
      else
        uri = resolved_uri;

      DNSServiceRefDeallocate(ref);
    }
    else
#endif /* HAVE_DNSSD */

    uri = NULL;

    if (!uri)
    {
      fprintf(stderr, "ERROR: Unable to resolve DNS-SD service \"%s\"!\n", uri);
      exit(CUPS_BACKEND_STOP);
    }
  }

  return (uri);
}


#ifdef HAVE_DNSSD
/*
 * 'resolve_callback()' - Build a device URI for the given service name.
 */

static void
resolve_callback(
    DNSServiceRef       sdRef,		/* I - Service reference */
    DNSServiceFlags     flags,		/* I - Results flags */
    uint32_t            interfaceIndex,	/* I - Interface number */
    DNSServiceErrorType errorCode,	/* I - Error, if any */
    const char          *fullName,	/* I - Full service name */
    const char          *hostTarget,	/* I - Hostname */
    uint16_t            port,		/* I - Port number */
    uint16_t            txtLen,		/* I - Length of TXT record */
    const unsigned char *txtRecord,	/* I - TXT record data */
    void                *context)	/* I - Pointer to URI buffer */
{
  const char	*scheme;		/* URI scheme */
  char		rp[257];		/* Remote printer */
  const void	*value;			/* Value from TXT record */
  uint8_t	valueLen;		/* Length of value */


  fprintf(stderr,
          "DEBUG2: resolve_callback(sdRef=%p, flags=%x, interfaceIndex=%u, "
	  "errorCode=%d, fullName=\"%s\", hostTarget=\"%s\", port=%u, "
	  "txtLen=%u, txtRecord=%p, context=%p)\n", sdRef, flags,
	  interfaceIndex, errorCode, fullName, hostTarget, port, txtLen,
	  txtRecord, context);

 /*
  * Figure out the scheme from the full name...
  */

  if (strstr(fullName, "._ipp"))
    scheme = "ipp";
  else if (strstr(fullName, "._printer."))
    scheme = "lpd";
  else if (strstr(fullName, "._pdl-datastream."))
    scheme = "socket";
  else
    scheme = "riousbprint";

 /*
  * Extract the "remote printer" key from the TXT record...
  */

  if ((value = TXTRecordGetValuePtr(txtLen, txtRecord, "rp",
                                    &valueLen)) != NULL)
  {
   /*
    * Convert to resource by concatenating with a leading "/"...
    */

    rp[0] = '/';
    memcpy(rp, value, valueLen);
    rp[valueLen + 1] = '\0';
  }
  else
    rp[0] = '\0';

 /*
  * Assemble the final device URI...
  */

  httpAssembleURI(HTTP_URI_CODING_ALL, (char *)context, HTTP_MAX_URI, scheme,
                  NULL, hostTarget, ntohs(port), rp);

  fprintf(stderr, "DEBUG: Resolved URI is \"%s\"...\n", (char *)context);
}
#endif /* HAVE_DNSSD */


/*
 * End of "$Id$".
 */
