/*
 * "$Id$"
 *
 *   Sorted array routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsArrayAdd()     - Add an element to the array.
 *   cupsArrayClear()   - Clear the array.
 *   cupsArrayCount()   - Get the number of elements in the array.
 *   cupsArrayCurrent() - Return the current element in the array.
 *   cupsArrayDelete()  - Free all memory used by the array.
 *   cupsArrayDup()     - Duplicate the array.
 *   cupsArrayFind()    - Find an element in the array.
 *   cupsArrayFirst()   - Get the first element in the array.
 *   cupsArrayIndex()   - Get the N-th element in the array.
 *   cupsArrayInsert()  - Insert an element in the array.
 *   cupsArrayLast()    - Get the last element in the array.
 *   cupsArrayNew()     - Create a new array.
 *   cupsArrayNext()    - Get the next element in the array.
 *   cupsArrayPrev()    - Get the previous element in the array.
 *   cupsArrayRemove()  - Remove an element from the array.
 *   cupsArrayRestore() - Reset the current element to the last cupsArraySave.
 *   cupsArraySave()    - Mark the current element for a later cupsArrayRestore.
 *   cups_array_find()  - Find an element in the array...
 */

/*
 * Include necessary headers...
 */

#include "array.h"
#include "string.h"
#include "debug.h"


/*
 * Limits...
 */

#define _CUPS_MAXSAVE	32		/**** Maximum number of saves ****/


/*
 * Types and structures...
 */

struct _cups_array_s			/**** CUPS array structure ****/
{
 /*
  * The current implementation uses an insertion sort into an array of
  * sorted pointers.  We leave the array type private/opaque so that we
  * can change the underlying implementation without affecting the users
  * of this API.
  */

  int			num_elements,	/* Number of array elements */
			alloc_elements,	/* Allocated array elements */
			current,	/* Current element */
			insert,		/* Last inserted element */
			unique,		/* Are all elements unique? */
			num_saved,	/* Number of saved elements */
			saved[_CUPS_MAXSAVE];
					/* Saved elements */
  void			**elements;	/* Array elements */
  cups_array_func_t	compare;	/* Element comparison function */
  void			*data;		/* User data passed to compare */
};


/*
 * Local functions...
 */

static int	cups_array_add(cups_array_t *a, void *e, int insert);
static int	cups_array_find(cups_array_t *a, void *e, int prev, int *rdiff);


/*
 * 'cupsArrayAdd()' - Add an element to the array.
 *
 * When adding an element to a sorted array, non-unique elements are
 * appended at the end of the run.  For unsorted arrays, the element
 * is inserted at the end of the array.
 */

int					/* O - 1 on success, 0 on failure */
cupsArrayAdd(cups_array_t *a,		/* I - Array */
             void         *e)		/* I - Element */
{
  DEBUG_printf(("cupsArrayAdd(a=%p, e=%p)\n", a, e));

 /*
  * Range check input...
  */

  if (!a || !e)
  {
    DEBUG_puts("cupsArrayAdd: returning 0");
    return (0);
  }

 /*
  * Append the element...
  */

  return (cups_array_add(a, e, 0));
}


/*
 * 'cupsArrayClear()' - Clear the array.
 */

void
cupsArrayClear(cups_array_t *a)		/* I - Array */
{
 /*
  * Range check input...
  */

  if (!a)
    return;

 /*
  * Set the number of elements to 0; we don't actually free the memory
  * here - that is done in cupsArrayDelete()...
  */

  a->num_elements = 0;
  a->current      = -1;
  a->insert       = -1;
  a->unique       = 1;
  a->num_saved    = 0;
}


/*
 * 'cupsArrayCount()' - Get the number of elements in the array.
 */

int					/* O - Number of elements */
cupsArrayCount(cups_array_t *a)		/* I - Array */
{
 /*
  * Range check input...
  */

  if (!a)
    return (0);

 /*
  * Return the number of elements...
  */

  return (a->num_elements);
}


/*
 * 'cupsArrayCurrent()' - Return the current element in the array.
 */

void *					/* O - Element */
cupsArrayCurrent(cups_array_t *a)	/* I - Array */
{
 /*
  * Range check input...
  */

  if (!a)
    return (NULL);

 /*
  * Return the current element...
  */

  if (a->current >= 0 && a->current < a->num_elements)
    return (a->elements[a->current]);
  else
    return (NULL);
}


/*
 * 'cupsArrayDelete()' - Free all memory used by the array.
 */

void
cupsArrayDelete(cups_array_t *a)	/* I - Array */
{
 /*
  * Range check input...
  */

  if (!a)
    return;

 /*
  * Free the array of element pointers - the caller is responsible
  * for freeing the elements themselves...
  */

  if (a->alloc_elements)
    free(a->elements);

  free(a);
}


