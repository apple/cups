/*
 * Simulated client test program for CUPS.
 *
 * Copyright © 2017-2019 by Apple Inc.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <cups/cups.h>
#include <cups/raster.h>
#include <cups/string-private.h>
#include <cups/thread-private.h>


/*
 * Constants...
 */

#define MAX_CLIENTS	100		/* Maximum number of client threads */


/*
 * Local types...
 */

typedef struct _client_data_s
{
  const char		*uri,		/* Printer URI */
			*hostname,	/* Hostname */
			*user,		/* Username */
			*resource;	/* Resource path */
  int			port;		/* Port number */
  http_encryption_t	encryption;	/* Use encryption? */
  const char		*docfile,	/* Document file */
			*docformat;	/* Document format */
  int			grayscale,	/* Force grayscale? */
			keepfile;	/* Keep temporary file? */
  ipp_pstate_t		printer_state;	/* Current printer state */
  char			printer_state_reasons[1024];
					/* Current printer-state-reasons */
  int			job_id; 	/* Job ID for submitted job */
  ipp_jstate_t		job_state;	/* Current job state */
  char			job_state_reasons[1024];
					/* Current job-state-reasons */
} _client_data_t;


/*
 * Local globals...
 */

static int		client_count = 0;
static _cups_mutex_t	client_mutex = _CUPS_MUTEX_INITIALIZER;
static int		verbosity = 0;


/*
 * Local functions...
 */

