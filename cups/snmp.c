/*
 * "$Id: snmp.c 11594 2014-02-14 20:09:01Z msweet $"
 *
 * SNMP functions for CUPS.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 2006-2007 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * "LICENSE" which should have been included with this file.  If this
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers.
 */

#include "cups-private.h"
#include "snmp-private.h"
#ifdef HAVE_POLL
#  include <poll.h>
#endif /* HAVE_POLL */


/*
 * Local functions...
 */

static void		asn1_debug(const char *prefix, unsigned char *buffer,
			           size_t len, int indent);
static int		asn1_decode_snmp(unsigned char *buffer, size_t len,
			                 cups_snmp_t *packet);
static int		asn1_encode_snmp(unsigned char *buffer, size_t len,
			                 cups_snmp_t *packet);
static int		asn1_get_integer(unsigned char **buffer,
			                 unsigned char *bufend,
			                 unsigned length);
static int		asn1_get_oid(unsigned char **buffer,
			             unsigned char *bufend,
				     unsigned length, int *oid, int oidsize);
static int		asn1_get_packed(unsigned char **buffer,
			                unsigned char *bufend);
static char		*asn1_get_string(unsigned char **buffer,
			                 unsigned char *bufend,
			                 unsigned length, char *string,
			                 size_t strsize);
static unsigned		asn1_get_length(unsigned char **buffer,
			                unsigned char *bufend);
static int		asn1_get_type(unsigned char **buffer,
			              unsigned char *bufend);
static void		asn1_set_integer(unsigned char **buffer,
			                 int integer);
static void		asn1_set_length(unsigned char **buffer,
			                unsigned length);
static void		asn1_set_oid(unsigned char **buffer,
			             const int *oid);
static void		asn1_set_packed(unsigned char **buffer,
			                int integer);
static unsigned		asn1_size_integer(int integer);
static unsigned		asn1_size_length(unsigned length);
static unsigned		asn1_size_oid(const int *oid);
static unsigned		asn1_size_packed(int integer);
static void		snmp_set_error(cups_snmp_t *packet,
			               const char *message);


/*
 * '_cupsSNMPClose()' - Close a SNMP socket.
 */

void
_cupsSNMPClose(int fd)			/* I - SNMP socket file descriptor */
{
  DEBUG_printf(("4_cupsSNMPClose(fd=%d)", fd));

  httpAddrClose(NULL, fd);
}


/*
 * '_cupsSNMPCopyOID()' - Copy an OID.
 *
 * The array pointed to by "src" is terminated by the value -1.
 */

int *					/* O - New OID */
_cupsSNMPCopyOID(int       *dst,	/* I - Destination OID */
                 const int *src,	/* I - Source OID */
		 int       dstsize)	/* I - Number of integers in dst */
{
  int	i;				/* Looping var */


  DEBUG_printf(("4_cupsSNMPCopyOID(dst=%p, src=%p, dstsize=%d)", dst, src,
                dstsize));

  for (i = 0, dstsize --; src[i] >= 0 && i < dstsize; i ++)
    dst[i] = src[i];

  dst[i] = -1;

  return (dst);
}


/*
 * '_cupsSNMPDefaultCommunity()' - Get the default SNMP community name.
 *
 * The default community name is the first community name found in the
 * snmp.conf file. If no community name is defined there, "public" is used.
 */

const char *				/* O - Default community name */
_cupsSNMPDefaultCommunity(void)
{
  cups_file_t	*fp;			/* snmp.conf file */
  char		line[1024],		/* Line from file */
		*value;			/* Value from file */
  int		linenum;		/* Line number in file */
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */


  DEBUG_puts("4_cupsSNMPDefaultCommunity()");

  if (!cg->snmp_community[0])
  {
    strlcpy(cg->snmp_community, "public", sizeof(cg->snmp_community));

    snprintf(line, sizeof(line), "%s/snmp.conf", cg->cups_serverroot);
    if ((fp = cupsFileOpen(line, "r")) != NULL)
    {
      linenum = 0;
      while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
	if (!_cups_strcasecmp(line, "Community") && value)
	{
	  strlcpy(cg->snmp_community, value, sizeof(cg->snmp_community));
	  break;
	}

      cupsFileClose(fp);
    }
  }

  DEBUG_printf(("5_cupsSNMPDefaultCommunity: Returning \"%s\"",
                cg->snmp_community));

  return (cg->snmp_community);
}


