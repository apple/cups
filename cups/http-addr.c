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
 *   httpAddrLoad()      - Load a host entry address into an HTTP address.
 *   httpAddrLocalhost() - Check for the local loopback address.
 *   httpAddrLookup()    - Lookup the hostname associated with the address.
 *   httpAddrString()    - Convert an IP address to a dotted string.
 *   httpGetHostByName() - Lookup a hostname or IP address, and return
 *                         address records for the specified name.
 */

/*
 * Include necessary headers...
 */

#include "http.h"
#include "debug.h"
#include "string.h"
#include <stdlib.h>


/*
 * 'httpAddrAny()' - Check for the "any" address.
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
#ifdef AF_LOCAL
  if (host->h_addrtype == AF_LOCAL)
  {
    addr->un.sun_family = AF_LOCAL;
    strlcpy(addr->un.sun_path, host->h_addr_list[n], sizeof(addr->un.sun_path));
  }
  else
#endif /* AF_LOCAL */
  if (host->h_addrtype == AF_INET)
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

int					/* O - 1 if local host, 0 otherwise */
httpAddrLocalhost(const http_addr_t *addr)
					/* I - Address to check */
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
 * 'httpGetHostByName()' - Lookup a hostname or IP address, and return
 *                         address records for the specified name.
 */

struct hostent *			/* O - Host entry */
httpGetHostByName(const char *name)	/* I - Hostname or IP address */
{
  int			i;		/* Looping var */
  const char		*nameptr;	/* Pointer into name */
  unsigned		ip[4];		/* IP address components */
  static unsigned	packed_ip[100][4];/* Packed IPv4/6 addresses */
  static char		*packed_ptr[101];/* Pointer to packed address */
  static struct hostent	host_ip;	/* Host entry for IP/domain address */
#ifdef HAVE_GETADDRINFO
  static char		h_name[1024];	/* Hostname */
#endif /* HAVE_GETADDRINFO */


  DEBUG_printf(("httpGetHostByName(name=\"%s\")\n", name));

#if defined(__APPLE__)
  /* OS X hack to avoid it's ocassional long delay in lookupd */
  static const char sLoopback[] = "127.0.0.1";
  if (!strcmp(name, "localhost"))
    name = sLoopback;
#endif /* __APPLE__ */

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

    host_ip.h_name      = (char *)name;
    host_ip.h_aliases   = NULL;
    host_ip.h_addrtype  = AF_LOCAL;
    host_ip.h_length    = strlen(name) + 1;
    host_ip.h_addr_list = packed_ptr;
    packed_ptr[0]       = (char *)name;
    packed_ptr[1]       = NULL;

    DEBUG_puts("httpGetHostByName: returning domain socket address...");

    return (&host_ip);
  }
#endif /* AF_LOCAL */
#ifdef AF_INET6
  if (name[0] == '[')
  {
   /*
    * A raw 128-bit IPv6 address of the form "[xxxx:xxxx:xxxx:xxxx]"
    */

    host_ip.h_name      = (char *)name;
    host_ip.h_aliases   = NULL;
    host_ip.h_addrtype  = AF_INET6;
    host_ip.h_length    = 16;
    host_ip.h_addr_list = packed_ptr;
    packed_ptr[0]       = (char *)(packed_ip[0]);
    packed_ptr[1]       = NULL;

    for (i = 0, nameptr = name + 1; *nameptr && i < 4; i ++)
    {
      if (*nameptr == ']')
        break;
      else if (*nameptr == ':')
        packed_ip[0][i] = 0;
      else
        packed_ip[0][i] = htonl(strtoul(nameptr, (char **)&nameptr, 16));

      if (*nameptr == ':' || *nameptr == ']')
        nameptr ++;
    }

    while (i < 4)
    {
      packed_ip[0][i] = 0;
      i ++;
    }

    if (*nameptr)
      return (NULL);

    DEBUG_puts("httpGetHostByName: returning IPv6 address...");

    return (&host_ip);
  }
#endif /* AF_INET6 */

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

    packed_ip[0][0] = htonl(((((((ip[0] << 8) | ip[1]) << 8) | ip[2]) << 8) |
                             ip[3]));

   /*
    * Fill in the host entry and return it...
    */

    host_ip.h_name      = (char *)name;
    host_ip.h_aliases   = NULL;
    host_ip.h_addrtype  = AF_INET;
    host_ip.h_length    = 4;
    host_ip.h_addr_list = packed_ptr;
    packed_ptr[0]       = (char *)packed_ip[0];
    packed_ptr[1]       = NULL;

    DEBUG_puts("httpGetHostByName: returning IPv4 address...");

    return (&host_ip);
  }
  else
#ifdef HAVE_GETADDRINFO
  {
   /*
    * Use the getaddrinfo() function to get the IP address for the
    * name...
    */

    struct addrinfo	hints,		/* Address lookup hints */
			*results,	/* Address lookup results */
			*current;	/* Current result */
    http_addr_t		*address;	/* Current address */


    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_CANONNAME;

    if (getaddrinfo(name, NULL, &hints, &results))
    {
     /*
      * If getaddrinfo() fails, try gethostbyname()...
      */

      return (gethostbyname(name));
    }

   /*
    * Initialize hostent structure, preferring the IPv6 address...
    */

    for (current = results; current; current = current->ai_next)
      if (current->ai_family == AF_INET6)
        break;

    if (!current)
    {
      for (current = results; current; current = current->ai_next)
	if (current->ai_family == AF_INET)
          break;

      if (!current)
      {
       /*
	* No IPv4 or IPv6 addresses, try gethostbyname()...
	*/

        freeaddrinfo(results);

	return (gethostbyname(name));
      }
    }

    strlcpy(h_name, current->ai_canonname, sizeof(h_name));

    host_ip.h_name      = h_name;
    host_ip.h_aliases   = NULL;
    host_ip.h_addrtype  = current->ai_family;
    host_ip.h_addr_list = packed_ptr;

    if (current->ai_family == AF_INET6)
      host_ip.h_length = 16;
    else
      host_ip.h_length = 4;

   /*
    * Convert the address info to a hostent structure...
    */

    for (i = 0, current = results; i < 100 && current; current = current->ai_next)
      if (current->ai_family == host_ip.h_addrtype)
      {
       /*
        * Copy this address...
	*/

        address = (http_addr_t *)(current->ai_addr);

        if (current->ai_family == AF_INET)
	  memcpy((char *)packed_ip[i], (char *)&(address->ipv4.sin_addr), 4);
	else
	  memcpy((char *)packed_ip[i], (char *)&(address->ipv6.sin6_addr), 16);

	packed_ptr[i] = (char *)packed_ip[i];
	i ++;
      }

    packed_ptr[i] = NULL;

   /*
    * Free the getaddrinfo() results and return the hostent structure...
    */

    freeaddrinfo(results);

    return (&host_ip);
  }
#else
  {
   /*
    * Use the gethostbyname() function to get the IP address for
    * the name...
    */

    DEBUG_puts("httpGetHostByName: returning domain lookup address(es)...");

    return (gethostbyname(name));
  }
#endif /* HAVE_GETADDRINFO */
}


/*
 * 'httpGetHostname()' - Get the FQDN for the local system.
 *
 * This function uses both gethostname() and gethostbyname() to
 * get the local hostname with domain.
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
