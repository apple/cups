/*
 * "$Id$"
 *
 *   cups-lpd test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 2006 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   main()          - Simulate an LPD client.
 *   do_command()    - Send the LPD command and wait for a response.
 *   print_job()     - Submit a file for printing.
 *   print_waiting() - Print waiting jobs.
 *   remove_job()    - Cancel a print job.
 *   status_long()   - Show the long printer status.
 *   status_short()  - Show the short printer status.
 *   usage()         - Show program usage...
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>


/*
 * Local functions...
 */

static int	do_command(int outfd, int infd, const char *command);
static int	print_job(int outfd, int infd, char *dest, char **args);
static int	print_waiting(int outfd, int infd, char *dest);
static int	remove_job(int outfd, int infd, char *dest, char **args);
static int	status_long(int outfd, int infd, char *dest, char **args);
static int	status_short(int outfd, int infd, char *dest, char **args);
static void	usage(void);


/*
 * 'main()' - Simulate an LPD client.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int	i;				/* Looping var */
  int	status;				/* Test status */
  char	*op,				/* Operation to test */
	**opargs,			/* Remaining arguments */
	*dest;				/* Destination */
  int	cupslpd_argc;			/* Argument count for cups-lpd */
  char	*cupslpd_argv[1000];		/* Arguments for cups-lpd */
  int	cupslpd_stdin[2],		/* Standard input for cups-lpd */
	cupslpd_stdout[2],		/* Standard output for cups-lpd */
	cupslpd_pid,			/* Process ID for cups-lpd */
	cupslpd_status;			/* Status of cups-lpd process */


 /*
  * Collect command-line arguments...
  */

  op              = NULL;
  opargs          = NULL;
  dest            = NULL;
  cupslpd_argc    = 1;
  cupslpd_argv[0] = (char *)"cups-lpd";

  for (i = 1; i < argc; i ++)
    if (!strncmp(argv[i], "-o", 2))
    {
      cupslpd_argv[cupslpd_argc++] = argv[i];

      if (!argv[i][2])
      {
        i ++;

	if (i >= argc)
	  usage();

	cupslpd_argv[cupslpd_argc++] = argv[i];
      }
    }
    else if (argv[i][0] == '-')
      usage();
    else if (!op)
      op = argv[i];
    else if (!dest)
      dest = argv[i];
    else
    {
      opargs = argv + i;
      break;
    }

  if (!op ||
      (!strcmp(op, "print-job") && (!dest || !opargs)) ||
      (!strcmp(op, "remove-job") && (!dest || !opargs)) ||
      (strcmp(op, "print-job") && strcmp(op, "print-waiting") &&
       strcmp(op, "remove-job") && strcmp(op, "status-long") &&
       strcmp(op, "status-short")))
  {
    printf("op=\"%s\", dest=\"%s\", opargs=%p\n", op, dest, opargs);
    usage();
  }

 /*
  * Run the cups-lpd program using pipes...
  */

  cupslpd_argv[cupslpd_argc] = NULL;

  pipe(cupslpd_stdin);
  pipe(cupslpd_stdout);

  if ((cupslpd_pid = fork()) < 0)
  {
   /*
    * Error!
    */

    perror("testlpd: Unable to fork");
    return (1);
  }
  else if (cupslpd_pid == 0)
  {
   /*
    * Child goes here...
    */

    close(0);
    dup(cupslpd_stdin[0]);
    close(cupslpd_stdin[0]);
    close(cupslpd_stdin[1]);

    close(1);
    dup(cupslpd_stdout[1]);
    close(cupslpd_stdout[0]);
    close(cupslpd_stdout[1]);

    execv("./cups-lpd", cupslpd_argv);

    perror("testlpd: Unable to exec ./cups-lpd");
    exit(errno);
  }
  else
  {
    close(cupslpd_stdin[0]);
    close(cupslpd_stdout[1]);
  }

 /*
  * Do the operation test...
  */

  if (!strcmp(op, "print-job"))
    status = print_job(cupslpd_stdin[1], cupslpd_stdout[0], dest, opargs);
  else if (!strcmp(op, "print-waiting"))
    status = print_waiting(cupslpd_stdin[1], cupslpd_stdout[0], dest);
  else if (!strcmp(op, "remove-job"))
    status = remove_job(cupslpd_stdin[1], cupslpd_stdout[0], dest, opargs);
  else if (!strcmp(op, "status-long"))
    status = status_long(cupslpd_stdin[1], cupslpd_stdout[0], dest, opargs);
  else if (!strcmp(op, "status-short"))
    status = status_short(cupslpd_stdin[1], cupslpd_stdout[0], dest, opargs);
  else
  {
    printf("Unknown operation \"%s\"!\n", op);
    status = 1;
  }

 /*
  * Kill the test program...
  */

  close(cupslpd_stdin[1]);
  close(cupslpd_stdout[0]);

  while (wait(&cupslpd_status) != cupslpd_pid);

  printf("cups-lpd exit status was %d...\n", cupslpd_status);

 /*
  * Return the test status...
  */

  return (status);
}


