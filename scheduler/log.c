/*
 * "$Id$"
 *
 *   Log file routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cupsdGetDateTime()   - Returns a pointer to a date/time string.
 *   cupsdLogGSSMessage() - Log a GSSAPI error...
 *   cupsdLogMessage()    - Log a message to the error log file.
 *   cupsdLogPage()       - Log a page to the page log file.
 *   cupsdLogRequest()    - Log an HTTP request in Common Log Format.
 *   check_log_file()     - Open/rotate a log file if it needs it.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <stdarg.h>
#include <syslog.h>


/*
 * Local functions...
 */

static int	check_log_file(cups_file_t **, const char *);


/*
 * 'cupsdGetDateTime()' - Returns a pointer to a date/time string.
 */

char *					/* O - Date/time string */
cupsdGetDateTime(time_t t)		/* I - Time value */
{
  struct tm	*date;			/* Date/time value */
  static time_t	last_time = -1;		/* Last time value */
  static char	s[1024];		/* Date/time string */
  static const char * const months[12] =/* Months */
		{
		  "Jan",
		  "Feb",
		  "Mar",
		  "Apr",
		  "May",
		  "Jun",
		  "Jul",
		  "Aug",
		  "Sep",
		  "Oct",
		  "Nov",
		  "Dec"
		};


  if (t != last_time)
  {
    last_time = t;

   /*
    * Get the date and time from the UNIX time value, and then format it
    * into a string.  Note that we *can't* use the strftime() function since
    * it is localized and will seriously confuse automatic programs if the
    * month names are in the wrong language!
    *
    * Also, we use the "timezone" variable that contains the current timezone
    * offset from GMT in seconds so that we are reporting local time in the
    * log files.  If you want GMT, set the TZ environment variable accordingly
    * before starting the scheduler.
    *
    * (*BSD and Darwin store the timezone offset in the tm structure)
    */

    date = localtime(&t);

    snprintf(s, sizeof(s), "[%02d/%s/%04d:%02d:%02d:%02d %+03ld%02ld]",
	     date->tm_mday, months[date->tm_mon], 1900 + date->tm_year,
	     date->tm_hour, date->tm_min, date->tm_sec,
#ifdef HAVE_TM_GMTOFF
             date->tm_gmtoff / 3600, (date->tm_gmtoff / 60) % 60);
#else
             timezone / 3600, (timezone / 60) % 60);
#endif /* HAVE_TM_GMTOFF */
  }

  return (s);
}


#ifdef HAVE_GSSAPI
/*
 * 'cupsdLogGSSMessage()' - Log a GSSAPI error...
 */

int					/* O - 1 on success, 0 on error */
cupsdLogGSSMessage(
    int        level,			/* I - Log level */
    int	       major_status,		/* I - Major GSSAPI status */
    int	       minor_status, 		/* I - Minor GSSAPI status */
    const char *message,		/* I - printf-style message string */
    ...)				/* I - Additional args as needed */
{
  OM_uint32	err_major_status,	/* Major status code for display */
		err_minor_status;	/* Minor status code for display */
  OM_uint32	msg_ctx;		/* Message context */
  gss_buffer_desc major_status_string = GSS_C_EMPTY_BUFFER,
					/* Major status message */
		minor_status_string = GSS_C_EMPTY_BUFFER;
					/* Minor status message */
  int		ret;			/* Return value */


  msg_ctx             = 0;
  err_major_status    = gss_display_status(&err_minor_status,
	                        	   major_status,
					   GSS_C_GSS_CODE,
					   GSS_C_NO_OID,
					   &msg_ctx,
					   &major_status_string);

  if (!GSS_ERROR(err_major_status))
    err_major_status = gss_display_status(&err_minor_status,
	                        	  minor_status,
					  GSS_C_MECH_CODE,
					  GSS_C_NULL_OID,
					  &msg_ctx,
					  &minor_status_string);

  ret = cupsdLogMessage(level, "%s: %s, %s", message,
			(char *)major_status_string.value,
			(char *)minor_status_string.value);
  gss_release_buffer(&err_minor_status, &major_status_string);
  gss_release_buffer(&err_minor_status, &minor_status_string);

  return (ret);
}
#endif /* HAVE_GSSAPI */


