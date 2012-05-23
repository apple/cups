/*
 * "$Id: dirsvc.c 7933 2008-09-11 00:44:58Z mike $"
 *
 *   Directory services routines for the CUPS scheduler.
 *
 *   Copyright 2007-2012 by Apple Inc.
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
 *   cupsdDeregisterPrinter()  - Stop sending broadcast information for a local
 *				 printer and remove any pending references to
 *				 remote printers.
 *   cupsdRegisterPrinter()    - Start sending broadcast information for a
 *				 printer or update the broadcast contents.
 *   cupsdStartBrowsing()      - Start sending and receiving broadcast
 *				 information.
 *   cupsdStopBrowsing()       - Stop sending and receiving broadcast
 *				 information.
 *   cupsdUpdateDNSSDName()    - Update the computer name we use for
 *				 browsing...
 *   dnssdAddAlias()	       - Add a DNS-SD alias name.
 *   dnssdBuildTxtRecord()     - Build a TXT record from printer info.
 *   dnssdDeregisterInstance() - Deregister a DNS-SD service instance.
 *   dnssdDeregisterPrinter()  - Deregister all services for a printer.
 *   dnssdErrorString()        - Return an error string for an error code.
 *   dnssdRegisterCallback()   - Free a TXT record.
 *   dnssdRegisterCallback()   - DNSServiceRegister callback.
 *   dnssdRegisterInstance()   - Register an instance of a printer service.
 *   dnssdRegisterPrinter()    - Start sending broadcast information for a
 *				 printer or update the broadcast contents.
 *   dnssdStop()	       - Stop all DNS-SD registrations.
 *   dnssdUpdate()	       - Handle DNS-SD queries.
 *   get_auth_info_required()  - Get the auth-info-required value to advertise.
 *   get_hostconfig()	       - Get an /etc/hostconfig service setting.
 *   update_lpd()	       - Update the LPD configuration as needed.
 *   update_smb()	       - Update the SMB configuration as needed.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <grp.h>

#if defined(HAVE_DNSSD) && defined(__APPLE__)
#  include <nameser.h>
#  include <CoreFoundation/CoreFoundation.h>
#  include <SystemConfiguration/SystemConfiguration.h>
#endif /* HAVE_DNSSD && __APPLE__ */


/*
 * Local functions...
 */

#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
static char		*get_auth_info_required(cupsd_printer_t *p,
			                        char *buffer, size_t bufsize);
#endif /* HAVE_DNSSD || HAVE_AVAHI */
#ifdef __APPLE__
static int		get_hostconfig(const char *name);
#endif /* __APPLE__ */
static void		update_lpd(int onoff);
static void		update_smb(int onoff);


#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
#  ifdef __APPLE__
static void		dnssdAddAlias(const void *key, const void *value,
			              void *context);
#  endif /* __APPLE__ */
static cupsd_txt_t	dnssdBuildTxtRecord(cupsd_printer_t *p, int for_lpd);
static void		dnssdDeregisterInstance(cupsd_srv_t *srv);
static void		dnssdDeregisterPrinter(cupsd_printer_t *p,
			                       int clear_name);
static const char	*dnssdErrorString(int error);
static void		dnssdFreeTxtRecord(cupsd_txt_t *txt);
#  ifdef HAVE_DNSSD
static void		dnssdRegisterCallback(DNSServiceRef sdRef,
					      DNSServiceFlags flags,
					      DNSServiceErrorType errorCode,
					      const char *name,
					      const char *regtype,
					      const char *domain,
					      void *context);
#  else
static void		dnssdRegisterCallback(AvahiEntryGroup *p,
					      AvahiEntryGroupState state,
					      void *context);
#  endif /* HAVE_DNSSD */
static int		dnssdRegisterInstance(cupsd_srv_t *srv,
					      cupsd_printer_t *p,
					      char *name, const char *type,
					      const char *subtypes, int port,
					      cupsd_txt_t *txt, int commit);
static void		dnssdRegisterPrinter(cupsd_printer_t *p);
static void		dnssdStop(void);
#  ifdef HAVE_DNSSD
static void		dnssdUpdate(void);
#  endif /* HAVE_DNSSD */
#endif /* HAVE_DNSSD || HAVE_AVAHI */


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

