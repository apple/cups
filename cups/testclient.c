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

static void	*monitor_printer(_client_monitor_t *monitor);


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  return (0);
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

  while (monitor->job_state < IPP_JOB_CANCELED)
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

   /*
    * Sleep for 5 seconds...
    */

    sleep(5);
  }

 /*
  * Cleanup and return...
  */

  httpClose(http);

  return (NULL);
}
