/*
 * Main loop for the CUPS scheduler.
 *
 * Copyright © 2007-2019 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products, all rights reserved.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#define _MAIN_C_
#include "cupsd.h"
#include <sys/resource.h>
#ifdef __APPLE__
#  include <xpc/xpc.h>
#  include <pthread/qos.h>
#endif /* __APPLE__ */
#ifdef HAVE_ASL_H
#  include <asl.h>
#elif defined(HAVE_SYSTEMD_SD_JOURNAL_H)
#  define SD_JOURNAL_SUPPRESS_LOCATION
#  include <systemd/sd-journal.h>
#endif /* HAVE_ASL_H */
#include <syslog.h>
#include <grp.h>

#ifdef HAVE_LAUNCH_H
#  include <launch.h>
#endif /* HAVE_LAUNCH_H */

#ifdef HAVE_SYSTEMD
#  include <systemd/sd-daemon.h>
#endif /* HAVE_SYSTEMD */

#ifdef HAVE_ONDEMAND
#  define CUPS_KEEPALIVE CUPS_CACHEDIR "/org.cups.cupsd"
					/* Name of the KeepAlive file */
#endif /* HAVE_ONDEMAND */

#if defined(HAVE_MALLOC_H) && defined(HAVE_MALLINFO)
#  include <malloc.h>
#endif /* HAVE_MALLOC_H && HAVE_MALLINFO */

#ifdef HAVE_NOTIFY_H
#  include <notify.h>
#endif /* HAVE_NOTIFY_H */

#ifdef HAVE_DBUS
#  include <dbus/dbus.h>
#endif /* HAVE_DBUS */

#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */


/*
 * Local functions...
 */

static void		parent_handler(int sig);
static void		process_children(void);
static void		sigchld_handler(int sig);
static void		sighup_handler(int sig);
static void		sigterm_handler(int sig);
static long		select_timeout(int fds);
static void		service_checkin(void);
static void		service_checkout(int shutdown);
static void		usage(int status) _CUPS_NORETURN;


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
static time_t           local_timeout = 0;
                                        /* Next local printer timeout */


/*
 * 'main()' - Main entry for the CUPS scheduler.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int			i;		/* Looping var */
  char			*opt;		/* Option character */
  int			close_all = 1,	/* Close all file descriptors? */
			disconnect = 1,	/* Disconnect from controlling terminal? */
			fg = 0,		/* Run in foreground? */
			run_as_child = 0,
					/* Running as child process? */
			print_profile = 0;
					/* Print the sandbox profile to stdout? */
  int			fds;		/* Number of ready descriptors */
  cupsd_client_t	*con;		/* Current client */
  cupsd_job_t		*job;		/* Current job */
  cupsd_listener_t	*lis;		/* Current listener */
  time_t		current_time,	/* Current time */
			activity,	/* Client activity timer */
			senddoc_time,	/* Send-Document time */
			expire_time,	/* Subscription expire time */
			report_time,	/* Malloc/client/job report time */
			event_time;	/* Last event notification time */
  long			timeout;	/* Timeout for cupsdDoSelect() */
  struct rlimit		limit;		/* Runtime limit */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction	action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */
#ifdef __APPLE__
  int			use_sysman = 1;	/* Use system management functions? */
#else
  time_t		netif_time = 0;	/* Time since last network update */
#endif /* __APPLE__ */
#if defined(HAVE_ONDEMAND)
  int			service_idle_exit = 0;
					/* Idle exit on select timeout? */
#endif /* HAVE_ONDEMAND */


#ifdef HAVE_GETEUID
 /*
  * Check for setuid invocation, which we do not support!
  */

  if (getuid() != geteuid())
  {
    fputs("cupsd: Cannot run as a setuid program.\n", stderr);
    return (1);
  }
#endif /* HAVE_GETEUID */

 /*
  * Check for command-line arguments...
  */

  fg = 0;

  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "--help"))
      usage(0);
    else if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt != '\0'; opt ++)
      {
        switch (*opt)
	{
	  case 'C' : /* Run as child with config file */
              run_as_child = 1;
	      fg           = 1;
	      close_all    = 0;

	  case 'c' : /* Configuration file */
	      i ++;
	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr, _("cupsd: Expected config filename "
		                        "after \"-c\" option."));
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
		                _("cupsd: Unable to get current directory."));
                  return (1);
		}

		if (!getcwd(current, 1024))
		{
		  _cupsLangPuts(stderr,
		                _("cupsd: Unable to get current directory."));
                  free(current);
		  return (1);
		}

		cupsdSetStringf(&ConfigurationFile, "%s/%s", current, argv[i]);
		free(current);
              }
	      break;

          case 'f' : /* Run in foreground... */
	      fg         = 1;
	      disconnect = 0;
	      close_all  = 0;
	      break;

          case 'F' : /* Run in foreground, but disconnect from terminal... */
	      fg        = 1;
	      close_all = 0;
	      break;

          case 'h' : /* Show usage/help */
	      usage(0);

          case 'l' : /* Started by launchd/systemd/upstart... */