#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
  if (removeit && (BrowseLocalProtocols & BROWSE_DNSSD) && DNSSDMaster)
    dnssdDeregisterPrinter(p, 1);
#endif /* HAVE_DNSSD || HAVE_AVAHI */
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

#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
  if ((BrowseLocalProtocols & BROWSE_DNSSD) && DNSSDMaster)
    dnssdRegisterPrinter(p);
#endif /* HAVE_DNSSD || HAVE_AVAHI */
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

#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
  if (BrowseLocalProtocols & BROWSE_DNSSD)
  {
    cupsd_listener_t	*lis;		/* Current listening socket */
#  ifdef HAVE_DNSSD
    DNSServiceErrorType error;		/* Error from service creation */

   /*
    * First create a "master" connection for all registrations...
    */

    if ((error = DNSServiceCreateConnection(&DNSSDMaster))
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

      int fd = DNSServiceRefSockFD(DNSSDMaster);

      fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);

      cupsdAddSelect(fd, (cupsd_selfunc_t)dnssdUpdate, NULL, NULL);
    }

#  else /* HAVE_AVAHI */
    if ((DNSSDMaster = avahi_threaded_poll_new()) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to create DNS-SD thread.");

      if (FatalErrors & CUPSD_FATAL_BROWSE)
	cupsdEndProcess(getpid(), 0);
    }
    else
    {
      int error;			/* Error code, if any */

      DNSSDClient = avahi_client_new(avahi_threaded_poll_get(DNSSDMaster), 0,
                                     NULL, NULL, &error);

      if (DNSSDClient == NULL)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
                        "Unable to communicate with avahi-daemon: %s",
                        dnssdErrorString(error));

        if (FatalErrors & CUPSD_FATAL_BROWSE)
	  cupsdEndProcess(getpid(), 0);
      }

      avahi_threaded_poll_start(DNSSDMaster);
    }
#  endif /* HAVE_DNSSD */

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
#endif /* HAVE_DNSSD || HAVE_AVAHI */

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

#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
  if ((BrowseLocalProtocols & BROWSE_DNSSD) && DNSSDMaster)
    dnssdStop();
#endif /* HAVE_DNSSD || HAVE_AVAHI */

 /*
  * Disable LPD and SMB printer sharing as needed through external programs...
  */

  if (BrowseLocalProtocols & BROWSE_LPD)
    update_lpd(0);

  if (BrowseLocalProtocols & BROWSE_SMB)
    update_smb(0);
}


#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
/*
 * 'cupsdUpdateDNSSDName()' - Update the computer name we use for browsing...
 */

void
cupsdUpdateDNSSDName(void)
{
  char		webif[1024];		/* Web interface share name */
#  ifdef __APPLE__
  SCDynamicStoreRef sc;			/* Context for dynamic store */
  CFDictionaryRef btmm;			/* Back-to-My-Mac domains */
  CFStringEncoding nameEncoding;	/* Encoding of computer name */
  CFStringRef	nameRef;		/* Host name CFString */
  char		nameBuffer[1024];	/* C-string buffer */
#  endif /* __APPLE__ */


 /*
  * Only share the web interface and printers when non-local listening is
  * enabled...
  */

  if (!DNSSDPort)
    return;

 /*
  * Get the computer name as a c-string...
  */

#  ifdef __APPLE__
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
#  endif /* __APPLE__ */
#  ifdef HAVE_AVAHI
  {
    cupsdSetString(&DNSSDComputerName, avahi_client_get_host_name(DNSSDClient));
    cupsdSetString(&DNSSDHostName,
                   avahi_client_get_host_name_fqdn(DNSSDClient));
  }
#  else /* HAVE_DNSSD */
  {
    cupsdSetString(&DNSSDComputerName, ServerName);
    cupsdSetString(&DNSSDHostName, ServerName);
  }
#  endif /* HAVE_AVAHI */

 /*
  * Then (re)register the web interface if enabled...
  */

  if (BrowseWebIF)
  {
    if (DNSSDComputerName)
      snprintf(webif, sizeof(webif), "CUPS @ %s", DNSSDComputerName);
    else
      strlcpy(webif, "CUPS", sizeof(webif));

    dnssdDeregisterInstance(&WebIFSrv);
    dnssdRegisterInstance(&WebIFSrv, NULL, webif, "_http._tcp", "_printer",
                          DNSSDPort, NULL, 1);
  }
}


