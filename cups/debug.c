/*
 * "$Id: debug.c 12328 2014-12-09 20:38:47Z msweet $"
 *
 * Debugging functions for CUPS.
 *
 * Copyright 2008-2014 by Apple Inc.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include "thread-private.h"
#ifdef WIN32
#  include <sys/timeb.h>
#  include <time.h>
#  include <io.h>
#  define getpid (int)GetCurrentProcessId
int					/* O  - 0 on success, -1 on failure */
_cups_gettimeofday(struct timeval *tv,	/* I  - Timeval struct */
                   void		  *tz)	/* I  - Timezone */
{
  struct _timeb timebuffer;		/* Time buffer struct */
  _ftime(&timebuffer);
  tv->tv_sec  = (long)timebuffer.time;
  tv->tv_usec = timebuffer.millitm * 1000;
  return 0;
}
#else
#  include <sys/time.h>
#  include <unistd.h>
#endif /* WIN32 */
#include <regex.h>
#include <fcntl.h>


/*
 * Globals...
 */

int			_cups_debug_fd = -1;
					/* Debug log file descriptor */
int			_cups_debug_level = 1;
					/* Log level (0 to 9) */


#ifdef DEBUG
/*
 * Local globals...
 */

static regex_t		*debug_filter = NULL;
					/* Filter expression for messages */
static int		debug_init = 0;	/* Did we initialize debugging? */
static _cups_mutex_t	debug_init_mutex = _CUPS_MUTEX_INITIALIZER,
					/* Mutex to control initialization */
			debug_log_mutex = _CUPS_MUTEX_INITIALIZER;
					/* Mutex to serialize log entries */


/*
 * 'debug_thread_id()' - Return an integer representing the current thread.
 */

static int				/* O - Local thread ID */
debug_thread_id(void)
{
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */


  return (cg->thread_id);
}


/*
 * 'debug_vsnprintf()' - Format a string into a fixed size buffer.
 */

