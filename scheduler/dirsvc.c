/*
 * "$Id: dirsvc.c 7933 2008-09-11 00:44:58Z mike $"
 *
 *   Directory services routines for the CUPS scheduler.
 *
 *   Copyright 2007-2011 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cupsdDeregisterPrinter() - Stop sending broadcast information for a local
 *				printer and remove any pending references to
 *				remote printers.
 *   cupsdRegisterPrinter()   - Start sending broadcast information for a
 *				printer or update the broadcast contents.
 *   cupsdStartBrowsing()     - Start sending and receiving broadcast
 *				information.
 *   cupsdStopBrowsing()      - Stop sending and receiving broadcast
 *				information.
 *   cupsdUpdateDNSSDName()   - Update the computer name we use for browsing...
 *   dnssdAddAlias()	      - Add a DNS-SD alias name.
 *   dnssdBuildTxtRecord()    - Build a TXT record from printer info.
 *   dnssdDeregisterPrinter() - Stop sending broadcast information for a
 *				printer.
 *   dnssdPackTxtRecord()     - Pack an array of key/value pairs into the TXT
 *				record format.
 *   dnssdRegisterCallback()  - DNSServiceRegister callback.
 *   dnssdRegisterPrinter()   - Start sending broadcast information for a
 *				printer or update the broadcast contents.
 *   dnssdStop()	      - Stop all DNS-SD registrations.
 *   dnssdUpdate()	      - Handle DNS-SD queries.
 *   get_auth_info_required() - Get the auth-info-required value to advertise.
 *   get_hostconfig()	      - Get an /etc/hostconfig service setting.
 *   update_lpd()	      - Update the LPD configuration as needed.
 *   update_smb()	      - Update the SMB configuration as needed.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <grp.h>

#ifdef HAVE_DNSSD
#  include <dns_sd.h>
#  ifdef __APPLE__
#    include <nameser.h>
#    ifdef HAVE_COREFOUNDATION
#      include <CoreFoundation/CoreFoundation.h>
#    endif /* HAVE_COREFOUNDATION */
#    ifdef HAVE_SYSTEMCONFIGURATION
#      include <SystemConfiguration/SystemConfiguration.h>
#    endif /* HAVE_SYSTEMCONFIGURATION */
#  endif /* __APPLE__ */
#endif /* HAVE_DNSSD */


/*
 * Local functions...
 */

static char	*get_auth_info_required(cupsd_printer_t *p, char *buffer,
		                        size_t bufsize);
#ifdef __APPLE__
static int	get_hostconfig(const char *name);
#endif /* __APPLE__ */
static void	update_lpd(int onoff);
static void	update_smb(int onoff);


#ifdef HAVE_DNSSD
#  ifdef HAVE_COREFOUNDATION
static void	dnssdAddAlias(const void *key, const void *value,
		              void *context);
#  endif /* HAVE_COREFOUNDATION */
static char	*dnssdBuildTxtRecord(int *txt_len, cupsd_printer_t *p,
		                     int for_lpd);
static void	dnssdDeregisterPrinter(cupsd_printer_t *p);
static char	*dnssdPackTxtRecord(int *txt_len, char *keyvalue[][2],
		                    int count);
static void	dnssdRegisterCallback(DNSServiceRef sdRef,
		                      DNSServiceFlags flags,
				      DNSServiceErrorType errorCode,
				      const char *name, const char *regtype,
				      const char *domain, void *context);
static void	dnssdRegisterPrinter(cupsd_printer_t *p);
static void	dnssdStop(void);
static void	dnssdUpdate(void);
#endif /* HAVE_DNSSD */


/*
 * 'cupsdDeregisterPrinter()' - Stop sending broadcast information for a
 *				local printer and remove any pending
 *                              references to remote printers.
 */

void
cupsdDeregisterPrinter(
    cupsd_printer_t *p,			/* I - Printer to register */
    int             removeit)		/* I - Printer being permanently removed */
{
 /*
  * Only deregister if browsing is enabled and it's a local printer...
  */

  cupsdLogMessage(CUPSD_LOG_DEBUG,
                  "cupsdDeregisterPrinter(p=%p(%s), removeit=%d)", p, p->name,
		  removeit);

  if (!Browsing || !p->shared ||
      (p->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_SCANNER)))
    return;

 /*
  * Announce the deletion...
  */

#ifdef HAVE_DNSSD
  if (removeit && (BrowseLocalProtocols & BROWSE_DNSSD) && DNSSDRef)
    dnssdDeregisterPrinter(p);
#endif /* HAVE_DNSSD */
}


/*
 * 'cupsdRegisterPrinter()' - Start sending broadcast information for a
 *                            printer or update the broadcast contents.
 */

void
cupsdRegisterPrinter(cupsd_printer_t *p)/* I - Printer */
{
  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdRegisterPrinter(p=%p(%s))", p,
                  p->name);

  if (!Browsing || !BrowseLocalProtocols ||
      (p->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_SCANNER)))
    return;

#ifdef HAVE_DNSSD
  if ((BrowseLocalProtocols & BROWSE_DNSSD) && DNSSDRef)
    dnssdRegisterPrinter(p);
#endif /* HAVE_DNSSD */
}


