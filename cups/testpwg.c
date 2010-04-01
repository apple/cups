/*
 * "$Id$"
 *
 *   PWG test program for CUPS.
 *
 *   Copyright 2009-2010 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main()     - Main entry.
 *   test_pwg() - Test the PWG mapping functions.
 */

/*
 * Include necessary headers...
 */

#include "ppd-private.h"


/*
 * Local functions...
 */

static int	test_pwg(_pwg_t *pwg);


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		status;			/* Status of tests (0 = success, 1 = fail) */
  const char	*ppdfile;		/* PPD filename */
  ppd_file_t	*ppd;			/* PPD file */
  _pwg_t	*pwg;			/* PWG mapping data */
  _pwg_media_t	*pwgmedia;		/* PWG media size */


  status = 0;

  if (argc != 2)
  {
    puts("Usage: ./testpwg filename.ppd");
    return (1);
  }
  else
    ppdfile = argv[1];

  printf("ppdOpenFile(%s): ", ppdfile);
  if ((ppd = ppdOpenFile(ppdfile)) == NULL)
  {
    ppd_status_t err;			/* Last error in file */
    int		line;			/* Line number in file */


    err = ppdLastError(&line);

    printf("FAIL (%s on line %d)\n", ppdErrorString(err), line);

    return (1);
  }
  else
    puts("PASS");

  fputs("_pwgCreateWithPPD(ppd): ", stdout);
  if ((pwg = _pwgCreateWithPPD(ppd)) == NULL)
  {
    puts("FAIL");
    status ++;
  }
  else
  {
    puts("PASS");
    status += test_pwg(pwg);

   /*
    * _pwgDestroy should never fail...
    */

    fputs("_pwgDestroy(pwg): ", stdout);
    _pwgDestroy(pwg);
    puts("PASS");
  }

  fputs("_pwgMediaForSize(29700, 42000): ", stdout);
  if ((pwgmedia = _pwgMediaForSize(29700, 42000)) == NULL)
  {
    puts("FAIL (not found)");
    status ++;
  }
  else if (strcmp(pwgmedia->pwg, "iso_a3_297x420mm"))
  {
    printf("FAIL (%s)\n", pwgmedia->pwg);
    status ++;
  }
  else
    puts("PASS");

  return (status);
}


/*
 * 'test_pwg()' - Test the PWG mapping functions.
 */

static int				/* O - 1 on failure, 0 on success */
test_pwg(_pwg_t *pwg)			/* I - PWG mapping data */
{
  int		i,			/* Looping var */
		status = 0;		/* Return status */
  _pwg_t	*pwg2;			/* Loaded data */
  _pwg_size_t	*size,			/* Size from original */
		*size2;			/* Size from saved */
  _pwg_map_t	*map,			/* Map from original */
		*map2;			/* Map from saved */


 /*
  * Verify that we can write and read back the same data...
  */

  fputs("_pwgWriteFile(test.pwg): ", stdout);
  if (!_pwgWriteFile(pwg, "test.pwg"))
  {
    puts("FAIL");
    status ++;
  }
  else
    puts("PASS");

  fputs("_pwgCreateWithFile(test.pwg): ", stdout);
  if ((pwg2 = _pwgCreateWithFile("test.pwg")) == NULL)
  {
    puts("FAIL");
    status ++;
  }
  else
  {
    if (pwg2->num_sizes != pwg->num_sizes)
    {
      if (!status)
        puts("FAIL");

      printf("    SAVED num_sizes=%d, ORIG num_sizes=%d\n", pwg2->num_sizes,
             pwg->num_sizes);

      status ++;
    }
    else
    {
      for (i = pwg->num_sizes, size = pwg->sizes, size2 = pwg2->sizes;
           i > 0;
	   i --, size ++, size2 ++)
      {
        if (strcmp(size2->map.pwg, size->map.pwg) ||
	    strcmp(size2->map.ppd, size->map.ppd) ||
	    size2->width != size->width ||
	    size2->length != size->length ||
	    size2->left != size->left ||
	    size2->bottom != size->bottom ||
	    size2->right != size->right ||
	    size2->top != size->top)
	{
	  if (!status)
	    puts("FAIL");

	  if (strcmp(size->map.pwg, size2->map.pwg))
	    printf("    SAVED size->map.pwg=\"%s\", ORIG "
	           "size->map.pwg=\"%s\"\n", size2->map.pwg, size->map.pwg);

	  if (strcmp(size2->map.ppd, size->map.ppd))
	    printf("    SAVED size->map.ppd=\"%s\", ORIG "
	           "size->map.ppd=\"%s\"\n", size2->map.ppd, size->map.ppd);

	  if (size2->width != size->width)
	    printf("    SAVED size->width=%d, ORIG size->width=%d\n",
		   size2->width, size->width);

	  if (size2->length != size->length)
	    printf("    SAVED size->length=%d, ORIG size->length=%d\n",
		   size2->length, size->length);

	  if (size2->left != size->left)
	    printf("    SAVED size->left=%d, ORIG size->left=%d\n",
		   size2->left, size->left);

	  if (size2->bottom != size->bottom)
	    printf("    SAVED size->bottom=%d, ORIG size->bottom=%d\n",
		   size2->bottom, size->bottom);

	  if (size2->right != size->right)
	    printf("    SAVED size->right=%d, ORIG size->right=%d\n",
		   size2->right, size->right);

	  if (size2->top != size->top)
	    printf("    SAVED size->top=%d, ORIG size->top=%d\n",
		   size2->top, size->top);

	  status ++;
	  break;
	}
      }

      for (i = pwg->num_sources, map = pwg->sources, map2 = pwg2->sources;
           i > 0;
	   i --, map ++, map2 ++)
      {
        if (strcmp(map2->pwg, map->pwg) ||
	    strcmp(map2->ppd, map->ppd))
	{
	  if (!status)
	    puts("FAIL");

	  if (strcmp(map->pwg, map2->pwg))
	    printf("    SAVED source->pwg=\"%s\", ORIG source->pwg=\"%s\"\n",
	           map2->pwg, map->pwg);

	  if (strcmp(map2->ppd, map->ppd))
	    printf("    SAVED source->ppd=\"%s\", ORIG source->ppd=\"%s\"\n",
	           map2->ppd, map->ppd);

	  status ++;
	  break;
	}
      }

      for (i = pwg->num_types, map = pwg->types, map2 = pwg2->types;
           i > 0;
	   i --, map ++, map2 ++)
      {
        if (strcmp(map2->pwg, map->pwg) ||
	    strcmp(map2->ppd, map->ppd))
	{
	  if (!status)
	    puts("FAIL");

	  if (strcmp(map->pwg, map2->pwg))
	    printf("    SAVED type->pwg=\"%s\", ORIG type->pwg=\"%s\"\n",
	           map2->pwg, map->pwg);

	  if (strcmp(map2->ppd, map->ppd))
	    printf("    SAVED type->ppd=\"%s\", ORIG type->ppd=\"%s\"\n",
	           map2->ppd, map->ppd);

	  status ++;
	  break;
	}
      }
    }

    if (!status)
      puts("PASS");
  }

  return (status);
}


/*
 * End of "$Id$".
 */
