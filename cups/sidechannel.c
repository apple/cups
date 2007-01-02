/*
 * "$Id$"
 *
 *   Side-channel API code for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2006 by Easy Software Products.
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
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsSideChannelDoRequest() - Send a side-channel command to a backend
 *                                and wait for a response.
 *   cupsSideChannelRead()      - Read a side-channel message.
 *   cupsSideChannelWrite()     - Write a side-channel message.
 */

/*
 * Include necessary headers...
 */

#include "sidechannel.h"
#include "string.h"
#include <unistd.h>
#include <errno.h>
#ifdef __hpux
#  include <sys/time.h>
#else
#  include <sys/select.h>
#endif /* __hpux */
#ifndef WIN32
#  include <sys/time.h>
#endif /* !WIN32 */
#ifdef HAVE_POLL
#  include <sys/poll.h>
#endif /* HAVE_POLL */


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
 * @since CUPS 1.3@
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
 * @since CUPS 1.3@
 */

int					/* O - 0 on success, -1 on error */
cupsSideChannelRead(
    cups_sc_command_t *command,		/* O - Command code */
    cups_sc_status_t  *status,		/* O - Status code */
    char              *data,		/* O - Data buffer pointer */
    int               *datalen,		/* IO - Size of data buffer on entry, number of bytes in buffer on return */
    double            timeout)		/* I  - Timeout in seconds */
{
  char		buffer[16388];		/* Message buffer */
  int		bytes;			/* Bytes read */
  int		templen;		/* Data length from message */
#ifdef HAVE_POLL
  struct pollfd	pfd;			/* Poll structure for poll() */
#else /* select() */
  fd_set	input_set;		/* Input set for select() */
  struct timeval stimeout;		/* Timeout value for select() */
#endif /* HAVE_POLL */


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

  if (timeout < 0.0)
  {
    if (poll(&pfd, 1, -1) < 1)
      return (-1);
  }
  else if (poll(&pfd, 1, (long)(timeout * 1000)) < 1)
    return (-1);

#else /* select() */
  FD_ZERO(&input_set);
  FD_SET(CUPS_SC_FD, &input_set);

  if (timeout < 0.0)
  {
    if (select(CUPS_SC_FD + 1, &input_set, NULL, NULL, NULL) < 1)
      return (-1);
  }
  else
  {
    stimeout.tv_sec  = (int)timeout;
    stimeout.tv_usec = (int)(timeout * 1000000) % 1000000;

    if (select(CUPS_SC_FD + 1, &input_set, NULL, NULL, &stimeout) < 1)
      return (-1);
  }
#endif /* HAVE_POLL */

 /*
  * Read a side-channel message for the format:
  *
  * Byte(s)  Description
  * -------  -------------------------------------------
  * 0        Command code
  * 1        Status code
  * 2-3      Data length (network byte order) <= 16384
  * 4-N      Data
  */

  while ((bytes = read(CUPS_SC_FD, buffer, sizeof(buffer))) < 0)
    if (errno != EINTR && errno != EAGAIN)
      return (-1);

 /*
  * Validate the command code in the message...
  */

  if (buffer[0] < CUPS_SC_CMD_SOFT_RESET || buffer[0] > CUPS_SC_CMD_GET_STATE)
    return (-1);

  *command = (cups_sc_command_t)buffer[0];

 /*
  * Validate the data length in the message...
  */

  templen = ((buffer[0] & 255) << 8) | (buffer[1] & 255);

  if (templen > 0 && (!data || !datalen))
  {
   /*
    * Either the response is bigger than the provided buffer or the
    * response is bigger than we've read...
    */

    *status = CUPS_SC_STATUS_TOO_BIG;
  }
  else if (templen > *datalen || templen > (bytes - 4))
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

    *status  = (cups_sc_command_t)buffer[1];
    *datalen = templen;

    memcpy(data, buffer + 4, templen);
  }

  return (0);
}


/*
 * 'cupsSideChannelWrite()' - Write a side-channel message.
 *
 * This function is normally only called by backend programs to send
 * responses to a filter, driver, or port monitor program.
 *
 * @since CUPS 1.3@
 */

int					/* O - 0 on success, -1 on error */
cupsSideChannelWrite(
    cups_sc_command_t command,		/* I - Command code */
    cups_sc_status_t  status,		/* I - Status code */
    const char        *data,		/* I - Data buffer pointer */
    int               datalen,		/* I - Number of bytes of data */
    double            timeout)		/* I - Timeout in seconds */
{
  char		buffer[16388];		/* Message buffer */
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

  if (command < CUPS_SC_CMD_SOFT_RESET || command > CUPS_SC_CMD_GET_STATE ||
      datalen < 0 || datalen > 16384 || (datalen > 0 && !data))
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
      return (-1);

  return (0);
}


/*
 * End of "$Id$".
 */
