/*
 * "$Id: job.c 13047 2016-01-13 19:16:12Z msweet $"
 *
 * Job management routines for the CUPS scheduler.
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
#include <cups/backend.h>
#include <cups/dir.h>
#ifdef __APPLE__
#  include <IOKit/pwr_mgt/IOPMLib.h>
#  ifdef HAVE_IOKIT_PWR_MGT_IOPMLIBPRIVATE_H
#    include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#  endif /* HAVE_IOKIT_PWR_MGT_IOPMLIBPRIVATE_H */
#endif /* __APPLE__ */


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
static int	compare_completed_jobs(void *first, void *second, void *data);
static int	compare_jobs(void *first, void *second, void *data);
static void	dump_job_history(cupsd_job_t *job);
static void	finalize_job(cupsd_job_t *job, int set_job_state);
static void	free_job_history(cupsd_job_t *job);
static char	*get_options(cupsd_job_t *job, int banner_page, char *copies,
		             size_t copies_size, char *title,
			     size_t title_size);
static size_t	ipp_length(ipp_t *ipp);
static void	load_job_cache(const char *filename);
static void	load_next_job_id(const char *filename);
static void	load_request_root(void);
static void	remove_job_files(cupsd_job_t *job);
static void	remove_job_history(cupsd_job_t *job);
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
      else if (job->state_value < IPP_JOB_CANCELED)
	cupsdSetJobState(job, IPP_JOB_CANCELED, CUPSD_JOB_DEFAULT,
			 "Job canceled by user.");
    }
  }
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


  curtime = time(NULL);

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdCheckJobs: %d active jobs, sleeping=%d, ac-power=%d, reload=%d, curtime=%ld", cupsArrayCount(ActiveJobs), Sleeping, ACPower, NeedReload, (long)curtime);

  for (job = (cupsd_job_t *)cupsArrayFirst(ActiveJobs);
       job;
       job = (cupsd_job_t *)cupsArrayNext(ActiveJobs))
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdCheckJobs: Job %d - dest=\"%s\", printer=%p, "
                    "state=%d, cancel_time=%ld, hold_until=%ld, kill_time=%ld, "
                    "pending_cost=%d, pending_timeout=%ld", job->id, job->dest,
                    job->printer, job->state_value, (long)job->cancel_time,
                    (long)job->hold_until, (long)job->kill_time,
                    job->pending_cost, (long)job->pending_timeout);

   /*
    * Kill jobs if they are unresponsive...
    */

    if (job->kill_time && job->kill_time <= curtime)
    {
      if (!job->completed)
        cupsdLogJob(job, CUPSD_LOG_ERROR, "Stopping unresponsive job.");

      stop_job(job, CUPSD_JOB_FORCE);
      continue;
    }

   /*
    * Cancel stuck jobs...
    */

    if (job->cancel_time && job->cancel_time <= curtime)
    {
      int cancel_after;			/* job-cancel-after value */

      attr         = ippFindAttribute(job->attrs, "job-cancel-after", IPP_TAG_INTEGER);
      cancel_after = attr ? ippGetInteger(attr, 0) : MaxJobTime;

      if (job->completed)
	cupsdSetJobState(job, IPP_JOB_CANCELED, CUPSD_JOB_FORCE, "Marking stuck job as completed after %d seconds.", cancel_after);
      else
	cupsdSetJobState(job, IPP_JOB_CANCELED, CUPSD_JOB_DEFAULT, "Canceling stuck job after %d seconds.", cancel_after);
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

    if (job->state_value == IPP_JOB_PENDING && !NeedReload &&
        (!Sleeping || ACPower) && !DoingShutdown && !job->printer)
    {
      printer = cupsdFindDest(job->dest);
      pclass  = NULL;

      while (printer && (printer->type & CUPS_PRINTER_CLASS))
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
            ippSetString(job->attrs, &attr, 0, printer->uri);
	  else
	    ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI,
	                 "job-actual-printer-uri", NULL, printer->uri);

          job->dirty = 1;
          cupsdMarkDirty(CUPSD_DIRTY_JOBS);
	}

        if (!printer->job && printer->state == IPP_PRINTER_IDLE)
        {
	 /*
	  * Start the job...
	  */

	  cupsArraySave(ActiveJobs);
	  start_job(job, printer);
	  cupsArrayRestore(ActiveJobs);
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
  time_t	curtime;		/* Current time */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdCleanJobs: MaxJobs=%d, JobHistory=%d, JobFiles=%d",
                  MaxJobs, JobHistory, JobFiles);

  if (MaxJobs <= 0 && JobHistory == INT_MAX && JobFiles == INT_MAX)
    return;

  curtime          = time(NULL);
  JobHistoryUpdate = 0;

  for (job = (cupsd_job_t *)cupsArrayFirst(Jobs);
       job;
       job = (cupsd_job_t *)cupsArrayNext(Jobs))
  {
    if (job->state_value >= IPP_JOB_CANCELED && !job->printer)
    {
     /*
      * Expire old jobs (or job files)...
      */

      if ((MaxJobs > 0 && cupsArrayCount(Jobs) >= MaxJobs) ||
          (job->history_time && job->history_time <= curtime))
      {
        cupsdLogJob(job, CUPSD_LOG_DEBUG, "Removing from history.");
	cupsdDeleteJob(job, CUPSD_JOB_PURGE);
      }
      else if (job->file_time && job->file_time <= curtime)
      {
        cupsdLogJob(job, CUPSD_LOG_DEBUG, "Removing document files.");
        remove_job_files(job);

        cupsdMarkDirty(CUPSD_DIRTY_JOBS);

        if (job->history_time < JobHistoryUpdate || !JobHistoryUpdate)
	  JobHistoryUpdate = job->history_time;
      }
      else
      {
        if (job->history_time < JobHistoryUpdate || !JobHistoryUpdate)
	  JobHistoryUpdate = job->history_time;

	if (job->file_time < JobHistoryUpdate || !JobHistoryUpdate)
	  JobHistoryUpdate = job->file_time;
      }
    }
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdCleanJobs: JobHistoryUpdate=%ld",
                  (long)JobHistoryUpdate);
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
  struct stat		fileinfo;	/* Job file information */
  int			argc = 0;	/* Number of arguments */
  char			**argv = NULL,	/* Filter command-line arguments */
			filename[1024],	/* Job filename */
			command[1024],	/* Full path to command */
			jobid[255],	/* Job ID string */
			title[IPP_MAX_NAME],
					/* Job title string */
			copies[255],	/* # copies string */
			*options,	/* Options string */
			*envp[MAX_ENV + 21],
					/* Environment variables */
			charset[255],	/* CHARSET env variable */
			class_name[255],/* CLASS env variable */
			classification[1024],
					/* CLASSIFICATION env variable */
			content_type[1024],
					/* CONTENT_TYPE env variable */
			device_uri[1024],
					/* DEVICE_URI env variable */
			final_content_type[1024] = "",
					/* FINAL_CONTENT_TYPE env variable */
			lang[255],	/* LANG env variable */
#ifdef __APPLE__
			apple_language[255],
					/* APPLE_LANGUAGE env variable */
#endif /* __APPLE__ */
			auth_info_required[255],
					/* AUTH_INFO_REQUIRED env variable */
			ppd[1024],	/* PPD env variable */
			printer_info[255],
					/* PRINTER_INFO env variable */
			printer_location[255],
					/* PRINTER_LOCATION env variable */
			printer_name[255],
					/* PRINTER env variable */
			*printer_state_reasons = NULL,
					/* PRINTER_STATE_REASONS env var */
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

    mime_type_t	*dst = job->printer->filetype;
					/* Destination file type */

    snprintf(filename, sizeof(filename), "%s/d%05d-%03d", RequestRoot,
             job->id, job->current_file + 1);
    if (stat(filename, &fileinfo))
      fileinfo.st_size = 0;

    if (job->retry_as_raster)
    {
     /*
      * Need to figure out whether the printer supports image/pwg-raster or
      * image/urf, and use the corresponding type...
      */

      char	type[MIME_MAX_TYPE];	/* MIME media type for printer */

      snprintf(type, sizeof(type), "%s/image/urf", job->printer->name);
      if ((dst = mimeType(MimeDatabase, "printer", type)) == NULL)
      {
	snprintf(type, sizeof(type), "%s/image/pwg-raster", job->printer->name);
	dst = mimeType(MimeDatabase, "printer", type);
      }

      if (dst)
        cupsdLogJob(job, CUPSD_LOG_DEBUG, "Retrying job as \"%s\".", strchr(dst->type, '/') + 1);
      else
        cupsdLogJob(job, CUPSD_LOG_ERROR, "Unable to retry job using a supported raster format.");
    }

    filters = mimeFilter2(MimeDatabase, job->filetypes[job->current_file], (size_t)fileinfo.st_size, dst, &(job->cost));

    if (!filters)
    {
      cupsdLogJob(job, CUPSD_LOG_ERROR,
		  "Unable to convert file %d to printable format.",
		  job->current_file);

      abort_message = "Aborting job because it cannot be printed.";
      abort_state   = IPP_JOB_ABORTED;

      ippSetString(job->attrs, &job->reasons, 0, "document-unprintable-error");
      goto abort_job;
    }

   /*
    * Figure out the final content type...
    */

    cupsdLogJob(job, CUPSD_LOG_DEBUG, "%d filters for job:",
                cupsArrayCount(filters));
    for (filter = (mime_filter_t *)cupsArrayFirst(filters);
         filter;
         filter = (mime_filter_t *)cupsArrayNext(filters))
      cupsdLogJob(job, CUPSD_LOG_DEBUG, "%s (%s/%s to %s/%s, cost %d)",
		  filter->filter,
		  filter->src ? filter->src->super : "???",
		  filter->src ? filter->src->type : "???",
		  filter->dst ? filter->dst->super : "???",
		  filter->dst ? filter->dst->type : "???",
		  filter->cost);

    if (!job->printer->remote)
    {
      for (filter = (mime_filter_t *)cupsArrayLast(filters);
           filter && filter->dst;
           filter = (mime_filter_t *)cupsArrayPrev(filters))
        if (strcmp(filter->dst->super, "printer") ||
            strcmp(filter->dst->type, job->printer->name))
          break;

      if (filter && filter->dst)
      {
	if ((ptr = strchr(filter->dst->type, '/')) != NULL)
	  snprintf(final_content_type, sizeof(final_content_type),
		   "FINAL_CONTENT_TYPE=%s", ptr + 1);
	else
	  snprintf(final_content_type, sizeof(final_content_type),
		   "FINAL_CONTENT_TYPE=%s/%s", filter->dst->super,
		   filter->dst->type);
      }
      else
        snprintf(final_content_type, sizeof(final_content_type),
                 "FINAL_CONTENT_TYPE=printer/%s", job->printer->name);
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

  if (job->compressions[job->current_file] &&
      (!job->printer->remote || job->num_files == 1))
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
		"Too many filters (%d > %d), unable to print.",
		cupsArrayCount(filters), MAX_FILTERS);

    abort_message = "Aborting job because it needs too many filters to print.";
    abort_state   = IPP_JOB_ABORTED;

    ippSetString(job->attrs, &job->reasons, 0, "document-unprintable-error");

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
		  "... but someone added one without setting job_sheets.");
  }
  else if (job->job_sheets->num_values == 1)
    cupsdLogJob(job, CUPSD_LOG_DEBUG, "job-sheets=%s",
		job->job_sheets->values[0].string.text);
  else
    cupsdLogJob(job, CUPSD_LOG_DEBUG, "job-sheets=%s,%s",
                job->job_sheets->values[0].string.text,
                job->job_sheets->values[1].string.text);

  if (job->printer->type & CUPS_PRINTER_REMOTE)
    banner_page = 0;
  else if (job->job_sheets == NULL)
    banner_page = 0;
  else if (_cups_strcasecmp(job->job_sheets->values[0].string.text, "none") != 0 &&
	   job->current_file == 0)
    banner_page = 1;
  else if (job->job_sheets->num_values > 1 &&
	   _cups_strcasecmp(job->job_sheets->values[1].string.text, "none") != 0 &&
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
    argc = 6 + job->num_files;
  else
    argc = 7;

  if ((argv = calloc((size_t)argc + 1, sizeof(char *))) == NULL)
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
    argv[6] = strdup(filename);
  }

  for (i = 0; argv[i]; i ++)
    cupsdLogJob(job, CUPSD_LOG_DEBUG, "argv[%d]=\"%s\"", i, argv[i]);

 /*
  * Create environment variable strings for the filters...
  */

  attr = ippFindAttribute(job->attrs, "attributes-natural-language",
                          IPP_TAG_LANGUAGE);