/*
 * 'do_command()' - Send the LPD command and wait for a response.
 */

static int				/* O - Status from cups-lpd */
do_command(int        outfd,		/* I - Command file descriptor */
           int        infd,		/* I - Response file descriptor */
	   const char *command)		/* I - Command line to send */
{
  int	len;				/* Length of command line */
  char	status;				/* Status byte */


  printf("COMMAND: %02X %s", command[0], command + 1);

  len = strlen(command);

  if (write(outfd, command, len) < len)
  {
    puts("    Write failed!");
    return (-1);
  }

  if (read(infd, &status, 1) < 1)
    puts("STATUS: ERROR");
  else
    printf("STATUS: %d\n", status);

  return (status);
}


/*
 * 'print_job()' - Submit a file for printing.
 */

static int				/* O - Status from cups-lpd */
print_job(int  outfd,			/* I - Command file descriptor */
          int  infd,			/* I - Response file descriptor */
	  char *dest,			/* I - Destination */
	  char **args)			/* I - Arguments */
{
  int		fd;			/* Print file descriptor */
  char		command[1024],		/* Command buffer */
		control[1024],		/* Control file */
		buffer[8192];		/* Print buffer */
  int		status;			/* Status of command */
  struct stat	fileinfo;		/* File information */
  char		*jobname;		/* Job name */
  int		sequence;		/* Sequence number */
  int		bytes;			/* Bytes read/written */


 /*
  * Check the print file...
  */

  if (stat(args[0], &fileinfo))
  {
    perror(args[0]);
    return (-1);
  }

  if ((fd = open(args[0], O_RDONLY)) < 0)
  {
    perror(args[0]);
    return (-1);
  }

 /*
  * Send the "receive print job" command...
  */

  snprintf(command, sizeof(command), "\002%s\n", dest);
  if ((status = do_command(outfd, infd, command)) != 0)
  {
    close(fd);
    return (status);
  }

 /*
  * Format a control file string that will be used to submit the job...
  */

  if ((jobname = strrchr(args[0], '/')) != NULL)
    jobname ++;
  else
    jobname = args[0];

  sequence = (int)getpid() % 1000;

  snprintf(control, sizeof(control),
           "Hlocalhost\n"
           "P%s\n"
           "J%s\n"
           "ldfA%03dlocalhost\n"
           "UdfA%03dlocalhost\n"
           "N%s\n",
	   cupsUser(), jobname, sequence, sequence, jobname);

 /*
  * Send the control file...
  */

  bytes = strlen(control);

  snprintf(command, sizeof(command), "\002%d cfA%03dlocalhost\n",
           bytes, sequence);

  if ((status = do_command(outfd, infd, command)) != 0)
  {
    close(fd);
    return (status);
  }

  bytes ++;

  if (write(outfd, control, bytes) < bytes)
  {
    printf("CONTROL: Unable to write %d bytes!\n", bytes);
    close(fd);
    return (-1);
  }

  printf("CONTROL: Wrote %d bytes.\n", bytes);

  if (read(infd, command, 1) < 1)
  {
    puts("STATUS: ERROR");
    close(fd);
    return (-1);
  }
  else
  {
    status = command[0];

    printf("STATUS: %d\n", status);
  }

 /*
  * Send the data file...
  */

  snprintf(command, sizeof(command), "\003%d dfA%03dlocalhost\n",
           (int)fileinfo.st_size, sequence);

  if ((status = do_command(outfd, infd, command)) != 0)
  {
    close(fd);
    return (status);
  }

  while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
  {
    if (write(outfd, buffer, bytes) < bytes)
    {
      printf("DATA: Unable to write %d bytes!\n", bytes);
      close(fd);
      return (-1);
    }
  }

  write(outfd, "", 1);

  close(fd);

  printf("DATA: Wrote %d bytes.\n", (int)fileinfo.st_size);

  if (read(infd, command, 1) < 1)
  {
    puts("STATUS: ERROR");
    close(fd);
    return (-1);
  }
  else
  {
    status = command[0];

    printf("STATUS: %d\n", status);
  }

  return (status);
}


