/*
 * "$Id: snmp.c 7810 2008-07-29 01:11:15Z mike $"
 *
 *   SNMP discovery backend for CUPS.
 *
 *   Copyright 2007-2011 by Apple Inc.
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
 *   main()                    - Discover printers via SNMP.
 *   add_array()               - Add a string to an array.
 *   add_cache()               - Add a cached device...
 *   add_device_uri()          - Add a device URI to the cache.
 *   alarm_handler()           - Handle alarm signals...
 *   compare_cache()           - Compare two cache entries.
 *   debug_printf()            - Display some debugging information.
 *   fix_make_model()          - Fix common problems in the make-and-model
 *                               string.
 *   free_array()              - Free an array of strings.
 *   free_cache()              - Free the array of cached devices.
 *   get_interface_addresses() - Get the broadcast address(es) associated with
 *                               an interface.
 *   list_device()             - List a device we found...
 *   password_cb()             - Handle authentication requests.
 *   probe_device()            - Probe a device to discover whether it is a
 *                               printer.
 *   read_snmp_conf()          - Read the snmp.conf file.
 *   read_snmp_response()      - Read and parse a SNMP response...
 *   run_time()                - Return the total running time...
 *   scan_devices()            - Scan for devices using SNMP.
 *   try_connect()             - Try connecting on a port...
 *   update_cache()            - Update a cached device...
 */

/*
 * Include necessary headers.
 */

#include "backend-private.h"
#include <cups/array.h>
#include <cups/file.h>
#include <cups/http-private.h>
#include <regex.h>


/*
 * This backend implements SNMP printer discovery.  It uses a broadcast-
 * based approach to get SNMP response packets from potential printers,
 * requesting OIDs from the Host and Port Monitor MIBs, does a URI
 * lookup based on the device description string, and finally a probe of
 * port 9100 (AppSocket) and 515 (LPD).
 *
 * The current focus is on printers with internal network cards, although
 * the code also works with many external print servers as well.
 *
 * The backend reads the snmp.conf file from the CUPS_SERVERROOT directory
 * which can contain comments, blank lines, or any number of the following
 * directives:
 *
 *     Address ip-address
 *     Address @LOCAL
 *     Address @IF(name)
 *     Community name
 *     DebugLevel N
 *     DeviceURI "regex pattern" uri
 *     HostNameLookups on
 *     HostNameLookups off
 *     MaxRunTime N
 *
 * The default is to use:
 *
 *     Address @LOCAL
 *     Community public
 *     DebugLevel 0
 *     HostNameLookups off
 *     MaxRunTime 120
 *
 * This backend is known to work with the following network printers and
 * print servers:
 *
 *     Axis OfficeBasic, 5400, 5600
 *     Brother
 *     EPSON
 *     Genicom
 *     HP JetDirect
 *     Lexmark
 *     Sharp
 *     Tektronix
 *     Xerox
 *
 * It does not currently work with:
 *
 *     DLink
 *     Linksys
 *     Netgear
 *     Okidata
 *
 * (for all of these, they do not support the Host MIB)
 */

/*
 * Types...
 */

enum					/**** Request IDs for each field ****/
{
  DEVICE_TYPE = 1,
  DEVICE_DESCRIPTION,
  DEVICE_LOCATION,
  DEVICE_ID,
  DEVICE_URI,
  DEVICE_PRODUCT
};

typedef struct device_uri_s		/**** DeviceURI values ****/
{
  regex_t	re;			/* Regular expression to match */
  cups_array_t	*uris;			/* URIs */
} device_uri_t;

typedef struct snmp_cache_s		/**** SNMP scan cache ****/
{
  http_addr_t	address;		/* Address of device */
  char		*addrname,		/* Name of device */
		*uri,			/* device-uri */
		*id,			/* device-id */
		*info,			/* device-info */
		*location,		/* device-location */
		*make_and_model;	/* device-make-and-model */
  int		sent;			/* Has this device been listed? */
} snmp_cache_t;


/*
 * Local functions...
 */

static char		*add_array(cups_array_t *a, const char *s);
static void		add_cache(http_addr_t *addr, const char *addrname,
			          const char *uri, const char *id,
				  const char *make_and_model);
static device_uri_t	*add_device_uri(char *value);
static void		alarm_handler(int sig);
static int		compare_cache(snmp_cache_t *a, snmp_cache_t *b);
static void		debug_printf(const char *format, ...);
static void		fix_make_model(char *make_model,
			               const char *old_make_model,
				       int make_model_size);
