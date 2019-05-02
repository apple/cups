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
#include <limits.h>
#include <sys/wait.h>

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

static void	ascii85(const unsigned char *data, int length, int eod);
static void	dsc_header(int num_pages);
static void	dsc_page(int page);
static void	dsc_trailer(int num_pages);
static int	get_options(cups_option_t **options);
static int	jpeg_to_ps(const char *filename, int copies);
static int	pdf_to_ps(const char *filename, int copies, int num_options, cups_option_t *options);
static int	ps_to_ps(const char *filename, int copies);
static int	raster_to_ps(const char *filename);


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
    return (ps_to_ps(argv[1], copies));
  }
  else if (!strcasecmp(content_type, "image/jpeg"))
  {
    return (jpeg_to_ps(argv[1], copies));
  }
  else if (!strcasecmp(content_type, "image/pwg-raster") || !strcasecmp(content_type, "image/urf"))
  {
    return (raster_to_ps(argv[1]));
  }
  else
  {
    fprintf(stderr, "ERROR: CONTENT_TYPE %s not supported.\n", content_type);
    return (1);
  }
}


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


/*
 * 'dsc_header()' - Write out a standard Document Structuring Conventions
 *                  PostScript header.
 */

static void
dsc_header(int num_pages)		/* I - Number of pages or 0 if not known */
{
  const char	*job_name = getenv("IPP_JOB_NAME");
					/* job-name value */


#if !CUPS_LITE
  const char	*job_id = getenv("IPP_JOB_ID");
					/* job-id value */

  ppdEmitJCL(ppd, stdout, job_id ? atoi(job_id) : 0, cupsUser(), job_name ? job_name : "Unknown");
#endif /* !CUPS_LITE */

  puts("%!PS-Adobe-3.0");
  puts("%%LanguageLevel: 2");
  printf("%%%%Creator: ippeveps/%d.%d.%d\n", CUPS_VERSION_MAJOR, CUPS_VERSION_MINOR, CUPS_VERSION_PATCH);
  if (job_name)
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

  fprintf(stderr, "ATTR: job-impressions-completed=%d\n", page);

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

    /* TODO: Fix me - values are names, not numbers... Also need to support finishings-col */
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

      if (!strcmp(value, "draft"))
        pq = 0;
      else if (!strcmp(value, "high"))
        pq = 2;
      else
        pq = 1;

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
           int           copies)	/* I - Number of copies */
{
  int		fd;			/* JPEG file descriptor */
  int		copy;			/* Current copy */
  int		width = 0,		/* Width */
		height = 0,		/* Height */
		depth = 0,		/* Number of colors */
		length;			/* Length of marker */
  unsigned char	buffer[65536],		/* Copy buffer */
		*bufptr,		/* Pointer info buffer */
		*bufend;		/* End of buffer */
  ssize_t	bytes;			/* Bytes in buffer */
  const char	*decode;		/* Decode array */
  float		page_left,		/* Left margin */
		page_top,		/* Top margin */
		page_width,		/* Page width in points */
		page_height,		/* Page heigth in points */
		x_factor,		/* X image scaling factor */
		y_factor,		/* Y image scaling factor */
		page_scaling;		/* Image scaling factor */
#if !CUPS_LITE
  ppd_size_t	*page_size;		/* Current page size */
#endif /* !CUPS_LITE */


 /*
  * Open the input file...
  */

  if (filename)
  {
    if ((fd = open(filename, O_RDONLY)) < 0)
    {
      fprintf(stderr, "ERROR: Unable to open \"%s\": %s\n", filename, strerror(errno));
      return (1);
    }
  }
  else
  {
    fd     = 0;
    copies = 1;
  }

 /*
  * Read the JPEG dimensions...
  */

  bytes = read(fd, buffer, sizeof(buffer));

  if (bytes < 3 || memcmp(buffer, "\377\330\377", 3))
  {
    fputs("ERROR: Not a JPEG image.\n", stderr);

    if (fd > 0)
      close(fd);

    return (1);
  }

  for (bufptr = buffer + 2, bufend = buffer + bytes; bufptr < bufend;)
  {
   /*
    * Scan the file for a SOFn marker, then we can get the dimensions...
    */

    if (*bufptr == 0xff)
    {
      bufptr ++;

      if (bufptr >= bufend)
      {
       /*
	* If we are at the end of the current buffer, re-fill and continue...
	*/

	if ((bytes = read(fd, buffer, sizeof(buffer))) <= 0)
	  break;

	bufptr = buffer;
	bufend = buffer + bytes;
      }

      if (*bufptr == 0xff)
	continue;

      if ((bufptr + 16) >= bufend)
      {
       /*
	* Read more of the marker...
	*/

	bytes = bufend - bufptr;

	memmove(buffer, bufptr, (size_t)bytes);
	bufptr = buffer;
	bufend = buffer + bytes;

	if ((bytes = read(fd, bufend, sizeof(buffer) - (size_t)bytes)) <= 0)
	  break;

	bufend += bytes;
      }

      length = (size_t)((bufptr[1] << 8) | bufptr[2]);

      if ((*bufptr >= 0xc0 && *bufptr <= 0xc3) || (*bufptr >= 0xc5 && *bufptr <= 0xc7) || (*bufptr >= 0xc9 && *bufptr <= 0xcb) || (*bufptr >= 0xcd && *bufptr <= 0xcf))
      {
       /*
	* SOFn marker, look for dimensions...
	*/

	width  = (bufptr[6] << 8) | bufptr[7];
	height = (bufptr[4] << 8) | bufptr[5];
	depth  = bufptr[8];
	break;
      }

     /*
      * Skip past this marker...
      */

      bufptr ++;
      bytes = bufend - bufptr;

      while (length >= bytes)
      {
	length -= bytes;

	if ((bytes = read(fd, buffer, sizeof(buffer))) <= 0)
	  break;

	bufptr = buffer;
	bufend = buffer + bytes;
      }

      if (length > bytes)
	break;

      bufptr += length;
    }
  }

  fprintf(stderr, "DEBUG: JPEG dimensions are %dx%dx%d\n", width, height, depth);

  if (width <= 0 || height <= 0 || depth <= 0)
  {
    fputs("ERROR: No valid image data in JPEG file.\n", stderr);

    if (fd > 0)
      close(fd);

    return (1);
  }

  fputs("ATTR: job-impressions=1\n", stderr);

 /*
  * Figure out the dimensions/scaling of the final image...
  */

#if CUPS_LITE
  page_left   = 18.0f;
  page_top    = 756.0f;
  page_width  = 576.0f;
  page_height = 720.0f;

#else
  if ((page_size = ppdPageSize(ppd, NULL)) != NULL)
  {
    page_left   = page_size->left;
    page_top    = page_size->top;
    page_width  = page_size->right - page_left;
    page_height = page_top - page_size->bottom;
  }
  else
  {
    page_left   = 18.0f;
    page_top    = 756.0f;
    page_width  = 576.0f;
    page_height = 720.0f;
  }
#endif /* CUPS_LITE */

  fprintf(stderr, "DEBUG: page_left=%.2f, page_top=%.2f, page_width=%.2f, page_height=%.2f\n", page_left, page_top, page_width, page_height);

  /* TODO: Support orientation/rotation, different print-scaling modes */
  x_factor = page_width / width;
  y_factor = page_height / height;

  if (x_factor > y_factor && (height * x_factor) <= page_height)
    page_scaling = x_factor;
  else
    page_scaling = y_factor;

  fprintf(stderr, "DEBUG: Scaled dimensions are %.2fx%.2f\n", width * page_scaling, height * page_scaling);

 /*
  * Write pages...
  */

  dsc_header(copies);

  for (copy = 1; copy <= copies; copy ++)
  {
    dsc_page(copy);

    if (depth == 1)
    {
      puts("/DeviceGray setcolorspace");
      decode = "0 1";
    }
    else if (depth == 3)
    {
      puts("/DeviceRGB setcolorspace");
      decode = "0 1 0 1 0 1";
    }
    else
    {
      puts("/DeviceCMYK setcolorspace");
      decode = "0 1 0 1 0 1 0 1";
    }

    printf("gsave %.3f %.3f translate %.3f %.3f scale\n", page_left + 0.5f * (page_width - width * page_scaling), page_top - 0.5f * (page_height - height * page_scaling), page_scaling, page_scaling);
    printf("<</ImageType 1/Width %d/Height %d/BitsPerComponent 8/ImageMatrix[1 0 0 -1 0 1]/Decode[%s]/DataSource currentfile/ASCII85Decode filter/DCTDecode filter/Interpolate true>>image\n", width, height, decode);

    if (fd > 0)
      lseek(fd, 0, SEEK_SET);

    while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
      ascii85(buffer, (int)bytes, 0);

    ascii85(buffer, 0, 1);

    puts("grestore showpage");
  }

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
  const char	*job_id,		/* job-id value */
		*job_name;		/* job-name value */


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

  if ((job_id = getenv("IPP_JOB_ID")) == NULL)
    job_id = "42";
  if ((job_name = getenv("IPP_JOB_NAME")) == NULL)
    job_name = "untitled";

  pdf_argv[0] = "printer";
  pdf_argv[1] = job_id;
  pdf_argv[2] = cupsUser();
  pdf_argv[3] = job_name;
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

  status = ps_to_ps(tempfile, copies);

  unlink(tempfile);

  return (status);
}


