/*
 * Simulated client test program for CUPS.
 *
 * Copyright 2017 by Apple Inc.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include "raster.h"
#include "thread-private.h"
#include <stdlib.h>


/*
 * Local types...
 */

typedef struct _client_monitor_s
{
  const char		*uri,		/* Printer URI */
			*hostname,	/* Hostname */
			*user,		/* Username */
			*resource;	/* Resource path */
  int			port;		/* Port number */
  http_encryption_t	encryption;	/* Use encryption? */
  ipp_pstate_t		printer_state;	/* Current printer state */
  char                  printer_state_reasons[1024];
                                        /* Current printer-state-reasons */
  int			job_id; 	/* Job ID for submitted job */
  ipp_jstate_t		job_state;	/* Current job state */
  char                  job_state_reasons[1024];
                                        /* Current job-state-reasons */
} _client_monitor_t;


/*
 * Local functions...
 */

static const char       *make_raster_file(ipp_t *response, char *tempname, size_t tempsize, const char **format);
static void	        *monitor_printer(_client_monitor_t *monitor);
static void             show_capabilities(ipp_t *response);
static void             usage(void);


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int                   i;              /* Looping var */
  const char            *opt,           /* Current option */
                        *uri = NULL,    /* Printer URI */
                        *printfile = NULL,
                                        /* Print file */
                        *printformat = NULL;
                                        /* Print format */
  char                  tempfile[1024] = "",
                                        /* Temporary file (if any) */
                        scheme[32],     /* URI scheme */
                        userpass[256],  /* Username:password */
                        hostname[256],  /* Hostname */
                        resource[256];  /* Resource path */
  int                   port;           /* Port number */
  http_encryption_t     encryption;     /* Encryption mode */
  _client_monitor_t     monitor;        /* Monitoring data */
  http_t                *http;          /* HTTP connection */
  ipp_t                 *request,       /* IPP request */
                        *response;      /* IPP response */
  ipp_attribute_t       *attr;          /* IPP attribute */
  static const char * const pattrs[] =  /* Printer attributes we are interested in */
  {
    "job-template",
    "printer-defaults",
    "printer-description",
    "media-col-database",
    "media-col-ready"
  };


 /*
  * Parse command-line options...
  */

  for (i = 1; i < argc; i ++)
  {
    if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
        switch (*opt)
        {
          case 'f' : /* -f print-file */
              if (printfile)
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

              printfile = argv[i];
              break;

          default :
              printf("Unknown option '-%c'.\n", *opt);
              usage();
              return (1);
        }
      }
    }
    else if (uri || (strncmp(argv[i], "ipp://", 6) && strncmp(argv[i], "ipps://", 7)))
    {
      printf("Unknown command-line argument '%s'.\n", argv[i]);
      usage();
      return (1);
    }
    else
      uri = argv[i];
  }

 /*
  * Make sure we have everything we need.
  */

  if (!uri)
  {
    puts("Expected printer URI.");
    usage();
    return (1);
  }

 /*
  * Connect to the printer...
  */

  if (httpSeparateURI(HTTP_URI_CODING_ALL, device_uri, scheme, sizeof(scheme), username, sizeof(username), hostname, sizeof(hostname), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
  {
    printf("Bad printer URI '%s'.\n", uri);
    return (1);
  }

  if (!port)
    port = IPP_PORT;

  if (!strcmp(scheme, "https") || !strcmp(scheme, "ipps"))
    encryption = HTTP_ENCRYPTION_ALWAYS;
  else
    encryption = HTTP_ENCRYPTION_IF_REQUESTED;

  if ((http = httpConnect2(hostname, port, NULL, AF_UNSPEC, encryption, 1, 0, NULL)) == NULL)
  {
    printf("Unable to connect to '%s' on port %d: %s\n", hostname, port, cupsLastErrorString());
    return (1);
  }

 /*
  * Query printer status and capabilities...
  */

  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", (int)(sizeof(pattrs) / sizeof(pattrs[0])), NULL, pattrs);

  response = cupsDoRequest(http, request, resource);

  show_capabilities(response);

 /*
  * Now figure out what we will be printing...
  */

  if (printfile)
  {
   /*
    * User specified a print file, figure out the format...
    */

    if ((opt = strrchr(printfile, '.')) != NULL)
    {
     /*
      * Guess the format from the extension...
      */

      if (!strcmp(opt, ".jpg"))
        printformat = "image/jpeg";
      else if (!strcmp(opt, ".pdf"))
        printformat = "application/pdf";
      else if (!strcmp(opt, ".ps"))
        printformat = "application/postscript";
      else if (!strcmp(opt, ".pwg"))
        printformat = "image/pwg-raster";
      else if (!strcmp(opt, ".urf"))
        printformat = "image/urf";
      else
        printformat = "application/octet-stream";
    }
    else
    {
     /*
      * Tell the printer to auto-detect...
      */

      printformat = "application/octet-stream";
    }
  }
  else
  {
   /*
    * No file specified, make something to test with...
    */

    if ((printfile = make_raster_file(response, tempfile, sizeof(tempfile), &printformat)) == NULL)
      return (1);
  }

  ippDelete(response);

 /*
  * Start monitoring the printer in the background...
  */

  memset(&monitor, 0, sizeof(monitor));

  monitor.uri           = uri;
  monitor.hostname      = hostname;
  monitor.resource      = resource;
  monitor.port          = port;
  monitor.encryption    = encryption;

  _cupsThreadCreate((_cups_thread_func_t)monitor_printer, &monitor);

 /*
  * Create the job and wait for completion...
  */

  request = ippNewRequest(IPP_OP_CREATE_JOB);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  if ((opt = strrchr(printfile, '/')) != NULL)
    opt ++;
  else
    opt = printfile;

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_name, "job-name", NULL, opt);

  response = cupsDoRequest(http, request, resource);

  if (cupsLastError() >= IPP_STATUS_REDIRECTION_OTHER_SITE)
  {
    printf("Unable to create print job: %s\n", cupsLastErrorString());

    monitor.job_state = IPP_JSTATE_ABORTED;

    goto cleanup;
  }

  if ((attr = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) == NULL)
  {
    puts("No job-id returned in Create-Job request.");

    monitor.job_state = IPP_JSTATE_ABORTED;

    goto cleanup;
  }

  monitor.job_id = ippGetInteger(attr, 0);

  ippDelete(response);

  request = ippNewRequest(IPP_OP_CREATE_JOB);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", monitor.job_id);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, printformat);

  response = cupsDoFileRequest(http, request, resource, printfile);

  if (cupsLastError() >= IPP_STATUS_REDIRECTION_OTHER_SITE)
  {
    printf("Unable to print file: %s\n", cupsLastErrorString());

    monitor.job_state = IPP_JSTATE_ABORTED;

    goto cleanup;
  }

  while (monitor.job_state < IPP_JSTATE_CANCELED)
    sleep(1);

 /*
  * Cleanup after ourselves...
  */

  cleanup:

  httpClose(http);

  if (tempfile[0])
    unlink(tempfile);

  return (monitor.job_state == IPP_JSTATE_COMPLETED);
}


