/*
 * "$Id$"
 *
 *   Scheduler main loop for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main()                    - Main entry for the CUPS scheduler.
 *   cupsdClosePipe()          - Close a pipe as necessary.
 *   cupsdOpenPipe()           - Create a pipe which is closed on exec.
 *   cupsdCatchChildSignals()  - Catch SIGCHLD signals...
 *   cupsdHoldSignals()        - Hold child and termination signals.
 *   cupsdIgnoreChildSignals() - Ignore SIGCHLD signals...
 *   cupsdReleaseSignals()     - Release signals for delivery.
 *   cupsdSetString()          - Set a string value.
 *   cupsdSetStringf()         - Set a formatted string value.
 *   parent_handler()          - Catch USR1/CHLD signals...
 *   process_children()        - Process all dead children...
 *   sigchld_handler()         - Handle 'child' signals from old processes.
 *   sighup_handler()          - Handle 'hangup' signals to reconfigure the
 *                               scheduler.
 *   sigterm_handler()         - Handle 'terminate' signals that stop the
 *                               scheduler.
 *   select_timeout()          - Calculate the select timeout value.
 *   usage()                   - Show scheduler usage.
 */

/*
 * Include necessary headers...
 */

#define _MAIN_C_
#include "cupsd.h"
#include <sys/resource.h>
#include <syslog.h>
#include <grp.h>

#if defined(HAVE_MALLOC_H) && defined(HAVE_MALLINFO)
#  include <malloc.h>
#endif /* HAVE_MALLOC_H && HAVE_MALLINFO */


/*
 * Local functions...
 */

static void	parent_handler(int sig);
static void	process_children(void);
static void	sigchld_handler(int sig);
static void	sighup_handler(int sig);
static void	sigterm_handler(int sig);
static long	select_timeout(int fds);
static void	usage(void);


/*
 * Local globals...
 */

static int	parent_signal = 0;	/* Set to signal number from child */
static int	holdcount = 0;		/* Number of times "hold" was called */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
static sigset_t	holdmask;		/* Old POSIX signal mask */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */
static int	dead_children = 0;	/* Dead children? */
static int	stop_scheduler = 0;	/* Should the scheduler stop? */


/*
 * 'main()' - Main entry for the CUPS scheduler.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int			i;		/* Looping var */
  char			*opt;		/* Option character */
  int			fg;		/* Run in the foreground */
  int			fds;		/* Number of ready descriptors select returns */
  fd_set		*input,		/* Input set for select() */
			*output;	/* Output set for select() */
  cupsd_client_t	*con;		/* Current client */
  cupsd_job_t		*job;		/* Current job */
  cupsd_listener_t	*lis;		/* Current listener */
  time_t		current_time,	/* Current time */
			activity,	/* Activity timer */
			browse_time,	/* Next browse send time */
			senddoc_time,	/* Send-Document time */
			expire_time;	/* Subscription expire time */
#ifdef HAVE_MALLINFO
  time_t		mallinfo_time;	/* Malloc information time */
#endif /* HAVE_MALLINFO */
  struct timeval	timeout;	/* select() timeout */
  struct rlimit		limit;		/* Runtime limit */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction	action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */
#ifdef __sgi
  cups_file_t		*fp;		/* Fake lpsched lock file */
  struct stat		statbuf;	/* Needed for checking lpsched FIFO */
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

		cupsdSetString(&ConfigurationFile, argv[i]);
              }
	      else
	      {
	       /*
	        * Relative directory...
		*/

                char current[1024];	/* Current directory */


                getcwd(current, sizeof(current));
		cupsdSetStringf(&ConfigurationFile, "%s/%s", current, argv[i]);
              }
	      break;

          case 'f' : /* Run in foreground... */
	      fg = 1;
	      break;

          case 'F' : /* Run in foreground, but still disconnect from terminal... */
	      fg = -1;
	      break;

	  default : /* Unknown option */
              fprintf(stderr, "cupsd: Unknown option \'%c\' - aborting!\n",
	              *opt);
	      usage();
	      break;
	}
    else
    {
      fprintf(stderr, "cupsd: Unknown argument \'%s\' - aborting!\n", argv[i]);
      usage();
    }

  if (!ConfigurationFile)
    cupsdSetString(&ConfigurationFile, CUPS_SERVERROOT "/cupsd.conf");

 /*
  * If the user hasn't specified "-f", run in the background...
  */

  if (!fg)
  {
   /*
    * Setup signal handlers for the parent...
    */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
    sigset(SIGUSR1, parent_handler);
    sigset(SIGCHLD, parent_handler);

    sigset(SIGHUP, SIG_IGN);
#elif defined(HAVE_SIGACTION)
    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGUSR1);
    action.sa_handler = parent_handler;
    sigaction(SIGUSR1, &action, NULL);
    sigaction(SIGCHLD, &action, NULL);

    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_IGN;
    sigaction(SIGHUP, &action, NULL);