#ifdef __APPLE__
  strlcpy(apple_language, "APPLE_LANGUAGE=", sizeof(apple_language));
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

	strlcpy(lang, "LANG=C", sizeof(lang));
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
  if (job->printer->num_reasons > 0)
  {
    char	*psrptr;		/* Pointer into PRINTER_STATE_REASONS */
    size_t	psrlen;			/* Size of PRINTER_STATE_REASONS */

    for (psrlen = 22, i = 0; i < job->printer->num_reasons; i ++)
      psrlen += strlen(job->printer->reasons[i]) + 1;

    if ((printer_state_reasons = malloc(psrlen)) != NULL)
    {
     /*
      * All of these strcpy's are safe because we allocated the psr string...
      */

      strlcpy(printer_state_reasons, "PRINTER_STATE_REASONS=", psrlen);
      for (psrptr = printer_state_reasons + 22, i = 0;
           i < job->printer->num_reasons;
	   i ++)
      {
        if (i)
	  *psrptr++ = ',';
	strlcpy(psrptr, job->printer->reasons[i], psrlen - (size_t)(psrptr - printer_state_reasons));
	psrptr += strlen(psrptr);
      }
    }
  }
  snprintf(rip_max_cache, sizeof(rip_max_cache), "RIP_MAX_CACHE=%s", RIPCache);

  if (job->printer->num_auth_info_required == 1)
    snprintf(auth_info_required, sizeof(auth_info_required),
             "AUTH_INFO_REQUIRED=%s",
	     job->printer->auth_info_required[0]);
  else if (job->printer->num_auth_info_required == 2)
    snprintf(auth_info_required, sizeof(auth_info_required),
             "AUTH_INFO_REQUIRED=%s,%s",
	     job->printer->auth_info_required[0],
	     job->printer->auth_info_required[1]);
  else if (job->printer->num_auth_info_required == 3)
    snprintf(auth_info_required, sizeof(auth_info_required),
             "AUTH_INFO_REQUIRED=%s,%s,%s",
	     job->printer->auth_info_required[0],
	     job->printer->auth_info_required[1],
	     job->printer->auth_info_required[2]);
  else if (job->printer->num_auth_info_required == 4)
    snprintf(auth_info_required, sizeof(auth_info_required),
             "AUTH_INFO_REQUIRED=%s,%s,%s,%s",
	     job->printer->auth_info_required[0],
	     job->printer->auth_info_required[1],
	     job->printer->auth_info_required[2],
	     job->printer->auth_info_required[3]);
  else
    strlcpy(auth_info_required, "AUTH_INFO_REQUIRED=none",
	    sizeof(auth_info_required));

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
  envp[envc ++] = printer_state_reasons ? printer_state_reasons :
                                          "PRINTER_STATE_REASONS=none";
  envp[envc ++] = banner_page ? "CUPS_FILETYPE=job-sheet" :
                                "CUPS_FILETYPE=document";

  if (final_content_type[0])
    envp[envc ++] = final_content_type;

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

  if (job->dtype & CUPS_PRINTER_CLASS)
  {
    snprintf(class_name, sizeof(class_name), "CLASS=%s", job->dest);
    envp[envc ++] = class_name;
  }

  envp[envc ++] = auth_info_required;

  for (i = 0;
       i < (int)(sizeof(job->auth_env) / sizeof(job->auth_env[0]));
       i ++)
    if (job->auth_env[i])
      envp[envc ++] = job->auth_env[i];
    else
      break;

  if (job->auth_uid)
    envp[envc ++] = job->auth_uid;

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
      if (job->current_file == 1 ||
          (job->printer->pc && job->printer->pc->single_file))
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

    if (argv[6])
    {
      free(argv[6]);
      argv[6] = NULL;
    }

    slot = !slot;
  }

  cupsArrayDelete(filters);
  filters = NULL;

 /*
  * Finally, pipe the final output into a backend process if needed...
  */

  if (strncmp(job->printer->device_uri, "file:", 5) != 0)
  {
    if (job->current_file == 1 || job->printer->remote ||
        (job->printer->pc && job->printer->pc->single_file))
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
        backroot = !(backinfo.st_mode & (S_IWGRP | S_IRWXO));

      argv[0] = job->printer->sanitized_device_uri;

      filterfds[slot][0] = -1;
      filterfds[slot][1] = -1;

      pid = cupsdStartProcess(command, argv, envp, filterfds[!slot][0],
			      filterfds[slot][1], job->status_pipes[1],
			      job->back_pipes[1], job->side_pipes[1],
			      backroot, job->bprofile, job, &(job->backend));

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

    if (job->current_file == job->num_files ||
        (job->printer->pc && job->printer->pc->single_file))
      cupsdClosePipe(job->print_pipes);

    if (job->current_file == job->num_files)
    {
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

    if (job->current_file == job->num_files ||
        (job->printer->pc && job->printer->pc->single_file))
      cupsdClosePipe(job->print_pipes);

    if (job->current_file == job->num_files)
    {
      close(job->status_pipes[1]);
      job->status_pipes[1] = -1;
    }
  }

  cupsdClosePipe(filterfds[slot]);

  for (i = 6; i < argc; i ++)
    if (argv[i])
      free(argv[i]);

  free(argv);

  if (printer_state_reasons)
    free(printer_state_reasons);

  cupsdAddSelect(job->status_buffer->fd, (cupsd_selfunc_t)update_job, NULL,
                 job);

  cupsdAddEvent(CUPSD_EVENT_JOB_STATE, job->printer, job, "Job #%d started.",
                job->id);

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
    for (i = 6; i < argc; i ++)
      if (argv[i])
	free(argv[i]);
  }

  if (printer_state_reasons)
    free(printer_state_reasons);

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
  int	i;				/* Looping var */


  if (job->printer)
    finalize_job(job, 1);

  if (action == CUPSD_JOB_PURGE)
    remove_job_history(job);

  cupsdClearString(&job->username);
  cupsdClearString(&job->dest);
  for (i = 0;
       i < (int)(sizeof(job->auth_env) / sizeof(job->auth_env[0]));
       i ++)
    cupsdClearString(job->auth_env + i);
  cupsdClearString(&job->auth_uid);

  if (action == CUPSD_JOB_PURGE)
    remove_job_files(job);
  else if (job->num_files > 0)
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
 * 'cupsdGetCompletedJobs()'- Generate a completed jobs list.
 */

