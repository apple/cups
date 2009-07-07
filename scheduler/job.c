/*
 * "$Id: job.c 7902 2008-09-03 14:20:17Z mike $"
 *
 *   Job management routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2009 by Apple Inc.
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
 *   cupsdAddJob()              - Add a new job to the job queue.
 *   cupsdCancelJobs()          - Cancel all jobs for the given
 *                                destination/user.
 *   cupsdCheckJobs()           - Check the pending jobs and start any if the
 *                                destination is available.
 *   cupsdCleanJobs()           - Clean out old jobs.
 *   cupsdContinueJob()         - Continue printing with the next file in a job.
 *   cupsdDeleteJob()           - Free all memory used by a job.
 *   cupsdFreeAllJobs()         - Free all jobs from memory.
 *   cupsdFindJob()             - Find the specified job.
 *   cupsdGetPrinterJobCount()  - Get the number of pending, processing, or held
 *                                jobs in a printer or class.
 *   cupsdGetUserJobCount()     - Get the number of pending, processing, or held
 *                                jobs for a user.
 *   cupsdLoadAllJobs()         - Load all jobs from disk.
 *   cupsdLoadJob()             - Load a single job.
 *   cupsdMoveJob()             - Move the specified job to a different
 *                                destination.
 *   cupsdReleaseJob()          - Release the specified job.
 *   cupsdRestartJob()          - Restart the specified job.
 *   cupsdSaveAllJobs()         - Save a summary of all jobs to disk.
 *   cupsdSaveJob()             - Save a job to disk.
 *   cupsdSetJobHoldUntil()     - Set the hold time for a job.
 *   cupsdSetJobPriority()      - Set the priority of a job, moving it up/down
 *                                in the list as needed.
 *   cupsdSetJobState()         - Set the state of the specified print job.
 *   cupsdStopAllJobs()         - Stop all print jobs.
 *   cupsdUnloadCompletedJobs() - Flush completed job history from memory.
 *   compare_active_jobs()      - Compare the job IDs and priorities of two
 *                                jobs.
 *   compare_jobs()             - Compare the job IDs of two jobs.
 *   dump_job_history()         - Dump any debug messages for a job.
 *   free_job_history()         - Free any log history.
 *   finalize_job()             - Cleanup after job filter processes and support
 *                                data.
 *   get_options()              - Get a string containing the job options.
 *   ipp_length()               - Compute the size of the buffer needed to hold
 *                                the textual IPP attributes.
 *   load_job_cache()           - Load jobs from the job.cache file.
 *   load_next_job_id()         - Load the NextJobId value from the job.cache
 *                                file.
 *   load_request_root()        - Load jobs from the RequestRoot directory.
 *   set_hold_until()           - Set the hold time and update job-hold-until
 *                                attribute.
 *   set_time()                 - Set one of the "time-at-xyz" attributes.
 *   start_job()                - Start a print job.
 *   stop_job()                 - Stop a print job.
 *   unload_job()               - Unload a job from memory.
 *   update_job()               - Read a status update from a job's filters.
 *   update_job_attrs()         - Update the job-printer-* attributes.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <grp.h>
#include <cups/backend.h>
#include <cups/dir.h>


/*
 * Design Notes for Job Management
 * -------------------------------
 *
 * STATE CHANGES
 *
 *     pending       Do nothing/check jobs
 *     pending-held  Send SIGTERM to filters and backend
 *     processing    Do nothing/start job
 *     stopped       Send SIGKILL to filters and backend
 *     canceled      Send SIGTERM to filters and backend
 *     aborted       Finalize
 *     completed     Finalize
 *
 *     Finalize clears the printer <-> job association, deletes the status
 *     buffer, closes all of the pipes, etc. and doesn't get run until all of
 *     the print processes are finished.
 *
 * UNLOADING OF JOBS (cupsdUnloadCompletedJobs)
 *
 *     We unload the job attributes when they are not needed to reduce overall
 *     memory consumption.  We don't unload jobs where job->state_value <
 *     IPP_JOB_STOPPED, job->printer != NULL, or job->access_time is recent.
 *
 * STARTING OF JOBS (start_job)
 *
 *     When a job is started, a status buffer, several pipes, a security
 *     profile, and a backend process are created for the life of that job.
 *     These are shared for every file in a job.  For remote print jobs, the
 *     IPP backend is provided with every file in the job and no filters are
 *     run.
 *
 *     The job->printer member tracks which printer is printing a job, which
 *     can be different than the destination in job->dest for classes.  The
 *     printer object also has a job pointer to track which job is being
 *     printed.
 *
 * PRINTING OF JOB FILES (cupsdContinueJob)
 *
 *     Each file in a job is filtered by 0 or more programs.  After getting the
 *     list of filters needed and the total cost, the job is either passed or
 *     put back to the processing state until the current FilterLevel comes down
 *     enough to allow printing.
 *
 *     If we can print, we build a string for the print options and run each of
 *     the filters, piping the output from one into the next.
 *
 * JOB STATUS UPDATES (update_job)
 *
 *     The update_job function gets called whenever there are pending messages
 *     on the status pipe.  These generally are updates to the marker-*,
 *     printer-state-message, or printer-state-reasons attributes.  On EOF,
 *     finalize_job is called to clean up.
 *
 * FINALIZING JOBS (finalize_job)
 *
 *     When all filters and the backend are done, we set the job state to
 *     completed (no errors), aborted (filter errors or abort-job policy),
 *     pending-held (auth required or retry-job policy), or pending
 *     (retry-current-job or stop-printer policies) as appropriate.
 *
 *     Then we close the pipes and free the status buffers and profiles.
 *
 * JOB FILE COMPLETION (process_children in main.c)
 *
 *     For multiple-file jobs, process_children (in main.c) sees that all
 *     filters have exited and calls in to print the next file if there are
 *     more files in the job, otherwise it waits for the backend to exit and
 *     update_job to do the cleanup.
 */


/*
 * Local globals...
 */

static mime_filter_t	gziptoany_filter =
			{
			  NULL,		/* Source type */
			  NULL,		/* Destination type */
			  0,		/* Cost */
			  "gziptoany"	/* Filter program to run */
			};


/*
 * Local functions...
 */

static int	compare_active_jobs(void *first, void *second, void *data);
static int	compare_jobs(void *first, void *second, void *data);
static void	dump_job_history(cupsd_job_t *job);
static void	finalize_job(cupsd_job_t *job);
static void	free_job_history(cupsd_job_t *job);
static char	*get_options(cupsd_job_t *job, int banner_page, char *copies,
		             size_t copies_size, char *title,
			     size_t title_size);
static int	ipp_length(ipp_t *ipp);
static void	load_job_cache(const char *filename);
static void	load_next_job_id(const char *filename);
static void	load_request_root(void);
static void	set_hold_until(cupsd_job_t *job, time_t holdtime);
static void	set_time(cupsd_job_t *job, const char *name);
static void	start_job(cupsd_job_t *job, cupsd_printer_t *printer);
static void	stop_job(cupsd_job_t *job, cupsd_jobaction_t action);
static void	unload_job(cupsd_job_t *job);
static void	update_job(cupsd_job_t *job);
static void	update_job_attrs(cupsd_job_t *job, int do_message);


/*
 * 'cupsdAddJob()' - Add a new job to the job queue.
 */

cupsd_job_t *				/* O - New job record */
cupsdAddJob(int        priority,	/* I - Job priority */
            const char *dest)		/* I - Job destination */
{
  cupsd_job_t	*job;			/* New job record */


  if ((job = calloc(sizeof(cupsd_job_t), 1)) == NULL)
    return (NULL);

  job->id              = NextJobId ++;
  job->priority        = priority;
  job->back_pipes[0]   = -1;
  job->back_pipes[1]   = -1;
  job->print_pipes[0]  = -1;
  job->print_pipes[1]  = -1;
  job->side_pipes[0]   = -1;
  job->side_pipes[1]   = -1;
  job->status_pipes[0] = -1;
  job->status_pipes[1] = -1;

  cupsdSetString(&job->dest, dest);

 /*
  * Add the new job to the "all jobs" and "active jobs" lists...
  */

  cupsArrayAdd(Jobs, job);
  cupsArrayAdd(ActiveJobs, job);

  return (job);
}


/*
 * 'cupsdCancelJobs()' - Cancel all jobs for the given destination/user.
 */

void
cupsdCancelJobs(const char *dest,	/* I - Destination to cancel */
                const char *username,	/* I - Username or NULL */
	        int        purge)	/* I - Purge jobs? */
{
  cupsd_job_t	*job;			/* Current job */


  for (job = (cupsd_job_t *)cupsArrayFirst(Jobs);
       job;
       job = (cupsd_job_t *)cupsArrayNext(Jobs))
  {
    if ((!job->dest || !job->username) && !cupsdLoadJob(job))
      continue;

    if ((!dest || !strcmp(job->dest, dest)) &&
        (!username || !strcmp(job->username, username)))
    {
     /*
      * Cancel all jobs matching this destination/user...
      */

      if (purge)
	cupsdSetJobState(job, IPP_JOB_CANCELED, CUPSD_JOB_PURGE,
	                 "Job purged by user.");
      else
	cupsdSetJobState(job, IPP_JOB_CANCELED, CUPSD_JOB_DEFAULT,
			 "Job canceled by user.");
    }
  }

  cupsdCheckJobs();
}


/*
 * 'cupsdCheckJobs()' - Check the pending jobs and start any if the destination
 *                      is available.
 */

void
cupsdCheckJobs(void)
{
  cupsd_job_t		*job;		/* Current job in queue */
  cupsd_printer_t	*printer,	/* Printer destination */
			*pclass;	/* Printer class destination */
  ipp_attribute_t	*attr;		/* Job attribute */
  time_t		curtime;	/* Current time */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdCheckJobs: %d active jobs, sleeping=%d, reload=%d",
                  cupsArrayCount(ActiveJobs), Sleeping, NeedReload);

  curtime = time(NULL);

  for (job = (cupsd_job_t *)cupsArrayFirst(ActiveJobs);
       job;
       job = (cupsd_job_t *)cupsArrayNext(ActiveJobs))
  {
   /*
    * Kill jobs if they are unresponsive...
    */

    if (job->kill_time && job->kill_time <= curtime)
    {
      stop_job(job, CUPSD_JOB_FORCE);
      continue;
    }

   /*
    * Start held jobs if they are ready...
    */

    if (job->state_value == IPP_JOB_HELD &&
        job->hold_until &&
	job->hold_until < curtime)
    {
      if (job->pending_timeout)
      {
       /*
        * This job is pending; check that we don't have an active Send-Document
	* operation in progress on any of the client connections, then timeout
	* the job so we can start printing...
	*/

        cupsd_client_t	*con;		/* Current client connection */


	for (con = (cupsd_client_t *)cupsArrayFirst(Clients);
	     con;
	     con = (cupsd_client_t *)cupsArrayNext(Clients))
	  if (con->request &&
	      con->request->request.op.operation_id == IPP_SEND_DOCUMENT)
	    break;

        if (con)
	  continue;

        if (cupsdTimeoutJob(job))
	  continue;
      }

      cupsdSetJobState(job, IPP_JOB_PENDING, CUPSD_JOB_DEFAULT,
                       "Job submission timed out.");
    }

   /*
    * Continue jobs that are waiting on the FilterLimit...
    */

    if (job->pending_cost > 0 &&
	((FilterLevel + job->pending_cost) < FilterLimit || FilterLevel == 0))
      cupsdContinueJob(job);

   /*
    * Start pending jobs if the destination is available...
    */

    if (job->state_value == IPP_JOB_PENDING && !NeedReload && !Sleeping &&
        !job->printer)
    {
      printer = cupsdFindDest(job->dest);
      pclass  = NULL;

      while (printer &&
             (printer->type & (CUPS_PRINTER_IMPLICIT | CUPS_PRINTER_CLASS)))
      {
       /*
        * If the class is remote, just pass it to the remote server...
	*/

        pclass = printer;

        if (pclass->state == IPP_PRINTER_STOPPED)
	  printer = NULL;
        else if (pclass->type & CUPS_PRINTER_REMOTE)
	  break;
	else
	  printer = cupsdFindAvailablePrinter(printer->name);
      }

      if (!printer && !pclass)
      {
       /*
        * Whoa, the printer and/or class for this destination went away;
	* cancel the job...
	*/

        cupsdSetJobState(job, IPP_JOB_ABORTED, CUPSD_JOB_PURGE,
	                 "Job aborted because the destination printer/class "
			 "has gone away.");
      }
      else if (printer && !printer->holding_new_jobs)
      {
       /*
        * See if the printer is available or remote and not printing a job;
	* if so, start the job...
	*/

        if (pclass)
	{
	 /*
	  * Add/update a job-actual-printer-uri attribute for this job
	  * so that we know which printer actually printed the job...
	  */

          if ((attr = ippFindAttribute(job->attrs, "job-actual-printer-uri",
	                               IPP_TAG_URI)) != NULL)
            cupsdSetString(&attr->values[0].string.text, printer->uri);
	  else
	    ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI,
	                 "job-actual-printer-uri", NULL, printer->uri);

          job->dirty = 1;
          cupsdMarkDirty(CUPSD_DIRTY_JOBS);
	}

        if ((!(printer->type & CUPS_PRINTER_DISCOVERED) && /* Printer is local */
	     printer->state == IPP_PRINTER_IDLE) ||	/* and idle, OR */
	    ((printer->type & CUPS_PRINTER_DISCOVERED) && /* Printer is remote */
	     !printer->job))				/* and not printing */
        {
	 /*
	  * Start the job...
	  */

	  start_job(job, printer);
	}
      }
    }
  }
}


/*
 * 'cupsdCleanJobs()' - Clean out old jobs.
 */

void
cupsdCleanJobs(void)
{
  cupsd_job_t	*job;			/* Current job */


  if (MaxJobs <= 0)
    return;

  for (job = (cupsd_job_t *)cupsArrayFirst(Jobs);
       job && cupsArrayCount(Jobs) >= MaxJobs;
       job = (cupsd_job_t *)cupsArrayNext(Jobs))
    if (job->state_value >= IPP_JOB_CANCELED && !job->printer)
      cupsdDeleteJob(job, CUPSD_JOB_PURGE);
}


/*
 * 'cupsdContinueJob()' - Continue printing with the next file in a job.
 */

