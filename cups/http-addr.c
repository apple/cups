/*
 * "$Id$"
 *
 *   HTTP address routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   httpAddrAny()       - Check for the "any" address.
 *   httpAddrEqual()     - Compare two addresses.
 *   httpAddrLocalhost() - Check for the local loopback address.
 *   httpAddrLookup()    - Lookup the hostname associated with the address.
 *   httpAddrString()    - Convert an IP address to a dotted string.
 *   httpGetHostByName() - Lookup a hostname or IP address, and return
 *                         address records for the specified name.
 *   httpGetHostname()   - Get the FQDN for the local system.
 */

/*
 * Include necessary headers...
 */

#include "globals.h"
#include "debug.h"
#include <stdlib.h>


/*
 * 'httpAddrAny()' - Check for the "any" address.
 *
 * @since CUPS 1.2@
 */

int					/* O - 1 if "any", 0 otherwise */
httpAddrAny(const http_addr_t *addr)	/* I - Address to check */
{
#ifdef AF_INET6
  if (addr->addr.sa_family == AF_INET6 &&
      IN6_IS_ADDR_UNSPECIFIED(&(addr->ipv6.sin6_addr)))
    return (1);
#endif /* AF_INET6 */

  if (addr->addr.sa_family == AF_INET &&
      ntohl(addr->ipv4.sin_addr.s_addr) == 0x00000000)
    return (1);

  return (0);
}


/*
 * 'httpAddrEqual()' - Compare two addresses.
 *
 * @since CUPS 1.2@
 */

int						/* O - 1 if equal, 0 if != */
httpAddrEqual(const http_addr_t *addr1,		/* I - First address */
              const http_addr_t *addr2)		/* I - Second address */
{
  if (addr1->addr.sa_family != addr2->addr.sa_family)
    return (0);

#ifdef AF_LOCAL
  if (addr1->addr.sa_family == AF_LOCAL)
    return (!strcmp(addr1->un.sun_path, addr2->un.sun_path));
#endif /* AF_LOCAL */

#ifdef AF_INET6
  if (addr1->addr.sa_family == AF_INET6)
    return (memcmp(&(addr1->ipv6.sin6_addr), &(addr2->ipv6.sin6_addr), 16) == 0);
#endif /* AF_INET6 */

  return (addr1->ipv4.sin_addr.s_addr == addr2->ipv4.sin_addr.s_addr);
}


/*
 * 'httpAddrLength()' - Return the length of the address in bytes.
 *
 * @since CUPS 1.2@
 */

int					/* O - Length in bytes */
httpAddrLength(const http_addr_t *addr)	/* I - Address */
{
#ifdef AF_INET6
  if (addr->addr.sa_family == AF_INET6)
    return (sizeof(addr->ipv6));
  else
#endif /* AF_INET6 */
#ifdef AF_LOCAL
  if (addr->addr.sa_family == AF_LOCAL)
    return (sizeof(addr->un.sun_family) + strlen(addr->un.sun_path));
  else
#endif /* AF_LOCAL */
  if (addr->addr.sa_family == AF_INET)
    return (sizeof(addr->ipv4));
  else
    return (0);

}


/*
 * 'httpAddrLocalhost()' - Check for the local loopback address.
 *
 * @since CUPS 1.2@
 */

