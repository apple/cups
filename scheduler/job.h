/*
 * "$Id: job.h,v 1.3 1999/02/09 22:04:15 mike Exp $"
 *
 *   Print job definition for the Common UNIX Printing System (CUPS) scheduler.
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
 */

/*
 * Job request structure...
 */

typedef struct job_str
{
  struct job_str *next;			/* Next job in queue */
  int		id,			/* Job ID */
		priority;		/* Job priority */
  ipp_jstate_t	state;			/* Job state */
  char		dest[IPP_MAX_NAME],	/* Destination printer or class */
		user[MAX_USERPASS],	/* Sending user */
		title[IPP_MAX_NAME];	/* Job title */
  char		filename[HTTP_MAX_URI];	/* Name of job file */
  size_t	offset;			/* Offset to start of file */
  char		**options;		/* Job options, NULL terminated */
  int		*procs;			/* Process IDs, 0 terminated */
  printer_t	*printer;		/* Printer this job is assigned to */
} job_t;


/*
 * Globals...
 */

VAR int		NumJobs		VALUE(0);	/* Number of jobs in queue */
VAR job_t	*Jobs		VALUE(NULL);	/* List of current jobs */
VAR int		NextJobId	VALUE(1);	/* Next job ID to use */


/*
 * Prototypes...
 */

extern job_t	*AddJob(int priority, char *dest);
extern void	CancelJob(int id);
extern void	CancelJobs(char *dest);
extern void	CheckJobs(void);
extern void	DeleteJob(int id);
extern job_t	*FindJob(int id);
extern void	LoadJobs(void);
extern void	MoveJob(int id, char *dest);
extern void	StartJob(int id, printer_t *printer);
extern void	StopJob(int id);


/*
 * End of "$Id: job.h,v 1.3 1999/02/09 22:04:15 mike Exp $".
 */