#else
    signal(SIGUSR1, parent_handler);
    signal(SIGCLD, parent_handler);

    signal(SIGHUP, SIG_IGN);
#endif /* HAVE_SIGSET */

    if (fork() > 0)
    {
     /*
      * OK, wait for the child to startup and send us SIGUSR1 or to crash
      * and the OS send us SIGCHLD...  We also need to ignore SIGHUP which
      * might be sent by the init script to restart the scheduler...
      */

      for (; parent_signal == 0;)
        sleep(1);

      if (parent_signal == SIGUSR1)
        return (0);

      if (wait(&i) < 0)
      {
        perror("cupsd");
	return (1);
      }
      else if (WIFEXITED(i))
      {
        fprintf(stderr, "cupsd: Child exited with status %d!\n", WEXITSTATUS(i));
	return (2);
      }
      else
      {
        fprintf(stderr, "cupsd: Child exited on signal %d!\n", WTERMSIG(i));
	return (3);
      }
    }
  }

  if (fg < 1)
  {
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

    setsid();

   /*
    * Close all open files...
    */

    getrlimit(RLIMIT_NOFILE, &limit);

    for (i = 0; i < limit.rlim_cur; i ++)
      close(i);
#endif /* DEBUG */
  }

 /*
  * Set the timezone info...
  */

  tzset();

#ifdef LC_TIME
  setlocale(LC_TIME, "");
#endif /* LC_TIME */

 /*
  * Set the maximum number of files...
  */

  getrlimit(RLIMIT_NOFILE, &limit);

  if (limit.rlim_max > CUPS_MAX_FDS)
    MaxFDs = CUPS_MAX_FDS;
  else
    MaxFDs = limit.rlim_max;

  limit.rlim_cur = MaxFDs;

  setrlimit(RLIMIT_NOFILE, &limit);

 /*
  * Allocate memory for the input and output sets...
  */

  SetSize = (MaxFDs + 31) / 8 + 4;
  if (SetSize < sizeof(fd_set))
    SetSize = sizeof(fd_set);

  InputSet  = (fd_set *)calloc(1, SetSize);
  OutputSet = (fd_set *)calloc(1, SetSize);
  input     = (fd_set *)calloc(1, SetSize);
  output    = (fd_set *)calloc(1, SetSize);

  if (InputSet == NULL || OutputSet == NULL || input == NULL || output == NULL)
  {
    syslog(LOG_LPR, "Unable to allocate memory for select() sets - exiting!");
    return (1);
  }

 /*
  * Read configuration...
  */

  if (!cupsdReadConfiguration())
  {
    syslog(LOG_LPR, "Unable to read configuration file \'%s\' - exiting!",
           ConfigurationFile);
    return (1);
  }

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
  sigaddset(&action.sa_mask, SIGCHLD);
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

#ifdef __sgi
 /*
  * Try to create a fake lpsched lock file if one is not already there.
  * Some Adobe applications need it under IRIX in order to enable
  * printing...
  */

  if ((fp = cupsFileOpen("/var/spool/lp/SCHEDLOCK", "w")) == NULL)
  {
    syslog(LOG_LPR, "Unable to create fake lpsched lock file "
                    "\"/var/spool/lp/SCHEDLOCK\"\' - %s!",
           strerror(errno));
  }
  else
  {
    fchmod(cupsFileNumber(fp), 0644);
    fchown(cupsFileNumber(fp), User, Group);

    cupsFileClose(fp);
  }
