/*
 * "$Id: dest.c 13075 2016-01-29 21:14:05Z msweet $"
 *
 * User-defined destination (and option) support for CUPS.
 *
 * Copyright 2007-2016 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include <sys/stat.h>

#ifdef HAVE_NOTIFY_H
#  include <notify.h>
#endif /* HAVE_NOTIFY_H */

#ifdef HAVE_POLL
#  include <poll.h>
#endif /* HAVE_POLL */

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
 * Constants...
 */

#ifdef __APPLE__
#  include <SystemConfiguration/SystemConfiguration.h>
#  define kCUPSPrintingPrefs	CFSTR("org.cups.PrintingPrefs")
#  define kDefaultPaperIDKey	CFSTR("DefaultPaperID")
#  define kLastUsedPrintersKey	CFSTR("LastUsedPrinters")
#  define kLocationNetworkKey	CFSTR("Network")
#  define kLocationPrinterIDKey	CFSTR("PrinterID")
#  define kUseLastPrinter	CFSTR("UseLastPrinter")
#endif /* __APPLE__ */


/*
 * Types...
 */

#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
typedef enum _cups_dnssd_state_e	/* Enumerated device state */
{
  _CUPS_DNSSD_NEW,
  _CUPS_DNSSD_QUERY,
  _CUPS_DNSSD_PENDING,
  _CUPS_DNSSD_ACTIVE,
  _CUPS_DNSSD_LOCAL,
  _CUPS_DNSSD_INCOMPATIBLE,
  _CUPS_DNSSD_ERROR
} _cups_dnssd_state_t;

typedef struct _cups_dnssd_data_s	/* Enumeration data */
{
#  ifdef HAVE_DNSSD
  DNSServiceRef		main_ref;	/* Main service reference */
#  else /* HAVE_AVAHI */
  AvahiSimplePoll	*simple_poll;	/* Polling interface */
  AvahiClient		*client;	/* Client information */
  int			got_data;	/* Did we get data? */
#  endif /* HAVE_DNSSD */
  cups_dest_cb_t	cb;		/* Callback */
  void			*user_data;	/* User data pointer */
  cups_ptype_t		type,		/* Printer type filter */
			mask;		/* Printer type mask */
  cups_array_t		*devices;	/* Devices found so far */
} _cups_dnssd_data_t;

typedef struct _cups_dnssd_device_s	/* Enumerated device */
{
  _cups_dnssd_state_t	state;		/* State of device listing */
#  ifdef HAVE_DNSSD
  DNSServiceRef		ref;		/* Service reference for query */
#  else /* HAVE_AVAHI */
  AvahiRecordBrowser	*ref;		/* Browser for query */
#  endif /* HAVE_DNSSD */
  char			*domain,	/* Domain name */
			*fullName,	/* Full name */
			*regtype;	/* Registration type */
  cups_ptype_t		type;		/* Device registration type */
  cups_dest_t		dest;		/* Destination record */
} _cups_dnssd_device_t;

typedef struct _cups_dnssd_resolve_s	/* Data for resolving URI */
{
  int			*cancel;	/* Pointer to "cancel" variable */
  struct timeval	end_time;	/* Ending time */
} _cups_dnssd_resolve_t;
#endif /* HAVE_DNSSD */


/*
 * Local functions...
 */

#ifdef __APPLE__
static CFArrayRef	appleCopyLocations(void);
static CFStringRef	appleCopyNetwork(void);
static char		*appleGetPaperSize(char *name, size_t namesize);
static CFStringRef	appleGetPrinter(CFArrayRef locations,
			                CFStringRef network, CFIndex *locindex);
#endif /* __APPLE__ */
static cups_dest_t	*cups_add_dest(const char *name, const char *instance,
				       int *num_dests, cups_dest_t **dests);
#ifdef __BLOCKS__
static int		cups_block_cb(cups_dest_block_t block, unsigned flags,
			              cups_dest_t *dest);
#endif /* __BLOCKS__ */
static int		cups_compare_dests(cups_dest_t *a, cups_dest_t *b);
#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
#  ifdef HAVE_DNSSD
static void		cups_dnssd_browse_cb(DNSServiceRef sdRef,
					     DNSServiceFlags flags,
					     uint32_t interfaceIndex,
					     DNSServiceErrorType errorCode,
					     const char *serviceName,
					     const char *regtype,
					     const char *replyDomain,
					     void *context);
#  else /* HAVE_AVAHI */
static void		cups_dnssd_browse_cb(AvahiServiceBrowser *browser,
					     AvahiIfIndex interface,
					     AvahiProtocol protocol,
					     AvahiBrowserEvent event,
					     const char *serviceName,
					     const char *regtype,
					     const char *replyDomain,
					     AvahiLookupResultFlags flags,
					     void *context);
static void		cups_dnssd_client_cb(AvahiClient *client,
					     AvahiClientState state,
					     void *context);
#  endif /* HAVE_DNSSD */
static int		cups_dnssd_compare_devices(_cups_dnssd_device_t *a,
			                           _cups_dnssd_device_t *b);
static void		cups_dnssd_free_device(_cups_dnssd_device_t *device,
			                       _cups_dnssd_data_t *data);
static _cups_dnssd_device_t *
			cups_dnssd_get_device(_cups_dnssd_data_t *data,
					      const char *serviceName,
					      const char *regtype,
					      const char *replyDomain);
#  ifdef HAVE_DNSSD
static void		cups_dnssd_local_cb(DNSServiceRef sdRef,
					    DNSServiceFlags flags,
					    uint32_t interfaceIndex,
					    DNSServiceErrorType errorCode,
					    const char *serviceName,
					    const char *regtype,
					    const char *replyDomain,
					    void *context);
static void		cups_dnssd_query_cb(DNSServiceRef sdRef,
					    DNSServiceFlags flags,
					    uint32_t interfaceIndex,
					    DNSServiceErrorType errorCode,
					    const char *fullName,
					    uint16_t rrtype, uint16_t rrclass,
					    uint16_t rdlen, const void *rdata,
					    uint32_t ttl, void *context);
#  else /* HAVE_AVAHI */
static int		cups_dnssd_poll_cb(struct pollfd *pollfds,
					   unsigned int num_pollfds,
					   int timeout, void *context);
static void		cups_dnssd_query_cb(AvahiRecordBrowser *browser,
					    AvahiIfIndex interface,
					    AvahiProtocol protocol,
					    AvahiBrowserEvent event,
					    const char *name, uint16_t rrclass,
					    uint16_t rrtype, const void *rdata,
					    size_t rdlen,
					    AvahiLookupResultFlags flags,
					    void *context);
#  endif /* HAVE_DNSSD */
static const char	*cups_dnssd_resolve(cups_dest_t *dest, const char *uri,
					    int msec, int *cancel,
					    cups_dest_cb_t cb, void *user_data);
static int		cups_dnssd_resolve_cb(void *context);
static void		cups_dnssd_unquote(char *dst, const char *src,
			                   size_t dstsize);
#endif /* HAVE_DNSSD || HAVE_AVAHI */
static int		cups_find_dest(const char *name, const char *instance,
				       int num_dests, cups_dest_t *dests, int prev,
				       int *rdiff);
static char		*cups_get_default(const char *filename, char *namebuf,
					  size_t namesize, const char **instance);
static int		cups_get_dests(const char *filename, const char *match_name,
			               const char *match_inst, int user_default_set,
				       int num_dests, cups_dest_t **dests);
static char		*cups_make_string(ipp_attribute_t *attr, char *buffer,
			                  size_t bufsize);


/*
 * 'cupsAddDest()' - Add a destination to the list of destinations.
 *
 * This function cannot be used to add a new class or printer queue,
 * it only adds a new container of saved options for the named
 * destination or instance.
 *
 * If the named destination already exists, the destination list is
 * returned unchanged.  Adding a new instance of a destination creates
 * a copy of that destination's options.
 *
 * Use the @link cupsSaveDests@ function to save the updated list of
 * destinations to the user's lpoptions file.
 */

int					/* O  - New number of destinations */
cupsAddDest(const char  *name,		/* I  - Destination name */
            const char	*instance,	/* I  - Instance name or @code NULL@ for none/primary */
            int         num_dests,	/* I  - Number of destinations */
            cups_dest_t **dests)	/* IO - Destinations */
{
  int		i;			/* Looping var */
  cups_dest_t	*dest;			/* Destination pointer */
  cups_dest_t	*parent = NULL;		/* Parent destination */
  cups_option_t	*doption,		/* Current destination option */
		*poption;		/* Current parent option */


  if (!name || !dests)
    return (0);

  if (!cupsGetDest(name, instance, num_dests, *dests))
  {
    if (instance && !cupsGetDest(name, NULL, num_dests, *dests))
      return (num_dests);

    if ((dest = cups_add_dest(name, instance, &num_dests, dests)) == NULL)
      return (num_dests);

   /*
    * Find the base dest again now the array has been realloc'd.
    */

    parent = cupsGetDest(name, NULL, num_dests, *dests);

    if (instance && parent && parent->num_options > 0)
    {
     /*
      * Copy options from parent...
      */

      dest->options = calloc(sizeof(cups_option_t), (size_t)parent->num_options);

      if (dest->options)
      {
        dest->num_options = parent->num_options;

	for (i = dest->num_options, doption = dest->options,
	         poption = parent->options;
	     i > 0;
	     i --, doption ++, poption ++)
	{
	  doption->name  = _cupsStrRetain(poption->name);
	  doption->value = _cupsStrRetain(poption->value);
	}
      }
    }
  }

  return (num_dests);
}


#ifdef __APPLE__
/*
 * '_cupsAppleCopyDefaultPaperID()' - Get the default paper ID.
 */

CFStringRef				/* O - Default paper ID */
_cupsAppleCopyDefaultPaperID(void)
{
  return (CFPreferencesCopyAppValue(kDefaultPaperIDKey,
                                    kCUPSPrintingPrefs));
}


/*
 * '_cupsAppleCopyDefaultPrinter()' - Get the default printer at this location.
 */

CFStringRef				/* O - Default printer name */
_cupsAppleCopyDefaultPrinter(void)
{
  CFStringRef	network;		/* Network location */
  CFArrayRef	locations;		/* Location array */
  CFStringRef	locprinter;		/* Current printer */


 /*
  * Use location-based defaults only if "use last printer" is selected in the
  * system preferences...
  */

  if (!_cupsAppleGetUseLastPrinter())
  {
    DEBUG_puts("1_cupsAppleCopyDefaultPrinter: Not using last printer as "
	       "default.");
    return (NULL);
  }

 /*
  * Get the current location...
  */

  if ((network = appleCopyNetwork()) == NULL)
  {
    DEBUG_puts("1_cupsAppleCopyDefaultPrinter: Unable to get current "
               "network.");
    return (NULL);
  }

 /*
  * Lookup the network in the preferences...
  */

  if ((locations = appleCopyLocations()) == NULL)
  {
   /*
    * Missing or bad location array, so no location-based default...
    */

    DEBUG_puts("1_cupsAppleCopyDefaultPrinter: Missing or bad last used "
	       "printer array.");

    CFRelease(network);

    return (NULL);
  }

  DEBUG_printf(("1_cupsAppleCopyDefaultPrinter: Got locations, %d entries.",
                (int)CFArrayGetCount(locations)));

  if ((locprinter = appleGetPrinter(locations, network, NULL)) != NULL)
    CFRetain(locprinter);

  CFRelease(network);
  CFRelease(locations);

  return (locprinter);
}


/*
 * '_cupsAppleGetUseLastPrinter()' - Get whether to use the last used printer.
 */

int					/* O - 1 to use last printer, 0 otherwise */
_cupsAppleGetUseLastPrinter(void)
{
  Boolean	uselast,		/* Use last printer preference value */
		uselast_set;		/* Valid is set? */


  if (getenv("CUPS_DISABLE_APPLE_DEFAULT"))
    return (0);

  uselast = CFPreferencesGetAppBooleanValue(kUseLastPrinter,
                                            kCUPSPrintingPrefs,
					    &uselast_set);
  if (!uselast_set)
    return (1);
  else
    return (uselast);
}


/*
 * '_cupsAppleSetDefaultPaperID()' - Set the default paper id.
 */

void
_cupsAppleSetDefaultPaperID(
    CFStringRef name)			/* I - New paper ID */
{
  CFPreferencesSetAppValue(kDefaultPaperIDKey, name, kCUPSPrintingPrefs);
  CFPreferencesAppSynchronize(kCUPSPrintingPrefs);
  notify_post("com.apple.printerPrefsChange");
}


/*
 * '_cupsAppleSetDefaultPrinter()' - Set the default printer for this location.
 */

