/*
 * "$Id: main.c,v 1.57.2.15 2002/06/14 19:48:56 mike Exp $"
 *
 *   Scheduler main loop for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE" which should have been included with this file.  If this
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
 *   main()               - Main entry for the CUPS scheduler.
 *   CatchChildSignals()  - Catch SIGCHLD signals...
 *   IgnoreChildSignals() - Ignore SIGCHLD signals...
 *   sigchld_handler()    - Handle 'child' signals from old processes.
 *   sighup_handler()     - Handle 'hangup' signals to reconfigure the scheduler.
 *   sigterm_handler()    - Handle 'terminate' signals that stop the scheduler.
 *   usage()              - Show scheduler usage.
 */

/*
 * Include necessary headers...
 */

#define _MAIN_C_
#include "cupsd.h"
#include <sys/resource.h>
#include <syslog.h>

#if defined(HAVE_MALLOC_H) && defined(HAVE_MALLINFO)
#  include <malloc.h>
#endif /* HAVE_MALLOC_H && HAVE_MALLINFO */

#ifndef FD_SETSIZE
#  define FD_SETSIZE	1024
#endif /* !FD_SETSIZE */


/*
 * Local functions...
 */

static void	sigchld_handler(int sig);
static void	sighup_handler(int sig);
static void	sigterm_handler(int sig);
static void	usage(void);


/*
 * 'main()' - Main entry for the CUPS scheduler.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int			i;		/* Looping var */
  char			*opt;		/* Option character */
  int			fg;		/* Run in the foreground */
  fd_set		input,		/* Input set for select() */
			output;		/* Output set for select() */
  client_t		*con;		/* Current client */
  job_t			*job,		/* Current job */
			*next;		/* Next job */
  listener_t		*lis;		/* Current listener */
  time_t		activity;	/* Activity timer */
  time_t		senddoc_time;	/* Send-Document time */
#ifdef HAVE_MALLINFO
  time_t		mallinfo_time;	/* Malloc information time */
#endif /* HAVE_MALLINFO */
  struct timeval	timeout;	/* select() timeout */
  struct rlimit		limit;		/* Runtime limit */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction	action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */
#ifdef __sgi
  FILE			*fp;		/* Fake lpsched lock file */
#endif /* __sgi */


 /*
  * Check for command-line arguments...
  */

  fg = 0;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      for (opt = argv[i] + 1; *opt != '\0'; opt ++)
        switch (*opt)
	{
	  case 'c' : /* Configuration file */
	      i ++;
	      if (i >= argc)
	        usage();

              if (argv[i][0] == '/')
	      {
	       /*
	        * Absolute directory...
		*/

		strlcpy(ConfigurationFile, argv[i], sizeof(ConfigurationFile));
              }
	      else
	      {
	       /*
	        * Relative directory...
		*/

                getcwd(ConfigurationFile, sizeof(ConfigurationFile));
		strlcat(ConfigurationFile, "/", sizeof(ConfigurationFile));
		strlcat(ConfigurationFile, argv[i], sizeof(ConfigurationFile));
              }
	      break;

          case 'f' : /* Run in foreground... */
	      fg = 1;
	      break;

	  default : /* Unknown option */
              fprintf(stderr, "cupsd: Unknown option \'%c\' - aborting!\n", *opt);
	      usage();
	      break;
	}
    else
    {
      fprintf(stderr, "cupsd: Unknown argument \'%s\' - aborting!\n", argv[i]);
      usage();
    }

 /*
  * If the user hasn't specified "-f", run in the background...
  */

  if (!fg)
  {
    if (fork() > 0)
      return (0);

   /*
    * Make sure we aren't tying up any filesystems...
    */

    chdir("/");

#ifndef DEBUG
   /*
    * Disable core dumps...
    */

    getrlimit(RLIMIT_CORE, &limit);
    limit.rlim_cur = 0;
    setrlimit(RLIMIT_CORE, &limit);

   /*
    * Disconnect from the controlling terminal...
    */

    close(0);
    close(1);
    close(2);

    setsid();
#endif /* DEBUG */
  }

 /*
  * Set the timezone info...
  */

  if (getenv("TZ") != NULL)
    snprintf(TZ, sizeof(TZ), "TZ=%s", getenv("TZ"));

  tzset();

 /*
  * Set the maximum number of files...
  */

  getrlimit(RLIMIT_NOFILE, &limit);
  if (limit.rlim_max > FD_SETSIZE)	/* Can't exceed size of FD set! */
    MaxFDs = FD_SETSIZE;
  else
    MaxFDs = limit.rlim_max;

  limit.rlim_cur = MaxFDs;
  setrlimit(RLIMIT_NOFILE, &limit);

 /*
  * Catch hangup and child signals and ignore broken pipes...
  */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  if (RunAsUser)
    sigset(SIGHUP, sigterm_handler);
  else
    sigset(SIGHUP, sighup_handler);
  sigset(SIGPIPE, SIG_IGN);
  sigset(SIGTERM, sigterm_handler);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGHUP);

  if (RunAsUser)
    action.sa_handler = sigterm_handler;
  else
    action.sa_handler = sighup_handler;

  sigaction(SIGHUP, &action, NULL);

  sigemptyset(&action.sa_mask);
  action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &action, NULL);

  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGTERM);
  action.sa_handler = sigterm_handler;
  sigaction(SIGTERM, &action, NULL);
