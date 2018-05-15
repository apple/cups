/*
 * Utility for converting PDF and JPEG files to raster data, PCL, PDF, or PS.
 *
 * Copyright © 2016-2018 by Apple Inc..
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessayr headers...
 */

#include <cups/xform-private.h>


/*
 * 'main()' - Main entry for transform utility.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
#if 0
  int		i;			/* Looping var */
  const char	*filename = NULL,	/* File to transform */
		*content_type,		/* Source content type */
		*device_uri,		/* Destination URI */
		*output_type,		/* Destination content type */
		*resolutions,		/* pwg-raster-document-resolution-supported */
		*sheet_back,		/* pwg-raster-document-sheet-back */
		*types,			/* pwg-raster-document-type-supported */
		*opt;			/* Option character */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  int		fd = 1;			/* Output file/socket */
  http_t	*http = NULL;		/* Output HTTP connection */
  void		*write_ptr = &fd;	/* Pointer to file/socket/HTTP connection */
  char		resource[1024];		/* URI resource path */
  xform_write_cb_t write_cb = (xform_write_cb_t)write_fd;
					/* Write callback */
  int		status = 0;		/* Exit status */
  _cups_thread_t monitor = 0;		/* Monitoring thread ID */


 /*
  * Process the command-line...
  */

  num_options  = load_env_options(&options);
  content_type = getenv("CONTENT_TYPE");
  device_uri   = getenv("DEVICE_URI");
  output_type  = getenv("OUTPUT_TYPE");
  resolutions  = getenv("PWG_RASTER_DOCUMENT_RESOLUTION_SUPPORTED");
  sheet_back   = getenv("PWG_RASTER_DOCUMENT_SHEET_BACK");
  types        = getenv("PWG_RASTER_DOCUMENT_TYPE_SUPPORTED");

  if ((opt = getenv("SERVER_LOGLEVEL")) != NULL)
  {
    if (!strcmp(opt, "debug"))
      Verbosity = 2;
    else if (!strcmp(opt, "info"))
      Verbosity = 1;
  }

  for (i = 1; i < argc; i ++)
  {
    if (!strncmp(argv[i], "--", 2))
    {
      if (!strcmp(argv[i], "--help"))
      {
        usage(0);
      }
      else if (!strcmp(argv[i], "--version"))
      {
        puts(CUPS_SVERSION);
      }
      else
      {
	fprintf(stderr, "ERROR: Unknown option '%s'.\n", argv[i]);
	usage(1);
      }
    }
    else if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
        switch (*opt)
	{
	  case 'd' :
	      i ++;
	      if (i >= argc)
	        usage(1);

	      device_uri = argv[i];
	      break;

	  case 'i' :
	      i ++;
	      if (i >= argc)
	        usage(1);

	      content_type = argv[i];
	      break;

	  case 'm' :
	      i ++;
	      if (i >= argc)
	        usage(1);

	      output_type = argv[i];
	      break;

	  case 'o' :
	      i ++;
	      if (i >= argc)
	        usage(1);

	      num_options = cupsParseOptions(argv[i], num_options, &options);
	      break;

	  case 'r' : /* pwg-raster-document-resolution-supported values */
	      i ++;
	      if (i >= argc)
	        usage(1);

	      resolutions = argv[i];
	      break;

	  case 's' : /* pwg-raster-document-sheet-back value */
	      i ++;
	      if (i >= argc)
	        usage(1);

	      sheet_back = argv[i];
	      break;

	  case 't' : /* pwg-raster-document-type-supported values */
	      i ++;
	      if (i >= argc)
	        usage(1);

	      types = argv[i];
	      break;

	  case 'v' : /* Be verbose... */
	      Verbosity ++;
	      break;

	  default :
	      fprintf(stderr, "ERROR: Unknown option '-%c'.\n", *opt);
	      usage(1);
	      break;
	}
      }
    }
    else if (!filename)
      filename = argv[i];
    else
      usage(1);
  }

 /*
  * Check that we have everything we need...
  */

  if (!filename)
    usage(1);

  if (!content_type)
  {
    if ((opt = strrchr(filename, '.')) != NULL)
    {
      if (!strcmp(opt, ".pdf"))
        content_type = "application/pdf";
      else if (!strcmp(opt, ".jpg") || !strcmp(opt, ".jpeg"))
        content_type = "image/jpeg";
    }
  }

  if (!content_type)
  {
    fprintf(stderr, "ERROR: Unknown format for \"%s\", please specify with '-i' option.\n", filename);
    usage(1);
  }
  else if (strcmp(content_type, "application/pdf") && strcmp(content_type, "image/jpeg"))
  {
    fprintf(stderr, "ERROR: Unsupported format \"%s\" for \"%s\".\n", content_type, filename);
    usage(1);
  }

  if (!output_type)
  {
    fputs("ERROR: Unknown output format, please specify with '-m' option.\n", stderr);
    usage(1);
  }
  else if (strcmp(output_type, "application/vnd.hp-pcl") && strcmp(output_type, "image/pwg-raster") && strcmp(output_type, "image/urf"))
  {
    fprintf(stderr, "ERROR: Unsupported output format \"%s\".\n", output_type);
    usage(1);
  }

  if (!resolutions)
    resolutions = "300dpi";
  if (!sheet_back)
    sheet_back = "normal";
  if (!types)
    types = "sgray_8";

 /*
  * If the device URI is specified, open the connection...
  */

  if (device_uri)
  {
    char	scheme[32],		/* URI scheme */
		userpass[256],		/* URI user:pass */
		host[256],		/* URI host */
		service[32];		/* Service port */
    int		port;			/* URI port number */
    http_addrlist_t *list;		/* Address list for socket */

    if (httpSeparateURI(HTTP_URI_CODING_ALL, device_uri, scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
    {
      fprintf(stderr, "ERROR: Invalid device URI \"%s\".\n", device_uri);
      usage(1);
    }

    if (strcmp(scheme, "socket") && strcmp(scheme, "ipp") && strcmp(scheme, "ipps"))
    {
      fprintf(stderr, "ERROR: Unsupported device URI scheme \"%s\".\n", scheme);
      usage(1);
    }

    snprintf(service, sizeof(service), "%d", port);
    if ((list = httpAddrGetList(host, AF_UNSPEC, service)) == NULL)
    {
      fprintf(stderr, "ERROR: Unable to lookup device URI host \"%s\": %s\n", host, cupsLastErrorString());
      return (1);
    }

    if (!strcmp(scheme, "socket"))
    {
     /*
      * AppSocket connection...
      */

      if (!httpAddrConnect2(list, &fd, 30000, NULL))
      {
	fprintf(stderr, "ERROR: Unable to connect to \"%s\" on port %d: %s\n", host, port, cupsLastErrorString());
	return (1);
      }
    }
    else
    {
      http_encryption_t encryption;	/* Encryption mode */
      ipp_t		*request,	/* IPP request */
			*response;	/* IPP response */
      ipp_attribute_t	*attr;		/* operations-supported */
      int		create_job = 0;	/* Support for Create-Job/Send-Document? */
      const char	*job_name;	/* Title of job */
      const char	*media;		/* Value of "media" option */
      const char	*sides;		/* Value of "sides" option */

     /*
      * Connect to the IPP/IPPS printer...
      */

      if (port == 443 || !strcmp(scheme, "ipps"))
        encryption = HTTP_ENCRYPTION_ALWAYS;
      else
        encryption = HTTP_ENCRYPTION_IF_REQUESTED;

      if ((http = httpConnect2(host, port, list, AF_UNSPEC, encryption, 1, 30000, NULL)) == NULL)
      {
	fprintf(stderr, "ERROR: Unable to connect to \"%s\" on port %d: %s\n", host, port, cupsLastErrorString());
	return (1);
      }

     /*
      * See if it supports Create-Job + Send-Document...
      */

      request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, device_uri);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", NULL, "operations-supported");

      response = cupsDoRequest(http, request, resource);
      if (cupsLastError() > IPP_STATUS_OK_EVENTS_COMPLETE)
      {
        fprintf(stderr, "ERROR: Unable to get printer capabilities: %s\n", cupsLastErrorString());
	return (1);
      }

      if ((attr = ippFindAttribute(response, "operations-supported", IPP_TAG_ENUM)) == NULL)
      {
        fputs("ERROR: Unable to get list of supported operations from printer.\n", stderr);
	return (1);
      }

      create_job = ippContainsInteger(attr, IPP_OP_CREATE_JOB) && ippContainsInteger(attr, IPP_OP_SEND_DOCUMENT);

      ippDelete(response);

     /*
      * Create the job and start printing...
      */

      if ((job_name = getenv("IPP_JOB_NAME")) == NULL)
      {
	if ((job_name = strrchr(filename, '/')) != NULL)
	  job_name ++;
	else
	  job_name = filename;
      }

      if (create_job)
      {
        int		job_id = 0;	/* Job ID */

        request = ippNewRequest(IPP_OP_CREATE_JOB);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, device_uri);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL, job_name);

        response = cupsDoRequest(http, request, resource);
        if ((attr = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) != NULL)
	  job_id = ippGetInteger(attr, 0);
        ippDelete(response);

	if (cupsLastError() > IPP_STATUS_OK_EVENTS_COMPLETE)
	{
	  fprintf(stderr, "ERROR: Unable to create print job: %s\n", cupsLastErrorString());
	  return (1);
	}
	else if (job_id <= 0)
	{
          fputs("ERROR: No job-id for created print job.\n", stderr);
	  return (1);
	}

        request = ippNewRequest(IPP_OP_SEND_DOCUMENT);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, device_uri);
	ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, output_type);
        ippAddBoolean(request, IPP_TAG_OPERATION, "last-document", 1);
      }
      else
      {
        request = ippNewRequest(IPP_OP_PRINT_JOB);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, device_uri);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, output_type);
      }

      if ((media = cupsGetOption("media", num_options, options)) != NULL)
        ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "media", NULL, media);

      if ((sides = cupsGetOption("sides", num_options, options)) != NULL)
        ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "sides", NULL, sides);

      if (cupsSendRequest(http, request, resource, 0) != HTTP_STATUS_CONTINUE)
      {
        fprintf(stderr, "ERROR: Unable to send print data: %s\n", cupsLastErrorString());
	return (1);
      }

      ippDelete(request);

      write_cb  = (xform_write_cb_t)httpWrite2;
      write_ptr = http;

      monitor = _cupsThreadCreate((_cups_thread_func_t)monitor_ipp, (void *)device_uri);
    }

    httpAddrFreeList(list);
  }

 /*
  * Do transform...
  */

  status = xform_document(filename, content_type, output_type, resolutions, sheet_back, types, num_options, options, write_cb, write_ptr);

  if (http)
  {
    ippDelete(cupsGetResponse(http, resource));

    if (cupsLastError() > IPP_STATUS_OK_EVENTS_COMPLETE)
    {
      fprintf(stderr, "ERROR: Unable to send print data: %s\n", cupsLastErrorString());
      status = 1;
    }

    httpClose(http);
  }
  else if (fd != 1)
    close(fd);

  if (monitor)
    _cupsThreadCancel(monitor);

  return (status);