static ssize_t				/* O - Number of bytes formatted */
debug_vsnprintf(char       *buffer,	/* O - Output buffer */
                size_t     bufsize,	/* O - Size of output buffer */
	        const char *format,	/* I - printf-style format string */
	        va_list    ap)		/* I - Pointer to additional arguments */
{
  char		*bufptr,		/* Pointer to position in buffer */
		*bufend,		/* Pointer to end of buffer */
		size,			/* Size character (h, l, L) */
		type;			/* Format type character */
  int		width,			/* Width of field */
		prec;			/* Number of characters of precision */
  char		tformat[100],		/* Temporary format string for snprintf() */
		*tptr,			/* Pointer into temporary format */
		temp[1024];		/* Buffer for formatted numbers */
  char		*s;			/* Pointer to string */
  ssize_t	bytes;			/* Total number of bytes needed */


  if (!buffer || bufsize < 2 || !format)
    return (-1);

 /*
  * Loop through the format string, formatting as needed...
  */

  bufptr = buffer;
  bufend = buffer + bufsize - 1;
  bytes  = 0;

  while (*format)
  {
    if (*format == '%')
    {
      tptr = tformat;
      *tptr++ = *format++;

      if (*format == '%')
      {
        if (bufptr < bufend)
	  *bufptr++ = *format;
        bytes ++;
        format ++;
	continue;
      }
      else if (strchr(" -+#\'", *format))
        *tptr++ = *format++;

      if (*format == '*')
      {
       /*
        * Get width from argument...
	*/

	format ++;
	width = va_arg(ap, int);

	snprintf(tptr, sizeof(tformat) - (size_t)(tptr - tformat), "%d", width);
	tptr += strlen(tptr);
      }
      else
      {
	width = 0;

	while (isdigit(*format & 255))
	{
	  if (tptr < (tformat + sizeof(tformat) - 1))
	    *tptr++ = *format;

	  width = width * 10 + *format++ - '0';
	}
      }

      if (*format == '.')
      {
	if (tptr < (tformat + sizeof(tformat) - 1))
	  *tptr++ = *format;

        format ++;

        if (*format == '*')
	{
         /*
	  * Get precision from argument...
	  */

	  format ++;
	  prec = va_arg(ap, int);

	  snprintf(tptr, sizeof(tformat) - (size_t)(tptr - tformat), "%d", prec);
	  tptr += strlen(tptr);
	}
	else
	{
	  prec = 0;

	  while (isdigit(*format & 255))
	  {
	    if (tptr < (tformat + sizeof(tformat) - 1))
	      *tptr++ = *format;

	    prec = prec * 10 + *format++ - '0';
	  }
	}
      }

      if (*format == 'l' && format[1] == 'l')
      {
        size = 'L';

	if (tptr < (tformat + sizeof(tformat) - 2))
	{
	  *tptr++ = 'l';
	  *tptr++ = 'l';
	}

	format += 2;
      }
      else if (*format == 'h' || *format == 'l' || *format == 'L')
      {
	if (tptr < (tformat + sizeof(tformat) - 1))
	  *tptr++ = *format;

        size = *format++;
      }
      else
        size = 0;

      if (!*format)
        break;

      if (tptr < (tformat + sizeof(tformat) - 1))
        *tptr++ = *format;

      type  = *format++;
      *tptr = '\0';

      switch (type)
      {
	case 'E' : /* Floating point formats */
	case 'G' :
	case 'e' :
	case 'f' :
	case 'g' :
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

	    snprintf(temp, sizeof(temp), tformat, va_arg(ap, double));

            bytes += (int)strlen(temp);

            if (bufptr)
	    {
	      strlcpy(bufptr, temp, (size_t)(bufend - bufptr));
	      bufptr += strlen(bufptr);
	    }
	    break;

        case 'B' : /* Integer formats */
	case 'X' :
	case 'b' :
        case 'd' :
	case 'i' :
	case 'o' :
	case 'u' :
	case 'x' :
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

#  ifdef HAVE_LONG_LONG
            if (size == 'L')
	      snprintf(temp, sizeof(temp), tformat, va_arg(ap, long long));
	    else
#  endif /* HAVE_LONG_LONG */
            if (size == 'l')
	      snprintf(temp, sizeof(temp), tformat, va_arg(ap, long));
	    else
	      snprintf(temp, sizeof(temp), tformat, va_arg(ap, int));

            bytes += (int)strlen(temp);

	    if (bufptr)
	    {
	      strlcpy(bufptr, temp, (size_t)(bufend - bufptr));
	      bufptr += strlen(bufptr);
	    }
	    break;

	case 'p' : /* Pointer value */
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

	    snprintf(temp, sizeof(temp), tformat, va_arg(ap, void *));

            bytes += (int)strlen(temp);

	    if (bufptr)
	    {
	      strlcpy(bufptr, temp, (size_t)(bufend - bufptr));
	      bufptr += strlen(bufptr);
	    }
	    break;

        case 'c' : /* Character or character array */
	    bytes += width;

	    if (bufptr)
	    {
	      if (width <= 1)
	        *bufptr++ = (char)va_arg(ap, int);
	      else
	      {
		if ((bufptr + width) > bufend)
		  width = (int)(bufend - bufptr);

		memcpy(bufptr, va_arg(ap, char *), (size_t)width);
		bufptr += width;
	      }
	    }
	    break;

	case 's' : /* String */
	    if ((s = va_arg(ap, char *)) == NULL)
	      s = "(null)";

           /*
	    * Copy the C string, replacing control chars and \ with
	    * C character escapes...
	    */

            for (bufend --; *s && bufptr < bufend; s ++)
	    {
	      if (*s == '\n')
	      {
	        *bufptr++ = '\\';
		*bufptr++ = 'n';
		bytes += 2;
	      }
	      else if (*s == '\r')
	      {
	        *bufptr++ = '\\';
		*bufptr++ = 'r';
		bytes += 2;
	      }
	      else if (*s == '\t')
	      {
	        *bufptr++ = '\\';
		*bufptr++ = 't';
		bytes += 2;
	      }
	      else if (*s == '\\')
	      {
	        *bufptr++ = '\\';
		*bufptr++ = '\\';
		bytes += 2;
	      }
	      else if (*s == '\'')
	      {
	        *bufptr++ = '\\';
		*bufptr++ = '\'';
		bytes += 2;
	      }
	      else if (*s == '\"')
	      {
	        *bufptr++ = '\\';
		*bufptr++ = '\"';
		bytes += 2;
	      }
	      else if ((*s & 255) < ' ')
	      {
	        if ((bufptr + 2) >= bufend)
	          break;

	        *bufptr++ = '\\';
		*bufptr++ = '0';
		*bufptr++ = '0' + *s / 8;
		*bufptr++ = '0' + (*s & 7);
		bytes += 4;
	      }
	      else
	      {
	        *bufptr++ = *s;
		bytes ++;
	      }
            }

            bufend ++;
	    break;

	case 'n' : /* Output number of chars so far */
	    *(va_arg(ap, int *)) = (int)bytes;
	    break;
      }
    }
    else
    {
      bytes ++;

      if (bufptr < bufend)
        *bufptr++ = *format;

      format ++;
    }
  }

 /*
  * Nul-terminate the string and return the number of characters needed.
  */

  *bufptr = '\0';

  return (bytes);
}


