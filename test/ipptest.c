/*
 * "$Id: ipptest.c,v 1.8.2.6 2002/05/16 14:00:19 mike Exp $"
 *
 *   IPP test command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
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
 *   main()          - Parse options and do tests.
 *   do_tests()      - Do tests as specified in the test file.
 *   get_operation() - Get an IPP opcode from an operation name...
 *   get_status()    - Get an IPP status from a status name...
 *   get_tag()       - Get an IPP value or group tag from a name...
 *   get_token()     - Get a token from a file.
 *   print_attr()    - Print an attribute on the screen.
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


/*
 * Local globals...
 */


/*
 * Local functions...
 */

int		do_tests(const char *, const char *);
ipp_op_t	get_operation(const char *);
ipp_status_t	get_status(const char *);
ipp_tag_t	get_tag(const char *);
char		*get_token(FILE *, char *, int);
void		print_attr(ipp_attribute_t *);


/*
 * 'main()' - Parse options and do tests.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  int		status;			/* Status of tests... */


 /*
  * We need at least:
  *
  *     testipp URL testfile
  */

  if (argc < 3)
  {
    fputs("Usage: testipp URL testfile [ ... testfileN ]\n", stderr);
    return (1);
  }

 /*
  * Run tests...
  */

  for (i = 2, status = 1; status && i < argc; i ++)
    status = status && do_tests(argv[1], argv[i]);

 /*
  * Exit...
  */

  return (!status);
}


/*
 * 'do_tests()' - Do tests as specified in the test file.
 */

