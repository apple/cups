/*
 * "$Id: cupsd.h,v 1.20 2000/02/01 21:00:24 mike Exp $"
 *
 *   Main header file for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-2000 by Easy Software Products, all rights reserved.
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
 * Include necessary headers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>

#if defined(WIN32) || defined(__EMX__)
#  include <direct.h>
#else
#  include <unistd.h>
#endif /* WIN32 || __EMX__ */

#include <cups/cups.h>
#include <cups/string.h>
#include "mime.h"
#include <cups/http.h>
#include <cups/ipp.h>
#include <cups/language.h>
#include <cups/debug.h>


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

#define MAX_BROWSERS		10	/* Maximum number of browse addresses */
#define MAX_LISTENERS		10	/* Maximum number of listener sockets */
#define MAX_USERPASS		16	/* Maximum size of username/password */
#define MAX_FILTERS		20	/* Maximum number of filters */


/*
 * Defaults...
 */

#define DEFAULT_HISTORY		1	/* Preserve job history? */
#define DEFAULT_FILES		0	/* Preserve job files? */
#define DEFAULT_TIMEOUT		300	/* Timeout during requests/updates */
#define DEFAULT_KEEPALIVE	60	/* Timeout between requests */
#define DEFAULT_INTERVAL	30	/* Interval between browse updates */
#ifdef WIN32 /* Fix for broken Linux setlocale() */
#  define DEFAULT_LANGUAGE	setlocale(LC_ALL,"")
					/* Default language encoding */
#else
#  define DEFAULT_LANGUAGE	getenv("LANG")
					/* Default language encoding */
#endif /* !WIN32 */
#define DEFAULT_CHARSET		"iso-8859-1"
					/* Default charset */

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

#include "cert.h"
#include "client.h"
#include "auth.h"
#include "dirsvc.h"
#include "printers.h"
#include "classes.h"
#include "job.h"
#include "conf.h"


/*
 * Globals...
 */

VAR fd_set		InputSet,	/* Input files for select() */
			OutputSet;	/* Output files for select() */

VAR time_t		StartTime;	/* Time server was started */
VAR int			NeedReload	VALUE(TRUE);
					/* Need to load configuration? */
VAR char		TZ[1024]	VALUE("TZ=GMT");
					/* Timezone configuration */

VAR ipp_t		*Devices	VALUE(NULL),
					/* Available devices */
			*PPDs		VALUE(NULL);
					/* Available PPDs */


/*
 * Prototypes...
 */

extern void	LoadDevices(const char *d);
extern void	LoadPPDs(const char *d);

/*
 * End of "$Id: cupsd.h,v 1.20 2000/02/01 21:00:24 mike Exp $".
 */