#endif /* 0 */

  (void)argc;
  (void)argv;

  return (0);
}


/*
 * 'pcl_end_job()' - End a PCL "job".
 */

static void
pcl_end_job(xform_raster_t   *ras,	/* I - Raster information */
            xform_write_cb_t cb,	/* I - Write callback */
            void             *ctx)	/* I - Write context */
{
  (void)ras;

 /*
  * Send a PCL reset sequence.
  */

  (*cb)(ctx, (const unsigned char *)"\033E", 2);
}


/*
 * 'pcl_end_page()' - End of PCL page.
 */

static void
pcl_end_page(xform_raster_t   *ras,	/* I - Raster information */
	     unsigned         page,	/* I - Current page */
             xform_write_cb_t cb,	/* I - Write callback */
             void             *ctx)	/* I - Write context */
{
 /*
  * End graphics...
  */

  (*cb)(ctx, (const unsigned char *)"\033*r0B", 5);

 /*
  * Formfeed as needed...
  */

  if (!(ras->header.Duplex && (page & 1)))
    (*cb)(ctx, (const unsigned char *)"\014", 1);

 /*
  * Free the output buffer...
  */

  free(ras->out_buffer);
  ras->out_buffer = NULL;
}


/*
 * 'pcl_init()' - Initialize callbacks for PCL output.
 */