/*
 * '_cups_debug_printf()' - Write a formatted line to the log.
 */

void DLLExport
_cups_debug_printf(const char *format,	/* I - Printf-style format string */
                   ...)			/* I - Additional arguments as needed */
{
  va_list		ap;		/* Pointer to arguments */
  struct timeval	curtime;	/* Current time */
  char			buffer[2048];	/* Output buffer */
  ssize_t		bytes;		/* Number of bytes in buffer */
  int			level;		/* Log level in message */


 /*
  * See if we need to do any logging...
  */

  if (!debug_init)
    _cups_debug_set(getenv("CUPS_DEBUG_LOG"), getenv("CUPS_DEBUG_LEVEL"),
                    getenv("CUPS_DEBUG_FILTER"), 0);

  if (_cups_debug_fd < 0)
    return;

 /*
  * Filter as needed...
  */

  if (isdigit(format[0]))
    level = *format++ - '0';
  else
    level = 0;

  if (level > _cups_debug_level)
    return;

  if (debug_filter)
  {
    int	result;				/* Filter result */

    _cupsMutexLock(&debug_init_mutex);
    result = regexec(debug_filter, format, 0, NULL, 0);
    _cupsMutexUnlock(&debug_init_mutex);

    if (result)
      return;
  }

 /*
  * Format the message...
  */

  gettimeofday(&curtime, NULL);
  snprintf(buffer, sizeof(buffer), "T%03d %02d:%02d:%02d.%03d  ",
           debug_thread_id(), (int)((curtime.tv_sec / 3600) % 24),
	   (int)((curtime.tv_sec / 60) % 60),
	   (int)(curtime.tv_sec % 60), (int)(curtime.tv_usec / 1000));

  va_start(ap, format);
  bytes = debug_vsnprintf(buffer + 19, sizeof(buffer) - 20, format, ap) + 19;
  va_end(ap);

  if ((size_t)bytes >= (sizeof(buffer) - 1))
  {
    buffer[sizeof(buffer) - 2] = '\n';
    bytes = sizeof(buffer) - 1;
  }
  else if (buffer[bytes - 1] != '\n')
  {
    buffer[bytes++] = '\n';
    buffer[bytes]   = '\0';
  }

 /*
  * Write it out...
  */

  _cupsMutexLock(&debug_log_mutex);
  write(_cups_debug_fd, buffer, (size_t)bytes);
  _cupsMutexUnlock(&debug_log_mutex);
}


/*
 * '_cups_debug_puts()' - Write a single line to the log.
 */

