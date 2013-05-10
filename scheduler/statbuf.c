/*
 * "$Id$"
 *
 *   Status buffer routines for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cupsdStatBufDelete() - Destroy a status buffer.
 *   cupsdStatBufNew()    - Create a new status buffer.
 *   cupsdStatBufUpdate() - Update the status buffer.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <stdarg.h>


/*
 * 'cupsdStatBufDelete()' - Destroy a status buffer.
 */

void
cupsdStatBufDelete(cupsd_statbuf_t *sb)	/* I - Status buffer */
{
 /*
  * Range check input...
  */

  if (!sb)
    return;

 /*
  * Close the status pipe and free memory used...
  */

  close(sb->fd);

  free(sb);
}


/*
 * 'cupsdStatBufNew()' - Create a new status buffer.
 */

cupsd_statbuf_t	*			/* O - New status buffer */
cupsdStatBufNew(int fd,			/* I - File descriptor of pipe */
                const char *prefix,	/* I - Printf-style prefix string */
		...)			/* I - Additional args as needed */
{
  cupsd_statbuf_t	*sb;		/* New status buffer */
  va_list		ap;		/* Argument list */


 /*
  * Range check input...
  */

  if (fd < 0)
    return (NULL);

 /*
  * Allocate the status buffer...
  */

  if ((sb = calloc(1, sizeof(cupsd_statbuf_t))) != NULL)
  {
   /*
    * Assign the file descriptor...
    */

    sb->fd = fd;

   /*
    * Format the prefix string, if any.  This is usually "[Job 123]"
    * or "[Sub 123]", and so forth.
    */

    if (prefix)
    {
     /*
      * Printf-style prefix string...
      */

      va_start(ap, prefix);
      vsnprintf(sb->prefix, sizeof(sb->prefix), prefix, ap);
      va_end(ap);
    }
    else
    {
     /*
      * No prefix string...
      */

      sb->prefix[0] = '\0';
    }
  }

  return (sb);
}


/*
 * 'cupsdStatBufUpdate()' - Update the status buffer.
 */