void
_cupsAppleSetDefaultPrinter(
    CFStringRef name)			/* I - Default printer/class name */
{
  CFStringRef		network;	/* Current network */
  CFArrayRef		locations;	/* Old locations array */
  CFIndex		locindex;	/* Index in locations array */
  CFStringRef		locprinter;	/* Current printer */
  CFMutableArrayRef	newlocations;	/* New locations array */
  CFMutableDictionaryRef newlocation;	/* New location */


 /*
  * Get the current location...
  */

  if ((network = appleCopyNetwork()) == NULL)
  {
    DEBUG_puts("1_cupsAppleSetDefaultPrinter: Unable to get current network...");
    return;
  }

 /*
  * Lookup the network in the preferences...
  */

  if ((locations = appleCopyLocations()) != NULL)
    locprinter = appleGetPrinter(locations, network, &locindex);
  else
  {
    locprinter = NULL;
    locindex   = -1;
  }

  if (!locprinter || CFStringCompare(locprinter, name, 0) != kCFCompareEqualTo)
  {
   /*
    * Need to change the locations array...
    */

    if (locations)
    {
      newlocations = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0,
                                              locations);

      if (locprinter)
        CFArrayRemoveValueAtIndex(newlocations, locindex);
    }
    else
      newlocations = CFArrayCreateMutable(kCFAllocatorDefault, 0,
					  &kCFTypeArrayCallBacks);

    newlocation = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);

    if (newlocation && newlocations)
    {
     /*
      * Put the new location at the front of the array...
      */

      CFDictionaryAddValue(newlocation, kLocationNetworkKey, network);
      CFDictionaryAddValue(newlocation, kLocationPrinterIDKey, name);
      CFArrayInsertValueAtIndex(newlocations, 0, newlocation);

     /*
      * Limit the number of locations to 10...
      */

      while (CFArrayGetCount(newlocations) > 10)
        CFArrayRemoveValueAtIndex(newlocations, 10);

     /*
      * Push the changes out...
      */

      CFPreferencesSetAppValue(kLastUsedPrintersKey, newlocations,
                               kCUPSPrintingPrefs);
      CFPreferencesAppSynchronize(kCUPSPrintingPrefs);
      notify_post("com.apple.printerPrefsChange");
    }

    if (newlocations)
      CFRelease(newlocations);

    if (newlocation)
      CFRelease(newlocation);
  }

  if (locations)
    CFRelease(locations);

  CFRelease(network);
}


/*
 * '_cupsAppleSetUseLastPrinter()' - Set whether to use the last used printer.
 */

void
_cupsAppleSetUseLastPrinter(
    int uselast)			/* O - 1 to use last printer, 0 otherwise */
{
  CFPreferencesSetAppValue(kUseLastPrinter,
			   uselast ? kCFBooleanTrue : kCFBooleanFalse,
			   kCUPSPrintingPrefs);
  CFPreferencesAppSynchronize(kCUPSPrintingPrefs);
  notify_post("com.apple.printerPrefsChange");
}
#endif /* __APPLE__ */


/*
 * 'cupsConnectDest()' - Connect to the server for a destination.
 *
 * Connect to the destination, returning a new http_t connection object and
 * optionally the resource path to use for the destination.  These calls will
 * block until a connection is made, the timeout expires, the integer pointed
 * to by "cancel" is non-zero, or the callback function (or block) returns 0,
 * The caller is responsible for calling httpClose() on the returned object.
 *
 * @since CUPS 1.6/OS X 10.8@
 */

http_t *				/* O - Connection to server or @code NULL@ */
cupsConnectDest(
    cups_dest_t    *dest,		/* I - Destination */
    unsigned       flags,		/* I - Connection flags */
    int            msec,		/* I - Timeout in milliseconds */
    int            *cancel,		/* I - Pointer to "cancel" variable */
    char           *resource,		/* I - Resource buffer */
    size_t         resourcesize,	/* I - Size of resource buffer */
    cups_dest_cb_t cb,			/* I - Callback function */
    void           *user_data)		/* I - User data pointer */
{
  const char	*uri;			/* Printer URI */
  char		scheme[32],		/* URI scheme */
		userpass[256],		/* Username and password (unused) */
		hostname[256],		/* Hostname */
		tempresource[1024];	/* Temporary resource buffer */
  int		port;			/* Port number */
  char		portstr[16];		/* Port number string */
  http_encryption_t encryption;		/* Encryption to use */
  http_addrlist_t *addrlist;		/* Address list for server */
  http_t	*http;			/* Connection to server */


 /*
  * Range check input...
  */

  if (!dest)
  {
    if (resource)
      *resource = '\0';

    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    return (NULL);
  }

  if (!resource || resourcesize < 1)
  {
    resource     = tempresource;
    resourcesize = sizeof(tempresource);
  }

 /*
  * Grab the printer URI...
  */

  if ((uri = cupsGetOption("printer-uri-supported", dest->num_options,
                           dest->options)) == NULL)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(ENOENT), 0);

    if (cb)
      (*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_ERROR,
            dest);

    return (NULL);
  }

#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
  if (strstr(uri, "._tcp"))
  {
    if ((uri = cups_dnssd_resolve(dest, uri, msec, cancel, cb,
                                  user_data)) == NULL)
      return (NULL);
  }
#endif /* HAVE_DNSSD || HAVE_AVAHI */

  if (httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, sizeof(scheme),
                      userpass, sizeof(userpass), hostname, sizeof(hostname),
                      &port, resource, (int)resourcesize) < HTTP_URI_STATUS_OK)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad printer-uri."), 1);

    if (cb)
      (*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_ERROR,
            dest);

    return (NULL);
  }

 /*
  * Lookup the address for the server...
  */

  if (cb)
    (*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_RESOLVING,
          dest);

  snprintf(portstr, sizeof(portstr), "%d", port);

  if ((addrlist = httpAddrGetList(hostname, AF_UNSPEC, portstr)) == NULL)
  {
    if (cb)
      (*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_ERROR,
            dest);

    return (NULL);
  }

  if (cancel && *cancel)
  {
    httpAddrFreeList(addrlist);

    if (cb)
      (*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_CANCELED,
            dest);

    return (NULL);
  }

 /*
  * Create the HTTP object pointing to the server referenced by the URI...
  */

  if (!strcmp(scheme, "ipps") || port == 443)
    encryption = HTTP_ENCRYPTION_ALWAYS;
  else
    encryption = HTTP_ENCRYPTION_IF_REQUESTED;

  http = httpConnect2(hostname, port, addrlist, AF_UNSPEC, encryption, 1, 0,
                      NULL);
  httpAddrFreeList(addrlist);

 /*
  * Connect if requested...
  */

  if (flags & CUPS_DEST_FLAGS_UNCONNECTED)
  {
    if (cb)
      (*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED, dest);
  }
  else
  {
    if (cb)
      (*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_CONNECTING,
            dest);

    if (!httpReconnect2(http, msec, cancel) && cb)
    {
      if (cancel && *cancel)
	(*cb)(user_data,
	      CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_CONNECTING, dest);
      else
	(*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_ERROR,
	      dest);
    }
    else if (cb)
      (*cb)(user_data, CUPS_DEST_FLAGS_NONE, dest);
  }

  return (http);
}


#ifdef __BLOCKS__
/*
 * 'cupsConnectDestBlock()' - Connect to the server for a destination.
 *
 * Connect to the destination, returning a new http_t connection object and
 * optionally the resource path to use for the destination.  These calls will
 * block until a connection is made, the timeout expires, the integer pointed
 * to by "cancel" is non-zero, or the callback function (or block) returns 0,
 * The caller is responsible for calling httpClose() on the returned object.
 *
 * @since CUPS 1.6/OS X 10.8@
 */

http_t *				/* O - Connection to server or @code NULL@ */
cupsConnectDestBlock(
    cups_dest_t       *dest,		/* I - Destination */
    unsigned          flags,		/* I - Connection flags */
    int               msec,		/* I - Timeout in milliseconds */
    int               *cancel,		/* I - Pointer to "cancel" variable */
    char              *resource,	/* I - Resource buffer */
    size_t            resourcesize,	/* I - Size of resource buffer */
    cups_dest_block_t block)		/* I - Callback block */
{
  return (cupsConnectDest(dest, flags, msec, cancel, resource, resourcesize,
                          (cups_dest_cb_t)cups_block_cb, (void *)block));
}
#endif /* __BLOCKS__ */


/*
 * 'cupsCopyDest()' - Copy a destination.
 *
 * Make a copy of the destination to an array of destinations (or just a single
 * copy) - for use with the cupsEnumDests* functions. The caller is responsible
 * for calling cupsFreeDests() on the returned object(s).
 *
 * @since CUPS 1.6/OS X 10.8@
 */

int
cupsCopyDest(cups_dest_t *dest,
             int         num_dests,
             cups_dest_t **dests)
{
  int		i;			/* Looping var */
  cups_dest_t	*new_dest;		/* New destination pointer */
  cups_option_t	*new_option,		/* Current destination option */
		*option;		/* Current parent option */


 /*
  * Range check input...
  */

  if (!dest || num_dests < 0 || !dests)
    return (num_dests);

 /*
  * See if the destination already exists...
  */

  if ((new_dest = cupsGetDest(dest->name, dest->instance, num_dests,
                              *dests)) != NULL)
  {
   /*
    * Protect against copying destination to itself...
    */

    if (new_dest == dest)
      return (num_dests);

   /*
    * Otherwise, free the options...
    */

    cupsFreeOptions(new_dest->num_options, new_dest->options);

    new_dest->num_options = 0;
    new_dest->options     = NULL;
  }
  else
    new_dest = cups_add_dest(dest->name, dest->instance, &num_dests, dests);

  if (new_dest)
  {
    if ((new_dest->options = calloc(sizeof(cups_option_t), (size_t)dest->num_options)) == NULL)
      return (cupsRemoveDest(dest->name, dest->instance, num_dests, dests));

    new_dest->num_options = dest->num_options;

    for (i = dest->num_options, option = dest->options,
	     new_option = new_dest->options;
	 i > 0;
	 i --, option ++, new_option ++)
    {
      new_option->name  = _cupsStrRetain(option->name);
      new_option->value = _cupsStrRetain(option->value);
    }
  }

  return (num_dests);
}


/*
 * 'cupsEnumDests()' - Enumerate available destinations with a callback function.
 *
 * Destinations are enumerated from one or more sources. The callback function
 * receives the @code user_data@ pointer, destination name, instance, number of
 * options, and options which can be used as input to the @link cupsAddDest@
 * function.  The function must return 1 to continue enumeration or 0 to stop.
 *
 * Enumeration happens on the current thread and does not return until all
 * destinations have been enumerated or the callback function returns 0.
 *
 * @since CUPS 1.6/OS X 10.8@
 */

