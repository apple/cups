/*
 * "$Id: client.h,v 1.3 1999/02/19 22:07:05 mike Exp $"
 *
 *   Client definitions for the Common UNIX Printing System (CUPS) scheduler.
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
  char		username[16],		/* Username from Authorization: line */
		password[16],		/* Password from Authorization: line */
		uri[HTTP_MAX_URI],	/* Localized URL/URI for GET/PUT */
		filename[HTTP_MAX_URI];	/* Filename of output file */
  int		file;			/* Input/output file */
  int		pipe_pid;		/* Pipe process ID (or 0 if not a pipe) */
  cups_lang_t	*language;		/* Language to use */
} client_t;

#define HTTP(con) &((con)->http)


/*
 * HTTP listener structure...
 */

typedef struct
{
  int			fd;		/* File descriptor for this client */
  struct sockaddr_in	address;	/* Bind address of socket */
} listener_t;


/*
 * Globals...
 */

VAR int			NumListeners	VALUE(0);
					/* Number of listening sockets */
VAR listener_t		Listeners[MAX_LISTENERS];
					/* Listening sockets */
VAR int			NumClients	VALUE(0);
					/* Number of HTTP clients */
VAR client_t		Clients[MAX_CLIENTS];
					/* HTTP clients */



/*
 * Prototypes...
 */

extern void	AcceptClient(listener_t *lis);
extern void	CloseAllClients(void);
extern void	CloseClient(client_t *con);
extern int	ReadClient(client_t *con);
extern int	SendCommand(client_t *con, char *command, char *options);
extern int	SendError(client_t *con, http_status_t code);
extern int	SendFile(client_t *con, http_status_t code, char *filename,
		         char *type, struct stat *filestats);
extern int	SendHeader(client_t *con, http_status_t code, char *type);
extern void	StartListening(void);
extern void	StopListening(void);
extern int	WriteClient(client_t *con);


/*
 * End of "$Id: client.h,v 1.3 1999/02/19 22:07:05 mike Exp $".
 */
