/*
 * "$Id$"
 *
 *   File type conversion routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   mimeAddFilter()    - Add a filter to the current MIME database.
 *   mimeFilter()       - Find the fastest way to convert from one type to
 *                        another.
 *   mimeFilterLookup() - Lookup a filter...
 *   compare_filters()  - Compare two filters...
 *   find_filters()     - Find the filters to convert from one type to another.
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
 * Local types...
 */

typedef struct _mime_typelist_s		/**** List of source types ****/
{
  struct _mime_typelist_s *next;	/* Next source type */
  mime_type_t		*src;		/* Source type */
} _mime_typelist_t;


/*
 * Local functions...
 */

static int		compare_filters(mime_filter_t *, mime_filter_t *);
static int		compare_srcs(mime_filter_t *, mime_filter_t *);
static cups_array_t	*find_filters(mime_t *mime, mime_type_t *src,
			              mime_type_t *dst, int *cost,
				      _mime_typelist_t *visited);


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

  if (!mime || !src || !dst || !filter)
    return (NULL);

 /*
  * See if we already have an existing filter for the given source and
  * destination...
  */

  if ((temp = mimeFilterLookup(mime, src, dst)) != NULL)
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

    if (!mime->filters)
      mime->filters = cupsArrayNew((cups_array_func_t)compare_filters, NULL);

    if (!mime->filters)
      return (NULL);

    if ((temp = calloc(1, sizeof(mime_filter_t))) == NULL)
      return (NULL);

   /*
    * Copy the information over and sort if necessary...
    */

    temp->src  = src;
    temp->dst  = dst;
    temp->cost = cost;
    strlcpy(temp->filter, filter, sizeof(temp->filter));

    cupsArrayAdd(mime->filters, temp);
  }

 /*
  * Return the new/updated filter...
  */

  return (temp);
}


/*
 * 'mimeFilter()' - Find the fastest way to convert from one type to another.
 */

cups_array_t *				/* O - Array of filters to run */
mimeFilter(mime_t      *mime,		/* I - MIME database */
           mime_type_t *src,		/* I - Source file type */
	   mime_type_t *dst,		/* I - Destination file type */
	   int         *cost)		/* O - Cost of filters */
{
 /*
  * Range-check the input...
  */

  DEBUG_printf(("mimeFilter(mime=%p, src=%p(%s/%s), dst=%p(%s/%s), "
                "cost=%p(%d))\n",
        	mime, src, src ? src->super : "?", src ? src->type : "?",
		dst, dst ? dst->super : "?", dst ? dst->type : "?",
		cost, cost ? *cost : 0));


  if (cost)
    *cost = 0;

  if (!mime || !src || !dst)
    return (NULL);

 /*
  * (Re)build the source lookup array as needed...
  */

  if (!mime->srcs)
  {
    mime_filter_t	*current;	/* Current filter */


    mime->srcs = cupsArrayNew((cups_array_func_t)compare_srcs, NULL);

    for (current = mimeFirstFilter(mime);
         current;
	 current = mimeNextFilter(mime))
      cupsArrayAdd(mime->srcs, current);
  }

 /*
  * Find the filters...
  */

  return (find_filters(mime, src, dst, cost, NULL));
}


/*
 * 'mimeFilterLookup()' - Lookup a filter...
 */

mime_filter_t *				/* O - Filter for src->dst */
mimeFilterLookup(mime_t      *mime,	/* I - MIME database */
                 mime_type_t *src,	/* I - Source type */
                 mime_type_t *dst)	/* I - Destination type */
{
  mime_filter_t	key;			/* Key record for filter search */


  key.src = src;
  key.dst = dst;

  return ((mime_filter_t *)cupsArrayFind(mime->filters, &key));
}


/*
 * 'compare_filters()' - Compare two filters...
 */

static int				/* O - Comparison result */
compare_filters(mime_filter_t *f0,	/* I - First filter */
                mime_filter_t *f1)	/* I - Second filter */
{
  int	i;				/* Result of comparison */


  if ((i = strcmp(f0->src->super, f1->src->super)) == 0)
    if ((i = strcmp(f0->src->type, f1->src->type)) == 0)
      if ((i = strcmp(f0->dst->super, f1->dst->super)) == 0)
        i = strcmp(f0->dst->type, f1->dst->type);

  return (i);
}


/*
 * 'compare_srcs()' - Compare two srcs...
 */