int					/* O - 1 if local host, 0 otherwise */
httpAddrLocalhost(
    const http_addr_t *addr)		/* I - Address to check */
{
#ifdef AF_INET6
  if (addr->addr.sa_family == AF_INET6 &&
      (IN6_IS_ADDR_LOOPBACK(&(addr->ipv6.sin6_addr)) ||
       IN6_IS_ADDR_UNSPECIFIED(&(addr->ipv6.sin6_addr))))
    return (1);
#endif /* AF_INET6 */

#ifdef AF_LOCAL
  if (addr->addr.sa_family == AF_LOCAL)
    return (1);
#endif /* AF_LOCAL */

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
 *
 * @since CUPS 1.2@
 */

char *						/* O - Host name */
httpAddrLookup(const http_addr_t *addr,		/* I - Address to lookup */
               char              *name,		/* I - Host name buffer */
	       int               namelen)	/* I - Size of name buffer */
{
  struct hostent	*host;			/* Host from name service */


  DEBUG_printf(("httpAddrLookup(addr=%p, name=%p, namelen=%d)\n",
                addr, name, namelen));

#ifdef AF_INET6
  if (addr->addr.sa_family == AF_INET6)
    host = gethostbyaddr(ADDR_CAST &(addr->ipv6.sin6_addr),
                         sizeof(struct in6_addr), AF_INET6);
  else
#endif /* AF_INET6 */
#ifdef AF_LOCAL
  if (addr->addr.sa_family == AF_LOCAL)
  {
    strlcpy(name, addr->un.sun_path, namelen);
    return (name);
  }
  else
#endif /* AF_LOCAL */
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

  strlcpy(name, host->h_name, namelen);

  return (name);
}


/*
 * 'httpAddrString()' - Convert an IP address to a dotted string.
 *
 * @since CUPS 1.2@
 */

char *						/* O - IP string */
httpAddrString(const http_addr_t *addr,		/* I - Address to convert */
               char              *s,		/* I - String buffer */
	       int               slen)		/* I - Length of string */
{
  DEBUG_printf(("httpAddrString(addr=%p, s=%p, slen=%d)\n",
                addr, s, slen));

#ifdef AF_INET6
  if (addr->addr.sa_family == AF_INET6)
    snprintf(s, slen, "[%x:%x:%x:%x]",
             ntohl(addr->ipv6.sin6_addr.s6_addr32[0]),
             ntohl(addr->ipv6.sin6_addr.s6_addr32[1]),
             ntohl(addr->ipv6.sin6_addr.s6_addr32[2]),
             ntohl(addr->ipv6.sin6_addr.s6_addr32[3]));
  else
#endif /* AF_INET6 */
#ifdef AF_LOCAL
  if (addr->addr.sa_family == AF_LOCAL)
    strlcpy(s, addr->un.sun_path, slen);
  else
#endif /* AF_LOCAL */
  if (addr->addr.sa_family == AF_INET)
  {
    unsigned temp;				/* Temporary address */


    temp = ntohl(addr->ipv4.sin_addr.s_addr);

    snprintf(s, slen, "%d.%d.%d.%d", (temp >> 24) & 255,
             (temp >> 16) & 255, (temp >> 8) & 255, temp & 255);
  }
  else
    strlcpy(s, "UNKNOWN", slen);

  DEBUG_printf(("httpAddrString: returning \"%s\"...\n", s));

  return (s);
}


/*
 * 'httpGetHostByName()' - Lookup a hostname or IPv4 address, and return
 *                         address records for the specified name.
 *
 * @deprecated@
 */

struct hostent *			/* O - Host entry */
httpGetHostByName(const char *name)	/* I - Hostname or IP address */
{
  const char		*nameptr;	/* Pointer into name */
  unsigned		ip[4];		/* IP address components */
  _cups_globals_t	*cg = _cupsGlobals();
  					/* Pointer to library globals */


  DEBUG_printf(("httpGetHostByName(name=\"%s\")\n", name));

 /*
  * Avoid lookup delays and configuration problems when connecting
  * to the localhost address...
  */

  if (!strcmp(name, "localhost"))
    name = "127.0.0.1";

 /*
  * This function is needed because some operating systems have a
  * buggy implementation of gethostbyname() that does not support
  * IP addresses.  If the first character of the name string is a
  * number, then sscanf() is used to extract the IP components.
  * We then pack the components into an IPv4 address manually,
  * since the inet_aton() function is deprecated.  We use the
  * htonl() macro to get the right byte order for the address.
  *
  * We also support domain sockets when supported by the underlying
  * OS...
  */

#ifdef AF_LOCAL
  if (name[0] == '/')
  {
   /*
    * A domain socket address, so make an AF_LOCAL entry and return it...
    */

    cg->hostent.h_name      = (char *)name;
    cg->hostent.h_aliases   = NULL;
    cg->hostent.h_addrtype  = AF_LOCAL;
    cg->hostent.h_length    = strlen(name) + 1;
    cg->hostent.h_addr_list = cg->ip_ptrs;
    cg->ip_ptrs[0]          = (char *)name;
    cg->ip_ptrs[1]          = NULL;

    DEBUG_puts("httpGetHostByName: returning domain socket address...");

    return (&cg->hostent);
  }
#endif /* AF_LOCAL */

  for (nameptr = name; isdigit(*nameptr & 255) || *nameptr == '.'; nameptr ++);

  if (!*nameptr)
  {
   /*
    * We have an IPv4 address; break it up and provide the host entry
    * to the caller.
    */

    if (sscanf(name, "%u.%u.%u.%u", ip, ip + 1, ip + 2, ip + 3) != 4)
      return (NULL);			/* Must have 4 numbers */

    if (ip[0] > 255 || ip[1] > 255 || ip[2] > 255 || ip[3] > 255)
      return (NULL);			/* Invalid byte ranges! */

    cg->ip_addr = htonl(((((((ip[0] << 8) | ip[1]) << 8) | ip[2]) << 8) |
                         ip[3]));

   /*
    * Fill in the host entry and return it...
    */

    cg->hostent.h_name      = (char *)name;
    cg->hostent.h_aliases   = NULL;
    cg->hostent.h_addrtype  = AF_INET;
    cg->hostent.h_length    = 4;
    cg->hostent.h_addr_list = cg->ip_ptrs;
    cg->ip_ptrs[0]          = (char *)&(cg->ip_addr);
    cg->ip_ptrs[1]          = NULL;

    DEBUG_puts("httpGetHostByName: returning IPv4 address...");

    return (&cg->hostent);
  }
  else
  {
   /*
    * Use the gethostbyname() function to get the IPv4 address for
    * the name...
    */

    DEBUG_puts("httpGetHostByName: returning domain lookup address(es)...");

    return (gethostbyname(name));
  }
}


/*
 * 'httpGetHostname()' - Get the FQDN for the local system.
 *
 * This function uses both gethostname() and gethostbyname() to
 * get the local hostname with domain.
 *
 * @since CUPS 1.2@
 */

const char *				/* O - FQDN for this system */
httpGetHostname(char *s,		/* I - String buffer for name */
                int  slen)		/* I - Size of buffer */
{
  struct hostent	*host;		/* Host entry to get FQDN */


 /*
  * Get the hostname...
  */

  gethostname(s, slen);

  if (!strchr(s, '.'))
  {
   /*
    * The hostname is not a FQDN, so look it up...
    */

    if ((host = gethostbyname(s)) != NULL)
      strlcpy(s, host->h_name, slen);
  }

 /*
  * Return the hostname with as much domain info as we have...
  */

  return (s);
}


/*
 * End of "$Id$".
 */
