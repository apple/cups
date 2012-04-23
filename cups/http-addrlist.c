/*
 * "$Id: http-addrlist.c 7910 2008-09-06 00:25:17Z mike $"
 *
 *   HTTP address list routines for CUPS.
 *
 *   Copyright 2007-2012 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   httpAddrConnect()  - Connect to any of the addresses in the list.
 *   httpAddrConnect2() - Connect to any of the addresses in the list with a
 *                        timeout and optional cancel.
 *   httpAddrFreeList() - Free an address list.
 *   httpAddrGetList()  - Get a list of addresses for a hostname.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#ifdef HAVE_RESOLV_H
#  include <resolv.h>
#endif /* HAVE_RESOLV_H */
#ifdef HAVE_POLL
#  include <poll.h>
#endif /* HAVE_POLL */
#ifndef WIN32
#  include <sys/fcntl.h>
#endif /* WIN32 */


/*
 * 'httpAddrConnect()' - Connect to any of the addresses in the list.
 *
 * @since CUPS 1.2/OS X 10.5@
 */

http_addrlist_t *			/* O - Connected address or NULL on failure */
httpAddrConnect(
    http_addrlist_t *addrlist,		/* I - List of potential addresses */
    int             *sock)		/* O - Socket */
{
  DEBUG_printf(("httpAddrConnect(addrlist=%p, sock=%p)", addrlist, sock));

  return (httpAddrConnect2(addrlist, sock, 30000, NULL));
}


/*
 * 'httpAddrConnect2()' - Connect to any of the addresses in the list with a
 *                        timeout and optional cancel.
 *
 * @since CUPS 1.6/OS X 10.8@
 */