static void		free_array(cups_array_t *a);
static void		free_cache(void);
static http_addrlist_t	*get_interface_addresses(const char *ifname);
static void		list_device(snmp_cache_t *cache);
static const char	*password_cb(const char *prompt);
static void		probe_device(snmp_cache_t *device);
static void		read_snmp_conf(const char *address);
static void		read_snmp_response(int fd);
static double		run_time(void);
static void		scan_devices(int ipv4, int ipv6);
static int		try_connect(http_addr_t *addr, const char *addrname,
			            int port);
static void		update_cache(snmp_cache_t *device, const char *uri,
			             const char *id, const char *make_model);


/*
 * Local globals...
 */

static cups_array_t	*Addresses = NULL;
static cups_array_t	*Communities = NULL;
static cups_array_t	*Devices = NULL;
static int		DebugLevel = 0;
static const int	DescriptionOID[] = { CUPS_OID_hrDeviceDescr, 1, -1 };
static const int	LocationOID[] = { CUPS_OID_sysLocation, 0, -1 };
static const int	DeviceTypeOID[] = { CUPS_OID_hrDeviceType, 1, -1 };
static const int	DeviceIdOID[] = { CUPS_OID_ppmPrinterIEEE1284DeviceId, 1, -1 };
static const int	UriOID[] = { CUPS_OID_ppmPortServiceNameOrURI, 1, 1, -1 };
static const int	LexmarkProductOID[] = { 1,3,6,1,4,1,641,2,1,2,1,2,1,-1 };
static const int	LexmarkProductOID2[] = { 1,3,6,1,4,1,674,10898,100,2,1,2,1,2,1,-1 };
static const int	LexmarkDeviceIdOID[] = { 1,3,6,1,4,1,641,2,1,2,1,3,1,-1 };
static const int	XeroxProductOID[] = { 1,3,6,1,4,1,128,2,1,3,1,2,0,-1 };
static cups_array_t	*DeviceURIs = NULL;
static int		HostNameLookups = 0;
static int		MaxRunTime = 120;
static struct timeval	StartTime;


/*
 * 'main()' - Discover printers via SNMP.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments (6 or 7) */
     char *argv[])			/* I - Command-line arguments */
{
  int		ipv4,			/* SNMP IPv4 socket */
		ipv6;			/* SNMP IPv6 socket */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Check command-line options...
  */

  if (argc > 2)
  {
    _cupsLangPuts(stderr, _("Usage: snmp [host-or-ip-address]"));
    return (1);
  }

 /*
  * Set the password callback for IPP operations...
  */

  cupsSetPasswordCB(password_cb);

 /*
  * Catch SIGALRM signals...
  */

#ifdef HAVE_SIGSET
  sigset(SIGALRM, alarm_handler);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGALRM);
  action.sa_handler = alarm_handler;
  sigaction(SIGALRM, &action, NULL);
#else
  signal(SIGALRM, alarm_handler);
#endif /* HAVE_SIGSET */

 /*
  * Open the SNMP socket...
  */

  if ((ipv4 = _cupsSNMPOpen(AF_INET)) < 0)
    return (1);

#ifdef AF_INET6
  if ((ipv6 = _cupsSNMPOpen(AF_INET6)) < 0)
    return (1);
#else
  ipv6 = -1;
#endif /* AF_INET6 */

 /*
  * Read the configuration file and any cache data...
  */

  read_snmp_conf(argv[1]);

  _cupsSNMPSetDebug(DebugLevel);

  Devices = cupsArrayNew((cups_array_func_t)compare_cache, NULL);

 /*
  * Scan for devices...
  */

  scan_devices(ipv4, ipv6);

 /*
  * Close, free, and return with no errors...
  */

  _cupsSNMPClose(ipv4);
  if (ipv6 >= 0)
    _cupsSNMPClose(ipv6);

  free_array(Addresses);
  free_array(Communities);
  free_cache();

  return (0);
}


/*
 * 'add_array()' - Add a string to an array.
 */

static char *				/* O - New string */
add_array(cups_array_t *a,		/* I - Array */
          const char   *s)		/* I - String to add */
{
  char	*dups;				/* New string */


  dups = strdup(s);

  cupsArrayAdd(a, dups);

  return (dups);
}


/*
 * 'add_cache()' - Add a cached device...
 */

static void
add_cache(http_addr_t *addr,		/* I - Device IP address */
          const char  *addrname,	/* I - IP address or name string */
          const char  *uri,		/* I - Device URI */
          const char  *id,		/* I - 1284 device ID */
	  const char  *make_and_model)	/* I - Make and model */
{
  snmp_cache_t	*temp;			/* New device entry */


  debug_printf("DEBUG: add_cache(addr=%p, addrname=\"%s\", uri=\"%s\", "
                  "id=\"%s\", make_and_model=\"%s\")\n",
               addr, addrname, uri ? uri : "(null)", id ? id : "(null)",
	       make_and_model ? make_and_model : "(null)");

  temp = calloc(1, sizeof(snmp_cache_t));
  memcpy(&(temp->address), addr, sizeof(temp->address));

  temp->addrname = strdup(addrname);

  if (uri)
    temp->uri = strdup(uri);

  if (id)
    temp->id = strdup(id);

  if (make_and_model)
    temp->make_and_model = strdup(make_and_model);

  cupsArrayAdd(Devices, temp);

  if (uri)
    list_device(temp);
}