void
cupsdContinueJob(cupsd_job_t *job)	/* I - Job */
{
  int			i;		/* Looping var */
  int			slot;		/* Pipe slot */
  cups_array_t		*filters = NULL,/* Filters for job */
			*prefilters;	/* Filters with prefilters */
  mime_filter_t		*filter,	/* Current filter */
			*prefilter,	/* Prefilter */
			port_monitor;	/* Port monitor filter */
  char			scheme[255];	/* Device URI scheme */
  ipp_attribute_t	*attr;		/* Current attribute */
  const char		*ptr,		/* Pointer into value */
			*abort_message;	/* Abort message */
  ipp_jstate_t		abort_state = IPP_JOB_STOPPED;
					/* New job state on abort */
  struct stat		backinfo;	/* Backend file information */
  int			backroot;	/* Run backend as root? */
  int			pid;		/* Process ID of new filter process */
  int			banner_page;	/* 1 if banner page, 0 otherwise */
  int			filterfds[2][2] = { { -1, -1 }, { -1, -1 } };
					/* Pipes used between filters */
  int			envc;		/* Number of environment variables */
  char			**argv = NULL,	/* Filter command-line arguments */
			filename[1024],	/* Job filename */
			command[1024],	/* Full path to command */
			jobid[255],	/* Job ID string */
			title[IPP_MAX_NAME],
					/* Job title string */
			copies[255],	/* # copies string */
			*options,	/* Options string */
			*envp[MAX_ENV + 19],
					/* Environment variables */
			charset[255],	/* CHARSET env variable */
			class_name[255],/* CLASS env variable */
			classification[1024],
					/* CLASSIFICATION env variable */
			content_type[1024],
					/* CONTENT_TYPE env variable */
			device_uri[1024],
					/* DEVICE_URI env variable */
			final_content_type[1024],
					/* FINAL_CONTENT_TYPE env variable */
			lang[255],	/* LANG env variable */
#ifdef __APPLE__
			apple_language[255],
					/* APPLE_LANGUAGE env variable */
#endif /* __APPLE__ */
			ppd[1024],	/* PPD env variable */
			printer_info[255],
					/* PRINTER_INFO env variable */
			printer_location[255],
					/* PRINTER_LOCATION env variable */
			printer_name[255],
					/* PRINTER env variable */
			rip_max_cache[255];
					/* RIP_MAX_CACHE env variable */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdContinueJob(job=%p(%d)): current_file=%d, num_files=%d",
	          job, job->id, job->current_file, job->num_files);

 /*
  * Figure out what filters are required to convert from
  * the source to the destination type...
  */

  FilterLevel -= job->cost;

  job->cost         = 0;
  job->pending_cost = 0;

  memset(job->filters, 0, sizeof(job->filters));


  if (job->printer->raw)
  {
   /*
    * Remote jobs and raw queues go directly to the printer without
    * filtering...
    */

    cupsdLogJob(job, CUPSD_LOG_DEBUG, "Sending job to queue tagged as raw...");
  }
  else
  {
   /*
    * Local jobs get filtered...
    */

    filters = mimeFilter(MimeDatabase, job->filetypes[job->current_file],
                         job->printer->filetype, &(job->cost));

    if (!filters)
    {
      cupsdLogJob(job, CUPSD_LOG_ERROR,
		  "Unable to convert file %d to printable format!",
		  job->current_file);

      abort_message = "Aborting job because it cannot be printed.";
      abort_state   = IPP_JOB_ABORTED;

      goto abort_job;
    }

   /*
    * Remove NULL ("-") filters...
    */

    for (filter = (mime_filter_t *)cupsArrayFirst(filters);
         filter;
	 filter = (mime_filter_t *)cupsArrayNext(filters))
      if (!strcmp(filter->filter, "-"))
        cupsArrayRemove(filters, filter);

    if (cupsArrayCount(filters) == 0)
    {
      cupsArrayDelete(filters);
      filters = NULL;
    }

   /*
    * If this printer has any pre-filters, insert the required pre-filter
    * in the filters array...
    */

    if (job->printer->prefiltertype && filters)
    {
      prefilters = cupsArrayNew(NULL, NULL);

      for (filter = (mime_filter_t *)cupsArrayFirst(filters);
	   filter;
	   filter = (mime_filter_t *)cupsArrayNext(filters))
      {
	if ((prefilter = mimeFilterLookup(MimeDatabase, filter->src,
					  job->printer->prefiltertype)))
	{
	  cupsArrayAdd(prefilters, prefilter);
	  job->cost += prefilter->cost;
	}

	cupsArrayAdd(prefilters, filter);
      }

      cupsArrayDelete(filters);
      filters = prefilters;
    }
  }

 /*
  * Set a minimum cost of 100 for all jobs so that FilterLimit
  * works with raw queues and other low-cost paths.
  */

  if (job->cost < 100)
    job->cost = 100;

 /*
  * See if the filter cost is too high...
  */

  if ((FilterLevel + job->cost) > FilterLimit && FilterLevel > 0 &&
      FilterLimit > 0)
  {
   /*
    * Don't print this job quite yet...
    */

    cupsArrayDelete(filters);

    cupsdLogJob(job, CUPSD_LOG_INFO,
		"Holding because filter limit has been reached.");
    cupsdLogJob(job, CUPSD_LOG_DEBUG2,
		"cupsdContinueJob: file=%d, cost=%d, level=%d, limit=%d",
		job->current_file, job->cost, FilterLevel,
		FilterLimit);

    job->pending_cost = job->cost;
    job->cost         = 0;
    return;
  }

  FilterLevel += job->cost;

 /*
  * Add decompression/raw filter as needed...
  */

  if ((!job->printer->raw && job->compressions[job->current_file]) ||
      (!filters && !job->printer->remote &&
       (job->num_files > 1 || !strncmp(job->printer->device_uri, "file:", 5))))
  {
   /*
    * Add gziptoany filter to the front of the list...
    */

    if (!filters)
      filters = cupsArrayNew(NULL, NULL);

    if (!cupsArrayInsert(filters, &gziptoany_filter))
    {
      cupsdLogJob(job, CUPSD_LOG_DEBUG,
		  "Unable to add decompression filter - %s", strerror(errno));

      cupsArrayDelete(filters);

      abort_message = "Stopping job because the scheduler ran out of memory.";

      goto abort_job;
    }
  }

 /*
  * Add port monitor, if any...
  */

  if (job->printer->port_monitor)
  {
   /*
    * Add port monitor to the end of the list...
    */

    if (!filters)
      filters = cupsArrayNew(NULL, NULL);

    port_monitor.src  = NULL;
    port_monitor.dst  = NULL;
    port_monitor.cost = 0;

    snprintf(port_monitor.filter, sizeof(port_monitor.filter),
             "%s/monitor/%s", ServerBin, job->printer->port_monitor);

    if (!cupsArrayAdd(filters, &port_monitor))
    {
      cupsdLogJob(job, CUPSD_LOG_DEBUG,
		  "Unable to add port monitor - %s", strerror(errno));

      abort_message = "Stopping job because the scheduler ran out of memory.";

      goto abort_job;
    }
  }

 /*
  * Make sure we don't go over the "MAX_FILTERS" limit...
  */

  if (cupsArrayCount(filters) > MAX_FILTERS)
  {
    cupsdLogJob(job, CUPSD_LOG_DEBUG,
		"Too many filters (%d > %d), unable to print!",
		cupsArrayCount(filters), MAX_FILTERS);

    abort_message = "Aborting job because it needs too many filters to print.";
    abort_state   = IPP_JOB_ABORTED;

    goto abort_job;
  }

 /*
  * Determine if we are printing a banner page or not...
  */

  if (job->job_sheets == NULL)
  {
    cupsdLogJob(job, CUPSD_LOG_DEBUG, "No job-sheets attribute.");
    if ((job->job_sheets =
         ippFindAttribute(job->attrs, "job-sheets", IPP_TAG_ZERO)) != NULL)
      cupsdLogJob(job, CUPSD_LOG_DEBUG,
		  "... but someone added one without setting job_sheets!");
  }
  else if (job->job_sheets->num_values == 1)
    cupsdLogJob(job, CUPSD_LOG_DEBUG, "job-sheets=%s",
		job->job_sheets->values[0].string.text);
  else
    cupsdLogJob(job, CUPSD_LOG_DEBUG, "job-sheets=%s,%s",
                job->job_sheets->values[0].string.text,
                job->job_sheets->values[1].string.text);

  if (job->printer->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT))
    banner_page = 0;
  else if (job->job_sheets == NULL)
    banner_page = 0;
  else if (strcasecmp(job->job_sheets->values[0].string.text, "none") != 0 &&
	   job->current_file == 0)
    banner_page = 1;
  else if (job->job_sheets->num_values > 1 &&
	   strcasecmp(job->job_sheets->values[1].string.text, "none") != 0 &&
	   job->current_file == (job->num_files - 1))
    banner_page = 1;
  else
    banner_page = 0;

  if ((options = get_options(job, banner_page, copies, sizeof(copies), title,
                             sizeof(title))) == NULL)
  {
    abort_message = "Stopping job because the scheduler ran out of memory.";

    goto abort_job;
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
  *
  * For remote jobs, we send all of the files in the argument list.
  */

  if (job->printer->remote)
    argv = calloc(7 + job->num_files, sizeof(char *));
  else
    argv = calloc(8, sizeof(char *));

  if (!argv)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG, "Unable to allocate argument array - %s",
                    strerror(errno));

    abort_message = "Stopping job because the scheduler ran out of memory.";

    goto abort_job;
  }

  sprintf(jobid, "%d", job->id);

  argv[0] = job->printer->name;
  argv[1] = jobid;
  argv[2] = job->username;
  argv[3] = title;
  argv[4] = copies;
  argv[5] = options;

  if (job->printer->remote && job->num_files > 1)
  {
    for (i = 0; i < job->num_files; i ++)
    {
      snprintf(filename, sizeof(filename), "%s/d%05d-%03d", RequestRoot,
               job->id, i + 1);
      argv[6 + i] = strdup(filename);
    }
  }
  else
  {
    snprintf(filename, sizeof(filename), "%s/d%05d-%03d", RequestRoot,
             job->id, job->current_file + 1);
    argv[6] = filename;
  }

  for (i = 0; argv[i]; i ++)
    cupsdLogJob(job, CUPSD_LOG_DEBUG, "argv[%d]=\"%s\"", i, argv[i]);

 /*
  * Create environment variable strings for the filters...
  */

  attr = ippFindAttribute(job->attrs, "attributes-natural-language",
                          IPP_TAG_LANGUAGE);

#ifdef __APPLE__
  strcpy(apple_language, "APPLE_LANGUAGE=");
  _cupsAppleLanguage(attr->values[0].string.text,
		     apple_language + 15, sizeof(apple_language) - 15);
#endif /* __APPLE__ */

  switch (strlen(attr->values[0].string.text))
  {
    default :
       /*
        * This is an unknown or badly formatted language code; use
	* the POSIX locale...
	*/

	strcpy(lang, "LANG=C");
	break;

    case 2 :
       /*
        * Just the language code (ll)...
	*/

        snprintf(lang, sizeof(lang), "LANG=%s.UTF-8",
	         attr->values[0].string.text);
        break;

    case 5 :
       /*
        * Language and country code (ll-cc)...
	*/

        snprintf(lang, sizeof(lang), "LANG=%c%c_%c%c.UTF-8",
	         attr->values[0].string.text[0],
		 attr->values[0].string.text[1],
		 toupper(attr->values[0].string.text[3] & 255),
		 toupper(attr->values[0].string.text[4] & 255));
        break;
  }

  if ((attr = ippFindAttribute(job->attrs, "document-format",
                               IPP_TAG_MIMETYPE)) != NULL &&
      (ptr = strstr(attr->values[0].string.text, "charset=")) != NULL)
    snprintf(charset, sizeof(charset), "CHARSET=%s", ptr + 8);
  else
    strlcpy(charset, "CHARSET=utf-8", sizeof(charset));

  snprintf(content_type, sizeof(content_type), "CONTENT_TYPE=%s/%s",
           job->filetypes[job->current_file]->super,
           job->filetypes[job->current_file]->type);
  snprintf(device_uri, sizeof(device_uri), "DEVICE_URI=%s",
           job->printer->device_uri);
  snprintf(ppd, sizeof(ppd), "PPD=%s/ppd/%s.ppd", ServerRoot,
	   job->printer->name);
  snprintf(printer_info, sizeof(printer_name), "PRINTER_INFO=%s",
           job->printer->info ? job->printer->info : "");
  snprintf(printer_location, sizeof(printer_name), "PRINTER_LOCATION=%s",
           job->printer->location ? job->printer->location : "");
  snprintf(printer_name, sizeof(printer_name), "PRINTER=%s", job->printer->name);
  snprintf(rip_max_cache, sizeof(rip_max_cache), "RIP_MAX_CACHE=%s", RIPCache);

  envc = cupsdLoadEnv(envp, (int)(sizeof(envp) / sizeof(envp[0])));

  envp[envc ++] = charset;
  envp[envc ++] = lang;
#ifdef __APPLE__
  envp[envc ++] = apple_language;
#endif /* __APPLE__ */
  envp[envc ++] = ppd;
  envp[envc ++] = rip_max_cache;
  envp[envc ++] = content_type;
  envp[envc ++] = device_uri;
  envp[envc ++] = printer_info;
  envp[envc ++] = printer_location;
  envp[envc ++] = printer_name;
  envp[envc ++] = banner_page ? "CUPS_FILETYPE=job-sheet" :
                                "CUPS_FILETYPE=document";

  if (!job->printer->remote && !job->printer->raw)
  {
    filter = (mime_filter_t *)cupsArrayLast(filters);

    if (job->printer->port_monitor)
      filter = (mime_filter_t *)cupsArrayPrev(filters);

    if (filter && filter->dst)
    {
      snprintf(final_content_type, sizeof(final_content_type),
	       "FINAL_CONTENT_TYPE=%s/%s",
	       filter->dst->super, filter->dst->type);
      envp[envc ++] = final_content_type;
    }
  }

  if (Classification && !banner_page)
  {
    if ((attr = ippFindAttribute(job->attrs, "job-sheets",
                                 IPP_TAG_NAME)) == NULL)
      snprintf(classification, sizeof(classification), "CLASSIFICATION=%s",
               Classification);
    else if (attr->num_values > 1 &&
             strcmp(attr->values[1].string.text, "none") != 0)
      snprintf(classification, sizeof(classification), "CLASSIFICATION=%s",
               attr->values[1].string.text);
    else
      snprintf(classification, sizeof(classification), "CLASSIFICATION=%s",
               attr->values[0].string.text);

    envp[envc ++] = classification;
  }

  if (job->dtype & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT))
  {
    snprintf(class_name, sizeof(class_name), "CLASS=%s", job->dest);
    envp[envc ++] = class_name;
  }

  if (job->auth_username)
    envp[envc ++] = job->auth_username;
  if (job->auth_domain)
    envp[envc ++] = job->auth_domain;
  if (job->auth_password)
    envp[envc ++] = job->auth_password;

#ifdef HAVE_GSSAPI
  if (job->ccname)
    envp[envc ++] = job->ccname;