/*
 * 'cupsArrayDup()' - Duplicate the array.
 */

cups_array_t *				/* O - Duplicate array */
cupsArrayDup(cups_array_t *a)		/* I - Array */
{
  cups_array_t	*da;			/* Duplicate array */


 /*
  * Range check input...
  */

  if (!a)
    return (NULL);

 /*
  * Allocate memory for the array...
  */

  da = calloc(1, sizeof(cups_array_t));
  if (!da)
    return (NULL);

  da->compare   = a->compare;
  da->data      = a->data;
  da->current   = a->current;
  da->insert    = a->insert;
  da->unique    = a->unique;
  da->num_saved = a->num_saved;

  memcpy(da->saved, a->saved, sizeof(a->saved));

  if (a->num_elements)
  {
   /*
    * Allocate memory for the elements...
    */

    da->elements = malloc(a->num_elements * sizeof(void *));
    if (!da->elements)
    {
      free(da);
      return (NULL);
    }

   /*
    * Copy the element pointers...
    */

    memcpy(da->elements, a->elements, a->num_elements * sizeof(void *));
    da->num_elements   = a->num_elements;
    da->alloc_elements = a->num_elements;
  }

 /*
  * Return the new array...
  */

  return (da);
}


/*
 * 'cupsArrayFind()' - Find an element in the array.
 */

void *					/* O - Element found or NULL */
cupsArrayFind(cups_array_t *a,		/* I - Array */
              void         *e)		/* I - Element */
{
  int	current,			/* Current element */
	diff;				/* Difference */


 /*
  * Range check input...
  */

  if (!a || !e)
    return (NULL);

 /*
  * See if we have any elements...
  */

  if (!a->num_elements)
    return (NULL);

 /*
  * Yes, look for a match...
  */

  current = cups_array_find(a, e, a->current, &diff);
  if (!diff)
  {
   /*
    * Found a match!  If the array does not contain unique values, find
    * the first element that is the same...
    */

    if (!a->unique && a->compare)
    {
     /*
      * The array is not unique, find the first match...
      */

      while (current > 0 && !(*(a->compare))(e, a->elements[current - 1],
                                             a->data))
        current --;
    }

    a->current = current;

    return (a->elements[current]);
  }
  else
  {
   /*
    * No match...
    */

    a->current = -1;

    return (NULL);
  }
}


/*
 * 'cupsArrayFirst()' - Get the first element in the array.
 */

void *					/* O - First element or NULL */
cupsArrayFirst(cups_array_t *a)		/* I - Array */
{
 /*
  * Range check input...
  */

  if (!a)
    return (NULL);

 /*
  * Return the first element...
  */

  a->current = 0;

  return (cupsArrayCurrent(a));
}


/*
 * 'cupsArrayIndex()' - Get the N-th element in the array.
 */

void *					/* O - N-th element or NULL */
cupsArrayIndex(cups_array_t *a,		/* I - Array */
               int          n)		/* I - Index into array, starting at 0 */
{
  if (!a)
    return (NULL);

  a->current = n;

  return (cupsArrayCurrent(a));
}


/*
 * 'cupsArrayInsert()' - Insert an element in the array.
 *
 * When inserting an element in a sorted array, non-unique elements are
 * inserted at the beginning of the run.  For unsorted arrays, the element
 * is inserted at the beginning of the array.
 */

int					/* O - 0 on failure, 1 on success */
cupsArrayInsert(cups_array_t *a,	/* I - Array */
		void         *e)	/* I - Element */
{
  DEBUG_printf(("cupsArrayInsert(a=%p, e=%p)\n", a, e));

 /*
  * Range check input...
  */

  if (!a || !e)
  {
    DEBUG_puts("cupsArrayInsert: returning 0");
    return (0);
  }

 /*
  * Insert the element...
  */

  return (cups_array_add(a, e, 1));
}


/*
 * 'cupsArrayLast()' - Get the last element in the array.
 */

void *					/* O - Last element or NULL */
cupsArrayLast(cups_array_t *a)		/* I - Array */
{
 /*
  * Range check input...
  */

  if (!a)
    return (NULL);

 /*
  * Return the last element...
  */

  a->current = a->num_elements - 1;

  return (cupsArrayCurrent(a));
}


/*
 * 'cupsArrayNew()' - Create a new array.
 */

cups_array_t *				/* O - Array */
cupsArrayNew(cups_array_func_t f,	/* I - Comparison function */
             void              *d)	/* I - User data */
{
  cups_array_t	*a;			/* Array  */


 /*
  * Allocate memory for the array...
  */

  a = calloc(1, sizeof(cups_array_t));
  if (!a)
    return (NULL);

  a->compare   = f;
  a->data      = d;
  a->current   = -1;
  a->insert    = -1;
  a->num_saved = 0;
  a->unique    = 1;

  return (a);
}