cups_array_t *				/* O - Array of jobs */
cupsdGetCompletedJobs(
    cupsd_printer_t *p)			/* I - Printer */
{
  cups_array_t	*list;			/* Array of jobs */
  cupsd_job_t	*job;			/* Current job */


  list = cupsArrayNew(compare_completed_jobs, NULL);

  for (job = (cupsd_job_t *)cupsArrayFirst(Jobs);
       job;
       job = (cupsd_job_t *)cupsArrayNext(Jobs))
    if ((!p || !_cups_strcasecmp(p->name, job->dest)) && job->state_value >= IPP_JOB_STOPPED && job->completed_time)
      cupsArrayAdd(list, job);

  return (list);
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
    if (job->dest && !_cups_strcasecmp(job->dest, dest))
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
    if (!_cups_strcasecmp(job->username, username))
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
  struct stat	fileinfo;		/* Information on job.cache file */
  cups_dir_t	*dir;			/* RequestRoot dir */
  cups_dentry_t	*dent;			/* Entry in RequestRoot */
  int		load_cache = 1;		/* Load the job.cache file? */


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
   /*
    * No job.cache file...
    */

    load_cache = 0;

    if (errno != ENOENT)
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to get file information for \"%s\" - %s",
		      filename, strerror(errno));
  }
  else if ((dir = cupsDirOpen(RequestRoot)) == NULL)
  {
   /*
    * No spool directory...
    */

    load_cache = 0;
  }
  else
  {
    while ((dent = cupsDirRead(dir)) != NULL)
    {
      if (strlen(dent->filename) >= 6 && dent->filename[0] == 'c' && dent->fileinfo.st_mtime > fileinfo.st_mtime)
      {
       /*
        * Job history file is newer than job.cache file...
	*/

        load_cache = 0;
	break;
      }
    }

    cupsDirClose(dir);
  }

 /*
  * Load the most recent source for job data...
  */

  if (load_cache)
  {
   /*
    * Load the job.cache file...
    */

    load_job_cache(filename);
  }
  else
  {
   /*
    * Load the job history files...
    */

    load_request_root();

    load_next_job_id(filename);
  }

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
  int			i;		/* Looping var */
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
    cupsdLogJob(job, CUPSD_LOG_ERROR, "Ran out of memory for job attributes.");
    return (0);
  }

 /*
  * Load job attributes...
  */

  cupsdLogJob(job, CUPSD_LOG_DEBUG, "Loading attributes...");

  snprintf(jobfile, sizeof(jobfile), "%s/c%05d", RequestRoot, job->id);
  if ((fp = cupsdOpenConfFile(jobfile)) == NULL)
    goto error;

  if (ippReadIO(fp, (ipp_iocb_t)cupsFileRead, 1, NULL, job->attrs) != IPP_DATA)
  {
    cupsdLogJob(job, CUPSD_LOG_ERROR,
		"Unable to read job control file \"%s\".", jobfile);
    cupsFileClose(fp);
    goto error;
  }

  cupsFileClose(fp);

 /*
  * Copy attribute data to the job object...
  */

  if (!ippFindAttribute(job->attrs, "time-at-creation", IPP_TAG_INTEGER))
  {
    cupsdLogJob(job, CUPSD_LOG_ERROR,
		"Missing or bad time-at-creation attribute in control file.");
    goto error;
  }

  if ((job->state = ippFindAttribute(job->attrs, "job-state",
                                     IPP_TAG_ENUM)) == NULL)
  {
    cupsdLogJob(job, CUPSD_LOG_ERROR,
		"Missing or bad job-state attribute in control file.");
    goto error;
  }

  job->state_value  = (ipp_jstate_t)job->state->values[0].integer;
  job->file_time    = 0;
  job->history_time = 0;

  if ((attr = ippFindAttribute(job->attrs, "time-at-creation", IPP_TAG_INTEGER)) != NULL)
    job->creation_time = attr->values[0].integer;

  if (job->state_value >= IPP_JOB_CANCELED && (attr = ippFindAttribute(job->attrs, "time-at-completed", IPP_TAG_INTEGER)) != NULL)
  {
    job->completed_time = attr->values[0].integer;

    if (JobHistory < INT_MAX)
      job->history_time = attr->values[0].integer + JobHistory;
    else
      job->history_time = INT_MAX;

    if (job->history_time < time(NULL))
      goto error;			/* Expired, remove from history */

    if (job->history_time < JobHistoryUpdate || !JobHistoryUpdate)
      JobHistoryUpdate = job->history_time;

    if (JobFiles < INT_MAX)
      job->file_time = attr->values[0].integer + JobFiles;
    else
      job->file_time = INT_MAX;

    if (job->file_time < JobHistoryUpdate || !JobHistoryUpdate)
      JobHistoryUpdate = job->file_time;

    cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdLoadJob: JobHistoryUpdate=%ld",
		    (long)JobHistoryUpdate);
  }

  if (!job->dest)
  {
    if ((attr = ippFindAttribute(job->attrs, "job-printer-uri",
                                 IPP_TAG_URI)) == NULL)
    {
      cupsdLogJob(job, CUPSD_LOG_ERROR,
		  "No job-printer-uri attribute in control file.");
      goto error;
    }

    if ((dest = cupsdValidateDest(attr->values[0].string.text, &(job->dtype),
                                  &destptr)) == NULL)
    {
      cupsdLogJob(job, CUPSD_LOG_ERROR,
		  "Unable to queue job for destination \"%s\".",
		  attr->values[0].string.text);
      goto error;
    }

    cupsdSetString(&job->dest, dest);
  }
  else if ((destptr = cupsdFindDest(job->dest)) == NULL)
  {
    cupsdLogJob(job, CUPSD_LOG_ERROR,
		"Unable to queue job for destination \"%s\".",
		job->dest);
    goto error;
  }

  if ((job->reasons = ippFindAttribute(job->attrs, "job-state-reasons",
                                       IPP_TAG_KEYWORD)) == NULL)
  {
    const char	*reason;		/* job-state-reason keyword */

    cupsdLogJob(job, CUPSD_LOG_DEBUG,
		"Adding missing job-state-reasons attribute to  control file.");

    switch (job->state_value)
    {
      default :
      case IPP_JOB_PENDING :
          if (destptr->state == IPP_PRINTER_STOPPED)
            reason = "printer-stopped";
          else
            reason = "none";
          break;

      case IPP_JOB_HELD :
          if ((attr = ippFindAttribute(job->attrs, "job-hold-until",
                                       IPP_TAG_ZERO)) != NULL &&
              (attr->value_tag == IPP_TAG_NAME ||
	       attr->value_tag == IPP_TAG_NAMELANG ||
	       attr->value_tag == IPP_TAG_KEYWORD) &&
	      strcmp(attr->values[0].string.text, "no-hold"))
	    reason = "job-hold-until-specified";
	  else
	    reason = "job-incoming";
          break;

      case IPP_JOB_PROCESSING :
          reason = "job-printing";
          break;

      case IPP_JOB_STOPPED :
          reason = "job-stopped";
          break;

      case IPP_JOB_CANCELED :
          reason = "job-canceled-by-user";
          break;

      case IPP_JOB_ABORTED :
          reason = "aborted-by-system";
          break;

      case IPP_JOB_COMPLETED :
          reason = "job-completed-successfully";
          break;
    }

    job->reasons = ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_KEYWORD,
                                "job-state-reasons", NULL, reason);
  }
  else if (job->state_value == IPP_JOB_PENDING)
  {
    if (destptr->state == IPP_PRINTER_STOPPED)
      ippSetString(job->attrs, &job->reasons, 0, "printer-stopped");
    else
      ippSetString(job->attrs, &job->reasons, 0, "none");
  }

  job->impressions = ippFindAttribute(job->attrs, "job-impressions-completed", IPP_TAG_INTEGER);
  job->sheets      = ippFindAttribute(job->attrs, "job-media-sheets-completed", IPP_TAG_INTEGER);
  job->job_sheets  = ippFindAttribute(job->attrs, "job-sheets", IPP_TAG_NAME);

  if (!job->impressions)
    job->impressions = ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-impressions-completed", 0);

  if (!job->priority)
  {
    if ((attr = ippFindAttribute(job->attrs, "job-priority",
                        	 IPP_TAG_INTEGER)) == NULL)
    {
      cupsdLogJob(job, CUPSD_LOG_ERROR,
		  "Missing or bad job-priority attribute in control file.");
      goto error;
    }

    job->priority = attr->values[0].integer;
  }

  if (!job->username)
  {
    if ((attr = ippFindAttribute(job->attrs, "job-originating-user-name",
                        	 IPP_TAG_NAME)) == NULL)
    {
      cupsdLogJob(job, CUPSD_LOG_ERROR,
		  "Missing or bad job-originating-user-name "
		  "attribute in control file.");
      goto error;
    }

    cupsdSetString(&job->username, attr->values[0].string.text);
  }

  if (!job->name)
  {
    if ((attr = ippFindAttribute(job->attrs, "job-name", IPP_TAG_NAME)) != NULL)
      cupsdSetString(&job->name, attr->values[0].string.text);
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

  if ((attr = ippFindAttribute(job->attrs, "job-k-octets", IPP_TAG_INTEGER)) != NULL)
    job->koctets = attr->values[0].integer;

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

      cupsdLogJob(job, CUPSD_LOG_DEBUG,
		  "Auto-typing document file \"%s\"...", jobfile);

      if (fileid > job->num_files)
      {
        if (job->num_files == 0)
	{
	  compressions = (int *)calloc((size_t)fileid, sizeof(int));
	  filetypes    = (mime_type_t **)calloc((size_t)fileid, sizeof(mime_type_t *));
	}
	else
	{
	  compressions = (int *)realloc(job->compressions, sizeof(int) * (size_t)fileid);
	  filetypes    = (mime_type_t **)realloc(job->filetypes, sizeof(mime_type_t *) * (size_t)fileid);
        }

	if (compressions)
	  job->compressions = compressions;

	if (filetypes)
	  job->filetypes = filetypes;

        if (!compressions || !filetypes)
	{
          cupsdLogJob(job, CUPSD_LOG_ERROR,
		      "Ran out of memory for job file types.");

	  ippDelete(job->attrs);
	  job->attrs = NULL;

	  if (job->compressions)
	  {
	    free(job->compressions);
	    job->compressions = NULL;
	  }

	  if (job->filetypes)
	  {
	    free(job->filetypes);
	    job->filetypes = NULL;
	  }

	  job->num_files = 0;
	  return (0);
	}

	job->num_files = fileid;
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

    for (i = 0;
	 i < (int)(sizeof(job->auth_env) / sizeof(job->auth_env[0]));
	 i ++)
      cupsdClearString(job->auth_env + i);
    cupsdClearString(&job->auth_uid);

    if ((fp = cupsFileOpen(jobfile, "r")) != NULL)
    {
      int	bytes,			/* Size of auth data */
		linenum = 1;		/* Current line number */
      char	line[65536],		/* Line from file */
		*value,			/* Value from line */
		data[65536];		/* Decoded data */


      if (cupsFileGets(fp, line, sizeof(line)) &&
          !strcmp(line, "CUPSD-AUTH-V3"))
      {
        i = 0;
        while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
        {
         /*
          * Decode value...
          */

          if (strcmp(line, "negotiate") && strcmp(line, "uid"))
          {
	    bytes = sizeof(data);
	    httpDecode64_2(data, &bytes, value);
	  }

         /*
          * Assign environment variables...
          */

          if (!strcmp(line, "uid"))
          {
            cupsdSetStringf(&job->auth_uid, "AUTH_UID=%s", value);
            continue;
          }
          else if (i >= (int)(sizeof(job->auth_env) / sizeof(job->auth_env[0])))
            break;

	  if (!strcmp(line, "username"))
	    cupsdSetStringf(job->auth_env + i, "AUTH_USERNAME=%s", data);
	  else if (!strcmp(line, "domain"))
	    cupsdSetStringf(job->auth_env + i, "AUTH_DOMAIN=%s", data);
	  else if (!strcmp(line, "password"))
	    cupsdSetStringf(job->auth_env + i, "AUTH_PASSWORD=%s", data);
	  else if (!strcmp(line, "negotiate"))
	    cupsdSetStringf(job->auth_env + i, "AUTH_NEGOTIATE=%s", value);
	  else
	    continue;

	  i ++;
	}
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

  remove_job_history(job);
  remove_job_files(job);

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

  if (job->state_value > IPP_JOB_HELD)
    cupsdSetJobState(job, IPP_JOB_PENDING, CUPSD_JOB_DEFAULT,
		     "Stopping job prior to move.");

  cupsdAddEvent(CUPSD_EVENT_JOB_CONFIG_CHANGED, oldp, job,
                "Job #%d moved from %s to %s.", job->id, olddest,
		p->name);

  cupsdSetString(&job->dest, p->name);
  job->dtype = p->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_REMOTE);

  if ((attr = ippFindAttribute(job->attrs, "job-printer-uri",
                               IPP_TAG_URI)) != NULL)
    ippSetString(job->attrs, &attr, 0, p->uri);

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
  cups_file_t	*fp;			/* job.cache file */
  char		filename[1024],		/* job.cache filename */
		temp[1024];		/* Temporary string */
  cupsd_job_t	*job;			/* Current job */
  time_t	curtime;		/* Current time */
  struct tm	*curdate;		/* Current date */


  snprintf(filename, sizeof(filename), "%s/job.cache", CacheDir);
  if ((fp = cupsdCreateConfFile(filename, ConfigFilePerm)) == NULL)
    return;

  cupsdLogMessage(CUPSD_LOG_INFO, "Saving job.cache...");

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
    cupsFilePrintf(fp, "Created %ld\n", (long)job->creation_time);
    if (job->completed_time)
      cupsFilePrintf(fp, "Completed %ld\n", (long)job->completed_time);
    cupsFilePrintf(fp, "Priority %d\n", job->priority);
    if (job->hold_until)
      cupsFilePrintf(fp, "HoldUntil %ld\n", (long)job->hold_until);
    cupsFilePrintf(fp, "Username %s\n", job->username);
    if (job->name)
      cupsFilePutConf(fp, "Name", job->name);
    cupsFilePrintf(fp, "Destination %s\n", job->dest);
    cupsFilePrintf(fp, "DestType %d\n", job->dtype);
    cupsFilePrintf(fp, "KOctets %d\n", job->koctets);
    cupsFilePrintf(fp, "NumFiles %d\n", job->num_files);
    for (i = 0; i < job->num_files; i ++)
      cupsFilePrintf(fp, "File %d %s/%s %d\n", i + 1, job->filetypes[i]->super,
                     job->filetypes[i]->type, job->compressions[i]);
    cupsFilePuts(fp, "</Job>\n");
  }

  cupsdCloseCreatedConfFile(fp, filename);
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

  if ((fp = cupsdCreateConfFile(filename, ConfigFilePerm & 0600)) == NULL)
    return;

  fchown(cupsFileNumber(fp), RunUser, Group);

  job->attrs->state = IPP_IDLE;

  if (ippWriteIO(fp, (ipp_iocb_t)cupsFileWrite, 1, NULL,
                 job->attrs) != IPP_DATA)
  {
    cupsdLogJob(job, CUPSD_LOG_ERROR, "Unable to write job control file.");
    cupsFileClose(fp);
    return;
  }

  if (!cupsdCloseCreatedConfFile(fp, filename))
  {
   /*
    * Remove backup file and mark this job as clean...
    */

    strlcat(filename, ".O", sizeof(filename));
    unlink(filename);

    job->dirty = 0;
  }
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
      ippSetString(job->attrs, &attr, 0, when);
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

  if (strcmp(when, "no-hold"))
    ippSetString(job->attrs, &job->reasons, 0, "job-hold-until-specified");
  else
    ippSetString(job->attrs, &job->reasons, 0, "none");

 /*
  * Update the hold time...
  */

  job->cancel_time = 0;

  if (!strcmp(when, "indefinite") || !strcmp(when, "auth-info-required"))
  {
   /*
    * Hold indefinitely...
    */

    job->hold_until = 0;

    if (MaxHoldTime > 0)
      job->cancel_time = time(NULL) + MaxHoldTime;
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
  * Don't do anything if the state is unchanged and we aren't purging the
  * job...
  */

  oldstate = job->state_value;
  if (newstate == oldstate && action != CUPSD_JOB_PURGE)
    return;

 /*
  * Stop any processes that are working on the current job...
  */

  if (oldstate == IPP_JOB_PROCESSING)
    stop_job(job, action);

 /*
  * Set the new job state...
  */

  job->state_value = newstate;

  if (job->state)
    job->state->values[0].integer = newstate;

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
	  ippSetValueTag(job->attrs, &attr, IPP_TAG_KEYWORD);
	  ippSetString(job->attrs, &attr, 0, "no-hold");
	}

    default :
	break;

    case IPP_JOB_ABORTED :
    case IPP_JOB_CANCELED :
    case IPP_JOB_COMPLETED :
	set_time(job, "time-at-completed");
	ippSetString(job->attrs, &job->reasons, 0, "processing-to-stop-point");
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
        if (newstate == IPP_JOB_CANCELED)
	{
	 /*
	  * Remove the job from the active list if there are no processes still
	  * running for it...
	  */

	  for (i = 0; job->filters[i] < 0; i++);

	  if (!job->filters[i] && job->backend <= 0)
	    cupsArrayRemove(ActiveJobs, job);
	}
	else
	{
	 /*
	  * Otherwise just remove the job from the active list immediately...
	  */

	  cupsArrayRemove(ActiveJobs, job);
	}

       /*
        * Expire job subscriptions since the job is now "completed"...
	*/

        cupsdExpireSubscriptions(NULL, job);

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

	for (i = 0;
	     i < (int)(sizeof(job->auth_env) / sizeof(job->auth_env[0]));
	     i ++)
	  cupsdClearString(job->auth_env + i);

	cupsdClearString(&job->auth_uid);

       /*
	* Remove the print file for good if we aren't preserving jobs or
	* files...
	*/

	if (!JobHistory || !JobFiles || action == CUPSD_JOB_PURGE)
	  remove_job_files(job);

	if (JobHistory && action != CUPSD_JOB_PURGE)
	{
	 /*
	  * Save job state info...
	  */

	  job->dirty = 1;
	  cupsdMarkDirty(CUPSD_DIRTY_JOBS);
	}
	else if (!job->printer)
	{
	 /*
	  * Delete the job immediately if not actively printing...
	  */

	  cupsdDeleteJob(job, CUPSD_JOB_PURGE);
	  job = NULL;
	}
	break;
  }

 /*
  * Finalize the job immediately if we forced things...
  */

  if (action >= CUPSD_JOB_FORCE && job && job->printer)
    finalize_job(job, 0);

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
    if (job->completed)
    {
      cupsdSetJobState(job, IPP_JOB_COMPLETED, CUPSD_JOB_FORCE, NULL);
    }
    else
    {
      if (kill_delay)
        job->kill_time = time(NULL) + kill_delay;

      cupsdSetJobState(job, IPP_JOB_PENDING, action, NULL);
    }
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
 * 'cupsdUpdateJobs()' - Update the history/file files for all jobs.
 */

void
cupsdUpdateJobs(void)
{
  cupsd_job_t		*job;		/* Current job */
  time_t		curtime;	/* Current time */
  ipp_attribute_t	*attr;		/* time-at-completed attribute */


  curtime          = time(NULL);
  JobHistoryUpdate = 0;

  for (job = (cupsd_job_t *)cupsArrayFirst(Jobs);
       job;
       job = (cupsd_job_t *)cupsArrayNext(Jobs))
  {
    if (job->state_value >= IPP_JOB_CANCELED &&
        (attr = ippFindAttribute(job->attrs, "time-at-completed",
                                 IPP_TAG_INTEGER)) != NULL)
    {
     /*
      * Update history/file expiration times...
      */

      if (JobHistory < INT_MAX)
	job->history_time = attr->values[0].integer + JobHistory;
      else
	job->history_time = INT_MAX;

      if (job->history_time < curtime)
      {
        cupsdDeleteJob(job, CUPSD_JOB_PURGE);
        continue;
      }

      if (job->history_time < JobHistoryUpdate || !JobHistoryUpdate)
	JobHistoryUpdate = job->history_time;

      if (JobFiles < INT_MAX)
	job->file_time = attr->values[0].integer + JobFiles;
      else
	job->file_time = INT_MAX;

      if (job->file_time < JobHistoryUpdate || !JobHistoryUpdate)
	JobHistoryUpdate = job->file_time;
    }
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdUpdateAllJobs: JobHistoryUpdate=%ld",
                  (long)JobHistoryUpdate);
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


  (void)data;

  if ((diff = ((cupsd_job_t *)second)->priority -
              ((cupsd_job_t *)first)->priority) != 0)
    return (diff);
  else
    return (((cupsd_job_t *)first)->id - ((cupsd_job_t *)second)->id);
}


/*
 * 'compare_completed_jobs()' - Compare the job IDs and completion times of two jobs.
 */

static int				/* O - Difference */
compare_completed_jobs(void *first,	/* I - First job */
                       void *second,	/* I - Second job */
		       void *data)	/* I - App data (not used) */
{
  int	diff;				/* Difference */


  (void)data;

  if ((diff = ((cupsd_job_t *)second)->completed_time -
              ((cupsd_job_t *)first)->completed_time) != 0)
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
  (void)data;

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
      strlcpy(ptr, "none", sizeof(temp) - (size_t)(ptr - temp));
    else
    {
      for (i = 0;
           i < printer->num_reasons && ptr < (temp + sizeof(temp) - 2);
           i ++)
      {
        if (i)
	  *ptr++ = ',';

	strlcpy(ptr, printer->reasons[i], sizeof(temp) - (size_t)(ptr - temp));
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
finalize_job(cupsd_job_t *job,		/* I - Job */
             int         set_job_state)	/* I - 1 = set the job state */
{
  ipp_pstate_t		printer_state;	/* New printer state value */
  ipp_jstate_t		job_state;	/* New job state value */
  const char		*message;	/* Message for job state */
  char			buffer[1024];	/* Buffer for formatted messages */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "finalize_job(job=%p(%d))", job, job->id);

 /*
  * Clear the "connecting-to-device" and "cups-waiting-for-job-completed"
  * reasons, which are only valid when a printer is processing, along with any
  * remote printing job state...
  */

  cupsdSetPrinterReasons(job->printer, "-connecting-to-device,"
                                       "cups-waiting-for-job-completed,"
				       "cups-remote-pending,"
				       "cups-remote-pending-held,"
				       "cups-remote-processing,"
				       "cups-remote-stopped,"
				       "cups-remote-canceled,"
				       "cups-remote-aborted,"
				       "cups-remote-completed");

 /*
  * Similarly, clear the "offline-report" reason for non-USB devices since we
  * rarely have current information for network devices...
  */

  if (strncmp(job->printer->device_uri, "usb:", 4) &&
      strncmp(job->printer->device_uri, "ippusb:", 7))
    cupsdSetPrinterReasons(job->printer, "-offline-report");

 /*
  * Free the security profile...
  */

  cupsdDestroyProfile(job->profile);
  job->profile = NULL;
  cupsdDestroyProfile(job->bprofile);
  job->bprofile = NULL;

 /*
  * Clear the unresponsive job watchdog timers...
  */

  job->cancel_time = 0;
  job->kill_time   = 0;

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

        if (!job->status)
	  ippSetString(job->attrs, &job->reasons, 0,
		       "job-completed-successfully");
        break;

    case IPP_JOB_STOPPED :
        message = "Job stopped.";

	ippSetString(job->attrs, &job->reasons, 0, "job-stopped");
	break;

    case IPP_JOB_CANCELED :
        message = "Job canceled.";

	ippSetString(job->attrs, &job->reasons, 0, "job-canceled-by-user");
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
    {
      ippSetString(job->attrs, &job->reasons, 0, "cups-backend-crashed");
      exit_code = job->status;
    }

    cupsdLogJob(job, CUPSD_LOG_INFO, "Backend returned status %d (%s)",
		exit_code,
		exit_code == CUPS_BACKEND_FAILED ? "failed" :
		    exit_code == CUPS_BACKEND_AUTH_REQUIRED ?
			"authentication required" :
		    exit_code == CUPS_BACKEND_HOLD ? "hold job" :
		    exit_code == CUPS_BACKEND_STOP ? "stop printer" :
		    exit_code == CUPS_BACKEND_CANCEL ? "cancel job" :
		    exit_code == CUPS_BACKEND_RETRY ? "retry job later" :
		    exit_code == CUPS_BACKEND_RETRY_CURRENT ? "retry job immediately" :
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

          if (job->dtype & CUPS_PRINTER_CLASS)
	  {
	   /*
	    * Queued on a class - mark the job as pending and we'll retry on
	    * another printer...
	    */

            if (job_state == IPP_JOB_COMPLETED)
	    {
	      job_state = IPP_JOB_PENDING;
	      message   = "Retrying job on another printer.";

	      ippSetString(job->attrs, &job->reasons, 0,
	                   "resources-are-not-ready");
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

	      ippSetString(job->attrs, &job->reasons, 0, "none");
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

	      if (job->tries > JobRetryLimit && JobRetryLimit > 0)
	      {
	       /*
		* Too many tries...
		*/

		snprintf(buffer, sizeof(buffer),
			 "Job aborted after %d unsuccessful attempts.",
			 JobRetryLimit);
		job_state = IPP_JOB_ABORTED;
		message   = buffer;

		ippSetString(job->attrs, &job->reasons, 0, "aborted-by-system");
	      }
	      else
	      {
	       /*
		* Try again in N seconds...
		*/

		snprintf(buffer, sizeof(buffer),
			 "Job held for %d seconds since it could not be sent.",
			 JobRetryInterval);

		job->hold_until = time(NULL) + JobRetryInterval;
		job_state       = IPP_JOB_HELD;
		message         = buffer;

		ippSetString(job->attrs, &job->reasons, 0,
		             "resources-are-not-ready");
	      }
            }
	  }
	  else if (!strcmp(job->printer->error_policy, "abort-job") &&
	           job_state == IPP_JOB_COMPLETED)
	  {
	    job_state = IPP_JOB_ABORTED;
	    message   = "Job aborted due to backend errors; please consult "
	                "the error_log file for details.";

	    ippSetString(job->attrs, &job->reasons, 0, "aborted-by-system");
	  }
	  else if (job->state_value == IPP_JOB_PROCESSING)
          {
            job_state     = IPP_JOB_PENDING;
	    printer_state = IPP_PRINTER_STOPPED;
	    message       = "Printer stopped due to backend errors; please "
			    "consult the error_log file for details.";

	    ippSetString(job->attrs, &job->reasons, 0, "none");
	  }
          break;

      case CUPS_BACKEND_CANCEL :
         /*
	  * Cancel the job...
	  */

	  if (job_state == IPP_JOB_COMPLETED)
	  {
	    job_state = IPP_JOB_CANCELED;
	    message   = "Job canceled at printer.";

	    ippSetString(job->attrs, &job->reasons, 0, "canceled-at-device");
	  }
          break;

      case CUPS_BACKEND_HOLD :
	  if (job_state == IPP_JOB_COMPLETED)
	  {
	   /*
	    * Hold the job...
	    */

	    const char *reason = ippGetString(job->reasons, 0, NULL);

	    cupsdLogJob(job, CUPSD_LOG_DEBUG, "job-state-reasons=\"%s\"",
	                reason);

	    if (!reason || strncmp(reason, "account-", 8))
	    {
	      cupsdSetJobHoldUntil(job, "indefinite", 1);

	      ippSetString(job->attrs, &job->reasons, 0,
			   "job-hold-until-specified");
	      message = "Job held indefinitely due to backend errors; please "
			"consult the error_log file for details.";
            }
            else if (!strcmp(reason, "account-info-needed"))
            {
	      cupsdSetJobHoldUntil(job, "indefinite", 0);

	      message = "Job held indefinitely - account information is "
	                "required.";
            }
            else if (!strcmp(reason, "account-closed"))
            {
	      cupsdSetJobHoldUntil(job, "indefinite", 0);

	      message = "Job held indefinitely - account has been closed.";
	    }
            else if (!strcmp(reason, "account-limit-reached"))
            {
	      cupsdSetJobHoldUntil(job, "indefinite", 0);

	      message = "Job held indefinitely - account limit has been "
	                "reached.";
	    }
            else
            {
	      cupsdSetJobHoldUntil(job, "indefinite", 0);

	      message = "Job held indefinitely - account authorization failed.";
	    }

	    job_state = IPP_JOB_HELD;
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
	  {
	    job_state = IPP_JOB_PENDING;

	    ippSetString(job->attrs, &job->reasons, 0,
	                 "resources-are-not-ready");
	  }
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

            if (strncmp(job->reasons->values[0].string.text, "account-", 8))
	      ippSetString(job->attrs, &job->reasons, 0,
			   "cups-held-for-authentication");
          }
          break;

      case CUPS_BACKEND_RETRY :
	  if (job_state == IPP_JOB_COMPLETED)
	  {
	   /*
	    * Hold the job if the number of retries is less than the
	    * JobRetryLimit, otherwise abort the job.
	    */

	    job->tries ++;

	    if (job->tries > JobRetryLimit && JobRetryLimit > 0)
	    {
	     /*
	      * Too many tries...
	      */

	      snprintf(buffer, sizeof(buffer),
		       "Job aborted after %d unsuccessful attempts.",
		       JobRetryLimit);
	      job_state = IPP_JOB_ABORTED;
	      message   = buffer;

	      ippSetString(job->attrs, &job->reasons, 0, "aborted-by-system");
	    }
	    else
	    {
	     /*
	      * Try again in N seconds...
	      */

	      snprintf(buffer, sizeof(buffer),
		       "Job held for %d seconds since it could not be sent.",
		       JobRetryInterval);

	      job->hold_until = time(NULL) + JobRetryInterval;
	      job_state       = IPP_JOB_HELD;
	      message         = buffer;

	      ippSetString(job->attrs, &job->reasons, 0,
	                   "resources-are-not-ready");
	    }
	  }
          break;

      case CUPS_BACKEND_RETRY_CURRENT :
	 /*
	  * Mark the job as pending and retry on the same printer...
	  */

	  if (job_state == IPP_JOB_COMPLETED)
	  {
	    job_state = IPP_JOB_PENDING;
	    message   = "Retrying job on same printer.";

	    ippSetString(job->attrs, &job->reasons, 0, "none");
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

      if (WIFSIGNALED(job->status))
	ippSetString(job->attrs, &job->reasons, 0, "cups-filter-crashed");
      else
	ippSetString(job->attrs, &job->reasons, 0, "job-completed-with-errors");
    }
  }

 /*
  * Update the printer and job state.
  */

  if (set_job_state && job_state != job->state_value)
    cupsdSetJobState(job, job_state, CUPSD_JOB_DEFAULT, "%s", message);

  cupsdSetPrinterState(job->printer, printer_state,
                       printer_state == IPP_PRINTER_STOPPED);
  update_job_attrs(job, 0);

  if (job->history)
  {
    if (job->status &&
        (job->state_value == IPP_JOB_ABORTED ||
         job->state_value == IPP_JOB_STOPPED))
      dump_job_history(job);
    else
      free_job_history(job);
  }

  cupsArrayRemove(PrintingJobs, job);

 /*
  * Apply any PPD updates...
  */

  if (job->num_keywords)
  {
    if (cupsdUpdatePrinterPPD(job->printer, job->num_keywords, job->keywords))
      cupsdSetPrinterAttrs(job->printer);

    cupsFreeOptions(job->num_keywords, job->keywords);

    job->num_keywords = 0;
    job->keywords     = NULL;
  }

 /*
  * Clear the printer <-> job association...
  */

  job->printer->job = NULL;
  job->printer      = NULL;
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
  size_t		newlength;	/* New option buffer length */
  char			*optptr,	/* Pointer to options */
			*valptr;	/* Pointer in value string */
  ipp_attribute_t	*attr;		/* Current attribute */
  _ppd_cache_t		*pc;		/* PPD cache and mapping data */
  int			num_pwgppds;	/* Number of PWG->PPD options */
  cups_option_t		*pwgppds,	/* PWG->PPD options */
			*pwgppd,	/* Current PWG->PPD option */
			*preset;	/* Current preset option */
  int			print_color_mode,
					/* Output mode (if any) */
			print_quality;	/* Print quality (if any) */
  const char		*ppd;		/* PPD option choice */
  int			exact;		/* Did we get an exact match? */
  static char		*options = NULL;/* Full list of options */
  static size_t		optlength = 0;	/* Length of option buffer */


 /*
  * Building the options string is harder than it needs to be, but for the
  * moment we need to pass strings for command-line args and not IPP attribute
  * pointers... :)
  *
  * First build an options array for any PWG->PPD mapped option/choice pairs.
  */

  pc          = job->printer->pc;
  num_pwgppds = 0;
  pwgppds     = NULL;

  if (pc &&
      !ippFindAttribute(job->attrs,
                        "com.apple.print.DocumentTicket.PMSpoolFormat",
			IPP_TAG_ZERO) &&
      !ippFindAttribute(job->attrs, "APPrinterPreset", IPP_TAG_ZERO) &&
      (ippFindAttribute(job->attrs, "print-color-mode", IPP_TAG_ZERO) ||
       ippFindAttribute(job->attrs, "print-quality", IPP_TAG_ZERO)))
  {
   /*
    * Map print-color-mode and print-quality to a preset...
    */

    if ((attr = ippFindAttribute(job->attrs, "print-color-mode",
				 IPP_TAG_KEYWORD)) != NULL &&
        !strcmp(attr->values[0].string.text, "monochrome"))
      print_color_mode = _PWG_PRINT_COLOR_MODE_MONOCHROME;
    else
      print_color_mode = _PWG_PRINT_COLOR_MODE_COLOR;

    if ((attr = ippFindAttribute(job->attrs, "print-quality",
				 IPP_TAG_ENUM)) != NULL &&
	attr->values[0].integer >= IPP_QUALITY_DRAFT &&
	attr->values[0].integer <= IPP_QUALITY_HIGH)
      print_quality = attr->values[0].integer - IPP_QUALITY_DRAFT;
    else
      print_quality = _PWG_PRINT_QUALITY_NORMAL;

    if (pc->num_presets[print_color_mode][print_quality] == 0)
    {
     /*
      * Try to find a preset that works so that we maximize the chances of us
      * getting a good print using IPP attributes.
      */

      if (pc->num_presets[print_color_mode][_PWG_PRINT_QUALITY_NORMAL] > 0)
        print_quality = _PWG_PRINT_QUALITY_NORMAL;
      else if (pc->num_presets[_PWG_PRINT_COLOR_MODE_COLOR][print_quality] > 0)
        print_color_mode = _PWG_PRINT_COLOR_MODE_COLOR;
      else
      {
        print_quality    = _PWG_PRINT_QUALITY_NORMAL;
        print_color_mode = _PWG_PRINT_COLOR_MODE_COLOR;
      }
    }

    if (pc->num_presets[print_color_mode][print_quality] > 0)
    {
     /*
      * Copy the preset options as long as the corresponding names are not
      * already defined in the IPP request...
      */

      for (i = pc->num_presets[print_color_mode][print_quality],
	       preset = pc->presets[print_color_mode][print_quality];
	   i > 0;
	   i --, preset ++)
      {
        if (!ippFindAttribute(job->attrs, preset->name, IPP_TAG_ZERO))
	  num_pwgppds = cupsAddOption(preset->name, preset->value, num_pwgppds,
	                              &pwgppds);
      }
    }
  }

  if (pc)
  {
    if (!ippFindAttribute(job->attrs, "InputSlot", IPP_TAG_ZERO) &&
	!ippFindAttribute(job->attrs, "HPPaperSource", IPP_TAG_ZERO))
    {
      if ((ppd = _ppdCacheGetInputSlot(pc, job->attrs, NULL)) != NULL)
	num_pwgppds = cupsAddOption(pc->source_option, ppd, num_pwgppds,
				    &pwgppds);
    }
    if (!ippFindAttribute(job->attrs, "MediaType", IPP_TAG_ZERO) &&
	(ppd = _ppdCacheGetMediaType(pc, job->attrs, NULL)) != NULL)
      num_pwgppds = cupsAddOption("MediaType", ppd, num_pwgppds, &pwgppds);

    if (!ippFindAttribute(job->attrs, "PageRegion", IPP_TAG_ZERO) &&
	!ippFindAttribute(job->attrs, "PageSize", IPP_TAG_ZERO) &&
	(ppd = _ppdCacheGetPageSize(pc, job->attrs, NULL, &exact)) != NULL)
    {
      num_pwgppds = cupsAddOption("PageSize", ppd, num_pwgppds, &pwgppds);

      if (!ippFindAttribute(job->attrs, "media", IPP_TAG_ZERO))
        num_pwgppds = cupsAddOption("media", ppd, num_pwgppds, &pwgppds);
    }

    if (!ippFindAttribute(job->attrs, "OutputBin", IPP_TAG_ZERO) &&
	(attr = ippFindAttribute(job->attrs, "output-bin",
				 IPP_TAG_ZERO)) != NULL &&
	(attr->value_tag == IPP_TAG_KEYWORD ||
	 attr->value_tag == IPP_TAG_NAME) &&
	(ppd = _ppdCacheGetOutputBin(pc, attr->values[0].string.text)) != NULL)
    {
     /*
      * Map output-bin to OutputBin option...
      */

      num_pwgppds = cupsAddOption("OutputBin", ppd, num_pwgppds, &pwgppds);
    }

    if (pc->sides_option &&
        !ippFindAttribute(job->attrs, pc->sides_option, IPP_TAG_ZERO) &&
	(attr = ippFindAttribute(job->attrs, "sides", IPP_TAG_KEYWORD)) != NULL)
    {
     /*
      * Map sides to duplex option...
      */

      if (!strcmp(attr->values[0].string.text, "one-sided"))
        num_pwgppds = cupsAddOption(pc->sides_option, pc->sides_1sided,
				    num_pwgppds, &pwgppds);
      else if (!strcmp(attr->values[0].string.text, "two-sided-long-edge"))
        num_pwgppds = cupsAddOption(pc->sides_option, pc->sides_2sided_long,
				    num_pwgppds, &pwgppds);
      else if (!strcmp(attr->values[0].string.text, "two-sided-short-edge"))
        num_pwgppds = cupsAddOption(pc->sides_option, pc->sides_2sided_short,
				    num_pwgppds, &pwgppds);
    }

   /*
    * Map finishings values...
    */

    num_pwgppds = _ppdCacheGetFinishingOptions(pc, job->attrs,
                                               IPP_FINISHINGS_NONE, num_pwgppds,
                                               &pwgppds);
  }

 /*
  * Figure out how much room we need...
  */

  newlength = ipp_length(job->attrs);

  for (i = num_pwgppds, pwgppd = pwgppds; i > 0; i --, pwgppd ++)
    newlength += 1 + strlen(pwgppd->name) + 1 + strlen(pwgppd->value);

 /*
  * Then allocate/reallocate the option buffer as needed...
  */

  if (newlength == 0)			/* This can never happen, but Clang */
    newlength = 1;			/* thinks it can... */

  if (newlength > optlength || !options)
  {
    if (!options)
      optptr = malloc(newlength);
    else
      optptr = realloc(options, newlength);

    if (!optptr)
    {
      cupsdLogJob(job, CUPSD_LOG_CRIT,
		  "Unable to allocate " CUPS_LLFMT " bytes for option buffer.",
		  CUPS_LLCAST newlength);
      return (NULL);
    }

    options   = optptr;
    optlength = newlength;
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

      if (attr->value_tag == IPP_TAG_NOVALUE ||
          attr->value_tag == IPP_TAG_MIMETYPE ||
	  attr->value_tag == IPP_TAG_NAMELANG ||
	  attr->value_tag == IPP_TAG_TEXTLANG ||
	  (attr->value_tag == IPP_TAG_URI && strcmp(attr->name, "job-uuid") &&
	   strcmp(attr->name, "job-authorization-uri")) ||
	  attr->value_tag == IPP_TAG_URISCHEME ||
	  attr->value_tag == IPP_TAG_BEGIN_COLLECTION) /* Not yet supported */
	continue;

      if (!strcmp(attr->name, "job-hold-until") ||
          !strcmp(attr->name, "job-id") ||
          !strcmp(attr->name, "job-k-octets") ||
          !strcmp(attr->name, "job-media-sheets") ||
          !strcmp(attr->name, "job-media-sheets-completed") ||
          !strcmp(attr->name, "job-state") ||
          !strcmp(attr->name, "job-state-reasons"))
	continue;

      if (!strncmp(attr->name, "job-", 4) &&
          strcmp(attr->name, "job-account-id") &&
          strcmp(attr->name, "job-accounting-user-id") &&
          strcmp(attr->name, "job-authorization-uri") &&
          strcmp(attr->name, "job-billing") &&
          strcmp(attr->name, "job-impressions") &&
          strcmp(attr->name, "job-originating-host-name") &&
          strcmp(attr->name, "job-password") &&
          strcmp(attr->name, "job-password-encryption") &&
          strcmp(attr->name, "job-uuid") &&
          !(job->printer->type & CUPS_PRINTER_REMOTE))
	continue;

      if ((!strcmp(attr->name, "job-impressions") ||
           !strcmp(attr->name, "page-label") ||
           !strcmp(attr->name, "page-border") ||
           !strncmp(attr->name, "number-up", 9) ||
	   !strcmp(attr->name, "page-ranges") ||
	   !strcmp(attr->name, "page-set") ||
	   !_cups_strcasecmp(attr->name, "AP_FIRSTPAGE_InputSlot") ||
	   !_cups_strcasecmp(attr->name, "AP_FIRSTPAGE_ManualFeed") ||
	   !_cups_strcasecmp(attr->name, "com.apple.print.PrintSettings."
	                           "PMTotalSidesImaged..n.") ||
	   !_cups_strcasecmp(attr->name, "com.apple.print.PrintSettings."
	                           "PMTotalBeginPages..n.")) &&
	  banner_page)
        continue;

     /*
      * Otherwise add them to the list...
      */

      if (optptr > options)
	strlcat(optptr, " ", optlength - (size_t)(optptr - options));

      if (attr->value_tag != IPP_TAG_BOOLEAN)
      {
	strlcat(optptr, attr->name, optlength - (size_t)(optptr - options));
	strlcat(optptr, "=", optlength - (size_t)(optptr - options));
      }

      for (i = 0; i < attr->num_values; i ++)
      {
	if (i)
	  strlcat(optptr, ",", optlength - (size_t)(optptr - options));

	optptr += strlen(optptr);

	switch (attr->value_tag)
	{
	  case IPP_TAG_INTEGER :
	  case IPP_TAG_ENUM :
	      snprintf(optptr, optlength - (size_t)(optptr - options),
	               "%d", attr->values[i].integer);
	      break;

	  case IPP_TAG_BOOLEAN :
	      if (!attr->values[i].boolean)
		strlcat(optptr, "no", optlength - (size_t)(optptr - options));

	      strlcat(optptr, attr->name, optlength - (size_t)(optptr - options));
	      break;

	  case IPP_TAG_RANGE :
	      if (attr->values[i].range.lower == attr->values[i].range.upper)
		snprintf(optptr, optlength - (size_t)(optptr - options) - 1,
	        	 "%d", attr->values[i].range.lower);
              else
		snprintf(optptr, optlength - (size_t)(optptr - options) - 1,
	        	 "%d-%d", attr->values[i].range.lower,
			 attr->values[i].range.upper);
	      break;

	  case IPP_TAG_RESOLUTION :
	      snprintf(optptr, optlength - (size_t)(optptr - options) - 1,
	               "%dx%d%s", attr->values[i].resolution.xres,
		       attr->values[i].resolution.yres,
		       attr->values[i].resolution.units == IPP_RES_PER_INCH ?
			   "dpi" : "dpcm");
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

 /*
  * Finally loop through the PWG->PPD mapped options and add them...
  */

  for (i = num_pwgppds, pwgppd = pwgppds; i > 0; i --, pwgppd ++)
  {
    *optptr++ = ' ';
    strlcpy(optptr, pwgppd->name, optlength - (size_t)(optptr - options));
    optptr += strlen(optptr);
    *optptr++ = '=';
    strlcpy(optptr, pwgppd->value, optlength - (size_t)(optptr - options));
    optptr += strlen(optptr);
  }

  cupsFreeOptions(num_pwgppds, pwgppds);

 /*
  * Return the options string...
  */

  return (options);
}


/*
 * 'ipp_length()' - Compute the size of the buffer needed to hold
 *		    the textual IPP attributes.
 */

static size_t				/* O - Size of attribute buffer */
ipp_length(ipp_t *ipp)			/* I - IPP request */
{
  size_t		bytes; 		/* Number of bytes */
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

    if (attr->value_tag == IPP_TAG_NOVALUE ||
	attr->value_tag == IPP_TAG_MIMETYPE ||
	attr->value_tag == IPP_TAG_NAMELANG ||
	attr->value_tag == IPP_TAG_TEXTLANG ||
	attr->value_tag == IPP_TAG_URI ||
	attr->value_tag == IPP_TAG_URISCHEME)
      continue;

   /*
    * Add space for a leading space and commas between each value.
    * For the first attribute, the leading space isn't used, so the
    * extra byte can be used as the nul terminator...
    */

    bytes ++;				/* " " separator */
    bytes += (size_t)attr->num_values;	/* "," separators */

   /*
    * Boolean attributes appear as "foo,nofoo,foo,nofoo", while
    * other attributes appear as "foo=value1,value2,...,valueN".
    */

    if (attr->value_tag != IPP_TAG_BOOLEAN)
      bytes += strlen(attr->name);
    else
      bytes += (size_t)attr->num_values * strlen(attr->name);

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

	  bytes += (size_t)attr->num_values * 11;
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

	  bytes += (size_t)attr->num_values * 23;
	  break;

      case IPP_TAG_RESOLUTION :
         /*
	  * A resolution is two signed integers separated by an "x" and
	  * suffixed by the units, or 26 characters max.
	  */

	  bytes += (size_t)attr->num_values * 26;
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

  if ((fp = cupsdOpenConfFile(filename)) == NULL)
  {
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
    if (!_cups_strcasecmp(line, "NextJobId"))
    {
      if (value)
        NextJobId = atoi(value);
    }
    else if (!_cups_strcasecmp(line, "<Job"))
    {
      if (job)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Missing </Job> directive on line %d of %s.", linenum, filename);
        continue;
      }

      if (!value)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Missing job ID on line %d of %s.", linenum, filename);
	continue;
      }

      jobid = atoi(value);

      if (jobid < 1)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Bad job ID %d on line %d of %s.", jobid, linenum, filename);
        continue;
      }

      snprintf(jobfile, sizeof(jobfile), "%s/c%05d", RequestRoot, jobid);
      if (access(jobfile, 0))
      {
	snprintf(jobfile, sizeof(jobfile), "%s/c%05d.N", RequestRoot, jobid);
	if (access(jobfile, 0))
	{
	  cupsdLogMessage(CUPSD_LOG_ERROR, "[Job %d] Files have gone away.",
			  jobid);
	  continue;
	}
      }

      job = calloc(1, sizeof(cupsd_job_t));
      if (!job)
      {
        cupsdLogMessage(CUPSD_LOG_EMERG,
		        "[Job %d] Unable to allocate memory for job.", jobid);
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

      cupsdLogJob(job, CUPSD_LOG_DEBUG, "Loading from cache...");
    }
    else if (!job)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
	              "Missing <Job #> directive on line %d of %s.", linenum, filename);
      continue;
    }
    else if (!_cups_strcasecmp(line, "</Job>"))
    {
      cupsArrayAdd(Jobs, job);

      if (job->state_value <= IPP_JOB_STOPPED && cupsdLoadJob(job))
	cupsArrayAdd(ActiveJobs, job);
      else if (job->state_value > IPP_JOB_STOPPED)
      {
        if (!job->completed_time || !job->creation_time || !job->name || !job->koctets)
	{
	  cupsdLoadJob(job);
	  unload_job(job);
	}
      }

      job = NULL;
    }
    else if (!value)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Missing value on line %d of %s.", linenum, filename);
      continue;
    }
    else if (!_cups_strcasecmp(line, "State"))
    {
      job->state_value = (ipp_jstate_t)atoi(value);

      if (job->state_value < IPP_JOB_PENDING)
        job->state_value = IPP_JOB_PENDING;
      else if (job->state_value > IPP_JOB_COMPLETED)
        job->state_value = IPP_JOB_COMPLETED;
    }
    else if (!_cups_strcasecmp(line, "Name"))
    {
      cupsdSetString(&(job->name), value);
    }
    else if (!_cups_strcasecmp(line, "Created"))
    {
      job->creation_time = strtol(value, NULL, 10);
    }
    else if (!_cups_strcasecmp(line, "Completed"))
    {
      job->completed_time = strtol(value, NULL, 10);
    }
    else if (!_cups_strcasecmp(line, "HoldUntil"))
    {
      job->hold_until = strtol(value, NULL, 10);
    }
    else if (!_cups_strcasecmp(line, "Priority"))
    {
      job->priority = atoi(value);
    }
    else if (!_cups_strcasecmp(line, "Username"))
    {
      cupsdSetString(&job->username, value);
    }
    else if (!_cups_strcasecmp(line, "Destination"))
    {
      cupsdSetString(&job->dest, value);
    }
    else if (!_cups_strcasecmp(line, "DestType"))
    {
      job->dtype = (cups_ptype_t)atoi(value);
    }
    else if (!_cups_strcasecmp(line, "KOctets"))
    {
      job->koctets = atoi(value);
    }
    else if (!_cups_strcasecmp(line, "NumFiles"))
    {
      job->num_files = atoi(value);

      if (job->num_files < 0)
      {
	cupsdLogMessage(CUPSD_LOG_ERROR, "Bad NumFiles value %d on line %d of %s.", job->num_files, linenum, filename);
        job->num_files = 0;
	continue;
      }

      if (job->num_files > 0)
      {
        snprintf(jobfile, sizeof(jobfile), "%s/d%05d-001", RequestRoot,
	         job->id);
        if (access(jobfile, 0))
	{
	  cupsdLogJob(job, CUPSD_LOG_INFO, "Data files have gone away.");
          job->num_files = 0;
	  continue;
	}

        job->filetypes    = calloc((size_t)job->num_files, sizeof(mime_type_t *));
	job->compressions = calloc((size_t)job->num_files, sizeof(int));

        if (!job->filetypes || !job->compressions)
	{
	  cupsdLogJob(job, CUPSD_LOG_EMERG,
		      "Unable to allocate memory for %d files.",
		      job->num_files);
          break;
	}
      }
    }
    else if (!_cups_strcasecmp(line, "File"))
    {
      int	number,			/* File number */
		compression;		/* Compression value */
      char	super[MIME_MAX_SUPER],	/* MIME super type */
		type[MIME_MAX_TYPE];	/* MIME type */


      if (sscanf(value, "%d%*[ \t]%15[^/]/%255s%d", &number, super, type,
                 &compression) != 4)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Bad File on line %d of %s.", linenum, filename);
	continue;
      }

      if (number < 1 || number > job->num_files)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Bad File number %d on line %d of %s.", number, linenum, filename);
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

        cupsdLogJob(job, CUPSD_LOG_ERROR,
		    "Unknown MIME type %s/%s for file %d.",
		    super, type, number + 1);

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
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unknown %s directive on line %d of %s.", line, linenum, filename);
  }

  if (job)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
		    "Missing </Job> directive on line %d of %s.", linenum, filename);
    cupsdDeleteJob(job, CUPSD_JOB_PURGE);
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
    if (!_cups_strcasecmp(line, "NextJobId"))
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
        cupsdLogMessage(CUPSD_LOG_ERROR, "Ran out of memory for jobs.");
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
      else
        free(job);
    }

  cupsDirClose(dir);
}


