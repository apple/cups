/*
 * "$Id$"
 *
 *   Directory services definitions for the Common UNIX Printing System
 *   (CUPS) scheduler.
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636 USA
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
#define BROWSE_DNSSD	8		/* DNS Service Discovery aka Bonjour */
#define BROWSE_ALL	15		/* All protocols */


/*
 * Browse address...
 */

typedef struct
{
  char			iface[32];	/* Destination interface */
  http_addr_t		to;		/* Destination address */
} cupsd_dirsvc_addr_t;


/*
 * Relay structure...
 */

typedef struct
{
  cupsd_authmask_t	from;		/* Source address/name mask */
  http_addr_t		to;		/* Destination address */
} cupsd_dirsvc_relay_t;


/*
 * Polling structure...
 */

typedef struct
{
  char			hostname[64];	/* Hostname (actually, IP address) */
  int			port;		/* Port number */
  int			pid;		/* Current poll server PID */
} cupsd_dirsvc_poll_t;


/*
 * Globals...
 */

VAR int			Browsing	VALUE(TRUE),
					/* Whether or not browsing is enabled */
			BrowseLocalProtocols
					VALUE(BROWSE_ALL),
					/* Protocols to support for local printers */
			BrowseRemoteProtocols
					VALUE(BROWSE_ALL),
					/* Protocols to support for remote printers */
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
VAR char		*BrowseLocalOptions
					VALUE(NULL),
					/* Options to add to local printer URIs */
			*BrowseRemoteOptions
					VALUE(NULL);
					/* Options to add to remote printer URIs */
VAR cupsd_dirsvc_addr_t	*Browsers	VALUE(NULL);
					/* Broadcast addresses */
VAR cupsd_location_t	*BrowseACL	VALUE(NULL);
					/* Browser access control list */
VAR cupsd_printer_t	*BrowseNext	VALUE(NULL);
					/* Next class/printer to broadcast */
VAR int			NumRelays	VALUE(0);
					/* Number of broadcast relays */
VAR cupsd_dirsvc_relay_t *Relays	VALUE(NULL);
					/* Broadcast relays */
VAR int			NumPolled	VALUE(0);
					/* Number of polled servers */
VAR cupsd_dirsvc_poll_t	*Polled		VALUE(NULL);
					/* Polled servers */
VAR int			PollPipe	VALUE(0);
					/* Status pipe for pollers */
VAR cupsd_statbuf_t	*PollStatusBuffer VALUE(NULL);
					/* Status buffer for pollers */

#ifdef HAVE_LIBSLP
VAR SLPHandle		BrowseSLPHandle	VALUE(NULL);
					/* SLP API handle */
VAR time_t		BrowseSLPRefresh VALUE(0);
					/* Next SLP refresh time */
#endif /* HAVE_LIBSLP */


/*
 * Prototypes...
 */

extern void	cupsdLoadRemoteCache(void);
extern void	cupsdProcessBrowseData(const char *uri, cups_ptype_t type,
		                       ipp_pstate_t state, const char *location,
				       const char *info, const char *make_model,
				       int num_attrs, cups_option_t *attrs);
extern void	cupsdProcessImplicitClasses(void);
extern void	cupsdSaveRemoteCache(void);
extern void	cupsdSendBrowseDelete(cupsd_printer_t *p);
extern void	cupsdSendBrowseList(void);
extern void	cupsdSendCUPSBrowse(cupsd_printer_t *p);
extern void	cupsdSendSLPBrowse(cupsd_printer_t *p);
extern void	cupsdStartBrowsing(void);
extern void	cupsdStartPolling(void);
extern void	cupsdStopBrowsing(void);
extern void	cupsdStopPolling(void);
extern void	cupsdUpdateCUPSBrowse(void);
extern void	cupsdUpdatePolling(void);
extern void	cupsdUpdateSLPBrowse(void);


/*
 * End of "$Id$".
 */
