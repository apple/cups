/*
 * "$Id: array.c 12031 2014-07-15 19:57:59Z msweet $"
 *
 * Sorted array routines for CUPS.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include "string-private.h"
#include "debug-private.h"
#include "array-private.h"


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
  cups_ahash_func_t	hashfunc;	/* Hash function */
  int			hashsize,	/* Size of hash */
			*hash;		/* Hash array */
  cups_acopy_func_t	copyfunc;	/* Copy function */
  cups_afree_func_t	freefunc;	/* Free function */
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
 * appended at the end of the run of identical elements.  For unsorted arrays,
 * the element is appended to the end of the array.
 *
 * @since CUPS 1.2/OS X 10.5@
 */

int					/* O - 1 on success, 0 on failure */
cupsArrayAdd(cups_array_t *a,		/* I - Array */
             void         *e)		/* I - Element */
{
  DEBUG_printf(("2cupsArrayAdd(a=%p, e=%p)", a, e));

 /*
  * Range check input...
  */

  if (!a || !e)
  {
    DEBUG_puts("3cupsArrayAdd: returning 0");
    return (0);
  }

 /*
  * Append the element...
  */

  return (cups_array_add(a, e, 0));
}


/*
 * '_cupsArrayAddStrings()' - Add zero or more delimited strings to an array.
 *
 * Note: The array MUST be created using the @link _cupsArrayNewStrings@
 * function. Duplicate strings are NOT added. If the string pointer "s" is NULL
 * or the empty string, no strings are added to the array.
 */

int					/* O - 1 on success, 0 on failure */
_cupsArrayAddStrings(cups_array_t *a,	/* I - Array */
                     const char   *s,	/* I - Delimited strings or NULL */
                     char         delim)/* I - Delimiter character */
{
  char		*buffer,		/* Copy of string */
		*start,			/* Start of string */
		*end;			/* End of string */
  int		status = 1;		/* Status of add */


  DEBUG_printf(("_cupsArrayAddStrings(a=%p, s=\"%s\", delim='%c')", a, s,
                delim));

  if (!a || !s || !*s)
  {
    DEBUG_puts("1_cupsArrayAddStrings: Returning 0");
    return (0);
  }

  if (delim == ' ')
  {
   /*
    * Skip leading whitespace...
    */

    DEBUG_puts("1_cupsArrayAddStrings: Skipping leading whitespace.");

    while (*s && isspace(*s & 255))
      s ++;

    DEBUG_printf(("1_cupsArrayAddStrings: Remaining string \"%s\".", s));
  }

  if (!strchr(s, delim) &&
      (delim != ' ' || (!strchr(s, '\t') && !strchr(s, '\n'))))
  {
   /*
    * String doesn't contain a delimiter, so add it as a single value...
    */

    DEBUG_puts("1_cupsArrayAddStrings: No delimiter seen, adding a single "
               "value.");

    if (!cupsArrayFind(a, (void *)s))
      status = cupsArrayAdd(a, (void *)s);
  }
  else if ((buffer = strdup(s)) == NULL)
  {
    DEBUG_puts("1_cupsArrayAddStrings: Unable to duplicate string.");
    status = 0;
  }
  else
  {
    for (start = end = buffer; *end; start = end)
    {
     /*
      * Find the end of the current delimited string and see if we need to add
      * it...
      */

      if (delim == ' ')
      {
        while (*end && !isspace(*end & 255))
          end ++;
        while (*end && isspace(*end & 255))
          *end++ = '\0';
      }
      else if ((end = strchr(start, delim)) != NULL)
        *end++ = '\0';
      else
        end = start + strlen(start);

      DEBUG_printf(("1_cupsArrayAddStrings: Adding \"%s\", end=\"%s\"", start,
                    end));

      if (!cupsArrayFind(a, start))
        status &= cupsArrayAdd(a, start);
    }

    free(buffer);
  }

  DEBUG_printf(("1_cupsArrayAddStrings: Returning %d.", status));

  return (status);
}


