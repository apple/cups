/*
 * "$Id: dirsvc.h,v 1.2 1998/10/16 18:28:01 mike Exp $"
 *
 *   Directory services definitions for the Common UNIX Printing System
 *   (CUPS) scheduler.
 *
 *   Copyright 1997-1998 by Easy Software Products, all rights reserved.
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

/*
 * Browsing structure...
 */

typedef struct
{
  char	name[256];			/* Name of printer or class */
  int	type_code,			/* Printer type code */
	status_code;			/* Printer status code */
} browse_t;


/*
 * Globals...
 */

VAR int			Browsing	VALUE(TRUE),
					/* Whether or not browsing is enabled */
			BrowsePort	VALUE(DEFAULT_PORT),
					/* Port number for broadcasts */
			BrowseSocket	VALUE(0),
					/* Socket for broadcast */
			BrowseInterval	VALUE(DEFAULT_INTERVAL),
					/* Broadcast interval in seconds */
			BrowseTimeout	VALUE(DEFAULT_TIMEOUT),
					/* Time out for printers in seconds */
			NumBrowsers	VALUE(0);
					/* Number of broadcast addresses */
VAR struct sockaddr_in	Browsers[MAX_BROWSERS];
					/* Broadcast addresses */
VAR time_t		LastBrowseTime	VALUE(0);
					/* Time of last broadcast */


/*
 * Prototypes...
 */

extern void	StartBrowsing(void);
extern void	StopBrowsing(void);
extern void	UpdateBrowseList(void);
extern void	SendBrowseList(void);


/*
 * End of "$Id: dirsvc.h,v 1.2 1998/10/16 18:28:01 mike Exp $".
 */
