/*
 * "$Id: testsub.c 11889 2014-05-22 13:54:15Z msweet $"
 *
 * Scheduler notification tester for CUPS.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 2006-2007 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/debug-private.h>
#include <cups/string-private.h>
#include <signal.h>
#include <cups/ipp-private.h>	/* TODO: Update so we don't need this */


/*
 * Local globals...
 */

static int	terminate = 0;


/*
 * Local functions...
 */

static void	print_attributes(ipp_t *ipp, int indent);
static void	sigterm_handler(int sig);
static void	usage(void) __attribute__((noreturn));


/*
 * 'main()' - Subscribe to the .
 */

int
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  const char	*uri;			/* URI to use */
  int		num_events;		/* Number of events */
  const char	*events[100];		/* Events */
  int		subscription_id,	/* notify-subscription-id */
		sequence_number,	/* notify-sequence-number */
		interval;		/* Interval between polls */
  http_t	*http;			/* HTTP connection */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* Current attribute */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Parse command-line...
  */

  num_events = 0;
  uri        = NULL;

  for (i = 1; i < argc; i ++)
    if (!strcmp(argv[i], "-E"))
      cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);
    else if (!strcmp(argv[i], "-e"))
    {
      i ++;
      if (i >= argc || num_events >= 100)
        usage();

      events[num_events] = argv[i];
      num_events ++;
    }
    else if (!strcmp(argv[i], "-h"))
    {
      i ++;
      if (i >= argc)
        usage();

      cupsSetServer(argv[i]);
    }
    else if (uri || strncmp(argv[i], "ipp://", 6))
      usage();
    else
      uri = argv[i];

  if (!uri)
    usage();

  if (num_events == 0)
  {
    events[0]  = "all";
    num_events = 1;
  }

 /*
  * Connect to the server...
  */

  if ((http = httpConnectEncrypt(cupsServer(), ippPort(),
                                 cupsEncryption())) == NULL)
  {
    perror(cupsServer());
    return (1);
  }

 /*
  * Catch CTRL-C and SIGTERM...
  */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGINT, sigterm_handler);
  sigset(SIGTERM, sigterm_handler);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  action.sa_handler = sigterm_handler;
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
#else
  signal(SIGINT, sigterm_handler);
  signal(SIGTERM, sigterm_handler);
