/*
 * "$Id$"
 *
 *   Timeout functions for the CUPS Scheduler.
 *
 *   Copyright 2012 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   Copyright (C) 2010, 2011 Red Hat, Inc.
 *   Authors:
 *     Tim Waugh <twaugh@redhat.com>
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *   COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *   STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 *   OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Contents:
 *
 *   cupsdAddTimeout()	  - Add a timed callback.
 *   cupsdNextTimeout()   - Find the next enabled timed callback.
 *   cupsdRemoveTimeout() - Discard a timed callback.
 *   cupsdRunTimeout()	  - Run a timed callback.
 *   cupsdUpdateTimeout() - Adjust the time of a timed callback or disable it.
 *   compare_addrs()	  - Compare pointers for array sorting.
 *   compare_timeouts()   - Compare timed callbacks for array sorting.
 */

#include "cupsd.h"
#ifdef HAVE_AVAHI /* Applies to entire file... */

/*
 * Include necessary headers...
 */

#  include <avahi-common/timeval.h>


/*
 * Local types...
 */

struct _cupsd_timeout_s			/* Timeout data */
{
  struct timeval	when;		/* When to fire timeout */
  int			enabled;	/* Is the timeout enabled? */
  cupsd_timeoutfunc_t	callback;	/* Timeout callback */
  void			*data;		/* User data for callback */
};


/*
 * Local functions...
 */

static int	compare_addrs(void *p0, void *p1);
static int	compare_timeouts(cupsd_timeout_t *p0, cupsd_timeout_t *p1);


/*
 * 'cupsdAddTimeout()' - Add a timed callback.
 */

cupsd_timeout_t *			/* O - Timeout handle */
cupsdAddTimeout(
    const struct timeval *tv,		/* I - Absolute time */
    cupsd_timeoutfunc_t  cb,		/* I - Callback function */
    void                 *data)		/* I - User data */
{
  cupsd_timeout_t *timeout;		/* I - New timeout */


  if ((timeout = calloc(1, sizeof(cupsd_timeout_t))) != NULL)
  {
    timeout->enabled = (tv != NULL);
    if (tv)
      timeout->when = *tv;

    timeout->callback = cb;
    timeout->data     = data;

    if (!Timeouts)
      Timeouts = cupsArrayNew((cups_array_func_t)compare_timeouts, NULL);

    cupsArrayAdd(Timeouts, timeout);
  }

  return (timeout);
}


/*
 * 'cupsdNextTimeout()' - Find the next enabled timed callback.
 */

cupsd_timeout_t *			/* O - Next enabled timeout or NULL */
cupsdNextTimeout(long *delay)		/* O - Seconds before scheduled */
{
  cupsd_timeout_t *first = cupsArrayFirst(Timeouts);
					/* First timeout */
  struct timeval curtime;		/* Current time */


  if (first && !first->enabled)
    first = NULL;

  if (first && delay)
  {
    gettimeofday(&curtime, NULL);
    if (avahi_timeval_compare(&curtime, &first->when) > 0)
      *delay = 0;
    else
    {
      *delay = 1 + first->when.tv_sec - curtime.tv_sec;
      if (first->when.tv_usec < curtime.tv_usec)
	(*delay) --;
    }
  }

  return (first);
}


/*
 * 'cupsdRemoveTimeout()' - Discard a timed callback.
 */

void
cupsdRemoveTimeout(
    cupsd_timeout_t *timeout)		/* I - Timeout */
{
  cupsArrayRemove(Timeouts, timeout);
  free(timeout);
}


/*
 * 'cupsdRunTimeout()' - Run a timed callback.
 */

void
cupsdRunTimeout(
    cupsd_timeout_t *timeout)		/* I - Timeout */
{
  if (timeout)
  {
    timeout->enabled = 0;
    if (timeout->callback)
      (*timeout->callback)(timeout, timeout->data);
  }
}


/*
 * 'cupsdUpdateTimeout()' - Adjust the time of a timed callback or disable it.
 */

void
cupsdUpdateTimeout(
    cupsd_timeout_t      *timeout,	/* I - Timeout */
    const struct timeval *tv)		/* I - Absolute time or NULL */
{
  cupsArrayRemove(Timeouts, timeout);
  timeout->enabled = (tv != NULL);

  if (tv)
    timeout->when = *tv;

  cupsArrayAdd(Timeouts, timeout);
}


/*
 * 'compare_addrs()' - Compare pointers for array sorting.
 */

static int				/* O - Result of comparison */
compare_addrs(void *p0,			/* I - First pointer */
              void *p1)			/* I - Second pointer */
{
  if (p0 == p1)
    return (0);
  else if (p0 < p1)
    return (-1);
  else
    return (1);
}


/*
 * 'compare_timeouts()' - Compare timed callbacks for array sorting.
 */

static int				/* O - Result of comparison */
compare_timeouts(cupsd_timeout_t *p0,	/* I - First timeout */
                 cupsd_timeout_t *p1)	/* I - Second timeout */
{
  int addrsdiff = compare_addrs (p0, p1);
					/* Address difference */
  int tvdiff;				/* Time difference */


  if (addrsdiff == 0)
    return (0);

  if (!p0->enabled || !p1->enabled)
  {
    if (!p0->enabled && !p1->enabled)
      return (addrsdiff);

    return (p0->enabled ? -1 : 1);
  }

  tvdiff = avahi_timeval_compare(&p0->when, &p1->when);
  if (tvdiff != 0)
    return (tvdiff);

  return (addrsdiff);
}
#endif /* HAVE_AVAHI */


/*
 * End of "$Id$".
 */