#endif /* __sgi */

 /*
  * Initialize authentication certificates...
  */

  cupsdInitCerts();

 /*
  * If we are running in the background, signal the parent process that
  * we are up and running...
  */

  if (!fg)
  {
   /*
    * Send a signal to the parent process, but only if the parent is
    * not PID 1 (init).  This avoids accidentally shutting down the
    * system on OpenBSD if you CTRL-C the server before it is up...
    */

    i = getppid();	/* Save parent PID to avoid race condition */

    if (i != 1)
      kill(i, SIGUSR1);
  }

 /*
  * If the administrator has configured the server to run as an unpriviledged
  * user, change to that user now...
  */

  if (RunAsUser)
  {
    setgid(Group);
    setgroups(1, &Group);
    setuid(User);
  }

 /*
  * Catch signals...
  */

  cupsdCatchChildSignals();

 /*
  * Start any pending print jobs...
  */

  cupsdCheckJobs();

 /*
  * Loop forever...
  */

#ifdef HAVE_MALLINFO
  mallinfo_time = 0;
#endif /* HAVE_MALLINFO */
  browse_time   = time(NULL);
  senddoc_time  = time(NULL);
  expire_time   = time(NULL);
  fds           = 1;

  while (!stop_scheduler)
  {
#ifdef DEBUG
    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "main: Top of loop, dead_children=%d, NeedReload=%d",
                    dead_children, NeedReload);