int					/* O - 1 on success, 0 on failure */
cupsEnumDests(
    unsigned       flags,		/* I - Enumeration flags */
    int            msec,		/* I - Timeout in milliseconds,
					 *     -1 for indefinite */
    int            *cancel,		/* I - Pointer to "cancel" variable */
    cups_ptype_t   type,		/* I - Printer type bits */
    cups_ptype_t   mask,		/* I - Mask for printer type bits */
    cups_dest_cb_t cb,			/* I - Callback function */
    void           *user_data)		/* I - User data */
{
  int			i,		/* Looping var */
			num_dests;	/* Number of destinations */
  cups_dest_t		*dests = NULL,	/* Destinations */
			*dest;		/* Current destination */
  const char		*defprinter;	/* Default printer */
  char			name[1024],	/* Copy of printer name */
			*instance,	/* Pointer to instance name */
			*user_default;	/* User default printer */
#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
  int			count,		/* Number of queries started */
			remaining;	/* Remainder of timeout */
  _cups_dnssd_data_t	data;		/* Data for callback */
  _cups_dnssd_device_t	*device;	/* Current device */
#  ifdef HAVE_DNSSD
  int			nfds,		/* Number of files responded */
			main_fd;	/* File descriptor for lookups */
  DNSServiceRef		ipp_ref,	/* IPP browser */
			local_ipp_ref;	/* Local IPP browser */
#    ifdef HAVE_SSL
  DNSServiceRef		ipps_ref,	/* IPPS browser */
			local_ipps_ref;	/* Local IPPS browser */
#    endif /* HAVE_SSL */
#    ifdef HAVE_POLL
  struct pollfd		pfd;		/* Polling data */
#    else
  fd_set		input;		/* Input set for select() */
  struct timeval	timeout;	/* Timeout for select() */
#    endif /* HAVE_POLL */
#  else /* HAVE_AVAHI */
  int			error;		/* Error value */
  AvahiServiceBrowser	*ipp_ref;	/* IPP browser */
#    ifdef HAVE_SSL
  AvahiServiceBrowser	*ipps_ref;	/* IPPS browser */
#    endif /* HAVE_SSL */
#  endif /* HAVE_DNSSD */
#endif /* HAVE_DNSSD || HAVE_AVAHI */

 /*
  * Range check input...
  */

  (void)flags;

  if (!cb)
    return (0);

 /*
  * Get the list of local printers and pass them to the callback function...
  */

  num_dests = _cupsGetDests(CUPS_HTTP_DEFAULT, IPP_OP_CUPS_GET_PRINTERS, NULL,
                            &dests, type, mask | CUPS_PRINTER_3D);

  if ((user_default = _cupsUserDefault(name, sizeof(name))) != NULL)
    defprinter = name;
  else if ((defprinter = cupsGetDefault2(CUPS_HTTP_DEFAULT)) != NULL)
  {
    strlcpy(name, defprinter, sizeof(name));
    defprinter = name;
  }

  if (defprinter)
  {
   /*
    * Separate printer and instance name...
    */

    if ((instance = strchr(name, '/')) != NULL)
      *instance++ = '\0';

   /*
    * Lookup the printer and instance and make it the default...
    */

    if ((dest = cupsGetDest(name, instance, num_dests, dests)) != NULL)
      dest->is_default = 1;
  }

  for (i = num_dests, dest = dests;
       i > 0 && (!cancel || !*cancel);
       i --, dest ++)
    if (!(*cb)(user_data, i > 1 ? CUPS_DEST_FLAGS_MORE : CUPS_DEST_FLAGS_NONE,
               dest))
      break;

  cupsFreeDests(num_dests, dests);

  if (i > 0 || msec == 0)
    return (1);

#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
 /*
  * Get Bonjour-shared printers...
  */

  data.type      = type;
  data.mask      = mask;
  data.cb        = cb;
  data.user_data = user_data;
  data.devices   = cupsArrayNew3((cups_array_func_t)cups_dnssd_compare_devices, NULL, NULL, 0, NULL, (cups_afree_func_t)cups_dnssd_free_device);

#  ifdef HAVE_DNSSD
  if (DNSServiceCreateConnection(&data.main_ref) != kDNSServiceErr_NoError)
    return (0);

  main_fd = DNSServiceRefSockFD(data.main_ref);

  ipp_ref = data.main_ref;
  DNSServiceBrowse(&ipp_ref, kDNSServiceFlagsShareConnection, 0,
                   "_ipp._tcp", NULL,
                   (DNSServiceBrowseReply)cups_dnssd_browse_cb, &data);

  local_ipp_ref = data.main_ref;
  DNSServiceBrowse(&local_ipp_ref, kDNSServiceFlagsShareConnection,
                   kDNSServiceInterfaceIndexLocalOnly,
                   "_ipp._tcp", NULL,
                   (DNSServiceBrowseReply)cups_dnssd_local_cb, &data);

#    ifdef HAVE_SSL
  ipps_ref = data.main_ref;
  DNSServiceBrowse(&ipps_ref, kDNSServiceFlagsShareConnection, 0,
                   "_ipps._tcp", NULL,
                   (DNSServiceBrowseReply)cups_dnssd_browse_cb, &data);

  local_ipps_ref = data.main_ref;
  DNSServiceBrowse(&local_ipps_ref, kDNSServiceFlagsShareConnection,
                   kDNSServiceInterfaceIndexLocalOnly,
                   "_ipps._tcp", NULL,
                   (DNSServiceBrowseReply)cups_dnssd_local_cb, &data);
#    endif /* HAVE_SSL */

#  else /* HAVE_AVAHI */
  if ((data.simple_poll = avahi_simple_poll_new()) == NULL)
  {
    DEBUG_puts("cupsEnumDests: Unable to create Avahi simple poll object.");
    return (1);
  }

  avahi_simple_poll_set_func(data.simple_poll, cups_dnssd_poll_cb, &data);

  data.client = avahi_client_new(avahi_simple_poll_get(data.simple_poll),
				 0, cups_dnssd_client_cb, &data,
				 &error);
  if (!data.client)
  {
    DEBUG_puts("cupsEnumDests: Unable to create Avahi client.");
    avahi_simple_poll_free(data.simple_poll);
    return (1);
  }

  ipp_ref  = avahi_service_browser_new(data.client, AVAHI_IF_UNSPEC,
				       AVAHI_PROTO_UNSPEC, "_ipp._tcp", NULL,
				       0, cups_dnssd_browse_cb, &data);
#    ifdef HAVE_SSL
  ipps_ref = avahi_service_browser_new(data.client, AVAHI_IF_UNSPEC,
			               AVAHI_PROTO_UNSPEC, "_ipps._tcp", NULL,
			               0, cups_dnssd_browse_cb, &data);
#    endif /* HAVE_SSL */
#  endif /* HAVE_DNSSD */

  if (msec < 0)
    remaining = INT_MAX;
  else
    remaining = msec;

  while (remaining > 0 && (!cancel || !*cancel))
  {
   /*
    * Check for input...
    */

#  ifdef HAVE_DNSSD
#    ifdef HAVE_POLL
    pfd.fd     = main_fd;
    pfd.events = POLLIN;

    nfds = poll(&pfd, 1, remaining > 250 ? 250 : remaining);

#    else
    FD_ZERO(&input);
    FD_SET(main_fd, &input);

    timeout.tv_sec  = 0;
    timeout.tv_usec = remaining > 250 ? 250000 : remaining * 1000;

    nfds = select(main_fd + 1, &input, NULL, NULL, &timeout);
#    endif /* HAVE_POLL */

    if (nfds > 0)
      DNSServiceProcessResult(data.main_ref);
    else if (nfds == 0)
      remaining -= 250;

#  else /* HAVE_AVAHI */
    data.got_data = 0;

    if ((error = avahi_simple_poll_iterate(data.simple_poll, 250)) > 0)
    {
     /*
      * We've been told to exit the loop.  Perhaps the connection to
      * Avahi failed.
      */

      break;
    }

    if (!data.got_data)
      remaining -= 250;
#  endif /* HAVE_DNSSD */

    for (device = (_cups_dnssd_device_t *)cupsArrayFirst(data.devices),
             count = 0;
         device;
         device = (_cups_dnssd_device_t *)cupsArrayNext(data.devices))
    {
      if (device->ref)
        count ++;

      if (!device->ref && device->state == _CUPS_DNSSD_NEW)
      {
	DEBUG_printf(("1cupsEnumDests: Querying '%s'.", device->fullName));

#  ifdef HAVE_DNSSD
        device->ref = data.main_ref;

	if (DNSServiceQueryRecord(&(device->ref),
				  kDNSServiceFlagsShareConnection,
				  0, device->fullName,
				  kDNSServiceType_TXT,
				  kDNSServiceClass_IN,
				  (DNSServiceQueryRecordReply)cups_dnssd_query_cb,
				  &data) == kDNSServiceErr_NoError)
	{
	  count ++;
	}
	else
	{
	  device->ref   = 0;
	  device->state = _CUPS_DNSSD_ERROR;

	  DEBUG_puts("1cupsEnumDests: Query failed.");
	}

#  else /* HAVE_AVAHI */
	if ((device->ref = avahi_record_browser_new(data.client,
	                                            AVAHI_IF_UNSPEC,
						    AVAHI_PROTO_UNSPEC,
						    device->fullName,
						    AVAHI_DNS_CLASS_IN,
						    AVAHI_DNS_TYPE_TXT,
						    0,
						    cups_dnssd_query_cb,
						    &data)) != NULL)
        {
	  count ++;
	}
	else
	{
	  device->state = _CUPS_DNSSD_ERROR;

	  DEBUG_printf(("1cupsEnumDests: Query failed: %s",
	                avahi_strerror(avahi_client_errno(data.client))));
	}
#  endif /* HAVE_DNSSD */
      }
      else if (device->ref && device->state == _CUPS_DNSSD_PENDING)
      {
        if ((device->type & mask) == type)
        {
	  if (!(*cb)(user_data, CUPS_DEST_FLAGS_NONE, &device->dest))
	  {
	    remaining = -1;
	    break;
	  }
        }

        device->state = _CUPS_DNSSD_ACTIVE;
      }
    }
  }

  cupsArrayDelete(data.devices);

#  ifdef HAVE_DNSSD
  DNSServiceRefDeallocate(ipp_ref);
  DNSServiceRefDeallocate(local_ipp_ref);

#    ifdef HAVE_SSL
  DNSServiceRefDeallocate(ipp_ref);
  DNSServiceRefDeallocate(local_ipp_ref);
#    endif /* HAVE_SSL */

  DNSServiceRefDeallocate(data.main_ref);

#  else /* HAVE_AVAHI */
  avahi_service_browser_free(ipp_ref);
#    ifdef HAVE_SSL
  avahi_service_browser_free(ipps_ref);
#    endif /* HAVE_SSL */

  avahi_client_free(data.client);
  avahi_simple_poll_free(data.simple_poll);
#  endif /* HAVE_DNSSD */
#endif /* HAVE_DNSSD || HAVE_DNSSD */

  return (1);
}


#  ifdef __BLOCKS__
/*
 * 'cupsEnumDestsBlock()' - Enumerate available destinations with a block.
 *
 * Destinations are enumerated from one or more sources. The block receives the
 * destination name, instance, number of options, and options which can be used
 * as input to the @link cupsAddDest@ function.  The block must return 1 to
 * continue enumeration or 0 to stop.
 *
 * Enumeration happens on the current thread and does not return until all
 * destinations have been enumerated or the block returns 0.
 *
 * @since CUPS 1.6/OS X 10.8@
 */

int					/* O - 1 on success, 0 on failure */
cupsEnumDestsBlock(
    unsigned          flags,		/* I - Enumeration flags */
    int               timeout,		/* I - Timeout in milliseconds, 0 for indefinite */
    int               *cancel,		/* I - Pointer to "cancel" variable */
    cups_ptype_t      type,		/* I - Printer type bits */
    cups_ptype_t      mask,		/* I - Mask for printer type bits */
    cups_dest_block_t block)		/* I - Block */
{
  return (cupsEnumDests(flags, timeout, cancel, type, mask,
                        (cups_dest_cb_t)cups_block_cb, (void *)block));
}
#  endif /* __BLOCKS__ */


/*
 * 'cupsFreeDests()' - Free the memory used by the list of destinations.
 */

void
cupsFreeDests(int         num_dests,	/* I - Number of destinations */
              cups_dest_t *dests)	/* I - Destinations */
{
  int		i;			/* Looping var */
  cups_dest_t	*dest;			/* Current destination */


  if (num_dests == 0 || dests == NULL)
    return;

  for (i = num_dests, dest = dests; i > 0; i --, dest ++)
  {
    _cupsStrFree(dest->name);
    _cupsStrFree(dest->instance);

    cupsFreeOptions(dest->num_options, dest->options);
  }

  free(dests);
}


/*
 * 'cupsGetDest()' - Get the named destination from the list.
 *
 * Use the @link cupsGetDests@ or @link cupsGetDests2@ functions to get a
 * list of supported destinations for the current user.
 */

cups_dest_t *				/* O - Destination pointer or @code NULL@ */
cupsGetDest(const char  *name,		/* I - Destination name or @code NULL@ for the default destination */
            const char	*instance,	/* I - Instance name or @code NULL@ */
            int         num_dests,	/* I - Number of destinations */
            cups_dest_t *dests)		/* I - Destinations */
{
  int	diff,				/* Result of comparison */
	match;				/* Matching index */


  if (num_dests <= 0 || !dests)
    return (NULL);

  if (!name)
  {
   /*
    * NULL name for default printer.
    */

    while (num_dests > 0)
    {
      if (dests->is_default)
        return (dests);

      num_dests --;
      dests ++;
    }
  }
  else
  {
   /*
    * Lookup name and optionally the instance...
    */

    match = cups_find_dest(name, instance, num_dests, dests, -1, &diff);

    if (!diff)
      return (dests + match);
  }

  return (NULL);
}


/*
 * '_cupsGetDestResource()' - Get the resource path and URI for a destination.
 */

const char *				/* O - Printer URI */
_cupsGetDestResource(
    cups_dest_t *dest,			/* I - Destination */
    char        *resource,		/* I - Resource buffer */
    size_t      resourcesize)		/* I - Size of resource buffer */
{
  const char	*uri;			/* Printer URI */
  char		scheme[32],		/* URI scheme */
		userpass[256],		/* Username and password (unused) */
		hostname[256];		/* Hostname */
  int		port;			/* Port number */


 /*
  * Range check input...
  */

  if (!dest || !resource || resourcesize < 1)
  {
    if (resource)
      *resource = '\0';

    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    return (NULL);
  }

 /*
  * Grab the printer URI...
  */

  if ((uri = cupsGetOption("printer-uri-supported", dest->num_options,
                           dest->options)) == NULL)
  {
    if (resource)
      *resource = '\0';

    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(ENOENT), 0);

    return (NULL);
  }

#ifdef HAVE_DNSSD
  if (strstr(uri, "._tcp"))
  {
    if ((uri = cups_dnssd_resolve(dest, uri, 5000, NULL, NULL, NULL)) == NULL)
      return (NULL);
  }
#endif /* HAVE_DNSSD */

  if (httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, sizeof(scheme),
                      userpass, sizeof(userpass), hostname, sizeof(hostname),
                      &port, resource, (int)resourcesize) < HTTP_URI_STATUS_OK)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad printer-uri."), 1);

    return (NULL);
  }

  return (uri);
}


/*
 * 'cupsGetDestWithURI()' - Get a destination associated with a URI.
 *
 * "name" is the desired name for the printer. If @code NULL@, a name will be
 * created using the URI.
 *
 * "uri" is the "ipp" or "ipps" URI for the printer.
 *
 * @since CUPS 2.0/OS X 10.10@
 */

cups_dest_t *				/* O - Destination or @code NULL@ */
cupsGetDestWithURI(const char *name,	/* I - Desired printer name or @code NULL@ */
                   const char *uri)	/* I - URI for the printer */
{
  cups_dest_t	*dest;			/* New destination */
  char		temp[1024],		/* Temporary string */
		scheme[256],		/* Scheme from URI */
		userpass[256],		/* Username:password from URI */
		hostname[256],		/* Hostname from URI */
		resource[1024],		/* Resource path from URI */
		*ptr;			/* Pointer into string */
  int		port;			/* Port number from URI */


 /*
  * Range check input...
  */

  if (!uri)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    return (NULL);
  }

  if (httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, sizeof(scheme), userpass, sizeof(userpass), hostname, sizeof(hostname), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK ||
      (strncmp(uri, "ipp://", 6) && strncmp(uri, "ipps://", 7)))
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad printer-uri."), 1);

    return (NULL);
  }

  if (!name)
  {
   /*
    * Create the name from the URI...
    */

    if (strstr(hostname, "._tcp"))
    {
     /*
      * Use the service instance name...
      */

      if ((ptr = strchr(hostname, '.')) != NULL)
        *ptr = '\0';

      name = hostname;
    }
    else if (!strncmp(resource, "/classes/", 9))
    {
      snprintf(temp, sizeof(temp), "%s @ %s", resource + 9, hostname);
      name = temp;
    }
    else if (!strncmp(resource, "/printers/", 10))
    {
      snprintf(temp, sizeof(temp), "%s @ %s", resource + 10, hostname);
      name = temp;
    }
    else
    {
      name = hostname;
    }
  }

 /*
  * Create the destination...
  */

  if ((dest = calloc(1, sizeof(cups_dest_t))) == NULL)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
    return (NULL);
  }

  dest->name        = _cupsStrAlloc(name);
  dest->num_options = cupsAddOption("printer-uri-supported", uri, dest->num_options, &(dest->options));
  dest->num_options = cupsAddOption("printer-info", name, dest->num_options, &(dest->options));

  return (dest);
}


