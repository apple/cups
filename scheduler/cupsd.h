/*
 * "$Id: cupsd.h,v 1.28.2.13 2003/01/29 20:08:21 mike Exp $"
 *
 *   Main header file for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-2003 by Easy Software Products, all rights reserved.
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
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>

#ifdef WIN32
#  include <direct.h>
#else
#  include <unistd.h>
#endif /* WIN32 */

#include <cups/cups.h>
#include <cups/string.h>
#include "mime.h"
#include <cups/http.h>
#include <cups/ipp.h>
#include <cups/language.h>
#include <cups/debug.h>

#if defined(HAVE_CDSASSL)
#  include <CoreFoundation/CoreFoundation.h>
#endif /* HAVE_CDSASSL */


/*
 * Some OS's don't have hstrerror(), most notably Solaris...
 */

#ifndef HAVE_HSTRERROR
#  define hstrerror cups_hstrerror

extern const char *cups_hstrerror(int);
#endif /* !HAVE_HSTRERROR */


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
#define MAX_USERPASS		33	/* Maximum size of username/password */
#define MAX_FILTERS		20	/* Maximum number of filters */
#define MAX_SYSTEM_GROUPS	32	/* Maximum number of system groups */


/*
 * Defaults...
 */

#define DEFAULT_HISTORY		1	/* Preserve job history? */
#define DEFAULT_FILES		0	/* Preserve job files? */
#define DEFAULT_TIMEOUT		300	/* Timeout during requests/updates */
#define DEFAULT_KEEPALIVE	60	/* Timeout between requests */
#define DEFAULT_INTERVAL	30	/* Interval between browse updates */
#define DEFAULT_LANGUAGE	setlocale(LC_ALL,"")
					/* Default language encoding */
#define DEFAULT_CHARSET		"utf-8"	/* Default charset */


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
#include "policy.h"
#include "printers.h"
#include "classes.h"
#include "job.h"
#include "conf.h"
#include "banners.h"
#include "dirsvc.h"
#include "network.h"


/*
 * Directory handling functions...
 */

#if HAVE_DIRENT_H
#  include <dirent.h>
typedef struct dirent DIRENT;
#  define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#  if HAVE_SYS_NDIR_H
#    include <sys/ndir.h>
#  endif
#  if HAVE_SYS_DIR_H
#    include <sys/dir.h>
#  endif
#  if HAVE_NDIR_H
#    include <ndir.h>
#  endif
typedef struct direct DIRENT;
#  define NAMLEN(dirent) (dirent)->d_namlen
#endif


/*
 * Globals...
 */

VAR int			MaxFDs;		/* Maximum number of files */
VAR fd_set		InputSet,	/* Input files for select() */
			OutputSet;	/* Output files for select() */

VAR int			NeedReload	VALUE(TRUE);
					/* Need to load configuration? */
VAR char		*TZ		VALUE(NULL);
					/* Timezone configuration */

VAR ipp_t		*Devices	VALUE(NULL),
					/* Available devices */
			*PPDs		VALUE(NULL);
					/* Available PPDs */


/*
 * Prototypes...
 */

extern void	CatchChildSignals(void);
extern void	ClearString(char **s);
extern void	IgnoreChildSignals(void);
extern void	LoadDevices(const char *d);
extern void	LoadPPDs(const char *d);
extern void	SetString(char **s, const char *v);
extern void	SetStringf(char **s, const char *f, ...);
extern void	StartServer(void);
extern void	StopServer(void);


/*
 * End of "$Id: cupsd.h,v 1.28.2.13 2003/01/29 20:08:21 mike Exp $".
 */
