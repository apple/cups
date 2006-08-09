/*
 * "$Id$"
 *
 *   Scheduler main loop for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
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
 *   cupsdHoldSignals()        - Hold child and termination signals.
 *   cupsdReleaseSignals()     - Release signals for delivery.
 *   cupsdSetString()          - Set a string value.
 *   cupsdSetStringf()         - Set a formatted string value.
 *   launchd_checkin()         - Check-in with launchd and collect the
 *                               listening fds.
 *   launchd_reload()          - Tell launchd to reload the configuration
 *                               file to pick up the new listening directives.
 *   launchd_sync_conf()       - Re-write the launchd(8) config file
 *			         org.cups.cupsd.plist based on cupsd.conf.
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

#ifdef HAVE_LAUNCH_H
#  include <launch.h>
#  include <libgen.h>
#endif /* HAVE_LAUNCH_H */

#if defined(HAVE_MALLOC_H) && defined(HAVE_MALLINFO)
#  include <malloc.h>
#endif /* HAVE_MALLOC_H && HAVE_MALLINFO */
#ifdef HAVE_NOTIFY_H
#  include <notify.h>
#endif /* HAVE_NOTIFY_H */


/*
 * Local functions...
 */

#ifdef HAVE_LAUNCHD
static void	launchd_checkin(void);
static void	launchd_reload(void);
static int	launchd_sync_conf(void);
#endif /* HAVE_LAUNCHD */

static void	parent_handler(int sig);
static void	process_children(void);
static void	sigchld_handler(int sig);
static void	sighup_handler(int sig);
static void	sigterm_handler(int sig);
static long	select_timeout(int fds);
static void	usage(int status);


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
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int			i;		/* Looping var */
  char			*opt;		/* Option character */
  int			fg;		/* Run in the foreground */
  int			fds;		/* Number of ready descriptors */
  fd_set		*input,		/* Input set for select() */
			*output;	/* Output set for select() */
  cupsd_client_t	*con;		/* Current client */
  cupsd_job_t		*job;		/* Current job */
  cupsd_listener_t	*lis;		/* Current listener */
  time_t		current_time,	/* Current time */
			activity,	/* Client activity timer */
			browse_time,	/* Next browse send time */
			senddoc_time,	/* Send-Document time */
			expire_time;	/* Subscription expire time */
  time_t		mallinfo_time;	/* Malloc information time */
  size_t		string_count,	/* String count */
			alloc_bytes,	/* Allocated string bytes */
			total_bytes;	/* Total string bytes */
  struct timeval	timeout;	/* select() timeout */
  struct rlimit		limit;		/* Runtime limit */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction	action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */
#ifdef __sgi
  cups_file_t		*fp;		/* Fake lpsched lock file */
  struct stat		statbuf;	/* Needed for checking lpsched FIFO */
#endif /* __sgi */
#if HAVE_LAUNCHD
  int			launchd_idle_exit;
					/* Idle exit on select timeout? */
#endif	/* HAVE_LAUNCHD */


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
	      {
	        _cupsLangPuts(stderr, _("cupsd: Expected config filename "
		                        "after \"-c\" option!\n"));
	        usage(1);
	      }

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

                char *current;		/* Current directory */


	       /*
	        * Allocate a buffer for the current working directory to
		* reduce run-time stack usage; this approximates the
		* behavior of some implementations of getcwd() when they
		* are passed a NULL pointer.
	        */

                current = malloc(1024);
		getcwd(current, 1024);

		cupsdSetStringf(&ConfigurationFile, "%s/%s", current, argv[i]);

		free(current);
              }
	      break;

          case 'f' : /* Run in foreground... */
	      fg = 1;
	      break;

          case 'F' : /* Run in foreground, but disconnect from terminal... */
	      fg = -1;
	      break;

          case 'h' : /* Show usage/help */
	      usage(0);
	      break;

          case 'l' : /* Started by launchd... */
#ifdef HAVE_LAUNCHD
	      Launchd = 1;
	      fg      = 1;
#else
	      _cupsLangPuts(stderr, _("cupsd: launchd(8) support not compiled "
	                              "in, running in normal mode.\n"));
              fg = 0;
