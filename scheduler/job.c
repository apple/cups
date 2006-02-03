/*
 * "$Id$"
 *
 *   Job management routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   cupsdAddJob()              - Add a new job to the job queue...
 *   cupsdCancelJob()           - Cancel the specified print job.
 *   cupsdCancelJobs()          - Cancel all jobs for the given destination/user...
 *   cupsdCheckJobs()           - Check the pending jobs and start any if the
 *                                destination is available.
 *   cupsdCleanJobs()           - Clean out old jobs.
 *   cupsdFreeAllJobs()         - Free all jobs from memory.
 *   cupsdFindJob()             - Find the specified job.
 *   cupsdGetPrinterJobCount()  - Get the number of pending, processing,
 *                                or held jobs in a printer or class.
 *   cupsdGetUserJobCount()     - Get the number of pending, processing,
 *                                or held jobs for a user.
 *   cupsdHoldJob()             - Hold the specified job.
 *   cupsdLoadAllJobs()         - Load all jobs from disk.
 *   cupsdMoveJob()             - Move the specified job to a different
 *                                destination.
 *   cupsdReleaseJob()          - Release the specified job.
 *   cupsdRestartJob()          - Restart the specified job.
 *   cupsdSaveJob()             - Save a job to disk.
 *   cupsdSetJobHoldUntil()     - Set the hold time for a job...
 *   cupsdSetJobPriority()      - Set the priority of a job, moving it up/down
 *                                in the list as needed.
 *   cupsdStartJob()            - Start a print job.
 *   cupsdStopAllJobs()         - Stop all print jobs.
 *   cupsdStopJob()             - Stop a print job.
 *   cupsdUpdateJob()           - Read a status update from a job's filters.
 *   compare_active_jobs()      - Compare the job IDs and priorities of two jobs.
 *   compare_jobs()             - Compare the job IDs of two jobs.
 *   ipp_length()               - Compute the size of the buffer needed to hold 
 *		                  the textual IPP attributes.
 *   set_hold_until()           - Set the hold time and update job-hold-until attribute.
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
static void	set_time(cupsd_job_t *job, const char *name);
static void	set_hold_until(cupsd_job_t *job, time_t holdtime);


/*
 * 'cupsdAddJob()' - Add a new job to the job queue...
 */

cupsd_job_t *				/* O - New job record */
cupsdAddJob(int        priority,	/* I - Job priority */
            const char *dest)		/* I - Job destination */
{
  cupsd_job_t	*job;			/* New job record */


  job = calloc(sizeof(cupsd_job_t), 1);

  job->id             = NextJobId ++;
  job->priority       = priority;
  job->back_pipes[0]  = -1;
  job->back_pipes[1]  = -1;
  job->print_pipes[0] = -1;
  job->print_pipes[1] = -1;

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
cupsdCancelJob(cupsd_job_t *job,	/* I - Job to cancel */
               int         purge)	/* I - Purge jobs? */
{
  int		i;			/* Looping var */
  char		filename[1024];		/* Job filename */


  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdCancelJob: id = %d", job->id);

 /*
  * Remove the job from the active list...
  */

  cupsArrayRemove(ActiveJobs, job);

 /*
  * Stop any processes that are working on the current job...
  */

  if (job->state->values[0].integer == IPP_JOB_PROCESSING)
    cupsdStopJob(job, 0);

  cupsArrayRemove(ActiveJobs, job);

  job->state->values[0].integer = IPP_JOB_CANCELLED;

  set_time(job, "time-at-completed");

  cupsdExpireSubscriptions(NULL, job);

 /*
  * Remove any authentication data...
  */

  snprintf(filename, sizeof(filename), "%s/a%05d", RequestRoot,
	   job->id);
  unlink(filename);

 /*
  * Remove the print file for good if we aren't preserving jobs or
  * files...
  */

  job->current_file = 0;

  if (!JobHistory || !JobFiles || purge ||
      (job->dtype & CUPS_PRINTER_REMOTE))
    for (i = 1; i <= job->num_files; i ++)
    {
      snprintf(filename, sizeof(filename), "%s/d%05d-%03d", RequestRoot,
	       job->id, i);
      unlink(filename);
    }

  if (JobHistory && !purge && !(job->dtype & CUPS_PRINTER_REMOTE))
  {
   /*
    * Save job state info...
    */

    cupsdSaveJob(job);
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

    if (job->attrs != NULL)
      ippDelete(job->attrs);

    if (job->num_files > 0)
    {
      free(job->compressions);
      free(job->filetypes);
    }

    cupsdClearString(&job->username);
    cupsdClearString(&job->dest);

    free(job);
  }
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
    if ((dest == NULL || !strcmp(job->dest, dest)) &&
        (username == NULL || !strcmp(job->username, username)))
    {
     /*
      * Cancel all jobs matching this destination/user...
      */

      cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                    purge ? "Job purged." : "Job canceled.");

      cupsdCancelJob(job, purge);
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


  DEBUG_puts("cupsdCheckJobs()");

  for (job = (cupsd_job_t *)cupsArrayFirst(ActiveJobs);
       job;
       job = (cupsd_job_t *)cupsArrayNext(ActiveJobs))
  {
   /*
    * Start held jobs if they are ready...
    */

    if (job->state->values[0].integer == IPP_JOB_HELD &&
        job->hold_until &&
	job->hold_until < time(NULL))
      job->state->values[0].integer = IPP_JOB_PENDING;

   /*
    * Start pending jobs if the destination is available...
    */

    if (job->state->values[0].integer == IPP_JOB_PENDING && !NeedReload &&
        !Sleeping)
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

        if (!(pclass->type & CUPS_PRINTER_REMOTE))
	{
	  if (pclass->state != IPP_PRINTER_STOPPED)
	    printer = cupsdFindAvailablePrinter(job->dest);
	  else
	    printer = NULL;
	}
      }

      if (!printer && !pclass)
      {
       /*
        * Whoa, the printer and/or class for this destination went away;
	* cancel the job...
	*/

        cupsdLogMessage(CUPSD_LOG_WARN,
	                "Printer/class %s has gone away; cancelling job %d!",
	                job->dest, job->id);

	cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                      "Job canceled because the destination printer/class has gone away.");

        cupsdCancelJob(job, 1);
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

          ipp_attribute_t	*attr;	/* job-actual-printer-uri attribute */


          if ((attr = ippFindAttribute(job->attrs, "job-actual-printer-uri",
	                               IPP_TAG_URI)) != NULL)
            cupsdSetString(&attr->values[0].string.text, printer->uri);
	  else
	    ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI,
	                 "job-actual-printer-uri", NULL, printer->uri);
	}

        if (printer->state == IPP_PRINTER_IDLE ||	/* Printer is idle */
	    ((printer->type & CUPS_PRINTER_REMOTE) &&	/* Printer is remote */
	     !printer->job))				/* and not printing a job */
	  cupsdStartJob(job, printer);
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


  if (!MaxJobs)
    return;

  for (job = (cupsd_job_t *)cupsArrayFirst(Jobs);
       job && cupsArrayCount(Jobs) >= MaxJobs;
       job = (cupsd_job_t *)cupsArrayNext(Jobs))
    if (job->state->values[0].integer >= IPP_JOB_CANCELLED)
      cupsdCancelJob(job, 1);
}


