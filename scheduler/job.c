/*
 * "$Id: job.c,v 1.5 1999/02/26 22:02:07 mike Exp $"
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   AddJob()     - Add a new job to the job queue...
 *   CancelJob()  - Cancel the specified print job.
 *   CancelJobs() - Cancel all jobs on the given printer or class.
 *   CheckJobs()  - Check the pending jobs and start any if the destination
 *                  is available.
 *   FindJob()    - Find the specified job.
 *   MoveJob()    - Move the specified job to a different destination.
 *   StartJob()   - Start a print job.
 *   StopJob()    - Stop a print job.
 *   UpdateJob()  - Read a status update from a job's filters.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


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

    if (current->state == IPP_JOB_PENDING)
    {
      DEBUG_printf(("CheckJobs: current->dest = \'%s\'\n", current->dest));

      if ((printer = FindPrinter(current->dest)) == NULL)
        printer = FindAvailablePrinter(current->dest);

      if (printer == NULL && FindClass(current->dest) == NULL)
      {
       /*
        * Whoa, the printer and/or class for this destination went away;
	* cancel the job...
	*/

        DEBUG_puts("CheckJobs: printer/class has gone away; cancelling the job!");
        CancelJob(current->id);

	if (prev == NULL)
	  current = Jobs;
	else
	  current = prev->next;
      }
      else
      {
       /*
        * See if the printer is available; if so, start the job...
	*/

        DEBUG_printf(("CheckJobs: printer->state = %d\n", printer->state));

        if (printer->state == IPP_PRINTER_IDLE)
	  StartJob(current->id, printer);

        current = current->next;
      }
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
  int		statusfds[2],	/* Pipes used between the filters and scheduler */
		filterfds[2][2];/* Pipes used between the filters */
  char		*argv[8];	/* Filter command-line arguments */
  char		command[1024],	/* Full path to filter/backend command */
		jobid[255],	/* Job ID string */
		title[IPP_MAX_NAME],
				/* Job title string */
		copies[255],	/* # copies string */
		options[16384],	/* Full list of options */
		*optptr;	/* Pointer to options */
  ipp_attribute_t *attr;	/* Current attribute */
  char		*envp[8];	/* Environment variables */
  char		language[255],	/* LANG environment variable */
		charset[255];	/* CHARSET environment variable */
  int		pid;		/* Process ID of new filter process */


  DEBUG_printf(("StartJob(%d, %08x)\n", id, printer));

  for (current = Jobs; current != NULL; current = current->next)
    if (current->id == id)
    {
     /*
      * Update the printer and job state to "processing"...
      */

      DEBUG_puts("StartJob: found job in list.");

      current->state   = IPP_JOB_PROCESSING;
      current->printer = printer;
      printer->job     = current;
      printer->state   = IPP_PRINTER_PROCESSING;

     /*
      * Figure out what filters are required to convert from
      * the source to the destination type...
      */

      num_filters = 0;
      filters     = mimeFilter(MimeDatabase, current->filetype,
                               printer->filetype, &num_filters);

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
	         attr->value_tag == IPP_TAG_NAME)
	  strcpy(title, attr->values[0].string);
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
	          if (strchr(attr->values[i].string, ' ') != NULL ||
		      strchr(attr->values[i].string, '\t') != NULL ||
		      strchr(attr->values[i].string, '\n') != NULL)
		  {
		    strcat(optptr, "\'");
		    strcat(optptr, attr->values[i].string);
		    strcat(optptr, "\'");
		  }
		  else
		    strcat(optptr, attr->values[i].string);
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

      argv[0] = (char *)printer->name;
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

      attr = ippFindAttribute(current->attrs, "attributes-natural-language");
      sprintf(language, "LANG=%s", attr->values[0].string);

      attr = ippFindAttribute(current->attrs, "document-format");
      if ((optptr = strstr(attr->values[0].string, "charset=")) != NULL)
        sprintf(charset, "CHARSET=%s", optptr + 8);
      else
      {
        attr = ippFindAttribute(current->attrs, "attributes-charset");
        sprintf(charset, "CHARSET=%s", attr->values[0].string);
      }

      envp[0] = "PATH=/bin:/usr/bin";
      envp[1] = "SOFTWARE=CUPS/1.0";
      envp[2] = "TZ=GMT";
      envp[3] = "USER=root";
      envp[4] = charset;
      envp[5] = language;
      envp[6] = "TZ=GMT";
      envp[7] = NULL;

     /*
      * Now create processes for all of the filters...
      */

      if (pipe(statusfds))
      {
        LogMessage(LOG_ERROR, "StartJob: unable to create status pipes - %s.",
	           strerror(errno));
        return;
      }

      current->pipe                   = statusfds[0];
      current->procs[num_filters + 1] = 0;

      for (i = 0; i < num_filters; i ++)
      {
        if (i == 1)
	  argv[6] = NULL;

        if (filters[i].filter[0] != '/')
	  sprintf(command, "%s/filters/%s", ServerRoot, filters[i].filter);
	else
	  strcpy(command, filters[i].filter);

        if (i < (num_filters - 1) ||
	    strncmp(printer->device_uri, "file:", 5) != 0)
          pipe(filterfds[i & 1]);
	else
	{
	  filterfds[i & 1][0] = -1;
	  filterfds[i & 1][1] = open(printer->device_uri + 5, O_WRONLY);
	}

        if ((pid = fork()) == 0)
	{
	 /*
	  * Child process goes here...
	  */

         /*
	  * Update stdin/stdout/stderr as needed...
	  */

          close(0);
	  if (i)
	  {
	    dup(filterfds[!(i & 1)][0]);
            close(filterfds[!(i & 1)][0]);
            close(filterfds[!(i & 1)][1]);
	  }
	  else
	    open("/dev/null", O_RDONLY);

	  close(1);
	  dup(filterfds[i & 1][1]);
          close(filterfds[i & 1][0]);
          close(filterfds[i & 1][1]);

	  close(2);
	  dup(statusfds[1]);

         /*
	  * Change user to something "safe"...
	  */

	  setuid(User);
	  setgid(Group);

         /*
	  * Execute the command; if for some reason this doesn't work,
	  * return error code 1.
	  */

	  execve(command, argv, envp);

          DEBUG_printf(("StartJob: unable to execve() %s - %s.\n", command,
	                strerror(errno)));

	  exit(-1);
	}
	else if (pid < 0)
	{
	 /*
	  * ERROR
	  */

          DEBUG_printf(("StartJob: unable to fork() %s - %s.\n", command,
	                strerror(errno)));

          current->procs[i] = 0;
	  CancelJob(current->id);
	  return;
	}
	else
	{
	 /*
	  * Parent process goes here...
	  */

	  current->procs[i] = pid;

          DEBUG_printf(("StartJob: started %s - pid = %d.\n", command, pid));

          if (i)
	  {
            close(filterfds[!(i & 1)][0]);
            close(filterfds[!(i & 1)][1]);
	  }
	}
      }

     /*
      * Finally, pipe the final output into a backend process if needed...
      */

      if (strncmp(printer->device_uri, "file:", 5) != 0)
      {
      }

      close(filterfds[!(i & 1)][0]);
      close(filterfds[!(i & 1)][1]);
      close(statusfds[1]);

      FD_SET(current->pipe, &InputSet);
    }
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

	current->state          = IPP_JOB_PENDING;
	current->printer->state = IPP_PRINTER_IDLE;
        current->printer->job   = NULL;
        current->printer        = NULL;

        for (i = 0; current->procs[i]; i ++)
	  if (current->procs[i] > 0)
	    kill(current->procs[i], SIGTERM);

        close(current->pipe);
	FD_CLR(current->pipe, &InputSet);
	current->pipe     = 0;
	current->procs[0] = 0;
      }
      return;
    }
}


/*
 * 'UpdateJob()' - Read a status update from a job's filters.
 */

void
UpdateJob(job_t *job)	/* I - Job to check */
{
  int	bytes;		/* Number of bytes read */
  char	buffer[8192];	/* Data buffer */


  if ((bytes = read(job->pipe, buffer, sizeof(buffer))) > 0)
    fwrite(buffer, bytes, 1, stderr);
  else
  {
    DEBUG_printf(("UpdateJob: job %d is complete.\n", job->id));
    CancelJob(job->id);
  }
}


/*
 * End of "$Id: job.c,v 1.5 1999/02/26 22:02:07 mike Exp $".
 */