/*
 * '_cupsSNMPIsOID()' - Test whether a SNMP response contains the specified OID.
 *
 * The array pointed to by "oid" is terminated by the value -1.
 */

int					/* O - 1 if equal, 0 if not equal */
_cupsSNMPIsOID(cups_snmp_t *packet,	/* I - Response packet */
               const int   *oid)	/* I - OID */
{
  int	i;				/* Looping var */


 /*
  * Range check input...
  */

  DEBUG_printf(("4_cupsSNMPIsOID(packet=%p, oid=%p)", packet, oid));

  if (!packet || !oid)
  {
    DEBUG_puts("5_cupsSNMPIsOID: Returning 0");

    return (0);
  }

 /*
  * Compare OIDs...
  */

  for (i = 0;
       i < CUPS_SNMP_MAX_OID && oid[i] >= 0 && packet->object_name[i] >= 0;
       i ++)
    if (oid[i] != packet->object_name[i])
    {
      DEBUG_puts("5_cupsSNMPIsOID: Returning 0");

      return (0);
    }

  DEBUG_printf(("5_cupsSNMPIsOID: Returning %d",
                i < CUPS_SNMP_MAX_OID && oid[i] == packet->object_name[i]));

  return (i < CUPS_SNMP_MAX_OID && oid[i] == packet->object_name[i]);
}


/*
 * '_cupsSNMPIsOIDPrefixed()' - Test whether a SNMP response uses the specified
 *                              OID prefix.
 *
 * The array pointed to by "prefix" is terminated by the value -1.
 */

int					/* O - 1 if prefixed, 0 if not prefixed */
_cupsSNMPIsOIDPrefixed(
    cups_snmp_t *packet,		/* I - Response packet */
    const int   *prefix)		/* I - OID prefix */
{
  int	i;				/* Looping var */


 /*
  * Range check input...
  */

  DEBUG_printf(("4_cupsSNMPIsOIDPrefixed(packet=%p, prefix=%p)", packet,
                prefix));

  if (!packet || !prefix)
  {
    DEBUG_puts("5_cupsSNMPIsOIDPrefixed: Returning 0");

    return (0);
  }

 /*
  * Compare OIDs...
  */

  for (i = 0;
       i < CUPS_SNMP_MAX_OID && prefix[i] >= 0 && packet->object_name[i] >= 0;
       i ++)
    if (prefix[i] != packet->object_name[i])
    {
      DEBUG_puts("5_cupsSNMPIsOIDPrefixed: Returning 0");

      return (0);
    }

  DEBUG_printf(("5_cupsSNMPIsOIDPrefixed: Returning %d",
                i < CUPS_SNMP_MAX_OID));

  return (i < CUPS_SNMP_MAX_OID);
}


/*
 * '_cupsSNMPOIDToString()' - Convert an OID to a string.
 */


char *					/* O - New string or @code NULL@ on error */
_cupsSNMPOIDToString(const int *src,	/* I - OID */
                     char      *dst,	/* I - String buffer */
                     size_t    dstsize)	/* I - Size of string buffer */
{
  char	*dstptr,			/* Pointer into string buffer */
	*dstend;			/* End of string buffer */


  DEBUG_printf(("4_cupsSNMPOIDToString(src=%p, dst=%p, dstsize=" CUPS_LLFMT ")",
                src, dst, CUPS_LLCAST dstsize));

 /*
  * Range check input...
  */

  if (!src || !dst || dstsize < 4)
    return (NULL);

 /*
  * Loop through the OID array and build a string...
  */

  for (dstptr = dst, dstend = dstptr + dstsize - 1;
       *src >= 0 && dstptr < dstend;
       src ++, dstptr += strlen(dstptr))
    snprintf(dstptr, (size_t)(dstend - dstptr + 1), ".%d", *src);

  if (*src >= 0)
    return (NULL);
  else
    return (dst);
}


/*
 * '_cupsSNMPOpen()' - Open a SNMP socket.
 */

