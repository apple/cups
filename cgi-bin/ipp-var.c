/*
 * "$Id: ipp-var.c 7940 2008-09-16 00:45:16Z mike $"
 *
 *   CGI <-> IPP variable routines for CUPS.
 *
 *   Copyright 2007-2012 by Apple Inc.
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
 *   cgiGetAttributes()    - Get the list of attributes that are needed by the
 *                           template file.
 *   cgiGetIPPObjects()    - Get the objects in an IPP response.
 *   cgiMoveJobs()         - Move one or more jobs.
 *   cgiPrintCommand()     - Print a CUPS command job.
 *   cgiPrintTestPage()    - Print a test page.
 *   cgiRewriteURL()       - Rewrite a printer URI into a web browser URL...
 *   cgiSetIPPObjectVars() - Set CGI variables from an IPP object.
 *   cgiSetIPPVars()       - Set CGI variables from an IPP response.
 *   cgiShowIPPError()     - Show the last IPP error message.
 *   cgiShowJobs()         - Show print jobs.
 *   cgiText()             - Return localized text.
 */

/*
 * Include necessary headers...
 */

#include "cgi-private.h"


/*
 * 'cgiGetAttributes()' - Get the list of attributes that are needed
 *                        by the template file.
 */

void
cgiGetAttributes(ipp_t      *request,	/* I - IPP request */
                 const char *tmpl)	/* I - Base filename */
{
  int		num_attrs;		/* Number of attributes */
  char		*attrs[1000];		/* Attributes */
  int		i;			/* Looping var */
  char		filename[1024],		/* Filename */
		locale[16];		/* Locale name */
  const char	*directory,		/* Directory */
		*lang;			/* Language */
  FILE		*in;			/* Input file */
  int		ch;			/* Character from file */
  char		name[255],		/* Name of variable */
		*nameptr;		/* Pointer into name */


 /*
  * Convert the language to a locale name...
  */

  if ((lang = getenv("LANG")) != NULL)
  {
    for (i = 0; lang[i] && i < 15; i ++)
      if (isalnum(lang[i] & 255))
        locale[i] = tolower(lang[i]);
      else
        locale[i] = '_';

    locale[i] = '\0';
  }
  else
    locale[0] = '\0';

 /*
  * See if we have a template file for this language...
  */

  directory = cgiGetTemplateDir();

  snprintf(filename, sizeof(filename), "%s/%s/%s", directory, locale, tmpl);
  if (access(filename, 0))
  {
    locale[2] = '\0';

    snprintf(filename, sizeof(filename), "%s/%s/%s", directory, locale, tmpl);
    if (access(filename, 0))
      snprintf(filename, sizeof(filename), "%s/%s", directory, tmpl);
  }

 /*
  * Open the template file...
  */

  if ((in = fopen(filename, "r")) == NULL)
    return;

 /*
  * Loop through the file adding attribute names as needed...
  */

  num_attrs = 0;
  attrs[0]  = NULL;			/* Eliminate compiler warning */

  while ((ch = getc(in)) != EOF)
    if (ch == '\\')
      getc(in);
    else if (ch == '{' && num_attrs < (sizeof(attrs) / sizeof(attrs[0])))
    {
     /*
      * Grab the name...
      */

      for (nameptr = name; (ch = getc(in)) != EOF;)
        if (strchr("}]<>=!~ \t\n", ch))
          break;
        else if (nameptr > name && ch == '?')
	  break;
	else if (nameptr < (name + sizeof(name) - 1))
	{
	  if (ch == '_')
	    *nameptr++ = '-';
	  else
            *nameptr++ = ch;
	}

      *nameptr = '\0';

      if (!strncmp(name, "printer_state_history", 21))
        strcpy(name, "printer_state_history");

     /*
      * Possibly add it to the list of attributes...
      */

      for (i = 0; i < num_attrs; i ++)
        if (!strcmp(attrs[i], name))
	  break;

      if (i >= num_attrs)
      {
	attrs[num_attrs] = strdup(name);
	num_attrs ++;
      }
    }

 /*
  * If we have attributes, add a requested-attributes attribute to the
  * request...
  */

  if (num_attrs > 0)
  {
    ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                  "requested-attributes", num_attrs, NULL, (const char **)attrs);

    for (i = 0; i < num_attrs; i ++)
      free(attrs[i]);
  }

  fclose(in);
}


/*
 * 'cgiGetIPPObjects()' - Get the objects in an IPP response.
 */

