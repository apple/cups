/*
 * "$Id: http.h,v 1.5 1999/01/24 14:25:11 mike Exp $"
 *
 *   Hyper-Text Transfer Protocol definitions for the Common UNIX Printing
 *   System (CUPS) scheduler.
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
 * HTTP state values...
 */

typedef enum
{
  HTTP_WAITING,			/* Waiting for command */
  HTTP_OPTIONS,			/* OPTIONS command, waiting for blank line */
  HTTP_GET,			/* GET command, waiting for blank line */
  HTTP_GET_DATA,		/* GET command, sending data */
  HTTP_HEAD,			/* HEAD command, waiting for blank line */
  HTTP_POST,			/* POST command, waiting for blank line */
  HTTP_POST_DATA,		/* POST command, receiving data */
  HTTP_POST_RESPONSE,		/* POST command, receiving data */
  HTTP_PUT,			/* PUT command, waiting for blank line */
  HTTP_PUT_DATA,		/* PUT command, receiving data */
  HTTP_DELETE,			/* DELETE command, waiting for blank line */
  HTTP_TRACE,			/* TRACE command, waiting for blank line */
  HTTP_CLOSE			/* CLOSE command, waiting for blank line */
} http_state_t;


/*
 * HTTP version numbers...
 */

typedef enum
{
  HTTP_0_9 = 9,			/* HTTP/0.9 */
  HTTP_1_0 = 100,		/* HTTP/1.0 */
  HTTP_1_1 = 101		/* HTTP/1.1 */
} http_version_t;


/*
 * HTTP transfer encoding values...
 */

typedef enum
{
  HTTP_DATA_SINGLE,		/* Data is sent in one stream */
  HTTP_DATA_CHUNKED		/* Data is chunked */
} http_encoding_t;


/*
 * HTTP status codes...
 */

typedef enum
{
  HTTP_OK = 200,		/* OPTIONS/GET/HEAD/POST/TRACE command was successful */
  HTTP_CREATED,			/* PUT command was successful */
  HTTP_ACCEPTED,		/* DELETE command was successful */
  HTTP_NOT_AUTHORITATIVE,	/* Information isn't authoritative */
  HTTP_NO_CONTENT,		/* Successful command, no new data */
  HTTP_RESET_CONTENT,		/* Content was reset/recreated */
  HTTP_PARTIAL_CONTENT,		/* Only a partial file was recieved/sent */

  HTTP_NOT_MODIFIED = 304,	/* File not modified */

  HTTP_BAD_REQUEST = 400,	/* Bad request */
  HTTP_UNAUTHORIZED,		/* Unauthorized to access host */
  HTTP_PAYMENT_REQUIRED,	/* Payment required */
  HTTP_FORBIDDEN,		/* Forbidden to access this URI */
  HTTP_NOT_FOUND,		/* URI was not found */
  HTTP_METHOD_NOT_ALLOWED,	/* Method is not allowed */
  HTTP_NOT_ACCEPTABLE,		/* Not Acceptable */
  HTTP_PROXY_AUTHENTICATION,	/* Proxy Authentication is Required */
  HTTP_REQUEST_TIMEOUT,		/* Request timed out */
  HTTP_CONFLICT,		/* Request is self-conflicting */
  HTTP_GONE,			/* Server has gone away */
  HTTP_LENGTH_REQUIRED,		/* A content length or encoding is required */
  HTTP_PRECONDITION,		/* Precondition failed */
  HTTP_REQUEST_TOO_LARGE,	/* Request entity too large */
  HTTP_URI_TOO_LONG,		/* URI too long */
  HTTP_UNSUPPORTED_MEDIATYPE,	/* The requested media type is unsupported */

  HTTP_SERVER_ERROR = 500,	/* Internal server error */
  HTTP_NOT_IMPLEMENTED,		/* Feature not implemented */
  HTTP_BAD_GATEWAY,		/* Bad gateway */
  HTTP_SERVICE_UNAVAILABLE,	/* Service is unavailable */
  HTTP_GATEWAY_TIMEOUT,		/* Gateway connection timed out */
  HTTP_NOT_SUPPORTED		/* HTTP version not supported */
} http_code_t;


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
  http_state_t		state;		/* State of client */
  http_version_t	version;	/* Protocol version */
  int			keep_alive;	/* Keep-alive supported? */
  char			host[256],	/* Host: line */
			user_agent[128],/* User-Agent: line */
			username[16],	/* Username from Authorization: line */
			password[16],	/* Password from Authorization: line */
			uri[1024],	/* Localized URL/URI for GET/PUT */
			content_type[64],/* Content-Type: line */
			language[16];	/* Accept-Language: line (first available) */
  time_t		remote_time;	/* Remote file time */
  int			remote_size;	/* Remote file size */
  http_encoding_t	data_encoding;	/* Chunked or not */
  int			data_length,	/* Content-Length: or chunk length line */
			data_remaining;	/* Number of bytes left */
  char			filename[MAX_NAME];/* Filename of output file */
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
extern int	SendCGI(client_t *con);
extern int	SendCommand(client_t *con, http_code_t code, char *command, char *type);
extern int	SendError(client_t *con, http_code_t code);
extern int	SendFile(client_t *con, http_code_t code, char *filename,
		         char *type, struct stat *filestats);
extern int	SendHeader(client_t *con, http_code_t code, char *type);
extern void	StartListening(void);
extern void	StopListening(void);
extern int	WriteClient(client_t *con);


/*
 * End of "$Id: http.h,v 1.5 1999/01/24 14:25:11 mike Exp $".
 */
