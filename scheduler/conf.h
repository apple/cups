/*
 * "$Id: conf.h,v 1.34 2001/02/21 20:16:47 mike Exp $"
 *
 *   Configuration file definitions for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 1997-2001 by Easy Software Products, all rights reserved.
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
 * Globals...
 */

VAR char		ConfigurationFile[256]	VALUE(CUPS_SERVERROOT "/cupsd.conf"),
					/* Configuration file to use */
			ServerName[256]		VALUE(""),
					/* FQDN for server */
			ServerAdmin[256]	VALUE(""),
					/* Administrator's email */
			ServerRoot[1024]	VALUE(CUPS_SERVERROOT),
					/* Root directory for scheduler */
			ServerBin[1024]		VALUE(CUPS_SERVERBIN),
					/* Root directory for binaries */
			RequestRoot[1024]	VALUE(CUPS_REQUESTS),
					/* Directory for request files */
			DocumentRoot[1024]	VALUE(CUPS_DOCROOT),
					/* Root directory for documents */
			SystemGroup[32],
					/* System group name */
			AccessLog[1024]		VALUE(CUPS_LOGDIR "/access_log"),
					/* Access log filename */
			ErrorLog[1024]		VALUE(CUPS_LOGDIR "/error_log"),
					/* Error log filename */
			PageLog[1024]		VALUE(CUPS_LOGDIR "/page_log"),
					/* Page log filename */
			DataDir[1024]		VALUE(CUPS_DATADIR),
					/* Data file directory */
			DefaultLanguage[32]	VALUE("C"),
					/* Default language encoding */
			DefaultCharset[32]	VALUE(DEFAULT_CHARSET),
					/* Default charset */
			RIPCache[32]		VALUE("8m"),
					/* Amount of memory for RIPs */
			TempDir[1024]		VALUE(CUPS_REQUESTS "/tmp"),
					/* Temporary directory */
			Printcap[1024]		VALUE(""),
					/* Printcap file */
			FontPath[1024]		VALUE(CUPS_FONTPATH),
					/* Font search path */
			RemoteRoot[32]		VALUE("remroot");
					/* Remote root user */
VAR int			User			VALUE(1),
					/* User ID for server */
			Group			VALUE(0),
					/* Group ID for server */
			LogLevel		VALUE(L_ERROR),
					/* Log level */
			MaxClients		VALUE(0),
					/* Maximum number of clients */
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
			ImplicitClasses		VALUE(TRUE),
					/* Are classes implicitly created? */
			FilterLimit		VALUE(0),
					/* Max filter cost at any time */
			FilterLevel		VALUE(0),
					/* Current filter level */
			RunAsUser		VALUE(FALSE);
					/* Run as unpriviledged user? */
VAR FILE		*AccessFile		VALUE(NULL),
					/* Access log file */
			*ErrorFile		VALUE(NULL),
					/* Error log file */
			*PageFile		VALUE(NULL);
					/* Page log file */
VAR mime_t		*MimeDatabase		VALUE(NULL);
					/* MIME type database */

#ifdef HAVE_LIBSSL
VAR char		ServerCertificate[1024]	VALUE("ssl/server.crt"),
					/* Server certificate file */
			ServerKey[1024]		VALUE("ssl/server.key");
					/* Server key file */
#endif /* HAVE_LIBSSL */


/*
 * Prototypes...
 */

extern char	*GetDateTime(time_t t);
extern int	ReadConfiguration(void);
extern int	LogRequest(client_t *con, http_status_t code);
extern int	LogMessage(int level, const char *message, ...);
extern int	LogPage(job_t *job, const char *page);


/*
 * End of "$Id: conf.h,v 1.34 2001/02/21 20:16:47 mike Exp $".
 */
