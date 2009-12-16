/*
 * "$Id: ipptest.c 7847 2008-08-19 04:22:14Z mike $"
 *
 *   IPP test command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2009 by Apple Inc.
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
 *   main()           - Parse options and do tests.
 *   do_tests()       - Do tests as specified in the test file.
 *   expect_matches() - Return true if the tag matches the specification.
 *   get_token()      - Get a token from a file.
 *   print_attr()     - Print an attribute on the screen.
 *   print_col()      - Print a collection attribute on the screen.
 *   usage()          - Show program usage.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <cups/string.h>
#include <errno.h>
#include <ctype.h>

#include <cups/cups.h>
#include <cups/language.h>
#include <cups/http-private.h>
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
	*if_defined;			/* Only required if variable defined */
} _cups_expect_t;


/*
 * Globals...
 */

int		Chunking = 0;		/* Use chunked requests */
int		Verbosity = 0;		/* Show all attributes? */


/*
 * Local functions...
 */

static int	do_tests(const char *uri, const char *testfile);
static int      expect_matches(_cups_expect_t *expect, ipp_tag_t value_tag);
static char	*get_token(FILE *fp, char *buf, int buflen,
		           int *linenum);
static void	print_attr(ipp_attribute_t *attr);
static void	print_col(ipp_t *col);
static void	usage(void);


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
		fputs("ipptest: Missing name=value for \"-d\"!\n", stderr);
		usage();
              }
	      else
		putenv(argv[i]);
	      break;

          case 'i' : /* Test every N seconds */
	      i++;

	      if (i >= argc)
	      {
		fputs("ipptest: Missing seconds for \"-i\"!\n", stderr);
		usage();
              }
	      else
		interval = atoi(argv[i]);
	      break;

          case 'v' : /* Be verbose */
	      Verbosity ++;
	      break;

	  default :
	      fprintf(stderr, "ipptest: Unknown option \"-%c\"!\n", *opt);
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
        fputs("ipptest: May only specify a single URI before a test!\n",
              stderr);
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

  if (interval)
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
  http_t	*http;			/* HTTP connection to server */
  char		scheme[HTTP_MAX_URI],	/* URI scheme */
		userpass[HTTP_MAX_URI],	/* username:password */
		server[HTTP_MAX_URI],	/* Server */
		resource[HTTP_MAX_URI];	/* Resource path */
  int		port;			/* Port number */
  FILE		*fp;			/* Test file */
  char		token[1024],		/* Token from file */
		*tokenptr,		/* Pointer into token */
		temp[1024],		/* Temporary string */
		*tempptr;		/* Pointer into temp string */
  ipp_t		*request;		/* IPP request */
  ipp_t		*response;		/* IPP response */
  ipp_op_t	op;			/* Operation */
  ipp_tag_t	group;			/* Current group */
  ipp_tag_t	value;			/* Current value type */
  ipp_attribute_t *attrptr,		/* Attribute pointer */
		*found;			/* Found attribute */
  char		attr[128];		/* Attribute name */
  int		num_statuses;		/* Number of valid status codes */
  ipp_status_t	statuses[100];		/* Valid status codes */
  int		num_expects;		/* Number of expected attributes */
  _cups_expect_t expects[100],		/* Expected attributes */
		*expect,		/* Current expected attribute */
		*last_expect;		/* Last EXPECT (for predicates) */
  int		num_displayed;		/* Number of displayed attributes */
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
    printf("Unable to open test file %s - %s\n", testfile, strerror(errno));
    return (0);
  }

 /*
  * Connect to the server...
  */

  httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, sizeof(scheme), userpass,
                  sizeof(userpass), server, sizeof(server), &port, resource,
		  sizeof(resource));
  if ((http = httpConnect(server, port)) == NULL)
  {
    printf("Unable to connect to %s on port %d - %s\n", server, port,
           strerror(errno));
    fclose(fp);
    return (0);
  }

 /*
  * Loop on tests...
  */

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
      printf("Unexpected token %s seen on line %d - aborting test!\n", token,
             linenum);
      httpClose(http);
      return (0);
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
          strcasecmp(token, "OF-TYPE") &&
          strcasecmp(token, "SAME-COUNT-AS"))
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
	  printf("Bad version %s seen on line %d - aborting test!\n", token,
		 linenum);
	  httpClose(http);
	  ippDelete(request);
	  return (0);
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
	      puts("    ERROR: resolution tag not yet supported!");
	      break;

	  case IPP_TAG_RANGE :
	      puts("    ERROR: range tag not yet supported!");
	      break;

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
      else if (!strcasecmp(token, "STATUS") &&
               num_statuses < (int)(sizeof(statuses) / sizeof(statuses[0])))
      {
       /*
        * Status...
	*/

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
	  fprintf(stderr, "ipptest: Too many EXPECT's on line %d\n", linenum);
	  httpClose(http);
	  ippDelete(request);
	  return (0);
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
      }
      else if (!strcasecmp(token, "OF-TYPE"))
      {
	get_token(fp, token, sizeof(token), &linenum);

	if (last_expect)
	  last_expect->of_type = strdup(token);
	else
	{
	  fprintf(stderr,
		  "ipptest: OF-TYPE without a preceding EXPECT on line %d\n",
		  linenum);
	  httpClose(http);
	  ippDelete(request);
	  return (0);
	}
      }
      else if (!strcasecmp(token, "SAME-COUNT-AS"))
      {
	get_token(fp, token, sizeof(token), &linenum);

	if (last_expect)
	  last_expect->same_count_as = strdup(token);
	else
	{
	  fprintf(stderr,
		  "ipptest: SAME-COUNT-AS without a preceding EXPECT on line "
		  "%d\n", linenum);
	  httpClose(http);
	  ippDelete(request);
	  return (0);
	}
      }
      else if (!strcasecmp(token, "IF-DEFINED"))
      {
	get_token(fp, token, sizeof(token), &linenum);

	if (last_expect)
	  last_expect->if_defined = strdup(token);
	else
	{
	  fprintf(stderr,
		  "ipptest: IF-DEFINED without a preceding EXPECT on line %d\n",
		  linenum);
	  httpClose(http);
	  ippDelete(request);
	  return (0);
	}
      }
      else if (!strcasecmp(token, "DISPLAY") &&
               num_displayed < (int)(sizeof(displayed) / sizeof(displayed[0])))
      {
       /*
        * Display attributes...
	*/

	get_token(fp, token, sizeof(token), &linenum);
	displayed[num_displayed] = strdup(token);
	num_displayed ++;
      }
      else
      {
	fprintf(stderr,
	        "ipptest: Unexpected token %s seen on line %d - aborting "
	        "test!\n", token, linenum);
	httpClose(http);
	ippDelete(request);
	return (0);
      }
    }

   /*
    * Submit the IPP request...
    */

    request->request.op.version[0]   = version / 10;
    request->request.op.version[1]   = version % 10;
    request->request.op.operation_id = op;
    request->request.op.request_id   = 1;

    if (Verbosity)
    {
      printf("    %s:\n", ippOpString(op));

      for (attrptr = request->attrs; attrptr; attrptr = attrptr->next)
	print_attr(attrptr);
    }

    printf("    %-60.60s [", name);
    fflush(stdout);

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

    if (response == NULL)
    {
      time_t curtime;

      curtime = time(NULL);

      puts("FAIL]");
      printf("        ERROR %04x (%s) @ %s\n", cupsLastError(),
	     cupsLastErrorString(), ctime(&curtime));
      pass = 0;
    }
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

          found = ippFindAttribute(response, expect->name, IPP_TAG_ZERO);

          if ((found == NULL) != expect->not_expect ||
              (found && !expect_matches(expect, found->value_tag)))
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

      if (pass)
      {
	puts("PASS]");
	printf("        RECEIVED: %lu bytes in response\n",
	       (unsigned long)ippLength(response));

        if (Verbosity)
	{
	  for (attrptr = response->attrs;
	       attrptr != NULL;
	       attrptr = attrptr->next)
	  {
	    print_attr(attrptr);
          }
        }
        else if (num_displayed > 0)
	{
	  for (attrptr = response->attrs;
	       attrptr != NULL;
	       attrptr = attrptr->next)
	  {
	    if (attrptr->name)
	    {
	      for (i = 0; i < num_displayed; i ++)
	      {
		if (!strcmp(displayed[i], attrptr->name))
		{
		  print_attr(attrptr);
		  break;
		}
	      }
	    }
	  }
        }
      }
      else
      {
	puts("FAIL]");
	printf("        RECEIVED: %lu bytes in response\n",
	       (unsigned long)ippLength(response));

        if (http->version != HTTP_1_1)
          printf("        BAD HTTP VERSION (%d.%d)\n", http->version / 100,
                 http->version % 100);

	for (i = 0; i < num_statuses; i ++)
          if (response->request.status.status_code == statuses[i])
	    break;

	if (i == num_statuses && num_statuses > 0)
	  puts("        BAD STATUS");

	printf("        status-code = %04x (%s)\n",
	       cupsLastError(), ippErrorString(cupsLastError()));

        for (i = num_expects, expect = expects; i > 0; i --, expect ++)
        {
          if (expect->if_defined && !getenv(expect->if_defined))
            continue;

          found = ippFindAttribute(response, expect->name, IPP_TAG_ZERO);

          if ((found == NULL) != expect->not_expect)
          {
            if (expect->not_expect)
	      printf("        NOT EXPECTED: %s\n", expect->name);
	    else
	      printf("        EXPECTED: %s\n", expect->name);
	  }
          else if (found)
          {
            if (!expect_matches(expect, found->value_tag))
              printf("        EXPECTED: %s of type %s but got %s\n", 
                     expect->name, expect->of_type,
                     ippTagString(found->value_tag));
	    else if (expect->same_count_as)
	    {
	      attrptr = ippFindAttribute(response, expect->same_count_as,
					 IPP_TAG_ZERO);
  
	      if (!attrptr)
		printf("        EXPECTED: %s (%d values) same count as %s "
		       "(not returned)\n", 
		       expect->name, found->num_values, expect->same_count_as);
	      else if (attrptr->num_values != found->num_values)
		printf("        EXPECTED: %s (%d values) same count as %s "
		       "(%d values)\n", 
		       expect->name, found->num_values, expect->same_count_as,
		       attrptr->num_values);
	    }
	  }
        }

	for (attrptr = response->attrs; attrptr != NULL; attrptr = attrptr->next)
	  print_attr(attrptr);
      }

      ippDelete(response);
    }

    for (i = num_expects, expect = expects; i > 0; i --, expect ++)
    {
      free(expect->name);
      if (expect->of_type)
        free(expect->of_type);
      if (expect->same_count_as)
        free(expect->same_count_as);
      if (expect->if_defined)
        free(expect->if_defined);
    }
    if (!pass)
      break;
  }

  fclose(fp);
  httpClose(http);

  return (pass);
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
    else if (ch == '\'' || ch == '\"')
    {
     /*
      * Quoted text...
      */

      quote  = ch;
      bufptr = buf;
      bufend = buf + buflen - 1;

      while ((ch = getc(fp)) != EOF)
	if (ch == quote)
          break;
	else if (bufptr < bufend)
          *bufptr++ = ch;

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

      *bufptr = '\0';
      return (buf);
    }
  }
}


/*
 * 'print_attr()' - Print an attribute on the screen.
 */

static void
print_attr(ipp_attribute_t *attr)	/* I - Attribute to print */
{
  int		i;			/* Looping var */


  if (attr->name == NULL)
  {
    puts("        -- separator --");
    return;
  }

  printf("        %s (%s%s) = ", attr->name,
         attr->num_values > 1 ? "1setOf " : "",
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
	  if (i)
	    putchar(' ');

	  print_col(attr->values[i].collection);
	}
	break;

    default :
	break; /* anti-compiler-warning-code */
  }

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
 * 'usage()' - Show program usage.
 */

static void
usage(void)
{
  fputs("Usage: ipptest [options] URL testfile [ ... testfileN ]\n", stderr);
  fputs("Options:\n", stderr);
  fputs("\n", stderr);
  fputs("-c             Send requests using chunking.\n", stderr);
  fputs("-d name=value  Define variable.\n", stderr);
  fputs("-i seconds     Repeat the last test file with the given interval.\n",
        stderr);
  fputs("-v             Show all attributes in response, even on success.\n",
        stderr);

  exit(1);
}


/*
 * End of "$Id: ipptest.c 7847 2008-08-19 04:22:14Z mike $".
 */
