/*
 * "$Id: log.c,v 1.8 2000/03/21 18:35:38 mike Exp $"
 *
 *   Log file routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   LogMessage()   - Log a message to the error log file.
 *   LogPage()      - Log a page to the page log file.
 *   LogRequest()   - Log an HTTP request in Common Log Format.
 *   get_datetime() - Returns a pointer to a date/time string.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <stdarg.h>

#ifdef HAVE_VSYSLOG
#  include <syslog.h>
#endif /* HAVE_VSYSLOG */


/*
 * Local functions...
 */

static char	*get_datetime(time_t t);


/*
 * 'LogMessage()' - Log a message to the error log file.
 */

int				/* O - 1 on success, 0 on error */
LogMessage(int        level,	/* I - Log level */
           const char *message,	/* I - printf-style message string */
	   ...)			/* I - Additional args as needed */
{
  char		filename[1024],	/* Name of error log file */
		backname[1024],	/* Backup filename */
		line[1024];	/* Line for output file */
  va_list	ap;		/* Argument pointer */
  static char	levels[] =	/* Log levels... */
		{
		  'N',
		  'E',
		  'W',
		  'I',
		  'D'
		};
#ifdef HAVE_VSYSLOG
  static int	syslevels[] =	/* SYSLOG levels... */
		{
		  LOG_NOTICE,
		  LOG_ERR,
		  LOG_WARNING,
		  LOG_INFO,
		  LOG_DEBUG
		};
#endif /* HAVE_VSYSLOG */


 /*
  * See if we want to log this message...
  */

  if (level > LogLevel)
    return (1);

#ifdef HAVE_VSYSLOG
 /*
  * See if we are logging errors via syslog...
  */

  if (strcmp(ErrorLog, "syslog") == 0)
  {
    va_start(ap, message);
    vsyslog(syslevels[level], message, ap);
    va_end(ap);

    return (1);
  }
#endif /* HAVE_VSYSLOG */

 /*
  * Not using syslog; see if the error log file is open...
  */

  if (ErrorFile == NULL)
  {
   /*
    * Nope, open error log...
    */

    if (ErrorLog[0] == '\0')
      return (1);
    else if (ErrorLog[0] != '/')
      sprintf(filename, "%s/%s", ServerRoot, ErrorLog);
    else
      strcpy(filename, ErrorLog);

    if ((ErrorFile = fopen(filename, "a")) == NULL)
      return (0);
  }

 /*
  * Do we need to rotate the log?
  */

  if (ftell(ErrorFile) > MaxLogSize && MaxLogSize > 0)
  {
   /*
    * Rotate error_log file...
    */

    fclose(ErrorFile);

    if (ErrorLog[0] != '/')
      sprintf(filename, "%s/%s", ServerRoot, ErrorLog);
    else
      strcpy(filename, ErrorLog);

    strcpy(backname, filename);
    strcat(backname, ".O");

    unlink(backname);
    rename(filename, backname);

    if ((ErrorFile = fopen(filename, "a")) == NULL)
      return (0);
  }

 /*
  * Print the log level and date/time...
  */

  fprintf(ErrorFile, "%c %s ", levels[level], get_datetime(time(NULL)));

 /*
  * Then the log message...
  */

  va_start(ap, message);
  vsnprintf(line, sizeof(line), message, ap);
  va_end(ap);

 /*
  * Then a newline...
  */

  if (line[strlen(line) - 1] != '\n')
    strcat(line, "\n");

  fputs(line, ErrorFile);
  fflush(ErrorFile);

  return (1);
}


/*
 * 'LogPage()' - Log a page to the page log file.
 */

