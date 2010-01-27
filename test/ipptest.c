/*
 * "$Id$"
 *
 *   IPP test command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2010 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   main()              - Parse options and do tests.
 *   do_tests()          - Do tests as specified in the test file.
 *   expect_matches()    - Return true if the tag matches the specification.
 *   get_token()         - Get a token from a file.
 *   iso_date()          - Return an ISO 8601 date/time string for the given IPP
 *                         dateTime value.
 *   print_attr()        - Print an attribute on the screen.
 *   print_col()         - Print a collection attribute on the screen.
 *   print_fatal_error() - Print a fatal error message.
 *   print_test_error()  - Print a test error message.
 *   print_xml_header()  - Print a standard XML plist header.
 *   print_xml_string()  - Print an XML string with escaping.
 *   print_xml_trailer() - Print the XML trailer with success/fail value.
 *   usage()             - Show program usage.
 *   with_value()        - Test a WITH-VALUE predicate.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <cups/string.h>
#include <errno.h>
#include <ctype.h>
#include <regex.h>

#include <cups/cups.h>
#include <cups/i18n.h>
#include <cups/http-private.h>
#include <cups/string.h>
#ifndef O_BINARY
#  define O_BINARY 0
#endif /* !O_BINARY */


/*
 * Types...
 */

typedef struct _cups_expect_s		/**** Expected attribute info ****/
{
  int	not_expect;			/* Don't expect attribute? */
  char	*name,				/* Attribute name */
	*of_type,			/* Type name */
	*same_count_as,			/* Parallel attribute name */
	*if_defined,			/* Only required if variable defined */
	*if_undefined,			/* Only required if variable is not defined */
	*with_value;			/* Attribute must include this value */
  int	with_regex;			/* WITH-VALUE is a regular expression */
} _cups_expect_t;


/*
 * Globals...
 */

int		Chunking = 1;		/* Use chunked requests */
int		Verbosity = 0;		/* Show all attributes? */
int		XML = 0,		/* Produce XML output? */
		XMLHeader = 0;		/* 1 if header is written */


/*
 * Local functions...
 */

static int	do_tests(const char *uri, const char *testfile);
static int      expect_matches(_cups_expect_t *expect, ipp_tag_t value_tag);
static char	*get_token(FILE *fp, char *buf, int buflen,
		           int *linenum);
static char	*iso_date(ipp_uchar_t *date);
static void	print_attr(ipp_attribute_t *attr);
static void	print_col(ipp_t *col);
static void	print_fatal_error(const char *s, ...)
#ifdef __GNUC__
__attribute__ ((__format__ (__printf__, 1, 2)))
#endif /* __GNUC__ */
;
static void	print_test_error(const char *s, ...)
#ifdef __GNUC__
__attribute__ ((__format__ (__printf__, 1, 2)))
#endif /* __GNUC__ */
;
static void	print_xml_header(void);
static void	print_xml_string(const char *element, const char *s);
static void	print_xml_trailer(int success, const char *message);
static void	usage(void);
static int      with_value(char *value, int regex, ipp_attribute_t *attr);


/*
 * 'main()' - Parse options and do tests.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  int		status;			/* Status of tests... */
  char		*opt;			/* Current option */
  const char	*uri,			/* URI to use */
		*testfile;		/* Test file to use */
  int		interval;		/* Test interval */


  _cupsSetLocale(argv);

 /*
  * We need at least:
  *
  *     testipp URL testfile
  */

  uri      = NULL;
  testfile = NULL;
  status   = 0;
  interval = 0;

  for (i = 1; i < argc; i ++)
  {
    if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
        switch (*opt)
        {
          case 'c' : /* Enable HTTP chunking */
              Chunking = 1;
              break;

          case 'd' : /* Define a variable */
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
		              _("ipptest: Missing name=value for \"-d\".\n"));
		usage();
              }
	      else
		putenv(argv[i]);
	      break;

          case 'i' : /* Test every N seconds */
	      i++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
		              _("ipptest: Missing seconds for \"-i\".\n"));
		usage();
              }
	      else
		interval = atoi(argv[i]);

              if (XML && interval)
	      {
	        _cupsLangPuts(stderr, _("ipptest: \"-i\" is incompatible with "
			                "\"-x\".\n"));
		usage();
	      }
	      break;

          case 'l' : /* Disable HTTP chunking */
              Chunking = 0;
              break;

          case 'v' : /* Be verbose */
	      Verbosity ++;
	      break;

          case 'X' : /* Produce XML output */
	      XML = 1;

              if (interval)
	      {
	        _cupsLangPuts(stderr, _("ipptest: \"-i\" is incompatible with "
				        "\"-x\".\n"));
		usage();
	      }
	      break;

	  default :
	      _cupsLangPrintf(stderr, _("ipptest: Unknown option \"-%c\".\n"),
	                      *opt);
	      usage();
	      break;
	}
      }
    }
    else if (!strncmp(argv[i], "ipp://", 6) ||
             !strncmp(argv[i], "http://", 7) ||
             !strncmp(argv[i], "https://", 8))
    {
     /*
      * Set URI...
      */

      if (!testfile && uri)
      {
        _cupsLangPuts(stderr, _("ipptest: May only specify a single URI before "
	                        "a test!\n"));
        usage();
      }

      uri      = argv[i];
      testfile = NULL;
    }
    else
    {
     /*
      * Run test...
      */

      testfile = argv[i];

      if (!do_tests(uri, testfile))
        status ++;
    }
  }

  if (!uri || !testfile)
    usage();

 /*
  * Loop if the interval is set...
  */

  if (XML)
    print_xml_trailer(status == 0, NULL);
  else if (interval)
  {
    for (;;)
    {
      sleep(interval);
      do_tests(uri, testfile);
    }
  }

 /*
  * Exit...
  */

  return (status);
}

        
/*
 * 'do_tests()' - Do tests as specified in the test file.
 */