#ifdef HAVE_ONDEMAND
	      OnDemand   = 1;
	      fg         = 1;
	      close_all  = 0;
	      disconnect = 0;
#else
	      _cupsLangPuts(stderr, _("cupsd: On-demand support not compiled "
	                              "in, running in normal mode."));
              fg         = 0;
	      disconnect = 1;
	      close_all  = 1;
#endif /* HAVE_ONDEMAND */
	      break;

          case 'p' : /* Stop immediately for profiling */
              fputs("cupsd: -p (startup profiling) is for internal testing "
                    "use only!\n", stderr);
	      stop_scheduler = 1;
	      fg             = 1;
	      disconnect     = 0;
	      close_all      = 0;
	      break;

          case 'P' : /* Disable security profiles */
              fputs("cupsd: -P (disable sandboxing) is for internal testing use only.\n", stderr);
	      UseSandboxing = 0;
	      break;

          case 's' : /* Set cups-files.conf location */
              i ++;
	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr, _("cupsd: Expected cups-files.conf "
	                                "filename after \"-s\" option."));
	        usage(1);
	      }

              if (argv[i][0] != '/')
	      {
	       /*
	        * Relative filename not allowed...
		*/

	        _cupsLangPuts(stderr, _("cupsd: Relative cups-files.conf "
	                                "filename not allowed."));
	        usage(1);
              }

	      cupsdSetString(&CupsFilesFile, argv[i]);
	      break;

#ifdef __APPLE__
          case 'S' : /* Disable system management functions */
              fputs("cupsd: -S (disable system management) for internal "
                    "testing use only!\n", stderr);
	      use_sysman = 0;
	      break;
#endif /* __APPLE__ */

          case 't' : /* Test the cupsd.conf file... */
	      TestConfigFile = 1;
	      fg             = 1;
	      disconnect     = 0;
	      close_all      = 0;
	      break;

          case 'T' : /* Print security profile */
              print_profile = 1;
              fg            = 1;
              disconnect    = 0;
              close_all     = 0;
              break;

	  default : /* Unknown option */
              _cupsLangPrintf(stderr, _("cupsd: Unknown option \"%c\" - "
	                                "aborting."), *opt);
	      usage(1);
	}
      }
    }
    else
    {
      _cupsLangPrintf(stderr, _("cupsd: Unknown argument \"%s\" - aborting."),
                      argv[i]);
      usage(1);
    }
  }

  if (!ConfigurationFile)
    cupsdSetString(&ConfigurationFile, CUPS_SERVERROOT "/cupsd.conf");

  if (!CupsFilesFile)
  {
    char	*filename,		/* Copy of cupsd.conf filename */
		*slash;			/* Final slash in cupsd.conf filename */
    size_t	len;			/* Size of buffer */

    len = strlen(ConfigurationFile) + 15;
    if ((filename = malloc(len)) == NULL)
    {
      _cupsLangPrintf(stderr,
		      _("cupsd: Unable to get path to "
			"cups-files.conf file."));
      return (1);
    }

    strlcpy(filename, ConfigurationFile, len);
    if ((slash = strrchr(filename, '/')) == NULL)
    {
      free(filename);
      _cupsLangPrintf(stderr,
		      _("cupsd: Unable to get path to "
			"cups-files.conf file."));
      return (1);
    }

    strlcpy(slash, "/cups-files.conf", len - (size_t)(slash - filename));
    cupsdSetString(&CupsFilesFile, filename);
    free(filename);
  }

  if (disconnect)
  {
   /*
    * Make sure we aren't tying up any filesystems...
    */

    chdir("/");

   /*
    * Disconnect from the controlling terminal...
    */

    setsid();
  }

  if (close_all)
  {
   /*
    * Close all open files...
    */

    getrlimit(RLIMIT_NOFILE, &limit);

    for (i = 0; i < (int)limit.rlim_cur && i < 1024; i ++)
      close(i);

   /*
    * Redirect stdin/out/err to /dev/null...
    */

    if ((i = open("/dev/null", O_RDONLY)) != 0)
    {
      dup2(i, 0);
      close(i);
    }

    if ((i = open("/dev/null", O_WRONLY)) != 1)
    {
      dup2(i, 1);
      close(i);
    }

    if ((i = open("/dev/null", O_WRONLY)) != 2)
    {
      dup2(i, 2);
      close(i);
    }
  }
  else
    LogStderr = cupsFileStderr();

 /*
  * Run in the background as needed...
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
        fprintf(stderr, "cupsd: Child exited with status %d\n",
	        WEXITSTATUS(i));
	return (2);
      }
      else
      {
        fprintf(stderr, "cupsd: Child exited on signal %d\n", WTERMSIG(i));
	return (3);
      }
    }

#if defined(__OpenBSD__) && OpenBSD < 201211
   /*
    * Call _thread_sys_closefrom() so the child process doesn't reset the
    * parent's file descriptors to be blocking.  This is a workaround for a
    * limitation of userland libpthread on older versions of OpenBSD.
    */

    _thread_sys_closefrom(0);