/*
 * 'remove_job_files()' - Remove the document files for a job.
 */

static void
remove_job_files(cupsd_job_t *job)	/* I - Job */
{
  int	i;				/* Looping var */
  char	filename[1024];			/* Document filename */


  if (job->num_files <= 0)
    return;

  for (i = 1; i <= job->num_files; i ++)
  {
    snprintf(filename, sizeof(filename), "%s/d%05d-%03d", RequestRoot,
	     job->id, i);
    cupsdUnlinkOrRemoveFile(filename);
  }

  free(job->filetypes);
  free(job->compressions);

  job->file_time    = 0;
  job->num_files    = 0;
  job->filetypes    = NULL;
  job->compressions = NULL;

  LastEvent |= CUPSD_EVENT_PRINTER_STATE_CHANGED;
}


/*
 * 'remove_job_history()' - Remove the control file for a job.
 */

static void
remove_job_history(cupsd_job_t *job)	/* I - Job */
{
  char	filename[1024];			/* Control filename */


 /*
  * Remove the job info file...
  */

  snprintf(filename, sizeof(filename), "%s/c%05d", RequestRoot,
	   job->id);
  cupsdUnlinkOrRemoveFile(filename);

  LastEvent |= CUPSD_EVENT_PRINTER_STATE_CHANGED;
}


