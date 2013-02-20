/*
 * "$Id: log.c 7918 2008-09-08 22:03:01Z mike $"
 *
 *   Log file routines for the CUPS scheduler.
 *
 *   Copyright 2007-2011 by Apple Inc.
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
 *   cupsdCheckLogFile()     - Open/rotate a log file if it needs it.
 *   cupsdGetDateTime()   - Returns a pointer to a date/time string.
 *   cupsdLogGSSMessage() - Log a GSSAPI error...
 *   cupsdLogJob()        - Log a job message.
 *   cupsdLogMessage()    - Log a message to the error log file.
 *   cupsdLogPage()       - Log a page to the page log file.
 *   cupsdLogRequest()    - Log an HTTP request in Common Log Format.
 *   cupsdWriteErrorLog() - Write a line to the ErrorLog.
 *   format_log_line()    - Format a line for a log file.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <stdarg.h>
#include <syslog.h>


/*
 * Local globals...
 */

static int	log_linesize = 0;	/* Size of line for output file */
static char	*log_line = NULL;	/* Line for output file */

#ifdef HAVE_VSYSLOG
static const int syslevels[] =		/* SYSLOG levels... */
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


/*
 * Local functions...
 */

static int	format_log_line(const char *message, va_list ap);


/*
 * 'cupsdCheckLogFile()' - Open/rotate a log file if it needs it.
 */

int					/* O  - 1 if log file open */
cupsdCheckLogFile(cups_file_t **lf,	/* IO - Log file */
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
       /*
        * Try updating the permissions of the containing log directory, using
	* the log file permissions as a basis...
	*/

        int log_dir_perm = 0300 | LogFilePerm;
					/* LogFilePerm + owner write/search */
	if (log_dir_perm & 0040)
	  log_dir_perm |= 0010;		/* Add group search */
	if (log_dir_perm & 0004)
	  log_dir_perm |= 0001;		/* Add other search */

        cupsdCheckPermissions(CUPS_LOGDIR, NULL, log_dir_perm, RunUser, Group,
	                      1, -1);

        *lf = cupsFileOpen(filename, "a");
      }

      if (*lf == NULL)
      {
	syslog(LOG_ERR, "Unable to open log file \"%s\" - %s", filename,
	       strerror(errno));

        if (FatalErrors & CUPSD_FATAL_LOG)
	  cupsdEndProcess(getpid(), 0);

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

      if (FatalErrors & CUPSD_FATAL_LOG)
	cupsdEndProcess(getpid(), 0);

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
 * 'cupsdGetDateTime()' - Returns a pointer to a date/time string.
 */

char *					/* O - Date/time string */
cupsdGetDateTime(struct timeval *t,	/* I - Time value or NULL for current */
                 cupsd_time_t   format)	/* I - Format to use */
{
  struct timeval	curtime;	/* Current time value */
  struct tm		*date;		/* Date/time value */
  static struct timeval	last_time = { 0, 0 };
	    				/* Last time we formatted */
  static char		s[1024];	/* Date/time string */
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


 /*
  * Make sure we have a valid time...
  */

  if (!t)
  {
    gettimeofday(&curtime, NULL);
    t = &curtime;
  }

  if (t->tv_sec != last_time.tv_sec ||
      (LogTimeFormat == CUPSD_TIME_USECS && t->tv_usec != last_time.tv_usec))
  {
    last_time = *t;

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

    date = localtime(&(t->tv_sec));

    if (format == CUPSD_TIME_STANDARD)
      snprintf(s, sizeof(s), "[%02d/%s/%04d:%02d:%02d:%02d %+03ld%02ld]",
	       date->tm_mday, months[date->tm_mon], 1900 + date->tm_year,
	       date->tm_hour, date->tm_min, date->tm_sec,
#ifdef HAVE_TM_GMTOFF
	       date->tm_gmtoff / 3600, (date->tm_gmtoff / 60) % 60);
#else
	       timezone / 3600, (timezone / 60) % 60);
#endif /* HAVE_TM_GMTOFF */
    else
      snprintf(s, sizeof(s), "[%02d/%s/%04d:%02d:%02d:%02d.%06d %+03ld%02ld]",
	       date->tm_mday, months[date->tm_mon], 1900 + date->tm_year,
	       date->tm_hour, date->tm_min, date->tm_sec, (int)t->tv_usec,
#ifdef HAVE_TM_GMTOFF
	       date->tm_gmtoff / 3600, (date->tm_gmtoff / 60) % 60);
#else
	       timezone / 3600, (timezone / 60) % 60);
#endif /* HAVE_TM_GMTOFF */
  }

  return (s);
}