#endif /* __OpenBSD__ && OpenBSD < 201211 */

   /*
    * Since many system libraries create fork-unsafe data on execution of a
    * program, we need to re-execute the background cupsd with the "-C" and "-s"
    * options to avoid problems.  Unfortunately, we also have to assume that
    * argv[0] contains the name of the cupsd executable - there is no portable
    * way to get the real pathname...
    */

    execlp(argv[0], argv[0], "-C", ConfigurationFile, "-s", CupsFilesFile, (char *)0);
    exit(errno);
  }

 /*
  * Let the system know we are busy while we bring up cupsd...
  */

  cupsdSetBusyState(1);

 /*
  * Set the timezone info...
  */

  tzset();

#ifdef LC_TIME
  setlocale(LC_TIME, "");
#endif /* LC_TIME */

#ifdef HAVE_DBUS_THREADS_INIT
 /*
  * Enable threading support for D-BUS...
  */

  dbus_threads_init_default();
#endif /* HAVE_DBUS_THREADS_INIT */

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

  limit.rlim_cur = (rlim_t)MaxFDs;

  setrlimit(RLIMIT_NOFILE, &limit);

  cupsdStartSelect();

 /*
  * Read configuration...
  */

  if (!cupsdReadConfiguration())
    return (1);
  else if (TestConfigFile)
  {
    printf("\"%s\" is OK.\n", CupsFilesFile);
    printf("\"%s\" is OK.\n", ConfigurationFile);
    return (0);
  }
  else if (print_profile)
  {
    cups_file_t	*fp;			/* File pointer */
    const char	*profile = cupsdCreateProfile(42, 0);
					/* Profile */
    char	line[1024];		/* Line from file */


    if ((fp = cupsFileOpen(profile, "r")) == NULL)
    {
      printf("Unable to open profile file \"%s\": %s\n", profile ? profile : "(null)", strerror(errno));
      return (1);
    }

    while (cupsFileGets(fp, line, sizeof(line)))
      puts(line);

    cupsFileClose(fp);

    return (0);
  }

 /*
  * Clean out old temp files and printer cache data.
  */

  if (!RequestRoot || !strncmp(TempDir, RequestRoot, strlen(RequestRoot)))
    cupsdCleanFiles(TempDir, NULL);

  cupsdCleanFiles(CacheDir, "*.ipp");

 /*
  * If we were started on demand by launchd or systemd get the listen sockets
  * file descriptors...
  */

  service_checkin();
  service_checkout(0);

 /*
  * Startup the server...
  */

  httpInitialize();

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

 /*
  * Initialize authentication certificates...
  */

  cupsdInitCerts();

 /*
  * If we are running in the background, signal the parent process that
  * we are up and running...
  */

  if (!fg || run_as_child)
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

  if (use_sysman)
    cupsdStartSystemMonitor();
#endif /* __APPLE__ */

 /*
  * Send server-started event...
  */

#ifdef HAVE_ONDEMAND
  if (OnDemand)
    cupsdAddEvent(CUPSD_EVENT_SERVER_STARTED, NULL, NULL, "Scheduler started on demand.");
  else