/*
 * 'cupsArrayClear()' - Clear the array.
 *
 * This function is equivalent to removing all elements in the array.
 * The caller is responsible for freeing the memory used by the
 * elements themselves.
 *
 * @since CUPS 1.2/OS X 10.5@
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
  * Free the existing elements as needed..
  */

  if (a->freefunc)
  {
    int		i;			/* Looping var */
    void	**e;			/* Current element */

    for (i = a->num_elements, e = a->elements; i > 0; i --, e ++)
      (a->freefunc)(*e, a->data);
  }

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
 *
 * @since CUPS 1.2/OS X 10.5@
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
 *
 * The current element is undefined until you call @link cupsArrayFind@,
 * @link cupsArrayFirst@, or @link cupsArrayIndex@, or @link cupsArrayLast@.
 *
 * @since CUPS 1.2/OS X 10.5@
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
 *
 * The caller is responsible for freeing the memory used by the
 * elements themselves.
 *
 * @since CUPS 1.2/OS X 10.5@
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
  * Free the elements if we have a free function (otherwise the caller is
  * responsible for doing the dirty work...)
  */

  if (a->freefunc)
  {
    int		i;			/* Looping var */
    void	**e;			/* Current element */

    for (i = a->num_elements, e = a->elements; i > 0; i --, e ++)
      (a->freefunc)(*e, a->data);
  }

 /*
  * Free the array of element pointers...
  */

  if (a->alloc_elements)
    free(a->elements);

  if (a->hashsize)
    free(a->hash);

  free(a);
}


/*
 * 'cupsArrayDup()' - Duplicate the array.
 *
 * @since CUPS 1.2/OS X 10.5@
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

    da->elements = malloc((size_t)a->num_elements * sizeof(void *));
    if (!da->elements)
    {
      free(da);
      return (NULL);
    }

   /*
    * Copy the element pointers...
    */

    if (a->copyfunc)
    {
     /*
      * Use the copy function to make a copy of each element...
      */

      int	i;			/* Looping var */

      for (i = 0; i < a->num_elements; i ++)
	da->elements[i] = (a->copyfunc)(a->elements[i], a->data);
    }
    else
    {
     /*
      * Just copy raw pointers...
      */

      memcpy(da->elements, a->elements, (size_t)a->num_elements * sizeof(void *));
    }

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
 *
 * @since CUPS 1.2/OS X 10.5@
 */

