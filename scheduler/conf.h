/*
 * "$Id$"
 *
 *   Configuration file definitions for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */


/*
 * Log levels...
 */

typedef enum
{
  CUPSD_LOG_PPD = -4,			/* Used internally for PPD keywords */
  CUPSD_LOG_ATTR,			/* Used internally for attributes */
  CUPSD_LOG_STATE,			/* Used internally for state-reasons */
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


/*
 * Printcap formats...
 */

#define PRINTCAP_BSD		0	/* Berkeley LPD format */
#define PRINTCAP_SOLARIS	1	/* Solaris lpsched format */


/*
 * Globals...
 */

VAR char		*ConfigurationFile	VALUE(NULL),
					/* Configuration file to use */
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
VAR int			ServerNameIsIP		VALUE(0);
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
			*DefaultCharset		VALUE(NULL),
					/* Default charset */
			*DefaultLocale		VALUE(NULL),
					/* Default locale */
			*ErrorPolicy		VALUE(NULL),
					/* Default printer-error-policy */
			*RIPCache		VALUE(NULL),
					/* Amount of memory for RIPs */
			*TempDir		VALUE(NULL),
					/* Temporary directory */
			*Printcap		VALUE(NULL),
					/* Printcap file */
			*PrintcapGUI		VALUE(NULL),
					/* GUI program to use for IRIX */
			*FontPath		VALUE(NULL),
					/* Font search path */
			*RemoteRoot		VALUE(NULL),
					/* Remote root user */
			*Classification		VALUE(NULL);
					/* Classification of system */
#ifdef HAVE_GSSAPI
VAR char		*GSSServiceName		VALUE(NULL);
					/* GSS service name */
VAR char		*Krb5Keytab		VALUE(NULL);
					/* Kerberos Keytab */
#endif /* HAVE_GSSAPI */
VAR uid_t		User			VALUE(1);
					/* User ID for server */
VAR gid_t		Group			VALUE(0);
					/* Group ID for server */
VAR int			ClassifyOverride	VALUE(0),
					/* Allow overrides? */
			ConfigFilePerm		VALUE(0640),
					/* Permissions for config files */
			LogFilePerm		VALUE(0644),
					/* Permissions for log files */
			LogLevel		VALUE(CUPSD_LOG_ERROR),
					/* Log level */
			MaxClients		VALUE(0),
					/* Maximum number of clients */
			MaxClientsPerHost	VALUE(0),
					/* Maximum number of clients per host */
			MaxCopies		VALUE(CUPS_DEFAULT_MAX_COPIES),
					/* Maximum number of copies per job */
			MaxLogSize		VALUE(1024 * 1024),
					/* Maximum size of log files */
			MaxPrinterHistory	VALUE(10),
					/* Maximum printer state history */
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
			ImplicitClasses		VALUE(TRUE),
					/* Are classes implicitly created? */
			ImplicitAnyClasses	VALUE(FALSE),
					/* Create AnyPrinter classes? */
			HideImplicitMembers	VALUE(TRUE),
					/* Hide implicit class members? */
			FileDevice		VALUE(FALSE),
					/* Allow file: devices? */
			FilterLimit		VALUE(0),
					/* Max filter cost at any time */
			FilterLevel		VALUE(0),
					/* Current filter level */
			FilterNice		VALUE(0),
					/* Nice value for filters */
			ReloadTimeout		VALUE(0),
					/* Timeout before reload from SIGHUP */
			RootCertDuration	VALUE(300),
					/* Root certificate update interval */
			RunUser			VALUE(0),
					/* User to run as, used for files */
			PrintcapFormat		VALUE(PRINTCAP_BSD),
					/* Format of printcap file? */
			DefaultShared		VALUE(TRUE);
					/* Share printers by default? */
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
VAR char		*ServerCertificate	VALUE(NULL);
					/* Server certificate file */
#  if defined(HAVE_LIBSSL) || defined(HAVE_GNUTLS)
VAR char		*ServerKey		VALUE(NULL);
					/* Server key file */
#  endif /* HAVE_LIBSSL || HAVE_GNUTLS */
#endif /* HAVE_SSL */

#ifdef HAVE_LAUNCHD
VAR int			LaunchdTimeout		VALUE(DEFAULT_TIMEOUT);
					/* Time after which an idle cupsd will exit */
VAR char		*LaunchdConf		VALUE(NULL);
					/* launchd(8) configuration file */
#endif /* HAVE_LAUNCHD */

#ifdef __APPLE__
VAR int			AppleQuotas		VALUE(TRUE);
					/* Use Apple PrintService Quotas instead of CUPS quotas */
#endif  /* __APPLE__ */

#ifdef HAVE_AUTHORIZATION_H
VAR char		*SystemGroupAuthKey	VALUE(NULL);
					/* System group auth key */
#endif /* HAVE_AUTHORIZATION_H */


/*
 * Prototypes...
 */

extern int	cupsdCheckPermissions(const char *filename,
		                      const char *suffix, int mode,
	 			      int user, int group, int is_dir,
				      int create_dir);
extern char	*cupsdGetDateTime(time_t t);
#ifdef HAVE_GSSAPI
extern int	cupsdLogGSSMessage(int level, int major_status,
		                   int minor_status,
		                   const char *message, ...);
#endif /* HAVE_GSSAPI */
extern int	cupsdLogMessage(int level, const char *message, ...)
#ifdef __GNUC__
__attribute__ ((__format__ (__printf__, 2, 3)))
#endif /* __GNUC__ */
;
extern int	cupsdLogPage(cupsd_job_t *job, const char *page);
extern int	cupsdLogRequest(cupsd_client_t *con, http_status_t code);
extern int	cupsdReadConfiguration(void);


/*
 * End of "$Id$".
 */
