/*
 * "$Id: sidechannel.c 10996 2013-05-29 11:51:34Z msweet $"
 *
 *   Side-channel API code for CUPS.
 *
 *   Copyright 2007-2012 by Apple Inc.
 *   Copyright 2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsSideChannelDoRequest() - Send a side-channel command to a backend and
 *                                wait for a response.
 *   cupsSideChannelRead()      - Read a side-channel message.
 *   cupsSideChannelSNMPGet()   - Query a SNMP OID's value.
 *   cupsSideChannelSNMPWalk()  - Query multiple SNMP OID values.
 *   cupsSideChannelWrite()     - Write a side-channel message.
 */

/*
 * Include necessary headers...
 */

#include "sidechannel.h"
#include "cups-private.h"
#ifdef WIN32
#  include <io.h>
#else
#  include <unistd.h>
#endif /* WIN32 */
#ifdef __hpux
#  include <sys/time.h>
#elif !defined(WIN32)
#  include <sys/select.h>
#endif /* __hpux */
#ifndef WIN32
#  include <sys/time.h>
#endif /* !WIN32 */
#ifdef HAVE_POLL
#  include <poll.h>
#endif /* HAVE_POLL */


/*
 * Buffer size for side-channel requests...
 */

#define _CUPS_SC_MAX_DATA	65535
#define _CUPS_SC_MAX_BUFFER	65540


/*
 * 'cupsSideChannelDoRequest()' - Send a side-channel command to a backend and wait for a response.
 *
 * This function is normally only called by filters, drivers, or port
 * monitors in order to communicate with the backend used by the current
 * printer.  Programs must be prepared to handle timeout or "not
 * implemented" status codes, which indicate that the backend or device
 * do not support the specified side-channel command.
 *
 * The "datalen" parameter must be initialized to the size of the buffer
 * pointed to by the "data" parameter.  cupsSideChannelDoRequest() will
 * update the value to contain the number of data bytes in the buffer.
 *
 * @since CUPS 1.3/OS X 10.5@
 */

cups_sc_status_t			/* O  - Status of command */
cupsSideChannelDoRequest(
    cups_sc_command_t command,		/* I  - Command to send */
    char              *data,		/* O  - Response data buffer pointer */
    int               *datalen,		/* IO - Size of data buffer on entry, number of bytes in buffer on return */
    double            timeout)		/* I  - Timeout in seconds */
{
  cups_sc_status_t	status;		/* Status of command */
  cups_sc_command_t	rcommand;	/* Response command */


  if (cupsSideChannelWrite(command, CUPS_SC_STATUS_NONE, NULL, 0, timeout))
    return (CUPS_SC_STATUS_TIMEOUT);

  if (cupsSideChannelRead(&rcommand, &status, data, datalen, timeout))
    return (CUPS_SC_STATUS_TIMEOUT);

  if (rcommand != command)
    return (CUPS_SC_STATUS_BAD_MESSAGE);

  return (status);
}


/*
 * 'cupsSideChannelRead()' - Read a side-channel message.
 *
 * This function is normally only called by backend programs to read
 * commands from a filter, driver, or port monitor program.  The
 * caller must be prepared to handle incomplete or invalid messages
 * and return the corresponding status codes.
 *
 * The "datalen" parameter must be initialized to the size of the buffer
 * pointed to by the "data" parameter.  cupsSideChannelDoRequest() will
 * update the value to contain the number of data bytes in the buffer.
 *
 * @since CUPS 1.3/OS X 10.5@
 */