/*
 * '_cupsGetDests()' - Get destinations from a server.
 *
 * "op" is IPP_OP_CUPS_GET_PRINTERS to get a full list, IPP_OP_CUPS_GET_DEFAULT
 * to get the system-wide default printer, or IPP_OP_GET_PRINTER_ATTRIBUTES for
 * a known printer.
 *
 * "name" is the name of an existing printer and is only used when "op" is
 * IPP_OP_GET_PRINTER_ATTRIBUTES.
 *
 * "dest" is initialized to point to the array of destinations.
 *
 * 0 is returned if there are no printers, no default printer, or the named
 * printer does not exist, respectively.
 *
 * Free the memory used by the destination array using the @link cupsFreeDests@
 * function.
 *
 * Note: On OS X this function also gets the default paper from the system
 * preferences (~/L/P/org.cups.PrintingPrefs.plist) and includes it in the
 * options array for each destination that supports it.
 */

int					/* O  - Number of destinations */
_cupsGetDests(http_t       *http,	/* I  - Connection to server or
					 *      @code CUPS_HTTP_DEFAULT@ */
	      ipp_op_t     op,		/* I  - IPP operation */
	      const char   *name,	/* I  - Name of destination */
	      cups_dest_t  **dests,	/* IO - Destinations */
	      cups_ptype_t type,	/* I  - Printer type bits */
	      cups_ptype_t mask)	/* I  - Printer type mask */
{
  int		num_dests = 0;		/* Number of destinations */
  cups_dest_t	*dest;			/* Current destination */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  const char	*printer_name;		/* printer-name attribute */
  char		uri[1024];		/* printer-uri value */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
#ifdef __APPLE__
  char		media_default[41];	/* Default paper size */
#endif /* __APPLE__ */
  char		optname[1024],		/* Option name */
		value[2048],		/* Option value */
		*ptr;			/* Pointer into name/value */
  static const char * const pattrs[] =	/* Attributes we're interested in */
		{
		  "auth-info-required",
		  "device-uri",
		  "job-sheets-default",
		  "marker-change-time",
		  "marker-colors",
		  "marker-high-levels",
		  "marker-levels",
		  "marker-low-levels",
		  "marker-message",
		  "marker-names",
		  "marker-types",
#ifdef __APPLE__
		  "media-supported",
#endif /* __APPLE__ */
		  "printer-commands",
		  "printer-defaults",
		  "printer-info",
		  "printer-is-accepting-jobs",
		  "printer-is-shared",
		  "printer-location",
		  "printer-make-and-model",
		  "printer-mandatory-job-attributes",
		  "printer-name",
		  "printer-state",
		  "printer-state-change-time",
		  "printer-state-reasons",
		  "printer-type",
		  "printer-uri-supported"
		};


#ifdef __APPLE__
 /*
  * Get the default paper size...
  */

  appleGetPaperSize(media_default, sizeof(media_default));
#endif /* __APPLE__ */

 /*
  * Build a IPP_OP_CUPS_GET_PRINTERS or IPP_OP_GET_PRINTER_ATTRIBUTES request, which
  * require the following attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requesting-user-name
  *    printer-uri [for IPP_OP_GET_PRINTER_ATTRIBUTES]
  */

  request = ippNewRequest(op);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(pattrs) / sizeof(pattrs[0]),
		NULL, pattrs);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, cupsUser());

  if (name && op != IPP_OP_CUPS_GET_DEFAULT)
  {
    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                     "localhost", ippPort(), "/printers/%s", name);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL,
                 uri);
  }
  else if (mask)
  {
    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_ENUM, "printer-type", (int)type);
    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_ENUM, "printer-type-mask", (int)mask);
  }

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a printer...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this printer...
      */

      printer_name = NULL;
      num_options  = 0;
      options      = NULL;

      for (; attr && attr->group_tag == IPP_TAG_PRINTER; attr = attr->next)
      {
	if (attr->value_tag != IPP_TAG_INTEGER &&
	    attr->value_tag != IPP_TAG_ENUM &&
	    attr->value_tag != IPP_TAG_BOOLEAN &&
	    attr->value_tag != IPP_TAG_TEXT &&
	    attr->value_tag != IPP_TAG_TEXTLANG &&
	    attr->value_tag != IPP_TAG_NAME &&
	    attr->value_tag != IPP_TAG_NAMELANG &&
	    attr->value_tag != IPP_TAG_KEYWORD &&
	    attr->value_tag != IPP_TAG_RANGE &&
	    attr->value_tag != IPP_TAG_URI)
          continue;

        if (!strcmp(attr->name, "auth-info-required") ||
	    !strcmp(attr->name, "device-uri") ||
	    !strcmp(attr->name, "marker-change-time") ||
	    !strcmp(attr->name, "marker-colors") ||
	    !strcmp(attr->name, "marker-high-levels") ||
	    !strcmp(attr->name, "marker-levels") ||
	    !strcmp(attr->name, "marker-low-levels") ||
	    !strcmp(attr->name, "marker-message") ||
	    !strcmp(attr->name, "marker-names") ||
	    !strcmp(attr->name, "marker-types") ||
	    !strcmp(attr->name, "printer-commands") ||
	    !strcmp(attr->name, "printer-info") ||
	    !strcmp(attr->name, "printer-is-shared") ||
	    !strcmp(attr->name, "printer-make-and-model") ||
	    !strcmp(attr->name, "printer-mandatory-job-attributes") ||
	    !strcmp(attr->name, "printer-state") ||
	    !strcmp(attr->name, "printer-state-change-time") ||
	    !strcmp(attr->name, "printer-type") ||
            !strcmp(attr->name, "printer-is-accepting-jobs") ||
            !strcmp(attr->name, "printer-location") ||
            !strcmp(attr->name, "printer-state-reasons") ||
	    !strcmp(attr->name, "printer-uri-supported"))
        {
	 /*
	  * Add a printer description attribute...
	  */

          num_options = cupsAddOption(attr->name,
	                              cups_make_string(attr, value,
				                       sizeof(value)),
				      num_options, &options);
	}
#ifdef __APPLE__
	else if (!strcmp(attr->name, "media-supported"))
	{
	 /*
	  * See if we can set a default media size...
	  */

          int	i;			/* Looping var */

	  for (i = 0; i < attr->num_values; i ++)
	    if (!_cups_strcasecmp(media_default, attr->values[i].string.text))
	    {
	      num_options = cupsAddOption("media", media_default, num_options,
	                                  &options);
              break;
	    }
	}
#endif /* __APPLE__ */
        else if (!strcmp(attr->name, "printer-name") &&
	         attr->value_tag == IPP_TAG_NAME)
	  printer_name = attr->values[0].string.text;
        else if (strncmp(attr->name, "notify-", 7) &&
	         (attr->value_tag == IPP_TAG_BOOLEAN ||
		  attr->value_tag == IPP_TAG_ENUM ||
		  attr->value_tag == IPP_TAG_INTEGER ||
		  attr->value_tag == IPP_TAG_KEYWORD ||
		  attr->value_tag == IPP_TAG_NAME ||
		  attr->value_tag == IPP_TAG_RANGE) &&
		 (ptr = strstr(attr->name, "-default")) != NULL)
	{
	 /*
	  * Add a default option...
	  */

          strlcpy(optname, attr->name, sizeof(optname));
	  optname[ptr - attr->name] = '\0';

	  if (_cups_strcasecmp(optname, "media") ||
	      !cupsGetOption("media", num_options, options))
	    num_options = cupsAddOption(optname,
					cups_make_string(attr, value,
							 sizeof(value)),
					num_options, &options);
	}
      }

     /*
      * See if we have everything needed...
      */

      if (!printer_name)
      {
        cupsFreeOptions(num_options, options);

        if (attr == NULL)
	  break;
	else
          continue;
      }

      if ((dest = cups_add_dest(printer_name, NULL, &num_dests, dests)) != NULL)
      {
        dest->num_options = num_options;
	dest->options     = options;
      }
      else
        cupsFreeOptions(num_options, options);

      if (attr == NULL)
	break;
    }

    ippDelete(response);
  }

 /*
  * Return the count...
  */

  return (num_dests);
}


/*
 * 'cupsGetDests()' - Get the list of destinations from the default server.
 *
 * Starting with CUPS 1.2, the returned list of destinations include the
 * printer-info, printer-is-accepting-jobs, printer-is-shared,
 * printer-make-and-model, printer-state, printer-state-change-time,
 * printer-state-reasons, and printer-type attributes as options.  CUPS 1.4
 * adds the marker-change-time, marker-colors, marker-high-levels,
 * marker-levels, marker-low-levels, marker-message, marker-names,
 * marker-types, and printer-commands attributes as well.
 *
 * Use the @link cupsFreeDests@ function to free the destination list and
 * the @link cupsGetDest@ function to find a particular destination.
 */

int					/* O - Number of destinations */
cupsGetDests(cups_dest_t **dests)	/* O - Destinations */
{
  return (cupsGetDests2(CUPS_HTTP_DEFAULT, dests));
}


/*
 * 'cupsGetDests2()' - Get the list of destinations from the specified server.
 *
 * Starting with CUPS 1.2, the returned list of destinations include the
 * printer-info, printer-is-accepting-jobs, printer-is-shared,
 * printer-make-and-model, printer-state, printer-state-change-time,
 * printer-state-reasons, and printer-type attributes as options.  CUPS 1.4
 * adds the marker-change-time, marker-colors, marker-high-levels,
 * marker-levels, marker-low-levels, marker-message, marker-names,
 * marker-types, and printer-commands attributes as well.
 *
 * Use the @link cupsFreeDests@ function to free the destination list and
 * the @link cupsGetDest@ function to find a particular destination.
 *
 * @since CUPS 1.1.21/OS X 10.4@
 */

int					/* O - Number of destinations */
cupsGetDests2(http_t      *http,	/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
              cups_dest_t **dests)	/* O - Destinations */
{
  int		num_dests;		/* Number of destinations */
  cups_dest_t	*dest;			/* Destination pointer */
  const char	*home;			/* HOME environment variable */
  char		filename[1024];		/* Local ~/.cups/lpoptions file */
  const char	*defprinter;		/* Default printer */
  char		name[1024],		/* Copy of printer name */
		*instance,		/* Pointer to instance name */
		*user_default;		/* User default printer */
  int		num_reals;		/* Number of real queues */
  cups_dest_t	*reals;			/* Real queues */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * Range check the input...
  */

  if (!dests)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad NULL dests pointer"), 1);
    return (0);
  }

 /*
  * Grab the printers and classes...
  */

  *dests    = (cups_dest_t *)0;
  num_dests = _cupsGetDests(http, IPP_OP_CUPS_GET_PRINTERS, NULL, dests, 0, CUPS_PRINTER_3D);

  if (cupsLastError() >= IPP_STATUS_REDIRECTION_OTHER_SITE)
  {
    cupsFreeDests(num_dests, *dests);
    *dests = (cups_dest_t *)0;
    return (0);
  }

 /*
  * Make a copy of the "real" queues for a later sanity check...
  */

  if (num_dests > 0)
  {
    num_reals = num_dests;
    reals     = calloc((size_t)num_reals, sizeof(cups_dest_t));

    if (reals)
      memcpy(reals, *dests, (size_t)num_reals * sizeof(cups_dest_t));
    else
      num_reals = 0;
  }
  else
  {
    num_reals = 0;
    reals     = NULL;
  }

 /*
  * Grab the default destination...
  */

  if ((user_default = _cupsUserDefault(name, sizeof(name))) != NULL)
    defprinter = name;
  else if ((defprinter = cupsGetDefault2(http)) != NULL)
  {
    strlcpy(name, defprinter, sizeof(name));
    defprinter = name;
  }

  if (defprinter)
  {
   /*
    * Separate printer and instance name...
    */

    if ((instance = strchr(name, '/')) != NULL)
      *instance++ = '\0';

   /*
    * Lookup the printer and instance and make it the default...
    */

    if ((dest = cupsGetDest(name, instance, num_dests, *dests)) != NULL)
      dest->is_default = 1;
  }
  else
    instance = NULL;

 /*
  * Load the /etc/cups/lpoptions and ~/.cups/lpoptions files...
  */

  snprintf(filename, sizeof(filename), "%s/lpoptions", cg->cups_serverroot);
  num_dests = cups_get_dests(filename, NULL, NULL, user_default != NULL,
                             num_dests, dests);

  if ((home = getenv("HOME")) != NULL)
  {
    snprintf(filename, sizeof(filename), "%s/.cups/lpoptions", home);

    num_dests = cups_get_dests(filename, NULL, NULL, user_default != NULL,
                               num_dests, dests);
  }

 /*
  * Validate the current default destination - this prevents old
  * Default lines in /etc/cups/lpoptions and ~/.cups/lpoptions from
  * pointing to a non-existent printer or class...
  */

  if (num_reals)
  {
   /*
    * See if we have a default printer...
    */

    if ((dest = cupsGetDest(NULL, NULL, num_dests, *dests)) != NULL)
    {
     /*
      * Have a default; see if it is real...
      */

      if (!cupsGetDest(dest->name, NULL, num_reals, reals))
      {
       /*
        * Remove the non-real printer from the list, since we don't want jobs
        * going to an unexpected printer... (<rdar://problem/14216472>)
        */

        num_dests = cupsRemoveDest(dest->name, dest->instance, num_dests,
                                   dests);
      }
    }

   /*
    * Free memory...
    */

    free(reals);
  }

 /*
  * Return the number of destinations...
  */

  if (num_dests > 0)
    _cupsSetError(IPP_STATUS_OK, NULL, 0);

  return (num_dests);
}