http_addrlist_t *			/* O - Connected address or NULL on failure */
httpAddrConnect2(
    http_addrlist_t *addrlist,		/* I - List of potential addresses */
    int             *sock,		/* O - Socket */
    int             msec,		/* I - Timeout in milliseconds */
    int             *cancel)		/* I - Pointer to "cancel" variable */
{
  int			val;		/* Socket option value */
#ifdef O_NONBLOCK
  socklen_t		len;		/* Length of value */
  http_addr_t		peer;		/* Peer address */
  int			flags,		/* Socket flags */
			remaining;	/* Remaining timeout */
#  ifdef HAVE_POLL
  struct pollfd		pfd;		/* Polled file descriptor */
#  else
  fd_set		input_set,	/* select() input set */
			output_set;	/* select() output set */
  struct timeval	timeout;	/* Timeout */
#  endif /* HAVE_POLL */
  int			nfds;		/* Result from select()/poll() */
#endif /* O_NONBLOCK */
#ifdef DEBUG
  char			temp[256];	/* Temporary address string */
#endif /* DEBUG */


  DEBUG_printf(("httpAddrConnect2(addrlist=%p, sock=%p, msec=%d, cancel=%p)",
                addrlist, sock, msec, cancel));

  if (!sock)
  {
    errno = EINVAL;
    _cupsSetError(IPP_INTERNAL_ERROR, strerror(errno), 0);
    return (NULL);
  }

  if (cancel && *cancel)
    return (NULL);

  if (msec <= 0 || getenv("CUPS_DISABLE_ASYNC_CONNECT"))
    msec = INT_MAX;

 /*
  * Loop through each address until we connect or run out of addresses...
  */

  while (addrlist)
  {
    if (cancel && *cancel)
      return (NULL);

   /*
    * Create the socket...
    */

    DEBUG_printf(("2httpAddrConnect2: Trying %s:%d...",
		  httpAddrString(&(addrlist->addr), temp, sizeof(temp)),
		  _httpAddrPort(&(addrlist->addr))));

    if ((*sock = (int)socket(_httpAddrFamily(&(addrlist->addr)), SOCK_STREAM,
                             0)) < 0)
    {
     /*
      * Don't abort yet, as this could just be an issue with the local
      * system not being configured with IPv4/IPv6/domain socket enabled...
      */

      addrlist = addrlist->next;
      continue;
    }

   /*
    * Set options...
    */

    val = 1;
#ifdef WIN32
    setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&val,
               sizeof(val));
#else
    setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
#endif /* WIN32 */

#ifdef SO_REUSEPORT
    val = 1;
    setsockopt(*sock, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
#endif /* SO_REUSEPORT */

#ifdef SO_NOSIGPIPE
    val = 1;
    setsockopt(*sock, SOL_SOCKET, SO_NOSIGPIPE, &val, sizeof(val));
#endif /* SO_NOSIGPIPE */

   /*
    * Using TCP_NODELAY improves responsiveness, especially on systems
    * with a slow loopback interface...
    */

    val = 1;
#ifdef WIN32
    setsockopt(*sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&val,
               sizeof(val));
#else
    setsockopt(*sock, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));
#endif /* WIN32 */

#ifdef FD_CLOEXEC
   /*
    * Close this socket when starting another process...
    */

    fcntl(*sock, F_SETFD, FD_CLOEXEC);
#endif /* FD_CLOEXEC */

#ifdef O_NONBLOCK
   /*
    * Do an asynchronous connect by setting the socket non-blocking...
    */

    DEBUG_printf(("httpAddrConnect2: Setting non-blocking connect()"));

    flags = fcntl(*sock, F_GETFL, 0);
    if (msec != INT_MAX)
    {
      DEBUG_puts("httpAddrConnect2: Setting non-blocking connect()");

      fcntl(*sock, F_SETFL, flags | O_NONBLOCK);
    }
#endif /* O_NONBLOCK */

   /*
    * Then connect...
    */

    if (!connect(*sock, &(addrlist->addr.addr),
                 httpAddrLength(&(addrlist->addr))))
    {
      DEBUG_printf(("1httpAddrConnect2: Connected to %s:%d...",
		    httpAddrString(&(addrlist->addr), temp, sizeof(temp)),
		    _httpAddrPort(&(addrlist->addr))));

#ifdef O_NONBLOCK
      fcntl(*sock, F_SETFL, flags);
#endif /* O_NONBLOCK */

      return (addrlist);
    }

#ifdef O_NONBLOCK
#  ifdef WIN32
    if (WSAGetLastError() == WSAEINPROGRESS ||
        WSAGetLastError() == WSAEWOULDBLOCK)
#  else
    if (errno == EINPROGRESS || errno == EWOULDBLOCK)
#  endif /* WIN32 */
    {
      DEBUG_puts("1httpAddrConnect2: Finishing async connect()");

      fcntl(*sock, F_SETFL, flags);

      for (remaining = msec; remaining > 0; remaining -= 250)
      {
	do
        {
          if (cancel && *cancel)
          {
	   /*
	    * Close this socket and return...
	    */

            DEBUG_puts("1httpAddrConnect2: Canceled connect()");

#    ifdef WIN32
	    closesocket(*sock);
#    else
	    close(*sock);
#    endif /* WIN32 */

	    *sock = -1;

	    return (NULL);
          }

#  ifdef HAVE_POLL
	  pfd.fd     = *sock;
	  pfd.events = POLLIN | POLLOUT;

          nfds = poll(&pfd, 1, remaining > 250 ? 250 : remaining);

	  DEBUG_printf(("1httpAddrConnect2: poll() returned %d (%d)", nfds,
	                errno));

#  else
	  FD_ZERO(&input_set);
	  FD_SET(*sock, &input_set);
	  output_set = input_set;

	  timeout.tv_sec  = 0;
	  timeout.tv_usec = (remaining > 250 ? 250 : remaining) * 1000;

	  nfds = select(*sock + 1, &input_set, &output_set, NULL, &timeout);

	  DEBUG_printf(("1httpAddrConnect2: select() returned %d (%d)", nfds,
	                errno));
#  endif /* HAVE_POLL */
	}
#  ifdef WIN32
	while (nfds < 0 && (WSAGetLastError() == WSAEINTR ||
			    WSAGetLastError() == WSAEWOULDBLOCK));
#  else
	while (nfds < 0 && (errno == EINTR || errno == EAGAIN));
#  endif /* WIN32 */

        if (nfds > 0)
        {
          len = sizeof(peer);
          if (!getpeername(*sock, (struct sockaddr *)&peer, &len))
          {
	    DEBUG_printf(("1httpAddrConnect2: Connected to %s:%d...",
			  httpAddrString(&peer, temp, sizeof(temp)),
			  _httpAddrPort(&peer)));

	    return (addrlist);
	  }

          break;
        }
      }
    }
#endif /* O_NONBLOCK */

    DEBUG_printf(("1httpAddrConnect2: Unable to connect to %s:%d: %s",
		  httpAddrString(&(addrlist->addr), temp, sizeof(temp)),
		  _httpAddrPort(&(addrlist->addr)), strerror(errno)));

#ifndef WIN32
    if (errno == EINPROGRESS)
      errno = ETIMEDOUT;
#endif /* !WIN32 */

   /*
    * Close this socket and move to the next address...
    */

#ifdef WIN32
    closesocket(*sock);
#else
    close(*sock);
#endif /* WIN32 */

    *sock    = -1;
    addrlist = addrlist->next;
  }

  if (!addrlist)
