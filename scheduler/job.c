/*
 * "$Id: job.c,v 1.50 2000/01/21 03:58:45 mike Exp $"
 *
 *   Job management routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products, all rights reserved.
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
 *   AddJob()         - Add a new job to the job queue...
 *   CancelJob()      - Cancel the specified print job.
 *   CancelJobs()     - Cancel all jobs on the given printer or class.
 *   CheckJobs()      - Check the pending jobs and start any if the destination
 *                      is available.
 *   FindJob()        - Find the specified job.
 *   HoldJob()        - Hold the specified job.
 *   LoadAllJobs()    - Load all jobs from disk.
 *   LoadJob()        - Load a job from disk.
 *   MoveJob()        - Move the specified job to a different destination.
 *   RestartJob()     - Resume the specified job.
 *   SaveJob()        - Save a job to disk.
 *   StartJob()       - Start a print job.
 *   StopAllJobs()    - Stop all print jobs.
 *   StopJob()        - Stop a print job.
 *   UpdateJob()      - Read a status update from a job's filters.
 *   ValidateDest()   - Validate a printer/class destination.
 *   ipp_read_file()  - Read an IPP request from a file.
 *   ipp_write_file() - Write an IPP request to a file.
 *   start_process()  - Start a background process.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"

#if defined(WIN32) || defined(__EMX__)
#  include <windows.h>
#elif HAVE_DIRENT_H
#  include <dirent.h>
typedef struct dirent DIRENT;
#  define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#  if HAVE_SYS_NDIR_H
#    include <sys/ndir.h>
#  endif
#  if HAVE_SYS_DIR_H
#    include <sys/dir.h>
#  endif
#  if HAVE_NDIR_H
#    include <ndir.h>
#  endif
typedef struct direct DIRENT;
#  define NAMLEN(dirent) (dirent)->d_namlen
#endif


/*
 * Local functions...
 */

static ipp_state_t	ipp_read_file(const char *filename, ipp_t *ipp);
static ipp_state_t	ipp_write_file(const char *filename, ipp_t *ipp);
static int		start_process(const char *command, char *argv[],
			              char *envp[], int in, int out, int err,
				      int root);


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

  job->id       = NextJobId ++;
  job->priority = priority;
  strncpy(job->dest, dest, sizeof(job->dest) - 1);

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
CancelJob(int id)		/* I - Job to cancel */
{
  int	i;			/* Looping var */
  job_t	*current,		/* Current job */
	*prev;			/* Previous job in list */
  char	filename[1024];		/* Job filename */


  DEBUG_printf(("CancelJob(%d)\n", id));

  for (current = Jobs, prev = NULL; current != NULL; prev = current, current = current->next)
    if (current->id == id)
    {
     /*
      * Stop any processes that are working on the current...
      */

      DEBUG_puts("CancelJob: found job in list.");

      if (current->state->values[0].integer == IPP_JOB_PROCESSING)
	StopJob(current->id);

      current->state->values[0].integer = IPP_JOB_CANCELLED;

     /*
      * Remove the print file for good if we aren't preserving jobs or
      * files...
      */

      current->current_file = 0;

      if (!JobHistory || !JobFiles)
        for (i = 1; i <= current->num_files; i ++)
	{
	  snprintf(filename, sizeof(filename), "%s/d%05d-%03d", RequestRoot,
	           current->id, i);
          unlink(filename);
	}

      if (JobHistory)
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

        free(current);
      }

      return;
    }
}


/*
 * 'CancelJobs()' - Cancel all jobs on the given printer or class.
 */

