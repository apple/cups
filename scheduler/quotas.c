/*
 * "$Id: quotas.c 6947 2007-09-12 21:09:49Z mike $"
 *
 *   Quota routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2009 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cupsdFindQuota()   - Find a quota record.
 *   cupsdFreeQuotas()  - Free quotas for a printer.
 *   cupsdUpdateQuota() - Update quota data for the specified printer and user.
 *   add_quota()        - Add a quota record for this printer and user.
 *   compare_quotas()   - Compare two quota records...
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * Local functions...
 */

static cupsd_quota_t	*add_quota(cupsd_printer_t *p, const char *username);
static int		compare_quotas(const cupsd_quota_t *q1,
			               const cupsd_quota_t *q2);


/*
 * 'cupsdFindQuota()' - Find a quota record.
 */

cupsd_quota_t *				/* O - Quota data */
cupsdFindQuota(
    cupsd_printer_t *p,			/* I - Printer */
    const char      *username)		/* I - User */
{
  cupsd_quota_t	*q,			/* Quota data pointer */
		match;			/* Search data */
  char		*ptr;			/* Pointer into username */


  if (!p || !username)
    return (NULL);

  strlcpy(match.username, username, sizeof(match.username));
  if ((ptr = strchr(match.username, '@')) != NULL)
    *ptr = '\0';			/* Strip @domain/@KDC */

  if ((q = (cupsd_quota_t *)cupsArrayFind(p->quotas, &match)) != NULL)
    return (q);
  else
    return (add_quota(p, username));
}


/*
 * 'cupsdFreeQuotas()' - Free quotas for a printer.
 */

void
cupsdFreeQuotas(cupsd_printer_t *p)	/* I - Printer */
{
  cupsd_quota_t *q;			/* Current quota record */


  if (!p)
    return;

  for (q = (cupsd_quota_t *)cupsArrayFirst(p->quotas);
       q;
       q = (cupsd_quota_t *)cupsArrayNext(p->quotas))
    free(q);

  cupsArrayDelete(p->quotas);

  p->quotas = NULL;
}


/*
 * 'cupsdUpdateQuota()' - Update quota data for the specified printer and user.
 */

cupsd_quota_t *				/* O - Quota data */
cupsdUpdateQuota(
    cupsd_printer_t *p,			/* I - Printer */
    const char      *username,		/* I - User */
    int             pages,		/* I - Number of pages */
    int             k)			/* I - Number of kilobytes */
{
  cupsd_quota_t		*q;		/* Quota data */
  cupsd_job_t		*job;		/* Current job */
  time_t		curtime;	/* Current time */
  ipp_attribute_t	*attr;		/* Job attribute */


  if (!p || !username)
    return (NULL);

  if (!p->k_limit && !p->page_limit)
    return (NULL);

  if ((q = cupsdFindQuota(p, username)) == NULL)
    return (NULL);

  cupsdLogMessage(CUPSD_LOG_DEBUG,
                  "cupsdUpdateQuota: p=%s username=%s pages=%d k=%d",
                  p->name, username, pages, k);

#if defined(__APPLE__) && defined(HAVE_DLFCN_H)
 /*
  * Use Apple PrintService quota enforcement if installed (X Server only)
  */

  if (AppleQuotas && PSQUpdateQuotaProc)
  { 
    q->page_count = (*PSQUpdateQuotaProc)(p->name, p->info, username, pages, 0);

    return (q);
  }
#endif /* __APPLE__ && HAVE_DLFCN_H */

  curtime = time(NULL);

  if (curtime < q->next_update)
  {
    q->page_count += pages;
    q->k_count    += k;

    return (q);
  }

  if (p->quota_period)
    curtime -= p->quota_period;
  else
    curtime = 0;

  q->next_update = 0;
  q->page_count  = 0;
  q->k_count     = 0;

  for (job = (cupsd_job_t *)cupsArrayFirst(Jobs);
       job;
       job = (cupsd_job_t *)cupsArrayNext(Jobs))
  {
   /*
    * We only care about the current printer/class and user...
    */

    if (strcasecmp(job->dest, p->name) != 0 ||
        strcasecmp(job->username, q->username) != 0)
      continue;

   /*
    * Make sure attributes are loaded; we always call cupsdLoadJob() to ensure
    * the access_time member is updated so the job isn't unloaded right away...
    */

    if (!cupsdLoadJob(job))
      continue;

    if ((attr = ippFindAttribute(job->attrs, "time-at-completion",
                                 IPP_TAG_INTEGER)) == NULL)
      if ((attr = ippFindAttribute(job->attrs, "time-at-processing",
                                   IPP_TAG_INTEGER)) == NULL)
        attr = ippFindAttribute(job->attrs, "time-at-creation",
                                IPP_TAG_INTEGER);

    if (attr->values[0].integer < curtime)
    {
     /*
      * This job is too old to count towards the quota, ignore it...
      */

      if (JobAutoPurge && !job->printer && job->state_value > IPP_JOB_STOPPED)
        cupsdDeleteJob(job, CUPSD_JOB_PURGE);

      continue;
    }

    if (q->next_update == 0)
      q->next_update = attr->values[0].integer + p->quota_period;

    if ((attr = ippFindAttribute(job->attrs, "job-media-sheets-completed",
                                 IPP_TAG_INTEGER)) != NULL)
      q->page_count += attr->values[0].integer;

    if ((attr = ippFindAttribute(job->attrs, "job-k-octets",
                                 IPP_TAG_INTEGER)) != NULL)
      q->k_count += attr->values[0].integer;
  }

  return (q);
}


/*
 * 'add_quota()' - Add a quota record for this printer and user.
 */

static cupsd_quota_t *			/* O - Quota data */
add_quota(cupsd_printer_t *p,		/* I - Printer */
          const char      *username)	/* I - User */
{
  cupsd_quota_t	*q;			/* New quota data */
  char		*ptr;			/* Pointer into username */


  if (!p || !username)
    return (NULL);

  if (!p->quotas)
    p->quotas = cupsArrayNew((cups_array_func_t)compare_quotas, NULL);

  if (!p->quotas)
    return (NULL);

  if ((q = calloc(1, sizeof(cupsd_quota_t))) == NULL)
    return (NULL);

  strlcpy(q->username, username, sizeof(q->username));
  if ((ptr = strchr(q->username, '@')) != NULL)
    *ptr = '\0';			/* Strip @domain/@KDC */

  cupsArrayAdd(p->quotas, q);

  return (q);
}


/*
 * 'compare_quotas()' - Compare two quota records...
 */

static int				/* O - Result of comparison */
compare_quotas(const cupsd_quota_t *q1,	/* I - First quota record */
               const cupsd_quota_t *q2)	/* I - Second quota record */
{
  return (strcasecmp(q1->username, q2->username));
}


/*
 * End of "$Id: quotas.c 6947 2007-09-12 21:09:49Z mike $".
 */
