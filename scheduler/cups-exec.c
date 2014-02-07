/*
 * "$Id$"
 *
 * Sandbox helper for CUPS.
 *
 * Copyright 2007-2014 by Apple Inc.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Usage:
 *
 *     cups-exec /path/to/profile UID GID NICE /path/to/program argv0 argv1 ... argvN
 */

/*
 * Include necessary headers...
 */

#include <cups/string-private.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef HAVE_SANDBOX_H
#  include <sandbox.h>
#  ifndef SANDBOX_NAMED_EXTERNAL
#    define SANDBOX_NAMED_EXTERNAL  0x0003
#  endif /* !SANDBOX_NAMED_EXTERNAL */
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif /* HAVE_SANDBOX_H */


/*
 * 'main()' - Apply sandbox profile and execute program.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  uid_t	uid;				/* UID */
  gid_t	gid;				/* GID */
  int	niceval;			/* Nice value */
#ifdef HAVE_SANDBOX_H
  char	*sandbox_error = NULL;		/* Sandbox error, if any */
#endif /* HAVE_SANDBOX_H */


 /*
  * Check that we have enough arguments...
  */

  if (argc < 7)
  {
    puts("Usage: cups-exec /path/to/profile UID GID NICE /path/to/program argv0 argv1 ... argvN");
    return (1);
  }

 /*
  * Make sure side and back channel FDs are non-blocking...
  */

  fcntl(3, F_SETFL, O_NDELAY);
  fcntl(4, F_SETFL, O_NDELAY);

#ifdef HAVE_SANDBOX_H
 /*
  * Run in a separate security profile...
  */

  if (strcmp(argv[1], "none") &&
      sandbox_init(argv[1], SANDBOX_NAMED_EXTERNAL, &sandbox_error))
  {
    fprintf(stderr, "DEBUG: sandbox_init failed: %s (%s)\n", sandbox_error,
	    strerror(errno));
    sandbox_free_error(sandbox_error);
    return (1);
  }
#endif /* HAVE_SANDBOX_H */

 /*
  * Change UID, GID, and nice value...
  */

  uid     = (uid_t)atoi(argv[2]);
  gid     = (gid_t)atoi(argv[3]);
  niceval = atoi(argv[4]);

  if (uid)
    nice(niceval);

  if (!getuid())
  {
    if (setgid(gid))
      exit(errno + 100);

    if (setgroups(1, &gid))
      exit(errno + 100);

    if (uid && setuid(uid))
      exit(errno + 100);
  }

  umask(077);

 /*
  * Execute the program...
  */

  execv(argv[5], argv + 6);

 /*
  * If we get here, execv() failed...
  */

  fprintf(stderr, "DEBUG: execv failed: %s\n", strerror(errno));
  return (1);
}


/*
 * End of "$Id$".
 */