/*
 * 'cupsdFinishJob()' - Finish a job.
 */

void
cupsdFinishJob(cupsd_job_t *job)	/* I - Job */
{
  int			job_history;	/* Did cupsdCancelJob() keep the job? */
  cupsd_printer_t	*printer;	/* Current printer */


  cupsdLogMessage(CUPSD_LOG_DEBUG,
                  "cupsdFinishJob: job %d, file %d is complete.",
                  job->id, job->current_file - 1);

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdFinishJob: job->status is %d",
                  job->status);

  if (job->status_buffer && job->current_file >= job->num_files)
  {
   /*
    * Close the pipe and clear the input bit.
    */

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdFinishJob: Removing fd %d from InputSet...",
                    job->status_buffer->fd);

    FD_CLR(job->status_buffer->fd, InputSet);

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdFinishJob: Closing status input pipe %d...",
                    job->status_buffer->fd);

    cupsdStatBufDelete(job->status_buffer);

    job->status_buffer = NULL;
  }

  printer = job->printer;

  if (job->status < 0)
  {
   /*
    * Backend had errors; stop it...
    */

    switch (-job->status)
    {
      default :
      case CUPS_BACKEND_FAILED :
         /*
	  * Backend failure, use the error-policy to determine how to
	  * act...
	  */

	  cupsdStopJob(job, 0);
	  job->state->values[0].integer = IPP_JOB_PENDING;
	  cupsdSaveJob(job);

	 /*
	  * If the job was queued to a class, try requeuing it...  For
	  * faxes and retry-job queues, hold the current job for 5 minutes.
	  */

	  if (job->dtype & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT))
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

	      cupsdLogMessage(CUPSD_LOG_ERROR,
	                      "Canceling job %d since it could not be sent after %d tries.",
	        	      job->id, JobRetryLimit);

	      cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                	    "Job canceled since it could not be sent after %d tries.",
			    JobRetryLimit);

	      cupsdCancelJob(job, 0);
	    }
	    else
	    {
	     /*
	      * Try again in N seconds...
	      */

	      set_hold_until(job, time(NULL) + JobRetryInterval);
	    }
	  }
	  else if (!strcmp(printer->error_policy, "abort-job"))
	    cupsdCancelJob(job, 0);
          break;

      case CUPS_BACKEND_CANCEL :
         /*
	  * Cancel the job...
	  */

	  cupsdCancelJob(job, 0);
          break;

      case CUPS_BACKEND_HOLD :
         /*
	  * Hold the job...
	  */

	  cupsdStopJob(job, 0);
	  cupsdSetJobHoldUntil(job, "indefinite");
	  cupsdSaveJob(job);
          break;

      case CUPS_BACKEND_STOP :
         /*
	  * Stop the printer...
	  */

	  cupsdStopJob(job, 0);
	  cupsdSaveJob(job);
	  cupsdSetPrinterState(printer, IPP_PRINTER_STOPPED, 1);
          break;

      case CUPS_BACKEND_AUTH_REQUIRED :
	  cupsdStopJob(job, 0);
	  cupsdSetJobHoldUntil(job, "authenticated");
	  cupsdSaveJob(job);

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

    cupsdStopJob(job, 1);
    cupsdSaveJob(job);
    cupsdAddEvent(CUPSD_EVENT_JOB_STOPPED, job->printer, job,
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
      cupsdStartJob(job, job->printer);
    }
    else
    {
     /*
      * Close out this job...
      */

      cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                    "Job completed successfully.");

      job_history = JobHistory && !(job->dtype & CUPS_PRINTER_REMOTE);

      cupsdCancelJob(job, 0);

      if (job_history)
      {
        job->state->values[0].integer = IPP_JOB_COMPLETED;
	cupsdSaveJob(job);
      }

     /*
      * Clear the printer's state_message and move on...
      */

      printer->state_message[0] = '\0';

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


  cupsdHoldSignals();

  cupsdStopAllJobs();

  for (job = (cupsd_job_t *)cupsArrayFirst(Jobs);
       job;
       job = (cupsd_job_t *)cupsArrayNext(Jobs))
  {
    cupsArrayRemove(Jobs, job);
    cupsArrayRemove(ActiveJobs, job);

    ippDelete(job->attrs);

    if (job->num_files > 0)
    {
      free(job->compressions);
      free(job->filetypes);
    }

    free(job);
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
    if (!strcasecmp(job->dest, dest))
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
  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdHoldJob: id = %d", job->id);

  if (job->state->values[0].integer == IPP_JOB_PROCESSING)
    cupsdStopJob(job, 0);

  DEBUG_puts("cupsdHoldJob: setting state to held...");

  job->state->values[0].integer = IPP_JOB_HELD;

  cupsdSaveJob(job);

  cupsdCheckJobs();
}


/*
 * 'cupsdLoadAllJobs()' - Load all jobs from disk.
 */

