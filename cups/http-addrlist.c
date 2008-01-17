/*
 * "$Id: http-addrlist.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   HTTP address list routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
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
 *   httpAddrFreeList() - Free an address list.
 *   httpAddrGetList()  - Get a list of addresses for a hostname.
 */

/*
 * Include necessary headers...
 */

#include "http-private.h"
#include "globals.h"
#include "debug.h"
#include <stdlib.h>


/*
 * 'httpAddrConnect()' - Connect to any of the addresses in the list.
 *
 * @since CUPS 1.2@
 */

http_addrlist_t *			/* O - Connected address or NULL on failure */
httpAddrConnect(
    http_addrlist_t *addrlist,		/* I - List of potential addresses */
    int             *sock)		/* O - Socket */
{
  int	val;				/* Socket option value */


 /*
  * Loop through each address until we connect or run out of addresses...
  */

  while (addrlist)
  {
   /*
    * Create the socket...
    */

    if ((*sock = (int)socket(addrlist->addr.addr.sa_family, SOCK_STREAM, 0)) < 0)
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

   /*
    * Then connect...
    */

    if (!connect(*sock, &(addrlist->addr.addr),
                 httpAddrLength(&(addrlist->addr))))
      break;

   /*
    * Close this socket and move to the next address...
    */

#ifdef WIN32
    closesocket(*sock);
#else
    close(*sock);
#endif /* WIN32 */

    addrlist = addrlist->next;
  }

  return (addrlist);
}


/*
 * 'httpAddrFreeList()' - Free an address list.
 *
 * @since CUPS 1.2@
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
 * @since CUPS 1.2@
 */

http_addrlist_t	*			/* O - List of addresses or NULL */
httpAddrGetList(const char *hostname,	/* I - Hostname, IP address, or NULL for passive listen address */
                int        family,	/* I - Address family or AF_UNSPEC */
		const char *service)	/* I - Service name or port number */
{
  http_addrlist_t	*first,		/* First address in list */
			*addr,		/* Current address in list */
			*temp;		/* New address */


#ifdef DEBUG
  printf("httpAddrGetList(hostname=\"%s\", family=AF_%s, service=\"%s\")\n",
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
  {
#ifdef HAVE_GETADDRINFO
    struct addrinfo	hints,		/* Address lookup hints */
			*results,	/* Address lookup results */
			*current;	/* Current result */
    char		ipv6[1024],	/* IPv6 address */
			*ipv6zone;	/* Pointer to zone separator */
    int			ipv6len;	/* Length of IPv6 address */

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

    if (!getaddrinfo(hostname, service, &hints, &results))
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
      else if (!strcmp(service, "ipp"))
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
    }
#endif /* HAVE_GETADDRINFO */
  }

 /*
  * Detect some common errors and handle them sanely...
  */

  if (!addr && (!hostname || !strcmp(hostname, "localhost")))
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
    else if (!strcmp(service, "ipp"))
      portnum = 631;
    else if (!strcmp(service, "lpd"))
      portnum = 515;
    else if (!strcmp(service, "socket"))
      portnum = 9100;
    else
      return (NULL);

    if (hostname && !strcmp(hostname, "localhost"))
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
	else
          addr = temp;
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
	  httpAddrFreeList(first);
	  return (NULL);
	}

        temp->addr.ipv4.sin_family = AF_INET;
	temp->addr.ipv4.sin_port   = htons(portnum);

        if (!first)
          first = temp;

        if (addr)
	  addr->next = temp;
	else
          addr = temp;
      }
    }
  }

 /*
  * Return the address list...
  */

  return (first);
}


/*
 * End of "$Id: http-addrlist.c 6649 2007-07-11 21:46:42Z mike $".
 */