#endif /* HAVE_ONDEMAND */
  if (fg)
    cupsdAddEvent(CUPSD_EVENT_SERVER_STARTED, NULL, NULL, "Scheduler started in foreground.");
  else
    cupsdAddEvent(CUPSD_EVENT_SERVER_STARTED, NULL, NULL, "Scheduler started in background.");

  cupsdSetBusyState(0);

 /*
  * Start any pending print jobs...
  */

  cupsdCheckJobs();

 /*
  * Loop forever...
  */

  current_time  = time(NULL);
  event_time    = current_time;
  expire_time   = current_time;
  local_timeout = 0;
  fds           = 1;
  report_time   = 0;
  senddoc_time  = current_time;

  while (!stop_scheduler)
  {
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
	  if (httpGetState(con->http) == HTTP_WAITING)
	    cupsdCloseClient(con);
	  else
	    con->http->keep_alive = HTTP_KEEPALIVE_OFF;

        cupsdPauseListening();
      }

     /*
      * Restart if all clients are closed and all jobs finished, or
      * if the reload timeout has elapsed...
      */

      if ((cupsArrayCount(Clients) == 0 &&
           (cupsArrayCount(PrintingJobs) == 0 || NeedReload != RELOAD_ALL)) ||
          (time(NULL) - ReloadTime) >= ReloadTimeout)
      {
       /*
	* Shutdown the server...
	*/

#ifdef HAVE_ONDEMAND
	if (OnDemand)
	{
#  ifndef HAVE_SYSTEMD /* Issue #5640: systemd doesn't actually support launch-on-demand services, need to fake it */
	  stop_scheduler = 1;
#  endif /* HAVE_SYSTEMD */
	  break;
	}
#endif /* HAVE_ONDEMAND */

        DoingShutdown = 1;

	cupsdStopServer();

       /*
	* Read configuration...
	*/

        if (!cupsdReadConfiguration())
        {
#ifdef HAVE_SYSTEMD_SD_JOURNAL_H
	  sd_journal_print(LOG_ERR, "Unable to read configuration file \"%s\" - exiting.", ConfigurationFile);
#else
          syslog(LOG_LPR, "Unable to read configuration file \'%s\' - exiting.", ConfigurationFile);
#endif /* HAVE_SYSTEMD_SD_JOURNAL_H */

          break;
	}

       /*
        * Startup the server...
        */

        DoingShutdown = 0;

        cupsdStartServer();

       /*
        * Send a server-restarted event...
	*/

        cupsdAddEvent(CUPSD_EVENT_SERVER_RESTARTED, NULL, NULL,
                      "Scheduler restarted.");
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

#ifdef HAVE_ONDEMAND
   /*
    * If no other work is scheduled and we're being controlled by launchd,
    * systemd, etc. then timeout after 'IdleExitTimeout' seconds of
    * inactivity...
    */

    if (timeout == 86400 && OnDemand && IdleExitTimeout &&
#  ifdef HAVE_SYSTEMD
        !WebInterface &&
#  endif /* HAVE_SYSTEMD */
        !cupsArrayCount(ActiveJobs))
    {
      cupsd_printer_t *p = NULL;	/* Current printer */

      if (Browsing && BrowseLocalProtocols)
      {
        for (p = (cupsd_printer_t *)cupsArrayFirst(Printers); p; p = (cupsd_printer_t *)cupsArrayNext(Printers))
          if (p->shared)
            break;
      }

      if (!p)
      {
	timeout		  = IdleExitTimeout;
	service_idle_exit = 1;
      }
    }
    else
      service_idle_exit = 0;
#endif	/* HAVE_ONDEMAND */

    if ((fds = cupsdDoSelect(timeout)) < 0)
    {
     /*
      * Got an error from select!
      */

#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
      cupsd_printer_t	*p;		/* Current printer */
#endif /* HAVE_DNSSD || HAVE_AVAHI */

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
	                i, con->number, con->file, httpGetState(con->http));

      for (i = 0, lis = (cupsd_listener_t *)cupsArrayFirst(Listeners);
           lis;
	   i ++, lis = (cupsd_listener_t *)cupsArrayNext(Listeners))
        cupsdLogMessage(CUPSD_LOG_EMERG, "Listeners[%d] = %d", i, lis->fd);

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

#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
      for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
	   p;
	   p = (cupsd_printer_t *)cupsArrayNext(Printers))
        cupsdLogMessage(CUPSD_LOG_EMERG, "printer[%s] reg_name=\"%s\"", p->name,
	                p->reg_name ? p->reg_name : "(null)");
#endif /* HAVE_DNSSD || HAVE_AVAHI */

      break;
    }

    current_time = time(NULL);

   /*
    * Write dirty config/state files...
    */

    if (DirtyCleanTime && current_time >= DirtyCleanTime)
      cupsdCleanDirty();

#ifdef __APPLE__
   /*
    * If we are going to sleep and still have pending jobs, stop them after
    * a period of time...
    */

    if (SleepJobs > 0 && current_time >= SleepJobs &&
        cupsArrayCount(PrintingJobs) > 0)
    {
      SleepJobs = 0;
      cupsdStopAllJobs(CUPSD_JOB_DEFAULT, 5);
    }
#endif /* __APPLE__ */

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

#ifdef HAVE_ONDEMAND
   /*
    * If no other work was scheduled and we're being controlled by launchd,
    * systemd, or upstart then timeout after 'LaunchdTimeout' seconds of
    * inactivity...
    */

    if (!fds && service_idle_exit)
    {
      cupsdLogMessage(CUPSD_LOG_INFO,
                      "Printer sharing is off and there are no jobs pending, "
		      "will restart on demand.");
      stop_scheduler = 1;
      break;
    }
#endif /* HAVE_ONDEMAND */

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
      cupsdExpireSubscriptions(NULL, NULL);

      cupsdUnloadCompletedJobs();

      expire_time = current_time;
    }

   /*
    * Delete stale local printers...
    */

    if (current_time >= local_timeout)
    {
      cupsdDeleteTemporaryPrinters(0);
      local_timeout = 0;
    }

#ifndef HAVE_AUTHORIZATION_H
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
      cupsdAddCert(0, "root", cupsdDefaultAuthType());
    }
