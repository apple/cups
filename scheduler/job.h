/*
 * "$Id$"
 *
 *   Print job definitions for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Job request structure...
 */

typedef struct cupsd_job_s
{
  int			id,		/* Job ID */
			priority,	/* Job priority */
			dirty;		/* Do we need to write the "c" file? */
  ipp_jstate_t		state_value;	/* Cached job-state */
  int			pending_timeout;/* Non-zero if the job was created and waiting on files */
  char			*username;	/* Printing user */
  char			*dest;		/* Destination printer or class */
  cups_ptype_t		dtype;		/* Destination type (class/remote bits) */
  int			num_files;	/* Number of files in job */
  mime_type_t		**filetypes;	/* File types */
  int			*compressions;	/* Compression status of each file */
  time_t		access_time;	/* Last access time */
  ipp_attribute_t	*sheets;	/* job-media-sheets-completed */
  time_t		hold_until;	/* Hold expiration date/time */
  ipp_attribute_t	*state;		/* Job state */
  ipp_attribute_t	*job_sheets;	/* Job sheets (NULL if none) */
  ipp_attribute_t	*printer_message,
					/* job-printer-state-message */
			*printer_reasons;
					/* job-printer-state-reasons */
  int			current_file;	/* Current file in job */
  ipp_t			*attrs;		/* Job attributes */
  int			print_pipes[2],	/* Print data pipes */
			back_pipes[2],	/* Backchannel pipes */
			side_pipes[2],	/* Sidechannel pipes */
			status_pipes[2];/* Status pipes */
  cupsd_statbuf_t	*status_buffer;	/* Status buffer for this job */
  int			status_level;	/* Highest log level in a status message */
  int			cost;		/* Filtering cost */
  int			filters[MAX_FILTERS + 1];
					/* Filter process IDs, 0 terminated */
  int			backend;	/* Backend process ID */
  int			status;		/* Status code from filters */
  cupsd_printer_t	*printer;	/* Printer this job is assigned to */
  int			tries;		/* Number of tries for this job */
  char			*auth_username,	/* AUTH_USERNAME environment variable, if any */
			*auth_domain,	/* AUTH_DOMAIN environment variable, if any */
			*auth_password;	/* AUTH_PASSWORD environment variable, if any */
  void			*profile;	/* Security profile */
#ifdef HAVE_GSSAPI
  krb5_ccache		ccache;		/* Kerberos credential cache */
  char			*ccname;	/* KRB5CCNAME environment variable */
#endif /* HAVE_GSSAPI */
} cupsd_job_t;


/*
 * Globals...
 */

VAR int			JobHistory	VALUE(1);
					/* Preserve job history? */
VAR int			JobFiles	VALUE(0);
					/* Preserve job files? */
VAR int			MaxJobs		VALUE(0),
					/* Max number of jobs */
			MaxActiveJobs	VALUE(0),
					/* Max number of active jobs */
			MaxJobsPerUser	VALUE(0),
					/* Max jobs per user */
			MaxJobsPerPrinter VALUE(0);
					/* Max jobs per printer */
VAR int			JobAutoPurge	VALUE(0);
					/* Automatically purge jobs */
VAR cups_array_t	*Jobs		VALUE(NULL),
					/* List of current jobs */
			*ActiveJobs	VALUE(NULL),
					/* List of active jobs */
			*PrintingJobs	VALUE(NULL);
					/* List of jobs that are printing */
VAR int			NextJobId	VALUE(1);
					/* Next job ID to use */
VAR int			JobRetryLimit	VALUE(5),
					/* Max number of tries */
			JobRetryInterval VALUE(300);
					/* Seconds between retries */


/*
 * Prototypes...
 */

extern cupsd_job_t	*cupsdAddJob(int priority, const char *dest);
extern void		cupsdCancelJob(cupsd_job_t *job, int purge,
			               ipp_jstate_t newstate);
extern void		cupsdCancelJobs(const char *dest, const char *username,
			                int purge);
extern void		cupsdCheckJobs(void);
extern void		cupsdCleanJobs(void);
extern void		cupsdDeleteJob(cupsd_job_t *job);
extern cupsd_job_t	*cupsdFindJob(int id);
extern void		cupsdFinishJob(cupsd_job_t *job);
extern void		cupsdFreeAllJobs(void);
extern int		cupsdGetPrinterJobCount(const char *dest);
extern int		cupsdGetUserJobCount(const char *username);
extern void		cupsdHoldJob(cupsd_job_t *job);
extern void		cupsdLoadAllJobs(void);
extern void		cupsdLoadJob(cupsd_job_t *job);
extern void		cupsdMoveJob(cupsd_job_t *job, cupsd_printer_t *p);
extern void		cupsdReleaseJob(cupsd_job_t *job);
extern void		cupsdRestartJob(cupsd_job_t *job);
extern void		cupsdSaveAllJobs(void);
extern void		cupsdSaveJob(cupsd_job_t *job);
extern void		cupsdSetJobHoldUntil(cupsd_job_t *job, const char *when);
extern void		cupsdSetJobPriority(cupsd_job_t *job, int priority);
extern void		cupsdStopAllJobs(int force);
extern void		cupsdStopJob(cupsd_job_t *job, int force);
extern int		cupsdTimeoutJob(cupsd_job_t *job);
extern void		cupsdUnloadCompletedJobs(void);


/*
 * End of "$Id$".
 */