void
cupsdLoadAllJobs(void)
{
  cups_dir_t		*dir;		/* Directory */
  cups_dentry_t		*dent;		/* Directory entry */
  char			filename[1024];	/* Full filename of job file */
  cups_file_t		*fp;		/* Job file */
  cupsd_job_t		*job;		/* New job */
  int			jobid,		/* Current job ID */
			fileid;		/* Current file ID */
  ipp_attribute_t	*attr;		/* Job attribute */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  const char		*dest;		/* Destination */
  mime_type_t		**filetypes;	/* New filetypes array */
  int			*compressions;	/* New compressions array */


 /*
  * First create the job lists...
  */

  if (!Jobs)
    Jobs = cupsArrayNew(compare_jobs, NULL);

  if (!ActiveJobs)
    ActiveJobs = cupsArrayNew(compare_active_jobs, NULL);

 /*
  * Then open the requests directory...
  */

  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdLoadAllJobs: Scanning %s...",
                  RequestRoot);

  if ((dir = cupsDirOpen(RequestRoot)) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "cupsdLoadAllJobs: Unable to open spool directory %s: %s",
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
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "cupsdLoadAllJobs: Ran out of memory for jobs!");
	cupsDirClose(dir);
	return;
      }

      if ((job->attrs = ippNew()) == NULL)
      {
        free(job);
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "cupsdLoadAllJobs: Ran out of memory for job attributes!");
	cupsDirClose(dir);
	return;
      }

     /*
      * Assign the job ID...
      */

      job->id             = atoi(dent->filename + 1);
      job->back_pipes[0]  = -1;
      job->back_pipes[1]  = -1;
      job->print_pipes[0] = -1;
      job->print_pipes[1] = -1;

      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "cupsdLoadAllJobs: Loading attributes for job %d...",
                      job->id);

      if (job->id >= NextJobId)
        NextJobId = job->id + 1;

     /*
      * Load the job control file...
      */

      snprintf(filename, sizeof(filename), "%s/%s", RequestRoot, dent->filename);
      if ((fp = cupsFileOpen(filename, "r")) == NULL)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "cupsdLoadAllJobs: Unable to open job control file "
			"\"%s\" - %s!",
	                filename, strerror(errno));
	ippDelete(job->attrs);
	free(job);
	unlink(filename);
	continue;
      }
      else
      {
        if (ippReadIO(fp, (ipp_iocb_t)cupsFileRead, 1, NULL,
	              job->attrs) != IPP_DATA)
	{
          cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "cupsdLoadAllJobs: Unable to read job control file "
			  "\"%s\"!",
	                  filename);
	  cupsFileClose(fp);
	  ippDelete(job->attrs);
	  free(job);
	  unlink(filename);
	  continue;
	}

	cupsFileClose(fp);
      }

      if ((job->state = ippFindAttribute(job->attrs, "job-state", IPP_TAG_ENUM)) == NULL)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "cupsdLoadAllJobs: Missing or bad job-state attribute "
			"in control file \"%s\"!",
	                filename);
	ippDelete(job->attrs);
	free(job);
	unlink(filename);
	continue;
      }

      if ((attr = ippFindAttribute(job->attrs, "job-printer-uri", IPP_TAG_URI)) == NULL)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "cupsdLoadAllJobs: No job-printer-uri attribute in "
			"control file \"%s\"!",
	                filename);
	ippDelete(job->attrs);
	free(job);
	unlink(filename);
	continue;
      }

      httpSeparateURI(HTTP_URI_CODING_ALL, attr->values[0].string.text, method,
                      sizeof(method), username, sizeof(username), host,
		      sizeof(host), &port, resource, sizeof(resource));

      if ((dest = cupsdValidateDest(host, resource, &(job->dtype),
                                    NULL)) == NULL)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "cupsdLoadAllJobs: Unable to queue job for destination "
			"\"%s\"!",
	                attr->values[0].string.text);
	ippDelete(job->attrs);
	free(job);
	unlink(filename);
	continue;
      }

      cupsdSetString(&job->dest, dest);

      job->sheets     = ippFindAttribute(job->attrs, "job-media-sheets-completed",
                                         IPP_TAG_INTEGER);
      job->job_sheets = ippFindAttribute(job->attrs, "job-sheets", IPP_TAG_NAME);

      if ((attr = ippFindAttribute(job->attrs, "job-priority",
                                   IPP_TAG_INTEGER)) == NULL)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "cupsdLoadAllJobs: Missing or bad job-priority "
			"attribute in control file \"%s\"!",
	                filename);
	ippDelete(job->attrs);
	free(job);
	unlink(filename);
	continue;
      }
      job->priority = attr->values[0].integer;

      if ((attr = ippFindAttribute(job->attrs, "job-originating-user-name",
                                   IPP_TAG_NAME)) == NULL)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "cupsdLoadAllJobs: Missing or bad "
			"job-originating-user-name attribute in control file "
			"\"%s\"!",
	                filename);
	ippDelete(job->attrs);
	free(job);
	unlink(filename);
	continue;
      }
      cupsdSetString(&job->username, attr->values[0].string.text);

     /*
      * Insert the job into the array, sorting by job priority and ID...
      */

      cupsArrayAdd(Jobs, job);
      if (job->state->values[0].integer < IPP_JOB_STOPPED)
        cupsArrayAdd(ActiveJobs,job);

     /*
      * Set the job hold-until time and state...
      */

      if (job->state->values[0].integer == IPP_JOB_HELD)
      {
	if ((attr = ippFindAttribute(job->attrs, "job-hold-until",
	                             IPP_TAG_KEYWORD)) == NULL)
          attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

        if (attr == NULL)
          job->state->values[0].integer = IPP_JOB_PENDING;
	else
          cupsdSetJobHoldUntil(job, attr->values[0].string.text);
      }
      else if (job->state->values[0].integer == IPP_JOB_PROCESSING)
        job->state->values[0].integer = IPP_JOB_PENDING;
    }

 /*
  * Read all the d##### files...
  */

  cupsDirRewind(dir);

  while ((dent = cupsDirRead(dir)) != NULL)
    if (strlen(dent->filename) > 7 && dent->filename[0] == 'd' &&
        strchr(dent->filename, '-'))
    {
     /*
      * Find the job...
      */

      jobid  = atoi(dent->filename + 1);
      fileid = atoi(strchr(dent->filename, '-') + 1);

      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "cupsdLoadAllJobs: Auto-typing document file %s...",
                      dent->filename);

      snprintf(filename, sizeof(filename), "%s/%s", RequestRoot, dent->filename);

      if ((job = cupsdFindJob(jobid)) == NULL)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "cupsdLoadAllJobs: Orphaned print file \"%s\"!",
	                filename);
        unlink(filename);
	continue;
      }

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
	                                         sizeof(mime_type_t *) * fileid);
        }

        if (compressions == NULL || filetypes == NULL)
	{
          cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "cupsdLoadAllJobs: Ran out of memory for job file "
			  "types!");
	  continue;
	}

        job->compressions = compressions;
        job->filetypes    = filetypes;
	job->num_files    = fileid;
      }

      job->filetypes[fileid - 1] = mimeFileType(MimeDatabase, filename, NULL,
                                                job->compressions + fileid - 1);

      if (job->filetypes[fileid - 1] == NULL)
        job->filetypes[fileid - 1] = mimeType(MimeDatabase, "application",
	                                      "vnd.cups-raw");
    }

  cupsDirClose(dir);

 /*
  * Clean out old jobs as needed...
  */

  cupsdCleanJobs();
}