#else
  if (RunAsUser)
    signal(SIGHUP, sigterm_handler);
  else
    signal(SIGHUP, sighup_handler);

  signal(SIGPIPE, SIG_IGN);
  signal(SIGTERM, sigterm_handler);
#endif /* HAVE_SIGSET */

 /*
  * Read configuration...
  */

  if (!ReadConfiguration())
  {
    syslog(LOG_LPR, "Unable to read configuration file \'%s\' - exiting!",
           ConfigurationFile);
    return (1);
  }

#ifdef __sgi
 /*
  * Try to create a fake lpsched lock file if one is not already there.
  * Some Adobe applications need it under IRIX in order to enable
  * printing...
  */

  if ((fp = fopen("/var/spool/lp/SCHEDLOCK", "a")) == NULL)
  {
    syslog(LOG_LPR, "Unable to create fake lpsched lock file "
                    "\"/var/spool/lp/SCHEDLOCK\"\' - %s!",
           strerror(errno));
  }
  else
  {
    fclose(fp);

    chmod("/var/spool/lp/SCHEDLOCK", 0644);
    chown("/var/spool/lp/SCHEDLOCK", User, Group);
  }
#endif /* __sgi */

 /*
  * Initialize authentication certificates...
  */

  InitCerts();

 /*
  * Load all the jobs...
  */

  LoadAllJobs();

 /*
  * Loop forever...
  */

  senddoc_time = time(NULL);

#ifdef HAVE_MALLINFO
  mallinfo_time = 0;