#endif /* DEBUG */

   /*
    * Check if there are dead children to handle...
    */

    if (dead_children)
      process_children();

   /*
    * Check if we need to load the server configuration file...
    */

    if (NeedReload)
    {
     /*
      * Close any idle clients...
      */

      if (NumClients > 0)
      {
        for (i = NumClients, con = Clients; i > 0; i --, con ++)
	  if (con->http.state == HTTP_WAITING)
	  {
	    cupsdCloseClient(con);
	    con --;
	  }
	  else
	    con->http.keep_alive = HTTP_KEEPALIVE_OFF;

        cupsdPauseListening();
      }

     /*
      * Check for any active jobs...
      */

      for (job = (cupsd_job_t *)cupsArrayFirst(ActiveJobs);
	   job;
	   job = (cupsd_job_t *)cupsArrayNext(ActiveJobs))
        if (job->state->values[0].integer == IPP_JOB_PROCESSING)
	  break;

     /*
      * Restart if all clients are closed and all jobs finished, or
      * if the reload timeout has elapsed...
      */

      if ((NumClients == 0 && (!job || NeedReload != RELOAD_ALL)) ||
          (time(NULL) - ReloadTime) >= ReloadTimeout)
      {
        if (!cupsdReadConfiguration())
        {
          syslog(LOG_LPR, "Unable to read configuration file \'%s\' - exiting!",
		 ConfigurationFile);
          break;
	}
      }
    }

   /*
    * Check for available input or ready output.  If select() returns
    * 0 or -1, something bad happened and we should exit immediately.
    *
    * Note that we at least have one listening socket open at all
    * times.
    */

    memcpy(input, InputSet, SetSize);
    memcpy(output, OutputSet, SetSize);

    timeout.tv_sec  = select_timeout(fds);
    timeout.tv_usec = 0;

    if ((fds = select(MaxFDs, input, output, NULL, &timeout)) < 0)
    {
      char	s[16384],		/* String buffer */
		*sptr;			/* Pointer into buffer */
      int	slen;			/* Length of string buffer */


     /*
      * Got an error from select!
      */

      if (errno == EINTR)	/* Just interrupted by a signal */
        continue;

     /*
      * Log all sorts of debug info to help track down the problem.
      */

      cupsdLogMessage(CUPSD_LOG_EMERG, "select() failed - %s!",
                      strerror(errno));

      strcpy(s, "InputSet =");
      slen = 10;
      sptr = s + 10;

      for (i = 0; i < MaxFDs; i ++)
        if (FD_ISSET(i, InputSet))
	{
          snprintf(sptr, sizeof(s) - slen, " %d", i);
	  slen += strlen(sptr);
	  sptr += strlen(sptr);
	}

      cupsdLogMessage(CUPSD_LOG_EMERG, s);

      strcpy(s, "OutputSet =");
      slen = 11;
      sptr = s + 11;

      for (i = 0; i < MaxFDs; i ++)
        if (FD_ISSET(i, OutputSet))
	{
          snprintf(sptr, sizeof(s) - slen, " %d", i);
	  slen += strlen(sptr);
	  sptr += strlen(sptr);
	}

      cupsdLogMessage(CUPSD_LOG_EMERG, s);

      for (i = 0, con = Clients; i < NumClients; i ++, con ++)
        cupsdLogMessage(CUPSD_LOG_EMERG,
	                "Clients[%d] = %d, file = %d, state = %d",
	                i, con->http.fd, con->file, con->http.state);

      for (i = 0, lis = Listeners; i < NumListeners; i ++, lis ++)
        cupsdLogMessage(CUPSD_LOG_EMERG, "Listeners[%d] = %d", i, lis->fd);

      cupsdLogMessage(CUPSD_LOG_EMERG, "BrowseSocket = %d", BrowseSocket);

      for (job = (cupsd_job_t *)cupsArrayFirst(ActiveJobs);
	   job;
	   job = (cupsd_job_t *)cupsArrayNext(ActiveJobs))
        cupsdLogMessage(CUPSD_LOG_EMERG, "Jobs[%d] = %d < [%d %d] > [%d %d]",
	        	job->id,
			job->status_buffer ? job->status_buffer->fd : -1,
			job->print_pipes[0], job->print_pipes[1],
			job->back_pipes[0], job->back_pipes[1]);
      break;
    }

    current_time = time(NULL);

   /*
    * Check for status info from job filters...
    */

    for (job = (cupsd_job_t *)cupsArrayFirst(ActiveJobs);
	 job;
	 job = (cupsd_job_t *)cupsArrayNext(ActiveJobs))
      if (job->status_buffer && FD_ISSET(job->status_buffer->fd, input))
      {
       /*
        * Clear the input bit to avoid updating the next job
	* using the same status pipe file descriptor...
	*/

        FD_CLR(job->status_buffer->fd, input);

       /*
        * Read any status messages from the filters...
	*/

        cupsdUpdateJob(job);
      }

   /*
    * Update CGI messages as needed...
    */

    if (CGIPipes[0] >= 0 && FD_ISSET(CGIPipes[0], input))
      cupsdUpdateCGI();

   /*
    * Update notifier messages as needed...
    */

    if (NotifierPipes[0] >= 0 && FD_ISSET(NotifierPipes[0], input))
      cupsdUpdateNotifierStatus();

   /*
    * Expire subscriptions as needed...
    */

    if (cupsArrayCount(Subscriptions) > 0 && current_time > expire_time)
    {
      cupsdExpireSubscriptions(NULL, NULL);

      expire_time = current_time;
    }

   /*
    * Update the browse list as needed...
    */

    if (Browsing && (BrowseLocalProtocols | BrowseRemoteProtocols))
    {
      if (BrowseSocket >= 0 && FD_ISSET(BrowseSocket, input))
        cupsdUpdateCUPSBrowse();

      if (PollPipe >= 0 && FD_ISSET(PollPipe, input))
        cupsdUpdatePolling();

#ifdef HAVE_LIBSLP
      if (((BrowseLocalProtocols | BrowseRemoteProtocols) & BROWSE_SLP) &&
          BrowseSLPRefresh <= current_time)
        cupsdUpdateSLPBrowse();
#endif /* HAVE_LIBSLP */

      if (current_time > browse_time)
      {
        cupsdSendBrowseList();
	browse_time = current_time;
      }
    }

   /*
    * Check for new connections on the "listen" sockets...
    */

    for (i = NumListeners, lis = Listeners; i > 0; i --, lis ++)
      if (lis->fd >= 0 && FD_ISSET(lis->fd, input))
      {
        FD_CLR(lis->fd, input);
        cupsdAcceptClient(lis);
      }

   /*
    * Check for new data on the client sockets...
    */

    for (i = NumClients, con = Clients; i > 0; i --, con ++)
    {
     /*
      * Process the input buffer...
      */

      if (FD_ISSET(con->http.fd, input) || con->http.used)
      {
        FD_CLR(con->http.fd, input);

        if (!cupsdReadClient(con))
	{
	  if (con->pipe_pid)
	    FD_CLR(con->file, input);

	  con --;
	  continue;
	}
      }

     /*
      * Write data as needed...
      */

      if (con->pipe_pid && FD_ISSET(con->file, input))
      {
       /*
        * Keep track of pending input from the file/pipe separately
	* so that we don't needlessly spin on select() when the web
	* client is not ready to receive data...
	*/

	FD_CLR(con->file, input);
        con->file_ready = 1;

#ifdef DEBUG
        cupsdLogMessage(CUPSD_LOG_DEBUG2, "main: Data ready file %d!",
	                con->file);
#endif /* DEBUG */

	if (!FD_ISSET(con->http.fd, output))
	{
	  cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                  "main: Removing fd %d from InputSet...", con->file);
	  FD_CLR(con->file, InputSet);
	}
      }

      if (FD_ISSET(con->http.fd, output))
      {
        FD_CLR(con->http.fd, output);

	if (!con->pipe_pid || con->file_ready)
          if (!cupsdWriteClient(con))
	  {
	    con --;
	    continue;
	  }
      }

     /*
      * Check the activity and close old clients...
      */

      activity = current_time - Timeout;
      if (con->http.activity < activity && !con->pipe_pid)
      {
        cupsdLogMessage(CUPSD_LOG_DEBUG,
	                "Closing client %d after %d seconds of inactivity...",
	                con->http.fd, Timeout);

        cupsdCloseClient(con);
        con --;
        continue;
      }
    }

   /*
    * Update any pending multi-file documents...
    */

    if ((current_time - senddoc_time) >= 10)
    {
      cupsdCheckJobs();
      senddoc_time = current_time;
    }