#endif /* HAVE_LAUNCHD */
	      break;

          case 'p' : /* Stop immediately for profiling */
              puts("Warning: -p option is for internal testing use only!");
	      stop_scheduler = 1;
	      fg             = 1;
	      break;

	  default : /* Unknown option */
              _cupsLangPrintf(stderr, _("cupsd: Unknown option \"%c\" - "
	                                "aborting!\n"), *opt);
	      usage(1);
	      break;
	}
    else
    {
      _cupsLangPrintf(stderr, _("cupsd: Unknown argument \"%s\" - aborting!\n"),
                      argv[i]);
      usage(1);
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
        fprintf(stderr, "cupsd: Child exited with status %d!\n",
	        WEXITSTATUS(i));
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

#if HAVE_LAUNCHD
  if (Launchd)
  {
   /*
    * If we were started by launchd make sure the cupsd plist file contains the
    * same listeners as cupsd.conf; If it didn't then reload it before getting
    * the list of listening file descriptors...
    */

    if (launchd_sync_conf())
    {
      launchd_reload();

     /*
      * Until rdar://3854821 is fixed we have to exit after the reload...
      */

      cupsdLogMessage(CUPSD_LOG_DEBUG2, "Exiting on launchd_reload");
      exit(0);
    }

    launchd_checkin();
  }
#endif /* HAVE_LAUNCHD */

 /*
  * Startup the server...
  */

  cupsdStartServer();

 /*
  * Catch hangup and child signals and ignore broken pipes...
  */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGCHLD, sigchld_handler);
  sigset(SIGHUP, sighup_handler);
  sigset(SIGPIPE, SIG_IGN);
  sigset(SIGTERM, sigterm_handler);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGTERM);
  sigaddset(&action.sa_mask, SIGCHLD);
  action.sa_handler = sigchld_handler;
  sigaction(SIGCHLD, &action, NULL);

  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGHUP);
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
  signal(SIGCLD, sigchld_handler);	/* No, SIGCLD isn't a typo... */
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

#ifdef __APPLE__
 /*
  * Start power management framework...
  */

  cupsdStartSystemMonitor();