#endif /* HAVE_GSSAPI */

  envp[envc] = NULL;

  for (i = 0; i < envc; i ++)
    if (!strncmp(envp[i], "AUTH_", 5))
      cupsdLogJob(job, CUPSD_LOG_DEBUG, "envp[%d]=\"AUTH_%c****\"", i,
                  envp[i][5]);
    else if (strncmp(envp[i], "DEVICE_URI=", 11))
      cupsdLogJob(job, CUPSD_LOG_DEBUG, "envp[%d]=\"%s\"", i, envp[i]);
    else
      cupsdLogJob(job, CUPSD_LOG_DEBUG, "envp[%d]=\"DEVICE_URI=%s\"", i,
                  job->printer->sanitized_device_uri);

  if (job->printer->remote)
    job->current_file = job->num_files;
  else
    job->current_file ++;

 /*
  * Now create processes for all of the filters...
  */

  for (i = 0, slot = 0, filter = (mime_filter_t *)cupsArrayFirst(filters);
       filter;
       i ++, filter = (mime_filter_t *)cupsArrayNext(filters))
  {
    if (filter->filter[0] != '/')
      snprintf(command, sizeof(command), "%s/filter/%s", ServerBin,
               filter->filter);
    else
      strlcpy(command, filter->filter, sizeof(command));

    if (i < (cupsArrayCount(filters) - 1))
    {
      if (cupsdOpenPipe(filterfds[slot]))
      {
        abort_message = "Stopping job because the scheduler could not create "
	                "the filter pipes.";

        goto abort_job;
      }
    }
    else
    {
      if (job->current_file == 1)
      {
	if (strncmp(job->printer->device_uri, "file:", 5) != 0)
	{
	  if (cupsdOpenPipe(job->print_pipes))
	  {
	    abort_message = "Stopping job because the scheduler could not "
	                    "create the backend pipes.";

            goto abort_job;
	  }
	}
	else
	{
	  job->print_pipes[0] = -1;
	  if (!strcmp(job->printer->device_uri, "file:/dev/null") ||
	      !strcmp(job->printer->device_uri, "file:///dev/null"))
	    job->print_pipes[1] = -1;
	  else
	  {
	    if (!strncmp(job->printer->device_uri, "file:/dev/", 10))
	      job->print_pipes[1] = open(job->printer->device_uri + 5,
	                        	 O_WRONLY | O_EXCL);
	    else if (!strncmp(job->printer->device_uri, "file:///dev/", 12))
	      job->print_pipes[1] = open(job->printer->device_uri + 7,
	                        	 O_WRONLY | O_EXCL);
	    else if (!strncmp(job->printer->device_uri, "file:///", 8))
	      job->print_pipes[1] = open(job->printer->device_uri + 7,
	                        	 O_WRONLY | O_CREAT | O_TRUNC, 0600);
	    else
	      job->print_pipes[1] = open(job->printer->device_uri + 5,
	                        	 O_WRONLY | O_CREAT | O_TRUNC, 0600);

	    if (job->print_pipes[1] < 0)
	    {
	      abort_message = "Stopping job because the scheduler could not "
	                      "open the output file.";

              goto abort_job;
	    }

	    fcntl(job->print_pipes[1], F_SETFD,
        	  fcntl(job->print_pipes[1], F_GETFD) | FD_CLOEXEC);
          }
	}
      }

      filterfds[slot][0] = job->print_pipes[0];
      filterfds[slot][1] = job->print_pipes[1];
    }

    pid = cupsdStartProcess(command, argv, envp, filterfds[!slot][0],
                            filterfds[slot][1], job->status_pipes[1],
		            job->back_pipes[0], job->side_pipes[0], 0,
			    job->profile, job, job->filters + i);

    cupsdClosePipe(filterfds[!slot]);

    if (pid == 0)
    {
      cupsdLogJob(job, CUPSD_LOG_ERROR, "Unable to start filter \"%s\" - %s.",
		  filter->filter, strerror(errno));

      abort_message = "Stopping job because the scheduler could not execute a "
		      "filter.";

      goto abort_job;
    }

    cupsdLogJob(job, CUPSD_LOG_INFO, "Started filter %s (PID %d)", command,
                pid);

    argv[6] = NULL;
    slot    = !slot;
  }

  cupsArrayDelete(filters);
  filters = NULL;

 /*
  * Finally, pipe the final output into a backend process if needed...
  */

  if (strncmp(job->printer->device_uri, "file:", 5) != 0)
  {
    if (job->current_file == 1 || job->printer->remote)
    {
      sscanf(job->printer->device_uri, "%254[^:]", scheme);
      snprintf(command, sizeof(command), "%s/backend/%s", ServerBin, scheme);

     /*
      * See if the backend needs to run as root...
      */

      if (RunUser)
        backroot = 0;
      else if (stat(command, &backinfo))
	backroot = 0;
      else
        backroot = !(backinfo.st_mode & (S_IRWXG | S_IRWXO));

      argv[0] = job->printer->sanitized_device_uri;

      filterfds[slot][0] = -1;
      filterfds[slot][1] = -1;

      pid = cupsdStartProcess(command, argv, envp, filterfds[!slot][0],
			      filterfds[slot][1], job->status_pipes[1],
			      job->back_pipes[1], job->side_pipes[1],
			      backroot, job->profile, job, &(job->backend));

      if (pid == 0)
      {
	abort_message = "Stopping job because the sheduler could not execute "
			"the backend.";

        goto abort_job;
      }
      else
      {
	cupsdLogJob(job, CUPSD_LOG_INFO, "Started backend %s (PID %d)",
		    command, pid);
      }
    }

    if (job->current_file == job->num_files)
    {
      cupsdClosePipe(job->print_pipes);
      cupsdClosePipe(job->back_pipes);
      cupsdClosePipe(job->side_pipes);

      close(job->status_pipes[1]);
      job->status_pipes[1] = -1;
    }
  }
  else
  {
    filterfds[slot][0] = -1;
    filterfds[slot][1] = -1;

    if (job->current_file == job->num_files)
    {
      cupsdClosePipe(job->print_pipes);

      close(job->status_pipes[1]);
      job->status_pipes[1] = -1;
    }
  }

  cupsdClosePipe(filterfds[slot]);

  if (job->printer->remote && job->num_files > 1)
  {
    for (i = 0; i < job->num_files; i ++)
      free(argv[i + 6]);
  }

  free(argv);

  cupsdAddSelect(job->status_buffer->fd, (cupsd_selfunc_t)update_job, NULL,
                 job);

  cupsdAddEvent(CUPSD_EVENT_JOB_STATE, job->printer, job, "Job #%d started.",
                job->id);

 /*
  * If we get here than we are able to run the printer driver filters, so clear
  * the missing and insecure warnings...
  */

  if (cupsdSetPrinterReasons(job->printer, "-cups-missing-filter-warning,"
			                   "cups-insecure-filter-warning"))
    cupsdAddEvent(CUPSD_EVENT_PRINTER_STATE, job->printer, NULL,
                  "Printer drivers now functional.");

  return;


 /*
  * If we get here, we need to abort the current job and close out all
  * files and pipes...
  */

  abort_job:

  FilterLevel -= job->cost;
  job->cost = 0;

  for (slot = 0; slot < 2; slot ++)
    cupsdClosePipe(filterfds[slot]);

  cupsArrayDelete(filters);

  if (argv)
  {
    if (job->printer->remote && job->num_files > 1)
    {
      for (i = 0; i < job->num_files; i ++)
	free(argv[i + 6]);
    }

    free(argv);
  }

  cupsdClosePipe(job->print_pipes);
  cupsdClosePipe(job->back_pipes);
  cupsdClosePipe(job->side_pipes);

  cupsdRemoveSelect(job->status_pipes[0]);
  cupsdClosePipe(job->status_pipes);
  cupsdStatBufDelete(job->status_buffer);
  job->status_buffer = NULL;

 /*
  * Update the printer and job state.
  */

  cupsdSetJobState(job, abort_state, CUPSD_JOB_DEFAULT, "%s", abort_message);
  cupsdSetPrinterState(job->printer, IPP_PRINTER_IDLE, 0);
  update_job_attrs(job, 0);

  if (job->history)
    free_job_history(job);

  cupsArrayRemove(PrintingJobs, job);

 /*
  * Clear the printer <-> job association...
  */

  job->printer->job = NULL;
  job->printer      = NULL;
}


/*
 * 'cupsdDeleteJob()' - Free all memory used by a job.
 */

void
cupsdDeleteJob(cupsd_job_t       *job,	/* I - Job */
               cupsd_jobaction_t action)/* I - Action */
{
  if (job->printer)
    finalize_job(job);

  if (action == CUPSD_JOB_PURGE)
  {
   /*
    * Remove the job info file...
    */

    char	filename[1024];		/* Job filename */

    snprintf(filename, sizeof(filename), "%s/c%05d", RequestRoot,
	     job->id);
    unlink(filename);
  }

  cupsdClearString(&job->username);
  cupsdClearString(&job->dest);
  cupsdClearString(&job->auth_username);
  cupsdClearString(&job->auth_domain);
  cupsdClearString(&job->auth_password);

#ifdef HAVE_GSSAPI
 /*
  * Destroy the credential cache and clear the KRB5CCNAME env var string.
  */

  if (job->ccache)
  {
    krb5_cc_destroy(KerberosContext, job->ccache);
    job->ccache = NULL;
  }

  cupsdClearString(&job->ccname);
#endif /* HAVE_GSSAPI */

  if (job->num_files > 0)
  {
    free(job->compressions);
    free(job->filetypes);

    job->num_files = 0;
  }

  if (job->history)
    free_job_history(job);

  unload_job(job);

  cupsArrayRemove(Jobs, job);
  cupsArrayRemove(ActiveJobs, job);
  cupsArrayRemove(PrintingJobs, job);

  free(job);
}


/*
 * 'cupsdFreeAllJobs()' - Free all jobs from memory.
 */

void
cupsdFreeAllJobs(void)
{
  cupsd_job_t	*job;			/* Current job */


  if (!Jobs)
    return;

  cupsdHoldSignals();

  cupsdStopAllJobs(CUPSD_JOB_FORCE, 0);
  cupsdSaveAllJobs();

  for (job = (cupsd_job_t *)cupsArrayFirst(Jobs);
       job;
       job = (cupsd_job_t *)cupsArrayNext(Jobs))
    cupsdDeleteJob(job, CUPSD_JOB_DEFAULT);

  cupsdReleaseSignals();
}


/*
 * 'cupsdFindJob()' - Find the specified job.
 */

cupsd_job_t *				/* O - Job data */
cupsdFindJob(int id)			/* I - Job ID */
{
  cupsd_job_t	key;			/* Search key */


  key.id = id;

  return ((cupsd_job_t *)cupsArrayFind(Jobs, &key));
}


/*
 * 'cupsdGetPrinterJobCount()' - Get the number of pending, processing,
 *                               or held jobs in a printer or class.
 */

int					/* O - Job count */
cupsdGetPrinterJobCount(
    const char *dest)			/* I - Printer or class name */
{
  int		count;			/* Job count */
  cupsd_job_t	*job;			/* Current job */


  for (job = (cupsd_job_t *)cupsArrayFirst(ActiveJobs), count = 0;
       job;
       job = (cupsd_job_t *)cupsArrayNext(ActiveJobs))
    if (job->dest && !strcasecmp(job->dest, dest))
      count ++;

  return (count);
}


/*
 * 'cupsdGetUserJobCount()' - Get the number of pending, processing,
 *                            or held jobs for a user.
 */

int					/* O - Job count */
cupsdGetUserJobCount(
    const char *username)		/* I - Username */
{
  int		count;			/* Job count */
  cupsd_job_t	*job;			/* Current job */


  for (job = (cupsd_job_t *)cupsArrayFirst(ActiveJobs), count = 0;
       job;
       job = (cupsd_job_t *)cupsArrayNext(ActiveJobs))
    if (!strcasecmp(job->username, username))
      count ++;

  return (count);
}


/*
 * 'cupsdLoadAllJobs()' - Load all jobs from disk.
 */

void
cupsdLoadAllJobs(void)
{
  char		filename[1024];		/* Full filename of job.cache file */
  struct stat	fileinfo,		/* Information on job.cache file */
		dirinfo;		/* Information on RequestRoot dir */



 /*
  * Create the job arrays as needed...
  */

  if (!Jobs)
    Jobs = cupsArrayNew(compare_jobs, NULL);

  if (!ActiveJobs)
    ActiveJobs = cupsArrayNew(compare_active_jobs, NULL);

  if (!PrintingJobs)
    PrintingJobs = cupsArrayNew(compare_jobs, NULL);

 /*
  * See whether the job.cache file is older than the RequestRoot directory...
  */

  snprintf(filename, sizeof(filename), "%s/job.cache", CacheDir);

  if (stat(filename, &fileinfo))
  {
    fileinfo.st_mtime = 0;

    if (errno != ENOENT)
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to get file information for \"%s\" - %s",
		      filename, strerror(errno));
  }

  if (stat(RequestRoot, &dirinfo))
  {
    dirinfo.st_mtime = 0;

    if (errno != ENOENT)
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to get directory information for \"%s\" - %s",
		      RequestRoot, strerror(errno));
  }

 /*
  * Load the most recent source for job data...
  */

  if (dirinfo.st_mtime > fileinfo.st_mtime)
  {
    load_request_root();

    load_next_job_id(filename);
  }
  else
    load_job_cache(filename);

 /*
  * Clean out old jobs as needed...
  */

  if (MaxJobs > 0 && cupsArrayCount(Jobs) >= MaxJobs)
    cupsdCleanJobs();
}


/*
 * 'cupsdLoadJob()' - Load a single job.
 */