void
CancelJobs(const char *dest)	/* I - Destination to cancel */
{
  job_t	*current,		/* Current job */
	*prev;			/* Previous job in list */


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
  job_t		*current,	/* Current job in queue */
		*prev;		/* Previous job in queue */
  printer_t	*printer;	/* Printer/class destination */


  DEBUG_puts("CheckJobs()");

  for (current = Jobs, prev = NULL; current != NULL; prev = current)
  {
    DEBUG_printf(("CheckJobs: current->state->values[0].integer = %d\n",
                  current->state->values[0].integer));

   /*
    * Start pending jobs if the destination is available...
    */

    if (current->state->values[0].integer == IPP_JOB_PENDING)
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
 * 'HoldJob()' - Hold the specified job.
 */

void
HoldJob(int id)			/* I - Job ID */
{
  job_t	*job;			/* Job data */


  if ((job = FindJob(id)) == NULL)
    return;

  if (job->state->values[0].integer == IPP_JOB_PROCESSING)
    StopJob(id);

  job->state->values[0].integer = IPP_JOB_HELD;

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
  const char	*dest;		/* Destination */
  mime_type_t	**filetypes;	/* New filetypes array */


 /*
  * First open the requests directory...
  */

  if ((dir = opendir(RequestRoot)) == NULL)
    return;

 /*
  * Read all the c##### files...
  */

  while ((dent = readdir(dir)) != NULL)
    if (NAMLEN(dent) == 6 && dent->d_name[0] == 'c')
    {
     /*
      * Allocate memory for the job...
      */

      if ((job = calloc(sizeof(job_t), 1)) == NULL)
      {
        LogMessage(LOG_ERROR, "LoadAddJobs: Ran out of memory for jobs!");
	closedir(dir);
	return;
      }

      if ((job->attrs = ippNew()) == NULL)
      {
        free(job);
        LogMessage(LOG_ERROR, "LoadAddJobs: Ran out of memory for job attributes!");
	closedir(dir);
	return;
      }

     /*
      * Assign the job ID...
      */

      job->id = atoi(dent->d_name + 1);

      if (job->id >= NextJobId)
        NextJobId = job->id + 1;

     /*
      * Load the job control file...
      */

      snprintf(filename, sizeof(filename), "%s/%s", RequestRoot, dent->d_name);
      if (ipp_read_file(filename, job->attrs) != IPP_DATA)
      {
        LogMessage(LOG_ERROR, "LoadAllJobs: Unable to read job control file \"%s\"!",
	           filename);
	ippDelete(job->attrs);
	free(job);
	continue;
      }

      attr = ippFindAttribute(job->attrs, "job-printer-uri", IPP_TAG_URI);
      httpSeparate(attr->values[0].string.text, method, username, host,
                   &port, resource);

      if ((dest = ValidateDest(resource, &(job->dtype))) == NULL)
      {
        LogMessage(LOG_ERROR, "LoadAllJobs: Unable to queue job for destination \"%s\"!",
	           attr->values[0].string.text);
	ippDelete(job->attrs);
	free(job);
	continue;
      }

      strncpy(job->dest, dest, sizeof(job->dest) - 1);

      job->state = ippFindAttribute(job->attrs, "job-state", IPP_TAG_ENUM);

      attr = ippFindAttribute(job->attrs, "job-priority", IPP_TAG_INTEGER);
      job->priority = attr->values[0].integer;

      attr = ippFindAttribute(job->attrs, "job-name", IPP_TAG_NAME);
      strncpy(job->title, attr->values[0].string.text,
              sizeof(job->title) - 1);

      attr = ippFindAttribute(job->attrs, "job-originating-user-name", IPP_TAG_NAME);
      strncpy(job->username, attr->values[0].string.text,
              sizeof(job->username) - 1);

     /*
      * Insert the job into the array, sorting by job priority and ID...
      */

      for (current = Jobs, prev = NULL; current != NULL; prev = current, current = current->next)
	if (job->priority > current->priority)
	  break;
	else if (job->priority == current->priority && job->id < current->id)
	  break;

      job->next = current;
      if (prev != NULL)
	prev->next = job;
      else
	Jobs = job;
    }

 /*
  * Read all the d##### files...
  */

  rewinddir(dir);

  while ((dent = readdir(dir)) != NULL)
    if (NAMLEN(dent) > 7 && dent->d_name[0] == 'd')
    {
     /*
      * Find the job...
      */

      jobid  = atoi(dent->d_name + 1);
      fileid = atoi(dent->d_name + 7);

      snprintf(filename, sizeof(filename), "%s/%s", RequestRoot, dent->d_name);

      if ((job = FindJob(jobid)) == NULL)
      {
        LogMessage(LOG_ERROR, "LoadAddJobs: Orphaned print file \"%s\"!",
	           filename);
	continue;
      }

      if (fileid > job->num_files)
      {
        if (job->num_files == 0)
	  filetypes = (mime_type_t **)calloc(sizeof(mime_type_t *), fileid);
	else
	  filetypes = (mime_type_t **)realloc(job->filetypes,
	                                    sizeof(mime_type_t *) * fileid);

        if (filetypes == NULL)
	{
          LogMessage(LOG_ERROR, "LoadAddJobs: Ran out of memory for job file types!");
	  continue;
	}

        job->filetypes = filetypes;
	job->num_files = fileid;
      }

      job->filetypes[fileid - 1] = mimeFileType(MimeDatabase, filename);
    }

 /*
  * Check to see if we need to start any jobs...
  */

  CheckJobs();
}


/*
 * 'MoveJob()' - Move the specified job to a different destination.
 */

void
MoveJob(int        id,		/* I - Job ID */
        const char *dest)	/* I - Destination */
{
  job_t	*current;		/* Current job */


  for (current = Jobs; current != NULL; current = current->next)
    if (current->id == id)
    {
      if (current->state->values[0].integer == IPP_JOB_PENDING)
        strncpy(current->dest, dest, sizeof(current->dest) - 1);

      SaveJob(current->id);

      return;
    }
}


/*
 * 'RestartJob()' - Resume the specified job.
 */

void
RestartJob(int id)		/* I - Job ID */
{
  job_t	*job;			/* Job data */


  if ((job = FindJob(id)) == NULL)
    return;

  if (job->state->values[0].integer == IPP_JOB_HELD)
  {
    job->state->values[0].integer = IPP_JOB_PENDING;
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


  if ((job = FindJob(id)) == NULL)
    return;

  snprintf(filename, sizeof(filename), "%s/c%05d", RequestRoot, id);
  ipp_write_file(filename, job->attrs);
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
		filename[1024],	/* Job filename */
		command[1024],	/* Full path to filter/backend command */
		jobid[255],	/* Job ID string */
		title[IPP_MAX_NAME],
				/* Job title string */
		copies[255],	/* # copies string */
		options[16384],	/* Full list of options */
		*envp[15],	/* Environment variables */
		language[255],	/* LANG environment variable */
		charset[255],	/* CHARSET environment variable */
		content_type[255],/* CONTENT_TYPE environment variable */
		device_uri[1024],/* DEVICE_URI environment variable */
		ppd[1024],	/* PPD environment variable */
		printer_name[255],/* PRINTER environment variable */
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

  current->state->values[0].integer = IPP_JOB_PROCESSING;
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

    filters = mimeFilter(MimeDatabase, current->filetypes[current->current_file],
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
    else if (attr->group_tag == IPP_TAG_JOB &&
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
  sprintf(filename, "%s/d%05d-%03d", RequestRoot, current->id,
          current->current_file + 1);

  argv[0] = printer->name;
  argv[1] = jobid;
  argv[2] = current->username;
  argv[3] = title;
  argv[4] = copies;
  argv[5] = options;
  argv[6] = filename;
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

  sprintf(content_type, "CONTENT_TYPE=%s/%s",
          current->filetypes[current->current_file]->super,
          current->filetypes[current->current_file]->type);
  sprintf(device_uri, "DEVICE_URI=%s", printer->device_uri);
  sprintf(ppd, "PPD=%s/ppd/%s.ppd", ServerRoot, printer->name);
  sprintf(printer_name, "PRINTER=%s", printer->name);
  sprintf(cache, "RIP_MAX_CACHE=%s", RIPCache);
  sprintf(root, "SERVER_ROOT=%s", ServerRoot);
  sprintf(tmpdir, "TMPDIR=%s", TempDir);

  envp[0]  = "PATH=/bin:/usr/bin";
  envp[1]  = "SOFTWARE=CUPS/1.1";
  envp[2]  = "TZ=GMT";
  envp[3]  = "USER=root";
  envp[4]  = charset;
  envp[5]  = language;
  envp[6]  = TZ;
  envp[7]  = ppd;
  envp[8]  = root;
  envp[9]  = cache;
  envp[10] = tmpdir;
  envp[11] = content_type;
  envp[12] = device_uri;
  envp[13] = printer_name;
  envp[14] = NULL;

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
  DEBUG_puts(envp[11]);
  DEBUG_puts(envp[12]);
  DEBUG_puts(envp[13]);

  current->current_file ++;

 /*
  * Now create processes for all of the filters...
  */

  if (pipe(statusfds))
  {
    LogMessage(LOG_ERROR, "StartJob: unable to create status pipes - %s.",
	       strerror(errno));
    StopPrinter(printer);
    sprintf(printer->state_message, "Unable to create status pipes - %s.",
            strerror(errno));
    return;
  }

  DEBUG_printf(("statusfds = %d, %d\n", statusfds[0], statusfds[1]));

  current->pipe   = statusfds[0];
  current->status = 0;
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
      sprintf(command, "%s/filter/%s", ServerBin, filters[i].filter);
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
                        filterfds[i & 1][1], statusfds[1], 0);

    close(filterfds[!(i & 1)][0]);
    close(filterfds[!(i & 1)][1]);

    if (pid == 0)
    {
      LogMessage(LOG_ERROR, "Unable to start filter \"%s\" - %s.",
                 filters[i].filter, strerror(errno));
      StopPrinter(current->printer);
      sprintf(printer->state_message, "Unable to start filter \"%s\" - %s.",
              filters[i].filter, strerror(errno));
      return;
    }
    else
    {
      current->procs[i] = pid;

      LogMessage(LOG_DEBUG, "Started %s (PID %d) for job %d.", command, pid,
                 current->id);
    }
  }

  if (filters != NULL)
    free(filters);

 /*
  * Finally, pipe the final output into a backend process if needed...
  */

  if (strncmp(printer->device_uri, "file:", 5) != 0)
  {
    sscanf(printer->device_uri, "%254[^:]", method);
    sprintf(command, "%s/backend/%s", ServerBin, method);

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
			filterfds[i & 1][1], statusfds[1], 1);

    close(filterfds[!(i & 1)][0]);
    close(filterfds[!(i & 1)][1]);

    if (pid == 0)
    {
      LogMessage(LOG_ERROR, "Unable to start backend \"%s\" - %s.",
                 method, strerror(errno));
      StopPrinter(current->printer);
      sprintf(printer->state_message, "Unable to start backend \"%s\" - %s.",
              method, strerror(errno));
      return;
    }
    else
    {
      current->procs[i] = pid;

      LogMessage(LOG_DEBUG, "Started %s (PID %d) for job %d.", command, pid,
                 current->id);
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
 * 'StopAllJobs()' - Stop all print jobs.
 */

void
StopAllJobs(void)
{
  job_t	*current;		/* Current job */


  DEBUG_puts(("StopAllJobs()\n", id));

  for (current = Jobs; current != NULL; current = current->next)
    if (current->state->values[0].integer == IPP_JOB_PROCESSING)
    {
      StopJob(current->id);
      current->state->values[0].integer = IPP_JOB_PENDING;
    }
}


/*
 * 'StopJob()' - Stop a print job.
 */

void
StopJob(int id)			/* I - Job ID */
{
  int	i;			/* Looping var */
  job_t	*current;		/* Current job */


  DEBUG_printf(("StopJob(%d)\n", id));

  for (current = Jobs; current != NULL; current = current->next)
    if (current->id == id)
    {
      DEBUG_puts("StopJob: found job in list.");

      if (current->state->values[0].integer == IPP_JOB_PROCESSING)
      {
        DEBUG_puts("StopJob: job state is \'processing\'.");

        if (current->status < 0)
	  SetPrinterState(current->printer, IPP_PRINTER_STOPPED);
	else
	  SetPrinterState(current->printer, IPP_PRINTER_IDLE);

        DEBUG_printf(("StopJob: printer state is %d\n", current->printer->state));

	current->state->values[0].integer = IPP_JOB_STOPPED;
        current->printer->job = NULL;
        current->printer      = NULL;

	current->current_file --;

        for (i = 0; current->procs[i]; i ++)
	  if (current->procs[i] > 0)
	  {
	    kill(current->procs[i], SIGTERM);
	    current->procs[i] = 0;
	  }

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
      else if (strncmp(buffer, "INFO:", 5) == 0)
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

	if ((loglevel == LOG_INFO && !job->status) ||
	    loglevel < LOG_INFO)
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

    if (job->status < 0)
    {
     /*
      * Backend had errors; stop it...
      */

      StopJob(job->id);
      job->state->values[0].integer = IPP_JOB_PENDING;
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
        CancelJob(job->id);

        if (JobHistory)
          job->state->values[0].integer = IPP_JOB_ABORTED;

        CheckJobs();
      }
    }
    else
    {
     /*
      * Job printed successfully; cancel it...
      */

      if (job->current_file < job->num_files)
        StartJob(job->id, job->printer);
      else
      {
	CancelJob(job->id);

	if (JobHistory)
          job->state->values[0].integer = IPP_JOB_COMPLETED;

	CheckJobs();
      }
    }
  }
}


/*
 * 'ValidateDest()' - Validate a printer/class destination.
 */

const char *				/* O - Printer or class name */
ValidateDest(const char   *resource,	/* I - Resource name */
             cups_ptype_t *dtype)	/* O - Type (printer or class) */
{
  if (strncmp(resource, "/classes/", 9) == 0)
  {
   /*
    * Print to a class...
    */

    *dtype = CUPS_PRINTER_CLASS;

    if (FindClass(resource + 9) == NULL)
      return (NULL);
    else
      return (resource + 9);
  }
  else if (strncmp(resource, "/printers/", 10) == 0)
  {
   /*
    * Print to a specific printer...
    */

    *dtype = (cups_ptype_t)0;

    if (FindPrinter(resource + 10) == NULL)
    {
      *dtype = CUPS_PRINTER_CLASS;

      if (FindClass(resource + 10) == NULL)
        return (NULL);
    }

    return (resource + 10);
  }
  else
    return (NULL);
}


/*
 * 'ipp_read_file()' - Read an IPP request from a file.
 */

static ipp_state_t			/* O - State */
ipp_read_file(const char *filename,	/* I - File to read from */
              ipp_t      *ipp)		/* I - Request to read into */
{
  int			fd;		/* File descriptor for file */
  int			n;		/* Length of data */
  unsigned char		buffer[8192];	/* Data buffer */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_tag_t		tag;		/* Current tag */


 /*
  * Open the file if possible...
  */

  if (filename == NULL || ipp == NULL)
    return (IPP_ERROR);

  if ((fd = open(filename, O_RDONLY)) == -1)
    return (IPP_ERROR);

 /*
  * Read the IPP request...
  */

  ipp->state = IPP_IDLE;

  switch (ipp->state)
  {
    case IPP_IDLE :
        ipp->state ++; /* Avoid common problem... */

    case IPP_HEADER :
       /*
        * Get the request header...
	*/

        if ((n = read(fd, buffer, 8)) < 8)
	{
	  DEBUG_printf(("ippRead: Unable to read header (%d bytes read)!\n", n));
	  close(fd);
	  return (n == 0 ? IPP_IDLE : IPP_ERROR);
	}

       /*
        * Verify the major version number...
	*/

	if (buffer[0] != 1)
	{
	  DEBUG_printf(("ippRead: version number (%d.%d) is bad.\n", buffer[0],
	                buffer[1]));
	  close(fd);
	  return (IPP_ERROR);
	}

       /*
        * Then copy the request header over...
	*/

        ipp->request.any.version[0]  = buffer[0];
        ipp->request.any.version[1]  = buffer[1];
        ipp->request.any.op_status   = (buffer[2] << 8) | buffer[3];
        ipp->request.any.request_id  = (((((buffer[4] << 8) | buffer[5]) << 8) |
	                               buffer[6]) << 8) | buffer[7];

        ipp->state   = IPP_ATTRIBUTE;
	ipp->current = NULL;
	ipp->curtag  = IPP_TAG_ZERO;

    case IPP_ATTRIBUTE :
        while (read(fd, buffer, 1) > 0)
	{
	 /*
	  * Read this attribute...
	  */

          tag = (ipp_tag_t)buffer[0];

	  if (tag == IPP_TAG_END)
	  {
	   /*
	    * No more attributes left...
	    */

            DEBUG_puts("ippRead: IPP_TAG_END!");

	    ipp->state = IPP_DATA;
	    break;
	  }
          else if (tag < IPP_TAG_UNSUPPORTED_VALUE)
	  {
	   /*
	    * Group tag...  Set the current group and continue...
	    */

            if (ipp->curtag == tag)
	      ippAddSeparator(ipp);

	    ipp->curtag  = tag;
	    ipp->current = NULL;
	    DEBUG_printf(("ippRead: group tag = %x\n", tag));
	    continue;
	  }

          DEBUG_printf(("ippRead: value tag = %x\n", tag));

         /*
	  * Get the name...
	  */

          if (read(fd, buffer, 2) < 2)
	  {
	    DEBUG_puts("ippRead: unable to read name length!");
	    close(fd);
	    return (IPP_ERROR);
	  }

          n = (buffer[0] << 8) | buffer[1];

          DEBUG_printf(("ippRead: name length = %d\n", n));

          if (n == 0)
	  {
	   /*
	    * More values for current attribute...
	    */

            if (ipp->current == NULL)
	    {
	      close(fd);
              return (IPP_ERROR);
	    }

            attr = ipp->current;

	    if (attr->num_values >= IPP_MAX_VALUES)
	    {
	      close(fd);
              return (IPP_ERROR);
	    }
	  }
	  else
	  {
	   /*
	    * New attribute; read the name and add it...
	    */

	    if (read(fd, buffer, n) < n)
	    {
	      DEBUG_puts("ippRead: unable to read name!");
	      close(fd);
	      return (IPP_ERROR);
	    }

	    buffer[n] = '\0';
	    DEBUG_printf(("ippRead: name = \'%s\'\n", buffer));

	    attr = ipp->current = _ipp_add_attr(ipp, IPP_MAX_VALUES);

	    attr->group_tag  = ipp->curtag;
	    attr->value_tag  = tag;
	    attr->name       = strdup((char *)buffer);
	    attr->num_values = 0;
	  }

	  if (read(fd, buffer, 2) < 2)
	  {
	    DEBUG_puts("ippRead: unable to read value length!");
	    close(fd);
	    return (IPP_ERROR);
	  }

	  n = (buffer[0] << 8) | buffer[1];
          DEBUG_printf(("ippRead: value length = %d\n", n));

	  switch (tag)
	  {
	    case IPP_TAG_INTEGER :
	    case IPP_TAG_ENUM :
	        if (read(fd, buffer, 4) < 4)
	        {
	          close(fd);
                  return (IPP_ERROR);
	        }

		n = (((((buffer[0] << 8) | buffer[1]) << 8) | buffer[2]) << 8) |
		    buffer[3];

                attr->values[attr->num_values].integer = n;
	        break;
	    case IPP_TAG_BOOLEAN :
	        if (read(fd, buffer, 1) < 1)
	        {
	          close(fd);
                  return (IPP_ERROR);
	        }

                attr->values[attr->num_values].boolean = buffer[0];
	        break;
	    case IPP_TAG_TEXT :
	    case IPP_TAG_NAME :
	    case IPP_TAG_KEYWORD :
	    case IPP_TAG_STRING :
	    case IPP_TAG_URI :
	    case IPP_TAG_URISCHEME :
	    case IPP_TAG_CHARSET :
	    case IPP_TAG_LANGUAGE :
	    case IPP_TAG_MIMETYPE :
	        if (read(fd, buffer, n) < n)
	        {
	          close(fd);
                  return (IPP_ERROR);
	        }

                buffer[n] = '\0';
		DEBUG_printf(("ippRead: value = \'%s\'\n", buffer));

                attr->values[attr->num_values].string.text = strdup((char *)buffer);
	        break;
	    case IPP_TAG_DATE :
	        if (read(fd, buffer, 11) < 11)
	        {
	          close(fd);
                  return (IPP_ERROR);
	        }

                memcpy(attr->values[attr->num_values].date, buffer, 11);
	        break;
	    case IPP_TAG_RESOLUTION :
	        if (read(fd, buffer, 9) < 9)
	        {
	          close(fd);
                  return (IPP_ERROR);
	        }

                attr->values[attr->num_values].resolution.xres =
		    (((((buffer[0] << 8) | buffer[1]) << 8) | buffer[2]) << 8) |
		    buffer[3];
                attr->values[attr->num_values].resolution.yres =
		    (((((buffer[4] << 8) | buffer[5]) << 8) | buffer[6]) << 8) |
		    buffer[7];
                attr->values[attr->num_values].resolution.units =
		    (ipp_res_t)buffer[8];
	        break;
	    case IPP_TAG_RANGE :
	        if (read(fd, buffer, 8) < 8)
	        {
	          close(fd);
                  return (IPP_ERROR);
	        }

                attr->values[attr->num_values].range.lower =
		    (((((buffer[0] << 8) | buffer[1]) << 8) | buffer[2]) << 8) |
		    buffer[3];
                attr->values[attr->num_values].range.upper =
		    (((((buffer[4] << 8) | buffer[5]) << 8) | buffer[6]) << 8) |
		    buffer[7];
	        break;
	    case IPP_TAG_TEXTLANG :
	    case IPP_TAG_NAMELANG :
	        if (read(fd, buffer, n) < n)
	        {
	          close(fd);
                  return (IPP_ERROR);
	        }

                buffer[n] = '\0';

                attr->values[attr->num_values].string.charset = strdup((char *)buffer);

	        if (read(fd, buffer, 2) < 2)
	        {
	          close(fd);
                  return (IPP_ERROR);
	        }

		n = (buffer[0] << 8) | buffer[1];

	        if (read(fd, buffer, n) < n)
	        {
	          close(fd);
                  return (IPP_ERROR);
	        }

                buffer[n] = '\0';

                attr->values[attr->num_values].string.text = strdup((char *)buffer);
	        break;
	  }

          attr->num_values ++;
	}
        break;

    case IPP_DATA :
        break;
  }

 /*
  * Close the file and return...
  */

  close(fd);

  return (ipp->state);
}


/*
 * 'ipp_write_file()' - Write an IPP request to a file.
 */

static ipp_state_t			/* O - State */
ipp_write_file(const char *filename,	/* I - File to write to */
               ipp_t      *ipp)		/* I - Request to write */
{
  int			fd;		/* File descriptor */
  int			i;		/* Looping var */
  int			n;		/* Length of data */
  unsigned char		buffer[8192],	/* Data buffer */
			*bufptr;	/* Pointer into buffer */
  ipp_attribute_t	*attr;		/* Current attribute */


 /*
  * Open the file if possible...
  */

  if (filename == NULL || ipp == NULL)
    return (IPP_ERROR);

  if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0640)) == -1)
    return (IPP_ERROR);

  fchmod(fd, 0640);
  fchown(fd, User, Group);

 /*
  * Write the IPP request...
  */

  ipp->state = IPP_IDLE;

  switch (ipp->state)
  {
    case IPP_IDLE :
        ipp->state ++; /* Avoid common problem... */

    case IPP_HEADER :
       /*
        * Send the request header...
	*/

        bufptr = buffer;

	*bufptr++ = 1;
	*bufptr++ = 0;
	*bufptr++ = ipp->request.any.op_status >> 8;
	*bufptr++ = ipp->request.any.op_status;
	*bufptr++ = ipp->request.any.request_id >> 24;
	*bufptr++ = ipp->request.any.request_id >> 16;
	*bufptr++ = ipp->request.any.request_id >> 8;
	*bufptr++ = ipp->request.any.request_id;

        if (write(fd, (char *)buffer, bufptr - buffer) < 0)
	{
	  DEBUG_puts("ippWrite: Could not write IPP header...");
	  close(fd);
	  return (IPP_ERROR);
	}

        ipp->state   = IPP_ATTRIBUTE;
	ipp->current = ipp->attrs;
	ipp->curtag  = IPP_TAG_ZERO;

    case IPP_ATTRIBUTE :
        while (ipp->current != NULL)
	{
	 /*
	  * Write this attribute...
	  */

	  bufptr = buffer;
	  attr   = ipp->current;

	  ipp->current = ipp->current->next;

          if (ipp->curtag != attr->group_tag)
	  {
	   /*
	    * Send a group operation tag...
	    */

	    ipp->curtag = attr->group_tag;

            if (attr->group_tag == IPP_TAG_ZERO)
	      continue;

            DEBUG_printf(("ippWrite: wrote group tag = %x\n", attr->group_tag));
	    *bufptr++ = attr->group_tag;
	  }

          n = strlen(attr->name);

          DEBUG_printf(("ippWrite: writing value tag = %x\n", attr->value_tag));
          DEBUG_printf(("ippWrite: writing name = %d, \'%s\'\n", n, attr->name));

          *bufptr++ = attr->value_tag;
	  *bufptr++ = n >> 8;
	  *bufptr++ = n;
	  memcpy(bufptr, attr->name, n);
	  bufptr += n;

	  switch (attr->value_tag)
	  {
	    case IPP_TAG_INTEGER :
	    case IPP_TAG_ENUM :
	        for (i = 0; i < attr->num_values; i ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

	          *bufptr++ = 0;
		  *bufptr++ = 4;
		  *bufptr++ = attr->values[i].integer >> 24;
		  *bufptr++ = attr->values[i].integer >> 16;
		  *bufptr++ = attr->values[i].integer >> 8;
		  *bufptr++ = attr->values[i].integer;
		}
		break;

	    case IPP_TAG_BOOLEAN :
	        for (i = 0; i < attr->num_values; i ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

	          *bufptr++ = 0;
		  *bufptr++ = 1;
		  *bufptr++ = attr->values[i].boolean;
		}
		break;

	    case IPP_TAG_TEXT :
	    case IPP_TAG_NAME :
	    case IPP_TAG_KEYWORD :
	    case IPP_TAG_STRING :
	    case IPP_TAG_URI :
	    case IPP_TAG_URISCHEME :
	    case IPP_TAG_CHARSET :
	    case IPP_TAG_LANGUAGE :
	    case IPP_TAG_MIMETYPE :
	        for (i = 0; i < attr->num_values; i ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

        	    DEBUG_printf(("ippWrite: writing value tag = %x\n",
		                  attr->value_tag));
        	    DEBUG_printf(("ippWrite: writing name = 0, \'\'\n"));

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

                  n = strlen(attr->values[i].string.text);

                  DEBUG_printf(("ippWrite: writing string = %d, \'%s\'\n", n,
		                attr->values[i].string.text));

                  if ((sizeof(buffer) - (bufptr - buffer)) < (n + 2))
		  {
                    if (write(fd, (char *)buffer, bufptr - buffer) < 0)
	            {
	              DEBUG_puts("ippWrite: Could not write IPP attribute...");
	              close(fd);
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

	          *bufptr++ = n >> 8;
		  *bufptr++ = n;
		  memcpy(bufptr, attr->values[i].string.text, n);
		  bufptr += n;
		}
		break;

	    case IPP_TAG_DATE :
	        for (i = 0; i < attr->num_values; i ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

	          *bufptr++ = 0;
		  *bufptr++ = 11;
		  memcpy(bufptr, attr->values[i].date, 11);
		  bufptr += 11;
		}
		break;

	    case IPP_TAG_RESOLUTION :
	        for (i = 0; i < attr->num_values; i ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

	          *bufptr++ = 0;
		  *bufptr++ = 9;
		  *bufptr++ = attr->values[i].resolution.xres >> 24;
		  *bufptr++ = attr->values[i].resolution.xres >> 16;
		  *bufptr++ = attr->values[i].resolution.xres >> 8;
		  *bufptr++ = attr->values[i].resolution.xres;
		  *bufptr++ = attr->values[i].resolution.yres >> 24;
		  *bufptr++ = attr->values[i].resolution.yres >> 16;
		  *bufptr++ = attr->values[i].resolution.yres >> 8;
		  *bufptr++ = attr->values[i].resolution.yres;
		  *bufptr++ = attr->values[i].resolution.units;
		}
		break;

	    case IPP_TAG_RANGE :
	        for (i = 0; i < attr->num_values; i ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

	          *bufptr++ = 0;
		  *bufptr++ = 8;
		  *bufptr++ = attr->values[i].range.lower >> 24;
		  *bufptr++ = attr->values[i].range.lower >> 16;
		  *bufptr++ = attr->values[i].range.lower >> 8;
		  *bufptr++ = attr->values[i].range.lower;
		  *bufptr++ = attr->values[i].range.upper >> 24;
		  *bufptr++ = attr->values[i].range.upper >> 16;
		  *bufptr++ = attr->values[i].range.upper >> 8;
		  *bufptr++ = attr->values[i].range.upper;
		}
		break;

	    case IPP_TAG_TEXTLANG :
	    case IPP_TAG_NAMELANG :
	        for (i = 0; i < attr->num_values; i ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

                  n = strlen(attr->values[i].string.charset);

                  if ((sizeof(buffer) - (bufptr - buffer)) < (n + 2))
		  {
                    if (write(fd, (char *)buffer, bufptr - buffer) < 0)
	            {
	              DEBUG_puts("ippWrite: Could not write IPP attribute...");
	              close(fd);
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

	          *bufptr++ = n >> 8;
		  *bufptr++ = n;
		  memcpy(bufptr, attr->values[i].string.charset, n);
		  bufptr += n;

                  n = strlen(attr->values[i].string.text);

                  if ((sizeof(buffer) - (bufptr - buffer)) < (n + 2))
		  {
                    if (write(fd, (char *)buffer, bufptr - buffer) < 0)
	            {
	              DEBUG_puts("ippWrite: Could not write IPP attribute...");
	              close(fd);
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

	          *bufptr++ = n >> 8;
		  *bufptr++ = n;
		  memcpy(bufptr, attr->values[i].string.text, n);
		  bufptr += n;
		}
		break;
	  }

         /*
	  * Write the data out...
	  */

          if (write(fd, (char *)buffer, bufptr - buffer) < 0)
	  {
	    DEBUG_puts("ippWrite: Could not write IPP attribute...");
	    close(fd);
	    return (IPP_ERROR);
	  }

          DEBUG_printf(("ippWrite: wrote %d bytes\n", bufptr - buffer));
	}

	if (ipp->current == NULL)
	{
         /*
	  * Done with all of the attributes; add the end-of-attributes tag...
	  */

          buffer[0] = IPP_TAG_END;
	  if (write(fd, (char *)buffer, 1) < 0)
	  {
	    DEBUG_puts("ippWrite: Could not write IPP end-tag...");
	    close(fd);
	    return (IPP_ERROR);
	  }

	  ipp->state = IPP_DATA;
	}
        break;

    case IPP_DATA :
        break;
  }

 /*
  * Close the file and return...
  */

  close(fd);

  return (ipp->state);
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
	      int        root)		/* I - Run as root? */
{
  int	fd;				/* Looping var */
  int	pid;				/* Process ID */


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

    if (!root)
    {
      setgid(Group);
      setuid(User);
    }

   /*
    * Execute the command; if for some reason this doesn't work,
    * return the error code...
    */

    execve(command, argv, envp);

    perror(command);

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
 * End of "$Id: job.c,v 1.50 2000/01/21 03:58:45 mike Exp $".
 */
