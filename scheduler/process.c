/*
 * "$Id$"
 *
 *   Process management routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
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
 *   cupsdCreateProfile()  - Create an execution profile for a subprocess.
 *   cupsdDestroyProfile() - Delete an execution profile.
 *   cupsdEndProcess()     - End a process.
 *   cupsdFinishProcess()  - Finish a process and get its name.
 *   cupsdStartProcess()   - Start a process.
 *   compare_procs()       - Compare two processes.
 *   cupsd_requote()       - Make a regular-expression version of a string.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <grp.h>
#ifdef __APPLE__
#  include <libgen.h>
#endif /* __APPLE__ */ 
#ifdef HAVE_SANDBOX_H
#  define __APPLE_API_PRIVATE
#  include <sandbox.h>
#endif /* HAVE_SANDBOX_H */


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
#ifdef HAVE_SANDBOX_H
static char	*cupsd_requote(char *dst, const char *src, size_t dstsize);
#endif /* HAVE_SANDBOX_H */


/*
 * 'cupsdCreateProfile()' - Create an execution profile for a subprocess.
 */

void *					/* O - Profile or NULL on error */
cupsdCreateProfile(int job_id)		/* I - Job ID or 0 for none */
{
#ifdef HAVE_SANDBOX_H
  cups_file_t	*fp;			/* File pointer */
  char		profile[1024],		/* File containing the profile */
		cache[1024],		/* Quoted CacheDir */
		request[1024],		/* Quoted RequestRoot */
		root[1024],		/* Quoted ServerRoot */
		temp[1024];		/* Quoted TempDir */
  size_t	requestlen;		/* Length of RequestRoot */


  if ((fp = cupsTempFile2(profile, sizeof(profile))) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to create security profile: %s",
                    strerror(errno));
    return (NULL);
  }

  cupsd_requote(cache, CacheDir, sizeof(cache));
  cupsd_requote(request, RequestRoot, sizeof(request));
  cupsd_requote(root, ServerRoot, sizeof(root));
  cupsd_requote(temp, TempDir, sizeof(temp));

  cupsFilePuts(fp, "(version 1)\n");
  cupsFilePuts(fp, "(debug deny)\n");
  cupsFilePuts(fp, "(allow default)\n");
  cupsFilePrintf(fp,
                 "(deny file-write* file-read-data file-read-metadata\n"
                 "  (regex #\"^%s\"))\n", request);
  cupsFilePrintf(fp,
                 "(deny file-write*\n"
                 "  (regex #\"^%s\" #\"^/etc\" #\"^/usr/local/etc\" "
		 "#\"^/Library\" #\"^/System\"))\n", root);
  cupsFilePrintf(fp,
                 "(allow file-write* file-read-data file-read-metadata\n"
                 "  (regex #\"^%s$\" #\"^%s/\" #\"^%s$\" #\"^%s/\"))\n",
		 temp, cache, temp, cache);
  if (job_id)
    cupsFilePrintf(fp,
                   "(allow file-read-data file-read-metadata\n"
                   "  (regex #\"^%s/([ac]%05d|d%05d-[0-9][0-9][0-9])$\"))\n",
		   request);

  cupsFileClose(fp);

  return ((void *)strdup(profile));
#else

  return (NULL);
#endif /* HAVE_SANDBOX_H */
}


/*
 * 'cupsdDestroyProfile()' - Delete an execution profile.
 */

void
cupsdDestroyProfile(void *profile)	/* I - Profile */
{
#ifdef HAVE_SANDBOX_H
  if (profile)
  {
    unlink((char *)profile);
    free(profile);
  }
#endif /* HAVE_SANDBOX_H */
}


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
    int        sidefd,			/* I - Sidechannel file descriptor */
    int        root,			/* I - Run as root? */
    void       *profile,		/* I - Security profile to use */
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

  if (access(command, X_OK))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to execute %s: %s", command,
                    strerror(errno));
    *pid = 0;
    return (0);
  }

#if defined(__APPLE__)
  if (envp)
  {
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
		 dirname((char *)command), linkpath);
    }
    else
      snprintf(processPath, sizeof(processPath), "CFProcessPath=%s", command);

    envp[0] = processPath;		/* Replace <CFProcessPath> string */
  }
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
    if (sidefd != 4 && sidefd > 0)
    {
      close(4);
      dup(sidefd);
      fcntl(4, F_SETFL, O_NDELAY);
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

#ifdef HAVE_SANDBOX_H
   /*
    * Run in a separate security profile...
    */

    if (profile)
    {
      char *error;			/* Sandbox error, if any */

      if (sandbox_init((char *)profile, SANDBOX_NAMED_EXTERNAL, &error))
      {
        fprintf(stderr, "ERROR: sandbox_init failed: %s (%s)\n", error,
	        strerror(errno));
	sandbox_free_error(error);
      }
    }
#endif /* HAVE_SANDBOX_H */

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


#ifdef HAVE_SANDBOX_H
/*
 * 'cupsd_requote()' - Make a regular-expression version of a string.
 */

static char *				/* O - Quoted string */
cupsd_requote(char       *dst,		/* I - Destination buffer */
              const char *src,		/* I - Source string */
	      size_t     dstsize)	/* I - Size of destination buffer */
{
  int	ch;				/* Current character */
  char	*dstptr,			/* Current position in buffer */
	*dstend;			/* End of destination buffer */


  dstptr = dst;
  dstend = dst + dstsize - 2;

  while (*src && dstptr < dstend)
  {
    ch = *src++;

    if (strchr(".?*()[]^$\\", ch))
      *dstptr++ = '\\';

    *dstptr++ = ch;
  }

  *dstptr = '\0';

  return (dst);
}
#endif /* HAVE_SANDBOX_H */


/*
 * End of "$Id$".
 */
