/*
 * "$Id: cupsd.h,v 1.3 1999/01/24 14:25:11 mike Exp $"
 *
 *   Main header file for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

/*
 * Include necessary headers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <bstring.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <mime.h>

#ifdef WIN32
#  include <windows.h>
#  include <winsock.h>
#else
#  include <sys/socket.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <netinet/in_systm.h>
#  include <netinet/ip.h>
#  include <netinet/tcp.h>
#endif /* WIN32 */


/*
 * Common constants.
 */

#ifndef FALSE
#  define FALSE		0
#  define TRUE		(!FALSE)
#endif /* !FALSE */


/*
 * Implementation limits...
 */

#define MAX_NAME		128	/* Max length of printer or class name */
#define MAX_HOST		256	/* Max length of host name */
#define MAX_URI			1024	/* Max length of URI */
#define MAX_STATUS		256	/* Max length of status text */
#define MAX_BROWSERS		10	/* Maximum number of browse addresses */
#define MAX_LISTENERS		10	/* Maximum number of listener sockets */
#define MAX_CLIENTS		100	/* Maximum number of client sockets */
#define MAX_BUFFER		8192	/* Maximum size of network buffer */


/*
 * Defaults...
 */

#define DEFAULT_PORT		631	/* IPP port reserved with the IANA */
#define DEFAULT_TIMEOUT		300	/* Timeout during requests/updates */
#define DEFAULT_KEEPALIVE	30	/* Timeout between requests */
#define DEFAULT_INTERVAL	30	/* Interval between browse updates */
#define DEFAULT_LANGUAGE	"en"	/* Default language encoding */
#define DEFAULT_CHARSET		"iso-8859-1"	/* Default charset */
#define DEFAULT_GROUP		"sys"	/* Default system group */
#define DEFAULT_UID		9	/* Default user ID */
#define DEFAULT_GID		0	/* Default group ID */


/*
 * Global variable macros...
 */

#ifdef _MAIN_C_
#  define VAR
#  define VALUE(x) =x
#else
#  define VAR      extern
#  define VALUE(x)
#endif /* _MAIN_C */


/*
 * Other stuff for the scheduler...
 */

#include "http.h"
#include "ipp.h"
#include "auth.h"
#include "conf.h"
#include "dirsvc.h"
#include "printers.h"
#include "classes.h"
#include "job.h"


/*
 * Globals...
 */

VAR fd_set		InputSet,	/* Input files for select() */
			OutputSet;	/* Output files for select() */

VAR int			NeedReload	VALUE(TRUE);
					/* Need to load configuration */


/*
 * Prototypes...
 */


/*
 * End of "$Id: cupsd.h,v 1.3 1999/01/24 14:25:11 mike Exp $".
 */