/*
 * 'cupsGetNamedDest()' - Get options for the named destination.
 *
 * This function is optimized for retrieving a single destination and should
 * be used instead of @link cupsGetDests@ and @link cupsGetDest@ when you either
 * know the name of the destination or want to print to the default destination.
 * If @code NULL@ is returned, the destination does not exist or there is no
 * default destination.
 *
 * If "http" is @code CUPS_HTTP_DEFAULT@, the connection to the default print
 * server will be used.
 *
 * If "name" is @code NULL@, the default printer for the current user will be
 * returned.
 *
 * The returned destination must be freed using @link cupsFreeDests@ with a
 * "num_dests" value of 1.
 *
 * @since CUPS 1.4/OS X 10.6@
 */

cups_dest_t *				/* O - Destination or @code NULL@ */
cupsGetNamedDest(http_t     *http,	/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
                 const char *name,	/* I - Destination name or @code NULL@ for the default destination */
                 const char *instance)	/* I - Instance name or @code NULL@ */
{
  cups_dest_t	*dest;			/* Destination */
  char		filename[1024],		/* Path to lpoptions */
		defname[256];		/* Default printer name */
  const char	*home = getenv("HOME");	/* Home directory */
  int		set_as_default = 0;	/* Set returned destination as default */
  ipp_op_t	op = IPP_OP_GET_PRINTER_ATTRIBUTES;
					/* IPP operation to get server ops */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * If "name" is NULL, find the default destination...
  */

  if (!name)
  {
    set_as_default = 1;
    name           = _cupsUserDefault(defname, sizeof(defname));

    if (name)
    {
      char	*ptr;			/* Temporary pointer... */

      if ((ptr = strchr(defname, '/')) != NULL)
      {
        *ptr++   = '\0';
	instance = ptr;
      }
      else
        instance = NULL;
    }
    else if (home)
    {
     /*
      * No default in the environment, try the user's lpoptions files...
      */

      snprintf(filename, sizeof(filename), "%s/.cups/lpoptions", home);

      name = cups_get_default(filename, defname, sizeof(defname), &instance);
    }

    if (!name)
    {
     /*
      * Still not there?  Try the system lpoptions file...
      */

      snprintf(filename, sizeof(filename), "%s/lpoptions",
	       cg->cups_serverroot);
      name = cups_get_default(filename, defname, sizeof(defname), &instance);
    }

    if (!name)
    {
     /*
      * No locally-set default destination, ask the server...
      */

      op = IPP_OP_CUPS_GET_DEFAULT;
    }
  }

 /*
  * Get the printer's attributes...
  */

  if (!_cupsGetDests(http, op, name, &dest, 0, CUPS_PRINTER_3D))
    return (NULL);

  if (instance)
    dest->instance = _cupsStrAlloc(instance);

  if (set_as_default)
    dest->is_default = 1;

 /*
  * Then add local options...
  */

  snprintf(filename, sizeof(filename), "%s/lpoptions", cg->cups_serverroot);
  cups_get_dests(filename, name, instance, 1, 1, &dest);

  if (home)
  {
    snprintf(filename, sizeof(filename), "%s/.cups/lpoptions", home);

    cups_get_dests(filename, name, instance, 1, 1, &dest);
  }

 /*
  * Return the result...
  */

  return (dest);
}


/*
 * 'cupsRemoveDest()' - Remove a destination from the destination list.
 *
 * Removing a destination/instance does not delete the class or printer
 * queue, merely the lpoptions for that destination/instance.  Use the
 * @link cupsSetDests@ or @link cupsSetDests2@ functions to save the new
 * options for the user.
 *
 * @since CUPS 1.3/OS X 10.5@
 */

int					/* O  - New number of destinations */
cupsRemoveDest(const char  *name,	/* I  - Destination name */
               const char  *instance,	/* I  - Instance name or @code NULL@ */
	       int         num_dests,	/* I  - Number of destinations */
	       cups_dest_t **dests)	/* IO - Destinations */
{
  int		i;			/* Index into destinations */
  cups_dest_t	*dest;			/* Pointer to destination */


 /*
  * Find the destination...
  */

  if ((dest = cupsGetDest(name, instance, num_dests, *dests)) == NULL)
    return (num_dests);

 /*
  * Free memory...
  */

  _cupsStrFree(dest->name);
  _cupsStrFree(dest->instance);
  cupsFreeOptions(dest->num_options, dest->options);

 /*
  * Remove the destination from the array...
  */

  num_dests --;

  i = (int)(dest - *dests);

  if (i < num_dests)
    memmove(dest, dest + 1, (size_t)(num_dests - i) * sizeof(cups_dest_t));

  return (num_dests);
}


/*
 * 'cupsSetDefaultDest()' - Set the default destination.
 *
 * @since CUPS 1.3/OS X 10.5@
 */

void
cupsSetDefaultDest(
    const char  *name,			/* I - Destination name */
    const char  *instance,		/* I - Instance name or @code NULL@ */
    int         num_dests,		/* I - Number of destinations */
    cups_dest_t *dests)			/* I - Destinations */
{
  int		i;			/* Looping var */
  cups_dest_t	*dest;			/* Current destination */


 /*
  * Range check input...
  */

  if (!name || num_dests <= 0 || !dests)
    return;

 /*
  * Loop through the array and set the "is_default" flag for the matching
  * destination...
  */

  for (i = num_dests, dest = dests; i > 0; i --, dest ++)
    dest->is_default = !_cups_strcasecmp(name, dest->name) &&
                       ((!instance && !dest->instance) ||
		        (instance && dest->instance &&
			 !_cups_strcasecmp(instance, dest->instance)));
}


/*
 * 'cupsSetDests()' - Save the list of destinations for the default server.
 *
 * This function saves the destinations to /etc/cups/lpoptions when run
 * as root and ~/.cups/lpoptions when run as a normal user.
 */

void
cupsSetDests(int         num_dests,	/* I - Number of destinations */
             cups_dest_t *dests)	/* I - Destinations */
{
  cupsSetDests2(CUPS_HTTP_DEFAULT, num_dests, dests);
}


/*
 * 'cupsSetDests2()' - Save the list of destinations for the specified server.
 *
 * This function saves the destinations to /etc/cups/lpoptions when run
 * as root and ~/.cups/lpoptions when run as a normal user.
 *
 * @since CUPS 1.1.21/OS X 10.4@
 */

int					/* O - 0 on success, -1 on error */
cupsSetDests2(http_t      *http,	/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
              int         num_dests,	/* I - Number of destinations */
              cups_dest_t *dests)	/* I - Destinations */
{
  int		i, j;			/* Looping vars */
  int		wrote;			/* Wrote definition? */
  cups_dest_t	*dest;			/* Current destination */
  cups_option_t	*option;		/* Current option */
  _ipp_option_t	*match;			/* Matching attribute for option */
  FILE		*fp;			/* File pointer */
#ifndef WIN32
  const char	*home;			/* HOME environment variable */
#endif /* WIN32 */
  char		filename[1024];		/* lpoptions file */
  int		num_temps;		/* Number of temporary destinations */
  cups_dest_t	*temps = NULL,		/* Temporary destinations */
		*temp;			/* Current temporary dest */
  const char	*val;			/* Value of temporary option */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * Range check the input...
  */

  if (!num_dests || !dests)
    return (-1);

 /*
  * Get the server destinations...
  */

  num_temps = _cupsGetDests(http, IPP_OP_CUPS_GET_PRINTERS, NULL, &temps, 0, CUPS_PRINTER_3D);

  if (cupsLastError() >= IPP_STATUS_REDIRECTION_OTHER_SITE)
  {
    cupsFreeDests(num_temps, temps);
    return (-1);
  }

 /*
  * Figure out which file to write to...
  */

  snprintf(filename, sizeof(filename), "%s/lpoptions", cg->cups_serverroot);

#ifndef WIN32
  if (getuid())
  {
   /*
    * Merge in server defaults...
    */

    num_temps = cups_get_dests(filename, NULL, NULL, 0, num_temps, &temps);

   /*
    * Point to user defaults...
    */

    if ((home = getenv("HOME")) != NULL)
    {
     /*
      * Create ~/.cups subdirectory...
      */

      snprintf(filename, sizeof(filename), "%s/.cups", home);
      if (access(filename, 0))
        mkdir(filename, 0700);

      snprintf(filename, sizeof(filename), "%s/.cups/lpoptions", home);
    }
  }
#endif /* !WIN32 */

 /*
  * Try to open the file...
  */

  if ((fp = fopen(filename, "w")) == NULL)
  {
    cupsFreeDests(num_temps, temps);
    return (-1);
  }

#ifndef WIN32
 /*
  * Set the permissions to 0644 when saving to the /etc/cups/lpoptions
  * file...
  */

  if (!getuid())
    fchmod(fileno(fp), 0644);
#endif /* !WIN32 */

 /*
  * Write each printer; each line looks like:
  *
  *    Dest name[/instance] options
  *    Default name[/instance] options
  */

  for (i = num_dests, dest = dests; i > 0; i --, dest ++)
    if (dest->instance != NULL || dest->num_options != 0 || dest->is_default)
    {
      if (dest->is_default)
      {
	fprintf(fp, "Default %s", dest->name);
	if (dest->instance)
	  fprintf(fp, "/%s", dest->instance);

        wrote = 1;
      }
      else
        wrote = 0;

      if ((temp = cupsGetDest(dest->name, dest->instance, num_temps, temps)) == NULL)
        temp = cupsGetDest(dest->name, NULL, num_temps, temps);

      for (j = dest->num_options, option = dest->options; j > 0; j --, option ++)
      {
       /*
        * See if this option is a printer attribute; if so, skip it...
	*/

        if ((match = _ippFindOption(option->name)) != NULL &&
	    match->group_tag == IPP_TAG_PRINTER)
	  continue;

       /*
	* See if the server/global options match these; if so, don't
	* write 'em.
	*/

        if (temp &&
	    (val = cupsGetOption(option->name, temp->num_options,
	                         temp->options)) != NULL &&
            !_cups_strcasecmp(val, option->value))
	  continue;

       /*
        * Options don't match, write to the file...
	*/

        if (!wrote)
	{
	  fprintf(fp, "Dest %s", dest->name);
	  if (dest->instance)
	    fprintf(fp, "/%s", dest->instance);
          wrote = 1;
	}

        if (option->value[0])
	{
	  if (strchr(option->value, ' ') ||
	      strchr(option->value, '\\') ||
	      strchr(option->value, '\"') ||
	      strchr(option->value, '\''))
	  {
	   /*
	    * Quote the value...
	    */

	    fprintf(fp, " %s=\"", option->name);

	    for (val = option->value; *val; val ++)
	    {
	      if (strchr("\"\'\\", *val))
	        putc('\\', fp);

              putc(*val, fp);
	    }

	    putc('\"', fp);
          }
	  else
	  {
	   /*
	    * Store the literal value...
	    */

	    fprintf(fp, " %s=%s", option->name, option->value);
          }
	}
	else
	  fprintf(fp, " %s", option->name);
      }

      if (wrote)
        fputs("\n", fp);
    }

 /*
  * Free the temporary destinations and close the file...
  */

  cupsFreeDests(num_temps, temps);

  fclose(fp);

#ifdef __APPLE__
 /*
  * Set the default printer for this location - this allows command-line
  * and GUI applications to share the same default destination...
  */

  if ((dest = cupsGetDest(NULL, NULL, num_dests, dests)) != NULL)
  {
    CFStringRef name = CFStringCreateWithCString(kCFAllocatorDefault,
                                                 dest->name,
                                                 kCFStringEncodingUTF8);
					/* Default printer name */

    if (name)
    {
      _cupsAppleSetDefaultPrinter(name);
      CFRelease(name);
    }
  }
#endif /* __APPLE__ */

#ifdef HAVE_NOTIFY_POST
 /*
  * Send a notification so that MacOS X applications can know about the
  * change, too.
  */

  notify_post("com.apple.printerListChange");
#endif /* HAVE_NOTIFY_POST */

  return (0);
}


/*
 * '_cupsUserDefault()' - Get the user default printer from environment
 *                        variables and location information.
 */

char *					/* O - Default printer or NULL */
_cupsUserDefault(char   *name,		/* I - Name buffer */
                 size_t namesize)	/* I - Size of name buffer */
{
  const char	*env;			/* LPDEST or PRINTER env variable */
#ifdef __APPLE__
  CFStringRef	locprinter;		/* Last printer as this location */
#endif /* __APPLE__ */


  if ((env = getenv("LPDEST")) == NULL)
    if ((env = getenv("PRINTER")) != NULL && !strcmp(env, "lp"))
      env = NULL;

  if (env)
  {
    strlcpy(name, env, namesize);
    return (name);
  }

#ifdef __APPLE__
 /*
  * Use location-based defaults if "use last printer" is selected in the
  * system preferences...
  */

  if ((locprinter = _cupsAppleCopyDefaultPrinter()) != NULL)
  {
    CFStringGetCString(locprinter, name, (CFIndex)namesize, kCFStringEncodingUTF8);
    CFRelease(locprinter);
  }
  else
    name[0] = '\0';

  DEBUG_printf(("1_cupsUserDefault: Returning \"%s\".", name));

  return (*name ? name : NULL);

#else
 /*
  * No location-based defaults on this platform...
  */

  name[0] = '\0';
  return (NULL);
#endif /* __APPLE__ */
}


#ifdef __APPLE__
/*
 * 'appleCopyLocations()' - Copy the location history array.
 */

static CFArrayRef			/* O - Location array or NULL */
appleCopyLocations(void)
{
  CFArrayRef	locations;		/* Location array */


 /*
  * Look up the location array in the preferences...
  */

  if ((locations = CFPreferencesCopyAppValue(kLastUsedPrintersKey,
                                             kCUPSPrintingPrefs)) == NULL)
    return (NULL);

  if (CFGetTypeID(locations) != CFArrayGetTypeID())
  {
    CFRelease(locations);
    return (NULL);
  }

  return (locations);
}


/*
 * 'appleCopyNetwork()' - Get the network ID for the current location.
 */

