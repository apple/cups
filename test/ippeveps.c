/*
 * Generic Adobe PostScript printer command for ippeveprinter/CUPS.
 *
 * Copyright © 2019 by Apple Inc.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include "ippevecommon.h"
#if !CUPS_LITE
#  include <cups/ppd-private.h>
#endif /* !CUPS_LITE */

#ifdef __APPLE__
#  define PDFTOPS CUPS_SERVERBIN "/filter/cgpdftops"
#else
#  define PDFTOPS CUPS_SERVERBIN "/filter/pdftops"
#endif /* __APPLE__ */


/*
 * Local globals...
 */

#if !CUPS_LITE
static ppd_file_t	*ppd = NULL;	/* PPD file data */
static _ppd_cache_t	*ppd_cache = NULL;
					/* IPP to PPD cache data */
#endif /* !CUPS_LITE */


/*
 * Local functions...
 */

//static void	ascii85(const unsigned char *data, int length, int eod);
static void	dsc_header(int num_options, cups_option_t *options, int num_pages);
static void	dsc_page(int page);
static void	dsc_trailer(int num_pages);
static int	get_options(cups_option_t **options);
static int	jpeg_to_ps(const char *filename, int copies, int num_options, cups_option_t *options);
static int	pdf_to_ps(const char *filename, int copies, int num_options, cups_option_t *options);
static int	ps_to_ps(const char *filename, int copies, int num_options, cups_option_t *options);
static int	raster_to_ps(const char *filename, int num_options, cups_option_t *options);


/*
 * 'main()' - Main entry for PostScript printer command.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  const char	*content_type,		/* Content type to print */
		*ipp_copies;		/* IPP_COPIES value */
  int		copies;			/* Number of copies */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */


 /*
  * Get print options...
  */

  num_options = get_options(&options);
  if ((ipp_copies = getenv("IPP_COPIES")) != NULL)
    copies = atoi(ipp_copies);
  else
    copies = 1;

 /*
  * Print it...
  */

  if (argc > 2)
  {
    fputs("ERROR: Too many arguments supplied, aborting.\n", stderr);
    return (1);
  }
  else if ((content_type = getenv("CONTENT_TYPE")) == NULL)
  {
    fputs("ERROR: CONTENT_TYPE environment variable not set, aborting.\n", stderr);
    return (1);
  }
  else if (!strcasecmp(content_type, "application/pdf"))
  {
    return (pdf_to_ps(argv[1], copies, num_options, options));
  }
  else if (!strcasecmp(content_type, "application/postscript"))
  {
    return (ps_to_ps(argv[1], copies, num_options, options));
  }
  else if (!strcasecmp(content_type, "image/jpeg"))
  {
    return (jpeg_to_ps(argv[1], copies, num_options, options));
  }
  else if (!strcasecmp(content_type, "image/pwg-raster") || !strcasecmp(content_type, "image/urf"))
  {
    return (raster_to_ps(argv[1], num_options, options));
  }
  else
  {
    fprintf(stderr, "ERROR: CONTENT_TYPE %s not supported.\n", content_type);
    return (1);
  }
}


#if 0
/*
 * 'ascii85()' - Write binary data using a Base85 encoding...
 */

