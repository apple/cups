/*
 * "$Id: job.h,v 1.15 2000/03/30 05:19:29 mike Exp $"
 *
 *   Print job definitions for the Common UNIX Printing System (CUPS) scheduler.
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
 */

/*
 * Job request structure...
 */

typedef struct job_str
{
  struct job_str *next;			/* Next job in queue */
  int		id,			/* Job ID */
		priority;		/* Job priority */
  ipp_attribute_t *state;		/* Job state */
  char		username[16];		/* Printing user */
  char		dest[IPP_MAX_NAME];	/* Destination printer or class */
  cups_ptype_t	dtype;			/* Destination type (class/remote bits) */
  char		title[IPP_MAX_NAME];	/* Job name/title */
  ipp_attribute_t *job_sheets;		/* Job sheets (NULL if none) */
  int		num_files;		/* Number of files in job */
  int		current_file;		/* Current file in job */
  mime_type_t	**filetypes;		/* File types */
  ipp_t		*attrs;			/* Job attributes */
  int		pipe;			/* Status pipe for this job */
  int		procs[MAX_FILTERS + 2];	/* Process IDs, 0 terminated */
  int		status;			/* Status code from filters */
  printer_t	*printer;		/* Printer this job is assigned to */
} job_t;


/*
 * Globals...
 */

VAR int		JobHistory	VALUE(1);	/* Preserve job history? */
VAR int		JobFiles	VALUE(0);	/* Preserve job files? */
VAR int		NumJobs		VALUE(0);	/* Number of jobs in queue */
VAR job_t	*Jobs		VALUE(NULL);	/* List of current jobs */
VAR int		NextJobId	VALUE(1);	/* Next job ID to use */


/*
 * Prototypes...
 */

extern job_t	*AddJob(int priority, const char *dest);
extern void	CancelJob(int id);
extern void	CancelJobs(const char *dest);
extern void	CheckJobs(void);
extern void	DeleteJob(int id);
extern job_t	*FindJob(int id);
extern void	HoldJob(int id);
extern void	LoadAllJobs(void);
extern void	MoveJob(int id, const char *dest);
extern void	ReleaseJob(int id);
extern void	RestartJob(int id);
extern void	SaveJob(int id);
extern void	SetJobPriority(int id, int priority);
extern void	StartJob(int id, printer_t *printer);
extern void	StopAllJobs(void);
extern void	StopJob(int id);
extern void	UpdateJob(job_t *job);
extern const char *ValidateDest(const char *resource, cups_ptype_t *dtype);


/*
 * End of "$Id: job.h,v 1.15 2000/03/30 05:19:29 mike Exp $".
 */