void *					/* O - Element found or @code NULL@ */
cupsArrayFind(cups_array_t *a,		/* I - Array */
              void         *e)		/* I - Element */
{
  int	current,			/* Current element */
	diff,				/* Difference */
	hash;				/* Hash index */


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

  if (a->hash)
  {
    hash = (*(a->hashfunc))(e, a->data);

    if (hash < 0 || hash >= a->hashsize)
    {
      current = a->current;
      hash    = -1;
    }
    else
    {
      current = a->hash[hash];

      if (current < 0 || current >= a->num_elements)
        current = a->current;
    }
  }
  else
  {
    current = a->current;
    hash    = -1;
  }

  current = cups_array_find(a, e, current, &diff);
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

    if (hash >= 0)
      a->hash[hash] = current;

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
 *
 * @since CUPS 1.2/OS X 10.5@
 */

void *					/* O - First element or @code NULL@ if the array is empty */
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
 * 'cupsArrayGetIndex()' - Get the index of the current element.
 *
 * The current element is undefined until you call @link cupsArrayFind@,
 * @link cupsArrayFirst@, or @link cupsArrayIndex@, or @link cupsArrayLast@.
 *
 * @since CUPS 1.3/OS X 10.5@
 */

int					/* O - Index of the current element, starting at 0 */
cupsArrayGetIndex(cups_array_t *a)	/* I - Array */
{
  if (!a)
    return (-1);
  else
    return (a->current);
}


/*
 * 'cupsArrayGetInsert()' - Get the index of the last inserted element.
 *
 * @since CUPS 1.3/OS X 10.5@
 */

int					/* O - Index of the last inserted element, starting at 0 */
cupsArrayGetInsert(cups_array_t *a)	/* I - Array */
{
  if (!a)
    return (-1);
  else
    return (a->insert);
}


/*
 * 'cupsArrayIndex()' - Get the N-th element in the array.
 *
 * @since CUPS 1.2/OS X 10.5@
 */

void *					/* O - N-th element or @code NULL@ */
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
 * inserted at the beginning of the run of identical elements.  For unsorted
 * arrays, the element is inserted at the beginning of the array.
 *
 * @since CUPS 1.2/OS X 10.5@
 */

int					/* O - 0 on failure, 1 on success */
cupsArrayInsert(cups_array_t *a,	/* I - Array */
		void         *e)	/* I - Element */
{
  DEBUG_printf(("2cupsArrayInsert(a=%p, e=%p)", a, e));

 /*
  * Range check input...
  */

  if (!a || !e)
  {
    DEBUG_puts("3cupsArrayInsert: returning 0");
    return (0);
  }

 /*
  * Insert the element...
  */

  return (cups_array_add(a, e, 1));
}


/*
 * 'cupsArrayLast()' - Get the last element in the array.
 *
 * @since CUPS 1.2/OS X 10.5@
 */

void *					/* O - Last element or @code NULL@ if the array is empty */
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
 *
 * The comparison function ("f") is used to create a sorted array. The function
 * receives pointers to two elements and the user data pointer ("d") - the user
 * data pointer argument can safely be omitted when not required so functions
 * like @code strcmp@ can be used for sorted string arrays.
 *
 * @since CUPS 1.2/OS X 10.5@
 */

cups_array_t *				/* O - Array */
cupsArrayNew(cups_array_func_t f,	/* I - Comparison function or @code NULL@ for an unsorted array */
             void              *d)	/* I - User data pointer or @code NULL@ */
{
  return (cupsArrayNew3(f, d, 0, 0, 0, 0));
}


/*
 * 'cupsArrayNew2()' - Create a new array with hash.
 *
 * The comparison function ("f") is used to create a sorted array. The function
 * receives pointers to two elements and the user data pointer ("d") - the user
 * data pointer argument can safely be omitted when not required so functions
 * like @code strcmp@ can be used for sorted string arrays.
 *
 * The hash function ("h") is used to implement cached lookups with the
 * specified hash size ("hsize").
 *
 * @since CUPS 1.3/OS X 10.5@
 */

cups_array_t *				/* O - Array */
cupsArrayNew2(cups_array_func_t  f,	/* I - Comparison function or @code NULL@ for an unsorted array */
              void               *d,	/* I - User data or @code NULL@ */
              cups_ahash_func_t  h,	/* I - Hash function or @code NULL@ for unhashed lookups */
	      int                hsize)	/* I - Hash size (>= 0) */
{
  return (cupsArrayNew3(f, d, h, hsize, 0, 0));
}


/*
 * 'cupsArrayNew3()' - Create a new array with hash and/or free function.
 *
 * The comparison function ("f") is used to create a sorted array. The function
 * receives pointers to two elements and the user data pointer ("d") - the user
 * data pointer argument can safely be omitted when not required so functions
 * like @code strcmp@ can be used for sorted string arrays.
 *
 * The hash function ("h") is used to implement cached lookups with the
 * specified hash size ("hsize").
 *
 * The copy function ("cf") is used to automatically copy/retain elements when
 * added or the array is copied.
 *
 * The free function ("cf") is used to automatically free/release elements when
 * removed or the array is deleted.
 *
 * @since CUPS 1.5/OS X 10.7@
 */

cups_array_t *				/* O - Array */
cupsArrayNew3(cups_array_func_t  f,	/* I - Comparison function or @code NULL@ for an unsorted array */
              void               *d,	/* I - User data or @code NULL@ */
              cups_ahash_func_t  h,	/* I - Hash function or @code NULL@ for unhashed lookups */
	      int                hsize,	/* I - Hash size (>= 0) */
	      cups_acopy_func_t  cf,	/* I - Copy function */
	      cups_afree_func_t  ff)	/* I - Free function */
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

  if (hsize > 0 && h)
  {
    a->hashfunc  = h;
    a->hashsize  = hsize;
    a->hash      = malloc((size_t)hsize * sizeof(int));

    if (!a->hash)
    {
      free(a);
      return (NULL);
    }

    memset(a->hash, -1, (size_t)hsize * sizeof(int));
  }

  a->copyfunc = cf;
  a->freefunc = ff;

  return (a);
}


