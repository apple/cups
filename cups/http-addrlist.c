/*
 * "$Id$"
 *
 *   HTTP address list routines for the Common UNIX Printing System (CUPS).
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
 *   httpAddrConnect()  - Connect to any of the addresses in the list.
 *   httpAddrFreeList() - Free an address list.
 *   httpAddrGetList()  - Get a list of address for a hostname.
 */

/*
 * Include necessary headers...
 */

#include "globals.h"
#include "debug.h"
#include <stdlib.h>
#include "http-private.h"


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

    if ((*sock = socket(addrlist->addr.addr.sa_family, SOCK_STREAM, 0)) < 0)
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
    setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

#ifdef SO_REUSEPORT
    val = 1;
    setsockopt(*sock, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
#endif /* SO_REUSEPORT */

   /*
    * Using TCP_NODELAY improves responsiveness, especially on systems
    * with a slow loopback interface...
    */

    val = 1;
    setsockopt(*sock, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)); 

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

    closesocket(*sock);

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
 * 'httpAddrGetList()' - Get a list of address for a hostname.
 *
 * @since CUPS 1.2@
 */

http_addrlist_t	*			/* O - List of addresses or NULL */
httpAddrGetList(const char *hostname,	/* I - Hostname or IP address */
                int        family,	/* I - Address family or AF_UNSPEC */
		const char *service)	/* I - Service name or port number */
{
  http_addrlist_t	*first,		/* First address in list */
			*addr,		/* Current address in list */
			*temp;		/* New address */


#ifdef DEBUG
  printf("httpAddrGetList(hostname=\"%s\", family=AF_%s, service=\"%s\")\n",
         hostname, family == AF_UNSPEC ? "UNSPEC" :
#  ifdef AF_LOCAL
		       family == AF_LOCAL ? "LOCAL" :
#  endif /* AF_LOCAL */
#  ifdef AF_INET6
		       family == AF_INET6 ? "INET6" :
#  endif /* AF_INET6 */
		       family == AF_INET ? "INET" : "???", service);
#endif /* DEBUG */

 /*
  * Avoid lookup delays and configuration problems when connecting
  * to the localhost address...
  */

  if (!strcmp(hostname, "localhost"))
    hostname = "127.0.0.1";

 /*
  * Lookup the address the best way we can...
  */

  first = addr = NULL;

  if (hostname[0] == '/')
  {
   /*
    * Domain socket address...
    */

    first = (http_addrlist_t *)calloc(1, sizeof(http_addrlist_t));
    first->addr.un.sun_family = AF_LOCAL;
    strlcpy(first->addr.un.sun_path, hostname, sizeof(first->addr.un.sun_path));
  }
  else
  {
#ifdef HAVE_GETADDRINFO
    struct addrinfo	hints,		/* Address lookup hints */
			*results,	/* Address lookup results */
			*current;	/* Current result */



   /*
    * Lookup the address as needed...
    */

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = family;
    hints.ai_socktype = SOCK_STREAM;

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
    int			i;		/* Looping vars */
    unsigned		ip[4];		/* IPv4 address components */
    const char		*ptr;		/* Pointer into hostname */
    struct hostent	*host;		/* Result of lookup */
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

    if (!*nameptr)
    {
     /*
      * We have an IPv4 address; break it up and create an IPv4 address...
      */

      if (sscanf(name, "%u.%u.%u.%u", ip, ip + 1, ip + 2, ip + 3) == 4 &&
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
#ifdef AF_INET6
             (host->h_addrtype == AF_INET || host->h_addrtype == AF_INET6))
#else
             host->h_addrtype == AF_INET)
#endif /* AF_INET6 */
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

#ifdef AF_INET6
        if (host->h_addrtype == AF_INET6)
	{
          first->addr.ipv6.sin6_family = AF_INET6;
	  memcpy(&(temp->addr.ipv6), host->h_addr_list[i],
	         sizeof(temp->addr.ipv6));
          temp->addr.ipv6.sin6_port = htons(portnum);
	}
	else
#endif /* AF_INET6 */
	{
          first->addr.ipv4.sin_family = AF_INET;
	  memcpy(&(temp->addr.ipv4), host->h_addr_list[i],
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
#endif /* HAVE_GETADDRINFO */
  }

 /*
  * Return the address list...
  */

  return (first);
}


/*
 * End of "$Id$".
 */