/*
 * 'cupsdStartBrowsing()' - Start sending and receiving broadcast information.
 */

void
cupsdStartBrowsing(void)
{
  cupsd_printer_t	*p;		/* Current printer */


  if (!Browsing || !BrowseLocalProtocols)
    return;

#ifdef HAVE_DNSSD
  if (BrowseLocalProtocols & BROWSE_DNSSD)
  {
    DNSServiceErrorType error;		/* Error from service creation */
    cupsd_listener_t	*lis;		/* Current listening socket */


   /*
    * First create a "master" connection for all registrations...
    */

    if ((error = DNSServiceCreateConnection(&DNSSDRef))
	    != kDNSServiceErr_NoError)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
		      "Unable to create master DNS-SD reference: %d", error);

      if (FatalErrors & CUPSD_FATAL_BROWSE)
	cupsdEndProcess(getpid(), 0);
    }
    else
    {
     /*
      * Add the master connection to the select list...
      */

      int fd = DNSServiceRefSockFD(DNSSDRef);

      fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);

      cupsdAddSelect(fd, (cupsd_selfunc_t)dnssdUpdate, NULL, NULL);

     /*
      * Then get the port we use for registrations.  If we are not listening
      * on any non-local ports, there is no sense sharing local printers via
      * Bonjour...
      */

      DNSSDPort = 0;

      for (lis = (cupsd_listener_t *)cupsArrayFirst(Listeners);
	   lis;
	   lis = (cupsd_listener_t *)cupsArrayNext(Listeners))
      {
	if (httpAddrLocalhost(&(lis->address)))
	  continue;

        DNSSDPort = _httpAddrPort(&(lis->address));
	break;
      }

     /*
      * Set the computer name and register the web interface...
      */

      cupsdUpdateDNSSDName();
    }
  }
#endif /* HAVE_DNSSD */

 /*
  * Enable LPD and SMB printer sharing as needed through external programs...
  */

  if (BrowseLocalProtocols & BROWSE_LPD)
    update_lpd(1);

  if (BrowseLocalProtocols & BROWSE_SMB)
    update_smb(1);

 /*
  * Register the individual printers
  */

  for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
       p;
       p = (cupsd_printer_t *)cupsArrayNext(Printers))
    if (!(p->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_SCANNER)))
      cupsdRegisterPrinter(p);
}


/*
 * 'cupsdStopBrowsing()' - Stop sending and receiving broadcast information.
 */

void
cupsdStopBrowsing(void)
{
  cupsd_printer_t	*p;		/* Current printer */


  if (!Browsing || !BrowseLocalProtocols)
    return;

 /*
  * De-register the individual printers
  */

  for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
       p;
       p = (cupsd_printer_t *)cupsArrayNext(Printers))
    if (!(p->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_SCANNER)))
      cupsdDeregisterPrinter(p, 1);

 /*
  * Shut down browsing sockets...
  */

#ifdef HAVE_DNSSD
  if ((BrowseLocalProtocols & BROWSE_DNSSD) && DNSSDRef)
    dnssdStop();
#endif /* HAVE_DNSSD */

 /*
  * Disable LPD and SMB printer sharing as needed through external programs...
  */

  if (BrowseLocalProtocols & BROWSE_LPD)
    update_lpd(0);

  if (BrowseLocalProtocols & BROWSE_SMB)
    update_smb(0);
}


#ifdef HAVE_DNSSD
/*
 * 'cupsdUpdateDNSSDName()' - Update the computer name we use for browsing...
 */

