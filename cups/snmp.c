/*
 * "$Id$"
 *
 *   SNMP functions for the Common UNIX Printing System (CUPS).
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
 *   cupsSNMPClose()     - Close a SNMP socket.
 *   cupsSNMPDebug()     - Enable/disable debug logging to stderr.
 *   cupsSNMPHasPrefix() - Test whetehr a SNMP response uses the specified OID
 *                         prefix.
 *   cupsSNMPIsOID()     - Test whether a SNMP response contains the specified
 *                         OID.
 *   cupsSNMPOpen()      - Open a SNMP socket.
 *   cupsSNMPRead()      - Read and parse a SNMP response...
 *   cupsSNMPWrite()     - Send an SNMP query packet.
 *   asn1_debug()        - Dump an ASN1-encoded message.
 *   asn1_decode_snmp()  - Decode a SNMP packet.
 *   asn1_encode_snmp()  - Encode a SNMP packet.
 *   asn1_get_integer()  - Get an integer value.
 *   asn1_get_length()   - Get a value length.
 *   asn1_get_oid()      - Get an OID value.
 *   asn1_get_packed()   - Get a packed integer value.
 *   asn1_get_string()   - Get a string value.
 *   asn1_get_type()     - Get a value type.
 *   asn1_set_integer()  - Set an integer value.
 *   asn1_set_length()   - Set a value length.
 *   asn1_set_oid()      - Set an OID value.
 *   asn1_set_packed()   - Set a packed integer value.
 *   asn1_size_integer() - Figure out the number of bytes needed for an integer
 *                         value.
 *   asn1_size_length()  - Figure out the number of bytes needed for a length
 *                         value.
 *   asn1_size_oid()     - Figure out the numebr of bytes needed for an OID
 *                         value.
 *   asn1_size_packed()  - Figure out the number of bytes needed for a packed
 *                         integer value.
 *   snmp_set_error()    - Set the localized error for a packet.
 */

/*
 * Include necessary headers.
 */

#include "globals.h"
#include "snmp.h"
#include <errno.h>


/*
 * Local functions...
 */

static int		asn1_decode_snmp(unsigned char *buffer, size_t len,
			                 cups_snmp_t *packet);
static void		asn1_debug(const char *prefix, unsigned char *buffer,
			           size_t len, int indent);
static int		asn1_encode_snmp(unsigned char *buffer, size_t len,
			                 cups_snmp_t *packet);
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
static void		snmp_set_error(cups_snmp_t *packet,
			               const char *message);


/*
 * 'cupsSNMPClose()' - Close a SNMP socket.
 *
 * @since CUPS 1.4@
 */

void
cupsSNMPClose(int fd)			/* I - SNMP socket file descriptor */
{
#ifdef WIN32
  closesocket(fd);
#else
  close(fd);
#endif /* WIN32 */
}


/*
 * 'cupsSNMPDebug()' - Enable/disable debug logging to stderr.
 *
 * @since CUPS 1.4@
 */

void
cupsSNMPDebug(int level)		/* I - 1 to enable debug output, 0 otherwise */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */


  cg->snmp_debug = level;
}


/*
 * 'cupsSNMPHasPrefix()' - Test whetehr a SNMP response uses the specified OID
 *                         prefix.
 *
 * The array pointed to by "prefix" is 0-terminated.
 *
 * @since CUPS 1.4@
 */

int					/* O - 1 if equal, 0 if not equal */
cupsSNMPHasPrefix(cups_snmp_t *packet,	/* I - Response packet */
                  const int   *prefix)	/* I - OID prefix */
{
  int	i;				/* Looping var */


 /*
  * Range check input...
  */

  if (!packet || !prefix)
    return (0);

 /*
  * Compare OIDs...
  */

  for (i = 0;
       i < CUPS_SNMP_MAX_OID && prefix[i] && packet->object_name[i];
       i ++)
    if (prefix[i] != packet->object_name[i])
      return (0);

  return (i < CUPS_SNMP_MAX_OID);
}


/*
 * 'cupsSNMPIsOID()' - Test whether a SNMP response contains the specified OID.
 *
 * The array pointed to by "oid" is 0-terminated.
 *
 * @since CUPS 1.4@
 */

