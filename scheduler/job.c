/*
 * "$Id$"
 *
 *   Job management routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
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
 *   cupsdAddJob()              - Add a new job to the job queue...
 *   cupsdCancelJob()           - Cancel the specified print job.
 *   cupsdCancelJobs()          - Cancel all jobs for the given
 *                                destination/user...
 *   cupsdCheckJobs()           - Check the pending jobs and start any if
 *                                the destination is available.
 *   cupsdCleanJobs()           - Clean out old jobs.
 *   cupsdDeleteJob()           - Free all memory used by a job.
 *   cupsdFinishJob()           - Finish a job.
 *   cupsdFreeAllJobs()         - Free all jobs from memory.
 *   cupsdFindJob()             - Find the specified job.
 *   cupsdGetPrinterJobCount()  - Get the number of pending, processing,
 *   cupsdGetUserJobCount()     - Get the number of pending, processing,
 *   cupsdHoldJob()             - Hold the specified job.
 *   cupsdLoadAllJobs()         - Load all jobs from disk.
 *   cupsdLoadJob()             - Load a single job...
 *   cupsdMoveJob()             - Move the specified job to a different
 *                                destination.
 *   cupsdReleaseJob()          - Release the specified job.
 *   cupsdRestartJob()          - Restart the specified job.
 *   cupsdSaveAllJobs()         - Save a summary of all jobs to disk.
 *   cupsdSaveJob()             - Save a job to disk.
 *   cupsdSetJobHoldUntil()     - Set the hold time for a job...
 *   cupsdSetJobPriority()      - Set the priority of a job, moving it
 *                                up/down in the list as needed.
 *   cupsdStopAllJobs()         - Stop all print jobs.
 *   cupsdStopJob()             - Stop a print job.
 *   cupsdUnloadCompletedJobs() - Flush completed job history from memory.
 *   compare_active_jobs()      - Compare the job IDs and priorities of two
 *                                jobs.
 *   compare_jobs()             - Compare the job IDs of two jobs.
 *   ipp_length()               - Compute the size of the buffer needed to
 *                                hold the textual IPP attributes.
 *   load_job_cache()           - Load jobs from the job.cache file.
 *   load_next_job_id()         - Load the NextJobId value from the
 *                                job.cache file.
 *   load_request_root()        - Load jobs from the RequestRoot directory.
 *   set_time()                 - Set one of the "time-at-xyz" attributes...
 *   set_hold_until()           - Set the hold time and update job-hold-until
 *                                attribute...
 *   start_job()                - Start a print job.
 *   unload_job()               - Unload a job from memory.
 *   update_job()               - Read a status update from a jobs filters.
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
static int	ipp_length(ipp_t *ipp);
static void	load_job_cache(const char *filename);
static void	load_next_job_id(const char *filename);
static void	load_request_root(void);
static void	set_time(cupsd_job_t *job, const char *name);
static void	set_hold_until(cupsd_job_t *job, time_t holdtime);
static void	start_job(cupsd_job_t *job, cupsd_printer_t *printer);
static void	unload_job(cupsd_job_t *job);
static void	update_job(cupsd_job_t *job);
static void	update_job_attrs(cupsd_job_t *job, int do_message);


/*
 * 'cupsdAddJob()' - Add a new job to the job queue...
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
 * 'cupsdCancelJob()' - Cancel the specified print job.
 */

void
cupsdCancelJob(cupsd_job_t  *job,	/* I - Job to cancel */
               int          purge,	/* I - Purge jobs? */
	       ipp_jstate_t newstate)	/* I - New job state */
{
  int		i;			/* Looping var */
  char		filename[1024];		/* Job filename */
  cupsd_printer_t *printer;		/* Printer used by job */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdCancelJob: id = %d", job->id);

 /*
  * Stop any processes that are working on the current job...
  */

  printer = job->printer;

  if (job->state_value == IPP_JOB_PROCESSING)
    cupsdStopJob(job, 0);

  cupsdLoadJob(job);

  if (job->attrs)
    job->state->values[0].integer = newstate;

  job->state_value = newstate;

  set_time(job, "time-at-completed");

 /*
  * Send any pending notifications and then expire them...
  */

  switch (newstate)
  {
    default :
        break;

    case IPP_JOB_CANCELED :
	cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, printer, job,
                      purge ? "Job purged." : "Job canceled.");
        break;

    case IPP_JOB_ABORTED :
	cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, printer, job,
                      "Job aborted; please consult the error_log file "
		      "for details.");
        break;

    case IPP_JOB_COMPLETED :
       /*
	* Clear the printer's printer-state-message and move on...
	*/

	printer->state_message[0] = '\0';

	cupsdSetPrinterState(printer, IPP_PRINTER_IDLE, 0);

	cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, printer, job,
                      "Job completed.");
        break;
  }

  cupsdExpireSubscriptions(NULL, job);

 /*
  * Remove the job from the active and printing lists...
  */

  cupsArrayRemove(ActiveJobs, job);
  cupsArrayRemove(PrintingJobs, job);

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

  job->current_file = 0;

  if (!JobHistory || !JobFiles || purge)
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

  if (JobHistory && !purge)
  {
   /*
    * Save job state info...
    */

    job->dirty = 1;
    cupsdMarkDirty(CUPSD_DIRTY_JOBS);
  }
  else
  {
   /*
    * Remove the job info file...
    */

    snprintf(filename, sizeof(filename), "%s/c%05d", RequestRoot,
	     job->id);
    unlink(filename);

   /*
    * Remove the job from the "all jobs" list...
    */

    cupsArrayRemove(Jobs, job);

   /*
    * Free all memory used...
    */

    cupsdDeleteJob(job);
  }

  cupsdSetBusyState();
}