void
cupsdUpdateDNSSDName(void)
{
  DNSServiceErrorType error;		/* Error from service creation */
  char		webif[1024];		/* Web interface share name */
#  ifdef HAVE_SYSTEMCONFIGURATION
  SCDynamicStoreRef sc;			/* Context for dynamic store */
  CFDictionaryRef btmm;			/* Back-to-My-Mac domains */
  CFStringEncoding nameEncoding;	/* Encoding of computer name */
  CFStringRef	nameRef;		/* Host name CFString */
  char		nameBuffer[1024];	/* C-string buffer */
#  endif /* HAVE_SYSTEMCONFIGURATION */


 /*
  * Only share the web interface and printers when non-local listening is
  * enabled...
  */

  if (!DNSSDPort)
    return;

 /*
  * Get the computer name as a c-string...
  */

#  ifdef HAVE_SYSTEMCONFIGURATION
  sc = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("cupsd"), NULL, NULL);

  if (sc)
  {
   /*
    * Get the computer name from the dynamic store...
    */

    cupsdClearString(&DNSSDComputerName);

    if ((nameRef = SCDynamicStoreCopyComputerName(sc, &nameEncoding)) != NULL)
    {
      if (CFStringGetCString(nameRef, nameBuffer, sizeof(nameBuffer),
			     kCFStringEncodingUTF8))
      {
        cupsdLogMessage(CUPSD_LOG_DEBUG,
	                "Dynamic store computer name is \"%s\".", nameBuffer);
	cupsdSetString(&DNSSDComputerName, nameBuffer);
      }

      CFRelease(nameRef);
    }

    if (!DNSSDComputerName)
    {
     /*
      * Use the ServerName instead...
      */

      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "Using ServerName \"%s\" as computer name.", ServerName);
      cupsdSetString(&DNSSDComputerName, ServerName);
    }

   /*
    * Get the local hostname from the dynamic store...
    */

    cupsdClearString(&DNSSDHostName);

    if ((nameRef = SCDynamicStoreCopyLocalHostName(sc)) != NULL)
    {
      if (CFStringGetCString(nameRef, nameBuffer, sizeof(nameBuffer),
			     kCFStringEncodingUTF8))
      {
        cupsdLogMessage(CUPSD_LOG_DEBUG,
	                "Dynamic store host name is \"%s\".", nameBuffer);
	cupsdSetString(&DNSSDHostName, nameBuffer);
      }

      CFRelease(nameRef);
    }

    if (!DNSSDHostName)
    {
     /*
      * Use the ServerName instead...
      */

      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "Using ServerName \"%s\" as host name.", ServerName);
      cupsdSetString(&DNSSDHostName, ServerName);
    }

   /*
    * Get any Back-to-My-Mac domains and add them as aliases...
    */

    cupsdFreeAliases(DNSSDAlias);
    DNSSDAlias = NULL;

    btmm = SCDynamicStoreCopyValue(sc, CFSTR("Setup:/Network/BackToMyMac"));
    if (btmm && CFGetTypeID(btmm) == CFDictionaryGetTypeID())
    {
      cupsdLogMessage(CUPSD_LOG_DEBUG, "%d Back to My Mac aliases to add.",
		      (int)CFDictionaryGetCount(btmm));
      CFDictionaryApplyFunction(btmm, dnssdAddAlias, NULL);
    }
    else if (btmm)
      cupsdLogMessage(CUPSD_LOG_ERROR,
		      "Bad Back to My Mac data in dynamic store!");
    else
      cupsdLogMessage(CUPSD_LOG_DEBUG, "No Back to My Mac aliases to add.");

    if (btmm)
      CFRelease(btmm);

    CFRelease(sc);
  }
  else
#  endif /* HAVE_SYSTEMCONFIGURATION */
  {
    cupsdSetString(&DNSSDComputerName, ServerName);
    cupsdSetString(&DNSSDHostName, ServerName);
  }

 /*
  * Then (re)register the web interface if enabled...
  */

  if (BrowseWebIF)
  {
    if (DNSSDComputerName)
      snprintf(webif, sizeof(webif), "CUPS @ %s", DNSSDComputerName);
    else
      strlcpy(webif, "CUPS Web Interface", sizeof(webif));

    if (WebIFRef)
      DNSServiceRefDeallocate(WebIFRef);

    WebIFRef = DNSSDRef;
    if ((error = DNSServiceRegister(&WebIFRef,
				    kDNSServiceFlagsShareConnection,
				    0, webif, "_http._tcp", NULL,
				    NULL, htons(DNSSDPort), 7,
				    "\006path=/", dnssdRegisterCallback,
				    NULL)) != kDNSServiceErr_NoError)
      cupsdLogMessage(CUPSD_LOG_ERROR,
		      "DNS-SD web interface registration failed: %d", error);
  }
}
#endif /* HAVE_DNSSD */


#ifdef HAVE_DNSSD
#  ifdef HAVE_COREFOUNDATION
/*
 * 'dnssdAddAlias()' - Add a DNS-SD alias name.
 */

static void
dnssdAddAlias(const void *key,		/* I - Key */
              const void *value,	/* I - Value (domain) */
	      void       *context)	/* I - Unused */
{
  char	valueStr[1024],			/* Domain string */
	hostname[1024];			/* Complete hostname */


  (void)key;
  (void)context;

  if (CFGetTypeID((CFStringRef)value) == CFStringGetTypeID() &&
      CFStringGetCString((CFStringRef)value, valueStr, sizeof(valueStr),
                         kCFStringEncodingUTF8))
  {
    snprintf(hostname, sizeof(hostname), "%s.%s", DNSSDHostName, valueStr);
    if (!DNSSDAlias)
      DNSSDAlias = cupsArrayNew(NULL, NULL);

    cupsdAddAlias(DNSSDAlias, hostname);
    cupsdLogMessage(CUPSD_LOG_DEBUG, "Added Back to My Mac ServerAlias %s",
		    hostname);
  }
  else
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Bad Back to My Mac domain in dynamic store!");
}
#  endif /* HAVE_COREFOUNDATION */


/*
 * 'dnssdBuildTxtRecord()' - Build a TXT record from printer info.
 */