static CFStringRef			/* O - Network ID */
appleCopyNetwork(void)
{
  SCDynamicStoreRef	dynamicStore;	/* System configuration data */
  CFStringRef		key;		/* Current network configuration key */
  CFDictionaryRef	ip_dict;	/* Network configuration data */
  CFStringRef		network = NULL;	/* Current network ID */


  if ((dynamicStore = SCDynamicStoreCreate(NULL, CFSTR("libcups"), NULL,
                                           NULL)) != NULL)
  {
   /*
    * First use the IPv6 router address, if available, since that will generally
    * be a globally-unique link-local address.
    */

    if ((key = SCDynamicStoreKeyCreateNetworkGlobalEntity(
                   NULL, kSCDynamicStoreDomainState, kSCEntNetIPv6)) != NULL)
    {
      if ((ip_dict = SCDynamicStoreCopyValue(dynamicStore, key)) != NULL)
      {
	if ((network = CFDictionaryGetValue(ip_dict,
	                                    kSCPropNetIPv6Router)) != NULL)
          CFRetain(network);

        CFRelease(ip_dict);
      }

      CFRelease(key);
    }

   /*
    * If that doesn't work, try the IPv4 router address. This isn't as unique
    * and will likely be a 10.x.y.z or 192.168.y.z address...
    */

    if (!network)
    {
      if ((key = SCDynamicStoreKeyCreateNetworkGlobalEntity(
		     NULL, kSCDynamicStoreDomainState, kSCEntNetIPv4)) != NULL)
      {
	if ((ip_dict = SCDynamicStoreCopyValue(dynamicStore, key)) != NULL)
	{
	  if ((network = CFDictionaryGetValue(ip_dict,
					      kSCPropNetIPv4Router)) != NULL)
	    CFRetain(network);

	  CFRelease(ip_dict);
	}

	CFRelease(key);
      }
    }

    CFRelease(dynamicStore);
  }

  return (network);
}


/*
 * 'appleGetPaperSize()' - Get the default paper size.
 */

static char *				/* O - Default paper size */
appleGetPaperSize(char   *name,		/* I - Paper size name buffer */
                  size_t namesize)	/* I - Size of buffer */
{
  CFStringRef	defaultPaperID;		/* Default paper ID */
  pwg_media_t	*pwgmedia;		/* PWG media size */


  defaultPaperID = _cupsAppleCopyDefaultPaperID();
  if (!defaultPaperID ||
      CFGetTypeID(defaultPaperID) != CFStringGetTypeID() ||
      !CFStringGetCString(defaultPaperID, name, (CFIndex)namesize, kCFStringEncodingUTF8))
    name[0] = '\0';
  else if ((pwgmedia = pwgMediaForLegacy(name)) != NULL)
    strlcpy(name, pwgmedia->pwg, namesize);

  if (defaultPaperID)
    CFRelease(defaultPaperID);

  return (name);
}


/*
 * 'appleGetPrinter()' - Get a printer from the history array.
 */

static CFStringRef			/* O - Printer name or NULL */
appleGetPrinter(CFArrayRef  locations,	/* I - Location array */
                CFStringRef network,	/* I - Network name */
		CFIndex     *locindex)	/* O - Index in array */
{
  CFIndex		i,		/* Looping var */
			count;		/* Number of locations */
  CFDictionaryRef	location;	/* Current location */
  CFStringRef		locnetwork,	/* Current network */
			locprinter;	/* Current printer */


  for (i = 0, count = CFArrayGetCount(locations); i < count; i ++)
    if ((location = CFArrayGetValueAtIndex(locations, i)) != NULL &&
        CFGetTypeID(location) == CFDictionaryGetTypeID())
    {
      if ((locnetwork = CFDictionaryGetValue(location,
                                             kLocationNetworkKey)) != NULL &&
          CFGetTypeID(locnetwork) == CFStringGetTypeID() &&
	  CFStringCompare(network, locnetwork, 0) == kCFCompareEqualTo &&
	  (locprinter = CFDictionaryGetValue(location,
	                                     kLocationPrinterIDKey)) != NULL &&
	  CFGetTypeID(locprinter) == CFStringGetTypeID())
      {
        if (locindex)
	  *locindex = i;

	return (locprinter);
      }
    }

  return (NULL);
}
#endif /* __APPLE__ */


/*
 * 'cups_add_dest()' - Add a destination to the array.
 *
 * Unlike cupsAddDest(), this function does not check for duplicates.
 */

static cups_dest_t *			/* O  - New destination */
cups_add_dest(const char  *name,	/* I  - Name of destination */
              const char  *instance,	/* I  - Instance or NULL */
              int         *num_dests,	/* IO - Number of destinations */
	      cups_dest_t **dests)	/* IO - Destinations */
{
  int		insert,			/* Insertion point */
		diff;			/* Result of comparison */
  cups_dest_t	*dest;			/* Destination pointer */


 /*
  * Add new destination...
  */

  if (*num_dests == 0)
    dest = malloc(sizeof(cups_dest_t));
  else
    dest = realloc(*dests, sizeof(cups_dest_t) * (size_t)(*num_dests + 1));

  if (!dest)
    return (NULL);

  *dests = dest;

 /*
  * Find where to insert the destination...
  */

  if (*num_dests == 0)
    insert = 0;
  else
  {
    insert = cups_find_dest(name, instance, *num_dests, *dests, *num_dests - 1,
                            &diff);

    if (diff > 0)
      insert ++;
  }

 /*
  * Move the array elements as needed...
  */

  if (insert < *num_dests)
    memmove(*dests + insert + 1, *dests + insert, (size_t)(*num_dests - insert) * sizeof(cups_dest_t));

  (*num_dests) ++;

 /*
  * Initialize the destination...
  */

  dest              = *dests + insert;
  dest->name        = _cupsStrAlloc(name);
  dest->instance    = _cupsStrAlloc(instance);
  dest->is_default  = 0;
  dest->num_options = 0;
  dest->options     = (cups_option_t *)0;

  return (dest);
}


#  ifdef __BLOCKS__
/*
 * 'cups_block_cb()' - Enumeration callback for block API.
 */

static int				/* O - 1 to continue, 0 to stop */
cups_block_cb(
    cups_dest_block_t block,		/* I - Block */
    unsigned          flags,		/* I - Destination flags */
    cups_dest_t       *dest)		/* I - Destination */
{
  return ((block)(flags, dest));
}
#  endif /* __BLOCKS__ */


/*
 * 'cups_compare_dests()' - Compare two destinations.
 */

static int				/* O - Result of comparison */
cups_compare_dests(cups_dest_t *a,	/* I - First destination */
                   cups_dest_t *b)	/* I - Second destination */
{
  int	diff;				/* Difference */


  if ((diff = _cups_strcasecmp(a->name, b->name)) != 0)
    return (diff);
  else if (a->instance && b->instance)
    return (_cups_strcasecmp(a->instance, b->instance));
  else
    return ((a->instance && !b->instance) - (!a->instance && b->instance));
}


#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
#  ifdef HAVE_DNSSD
/*
 * 'cups_dnssd_browse_cb()' - Browse for printers.
 */

static void
cups_dnssd_browse_cb(
    DNSServiceRef       sdRef,		/* I - Service reference */
    DNSServiceFlags     flags,		/* I - Option flags */
    uint32_t            interfaceIndex,	/* I - Interface number */
    DNSServiceErrorType errorCode,	/* I - Error, if any */
    const char          *serviceName,	/* I - Name of service/device */
    const char          *regtype,	/* I - Type of service */
    const char          *replyDomain,	/* I - Service domain */
    void                *context)	/* I - Enumeration data */
{
  _cups_dnssd_data_t	*data = (_cups_dnssd_data_t *)context;
					/* Enumeration data */


  DEBUG_printf(("5cups_dnssd_browse_cb(sdRef=%p, flags=%x, "
		"interfaceIndex=%d, errorCode=%d, serviceName=\"%s\", "
		"regtype=\"%s\", replyDomain=\"%s\", context=%p)",
		sdRef, flags, interfaceIndex, errorCode, serviceName, regtype,
		replyDomain, context));

 /*
  * Don't do anything on error...
  */

  if (errorCode != kDNSServiceErr_NoError)
    return;

 /*
  * Get the device...
  */

  cups_dnssd_get_device(data, serviceName, regtype, replyDomain);
}


#  else /* HAVE_AVAHI */
/*
 * 'cups_dnssd_browse_cb()' - Browse for printers.
 */

static void
cups_dnssd_browse_cb(
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
#ifdef DEBUG
  AvahiClient		*client = avahi_service_browser_get_client(browser);
					/* Client information */
#endif /* DEBUG */
  _cups_dnssd_data_t	*data = (_cups_dnssd_data_t *)context;
					/* Enumeration data */


  (void)interface;
  (void)protocol;
  (void)context;

  switch (event)
  {
    case AVAHI_BROWSER_FAILURE:
	DEBUG_printf(("cups_dnssd_browse_cb: %s",
		      avahi_strerror(avahi_client_errno(client))));
	avahi_simple_poll_quit(data->simple_poll);
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

	  DEBUG_printf(("cups_dnssd_browse_cb: Ignoring local service \"%s\".",
	                name));
	}
	else
	{
	 /*
	  * Create a device entry for it if it doesn't yet exist.
	  */

	  cups_dnssd_get_device(data, name, type, domain);
	}
	break;

    case AVAHI_BROWSER_REMOVE:
    case AVAHI_BROWSER_ALL_FOR_NOW:
    case AVAHI_BROWSER_CACHE_EXHAUSTED:
        break;
  }
}


/*
 * 'cups_dnssd_client_cb()' - Avahi client callback function.
 */

static void
cups_dnssd_client_cb(
    AvahiClient      *client,		/* I - Client information (unused) */
    AvahiClientState state,		/* I - Current state */
    void             *context)		/* I - User data (unused) */
{
  _cups_dnssd_data_t	*data = (_cups_dnssd_data_t *)context;
					/* Enumeration data */


  (void)client;

 /*
  * If the connection drops, quit.
  */

  if (state == AVAHI_CLIENT_FAILURE)
  {
    DEBUG_puts("cups_dnssd_client_cb: Avahi connection failed.");
    avahi_simple_poll_quit(data->simple_poll);
  }
}
#  endif /* HAVE_DNSSD */


/*
 * 'cups_dnssd_compare_device()' - Compare two devices.
 */

static int				/* O - Result of comparison */
cups_dnssd_compare_devices(
    _cups_dnssd_device_t *a,		/* I - First device */
    _cups_dnssd_device_t *b)		/* I - Second device */
{
  return (strcmp(a->dest.name, b->dest.name));
}


/*
 * 'cups_dnssd_free_device()' - Free the memory used by a device.
 */

static void
cups_dnssd_free_device(
    _cups_dnssd_device_t *device,	/* I - Device */
    _cups_dnssd_data_t   *data)		/* I - Enumeration data */
{
  DEBUG_printf(("5cups_dnssd_free_device(device=%p(%s), data=%p)", device,
                device->dest.name, data));

#  ifdef HAVE_DNSSD
  if (device->ref)
    DNSServiceRefDeallocate(device->ref);
#  else /* HAVE_AVAHI */
  if (device->ref)
    avahi_record_browser_free(device->ref);
#  endif /* HAVE_DNSSD */

  _cupsStrFree(device->domain);
  _cupsStrFree(device->fullName);
  _cupsStrFree(device->regtype);
  _cupsStrFree(device->dest.name);

  cupsFreeOptions(device->dest.num_options, device->dest.options);

  free(device);
}


/*
 * 'cups_dnssd_get_device()' - Lookup a device and create it as needed.
 */

static _cups_dnssd_device_t *		/* O - Device */
cups_dnssd_get_device(
    _cups_dnssd_data_t *data,		/* I - Enumeration data */
    const char         *serviceName,	/* I - Service name */
    const char         *regtype,	/* I - Registration type */
    const char         *replyDomain)	/* I - Domain name */
{
  _cups_dnssd_device_t	key,		/* Search key */
			*device;	/* Device */
  char			fullName[kDNSServiceMaxDomainName];
					/* Full name for query */


  DEBUG_printf(("5cups_dnssd_get_device(data=%p, serviceName=\"%s\", "
                "regtype=\"%s\", replyDomain=\"%s\")", data, serviceName,
                regtype, replyDomain));

 /*
  * See if this is an existing device...
  */

  key.dest.name = (char *)serviceName;

  if ((device = cupsArrayFind(data->devices, &key)) != NULL)
  {
   /*
    * Yes, see if we need to do anything with this...
    */

    int	update = 0;			/* Non-zero if we need to update */

    if (!_cups_strcasecmp(replyDomain, "local.") &&
	_cups_strcasecmp(device->domain, replyDomain))
    {
     /*
      * Update the "global" listing to use the .local domain name instead.
      */

      _cupsStrFree(device->domain);
      device->domain = _cupsStrAlloc(replyDomain);

      DEBUG_printf(("6cups_dnssd_get_device: Updating '%s' to use local "
                    "domain.", device->dest.name));

      update = 1;
    }

    if (!_cups_strcasecmp(regtype, "_ipps._tcp") &&
	_cups_strcasecmp(device->regtype, regtype))
    {
     /*
      * Prefer IPPS over IPP.
      */

      _cupsStrFree(device->regtype);
      device->regtype = _cupsStrAlloc(regtype);

      DEBUG_printf(("6cups_dnssd_get_device: Updating '%s' to use IPPS.",
		    device->dest.name));

      update = 1;
    }

    if (!update)
    {
      DEBUG_printf(("6cups_dnssd_get_device: No changes to '%s'.",
                    device->dest.name));
      return (device);
    }
  }
  else
  {
   /*
    * No, add the device...
    */

    DEBUG_printf(("6cups_dnssd_get_device: Adding '%s' for %s with domain "
                  "'%s'.", serviceName,
                  !strcmp(regtype, "_ipps._tcp") ? "IPPS" : "IPP",
                  replyDomain));

    device            = calloc(sizeof(_cups_dnssd_device_t), 1);
    device->dest.name = _cupsStrAlloc(serviceName);
    device->domain    = _cupsStrAlloc(replyDomain);
    device->regtype   = _cupsStrAlloc(regtype);

    cupsArrayAdd(data->devices, device);
  }

 /*
  * Set the "full name" of this service, which is used for queries...
  */

#  ifdef HAVE_DNSSD
  DNSServiceConstructFullName(fullName, device->dest.name, device->regtype,
			      device->domain);
#  else /* HAVE_AVAHI */
  avahi_service_name_join(fullName, kDNSServiceMaxDomainName, serviceName,
                          regtype, replyDomain);
#  endif /* HAVE_DNSSD */

  _cupsStrFree(device->fullName);
  device->fullName = _cupsStrAlloc(fullName);

  if (device->ref)
  {
#  ifdef HAVE_DNSSD
    DNSServiceRefDeallocate(device->ref);
#  else /* HAVE_AVAHI */
    avahi_record_browser_free(device->ref);
#  endif /* HAVE_DNSSD */

    device->ref = 0;
  }

  if (device->state == _CUPS_DNSSD_ACTIVE)
  {
    (*data->cb)(data->user_data, CUPS_DEST_FLAGS_REMOVED, &device->dest);
    device->state = _CUPS_DNSSD_NEW;
  }

  return (device);
}