#endif /* __APPLE__ */

 /*
  * Start any pending print jobs...
  */

  cupsdCheckJobs();

 /*
  * Loop forever...
  */

  mallinfo_time = 0;
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

      if (cupsArrayCount(Clients) > 0)
      {
	for (con = (cupsd_client_t *)cupsArrayFirst(Clients);
	     con;
	     con = (cupsd_client_t *)cupsArrayNext(Clients))
	  if (con->http.state == HTTP_WAITING)
	    cupsdCloseClient(con);
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
        if (job->state_value == IPP_JOB_PROCESSING)
	  break;

     /*
      * Restart if all clients are closed and all jobs finished, or
      * if the reload timeout has elapsed...
      */

      if ((cupsArrayCount(Clients) == 0 &&
           (!job || NeedReload != RELOAD_ALL)) ||
          (time(NULL) - ReloadTime) >= ReloadTimeout)
      {
       /*
	* Shutdown the server...
	*/

	cupsdStopServer();

       /*
	* Read configuration...
	*/

        if (!cupsdReadConfiguration())
        {
          syslog(LOG_LPR, "Unable to read configuration file \'%s\' - exiting!",
		 ConfigurationFile);
          break;
	}

#if HAVE_LAUNCHD
	if (Launchd)
	{
	  if (launchd_sync_conf())
	  {
	    launchd_reload();

	   /*
	    * Until rdar://3854821 is fixed we have to exit after the reload...
	    */

	    cupsdLogMessage(CUPSD_LOG_DEBUG2, "Exiting on launchd_reload");
	    stop_scheduler = 1;
	    break;
	  }

	  launchd_checkin();
	}
#endif /* HAVE_LAUNCHD */

       /*
        * Startup the server...
        */

        cupsdStartServer();
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

#if HAVE_LAUNCHD
   /*
    * If no other work is scheduled and we're being controlled by
    * launchd(8) then timeout after 'LaunchdTimeout' seconds of
    * inactivity...
    */

    if (timeout.tv_sec == 86400 && Launchd && LaunchdTimeout && !NumPolled &&
	(!Browsing || !(BrowseLocalProtocols & BROWSE_DNSSD) ||
	 cupsArrayCount(Printers) == 0))
    {
      timeout.tv_sec    = LaunchdTimeout;
      launchd_idle_exit = 1;
    }
    else
      launchd_idle_exit = 0;
#endif	/* HAVE_LAUNCHD */

    if (timeout.tv_sec < 86400)		/* Only use timeout for < 1 day */
      fds = select(MaxFDs, input, output, NULL, &timeout);
    else
      fds = select(MaxFDs, input, output, NULL, NULL);

    if (fds < 0)
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

      cupsdLogMessage(CUPSD_LOG_EMERG, "%s", s);

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

      cupsdLogMessage(CUPSD_LOG_EMERG, "%s", s);

      for (i = 0, con = (cupsd_client_t *)cupsArrayFirst(Clients);
	   con;
	   i ++, con = (cupsd_client_t *)cupsArrayNext(Clients))
        cupsdLogMessage(CUPSD_LOG_EMERG,
	                "Clients[%d] = %d, file = %d, state = %d",
	                i, con->http.fd, con->file, con->http.state);

      for (i = 0, lis = (cupsd_listener_t *)cupsArrayFirst(Listeners);
           lis;
	   i ++, lis = (cupsd_listener_t *)cupsArrayNext(Listeners))
        cupsdLogMessage(CUPSD_LOG_EMERG, "Listeners[%d] = %d", i, lis->fd);

      cupsdLogMessage(CUPSD_LOG_EMERG, "BrowseSocket = %d", BrowseSocket);

      cupsdLogMessage(CUPSD_LOG_EMERG, "CGIPipes[0] = %d", CGIPipes[0]);

#ifdef __APPLE__
      cupsdLogMessage(CUPSD_LOG_EMERG, "SysEventPipes[0] = %d",
                      SysEventPipes[0]);
#endif /* __APPLE__ */

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

#if HAVE_LAUNCHD
   /*
    * If no other work was scheduled and we're being controlled by launchd(8)
    * then timeout after 'LaunchdTimeout' seconds of inactivity...
    */

    if (!fds && launchd_idle_exit)
    {
      cupsdLogMessage(CUPSD_LOG_INFO,
                      "Printer sharing is off and there are no jobs pending, "
		      "will restart on demand.");
      stop_scheduler = 1;
      break;
    }
#endif /* HAVE_LAUNCHD */

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
    * Handle system management events as needed...
    */

#ifdef __APPLE__
    if (SysEventPipes[0] >= 0 && FD_ISSET(SysEventPipes[0], input))
      cupsdUpdateSystemMonitor();
#endif	/* __APPLE__ */

   /*
    * Update notifier messages as needed...
    */

    if (NotifierPipes[0] >= 0 && FD_ISSET(NotifierPipes[0], input))
      cupsdUpdateNotifierStatus();

   /*
    * Expire subscriptions and unload completed jobs as needed...
    */

    if (current_time > expire_time)
    {
      if (cupsArrayCount(Subscriptions) > 0)
        cupsdExpireSubscriptions(NULL, NULL);

      cupsdUnloadCompletedJobs();

      expire_time = current_time;
    }

   /*
    * Update the browse list as needed...
    */

    if (Browsing && BrowseRemoteProtocols)
    {
      if (BrowseSocket >= 0 && FD_ISSET(BrowseSocket, input))
        cupsdUpdateCUPSBrowse();

      if (PollPipe >= 0 && FD_ISSET(PollPipe, input))
        cupsdUpdatePolling();

#ifdef HAVE_LIBSLP
      if ((BrowseRemoteProtocols & BROWSE_SLP) &&
          BrowseSLPRefresh <= current_time)
        cupsdUpdateSLPBrowse();
#endif /* HAVE_LIBSLP */

#ifdef HAVE_LDAP
      if ((BrowseRemoteProtocols & BROWSE_LDAP) &&
          BrowseLDAPRefresh <= current_time)
        cupsdUpdateLDAPBrowse();
#endif /* HAVE_LDAP */
    }

    if (Browsing && BrowseLocalProtocols && current_time > browse_time)
    {
      cupsdSendBrowseList();
      browse_time = current_time;
    }

   /*
    * Check for new connections on the "listen" sockets...
    */

    for (lis = (cupsd_listener_t *)cupsArrayFirst(Listeners);
         lis;
	 lis = (cupsd_listener_t *)cupsArrayNext(Listeners))
      if (lis->fd >= 0 && FD_ISSET(lis->fd, input))
      {
        FD_CLR(lis->fd, input);
        cupsdAcceptClient(lis);
      }

   /*
    * Check for new data on the client sockets...
    */

    for (con = (cupsd_client_t *)cupsArrayFirst(Clients);
	 con;
	 con = (cupsd_client_t *)cupsArrayNext(Clients))
    {
     /*
      * Process the input buffer...
      */

      if (FD_ISSET(con->http.fd, input) || con->http.used)
      {
        int fd = con->file;


        FD_CLR(con->http.fd, input);

        if (!cupsdReadClient(con))
	{
	  if (fd >= 0)
	    FD_CLR(fd, input);

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
	  FD_CLR(con->file, input);
	  FD_CLR(con->file, InputSet);
	}
      }

      if (FD_ISSET(con->http.fd, output))
      {
        FD_CLR(con->http.fd, output);

	if (!con->pipe_pid || con->file_ready)
          if (!cupsdWriteClient(con))
	    continue;
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

   /*
    * Log memory usage every minute...
    */

    if ((current_time - mallinfo_time) >= 60 && LogLevel >= CUPSD_LOG_DEBUG2)
    {
#ifdef HAVE_MALLINFO
      struct mallinfo mem;		/* Malloc information */


      mem = mallinfo();
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "mallinfo: arena = %d, used = %d, free = %d\n",
                      mem.arena, mem.usmblks + mem.uordblks,
		      mem.fsmblks + mem.fordblks);
#endif /* HAVE_MALLINFO */

      string_count = _cupsStrStatistics(&alloc_bytes, &total_bytes);
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "stringpool: " CUPS_LLFMT " strings, "
		      CUPS_LLFMT " allocated, " CUPS_LLFMT " total bytes",
		      CUPS_LLCAST string_count, CUPS_LLCAST alloc_bytes,
		      CUPS_LLCAST total_bytes);

      mallinfo_time = current_time;
    }

   /*
    * Update the root certificate once every 5 minutes if we have client
    * connections...
    */

    if ((current_time - RootCertTime) >= RootCertDuration && RootCertDuration &&
        !RunUser && cupsArrayCount(Clients))
    {
     /*
      * Update the root certificate...
      */

      cupsdDeleteCert(0);
      cupsdAddCert(0, "root");
    }

   /*
    * Handle OS-specific event notification for any events that have
    * accumulated.  Don't send these more than once a second...
    */

    if (LastEvent)
    {
#ifdef HAVE_NOTIFY_POST
      if (LastEvent & CUPSD_EVENT_PRINTER_CHANGED)
      {
        cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                "notify_post(\"com.apple.printerListChange\")");
	notify_post("com.apple.printerListChange");
      }

      if (LastEvent & CUPSD_EVENT_PRINTER_STATE_CHANGED)
      {
        cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                "notify_post(\"com.apple.printerHistoryChange\")");
	notify_post("com.apple.printerHistoryChange");
      }

      if (LastEvent & (CUPSD_EVENT_JOB_STATE_CHANGED |
                       CUPSD_EVENT_JOB_CONFIG_CHANGED |
                       CUPSD_EVENT_JOB_PROGRESS))
      {
        cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                "notify_post(\"com.apple.jobChange\")");
	notify_post("com.apple.jobChange");
      }