static char *				/* O - TXT record */
dnssdBuildTxtRecord(
    int             *txt_len,		/* O - TXT record length */
    cupsd_printer_t *p,			/* I - Printer information */
    int             for_lpd)		/* I - 1 = LPD, 0 = IPP */
{
  int		i;			/* Looping var */
  char		admin_hostname[256],	/* .local hostname for admin page */
		adminurl_str[256],	/* URL for the admin page */
		type_str[32],		/* Type to string buffer */
		state_str[32],		/* State to string buffer */
		rp_str[1024],		/* Queue name string buffer */
		air_str[1024],		/* auth-info-required string buffer */
		*keyvalue[32][2];	/* Table of key/value pairs */


 /*
  * Load up the key value pairs...
  */

  i = 0;

  keyvalue[i  ][0] = "txtvers";
  keyvalue[i++][1] = "1";

  keyvalue[i  ][0] = "qtotal";
  keyvalue[i++][1] = "1";

  keyvalue[i  ][0] = "rp";
  keyvalue[i++][1] = rp_str;
  if (for_lpd)
    strlcpy(rp_str, p->name, sizeof(rp_str));
  else
    snprintf(rp_str, sizeof(rp_str), "%s/%s",
	     (p->type & CUPS_PRINTER_CLASS) ? "classes" : "printers", p->name);

  keyvalue[i  ][0] = "ty";
  keyvalue[i++][1] = p->make_model ? p->make_model : "Unknown";

  snprintf(admin_hostname, sizeof(admin_hostname), "%s.local.", DNSSDHostName);
  httpAssembleURIf(HTTP_URI_CODING_ALL, adminurl_str, sizeof(adminurl_str),
                   "http", NULL, admin_hostname, DNSSDPort, "/%s/%s",
		   (p->type & CUPS_PRINTER_CLASS) ? "classes" : "printers",
		   p->name);
  keyvalue[i  ][0] = "adminurl";
  keyvalue[i++][1] = adminurl_str;

  keyvalue[i  ][0] = "note";
  keyvalue[i++][1] = p->location ? p->location : "";

  keyvalue[i  ][0] = "priority";
  keyvalue[i++][1] = for_lpd ? "100" : "0";

  keyvalue[i  ][0] = "product";
  keyvalue[i++][1] = p->pc && p->pc->product ? p->pc->product : "Unknown";

  keyvalue[i  ][0] = "pdl";
  keyvalue[i++][1] = p->pdl ? p->pdl : "application/postscript";

  if (get_auth_info_required(p, air_str, sizeof(air_str)))
  {
    keyvalue[i  ][0] = "air";
    keyvalue[i++][1] = air_str;
  }

  keyvalue[i  ][0] = "UUID";
  keyvalue[i++][1] = p->uuid + 9;

#ifdef HAVE_SSL
  keyvalue[i  ][0] = "TLS";
  keyvalue[i++][1] = "1.2";
#endif /* HAVE_SSL */

  keyvalue[i  ][0] = "Transparent";
  keyvalue[i++][1] = "F";

  keyvalue[i  ][0] = "Binary";
  keyvalue[i++][1] = "F";

  keyvalue[i  ][0] = "Fax";
  keyvalue[i++][1] = (p->type & CUPS_PRINTER_FAX) ? "T" : "F";

  keyvalue[i  ][0] = "Color";
  keyvalue[i++][1] = (p->type & CUPS_PRINTER_COLOR) ? "T" : "F";

  keyvalue[i  ][0] = "Duplex";
  keyvalue[i++][1] = (p->type & CUPS_PRINTER_DUPLEX) ? "T" : "F";

  keyvalue[i  ][0] = "Staple";
  keyvalue[i++][1] = (p->type & CUPS_PRINTER_STAPLE) ? "T" : "F";

  keyvalue[i  ][0] = "Copies";
  keyvalue[i++][1] = (p->type & CUPS_PRINTER_COPIES) ? "T" : "F";

  keyvalue[i  ][0] = "Collate";
  keyvalue[i++][1] = (p->type & CUPS_PRINTER_COLLATE) ? "T" : "F";

  keyvalue[i  ][0] = "Punch";
  keyvalue[i++][1] = (p->type & CUPS_PRINTER_PUNCH) ? "T" : "F";

  keyvalue[i  ][0] = "Bind";
  keyvalue[i++][1] = (p->type & CUPS_PRINTER_BIND) ? "T" : "F";

  keyvalue[i  ][0] = "Sort";
  keyvalue[i++][1] = (p->type & CUPS_PRINTER_SORT) ? "T" : "F";

  keyvalue[i  ][0] = "Scan";
  keyvalue[i++][1] = (p->type & CUPS_PRINTER_MFP) ? "T" : "F";

  snprintf(type_str, sizeof(type_str), "0x%X", p->type | CUPS_PRINTER_REMOTE);
  snprintf(state_str, sizeof(state_str), "%d", p->state);

  keyvalue[i  ][0] = "printer-state";
  keyvalue[i++][1] = state_str;

  keyvalue[i  ][0] = "printer-type";
  keyvalue[i++][1] = type_str;

 /*
  * Then pack them into a proper txt record...
  */

  return (dnssdPackTxtRecord(txt_len, keyvalue, i));
}


/*
 * 'dnssdDeregisterPrinter()' - Stop sending broadcast information for a
 *                              printer.
 */