static void
ascii85(const unsigned char *data,	/* I - Data to write */
        int                 length,	/* I - Number of bytes to write */
        int                 eod)	/* I - 1 if this is the end, 0 otherwise */
{
  unsigned	b = 0;			/* Current 32-bit word */
  unsigned char	c[5];			/* Base-85 encoded characters */
  static int	col = 0;		/* Column */
  static unsigned char leftdata[4];	/* Leftover data at the end */
  static int	leftcount = 0;		/* Size of leftover data */


  length += leftcount;

  while (length > 3)
  {
    switch (leftcount)
    {
      case 0 :
          b = (unsigned)((((((data[0] << 8) | data[1]) << 8) | data[2]) << 8) | data[3]);
	  break;
      case 1 :
          b = (unsigned)((((((leftdata[0] << 8) | data[0]) << 8) | data[1]) << 8) | data[2]);
	  break;
      case 2 :
          b = (unsigned)((((((leftdata[0] << 8) | leftdata[1]) << 8) | data[0]) << 8) | data[1]);
	  break;
      case 3 :
          b = (unsigned)((((((leftdata[0] << 8) | leftdata[1]) << 8) | leftdata[2]) << 8) | data[0]);
	  break;
    }

    if (col >= 76)
    {
      col = 0;
      putchar('\n');
    }

    if (b == 0)
    {
      putchar('z');
      col ++;
    }
    else
    {
      c[4] = (b % 85) + '!';
      b /= 85;
      c[3] = (b % 85) + '!';
      b /= 85;
      c[2] = (b % 85) + '!';
      b /= 85;
      c[1] = (b % 85) + '!';
      b /= 85;
      c[0] = (unsigned char)(b + '!');

      fwrite(c, 1, 5, stdout);
      col += 5;
    }

    data      += 4 - leftcount;
    length    -= 4 - leftcount;
    leftcount = 0;
  }

  if (length > 0)
  {
    // Copy any remainder into the leftdata array...
    if ((length - leftcount) > 0)
      memcpy(leftdata + leftcount, data, (size_t)(length - leftcount));

    memset(leftdata + length, 0, (size_t)(4 - length));

    leftcount = length;
  }

  if (eod)
  {
    // Do the end-of-data dance...
    if (col >= 76)
    {
      col = 0;
      putchar('\n');
    }

    if (leftcount > 0)
    {
      // Write the remaining bytes as needed...
      b = (unsigned)((((((leftdata[0] << 8) | leftdata[1]) << 8) | leftdata[2]) << 8) | leftdata[3]);

      c[4] = (b % 85) + '!';
      b /= 85;
      c[3] = (b % 85) + '!';
      b /= 85;
      c[2] = (b % 85) + '!';
      b /= 85;
      c[1] = (b % 85) + '!';
      b /= 85;
      c[0] = (unsigned char)(b + '!');

      fwrite(c, (size_t)(leftcount + 1), 1, stdout);

      leftcount = 0;
    }

    puts("~>");
    col = 0;
  }
}
#endif // 0


/*
 * 'dsc_header()' - Write out a standard Document Structuring Conventions
 *                  PostScript header.
 */

static void
dsc_header(int           num_options,	/* I - Number of options */
           cups_option_t *options,	/* I - Options */
           int           num_pages)	/* I - Number of pages in output */
{
  const char	*job_name = getenv("IPP_JOB_NAME");
					/* job-name value */


#if !CUPS_LITE
  const char	*job_id = getenv("IPP_JOB_ID");
  					/* job-id value */

  ppdEmitJCL(ppd, stdout, job_id ? atoi(job_id) : 0, cupsUser(), job_name);
#endif /* !CUPS_LITE */

  puts("%!PS-Adobe-3.0");
  puts("%%LanguageLevel: 2");
  printf("%%%%Creator: ippeveps/%d.%d.%d\n", CUPS_VERSION_MAJOR, CUPS_VERSION_MINOR, CUPS_VERSION_PATCH);
  if ((job_name = getenv("IPP_JOB_NAME")) != NULL)
  {
    fputs("%%Title: ", stdout);
    while (*job_name)
    {
      if (*job_name >= 0x20 && *job_name < 0x7f)
        putchar(*job_name);
      else
        putchar('?');

      job_name ++;
    }
    putchar('\n');
  }
  if (num_pages > 0)
    printf("%%%%Pages: %d\n", num_pages);
  else
    puts("%%Pages: (atend)");
  puts("%%EndComments");

#if !CUPS_LITE
  if (ppd)
  {
    puts("%%BeginProlog");
    if (ppd->patches)
    {
      puts("%%BeginFeature: *JobPatchFile 1");
      puts(ppd->patches);
      puts("%%EndFeature");
    }
    ppdEmit(ppd, stdout, PPD_ORDER_PROLOG);
    puts("%%EndProlog");

    puts("%%BeginSetup");
    ppdEmit(ppd, stdout, PPD_ORDER_DOCUMENT);
    ppdEmit(ppd, stdout, PPD_ORDER_ANY);
    puts("%%EndSetup");
  }
#endif /* !CUPS_LITE */
}


