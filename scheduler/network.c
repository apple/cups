/*
 * "$Id$"
 *
 *   Network interface functions for the Common UNIX Printing System
 *   (CUPS) scheduler.
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE" which should have been included with this file.  If this
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
 *   cupsdNetIFFind()   - Find a network interface.
 *   cupsdNetIFFree()   - Free the current network interface list.
 *   cupsdNetIFUpdate() - Update the network interface list as needed...
 *   getifaddrs()       - Get a list of network interfaces on the system.
 *   freeifaddrs()      - Free an interface list...
 */

/*
 * Include necessary headers.
 */

#include "cupsd.h"

#include <net/if.h>

#ifdef HAVE_GETIFADDRS
/*
 * Use native getifaddrs() function...
 */
#  include <ifaddrs.h>
#else
/*
 * Use getifaddrs() emulation...
 */

#  include <sys/ioctl.h>
#  ifdef HAVE_SYS_SOCKIO_H
#    include <sys/sockio.h>
#  endif

#  ifdef ifa_dstaddr
#    undef ifa_dstaddr
#  endif /* ifa_dstaddr */
#  ifndef ifr_netmask
#    define ifr_netmask ifr_addr
#  endif /* !ifr_netmask */

struct ifaddrs				/**** Interface Structure ****/
{
  struct ifaddrs	*ifa_next;	/* Next interface in list */
  char			*ifa_name;	/* Name of interface */
  unsigned int		ifa_flags;	/* Flags (up, point-to-point, etc.) */
  struct sockaddr	*ifa_addr,	/* Network address */
			*ifa_netmask,	/* Address mask */
			*ifa_dstaddr;	/* Broadcast or destination address */
  void			*ifa_data;	/* Interface statistics */
};

int	getifaddrs(struct ifaddrs **addrs);
void	freeifaddrs(struct ifaddrs *addrs);
#endif /* HAVE_GETIFADDRS */


/*
 * 'cupsdNetIFFind()' - Find a network interface.
 */

cupsd_netif_t *				/* O - Network interface data */
cupsdNetIFFind(const char *name)	/* I - Name of interface */
{
  cupsd_netif_t	*temp;			/* Current network interface */


 /*
  * Update the interface list as needed...
  */

  cupsdNetIFUpdate();

 /*
  * Search for the named interface...
  */

  for (temp = NetIFList; temp != NULL; temp = temp->next)
    if (!strcasecmp(name, temp->name))
      return (temp);

  return (NULL);
}


/*
 * 'cupsdNetIFFree()' - Free the current network interface list.
 */

void
cupsdNetIFFree(void)
{
  cupsd_netif_t	*next;			/* Next interface in list */


 /*
  * Loop through the interface list and free all the records...
  */

  while (NetIFList != NULL)
  {
    next = NetIFList->next;

    free(NetIFList);

    NetIFList = next;
  }
}


/*
 * 'cupsdNetIFUpdate()' - Update the network interface list as needed...
 */