/*
 * 'cupsdLogMessage()' - Log a message to the error log file.
 */

int					/* O - 1 on success, 0 on error */
cupsdLogMessage(int        level,	/* I - Log level */
                const char *message,	/* I - printf-style message string */
	        ...)			/* I - Additional args as needed */
{
  int			len;		/* Length of message */
  va_list		ap;		/* Argument pointer */
  static const char	levels[] =	/* Log levels... */
		{
		  ' ',
		  'X',
		  'A',
		  'C',
		  'E',
		  'W',
		  'N',
		  'I',
		  'D',
		  'd'
		};
#ifdef HAVE_VSYSLOG
  static const int syslevels[] =	/* SYSLOG levels... */
		{
		  0,
		  LOG_EMERG,
		  LOG_ALERT,
		  LOG_CRIT,
		  LOG_ERR,
		  LOG_WARNING,
		  LOG_NOTICE,
		  LOG_INFO,
		  LOG_DEBUG,
		  LOG_DEBUG
		};
#endif /* HAVE_VSYSLOG */
  static int	linesize = 0;		/* Size of line for output file */
  static char	*line = NULL;		/* Line for output file */


 /*
  * See if we want to log this message...
  */

  if (TestConfigFile)
  {
    if (level <= CUPSD_LOG_WARN)
    {
      va_start(ap, message);
      vfprintf(stderr, message, ap);
      putc('\n', stderr);
      va_end(ap);
    }

    return (1);
  }

  if (level > LogLevel || !ErrorLog)
    return (1);

#ifdef HAVE_VSYSLOG
 /*
  * See if we are logging errors via syslog...
  */

  if (!strcmp(ErrorLog, "syslog"))
  {
    va_start(ap, message);
    vsyslog(syslevels[level], message, ap);
    va_end(ap);

    return (1);
  }
#endif /* HAVE_VSYSLOG */

 /*
  * Not using syslog; check the log file...
  */

  if (!check_log_file(&ErrorFile, ErrorLog))
    return (0);

 /*
  * Print the log level and date/time...
  */

  cupsFilePrintf(ErrorFile, "%c %s ", levels[level], cupsdGetDateTime(time(NULL)));

 /*
  * Allocate the line buffer as needed...
  */

  if (!linesize)
  {
    linesize = 8192;
    line     = malloc(linesize);

    if (!line)
    {
      cupsFilePrintf(ErrorFile,
                     "ERROR: Unable to allocate memory for line - %s\n",
                     strerror(errno));
      cupsFileFlush(ErrorFile);

      return (0);
    }
  }

 /*
  * Format the log message...
  */

  va_start(ap, message);
  len = vsnprintf(line, linesize, message, ap);
  va_end(ap);

 /*
  * Resize the buffer as needed...
  */

  if (len >= linesize)
  {
    char	*temp;			/* Temporary string pointer */


    len ++;

    if (len < 8192)
      len = 8192;
    else if (len > 65536)
      len = 65536;

    temp = realloc(line, len);

    if (temp)
    {
      line     = temp;
      linesize = len;
    }

    va_start(ap, message);
    len = vsnprintf(line, linesize, message, ap);
    va_end(ap);
  }

  if (len >= linesize)
    len = linesize - 1;

 /*
  * Then the log message...
  */

  cupsFilePuts(ErrorFile, line);

 /*
  * Then a newline...
  */

  if (len > 0 && line[len - 1] != '\n')
    cupsFilePutChar(ErrorFile, '\n');

 /*
  * Flush the line to the file and return...
  */

  cupsFileFlush(ErrorFile);

  return (1);
}


