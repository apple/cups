/*
 * Network interface definitions for the CUPS scheduler.
 *
 * Copyright © 2007-2010 by Apple Inc.
 * Copyright © 1997-2006 by Easy Software Products, all rights reserved.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
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
