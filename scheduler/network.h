/*
 * "$Id: network.h 6649 2007-07-11 21:46:42Z mike $"
 *
 *   Network interface definitions for the Common UNIX Printing System
 *   (CUPS) scheduler.
 *
 *   Copyright 2007-2009 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   "LICENSE" which should have been included with this file.  If this
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Structures...
 */

typedef struct cupsd_netif_s		/**** Network interface data ****/
{
  int			is_local,	/* Local (not point-to-point) interface? */
			port;		/* Listen port */
  http_addr_t		address,	/* Network address */
			mask,		/* Network mask */
			broadcast;	/* Broadcast address */
  size_t		hostlen;	/* Length of hostname */
  char			name[32],	/* Network interface name */
			hostname[1];	/* Hostname associated with interface */
} cupsd_netif_t;


/*
 * Globals...
 */

VAR int			NetIFUpdate	VALUE(1);
					/* Network interface list needs updating */
VAR cups_array_t	*NetIFList	VALUE(NULL);
					/* Array of network interfaces */

/*
 * Prototypes...
 */

extern cupsd_netif_t	*cupsdNetIFFind(const char *name);
extern void		cupsdNetIFUpdate(void);


/*
 * End of "$Id: network.h 6649 2007-07-11 21:46:42Z mike $".
 */