#endif /* HAVE_MALLINFO */

  for (;;)
  {
   /*
    * Check if we need to load the server configuration file...
    */

    if (NeedReload)
    {
      if (NumClients > 0)
      {
        for (i = NumClients, con = Clients; i > 0; i --, con ++)
	  if (con->http.state == HTTP_WAITING)
	  {
	    CloseClient(con);
	    con --;
	  }
	  else
	    con->http.keep_alive = HTTP_KEEPALIVE_OFF;

        PauseListening();
      }
      else if (!ReadConfiguration())
      {
        syslog(LOG_LPR, "Unable to read configuration file \'%s\' - exiting!",
	       ConfigurationFile);
        break;
      }
    }

   /*
    * Check for available input or ready output.  If select() returns
    * 0 or -1, something bad happened and we should exit immediately.
    *
    * Note that we at least have one listening socket open at all
    * times.
    */

    input  = InputSet;
    output = OutputSet;

    for (i = NumClients, con = Clients; i > 0; i --, con ++)
      if (con->http.used > 0)
        break;

    if (i)
    {
      timeout.tv_sec  = 0;
      timeout.tv_usec = 0;
    }
    else
    {
     /*
      * If we have no pending data from a client, see when we really
      * need to wake up...
      */

      timeout.tv_sec  = 1;
      timeout.tv_usec = 0;
    }

    if ((i = select(MaxFDs, &input, &output, NULL, &timeout)) < 0)
    {
      char	s[16384],	/* String buffer */
		*sptr;		/* Pointer into buffer */
      int	slen;		/* Length of string buffer */


     /*
      * Got an error from select!
      */

      if (errno == EINTR)	/* Just interrupted by a signal */
        continue;

     /*
      * Log all sorts of debug info to help track down the problem.
      */

      LogMessage(L_EMERG, "select() failed - %s!", strerror(errno));

      strcpy(s, "InputSet =");
      slen = 10;
      sptr = s + 10;

      for (i = 0; i < MaxFDs; i ++)
        if (FD_ISSET(i, &InputSet))
	{
          snprintf(sptr, sizeof(s) - slen, " %d", i);
	  slen += strlen(sptr);
	  sptr += strlen(sptr);
	}

      LogMessage(L_EMERG, s);

      strcpy(s, "OutputSet =");
      slen = 11;
      sptr = s + 11;

      for (i = 0; i < MaxFDs; i ++)
        if (FD_ISSET(i, &OutputSet))
	{
          snprintf(sptr, sizeof(s) - slen, " %d", i);
	  slen += strlen(sptr);
	  sptr += strlen(sptr);
	}

      LogMessage(L_EMERG, s);

      for (i = 0, con = Clients; i < NumClients; i ++, con ++)
        LogMessage(L_EMERG, "Clients[%d] = %d, file = %d, state = %d",
	           i, con->http.fd, con->file, con->http.state);

      for (i = 0, lis = Listeners; i < NumListeners; i ++, lis ++)
        LogMessage(L_EMERG, "Listeners[%d] = %d", i, lis->fd);

      LogMessage(L_EMERG, "BrowseSocket = %d", BrowseSocket);

      for (job = Jobs; job != NULL; job = job->next)
        LogMessage(L_EMERG, "Jobs[%d] = %d < [%d %d] > [%d %d]",
	           job->id, job->status_pipe,
		   job->print_pipes[0], job->print_pipes[1],
		   job->back_pipes[0], job->back_pipes[1]);

      break;
    }

    for (i = NumListeners, lis = Listeners; i > 0; i --, lis ++)
      if (FD_ISSET(lis->fd, &input))
        AcceptClient(lis);

    for (i = NumClients, con = Clients; i > 0; i --, con ++)
    {
     /*
      * Process the input buffer...
      */

      if (FD_ISSET(con->http.fd, &input) || con->http.used)
        if (!ReadClient(con))
	{
	  con --;
	  continue;
	}

     /*
      * Write data as needed...
      */

      if (FD_ISSET(con->http.fd, &output) &&
          (!con->pipe_pid || FD_ISSET(con->file, &input)))
        if (!WriteClient(con))
	{
	  con --;
	  continue;
	}

     /*
      * Check the activity and close old clients...
      */

      activity = time(NULL) - Timeout;
      if (con->http.activity < activity && !con->pipe_pid)
      {
        CloseClient(con);
        con --;
        continue;
      }
    }

   /*
    * Check for status info from job filters...
    */

    for (job = Jobs; job != NULL; job = next)
    {
      next = job->next;

      if (job->status_pipe >= 0 && FD_ISSET(job->status_pipe, &input))
      {
       /*
        * Clear the input bit to avoid updating the next job
	* using the same status pipe file descriptor...
	*/

        FD_CLR(job->status_pipe, &input);

       /*
        * Read any status messages from the filters...
	*/

        UpdateJob(job);
      }
    }

   /*
    * Update the browse list as needed...
    */

    if (Browsing && BrowseProtocols)
    {
      if (BrowseSocket >= 0 && FD_ISSET(BrowseSocket, &input))
        UpdateCUPSBrowse();

      if (PollPipe >= 0 && FD_ISSET(PollPipe, &input))
        UpdatePolling();

#ifdef HAVE_LIBSLP
      if ((BrowseProtocols & BROWSE_SLP) && BrowseSLPRefresh <= time(NULL))
        UpdateSLPBrowse();
#endif /* HAVE_LIBSLP */

      SendBrowseList();
    }

   /*
    * Update any pending multi-file documents...
    */

    if ((time(NULL) - senddoc_time) >= 10)
    {
      CheckJobs();
      senddoc_time = time(NULL);
    }

#ifdef HAVE_MALLINFO
   /*
    * Log memory usage every minute...
    */

    if ((time(NULL) - mallinfo_time) >= 60 && LogLevel >= L_DEBUG)
    {
      struct mallinfo mem;	/* Malloc information */


      mem = mallinfo();
      LogMessage(L_DEBUG, "mallinfo: arena = %d, used = %d, free = %d\n",
                 mem.arena, mem.usmblks + mem.uordblks,
		 mem.fsmblks + mem.fordblks);
      mallinfo_time = time(NULL);
    }
#endif /* HAVE_MALLINFO */

   /*
    * Update the root certificate once every 5 minutes...
    */

    if ((time(NULL) - RootCertTime) >= RootCertDuration && RootCertDuration)
    {
     /*
      * Update the root certificate...
      */

      DeleteCert(0);
      AddCert(0, "root");
    }
  }

 /*
  * If we get here something very bad happened and we need to exit
  * immediately.
  */

  StopBrowsing();
  StopAllJobs();
  DeleteAllCerts();
  CloseAllClients();
  StopListening();

  return (1);
}


