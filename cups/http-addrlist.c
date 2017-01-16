/*
 * HTTP address list routines for CUPS.
 *
 * Copyright 2007-2016 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products, all rights reserved.
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
#ifdef HAVE_RESOLV_H
#  include <resolv.h>
#endif /* HAVE_RESOLV_H */
#ifdef HAVE_POLL
#  include <poll.h>
#endif /* HAVE_POLL */
#ifndef WIN32
#  include <fcntl.h>
#endif /* WIN32 */


/*
 * 'httpAddrConnect()' - Connect to any of the addresses in the list.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

http_addrlist_t *			/* O - Connected address or NULL on failure */
httpAddrConnect(
    http_addrlist_t *addrlist,		/* I - List of potential addresses */
    int             *sock)		/* O - Socket */
{
  DEBUG_printf(("httpAddrConnect(addrlist=%p, sock=%p)", (void *)addrlist, (void *)sock));

  return (httpAddrConnect2(addrlist, sock, 30000, NULL));
}


/*
 * 'httpAddrConnect2()' - Connect to any of the addresses in the list with a
 *                        timeout and optional cancel.
 *
 * @since CUPS 1.7/macOS 10.9@
 */

http_addrlist_t *			/* O - Connected address or NULL on failure */
httpAddrConnect2(
    http_addrlist_t *addrlist,		/* I - List of potential addresses */
    int             *sock,		/* O - Socket */
    int             msec,		/* I - Timeout in milliseconds */
    int             *cancel)		/* I - Pointer to "cancel" variable */
{
  int			val;		/* Socket option value */
#ifndef WIN32
  int			flags;		/* Socket flags */
#endif /* !WIN32 */
  int			remaining;	/* Remaining timeout */
  int			i,		/* Looping var */
			nfds,		/* Number of file descriptors */
			fds[100],	/* Socket file descriptors */
			result;		/* Result from select() or poll() */
  http_addrlist_t	*addrs[100];	/* Addresses */
#ifndef HAVE_POLL
  int			max_fd = -1;	/* Highest file descriptor */
#endif /* !HAVE_POLL */
#ifdef O_NONBLOCK
#  ifdef HAVE_POLL
  struct pollfd		pfds[100];	/* Polled file descriptors */
#  else
  fd_set		input_set,	/* select() input set */
			output_set,	/* select() output set */
			error_set;	/* select() error set */
  struct timeval	timeout;	/* Timeout */
#  endif /* HAVE_POLL */
#endif /* O_NONBLOCK */
#ifdef DEBUG
  socklen_t		len;		/* Length of value */
  http_addr_t		peer;		/* Peer address */
  char			temp[256];	/* Temporary address string */
#endif /* DEBUG */


  DEBUG_printf(("httpAddrConnect2(addrlist=%p, sock=%p, msec=%d, cancel=%p)", (void *)addrlist, (void *)sock, msec, (void *)cancel));

  if (!sock)
  {
    errno = EINVAL;
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
    return (NULL);
  }

  if (cancel && *cancel)
    return (NULL);

  if (msec <= 0)
    msec = INT_MAX;

 /*
  * Loop through each address until we connect or run out of addresses...
  */

  nfds      = 0;
  remaining = msec;

  while (remaining > 0)
  {
    if (cancel && *cancel)
    {
      while (nfds > 0)
      {
        nfds --;
	httpAddrClose(NULL, fds[nfds]);
      }

      return (NULL);
    }

    if (addrlist && nfds < (int)(sizeof(fds) / sizeof(fds[0])))
    {
     /*
      * Create the socket...
      */

      DEBUG_printf(("2httpAddrConnect2: Trying %s:%d...", httpAddrString(&(addrlist->addr), temp, sizeof(temp)), httpAddrPort(&(addrlist->addr))));

      if ((fds[nfds] = (int)socket(httpAddrFamily(&(addrlist->addr)), SOCK_STREAM, 0)) < 0)
      {
       /*
	* Don't abort yet, as this could just be an issue with the local
	* system not being configured with IPv4/IPv6/domain socket enabled.
	*
	* Just skip this address...
	*/

        addrlist = addrlist->next;
	continue;
      }

     /*
      * Set options...
      */

      val = 1;
      setsockopt(fds[nfds], SOL_SOCKET, SO_REUSEADDR, CUPS_SOCAST &val, sizeof(val));

#ifdef SO_REUSEPORT
      val = 1;
      setsockopt(fds[nfds], SOL_SOCKET, SO_REUSEPORT, CUPS_SOCAST &val, sizeof(val));
#endif /* SO_REUSEPORT */

#ifdef SO_NOSIGPIPE
      val = 1;
      setsockopt(fds[nfds], SOL_SOCKET, SO_NOSIGPIPE, CUPS_SOCAST &val, sizeof(val));
#endif /* SO_NOSIGPIPE */

     /*
      * Using TCP_NODELAY improves responsiveness, especially on systems
      * with a slow loopback interface...
      */

      val = 1;
      setsockopt(fds[nfds], IPPROTO_TCP, TCP_NODELAY, CUPS_SOCAST &val, sizeof(val));

#ifdef FD_CLOEXEC
     /*
      * Close this socket when starting another process...
      */

      fcntl(fds[nfds], F_SETFD, FD_CLOEXEC);
#endif /* FD_CLOEXEC */

#ifdef O_NONBLOCK
     /*
      * Do an asynchronous connect by setting the socket non-blocking...
      */

      DEBUG_printf(("httpAddrConnect2: Setting non-blocking connect()"));

      flags = fcntl(fds[nfds], F_GETFL, 0);
      fcntl(fds[nfds], F_SETFL, flags | O_NONBLOCK);
#endif /* O_NONBLOCK */

     /*
      * Then connect...
      */

      if (!connect(fds[nfds], &(addrlist->addr.addr), (socklen_t)httpAddrLength(&(addrlist->addr))))
      {
	DEBUG_printf(("1httpAddrConnect2: Connected to %s:%d...", httpAddrString(&(addrlist->addr), temp, sizeof(temp)), httpAddrPort(&(addrlist->addr))));

#ifdef O_NONBLOCK
	fcntl(fds[nfds], F_SETFL, flags);
#endif /* O_NONBLOCK */

	*sock = fds[nfds];

	while (nfds > 0)
	{
	  nfds --;
	  httpAddrClose(NULL, fds[nfds]);
	}

	return (addrlist);
      }

#ifdef WIN32
      if (WSAGetLastError() != WSAEINPROGRESS && WSAGetLastError() != WSAEWOULDBLOCK)
#else
      if (errno != EINPROGRESS && errno != EWOULDBLOCK)
#endif /* WIN32 */
      {
	DEBUG_printf(("1httpAddrConnect2: Unable to connect to %s:%d: %s", httpAddrString(&(addrlist->addr), temp, sizeof(temp)), httpAddrPort(&(addrlist->addr)), strerror(errno)));
	httpAddrClose(NULL, fds[nfds]);
	addrlist = addrlist->next;
	continue;
      }

#ifndef WIN32
      fcntl(fds[nfds], F_SETFL, flags);
#endif /* !WIN32 */

#ifndef HAVE_POLL
      if (fds[nfds] > max_fd)
	max_fd = fds[nfds];
#endif /* !HAVE_POLL */

      addrs[nfds] = addrlist;
      nfds ++;
      addrlist = addrlist->next;
    }

    if (!addrlist && nfds == 0)
      break;

   /*
    * See if we can connect to any of the addresses so far...
    */

#ifdef O_NONBLOCK
    DEBUG_puts("1httpAddrConnect2: Finishing async connect()");

    do
    {
      if (cancel && *cancel)
      {
       /*
	* Close this socket and return...
	*/

	DEBUG_puts("1httpAddrConnect2: Canceled connect()");

	while (nfds > 0)
	{
	  nfds --;
	  httpAddrClose(NULL, fds[nfds]);
	}

	*sock = -1;

	return (NULL);
      }

#  ifdef HAVE_POLL
      for (i = 0; i < nfds; i ++)
      {
	pfds[i].fd     = fds[i];
	pfds[i].events = POLLIN | POLLOUT;
      }

      result = poll(pfds, (nfds_t)nfds, addrlist ? 100 : remaining > 250 ? 250 : remaining);

      DEBUG_printf(("1httpAddrConnect2: poll() returned %d (%d)", result, errno));

#  else
      FD_ZERO(&input_set);
      for (i = 0; i < nfds; i ++)
	FD_SET(fds[i], &input_set);
      output_set = input_set;
      error_set  = input_set;

      timeout.tv_sec  = 0;
      timeout.tv_usec = (addrlist ? 100 : remaining > 250 ? 250 : remaining) * 1000;

      result = select(max_fd + 1, &input_set, &output_set, &error_set, &timeout);

      DEBUG_printf(("1httpAddrConnect2: select() returned %d (%d)", result, errno));
#  endif /* HAVE_POLL */
    }
#  ifdef WIN32
    while (result < 0 && (WSAGetLastError() == WSAEINTR || WSAGetLastError() == WSAEWOULDBLOCK));
#  else
    while (result < 0 && (errno == EINTR || errno == EAGAIN));
#  endif /* WIN32 */

    if (result > 0)
    {
      http_addrlist_t *connaddr = NULL;	/* Connected address, if any */

      for (i = 0; i < nfds; i ++)
      {
#  ifdef HAVE_POLL
	DEBUG_printf(("pfds[%d].revents=%x\n", i, pfds[i].revents));
	if (pfds[i].revents && !(pfds[i].revents & (POLLERR | POLLHUP)))
#  else
	if (FD_ISSET(fds[i], &input_set) && !FD_ISSET(fds[i], &error_set))
#  endif /* HAVE_POLL */
	{
	  *sock    = fds[i];
	  connaddr = addrs[i];

#  ifdef DEBUG
	  len   = sizeof(peer);
	  if (!getpeername(fds[i], (struct sockaddr *)&peer, &len))
	    DEBUG_printf(("1httpAddrConnect2: Connected to %s:%d...", httpAddrString(&peer, temp, sizeof(temp)), httpAddrPort(&peer)));
#  endif /* DEBUG */
	}
#  ifdef HAVE_POLL
	else if (pfds[i].revents & (POLLERR | POLLHUP))
#  else
	else if (FD_ISSET(fds[i], &error_set))
#  endif /* HAVE_POLL */
        {
         /*
          * Error on socket, remove from the "pool"...
          */

	  httpAddrClose(NULL, fds[i]);
          nfds --;
          if (i < nfds)
          {
            memmove(fds + i, fds + i + 1, (size_t)(nfds - i) * (sizeof(fds[0])));
            memmove(addrs + i, addrs + i + 1, (size_t)(nfds - i) * (sizeof(addrs[0])));
          }
          i --;
        }
      }

      if (connaddr)
        return (connaddr);
    }
#endif /* O_NONBLOCK */

    if (addrlist)
      remaining -= 100;
    else
      remaining -= 250;
  }

  while (nfds > 0)
  {
    nfds --;
    httpAddrClose(NULL, fds[nfds]);
  }

#ifdef WIN32
  _cupsSetError(IPP_STATUS_ERROR_SERVICE_UNAVAILABLE, "Connection failed", 0);
#else
  _cupsSetError(IPP_STATUS_ERROR_SERVICE_UNAVAILABLE, strerror(errno), 0);
#endif /* WIN32 */

  return (NULL);
}