int					/* O - 1 on success, 0 on failure */
cupsdLoadJob(cupsd_job_t *job)		/* I - Job */
{
  char			jobfile[1024];	/* Job filename */
  cups_file_t		*fp;		/* Job file */
  int			fileid;		/* Current file ID */
  ipp_attribute_t	*attr;		/* Job attribute */
  const char		*dest;		/* Destination name */
  cupsd_printer_t	*destptr;	/* Pointer to destination */
  mime_type_t		**filetypes;	/* New filetypes array */
  int			*compressions;	/* New compressions array */


  if (job->attrs)
  {
    if (job->state_value > IPP_JOB_STOPPED)
      job->access_time = time(NULL);

    return (1);
  }

  if ((job->attrs = ippNew()) == NULL)
  {
    cupsdLogJob(job, CUPSD_LOG_ERROR, "Ran out of memory for job attributes!");
    return (0);
  }

 /*
  * Load job attributes...
  */

  cupsdLogMessage(CUPSD_LOG_DEBUG, "[Job %d] Loading attributes...", job->id);

  snprintf(jobfile, sizeof(jobfile), "%s/c%05d", RequestRoot, job->id);
  if ((fp = cupsFileOpen(jobfile, "r")) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
		    "[Job %d] Unable to open job control file \"%s\" - %s!",
		    job->id, jobfile, strerror(errno));
    goto error;
  }

  if (ippReadIO(fp, (ipp_iocb_t)cupsFileRead, 1, NULL, job->attrs) != IPP_DATA)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
		    "[Job %d] Unable to read job control file \"%s\"!", job->id,
		    jobfile);
    cupsFileClose(fp);
    goto error;
  }

  cupsFileClose(fp);

 /*
  * Copy attribute data to the job object...
  */

  if (!ippFindAttribute(job->attrs, "time-at-creation", IPP_TAG_INTEGER))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
		    "[Job %d] Missing or bad time-at-creation attribute in "
		    "control file!", job->id);
    goto error;
  }

  if ((job->state = ippFindAttribute(job->attrs, "job-state",
                                     IPP_TAG_ENUM)) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
		    "[Job %d] Missing or bad job-state attribute in control "
		    "file!", job->id);
    goto error;
  }

  job->state_value = (ipp_jstate_t)job->state->values[0].integer;

  if (!job->dest)
  {
    if ((attr = ippFindAttribute(job->attrs, "job-printer-uri",
                                 IPP_TAG_URI)) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
		      "[Job %d] No job-printer-uri attribute in control file!",
		      job->id);
      goto error;
    }

    if ((dest = cupsdValidateDest(attr->values[0].string.text, &(job->dtype),
                                  &destptr)) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
		      "[Job %d] Unable to queue job for destination \"%s\"!",
		      job->id, attr->values[0].string.text);
      goto error;
    }

    cupsdSetString(&job->dest, dest);
  }
  else if ((destptr = cupsdFindDest(job->dest)) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
		    "[Job %d] Unable to queue job for destination \"%s\"!",
		    job->id, job->dest);
    goto error;
  }

  job->sheets     = ippFindAttribute(job->attrs, "job-media-sheets-completed",
                                     IPP_TAG_INTEGER);
  job->job_sheets = ippFindAttribute(job->attrs, "job-sheets", IPP_TAG_NAME);

  if (!job->priority)
  {
    if ((attr = ippFindAttribute(job->attrs, "job-priority",
                        	 IPP_TAG_INTEGER)) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
		      "[Job %d] Missing or bad job-priority attribute in "
		      "control file!", job->id);
      goto error;
    }

    job->priority = attr->values[0].integer;
  }

  if (!job->username)
  {
    if ((attr = ippFindAttribute(job->attrs, "job-originating-user-name",
                        	 IPP_TAG_NAME)) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
		      "[Job %d] Missing or bad job-originating-user-name "
		      "attribute in control file!", job->id);
      goto error;
    }

    cupsdSetString(&job->username, attr->values[0].string.text);
  }

 /*
  * Set the job hold-until time and state...
  */

  if (job->state_value == IPP_JOB_HELD)
  {
    if ((attr = ippFindAttribute(job->attrs, "job-hold-until",
	                         IPP_TAG_KEYWORD)) == NULL)
      attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

    if (attr)
      cupsdSetJobHoldUntil(job, attr->values[0].string.text, CUPSD_JOB_DEFAULT);
    else
    {
      job->state->values[0].integer = IPP_JOB_PENDING;
      job->state_value              = IPP_JOB_PENDING;
    }
  }
  else if (job->state_value == IPP_JOB_PROCESSING)
  {
    job->state->values[0].integer = IPP_JOB_PENDING;
    job->state_value              = IPP_JOB_PENDING;
  }

  if (!job->num_files)
  {
   /*
    * Find all the d##### files...
    */

    for (fileid = 1; fileid < 10000; fileid ++)
    {
      snprintf(jobfile, sizeof(jobfile), "%s/d%05d-%03d", RequestRoot,
               job->id, fileid);

      if (access(jobfile, 0))
        break;

      cupsdLogMessage(CUPSD_LOG_DEBUG,
		      "[Job %d] Auto-typing document file \"%s\"...", job->id,
		      jobfile);

      if (fileid > job->num_files)
      {
        if (job->num_files == 0)
	{
	  compressions = (int *)calloc(fileid, sizeof(int));
	  filetypes    = (mime_type_t **)calloc(fileid, sizeof(mime_type_t *));
	}
	else
	{
	  compressions = (int *)realloc(job->compressions,
	                                sizeof(int) * fileid);
	  filetypes    = (mime_type_t **)realloc(job->filetypes,
	                                         sizeof(mime_type_t *) *
						 fileid);
        }

        if (!compressions || !filetypes)
	{
          cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "[Job %d] Ran out of memory for job file types!",
			  job->id);
	  return (1);
	}

        job->compressions = compressions;
        job->filetypes    = filetypes;
	job->num_files    = fileid;
      }

      job->filetypes[fileid - 1] = mimeFileType(MimeDatabase, jobfile, NULL,
                                                job->compressions + fileid - 1);

      if (!job->filetypes[fileid - 1])
        job->filetypes[fileid - 1] = mimeType(MimeDatabase, "application",
	                                      "vnd.cups-raw");
    }
  }

 /*
  * Load authentication information as needed...
  */

  if (job->state_value < IPP_JOB_STOPPED)
  {
    snprintf(jobfile, sizeof(jobfile), "%s/a%05d", RequestRoot, job->id);

    cupsdClearString(&job->auth_username);
    cupsdClearString(&job->auth_domain);
    cupsdClearString(&job->auth_password);

    if ((fp = cupsFileOpen(jobfile, "r")) != NULL)
    {
      int	i,			/* Looping var */
		bytes;			/* Size of auth data */
      char	line[255],		/* Line from file */
		data[255];		/* Decoded data */


      for (i = 0;
           i < destptr->num_auth_info_required &&
	       cupsFileGets(fp, line, sizeof(line));
	   i ++)
      {
        bytes = sizeof(data);
        httpDecode64_2(data, &bytes, line);

	if (!strcmp(destptr->auth_info_required[i], "username"))
	  cupsdSetStringf(&job->auth_username, "AUTH_USERNAME=%s", data);
	else if (!strcmp(destptr->auth_info_required[i], "domain"))
	  cupsdSetStringf(&job->auth_domain, "AUTH_DOMAIN=%s", data);
	else if (!strcmp(destptr->auth_info_required[i], "password"))
	  cupsdSetStringf(&job->auth_password, "AUTH_PASSWORD=%s", data);
      }

      cupsFileClose(fp);
    }
  }

  job->access_time = time(NULL);
  return (1);

 /*
  * If we get here then something bad happened...
  */

  error:

  ippDelete(job->attrs);
  job->attrs = NULL;
  unlink(jobfile);

  return (0);
}


/*
 * 'cupsdMoveJob()' - Move the specified job to a different destination.
 */

void
cupsdMoveJob(cupsd_job_t     *job,	/* I - Job */
             cupsd_printer_t *p)	/* I - Destination printer or class */
{
  ipp_attribute_t	*attr;		/* job-printer-uri attribute */
  const char		*olddest;	/* Old destination */
  cupsd_printer_t	*oldp;		/* Old pointer */


 /*
  * Don't move completed jobs...
  */

  if (job->state_value > IPP_JOB_STOPPED)
    return;

 /*
  * Get the old destination...
  */

  olddest = job->dest;

  if (job->printer)
    oldp = job->printer;
  else
    oldp = cupsdFindDest(olddest);

 /*
  * Change the destination information...
  */

  cupsdSetJobState(job, IPP_JOB_PENDING, CUPSD_JOB_DEFAULT,
                   "Stopping job prior to move.");

  cupsdAddEvent(CUPSD_EVENT_JOB_CONFIG_CHANGED, oldp, job,
                "Job #%d moved from %s to %s.", job->id, olddest,
		p->name);

  cupsdSetString(&job->dest, p->name);
  job->dtype = p->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_REMOTE |
                          CUPS_PRINTER_IMPLICIT);

  if ((attr = ippFindAttribute(job->attrs, "job-printer-uri",
                               IPP_TAG_URI)) != NULL)
    cupsdSetString(&(attr->values[0].string.text), p->uri);

  cupsdAddEvent(CUPSD_EVENT_JOB_STOPPED, p, job,
                "Job #%d moved from %s to %s.", job->id, olddest,
		p->name);

  job->dirty = 1;
  cupsdMarkDirty(CUPSD_DIRTY_JOBS);
}


/*
 * 'cupsdReleaseJob()' - Release the specified job.
 */

void
cupsdReleaseJob(cupsd_job_t *job)	/* I - Job */
{
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdReleaseJob(job=%p(%d))", job,
                  job->id);

  if (job->state_value == IPP_JOB_HELD)
  {
   /*
    * Add trailing banner as needed...
    */

    if (job->pending_timeout)
      cupsdTimeoutJob(job);

    cupsdSetJobState(job, IPP_JOB_PENDING, CUPSD_JOB_DEFAULT,
                     "Job released by user.");
  }
}


/*
 * 'cupsdRestartJob()' - Restart the specified job.
 */

void
cupsdRestartJob(cupsd_job_t *job)	/* I - Job */
{
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdRestartJob(job=%p(%d))", job,
                  job->id);

  if (job->state_value == IPP_JOB_STOPPED || job->num_files)
    cupsdSetJobState(job, IPP_JOB_PENDING, CUPSD_JOB_DEFAULT,
                     "Job restarted by user.");
}


/*
 * 'cupsdSaveAllJobs()' - Save a summary of all jobs to disk.
 */

void
cupsdSaveAllJobs(void)
{
  int		i;			/* Looping var */
  cups_file_t	*fp;			/* Job cache file */
  char		temp[1024];		/* Temporary string */
  cupsd_job_t	*job;			/* Current job */
  time_t	curtime;		/* Current time */
  struct tm	*curdate;		/* Current date */


  snprintf(temp, sizeof(temp), "%s/job.cache", CacheDir);
  if ((fp = cupsFileOpen(temp, "w")) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to create job cache file \"%s\" - %s",
                    temp, strerror(errno));
    return;
  }

  cupsdLogMessage(CUPSD_LOG_INFO, "Saving job cache file \"%s\"...", temp);

 /*
  * Restrict access to the file...
  */

  fchown(cupsFileNumber(fp), getuid(), Group);
  fchmod(cupsFileNumber(fp), ConfigFilePerm);

 /*
  * Write a small header to the file...
  */

  curtime = time(NULL);
  curdate = localtime(&curtime);
  strftime(temp, sizeof(temp) - 1, "%Y-%m-%d %H:%M", curdate);

  cupsFilePuts(fp, "# Job cache file for " CUPS_SVERSION "\n");
  cupsFilePrintf(fp, "# Written by cupsd on %s\n", temp);
  cupsFilePrintf(fp, "NextJobId %d\n", NextJobId);

 /*
  * Write each job known to the system...
  */

  for (job = (cupsd_job_t *)cupsArrayFirst(Jobs);
       job;
       job = (cupsd_job_t *)cupsArrayNext(Jobs))
  {
    cupsFilePrintf(fp, "<Job %d>\n", job->id);
    cupsFilePrintf(fp, "State %d\n", job->state_value);
    cupsFilePrintf(fp, "Priority %d\n", job->priority);
    cupsFilePrintf(fp, "HoldUntil %d\n", (int)job->hold_until);
    cupsFilePrintf(fp, "Username %s\n", job->username);
    cupsFilePrintf(fp, "Destination %s\n", job->dest);
    cupsFilePrintf(fp, "DestType %d\n", job->dtype);
    cupsFilePrintf(fp, "NumFiles %d\n", job->num_files);
    for (i = 0; i < job->num_files; i ++)
      cupsFilePrintf(fp, "File %d %s/%s %d\n", i + 1, job->filetypes[i]->super,
                     job->filetypes[i]->type, job->compressions[i]);
    cupsFilePuts(fp, "</Job>\n");
  }

  cupsFileClose(fp);
}


/*
 * 'cupsdSaveJob()' - Save a job to disk.
 */

void
cupsdSaveJob(cupsd_job_t *job)		/* I - Job */
{
  char		filename[1024];		/* Job control filename */
  cups_file_t	*fp;			/* Job file */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdSaveJob(job=%p(%d)): job->attrs=%p",
                  job, job->id, job->attrs);

  snprintf(filename, sizeof(filename), "%s/c%05d", RequestRoot, job->id);

  if ((fp = cupsFileOpen(filename, "w")) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
		    "[Job %d] Unable to create job control file \"%s\" - %s.",
		    job->id, filename, strerror(errno));
    return;
  }

  fchmod(cupsFileNumber(fp), 0600);
  fchown(cupsFileNumber(fp), RunUser, Group);

  job->attrs->state = IPP_IDLE;

  if (ippWriteIO(fp, (ipp_iocb_t)cupsFileWrite, 1, NULL,
                 job->attrs) != IPP_DATA)
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "[Job %d] Unable to write job control file!", job->id);

  cupsFileClose(fp);

  job->dirty = 0;
}


/*
 * 'cupsdSetJobHoldUntil()' - Set the hold time for a job.
 */

void
cupsdSetJobHoldUntil(cupsd_job_t *job,	/* I - Job */
                     const char  *when,	/* I - When to resume */
		     int         update)/* I - Update job-hold-until attr? */
{
  time_t	curtime;		/* Current time */
  struct tm	*curdate;		/* Current date */
  int		hour;			/* Hold hour */
  int		minute;			/* Hold minute */
  int		second = 0;		/* Hold second */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdSetJobHoldUntil(job=%p(%d), when=\"%s\", update=%d)",
                  job, job->id, when, update);

  if (update)
  {
   /*
    * Update the job-hold-until attribute...
    */

    ipp_attribute_t *attr;		/* job-hold-until attribute */

    if ((attr = ippFindAttribute(job->attrs, "job-hold-until",
				 IPP_TAG_KEYWORD)) == NULL)
      attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

    if (attr)
      cupsdSetString(&(attr->values[0].string.text), when);
    else
      attr = ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_KEYWORD,
                          "job-hold-until", NULL, when);

    if (attr)
    {
      if (isdigit(when[0] & 255))
	attr->value_tag = IPP_TAG_NAME;
      else
	attr->value_tag = IPP_TAG_KEYWORD;

      job->dirty = 1;
      cupsdMarkDirty(CUPSD_DIRTY_JOBS);
    }
  }

 /*
  * Update the hold time...
  */

  if (!strcmp(when, "indefinite") || !strcmp(when, "auth-info-required"))
  {
   /*
    * Hold indefinitely...
    */

    job->hold_until = 0;
  }
  else if (!strcmp(when, "day-time"))
  {
   /*
    * Hold to 6am the next morning unless local time is < 6pm.
    */

    curtime = time(NULL);
    curdate = localtime(&curtime);

    if (curdate->tm_hour < 18)
      job->hold_until = curtime;
    else
      job->hold_until = curtime +
                        ((29 - curdate->tm_hour) * 60 + 59 -
			 curdate->tm_min) * 60 + 60 - curdate->tm_sec;
  }
  else if (!strcmp(when, "evening") || !strcmp(when, "night"))
  {
   /*
    * Hold to 6pm unless local time is > 6pm or < 6am.
    */

    curtime = time(NULL);
    curdate = localtime(&curtime);

    if (curdate->tm_hour < 6 || curdate->tm_hour >= 18)
      job->hold_until = curtime;
    else
      job->hold_until = curtime +
                        ((17 - curdate->tm_hour) * 60 + 59 -
			 curdate->tm_min) * 60 + 60 - curdate->tm_sec;
  }
  else if (!strcmp(when, "second-shift"))
  {
   /*
    * Hold to 4pm unless local time is > 4pm.
    */

    curtime = time(NULL);
    curdate = localtime(&curtime);

    if (curdate->tm_hour >= 16)
      job->hold_until = curtime;
    else
      job->hold_until = curtime +
                        ((15 - curdate->tm_hour) * 60 + 59 -
			 curdate->tm_min) * 60 + 60 - curdate->tm_sec;
  }
  else if (!strcmp(when, "third-shift"))
  {
   /*
    * Hold to 12am unless local time is < 8am.
    */

    curtime = time(NULL);
    curdate = localtime(&curtime);

    if (curdate->tm_hour < 8)
      job->hold_until = curtime;
    else
      job->hold_until = curtime +
                        ((23 - curdate->tm_hour) * 60 + 59 -
			 curdate->tm_min) * 60 + 60 - curdate->tm_sec;
  }
  else if (!strcmp(when, "weekend"))
  {
   /*
    * Hold to weekend unless we are in the weekend.
    */

    curtime = time(NULL);
    curdate = localtime(&curtime);

    if (curdate->tm_wday == 0 || curdate->tm_wday == 6)
      job->hold_until = curtime;
    else
      job->hold_until = curtime +
                        (((5 - curdate->tm_wday) * 24 +
                          (17 - curdate->tm_hour)) * 60 + 59 -
			   curdate->tm_min) * 60 + 60 - curdate->tm_sec;
  }
  else if (sscanf(when, "%d:%d:%d", &hour, &minute, &second) >= 2)
  {
   /*
    * Hold to specified GMT time (HH:MM or HH:MM:SS)...
    */

    curtime = time(NULL);
    curdate = gmtime(&curtime);

    job->hold_until = curtime +
                      ((hour - curdate->tm_hour) * 60 + minute -
		       curdate->tm_min) * 60 + second - curdate->tm_sec;

   /*
    * Hold until next day as needed...
    */

    if (job->hold_until < curtime)
      job->hold_until += 24 * 60 * 60;
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdSetJobHoldUntil: hold_until=%d",
                  (int)job->hold_until);
}


/*
 * 'cupsdSetJobPriority()' - Set the priority of a job, moving it up/down in
 *                           the list as needed.
 */

