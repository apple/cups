/*
 * "$Id: testpwg.c 11240 2013-08-14 20:33:55Z msweet $"
 *
 *   PWG test program for CUPS.
 *
 *   Copyright 2009-2013 by Apple Inc.
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
 *   main()           - Main entry.
 *   test_pagesize()  - Test the PWG mapping functions.
 *   test_ppd_cache() - Test the PPD cache functions.
 */

/*
 * Include necessary headers...
 */

#include "ppd-private.h"
#include "file-private.h"


/*
 * Local functions...
 */

static int	test_pagesize(_ppd_cache_t *pc, ppd_file_t *ppd,
		              const char *ppdsize);
static int	test_ppd_cache(_ppd_cache_t *pc, ppd_file_t *ppd);


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int			status;		/* Status of tests (0 = success, 1 = fail) */
  const char		*ppdfile;	/* PPD filename */
  ppd_file_t		*ppd;		/* PPD file */
  _ppd_cache_t		*pc;		/* PPD cache and PWG mapping data */
  const pwg_media_t	*pwgmedia;	/* PWG media size */
  size_t		i,		/* Looping var */
			num_media;	/* Number of media sizes */
  const pwg_media_t	*mediatable;	/* Media size table */
  int			dupmedia = 0;	/* Duplicate media sizes? */


  status = 0;

  if (argc < 2 || argc > 3)
  {
    puts("Usage: ./testpwg filename.ppd [jobfile]");
    return (1);
  }

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

  fputs("_ppdCacheCreateWithPPD(ppd): ", stdout);
  if ((pc = _ppdCacheCreateWithPPD(ppd)) == NULL)
  {
    puts("FAIL");
    status ++;
  }
  else
  {
    puts("PASS");
    status += test_ppd_cache(pc, ppd);

    if (argc == 3)
    {
     /*
      * Test PageSize mapping code.
      */

      int		fd;		/* Job file descriptor */
      const char	*pagesize;	/* PageSize value */
      ipp_t		*job;		/* Job attributes */
      ipp_attribute_t	*media;		/* Media attribute */

      if ((fd = open(argv[2], O_RDONLY)) >= 0)
      {
	job = ippNew();
	ippReadFile(fd, job);
	close(fd);

        if ((media = ippFindAttribute(job, "media", IPP_TAG_ZERO)) != NULL &&
	    media->value_tag != IPP_TAG_NAME &&
	    media->value_tag != IPP_TAG_KEYWORD)
	  media = NULL;

	if (media)
	  printf("_ppdCacheGetPageSize(media=%s): ",
	         media->values[0].string.text);
	else
	  fputs("_ppdCacheGetPageSize(media-col): ", stdout);

        fflush(stdout);

	if ((pagesize = _ppdCacheGetPageSize(pc, job, NULL, NULL)) == NULL)
	{
	  puts("FAIL (Not Found)");
	  status = 1;
	}
	else if (media && _cups_strcasecmp(pagesize, media->values[0].string.text))
	{
	  printf("FAIL (Got \"%s\", Expected \"%s\")\n", pagesize,
		 media->values[0].string.text);
	  status = 1;
	}
	else
	  printf("PASS (%s)\n", pagesize);

	ippDelete(job);
      }
      else
      {
        perror(argv[2]);
	status = 1;
      }
    }

   /*
    * _ppdCacheDestroy should never fail...
    */

    fputs("_ppdCacheDestroy(pc): ", stdout);
    _ppdCacheDestroy(pc);
    puts("PASS");
  }

  fputs("pwgMediaForPWG(\"iso_a4_210x297mm\"): ", stdout);
  if ((pwgmedia = pwgMediaForPWG("iso_a4_210x297mm")) == NULL)
  {
    puts("FAIL (not found)");
    status ++;
  }
  else if (strcmp(pwgmedia->pwg, "iso_a4_210x297mm"))
  {
    printf("FAIL (%s)\n", pwgmedia->pwg);
    status ++;
  }
  else if (pwgmedia->width != 21000 || pwgmedia->length != 29700)
  {
    printf("FAIL (%dx%d)\n", pwgmedia->width, pwgmedia->length);
    status ++;
  }
  else
    puts("PASS");

  fputs("pwgMediaForPWG(\"roll_max_36.1025x3622.0472in\"): ", stdout);
  if ((pwgmedia = pwgMediaForPWG("roll_max_36.1025x3622.0472in")) == NULL)
  {
    puts("FAIL (not found)");
    status ++;
  }
  else if (pwgmedia->width != 91700 || pwgmedia->length != 9199999)
  {
    printf("FAIL (%dx%d)\n", pwgmedia->width, pwgmedia->length);
    status ++;
  }
  else
    printf("PASS (%dx%d)\n", pwgmedia->width, pwgmedia->length);

  fputs("pwgMediaForLegacy(\"na-letter\"): ", stdout);
  if ((pwgmedia = pwgMediaForLegacy("na-letter")) == NULL)
  {
    puts("FAIL (not found)");
    status ++;
  }
  else if (strcmp(pwgmedia->pwg, "na_letter_8.5x11in"))
  {
    printf("FAIL (%s)\n", pwgmedia->pwg);
    status ++;
  }
  else if (pwgmedia->width != 21590 || pwgmedia->length != 27940)
  {
    printf("FAIL (%dx%d)\n", pwgmedia->width, pwgmedia->length);
    status ++;
  }
  else
    puts("PASS");

  fputs("pwgMediaForPPD(\"4x6\"): ", stdout);
  if ((pwgmedia = pwgMediaForPPD("4x6")) == NULL)
  {
    puts("FAIL (not found)");
    status ++;
  }
  else if (strcmp(pwgmedia->pwg, "na_index-4x6_4x6in"))
  {
    printf("FAIL (%s)\n", pwgmedia->pwg);
    status ++;
  }
  else if (pwgmedia->width != 10160 || pwgmedia->length != 15240)
  {
    printf("FAIL (%dx%d)\n", pwgmedia->width, pwgmedia->length);
    status ++;
  }
  else
    puts("PASS");

  fputs("pwgMediaForPPD(\"10x15cm\"): ", stdout);
  if ((pwgmedia = pwgMediaForPPD("10x15cm")) == NULL)
  {
    puts("FAIL (not found)");
    status ++;
  }
  else if (strcmp(pwgmedia->pwg, "om_100x150mm_100x150mm"))
  {
    printf("FAIL (%s)\n", pwgmedia->pwg);
    status ++;
  }
  else if (pwgmedia->width != 10000 || pwgmedia->length != 15000)
  {
    printf("FAIL (%dx%d)\n", pwgmedia->width, pwgmedia->length);
    status ++;
  }
  else
    puts("PASS");

  fputs("pwgMediaForPPD(\"Custom.10x15cm\"): ", stdout);
  if ((pwgmedia = pwgMediaForPPD("Custom.10x15cm")) == NULL)
  {
    puts("FAIL (not found)");
    status ++;
  }
  else if (strcmp(pwgmedia->pwg, "custom_10x15cm_100x150mm"))
  {
    printf("FAIL (%s)\n", pwgmedia->pwg);
    status ++;
  }
  else if (pwgmedia->width != 10000 || pwgmedia->length != 15000)
  {
    printf("FAIL (%dx%d)\n", pwgmedia->width, pwgmedia->length);
    status ++;
  }
  else
    puts("PASS");

  fputs("pwgMediaForSize(29700, 42000): ", stdout);
  if ((pwgmedia = pwgMediaForSize(29700, 42000)) == NULL)
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

  fputs("pwgMediaForSize(9842, 19050): ", stdout);
  if ((pwgmedia = pwgMediaForSize(9842, 19050)) == NULL)
  {
    puts("FAIL (not found)");
    status ++;
  }
  else if (strcmp(pwgmedia->pwg, "na_monarch_3.875x7.5in"))
  {
    printf("FAIL (%s)\n", pwgmedia->pwg);
    status ++;
  }
  else
    printf("PASS (%s)\n", pwgmedia->pwg);

  fputs("pwgMediaForSize(9800, 19000): ", stdout);
  if ((pwgmedia = pwgMediaForSize(9800, 19000)) == NULL)
  {
    puts("FAIL (not found)");
    status ++;
  }
  else if (strcmp(pwgmedia->pwg, "jpn_you6_98x190mm"))
  {
    printf("FAIL (%s)\n", pwgmedia->pwg);
    status ++;
  }
  else
    printf("PASS (%s)\n", pwgmedia->pwg);

  fputs("Duplicate size test: ", stdout);
  for (mediatable = _pwgMediaTable(&num_media);
       num_media > 1;
       num_media --, mediatable ++)
  {
    for (i = num_media - 1, pwgmedia = mediatable + 1; i > 0; i --, pwgmedia ++)
    {
      if (pwgmedia->width == mediatable->width &&
          pwgmedia->length == mediatable->length)
      {
        if (!dupmedia)
        {
          dupmedia = 1;
          status ++;
          puts("FAIL");
        }

        printf("    %s and %s have the same dimensions (%dx%d)\n",
               pwgmedia->pwg, mediatable->pwg, pwgmedia->width,
               pwgmedia->length);
      }
    }
  }
  if (!dupmedia)
    puts("PASS");


  return (status);
}