static void
dnssdDeregisterPrinter(
    cupsd_printer_t *p)			/* I - Printer */
{
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "dnssdDeregisterPrinter(%s)", p->name);

 /*
  * Closing the socket deregisters the service
  */

  if (p->ipp_ref)
  {
    DNSServiceRefDeallocate(p->ipp_ref);
    p->ipp_ref = NULL;
  }

  if (p->ipp_txt)
  {
   /*
    * p->ipp_txt is malloc'd, not _cupsStrAlloc'd...
    */

    free(p->ipp_txt);
    p->ipp_txt = NULL;
  }

  if (p->printer_ref)
  {
    DNSServiceRefDeallocate(p->printer_ref);
    p->printer_ref = NULL;
  }

  if (p->printer_txt)
  {
   /*
    * p->printer_txt is malloc'd, not _cupsStrAlloc'd...
    */

    free(p->printer_txt);
    p->printer_txt = NULL;
  }

 /*
  * Remove the printer from the array of DNS-SD printers, then clear the
  * registered name...
  */

  cupsArrayRemove(DNSSDPrinters, p);
  cupsdClearString(&p->reg_name);
}


/*
 * 'dnssdPackTxtRecord()' - Pack an array of key/value pairs into the
 *                          TXT record format.
 */

static char *				/* O - TXT record */
dnssdPackTxtRecord(int  *txt_len,	/* O - TXT record length */
		   char *keyvalue[][2],	/* I - Table of key value pairs */
		   int  count)		/* I - Items in table */
{
  int  i;				/* Looping var */
  int  length;				/* Length of TXT record */
  int  length2;				/* Length of value */
  char *txtRecord;			/* TXT record buffer */
  char *cursor;				/* Looping pointer */


 /*
  * Calculate the buffer size
  */

  if (count <= 0)
    return (NULL);

  for (length = i = 0; i < count; i++)
    length += 1 + strlen(keyvalue[i][0]) +
	      (keyvalue[i][1] ? 1 + strlen(keyvalue[i][1]) : 0);

 /*
  * Allocate and fill it
  */

  txtRecord = malloc(length);
  if (txtRecord)
  {
    *txt_len = length;

    for (cursor = txtRecord, i = 0; i < count; i++)
    {
     /*
      * Drop in the p-string style length byte followed by the data
      */

      length  = strlen(keyvalue[i][0]);
      length2 = keyvalue[i][1] ? 1 + strlen(keyvalue[i][1]) : 0;

      *cursor++ = (unsigned char)(length + length2);

      memcpy(cursor, keyvalue[i][0], length);
      cursor += length;

      if (length2)
      {
        length2 --;
	*cursor++ = '=';
	memcpy(cursor, keyvalue[i][1], length2);
	cursor += length2;
      }
    }
  }

  return (txtRecord);
}


/*
 * 'dnssdRegisterCallback()' - DNSServiceRegister callback.
 */

static void
dnssdRegisterCallback(
    DNSServiceRef	sdRef,		/* I - DNS Service reference */
    DNSServiceFlags	flags,		/* I - Reserved for future use */
    DNSServiceErrorType	errorCode,	/* I - Error code */
    const char		*name,     	/* I - Service name */
    const char		*regtype,  	/* I - Service type */
    const char		*domain,   	/* I - Domain. ".local" for now */
    void		*context)	/* I - User-defined context */
{
  cupsd_printer_t *p = (cupsd_printer_t *)context;
					/* Current printer */


  (void)sdRef;
  (void)flags;
  (void)domain;

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "dnssdRegisterCallback(%s, %s) for %s (%s)",
                  name, regtype, p ? p->name : "Web Interface",
		  p ? (p->reg_name ? p->reg_name : "(null)") : "NA");

  if (errorCode)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
		    "DNSServiceRegister failed with error %d", (int)errorCode);
    return;
  }
  else if (p && (!p->reg_name || _cups_strcasecmp(name, p->reg_name)))
  {
    cupsdLogMessage(CUPSD_LOG_INFO, "Using service name \"%s\" for \"%s\"",
                    name, p->name);

    cupsArrayRemove(DNSSDPrinters, p);
    cupsdSetString(&p->reg_name, name);
    cupsArrayAdd(DNSSDPrinters, p);

    LastEvent |= CUPSD_EVENT_PRINTER_MODIFIED;
  }
}


/*
 * 'dnssdRegisterPrinter()' - Start sending broadcast information for a printer
 *		              or update the broadcast contents.
 */