int				/* O - 1 on success, 0 on error */
LogPage(job_t       *job,	/* I - Job being printed */
        const char  *page)	/* I - Page being printed */
{
  char		filename[1024],	/* Name of error log file */
		backname[1024];	/* Backup filename */


#ifdef HAVE_VSYSLOG
 /*
  * See if we are logging pages via syslog...
  */

  if (strcmp(PageLog, "syslog") == 0)
  {
    syslog(LOG_INFO, "PAGE %s %s %d %s", job->printer->name, job->username,
           job->id, page);

    return (1);
  }
#endif /* HAVE_VSYSLOG */

 /*
  * See if the page log file is open...
  */

  if (PageFile == NULL)
  {
   /*
    * Nope, open page log...
    */

    if (PageLog[0] == '\0')
      return (1);
    else if (PageLog[0] != '/')
      sprintf(filename, "%s/%s", ServerRoot, PageLog);
    else
      strcpy(filename, PageLog);

    if ((PageFile = fopen(filename, "a")) == NULL)
      return (0);
  }

 /*
  * Do we need to rotate the log?
  */

  if (ftell(PageFile) > MaxLogSize && MaxLogSize > 0)
  {
   /*
    * Rotate page_log file...
    */

    fclose(PageFile);

    if (PageLog[0] != '/')
      sprintf(filename, "%s/%s", ServerRoot, PageLog);
    else
      strcpy(filename, PageLog);

    strcpy(backname, filename);
    strcat(backname, ".O");

    unlink(backname);
    rename(filename, backname);

    if ((PageFile = fopen(filename, "a")) == NULL)
      return (0);
  }

 /*
  * Print a page log entry of the form:
  *
  *    printer job-id user [DD/MON/YYYY:HH:MM:SS +TTTT] page num-copies
  */

  fprintf(PageFile, "%s %s %d %s %s\n", job->printer->name, job->username,
          job->id, get_datetime(time(NULL)), page);
  fflush(PageFile);

  return (1);
}


/*
 * 'LogRequest()' - Log an HTTP request in Common Log Format.
 */

int				/* O - 1 on success, 0 on error */
LogRequest(client_t      *con,	/* I - Request to log */
           http_status_t code)	/* I - Response code */
{
  char		filename[1024],	/* Name of access log file */
		backname[1024];	/* Backup filename */
  static const char *states[] =	/* HTTP client states... */
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

  if (strcmp(AccessLog, "syslog") == 0)
  {
    syslog(LOG_INFO, "REQUEST %s - %s \"%s %s HTTP/%d.%d\" %d %d\n",
           con->http.hostname, con->username[0] != '\0' ? con->username : "-",
	   states[con->operation], con->uri,
	   con->http.version / 100, con->http.version % 100,
	   code, con->bytes);

    return (1);
  }
#endif /* HAVE_VSYSLOG */

 /*
  * See if the access log is open...
  */

  if (AccessFile == NULL)
  {
   /*
    * Nope, open the access log file...
    */

    if (AccessLog[0] == '\0')
      return (1);
    else if (AccessLog[0] != '/')
      sprintf(filename, "%s/%s", ServerRoot, AccessLog);
    else
      strcpy(filename, AccessLog);

    if ((AccessFile = fopen(filename, "a")) == NULL)
      return (0);
  }

 /*
  * See if we need to rotate the log file...
  */

  if (ftell(AccessFile) > MaxLogSize && MaxLogSize > 0)
  {
   /*
    * Rotate access_log file...
    */

    fclose(AccessFile);

    if (AccessLog[0] != '/')
      sprintf(filename, "%s/%s", ServerRoot, AccessLog);
    else
      strcpy(filename, AccessLog);

    strcpy(backname, filename);
    strcat(backname, ".O");

    unlink(backname);
    rename(filename, backname);

    if ((AccessFile = fopen(filename, "a")) == NULL)
      return (0);
  }

 /*
  * Write a log of the request in "common log format"...
  */

  fprintf(AccessFile, "%s - %s %s \"%s %s HTTP/%d.%d\" %d %d\n",
          con->http.hostname, con->username[0] != '\0' ? con->username : "-",
	  get_datetime(con->start), states[con->operation], con->uri,
	  con->http.version / 100, con->http.version % 100,
	  code, con->bytes);
  fflush(AccessFile);

  return (1);
}


/*
 * 'get_datetime()' - Returns a pointer to a date/time string.
 */

static char *			/* O - Date/time string */
get_datetime(time_t t)		/* I - Time value */
{
  struct tm	*date;		/* Date/time value */
  static char	s[1024];	/* Date/time string */
  static const char *months[12] =/* Months */
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
  * (*BSD stores the timezone offset in the tm structure)
  */

  date = localtime(&t);

  sprintf(s, "[%02d/%s/%04d:%02d:%02d:%02d %+03ld%02ld]",
	  date->tm_mday, months[date->tm_mon], 1900 + date->tm_year,
	  date->tm_hour, date->tm_min, date->tm_sec,
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
          -date->tm_gmtoff / 3600, (date->tm_gmtoff / 60) % 60);
#else
          -timezone / 3600, (timezone / 60) % 60);
#endif /* __*BSD__ */
 
  return (s);
}


/*
 * End of "$Id: log.c,v 1.8 2000/03/21 18:35:38 mike Exp $".
 */