/*
 * 'dsc_page()' - Mark the start of a page.
 */

static void
dsc_page(int page)			/* I - Page numebr (1-based) */
{
  printf("%%%%Page: (%d) %d\n", page, page);

#if !CUPS_LITE
  if (ppd)
  {
    puts("%%BeginPageSetup");
    ppdEmit(ppd, stdout, PPD_ORDER_PAGE);
    puts("%%EndPageSetup");
  }
#endif /* !CUPS_LITE */
}


/*
 * 'dsc_trailer()' - Mark the end of the document.
 */

static void
dsc_trailer(int num_pages)		/* I - Number of pages */
{
  if (num_pages > 0)
  {
    puts("%%Trailer");
    printf("%%%%Pages: %d\n", num_pages);
    puts("%%EOF");
  }

#if !CUPS_LITE
  if (ppd && ppd->jcl_end)
    ppdEmitJCLEnd(ppd, stdout);
  else
#endif /* !CUPS_LITE */
    putchar(0x04);
}


/*
 * 'get_options()' - Get the PPD options corresponding to the IPP Job Template
 *                   attributes.
 */

static int				/* O - Number of options */
get_options(cups_option_t **options)	/* O - Options */
{
  int		num_options = 0;	/* Number of options */
  const char	*value;			/* Option value */
  pwg_media_t	*media = NULL;		/* Media mapping */
  int		num_media_col = 0;	/* Number of media-col values */
  cups_option_t	*media_col = NULL;	/* media-col values */
#if !CUPS_LITE
  const char	*choice;		/* PPD choice */
#endif /* !CUPS_LITE */


 /*
  * No options to start...
  */

  *options = NULL;

 /*
  * Copies...
  */

  if ((value = getenv("IPP_COPIES")) == NULL)
    value = getenv("IPP_COPIES_DEFAULT");
  if (value)
    num_options = cupsAddOption("copies", value, num_options, options);

 /*
  * Media...
  */

  if ((value = getenv("IPP_MEDIA")) == NULL)
    if ((value = getenv("IPP_MEDIA_COL")) == NULL)
      if ((value = getenv("IPP_MEDIA_DEFAULT")) == NULL)
        value = getenv("IPP_MEDIA_COL_DEFAULT");

  if (value)
  {
    if (*value == '{')
    {
     /*
      * media-col value...
      */

      num_media_col = cupsParseOptions(value, 0, &media_col);
    }
    else
    {
     /*
      * media value - map to media-col.media-size-name...
      */

      num_media_col = cupsAddOption("media-size-name", value, 0, &media_col);
    }
  }

  if ((value = cupsGetOption("media-size-name", num_media_col, media_col)) != NULL)
  {
    media = pwgMediaForPWG(value);
  }
  else if ((value = cupsGetOption("media-size", num_media_col, media_col)) != NULL)
  {
    int		num_media_size;		/* Number of media-size values */
    cups_option_t *media_size;		/* media-size values */
    const char	*x_dimension,		/* x-dimension value */
		*y_dimension;		/* y-dimension value */

    num_media_size = cupsParseOptions(value, 0, &media_size);

    if ((x_dimension = cupsGetOption("x-dimension", num_media_size, media_size)) != NULL && (y_dimension = cupsGetOption("y-dimension", num_media_size, media_size)) != NULL)
      media = pwgMediaForSize(atoi(x_dimension), atoi(y_dimension));

    cupsFreeOptions(num_media_size, media_size);
  }

  if (media)
    num_options = cupsAddOption("PageSize", media->ppd, num_options, options);

#if !CUPS_LITE
 /*
  * Load PPD file and the corresponding IPP <-> PPD cache data...
  */

  if ((ppd = ppdOpenFile(getenv("PPD"))) != NULL)
  {
    ppd_cache = _ppdCacheCreateWithPPD(ppd);

    if ((value = getenv("IPP_FINISHINGS")) == NULL)
      value = getenv("IPP_FINISHINGS_DEFAULT");

    if (value)
    {
      char	*ptr;			/* Pointer into value */
      int	fin;			/* Current value */

      for (fin = strtol(value, &ptr, 10); fin > 0; fin = strtol(ptr + 1, &ptr, 10))
      {
	num_options = _ppdCacheGetFinishingOptions(ppd_cache, NULL, (ipp_finishings_t)fin, num_options, options);

	if (*ptr != ',')
	  break;
      }
    }

    if ((value = cupsGetOption("media-source", num_media_col, media_col)) != NULL)
    {
      if ((choice = _ppdCacheGetInputSlot(ppd_cache, NULL, value)) != NULL)
	num_options = cupsAddOption("InputSlot", choice, num_options, options);
    }

    if ((value = cupsGetOption("media-type", num_media_col, media_col)) != NULL)
    {
      if ((choice = _ppdCacheGetMediaType(ppd_cache, NULL, value)) != NULL)
	num_options = cupsAddOption("MediaType", choice, num_options, options);
    }

    if ((value = getenv("IPP_OUTPUT_BIN")) == NULL)
      value = getenv("IPP_OUTPUT_BIN_DEFAULT");

    if (value)
    {
      if ((choice = _ppdCacheGetOutputBin(ppd_cache, value)) != NULL)
	num_options = cupsAddOption("OutputBin", choice, num_options, options);
    }

    if ((value = getenv("IPP_SIDES")) == NULL)
      value = getenv("IPP_SIDES_DEFAULT");

    if (value && ppd_cache->sides_option)
    {
      if (!strcmp(value, "one-sided") && ppd_cache->sides_1sided)
	num_options = cupsAddOption(ppd_cache->sides_option, ppd_cache->sides_1sided, num_options, options);
      else if (!strcmp(value, "two-sided-long-edge") && ppd_cache->sides_2sided_long)
	num_options = cupsAddOption(ppd_cache->sides_option, ppd_cache->sides_2sided_long, num_options, options);
      else if (!strcmp(value, "two-sided-short-edge") && ppd_cache->sides_2sided_short)
	num_options = cupsAddOption(ppd_cache->sides_option, ppd_cache->sides_2sided_short, num_options, options);
    }

    if ((value = getenv("IPP_PRINT_QUALITY")) == NULL)
      value = getenv("IPP_PRINT_QUALITY_DEFAULT");

    if (value)
    {
      int		i;		/* Looping var */
      int		pq;		/* Print quality (0-2) */
      int		pcm = 1;	/* Print color model (0 = mono, 1 = color) */
      int		num_presets;	/* Number of presets */
      cups_option_t	*presets;	/* Presets */

      if ((pq = atoi(value) - 3) < 0)
	pq = 0;
      else if (pq > 2)
	pq = 2;

      if ((value = getenv("IPP_PRINT_COLOR_MODE")) == NULL)
	value = getenv("IPP_PRINT_COLOR_MODE_DEFAULT");

      if (value && !strcmp(value, "monochrome"))
	pcm = 0;

      num_presets = ppd_cache->num_presets[pcm][pq];
      presets     = ppd_cache->presets[pcm][pq];

      for (i = 0; i < num_presets; i ++)
	num_options = cupsAddOption(presets[i].name, presets[i].value, num_options, options);
    }

   /*
    * Mark the PPD with the options...
    */

    ppdMarkDefaults(ppd);
    cupsMarkOptions(ppd, num_options, *options);
  }
#endif /* !CUPS_LITE */

  cupsFreeOptions(num_media_col, media_col);

  return (num_options);
}


