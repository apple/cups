/*
 * "$Id: filter.c,v 1.3.2.3 2002/07/02 19:15:25 mike Exp $"
 *
 *   File type conversion routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products, all rights reserved.
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
 *   mimeAddFilter() - Add a filter to the current MIME database.
 *   mimeFilter()    - Find the fastest way to convert from one type to another.
 *   compare()       - Compare two filter types...
 *   lookup()        - Lookup a filter...
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <cups/debug.h>
#include <cups/string.h>
#include "mime.h"


/*
 * Local functions...
 */

static int		compare(mime_filter_t *, mime_filter_t *);
static mime_filter_t	*lookup(mime_t *, mime_type_t *, mime_type_t *);


/*
 * 'mimeAddFilter()' - Add a filter to the current MIME database.
 */

mime_filter_t *				/* O - New filter */
mimeAddFilter(mime_t      *mime,	/* I - MIME database */
              mime_type_t *src,		/* I - Source type */
	      mime_type_t *dst,		/* I - Destination type */
              int         cost,		/* I - Relative time/resource cost */
	      const char  *filter)	/* I - Filter program to run */
{
  mime_filter_t	*temp;			/* New filter */


 /*
  * Range-check the input...
  */

  if (mime == NULL || src == NULL || dst == NULL || filter == NULL)
    return (NULL);

  if (strlen(filter) > (MIME_MAX_FILTER - 1))
    return (NULL);

 /*
  * See if we already have an existing filter for the given source and
  * destination...
  */

  if ((temp = lookup(mime, src, dst)) != NULL)
  {
   /*
    * Yup, does the existing filter have a higher cost?  If so, copy the
    * filter and cost to the existing filter entry and return it...
    */

    if (temp->cost > cost)
    {
      temp->cost = cost;
      strlcpy(temp->filter, filter, sizeof(temp->filter));
    }
  }
  else
  {
   /*
    * Nope, add a new one...
    */

    if (mime->num_filters == 0)
      temp = malloc(sizeof(mime_filter_t));
    else
      temp = realloc(mime->filters, sizeof(mime_filter_t) * (mime->num_filters + 1));

    if (temp == NULL)
      return (NULL);

    mime->filters = temp;
    temp += mime->num_filters;
    mime->num_filters ++;

   /*
    * Copy the information over and sort if necessary...
    */

    temp->src  = src;
    temp->dst  = dst;
    temp->cost = cost;
    strlcpy(temp->filter, filter, sizeof(temp->filter));

    if (mime->num_filters > 1)
      qsort(mime->filters, mime->num_filters, sizeof(mime_filter_t),
            (int (*)(const void *, const void *))compare);
  }

 /*
  * Return the new/updated filter...
  */

  return (temp);
}


/*
 * 'mimeFilter()' - Find the fastest way to convert from one type to another.
 */

