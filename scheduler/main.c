/*
 * "$Id$"
 *
 *   Scheduler main loop for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   "LICENSE" which should have been included with this file.  If this
 *   file is missing or damaged, see the license at "http://www.cups.org/".
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
 *   launchd_checkout()        - Check-out with launchd.
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
#include <cups/dir.h>

#ifdef HAVE_LAUNCH_H
#  include <launch.h>
#  include <libgen.h>
#  define CUPS_KEEPALIVE	CUPS_CACHEDIR "/org.cups.cupsd"
					/* Name of the launchd KeepAlive file */
#  ifndef LAUNCH_JOBKEY_KEEPALIVE
#    define LAUNCH_JOBKEY_KEEPALIVE "KeepAlive"
#  endif /* !LAUNCH_JOBKEY_KEEPALIVE */
#  ifndef LAUNCH_JOBKEY_PATHSTATE
#    define LAUNCH_JOBKEY_PATHSTATE "PathState"
#  endif /* !LAUNCH_JOBKEY_PATHSTATE */
#  ifndef LAUNCH_JOBKEY_SERVICEIPC
#    define LAUNCH_JOBKEY_SERVICEIPC "ServiceIPC"
#  endif /* !LAUNCH_JOBKEY_SERVICEIPC */
#endif /* HAVE_LAUNCH_H */

#if defined(HAVE_MALLOC_H) && defined(HAVE_MALLINFO)
#  include <malloc.h>
#endif /* HAVE_MALLOC_H && HAVE_MALLINFO */
#ifdef HAVE_NOTIFY_H
#  include <notify.h>
#endif /* HAVE_NOTIFY_H */

#if defined(__APPLE__) && defined(HAVE_DLFCN_H)
#  include <dlfcn.h>
#endif /* __APPLE__ && HAVE_DLFCN_H */


/*
 * Local functions...
 */

#ifdef HAVE_LAUNCHD
static void		launchd_checkin(void);
static void		launchd_checkout(void);
#endif /* HAVE_LAUNCHD */
static void		parent_handler(int sig);
static void		process_children(void);
static void		sigchld_handler(int sig);
static void		sighup_handler(int sig);
static void		sigterm_handler(int sig);
static long		select_timeout(int fds);
static void		usage(int status);


/*
 * Local globals...
 */

static int		parent_signal = 0;
					/* Set to signal number from child */
static int		holdcount = 0;	/* Number of times "hold" was called */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
static sigset_t		holdmask;	/* Old POSIX signal mask */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */
static int		dead_children = 0;
					/* Dead children? */
static int		stop_scheduler = 0;
					/* Should the scheduler stop? */

#if defined(__APPLE__) && defined(HAVE_DLFCN_H)
static const char *PSQLibPath = "/usr/lib/libPrintServiceQuota.dylib";
static const char *PSQLibFuncName = "PSQUpdateQuota";
static void *PSQLibRef;			/* libPrintServiceQuota.dylib */
#endif /* HAVE_DLFCN_H */


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
  cupsd_client_t	*con;		/* Current client */
  cupsd_job_t		*job;		/* Current job */
  cupsd_listener_t	*lis;		/* Current listener */
  time_t		current_time,	/* Current time */
			activity,	/* Client activity timer */
			browse_time,	/* Next browse send time */
			senddoc_time,	/* Send-Document time */
			expire_time,	/* Subscription expire time */
			report_time,	/* Malloc/client/job report time */
			event_time;	/* Last time an event notification was done */
  long			timeout;	/* Timeout for cupsdDoSelect() */
  struct rlimit		limit;		/* Runtime limit */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction	action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */
#ifdef __sgi
  cups_file_t		*fp;		/* Fake lpsched lock file */
  struct stat		statbuf;	/* Needed for checking lpsched FIFO */
#endif /* __sgi */
#ifdef __APPLE__
  int			run_as_child = 0;
					/* Needed for Mac OS X fork/exec */
#else
  time_t		netif_time = 0;	/* Time since last network update */
#endif /* __APPLE__ */
#if HAVE_LAUNCHD
  int			launchd_idle_exit;
					/* Idle exit on select timeout? */
#endif	/* HAVE_LAUNCHD */


#ifdef HAVE_GETEUID
 /*
  * Check for setuid invocation, which we do not support!
  */

  if (getuid() != geteuid())
  {
    fputs("cupsd: Cannot run as a setuid program!\n", stderr);
    return (1);
  }