#endif /* HAVE_SIGSET */

 /*
  * Create the subscription...
  */

  if (strstr(uri, "/jobs/"))
  {
    request = ippNewRequest(IPP_CREATE_JOB_SUBSCRIPTION);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri", NULL, uri);
  }
  else
  {
    request = ippNewRequest(IPP_CREATE_PRINTER_SUBSCRIPTION);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL,
                 uri);
  }

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

  ippAddStrings(request, IPP_TAG_SUBSCRIPTION, IPP_TAG_KEYWORD, "notify-events",
                num_events, NULL, events);
  ippAddString(request, IPP_TAG_SUBSCRIPTION, IPP_TAG_KEYWORD,
               "notify-pull-method", NULL, "ippget");

  response = cupsDoRequest(http, request, uri);
  if (cupsLastError() >= IPP_BAD_REQUEST)
  {
    fprintf(stderr, "Create-%s-Subscription: %s\n",
            strstr(uri, "/jobs") ? "Job" : "Printer", cupsLastErrorString());
    ippDelete(response);
    httpClose(http);
    return (1);
  }

  if ((attr = ippFindAttribute(response, "notify-subscription-id",
                               IPP_TAG_INTEGER)) == NULL)
  {
    fputs("ERROR: No notify-subscription-id in response!\n", stderr);
    ippDelete(response);
    httpClose(http);
    return (1);
  }

  subscription_id = attr->values[0].integer;

  printf("Create-%s-Subscription: notify-subscription-id=%d\n",
         strstr(uri, "/jobs/") ? "Job" : "Printer", subscription_id);

  ippDelete(response);

 /*
  * Monitor for events...
  */

  sequence_number = 0;

  while (!terminate)
  {
   /*
    * Get the current events...
    */

    printf("\nGet-Notifications(%d,%d):", subscription_id, sequence_number);
    fflush(stdout);

    request = ippNewRequest(IPP_GET_NOTIFICATIONS);

    if (strstr(uri, "/jobs/"))
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri", NULL, uri);
    else
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL,
                   uri);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                 "requesting-user-name", NULL, cupsUser());

    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER,
                  "notify-subscription-ids", subscription_id);
    if (sequence_number)
      ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER,
                    "notify-sequence-numbers", sequence_number + 1);

    response = cupsDoRequest(http, request, uri);

    printf(" %s\n", ippErrorString(cupsLastError()));

    if (cupsLastError() >= IPP_BAD_REQUEST)
      fprintf(stderr, "Get-Notifications: %s\n", cupsLastErrorString());
    else if (response)
    {
      print_attributes(response, 0);

      for (attr = ippFindAttribute(response, "notify-sequence-number",
                                   IPP_TAG_INTEGER);
           attr;
	   attr = ippFindNextAttribute(response, "notify-sequence-number",
	                               IPP_TAG_INTEGER))
        if (attr->values[0].integer > sequence_number)
	  sequence_number = attr->values[0].integer;
    }

    if ((attr = ippFindAttribute(response, "notify-get-interval",
                                 IPP_TAG_INTEGER)) != NULL &&
        attr->values[0].integer > 0)
      interval = attr->values[0].integer;
    else
      interval = 5;

    ippDelete(response);
    sleep((unsigned)interval);
  }

 /*
  * Cancel the subscription...
  */

  printf("\nCancel-Subscription:");
  fflush(stdout);

  request = ippNewRequest(IPP_CANCEL_SUBSCRIPTION);

  if (strstr(uri, "/jobs/"))
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri", NULL, uri);
  else
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL,
                 uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER,
                "notify-subscription-id", subscription_id);

  ippDelete(cupsDoRequest(http, request, uri));

  printf(" %s\n", ippErrorString(cupsLastError()));

  if (cupsLastError() >= IPP_BAD_REQUEST)
    fprintf(stderr, "Cancel-Subscription: %s\n", cupsLastErrorString());

 /*
  * Close the connection and return...
  */

  httpClose(http);

  return (0);
}


/*
 * 'print_attributes()' - Print the attributes in a request...
 */

