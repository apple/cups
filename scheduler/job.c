/*
 * "$Id: job.c,v 1.32 1999/07/13 13:06:21 mike Exp $"
 *
 *   Job management routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   AddJob()        - Add a new job to the job queue...
 *   CancelJob()     - Cancel the specified print job.
 *   CancelJobs()    - Cancel all jobs on the given printer or class.
 *   CheckJobs()     - Check the pending jobs and start any if the destination
 *                     is available.
 *   FindJob()       - Find the specified job.
 *   MoveJob()       - Move the specified job to a different destination.
 *   StartJob()      - Start a print job.
 *   StopJob()       - Stop a print job.
 *   UpdateJob()     - Read a status update from a job's filters.
 *   start_process() - Start a background process.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * Local functions...
 */

static int	start_process(char *command, char *argv[], char *envp[],
		              int in, int out, int err);


/*
 * 'AddJob()' - Add a new job to the job queue...
 */

job_t *			/* O - New job record */
AddJob(int  priority,	/* I - Job priority */
       char *dest)	/* I - Job destination */
{
  job_t	*job,		/* New job record */
	*current,	/* Current job in queue */
	*prev;		/* Previous job in queue */


  job = calloc(sizeof(job_t), 1);

  job->id       = NextJobId ++;
  job->priority = priority;
  strncpy(job->dest, dest, sizeof(job->dest) - 1);
  job->state    = IPP_JOB_HELD;

  NumJobs ++;

  for (current = Jobs, prev = NULL; current != NULL; prev = current, current = current->next)
    if (job->priority > current->priority)
      break;

  job->next = current;
  if (prev != NULL)
    prev->next = job;
  else
    Jobs = job;

  return (job);
}


/*
 * 'CancelJob()' - Cancel the specified print job.
 */

void
CancelJob(int id)	/* I - Job to cancel */
{
  job_t	*current,	/* Current job */
	*prev;		/* Previous job in list */


  DEBUG_printf(("CancelJob(%d)\n", id));

  for (current = Jobs, prev = NULL; current != NULL; prev = current, current = current->next)
    if (current->id == id)
    {
     /*
      * Stop any processes that are working on the current...
      */

      DEBUG_puts("CancelJob: found job in list.");

      if (current->state == IPP_JOB_PROCESSING)
	StopJob(current->id);

     /*
      * Update pointers...
      */

      if (prev == NULL)
        Jobs = current->next;
      else
        prev->next = current->next;

     /*
      * Free all memory used...
      */

      if (current->attrs != NULL)
        ippDelete(current->attrs);

     /*
      * Remove the print file for good...
      */

      unlink(current->filename);
      free(current);
      return;
    }
}


/*
 * 'CancelJobs()' - Cancel all jobs on the given printer or class.
 */

void
CancelJobs(char *dest)	/* I - Destination to cancel */
{
  job_t	*current,	/* Current job */
	*prev;		/* Previous job in list */


  for (current = Jobs, prev = NULL; current != NULL; prev = current)
    if (strcmp(current->dest, dest) == 0)
    {
     /*
      * Cancel all jobs matching this destination...
      */

      CancelJob(current->id);

      if (prev == NULL)
	current = Jobs;
      else
	current = prev->next;
    }
    else
      current = current->next;

  CheckJobs();
}


/*
 * 'CheckJobs()' - Check the pending jobs and start any if the destination
 *                 is available.
 */

void
CheckJobs(void)
{
  job_t		*current,
		*prev;
  printer_t	*printer;


  DEBUG_puts("CheckJobs()");

  for (current = Jobs, prev = NULL; current != NULL; prev = current)
  {
    DEBUG_printf(("CheckJobs: current->state = %d\n", current->state));

    if (current->state != IPP_JOB_PROCESSING)
    {
      DEBUG_printf(("CheckJobs: current->dest = \'%s\'\n", current->dest));

      if (FindClass(current->dest) != NULL)
        printer = FindAvailablePrinter(current->dest);
      else
        printer = FindPrinter(current->dest);

      if (printer == NULL && FindClass(current->dest) == NULL)
      {
       /*
        * Whoa, the printer and/or class for this destination went away;
	* cancel the job...
	*/

        LogMessage(LOG_WARN, "Printer/class %s has gone away; cancelling job %d!",
	           current->dest, current->id);
        CancelJob(current->id);

	if (prev == NULL)
	  current = Jobs;
	else
	  current = prev->next;
      }
      else if (printer != NULL)
      {
       /*
        * See if the printer is available; if so, start the job...
	*/

        DEBUG_printf(("CheckJobs: printer->state = %d\n", printer->state));

        if (printer->state == IPP_PRINTER_IDLE)
	  StartJob(current->id, printer);

        current = current->next;
      }
      else
        current = current->next;
    }
    else
      current = current->next;
  }
}


