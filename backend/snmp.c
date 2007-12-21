/*
 * "$Id: snmp.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   SNMP discovery backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
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
 *   asn1_decode_snmp()        - Decode a SNMP packet.
 *   asn1_debug()              - Decode an ASN1-encoded message.
 *   asn1_encode_snmp()        - Encode a SNMP packet.
 *   asn1_get_integer()        - Get an integer value.
 *   asn1_get_length()         - Get a value length.
 *   asn1_get_oid()            - Get an OID value.
 *   asn1_get_packed()         - Get a packed integer value.
 *   asn1_get_string()         - Get a string value.
 *   asn1_get_type()           - Get a value type.
 *   asn1_set_integer()        - Set an integer value.
 *   asn1_set_length()         - Set a value length.
 *   asn1_set_oid()            - Set an OID value.
 *   asn1_set_packed()         - Set a packed integer value.
 *   asn1_size_integer()       - Figure out the number of bytes needed for an
 *                               integer value.
 *   asn1_size_length()        - Figure out the number of bytes needed for a
 *                               length value.
 *   asn1_size_oid()           - Figure out the numebr of bytes needed for an
 *                               OID value.
 *   asn1_size_packed()        - Figure out the number of bytes needed for a
 *                               packed integer value.
 *   compare_cache()           - Compare two cache entries.
 *   debug_printf()            - Display some debugging information.
 *   fix_make_model()          - Fix common problems in the make-and-model
 *                               string.
 *   free_array()              - Free an array of strings.
 *   free_cache()              - Free the array of cached devices.
 *   get_interface_addresses() - Get the broadcast address(es) associated
 *                               with an interface.
 *   hex_debug()               - Output hex debugging data...
 *   list_device()             - List a device we found...
 *   open_snmp_socket()        - Open the SNMP broadcast socket.
 *   password_cb()             - Handle authentication requests.
 *   probe_device()            - Probe a device to discover whether it is a
 *                               printer.
 *   read_snmp_conf()          - Read the snmp.conf file.
 *   read_snmp_response()      - Read and parse a SNMP response...
 *   run_time()                - Return the total running time...
 *   scan_devices()            - Scan for devices using SNMP.
 *   send_snmp_query()         - Send an SNMP query packet.
 *   try_connect()             - Try connecting on a port...
 *   update_cache()            - Update a cached device...
 */

/*
 * Include necessary headers.
 */

#include <cups/http-private.h>
#include "backend-private.h"
#include <cups/array.h>
#include <cups/file.h>
#include <regex.h>


/*
 * This backend implements SNMP printer discovery.  It uses a broadcast-
 * based approach to get SNMP response packets from potential printers,
 * tries a mDNS lookup (Mac OS X only at present), a URI lookup based on
 * the device description string, and finally a probe of port 9100
 * (AppSocket) and 515 (LPD).
 *
 * The current focus is on printers with internal network cards, although
 * the code also works with many external print servers as well.  Future
 * versions will support scanning for vendor-specific SNMP OIDs and the
 * new PWG Port Monitor MIB and not just the Host MIB OIDs.
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
 *     MaxRunTime 10
 *
 * This backend is known to work with the following network printers and
 * print servers:
 *
 *     Axis OfficeBasic, 5400, 5600
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
 * Constants...
 */

#define SNMP_PORT		161	/* SNMP well-known port */
#define SNMP_MAX_OID		64	/* Maximum number of OID numbers */
#define SNMP_MAX_PACKET		1472	/* Maximum size of SNMP packet */
#define SNMP_MAX_STRING		512	/* Maximum size of string */
#define SNMP_VERSION_1		0	/* SNMPv1 */

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
		*make_and_model;	/* device-make-and-model */
} snmp_cache_t;

typedef struct snmp_packet_s		/**** SNMP packet ****/
{
  const char	*error;			/* Encode/decode error */
  int		version;		/* Version number */
  char		community[SNMP_MAX_STRING];
					/* Community name */
  int		request_type;		/* Request type */
  int		request_id;		/* request-id value */
  int		error_status;		/* error-status value */
  int		error_index;		/* error-index value */
  int		object_name[SNMP_MAX_OID];
					/* object-name value */
  int		object_type;		/* object-value type */
  union
  {
    int		boolean;		/* Boolean value */
    int		integer;		/* Integer value */
    int		oid[SNMP_MAX_OID];	/* OID value */
    char	string[SNMP_MAX_STRING];/* String value */
  }		object_value;		/* object-value value */
} snmp_packet_t;


/*
 * Private CUPS API to set the last error...
 */

extern void	_cupsSetError(ipp_status_t status, const char *message);


/*
 * Local functions...
 */

static char		*add_array(cups_array_t *a, const char *s);
static void		add_cache(http_addr_t *addr, const char *addrname,
			          const char *uri, const char *id,
				  const char *make_and_model);
