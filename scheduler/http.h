/*
 * "$Id: http.h,v 1.3 1998/10/13 18:24:15 mike Exp $"
 *
 *   HTTP test program definitions for CUPS.
 *
 * Revision History:
 *
 *   $Log: http.h,v $
 *   Revision 1.3  1998/10/13 18:24:15  mike
 *   Added activity timeout code.
 *   Added Basic authorization code.
 *   Fixed problem with main loop that would cause a core dump.
 *
 *   Revision 1.2  1998/10/12  15:31:08  mike
 *   Switched from stdio files to file descriptors.
 *   Added FD_CLOEXEC flags to all non-essential files.
 *   Added pipe_command() function.
 *   Added write checks for all writes.
 *
 *   Revision 1.1  1998/10/12  13:57:19  mike
 *   Initial revision
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#ifdef WIN32
#  include <windows.h>
#  include <process.h>
#  include <winsock.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#  include <bstring.h>
#  include <sys/time.h>
#  include <sys/socket.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <netinet/in_systm.h>
#  include <netinet/ip.h>
#  include <netinet/tcp.h>
#endif /* WIN32 */


/*
 * Constants...
 */

#ifndef FALSE
#  define FALSE		0
#  define TRUE		(!FALSE)
#endif /* !FALSE */

#define MAX_CLIENTS		100	/* Maximum number of simultaneous clients */
#define MAX_BUFFER		8192	/* Network buffer size */

#define IPP_PORT		631	/* Port number for ipp: services */


/*
 * HTTP values...
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
#define HTTP_UNKNOWN		-1	/* Unknown command, waiting for blank line */

#define HTTP_0_9		9	/* HTTP/0.9 */
#define HTTP_1_0		100	/* HTTP/1.0 */
#define HTTP_1_1		101	/* HTTP/1.1 */

#define HTTP_DATA_SINGLE	0	/* Data is sent in one stream */
#define HTTP_DATA_CHUNKED	1	/* Data is chunked */

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
 * Data structures...
 */

typedef struct	/**** Network connection data ****/
{
  int			fd;		/* File descriptor for this connection */
  time_t		activity;	/* Time since last read/write */
  struct sockaddr_in	local,		/* Address of local interface */
			remote;		/* Address of remote interface */
  int			state,		/* State of connection */
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
} connection_t;


/*
 * Globals...
 */

#ifdef _MAIN_C_
#  define VAR
#  define VALUE(x) =x
#else
#  define VAR      extern
#  define VALUE(x)
#endif /* _MAIN_C */

VAR int			Listener;
VAR int			NumConnections VALUE(0);
VAR connection_t	Connection[MAX_CLIENTS];
VAR fd_set		InputSet,
			OutputSet;


/*
 * Prototypes...
 */

extern void	AcceptConnection(void);
extern void	CloseConnection(connection_t *con);
extern int	IsAuthorized(connection_t *con, int level);
extern int	ReadConnection(connection_t *con);
extern int	SendCommand(connection_t *con, int code, char *command, char *type);
extern int	SendError(connection_t *con, int code);
extern int	SendFile(connection_t *con, int code, char *filename,
		         char *type, struct stat *filestats);
extern int	SendHeader(connection_t *con, int code, char *type);
extern void	StartListening(void);
extern int	WriteConnection(connection_t *con);


/*
 * End of "$Id: http.h,v 1.3 1998/10/13 18:24:15 mike Exp $".
 */