/*
 * 'set_time()' - Set one of the "time-at-xyz" attributes.
 */

static void
set_time(cupsd_job_t *job,		/* I - Job to update */
         const char  *name)		/* I - Name of attribute */
{
  char			date_name[128];	/* date-time-at-xxx */
  ipp_attribute_t	*attr;		/* Time attribute */
  time_t		curtime;	/* Current time */


  curtime = time(NULL);

  cupsdLogJob(job, CUPSD_LOG_DEBUG, "%s=%ld", name, (long)curtime);

  if ((attr = ippFindAttribute(job->attrs, name, IPP_TAG_ZERO)) != NULL)
  {
    attr->value_tag         = IPP_TAG_INTEGER;
    attr->values[0].integer = curtime;
  }

  snprintf(date_name, sizeof(date_name), "date-%s", name);

  if ((attr = ippFindAttribute(job->attrs, date_name, IPP_TAG_ZERO)) != NULL)
  {
    attr->value_tag = IPP_TAG_DATE;
    ippSetDate(job->attrs, &attr, 0, ippTimeToDate(curtime));
  }

  if (!strcmp(name, "time-at-completed"))
  {
    job->completed_time = curtime;

    if (JobHistory < INT_MAX && attr)
      job->history_time = attr->values[0].integer + JobHistory;
    else
      job->history_time = INT_MAX;

    if (job->history_time < JobHistoryUpdate || !JobHistoryUpdate)
      JobHistoryUpdate = job->history_time;

    if (JobFiles < INT_MAX && attr)
      job->file_time = attr->values[0].integer + JobFiles;
    else
      job->file_time = INT_MAX;

    if (job->file_time < JobHistoryUpdate || !JobHistoryUpdate)
      JobHistoryUpdate = job->file_time;

    cupsdLogMessage(CUPSD_LOG_DEBUG2, "set_time: JobHistoryUpdate=%ld",
		    (long)JobHistoryUpdate);
  }
}


