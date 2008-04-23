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
 *   main()  - Run the named backend.
 *   usage() - Show usage information.
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
 * Local functions...
 */

static void	usage(void);


/*
 * 'main()' - Run the named backend.
 *
 * Usage:
 *
 *    betest [-s] [-t] device-uri job-id user title copies options [file]
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		first_arg,		/* First argument for backend */
		do_query = 0,		/* Do PostScript query? */
		do_side_tests = 0,	/* Test side-channel ops? */
		do_trickle = 0;		/* Trickle data to backend */
  char		scheme[255],		/* Scheme in URI == backend */
		backend[1024];		/* Backend path */
  const char	*serverbin;		/* CUPS_SERVERBIN environment variable */
  int		back_fds[2],		/* Back-channel pipe */
		side_fds[2],		/* Side-channel socket */
		data_fds[2],		/* Data pipe */
		pid,			/* Process ID */
		status;			/* Exit status */


 /*
  * See if we have side-channel tests to do...
  */

  for (first_arg = 1;
       argv[first_arg] && argv[first_arg][0] == '-';
       first_arg ++)
    if (!strcmp(argv[first_arg], "-ps"))
      do_query = 1;
    else if (!strcmp(argv[first_arg], "-s"))
      do_side_tests = 1;
    else if (!strcmp(argv[first_arg], "-t"))
      do_trickle = 1;
    else
      usage();

  argc -= first_arg;
  if (argc < 6 || argc > 7 || (argc == 7 && do_trickle))
    usage();

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
    if (access(backend, X_OK))
    {
      fprintf(stderr, "testbackend: Unknown device scheme \"%s\"!\n", scheme);
      return (1);
    }
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
  * Execute the trickle process as needed...
  */

  if (do_trickle || do_query)
  {
    pipe(data_fds);

    if ((pid = fork()) == 0)
    {
     /*
      * Trickle/query child comes here...  Rearrange file descriptors so that
      * FD 
      */

      close(0);
      open("/dev/null", O_RDONLY);

      close(1);
      dup(data_fds[1]);
      close(data_fds[0]);
      close(data_fds[1]);

      close(3);
      dup(back_fds[0]);
      close(back_fds[0]);
      close(back_fds[1]);

      close(4);
      dup(side_fds[0]);
      close(side_fds[0]);
      close(side_fds[1]);

      if (do_trickle)
      {
       /*
	* Write 10 spaces, 1 per second...
	*/

	int i;				/* Looping var */

	for (i = 0; i < 10; i ++)
	{
	  write(1, " ", 1);
	  sleep(1);
	}
      }
      else
      {
       /*
        * Do a simple PostScript query job to get the default page size.
	*/

        char	buffer[1024];		/* Buffer for response data */
	ssize_t	bytes;			/* Number of bytes of response data */
        static const char *ps_query =	/* PostScript query file */
		"%!\n"
		"save\n"
		"currentpagedevice /PageSize get aload pop\n"
		"2 copy gt {exch} if\n"
		"(Unknown)\n"
		"19 dict\n"
		"dup [612 792] (Letter) put\n"
		"dup [612 1008] (Legal) put\n"
		"dup [612 935] (w612h935) put\n"
		"dup [522 756] (Executive) put\n"
		"dup [595 842] (A4) put\n"
		"dup [420 595] (A5) put\n"
		"dup [499 709] (ISOB5) put\n"
		"dup [516 728] (B5) put\n"
		"dup [612 936] (w612h936) put\n"
		"dup [284 419] (Postcard) put\n"
		"dup [419.5 567] (DoublePostcard) put\n"
		"dup [558 774] (w558h774) put\n"
		"dup [553 765] (w553h765) put\n"
		"dup [522 737] (w522h737) put\n"
		"dup [499 709] (EnvISOB5) put\n"
		"dup [297 684] (Env10) put\n"
		"dup [459 649] (EnvC5) put\n"
		"dup [312 624] (EnvDL) put\n"
		"dup [279 540] (EnvMonarch) put\n"
		"{ exch aload pop 4 index sub abs 5 le exch\n"
		"  5 index sub abs 5 le and\n"
		"  {exch pop exit} {pop} ifelse\n"
		"} bind forall\n"
		"= flush pop pop\n"
		"restore\n"
		"\004";


        write(1, ps_query, strlen(ps_query));
	write(2, "DEBUG: START\n", 13);
        while ((bytes = cupsBackChannelRead(buffer, sizeof(buffer), 30.0)) > 0)
	  write(2, buffer, bytes);
	write(2, "\nDEBUG: END\n", 12);
      }

      exit(0);
    }
    else if (pid < 0)
    {
      perror("testbackend: Unable to fork");
      return (1);
    }
  }
  else
    data_fds[0] = data_fds[1] = -1;

 /*
  * Execute the backend...
  */

  if ((pid = fork()) == 0)
  {
   /*
    * Child comes here...
    */

    if (do_trickle || do_query)
    {
      close(0);
      dup(data_fds[0]);
      close(data_fds[0]);
      close(data_fds[1]);
    }

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

  if (do_trickle || do_query)
  {
    close(data_fds[0]);
    close(data_fds[1]);
  }

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
 * 'usage()' - Show usage information.
 */

static void
usage(void)
{
  fputs("Usage: betest [-ps] [-s] [-t] device-uri job-id user title copies "
        "options [file]\n", stderr);
  exit(1);
}


/*
 * End of "$Id$".
 */
