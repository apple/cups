/*
 * "$Id: dirsvc.h,v 1.8 2000/01/04 13:46:09 mike Exp $"
 *
 *   Directory services definitions for the Common UNIX Printing System
 *   (CUPS) scheduler.
 *
 *   Copyright 1997-2000 by Easy Software Products, all rights reserved.
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
 * Globals...
 */

VAR int			Browsing	VALUE(TRUE),
					/* Whether or not browsing is enabled */
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


/*
 * Prototypes...
 */

extern void	StartBrowsing(void);
extern void	StopBrowsing(void);
extern void	UpdateBrowseList(void);
extern void	SendBrowseList(void);


/*
 * End of "$Id: dirsvc.h,v 1.8 2000/01/04 13:46:09 mike Exp $".
 */