#ifdef HAVE_MALLINFO
   /*
    * Log memory usage every minute...
    */

    if ((current_time - mallinfo_time) >= 60 && LogLevel >= CUPSD_LOG_DEBUG)
    {
      struct mallinfo mem;		/* Malloc information */


      mem = mallinfo();
      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "mallinfo: arena = %d, used = %d, free = %d\n",
                      mem.arena, mem.usmblks + mem.uordblks,
		      mem.fsmblks + mem.fordblks);
      mallinfo_time = current_time;
    }
#endif /* HAVE_MALLINFO */

   /*
    * Update the root certificate once every 5 minutes...
    */

    if ((current_time - RootCertTime) >= RootCertDuration && RootCertDuration &&
        !RunUser)
    {
     /*
      * Update the root certificate...
      */

      cupsdDeleteCert(0);
      cupsdAddCert(0, "root");
    }
  }

 /*
  * Log a message based on what happened...
  */

  if (stop_scheduler)
    cupsdLogMessage(CUPSD_LOG_INFO, "Scheduler shutting down normally.");
  else
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Scheduler shutting down due to program error.");

 /*
  * Close all network clients and stop all jobs...
  */

  cupsdStopServer();

  cupsdStopAllJobs();

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

 /*
  * Free memory used by FD sets and return...
  */

  free(InputSet);
  free(OutputSet);
  free(input);
  free(output);

  return (!stop_scheduler);
}


/*
 * 'cupsdClosePipe()' - Close a pipe as necessary.
 */

void
cupsdClosePipe(int *fds)		/* I - Pipe file descriptors (2) */
{
 /*
  * Close file descriptors as needed...
  */

  if (fds[0] >= 0)
  {
    close(fds[0]);
    fds[0] = -1;
  }

  if (fds[1] >= 0)
  {
    close(fds[1]);
    fds[1] = -1;
  }
}


/*
 * 'cupsdOpenPipe()' - Create a pipe which is closed on exec.
 */

int					/* O - 0 on success, -1 on error */
cupsdOpenPipe(int *fds)			/* O - Pipe file descriptors (2) */
{
 /*
  * Create the pipe...
  */

  if (pipe(fds))
    return (-1);

 /*
  * Set the "close on exec" flag on each end of the pipe...
  */

  if (fcntl(fds[0], F_SETFD, fcntl(fds[0], F_GETFD) | FD_CLOEXEC))
  {
    close(fds[0]);
    close(fds[1]);
    return (-1);
  }

  if (fcntl(fds[1], F_SETFD, fcntl(fds[1], F_GETFD) | FD_CLOEXEC))
  {
    close(fds[0]);
    close(fds[1]);
    return (-1);
  }

 /*
  * Return 0 indicating success...
  */

  return (0);
}


