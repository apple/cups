/*
 * "$Id: dirsvc.h,v 1.12 2001/01/22 15:03:59 mike Exp $"
 *
 *   Directory services definitions for the Common UNIX Printing System
 *   (CUPS) scheduler.
 *
 *   Copyright 1997-2001 by Easy Software Products, all rights reserved.
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
 */

/*
 * Relay structure...
 */

typedef struct
{
  authmask_t		from;		/* Source address/name mask */
  struct sockaddr_in	to;		/* Destination address */
} dirsvc_relay_t;


/*
 * Polling structure...
 */

typedef struct
{
  char			hostname[16];	/* Hostname (actually, IP address) */
  int			port;		/* Port number */
  int			pid;		/* Current poll server PID */
} dirsvc_poll_t;


/*
 * Globals...
 */

VAR int			Browsing	VALUE(TRUE),
					/* Whether or not browsing is enabled */
			BrowseShortNames VALUE(TRUE),
					/* Short names for remote printers? */
			BrowseSocket	VALUE(-1),
					/* Socket for browsing */
			BrowsePort	VALUE(IPP_PORT),
					/* Port number for broadcasts */
			BrowseInterval	VALUE(DEFAULT_INTERVAL),
					/* Broadcast interval in seconds */
			BrowseTimeout	VALUE(DEFAULT_TIMEOUT),
					/* Time out for printers in seconds */
			NumBrowsers	VALUE(0);
					/* Number of broadcast addresses */
VAR struct sockaddr_in	Browsers[MAX_BROWSERS];
					/* Broadcast addresses */
VAR location_t		*BrowseACL	VALUE(NULL);
					/* Browser access control list */
VAR int			NumRelays	VALUE(0);
					/* Number of broadcast relays */
VAR dirsvc_relay_t	Relays[MAX_BROWSERS];
					/* Broadcast relays */
VAR int			NumPolled	VALUE(0);
					/* Number of polled servers */
VAR dirsvc_poll_t	Polled[MAX_BROWSERS];
					/* Polled servers */


/*
 * Prototypes...
 */

extern void	StartBrowsing(void);
extern void	StopBrowsing(void);
extern void	UpdateBrowseList(void);
extern void	SendBrowseList(void);

extern void	StartPolling(void);
extern void	StopPolling(void);


/*
 * End of "$Id: dirsvc.h,v 1.12 2001/01/22 15:03:59 mike Exp $".
 */
