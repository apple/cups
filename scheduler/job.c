/*
 * "$Id: job.c,v 1.124.2.86 2004/06/29 03:28:08 mike Exp $"
 *
 *   Job management routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636-3142 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   AddJob()             - Add a new job to the job queue...
 *   CancelJob()          - Cancel the specified print job.
 *   CancelJobs()         - Cancel all jobs for the given destination/user...
 *   CheckJobs()          - Check the pending jobs and start any if the
 *                          destination is available.
 *   CleanJobs()          - Clean out old jobs.
 *   FreeAllJobs()        - Free all jobs from memory.
 *   FindJob()            - Find the specified job.
 *   GetPrinterJobCount() - Get the number of pending, processing,
 *                          or held jobs in a printer or class.
 *   GetUserJobCount()    - Get the number of pending, processing,
 *                          or held jobs for a user.
 *   HoldJob()            - Hold the specified job.
 *   LoadAllJobs()        - Load all jobs from disk.
 *   LoadJob()            - Load a job from disk.
 *   MoveJob()            - Move the specified job to a different
 *                          destination.
 *   ReleaseJob()         - Release the specified job.
 *   RestartJob()         - Restart the specified job.
 *   SaveJob()            - Save a job to disk.
 *   SetJobHoldUntil()    - Set the hold time for a job...
 *   SetJobPriority()     - Set the priority of a job, moving it up/down
 *                          in the list as needed.
 *   StartJob()           - Start a print job.
 *   StopAllJobs()        - Stop all print jobs.
 *   StopJob()            - Stop a print job.
 *   UpdateJob()          - Read a status update from a job's filters.
 *   ipp_length()         - Compute the size of the buffer needed to hold 
 *		            the textual IPP attributes.
 *   start_process()      - Start a background process.
 *   set_hold_until()     - Set the hold time and update job-hold-until attribute.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <grp.h>


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

static int		ipp_length(ipp_t *ipp);
static void		set_time(job_t *job, const char *name);
static int		start_process(const char *command, char *argv[],
			              char *envp[], int infd, int outfd,
				      int errfd, int backfd, int root,
				      int *pid);
static void		set_hold_until(job_t *job, time_t holdtime);


/*
 * 'AddJob()' - Add a new job to the job queue...
 */

