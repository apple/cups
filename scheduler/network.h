/*
 * "$Id: network.h,v 1.1.2.2 2003/01/07 18:27:26 mike Exp $"
 *
 *   Network interface definitions for the Common UNIX Printing System
 *   (CUPS) scheduler.
 *
 *   Copyright 1997-2003 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

/*
 * Structures...
 */

typedef struct cups_netif_str		/**** Network interface data ****/
{
  struct cups_netif_str	*next;		/* Next interface in list */
  char			name[32],	/* Network interface name */
			hostname[HTTP_MAX_HOST];
					/* Hostname associated with interface */
  int			is_local;	/* Local (not point-to-point) interface? */
  http_addr_t		address,	/* Network address */
			mask,		/* Network mask */
			broadcast;	/* Broadcast address */
} cups_netif_t;


/*
 * Globals...
 */

VAR time_t		NetIFTime	VALUE(0);
					/* Network interface list time */
VAR cups_netif_t	*NetIFList	VALUE(NULL);
					/* List of network interfaces */

/*
 * Prototypes...
 */

extern cups_netif_t	*NetIFFind(const char *name);
extern void		NetIFFree(void);
extern void		NetIFUpdate(void);


/*
 * End of "$Id: network.h,v 1.1.2.2 2003/01/07 18:27:26 mike Exp $".
 */
