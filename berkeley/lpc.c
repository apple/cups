/*
 * "$Id: lpc.c,v 1.4 1999/06/23 14:08:20 mike Exp $"
 *
 *   "lpc" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products.
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
 *   main()            - Parse options and commands.
 *   compare_strings() - Compare two command-line strings.
 *   do_command()      - Do an lpc command...
 *   show_help()       - Show help messages.
 *   show_status()     - Show printers.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <cups/cups.h>
#include <cups/language.h>
#include <cups/debug.h>


/*
 * Local functions...
 */

static int	compare_strings(char *, char *, int);
static void	do_command(http_t *, char *, char *);
static void	show_help(char *);
static void	show_status(http_t *, char *);


/*
 * 'main()' - Parse options and commands.
 */

int
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  int		i;		/* Looping var */
  http_t	*http;		/* Connection to server */
  char		line[1024],	/* Input line from user */
		*params;	/* Pointer to parameters */


 /*
  * Connect to the scheduler...
  */

  http = httpConnect(cupsServer(), ippPort());

  if (argc > 1)
  {
   /*
    * Process a single command on the command-line...
    */

    do_command(http, argv[1], argv[2]);
  }
  else
  {
   /*
    * Do the command prompt thing...
    */

    printf("lpc> ");
    while (fgets(line, sizeof(line), stdin) != NULL)
    {
     /*
      * Strip the trailing newline...
      */

      line[strlen(line) - 1] = '\0';

     /*
      * Find any options in the string...
      */

      while (isspace(line[0]))
        strcpy(line, line + 1);

      for (params = line; *params != '\0'; params ++)
        if (isspace(*params))
	  break;

     /*
      * Remove whitespace between the command and parameters...
      */

      while (isspace(*params))
        *params++ = '\0';

     /*
      * The "quit" and "exit" commands exit; otherwise, process as needed...
      */

      if (compare_strings(line, "quit", 1) == 0 ||
          compare_strings(line, "exit", 2) == 0)
        break;

      if (*params == '\0')
        do_command(http, line, NULL);
      else
        do_command(http, line, params);

     /*
      * Put another prompt out to the user...
      */

      printf("lpc> ");
    }
  }

 /*
  * Close the connection to the server and return...
  */

  httpClose(http);

  return (0);
}


/*
 * 'compare_strings()' - Compare two command-line strings.
 */

static int			/* O - -1 or 1 = no match, 0 = match */
compare_strings(char *s,	/* I - Command-line string */
                char *t,	/* I - Option string */
                int  tmin)	/* I - Minimum number of unique chars in option */
{
  int	slen;			/* Length of command-line string */


  slen = strlen(s);
  if (slen < tmin)
    return (-1);
  else
    return (strncmp(s, t, slen));
}


/*
 * 'do_command()' - Do an lpc command...
 */

static void
do_command(http_t *http,	/* I - HTTP connection to server */
           char   *command,	/* I - Command string */
	   char   *params)	/* I - Parameters for command */
{
  if (compare_strings(command, "status", 4) == 0)
    show_status(http, params);
  else if (compare_strings(command, "help", 1) == 0 ||
           strcmp(command, "?") == 0)
    show_help(params);
  else
    puts("?Invalid command");
}


/*
 * 'show_help()' - Show help messages.
 */

static void
show_help(char *command)	/* I - Command to describe or NULL */
{
  if (command == NULL)
  {
    puts("Commands may be abbreviated.  Commands are:");
    puts("");
    puts("exit    help    quit    status  ?");
  }
  else if (compare_strings(command, "help", 1) == 0 ||
           strcmp(command, "?") == 0)
    puts("help\t\tget help on commands");
  else if (compare_strings(command, "status", 4) == 0)
    puts("status\t\tshow status of daemon and queue");
  else
    puts("?Invalid help command unknown");
}


/*
 * 'show_status()' - Show printers.
 */

