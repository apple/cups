/*
 * "$Id: dirsvc.h,v 1.12.2.9 2004/06/29 13:15:10 mike Exp $"
 *
 *   Directory services definitions for the Common UNIX Printing System
 *   (CUPS) scheduler.
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
 *       Hollywood, Maryland 20636-3142 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

/*
 * Include necessary headers...
 */

#ifdef HAVE_LIBSLP
#  include <slp.h>
#endif /* HAVE_LIBSLP */


/*
 * Browse protocols...
 */

#define BROWSE_CUPS	1		/* CUPS */
#define	BROWSE_SLP	2		/* SLPv2 */
#define BROWSE_LDAP	4		/* LDAP (not supported yet) */
#define BROWSE_ALL	7		/* All protocols */


/*
 * Browse address...
 */

typedef struct
{
  char			iface[32];	/* Destination interface */
  http_addr_t		to;		/* Destination address */
} dirsvc_addr_t;


/*
 * Relay structure...
 */

typedef struct
{
  authmask_t		from;		/* Source address/name mask */
  http_addr_t		to;		/* Destination address */
} dirsvc_relay_t;


/*
 * Polling structure...
 */

typedef struct
{
  char			hostname[64];	/* Hostname (actually, IP address) */
  int			port;		/* Port number */
  int			pid;		/* Current poll server PID */
} dirsvc_poll_t;


/*
 * Globals...
 */

VAR int			Browsing	VALUE(TRUE),
					/* Whether or not browsing is enabled */
			BrowseProtocols	VALUE(BROWSE_ALL),
					/* Protocols to support */
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
VAR dirsvc_addr_t	*Browsers	VALUE(NULL);
					/* Broadcast addresses */
VAR location_t		*BrowseACL	VALUE(NULL);
					/* Browser access control list */
VAR printer_t		*BrowseNext	VALUE(NULL);
					/* Next class/printer to broadcast */
VAR int			NumRelays	VALUE(0);
					/* Number of broadcast relays */
VAR dirsvc_relay_t	*Relays		VALUE(NULL);
					/* Broadcast relays */
VAR int			NumPolled	VALUE(0);
					/* Number of polled servers */
VAR dirsvc_poll_t	*Polled		VALUE(NULL);
					/* Polled servers */
VAR int			PollPipe	VALUE(0);
					/* Status pipe for pollers */

#ifdef HAVE_LIBSLP
VAR SLPHandle		BrowseSLPHandle	VALUE(NULL);
					/* SLP API handle */
VAR time_t		BrowseSLPRefresh VALUE(0);
					/* Next SLP refresh time */
#endif /* HAVE_LIBSLP */


/*
 * Prototypes...
 */

extern void	ProcessBrowseData(const char *uri, cups_ptype_t type,
		                  ipp_pstate_t state, const char *location,
				  const char *info, const char *make_model);
extern void	SendBrowseList(void);
extern void	SendCUPSBrowse(printer_t *p);
extern void	SendSLPBrowse(printer_t *p);
extern void	StartBrowsing(void);
extern void	StartPolling(void);
extern void	StopBrowsing(void);
extern void	StopPolling(void);
extern void	UpdateCUPSBrowse(void);
extern void	UpdatePolling(void);
extern void	UpdateSLPBrowse(void);


/*
 * End of "$Id: dirsvc.h,v 1.12.2.9 2004/06/29 13:15:10 mike Exp $".
 */
