/*
 * "$Id: http-addr.c,v 1.8 2004/03/19 11:55:44 mike Exp $"
 *
 *   HTTP host/address routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2004 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   httpGetHostByName() - Lookup a hostname or IP address, and return
 *                         address records for the specified name.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "string.h"

#include "http.h"


/*
 * 'httpGetHostByName()' - Lookup a hostname or IP address, and return
 *                         address records for the specified name.
 */

struct hostent *			/* O - Host entry */
httpGetHostByName(const char *name)	/* I - Hostname or IP address */
{
  const char		*nameptr;	/* Pointer into name */
  unsigned		ip[4];		/* IP address components */
  static unsigned	packed_ip;	/* Packed IPv4 address */
  static char		*packed_ptr[2];	/* Pointer to packed address */
  static struct hostent	host_ip;	/* Host entry for IP address */


#if defined(__APPLE__)
  /* OS X hack to avoid it's ocassional long delay in lookupd */
  static const char sLoopback[] = "127.0.0.1";
  if (strcmp(name, "localhost") == 0)
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
  */

  for (nameptr = name; isdigit(*nameptr & 255) || *nameptr == '.'; nameptr ++);

  if (!*nameptr)
  {
   /*
    * We have an IP address; break it up and provide the host entry
    * to the caller.  Currently only supports IPv4 addresses, although
    * it should be trivial to support IPv6 in CUPS 1.2.
    */

    if (sscanf(name, "%u.%u.%u.%u", ip, ip + 1, ip + 2, ip + 3) != 4)
      return (NULL);			/* Must have 4 numbers */

    if (ip[0] > 255 || ip[1] > 255 || ip[2] > 255 || ip[3] > 255)
      return (NULL);			/* Invalid byte ranges! */

    packed_ip = htonl(((((((ip[0] << 8) | ip[1]) << 8) | ip[2]) << 8) | ip[3]));

   /*
    * Fill in the host entry and return it...
    */

    host_ip.h_name      = (char *)name;
    host_ip.h_aliases   = NULL;
    host_ip.h_addrtype  = AF_INET;
    host_ip.h_length    = 4;
    host_ip.h_addr_list = packed_ptr;
    packed_ptr[0]       = (char *)(&packed_ip);
    packed_ptr[1]       = NULL;

    return (&host_ip);
  }
  else
  {
   /*
    * Use the gethostbyname() function to get the IP address for
    * the name...
    */

    return (gethostbyname(name));
  }
}


/*
 * End of "$Id: http-addr.c,v 1.8 2004/03/19 11:55:44 mike Exp $".
 */
