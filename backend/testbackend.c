/*
 * "$Id: testbackend.c 11594 2014-02-14 20:09:01Z msweet $"
 *
 * Backend test program for CUPS.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2005 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * "LICENSE" which should have been included with this file.  If this
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers.
 */

#include <cups/string-private.h>
#include <cups/cups.h>
#include <cups/sidechannel.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>


/*
 * Local globals...
 */

static int	job_canceled = 0;


/*
 * Local functions...
 */

static void	sigterm_handler(int sig);
static void	usage(void) __attribute__((noreturn));
static void	walk_cb(const char *oid, const char *data, int datalen,
		        void *context);


/*
 * 'main()' - Run the named backend.
 *
 * Usage:
 *
 *    testbackend [-s] [-t] device-uri job-id user title copies options [file]
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		first_arg,		/* First argument for backend */
		do_cancel = 0,		/* Simulate a cancel-job via SIGTERM */
		do_ps = 0,		/* Do PostScript query+test? */
		do_pcl = 0,		/* Do PCL query+test? */
		do_side_tests = 0,	/* Test side-channel ops? */
		do_trickle = 0,		/* Trickle data to backend */
		do_walk = 0,		/* Do OID lookup (0) or walking (1) */
		show_log = 0;		/* Show log messages from backends? */
  const char	*oid = ".1.3.6.1.2.1.43.10.2.1.4.1.1";
  					/* OID to lookup or walk */
  char		scheme[255],		/* Scheme in URI == backend */
		backend[1024],		/* Backend path */
		libpath[1024],		/* Path for libcups */
		*ptr;			/* Pointer into path */
  const char	*serverbin;		/* CUPS_SERVERBIN environment variable */
  int		fd,			/* Temporary file descriptor */
		back_fds[2],		/* Back-channel pipe */
		side_fds[2],		/* Side-channel socket */
		data_fds[2],		/* Data pipe */
		back_pid = -1,		/* Backend process ID */
		data_pid = -1,		/* Trickle process ID */
		pid,			/* Process ID */
		status;			/* Exit status */


 /*
  * Get the current directory and point the run-time linker at the "cups"
  * subdirectory...
  */

  if (getcwd(libpath, sizeof(libpath)) &&
      (ptr = strrchr(libpath, '/')) != NULL && !strcmp(ptr, "/backend"))
  {
    strlcpy(ptr, "/cups", sizeof(libpath) - (size_t)(ptr - libpath));
    if (!access(libpath, 0))
    {
#ifdef __APPLE__
      fprintf(stderr, "Setting DYLD_LIBRARY_PATH to \"%s\".\n", libpath);
      setenv("DYLD_LIBRARY_PATH", libpath, 1);
#else
      fprintf(stderr, "Setting LD_LIBRARY_PATH to \"%s\".\n", libpath);
      setenv("LD_LIBRARY_PATH", libpath, 1);
#endif /* __APPLE__ */
    }
    else
      perror(libpath);
  }

 /*
  * See if we have side-channel tests to do...
  */

  for (first_arg = 1;
       argv[first_arg] && argv[first_arg][0] == '-';
       first_arg ++)
    if (!strcmp(argv[first_arg], "-d"))
      show_log = 1;
    else if (!strcmp(argv[first_arg], "-cancel"))
      do_cancel = 1;
    else if (!strcmp(argv[first_arg], "-pcl"))
      do_pcl = 1;
    else if (!strcmp(argv[first_arg], "-ps"))
      do_ps = 1;
    else if (!strcmp(argv[first_arg], "-s"))
      do_side_tests = 1;
    else if (!strcmp(argv[first_arg], "-t"))
      do_trickle = 1;
    else if (!strcmp(argv[first_arg], "-get") && (first_arg + 1) < argc)
    {
      first_arg ++;

      do_side_tests = 1;
      oid           = argv[first_arg];
    }
    else if (!strcmp(argv[first_arg], "-walk") && (first_arg + 1) < argc)
    {
      first_arg ++;

      do_side_tests = 1;
      do_walk       = 1;
      oid           = argv[first_arg];
    }
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

  if (do_trickle || do_pcl || do_ps || do_cancel)
  {
    pipe(data_fds);

    signal(SIGTERM, sigterm_handler);

    if ((data_pid = fork()) == 0)
    {
     /*
      * Trickle/query child comes here.  Rearrange file descriptors so that
      * FD 1, 3, and 4 point to the backend...
      */

      if ((fd = open("/dev/null", O_RDONLY)) != 0)
      {
        dup2(fd, 0);
	close(fd);
      }

      if (data_fds[1] != 1)
      {
        dup2(data_fds[1], 1);
	close(data_fds[1]);
      }
      close(data_fds[0]);

      if (back_fds[0] != 3)
      {
        dup2(back_fds[0], 3);
        close(back_fds[0]);
      }
      close(back_fds[1]);

      if (side_fds[0] != 4)
      {
        dup2(side_fds[0], 4);
        close(side_fds[0]);
      }
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
      else if (do_cancel)
      {
       /*
        * Write PS or PCL lines until we see SIGTERM...
	*/

        int	line = 0, page = 0;	/* Current line and page */
	ssize_t	bytes;			/* Number of bytes of response data */
	char	buffer[1024];		/* Output buffer */


        if (do_pcl)
	  write(1, "\033E", 2);
	else
	  write(1, "%!\n/Courier findfont 12 scalefont setfont 0 setgray\n", 52);

        while (!job_canceled)
	{
	  if (line == 0)
	  {
	    page ++;

	    if (do_pcl)
	      snprintf(buffer, sizeof(buffer), "PCL Page %d\r\n\r\n", page);
	    else
	      snprintf(buffer, sizeof(buffer),
	               "18 732 moveto (PS Page %d) show\n", page);

	    write(1, buffer, strlen(buffer));
	  }

          line ++;

	  if (do_pcl)
	    snprintf(buffer, sizeof(buffer), "Line %d\r\n", line);
	  else
	    snprintf(buffer, sizeof(buffer), "18 %d moveto (Line %d) show\n",
		     720 - line * 12, line);

	  write(1, buffer, strlen(buffer));

          if (line >= 55)
	  {
	   /*
	    * Eject after 55 lines...
	    */

	    line = 0;
	    if (do_pcl)
	      write(1, "\014", 1);
	    else
	      write(1, "showpage\n", 9);
	  }

	 /*
	  * Check for back-channel data...
	  */

	  if ((bytes = cupsBackChannelRead(buffer, sizeof(buffer), 0)) > 0)
	    write(2, buffer, (size_t)bytes);

	 /*
	  * Throttle output to ~100hz...
	  */

	  usleep(10000);
	}

       /*
        * Eject current page with info...
	*/

        if (do_pcl)
	  snprintf(buffer, sizeof(buffer),
		   "Canceled on line %d of page %d\r\n\014\033E", line, page);
	else
	  snprintf(buffer, sizeof(buffer),
	           "\n18 %d moveto (Canceled on line %d of page %d)\nshowpage\n",
		   720 - line * 12, line, page);

	write(1, buffer, strlen(buffer));

       /*
        * See if we get any back-channel data...
	*/

        while ((bytes = cupsBackChannelRead(buffer, sizeof(buffer), 5.0)) > 0)
	  write(2, buffer, (size_t)bytes);

	exit(0);
      }
      else
      {
       /*
        * Do PS or PCL query + test pages.
	*/

        char		buffer[1024];	/* Buffer for response data */
	ssize_t		bytes;		/* Number of bytes of response data */
	double		timeout;	/* Timeout */
	const char	*data;		/* Data to send */
        static const char *pcl_data =	/* PCL data */
		"\033%-12345X@PJL\r\n"
		"@PJL JOB NAME = \"Hello, World!\"\r\n"
		"@PJL INFO USTATUS\r\n"
		"@PJL ENTER LANGUAGE = PCL\r\n"
		"\033E"
		"Hello, World!\n"
		"\014"
		"\033%-12345X@PJL\r\n"
		"@PJL EOJ NAME=\"Hello, World!\"\r\n"
		"\033%-12345X";
        static const char *ps_data =	/* PostScript data */
		"%!\n"
		"save\n"
		"product = flush\n"
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
		"/Courier findfont 12 scalefont setfont\n"
		"0 setgray 36 720 moveto (Hello, ) show product show (!) show\n"
		"showpage\n"
		"restore\n"
		"\004";


	if (do_pcl)
	  data = pcl_data;
	else
	  data = ps_data;

        write(1, data, strlen(data));
	write(2, "DEBUG: START\n", 13);
	timeout = 60.0;
        while ((bytes = cupsBackChannelRead(buffer, sizeof(buffer),
	                                    timeout)) > 0)
	{
	  write(2, buffer, (size_t)bytes);
	  timeout = 5.0;
	}
	write(2, "\nDEBUG: END\n", 12);
      }

      exit(0);
    }
    else if (data_pid < 0)
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

  if ((back_pid = fork()) == 0)
  {
   /*
    * Child comes here...
    */

    if (do_trickle || do_ps || do_pcl || do_cancel)
    {
      if (data_fds[0] != 0)
      {
        dup2(data_fds[0], 0);
        close(data_fds[0]);
      }
      close(data_fds[1]);
    }

    if (!show_log)
    {
      if ((fd = open("/dev/null", O_WRONLY)) != 2)
      {
        dup2(fd, 2);
	close(fd);
      }
    }

    if (back_fds[1] != 3)
    {
      dup2(back_fds[1], 3);
      close(back_fds[0]);
    }
    close(back_fds[1]);

    if (side_fds[1] != 4)
    {
      dup2(side_fds[1], 4);
      close(side_fds[0]);
    }
    close(side_fds[1]);

    execv(backend, argv + first_arg);
    fprintf(stderr, "testbackend: Unable to execute \"%s\": %s\n", backend,
            strerror(errno));
    return (errno);
  }
  else if (back_pid < 0)
  {
    perror("testbackend: Unable to fork");
    return (1);
  }

 /*
  * Parent comes here, setup back and side channel file descriptors...
  */

  if (do_trickle || do_ps || do_pcl || do_cancel)
  {
    close(data_fds[0]);
    close(data_fds[1]);
  }

  if (back_fds[0] != 3)
  {
    dup2(back_fds[0], 3);
    close(back_fds[0]);
  }
  close(back_fds[1]);

  if (side_fds[0] != 4)
  {
    dup2(side_fds[0], 4);
    close(side_fds[0]);
  }
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


    sleep(2);

    length   = 0;
    scstatus = cupsSideChannelDoRequest(CUPS_SC_CMD_DRAIN_OUTPUT, buffer,
                                        &length, 60.0);
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

    if (do_walk)
    {
     /*
      * Walk the OID tree...
      */

      scstatus = cupsSideChannelSNMPWalk(oid, 5.0, walk_cb, NULL);
      printf("CUPS_SC_CMD_SNMP_WALK returned %s\n", statuses[scstatus]);
    }
    else
    {
     /*
      * Lookup the same OID twice...
      */

      length   = sizeof(buffer);
      scstatus = cupsSideChannelSNMPGet(oid, buffer, &length, 5.0);
      printf("CUPS_SC_CMD_SNMP_GET %s returned %s, %d bytes (%s)\n", oid,
	     statuses[scstatus], (int)length, buffer);

      length   = sizeof(buffer);
      scstatus = cupsSideChannelSNMPGet(oid, buffer, &length, 5.0);
      printf("CUPS_SC_CMD_SNMP_GET %s returned %s, %d bytes (%s)\n", oid,
	     statuses[scstatus], (int)length, buffer);
    }

    length   = 0;
    scstatus = cupsSideChannelDoRequest(CUPS_SC_CMD_SOFT_RESET, buffer,
                                        &length, 5.0);
    printf("CUPS_SC_CMD_SOFT_RESET returned %s\n", statuses[scstatus]);
  }

  if (do_cancel)
  {
    sleep(1);
    kill(data_pid, SIGTERM);
    kill(back_pid, SIGTERM);
  }

  while ((pid = wait(&status)) > 0)
  {
    if (status)
    {
      if (WIFEXITED(status))
	printf("%s exited with status %d!\n",
	       pid == back_pid ? backend : "test",
	       WEXITSTATUS(status));
      else
	printf("%s crashed with signal %d!\n",
	       pid == back_pid ? backend : "test",
	       WTERMSIG(status));
    }
  }

 /*
  * Exit accordingly...
  */

  return (status != 0);
}