static int				/* 1 = success, 0 = failure */
do_tests(const char *uri,		/* I - URI to connect on */
         const char *testfile)		/* I - Test file to use */
{
  int		i;			/* Looping var */
  int		linenum;		/* Current line number */
  int		version;		/* IPP version number to use */
  http_t	*http = NULL;		/* HTTP connection to server */
  char		scheme[HTTP_MAX_URI],	/* URI scheme */
		userpass[HTTP_MAX_URI],	/* username:password */
		server[HTTP_MAX_URI],	/* Server */
		resource[HTTP_MAX_URI];	/* Resource path */
  int		port;			/* Port number */
  FILE		*fp = NULL;		/* Test file */
  char		token[1024],		/* Token from file */
		*tokenptr,		/* Pointer into token */
		temp[1024],		/* Temporary string */
		*tempptr;		/* Pointer into temp string */
  ipp_t		*request = NULL;	/* IPP request */
  ipp_t		*response = NULL;	/* IPP response */
  ipp_op_t	op;			/* Operation */
  ipp_tag_t	group;			/* Current group */
  ipp_tag_t	value;			/* Current value type */
  ipp_attribute_t *attrptr,		/* Attribute pointer */
		*found;			/* Found attribute */
  char		attr[128];		/* Attribute name */
  int		num_statuses;		/* Number of valid status codes */
  ipp_status_t	statuses[100];		/* Valid status codes */
  int		num_expects = 0;	/* Number of expected attributes */
  _cups_expect_t expects[200],		/* Expected attributes */
		*expect,		/* Current expected attribute */
		*last_expect;		/* Last EXPECT (for predicates) */
  int		num_displayed = 0;	/* Number of displayed attributes */
  char		*displayed[100];	/* Displayed attributes */
  char		name[1024];		/* Name of test */
  char		filename[1024];		/* Filename */
  int		pass;			/* Did we pass the test? */
  int		job_id;			/* Job ID from last operation */
  int		subscription_id;	/* Subscription ID from last operation */


 /*
  * Open the test file...
  */

  if ((fp = fopen(testfile, "r")) == NULL)
  {
    print_fatal_error("Unable to open test file %s - %s", testfile,
                      strerror(errno));
    goto test_error;
  }

 /*
  * Connect to the server...
  */

  httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, sizeof(scheme), userpass,
                  sizeof(userpass), server, sizeof(server), &port, resource,
		  sizeof(resource));
  if ((http = httpConnect(server, port)) == NULL)
  {
    print_fatal_error("Unable to connect to %s on port %d - %s", server, port,
                      strerror(errno));
    goto test_error;
  }

 /*
  * Loop on tests...
  */

  if (XML)
    print_xml_header();
  else
    printf("\"%s\":\n", testfile);

  pass            = 1;
  job_id          = 0;
  subscription_id = 0;
  version         = 11;
  linenum         = 1;

  while (get_token(fp, token, sizeof(token), &linenum) != NULL)
  {
   /*
    * Expect an open brace...
    */

    if (strcmp(token, "{"))
    {
      print_fatal_error("Unexpected token %s seen on line %d.", token, linenum);
      goto test_error;
    }

   /*
    * Initialize things...
    */

    httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, sizeof(scheme), userpass,
                    sizeof(userpass), server, sizeof(server), &port, resource,
		    sizeof(resource));

    request       = ippNew();
    op            = (ipp_op_t)0;
    group         = IPP_TAG_ZERO;
    num_statuses  = 0;
    num_expects   = 0;
    num_displayed = 0;
    last_expect   = NULL;
    filename[0]   = '\0';

    strcpy(name, testfile);
    if (strrchr(name, '.') != NULL)
      *strrchr(name, '.') = '\0';

   /*
    * Parse until we see a close brace...
    */

    while (get_token(fp, token, sizeof(token), &linenum) != NULL)
    {
      if (strcasecmp(token, "EXPECT") &&
          strcasecmp(token, "IF-DEFINED") &&
          strcasecmp(token, "IF-UNDEFINED") &&
          strcasecmp(token, "OF-TYPE") &&
          strcasecmp(token, "SAME-COUNT-AS") &&
          strcasecmp(token, "WITH-VALUE"))
        last_expect = NULL;

      if (!strcmp(token, "}"))
        break;
      else if (!strcasecmp(token, "NAME"))
      {
       /*
        * Name of test...
	*/

	get_token(fp, name, sizeof(name), &linenum);
      }
      else if (!strcasecmp(token, "VERSION"))
      {
       /*
        * IPP version number for test...
	*/

        int major, minor;		/* Major/minor IPP version */


	get_token(fp, temp, sizeof(temp), &linenum);
	if (sscanf(temp, "%d.%d", &major, &minor) == 2 &&
	    major >= 0 && minor >= 0 && minor < 10)
	  version = major * 10 + minor;
	else
	{
	  print_fatal_error("Bad version %s seen on line %d.", token, linenum);
	  goto test_error;
	}
      }
      else if (!strcasecmp(token, "RESOURCE"))
      {
       /*
        * Resource name...
	*/

	get_token(fp, resource, sizeof(resource), &linenum);
      }
      else if (!strcasecmp(token, "OPERATION"))
      {
       /*
        * Operation...
	*/

	get_token(fp, token, sizeof(token), &linenum);
	op = ippOpValue(token);
      }
      else if (!strcasecmp(token, "GROUP"))
      {
       /*
        * Attribute group...
	*/

	get_token(fp, token, sizeof(token), &linenum);
	value = ippTagValue(token);

	if (value == group)
	  ippAddSeparator(request);

        group = value;
      }
      else if (!strcasecmp(token, "DELAY"))
      {
       /*
        * Delay before operation...
	*/

        int delay;

	get_token(fp, token, sizeof(token), &linenum);
	if ((delay = atoi(token)) > 0)
	  sleep(delay);
      }
      else if (!strcasecmp(token, "ATTR"))
      {
       /*
        * Attribute...
	*/

	get_token(fp, token, sizeof(token), &linenum);
	value = ippTagValue(token);
	get_token(fp, attr, sizeof(attr), &linenum);
	get_token(fp, temp, sizeof(temp), &linenum);

        token[sizeof(token) - 1] = '\0';

        for (tempptr = temp, tokenptr = token;
	     *tempptr && tokenptr < (token + sizeof(token) - 1);)
	{
	  if (*tempptr == '$')
	  {
	   /*
	    * Substitute a string/number...
	    */

            if (!strncasecmp(tempptr + 1, "uri", 3))
	    {
	      strlcpy(tokenptr, uri, sizeof(token) - (tokenptr - token));
	      tempptr += 4;
	    }
	    else if (!strncasecmp(tempptr + 1, "scheme", 6) ||
	             !strncasecmp(tempptr + 1, "method", 6))
	    {
	      strlcpy(tokenptr, scheme, sizeof(token) - (tokenptr - token));
	      tempptr += 7;
	    }
	    else if (!strncasecmp(tempptr + 1, "username", 8))
	    {
	      strlcpy(tokenptr, userpass, sizeof(token) - (tokenptr - token));
	      tempptr += 9;
	    }
	    else if (!strncasecmp(tempptr + 1, "hostname", 8))
	    {
	      strlcpy(tokenptr, server, sizeof(token) - (tokenptr - token));
	      tempptr += 9;
	    }
	    else if (!strncasecmp(tempptr + 1, "port", 4))
	    {
	      snprintf(tokenptr, sizeof(token) - (tokenptr - token),
	               "%d", port);
	      tempptr += 5;
	    }
	    else if (!strncasecmp(tempptr + 1, "resource", 8))
	    {
	      strlcpy(tokenptr, resource, sizeof(token) - (tokenptr - token));
	      tempptr += 9;
	    }
	    else if (!strncasecmp(tempptr + 1, "job-id", 6))
	    {
	      snprintf(tokenptr, sizeof(token) - (tokenptr - token),
	               "%d", job_id);
	      tempptr += 7;
	    }
	    else if (!strncasecmp(tempptr + 1, "notify-subscription-id", 22))
	    {
	      snprintf(tokenptr, sizeof(token) - (tokenptr - token),
	               "%d", subscription_id);
	      tempptr += 23;
	    }
	    else if (!strncasecmp(tempptr + 1, "user", 4))
	    {
	      strlcpy(tokenptr, cupsUser(), sizeof(token) - (tokenptr - token));
	      tempptr += 5;
	    }
	    else if (!strncasecmp(tempptr + 1, "ENV[", 4))
	    {
	      char *end;		/* End of $ENV[name] */


	      if ((end = strchr(tempptr + 5, ']')) != NULL)
	      {
	        *end++ = '\0';
		strlcpy(tokenptr,
		        getenv(tempptr + 5) ? getenv(tempptr + 5) : tempptr + 5,
		        sizeof(token) - (tokenptr - token));
		tempptr = end;
	      }
	      else
	      {
		*tokenptr++ = *tempptr++;
		*tokenptr   = '\0';
	      }
	    }
            else
	    {
	      *tokenptr++ = *tempptr++;
	      *tokenptr   = '\0';
	    }

            tokenptr += strlen(tokenptr);
	  }
	  else
	  {
	    *tokenptr++ = *tempptr++;
	    *tokenptr   = '\0';
	  }
	}

        switch (value)
	{
	  case IPP_TAG_BOOLEAN :
	      if (!strcasecmp(token, "true"))
		ippAddBoolean(request, group, attr, 1);
              else
		ippAddBoolean(request, group, attr, atoi(token));
	      break;

	  case IPP_TAG_INTEGER :
	  case IPP_TAG_ENUM :
	      ippAddInteger(request, group, value, attr, atoi(token));
	      break;

	  case IPP_TAG_RESOLUTION :
	      print_fatal_error("resolution tag not yet supported on line %d",
	                        linenum);
	      goto test_error;

	  case IPP_TAG_RANGE :
	      print_fatal_error("range tag not yet supported on line %d",
	                        linenum);
	      goto test_error;

	  default :
	      if (!strchr(token, ','))
	        ippAddString(request, group, value, attr, NULL, token);
	      else
	      {
	       /*
	        * Multiple string values...
		*/

                int	num_values;	/* Number of values */
                char	*values[100],	/* Values */
			*ptr;		/* Pointer to next value */


                values[0]  = token;
		num_values = 1;

                for (ptr = strchr(token, ','); ptr; ptr = strchr(ptr, ','))
		{
		  *ptr++ = '\0';
		  values[num_values] = ptr;
		  num_values ++;
		}

	        ippAddStrings(request, group, value, attr, num_values,
		              NULL, (const char **)values);
	      }
	      break;
	}
      }
      else if (!strcasecmp(token, "FILE"))
      {
       /*
        * File...
	*/

	get_token(fp, filename, sizeof(filename), &linenum);
      }
      else if (!strcasecmp(token, "STATUS"))
      {
       /*
        * Status...
	*/

        if (num_statuses >= (int)(sizeof(statuses) / sizeof(statuses[0])))
	{
	  print_fatal_error("Too many STATUS's on line %d.", linenum);
	  goto test_error;
	}

	get_token(fp, token, sizeof(token), &linenum);
	statuses[num_statuses] = ippErrorValue(token);
	num_statuses ++;
      }
      else if (!strcasecmp(token, "EXPECT"))
      {
       /*
        * Expected attributes...
	*/

        if (num_expects >= (int)(sizeof(expects) / sizeof(expects[0])))
        {
	  print_fatal_error("Too many EXPECT's on line %d.", linenum);
	  goto test_error;
        }

	get_token(fp, token, sizeof(token), &linenum);

        last_expect = expects + num_expects;
	num_expects ++;

        if (token[0] == '!')
        {
          last_expect->not_expect = 1;
          last_expect->name       = strdup(token + 1);
        }
        else
        {
          last_expect->not_expect = 0;
	  last_expect->name       = strdup(token);
	}

        last_expect->of_type       = NULL;
        last_expect->same_count_as = NULL;
        last_expect->if_defined    = NULL;
        last_expect->if_undefined  = NULL;
        last_expect->with_value    = NULL;
      }
      else if (!strcasecmp(token, "OF-TYPE"))
      {
	get_token(fp, token, sizeof(token), &linenum);

	if (last_expect)
	  last_expect->of_type = strdup(token);
	else
	{
	  print_fatal_error("OF-TYPE without a preceding EXPECT on line %d.",
	                    linenum);
	  goto test_error;
	}
      }
      else if (!strcasecmp(token, "SAME-COUNT-AS"))
      {
	get_token(fp, token, sizeof(token), &linenum);

	if (last_expect)
	  last_expect->same_count_as = strdup(token);
	else
	{
	  print_fatal_error("SAME-COUNT-AS without a preceding EXPECT on line "
	                    "%d.", linenum);
	  goto test_error;
	}
      }
      else if (!strcasecmp(token, "IF-DEFINED"))
      {
	get_token(fp, token, sizeof(token), &linenum);

	if (last_expect)
	  last_expect->if_defined = strdup(token);
	else
	{
	  print_fatal_error("IF-DEFINED without a preceding EXPECT on line %d.",
		            linenum);
	  goto test_error;
	}
      }
      else if (!strcasecmp(token, "IF-UNDEFINED"))
      {
	get_token(fp, token, sizeof(token), &linenum);

	if (last_expect)
	  last_expect->if_undefined = strdup(token);
	else
	{
	  print_fatal_error("IF-UNDEFINED without a preceding EXPECT on line "
	                    "%d.", linenum);
	  goto test_error;
	}
      }
      else if (!strcasecmp(token, "WITH-VALUE"))
      {
      	get_token(fp, token, sizeof(token), &linenum);
        if (last_expect)
	{
	  tokenptr = token + strlen(token) - 1;
	  if (token[0] == '/' && tokenptr > token && *tokenptr == '/')
	  {
	   /*
	    * WITH-VALUE is a POSIX extended regular expression.
	    */

	    last_expect->with_value = calloc(1, tokenptr - token);
	    last_expect->with_regex = 1;

	    if (last_expect->with_value)
	      memcpy(last_expect->with_value, token + 1, tokenptr - token - 1);
	  }
	  else
	  {
	   /*
	    * WITH-VALUE is a literal value...
	    */

	    last_expect->with_value = strdup(token);
	  }
	}
	else
	{
	  print_fatal_error("WITH-VALUE without a preceding EXPECT on line %d.",
		            linenum);
	  goto test_error;
	}
      }
      else if (!strcasecmp(token, "DISPLAY"))
      {
       /*
        * Display attributes...
	*/

        if (num_displayed >= (int)(sizeof(displayed) / sizeof(displayed[0])))
	{
	  print_fatal_error("Too many DISPLAY's on line %d", linenum);
	  goto test_error;
	}

	get_token(fp, token, sizeof(token), &linenum);
	displayed[num_displayed] = strdup(token);
	num_displayed ++;
      }
      else
      {
	print_fatal_error("Unexpected token %s seen on line %d.", token,
	                  linenum);
	goto test_error;
      }
    }

   /*
    * Submit the IPP request...
    */

    request->request.op.version[0]   = version / 10;
    request->request.op.version[1]   = version % 10;
    request->request.op.operation_id = op;
    request->request.op.request_id   = 1;

    if (XML)
    {
      puts("<dict>");
      puts("<key>Name</key>");
      print_xml_string("string", name);
      puts("<key>Operation</key>");
      print_xml_string("string", ippOpString(op));
      puts("<key>RequestAttributes</key>");
      puts("<dict>");
      for (attrptr = request->attrs; attrptr; attrptr = attrptr->next)
	print_attr(attrptr);
      puts("</dict>");
    }
    else
    {
      if (Verbosity)
      {
        printf("    %s:\n", ippOpString(op));

        for (attrptr = request->attrs; attrptr; attrptr = attrptr->next)
          print_attr(attrptr);
      }

      printf("    %-60.60s [", name);
      fflush(stdout);
    }

    if (Chunking)
    {
      http_status_t status = cupsSendRequest(http, request, resource, 0);

      if (status == HTTP_CONTINUE && filename[0])
      {
        int	fd;			/* File to send */
        char	buffer[8192];		/* Copy buffer */
        ssize_t	bytes;			/* Bytes read/written */


        if ((fd = open(filename, O_RDONLY | O_BINARY)) >= 0)
        {
          while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
            if ((status = cupsWriteRequestData(http, buffer,
                                               bytes)) != HTTP_CONTINUE)
              break;
        }
        else
          status = HTTP_ERROR;
      }

      ippDelete(request);

      if (status == HTTP_CONTINUE)
	response = cupsGetResponse(http, resource);
      else
	response = NULL;
    }
    else if (filename[0])
      response = cupsDoFileRequest(http, request, resource, filename);
    else
      response = cupsDoIORequest(http, request, resource, -1,
                                 Verbosity ? 1 : -1);

    request = NULL;

    if (!response)
      pass = 0;
    else
    {
      if (http->version != HTTP_1_1)
        pass = 0;

      if ((attrptr = ippFindAttribute(response, "job-id",
                                      IPP_TAG_INTEGER)) != NULL)
        job_id = attrptr->values[0].integer;

      if ((attrptr = ippFindAttribute(response, "notify-subscription-id",
                                      IPP_TAG_INTEGER)) != NULL)
        subscription_id = attrptr->values[0].integer;

      for (i = 0; i < num_statuses; i ++)
        if (response->request.status.status_code == statuses[i])
	  break;

      if (i == num_statuses && num_statuses > 0)
	pass = 0;
      else
      {
        for (i = num_expects, expect = expects; i > 0; i --, expect ++)
        {
          if (expect->if_defined && !getenv(expect->if_defined))
            continue;

          if (expect->if_undefined && getenv(expect->if_undefined))
            continue;
            
          found = ippFindAttribute(response, expect->name, IPP_TAG_ZERO);

          if ((found == NULL) != expect->not_expect ||
              (found && !expect_matches(expect, found->value_tag)))
          {
      	    pass = 0;
      	    break;          
          }

          if (found &&
	      !with_value(expect->with_value, expect->with_regex, found))
          {
            pass = 0;
            break;
          }
          
          if (found && expect->same_count_as)
          {
            attrptr = ippFindAttribute(response, expect->same_count_as,
                                       IPP_TAG_ZERO);

            if (!attrptr || attrptr->num_values != found->num_values)
            {
              pass = 0;
              break;
            }
          }
        }
      }
    }

    if (XML)
    {
      puts("<key>Successful</key>");
      puts(pass ? "<true />" : "<false />");
      puts("<key>StatusCode</key>");
      print_xml_string("string", ippErrorString(cupsLastError()));
      puts("<key>ResponseAttributes</key>");
      puts("<dict>");
      for (attrptr = response ? response->attrs : NULL;
	   attrptr;
	   attrptr = attrptr->next)
	print_attr(attrptr);
      puts("</dict>");
    }
    else
    {
      puts(pass ? "PASS]" : "FAIL]");
      printf("        RECEIVED: %lu bytes in response\n",
	     (unsigned long)ippLength(response));
      printf("        status-code = %x (%s)\n", cupsLastError(),
	     ippErrorString(cupsLastError()));

      if ((Verbosity || !pass) && response)
      {
	for (attrptr = response->attrs;
	     attrptr != NULL;
	     attrptr = attrptr->next)
	{
	  print_attr(attrptr);
	}
      }
    }

    if (pass && !XML && !Verbosity && num_displayed > 0)
    {
      for (attrptr = response->attrs;
	   attrptr != NULL;
	   attrptr = attrptr->next)
	if (attrptr->name)
	  for (i = 0; i < num_displayed; i ++)
	    if (!strcmp(displayed[i], attrptr->name))
	    {
	      print_attr(attrptr);
	      break;
	    }
    }
    else if (!pass)
    {
      if (XML)
      {
	puts("<key>Errors</key>");
	puts("<array>");
      }

      if (http->version != HTTP_1_1)
	print_test_error("Bad HTTP version (%d.%d)", http->version / 100,
			 http->version % 100);

      if (!response)
	print_test_error("IPP request failed with status %04x (%s)",
			 cupsLastError(), cupsLastErrorString());

      for (i = 0; i < num_statuses; i ++)
	if (response->request.status.status_code == statuses[i])
	  break;

      if (i == num_statuses && num_statuses > 0)
	print_test_error("Bad status-code");

      for (i = num_expects, expect = expects; i > 0; i --, expect ++)
      {
	if (expect->if_defined && !getenv(expect->if_defined))
	  continue;

	if (expect->if_undefined && getenv(expect->if_undefined))
	  continue;
    
	found = ippFindAttribute(response, expect->name, IPP_TAG_ZERO);

	if ((found == NULL) != expect->not_expect)
	{
	  if (expect->not_expect)
	    print_test_error("NOT EXPECTED: %s", expect->name);
	  else
	    print_test_error("EXPECTED: %s", expect->name);
	}
	else if (found)
	{
	  if (!expect_matches(expect, found->value_tag))
	    print_test_error("EXPECTED: %s OF-TYPE %s (got %s)", 
			     expect->name, expect->of_type,
			     ippTagString(found->value_tag));
	  else if (!with_value(expect->with_value, expect->with_regex, found))
	  {
	    if (expect->with_regex)
	      print_test_error("EXPECTED: %s WITH-VALUE /%s/",
			       expect->name, expect->with_value);         
	    else
	      print_test_error("EXPECTED: %s WITH-VALUE \"%s\"",
			       expect->name, expect->with_value);         
	  }
	  else if (expect->same_count_as)
	  {
	    attrptr = ippFindAttribute(response, expect->same_count_as,
				       IPP_TAG_ZERO);

	    if (!attrptr)
	      print_test_error("EXPECTED: %s (%d values) SAME-COUNT-AS %s "
			       "(not returned)", expect->name,
			       found->num_values, expect->same_count_as);
	    else if (attrptr->num_values != found->num_values)
	      print_test_error("EXPECTED: %s (%d values) SAME-COUNT-AS %s "
			       "(%d values)", expect->name, found->num_values,
			       expect->same_count_as, attrptr->num_values);
	  }
	}
      }

      if (XML)
	puts("</array>");
    }

    if (XML)
      puts("</dict>");

    ippDelete(response);
    response = NULL;

    for (i = num_expects, expect = expects; i > 0; i --, expect ++)
    {
      free(expect->name);
      if (expect->of_type)
        free(expect->of_type);
      if (expect->same_count_as)
        free(expect->same_count_as);
      if (expect->if_defined)
        free(expect->if_defined);
      if (expect->if_undefined)
        free(expect->if_undefined);  
      if (expect->with_value)
        free(expect->with_value);
    }
    num_expects = 0;

    for (i = 0; i < num_displayed; i ++)
      free(displayed[i]);
    num_displayed = 0;

    if (!pass)
      break;
  }

  fclose(fp);
  httpClose(http);

  return (pass);

 /*
  * If we get here there was a fatal test error...
  */

  test_error:

  if (fp)
    fclose(fp);

  httpClose(http);
  ippDelete(request);
  ippDelete(response);

  for (i = num_expects, expect = expects; i > 0; i --, expect ++)
  {
    free(expect->name);
    if (expect->of_type)
      free(expect->of_type);
    if (expect->same_count_as)
      free(expect->same_count_as);
    if (expect->if_defined)
      free(expect->if_defined);
    if (expect->if_undefined)
      free(expect->if_undefined);  
    if (expect->with_value)
      free(expect->with_value);
  }

  for (i = 0; i < num_displayed; i ++)
    free(displayed[i]);

  return (0);
}