/*
 * 'cupsdCatchChildSignals()' - Catch SIGCHLD signals...
 */

void
cupsdCatchChildSignals(void)
{
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction	action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGCHLD, sigchld_handler);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGTERM);
  sigaddset(&action.sa_mask, SIGCHLD);
  action.sa_handler = sigchld_handler;
  sigaction(SIGCHLD, &action, NULL);
#else
  signal(SIGCLD, sigchld_handler);	/* No, SIGCLD isn't a typo... */
#endif /* HAVE_SIGSET */
}


/*
 * 'cupsdClearString()' - Clear a string.
 */

void
cupsdClearString(char **s)		/* O - String value */
{
  if (s && *s)
  {
    free(*s);
    *s = NULL;
  }
}


/*
 * 'cupsdHoldSignals()' - Hold child and termination signals.
 */

void
cupsdHoldSignals(void)
{
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  sigset_t		newmask;	/* New POSIX signal mask */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


  holdcount ++;
  if (holdcount > 1)
    return;

#ifdef HAVE_SIGSET
  sighold(SIGTERM);
  sighold(SIGCHLD);
#elif defined(HAVE_SIGACTION)
  sigemptyset(&newmask);
  sigaddset(&newmask, SIGTERM);
  sigaddset(&newmask, SIGCHLD);
  sigprocmask(SIG_BLOCK, &newmask, &holdmask);
#endif /* HAVE_SIGSET */
}


/*
 * 'cupsdIgnoreChildSignals()' - Ignore SIGCHLD signals...
 *
 * We don't really ignore them, we set the signal handler to SIG_DFL,
 * since some OS's rely on signals for the wait4() function to work.
 */

void
cupsdIgnoreChildSignals(void)
{
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction	action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGCHLD, SIG_DFL);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGCHLD);
  action.sa_handler = SIG_DFL;
  sigaction(SIGCHLD, &action, NULL);
#else
  signal(SIGCLD, SIG_DFL);	/* No, SIGCLD isn't a typo... */
#endif /* HAVE_SIGSET */
}


/*
 * 'cupsdReleaseSignals()' - Release signals for delivery.
 */

void
cupsdReleaseSignals(void)
{
  holdcount --;
  if (holdcount > 0)
    return;

#ifdef HAVE_SIGSET
  sigrelse(SIGTERM);
  sigrelse(SIGCHLD);
#elif defined(HAVE_SIGACTION)
  sigprocmask(SIG_SETMASK, &holdmask, NULL);
#endif /* HAVE_SIGSET */
}


/*
 * 'cupsdSetString()' - Set a string value.
 */

void
cupsdSetString(char       **s,		/* O - New string */
               const char *v)		/* I - String value */
{
  if (!s || *s == v)
    return;

  if (*s)
    free(*s);

  if (v)
    *s = strdup(v);
  else
    *s = NULL;
}


/*
 * 'cupsdSetStringf()' - Set a formatted string value.
 */

void
cupsdSetStringf(char       **s,		/* O - New string */
                const char *f,		/* I - Printf-style format string */
	        ...)			/* I - Additional args as needed */
{
  char		v[4096];		/* Formatting string value */
  va_list	ap;			/* Argument pointer */
  char		*olds;			/* Old string */


  if (!s)
    return;

  olds = *s;

  if (f)
  {
    va_start(ap, f);
    vsnprintf(v, sizeof(v), f, ap);
    va_end(ap);

    *s = strdup(v);
  }
  else
    *s = NULL;

  if (olds)
    free(olds);
}


/*
 * 'parent_handler()' - Catch USR1/CHLD signals...
 */

static void
parent_handler(int sig)			/* I - Signal */
{
 /*
  * Store the signal we got from the OS and return...
  */

  parent_signal = sig;
}


/*
 * 'process_children()' - Process all dead children...
 */

static void
process_children(void)
{
  int		status;			/* Exit status of child */
  int		pid;			/* Process ID of child */
  cupsd_job_t	*job;			/* Current job */
  int		i;			/* Looping var */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "process_children()");

 /*
  * Reset the dead_children flag...
  */

  dead_children = 0;

 /*
  * Collect the exit status of some children...
  */