#  ifdef __APPLE__
/*
 * 'dnssdAddAlias()' - Add a DNS-SD alias name.
 */

static void
dnssdAddAlias(const void *key,		/* I - Key */
              const void *value,	/* I - Value (domain) */
	      void       *context)	/* I - Unused */
{
  char	valueStr[1024],			/* Domain string */
	hostname[1024],			/* Complete hostname */
	*hostptr;			/* Pointer into hostname */


  (void)key;
  (void)context;

  if (CFGetTypeID((CFStringRef)value) == CFStringGetTypeID() &&
      CFStringGetCString((CFStringRef)value, valueStr, sizeof(valueStr),
                         kCFStringEncodingUTF8))
  {
    snprintf(hostname, sizeof(hostname), "%s.%s", DNSSDHostName, valueStr);
    hostptr = hostname + strlen(hostname) - 1;
    if (*hostptr == '.')
      *hostptr = '\0';			/* Strip trailing dot */

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
#  endif /* __APPLE__ */


/*
 * 'dnssdBuildTxtRecord()' - Build a TXT record from printer info.
 */

static cupsd_txt_t			/* O - TXT record */
dnssdBuildTxtRecord(
    cupsd_printer_t *p,			/* I - Printer information */
    int             for_lpd)		/* I - 1 = LPD, 0 = IPP */
{
  int		i,			/* Looping var */
		count;			/* Count of key/value pairs */
  char		admin_hostname[256],	/* .local hostname for admin page */
		adminurl_str[256],	/* URL for the admin page */
		type_str[32],		/* Type to string buffer */
		state_str[32],		/* State to string buffer */
		rp_str[1024],		/* Queue name string buffer */
		air_str[1024],		/* auth-info-required string buffer */
		*keyvalue[32][2];	/* Table of key/value pairs */
  cupsd_txt_t	txt;			/* TXT record */


 /*
  * Load up the key value pairs...
  */

  count = 0;

  if (!for_lpd || (BrowseLocalProtocols & BROWSE_LPD))
  {
    keyvalue[count  ][0] = "txtvers";
    keyvalue[count++][1] = "1";

    keyvalue[count  ][0] = "qtotal";
    keyvalue[count++][1] = "1";

    keyvalue[count  ][0] = "rp";
    keyvalue[count++][1] = rp_str;
    if (for_lpd)
      strlcpy(rp_str, p->name, sizeof(rp_str));
    else
      snprintf(rp_str, sizeof(rp_str), "%s/%s",
	       (p->type & CUPS_PRINTER_CLASS) ? "classes" : "printers",
	       p->name);

    keyvalue[count  ][0] = "ty";
    keyvalue[count++][1] = p->make_model ? p->make_model : "Unknown";

    if (strstr(DNSSDHostName, ".local"))
      strlcpy(admin_hostname, DNSSDHostName, sizeof(admin_hostname));
    else
      snprintf(admin_hostname, sizeof(admin_hostname), "%s.local.",
               DNSSDHostName);
    httpAssembleURIf(HTTP_URI_CODING_ALL, adminurl_str, sizeof(adminurl_str),
#  ifdef HAVE_SSL
		     "https",
#  else
		     "http",
#  endif /* HAVE_SSL */
		     NULL, admin_hostname, DNSSDPort, "/%s/%s",
		     (p->type & CUPS_PRINTER_CLASS) ? "classes" : "printers",
		     p->name);
    keyvalue[count  ][0] = "adminurl";
    keyvalue[count++][1] = adminurl_str;

    if (p->location)
    {
      keyvalue[count  ][0] = "note";
      keyvalue[count++][1] = p->location;
    }

    keyvalue[count  ][0] = "priority";
    keyvalue[count++][1] = for_lpd ? "100" : "0";

    keyvalue[count  ][0] = "product";
    keyvalue[count++][1] = p->pc && p->pc->product ? p->pc->product : "Unknown";

    keyvalue[count  ][0] = "pdl";
    keyvalue[count++][1] = p->pdl ? p->pdl : "application/postscript";

    if (get_auth_info_required(p, air_str, sizeof(air_str)))
    {
      keyvalue[count  ][0] = "air";
      keyvalue[count++][1] = air_str;
    }

    keyvalue[count  ][0] = "UUID";
    keyvalue[count++][1] = p->uuid + 9;

  #ifdef HAVE_SSL
    keyvalue[count  ][0] = "TLS";
    keyvalue[count++][1] = "1.2";
  #endif /* HAVE_SSL */

    if (p->type & CUPS_PRINTER_FAX)
    {
      keyvalue[count  ][0] = "Fax";
      keyvalue[count++][1] = (p->type & CUPS_PRINTER_FAX) ? "T" : "F";
    }

    if (p->type & CUPS_PRINTER_COLOR)
    {
      keyvalue[count  ][0] = "Color";
      keyvalue[count++][1] = (p->type & CUPS_PRINTER_COLOR) ? "T" : "F";
    }

    if (p->type & CUPS_PRINTER_DUPLEX)
    {
      keyvalue[count  ][0] = "Duplex";
      keyvalue[count++][1] = (p->type & CUPS_PRINTER_DUPLEX) ? "T" : "F";
    }

    if (p->type & CUPS_PRINTER_STAPLE)
    {
      keyvalue[count  ][0] = "Staple";
      keyvalue[count++][1] = (p->type & CUPS_PRINTER_STAPLE) ? "T" : "F";
    }

    if (p->type & CUPS_PRINTER_COPIES)
    {
      keyvalue[count  ][0] = "Copies";
      keyvalue[count++][1] = (p->type & CUPS_PRINTER_COPIES) ? "T" : "F";
    }

    if (p->type & CUPS_PRINTER_COLLATE)
    {
      keyvalue[count  ][0] = "Collate";
      keyvalue[count++][1] = (p->type & CUPS_PRINTER_COLLATE) ? "T" : "F";
    }

    if (p->type & CUPS_PRINTER_PUNCH)
    {
      keyvalue[count  ][0] = "Punch";
      keyvalue[count++][1] = (p->type & CUPS_PRINTER_PUNCH) ? "T" : "F";
    }

    if (p->type & CUPS_PRINTER_BIND)
    {
      keyvalue[count  ][0] = "Bind";
      keyvalue[count++][1] = (p->type & CUPS_PRINTER_BIND) ? "T" : "F";
    }

    if (p->type & CUPS_PRINTER_SORT)
    {
      keyvalue[count  ][0] = "Sort";
      keyvalue[count++][1] = (p->type & CUPS_PRINTER_SORT) ? "T" : "F";
    }

    if (p->type & CUPS_PRINTER_MFP)
    {
      keyvalue[count  ][0] = "Scan";
      keyvalue[count++][1] = (p->type & CUPS_PRINTER_MFP) ? "T" : "F";
    }

    snprintf(type_str, sizeof(type_str), "0x%X", p->type | CUPS_PRINTER_REMOTE);
    snprintf(state_str, sizeof(state_str), "%d", p->state);

    keyvalue[count  ][0] = "printer-state";
    keyvalue[count++][1] = state_str;

    keyvalue[count  ][0] = "printer-type";
    keyvalue[count++][1] = type_str;
  }

 /*
  * Then pack them into a proper txt record...
  */

#  ifdef HAVE_DNSSD
  TXTRecordCreate(&txt, 0, NULL);

  for (i = 0; i < count; i ++)
  {
    size_t len = strlen(keyvalue[i][1]);

    if (len < 256)
      TXTRecordSetValue(&txt, keyvalue[i][0], (uint8_t)len, keyvalue[i][1]);
  }

#  else
  for (i = 0, txt = NULL; i < count; i ++)
    txt = avahi_string_list_add_printf(txt, "%s=%s", keyvalue[i][0],
                                       keyvalue[i][1]);
#  endif /* HAVE_DNSSD */

  return (txt);
}


/*
 * 'dnssdDeregisterInstance()' - Deregister a DNS-SD service instance.
 */

static void
dnssdDeregisterInstance(
    cupsd_srv_t     *srv)		/* I - Service */
{
  if (!srv || !*srv)
    return;

#  ifdef HAVE_DNSSD
  DNSServiceRefDeallocate(*srv);

#  else /* HAVE_AVAHI */
  avahi_threaded_poll_lock(DNSSDMaster);
  avahi_entry_group_free(*srv);
  avahi_threaded_poll_unlock(DNSSDMaster);
#  endif /* HAVE_DNSSD */

  *srv = NULL;
}


/*
 * 'dnssdDeregisterPrinter()' - Deregister all services for a printer.
 */

static void
dnssdDeregisterPrinter(
    cupsd_printer_t *p,			/* I - Printer */
    int             clear_name)		/* I - Clear the name? */

{
  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "dnssdDeregisterPrinter(p=%p(%s), clear_name=%d)", p, p->name,
                  clear_name);

  if (p->ipp_srv)
  {
    dnssdDeregisterInstance(&p->ipp_srv);

#  ifdef HAVE_DNSSD
#    ifdef HAVE_SSL
    dnssdDeregisterInstance(&p->ipps_srv);
#    endif /* HAVE_SSL */
    dnssdDeregisterInstance(&p->printer_srv);
#  endif /* HAVE_DNSSD */
  }

 /*
  * Remove the printer from the array of DNS-SD printers but keep the
  * registered name...
  */

  cupsArrayRemove(DNSSDPrinters, p);

 /*
  * Optionally clear the service name...
  */

  if (clear_name)
    cupsdClearString(&p->reg_name);
}


/*
 * 'dnssdErrorString()' - Return an error string for an error code.
 */

static const char *			/* O - Error message */
dnssdErrorString(int error)		/* I - Error number */
{
#  ifdef HAVE_DNSSD
  switch (error)
  {
    case kDNSServiceErr_NoError :
        return ("OK.");

    default :
    case kDNSServiceErr_Unknown :
        return ("Unknown error.");

    case kDNSServiceErr_NoSuchName :
        return ("Service not found.");

    case kDNSServiceErr_NoMemory :
        return ("Out of memory.");

    case kDNSServiceErr_BadParam :
        return ("Bad parameter.");

    case kDNSServiceErr_BadReference :
        return ("Bad service reference.");

    case kDNSServiceErr_BadState :
        return ("Bad state.");

    case kDNSServiceErr_BadFlags :
        return ("Bad flags.");

    case kDNSServiceErr_Unsupported :
        return ("Unsupported.");

    case kDNSServiceErr_NotInitialized :
        return ("Not initialized.");

    case kDNSServiceErr_AlreadyRegistered :
        return ("Already registered.");

    case kDNSServiceErr_NameConflict :
        return ("Name conflict.");

    case kDNSServiceErr_Invalid :
        return ("Invalid name.");

    case kDNSServiceErr_Firewall :
        return ("Firewall prevents registration.");

    case kDNSServiceErr_Incompatible :
        return ("Client library incompatible.");

    case kDNSServiceErr_BadInterfaceIndex :
        return ("Bad interface index.");

    case kDNSServiceErr_Refused :
        return ("Server prevents registration.");

    case kDNSServiceErr_NoSuchRecord :
        return ("Record not found.");

    case kDNSServiceErr_NoAuth :
        return ("Authentication required.");

    case kDNSServiceErr_NoSuchKey :
        return ("Encryption key not found.");

    case kDNSServiceErr_NATTraversal :
        return ("Unable to traverse NAT boundary.");

    case kDNSServiceErr_DoubleNAT :
        return ("Unable to traverse double-NAT boundary.");

    case kDNSServiceErr_BadTime :
        return ("Bad system time.");

    case kDNSServiceErr_BadSig :
        return ("Bad signature.");

    case kDNSServiceErr_BadKey :
        return ("Bad encryption key.");

    case kDNSServiceErr_Transient :
        return ("Transient error occurred - please try again.");

    case kDNSServiceErr_ServiceNotRunning :
        return ("Server not running.");

    case kDNSServiceErr_NATPortMappingUnsupported :
        return ("NAT doesn't support NAT-PMP or UPnP.");

    case kDNSServiceErr_NATPortMappingDisabled :
        return ("NAT supports NAT-PNP or UPnP but it is disabled.");

    case kDNSServiceErr_NoRouter :
        return ("No Internet/default router configured.");

    case kDNSServiceErr_PollingMode :
        return ("Service polling mode error.");

    case kDNSServiceErr_Timeout :
        return ("Service timeout.");
  }

#  else /* HAVE_AVAHI */
  return (avahi_strerror(error));
#  endif /* HAVE_DNSSD */
}


/*
 * 'dnssdRegisterCallback()' - Free a TXT record.
 */

static void
dnssdFreeTxtRecord(cupsd_txt_t *txt)	/* I - TXT record */
{
#  ifdef HAVE_DNSSD
  TXTRecordDeallocate(txt);

#  else /* HAVE_AVAHI */
  avahi_string_list_free(*txt);
  *txt = NULL;
#  endif /* HAVE_DNSSD */
}


/*
 * 'dnssdRegisterCallback()' - DNSServiceRegister callback.
 */

#  ifdef HAVE_DNSSD
static void
dnssdRegisterCallback(
    DNSServiceRef	sdRef,		/* I - DNS Service reference */
    DNSServiceFlags	flags,		/* I - Reserved for future use */
    DNSServiceErrorType	errorCode,	/* I - Error code */
    const char		*name,     	/* I - Service name */
    const char		*regtype,  	/* I - Service type */
    const char		*domain,   	/* I - Domain. ".local" for now */
    void		*context)	/* I - Printer */
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

#  else /* HAVE_AVAHI */
static void
dnssdRegisterCallback(
    AvahiEntryGroup      *srv,		/* I - Service */
    AvahiEntryGroupState state,		/* I - Registration state */
    void                 *context)	/* I - Printer */
{
  cupsd_printer_t *p = (cupsd_printer_t *)context;
					/* Current printer */

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "dnssdRegisterCallback(srv=%p, state=%d, context=%p) "
                  "for %s (%s)", srv, state, context,
                  p ? p->name : "Web Interface",
		  p ? (p->reg_name ? p->reg_name : "(null)") : "NA");

  /* TODO: Handle collisions with avahi_alternate_service_name(p->reg_name)? */
}
#  endif /* HAVE_DNSSD */