/*
 * 'start_job()' - Start a print job.
 */

static void
start_job(cupsd_job_t     *job,		/* I - Job ID */
          cupsd_printer_t *printer)	/* I - Printer to print job */
{
  const char	*filename;		/* Support filename */
  ipp_attribute_t *cancel_after = ippFindAttribute(job->attrs,
						   "job-cancel-after",
						   IPP_TAG_INTEGER);
					/* job-cancel-after attribute */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "start_job(job=%p(%d), printer=%p(%s))",
                  job, job->id, printer, printer->name);

 /*
  * Make sure we have some files around before we try to print...
  */

  if (job->num_files == 0)
  {
    ippSetString(job->attrs, &job->reasons, 0, "aborted-by-system");
    cupsdSetJobState(job, IPP_JOB_ABORTED, CUPSD_JOB_DEFAULT,
                     "Aborting job because it has no files.");
    return;
  }

 /*
  * Update the printer and job state to "processing"...
  */

  if (!cupsdLoadJob(job))
    return;

  if (!job->printer_message)
    job->printer_message = ippFindAttribute(job->attrs,
                                            "job-printer-state-message",
                                            IPP_TAG_TEXT);
  if (job->printer_message)
    ippSetString(job->attrs, &job->printer_message, 0, "");

  ippSetString(job->attrs, &job->reasons, 0, "job-printing");
  cupsdSetJobState(job, IPP_JOB_PROCESSING, CUPSD_JOB_DEFAULT, NULL);
  cupsdSetPrinterState(printer, IPP_PRINTER_PROCESSING, 0);
  cupsdSetPrinterReasons(printer, "-cups-remote-pending,"
				  "cups-remote-pending-held,"
				  "cups-remote-processing,"
				  "cups-remote-stopped,"
				  "cups-remote-canceled,"
				  "cups-remote-aborted,"
				  "cups-remote-completed");

  job->cost         = 0;
  job->current_file = 0;
  job->file_time    = 0;
  job->history_time = 0;
  job->progress     = 0;
  job->printer      = printer;
  printer->job      = job;

  if (cancel_after)
    job->cancel_time = time(NULL) + ippGetInteger(cancel_after, 0);
  else if (MaxJobTime > 0)
    job->cancel_time = time(NULL) + MaxJobTime;
  else
    job->cancel_time = 0;

 /*
  * Check for support files...
  */

  cupsdSetPrinterReasons(job->printer, "-cups-missing-filter-warning,"
			               "cups-insecure-filter-warning");

  if (printer->pc)
  {
    for (filename = (const char *)cupsArrayFirst(printer->pc->support_files);
         filename;
         filename = (const char *)cupsArrayNext(printer->pc->support_files))
    {
      if (_cupsFileCheck(filename, _CUPS_FILE_CHECK_FILE, !RunUser,
			 cupsdLogFCMessage, printer))
        break;
    }
  }

 /*
  * Setup the last exit status and security profiles...
  */

  job->status   = 0;
  job->profile  = cupsdCreateProfile(job->id, 0);
  job->bprofile = cupsdCreateProfile(job->id, 1);

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
    cupsdDestroyProfile(job->bprofile);
    job->bprofile = NULL;
    return;
  }

  job->status_buffer = cupsdStatBufNew(job->status_pipes[0], NULL);
  job->status_level  = CUPSD_LOG_INFO;

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
    cupsdDestroyProfile(job->bprofile);
    job->bprofile = NULL;
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
    cupsdDestroyProfile(job->bprofile);
    job->bprofile = NULL;
    return;
  }

  fcntl(job->side_pipes[0], F_SETFL,
	fcntl(job->side_pipes[0], F_GETFL) | O_NONBLOCK);
  fcntl(job->side_pipes[1], F_SETFL,
	fcntl(job->side_pipes[1], F_GETFL) | O_NONBLOCK);

  fcntl(job->side_pipes[0], F_SETFD,
	fcntl(job->side_pipes[0], F_GETFD) | FD_CLOEXEC);
  fcntl(job->side_pipes[1], F_SETFD,
	fcntl(job->side_pipes[1], F_GETFD) | FD_CLOEXEC);

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

  if (action == CUPSD_JOB_DEFAULT && !job->kill_time && job->backend > 0)
    job->kill_time = time(NULL) + JobKillDelay;
  else if (action >= CUPSD_JOB_FORCE)
    job->kill_time = 0;

  for (i = 0; job->filters[i]; i ++)
    if (job->filters[i] > 0)
    {
      cupsdEndProcess(job->filters[i], action >= CUPSD_JOB_FORCE);

      if (action >= CUPSD_JOB_FORCE)
        job->filters[i] = -job->filters[i];
    }

  if (job->backend > 0)
  {
    cupsdEndProcess(job->backend, action >= CUPSD_JOB_FORCE);

    if (action >= CUPSD_JOB_FORCE)
      job->backend = -job->backend;
  }

  if (action >= CUPSD_JOB_FORCE)
  {
   /*
    * Clear job status...
    */

    job->status = 0;
  }
}


