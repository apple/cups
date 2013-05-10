/*
 * "$Id$"
 *
 *   "mailto" notifier for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
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
 *   main()               - Main entry for the mailto notifier.
 *   email_message()      - Email a notification message.
 *   load_configuration() - Load the mailto.conf file.
 *   pipe_sendmail()      - Open a pipe to sendmail...
 *   print_attributes()   - Print the attributes in a request...
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/i18n.h>
#include <cups/string.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>


/*
 * Globals...
 */

char	mailtoCc[1024];			/* Cc email address */
char	mailtoFrom[1024];		/* From email address */
char	mailtoReplyTo[1024];		/* Reply-To email address */
char	mailtoSubject[1024];		/* Subject prefix */
char	mailtoSMTPServer[1024];		/* SMTP server to use */
char	mailtoSendmail[1024];		/* Sendmail program to use */


/*
 * Local functions...
 */

void		email_message(const char *to, const char *subject,
		              const char *text);
int		load_configuration(void);
cups_file_t	*pipe_sendmail(const char *to);
void		print_attributes(ipp_t *ipp, int indent);


/*
 * 'main()' - Main entry for the mailto notifier.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  ipp_t		*msg;			/* Event message from scheduler */
  ipp_state_t	state;			/* IPP event state */
  char		*subject,		/* Subject for notification message */
		*text;			/* Text for notification message */
  cups_lang_t	*lang;			/* Language info */
  char		temp[1024];		/* Temporary string */
  int		templen;		/* Length of temporary string */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* POSIX sigaction data */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Don't buffer stderr...
  */

  setbuf(stderr, NULL);

 /*
  * Ignore SIGPIPE signals...
  */

#ifdef HAVE_SIGSET
  sigset(SIGPIPE, SIG_IGN);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &action, NULL);
#else
  signal(SIGPIPE, SIG_IGN);
#endif /* HAVE_SIGSET */

 /*
  * Validate command-line options...
  */

  if (argc != 3)
  {
    fputs("Usage: mailto mailto:user@domain.com notify-user-data\n", stderr);
    return (1);
  }

  if (strncmp(argv[1], "mailto:", 7))
  {
    fprintf(stderr, "ERROR: Bad recipient \"%s\"!\n", argv[1]);
    return (1);
  }

  fprintf(stderr, "DEBUG: argc=%d\n", argc);
  for (i = 0; i < argc; i ++)
    fprintf(stderr, "DEBUG: argv[%d]=\"%s\"\n", i, argv[i]);

 /*
  * Load configuration data...
  */

  if ((lang = cupsLangDefault()) == NULL)
    return (1);

  if (!load_configuration())
    return (1);

 /*
  * Get the reply-to address...
  */

  templen = sizeof(temp);
  httpDecode64_2(temp, &templen, argv[2]);

  if (!strncmp(temp, "mailto:", 7))
    strlcpy(mailtoReplyTo, temp + 7, sizeof(mailtoReplyTo));
  else if (temp[0])
    fprintf(stderr, "WARNING: Bad notify-user-data value (%d bytes) ignored!\n",
            templen);

 /*
  * Loop forever until we run out of events...
  */

  for (;;)
  {
   /*
    * Get the next event...
    */

    msg = ippNew();
    while ((state = ippReadFile(0, msg)) != IPP_DATA)
    {
      if (state <= IPP_IDLE)
        break;
    }

    fprintf(stderr, "DEBUG: state=%d\n", state);

    if (state == IPP_ERROR)
      fputs("DEBUG: ippReadFile() returned IPP_ERROR!\n", stderr);

    if (state <= IPP_IDLE)
    {
     /*
      * Out of messages, free memory and then exit...
      */

      ippDelete(msg);
      return (0);
    }

   /*
    * Get the subject and text for the message, then email it...
    */

    subject = cupsNotifySubject(lang, msg);
    text    = cupsNotifyText(lang, msg);

    fprintf(stderr, "DEBUG: subject=\"%s\"\n", subject);
    fprintf(stderr, "DEBUG: text=\"%s\"\n", text);

    if (subject && text)
      email_message(argv[1] + 7, subject, text);
    else
    {
      fputs("ERROR: Missing attributes in event notification!\n", stderr);
      print_attributes(msg, 4);
    }

   /*
    * Free the memory used for this event...
    */

    if (subject)
      free(subject);

    if (text)
      free(text);

    ippDelete(msg);
  }
}


