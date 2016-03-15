/*
 * "$Id: process.c 12522 2015-02-17 20:01:33Z msweet $"
 *
 * Process management routines for the CUPS scheduler.
 *
 * Copyright 2007-2015 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <grp.h>
#ifdef __APPLE__
#  include <libgen.h>
#endif /* __APPLE__ */
#ifdef HAVE_POSIX_SPAWN
#  include <spawn.h>
extern char **environ;
#endif /* HAVE_POSIX_SPAWN */
#ifdef HAVE_POSIX_SPAWN
#  if !defined(__OpenBSD__) || OpenBSD >= 201505
#    define USE_POSIX_SPAWN 1
#  else
#    define USE_POSIX_SPAWN 0
#  endif /* !__OpenBSD__ || */
#else
#  define USE_POSIX_SPAWN 0
#endif /* HAVE_POSIX_SPAWN */


/*
 * Process structure...
 */

typedef struct
{
  int	pid,				/* Process ID */
	job_id;				/* Job associated with process */
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
cupsdCreateProfile(int job_id,		/* I - Job ID or 0 for none */
                   int allow_networking)/* I - Allow networking off machine? */
{
#ifdef HAVE_SANDBOX_H
  cups_file_t		*fp;		/* File pointer */
  char			profile[1024],	/* File containing the profile */
			bin[1024],	/* Quoted ServerBin */
			cache[1024],	/* Quoted CacheDir */
			domain[1024],	/* Domain socket, if any */
			request[1024],	/* Quoted RequestRoot */
			root[1024],	/* Quoted ServerRoot */
			state[1024],	/* Quoted StateDir */
			temp[1024];	/* Quoted TempDir */
  const char		*nodebug;	/* " (with no-log)" for no debug */
  cupsd_listener_t	*lis;		/* Current listening socket */


  if (!UseSandboxing || Sandboxing == CUPSD_SANDBOXING_OFF)
  {
   /*
    * Only use sandbox profiles as root...
    */

    cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdCreateProfile(job_id=%d, allow_networking=%d) = NULL", job_id, allow_networking);

    return (NULL);
  }

  if ((fp = cupsTempFile2(profile, sizeof(profile))) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdCreateProfile(job_id=%d, allow_networking=%d) = NULL", job_id, allow_networking);
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to create security profile: %s",
                    strerror(errno));
    return (NULL);
  }

  fchown(cupsFileNumber(fp), RunUser, Group);
  fchmod(cupsFileNumber(fp), 0640);

  cupsd_requote(bin, ServerBin, sizeof(bin));
  cupsd_requote(cache, CacheDir, sizeof(cache));
  cupsd_requote(request, RequestRoot, sizeof(request));
  cupsd_requote(root, ServerRoot, sizeof(root));
  cupsd_requote(state, StateDir, sizeof(state));
  cupsd_requote(temp, TempDir, sizeof(temp));

  nodebug = LogLevel < CUPSD_LOG_DEBUG ? " (with no-log)" : "";

  cupsFilePuts(fp, "(version 1)\n");
  if (Sandboxing == CUPSD_SANDBOXING_STRICT)
    cupsFilePuts(fp, "(deny default)\n");
  else
    cupsFilePuts(fp, "(allow default)\n");
  if (LogLevel >= CUPSD_LOG_DEBUG)
    cupsFilePuts(fp, "(debug deny)\n");
  cupsFilePuts(fp, "(import \"system.sb\")\n");
  cupsFilePuts(fp, "(system-network)\n");
  cupsFilePuts(fp, "(allow mach-per-user-lookup)\n");
  cupsFilePuts(fp, "(allow ipc-posix-sem)\n");
  cupsFilePuts(fp, "(allow ipc-posix-shm)\n");
  cupsFilePuts(fp, "(allow ipc-sysv-shm)\n");
  cupsFilePuts(fp, "(allow mach-lookup)\n");
  if (!RunUser)
    cupsFilePrintf(fp,
		   "(deny file-write* file-read-data file-read-metadata\n"
		   "  (regex"
		   " #\"^/Users$\""
		   " #\"^/Users/\""
		   ")%s)\n", nodebug);
  cupsFilePrintf(fp,
                 "(deny file-write*\n"
                 "  (regex"
		 " #\"^%s$\""		/* ServerRoot */
		 " #\"^%s/\""		/* ServerRoot/... */
		 " #\"^/private/etc$\""
		 " #\"^/private/etc/\""
		 " #\"^/usr/local/etc$\""
		 " #\"^/usr/local/etc/\""
		 " #\"^/Library$\""
		 " #\"^/Library/\""
		 " #\"^/System$\""
		 " #\"^/System/\""
		 ")%s)\n",
		 root, root, nodebug);
  /* Specifically allow applications to stat RequestRoot and some other system folders */
  cupsFilePrintf(fp,
                 "(allow file-read-metadata\n"
                 "  (regex"
		 " #\"^/$\""		/* / */
		 " #\"^/usr$\""		/* /usr */
		 " #\"^/Library$\""	/* /Library */
		 " #\"^/Library/Printers$\""	/* /Library/Printers */
		 " #\"^%s$\""		/* RequestRoot */
		 "))\n",
		 request);
  /* Read and write TempDir, CacheDir, and other common folders */
  cupsFilePuts(fp,
	       "(allow file-write* file-read-data file-read-metadata\n"
	       "  (regex"
	       " #\"^/private/var/db/\""
	       " #\"^/private/var/folders/\""
	       " #\"^/private/var/lib/\""
	       " #\"^/private/var/log/\""
	       " #\"^/private/var/mysql/\""
	       " #\"^/private/var/run/\""
	       " #\"^/private/var/spool/\""
	       " #\"^/Library/Application Support/\""
	       " #\"^/Library/Caches/\""
	       " #\"^/Library/Logs/\""
	       " #\"^/Library/Preferences/\""
	       " #\"^/Library/WebServer/\""
	       " #\"^/Users/Shared/\""
	       "))\n");
  cupsFilePrintf(fp,
		 "(deny file-write*\n"
		 "       (regex #\"^%s$\")%s)\n",
		 request, nodebug);
  cupsFilePrintf(fp,
		 "(deny file-write* file-read-data file-read-metadata\n"
		 "       (regex #\"^%s/\")%s)\n",
		 request, nodebug);
  cupsFilePrintf(fp,
                 "(allow file-write* file-read-data file-read-metadata\n"
                 "  (regex"
		 " #\"^%s$\""		/* TempDir */
		 " #\"^%s/\""		/* TempDir/... */
		 " #\"^%s$\""		/* CacheDir */
		 " #\"^%s/\""		/* CacheDir/... */
		 " #\"^%s$\""		/* StateDir */
		 " #\"^%s/\""		/* StateDir/... */
		 "))\n",
		 temp, temp, cache, cache, state, state);
  /* Read common folders */
  cupsFilePrintf(fp,
                 "(allow file-read-data file-read-metadata\n"
                 "  (regex"
                 " #\"^/AppleInternal$\""
                 " #\"^/AppleInternal/\""
                 " #\"^/bin$\""		/* /bin */
                 " #\"^/bin/\""		/* /bin/... */
                 " #\"^/private$\""
                 " #\"^/private/etc$\""
                 " #\"^/private/etc/\""
                 " #\"^/private/tmp$\""
                 " #\"^/private/tmp/\""
                 " #\"^/private/var$\""
                 " #\"^/private/var/db$\""
                 " #\"^/private/var/folders$\""
                 " #\"^/private/var/lib$\""
                 " #\"^/private/var/log$\""
                 " #\"^/private/var/mysql$\""
                 " #\"^/private/var/run$\""
                 " #\"^/private/var/spool$\""
                 " #\"^/private/var/tmp$\""
                 " #\"^/private/var/tmp/\""
                 " #\"^/usr/bin$\""	/* /usr/bin */
                 " #\"^/usr/bin/\""	/* /usr/bin/... */
                 " #\"^/usr/libexec/cups$\""	/* /usr/libexec/cups */
                 " #\"^/usr/libexec/cups/\""	/* /usr/libexec/cups/... */
                 " #\"^/usr/libexec/fax$\""	/* /usr/libexec/fax */
                 " #\"^/usr/libexec/fax/\""	/* /usr/libexec/fax/... */
                 " #\"^/usr/sbin$\""	/* /usr/sbin */
                 " #\"^/usr/sbin/\""	/* /usr/sbin/... */
		 " #\"^/Library$\""	/* /Library */
		 " #\"^/Library/\""	/* /Library/... */
		 " #\"^/System$\""	/* /System */
		 " #\"^/System/\""	/* /System/... */
		 " #\"^%s/Library$\""	/* RequestRoot/Library */
		 " #\"^%s/Library/\""	/* RequestRoot/Library/... */
		 " #\"^%s$\""		/* ServerBin */
		 " #\"^%s/\""		/* ServerBin/... */
		 " #\"^%s$\""		/* ServerRoot */
		 " #\"^%s/\""		/* ServerRoot/... */
		 "))\n",
		 request, request, bin, bin, root, root);
  if (Sandboxing == CUPSD_SANDBOXING_RELAXED)
  {
    /* Limited write access to /Library/Printers/... */
    cupsFilePuts(fp,
		 "(allow file-write*\n"
		 "  (regex"
		 " #\"^/Library/Printers/.*/\""
		 "))\n");
    cupsFilePrintf(fp,
		   "(deny file-write*\n"
		   "  (regex"
		   " #\"^/Library/Printers/PPDs$\""
		   " #\"^/Library/Printers/PPDs/\""
		   " #\"^/Library/Printers/PPD Plugins$\""
		   " #\"^/Library/Printers/PPD Plugins/\""
		   ")%s)\n", nodebug);
  }
  /* Allow execution of child processes as long as the programs are not in a user directory */
  cupsFilePuts(fp, "(allow process*)\n");
  cupsFilePuts(fp, "(deny process-exec (regex #\"^/Users/\"))\n");
  if (RunUser && getenv("CUPS_TESTROOT"))
  {
    /* Allow source directory access in "make test" environment */
    char	testroot[1024];		/* Root directory of test files */

    cupsd_requote(testroot, getenv("CUPS_TESTROOT"), sizeof(testroot));

    cupsFilePrintf(fp,
		   "(allow file-write* file-read-data file-read-metadata\n"
		   "  (regex"
		   " #\"^%s$\""		/* CUPS_TESTROOT */
		   " #\"^%s/\""		/* CUPS_TESTROOT/... */
		   "))\n",
		   testroot, testroot);
    cupsFilePrintf(fp,
		   "(allow process-exec\n"
		   "  (regex"
		   " #\"^%s/\""		/* CUPS_TESTROOT/... */
		   "))\n",
		   testroot);
    cupsFilePrintf(fp, "(allow sysctl*)\n");
  }
  if (job_id)
  {
    /* Allow job filters to read the current job files... */
    cupsFilePrintf(fp,
                   "(allow file-read-data file-read-metadata\n"
                   "       (regex #\"^%s/([ac]%05d|d%05d-[0-9][0-9][0-9])$\"))\n",
		   request, job_id, job_id);
  }
  else
  {
    /* Allow email notifications from notifiers... */
    cupsFilePuts(fp,
		 "(allow process-exec\n"
		 "  (literal \"/usr/sbin/sendmail\")\n"
		 "  (with no-sandbox))\n");
  }
  /* Allow access to Bluetooth, USB, and notify_post. */
  cupsFilePuts(fp, "(allow iokit*)\n");
  cupsFilePuts(fp, "(allow distributed-notification-post)\n");
  /* Allow outbound networking to local services */
  cupsFilePuts(fp, "(allow network-outbound"
		   "\n       (regex #\"^/private/var/run/\" #\"^/private/tmp/\" #\"^/private/var/tmp/\")");
  for (lis = (cupsd_listener_t *)cupsArrayFirst(Listeners);
       lis;
       lis = (cupsd_listener_t *)cupsArrayNext(Listeners))
  {
    if (httpAddrFamily(&(lis->address)) == AF_LOCAL)
    {
      httpAddrString(&(lis->address), domain, sizeof(domain));
      cupsFilePrintf(fp, "\n       (literal \"%s\")", domain);
    }
  }
  if (allow_networking)
  {
    /* Allow TCP and UDP networking off the machine... */
    cupsFilePuts(fp, "\n       (remote tcp))\n");
    cupsFilePuts(fp, "(allow network-bind)\n"); /* for LPD resvport */
    cupsFilePuts(fp, "(allow network*\n"
		     "       (local udp \"*:*\")\n"
		     "       (remote udp \"*:*\"))\n");

    /* Also allow access to device files... */
    cupsFilePuts(fp, "(allow file-write* file-read-data file-read-metadata file-ioctl\n"
                     "       (regex #\"^/dev/\"))\n");

    /* And allow kernel extensions to be loaded, e.g., SMB */
    cupsFilePuts(fp, "(allow system-kext-load)\n");
  }
  else
  {
    /* Only allow SNMP (UDP) and LPD (TCP) off the machine... */
    cupsFilePuts(fp, ")\n");
    cupsFilePuts(fp, "(allow network-outbound\n"
		     "       (remote udp \"*:161\")"
		     "       (remote tcp \"*:515\"))\n");
    cupsFilePuts(fp, "(allow network-inbound\n"
		     "       (local udp \"localhost:*\"))\n");
  }
  cupsFileClose(fp);

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdCreateProfile(job_id=%d,allow_networking=%d) = \"%s\"", job_id, allow_networking, profile);
  return ((void *)strdup(profile));