/*
 * 'unload_job()' - Unload a job from memory.
 */

static void
unload_job(cupsd_job_t *job)		/* I - Job */
{
  if (!job->attrs)
    return;

  cupsdLogJob(job, CUPSD_LOG_DEBUG, "Unloading...");

  ippDelete(job->attrs);

  job->attrs           = NULL;
  job->state           = NULL;
  job->reasons         = NULL;
  job->impressions     = NULL;
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
  cupsd_printer_t *printer = job->printer;
					/* Printer */
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

      if (job->impressions)
      {
        if (!_cups_strncasecmp(message, "total ", 6))
	{
	 /*
	  * Got a total count of pages from a backend or filter...
	  */

	  copies = atoi(message + 6);
	  copies -= ippGetInteger(job->impressions, 0); /* Just track the delta */
	}
	else if (!sscanf(message, "%*d%d", &copies))
	  copies = 1;

        ippSetInteger(job->attrs, &job->impressions, 0, ippGetInteger(job->impressions, 0) + copies);
        job->dirty = 1;
	cupsdMarkDirty(CUPSD_DIRTY_JOBS);
      }

      if (job->sheets)
      {
        if (!_cups_strncasecmp(message, "total ", 6))
	{
	 /*
	  * Got a total count of pages from a backend or filter...
	  */

	  copies = atoi(message + 6);
	  copies -= ippGetInteger(job->sheets, 0); /* Just track the delta */
	}
	else if (!sscanf(message, "%*d%d", &copies))
	  copies = 1;

        ippSetInteger(job->attrs, &job->sheets, 0, ippGetInteger(job->sheets, 0) + copies);
        job->dirty = 1;
	cupsdMarkDirty(CUPSD_DIRTY_JOBS);

	if (job->printer->page_limit)
	  cupsdUpdateQuota(job->printer, job->username, copies, 0);
      }

      cupsdLogPage(job, message);

      if (job->sheets)
	cupsdAddEvent(CUPSD_EVENT_JOB_PROGRESS, job->printer, job, "Printed %d page(s).", ippGetInteger(job->sheets, 0));
    }
    else if (loglevel == CUPSD_LOG_JOBSTATE)
    {
     /*
      * Support "keyword" to set job-state-reasons to the specified keyword.
      * This is sufficient for the current paid printing stuff.
      */

      cupsdLogJob(job, CUPSD_LOG_DEBUG, "JOBSTATE: %s", message);

      if (!strcmp(message, "cups-retry-as-raster"))
        job->retry_as_raster = 1;
      else
        ippSetString(job->attrs, &job->reasons, 0, message);
    }
    else if (loglevel == CUPSD_LOG_STATE)
    {
      cupsdLogJob(job, CUPSD_LOG_DEBUG, "STATE: %s", message);

      if (!strcmp(message, "paused"))
      {
        cupsdStopPrinter(job->printer, 1);
	return;
      }
      else if (message[0] && cupsdSetPrinterReasons(job->printer, message))
      {
	event |= CUPSD_EVENT_PRINTER_STATE;

        if (MaxJobTime > 0)
        {
         /*
          * Reset cancel time after connecting to the device...
          */

          for (i = 0; i < job->printer->num_reasons; i ++)
            if (!strcmp(job->printer->reasons[i], "connecting-to-device"))
              break;

          if (i >= job->printer->num_reasons)
          {
	    ipp_attribute_t *cancel_after = ippFindAttribute(job->attrs,
							     "job-cancel-after",
							     IPP_TAG_INTEGER);
					/* job-cancel-after attribute */

            if (cancel_after)
	      job->cancel_time = time(NULL) + ippGetInteger(cancel_after, 0);
	    else
	      job->cancel_time = time(NULL) + MaxJobTime;
	  }
        }
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

      if ((attr = cupsGetOption("auth-info-default", num_attrs,
                                attrs)) != NULL)
      {
        job->printer->num_options = cupsAddOption("auth-info", attr,
						  job->printer->num_options,
						  &(job->printer->options));
	cupsdSetPrinterAttrs(job->printer);

	cupsdMarkDirty(CUPSD_DIRTY_PRINTERS);
      }

      if ((attr = cupsGetOption("auth-info-required", num_attrs,
                                attrs)) != NULL)
      {
        cupsdSetAuthInfoRequired(job->printer, attr, NULL);
	cupsdSetPrinterAttrs(job->printer);

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

      cupsdLogJob(job, CUPSD_LOG_DEBUG, "PPD: %s", message);

      job->num_keywords = cupsParseOptions(message, job->num_keywords,
                                           &job->keywords);
    }
    else
    {
     /*
      * Strip legacy message prefix...
      */

      if (!strncmp(message, "recoverable:", 12))
      {
        ptr = message + 12;
	while (isspace(*ptr & 255))
          ptr ++;
      }
      else if (!strncmp(message, "recovered:", 10))
      {
        ptr = message + 10;
	while (isspace(*ptr & 255))
          ptr ++;
      }
      else
        ptr = message;

      if (*ptr)
        cupsdLogJob(job, loglevel, "%s", ptr);

      if (loglevel < CUPSD_LOG_DEBUG &&
          strcmp(job->printer->state_message, ptr))
      {
	strlcpy(job->printer->state_message, ptr,
		sizeof(job->printer->state_message));

	event |= CUPSD_EVENT_PRINTER_STATE | CUPSD_EVENT_JOB_PROGRESS;

	if (loglevel <= job->status_level && job->status_level > CUPSD_LOG_ERROR)
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

  if (event & CUPSD_EVENT_JOB_PROGRESS)
    cupsdAddEvent(CUPSD_EVENT_JOB_PROGRESS, job->printer, job,
                  "%s", job->printer->state_message);
  if (event & CUPSD_EVENT_PRINTER_STATE)
    cupsdAddEvent(CUPSD_EVENT_PRINTER_STATE, job->printer, NULL,
		  (job->printer->type & CUPS_PRINTER_CLASS) ?
		      "Class \"%s\" state changed." :
		      "Printer \"%s\" state changed.",
		  job->printer->name);


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

    finalize_job(job, 1);

   /*
    * Try printing another job...
    */

    if (printer->state != IPP_PRINTER_STOPPED)
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
  {
    ippSetString(job->attrs, &job->printer_message, 0, "");

    job->dirty = 1;
    cupsdMarkDirty(CUPSD_DIRTY_JOBS);
  }
  else if (job->printer->state_message[0] && do_message)
  {
    ippSetString(job->attrs, &job->printer_message, 0, job->printer->state_message);

    job->dirty = 1;
    cupsdMarkDirty(CUPSD_DIRTY_JOBS);
  }

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

  job->dirty = 1;
  cupsdMarkDirty(CUPSD_DIRTY_JOBS);
}


/*
 * End of "$Id: job.c 13047 2016-01-13 19:16:12Z msweet $".
 */
