/*
 * "$Id: http.h,v 1.4 1998/10/16 18:28:01 mike Exp $"
 *
 *   Hyper-Text Transfer Protocol definitions for the Common UNIX Printing
 *   System (CUPS) scheduler.
 *
 *   Copyright 1997-1998 by Easy Software Products, all rights reserved.
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
 * HTTP state values...
 */

#define HTTP_WAITING		0	/* Waiting for command */
#define HTTP_OPTIONS		1	/* OPTIONS command, waiting for blank line */
#define HTTP_GET		2	/* GET command, waiting for blank line */
#define HTTP_GET_DATA		3	/* GET command, sending data */
#define HTTP_HEAD		4	/* HEAD command, waiting for blank line */
#define HTTP_POST		5	/* POST command, waiting for blank line */
#define HTTP_POST_DATA		6	/* POST command, receiving data */
#define HTTP_PUT		7	/* PUT command, waiting for blank line */
#define HTTP_PUT_DATA		8	/* PUT command, receiving data */
#define HTTP_DELETE		9	/* DELETE command, waiting for blank line */
#define HTTP_TRACE		10	/* TRACE command, waiting for blank line */
#define HTTP_CLOSE		11	/* CLOSE command, waiting for blank line */


/*
 * HTTP version numbers...
 */

#define HTTP_0_9		9	/* HTTP/0.9 */
#define HTTP_1_0		100	/* HTTP/1.0 */
#define HTTP_1_1		101	/* HTTP/1.1 */


/*
 * HTTP transfer encoding values...
 */

#define HTTP_DATA_SINGLE	0	/* Data is sent in one stream */
#define HTTP_DATA_CHUNKED	1	/* Data is chunked */


/*
 * HTTP status codes...
 */

#define HTTP_OK			200	/* OPTIONS/GET/HEAD/POST/TRACE command was successful */
#define HTTP_CREATED		201	/* PUT command was successful */
#define HTTP_ACCEPTED		202	/* DELETE command was successful */
#define HTTP_NO_CONTENT		204	/* Successful command, no new data */

#define HTTP_NOT_MODIFIED	304	/* File not modified */

#define HTTP_BAD_REQUEST	400	/* Bad request */
#define HTTP_UNAUTHORIZED	401	/* Unauthorized to access host */
#define HTTP_FORBIDDEN		403	/* Forbidden to access this URI */
#define HTTP_NOT_FOUND		404	/* URI was not found */
#define HTTP_URI_TOO_LONG	414	/* URI too long */

#define HTTP_NOT_IMPLEMENTED	501	/* Feature not implemented */
#define HTTP_NOT_SUPPORTED	505	/* HTTP version not supported */


/*
 * HTTP client structure...
 */

typedef struct
{
  int			fd;		/* File descriptor for this client */
  time_t		activity;	/* Time since last read/write */
  struct sockaddr_in	remote;		/* Address of remote interface */
  char			remote_host[256];/* Remote host name */
  int			remote_length;	/* Remote host name length */
  int			state,		/* State of client */
			version,	/* Protocol version */
			keep_alive;	/* Keep-alive supported? */
  char			host[256],	/* Host: line */
			user_agent[128],/* User-Agent: line */
			username[16],	/* Username from Authorization: line */
			password[16],	/* Password from Authorization: line */
			uri[1024],	/* Localized URL/URI for GET/PUT */
			content_type[64],/* Content-Type: line */
			language[16];	/* Accept-Language: line (first available) */
  time_t		remote_time;	/* Remote file time */
  int			remote_size;	/* Remote file size */
  int			data_encoding,	/* Chunked or not */
			data_length;	/* Content-Length: or chunk length line */
  int			file;		/* Input/output file */
  int			pipe_pid;	/* Pipe process ID (or 0 if not a pipe) */
  int			bufused;	/* Number of bytes used in input buffer */
  char			buf[MAX_BUFFER];/* Buffer for incoming messages */
} client_t;


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
extern int	SendCommand(client_t *con, int code, char *command, char *type);
extern int	SendError(client_t *con, int code);
extern int	SendFile(client_t *con, int code, char *filename,
		         char *type, struct stat *filestats);
extern int	SendHeader(client_t *con, int code, char *type);
extern void	StartListening(void);
extern void	StopListening(void);
extern int	WriteClient(client_t *con);


/*
 * End of "$Id: http.h,v 1.4 1998/10/16 18:28:01 mike Exp $".
 */