/*
 * 'cupsdLogPage()' - Log a page to the page log file.
 */

int					/* O - 1 on success, 0 on error */
cupsdLogPage(cupsd_job_t *job,		/* I - Job being printed */
             const char  *page)		/* I - Page being printed */
{
  ipp_attribute_t *billing,		/* job-billing attribute */
                  *hostname;		/* job-originating-host-name attribute */


  billing  = ippFindAttribute(job->attrs, "job-billing", IPP_TAG_ZERO);
  hostname = ippFindAttribute(job->attrs, "job-originating-host-name",
                              IPP_TAG_ZERO);

#ifdef HAVE_VSYSLOG
 /*
  * See if we are logging pages via syslog...
  */

  if (!strcmp(PageLog, "syslog"))
  {
    syslog(LOG_INFO, "PAGE %s %s %d %s %s %s", job->printer->name,
           job->username ? job->username : "-",
           job->id, page, billing ? billing->values[0].string.text : "-",
           hostname->values[0].string.text);

    return (1);
  }
#endif /* HAVE_VSYSLOG */

 /*
  * Not using syslog; check the log file...
  */

  if (!check_log_file(&PageFile, PageLog))
    return (0);

 /*
  * Print a page log entry of the form:
  *
  *    printer user job-id [DD/MON/YYYY:HH:MM:SS +TTTT] page num-copies \
  *        billing hostname
  */

  cupsFilePrintf(PageFile, "%s %s %d %s %s %s %s\n", job->printer->name,
        	 job->username ? job->username : "-",
        	 job->id, cupsdGetDateTime(time(NULL)), page,
		 billing ? billing->values[0].string.text : "-",
        	 hostname->values[0].string.text);
  cupsFileFlush(PageFile);

  return (1);
}


/*
 * 'cupsdLogRequest()' - Log an HTTP request in Common Log Format.
 */

int					/* O - 1 on success, 0 on error */
cupsdLogRequest(cupsd_client_t *con,	/* I - Request to log */
                http_status_t  code)	/* I - Response code */
{
  static const char * const states[] =	/* HTTP client states... */
		{
		  "WAITING",
		  "OPTIONS",
		  "GET",
		  "GET",
		  "HEAD",
		  "POST",
		  "POST",
		  "POST",
		  "PUT",
		  "PUT",
		  "DELETE",
		  "TRACE",
		  "CLOSE",
		  "STATUS"
		};


#ifdef HAVE_VSYSLOG
 /*
  * See if we are logging accesses via syslog...
  */

  if (!strcmp(AccessLog, "syslog"))
  {
    syslog(LOG_INFO,
           "REQUEST %s - %s \"%s %s HTTP/%d.%d\" %d " CUPS_LLFMT " %s %s\n",
           con->http.hostname, con->username[0] != '\0' ? con->username : "-",
	   states[con->operation], con->uri,
	   con->http.version / 100, con->http.version % 100,
	   code, CUPS_LLCAST con->bytes,
	   con->request ?
	       ippOpString(con->request->request.op.operation_id) : "-",
	   con->response ?
	       ippErrorString(con->response->request.status.status_code) : "-");

    return (1);
  }
#endif /* HAVE_VSYSLOG */

 /*
  * Not using syslog; check the log file...
  */

  if (!check_log_file(&AccessFile, AccessLog))
    return (0);

 /*
  * Write a log of the request in "common log format"...
  */

  cupsFilePrintf(AccessFile,
                 "%s - %s %s \"%s %s HTTP/%d.%d\" %d " CUPS_LLFMT " %s %s\n",
        	 con->http.hostname, con->username[0] != '\0' ? con->username : "-",
		 cupsdGetDateTime(con->start), states[con->operation], con->uri,
		 con->http.version / 100, con->http.version % 100,
		 code, CUPS_LLCAST con->bytes,
		 con->request ?
		     ippOpString(con->request->request.op.operation_id) : "-",
		 con->response ?
		     ippErrorString(con->response->request.status.status_code) :
		     "-");

  cupsFileFlush(AccessFile);

  return (1);
}