/*
 * 'cupsArrayNext()' - Get the next element in the array.
 */

void *					/* O - Next element or NULL */
cupsArrayNext(cups_array_t *a)		/* I - Array */
{
 /*
  * Range check input...
  */

  if (!a)
    return (NULL);

 /*
  * Return the next element...
  */

  if (a->current < a->num_elements)
    a->current ++;

  return (cupsArrayCurrent(a));
}


/*
 * 'cupsArrayPrev()' - Get the previous element in the array.
 */

void *					/* O - Previous element or NULL */
cupsArrayPrev(cups_array_t *a)		/* I - Array */
{
 /*
  * Range check input...
  */

  if (!a)
    return (NULL);

 /*
  * Return the previous element...
  */

  if (a->current >= 0)
    a->current --;

  return (cupsArrayCurrent(a));
}


/*
 * 'cupsArrayRemove()' - Remove an element from the array.
 */

int					/* O - 1 on success, 0 on failure */
cupsArrayRemove(cups_array_t *a,	/* I - Array */
                void         *e)	/* I - Element */
{
  int	i,				/* Looping var */
	current,			/* Current element */
	diff;				/* Difference */


 /*
  * Range check input...
  */

  if (!a || !e)
    return (0);

 /*
  * See if the element is in the array...
  */

  if (!a->num_elements)
    return (0);

  current = cups_array_find(a, e, a->current, &diff);
  if (diff)
    return (0);

 /*
  * Yes, now remove it...
  */

  a->num_elements --;

  if (current < a->num_elements)
    memmove(a->elements + current, a->elements + current + 1,
            (a->num_elements - current) * sizeof(void *));

  if (current <= a->current)
    a->current --;

  if (current <= a->insert)
    a->insert --;

  for (i = 0; i < a->num_saved; i ++)
    if (current <= a->saved[i])
      a->saved[i] --;

  if (a->num_elements <= 1)
    a->unique = 1;

  return (1);
}


/*
 * 'cupsArrayRestore()' - Reset the current element to the last cupsArraySave.
 */

void *					/* O - New current element */
cupsArrayRestore(cups_array_t *a)	/* I - Array */
{
  if (!a)
    return (NULL);

  if (a->num_saved <= 0)
    return (NULL);

  a->num_saved --;
  a->current = a->saved[a->num_saved];

  if (a->current >= 0 && a->current < a->num_elements)
    return (a->elements[a->current]);
  else
    return (NULL);
}


/*
 * 'cupsArraySave()' - Mark the current element for a later cupsArrayRestore.
 *
 * The save/restore stack is guaranteed to be at least 32 elements deep.
 */

int					/* O - 1 on success, 0 on failure */
cupsArraySave(cups_array_t *a)		/* I - Array */
{
  if (!a)
    return (0);

  if (a->num_saved >= _CUPS_MAXSAVE)
    return (0);

  a->saved[a->num_saved] = a->current;
  a->num_saved ++;

  return (1);
}


/*
 * 'cups_array_add()' - Insert or append an element to the array...
 */