/*
 * 'email_message()' - Email a notification message.
 */

void
email_message(const char *to,		/* I - Recipient of message */
              const char *subject,	/* I - Subject of message */
	      const char *text)		/* I - Text of message */
{
  cups_file_t	*fp;			/* Pipe/socket to mail server */
  const char	*nl;			/* Newline to use */
  char		response[1024];		/* SMTP response buffer */


 /*
  * Connect to the mail server...
  */

  if (mailtoSendmail[0])
  {
   /*
    * Use the sendmail command...
    */

    fp = pipe_sendmail(to);

    if (!fp)
      return;

    nl = "\n";
  }
  else
  {
   /*
    * Use an SMTP server...
    */

    char	hostbuf[1024];		/* Local hostname */


    if (strchr(mailtoSMTPServer, ':'))
      fp = cupsFileOpen(mailtoSMTPServer, "s");
    else
    {
      char	spec[1024];		/* Host:service spec */


      snprintf(spec, sizeof(spec), "%s:smtp", mailtoSMTPServer);
      fp = cupsFileOpen(spec, "s");
    }

    if (!fp)
    {
      fprintf(stderr, "ERROR: Unable to connect to SMTP server \"%s\"!\n",
              mailtoSMTPServer);
      return;
    }

    fprintf(stderr, "DEBUG: Connected to \"%s\"...\n", mailtoSMTPServer);

    cupsFilePrintf(fp, "HELO %s\r\n",
                   httpGetHostname(NULL, hostbuf, sizeof(hostbuf)));
    fprintf(stderr, "DEBUG: >>> HELO %s\n", hostbuf);

    if (!cupsFileGets(fp, response, sizeof(response)) || atoi(response) >= 500)
      goto smtp_error;
    fprintf(stderr, "DEBUG: <<< %s\n", response);

    cupsFilePrintf(fp, "MAIL FROM:%s\r\n", mailtoFrom);
    fprintf(stderr, "DEBUG: >>> MAIL FROM:%s\n", mailtoFrom);

    if (!cupsFileGets(fp, response, sizeof(response)) || atoi(response) >= 500)
      goto smtp_error;
    fprintf(stderr, "DEBUG: <<< %s\n", response);

    cupsFilePrintf(fp, "RCPT TO:%s\r\n", to);
    fprintf(stderr, "DEBUG: >>> RCPT TO:%s\n", to);

    if (!cupsFileGets(fp, response, sizeof(response)) || atoi(response) >= 500)
      goto smtp_error;
    fprintf(stderr, "DEBUG: <<< %s\n", response);

    cupsFilePuts(fp, "DATA\r\n");
    fputs("DEBUG: DATA\n", stderr);

    if (!cupsFileGets(fp, response, sizeof(response)) || atoi(response) >= 500)
      goto smtp_error;
    fprintf(stderr, "DEBUG: <<< %s\n", response);

    nl = "\r\n";
  }

 /*
  * Send the message...
  */

  cupsFilePrintf(fp, "Date: %s%s", httpGetDateString(time(NULL)), nl);
  cupsFilePrintf(fp, "From: %s%s", mailtoFrom, nl);
  cupsFilePrintf(fp, "Subject: %s %s%s", mailtoSubject, subject, nl);
  if (mailtoReplyTo[0])
  {
    cupsFilePrintf(fp, "Sender: %s%s", mailtoReplyTo, nl);
    cupsFilePrintf(fp, "Reply-To: %s%s", mailtoReplyTo, nl);
  }
  cupsFilePrintf(fp, "To: %s%s", to, nl);
  if (mailtoCc[0])
    cupsFilePrintf(fp, "Cc: %s%s", mailtoCc, nl);
  cupsFilePrintf(fp, "Content-Type: text/plain%s", nl);
  cupsFilePuts(fp, nl);
  cupsFilePrintf(fp, "%s%s", text, nl);
  cupsFilePrintf(fp, ".\n", nl);

 /*
  * Close the connection to the mail server...
  */

  if (mailtoSendmail[0])
  {
   /*
    * Close the pipe and wait for the sendmail command to finish...
    */

    int	status;				/* Exit status */


    cupsFileClose(fp);

    if (wait(&status))
      status = errno << 8;

   /*
    * Report any non-zero status...
    */

    if (status)
    {
      if (WIFEXITED(status))
        fprintf(stderr, "ERROR: Sendmail command returned status %d!\n",
	        WEXITSTATUS(status));
      else
        fprintf(stderr, "ERROR: Sendmail command crashed on signal %d!\n",
	        WTERMSIG(status));
    }
  }
  else
  {
   /*
    * Finish up the SMTP submission and close the connection...
    */

    if (!cupsFileGets(fp, response, sizeof(response)) || atoi(response) >= 500)
      goto smtp_error;
    fprintf(stderr, "DEBUG: <<< %s\n", response);

   /*
    * Process SMTP errors here...
    */

    smtp_error:

    cupsFilePuts(fp, "QUIT\r\n");
    fputs("DEBUG: QUIT\n", stderr);

    if (!cupsFileGets(fp, response, sizeof(response)) || atoi(response) >= 500)
      goto smtp_error;
    fprintf(stderr, "DEBUG: <<< %s\n", response);

    cupsFileClose(fp);

    fprintf(stderr, "DEBUG: Closed connection to \"%s\"...\n",
            mailtoSMTPServer);
  }
}