/*
 * '_cupsArrayNewStrings()' - Create a new array of comma-delimited strings.
 *
 * Note: The array automatically manages copies of the strings passed. If the
 * string pointer "s" is NULL or the empty string, no strings are added to the
 * newly created array.
 */

cups_array_t *				/* O - Array */
_cupsArrayNewStrings(const char *s,	/* I - Delimited strings or NULL */
                     char       delim)	/* I - Delimiter character */
{
  cups_array_t	*a;			/* Array */


  if ((a = cupsArrayNew3((cups_array_func_t)strcmp, NULL, NULL, 0,
                         (cups_acopy_func_t)_cupsStrAlloc,
			 (cups_afree_func_t)_cupsStrFree)) != NULL)
    _cupsArrayAddStrings(a, s, delim);

  return (a);
}


/*
 * 'cupsArrayNext()' - Get the next element in the array.
 *
 * This function is equivalent to "cupsArrayIndex(a, cupsArrayGetIndex(a) + 1)".
 *
 * The next element is undefined until you call @link cupsArrayFind@,
 * @link cupsArrayFirst@, or @link cupsArrayIndex@, or @link cupsArrayLast@
 * to set the current element.
 *
 * @since CUPS 1.2/OS X 10.5@
 */

void *					/* O - Next element or @code NULL@ */
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
 *
 * This function is equivalent to "cupsArrayIndex(a, cupsArrayGetIndex(a) - 1)".
 *
 * The previous element is undefined until you call @link cupsArrayFind@,
 * @link cupsArrayFirst@, or @link cupsArrayIndex@, or @link cupsArrayLast@
 * to set the current element.
 *
 * @since CUPS 1.2/OS X 10.5@
 */

void *					/* O - Previous element or @code NULL@ */
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
 *
 * If more than one element matches "e", only the first matching element is
 * removed.
 *
 * The caller is responsible for freeing the memory used by the
 * removed element.
 *
 * @since CUPS 1.2/OS X 10.5@
 */

int					/* O - 1 on success, 0 on failure */
cupsArrayRemove(cups_array_t *a,	/* I - Array */
                void         *e)	/* I - Element */
{
  ssize_t	i,			/* Looping var */
		current;		/* Current element */
  int		diff;			/* Difference */


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

  if (a->freefunc)
    (a->freefunc)(a->elements[current], a->data);

  if (current < a->num_elements)
    memmove(a->elements + current, a->elements + current + 1,
            (size_t)(a->num_elements - current) * sizeof(void *));

  if (current <= a->current)
    a->current --;

  if (current < a->insert)
    a->insert --;
  else if (current == a->insert)
    a->insert = -1;

  for (i = 0; i < a->num_saved; i ++)
    if (current <= a->saved[i])
      a->saved[i] --;

  if (a->num_elements <= 1)
    a->unique = 1;

  return (1);
}


/*
 * 'cupsArrayRestore()' - Reset the current element to the last @link cupsArraySave@.
 *
 * @since CUPS 1.2/OS X 10.5@
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
 * 'cupsArraySave()' - Mark the current element for a later @link cupsArrayRestore@.
 *
 * The current element is undefined until you call @link cupsArrayFind@,
 * @link cupsArrayFirst@, or @link cupsArrayIndex@, or @link cupsArrayLast@
 * to set the current element.
 *
 * The save/restore stack is guaranteed to be at least 32 elements deep.
 *
 * @since CUPS 1.2/OS X 10.5@
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
 * 'cupsArrayUserData()' - Return the user data for an array.
 *
 * @since CUPS 1.2/OS X 10.5@
 */