/*
 * 'expect_matches()' - Return true if the tag matches the specification.
 */
 
static int				/* O - 1 if matches, 0 otherwise */
expect_matches(
    _cups_expect_t *expect,		/* I - Expected attribute */
    ipp_tag_t      value_tag)		/* I - Value tag for attribute */
{
  int	match;				/* Match? */
  char	*of_type,			/* Type name to match */
	*next;				/* Next name to match */


 /*
  * If we don't expect a particular type, return immediately...
  */

  if (!expect->of_type)
    return (1);

 /*
  * Parse the "of_type" value since the string can contain multiple attribute
  * types separated by "|"...
  */

  for (of_type = expect->of_type, match = 0; !match && of_type; of_type = next)
  {
   /*
    * Find the next separator, and set it (temporarily) to nul if present.
    */

    if ((next = strchr(of_type, '|')) != NULL)
      *next = '\0';
  
   /*
    * Support some meta-types to make it easier to write the test file.
    */

    if (!strcmp(of_type, "text"))
      match = value_tag == IPP_TAG_TEXTLANG || value_tag == IPP_TAG_TEXT;            
    else if (!strcmp(of_type, "name"))
      match = value_tag == IPP_TAG_NAMELANG || value_tag == IPP_TAG_NAME;    
    else if (!strcmp(of_type, "collection"))
      match = value_tag == IPP_TAG_BEGIN_COLLECTION;   
    else
      match = value_tag == ippTagValue(of_type);

   /*
    * Restore the separator if we have one...
    */

    if (next)
      *next++ = '|';
  }

  return (match);
}