/*
 * 'print_waiting()' - Print waiting jobs.
 */

static int				/* O - Status from cups-lpd */
print_waiting(int  outfd,		/* I - Command file descriptor */
              int  infd,		/* I - Response file descriptor */
	      char *dest)		/* I - Destination */
{
  char		command[1024];		/* Command buffer */


 /*
  * Send the "print waiting jobs" command...
  */

  snprintf(command, sizeof(command), "\001%s\n", dest);

  return (do_command(outfd, infd, command));
}


/*
 * 'remove_job()' - Cancel a print job.
 */

static int				/* O - Status from cups-lpd */
remove_job(int  outfd,			/* I - Command file descriptor */
           int  infd,			/* I - Response file descriptor */
	   char *dest,			/* I - Destination */
	   char **args)			/* I - Arguments */
{
  int		i;			/* Looping var */
  char		command[1024];		/* Command buffer */

 /*
  * Send the "remove jobs" command...
  */

  snprintf(command, sizeof(command), "\005%s", dest);

  for (i = 0; args[i]; i ++)
  {
    strlcat(command, " ", sizeof(command));
    strlcat(command, args[i], sizeof(command));
  }

  strlcat(command, "\n", sizeof(command));

  return (do_command(outfd, infd, command));
}


/*
 * 'status_long()' - Show the long printer status.
 */

static int				/* O - Status from cups-lpd */
status_long(int  outfd,			/* I - Command file descriptor */
            int  infd,			/* I - Response file descriptor */
	    char *dest,			/* I - Destination */
	    char **args)		/* I - Arguments */
{
  char		command[1024],		/* Command buffer */
		buffer[8192];		/* Status buffer */
  int		bytes;			/* Bytes read/written */


 /*
  * Send the "send short status" command...
  */

  if (args)
    snprintf(command, sizeof(command), "\004%s %s\n", dest, args[0]);
  else
    snprintf(command, sizeof(command), "\004%s\n", dest);

  bytes = strlen(command);

  if (write(outfd, command, bytes) < bytes)
    return (-1);

 /*
  * Read the status back...
  */

  while ((bytes = read(infd, buffer, sizeof(buffer))) > 0)
  {
    fwrite(buffer, 1, bytes, stdout);
    fflush(stdout);
  }

  return (0);
}


/*
 * 'status_short()' - Show the short printer status.
 */

static int				/* O - Status from cups-lpd */
status_short(int  outfd,		/* I - Command file descriptor */
             int  infd,			/* I - Response file descriptor */
	     char *dest,		/* I - Destination */
	     char **args)		/* I - Arguments */
{
  char		command[1024],		/* Command buffer */
		buffer[8192];		/* Status buffer */
  int		bytes;			/* Bytes read/written */


 /*
  * Send the "send short status" command...
  */

  if (args)
    snprintf(command, sizeof(command), "\003%s %s\n", dest, args[0]);
  else
    snprintf(command, sizeof(command), "\003%s\n", dest);

  bytes = strlen(command);

  if (write(outfd, command, bytes) < bytes)
    return (-1);

 /*
  * Read the status back...
  */

  while ((bytes = read(infd, buffer, sizeof(buffer))) > 0)
  {
    fwrite(buffer, 1, bytes, stdout);
    fflush(stdout);
  }

  return (0);
}


/*
 * 'usage()' - Show program usage...
 */

static void
usage(void)
{
  puts("Usage: testlpd [options] print-job printer filename [... filename]");
  puts("       testlpd [options] print-waiting [printer or user]");
  puts("       testlpd [options] remove-job printer [user [job-id]]");
  puts("       testlpd [options] status-long [printer or user]");
  puts("       testlpd [options] status-short [printer or user]");
  puts("");
  puts("Options:");
  puts("    -o name=value");

  exit(0);
}


/*
 * End of "$Id$".
 */
