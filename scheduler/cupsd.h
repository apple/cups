/*
 * "$Id$"
 *
 *   Main header file for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */


/*
 * Include necessary headers.
 */

#include <cups/http-private.h>
#include <cups/string.h>
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

#include <cups/array.h>
#include <cups/cups.h>
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
#  define VALUE2(x,y) ={x,y}
#else
#  define VAR      extern
#  define VALUE(x)
#  define VALUE2(x,y)
#endif /* _MAIN_C */


/*
 * Other stuff for the scheduler...
 */

#include "statbuf.h"
#include "cert.h"
#include "auth.h"
#include "client.h"
#include "policy.h"
#include "printers.h"
#include "classes.h"
#include "job.h"
#include "conf.h"
#include "banners.h"
#include "dirsvc.h"
#include "network.h"
#include "subscriptions.h"


/*
 * Reload types...
 */

#define RELOAD_NONE	0		/* No reload needed */
#define RELOAD_ALL	1		/* Reload everything */
#define RELOAD_CUPSD	2		/* Reload only cupsd.conf */


/*
 * Globals...
 */

VAR int			MaxFDs,		/* Maximum number of files */
			SetSize;	/* The size of the input/output sets */
VAR fd_set		*InputSet,	/* Input files for select() */
			*OutputSet;	/* Output files for select() */

VAR time_t		ReloadTime	VALUE(0);
					/* Time of reload request... */
VAR int			NeedReload	VALUE(RELOAD_ALL);
					/* Need to load configuration? */


/*
 * Prototypes...
 */

extern void	CatchChildSignals(void);
extern void	ClearString(char **s);
extern void	HoldSignals(void);
extern void	IgnoreChildSignals(void);
extern void	ReleaseSignals(void);
extern void	SetString(char **s, const char *v);
extern void	SetStringf(char **s, const char *f, ...)
#ifdef __GNUC__
__attribute__ ((__format__ (__printf__, 2, 3)))
#endif /* __GNUC__ */
;
extern void	StartServer(void);
extern void	StopServer(void);
extern void	cupsdClosePipe(int *fds);
extern int	cupsdOpenPipe(int *fds);

extern void	cupsdClearEnv(void);
extern void	cupsdInitEnv(void);
extern int	cupsdLoadEnv(char *envp[], int envmax);
extern void	cupsdSetEnv(const char *name, const char *value);
extern void	cupsdSetEnvf(const char *name, const char *value, ...)
#ifdef __GNUC__
__attribute__ ((__format__ (__printf__, 2, 3)))
#endif /* __GNUC__ */
;

extern int	cupsdEndProcess(int pid, int force);
extern int	cupsdStartProcess(const char *command, char *argv[],
				  char *envp[], int infd, int outfd,
				  int errfd, int backfd, int root, int *pid);


/*
 * End of "$Id$".
 */