static void
print_attributes(ipp_t *ipp,		/* I - IPP request */
                 int   indent)		/* I - Indentation */
{
  int			i;		/* Looping var */
  ipp_tag_t		group;		/* Current group */
  ipp_attribute_t	*attr;		/* Current attribute */
  _ipp_value_t		*val;		/* Current value */
  static const char * const tags[] =	/* Value/group tag strings */
			{
			  "reserved-00",
			  "operation-attributes-tag",
			  "job-attributes-tag",
			  "end-of-attributes-tag",
			  "printer-attributes-tag",
			  "unsupported-attributes-tag",
			  "subscription-attributes-tag",
			  "event-attributes-tag",
			  "reserved-08",
			  "reserved-09",
			  "reserved-0A",
			  "reserved-0B",
			  "reserved-0C",
			  "reserved-0D",
			  "reserved-0E",
			  "reserved-0F",
			  "unsupported",
			  "default",
			  "unknown",
			  "no-value",
			  "reserved-14",
			  "not-settable",
			  "delete-attr",
			  "admin-define",
			  "reserved-18",
			  "reserved-19",
			  "reserved-1A",
			  "reserved-1B",
			  "reserved-1C",
			  "reserved-1D",
			  "reserved-1E",
			  "reserved-1F",
			  "reserved-20",
			  "integer",
			  "boolean",
			  "enum",
			  "reserved-24",
			  "reserved-25",
			  "reserved-26",
			  "reserved-27",
			  "reserved-28",
			  "reserved-29",
			  "reserved-2a",
			  "reserved-2b",
			  "reserved-2c",
			  "reserved-2d",
			  "reserved-2e",
			  "reserved-2f",
			  "octetString",
			  "dateTime",
			  "resolution",
			  "rangeOfInteger",
			  "begCollection",
			  "textWithLanguage",
			  "nameWithLanguage",
			  "endCollection",
			  "reserved-38",
			  "reserved-39",
			  "reserved-3a",
			  "reserved-3b",
			  "reserved-3c",
			  "reserved-3d",
			  "reserved-3e",
			  "reserved-3f",
			  "reserved-40",
			  "textWithoutLanguage",
			  "nameWithoutLanguage",
			  "reserved-43",
			  "keyword",
			  "uri",
			  "uriScheme",
			  "charset",
			  "naturalLanguage",
			  "mimeMediaType",
			  "memberName"
			};


  for (group = IPP_TAG_ZERO, attr = ipp->attrs; attr; attr = attr->next)
  {
    if ((attr->group_tag == IPP_TAG_ZERO && indent <= 8) || !attr->name)
    {
      group = IPP_TAG_ZERO;
      putchar('\n');
      continue;
    }

    if (group != attr->group_tag)
    {
      group = attr->group_tag;

      putchar('\n');
      for (i = 4; i < indent; i ++)
        putchar(' ');

      printf("%s:\n\n", tags[group]);
    }

    for (i = 0; i < indent; i ++)
      putchar(' ');

    printf("%s (", attr->name);
    if (attr->num_values > 1)
      printf("1setOf ");
    printf("%s):", tags[attr->value_tag]);

    switch (attr->value_tag)
    {
      case IPP_TAG_ENUM :
      case IPP_TAG_INTEGER :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    printf(" %d", val->integer);
          putchar('\n');
          break;

      case IPP_TAG_BOOLEAN :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    printf(" %s", val->boolean ? "true" : "false");
          putchar('\n');
          break;

      case IPP_TAG_RANGE :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    printf(" %d-%d", val->range.lower, val->range.upper);
          putchar('\n');
          break;

      case IPP_TAG_DATE :
          {
	    char	vstring[256];	/* Formatted time */

	    for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	      printf(" (%s)", _cupsStrDate(vstring, sizeof(vstring), ippDateToTime(val->date)));
          }
          putchar('\n');
          break;

      case IPP_TAG_RESOLUTION :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    printf(" %dx%d%s", val->resolution.xres, val->resolution.yres,
	           val->resolution.units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
          putchar('\n');
          break;

      case IPP_TAG_STRING :
      case IPP_TAG_TEXTLANG :
      case IPP_TAG_NAMELANG :
      case IPP_TAG_TEXT :
      case IPP_TAG_NAME :
      case IPP_TAG_KEYWORD :
      case IPP_TAG_URI :
      case IPP_TAG_URISCHEME :
      case IPP_TAG_CHARSET :
      case IPP_TAG_LANGUAGE :
      case IPP_TAG_MIMETYPE :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    printf(" \"%s\"", val->string.text);
          putchar('\n');
          break;

      case IPP_TAG_BEGIN_COLLECTION :
          putchar('\n');

          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	  {
	    if (i)
	      putchar('\n');
	    print_attributes(val->collection, indent + 4);
	  }
          break;

      default :
          printf("UNKNOWN (%d values)\n", attr->num_values);
          break;
    }
  }
}


/*
 * 'sigterm_handler()' - Flag when the user hits CTRL-C...
 */

static void
sigterm_handler(int sig)		/* I - Signal number (unused) */
{
  (void)sig;

  terminate = 1;
}


/*
 * 'usage()' - Show program usage...
 */

static void
usage(void)
{
  puts("Usage: testsub [-E] [-e event ... -e eventN] [-h hostname] URI");
  exit(0);
}



/*
 * End of "$Id: testsub.c 11889 2014-05-22 13:54:15Z msweet $".
 */