static const char	*make_raster_file(ipp_t *response, int grayscale, char *tempname, size_t tempsize, const char **format);
static void		*monitor_printer(_client_data_t *data);
static void		*run_client(_client_data_t *data);
static void		show_attributes(const char *title, int request, ipp_t *ipp);
static void		show_capabilities(ipp_t *response);
static void		usage(void);


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int			i;		/* Looping var */
  const char		*opt;		/* Current option */
  int			num_clients = 0,/* Number of clients to simulate */
			clients_started = 0;
					/* Number of clients that have been started */
  char			scheme[32],     /* URI scheme */
			userpass[256],  /* Username:password */
			hostname[256],  /* Hostname */
			resource[256];  /* Resource path */
  _client_data_t	data;		/* Client data */


 /*
  * Parse command-line options...
  */

  if (argc == 1)
    return (0);

  memset(&data, 0, sizeof(data));

  for (i = 1; i < argc; i ++)
  {
    if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
        switch (*opt)
        {
          case 'c' : /* -c num-clients */
              if (num_clients)
              {
                puts("Number of clients can only be specified once.");
                usage();
                return (1);
              }

              i ++;
              if (i >= argc)
              {
                puts("Expected client count after '-c'.");
                usage();
                return (1);
              }

              if ((num_clients = atoi(argv[i])) < 1)
              {
                puts("Number of clients must be one or more.");
                usage();
                return (1);
              }
              break;

          case 'd' : /* -d document-format */
              if (data.docformat)
              {
                puts("Document format can only be specified once.");
                usage();
                return (1);
              }

              i ++;
              if (i >= argc)
              {
                puts("Expected document format after '-d'.");
                usage();
                return (1);
              }

              data.docformat = argv[i];
              break;

          case 'f' : /* -f print-file */
              if (data.docfile)
              {
                puts("Print file can only be specified once.");
                usage();
                return (1);
              }

              i ++;
              if (i >= argc)
              {
                puts("Expected print file after '-f'.");
                usage();
                return (1);
              }

              data.docfile = argv[i];
              break;

          case 'g' :
              data.grayscale = 1;
              break;

          case 'k' :
              data.keepfile = 1;
              break;

          case 'v' :
              verbosity ++;
              break;

          default :
              printf("Unknown option '-%c'.\n", *opt);
              usage();
              return (1);
        }
      }
    }
    else if (data.uri || (strncmp(argv[i], "ipp://", 6) && strncmp(argv[i], "ipps://", 7)))
    {
      printf("Unknown command-line argument '%s'.\n", argv[i]);
      usage();
      return (1);
    }
    else
      data.uri = argv[i];
  }

 /*
  * Make sure we have everything we need.
  */

  if (!data.uri)
  {
    puts("Expected printer URI.");
    usage();
    return (1);
  }

  if (num_clients < 1)
    num_clients = 1;

 /*
  * Connect to the printer...
  */

  if (httpSeparateURI(HTTP_URI_CODING_ALL, data.uri, scheme, sizeof(scheme), userpass, sizeof(userpass), hostname, sizeof(hostname), &data.port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
  {
    printf("Bad printer URI '%s'.\n", data.uri);
    return (1);
  }

  if (!data.port)
    data.port = IPP_PORT;

  if (!strcmp(scheme, "https") || !strcmp(scheme, "ipps"))
    data.encryption = HTTP_ENCRYPTION_ALWAYS;
  else
    data.encryption = HTTP_ENCRYPTION_IF_REQUESTED;

 /*
  * Start the client threads...
  */

  data.hostname = hostname;
  data.resource = resource;

  while (clients_started < num_clients)
  {
    _cupsMutexLock(&client_mutex);
    if (client_count < MAX_CLIENTS)
    {
      _cups_thread_t	tid;		/* New thread */

      client_count ++;
      _cupsMutexUnlock(&client_mutex);
      tid = _cupsThreadCreate((_cups_thread_func_t)run_client, &data);
      _cupsThreadDetach(tid);
    }
    else
    {
      _cupsMutexUnlock(&client_mutex);
      sleep(1);
    }
  }

  while (client_count > 0)
  {
    _cupsMutexLock(&client_mutex);
    printf("%d RUNNING CLIENTS\n", client_count);
    _cupsMutexUnlock(&client_mutex);
    sleep(1);
  }

  return (0);
}


/*
 * 'make_raster_file()' - Create a temporary raster file.
 */

static const char *                     /* O - Print filename */
make_raster_file(ipp_t      *response,  /* I - Printer attributes */
                 int        grayscale,  /* I - Force grayscale? */
                 char       *tempname,  /* I - Temporary filename buffer */
                 size_t     tempsize,   /* I - Size of temp file buffer */
                 const char **format)   /* O - Print format */
{
  int                   i,              /* Looping var */
                        count;          /* Number of values */
  ipp_attribute_t       *attr;          /* Printer attribute */
  const char            *type = NULL;   /* Raster type (colorspace + bits) */
  pwg_media_t           *media = NULL;  /* Media size */
  int                   xdpi = 0,       /* Horizontal resolution */
                        ydpi = 0;       /* Vertical resolution */
  int                   fd;             /* Temporary file */
  cups_mode_t           mode;           /* Raster mode */
  cups_raster_t         *ras;           /* Raster stream */
  cups_page_header2_t   header;         /* Page header */
  unsigned char         *line,          /* Line of raster data */
                        *lineptr;       /* Pointer into line */
  unsigned              y,              /* Current position on page */
                        xcount, ycount, /* Current count for X and Y */
                        xrep, yrep,     /* Repeat count for X and Y */
                        xoff, yoff,     /* Offsets for X and Y */
                        yend;           /* End Y value */
  int                   temprow,        /* Row in template */
                        tempcolor;      /* Template color */
  const char            *template;      /* Pointer into template */
  const unsigned char   *color;         /* Current color */
  static const unsigned char colors[][3] =
  {                                     /* Colors for test */
    { 191, 191, 191 },
    { 127, 127, 127 },
    {  63,  63,  63 },
    {   0,   0,   0 },
    { 255,   0,   0 },
    { 255, 127,   0 },
    { 255, 255,   0 },
    { 127, 255,   0 },
    {   0, 255,   0 },
    {   0, 255, 127 },
    {   0, 255, 255 },
    {   0, 127, 255 },
    {   0,   0, 255 },
    { 127,   0, 255 },
    { 255,   0, 255 }
  };
  static const char * const templates[] =
  {                                     /* Raster template */
    " CCC   U   U  PPPP    SSS          TTTTT  EEEEE   SSS   TTTTT          000     1     222    333      4   55555   66    77777   888    999   ",
    "C   C  U   U  P   P  S   S           T    E      S   S    T           0   0   11    2   2  3   3  4  4   5      6          7  8   8  9   9  ",
    "C      U   U  P   P  S               T    E      S        T           0   0    1        2      3  4  4   5      6         7   8   8  9   9  ",
    "C      U   U  PPPP    SSS   -----    T    EEEE    SSS     T           0 0 0    1      22    333   44444   555   6666      7    888    9999  ",
    "C      U   U  P          S           T    E          S    T           0   0    1     2         3     4       5  6   6    7    8   8      9  ",
    "C   C  U   U  P      S   S           T    E      S   S    T           0   0    1    2      3   3     4   5   5  6   6    7    8   8      9  ",
    " CCC    UUU   P       SSS            T    EEEEE   SSS     T            000    111   22222   333      4    555    666     7     888     99   ",
    "                                                                                                                                            "
  };


 /*
  * Figure out the output format...
  */

  if ((attr = ippFindAttribute(response, "document-format-supported", IPP_TAG_MIMETYPE)) == NULL)
  {
    puts("No supported document formats, aborting.");
    return (NULL);
  }

  if (*format)
  {
    if (!ippContainsString(attr, *format))
    {
      printf("Printer does not support document-format '%s'.\n", *format);
      return (NULL);
    }

    if (!strcmp(*format, "image/urf"))
      mode = CUPS_RASTER_WRITE_APPLE;
    else if (!strcmp(*format, "image/pwg-raster"))
      mode = CUPS_RASTER_WRITE_PWG;
    else
    {
      printf("Unable to generate document-format '%s'.\n", *format);
      return (NULL);
    }
  }
  else if (ippContainsString(attr, "image/urf"))
  {
   /*
    * Apple Raster format...
    */

    *format = "image/urf";
    mode    = CUPS_RASTER_WRITE_APPLE;
  }
  else if (ippContainsString(attr, "image/pwg-raster"))
  {
   /*
    * PWG Raster format...
    */

    *format = "image/pwg-raster";
    mode    = CUPS_RASTER_WRITE_PWG;
  }
  else
  {
   /*
    * No supported raster format...
    */

    puts("Printer does not support Apple or PWG raster files, aborting.");
    return (NULL);
  }

 /*
  * Figure out the the media, resolution, and color mode...
  */

  if ((attr = ippFindAttribute(response, "media-ready", IPP_TAG_KEYWORD)) != NULL)
  {
   /*
    * Use ready media...
    */

    if (ippContainsString(attr, "na_letter_8.5x11in"))
      media = pwgMediaForPWG("na_letter_8.5x11in");
    else if (ippContainsString(attr, "iso_a4_210x297mm"))
      media = pwgMediaForPWG("iso_a4_210x297mm");
    else
      media = pwgMediaForPWG(ippGetString(attr, 0, NULL));
  }
  else if ((attr = ippFindAttribute(response, "media-default", IPP_TAG_KEYWORD)) != NULL)
  {
   /*
    * Use default media...
    */

    media = pwgMediaForPWG(ippGetString(attr, 0, NULL));
  }
  else
  {
    puts("No default or ready media reported by printer, aborting.");
    return (NULL);
  }

  if (mode == CUPS_RASTER_WRITE_APPLE && (attr = ippFindAttribute(response, "urf-supported", IPP_TAG_KEYWORD)) != NULL)
  {
    for (i = 0, count = ippGetCount(attr); i < count; i ++)
    {
      const char *val = ippGetString(attr, i, NULL);

      if (!strncmp(val, "RS", 2))
        xdpi = ydpi = atoi(val + 2);
      else if (!strncmp(val, "W8", 2) && !type)
        type = "sgray_8";
      else if (!strncmp(val, "SRGB24", 6) && !grayscale)
        type = "srgb_8";
    }
  }
  else if (mode == CUPS_RASTER_WRITE_PWG && (attr = ippFindAttribute(response, "pwg-raster-document-resolution-supported", IPP_TAG_RESOLUTION)) != NULL)
  {
    for (i = 0, count = ippGetCount(attr); i < count; i ++)
    {
      int tempxdpi, tempydpi;
      ipp_res_t tempunits;

      tempxdpi = ippGetResolution(attr, 0, &tempydpi, &tempunits);

      if (i == 0 || tempxdpi < xdpi || tempydpi < ydpi)
      {
        xdpi = tempxdpi;
        ydpi = tempydpi;
      }
    }

    if ((attr = ippFindAttribute(response, "pwg-raster-document-type-supported", IPP_TAG_KEYWORD)) != NULL)
    {
      if (!grayscale && ippContainsString(attr, "srgb_8"))
        type = "srgb_8";
      else if (ippContainsString(attr, "sgray_8"))
        type = "sgray_8";
    }
  }

  if (xdpi < 72 || ydpi < 72)
  {
    puts("No supported raster resolutions, aborting.");
    return (NULL);
  }

  if (!type)
  {
    puts("No supported color spaces or bit depths, aborting.");
    return (NULL);
  }

 /*
  * Make the raster context and details...
  */

  if (!cupsRasterInitPWGHeader(&header, media, type, xdpi, ydpi, "one-sided", NULL))
  {
    printf("Unable to initialize raster context: %s\n", cupsRasterErrorString());
    return (NULL);
  }

  header.cupsInteger[CUPS_RASTER_PWG_TotalPageCount] = 1;

  if (header.cupsWidth > (2 * header.HWResolution[0]))
  {
    xoff = header.HWResolution[0] / 2;
    yoff = header.HWResolution[1] / 2;
  }
  else
  {
    xoff = header.HWResolution[0] / 4;
    yoff = header.HWResolution[1] / 4;
  }

  xrep = (header.cupsWidth - 2 * xoff) / 140;
  yrep = xrep * header.HWResolution[1] / header.HWResolution[0];
  yend = header.cupsHeight - yoff;

 /*
  * Prepare the raster file...
  */

  if ((line = malloc(header.cupsBytesPerLine)) == NULL)
  {
    printf("Unable to allocate %u bytes for raster output: %s\n", header.cupsBytesPerLine, strerror(errno));
    return (NULL);
  }

  if ((fd = cupsTempFd(tempname, (int)tempsize)) < 0)
  {
    printf("Unable to create temporary print file: %s\n", strerror(errno));
    free(line);
    return (NULL);
  }

  if ((ras = cupsRasterOpen(fd, mode)) == NULL)
  {
    printf("Unable to open raster stream: %s\n", cupsRasterErrorString());
    close(fd);
    free(line);
    return (NULL);
  }

 /*
  * Write a single page consisting of the template dots repeated over the page.
  */

  cupsRasterWriteHeader2(ras, &header);

  memset(line, 0xff, header.cupsBytesPerLine);

  for (y = 0; y < yoff; y ++)
    cupsRasterWritePixels(ras, line, header.cupsBytesPerLine);

  for (temprow = 0, tempcolor = 0; y < yend;)
  {
    template = templates[temprow];
    color    = colors[tempcolor];

    temprow ++;
    if (temprow >= (int)(sizeof(templates) / sizeof(templates[0])))
    {
      temprow = 0;
      tempcolor ++;
      if (tempcolor >= (int)(sizeof(colors) / sizeof(colors[0])))
        tempcolor = 0;
      else if (tempcolor > 3 && header.cupsColorSpace == CUPS_CSPACE_SW)
        tempcolor = 0;
    }

    memset(line, 0xff, header.cupsBytesPerLine);

    if (header.cupsColorSpace == CUPS_CSPACE_SW)
    {
     /*
      * Do grayscale output...
      */

      for (lineptr = line + xoff; *template; template ++)
      {
        if (*template != ' ')
        {
          for (xcount = xrep; xcount > 0; xcount --)
            *lineptr++ = *color;
        }
        else
        {
          lineptr += xrep;
        }
      }
    }
    else
    {
     /*
      * Do color output...
      */

      for (lineptr = line + 3 * xoff; *template; template ++)
      {
        if (*template != ' ')
        {
          for (xcount = xrep; xcount > 0; xcount --, lineptr += 3)
            memcpy(lineptr, color, 3);
        }
        else
        {
          lineptr += 3 * xrep;
        }
      }
    }

    for (ycount = yrep; ycount > 0 && y < yend; ycount --, y ++)
      cupsRasterWritePixels(ras, line, header.cupsBytesPerLine);
  }

  memset(line, 0xff, header.cupsBytesPerLine);

  for (y = 0; y < header.cupsHeight; y ++)
    cupsRasterWritePixels(ras, line, header.cupsBytesPerLine);

  free(line);

  cupsRasterClose(ras);

  close(fd);

  printf("PRINT FILE: %s\n", tempname);

  return (tempname);
}


/*
 * 'monitor_printer()' - Monitor the job and printer states.
 */

static void *				/* O - Thread exit code */
monitor_printer(
    _client_data_t *data)		/* I - Client data */
{
  http_t	*http;			/* Connection to printer */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* Attribute in response */
  ipp_pstate_t	printer_state;		/* Printer state */
  char          printer_state_reasons[1024];
                                        /* Printer state reasons */
  ipp_jstate_t	job_state;		/* Job state */
  char          job_state_reasons[1024];/* Printer state reasons */
  static const char * const jattrs[] =  /* Job attributes we want */
  {
    "job-state",
    "job-state-reasons"
  };
  static const char * const pattrs[] =  /* Printer attributes we want */
  {
    "printer-state",
    "printer-state-reasons"
  };


 /*
  * Open a connection to the printer...
  */

  http = httpConnect2(data->hostname, data->port, NULL, AF_UNSPEC, data->encryption, 1, 0, NULL);

 /*
  * Loop until the job is canceled, aborted, or completed.
  */

  printer_state            = (ipp_pstate_t)0;
  printer_state_reasons[0] = '\0';

  job_state            = (ipp_jstate_t)0;
  job_state_reasons[0] = '\0';

  while (data->job_state < IPP_JSTATE_CANCELED)
  {
   /*
    * Reconnect to the printer as needed...
    */

    if (httpGetFd(http) < 0)
      httpReconnect2(http, 30000, NULL);

    if (httpGetFd(http) >= 0)
    {
     /*
      * Connected, so check on the printer state...
      */

      request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, data->uri);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
      ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", (int)(sizeof(pattrs) / sizeof(pattrs[0])), NULL, pattrs);

      response = cupsDoRequest(http, request, data->resource);

      if ((attr = ippFindAttribute(response, "printer-state", IPP_TAG_ENUM)) != NULL)
        printer_state = (ipp_pstate_t)ippGetInteger(attr, 0);

      if ((attr = ippFindAttribute(response, "printer-state-reasons", IPP_TAG_KEYWORD)) != NULL)
        ippAttributeString(attr, printer_state_reasons, sizeof(printer_state_reasons));

      if (printer_state != data->printer_state || strcmp(printer_state_reasons, data->printer_state_reasons))
      {
        printf("PRINTER: %s (%s)\n", ippEnumString("printer-state", (int)printer_state), printer_state_reasons);

        data->printer_state = printer_state;
        strlcpy(data->printer_state_reasons, printer_state_reasons, sizeof(data->printer_state_reasons));
      }

      ippDelete(response);

      if (data->job_id > 0)
      {
       /*
        * Check the status of the job itself...
        */

        request = ippNewRequest(IPP_OP_GET_JOB_ATTRIBUTES);
        ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, data->uri);
        ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", data->job_id);
        ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
        ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", (int)(sizeof(jattrs) / sizeof(jattrs[0])), NULL, jattrs);

        response = cupsDoRequest(http, request, data->resource);

        if ((attr = ippFindAttribute(response, "job-state", IPP_TAG_ENUM)) != NULL)
          job_state = (ipp_jstate_t)ippGetInteger(attr, 0);

        if ((attr = ippFindAttribute(response, "job-state-reasons", IPP_TAG_KEYWORD)) != NULL)
          ippAttributeString(attr, job_state_reasons, sizeof(job_state_reasons));

        if (job_state != data->job_state || strcmp(job_state_reasons, data->job_state_reasons))
        {
          printf("JOB %d: %s (%s)\n", data->job_id, ippEnumString("job-state", (int)job_state), job_state_reasons);

          data->job_state = job_state;
          strlcpy(data->job_state_reasons, job_state_reasons, sizeof(data->job_state_reasons));
        }

        ippDelete(response);
      }
    }

    if (data->job_state < IPP_JSTATE_CANCELED)
    {
     /*
      * Sleep for 5 seconds...
      */

      sleep(5);
    }
  }

 /*
  * Cleanup and return...
  */

  httpClose(http);

  printf("FINISHED MONITORING JOB %d\n", data->job_id);

  return (NULL);
}


