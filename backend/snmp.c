/*
 * "$Id$"
 *
 *   SNMP discovery backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2006 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main()                    - Discover printers via SNMP.
 *   add_array()               - Add a string to an array.
 *   add_cache()               - Add a cached device...
 *   alarm_handler()           - Handle alarm signals...
 *   asn1_debug()              - Decode an ASN1-encoded message.
 *   compare_cache()           - Compare two cache entries.
 *   debug_printf()            - Display some debugging information.
 *   fix_make_model()          - Fix common problems in the make-and-model
 *                               string.
 *   free_array()              - Free an array of strings.
 *   free_cache()              - Free the array of cached devices.
 *   get_interface_addresses() - Get the broadcast address(es) associated
 *                               with an interface.
 *   hex_debug()               - Output hex debugging data...
 *   list_devices()            - List all of the devices we found...
 *   open_snmp_socket()        - Open the SNMP broadcast socket.
 *   probe_device()            - Probe a device to discover whether it is a
 *                               printer.
 *   read_snmp_conf()          - Read the snmp.conf file.
 *   read_snmp_response()      - Read and parse a SNMP response...
 *   scan_devices()            - Scan for devices using SNMP.
 *   send_snmp_query()         - Send an SNMP query packet.
 *   try_connect()             - Try connecting on a port...
 *   update_cache()            - Update a cached device...
 */

/*
 * Include necessary headers.
 */

#include <cups/backend.h>
#include <cups/http-private.h>
#include <cups/cups.h>
#include <cups/string.h>
#include <cups/array.h>
#include <cups/file.h>
#include <errno.h>
#include <signal.h>

#define SNMP_BACKEND
#include "ieee1284.c"


/*
 * This backend implements SNMP printer discovery.  It uses a broadcast-
 * based approach to get SNMP response packets from potential printers
 * and then interrogates each responder by trying to connect on port 515,
 * 631, 4010, 4020, 4030, 9100, 9101, and 9102.
 *
 * The backend reads the snmp.conf file from the CUPS_SERVERROOT directory
 * which can contain comments, blank lines, or any number of the following
 * directives:
 *
 *     Community name
 *     Address ip-address
 *     Address @LOCAL
 *     Address @IF(name)
 *
 * The default is to use:
 *
 *     Community public
 *     Address @LOCAL
 */

/*
 * Constants...
 */

#define SNMP_PORT		161	/* SNMP well-known port */
#define SNMP_MAX_PACKET		1472	/* Maximum size of SNMP packet */
#define SNMP_VERSION_1		0	/* SNMPv1 */
#define SNMP_DEVICE_PRINTER	5	/* hrDevicePrinter */

#define ASN1_END_OF_CONTENTS	0x00	/* End-of-contents */
#define ASN1_BOOLEAN		0x01	/* BOOLEAN */
#define ASN1_INTEGER		0x02	/* INTEGER or ENUMERATION */
#define ASN1_BIT_STRING		0x03	/* BIT STRING */
#define ASN1_OCTET_STRING	0x04	/* OCTET STRING */
#define ASN1_NULL_VALUE		0x05	/* NULL VALUE */
#define ASN1_OID		0x06	/* OBJECT IDENTIFIER */
#define ASN1_SEQUENCE		0x30	/* SEQUENCE */
#define ASN1_GET_REQUEST	0xa0	/* Get-Request-PDU */
#define ASN1_GET_RESPONSE	0xa2	/* Get-Response-PDU */


/*
 * Types...
 */

typedef struct snmp_cache_s		/**** SNMP scan cache ****/
{
  http_addr_t	address;		/* Address of device */
  char		*addrname,		/* Name of device */
		*uri,			/* device-uri */
		*id,			/* device-id */
		*make_and_model;	/* device-make-and-model */
} snmp_cache_t;


/*
 * Local functions...
 */

static char		*add_array(cups_array_t *a, const char *s);
static void		add_cache(http_addr_t *addr, const char *addrname,
			          const char *uri, const char *id,
				  const char *make_and_model);
static void		alarm_handler(int sig);
static void		asn1_debug(const unsigned char *buffer, size_t len,
			           int indent);
static int		compare_cache(snmp_cache_t *a, snmp_cache_t *b);
static void		debug_printf(const char *format, ...);
static void		fix_make_model(char *make_model,
			               const char *old_make_model,
				       int make_model_size);
