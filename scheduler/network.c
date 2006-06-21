/*
 * "$Id$"
 *
 *   Network interface functions for the Common UNIX Printing System
 *   (CUPS) scheduler.
 *
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
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
 *   compare_netif()    - Compare two network interfaces.
 */

/*
 * Include necessary headers.
 */

#include <cups/http-private.h>
#include "cupsd.h"


/*
 * Local functions...
 */

static void	cupsdNetIFFree(void);
static int	compare_netif(cupsd_netif_t *a, cupsd_netif_t *b);


/*
 * 'cupsdNetIFFind()' - Find a network interface.
 */

cupsd_netif_t *				/* O - Network interface data */
cupsdNetIFFind(const char *name)	/* I - Name of interface */
{
  cupsd_netif_t	key;			/* Search key */


 /*
  * Update the interface list as needed...
  */

  cupsdNetIFUpdate();

 /*
  * Search for the named interface...
  */

  strlcpy(key.name, name, sizeof(key.name));

  return ((cupsd_netif_t *)cupsArrayFind(NetIFList, &key));
}


/*
 * 'cupsdNetIFFree()' - Free the current network interface list.
 */

static void
cupsdNetIFFree(void)
{
  cupsd_netif_t	*current;		/* Current interface in array */


 /*
  * Loop through the interface list and free all the records...
  */

  for (current = (cupsd_netif_t *)cupsArrayFirst(NetIFList);
       current;
       current = (cupsd_netif_t *)cupsArrayNext(NetIFList))
  {
    cupsArrayRemove(NetIFList, current);
    free(current);
  }
}


/*
 * 'cupsdNetIFUpdate()' - Update the network interface list as needed...
 */

void
cupsdNetIFUpdate(void)
{
  int			match;		/* Matching address? */
  cupsd_listener_t	*lis;		/* Listen address */
  cupsd_netif_t		*temp;		/* New interface */
  struct ifaddrs	*addrs,		/* Interface address list */
			*addr;		/* Current interface address */
  http_addrlist_t	*saddr;		/* Current server address */
  char			hostname[1024];	/* Hostname for address */


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
  * Make sure we have an array...
  */

  if (!NetIFList)
    NetIFList = cupsArrayNew((cups_array_func_t)compare_netif, NULL);

  if (!NetIFList)
    return;

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
    * Try looking up the hostname for the address as needed...
    */

    if (HostNameLookups)
      httpAddrLookup((http_addr_t *)(addr->ifa_addr), hostname,
                     sizeof(hostname));
    else
    {
     /*
      * Map the default server address and localhost to the server name
      * and localhost, respectively; for all other addresses, use the
      * dotted notation...
      */

      if (httpAddrLocalhost((http_addr_t *)(addr->ifa_addr)))
        strcpy(hostname, "localhost");
      else
      {
        for (saddr = ServerAddrs; saddr; saddr = saddr->next)
	  if (httpAddrEqual((http_addr_t *)(addr->ifa_addr), &(saddr->addr)))
	    break;

	if (saddr)
          strlcpy(hostname, ServerName, sizeof(hostname));
	else
          httpAddrString((http_addr_t *)(addr->ifa_addr), hostname,
	        	 sizeof(hostname));
      }
    }

   /*
    * Create a new address element...
    */

    if ((temp = calloc(1, sizeof(cupsd_netif_t) +
                          strlen(hostname))) == NULL)
      break;

   /*
    * Copy all of the information...
    */

    strlcpy(temp->name, addr->ifa_name, sizeof(temp->name));
    strcpy(temp->hostname, hostname);	/* Safe because hostname is allocated */

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

    for (lis = (cupsd_listener_t *)cupsArrayFirst(Listeners);
         lis;
	 lis = (cupsd_listener_t *)cupsArrayNext(Listeners))
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
    * Add it to the array...
    */

    cupsArrayAdd(NetIFList, temp);

    cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdNetIFUpdate: \"%s\" = %s...",
                    temp->name, temp->hostname);
  }

  freeifaddrs(addrs);
}


/*
 * 'compare_netif()' - Compare two network interfaces.
 */

static int				/* O - Result of comparison */
compare_netif(cupsd_netif_t *a,		/* I - First network interface */
              cupsd_netif_t *b)		/* I - Second network interface */
{
  return (strcmp(a->name, b->name));
}


/*
 * End of "$Id$".
 */