/*
 * 'FindJob()' - Find the specified job.
 */

job_t *			/* O - Job data */
FindJob(int id)		/* I - Job ID */
{
  job_t	*current;	/* Current job */


  for (current = Jobs; current != NULL; current = current->next)
    if (current->id == id)
      break;

  return (current);
}


/*
 * 'MoveJob()' - Move the specified job to a different destination.
 */

void
MoveJob(int id, char *dest)
{
  job_t	*current;	/* Current job */


  for (current = Jobs; current != NULL; current = current->next)
    if (current->id == id)
    {
      if (current->state == IPP_JOB_PENDING)
        strcpy(current->dest, dest);

      return;
    }
}


/*
 * 'StartJob()' - Start a print job.
 */

void
StartJob(int       id,		/* I - Job ID */
         printer_t *printer)	/* I - Printer to print job */
{
  job_t		*current;	/* Current job */
  int		i;		/* Looping var */
  int		num_filters;	/* Number of filters for job */
  mime_filter_t	*filters;	/* Filters for job */
  char		method[255],	/* Method for output */
		*optptr;	/* Pointer to options */
  ipp_attribute_t *attr;	/* Current attribute */
  int		pid;		/* Process ID of new filter process */
  int		statusfds[2],	/* Pipes used between the filters and scheduler */
		filterfds[2][2];/* Pipes used between the filters */
  char		*argv[8],	/* Filter command-line arguments */
		command[1024],	/* Full path to filter/backend command */
		jobid[255],	/* Job ID string */
		title[IPP_MAX_NAME],
				/* Job title string */
		copies[255],	/* # copies string */
		options[16384],	/* Full list of options */
		*envp[12],	/* Environment variables */
		language[255],	/* LANG environment variable */
		charset[255],	/* CHARSET environment variable */
		ppd[1024],	/* PPD environment variable */
		root[1024],	/* SERVER_ROOT environment variable */
		cache[255],	/* RIP_MAX_CACHE environment variable */
		tmpdir[1024];	/* TMPDIR environment variable */


  DEBUG_printf(("StartJob(%d, %08x)\n", id, printer));


  for (current = Jobs; current != NULL; current = current->next)
    if (current->id == id)
      break;

  if (current == NULL)
    return;

 /*
  * Update the printer and job state to "processing"...
  */

  DEBUG_puts("StartJob: found job in list.");

  current->state   = IPP_JOB_PROCESSING;
  current->status  = 0;
  current->printer = printer;
  printer->job     = current;
  SetPrinterState(printer, IPP_PRINTER_PROCESSING);

 /*
  * Figure out what filters are required to convert from
  * the source to the destination type...
  */

  num_filters = 0;

#ifdef DEBUG
  printf("Filtering from %s/%s to %s/%s...\n",
         current->filetype->super, current->filetype->type,
         printer->filetype->super, printer->filetype->type);
  printf("num_filters = %d\n", MimeDatabase->num_filters);
  for (i = 0; i < MimeDatabase->num_filters; i ++)
    printf("filters[%d] = %s/%s to %s/%s using \"%s\" (cost %d)\n",
	   i, MimeDatabase->filters[i].src->super,
	   MimeDatabase->filters[i].src->type,
	   MimeDatabase->filters[i].dst->super,
	   MimeDatabase->filters[i].dst->type,
	   MimeDatabase->filters[i].filter,
	   MimeDatabase->filters[i].cost);
#endif /* DEBUG */	       

  if (printer->type & CUPS_PRINTER_REMOTE)
  {
   /*
    * Remote jobs go directly to the remote job...
    */

    filters = NULL;
  }
  else
  {
   /*
    * Local jobs get filtered...
    */

    filters = mimeFilter(MimeDatabase, current->filetype,
                         printer->filetype, &num_filters);

    if (num_filters == 0)
    {
      LogMessage(LOG_ERROR, "Unable to convert file to printable format for job %s-%d!",
	         printer->name, current->id);
      CancelJob(current->id);
      return;
    }
  }

 /*
  * Building the options string is harder than it needs to be, but
  * for the moment we need to pass strings for command-line args and
  * not IPP attribute pointers... :)
  */

  optptr  = options;
  *optptr = '\0';

  sprintf(title, "%s-%d", printer->name, current->id);
  strcpy(copies, "1");

  for (attr = current->attrs->attrs; attr != NULL; attr = attr->next)
  {
    if (strcmp(attr->name, "copies") == 0 &&
	attr->value_tag == IPP_TAG_INTEGER)
      sprintf(copies, "%d", attr->values[0].integer);
    else if (strcmp(attr->name, "job-name") == 0 &&
	     (attr->value_tag == IPP_TAG_NAME ||
	      attr->value_tag == IPP_TAG_NAMELANG))
      strcpy(title, attr->values[0].string.text);
    else if ((attr->group_tag == IPP_TAG_JOB ||
	      attr->group_tag == IPP_TAG_EXTENSION) &&
	     (optptr - options) < (sizeof(options) - 128))
    {
      if (attr->value_tag == IPP_TAG_MIMETYPE ||
	  attr->value_tag == IPP_TAG_NAMELANG ||
	  attr->value_tag == IPP_TAG_TEXTLANG ||
	  attr->value_tag == IPP_TAG_URI ||
	  attr->value_tag == IPP_TAG_URISCHEME)
	continue;

      if (optptr > options)
	strcat(optptr, " ");

      if (attr->value_tag != IPP_TAG_BOOLEAN)
      {
	strcat(optptr, attr->name);
	strcat(optptr, "=");
      }

      for (i = 0; i < attr->num_values; i ++)
      {
	if (i)
	  strcat(optptr, ",");

	optptr += strlen(optptr);

	switch (attr->value_tag)
	{
	  case IPP_TAG_INTEGER :
	  case IPP_TAG_ENUM :
	      sprintf(optptr, "%d", attr->values[i].integer);
	      break;

	  case IPP_TAG_BOOLEAN :
	      if (!attr->values[i].boolean)
		strcat(optptr, "no");

	  case IPP_TAG_NOVALUE :
	      strcat(optptr, attr->name);
	      break;

	  case IPP_TAG_RANGE :
	      sprintf(optptr, "%d-%d", attr->values[i].range.lower,
		      attr->values[i].range.upper);
	      break;

	  case IPP_TAG_RESOLUTION :
	      sprintf(optptr, "%dx%d%s", attr->values[i].resolution.xres,
		      attr->values[i].resolution.yres,
		      attr->values[i].resolution.units == IPP_RES_PER_INCH ?
			  "dpi" : "dpc");
	      break;

          case IPP_TAG_STRING :
	  case IPP_TAG_TEXT :
	  case IPP_TAG_NAME :
	  case IPP_TAG_KEYWORD :
	  case IPP_TAG_CHARSET :
	  case IPP_TAG_LANGUAGE :
	      if (strchr(attr->values[i].string.text, ' ') != NULL ||
		  strchr(attr->values[i].string.text, '\t') != NULL ||
		  strchr(attr->values[i].string.text, '\n') != NULL)
	      {
		strcat(optptr, "\'");
		strcat(optptr, attr->values[i].string.text);
		strcat(optptr, "\'");
	      }
	      else
		strcat(optptr, attr->values[i].string.text);
	      break;
	}
      }

      optptr += strlen(optptr);
    }
  }

 /*
  * Build the command-line arguments for the filters.  Each filter
  * has 6 or 7 arguments:
  *
  *     argv[0] = printer
  *     argv[1] = job ID
  *     argv[2] = username
  *     argv[3] = title
  *     argv[4] = # copies
  *     argv[5] = options
  *     argv[6] = filename (optional; normally stdin)
  *
  * This allows legacy printer drivers that use the old System V
  * printing interface to be used by CUPS.
  */

  sprintf(jobid, "%d", current->id);

  argv[0] = printer->name;
  argv[1] = jobid;
  argv[2] = current->username;
  argv[3] = title;
  argv[4] = copies;
  argv[5] = options;
  argv[6] = current->filename;
  argv[7] = NULL;

  DEBUG_printf(("StartJob: args = \'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\'\n",
                argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]));

 /*
  * Create environment variable strings for the filters...
  */

  attr = ippFindAttribute(current->attrs, "attributes-natural-language",
                          IPP_TAG_LANGUAGE);
  sprintf(language, "LANG=%s", attr->values[0].string.text);

  attr = ippFindAttribute(current->attrs, "document-format",
                          IPP_TAG_MIMETYPE);
  if ((optptr = strstr(attr->values[0].string.text, "charset=")) != NULL)
    sprintf(charset, "CHARSET=%s", optptr + 8);
  else
  {
    attr = ippFindAttribute(current->attrs, "attributes-charset",
	                    IPP_TAG_CHARSET);
    sprintf(charset, "CHARSET=%s", attr->values[0].string.text);
  }

  sprintf(ppd, "PPD=%s/ppd/%s.ppd", ServerRoot, printer->name);
  sprintf(root, "SERVER_ROOT=%s", ServerRoot);
  sprintf(cache, "RIP_MAX_CACHE=%s", RIPCache);
  sprintf(tmpdir, "TMPDIR=%s", TempDir);

  envp[0]  = "PATH=/bin:/usr/bin";
  envp[1]  = "SOFTWARE=CUPS/1.0";
  envp[2]  = "TZ=GMT";
  envp[3]  = "USER=root";
  envp[4]  = charset;
  envp[5]  = language;
  envp[6]  = "TZ=GMT";
  envp[7]  = ppd;
  envp[8]  = root;
  envp[9]  = cache;
  envp[10] = tmpdir;
  envp[11] = NULL;

  DEBUG_puts(envp[0]);
  DEBUG_puts(envp[1]);
  DEBUG_puts(envp[2]);
  DEBUG_puts(envp[3]);
  DEBUG_puts(envp[4]);
  DEBUG_puts(envp[5]);
  DEBUG_puts(envp[6]);
  DEBUG_puts(envp[7]);
  DEBUG_puts(envp[8]);
  DEBUG_puts(envp[9]);
  DEBUG_puts(envp[10]);

 /*
  * Now create processes for all of the filters...
  */

  if (pipe(statusfds))
  {
    LogMessage(LOG_ERROR, "StartJob: unable to create status pipes - %s.",
	       strerror(errno));
    return;
  }

  DEBUG_printf(("statusfds = %d, %d\n", statusfds[0], statusfds[1]));

  current->pipe = statusfds[0];
  memset(current->procs, 0, sizeof(current->procs));

  if (num_filters > 0 && strcmp(filters[num_filters - 1].filter, "-") == 0)
    num_filters --;

  filterfds[1][0] = open("/dev/null", O_RDONLY);
  filterfds[1][1] = -1;
  DEBUG_printf(("filterfds[%d] = %d, %d\n", 1, filterfds[1][0],
                filterfds[1][1]));

  for (i = 0; i < num_filters; i ++)
  {
    if (i == 1)
      argv[6] = NULL;

    if (filters[i].filter[0] != '/')
      sprintf(command, "%s/filter/%s", ServerRoot, filters[i].filter);
    else
      strcpy(command, filters[i].filter);

    DEBUG_printf(("%s: %s %s %s %s %s %s %s\n", command, argv[0],
	          argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]));

    if (i < (num_filters - 1) ||
	strncmp(printer->device_uri, "file:", 5) != 0)
      pipe(filterfds[i & 1]);
    else
    {
      filterfds[i & 1][0] = -1;
      if (strncmp(printer->device_uri, "file:/dev/", 10) == 0)
	filterfds[i & 1][1] = open(printer->device_uri + 5,
	                           O_WRONLY | O_EXCL);
      else
	filterfds[i & 1][1] = open(printer->device_uri + 5,
	                           O_WRONLY | O_CREAT | O_TRUNC, 0666);
    }

    DEBUG_printf(("filterfds[%d] = %d, %d\n", i & 1, filterfds[i & 1][0],
         	  filterfds[i & 1][1]));

    pid = start_process(command, argv, envp, filterfds[!(i & 1)][0],
	                filterfds[i & 1][1], statusfds[1]);

    close(filterfds[!(i & 1)][0]);
    close(filterfds[!(i & 1)][1]);

    if (pid == 0)
    {
      StopPrinter(current->printer);
      return;
    }
    else
    {
      current->procs[i] = pid;

      DEBUG_printf(("StartJob: started %s - pid = %d.\n", command, pid));
    }
  }

  if (filters != NULL)
    free(filters);

 /*
  * Finally, pipe the final output into a backend process if needed...
  */

  if (strncmp(printer->device_uri, "file:", 5) != 0)
  {
    sscanf(printer->device_uri, "%[^:]", method);
    sprintf(command, "%s/backend/%s", ServerRoot, method);

    argv[0] = printer->device_uri;
    if (num_filters)
      argv[6] = NULL;

    DEBUG_printf(("%s: %s %s %s %s %s %s %s\n", command, argv[0],
	          argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]));

    filterfds[i & 1][0] = -1;
    filterfds[i & 1][1] = open("/dev/null", O_WRONLY);

    DEBUG_printf(("filterfds[%d] = %d, %d\n", i & 1, filterfds[i & 1][0],
        	  filterfds[i & 1][1]));

    pid = start_process(command, argv, envp, filterfds[!(i & 1)][0],
	                filterfds[i & 1][1], statusfds[1]);

    close(filterfds[!(i & 1)][0]);
    close(filterfds[!(i & 1)][1]);

    if (pid == 0)
    {
      StopPrinter(current->printer);
      return;
    }
    else
    {
      current->procs[i] = pid;

      DEBUG_printf(("StartJob: started %s - pid = %d.\n", command, pid));
    }
  }
  else
  {
    filterfds[i & 1][0] = -1;
    filterfds[i & 1][1] = -1;

    close(filterfds[!(i & 1)][0]);
    close(filterfds[!(i & 1)][1]);
  }

  close(filterfds[i & 1][0]);
  close(filterfds[i & 1][1]);

  close(statusfds[1]);

  FD_SET(current->pipe, &InputSet);
}