/*
 * 'get_token()' - Get a token from a file.
 */

static char *				/* O  - Token from file or NULL on EOF */
get_token(FILE *fp,			/* I  - File to read from */
          char *buf,			/* I  - Buffer to read into */
	  int  buflen,			/* I  - Length of buffer */
	  int  *linenum)		/* IO - Current line number */
{
  int	ch,				/* Character from file */
	quote;				/* Quoting character */
  char	*bufptr,			/* Pointer into buffer */
	*bufend;			/* End of buffer */


  for (;;)
  {
   /*
    * Skip whitespace...
    */

    while (isspace(ch = getc(fp)))
    {
      if (ch == '\n')
        (*linenum) ++;
    }

   /*
    * Read a token...
    */

    if (ch == EOF)
      return (NULL);
    else if (ch == '\'' || ch == '\"' || ch == '/')
    {
     /*
      * Quoted text or regular expression...
      */

      quote  = ch;
      bufptr = buf;
      bufend = buf + buflen - 1;

      if (quote == '/')
        *bufptr++ = ch;

      while ((ch = getc(fp)) != EOF)
      {
        if (ch == '\\')
	{
	 /*
	  * Escape next character...
	  */

	  if (bufptr < bufend)
	    *bufptr++ = ch;

	  if ((ch = getc(fp)) != EOF && bufptr < bufend)
	    *bufptr++ = ch;
	}
	else if (ch == quote)
          break;
	else if (bufptr < bufend)
          *bufptr++ = ch;
      }

      if (quote == '/' && ch == quote && bufptr < bufend)
        *bufptr++ = quote;

      *bufptr = '\0';

      return (buf);
    }
    else if (ch == '#')
    {
     /*
      * Comment...
      */

      while ((ch = getc(fp)) != EOF)
	if (ch == '\n')
          break;

      (*linenum) ++;
    }
    else
    {
     /*
      * Whitespace delimited text...
      */

      ungetc(ch, fp);

      bufptr = buf;
      bufend = buf + buflen - 1;

      while ((ch = getc(fp)) != EOF)
	if (isspace(ch) || ch == '#')
          break;
	else if (bufptr < bufend)
          *bufptr++ = ch;

      if (ch == '#')
        ungetc(ch, fp);
      else if (ch == '\n')
        (*linenum) ++;
        
      *bufptr = '\0';

      return (buf);
    }
  }
}