static device_uri_t	*add_device_uri(char *value);
static void		alarm_handler(int sig);
static int		asn1_decode_snmp(unsigned char *buffer, size_t len,
			                 snmp_packet_t *packet);
static void		asn1_debug(unsigned char *buffer, size_t len,
			           int indent);
static int		asn1_encode_snmp(unsigned char *buffer, size_t len,
			                 snmp_packet_t *packet);
static int		asn1_get_integer(unsigned char **buffer,
			                 unsigned char *bufend,
			                 int length);
static int		asn1_get_oid(unsigned char **buffer,
			             unsigned char *bufend,
				     int length, int *oid, int oidsize);
static int		asn1_get_packed(unsigned char **buffer,
			                unsigned char *bufend);
static char		*asn1_get_string(unsigned char **buffer,
			                 unsigned char *bufend,
			                 int length, char *string,
			                 int strsize);
static int		asn1_get_length(unsigned char **buffer,
			                unsigned char *bufend);
static int		asn1_get_type(unsigned char **buffer,
			              unsigned char *bufend);
static void		asn1_set_integer(unsigned char **buffer,
			                 int integer);
static void		asn1_set_length(unsigned char **buffer,
			                int length);
static void		asn1_set_oid(unsigned char **buffer,
			             const int *oid);
static void		asn1_set_packed(unsigned char **buffer,
			                int integer);
static int		asn1_size_integer(int integer);
static int		asn1_size_length(int length);
static int		asn1_size_oid(const int *oid);
static int		asn1_size_packed(int integer);
static int		compare_cache(snmp_cache_t *a, snmp_cache_t *b);
static void		debug_printf(const char *format, ...);
static void		fix_make_model(char *make_model,
			               const char *old_make_model,
				       int make_model_size);
static void		free_array(cups_array_t *a);
static void		free_cache(void);
static http_addrlist_t	*get_interface_addresses(const char *ifname);
static void		hex_debug(unsigned char *buffer, size_t len);
static void		list_device(snmp_cache_t *cache);
static int		open_snmp_socket(void);
static const char	*password_cb(const char *prompt);
static void		probe_device(snmp_cache_t *device);
static void		read_snmp_conf(const char *address);
static void		read_snmp_response(int fd);
static double		run_time(void);
static void		scan_devices(int fd);
static void		send_snmp_query(int fd, http_addr_t *addr, int version,
			                const char *community,
					const unsigned request_id,
					const int *oid);
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
static int		DeviceDescOID[] = { 1, 3, 6, 1, 2, 1, 25, 3,
			                    2, 1, 3, 1, 0 };
static unsigned		DeviceDescRequest;
static int		DeviceTypeOID[] = { 1, 3, 6, 1, 2, 1, 25, 3,
			                    2, 1, 2, 1, 0 };
static unsigned		DeviceTypeRequest;
static cups_array_t	*DeviceURIs = NULL;
static int		HostNameLookups = 0;
static int		MaxRunTime = 10;
static struct timeval	StartTime;


