/*
 * "$Id: conf.h 12689 2015-06-03 19:49:54Z msweet $"
 *
 * Configuration file definitions for the CUPS scheduler.
 *
 * Copyright 2007-2015 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 */


/*
 * Log levels...
 */

typedef enum
{
  CUPSD_LOG_PPD = -5,			/* Used internally for PPD keywords */
  CUPSD_LOG_ATTR,			/* Used internally for attributes */
  CUPSD_LOG_STATE,			/* Used internally for printer-state-reasons */
  CUPSD_LOG_JOBSTATE,			/* Used internally for job-state-reasons */
  CUPSD_LOG_PAGE,			/* Used internally for page logging */
  CUPSD_LOG_NONE,
  CUPSD_LOG_EMERG,			/* Emergency issues */
  CUPSD_LOG_ALERT,			/* Something bad happened that needs attention */
  CUPSD_LOG_CRIT,			/* Critical error but server continues */
  CUPSD_LOG_ERROR,			/* Error condition */
  CUPSD_LOG_WARN,			/* Warning */
  CUPSD_LOG_NOTICE,			/* Normal condition that needs logging */
  CUPSD_LOG_INFO,			/* General information */
  CUPSD_LOG_DEBUG,			/* General debugging */
  CUPSD_LOG_DEBUG2			/* Detailed debugging */
} cupsd_loglevel_t;

typedef enum
{
  CUPSD_ACCESSLOG_NONE,			/* Log no requests */
  CUPSD_ACCESSLOG_CONFIG,		/* Log config requests */
  CUPSD_ACCESSLOG_ACTIONS,		/* Log config, print, and job management requests */
  CUPSD_ACCESSLOG_ALL			/* Log everything */
} cupsd_accesslog_t;

typedef enum
{
  CUPSD_TIME_STANDARD,			/* "Standard" Apache/CLF format */
  CUPSD_TIME_USECS			/* Standard format with microseconds */
} cupsd_time_t;

typedef enum
{
  CUPSD_SANDBOXING_OFF,			/* No sandboxing */
  CUPSD_SANDBOXING_RELAXED,		/* Relaxed sandboxing */
  CUPSD_SANDBOXING_STRICT		/* Strict sandboxing */
} cupsd_sandboxing_t;


/*
 * FatalErrors flags...
 */

#define CUPSD_FATAL_NONE	0	/* No errors are fatal */
#define CUPSD_FATAL_BROWSE	1	/* Browse bind errors are fatal */
#define CUPSD_FATAL_CONFIG	2	/* Config file syntax errors are fatal */
#define CUPSD_FATAL_LISTEN	4	/* Listen/Port bind errors are fatal */
#define CUPSD_FATAL_LOG		8	/* Log file errors are fatal */
#define CUPSD_FATAL_PERMISSIONS	16	/* File permission errors are fatal */
#define CUPSD_FATAL_ALL		~0	/* All errors are fatal */


/*
 * Printcap formats...
 */

#define PRINTCAP_BSD		0	/* Berkeley LPD format */
#define PRINTCAP_SOLARIS	1	/* Solaris lpsched format */
#define PRINTCAP_PLIST		2	/* OS X plist format */


/*
 * ServerAlias data...
 */

typedef struct
{
  size_t	namelen;		/* Length of alias name */
  char		name[1];		/* Alias name */
} cupsd_alias_t;


/*
 * Globals...
 */

VAR char		*ConfigurationFile	VALUE(NULL),
					/* cupsd.conf file to use */
			*CupsFilesFile		VALUE(NULL),
					/* cups-files.conf file to use */
			*ServerName		VALUE(NULL),
					/* FQDN for server */
			*ServerAdmin		VALUE(NULL),
					/* Administrator's email */
			*ServerRoot		VALUE(NULL),
					/* Root directory for scheduler */
			*ServerBin		VALUE(NULL),
					/* Root directory for binaries */
			*StateDir		VALUE(NULL),
					/* Root directory for state data */
			*RequestRoot		VALUE(NULL),
					/* Directory for request files */
			*DocumentRoot		VALUE(NULL);
					/* Root directory for documents */
VAR cups_array_t	*ServerAlias		VALUE(NULL);
					/* Alias names for server */