/*
 * 'run_client()' - Run a client thread.
 */

static void *				/* O - Thread exit code */
run_client(
    _client_data_t *data)		/* I - Client data */
{
  _cups_thread_t monitor_id;		/* Monitoring thread ID */
  const char	*name;			/* Job name */
  char		tempfile[1024] = "";	/* Temporary file (if any) */
  _client_data_t ldata;			/* Local client data */
  http_t	*http;			/* Connection to printer */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* Attribute in response */
  static const char * const pattrs[] =  /* Printer attributes we are interested in */
  {
    "all",
    "media-col-database"
  };


  ldata = *data;

 /*
  * Start monitoring the printer in the background...
  */

  monitor_id = _cupsThreadCreate((_cups_thread_func_t)monitor_printer, &ldata);

 /*
  * Open a connection to the printer...
  */

  http = httpConnect2(data->hostname, data->port, NULL, AF_UNSPEC, data->encryption, 1, 0, NULL);

 /*
  * Query printer status and capabilities...
  */

  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, ldata.uri);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", (int)(sizeof(pattrs) / sizeof(pattrs[0])), NULL, pattrs);

  response = cupsDoRequest(http, request, ldata.resource);

  if (verbosity)
    show_capabilities(response);

 /*
  * Now figure out what we will be printing...
  */

  if (ldata.docfile)
  {
   /*
    * User specified a print file, figure out the format...
    */
    const char *ext;			/* Filename extension */

    if ((ext = strrchr(ldata.docfile, '.')) != NULL)
    {
     /*
      * Guess the format from the extension...
      */

      if (!strcmp(ext, ".jpg"))
        ldata.docformat = "image/jpeg";
      else if (!strcmp(ext, ".pdf"))
        ldata.docformat = "application/pdf";
      else if (!strcmp(ext, ".ps"))
        ldata.docformat = "application/postscript";
      else if (!strcmp(ext, ".pwg"))
        ldata.docformat = "image/pwg-raster";
      else if (!strcmp(ext, ".urf"))
        ldata.docformat = "image/urf";
      else
        ldata.docformat = "application/octet-stream";
    }
    else
    {
     /*
      * Tell the printer to auto-detect...
      */

      ldata.docformat = "application/octet-stream";
    }
  }
  else
  {
   /*
    * No file specified, make something to test with...
    */

    if ((ldata.docfile = make_raster_file(response, ldata.grayscale, tempfile, sizeof(tempfile), &ldata.docformat)) == NULL)
      return ((void *)1);
  }

  ippDelete(response);

 /*
  * Create a job and wait for completion...
  */

  request = ippNewRequest(IPP_OP_CREATE_JOB);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, ldata.uri);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  if ((name = strrchr(ldata.docfile, '/')) != NULL)
    name ++;
  else
    name = ldata.docfile;

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL, name);

  if (verbosity)
    show_attributes("Create-Job request", 1, request);

  response = cupsDoRequest(http, request, ldata.resource);

  if (verbosity)
    show_attributes("Create-Job response", 0, response);

  if (cupsLastError() >= IPP_STATUS_REDIRECTION_OTHER_SITE)
  {
    printf("Unable to create print job: %s\n", cupsLastErrorString());

    ldata.job_state = IPP_JSTATE_ABORTED;

    goto cleanup;
  }

  if ((attr = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) == NULL)
  {
    puts("No job-id returned in Create-Job request.");

    ldata.job_state = IPP_JSTATE_ABORTED;

    goto cleanup;
  }

  ldata.job_id = ippGetInteger(attr, 0);

  printf("CREATED JOB %d, sending %s of type %s\n", ldata.job_id, ldata.docfile, ldata.docformat);

  ippDelete(response);

  request = ippNewRequest(IPP_OP_SEND_DOCUMENT);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, ldata.uri);
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", ldata.job_id);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, ldata.docformat);
  ippAddBoolean(request, IPP_TAG_OPERATION, "last-document", 1);

  if (verbosity)
    show_attributes("Send-Document request", 1, request);

  response = cupsDoFileRequest(http, request, ldata.resource, ldata.docfile);

  if (verbosity)
    show_attributes("Send-Document response", 0, response);

  if (cupsLastError() >= IPP_STATUS_REDIRECTION_OTHER_SITE)
  {
    printf("Unable to print file: %s\n", cupsLastErrorString());

    ldata.job_state = IPP_JSTATE_ABORTED;

    goto cleanup;
  }

  puts("WAITING FOR JOB TO COMPLETE");

  while (ldata.job_state < IPP_JSTATE_CANCELED)
    sleep(1);

 /*
  * Cleanup after ourselves...
  */

  cleanup:

  httpClose(http);

  if (tempfile[0] && !ldata.keepfile)
    unlink(tempfile);

  _cupsThreadWait(monitor_id);

  _cupsMutexLock(&client_mutex);
  client_count --;
  _cupsMutexUnlock(&client_mutex);

  return (NULL);
}