#endif /* HAVE_GETEUID */

 /*
  * Check for command-line arguments...
  */

  fg = 0;

#ifdef HAVE_LAUNCHD
  if (getenv("CUPSD_LAUNCHD"))
  {
    Launchd = 1;
    fg      = 1;
  }
#endif /* HAVE_LAUNCHD */

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      for (opt = argv[i] + 1; *opt != '\0'; opt ++)
        switch (*opt)
	{
#ifdef __APPLE__
	  case 'C' : /* Run as child with config file */
              run_as_child = 1;
	      fg           = -1;
#endif /* __APPLE__ */

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

                if ((current = malloc(1024)) == NULL)
		{
		  _cupsLangPuts(stderr,
		                _("cupsd: Unable to get current directory!\n"));
                  return (1);
		}

		if (!getcwd(current, 1024))
		{
		  _cupsLangPuts(stderr,
		                _("cupsd: Unable to get current directory!\n"));
                  free(current);
		  return (1);
		}

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

          case 't' : /* Test the cupsd.conf file... */
	      TestConfigFile = 1;
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

#ifdef __APPLE__
   /*
    * Since CoreFoundation has an overly-agressive check for whether a
    * process has forked but not exec'd (whether CF has been called or
    * not...), we now have to exec ourselves with the "-f" option to
    * eliminate their bogus warning messages.
    */

    execlp(argv[0], argv[0], "-C", ConfigurationFile, (char *)0);
    exit(errno);
#endif /* __APPLE__ */
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

    for (i = 0; i < limit.rlim_cur && i < 1024; i ++)
      close(i);

   /*
    * Redirect stdin/out/err to /dev/null...
    */

    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);
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

#if !defined(HAVE_POLL) && !defined(HAVE_EPOLL) && !defined(HAVE_KQUEUE)
  if (limit.rlim_max > FD_SETSIZE)
    MaxFDs = FD_SETSIZE;
  else
#endif /* !HAVE_POLL && !HAVE_EPOLL && !HAVE_KQUEUE */
#ifdef RLIM_INFINITY
  if (limit.rlim_max == RLIM_INFINITY)
    MaxFDs = 16384;
  else
#endif /* RLIM_INFINITY */
    MaxFDs = limit.rlim_max;

  limit.rlim_cur = MaxFDs;

  setrlimit(RLIMIT_NOFILE, &limit);

  cupsdStartSelect();

 /*
  * Read configuration...
  */

  if (!cupsdReadConfiguration())
  {
    if (TestConfigFile)
      printf("%s contains errors\n", ConfigurationFile);
    else
      syslog(LOG_LPR, "Unable to read configuration file \'%s\' - exiting!",
	     ConfigurationFile);
    return (1);
  }
  else if (TestConfigFile)
  {
    printf("%s is OK\n", ConfigurationFile);
    return (0);
  }

  if (!strncmp(TempDir, RequestRoot, strlen(RequestRoot)))
  {
   /*
    * Clean out the temporary directory...
    */

    cups_dir_t		*dir;		/* Temporary directory */
    cups_dentry_t	*dent;		/* Directory entry */
    char		tempfile[1024];	/* Temporary filename */


    if ((dir = cupsDirOpen(TempDir)) != NULL)
    {
      cupsdLogMessage(CUPSD_LOG_INFO,
                      "Cleaning out old temporary files in \"%s\"...", TempDir);

      while ((dent = cupsDirRead(dir)) != NULL)
      {
        snprintf(tempfile, sizeof(tempfile), "%s/%s", TempDir, dent->filename);

	if (cupsdRemoveFile(tempfile))
	  cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "Unable to remove temporary file \"%s\" - %s",
	                  tempfile, strerror(errno));
        else
	  cupsdLogMessage(CUPSD_LOG_DEBUG, "Removed temporary file \"%s\"...",
	                  tempfile);
      }

      cupsDirClose(dir);
    }
    else
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to open temporary directory \"%s\" - %s",
                      TempDir, strerror(errno));
  }

#if HAVE_LAUNCHD
  if (Launchd)
  {
   /*
    * If we were started by launchd get the listen sockets file descriptors...
    */

    launchd_checkin();
  }
#endif /* HAVE_LAUNCHD */

#if defined(__APPLE__) && defined(HAVE_DLFCN_H)
 /*
  * Load Print Service quota enforcement library (X Server only)
  */

  PSQLibRef = dlopen(PSQLibPath, RTLD_LAZY);

  if (PSQLibRef)
    PSQUpdateQuotaProc = dlsym(PSQLibRef, PSQLibFuncName);