mime_filter_t *				/* O - Array of filters to run */
mimeFilter(mime_t      *mime,		/* I - MIME database */
           mime_type_t *src,		/* I - Source file type */
	   mime_type_t *dst,		/* I - Destination file type */
           int         *num_filters)	/* O - Number of filters to run */
{
  int		i, j,			/* Looping vars */
		num_temp,		/* Number of temporary filters */
		num_mintemp,		/* Number of filters in the minimum */
		cost,			/* Current cost */
		mincost;		/* Current minimum */
  mime_filter_t	*temp,			/* Temporary filter */
		*mintemp,		/* Current minimum */
		*mincurrent,		/* Current filter for minimum */
		*current,		/* Current filter */
		*filters;		/* Filters to use */


 /*
  * Range-check the input...
  */

  DEBUG_printf(("mimeFilter(mime=%p, src=%p(%s/%s), dst=%p(%s/%s), num_filters=%p(%d))\n",
        	mime, src, src ? src->super : "?", src ? src->type : "?",
		dst, dst ? dst->super : "?", dst ? dst->type : "?",
		num_filters, num_filters ? *num_filters : 0));

  if (mime == NULL || src == NULL || dst == NULL || num_filters == NULL)
    return (NULL);

  *num_filters = 0;

 /*
  * See if there is a filter that can convert the files directly...
  */

  if ((temp = lookup(mime, src, dst)) != NULL)
  {
   /*
    * Got a direct filter!
    */

    if ((filters = (mime_filter_t *)malloc(sizeof(mime_filter_t))) == NULL)
      return (NULL);

    memcpy(filters, temp, sizeof(mime_filter_t));
    *num_filters = 1;

    DEBUG_puts("    Returning 1 filter:");
    DEBUG_printf(("    %s\n", filters->filter));

    return (filters);
  }

 /*
  * OK, now look for filters from the source type to any other type...
  */

  mincost     = 9999999;
  mintemp     = NULL;
  num_mintemp = 0;
  mincurrent  = NULL;

  for (i = mime->num_filters, current = mime->filters; i > 0; i --, current ++)
    if (current->src == src)
    {
     /*
      * See if we have any filters that can convert from the destination type
      * of this filter to the final type...
      */

      if ((temp = mimeFilter(mime, current->dst, dst, &num_temp)) == NULL)
        continue;

     /*
      * Found a match; see if this one is less costly than the last (if
      * any...)
      */

      for (j = 0, cost = 0; j < num_temp; j ++)
        cost += temp->cost;

      if (cost < mincost)
      {
        if (mintemp != NULL)
	  free(mintemp);

	mincost     = cost;
	mintemp     = temp;
	num_mintemp = num_temp;
	mincurrent  = current;
      }
      else
        free(temp);
    }

  if (mintemp != NULL)
  {
   /*
    * Hey, we got a match!  Add the current filter to the beginning of the
    * filter list...
    */

    filters = (mime_filter_t *)realloc(mintemp, sizeof(mime_filter_t) *
                                                (num_mintemp + 1));

    if (filters == NULL)
    {
      *num_filters = 0;
      return (NULL);
    }

    memmove(filters + 1, filters, num_mintemp * sizeof(mime_filter_t));
    memcpy(filters, mincurrent, sizeof(mime_filter_t));

    *num_filters = num_mintemp + 1;

#ifdef DEBUG
    printf("    Returning %d filters:\n", *num_filters);
    for (i = 0; i <= num_mintemp; i ++)
      printf("    %s\n", filters[i].filter);
#endif /* DEBUG */

    return (filters);
  }

  DEBUG_puts("    Returning zippo...");

  return (NULL);
}


/*
 * 'compare()' - Compare two filter types...
 */

static int			/* O - Comparison result */
compare(mime_filter_t *f0,	/* I - First filter */
        mime_filter_t *f1)	/* I - Second filter */
{
  int	i;			/* Result of comparison */


  if ((i = strcmp(f0->src->super, f1->src->super)) == 0)
    if ((i = strcmp(f0->src->type, f1->src->type)) == 0)
      if ((i = strcmp(f0->dst->super, f1->dst->super)) == 0)
        i = strcmp(f0->dst->type, f1->dst->type);

  return (i);
}


/*
 * 'lookup()' - Lookup a filter...
 */

static mime_filter_t *		/* O - Filter for src->dst */
lookup(mime_t      *mime,	/* I - MIME database */
       mime_type_t *src,	/* I - Source type */
       mime_type_t *dst)	/* I - Destination type */
{
  mime_filter_t	key;		/* Key record for filter search */


  if (mime->num_filters == 0)
    return (NULL);

  key.src = src;
  key.dst = dst;

  return ((mime_filter_t *)bsearch(&key, mime->filters, mime->num_filters,
                                   sizeof(mime_filter_t),
				   (int (*)(const void *, const void *))compare));
}


/*
 * End of "$Id: filter.c,v 1.3.2.3 2002/07/02 19:15:25 mike Exp $".
 */