/*
 * 'show_attributes()' - Show attributes in a request or response.
 */

static void
show_attributes(const char *title,      /* I - Title */
                int        request,     /* I - 1 for request, 0 for response */
                ipp_t      *ipp)        /* I - IPP request/response */
{
  int                   minor, major = ippGetVersion(ipp, &minor);
                                        /* IPP version number */
  ipp_tag_t             group = IPP_TAG_ZERO;
                                        /* Current group tag */
  ipp_attribute_t       *attr;          /* Current attribute */
  const char            *name;          /* Attribute name */
  char                  buffer[1024];   /* Value */


  printf("%s:\n", title);
  printf("  version=%d.%d\n", major, minor);
  printf("  request-id=%d\n", ippGetRequestId(ipp));
  if (!request)
    printf("  status-code=%s\n", ippErrorString(ippGetStatusCode(ipp)));

  for (attr = ippFirstAttribute(ipp); attr; attr = ippNextAttribute(ipp))
  {
    if (group != ippGetGroupTag(attr))
    {
      group = ippGetGroupTag(attr);
      if (group)
        printf("  %s:\n", ippTagString(group));
    }

    if ((name = ippGetName(attr)) != NULL)
    {
      ippAttributeString(attr, buffer, sizeof(buffer));
      printf("    %s(%s%s)=%s\n", name, ippGetCount(attr) > 1 ? "1setOf " : "", ippTagString(ippGetValueTag(attr)), buffer);
    }
  }
}


