/*
 * "$Id: filter.c,v 1.5 1999/03/01 20:51:51 mike Exp $"
 *
 *   File type conversion routines for the Common UNIX Printing System (CUPS).
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
 *   mimeAddFilter() - Add a filter to the current MIME database.
 *   mimeFilter()    - Find the fastest way to convert from one type to another.
 *   compare()       - Compare two filter types...
 *   lookup()        - Lookup a filter...
 *
 * Revision History:
 *
 *   $Log: filter.c,v $
 *   Revision 1.5  1999/03/01 20:51:51  mike
 *   Code cleanup - removed extraneous semi-colons...
 *
 *   Revision 1.4  1999/02/05 17:40:51  mike
 *   Added IPP client read/write code.
 *
 *   Added string functions missing from some UNIXs.
 *
 *   Added option parsing functions.
 *
 *   Added IPP convenience functions (not implemented yet).
 *
 *   Updated source files to use local string.h as needed (for
 *   missing string functions)
 *
 *   Revision 1.3  1999/01/24 14:18:43  mike
 *   Check-in prior to CVS use.
 *
 *   Revision 1.2  1998/08/06 14:38:38  mike
 *   Finished coding and testing for CUPS 1.0.
 *
 *   Revision 1.1  1998/06/11 20:50:53  mike
 *   Initial revision
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "string.h"
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
	      char        *filter)	/* I - Filter program to run */
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
      strcpy(temp->filter, filter);
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
    strcpy(temp->filter, filter);

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
 *
 * NOTE: Currently we do not use the "cost" field provided with each filter.
 *       This will be addressed in a future version of this function.  For
 *       now all filters are assumed to be equally costly and we find the
 *       smallest number of filters to run that satisfies the filter
 *       requirements.
 */

mime_filter_t *				/* O - Array of filters to run */
mimeFilter(mime_t      *mime,		/* I - MIME database */
           mime_type_t *src,		/* I - Source file type */
	   mime_type_t *dst,		/* I - Destination file type */
           int         *num_filters)	/* O - Number of filters to run */
{
  int		i;			/* Looping var */
  mime_filter_t	*temp,			/* Temporary filter */
		*current,		/* Current filter */
		*filters;		/* Filters to use */


 /*
  * Range-check the input...
  */

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
    return (filters);
  }

 /*
  * OK, now look for filters from the source type to any other type...
  */

  for (i = mime->num_filters, current = mime->filters; i > 0; i --, current ++)
    if (current->src == src)
    {
     /*
      * See if we have any filters that can convert from the destination type
      * of this filter to the final type...
      */

      if ((filters = mimeFilter(mime, current->dst, dst, num_filters)) == NULL)
        continue;

     /*
      * Hey, we got a match!  Add the current filter to the beginning of the
      * filter list...
      */

      filters = (mime_filter_t *)realloc(filters, sizeof(mime_filter_t) *
                                                  (*num_filters + 1));

      if (filters == NULL)
      {
        *num_filters = 0;
        continue;
      }

      memmove(filters + 1, filters, *num_filters * sizeof(mime_filter_t));
      memcpy(filters, current, sizeof(mime_filter_t));

      (*num_filters) ++;

      return (filters);
    }

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
 * End of "$Id: filter.c,v 1.5 1999/03/01 20:51:51 mike Exp $".
 */