static void		free_array(cups_array_t *a);
static void		free_cache(void);
static http_addrlist_t	*get_interface_addresses(const char *ifname);
static void		hex_debug(const unsigned char *buffer, size_t len);
static void		list_devices(void);
static int		open_snmp_socket(void);
static void		probe_device(snmp_cache_t *device);
static void		read_snmp_conf(const char *address);
static void		read_snmp_response(int fd);
static void		scan_devices(int fd);
static void		send_snmp_query(int fd, http_addr_t *addr, int version,
			                const char *community,
					const unsigned request_id,
					const unsigned char *oid);
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
static unsigned char	DeviceTypeOID[] = { 1, 3, 6, 1, 2, 1, 25, 3,
			                    2, 1, 2, 1, 0 };
static unsigned char	DeviceDescOID[] = { 1, 3, 6, 1, 2, 1, 25, 3,
			                    2, 1, 3, 1, 0 };
static unsigned		DeviceTypeRequest;
static unsigned		DeviceDescRequest;
static int		HostNameLookups = 1;


/*
 * 'main()' - Discover printers via SNMP.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments (6 or 7) */
     char *argv[])			/* I - Command-line arguments */
{
  int		fd;			/* SNMP socket */


 /*
  * Check command-line options...
  */

  if (argc > 2)
  {
    fputs("Usage: snmp [host-or-ip-address]\n", stderr);
    return (1);
  }

 /*
  * Open the SNMP socket...
  */

  if ((fd = open_snmp_socket()) < 0)
    return (1);

 /*
  * Read the configuration file and any cache data...
  */

  read_snmp_conf(argv[1]);

  Devices = cupsArrayNew((cups_array_func_t)compare_cache, NULL);

 /*
  * Scan for devices...
  */

  scan_devices(fd);

 /*
  * Display the results...
  */

  list_devices();

 /*
  * Close, free, and return with no errors...
  */

  close(fd);

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
               addr, addrname, uri, id ? id :  "(null)",
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

  if (DebugLevel)
    write(2, "DEBUG: ALARM!\n", 14);
}


/*
 * 'asn1_debug()' - Decode an ASN1-encoded message.
 */

static void
asn1_debug(const unsigned char *buffer,	/* I - Buffer */
           size_t              len,	/* I - Length of buffer */
           int                 indent)	/* I - Indentation */
{
  unsigned	integer;		/* Number value */
  char		string[1024];		/* String value */
  unsigned char	value_type;		/* Type of value */
  int		value_length;		/* Length of value */


  while (len > 0)
  {
    value_type   = *buffer++;
    value_length = 0;
    len --;

    while ((*buffer & 128) && len > 0)
    {
      value_length = (value_length << 7) | (*buffer++ & 127);
      len --;
    }

    if (len > 0)
    {
      value_length = (value_length << 7) | *buffer++;
      len --;
    }      

    switch (value_type)
    {
      case ASN1_INTEGER :
	  switch (value_length)
	  {
	    case 1 :
        	integer = buffer[0];
        	break;

	    case 2 :
        	integer = (buffer[0] << 8) | buffer[1];
        	break;

	    case 3 :
        	integer = (((buffer[0] << 8) | buffer[1]) << 8) | buffer[2];
        	break;

	    default :
        	integer = (((((buffer[0] << 8) | buffer[1]) << 8) |
	        	    buffer[2]) << 8) | buffer[3];
        	break;
	  }

          fprintf(stderr, "DEBUG: %*sINTEGER %d bytes %d\n", indent, "",
	          value_length, integer);
          break;

      case ASN1_OCTET_STRING :
          if (value_length < sizeof(string))
	  {
	    memcpy(string, buffer, value_length);
	    string[value_length] = '\0';
	  }
	  else
	  {
	    memcpy(string, buffer, sizeof(string) - 1);
	    string[sizeof(string) - 1] = '\0';
	  }

          fprintf(stderr, "DEBUG: %*sOCTET STRING %d bytes \"%s\"\n", indent, "",
	          value_length, string);
          break;

      case ASN1_NULL_VALUE :
          fprintf(stderr, "DEBUG: %*sNULL VALUE %d bytes\n", indent, "",
	          value_length);
          break;

      case ASN1_OID :
          fprintf(stderr, "DEBUG: %*sOID %d bytes .%d.%d", indent, "",
	          value_length, buffer[0] / 40, buffer[0] % 40);
	  for (value_length --, buffer ++, len --;
	       value_length > 0;
	       value_length --, buffer ++, len --)
	    fprintf(stderr, ".%d", buffer[0]);
	  putc('\n', stderr);
          break;

      case ASN1_SEQUENCE :
          fprintf(stderr, "DEBUG: %*sSEQUENCE %d bytes\n", indent, "",
	          value_length);
          asn1_debug(buffer, value_length, indent + 4);
          break;

      case ASN1_GET_REQUEST :
          fprintf(stderr, "DEBUG: %*sGet-Request-PDU %d bytes\n", indent, "",
	          value_length);
          asn1_debug(buffer, value_length, indent + 4);
          break;

      case ASN1_GET_RESPONSE :
          fprintf(stderr, "DEBUG: %*sGet-Response-PDU %d bytes\n", indent, "",
	          value_length);
          asn1_debug(buffer, value_length, indent + 4);
          break;

      default :
          fprintf(stderr, "DEBUG: %*sUNKNOWN(%x) %d bytes\n", indent, "",
	          value_type, value_length);
          break;
    }

    buffer += value_length;
    len    -= value_length;
  }
}
          