#endif /* __APPLE__ && HAVE_DLFCN_H */

#ifdef HAVE_GSSAPI
#  ifdef __APPLE__
 /*
  * If the weak-linked GSSAPI/Kerberos library is not present, don't try
  * to use it...
  */

  if (krb5_init_context != NULL)
#  endif /* __APPLE__ */

 /*
  * Setup a Kerberos context for the scheduler to use...
  */

  if (krb5_init_context(&KerberosContext))
  {
    KerberosContext = NULL;

    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to initialize Kerberos context");
  }
#endif /* HAVE_GSSAPI */

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

#ifdef __APPLE__
  if (!fg || run_as_child)
#else
  if (!fg)
#endif /* __APPLE__ */
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

  current_time  = time(NULL);
  browse_time   = current_time;
  event_time    = current_time;
  expire_time   = current_time;
  fds           = 1;
  report_time   = 0;
  senddoc_time  = current_time;

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
	 /*
	  * If we were started by launchd get the listen sockets file descriptors...
	  */

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
    * Check for available input or ready output.  If cupsdDoSelect()
    * returns 0 or -1, something bad happened and we should exit
    * immediately.
    *
    * Note that we at least have one listening socket open at all
    * times.
    */

    if ((timeout = select_timeout(fds)) > 1 && LastEvent)
      timeout = 1;

#if HAVE_LAUNCHD
   /*
    * If no other work is scheduled and we're being controlled by
    * launchd then timeout after 'LaunchdTimeout' seconds of
    * inactivity...
    */

    if (timeout == 86400 && Launchd && LaunchdTimeout && !NumPolled &&
        !cupsArrayCount(ActiveJobs) &&
	(!Browsing || 
	 (!BrowseRemoteProtocols && 
	  (!NumBrowsers || !BrowseLocalProtocols ||
	   cupsArrayCount(Printers) == 0))))
    {
      timeout		= LaunchdTimeout;
      launchd_idle_exit = 1;
    }
    else
      launchd_idle_exit = 0;
#endif	/* HAVE_LAUNCHD */

    if ((fds = cupsdDoSelect(timeout)) < 0)
    {
     /*
      * Got an error from select!
      */

#ifdef HAVE_DNSSD
      cupsd_printer_t	*p;		/* Current printer */
#endif /* HAVE_DNSSD */


      if (errno == EINTR)		/* Just interrupted by a signal */
        continue;

     /*
      * Log all sorts of debug info to help track down the problem.
      */

      cupsdLogMessage(CUPSD_LOG_EMERG, "cupsdDoSelect() failed - %s!",
                      strerror(errno));

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

#ifdef HAVE_DNSSD
      for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
	   p;
	   p = (cupsd_printer_t *)cupsArrayNext(Printers))
        cupsdLogMessage(CUPSD_LOG_EMERG, "printer[%s] %d", p->name,
	                p->dnssd_ipp_fd);
#endif /* HAVE_DNSSD */

      break;
    }

    current_time = time(NULL);

#ifndef __APPLE__
   /*
    * Update the network interfaces once a minute...
    */

    if ((current_time - netif_time) >= 60)
    {
      netif_time  = current_time;
      NetIFUpdate = 1;
    }
#endif /* !__APPLE__ */