cups_array_t *				/* O - Array of objects */
cgiGetIPPObjects(ipp_t *response,	/* I - IPP response */
                 void  *search)		/* I - Search filter */
{
  int			i;		/* Looping var */
  cups_array_t		*objs;		/* Array of objects */
  ipp_attribute_t	*attr,		/* Current attribute */
			*first;		/* First attribute for object */
  ipp_tag_t		group;		/* Current group tag */
  int			add;		/* Add this object to the array? */


  if (!response)
    return (0);

  for (add = 0, first = NULL, objs = cupsArrayNew(NULL, NULL),
           group = IPP_TAG_ZERO, attr = response->attrs;
       attr;
       attr = attr->next)
  {
    if (attr->group_tag != group)
    {
      group = attr->group_tag;

      if (group != IPP_TAG_ZERO && group != IPP_TAG_OPERATION)
      {
        first = attr;
	add   = 0;
      }
      else if (add && first)
      {
        cupsArrayAdd(objs, first);

	add   = 0;
	first = NULL;
      }
    }

    if (attr->name && attr->group_tag != IPP_TAG_OPERATION && !add)
    {
      if (!search)
      {
       /*
        * Add all objects if there is no search...
	*/

        add = 1;
      }
      else
      {
       /*
        * Check the search string against the string and integer values.
	*/

        switch (attr->value_tag)
	{
	  case IPP_TAG_TEXTLANG :
	  case IPP_TAG_NAMELANG :
	  case IPP_TAG_TEXT :
	  case IPP_TAG_NAME :
	  case IPP_TAG_KEYWORD :
	  case IPP_TAG_URI :
	  case IPP_TAG_MIMETYPE :
	      for (i = 0; !add && i < attr->num_values; i ++)
		if (cgiDoSearch(search, attr->values[i].string.text))
		  add = 1;
	      break;

          case IPP_TAG_INTEGER :
	      for (i = 0; !add && i < attr->num_values; i ++)
	      {
	        char	buf[255];	/* Number buffer */


                sprintf(buf, "%d", attr->values[i].integer);

		if (cgiDoSearch(search, buf))
		  add = 1;
	      }
	      break;

          default :
	      break;
	}
      }
    }
  }

  if (add && first)
    cupsArrayAdd(objs, first);

  return (objs);
}


/*
 * 'cgiMoveJobs()' - Move one or more jobs.
 *
 * At least one of dest or job_id must be non-zero/NULL.
 */

void
cgiMoveJobs(http_t     *http,		/* I - Connection to server */
            const char *dest,		/* I - Destination or NULL */
            int        job_id)		/* I - Job ID or 0 for all */
{
  int		i;			/* Looping var */
  const char	*user;			/* Username */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* Current attribute */
  const char	*name;			/* Destination name */
  const char	*job_printer_uri;	/* JOB_PRINTER_URI form variable */
  char		current_dest[1024];	/* Current destination */


 /*
  * Make sure we have a username...
  */

  if ((user = getenv("REMOTE_USER")) == NULL)
  {
    puts("Status: 401\n");
    exit(0);
  }

 /*
  * See if the user has already selected a new destination...
  */

  if ((job_printer_uri = cgiGetVariable("JOB_PRINTER_URI")) == NULL)
  {
   /*
    * Make sure necessary form variables are set...
    */

    if (job_id)
    {
      char	temp[255];		/* Temporary string */


      sprintf(temp, "%d", job_id);
      cgiSetVariable("JOB_ID", temp);
    }

    if (dest)
      cgiSetVariable("PRINTER_NAME", dest);

   /*
    * No new destination specified, show the user what the available
    * printers/classes are...
    */

    if (!dest)
    {
     /*
      * Get the current destination for job N...
      */

      char	job_uri[1024];		/* Job URI */


      request = ippNewRequest(IPP_GET_JOB_ATTRIBUTES);

      snprintf(job_uri, sizeof(job_uri), "ipp://localhost/jobs/%d", job_id);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri",
                   NULL, job_uri);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                   "requested-attributes", NULL, "job-printer-uri");

      if ((response = cupsDoRequest(http, request, "/")) != NULL)
      {
        if ((attr = ippFindAttribute(response, "job-printer-uri",
	                             IPP_TAG_URI)) != NULL)
	{
	 /*
	  * Pull the name from the URI...
	  */

	  strlcpy(current_dest, strrchr(attr->values[0].string.text, '/') + 1,
	          sizeof(current_dest));
          dest = current_dest;
	}

        ippDelete(response);
      }

      if (!dest)
      {
       /*
        * Couldn't get the current destination...
	*/

        cgiStartHTML(cgiText(_("Move Job")));
	cgiShowIPPError(_("Unable to find destination for job"));
	cgiEndHTML();
	return;
      }
    }

   /*
    * Get the list of available destinations...
    */

    request = ippNewRequest(CUPS_GET_PRINTERS);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                 "requested-attributes", NULL, "printer-uri-supported");

    if (user)
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
		   "requesting-user-name", NULL, user);

    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_ENUM, "printer-type",
                  CUPS_PRINTER_LOCAL);
    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_ENUM, "printer-type-mask",
                  CUPS_PRINTER_SCANNER);

    if ((response = cupsDoRequest(http, request, "/")) != NULL)
    {
      for (i = 0, attr = ippFindAttribute(response, "printer-uri-supported",
                                          IPP_TAG_URI);
           attr;
	   attr = ippFindNextAttribute(response, "printer-uri-supported",
	                               IPP_TAG_URI))
      {
       /*
	* Pull the name from the URI...
	*/

	name = strrchr(attr->values[0].string.text, '/') + 1;

       /*
        * If the name is not the same as the current destination, add it!
	*/

        if (_cups_strcasecmp(name, dest))
	{
	  cgiSetArray("JOB_PRINTER_URI", i, attr->values[0].string.text);
	  cgiSetArray("JOB_PRINTER_NAME", i, name);
	  i ++;
	}
      }

      ippDelete(response);
    }

   /*
    * Show the form...
    */

    if (job_id)
      cgiStartHTML(cgiText(_("Move Job")));
    else
      cgiStartHTML(cgiText(_("Move All Jobs")));

    if (cgiGetSize("JOB_PRINTER_NAME") > 0)
      cgiCopyTemplateLang("job-move.tmpl");
    else
    {
      if (job_id)
	cgiSetVariable("MESSAGE", cgiText(_("Unable to move job")));
      else
	cgiSetVariable("MESSAGE", cgiText(_("Unable to move jobs")));

      cgiSetVariable("ERROR", cgiText(_("No destinations added.")));
      cgiCopyTemplateLang("error.tmpl");
    }
  }
  else
  {
   /*
    * Try moving the job or jobs...
    */

    char	uri[1024],		/* Job/printer URI */
		resource[1024],		/* Post resource */
		refresh[1024];		/* Refresh URL */
    const char	*job_printer_name;	/* New printer name */


    request = ippNewRequest(CUPS_MOVE_JOB);

    if (job_id)
    {
     /*
      * Move 1 job...
      */

      snprintf(resource, sizeof(resource), "/jobs/%d", job_id);

      snprintf(uri, sizeof(uri), "ipp://localhost/jobs/%d", job_id);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri",
                   NULL, uri);
    }
    else
    {
     /*
      * Move all active jobs on a destination...
      */

      snprintf(resource, sizeof(resource), "/%s/%s",
               cgiGetVariable("SECTION"), dest);

      httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                       "localhost", ippPort(), "/%s/%s",
		       cgiGetVariable("SECTION"), dest);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                   NULL, uri);
    }

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-printer-uri",
                 NULL, job_printer_uri);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                 "requesting-user-name", NULL, user);

    ippDelete(cupsDoRequest(http, request, resource));

   /*
    * Show the results...
    */

    job_printer_name = strrchr(job_printer_uri, '/') + 1;

    if (cupsLastError() <= IPP_OK_CONFLICT)
    {
      const char *path = strstr(job_printer_uri, "/printers/");
      if (!path)
      {
        path = strstr(job_printer_uri, "/classes/");
        cgiSetVariable("IS_CLASS", "YES");
      }

      if (path)
      {
        cgiFormEncode(uri, path, sizeof(uri));
        snprintf(refresh, sizeof(refresh), "2;URL=%s", uri);
	cgiSetVariable("refresh_page", refresh);
      }
    }

    if (job_id)
      cgiStartHTML(cgiText(_("Move Job")));
    else
      cgiStartHTML(cgiText(_("Move All Jobs")));

    if (cupsLastError() > IPP_OK_CONFLICT)
    {
      if (job_id)
	cgiShowIPPError(_("Unable to move job"));
      else
        cgiShowIPPError(_("Unable to move jobs"));
    }
    else
    {
      cgiSetVariable("JOB_PRINTER_NAME", job_printer_name);
      cgiCopyTemplateLang("job-moved.tmpl");
    }
  }

  cgiEndHTML();
}