static int				/* O - Comparison result */
compare_srcs(mime_filter_t *f0,		/* I - First filter */
             mime_filter_t *f1)		/* I - Second filter */
{
  int	i;				/* Result of comparison */


  if ((i = strcmp(f0->src->super, f1->src->super)) == 0)
    i = strcmp(f0->src->type, f1->src->type);

  return (i);
}


/*
 * 'find_filters()' - Find the filters to convert from one type to another.
 */

static cups_array_t *			/* O - Array of filters to run */
find_filters(mime_t           *mime,	/* I - MIME database */
             mime_type_t      *src,	/* I - Source file type */
	     mime_type_t      *dst,	/* I - Destination file type */
	     int              *cost,	/* O - Cost of filters */
	     _mime_typelist_t *list)	/* I - Source types we've used */
{
  int			tempcost,	/* Temporary cost */
			mincost;	/* Current minimum */
  cups_array_t		*temp,		/* Temporary filter */
			*mintemp;	/* Current minimum */
  mime_filter_t		*current,	/* Current filter */
			srckey;		/* Source type key */
  _mime_typelist_t	listnode,	/* New list node */
			*listptr;	/* Pointer in list */


  DEBUG_printf(("find_filters(mime=%p, src=%p(%s/%s), dst=%p(%s/%s), "
                "cost=%p, list=%p)\n", mime, src, src->super, src->type,
		dst, dst->super, dst->type, cost, list));

 /*
  * See if there is a filter that can convert the files directly...
  */

  if ((current = mimeFilterLookup(mime, src, dst)) != NULL)
  {
   /*
    * Got a direct filter!
    */

    DEBUG_puts("find_filters: Direct filter found!");

    if ((mintemp = cupsArrayNew(NULL, NULL)) == NULL)
      return (NULL);

    cupsArrayAdd(mintemp, current);

    mincost = current->cost;

    if (!cost)
      return (mintemp);

    DEBUG_puts("find_filters: Found direct filter:");
    DEBUG_printf(("find_filters: %s (cost=%d)\n", current->filter, mincost));
  }
  else
  {
   /*
    * No direct filter...
    */

    mintemp = NULL;
    mincost = 9999999;
  }

 /*
  * Initialize this node in the type list...
  */

  listnode.next = list;

 /*
  * OK, now look for filters from the source type to any other type...
  */

  srckey.src = src;

  for (current = (mime_filter_t *)cupsArrayFind(mime->srcs, &srckey);
       current && current->src == src;
       current = (mime_filter_t *)cupsArrayNext(mime->srcs))
  {
   /*
    * See if we have already tried the destination type as a source
    * type (this avoids extra filter looping...)
    */

    mime_type_t *current_dst;		/* Current destination type */


    for (listptr = list, current_dst = current->dst;
	 listptr;
	 listptr = listptr->next)
      if (current_dst == listptr->src)
	break;

    if (listptr)
      continue;

   /*
    * See if we have any filters that can convert from the destination type
    * of this filter to the final type...
    */

    listnode.src = current->src;

    cupsArraySave(mime->srcs);
    temp = find_filters(mime, current->dst, dst, &tempcost, &listnode);
    cupsArrayRestore(mime->srcs);

    if (!temp)
      continue;

    if (!cost)
      return (temp);

   /*
    * Found a match; see if this one is less costly than the last (if
    * any...)
    */

    tempcost += current->cost;

    if (tempcost < mincost)
    {
      cupsArrayDelete(mintemp);

     /*
      * Hey, we got a match!  Add the current filter to the beginning of the
      * filter list...
      */

      mintemp = temp;
      mincost = tempcost;
      cupsArrayInsert(mintemp, current);
    }
    else
      cupsArrayDelete(temp);
  }

  if (mintemp)
  {
   /*
    * Hey, we got a match!
    */

#ifdef DEBUG
    DEBUG_printf(("find_filters: Returning %d filters:\n",
                  cupsArrayCount(mintemp)));

    for (current = (mime_filter_t *)cupsArrayFirst(mintemp);
         current;
	 current = (mime_filter_t *)cupsArrayNext(mintemp))
      DEBUG_printf(("find_filters: %s\n", current->filter));
#endif /* DEBUG */

    if (cost)
      *cost = mincost;

    return (mintemp);
  }

  DEBUG_puts("find_filters: Returning zippo...");

  return (NULL);
}


/*
 * End of "$Id$".
 */