/*
 * 'ps_to_ps()' - Copy PostScript to the standard output.
 */

static int				/* O - Exit status */
ps_to_ps(const char    *filename,	/* I - Filename */
	 int           copies)		/* I - Number of copies */
{
  FILE		*fp;				/* File to read from */
  int		copy,				/* Current copy */
		page,				/* Current page number */
		num_pages = 0,			/* Total number of pages */
		first_page,			/* First page */
		last_page;			/* Last page */
  const char	*page_ranges;			/* page-ranges option */
  long		first_pos = -1;			/* Offset for first page */
  char		line[1024];			/* Line from file */


 /*
  * Open the print file...
  */

  if (filename)
  {
    if ((fp = fopen(filename, "rb")) == NULL)
    {
      fprintf(stderr, "ERROR: Unable to open \"%s\": %s\n", filename, strerror(errno));
      return (1);
    }
  }
  else
  {
    copies = 1;
    fp     = stdin;
  }

 /*
  * Check page ranges...
  */

  if ((page_ranges = getenv("IPP_PAGE_RANGES")) != NULL)
  {
    if (sscanf(page_ranges, "%d-%d", &first_page, &last_page) != 2)
    {
      first_page = 1;
      last_page  = INT_MAX;
    }
  }
  else
  {
    first_page = 1;
    last_page  = INT_MAX;
  }

 /*
  * Write the PostScript header for the document...
  */

  dsc_header(0);

  first_pos = 0;

  while (fgets(line, sizeof(line), fp))
  {
    if (!strncmp(line, "%%Page:", 7))
      break;

    first_pos = ftell(fp);

    if (line[0] != '%')
      fputs(line, stdout);
  }

  if (!strncmp(line, "%%Page:", 7))
  {
    for (copy = 0; copy < copies; copy ++)
    {
      int copy_page = 0;		/* Do we copy the page data? */

      if (fp != stdin)
        fseek(fp, first_pos, SEEK_SET);

      page = 0;
      while (fgets(line, sizeof(line), fp))
      {
        if (!strncmp(line, "%%Page:", 7))
        {
          page ++;
          copy_page = page >= first_page && page <= last_page;

          if (copy_page)
          {
	    num_pages ++;
	    dsc_page(num_pages);
	  }
	}
	else if (copy_page)
	  fputs(line, stdout);
      }
    }
  }

  dsc_trailer(num_pages);

  fprintf(stderr, "ATTR: job-impressions=%d\n", num_pages / copies);

  if (fp != stdin)
    fclose(fp);

  return (0);
}