/*
 * 'CatchChildSignals()' - Catch SIGCHLD signals...
 */

void
CatchChildSignals(void)
{
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction	action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGCHLD, sigchld_handler);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGCHLD);
  action.sa_handler = sigchld_handler;
  sigaction(SIGCHLD, &action, NULL);
#else
  signal(SIGCLD, sigchld_handler);	/* No, SIGCLD isn't a typo... */
#endif /* HAVE_SIGSET */
}


/*
 * 'IgnoreChildSignals()' - Ignore SIGCHLD signals...
 */

void
IgnoreChildSignals(void)
{
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction	action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGCHLD, SIG_IGN);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGCHLD);
  action.sa_handler = SIG_IGN;
  sigaction(SIGCHLD, &action, NULL);
#else
  signal(SIGCLD, SIG_IGN);	/* No, SIGCLD isn't a typo... */
#endif /* HAVE_SIGSET */
}


/*
 * 'sigchld_handler()' - Handle 'child' signals from old processes.
 */

static void
sigchld_handler(int sig)	/* I - Signal number */
{
  int		status;		/* Exit status of child */
  int		pid;		/* Process ID of child */
  job_t		*job;		/* Current job */
  int		i;		/* Looping var */


  (void)sig;

#ifdef HAVE_WAITPID
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
#elif defined(HAVE_WAIT3)
  while ((pid = wait3(&status, WNOHANG, NULL)) > 0)
#else
  if ((pid = wait(&status)) > 0)
#endif /* HAVE_WAITPID */
  {
    DEBUG_printf(("sigcld_handler: pid = %d, status = %d\n", pid, status));

   /*
    * Delete certificates for CGI processes...
    */

    if (pid)
      DeleteCert(pid);

   /*
    * Ignore SIGTERM errors - that comes when a job is cancelled...
    */

    if (status == SIGTERM)
      status = 0;

    if (status)
    {
      if (WIFEXITED(status))
	LogMessage(L_ERROR, "PID %d stopped with status %d!", pid,
	           WEXITSTATUS(status));
      else
	LogMessage(L_ERROR, "PID %d crashed on signal %d!", pid,
	           WTERMSIG(status));
    }

    for (job = Jobs; job != NULL; job = job->next)
      if (job->state->values[0].integer == IPP_JOB_PROCESSING)
      {
	for (i = 0; job->filters[i]; i ++)
          if (job->filters[i] == pid)
	    break;

	if (job->filters[i] || job->backend == pid)
	{
	 /*
          * OK, this process has gone away; what's left?
	  */

          if (job->filters[i])
	    job->filters[i] = -pid;
	  else
	    job->backend = -pid;

          if (status && job->status >= 0)
	  {
	   /*
	    * An error occurred; save the exit status so we know to stop
	    * the printer or cancel the job when all of the filters finish...
	    *
	    * A negative status indicates that the backend failed and the
	    * printer needs to be stopped.
	    */

            if (job->filters[i])
 	      job->status = status;	/* Filter failed */
	    else
 	      job->status = -status;	/* Backend failed */
	  }
	  break;
	}
      }
  }

#ifdef HAVE_SIGSET
  sigset(SIGCHLD, sigchld_handler);
#elif !defined(HAVE_SIGACTION)
  signal(SIGCLD, sigchld_handler);
#endif /* HAVE_SIGSET */
}