#endif /* !HAVE_AUTHORIZATION_H */

   /*
    * Clean job history...
    */

    if (JobHistoryUpdate && current_time >= JobHistoryUpdate)
      cupsdCleanJobs();

   /*
    * Update any pending multi-file documents...
    */

    if ((current_time - senddoc_time) >= 10)
    {
      cupsdCheckJobs();
      senddoc_time = current_time;
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

      if (httpGetReady(con->http))
      {
        cupsdReadClient(con);
	continue;
      }

     /*
      * Check the activity and close old clients...
      */

      activity = current_time - Timeout;
      if (httpGetActivity(con->http) < activity && !con->pipe_pid)
      {
        cupsdLogMessage(CUPSD_LOG_DEBUG, "Closing client %d after %d seconds of inactivity.", con->number, Timeout);

        cupsdCloseClient(con);
        continue;
      }
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
  {
    cupsdLogMessage(CUPSD_LOG_INFO, "Scheduler shutting down normally.");
    cupsdAddEvent(CUPSD_EVENT_SERVER_STOPPED, NULL, NULL,
                  "Scheduler shutting down normally.");
  }
  else
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Scheduler shutting down due to program error.");
    cupsdAddEvent(CUPSD_EVENT_SERVER_STOPPED, NULL, NULL,
                  "Scheduler shutting down due to program error.");
  }

 /*
  * Close all network clients...
  */

  DoingShutdown = 1;

  cupsdStopServer();

 /*
  * Update the KeepAlive/PID file as needed...
  */

  service_checkout(1);

 /*
  * Stop all jobs...
  */

  cupsdFreeAllJobs();

 /*
  * Delete all temporary printers...
  */

  cupsdDeleteTemporaryPrinters(1);

#ifdef __APPLE__
 /*
  * Stop monitoring system event monitoring...
  */

  if (use_sysman)
    cupsdStopSystemMonitor();
#endif /* __APPLE__ */

  cupsdStopSelect();

  return (!stop_scheduler);
}


/*
 * 'cupsdAddString()' - Copy and add a string to an array.
 */

int					/* O  - 1 on success, 0 on failure */
cupsdAddString(cups_array_t **a,	/* IO - String array */
               const char   *s)		/* I  - String to copy and add */
{
  if (!*a)
    *a = cupsArrayNew3((cups_array_func_t)strcmp, NULL,
		       (cups_ahash_func_t)NULL, 0,
		       (cups_acopy_func_t)strdup,
		       (cups_afree_func_t)free);

  return (cupsArrayAdd(*a, (char *)s));
}


/*
 * 'cupsdCheckProcess()' - Tell the main loop to check for dead children.
 */