/*
 * 'test_pagesize()' - Test the PWG mapping functions.
 */

static int				/* O - 1 on failure, 0 on success */
test_pagesize(_ppd_cache_t *pc,		/* I - PWG mapping data */
              ppd_file_t   *ppd,	/* I - PPD file */
	      const char   *ppdsize)	/* I - PPD page size */
{
  int		status = 0;		/* Return status */
  ipp_t		*job;			/* Job attributes */
  const char	*pagesize;		/* PageSize value */


  if (ppdPageSize(ppd, ppdsize))
  {
    printf("_ppdCacheGetPageSize(keyword=%s): ", ppdsize);
    fflush(stdout);

    if ((pagesize = _ppdCacheGetPageSize(pc, NULL, ppdsize, NULL)) == NULL)
    {
      puts("FAIL (Not Found)");
      status = 1;
    }
    else if (_cups_strcasecmp(pagesize, ppdsize))
    {
      printf("FAIL (Got \"%s\", Expected \"%s\")\n", pagesize, ppdsize);
      status = 1;
    }
    else
      puts("PASS");

    job = ippNew();
    ippAddString(job, IPP_TAG_JOB, IPP_TAG_KEYWORD, "media", NULL, ppdsize);

    printf("_ppdCacheGetPageSize(media=%s): ", ppdsize);
    fflush(stdout);

    if ((pagesize = _ppdCacheGetPageSize(pc, job, NULL, NULL)) == NULL)
    {
      puts("FAIL (Not Found)");
      status = 1;
    }
    else if (_cups_strcasecmp(pagesize, ppdsize))
    {
      printf("FAIL (Got \"%s\", Expected \"%s\")\n", pagesize, ppdsize);
      status = 1;
    }
    else
      puts("PASS");

    ippDelete(job);
  }

  return (status);
}


