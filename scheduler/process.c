/*
 * "$Id$"
 *
 *   Process management routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
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
 *   cupsdEndProcess()    - End a process.
 *   cupsdFinishProcess() - Finish a process and get its name.
 *   cupsdStartProcess()  - Start a process.
 *   compare_procs()      - Compare two processes.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <grp.h>
#if defined(__APPLE__)
#  include <libgen.h>
#endif /* __APPLE__ */ 


/*
 * Process structure...
 */

typedef struct
{
  int	pid;				/* Process ID */
  char	name[1];			/* Name of process */
} cupsd_proc_t;


/*
 * Local globals...
 */

static cups_array_t	*process_array = NULL;


/*
 * Local functions...
 */

static int	compare_procs(cupsd_proc_t *a, cupsd_proc_t *b);


/*
 * 'cupsdEndProcess()' - End a process.
 */

int					/* O - 0 on success, -1 on failure */
cupsdEndProcess(int pid,		/* I - Process ID */
                int force)		/* I - Force child to die */
{
  if (force)
    return (kill(pid, SIGKILL));
  else
    return (kill(pid, SIGTERM));
}


/*
 * 'cupsdFinishProcess()' - Finish a process and get its name.
 */

const char *				/* O - Process name */
cupsdFinishProcess(int  pid,		/* I - Process ID */
                   char *name,		/* I - Name buffer */
		   int  namelen)	/* I - Size of name buffer */
{
  cupsd_proc_t	key,			/* Search key */
		*proc;			/* Matching process */


  key.pid = pid;

  if ((proc = (cupsd_proc_t *)cupsArrayFind(process_array, &key)) != NULL)
  {
    strlcpy(name, proc->name, namelen);
    cupsArrayRemove(process_array, proc);
    free(proc);

    return (name);
  }
  else
    return ("unknown");
}


/*
 * 'cupsdStartProcess()' - Start a process.
 */

int					/* O - Process ID or 0 */
cupsdStartProcess(
    const char *command,		/* I - Full path to command */
    char       *argv[],			/* I - Command-line arguments */
    char       *envp[],			/* I - Environment */
    int        infd,			/* I - Standard input file descriptor */
    int        outfd,			/* I - Standard output file descriptor */
    int        errfd,			/* I - Standard error file descriptor */
    int        backfd,			/* I - Backchannel file descriptor */
    int        root,			/* I - Run as root? */
    int        *pid)			/* O - Process ID */
{
  cupsd_proc_t	*proc;			/* New process record */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* POSIX signal handler */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */
#if defined(__APPLE__)
  char		processPath[1024],	/* CFProcessPath environment variable */
		linkpath[1024];		/* Link path for symlinks... */
  int		linkbytes;		/* Bytes for link path */
#endif /* __APPLE__ */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdStartProcess(\"%s\", %p, %p, %d, %d, %d)",
                  command, argv, envp, infd, outfd, errfd);

#if defined(__APPLE__)
 /*
  * Add special voodoo magic for MacOS X - this allows MacOS X 
  * programs to access their bundle resources properly...
  */

  if ((linkbytes = readlink(command, linkpath, sizeof(linkpath) - 1)) > 0)
  {
   /*
    * Yes, this is a symlink to the actual program, nul-terminate and
    * use it...
    */

    linkpath[linkbytes] = '\0';

    if (linkpath[0] == '/')
      snprintf(processPath, sizeof(processPath), "CFProcessPath=%s",
	       linkpath);
    else
      snprintf(processPath, sizeof(processPath), "CFProcessPath=%s/%s",
	       dirname(command), linkpath);
  }
  else
    snprintf(processPath, sizeof(processPath), "CFProcessPath=%s", command);

  envp[0] = processPath;		/* Replace <CFProcessPath> string */
#endif	/* __APPLE__ */

 /*
  * Block signals before forking...
  */

  cupsdHoldSignals();

  if ((*pid = fork()) == 0)
  {
   /*
    * Child process goes here...
    *
    * Update stdin/stdout/stderr as needed...
    */

    if (infd != 0)
    {
      close(0);
      if (infd > 0)
        dup(infd);
      else
        open("/dev/null", O_RDONLY);
    }
    if (outfd != 1)
    {
      close(1);
      if (outfd > 0)
	dup(outfd);
      else
        open("/dev/null", O_WRONLY);
    }
    if (errfd != 2)
    {
      close(2);
      if (errfd > 0)
        dup(errfd);
      else
        open("/dev/null", O_WRONLY);
    }
    if (backfd != 3)
    {
      close(3);
      if (backfd > 0)
	dup(backfd);
      else
        open("/dev/null", O_RDWR);
      fcntl(3, F_SETFL, O_NDELAY);
    }

   /*
    * Change the priority of the process based on the FilterNice setting.
    * (this is not done for backends...)
    */

    if (!root)
      nice(FilterNice);

   /*
    * Change user to something "safe"...
    */

    if (!root && !RunUser)
    {
     /*
      * Running as root, so change to non-priviledged user...
      */

      if (setgid(Group))
	exit(errno);

      if (setgroups(1, &Group))
	exit(errno);

      if (setuid(User))
        exit(errno);
    }
    else
    {
     /*
      * Reset group membership to just the main one we belong to.
      */

      setgid(Group);
      setgroups(1, &Group);
    }

   /*
    * Change umask to restrict permissions on created files...
    */

    umask(077);

   /*
    * Unblock signals before doing the exec...
    */

#ifdef HAVE_SIGSET
    sigset(SIGTERM, SIG_DFL);
    sigset(SIGCHLD, SIG_DFL);
#elif defined(HAVE_SIGACTION)
    memset(&action, 0, sizeof(action));

    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_DFL;

    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGCHLD, &action, NULL);
#else
    signal(SIGTERM, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
#endif /* HAVE_SIGSET */

    cupsdReleaseSignals();

   /*
    * Execute the command; if for some reason this doesn't work,
    * return the error code...
    */

    if (envp)
      execve(command, argv, envp);
    else
      execv(command, argv);

    perror(command);

    exit(errno);
  }
  else if (*pid < 0)
  {
   /*
    * Error - couldn't fork a new process!
    */

    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to fork %s - %s.", command,
                    strerror(errno));

    *pid = 0;
  }
  else
  {
    if (!process_array)
      process_array = cupsArrayNew((cups_array_func_t)compare_procs, NULL);
 
    if (process_array)
    {
      if ((proc = calloc(1, sizeof(cupsd_proc_t) + strlen(command))) != NULL)
      {
        proc->pid = *pid;
	strcpy(proc->name, command);

	cupsArrayAdd(process_array, proc);
      }
    }
  }

  cupsdReleaseSignals();

  return (*pid);
}


/*
 * 'compare_procs()' - Compare two processes.
 */

static int				/* O - Result of comparison */
compare_procs(cupsd_proc_t *a,		/* I - First process */
              cupsd_proc_t *b)		/* I - Second process */
{
  return (a->pid - b->pid);
}


/*
 * End of "$Id$".
 */