/*
 * 'add_device_uri()' - Add a device URI to the cache.
 *
 * The value string is modified (chopped up) as needed.
 */

static device_uri_t *			/* O - Device URI */
add_device_uri(char *value)		/* I - Value from snmp.conf */
{
  device_uri_t	*device_uri;		/* Device URI */
  char		*start;			/* Start of value */


 /*
  * Allocate memory as needed...
  */

  if (!DeviceURIs)
    DeviceURIs = cupsArrayNew(NULL, NULL);

  if (!DeviceURIs)
    return (NULL);

  if ((device_uri = calloc(1, sizeof(device_uri_t))) == NULL)
    return (NULL);

  if ((device_uri->uris = cupsArrayNew(NULL, NULL)) == NULL)
  {
    free(device_uri);
    return (NULL);
  }

 /*
  * Scan the value string for the regular expression and URI(s)...
  */

  value ++; /* Skip leading " */

  for (start = value; *value && *value != '\"'; value ++)
    if (*value == '\\' && value[1])
      _cups_strcpy(value, value + 1);

  if (!*value)
  {
    fputs("ERROR: Missing end quote for DeviceURI!\n", stderr);

    cupsArrayDelete(device_uri->uris);
    free(device_uri);

    return (NULL);
  }

  *value++ = '\0';

  if (regcomp(&(device_uri->re), start, REG_EXTENDED | REG_ICASE))
  {
    fputs("ERROR: Bad regular expression for DeviceURI!\n", stderr);

    cupsArrayDelete(device_uri->uris);
    free(device_uri);

    return (NULL);
  }

  while (*value)
  {
    while (isspace(*value & 255))
      value ++;

    if (!*value)
      break;

    for (start = value; *value && !isspace(*value & 255); value ++);

    if (*value)
      *value++ = '\0';

    cupsArrayAdd(device_uri->uris, strdup(start));
  }

 /*
  * Add the device URI to the list and return it...
  */

  cupsArrayAdd(DeviceURIs, device_uri);

  return (device_uri);
}


/*
 * 'alarm_handler()' - Handle alarm signals...
 */

static void
alarm_handler(int sig)			/* I - Signal number */
{
 /*
  * Do nothing...
  */

  (void)sig;

#if !defined(HAVE_SIGSET) && !defined(HAVE_SIGACTION)
  signal(SIGALRM, alarm_handler);
#endif /* !HAVE_SIGSET && !HAVE_SIGACTION */

  if (DebugLevel)
    write(2, "DEBUG: ALARM!\n", 14);
}


/*
 * 'compare_cache()' - Compare two cache entries.
 */

static int				/* O - Result of comparison */
compare_cache(snmp_cache_t *a,		/* I - First cache entry */
              snmp_cache_t *b)		/* I - Second cache entry */
{
  return (_cups_strcasecmp(a->addrname, b->addrname));
}


/*
 * 'debug_printf()' - Display some debugging information.
 */

static void
debug_printf(const char *format,	/* I - Printf-style format string */
             ...)			/* I - Additional arguments as needed */
{
  va_list	ap;			/* Pointer to arguments */


  if (!DebugLevel)
    return;

  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
}


/*
 * 'fix_make_model()' - Fix common problems in the make-and-model string.
 */