/*
 * 'make_raster_file()' - Create a temporary raster file.
 */

static const char *                     /* O - Print filename */
make_raster_file(ipp_t      *response,  /* I - Printer attributes */
                 char       *tempname,  /* I - Temporary filename buffer */
                 size_t     tempsize,   /* I - Size of temp file buffer */
                 const char **format)   /* O - Print format */
{
}


/*
 * 'monitor_printer()' - Monitor the job and printer states.
 */

static void *				/* O - Thread exit code */
monitor_printer(
    _client_monitor_t *monitor)		/* I - Monitoring data */
{
  http_t	*http;			/* Connection to printer */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* Attribute in response */
  ipp_pstate_t	printer_state;		/* Printer state */
  char          printer_state_reasons[1024];
                                        /* Printer state reasons */
  int		job_id;			/* Job ID */
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

  http = httpConnect2(monitor->hostname, monitor->port, NULL, AF_UNSPEC,
                      monitor->encryption, 1, 0, NULL);
  httpSetTimeout(http, 30.0, timeout_cb, NULL);

 /*
  * Loop until the job is canceled, aborted, or completed.
  */

  printer_state            = (ipp_pstate_t)0;
  printer_state_reasons[0] = '\0';

  job_state            = (ipp_jstate_t)0;
  job_state_reasons[0] = '\0';

  while (monitor->job_state < IPP_JSTATE_CANCELED)
  {
   /*
    * Reconnect to the printer as needed...
    */

    if (httpGetFd(http) < 0)
      httpReconnect(http);

    if (httpGetFd(http) >= 0)
    {
     /*
      * Connected, so check on the printer state...
      */

      request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, monitor->uri);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
      ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", (int)(sizeof(pattrs) / sizeof(pattrs[0])), NULL, pattrs);

      response = cupsDoRequest(http, request, monitor->resource);

      if ((attr = ippFindAttribute(response, "printer-state", IPP_TAG_ENUM)) != NULL)
        printer_state = (ipp_pstate_t)ippGetInteger(attr, 0);

      if ((attr = ippFindAttribute(response, "printer-state-reasons", IPP_TAG_KEYWORD)) != NULL)
        ippAttributeString(attr, printer_state_reasons, sizeof(printer_state_reasons));

      if (printer_state != monitor->printer_state || strcmp(printer_state_reasons, monitor->printer_state_reasons))
      {
        printf("PRINTER: %s (%s)\n", ippEnumString("printer-state", printer_state), printer_state_reasons);

        monitor->printer_state = printer_state;
        strlcpy(monitor->printer_state_reasons, printer_state_reasons, sizeof(monitor->printer_state_reasons));
      }

      ippDelete(response);

      if (monitor->job_id > 0)
      {
       /*
        * Check the status of the job itself...
        */

        request = ippNewRequest(IPP_OP_GET_JOB_ATTRIBUTES);
        ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, monitor->uri);
        ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", monitor->job_id);
        ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
        ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", (int)(sizeof(jattrs) / sizeof(jattrs[0])), NULL, jattrs);

        response = cupsDoRequest(http, request, monitor->resource);

        if ((attr = ippFindAttribute(response, "job-state", IPP_TAG_ENUM)) != NULL)
          job_state = (ipp_jstate_t)ippGetInteger(attr, 0);

        if ((attr = ippFindAttribute(response, "job-state-reasons", IPP_TAG_KEYWORD)) != NULL)
          ippAttributeString(attr, job_state_reasons, sizeof(job_state_reasons));

        if (job_state != monitor->job_state || strcmp(job_state_reasons, monitor->job_state_reasons))
        {
          printf("JOB %d: %s (%s)\n", monitor->job_id, ippEnumString("job-state", job_state), job_state_reasons);

          monitor->job_state = job_state;
          strlcpy(monitor->job_state_reasons, job_state_reasons, sizeof(monitor->job_state_reasons));
        }

        ippDelete(response);
      }
    }

    if (monitor.job_state < IPP_JSTATE_CANCELED)
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

  return (NULL);
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
  static const char * const * pattrs[] =/* Attributes we want to show */
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
  }

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
  puts("Usage: ./testclient printer-uri [-f print-file]");
}
