/*
 * "$Id: main.c,v 1.31 1999/12/29 02:15:43 mike Exp $"
 *
 *   Scheduler main loop for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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
 *   main()            - Main entry for the CUPS scheduler.
 *   sigchld_handler() - Handle 'child' signals from old processes.
 *   sighup_handler()  - Handle 'hangup' signals to reconfigure the scheduler.
 *   usage()           - Show scheduler usage.
 */

/*
 * Include necessary headers...
 */

#define _MAIN_C_
#include "cupsd.h"
#include <sys/resource.h>


/*
 * Local functions...
 */

static void	sigchld_handler(int sig);
static void	sighup_handler(int sig);
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
  fd_set		input,		/* Input set for select() */
			output;		/* Output set for select() */
  client_t		*con;		/* Current client */
  job_t			*job,		/* Current job */
			*next;		/* Next job */
  listener_t		*lis;		/* Current listener */
  time_t		activity;	/* Activity timer */
  struct timeval	timeout;	/* select() timeout */
  struct rlimit		limit;		/* Runtime limit */
#ifdef HAVE_SIGACTION
  struct sigaction	action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION */


 /*
  * Check for command-line arguments...
  */

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      for (opt = argv[i] + 1; *opt != '\0'; opt ++)
        switch (*opt)
	{
	  case 'c' : /* Configuration file */
	      i ++;
	      if (i >= argc)
	        usage();

	      strncpy(ConfigurationFile, argv[i], sizeof(ConfigurationFile) - 1);
	      ConfigurationFile[sizeof(ConfigurationFile) - 1] = '\0';
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
  * Set the timezone info...
  */

  if (getenv("TZ") != NULL)
    sprintf(TZ, "TZ=%s", getenv("TZ"));

  tzset();

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
#endif /* DEBUG */

 /*
  * Set the maximum number of files...
  */

  getrlimit(RLIMIT_NOFILE, &limit);
  limit.rlim_cur = limit.rlim_max;
  setrlimit(RLIMIT_NOFILE, &limit);

 /*
  * Catch hangup and child signals and ignore broken pipes...
  */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGHUP, sighup_handler);
  sigset(SIGCHLD, sigchld_handler);
  sigset(SIGPIPE, SIG_IGN);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGHUP);
  action.sa_handler = sighup_handler;
  sigaction(SIGHUP, &action, NULL);

  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGCHLD);
  action.sa_handler = sigchld_handler;
  sigaction(SIGCHLD, &action, NULL);

  sigemptyset(&action.sa_mask);
  action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &action, NULL);
#else
  signal(SIGHUP, sighup_handler);
  signal(SIGCLD, sigchld_handler);	/* No, SIGCLD isn't a typo... */
  signal(SIGPIPE, SIG_IGN);
#endif /* HAVE_SIGSET */

 /*
  * Loop forever...
  */

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

	for (i = 0; i < NumListeners; i ++)
	  FD_CLR(Listeners[i].fd, &InputSet);
      }
      else if (!ReadConfiguration())
      {
        fprintf(stderr, "cupsd: Unable to read configuration file \'%s\' - exiting!\n",
	        ConfigurationFile);
        exit(1);
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

    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;

    for (i = NumClients, con = Clients; i > 0; i --, con ++)
      if (con->http.used > 0)
      {
        timeout.tv_sec  = 0;
	break;
      }

    if ((i = select(100, &input, &output, NULL, &timeout)) < 0)
    {
     /*
      * Got an error from select!
      */

      if (errno == EINTR)	/* Just interrupted by a signal */
        continue;

     /*
      * Display all sorts of logging info to help track down the problem.
      */

      perror("cupsd: select() failed");

      fprintf(stderr, "cupsd: InputSet =");
      for (i = 0; i < 100; i ++)
        if (FD_ISSET(i, &input))
          fprintf(stderr, " %d", i);
      fputs("\n", stderr);

      fprintf(stderr, "cupsd: OutputSet =");
      for (i = 0; i < 100; i ++)
        if (FD_ISSET(i, &output))
          fprintf(stderr, " %d", i);
      fputs("\n", stderr);

      for (i = 0, con = Clients; i < NumClients; i ++, con ++)
        fprintf(stderr, "cupsd: Clients[%d] = %d, file = %d, state = %d\n",
	        i, con->http.fd, con->file, con->http.state);

      for (i = 0, lis = Listeners; i < NumListeners; i ++, lis ++)
        fprintf(stderr, "cupsd: Listeners[%d] = %d\n", i, lis->fd);

      fprintf(stderr, "cupsd: BrowseSocket = %d\n", BrowseSocket);

      for (job = Jobs; job != NULL; job = job->next)
        fprintf(stderr, "cupsd: Jobs[%d] = %d\n", job->id, job->pipe);

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

      activity = time(NULL) - 30;
      if (con->http.activity < activity)
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

      if (job->pipe && FD_ISSET(job->pipe, &input))
        UpdateJob(job);
    }

   /*
    * Update the browse list as needed...
    */

    if (BrowseSocket >= 0)
    {
      if (FD_ISSET(BrowseSocket, &input))
        UpdateBrowseList();

      SendBrowseList();
    }
  }

 /*
  * If we get here something very bad happened and we need to exit
  * immediately.
  */

  CloseAllClients();
  StopListening();

  return (1);
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

    if (status == SIGTERM)
      status = 0;

    if (status)
    {
      if (status < 256)
	LogMessage(LOG_ERROR, "PID %d crashed on signal %d!", pid, status);
      else
	LogMessage(LOG_ERROR, "PID %d crashed with status %d!", pid,
	           status / 256);
    }

    for (job = Jobs; job != NULL; job = job->next)
      if (job->state == IPP_JOB_PROCESSING)
      {
	for (i = 0; job->procs[i]; i ++)
          if (job->procs[i] == pid)
	    break;

	if (job->procs[i])
	{
	 /*
          * OK, this process has gone away; what's left?
	  */

          job->procs[i] = -pid;

          if (status && job->status >= 0)
	  {
	   /*
	    * An error occurred; save the exit status so we know to stop
	    * the printer or cancel the job when all of the filters finish...
	    *
	    * A negative status indicates that the backend failed and the
	    * printer needs to be stopped.
	    */

            if (!job->procs[i + 1])
 	      job->status = -status;	/* Backend failed */
	    else
 	      job->status = status;	/* Filter failed */
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
}


/*
 * 'usage()' - Show scheduler usage.
 */

static void
usage(void)
{
  fputs("Usage: cupsd [-c config-file]\n", stderr);
  exit(1);
}


/*
 * End of "$Id: main.c,v 1.31 1999/12/29 02:15:43 mike Exp $".
 */