/*
 * 'StopJob()' - Stop a print job.
 */

void
StopJob(int id)
{
  int	i;		/* Looping var */
  job_t	*current;	/* Current job */


  DEBUG_printf(("StopJob(%d)\n", id));

  for (current = Jobs; current != NULL; current = current->next)
    if (current->id == id)
    {
      DEBUG_puts("StopJob: found job in list.");

      if (current->state == IPP_JOB_PROCESSING)
      {
        DEBUG_puts("StopJob: job state is \'processing\'.");

        if (current->status)
	  SetPrinterState(current->printer, IPP_PRINTER_STOPPED);
	else
	  SetPrinterState(current->printer, IPP_PRINTER_IDLE);

	current->state          = IPP_JOB_STOPPED;
        current->printer->job   = NULL;
        current->printer        = NULL;

        for (i = 0; current->procs[i]; i ++)
	  if (current->procs[i] > 0)
	    kill(current->procs[i], SIGTERM);
	current->procs[0] = 0;

        if (current->pipe)
        {
          close(current->pipe);
	  FD_CLR(current->pipe, &InputSet);
	  current->pipe = 0;
        }
      }
      return;
    }
}


/*
 * 'UpdateJob()' - Read a status update from a job's filters.
 */

void
UpdateJob(job_t *job)		/* I - Job to check */
{
  int		bytes;		/* Number of bytes read */
  char		*lineptr,	/* Pointer to end of line in buffer */
		*message;	/* Pointer to message text */
  int		loglevel;	/* Log level for message */
  static int	bufused = 0;	/* Amount of buffer used */
  static char	buffer[8192];	/* Data buffer */


  if ((bytes = read(job->pipe, buffer + bufused, sizeof(buffer) - bufused - 1)) > 0)
  {
    bufused += bytes;
    buffer[bufused] = '\0';

    while ((lineptr = strchr(buffer, '\n')) != NULL)
    {
     /*
      * Terminate each line and process it...
      */

      *lineptr++ = '\0';

     /*
      * Figure out the logging level...
      */

      if (strncmp(buffer, "ERROR:", 6) == 0)
      {
        loglevel = LOG_ERROR;
	message  = buffer + 6;
      }
      else if (strncmp(buffer, "WARNING:", 8) == 0)
      {
        loglevel = LOG_WARN;
	message  = buffer + 8;
      }
      if (strncmp(buffer, "INFO:", 5) == 0)
      {
        loglevel = LOG_INFO;
	message  = buffer + 5;
      }
      else if (strncmp(buffer, "DEBUG:", 6) == 0)
      {
        loglevel = LOG_DEBUG;
	message  = buffer + 6;
      }
      else if (strncmp(buffer, "PAGE:", 5) == 0)
      {
        loglevel = LOG_PAGE;
	message  = buffer + 5;
      }
      else
      {
        loglevel = LOG_DEBUG;
	message  = buffer;
      }

     /*
      * Skip leading whitespace in the message...
      */

      while (isspace(*message))
        message ++;

     /*
      * Send it to the log file and printer state message as needed...
      */

      if (loglevel == LOG_PAGE)
      {
       /*
        * Page message; send the message to the page_log file...
	*/

	LogPage(job, message);
      }
      else
      {
       /*
        * Other status message; send it to the error_log file...
	*/

	if (loglevel != LOG_INFO)
	  LogMessage(loglevel, "%s", message);

	if ((loglevel <= LOG_INFO && !job->state) ||
	    loglevel == LOG_ERROR)
          strncpy(job->printer->state_message, message,
                  sizeof(job->printer->state_message) - 1);
      }

     /*
      * Update the input buffer...
      */

      strcpy(buffer, lineptr);
      bufused -= lineptr - buffer;
    }
  }
  else
  {
    DEBUG_printf(("UpdateJob: job %d is complete.\n", job->id));

    if (job->status)
    {
     /*
      * Job had errors; stop it...
      */

      StopJob(job->id);
    }
    else
    {
     /*
      * Job printed successfully; cancel it...
      */

      job->printer->state_message[0] = '\0';

      CancelJob(job->id);
    }
  }
}