#else
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdCreateProfile(job_id=%d, allow_networking=%d) = NULL", job_id, allow_networking);

  return (NULL);
#endif /* HAVE_SANDBOX_H */
}


/*
 * 'cupsdDestroyProfile()' - Delete an execution profile.
 */

void
cupsdDestroyProfile(void *profile)	/* I - Profile */
{
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdDeleteProfile(profile=\"%s\")",
		  profile ? (char *)profile : "(null)");

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
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdEndProcess(pid=%d, force=%d)", pid,
                  force);

  if (!pid)
    return (0);

  if (!RunUser)
  {
   /*
    * When running as root, cupsd puts child processes in their own process
    * group.  Using "-pid" sends a signal to all processes in the group.
    */

    pid = -pid;
  }

  if (force)
    return (kill(pid, SIGKILL));
  else
    return (kill(pid, SIGTERM));
}


/*
 * 'cupsdFinishProcess()' - Finish a process and get its name.
 */

const char *				/* O - Process name */
cupsdFinishProcess(int    pid,		/* I - Process ID */
                   char   *name,	/* I - Name buffer */
		   size_t namelen,	/* I - Size of name buffer */
		   int    *job_id)	/* O - Job ID pointer or NULL */
{
  cupsd_proc_t	key,			/* Search key */
		*proc;			/* Matching process */


  key.pid = pid;

  if ((proc = (cupsd_proc_t *)cupsArrayFind(process_array, &key)) != NULL)
  {
    if (job_id)
      *job_id = proc->job_id;

    strlcpy(name, proc->name, namelen);
    cupsArrayRemove(process_array, proc);
    free(proc);
  }
  else
  {
    if (job_id)
      *job_id = 0;

    strlcpy(name, "unknown", namelen);
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdFinishProcess(pid=%d, name=%p, namelen=" CUPS_LLFMT ", job_id=%p(%d)) = \"%s\"", pid, name, CUPS_LLCAST namelen, job_id, job_id ? *job_id : 0, name);

  return (name);
}


