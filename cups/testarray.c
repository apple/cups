/*
 * "$Id$"
 *
 *   Array test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
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
 *   main() - Main entry.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <cups/string.h>
#include <errno.h>
#include "array.h"


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  cups_array_t	*array,			/* Test array */
		*dup_array;		/* Duplicate array */
  int		status;			/* Exit status */
  char		*text;			/* Text from array */
#ifdef DEBUG
  int		i;			/* Looping var */
#endif /* DEBUG */


 /*
  * No errors so far...
  */

  status = 0;

 /*
  * cupsArrayNew()
  */

  fputs("cupsArrayNew: ", stdout);

  array = cupsArrayNew((cups_array_func_t)strcmp, NULL);

  if (array)
    puts("PASS");
  else
  {
    puts("FAIL (returned NULL, expected pointer)");
    status ++;
  }

 /*
  * cupsArrayAdd()
  */

  fputs("cupsArrayAdd: ", stdout);

  if (!cupsArrayAdd(array, strdup("One Fish")))
  {
    puts("FAIL (\"One Fish\")");
    status ++;
  }
  else
  {
#ifdef DEBUG
    putchar('\n');
    for (text = (char *)cupsArrayFirst(array), i = 0;
         text;
	 text = (char *)cupsArrayNext(array), i ++)
      printf("    #1  array[%d]=\"%s\"\n", i, text);
#endif /* DEBUG */

    if (!cupsArrayAdd(array, strdup("Two Fish")))
    {
      puts("FAIL (\"Two Fish\")");
      status ++;
    }
    else
    {
#ifdef DEBUG
      for (text = (char *)cupsArrayFirst(array), i = 0;
           text;
	   text = (char *)cupsArrayNext(array), i ++)
	printf("    #2  array[%d]=\"%s\"\n", i, text);
#endif /* DEBUG */

      if (!cupsArrayAdd(array, strdup("Red Fish")))
      {
	puts("FAIL (\"Red Fish\")");
	status ++;
      }
      else
      {
#ifdef DEBUG
	for (text = (char *)cupsArrayFirst(array), i = 0;
             text;
	     text = (char *)cupsArrayNext(array), i ++)
	  printf("    #3  array[%d]=\"%s\"\n", i, text);
#endif /* DEBUG */

        if (!cupsArrayAdd(array, strdup("Blue Fish")))
	{
	  puts("FAIL (\"Blue Fish\")");
	  status ++;
	}
	else
	{
#ifdef DEBUG
	  for (text = (char *)cupsArrayFirst(array), i = 0;
               text;
	       text = (char *)cupsArrayNext(array), i ++)
	    printf("    #4  array[%d]=\"%s\"\n", i, text);
#endif /* DEBUG */

	  puts("PASS");
	}
      }
    }
  }

 /*
  * cupsArrayCount()
  */
 
  fputs("cupsArrayCount: ", stdout);
  if (cupsArrayCount(array) == 4)
    puts("PASS");
  else
  {
    printf("FAIL (returned %d, expected 4)\n", cupsArrayCount(array));
    status ++;
  }

 /*
  * cupsArrayFirst()
  */

  fputs("cupsArrayFirst: ", stdout);
  if ((text = (char *)cupsArrayFirst(array)) != NULL &&
      !strcmp(text, "Blue Fish"))
    puts("PASS");
  else
  {
    printf("FAIL (returned \"%s\", expected \"Blue Fish\")\n", text);
    status ++;
  }

 /*
  * cupsArrayNext()
  */

  fputs("cupsArrayNext: ", stdout);
  if ((text = (char *)cupsArrayNext(array)) != NULL &&
      !strcmp(text, "One Fish"))
    puts("PASS");
  else
  {
    printf("FAIL (returned \"%s\", expected \"One Fish\")\n", text);
    status ++;
  }

 /*
  * cupsArrayLast()
  */

  fputs("cupsArrayLast: ", stdout);
  if ((text = (char *)cupsArrayLast(array)) != NULL &&
      !strcmp(text, "Two Fish"))
    puts("PASS");
  else
  {
    printf("FAIL (returned \"%s\", expected \"Two Fish\")\n", text);
    status ++;
  }

 /*
  * cupsArrayPrev()
  */

  fputs("cupsArrayPrev: ", stdout);
  if ((text = (char *)cupsArrayPrev(array)) != NULL &&
      !strcmp(text, "Red Fish"))
    puts("PASS");
  else
  {
    printf("FAIL (returned \"%s\", expected \"Red Fish\")\n", text);
    status ++;
  }

 /*
  * cupsArrayFind()
  */

  fputs("cupsArrayFind: ", stdout);
  if ((text = (char *)cupsArrayFind(array, (void *)"One Fish")) != NULL &&
      !strcmp(text, "One Fish"))
    puts("PASS");
  else
  {
    printf("FAIL (returned \"%s\", expected \"One Fish\")\n", text);
    status ++;
  }

 /*
  * cupsArrayCurrent()
  */

  fputs("cupsArrayCurrent: ", stdout);
  if ((text = (char *)cupsArrayCurrent(array)) != NULL &&
      !strcmp(text, "One Fish"))
    puts("PASS");
  else
  {
    printf("FAIL (returned \"%s\", expected \"One Fish\")\n", text);
    status ++;
  }

 /*
  * cupsArrayDup()
  */

  fputs("cupsArrayDup: ", stdout);
  if ((dup_array = cupsArrayDup(array)) != NULL &&
      cupsArrayCount(dup_array) == 4)
    puts("PASS");
  else
  {
    printf("FAIL (returned %p with %d elements, expected pointer with 4 elements)\n",
           dup_array, cupsArrayCount(dup_array));
    status ++;
  }

 /*
  * cupsArrayRemove()
  */

  fputs("cupsArrayRemove: ", stdout);
  if (cupsArrayRemove(array, (void *)"One Fish") &&
      cupsArrayCount(array) == 3)
    puts("PASS");
  else
  {
    printf("FAIL (returned 0 with %d elements, expected 1 with 4 elements)\n",
           cupsArrayCount(array));
    status ++;
  }

  cupsArrayDelete(array);
  cupsArrayDelete(dup_array);

  if (!status)
    puts("\nALL TESTS PASSED!");
  else
    printf("\n%d TEST(S) FAILED!\n", status);

  return (status);
}


/*
 * End of "$Id$".
 */