/*
 * 'dnssdRegisterInstance()' - Register an instance of a printer service.
 */

static int				/* O - 1 on success, 0 on failure */
dnssdRegisterInstance(
    cupsd_srv_t     *srv,		/* O - Service */
    cupsd_printer_t *p,			/* I - Printer */
    char            *name,		/* I - DNS-SD service name */
    const char      *type,		/* I - DNS-SD service type */
    const char      *subtypes,		/* I - Subtypes to register or NULL */
    int             port,		/* I - Port number or 0 */
    cupsd_txt_t     *txt,		/* I - TXT record */
    int             commit)		/* I - Commit registration? */
{
  char	temp[256],			/* Temporary string */
	*ptr;				/* Pointer into string */
  int	error;				/* Any error */


  cupsdLogMessage(CUPSD_LOG_DEBUG,
		  "Registering \"%s\" with DNS-SD type \"%s\".", name, type);

  if (p && !srv)
  {
   /*
    * Assign the correct pointer for "srv"...
    */

#  ifdef HAVE_DNSSD
    if (!strcmp(type, "_printer._tcp"))
      srv = &p->printer_srv;		/* Target LPD service */
#    ifdef HAVE_SSL
    else if (!strcmp(type, "_ipps._tcp"))
      srv = &p->ipps_srv;		/* Target IPPS service */
#    endif /* HAVE_SSL */
    else
      srv = &p->ipp_srv;		/* Target IPP service */

#  else /* HAVE_AVAHI */
    srv = &p->ipp_srv;			/* Target service group */
#  endif /* HAVE_DNSSD */
  }

#  ifdef HAVE_DNSSD
  (void)commit;

#  else /* HAVE_AVAHI */
  avahi_threaded_poll_lock(DNSSDMaster);

  if (!*srv)
    *srv = avahi_entry_group_new(DNSSDClient, dnssdRegisterCallback, NULL);
  if (!*srv)
  {
    avahi_threaded_poll_unlock(DNSSDMaster);

    cupsdLogMessage(CUPSD_LOG_WARN, "DNS-SD registration of \"%s\" failed: %s",
                    name, dnssdErrorString(avahi_client_errno(DNSSDClient)));
    return (0);
  }
#  endif /* HAVE_DNSSD */

 /*
  * Make sure the name is <= 63 octets, and when we truncate be sure to
  * properly truncate any UTF-8 characters...
  */

  ptr = name + strlen(name);
  while ((ptr - name) > 63)
  {
    do
    {
      ptr --;
    }
    while (ptr > name && (*ptr & 0xc0) == 0x80);

    if (ptr > name)
      *ptr = '\0';
  }

 /*
  * Register the service...
  */

#  ifdef HAVE_DNSSD
  if (subtypes)
    snprintf(temp, sizeof(temp), "%s,%s", type, subtypes);
  else
    strlcpy(temp, type, sizeof(temp));

  *srv  = DNSSDMaster;
  error = DNSServiceRegister(srv, kDNSServiceFlagsShareConnection,
			     0, name, temp, NULL, NULL, htons(port),
			     txt ? TXTRecordGetLength(txt) : 0,
			     txt ? TXTRecordGetBytesPtr(txt) : NULL,
			     dnssdRegisterCallback, p);

#  else /* HAVE_AVAHI */
  if (txt)
  {
    AvahiStringList *temptxt;
    for (temptxt = *txt; temptxt; temptxt = temptxt->next)
      cupsdLogMessage(CUPSD_LOG_DEBUG, "DNS_SD \"%s\" %s", name, temptxt->text);
  }

  error = avahi_entry_group_add_service_strlst(*srv, AVAHI_IF_UNSPEC,
                                               AVAHI_PROTO_UNSPEC, 0, name,
                                               type, NULL, NULL, port,
                                               txt ? *txt : NULL);
  if (error)
    cupsdLogMessage(CUPSD_LOG_DEBUG, "DNS-SD service add for \"%s\" failed.",
                    name);

  if (!error && subtypes)
  {
   /*
    * Register all of the subtypes...
    */

    char	*start,			/* Start of subtype */
		subtype[256];		/* Subtype string */

    strlcpy(temp, subtypes, sizeof(temp));

    for (start = temp; *start; start = ptr)
    {
     /*
      * Skip leading whitespace...
      */

      while (*start && isspace(*start & 255))
        start ++;

     /*
      * Grab everything up to the next comma or the end of the string...
      */

      for (ptr = start; *ptr && *ptr != ','; ptr ++);

      if (*ptr)
        *ptr++ = '\0';

      if (!*start)
        break;

     /*
      * Register the subtype...
      */

      snprintf(subtype, sizeof(subtype), "%s._sub.%s", start, type);

      error = avahi_entry_group_add_service_subtype(*srv, AVAHI_IF_UNSPEC,
                                                    AVAHI_PROTO_UNSPEC, 0,
                                                    name, type, NULL, subtype);
      if (error)
      {
        cupsdLogMessage(CUPSD_LOG_DEBUG,
                        "DNS-SD subtype %s registration for \"%s\" failed." ,
                        subtype, name);
        break;
      }
    }
  }

  if (!error && commit)
  {
    if ((error = avahi_entry_group_commit(*srv)) != 0)
      cupsdLogMessage(CUPSD_LOG_DEBUG, "DNS-SD commit of \"%s\" failed.",
                      name);
  }

  avahi_threaded_poll_unlock(DNSSDMaster);
#  endif /* HAVE_DNSSD */

  if (error)
  {
    cupsdLogMessage(CUPSD_LOG_WARN, "DNS-SD registration of \"%s\" failed: %s",
                    name, dnssdErrorString(error));
    cupsdLogMessage(CUPSD_LOG_DEBUG, "DNS-SD type: %s", type);
    if (subtypes)
      cupsdLogMessage(CUPSD_LOG_DEBUG, "DNS-SD sub-types: %s", subtypes);
  }

  return (!error);
}


