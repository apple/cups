/*
 * "$Id: accept.c,v 1.11 2001/01/23 17:36:22 mike Exp $"
 *
 *   "accept", "disable", "enable", and "reject" commands for the Common
 *   UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2001 by Easy Software Products.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main() - Parse options and accept/reject jobs or disable/enable printers.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <config.h>
#include <cups/cups.h>
#include <cups/language.h>


/*
 * 'main()' - Parse options and accept/reject jobs or disable/enable printers.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  http_t	*http;		/* HTTP connection to server */
  int		i;		/* Looping var */
  char		*command,	/* Command to do */
		hostname[HTTP_MAX_URI],
				/* Name of host */
		printer[HTTP_MAX_URI],
				/* Name of printer or class */
		uri[1024],	/* Printer URI */
		*reason;	/* Reason for reject/disable */
  ipp_t		*request;	/* IPP request */
  ipp_t		*response;	/* IPP response */
  ipp_op_t	op;		/* Operation */
  cups_lang_t	*language;	/* Language */
  int		cancel;		/* Cancel jobs? */
  http_encryption_t encryption;	/* Encryption? */


 /*
  * See what operation we're supposed to do...
  */

  if ((command = strrchr(argv[0], '/')) != NULL)
    command ++;
  else
    command = argv[0];

  cancel = 0;

  if (strcmp(command, "accept") == 0)
    op = CUPS_ACCEPT_JOBS;
  else if (strcmp(command, "reject") == 0)
    op = CUPS_REJECT_JOBS;
  else if (strcmp(command, "disable") == 0)
    op = IPP_PAUSE_PRINTER;
  else if (strcmp(command, "enable") == 0)
    op = IPP_RESUME_PRINTER;
  else
  {
    fprintf(stderr, "%s: Don't know what to do!\n", command);
    return (1);
  }

  http       = NULL;
  reason     = NULL;
  encryption = cupsEncryption();

 /*
  * Process command-line arguments...
  */

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      switch (argv[i][1])
      {
        case 'E' : /* Encrypt */
#ifdef HAVE_LIBSSL
	    encryption = HTTP_ENCRYPT_REQUIRED;

	    if (http)
	      httpEncryption(http, encryption);
#else
            fprintf(stderr, "%s: Sorry, no encryption support compiled in!\n",
	            argv[0]);
#endif /* HAVE_LIBSSL */
	    break;

        case 'c' : /* Cancel jobs */
	    cancel = 1;
	    break;

        case 'h' : /* Connect to host */
	    if (http != NULL)
	      httpClose(http);

	    if (argv[i][2] != '\0')
	      http = httpConnect(argv[i] + 2, ippPort());
	    else
	    {
	      i ++;
	      if (i >= argc)
	      {
	        fprintf(stderr, "%s: Expected server name after -h!\n",
		        command);
	        return (1);
	      }

	      http = httpConnect(argv[i], ippPort());
	    }

	    if (http == NULL)
	    {
	      fputs(argv[0], stderr);
	      perror(": Unable to connect to server");
	      return (1);
	    }
	    break;

        case 'r' : /* Reason for cancellation */
	    if (argv[i][2] != '\0')
	      reason = argv[i] + 2;
	    else
	    {
	      i ++;
	      if (i >= argc)
	      {
	        fprintf(stderr, "%s: Expected reason text after -r!\n", command);
		return (1);
	      }

	      reason = argv[i];
	    }
	    break;

	default :
	    fprintf(stderr, "%s: Unknown option \'%c\'!\n", command,
	            argv[i][1]);
	    return (1);
      }
    else
    {
     /*
      * Accept/disable/enable/reject a destination...
      */

      if (sscanf(argv[i], "%1023[^@]@%1023s", printer, hostname) == 1)
	strcpy(hostname, "localhost");

      if (http != NULL && strcasecmp(http->hostname, hostname) != 0)
      {
	httpClose(http);
	http = NULL;
      }

      if (http == NULL)
        http = httpConnect(hostname, ippPort());

      if (http == NULL)
      {
	fputs(argv[0], stderr);
	perror(": Unable to connect to server");
	return (1);
      }

     /*
      * Build an IPP request, which requires the following
      * attributes:
      *
      *    attributes-charset
      *    attributes-natural-language
      *    printer-uri
      *    printer-state-message [optional]
      */

      request = ippNew();

      request->request.op.operation_id = op;
      request->request.op.request_id   = 1;

      language = cupsLangDefault();

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
              	   "attributes-charset", NULL, cupsLangEncoding(language));

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                   "attributes-natural-language", NULL, language->language);

      snprintf(uri, sizeof(uri), "ipp://%s:%d/printers/%s", hostname, ippPort(), printer);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
                   "printer-uri", NULL, uri);

      if (reason != NULL)
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_TEXT,
                     "printer-state-message", NULL, reason);

     /*
      * Do the request and get back a response...
      */

      httpEncryption(http, encryption);

      if ((response = cupsDoRequest(http, request, "/admin/")) != NULL)
      {
        if (response->request.status.status_code > IPP_OK_CONFLICT)
	{
          fprintf(stderr, "%s: Operation failed: %s\n", command,
	          ippErrorString(cupsLastError()));
	  return (1);
	}
	
        ippDelete(response);
      }
      else
      {
        fprintf(stderr, "%s: Operation failed: %s\n", command,
	        ippErrorString(cupsLastError()));
	return (1);
      }

     /*
      * Cancel all jobs if requested...
      */

      if (cancel)
      {
       /*
	* Build an IPP_PURGE_JOBS request, which requires the following
	* attributes:
	*
	*    attributes-charset
	*    attributes-natural-language
	*    printer-uri
	*/

	request = ippNew();

	request->request.op.operation_id = IPP_PURGE_JOBS;
	request->request.op.request_id   = 1;

	language = cupsLangDefault();

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
              	     "attributes-charset", NULL, cupsLangEncoding(language));

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                     "attributes-natural-language", NULL, language->language);

	snprintf(uri, sizeof(uri), "ipp://%s:%d/printers/%s", hostname, ippPort(), printer);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
                     "printer-uri", NULL, uri);

	if ((response = cupsDoRequest(http, request, "/admin/")) != NULL)
	{
          if (response->request.status.status_code > IPP_OK_CONFLICT)
	  {
            fprintf(stderr, "%s: Operation failed: %s\n", command,
	            ippErrorString(cupsLastError()));
	    return (1);
	  }

          ippDelete(response);
	}
	else
	{
          fprintf(stderr, "%s: Operation failed: %s\n", command,
	          ippErrorString(cupsLastError()));
	  return (1);
	}
      }
    }

  if (http != NULL)
    httpClose(http);

  return (0);
}


/*
 * End of "$Id: accept.c,v 1.11 2001/01/23 17:36:22 mike Exp $".
 */