/*
 * 'cupsdMoveJob()' - Move the specified job to a different destination.
 */

void
cupsdMoveJob(cupsd_job_t *job,		/* I - Job */
             const char  *dest)		/* I - Destination */
{
  ipp_attribute_t	*attr;		/* job-printer-uri attribute */
  cupsd_printer_t	*p;		/* Destination printer or class */


 /*
  * Find the printer...
  */

  if ((p = cupsdFindDest(dest)) == NULL)
    return;

 /*
  * Don't move completed jobs...
  */

  if (job->state->values[0].integer >= IPP_JOB_PROCESSING)
    return;

 /*
  * Change the destination information...
  */

  cupsdSetString(&job->dest, dest);
  job->dtype = p->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_REMOTE |
                          CUPS_PRINTER_IMPLICIT);

  if ((attr = ippFindAttribute(job->attrs, "job-printer-uri", IPP_TAG_URI)) != NULL)
    cupsdSetString(&(attr->values[0].string.text), p->uri);

  cupsdSaveJob(job);
}


/*
 * 'cupsdReleaseJob()' - Release the specified job.
 */

void
cupsdReleaseJob(cupsd_job_t *job)	/* I - Job */
{
  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdReleaseJob: id = %d", job->id);

  if (job->state->values[0].integer == IPP_JOB_HELD)
  {
    DEBUG_puts("cupsdReleaseJob: setting state to pending...");

    job->state->values[0].integer = IPP_JOB_PENDING;
    cupsdSaveJob(job);
    cupsdCheckJobs();
  }
}


/*
 * 'cupsdRestartJob()' - Restart the specified job.
 */

void
cupsdRestartJob(cupsd_job_t *job)	/* I - Job */
{
  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdRestartJob: id = %d", job->id);

  if (job->state->values[0].integer == IPP_JOB_STOPPED || JobFiles)
  {
    job->tries = 0;
    job->state->values[0].integer = IPP_JOB_PENDING;
    cupsdSaveJob(job);
    cupsdCheckJobs();
  }
}


/*
 * 'cupsdSaveJob()' - Save a job to disk.
 */

void
cupsdSaveJob(cupsd_job_t *job)		/* I - Job */
{
  char		filename[1024];		/* Job control filename */
  cups_file_t	*fp;			/* Job file */


  snprintf(filename, sizeof(filename), "%s/c%05d", RequestRoot, job->id);

  if ((fp = cupsFileOpen(filename, "w")) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "cupsdSaveJob: Unable to create job control file "
		    "\"%s\" - %s.",
                    filename, strerror(errno));
    return;
  }

  fchmod(cupsFileNumber(fp), 0600);
  fchown(cupsFileNumber(fp), RunUser, Group);

  ippWriteIO(fp, (ipp_iocb_t)cupsFileWrite, 1, NULL, job->attrs);

  cupsFileClose(fp);
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


  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdSetJobHoldUntil(%d, \"%s\")",
                  job->id, when);

  second = 0;

  if (!strcmp(when, "indefinite") || !strcmp(when, "authenticated"))
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
  else if (!strcmp(when, "evening") || strcmp(when, "night"))
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
      job->hold_until += 24 * 60 * 60 * 60;
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdSetJobHoldUntil: hold_until = %d",
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

  if (job->state->values[0].integer >= IPP_JOB_PROCESSING)
    return;

 /*
  * Set the new priority and re-add the job into the active list...
  */

  cupsArrayRemove(ActiveJobs, job);

  job->priority = priority;

  if ((attr = ippFindAttribute(job->attrs, "job-priority", IPP_TAG_INTEGER)) != NULL)
    attr->values[0].integer = priority;
  else
    ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-priority",
                  priority);

  cupsArrayAdd(ActiveJobs, job);

  cupsdSaveJob(job);
}


/*
 * 'cupsdStartJob()' - Start a print job.
 */

