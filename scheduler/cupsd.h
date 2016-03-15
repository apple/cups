/*
 * "$Id: cupsd.h 11717 2014-03-21 16:42:53Z msweet $"
 *
 * Main header file for the CUPS scheduler.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * "LICENSE" which should have been included with this file.  If this
 * file is missing or damaged, see the license at "http://www.cups.org/".
 */


/*
 * Include necessary headers.
 */

#include <cups/cups-private.h>
#include <cups/file-private.h>

#include <limits.h>
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

#include "mime.h"

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

#define DEFAULT_HISTORY		INT_MAX	/* Preserve job history? */
#define DEFAULT_FILES		86400	/* Preserve job files? */
#define DEFAULT_TIMEOUT		300	/* Timeout during requests/updates */
#define DEFAULT_KEEPALIVE	30	/* Timeout between requests */


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
#include "colorman.h"
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

VAR int			TestConfigFile	VALUE(0);
					/* Test the cupsd.conf file? */
VAR int			MaxFDs		VALUE(0);
					/* Maximum number of files */

VAR time_t		ReloadTime	VALUE(0);
					/* Time of reload request... */
VAR int			NeedReload	VALUE(RELOAD_ALL),
					/* Need to load configuration? */
			DoingShutdown	VALUE(0);
					/* Shutting down the scheduler? */
VAR void		*DefaultProfile	VALUE(0);
					/* Default security profile */

#if defined(HAVE_LAUNCHD) || defined(HAVE_SYSTEMD)
VAR int			OnDemand	VALUE(0);
					/* Launched on demand */
#endif /* HAVE_LAUNCHD || HAVE_SYSTEMD */


/*
 * Prototypes...
 */

/* env.c */
extern void		cupsdInitEnv(void);
extern int		cupsdLoadEnv(char *envp[], int envmax);
extern void		cupsdSetEnv(const char *name, const char *value);
extern void		cupsdSetEnvf(const char *name, const char *value, ...)
			__attribute__ ((__format__ (__printf__, 2, 3)));
extern void		cupsdUpdateEnv(void);

/* file.c */
extern void		cupsdCleanFiles(const char *path, const char *pattern);
extern int		cupsdCloseCreatedConfFile(cups_file_t *fp,
			                          const char *filename);
extern void		cupsdClosePipe(int *fds);
extern cups_file_t	*cupsdCreateConfFile(const char *filename, mode_t mode);
extern cups_file_t	*cupsdOpenConfFile(const char *filename);
extern int		cupsdOpenPipe(int *fds);
extern int		cupsdRemoveFile(const char *filename);
extern int		cupsdUnlinkOrRemoveFile(const char *filename);

/* main.c */
extern int		cupsdAddString(cups_array_t **a, const char *s);
extern void		cupsdCheckProcess(void);
extern void		cupsdClearString(char **s);
extern void		cupsdFreeStrings(cups_array_t **a);
extern void		cupsdHoldSignals(void);
extern char		*cupsdMakeUUID(const char *name, int number,
				       char *buffer, size_t bufsize);
extern void		cupsdReleaseSignals(void);
extern void		cupsdSetString(char **s, const char *v);
extern void		cupsdSetStringf(char **s, const char *f, ...)
			__attribute__ ((__format__ (__printf__, 2, 3)));

/* process.c */
extern void		*cupsdCreateProfile(int job_id, int allow_networking);
extern void		cupsdDestroyProfile(void *profile);
extern int		cupsdEndProcess(int pid, int force);
extern const char	*cupsdFinishProcess(int pid, char *name, size_t namelen, int *job_id);
extern int		cupsdStartProcess(const char *command, char *argv[],
					  char *envp[], int infd, int outfd,
					  int errfd, int backfd, int sidefd,
					  int root, void *profile,
					  cupsd_job_t *job, int *pid);

/* select.c */
extern int		cupsdAddSelect(int fd, cupsd_selfunc_t read_cb,
			               cupsd_selfunc_t write_cb, void *data);
extern int		cupsdDoSelect(long timeout);
#ifdef CUPSD_IS_SELECTING
extern int		cupsdIsSelecting(int fd);
#endif /* CUPSD_IS_SELECTING */
extern void		cupsdRemoveSelect(int fd);
extern void		cupsdStartSelect(void);
extern void		cupsdStopSelect(void);

/* server.c */
extern void		cupsdStartServer(void);
extern void		cupsdStopServer(void);


/*
 * End of "$Id: cupsd.h 11717 2014-03-21 16:42:53Z msweet $".
 */