/*
 * 'cupsdStartProcess()' - Start a process.
 */

int					/* O - Process ID or 0 */
cupsdStartProcess(
    const char  *command,		/* I - Full path to command */
    char        *argv[],		/* I - Command-line arguments */
    char        *envp[],		/* I - Environment */
    int         infd,			/* I - Standard input file descriptor */
    int         outfd,			/* I - Standard output file descriptor */
    int         errfd,			/* I - Standard error file descriptor */
    int         backfd,			/* I - Backchannel file descriptor */
    int         sidefd,			/* I - Sidechannel file descriptor */
    int         root,			/* I - Run as root? */
    void        *profile,		/* I - Security profile to use */
    cupsd_job_t *job,			/* I - Job associated with process */
    int         *pid)			/* O - Process ID */
{
  int		i;			/* Looping var */
  const char	*exec_path = command;	/* Command to be exec'd */
  char		*real_argv[110],	/* Real command-line arguments */
		cups_exec[1024],	/* Path to "cups-exec" program */
		user_str[16],		/* User string */
		group_str[16],		/* Group string */
		nice_str[16];		/* FilterNice string */
  uid_t		user;			/* Command UID */
  cupsd_proc_t	*proc;			/* New process record */
#if USE_POSIX_SPAWN
  posix_spawn_file_actions_t actions;	/* Spawn file actions */
  posix_spawnattr_t attrs;		/* Spawn attributes */
  sigset_t	defsignals;		/* Default signals */
#elif defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* POSIX signal handler */
#endif /* USE_POSIX_SPAWN */
#if defined(__APPLE__)
  char		processPath[1024],	/* CFProcessPath environment variable */
		linkpath[1024];		/* Link path for symlinks... */
  int		linkbytes;		/* Bytes for link path */
#endif /* __APPLE__ */


  *pid = 0;

 /*
  * Figure out the UID for the child process...
  */

  if (RunUser)
    user = RunUser;
  else if (root)
    user = 0;
  else
    user = User;

 /*
  * Check the permissions of the command we are running...
  */

  if (_cupsFileCheck(command, _CUPS_FILE_CHECK_PROGRAM, !RunUser,
                     cupsdLogFCMessage, job ? job->printer : NULL))
    return (0);

#if defined(__APPLE__)
  if (envp)
  {
   /*
    * Add special voodoo magic for OS X - this allows OS X programs to access
    * their bundle resources properly...
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
  * Use helper program when we have a sandbox profile...
  */

#if !USE_POSIX_SPAWN
  if (profile)
#endif /* !USE_POSIX_SPAWN */
  {
    snprintf(cups_exec, sizeof(cups_exec), "%s/daemon/cups-exec", ServerBin);
    snprintf(user_str, sizeof(user_str), "%d", user);
    snprintf(group_str, sizeof(group_str), "%d", Group);
    snprintf(nice_str, sizeof(nice_str), "%d", FilterNice);

    real_argv[0] = cups_exec;
    real_argv[1] = (char *)"-g";
    real_argv[2] = group_str;
    real_argv[3] = (char *)"-n";
    real_argv[4] = nice_str;
    real_argv[5] = (char *)"-u";
    real_argv[6] = user_str;
    real_argv[7] = profile ? profile : "none";
    real_argv[8] = (char *)command;

    for (i = 0;
         i < (int)(sizeof(real_argv) / sizeof(real_argv[0]) - 10) && argv[i];
	 i ++)
      real_argv[i + 9] = argv[i];

    real_argv[i + 9] = NULL;

    argv      = real_argv;
    exec_path = cups_exec;
  }

  if (LogLevel == CUPSD_LOG_DEBUG2)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdStartProcess: Preparing to start \"%s\", arguments:", command);

    for (i = 0; argv[i]; i ++)
      cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdStartProcess: argv[%d] = \"%s\"", i, argv[i]);
  }