static void
pcl_init(xform_raster_t *ras)		/* I - Raster information */
{
  ras->end_job    = pcl_end_job;
  ras->end_page   = pcl_end_page;
  ras->start_job  = pcl_start_job;
  ras->start_page = pcl_start_page;
  ras->write_line = pcl_write_line;
}


/*
 * 'pcl_printf()' - Write a formatted string.
 */

static void
pcl_printf(xform_write_cb_t cb,		/* I - Write callback */
           void             *ctx,	/* I - Write context */
	   const char       *format,	/* I - Printf-style format string */
	   ...)				/* I - Additional arguments as needed */
{
  va_list	ap;			/* Argument pointer */
  char		buffer[8192];		/* Buffer */


  va_start(ap, format);
  vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);

  (*cb)(ctx, (const unsigned char *)buffer, strlen(buffer));
}


/*
 * 'pcl_start_job()' - Start a PCL "job".
 */

static void
pcl_start_job(xform_raster_t   *ras,	/* I - Raster information */
              xform_write_cb_t cb,	/* I - Write callback */
              void             *ctx)	/* I - Write context */
{
  (void)ras;

 /*
  * Send a PCL reset sequence.
  */

  (*cb)(ctx, (const unsigned char *)"\033E", 2);
}


/*
 * 'pcl_start_page()' - Start a PCL page.
 */

