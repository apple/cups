/*
 * "$Id$"
 *
 *   Network interface definitions for the Common UNIX Printing System
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
 */

/*
 * Structures...
 */

typedef struct cupsd_netif_s		/**** Network interface data ****/
{
  struct cupsd_netif_s	*next;		/* Next interface in list */
  char			name[32],	/* Network interface name */
			hostname[HTTP_MAX_HOST];
					/* Hostname associated with interface */
  int			is_local,	/* Local (not point-to-point) interface? */
			port;		/* Listen port */
  http_addr_t		address,	/* Network address */
			mask,		/* Network mask */
			broadcast;	/* Broadcast address */
} cupsd_netif_t;


/*
 * Globals...
 */

VAR time_t		NetIFTime	VALUE(0);
					/* Network interface list time */
VAR cupsd_netif_t	*NetIFList	VALUE(NULL);
					/* List of network interfaces */

/*
 * Prototypes...
 */

extern cupsd_netif_t	*cupsdNetIFFind(const char *name);
extern void		cupsdNetIFFree(void);
extern void		cupsdNetIFUpdate(void);


/*
 * End of "$Id$".
 */