int					/* O - SNMP socket file descriptor */
_cupsSNMPOpen(int family)		/* I - Address family - @code AF_INET@ or @code AF_INET6@ */
{
  int		fd;			/* SNMP socket file descriptor */
  int		val;			/* Socket option value */


 /*
  * Create the SNMP socket...
  */

  DEBUG_printf(("4_cupsSNMPOpen(family=%d)", family));

  if ((fd = socket(family, SOCK_DGRAM, 0)) < 0)
  {
    DEBUG_printf(("5_cupsSNMPOpen: Returning -1 (%s)", strerror(errno)));

    return (-1);
  }

 /*
  * Set the "broadcast" flag...
  */

  val = 1;

  if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, CUPS_SOCAST &val, sizeof(val)))
  {
    DEBUG_printf(("5_cupsSNMPOpen: Returning -1 (%s)", strerror(errno)));

    close(fd);

    return (-1);
  }

  DEBUG_printf(("5_cupsSNMPOpen: Returning %d", fd));

  return (fd);
}


/*
 * '_cupsSNMPRead()' - Read and parse a SNMP response.
 *
 * If "timeout" is negative, @code _cupsSNMPRead@ will wait for a response
 * indefinitely.
 */

cups_snmp_t *				/* O - SNMP packet or @code NULL@ if none */
_cupsSNMPRead(int         fd,		/* I - SNMP socket file descriptor */
              cups_snmp_t *packet,	/* I - SNMP packet buffer */
	      double      timeout)	/* I - Timeout in seconds */
{
  unsigned char	buffer[CUPS_SNMP_MAX_PACKET];
					/* Data packet */
  ssize_t	bytes;			/* Number of bytes received */
  socklen_t	addrlen;		/* Source address length */
  http_addr_t	address;		/* Source address */


 /*
  * Range check input...
  */

  DEBUG_printf(("4_cupsSNMPRead(fd=%d, packet=%p, timeout=%.1f)", fd, packet,
                timeout));

  if (fd < 0 || !packet)
  {
    DEBUG_puts("5_cupsSNMPRead: Returning NULL");

    return (NULL);
  }

 /*
  * Optionally wait for a response...
  */

  if (timeout >= 0.0)
  {
    int			ready;		/* Data ready on socket? */
#ifdef HAVE_POLL
    struct pollfd	pfd;		/* Polled file descriptor */

    pfd.fd     = fd;
    pfd.events = POLLIN;

    while ((ready = poll(&pfd, 1, (int)(timeout * 1000.0))) < 0 &&
           (errno == EINTR || errno == EAGAIN));

#else
    fd_set		input_set;	/* select() input set */
    struct timeval	stimeout;	/* select() timeout */

    do
    {
      FD_ZERO(&input_set);
      FD_SET(fd, &input_set);

      stimeout.tv_sec  = (int)timeout;
      stimeout.tv_usec = (int)((timeout - stimeout.tv_sec) * 1000000);

      ready = select(fd + 1, &input_set, NULL, NULL, &stimeout);
    }
#  ifdef WIN32
    while (ready < 0 && WSAGetLastError() == WSAEINTR);
#  else
    while (ready < 0 && (errno == EINTR || errno == EAGAIN));
#  endif /* WIN32 */
#endif /* HAVE_POLL */

   /*
    * If we don't have any data ready, return right away...
    */

    if (ready <= 0)
    {
      DEBUG_puts("5_cupsSNMPRead: Returning NULL (timeout)");

      return (NULL);
    }
  }

 /*
  * Read the response data...
  */

  addrlen = sizeof(address);

  if ((bytes = recvfrom(fd, buffer, sizeof(buffer), 0, (void *)&address,
                        &addrlen)) < 0)
  {
    DEBUG_printf(("5_cupsSNMPRead: Returning NULL (%s)", strerror(errno)));

    return (NULL);
  }

 /*
  * Look for the response status code in the SNMP message header...
  */

  asn1_debug("DEBUG: IN ", buffer, (size_t)bytes, 0);

  asn1_decode_snmp(buffer, (size_t)bytes, packet);

  memcpy(&(packet->address), &address, sizeof(packet->address));

 /*
  * Return decoded data packet...
  */

  DEBUG_puts("5_cupsSNMPRead: Returning packet");

  return (packet);
}


/*
 * '_cupsSNMPSetDebug()' - Enable/disable debug logging to stderr.
 */

void
_cupsSNMPSetDebug(int level)		/* I - 1 to enable debug output, 0 otherwise */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */


  DEBUG_printf(("4_cupsSNMPSetDebug(level=%d)", level));

  cg->snmp_debug = level;
}