#ifdef HAVE_WAITPID
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
#elif defined(HAVE_WAIT3)
  while ((pid = wait3(&status, WNOHANG, NULL)) > 0)
#else
  if ((pid = wait(&status)) > 0)
#endif /* HAVE_WAITPID */
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "process_children: pid = %d, status = %d\n", pid, status);

   /*
    * Ignore SIGTERM errors - that comes when a job is cancelled...
    */

    if (status == SIGTERM)
      status = 0;

    if (status)
    {
      if (WIFEXITED(status))
	cupsdLogMessage(CUPSD_LOG_ERROR, "PID %d stopped with status %d!", pid,
	                WEXITSTATUS(status));
      else
	cupsdLogMessage(CUPSD_LOG_ERROR, "PID %d crashed on signal %d!", pid,
	                WTERMSIG(status));

      if (LogLevel < CUPSD_LOG_DEBUG)
        cupsdLogMessage(CUPSD_LOG_INFO,
	                "Hint: Try setting the LogLevel to \"debug\" to find out more.");
    }
    else
      cupsdLogMessage(CUPSD_LOG_DEBUG2, "PID %d exited with no errors.", pid);

   /*
    * Delete certificates for CGI processes...
    */

    if (pid)
      cupsdDeleteCert(pid);

   /*
    * Lookup the PID in the jobs list...
    */

    for (job = (cupsd_job_t *)cupsArrayFirst(ActiveJobs);
	 job;
	 job = (cupsd_job_t *)cupsArrayNext(ActiveJobs))
      if (job->state != NULL &&
          job->state->values[0].integer == IPP_JOB_PROCESSING)
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

	 /*
	  * If this is not the last file in a job, see if all of the
	  * filters are done, and if so move to the next file.
	  */

          if (job->current_file < job->num_files)
	  {
	    for (i = 0; job->filters[i] < 0; i ++);

	    if (!job->filters[i])
	    {
	     /*
	      * Process the next file...
	      */

	      cupsdFinishJob(job);
	    }
	  }
	  break;
	}
      }
  }
}


/*
 * 'sigchld_handler()' - Handle 'child' signals from old processes.
 */

static void
sigchld_handler(int sig)	/* I - Signal number */
{
  (void)sig;

 /*
  * Flag that we have dead children...
  */

  dead_children = 1;

 /*
  * Reset the signal handler as needed...
  */

#if !defined(HAVE_SIGSET) && !defined(HAVE_SIGACTION)
  signal(SIGCLD, sigchld_handler);
#endif /* !HAVE_SIGSET && !HAVE_SIGACTION */
}


/*
 * 'sighup_handler()' - Handle 'hangup' signals to reconfigure the scheduler.
 */

static void
sighup_handler(int sig)	/* I - Signal number */
{
  (void)sig;

  NeedReload = RELOAD_ALL;
  ReloadTime = time(NULL);

#if !defined(HAVE_SIGSET) && !defined(HAVE_SIGACTION)
  signal(SIGHUP, sighup_handler);
#endif /* !HAVE_SIGSET && !HAVE_SIGACTION */
}


/*
 * 'sigterm_handler()' - Handle 'terminate' signals that stop the scheduler.
 */

static void
sigterm_handler(int sig)		/* I - Signal */
{
  (void)sig;	/* remove compiler warnings... */

 /*
  * Flag that we should stop and return...
  */

  stop_scheduler = 1;
}


/*
 * 'select_timeout()' - Calculate the select timeout value.
 *
 */