int					/* O - 1 if equal, 0 if not equal */
cupsSNMPIsOID(cups_snmp_t *packet,	/* I - Response packet */
              const int   *oid)		/* I - OID */
{
  int	i;				/* Looping var */


 /*
  * Range check input...
  */

  if (!packet || !oid)
    return (0);

 /*
  * Compare OIDs...
  */

  for (i = 0; i < CUPS_SNMP_MAX_OID && oid[i] && packet->object_name[i]; i ++)
    if (oid[i] != packet->object_name[i])
      return (0);

  return (i < CUPS_SNMP_MAX_OID && oid[i] == packet->object_name[i]);
}


/*
 * 'cupsSNMPOpen()' - Open a SNMP socket.
 *
 * @since CUPS 1.4@
 */

int					/* O - SNMP socket file descriptor */
cupsSNMPOpen(void)
{
  int		fd;			/* SNMP socket file descriptor */
  int		val;			/* Socket option value */


 /*
  * Create the SNMP socket...
  */

  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    return (-1);

 /*
  * Set the "broadcast" flag...
  */

  val = 1;

  if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)))
  {
    close(fd);

    return (-1);
  }

  return (fd);
}


/*
 * 'cupsSNMPRead()' - Read and parse a SNMP response...
 *
 * @since CUPS 1.4@
 */

cups_snmp_t *				/* O - SNMP packet or NULL if none */
cupsSNMPRead(int         fd,		/* I - SNMP socket file descriptor */
             cups_snmp_t *packet)	/* I - SNMP packet buffer */
{
  unsigned char	buffer[CUPS_SNMP_MAX_PACKET];
					/* Data packet */
  int		bytes;			/* Number of bytes received */
  socklen_t	addrlen;		/* Source address length */


 /*
  * Range check input...
  */

  if (fd < 0 || !packet)
    return (NULL);

 /*
  * Read the response data...
  */

  addrlen = sizeof(packet->address);

  if ((bytes = recvfrom(fd, buffer, sizeof(buffer), 0,
                        (void *)&(packet->address), &addrlen)) < 0)
    return (NULL);

 /*
  * Look for the response status code in the SNMP message header...
  */

  asn1_debug("DEBUG: IN ", buffer, bytes, 0);

  asn1_decode_snmp(buffer, bytes, packet);

 /*
  * Return decoded data packet...
  */

  return (packet);
}


/*
 * 'cupsSNMPWrite()' - Send an SNMP query packet.
 *
 * The array pointed to by "oid" is 0-terminated.
 *
 * @since CUPS 1.4@
 */

int					/* O - 1 on success, 0 on error */
cupsSNMPWrite(
    int            fd,			/* I - SNMP socket */
    http_addr_t    *address,		/* I - Address to send to */
    int            version,		/* I - SNMP version */
    const char     *community,		/* I - Community name */
    cups_asn1_t    request_type,	/* I - Request type */
    const unsigned request_id,		/* I - Request ID */
    const int      *oid)		/* I - OID */
{
  int		i;			/* Looping var */
  cups_snmp_t	packet;			/* SNMP message packet */
  unsigned char	buffer[CUPS_SNMP_MAX_PACKET];
					/* SNMP message buffer */
  int		bytes;			/* Size of message */


 /*
  * Create the SNMP message...
  */

  memset(&packet, 0, sizeof(packet));

  packet.version      = version;
  packet.request_type = request_type;
  packet.request_id   = request_id;
  packet.object_type  = CUPS_ASN1_NULL_VALUE;
  
  strlcpy(packet.community, community, sizeof(packet.community));

  for (i = 0; oid[i]; i ++)
    packet.object_name[i] = oid[i];

  bytes = asn1_encode_snmp(buffer, sizeof(buffer), &packet);

  if (bytes < 0)
  {
    errno = E2BIG;

    return (0);
  }

  asn1_debug("DEBUG: OUT ", buffer, bytes, 0);

 /*
  * Send the message...
  */

#ifdef AF_INET6
  if (address->addr.sa_family == AF_INET6)
    address->ipv6.sin6_port = htons(CUPS_SNMP_PORT);
  else
#endif /* AF_INET6 */
  address->ipv4.sin_port = htons(CUPS_SNMP_PORT);

  return (sendto(fd, buffer, bytes, 0, (void *)address,
                 httpAddrLength(address)) == bytes);
}