int					/* O - 0 on success, -1 on error */
cupsSideChannelRead(
    cups_sc_command_t *command,		/* O - Command code */
    cups_sc_status_t  *status,		/* O - Status code */
    char              *data,		/* O - Data buffer pointer */
    int               *datalen,		/* IO - Size of data buffer on entry, number of bytes in buffer on return */
    double            timeout)		/* I  - Timeout in seconds */
{
  char		*buffer;		/* Message buffer */
  int		bytes;			/* Bytes read */
  int		templen;		/* Data length from message */
  int		nfds;			/* Number of file descriptors */
#ifdef HAVE_POLL
  struct pollfd	pfd;			/* Poll structure for poll() */
#else /* select() */
  fd_set	input_set;		/* Input set for select() */
  struct timeval stimeout;		/* Timeout value for select() */
#endif /* HAVE_POLL */


  DEBUG_printf(("cupsSideChannelRead(command=%p, status=%p, data=%p, "
                "datalen=%p(%d), timeout=%.3f)", command, status, data,
		datalen, datalen ? *datalen : -1, timeout));

 /*
  * Range check input...
  */

  if (!command || !status)
    return (-1);

 /*
  * See if we have pending data on the side-channel socket...
  */

#ifdef HAVE_POLL
  pfd.fd     = CUPS_SC_FD;
  pfd.events = POLLIN;

  while ((nfds = poll(&pfd, 1,
		      timeout < 0.0 ? -1 : (long)(timeout * 1000))) < 0 &&
	 (errno == EINTR || errno == EAGAIN))
    ;

#else /* select() */
  FD_ZERO(&input_set);
  FD_SET(CUPS_SC_FD, &input_set);

  stimeout.tv_sec  = (int)timeout;
  stimeout.tv_usec = (int)(timeout * 1000000) % 1000000;

  while ((nfds = select(CUPS_SC_FD + 1, &input_set, NULL, NULL,
			timeout < 0.0 ? NULL : &stimeout)) < 0 &&
	 (errno == EINTR || errno == EAGAIN))
    ;

#endif /* HAVE_POLL */

  if (nfds < 1)
  {
    *command = CUPS_SC_CMD_NONE;
    *status  = nfds==0 ? CUPS_SC_STATUS_TIMEOUT : CUPS_SC_STATUS_IO_ERROR;
    return (-1);
  }

 /*
  * Read a side-channel message for the format:
  *
  * Byte(s)  Description
  * -------  -------------------------------------------
  * 0        Command code
  * 1        Status code
  * 2-3      Data length (network byte order)
  * 4-N      Data
  */

  if ((buffer = _cupsBufferGet(_CUPS_SC_MAX_BUFFER)) == NULL)
  {
    *command = CUPS_SC_CMD_NONE;
    *status  = CUPS_SC_STATUS_TOO_BIG;

    return (-1);
  }

  while ((bytes = read(CUPS_SC_FD, buffer, _CUPS_SC_MAX_BUFFER)) < 0)
    if (errno != EINTR && errno != EAGAIN)
    {
      DEBUG_printf(("1cupsSideChannelRead: Read error: %s", strerror(errno)));

      _cupsBufferRelease(buffer);

      *command = CUPS_SC_CMD_NONE;
      *status  = CUPS_SC_STATUS_IO_ERROR;

      return (-1);
    }

 /*
  * Watch for EOF or too few bytes...
  */

  if (bytes < 4)
  {
    DEBUG_printf(("1cupsSideChannelRead: Short read of %d bytes", bytes));

    _cupsBufferRelease(buffer);

    *command = CUPS_SC_CMD_NONE;
    *status  = CUPS_SC_STATUS_BAD_MESSAGE;

    return (-1);
  }

 /*
  * Validate the command code in the message...
  */

  if (buffer[0] < CUPS_SC_CMD_SOFT_RESET ||
      buffer[0] >= CUPS_SC_CMD_MAX)
  {
    DEBUG_printf(("1cupsSideChannelRead: Bad command %d!", buffer[0]));

    _cupsBufferRelease(buffer);

    *command = CUPS_SC_CMD_NONE;
    *status  = CUPS_SC_STATUS_BAD_MESSAGE;

    return (-1);
  }

  *command = (cups_sc_command_t)buffer[0];

 /*
  * Validate the data length in the message...
  */

  templen = ((buffer[2] & 255) << 8) | (buffer[3] & 255);

  if (templen > 0 && (!data || !datalen))
  {
   /*
    * Either the response is bigger than the provided buffer or the
    * response is bigger than we've read...
    */

    *status = CUPS_SC_STATUS_TOO_BIG;
  }
  else if (!datalen || templen > *datalen || templen > (bytes - 4))
  {
   /*
    * Either the response is bigger than the provided buffer or the
    * response is bigger than we've read...
    */

    *status = CUPS_SC_STATUS_TOO_BIG;
  }
  else
  {
   /*
    * The response data will fit, copy it over and provide the actual
    * length...
    */

    *status  = (cups_sc_status_t)buffer[1];
    *datalen = templen;

    memcpy(data, buffer + 4, templen);
  }

  _cupsBufferRelease(buffer);

  DEBUG_printf(("1cupsSideChannelRead: Returning status=%d", *status));

  return (0);
}