/*
 * 'iso_date()' - Return an ISO 8601 date/time string for the given IPP dateTime
 *                value.
 */

static char *				/* O - ISO 8601 date/time string */
iso_date(ipp_uchar_t *date)		/* I - IPP (RFC 1903) date/time value */
{
  unsigned	year = (date[0] << 8) + date[1];
					/* Year */
  static char	buffer[255];		/* String buffer */


  if (date[9] == 0 && date[10] == 0)
    snprintf(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02uZ",
	     year, date[2], date[3], date[4], date[5], date[6]);
  else
    snprintf(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02u%c%02u%02u",
	     year, date[2], date[3], date[4], date[5], date[6],
	     date[8], date[9], date[10]);

  return (buffer);
}


/*
 * 'print_attr()' - Print an attribute on the screen.
 */

static void
print_attr(ipp_attribute_t *attr)	/* I - Attribute to print */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*colattr;	/* Collection attribute */


  if (XML)
  {
    if (!attr->name)
    {
      printf("<key>%s</key>\n<true />\n", ippTagString(attr->group_tag));
      return;
    }

    print_xml_string("key", attr->name);
    if (attr->num_values > 1)
      puts("<array>");
  }
  else
  {
    if (!attr->name)
    {
      puts("        -- separator --");
      return;
    }

    printf("        %s (%s%s) = ", attr->name,
	   attr->num_values > 1 ? "1setOf " : "",
	   ippTagString(attr->value_tag));
  }

  switch (attr->value_tag)
  {
    case IPP_TAG_INTEGER :
    case IPP_TAG_ENUM :
	for (i = 0; i < attr->num_values; i ++)
	  if (XML)
	    printf("<integer>%d</integer>\n", attr->values[i].integer);
	  else
	    printf("%d ", attr->values[i].integer);
	break;

    case IPP_TAG_BOOLEAN :
	for (i = 0; i < attr->num_values; i ++)
	  if (XML)
	    puts(attr->values[i].boolean ? "<true />" : "<false />");
	  else if (attr->values[i].boolean)
	    fputs("true ", stdout);
	  else
	    fputs("false ", stdout);
	break;

    case IPP_TAG_RANGE :
	for (i = 0; i < attr->num_values; i ++)
	  if (XML)
	    printf("<dict><key>lower</key><integer>%d</integer>"
	           "<key>upper</key><integer>%d</integer></dict>\n",
		   attr->values[i].range.lower, attr->values[i].range.upper);
	  else
	    printf("%d-%d ", attr->values[i].range.lower,
		   attr->values[i].range.upper);
	break;

    case IPP_TAG_RESOLUTION :
	for (i = 0; i < attr->num_values; i ++)
	  if (XML)
	    printf("<dict><key>xres</key><integer>%d</integer>"
	           "<key>yres</key><integer>%d</integer>"
		   "<key>units</key><string>%s</string></dict>\n",
	           attr->values[i].resolution.xres,
		   attr->values[i].resolution.yres,
		   attr->values[i].resolution.units == IPP_RES_PER_INCH ?
		       "dpi" : "dpc");
	  else
	    printf("%dx%d%s ", attr->values[i].resolution.xres,
		   attr->values[i].resolution.yres,
		   attr->values[i].resolution.units == IPP_RES_PER_INCH ?
		       "dpi" : "dpc");
	break;

    case IPP_TAG_DATE :
	for (i = 0; i < attr->num_values; i ++)
	  if (XML)
            printf("<date>%s</date>\n", iso_date(attr->values[i].date));
          else
	    printf("%s ", iso_date(attr->values[i].date));
        break;

    case IPP_TAG_STRING :
    case IPP_TAG_TEXT :
    case IPP_TAG_NAME :
    case IPP_TAG_KEYWORD :
    case IPP_TAG_CHARSET :
    case IPP_TAG_URI :
    case IPP_TAG_MIMETYPE :
    case IPP_TAG_LANGUAGE :
	for (i = 0; i < attr->num_values; i ++)
	  if (XML)
	    print_xml_string("string", attr->values[i].string.text);
	  else
	    printf("\"%s\" ", attr->values[i].string.text);
	break;

    case IPP_TAG_TEXTLANG :
    case IPP_TAG_NAMELANG :
	for (i = 0; i < attr->num_values; i ++)
	  if (XML)
	  {
	    fputs("<dict><key>language</key><string>", stdout);
	    print_xml_string(NULL, attr->values[i].string.charset);
	    fputs("</string><key>string</key><string>", stdout);
	    print_xml_string(NULL, attr->values[i].string.text);
	    puts("</string></dict>");
	  }
	  else
	    printf("\"%s\",%s ", attr->values[i].string.text,
		   attr->values[i].string.charset);
	break;

    case IPP_TAG_BEGIN_COLLECTION :
	for (i = 0; i < attr->num_values; i ++)
	{
	  if (XML)
	  {
	    puts("<dict>");
	    for (colattr = attr->values[i].collection->attrs;
	         colattr;
		 colattr = colattr->next)
	      print_attr(colattr);
	    puts("</dict>");
	  }
	  else
	  {
	    if (i)
	      putchar(' ');

	    print_col(attr->values[i].collection);
          }
	}
	break;

    default :
	if (XML)
	  printf("<string>&lt;&lt;%s&gt;&gt;</string>\n",
	         ippTagString(attr->value_tag));
	else
	  fputs(ippTagString(attr->value_tag), stdout);
	break;
  }

  if (XML)
  {
    if (attr->num_values > 1)
      puts("</array>");
  }
  else
    putchar('\n');
}


