/*
 * "$Id$"
 *
 *   Client definitions for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

/*
 * HTTP client structure...
 */

struct cupsd_client_s
{
  http_t		http;		/* HTTP client connection */
  ipp_t			*request,	/* IPP request information */
			*response;	/* IPP response information */
  cupsd_location_t	*best;		/* Best match for AAA */
  time_t		start;		/* Request start time */
  http_state_t		operation;	/* Request operation */
  off_t			bytes;		/* Bytes transferred for this request */
  char			username[33],	/* Username from Authorization: line */
			password[33],	/* Password from Authorization: line */
			uri[HTTP_MAX_URI],
					/* Localized URL/URI for GET/PUT */
			*filename,	/* Filename of output file */
			*command,	/* Command to run */
			*options;	/* Options for command */
  int			file;		/* Input/output file */
  int			file_ready;	/* Input ready on file/pipe? */
  int			pipe_pid;	/* Pipe process ID (or 0 if not a pipe) */
  int			sent_header,	/* Non-zero if sent HTTP header */
			got_fields,	/* Non-zero if all fields seen */
			field_col;	/* Column within line */
  cups_lang_t		*language;	/* Language to use */
#ifdef HAVE_SSL
  int			auto_ssl;	/* Automatic test for SSL/TLS */
#endif /* HAVE_SSL */
  http_addr_t		clientaddr;	/* Client address */
  char			servername[256];/* Server name for connection */
  int			serverport;	/* Server port for connection */
};

#define HTTP(con) &((con)->http)


/*
 * HTTP listener structure...
 */

typedef struct
{
  int			fd;		/* File descriptor for this server */
  http_addr_t		address;	/* Bind address of socket */
  http_encryption_t	encryption;	/* To encrypt or not to encrypt... */
} cupsd_listener_t;


/*
 * Globals...
 */

VAR int			ListenBackLog	VALUE(SOMAXCONN),
					/* Max backlog of pending connections */
			LocalPort	VALUE(631);
					/* Local port to use */
VAR http_encryption_t	LocalEncryption	VALUE(HTTP_ENCRYPT_IF_REQUESTED);
					/* Local port encryption to use */
VAR cups_array_t	*Listeners	VALUE(NULL);
					/* Listening sockets */
VAR cups_array_t	*Clients	VALUE(NULL);
					/* HTTP clients */
VAR http_addrlist_t	*ServerAddrs	VALUE(NULL);
					/* Server address(es) */
VAR char		*ServerHeader	VALUE(NULL);
					/* Server header in requests */
VAR int			CGIPipes[2]	VALUE2(-1,-1);
					/* Pipes for CGI error/debug output */
VAR cupsd_statbuf_t	*CGIStatusBuffer VALUE(NULL);
					/* Status buffer for pipes */


/*
 * Prototypes...
 */

extern void	cupsdAcceptClient(cupsd_listener_t *lis);
extern void	cupsdCloseAllClients(void);
extern int	cupsdCloseClient(cupsd_client_t *con);
extern void	cupsdDeleteAllListeners(void);
extern void	cupsdFlushHeader(cupsd_client_t *con);
extern void	cupsdPauseListening(void);
extern int	cupsdProcessIPPRequest(cupsd_client_t *con);
extern int	cupsdReadClient(cupsd_client_t *con);
extern void	cupsdResumeListening(void);
extern int	cupsdSendCommand(cupsd_client_t *con, char *command,
		                 char *options, int root);
extern int	cupsdSendError(cupsd_client_t *con, http_status_t code);
extern int	cupsdSendHeader(cupsd_client_t *con, http_status_t code,
		                char *type);
extern void	cupsdShutdownClient(cupsd_client_t *con);
extern void	cupsdStartListening(void);
extern void	cupsdStopListening(void);
extern void	cupsdUpdateCGI(void);
extern int	cupsdWriteClient(cupsd_client_t *con);


/*
 * End of "$Id$".
 */