#ifdef WIN32
    _cupsSetError(IPP_SERVICE_UNAVAILABLE, "Connection failed", 0);
#else
    _cupsSetError(IPP_SERVICE_UNAVAILABLE, strerror(errno), 0);
#endif /* WIN32 */

  return (addrlist);
}


/*
 * 'httpAddrFreeList()' - Free an address list.
 *
 * @since CUPS 1.2/OS X 10.5@
 */

void
httpAddrFreeList(
    http_addrlist_t *addrlist)		/* I - Address list to free */
{
  http_addrlist_t	*next;		/* Next address in list */


 /*
  * Free each address in the list...
  */

  while (addrlist)
  {
    next = addrlist->next;

    free(addrlist);

    addrlist = next;
  }
}


/*
 * 'httpAddrGetList()' - Get a list of addresses for a hostname.
 *
 * @since CUPS 1.2/OS X 10.5@
 */

http_addrlist_t	*			/* O - List of addresses or NULL */
httpAddrGetList(const char *hostname,	/* I - Hostname, IP address, or NULL for passive listen address */
                int        family,	/* I - Address family or AF_UNSPEC */
		const char *service)	/* I - Service name or port number */
{
  http_addrlist_t	*first,		/* First address in list */
			*addr,		/* Current address in list */
			*temp;		/* New address */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */


#ifdef DEBUG
  _cups_debug_printf("httpAddrGetList(hostname=\"%s\", family=AF_%s, "
                     "service=\"%s\")\n",
		     hostname ? hostname : "(nil)",
		     family == AF_UNSPEC ? "UNSPEC" :
#  ifdef AF_LOCAL
	                 family == AF_LOCAL ? "LOCAL" :
#  endif /* AF_LOCAL */
#  ifdef AF_INET6
	                 family == AF_INET6 ? "INET6" :
#  endif /* AF_INET6 */
	                 family == AF_INET ? "INET" : "???", service);
#endif /* DEBUG */

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

 /*
  * Lookup the address the best way we can...
  */

  first = addr = NULL;

#ifdef AF_LOCAL
  if (hostname && hostname[0] == '/')
  {
   /*
    * Domain socket address...
    */

    if ((first = (http_addrlist_t *)calloc(1, sizeof(http_addrlist_t))) != NULL)
    {
      first->addr.un.sun_family = AF_LOCAL;
      strlcpy(first->addr.un.sun_path, hostname, sizeof(first->addr.un.sun_path));
    }
  }
  else
#endif /* AF_LOCAL */
  if (!hostname || _cups_strcasecmp(hostname, "localhost"))
  {
#ifdef HAVE_GETADDRINFO
    struct addrinfo	hints,		/* Address lookup hints */
			*results,	/* Address lookup results */
			*current;	/* Current result */
    char		ipv6[64],	/* IPv6 address */
			*ipv6zone;	/* Pointer to zone separator */
    int			ipv6len;	/* Length of IPv6 address */
    int			error;		/* getaddrinfo() error */


   /*
    * Lookup the address as needed...
    */

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = family;
    hints.ai_flags    = hostname ? 0 : AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;

    if (hostname && *hostname == '[')
    {
     /*
      * Remove brackets from numeric IPv6 address...
      */

      if (!strncmp(hostname, "[v1.", 4))
      {
       /*
        * Copy the newer address format which supports link-local addresses...
	*/

	strlcpy(ipv6, hostname + 4, sizeof(ipv6));
	if ((ipv6len = (int)strlen(ipv6) - 1) >= 0 && ipv6[ipv6len] == ']')
	{
          ipv6[ipv6len] = '\0';
	  hostname      = ipv6;

         /*
	  * Convert "+zone" in address to "%zone"...
	  */

          if ((ipv6zone = strrchr(ipv6, '+')) != NULL)
	    *ipv6zone = '%';
	}
      }
      else
      {
       /*
        * Copy the regular non-link-local IPv6 address...
	*/

	strlcpy(ipv6, hostname + 1, sizeof(ipv6));
	if ((ipv6len = (int)strlen(ipv6) - 1) >= 0 && ipv6[ipv6len] == ']')
	{
          ipv6[ipv6len] = '\0';
	  hostname      = ipv6;
	}
      }
    }

    if ((error = getaddrinfo(hostname, service, &hints, &results)) == 0)
    {
     /*
      * Copy the results to our own address list structure...
      */

      for (current = results; current; current = current->ai_next)
        if (current->ai_family == AF_INET || current->ai_family == AF_INET6)
	{
	 /*
          * Copy the address over...
	  */

	  temp = (http_addrlist_t *)calloc(1, sizeof(http_addrlist_t));
	  if (!temp)
	  {
	    httpAddrFreeList(first);
	    _cupsSetError(IPP_INTERNAL_ERROR, strerror(errno), 0);
	    return (NULL);
	  }

          if (current->ai_family == AF_INET6)
	    memcpy(&(temp->addr.ipv6), current->ai_addr,
	           sizeof(temp->addr.ipv6));
	  else
	    memcpy(&(temp->addr.ipv4), current->ai_addr,
	           sizeof(temp->addr.ipv4));

         /*
	  * Append the address to the list...
	  */

	  if (!first)
	    first = temp;

	  if (addr)
	    addr->next = temp;

	  addr = temp;
	}

     /*
      * Free the results from getaddrinfo()...
      */

      freeaddrinfo(results);
    }
    else
    {
      if (error == EAI_FAIL)
        cg->need_res_init = 1;

      _cupsSetError(IPP_INTERNAL_ERROR, gai_strerror(error), 0);
    }

#else
    if (hostname)
    {
      int		i;		/* Looping vars */
      unsigned		ip[4];		/* IPv4 address components */
      const char	*ptr;		/* Pointer into hostname */
      struct hostent	*host;		/* Result of lookup */
      struct servent	*port;		/* Port number for service */
      int		portnum;	/* Port number */


     /*
      * Lookup the service...
      */

      if (!service)
	portnum = 0;
      else if (isdigit(*service & 255))
	portnum = atoi(service);
      else if ((port = getservbyname(service, NULL)) != NULL)
	portnum = ntohs(port->s_port);
      else if (!strcmp(service, "http"))
        portnum = 80;
      else if (!strcmp(service, "https"))
        portnum = 443;
      else if (!strcmp(service, "ipp") || !strcmp(service, "ipps"))
        portnum = 631;
      else if (!strcmp(service, "lpd"))
        portnum = 515;
      else if (!strcmp(service, "socket"))
        portnum = 9100;
      else
	return (NULL);

     /*
      * This code is needed because some operating systems have a
      * buggy implementation of gethostbyname() that does not support
      * IPv4 addresses.  If the hostname string is an IPv4 address, then
      * sscanf() is used to extract the IPv4 components.  We then pack
      * the components into an IPv4 address manually, since the
      * inet_aton() function is deprecated.  We use the htonl() macro
      * to get the right byte order for the address.
      */

      for (ptr = hostname; isdigit(*ptr & 255) || *ptr == '.'; ptr ++);

      if (!*ptr)
      {
       /*
	* We have an IPv4 address; break it up and create an IPv4 address...
	*/

	if (sscanf(hostname, "%u.%u.%u.%u", ip, ip + 1, ip + 2, ip + 3) == 4 &&
            ip[0] <= 255 && ip[1] <= 255 && ip[2] <= 255 && ip[3] <= 255)
	{
	  first = (http_addrlist_t *)calloc(1, sizeof(http_addrlist_t));
	  if (!first)
	    return (NULL);

          first->addr.ipv4.sin_family = AF_INET;
          first->addr.ipv4.sin_addr.s_addr = htonl(((((((ip[0] << 8) |
	                                               ip[1]) << 8) |
						     ip[2]) << 8) | ip[3]));
          first->addr.ipv4.sin_port = htons(portnum);
	}
      }
      else if ((host = gethostbyname(hostname)) != NULL &&
#  ifdef AF_INET6
               (host->h_addrtype == AF_INET || host->h_addrtype == AF_INET6))
#  else
               host->h_addrtype == AF_INET)
#  endif /* AF_INET6 */
      {
	for (i = 0; host->h_addr_list[i]; i ++)
	{
	 /*
          * Copy the address over...
	  */

	  temp = (http_addrlist_t *)calloc(1, sizeof(http_addrlist_t));
	  if (!temp)
	  {
	    httpAddrFreeList(first);
	    return (NULL);
	  }

#  ifdef AF_INET6
          if (host->h_addrtype == AF_INET6)
	  {
            temp->addr.ipv6.sin6_family = AF_INET6;
	    memcpy(&(temp->addr.ipv6.sin6_addr), host->h_addr_list[i],
	           sizeof(temp->addr.ipv6));
            temp->addr.ipv6.sin6_port = htons(portnum);
	  }
	  else
#  endif /* AF_INET6 */
	  {
            temp->addr.ipv4.sin_family = AF_INET;
	    memcpy(&(temp->addr.ipv4.sin_addr), host->h_addr_list[i],
	           sizeof(temp->addr.ipv4));
            temp->addr.ipv4.sin_port = htons(portnum);
          }

	 /*
	  * Append the address to the list...
	  */

	  if (!first)
	    first = temp;

	  if (addr)
	    addr->next = temp;

	  addr = temp;
	}
      }
      else
      {
        if (h_errno == NO_RECOVERY)
          cg->need_res_init = 1;

	_cupsSetError(IPP_INTERNAL_ERROR, hstrerror(h_errno), 0);
      }
    }
#endif /* HAVE_GETADDRINFO */
  }

 /*
  * Detect some common errors and handle them sanely...
  */

  if (!addr && (!hostname || !_cups_strcasecmp(hostname, "localhost")))
  {
    struct servent	*port;		/* Port number for service */
    int			portnum;	/* Port number */


   /*
    * Lookup the service...
    */

    if (!service)
      portnum = 0;
    else if (isdigit(*service & 255))
      portnum = atoi(service);
    else if ((port = getservbyname(service, NULL)) != NULL)
      portnum = ntohs(port->s_port);
    else if (!strcmp(service, "http"))
      portnum = 80;
    else if (!strcmp(service, "https"))
      portnum = 443;
    else if (!strcmp(service, "ipp") || !strcmp(service, "ipps"))
      portnum = 631;
    else if (!strcmp(service, "lpd"))
      portnum = 515;
    else if (!strcmp(service, "socket"))
      portnum = 9100;
    else
    {
      httpAddrFreeList(first);

      _cupsSetError(IPP_INTERNAL_ERROR, _("Unknown service name."), 1);
      return (NULL);
    }

    if (hostname && !_cups_strcasecmp(hostname, "localhost"))
    {
     /*
      * Unfortunately, some users ignore all of the warnings in the
      * /etc/hosts file and delete "localhost" from it. If we get here
      * then we were unable to resolve the name, so use the IPv6 and/or
      * IPv4 loopback interface addresses...
      */

#ifdef AF_INET6
      if (family != AF_INET)
      {
       /*
        * Add [::1] to the address list...
	*/

	temp = (http_addrlist_t *)calloc(1, sizeof(http_addrlist_t));
	if (!temp)
	{
	  _cupsSetError(IPP_INTERNAL_ERROR, strerror(errno), 0);
	  httpAddrFreeList(first);
	  return (NULL);
	}

        temp->addr.ipv6.sin6_family            = AF_INET6;
	temp->addr.ipv6.sin6_port              = htons(portnum);
#  ifdef WIN32
	temp->addr.ipv6.sin6_addr.u.Byte[15]   = 1;
#  else
	temp->addr.ipv6.sin6_addr.s6_addr32[3] = htonl(1);
#  endif /* WIN32 */

        if (!first)
          first = temp;

        addr = temp;
      }

      if (family != AF_INET6)
#endif /* AF_INET6 */
      {
       /*
        * Add 127.0.0.1 to the address list...
	*/

	temp = (http_addrlist_t *)calloc(1, sizeof(http_addrlist_t));
	if (!temp)
	{
	  _cupsSetError(IPP_INTERNAL_ERROR, strerror(errno), 0);
	  httpAddrFreeList(first);
	  return (NULL);
	}

        temp->addr.ipv4.sin_family      = AF_INET;
	temp->addr.ipv4.sin_port        = htons(portnum);
	temp->addr.ipv4.sin_addr.s_addr = htonl(0x7f000001);

        if (!first)
          first = temp;

        if (addr)
	  addr->next = temp;
      }
    }
    else if (!hostname)
    {
     /*
      * Provide one or more passive listening addresses...
      */

#ifdef AF_INET6
      if (family != AF_INET)
      {
       /*
        * Add [::] to the address list...
	*/

	temp = (http_addrlist_t *)calloc(1, sizeof(http_addrlist_t));
	if (!temp)
	{
	  _cupsSetError(IPP_INTERNAL_ERROR, strerror(errno), 0);
	  httpAddrFreeList(first);
	  return (NULL);
	}

        temp->addr.ipv6.sin6_family = AF_INET6;
	temp->addr.ipv6.sin6_port   = htons(portnum);

        if (!first)
          first = temp;

        addr = temp;
      }

      if (family != AF_INET6)
#endif /* AF_INET6 */
      {
       /*
        * Add 0.0.0.0 to the address list...
	*/

	temp = (http_addrlist_t *)calloc(1, sizeof(http_addrlist_t));
	if (!temp)
	{
	  _cupsSetError(IPP_INTERNAL_ERROR, strerror(errno), 0);
	  httpAddrFreeList(first);
	  return (NULL);
	}

        temp->addr.ipv4.sin_family = AF_INET;
	temp->addr.ipv4.sin_port   = htons(portnum);

        if (!first)
          first = temp;

        if (addr)
	  addr->next = temp;
      }
    }
  }

 /*
  * Return the address list...
  */

  return (first);
}


/*
 * End of "$Id: http-addrlist.c 7910 2008-09-06 00:25:17Z mike $".
 */
