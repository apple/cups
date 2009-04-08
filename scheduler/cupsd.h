/*
 * "$Id: cupsd.h 7928 2008-09-10 22:14:22Z mike $"
 *
 *   Main header file for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 2007-2009 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   "LICENSE" which should have been included with this file.  If this
 *   file is missing or damaged, see the license at "http://www.cups.org/".
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
#include <cups/i18n.h>
#include <cups/debug.h>

#if defined(HAVE_CDSASSL)
#  include <CoreFoundation/CoreFoundation.h>
#endif /* HAVE_CDSASSL */


/*
 * Some OS's don't have hstrerror(), most notably Solaris...
 */

#ifndef HAVE_HSTRERROR
#  ifdef hstrerror
#    undef hstrerror
#  endif /* hstrerror */
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

#define MAX_ENV			100	/* Maximum number of environment strings */
#define MAX_USERPASS		33	/* Maximum size of username/password */
#define MAX_FILTERS		20	/* Maximum number of filters */
#define MAX_SYSTEM_GROUPS	32	/* Maximum number of system groups */


/*
 * Defaults...
 */

#define DEFAULT_HISTORY		1	/* Preserve job history? */
#define DEFAULT_FILES		0	/* Preserve job files? */
#define DEFAULT_TIMEOUT		300	/* Timeout during requests/updates */
#define DEFAULT_KEEPALIVE	30	/* Timeout between requests */
#define DEFAULT_INTERVAL	30	/* Interval between browse updates */
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

#include "sysman.h"
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
 * Select callback function type...
 */

typedef void (*cupsd_selfunc_t)(void *data);


/*
 * Globals...
 */

VAR int			TestConfigFile	VALUE(0),
					/* Test the cupsd.conf file? */
			UseProfiles	VALUE(1);
					/* Use security profiles for child procs? */
VAR int			MaxFDs		VALUE(0);
					/* Maximum number of files */

VAR time_t		ReloadTime	VALUE(0);
					/* Time of reload request... */
VAR int			NeedReload	VALUE(RELOAD_ALL);
					/* Need to load configuration? */
VAR void		*DefaultProfile	VALUE(0);
					/* Default security profile */

#ifdef HAVE_GSSAPI
VAR int			KerberosInitialized	VALUE(0);
					/* Has Kerberos been initialized? */
VAR krb5_context	KerberosContext VALUE(NULL);
					/* Kerberos context for credentials */
#endif /* HAVE_GSSAPI */

#ifdef HAVE_LAUNCH_H
VAR int			Launchd		VALUE(0);
					/* Running from launchd */
#endif /* HAVE_LAUNCH_H */

#if defined(__APPLE__) && defined(HAVE_DLFCN_H)
typedef int (*PSQUpdateQuotaProcPtr)(const char *printer, const char *info, 
                                     const char *user, int nPages, int options);
VAR PSQUpdateQuotaProcPtr PSQUpdateQuotaProc
					VALUE(0);
					/* Apple PrintService quota function */
#endif /* __APPLE__ && HAVE_DLFCN_H */




/*
 * Prototypes...
 */

extern void	cupsdCheckProcess(void);
extern void	cupsdClearString(char **s);
extern void	cupsdHoldSignals(void);
extern void	cupsdReleaseSignals(void);
extern void	cupsdSetString(char **s, const char *v);
extern void	cupsdSetStringf(char **s, const char *f, ...)
#ifdef __GNUC__
__attribute__ ((__format__ (__printf__, 2, 3)))
#endif /* __GNUC__ */
;
extern void	cupsdStartServer(void);
extern void	cupsdStopServer(void);
extern void	cupsdClosePipe(int *fds);
extern int	cupsdOpenPipe(int *fds);

extern void	cupsdInitEnv(void);
extern int	cupsdLoadEnv(char *envp[], int envmax);
extern void	cupsdSetEnv(const char *name, const char *value);
extern void	cupsdSetEnvf(const char *name, const char *value, ...)
#ifdef __GNUC__
__attribute__ ((__format__ (__printf__, 2, 3)))
#endif /* __GNUC__ */
;

extern void	*cupsdCreateProfile(int job_id);
extern void	cupsdDestroyProfile(void *profile);
extern int	cupsdEndProcess(int pid, int force);
extern const char *cupsdFinishProcess(int pid, char *name, int namelen,
		                      int *job_id);
extern int	cupsdStartProcess(const char *command, char *argv[],
				  char *envp[], int infd, int outfd,
				  int errfd, int backfd, int sidefd,
				  int root, void *profile, cupsd_job_t *job,
				  int *pid);

extern int	cupsdAddSelect(int fd, cupsd_selfunc_t read_cb,
		               cupsd_selfunc_t write_cb, void *data);
extern int	cupsdDoSelect(long timeout);
#ifdef CUPSD_IS_SELECTING
extern int	cupsdIsSelecting(int fd);
#endif /* CUPSD_IS_SELECTING */
extern void	cupsdRemoveSelect(int fd);
extern void	cupsdStartSelect(void);
extern void	cupsdStopSelect(void);

extern int	cupsdRemoveFile(const char *filename);


/*
 * End of "$Id: cupsd.h 7928 2008-09-10 22:14:22Z mike $".
 */