void
cupsdSetJobPriority(
    cupsd_job_t *job,			/* I - Job ID */
    int         priority)		/* I - New priority (0 to 100) */
{
  ipp_attribute_t	*attr;		/* Job attribute */


 /*
  * Don't change completed jobs...
  */

  if (job->state_value >= IPP_JOB_PROCESSING)
    return;

 /*
  * Set the new priority and re-add the job into the active list...
  */

  cupsArrayRemove(ActiveJobs, job);

  job->priority = priority;

  if ((attr = ippFindAttribute(job->attrs, "job-priority",
                               IPP_TAG_INTEGER)) != NULL)
    attr->values[0].integer = priority;
  else
    ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-priority",
                  priority);

  cupsArrayAdd(ActiveJobs, job);

  job->dirty = 1;
  cupsdMarkDirty(CUPSD_DIRTY_JOBS);
}


/*
 * 'cupsdSetJobState()' - Set the state of the specified print job.
 */

void
cupsdSetJobState(
    cupsd_job_t       *job,		/* I - Job to cancel */
    ipp_jstate_t      newstate,		/* I - New job state */
    cupsd_jobaction_t action,		/* I - Action to take */
    const char        *message,		/* I - Message to log */
    ...)				/* I - Additional arguments as needed */
{
  int			i;		/* Looping var */
  ipp_jstate_t		oldstate;	/* Old state */
  char			filename[1024];	/* Job filename */
  ipp_attribute_t	*attr;		/* Job attribute */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdSetJobState(job=%p(%d), state=%d, newstate=%d, "
		  "action=%d, message=\"%s\")", job, job->id, job->state_value,
		  newstate, action, message ? message : "(null)");


 /*
  * Make sure we have the job attributes...
  */

  if (!cupsdLoadJob(job))
    return;

 /*
  * Don't do anything if the state is unchanged...
  */

  if (newstate == (oldstate = job->state_value))
    return;

 /*
  * Stop any processes that are working on the current job...
  */

  if (oldstate == IPP_JOB_PROCESSING)
    stop_job(job, action != CUPSD_JOB_DEFAULT);

 /*
  * Set the new job state...
  */

  job->state->values[0].integer = newstate;
  job->state_value              = newstate;

  switch (newstate)
  {
    case IPP_JOB_PENDING :
       /*
	* Update job-hold-until as needed...
	*/

	if ((attr = ippFindAttribute(job->attrs, "job-hold-until",
				     IPP_TAG_KEYWORD)) == NULL)
	  attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

	if (attr)
	{
	  attr->value_tag = IPP_TAG_KEYWORD;
	  cupsdSetString(&(attr->values[0].string.text), "no-hold");
	}

    default :
	break;

    case IPP_JOB_ABORTED :
    case IPP_JOB_CANCELED :
    case IPP_JOB_COMPLETED :
	set_time(job, "time-at-completed");
        break;
  }

 /*
  * Log message as needed...
  */

  if (message)
  {
    char	buffer[2048];		/* Message buffer */
    va_list	ap;			/* Pointer to additional arguments */

    va_start(ap, message);
    vsnprintf(buffer, sizeof(buffer), message, ap);
    va_end(ap);

    if (newstate > IPP_JOB_STOPPED)
      cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job, "%s", buffer);
    else
      cupsdAddEvent(CUPSD_EVENT_JOB_STATE, job->printer, job, "%s", buffer);

    if (newstate == IPP_JOB_STOPPED || newstate == IPP_JOB_ABORTED)
      cupsdLogJob(job, CUPSD_LOG_ERROR, "%s", buffer);
    else
      cupsdLogJob(job, CUPSD_LOG_INFO, "%s", buffer);
  }

 /*
  * Handle post-state-change actions...
  */

  switch (newstate)
  {
    case IPP_JOB_PROCESSING :
       /*
        * Add the job to the "printing" list...
	*/

        if (!cupsArrayFind(PrintingJobs, job))
	  cupsArrayAdd(PrintingJobs, job);

       /*
	* Set the processing time...
	*/

	set_time(job, "time-at-processing");

    case IPP_JOB_PENDING :
    case IPP_JOB_HELD :
    case IPP_JOB_STOPPED :
       /*
        * Make sure the job is in the active list...
	*/

        if (!cupsArrayFind(ActiveJobs, job))
	  cupsArrayAdd(ActiveJobs, job);

       /*
	* Save the job state to disk...
	*/

	job->dirty = 1;
	cupsdMarkDirty(CUPSD_DIRTY_JOBS);
        break;

    case IPP_JOB_ABORTED :
    case IPP_JOB_CANCELED :
    case IPP_JOB_COMPLETED :
       /*
        * Expire job subscriptions since the job is now "completed"...
	*/

        cupsdExpireSubscriptions(NULL, job);

       /*
	* Remove the job from the active list...
	*/

	cupsArrayRemove(ActiveJobs, job);

#ifdef __APPLE__
       /*
	* If we are going to sleep and the PrintingJobs count is now 0, allow the
	* sleep to happen immediately...
	*/

	if (Sleeping && cupsArrayCount(PrintingJobs) == 0)
	  cupsdAllowSleep();
#endif /* __APPLE__ */

       /*
	* Remove any authentication data...
	*/

	snprintf(filename, sizeof(filename), "%s/a%05d", RequestRoot, job->id);
	if (cupsdRemoveFile(filename) && errno != ENOENT)
	  cupsdLogMessage(CUPSD_LOG_ERROR,
			  "Unable to remove authentication cache: %s",
			  strerror(errno));

	cupsdClearString(&job->auth_username);
	cupsdClearString(&job->auth_domain);
	cupsdClearString(&job->auth_password);

#ifdef HAVE_GSSAPI
       /*
	* Destroy the credential cache and clear the KRB5CCNAME env var string.
	*/

	if (job->ccache)
	{
	  krb5_cc_destroy(KerberosContext, job->ccache);
	  job->ccache = NULL;
	}

	cupsdClearString(&job->ccname);
#endif /* HAVE_GSSAPI */

       /*
	* Remove the print file for good if we aren't preserving jobs or
	* files...
	*/

	if (!JobHistory || !JobFiles || action == CUPSD_JOB_PURGE)
	{
	  for (i = 1; i <= job->num_files; i ++)
	  {
	    snprintf(filename, sizeof(filename), "%s/d%05d-%03d", RequestRoot,
		     job->id, i);
	    unlink(filename);
	  }

	  if (job->num_files > 0)
	  {
	    free(job->filetypes);
	    free(job->compressions);

	    job->num_files    = 0;
	    job->filetypes    = NULL;
	    job->compressions = NULL;
	  }
	}

	if (JobHistory && action != CUPSD_JOB_PURGE)
	{
	 /*
	  * Save job state info...
	  */

	  job->dirty = 1;
	  cupsdMarkDirty(CUPSD_DIRTY_JOBS);
	}
	else if (!job->printer)
	  cupsdDeleteJob(job, CUPSD_JOB_PURGE);
	break;
  }

 /*
  * Finalize the job immediately if we forced things...
  */

  if (action == CUPSD_JOB_FORCE)
    finalize_job(job);

 /*
  * Update the server "busy" state...
  */

  cupsdSetBusyState();
}


/*
 * 'cupsdStopAllJobs()' - Stop all print jobs.
 */

void
cupsdStopAllJobs(
    cupsd_jobaction_t action,		/* I - Action */
    int               kill_delay)	/* I - Number of seconds before we kill */
{
  cupsd_job_t	*job;			/* Current job */


  DEBUG_puts("cupsdStopAllJobs()");

  for (job = (cupsd_job_t *)cupsArrayFirst(PrintingJobs);
       job;
       job = (cupsd_job_t *)cupsArrayNext(PrintingJobs))
  {
    if (kill_delay)
      job->kill_time = time(NULL) + kill_delay;

    cupsdSetJobState(job, IPP_JOB_PENDING, action, NULL);
  }
}


/*
 * 'cupsdUnloadCompletedJobs()' - Flush completed job history from memory.
 */

void
cupsdUnloadCompletedJobs(void)
{
  cupsd_job_t	*job;			/* Current job */
  time_t	expire;			/* Expiration time */


  expire = time(NULL) - 60;

  for (job = (cupsd_job_t *)cupsArrayFirst(Jobs);
       job;
       job = (cupsd_job_t *)cupsArrayNext(Jobs))
    if (job->attrs && job->state_value >= IPP_JOB_STOPPED && !job->printer &&
        job->access_time < expire)
    {
      if (job->dirty)
        cupsdSaveJob(job);

      unload_job(job);
    }
}


/*
 * 'compare_active_jobs()' - Compare the job IDs and priorities of two jobs.
 */

static int				/* O - Difference */
compare_active_jobs(void *first,	/* I - First job */
                    void *second,	/* I - Second job */
		    void *data)		/* I - App data (not used) */
{
  int	diff;				/* Difference */


  if ((diff = ((cupsd_job_t *)second)->priority -
              ((cupsd_job_t *)first)->priority) != 0)
    return (diff);
  else
    return (((cupsd_job_t *)first)->id - ((cupsd_job_t *)second)->id);
}


/*
 * 'compare_jobs()' - Compare the job IDs of two jobs.
 */

static int				/* O - Difference */
compare_jobs(void *first,		/* I - First job */
             void *second,		/* I - Second job */
	     void *data)		/* I - App data (not used) */
{
  return (((cupsd_job_t *)first)->id - ((cupsd_job_t *)second)->id);
}


/*
 * 'dump_job_history()' - Dump any debug messages for a job.
 */

static void
dump_job_history(cupsd_job_t *job)	/* I - Job */
{
  int			i,		/* Looping var */
			oldsize;	/* Current MaxLogSize */
  struct tm		*date;		/* Date/time value */
  cupsd_joblog_t	*message;	/* Current message */
  char			temp[2048],	/* Log message */
			*ptr,		/* Pointer into log message */
			start[256],	/* Start time */
			end[256];	/* End time */
  cupsd_printer_t	*printer;	/* Printer for job */


 /*
  * See if we have anything to dump...
  */

  if (!job->history)
    return;

 /*
  * Disable log rotation temporarily...
  */

  oldsize    = MaxLogSize;
  MaxLogSize = 0;

 /*
  * Copy the debug messages to the log...
  */

  message = (cupsd_joblog_t *)cupsArrayFirst(job->history);
  date = localtime(&(message->time));
  strftime(start, sizeof(start), "%X", date);

  message = (cupsd_joblog_t *)cupsArrayLast(job->history);
  date = localtime(&(message->time));
  strftime(end, sizeof(end), "%X", date);

  snprintf(temp, sizeof(temp),
           "[Job %d] The following messages were recorded from %s to %s",
           job->id, start, end);
  cupsdWriteErrorLog(CUPSD_LOG_DEBUG, temp);

  for (message = (cupsd_joblog_t *)cupsArrayFirst(job->history);
       message;
       message = (cupsd_joblog_t *)cupsArrayNext(job->history))
    cupsdWriteErrorLog(CUPSD_LOG_DEBUG, message->message);

  snprintf(temp, sizeof(temp), "[Job %d] End of messages", job->id);
  cupsdWriteErrorLog(CUPSD_LOG_DEBUG, temp);

 /*
  * Log the printer state values...
  */

  if ((printer = job->printer) == NULL)
    printer = cupsdFindDest(job->dest);

  if (printer)
  {
    snprintf(temp, sizeof(temp), "[Job %d] printer-state=%d(%s)", job->id,
             printer->state,
	     printer->state == IPP_PRINTER_IDLE ? "idle" :
	         printer->state == IPP_PRINTER_PROCESSING ? "processing" :
		 "stopped");
    cupsdWriteErrorLog(CUPSD_LOG_DEBUG, temp);

    snprintf(temp, sizeof(temp), "[Job %d] printer-state-message=\"%s\"",
             job->id, printer->state_message);
    cupsdWriteErrorLog(CUPSD_LOG_DEBUG, temp);

    snprintf(temp, sizeof(temp), "[Job %d] printer-state-reasons=", job->id);
    ptr = temp + strlen(temp);
    if (printer->num_reasons == 0)
      strlcpy(ptr, "none", sizeof(temp) - (ptr - temp));
    else
    {
      for (i = 0;
           i < printer->num_reasons && ptr < (temp + sizeof(temp) - 2);
           i ++)
      {
        if (i)
	  *ptr++ = ',';

	strlcpy(ptr, printer->reasons[i], sizeof(temp) - (ptr - temp));
	ptr += strlen(ptr);
      }
    }
    cupsdWriteErrorLog(CUPSD_LOG_DEBUG, temp);
  }

 /*
  * Restore log file rotation...
  */

  MaxLogSize = oldsize;

 /*
  * Free all messages...
  */

  free_job_history(job);
}


/*
 * 'free_job_history()' - Free any log history.
 */

static void
free_job_history(cupsd_job_t *job)	/* I - Job */
{
  char	*message;			/* Current message */


  if (!job->history)
    return;

  for (message = (char *)cupsArrayFirst(job->history);
       message;
       message = (char *)cupsArrayNext(job->history))
    free(message);

  cupsArrayDelete(job->history);
  job->history = NULL;
}


/*
 * 'finalize_job()' - Cleanup after job filter processes and support data.
 */

