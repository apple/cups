/*
 * HTTP address routines for CUPS.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include <sys/stat.h>
#ifdef HAVE_RESOLV_H
#  include <resolv.h>
#endif /* HAVE_RESOLV_H */
#ifdef __APPLE__
#  include <CoreFoundation/CoreFoundation.h>
#  include <SystemConfiguration/SystemConfiguration.h>
#endif /* __APPLE__ */


/*
 * 'httpAddrAny()' - Check for the "any" address.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

int					/* O - 1 if "any", 0 otherwise */
httpAddrAny(const http_addr_t *addr)	/* I - Address to check */
{
  if (!addr)
    return (0);

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
 * 'httpAddrClose()' - Close a socket created by @link httpAddrConnect@ or
 *                     @link httpAddrListen@.
 *
 * Pass @code NULL@ for sockets created with @link httpAddrConnect2@ and the
 * listen address for sockets created with @link httpAddrListen@.  This function
 * ensures that domain sockets are removed when closed.
 *
 * @since CUPS 2.0/OS 10.10@
 */

int						/* O - 0 on success, -1 on failure */
httpAddrClose(http_addr_t *addr,		/* I - Listen address or @code NULL@ */
              int         fd)			/* I - Socket file descriptor */
{
#ifdef _WIN32
  if (closesocket(fd))
#else
  if (close(fd))
#endif /* _WIN32 */
    return (-1);

#ifdef AF_LOCAL
  if (addr && addr->addr.sa_family == AF_LOCAL)
    return (unlink(addr->un.sun_path));
#endif /* AF_LOCAL */

  return (0);
}


/*
 * 'httpAddrEqual()' - Compare two addresses.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

int						/* O - 1 if equal, 0 if not */
httpAddrEqual(const http_addr_t *addr1,		/* I - First address */
              const http_addr_t *addr2)		/* I - Second address */
{
  if (!addr1 && !addr2)
    return (1);

  if (!addr1 || !addr2)
    return (0);

  if (addr1->addr.sa_family != addr2->addr.sa_family)
    return (0);

#ifdef AF_LOCAL
  if (addr1->addr.sa_family == AF_LOCAL)
    return (!strcmp(addr1->un.sun_path, addr2->un.sun_path));
#endif /* AF_LOCAL */

#ifdef AF_INET6
  if (addr1->addr.sa_family == AF_INET6)
    return (!memcmp(&(addr1->ipv6.sin6_addr), &(addr2->ipv6.sin6_addr), 16));
#endif /* AF_INET6 */

  return (addr1->ipv4.sin_addr.s_addr == addr2->ipv4.sin_addr.s_addr);
}


/*
 * 'httpAddrLength()' - Return the length of the address in bytes.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

int					/* O - Length in bytes */
httpAddrLength(const http_addr_t *addr)	/* I - Address */
{
  if (!addr)
    return (0);

#ifdef AF_INET6
  if (addr->addr.sa_family == AF_INET6)
    return (sizeof(addr->ipv6));
  else
#endif /* AF_INET6 */
#ifdef AF_LOCAL
  if (addr->addr.sa_family == AF_LOCAL)
    return ((int)(offsetof(struct sockaddr_un, sun_path) + strlen(addr->un.sun_path) + 1));
  else
#endif /* AF_LOCAL */
  if (addr->addr.sa_family == AF_INET)
    return (sizeof(addr->ipv4));
  else
    return (0);

}


/*
 * 'httpAddrListen()' - Create a listening socket bound to the specified
 *                      address and port.
 *
 * @since CUPS 1.7/macOS 10.9@
 */

int					/* O - Socket or -1 on error */
httpAddrListen(http_addr_t *addr,	/* I - Address to bind to */
               int         port)	/* I - Port number to bind to */
{
  int		fd = -1,		/* Socket */
		val,			/* Socket value */
                status;			/* Bind status */


 /*
  * Range check input...
  */

  if (!addr || port < 0)
    return (-1);

 /*
  * Create the socket and set options...
  */

  if ((fd = socket(addr->addr.sa_family, SOCK_STREAM, 0)) < 0)
  {
    _cupsSetHTTPError(HTTP_STATUS_ERROR);
    return (-1);
  }

  val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, CUPS_SOCAST &val, sizeof(val));

#ifdef IPV6_V6ONLY
  if (addr->addr.sa_family == AF_INET6)
    setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, CUPS_SOCAST &val, sizeof(val));
#endif /* IPV6_V6ONLY */

 /*
  * Bind the socket...
  */