void
cupsdStartJob(cupsd_job_t     *job,	/* I - Job ID */
              cupsd_printer_t *printer)	/* I - Printer to print job */
{
  int			i;		/* Looping var */
  int			slot;		/* Pipe slot */
  cups_array_t		*filters;	/* Filters for job */
  mime_filter_t		*filter,	/* Current filter */
			port_monitor;	/* Port monitor filter */
  char			method[255],	/* Method for output */
			*optptr,	/* Pointer to options */
			*valptr;	/* Pointer in value string */
  ipp_attribute_t	*attr;		/* Current attribute */
  struct stat		backinfo;	/* Backend file information */
  int			backroot;	/* Run backend as root? */
  int			pid;		/* Process ID of new filter process */
  int			banner_page;	/* 1 if banner page, 0 otherwise */
  int			statusfds[2],	/* Pipes used between the filters and scheduler */
			filterfds[2][2];/* Pipes used between the filters */
  int			envc;		/* Number of environment variables */
  char			*argv[8],	/* Filter command-line arguments */
			sani_uri[1024],	/* Sanitized DEVICE_URI env var */
			filename[1024],	/* Job filename */
			command[1024],	/* Full path to filter/backend command */
			jobid[255],	/* Job ID string */
			title[IPP_MAX_NAME],
					/* Job title string */
			copies[255],	/* # copies string */
			*envp[MAX_ENV],	/* Environment variables */
			charset[255],	/* CHARSET environment variable */
			class_name[255],/* CLASS environment variable */
			classification[1024],
					/* CLASSIFICATION environment variable */
			content_type[1024],
					/* CONTENT_TYPE environment variable */
			device_uri[1024],
					/* DEVICE_URI environment variable */
			lang[255],	/* LANG environment variable */
			ppd[1024],	/* PPD environment variable */
			printer_name[255],
					/* PRINTER environment variable */
			rip_max_cache[255];
					/* RIP_MAX_CACHE environment variable */
  static char		*options = NULL;/* Full list of options */
  static int		optlength = 0;	/* Length of option buffer */


  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdStartJob() id = %d, file = %d/%d",
                  job->id, job->current_file, job->num_files);

  if (job->num_files == 0)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Job ID %d has no files!  Cancelling it!",
                    job->id);

    cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                  "Job canceled because it has no files.");

    cupsdCancelJob(job, 0);
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

    cupsdLogMessage(CUPSD_LOG_DEBUG,
                    "cupsdStartJob: Sending job to queue tagged as raw...");

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
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to convert file %d to printable format for job %d!",
	              job->current_file, job->id);
      cupsdLogMessage(CUPSD_LOG_INFO,
                      "Hint: Do you have ESP Ghostscript installed?");

      if (LogLevel < CUPSD_LOG_DEBUG)
        cupsdLogMessage(CUPSD_LOG_INFO,
	                "Hint: Try setting the LogLevel to \"debug\".");

      job->current_file ++;

      if (job->current_file == job->num_files)
      {
	cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                      "Job canceled because it has no files that can be printed.");

        cupsdCancelJob(job, 0);
      }

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
  }

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

    cupsdLogMessage(CUPSD_LOG_INFO,
                    "Holding job %d because filter limit has been reached.",
                    job->id);
    cupsdLogMessage(CUPSD_LOG_DEBUG,
                    "cupsdStartJob: id=%d, file=%d, cost=%d, level=%d, limit=%d",
                    job->id, job->current_file, job->cost, FilterLevel,
		    FilterLimit);
    return;
  }

  FilterLevel += job->cost;

 /*
  * Add decompression filters, if any...
  */

  if (job->compressions[job->current_file])
  {
   /*
    * Add gziptoany filter to the front of the list...
    */

    if (!cupsArrayInsert(filters, &gziptoany_filter))
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to add decompression filter - %s",
                      strerror(errno));

      if (filters != NULL)
        free(filters);

      job->current_file ++;

      if (job->current_file == job->num_files)
      {
	cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                      "Job canceled because the print file could not be "
		      "decompressed.");

        cupsdCancelJob(job, 0);
      }

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

    if (!cupsArrayAdd(filters, &port_monitor))
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to add port monitor - %s",
                      strerror(errno));

      if (filters != NULL)
        free(filters);

      job->current_file ++;

      if (job->current_file == job->num_files)
      {
	cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                      "Job canceled because the port monitor could not be "
		      "added.");

        cupsdCancelJob(job, 0);
      }

      return;
    }

    snprintf(port_monitor.filter, sizeof(port_monitor.filter),
             "%s/monitor/%s", ServerBin, printer->port_monitor);
  }

 /*
  * Update the printer and job state to "processing"...
  */

  job->state->values[0].integer = IPP_JOB_PROCESSING;
  job->status  = 0;
  job->printer = printer;
  printer->job = job;
  cupsdSetPrinterState(printer, IPP_PRINTER_PROCESSING, 0);

  if (job->current_file == 0)
  {
    set_time(job, "time-at-processing");
    cupsdOpenPipe(job->back_pipes);
  }

 /*
  * Determine if we are printing a banner page or not...
  */

  if (job->job_sheets == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG, "No job-sheets attribute.");
    if ((job->job_sheets =
         ippFindAttribute(job->attrs, "job-sheets", IPP_TAG_ZERO)) != NULL)
      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "... but someone added one without setting job_sheets!");
  }
  else if (job->job_sheets->num_values == 1)
    cupsdLogMessage(CUPSD_LOG_DEBUG, "job-sheets=%s",
               job->job_sheets->values[0].string.text);
  else
    cupsdLogMessage(CUPSD_LOG_DEBUG, "job-sheets=%s,%s",
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

  cupsdLogMessage(CUPSD_LOG_DEBUG, "banner_page = %d", banner_page);

 /*
  * Building the options string is harder than it needs to be, but
  * for the moment we need to pass strings for command-line args and
  * not IPP attribute pointers... :)
  *
  * First allocate/reallocate the option buffer as needed...
  */

  i = ipp_length(job->attrs);

  if (i > optlength)
  {
    if (optlength == 0)
      optptr = malloc(i);
    else
      optptr = realloc(options, i);

    if (optptr == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_CRIT,
                      "cupsdStartJob: Unable to allocate %d bytes for option buffer for job %d!",
                      i, job->id);

      if (filters != NULL)
        free(filters);

      FilterLevel -= job->cost;

      cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                    "Job canceled because the server ran out of memory.");

      cupsdCancelJob(job, 0);
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
    if (strcmp(attr->name, "copies") == 0 &&
	attr->value_tag == IPP_TAG_INTEGER)
    {
     /*
      * Don't use the # copies attribute if we are printing the job sheets...
      */

      if (!banner_page)
        sprintf(copies, "%d", attr->values[0].integer);
    }
    else if (strcmp(attr->name, "job-name") == 0 &&
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
	  attr->value_tag == IPP_TAG_URI ||
	  attr->value_tag == IPP_TAG_URISCHEME ||
	  attr->value_tag == IPP_TAG_BEGIN_COLLECTION) /* Not yet supported */
	continue;

      if (strncmp(attr->name, "time-", 5) == 0)
	continue;

      if (strncmp(attr->name, "job-", 4) == 0 &&
          !(printer->type & CUPS_PRINTER_REMOTE))
	continue;

      if (strncmp(attr->name, "job-", 4) == 0 &&
          strcmp(attr->name, "job-billing") != 0 &&
          strcmp(attr->name, "job-sheets") != 0 &&
          strcmp(attr->name, "job-hold-until") != 0 &&
	  strcmp(attr->name, "job-priority") != 0)
	continue;

      if ((strcmp(attr->name, "page-label") == 0 ||
           strcmp(attr->name, "page-border") == 0 ||
           strncmp(attr->name, "number-up", 9) == 0 ||
	   strcmp(attr->name, "page-set") == 0) &&
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
  */

  sprintf(jobid, "%d", job->id);
  snprintf(filename, sizeof(filename), "%s/d%05d-%03d", RequestRoot,
           job->id, job->current_file + 1);

  argv[0] = printer->name;
  argv[1] = jobid;
  argv[2] = job->username;
  argv[3] = title;
  argv[4] = copies;
  argv[5] = options;
  argv[6] = filename;
  argv[7] = NULL;

  cupsdLogMessage(CUPSD_LOG_DEBUG,
                  "cupsdStartJob: argv = \"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"",
                  argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);

 /*
  * Create environment variable strings for the filters...
  */

  attr = ippFindAttribute(job->attrs, "attributes-natural-language",
                          IPP_TAG_LANGUAGE);

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

        snprintf(lang, sizeof(lang), "LANG=%s",
	         attr->values[0].string.text);
        break;

    case 5 :
       /*
        * Language and country code (ll-cc)...
	*/

        snprintf(lang, sizeof(lang), "LANG=%c%c_%c%c",
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
  snprintf(device_uri, sizeof(device_uri), "DEVICE_URI=%s", printer->device_uri);
  cupsdSanitizeURI(printer->device_uri, sani_uri, sizeof(sani_uri));
  snprintf(ppd, sizeof(ppd), "PPD=%s/ppd/%s.ppd", ServerRoot, printer->name);
  snprintf(printer_name, sizeof(printer_name), "PRINTER=%s", printer->name);
  snprintf(rip_max_cache, sizeof(rip_max_cache), "RIP_MAX_CACHE=%s", RIPCache);

  envc = cupsdLoadEnv(envp, (int)(sizeof(envp) / sizeof(envp[0])));

  envp[envc ++] = charset;
  envp[envc ++] = lang;
  envp[envc ++] = ppd;
  envp[envc ++] = rip_max_cache;
  envp[envc ++] = content_type;
  envp[envc ++] = device_uri;
  envp[envc ++] = printer_name;

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

  envp[envc] = NULL;

  for (i = 0; i < envc; i ++)
    if (strncmp(envp[i], "DEVICE_URI=", 11))
      cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdStartJob: envp[%d]=\"%s\"",
                      i, envp[i]);
    else
      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "cupsdStartJob: envp[%d]=\"DEVICE_URI=%s\"", i, sani_uri);

  job->current_file ++;

 /*
  * Now create processes for all of the filters...
  */

  if (cupsdOpenPipe(statusfds))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to create job status pipes - %s.",
	            strerror(errno));
    snprintf(printer->state_message, sizeof(printer->state_message),
             "Unable to create status pipes - %s.", strerror(errno));

    cupsdAddPrinterHistory(printer);

    if (filters != NULL)
      free(filters);

    cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                  "Job canceled because the server could not create the job status pipes.");

    cupsdCancelJob(job, 0);
    return;
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdStartJob: statusfds = [ %d %d ]",
                  statusfds[0], statusfds[1]);