static void
finalize_job(cupsd_job_t *job)		/* I - Job */
{
  ipp_pstate_t		printer_state;	/* New printer state value */
  ipp_jstate_t		job_state;	/* New job state value */
  const char		*message;	/* Message for job state */
  char			buffer[1024];	/* Buffer for formatted messages */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "finalize_job(job=%p(%d))", job, job->id);

 /*
  * Clear the "connecting-to-device" and "com.apple.print.recoverable-warning"
  * reasons, which are only valid when a printer is processing...
  */

  cupsdSetPrinterReasons(job->printer, "-connecting-to-device");
  cupsdSetPrinterReasons(job->printer, "-com.apple.print.recoverable-warning");

 /*
  * Similarly, clear the "offline-report" reason for non-USB devices since we
  * rarely have current information for network devices...
  */

  if (strncmp(job->printer->device_uri, "usb:", 4))
    cupsdSetPrinterReasons(job->printer, "-offline-report");

 /*
  * Free the security profile...
  */

  cupsdDestroyProfile(job->profile);
  job->profile = NULL;

 /*
  * Close pipes and status buffer...
  */

  cupsdClosePipe(job->print_pipes);
  cupsdClosePipe(job->back_pipes);
  cupsdClosePipe(job->side_pipes);

  cupsdRemoveSelect(job->status_pipes[0]);
  cupsdClosePipe(job->status_pipes);
  cupsdStatBufDelete(job->status_buffer);
  job->status_buffer = NULL;

 /*
  * Process the exit status...
  */

  if (job->printer->state == IPP_PRINTER_PROCESSING)
    printer_state = IPP_PRINTER_IDLE;
  else
    printer_state = job->printer->state;

  switch (job_state = job->state_value)
  {
    case IPP_JOB_PENDING :
        message = "Job paused.";
	break;

    case IPP_JOB_HELD :
        message = "Job held.";
	break;

    default :
    case IPP_JOB_PROCESSING :
    case IPP_JOB_COMPLETED :
	job_state = IPP_JOB_COMPLETED;
	message   = "Job completed.";
        break;

    case IPP_JOB_STOPPED :
        message = "Job stopped.";
	break;

    case IPP_JOB_CANCELED :
        message = "Job canceled.";
	break;

    case IPP_JOB_ABORTED :
        message = "Job aborted.";
	break;
  }

  if (job->status < 0)
  {
   /*
    * Backend had errors...
    */

    int exit_code;			/* Exit code from backend */


   /*
    * Convert the status to an exit code.  Due to the way the W* macros are
    * implemented on MacOS X (bug?), we have to store the exit status in a
    * variable first and then convert...
    */

    exit_code = -job->status;
    if (WIFEXITED(exit_code))
      exit_code = WEXITSTATUS(exit_code);
    else
      exit_code = job->status;

    cupsdLogJob(job, CUPSD_LOG_INFO, "Backend returned status %d (%s)",
		exit_code,
		exit_code == CUPS_BACKEND_FAILED ? "failed" :
		    exit_code == CUPS_BACKEND_AUTH_REQUIRED ?
			"authentication required" :
		    exit_code == CUPS_BACKEND_HOLD ? "hold job" :
		    exit_code == CUPS_BACKEND_STOP ? "stop printer" :
		    exit_code == CUPS_BACKEND_CANCEL ? "cancel job" :
		    exit_code < 0 ? "crashed" : "unknown");

   /*
    * Do what needs to be done...
    */

    switch (exit_code)
    {
      default :
      case CUPS_BACKEND_FAILED :
         /*
	  * Backend failure, use the error-policy to determine how to
	  * act...
	  */

          if (job->dtype & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT))
	  {
	   /*
	    * Queued on a class - mark the job as pending and we'll retry on
	    * another printer...
	    */

            if (job_state == IPP_JOB_COMPLETED)
	    {
	      job_state = IPP_JOB_PENDING;
	      message   = "Retrying job on another printer.";
	    }
          }
	  else if (!strcmp(job->printer->error_policy, "retry-current-job"))
	  {
	   /*
	    * The error policy is "retry-current-job" - mark the job as pending
	    * and we'll retry on the same printer...
	    */

            if (job_state == IPP_JOB_COMPLETED)
	    {
	      job_state = IPP_JOB_PENDING;
	      message   = "Retrying job on same printer.";
	    }
          }
	  else if ((job->printer->type & CUPS_PRINTER_FAX) ||
        	   !strcmp(job->printer->error_policy, "retry-job"))
	  {
            if (job_state == IPP_JOB_COMPLETED)
	    {
	     /*
	      * The job was queued on a fax or the error policy is "retry-job" -
	      * hold the job if the number of retries is less than the
	      * JobRetryLimit, otherwise abort the job.
	      */

	      job->tries ++;

	      if (job->tries >= JobRetryLimit)
	      {
	       /*
		* Too many tries...
		*/

		snprintf(buffer, sizeof(buffer),
			 "Job aborted after %d unsuccessful attempts.",
			 JobRetryLimit);
		job_state = IPP_JOB_ABORTED;
		message   = buffer;
	      }
	      else
	      {
	       /*
		* Try again in N seconds...
		*/

		set_hold_until(job, time(NULL) + JobRetryInterval);

		snprintf(buffer, sizeof(buffer),
			 "Job held for %d seconds since it could not be sent.",
			 JobRetryInterval);
		job_state = IPP_JOB_HELD;
		message   = buffer;
	      }
            }
	  }
	  else if (!strcmp(job->printer->error_policy, "abort-job") &&
	           job_state == IPP_JOB_COMPLETED)
	  {
	    job_state = IPP_JOB_ABORTED;
	    message   = "Job aborted due to backend errors; please consult "
	                "the error_log file for details.";
	  }
	  else if (job->state_value == IPP_JOB_PROCESSING)
          {
            job_state     = IPP_JOB_PENDING;
	    printer_state = IPP_PRINTER_STOPPED;
	    message       = "Printer stopped due to backend errors; please "
			    "consult the error_log file for details.";
	  }
          break;

      case CUPS_BACKEND_CANCEL :
         /*
	  * Abort the job...
	  */

	  if (job_state == IPP_JOB_COMPLETED)
	  {
	    job_state = IPP_JOB_ABORTED;
	    message   = "Job aborted due to backend errors; please consult "
			"the error_log file for details.";
	  }
          break;

      case CUPS_BACKEND_HOLD :
	  if (job_state == IPP_JOB_COMPLETED)
	  {
	   /*
	    * Hold the job...
	    */

	    cupsdSetJobHoldUntil(job, "indefinite", 1);

	    job_state = IPP_JOB_HELD;
	    message   = "Job held indefinitely due to backend errors; please "
			"consult the error_log file for details.";
          }
          break;

      case CUPS_BACKEND_STOP :
         /*
	  * Stop the printer...
	  */

	  printer_state = IPP_PRINTER_STOPPED;
	  message       = "Printer stopped due to backend errors; please "
			  "consult the error_log file for details.";

	  if (job_state == IPP_JOB_COMPLETED)
	    job_state = IPP_JOB_PENDING;
          break;

      case CUPS_BACKEND_AUTH_REQUIRED :
         /*
	  * Hold the job for authentication...
	  */

	  if (job_state == IPP_JOB_COMPLETED)
	  {
	    cupsdSetJobHoldUntil(job, "auth-info-required", 1);

	    job_state = IPP_JOB_HELD;
	    message   = "Job held for authentication.";
          }
          break;
    }
  }
  else if (job->status > 0)
  {
   /*
    * Filter had errors; stop job...
    */

    if (job_state == IPP_JOB_COMPLETED)
    {
      job_state = IPP_JOB_STOPPED;
      message   = "Job stopped due to filter errors; please consult the "
		  "error_log file for details.";
    }
  }

 /*
  * Update the printer and job state.
  */

  cupsdSetJobState(job, job_state, CUPSD_JOB_DEFAULT, "%s", message);
  cupsdSetPrinterState(job->printer, printer_state,
                       printer_state == IPP_PRINTER_STOPPED);
  update_job_attrs(job, 0);

  if (job->history)
  {
    if (job->status)
      dump_job_history(job);
    else
      free_job_history(job);
  }

  cupsArrayRemove(PrintingJobs, job);

 /*
  * Clear the printer <-> job association...
  */

  job->printer->job = NULL;
  job->printer      = NULL;

 /*
  * Try printing another job...
  */

  if (printer_state != IPP_PRINTER_STOPPED)
    cupsdCheckJobs();
}


/*
 * 'get_options()' - Get a string containing the job options.
 */

static char *				/* O - Options string */
get_options(cupsd_job_t *job,		/* I - Job */
            int         banner_page,	/* I - Printing a banner page? */
	    char        *copies,	/* I - Copies buffer */
	    size_t      copies_size,	/* I - Size of copies buffer */
	    char        *title,		/* I - Title buffer */
	    size_t      title_size)	/* I - Size of title buffer */
{
  int			i;		/* Looping var */
  char			*optptr,	/* Pointer to options */
			*valptr;	/* Pointer in value string */
  ipp_attribute_t	*attr;		/* Current attribute */
  static char		*options = NULL;/* Full list of options */
  static int		optlength = 0;	/* Length of option buffer */


 /*
  * Building the options string is harder than it needs to be, but
  * for the moment we need to pass strings for command-line args and
  * not IPP attribute pointers... :)
  *
  * First allocate/reallocate the option buffer as needed...
  */

  i = ipp_length(job->attrs);

  if (i > optlength || !options)
  {
    if (!options)
      optptr = malloc(i);
    else
      optptr = realloc(options, i);

    if (!optptr)
    {
      cupsdLogJob(job, CUPSD_LOG_CRIT,
		  "Unable to allocate %d bytes for option buffer!", i);
      return (NULL);
    }

    options   = optptr;
    optlength = i;
  }

 /*
  * Now loop through the attributes and convert them to the textual
  * representation used by the filters...
  */

  optptr  = options;
  *optptr = '\0';

  snprintf(title, title_size, "%s-%d", job->printer->name, job->id);
  strlcpy(copies, "1", copies_size);

  for (attr = job->attrs->attrs; attr != NULL; attr = attr->next)
  {
    if (!strcmp(attr->name, "copies") &&
	attr->value_tag == IPP_TAG_INTEGER)
    {
     /*
      * Don't use the # copies attribute if we are printing the job sheets...
      */

      if (!banner_page)
        snprintf(copies, copies_size, "%d", attr->values[0].integer);
    }
    else if (!strcmp(attr->name, "job-name") &&
	     (attr->value_tag == IPP_TAG_NAME ||
	      attr->value_tag == IPP_TAG_NAMELANG))
      strlcpy(title, attr->values[0].string.text, title_size);
    else if (attr->group_tag == IPP_TAG_JOB)
    {
     /*
      * Filter out other unwanted attributes...
      */

      if (attr->value_tag == IPP_TAG_MIMETYPE ||
	  attr->value_tag == IPP_TAG_NAMELANG ||
	  attr->value_tag == IPP_TAG_TEXTLANG ||
	  (attr->value_tag == IPP_TAG_URI && strcmp(attr->name, "job-uuid")) ||
	  attr->value_tag == IPP_TAG_URISCHEME ||
	  attr->value_tag == IPP_TAG_BEGIN_COLLECTION) /* Not yet supported */
	continue;

      if (!strncmp(attr->name, "time-", 5))
	continue;

      if (!strncmp(attr->name, "job-", 4) &&
          strcmp(attr->name, "job-billing") &&
          strcmp(attr->name, "job-impressions") &&
          strcmp(attr->name, "job-originating-host-name") &&
          strcmp(attr->name, "job-uuid") &&
          !(job->printer->type & CUPS_PRINTER_REMOTE))
	continue;

      if ((!strcmp(attr->name, "job-impressions") ||
           !strcmp(attr->name, "page-label") ||
           !strcmp(attr->name, "page-border") ||
           !strncmp(attr->name, "number-up", 9) ||
	   !strcmp(attr->name, "page-ranges") ||
	   !strcmp(attr->name, "page-set") ||
	   !strcasecmp(attr->name, "AP_FIRSTPAGE_InputSlot") ||
	   !strcasecmp(attr->name, "AP_FIRSTPAGE_ManualFeed") ||
	   !strcasecmp(attr->name, "com.apple.print.PrintSettings."
	                           "PMTotalSidesImaged..n.") ||
	   !strcasecmp(attr->name, "com.apple.print.PrintSettings."
	                           "PMTotalBeginPages..n.")) &&
	  banner_page)
        continue;

     /*
      * Otherwise add them to the list...
      */

      if (optptr > options)
	strlcat(optptr, " ", optlength - (optptr - options));

      if (attr->value_tag != IPP_TAG_BOOLEAN)
      {
	strlcat(optptr, attr->name, optlength - (optptr - options));
	strlcat(optptr, "=", optlength - (optptr - options));
      }

      for (i = 0; i < attr->num_values; i ++)
      {
	if (i)
	  strlcat(optptr, ",", optlength - (optptr - options));

	optptr += strlen(optptr);

	switch (attr->value_tag)
	{
	  case IPP_TAG_INTEGER :
	  case IPP_TAG_ENUM :
	      snprintf(optptr, optlength - (optptr - options),
	               "%d", attr->values[i].integer);
	      break;

	  case IPP_TAG_BOOLEAN :
	      if (!attr->values[i].boolean)
		strlcat(optptr, "no", optlength - (optptr - options));

	  case IPP_TAG_NOVALUE :
	      strlcat(optptr, attr->name,
	              optlength - (optptr - options));
	      break;

	  case IPP_TAG_RANGE :
	      if (attr->values[i].range.lower == attr->values[i].range.upper)
		snprintf(optptr, optlength - (optptr - options) - 1,
	        	 "%d", attr->values[i].range.lower);
              else
		snprintf(optptr, optlength - (optptr - options) - 1,
	        	 "%d-%d", attr->values[i].range.lower,
			 attr->values[i].range.upper);
	      break;

	  case IPP_TAG_RESOLUTION :
	      snprintf(optptr, optlength - (optptr - options) - 1,
	               "%dx%d%s", attr->values[i].resolution.xres,
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
	  case IPP_TAG_URI :
	      for (valptr = attr->values[i].string.text; *valptr;)
	      {
	        if (strchr(" \t\n\\\'\"", *valptr))
		  *optptr++ = '\\';
		*optptr++ = *valptr++;
	      }

	      *optptr = '\0';
	      break;

          default :
	      break; /* anti-compiler-warning-code */
	}
      }

      optptr += strlen(optptr);
    }
  }


  return (options);
}


/*
 * 'ipp_length()' - Compute the size of the buffer needed to hold
 *		    the textual IPP attributes.
 */

static int				/* O - Size of attribute buffer */
ipp_length(ipp_t *ipp)			/* I - IPP request */
{
  int			bytes; 		/* Number of bytes */
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* Current attribute */


 /*
  * Loop through all attributes...
  */

  bytes = 0;

  for (attr = ipp->attrs; attr != NULL; attr = attr->next)
  {
   /*
    * Skip attributes that won't be sent to filters...
    */

    if (attr->value_tag == IPP_TAG_MIMETYPE ||
	attr->value_tag == IPP_TAG_NAMELANG ||
	attr->value_tag == IPP_TAG_TEXTLANG ||
	attr->value_tag == IPP_TAG_URI ||
	attr->value_tag == IPP_TAG_URISCHEME)
      continue;

    if (strncmp(attr->name, "time-", 5) == 0)
      continue;

   /*
    * Add space for a leading space and commas between each value.
    * For the first attribute, the leading space isn't used, so the
    * extra byte can be used as the nul terminator...
    */

    bytes ++;				/* " " separator */
    bytes += attr->num_values;		/* "," separators */

   /*
    * Boolean attributes appear as "foo,nofoo,foo,nofoo", while
    * other attributes appear as "foo=value1,value2,...,valueN".
    */

    if (attr->value_tag != IPP_TAG_BOOLEAN)
      bytes += strlen(attr->name);
    else
      bytes += attr->num_values * strlen(attr->name);

   /*
    * Now add the size required for each value in the attribute...
    */

    switch (attr->value_tag)
    {
      case IPP_TAG_INTEGER :
      case IPP_TAG_ENUM :
         /*
	  * Minimum value of a signed integer is -2147483647, or 11 digits.
	  */

	  bytes += attr->num_values * 11;
	  break;

      case IPP_TAG_BOOLEAN :
         /*
	  * Add two bytes for each false ("no") value...
	  */

          for (i = 0; i < attr->num_values; i ++)
	    if (!attr->values[i].boolean)
	      bytes += 2;
	  break;

      case IPP_TAG_RANGE :
         /*
	  * A range is two signed integers separated by a hyphen, or
	  * 23 characters max.
	  */

	  bytes += attr->num_values * 23;
	  break;

      case IPP_TAG_RESOLUTION :
         /*
	  * A resolution is two signed integers separated by an "x" and
	  * suffixed by the units, or 26 characters max.
	  */

	  bytes += attr->num_values * 26;
	  break;

      case IPP_TAG_STRING :
      case IPP_TAG_TEXT :
      case IPP_TAG_NAME :
      case IPP_TAG_KEYWORD :
      case IPP_TAG_CHARSET :
      case IPP_TAG_LANGUAGE :
      case IPP_TAG_URI :
         /*
	  * Strings can contain characters that need quoting.  We need
	  * at least 2 * len + 2 characters to cover the quotes and
	  * any backslashes in the string.
	  */

          for (i = 0; i < attr->num_values; i ++)
	    bytes += 2 * strlen(attr->values[i].string.text) + 2;
	  break;

       default :
	  break; /* anti-compiler-warning-code */
    }
  }

  return (bytes);
}


/*
 * 'load_job_cache()' - Load jobs from the job.cache file.
 */

static void
load_job_cache(const char *filename)	/* I - job.cache filename */
{
  cups_file_t	*fp;			/* job.cache file */
  char		line[1024],		/* Line buffer */
		*value;			/* Value on line */
  int		linenum;		/* Line number in file */
  cupsd_job_t	*job;			/* Current job */
  int		jobid;			/* Job ID */
  char		jobfile[1024];		/* Job filename */


 /*
  * Open the job.cache file...
  */

  if ((fp = cupsFileOpen(filename, "r")) == NULL)
  {
    if (errno != ENOENT)
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to open job cache file \"%s\": %s",
                      filename, strerror(errno));

    load_request_root();

    return;
  }

 /*
  * Read entries from the job cache file and create jobs as needed.
  */

  cupsdLogMessage(CUPSD_LOG_INFO, "Loading job cache file \"%s\"...",
                  filename);

  linenum = 0;
  job     = NULL;

  while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
  {
    if (!strcasecmp(line, "NextJobId"))
    {
      if (value)
        NextJobId = atoi(value);
    }
    else if (!strcasecmp(line, "<Job"))
    {
      if (job)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Missing </Job> directive on line %d!",
	                linenum);
        continue;
      }

      if (!value)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Missing job ID on line %d!", linenum);
	continue;
      }

      jobid = atoi(value);

      if (jobid < 1)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Bad job ID %d on line %d!", jobid,
	                linenum);
        continue;
      }

      snprintf(jobfile, sizeof(jobfile), "%s/c%05d", RequestRoot, jobid);
      if (access(jobfile, 0))
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "[Job %d] Files have gone away!",
	                jobid);
        continue;
      }

      job = calloc(1, sizeof(cupsd_job_t));
      if (!job)
      {
        cupsdLogMessage(CUPSD_LOG_EMERG,
		        "[Job %d] Unable to allocate memory for job!", jobid);
        break;
      }

      job->id              = jobid;
      job->back_pipes[0]   = -1;
      job->back_pipes[1]   = -1;
      job->print_pipes[0]  = -1;
      job->print_pipes[1]  = -1;
      job->side_pipes[0]   = -1;
      job->side_pipes[1]   = -1;
      job->status_pipes[0] = -1;
      job->status_pipes[1] = -1;

      cupsdLogMessage(CUPSD_LOG_DEBUG, "[Job %d] Loading from cache...",
                      job->id);
    }
    else if (!job)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
	              "Missing <Job #> directive on line %d!", linenum);
      continue;
    }
    else if (!strcasecmp(line, "</Job>"))
    {
      cupsArrayAdd(Jobs, job);

      if (job->state_value <= IPP_JOB_STOPPED && cupsdLoadJob(job))
	cupsArrayAdd(ActiveJobs, job);

      job = NULL;
    }
    else if (!value)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Missing value on line %d!", linenum);
      continue;
    }
    else if (!strcasecmp(line, "State"))
    {
      job->state_value = (ipp_jstate_t)atoi(value);

      if (job->state_value < IPP_JOB_PENDING)
        job->state_value = IPP_JOB_PENDING;
      else if (job->state_value > IPP_JOB_COMPLETED)
        job->state_value = IPP_JOB_COMPLETED;
    }
    else if (!strcasecmp(line, "HoldUntil"))
    {
      job->hold_until = atoi(value);
    }
    else if (!strcasecmp(line, "Priority"))
    {
      job->priority = atoi(value);
    }
    else if (!strcasecmp(line, "Username"))
    {
      cupsdSetString(&job->username, value);
    }
    else if (!strcasecmp(line, "Destination"))
    {
      cupsdSetString(&job->dest, value);
    }
    else if (!strcasecmp(line, "DestType"))
    {
      job->dtype = (cups_ptype_t)atoi(value);
    }
    else if (!strcasecmp(line, "NumFiles"))
    {
      job->num_files = atoi(value);

      if (job->num_files < 0)
      {
	cupsdLogMessage(CUPSD_LOG_ERROR, "Bad NumFiles value %d on line %d!",
	                job->num_files, linenum);
        job->num_files = 0;
	continue;
      }

      if (job->num_files > 0)
      {
        snprintf(jobfile, sizeof(jobfile), "%s/d%05d-001", RequestRoot,
	         job->id);
        if (access(jobfile, 0))
	{
	  cupsdLogMessage(CUPSD_LOG_INFO, "[Job %d] Data files have gone away!",
	                  job->id);
          job->num_files = 0;
	  continue;
	}

        job->filetypes    = calloc(job->num_files, sizeof(mime_type_t *));
	job->compressions = calloc(job->num_files, sizeof(int));

        if (!job->filetypes || !job->compressions)
	{
	  cupsdLogMessage(CUPSD_LOG_EMERG,
		          "[Job %d] Unable to allocate memory for %d files!",
		          job->id, job->num_files);
          break;
	}
      }
    }
    else if (!strcasecmp(line, "File"))
    {
      int	number,			/* File number */
		compression;		/* Compression value */
      char	super[MIME_MAX_SUPER],	/* MIME super type */
		type[MIME_MAX_TYPE];	/* MIME type */


      if (sscanf(value, "%d%*[ \t]%15[^/]/%255s%d", &number, super, type,
                 &compression) != 4)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Bad File on line %d!", linenum);
	continue;
      }

      if (number < 1 || number > job->num_files)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Bad File number %d on line %d!",
	                number, linenum);
        continue;
      }

      number --;

      job->compressions[number] = compression;
      job->filetypes[number]    = mimeType(MimeDatabase, super, type);

      if (!job->filetypes[number])
      {
       /*
        * If the original MIME type is unknown, auto-type it!
	*/

        cupsdLogMessage(CUPSD_LOG_ERROR,
		        "[Job %d] Unknown MIME type %s/%s for file %d!",
		        job->id, super, type, number + 1);

        snprintf(jobfile, sizeof(jobfile), "%s/d%05d-%03d", RequestRoot,
	         job->id, number + 1);
        job->filetypes[number] = mimeFileType(MimeDatabase, jobfile, NULL,
	                                      job->compressions + number);

       /*
        * If that didn't work, assume it is raw...
	*/

        if (!job->filetypes[number])
	  job->filetypes[number] = mimeType(MimeDatabase, "application",
	                                    "vnd.cups-raw");
      }
    }
    else
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unknown %s directive on line %d!",
                      line, linenum);
  }

  cupsFileClose(fp);
}