/*
 * 'cupsSideChannelSNMPGet()' - Query a SNMP OID's value.
 *
 * This function asks the backend to do a SNMP OID query on behalf of the
 * filter, port monitor, or backend using the default community name.
 *
 * "oid" contains a numeric OID consisting of integers separated by periods,
 * for example ".1.3.6.1.2.1.43".  Symbolic names from SNMP MIBs are not
 * supported and must be converted to their numeric forms.
 *
 * On input, "data" and "datalen" provide the location and size of the
 * buffer to hold the OID value as a string. HEX-String (binary) values are
 * converted to hexadecimal strings representing the binary data, while
 * NULL-Value and unknown OID types are returned as the empty string.
 * The returned "datalen" does not include the trailing nul.
 *
 * @code CUPS_SC_STATUS_NOT_IMPLEMENTED@ is returned by backends that do not
 * support SNMP queries.  @code CUPS_SC_STATUS_NO_RESPONSE@ is returned when
 * the printer does not respond to the SNMP query.
 *
 * @since CUPS 1.4/OS X 10.6@
 */

cups_sc_status_t			/* O  - Query status */
cupsSideChannelSNMPGet(
    const char *oid,			/* I  - OID to query */
    char       *data,			/* I  - Buffer for OID value */
    int        *datalen,		/* IO - Size of OID buffer on entry, size of value on return */
    double     timeout)			/* I  - Timeout in seconds */
{
  cups_sc_status_t	status;		/* Status of command */
  cups_sc_command_t	rcommand;	/* Response command */
  char			*real_data;	/* Real data buffer for response */
  int			real_datalen,	/* Real length of data buffer */
			real_oidlen;	/* Length of returned OID string */


  DEBUG_printf(("cupsSideChannelSNMPGet(oid=\"%s\", data=%p, datalen=%p(%d), "
                "timeout=%.3f)", oid, data, datalen, datalen ? *datalen : -1,
		timeout));

 /*
  * Range check input...
  */

  if (!oid || !*oid || !data || !datalen || *datalen < 2)
    return (CUPS_SC_STATUS_BAD_MESSAGE);

  *data = '\0';

 /*
  * Send the request to the backend and wait for a response...
  */

  if (cupsSideChannelWrite(CUPS_SC_CMD_SNMP_GET, CUPS_SC_STATUS_NONE, oid,
                           (int)strlen(oid) + 1, timeout))
    return (CUPS_SC_STATUS_TIMEOUT);

  if ((real_data = _cupsBufferGet(_CUPS_SC_MAX_BUFFER)) == NULL)
    return (CUPS_SC_STATUS_TOO_BIG);

  real_datalen = _CUPS_SC_MAX_BUFFER;
  if (cupsSideChannelRead(&rcommand, &status, real_data, &real_datalen, timeout))
  {
    _cupsBufferRelease(real_data);
    return (CUPS_SC_STATUS_TIMEOUT);
  }

  if (rcommand != CUPS_SC_CMD_SNMP_GET)
  {
    _cupsBufferRelease(real_data);
    return (CUPS_SC_STATUS_BAD_MESSAGE);
  }

  if (status == CUPS_SC_STATUS_OK)
  {
   /*
    * Parse the response of the form "oid\0value"...
    */

    real_oidlen  = strlen(real_data) + 1;
    real_datalen -= real_oidlen;

    if ((real_datalen + 1) > *datalen)
    {
      _cupsBufferRelease(real_data);
      return (CUPS_SC_STATUS_TOO_BIG);
    }

    memcpy(data, real_data + real_oidlen, real_datalen);
    data[real_datalen] = '\0';

    *datalen = real_datalen;
  }

  _cupsBufferRelease(real_data);

  return (status);
}