job_t *				/* O - New job record */
AddJob(int        priority,	/* I - Job priority */
       const char *dest)	/* I - Job destination */
{
  job_t	*job,			/* New job record */
	*current,		/* Current job in queue */
	*prev;			/* Previous job in queue */


  job = calloc(sizeof(job_t), 1);

  job->id             = NextJobId ++;
  job->priority       = priority;
  job->back_pipes[0]  = -1;
  job->back_pipes[1]  = -1;
  job->print_pipes[0] = -1;
  job->print_pipes[1] = -1;
  job->status_pipe    = -1;

  SetString(&job->dest, dest);

  NumJobs ++;

  for (current = Jobs, prev = NULL;
       current != NULL;
       prev = current, current = current->next)
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
CancelJob(int id,		/* I - Job to cancel */
          int purge)		/* I - Purge jobs? */
{
  int	i;			/* Looping var */
  job_t	*current,		/* Current job */
	*prev;			/* Previous job in list */
  char	filename[1024];		/* Job filename */


  LogMessage(L_DEBUG, "CancelJob: id = %d", id);

  for (current = Jobs, prev = NULL; current != NULL; prev = current, current = current->next)
    if (current->id == id)
    {
     /*
      * Stop any processes that are working on the current...
      */

      DEBUG_puts("CancelJob: found job in list.");

      if (current->state->values[0].integer == IPP_JOB_PROCESSING)
	StopJob(current->id, 0);

      current->state->values[0].integer = IPP_JOB_CANCELLED;

      set_time(current, "time-at-completed");

     /*
      * Remove the print file for good if we aren't preserving jobs or
      * files...
      */

      current->current_file = 0;

      if (!JobHistory || !JobFiles || purge ||
          (current->dtype & CUPS_PRINTER_REMOTE))
        for (i = 1; i <= current->num_files; i ++)
	{
	  snprintf(filename, sizeof(filename), "%s/d%05d-%03d", RequestRoot,
	           current->id, i);
          unlink(filename);
	}

      if (JobHistory && !purge && !(current->dtype & CUPS_PRINTER_REMOTE))
      {
       /*
        * Save job state info...
	*/

        SaveJob(current->id);
      }
      else
      {
       /*
        * Remove the job info file...
	*/

	snprintf(filename, sizeof(filename), "%s/c%05d", RequestRoot,
	         current->id);
	unlink(filename);

       /*
        * Update pointers if we aren't preserving jobs...
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

        if (current->num_files > 0)
	{
          free(current->compressions);
          free(current->filetypes);
	}

        ClearString(&current->username);
        ClearString(&current->dest);

        free(current);

	NumJobs --;
      }

      return;
    }
}


/*
 * 'CancelJobs()' - Cancel all jobs for the given destination/user...
 */

void
CancelJobs(const char *dest,		/* I - Destination to cancel */
           const char *username,	/* I - Username or NULL */
	   int        purge)		/* I - Purge jobs? */
{
  job_t	*current,			/* Current job */
	*next;				/* Next job */


  for (current = Jobs; current != NULL;)
    if ((dest == NULL || !strcmp(current->dest, dest)) &&
        (username == NULL || !strcmp(current->username, username)))
    {
     /*
      * Cancel all jobs matching this destination/user...
      */

      next = current->next;

      CancelJob(current->id, purge);

      current = next;
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
  job_t		*current,	/* Current job in queue */
		*next;		/* Next job in queue */
  printer_t	*printer,	/* Printer destination */
		*pclass;	/* Printer class destination */


  DEBUG_puts("CheckJobs()");

  for (current = Jobs; current != NULL; current = next)
  {
   /*
    * Save next pointer in case the job is cancelled en-route.
    */

    next = current->next;

   /*
    * Start held jobs if they are ready...
    */

    if (current->state->values[0].integer == IPP_JOB_HELD &&
        current->hold_until &&
	current->hold_until < time(NULL))
      current->state->values[0].integer = IPP_JOB_PENDING;

   /*
    * Start pending jobs if the destination is available...
    */

    if (current->state->values[0].integer == IPP_JOB_PENDING && !NeedReload)
    {
      if ((pclass = FindClass(current->dest)) != NULL)
      {
       /*
        * If the class is remote, just pass it to the remote server...
	*/

        if (pclass->type & CUPS_PRINTER_REMOTE)
	  printer = pclass;
	else if (pclass->state != IPP_PRINTER_STOPPED)
	  printer = FindAvailablePrinter(current->dest);
	else
	  printer = NULL;
      }
      else
        printer = FindPrinter(current->dest);

      if (printer != NULL && (printer->type & CUPS_PRINTER_IMPLICIT))
      {
       /*
        * Handle implicit classes...
	*/

        pclass = printer;

	if (pclass->state != IPP_PRINTER_STOPPED)
	  printer = FindAvailablePrinter(current->dest);
	else
	  printer = NULL;
      }

      if (printer == NULL && pclass == NULL)
      {
       /*
        * Whoa, the printer and/or class for this destination went away;
	* cancel the job...
	*/

        LogMessage(L_WARN, "Printer/class %s has gone away; cancelling job %d!",
	           current->dest, current->id);
        CancelJob(current->id, 1);
      }
      else if (printer != NULL)
      {
       /*
        * See if the printer is available or remote and not printing a job;
	* if so, start the job...
	*/

        if (printer->state == IPP_PRINTER_IDLE ||	/* Printer is idle */
	    ((printer->type & CUPS_PRINTER_REMOTE) &&	/* Printer is remote */
	     !printer->job))				/* and not printing a job */
	  StartJob(current->id, printer);
      }
    }
  }
}


/*
 * 'CleanJobs()' - Clean out old jobs.
 */

void
CleanJobs(void)
{
  job_t	*job,		/* Current job */
	*next;		/* Next job */


  if (MaxJobs == 0)
    return;

  for (job = Jobs; job && NumJobs >= MaxJobs; job = next)
  {
    next = job->next;

    if (job->state->values[0].integer >= IPP_JOB_CANCELLED)
      CancelJob(job->id, 1);
  }
}


/*
 * 'FreeAllJobs()' - Free all jobs from memory.
 */

void
FreeAllJobs(void)
{
  job_t	*job,		/* Current job */
	*next;		/* Next job */


  HoldSignals();

  StopAllJobs();

  for (job = Jobs; job; job = next)
  {
    next = job->next;

    ippDelete(job->attrs);

    if (job->num_files > 0)
    {
      free(job->compressions);
      free(job->filetypes);
    }

    free(job);
  }

  Jobs = NULL;

  ReleaseSignals();
}


/*
 * 'FindJob()' - Find the specified job.
 */

job_t *				/* O - Job data */
FindJob(int id)			/* I - Job ID */
{
  job_t	*current;		/* Current job */


  for (current = Jobs; current != NULL; current = current->next)
    if (current->id == id)
      break;

  return (current);
}


/*
 * 'GetPrinterJobCount()' - Get the number of pending, processing,
 *                          or held jobs in a printer or class.
 */

int					/* O - Job count */
GetPrinterJobCount(const char *dest)	/* I - Printer or class name */
{
  int	count;				/* Job count */
  job_t	*job;				/* Current job */


  for (job = Jobs, count = 0; job != NULL; job = job->next)
    if (job->state->values[0].integer <= IPP_JOB_PROCESSING &&
        strcasecmp(job->dest, dest) == 0)
      count ++;

  return (count);
}


/*
 * 'GetUserJobCount()' - Get the number of pending, processing,
 *                       or held jobs for a user.
 */

int					/* O - Job count */
GetUserJobCount(const char *username)	/* I - Username */
{
  int	count;				/* Job count */
  job_t	*job;				/* Current job */


  for (job = Jobs, count = 0; job != NULL; job = job->next)
    if (job->state->values[0].integer <= IPP_JOB_PROCESSING &&
        strcmp(job->username, username) == 0)
      count ++;

  return (count);
}


/*
 * 'HoldJob()' - Hold the specified job.
 */

void
HoldJob(int id)			/* I - Job ID */
{
  job_t	*job;			/* Job data */


  LogMessage(L_DEBUG, "HoldJob: id = %d", id);

  if ((job = FindJob(id)) == NULL)
    return;

  if (job->state->values[0].integer == IPP_JOB_PROCESSING)
    StopJob(id, 0);

  DEBUG_puts("HoldJob: setting state to held...");

  job->state->values[0].integer = IPP_JOB_HELD;

  SaveJob(id);

  CheckJobs();
}


/*
 * 'LoadAllJobs()' - Load all jobs from disk.
 */

void
LoadAllJobs(void)
{
  DIR		*dir;		/* Directory */
  DIRENT	*dent;		/* Directory entry */
  char		filename[1024];	/* Full filename of job file */
  int		fd;		/* File descriptor */
  job_t		*job,		/* New job */
		*current,	/* Current job */
		*prev;		/* Previous job */
  int		jobid,		/* Current job ID */
		fileid;		/* Current file ID */
  ipp_attribute_t *attr;	/* Job attribute */
  char		method[HTTP_MAX_URI],
				/* Method portion of URI */
		username[HTTP_MAX_URI],
				/* Username portion of URI */
		host[HTTP_MAX_URI],
				/* Host portion of URI */
		resource[HTTP_MAX_URI];
				/* Resource portion of URI */
  int		port;		/* Port portion of URI */
  printer_t	*p;		/* Printer or class */
  const char	*dest;		/* Destination */
  mime_type_t	**filetypes;	/* New filetypes array */
  int		*compressions;	/* New compressions array */


 /*
  * First open the requests directory...
  */

  LogMessage(L_DEBUG, "LoadAllJobs: Scanning %s...", RequestRoot);

  NumJobs = 0;

  if ((dir = opendir(RequestRoot)) == NULL)
  {
    LogMessage(L_ERROR, "LoadAllJobs: Unable to open spool directory %s: %s",
               RequestRoot, strerror(errno));
    return;
  }

 /*
  * Read all the c##### files...
  */

  while ((dent = readdir(dir)) != NULL)
    if (NAMLEN(dent) >= 6 && dent->d_name[0] == 'c')
    {
     /*
      * Allocate memory for the job...
      */

      if ((job = calloc(sizeof(job_t), 1)) == NULL)
      {
        LogMessage(L_ERROR, "LoadAllJobs: Ran out of memory for jobs!");
	closedir(dir);
	return;
      }

      if ((job->attrs = ippNew()) == NULL)
      {
        free(job);
        LogMessage(L_ERROR, "LoadAllJobs: Ran out of memory for job attributes!");
	closedir(dir);
	return;
      }

     /*
      * Assign the job ID...
      */

      job->id = atoi(dent->d_name + 1);

      LogMessage(L_DEBUG, "LoadAllJobs: Loading attributes for job %d...\n",
                 job->id);

      if (job->id >= NextJobId)
        NextJobId = job->id + 1;

     /*
      * Load the job control file...
      */

      snprintf(filename, sizeof(filename), "%s/%s", RequestRoot, dent->d_name);
      if ((fd = open(filename, O_RDONLY)) < 0)
      {
        LogMessage(L_ERROR, "LoadAllJobs: Unable to open job control file \"%s\" - %s!",
	           filename, strerror(errno));
	ippDelete(job->attrs);
	free(job);
	unlink(filename);
	continue;
      }
      else
      {
        if (ippReadFile(fd, job->attrs) != IPP_DATA)
	{
          LogMessage(L_ERROR, "LoadAllJobs: Unable to read job control file \"%s\"!",
	             filename);
	  close(fd);
	  ippDelete(job->attrs);
	  free(job);
	  unlink(filename);
	  continue;
	}

	close(fd);
      }

      if ((job->state = ippFindAttribute(job->attrs, "job-state", IPP_TAG_ENUM)) == NULL)
      {
        LogMessage(L_ERROR, "LoadAllJobs: Missing or bad job-state attribute in control file \"%s\"!",
	           filename);
	ippDelete(job->attrs);
	free(job);
	unlink(filename);
	continue;
      }

      if ((attr = ippFindAttribute(job->attrs, "job-printer-uri", IPP_TAG_URI)) == NULL)
      {
        LogMessage(L_ERROR, "LoadAllJobs: No job-printer-uri attribute in control file \"%s\"!",
	           filename);
	ippDelete(job->attrs);
	free(job);
	unlink(filename);
	continue;
      }

      httpSeparate(attr->values[0].string.text, method, username, host,
                   &port, resource);

      if ((dest = ValidateDest(host, resource, &(job->dtype))) == NULL &&
          job->state != NULL &&
	  job->state->values[0].integer <= IPP_JOB_PROCESSING)
      {
       /*
	* Job queued on remote printer or class, so add it...
	*/

	if (strncmp(resource, "/classes/", 9) == 0)
	{
	  p = AddClass(resource + 9);
	  SetString(&p->make_model, "Remote Class on unknown");
	}
	else
	{
	  p = AddPrinter(resource + 10);
	  SetString(&p->make_model, "Remote Printer on unknown");
	}

        p->state       = IPP_PRINTER_STOPPED;
	p->type        |= CUPS_PRINTER_REMOTE;
	p->browse_time = 2147483647;

	SetString(&p->location, "Location Unknown");
	SetString(&p->info, "No Information Available");
	p->hostname[0] = '\0';

	SetPrinterAttrs(p);
	dest = p->name;
      }

      if (dest == NULL)
      {
        LogMessage(L_ERROR, "LoadAllJobs: Unable to queue job for destination \"%s\"!",
	           attr->values[0].string.text);
	ippDelete(job->attrs);
	free(job);
	unlink(filename);
	continue;
      }

      SetString(&job->dest, dest);

      job->sheets     = ippFindAttribute(job->attrs, "job-media-sheets-completed",
                                         IPP_TAG_INTEGER);
      job->job_sheets = ippFindAttribute(job->attrs, "job-sheets", IPP_TAG_NAME);

      if ((attr = ippFindAttribute(job->attrs, "job-priority", IPP_TAG_INTEGER)) == NULL)
      {
        LogMessage(L_ERROR, "LoadAllJobs: Missing or bad job-priority attribute in control file \"%s\"!",
	           filename);
	ippDelete(job->attrs);
	free(job);
	unlink(filename);
	continue;
      }
      job->priority = attr->values[0].integer;

      if ((attr = ippFindAttribute(job->attrs, "job-originating-user-name", IPP_TAG_NAME)) == NULL)
      {
        LogMessage(L_ERROR, "LoadAllJobs: Missing or bad job-originating-user-name attribute in control file \"%s\"!",
	           filename);
	ippDelete(job->attrs);
	free(job);
	unlink(filename);
	continue;
      }
      SetString(&job->username, attr->values[0].string.text);

     /*
      * Insert the job into the array, sorting by job priority and ID...
      */

      for (current = Jobs, prev = NULL;
           current != NULL;
	   prev = current, current = current->next)
	if (job->priority > current->priority)
	  break;
	else if (job->priority == current->priority && job->id < current->id)
	  break;

      job->next = current;
      if (prev != NULL)
	prev->next = job;
      else
	Jobs = job;

      NumJobs ++;

     /*
      * Set the job hold-until time and state...
      */

      if (job->state->values[0].integer == IPP_JOB_HELD)
      {
	if ((attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_KEYWORD)) == NULL)
          attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

        if (attr == NULL)
          job->state->values[0].integer = IPP_JOB_PENDING;
	else
          SetJobHoldUntil(job->id, attr->values[0].string.text);
      }
      else if (job->state->values[0].integer == IPP_JOB_PROCESSING)
        job->state->values[0].integer = IPP_JOB_PENDING;
    }

 /*
  * Read all the d##### files...
  */

  rewinddir(dir);

  while ((dent = readdir(dir)) != NULL)
    if (NAMLEN(dent) > 7 && dent->d_name[0] == 'd' && strchr(dent->d_name, '-'))
    {
     /*
      * Find the job...
      */

      jobid  = atoi(dent->d_name + 1);
      fileid = atoi(strchr(dent->d_name, '-') + 1);

      LogMessage(L_DEBUG, "LoadAllJobs: Auto-typing document file %s...",
                 dent->d_name);

      snprintf(filename, sizeof(filename), "%s/%s", RequestRoot, dent->d_name);

      if ((job = FindJob(jobid)) == NULL)
      {
        LogMessage(L_ERROR, "LoadAllJobs: Orphaned print file \"%s\"!",
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
          LogMessage(L_ERROR, "LoadAllJobs: Ran out of memory for job file types!");
	  continue;
	}

        job->compressions = compressions;
        job->filetypes    = filetypes;
	job->num_files    = fileid;
      }

      job->filetypes[fileid - 1] = mimeFileType(MimeDatabase, filename,
                                                job->compressions + fileid - 1);

      if (job->filetypes[fileid - 1] == NULL)
        job->filetypes[fileid - 1] = mimeType(MimeDatabase, "application",
	                                      "vnd.cups-raw");
    }

  closedir(dir);

 /*
  * Clean out old jobs as needed...
  */

  CleanJobs();
}


/*
 * 'MoveJob()' - Move the specified job to a different destination.
 */

void
MoveJob(int        id,		/* I - Job ID */
        const char *dest)	/* I - Destination */
{
  job_t			*current;/* Current job */
  ipp_attribute_t	*attr;	/* job-printer-uri attribute */
  printer_t		*p;	/* Destination printer or class */


  if ((p = FindPrinter(dest)) == NULL)
    p = FindClass(dest);

  if (p == NULL)
    return;

  for (current = Jobs; current != NULL; current = current->next)
    if (current->id == id)
    {
      if (current->state->values[0].integer >= IPP_JOB_PROCESSING)
        break;

      SetString(&current->dest, dest);
      current->dtype = p->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_REMOTE |
                                  CUPS_PRINTER_IMPLICIT);

      if ((attr = ippFindAttribute(current->attrs, "job-printer-uri", IPP_TAG_URI)) != NULL)
      {
        free(attr->values[0].string.text);
	attr->values[0].string.text = strdup(p->uri);
      }

      SaveJob(current->id);

      return;
    }
}


/*
 * 'ReleaseJob()' - Release the specified job.
 */

void
ReleaseJob(int id)		/* I - Job ID */
{
  job_t	*job;			/* Job data */


  LogMessage(L_DEBUG, "ReleaseJob: id = %d", id);

  if ((job = FindJob(id)) == NULL)
    return;

  if (job->state->values[0].integer == IPP_JOB_HELD)
  {
    DEBUG_puts("ReleaseJob: setting state to pending...");

    job->state->values[0].integer = IPP_JOB_PENDING;
    SaveJob(id);
    CheckJobs();
  }
}


/*
 * 'RestartJob()' - Restart the specified job.
 */

void
RestartJob(int id)		/* I - Job ID */
{
  job_t	*job;			/* Job data */


  if ((job = FindJob(id)) == NULL)
    return;

  if (job->state->values[0].integer == IPP_JOB_STOPPED || JobFiles)
  {
    job->tries = 0;
    job->state->values[0].integer = IPP_JOB_PENDING;
    SaveJob(id);
    CheckJobs();
  }
}


/*
 * 'SaveJob()' - Save a job to disk.
 */

void
SaveJob(int id)			/* I - Job ID */
{
  job_t	*job;			/* Pointer to job */
  char	filename[1024];		/* Job control filename */
  int	fd;			/* File descriptor */


  if ((job = FindJob(id)) == NULL)
    return;

  snprintf(filename, sizeof(filename), "%s/c%05d", RequestRoot, id);

  if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0)
  {
    LogMessage(L_ERROR, "SaveJob: Unable to create job control file \"%s\" - %s.",
               filename, strerror(errno));
    return;
  }

  fchmod(fd, 0600);
  fchown(fd, RunUser, Group);

  ippWriteFile(fd, job->attrs);

  LogMessage(L_DEBUG2, "SaveJob: Closing file %d...", fd);

  close(fd);
}


/*
 * 'SetJobHoldUntil()' - Set the hold time for a job...
 */

void
SetJobHoldUntil(int        id,		/* I - Job ID */
                const char *when)	/* I - When to resume */
{
  job_t		*job;			/* Pointer to job */
  time_t	curtime;		/* Current time */
  struct tm	*curdate;		/* Current date */
  int		hour;			/* Hold hour */
  int		minute;			/* Hold minute */
  int		second;			/* Hold second */


  LogMessage(L_DEBUG, "SetJobHoldUntil(%d, \"%s\")", id, when);

  if ((job = FindJob(id)) == NULL)
    return;

  second = 0;

  if (strcmp(when, "indefinite") == 0)
  {
   /*
    * Hold indefinitely...
    */

    job->hold_until = 0;
  }
  else if (strcmp(when, "day-time") == 0)
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
  else if (strcmp(when, "evening") == 0 || strcmp(when, "night") == 0)
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
  else if (strcmp(when, "second-shift") == 0)
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
  else if (strcmp(when, "third-shift") == 0)
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
  else if (strcmp(when, "weekend") == 0)
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
      job->hold_until += 24 * 60 * 60 * 60;
  }

  LogMessage(L_DEBUG, "SetJobHoldUntil: hold_until = %d", (int)job->hold_until);
}


/*
 * 'SetJobPriority()' - Set the priority of a job, moving it up/down in the
 *                      list as needed.
 */

void
SetJobPriority(int id,		/* I - Job ID */
               int priority)	/* I - New priority (0 to 100) */
{
  job_t		*job,		/* Job to change */
		*current,	/* Current job */
		*prev;		/* Previous job */
  ipp_attribute_t *attr;	/* Job attribute */


 /*
  * Find the job...
  */

  for (current = Jobs, prev = NULL;
       current != NULL;
       prev = current, current = current->next)
    if (current->id == id)
      break;

  if (current == NULL)
    return;

 /*
  * Set the new priority...
  */

  job = current;
  job->priority = priority;

  if ((attr = ippFindAttribute(job->attrs, "job-priority", IPP_TAG_INTEGER)) != NULL)
    attr->values[0].integer = priority;
  else
    ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-priority",
                  priority);

  SaveJob(job->id);

 /*
  * See if we need to do any sorting...
  */

  if ((prev == NULL || job->priority < prev->priority) &&
      (job->next == NULL || job->next->priority < job->priority))
    return;

 /*
  * Remove the job from the list, and then insert it where it belongs...
  */

  if (prev == NULL)
    Jobs = job->next;
  else
    prev->next = job->next;

  for (current = Jobs, prev = NULL;
       current != NULL;
       prev = current, current = current->next)
    if (job->priority > current->priority)
      break;

  job->next = current;
  if (prev != NULL)
    prev->next = job;
  else
    Jobs = job;
}