#endif /* HAVE_NOTIFY_POST */

     /*
      * Reset the accumulated events...
      */

      LastEvent     = CUPSD_EVENT_NONE;
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

  cupsdFreeAllJobs();

#ifdef __APPLE__
  cupsdStopSystemMonitor();
#endif /* __APPLE__ */

#ifdef HAVE_LAUNCHD
 /*
  * Update the launchd config file as needed...
  */

  launchd_sync_conf();
#endif /* HAVE_LAUNCHD */

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
  {
    fds[0] = -1;
    fds[1] = -1;

    return (-1);
  }

 /*
  * Set the "close on exec" flag on each end of the pipe...
  */

  if (fcntl(fds[0], F_SETFD, fcntl(fds[0], F_GETFD) | FD_CLOEXEC))
  {
    close(fds[0]);
    close(fds[1]);

    fds[0] = -1;
    fds[1] = -1;

    return (-1);
  }

  if (fcntl(fds[1], F_SETFD, fcntl(fds[1], F_GETFD) | FD_CLOEXEC))
  {
    close(fds[0]);
    close(fds[1]);

    fds[0] = -1;
    fds[1] = -1;

    return (-1);
  }

 /*
  * Return 0 indicating success...
  */

  return (0);
}


/*
 * 'cupsdClearString()' - Clear a string.
 */

void
cupsdClearString(char **s)		/* O - String value */
{
  if (s && *s)
  {
    _cupsStrFree(*s);
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
    _cupsStrFree(*s);

  if (v)
    *s = _cupsStrAlloc(v);
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

    *s = _cupsStrAlloc(v);
  }
  else
    *s = NULL;

  if (olds)
    _cupsStrFree(olds);
}


#ifdef HAVE_LAUNCHD
/*
 * 'launchd_checkin()' - Check-in with launchd and collect the listening fds.
 */