void
cupsdNetIFUpdate(void)
{
  int			i,		/* Looping var */
			match;		/* Matching address? */
  cupsd_listener_t	*lis;		/* Listen address */
  cupsd_netif_t		*temp;		/* Current interface */
  struct ifaddrs	*addrs,		/* Interface address list */
			*addr;		/* Current interface address */
  http_addrlist_t	*saddr;		/* Current server address */


 /*
  * Update the network interface list no more often than once a
  * minute...
  */

  if ((time(NULL) - NetIFTime) < 60)
    return;

  NetIFTime = time(NULL);

 /*
  * Free the old interfaces...
  */

  cupsdNetIFFree();

 /*
  * Grab a new list of interfaces...
  */

  if (getifaddrs(&addrs) < 0)
    return;

  for (addr = addrs; addr != NULL; addr = addr->ifa_next)
  {
   /*
    * See if this interface address is IPv4 or IPv6...
    */

    if (addr->ifa_addr == NULL ||
        (addr->ifa_addr->sa_family != AF_INET
#ifdef AF_INET6
	 && addr->ifa_addr->sa_family != AF_INET6
#endif
	) ||
        addr->ifa_netmask == NULL || addr->ifa_name == NULL)
      continue;

   /*
    * OK, we have an IPv4/6 address, so create a new list node...
    */

    if ((temp = calloc(1, sizeof(cupsd_netif_t))) == NULL)
      break;

    temp->next = NetIFList;
    NetIFList  = temp;

   /*
    * Then copy all of the information...
    */

    strlcpy(temp->name, addr->ifa_name, sizeof(temp->name));

    if (addr->ifa_addr->sa_family == AF_INET)
    {
     /*
      * Copy IPv4 addresses...
      */

      memcpy(&(temp->address), addr->ifa_addr, sizeof(struct sockaddr_in));
      memcpy(&(temp->mask), addr->ifa_netmask, sizeof(struct sockaddr_in));

      if (addr->ifa_dstaddr)
	memcpy(&(temp->broadcast), addr->ifa_dstaddr,
	       sizeof(struct sockaddr_in));
    }
#ifdef AF_INET6
    else
    {
     /*
      * Copy IPv6 addresses...
      */

      memcpy(&(temp->address), addr->ifa_addr, sizeof(struct sockaddr_in6));
      memcpy(&(temp->mask), addr->ifa_netmask, sizeof(struct sockaddr_in6));

      if (addr->ifa_dstaddr)
	memcpy(&(temp->broadcast), addr->ifa_dstaddr,
	       sizeof(struct sockaddr_in6));
    }
#endif /* AF_INET6 */

    if (!(addr->ifa_flags & IFF_POINTOPOINT) &&
        !httpAddrLocalhost(&(temp->address)))
      temp->is_local = 1;

   /*
    * Determine which port to use when advertising printers...
    */

    for (i = NumListeners, lis = Listeners; i > 0; i --, lis ++)
    {
      match = 0;

      if (httpAddrAny(&(lis->address)))
        match = 1;
      else if (addr->ifa_addr->sa_family == AF_INET &&
               lis->address.addr.sa_family == AF_INET &&
               (lis->address.ipv4.sin_addr.s_addr &
	           temp->mask.ipv4.sin_addr.s_addr) ==
	               temp->address.ipv4.sin_addr.s_addr)
        match = 1;
#ifdef AF_INET6
      else if (addr->ifa_addr->sa_family == AF_INET6 &&
               lis->address.addr.sa_family == AF_INET6 &&
               (lis->address.ipv6.sin6_addr.s6_addr[0] &
	           temp->mask.ipv6.sin6_addr.s6_addr[0]) ==
	               temp->address.ipv6.sin6_addr.s6_addr[0] &&
               (lis->address.ipv6.sin6_addr.s6_addr[1] &
	           temp->mask.ipv6.sin6_addr.s6_addr[1]) ==
	               temp->address.ipv6.sin6_addr.s6_addr[1] &&
               (lis->address.ipv6.sin6_addr.s6_addr[2] &
	           temp->mask.ipv6.sin6_addr.s6_addr[2]) ==
	               temp->address.ipv6.sin6_addr.s6_addr[2] &&
               (lis->address.ipv6.sin6_addr.s6_addr[3] &
	           temp->mask.ipv6.sin6_addr.s6_addr[3]) ==
	               temp->address.ipv6.sin6_addr.s6_addr[3])
        match = 1;
#endif /* AF_INET6 */

      if (match)
      {
        if (lis->address.addr.sa_family == AF_INET)
          temp->port = ntohs(lis->address.ipv4.sin_port);
#ifdef AF_INET6
        else if (lis->address.addr.sa_family == AF_INET6)
          temp->port = ntohs(lis->address.ipv6.sin6_port);
#endif /* AF_INET6 */
	break;
      }
    }

   /*
    * Finally, try looking up the hostname for the address as needed...
    */

    if (HostNameLookups)
      httpAddrLookup(&(temp->address), temp->hostname, sizeof(temp->hostname));
    else
    {
     /*
      * Map the default server address and localhost to the server name
      * and localhost, respectively; for all other addresses, use the
      * dotted notation...
      */

      if (httpAddrLocalhost(&(temp->address)))
        strcpy(temp->hostname, "localhost");
      else
      {
        for (saddr = ServerAddrs; saddr; saddr = saddr->next)
	  if (httpAddrEqual(&(temp->address), &(saddr->addr)))
	    break;

	if (saddr)
          strlcpy(temp->hostname, ServerName, sizeof(temp->hostname));
	else
          httpAddrString(&(temp->address), temp->hostname,
	        	 sizeof(temp->hostname));
      }
    }

    cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdNetIFUpdate: \"%s\" = %s...",
                    temp->name, temp->hostname);
  }

  freeifaddrs(addrs);
}


#ifndef HAVE_GETIFADDRS
/*
 * 'getifaddrs()' - Get a list of network interfaces on the system.
 */