/*
 * 'main()' - Discover printers via SNMP.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments (6 or 7) */
     char *argv[])			/* I - Command-line arguments */
{
  int		fd;			/* SNMP socket */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Check command-line options...
  */

  if (argc > 2)
  {
    fputs(_("Usage: snmp [host-or-ip-address]\n"), stderr);
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
 * 'asn1_decode_snmp()' - Decode a SNMP packet.
 */

static int				/* O - 0 on success, -1 on error */
asn1_decode_snmp(unsigned char *buffer,	/* I - Buffer */
                 size_t        len,	/* I - Size of buffer */
                 snmp_packet_t *packet)	/* I - SNMP packet */
{
  unsigned char	*bufptr,		/* Pointer into the data */
		*bufend;		/* End of data */
  int		length;			/* Length of value */


 /*
  * Initialize the decoding...
  */

  memset(packet, 0, sizeof(snmp_packet_t));

  bufptr = buffer;
  bufend = buffer + len;

  if (asn1_get_type(&bufptr, bufend) != ASN1_SEQUENCE)
    packet->error = "Packet does not start with SEQUENCE";
  else if (asn1_get_length(&bufptr, bufend) == 0)
    packet->error = "SEQUENCE uses indefinite length";
  else if (asn1_get_type(&bufptr, bufend) != ASN1_INTEGER)
    packet->error = "No version number";
  else if ((length = asn1_get_length(&bufptr, bufend)) == 0)
    packet->error = "Version uses indefinite length";
  else if ((packet->version = asn1_get_integer(&bufptr, bufend, length))
               != SNMP_VERSION_1)
    packet->error = "Bad SNMP version number";
  else if (asn1_get_type(&bufptr, bufend) != ASN1_OCTET_STRING)
    packet->error = "No community name";
  else if ((length = asn1_get_length(&bufptr, bufend)) == 0)
    packet->error = "Community name uses indefinite length";
  else
  {
    asn1_get_string(&bufptr, bufend, length, packet->community,
                    sizeof(packet->community));

    if ((packet->request_type = asn1_get_type(&bufptr, bufend))
            != ASN1_GET_RESPONSE)
      packet->error = "Packet does not contain a Get-Response-PDU";
    else if (asn1_get_length(&bufptr, bufend) == 0)
      packet->error = "Get-Response-PDU uses indefinite length";
    else if (asn1_get_type(&bufptr, bufend) != ASN1_INTEGER)
      packet->error = "No request-id";
    else if ((length = asn1_get_length(&bufptr, bufend)) == 0)
      packet->error = "request-id uses indefinite length";
    else
    {
      packet->request_id = asn1_get_integer(&bufptr, bufend, length);

      if (asn1_get_type(&bufptr, bufend) != ASN1_INTEGER)
	packet->error = "No error-status";
      else if ((length = asn1_get_length(&bufptr, bufend)) == 0)
	packet->error = "error-status uses indefinite length";
      else
      {
	packet->error_status = asn1_get_integer(&bufptr, bufend, length);

	if (asn1_get_type(&bufptr, bufend) != ASN1_INTEGER)
	  packet->error = "No error-index";
	else if ((length = asn1_get_length(&bufptr, bufend)) == 0)
	  packet->error = "error-index uses indefinite length";
	else
	{
	  packet->error_index = asn1_get_integer(&bufptr, bufend, length);

          if (asn1_get_type(&bufptr, bufend) != ASN1_SEQUENCE)
	    packet->error = "No variable-bindings SEQUENCE";
	  else if (asn1_get_length(&bufptr, bufend) == 0)
	    packet->error = "variable-bindings uses indefinite length";
	  else if (asn1_get_type(&bufptr, bufend) != ASN1_SEQUENCE)
	    packet->error = "No VarBind SEQUENCE";
	  else if (asn1_get_length(&bufptr, bufend) == 0)
	    packet->error = "VarBind uses indefinite length";
	  else if (asn1_get_type(&bufptr, bufend) != ASN1_OID)
	    packet->error = "No name OID";
	  else if ((length = asn1_get_length(&bufptr, bufend)) == 0)
	    packet->error = "Name OID uses indefinite length";
          else
	  {
	    asn1_get_oid(&bufptr, bufend, length, packet->object_name,
	                 SNMP_MAX_OID);

            packet->object_type = asn1_get_type(&bufptr, bufend);

	    if ((length = asn1_get_length(&bufptr, bufend)) == 0 &&
	        packet->object_type != ASN1_NULL_VALUE &&
	        packet->object_type != ASN1_OCTET_STRING)
	      packet->error = "Value uses indefinite length";
	    else
	    {
	      switch (packet->object_type)
	      {
	        case ASN1_BOOLEAN :
		    packet->object_value.boolean =
		        asn1_get_integer(&bufptr, bufend, length);
	            break;

	        case ASN1_INTEGER :
		    packet->object_value.integer =
		        asn1_get_integer(&bufptr, bufend, length);
	            break;

		case ASN1_NULL_VALUE :
		    break;

	        case ASN1_OCTET_STRING :
		    asn1_get_string(&bufptr, bufend, length,
		                    packet->object_value.string,
				    SNMP_MAX_STRING);
	            break;

	        case ASN1_OID :
		    asn1_get_oid(&bufptr, bufend, length,
		                 packet->object_value.oid, SNMP_MAX_OID);
	            break;

                default :
		    packet->error = "Unsupported value type";
		    break;
	      }
	    }
          }
	}
      }
    }
  }

  return (packet->error ? -1 : 0);
}


/*
 * 'asn1_debug()' - Decode an ASN1-encoded message.
 */

static void
asn1_debug(unsigned char *buffer,	/* I - Buffer */
           size_t        len,		/* I - Length of buffer */
           int           indent)	/* I - Indentation */
{
  int		i;			/* Looping var */
  unsigned char	*bufend;		/* End of buffer */
  int		integer;		/* Number value */
  int		oid[SNMP_MAX_OID];	/* OID value */
  char		string[SNMP_MAX_STRING];/* String value */
  unsigned char	value_type;		/* Type of value */
  int		value_length;		/* Length of value */


  bufend = buffer + len;

  while (buffer < bufend)
  {
   /*
    * Get value type...
    */

    value_type   = asn1_get_type(&buffer, bufend);
    value_length = asn1_get_length(&buffer, bufend);

    switch (value_type)
    {
      case ASN1_BOOLEAN :
          integer = asn1_get_integer(&buffer, bufend, value_length);

          fprintf(stderr, "DEBUG: %*sBOOLEAN %d bytes %d\n", indent, "",
	          value_length, integer);
          break;

      case ASN1_INTEGER :
          integer = asn1_get_integer(&buffer, bufend, value_length);

          fprintf(stderr, "DEBUG: %*sINTEGER %d bytes %d\n", indent, "",
	          value_length, integer);
          break;

      case ASN1_OCTET_STRING :
          fprintf(stderr, "DEBUG: %*sOCTET STRING %d bytes \"%s\"\n", indent, "",
	          value_length, asn1_get_string(&buffer, bufend,
		                                value_length, string,
						sizeof(string)));
          break;

      case ASN1_NULL_VALUE :
          fprintf(stderr, "DEBUG: %*sNULL VALUE %d bytes\n", indent, "",
	          value_length);

	  buffer += value_length;
          break;

      case ASN1_OID :
          asn1_get_oid(&buffer, bufend, value_length, oid, SNMP_MAX_OID);

          fprintf(stderr, "DEBUG: %*sOID %d bytes ", indent, "",
	          value_length);
	  for (i = 0; oid[i]; i ++)
	    fprintf(stderr, ".%d", oid[i]);
	  putc('\n', stderr);
          break;

      case ASN1_SEQUENCE :
          fprintf(stderr, "DEBUG: %*sSEQUENCE %d bytes\n", indent, "",
	          value_length);
          asn1_debug(buffer, value_length, indent + 4);

	  buffer += value_length;
          break;

      case ASN1_GET_REQUEST :
          fprintf(stderr, "DEBUG: %*sGet-Request-PDU %d bytes\n", indent, "",
	          value_length);
          asn1_debug(buffer, value_length, indent + 4);

	  buffer += value_length;
          break;

      case ASN1_GET_RESPONSE :
          fprintf(stderr, "DEBUG: %*sGet-Response-PDU %d bytes\n", indent, "",
	          value_length);
          asn1_debug(buffer, value_length, indent + 4);

	  buffer += value_length;
          break;

      default :
          fprintf(stderr, "DEBUG: %*sUNKNOWN(%x) %d bytes\n", indent, "",
	          value_type, value_length);

	  buffer += value_length;
          break;
    }
  }
}
          

/*
 * 'asn1_encode_snmp()' - Encode a SNMP packet.
 */

static int				/* O - Length on success, -1 on error */
asn1_encode_snmp(unsigned char *buffer,	/* I - Buffer */
                 size_t        bufsize,	/* I - Size of buffer */
                 snmp_packet_t *packet)	/* I - SNMP packet */
{
  unsigned char	*bufptr;		/* Pointer into buffer */
  int		total,			/* Total length */
		msglen,			/* Length of entire message */
		commlen,		/* Length of community string */
		reqlen,			/* Length of request */
		listlen,		/* Length of variable list */
		varlen,			/* Length of variable */
		namelen,		/* Length of object name OID */
		valuelen;		/* Length of object value */


 /*
  * Get the lengths of the community string, OID, and message...
  */

  namelen = asn1_size_oid(packet->object_name);

  switch (packet->object_type)
  {
    case ASN1_NULL_VALUE :
        valuelen = 0;
	break;

    case ASN1_BOOLEAN :
        valuelen = asn1_size_integer(packet->object_value.boolean);
	break;

    case ASN1_INTEGER :
        valuelen = asn1_size_integer(packet->object_value.integer);
	break;

    case ASN1_OCTET_STRING :
        valuelen = strlen(packet->object_value.string);
	break;

    case ASN1_OID :
        valuelen = asn1_size_oid(packet->object_value.oid);
	break;

    default :
        packet->error = "Unknown object type";
        return (-1);
  }

  varlen  = 1 + asn1_size_length(namelen) + namelen +
            1 + asn1_size_length(valuelen) + valuelen;
  listlen = 1 + asn1_size_length(varlen) + varlen;
  reqlen  = 2 + asn1_size_integer(packet->request_id) +
            2 + asn1_size_integer(packet->error_status) +
            2 + asn1_size_integer(packet->error_index) +
            1 + asn1_size_length(listlen) + listlen;
  commlen = strlen(packet->community);
  msglen  = 2 + asn1_size_integer(packet->version) +
            1 + asn1_size_length(commlen) + commlen +
	    1 + asn1_size_length(reqlen) + reqlen;
  total   = 1 + asn1_size_length(msglen) + msglen;

  if (total > bufsize)
  {
    packet->error = "Message too large for buffer";
    return (-1);
  }

 /*
  * Then format the message...
  */

  bufptr = buffer;

  *bufptr++ = ASN1_SEQUENCE;		/* SNMPv1 message header */
  asn1_set_length(&bufptr, msglen);

  asn1_set_integer(&bufptr, packet->version);
					/* version */

  *bufptr++ = ASN1_OCTET_STRING;	/* community */
  asn1_set_length(&bufptr, commlen);
  memcpy(bufptr, packet->community, commlen);
  bufptr += commlen;

  *bufptr++ = packet->request_type;	/* Get-Request-PDU */
  asn1_set_length(&bufptr, reqlen);

  asn1_set_integer(&bufptr, packet->request_id);

  asn1_set_integer(&bufptr, packet->error_status);

  asn1_set_integer(&bufptr, packet->error_index);

  *bufptr++ = ASN1_SEQUENCE;		/* variable-bindings */
  asn1_set_length(&bufptr, listlen);

  *bufptr++ = ASN1_SEQUENCE;		/* variable */
  asn1_set_length(&bufptr, varlen);

  asn1_set_oid(&bufptr, packet->object_name);
					/* ObjectName */

  switch (packet->object_type)
  {
    case ASN1_NULL_VALUE :
	*bufptr++ = ASN1_NULL_VALUE;	/* ObjectValue */
	*bufptr++ = 0;			/* Length */
        break;

    case ASN1_BOOLEAN :
        asn1_set_integer(&bufptr, packet->object_value.boolean);
	break;

    case ASN1_INTEGER :
        asn1_set_integer(&bufptr, packet->object_value.integer);
	break;

    case ASN1_OCTET_STRING :
        *bufptr++ = ASN1_OCTET_STRING;
	asn1_set_length(&bufptr, valuelen);
	memcpy(bufptr, packet->object_value.string, valuelen);
	bufptr += valuelen;
	break;

    case ASN1_OID :
        asn1_set_oid(&bufptr, packet->object_value.oid);
	break;
  }

  return (bufptr - buffer);
}


/*
 * 'asn1_get_integer()' - Get an integer value.
 */

static int				/* O  - Integer value */
asn1_get_integer(
    unsigned char **buffer,		/* IO - Pointer in buffer */
    unsigned char *bufend,		/* I  - End of buffer */
    int           length)		/* I  - Length of value */
{
  int	value;				/* Integer value */


  for (value = 0;
       length > 0 && *buffer < bufend;
       length --, (*buffer) ++)
    value = (value << 8) | **buffer;

  return (value);
}


/*
 * 'asn1_get_length()' - Get a value length.
 */

static int				/* O  - Length */
asn1_get_length(unsigned char **buffer,	/* IO - Pointer in buffer */
		unsigned char *bufend)	/* I  - End of buffer */
{
  int	length;				/* Length */


  length = **buffer;
  (*buffer) ++;

  if (length & 128)
    length = asn1_get_integer(buffer, bufend, length & 127);

  return (length);
}


/*
 * 'asn1_get_oid()' - Get an OID value.
 */

static int				/* O  - Last OID number */
asn1_get_oid(
    unsigned char **buffer,		/* IO - Pointer in buffer */
    unsigned char *bufend,		/* I  - End of buffer */
    int           length,		/* I  - Length of value */
    int           *oid,			/* I  - OID buffer */
    int           oidsize)		/* I  - Size of OID buffer */
{
  unsigned char	*valend;		/* End of value */
  int		*oidend;		/* End of OID buffer */
  int		number;			/* OID number */


  valend = *buffer + length;
  oidend = oid + oidsize - 1;

  if (valend > bufend)
    valend = bufend;

  number = asn1_get_packed(buffer, bufend);

  if (number < 80)
  {
    *oid++ = number / 40;
    number = number % 40;
    *oid++ = number;
  }
  else
  {
    *oid++ = 2;
    number -= 80;
    *oid++ = number;
  }

  while (*buffer < valend)
  {
    number = asn1_get_packed(buffer, bufend);

    if (oid < oidend)
      *oid++ = number;
  }

  *oid = 0;

  return (number);
}


/*
 * 'asn1_get_packed()' - Get a packed integer value.
 */

static int				/* O  - Value */
asn1_get_packed(
    unsigned char **buffer,		/* IO - Pointer in buffer */
    unsigned char *bufend)		/* I  - End of buffer */
{
  int	value;				/* Value */


  value = 0;

  while ((**buffer & 128) && *buffer < bufend)
  {
    value = (value << 7) | (**buffer & 127);
    (*buffer) ++;
  }

  if (*buffer < bufend)
  {
    value = (value << 7) | **buffer;
    (*buffer) ++;
  }

  return (value);
}


/*
 * 'asn1_get_string()' - Get a string value.
 */

static char *				/* O  - String */
asn1_get_string(
    unsigned char **buffer,		/* IO - Pointer in buffer */
    unsigned char *bufend,		/* I  - End of buffer */
    int           length,		/* I  - Value length */
    char          *string,		/* I  - String buffer */
    int           strsize)		/* I  - String buffer size */
{
  if (length < 0)
  {
   /*
    * Disallow negative lengths!
    */

    fprintf(stderr, "ERROR: Bad ASN1 string length %d!\n", length);
    *string = '\0';
  }
  else if (length < strsize)
  {
   /*
    * String is smaller than the buffer...
    */

    if (length > 0)
      memcpy(string, *buffer, length);

    string[length] = '\0';
  }
  else
  {
   /*
    * String is larger than the buffer...
    */

    memcpy(string, buffer, strsize - 1);
    string[strsize - 1] = '\0';
  }

  if (length > 0)
    (*buffer) += length;

  return (string);
}


/*
 * 'asn1_get_type()' - Get a value type.
 */

static int				/* O  - Type */
asn1_get_type(unsigned char **buffer,	/* IO - Pointer in buffer */
	      unsigned char *bufend)	/* I  - End of buffer */
{
  int	type;				/* Type */


  type = **buffer;
  (*buffer) ++;

  if ((type & 31) == 31)
    type = asn1_get_packed(buffer, bufend);

  return (type);
}


/*
 * 'asn1_set_integer()' - Set an integer value.
 */

static void
asn1_set_integer(unsigned char **buffer,/* IO - Pointer in buffer */
                 int           integer)	/* I  - Integer value */
{
  **buffer = ASN1_INTEGER;
  (*buffer) ++;

  if (integer > 0x7fffff || integer < -0x800000)
  {
    **buffer = 4;
    (*buffer) ++;
    **buffer = integer >> 24;
    (*buffer) ++;
    **buffer = integer >> 16;
    (*buffer) ++;
    **buffer = integer >> 8;
    (*buffer) ++;
    **buffer = integer;
    (*buffer) ++;
  }
  else if (integer > 0x7fff || integer < -0x8000)
  {
    **buffer = 3;
    (*buffer) ++;
    **buffer = integer >> 16;
    (*buffer) ++;
    **buffer = integer >> 8;
    (*buffer) ++;
    **buffer = integer;
    (*buffer) ++;
  }
  else if (integer > 0x7f || integer < -0x80)
  {
    **buffer = 2;
    (*buffer) ++;
    **buffer = integer >> 8;
    (*buffer) ++;
    **buffer = integer;
    (*buffer) ++;
  }
  else
  {
    **buffer = 1;
    (*buffer) ++;
    **buffer = integer;
    (*buffer) ++;
  }
}


/*
 * 'asn1_set_length()' - Set a value length.
 */

static void
asn1_set_length(unsigned char **buffer,	/* IO - Pointer in buffer */
		int           length)	/* I  - Length value */
{
  if (length > 255)
  {
    **buffer = 0x82;			/* 2-byte length */
    (*buffer) ++;
    **buffer = length >> 8;
    (*buffer) ++;
    **buffer = length;
    (*buffer) ++;
  }
  else if (length > 127)
  {
    **buffer = 0x81;			/* 1-byte length */
    (*buffer) ++;
    **buffer = length;
    (*buffer) ++;
  }
  else
  {
    **buffer = length;			/* Length */
    (*buffer) ++;
  }
}


/*
 * 'asn1_set_oid()' - Set an OID value.
 */

static void
asn1_set_oid(unsigned char **buffer,	/* IO - Pointer in buffer */
             const int     *oid)	/* I  - OID value */
{
  **buffer = ASN1_OID;
  (*buffer) ++;

  asn1_set_length(buffer, asn1_size_oid(oid));

  asn1_set_packed(buffer, oid[0] * 40 + oid[1]);

  for (oid += 2; *oid; oid ++)
    asn1_set_packed(buffer, *oid);
}


/*
 * 'asn1_set_packed()' - Set a packed integer value.
 */

static void
asn1_set_packed(unsigned char **buffer,	/* IO - Pointer in buffer */
		int           integer)	/* I  - Integer value */
{
  if (integer > 0xfffffff)
  {
    **buffer = (integer >> 28) & 0x7f;
    (*buffer) ++;
  }

  if (integer > 0x1fffff)
  {
    **buffer = (integer >> 21) & 0x7f;
    (*buffer) ++;
  }

  if (integer > 0x3fff)
  {
    **buffer = (integer >> 14) & 0x7f;
    (*buffer) ++;
  }

  if (integer > 0x7f)
  {
    **buffer = (integer >> 7) & 0x7f;
    (*buffer) ++;
  }

  **buffer = integer & 0x7f;
  (*buffer) ++;
}

/*
 * 'asn1_size_integer()' - Figure out the number of bytes needed for an
 *                         integer value.
 */

static int				/* O - Size in bytes */
asn1_size_integer(int integer)		/* I - Integer value */
{
  if (integer > 0x7fffff || integer < -0x800000)
    return (4);
  else if (integer > 0x7fff || integer < -0x8000)
    return (3);
  else if (integer > 0x7f || integer < -0x80)
    return (2);
  else
    return (1);
}


/*
 * 'asn1_size_length()' - Figure out the number of bytes needed for a
 *                        length value.
 */

static int				/* O - Size in bytes */
asn1_size_length(int length)		/* I - Length value */
{
  if (length > 0xff)
    return (3);
  else if (length > 0x7f)
    return (2);
  else
    return (1);
}


/*
 * 'asn1_size_oid()' - Figure out the numebr of bytes needed for an
 *                     OID value.
 */

static int				/* O - Size in bytes */
asn1_size_oid(const int *oid)		/* I - OID value */
{
  int	length;				/* Length of value */


  for (length = asn1_size_packed(oid[0] * 40 + oid[1]), oid += 2; *oid; oid ++)
    length += asn1_size_packed(*oid);

  return (length);
}


/*
 * 'asn1_size_packed()' - Figure out the number of bytes needed for a
 *                        packed integer value.
 */

static int				/* O - Size in bytes */
asn1_size_packed(int integer)		/* I - Integer value */
{
  if (integer > 0xfffffff)
    return (5);
  else if (integer > 0x1fffff)
    return (4);
  else if (integer > 0x3fff)
    return (3);
  else if (integer > 0x7f)
    return (2);
  else
    return (1);
}


/*
 * 'compare_cache()' - Compare two cache entries.
 */

static int				/* O - Result of comparison */
compare_cache(snmp_cache_t *a,		/* I - First cache entry */
              snmp_cache_t *b)		/* I - Second cache entry */
{
  return (strcasecmp(a->addrname, b->addrname));
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

  if (!strncasecmp(old_make_model, "Hewlett-Packard", 15))
  {
   /*
    * Strip leading Hewlett-Packard and hp prefixes and replace
    * with a single HP manufacturer prefix...
    */

    mmptr = (char *)old_make_model + 15;

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
  else if (!strncasecmp(old_make_model, "officejet", 9))
    snprintf(make_model, make_model_size, "HP OfficeJet%s", old_make_model + 9);
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
 * 'hex_debug()' - Output hex debugging data...
 */

static void
hex_debug(unsigned char *buffer,	/* I - Buffer */
          size_t        len)		/* I - Number of bytes */
{
  int	col;				/* Current column */


  fputs("DEBUG: Hex dump of packet:\n", stderr);

  for (col = 0; len > 0; col ++, buffer ++, len --)
  {
    if ((col & 15) == 0)
      fprintf(stderr, "DEBUG: %04X ", col);

    fprintf(stderr, " %02X", *buffer);

    if ((col & 15) == 15)
      putc('\n', stderr);
  }

  if (col & 15)
    putc('\n', stderr);
}


/*
 * 'list_device()' - List a device we found...
 */

static void
list_device(snmp_cache_t *cache)	/* I - Cached device */
{
  if (cache->uri)
    printf("network %s \"%s\" \"%s %s\" \"%s\"\n",
           cache->uri,
	   cache->make_and_model ? cache->make_and_model : "Unknown",
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
  * TODO: Try an mDNS query first, and then fallback on direct probes...
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
    if (!regexec(&(device_uri->re), device->make_and_model, 0, NULL, 0))
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
      else if (!strcasecmp(line, "Address"))
      {
        if (!address)
          add_array(Addresses, value);
      }
      else if (!strcasecmp(line, "Community"))
        add_array(Communities, value);
      else if (!strcasecmp(line, "DebugLevel"))
        DebugLevel = atoi(value);
      else if (!strcasecmp(line, "DeviceURI"))
      {
        if (*value != '\"')
	  fprintf(stderr,
	          "ERROR: Missing double quote for regular expression on "
		  "line %d of %s!\n", linenum, filename);
        else
	  add_device_uri(value);
      }
      else if (!strcasecmp(line, "HostNameLookups"))
        HostNameLookups = !strcasecmp(value, "on") ||
	                  !strcasecmp(value, "yes") ||
	                  !strcasecmp(value, "true") ||
	                  !strcasecmp(value, "double");
      else if (!strcasecmp(line, "MaxRunTime"))
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
  unsigned char	buffer[SNMP_MAX_PACKET];/* Data packet */
  int		bytes;			/* Number of bytes received */
  http_addr_t	addr;			/* Source address */
  socklen_t	addrlen;		/* Source address length */
  char		addrname[256];		/* Source address name */
  snmp_packet_t	packet;			/* Decoded packet */
  snmp_cache_t	key,			/* Search key */
		*device;		/* Matching device */


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

  debug_printf("DEBUG: %.3f Received %d bytes from %s...\n", run_time(),
               bytes, addrname);

 /*
  * Look for the response status code in the SNMP message header...
  */

  if (asn1_decode_snmp(buffer, bytes, &packet))
  {
    fprintf(stderr, "ERROR: Bad SNMP packet from %s: %s\n",
            addrname, packet.error);

    asn1_debug(buffer, bytes, 0);
    hex_debug(buffer, bytes);

    return;
  }

  debug_printf("DEBUG: community=\"%s\"\n", packet.community);
  debug_printf("DEBUG: request-id=%d\n", packet.request_id);
  debug_printf("DEBUG: error-status=%d\n", packet.error_status);

  if (DebugLevel > 1)
    asn1_debug(buffer, bytes, 0);

  if (DebugLevel > 2)
    hex_debug(buffer, bytes);

  if (packet.error_status)
    return;

 /*
  * Find a matching device in the cache...
  */

  key.addrname = addrname;
  device       = (snmp_cache_t *)cupsArrayFind(Devices, &key);

 /*
  * Process the message...
  */

  if (packet.request_id == DeviceTypeRequest)
  {
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
    * Add the device and request the device description...
    */

    add_cache(&addr, addrname, NULL, NULL, NULL);

    send_snmp_query(fd, &addr, SNMP_VERSION_1, packet.community,
                    DeviceDescRequest, DeviceDescOID);
  }
  else if (packet.request_id == DeviceDescRequest &&
           packet.object_type == ASN1_OCTET_STRING)
  {
   /*
    * Update an existing cache entry...
    */

    char	make_model[256];	/* Make and model */


    if (!device)
    {
      debug_printf("DEBUG: Discarding device description for \"%s\"...\n",
	           addrname);
      return;
    }

   /*
    * Convert the description to a make and model string...
    */

    if (strchr(packet.object_value.string, ':') &&
        strchr(packet.object_value.string, ';'))
    {
     /*
      * Description is the IEEE-1284 device ID...
      */

      backendGetMakeModel(packet.object_value.string, make_model,
                	  sizeof(make_model));
    }
    else
    {
     /*
      * Description is plain text...
      */

      fix_make_model(make_model, packet.object_value.string,
                     sizeof(make_model));
    }

    if (device->make_and_model)
      free(device->make_and_model);

    device->make_and_model = strdup(make_model);
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
scan_devices(int fd)			/* I - SNMP socket */
{
  char			*address,	/* Current address */
			*community;	/* Current community */
  fd_set		input;		/* Input set for select() */
  struct timeval	timeout;	/* Timeout for select() */
  time_t		endtime;	/* End time for scan */
  http_addrlist_t	*addrs,		/* List of addresses */
			*addr;		/* Current address */
  snmp_cache_t		*device;	/* Current device */


 /*
  * Setup the request IDs...
  */

  gettimeofday(&StartTime, NULL);

  DeviceTypeRequest = StartTime.tv_sec;
  DeviceDescRequest = StartTime.tv_sec + 1;

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

  while (time(NULL) < endtime)
  {
    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;

    FD_SET(fd, &input);
    if (select(fd + 1, &input, NULL, NULL, &timeout) < 0)
    {
      fprintf(stderr, "ERROR: %.3f select() for %d failed: %s\n", run_time(),
              fd, strerror(errno));
      break;
    }

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
    if (MaxRunTime > 0 && run_time() >= MaxRunTime)
      break;
    else if (!device->uri)
      probe_device(device);

  debug_printf("DEBUG: %.3f Scan complete!\n", run_time());
}


/*
 * 'send_snmp_query()' - Send an SNMP query packet.
 */

static void
send_snmp_query(
    int            fd,			/* I - SNMP socket */
    http_addr_t    *addr,		/* I - Address to send to */
    int            version,		/* I - SNMP version */
    const char     *community,		/* I - Community name */
    const unsigned request_id,		/* I - Request ID */
    const int      *oid)		/* I - OID */
{
  int		i;			/* Looping var */
  snmp_packet_t	packet;			/* SNMP message packet */
  unsigned char	buffer[SNMP_MAX_PACKET];/* SNMP message buffer */
  int		bytes;			/* Size of message */
  char		addrname[32];		/* Address name */


 /*
  * Create the SNMP message...
  */

  memset(&packet, 0, sizeof(packet));

  packet.version      = version;
  packet.request_type = ASN1_GET_REQUEST;
  packet.request_id   = request_id;
  packet.object_type  = ASN1_NULL_VALUE;
  
  strlcpy(packet.community, community, sizeof(packet.community));

  for (i = 0; oid[i]; i ++)
    packet.object_name[i] = oid[i];

  bytes = asn1_encode_snmp(buffer, sizeof(buffer), &packet);

  if (bytes < 0)
  {
    fprintf(stderr, "ERROR: Unable to send SNMP query: %s\n",
            packet.error);
    return;
  }

 /*
  * Send the message...
  */

  debug_printf("DEBUG: %.3f Sending %d bytes to %s...\n", run_time(),
               bytes, httpAddrString(addr, addrname, sizeof(addrname)));
  if (DebugLevel > 1)
    asn1_debug(buffer, bytes, 0);
  if (DebugLevel > 2)
    hex_debug(buffer, bytes);

  addr->ipv4.sin_port = htons(SNMP_PORT);

  if (sendto(fd, buffer, bytes, 0, (void *)addr, sizeof(addr->ipv4)) < bytes)
    fprintf(stderr, "ERROR: Unable to send %d bytes to %s: %s\n",
            bytes, addrname, strerror(errno));
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

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    fprintf(stderr, "ERROR: Unable to create socket: %s\n",
            strerror(errno));
    return (-1);
  }

  addr->ipv4.sin_port = htons(port);

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
 * End of "$Id: snmp.c 6649 2007-07-11 21:46:42Z mike $".
 */