#  ifdef HAVE_DNSSD
/*
 * 'cups_dnssd_local_cb()' - Browse for local printers.
 */

static void
cups_dnssd_local_cb(
    DNSServiceRef       sdRef,		/* I - Service reference */
    DNSServiceFlags     flags,		/* I - Option flags */
    uint32_t            interfaceIndex,	/* I - Interface number */
    DNSServiceErrorType errorCode,	/* I - Error, if any */
    const char          *serviceName,	/* I - Name of service/device */
    const char          *regtype,	/* I - Type of service */
    const char          *replyDomain,	/* I - Service domain */
    void                *context)	/* I - Devices array */
{
  _cups_dnssd_data_t	*data = (_cups_dnssd_data_t *)context;
					/* Enumeration data */
  _cups_dnssd_device_t	*device;	/* Device */


  DEBUG_printf(("5cups_dnssd_local_cb(sdRef=%p, flags=%x, "
		"interfaceIndex=%d, errorCode=%d, serviceName=\"%s\", "
		"regtype=\"%s\", replyDomain=\"%s\", context=%p)",
		sdRef, flags, interfaceIndex, errorCode, serviceName,
		regtype, replyDomain, context));

 /*
  * Only process "add" data...
  */

  if (errorCode != kDNSServiceErr_NoError || !(flags & kDNSServiceFlagsAdd))
    return;

 /*
  * Get the device...
  */

  device = cups_dnssd_get_device(data, serviceName, regtype, replyDomain);

 /*
  * Hide locally-registered devices...
  */

  DEBUG_printf(("6cups_dnssd_local_cb: Hiding local printer '%s'.",
                serviceName));

  if (device->ref)
  {
    DNSServiceRefDeallocate(device->ref);
    device->ref = 0;
  }

  if (device->state == _CUPS_DNSSD_ACTIVE)
    (*data->cb)(data->user_data, CUPS_DEST_FLAGS_REMOVED, &device->dest);

  device->state = _CUPS_DNSSD_LOCAL;
}
#  endif /* HAVE_DNSSD */


#  ifdef HAVE_AVAHI
/*
 * 'cups_dnssd_poll_cb()' - Wait for input on the specified file descriptors.
 *
 * Note: This function is needed because avahi_simple_poll_iterate is broken
 *       and always uses a timeout of 0 (!) milliseconds.
 *       (Avahi Ticket #364)
 */

static int				/* O - Number of file descriptors matching */
cups_dnssd_poll_cb(
    struct pollfd *pollfds,		/* I - File descriptors */
    unsigned int  num_pollfds,		/* I - Number of file descriptors */
    int           timeout,		/* I - Timeout in milliseconds (unused) */
    void          *context)		/* I - User data (unused) */
{
  _cups_dnssd_data_t	*data = (_cups_dnssd_data_t *)context;
					/* Enumeration data */
  int			val;		/* Return value */


  (void)timeout;

  val = poll(pollfds, num_pollfds, 250);

  if (val < 0)
  {
    DEBUG_printf(("cups_dnssd_poll_cb: %s", strerror(errno)));
  }
  else if (val > 0)
    data->got_data = 1;

  return (val);
}
#  endif /* HAVE_AVAHI */


/*
 * 'cups_dnssd_query_cb()' - Process query data.
 */

#  ifdef HAVE_DNSSD
static void
cups_dnssd_query_cb(
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
    void                *context)	/* I - Enumeration data */
{
#  else /* HAVE_AVAHI */
static void
cups_dnssd_query_cb(
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
    void                   *context)	/* I - Enumeration data */
{
#    ifdef DEBUG
  AvahiClient		*client = avahi_record_browser_get_client(browser);
					/* Client information */
#    endif /* DEBUG */
#  endif /* HAVE_DNSSD */
  _cups_dnssd_data_t	*data = (_cups_dnssd_data_t *)context;
					/* Enumeration data */
  char			name[1024],	/* Service name */
			*ptr;		/* Pointer into string */
  _cups_dnssd_device_t	dkey,		/* Search key */
			*device;	/* Device */


#  ifdef HAVE_DNSSD
  DEBUG_printf(("5cups_dnssd_query_cb(sdRef=%p, flags=%x, "
		"interfaceIndex=%d, errorCode=%d, fullName=\"%s\", "
		"rrtype=%u, rrclass=%u, rdlen=%u, rdata=%p, ttl=%u, "
		"context=%p)", sdRef, flags, interfaceIndex, errorCode,
		fullName, rrtype, rrclass, rdlen, rdata, ttl, context));

 /*
  * Only process "add" data...
  */

  if (errorCode != kDNSServiceErr_NoError || !(flags & kDNSServiceFlagsAdd))
    return;

#  else /* HAVE_AVAHI */
  DEBUG_printf(("5cups_dnssd_query_cb(browser=%p, interfaceIndex=%d, "
		"protocol=%d, event=%d, fullName=\"%s\", rrclass=%u, "
		"rrtype=%u, rdata=%p, rdlen=%u, flags=%x, context=%p)",
		browser, interfaceIndex, protocol, event, fullName, rrclass,
		rrtype, rdata, (unsigned)rdlen, flags, context));

 /*
  * Only process "add" data...
  */

  if (event != AVAHI_BROWSER_NEW)
  {
    if (event == AVAHI_BROWSER_FAILURE)
      DEBUG_printf(("cups_dnssd_query_cb: %s",
		    avahi_strerror(avahi_client_errno(client))));

    return;
  }
#  endif /* HAVE_DNSSD */

 /*
  * Lookup the service in the devices array.
  */

  dkey.dest.name = name;

  cups_dnssd_unquote(name, fullName, sizeof(name));

  if ((ptr = strstr(name, "._")) != NULL)
    *ptr = '\0';

  if ((device = cupsArrayFind(data->devices, &dkey)) != NULL)
  {
   /*
    * Found it, pull out the make and model from the TXT record and save it...
    */

    const uint8_t	*txt,		/* Pointer into data */
			*txtnext,	/* Next key/value pair */
			*txtend;	/* End of entire TXT record */
    uint8_t		txtlen;		/* Length of current key/value pair */
    char		key[256],	/* Key string */
			value[256],	/* Value string */
			make_and_model[512],
					/* Manufacturer and model */
			model[256],	/* Model */
			uriname[1024],	/* Name for URI */
			uri[1024];	/* Printer URI */
    cups_ptype_t	type = CUPS_PRINTER_REMOTE | CUPS_PRINTER_BW;
					/* Printer type */
    int			saw_printer_type = 0;
					/* Did we see a printer-type key? */

    device->state     = _CUPS_DNSSD_PENDING;
    make_and_model[0] = '\0';

    strlcpy(model, "Unknown", sizeof(model));

    for (txt = rdata, txtend = txt + rdlen;
	 txt < txtend;
	 txt = txtnext)
    {
     /*
      * Read a key/value pair starting with an 8-bit length.  Since the
      * length is 8 bits and the size of the key/value buffers is 256, we
      * don't need to check for overflow...
      */

      txtlen = *txt++;

      if (!txtlen || (txt + txtlen) > txtend)
	break;

      txtnext = txt + txtlen;

      for (ptr = key; txt < txtnext && *txt != '='; txt ++)
	*ptr++ = (char)*txt;
      *ptr = '\0';

      if (txt < txtnext && *txt == '=')
      {
	txt ++;

	if (txt < txtnext)
	  memcpy(value, txt, (size_t)(txtnext - txt));
	value[txtnext - txt] = '\0';

	DEBUG_printf(("6cups_dnssd_query_cb: %s=%s", key, value));
      }
      else
      {
	DEBUG_printf(("6cups_dnssd_query_cb: '%s' with no value.", key));
	continue;
      }

      if (!_cups_strcasecmp(key, "usb_MFG") ||
          !_cups_strcasecmp(key, "usb_MANU") ||
	  !_cups_strcasecmp(key, "usb_MANUFACTURER"))
	strlcpy(make_and_model, value, sizeof(make_and_model));
      else if (!_cups_strcasecmp(key, "usb_MDL") ||
               !_cups_strcasecmp(key, "usb_MODEL"))
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
      else if (!_cups_strcasecmp(key, "note"))
        device->dest.num_options = cupsAddOption("printer-location", value,
						 device->dest.num_options,
						 &device->dest.options);
      else if (!_cups_strcasecmp(key, "pdl"))
      {
       /*
        * Look for PDF-capable printers; only PDF-capable printers are shown.
        */

        const char	*start, *next;	/* Pointer into value */
        int		have_pdf = 0;	/* Have PDF? */

        for (start = value; start && *start; start = next)
        {
          if (!_cups_strncasecmp(start, "application/pdf", 15) &&
              (!start[15] || start[15] == ','))
          {
            have_pdf = 1;
            break;
          }

          if ((next = strchr(start, ',')) != NULL)
            next ++;
        }

        if (!have_pdf)
          device->state = _CUPS_DNSSD_INCOMPATIBLE;
      }
      else if (!_cups_strcasecmp(key, "printer-type"))
      {
       /*
        * Value is either NNNN or 0xXXXX
        */

	saw_printer_type = 1;
        type             = (cups_ptype_t)strtol(value, NULL, 0);
      }
      else if (!saw_printer_type)
      {
	if (!_cups_strcasecmp(key, "air") &&
		 !_cups_strcasecmp(value, "t"))
	  type |= CUPS_PRINTER_AUTHENTICATED;
	else if (!_cups_strcasecmp(key, "bind") &&
		 !_cups_strcasecmp(value, "t"))
	  type |= CUPS_PRINTER_BIND;
	else if (!_cups_strcasecmp(key, "collate") &&
		 !_cups_strcasecmp(value, "t"))
	  type |= CUPS_PRINTER_COLLATE;
	else if (!_cups_strcasecmp(key, "color") &&
		 !_cups_strcasecmp(value, "t"))
	  type |= CUPS_PRINTER_COLOR;
	else if (!_cups_strcasecmp(key, "copies") &&
		 !_cups_strcasecmp(value, "t"))
	  type |= CUPS_PRINTER_COPIES;
	else if (!_cups_strcasecmp(key, "duplex") &&
		 !_cups_strcasecmp(value, "t"))
	  type |= CUPS_PRINTER_DUPLEX;
	else if (!_cups_strcasecmp(key, "fax") &&
		 !_cups_strcasecmp(value, "t"))
	  type |= CUPS_PRINTER_MFP;
	else if (!_cups_strcasecmp(key, "papercustom") &&
		 !_cups_strcasecmp(value, "t"))
	  type |= CUPS_PRINTER_VARIABLE;
	else if (!_cups_strcasecmp(key, "papermax"))
	{
	  if (!_cups_strcasecmp(value, "legal-a4"))
	    type |= CUPS_PRINTER_SMALL;
	  else if (!_cups_strcasecmp(value, "isoc-a2"))
	    type |= CUPS_PRINTER_MEDIUM;
	  else if (!_cups_strcasecmp(value, ">isoc-a2"))
	    type |= CUPS_PRINTER_LARGE;
	}
	else if (!_cups_strcasecmp(key, "punch") &&
		 !_cups_strcasecmp(value, "t"))
	  type |= CUPS_PRINTER_PUNCH;
	else if (!_cups_strcasecmp(key, "scan") &&
		 !_cups_strcasecmp(value, "t"))
	  type |= CUPS_PRINTER_MFP;
	else if (!_cups_strcasecmp(key, "sort") &&
		 !_cups_strcasecmp(value, "t"))
	  type |= CUPS_PRINTER_SORT;
	else if (!_cups_strcasecmp(key, "staple") &&
		 !_cups_strcasecmp(value, "t"))
	  type |= CUPS_PRINTER_STAPLE;
      }
    }

   /*
    * Save the printer-xxx values...
    */

    device->dest.num_options = cupsAddOption("printer-info", name,
					     device->dest.num_options,
					     &device->dest.options);

    if (make_and_model[0])
    {
      strlcat(make_and_model, " ", sizeof(make_and_model));
      strlcat(make_and_model, model, sizeof(make_and_model));

      device->dest.num_options = cupsAddOption("printer-make-and-model",
      					       make_and_model,
					       device->dest.num_options,
					       &device->dest.options);
    }
    else
      device->dest.num_options = cupsAddOption("printer-make-and-model",
      					       model,
					       device->dest.num_options,
					       &device->dest.options);

    device->type = type;
    snprintf(value, sizeof(value), "%u", type);
    device->dest.num_options = cupsAddOption("printer-type", value,
					     device->dest.num_options,
					     &device->dest.options);

   /*
    * Save the URI...
    */

    cups_dnssd_unquote(uriname, device->fullName, sizeof(uriname));
    httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri),
                    !strcmp(device->regtype, "_ipps._tcp") ? "ipps" : "ipp",
                    NULL, uriname, 0, saw_printer_type ? "/cups" : "/");

    DEBUG_printf(("6cups_dnssd_query: printer-uri-supported=\"%s\"", uri));

    device->dest.num_options = cupsAddOption("printer-uri-supported", uri,
					     device->dest.num_options,
					     &device->dest.options);
  }
  else
    DEBUG_printf(("6cups_dnssd_query: Ignoring TXT record for '%s'.",
                  fullName));
}