/*
 * 'start_process()' - Start a background process.
 */

static int			/* O - Process ID or 0 */
start_process(char *command,	/* I - Full path to command */
              char *argv[],	/* I - Command-line arguments */
	      char *envp[],	/* I - Environment */
              int  infd,	/* I - Standard input file descriptor */
	      int  outfd,	/* I - Standard output file descriptor */
	      int  errfd)	/* I - Standard error file descriptor */
{
  int	fd;			/* Looping var */
  int	pid;			/* Process ID */


  DEBUG_printf(("start_process(\"%s\", %08x, %08x, %d, %d, %d)\n",
                command, argv, envp, infd, outfd, errfd));

  if ((pid = fork()) == 0)
  {
   /*
    * Child process goes here...
    *
    * Update stdin/stdout/stderr as needed...
    */

    close(0);
    dup(infd);
    close(1);
    dup(outfd);
    if (errfd > 2)
    {
      close(2);
      dup(errfd);
    }

   /*
    * Close extra file descriptors...
    */

    for (fd = 3; fd < 1024; fd ++)
      close(fd);

   /*
    * Change user to something "safe"...
    */

    setuid(User);
    setgid(Group);

   /*
    * Execute the command; if for some reason this doesn't work,
    * return the error code...
    */

    execve(command, argv, envp);

    perror("cupsd: execve() failed");

    exit(errno);
  }
  else if (pid < 0)
  {
   /*
    * Error - couldn't fork a new process!
    */

    DEBUG_printf(("StartJob: unable to fork() %s - %s.\n", command,
	          strerror(errno)));

    return (0);
  }

  return (pid);
}


/*
 * End of "$Id: job.c,v 1.32 1999/07/13 13:06:21 mike Exp $".
 */