static void
dnssdRegisterPrinter(cupsd_printer_t *p)/* I - Printer */
{
  DNSServiceErrorType	se;		/* dnssd errors */
  char			*ipp_txt,	/* IPP TXT record buffer */
			*printer_txt,	/* LPD TXT record buffer */
			name[1024],	/* Service name */
			*nameptr;	/* Pointer into name */
  int			ipp_len,	/* IPP TXT record length */
			printer_len,	/* LPD TXT record length */
			printer_port;	/* LPD port number */
  const char		*regtype;	/* Registration type */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "dnssdRegisterPrinter(%s) %s", p->name,
                  !p->ipp_ref ? "new" : "update");

 /*
  * If per-printer sharing was just disabled make sure we're not
  * registered before returning.
  */

  if (!p->shared)
  {
    dnssdDeregisterPrinter(p);
    return;
  }

 /*
  * The registered name takes the form of "<printer-info> @ <computer name>"...
  */

  if (p->info && strlen(p->info) > 0)
  {
    if (DNSSDComputerName)
      snprintf(name, sizeof(name), "%s @ %s", p->info, DNSSDComputerName);
    else
      strlcpy(name, p->info, sizeof(name));
  }
  else if (DNSSDComputerName)
    snprintf(name, sizeof(name), "%s @ %s", p->name, DNSSDComputerName);
  else
    strlcpy(name, p->name, sizeof(name));

 /*
  * If an existing printer was renamed, unregister it and start over...
  */

  if (p->reg_name && strcmp(p->reg_name, name))
    dnssdDeregisterPrinter(p);

  if (!p->reg_name)
  {
    cupsdSetString(&p->reg_name, name);
    cupsArrayAdd(DNSSDPrinters, p);
  }

 /*
  * Register IPP and (optionally) LPD...
  */

  ipp_len = 0;				/* anti-compiler-warning-code */
  ipp_txt = dnssdBuildTxtRecord(&ipp_len, p, 0);

  if (p->ipp_ref &&
      (ipp_len != p->ipp_len || memcmp(ipp_txt, p->ipp_txt, ipp_len)))
  {
   /*
    * Update the existing registration...
    */

    /* A TTL of 0 means use record's original value (Radar 3176248) */
    if ((se = DNSServiceUpdateRecord(p->ipp_ref, NULL, 0, ipp_len, ipp_txt,
				     0)) == kDNSServiceErr_NoError)
    {
      if (p->ipp_txt)
	free(p->ipp_txt);

      p->ipp_txt = ipp_txt;
      p->ipp_len = ipp_len;
      ipp_txt    = NULL;
    }
    else
    {
     /*
      * Failed to update record, lets close this reference and move on...
      */

      cupsdLogMessage(CUPSD_LOG_ERROR,
		      "Unable to update IPP DNS-SD record for %s - %d", p->name,
		      se);

      DNSServiceRefDeallocate(p->ipp_ref);
      p->ipp_ref = NULL;
    }
  }

  if (!p->ipp_ref)
  {
   /*
    * Initial registration.  Use the _fax-ipp regtype for fax queues...
    */

    regtype = (p->type & CUPS_PRINTER_FAX) ? "_fax-ipp._tcp" : DNSSDRegType;

    cupsdLogMessage(CUPSD_LOG_DEBUG,
		    "Registering DNS-SD printer %s with name \"%s\" and "
		    "type \"%s\"", p->name, name, regtype);

   /*
    * Register the queue, dropping characters as needed until we succeed...
    */

    nameptr = name + strlen(name);

    do
    {
      p->ipp_ref = DNSSDRef;
      if ((se = DNSServiceRegister(&p->ipp_ref, kDNSServiceFlagsShareConnection,
                                   0, name, regtype, NULL, NULL,
				   htons(DNSSDPort), ipp_len, ipp_txt,
				   dnssdRegisterCallback,
				   p)) == kDNSServiceErr_BadParam)
      {
       /*
        * Name is too long, drop trailing characters, taking into account
	* UTF-8 encoding...
	*/

        nameptr --;

        while (nameptr > name && (*nameptr & 0xc0) == 0x80)
	  nameptr --;

        if (nameptr > name)
          *nameptr = '\0';
      }
    }
    while (se == kDNSServiceErr_BadParam && nameptr > name);

    if (se == kDNSServiceErr_NoError)
    {
      p->ipp_txt = ipp_txt;
      p->ipp_len = ipp_len;
      ipp_txt    = NULL;
    }
    else
      cupsdLogMessage(CUPSD_LOG_WARN,
                      "DNS-SD IPP registration of \"%s\" failed: %d",
		      p->name, se);
  }

  if (ipp_txt)
    free(ipp_txt);

  if (BrowseLocalProtocols & BROWSE_LPD)
  {
    printer_len  = 0;			/* anti-compiler-warning-code */
    printer_port = 515;
    printer_txt  = dnssdBuildTxtRecord(&printer_len, p, 1);
  }
  else
  {
    printer_len  = 0;
    printer_port = 0;
    printer_txt  = NULL;
  }

  if (p->printer_ref &&
      (printer_len != p->printer_len ||
       memcmp(printer_txt, p->printer_txt, printer_len)))
  {
   /*
    * Update the existing registration...
    */

    /* A TTL of 0 means use record's original value (Radar 3176248) */
    if ((se = DNSServiceUpdateRecord(p->printer_ref, NULL, 0, printer_len,
				     printer_txt,
				     0)) == kDNSServiceErr_NoError)
    {
      if (p->printer_txt)
	free(p->printer_txt);

      p->printer_txt = printer_txt;
      p->printer_len = printer_len;
      printer_txt    = NULL;
    }
    else
    {
     /*
      * Failed to update record, lets close this reference and move on...
      */

      cupsdLogMessage(CUPSD_LOG_ERROR,
		      "Unable to update LPD DNS-SD record for %s - %d",
		      p->name, se);

      DNSServiceRefDeallocate(p->printer_ref);
      p->printer_ref = NULL;
    }
  }

  if (!p->printer_ref)
  {
   /*
    * Initial registration...
    */

    cupsdLogMessage(CUPSD_LOG_DEBUG,
		    "Registering DNS-SD printer %s with name \"%s\" and "
		    "type \"_printer._tcp\"", p->name, name);

    p->printer_ref = DNSSDRef;
    if ((se = DNSServiceRegister(&p->printer_ref,
				 kDNSServiceFlagsShareConnection,
				 0, name, "_printer._tcp", NULL, NULL,
				 htons(printer_port), printer_len, printer_txt,
				 dnssdRegisterCallback,
				 p)) == kDNSServiceErr_NoError)
    {
      p->printer_txt = printer_txt;
      p->printer_len = printer_len;
      printer_txt    = NULL;
    }
    else
      cupsdLogMessage(CUPSD_LOG_WARN,
		      "DNS-SD LPD registration of \"%s\" failed: %d",
		      p->name, se);
  }

  if (printer_txt)
    free(printer_txt);
}


