/*
 * "$Id: quotas.c,v 1.4.2.2 2002/01/02 18:05:05 mike Exp $"
 *
 *   Quota routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
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
 *   AddQuota()    - Add a quota record for this printer and user.
 *   FindQuota()   - Find a quota record.
 *   FreeQuotas()  - Free quotas for a printer.
 *   UpdateQuota() - Update quota data for the specified printer and user.
 *   compare()     - Compare two quota records...
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * Local functions...
 */

static int	compare(const quota_t *q1, const quota_t *q2);


/*
 * 'AddQuota()' - Add a quota record for this printer and user.
 */

quota_t *				/* O - Quota data */
AddQuota(printer_t  *p,			/* I - Printer */
         const char *username)		/* I - User */
{
  quota_t	*q;			/* New quota data */


  if (!p || !username)
    return (NULL);

  if (p->num_quotas == 0)
    q = malloc(sizeof(quota_t));
  else
    q = realloc(p->quotas, sizeof(quota_t) * (p->num_quotas + 1));

  if (!q)
    return (NULL);

  p->quotas = q;
  q         += p->num_quotas;
  p->num_quotas ++;

  memset(q, 0, sizeof(quota_t));
  strncpy(q->username, username, sizeof(q->username) - 1);

  if (p->num_quotas > 1)
    qsort(p->quotas, p->num_quotas, sizeof(quota_t),
          (int (*)(const void *, const void *))compare);

  return (FindQuota(p, username));
}


/*
 * 'FindQuota()' - Find a quota record.
 */

quota_t *				/* O - Quota data */
FindQuota(printer_t  *p,		/* I - Printer */
          const char *username)		/* I - User */
{
  quota_t	*q,			/* Quota data pointer */
		match;			/* Search data */


  if (!p || !username)
    return (NULL);

  if (p->num_quotas == 0)
    q = NULL;
  else
  {
    strncpy(match.username, username, sizeof(match.username) - 1);
    match.username[sizeof(match.username) - 1] = '\0';

    q = bsearch(&match, p->quotas, p->num_quotas, sizeof(quota_t),
                (int(*)(const void *, const void *))compare);
  }

  if (q)
    return (q);
  else
    return (AddQuota(p, username));
}


/*
 * 'FreeQuotas()' - Free quotas for a printer.
 */

void
FreeQuotas(printer_t *p)		/* I - Printer */
{
  if (!p)
    return;

  if (p->num_quotas)
    free(p->quotas);

  p->num_quotas = 0;
  p->quotas     = NULL;
}


/*
 * 'UpdateQuota()' - Update quota data for the specified printer and user.
 */

quota_t *				/* O - Quota data */
UpdateQuota(printer_t  *p,		/* I - Printer */
            const char *username,	/* I - User */
	    int        pages,		/* I - Number of pages */
	    int        k)		/* I - Number of kilobytes */
{
  quota_t	*q;			/* Quota data */
  job_t		*job,			/* Current job */
		*next;			/* Next job */
  time_t	curtime;		/* Current time */
  ipp_attribute_t *attr;		/* Job attribute */


  if (!p || !username)
    return (NULL);

  if (!p->k_limit && !p->page_limit)
    return (NULL);

  if ((q = FindQuota(p, username)) == NULL)
    return (NULL);

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

  for (job = Jobs; job; job = next)
  {
    next = job->next;

    if (strcasecmp(job->dest, p->name) != 0 ||
        strcasecmp(job->username, q->username) != 0)
      continue;

    if ((attr = ippFindAttribute(job->attrs, "time-at-completion",
                                 IPP_TAG_INTEGER)) == NULL)
      if ((attr = ippFindAttribute(job->attrs, "time-at-processing",
                                   IPP_TAG_INTEGER)) == NULL)
        attr = ippFindAttribute(job->attrs, "time-at-creation",
                                IPP_TAG_INTEGER);

    if (attr == NULL)
      break;

    if (attr->values[0].integer < curtime)
    {
      if (JobAutoPurge)
        CancelJob(job->id, 1);

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
 * 'compare()' - Compare two quota records...
 */

static int				/* O - Result of comparison */
compare(const quota_t *q1,		/* I - First quota record */
        const quota_t *q2)		/* I - Second quota record */
{
  return (strcasecmp(q1->username, q2->username));
}


/*
 * End of "$Id: quotas.c,v 1.4.2.2 2002/01/02 18:05:05 mike Exp $".
 */