static void
show_status(http_t *http,	/* I - HTTP connection to server */
            char   *dests)	/* I - Destinations */
{
  ipp_t		*request,	/* IPP Request */
		*response,	/* IPP Response */
		*jobs;		/* IPP Get Jobs response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  char		*printer,	/* Printer name */
		*device;	/* Device URI */
  ipp_pstate_t	pstate;		/* Printer state */
  int		accepting;	/* Is printer accepting jobs? */
  int		jobcount;	/* Count of current jobs */
  char		*dptr,		/* Pointer into destination list */
		*ptr;		/* Pointer into printer name */
  int		match;		/* Non-zero if this job matches */
  char		printer_uri[HTTP_MAX_URI];
				/* Printer URI */


  DEBUG_printf(("show_status(%08x, %08x)\n", http, dests));

  if (http == NULL)
    return;

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_PRINTERS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                      "attributes-charset", NULL, cupsLangEncoding(language));

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                      "attributes-natural-language", NULL, language->language);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/printers/")) != NULL)
  {
    DEBUG_puts("show_status: request succeeded...");

   /*
    * Loop through the printers returned in the list and display
    * their status...
    */

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a job...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this job...
      */

      printer   = NULL;
      device    = "file:/dev/null";
      pstate    = IPP_PRINTER_IDLE;
      jobcount  = 0;
      accepting = 1;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (strcmp(attr->name, "printer-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  printer = attr->values[0].string.text;

        if (strcmp(attr->name, "device-uri") == 0 &&
	    attr->value_tag == IPP_TAG_URI)
	  device = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-state") == 0 &&
	    attr->value_tag == IPP_TAG_ENUM)
	  pstate = (ipp_pstate_t)attr->values[0].integer;

        if (strcmp(attr->name, "printer-is-accepting-jobs") == 0 &&
	    attr->value_tag == IPP_TAG_BOOLEAN)
	  accepting = attr->values[0].boolean;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (printer == NULL)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

     /*
      * See if this is a printer we're interested in...
      */

      match = dests == NULL;

      if (dests != NULL)
      {
        for (dptr = dests; *dptr != '\0';)
	{
	 /*
	  * Skip leading whitespace and commas...
	  */

	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = printer;
	       *ptr != '\0' && *dptr != '\0' && *ptr == *dptr;
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr) && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;
        }
      }

     /*
      * Display the printer entry if needed...
      */

      if (match)
      {
       /*
        * If the printer state is "IPP_PRINTER_PROCESSING", then grab the
	* current job for the printer.
	*/

        if (pstate == IPP_PRINTER_PROCESSING)
	{
	 /*
	  * Build an IPP_GET_JOBS request, which requires the following
	  * attributes:
	  *
	  *    attributes-charset
	  *    attributes-natural-language
	  *    printer-uri
	  *    limit
	  */

	  request = ippNew();

	  request->request.op.operation_id = IPP_GET_JOBS;
	  request->request.op.request_id   = 1;

	  language = cupsLangDefault();

	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                       "attributes-charset", NULL,
		       cupsLangEncoding(language));

	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                       "attributes-natural-language", NULL,
		       language->language);

          sprintf(printer_uri, "ipp://localhost/printers/%s", printer);
	  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
	                      "printer-uri", NULL, printer_uri);

          if ((jobs = cupsDoRequest(http, request, "/jobs/")) != NULL)
	  {
	    for (attr = jobs->attrs; attr != NULL; attr = attr->next)
	      if (strcmp(attr->name, "job-id") == 0)
	        jobcount ++;

            ippDelete(jobs);
	  }
        }

       /*
        * Display it...
	*/

        printf("%s:\n", printer);
	if (strncmp(device, "file:", 5) == 0)
	  printf("\tprinter is on device \'%s\' speed -1\n", device + 5);
	else
	  printf("\tprinter is on device \'%s\' speed -1\n", device);
	printf("\tqueuing is %sabled\n", accepting ? "en" : "dis");
	printf("\tprinting is %sabled\n",
	       pstate == IPP_PRINTER_STOPPED ? "dis" : "en");
	if (jobcount == 0)
	  puts("\tno entries");
	else
	  printf("\t%d entries\n", jobcount);
	puts("\tdaemon present");
      }

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
}


/*
 * End of "$Id: lpc.c,v 1.4 1999/06/23 14:08:20 mike Exp $".
 */
