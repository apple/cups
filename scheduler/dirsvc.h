/*
 * "$Id: dirsvc.h 7933 2008-09-11 00:44:58Z mike $"
 *
 *   Directory services definitions for the CUPS scheduler.
 *
 *   Copyright 2007-2012 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Browse protocols...
 */

#define BROWSE_DNSSD	1		/* DNS Service Discovery (aka Bonjour) */
#define BROWSE_SMB	2		/* SMB/Samba */
#define BROWSE_LPD	4		/* LPD via xinetd or launchd */
#define BROWSE_ALL	7		/* All protocols */


/*
 * Globals...
 */

VAR int			Browsing	VALUE(TRUE),
					/* Whether or not browsing is enabled */
			BrowseWebIF	VALUE(FALSE),
					/* Whether the web interface is advertised */
			BrowseLocalProtocols
					VALUE(BROWSE_ALL);
					/* Protocols to support for local printers */
#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
VAR char		*DNSSDComputerName VALUE(NULL),
					/* Computer/server name */
			*DNSSDHostName	VALUE(NULL),
					/* Hostname */
			*DNSSDSubTypes VALUE(NULL);
					/* Bonjour registration subtypes */
VAR cups_array_t	*DNSSDAlias	VALUE(NULL);
					/* List of dynamic ServerAlias's */
VAR int			DNSSDPort	VALUE(0);
					/* Port number to register */
VAR cups_array_t	*DNSSDPrinters	VALUE(NULL);
					/* Printers we have registered */
#  ifdef HAVE_DNSSD
VAR DNSServiceRef	DNSSDMaster	VALUE(NULL);
					/* Master DNS-SD service reference */
#  else /* HAVE_AVAHI */
VAR AvahiThreadedPoll	*DNSSDMaster	VALUE(NULL);
					/* Master polling interface for Avahi */
VAR AvahiClient		*DNSSDClient	VALUE(NULL);
					/* Client information */
#  endif /* HAVE_DNSSD */
VAR cupsd_srv_t		WebIFSrv	VALUE(NULL);
					/* Service reference for the web interface */
#endif /* HAVE_DNSSD || HAVE_AVAHI */

VAR char		*LPDConfigFile	VALUE(NULL),
					/* LPD configuration file */
			*SMBConfigFile	VALUE(NULL);
					/* SMB configuration file */


/*
 * Prototypes...
 */

extern void	cupsdDeregisterPrinter(cupsd_printer_t *p, int removeit);
extern void	cupsdRegisterPrinter(cupsd_printer_t *p);
extern void	cupsdStartBrowsing(void);
extern void	cupsdStopBrowsing(void);
#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
extern void	cupsdUpdateDNSSDName(void);
#endif /* HAVE_DNSSD || HAVE_AVAHI */


/*
 * End of "$Id: dirsvc.h 7933 2008-09-11 00:44:58Z mike $".
 */