/*
 * 'cgiPrintCommand()' - Print a CUPS command job.
 */

void
cgiPrintCommand(http_t     *http,	/* I - Connection to server */
                const char *dest,	/* I - Destination printer */
                const char *command,	/* I - Command to send */
		const char *title)	/* I - Page/job title */
{
  int		job_id;			/* Command file job */
  char		uri[HTTP_MAX_URI],	/* Job URI */
		resource[1024],		/* Printer resource path */
		refresh[1024],		/* Refresh URL */
		command_file[1024];	/* Command "file" */
  http_status_t	status;			/* Document status */
  cups_option_t	hold_option;		/* job-hold-until option */
  const char	*user;			/* User name */
  ipp_t		*request,		/* Get-Job-Attributes request */
		*response;		/* Get-Job-Attributes response */
  ipp_attribute_t *attr;		/* Current job attribute */
  static const char const *job_attrs[] =/* Job attributes we want */
		{
		  "job-state",
		  "job-printer-state-message"
		};


 /*
  * Create the CUPS command file...
  */

  snprintf(command_file, sizeof(command_file), "#CUPS-COMMAND\n%s\n", command);

 /*
  * Show status...
  */

  if (cgiSupportsMultipart())
  {
    cgiStartMultipart();
    cgiStartHTML(title);
    cgiCopyTemplateLang("command.tmpl");
    cgiEndHTML();
    fflush(stdout);
  }

 /*
  * Send the command file job...
  */

  hold_option.name  = "job-hold-until";
  hold_option.value = "no-hold";

  if ((user = getenv("REMOTE_USER")) != NULL)
    cupsSetUser(user);
  else
    cupsSetUser("anonymous");

  if ((job_id = cupsCreateJob(http, dest, title,
			      1, &hold_option)) < 1)
  {
    cgiSetVariable("MESSAGE", cgiText(_("Unable to send command to printer driver")));
    cgiSetVariable("ERROR", cupsLastErrorString());
    cgiStartHTML(title);
    cgiCopyTemplateLang("error.tmpl");
    cgiEndHTML();

    if (cgiSupportsMultipart())
      cgiEndMultipart();
    return;
  }

  status = cupsStartDocument(http, dest, job_id, NULL, CUPS_FORMAT_COMMAND, 1);
  if (status == HTTP_CONTINUE)
    status = cupsWriteRequestData(http, command_file,
				  strlen(command_file));
  if (status == HTTP_CONTINUE)
    cupsFinishDocument(http, dest);

  if (cupsLastError() >= IPP_REDIRECTION_OTHER_SITE)
  {
    cgiSetVariable("MESSAGE", cgiText(_("Unable to send command to printer driver")));
    cgiSetVariable("ERROR", cupsLastErrorString());
    cgiStartHTML(title);
    cgiCopyTemplateLang("error.tmpl");
    cgiEndHTML();

    if (cgiSupportsMultipart())
      cgiEndMultipart();

    cupsCancelJob(dest, job_id);
    return;
  }

 /*
  * Wait for the job to complete...
  */

  if (cgiSupportsMultipart())
  {
    for (;;)
    {
     /*
      * Get the current job state...
      */

      snprintf(uri, sizeof(uri), "ipp://localhost/jobs/%d", job_id);
      request = ippNewRequest(IPP_GET_JOB_ATTRIBUTES);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri",
		   NULL, uri);
      if (user)
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
		     "requesting-user-name", NULL, user);
      ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
		    "requested-attributes", 2, NULL, job_attrs);

      if ((response = cupsDoRequest(http, request, "/")) != NULL)
	cgiSetIPPVars(response, NULL, NULL, NULL, 0);

      attr = ippFindAttribute(response, "job-state", IPP_TAG_ENUM);
      if (!attr || attr->values[0].integer >= IPP_JOB_STOPPED ||
          attr->values[0].integer == IPP_JOB_HELD)
      {
	ippDelete(response);
	break;
      }

     /*
      * Job not complete, so update the status...
      */

      ippDelete(response);

      cgiStartHTML(title);
      cgiCopyTemplateLang("command.tmpl");
      cgiEndHTML();
      fflush(stdout);

      sleep(5);
    }
  }

 /*
  * Send the final page that reloads the printer's page...
  */

  snprintf(resource, sizeof(resource), "/printers/%s", dest);

  cgiFormEncode(uri, resource, sizeof(uri));
  snprintf(refresh, sizeof(refresh), "5;URL=%s", uri);
  cgiSetVariable("refresh_page", refresh);

  cgiStartHTML(title);
  cgiCopyTemplateLang("command.tmpl");
  cgiEndHTML();

  if (cgiSupportsMultipart())
    cgiEndMultipart();
}