/*
 * 'load_configuration()' - Load the mailto.conf file.
 */

int					/* I - 1 on success, 0 on failure */
load_configuration(void)
{
  cups_file_t	*fp;			/* mailto.conf file */
  const char	*server_root,		/* CUPS_SERVERROOT environment variable */
		*server_admin;		/* SERVER_ADMIN environment variable */
  char		line[1024],		/* Line from file */
		*value;			/* Value for directive */
  int		linenum;		/* Line number in file */


 /*
  * Initialize defaults...
  */

  mailtoCc[0] = '\0';

  if ((server_admin = getenv("SERVER_ADMIN")) != NULL)
    strlcpy(mailtoFrom, server_admin, sizeof(mailtoFrom));
  else
    snprintf(mailtoFrom, sizeof(mailtoFrom), "root@%s",
             httpGetHostname(NULL, line, sizeof(line)));

  strlcpy(mailtoSendmail, "/usr/sbin/sendmail", sizeof(mailtoSendmail));

  mailtoSMTPServer[0] = '\0';

  mailtoSubject[0] = '\0';

 /*
  * Try loading the config file...
  */

  if ((server_root = getenv("CUPS_SERVERROOT")) == NULL)
    server_root = CUPS_SERVERROOT;

  snprintf(line, sizeof(line), "%s/mailto.conf", server_root);

  if ((fp = cupsFileOpen(line, "r")) == NULL)
  {
    fprintf(stderr, "ERROR: Unable to open \"%s\" - %s\n", line,
            strerror(errno));
    return (1);
  }

  linenum = 0;

  while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
  {
    if (!value)
    {
      fprintf(stderr, "ERROR: No value found for %s directive on line %d!\n",
              line, linenum);
      cupsFileClose(fp);
      return (0);
    }

    if (!strcasecmp(line, "Cc"))
      strlcpy(mailtoCc, value, sizeof(mailtoCc));
    else if (!strcasecmp(line, "From"))
      strlcpy(mailtoFrom, value, sizeof(mailtoFrom));
    else if (!strcasecmp(line, "Sendmail"))
    {
      strlcpy(mailtoSendmail, value, sizeof(mailtoSendmail));
      mailtoSMTPServer[0] = '\0';
    }
    else if (!strcasecmp(line, "SMTPServer"))
    {
      mailtoSendmail[0] = '\0';
      strlcpy(mailtoSMTPServer, value, sizeof(mailtoSMTPServer));
    }
    else if (!strcasecmp(line, "Subject"))
      strlcpy(mailtoSubject, value, sizeof(mailtoSubject));
    else
    {
      fprintf(stderr,
              "ERROR: Unknown configuration directive \"%s\" on line %d!\n",
              line, linenum);
    }
  }

 /*
  * Close file and return...
  */

  cupsFileClose(fp);

  return (1);
}