void DLLExport
_cups_debug_puts(const char *s)		/* I - String to output */
{
  struct timeval	curtime;	/* Current time */
  char			buffer[2048];	/* Output buffer */
  ssize_t		bytes;		/* Number of bytes in buffer */
  int			level;		/* Log level in message */


 /*
  * See if we need to do any logging...
  */

  if (!debug_init)
    _cups_debug_set(getenv("CUPS_DEBUG_LOG"), getenv("CUPS_DEBUG_LEVEL"),
                    getenv("CUPS_DEBUG_FILTER"), 0);

  if (_cups_debug_fd < 0)
    return;

 /*
  * Filter as needed...
  */

  if (isdigit(s[0]))
    level = *s++ - '0';
  else
    level = 0;

  if (level > _cups_debug_level)
    return;

  if (debug_filter)
  {
    int	result;				/* Filter result */

    _cupsMutexLock(&debug_init_mutex);
    result = regexec(debug_filter, s, 0, NULL, 0);
    _cupsMutexUnlock(&debug_init_mutex);

    if (result)
      return;
  }

 /*
  * Format the message...
  */

  gettimeofday(&curtime, NULL);
  bytes = snprintf(buffer, sizeof(buffer), "T%03d %02d:%02d:%02d.%03d  %s",
                   debug_thread_id(), (int)((curtime.tv_sec / 3600) % 24),
		   (int)((curtime.tv_sec / 60) % 60),
		   (int)(curtime.tv_sec % 60), (int)(curtime.tv_usec / 1000),
		   s);

  if ((size_t)bytes >= (sizeof(buffer) - 1))
  {
    buffer[sizeof(buffer) - 2] = '\n';
    bytes = sizeof(buffer) - 1;
  }
  else if (buffer[bytes - 1] != '\n')
  {
    buffer[bytes++] = '\n';
    buffer[bytes]   = '\0';
  }

 /*
  * Write it out...
  */

  _cupsMutexLock(&debug_log_mutex);
  write(_cups_debug_fd, buffer, (size_t)bytes);
  _cupsMutexUnlock(&debug_log_mutex);
}


/*
 * '_cups_debug_set()' - Enable or disable debug logging.
 */

void DLLExport
_cups_debug_set(const char *logfile,	/* I - Log file or NULL */
                const char *level,	/* I - Log level or NULL */
		const char *filter,	/* I - Filter string or NULL */
		int        force)	/* I - Force initialization */
{
  _cupsMutexLock(&debug_init_mutex);

  if (!debug_init || force)
  {
   /*
    * Restore debug settings to defaults...
    */

    if (_cups_debug_fd != -1)
    {
      close(_cups_debug_fd);
      _cups_debug_fd = -1;
    }

    if (debug_filter)
    {
      regfree((regex_t *)debug_filter);
      debug_filter = NULL;
    }

    _cups_debug_level = 1;

   /*
    * Open logs, set log levels, etc.
    */

    if (!logfile)
      _cups_debug_fd = -1;
    else if (!strcmp(logfile, "-"))
      _cups_debug_fd = 2;
    else
    {
      char	buffer[1024];		/* Filename buffer */

      snprintf(buffer, sizeof(buffer), logfile, getpid());

      if (buffer[0] == '+')
	_cups_debug_fd = open(buffer + 1, O_WRONLY | O_APPEND | O_CREAT, 0644);
      else
	_cups_debug_fd = open(buffer, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    }

    if (level)
      _cups_debug_level = atoi(level);

    if (filter)
    {
      if ((debug_filter = (regex_t *)calloc(1, sizeof(regex_t))) == NULL)
	fputs("Unable to allocate memory for CUPS_DEBUG_FILTER - results not "
	      "filtered!\n", stderr);
      else if (regcomp(debug_filter, filter, REG_EXTENDED))
      {
	fputs("Bad regular expression in CUPS_DEBUG_FILTER - results not "
	      "filtered!\n", stderr);
	free(debug_filter);
	debug_filter = NULL;
      }
    }

    debug_init = 1;
  }

  _cupsMutexUnlock(&debug_init_mutex);
}
#endif /* DEBUG */


/*
 * End of "$Id: debug.c 12328 2014-12-09 20:38:47Z msweet $".
 */