/*
 * 'dnssdRegisterPrinter()' - Start sending broadcast information for a printer
 *		              or update the broadcast contents.
 */

static void
dnssdRegisterPrinter(cupsd_printer_t *p)/* I - Printer */
{
  char		name[256];		/* Service name */
  int		printer_port;		/* LPD port number */
  int		status;			/* Registration status */
  cupsd_txt_t	ipp_txt,		/* IPP(S) TXT record */
 		printer_txt;		/* LPD TXT record */

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "dnssdRegisterPrinter(%s) %s", p->name,
                  !p->ipp_srv ? "new" : "update");

 /*
  * Remove the current registrations if we have them and then return if
  * per-printer sharing was just disabled...
  */

  dnssdDeregisterPrinter(p, 0);

  if (!p->shared)
    return;

 /*
  * Set the registered name as needed; the registered name takes the form of
  * "<printer-info> @ <computer name>"...
  */

  if (!p->reg_name)
  {
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
  }
  else
    strlcpy(name, p->reg_name, sizeof(name));

 /*
  * Register IPP and LPD...
  *
  * We always must register the "_printer" service type in order to reserve
  * our name, but use port number 0 if we haven't actually configured cups-lpd
  * to share via LPD...
  */

  ipp_txt     = dnssdBuildTxtRecord(p, 0);
  printer_txt = dnssdBuildTxtRecord(p, 1);

  if (BrowseLocalProtocols & BROWSE_LPD)
    printer_port = 515;
  else
    printer_port = 0;

  status = dnssdRegisterInstance(NULL, p, name, "_printer._tcp", NULL,
                                 printer_port, &printer_txt, 0);