VAR int			ServerNameIsIP		VALUE(0);
					/* Is the ServerName an IP address? */
VAR int			NumSystemGroups		VALUE(0);
					/* Number of system group names */
VAR char		*SystemGroups[MAX_SYSTEM_GROUPS]
						VALUE({0});
					/* System group names */
VAR gid_t		SystemGroupIDs[MAX_SYSTEM_GROUPS]
						VALUE({0});
					/* System group IDs */
VAR char		*AccessLog		VALUE(NULL),
					/* Access log filename */
			*ErrorLog		VALUE(NULL),
					/* Error log filename */
			*PageLog		VALUE(NULL),
					/* Page log filename */
			*CacheDir		VALUE(NULL),
					/* Cache file directory */
			*DataDir		VALUE(NULL),
					/* Data file directory */
			*DefaultLanguage	VALUE(NULL),
					/* Default language encoding */
			*DefaultLocale		VALUE(NULL),
					/* Default locale */
			*DefaultPaperSize	VALUE(NULL),
					/* Default paper size */
			*ErrorPolicy		VALUE(NULL),
					/* Default printer-error-policy */
			*RIPCache		VALUE(NULL),
					/* Amount of memory for RIPs */
			*TempDir		VALUE(NULL),
					/* Temporary directory */
			*Printcap		VALUE(NULL),
					/* Printcap file */
			*FontPath		VALUE(NULL),
					/* Font search path */
			*RemoteRoot		VALUE(NULL),
					/* Remote root user */
			*Classification		VALUE(NULL);
					/* Classification of system */
VAR uid_t		User			VALUE(1),
					/* User ID for server */
			RunUser			VALUE(0);
					/* User to run as, used for files */
VAR gid_t		Group			VALUE(0);
					/* Group ID for server */
VAR cupsd_accesslog_t	AccessLogLevel		VALUE(CUPSD_ACCESSLOG_ACTIONS);
					/* Access log level */
VAR int			ClassifyOverride	VALUE(0),
					/* Allow overrides? */
			LogDebugHistory		VALUE(200),
					/* Amount of automatic debug history */
			FatalErrors		VALUE(CUPSD_FATAL_CONFIG),
					/* Which errors are fatal? */
			StrictConformance	VALUE(FALSE),
					/* Require strict IPP conformance? */
			SyncOnClose		VALUE(FALSE);
					/* Call fsync() when closing files? */
VAR mode_t		ConfigFilePerm		VALUE(0640U),
					/* Permissions for config files */
			LogFilePerm		VALUE(0644U);
					/* Permissions for log files */
VAR cupsd_loglevel_t	LogLevel		VALUE(CUPSD_LOG_WARN);
					/* Error log level */
VAR cupsd_time_t	LogTimeFormat		VALUE(CUPSD_TIME_STANDARD);
					/* Log file time format */
VAR cups_file_t		*LogStderr		VALUE(NULL);
					/* Stderr file, if any */
VAR cupsd_sandboxing_t	Sandboxing		VALUE(CUPSD_SANDBOXING_STRICT);
					/* Sandboxing level */
VAR int			UseSandboxing	VALUE(1);
					/* Use sandboxing for child procs? */
VAR int			MaxClients		VALUE(100),
					/* Maximum number of clients */
			MaxClientsPerHost	VALUE(0),
					/* Maximum number of clients per host */
			MaxCopies		VALUE(CUPS_DEFAULT_MAX_COPIES),
					/* Maximum number of copies per job */
			MaxLogSize		VALUE(1024 * 1024),
					/* Maximum size of log files */
			MaxRequestSize		VALUE(0),
					/* Maximum size of IPP requests */
			HostNameLookups		VALUE(FALSE),
					/* Do we do reverse lookups? */
			Timeout			VALUE(DEFAULT_TIMEOUT),
					/* Timeout during requests */
			KeepAlive		VALUE(TRUE),
					/* Support the Keep-Alive option? */
			KeepAliveTimeout	VALUE(DEFAULT_KEEPALIVE),
					/* Timeout between requests */
			FileDevice		VALUE(FALSE),
					/* Allow file: devices? */
			FilterLimit		VALUE(0),
					/* Max filter cost at any time */
			FilterLevel		VALUE(0),
					/* Current filter level */
			FilterNice		VALUE(0),
					/* Nice value for filters */
			ReloadTimeout		VALUE(DEFAULT_KEEPALIVE),
					/* Timeout before reload from SIGHUP */
			RootCertDuration	VALUE(300),
					/* Root certificate update interval */
			PrintcapFormat		VALUE(PRINTCAP_BSD),
					/* Format of printcap file? */
			DefaultShared		VALUE(TRUE),
					/* Share printers by default? */
			MultipleOperationTimeout VALUE(DEFAULT_TIMEOUT),
					/* multiple-operation-time-out value */
			WebInterface		VALUE(CUPS_DEFAULT_WEBIF);
					/* Enable the web interface? */