/*
 * 'cupsSideChannelSNMPWalk()' - Query multiple SNMP OID values.
 *
 * This function asks the backend to do multiple SNMP OID queries on behalf
 * of the filter, port monitor, or backend using the default community name.
 * All OIDs under the "parent" OID are queried and the results are sent to
 * the callback function you provide.
 *
 * "oid" contains a numeric OID consisting of integers separated by periods,
 * for example ".1.3.6.1.2.1.43".  Symbolic names from SNMP MIBs are not
 * supported and must be converted to their numeric forms.
 *
 * "timeout" specifies the timeout for each OID query. The total amount of
 * time will depend on the number of OID values found and the time required
 * for each query.
 *
 * "cb" provides a function to call for every value that is found. "context"
 * is an application-defined pointer that is sent to the callback function
 * along with the OID and current data. The data passed to the callback is the
 * same as returned by @link cupsSideChannelSNMPGet@.
 *
 * @code CUPS_SC_STATUS_NOT_IMPLEMENTED@ is returned by backends that do not
 * support SNMP queries.  @code CUPS_SC_STATUS_NO_RESPONSE@ is returned when
 * the printer does not respond to the first SNMP query.
 *
 * @since CUPS 1.4/OS X 10.6@
 */

cups_sc_status_t			/* O - Status of first query of @code CUPS_SC_STATUS_OK@ on success */
cupsSideChannelSNMPWalk(
    const char          *oid,		/* I - First numeric OID to query */
    double              timeout,	/* I - Timeout for each query in seconds */
    cups_sc_walk_func_t cb,		/* I - Function to call with each value */
    void                *context)	/* I - Application-defined pointer to send to callback */
{
  cups_sc_status_t	status;		/* Status of command */
  cups_sc_command_t	rcommand;	/* Response command */
  char			*real_data;	/* Real data buffer for response */
  int			real_datalen,	/* Real length of data buffer */
			real_oidlen,	/* Length of returned OID string */
			oidlen;		/* Length of first OID */
  const char		*current_oid;	/* Current OID */
  char			last_oid[2048];	/* Last OID */


  DEBUG_printf(("cupsSideChannelSNMPWalk(oid=\"%s\", timeout=%.3f, cb=%p, "
                "context=%p)", oid, timeout, cb, context));

 /*
  * Range check input...
  */

  if (!oid || !*oid || !cb)
    return (CUPS_SC_STATUS_BAD_MESSAGE);

  if ((real_data = _cupsBufferGet(_CUPS_SC_MAX_BUFFER)) == NULL)
    return (CUPS_SC_STATUS_TOO_BIG);

 /*
  * Loop until the OIDs don't match...
  */

  current_oid = oid;
  oidlen      = (int)strlen(oid);
  last_oid[0] = '\0';

  do
  {
   /*
    * Send the request to the backend and wait for a response...
    */

    if (cupsSideChannelWrite(CUPS_SC_CMD_SNMP_GET_NEXT, CUPS_SC_STATUS_NONE,
                             current_oid, (int)strlen(current_oid) + 1, timeout))
    {
      _cupsBufferRelease(real_data);
      return (CUPS_SC_STATUS_TIMEOUT);
    }

    real_datalen = _CUPS_SC_MAX_BUFFER;
    if (cupsSideChannelRead(&rcommand, &status, real_data, &real_datalen,
                            timeout))
    {
      _cupsBufferRelease(real_data);
      return (CUPS_SC_STATUS_TIMEOUT);
    }

    if (rcommand != CUPS_SC_CMD_SNMP_GET_NEXT)
    {
      _cupsBufferRelease(real_data);
      return (CUPS_SC_STATUS_BAD_MESSAGE);
    }

    if (status == CUPS_SC_STATUS_OK)
    {
     /*
      * Parse the response of the form "oid\0value"...
      */

      if (strncmp(real_data, oid, oidlen) || real_data[oidlen] != '.' ||
          !strcmp(real_data, last_oid))
      {
       /*
        * Done with this set of OIDs...
	*/

	_cupsBufferRelease(real_data);
        return (CUPS_SC_STATUS_OK);
      }

      if (real_datalen < sizeof(real_data))
        real_data[real_datalen] = '\0';

      real_oidlen  = strlen(real_data) + 1;
      real_datalen -= real_oidlen;

     /*
      * Call the callback with the OID and data...
      */

      (*cb)(real_data, real_data + real_oidlen, real_datalen, context);

     /*
      * Update the current OID...
      */

      current_oid = real_data;
      strlcpy(last_oid, current_oid, sizeof(last_oid));
    }
  }
  while (status == CUPS_SC_STATUS_OK);

  _cupsBufferRelease(real_data);

  return (status);
}