#if HAVE_LAUNCHD
   /*
    * If no other work was scheduled and we're being controlled by launchd
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
    * Resume listening for new connections as needed...
    */

    if (ListeningPaused && ListeningPaused <= current_time &&
        cupsArrayCount(Clients) < MaxClients)
      cupsdResumeListening();

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

    if (Browsing)
    {
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
      cupsdAddCert(0, "root", NULL);
    }

   /*
    * Check for new data on the client sockets...
    */

    for (con = (cupsd_client_t *)cupsArrayFirst(Clients);
	 con;
	 con = (cupsd_client_t *)cupsArrayNext(Clients))
    {
     /*
      * Process pending data in the input buffer...
      */

      if (con->http.used)
      {
        cupsdReadClient(con);
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
    * Log statistics at most once a minute when in debug mode...
    */

    if ((current_time - report_time) >= 60 && LogLevel >= CUPSD_LOG_DEBUG)
    {
      size_t		string_count,	/* String count */
			alloc_bytes,	/* Allocated string bytes */
			total_bytes;	/* Total string bytes */
#ifdef HAVE_MALLINFO
      struct mallinfo	mem;		/* Malloc information */


      mem = mallinfo();
      cupsdLogMessage(CUPSD_LOG_DEBUG, "Report: malloc-arena=%lu", mem.arena);
      cupsdLogMessage(CUPSD_LOG_DEBUG, "Report: malloc-used=%lu",
                      mem.usmblks + mem.uordblks);
      cupsdLogMessage(CUPSD_LOG_DEBUG, "Report: malloc-free=%lu",
		      mem.fsmblks + mem.fordblks);
#endif /* HAVE_MALLINFO */

      cupsdLogMessage(CUPSD_LOG_DEBUG, "Report: clients=%d",
                      cupsArrayCount(Clients));
      cupsdLogMessage(CUPSD_LOG_DEBUG, "Report: jobs=%d",
                      cupsArrayCount(Jobs));
      cupsdLogMessage(CUPSD_LOG_DEBUG, "Report: jobs-active=%d",
                      cupsArrayCount(ActiveJobs));
      cupsdLogMessage(CUPSD_LOG_DEBUG, "Report: printers=%d",
                      cupsArrayCount(Printers));
      cupsdLogMessage(CUPSD_LOG_DEBUG, "Report: printers-implicit=%d",
                      cupsArrayCount(ImplicitPrinters));

      string_count = _cupsStrStatistics(&alloc_bytes, &total_bytes);
      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "Report: stringpool-string-count=" CUPS_LLFMT,
		      CUPS_LLCAST string_count);
      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "Report: stringpool-alloc-bytes=" CUPS_LLFMT,
		      CUPS_LLCAST alloc_bytes);
      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "Report: stringpool-total-bytes=" CUPS_LLFMT,
		      CUPS_LLCAST total_bytes);

      report_time = current_time;
    }

   /*
    * Handle OS-specific event notification for any events that have
    * accumulated.  Don't send these more than once a second...
    */

    if (LastEvent && (current_time - event_time) >= 1)
    {
#ifdef HAVE_NOTIFY_POST
      if (LastEvent & (CUPSD_EVENT_PRINTER_ADDED |
                       CUPSD_EVENT_PRINTER_DELETED |
                       CUPSD_EVENT_PRINTER_MODIFIED))
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

      LastEvent  = CUPSD_EVENT_NONE;
      event_time = current_time;
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
  * Close all network clients...
  */

  cupsdStopServer();

#ifdef HAVE_LAUNCHD
 /*
  * Update the launchd KeepAlive file as needed...
  */

  if (Launchd)
    launchd_checkout();
#endif /* HAVE_LAUNCHD */

 /*
  * Stop all jobs...
  */

  cupsdFreeAllJobs();

#ifdef __APPLE__
 /*
  * Stop monitoring system event monitoring...
  */

  cupsdStopSystemMonitor();
#endif /* __APPLE__ */

#ifdef HAVE_GSSAPI
 /*
  * Free the scheduler's Kerberos context...
  */

#  ifdef __APPLE__
 /*
  * If the weak-linked GSSAPI/Kerberos library is not present, don't try
  * to use it...
  */

  if (krb5_init_context != NULL)
#  endif /* __APPLE__ */
  if (KerberosContext)
    krb5_free_context(KerberosContext);
#endif /* HAVE_GSSAPI */

#ifdef __APPLE__
#ifdef HAVE_DLFCN_H
 /* 
  * Unload Print Service quota enforcement library (X Server only) 
  */

  PSQUpdateQuotaProc = NULL;
  if (PSQLibRef)
  {
    dlclose(PSQLibRef);
    PSQLibRef = NULL;
  }
#endif /* HAVE_DLFCN_H */
#endif	/* __APPLE__ */

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

  cupsdStopSelect();

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
  int			fd;		/* File descriptor */
  char			s[256];		/* String addresss */


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
    count = launch_data_array_get_count(ld_array);

    for (i = 0; i < count; i ++)
    {
     /*
      * Get the launchd file descriptor and address...
      */

      tmp     = launch_data_array_get_index(ld_array, i);
      fd      = launch_data_get_fd(tmp);
      addrlen = sizeof(addr);

      if (getsockname(fd, (struct sockaddr *)&addr, &addrlen))
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "launchd_checkin: Unable to get local address - %s",
			strerror(errno));
	continue;
      }

     /*
      * Try to match the launchd socket address to one of the listeners...
      */

      for (lis = (cupsd_listener_t *)cupsArrayFirst(Listeners);
	   lis;
	   lis = (cupsd_listener_t *)cupsArrayNext(Listeners))
	if (httpAddrEqual(&lis->address, &addr))
	  break;

     /*
      * Add a new listener If there's no match...
      */

      if (lis)
      {
	cupsdLogMessage(CUPSD_LOG_DEBUG, 
		"launchd_checkin: Matched existing listener %s with fd %d...",
		httpAddrString(&(lis->address), s, sizeof(s)), fd);
      }
      else
      {
	cupsdLogMessage(CUPSD_LOG_DEBUG, 
		"launchd_checkin: Adding new listener %s with fd %d...",
		httpAddrString(&addr, s, sizeof(s)), fd);

        if ((lis = calloc(1, sizeof(cupsd_listener_t))) == NULL)
        {
	  cupsdLogMessage(CUPSD_LOG_ERROR,
               	          "launchd_checkin: Unable to allocate listener - %s.",
                	  strerror(errno));
	  exit(EXIT_FAILURE);
        }

        cupsArrayAdd(Listeners, lis);

	memcpy(&lis->address, &addr, sizeof(lis->address));
      }

      lis->fd = fd;