static void
pcl_start_page(xform_raster_t   *ras,	/* I - Raster information */
               unsigned         page,	/* I - Current page */
               xform_write_cb_t cb,	/* I - Write callback */
               void             *ctx)	/* I - Write context */
{
 /*
  * Setup margins to be 1/6" top and bottom and 1/4" or .135" on the
  * left and right.
  */

  ras->top    = ras->header.HWResolution[1] / 6;
  ras->bottom = ras->header.cupsHeight - ras->header.HWResolution[1] / 6 - 1;

  if (ras->header.PageSize[1] == 842)
  {
   /* A4 gets special side margins to expose an 8" print area */
    ras->left  = (ras->header.cupsWidth - 8 * ras->header.HWResolution[0]) / 2;
    ras->right = ras->left + 8 * ras->header.HWResolution[0] - 1;
  }
  else
  {
   /* All other sizes get 1/4" margins */
    ras->left  = ras->header.HWResolution[0] / 4;
    ras->right = ras->header.cupsWidth - ras->header.HWResolution[0] / 4 - 1;
  }

  if (!ras->header.Duplex || (page & 1))
  {
   /*
    * Set the media size...
    */

    pcl_printf(cb, ctx, "\033&l12D\033&k12H");
					/* Set 12 LPI, 10 CPI */
    pcl_printf(cb, ctx, "\033&l0O");	/* Set portrait orientation */

    switch (ras->header.PageSize[1])
    {
      case 540 : /* Monarch Envelope */
          pcl_printf(cb, ctx, "\033&l80A");
	  break;

      case 595 : /* A5 */
          pcl_printf(cb, ctx, "\033&l25A");
	  break;

      case 624 : /* DL Envelope */
          pcl_printf(cb, ctx, "\033&l90A");
	  break;

      case 649 : /* C5 Envelope */
          pcl_printf(cb, ctx, "\033&l91A");
	  break;

      case 684 : /* COM-10 Envelope */
          pcl_printf(cb, ctx, "\033&l81A");
	  break;

      case 709 : /* B5 Envelope */
          pcl_printf(cb, ctx, "\033&l100A");
	  break;

      case 756 : /* Executive */
          pcl_printf(cb, ctx, "\033&l1A");
	  break;

      case 792 : /* Letter */
          pcl_printf(cb, ctx, "\033&l2A");
	  break;

      case 842 : /* A4 */
          pcl_printf(cb, ctx, "\033&l26A");
	  break;

      case 1008 : /* Legal */
          pcl_printf(cb, ctx, "\033&l3A");
	  break;

      case 1191 : /* A3 */
          pcl_printf(cb, ctx, "\033&l27A");
	  break;

      case 1224 : /* Tabloid */
          pcl_printf(cb, ctx, "\033&l6A");
	  break;
    }

   /*
    * Set top margin and turn off perforation skip...
    */

    pcl_printf(cb, ctx, "\033&l%uE\033&l0L", 12 * ras->top / ras->header.HWResolution[1]);

    if (ras->header.Duplex)
    {
      int mode = ras->header.Duplex ? 1 + ras->header.Tumble != 0 : 0;

      pcl_printf(cb, ctx, "\033&l%dS", mode);
					/* Set duplex mode */
    }
  }
  else if (ras->header.Duplex)
    pcl_printf(cb, ctx, "\033&a2G");	/* Print on back side */

 /*
  * Set graphics mode...
  */

  pcl_printf(cb, ctx, "\033*t%uR", ras->header.HWResolution[0]);
					/* Set resolution */
  pcl_printf(cb, ctx, "\033*r%uS", ras->right - ras->left + 1);
					/* Set width */
  pcl_printf(cb, ctx, "\033*r%uT", ras->bottom - ras->top + 1);
					/* Set height */
  pcl_printf(cb, ctx, "\033&a0H\033&a%uV", 720 * ras->top / ras->header.HWResolution[1]);
					/* Set position */

  pcl_printf(cb, ctx, "\033*b2M");	/* Use PackBits compression */
  pcl_printf(cb, ctx, "\033*r1A");	/* Start graphics */

 /*
  * Allocate the output buffer...
  */

  ras->out_blanks  = 0;
  ras->out_length  = (ras->right - ras->left + 8) / 8;
  ras->out_buffer  = malloc(ras->out_length);
  ras->comp_buffer = malloc(2 * ras->out_length + 2);
}