/*
 * 'cupsSideChannelWrite()' - Write a side-channel message.
 *
 * This function is normally only called by backend programs to send
 * responses to a filter, driver, or port monitor program.
 *
 * @since CUPS 1.3/OS X 10.5@
 */

int					/* O - 0 on success, -1 on error */
cupsSideChannelWrite(
    cups_sc_command_t command,		/* I - Command code */
    cups_sc_status_t  status,		/* I - Status code */
    const char        *data,		/* I - Data buffer pointer */
    int               datalen,		/* I - Number of bytes of data */
    double            timeout)		/* I - Timeout in seconds */
{
  char		*buffer;		/* Message buffer */
  int		bytes;			/* Bytes written */
#ifdef HAVE_POLL
  struct pollfd	pfd;			/* Poll structure for poll() */
#else /* select() */
  fd_set	output_set;		/* Output set for select() */
  struct timeval stimeout;		/* Timeout value for select() */
#endif /* HAVE_POLL */


 /*
  * Range check input...
  */

  if (command < CUPS_SC_CMD_SOFT_RESET || command >= CUPS_SC_CMD_MAX ||
      datalen < 0 || datalen > _CUPS_SC_MAX_DATA || (datalen > 0 && !data))
    return (-1);

 /*
  * See if we can safely write to the side-channel socket...
  */

#ifdef HAVE_POLL
  pfd.fd     = CUPS_SC_FD;
  pfd.events = POLLOUT;

  if (timeout < 0.0)
  {
    if (poll(&pfd, 1, -1) < 1)
      return (-1);
  }
  else if (poll(&pfd, 1, (long)(timeout * 1000)) < 1)
    return (-1);

#else /* select() */
  FD_ZERO(&output_set);
  FD_SET(CUPS_SC_FD, &output_set);

  if (timeout < 0.0)
  {
    if (select(CUPS_SC_FD + 1, NULL, &output_set, NULL, NULL) < 1)
      return (-1);
  }
  else
  {
    stimeout.tv_sec  = (int)timeout;
    stimeout.tv_usec = (int)(timeout * 1000000) % 1000000;

    if (select(CUPS_SC_FD + 1, NULL, &output_set, NULL, &stimeout) < 1)
      return (-1);
  }
#endif /* HAVE_POLL */

 /*
  * Write a side-channel message in the format:
  *
  * Byte(s)  Description
  * -------  -------------------------------------------
  * 0        Command code
  * 1        Status code
  * 2-3      Data length (network byte order) <= 16384
  * 4-N      Data
  */

  if ((buffer = _cupsBufferGet(datalen + 4)) == NULL)
    return (-1);

  buffer[0] = command;
  buffer[1] = status;
  buffer[2] = datalen >> 8;
  buffer[3] = datalen & 255;

  bytes = 4;

  if (datalen > 0)
  {
    memcpy(buffer + 4, data, datalen);
    bytes += datalen;
  }

  while (write(CUPS_SC_FD, buffer, bytes) < 0)
    if (errno != EINTR && errno != EAGAIN)
    {
      _cupsBufferRelease(buffer);
      return (-1);
    }

  _cupsBufferRelease(buffer);

  return (0);
}


/*
 * End of "$Id: sidechannel.c 10996 2013-05-29 11:51:34Z msweet $".
 */