/*
 * 'test_ppd_cache()' - Test the PPD cache functions.
 */

static int				/* O - 1 on failure, 0 on success */
test_ppd_cache(_ppd_cache_t *pc,	/* I - PWG mapping data */
               ppd_file_t   *ppd)	/* I - PPD file */
{
  int		i,			/* Looping var */
		status = 0;		/* Return status */
  _ppd_cache_t	*pc2;			/* Loaded data */
  pwg_size_t	*size,			/* Size from original */
		*size2;			/* Size from saved */
  pwg_map_t	*map,			/* Map from original */
		*map2;			/* Map from saved */


 /*
  * Verify that we can write and read back the same data...
  */

  fputs("_ppdCacheWriteFile(test.pwg): ", stdout);
  if (!_ppdCacheWriteFile(pc, "test.pwg", NULL))
  {
    puts("FAIL");
    status ++;
  }
  else
    puts("PASS");

  fputs("_ppdCacheCreateWithFile(test.pwg): ", stdout);
  if ((pc2 = _ppdCacheCreateWithFile("test.pwg", NULL)) == NULL)
  {
    puts("FAIL");
    status ++;
  }
  else
  {
    // TODO: FINISH ADDING ALL VALUES IN STRUCTURE
    if (pc2->num_sizes != pc->num_sizes)
    {
      if (!status)
        puts("FAIL");

      printf("    SAVED num_sizes=%d, ORIG num_sizes=%d\n", pc2->num_sizes,
             pc->num_sizes);

      status ++;
    }
    else
    {
      for (i = pc->num_sizes, size = pc->sizes, size2 = pc2->sizes;
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

      for (i = pc->num_sources, map = pc->sources, map2 = pc2->sources;
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

      for (i = pc->num_types, map = pc->types, map2 = pc2->types;
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

    _ppdCacheDestroy(pc2);
  }

 /*
  * Test PageSize mapping code...
  */

  status += test_pagesize(pc, ppd, "Letter");
  status += test_pagesize(pc, ppd, "na-letter");
  status += test_pagesize(pc, ppd, "A4");
  status += test_pagesize(pc, ppd, "iso-a4");

  return (status);
}


/*
 * End of "$Id: testpwg.c 11240 2013-08-14 20:33:55Z msweet $".
 */