#  ifdef HAVE_SSL
  if (status)
    dnssdRegisterInstance(NULL, p, name, "_ipps._tcp", DNSSDSubTypes,
			  DNSSDPort, &ipp_txt, 0);
#  endif /* HAVE_SSL */

  if (status)
  {
   /*
    * Use the "_fax-ipp" service type for fax queues, otherwise use "_ipp"...
    */

    if (p->type & CUPS_PRINTER_FAX)
      status = dnssdRegisterInstance(NULL, p, name, "_fax-ipp._tcp",
                                     DNSSDSubTypes, DNSSDPort, &ipp_txt, 1);
    else
      status = dnssdRegisterInstance(NULL, p, name, "_ipp._tcp", DNSSDSubTypes,
                                     DNSSDPort, &ipp_txt, 1);
  }

  dnssdFreeTxtRecord(&ipp_txt);
  dnssdFreeTxtRecord(&printer_txt);

  if (status)
  {
   /*
    * Save the registered name and add the printer to the array of DNS-SD
    * printers...
    */

    cupsdSetString(&p->reg_name, name);
    cupsArrayAdd(DNSSDPrinters, p);
  }
  else
  {
   /*
    * Registration failed for this printer...
    */

    dnssdDeregisterInstance(&p->ipp_srv);

#  ifdef HAVE_DNSSD
#    ifdef HAVE_SSL
    dnssdDeregisterInstance(&p->ipps_srv);
#    endif /* HAVE_SSL */
    dnssdDeregisterInstance(&p->printer_srv);
#  endif /* HAVE_DNSSD */
  }
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
    dnssdDeregisterPrinter(p, 1);

 /*
  * Shutdown the rest of the service refs...
  */

  dnssdDeregisterInstance(&WebIFSrv);

#  ifdef HAVE_DNSSD
  cupsdRemoveSelect(DNSServiceRefSockFD(DNSSDMaster));

  DNSServiceRefDeallocate(DNSSDMaster);
  DNSSDMaster = NULL;

#  else /* HAVE_AVAHI */
  avahi_client_free(DNSSDClient);
  DNSSDClient = NULL;

  avahi_threaded_poll_free(DNSSDMaster);
  DNSSDMaster = NULL;
#  endif /* HAVE_DNSSD */

  cupsArrayDelete(DNSSDPrinters);
  DNSSDPrinters = NULL;

  DNSSDPort = 0;
}


#  ifdef HAVE_DNSSD
/*
 * 'dnssdUpdate()' - Handle DNS-SD queries.
 */

static void
dnssdUpdate(void)
{
  DNSServiceErrorType	sdErr;		/* Service discovery error */


  if ((sdErr = DNSServiceProcessResult(DNSSDMaster)) != kDNSServiceErr_NoError)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "DNS Service Discovery registration error %d!",
	            sdErr);
    dnssdStop();
  }
}
#  endif /* HAVE_DNSSD */


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
      auth_type = cupsdDefaultAuthType();

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
#endif /* HAVE_DNSSD || HAVE_AVAHI */


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