/*
 * 'cupsdLogFCMessage()' - Log a file checking message.
 */

void
cupsdLogFCMessage(
    void              *context,		/* I - Printer (if any) */
    _cups_fc_result_t result,		/* I - Check result */
    const char        *message)		/* I - Message to log */
{
  cupsd_printer_t	*p = (cupsd_printer_t *)context;
					/* Printer */
  cupsd_loglevel_t	level;		/* Log level */


  if (result == _CUPS_FILE_CHECK_OK)
    level = CUPSD_LOG_DEBUG2;
  else
    level = CUPSD_LOG_ERROR;

  if (p)
  {
    cupsdLogMessage(level, "%s: %s", p->name, message);

    if (result == _CUPS_FILE_CHECK_MISSING ||
        result == _CUPS_FILE_CHECK_WRONG_TYPE)
    {
      strlcpy(p->state_message, message, sizeof(p->state_message));

      if (cupsdSetPrinterReasons(p, "+cups-missing-filter-warning"))
        cupsdAddEvent(CUPSD_EVENT_PRINTER_STATE, p, NULL, "%s", message);
    }
    else if (result == _CUPS_FILE_CHECK_PERMISSIONS ||
             result == _CUPS_FILE_CHECK_RELATIVE_PATH)
    {
      strlcpy(p->state_message, message, sizeof(p->state_message));

      if (cupsdSetPrinterReasons(p, "+cups-insecure-filter-warning"))
        cupsdAddEvent(CUPSD_EVENT_PRINTER_STATE, p, NULL, "%s", message);
    }
  }
  else
    cupsdLogMessage(level, "%s", message);
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
  char		buffer[8192];		/* Buffer for vsnprintf */


  if (strchr(message, '%'))
  {
   /*
    * Format the message string...
    */

    va_list	ap;			/* Pointer to arguments */

    va_start(ap, message);
    vsnprintf(buffer, sizeof(buffer), message, ap);
    va_end(ap);

    message = buffer;
  }

  msg_ctx             = 0;
  err_major_status    = gss_display_status(&err_minor_status,
	                        	   major_status,
					   GSS_C_GSS_CODE,
					   GSS_C_NO_OID,
					   &msg_ctx,
					   &major_status_string);

  if (!GSS_ERROR(err_major_status))
    gss_display_status(&err_minor_status, minor_status, GSS_C_MECH_CODE,
                       GSS_C_NULL_OID, &msg_ctx, &minor_status_string);

  ret = cupsdLogMessage(level, "%s: %s, %s", message,
			(char *)major_status_string.value,
			(char *)minor_status_string.value);
  gss_release_buffer(&err_minor_status, &major_status_string);
  gss_release_buffer(&err_minor_status, &minor_status_string);

  return (ret);
}
#endif /* HAVE_GSSAPI */


/*
 * 'cupsdLogJob()' - Log a job message.
 */