static void
fix_make_model(
    char       *make_model,		/* I - New make-and-model string */
    const char *old_make_model,		/* I - Old make-and-model string */
    int        make_model_size)		/* I - Size of new string buffer */
{
  char	*mmptr;				/* Pointer into make-and-model string */


 /*
  * Fix some common problems with the make-and-model string so
  * that printer driver detection works better...
  */

  if (!_cups_strncasecmp(old_make_model, "Hewlett-Packard", 15))
  {
   /*
    * Strip leading Hewlett-Packard and hp prefixes and replace
    * with a single HP manufacturer prefix...
    */

    mmptr = (char *)old_make_model + 15;

    while (isspace(*mmptr & 255))
      mmptr ++;

    if (!_cups_strncasecmp(mmptr, "hp", 2))
    {
      mmptr += 2;

      while (isspace(*mmptr & 255))
	mmptr ++;
    }

    make_model[0] = 'H';
    make_model[1] = 'P';
    make_model[2] = ' ';
    strlcpy(make_model + 3, mmptr, make_model_size - 3);
  }
  else if (!_cups_strncasecmp(old_make_model, "deskjet", 7))
    snprintf(make_model, make_model_size, "HP DeskJet%s", old_make_model + 7);
  else if (!_cups_strncasecmp(old_make_model, "officejet", 9))
    snprintf(make_model, make_model_size, "HP OfficeJet%s", old_make_model + 9);
  else if (!_cups_strncasecmp(old_make_model, "stylus_pro_", 11))
    snprintf(make_model, make_model_size, "EPSON Stylus Pro %s",
             old_make_model + 11);
  else
    strlcpy(make_model, old_make_model, make_model_size);

  if ((mmptr = strstr(make_model, ", Inc.,")) != NULL)
  {
   /*
    * Strip inc. from name, e.g. "Tektronix, Inc., Phaser 560"
    * becomes "Tektronix Phaser 560"...
    */

    _cups_strcpy(mmptr, mmptr + 7);
  }

  if ((mmptr = strstr(make_model, " Network")) != NULL)
  {
   /*
    * Drop unnecessary informational text, e.g. "Xerox DocuPrint N2025
    * Network LaserJet - 2.12" becomes "Xerox DocuPrint N2025"...
    */

    *mmptr = '\0';
  }

  if ((mmptr = strchr(make_model, ',')) != NULL)
  {
   /*
    * Drop anything after a trailing comma...
    */

    *mmptr = '\0';
  }
}


/*
 * 'free_array()' - Free an array of strings.
 */

static void
free_array(cups_array_t *a)		/* I - Array */
{
  char	*s;				/* Current string */


  for (s = (char *)cupsArrayFirst(a); s; s = (char *)cupsArrayNext(a))
    free(s);

  cupsArrayDelete(a);
}


/*
 * 'free_cache()' - Free the array of cached devices.
 */

static void
free_cache(void)
{
  snmp_cache_t	*cache;			/* Cached device */


  for (cache = (snmp_cache_t *)cupsArrayFirst(Devices);
       cache;
       cache = (snmp_cache_t *)cupsArrayNext(Devices))
  {
    free(cache->addrname);

    if (cache->uri)
      free(cache->uri);

    if (cache->id)
      free(cache->id);

    if (cache->make_and_model)
      free(cache->make_and_model);

    free(cache);
  }

  cupsArrayDelete(Devices);
  Devices = NULL;
}


/*
 * 'get_interface_addresses()' - Get the broadcast address(es) associated
 *                               with an interface.
 */

static http_addrlist_t *		/* O - List of addresses */
get_interface_addresses(
    const char *ifname)			/* I - Interface name */
{
  struct ifaddrs	*addrs,		/* Interface address list */
			*addr;		/* Current interface address */
  http_addrlist_t	*first,		/* First address in list */
			*last,		/* Last address in list */
			*current;	/* Current address */


  if (getifaddrs(&addrs) < 0)
    return (NULL);

  for (addr = addrs, first = NULL, last = NULL; addr; addr = addr->ifa_next)
    if ((addr->ifa_flags & IFF_BROADCAST) && addr->ifa_broadaddr &&
        addr->ifa_broadaddr->sa_family == AF_INET &&
	(!ifname || !strcmp(ifname, addr->ifa_name)))
    {
      current = calloc(1, sizeof(http_addrlist_t));

      memcpy(&(current->addr), addr->ifa_broadaddr,
             sizeof(struct sockaddr_in));

      if (!last)
        first = current;
      else
        last->next = current;

      last = current;
    }

  freeifaddrs(addrs);

  return (first);
}


/*
 * 'list_device()' - List a device we found...
 */

static void
list_device(snmp_cache_t *cache)	/* I - Cached device */
{
  if (cache->uri)
    cupsBackendReport("network", cache->uri, cache->make_and_model,
                      cache->info, cache->id, cache->location);
}


/*
 * 'password_cb()' - Handle authentication requests.
 *
 * All we do right now is return NULL, indicating that no authentication
 * is possible.
 */

static const char *			/* O - Password (NULL) */
password_cb(const char *prompt)		/* I - Prompt message */
{
  (void)prompt;				/* Anti-compiler-warning-code */

  return (NULL);
}


/*
 * 'probe_device()' - Probe a device to discover whether it is a printer.
 *
 * TODO: Try using the Port Monitor MIB to discover the correct protocol
 *       to use - first need a commercially-available printer that supports
 *       it, though...
 */