/*
 * 'sigterm_handler()' - Flag when we get SIGTERM.
 */

static void
sigterm_handler(int sig)		/* I - Signal */
{
  (void)sig;

  job_canceled = 1;
}


/*
 * 'usage()' - Show usage information.
 */

static void
usage(void)
{
  puts("Usage: testbackend [-cancel] [-d] [-ps | -pcl] [-s [-get OID] "
       "[-walk OID]] [-t] device-uri job-id user title copies options [file]");
  puts("");
  puts("Options:");
  puts("  -cancel     Simulate a canceled print job after 2 seconds.");
  puts("  -d          Show log messages from backend.");
  puts("  -get OID    Lookup the specified SNMP OID.");
  puts("              (.1.3.6.1.2.1.43.10.2.1.4.1.1 is a good one for printers)");
  puts("  -pcl        Send PCL+PJL query and test page to backend.");
  puts("  -ps         Send PostScript query and test page to backend.");
  puts("  -s          Do side-channel + SNMP tests.");
  puts("  -t          Send spaces slowly to backend ('trickle').");
  puts("  -walk OID   Walk the specified SNMP OID.");
  puts("              (.1.3.6.1.2.1.43 is a good one for printers)");

  exit(1);
}


/*
 * 'walk_cb()' - Show results of cupsSideChannelSNMPWalk...
 */

static void
walk_cb(const char *oid,		/* I - OID */
        const char *data,		/* I - Data */
	int        datalen,		/* I - Length of data */
	void       *context)		/* I - Context (unused) */
{
  char temp[80];

  (void)context;

  if ((size_t)datalen > (sizeof(temp) - 1))
  {
    memcpy(temp, data, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
  }
  else
  {
    memcpy(temp, data, (size_t)datalen);
    temp[datalen] = '\0';
  }

  printf("CUPS_SC_CMD_SNMP_WALK %s, %d bytes (%s)\n", oid, datalen, temp);
}


/*
 * End of "$Id: testbackend.c 11594 2014-02-14 20:09:01Z msweet $".
 */