/*
 * 'sighup_handler()' - Handle 'hangup' signals to reconfigure the scheduler.
 */

static void
sighup_handler(int sig)	/* I - Signal number */
{
  (void)sig;

  NeedReload = TRUE;

#ifdef HAVE_SIGSET
  sigset(SIGHUP, sighup_handler);
#elif !defined(HAVE_SIGACTION)
  signal(SIGHUP, sighup_handler);
#endif /* HAVE_SIGSET */
}


/*
 * 'sigterm_handler()' - Handle 'terminate' signals that stop the scheduler.
 */

static void
sigterm_handler(int sig)
{
#ifdef __sgi
  struct stat	statbuf;		/* Needed for checking lpsched FIFO */
#endif /* __sgi */


  (void)sig;	/* remove compiler warnings... */

 /*
  * Log an error...
  */

  LogMessage(L_ERROR, "Scheduler shutting down due to SIGTERM.");

 /*
  * Close all network clients and stop all jobs...
  */

  CloseAllClients();
  StopListening();
  StopPolling();
  StopBrowsing();

  if (Clients != NULL)
    free(Clients);

  StopAllJobs();

  if (AccessFile != NULL)
    fclose(AccessFile);

  if (ErrorFile != NULL)
    fclose(ErrorFile);

  if (PageFile != NULL)
    fclose(PageFile);

  DeleteAllLocations();

  DeleteAllClasses();

  if (Devices)
    ippDelete(Devices);

  if (PPDs)
    ippDelete(PPDs);

  DeleteAllPrinters();

  if (MimeDatabase != NULL)
    mimeDelete(MimeDatabase);

#ifdef __sgi
 /*
  * Remove the fake IRIX lpsched lock file, but only if the existing
  * file is not a FIFO which indicates that the real IRIX lpsched is
  * running...
  */

  if (!stat("/var/spool/lp/FIFO", &statbuf))
    if (!S_ISFIFO(statbuf.st_mode))
      unlink("/var/spool/lp/SCHEDLOCK");
#endif /* __sgi */

  exit(1);
}


/*
 * 'usage()' - Show scheduler usage.
 */

static void
usage(void)
{
  fputs("Usage: cupsd [-c config-file] [-f]\n", stderr);
  exit(1);
}


/*
 * End of "$Id: main.c,v 1.57.2.15 2002/06/14 19:48:56 mike Exp $".
 */