char *					/* O - Line from buffer, "", or NULL */
cupsdStatBufUpdate(cupsd_statbuf_t *sb,	/* I - Status buffer */
                   int             *loglevel,
					/* O - Log level */ 
                   char            *line,
					/* I - Line buffer */
                   int             linelen)
					/* I - Size of line buffer */
{
  int		bytes;			/* Number of bytes read */
  char		*lineptr,		/* Pointer to end of line in buffer */
		*message;		/* Pointer to message text */


 /*
  * Check if the buffer already contains a full line...
  */

  if ((lineptr = strchr(sb->buffer, '\n')) == NULL)
  {
   /*
    * No, read more data...
    */

    if ((bytes = read(sb->fd, sb->buffer + sb->bufused,
                      CUPSD_SB_BUFFER_SIZE - sb->bufused - 1)) > 0)
    {
      sb->bufused += bytes;
      sb->buffer[sb->bufused] = '\0';

     /*
      * Guard against a line longer than the max buffer size...
      */

      if ((lineptr = strchr(sb->buffer, '\n')) == NULL &&
          sb->bufused == (CUPSD_SB_BUFFER_SIZE - 1))
	lineptr = sb->buffer + sb->bufused;
    }
    else if (bytes < 0 && errno == EINTR)
    {
     /*
      * Return an empty line if we are interrupted...
      */

      *loglevel = CUPSD_LOG_NONE;
      line[0]   = '\0';

      return (line);
    }
    else
    {
     /*
      * End-of-file, so use the whole buffer...
      */

      lineptr  = sb->buffer + sb->bufused;
      *lineptr = '\0';
    }

   /*
    * Final check for end-of-file...
    */

    if (sb->bufused == 0 && bytes == 0)
      lineptr = NULL;
  }

  if (!lineptr)
  {
   /*
    * End of file...
    */

    *loglevel = CUPSD_LOG_NONE;
    line[0]   = '\0';

    return (NULL);
  }

 /*
  * Terminate the line and process it...
  */

  *lineptr++ = '\0';

 /*
  * Figure out the logging level...
  */

  if (!strncmp(sb->buffer, "EMERG:", 6))
  {
    *loglevel = CUPSD_LOG_EMERG;
    message   = sb->buffer + 6;
  }
  else if (!strncmp(sb->buffer, "ALERT:", 6))
  {
    *loglevel = CUPSD_LOG_ALERT;
    message   = sb->buffer + 6;
  }
  else if (!strncmp(sb->buffer, "CRIT:", 5))
  {
    *loglevel = CUPSD_LOG_CRIT;
    message   = sb->buffer + 5;
  }
  else if (!strncmp(sb->buffer, "ERROR:", 6))
  {
    *loglevel = CUPSD_LOG_ERROR;
    message   = sb->buffer + 6;
  }
  else if (!strncmp(sb->buffer, "WARNING:", 8))
  {
    *loglevel = CUPSD_LOG_WARN;
    message   = sb->buffer + 8;
  }
  else if (!strncmp(sb->buffer, "NOTICE:", 7))
  {
    *loglevel = CUPSD_LOG_NOTICE;
    message   = sb->buffer + 7;
  }
  else if (!strncmp(sb->buffer, "INFO:", 5))
  {
    *loglevel = CUPSD_LOG_INFO;
    message   = sb->buffer + 5;
  }
  else if (!strncmp(sb->buffer, "DEBUG:", 6))
  {
    *loglevel = CUPSD_LOG_DEBUG;
    message   = sb->buffer + 6;
  }
  else if (!strncmp(sb->buffer, "DEBUG2:", 7))
  {
    *loglevel = CUPSD_LOG_DEBUG2;
    message   = sb->buffer + 7;
  }
  else if (!strncmp(sb->buffer, "PAGE:", 5))
  {
    *loglevel = CUPSD_LOG_PAGE;
    message   = sb->buffer + 5;
  }
  else if (!strncmp(sb->buffer, "STATE:", 6))
  {
    *loglevel = CUPSD_LOG_STATE;
    message   = sb->buffer + 6;
  }
  else if (!strncmp(sb->buffer, "ATTR:", 5))
  {
    *loglevel = CUPSD_LOG_ATTR;
    message   = sb->buffer + 5;
  }
  else if (!strncmp(sb->buffer, "PPD:", 4))
  {
    *loglevel = CUPSD_LOG_PPD;
    message   = sb->buffer + 4;
  }
  else
  {
    *loglevel = CUPSD_LOG_DEBUG;
    message   = sb->buffer;
  }

 /*
  * Skip leading whitespace in the message...
  */

  while (isspace(*message & 255))
    message ++;

 /*
  * Send it to the log file as needed...
  */

  if (sb->prefix[0])
  {
    if (*loglevel > CUPSD_LOG_NONE &&
	(*loglevel != CUPSD_LOG_INFO || LogLevel == CUPSD_LOG_DEBUG2))
    {
     /*
      * General status message; send it to the error_log file...
      */

      if (message[0] == '[')
	cupsdLogMessage(*loglevel, "%s", message);
      else
	cupsdLogMessage(*loglevel, "%s %s", sb->prefix, message);
    }
    else if (*loglevel < CUPSD_LOG_NONE && LogLevel == CUPSD_LOG_DEBUG2)
      cupsdLogMessage(CUPSD_LOG_DEBUG2, "%s %s", sb->prefix, sb->buffer);
  }

 /*
  * Copy the message to the line buffer...
  */

  strlcpy(line, message, linelen);

 /*
  * Copy over the buffer data we've used up...
  */

  if (lineptr < sb->buffer + sb->bufused)
    _cups_strcpy(sb->buffer, lineptr);

  sb->bufused -= lineptr - sb->buffer;

  if (sb->bufused < 0)
    sb->bufused = 0;

  return (line);
}


/*
 * End of "$Id$".
 */