#ifdef AF_LOCAL
  if (addr->addr.sa_family == AF_LOCAL)
  {
    mode_t	mask;			/* Umask setting */

   /*
    * Remove any existing domain socket file...
    */

    unlink(addr->un.sun_path);

   /*
    * Save the current umask and set it to 0 so that all users can access
    * the domain socket...
    */

    mask = umask(0);

   /*
    * Bind the domain socket...
    */

    status = bind(fd, (struct sockaddr *)addr, (socklen_t)httpAddrLength(addr));

   /*
    * Restore the umask and fix permissions...
    */

    umask(mask);
    chmod(addr->un.sun_path, 0140777);
  }
  else
#endif /* AF_LOCAL */
  {
    _httpAddrSetPort(addr, port);

    status = bind(fd, (struct sockaddr *)addr, (socklen_t)httpAddrLength(addr));
  }

  if (status)
  {
    _cupsSetHTTPError(HTTP_STATUS_ERROR);

    close(fd);

    return (-1);
  }

 /*
  * Listen...
  */

  if (listen(fd, 5))
  {
    _cupsSetHTTPError(HTTP_STATUS_ERROR);

    close(fd);

    return (-1);
  }

 /*
  * Close on exec...
  */

#ifndef _WIN32
  fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
#endif /* !_WIN32 */

#ifdef SO_NOSIGPIPE
 /*
  * Disable SIGPIPE for this socket.
  */

  val = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, CUPS_SOCAST &val, sizeof(val));
#endif /* SO_NOSIGPIPE */

  return (fd);
}


/*
 * 'httpAddrLocalhost()' - Check for the local loopback address.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

int					/* O - 1 if local host, 0 otherwise */
httpAddrLocalhost(
    const http_addr_t *addr)		/* I - Address to check */
{
  if (!addr)
    return (1);

#ifdef AF_INET6
  if (addr->addr.sa_family == AF_INET6 &&
      IN6_IS_ADDR_LOOPBACK(&(addr->ipv6.sin6_addr)))
    return (1);
#endif /* AF_INET6 */

#ifdef AF_LOCAL
  if (addr->addr.sa_family == AF_LOCAL)
    return (1);
#endif /* AF_LOCAL */

  if (addr->addr.sa_family == AF_INET &&
      (ntohl(addr->ipv4.sin_addr.s_addr) & 0xff000000) == 0x7f000000)
    return (1);

  return (0);
}


/*
 * 'httpAddrLookup()' - Lookup the hostname associated with the address.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

char *					/* O - Host name */
httpAddrLookup(
    const http_addr_t *addr,		/* I - Address to lookup */
    char              *name,		/* I - Host name buffer */
    int               namelen)		/* I - Size of name buffer */
{
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */


  DEBUG_printf(("httpAddrLookup(addr=%p, name=%p, namelen=%d)", (void *)addr, (void *)name, namelen));

 /*
  * Range check input...
  */

  if (!addr || !name || namelen <= 2)
  {
    if (name && namelen >= 1)
      *name = '\0';

    return (NULL);
  }

#ifdef AF_LOCAL
  if (addr->addr.sa_family == AF_LOCAL)
  {
    strlcpy(name, addr->un.sun_path, (size_t)namelen);
    return (name);
  }
#endif /* AF_LOCAL */

 /*
  * Optimize lookups for localhost/loopback addresses...
  */

  if (httpAddrLocalhost(addr))
  {
    strlcpy(name, "localhost", (size_t)namelen);
    return (name);
  }

#ifdef HAVE_RES_INIT
 /*
  * STR #2920: Initialize resolver after failure in cups-polld
  *
  * If the previous lookup failed, re-initialize the resolver to prevent
  * temporary network errors from persisting.  This *should* be handled by
  * the resolver libraries, but apparently the glibc folks do not agree.
  *
  * We set a flag at the end of this function if we encounter an error that
  * requires reinitialization of the resolver functions.  We then call
  * res_init() if the flag is set on the next call here or in httpAddrLookup().
  */

  if (cg->need_res_init)
  {
    res_init();

    cg->need_res_init = 0;
  }
#endif /* HAVE_RES_INIT */

#ifdef HAVE_GETNAMEINFO
  {
   /*
    * STR #2486: httpAddrLookup() fails when getnameinfo() returns EAI_AGAIN
    *
    * FWIW, I think this is really a bug in the implementation of
    * getnameinfo(), but falling back on httpAddrString() is easy to
    * do...
    */

    int error = getnameinfo(&addr->addr, (socklen_t)httpAddrLength(addr), name, (socklen_t)namelen, NULL, 0, 0);

    if (error)
    {
      if (error == EAI_FAIL)
        cg->need_res_init = 1;

      return (httpAddrString(addr, name, namelen));
    }
  }
#else
  {
    struct hostent	*host;			/* Host from name service */


#  ifdef AF_INET6
    if (addr->addr.sa_family == AF_INET6)
      host = gethostbyaddr((char *)&(addr->ipv6.sin6_addr),
                	   sizeof(struct in_addr), AF_INET6);
    else
#  endif /* AF_INET6 */
    host = gethostbyaddr((char *)&(addr->ipv4.sin_addr),
                	 sizeof(struct in_addr), AF_INET);

    if (host == NULL)
    {
     /*
      * No hostname, so return the raw address...
      */

      if (h_errno == NO_RECOVERY)
        cg->need_res_init = 1;

      return (httpAddrString(addr, name, namelen));
    }

    strlcpy(name, host->h_name, (size_t)namelen);
  }
#endif /* HAVE_GETNAMEINFO */

  DEBUG_printf(("1httpAddrLookup: returning \"%s\"...", name));

  return (name);
}