static void
launchd_checkin(void)
{
  int			i,		/* Looping var */
			count,		/* Numebr of listeners */
			portnum;	/* Port number */
  launch_data_t		ld_msg,		/* Launch data message */
			ld_resp,	/* Launch data response */
			ld_array,	/* Launch data array */
			ld_sockets,	/* Launch data sockets dictionary */
			tmp;		/* Launch data */
  cupsd_listener_t	*lis;		/* Listeners array */
  http_addr_t		addr;		/* Address variable */
  socklen_t		addrlen;	/* Length of address */


  cupsdLogMessage(CUPSD_LOG_DEBUG, "launchd_checkin: pid=%d", (int)getpid());

 /*
  * Check-in with launchd...
  */

  ld_msg = launch_data_new_string(LAUNCH_KEY_CHECKIN);
  if ((ld_resp = launch_msg(ld_msg)) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
		    "launchd_checkin: launch_msg(\"" LAUNCH_KEY_CHECKIN
		    "\") IPC failure");
    exit(EXIT_FAILURE);
  }

  if (launch_data_get_type(ld_resp) == LAUNCH_DATA_ERRNO)
  {
    errno = launch_data_get_errno(ld_resp);
    cupsdLogMessage(CUPSD_LOG_ERROR, "launchd_checkin: Check-in failed: %s",
                    strerror(errno));
    exit(EXIT_FAILURE);
  }

 /*
  * Get the sockets dictionary...
  */

  if (!(ld_sockets = launch_data_dict_lookup(ld_resp, LAUNCH_JOBKEY_SOCKETS)))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "launchd_checkin: No sockets found to answer requests on!");
    exit(EXIT_FAILURE);
  }

 /*
  * Get the array of listener sockets...
  */

  if (!(ld_array = launch_data_dict_lookup(ld_sockets, "Listeners")))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "launchd_checkin: No sockets found to answer requests on!");
    exit(EXIT_FAILURE);
  }

 /*
  * Add listening fd(s) to the Listener array...
  */

  if (launch_data_get_type(ld_array) == LAUNCH_DATA_ARRAY)
  {
   /*
    * Free the listeners array built from cupsd.conf...
    */

    cupsdDeleteAllListeners();

   /*
    * Create a new array of listeners from the launchd data...
    */

    Listeners = cupsArrayNew(NULL, NULL);
    count     = launch_data_array_get_count(ld_array);

    for (i = 0; i < count; i ++)
    {
     /*
      * Copy the current address and log it...
      */

      if ((lis = calloc(1, sizeof(cupsd_listener_t))) == NULL)
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
                	"launchd_checkin: Unable to allocate listener - %s.",
                	strerror(errno));
	exit(EXIT_FAILURE);
      }

      cupsArrayAdd(Listeners, lis);

      tmp     = launch_data_array_get_index(ld_array, i);
      lis->fd = launch_data_get_fd(tmp);
      addrlen = sizeof(lis->address);

      if (getsockname(lis->fd, (struct sockaddr *)&(lis->address), &addrlen))
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "launchd_checkin: Unable to get local address - %s",
			strerror(errno));
      }

#  ifdef HAVE_SSL
      portnum = 0;

#    ifdef AF_INET6
      if (addr.addr.sa_family == AF_INET6)
	portnum = ntohs(addr.ipv6.sin6_port);
      else
#    endif /* AF_INET6 */
      if (addr.addr.sa_family == AF_INET)
	portnum = ntohs(addr.ipv4.sin_port);

      if (portnum == 443)
	lis->encryption = HTTP_ENCRYPT_ALWAYS;
#  endif /* HAVE_SSL */
    }
  }

 /*
  * Collect the browse socket (if there is one)...
  */

  if ((ld_array = launch_data_dict_lookup(ld_sockets, "BrowseSockets")))
  {
    if (launch_data_get_type(ld_array) == LAUNCH_DATA_ARRAY)
    {
      if ((tmp = launch_data_array_get_index(ld_array, 0)))
      {
	if (launch_data_get_type(tmp) == LAUNCH_DATA_FD)
	{
	  if (BrowseSocket != -1)
	    close(BrowseSocket);
  
	  BrowseSocket = launch_data_get_fd(tmp);
	}
	else
	  cupsdLogMessage(CUPSD_LOG_WARN,
			  "launchd_checkin: BrowseSocket not a fd!");
     }
     else
       cupsdLogMessage(CUPSD_LOG_WARN,
		       "launchd_checkin: BrowseSockets is an empty array!");
   }
   else
     cupsdLogMessage(CUPSD_LOG_WARN,
                     "launchd_checkin: BrowseSockets is not an array!");
  }
  else
    cupsdLogMessage(CUPSD_LOG_DEBUG, "launchd_checkin: No BrowseSockets");

  launch_data_free(ld_msg);
  launch_data_free(ld_resp);
}


/*
 * 'launchd_reload()' - Tell launchd to reload the configuration file to pick
 *                      up the new listening directives.
 */