/*
 * 'asn1_decode_snmp()' - Decode a SNMP packet.
 */

static int				/* O - 0 on success, -1 on error */
asn1_decode_snmp(unsigned char *buffer,	/* I - Buffer */
                 size_t        len,	/* I - Size of buffer */
                 cups_snmp_t   *packet)	/* I - SNMP packet */
{
  unsigned char	*bufptr,		/* Pointer into the data */
		*bufend;		/* End of data */
  int		length;			/* Length of value */


 /*
  * Initialize the decoding...
  */

  memset(packet, 0, sizeof(cups_snmp_t));

  bufptr = buffer;
  bufend = buffer + len;

  if (asn1_get_type(&bufptr, bufend) != CUPS_ASN1_SEQUENCE)
    snmp_set_error(packet, _("Packet does not start with SEQUENCE"));
  else if (asn1_get_length(&bufptr, bufend) == 0)
    snmp_set_error(packet, _("SEQUENCE uses indefinite length"));
  else if (asn1_get_type(&bufptr, bufend) != CUPS_ASN1_INTEGER)
    snmp_set_error(packet, _("No version number"));
  else if ((length = asn1_get_length(&bufptr, bufend)) == 0)
    snmp_set_error(packet, _("Version uses indefinite length"));
  else if ((packet->version = asn1_get_integer(&bufptr, bufend, length))
               != CUPS_SNMP_VERSION_1)
    snmp_set_error(packet, _("Bad SNMP version number"));
  else if (asn1_get_type(&bufptr, bufend) != CUPS_ASN1_OCTET_STRING)
    snmp_set_error(packet, _("No community name"));
  else if ((length = asn1_get_length(&bufptr, bufend)) == 0)
    snmp_set_error(packet, _("Community name uses indefinite length"));
  else
  {
    asn1_get_string(&bufptr, bufend, length, packet->community,
                    sizeof(packet->community));

    if ((packet->request_type = asn1_get_type(&bufptr, bufend))
            != CUPS_ASN1_GET_RESPONSE)
      snmp_set_error(packet, _("Packet does not contain a Get-Response-PDU"));
    else if (asn1_get_length(&bufptr, bufend) == 0)
      snmp_set_error(packet, _("Get-Response-PDU uses indefinite length"));
    else if (asn1_get_type(&bufptr, bufend) != CUPS_ASN1_INTEGER)
      snmp_set_error(packet, _("No request-id"));
    else if ((length = asn1_get_length(&bufptr, bufend)) == 0)
      snmp_set_error(packet, _("request-id uses indefinite length"));
    else
    {
      packet->request_id = asn1_get_integer(&bufptr, bufend, length);

      if (asn1_get_type(&bufptr, bufend) != CUPS_ASN1_INTEGER)
	snmp_set_error(packet, _("No error-status"));
      else if ((length = asn1_get_length(&bufptr, bufend)) == 0)
	snmp_set_error(packet, _("error-status uses indefinite length"));
      else
      {
	packet->error_status = asn1_get_integer(&bufptr, bufend, length);

	if (asn1_get_type(&bufptr, bufend) != CUPS_ASN1_INTEGER)
	  snmp_set_error(packet, _("No error-index"));
	else if ((length = asn1_get_length(&bufptr, bufend)) == 0)
	  snmp_set_error(packet, _("error-index uses indefinite length"));
	else
	{
	  packet->error_index = asn1_get_integer(&bufptr, bufend, length);

          if (asn1_get_type(&bufptr, bufend) != CUPS_ASN1_SEQUENCE)
	    snmp_set_error(packet, _("No variable-bindings SEQUENCE"));
	  else if (asn1_get_length(&bufptr, bufend) == 0)
	    snmp_set_error(packet,
	                   _("variable-bindings uses indefinite length"));
	  else if (asn1_get_type(&bufptr, bufend) != CUPS_ASN1_SEQUENCE)
	    snmp_set_error(packet, _("No VarBind SEQUENCE"));
	  else if (asn1_get_length(&bufptr, bufend) == 0)
	    snmp_set_error(packet, _("VarBind uses indefinite length"));
	  else if (asn1_get_type(&bufptr, bufend) != CUPS_ASN1_OID)
	    snmp_set_error(packet, _("No name OID"));
	  else if ((length = asn1_get_length(&bufptr, bufend)) == 0)
	    snmp_set_error(packet, _("Name OID uses indefinite length"));
          else
	  {
	    asn1_get_oid(&bufptr, bufend, length, packet->object_name,
	                 CUPS_SNMP_MAX_OID);

            packet->object_type = asn1_get_type(&bufptr, bufend);

	    if ((length = asn1_get_length(&bufptr, bufend)) == 0 &&
	        packet->object_type != CUPS_ASN1_NULL_VALUE &&
	        packet->object_type != CUPS_ASN1_OCTET_STRING)
	      snmp_set_error(packet, _("Value uses indefinite length"));
	    else
	    {
	      switch (packet->object_type)
	      {
	        case CUPS_ASN1_BOOLEAN :
		    packet->object_value.boolean =
		        asn1_get_integer(&bufptr, bufend, length);
	            break;

	        case CUPS_ASN1_INTEGER :
		    packet->object_value.integer =
		        asn1_get_integer(&bufptr, bufend, length);
	            break;

		case CUPS_ASN1_NULL_VALUE :
		    break;

	        case CUPS_ASN1_OCTET_STRING :
		    asn1_get_string(&bufptr, bufend, length,
		                    packet->object_value.string,
				    CUPS_SNMP_MAX_STRING);
	            break;

	        case CUPS_ASN1_OID :
		    asn1_get_oid(&bufptr, bufend, length,
		                 packet->object_value.oid, CUPS_SNMP_MAX_OID);
	            break;

                default :
		    snmp_set_error(packet, _("Unsupported value type"));
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
asn1_debug(const char    *prefix,	/* I - Prefix string */
           unsigned char *buffer,	/* I - Buffer */
           size_t        len,		/* I - Length of buffer */
           int           indent)	/* I - Indentation */
{
  int		i;			/* Looping var */
  unsigned char	*bufend;		/* End of buffer */
  int		integer;		/* Number value */
  int		oid[CUPS_SNMP_MAX_OID];	/* OID value */
  char		string[CUPS_SNMP_MAX_STRING];
					/* String value */
  unsigned char	value_type;		/* Type of value */
  int		value_length;		/* Length of value */
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */


  if (cg->snmp_debug <= 0)
    return;

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
      case CUPS_ASN1_BOOLEAN :
          integer = asn1_get_integer(&buffer, bufend, value_length);

          fprintf(stderr, "%s%*sBOOLEAN %d bytes %d\n", prefix, indent, "",
	          value_length, integer);
          break;

      case CUPS_ASN1_INTEGER :
          integer = asn1_get_integer(&buffer, bufend, value_length);

          fprintf(stderr, "%s%*sINTEGER %d bytes %d\n", prefix, indent, "",
	          value_length, integer);
          break;

      case CUPS_ASN1_OCTET_STRING :
          fprintf(stderr, "%s%*sOCTET STRING %d bytes \"%s\"\n", prefix,
	          indent, "", value_length,
		  asn1_get_string(&buffer, bufend, value_length, string,
				  sizeof(string)));
          break;

      case CUPS_ASN1_NULL_VALUE :
          fprintf(stderr, "%s%*sNULL VALUE %d bytes\n", prefix, indent, "",
	          value_length);

	  buffer += value_length;
          break;

      case CUPS_ASN1_OID :
          asn1_get_oid(&buffer, bufend, value_length, oid, CUPS_SNMP_MAX_OID);

          fprintf(stderr, "%s%*sOID %d bytes ", prefix, indent, "",
	          value_length);
	  for (i = 0; oid[i]; i ++)
	    fprintf(stderr, ".%d", oid[i]);
	  putc('\n', stderr);
          break;

      case CUPS_ASN1_SEQUENCE :
          fprintf(stderr, "%s%*sSEQUENCE %d bytes\n", prefix, indent, "",
	          value_length);
          asn1_debug(prefix, buffer, value_length, indent + 4);

	  buffer += value_length;
          break;

      case CUPS_ASN1_GET_REQUEST :
          fprintf(stderr, "%s%*sGet-Request-PDU %d bytes\n", prefix, indent, "",
	          value_length);
          asn1_debug(prefix, buffer, value_length, indent + 4);

	  buffer += value_length;
          break;

      case CUPS_ASN1_GET_RESPONSE :
          fprintf(stderr, "%s%*sGet-Response-PDU %d bytes\n", prefix, indent,
	          "", value_length);
          asn1_debug(prefix, buffer, value_length, indent + 4);

	  buffer += value_length;
          break;

      default :
          fprintf(stderr, "%s%*sUNKNOWN(%x) %d bytes\n", prefix, indent, "",
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
                 cups_snmp_t   *packet)	/* I - SNMP packet */
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
    case CUPS_ASN1_NULL_VALUE :
        valuelen = 0;
	break;

    case CUPS_ASN1_BOOLEAN :
        valuelen = asn1_size_integer(packet->object_value.boolean);
	break;

    case CUPS_ASN1_INTEGER :
        valuelen = asn1_size_integer(packet->object_value.integer);
	break;

    case CUPS_ASN1_OCTET_STRING :
        valuelen = strlen(packet->object_value.string);
	break;

    case CUPS_ASN1_OID :
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

  *bufptr++ = CUPS_ASN1_SEQUENCE;	/* SNMPv1 message header */
  asn1_set_length(&bufptr, msglen);

  asn1_set_integer(&bufptr, packet->version);
					/* version */

  *bufptr++ = CUPS_ASN1_OCTET_STRING;	/* community */
  asn1_set_length(&bufptr, commlen);
  memcpy(bufptr, packet->community, commlen);
  bufptr += commlen;

  *bufptr++ = packet->request_type;	/* Get-Request-PDU */
  asn1_set_length(&bufptr, reqlen);

  asn1_set_integer(&bufptr, packet->request_id);

  asn1_set_integer(&bufptr, packet->error_status);

  asn1_set_integer(&bufptr, packet->error_index);

  *bufptr++ = CUPS_ASN1_SEQUENCE;	/* variable-bindings */
  asn1_set_length(&bufptr, listlen);

  *bufptr++ = CUPS_ASN1_SEQUENCE;	/* variable */
  asn1_set_length(&bufptr, varlen);

  asn1_set_oid(&bufptr, packet->object_name);
					/* ObjectName */

  switch (packet->object_type)
  {
    case CUPS_ASN1_NULL_VALUE :
	*bufptr++ = CUPS_ASN1_NULL_VALUE;
					/* ObjectValue */
	*bufptr++ = 0;			/* Length */
        break;

    case CUPS_ASN1_BOOLEAN :
        asn1_set_integer(&bufptr, packet->object_value.boolean);
	break;

    case CUPS_ASN1_INTEGER :
        asn1_set_integer(&bufptr, packet->object_value.integer);
	break;

    case CUPS_ASN1_OCTET_STRING :
        *bufptr++ = CUPS_ASN1_OCTET_STRING;
	asn1_set_length(&bufptr, valuelen);
	memcpy(bufptr, packet->object_value.string, valuelen);
	bufptr += valuelen;
	break;

    case CUPS_ASN1_OID :
        asn1_set_oid(&bufptr, packet->object_value.oid);
	break;

    default :
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

    memcpy(string, *buffer, strsize - 1);
    string[strsize - 1] = '\0';
  }

  if (length > 0)
    (*buffer) += length;

  return (length < 0 ? NULL : string);
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
  **buffer = CUPS_ASN1_INTEGER;
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
  **buffer = CUPS_ASN1_OID;
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
 * 'snmp_set_error()' - Set the localized error for a packet.
 */

static void
snmp_set_error(cups_snmp_t *packet,	/* I - Packet */
               const char *message)	/* I - Error message */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */


  if (!cg->lang_default)
    cg->lang_default = cupsLangDefault();

  packet->error = _cupsLangString(cg->lang_default, message);
}


/*
 * End of "$Id$".
 */