void
cupsdCheckProcess(void)
{
 /*
  * Flag that we have dead children...
  */

  dead_children = 1;
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
 * 'cupsdFreeStrings()' - Free an array of strings.
 */

void
cupsdFreeStrings(cups_array_t **a)	/* IO - String array */
{
  if (*a)
  {
    cupsArrayDelete(*a);
    *a = NULL;
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
  char		v[65536 + 64];		/* Formatting string value */
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
  int		pid,			/* Process ID of child */
		job_id;			/* Job ID of child */
  cupsd_job_t	*job;			/* Current job */
  int		i;			/* Looping var */
  char		name[1024];		/* Process name */
  const char	*type;			/* Type of program */


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
    * Collect the name of the process that finished...
    */

    cupsdFinishProcess(pid, name, sizeof(name), &job_id);

   /*
    * Delete certificates for CGI processes...
    */

    if (pid)
      cupsdDeleteCert(pid);

   /*
    * Handle completed job filters...
    */

    if (job_id > 0)
      job = cupsdFindJob(job_id);
    else
      job  = NULL;

    if (job)
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
	{
	  job->filters[i] = -pid;
	  type            = "Filter";
	}
	else
	{
	  job->backend = -pid;
	  type         = "Backend";
	}

	if (status && status != SIGTERM && status != SIGKILL &&
	    status != SIGPIPE)
	{
	 /*
	  * An error occurred; save the exit status so we know to stop
	  * the printer or cancel the job when all of the filters finish...
	  *
	  * A negative status indicates that the backend failed and the
	  * printer needs to be stopped.
	  *
	  * In order to preserve the most serious status, we always log
	  * when a process dies due to a signal (e.g. SIGABRT, SIGSEGV,
	  * and SIGBUS) and prefer to log the backend exit status over a
	  * filter's.
	  */

	  int old_status = abs(job->status);

          if (WIFSIGNALED(status) ||	/* This process crashed, or */
              !job->status ||		/* No process had a status, or */
              (!job->filters[i] && WIFEXITED(old_status)))
          {				/* Backend and filter didn't crash */
	    if (job->filters[i])
	    {
	      job->status = status;	/* Filter failed */
	    }
	    else
	    {
	      job->status = -status;	/* Backend failed */

	      if (job->current_file < job->num_files)
	        cupsdSetJobState(job, IPP_JOB_ABORTED, CUPSD_JOB_FORCE, "Canceling multi-file job due to backend failure.");
	    }
          }

	  if (job->state_value == IPP_JOB_PROCESSING &&
	      job->status_level > CUPSD_LOG_ERROR &&
	      (job->filters[i] || !WIFEXITED(status)))
	  {
	    char	message[1024];	/* New printer-state-message */


	    job->status_level = CUPSD_LOG_ERROR;

	    snprintf(message, sizeof(message), "%s failed", type);

            if (job->printer)
	    {
	      strlcpy(job->printer->state_message, message,
		       sizeof(job->printer->state_message));
	    }

	    if (!job->attrs)
	      cupsdLoadJob(job);

	    if (!job->printer_message && job->attrs)
	    {
	      if ((job->printer_message =
	               ippFindAttribute(job->attrs, "job-printer-state-message",
					IPP_TAG_TEXT)) == NULL)
		job->printer_message = ippAddString(job->attrs, IPP_TAG_JOB,
		                                    IPP_TAG_TEXT,
						    "job-printer-state-message",
						    NULL, NULL);
	    }

	    if (job->printer_message)
	      ippSetString(job->attrs, &job->printer_message, 0, message);
	  }
	}

       /*
	* If this is not the last file in a job, see if all of the
	* filters are done, and if so move to the next file.
	*/

	if (job->state_value >= IPP_JOB_CANCELED)
	{
	 /*
	  * Remove the job from the active list if there are no processes still
	  * running for it...
	  */

	  for (i = 0; job->filters[i] < 0; i++);

	  if (!job->filters[i] && job->backend <= 0)
	    cupsArrayRemove(ActiveJobs, job);
	}
	else if (job->current_file < job->num_files && job->printer)
	{
	  for (i = 0; job->filters[i] < 0; i ++);

	  if (!job->filters[i] &&
	      (!job->printer->pc || !job->printer->pc->single_file ||
	       job->backend <= 0))
	  {
	   /*
	    * Process the next file...
	    */

	    cupsdContinueJob(job);
	  }
	}
      }
    }

   /*
    * Show the exit status as needed, ignoring SIGTERM and SIGKILL errors
    * since they come when we kill/end a process...
    */

    if (status == SIGTERM || status == SIGKILL)
    {
      cupsdLogJob(job, CUPSD_LOG_DEBUG,
		  "PID %d (%s) was terminated normally with signal %d.", pid,
		  name, status);
    }
    else if (status == SIGPIPE)
    {
      cupsdLogJob(job, CUPSD_LOG_DEBUG,
		  "PID %d (%s) did not catch or ignore signal %d.", pid, name,
		  status);
    }
    else if (status)
    {
      if (WIFEXITED(status))
      {
        int code = WEXITSTATUS(status);	/* Exit code */

        if (code > 100)
	  cupsdLogJob(job, CUPSD_LOG_DEBUG,
		      "PID %d (%s) stopped with status %d (%s)", pid, name,
		      code, strerror(code - 100));
	else
	  cupsdLogJob(job, CUPSD_LOG_DEBUG,
		      "PID %d (%s) stopped with status %d.", pid, name, code);
      }
      else
	cupsdLogJob(job, CUPSD_LOG_DEBUG, "PID %d (%s) crashed on signal %d.",
		    pid, name, WTERMSIG(status));

      if (LogLevel < CUPSD_LOG_DEBUG)
        cupsdLogJob(job, CUPSD_LOG_INFO,
		    "Hint: Try setting the LogLevel to \"debug\" to find out "
		    "more.");
    }
    else
      cupsdLogJob(job, CUPSD_LOG_DEBUG, "PID %d (%s) exited with no errors.",
		  pid, name);
  }

 /*
  * If wait*() is interrupted by a signal, tell main() to call us again...
  */

  if (pid < 0 && errno == EINTR)
    dead_children = 1;
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
  cupsd_job_t		*job;		/* Job information */
  const char		*why;		/* Debugging aid */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "select_timeout: JobHistoryUpdate=%ld",
		  (long)JobHistoryUpdate);

 /*
  * Check to see if any of the clients have pending data to be
  * processed; if so, the timeout should be 0...
  */

  for (con = (cupsd_client_t *)cupsArrayFirst(Clients);
       con;
       con = (cupsd_client_t *)cupsArrayNext(Clients))
    if (httpGetReady(con->http))
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

#ifdef __APPLE__
 /*
  * When going to sleep, wake up to abort jobs that don't complete in time.
  */

  if (SleepJobs > 0 && SleepJobs < timeout)
  {
    timeout = SleepJobs;
    why     = "abort jobs before sleeping";
  }