static void
probe_device(snmp_cache_t *device)	/* I - Device */
{
  char		uri[1024],		/* Full device URI */
		*uriptr,		/* Pointer into URI */
		*format;		/* Format string for device */
  device_uri_t	*device_uri;		/* Current DeviceURI match */


  debug_printf("DEBUG: %.3f Probing %s...\n", run_time(), device->addrname);

#ifdef __APPLE__
 /*
  * If the printer supports Bonjour/mDNS, don't report it from the SNMP backend.
  */

  if (!try_connect(&(device->address), device->addrname, 5353))
  {
    debug_printf("DEBUG: %s supports mDNS, not reporting!\n", device->addrname);
    return;
  }
#endif /* __APPLE__ */

 /*
  * Lookup the device in the match table...
  */

  for (device_uri = (device_uri_t *)cupsArrayFirst(DeviceURIs);
       device_uri;
       device_uri = (device_uri_t *)cupsArrayNext(DeviceURIs))
    if (device->make_and_model &&
        !regexec(&(device_uri->re), device->make_and_model, 0, NULL, 0))
    {
     /*
      * Found a match, add the URIs...
      */

      for (format = (char *)cupsArrayFirst(device_uri->uris);
           format;
	   format = (char *)cupsArrayNext(device_uri->uris))
      {
        for (uriptr = uri; *format && uriptr < (uri + sizeof(uri) - 1);)
	  if (*format == '%' && format[1] == 's')
	  {
	   /*
	    * Insert hostname/address...
	    */

	    strlcpy(uriptr, device->addrname, sizeof(uri) - (uriptr - uri));
	    uriptr += strlen(uriptr);
	    format += 2;
	  }
	  else
	    *uriptr++ = *format++;

        *uriptr = '\0';

        update_cache(device, uri, NULL, NULL);
      }

      return;
    }

 /*
  * Then try the standard ports...
  */

  if (!try_connect(&(device->address), device->addrname, 9100))
  {
    debug_printf("DEBUG: %s supports AppSocket!\n", device->addrname);

    snprintf(uri, sizeof(uri), "socket://%s", device->addrname);
    update_cache(device, uri, NULL, NULL);
  }
  else if (!try_connect(&(device->address), device->addrname, 515))
  {
    debug_printf("DEBUG: %s supports LPD!\n", device->addrname);

    snprintf(uri, sizeof(uri), "lpd://%s/", device->addrname);
    update_cache(device, uri, NULL, NULL);
  }
}


/*
 * 'read_snmp_conf()' - Read the snmp.conf file.
 */

static void
read_snmp_conf(const char *address)	/* I - Single address to probe */
{
  cups_file_t	*fp;			/* File pointer */
  char		filename[1024],		/* Filename */
		line[1024],		/* Line from file */
		*value;			/* Value on line */
  int		linenum;		/* Line number */
  const char	*cups_serverroot;	/* CUPS_SERVERROOT env var */
  const char	*debug;			/* CUPS_DEBUG_LEVEL env var */
  const char	*runtime;		/* CUPS_MAX_RUN_TIME env var */


 /*
  * Initialize the global address and community lists...
  */

  Addresses   = cupsArrayNew(NULL, NULL);
  Communities = cupsArrayNew(NULL, NULL);

  if (address)
    add_array(Addresses, address);

  if ((debug = getenv("CUPS_DEBUG_LEVEL")) != NULL)
    DebugLevel = atoi(debug);

  if ((runtime = getenv("CUPS_MAX_RUN_TIME")) != NULL)
    MaxRunTime = atoi(runtime);

 /*
  * Find the snmp.conf file...
  */

  if ((cups_serverroot = getenv("CUPS_SERVERROOT")) == NULL)
    cups_serverroot = CUPS_SERVERROOT;

  snprintf(filename, sizeof(filename), "%s/snmp.conf", cups_serverroot);

  if ((fp = cupsFileOpen(filename, "r")) != NULL)
  {
   /*
    * Read the snmp.conf file...
    */

    linenum = 0;

    while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
    {
      if (!value)
        fprintf(stderr, "ERROR: Missing value on line %d of %s!\n", linenum,
	        filename);
      else if (!_cups_strcasecmp(line, "Address"))
      {
        if (!address)
          add_array(Addresses, value);
      }
      else if (!_cups_strcasecmp(line, "Community"))
        add_array(Communities, value);
      else if (!_cups_strcasecmp(line, "DebugLevel"))
        DebugLevel = atoi(value);
      else if (!_cups_strcasecmp(line, "DeviceURI"))
      {
        if (*value != '\"')
	  fprintf(stderr,
	          "ERROR: Missing double quote for regular expression on "
		  "line %d of %s!\n", linenum, filename);
        else
	  add_device_uri(value);
      }
      else if (!_cups_strcasecmp(line, "HostNameLookups"))
        HostNameLookups = !_cups_strcasecmp(value, "on") ||
	                  !_cups_strcasecmp(value, "yes") ||
	                  !_cups_strcasecmp(value, "true") ||
	                  !_cups_strcasecmp(value, "double");
      else if (!_cups_strcasecmp(line, "MaxRunTime"))
        MaxRunTime = atoi(value);
      else
        fprintf(stderr, "ERROR: Unknown directive %s on line %d of %s!\n",
	        line, linenum, filename);
    }

    cupsFileClose(fp);
  }

 /*
  * Use defaults if parameters are undefined...
  */

  if (cupsArrayCount(Addresses) == 0)
  {
   /*
    * If we have no addresses, exit immediately...
    */

    fprintf(stderr,
            "DEBUG: No address specified and no Address line in %s...\n",
	    filename);
    exit(0);
  }

  if (cupsArrayCount(Communities) == 0)
  {
    fputs("INFO: Using default SNMP Community public\n", stderr);
    add_array(Communities, "public");
  }
}