/*
 * 'pcl_write_line()' - Write a line of raster data.
 */

static void
pcl_write_line(
    xform_raster_t      *ras,		/* I - Raster information */
    unsigned            y,		/* I - Line number */
    const unsigned char *line,		/* I - Pixels on line */
    xform_write_cb_t    cb,		/* I - Write callback */
    void                *ctx)		/* I - Write context */
{
  unsigned	x;			/* Column number */
  unsigned char	bit,			/* Current bit */
		byte,			/* Current byte */
		*outptr,		/* Pointer into output buffer */
		*outend,		/* End of output buffer */
		*start,			/* Start of sequence */
		*compptr;		/* Pointer into compression buffer */
  unsigned	count;			/* Count of bytes for output */


  if (line[0] == 255 && !memcmp(line, line + 1, ras->right - ras->left))
  {
   /*
    * Skip blank line...
    */

    ras->out_blanks ++;
    return;
  }

 /*
  * Dither the line into the output buffer...
  */

  y &= 63;

  for (x = ras->left, bit = 128, byte = 0, outptr = ras->out_buffer; x <= ras->right; x ++, line ++)
  {
    if (*line <= threshold[x & 63][y])
      byte |= bit;

    if (bit == 1)
    {
      *outptr++ = byte;
      byte      = 0;
      bit       = 128;
    }
    else
      bit >>= 1;
  }

  if (bit != 128)
    *outptr++ = byte;

 /*
  * Apply compression...
  */

  compptr = ras->comp_buffer;
  outend  = outptr;
  outptr  = ras->out_buffer;

  while (outptr < outend)
  {
    if ((outptr + 1) >= outend)
    {
     /*
      * Single byte on the end...
      */

      *compptr++ = 0x00;
      *compptr++ = *outptr++;
    }
    else if (outptr[0] == outptr[1])
    {
     /*
      * Repeated sequence...
      */

      outptr ++;
      count = 2;

      while (outptr < (outend - 1) &&
	     outptr[0] == outptr[1] &&
	     count < 127)
      {
	outptr ++;
	count ++;
      }

      *compptr++ = (unsigned char)(257 - count);
      *compptr++ = *outptr++;
    }
    else
    {
     /*
      * Non-repeated sequence...
      */

      start = outptr;
      outptr ++;
      count = 1;

      while (outptr < (outend - 1) &&
	     outptr[0] != outptr[1] &&
	     count < 127)
      {
	outptr ++;
	count ++;
      }

      *compptr++ = (unsigned char)(count - 1);

      memcpy(compptr, start, count);
      compptr += count;
    }
  }

 /*
  * Output the line...
  */

  if (ras->out_blanks > 0)
  {
   /*
    * Skip blank lines first...
    */

    pcl_printf(cb, ctx, "\033*b%dY", ras->out_blanks);
    ras->out_blanks = 0;
  }

  pcl_printf(cb, ctx, "\033*b%dW", (int)(compptr - ras->comp_buffer));
  (*cb)(ctx, ras->comp_buffer, (size_t)(compptr - ras->comp_buffer));
}


/*
 * 'raster_end_job()' - End a raster "job".
 */

static void
raster_end_job(xform_raster_t   *ras,	/* I - Raster information */
	       xform_write_cb_t cb,	/* I - Write callback */
	       void             *ctx)	/* I - Write context */
{
  (void)cb;
  (void)ctx;

  cupsRasterClose(ras->ras);
}


/*
 * 'raster_end_page()' - End of raster page.
 */

static void
raster_end_page(xform_raster_t   *ras,	/* I - Raster information */
	        unsigned         page,	/* I - Current page */
		xform_write_cb_t cb,	/* I - Write callback */
		void             *ctx)	/* I - Write context */
{
  (void)page;
  (void)cb;
  (void)ctx;

  if (ras->header.cupsBitsPerPixel == 1)
  {
    free(ras->out_buffer);
    ras->out_buffer = NULL;
  }
}