static void
launchd_reload(void)
{
  int		child_status;		/* Exit status of child process */
  pid_t		child_pid,		/* Child PID */
		waitpid_status;		/* Child process exit status */
  char		*argv[4];		/* Argument strings */


 /*
  * The current launchd doesn't support a reload option (rdar://3854821).
  * Until this is fixed we need to reload the config file by execing launchctl
  * twice (to unload then load). NOTE: This will cause us to exit on SIGTERM
  * which will cancel all client & job activity.
  *
  * After this is fixed we'll be able to tell launchd to reload the file
  * and pick up the new listening descriptors without disrupting current
  * activity.
  */

 /*
  * Unloading the current configuration will cause launchd to send us a SIGTERM;
  * block it for now so we can get our work done...
  */

  cupsdHoldSignals();

 /*
  * Set up the unload arguments to launchctl...
  */

  argv[0] = "/bin/launchctl";
  argv[1] = "unload";
  argv[2] = LaunchdConf;
  argv[3] = NULL;

  if (cupsdStartProcess(argv[0], argv, NULL, -1, -1, -1, -1, 1, &child_pid) < 0)
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "launchd_reload: Unable to execute %s - %s", argv[0],
                    strerror(errno));
  else
  {
    do
    {
      waitpid_status = waitpid(child_pid, &child_status, 0);
    }
    while (waitpid_status == (pid_t)-1 && errno == EINTR);

    if (WIFSIGNALED(child_status))
      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "launchd_reload: %s pid %d crashed on signal %d!",
		      basename(argv[0]), child_pid, WTERMSIG(child_status));
    else
      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "launchd_reload: %s pid %d stopped with status %d!",
		      basename(argv[0]), child_pid, WEXITSTATUS(child_status));

   /*
    * Do it again with the load command...
    */

    argv[1] = "load";

    if (cupsdStartProcess(argv[0], argv, NULL, -1, -1, -1, -1, 1,
                          &child_pid) < 0)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "launchd_reload: Unable to fork for %s - %s", argv[0],
                      strerror(errno));
    }
    else
    {
      do
      {
	waitpid_status = waitpid(child_pid, &child_status, 0);
      } while (waitpid_status == (pid_t)-1 && errno == EINTR);

      if (WIFSIGNALED(child_status))
	cupsdLogMessage(CUPSD_LOG_DEBUG,
	                "launchd_reload: %s pid %d crashed on signal %d!",
			basename(argv[0]), child_pid, WTERMSIG(child_status));
      else
	cupsdLogMessage(CUPSD_LOG_DEBUG,
	                "launchd_reload: %s pid %d stopped with status %d",
			basename(argv[0]), child_pid,
			WEXITSTATUS(child_status));
    }
  }

 /*
  * Leave signals blocked since exit() will be called momentarily anyways...
  */
}


/*
 * 'launchd_sync_conf()' - Re-write the launchd(8) config file
 *			   org.cups.cupsd.plist based on cupsd.conf.
 */

