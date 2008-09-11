/*
 * "$Id$"
 *
 *   Directory services definitions for the Common UNIX Printing System
 *   (CUPS) scheduler.
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Include necessary headers...
 */

#ifdef HAVE_LIBSLP
#  include <slp.h>
#endif /* HAVE_LIBSLP */

#ifdef HAVE_LDAP
#  ifdef __sun
#    include <lber.h>
#  endif /* __sun */
#  include <ldap.h>
#  ifdef HAVE_LDAP_SSL_H
#    include <ldap_ssl.h>
#  endif /* HAVE_LDAP_SSL_H */
#endif /* HAVE_LDAP */

/*
 * Browse protocols...
 */

#define BROWSE_CUPS	1		/* CUPS */
#define	BROWSE_SLP	2		/* SLPv2 */
#define BROWSE_LDAP	4		/* LDAP */
#define BROWSE_DNSSD	8		/* DNS Service Discovery (aka Bonjour) */
#define BROWSE_SMB	16		/* SMB/Samba */
#define BROWSE_LPD	32		/* LPD via xinetd or launchd */
#define BROWSE_ALL	63		/* All protocols */


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
			BrowseWebIF	VALUE(FALSE),
					/* Whether the web interface is advertised */
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
			UseNetworkDefault VALUE(CUPS_DEFAULT_USE_NETWORK_DEFAULT),
					/* Use the network default printer? */
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

#ifdef HAVE_DNSSD
VAR char		*DNSSDName	VALUE(NULL);
					/* Computer/server name */
VAR int			DNSSDPort	VALUE(0);
					/* Port number to register */
VAR cups_array_t	*DNSSDPrinters	VALUE(NULL);
					/* Printers we have registered */
VAR DNSServiceRef	DNSSDRef	VALUE(NULL),
					/* Master DNS-SD service reference */
			WebIFRef	VALUE(NULL),
					/* Service reference for the web interface */
			RemoteRef	VALUE(NULL);
					/* Remote printer browse reference */
#endif /* HAVE_DNSSD */

#ifdef HAVE_LIBSLP
VAR SLPHandle		BrowseSLPHandle	VALUE(NULL);
					/* SLP API handle */
VAR time_t		BrowseSLPRefresh VALUE(0);
					/* Next SLP refresh time */
#endif /* HAVE_LIBSLP */

#ifdef HAVE_LDAP
VAR LDAP		*BrowseLDAPHandle VALUE(NULL);
					/* Handle to LDAP server */
VAR time_t		BrowseLDAPRefresh VALUE(0);
					/* Next LDAP refresh time */
VAR char		*BrowseLDAPBindDN VALUE(NULL),
					/* LDAP login DN */
			*BrowseLDAPDN	VALUE(NULL),
					/* LDAP search DN */
			*BrowseLDAPPassword VALUE(NULL),
					/* LDAP login password */
			*BrowseLDAPServer VALUE(NULL);
					/* LDAP server to use */
VAR int			BrowseLDAPUpdate VALUE(TRUE);
					/* enables LDAP updates */
#  ifdef HAVE_LDAP_SSL
VAR char		*BrowseLDAPCACertFile VALUE(NULL);
					/* LDAP CA CERT file to use */
#  endif /* HAVE_LDAP_SSL */
#endif /* HAVE_LDAP */
VAR char		*LPDConfigFile	VALUE(NULL),
					/* LPD configuration file */
			*SMBConfigFile	VALUE(NULL);
					/* SMB configuration file */


/*
 * Prototypes...
 */

extern void	cupsdDeregisterPrinter(cupsd_printer_t *p, int removeit);
extern void	cupsdLoadRemoteCache(void);
extern void	cupsdRegisterPrinter(cupsd_printer_t *p);
extern void	cupsdRestartPolling(void);
extern void	cupsdSaveRemoteCache(void);
extern void	cupsdSendBrowseList(void);
extern void	cupsdStartBrowsing(void);
extern void	cupsdStartPolling(void);
extern void	cupsdStopBrowsing(void);
extern void	cupsdStopPolling(void);
#ifdef HAVE_DNSSD
extern void	cupsdUpdateDNSSDName(void);
#endif /* HAVE_DNSSD */
#ifdef HAVE_LDAP
extern void	cupsdUpdateLDAPBrowse(void);
#endif /* HAVE_LDAP */
extern void	cupsdUpdateSLPBrowse(void);


/*
 * End of "$Id$".
 */