int					/* O - 0 on success, -1 on error */
getifaddrs(struct ifaddrs **addrs)	/* O - List of interfaces */
{
  int			sock;		/* Socket */
  char			buffer[65536],	/* Buffer for address info */
			*bufptr,	/* Pointer into buffer */
			*bufend;	/* End of buffer */
  struct ifconf		conf;		/* Interface configurations */
  struct sockaddr	addr;		/* Address data */
  struct ifreq		*ifp;		/* Interface data */
  int			ifpsize;	/* Size of interface data */
  struct ifaddrs	*temp;		/* Pointer to current interface */
  struct ifreq		request;	/* Interface request */


 /*
  * Start with an empty list...
  */

  if (addrs == NULL)
    return (-1);

  *addrs = NULL;

 /*
  * Create a UDP socket to get the interface data...
  */

  memset (&addr, 0, sizeof(addr));
  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    return (-1);

 /*
  * Try to get the list of interfaces...
  */

  conf.ifc_len = sizeof(buffer);
  conf.ifc_buf = buffer;

  if (ioctl(sock, SIOCGIFCONF, &conf) < 0)
  {
   /*
    * Couldn't get the list of interfaces...
    */

    close(sock);
    return (-1);
  }

 /*
  * OK, got the list of interfaces, now lets step through the
  * buffer to pull them out...
  */

#  ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
#    define sockaddr_len(a)	((a)->sa_len)
#  else
#    define sockaddr_len(a)	(sizeof(struct sockaddr))
#  endif /* HAVE_STRUCT_SOCKADDR_SA_LEN */

  for (bufptr = buffer, bufend = buffer + conf.ifc_len;
       bufptr < bufend;
       bufptr += ifpsize)
  {
   /*
    * Get the current interface information...
    */

    ifp     = (struct ifreq *)bufptr;
    ifpsize = sizeof(ifp->ifr_name) + sockaddr_len(&(ifp->ifr_addr));

    if (ifpsize < sizeof(struct ifreq))
      ifpsize = sizeof(struct ifreq);

    memset(&request, 0, sizeof(request));
    memcpy(request.ifr_name, ifp->ifr_name, sizeof(ifp->ifr_name));

   /*
    * Check the status of the interface...
    */

    if (ioctl(sock, SIOCGIFFLAGS, &request) < 0)
      continue;

   /*
    * Allocate memory for a single interface record...
    */

    if ((temp = calloc(1, sizeof(struct ifaddrs))) == NULL)
    {
     /*
      * Unable to allocate memory...
      */

      close(sock);
      return (-1);
    }

   /*
    * Add this record to the front of the list and copy the name, flags,
    * and network address...
    */

    temp->ifa_next  = *addrs;
    *addrs          = temp;
    temp->ifa_name  = strdup(ifp->ifr_name);
    temp->ifa_flags = request.ifr_flags;
    if ((temp->ifa_addr = calloc(1, sockaddr_len(&(ifp->ifr_addr)))) != NULL)
      memcpy(temp->ifa_addr, &(ifp->ifr_addr), sockaddr_len(&(ifp->ifr_addr)));

   /*
    * Try to get the netmask for the interface...
    */

    if (!ioctl(sock, SIOCGIFNETMASK, &request))
    {
     /*
      * Got it, make a copy...
      */

      if ((temp->ifa_netmask = calloc(1, sizeof(request.ifr_netmask))) != NULL)
	memcpy(temp->ifa_netmask, &(request.ifr_netmask),
	       sizeof(request.ifr_netmask));
    }

   /*
    * Then get the broadcast or point-to-point (destination) address,
    * if applicable...
    */

    if (temp->ifa_flags & IFF_BROADCAST)
    {
     /*
      * Have a broadcast address, so get it!
      */

      if (!ioctl(sock, SIOCGIFBRDADDR, &request))
      {
       /*
	* Got it, make a copy...
	*/

	if ((temp->ifa_dstaddr = calloc(1, sizeof(request.ifr_broadaddr))) != NULL)
	  memcpy(temp->ifa_dstaddr, &(request.ifr_broadaddr),
		 sizeof(request.ifr_broadaddr));
      }
    }
    else if (temp->ifa_flags & IFF_POINTOPOINT)
    {
     /*
      * Point-to-point interface; grab the remote address...
      */

      if (!ioctl(sock, SIOCGIFDSTADDR, &request))
      {
	temp->ifa_dstaddr = malloc(sizeof(request.ifr_dstaddr));
	memcpy(temp->ifa_dstaddr, &(request.ifr_dstaddr),
	       sizeof(request.ifr_dstaddr));
      }
    }
  }

 /*
  * OK, we're done with the socket, close it and return 0...
  */

  close(sock);

  return (0);
}


/*
 * 'freeifaddrs()' - Free an interface list...
 */

void
freeifaddrs(struct ifaddrs *addrs)	/* I - Interface list to free */
{
  struct ifaddrs	*next;		/* Next interface in list */


  while (addrs != NULL)
  {
   /*
    * Make a copy of the next interface pointer...
    */

    next = addrs->ifa_next;

   /*
    * Free data values as needed...
    */

    if (addrs->ifa_name)
    {
      free(addrs->ifa_name);
      addrs->ifa_name = NULL;
    }

    if (addrs->ifa_addr)
    {
      free(addrs->ifa_addr);
      addrs->ifa_addr = NULL;
    }

    if (addrs->ifa_netmask)
    {
      free(addrs->ifa_netmask);
      addrs->ifa_netmask = NULL;
    }

    if (addrs->ifa_dstaddr)
    {
      free(addrs->ifa_dstaddr);
      addrs->ifa_dstaddr = NULL;
    }

   /*
    * Free this node and continue to the next...
    */

    free(addrs);

    addrs = next;
  }
}
#endif /* !HAVE_GETIFADDRS */


/*
 * End of "$Id$".
 */
