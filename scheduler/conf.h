/*
 * "$Id: conf.h,v 1.36.2.19 2003/08/22 22:02:35 mike Exp $"
 *
 *   Configuration file definitions for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 1997-2003 by Easy Software Products, all rights reserved.
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
 * Log levels...
 */

#define L_STATE		-2	/* Used internally for state-reasons */
#define L_PAGE		-1	/* Used internally for page logging */
#define L_NONE		0
#define L_EMERG		1	/* Emergency issues */
#define L_ALERT		2	/* Something bad happened that needs attention */
#define L_CRIT		3	/* Critical error but server continues */
#define L_ERROR		4	/* Error condition */
#define L_WARN		5	/* Warning */
#define L_NOTICE	6	/* Normal condition that needs logging */
#define L_INFO		7	/* General information */
#define L_DEBUG		8	/* General debugging */
#define L_DEBUG2	9	/* Detailed debugging */


/*
 * Printcap formats...
 */

#define PRINTCAP_BSD	0	/* Berkeley LPD format */
#define PRINTCAP_SOLARIS 1	/* Solaris lpsched format */


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
			*RequestRoot		VALUE(NULL),
					/* Directory for request files */
			*DocumentRoot		VALUE(NULL);
					/* Root directory for documents */
VAR int			NumSystemGroups		VALUE(0);
					/* Number of system group names */
VAR char		*SystemGroups[MAX_SYSTEM_GROUPS],
					/* System group names */
			*AccessLog		VALUE(NULL),
					/* Access log filename */
			*ErrorLog		VALUE(NULL),
					/* Error log filename */
			*PageLog		VALUE(NULL),
					/* Page log filename */
			*DataDir		VALUE(NULL),
					/* Data file directory */
			*DefaultLanguage	VALUE(NULL),
					/* Default language encoding */
			*DefaultCharset		VALUE(NULL),
					/* Default charset */
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
VAR uid_t		User			VALUE(1);
					/* User ID for server */
VAR gid_t		Group			VALUE(0);
					/* Group ID for server */
VAR time_t		TimeZoneOffset		VALUE(0);
					/* Offset from UTC to local time */
VAR int			ClassifyOverride	VALUE(0),
					/* Allow overrides? */
			ConfigFilePerm		VALUE(0600),
					/* Permissions for config files */
			LogFilePerm		VALUE(0644),
					/* Permissions for log files */
			LogLevel		VALUE(L_ERROR),
					/* Log level */
			MaxClients		VALUE(0),
					/* Maximum number of clients */
			MaxClientsPerHost	VALUE(0),
					/* Maximum number of clients per host */
			MaxCopies		VALUE(100),
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
			RootCertDuration	VALUE(300),
					/* Root certificate update interval */
			RunAsUser		VALUE(FALSE),
					/* Run as unpriviledged user? */
			PrintcapFormat		VALUE(PRINTCAP_BSD);
					/* Format of printcap file? */
VAR cups_file_t		*AccessFile		VALUE(NULL),
					/* Access log file */
			*ErrorFile		VALUE(NULL),
					/* Error log file */
			*PageFile		VALUE(NULL);
					/* Page log file */
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
#  else
VAR CFArrayRef		ServerCertificatesArray	VALUE(NULL);
					/* Array containing certificates */
#  endif /* HAVE_LIBSSL || HAVE_GNUTLS */
#endif /* HAVE_SSL */


/*
 * Prototypes...
 */

extern char	*GetDateTime(time_t t);
extern int	ReadConfiguration(void);
extern int	LogRequest(client_t *con, http_status_t code);
extern int	LogMessage(int level, const char *message, ...)
#ifdef __GNUC__
__attribute__ ((__format__ (__printf__, 2, 3)))
#endif /* __GNUC__ */
;
extern int	LogPage(job_t *job, const char *page);


/*
 * End of "$Id: conf.h,v 1.36.2.19 2003/08/22 22:02:35 mike Exp $".
 */