/*
 * 'cups_dnssd_resolve()' - Resolve a Bonjour printer URI.
 */

static const char *			/* O - Resolved URI or NULL */
cups_dnssd_resolve(
    cups_dest_t    *dest,		/* I - Destination */
    const char     *uri,		/* I - Current printer URI */
    int            msec,		/* I - Time in milliseconds */
    int            *cancel,		/* I - Pointer to "cancel" variable */
    cups_dest_cb_t cb,			/* I - Callback */
    void           *user_data)		/* I - User data for callback */
{
  char			tempuri[1024];	/* Temporary URI buffer */
  _cups_dnssd_resolve_t	resolve;	/* Resolve data */


 /*
  * Resolve the URI...
  */

  resolve.cancel = cancel;
  gettimeofday(&resolve.end_time, NULL);
  if (msec > 0)
  {
    resolve.end_time.tv_sec  += msec / 1000;
    resolve.end_time.tv_usec += (msec % 1000) * 1000;

    while (resolve.end_time.tv_usec >= 1000000)
    {
      resolve.end_time.tv_sec ++;
      resolve.end_time.tv_usec -= 1000000;
    }
  }
  else
    resolve.end_time.tv_sec += 75;

  if (cb)
    (*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_RESOLVING,
	  dest);

  if ((uri = _httpResolveURI(uri, tempuri, sizeof(tempuri),
			     _HTTP_RESOLVE_FQDN, cups_dnssd_resolve_cb,
			     &resolve)) == NULL)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Unable to resolve printer-uri."), 1);

    if (cb)
      (*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_ERROR,
	    dest);

    return (NULL);
  }

 /*
  * Save the resolved URI...
  */

  dest->num_options = cupsAddOption("printer-uri-supported", uri,
				    dest->num_options, &dest->options);

  return (cupsGetOption("printer-uri-supported", dest->num_options,
                        dest->options));
}


/*
 * 'cups_dnssd_resolve_cb()' - See if we should continue resolving.
 */

static int				/* O - 1 to continue, 0 to stop */
cups_dnssd_resolve_cb(void *context)	/* I - Resolve data */
{
  _cups_dnssd_resolve_t	*resolve = (_cups_dnssd_resolve_t *)context;
					/* Resolve data */
  struct timeval	curtime;	/* Current time */


 /*
  * If the cancel variable is set, return immediately.
  */

  if (*resolve->cancel)
    return (0);

 /*
  * Otherwise check the end time...
  */

  gettimeofday(&curtime, NULL);

  return (curtime.tv_sec > resolve->end_time.tv_sec ||
          (curtime.tv_sec == resolve->end_time.tv_sec &&
           curtime.tv_usec > resolve->end_time.tv_usec));
}


/*
 * 'cups_dnssd_unquote()' - Unquote a name string.
 */

static void
cups_dnssd_unquote(char       *dst,	/* I - Destination buffer */
                   const char *src,	/* I - Source string */
		   size_t     dstsize)	/* I - Size of destination buffer */
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
#endif /* HAVE_DNSSD */


/*
 * 'cups_find_dest()' - Find a destination using a binary search.
 */

static int				/* O - Index of match */
cups_find_dest(const char  *name,	/* I - Destination name */
               const char  *instance,	/* I - Instance or NULL */
               int         num_dests,	/* I - Number of destinations */
	       cups_dest_t *dests,	/* I - Destinations */
	       int         prev,	/* I - Previous index */
	       int         *rdiff)	/* O - Difference of match */
{
  int		left,			/* Low mark for binary search */
		right,			/* High mark for binary search */
		current,		/* Current index */
		diff;			/* Result of comparison */
  cups_dest_t	key;			/* Search key */


  key.name     = (char *)name;
  key.instance = (char *)instance;

  if (prev >= 0)
  {
   /*
    * Start search on either side of previous...
    */

    if ((diff = cups_compare_dests(&key, dests + prev)) == 0 ||
        (diff < 0 && prev == 0) ||
	(diff > 0 && prev == (num_dests - 1)))
    {
      *rdiff = diff;
      return (prev);
    }
    else if (diff < 0)
    {
     /*
      * Start with previous on right side...
      */

      left  = 0;
      right = prev;
    }
    else
    {
     /*
      * Start wih previous on left side...
      */

      left  = prev;
      right = num_dests - 1;
    }
  }
  else
  {
   /*
    * Start search in the middle...
    */

    left  = 0;
    right = num_dests - 1;
  }

  do
  {
    current = (left + right) / 2;
    diff    = cups_compare_dests(&key, dests + current);

    if (diff == 0)
      break;
    else if (diff < 0)
      right = current;
    else
      left = current;
  }
  while ((right - left) > 1);

  if (diff != 0)
  {
   /*
    * Check the last 1 or 2 elements...
    */

    if ((diff = cups_compare_dests(&key, dests + left)) <= 0)
      current = left;
    else
    {
      diff    = cups_compare_dests(&key, dests + right);
      current = right;
    }
  }

 /*
  * Return the closest destination and the difference...
  */

  *rdiff = diff;

  return (current);
}


/*
 * 'cups_get_default()' - Get the default destination from an lpoptions file.
 */

static char *				/* O - Default destination or NULL */
cups_get_default(const char *filename,	/* I - File to read */
                 char       *namebuf,	/* I - Name buffer */
		 size_t     namesize,	/* I - Size of name buffer */
		 const char **instance)	/* I - Instance */
{
  cups_file_t	*fp;			/* lpoptions file */
  char		line[8192],		/* Line from file */
		*value,			/* Value for line */
		*nameptr;		/* Pointer into name */
  int		linenum;		/* Current line */


  *namebuf = '\0';

  if ((fp = cupsFileOpen(filename, "r")) != NULL)
  {
    linenum  = 0;

    while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
    {
      if (!_cups_strcasecmp(line, "default") && value)
      {
        strlcpy(namebuf, value, namesize);

	if ((nameptr = strchr(namebuf, ' ')) != NULL)
	  *nameptr = '\0';
	if ((nameptr = strchr(namebuf, '\t')) != NULL)
	  *nameptr = '\0';

	if ((nameptr = strchr(namebuf, '/')) != NULL)
	  *nameptr++ = '\0';

        *instance = nameptr;
	break;
      }
    }

    cupsFileClose(fp);
  }

  return (*namebuf ? namebuf : NULL);
}


/*
 * 'cups_get_dests()' - Get destinations from a file.
 */

static int				/* O - Number of destinations */
cups_get_dests(
    const char  *filename,		/* I - File to read from */
    const char  *match_name,		/* I - Destination name we want */
    const char  *match_inst,		/* I - Instance name we want */
    int         user_default_set,	/* I - User default printer set? */
    int         num_dests,		/* I - Number of destinations */
    cups_dest_t **dests)		/* IO - Destinations */
{
  int		i;			/* Looping var */
  cups_dest_t	*dest;			/* Current destination */
  cups_file_t	*fp;			/* File pointer */
  char		line[8192],		/* Line from file */
		*lineptr,		/* Pointer into line */
		*name,			/* Name of destination/option */
		*instance;		/* Instance of destination */
  int		linenum;		/* Current line number */


  DEBUG_printf(("7cups_get_dests(filename=\"%s\", match_name=\"%s\", "
                "match_inst=\"%s\", user_default_set=%d, num_dests=%d, "
		"dests=%p)", filename, match_name, match_inst,
		user_default_set, num_dests, dests));

 /*
  * Try to open the file...
  */

  if ((fp = cupsFileOpen(filename, "r")) == NULL)
    return (num_dests);

 /*
  * Read each printer; each line looks like:
  *
  *    Dest name[/instance] options
  *    Default name[/instance] options
  */

  linenum = 0;

  while (cupsFileGetConf(fp, line, sizeof(line), &lineptr, &linenum))
  {
   /*
    * See what type of line it is...
    */

    DEBUG_printf(("9cups_get_dests: linenum=%d line=\"%s\" lineptr=\"%s\"",
                  linenum, line, lineptr));

    if ((_cups_strcasecmp(line, "dest") && _cups_strcasecmp(line, "default")) || !lineptr)
    {
      DEBUG_puts("9cups_get_dests: Not a dest or default line...");
      continue;
    }

    name = lineptr;

   /*
    * Search for an instance...
    */

    while (!isspace(*lineptr & 255) && *lineptr && *lineptr != '/')
      lineptr ++;

    if (*lineptr == '/')
    {
     /*
      * Found an instance...
      */

      *lineptr++ = '\0';
      instance = lineptr;

     /*
      * Search for an instance...
      */

      while (!isspace(*lineptr & 255) && *lineptr)
	lineptr ++;
    }
    else
      instance = NULL;

    if (*lineptr)
      *lineptr++ = '\0';

    DEBUG_printf(("9cups_get_dests: name=\"%s\", instance=\"%s\"", name,
                  instance));

   /*
    * See if the primary instance of the destination exists; if not,
    * ignore this entry and move on...
    */

    if (match_name)
    {
      if (_cups_strcasecmp(name, match_name) ||
          (!instance && match_inst) ||
	  (instance && !match_inst) ||
	  (instance && _cups_strcasecmp(instance, match_inst)))
	continue;

      dest = *dests;
    }
    else if (cupsGetDest(name, NULL, num_dests, *dests) == NULL)
    {
      DEBUG_puts("9cups_get_dests: Not found!");
      continue;
    }
    else
    {
     /*
      * Add the destination...
      */

      num_dests = cupsAddDest(name, instance, num_dests, dests);

      if ((dest = cupsGetDest(name, instance, num_dests, *dests)) == NULL)
      {
       /*
	* Out of memory!
	*/

        DEBUG_puts("9cups_get_dests: Out of memory!");
        break;
      }
    }

   /*
    * Add options until we hit the end of the line...
    */

    dest->num_options = cupsParseOptions(lineptr, dest->num_options,
                                         &(dest->options));

   /*
    * If we found what we were looking for, stop now...
    */

    if (match_name)
      break;

   /*
    * Set this as default if needed...
    */

    if (!user_default_set && !_cups_strcasecmp(line, "default"))
    {
      DEBUG_puts("9cups_get_dests: Setting as default...");

      for (i = 0; i < num_dests; i ++)
        (*dests)[i].is_default = 0;

      dest->is_default = 1;
    }
  }

 /*
  * Close the file and return...
  */

  cupsFileClose(fp);

  return (num_dests);
}


/*
 * 'cups_make_string()' - Make a comma-separated string of values from an IPP
 *                        attribute.
 */

static char *				/* O - New string */
cups_make_string(
    ipp_attribute_t *attr,		/* I - Attribute to convert */
    char            *buffer,		/* I - Buffer */
    size_t          bufsize)		/* I - Size of buffer */
{
  int		i;			/* Looping var */
  char		*ptr,			/* Pointer into buffer */
		*end,			/* Pointer to end of buffer */
		*valptr;		/* Pointer into string attribute */


 /*
  * Return quickly if we have a single string value...
  */

  if (attr->num_values == 1 &&
      attr->value_tag != IPP_TAG_INTEGER &&
      attr->value_tag != IPP_TAG_ENUM &&
      attr->value_tag != IPP_TAG_BOOLEAN &&
      attr->value_tag != IPP_TAG_RANGE)
    return (attr->values[0].string.text);

 /*
  * Copy the values to the string, separating with commas and escaping strings
  * as needed...
  */

  end = buffer + bufsize - 1;

  for (i = 0, ptr = buffer; i < attr->num_values && ptr < end; i ++)
  {
    if (i)
      *ptr++ = ',';

    switch (attr->value_tag)
    {
      case IPP_TAG_INTEGER :
      case IPP_TAG_ENUM :
	  snprintf(ptr, (size_t)(end - ptr + 1), "%d", attr->values[i].integer);
	  break;

      case IPP_TAG_BOOLEAN :
	  if (attr->values[i].boolean)
	    strlcpy(ptr, "true", (size_t)(end - ptr + 1));
	  else
	    strlcpy(ptr, "false", (size_t)(end - ptr + 1));
	  break;

      case IPP_TAG_RANGE :
	  if (attr->values[i].range.lower == attr->values[i].range.upper)
	    snprintf(ptr, (size_t)(end - ptr + 1), "%d", attr->values[i].range.lower);
	  else
	    snprintf(ptr, (size_t)(end - ptr + 1), "%d-%d", attr->values[i].range.lower, attr->values[i].range.upper);
	  break;

      default :
	  for (valptr = attr->values[i].string.text;
	       *valptr && ptr < end;)
	  {
	    if (strchr(" \t\n\\\'\"", *valptr))
	    {
	      if (ptr >= (end - 1))
	        break;

	      *ptr++ = '\\';
	    }

	    *ptr++ = *valptr++;
	  }

	  *ptr = '\0';
	  break;
    }

    ptr += strlen(ptr);
  }

  *ptr = '\0';

  return (buffer);
}


/*
 * End of "$Id: dest.c 13075 2016-01-29 21:14:05Z msweet $".
 */