/*
 * 'load_next_job_id()' - Load the NextJobId value from the job.cache file.
 */

static void
load_next_job_id(const char *filename)	/* I - job.cache filename */
{
  cups_file_t	*fp;			/* job.cache file */
  char		line[1024],		/* Line buffer */
		*value;			/* Value on line */
  int		linenum;		/* Line number in file */
  int		next_job_id;		/* NextJobId value from line */


 /*
  * Read the NextJobId directive from the job.cache file and use
  * the value (if any).
  */

  if ((fp = cupsFileOpen(filename, "r")) == NULL)
  {
    if (errno != ENOENT)
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to open job cache file \"%s\": %s",
                      filename, strerror(errno));

    return;
  }

  cupsdLogMessage(CUPSD_LOG_INFO,
                  "Loading NextJobId from job cache file \"%s\"...", filename);

  linenum = 0;

  while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
  {
    if (!strcasecmp(line, "NextJobId"))
    {
      if (value)
      {
        next_job_id = atoi(value);

        if (next_job_id > NextJobId)
	  NextJobId = next_job_id;
      }
      break;
    }
  }

  cupsFileClose(fp);
}


/*
 * 'load_request_root()' - Load jobs from the RequestRoot directory.
 */

static void
load_request_root(void)
{
  cups_dir_t		*dir;		/* Directory */
  cups_dentry_t		*dent;		/* Directory entry */
  cupsd_job_t		*job;		/* New job */


 /*
  * Open the requests directory...
  */

  cupsdLogMessage(CUPSD_LOG_DEBUG, "Scanning %s for jobs...", RequestRoot);

  if ((dir = cupsDirOpen(RequestRoot)) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to open spool directory \"%s\": %s",
                    RequestRoot, strerror(errno));
    return;
  }

 /*
  * Read all the c##### files...
  */

  while ((dent = cupsDirRead(dir)) != NULL)
    if (strlen(dent->filename) >= 6 && dent->filename[0] == 'c')
    {
     /*
      * Allocate memory for the job...
      */

      if ((job = calloc(sizeof(cupsd_job_t), 1)) == NULL)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Ran out of memory for jobs!");
	cupsDirClose(dir);
	return;
      }

     /*
      * Assign the job ID...
      */

      job->id              = atoi(dent->filename + 1);
      job->back_pipes[0]   = -1;
      job->back_pipes[1]   = -1;
      job->print_pipes[0]  = -1;
      job->print_pipes[1]  = -1;
      job->side_pipes[0]   = -1;
      job->side_pipes[1]   = -1;
      job->status_pipes[0] = -1;
      job->status_pipes[1] = -1;

      if (job->id >= NextJobId)
        NextJobId = job->id + 1;

     /*
      * Load the job...
      */

      if (cupsdLoadJob(job))
      {
       /*
        * Insert the job into the array, sorting by job priority and ID...
        */

	cupsArrayAdd(Jobs, job);

	if (job->state_value <= IPP_JOB_STOPPED)
	  cupsArrayAdd(ActiveJobs, job);
	else
	  unload_job(job);
      }
    }

  cupsDirClose(dir);
}


/*
 * 'set_hold_until()' - Set the hold time and update job-hold-until attribute.
 */

static void
set_hold_until(cupsd_job_t *job, 	/* I - Job to update */
	       time_t      holdtime)	/* I - Hold until time */
{
  ipp_attribute_t	*attr;		/* job-hold-until attribute */
  struct tm		*holddate;	/* Hold date */
  char			holdstr[64];	/* Hold time */


 /*
  * Set the hold_until value and hold the job...
  */

  cupsdLogMessage(CUPSD_LOG_DEBUG, "set_hold_until: hold_until = %d",
                  (int)holdtime);

  job->state->values[0].integer = IPP_JOB_HELD;
  job->state_value              = IPP_JOB_HELD;
  job->hold_until               = holdtime;

 /*
  * Update the job-hold-until attribute with a string representing GMT
  * time (HH:MM:SS)...
  */

  holddate = gmtime(&holdtime);
  snprintf(holdstr, sizeof(holdstr), "%d:%d:%d", holddate->tm_hour,
	   holddate->tm_min, holddate->tm_sec);

  if ((attr = ippFindAttribute(job->attrs, "job-hold-until",
                               IPP_TAG_KEYWORD)) == NULL)
    attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

 /*
  * Either add the attribute or update the value of the existing one
  */

  if (attr == NULL)
    ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_KEYWORD, "job-hold-until",
                 NULL, holdstr);
  else
    cupsdSetString(&attr->values[0].string.text, holdstr);

  job->dirty = 1;
  cupsdMarkDirty(CUPSD_DIRTY_JOBS);
}


/*
 * 'set_time()' - Set one of the "time-at-xyz" attributes.
 */

static void
set_time(cupsd_job_t *job,		/* I - Job to update */
         const char  *name)		/* I - Name of attribute */
{
  ipp_attribute_t	*attr;		/* Time attribute */


  if ((attr = ippFindAttribute(job->attrs, name, IPP_TAG_ZERO)) != NULL)
  {
    attr->value_tag         = IPP_TAG_INTEGER;
    attr->values[0].integer = time(NULL);
  }
}


/*
 * 'start_job()' - Start a print job.
 */

static void
start_job(cupsd_job_t     *job,		/* I - Job ID */
          cupsd_printer_t *printer)	/* I - Printer to print job */
{
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "start_job(job=%p(%d), printer=%p(%s))",
                  job, job->id, printer, printer->name);

 /*
  * Make sure we have some files around before we try to print...
  */

  if (job->num_files == 0)
  {
    cupsdSetJobState(job, IPP_JOB_ABORTED, CUPSD_JOB_DEFAULT,
                     "Aborting job because it has no files.");
    return;
  }

 /*
  * Update the printer and job state to "processing"...
  */

  if (!cupsdLoadJob(job))
    return;

  cupsdSetJobState(job, IPP_JOB_PROCESSING, CUPSD_JOB_DEFAULT, NULL);
  cupsdSetPrinterState(printer, IPP_PRINTER_PROCESSING, 0);

  job->cost         = 0;
  job->current_file = 0;
  job->progress     = 0;
  job->printer      = printer;
  printer->job      = job;

 /*
  * Setup the last exit status and security profiles...
  */

  job->status  = 0;
  job->profile = cupsdCreateProfile(job->id);

 /*
  * Create the status pipes and buffer...
  */

  if (cupsdOpenPipe(job->status_pipes))
  {
    cupsdLogJob(job, CUPSD_LOG_DEBUG,
		"Unable to create job status pipes - %s.", strerror(errno));

    cupsdSetJobState(job, IPP_JOB_STOPPED, CUPSD_JOB_DEFAULT,
		     "Job stopped because the scheduler could not create the "
		     "job status pipes.");

    cupsdDestroyProfile(job->profile);
    job->profile = NULL;
    return;
  }

  job->status_buffer = cupsdStatBufNew(job->status_pipes[0], NULL);
  job->status_level  = CUPSD_LOG_INFO;

  if (job->printer_message)
    cupsdSetString(&(job->printer_message->values[0].string.text), "");

 /*
  * Create the backchannel pipes and make them non-blocking...
  */

  if (cupsdOpenPipe(job->back_pipes))
  {
    cupsdLogJob(job, CUPSD_LOG_DEBUG,
		"Unable to create back-channel pipes - %s.", strerror(errno));

    cupsdSetJobState(job, IPP_JOB_STOPPED, CUPSD_JOB_DEFAULT,
		     "Job stopped because the scheduler could not create the "
		     "back-channel pipes.");

    cupsdClosePipe(job->status_pipes);
    cupsdStatBufDelete(job->status_buffer);
    job->status_buffer = NULL;

    cupsdDestroyProfile(job->profile);
    job->profile = NULL;
    return;
  }

  fcntl(job->back_pipes[0], F_SETFL,
	fcntl(job->back_pipes[0], F_GETFL) | O_NONBLOCK);
  fcntl(job->back_pipes[1], F_SETFL,
	fcntl(job->back_pipes[1], F_GETFL) | O_NONBLOCK);

 /*
  * Create the side-channel pipes and make them non-blocking...
  */

  if (socketpair(AF_LOCAL, SOCK_STREAM, 0, job->side_pipes))
  {
    cupsdLogJob(job, CUPSD_LOG_DEBUG,
		"Unable to create side-channel pipes - %s.", strerror(errno));

    cupsdSetJobState(job, IPP_JOB_STOPPED, CUPSD_JOB_DEFAULT,
		     "Job stopped because the scheduler could not create the "
		     "side-channel pipes.");

    cupsdClosePipe(job->back_pipes);

    cupsdClosePipe(job->status_pipes);
    cupsdStatBufDelete(job->status_buffer);
    job->status_buffer = NULL;

    cupsdDestroyProfile(job->profile);
    job->profile = NULL;
    return;
  }

  fcntl(job->side_pipes[0], F_SETFL,
	fcntl(job->side_pipes[0], F_GETFL) | O_NONBLOCK);
  fcntl(job->side_pipes[1], F_SETFL,
	fcntl(job->side_pipes[1], F_GETFL) | O_NONBLOCK);

 /*
  * Now start the first file in the job...
  */

  cupsdContinueJob(job);
}


/*
 * 'stop_job()' - Stop a print job.
 */