#endif /* __APPLE__ */

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
    if ((httpGetActivity(con->http) + Timeout) < timeout)
    {
      timeout = httpGetActivity(con->http) + Timeout;
      why     = "timeout a client connection";
    }

 /*
  * Write out changes to configuration and state files...
  */

  if (DirtyCleanTime && timeout > DirtyCleanTime)
  {
    timeout = DirtyCleanTime;
    why     = "write dirty config/state files";
  }

 /*
  * Check for any job activity...
  */

  for (job = (cupsd_job_t *)cupsArrayFirst(ActiveJobs);
       job;
       job = (cupsd_job_t *)cupsArrayNext(ActiveJobs))
  {
    if (job->cancel_time && job->cancel_time < timeout)
    {
      timeout = job->cancel_time;
      why     = "cancel stuck jobs";
    }

    if (job->kill_time && job->kill_time < timeout)
    {
      timeout = job->kill_time;
      why     = "kill unresponsive jobs";
    }

    if (job->state_value == IPP_JOB_HELD && job->hold_until < timeout)
    {
      timeout = job->hold_until;
      why     = "release held jobs";
    }

    if (job->state_value == IPP_JOB_PENDING && timeout > (now + 10))
    {
      timeout = now + 10;
      why     = "start pending jobs";
      break;
    }
  }

 /*
  * Adjust from absolute to relative time.  We add 1 second to the timeout since
  * events occur after the timeout expires, and limit the timeout to 86400
  * seconds (1 day) to avoid select() timeout limits present on some operating
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


#ifdef HAVE_ONDEMAND
/*
 * 'service_add_listener()' - Bind an open fd as a Listener.
 */

static void
service_add_listener(int fd,		/* I - Socket file descriptor */
                     int idx)		/* I - Listener number, for logging */
{
  cupsd_listener_t	*lis;		/* Listeners array */
  http_addr_t		addr;		/* Address variable */
  socklen_t		addrlen;	/* Length of address */
  char			s[256];		/* String addresss */


  addrlen = sizeof(addr);

  if (getsockname(fd, (struct sockaddr *)&addr, &addrlen))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "service_add_listener: Unable to get local address for listener #%d: %s", idx + 1, strerror(errno));
    return;
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG, "service_add_listener: Listener #%d at fd %d, \"%s\".", idx + 1, fd, httpAddrString(&addr, s, sizeof(s)));

 /*
  * Try to match the on-demand socket address to one of the listeners...
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
    cupsdLogMessage(CUPSD_LOG_DEBUG, "service_add_listener: Matched existing listener #%d to %s.", idx + 1, httpAddrString(&(lis->address), s, sizeof(s)));
  }
  else
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG, "service_add_listener: Adding new listener #%d for %s.", idx + 1, httpAddrString(&addr, s, sizeof(s)));

    if ((lis = calloc(1, sizeof(cupsd_listener_t))) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "service_add_listener: Unable to allocate listener: %s.", strerror(errno));
      exit(EXIT_FAILURE);
    }

    cupsArrayAdd(Listeners, lis);

    memcpy(&lis->address, &addr, sizeof(lis->address));
  }

  lis->fd        = fd;
  lis->on_demand = 1;

#  ifdef HAVE_SSL
  if (httpAddrPort(&(lis->address)) == 443)
    lis->encryption = HTTP_ENCRYPT_ALWAYS;
#  endif /* HAVE_SSL */
}
#endif /* HAVE_ONDEMAND */


/*
 * 'service_checkin()' - Check-in with launchd and collect the listening fds.
 */

static void
service_checkin(void)
{
  cupsdLogMessage(CUPSD_LOG_DEBUG, "service_checkin: pid=%d", (int)getpid());

#ifdef HAVE_LAUNCHD
  if (OnDemand)
  {
    int       error;                        /* Check-in error, if any */
    size_t    i,                            /* Looping var */
              count;                        /* Number of listeners */
    int       *ld_sockets;                  /* Listener sockets */

#  ifdef __APPLE__
   /*
    * Force "user initiated" priority for the main thread...
    */

    pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);
#  endif /* __APPLE__ */

   /*
    * Check-in with launchd...
    */

    if ((error = launch_activate_socket("Listeners", &ld_sockets, &count)) != 0)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "service_checkin: Unable to get listener sockets: %s", strerror(error));
      exit(EXIT_FAILURE);
    }

   /*
    * Try to match the launchd sockets to the cupsd listeners...
    */

    cupsdLogMessage(CUPSD_LOG_DEBUG, "service_checkin: %d listeners.", (int)count);

    for (i = 0; i < count; i ++)
      service_add_listener(ld_sockets[i], (int)i);

    free(ld_sockets);

#  ifdef __APPLE__
    xpc_transaction_begin();
#  endif /* __APPLE__ */
  }

#elif defined(HAVE_SYSTEMD)
  if (OnDemand)
  {
    int         i,                      /* Looping var */
                count;                  /* Number of listeners */

   /*
    * Check-in with systemd...
    */

    if ((count = sd_listen_fds(0)) < 0)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "service_checkin: Unable to get listener sockets: %s", strerror(-count));
      exit(EXIT_FAILURE);
      return; /* anti-compiler-warning */
    }

   /*
    * Try to match the systemd sockets to the cupsd listeners...
    */

    cupsdLogMessage(CUPSD_LOG_DEBUG, "service_checkin: %d listeners.", count);

    for (i = 0; i < count; i ++)
      service_add_listener(SD_LISTEN_FDS_START + i, i);
  }