#ifdef FD_CLOEXEC
  fcntl(statusfds[0], F_SETFD, FD_CLOEXEC);
  fcntl(statusfds[1], F_SETFD, FD_CLOEXEC);
#endif /* FD_CLOEXEC */

  job->status_buffer = cupsdStatBufNew(statusfds[0], "[Job %d]",
                                           job->id);
  job->status        = 0;
  memset(job->filters, 0, sizeof(job->filters));

  filterfds[1][0] = open("/dev/null", O_RDONLY);
  filterfds[1][1] = -1;

  if (filterfds[1][0] < 0)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to open \"/dev/null\" - %s.",
                    strerror(errno));
    snprintf(printer->state_message, sizeof(printer->state_message),
             "Unable to open \"/dev/null\" - %s.", strerror(errno));

    cupsdAddPrinterHistory(printer);

    if (filters != NULL)
      free(filters);

    cupsdClosePipe(statusfds);
    cupsdCancelJob(job, 0);
    return;
  }

  fcntl(filterfds[1][0], F_SETFD, fcntl(filterfds[1][0], F_GETFD) | FD_CLOEXEC);

  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdStartJob: filterfds[%d] = [ %d %d ]",
                  1, filterfds[1][0], filterfds[1][1]);

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
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Unable to create job filter pipes - %s.",
		        strerror(errno));
	snprintf(printer->state_message, sizeof(printer->state_message),
		"Unable to create filter pipes - %s.", strerror(errno));
	cupsdAddPrinterHistory(printer);

	cupsArrayDelete(filters);

	cupsdClosePipe(statusfds);
	cupsdClosePipe(filterfds[!slot]);

	cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                      "Job canceled because the server could not create the filter pipes.");

	cupsdCancelJob(job, 0);
	return;
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
	    cupsdLogMessage(CUPSD_LOG_ERROR,
	                    "Unable to create job backend pipes - %s.",
		            strerror(errno));
	    snprintf(printer->state_message, sizeof(printer->state_message),
		    "Unable to create backend pipes - %s.", strerror(errno));
	    cupsdAddPrinterHistory(printer);

	    cupsArrayDelete(filters);

	    cupsdClosePipe(statusfds);
	    cupsdClosePipe(filterfds[!slot]);

	    cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                	  "Job canceled because the server could not create the backend pipes.");

	    cupsdCancelJob(job, 0);
	    return;
	  }
	}
	else
	{
	  job->print_pipes[0] = -1;
	  if (!strncmp(printer->device_uri, "file:/dev/", 10) &&
	      strcmp(printer->device_uri, "file:/dev/null"))
	    job->print_pipes[1] = open(printer->device_uri + 5,
	                               O_WRONLY | O_EXCL);
	  else if (!strncmp(printer->device_uri, "file:///dev/", 12) &&
	           strcmp(printer->device_uri, "file:///dev/null"))
	    job->print_pipes[1] = open(printer->device_uri + 7,
	                               O_WRONLY | O_EXCL);
	  else
	    job->print_pipes[1] = open(printer->device_uri + 5,
	                               O_WRONLY | O_CREAT | O_TRUNC, 0600);

	  if (job->print_pipes[1] < 0)
	  {
            cupsdLogMessage(CUPSD_LOG_ERROR,
	                    "Unable to open output file \"%s\" - %s.",
	                    printer->device_uri, strerror(errno));
            snprintf(printer->state_message, sizeof(printer->state_message),
		     "Unable to open output file \"%s\" - %s.",
	             printer->device_uri, strerror(errno));

	    cupsdAddPrinterHistory(printer);

	    cupsArrayDelete(filters);

	    cupsdClosePipe(statusfds);
	    cupsdClosePipe(filterfds[!slot]);

	    cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                	  "Job canceled because the server could not open the output file.");

	    cupsdCancelJob(job, 0);
	    return;
	  }

	  fcntl(job->print_pipes[1], F_SETFD,
        	fcntl(job->print_pipes[1], F_GETFD) | FD_CLOEXEC);
	}

	cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdStartJob: print_pipes = [ %d %d ]",
                   job->print_pipes[0], job->print_pipes[1]);
      }

      filterfds[slot][0] = job->print_pipes[0];
      filterfds[slot][1] = job->print_pipes[1];
    }

    cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdStartJob: filter = \"%s\"", command);
    cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdStartJob: filterfds[%d] = [ %d %d ]",
                    slot, filterfds[slot][0], filterfds[slot][1]);

    pid = cupsdStartProcess(command, argv, envp, filterfds[!slot][0],
                            filterfds[slot][1], statusfds[1],
		            job->back_pipes[0], 0, job->filters + i);

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdStartJob: Closing filter pipes for slot %d [ %d %d ]...",
                    !slot, filterfds[!slot][0], filterfds[!slot][1]);

    cupsdClosePipe(filterfds[!slot]);

    if (pid == 0)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to start filter \"%s\" - %s.",
                      filter->filter, strerror(errno));
      snprintf(printer->state_message, sizeof(printer->state_message),
               "Unable to start filter \"%s\" - %s.",
               filter->filter, strerror(errno));

      cupsdAddPrinterHistory(printer);

      cupsArrayDelete(filters);

      cupsdAddPrinterHistory(printer);

      cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                    "Job canceled because the server could not execute a filter.");

      cupsdCancelJob(job, 0);
      return;
    }

    cupsdLogMessage(CUPSD_LOG_INFO, "Started filter %s (PID %d) for job %d.",
                    command, pid, job->id);

    argv[6] = NULL;
    slot    = !slot;
  }

  cupsArrayDelete(filters);

 /*
  * Finally, pipe the final output into a backend process if needed...
  */

  if (strncmp(printer->device_uri, "file:", 5) != 0)
  {
    if (job->current_file == 1)
    {
      sscanf(printer->device_uri, "%254[^:]", method);
      snprintf(command, sizeof(command), "%s/backend/%s", ServerBin, method);

     /*
      * See if the backend needs to run as root...
      */

      if (RunUser)
        backroot = 0;
      else if (lstat(command, &backinfo))
	backroot = 0;
      else
        backroot = !(backinfo.st_mode & (S_IRWXG | S_IRWXO));

      argv[0] = sani_uri;

      filterfds[slot][0] = -1;
      filterfds[slot][1] = open("/dev/null", O_WRONLY);

      if (filterfds[slot][1] < 0)
      {
	cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to open \"/dev/null\" - %s.",
	                strerror(errno));
	snprintf(printer->state_message, sizeof(printer->state_message),
        	 "Unable to open \"/dev/null\" - %s.", strerror(errno));

	cupsdAddPrinterHistory(printer);

	cupsdClosePipe(statusfds);

	cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                      "Job canceled because the server could not open a file.");

	cupsdCancelJob(job, 0);
	return;
      }

      fcntl(filterfds[slot][1], F_SETFD,
            fcntl(filterfds[slot][1], F_GETFD) | FD_CLOEXEC);

      cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdStartJob: backend = \"%s\"",
                      command);
      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "cupsdStartJob: filterfds[%d] = [ %d %d ]",
        	      slot, filterfds[slot][0], filterfds[slot][1]);

      pid = cupsdStartProcess(command, argv, envp, filterfds[!slot][0],
			      filterfds[slot][1], statusfds[1],
			      job->back_pipes[1], backroot,
			      &(job->backend));

      if (pid == 0)
      {
	cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to start backend \"%s\" - %s.",
                        method, strerror(errno));
	snprintf(printer->state_message, sizeof(printer->state_message),
        	 "Unable to start backend \"%s\" - %s.", method, strerror(errno));

	cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                "cupsdStartJob: Closing print pipes [ %d %d ]...",
        	        job->print_pipes[0], job->print_pipes[1]);

        cupsdClosePipe(job->print_pipes);

	cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                "cupsdStartJob: Closing back pipes [ %d %d ]...",
        	        job->back_pipes[0], job->back_pipes[1]);

        cupsdClosePipe(job->back_pipes);

	cupsdAddEvent(CUPSD_EVENT_JOB_COMPLETED, job->printer, job,
                      "Job canceled because the server could not execute the backend.");

        cupsdCancelJob(job, 0);
	return;
      }
      else
      {
	cupsdLogMessage(CUPSD_LOG_INFO,
	                "Started backend %s (PID %d) for job %d.",
	                command, pid, job->id);
      }
    }

    if (job->current_file == job->num_files)
    {
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdStartJob: Closing print pipes [ %d %d ]...",
        	      job->print_pipes[0], job->print_pipes[1]);

      cupsdClosePipe(job->print_pipes);

      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdStartJob: Closing back pipes [ %d %d ]...",
        	      job->back_pipes[0], job->back_pipes[1]);

      cupsdClosePipe(job->back_pipes);
    }
  }
  else
  {
    filterfds[slot][0] = -1;
    filterfds[slot][1] = -1;

    if (job->current_file == job->num_files)
    {
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdStartJob: Closing print pipes [ %d %d ]...",
        	      job->print_pipes[0], job->print_pipes[1]);

      cupsdClosePipe(job->print_pipes);
    }
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdStartJob: Closing filter pipes for slot %d [ %d %d ]...",
                  slot, filterfds[slot][0], filterfds[slot][1]);

  cupsdClosePipe(filterfds[slot]);

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdStartJob: Closing status output pipe %d...",
                  statusfds[1]);

  close(statusfds[1]);

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdStartJob: Adding fd %d to InputSet...",
                  job->status_buffer->fd);

  FD_SET(job->status_buffer->fd, InputSet);

  cupsdAddEvent(CUPSD_EVENT_JOB_STATE, job->printer, job, "Job #%d started.",
                job->id);
}