static void
stop_job(cupsd_job_t       *job,	/* I - Job */
         cupsd_jobaction_t action)	/* I - Action */
{
  int	i;				/* Looping var */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "stop_job(job=%p(%d), action=%d)", job,
                  job->id, action);

  FilterLevel -= job->cost;
  job->cost   = 0;

  if (action == CUPSD_JOB_DEFAULT && !job->kill_time)
    job->kill_time = time(NULL) + JobKillDelay;
  else if (action == CUPSD_JOB_FORCE)
    job->kill_time = 0;

  for (i = 0; job->filters[i]; i ++)
    if (job->filters[i] > 0)
      cupsdEndProcess(job->filters[i], action == CUPSD_JOB_FORCE);

  if (job->backend > 0)
    cupsdEndProcess(job->backend, action == CUPSD_JOB_FORCE);
}


/*
 * 'unload_job()' - Unload a job from memory.
 */

static void
unload_job(cupsd_job_t *job)		/* I - Job */
{
  if (!job->attrs)
    return;

  cupsdLogMessage(CUPSD_LOG_DEBUG, "[Job %d] Unloading...", job->id);

  ippDelete(job->attrs);

  job->attrs           = NULL;
  job->state           = NULL;
  job->sheets          = NULL;
  job->job_sheets      = NULL;
  job->printer_message = NULL;
  job->printer_reasons = NULL;
}


/*
 * 'update_job()' - Read a status update from a job's filters.
 */

void
update_job(cupsd_job_t *job)		/* I - Job to check */
{
  int		i;			/* Looping var */
  int		copies;			/* Number of copies printed */
  char		message[CUPSD_SB_BUFFER_SIZE],
					/* Message text */
		*ptr;			/* Pointer update... */
  int		loglevel,		/* Log level for message */
		event = 0;		/* Events? */
  static const char * const levels[] =	/* Log levels */
		{
		  "NONE",
		  "EMERG",
		  "ALERT",
		  "CRIT",
		  "ERROR",
		  "WARN",
		  "NOTICE",
		  "INFO",
		  "DEBUG",
		  "DEBUG2"
		};


 /*
  * Get the printer associated with this job; if the printer is stopped for
  * any reason then job->printer will be reset to NULL, so make sure we have
  * a valid pointer...
  */

  while ((ptr = cupsdStatBufUpdate(job->status_buffer, &loglevel,
                                   message, sizeof(message))) != NULL)
  {
   /*
    * Process page and printer state messages as needed...
    */

    if (loglevel == CUPSD_LOG_PAGE)
    {
     /*
      * Page message; send the message to the page_log file and update the
      * job sheet count...
      */

      cupsdLogJob(job, CUPSD_LOG_DEBUG, "PAGE: %s", message);

      if (job->sheets)
      {
        if (!strncasecmp(message, "total ", 6))
	{
	 /*
	  * Got a total count of pages from a backend or filter...
	  */

	  copies = atoi(message + 6);
	  copies -= job->sheets->values[0].integer; /* Just track the delta */
	}
	else if (!sscanf(message, "%*d%d", &copies))
	  copies = 1;

        job->sheets->values[0].integer += copies;

	if (job->printer->page_limit)
	{
	  cupsd_quota_t *q = cupsdUpdateQuota(job->printer, job->username,
					      copies, 0);

#ifdef __APPLE__
	  if (AppleQuotas && q->page_count == -3)
	  {
	   /*
	    * Quota limit exceeded, cancel job in progress immediately...
	    */

	    cupsdSetJobState(job, IPP_JOB_CANCELED, CUPSD_JOB_DEFAULT,
	                     "Canceled job because pages exceed user %s "
			     "quota limit on printer %s (%s).",
			     job->username, job->printer->name,
			     job->printer->info);
	    return;
	  }
#else
          (void)q;
#endif /* __APPLE__ */
	}
      }

      cupsdLogPage(job, message);

      if (job->sheets)
	cupsdAddEvent(CUPSD_EVENT_JOB_PROGRESS, job->printer, job,
		      "Printed %d page(s).", job->sheets->values[0].integer);
    }
    else if (loglevel == CUPSD_LOG_STATE)
    {
      cupsdLogJob(job, CUPSD_LOG_DEBUG, "STATE: %s", message);

      if (!strcmp(message, "paused"))
      {
        cupsdStopPrinter(job->printer, 1);
	return;
      }
      else if (cupsdSetPrinterReasons(job->printer, message))
      {
	cupsdAddPrinterHistory(job->printer);
	event |= CUPSD_EVENT_PRINTER_STATE;
      }

      update_job_attrs(job, 0);
    }
    else if (loglevel == CUPSD_LOG_ATTR)
    {
     /*
      * Set attribute(s)...
      */

      int		num_attrs;	/* Number of attributes */
      cups_option_t	*attrs;		/* Attributes */
      const char	*attr;		/* Attribute */


      cupsdLogJob(job, CUPSD_LOG_DEBUG, "ATTR: %s", message);

      num_attrs = cupsParseOptions(message, 0, &attrs);

      if ((attr = cupsGetOption("auth-info-required", num_attrs,
                                attrs)) != NULL)
      {
        cupsdSetAuthInfoRequired(job->printer, attr, NULL);
	cupsdSetPrinterAttrs(job->printer);

	if (job->printer->type & CUPS_PRINTER_DISCOVERED)
	  cupsdMarkDirty(CUPSD_DIRTY_REMOTE);
	else
	  cupsdMarkDirty(CUPSD_DIRTY_PRINTERS);
      }

      if ((attr = cupsGetOption("job-media-progress", num_attrs,
                                attrs)) != NULL)
      {
        int progress = atoi(attr);


        if (progress >= 0 && progress <= 100)
	{
	  job->progress = progress;

	  if (job->sheets)
	    cupsdAddEvent(CUPSD_EVENT_JOB_PROGRESS, job->printer, job,
			  "Printing page %d, %d%%",
			  job->sheets->values[0].integer, job->progress);
        }
      }

      if ((attr = cupsGetOption("printer-alert", num_attrs, attrs)) != NULL)
      {
        cupsdSetString(&job->printer->alert, attr);
	event |= CUPSD_EVENT_PRINTER_STATE;
      }

      if ((attr = cupsGetOption("printer-alert-description", num_attrs,
                                attrs)) != NULL)
      {
        cupsdSetString(&job->printer->alert_description, attr);
	event |= CUPSD_EVENT_PRINTER_STATE;
      }

      if ((attr = cupsGetOption("marker-colors", num_attrs, attrs)) != NULL)
      {
        cupsdSetPrinterAttr(job->printer, "marker-colors", (char *)attr);
	job->printer->marker_time = time(NULL);
	event |= CUPSD_EVENT_PRINTER_STATE;
        cupsdMarkDirty(CUPSD_DIRTY_PRINTERS);
      }

      if ((attr = cupsGetOption("marker-levels", num_attrs, attrs)) != NULL)
      {
        cupsdSetPrinterAttr(job->printer, "marker-levels", (char *)attr);
	job->printer->marker_time = time(NULL);
	event |= CUPSD_EVENT_PRINTER_STATE;
        cupsdMarkDirty(CUPSD_DIRTY_PRINTERS);
      }

      if ((attr = cupsGetOption("marker-low-levels", num_attrs, attrs)) != NULL)
      {
        cupsdSetPrinterAttr(job->printer, "marker-low-levels", (char *)attr);
	job->printer->marker_time = time(NULL);
	event |= CUPSD_EVENT_PRINTER_STATE;
        cupsdMarkDirty(CUPSD_DIRTY_PRINTERS);
      }

      if ((attr = cupsGetOption("marker-high-levels", num_attrs, attrs)) != NULL)
      {
        cupsdSetPrinterAttr(job->printer, "marker-high-levels", (char *)attr);
	job->printer->marker_time = time(NULL);
	event |= CUPSD_EVENT_PRINTER_STATE;
        cupsdMarkDirty(CUPSD_DIRTY_PRINTERS);
      }

      if ((attr = cupsGetOption("marker-message", num_attrs, attrs)) != NULL)
      {
        cupsdSetPrinterAttr(job->printer, "marker-message", (char *)attr);
	job->printer->marker_time = time(NULL);
	event |= CUPSD_EVENT_PRINTER_STATE;
        cupsdMarkDirty(CUPSD_DIRTY_PRINTERS);
      }

      if ((attr = cupsGetOption("marker-names", num_attrs, attrs)) != NULL)
      {
        cupsdSetPrinterAttr(job->printer, "marker-names", (char *)attr);
	job->printer->marker_time = time(NULL);
	event |= CUPSD_EVENT_PRINTER_STATE;
        cupsdMarkDirty(CUPSD_DIRTY_PRINTERS);
      }

      if ((attr = cupsGetOption("marker-types", num_attrs, attrs)) != NULL)
      {
        cupsdSetPrinterAttr(job->printer, "marker-types", (char *)attr);
	job->printer->marker_time = time(NULL);
	event |= CUPSD_EVENT_PRINTER_STATE;
        cupsdMarkDirty(CUPSD_DIRTY_PRINTERS);
      }

      cupsFreeOptions(num_attrs, attrs);
    }
    else if (loglevel == CUPSD_LOG_PPD)
    {
     /*
      * Set attribute(s)...
      */

      int		num_keywords;	/* Number of keywords */
      cups_option_t	*keywords;	/* Keywords */


      cupsdLogJob(job, CUPSD_LOG_DEBUG, "PPD: %s", message);

      num_keywords = cupsParseOptions(message, 0, &keywords);

      if (cupsdUpdatePrinterPPD(job->printer, num_keywords, keywords))
        cupsdSetPrinterAttrs(job->printer);

      cupsFreeOptions(num_keywords, keywords);
    }
    else if (!strncmp(message, "recoverable:", 12))
    {
      ptr = message + 12;
      while (isspace(*ptr & 255))
        ptr ++;

      if (*ptr)
      {
	if (cupsdSetPrinterReasons(job->printer,
				   "+com.apple.print.recoverable-warning") ||
	    !job->printer->recoverable ||
	    strcmp(job->printer->recoverable, ptr))
	{
	  cupsdSetString(&(job->printer->recoverable), ptr);
	  cupsdAddPrinterHistory(job->printer);
	  event |= CUPSD_EVENT_PRINTER_STATE;
	}
      }
    }
    else if (!strncmp(message, "recovered:", 10))
    {
      ptr = message + 10;
      while (isspace(*ptr & 255))
        ptr ++;

      if (cupsdSetPrinterReasons(job->printer,
                                 "-com.apple.print.recoverable-warning") ||
	  !job->printer->recoverable || strcmp(job->printer->recoverable, ptr))
      {
	cupsdSetString(&(job->printer->recoverable), ptr);
	cupsdAddPrinterHistory(job->printer);
	event |= CUPSD_EVENT_PRINTER_STATE;
      }
    }
    else
    {
      cupsdLogJob(job, loglevel, "%s", message);

      if (loglevel < CUPSD_LOG_DEBUG)
      {
	strlcpy(job->printer->state_message, message,
		sizeof(job->printer->state_message));
	cupsdAddPrinterHistory(job->printer);

	event |= CUPSD_EVENT_PRINTER_STATE | CUPSD_EVENT_JOB_PROGRESS;

	if (loglevel < job->status_level)
	{
	 /*
	  * Some messages show in the job-printer-state-message attribute...
	  */

	  if (loglevel != CUPSD_LOG_NOTICE)
	    job->status_level = loglevel;

	  update_job_attrs(job, 1);

	  cupsdLogJob(job, CUPSD_LOG_DEBUG,
	              "Set job-printer-state-message to \"%s\", "
	              "current level=%s",
	              job->printer_message->values[0].string.text,
	              levels[job->status_level]);
	}
      }
    }

    if (!strchr(job->status_buffer->buffer, '\n'))
      break;
  }

  if (event & CUPSD_EVENT_PRINTER_STATE)
    cupsdAddEvent(CUPSD_EVENT_PRINTER_STATE, job->printer, NULL,
		  (job->printer->type & CUPS_PRINTER_CLASS) ?
		      "Class \"%s\" state changed." :
		      "Printer \"%s\" state changed.",
		  job->printer->name);

  if (event & CUPSD_EVENT_JOB_PROGRESS)
    cupsdAddEvent(CUPSD_EVENT_JOB_PROGRESS, job->printer, job,
                  "%s", job->printer->state_message);

  if (ptr == NULL && !job->status_buffer->bufused)
  {
   /*
    * See if all of the filters and the backend have returned their
    * exit statuses.
    */

    for (i = 0; job->filters[i] < 0; i ++);

    if (job->filters[i])
    {
     /*
      * EOF but we haven't collected the exit status of all filters...
      */

      cupsdCheckProcess();
      return;
    }

    if (job->current_file >= job->num_files && job->backend > 0)
    {
     /*
      * EOF but we haven't collected the exit status of the backend...
      */

      cupsdCheckProcess();
      return;
    }

   /*
    * Handle the end of job stuff...
    */

    finalize_job(job);

   /*
    * Check for new jobs...
    */

    cupsdCheckJobs();
  }
}


/*
 * 'update_job_attrs()' - Update the job-printer-* attributes.
 */

void
update_job_attrs(cupsd_job_t *job,	/* I - Job to update */
                 int         do_message)/* I - 1 = copy job-printer-state message */
{
  int			i;		/* Looping var */
  int			num_reasons;	/* Actual number of reasons */
  const char * const	*reasons;	/* Reasons */
  static const char	*none = "none";	/* "none" reason */


 /*
  * Get/create the job-printer-state-* attributes...
  */

  if (!job->printer_message)
  {
    if ((job->printer_message = ippFindAttribute(job->attrs,
                                                 "job-printer-state-message",
						 IPP_TAG_TEXT)) == NULL)
      job->printer_message = ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_TEXT,
                                          "job-printer-state-message",
					  NULL, "");
  }

  if (!job->printer_reasons)
    job->printer_reasons = ippFindAttribute(job->attrs,
					    "job-printer-state-reasons",
					    IPP_TAG_KEYWORD);

 /*
  * Copy or clear the printer-state-message value as needed...
  */

  if (job->state_value != IPP_JOB_PROCESSING &&
      job->status_level == CUPSD_LOG_INFO)
    cupsdSetString(&(job->printer_message->values[0].string.text), "");
  else if (job->printer->state_message[0] && do_message)
    cupsdSetString(&(job->printer_message->values[0].string.text),
		   job->printer->state_message);
  
 /*
  * ... and the printer-state-reasons value...
  */

  if (job->printer->num_reasons == 0)
  {
    num_reasons = 1;
    reasons     = &none;
  }
  else
  {
    num_reasons = job->printer->num_reasons;
    reasons     = (const char * const *)job->printer->reasons;
  }

  if (!job->printer_reasons || job->printer_reasons->num_values != num_reasons)
  {
   /*
    * Replace/create a job-printer-state-reasons attribute...
    */

    ippDeleteAttribute(job->attrs, job->printer_reasons);

    job->printer_reasons = ippAddStrings(job->attrs,
                                         IPP_TAG_JOB, IPP_TAG_KEYWORD,
					 "job-printer-state-reasons",
					 num_reasons, NULL, NULL);
  }
  else
  {
   /*
    * Don't bother clearing the reason strings if they are the same...
    */

    for (i = 0; i < num_reasons; i ++)
      if (strcmp(job->printer_reasons->values[i].string.text, reasons[i]))
        break;

    if (i >= num_reasons)
      return;

   /*
    * Not the same, so free the current strings...
    */

    for (i = 0; i < num_reasons; i ++)
      _cupsStrFree(job->printer_reasons->values[i].string.text);
  }

 /*
  * Copy the reasons...
  */

  for (i = 0; i < num_reasons; i ++)
    job->printer_reasons->values[i].string.text = _cupsStrAlloc(reasons[i]);
}


/*
 * End of "$Id: job.c 7902 2008-09-03 14:20:17Z mike $".
 */