/*
 * 'cgiPrintTestPage()' - Print a test page.
 */

void
cgiPrintTestPage(http_t     *http,	/* I - Connection to server */
                 const char *dest)	/* I - Destination printer/class */
{
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  char		uri[HTTP_MAX_URI],	/* Printer URI */
		resource[1024],		/* POST resource path */
		refresh[1024],		/* Refresh URL */
		filename[1024];		/* Test page filename */
  const char	*datadir;		/* CUPS_DATADIR env var */
  const char	*user;			/* Username */


 /*
  * See who is logged in...
  */

  user = getenv("REMOTE_USER");

 /*
  * Locate the test page file...
  */

  if ((datadir = getenv("CUPS_DATADIR")) == NULL)
    datadir = CUPS_DATADIR;

  snprintf(filename, sizeof(filename), "%s/data/testprint", datadir);

 /*
  * Point to the printer/class...
  */

  snprintf(resource, sizeof(resource), "/%s/%s", cgiGetVariable("SECTION"),
           dest);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", ippPort(), "/%s/%s", cgiGetVariable("SECTION"),
		   dest);

 /*
  * Build an IPP_PRINT_JOB request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requesting-user-name
  */

  request = ippNewRequest(IPP_PRINT_JOB);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

  if (user)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
		 "requesting-user-name", NULL, user);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name",
               NULL, "Test Page");

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoFileRequest(http, request, resource,
                                    filename)) != NULL)
  {
    cgiSetIPPVars(response, NULL, NULL, NULL, 0);

    ippDelete(response);
  }

  if (cupsLastError() <= IPP_OK_CONFLICT)
  {
   /*
    * Automatically reload the printer status page...
    */

    cgiFormEncode(uri, resource, sizeof(uri));
    snprintf(refresh, sizeof(refresh), "2;URL=%s", uri);
    cgiSetVariable("refresh_page", refresh);
  }
  else if (cupsLastError() == IPP_NOT_AUTHORIZED)
  {
    puts("Status: 401\n");
    exit(0);
  }

  cgiStartHTML(cgiText(_("Print Test Page")));

  if (cupsLastError() > IPP_OK_CONFLICT)
    cgiShowIPPError(_("Unable to print test page"));
  else
  {
    cgiSetVariable("PRINTER_NAME", dest);

    cgiCopyTemplateLang("test-page.tmpl");
  }

  cgiEndHTML();
}


/*
 * 'cgiRewriteURL()' - Rewrite a printer URI into a web browser URL...
 */