/*
 * 'httpAddrFamily()' - Get the address family of an address.
 */

int					/* O - Address family */
httpAddrFamily(http_addr_t *addr)	/* I - Address */
{
  if (addr)
    return (addr->addr.sa_family);
  else
    return (0);
}


/*
 * 'httpAddrPort()' - Get the port number associated with an address.
 *
 * @since CUPS 1.7/macOS 10.9@
 */

int					/* O - Port number */
httpAddrPort(http_addr_t *addr)		/* I - Address */
{
  if (!addr)
    return (-1);
#ifdef AF_INET6
  else if (addr->addr.sa_family == AF_INET6)
    return (ntohs(addr->ipv6.sin6_port));
#endif /* AF_INET6 */
  else if (addr->addr.sa_family == AF_INET)
    return (ntohs(addr->ipv4.sin_port));
  else
    return (0);
}


/*
 * '_httpAddrSetPort()' - Set the port number associated with an address.
 */

void
_httpAddrSetPort(http_addr_t *addr,	/* I - Address */
                 int         port)	/* I - Port */
{
  if (!addr || port <= 0)
    return;

#ifdef AF_INET6
  if (addr->addr.sa_family == AF_INET6)
    addr->ipv6.sin6_port = htons(port);
  else
#endif /* AF_INET6 */
  if (addr->addr.sa_family == AF_INET)
    addr->ipv4.sin_port = htons(port);
}