/*
 * 'cupsdStopAllJobs()' - Stop all print jobs.
 */

void
cupsdStopAllJobs(void)
{
  cupsd_job_t	*job;			/* Current job */


  DEBUG_puts("cupsdStopAllJobs()");

  for (job = (cupsd_job_t *)cupsArrayFirst(ActiveJobs);
       job;
       job = (cupsd_job_t *)cupsArrayNext(ActiveJobs))
    if (job->state->values[0].integer == IPP_JOB_PROCESSING)
    {
      cupsdStopJob(job, 1);
      job->state->values[0].integer = IPP_JOB_PENDING;
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


  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdStopJob: id = %d, force = %d",
                  job->id, force);

  if (job->state->values[0].integer != IPP_JOB_PROCESSING)
    return;

  FilterLevel -= job->cost;

  if (job->status < 0 &&
      !(job->dtype & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT)) &&
      !(job->printer->type & CUPS_PRINTER_FAX) &&
      !strcmp(job->printer->error_policy, "stop-printer"))
    cupsdSetPrinterState(job->printer, IPP_PRINTER_STOPPED, 1);
  else if (job->printer->state != IPP_PRINTER_STOPPED)
    cupsdSetPrinterState(job->printer, IPP_PRINTER_IDLE, 0);

  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdStopJob: printer state is %d",
                  job->printer->state);

  job->state->values[0].integer = IPP_JOB_STOPPED;
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

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdStopJob: Closing print pipes [ %d %d ]...",
                  job->print_pipes[0], job->print_pipes[1]);

  cupsdClosePipe(job->print_pipes);

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdStopJob: Closing back pipes [ %d %d ]...",
                  job->back_pipes[0], job->back_pipes[1]);

  cupsdClosePipe(job->back_pipes);

  if (job->status_buffer)
  {
   /*
    * Close the pipe and clear the input bit.
    */

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdStopJob: Removing fd %d from InputSet...",
	            job->status_buffer->fd);

    FD_CLR(job->status_buffer->fd, InputSet);

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdStopJob: Closing status input pipe %d...",
                    job->status_buffer->fd);

    cupsdStatBufDelete(job->status_buffer);

    job->status_buffer = NULL;
  }
}