/*
 * 'print_col()' - Print a collection attribute on the screen.
 */

static void
print_col(ipp_t *col)			/* I - Collection attribute to print */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* Current attribute in collection */


  putchar('{');
  for (attr = col->attrs; attr; attr = attr->next)
  {
    printf("%s(%s%s)=", attr->name, attr->num_values > 1 ? "1setOf " : "",
	   ippTagString(attr->value_tag));

    switch (attr->value_tag)
    {
      case IPP_TAG_INTEGER :
      case IPP_TAG_ENUM :
	  for (i = 0; i < attr->num_values; i ++)
	    printf("%d ", attr->values[i].integer);
	  break;

      case IPP_TAG_BOOLEAN :
	  for (i = 0; i < attr->num_values; i ++)
	    if (attr->values[i].boolean)
	      printf("true ");
	    else
	      printf("false ");
	  break;

      case IPP_TAG_NOVALUE :
	  printf("novalue");
	  break;

      case IPP_TAG_RANGE :
	  for (i = 0; i < attr->num_values; i ++)
	    printf("%d-%d ", attr->values[i].range.lower,
		   attr->values[i].range.upper);
	  break;

      case IPP_TAG_RESOLUTION :
	  for (i = 0; i < attr->num_values; i ++)
	    printf("%dx%d%s ", attr->values[i].resolution.xres,
		   attr->values[i].resolution.yres,
		   attr->values[i].resolution.units == IPP_RES_PER_INCH ?
		       "dpi" : "dpc");
	  break;

      case IPP_TAG_STRING :
      case IPP_TAG_TEXT :
      case IPP_TAG_NAME :
      case IPP_TAG_KEYWORD :
      case IPP_TAG_CHARSET :
      case IPP_TAG_URI :
      case IPP_TAG_MIMETYPE :
      case IPP_TAG_LANGUAGE :
	  for (i = 0; i < attr->num_values; i ++)
	    printf("\"%s\" ", attr->values[i].string.text);
	  break;

      case IPP_TAG_TEXTLANG :
      case IPP_TAG_NAMELANG :
	  for (i = 0; i < attr->num_values; i ++)
	    printf("\"%s\",%s ", attr->values[i].string.text,
		   attr->values[i].string.charset);
	  break;

      case IPP_TAG_BEGIN_COLLECTION :
	  for (i = 0; i < attr->num_values; i ++)
	  {
	    print_col(attr->values[i].collection);
	    putchar(' ');
	  }
	  break;

      default :
	  break; /* anti-compiler-warning-code */
    }
  }

  putchar('}');
}