/*
 * 'compare_cache()' - Compare two cache entries.
 */

static int				/* O - Result of comparison */
compare_cache(snmp_cache_t *a,		/* I - First cache entry */
              snmp_cache_t *b)		/* I - Second cache entry */
{
  return (a->address.ipv4.sin_addr.s_addr - b->address.ipv4.sin_addr.s_addr);
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
  const char	*mmptr;			/* Pointer into make-and-model string */


 /*
  * Fix some common problems with the make-and-model string so
  * that printer driver detection works better...
  */

  if (!strncasecmp(old_make_model, "Hewlett-Packard", 15))
  {
   /*
    * Strip leading Hewlett-Packard and hp prefixes and replace
    * with a single HP manufacturer prefix...
    */

    mmptr = old_make_model + 15;

    while (isspace(*mmptr & 255))
      mmptr ++;

    if (!strncasecmp(mmptr, "hp", 2))
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
  else if (!strncasecmp(old_make_model, "deskjet", 7))
    snprintf(make_model, make_model_size, "HP DeskJet%s", old_make_model + 7);
  else if (!strncasecmp(old_make_model, "stylus_pro_", 11))
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

    _cups_strcpy((char *)mmptr, mmptr + 7);
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
 * 'hex_debug()' - Output hex debugging data...
 */

static void
hex_debug(const unsigned char *buffer,	/* I - Buffer */
          size_t              len)	/* I - Number of bytes */
{
  int	col;				/* Current column */


  for (col = 0; len > 0; col ++, buffer ++, len --)
  {
    if ((col & 15) == 0)
      fputs("DEBUG:", stderr);

    fprintf(stderr, " %02X", *buffer);

    if ((col & 15) == 15)
      putc('\n', stderr);
  }

  if (col & 15)
    putc('\n', stderr);
}


/*
 * 'list_devices()' - List all of the devices we found...
 */

static void
list_devices(void)
{
  snmp_cache_t	*cache;			/* Cached device */


  for (cache = (snmp_cache_t *)cupsArrayFirst(Devices);
       cache;
       cache = (snmp_cache_t *)cupsArrayNext(Devices))
    if (cache->uri)
      printf("network %s \"%s\" \"%s\" \"%s\"\n",
             cache->uri,
	     cache->make_and_model ? cache->make_and_model : "Unknown",
	     cache->addrname, cache->id ? cache->id : "");
}


/*
 * 'open_snmp_socket()' - Open the SNMP broadcast socket.
 */

static int				/* O - SNMP socket file descriptor */
open_snmp_socket(void)
{
  int		fd;			/* SNMP socket file descriptor */
  int		val;			/* Socket option value */


 /*
  * Create the SNMP socket...
  */

  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
    fprintf(stderr, "ERROR: Unable to create SNMP socket - %s\n",
            strerror(errno));

    return (-1);
  }

 /*
  * Set the "broadcast" flag...
  */

  val = 1;

  if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)))
  {
    fprintf(stderr, "ERROR: Unable to set broadcast mode - %s\n",
            strerror(errno));

    close(fd);

    return (-1);
  }

  return (fd);
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
  int		i, j;			/* Looping vars */
  http_t	*http;			/* HTTP connection for IPP */
  char		uri[1024];		/* Full device URI */


 /*
  * Try connecting via IPP first...
  */

  debug_printf("DEBUG: Probing %s...\n", device->addrname);

  if ((http = httpConnect(device->addrname, 631)) != NULL)
  {
   /*
    * IPP is supported...
    */

    ipp_t		*request,	/* IPP request */
			*response;	/* IPP response */
    ipp_attribute_t	*model,		/* printer-make-and-model attribute */
			*info,		/* printer-info attribute */
			*supported;	/* printer-uri-supported attribute */
    char		make_model[256],/* Make and model string to use */
			temp[256];	/* Temporary make/model string */
    int			num_uris;	/* Number of good URIs */
    static const char * const resources[] =
			{		/* Common resource paths for IPP */
			  "/ipp",
			  "/ipp/port2",
			  "/ipp/port3",
			  "/EPSON_IPP_Printer",
			  "/LPT1",
			  "/LPT2",
			  "/COM1",
			  "/"
			};


    debug_printf("DEBUG: %s supports IPP!\n", device->addrname);

   /*
    * Use non-blocking IO...
    */

    httpBlocking(http, 0);

   /*
    * Loop through a list of common resources that covers 99% of the
    * IPP-capable printers on the market today...
    */

    for (i = 0, num_uris = 0;
         i < (int)(sizeof(resources) / sizeof(resources[0]));
         i ++)
    {
     /*
      * Don't look past /ipp if we have found a working URI...
      */

      if (num_uris > 0 && strncmp(resources[i], "/ipp", 4))
        break;

      httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                      device->addrname, 631, resources[i]);

      debug_printf("DEBUG: Trying %s (num_uris=%d)\n", uri, num_uris);

      request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                   NULL, uri);

      response = cupsDoRequest(http, request, resources[i]);

      debug_printf("DEBUG: %s %s (%s)\n", uri,
        	   ippErrorString(cupsLastError()), cupsLastErrorString());

      if (response && response->request.status.status_code == IPP_OK)
      {
        model     = ippFindAttribute(response, "printer-make-and-model",
	                             IPP_TAG_TEXT);
        info      = ippFindAttribute(response, "printer-info", IPP_TAG_TEXT);
        supported = ippFindAttribute(response, "printer-uri-supported",
	                             IPP_TAG_URI);

        if (!supported)
	{
	  fprintf(stderr, "ERROR: Missing printer-uri-supported from %s!\n",
	          device->addrname);

	  httpClose(http);
	  return;
	}

        debug_printf("DEBUG: printer-info=\"%s\"\n",
	             info ? info->values[0].string.text : "(null)");
        debug_printf("DEBUG: printer-make-and-model=\"%s\"\n",
	             model ? model->values[0].string.text : "(null)");

       /*
        * Don't advertise this port if the printer actually only supports
	* a more generic version...
	*/

        if (!strncmp(resources[i], "/ipp/", 5))
	{
	  for (j = 0; j < supported->num_values; j ++)
	    if (strstr(supported->values[j].string.text, "/ipp/"))
	      break;

	  if (j >= supported->num_values)
	  {
	    ippDelete(response);
	    break;
	  }
        }

       /*
        * Don't use the printer-info attribute if it does not contain the
	* IEEE-1284 device ID data...
	*/

        if (info &&
	    (!strchr(info->values[0].string.text, ':') ||
	     !strchr(info->values[0].string.text, ';')))
 	  info = NULL;

       /*
        * If we don't have a printer-make-and-model string from the printer
	* but do have the 1284 device ID string, generate a make-and-model
	* string from the device ID info...
	*/

	if (model)
          strlcpy(temp, model->values[0].string.text, sizeof(temp));
	else if (info)
	  get_make_model(info->values[0].string.text, temp, sizeof(temp));

        fix_make_model(make_model, temp, sizeof(make_model));

       /*
        * Update the current device or add a new printer to the cache...
	*/

        if (num_uris == 0)
	  update_cache(device, uri, 
	               info ? info->values[0].string.text : NULL,
	               make_model[0] ? make_model : NULL);
	else
          add_cache(&(device->address), device->addrname, uri,
	            info ? info->values[0].string.text : NULL,
	            make_model[0] ? make_model : NULL);

        num_uris ++;
      }

      ippDelete(response);

      if (num_uris > 0 && cupsLastError() != IPP_OK)
        break;
    }

    httpClose(http);

    if (num_uris > 0)
      return;
  }

 /*
  * OK, now try the standard ports...
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


 /*
  * Initialize the global address and community lists...
  */

  Addresses   = cupsArrayNew(NULL, NULL);
  Communities = cupsArrayNew(NULL, NULL);

  if (address)
    add_array(Addresses, address);

  if ((debug = getenv("CUPS_DEBUG_LEVEL")) != NULL)
    DebugLevel = atoi(debug);

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
      else if (!strcasecmp(line, "Address"))
      {
        if (!address)
          add_array(Addresses, value);
      }
      else if (!strcasecmp(line, "Community"))
        add_array(Communities, value);
      else if (!strcasecmp(line, "DebugLevel"))
        DebugLevel = atoi(value);
      else if (!strcasecmp(line, "HostNameLookups"))
        HostNameLookups = !strcasecmp(value, "on") ||
	                  !strcasecmp(value, "yes") ||
	                  !strcasecmp(value, "true") ||
	                  !strcasecmp(value, "double");
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
    fputs("INFO: Using default SNMP Address @LOCAL\n", stderr);
    add_array(Addresses, "@LOCAL");
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
  unsigned char	buffer[SNMP_MAX_PACKET],/* Data packet */
		*bufptr;		/* Pointer into the data */
  int		bytes;			/* Number of bytes received */
  http_addr_t	addr;			/* Source address */
  socklen_t	addrlen;		/* Source address length */
  char		addrname[256];		/* Source address name */
  unsigned	request_id,		/* request-id from packet */
		error_status;		/* error-status from packet */
  char		community[128];		/* Community name */


 /*
  * Read the response data...
  */

  addrlen = sizeof(addr);

  if ((bytes = recvfrom(fd, buffer, sizeof(buffer), 0, (void *)&addr,
                        &addrlen)) < 0)
  {
    fprintf(stderr, "ERROR: Unable to read data from socket: %s\n",
            strerror(errno));
    return;
  }

  if (HostNameLookups)
    httpAddrLookup(&addr, addrname, sizeof(addrname));
  else
    httpAddrString(&addr, addrname, sizeof(addrname));

  debug_printf("DEBUG: Received %d bytes from %s...\n", bytes, addrname);

  if (DebugLevel > 1)
    asn1_debug(buffer, bytes, 0);
  if (DebugLevel > 2)
    hex_debug(buffer, bytes);

 /*
  * Look for the response status code in the SNMP message header...
  */

  bufptr = buffer + 7 + buffer[6] + 2;
  if (buffer[0] != ASN1_SEQUENCE ||
      buffer[2] != ASN1_INTEGER || buffer[3] != 1 || buffer[4] != 0 ||
      buffer[5] != ASN1_OCTET_STRING || (buffer[6] & 128) ||
      buffer[7 + buffer[6]] != ASN1_GET_RESPONSE ||
      *bufptr != ASN1_INTEGER || bufptr[1] < 1 || bufptr[1] > 4)
  {
    fprintf(stderr, "ERROR: Bad SNMP packet from %s!\n", addrname);
    return;
  }

  memcpy(community, buffer + 7, buffer[6]);
  community[buffer[6]] = '\0';

  bufptr ++;
  switch (*bufptr)
  {
    case 1 :
        request_id = bufptr[1];
        break;

    case 2 :
        request_id = (bufptr[1] << 8) | bufptr[2];
        break;

    case 3 :
        request_id = (((bufptr[1] << 8) | bufptr[2]) << 8) | bufptr[3];
        break;

    default :
        request_id = (((((bufptr[1] << 8) | bufptr[2]) << 8) |
	               bufptr[3]) << 8) | bufptr[4];
        break;
  }

  bufptr += *bufptr + 1;

  debug_printf("DEBUG: request-id=%u\n", request_id);

  if (*bufptr != ASN1_INTEGER)
  {
    fprintf(stderr, "ERROR: Bad SNMP packet from %s!\n", addrname);
    return;
  }

  bufptr ++;
  switch (*bufptr)
  {
    case 1 :
        error_status = bufptr[1];
        break;

    case 2 :
        error_status = (bufptr[1] << 8) | bufptr[2];
        break;

    case 3 :
        error_status = (((bufptr[1] << 8) | bufptr[2]) << 8) | bufptr[3];
        break;

    default :
        error_status = (((((bufptr[1] << 8) | bufptr[2]) << 8) |
	               bufptr[3]) << 8) | bufptr[4];
        break;
  }

  bufptr += *bufptr + 1;

  debug_printf("DEBUG: error-status=%u\n", error_status);

  if (!error_status)
  {
    if (request_id == DeviceTypeRequest)
    {
     /*
      * Probe to get supported connections...
      */

      add_cache(&addr, addrname, NULL, NULL, NULL);

      send_snmp_query(fd, &addr, SNMP_VERSION_1, community,
                      DeviceDescRequest, DeviceDescOID);
    }
    else if (request_id == DeviceDescRequest && *bufptr == ASN1_INTEGER)
    {
     /*
      * Update an existing cache entry...
      */

      char		desc[128],	/* Description string */
			make_model[256];/* Make and model */
      snmp_cache_t	key,		/* Search key */
			*device;	/* Matching device */


     /*
      * Find a matching device in the cache...
      */

      key.address = addr;
      device      = (snmp_cache_t *)cupsArrayFind(Devices, &key);

      if (!device)
      {
        debug_printf("DEBUG: Discarding device description for \"%s\"...\n",
	             addrname);
        return;
      }

     /*
      * Get the description...
      */

      bufptr += 2 + bufptr[1];
      bufptr += 2;
      bufptr += 2;
      bufptr += 2 + bufptr[1];

      if (*bufptr != ASN1_OCTET_STRING || (bufptr[1] & 128))
      {
        fprintf(stderr,
	        "DEBUG: Discarding bad device description for \"%s\"...\n",
	        addrname);
        return;
      }

      memcpy(desc, bufptr + 2, bufptr[1]);
      desc[bufptr[1]] = '\0';

      debug_printf("DEBUG: Got device description \"%s\" for \"%s\"...\n",
		   desc, addrname);

     /*
      * Convert the description to a make and model string...
      */

      fix_make_model(make_model, desc, sizeof(make_model));

      if (device->make_and_model)
        free(device->make_and_model);

      device->make_and_model = strdup(make_model);
    }
  }
}


