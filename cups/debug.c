/*
 * "$Id$"
 *
 *   Debugging functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2008 by Apple Inc.
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
 *   _cups_debug_printf() - Write a formatted line to the log.
 *   _cups_debug_puts()   - Write a single line to the log.
 */

/*
 * Include necessary headers...
 */

#include "globals.h"
#include "debug.h"
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>


#ifdef DEBUG
/*
 * '_cups_debug_printf()' - Write a formatted line to the log.
 */

void
_cups_debug_printf(const char *format,	/* I - Printf-style format string */
                   ...)			/* I - Additional arguments as needed */
{
  va_list		ap;		/* Pointer to arguments */
  struct timeval	curtime;	/* Current time */
  char			buffer[2048];	/* Output buffer */
  size_t		bytes;		/* Number of bytes in buffer */
  const char		*cups_debug_log;/* CUPS_DEBUG_LOG environment variable */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */


 /*
  * See if we need to do any logging...
  */

  if (!cg->debug_init)
  {
    cg->debug_init = 1;

    if ((cups_debug_log = getenv("CUPS_DEBUG_LOG")) == NULL)
      cg->debug_fd = -1;
    else if (!strcmp(cups_debug_log, "-"))
      cg->debug_fd = 2;
    else
      cg->debug_fd = open(cups_debug_log, O_WRONLY | O_APPEND | O_CREAT, 0644);
  }

  if (cg->debug_fd < 0)
    return;

 /*
  * Format the message...
  */

  gettimeofday(&curtime, NULL);
  snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%03d ",
	   (int)((curtime.tv_sec / 3600) % 24),
	   (int)((curtime.tv_sec / 60) % 60),
	   (int)(curtime.tv_sec % 60), (int)(curtime.tv_usec / 1000));

  va_start(ap, format);
  vsnprintf(buffer + 13, sizeof(buffer) - 14, format, ap);
  va_end(ap);

  bytes = strlen(buffer);
  if (buffer[bytes - 1] != '\n')
  {
    buffer[bytes] = '\n';
    bytes ++;
    buffer[bytes] = '\0';
  }

 /*
  * Write it out...
  */

  write(cg->debug_fd, buffer, bytes);
}


/*
 * '_cups_debug_puts()' - Write a single line to the log.
 */

void
_cups_debug_puts(const char *s)		/* I - String to output */
{
  _cups_debug_printf("%s\n", s);
}


#elif 0 /* defined(__APPLE__) */
void	_cups_debug_printf(const char *format, ...) {}
void	_cups_debug_puts(const char *s) {}
#endif /* DEBUG */


/*
 * End of "$Id$".
 */