/*
 * 'cupsdUpdateJob()' - Read a status update from a job's filters.
 */

void
cupsdUpdateJob(cupsd_job_t *job)	/* I - Job to check */
{
  int		i;			/* Looping var */
  int		copies;			/* Number of copies printed */
  char		message[1024],		/* Message text */
		*ptr;			/* Pointer update... */
  int		loglevel;		/* Log level for message */


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

      if (job->sheets != NULL)
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
	  cupsdUpdateQuota(job->printer, job->username, copies, 0);
      }

      cupsdLogPage(job, message);

      cupsdAddEvent(CUPSD_EVENT_JOB_PROGRESS, job->printer, job,
                    "Printed %d page(s).", job->sheets->values[0].integer);
    }
    else if (loglevel == CUPSD_LOG_STATE)
    {
      cupsdSetPrinterReasons(job->printer, message);
      cupsdAddPrinterHistory(job->printer);
    }
    else if (loglevel == CUPSD_LOG_ATTR)
    {
     /*
      * Set attribute(s)...
      */

      /**** TODO ****/
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
    }
#endif /* __APPLE__ */
    else
    {
     /*
      * Some message to show in the printer-state-message attribute...
      */

      strlcpy(job->printer->state_message, message,
              sizeof(job->printer->state_message));
      cupsdAddPrinterHistory(job->printer);
    }

    if (!strchr(job->status_buffer->buffer, '\n'))
      break;
  }

  if (ptr == NULL)
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
 * 'compare_active_jobs()' - Compare the job IDs and priorities of two jobs.
 */

static int				/* O - Difference */
compare_active_jobs(void *first,	/* I - First job */
                    void *second,	/* I - Second job */
		    void *data)		/* I - App data (not used) */
{
  int	diff;				/* Difference */


  if ((diff = ((cupsd_job_t *)first)->priority - ((cupsd_job_t *)second)->priority) != 0)
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

int					/* O - Size of buffer to hold IPP attributes */
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

  cupsdLogMessage(CUPSD_LOG_DEBUG, "set_hold_until: hold_until = %d", (int)holdtime);

  job->state->values[0].integer = IPP_JOB_HELD;
  job->hold_until               = holdtime;

 /*
  * Update the job-hold-until attribute with a string representing GMT
  * time (HH:MM:SS)...
  */

  holddate = gmtime(&holdtime);
  snprintf(holdstr, sizeof(holdstr), "%d:%d:%d", holddate->tm_hour, 
	   holddate->tm_min, holddate->tm_sec);

  if ((attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_KEYWORD)) == NULL)
    attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

 /*
  * Either add the attribute or update the value of the existing one
  */

  if (attr == NULL)
    attr = ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_KEYWORD,
                        "job-hold-until", NULL, holdstr);
  else
    cupsdSetString(&attr->values[0].string.text, holdstr);

  cupsdSaveJob(job);
}


/*
 * End of "$Id$".
 */