/*
 * 'cupsdCancelJobs()' - Cancel all jobs for the given destination/user...
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
    if (!job->dest || !job->username)
      cupsdLoadJob(job);

    if (!job->dest || !job->username)
      continue;

    if ((dest == NULL || !strcmp(job->dest, dest)) &&
        (username == NULL || !strcmp(job->username, username)))
    {
     /*
      * Cancel all jobs matching this destination/user...
      */

      cupsdCancelJob(job, purge, IPP_JOB_CANCELED);
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


  DEBUG_puts("cupsdCheckJobs()");

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdCheckJobs: %d active jobs, sleeping=%d, reload=%d",
                  cupsArrayCount(ActiveJobs), Sleeping, NeedReload);

  for (job = (cupsd_job_t *)cupsArrayFirst(ActiveJobs);
       job;
       job = (cupsd_job_t *)cupsArrayNext(ActiveJobs))
  {
   /*
    * Start held jobs if they are ready...
    */

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdCheckJobs: Job %d: dest=%s, dtype=%x, "
		    "state_value=%d, loaded=%s", job->id, job->dest, job->dtype,
		    job->state_value, job->attrs ? "yes" : "no");

    if (job->state_value == IPP_JOB_HELD &&
        job->hold_until &&
	job->hold_until < time(NULL))
    {
      if (job->pending_timeout)
      {
       /* Add trailing banner as needed */
        if (cupsdTimeoutJob(job))
	  continue;
      }

      job->state->values[0].integer = IPP_JOB_PENDING;
      job->state_value              = IPP_JOB_PENDING;

      if ((attr = ippFindAttribute(job->attrs, "job-hold-until",
				   IPP_TAG_KEYWORD)) == NULL)
	attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

      if (attr)
      {
	attr->value_tag = IPP_TAG_KEYWORD;
	cupsdSetString(&(attr->values[0].string.text), "no-hold");
	job->dirty = 1;
	cupsdMarkDirty(CUPSD_DIRTY_JOBS);
      }
    }

   /*
    * Start pending jobs if the destination is available...
    */

    if (job->state_value == IPP_JOB_PENDING && !NeedReload && !Sleeping)
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

        cupsdLogJob(job, CUPSD_LOG_WARN,
		    "Printer/class %s has gone away; canceling job!",
		    job->dest);

	cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                      "Job canceled because the destination printer/class has "
		      "gone away.");

        cupsdCancelJob(job, 1, IPP_JOB_ABORTED);
      }
      else if (printer)
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
	     printer->state == IPP_PRINTER_IDLE) ||	/* and idle */
	    ((printer->type & CUPS_PRINTER_DISCOVERED) && /* Printer is remote */
	     !printer->job))				/* and not printing */
        {
	 /* 
	  * Clear any message and reasons for the queue...
	  */

          printer->state_message[0] = '\0';

	  cupsdSetPrinterReasons(printer, "none");

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
    if (job->state_value >= IPP_JOB_CANCELED)
      cupsdCancelJob(job, 1, IPP_JOB_CANCELED);
}


/*
 * 'cupsdDeleteJob()' - Free all memory used by a job.
 */

void
cupsdDeleteJob(cupsd_job_t *job)	/* I - Job */
{
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
  }

  ippDelete(job->attrs);

  free(job);
}


/*
 * 'cupsdFinishJob()' - Finish a job.
 */

void
cupsdFinishJob(cupsd_job_t *job)	/* I - Job */
{
  cupsd_printer_t	*printer;	/* Current printer */
  ipp_attribute_t	*attr;		/* job-hold-until attribute */


  cupsdLogJob(job, CUPSD_LOG_DEBUG, "File %d is complete.",
	      job->current_file - 1);

  cupsdLogJob(job, CUPSD_LOG_DEBUG2,
	      "cupsdFinishJob: job->status is %d",
	      job->status);

  if (job->status_buffer &&
      (job->status < 0 || job->current_file >= job->num_files))
  {
   /*
    * Close the pipe and clear the input bit.
    */

    cupsdRemoveSelect(job->status_buffer->fd);

    cupsdLogJob(job, CUPSD_LOG_DEBUG2,
		"cupsdFinishJob: Closing status pipes [ %d %d ]...",
		job->status_pipes[0], job->status_pipes[1]);

    cupsdClosePipe(job->status_pipes);
    cupsdStatBufDelete(job->status_buffer);

    job->status_buffer = NULL;
  }

  printer = job->printer;

  update_job_attrs(job, 0);

  if (job->status < 0)
  {
   /*
    * Backend had errors; stop it...
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

	  cupsdStopJob(job, 0);

          if (job->dtype & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT))
	  {
	   /*
	    * Mark the job as pending again - we'll retry on another
	    * printer...
	    */

	    job->state->values[0].integer = IPP_JOB_PENDING;
	    job->state_value              = IPP_JOB_PENDING;
          }

	  job->dirty = 1;
	  cupsdMarkDirty(CUPSD_DIRTY_JOBS);

	 /*
	  * If the job was queued to a class or the error policy is
	  * "retry-current-job", try requeuing it...  For faxes and retry-job
	  * queues, hold the current job for 5 minutes.
	  */

	  if ((job->dtype & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT)) ||
	      !strcmp(printer->error_policy, "retry-current-job"))
	    cupsdCheckJobs();
	  else if ((printer->type & CUPS_PRINTER_FAX) ||
        	   !strcmp(printer->error_policy, "retry-job"))
	  {
	   /*
	    * See how many times we've tried to send the job; if more than
	    * the limit, cancel the job.
	    */

	    job->tries ++;

	    if (job->tries >= JobRetryLimit)
	    {
	     /*
	      * Too many tries...
	      */

	      cupsdLogJob(job, CUPSD_LOG_ERROR,
			  "Canceling job since it could not be "
			  "sent after %d tries.",
			  JobRetryLimit);

	      cupsdCancelJob(job, 0, IPP_JOB_ABORTED);
	    }
	    else
	    {
	     /*
	      * Try again in N seconds...
	      */

	      set_hold_until(job, time(NULL) + JobRetryInterval);

	      cupsdAddEvent(CUPSD_EVENT_JOB_STATE, job->printer, job,
			    "Job held due to fax errors; please consult "
			    "the error_log file for details.");
	      cupsdSetPrinterState(printer, IPP_PRINTER_IDLE, 0);
	    }
	  }
	  else if (!strcmp(printer->error_policy, "abort-job"))
	    cupsdCancelJob(job, 0, IPP_JOB_ABORTED);
          else
          {
	    cupsdSetPrinterState(printer, IPP_PRINTER_STOPPED, 1);

	    cupsdAddEvent(CUPSD_EVENT_JOB_STOPPED, printer, job,
			  "Job stopped due to backend errors; please consult "
			  "the error_log file for details.");
	  }
          break;

      case CUPS_BACKEND_CANCEL :
         /*
	  * Cancel the job...
	  */

	  cupsdCancelJob(job, 0, IPP_JOB_CANCELED);
          break;

      case CUPS_BACKEND_HOLD :
         /*
	  * Hold the job...
	  */

	  cupsdStopJob(job, 0);

	  cupsdSetJobHoldUntil(job, "indefinite");

	  job->state->values[0].integer = IPP_JOB_HELD;
	  job->state_value              = IPP_JOB_HELD;

	  job->dirty = 1;
	  cupsdMarkDirty(CUPSD_DIRTY_JOBS);

	  cupsdAddEvent(CUPSD_EVENT_JOB_STOPPED, printer, job,
			"Job held due to backend errors; please consult "
			"the error_log file for details.");
          break;

      case CUPS_BACKEND_STOP :
         /*
	  * Stop the printer...
	  */

	  cupsdStopJob(job, 0);

	  job->state->values[0].integer = IPP_JOB_PENDING;
	  job->state_value              = IPP_JOB_PENDING;

	  job->dirty = 1;
	  cupsdMarkDirty(CUPSD_DIRTY_JOBS);

	  cupsdSetPrinterState(printer, IPP_PRINTER_STOPPED, 1);

	  cupsdAddEvent(CUPSD_EVENT_JOB_STOPPED, printer, job,
			"Job stopped due to backend errors; please consult "
			"the error_log file for details.");
          break;

      case CUPS_BACKEND_AUTH_REQUIRED :
	  cupsdStopJob(job, 0);

	  cupsdSetJobHoldUntil(job, "auth-info-required");

	  if ((attr = ippFindAttribute(job->attrs, "job-hold-until",
				       IPP_TAG_KEYWORD)) == NULL)
	    attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

	  if (attr)
	  {
	    attr->value_tag = IPP_TAG_KEYWORD;
	    cupsdSetString(&(attr->values[0].string.text),
	                   "auth-info-required");
	  }

	  job->state->values[0].integer = IPP_JOB_HELD;
	  job->state_value              = IPP_JOB_HELD;

	  job->dirty = 1;
	  cupsdMarkDirty(CUPSD_DIRTY_JOBS);

	  cupsdAddEvent(CUPSD_EVENT_JOB_STOPPED, printer, job,
	                "Authentication is required for job %d.", job->id);
          break;
    }

   /*
    * Try printing another job...
    */

    cupsdCheckJobs();
  }
  else if (job->status > 0)
  {
   /*
    * Filter had errors; stop job...
    */

    cupsdLogJob(job, CUPSD_LOG_ERROR, "Job stopped due to filter errors.");
    cupsdStopJob(job, 1);
    job->dirty = 1;
    cupsdMarkDirty(CUPSD_DIRTY_JOBS);
    cupsdAddEvent(CUPSD_EVENT_JOB_STOPPED, printer, job,
                  "Job stopped due to filter errors; please consult the "
		  "error_log file for details.");
    cupsdCheckJobs();
  }
  else
  {
   /*
    * Job printed successfully; cancel it...
    */

    if (job->current_file < job->num_files)
    {
     /*
      * Start the next file in the job...
      */

      FilterLevel -= job->cost;
      start_job(job, printer);
    }
    else
    {
     /*
      * Close out this job...
      */

      cupsdLogJob(job, CUPSD_LOG_INFO, "Completed successfully.");
      cupsdCancelJob(job, 0, IPP_JOB_COMPLETED);
      cupsdCheckJobs();
    }
  }
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

  cupsdStopAllJobs(1);
  cupsdSaveAllJobs();

  for (job = (cupsd_job_t *)cupsArrayFirst(Jobs);
       job;
       job = (cupsd_job_t *)cupsArrayNext(Jobs))
  {
    cupsArrayRemove(Jobs, job);
    cupsArrayRemove(ActiveJobs, job);
    cupsArrayRemove(PrintingJobs, job);

    cupsdDeleteJob(job);
  }

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
 * 'cupsdHoldJob()' - Hold the specified job.
 */