#if USE_POSIX_SPAWN
 /*
  * Setup attributes and file actions for the spawn...
  */

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdStartProcess: Setting spawn attributes.");
  sigemptyset(&defsignals);
  sigaddset(&defsignals, SIGTERM);
  sigaddset(&defsignals, SIGCHLD);
  sigaddset(&defsignals, SIGPIPE);

  posix_spawnattr_init(&attrs);
  posix_spawnattr_setflags(&attrs, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGDEF);
  posix_spawnattr_setpgroup(&attrs, 0);
  posix_spawnattr_setsigdefault(&attrs, &defsignals);

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdStartProcess: Setting file actions.");
  posix_spawn_file_actions_init(&actions);
  if (infd != 0)
  {
    if (infd < 0)
      posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
    else
      posix_spawn_file_actions_adddup2(&actions, infd, 0);
  }

  if (outfd != 1)
  {
    if (outfd < 0)
      posix_spawn_file_actions_addopen(&actions, 1, "/dev/null", O_WRONLY, 0);
    else
      posix_spawn_file_actions_adddup2(&actions, outfd, 1);
  }

  if (errfd != 2)
  {
    if (errfd < 0)
      posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);
    else
      posix_spawn_file_actions_adddup2(&actions, errfd, 2);
  }

  if (backfd != 3 && backfd >= 0)
    posix_spawn_file_actions_adddup2(&actions, backfd, 3);

  if (sidefd != 4 && sidefd >= 0)
    posix_spawn_file_actions_adddup2(&actions, sidefd, 4);

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdStartProcess: Calling posix_spawn.");

  if (posix_spawn(pid, exec_path, &actions, &attrs, argv, envp ? envp : environ))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to fork %s - %s.", command, strerror(errno));

    *pid = 0;
  }
  else
    cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdStartProcess: pid=%d", (int)*pid);

  posix_spawn_file_actions_destroy(&actions);
  posix_spawnattr_destroy(&attrs);

