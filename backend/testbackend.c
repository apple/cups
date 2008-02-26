/*
 * "$Id$"
 *
 *   Backend test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   "LICENSE" which should have been included with this file.  If this
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main() - Run the named backend.
 */

/*
 * Include necessary headers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <cups/string.h>
#include <cups/cups.h>
#include <cups/sidechannel.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>


/*
 * 'main()' - Run the named backend.
 *
 * Usage:
 *
 *    betest [-s] device-uri job-id user title copies options [file]
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		first_arg = 1,		/* First argument for backend */
		do_side_tests = 0;	/* Test side-channel ops? */
  char		scheme[255],		/* Scheme in URI == backend */
		backend[1024];		/* Backend path */
  const char	*serverbin;		/* CUPS_SERVERBIN environment variable */
  int		back_fds[2],		/* Back-channel pipe */
		side_fds[2],		/* Side-channel socket */
		pid,			/* Process ID */
		status;			/* Exit status */


  if (argc < 7 || argc > 9)
  {
    fputs("Usage: betest [-s] device-uri job-id user title copies options [file]\n",
          stderr);
    return (1);
  }

 /*
  * See if we have side-channel tests to do...
  */

  if (!strcmp(argv[first_arg], "-s"))
  {
    first_arg ++;
    do_side_tests = 1;
  }

 /*
  * Extract the scheme from the device-uri - that's the program we want to
  * execute.
  */

  if (sscanf(argv[first_arg], "%254[^:]", scheme) != 1)
  {
    fputs("testbackend: Bad device-uri - no colon!\n", stderr);
    return (1);
  }

  if (!access(scheme, X_OK))
    strlcpy(backend, scheme, sizeof(backend));
  else
  {
    if ((serverbin = getenv("CUPS_SERVERBIN")) == NULL)
      serverbin = CUPS_SERVERBIN;

    snprintf(backend, sizeof(backend), "%s/backend/%s", serverbin, scheme);
  }

 /*
  * Create the back-channel pipe and side-channel socket...
  */

  open("/dev/null", O_WRONLY);		/* Make sure fd 3 and 4 are used */
  open("/dev/null", O_WRONLY);

  pipe(back_fds);
  fcntl(back_fds[0], F_SETFL, fcntl(back_fds[0], F_GETFL) | O_NONBLOCK);
  fcntl(back_fds[1], F_SETFL, fcntl(back_fds[1], F_GETFL) | O_NONBLOCK);

  socketpair(AF_LOCAL, SOCK_STREAM, 0, side_fds);
  fcntl(side_fds[0], F_SETFL, fcntl(side_fds[0], F_GETFL) | O_NONBLOCK);
  fcntl(side_fds[1], F_SETFL, fcntl(side_fds[1], F_GETFL) | O_NONBLOCK);

 /*
  * Execute and return
  */

  if ((pid = fork()) == 0)
  {
   /*
    * Child comes here...
    */

    close(3);
    dup(back_fds[1]);
    close(back_fds[0]);
    close(back_fds[1]);

    close(4);
    dup(side_fds[1]);
    close(side_fds[0]);
    close(side_fds[1]);

    execv(backend, argv + first_arg);
    fprintf(stderr, "textbackend: Unable to execute \"%s\": %s\n", backend,
            strerror(errno));
    return (errno);
  }
  else if (pid < 0)
  {
    perror("testbackend: Unable to fork");
    return (1);
  }

 /*
  * Parent comes here, setup back and side channel file descriptors...
  */

  close(3);
  dup(back_fds[0]);
  close(back_fds[0]);
  close(back_fds[1]);

  close(4);
  dup(side_fds[0]);
  close(side_fds[0]);
  close(side_fds[1]);

 /*
  * Do side-channel tests as needed, then wait for the backend...
  */

  if (do_side_tests)
  {
    int			length;		/* Length of buffer */
    char		buffer[2049];	/* Buffer for reponse */
    cups_sc_status_t	scstatus;	/* Status of side-channel command */
    static const char * const statuses[] =
    {
      "CUPS_SC_STATUS_NONE",		/* No status */
      "CUPS_SC_STATUS_OK",		/* Operation succeeded */
      "CUPS_SC_STATUS_IO_ERROR",	/* An I/O error occurred */
      "CUPS_SC_STATUS_TIMEOUT",		/* The backend did not respond */
      "CUPS_SC_STATUS_NO_RESPONSE",	/* The device did not respond */
      "CUPS_SC_STATUS_BAD_MESSAGE",	/* The command/response message was invalid */
      "CUPS_SC_STATUS_TOO_BIG",		/* Response too big */
      "CUPS_SC_STATUS_NOT_IMPLEMENTED"	/* Command not implemented */
    };


    length   = 0;
    scstatus = cupsSideChannelDoRequest(CUPS_SC_CMD_DRAIN_OUTPUT, buffer,
                                        &length, 5.0);
    printf("CUPS_SC_CMD_DRAIN_OUTPUT returned %s\n", statuses[scstatus]);

    length   = 1;
    scstatus = cupsSideChannelDoRequest(CUPS_SC_CMD_GET_BIDI, buffer,
                                        &length, 5.0);
    printf("CUPS_SC_CMD_GET_BIDI returned %s, %d\n", statuses[scstatus], buffer[0]);

    length   = sizeof(buffer) - 1;
    scstatus = cupsSideChannelDoRequest(CUPS_SC_CMD_GET_DEVICE_ID, buffer,
                                        &length, 5.0);
    buffer[length] = '\0';
    printf("CUPS_SC_CMD_GET_DEVICE_ID returned %s, \"%s\"\n",
           statuses[scstatus], buffer);

    length   = 1;
    scstatus = cupsSideChannelDoRequest(CUPS_SC_CMD_GET_STATE, buffer,
                                        &length, 5.0);
    printf("CUPS_SC_CMD_GET_STATE returned %s, %02X\n", statuses[scstatus],
           buffer[0] & 255);

    length   = 0;
    scstatus = cupsSideChannelDoRequest(CUPS_SC_CMD_SOFT_RESET, buffer,
                                        &length, 5.0);
    printf("CUPS_SC_CMD_SOFT_RESET returned %s\n", statuses[scstatus]);
  }

  while (wait(&status) != pid);

  if (status)
  {
    if (WIFEXITED(status))
      printf("%s exited with status %d!\n", backend, WEXITSTATUS(status));
    else
      printf("%s crashed with signal %d!\n", backend, WTERMSIG(status));
  }

 /*
  * Exit accordingly...
  */

  return (status != 0);
}


/*
 * End of "$Id$".
 */