/*
 * 'jpeg_to_ps()' - Convert a JPEG file to PostScript.
 */

static int				/* O - Exit status */
jpeg_to_ps(const char    *filename,	/* I - Filename */
           int           copies,	/* I - Number of copies */
           int           num_options,	/* I - Number of options */
           cups_option_t *options)	/* I - options */
{
  (void)filename;

  dsc_header(num_options, options, copies);

  dsc_page(1);

  dsc_trailer(0);

  return (0);
}


/*
 * 'pdf_to_ps()' - Convert a PDF file to PostScript.
 */

static int				/* O - Exit status */
pdf_to_ps(const char    *filename,	/* I - Filename */
	  int           copies,		/* I - Number of copies */
	  int           num_options,	/* I - Number of options */
	  cups_option_t *options)	/* I - options */
{
  int		status;			/* Exit status */
  char		tempfile[1024];		/* Temporary file */
  int		tempfd;			/* Temporary file descriptor */
  int		pid;			/* Process ID */
  const char	*pdf_argv[8];		/* Command-line arguments */
  char		pdf_options[1024];	/* Options */
  const char	*value;			/* Option value */


 /*
  * Create a temporary file for the PostScript version...
  */

  if ((tempfd = cupsTempFd(tempfile, sizeof(tempfile))) < 0)
  {
    fprintf(stderr, "ERROR: Unable to create temporary file: %s\n", strerror(errno));
    return (1);
  }

 /*
  * Run cgpdftops or pdftops in the filter directory...
  */

  if ((value = cupsGetOption("PageSize", num_options, options)) != NULL)
    snprintf(pdf_options, sizeof(pdf_options), "PageSize=%s", value);
  else
    pdf_options[0] = '\0';

  pdf_argv[0] = "printer";
  pdf_argv[1] = getenv("IPP_JOB_ID");
  pdf_argv[2] = cupsUser();
  pdf_argv[3] = getenv("IPP_JOB_NAME");
  pdf_argv[4] = "1";
  pdf_argv[5] = pdf_options;
  pdf_argv[6] = filename;
  pdf_argv[7] = NULL;

  if ((pid = fork()) == 0)
  {
   /*
    * Child comes here...
    */

    close(1);
    dup2(tempfd, 1);
    close(tempfd);

    execv(PDFTOPS, (char * const *)pdf_argv);
    exit(errno);
  }
  else if (pid < 0)
  {
   /*
    * Unable to fork process...
    */

    perror("ERROR: Unable to start PDF filter");

    close(tempfd);
    unlink(tempfile);

    return (1);
  }
  else
  {
   /*
    * Wait for the filter to complete...
    */

    close(tempfd);

#  ifdef HAVE_WAITPID
    while (waitpid(pid, &status, 0) < 0);
#  else
    while (wait(&status) < 0);
#  endif /* HAVE_WAITPID */

    if (status)
    {
      if (WIFEXITED(status))
	fprintf(stderr, "ERROR: " PDFTOPS " exited with status %d.\n", WEXITSTATUS(status));
      else
	fprintf(stderr, "ERROR: " PDFTOPS " terminated with signal %d.\n", WTERMSIG(status));

      unlink(tempfile);
      return (1);
    }
  }

 /*
  * Copy the PostScript output from the command...
  */

  status = ps_to_ps(tempfile, copies, num_options, options);

  unlink(tempfile);

  return (status);
}


/*
 * 'ps_to_ps()' - Copy PostScript to the standard output.
 */

static int				/* O - Exit status */
ps_to_ps(const char    *filename,	/* I - Filename */
	 int           copies,		/* I - Number of copies */
	 int           num_options,	/* I - Number of options */
	 cups_option_t *options)	/* I - options */
{
  (void)filename;

  dsc_header(num_options, options, 0);

  dsc_page(1);

  dsc_trailer(0);

  return (0);
}


/*
 * 'raster_to_ps()' - Convert PWG Raster/Apple Raster to PostScript.
 */

static int				/* O - Exit status */
raster_to_ps(const char    *filename,	/* I - Filename */
	     int           num_options,	/* I - Number of options */
	     cups_option_t *options)	/* I - options */
{
  (void)filename;

  dsc_header(num_options, options, 0);

  dsc_page(1);

  dsc_trailer(0);

  return (0);
}