/*
 * 'pipe_sendmail()' - Open a pipe to sendmail...
 */

cups_file_t *				/* O - CUPS file */
pipe_sendmail(const char *to)		/* I - To: address */
{
  cups_file_t	*fp;			/* CUPS file */
  int		pid;			/* Process ID */
  int		pipefds[2];		/* Pipe file descriptors */
  int		argc;			/* Number of arguments */
  char		*argv[100],		/* Argument array */
		line[1024],		/* Sendmail command + args */
		*lineptr;		/* Pointer into line */


 /*
  * First break the mailtoSendmail string into arguments...
  */

  strlcpy(line, mailtoSendmail, sizeof(line));
  argv[0] = line;
  argc    = 1;

  for (lineptr = strchr(line, ' '); lineptr; lineptr = strchr(lineptr, ' '))
  {
    while (*lineptr == ' ')
      *lineptr++ = '\0';

    if (*lineptr)
    {
     /*
      * Point to the next argument...
      */

      argv[argc ++] = lineptr;

     /*
      * Stop if we have too many...
      */

      if (argc >= (int)(sizeof(argv) / sizeof(argv[0]) - 2))
        break;
    }
  }

  argv[argc ++] = (char *)to;
  argv[argc]    = NULL;

 /*
  * Create the pipe...
  */

  if (pipe(pipefds))
  {
    perror("ERROR: Unable to create pipe");
    return (NULL);
  }

 /*
  * Then run the command...
  */

  if ((pid = fork()) == 0)
  {
   /*
    * Child goes here - redirect stdin to the input side of the pipe,
    * redirect stdout to stderr, and exec...
    */

    close(0);
    dup(pipefds[0]);

    close(1);
    dup(2);

    close(pipefds[0]);
    close(pipefds[1]);

    execvp(argv[0], argv);
    exit(errno);
  }
  else if (pid < 0)
  {
   /*
    * Unable to fork - error out...
    */

    perror("ERROR: Unable to fork command");

    close(pipefds[0]);
    close(pipefds[1]);

    return (NULL);
  }

 /*
  * Create a CUPS file using the output side of the pipe and close the
  * input side...
  */

  close(pipefds[0]);

  if ((fp = cupsFileOpenFd(pipefds[1], "w")) == NULL)
  {
    int	status;				/* Status of command */


    close(pipefds[1]);
    wait(&status);
  }

  return (fp);
}


/*
 * 'print_attributes()' - Print the attributes in a request...
 */