/*
 * 'scan_devices()' - Scan for devices using SNMP.
 */

static void
scan_devices(int fd)			/* I - SNMP socket */
{
  char			*address,	/* Current address */
			*community;	/* Current community */
  fd_set		input;		/* Input set for select() */
  struct timeval	timeout;	/* Timeout for select() */
  time_t		curtime,	/* Current time */
			endtime;	/* End time for scan */
  http_addrlist_t	*addrs,		/* List of addresses */
			*addr;		/* Current address */
  snmp_cache_t		*device;	/* Current device */


 /*
  * Setup the request IDs...
  */

  DeviceTypeRequest = time(NULL);
  DeviceDescRequest = DeviceTypeRequest + 1;

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
      addrs = httpAddrGetList(address, AF_INET, NULL);

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
        send_snmp_query(fd, &(addr->addr), SNMP_VERSION_1, community,
	                DeviceTypeRequest, DeviceTypeOID);
    }

    httpAddrFreeList(addrs);
  }

 /*
  * Then read any responses that come in over the next 3 seconds...
  */

  endtime = time(NULL) + 3;

  FD_ZERO(&input);

  while ((curtime = time(NULL)) < endtime)
  {
    gettimeofday(&timeout, NULL);
    debug_printf("DEBUG: select() at %d.%06d...\n",
        	 (int)timeout.tv_sec, (int)timeout.tv_usec);

    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;

    FD_SET(fd, &input);
    if (select(fd + 1, &input, NULL, NULL, &timeout) < 0)
    {
      fprintf(stderr, "ERROR: select() for %d failed: %s\n", fd,
              strerror(errno));
      break;
    }

    gettimeofday(&timeout, NULL);
    debug_printf("DEBUG: select() returned at %d.%06d...\n",
        	 (int)timeout.tv_sec, (int)timeout.tv_usec);

    if (FD_ISSET(fd, &input))
      read_snmp_response(fd);
    else
      break;
  }

 /*
  * Finally, probe all of the printers we discovered to see how they are
  * connected...
  */

  for (device = (snmp_cache_t *)cupsArrayFirst(Devices);
       device;
       device = (snmp_cache_t *)cupsArrayNext(Devices))
    if (!device->uri)
      probe_device(device);
}