int					/* O - 1 on success, 0 on error */
cupsdLogJob(cupsd_job_t *job,		/* I - Job */
            int         level,		/* I - Log level */
	    const char  *message,	/* I - Printf-style message string */
	    ...)			/* I - Additional arguments as needed */
{
  va_list		ap, ap2;	/* Argument pointers */
  char			jobmsg[1024];	/* Format string for job message */
  int			status;		/* Formatting status */


 /*
  * See if we want to log this message...
  */

  if (TestConfigFile || !ErrorLog)
    return (1);

  if ((level > LogLevel ||
       (level == CUPSD_LOG_INFO && LogLevel < CUPSD_LOG_DEBUG)) &&
      LogDebugHistory <= 0)
    return (1);

 /*
  * Format and write the log message...
  */

  if (job)
    snprintf(jobmsg, sizeof(jobmsg), "[Job %d] %s", job->id, message);
  else
    strlcpy(jobmsg, message, sizeof(jobmsg));

  va_start(ap, message);

  do
  {
    va_copy(ap2, ap);
    status = format_log_line(jobmsg, ap2);
    va_end(ap2);
  }
  while (status == 0);

  va_end(ap);

  if (status > 0)
  {
    if (job &&
        (level > LogLevel ||
         (level == CUPSD_LOG_INFO && LogLevel < CUPSD_LOG_DEBUG)) &&
	LogDebugHistory > 0)
    {
     /*
      * Add message to the job history...
      */

      cupsd_joblog_t *temp;		/* Copy of log message */


      if ((temp = malloc(sizeof(cupsd_joblog_t) + strlen(log_line))) != NULL)
      {
        temp->time = time(NULL);
	strcpy(temp->message, log_line);
      }

      if (!job->history)
	job->history = cupsArrayNew(NULL, NULL);

      if (job->history && temp)
      {
	cupsArrayAdd(job->history, temp);

	if (cupsArrayCount(job->history) > LogDebugHistory)
	{
	 /*
	  * Remove excess messages...
	  */

	  temp = cupsArrayFirst(job->history);
	  cupsArrayRemove(job->history, temp);
	  free(temp);
	}
      }
      else if (temp)
	free(temp);

      return (1);
    }
    else if (level <= LogLevel &&
             (level != CUPSD_LOG_INFO || LogLevel >= CUPSD_LOG_DEBUG))
      return (cupsdWriteErrorLog(level, log_line));
    else
      return (1);
  }
  else
    return (cupsdWriteErrorLog(CUPSD_LOG_ERROR,
                               "Unable to allocate memory for log line!"));
}


/*
 * 'cupsdLogMessage()' - Log a message to the error log file.
 */

int					/* O - 1 on success, 0 on error */
cupsdLogMessage(int        level,	/* I - Log level */
                const char *message,	/* I - printf-style message string */
	        ...)			/* I - Additional args as needed */
{
  va_list		ap, ap2;	/* Argument pointers */
  int			status;		/* Formatting status */


 /*
  * See if we want to log this message...
  */

  if ((TestConfigFile || !ErrorLog) && level <= CUPSD_LOG_WARN)
  {
    va_start(ap, message);
#ifdef HAVE_VSYSLOG
    vsyslog(LOG_LPR | syslevels[level], message, ap);
#else
    vfprintf(stderr, message, ap);
    putc('\n', stderr);
#endif /* HAVE_VSYSLOG */
    va_end(ap);

    return (1);
  }

  if (level > LogLevel || !ErrorLog)
    return (1);

 /*
  * Format and write the log message...
  */

  va_start(ap, message);

  do
  {
    va_copy(ap2, ap);
    status = format_log_line(message, ap2);
    va_end(ap2);
  }
  while (status == 0);

  va_end(ap);

  if (status > 0)
    return (cupsdWriteErrorLog(level, log_line));
  else
    return (cupsdWriteErrorLog(CUPSD_LOG_ERROR,
                               "Unable to allocate memory for log line!"));
}


/*
 * 'cupsdLogPage()' - Log a page to the page log file.
 */