void *					/* O - User data */
cupsArrayUserData(cups_array_t *a)	/* I - Array */
{
  if (a)
    return (a->data);
  else
    return (NULL);
}


/*
 * 'cups_array_add()' - Insert or append an element to the array.
 *
 * @since CUPS 1.2/OS X 10.5@
 */

static int				/* O - 1 on success, 0 on failure */
cups_array_add(cups_array_t *a,		/* I - Array */
               void         *e,		/* I - Element to add */
	       int          insert)	/* I - 1 = insert, 0 = append */
{
  int		i,			/* Looping var */
		current;		/* Current element */
  int		diff;			/* Comparison with current element */


  DEBUG_printf(("7cups_array_add(a=%p, e=%p, insert=%d)", a, e, insert));

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
      temp  = malloc((size_t)count * sizeof(void *));
    }
    else
    {
      if (a->alloc_elements < 1024)
        count = a->alloc_elements * 2;
      else
        count = a->alloc_elements + 1024;

      temp = realloc(a->elements, (size_t)count * sizeof(void *));
    }

    DEBUG_printf(("9cups_array_add: count=" CUPS_LLFMT, CUPS_LLCAST count));

    if (!temp)
    {
      DEBUG_puts("9cups_array_add: allocation failed, returning 0");
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
            (size_t)(a->num_elements - current) * sizeof(void *));

    if (a->current >= current)
      a->current ++;

    for (i = 0; i < a->num_saved; i ++)
      if (a->saved[i] >= current)
	a->saved[i] ++;

    DEBUG_printf(("9cups_array_add: insert element at index " CUPS_LLFMT, CUPS_LLCAST current));
  }
#ifdef DEBUG
  else
    DEBUG_printf(("9cups_array_add: append element at " CUPS_LLFMT, CUPS_LLCAST current));
#endif /* DEBUG */

  if (a->copyfunc)
  {
    if ((a->elements[current] = (a->copyfunc)(e, a->data)) == NULL)
    {
      DEBUG_puts("8cups_array_add: Copy function returned NULL, returning 0");
      return (0);
    }
  }
  else
    a->elements[current] = e;

  a->num_elements ++;
  a->insert = current;

#ifdef DEBUG
  for (current = 0; current < a->num_elements; current ++)
    DEBUG_printf(("9cups_array_add: a->elements[" CUPS_LLFMT "]=%p", CUPS_LLCAST current, a->elements[current]));
#endif /* DEBUG */

  DEBUG_puts("9cups_array_add: returning 1");

  return (1);
}


/*
 * 'cups_array_find()' - Find an element in the array.
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


  DEBUG_printf(("7cups_array_find(a=%p, e=%p, prev=%d, rdiff=%p)", a, e, prev,
                rdiff));

  if (a->compare)
  {
   /*
    * Do a binary search for the element...
    */

    DEBUG_puts("9cups_array_find: binary search");

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

        DEBUG_printf(("9cups_array_find: Returning %d, diff=%d", prev, diff));

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

      DEBUG_printf(("9cups_array_find: left=%d, right=%d, current=%d, diff=%d",
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

    DEBUG_puts("9cups_array_find: linear search");

    diff = 1;

    for (current = 0; current < a->num_elements; current ++)
      if (a->elements[current] == e)
      {
        diff = 0;
        break;
      }
  }

 /*
  * Return the closest element and the difference...
  */

  DEBUG_printf(("8cups_array_find: Returning %d, diff=%d", current, diff));

  *rdiff = diff;

  return (current);
}


/*
 * End of "$Id: array.c 12031 2014-07-15 19:57:59Z msweet $".
 */