/*
 * 'raster_to_ps()' - Convert PWG Raster/Apple Raster to PostScript.
 *
 * The current implementation locally-decodes the raster data and then writes
 * whole, non-blank lines as 1-line high images with base-85 encoding, resulting
 * in between 10 and 20 times larger output.  A alternate implementation (if it
 * is deemed necessary) would be to implement a PostScript decode procedure that
 * handles the modified packbits decompression so that we just have the base-85
 * encoding overhead (25%).  Furthermore, Level 3 PostScript printers also
 * support Flate compression.
 *
 * That said, the most efficient path with the highest quality is for Clients
 * to supply PDF files and us to use the existing PDF to PostScript conversion
 * filters.
 */

static int				/* O - Exit status */
raster_to_ps(const char *filename)	/* I - Filename */
{
  int			fd;		/* Input file */
  cups_raster_t		*ras;		/* Raster stream */
  cups_page_header2_t	header;		/* Page header */
  int			page = 0;	/* Current page */
  unsigned		y;		/* Current line */
  unsigned char		*line;		/* Line buffer */
  unsigned char		white;		/* White color */
  const char		*decode;	/* Image decode array */


 /*
  * Open the input file...
  */

  if (filename)
  {
    if ((fd = open(filename, O_RDONLY)) < 0)
    {
      fprintf(stderr, "ERROR: Unable to open \"%s\": %s\n", filename, strerror(errno));
      return (1);
    }
  }
  else
  {
    fd = 0;
  }

 /*
  * Open the raster stream and send pages...
  */

  if ((ras = cupsRasterOpen(fd, CUPS_RASTER_READ)) == NULL)
  {
    fputs("ERROR: Unable to read raster data, aborting.\n", stderr);
    return (1);
  }

  dsc_header(0);

  while (cupsRasterReadHeader2(ras, &header))
  {
    page ++;

    fprintf(stderr, "DEBUG: Page %d: %ux%ux%u\n", page, header.cupsWidth, header.cupsHeight, header.cupsBitsPerPixel);

    if (header.cupsColorSpace != CUPS_CSPACE_W && header.cupsColorSpace != CUPS_CSPACE_SW && header.cupsColorSpace != CUPS_CSPACE_K && header.cupsColorSpace != CUPS_CSPACE_RGB && header.cupsColorSpace != CUPS_CSPACE_SRGB)
    {
      fputs("ERROR: Unsupported color space, aborting.\n", stderr);
      break;
    }
    else if (header.cupsBitsPerColor != 1 && header.cupsBitsPerColor != 8)
    {
      fputs("ERROR: Unsupported bit depth, aborting.\n", stderr);
      break;
    }

    line = malloc(header.cupsBytesPerLine);

    dsc_page(page);

    puts("gsave");
    printf("%.6f %.6f scale\n", 72.0f / header.HWResolution[0], 72.0f / header.HWResolution[1]);

    switch (header.cupsColorSpace)
    {
      case CUPS_CSPACE_W :
      case CUPS_CSPACE_SW :
          decode = "0 1";
          puts("/DeviceGray setcolorspace");
          white = 255;
          break;

      case CUPS_CSPACE_K :
          decode = "0 1";
          puts("/DeviceGray setcolorspace");
          white = 0;
          break;

      default :
          decode = "0 1 0 1 0 1";
          puts("/DeviceRGB setcolorspace");
          white = 255;
          break;
    }

    printf("gsave /L{grestore gsave 0 exch translate <</ImageType 1/Width %u/Height 1/BitsPerComponent %u/ImageMatrix[1 0 0 -1 0 1]/DataSource currentfile/ASCII85Decode filter/Decode[%s]>>image}bind def\n", header.cupsWidth, header.cupsBitsPerColor, decode);

    for (y = header.cupsHeight; y > 0; y --)
    {
      if (cupsRasterReadPixels(ras, line, header.cupsBytesPerLine))
      {
        if (line[0] != white || memcmp(line, line + 1, header.cupsBytesPerLine - 1))
        {
          printf("%d L\n", y - 1);
          ascii85(line, (int)header.cupsBytesPerLine, 1);
        }
      }
      else
        break;
    }

    fprintf(stderr, "DEBUG: y=%d at end...\n", y);

    puts("grestore grestore");
    puts("showpage");

    free(line);
  }

  cupsRasterClose(ras);

  dsc_trailer(page);

  fprintf(stderr, "ATTR: job-impressions=%d\n", page);

  return (0);
}