static int				/* O - 1 on success, 0 on failure */
cups_array_add(cups_array_t *a,		/* I - Array */
               void         *e,		/* I - Element to add */
	       int          insert)	/* I - 1 = insert, 0 = append */
{
  int	i,				/* Looping var */
	current,			/* Current element */
	diff;				/* Comparison with current element */


  DEBUG_printf(("cups_array_add(a=%p, e=%p, insert=%d)\n", a, e, insert));

 /*
  * Verify we have room for the new element...
  */

  if (a->num_elements >= a->alloc_elements)
  {
   /*
    * Allocate additional elements; start with 16 elements, then
    * double the size until 1024 elements, then add 1024 elements
    * thereafter...
    */

    void	**temp;			/* New array elements */
    int		count;			/* New allocation count */


    if (a->alloc_elements == 0)
    {
      count = 16;
      temp  = malloc(count * sizeof(void *));
    }
    else
    {
      if (a->alloc_elements < 1024)
        count = a->alloc_elements * 2;
      else
        count = a->alloc_elements + 1024;

      temp = realloc(a->elements, count * sizeof(void *));
    }

    DEBUG_printf(("cups_array_add: count=%d\n", count));

    if (!temp)
    {
      DEBUG_puts("cupsAddAdd: allocation failed, returning 0");
      return (0);
    }

    a->alloc_elements = count;
    a->elements       = temp;
  }

 /*
  * Find the insertion point for the new element; if there is no
  * compare function or elements, just add it to the beginning or end...
  */

  if (!a->num_elements || !a->compare)
  {
   /*
    * No elements or comparison function, insert/append as needed...
    */

    if (insert)
      current = 0;			/* Insert at beginning */
    else
      current = a->num_elements;	/* Append to the end */
  }
  else
  {
   /*
    * Do a binary search for the insertion point...
    */

    current = cups_array_find(a, e, a->insert, &diff);

    if (diff > 0)
    {
     /*
      * Insert after the current element...
      */

      current ++;
    }
    else if (!diff)
    {
     /*
      * Compared equal, make sure we add to the begining or end of
      * the current run of equal elements...
      */

      a->unique = 0;

      if (insert)
      {
       /*
        * Insert at beginning of run...
	*/

	while (current > 0 && !(*(a->compare))(e, a->elements[current - 1],
                                               a->data))
          current --;
      }
      else
      {
       /*
        * Append at end of run...
	*/

	do
	{
          current ++;
	}
	while (current < a->num_elements &&
               !(*(a->compare))(e, a->elements[current], a->data));
      }
    }
  }

 /*
  * Insert or append the element...
  */

  if (current < a->num_elements)
  {
   /*
    * Shift other elements to the right...
    */

    memmove(a->elements + current + 1, a->elements + current,
            (a->num_elements - current) * sizeof(void *));

    if (a->current >= current)
      a->current ++;

    for (i = 0; i < a->num_saved; i ++)
      if (a->saved[i] >= current)
	a->saved[i] ++;

    DEBUG_printf(("cups_array_add: insert element at index %d...\n", current));
  }
#ifdef DEBUG
  else
    printf("cups_array_add: append element at %d...\n", current);
#endif /* DEBUG */

  a->elements[current] = e;
  a->num_elements ++;
  a->insert = current;

#ifdef DEBUG
  for (current = 0; current < a->num_elements; current ++)
    printf("cups_array_add: a->elements[%d]=%p\n", current, a->elements[current]);
#endif /* DEBUG */

  DEBUG_puts("cups_array_add: returning 1");

  return (1);
}


/*
 * 'cups_array_find()' - Find an element in the array...
 */

static int				/* O - Index of match */
cups_array_find(cups_array_t *a,	/* I - Array */
        	void         *e,	/* I - Element */
		int          prev,	/* I - Previous index */
		int          *rdiff)	/* O - Difference of match */
{
  int	left,				/* Left side of search */
	right,				/* Right side of search */
	current,			/* Current element */
	diff;				/* Comparison with current element */


  DEBUG_printf(("cups_array_find(a=%p, e=%p, prev=%d, rdiff=%p)\n", a, e, prev,
                rdiff));

  if (a->compare)
  {
   /*
    * Do a binary search for the element...
    */

    DEBUG_puts("cups_array_find: binary search");

    if (prev >= 0 && prev < a->num_elements)
    {
     /*
      * Start search on either side of previous...
      */

      if ((diff = (*(a->compare))(e, a->elements[prev], a->data)) == 0 ||
          (diff < 0 && prev == 0) ||
	  (diff > 0 && prev == (a->num_elements - 1)))
      {
       /*
        * Exact or edge match, return it!
	*/

        DEBUG_printf(("cups_array_find: Returning %d, diff=%d\n", prev, diff));

	*rdiff = diff;

	return (prev);
      }
      else if (diff < 0)
      {
       /*
        * Start with previous on right side...
	*/

	left  = 0;
	right = prev;
      }
      else
      {
       /*
        * Start wih previous on left side...
	*/

        left  = prev;
	right = a->num_elements - 1;
      }
    }
    else
    {
     /*
      * Start search in the middle...
      */

      left  = 0;
      right = a->num_elements - 1;
    }

    do
    {
      current = (left + right) / 2;
      diff    = (*(a->compare))(e, a->elements[current], a->data);

      DEBUG_printf(("cups_array_find: left=%d, right=%d, current=%d, diff=%d\n",
                    left, right, current, diff));

      if (diff == 0)
	break;
      else if (diff < 0)
	right = current;
      else
	left = current;
    }
    while ((right - left) > 1);

    if (diff != 0)
    {
     /*
      * Check the last 1 or 2 elements...
      */

      if ((diff = (*(a->compare))(e, a->elements[left], a->data)) <= 0)
        current = left;
      else
      {
        diff    = (*(a->compare))(e, a->elements[right], a->data);
        current = right;
      }
    }
  }
  else
  {
   /*
    * Do a linear pointer search...
    */

    DEBUG_puts("cups_array_find: linear search");

    diff = 0;

    for (current = 0; current < a->num_elements; current ++)
      if (a->elements[current] == e)
        break;
  }

 /*
  * Return the closest element and the difference...
  */

  DEBUG_printf(("cups_array_find: Returning %d, diff=%d\n", current, diff));

  *rdiff = diff;

  return (current);
}


/*
 * End of "$Id$".
 */