#else
 /*
  * Block signals before forking...
  */

  cupsdHoldSignals();

  if ((*pid = fork()) == 0)
  {
   /*
    * Child process goes here; update stderr as needed...
    */

    if (errfd != 2)
    {
      if (errfd < 0)
        errfd = open("/dev/null", O_WRONLY);

      if (errfd != 2)
      {
        dup2(errfd, 2);
	close(errfd);
      }
    }

   /*
    * Put this process in its own process group so that we can kill any child
    * processes it creates.
    */

#  ifdef HAVE_SETPGID
    if (!RunUser && setpgid(0, 0))
      exit(errno + 100);
#  else
    if (!RunUser && setpgrp())
      exit(errno + 100);
#  endif /* HAVE_SETPGID */

   /*
    * Update the remaining file descriptors as needed...
    */

    if (infd != 0)
    {
      if (infd < 0)
        infd = open("/dev/null", O_RDONLY);

      if (infd != 0)
      {
        dup2(infd, 0);
	close(infd);
      }
    }

    if (outfd != 1)
    {
      if (outfd < 0)
        outfd = open("/dev/null", O_WRONLY);

      if (outfd != 1)
      {
        dup2(outfd, 1);
	close(outfd);
      }
    }

    if (backfd != 3 && backfd >= 0)
    {
      dup2(backfd, 3);
      close(backfd);
      fcntl(3, F_SETFL, O_NDELAY);
    }

    if (sidefd != 4 && sidefd >= 0)
    {
      dup2(sidefd, 4);
      close(sidefd);
      fcntl(4, F_SETFL, O_NDELAY);
    }

   /*
    * Change the priority of the process based on the FilterNice setting.
    * (this is not done for root processes...)
    */

    if (!root)
      nice(FilterNice);

   /*
    * Reset group membership to just the main one we belong to.
    */

    if (!RunUser && setgid(Group))
      exit(errno + 100);

    if (!RunUser && setgroups(1, &Group))
      exit(errno + 100);

   /*
    * Change user to something "safe"...
    */

    if (!RunUser && user && setuid(user))
      exit(errno + 100);

   /*
    * Change umask to restrict permissions on created files...
    */

    umask(077);

   /*
    * Unblock signals before doing the exec...
    */

#  ifdef HAVE_SIGSET
    sigset(SIGTERM, SIG_DFL);
    sigset(SIGCHLD, SIG_DFL);
    sigset(SIGPIPE, SIG_DFL);
#  elif defined(HAVE_SIGACTION)
    memset(&action, 0, sizeof(action));

    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_DFL;

    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGCHLD, &action, NULL);
    sigaction(SIGPIPE, &action, NULL);