/*
 * 'read_snmp_response()' - Read and parse a SNMP response...
 */

static void
read_snmp_response(int fd)		/* I - SNMP socket file descriptor */
{
  char		addrname[256];		/* Source address name */
  cups_snmp_t	packet;			/* Decoded packet */
  snmp_cache_t	key,			/* Search key */
		*device;		/* Matching device */


 /*
  * Read the response data...
  */

  if (!_cupsSNMPRead(fd, &packet, -1.0))
  {
    fprintf(stderr, "ERROR: Unable to read data from socket: %s\n",
            strerror(errno));
    return;
  }

  if (HostNameLookups)
    httpAddrLookup(&(packet.address), addrname, sizeof(addrname));
  else
    httpAddrString(&(packet.address), addrname, sizeof(addrname));

  debug_printf("DEBUG: %.3f Received data from %s...\n", run_time(), addrname);

 /*
  * Look for the response status code in the SNMP message header...
  */

  if (packet.error)
  {
    fprintf(stderr, "ERROR: Bad SNMP packet from %s: %s\n", addrname,
            packet.error);

    return;
  }

  debug_printf("DEBUG: community=\"%s\"\n", packet.community);
  debug_printf("DEBUG: request-id=%d\n", packet.request_id);
  debug_printf("DEBUG: error-status=%d\n", packet.error_status);

  if (packet.error_status && packet.request_id != DEVICE_TYPE)
    return;

 /*
  * Find a matching device in the cache...
  */

  key.addrname = addrname;
  device       = (snmp_cache_t *)cupsArrayFind(Devices, &key);

 /*
  * Process the message...
  */

  switch (packet.request_id)
  {
    case DEVICE_TYPE :
       /*
	* Got the device type response...
	*/

	if (device)
	{
	  debug_printf("DEBUG: Discarding duplicate device type for \"%s\"...\n",
		       addrname);
	  return;
	}

       /*
	* Add the device and request the device data...
	*/

	add_cache(&(packet.address), addrname, NULL, NULL, NULL);

	_cupsSNMPWrite(fd, &(packet.address), CUPS_SNMP_VERSION_1,
	               packet.community, CUPS_ASN1_GET_REQUEST,
		       DEVICE_DESCRIPTION, DescriptionOID);
	_cupsSNMPWrite(fd, &(packet.address), CUPS_SNMP_VERSION_1,
	               packet.community, CUPS_ASN1_GET_REQUEST,
		       DEVICE_ID, DeviceIdOID);
	_cupsSNMPWrite(fd, &(packet.address), CUPS_SNMP_VERSION_1,
	               packet.community, CUPS_ASN1_GET_REQUEST,
		       DEVICE_URI, UriOID);
	_cupsSNMPWrite(fd, &(packet.address), CUPS_SNMP_VERSION_1,
	               packet.community, CUPS_ASN1_GET_REQUEST,
		       DEVICE_LOCATION, LocationOID);
	_cupsSNMPWrite(fd, &(packet.address), CUPS_SNMP_VERSION_1,
	               packet.community, CUPS_ASN1_GET_REQUEST,
		       DEVICE_PRODUCT, LexmarkProductOID);
	_cupsSNMPWrite(fd, &(packet.address), CUPS_SNMP_VERSION_1,
	               packet.community, CUPS_ASN1_GET_REQUEST,
		       DEVICE_PRODUCT, LexmarkProductOID2);
	_cupsSNMPWrite(fd, &(packet.address), CUPS_SNMP_VERSION_1,
	               packet.community, CUPS_ASN1_GET_REQUEST,
		       DEVICE_ID, LexmarkDeviceIdOID);
	_cupsSNMPWrite(fd, &(packet.address), CUPS_SNMP_VERSION_1,
	               packet.community, CUPS_ASN1_GET_REQUEST,
		       DEVICE_PRODUCT, XeroxProductOID);
        break;

    case DEVICE_DESCRIPTION :
	if (device && packet.object_type == CUPS_ASN1_OCTET_STRING)
	{
	 /*
	  * Update an existing cache entry...
	  */

	  char	make_model[256];	/* Make and model */


	  if (strchr((char *)packet.object_value.string.bytes, ':') &&
	      strchr((char *)packet.object_value.string.bytes, ';'))
	  {
	   /*
	    * Description is the IEEE-1284 device ID...
	    */

	    if (!device->id)
	      device->id = strdup((char *)packet.object_value.string.bytes);

	    backendGetMakeModel((char *)packet.object_value.string.bytes,
				make_model, sizeof(make_model));

            if (device->info)
	      free(device->info);

	    device->info = strdup(make_model);
	  }
	  else
	  {
	   /*
	    * Description is plain text...
	    */

	    fix_make_model(make_model, (char *)packet.object_value.string.bytes,
			   sizeof(make_model));

            if (device->info)
	      free(device->info);

	    device->info = strdup((char *)packet.object_value.string.bytes);
	  }

	  if (!device->make_and_model)
	    device->make_and_model = strdup(make_model);
        }
	break;

    case DEVICE_ID :
	if (device && packet.object_type == CUPS_ASN1_OCTET_STRING &&
	    (!device->id ||
	     strlen(device->id) < packet.object_value.string.num_bytes))
	{
	 /*
	  * Update an existing cache entry...
	  */

	  char	make_model[256];	/* Make and model */


	  if (device->id)
	    free(device->id);

	  device->id = strdup((char *)packet.object_value.string.bytes);

	 /*
	  * Convert the ID to a make and model string...
	  */

	  backendGetMakeModel((char *)packet.object_value.string.bytes,
	                      make_model, sizeof(make_model));
	  if (device->make_and_model)
	    free(device->make_and_model);

	  device->make_and_model = strdup(make_model);
	}
	break;

    case DEVICE_LOCATION :
	if (device && packet.object_type == CUPS_ASN1_OCTET_STRING &&
	    !device->location)
	  device->location = strdup((char *)packet.object_value.string.bytes);
	break;

    case DEVICE_PRODUCT :
	if (device && packet.object_type == CUPS_ASN1_OCTET_STRING &&
	    !device->id)
	{
	 /*
	  * Update an existing cache entry...
	  */

          if (!device->info)
	    device->info = strdup((char *)packet.object_value.string.bytes);

          if (device->make_and_model)
	    free(device->make_and_model);

	  device->make_and_model = strdup((char *)packet.object_value.string.bytes);
	}
	break;

    case DEVICE_URI :
	if (device && packet.object_type == CUPS_ASN1_OCTET_STRING &&
	    !device->uri && packet.object_value.string.num_bytes > 0)
	{
	 /*
	  * Update an existing cache entry...
	  */

	  if (!strncmp((char *)packet.object_value.string.bytes, "lpr:", 4))
	  {
	   /*
	    * We want "lpd://..." for the URI...
	    */

	    packet.object_value.string.bytes[2] = 'd';
	  }

	  device->uri = strdup((char *)packet.object_value.string.bytes);
	}
	break;
  }
}


