/*
 * Network interface functions for CUPS.
 *
 * Copyright 2007-2010 by Apple Inc.
 * Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * "LICENSE" which should have been included with this file.  If this
 * file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Include necessary headers.
 */

#include "http-private.h"


#ifndef HAVE_GETIFADDRS
/*
 * '_cups_getifaddrs()' - Get a list of network interfaces on the system.
 */

int					/* O - 0 on success, -1 on error */
_cups_getifaddrs(struct ifaddrs **addrs)/* O - List of interfaces */
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

	if ((temp->ifa_broadaddr =
	         calloc(1, sizeof(request.ifr_broadaddr))) != NULL)
	  memcpy(temp->ifa_broadaddr, &(request.ifr_broadaddr),
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
 * '_cups_freeifaddrs()' - Free an interface list...
 */

void
_cups_freeifaddrs(struct ifaddrs *addrs)/* I - Interface list to free */
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