/*
 * 'httpAddrString()' - Convert an address to a numeric string.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

char *					/* O - Numeric address string */
httpAddrString(const http_addr_t *addr,	/* I - Address to convert */
               char              *s,	/* I - String buffer */
	       int               slen)	/* I - Length of string */
{
  DEBUG_printf(("httpAddrString(addr=%p, s=%p, slen=%d)", (void *)addr, (void *)s, slen));

 /*
  * Range check input...
  */

  if (!addr || !s || slen <= 2)
  {
    if (s && slen >= 1)
      *s = '\0';

    return (NULL);
  }

#ifdef AF_LOCAL
  if (addr->addr.sa_family == AF_LOCAL)
  {
    if (addr->un.sun_path[0] == '/')
      strlcpy(s, addr->un.sun_path, (size_t)slen);
    else
      strlcpy(s, "localhost", (size_t)slen);
  }
  else
#endif /* AF_LOCAL */
  if (addr->addr.sa_family == AF_INET)
  {
    unsigned temp;			/* Temporary address */

    temp = ntohl(addr->ipv4.sin_addr.s_addr);

    snprintf(s, (size_t)slen, "%d.%d.%d.%d", (temp >> 24) & 255,
             (temp >> 16) & 255, (temp >> 8) & 255, temp & 255);
  }
#ifdef AF_INET6
  else if (addr->addr.sa_family == AF_INET6)
  {
    char	*sptr,			/* Pointer into string */
		temps[64];		/* Temporary string for address */

#  ifdef HAVE_GETNAMEINFO
    if (getnameinfo(&addr->addr, (socklen_t)httpAddrLength(addr), temps, sizeof(temps), NULL, 0, NI_NUMERICHOST))
    {
     /*
      * If we get an error back, then the address type is not supported
      * and we should zero out the buffer...
      */

      s[0] = '\0';

      return (NULL);
    }
    else if ((sptr = strchr(temps, '%')) != NULL)
    {
     /*
      * Convert "%zone" to "+zone" to match URI form...
      */

      *sptr = '+';
    }

#  else
    int		i;			/* Looping var */
    unsigned	temp;			/* Current value */
    const char	*prefix;		/* Prefix for address */


    prefix = "";
    for (sptr = temps, i = 0; i < 4 && addr->ipv6.sin6_addr.s6_addr32[i]; i ++)
    {
      temp = ntohl(addr->ipv6.sin6_addr.s6_addr32[i]);

      snprintf(sptr, sizeof(temps) - (size_t)(sptr - temps), "%s%x", prefix, (temp >> 16) & 0xffff);
      prefix = ":";
      sptr += strlen(sptr);

      temp &= 0xffff;

      if (temp || i == 3 || addr->ipv6.sin6_addr.s6_addr32[i + 1])
      {
        snprintf(sptr, sizeof(temps) - (size_t)(sptr - temps), "%s%x", prefix, temp);
	sptr += strlen(sptr);
      }
    }

    if (i < 4)
    {
      while (i < 4 && !addr->ipv6.sin6_addr.s6_addr32[i])
	i ++;

      if (i < 4)
      {
        snprintf(sptr, sizeof(temps) - (size_t)(sptr - temps), "%s:", prefix);
	prefix = ":";
	sptr += strlen(sptr);

	for (; i < 4; i ++)
	{
          temp = ntohl(addr->ipv6.sin6_addr.s6_addr32[i]);

          if ((temp & 0xffff0000) ||
	      (i > 0 && addr->ipv6.sin6_addr.s6_addr32[i - 1]))
	  {
            snprintf(sptr, sizeof(temps) - (size_t)(sptr - temps), "%s%x", prefix, (temp >> 16) & 0xffff);
	    sptr += strlen(sptr);
          }

          snprintf(sptr, sizeof(temps) - (size_t)(sptr - temps), "%s%x", prefix, temp & 0xffff);
	  sptr += strlen(sptr);
	}
      }
      else if (sptr == s)
      {
       /*
        * Empty address...
	*/

        strlcpy(temps, "::", sizeof(temps));
      }
      else
      {
       /*
	* Empty at end...
	*/

        strlcpy(sptr, "::", sizeof(temps) - (size_t)(sptr - temps));
      }
    }
#  endif /* HAVE_GETNAMEINFO */

   /*
    * Add "[v1." and "]" around IPv6 address to convert to URI form.
    */

    snprintf(s, (size_t)slen, "[v1.%s]", temps);
  }
#endif /* AF_INET6 */
  else
    strlcpy(s, "UNKNOWN", (size_t)slen);

  DEBUG_printf(("1httpAddrString: returning \"%s\"...", s));

  return (s);
}


/*
 * 'httpGetAddress()' - Get the address of the connected peer of a connection.
 *
 * For connections created with @link httpConnect2@, the address is for the
 * server.  For connections created with @link httpAccept@, the address is for
 * the client.
 *
 * Returns @code NULL@ if the socket is currently unconnected.
 *
 * @since CUPS 2.0/OS 10.10@
 */

http_addr_t *				/* O - Connected address or @code NULL@ */
httpGetAddress(http_t *http)		/* I - HTTP connection */
{
  if (http)
    return (http->hostaddr);
  else
    return (NULL);
}


/*
 * 'httpGetHostByName()' - Lookup a hostname or IPv4 address, and return
 *                         address records for the specified name.
 *
 * @deprecated@ @exclude all@
 */

struct hostent *			/* O - Host entry */
httpGetHostByName(const char *name)	/* I - Hostname or IP address */
{
  const char		*nameptr;	/* Pointer into name */
  unsigned		ip[4];		/* IP address components */
  _cups_globals_t	*cg = _cupsGlobals();
  					/* Pointer to library globals */


  DEBUG_printf(("httpGetHostByName(name=\"%s\")", name));

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
    cg->hostent.h_length    = (int)strlen(name) + 1;
    cg->hostent.h_addr_list = cg->ip_ptrs;
    cg->ip_ptrs[0]          = (char *)name;
    cg->ip_ptrs[1]          = NULL;

    DEBUG_puts("1httpGetHostByName: returning domain socket address...");

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

    cg->ip_addr = htonl((((((((unsigned)ip[0] << 8) | (unsigned)ip[1]) << 8) |
                           (unsigned)ip[2]) << 8) |
                         (unsigned)ip[3]));

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

    DEBUG_puts("1httpGetHostByName: returning IPv4 address...");

    return (&cg->hostent);
  }
  else
  {
   /*
    * Use the gethostbyname() function to get the IPv4 address for
    * the name...
    */

    DEBUG_puts("1httpGetHostByName: returning domain lookup address(es)...");

    return (gethostbyname(name));
  }
}