char *					/* O - New URL */
cgiRewriteURL(const char *uri,		/* I - Current URI */
              char       *url,		/* O - New URL */
	      int        urlsize,	/* I - Size of URL buffer */
	      const char *newresource)	/* I - Replacement resource */
{
  char			scheme[HTTP_MAX_URI],
			userpass[HTTP_MAX_URI],
			hostname[HTTP_MAX_URI],
			rawresource[HTTP_MAX_URI],
			resource[HTTP_MAX_URI],
					/* URI components... */
			*rawptr,	/* Pointer into rawresource */
			*resptr;	/* Pointer into resource */
  int			port;		/* Port number */
  static int		ishttps = -1;	/* Using encryption? */
  static const char	*server;	/* Name of server */
  static char		servername[1024];
					/* Local server name */
  static const char	hexchars[] = "0123456789ABCDEF";
					/* Hexadecimal conversion characters */


 /*
  * Check if we have been called before...
  */

  if (ishttps < 0)
  {
   /*
    * No, initialize static vars for the conversion...
    *
    * First get the server name associated with the client interface as
    * well as the locally configured hostname.  We'll check *both* of
    * these to see if the printer URL is local...
    */

    if ((server = getenv("SERVER_NAME")) == NULL)
      server = "";

    httpGetHostname(NULL, servername, sizeof(servername));

   /*
    * Then flag whether we are using SSL on this connection...
    */

    ishttps = getenv("HTTPS") != NULL;
  }

 /*
  * Convert the URI to a URL...
  */

  httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, sizeof(scheme), userpass,
                  sizeof(userpass), hostname, sizeof(hostname), &port,
		  rawresource, sizeof(rawresource));

  if (!strcmp(scheme, "ipp") ||
      !strcmp(scheme, "http") ||
      !strcmp(scheme, "https"))
  {
    if (newresource)
    {
     /*
      * Force the specified resource name instead of the one in the URL...
      */

      strlcpy(resource, newresource, sizeof(resource));
    }
    else
    {
     /*
      * Rewrite the resource string so it doesn't contain any
      * illegal chars...
      */

      for (rawptr = rawresource, resptr = resource; *rawptr; rawptr ++)
	if ((*rawptr & 128) || *rawptr == '%' || *rawptr == ' ' ||
	    *rawptr == '#' || *rawptr == '?' ||
	    *rawptr == '.') /* For MSIE */
	{
	  if (resptr < (resource + sizeof(resource) - 3))
	  {
	    *resptr++ = '%';
	    *resptr++ = hexchars[(*rawptr >> 4) & 15];
	    *resptr++ = hexchars[*rawptr & 15];
	  }
	}
	else if (resptr < (resource + sizeof(resource) - 1))
	  *resptr++ = *rawptr;

      *resptr = '\0';
    }

   /*
    * Map local access to a local URI...
    */

    if (!_cups_strcasecmp(hostname, "127.0.0.1") ||
	!_cups_strcasecmp(hostname, "[::1]") ||
	!_cups_strcasecmp(hostname, "localhost") ||
	!_cups_strncasecmp(hostname, "localhost.", 10) ||
	!_cups_strcasecmp(hostname, server) ||
	!_cups_strcasecmp(hostname, servername))
    {
     /*
      * Make URI relative to the current server...
      */

      strlcpy(url, resource, urlsize);
    }
    else
    {
     /*
      * Rewrite URI with HTTP/HTTPS scheme...
      */

      if (userpass[0])
	snprintf(url, urlsize, "%s://%s@%s:%d%s",
		 ishttps ? "https" : "http",
		 userpass, hostname, port, resource);
      else
	snprintf(url, urlsize, "%s://%s:%d%s",
		 ishttps ? "https" : "http",
		 hostname, port, resource);
    }
  }
  else
    strlcpy(url, uri, urlsize);

  return (url);
}


/*
 * 'cgiSetIPPObjectVars()' - Set CGI variables from an IPP object.
 */