void
cupsdHoldJob(cupsd_job_t *job)		/* I - Job data */
{
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdHoldJob: id = %d", job->id);

  if (job->state_value == IPP_JOB_PROCESSING)
    cupsdStopJob(job, 0);
  else
    cupsdLoadJob(job);

  DEBUG_puts("cupsdHoldJob: setting state to held...");

  job->state->values[0].integer = IPP_JOB_HELD;
  job->state_value              = IPP_JOB_HELD;
  job->current_file             = 0;

  job->dirty = 1;
  cupsdMarkDirty(CUPSD_DIRTY_JOBS);
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
 * 'cupsdLoadJob()' - Load a single job...
 */

void
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

    return;
  }

  if ((job->attrs = ippNew()) == NULL)
  {
    cupsdLogJob(job, CUPSD_LOG_ERROR, "Ran out of memory for job attributes!");
    return;
  }

 /*
  * Load job attributes...
  */

  cupsdLogJob(job, CUPSD_LOG_DEBUG, "Loading attributes...");

  snprintf(jobfile, sizeof(jobfile), "%s/c%05d", RequestRoot, job->id);
  if ((fp = cupsFileOpen(jobfile, "r")) == NULL)
  {
    cupsdLogJob(job, CUPSD_LOG_ERROR,
		"Unable to open job control file \"%s\" - %s!",
		jobfile, strerror(errno));
    ippDelete(job->attrs);
    job->attrs = NULL;
    return;
  }

  if (ippReadIO(fp, (ipp_iocb_t)cupsFileRead, 1, NULL, job->attrs) != IPP_DATA)
  {
    cupsdLogJob(job, CUPSD_LOG_ERROR,
		"Unable to read job control file \"%s\"!",
		jobfile);
    cupsFileClose(fp);
    ippDelete(job->attrs);
    job->attrs = NULL;
    unlink(jobfile);
    return;
  }

  cupsFileClose(fp);

 /*
  * Copy attribute data to the job object...
  */

  if ((job->state = ippFindAttribute(job->attrs, "job-state",
                                     IPP_TAG_ENUM)) == NULL)
  {
    cupsdLogJob(job, CUPSD_LOG_ERROR,
		"Missing or bad job-state attribute in control file!");
    ippDelete(job->attrs);
    job->attrs = NULL;
    unlink(jobfile);
    return;
  }

  job->state_value = (ipp_jstate_t)job->state->values[0].integer;

  if (!job->dest)
  {
    if ((attr = ippFindAttribute(job->attrs, "job-printer-uri",
                                 IPP_TAG_URI)) == NULL)
    {
      cupsdLogJob(job, CUPSD_LOG_ERROR,
		  "No job-printer-uri attribute in control file!");
      ippDelete(job->attrs);
      job->attrs = NULL;
      unlink(jobfile);
      return;
    }

    if ((dest = cupsdValidateDest(attr->values[0].string.text, &(job->dtype),
                                  &destptr)) == NULL)
    {
      cupsdLogJob(job, CUPSD_LOG_ERROR,
		  "Unable to queue job for destination \"%s\"!",
		  attr->values[0].string.text);
      ippDelete(job->attrs);
      job->attrs = NULL;
      unlink(jobfile);
      return;
    }

    cupsdSetString(&job->dest, dest);
  }
  else if ((destptr = cupsdFindDest(job->dest)) == NULL)
  {
    cupsdLogJob(job, CUPSD_LOG_ERROR,
		"Unable to queue job for destination \"%s\"!", job->dest);
    ippDelete(job->attrs);
    job->attrs = NULL;
    unlink(jobfile);
    return;
  }

  job->sheets     = ippFindAttribute(job->attrs, "job-media-sheets-completed",
                                     IPP_TAG_INTEGER);
  job->job_sheets = ippFindAttribute(job->attrs, "job-sheets", IPP_TAG_NAME);

  if (!job->priority)
  {
    if ((attr = ippFindAttribute(job->attrs, "job-priority",
                        	 IPP_TAG_INTEGER)) == NULL)
    {
      cupsdLogJob(job, CUPSD_LOG_ERROR,
		  "Missing or bad job-priority attribute in control file!");
      ippDelete(job->attrs);
      job->attrs = NULL;
      unlink(jobfile);
      return;
    }

    job->priority = attr->values[0].integer;
  }

  if (!job->username)
  {
    if ((attr = ippFindAttribute(job->attrs, "job-originating-user-name",
                        	 IPP_TAG_NAME)) == NULL)
    {
      cupsdLogJob(job, CUPSD_LOG_ERROR,
		  "Missing or bad job-originating-user-name attribute in "
		  "control file!");
      ippDelete(job->attrs);
      job->attrs = NULL;
      unlink(jobfile);
      return;
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
      cupsdSetJobHoldUntil(job, attr->values[0].string.text);
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

      cupsdLogJob(job, CUPSD_LOG_DEBUG,
		  "Auto-typing document file \"%s\"...", jobfile);

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
          cupsdLogJob(job, CUPSD_LOG_ERROR,
	              "Ran out of memory for job file types!");
	  return;
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

  if (job->state_value == IPP_JOB_PROCESSING)
    cupsdStopJob(job, 0);
  else
    cupsdLoadJob(job);

  cupsdAddEvent(CUPSD_EVENT_JOB_STOPPED, oldp, job,
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
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdReleaseJob: id = %d", job->id);

  if (job->state_value == IPP_JOB_HELD)
  {
   /*
    * Add trailing banner as needed...
    */

    if (job->pending_timeout)
      cupsdTimeoutJob(job);

    DEBUG_puts("cupsdReleaseJob: setting state to pending...");

    job->state->values[0].integer = IPP_JOB_PENDING;
    job->state_value              = IPP_JOB_PENDING;
    job->dirty = 1;
    cupsdMarkDirty(CUPSD_DIRTY_JOBS);
  }
}


/*
 * 'cupsdRestartJob()' - Restart the specified job.
 */

void
cupsdRestartJob(cupsd_job_t *job)	/* I - Job */
{
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdRestartJob: id = %d", job->id);

  if (job->state_value == IPP_JOB_STOPPED || job->num_files)
  {
    ipp_jstate_t	old_state;	/* Old job state */


    cupsdLoadJob(job);

    old_state = job->state_value;

    job->tries = 0;
    job->state->values[0].integer = IPP_JOB_PENDING;
    job->state_value              = IPP_JOB_PENDING;

    job->dirty = 1;
    cupsdMarkDirty(CUPSD_DIRTY_JOBS);

    if (old_state > IPP_JOB_STOPPED)
      cupsArrayAdd(ActiveJobs, job);
  }
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
    cupsdLogJob(job, CUPSD_LOG_ERROR,
		"Unable to create job control file \"%s\" - %s.",
		filename, strerror(errno));
    return;
  }

  fchmod(cupsFileNumber(fp), 0600);
  fchown(cupsFileNumber(fp), RunUser, Group);

  job->attrs->state = IPP_IDLE;

  if (ippWriteIO(fp, (ipp_iocb_t)cupsFileWrite, 1, NULL,
                 job->attrs) != IPP_DATA)
    cupsdLogJob(job, CUPSD_LOG_ERROR, "Unable to write job control file!");

  cupsFileClose(fp);

  job->dirty = 0;
}


/*
 * 'cupsdSetJobHoldUntil()' - Set the hold time for a job...
 */

void
cupsdSetJobHoldUntil(cupsd_job_t *job,	/* I - Job */
                     const char  *when)	/* I - When to resume */
{
  time_t	curtime;		/* Current time */
  struct tm	*curdate;		/* Current date */
  int		hour;			/* Hold hour */
  int		minute;			/* Hold minute */
  int		second;			/* Hold second */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdSetJobHoldUntil(%d, \"%s\")",
                  job->id, when);

  second = 0;

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

    if (curdate->tm_wday || curdate->tm_wday == 6)
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

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdSetJobHoldUntil: hold_until = %d",
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
 * 'cupsdStopAllJobs()' - Stop all print jobs.
 */

void
cupsdStopAllJobs(int force)		/* I - 1 = Force all filters to stop */
{
  cupsd_job_t	*job;			/* Current job */


  DEBUG_puts("cupsdStopAllJobs()");

  for (job = (cupsd_job_t *)cupsArrayFirst(ActiveJobs);
       job;
       job = (cupsd_job_t *)cupsArrayNext(ActiveJobs))
    if (job->state_value == IPP_JOB_PROCESSING)
    {
      cupsdStopJob(job, force);
      job->state->values[0].integer = IPP_JOB_PENDING;
      job->state_value              = IPP_JOB_PENDING;
    }
}


/*
 * 'cupsdStopJob()' - Stop a print job.
 */

void
cupsdStopJob(cupsd_job_t *job,		/* I - Job */
             int         force)		/* I - 1 = Force all filters to stop */
{
  int	i;				/* Looping var */


  cupsdLogJob(job, CUPSD_LOG_DEBUG2, "cupsdStopJob: force = %d", force);

  if (job->state_value != IPP_JOB_PROCESSING)
    return;

  FilterLevel -= job->cost;

  if (job->printer->state == IPP_PRINTER_PROCESSING)
    cupsdSetPrinterState(job->printer, IPP_PRINTER_IDLE, 0);

  job->state->values[0].integer = IPP_JOB_STOPPED;
  job->state_value              = IPP_JOB_STOPPED;
  job->printer->job = NULL;
  job->printer      = NULL;

  job->current_file --;

  for (i = 0; job->filters[i]; i ++)
    if (job->filters[i] > 0)
    {
      cupsdEndProcess(job->filters[i], force);
      job->filters[i] = 0;
    }

  if (job->backend > 0)
  {
    cupsdEndProcess(job->backend, force);
    job->backend = 0;
  }

  cupsdDestroyProfile(job->profile);
  job->profile = NULL;

  cupsdLogJob(job, CUPSD_LOG_DEBUG2, "Closing print pipes [ %d %d ]...",
	      job->print_pipes[0], job->print_pipes[1]);

  cupsdClosePipe(job->print_pipes);

  cupsdLogJob(job, CUPSD_LOG_DEBUG2, "Closing back pipes [ %d %d ]...",
	      job->back_pipes[0], job->back_pipes[1]);

  cupsdClosePipe(job->back_pipes);

  cupsdLogJob(job, CUPSD_LOG_DEBUG2, "Closing side pipes [ %d %d ]...",
	      job->side_pipes[0], job->side_pipes[1]);

  cupsdClosePipe(job->side_pipes);

  if (job->status_buffer)
  {
   /*
    * Close the pipe and clear the input bit.
    */

    cupsdRemoveSelect(job->status_buffer->fd);

    cupsdLogJob(job, CUPSD_LOG_DEBUG2, "Closing status pipes [ %d %d ]...",
		job->status_pipes[0], job->status_pipes[1]);

    cupsdClosePipe(job->status_pipes);
    cupsdStatBufDelete(job->status_buffer);

    job->status_buffer = NULL;
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
    if (job->attrs && job->state_value >= IPP_JOB_STOPPED &&
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

      cupsdLogJob(job, CUPSD_LOG_DEBUG, "Loading from cache...");
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

      if (job->state_value <= IPP_JOB_STOPPED)
      {
        cupsArrayAdd(ActiveJobs, job);
	cupsdLoadJob(job);
      }

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
	  cupsdLogJob(job, CUPSD_LOG_INFO, "Data files have gone away!");
          job->num_files = 0;
	  continue;
	}

        job->filetypes    = calloc(job->num_files, sizeof(mime_type_t *));
	job->compressions = calloc(job->num_files, sizeof(int));

        if (!job->filetypes || !job->compressions)
	{
	  cupsdLogJob(job, CUPSD_LOG_EMERG,
		      "Unable to allocate memory for %d files!",
		      job->num_files);
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

        cupsdLogJob(job, CUPSD_LOG_ERROR,
		    "Unknown MIME type %s/%s for file %d!",
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

      cupsdLoadJob(job);

     /*
      * Insert the job into the array, sorting by job priority and ID...
      */

      cupsArrayAdd(Jobs, job);

      if (job->state_value <= IPP_JOB_STOPPED)
        cupsArrayAdd(ActiveJobs, job);
      else
        unload_job(job);
    }

  cupsDirClose(dir);
}


/*
 * 'set_time()' - Set one of the "time-at-xyz" attributes...
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
 * 'set_hold_until()' - Set the hold time and update job-hold-until attribute...
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
 * 'start_job()' - Start a print job.
 */

static void
start_job(cupsd_job_t     *job,		/* I - Job ID */
          cupsd_printer_t *printer)	/* I - Printer to print job */
{
  int			i;		/* Looping var */
  int			slot;		/* Pipe slot */
  cups_array_t		*filters,	/* Filters for job */
			*prefilters;	/* Filters with prefilters */
  mime_filter_t		*filter,	/* Current filter */
			*prefilter,	/* Prefilter */
			port_monitor;	/* Port monitor filter */
  char			method[255],	/* Method for output */
			*optptr,	/* Pointer to options */
			*valptr;	/* Pointer in value string */
  ipp_attribute_t	*attr;		/* Current attribute */
  struct stat		backinfo;	/* Backend file information */
  int			backroot;	/* Run backend as root? */
  int			pid;		/* Process ID of new filter process */
  int			banner_page;	/* 1 if banner page, 0 otherwise */
  int			filterfds[2][2];/* Pipes used between filters */
  int			envc;		/* Number of environment variables */
  char			**argv,		/* Filter command-line arguments */
			sani_uri[1024],	/* Sanitized DEVICE_URI env var */
			filename[1024],	/* Job filename */
			command[1024],	/* Full path to command */
			jobid[255],	/* Job ID string */
			title[IPP_MAX_NAME],
					/* Job title string */
			copies[255],	/* # copies string */
			*envp[MAX_ENV + 17],
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
			printer_name[255],
					/* PRINTER env variable */
			rip_max_cache[255];
					/* RIP_MAX_CACHE env variable */
  static char		*options = NULL;/* Full list of options */
  static int		optlength = 0;	/* Length of option buffer */


  cupsdLogJob(job, CUPSD_LOG_DEBUG2, "start_job: file = %d/%d",
	      job->current_file, job->num_files);

  if (job->num_files == 0)
  {
    cupsdLogJob(job, CUPSD_LOG_ERROR, "No files, canceling job!");
    cupsdCancelJob(job, 0, IPP_JOB_ABORTED);
    return;
  }

 /*
  * Figure out what filters are required to convert from
  * the source to the destination type...
  */

  filters   = NULL;
  job->cost = 0;

  if (printer->raw)
  {
   /*
    * Remote jobs and raw queues go directly to the printer without
    * filtering...
    */

    cupsdLogJob(job, CUPSD_LOG_DEBUG, "Sending job to queue tagged as raw...");

    filters = NULL;
  }
  else
  {
   /*
    * Local jobs get filtered...
    */

    filters = mimeFilter(MimeDatabase, job->filetypes[job->current_file],
                         printer->filetype, &(job->cost));

    if (!filters)
    {
      cupsdLogJob(job, CUPSD_LOG_ERROR,
		  "Unable to convert file %d to printable format!",
		  job->current_file);
      cupsdLogMessage(CUPSD_LOG_INFO,
                      "Hint: Do you have Ghostscript installed?");

      if (LogLevel < CUPSD_LOG_DEBUG)
        cupsdLogMessage(CUPSD_LOG_INFO,
	                "Hint: Try setting the LogLevel to \"debug\".");

      job->current_file ++;

      if (job->current_file == job->num_files)
        cupsdCancelJob(job, 0, IPP_JOB_ABORTED);

      return;
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

    if (printer->prefiltertype && filters)
    {
      prefilters = cupsArrayNew(NULL, NULL);

      for (filter = (mime_filter_t *)cupsArrayFirst(filters);
	   filter;
	   filter = (mime_filter_t *)cupsArrayNext(filters))
      {
	if ((prefilter = mimeFilterLookup(MimeDatabase, filter->src,
					  printer->prefiltertype)))
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
		"start_job: file=%d, cost=%d, level=%d, limit=%d",
		job->current_file, job->cost, FilterLevel,
		FilterLimit);
    return;
  }

  FilterLevel += job->cost;

 /*
  * Add decompression/raw filter as needed...
  */

  if ((!printer->raw && job->compressions[job->current_file]) ||
      (!filters && !printer->remote &&
       (job->num_files > 1 || !strncmp(printer->device_uri, "file:", 5))))
  {
   /*
    * Add gziptoany filter to the front of the list...
    */

    if (!filters)
      filters = cupsArrayNew(NULL, NULL);

    if (!cupsArrayInsert(filters, &gziptoany_filter))
    {
      cupsdLogJob(job, CUPSD_LOG_ERROR,
		  "Unable to add decompression filter - %s", strerror(errno));

      cupsArrayDelete(filters);

      job->current_file ++;

      if (job->current_file == job->num_files)
        cupsdCancelJob(job, 0, IPP_JOB_ABORTED);

      return;
    }
  }

 /*
  * Add port monitor, if any...
  */

  if (printer->port_monitor)
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
             "%s/monitor/%s", ServerBin, printer->port_monitor);

    if (!cupsArrayAdd(filters, &port_monitor))
    {
      cupsdLogJob(job, CUPSD_LOG_ERROR,
		  "Unable to add port monitor - %s", strerror(errno));

      cupsArrayDelete(filters);

      job->current_file ++;

      if (job->current_file == job->num_files)
        cupsdCancelJob(job, 0, IPP_JOB_ABORTED);

      return;
    }
  }

 /*
  * Make sure we don't go over the "MAX_FILTERS" limit...
  */

  if (cupsArrayCount(filters) > MAX_FILTERS)
  {
    cupsdLogJob(job, CUPSD_LOG_ERROR,
		"Too many filters (%d > %d), unable to print!",
		cupsArrayCount(filters), MAX_FILTERS);

    cupsArrayDelete(filters);
    cupsdCancelJob(job, 0, IPP_JOB_STOPPED);

    return;
  }

 /*
  * Update the printer and job state to "processing"...
  */

  job->state->values[0].integer = IPP_JOB_PROCESSING;
  job->state_value              = IPP_JOB_PROCESSING;
  job->progress                 = 0;
  job->status                   = 0;
  job->printer                  = printer;
  printer->job                  = job;

  cupsdSetPrinterState(printer, IPP_PRINTER_PROCESSING, 0);

  if (job->current_file == 0)
  {
   /*
    * Add to the printing list...
    */

    cupsArrayAdd(PrintingJobs, job);
    cupsdSetBusyState();

   /*
    * Set the processing time...
    */

    set_time(job, "time-at-processing");

   /*
    * Create the backchannel pipes and make them non-blocking...
    */

    cupsdOpenPipe(job->back_pipes);

    fcntl(job->back_pipes[0], F_SETFL,
          fcntl(job->back_pipes[0], F_GETFL) | O_NONBLOCK);

    fcntl(job->back_pipes[1], F_SETFL,
          fcntl(job->back_pipes[1], F_GETFL) | O_NONBLOCK);

   /*
    * Create the side-channel pipes and make them non-blocking...
    */

    socketpair(AF_LOCAL, SOCK_STREAM, 0, job->side_pipes);

    fcntl(job->side_pipes[0], F_SETFL,
          fcntl(job->side_pipes[0], F_GETFL) | O_NONBLOCK);

    fcntl(job->side_pipes[1], F_SETFL,
          fcntl(job->side_pipes[1], F_GETFL) | O_NONBLOCK);
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

  if (printer->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT))
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

  cupsdLogJob(job, CUPSD_LOG_DEBUG, "banner_page = %d", banner_page);

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
    if (optlength == 0)
      optptr = malloc(i);
    else
      optptr = realloc(options, i);

    if (optptr == NULL)
    {
      cupsdLogJob(job, CUPSD_LOG_CRIT,
		  "Unable to allocate %d bytes for option buffer!", i);

      cupsArrayDelete(filters);

      FilterLevel -= job->cost;

      cupsdCancelJob(job, 0, IPP_JOB_ABORTED);
      return;
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

  snprintf(title, sizeof(title), "%s-%d", printer->name, job->id);
  strcpy(copies, "1");

  for (attr = job->attrs->attrs; attr != NULL; attr = attr->next)
  {
    if (!strcmp(attr->name, "copies") &&
	attr->value_tag == IPP_TAG_INTEGER)
    {
     /*
      * Don't use the # copies attribute if we are printing the job sheets...
      */

      if (!banner_page)
        sprintf(copies, "%d", attr->values[0].integer);
    }
    else if (!strcmp(attr->name, "job-name") &&
	     (attr->value_tag == IPP_TAG_NAME ||
	      attr->value_tag == IPP_TAG_NAMELANG))
      strlcpy(title, attr->values[0].string.text, sizeof(title));
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
          strcmp(attr->name, "job-uuid") &&
          strcmp(attr->name, "job-impressions") &&
          strcmp(attr->name, "job-originating-host-name") &&
          !(printer->type & CUPS_PRINTER_REMOTE))
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

  if (printer->remote && job->num_files > 1)
    argv = calloc(7 + job->num_files, sizeof(char *));
  else
    argv = calloc(8, sizeof(char *));

  if (!argv)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to allocate argument array!");
    cupsArrayDelete(filters);

    FilterLevel -= job->cost;

    cupsdStopPrinter(printer, 0);
    return;
  }

  sprintf(jobid, "%d", job->id);

  argv[0] = printer->name;
  argv[1] = jobid;
  argv[2] = job->username;
  argv[3] = title;
  argv[4] = copies;
  argv[5] = options;

  if (printer->remote && job->num_files > 1)
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
    cupsdLogJob(job, CUPSD_LOG_DEBUG,
		"argv[%d]=\"%s\"", i, argv[i]);

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

        snprintf(lang, sizeof(lang), "LANG=%s.UTF8",
	         attr->values[0].string.text);
        break;

    case 5 :
       /*
        * Language and country code (ll-cc)...
	*/

        snprintf(lang, sizeof(lang), "LANG=%c%c_%c%c.UTF8",
	         attr->values[0].string.text[0],
		 attr->values[0].string.text[1],
		 toupper(attr->values[0].string.text[3] & 255),
		 toupper(attr->values[0].string.text[4] & 255));
        break;
  }

  attr = ippFindAttribute(job->attrs, "document-format",
                          IPP_TAG_MIMETYPE);
  if (attr != NULL &&
      (optptr = strstr(attr->values[0].string.text, "charset=")) != NULL)
    snprintf(charset, sizeof(charset), "CHARSET=%s", optptr + 8);
  else
  {
    attr = ippFindAttribute(job->attrs, "attributes-charset",
	                    IPP_TAG_CHARSET);
    snprintf(charset, sizeof(charset), "CHARSET=%s",
             attr->values[0].string.text);
  }

  snprintf(content_type, sizeof(content_type), "CONTENT_TYPE=%s/%s",
           job->filetypes[job->current_file]->super,
           job->filetypes[job->current_file]->type);
  snprintf(device_uri, sizeof(device_uri), "DEVICE_URI=%s",
           printer->device_uri);
  cupsdSanitizeURI(printer->device_uri, sani_uri, sizeof(sani_uri));
  snprintf(ppd, sizeof(ppd), "PPD=%s/ppd/%s.ppd", ServerRoot, printer->name);
  snprintf(printer_name, sizeof(printer_name), "PRINTER=%s", printer->name);
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
  envp[envc ++] = printer_name;
  envp[envc ++] = banner_page ? "CUPS_FILETYPE=job-sheet" :
                                "CUPS_FILETYPE=document";

  if (!printer->remote && !printer->raw)
  {
    filter = (mime_filter_t *)cupsArrayLast(filters);

    if (printer->port_monitor)
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
                  sani_uri);

  if (printer->remote)
    job->current_file = job->num_files;
  else
    job->current_file ++;

 /*
  * Now create processes for all of the filters...
  */

  filterfds[0][0] = -1;
  filterfds[0][1] = -1;
  filterfds[1][0] = -1;
  filterfds[1][1] = -1;

  if (!job->status_buffer)
  {
    if (cupsdOpenPipe(job->status_pipes))
    {
      cupsdLogJob(job, CUPSD_LOG_ERROR,
		  "Unable to create job status pipes - %s.", strerror(errno));
      snprintf(printer->state_message, sizeof(printer->state_message),
	       "Unable to create status pipes - %s.", strerror(errno));

      cupsdAddPrinterHistory(printer);

      cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
		    "Job canceled because the server could not create the job "
		    "status pipes.");

      goto abort_job;
    }

    cupsdLogJob(job, CUPSD_LOG_DEBUG2,
		"start_job: status_pipes = [ %d %d ]",
		job->status_pipes[0], job->status_pipes[1]);

    job->status_buffer = cupsdStatBufNew(job->status_pipes[0], NULL);
    job->status_level  = CUPSD_LOG_INFO;
  }

  job->status = 0;
  memset(job->filters, 0, sizeof(job->filters));

  if (!job->profile)
    job->profile = cupsdCreateProfile(job->id);

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
	cupsdLogJob(job, CUPSD_LOG_ERROR,
		    "Unable to create job filter pipes - %s.", strerror(errno));
	snprintf(printer->state_message, sizeof(printer->state_message),
		"Unable to create filter pipes - %s.", strerror(errno));
	cupsdAddPrinterHistory(printer);

	cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                      "Job canceled because the server could not create the "
		      "filter pipes.");

        goto abort_job;
      }
    }
    else
    {
      if (job->current_file == 1)
      {
	if (strncmp(printer->device_uri, "file:", 5) != 0)
	{
	  if (cupsdOpenPipe(job->print_pipes))
	  {
	    cupsdLogJob(job, CUPSD_LOG_ERROR,
			"Unable to create job backend pipes - %s.",
			strerror(errno));
	    snprintf(printer->state_message, sizeof(printer->state_message),
		    "Unable to create backend pipes - %s.", strerror(errno));
	    cupsdAddPrinterHistory(printer);

	    cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                	  "Job canceled because the server could not create "
			  "the backend pipes.");

            goto abort_job;
	  }
	}
	else
	{
	  job->print_pipes[0] = -1;
	  if (!strcmp(printer->device_uri, "file:/dev/null") ||
	      !strcmp(printer->device_uri, "file:///dev/null"))
	    job->print_pipes[1] = -1;
	  else
	  {
	    if (!strncmp(printer->device_uri, "file:/dev/", 10))
	      job->print_pipes[1] = open(printer->device_uri + 5,
	                        	 O_WRONLY | O_EXCL);
	    else if (!strncmp(printer->device_uri, "file:///dev/", 12))
	      job->print_pipes[1] = open(printer->device_uri + 7,
	                        	 O_WRONLY | O_EXCL);
	    else if (!strncmp(printer->device_uri, "file:///", 8))
	      job->print_pipes[1] = open(printer->device_uri + 7,
	                        	 O_WRONLY | O_CREAT | O_TRUNC, 0600);
	    else
	      job->print_pipes[1] = open(printer->device_uri + 5,
	                        	 O_WRONLY | O_CREAT | O_TRUNC, 0600);

	    if (job->print_pipes[1] < 0)
	    {
              cupsdLogJob(job, CUPSD_LOG_ERROR,
			  "Unable to open output file \"%s\" - %s.",
			  printer->device_uri, strerror(errno));
              snprintf(printer->state_message, sizeof(printer->state_message),
		       "Unable to open output file \"%s\" - %s.",
	               printer->device_uri, strerror(errno));

	      cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                	    "Job canceled because the server could not open the "
			    "output file.");

              goto abort_job;
	    }

	    fcntl(job->print_pipes[1], F_SETFD,
        	  fcntl(job->print_pipes[1], F_GETFD) | FD_CLOEXEC);
          }
	}

	cupsdLogJob(job, CUPSD_LOG_DEBUG2,
		    "start_job: print_pipes = [ %d %d ]",
		    job->print_pipes[0], job->print_pipes[1]);
      }

      filterfds[slot][0] = job->print_pipes[0];
      filterfds[slot][1] = job->print_pipes[1];
    }

    cupsdLogJob(job, CUPSD_LOG_DEBUG2, "start_job: filter=\"%s\"", command);
    cupsdLogJob(job, CUPSD_LOG_DEBUG2, "start_job: filterfds[%d]=[ %d %d ]",
		slot, filterfds[slot][0], filterfds[slot][1]);

    pid = cupsdStartProcess(command, argv, envp, filterfds[!slot][0],
                            filterfds[slot][1], job->status_pipes[1],
		            job->back_pipes[0], job->side_pipes[0], 0,
			    job->profile, job->filters + i);

    cupsdLogJob(job, CUPSD_LOG_DEBUG2,
		"start_job: Closing filter pipes for slot %d [ %d %d ]...",
		!slot, filterfds[!slot][0], filterfds[!slot][1]);

    cupsdClosePipe(filterfds[!slot]);

    if (pid == 0)
    {
      cupsdLogJob(job, CUPSD_LOG_ERROR, "Unable to start filter \"%s\" - %s.",
		  filter->filter, strerror(errno));
      snprintf(printer->state_message, sizeof(printer->state_message),
               "Unable to start filter \"%s\" - %s.",
               filter->filter, strerror(errno));

      cupsdAddPrinterHistory(printer);

      cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                    "Job canceled because the server could not execute a "
		    "filter.");

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

  if (strncmp(printer->device_uri, "file:", 5) != 0)
  {
    if (job->current_file == 1 || printer->remote)
    {
      sscanf(printer->device_uri, "%254[^:]", method);
      snprintf(command, sizeof(command), "%s/backend/%s", ServerBin, method);

     /*
      * See if the backend needs to run as root...
      */

      if (RunUser)
        backroot = 0;
      else if (stat(command, &backinfo))
	backroot = 0;
      else
        backroot = !(backinfo.st_mode & (S_IRWXG | S_IRWXO));

      argv[0] = sani_uri;

      filterfds[slot][0] = -1;
      filterfds[slot][1] = -1;

      cupsdLogJob(job, CUPSD_LOG_DEBUG2, "start_job: backend=\"%s\"", command);
      cupsdLogJob(job, CUPSD_LOG_DEBUG2, "start_job: filterfds[%d] = [ %d %d ]",
		  slot, filterfds[slot][0], filterfds[slot][1]);

      pid = cupsdStartProcess(command, argv, envp, filterfds[!slot][0],
			      filterfds[slot][1], job->status_pipes[1],
			      job->back_pipes[1], job->side_pipes[1],
			      backroot, job->profile, &(job->backend));

      if (pid == 0)
      {
	cupsdLogJob(job, CUPSD_LOG_ERROR, "Unable to start backend \"%s\" - %s.",
		    method, strerror(errno));
	snprintf(printer->state_message, sizeof(printer->state_message),
        	 "Unable to start backend \"%s\" - %s.", method,
		 strerror(errno));

	cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                      "Job canceled because the server could not execute "
		      "the backend.");

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
      cupsdLogJob(job, CUPSD_LOG_DEBUG2,
		  "start_job: Closing print pipes [ %d %d ]...",
		  job->print_pipes[0], job->print_pipes[1]);

      cupsdClosePipe(job->print_pipes);

      cupsdLogJob(job, CUPSD_LOG_DEBUG2,
		  "start_job: Closing back pipes [ %d %d ]...",
		  job->back_pipes[0], job->back_pipes[1]);

      cupsdClosePipe(job->back_pipes);

      cupsdLogJob(job, CUPSD_LOG_DEBUG2,
		  "start_job: Closing side pipes [ %d %d ]...",
		  job->side_pipes[0], job->side_pipes[1]);

      cupsdClosePipe(job->side_pipes);

      cupsdLogJob(job, CUPSD_LOG_DEBUG2,
		  "start_job: Closing status output pipe %d...",
		  job->status_pipes[1]);

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
      cupsdLogJob(job, CUPSD_LOG_DEBUG2,
		  "start_job: Closing print pipes [ %d %d ]...",
		  job->print_pipes[0], job->print_pipes[1]);

      cupsdClosePipe(job->print_pipes);

      cupsdLogJob(job, CUPSD_LOG_DEBUG2,
		  "start_job: Closing status output pipe %d...",
		  job->status_pipes[1]);

      close(job->status_pipes[1]);
      job->status_pipes[1] = -1;
    }
  }

  cupsdLogJob(job, CUPSD_LOG_DEBUG2,
	      "start_job: Closing filter pipes for slot %d [ %d %d ]...",
	      slot, filterfds[slot][0], filterfds[slot][1]);
  cupsdClosePipe(filterfds[slot]);

  if (printer->remote && job->num_files > 1)
  {
    for (i = 0; i < job->num_files; i ++)
      free(argv[i + 6]);
  }

  free(argv);

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

  for (slot = 0; slot < 2; slot ++)
  {
    cupsdLogJob(job, CUPSD_LOG_DEBUG2,
		"start_job: Closing filter pipes for slot %d [ %d %d ]...",
		slot, filterfds[slot][0], filterfds[slot][1]);
    cupsdClosePipe(filterfds[slot]);
  }

  cupsdLogJob(job, CUPSD_LOG_DEBUG2,
	      "start_job: Closing status pipes [ %d %d ]...",
	      job->status_pipes[0], job->status_pipes[1]);
  cupsdClosePipe(job->status_pipes);
  cupsdStatBufDelete(job->status_buffer);

  job->status_buffer = NULL;

  cupsArrayDelete(filters);

  if (printer->remote && job->num_files > 1)
  {
    for (i = 0; i < job->num_files; i ++)
      free(argv[i + 6]);
  }

  free(argv);

  cupsdStopJob(job, 0);
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
  char		message[1024],		/* Message text */
		*ptr;			/* Pointer update... */
  int		loglevel,		/* Log level for message */
		event = 0;		/* Events? */


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

	    cupsdLogJob(job, CUPSD_LOG_INFO,
			"Canceled because pages exceed user %s "
			"quota limit on printer %s (%s).",
			job->username, job->printer->name,
			job->printer->info);

	    cupsdCancelJob(job, 1, IPP_JOB_CANCELED);
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
      else
      {
	cupsdSetPrinterReasons(job->printer, message);
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
      }

      if ((attr = cupsGetOption("marker-levels", num_attrs, attrs)) != NULL)
      {
        cupsdSetPrinterAttr(job->printer, "marker-levels", (char *)attr);
	job->printer->marker_time = time(NULL);
	event |= CUPSD_EVENT_PRINTER_STATE;
      }

      if ((attr = cupsGetOption("marker-message", num_attrs, attrs)) != NULL)
      {
        cupsdSetPrinterAttr(job->printer, "marker-message", (char *)attr);
	job->printer->marker_time = time(NULL);
	event |= CUPSD_EVENT_PRINTER_STATE;
      }

      if ((attr = cupsGetOption("marker-names", num_attrs, attrs)) != NULL)
      {
        cupsdSetPrinterAttr(job->printer, "marker-names", (char *)attr);
	job->printer->marker_time = time(NULL);
	event |= CUPSD_EVENT_PRINTER_STATE;
      }

      if ((attr = cupsGetOption("marker-types", num_attrs, attrs)) != NULL)
      {
        cupsdSetPrinterAttr(job->printer, "marker-types", (char *)attr);
	job->printer->marker_time = time(NULL);
	event |= CUPSD_EVENT_PRINTER_STATE;
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
#ifdef __APPLE__
    else if (!strncmp(message, "recoverable:", 12))
    {
      cupsdSetPrinterReasons(job->printer,
                             "+com.apple.print.recoverable-warning");

      ptr = message + 12;
      while (isspace(*ptr & 255))
        ptr ++;

      cupsdSetString(&job->printer->recoverable, ptr);
      cupsdAddPrinterHistory(job->printer);
      event |= CUPSD_EVENT_PRINTER_STATE;
    }
    else if (!strncmp(message, "recovered:", 10))
    {
      cupsdSetPrinterReasons(job->printer,
                             "-com.apple.print.recoverable-warning");

      ptr = message + 10;
      while (isspace(*ptr & 255))
        ptr ++;

      cupsdSetString(&job->printer->recoverable, ptr);
      cupsdAddPrinterHistory(job->printer);
      event |= CUPSD_EVENT_PRINTER_STATE;
    }
#endif /* __APPLE__ */
    else
    {
      cupsdLogJob(job, loglevel, "%s", message);

      if (loglevel < CUPSD_LOG_DEBUG)
      {
	strlcpy(job->printer->state_message, message,
		sizeof(job->printer->state_message));
	cupsdAddPrinterHistory(job->printer);

	event |= CUPSD_EVENT_PRINTER_STATE;

	if (loglevel <= job->status_level)
	{
	 /*
	  * Some messages show in the printer-state-message attribute...
	  */

	  if (loglevel != CUPSD_LOG_NOTICE)
	    job->status_level = loglevel;

	  update_job_attrs(job, 1);
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

  if (ptr == NULL && !job->status_buffer->bufused)
  {
   /*
    * See if all of the filters and the backend have returned their
    * exit statuses.
    */

    for (i = 0; job->filters[i] < 0; i ++);

    if (job->filters[i])
      return;

    if (job->current_file >= job->num_files && job->backend > 0)
      return;

   /*
    * Handle the end of job stuff...
    */

    cupsdFinishJob(job);
  }
}


/*
 * 'update_job_attrs()' - Update the job-printer-* attributes.
 */

void
update_job_attrs(cupsd_job_t *job,	/* I - Job to update */
                 int         do_message)/* I - 1 = update job-printer-state message */
{
  int			i;		/* Looping var */
  int			num_reasons;	/* Actual number of reasons */
  const char * const	*reasons;	/* Reasons */
  static const char	*none = "none",	/* "none" */
			*paused = "paused";
					/* "paused" */


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
  * If the job isn't printing, return now...
  */

  if (!job->printer)
    return;

 /*
  * Otherwise copy the printer-state-message value...
  */

  if (job->printer->state_message[0] &&
      (do_message || !job->printer_message->values[0].string.text[0]))
    cupsdSetString(&(job->printer_message->values[0].string.text),
		   job->printer->state_message);

 /*
  * ... and the printer-state-reasons value...
  */

  if (job->printer->num_reasons == 0)
  {
    num_reasons = 1;
    reasons     = job->printer->state == IPP_PRINTER_STOPPED ? &paused : &none;
  }
  else
  {
    num_reasons = job->printer->num_reasons;
    reasons     = (const char * const *)job->printer->reasons;
  }

  if (!job->printer_reasons || job->printer_reasons->num_values != num_reasons)
  {
    ippDeleteAttribute(job->attrs, job->printer_reasons);

    job->printer_reasons = ippAddStrings(job->attrs,
                                         IPP_TAG_JOB, IPP_TAG_KEYWORD,
					 "job-printer-state-reasons",
					 num_reasons, NULL, NULL);
  }

  for (i = 0; i < num_reasons; i ++)
    cupsdSetString(&(job->printer_reasons->values[i].string.text), reasons[i]);
}


/*
 * End of "$Id$".
 */
