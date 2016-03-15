/*
 * "$Id: cups-exec.c 11144 2013-07-17 02:45:55Z msweet $"
 *
 *   Sandbox helper for CUPS.
 *
 *   Copyright 2007-2013 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Usage:
 *
 *     cups-exec /path/to/profile /path/to/program argv0 argv1 ... argvN
 *
 * Contents:
 *
 *   main() - Apply sandbox profile and execute program.
 */

/*
 * Include necessary headers...
 */

#include <cups/string-private.h>
#include <unistd.h>
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
#ifdef HAVE_SANDBOX_H
  char	*sandbox_error = NULL;		/* Sandbox error, if any */
#endif /* HAVE_SANDBOX_H */


 /*
  * Check that we have enough arguments...
  */

  if (argc < 4)
  {
    puts("Usage: cups-exec /path/to/profile /path/to/program argv0 argv1 ... "
         "argvN");
    return (1);
  }

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
  * Execute the program...
  */

  execv(argv[2], argv + 3);

 /*
  * If we get here, execv() failed...
  */

  fprintf(stderr, "DEBUG: execv failed: %s\n", strerror(errno));
  return (1);
}


/*
 * End of "$Id: cups-exec.c 11144 2013-07-17 02:45:55Z msweet $".
 */