/*
 * 'run_time()' - Return the total running time...
 */

static double				/* O - Number of seconds */
run_time(void)
{
  struct timeval	curtime;	/* Current time */


  gettimeofday(&curtime, NULL);

  return (curtime.tv_sec - StartTime.tv_sec +
          0.000001 * (curtime.tv_usec - StartTime.tv_usec));
}


/*
 * 'scan_devices()' - Scan for devices using SNMP.
 */

static void
scan_devices(int ipv4,			/* I - SNMP IPv4 socket */
             int ipv6)			/* I - SNMP IPv6 socket */
{
  int			fd,		/* File descriptor for this address */
			busy;		/* Are we busy processing something? */
  char			*address,	/* Current address */
			*community;	/* Current community */
  fd_set		input;		/* Input set for select() */
  struct timeval	timeout;	/* Timeout for select() */
  time_t		endtime;	/* End time for scan */
  http_addrlist_t	*addrs,		/* List of addresses */
			*addr;		/* Current address */
  snmp_cache_t		*device;	/* Current device */
  char			temp[1024];	/* Temporary address string */


  gettimeofday(&StartTime, NULL);

 /*
  * First send all of the broadcast queries...
  */

  for (address = (char *)cupsArrayFirst(Addresses);
       address;
       address = (char *)cupsArrayNext(Addresses))
  {
    if (!strcmp(address, "@LOCAL"))
      addrs = get_interface_addresses(NULL);
    else if (!strncmp(address, "@IF(", 4))
    {
      char	ifname[255];		/* Interface name */

      strlcpy(ifname, address + 4, sizeof(ifname));
      if (ifname[0])
        ifname[strlen(ifname) - 1] = '\0';

      addrs = get_interface_addresses(ifname);
    }
    else
      addrs = httpAddrGetList(address, AF_UNSPEC, NULL);

    if (!addrs)
    {
      fprintf(stderr, "ERROR: Unable to scan \"%s\"!\n", address);
      continue;
    }

    for (community = (char *)cupsArrayFirst(Communities);
         community;
	 community = (char *)cupsArrayNext(Communities))
    {
      debug_printf("DEBUG: Scanning for devices in \"%s\" via \"%s\"...\n",
        	   community, address);

      for (addr = addrs; addr; addr = addr->next)
      {
#ifdef AF_INET6
        if (_httpAddrFamily(&(addr->addr)) == AF_INET6)
	  fd = ipv6;
	else
#endif /* AF_INET6 */
        fd = ipv4;

        debug_printf("DEBUG: Sending get request to %s...\n",
	             httpAddrString(&(addr->addr), temp, sizeof(temp)));

        _cupsSNMPWrite(fd, &(addr->addr), CUPS_SNMP_VERSION_1, community,
	               CUPS_ASN1_GET_REQUEST, DEVICE_TYPE, DeviceTypeOID);
      }
    }

    httpAddrFreeList(addrs);
  }

 /*
  * Then read any responses that come in over the next 3 seconds...
  */

  endtime = time(NULL) + MaxRunTime;

  FD_ZERO(&input);

  while (time(NULL) < endtime)
  {
    timeout.tv_sec  = 2;
    timeout.tv_usec = 0;

    FD_SET(ipv4, &input);
    if (ipv6 >= 0)
      FD_SET(ipv6, &input);

    fd = ipv4 > ipv6 ? ipv4 : ipv6;
    if (select(fd + 1, &input, NULL, NULL, &timeout) < 0)
    {
      fprintf(stderr, "ERROR: %.3f select() for %d/%d failed: %s\n", run_time(),
              ipv4, ipv6, strerror(errno));
      break;
    }

    busy = 0;

    if (FD_ISSET(ipv4, &input))
    {
      read_snmp_response(ipv4);
      busy = 1;
    }

    if (ipv6 >= 0 && FD_ISSET(ipv6, &input))
    {
      read_snmp_response(ipv6);
      busy = 1;
    }

    if (!busy)
    {
     /*
      * List devices with complete information...
      */

      int sent_something = 0;

      for (device = (snmp_cache_t *)cupsArrayFirst(Devices);
           device;
	   device = (snmp_cache_t *)cupsArrayNext(Devices))
        if (!device->sent && device->info && device->make_and_model)
	{
	  if (device->uri)
	    list_device(device);
	  else
	    probe_device(device);

	  device->sent = sent_something = 1;
	}

      if (!sent_something)
        break;
    }
  }

  debug_printf("DEBUG: %.3f Scan complete!\n", run_time());
}