/*
 * '_cupsSNMPStringToOID()' - Convert a numeric OID string to an OID array.
 *
 * This function converts a string of the form ".N.N.N.N.N" to the
 * corresponding OID array terminated by -1.
 *
 * @code NULL@ is returned if the array is not large enough or the string is
 * not a valid OID number.
 */

int *					/* O - Pointer to OID array or @code NULL@ on error */
_cupsSNMPStringToOID(const char *src,	/* I - OID string */
                     int        *dst,	/* I - OID array */
		     int        dstsize)/* I - Number of integers in OID array */
{
  int	*dstptr,			/* Pointer into OID array */
	*dstend;			/* End of OID array */


  DEBUG_printf(("4_cupsSNMPStringToOID(src=\"%s\", dst=%p, dstsize=%d)",
                src, dst, dstsize));

 /*
  * Range check input...
  */

  if (!src || !dst || dstsize < 2)
    return (NULL);

 /*
  * Skip leading "."...
  */

  if (*src == '.')
    src ++;

 /*
  * Loop to the end of the string...
  */

  for (dstend = dst + dstsize - 1, dstptr = dst, *dstptr = 0;
       *src && dstptr < dstend;
       src ++)
  {
    if (*src == '.')
    {
      dstptr ++;
      *dstptr = 0;
    }
    else if (isdigit(*src & 255))
      *dstptr = *dstptr * 10 + *src - '0';
    else
      break;
  }

  if (*src)
    return (NULL);

 /*
  * Terminate the end of the OID array and return...
  */

  dstptr[1] = -1;

  return (dst);
}


/*
 * '_cupsSNMPWalk()' - Enumerate a group of OIDs.
 *
 * This function queries all of the OIDs with the specified OID prefix,
 * calling the "cb" function for every response that is received.
 *
 * The array pointed to by "prefix" is terminated by the value -1.
 *
 * If "timeout" is negative, @code _cupsSNMPWalk@ will wait for a response
 * indefinitely.
 */

int					/* O - Number of OIDs found or -1 on error */
_cupsSNMPWalk(int            fd,	/* I - SNMP socket */
              http_addr_t    *address,	/* I - Address to query */
	      int            version,	/* I - SNMP version */
	      const char     *community,/* I - Community name */
              const int      *prefix,	/* I - OID prefix */
	      double         timeout,	/* I - Timeout for each response in seconds */
	      cups_snmp_cb_t cb,	/* I - Function to call for each response */
	      void           *data)	/* I - User data pointer that is passed to the callback function */
{
  int		count = 0;		/* Number of OIDs found */
  unsigned	request_id = 0;		/* Current request ID */
  cups_snmp_t	packet;			/* Current response packet */
  int		lastoid[CUPS_SNMP_MAX_OID];
					/* Last OID we got */


 /*
  * Range check input...
  */

  DEBUG_printf(("4_cupsSNMPWalk(fd=%d, address=%p, version=%d, "
                "community=\"%s\", prefix=%p, timeout=%.1f, cb=%p, data=%p)",
		fd, address, version, community, prefix, timeout, cb, data));

  if (fd < 0 || !address || version != CUPS_SNMP_VERSION_1 || !community ||
      !prefix || !cb)
  {
    DEBUG_puts("5_cupsSNMPWalk: Returning -1");

    return (-1);
  }

 /*
  * Copy the OID prefix and then loop until we have no more OIDs...
  */

  _cupsSNMPCopyOID(packet.object_name, prefix, CUPS_SNMP_MAX_OID);
  lastoid[0] = -1;

  for (;;)
  {
    request_id ++;

    if (!_cupsSNMPWrite(fd, address, version, community,
                        CUPS_ASN1_GET_NEXT_REQUEST, request_id,
		        packet.object_name))
    {
      DEBUG_puts("5_cupsSNMPWalk: Returning -1");

      return (-1);
    }

    if (!_cupsSNMPRead(fd, &packet, timeout))
    {
      DEBUG_puts("5_cupsSNMPWalk: Returning -1");

      return (-1);
    }

    if (!_cupsSNMPIsOIDPrefixed(&packet, prefix) ||
        _cupsSNMPIsOID(&packet, lastoid))
    {
      DEBUG_printf(("5_cupsSNMPWalk: Returning %d", count));

      return (count);
    }

    if (packet.error || packet.error_status)
    {
      DEBUG_printf(("5_cupsSNMPWalk: Returning %d", count > 0 ? count : -1));

      return (count > 0 ? count : -1);
    }

    _cupsSNMPCopyOID(lastoid, packet.object_name, CUPS_SNMP_MAX_OID);

    count ++;

    (*cb)(&packet, data);
  }
}