ipp_attribute_t *			/* O - Next object */
cgiSetIPPObjectVars(
    ipp_attribute_t *obj,		/* I - Response data to be copied... */
    const char      *prefix,		/* I - Prefix for name or NULL */
    int             element)		/* I - Parent element number */
{
  ipp_attribute_t	*attr;		/* Attribute in response... */
  int			i;		/* Looping var */
  char			name[1024],	/* Name of attribute */
			*nameptr,	/* Pointer into name */
			value[16384],	/* Value(s) */
			*valptr;	/* Pointer into value */
  struct tm		*date;		/* Date information */


  fprintf(stderr, "DEBUG2: cgiSetIPPObjectVars(obj=%p, prefix=\"%s\", "
                  "element=%d)\n",
          obj, prefix ? prefix : "(null)", element);

 /*
  * Set common CGI template variables...
  */

  if (!prefix)
    cgiSetServerVersion();

 /*
  * Loop through the attributes and set them for the template...
  */

  for (attr = obj; attr && attr->group_tag != IPP_TAG_ZERO; attr = attr->next)
  {
   /*
    * Copy the attribute name, substituting "_" for "-"...
    */

    if (!attr->name)
      continue;

    if (prefix)
    {
      snprintf(name, sizeof(name), "%s.", prefix);
      nameptr = name + strlen(name);
    }
    else
      nameptr = name;

    for (i = 0; attr->name[i] && nameptr < (name + sizeof(name) - 1); i ++)
      if (attr->name[i] == '-')
	*nameptr++ = '_';
      else
        *nameptr++ = attr->name[i];

    *nameptr = '\0';

   /*
    * Add "job_printer_name" variable if we have a "job_printer_uri"
    * attribute...
    */

    if (!strcmp(name, "job_printer_uri"))
    {
      if ((valptr = strrchr(attr->values[0].string.text, '/')) == NULL)
	valptr = "unknown";
      else
	valptr ++;

      cgiSetArray("job_printer_name", element, valptr);
    }

   /*
    * Localize event names in "notify_events" variable...
    */

    if (!strcmp(name, "notify_events"))
    {
      size_t	remaining;		/* Remaining bytes in buffer */


      value[0] = '\0';
      valptr   = value;

      for (i = 0; i < attr->num_values; i ++)
      {
        if (valptr >= (value + sizeof(value) - 3))
	  break;

        if (i)
	{
	  *valptr++ = ',';
	  *valptr++ = ' ';
        }

        remaining = sizeof(value) - (valptr - value);

        if (!strcmp(attr->values[i].string.text, "printer-stopped"))
	  strlcpy(valptr, _("Printer Paused"), remaining);
	else if (!strcmp(attr->values[i].string.text, "printer-added"))
	  strlcpy(valptr, _("Printer Added"), remaining);
	else if (!strcmp(attr->values[i].string.text, "printer-modified"))
	  strlcpy(valptr, _("Printer Modified"), remaining);
	else if (!strcmp(attr->values[i].string.text, "printer-deleted"))
	  strlcpy(valptr, _("Printer Deleted"), remaining);
	else if (!strcmp(attr->values[i].string.text, "job-created"))
	  strlcpy(valptr, _("Job Created"), remaining);
	else if (!strcmp(attr->values[i].string.text, "job-completed"))
	  strlcpy(valptr, _("Job Completed"), remaining);
	else if (!strcmp(attr->values[i].string.text, "job-stopped"))
	  strlcpy(valptr, _("Job Stopped"), remaining);
	else if (!strcmp(attr->values[i].string.text, "job-config-changed"))
	  strlcpy(valptr, _("Job Options Changed"), remaining);
	else if (!strcmp(attr->values[i].string.text, "server-restarted"))
	  strlcpy(valptr, _("Server Restarted"), remaining);
	else if (!strcmp(attr->values[i].string.text, "server-started"))
	  strlcpy(valptr, _("Server Started"), remaining);
	else if (!strcmp(attr->values[i].string.text, "server-stopped"))
	  strlcpy(valptr, _("Server Stopped"), remaining);
	else if (!strcmp(attr->values[i].string.text, "server-audit"))
	  strlcpy(valptr, _("Server Security Auditing"), remaining);
	else
          strlcpy(valptr, attr->values[i].string.text, remaining);

        valptr += strlen(valptr);
      }

      cgiSetArray("notify_events", element, value);
      continue;
    }

   /*
    * Add "notify_printer_name" variable if we have a "notify_printer_uri"
    * attribute...
    */

    if (!strcmp(name, "notify_printer_uri"))
    {
      if ((valptr = strrchr(attr->values[0].string.text, '/')) == NULL)
	valptr = "unknown";
      else
	valptr ++;

      cgiSetArray("notify_printer_name", element, valptr);
    }

   /*
    * Add "notify_recipient_name" variable if we have a "notify_recipient_uri"
    * attribute, and rewrite recipient URI...
    */

    if (!strcmp(name, "notify_recipient_uri"))
    {
      char	uri[1024],		/* New URI */
		scheme[32],		/* Scheme portion of URI */
		userpass[256],		/* Username/password portion of URI */
		host[1024],		/* Hostname portion of URI */
		resource[1024],		/* Resource portion of URI */
		*options;		/* Options in URI */
      int	port;			/* Port number */


      httpSeparateURI(HTTP_URI_CODING_ALL, attr->values[0].string.text,
                      scheme, sizeof(scheme), userpass, sizeof(userpass),
		      host, sizeof(host), &port, resource, sizeof(resource));

      if (!strcmp(scheme, "rss"))
      {
       /*
        * RSS notification...
	*/

        if ((options = strchr(resource, '?')) != NULL)
	  *options = '\0';

        if (host[0])
	{
	 /*
	  * Link to remote feed...
	  */

	  httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "http",
	                  userpass, host, port, resource);
          strlcpy(name, uri, sizeof(name));
	}
	else
	{
	 /*
	  * Link to local feed...
	  */

	  snprintf(uri, sizeof(uri), "/rss%s", resource);
          strlcpy(name, resource + 1, sizeof(name));
	}
      }
      else
      {
       /*
        * Other...
	*/

        strlcpy(uri, attr->values[0].string.text, sizeof(uri));
	strlcpy(name, resource, sizeof(name));
      }

      cgiSetArray("notify_recipient_uri", element, uri);
      cgiSetArray("notify_recipient_name", element, name);
      continue;
    }

   /*
    * Add "admin_uri" variable if we have a "printer_uri_supported"
    * attribute...
    */

    if (!strcmp(name, "printer_uri_supported"))
    {
      cgiRewriteURL(attr->values[0].string.text, value, sizeof(value),
	            "/admin/");

      cgiSetArray("admin_uri", element, value);
    }

   /*
    * Copy values...
    */

    value[0] = '\0';	/* Initially an empty string */
    valptr   = value; /* Start at the beginning */

    for (i = 0; i < attr->num_values; i ++)
    {
      if (i)
	strlcat(valptr, ", ", sizeof(value) - (valptr - value));

      valptr += strlen(valptr);

      switch (attr->value_tag)
      {
	case IPP_TAG_INTEGER :
	case IPP_TAG_ENUM :
	    if (strncmp(name, "time_at_", 8) == 0)
	    {
	      time_t	t;		/* Temporary time value */

              t    = (time_t)attr->values[i].integer;
	      date = localtime(&t);

	      strftime(valptr, sizeof(value) - (valptr - value), "%c", date);
	    }
	    else
	      snprintf(valptr, sizeof(value) - (valptr - value),
		       "%d", attr->values[i].integer);
	    break;

	case IPP_TAG_BOOLEAN :
	    snprintf(valptr, sizeof(value) - (valptr - value),
	             "%d", attr->values[i].boolean);
	    break;

	case IPP_TAG_NOVALUE :
	    strlcat(valptr, "novalue", sizeof(value) - (valptr - value));
	    break;

	case IPP_TAG_RANGE :
	    snprintf(valptr, sizeof(value) - (valptr - value),
	             "%d-%d", attr->values[i].range.lower,
		     attr->values[i].range.upper);
	    break;

	case IPP_TAG_RESOLUTION :
	    snprintf(valptr, sizeof(value) - (valptr - value),
	             "%dx%d%s", attr->values[i].resolution.xres,
		     attr->values[i].resolution.yres,
		     attr->values[i].resolution.units == IPP_RES_PER_INCH ?
			 "dpi" : "dpcm");
	    break;

	case IPP_TAG_URI :
	    if (strchr(attr->values[i].string.text, ':') &&
	        strcmp(name, "device_uri"))
	    {
	     /*
	      * Rewrite URIs...
	      */

              if (!strcmp(name, "member_uris"))
	      {
		char	url[1024];	/* URL for class member... */


		cgiRewriteURL(attr->values[i].string.text, url,
		              sizeof(url), NULL);

                snprintf(valptr, sizeof(value) - (valptr - value),
		         "<A HREF=\"%s\">%s</A>", url,
			 strrchr(attr->values[i].string.text, '/') + 1);
	      }
	      else
		cgiRewriteURL(attr->values[i].string.text, valptr,
		              sizeof(value) - (valptr - value), NULL);
              break;
            }

        case IPP_TAG_STRING :
	case IPP_TAG_TEXT :
	case IPP_TAG_NAME :
	case IPP_TAG_KEYWORD :
	case IPP_TAG_CHARSET :
	case IPP_TAG_LANGUAGE :
	case IPP_TAG_MIMETYPE :
	    strlcat(valptr, attr->values[i].string.text,
	            sizeof(value) - (valptr - value));
	    break;

        case IPP_TAG_BEGIN_COLLECTION :
	    snprintf(value, sizeof(value), "%s%d", name, i + 1);
            cgiSetIPPVars(attr->values[i].collection, NULL, NULL, value,
	                  element);
            break;

        default :
	    break; /* anti-compiler-warning-code */
      }
    }

   /*
    * Add the element...
    */

    if (attr->value_tag != IPP_TAG_BEGIN_COLLECTION)
    {
      cgiSetArray(name, element, value);

      fprintf(stderr, "DEBUG2: %s[%d]=\"%s\"\n", name, element, value);
    }
  }

  return (attr ? attr->next : NULL);
}