void
print_attributes(ipp_t *ipp,		/* I - IPP request */
                 int   indent)		/* I - Indentation */
{
  int			i;		/* Looping var */
  ipp_tag_t		group;		/* Current group */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_value_t		*val;		/* Current value */
  static const char * const tags[] =	/* Value/group tag strings */
			{
			  "reserved-00",
			  "operation-attributes-tag",
			  "job-attributes-tag",
			  "end-of-attributes-tag",
			  "printer-attributes-tag",
			  "unsupported-attributes-tag",
			  "subscription-attributes-tag",
			  "event-attributes-tag",
			  "reserved-08",
			  "reserved-09",
			  "reserved-0A",
			  "reserved-0B",
			  "reserved-0C",
			  "reserved-0D",
			  "reserved-0E",
			  "reserved-0F",
			  "unsupported",
			  "default",
			  "unknown",
			  "no-value",
			  "reserved-14",
			  "not-settable",
			  "delete-attr",
			  "admin-define",
			  "reserved-18",
			  "reserved-19",
			  "reserved-1A",
			  "reserved-1B",
			  "reserved-1C",
			  "reserved-1D",
			  "reserved-1E",
			  "reserved-1F",
			  "reserved-20",
			  "integer",
			  "boolean",
			  "enum",
			  "reserved-24",
			  "reserved-25",
			  "reserved-26",
			  "reserved-27",
			  "reserved-28",
			  "reserved-29",
			  "reserved-2a",
			  "reserved-2b",
			  "reserved-2c",
			  "reserved-2d",
			  "reserved-2e",
			  "reserved-2f",
			  "octetString",
			  "dateTime",
			  "resolution",
			  "rangeOfInteger",
			  "begCollection",
			  "textWithLanguage",
			  "nameWithLanguage",
			  "endCollection",
			  "reserved-38",
			  "reserved-39",
			  "reserved-3a",
			  "reserved-3b",
			  "reserved-3c",
			  "reserved-3d",
			  "reserved-3e",
			  "reserved-3f",
			  "reserved-40",
			  "textWithoutLanguage",
			  "nameWithoutLanguage",
			  "reserved-43",
			  "keyword",
			  "uri",
			  "uriScheme",
			  "charset",
			  "naturalLanguage",
			  "mimeMediaType",
			  "memberName"
			};


  for (group = IPP_TAG_ZERO, attr = ipp->attrs; attr; attr = attr->next)
  {
    if ((attr->group_tag == IPP_TAG_ZERO && indent <= 8) || !attr->name)
    {
      group = IPP_TAG_ZERO;
      fputc('\n', stderr);
      continue;
    }

    if (group != attr->group_tag)
    {
      group = attr->group_tag;

      fprintf(stderr, "DEBUG: %*s%s:\n\n", indent - 4, "", tags[group]);
    }

    fprintf(stderr, "DEBUG: %*s%s (", indent, "", attr->name);
    if (attr->num_values > 1)
      fputs("1setOf ", stderr);
    fprintf(stderr, "%s):", tags[attr->value_tag]);

    switch (attr->value_tag)
    {
      case IPP_TAG_ENUM :
      case IPP_TAG_INTEGER :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    fprintf(stderr, " %d", val->integer);
          fputc('\n', stderr);
          break;

      case IPP_TAG_BOOLEAN :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    fprintf(stderr, " %s", val->boolean ? "true" : "false");
          fputc('\n', stderr);
          break;

      case IPP_TAG_RANGE :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    fprintf(stderr, " %d-%d", val->range.lower, val->range.upper);
	  fputc('\n', stderr);
          break;

      case IPP_TAG_DATE :
          {
	    time_t	vtime;		/* Date/Time value */
	    struct tm	*vdate;		/* Date info */
	    char	vstring[256];	/* Formatted time */

	    for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    {
	      vtime = ippDateToTime(val->date);
	      vdate = localtime(&vtime);
	      strftime(vstring, sizeof(vstring), "%c", vdate);
	      fprintf(stderr, " (%s)", vstring);
	    }
          }
	  fputc('\n', stderr);
          break;

      case IPP_TAG_RESOLUTION :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    fprintf(stderr, " %dx%d%s", val->resolution.xres,
	            val->resolution.yres,
	            val->resolution.units == IPP_RES_PER_INCH ? "dpi" : "dpc");
	  fputc('\n', stderr);
          break;

      case IPP_TAG_STRING :
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
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    fprintf(stderr, " \"%s\"", val->string.text);
	  fputc('\n', stderr);
          break;

      case IPP_TAG_BEGIN_COLLECTION :
	  fputc('\n', stderr);

          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	  {
	    if (i)
	      fputc('\n', stderr);
	    print_attributes(val->collection, indent + 4);
	  }
          break;

      default :
          fprintf(stderr, "UNKNOWN (%d values)\n", attr->num_values);
          break;
    }
  }
}


/*
 * End of "$Id$".
 */