int					/* 1 = success, 0 = failure */
do_tests(const char *uri,		/* I - URI to connect on */
         const char *testfile)		/* I - Test file to use */
{
  int		i;			/* Looping var */
  http_t	*http;			/* HTTP connection to server */
  char		method[HTTP_MAX_URI],	/* URI method */
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
  ipp_attribute_t *attrptr;		/* Attribute pointer */
  char		attr[128];		/* Attribute name */
  int		num_statuses;		/* Number of valid status codes */
  ipp_status_t	statuses[100];		/* Valid status codes */
  int		num_expects;		/* Number of expected attributes */
  char		*expects[100];		/* Expected attributes */
  char		name[1024];		/* Name of test */
  char		filename[1024];		/* Filename */
  int		pass;			/* Did we pass the test? */
  int		job_id;			/* Job ID from last operation */


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

  httpSeparate(uri, method, userpass, server, &port, resource);
  if ((http = httpConnect(server, port)) == NULL)
  {
    printf("Unable to connect to %s on port %d - %s\n", server, port,
           strerror(errno));
    return (0);
  }

 /*
  * Loop on tests...
  */

  printf("\"%s\":\n", testfile);
  pass   = 1;
  job_id = 0;

  while (get_token(fp, token, sizeof(token)) != NULL)
  {
   /*
    * Expect an open brace...
    */

    if (strcmp(token, "{") != 0)
    {
      printf("Unexpected token %s seen - aborting test!\n", token);
      httpClose(http);
      return (0);
    }

   /*
    * Initialize things...
    */

    httpSeparate(uri, method, userpass, server, &port, resource);

    request      = ippNew();
    op           = (ipp_op_t)0;
    group        = IPP_TAG_OPERATION;
    value        = IPP_TAG_ZERO;
    num_statuses = 0;
    num_expects  = 0;
    filename[0]  = '\0';

    strcpy(name, testfile);
    if (strrchr(name, '.') != NULL)
      *strrchr(name, '.') = '\0';

   /*
    * Parse until we see a close brace...
    */

    while (get_token(fp, token, sizeof(token)) != NULL)
    {
      if (strcmp(token, "}") == 0)
        break;
      else if (strcasecmp(token, "NAME") == 0)
      {
       /*
        * Name of test...
	*/

	get_token(fp, name, sizeof(name));
      }
      else if (strcasecmp(token, "RESOURCE") == 0)
      {
       /*
        * Resource name...
	*/

	get_token(fp, resource, sizeof(resource));
      }
      else if (strcasecmp(token, "OPERATION") == 0)
      {
       /*
        * Operation...
	*/

	get_token(fp, token, sizeof(token));
	op = get_operation(token);
      }
      else if (strcasecmp(token, "GROUP") == 0)
      {
       /*
        * Attribute group...
	*/

	get_token(fp, token, sizeof(token));
	group = get_tag(token);
      }
      else if (strcasecmp(token, "ATTR") == 0)
      {
       /*
        * Attribute...
	*/

	get_token(fp, token, sizeof(token));
	value = get_tag(token);
	get_token(fp, attr, sizeof(attr));
	get_token(fp, temp, sizeof(temp));

        token[sizeof(token) - 1] = '\0';

        for (tempptr = temp, tokenptr = token;
	     *tempptr && tokenptr < (token + sizeof(token) - 1);)
	  if (*tempptr == '$')
	  {
	   /*
	    * Substitute a string/number...
	    */

            if (strncasecmp(tempptr + 1, "uri", 3) == 0)
	    {
	      strlcpy(tokenptr, uri, sizeof(token) - (tokenptr - token));
	      tempptr += 4;
	    }
	    else if (strncasecmp(tempptr + 1, "method", 6) == 0)
	    {
	      strlcpy(tokenptr, method, sizeof(token) - (tokenptr - token));
	      tempptr += 7;
	    }
	    else if (strncasecmp(tempptr + 1, "username", 8) == 0)
	    {
	      strlcpy(tokenptr, userpass, sizeof(token) - (tokenptr - token));
	      tempptr += 9;
	    }
	    else if (strncasecmp(tempptr + 1, "hostname", 8) == 0)
	    {
	      strlcpy(tokenptr, server, sizeof(token) - (tokenptr - token));
	      tempptr += 9;
	    }
	    else if (strncasecmp(tempptr + 1, "port", 4) == 0)
	    {
	      snprintf(tokenptr, sizeof(token) - (tokenptr - token),
	               "%d", port);
	      tempptr += 5;
	    }
	    else if (strncasecmp(tempptr + 1, "resource", 8) == 0)
	    {
	      strlcpy(tokenptr, resource, sizeof(token) - (tokenptr - token));
	      tempptr += 9;
	    }
	    else if (strncasecmp(tempptr + 1, "job-id", 6) == 0)
	    {
	      snprintf(tokenptr, sizeof(token) - (tokenptr - token),
	               "%d", job_id);
	      tempptr += 7;
	    }
	    else if (strncasecmp(tempptr + 1, "user", 4) == 0)
	    {
	      strlcpy(tokenptr, cupsUser(), sizeof(token) - (tokenptr - token));
	      tempptr += 5;
	    }
            else
	    {
	      *tokenptr++ = *tempptr ++;
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
	      if (strcasecmp(token, "true") == 0)
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
	      ippAddString(request, group, value, attr, NULL, token);
	      break;
	}
      }
      else if (strcasecmp(token, "FILE") == 0)
      {
       /*
        * File...
	*/

	get_token(fp, filename, sizeof(filename));
      }
      else if (strcasecmp(token, "STATUS") == 0)
      {
       /*
        * Status...
	*/

	get_token(fp, token, sizeof(token));
	statuses[num_statuses] = get_status(token);
	num_statuses ++;
      }
      else if (strcasecmp(token, "EXPECT") == 0)
      {
       /*
        * Status...
	*/

	get_token(fp, token, sizeof(token));
	expects[num_expects] = strdup(token);
	num_expects ++;
      }
      else
      {
	printf("Unexpected token %s seen - aborting test!\n", token);
	httpClose(http);
	ippDelete(request);
	return (0);
      }
    }

   /*
    * Submit the IPP request...
    */

    request->request.op.operation_id = op;
    request->request.op.request_id   = 1;

    printf("    %-60.60s [    ]", name);
    fflush(stdout);

    if (filename[0])
      response = cupsDoFileRequest(http, request, resource, filename);
    else
      response = cupsDoRequest(http, request, resource);

    if (response == NULL)
    {
      time_t curtime;

      curtime = time(NULL);

      printf("\b\b\b\b\bFAIL]\n");
      printf("        ERROR %x\n", cupsLastError());
      printf("        (%s) @ %s\n",
	     ippErrorString(cupsLastError()), ctime(&curtime));
      pass = 0;
    }
    else
    {
      if ((attrptr = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) != NULL)
        job_id = attrptr->values[0].integer;

      for (i = 0; i < num_statuses; i ++)
        if (response->request.status.status_code == statuses[i])
	  break;

      if (i == num_statuses && num_statuses > 0)
      {
	printf("\b\b\b\b\bFAIL]\n");
        printf("        STATUS %x\n", response->request.status.status_code);
        printf("        (%s)\n",
	       ippErrorString(response->request.status.status_code));
	printf("        (%lu bytes in response)\n",
	       (unsigned long)ippLength(response));

	pass = 0;
      }
      else
      {
        for (i = 0; i < num_expects; i ++)
	  if (ippFindAttribute(response, expects[i], IPP_TAG_ZERO) == NULL)
	  {
	    if (pass)
	    {
	      printf("\b\b\b\b\bFAIL]\n");
	      printf("        (%lu bytes in response)\n",
	             (unsigned long)ippLength(response));
	      pass = 0;
	    }

	    printf("        EXPECTED %s\n", expects[i]);
	  }

	if (pass)
	{
	  printf("\b\b\b\b\bPASS]\n");
	  printf("        (%lu bytes in response)\n",
	         (unsigned long)ippLength(response));
	}
	else
	{
	  puts("        RECEIVED");
	  printf("        status-code = %04x\n",
	         response->request.status.status_code);

	  for (attrptr = response->attrs; attrptr != NULL; attrptr = attrptr->next)
	    print_attr(attrptr);
	}
      }

      ippDelete(response);
    }

    for (i = 0; i < num_expects; i ++)
      free(expects[i]);

    if (!pass)
      break;
  }

  fclose(fp);
  httpClose(http);

  return (pass);
}


/*
 * 'get_operation()' - Get an IPP opcode from an operation name...
 */

ipp_op_t
get_operation(const char *name)
{
  int		i;
  static char	*ipp_ops[] =
		{
		  "",
		  "",
		  "print-job",
		  "print-uri",
		  "validate-job",
		  "create-job",
		  "send-document",
		  "send-uri",
		  "cancel-job",
		  "get-job-attributes",
		  "get-jobs",
		  "get-printer-attributes",
		  "hold-job",
		  "release-job",
		  "restart-job",
		  "",
		  "pause-printer",
		  "resume-printer",
		  "purge-jobs",
		  "set-printer-attributes",
		  "set-job-attributes",
		  "get-printer-supported-values"
		},
		*cups_ops[] =
		{
		  "cups-get-default",
		  "cups-get-printers",
		  "cups-add-printer",
		  "cups-delete-printer",
		  "cups-get-classes",
		  "cups-add-class",
		  "cups-delete-class",
		  "cups-accept-jobs",
		  "cups-reject-jobs",
		  "cups-set-default",
		  "cups-get-devices",
		  "cups-get-ppds",
		  "cups-move-job"
		};


  for (i = 0; i < (sizeof(ipp_ops) / sizeof(ipp_ops[0])); i ++)
    if (strcasecmp(name, ipp_ops[i]) == 0)
      return ((ipp_op_t)i);

  for (i = 0; i < (sizeof(cups_ops) / sizeof(cups_ops[0])); i ++)
    if (strcasecmp(name, cups_ops[i]) == 0)
      return ((ipp_op_t)(i + 0x4001));

  return ((ipp_op_t)0);
}


/*
 * 'get_status()' - Get an IPP status from a status name...
 */

ipp_status_t
get_status(const char *name)
{
  int		i;
  static const char *status_oks[] =	/* "OK" status codes */
		{
		  "successful-ok",
		  "successful-ok-ignored-or-substituted-attributes",
		  "successful-ok-conflicting-attributes"
		},
		*status_400s[] =	/* Client errors */
		{
		  "client-error-bad-request",
		  "client-error-forbidden",
		  "client-error-not-authenticated",
		  "client-error-not-authorized",
		  "client-error-not-possible",
		  "client-error-timeout",
		  "client-error-not-found",
		  "client-error-gone",
		  "client-error-request-entity-too-large",
		  "client-error-request-value-too-long",
		  "client-error-document-format-not-supported",
		  "client-error-attributes-or-values-not-supported",
		  "client-error-uri-scheme-not-supported",
		  "client-error-charset-not-supported",
		  "client-error-conflicting-attributes",
		  "client-error-compression-not-supported",
		  "client-error-compression-error",
		  "client-error-document-format-error",
		  "client-error-document-access-error"
		},
		*status_500s[] =	/* Server errors */
		{
		  "server-error-internal-error",
		  "server-error-operation-not-supported",
		  "server-error-service-unavailable",
		  "server-error-version-not-supported",
		  "server-error-device-error",
		  "server-error-temporary-error",
		  "server-error-not-accepting-jobs",
		  "server-error-busy",
		  "server-error-job-canceled",
		  "server-error-multiple-document-jobs-not-supported"
		};


  for (i = 0; i < (sizeof(status_oks) / sizeof(status_oks[0])); i ++)
    if (strcasecmp(name, status_oks[i]) == 0)
      return ((ipp_status_t)i);

  for (i = 0; i < (sizeof(status_400s) / sizeof(status_400s[0])); i ++)
    if (strcasecmp(name, status_400s[i]) == 0)
      return ((ipp_status_t)(i + 0x400));

  for (i = 0; i < (sizeof(status_500s) / sizeof(status_500s[0])); i ++)
    if (strcasecmp(name, status_500s[i]) == 0)
      return ((ipp_status_t)(i + 0x500));

  return ((ipp_status_t)-1);
}


/*
 * 'get_tag()' - Get an IPP value or group tag from a name...
 */

ipp_tag_t
get_tag(const char *name)
{
  int		i;
  static char	*names[] =
		{
		  "zero", "operation", "job", "end", "printer",
		  "unsupported-group", "", "", "", "", "", "", "",
		  "", "", "", "unsupported-value", "default",
		  "unknown", "novalue", "", "notsettable",
		  "deleteattr", "anyvalue", "", "", "", "", "", "",
		  "", "", "", "integer", "boolean", "enum", "", "",
		  "", "", "", "", "", "", "", "", "", "", "string",
		  "date", "resolution", "range", "collection",
		  "textlang", "namelang", "", "", "", "", "", "", "",
		  "", "", "", "text", "name", "", "keyword", "uri",
		  "urischeme", "charset", "language", "mimetype"
		};


  for (i = 0; i < (sizeof(names) / sizeof(names[0])); i ++)
    if (strcasecmp(name, names[i]) == 0)
      return ((ipp_tag_t)i);

  return (IPP_TAG_ZERO);
}


/*
 * 'get_token()' - Get a token from a file.
 */

char *				/* O - Token from file or NULL on EOF */
get_token(FILE *fp,		/* I - File to read from */
          char *buf,		/* I - Buffer to read into */
	  int  buflen)		/* I - Length of buffer */
{
  int	ch,			/* Character from file */
	quote;			/* Quoting character */
  char	*bufptr,		/* Pointer into buffer */
	*bufend;		/* End of buffer */


  for (;;)
  {
   /*
    * Skip whitespace...
    */

    while (isspace(ch = getc(fp)));

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

void
print_attr(ipp_attribute_t *attr)	/* I - Attribute to print */
{
  int		i;			/* Looping var */


  if (attr->name == NULL)
  {
    puts("        -- separator --");
    return;
  }

  printf("        %s = ", attr->name);

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

    default :
	break; /* anti-compiler-warning-code */
  }

  putchar('\n');
}


/*
 * End of "$Id: ipptest.c,v 1.8.2.6 2002/05/16 14:00:19 mike Exp $".
 */