/*
 * 'cgiSetIPPVars()' - Set CGI variables from an IPP response.
 */

int					/* O - Maximum number of elements */
cgiSetIPPVars(ipp_t      *response,	/* I - Response data to be copied... */
              const char *filter_name,	/* I - Filter name */
	      const char *filter_value,	/* I - Filter value */
	      const char *prefix,	/* I - Prefix for name or NULL */
	      int        parent_el)	/* I - Parent element number */
{
  int			element;	/* Element in CGI array */
  ipp_attribute_t	*attr,		/* Attribute in response... */
			*filter;	/* Filtering attribute */


  fprintf(stderr, "DEBUG2: cgiSetIPPVars(response=%p, filter_name=\"%s\", "
                  "filter_value=\"%s\", prefix=\"%s\", parent_el=%d)\n",
          response, filter_name ? filter_name : "(null)",
	  filter_value ? filter_value : "(null)",
	  prefix ? prefix : "(null)", parent_el);

 /*
  * Set common CGI template variables...
  */

  if (!prefix)
    cgiSetServerVersion();

 /*
  * Loop through the attributes and set them for the template...
  */

  attr = response->attrs;

  if (!prefix)
    while (attr && attr->group_tag == IPP_TAG_OPERATION)
      attr = attr->next;

  for (element = parent_el; attr; element ++)
  {
   /*
    * Copy attributes to a separator...
    */

    while (attr && attr->group_tag == IPP_TAG_ZERO)
      attr= attr->next;

    if (!attr)
      break;

    if (filter_name)
    {
      for (filter = attr;
           filter != NULL && filter->group_tag != IPP_TAG_ZERO;
           filter = filter->next)
        if (filter->name && !strcmp(filter->name, filter_name) &&
	    (filter->value_tag == IPP_TAG_STRING ||
	     (filter->value_tag >= IPP_TAG_TEXTLANG &&
	      filter->value_tag <= IPP_TAG_MIMETYPE)) &&
	    filter->values[0].string.text != NULL &&
	    !_cups_strcasecmp(filter->values[0].string.text, filter_value))
	  break;

      if (!filter)
        return (element + 1);

      if (filter->group_tag == IPP_TAG_ZERO)
      {
        attr = filter;
	element --;
	continue;
      }
    }

    attr = cgiSetIPPObjectVars(attr, prefix, element);
  }

  fprintf(stderr, "DEBUG2: Returing %d from cgiSetIPPVars()...\n", element);

  return (element);
}


/*
 * 'cgiShowIPPError()' - Show the last IPP error message.
 *
 * The caller must still call cgiStartHTML() and cgiEndHTML().
 */

void
cgiShowIPPError(const char *message)	/* I - Contextual message */
{
  cgiSetVariable("MESSAGE", cgiText(message));
  cgiSetVariable("ERROR", cupsLastErrorString());
  cgiCopyTemplateLang("error.tmpl");
}


