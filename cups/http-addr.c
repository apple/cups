/*
 * "$Id: http-addr.c,v 1.1.2.2 2001/12/29 23:33:03 mike Exp $"
 *
 *   HTTP address routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2001 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   httpAddrEqual()     - Compare two addresses.
 *   httpAddrLoad()      - Load a host entry address into an HTTP address.
 *   httpAddrLocalhost() - Check for the local loopback address.
 *   httpAddrLookup()    - Lookup the hostname associated with the address.
 *   httpAddrString()    - Convert an IP address to a dotted string.
 */

/*
 * Include necessary headers...
 */

#include "http.h"
#include "debug.h"
#include "string.h"


/*
 * 'httpAddrEqual()' - Compare two addresses.
 */

int						/* O - 1 if equal, 0 if != */
httpAddrEqual(const http_addr_t *addr1,		/* I - First address */
              const http_addr_t *addr2)		/* I - Second address */
{
  if (addr1->addr.sa_family != addr2->addr.sa_family)
    return (0);

#ifdef AF_INET6
  if (addr1->addr.sa_family == AF_INET6)
    return (memcmp(&(addr1->ipv6.sin6_addr), &(addr2->ipv6.sin6_addr), 16) == 0);
#endif /* AF_INET6 */

  return (addr1->ipv4.sin_addr.s_addr == addr2->ipv4.sin_addr.s_addr);
}


/*
 * 'httpAddrLoad()' - Load a host entry address into an HTTP address.
 */

void
httpAddrLoad(const struct hostent *host,	/* I - Host entry */
             int                  port,		/* I - Port number */
             int                  n,		/* I - Index into host entry */
	     http_addr_t          *addr)	/* O - Address to load */
{
#ifdef AF_INET6
  if (host->h_addrtype == AF_INET6)
  {
#  ifdef WIN32
    addr->ipv6.sin6_port = htons((u_short)port);
#  else
    addr->ipv6.sin6_port = htons(port);
#  endif /* WIN32 */

    memcpy((char *)&(addr->ipv6.sin6_addr), host->h_addr_list[n],
           host->h_length);
    addr->ipv6.sin6_family = AF_INET6;
  }
  else
#endif /* AF_INET6 */
  {
#  ifdef WIN32
    addr->ipv4.sin_port = htons((u_short)port);
#  else
    addr->ipv4.sin_port = htons(port);
#  endif /* WIN32 */

    memcpy((char *)&(addr->ipv4.sin_addr), host->h_addr_list[n],
           host->h_length);
    addr->ipv4.sin_family = AF_INET;
  }
}


/*
 * 'httpAddrLocalhost()' - Check for the local loopback address.
 */

int						/* O - 1 if local host, 1 otherwise */
httpAddrLocalhost(const http_addr_t *addr)	/* I - Address to check */
{
#ifdef AF_INET6
  if (addr->addr.sa_family == AF_INET6 &&
      IN6_IS_ADDR_LOOPBACK(&(addr->ipv6.sin6_addr)))
    return (1);
#endif /* AF_INET6 */

  if (addr->addr.sa_family == AF_INET &&
      ntohl(addr->ipv4.sin_addr.s_addr) == 0x7f000001)
    return (1);

  return (0);
}


#ifdef __sgi
#  define ADDR_CAST (struct sockaddr *)
#else
#  define ADDR_CAST (char *)
#endif /* __sgi */


/*
 * 'httpAddrLookup()' - Lookup the hostname associated with the address.
 */

char *						/* O - Host name */
httpAddrLookup(const http_addr_t *addr,		/* I - Address to lookup */
               char              *name,		/* I - Host name buffer */
	       int               namelen)	/* I - Size of name buffer */
{
  struct hostent	*host;			/* Host from name service */


#ifdef AF_INET6
  if (addr->addr.sa_family == AF_INET6)
    host = gethostbyaddr(ADDR_CAST &(addr->ipv6.sin6_addr),
                         sizeof(struct in6_addr), AF_INET6);
  else
#endif /* AF_INET6 */
  if (addr->addr.sa_family == AF_INET)
    host = gethostbyaddr(ADDR_CAST &(addr->ipv4.sin_addr),
                         sizeof(struct in_addr), AF_INET);
  else
    host = NULL;

  if (host == NULL)
  {
    httpAddrString(addr, name, namelen);
    return (NULL);
  }

  strncpy(name, host->h_name, namelen - 1);
  name[namelen - 1] = '\0';

  return (name);
}


/*
 * 'httpAddrString()' - Convert an IP address to a dotted string.
 */

char *						/* O - IP string */
httpAddrString(const http_addr_t *addr,		/* I - Address to convert */
               char              *s,		/* I - String buffer */
	       int               slen)		/* I - Length of string */
{
#ifdef AF_INET6
  if (addr->addr.sa_family == AF_INET6)
    snprintf(s, slen, "%u.%u.%u.%u",
             ntohl(addr->ipv6.sin6_addr.s6_addr32[0]),
             ntohl(addr->ipv6.sin6_addr.s6_addr32[1]),
             ntohl(addr->ipv6.sin6_addr.s6_addr32[2]),
             ntohl(addr->ipv6.sin6_addr.s6_addr32[3]));
  else
#endif /* AF_INET6 */
  if (addr->addr.sa_family == AF_INET)
  {
    unsigned temp;				/* Temporary address */


    temp = ntohl(addr->ipv4.sin_addr.s_addr);

    snprintf(s, slen, "%d.%d.%d.%d", (temp >> 24) & 255,
             (temp >> 16) & 255, (temp >> 8) & 255, temp & 255);
  }
  else
  {
    strncpy(s, "UNKNOWN", slen - 1);
    s[slen - 1] = '\0';
  }

  return (s);
}


/*
 * End of "$Id: http-addr.c,v 1.1.2.2 2001/12/29 23:33:03 mike Exp $".
 */