/*
 * 'print_fatal_error()' - Print a fatal error message.
 */

static void
print_fatal_error(const char *s,	/* I - Printf-style format string */
                  ...)			/* I - Additional arguments as needed */
{
  char		buffer[10240];		/* Format buffer */
  va_list	ap;			/* Pointer to arguments */


 /*
  * Format the error message...
  */

  va_start(ap, s);
  vsnprintf(buffer, sizeof(buffer), s, ap);
  va_end(ap);

 /*
  * Then output it...
  */

  if (XML)
  {
    print_xml_header();
    print_xml_trailer(0, buffer);
  }
  else
    _cupsLangPrintf(stderr, "ipptest: %s\n", buffer);
}


/*
 * 'print_test_error()' - Print a test error message.
 */

static void
print_test_error(const char *s,		/* I - Printf-style format string */
                 ...)			/* I - Additional arguments as needed */
{
  char		buffer[10240];		/* Format buffer */
  va_list	ap;			/* Pointer to arguments */


 /*
  * Format the error message...
  */

  va_start(ap, s);
  vsnprintf(buffer, sizeof(buffer), s, ap);
  va_end(ap);

 /*
  * Then output it...
  */

  if (XML)
    print_xml_string("string", buffer);
  else
    printf("        %s\n", buffer);
}