/*
 * 'cgiShowJobs()' - Show print jobs.
 */

void
cgiShowJobs(http_t     *http,		/* I - Connection to server */
            const char *dest)		/* I - Destination name or NULL */
{
  int			i;		/* Looping var */
  const char		*which_jobs;	/* Which jobs to show */
  ipp_t			*request,	/* IPP request */
			*response;	/* IPP response */
  cups_array_t		*jobs;		/* Array of job objects */
  ipp_attribute_t	*job;		/* Job object */
  int			ascending,	/* Order of jobs (0 = descending) */
			first,		/* First job to show */
			count;		/* Number of jobs */
  const char		*var,		/* Form variable */
			*query,		/* Query string */
			*section;	/* Section in web interface */
  void			*search;	/* Search data */
  char			url[1024],	/* Printer URI */
			val[1024];	/* Form variable */


 /*
  * Build an IPP_GET_JOBS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNewRequest(IPP_GET_JOBS);

  if (dest)
  {
    httpAssembleURIf(HTTP_URI_CODING_ALL, url, sizeof(url), "ipp", NULL,
                     "localhost", ippPort(), "/printers/%s", dest);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, url);
  }
  else
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL,
        	 "ipp://localhost/");

  if ((which_jobs = cgiGetVariable("which_jobs")) != NULL)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "which-jobs",
                 NULL, which_jobs);

  cgiGetAttributes(request, "jobs.tmpl");

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
   /*
    * Get a list of matching job objects.
    */

    if ((query = cgiGetVariable("QUERY")) != NULL &&
        !cgiGetVariable("CLEAR"))
      search = cgiCompileSearch(query);
    else
    {
      query  = NULL;
      search = NULL;
    }

    jobs  = cgiGetIPPObjects(response, search);
    count = cupsArrayCount(jobs);

    if (search)
      cgiFreeSearch(search);

   /*
    * Figure out which jobs to display...
    */

    if ((var = cgiGetVariable("FIRST")) != NULL)
      first = atoi(var);
    else
      first = 0;

    if (first >= count)
      first = count - CUPS_PAGE_MAX;

    first = (first / CUPS_PAGE_MAX) * CUPS_PAGE_MAX;

    if (first < 0)
      first = 0;

    if ((var = cgiGetVariable("ORDER")) != NULL)
      ascending = !_cups_strcasecmp(var, "asc");
    else
      ascending = !which_jobs || !_cups_strcasecmp(which_jobs, "not-completed");

    section = cgiGetVariable("SECTION");

    cgiClearVariables();

    if (query)
      cgiSetVariable("QUERY", query);

    cgiSetVariable("ORDER", ascending ? "asc" : "dec");

    cgiSetVariable("SECTION", section);

    sprintf(val, "%d", count);
    cgiSetVariable("TOTAL", val);

    if (which_jobs)
      cgiSetVariable("WHICH_JOBS", which_jobs);

    if (ascending)
    {
      for (i = 0, job = (ipp_attribute_t *)cupsArrayIndex(jobs, first);
	   i < CUPS_PAGE_MAX && job;
	   i ++, job = (ipp_attribute_t *)cupsArrayNext(jobs))
        cgiSetIPPObjectVars(job, NULL, i);
    }
    else
    {
      for (i = 0, job = (ipp_attribute_t *)cupsArrayIndex(jobs, count - first - 1);
	   i < CUPS_PAGE_MAX && job;
	   i ++, job = (ipp_attribute_t *)cupsArrayPrev(jobs))
        cgiSetIPPObjectVars(job, NULL, i);
    }

   /*
    * Save navigation URLs...
    */

    if (dest)
    {
      snprintf(val, sizeof(val), "/%s/%s", section, dest);
      cgiSetVariable("PRINTER_NAME", dest);
      cgiSetVariable("PRINTER_URI_SUPPORTED", val);
    }
    else
      strlcpy(val, "/jobs/", sizeof(val));

    cgiSetVariable("THISURL", val);

    if (first > 0)
    {
      sprintf(val, "%d", first - CUPS_PAGE_MAX);
      cgiSetVariable("PREV", val);
    }

    if ((first + CUPS_PAGE_MAX) < count)
    {
      sprintf(val, "%d", first + CUPS_PAGE_MAX);
      cgiSetVariable("NEXT", val);
    }

   /*
    * Then show everything...
    */

    if (dest)
      cgiSetVariable("SEARCH_DEST", dest);

    cgiCopyTemplateLang("search.tmpl");

    cgiCopyTemplateLang("jobs-header.tmpl");

    if (count > CUPS_PAGE_MAX)
      cgiCopyTemplateLang("pager.tmpl");

    cgiCopyTemplateLang("jobs.tmpl");

    if (count > CUPS_PAGE_MAX)
      cgiCopyTemplateLang("pager.tmpl");

    cupsArrayDelete(jobs);
    ippDelete(response);
  }
}


/*
 * 'cgiText()' - Return localized text.
 */

const char *				/* O - Localized message */
cgiText(const char *message)		/* I - Message */
{
  static cups_lang_t	*language = NULL;
					/* Language */


  if (!language)
    language = cupsLangDefault();

  return (_cupsLangString(language, message));
}


/*
 * End of "$Id: ipp-var.c 7940 2008-09-16 00:45:16Z mike $".
 */