/*
 * 'try_connect()' - Try connecting on a port...
 */

static int				/* O - 0 on success or -1 on error */
try_connect(http_addr_t *addr,		/* I - Socket address */
            const char  *addrname,	/* I - Hostname or IP address */
            int         port)		/* I - Port number */
{
  int	fd;				/* Socket */
  int	status;				/* Connection status */


  debug_printf("DEBUG: %.3f Trying %s://%s:%d...\n", run_time(),
               port == 515 ? "lpd" : "socket", addrname, port);

  if ((fd = socket(_httpAddrFamily(addr), SOCK_STREAM, 0)) < 0)
  {
    fprintf(stderr, "ERROR: Unable to create socket: %s\n",
            strerror(errno));
    return (-1);
  }

  _httpAddrSetPort(addr, port);

  alarm(1);

  status = connect(fd, (void *)addr, httpAddrLength(addr));

  close(fd);
  alarm(0);

  return (status);
}


/*
 * 'update_cache()' - Update a cached device...
 */

static void
update_cache(snmp_cache_t *device,	/* I - Device */
             const char   *uri,		/* I - Device URI */
	     const char   *id,		/* I - Device ID */
	     const char   *make_model)	/* I - Device make and model */
{
  if (device->uri)
    free(device->uri);

  device->uri = strdup(uri);

  if (id)
  {
    if (device->id)
      free(device->id);

    device->id = strdup(id);
  }

  if (make_model)
  {
    if (device->make_and_model)
      free(device->make_and_model);

    device->make_and_model = strdup(make_model);
  }

  list_device(device);
}


/*
 * End of "$Id: snmp.c 7810 2008-07-29 01:11:15Z mike $".
 */