/*
 * '_cupsSNMPWrite()' - Send an SNMP query packet.
 *
 * The array pointed to by "oid" is terminated by the value -1.
 */

int					/* O - 1 on success, 0 on error */
_cupsSNMPWrite(
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
  ssize_t	bytes;			/* Size of message */
  http_addr_t	temp;			/* Copy of address */


 /*
  * Range check input...
  */

  DEBUG_printf(("4_cupsSNMPWrite(fd=%d, address=%p, version=%d, "
                "community=\"%s\", request_type=%d, request_id=%u, oid=%p)",
		fd, address, version, community, request_type, request_id, oid));

  if (fd < 0 || !address || version != CUPS_SNMP_VERSION_1 || !community ||
      (request_type != CUPS_ASN1_GET_REQUEST &&
       request_type != CUPS_ASN1_GET_NEXT_REQUEST) || request_id < 1 || !oid)
  {
    DEBUG_puts("5_cupsSNMPWrite: Returning 0 (bad arguments)");

    return (0);
  }

 /*
  * Create the SNMP message...
  */

  memset(&packet, 0, sizeof(packet));

  packet.version      = version;
  packet.request_type = request_type;
  packet.request_id   = request_id;
  packet.object_type  = CUPS_ASN1_NULL_VALUE;

  strlcpy(packet.community, community, sizeof(packet.community));

  for (i = 0; oid[i] >= 0 && i < (CUPS_SNMP_MAX_OID - 1); i ++)
    packet.object_name[i] = oid[i];
  packet.object_name[i] = -1;

  if (oid[i] >= 0)
  {
    DEBUG_puts("5_cupsSNMPWrite: Returning 0 (OID too big)");

    errno = E2BIG;
    return (0);
  }

  bytes = asn1_encode_snmp(buffer, sizeof(buffer), &packet);

  if (bytes < 0)
  {
    DEBUG_puts("5_cupsSNMPWrite: Returning 0 (request too big)");

    errno = E2BIG;
    return (0);
  }

  asn1_debug("DEBUG: OUT ", buffer, (size_t)bytes, 0);

 /*
  * Send the message...
  */

  temp = *address;

  _httpAddrSetPort(&temp, CUPS_SNMP_PORT);

  return (sendto(fd, buffer, (size_t)bytes, 0, (void *)&temp, (socklen_t)httpAddrLength(&temp)) == bytes);
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
  size_t	i;			/* Looping var */
  unsigned char	*bufend;		/* End of buffer */
  int		integer;		/* Number value */
  int		oid[CUPS_SNMP_MAX_OID];	/* OID value */
  char		string[CUPS_SNMP_MAX_STRING];
					/* String value */
  unsigned char	value_type;		/* Type of value */
  unsigned	value_length;		/* Length of value */
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */


  if (cg->snmp_debug <= 0)
    return;

  if (cg->snmp_debug > 1 && indent == 0)
  {
   /*
    * Do a hex dump of the packet...
    */

    size_t j;

    fprintf(stderr, "%sHex Dump (%d bytes):\n", prefix, (int)len);

    for (i = 0; i < len; i += 16)
    {
      fprintf(stderr, "%s%04x:", prefix, (unsigned)i);

      for (j = 0; j < 16 && (i + j) < len; j ++)
      {
        if (j && !(j & 3))
	  fprintf(stderr, "  %02x", buffer[i + j]);
        else
	  fprintf(stderr, " %02x", buffer[i + j]);
      }

      while (j < 16)
      {
        if (j && !(j & 3))
	  fputs("    ", stderr);
	else
	  fputs("   ", stderr);

        j ++;
      }

      fputs("    ", stderr);

      for (j = 0; j < 16 && (i + j) < len; j ++)
        if (buffer[i + j] < ' ' || buffer[i + j] >= 0x7f)
	  putc('.', stderr);
	else
	  putc(buffer[i + j], stderr);

      putc('\n', stderr);
    }
  }

  if (indent == 0)
    fprintf(stderr, "%sMessage:\n", prefix);

  bufend = buffer + len;

  while (buffer < bufend)
  {
   /*
    * Get value type...
    */

    value_type   = (unsigned char)asn1_get_type(&buffer, bufend);
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

      case CUPS_ASN1_COUNTER :
          integer = asn1_get_integer(&buffer, bufend, value_length);

          fprintf(stderr, "%s%*sCOUNTER %d bytes %u\n", prefix, indent, "",
	          value_length, (unsigned)integer);
          break;

      case CUPS_ASN1_GAUGE :
          integer = asn1_get_integer(&buffer, bufend, value_length);

          fprintf(stderr, "%s%*sGAUGE %d bytes %u\n", prefix, indent, "",
	          value_length, (unsigned)integer);
          break;

      case CUPS_ASN1_TIMETICKS :
          integer = asn1_get_integer(&buffer, bufend, value_length);

          fprintf(stderr, "%s%*sTIMETICKS %d bytes %u\n", prefix, indent, "",
	          value_length, (unsigned)integer);
          break;

      case CUPS_ASN1_OCTET_STRING :
          fprintf(stderr, "%s%*sOCTET STRING %d bytes \"%s\"\n", prefix,
	          indent, "", value_length,
		  asn1_get_string(&buffer, bufend, value_length, string,
				  sizeof(string)));
          break;

      case CUPS_ASN1_HEX_STRING :
	  asn1_get_string(&buffer, bufend, value_length, string,
			  sizeof(string));
          fprintf(stderr, "%s%*sHex-STRING %d bytes", prefix,
	          indent, "", value_length);
          for (i = 0; i < value_length; i ++)
	    fprintf(stderr, " %02X", string[i] & 255);
	  putc('\n', stderr);
          break;

      case CUPS_ASN1_NULL_VALUE :
          fprintf(stderr, "%s%*sNULL VALUE %d bytes\n", prefix, indent, "",
	          value_length);

	  buffer += value_length;
          break;

      case CUPS_ASN1_OID :
          integer = asn1_get_oid(&buffer, bufend, value_length, oid,
	                         CUPS_SNMP_MAX_OID);

          fprintf(stderr, "%s%*sOID %d bytes ", prefix, indent, "",
	          value_length);
	  for (i = 0; i < (unsigned)integer; i ++)
	    fprintf(stderr, ".%d", oid[i]);
	  putc('\n', stderr);
          break;

      case CUPS_ASN1_SEQUENCE :
          fprintf(stderr, "%s%*sSEQUENCE %d bytes\n", prefix, indent, "",
	          value_length);
          asn1_debug(prefix, buffer, value_length, indent + 4);

	  buffer += value_length;
          break;

      case CUPS_ASN1_GET_NEXT_REQUEST :
          fprintf(stderr, "%s%*sGet-Next-Request-PDU %d bytes\n", prefix,
	          indent, "", value_length);
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
 * 'asn1_decode_snmp()' - Decode a SNMP packet.
 */

static int				/* O - 0 on success, -1 on error */
asn1_decode_snmp(unsigned char *buffer,	/* I - Buffer */
                 size_t        len,	/* I - Size of buffer */
                 cups_snmp_t   *packet)	/* I - SNMP packet */
{
  unsigned char	*bufptr,		/* Pointer into the data */
		*bufend;		/* End of data */
  unsigned	length;			/* Length of value */


 /*
  * Initialize the decoding...
  */

  memset(packet, 0, sizeof(cups_snmp_t));
  packet->object_name[0] = -1;

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

    if ((packet->request_type = (cups_asn1_t)asn1_get_type(&bufptr, bufend))
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
      packet->request_id = (unsigned)asn1_get_integer(&bufptr, bufend, length);

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

            packet->object_type = (cups_asn1_t)asn1_get_type(&bufptr, bufend);

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
	        case CUPS_ASN1_BIT_STRING :
	        case CUPS_ASN1_HEX_STRING :
		    packet->object_value.string.num_bytes = length;
		    asn1_get_string(&bufptr, bufend, length,
		                    (char *)packet->object_value.string.bytes,
				    sizeof(packet->object_value.string.bytes));
	            break;

	        case CUPS_ASN1_OID :
		    asn1_get_oid(&bufptr, bufend, length,
		                 packet->object_value.oid, CUPS_SNMP_MAX_OID);
	            break;

	        case CUPS_ASN1_COUNTER :
		    packet->object_value.counter =
		        asn1_get_integer(&bufptr, bufend, length);
	            break;

	        case CUPS_ASN1_GAUGE :
		    packet->object_value.gauge =
		        (unsigned)asn1_get_integer(&bufptr, bufend, length);
	            break;

	        case CUPS_ASN1_TIMETICKS :
		    packet->object_value.timeticks =
		        (unsigned)asn1_get_integer(&bufptr, bufend, length);
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
 * 'asn1_encode_snmp()' - Encode a SNMP packet.
 */

static int				/* O - Length on success, -1 on error */
asn1_encode_snmp(unsigned char *buffer,	/* I - Buffer */
                 size_t        bufsize,	/* I - Size of buffer */
                 cups_snmp_t   *packet)	/* I - SNMP packet */
{
  unsigned char	*bufptr;		/* Pointer into buffer */
  unsigned	total,			/* Total length */
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
        valuelen = packet->object_value.string.num_bytes;
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
  reqlen  = 2 + asn1_size_integer((int)packet->request_id) +
            2 + asn1_size_integer(packet->error_status) +
            2 + asn1_size_integer(packet->error_index) +
            1 + asn1_size_length(listlen) + listlen;
  commlen = (unsigned)strlen(packet->community);
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

  *bufptr++ = packet->request_type;	/* Get-Request-PDU/Get-Next-Request-PDU */
  asn1_set_length(&bufptr, reqlen);

  asn1_set_integer(&bufptr, (int)packet->request_id);

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
	memcpy(bufptr, packet->object_value.string.bytes, valuelen);
	bufptr += valuelen;
	break;

    case CUPS_ASN1_OID :
        asn1_set_oid(&bufptr, packet->object_value.oid);
	break;

    default :
        break;
  }

  return ((int)(bufptr - buffer));
}


/*
 * 'asn1_get_integer()' - Get an integer value.
 */

static int				/* O  - Integer value */
asn1_get_integer(
    unsigned char **buffer,		/* IO - Pointer in buffer */
    unsigned char *bufend,		/* I  - End of buffer */
    unsigned      length)		/* I  - Length of value */
{
  int	value;				/* Integer value */


  if (length > sizeof(int))
  {
    (*buffer) += length;
    return (0);
  }

  for (value = (**buffer & 0x80) ? -1 : 0;
       length > 0 && *buffer < bufend;
       length --, (*buffer) ++)
    value = (value << 8) | **buffer;

  return (value);
}


/*
 * 'asn1_get_length()' - Get a value length.
 */

static unsigned				/* O  - Length */
asn1_get_length(unsigned char **buffer,	/* IO - Pointer in buffer */
		unsigned char *bufend)	/* I  - End of buffer */
{
  unsigned	length;			/* Length */


  length = **buffer;
  (*buffer) ++;

  if (length & 128)
  {
    int	count;				/* Number of bytes for length */


    if ((count = length & 127) > sizeof(unsigned))
    {
      (*buffer) += count;
      return (0);
    }

    for (length = 0;
	 count > 0 && *buffer < bufend;
	 count --, (*buffer) ++)
      length = (length << 8) | **buffer;
  }

  return (length);
}


/*
 * 'asn1_get_oid()' - Get an OID value.
 */

static int				/* O  - Number of OIDs */
asn1_get_oid(
    unsigned char **buffer,		/* IO - Pointer in buffer */
    unsigned char *bufend,		/* I  - End of buffer */
    unsigned      length,		/* I  - Length of value */
    int           *oid,			/* I  - OID buffer */
    int           oidsize)		/* I  - Size of OID buffer */
{
  unsigned char	*valend;		/* End of value */
  int		*oidptr,		/* Current OID */
		*oidend;		/* End of OID buffer */
  int		number;			/* OID number */


  valend = *buffer + length;
  oidptr = oid;
  oidend = oid + oidsize - 1;

  if (valend > bufend)
    valend = bufend;

  number = asn1_get_packed(buffer, bufend);

  if (number < 80)
  {
    *oidptr++ = number / 40;
    number    = number % 40;
    *oidptr++ = number;
  }
  else
  {
    *oidptr++ = 2;
    number    -= 80;
    *oidptr++ = number;
  }

  while (*buffer < valend)
  {
    number = asn1_get_packed(buffer, bufend);

    if (oidptr < oidend)
      *oidptr++ = number;
  }

  *oidptr = -1;

  return ((int)(oidptr - oid));
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
    unsigned      length,		/* I  - Value length */
    char          *string,		/* I  - String buffer */
    size_t        strsize)		/* I  - String buffer size */
{
  if (length > (unsigned)(bufend - *buffer))
    length = (unsigned)(bufend - *buffer);

  if (length < strsize)
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
  **buffer = CUPS_ASN1_INTEGER;
  (*buffer) ++;

  if (integer > 0x7fffff || integer < -0x800000)
  {
    **buffer = 4;
    (*buffer) ++;
    **buffer = (unsigned char)(integer >> 24);
    (*buffer) ++;
    **buffer = (unsigned char)(integer >> 16);
    (*buffer) ++;
    **buffer = (unsigned char)(integer >> 8);
    (*buffer) ++;
    **buffer = (unsigned char)integer;
    (*buffer) ++;
  }
  else if (integer > 0x7fff || integer < -0x8000)
  {
    **buffer = 3;
    (*buffer) ++;
    **buffer = (unsigned char)(integer >> 16);
    (*buffer) ++;
    **buffer = (unsigned char)(integer >> 8);
    (*buffer) ++;
    **buffer = (unsigned char)integer;
    (*buffer) ++;
  }
  else if (integer > 0x7f || integer < -0x80)
  {
    **buffer = 2;
    (*buffer) ++;
    **buffer = (unsigned char)(integer >> 8);
    (*buffer) ++;
    **buffer = (unsigned char)integer;
    (*buffer) ++;
  }
  else
  {
    **buffer = 1;
    (*buffer) ++;
    **buffer = (unsigned char)integer;
    (*buffer) ++;
  }
}


/*
 * 'asn1_set_length()' - Set a value length.
 */

static void
asn1_set_length(unsigned char **buffer,	/* IO - Pointer in buffer */
		unsigned      length)	/* I  - Length value */
{
  if (length > 255)
  {
    **buffer = 0x82;			/* 2-byte length */
    (*buffer) ++;
    **buffer = (unsigned char)(length >> 8);
    (*buffer) ++;
    **buffer = (unsigned char)length;
    (*buffer) ++;
  }
  else if (length > 127)
  {
    **buffer = 0x81;			/* 1-byte length */
    (*buffer) ++;
    **buffer = (unsigned char)length;
    (*buffer) ++;
  }
  else
  {
    **buffer = (unsigned char)length;	/* Length */
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

  if (oid[1] < 0)
  {
    asn1_set_packed(buffer, oid[0] * 40);
    return;
  }

  asn1_set_packed(buffer, oid[0] * 40 + oid[1]);

  for (oid += 2; *oid >= 0; oid ++)
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
    **buffer = ((integer >> 28) & 0x7f) | 0x80;
    (*buffer) ++;
  }

  if (integer > 0x1fffff)
  {
    **buffer = ((integer >> 21) & 0x7f) | 0x80;
    (*buffer) ++;
  }

  if (integer > 0x3fff)
  {
    **buffer = ((integer >> 14) & 0x7f) | 0x80;
    (*buffer) ++;
  }

  if (integer > 0x7f)
  {
    **buffer = ((integer >> 7) & 0x7f) | 0x80;
    (*buffer) ++;
  }

  **buffer = integer & 0x7f;
  (*buffer) ++;
}


/*
 * 'asn1_size_integer()' - Figure out the number of bytes needed for an
 *                         integer value.
 */

static unsigned				/* O - Size in bytes */
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

static unsigned				/* O - Size in bytes */
asn1_size_length(unsigned length)	/* I - Length value */
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

static unsigned				/* O - Size in bytes */
asn1_size_oid(const int *oid)		/* I - OID value */
{
  unsigned	length;			/* Length of value */


  if (oid[1] < 0)
    return (asn1_size_packed(oid[0] * 40));

  for (length = asn1_size_packed(oid[0] * 40 + oid[1]), oid += 2;
       *oid >= 0;
       oid ++)
    length += asn1_size_packed(*oid);

  return (length);
}


/*
 * 'asn1_size_packed()' - Figure out the number of bytes needed for a
 *                        packed integer value.
 */

static unsigned				/* O - Size in bytes */
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
 * End of "$Id: snmp.c 11594 2014-02-14 20:09:01Z msweet $".
 */
