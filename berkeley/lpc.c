/*
 * "$Id$"
 *
 *   "lpc" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
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
#include <cups/cups.h>
#include <cups/i18n.h>
#include <cups/debug.h>
#include <cups/string.h>


/*
 * Local functions...
 */

static int	compare_strings(const char *, const char *, int);
static void	do_command(http_t *, const char *, const char *);
static void	show_help(const char *);
static void	show_status(http_t *, const char *);


/*
 * 'main()' - Parse options and commands.
 */

int
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  http_t	*http;			/* Connection to server */
  char		line[1024],		/* Input line from user */
		*params;		/* Pointer to parameters */


  _cupsSetLocale(argv);

 /*
  * Connect to the scheduler...
  */

  http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption());

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

    _cupsLangPuts(stdout, _("lpc> "));
    while (fgets(line, sizeof(line), stdin) != NULL)
    {
     /*
      * Strip trailing whitespace...
      */

      for (params = line + strlen(line) - 1; params >= line;)
        if (!isspace(*params & 255))
	  break;
	else
	  *params-- = '\0';

     /*
      * Strip leading whitespace...
      */

      for (params = line; isspace(*params & 255); params ++);

      if (params > line)
        _cups_strcpy(line, params);

      if (!line[0])
      {
       /*
        * Nothing left, just show a prompt...
	*/

	_cupsLangPuts(stdout, _("lpc> "));
	continue;
      }

     /*
      * Find any options in the string...
      */

      for (params = line; *params != '\0'; params ++)
        if (isspace(*params & 255))
	  break;

     /*
      * Remove whitespace between the command and parameters...
      */

      while (isspace(*params & 255))
        *params++ = '\0';

     /*
      * The "quit" and "exit" commands exit; otherwise, process as needed...
      */

      if (!compare_strings(line, "quit", 1) ||
          !compare_strings(line, "exit", 2))
        break;

      if (*params == '\0')
        do_command(http, line, NULL);
      else
        do_command(http, line, params);

     /*
      * Put another prompt out to the user...
      */

      _cupsLangPuts(stdout, _("lpc> "));
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

static int				/* O - -1 or 1 = no match, 0 = match */
compare_strings(const char *s,		/* I - Command-line string */
                const char *t,		/* I - Option string */
                int        tmin)	/* I - Minimum number of unique chars in option */
{
  int	slen;				/* Length of command-line string */


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
do_command(http_t     *http,		/* I - HTTP connection to server */
           const char *command,		/* I - Command string */
	   const char *params)		/* I - Parameters for command */
{
  if (!compare_strings(command, "status", 4))
    show_status(http, params);
  else if (!compare_strings(command, "help", 1) || !strcmp(command, "?"))
    show_help(params);
  else
    _cupsLangPrintf(stdout,
                    _("%s is not implemented by the CUPS version of lpc.\n"),
		    command);
}


/*
 * 'show_help()' - Show help messages.
 */

static void
show_help(const char *command)		/* I - Command to describe or NULL */
{
  if (!command)
  {
    _cupsLangPrintf(stdout,
                    _("Commands may be abbreviated.  Commands are:\n"
		      "\n"
		      "exit    help    quit    status  ?\n"));
  }
  else if (!compare_strings(command, "help", 1) || !strcmp(command, "?"))
    _cupsLangPrintf(stdout, _("help\t\tget help on commands\n"));
  else if (!compare_strings(command, "status", 4))
    _cupsLangPrintf(stdout, _("status\t\tshow status of daemon and queue\n"));
  else
    _cupsLangPrintf(stdout, _("?Invalid help command unknown\n"));
}


/*
 * 'show_status()' - Show printers.
 */

static void
show_status(http_t     *http,		/* I - HTTP connection to server */
            const char *dests)		/* I - Destinations */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  cups_lang_t	*language;		/* Default language */
  char		*printer,		/* Printer name */
		*device,		/* Device URI */
                *delimiter;		/* Char search result */
  ipp_pstate_t	pstate;			/* Printer state */
  int		accepting;		/* Is printer accepting jobs? */
  int		jobcount;		/* Count of current jobs */
  const char	*dptr,			/* Pointer into destination list */
		*ptr;			/* Pointer into printer name */
  int		match;			/* Non-zero if this job matches */
  static const char *requested[] =	/* Requested attributes */
		{
		  "device-uri",
		  "printer-is-accepting-jobs",
		  "printer-name",
		  "printer-state",
		  "queued-job-count"
		};


  DEBUG_printf(("show_status(http=%p, dests=\"%s\")\n", http, dests));

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

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(requested) / sizeof(requested[0]),
		NULL, requested);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
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
        if (!strcmp(attr->name, "device-uri") &&
	    attr->value_tag == IPP_TAG_URI)
	  device = attr->values[0].string.text;
        else if (!strcmp(attr->name, "printer-is-accepting-jobs") &&
	         attr->value_tag == IPP_TAG_BOOLEAN)
	  accepting = attr->values[0].boolean;
        else if (!strcmp(attr->name, "printer-name") &&
	         attr->value_tag == IPP_TAG_NAME)
	  printer = attr->values[0].string.text;
        else if (!strcmp(attr->name, "printer-state") &&
	         attr->value_tag == IPP_TAG_ENUM)
	  pstate = (ipp_pstate_t)attr->values[0].integer;
        else if (!strcmp(attr->name, "queued-job-count") &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  jobcount = attr->values[0].integer;

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
      * A single 'all' printer name is special, meaning all printers.
      */

      if (dests != NULL && !strcmp(dests, "all"))
        dests = NULL;

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

	  while (isspace(*dptr & 255) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = printer;
	       *ptr != '\0' && *dptr != '\0' && *ptr == *dptr;
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' ||
	                       isspace(*dptr & 255)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr & 255) && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr & 255) || *dptr == ',')
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
        * Display it...
	*/

        printf("%s:\n", printer);
	if (!strncmp(device, "file:", 5))
	  _cupsLangPrintf(stdout,
	                  _("\tprinter is on device \'%s\' speed -1\n"),
			  device + 5);
	else
	{
	 /*
	  * Just show the scheme...
	  */

	  if ((delimiter = strchr(device, ':')) != NULL )
	  {
	    *delimiter = '\0';
	    _cupsLangPrintf(stdout,
	                    _("\tprinter is on device \'%s\' speed -1\n"),
			    device);
	  }
	}

        if (accepting)
	  _cupsLangPuts(stdout, _("\tqueuing is enabled\n"));
	else
	  _cupsLangPuts(stdout, _("\tqueuing is disabled\n"));

        if (pstate != IPP_PRINTER_STOPPED)
	  _cupsLangPuts(stdout, _("\tprinting is enabled\n"));
	else
	  _cupsLangPuts(stdout, _("\tprinting is disabled\n"));

	if (jobcount == 0)
	  _cupsLangPuts(stdout, _("\tno entries\n"));
	else
	  _cupsLangPrintf(stdout, _("\t%d entries\n"), jobcount);

	_cupsLangPuts(stdout, _("\tdaemon present\n"));
      }

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
}


/*
 * End of "$Id$".
 */
