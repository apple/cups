/*
 * "$Id: conf.h,v 1.5 1999/02/10 21:15:54 mike Exp $"
 *
 *   Configuration file definitions for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

/*
 * Log levels...
 */

#define LOG_NONE	0
#define LOG_ERROR	1
#define LOG_WARN	2
#define LOG_INFO	3
#define LOG_DEBUG	4


/*
 * Globals...
 */

VAR char		ConfigurationFile[256] VALUE("/var/spool/cups/cupsd.conf"),
					/* Configuration file to use */
			ServerName[256]	VALUE(""),
					/* FQDN for server */
			ServerAdmin[256] VALUE(""),
					/* Administrator's email */
			ServerRoot[1024] VALUE("."),
					/* Root directory for scheduler */
			DocumentRoot[1024] VALUE("."),
					/* Root directory for documents */
			SystemGroup[32]		VALUE(DEFAULT_GROUP),
					/* System group name */
			AccessLog[1024]	VALUE("logs/access_log"),
					/* Access log filename */
			ErrorLog[1024]	VALUE("logs/error_log"),
					/* Error log filename */
			DefaultLanguage[32] VALUE(DEFAULT_LANGUAGE),
					/* Default language encoding */
			DefaultCharset[32] VALUE(DEFAULT_CHARSET);
					/* Default charset */
VAR int			User		VALUE(DEFAULT_UID),
					/* User ID for server */
			Group		VALUE(DEFAULT_GID),
					/* Group ID for server */
			LogLevel	VALUE(LOG_ERROR),
					/* Log level */
			HostNameLookups	VALUE(FALSE),
					/* Do we do reverse lookups? */
			Timeout		VALUE(DEFAULT_TIMEOUT),
					/* Timeout during requests */
			KeepAlive	VALUE(TRUE),
					/* Support the Keep-Alive option? */
			KeepAliveTimeout VALUE(DEFAULT_KEEPALIVE),
					/* Timeout between requests */
			ImplicitClasses	VALUE(TRUE);
					/* Are classes implicitly created? */
VAR FILE		*AccessFile	VALUE(NULL),
					/* Access log file */
			*ErrorFile	VALUE(NULL);
					/* Error log file */
VAR mime_t		*MimeTypes	VALUE(NULL);


/*
 * Prototypes...
 */

extern int	ReadConfiguration(void);
extern int	LogRequest(client_t *con, http_status_t code);
extern int	LogMessage(int level, char *message, ...);


/*
 * End of "$Id: conf.h,v 1.5 1999/02/10 21:15:54 mike Exp $".
 */