#  ifdef HAVE_SSL
      portnum = 0;

#    ifdef AF_INET6
      if (lis->address.addr.sa_family == AF_INET6)
	portnum = ntohs(lis->address.ipv6.sin6_port);
      else
#    endif /* AF_INET6 */
      if (lis->address.addr.sa_family == AF_INET)
	portnum = ntohs(lis->address.ipv4.sin_port);

      if (portnum == 443)
	lis->encryption = HTTP_ENCRYPT_ALWAYS;
#  endif /* HAVE_SSL */
    }
  }

  launch_data_free(ld_msg);
  launch_data_free(ld_resp);
}


/*
 * 'launchd_checkout()' - Update the launchd KeepAlive file as needed.
 */

static void
launchd_checkout(void)
{
  int	fd;				/* File descriptor */


 /*
  * Create or remove the launchd KeepAlive file based on whether
  * there are active jobs, polling, browsing for remote printers or 
  * shared printers to advertise...
  */

  if ((cupsArrayCount(ActiveJobs) || NumPolled || 
       (Browsing && 
	(BrowseRemoteProtocols ||
        (BrowseLocalProtocols && NumBrowsers && cupsArrayCount(Printers))))))
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG,
                    "Creating launchd keepalive file \"" CUPS_KEEPALIVE "\"...");

    if ((fd = open(CUPS_KEEPALIVE, O_RDONLY | O_CREAT | O_EXCL, S_IRUSR)) >= 0)
      close(fd);
  }
  else
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG,
                    "Removing launchd keepalive file \"" CUPS_KEEPALIVE "\"...");

    unlink(CUPS_KEEPALIVE);
  }
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
    * Ignore SIGTERM errors - that comes when a job is canceled...
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

            if (job->printer && !(job->printer->type & CUPS_PRINTER_FAX) &&
	        job->status_level > CUPSD_LOG_ERROR)
	    {
	      job->status_level = CUPSD_LOG_ERROR;

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
sigchld_handler(int sig)		/* I - Signal number */
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
sighup_handler(int sig)			/* I - Signal number */
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
sigterm_handler(int sig)		/* I - Signal number */
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
  * If select has been active in the last second (fds > 0) or we have
  * many resources in use then don't bother trying to optimize the
  * timeout, just make it 1 second.
  */

  if (fds > 0 || cupsArrayCount(Clients) > 50)
    return (1);

 /*
  * Otherwise, check all of the possible events that we need to wake for...
  */

  now     = time(NULL);
  timeout = now + 86400;		/* 86400 == 1 day */
  why     = "do nothing";

 /*
  * Check whether we are accepting new connections...
  */

  if (ListeningPaused > 0 && cupsArrayCount(Clients) < MaxClients &&
      ListeningPaused < timeout)
  {
    if (ListeningPaused <= now)
      timeout = now;
    else
      timeout = ListeningPaused;

    why = "resume listening";
  }

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

    if ((BrowseLocalProtocols & BROWSE_CUPS) && NumBrowsers)
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
	else if (p->shared && !(p->type & CUPS_PRINTER_IMPLICIT))
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

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "select_timeout(%d): %ld seconds to %s",
                  fds, timeout, why);

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