int					/* O - 1 on success, 0 on error */
cupsdLogPage(cupsd_job_t *job,		/* I - Job being printed */
             const char  *page)		/* I - Page being printed */
{
  int			i;		/* Looping var */
  char			buffer[2048],	/* Buffer for page log */
			*bufptr,	/* Pointer into buffer */
			name[256];	/* Attribute name */
  const char		*format,	/* Pointer into PageLogFormat */
			*nameend;	/* End of attribute name */
  ipp_attribute_t	*attr;		/* Current attribute */
  char			number[256];	/* Page number */
  int			copies;		/* Number of copies */


 /*
  * Format the line going into the page log...
  */

  if (!PageLogFormat)
    return (1);

  strcpy(number, "1");
  copies = 1;
  sscanf(page, "%255s%d", number, &copies);

  for (format = PageLogFormat, bufptr = buffer; *format; format ++)
  {
    if (*format == '%')
    {
      format ++;

      switch (*format)
      {
        case '%' :			/* Literal % */
	    if (bufptr < (buffer + sizeof(buffer) - 1))
	      *bufptr++ = '%';
	    break;

        case 'p' :			/* Printer name */
	    strlcpy(bufptr, job->printer->name,
	            sizeof(buffer) - (bufptr - buffer));
	    bufptr += strlen(bufptr);
	    break;

        case 'j' :			/* Job ID */
	    snprintf(bufptr, sizeof(buffer) - (bufptr - buffer), "%d", job->id);
	    bufptr += strlen(bufptr);
	    break;

        case 'u' :			/* Username */
	    strlcpy(bufptr, job->username ? job->username : "-",
	            sizeof(buffer) - (bufptr - buffer));
	    bufptr += strlen(bufptr);
	    break;

        case 'T' :			/* Date and time */
	    strlcpy(bufptr, cupsdGetDateTime(NULL, LogTimeFormat),
	            sizeof(buffer) - (bufptr - buffer));
	    bufptr += strlen(bufptr);
	    break;

        case 'P' :			/* Page number */
	    strlcpy(bufptr, number, sizeof(buffer) - (bufptr - buffer));
	    bufptr += strlen(bufptr);
	    break;

        case 'C' :			/* Number of copies */
	    snprintf(bufptr, sizeof(buffer) - (bufptr - buffer), "%d", copies);
	    bufptr += strlen(bufptr);
	    break;

        case '{' :			/* {attribute} */
	    if ((nameend = strchr(format, '}')) != NULL &&
	        (nameend - format - 2) < (sizeof(name) - 1))
	    {
	     /*
	      * Pull the name from inside the brackets...
	      */

	      memcpy(name, format + 1, nameend - format - 1);
	      name[nameend - format - 1] = '\0';

	      format = nameend;

	      if ((attr = ippFindAttribute(job->attrs, name,
	                                   IPP_TAG_ZERO)) != NULL)
	      {
	       /*
	        * Add the attribute value...
		*/

                for (i = 0;
		     i < attr->num_values &&
		         bufptr < (buffer + sizeof(buffer) - 1);
		     i ++)
		{
		  if (i)
		    *bufptr++ = ',';

		  switch (attr->value_tag)
		  {
		    case IPP_TAG_INTEGER :
		    case IPP_TAG_ENUM :
			snprintf(bufptr, sizeof(buffer) - (bufptr - buffer),
			         "%d", attr->values[i].integer);
			bufptr += strlen(bufptr);
			break;

                    case IPP_TAG_BOOLEAN :
			snprintf(bufptr, sizeof(buffer) - (bufptr - buffer),
			         "%d", attr->values[i].boolean);
			bufptr += strlen(bufptr);
		        break;

		    case IPP_TAG_TEXTLANG :
		    case IPP_TAG_NAMELANG :
		    case IPP_TAG_TEXT :
		    case IPP_TAG_NAME :
		    case IPP_TAG_KEYWORD :
		    case IPP_TAG_URI :
		    case IPP_TAG_URISCHEME :
		    case IPP_TAG_CHARSET :
		    case IPP_TAG_LANGUAGE :
		    case IPP_TAG_MIMETYPE :
		        strlcpy(bufptr, attr->values[i].string.text,
			        sizeof(buffer) - (bufptr - buffer));
			bufptr += strlen(bufptr);
		        break;

		    default :
			strlcpy(bufptr, "???",
			        sizeof(buffer) - (bufptr - buffer));
			bufptr += strlen(bufptr);
		        break;
		  }
		}
	      }
	      else if (bufptr < (buffer + sizeof(buffer) - 1))
	        *bufptr++ = '-';
	      break;
	    }

        default :
	    if (bufptr < (buffer + sizeof(buffer) - 2))
	    {
	      *bufptr++ = '%';
	      *bufptr++ = *format;
	    }
	    break;
      }
    }
    else if (bufptr < (buffer + sizeof(buffer) - 1))
      *bufptr++ = *format;
  }

  *bufptr = '\0';

#ifdef HAVE_VSYSLOG
 /*
  * See if we are logging pages via syslog...
  */

  if (!strcmp(PageLog, "syslog"))
  {
    syslog(LOG_INFO, "%s", buffer);

    return (1);
  }
#endif /* HAVE_VSYSLOG */

 /*
  * Not using syslog; check the log file...
  */

  if (!cupsdCheckLogFile(&PageFile, PageLog))
    return (0);

 /*
  * Print a page log entry of the form:
  *
  *    printer user job-id [DD/MON/YYYY:HH:MM:SS +TTTT] page num-copies \
  *        billing hostname
  */

  cupsFilePrintf(PageFile, "%s\n", buffer);
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
  char	temp[2048];			/* Temporary string for URI */
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


 /*
  * Filter requests as needed...
  */

  if (AccessLogLevel < CUPSD_ACCESSLOG_ALL)
  {
   /*
    * Eliminate simple GET, POST, and PUT requests...
    */

    if ((con->operation == HTTP_GET &&
         strncmp(con->uri, "/admin/conf", 11) &&
	 strncmp(con->uri, "/admin/log", 10)) ||
	(con->operation == HTTP_POST && !con->request &&
	 strncmp(con->uri, "/admin", 6)) ||
	(con->operation != HTTP_GET && con->operation != HTTP_POST &&
	 con->operation != HTTP_PUT))
      return (1);

    if (con->request && con->response &&
        (con->response->request.status.status_code < IPP_REDIRECTION_OTHER_SITE ||
	 con->response->request.status.status_code == IPP_NOT_FOUND))
    {
     /*
      * Check successful requests...
      */

      ipp_op_t op = con->request->request.op.operation_id;
      static cupsd_accesslog_t standard_ops[] =
      {
        CUPSD_ACCESSLOG_ALL,	/* reserved */
        CUPSD_ACCESSLOG_ALL,	/* reserved */
        CUPSD_ACCESSLOG_ACTIONS,/* Print-Job */
        CUPSD_ACCESSLOG_ACTIONS,/* Print-URI */
        CUPSD_ACCESSLOG_ACTIONS,/* Validate-Job */
        CUPSD_ACCESSLOG_ACTIONS,/* Create-Job */
        CUPSD_ACCESSLOG_ACTIONS,/* Send-Document */
        CUPSD_ACCESSLOG_ACTIONS,/* Send-URI */
        CUPSD_ACCESSLOG_ACTIONS,/* Cancel-Job */
        CUPSD_ACCESSLOG_ALL,	/* Get-Job-Attributes */
        CUPSD_ACCESSLOG_ALL,	/* Get-Jobs */
        CUPSD_ACCESSLOG_ALL,	/* Get-Printer-Attributes */
        CUPSD_ACCESSLOG_ACTIONS,/* Hold-Job */
        CUPSD_ACCESSLOG_ACTIONS,/* Release-Job */
        CUPSD_ACCESSLOG_ACTIONS,/* Restart-Job */
	CUPSD_ACCESSLOG_ALL,	/* reserved */
        CUPSD_ACCESSLOG_CONFIG,	/* Pause-Printer */
        CUPSD_ACCESSLOG_CONFIG,	/* Resume-Printer */
        CUPSD_ACCESSLOG_CONFIG,	/* Purge-Jobs */
        CUPSD_ACCESSLOG_CONFIG,	/* Set-Printer-Attributes */
        CUPSD_ACCESSLOG_ACTIONS,/* Set-Job-Attributes */
        CUPSD_ACCESSLOG_CONFIG,	/* Get-Printer-Supported-Values */
        CUPSD_ACCESSLOG_ACTIONS,/* Create-Printer-Subscription */
        CUPSD_ACCESSLOG_ACTIONS,/* Create-Job-Subscription */
        CUPSD_ACCESSLOG_ALL,	/* Get-Subscription-Attributes */
        CUPSD_ACCESSLOG_ALL,	/* Get-Subscriptions */
        CUPSD_ACCESSLOG_ACTIONS,/* Renew-Subscription */
        CUPSD_ACCESSLOG_ACTIONS,/* Cancel-Subscription */
        CUPSD_ACCESSLOG_ALL,	/* Get-Notifications */
        CUPSD_ACCESSLOG_ACTIONS,/* Send-Notifications */
        CUPSD_ACCESSLOG_ALL,	/* reserved */
        CUPSD_ACCESSLOG_ALL,	/* reserved */
        CUPSD_ACCESSLOG_ALL,	/* reserved */
        CUPSD_ACCESSLOG_ALL,	/* Get-Print-Support-Files */
        CUPSD_ACCESSLOG_CONFIG,	/* Enable-Printer */
        CUPSD_ACCESSLOG_CONFIG,	/* Disable-Printer */
        CUPSD_ACCESSLOG_CONFIG,	/* Pause-Printer-After-Current-Job */
        CUPSD_ACCESSLOG_ACTIONS,/* Hold-New-Jobs */
        CUPSD_ACCESSLOG_ACTIONS,/* Release-Held-New-Jobs */
        CUPSD_ACCESSLOG_CONFIG,	/* Deactivate-Printer */
        CUPSD_ACCESSLOG_CONFIG,	/* Activate-Printer */
        CUPSD_ACCESSLOG_CONFIG,	/* Restart-Printer */
        CUPSD_ACCESSLOG_CONFIG,	/* Shutdown-Printer */
        CUPSD_ACCESSLOG_CONFIG,	/* Startup-Printer */
        CUPSD_ACCESSLOG_ACTIONS,/* Reprocess-Job */
        CUPSD_ACCESSLOG_ACTIONS,/* Cancel-Current-Job */
        CUPSD_ACCESSLOG_ACTIONS,/* Suspend-Current-Job */
        CUPSD_ACCESSLOG_ACTIONS,/* Resume-Job */
        CUPSD_ACCESSLOG_ACTIONS,/* Promote-Job */
        CUPSD_ACCESSLOG_ACTIONS	/* Schedule-Job-After */
      };
      static cupsd_accesslog_t cups_ops[] =
      {
        CUPSD_ACCESSLOG_ALL,	/* CUPS-Get-Default */
        CUPSD_ACCESSLOG_ALL,	/* CUPS-Get-Printers */
        CUPSD_ACCESSLOG_CONFIG,	/* CUPS-Add-Modify-Printer */
        CUPSD_ACCESSLOG_CONFIG,	/* CUPS-Delete-Printer */
        CUPSD_ACCESSLOG_ALL,	/* CUPS-Get-Classes */
        CUPSD_ACCESSLOG_CONFIG,	/* CUPS-Add-Modify-Class */
        CUPSD_ACCESSLOG_CONFIG,	/* CUPS-Delete-Class */
        CUPSD_ACCESSLOG_CONFIG,	/* CUPS-Accept-Jobs */
        CUPSD_ACCESSLOG_CONFIG,	/* CUPS-Reject-Jobs */
        CUPSD_ACCESSLOG_CONFIG,	/* CUPS-Set-Default */
        CUPSD_ACCESSLOG_CONFIG,	/* CUPS-Get-Devices */
        CUPSD_ACCESSLOG_CONFIG,	/* CUPS-Get-PPDs */
        CUPSD_ACCESSLOG_ACTIONS,/* CUPS-Move-Job */
        CUPSD_ACCESSLOG_ACTIONS,/* CUPS-Authenticate-Job */
        CUPSD_ACCESSLOG_ALL	/* CUPS-Get-PPD */
      };


      if ((op <= IPP_SCHEDULE_JOB_AFTER && standard_ops[op] > AccessLogLevel) ||
          (op >= CUPS_GET_DEFAULT && op <= CUPS_GET_PPD &&
	   cups_ops[op - CUPS_GET_DEFAULT] > AccessLogLevel))
        return (1);
    }
  }

#ifdef HAVE_VSYSLOG
 /*
  * See if we are logging accesses via syslog...
  */

  if (!strcmp(AccessLog, "syslog"))
  {
    syslog(LOG_INFO,
           "REQUEST %s - %s \"%s %s HTTP/%d.%d\" %d " CUPS_LLFMT " %s %s\n",
           con->http.hostname, con->username[0] != '\0' ? con->username : "-",
	   states[con->operation], _httpEncodeURI(temp, con->uri, sizeof(temp)),
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

  if (!cupsdCheckLogFile(&AccessFile, AccessLog))
    return (0);

 /*
  * Write a log of the request in "common log format"...
  */

  cupsFilePrintf(AccessFile,
                 "%s - %s %s \"%s %s HTTP/%d.%d\" %d " CUPS_LLFMT " %s %s\n",
        	 con->http.hostname,
		 con->username[0] != '\0' ? con->username : "-",
		 cupsdGetDateTime(&(con->start), LogTimeFormat),
		 states[con->operation],
		 _httpEncodeURI(temp, con->uri, sizeof(temp)),
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
 * 'cupsdWriteErrorLog()' - Write a line to the ErrorLog.
 */

int					/* O - 1 on success, 0 on failure */
cupsdWriteErrorLog(int        level,	/* I - Log level */
                   const char *message)	/* I - Message string */
{
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
 /*
  * See if we are logging errors via syslog...
  */

  if (!strcmp(ErrorLog, "syslog"))
  {
    syslog(syslevels[level], "%s", message);
    return (1);
  }
#endif /* HAVE_VSYSLOG */

 /*
  * Not using syslog; check the log file...
  */

  if (!cupsdCheckLogFile(&ErrorFile, ErrorLog))
    return (0);

 /*
  * Write the log message...
  */

  cupsFilePrintf(ErrorFile, "%c %s %s\n", levels[level],
                 cupsdGetDateTime(NULL, LogTimeFormat), message);
  cupsFileFlush(ErrorFile);

  return (1);
}


/*
 * 'format_log_line()' - Format a line for a log file.
 *
 * This function resizes a global string buffer as needed.  Each call returns
 * a pointer to this buffer, so the contents are only good until the next call
 * to format_log_line()...
 */

static int				/* O - -1 for fatal, 0 for retry, 1 for success */
format_log_line(const char *message,	/* I - Printf-style format string */
                va_list    ap)		/* I - Argument list */
{
  int		len;			/* Length of formatted line */


 /*
  * Allocate the line buffer as needed...
  */

  if (!log_linesize)
  {
    log_linesize = 8192;
    log_line     = malloc(log_linesize);

    if (!log_line)
      return (-1);
  }

 /*
  * Format the log message...
  */

  len = vsnprintf(log_line, log_linesize, message, ap);

 /*
  * Resize the buffer as needed...
  */

  if (len >= log_linesize && log_linesize < 65536)
  {
    char	*temp;			/* Temporary string pointer */


    len ++;

    if (len < 8192)
      len = 8192;
    else if (len > 65536)
      len = 65536;

    temp = realloc(log_line, len);

    if (temp)
    {
      log_line     = temp;
      log_linesize = len;

      return (0);
    }
  }

  return (1);
}


/*
 * End of "$Id: log.c 7918 2008-09-08 22:03:01Z mike $".
 */