/*
 * 'print_xml_header()' - Print a standard XML plist header.
 */

static void
print_xml_header(void)
{
  if (!XMLHeader)
  {
    puts("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    puts("<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" "
         "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
    puts("<plist version=\"1.0\">");
    puts("<dict>");
    puts("<key>Chunking</key>");
    puts(Chunking ? "<true />" : "<false />");
    puts("<key>Tests</key>");
    puts("<array>");

    XMLHeader = 1;
  }
}


/*
 * 'print_xml_string()' - Print an XML string with escaping.
 */

static void
print_xml_string(const char *element,	/* I - Element name or NULL */
		 const char *s)		/* I - String to print */
{
  if (element)
    printf("<%s>", element);

  while (*s)
  {
    if (*s == '&')
      fputs("&amp;", stdout);
    else if (*s == '<')
      fputs("&lt;", stdout);
    else if (*s == '>')
      fputs("&gt;", stdout);
    else
      putchar(*s);

    s ++;
  }

  if (element)
    printf("</%s>\n", element);
}


/*
 * 'print_xml_trailer()' - Print the XML trailer with success/fail value.
 */

static void
print_xml_trailer(int        success,	/* I - 1 on success, 0 on failure */
                  const char *message)	/* I - Error message or NULL */
{
  if (XMLHeader)
  {
    puts("</array>");
    puts("<key>Successful</key>");
    puts(success ? "<true />" : "<false />");
    if (message)
    {
      puts("<key>ErrorMessage</key>");
      print_xml_string("string", message);
    }
    puts("</dict>");
    puts("</plist>");

    XMLHeader = 0;
  }
}


/*
 * 'usage()' - Show program usage.
 */

static void
usage(void)
{
  _cupsLangPuts(stderr,
                _("Usage: ipptest [options] URI filename.test [ ... "
		  "filenameN.test ]\n"
		  "\n"
		  "Options:\n"
		  "\n"
		  "-c             Send requests using chunking (default)\n"
		  "-d name=value  Define variable.\n"
		  "-i seconds     Repeat the last test file with the given "
		  "interval.\n"
		  "-l             Send requests using content length\n"
		  "-v             Show all attributes sent and received.\n"
		  "-X             Produce XML instead of plain text.\n"));

  exit(1);
}


/*
 * 'with_value()' - Test a WITH-VALUE predicate.
 */

static int				/* O - 1 on match, 0 on non-match */
with_value(char            *value,	/* I - Value string */
           int             regex,	/* I - Value is a regular expression */
           ipp_attribute_t *attr)	/* I - Attribute to compare */
{
  int	i;				/* Looping var */
  char	*valptr;			/* Pointer into value */


 /*
  * NULL matches everything.
  */

  if (!value)
    return (1);

 /*
  * Compare the value string to the attribute value.
  */

  switch (attr->value_tag)
  {
    case IPP_TAG_INTEGER :
    case IPP_TAG_ENUM :
        if (regex)
	{
	 /*
	  * TODO: Report an error.
	  */

	  return (0);
	}

        for (i = 0; i < attr->num_values; i ++)
        {
          valptr = value;

	  while (isspace(*valptr & 255) || isdigit(*valptr & 255) ||
		 *valptr == '-')
	  {
	    if (attr->values[i].integer == strtol(valptr, &valptr, 0))
	      return (1);
	  }
        }
	break;

    case IPP_TAG_BOOLEAN :
        if (regex)
	{
	 /*
	  * TODO: Report an error.
	  */

	  return (0);
	}

	for (i = 0; i < attr->num_values; i ++)
	{
          if (!strcmp(value, "true") == attr->values[i].boolean)
	    return (1);
	}
	break;

    case IPP_TAG_NOVALUE :
        if (regex)
	{
	 /*
	  * TODO: Report an error.
	  */

	  return (0);
	}

        return (!strcmp(value, "no-value"));

    case IPP_TAG_STRING :
    case IPP_TAG_TEXT :
    case IPP_TAG_NAME :
    case IPP_TAG_KEYWORD :
    case IPP_TAG_CHARSET :
    case IPP_TAG_URI :
    case IPP_TAG_MIMETYPE :
    case IPP_TAG_LANGUAGE :
    case IPP_TAG_TEXTLANG :
    case IPP_TAG_NAMELANG :
        if (regex)
	{
	 /*
	  * Value is an extended, case-sensitive POSIX regular expression...
	  */

	  regex_t	re;		/* Regular expression */

          if ((i = regcomp(&re, value, REG_EXTENDED | REG_NOSUB)) != 0)
	  {
	   /*
	    * Bad regular expression!
	    *
	    * TODO: Report an error here.
	    */

	    return (0);
	  }

         /*
	  * See if ALL of the values match the given regular expression.
	  */

	  for (i = 0; i < attr->num_values; i ++)
	  {
	    if (regexec(&re, attr->values[i].string.text, 0, NULL, 0))
	      break;
	  }

	  regfree(&re);

          return (i == attr->num_values);
	}
	else
	{
	 /*
	  * Value is a literal string, see if at least one value matches the
	  * literal string...
	  */

	  for (i = 0; i < attr->num_values; i ++)
	  {
	    if (!strcmp(value, attr->values[i].string.text))
	      return (1);
	  }
	}
	break;

    default :
       /*
	* TODO: Report an error.
	*/

	return (0);
  }

  return (0);
}


/*
 * End of "$Id$".
 */