/*
 * 'show_capabilities()' - Show printer capabilities.
 */

static void
show_capabilities(ipp_t *response)      /* I - Printer attributes */
{
  int                   i;              /* Looping var */
  ipp_attribute_t       *attr;          /* Attribute */
  char                  buffer[1024];   /* Attribute value buffer */
  static const char * const pattrs[] =  /* Attributes we want to show */
  {
    "copies-default",
    "copies-supported",
    "finishings-default",
    "finishings-ready",
    "finishings-supported",
    "media-default",
    "media-ready",
    "media-supported",
    "output-bin-default",
    "output-bin-supported",
    "print-color-mode-default",
    "print-color-mode-supported",
    "sides-default",
    "sides-supported",
    "document-format-default",
    "document-format-supported",
    "pwg-raster-document-resolution-supported",
    "pwg-raster-document-type-supported",
    "urf-supported"
  };


  puts("CAPABILITIES:");
  for (i = 0; i < (int)(sizeof(pattrs) / sizeof(pattrs[0])); i ++)
  {
     if ((attr = ippFindAttribute(response, pattrs[i], IPP_TAG_ZERO)) != NULL)
     {
       ippAttributeString(attr, buffer, sizeof(buffer));
       printf("  %s=%s\n", pattrs[i], buffer);
     }
  }
}


/*
 * 'usage()' - Show program usage...
 */

static void
usage(void)
{
  puts("Usage: ./testclient printer-uri [options]");
  puts("Options:");
  puts("  -c num-clients      Simulate multiple clients");
  puts("  -d document-format  Generate the specified format");
  puts("  -f print-file       Print the named file");
  puts("  -g                  Force grayscale printing");
  puts("  -k                  Keep temporary files");
  puts("  -v                  Be more verbose");
}