#elif defined(HAVE_UPSTART)
  if (OnDemand)
  {
    const char    *e;                   /* Environment var */
    int           fd;                   /* File descriptor */


    if (!(e = getenv("UPSTART_EVENTS")))
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "service_checkin: We did not get started via Upstart.");
      exit(EXIT_FAILURE);
      return;
    }

    if (strcasecmp(e, "socket"))
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "service_checkin: We did not get triggered via an Upstart socket event.");
      exit(EXIT_FAILURE);
      return;
    }

    if ((e = getenv("UPSTART_FDS")) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "service_checkin: Unable to get listener sockets from UPSTART_FDS.");
      exit(EXIT_FAILURE);
      return;
    }

    cupsdLogMessage(CUPSD_LOG_DEBUG, "service_checkin: UPSTART_FDS=%s", e);

    fd = (int)strtol(e, NULL, 10);
    if (fd < 0)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "service_checkin: Could not parse UPSTART_FDS: %s", strerror(errno));
      exit(EXIT_FAILURE);
      return;
    }

   /*
    * Upstart only supportst a single on-demand socket file descriptor...
    */

    service_add_listener(fd, 0);
  }
#endif /* HAVE_LAUNCHD */
}


/*
 * 'service_checkout()' - Update the KeepAlive/PID file as needed.
 */

static void
service_checkout(int shutdown)          /* I - Shutting down? */
{
  cups_file_t   *fp;			/* File */
  char          pidfile[1024];          /* PID/KeepAlive file */


 /*
  * When running on-demand, use the KeepAlive file, otherwise write a PID file
  * to StateDir...
  */

#ifdef HAVE_ONDEMAND
  if (OnDemand)
  {
    int shared_printers = 0;		/* Do we have shared printers? */

    strlcpy(pidfile, CUPS_KEEPALIVE, sizeof(pidfile));

   /*
    * If printer sharing is on see if there are any actual shared printers...
    */

    if (Browsing && BrowseLocalProtocols)
    {
      cupsd_printer_t *p = NULL;	/* Current printer */

      for (p = (cupsd_printer_t *)cupsArrayFirst(Printers); p; p = (cupsd_printer_t *)cupsArrayNext(Printers))
      {
        if (p->shared)
          break;
      }

      shared_printers = (p != NULL);
    }

    if (cupsArrayCount(ActiveJobs) ||	/* Active jobs */
        WebInterface ||			/* Web interface enabled */
        NeedReload ||			/* Doing a reload */
        shared_printers)                /* Printers being shared */
    {
     /*
      * Create or remove the "keep-alive" file based on whether there are active
      * jobs or shared printers to advertise...
      */

      shutdown = 0;
    }
  }
  else
#endif /* HAVE_ONDEMAND */
  snprintf(pidfile, sizeof(pidfile), "%s/cupsd.pid", StateDir);

  if (shutdown)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG, "Removing KeepAlive/PID file \"%s\".", pidfile);

    unlink(pidfile);
  }
  else
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG, "Creating KeepAlive/PID file \"%s\".", pidfile);

    if ((fp = cupsFileOpen(pidfile, "w")) != NULL)
    {
     /*
      * Save the PID in the file...
      */

      cupsFilePrintf(fp, "%d\n", (int)getpid());
      cupsFileClose(fp);
    }
    else
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to create KeepAlive/PID file \"%s\": %s", pidfile, strerror(errno));
  }

#  ifdef __APPLE__
  if (OnDemand && shutdown)
    xpc_transaction_end();
#  endif /* __APPLE__ */
}


/*
 * 'usage()' - Show scheduler usage.
 */

static void
usage(int status)			/* O - Exit status */
{
  FILE	*fp = status ? stderr : stdout;	/* Output file */


  _cupsLangPuts(fp, _("Usage: cupsd [options]"));
  _cupsLangPuts(fp, _("Options:"));
  _cupsLangPuts(fp, _("-c cupsd.conf           Set cupsd.conf file to use."));
  _cupsLangPuts(fp, _("-f                      Run in the foreground."));
  _cupsLangPuts(fp, _("-F                      Run in the foreground but detach from console."));
  _cupsLangPuts(fp, _("-h                      Show this usage message."));
#ifdef HAVE_ONDEMAND
  _cupsLangPuts(fp, _("-l                      Run cupsd on demand."));
#endif /* HAVE_ONDEMAND */
  _cupsLangPuts(fp, _("-s cups-files.conf      Set cups-files.conf file to use."));
  _cupsLangPuts(fp, _("-t                      Test the configuration file."));

  exit(status);
}