/*
 * 'StartJob()' - Start a print job.
 */

void
StartJob(int       id,			/* I - Job ID */
         printer_t *printer)		/* I - Printer to print job */
{
  job_t		*current;		/* Current job */
  int		i;			/* Looping var */
  int		slot;			/* Pipe slot */
  int		num_filters;		/* Number of filters for job */
  mime_filter_t	*filters;		/* Filters for job */
  char		method[255],		/* Method for output */
		*optptr;		/* Pointer to options */
  ipp_attribute_t *attr;		/* Current attribute */
  int		pid;			/* Process ID of new filter process */
  int		banner_page;		/* 1 if banner page, 0 otherwise */
  int		statusfds[2],		/* Pipes used between the filters and scheduler */
		filterfds[2][2];	/* Pipes used between the filters */
  int		envc;			/* Number of environment variables */
  char		*argv[8],		/* Filter command-line arguments */
		filename[1024],		/* Job filename */
		command[1024],		/* Full path to filter/backend command */
		jobid[255],		/* Job ID string */
		title[IPP_MAX_NAME],	/* Job title string */
		copies[255],		/* # copies string */
		*envp[100],		/* Environment variables */
#ifdef __APPLE__
		processPath[1050],	/* CFProcessPath environment variable */
#endif	/* __APPLE__ */
		path[1024],		/* PATH environment variable */
		ipp_port[1024],		/* IPP_PORT environment variable */
		language[255],		/* LANG environment variable */
		charset[255],		/* CHARSET environment variable */
		classification[1024],	/* CLASSIFICATION environment variable */
		content_type[1024],	/* CONTENT_TYPE environment variable */
		device_uri[1024],	/* DEVICE_URI environment variable */
		ppd[1024],		/* PPD environment variable */
		class_name[255],	/* CLASS environment variable */
		printer_name[255],	/* PRINTER environment variable */
		root[1024],		/* CUPS_SERVERROOT environment variable */
		cache[255],		/* RIP_MAX_CACHE environment variable */
		tmpdir[1024],		/* TMPDIR environment variable */
		ld_library_path[1024],	/* LD_LIBRARY_PATH environment variable */
		ld_preload[1024],	/* LD_PRELOAD environment variable */
		dyld_library_path[1024],/* DYLD_LIBRARY_PATH environment variable */
		shlib_path[1024],	/* SHLIB_PATH environment variable */
		nlspath[1024],		/* NLSPATH environment variable */
		datadir[1024],		/* CUPS_DATADIR environment variable */
		fontpath[1050],		/* CUPS_FONTPATH environment variable */
		vg_args[1024],		/* VG_ARGS environment variable */
		ld_assume_kernel[1024];	/* LD_ASSUME_KERNEL environment variable */
  static char	*options = NULL;	/* Full list of options */
  static int	optlength = 0;		/* Length of option buffer */


  LogMessage(L_DEBUG, "StartJob(%d, %p)", id, printer);

  for (current = Jobs; current != NULL; current = current->next)
    if (current->id == id)
      break;

  if (current == NULL)
    return;

  LogMessage(L_DEBUG, "StartJob() id = %d, file = %d/%d", id,
             current->current_file, current->num_files);

  if (current->num_files == 0)
  {
    LogMessage(L_ERROR, "Job ID %d has no files!  Cancelling it!", id);
    CancelJob(id, 0);
    return;
  }

 /*
  * Figure out what filters are required to convert from
  * the source to the destination type...
  */

  num_filters   = 0;
  current->cost = 0;

  if (printer->raw)
  {
   /*
    * Remote jobs and raw queues go directly to the printer without
    * filtering...
    */

    LogMessage(L_DEBUG, "StartJob: Sending job to queue tagged as raw...");

    filters = NULL;
  }
  else
  {
   /*
    * Local jobs get filtered...
    */

    filters = mimeFilter(MimeDatabase, current->filetypes[current->current_file],
                         printer->filetype, &num_filters, MAX_FILTERS - 1);

    if (num_filters == 0)
    {
      LogMessage(L_ERROR, "Unable to convert file %d to printable format for job %d!",
	         current->current_file, current->id);
      LogMessage(L_INFO, "Hint: Do you have ESP Ghostscript installed?");

      if (LogLevel < L_DEBUG)
        LogMessage(L_INFO, "Hint: Try setting the LogLevel to \"debug\".");

      current->current_file ++;

      if (current->current_file == current->num_files)
        CancelJob(current->id, 0);

      return;
    }

   /*
    * Remove NULL ("-") filters...
    */

    for (i = 0; i < num_filters;)
      if (strcmp(filters[i].filter, "-") == 0)
      {
        num_filters --;
	if (i < num_filters)
	  memcpy(filters + i, filters + i + 1,
	         (num_filters - i) * sizeof(mime_filter_t));
      }
      else
        i ++;

    if (num_filters == 0)
    {
      free(filters);
      filters = NULL;
    }
    else
    {
     /*
      * Compute filter cost...
      */

      for (i = 0; i < num_filters; i ++)
	current->cost += filters[i].cost;
    }
  }

 /*
  * See if the filter cost is too high...
  */

  if ((FilterLevel + current->cost) > FilterLimit && FilterLevel > 0 &&
      FilterLimit > 0)
  {
   /*
    * Don't print this job quite yet...
    */

    if (filters != NULL)
      free(filters);

    LogMessage(L_INFO, "Holding job %d because filter limit has been reached.",
               id);
    LogMessage(L_DEBUG, "StartJob: id = %d, file = %d, "
                        "cost = %d, level = %d, limit = %d",
               id, current->current_file, current->cost, FilterLevel,
	       FilterLimit);
    return;
  }

  FilterLevel += current->cost;

 /*
  * Add decompression filters, if any...
  */

  if (current->compressions[current->current_file])
  {
   /*
    * Add gziptoany filter to the front of the list...
    */

    mime_filter_t	*temp_filters;

    if (num_filters == 0)
      temp_filters = malloc(sizeof(mime_filter_t));
    else
      temp_filters = realloc(filters,
                             sizeof(mime_filter_t) * (num_filters + 1));

    if (temp_filters == NULL)
    {
      LogMessage(L_ERROR, "Unable to add decompression filter - %s",
                 strerror(errno));

      free(filters);

      current->current_file ++;

      if (current->current_file == current->num_files)
        CancelJob(current->id, 0);

      return;
    }

    filters = temp_filters;
    memmove(filters + 1, filters, num_filters * sizeof(mime_filter_t));
    *filters = gziptoany_filter;
    num_filters ++;
  }

 /*
  * Update the printer and job state to "processing"...
  */

  current->state->values[0].integer = IPP_JOB_PROCESSING;
  current->status  = 0;
  current->printer = printer;
  printer->job     = current;
  SetPrinterState(printer, IPP_PRINTER_PROCESSING, 0);

  if (current->current_file == 0)
  {
    set_time(current, "time-at-processing");
    pipe(current->back_pipes);
  }

 /*
  * Determine if we are printing a banner page or not...
  */

  if (current->job_sheets == NULL)
  {
    LogMessage(L_DEBUG, "No job-sheets attribute.");
    if ((current->job_sheets =
         ippFindAttribute(current->attrs, "job-sheets", IPP_TAG_ZERO)) != NULL)
      LogMessage(L_DEBUG, "... but someone added one without setting job_sheets!");
  }
  else if (current->job_sheets->num_values == 1)
    LogMessage(L_DEBUG, "job-sheets=%s",
               current->job_sheets->values[0].string.text);
  else
    LogMessage(L_DEBUG, "job-sheets=%s,%s",
               current->job_sheets->values[0].string.text,
               current->job_sheets->values[1].string.text);

  if (printer->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT))
    banner_page = 0;
  else if (current->job_sheets == NULL)
    banner_page = 0;
  else if (strcasecmp(current->job_sheets->values[0].string.text, "none") != 0 &&
	   current->current_file == 0)
    banner_page = 1;
  else if (current->job_sheets->num_values > 1 &&
	   strcasecmp(current->job_sheets->values[1].string.text, "none") != 0 &&
	   current->current_file == (current->num_files - 1))
    banner_page = 1;
  else
    banner_page = 0;

  LogMessage(L_DEBUG, "banner_page = %d", banner_page);

 /*
  * Building the options string is harder than it needs to be, but
  * for the moment we need to pass strings for command-line args and
  * not IPP attribute pointers... :)
  *
  * First allocate/reallocate the option buffer as needed...
  */

  i = ipp_length(current->attrs);

  if (i > optlength)
  {
    if (optlength == 0)
      optptr = malloc(i);
    else
      optptr = realloc(options, i);

    if (optptr == NULL)
    {
      LogMessage(L_CRIT, "StartJob: Unable to allocate %d bytes for option buffer for job %d!",
                 i, id);

      if (filters != NULL)
        free(filters);

      FilterLevel -= current->cost;
      
      CancelJob(id, 0);
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

  snprintf(title, sizeof(title), "%s-%d", printer->name, current->id);
  strcpy(copies, "1");

  for (attr = current->attrs->attrs; attr != NULL; attr = attr->next)
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
           strncmp(attr->name, "number-up", 9) == 0) &&
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
	      if (strchr(attr->values[i].string.text, ' ') != NULL ||
		  strchr(attr->values[i].string.text, '\t') != NULL ||
		  strchr(attr->values[i].string.text, '\n') != NULL)
	      {
		strlcat(optptr, "\'", optlength - (optptr - options));
		strlcat(optptr, attr->values[i].string.text,
		        optlength - (optptr - options));
		strlcat(optptr, "\'", optlength - (optptr - options));
	      }
	      else
		strlcat(optptr, attr->values[i].string.text,
		        optlength - (optptr - options));
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

  sprintf(jobid, "%d", current->id);
  snprintf(filename, sizeof(filename), "%s/d%05d-%03d", RequestRoot,
           current->id, current->current_file + 1);

  argv[0] = printer->name;
  argv[1] = jobid;
  argv[2] = current->username;
  argv[3] = title;
  argv[4] = copies;
  argv[5] = options;
  argv[6] = filename;
  argv[7] = NULL;

  LogMessage(L_DEBUG, "StartJob: argv = \"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"",
             argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);

 /*
  * Create environment variable strings for the filters...
  */

  attr = ippFindAttribute(current->attrs, "attributes-natural-language",
                          IPP_TAG_LANGUAGE);

  switch (strlen(attr->values[0].string.text))
  {
    default :
       /*
        * This is an unknown or badly formatted language code; use
	* the POSIX locale...
	*/

	strcpy(language, "LANG=C");
	break;

    case 2 :
       /*
        * Just the language code (ll)...
	*/

        snprintf(language, sizeof(language), "LANG=%s",
	         attr->values[0].string.text);
        break;

    case 5 :
       /*
        * Language and country code (ll-cc)...
	*/

        snprintf(language, sizeof(language), "LANG=%c%c_%c%c",
	         attr->values[0].string.text[0],
		 attr->values[0].string.text[1],
		 toupper(attr->values[0].string.text[3] & 255),
		 toupper(attr->values[0].string.text[4] & 255));
        break;
  }

  attr = ippFindAttribute(current->attrs, "document-format",
                          IPP_TAG_MIMETYPE);
  if (attr != NULL &&
      (optptr = strstr(attr->values[0].string.text, "charset=")) != NULL)
    snprintf(charset, sizeof(charset), "CHARSET=%s", optptr + 8);
  else
  {
    attr = ippFindAttribute(current->attrs, "attributes-charset",
	                    IPP_TAG_CHARSET);
    snprintf(charset, sizeof(charset), "CHARSET=%s",
             attr->values[0].string.text);
  }

  snprintf(path, sizeof(path), "PATH=%s/filter:/bin:/usr/bin", ServerBin);
  snprintf(content_type, sizeof(content_type), "CONTENT_TYPE=%s/%s",
           current->filetypes[current->current_file]->super,
           current->filetypes[current->current_file]->type);
  snprintf(device_uri, sizeof(device_uri), "DEVICE_URI=%s", printer->device_uri);
  snprintf(ppd, sizeof(ppd), "PPD=%s/ppd/%s.ppd", ServerRoot, printer->name);
  snprintf(printer_name, sizeof(printer_name), "PRINTER=%s", printer->name);
  snprintf(cache, sizeof(cache), "RIP_MAX_CACHE=%s", RIPCache);
  snprintf(root, sizeof(root), "CUPS_SERVERROOT=%s", ServerRoot);
  snprintf(tmpdir, sizeof(tmpdir), "TMPDIR=%s", TempDir);
  snprintf(datadir, sizeof(datadir), "CUPS_DATADIR=%s", DataDir);
  snprintf(fontpath, sizeof(fontpath), "CUPS_FONTPATH=%s", FontPath);
  sprintf(ipp_port, "IPP_PORT=%d", LocalPort);

  envc = 0;

  envp[envc ++] = path;
  envp[envc ++] = "SOFTWARE=CUPS/1.1";
  envp[envc ++] = "USER=root";
  envp[envc ++] = charset;
  envp[envc ++] = language;
  if (TZ && TZ[0])
    envp[envc ++] = TZ;
  envp[envc ++] = ppd;
  envp[envc ++] = root;
  envp[envc ++] = cache;
  envp[envc ++] = tmpdir;
  envp[envc ++] = content_type;
  envp[envc ++] = device_uri;
  envp[envc ++] = printer_name;
  envp[envc ++] = datadir;
  envp[envc ++] = fontpath;
  envp[envc ++] = "CUPS_SERVER=localhost";
  envp[envc ++] = ipp_port;

  if (getenv("VG_ARGS") != NULL)
  {
    snprintf(vg_args, sizeof(vg_args), "VG_ARGS=%s", getenv("VG_ARGS"));
    envp[envc ++] = vg_args;
  }

  if (getenv("LD_ASSUME_KERNEL") != NULL)
  {
    snprintf(ld_assume_kernel, sizeof(ld_assume_kernel), "LD_ASSUME_KERNEL=%s",
             getenv("LD_ASSUME_KERNEL"));
    envp[envc ++] = ld_assume_kernel;
  }

  if (getenv("LD_LIBRARY_PATH") != NULL)
  {
    snprintf(ld_library_path, sizeof(ld_library_path), "LD_LIBRARY_PATH=%s",
             getenv("LD_LIBRARY_PATH"));
    envp[envc ++] = ld_library_path;
  }

  if (getenv("LD_PRELOAD") != NULL)
  {
    snprintf(ld_preload, sizeof(ld_preload), "LD_PRELOAD=%s",
             getenv("LD_PRELOAD"));
    envp[envc ++] = ld_preload;
  }

  if (getenv("DYLD_LIBRARY_PATH") != NULL)
  {
    snprintf(dyld_library_path, sizeof(dyld_library_path), "DYLD_LIBRARY_PATH=%s",
             getenv("DYLD_LIBRARY_PATH"));
    envp[envc ++] = dyld_library_path;
  }

  if (getenv("SHLIB_PATH") != NULL)
  {
    snprintf(shlib_path, sizeof(shlib_path), "SHLIB_PATH=%s",
             getenv("SHLIB_PATH"));
    envp[envc ++] = shlib_path;
  }

  if (getenv("NLSPATH") != NULL)
  {
    snprintf(nlspath, sizeof(nlspath), "NLSPATH=%s", getenv("NLSPATH"));
    envp[envc ++] = nlspath;
  }

  if (Classification && !banner_page)
  {
    if ((attr = ippFindAttribute(current->attrs, "job-sheets",
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

  if (current->dtype & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT))
  {
    snprintf(class_name, sizeof(class_name), "CLASS=%s", current->dest);
    envp[envc ++] = class_name;
  }

#ifdef __APPLE__
  strlcpy(processPath, "<CFProcessPath>", sizeof(processPath));
  envp[envc ++] = processPath;
#endif	/* __APPLE__ */

  envp[envc] = NULL;

  for (i = 0; i < envc; i ++)
    LogMessage(L_DEBUG, "StartJob: envp[%d]=\"%s\"", i, envp[i]);

  current->current_file ++;

 /*
  * Make sure we have a buffer to read status info into...
  */

  if (current->buffer == NULL)
  {
    LogMessage(L_DEBUG2, "StartJob: Allocating status buffer...");

    if ((current->buffer = malloc(JOB_BUFFER_SIZE)) == NULL)
    {
      LogMessage(L_EMERG, "Unable to allocate memory for job status buffer - %s",
                 strerror(errno));
      CancelJob(current->id, 0);
      return;
    }

    current->bufused = 0;
  }

 /*
  * Now create processes for all of the filters...
  */

  if (pipe(statusfds))
  {
    LogMessage(L_ERROR, "Unable to create job status pipes - %s.",
	       strerror(errno));
    snprintf(printer->state_message, sizeof(printer->state_message),
             "Unable to create status pipes - %s.", strerror(errno));

    AddPrinterHistory(printer);
    return;
  }

  LogMessage(L_DEBUG, "StartJob: statusfds = [ %d %d ]",
             statusfds[0], statusfds[1]);

  current->status_pipe = statusfds[0];
  current->status      = 0;
  memset(current->filters, 0, sizeof(current->filters));

  filterfds[1][0] = open("/dev/null", O_RDONLY);
  filterfds[1][1] = -1;

  LogMessage(L_DEBUG, "StartJob: filterfds[%d] = [ %d %d ]", 1, filterfds[1][0],
             filterfds[1][1]);

  for (i = 0, slot = 0; i < num_filters; i ++)
  {
    if (filters[i].filter[0] != '/')
      snprintf(command, sizeof(command), "%s/filter/%s", ServerBin,
               filters[i].filter);
    else
      strlcpy(command, filters[i].filter, sizeof(command));

#ifdef __APPLE__
   /*
    * Setting CFProcessPath lets OS X's Core Foundation code find
    * the bundle that may be associated with a filter or backend.
    */

    snprintf(processPath, sizeof(processPath), "CFProcessPath=%s", command);
    LogMessage(L_DEBUG, "StartJob: %s\n", processPath);
#endif	/* __APPLE__ */

    if (i < (num_filters - 1))
      pipe(filterfds[slot]);
    else
    {
      if (current->current_file == 1)
      {
	if (strncmp(printer->device_uri, "file:", 5) != 0)
	  pipe(current->print_pipes);
	else
	{
	  current->print_pipes[0] = -1;
	  if (strncmp(printer->device_uri, "file:/dev/", 10) == 0)
	    current->print_pipes[1] = open(printer->device_uri + 5,
	                                   O_WRONLY | O_EXCL);
	  else
	    current->print_pipes[1] = open(printer->device_uri + 5,
	                                   O_WRONLY | O_CREAT | O_TRUNC, 0600);
	}

	LogMessage(L_DEBUG2, "StartJob: print_pipes = [ %d %d ]",
                   current->print_pipes[0], current->print_pipes[1]);
      }

      filterfds[slot][0] = current->print_pipes[0];
      filterfds[slot][1] = current->print_pipes[1];
    }

    LogMessage(L_DEBUG, "StartJob: filter = \"%s\"", command);
    LogMessage(L_DEBUG, "StartJob: filterfds[%d] = [ %d %d ]",
               slot, filterfds[slot][0], filterfds[slot][1]);

    pid = start_process(command, argv, envp, filterfds[!slot][0],
                        filterfds[slot][1], statusfds[1],
			current->back_pipes[0], 0, current->filters + i);

    LogMessage(L_DEBUG2, "StartJob: Closing filter pipes for slot %d [ %d %d ]...",
               !slot, filterfds[!slot][0], filterfds[!slot][1]);

    close(filterfds[!slot][0]);
    close(filterfds[!slot][1]);

    if (pid == 0)
    {
      LogMessage(L_ERROR, "Unable to start filter \"%s\" - %s.",
                 filters[i].filter, strerror(errno));
      snprintf(printer->state_message, sizeof(printer->state_message),
               "Unable to start filter \"%s\" - %s.",
               filters[i].filter, strerror(errno));

      AddPrinterHistory(printer);

      if (filters != NULL)
	free(filters);

      AddPrinterHistory(printer);

      CancelJob(current->id, 0);
      return;
    }

    LogMessage(L_INFO, "Started filter %s (PID %d) for job %d.",
               command, pid, current->id);

    argv[6] = NULL;
    slot    = !slot;
  }

  if (filters != NULL)
    free(filters);

 /*
  * Finally, pipe the final output into a backend process if needed...
  */

  if (strncmp(printer->device_uri, "file:", 5) != 0)
  {
    if (current->current_file == 1)
    {
      sscanf(printer->device_uri, "%254[^:]", method);
      snprintf(command, sizeof(command), "%s/backend/%s", ServerBin, method);

#ifdef __APPLE__
     /*
      * Setting CFProcessPath lets OS X's Core Foundation code find
      * the bundle that may be associated with a filter or backend.
      */

      snprintf(processPath, sizeof(processPath), "CFProcessPath=%s", command);
      LogMessage(L_DEBUG, "StartJob: %s\n", processPath);
#endif	/* __APPLE__ */

      argv[0] = printer->device_uri;

      filterfds[slot][0] = -1;
      filterfds[slot][1] = open("/dev/null", O_WRONLY);

      LogMessage(L_DEBUG, "StartJob: backend = \"%s\"", command);
      LogMessage(L_DEBUG, "StartJob: filterfds[%d] = [ %d %d ]",
        	 slot, filterfds[slot][0], filterfds[slot][1]);

      pid = start_process(command, argv, envp, filterfds[!slot][0],
			  filterfds[slot][1], statusfds[1],
			  current->back_pipes[1], 1,
			  &(current->backend));

      if (pid == 0)
      {
	LogMessage(L_ERROR, "Unable to start backend \"%s\" - %s.",
                   method, strerror(errno));
	snprintf(printer->state_message, sizeof(printer->state_message),
        	 "Unable to start backend \"%s\" - %s.", method, strerror(errno));

	LogMessage(L_DEBUG2, "StartJob: Closing print pipes [ %d %d ]...",
        	   current->print_pipes[0], current->print_pipes[1]);

	close(current->print_pipes[0]);
	close(current->print_pipes[1]);

	current->print_pipes[0] = -1;
	current->print_pipes[1] = -1;

	LogMessage(L_DEBUG2, "StartJob: Closing back pipes [ %d %d ]...",
        	   current->back_pipes[0], current->back_pipes[1]);

	close(current->back_pipes[0]);
	close(current->back_pipes[1]);

	current->back_pipes[0] = -1;
	current->back_pipes[1] = -1;

        CancelJob(current->id, 0);
	return;
      }
      else
      {
	LogMessage(L_INFO, "Started backend %s (PID %d) for job %d.",
	           command, pid, current->id);
      }
    }

    if (current->current_file == current->num_files)
    {
      LogMessage(L_DEBUG2, "StartJob: Closing print pipes [ %d %d ]...",
        	 current->print_pipes[0], current->print_pipes[1]);

      close(current->print_pipes[0]);
      close(current->print_pipes[1]);

      current->print_pipes[0] = -1;
      current->print_pipes[1] = -1;

      LogMessage(L_DEBUG2, "StartJob: Closing back pipes [ %d %d ]...",
        	 current->back_pipes[0], current->back_pipes[1]);

      close(current->back_pipes[0]);
      close(current->back_pipes[1]);

      current->back_pipes[0] = -1;
      current->back_pipes[1] = -1;
    }
  }
  else
  {
    filterfds[slot][0] = -1;
    filterfds[slot][1] = -1;

    if (current->current_file == current->num_files)
    {
      LogMessage(L_DEBUG2, "StartJob: Closing print pipes [ %d %d ]...",
        	 current->print_pipes[0], current->print_pipes[1]);

      close(current->print_pipes[0]);
      close(current->print_pipes[1]);

      current->print_pipes[0] = -1;
      current->print_pipes[1] = -1;
    }
  }

  LogMessage(L_DEBUG2, "StartJob: Closing filter pipes for slot %d [ %d %d ]...",
             slot, filterfds[slot][0], filterfds[slot][1]);

  close(filterfds[slot][0]);
  close(filterfds[slot][1]);

  LogMessage(L_DEBUG2, "StartJob: Closing status output pipe %d...",
             statusfds[1]);

  close(statusfds[1]);

  LogMessage(L_DEBUG2, "StartJob: Adding fd %d to InputSet...",
             current->status_pipe);

  FD_SET(current->status_pipe, InputSet);
}


/*
 * 'StopAllJobs()' - Stop all print jobs.
 */

void
StopAllJobs(void)
{
  job_t	*current;		/* Current job */


  DEBUG_puts("StopAllJobs()");

  for (current = Jobs; current != NULL; current = current->next)
    if (current->state->values[0].integer == IPP_JOB_PROCESSING)
    {
      StopJob(current->id, 1);
      current->state->values[0].integer = IPP_JOB_PENDING;
    }
}


/*
 * 'StopJob()' - Stop a print job.
 */

void
StopJob(int id,			/* I - Job ID */
        int force)		/* I - 1 = Force all filters to stop */
{
  int	i;			/* Looping var */
  job_t	*current;		/* Current job */


  LogMessage(L_DEBUG, "StopJob: id = %d, force = %d", id, force);

  for (current = Jobs; current != NULL; current = current->next)
    if (current->id == id)
    {
      DEBUG_puts("StopJob: found job in list.");

      if (current->state->values[0].integer == IPP_JOB_PROCESSING)
      {
        DEBUG_puts("StopJob: job state is \'processing\'.");

        FilterLevel -= current->cost;

        if (current->status < 0 &&
	    !(current->dtype & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT)) &&
	    !(current->printer->type & CUPS_PRINTER_FAX))
	  SetPrinterState(current->printer, IPP_PRINTER_STOPPED, 1);
	else if (current->printer->state != IPP_PRINTER_STOPPED)
	  SetPrinterState(current->printer, IPP_PRINTER_IDLE, 0);

        LogMessage(L_DEBUG, "StopJob: printer state is %d", current->printer->state);

	current->state->values[0].integer = IPP_JOB_STOPPED;
        current->printer->job = NULL;
        current->printer      = NULL;

	current->current_file --;

        for (i = 0; current->filters[i]; i ++)
	  if (current->filters[i] > 0)
	  {
	    kill(current->filters[i], force ? SIGKILL : SIGTERM);
	    current->filters[i] = 0;
	  }

	if (current->backend > 0)
	{
	  kill(current->backend, force ? SIGKILL : SIGTERM);
	  current->backend = 0;
	}

	LogMessage(L_DEBUG2, "StopJob: Closing print pipes [ %d %d ]...",
        	   current->print_pipes[0], current->print_pipes[1]);

	close(current->print_pipes[0]);
        close(current->print_pipes[1]);

	current->print_pipes[0] = -1;
	current->print_pipes[1] = -1;

	LogMessage(L_DEBUG2, "StopJob: Closing back pipes [ %d %d ]...",
        	   current->back_pipes[0], current->back_pipes[1]);

        close(current->back_pipes[0]);
        close(current->back_pipes[1]);

	current->back_pipes[0] = -1;
	current->back_pipes[1] = -1;

        if (current->status_pipe >= 0)
        {
	 /*
	  * Close the pipe and clear the input bit.
	  */

	  LogMessage(L_DEBUG2, "StopJob: Closing status input pipe %d...",
        	     current->status_pipe);

          close(current->status_pipe);

          LogMessage(L_DEBUG2, "StopJob: Removing fd %d from InputSet...",
	             current->status_pipe);

	  FD_CLR(current->status_pipe, InputSet);
	  current->status_pipe = -1;
        }

        if (current->buffer)
	{
	 /*
	  * Free the status buffer...
	  */

          LogMessage(L_DEBUG2, "StopJob: Freeing status buffer...");

          free(current->buffer);
	  current->buffer  = NULL;
	  current->bufused = 0;
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
  int		i;		/* Looping var */
  int		bytes;		/* Number of bytes read */
  int		copies;		/* Number of copies printed */
  char		*lineptr,	/* Pointer to end of line in buffer */
		*message;	/* Pointer to message text */
  int		loglevel;	/* Log level for message */
  int		job_history;	/* Did CancelJob() keep the job? */
  cups_ptype_t	ptype;		/* Printer type (color, small, etc.) */


  if ((bytes = read(job->status_pipe, job->buffer + job->bufused,
                    JOB_BUFFER_SIZE - job->bufused - 1)) > 0)
  {
    job->bufused += bytes;
    job->buffer[job->bufused] = '\0';

    if ((lineptr = strchr(job->buffer, '\n')) == NULL &&
        job->bufused == (JOB_BUFFER_SIZE - 1))
      lineptr  = job->buffer + job->bufused;
  }
  else if (bytes < 0 && errno == EINTR)
    return;
  else
  {
    lineptr  = job->buffer + job->bufused;
    *lineptr = '\0';
  }

  if (job->bufused == 0 && bytes == 0)
    lineptr = NULL;

  while (lineptr != NULL)
  {
   /*
    * Terminate each line and process it...
    */

    *lineptr++ = '\0';

   /*
    * Figure out the logging level...
    */

    if (strncmp(job->buffer, "EMERG:", 6) == 0)
    {
      loglevel = L_EMERG;
      message  = job->buffer + 6;
    }
    else if (strncmp(job->buffer, "ALERT:", 6) == 0)
    {
      loglevel = L_ALERT;
      message  = job->buffer + 6;
    }
    else if (strncmp(job->buffer, "CRIT:", 5) == 0)
    {
      loglevel = L_CRIT;
      message  = job->buffer + 5;
    }
    else if (strncmp(job->buffer, "ERROR:", 6) == 0)
    {
      loglevel = L_ERROR;
      message  = job->buffer + 6;
    }
    else if (strncmp(job->buffer, "WARNING:", 8) == 0)
    {
      loglevel = L_WARN;
      message  = job->buffer + 8;
    }
    else if (strncmp(job->buffer, "NOTICE:", 6) == 0)
    {
      loglevel = L_NOTICE;
      message  = job->buffer + 6;
    }
    else if (strncmp(job->buffer, "INFO:", 5) == 0)
    {
      loglevel = L_INFO;
      message  = job->buffer + 5;
    }
    else if (strncmp(job->buffer, "DEBUG:", 6) == 0)
    {
      loglevel = L_DEBUG;
      message  = job->buffer + 6;
    }
    else if (strncmp(job->buffer, "DEBUG2:", 7) == 0)
    {
      loglevel = L_DEBUG2;
      message  = job->buffer + 7;
    }
    else if (strncmp(job->buffer, "PAGE:", 5) == 0)
    {
      loglevel = L_PAGE;
      message  = job->buffer + 5;
    }
    else if (strncmp(job->buffer, "STATE:", 6) == 0)
    {
      loglevel = L_STATE;
      message  = job->buffer + 6;
    }
    else
    {
      loglevel = L_DEBUG;
      message  = job->buffer;
    }

   /*
    * Skip leading whitespace in the message...
    */

    while (isspace(*message & 255))
      message ++;

   /*
    * Send it to the log file and printer state message as needed...
    */

    if (loglevel == L_PAGE)
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
	  UpdateQuota(job->printer, job->username, copies, 0);
      }

      LogPage(job, message);
    }
    else if (loglevel == L_STATE)
      SetPrinterReasons(job->printer, message);
    else
    {
     /*
      * Other status message; send it to the error_log file...
      */

      if (loglevel != L_INFO || LogLevel == L_DEBUG2)
	LogMessage(loglevel, "[Job %d] %s", job->id, message);

      if ((loglevel == L_INFO && !job->status) ||
	  loglevel < L_INFO)
      {
        strlcpy(job->printer->state_message, message,
                sizeof(job->printer->state_message));

        AddPrinterHistory(job->printer);
      }
    }

   /*
    * Copy over the buffer data we've used up...
    */

    cups_strcpy(job->buffer, lineptr);
    job->bufused -= lineptr - job->buffer;

    if (job->bufused < 0)
      job->bufused = 0;

    lineptr = strchr(job->buffer, '\n');
  }

  if (bytes <= 0)
  {
   /*
    * See if all of the filters and the backend have returned their
    * exit statuses.
    */

    for (i = 0; job->filters[i]; i ++)
      if (job->filters[i] > 0)
	return;

    if (job->current_file >= job->num_files && job->backend > 0)
      return;

   /*
    * Handle the end of job stuff...
    */

    LogMessage(L_DEBUG, "UpdateJob: job %d, file %d is complete.",
               job->id, job->current_file - 1);

    if (job->status_pipe >= 0)
    {
     /*
      * Close the pipe and clear the input bit.
      */

      LogMessage(L_DEBUG2, "UpdateJob: Closing status input pipe %d...",
        	 job->status_pipe);

      close(job->status_pipe);

      LogMessage(L_DEBUG2, "UpdateJob: Removing fd %d from InputSet...",
                 job->status_pipe);

      FD_CLR(job->status_pipe, InputSet);
      job->status_pipe = -1;
    }

    if (job->status < 0)
    {
     /*
      * Backend had errors; stop it...
      */

      ptype = job->printer->type;

      StopJob(job->id, 0);
      job->state->values[0].integer = IPP_JOB_PENDING;
      SaveJob(job->id);

     /*
      * If the job was queued to a class, try requeuing it...  For
      * faxes, hold the current job for 5 minutes.
      */

      if (job->dtype & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT))
        CheckJobs();
      else if (ptype & CUPS_PRINTER_FAX)
      {
       /*
        * See how many times we've tried to send the job; if more than
	* the limit, cancel the job.
	*/

        job->tries ++;

	if (job->tries >= FaxRetryLimit)
	{
	 /*
	  * Too many tries...
	  */

	  LogMessage(L_ERROR, "Canceling fax job %d since it could not be sent after %d tries.",
	             job->id, FaxRetryLimit);
	  CancelJob(job->id, 0);
	}
	else
	{
	 /*
	  * Try again in N seconds...
	  */

	  set_hold_until(job, time(NULL) + FaxRetryInterval);
	}

        CheckJobs();
      }
    }
    else if (job->status > 0)
    {
     /*
      * Filter had errors; cancel it...
      */

      if (job->current_file < job->num_files)
        StartJob(job->id, job->printer);
      else
      {
	job_history = JobHistory && !(job->dtype & CUPS_PRINTER_REMOTE);

        CancelJob(job->id, 0);

        if (job_history)
	{
          job->state->values[0].integer = IPP_JOB_ABORTED;
	  SaveJob(job->id);
	}

        CheckJobs();
      }
    }
    else
    {
     /*
      * Job printed successfully; cancel it...
      */

      if (job->current_file < job->num_files)
      {
        FilterLevel -= job->cost;
        StartJob(job->id, job->printer);
      }
      else
      {
	job_history = JobHistory && !(job->dtype & CUPS_PRINTER_REMOTE);

	CancelJob(job->id, 0);

        if (job_history)
	{
          job->state->values[0].integer = IPP_JOB_COMPLETED;
	  SaveJob(job->id);
	}

	CheckJobs();
      }
    }
  }
}


/*
 * 'ipp_length()' - Compute the size of the buffer needed to hold 
 *		    the textual IPP attributes.
 */

int				/* O - Size of buffer to hold IPP attributes */
ipp_length(ipp_t *ipp)		/* I - IPP request */
{
  int			bytes; 	/* Number of bytes */
  int			i;	/* Looping var */
  ipp_attribute_t	*attr;  /* Current attribute */


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
set_time(job_t      *job,	/* I - Job to update */
         const char *name)	/* I - Name of attribute */
{
  ipp_attribute_t	*attr;	/* Time attribute */


  if ((attr = ippFindAttribute(job->attrs, name, IPP_TAG_ZERO)) != NULL)
  {
    attr->value_tag         = IPP_TAG_INTEGER;
    attr->values[0].integer = time(NULL);
  }
}


/*
 * 'start_process()' - Start a background process.
 */

static int				/* O - Process ID or 0 */
start_process(const char *command,	/* I - Full path to command */
              char       *argv[],	/* I - Command-line arguments */
	      char       *envp[],	/* I - Environment */
              int        infd,		/* I - Standard input file descriptor */
	      int        outfd,		/* I - Standard output file descriptor */
	      int        errfd,		/* I - Standard error file descriptor */
	      int        backfd,	/* I - Backchannel file descriptor */
	      int        root,		/* I - Run as root? */
	      int        *pid)          /* O - Process ID */
{
  int	fd;				/* Looping var */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction	action;		/* POSIX signal handler */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


  LogMessage(L_DEBUG, "start_process(\"%s\", %p, %p, %d, %d, %d)",
             command, argv, envp, infd, outfd, errfd);

 /*
  * Block signals before forking...
  */

  HoldSignals();

  if ((*pid = fork()) == 0)
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
    if (backfd > 3)
    {
      close(3);
      dup(backfd);
      fcntl(3, F_SETFL, O_NDELAY);
    }

   /*
    * Close extra file descriptors...
    */

    for (fd = 4; fd < MaxFDs; fd ++)
      close(fd);

   /*
    * Change the priority of the process based on the FilterNice setting.
    * (this is not done for backends...)
    */

    if (!root)
      nice(FilterNice);

   /*
    * Change user to something "safe"...
    */

    if (!root && !RunUser)
    {
     /*
      * Running as root, so change to non-priviledged user...
      */

      if (setgid(Group))
        exit(errno);

      if (setgroups(1, &Group))
        exit(errno);

      if (setuid(User))
        exit(errno);
    }
    else
    {
     /*
      * Reset group membership to just the main one we belong to.
      */

      setgroups(1, &Group);
    }

   /*
    * Change umask to restrict permissions on created files...
    */

    umask(077);

   /*
    * Unblock signals before doing the exec...
    */

#ifdef HAVE_SIGSET
    sigset(SIGTERM, SIG_DFL);
    sigset(SIGCHLD, SIG_DFL);
#elif defined(HAVE_SIGACTION)
    memset(&action, 0, sizeof(action));

    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_DFL;

    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGCHLD, &action, NULL);
#else
    signal(SIGTERM, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
#endif /* HAVE_SIGSET */

    ReleaseSignals();

   /*
    * Execute the command; if for some reason this doesn't work,
    * return the error code...
    */

    execve(command, argv, envp);

    perror(command);

    exit(errno);
  }
  else if (*pid < 0)
  {
   /*
    * Error - couldn't fork a new process!
    */

    LogMessage(L_ERROR, "Unable to fork %s - %s.", command, strerror(errno));

    *pid = 0;
  }

  ReleaseSignals();

  return (*pid);
}


/*
 * 'set_hold_until()' - Set the hold time and update job-hold-until attribute...
 */

static void 
set_hold_until(job_t *job, 		/* I - Job to update */
	       time_t holdtime)		/* I - Hold until time */
{
  ipp_attribute_t	*attr;		/* job-hold-until attribute */
  struct tm		*holddate;	/* Hold date */
  char			holdstr[64];	/* Hold time */


 /*
  * Set the hold_until value and hold the job...
  */

  LogMessage(L_DEBUG, "set_hold_until: hold_until = %d", (int)holdtime);

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
    SetString(&attr->values[0].string.text, holdstr);

  SaveJob(job->id);
}


/*
 * End of "$Id: job.c,v 1.124.2.86 2004/06/29 03:28:08 mike Exp $".
 */