/*
 * 'check_log_file()' - Open/rotate a log file if it needs it.
 */

static int				/* O  - 1 if log file open */
check_log_file(cups_file_t **lf,	/* IO - Log file */
	       const char  *logname)	/* I  - Log filename */
{
  char		backname[1024],		/* Backup log filename */
		filename[1024],		/* Formatted log filename */
		*ptr;			/* Pointer into filename */
  const char	*logptr;		/* Pointer into log filename */


 /*
  * See if we have a log file to check...
  */

  if (!lf || !logname || !logname[0])
    return (1);

 /*
  * Format the filename as needed...
  */

  if (!*lf ||
      (strncmp(logname, "/dev/", 5) && cupsFileTell(*lf) > MaxLogSize &&
       MaxLogSize > 0))
  {
   /*
    * Handle format strings...
    */

    filename[sizeof(filename) - 1] = '\0';

    if (logname[0] != '/')
    {
      strlcpy(filename, ServerRoot, sizeof(filename));
      strlcat(filename, "/", sizeof(filename));
    }
    else
      filename[0] = '\0';

    for (logptr = logname, ptr = filename + strlen(filename);
         *logptr && ptr < (filename + sizeof(filename) - 1);
	 logptr ++)
      if (*logptr == '%')
      {
       /*
        * Format spec...
	*/

        logptr ++;
	if (*logptr == 's')
	{
	 /*
	  * Insert the server name...
	  */

	  strlcpy(ptr, ServerName, sizeof(filename) - (ptr - filename));
	  ptr += strlen(ptr);
	}
        else
	{
	 /*
	  * Otherwise just insert the character...
	  */

	  *ptr++ = *logptr;
	}
      }
      else
	*ptr++ = *logptr;

    *ptr = '\0';
  }

 /*
  * See if the log file is open...
  */

  if (!*lf)
  {
   /*
    * Nope, open the log file...
    */

    if ((*lf = cupsFileOpen(filename, "a")) == NULL)
    {
     /*
      * If the file is in CUPS_LOGDIR then try to create a missing directory...
      */

      if (!strncmp(filename, CUPS_LOGDIR, strlen(CUPS_LOGDIR)))
      {
        cupsdCheckPermissions(CUPS_LOGDIR, NULL, 0755, RunUser, Group, 1, -1);

        *lf = cupsFileOpen(filename, "a");
      }

      if (*lf == NULL)
      {
	syslog(LOG_ERR, "Unable to open log file \"%s\" - %s", filename,
	       strerror(errno));
	return (0);
      }
    }

    if (strncmp(filename, "/dev/", 5))
    {
     /*
      * Change ownership and permissions of non-device logs...
      */

      fchown(cupsFileNumber(*lf), RunUser, Group);
      fchmod(cupsFileNumber(*lf), LogFilePerm);
    }
  }

 /*
  * Do we need to rotate the log?
  */

  if (strncmp(logname, "/dev/", 5) && cupsFileTell(*lf) > MaxLogSize &&
      MaxLogSize > 0)
  {
   /*
    * Rotate log file...
    */

    cupsFileClose(*lf);

    strcpy(backname, filename);
    strlcat(backname, ".O", sizeof(backname));

    unlink(backname);
    rename(filename, backname);

    if ((*lf = cupsFileOpen(filename, "a")) == NULL)
    {
      syslog(LOG_ERR, "Unable to open log file \"%s\" - %s", filename,
             strerror(errno));

      return (0);
    }

   /*
    * Change ownership and permissions of non-device logs...
    */

    fchown(cupsFileNumber(*lf), RunUser, Group);
    fchmod(cupsFileNumber(*lf), LogFilePerm);
  }

  return (1);
}


/*
 * End of "$Id$".
 */
