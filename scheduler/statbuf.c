/*
 * "$Id$"
 *
 *   Status buffer routines for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
 * Contents:
 *
 *   cupsdStatBufNew()    - Create a new status buffer.
 *   cupsdStatBufDelete() - Destroy a status buffer.
 *   cupsdStatBufUpdate() - Update the status buffer.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <stdarg.h>


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

      *loglevel = L_NONE;
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

  if (lineptr == NULL)
  {
   /*
    * End of file...
    */

    *loglevel = L_NONE;
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

  if (strncmp(sb->buffer, "EMERG:", 6) == 0)
  {
    *loglevel = L_EMERG;
    message   = sb->buffer + 6;
  }
  else if (strncmp(sb->buffer, "ALERT:", 6) == 0)
  {
    *loglevel = L_ALERT;
    message   = sb->buffer + 6;
  }
  else if (strncmp(sb->buffer, "CRIT:", 5) == 0)
  {
    *loglevel = L_CRIT;
    message   = sb->buffer + 5;
  }
  else if (strncmp(sb->buffer, "ERROR:", 6) == 0)
  {
    *loglevel = L_ERROR;
    message   = sb->buffer + 6;
  }
  else if (strncmp(sb->buffer, "WARNING:", 8) == 0)
  {
    *loglevel = L_WARN;
    message   = sb->buffer + 8;
  }
  else if (strncmp(sb->buffer, "NOTICE:", 6) == 0)
  {
    *loglevel = L_NOTICE;
    message   = sb->buffer + 6;
  }
  else if (strncmp(sb->buffer, "INFO:", 5) == 0)
  {
    *loglevel = L_INFO;
    message   = sb->buffer + 5;
  }
  else if (strncmp(sb->buffer, "DEBUG:", 6) == 0)
  {
    *loglevel = L_DEBUG;
    message   = sb->buffer + 6;
  }
  else if (strncmp(sb->buffer, "DEBUG2:", 7) == 0)
  {
    *loglevel = L_DEBUG2;
    message   = sb->buffer + 7;
  }
  else if (strncmp(sb->buffer, "PAGE:", 5) == 0)
  {
    *loglevel = L_PAGE;
    message   = sb->buffer + 5;
  }
  else if (strncmp(sb->buffer, "STATE:", 6) == 0)
  {
    *loglevel = L_STATE;
    message   = sb->buffer + 6;
  }
  else
  {
    *loglevel = L_DEBUG;
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

  if (*loglevel > L_NONE && (*loglevel != L_INFO || LogLevel == L_DEBUG2))
  {
   /*
    * General status message; send it to the error_log file...
    */

    LogMessage(*loglevel, "%s %s", sb->prefix, message);
  }

 /*
  * Copy the message to the line buffer...
  */

  strlcpy(line, message, linelen);

 /*
  * Copy over the buffer data we've used up...
  */

  cups_strcpy(sb->buffer, lineptr);
  sb->bufused -= lineptr - sb->buffer;

  if (sb->bufused < 0)
    sb->bufused = 0;

  return (line);
}


/*
 * End of "$Id$".
 */