/*
 * 'dnssdStop()' - Stop all DNS-SD registrations.
 */

static void
dnssdStop(void)
{
  cupsd_printer_t	*p;		/* Current printer */


 /*
  * De-register the individual printers
  */

  for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
       p;
       p = (cupsd_printer_t *)cupsArrayNext(Printers))
    dnssdDeregisterPrinter(p);

 /*
  * Shutdown the rest of the service refs...
  */

  if (WebIFRef)
  {
    DNSServiceRefDeallocate(WebIFRef);
    WebIFRef = NULL;
  }

  cupsdRemoveSelect(DNSServiceRefSockFD(DNSSDRef));

  DNSServiceRefDeallocate(DNSSDRef);
  DNSSDRef = NULL;

  cupsArrayDelete(DNSSDPrinters);
  DNSSDPrinters = NULL;

  DNSSDPort = 0;
}


/*
 * 'dnssdUpdate()' - Handle DNS-SD queries.
 */

static void
dnssdUpdate(void)
{
  DNSServiceErrorType	sdErr;		/* Service discovery error */


  if ((sdErr = DNSServiceProcessResult(DNSSDRef)) != kDNSServiceErr_NoError)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "DNS Service Discovery registration error %d!",
	            sdErr);
    dnssdStop();
  }
}
#endif /* HAVE_DNSSD */


/*
 * 'get_auth_info_required()' - Get the auth-info-required value to advertise.
 */

static char *				/* O - String or NULL if none */
get_auth_info_required(
    cupsd_printer_t *p,			/* I - Printer */
    char            *buffer,		/* I - Value buffer */
    size_t          bufsize)		/* I - Size of value buffer */
{
  cupsd_location_t *auth;		/* Pointer to authentication element */
  char		resource[1024];		/* Printer/class resource path */


 /*
  * If auth-info-required is set for this printer, return that...
  */

  if (p->num_auth_info_required > 0 && strcmp(p->auth_info_required[0], "none"))
  {
    int		i;			/* Looping var */
    char	*bufptr;		/* Pointer into buffer */

    for (i = 0, bufptr = buffer; i < p->num_auth_info_required; i ++)
    {
      if (bufptr >= (buffer + bufsize - 2))
	break;

      if (i)
	*bufptr++ = ',';

      strlcpy(bufptr, p->auth_info_required[i], bufsize - (bufptr - buffer));
      bufptr += strlen(bufptr);
    }

    return (buffer);
  }

 /*
  * Figure out the authentication data requirements to advertise...
  */

  if (p->type & CUPS_PRINTER_CLASS)
    snprintf(resource, sizeof(resource), "/classes/%s", p->name);
  else
    snprintf(resource, sizeof(resource), "/printers/%s", p->name);

  if ((auth = cupsdFindBest(resource, HTTP_POST)) == NULL ||
      auth->type == CUPSD_AUTH_NONE)
    auth = cupsdFindPolicyOp(p->op_policy_ptr, IPP_PRINT_JOB);

  if (auth)
  {
    int	auth_type;			/* Authentication type */

    if ((auth_type = auth->type) == CUPSD_AUTH_DEFAULT)
      auth_type = DefaultAuthType;

    switch (auth_type)
    {
      case CUPSD_AUTH_NONE :
          return (NULL);

      case CUPSD_AUTH_NEGOTIATE :
	  strlcpy(buffer, "negotiate", bufsize);
	  break;

      default :
	  strlcpy(buffer, "username,password", bufsize);
	  break;
    }

    return (buffer);
  }

  return ("none");
}


#ifdef __APPLE__
/*
 * 'get_hostconfig()' - Get an /etc/hostconfig service setting.
 */

static int				/* O - 1 for YES or AUTOMATIC, 0 for NO */
get_hostconfig(const char *name)	/* I - Name of service */
{
  cups_file_t	*fp;			/* Hostconfig file */
  char		line[1024],		/* Line from file */
		*ptr;			/* Pointer to value */
  int		state = 1;		/* State of service */


 /*
  * Try opening the /etc/hostconfig file; if we can't open it, assume that
  * the service is enabled/auto.
  */

  if ((fp = cupsFileOpen("/etc/hostconfig", "r")) != NULL)
  {
   /*
    * Read lines from the file until we find the service...
    */

    while (cupsFileGets(fp, line, sizeof(line)))
    {
      if (line[0] == '#' || (ptr = strchr(line, '=')) == NULL)
        continue;

      *ptr++ = '\0';

      if (!_cups_strcasecmp(line, name))
      {
       /*
        * Found the service, see if it is set to "-NO-"...
	*/

	if (!_cups_strncasecmp(ptr, "-NO-", 4))
	  state = 0;
        break;
      }
    }

    cupsFileClose(fp);
  }

  return (state);
}
#endif /* __APPLE__ */