#  else
    signal(SIGTERM, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
#  endif /* HAVE_SIGSET */

    cupsdReleaseSignals();

   /*
    * Execute the command; if for some reason this doesn't work, log an error
    * exit with a non-zero value...
    */

    if (envp)
      execve(exec_path, argv, envp);
    else
      execv(exec_path, argv);

    exit(errno + 100);
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

  cupsdReleaseSignals();
#endif /* USE_POSIX_SPAWN */

  if (*pid)
  {
    if (!process_array)
      process_array = cupsArrayNew((cups_array_func_t)compare_procs, NULL);

    if (process_array)
    {
      if ((proc = calloc(1, sizeof(cupsd_proc_t) + strlen(command))) != NULL)
      {
        proc->pid    = *pid;
	proc->job_id = job ? job->id : 0;
	_cups_strcpy(proc->name, command);

	cupsArrayAdd(process_array, proc);
      }
    }
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
		  "cupsdStartProcess(command=\"%s\", argv=%p, envp=%p, "
		  "infd=%d, outfd=%d, errfd=%d, backfd=%d, sidefd=%d, root=%d, "
		  "profile=%p, job=%p(%d), pid=%p) = %d",
		  command, argv, envp, infd, outfd, errfd, backfd, sidefd,
		  root, profile, job, job ? job->id : 0, pid, *pid);

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

    if (ch == '/' && !*src)
      break;				/* Don't add trailing slash */

    if (strchr(".?*()[]^$\\", ch))
      *dstptr++ = '\\';

    *dstptr++ = (char)ch;
  }

  *dstptr = '\0';

  return (dst);
}
#endif /* HAVE_SANDBOX_H */


/*
 * End of "$Id: process.c 12522 2015-02-17 20:01:33Z msweet $".
 */