/*
 * 'httpAddrCopyList()' - Copy an address list.
 *
 * @since CUPS 1.7/macOS 10.9@
 */

http_addrlist_t	*			/* O - New address list or @code NULL@ on error */
httpAddrCopyList(
    http_addrlist_t *src)		/* I - Source address list */
{
  http_addrlist_t	*dst = NULL,	/* First list entry */
			*prev = NULL,	/* Previous list entry */
			*current = NULL;/* Current list entry */


  while (src)
  {
    if ((current = malloc(sizeof(http_addrlist_t))) == NULL)
    {
      current = dst;

      while (current)
      {
        prev    = current;
        current = current->next;

        free(prev);
      }

      return (NULL);
    }

    memcpy(current, src, sizeof(http_addrlist_t));

    current->next = NULL;

    if (prev)
      prev->next = current;
    else
      dst = current;

    prev = current;
    src  = src->next;
  }

  return (dst);
}


/*
 * 'httpAddrFreeList()' - Free an address list.
 *
 * @since CUPS 1.2/macOS 10.5@
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
 * @since CUPS 1.2/macOS 10.5@
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
      addr = first;
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
	    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
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

      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, gai_strerror(error), 0);
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
          first->addr.ipv4.sin_addr.s_addr = htonl((((((((unsigned)ip[0] << 8) |
	                                               (unsigned)ip[1]) << 8) |
						     (unsigned)ip[2]) << 8) |
						   (unsigned)ip[3]));
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

	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, hstrerror(h_errno), 0);
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

      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Unknown service name."), 1);
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
	  _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
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
	  _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
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
	  _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
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
	  _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
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