/*
 * 'raster_init()' - Initialize callbacks for raster output.
 */

static void
raster_init(xform_raster_t *ras)	/* I - Raster information */
{
  ras->end_job    = raster_end_job;
  ras->end_page   = raster_end_page;
  ras->start_job  = raster_start_job;
  ras->start_page = raster_start_page;
  ras->write_line = raster_write_line;
}


/*
 * 'raster_start_job()' - Start a raster "job".
 */

static void
raster_start_job(xform_raster_t   *ras,	/* I - Raster information */
		 xform_write_cb_t cb,	/* I - Write callback */
		 void             *ctx)	/* I - Write context */
{
  ras->ras = cupsRasterOpenIO((cups_raster_iocb_t)cb, ctx, !strcmp(ras->format, "image/pwg-raster") ? CUPS_RASTER_WRITE_PWG : CUPS_RASTER_WRITE_APPLE);
}


/*
 * 'raster_start_page()' - Start a raster page.
 */

static void
raster_start_page(xform_raster_t   *ras,/* I - Raster information */
		  unsigned         page,/* I - Current page */
		  xform_write_cb_t cb,	/* I - Write callback */
		  void             *ctx)/* I - Write context */
{
  (void)cb;
  (void)ctx;

  ras->left   = 0;
  ras->top    = 0;
  ras->right  = ras->header.cupsWidth - 1;
  ras->bottom = ras->header.cupsHeight - 1;

  if (ras->header.Duplex && !(page & 1))
    cupsRasterWriteHeader2(ras->ras, &ras->back_header);
  else
    cupsRasterWriteHeader2(ras->ras, &ras->header);

  if (ras->header.cupsBitsPerPixel == 1)
  {
    ras->out_length = ras->header.cupsBytesPerLine;
    ras->out_buffer = malloc(ras->header.cupsBytesPerLine);
  }
}


/*
 * 'raster_write_line()' - Write a line of raster data.
 */

static void
raster_write_line(
    xform_raster_t      *ras,		/* I - Raster information */
    unsigned            y,		/* I - Line number */
    const unsigned char *line,		/* I - Pixels on line */
    xform_write_cb_t    cb,		/* I - Write callback */
    void                *ctx)		/* I - Write context */
{
  (void)cb;
  (void)ctx;

  if (ras->header.cupsBitsPerPixel == 1)
  {
   /*
    * Dither the line into the output buffer...
    */

    unsigned		x;		/* Column number */
    unsigned char	bit,		/* Current bit */
			byte,		/* Current byte */
			*outptr;	/* Pointer into output buffer */

    y &= 63;

    if (ras->header.cupsColorSpace == CUPS_CSPACE_SW)
    {
      for (x = ras->left, bit = 128, byte = 0, outptr = ras->out_buffer; x <= ras->right; x ++, line ++)
      {
	if (*line > threshold[x % 25][y])
	  byte |= bit;

	if (bit == 1)
	{
	  *outptr++ = byte;
	  byte      = 0;
	  bit       = 128;
	}
	else
	  bit >>= 1;
      }
    }
    else
    {
      for (x = ras->left, bit = 128, byte = 0, outptr = ras->out_buffer; x <= ras->right; x ++, line ++)
      {
	if (*line <= threshold[x & 63][y])
	  byte |= bit;

	if (bit == 1)
	{
	  *outptr++ = byte;
	  byte      = 0;
	  bit       = 128;
	}
	else
	  bit >>= 1;
      }
    }

    if (bit != 128)
      *outptr++ = byte;

    cupsRasterWritePixels(ras->ras, ras->out_buffer, ras->header.cupsBytesPerLine);
  }
  else
    cupsRasterWritePixels(ras->ras, (unsigned char *)line, ras->header.cupsBytesPerLine);
}


/*
 * 'write_fd()' - Write to a file/socket.
 */

static ssize_t				/* O - Number of bytes written or -1 on error */
write_fd(int                 *fd,	/* I - File descriptor */
         const unsigned char *buffer,	/* I - Buffer */
         size_t              bytes)	/* I - Number of bytes to write */
{
  ssize_t	temp,			/* Temporary byte count */
		total = 0;		/* Total bytes written */


  while (bytes > 0)
  {
    if ((temp = write(*fd, buffer, bytes)) < 0)
    {
      if (errno == EINTR || errno == EAGAIN)
        continue;
      else
        return (-1);
    }

    total  += temp;
    bytes  -= (size_t)temp;
    buffer += temp;
  }

  return (total);
}