/*
 * 'update_lpd()' - Update the LPD configuration as needed.
 */

static void
update_lpd(int onoff)			/* - 1 = turn on, 0 = turn off */
{
  if (!LPDConfigFile)
    return;

#ifdef __APPLE__
 /*
  * Allow /etc/hostconfig CUPS_LPD service setting to override cupsd.conf
  * setting for backwards-compatibility.
  */

  if (onoff && !get_hostconfig("CUPS_LPD"))
    onoff = 0;
#endif /* __APPLE__ */

  if (!strncmp(LPDConfigFile, "xinetd:///", 10))
  {
   /*
    * Enable/disable LPD via the xinetd.d config file for cups-lpd...
    */

    char	newfile[1024];		/* New cups-lpd.N file */
    cups_file_t	*ofp,			/* Original file pointer */
		*nfp;			/* New file pointer */
    char	line[1024];		/* Line from file */


    snprintf(newfile, sizeof(newfile), "%s.N", LPDConfigFile + 9);

    if ((ofp = cupsFileOpen(LPDConfigFile + 9, "r")) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to open \"%s\" - %s",
                      LPDConfigFile + 9, strerror(errno));
      return;
    }

    if ((nfp = cupsFileOpen(newfile, "w")) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to create \"%s\" - %s",
                      newfile, strerror(errno));
      cupsFileClose(ofp);
      return;
    }

   /*
    * Copy all of the lines from the cups-lpd file...
    */

    while (cupsFileGets(ofp, line, sizeof(line)))
    {
      if (line[0] == '{')
      {
        cupsFilePrintf(nfp, "%s\n", line);
        snprintf(line, sizeof(line), "\tdisable = %s",
	         onoff ? "no" : "yes");
      }
      else if (!strstr(line, "disable ="))
        cupsFilePrintf(nfp, "%s\n", line);
    }

    cupsFileClose(nfp);
    cupsFileClose(ofp);
    rename(newfile, LPDConfigFile + 9);
  }
#ifdef __APPLE__
  else if (!strncmp(LPDConfigFile, "launchd:///", 11))
  {
   /*
    * Enable/disable LPD via the launchctl command...
    */

    char	*argv[5],		/* Arguments for command */
		*envp[MAX_ENV];		/* Environment for command */
    int		pid;			/* Process ID */


    cupsdLoadEnv(envp, (int)(sizeof(envp) / sizeof(envp[0])));
    argv[0] = (char *)"launchctl";
    argv[1] = (char *)(onoff ? "load" : "unload");
    argv[2] = (char *)"-w";
    argv[3] = LPDConfigFile + 10;
    argv[4] = NULL;

    cupsdStartProcess("/bin/launchctl", argv, envp, -1, -1, -1, -1, -1, 1,
                      NULL, NULL, &pid);
  }
#endif /* __APPLE__ */
  else
    cupsdLogMessage(CUPSD_LOG_INFO, "Unknown LPDConfigFile scheme!");
}


/*
 * 'update_smb()' - Update the SMB configuration as needed.
 */

static void
update_smb(int onoff)			/* I - 1 = turn on, 0 = turn off */
{
  if (!SMBConfigFile)
    return;

  if (!strncmp(SMBConfigFile, "samba:///", 9))
  {
   /*
    * Enable/disable SMB via the specified smb.conf config file...
    */

    char	newfile[1024];		/* New smb.conf.N file */
    cups_file_t	*ofp,			/* Original file pointer */
		*nfp;			/* New file pointer */
    char	line[1024];		/* Line from file */
    int		in_printers;		/* In [printers] section? */


    snprintf(newfile, sizeof(newfile), "%s.N", SMBConfigFile + 8);

    if ((ofp = cupsFileOpen(SMBConfigFile + 8, "r")) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to open \"%s\" - %s",
                      SMBConfigFile + 8, strerror(errno));
      return;
    }

    if ((nfp = cupsFileOpen(newfile, "w")) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to create \"%s\" - %s",
                      newfile, strerror(errno));
      cupsFileClose(ofp);
      return;
    }

   /*
    * Copy all of the lines from the smb.conf file...
    */

    in_printers = 0;

    while (cupsFileGets(ofp, line, sizeof(line)))
    {
      if (in_printers && strstr(line, "printable ="))
        snprintf(line, sizeof(line), "    printable = %s",
	         onoff ? "yes" : "no");

      cupsFilePrintf(nfp, "%s\n", line);

      if (line[0] == '[')
        in_printers = !strcmp(line, "[printers]");
    }

    cupsFileClose(nfp);
    cupsFileClose(ofp);
    rename(newfile, SMBConfigFile + 8);
  }
  else
    cupsdLogMessage(CUPSD_LOG_INFO, "Unknown SMBConfigFile scheme!");
}


/*
 * End of "$Id: dirsvc.c 7933 2008-09-11 00:44:58Z mike $".
 */