static int				/* O - 1 if the file was updated */
launchd_sync_conf(void)
{
  int			  portnum;	/* Port number */
  CFMutableDictionaryRef  cupsd_dict,	/* org.cups.cupsd.plist dictionary */
			  sockets,	/* Sockets dictionary */
			  listener;	/* Listener dictionary */
  CFDataRef		  resourceData;	/* XML property list */
  CFMutableArrayRef	  array;	/* Array */
  CFNumberRef		  socket_mode;	/* Domain socket mode bits */
  CFStringRef		  socket_path;	/* Domain socket path */
  CFTypeRef		  value;	/* CF value */
  CFURLRef		  fileURL;	/* File URL */
  SInt32		  errorCode;	/* Error code */
  cupsd_listener_t	  *lis;		/* Current listening socket */
  struct servent	  *service;	/* Services data base entry */
  char			  temp[1024];	/* Temporary buffer for value */
  struct stat		  cupsd_sb,	/* File info for cupsd.conf */
			  launchd_sb;	/* File info for org.cups.cupsd.plist */


 /*
  * If the launchd conf file modification time is newer than the cupsd.conf
  * time then there's nothing to do...
  */

  if (!stat(ConfigurationFile, &cupsd_sb) &&
      !stat(LaunchdConf, &launchd_sb) &&
      launchd_sb.st_mtimespec.tv_sec >= cupsd_sb.st_mtimespec.tv_sec)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG,
                    "launchd_sync_conf: Nothing to do, pid=%d.",
		    (int)getpid());
    return (0);
  }

 /*
  * Time to write a new 'org.cups.cupsd.plist' file.
  * Create the new dictionary and populate it with values...
  */

  if ((cupsd_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
				&kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks)) != NULL)
  {
    CFDictionaryAddValue(cupsd_dict, CFSTR(LAUNCH_JOBKEY_LABEL),
                         CFSTR("org.cups.cupsd"));
    CFDictionaryAddValue(cupsd_dict, CFSTR(LAUNCH_JOBKEY_ONDEMAND),
                         kCFBooleanTrue);

    if ((Browsing && BrowseLocalProtocols && cupsArrayCount(Printers)) ||
        cupsArrayCount(ActiveJobs))
      CFDictionaryAddValue(cupsd_dict, CFSTR(LAUNCH_JOBKEY_RUNATLOAD),
                           kCFBooleanTrue);
    else
      CFDictionaryAddValue(cupsd_dict, CFSTR(LAUNCH_JOBKEY_RUNATLOAD),
                           kCFBooleanFalse);

#ifdef LAUNCH_JOBKEY_SERVICEIPC
    CFDictionaryAddValue(cupsd_dict, CFSTR(LAUNCH_JOBKEY_SERVICEIPC),
			 kCFBooleanTrue);
#endif  /* LAUNCH_JOBKEY_SERVICEIPC */

    if ((array = CFArrayCreateMutable(kCFAllocatorDefault, 2,
                                      &kCFTypeArrayCallBacks)) != NULL)
    {
      CFDictionaryAddValue(cupsd_dict, CFSTR(LAUNCH_JOBKEY_PROGRAMARGUMENTS),
                           array);
      CFArrayAppendValue(array, CFSTR("/usr/sbin/cupsd"));
      CFArrayAppendValue(array, CFSTR("-l"));
      CFRelease(array);
    }

   /*
    * Add a sockets dictionary...
    */

    if ((sockets = (CFMutableDictionaryRef)CFDictionaryCreateMutable(
				kCFAllocatorDefault, 0,
				&kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks)) != NULL)
    {
      CFDictionaryAddValue(cupsd_dict, CFSTR(LAUNCH_JOBKEY_SOCKETS), sockets);

     /*
      * Add a Listeners array to the sockets dictionary...
      */

      if ((array = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                        &kCFTypeArrayCallBacks)) != NULL)
      {
	CFDictionaryAddValue(sockets, CFSTR("Listeners"), array);

       /*
	* For each listener add a dictionary to the listeners array...
	*/

	for (lis = (cupsd_listener_t *)cupsArrayFirst(Listeners);
	     lis;
	     lis = (cupsd_listener_t *)cupsArrayNext(Listeners))
	{
	  if ((listener = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
				&kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks)) != NULL)
	  {
	    CFArrayAppendValue(array, listener);

#  ifdef AF_LOCAL
	    if (lis->address.addr.sa_family == AF_LOCAL)
	    {
	      if ((socket_path = CFStringCreateWithCString(kCFAllocatorDefault,
					lis->address.un.sun_path,
					kCFStringEncodingUTF8)))
	      {
		CFDictionaryAddValue(listener,
		                     CFSTR(LAUNCH_JOBSOCKETKEY_PATHNAME),
		                     socket_path);
		CFRelease(socket_path);
	      }
	      portnum = 0140777; /* (S_IFSOCK|S_IRWXU|S_IRWXG|S_IRWXO) or *
	                          * 49663d decimal                        */
	      if ((socket_mode = CFNumberCreate(kCFAllocatorDefault,
	                                        kCFNumberIntType, &portnum)))
	      {
		CFDictionaryAddValue(listener, CFSTR("SockPathMode"),
		                     socket_mode);
		CFRelease(socket_mode);
	      }
	    }
	    else
#  endif /* AF_LOCAL */
	    {
#  ifdef AF_INET6
	      if (lis->address.addr.sa_family == AF_INET6)
	      {
		CFDictionaryAddValue(listener,
		                     CFSTR(LAUNCH_JOBSOCKETKEY_FAMILY),
		                     CFSTR("IPv6"));
		portnum = lis->address.ipv6.sin6_port;
	      }
	      else
#  endif /* AF_INET6 */
	      {
		CFDictionaryAddValue(listener,
		                     CFSTR(LAUNCH_JOBSOCKETKEY_FAMILY),
		                     CFSTR("IPv4"));
		portnum = lis->address.ipv4.sin_port;
	      }

	      if ((service = getservbyport(portnum, NULL)))
		value = CFStringCreateWithCString(kCFAllocatorDefault,
						  service->s_name,
						  kCFStringEncodingUTF8);
	      else
		value = CFNumberCreate(kCFAllocatorDefault,
				       kCFNumberIntType, &portnum);

	      if (value)
	      {
		CFDictionaryAddValue(listener,
		                     CFSTR(LAUNCH_JOBSOCKETKEY_SERVICENAME),
				     value);
		CFRelease(value);
	      }	

	      httpAddrString(&lis->address, temp, sizeof(temp));
	      if ((value = CFStringCreateWithCString(kCFAllocatorDefault, temp,
						     kCFStringEncodingUTF8)))
	      {
		CFDictionaryAddValue(listener,
		                     CFSTR(LAUNCH_JOBSOCKETKEY_NODENAME),
				     value);
		CFRelease(value);
	      }
	    }

	    CFRelease(listener);
	  }
	}

	CFRelease(array);
      }

     /*
      * Add the BrowseSocket to the sockets dictionary...
      */

      if (Browsing && (BrowseRemoteProtocols & BROWSE_CUPS))
      {
	if ((array = CFArrayCreateMutable(kCFAllocatorDefault, 0,
					  &kCFTypeArrayCallBacks)) != NULL)
	{
	  CFDictionaryAddValue(sockets, CFSTR("BrowseSockets"), array);

	  if ((listener = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
				  &kCFTypeDictionaryKeyCallBacks,
				  &kCFTypeDictionaryValueCallBacks)) != NULL)
	  {
	    CFArrayAppendValue(array, listener);

	    CFDictionaryAddValue(listener, CFSTR(LAUNCH_JOBSOCKETKEY_FAMILY),
	                         CFSTR("IPv4"));
	    CFDictionaryAddValue(listener, CFSTR(LAUNCH_JOBSOCKETKEY_TYPE),
	                         CFSTR("dgram"));

	    if ((service = getservbyport(BrowsePort, NULL)))
	      value = CFStringCreateWithCString(kCFAllocatorDefault,
						service->s_name,
						kCFStringEncodingUTF8);
	    else
	      value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType,
				     &BrowsePort);

	    CFDictionaryAddValue(listener,
	                         CFSTR(LAUNCH_JOBSOCKETKEY_SERVICENAME), value);
	    CFRelease(value);

	    CFRelease(listener);
	  }

	  CFRelease(array);
	}
      }

      CFRelease(sockets);
    }

    cupsdLogMessage(CUPSD_LOG_DEBUG,
                    "launchd_sync_conf: Updating \"%s\", pid=%d\n",
		    LaunchdConf, (int)getpid());

    if ((fileURL = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
				(const unsigned char *)LaunchdConf,
				strlen(LaunchdConf), false)))
    {
      if ((resourceData = CFPropertyListCreateXMLData(kCFAllocatorDefault,
                                                      cupsd_dict)))
      {
	if (!CFURLWriteDataAndPropertiesToResource(fileURL, resourceData,
	                                           NULL, &errorCode))
	{
	  cupsdLogMessage(CUPSD_LOG_WARN,
	                  "launchd_sync_conf: "
			  "CFURLWriteDataAndPropertiesToResource(\"%s\") "
			  "failed: %d\n",
			  LaunchdConf, (int)errorCode);
        }

	CFRelease(resourceData);
      }

      CFRelease(fileURL);
    }

    CFRelease(cupsd_dict);
  }

 /*
  * Let the caller know we updated the file...
  */

  return (1);
}
#endif /* HAVE_LAUNCHD */


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
  char		name[1024];		/* Process name */


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
   /*
    * Ignore SIGTERM errors - that comes when a job is cancelled...
    */

    cupsdFinishProcess(pid, name, sizeof(name));

    if (status == SIGTERM)
      status = 0;

    if (status)
    {
      if (WIFEXITED(status))
	cupsdLogMessage(CUPSD_LOG_ERROR, "PID %d (%s) stopped with status %d!",
	                pid, name, WEXITSTATUS(status));
      else
	cupsdLogMessage(CUPSD_LOG_ERROR, "PID %d (%s) crashed on signal %d!",
	                pid, name, WTERMSIG(status));

      if (LogLevel < CUPSD_LOG_DEBUG)
        cupsdLogMessage(CUPSD_LOG_INFO,
	                "Hint: Try setting the LogLevel to \"debug\" to find "
			"out more.");
    }
    else
      cupsdLogMessage(CUPSD_LOG_DEBUG, "PID %d (%s) exited with no errors.",
                      pid, name);

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
      if (job->state_value == IPP_JOB_PROCESSING)
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

            if (job->printer && !(job->printer->type & CUPS_PRINTER_FAX))
	    {
              snprintf(job->printer->state_message,
	               sizeof(job->printer->state_message), "%s failed", name);
              cupsdAddPrinterHistory(job->printer);
	    }
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
select_timeout(int fds)			/* I - Number of descriptors returned */
{
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

  for (con = (cupsd_client_t *)cupsArrayFirst(Clients);
       con;
       con = (cupsd_client_t *)cupsArrayNext(Clients))
    if (con->http.used > 0)
      return (0);

 /*
  * If select has been active in the last second (fds != 0) or we have
  * many resources in use then don't bother trying to optimize the
  * timeout, just make it 1 second.
  */

  if (fds || cupsArrayCount(Clients) > 50)
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

  for (con = (cupsd_client_t *)cupsArrayFirst(Clients);
       con;
       con = (cupsd_client_t *)cupsArrayNext(Clients))
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

#ifdef HAVE_LDAP
    if ((BrowseLocalProtocols & BROWSE_LDAP) && (BrowseLDAPRefresh < timeout))
    {
      timeout = BrowseLDAPRefresh;
      why     = "update LDAP browsing";
    }
#endif /* HAVE_LDAP */

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
      if (job->state_value <= IPP_JOB_PROCESSING)
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
  * Expire subscriptions as needed...
  */

  for (sub = (cupsd_subscription_t *)cupsArrayFirst(Subscriptions);
       sub;
       sub = (cupsd_subscription_t *)cupsArrayNext(Subscriptions))
    if (!sub->job && sub->expire && sub->expire < timeout)
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
usage(int status)			/* O - Exit status */
{
  _cupsLangPuts(status ? stderr : stdout,
                _("Usage: cupsd [-c config-file] [-f] [-F] [-h] [-l]\n"
		  "\n"
		  "-c config-file      Load alternate configuration file\n"
		  "-f                  Run in the foreground\n"
		  "-F                  Run in the foreground but detach\n"
		  "-h                  Show this usage message\n"
		  "-l                  Run cupsd from launchd(8)\n"));
  exit(status);
}


/*
 * End of "$Id$".
 */
