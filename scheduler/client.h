/*
 * "$Id: client.h,v 1.17.2.2 2002/01/02 18:04:59 mike Exp $"
 *
 *   Client definitions for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-2002 by Easy Software Products, all rights reserved.
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
 * HTTP client structure...
 */

typedef struct
{
  http_t	http;			/* HTTP client connection */
  ipp_t		*request,		/* IPP request information */
		*response;		/* IPP response information */
  time_t	start;			/* Request start time */
  http_state_t	operation;		/* Request operation */
  int		bytes;			/* Bytes transferred for this request */
  char		username[33],		/* Username from Authorization: line */
		password[33],		/* Password from Authorization: line */
		uri[HTTP_MAX_URI],	/* Localized URL/URI for GET/PUT */
		filename[HTTP_MAX_URI],	/* Filename of output file */
		command[HTTP_MAX_URI],	/* Command to run */
		*options;		/* Options for command */
  int		file;			/* Input/output file */
  int		pipe_pid;		/* Pipe process ID (or 0 if not a pipe) */
  int		got_fields,		/* Non-zero if all fields seen */
		field_col;		/* Column within line */
  cups_lang_t	*language;		/* Language to use */
} client_t;

#define HTTP(con) &((con)->http)


/*
 * HTTP listener structure...
 */

typedef struct
{
  int			fd;		/* File descriptor for this server */
  http_addr_t		address;	/* Bind address of socket */
  http_encryption_t	encryption;	/* To encrypt or not to encrypt... */
} listener_t;


/*
 * Globals...
 */

VAR int			ListenBackLog	VALUE(SOMAXCONN);
VAR int			NumListeners	VALUE(0);
					/* Number of listening sockets */
VAR listener_t		Listeners[MAX_LISTENERS];
					/* Listening sockets */
VAR int			NumClients	VALUE(0);
					/* Number of HTTP clients */
VAR client_t		*Clients	VALUE(NULL);
					/* HTTP clients */
VAR http_addr_t		ServerAddr;	/* Server address */


/*
 * Prototypes...
 */

extern void	AcceptClient(listener_t *lis);
extern void	CloseAllClients(void);
extern void	CloseClient(client_t *con);
extern int	EncryptClient(client_t *con);
extern void	PauseListening(void);
extern void	ProcessIPPRequest(client_t *con);
extern int	ReadClient(client_t *con);
extern void	ResumeListening(void);
extern int	SendCommand(client_t *con, char *command, char *options);
extern int	SendError(client_t *con, http_status_t code);
extern int	SendFile(client_t *con, http_status_t code, char *filename,
		         char *type, struct stat *filestats);
extern int	SendHeader(client_t *con, http_status_t code, char *type);
extern void	StartListening(void);
extern void	StopListening(void);
extern int	WriteClient(client_t *con);


/*
 * End of "$Id: client.h,v 1.17.2.2 2002/01/02 18:04:59 mike Exp $".
 */
