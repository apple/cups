/*
 * "$Id: job.c,v 1.3 1999/02/09 22:04:14 mike Exp $"
 *
 *   for the Common UNIX Printing System (CUPS).
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
 *
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


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
  strcpy(job->dest, dest);

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


void
CancelJob(int id)	/* I - Job to cancel */
{
  int	i;		/* Looping var */
  job_t	*current,	/* Current job */
	*prev;		/* Previous job in list */


  for (current = Jobs, prev = NULL; current != NULL; prev = current, current = current->next)
    if (current->id == id)
    {
     /*
      * Update pointers...
      */

      if (prev == NULL)
        Jobs = current->next;
      else
        prev->next = current->next;

     /*
      * Stop any processes that are working on the current...
      */

      if (current->state == IPP_JOB_PROCESSING)
	StopJob(current->id);

     /*
      * Free all memory used...
      */

      if (current->options != NULL)
      {
        for (i = 0; current->options[0] != NULL; i ++)
	  free(current->options[i]);

	free(current->options);
      }

     /*
      * Remove the current for good...
      */

      unlink(current->filename);
      free(current);
      return;
    }
}


void
CancelJobs(char *dest)	/* I - Destination to cancel */
{
  int	i;		/* Looping var */
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


job_t *			/* O - Job data */
FindJob(int id)		/* I - Job ID */
{
  job_t	*current,	/* Current job */
	*prev;		/* Previous job in list */


  for (current = Jobs, prev = NULL; current != NULL; prev = current, current = current->next)
    if (current->id == id)
      break;

  return (current);
}


void
LoadJobs(void)
{
}


void
MoveJob(int id, char *dest)
{
  job_t	*current,	/* Current job */
	*prev;		/* Previous job in list */


  for (current = Jobs, prev = NULL; current != NULL; prev = current, current = current->next)
    if (current->id == id)
    {
      if (current->state == IPP_JOB_PENDING)
        strcpy(current->dest, dest);

      return;
    }
}


void
StopJob(int id)
{
  int	i;		/* Looping var */
  job_t	*current,	/* Current job */
	*prev;		/* Previous job in list */


  for (current = Jobs, prev = NULL; current != NULL; prev = current, current = current->next)
    if (current->id == id)
    {
      if (current->state == IPP_JOB_PROCESSING)
      {
	current->state = IPP_JOB_PENDING;

        for (i = 0; current->procs[i]; i ++)
	  kill(current->procs[i], SIGTERM);

	free(current->procs);
	current->procs        = NULL;
        current->printer->job = NULL;
      }

      return;
    }
}


void
StartJob(int       id,
         printer_t *printer)
{
  job_t	*current,	/* Current job */
	*prev;		/* Previous job in list */


  for (current = Jobs, prev = NULL; current != NULL; prev = current, current = current->next)
    if (current->id == id)
    {
      current->state   = IPP_JOB_PROCESSING;
      current->printer = printer;
      printer->job     = current;
      printer->state   = IPP_PRINTER_PROCESSING;

      /**** DO FORK/EXEC STUFF HERE, TOO! ****/
    }
}


void
CheckJobs(void)
{
  job_t		*current,
		*prev;
  printer_t	*printer;


  for (current = Jobs, prev = NULL; current != NULL; prev = current)
    if (current->state == IPP_JOB_PENDING)
    {
      if ((printer = FindPrinter(current->dest)) == NULL)
        printer = FindAvailablePrinter(current->dest);

      if (printer == NULL && FindClass(current->dest) == NULL)
      {
       /*
        * Whoa, the printer and/or class for this destination went away;
	* cancel the job...
	*/

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

        if (printer->state == IPP_PRINTER_IDLE)
	  StartJob(current->id, printer);

        current = current->next;
      }
    }
    else
      current = current->next;
}


/*
 * End of "$Id: job.c,v 1.3 1999/02/09 22:04:14 mike Exp $".
 */