/*
 * 'httpGetHostname()' - Get the FQDN for the connection or local system.
 *
 * When "http" points to a connected socket, return the hostname or
 * address that was used in the call to httpConnect() or httpConnectEncrypt(),
 * or the address of the client for the connection from httpAcceptConnection().
 * Otherwise, return the FQDN for the local system using both gethostname()
 * and gethostbyname() to get the local hostname with domain.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

const char *				/* O - FQDN for connection or system */
httpGetHostname(http_t *http,		/* I - HTTP connection or NULL */
                char   *s,		/* I - String buffer for name */
                int    slen)		/* I - Size of buffer */
{
  if (http)
  {
    if (!s || slen <= 1)
    {
      if (http->hostname[0] == '/')
	return ("localhost");
      else
	return (http->hostname);
    }
    else if (http->hostname[0] == '/')
      strlcpy(s, "localhost", (size_t)slen);
    else
      strlcpy(s, http->hostname, (size_t)slen);
  }
  else
  {
   /*
    * Get the hostname...
    */

    if (!s || slen <= 1)
      return (NULL);

    if (gethostname(s, (size_t)slen) < 0)
      strlcpy(s, "localhost", (size_t)slen);

    if (!strchr(s, '.'))
    {
#ifdef HAVE_SCDYNAMICSTORECOPYCOMPUTERNAME
     /*
      * The hostname is not a FQDN, so use the local hostname from the
      * SystemConfiguration framework...
      */

      SCDynamicStoreRef	sc = SCDynamicStoreCreate(kCFAllocatorDefault,
                                                  CFSTR("libcups"), NULL, NULL);
					/* System configuration data */
      CFStringRef	local = sc ? SCDynamicStoreCopyLocalHostName(sc) : NULL;
					/* Local host name */
      char		localStr[1024];	/* Local host name C string */

      if (local && CFStringGetCString(local, localStr, sizeof(localStr),
                                      kCFStringEncodingUTF8))
      {
       /*
        * Append ".local." to the hostname we get...
	*/

        snprintf(s, (size_t)slen, "%s.local.", localStr);
      }

      if (local)
        CFRelease(local);
      if (sc)
        CFRelease(sc);

#else
     /*
      * The hostname is not a FQDN, so look it up...
      */

      struct hostent	*host;		/* Host entry to get FQDN */

      if ((host = gethostbyname(s)) != NULL && host->h_name)
      {
       /*
        * Use the resolved hostname...
	*/

	strlcpy(s, host->h_name, (size_t)slen);
      }
#endif /* HAVE_SCDYNAMICSTORECOPYCOMPUTERNAME */
    }

   /*
    * Make sure .local hostnames end with a period...
    */

    if (strlen(s) > 6 && !strcmp(s + strlen(s) - 6, ".local"))
      strlcat(s, ".", (size_t)slen);
  }

 /*
  * Convert the hostname to lowercase as needed...
  */

  if (s[0] != '/')
  {
    char	*ptr;			/* Pointer into string */

    for (ptr = s; *ptr; ptr ++)
      *ptr = (char)_cups_tolower((int)*ptr);
  }

 /*
  * Return the hostname with as much domain info as we have...
  */

  return (s);
}


/*
 * 'httpResolveHostname()' - Resolve the hostname of the HTTP connection
 *                           address.
 *
 * @since CUPS 2.0/OS 10.10@
 */

const char *				/* O - Resolved hostname or @code NULL@ */
httpResolveHostname(http_t *http,	/* I - HTTP connection */
                    char   *buffer,	/* I - Hostname buffer */
                    size_t bufsize)	/* I - Size of buffer */
{
  if (!http)
    return (NULL);

  if (isdigit(http->hostname[0] & 255) || http->hostname[0] == '[')
  {
    char	temp[1024];		/* Temporary string */

    if (httpAddrLookup(http->hostaddr, temp, sizeof(temp)))
      strlcpy(http->hostname, temp, sizeof(http->hostname));
    else
      return (NULL);
  }

  if (buffer)
  {
    if (http->hostname[0] == '/')
      strlcpy(buffer, "localhost", bufsize);
    else
      strlcpy(buffer, http->hostname, bufsize);

    return (buffer);
  }
  else if (http->hostname[0] == '/')
    return ("localhost");
  else
    return (http->hostname);
}