/*
 * 'send_snmp_query()' - Send an SNMP query packet.
 */

static void
send_snmp_query(
    int                 fd,		/* I - SNMP socket */
    http_addr_t         *addr,		/* I - Address to send to */
    int                 version,	/* I - SNMP version */
    const char          *community,	/* I - Community name */
    const unsigned      request_id,	/* I - Request ID */
    const unsigned char *oid)		/* I - OID */
{
  unsigned char	buffer[SNMP_MAX_PACKET],/* SNMP message packet */
		*bufptr;		/* Pointer into buffer */
  size_t	commlen,		/* Length of community string */
		oidlen,			/* Length of OID */
		len;			/* Length of entire message */
  const unsigned char *oidptr;		/* Pointer into OID */
  char		addrname[32];		/* Address name */


 /*
  * Get the lengths of the community string and OID...
  */

  commlen = strlen(community);

  for (oidlen = 1, oidptr = oid + 2; *oidptr; oidlen ++, oidptr ++);

 /*
  * Then format the message...
  */

  bufptr  = buffer;

  *bufptr++ = ASN1_SEQUENCE;		/* SNMPv1 message header */
  *bufptr++ = 5 + commlen + 20 + oidlen + 2;
					  /* Length */

  *bufptr++ = ASN1_INTEGER;		/* version */
  *bufptr++ = 1;			/* Length */
  *bufptr++ = version;			/* Value */

  *bufptr++ = ASN1_OCTET_STRING;	/* community */
  *bufptr++ = commlen;			/* Length */
  memcpy(bufptr, community, commlen);
  bufptr += commlen;			/* Value */

  *bufptr++ = ASN1_GET_REQUEST;		/* Get-Request-PDU */
  *bufptr++ = 31;			/* Length */

  *bufptr++ = ASN1_INTEGER;		/* request-id */
  *bufptr++ = 4;			/* Length */
  *bufptr++ = request_id >> 24;		/* Value */
  *bufptr++ = request_id >> 16;
  *bufptr++ = request_id >> 8;
  *bufptr++ = request_id;

  *bufptr++ = ASN1_INTEGER;		/* error-status */
  *bufptr++ = 1;			/* Length */
  *bufptr++ = 0;			/* Value */

  *bufptr++ = ASN1_INTEGER;		/* error-index */
  *bufptr++ = 1;			/* Length */
  *bufptr++ = 0;			/* Value */

  *bufptr++ = ASN1_SEQUENCE;		/* variable-bindings */
  *bufptr++ = oidlen + 6;		/* Length */

  *bufptr++ = ASN1_SEQUENCE;		/* VarBind */
  *bufptr++ = oidlen + 4;		/* Length */
  *bufptr++ = ASN1_OID;			/* ObjectName */
  *bufptr++ = oidlen;			/* Length */
  *bufptr++ = oid[0] * 40 + oid[1];	/* Value */
  for (oidptr = oid + 2; *oidptr; oidptr ++)
    *bufptr++ = *oidptr;

  *bufptr++ = ASN1_NULL_VALUE;		/* ObjectValue */
  *bufptr++ = 0;			/* Length */

 /*
  * Send the message...
  */

  len = bufptr - buffer;

  debug_printf("DEBUG: Sending %d bytes to %s...\n", (int)len,
               httpAddrString(addr, addrname, sizeof(addrname)));
  if (DebugLevel > 1)
    asn1_debug(buffer, len, 0);
  if (DebugLevel > 2)
    hex_debug(buffer, len);

  addr->ipv4.sin_port = htons(SNMP_PORT);

  if (sendto(fd, buffer, len, 0, (void *)addr, sizeof(addr->ipv4)) < len)
    fprintf(stderr, "ERROR: Unable to send %d bytes to %s: %s\n",
            (int)len, addrname, strerror(errno));
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


  debug_printf("DEBUG: Trying %s://%s:%d...\n", port == 515 ? "lpd" : "socket",
               addrname, port);

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    fprintf(stderr, "ERROR: Unable to create socket: %s\n", strerror(errno));
    return (-1);
  }

  addr->ipv4.sin_port = htons(port);

  signal(SIGALRM, alarm_handler);
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
}


/*
 * End of "$Id$".
 */