static long				/* O - Number of seconds */
select_timeout(int fds)			/* I - Number of ready descriptors select returned */
{
  int			i;		/* Looping var */
  long			timeout;	/* Timeout for select */
  time_t		now;		/* Current time */
  cupsd_client_t	*con;		/* Client information */
  cupsd_printer_t	*p;		/* Printer information */
  cupsd_job_t		*job;		/* Job information */
  cupsd_subscription_t	*sub;		/* Subscription information */
  const char		*why;		/* Debugging aid */


 /*
  * Check to see if any of the clients have pending data to be
  * processed; if so, the timeout should be 0...
  */

  for (i = NumClients, con = Clients; i > 0; i --, con ++)
    if (con->http.used > 0)
      return (0);

 /*
  * If select has been active in the last second (fds != 0) or we have
  * many resources in use then don't bother trying to optimize the
  * timeout, just make it 1 second.
  */

  if (fds || NumClients > 50)
    return (1);

 /*
  * Otherwise, check all of the possible events that we need to wake for...
  */

  now     = time(NULL);
  timeout = now + 86400;		/* 86400 == 1 day */
  why     = "do nothing";

 /*
  * Check the activity and close old clients...
  */

  for (i = NumClients, con = Clients; i > 0; i --, con ++)
    if ((con->http.activity + Timeout) < timeout)
    {
      timeout = con->http.activity + Timeout;
      why     = "timeout a client connection";
    }

 /*
  * Update the browse list as needed...
  */

  if (Browsing && BrowseLocalProtocols)
  {
#ifdef HAVE_LIBSLP
    if ((BrowseLocalProtocols & BROWSE_SLP) && (BrowseSLPRefresh < timeout))
    {
      timeout = BrowseSLPRefresh;
      why     = "update SLP browsing";
    }
#endif /* HAVE_LIBSLP */

    if (BrowseLocalProtocols & BROWSE_CUPS)
    {
      for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
           p;
	   p = (cupsd_printer_t *)cupsArrayNext(Printers))
      {
	if (p->type & CUPS_PRINTER_REMOTE)
	{
	  if ((p->browse_time + BrowseTimeout) < timeout)
	  {
	    timeout = p->browse_time + BrowseTimeout;
	    why     = "browse timeout a printer";
	  }
	}
	else if (!(p->type & CUPS_PRINTER_IMPLICIT))
	{
	  if (BrowseInterval && (p->browse_time + BrowseInterval) < timeout)
	  {
	    timeout = p->browse_time + BrowseInterval;
	    why     = "send browse update";
	  }
	}
      }
    }
  }

 /*
  * Check for any active jobs...
  */

  if (timeout > (now + 10) && ActiveJobs)
  {
    for (job = (cupsd_job_t *)cupsArrayFirst(ActiveJobs);
	 job;
	 job = (cupsd_job_t *)cupsArrayNext(ActiveJobs))
      if (job->state->values[0].integer <= IPP_JOB_PROCESSING)
      {
	timeout = now + 10;
	why     = "process active jobs";
	break;
      }
  }

#ifdef HAVE_MALLINFO
 /*
  * Log memory usage every minute...
  */

  if (LogLevel >= CUPSD_LOG_DEBUG && (mallinfo_time + 60) < timeout)
  {
    timeout = mallinfo_time + 60;
    why     = "display memory usage";
  }
#endif /* HAVE_MALLINFO */

 /*
  * Update the root certificate when needed...
  */

  if (!RunUser && RootCertDuration &&
      (RootCertTime + RootCertDuration) < timeout)
  {
    timeout = RootCertTime + RootCertDuration;
    why     = "update root certificate";
  }

 /*
  * Expire subscriptions as needed...
  */

  for (sub = (cupsd_subscription_t *)cupsArrayFirst(Subscriptions);
       sub;
       sub = (cupsd_subscription_t *)cupsArrayNext(Subscriptions))
    if (!sub->job && sub->expire < timeout)
    {
      timeout = sub->expire;
      why     = "expire subscription";
    }

 /*
  * Adjust from absolute to relative time.  If p->browse_time above
  * was 0 then we can end up with a negative value here, so check.
  * We add 1 second to the timeout since events occur after the
  * timeout expires, and limit the timeout to 86400 seconds (1 day)
  * to avoid select() timeout limits present on some operating
  * systems...
  */

  timeout = timeout - now + 1;

  if (timeout < 1)
    timeout = 1;
  else if (timeout > 86400)
    timeout = 86400;

 /*
  * Log and return the timeout value...
  */

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "select_timeout: %ld seconds to %s",
                  timeout, why);

  return (timeout);
}


/*
 * 'usage()' - Show scheduler usage.
 */

static void
usage(void)
{
  fputs("Usage: cupsd [-c config-file] [-f] [-F]\n", stderr);
  exit(1);
}


/*
 * End of "$Id$".
 */