VAR cups_file_t		*AccessFile		VALUE(NULL),
					/* Access log file */
			*ErrorFile		VALUE(NULL),
					/* Error log file */
			*PageFile		VALUE(NULL);
					/* Page log file */
VAR char		*PageLogFormat		VALUE(NULL);
					/* Page log format */
VAR mime_t		*MimeDatabase		VALUE(NULL);
					/* MIME type database */
VAR int			NumMimeTypes		VALUE(0);
					/* Number of MIME types */
VAR const char		**MimeTypes		VALUE(NULL);
					/* Array of MIME types */

#ifdef HAVE_SSL
VAR char		*ServerKeychain		VALUE(NULL);
					/* Keychain holding cert + key */
#endif /* HAVE_SSL */

#if defined(HAVE_LAUNCHD) || defined(HAVE_SYSTEMD)
VAR int			IdleExitTimeout		VALUE(60);
					/* Time after which an idle cupsd will exit */
#endif /* HAVE_LAUNCHD || HAVE_SYSTEMD */

#ifdef HAVE_AUTHORIZATION_H
VAR char		*SystemGroupAuthKey	VALUE(NULL);
					/* System group auth key */
#endif /* HAVE_AUTHORIZATION_H */

#ifdef HAVE_GSSAPI
VAR char		*GSSServiceName		VALUE(NULL);
					/* GSS service name */
int			HaveServerCreds		VALUE(0);
					/* Do we have server credentials? */
gss_cred_id_t		ServerCreds;	/* Server's GSS credentials */
#endif /* HAVE_GSSAPI */


/*
 * Prototypes...
 */

extern void	cupsdAddAlias(cups_array_t *aliases, const char *name);
extern int	cupsdCheckLogFile(cups_file_t **lf, const char *logname);
extern int	cupsdCheckPermissions(const char *filename,
		                      const char *suffix, mode_t mode,
	 			      uid_t user, gid_t group, int is_dir,
				      int create_dir);
extern int	cupsdCheckProgram(const char *filename, cupsd_printer_t *p);
extern int	cupsdDefaultAuthType(void);
extern void	cupsdFreeAliases(cups_array_t *aliases);
extern char	*cupsdGetDateTime(struct timeval *t, cupsd_time_t format);
extern int	cupsdLogClient(cupsd_client_t *con, int level,
                               const char *message, ...)
                               __attribute__((__format__(__printf__, 3, 4)));
extern void	cupsdLogFCMessage(void *context, _cups_fc_result_t result,
		                  const char *message);
#ifdef HAVE_GSSAPI
extern int	cupsdLogGSSMessage(int level, OM_uint32 major_status,
		                   OM_uint32 minor_status,
		                   const char *message, ...);
#endif /* HAVE_GSSAPI */
extern int	cupsdLogJob(cupsd_job_t *job, int level, const char *message,
		            ...) __attribute__((__format__(__printf__, 3, 4)));
extern int	cupsdLogMessage(int level, const char *message, ...)
		__attribute__ ((__format__ (__printf__, 2, 3)));
extern int	cupsdLogPage(cupsd_job_t *job, const char *page);
extern int	cupsdLogRequest(cupsd_client_t *con, http_status_t code);
extern int	cupsdReadConfiguration(void);
extern int	cupsdWriteErrorLog(int level, const char *message);


/*
 * End of "$Id: conf.h 12689 2015-06-03 19:49:54Z msweet $".
 */
