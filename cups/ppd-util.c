/*
 * PPD utilities for CUPS.
 *
 * Copyright © 2007-2018 by Apple Inc.
 * Copyright © 1997-2006 by Easy Software Products.
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

#include "cups-private.h"
#include "ppd-private.h"
#include <fcntl.h>
#include <sys/stat.h>
#if defined(_WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#endif /* _WIN32 || __EMX__ */


/*
 * Local functions...
 */

static int	cups_get_printer_uri(http_t *http, const char *name,
		                     char *host, int hostsize, int *port,
				     char *resource, int resourcesize,
				     int depth);


/*
 * 'cupsGetPPD()' - Get the PPD file for a printer on the default server.
 *
 * For classes, @code cupsGetPPD@ returns the PPD file for the first printer
 * in the class.
 *
 * The returned filename is stored in a static buffer and is overwritten with
 * each call to @code cupsGetPPD@ or @link cupsGetPPD2@.  The caller "owns" the
 * file that is created and must @code unlink@ the returned filename.
 */

const char *				/* O - Filename for PPD file */
cupsGetPPD(const char *name)		/* I - Destination name */
{
  _ppd_globals_t *pg = _ppdGlobals();	/* Pointer to library globals */
  time_t	modtime = 0;		/* Modification time */


 /*
  * Return the PPD file...
  */

  pg->ppd_filename[0] = '\0';

  if (cupsGetPPD3(CUPS_HTTP_DEFAULT, name, &modtime, pg->ppd_filename,
                  sizeof(pg->ppd_filename)) == HTTP_STATUS_OK)
    return (pg->ppd_filename);
  else
    return (NULL);
}


/*
 * 'cupsGetPPD2()' - Get the PPD file for a printer from the specified server.
 *
 * For classes, @code cupsGetPPD2@ returns the PPD file for the first printer
 * in the class.
 *
 * The returned filename is stored in a static buffer and is overwritten with
 * each call to @link cupsGetPPD@ or @code cupsGetPPD2@.  The caller "owns" the
 * file that is created and must @code unlink@ the returned filename.
 *
 * @since CUPS 1.1.21/macOS 10.4@
 */

const char *				/* O - Filename for PPD file */
cupsGetPPD2(http_t     *http,		/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
            const char *name)		/* I - Destination name */
{
  _ppd_globals_t *pg = _ppdGlobals();	/* Pointer to library globals */
  time_t	modtime = 0;		/* Modification time */


  pg->ppd_filename[0] = '\0';

  if (cupsGetPPD3(http, name, &modtime, pg->ppd_filename,
                  sizeof(pg->ppd_filename)) == HTTP_STATUS_OK)
    return (pg->ppd_filename);
  else
    return (NULL);
}


/*
 * 'cupsGetPPD3()' - Get the PPD file for a printer on the specified
 *                   server if it has changed.
 *
 * The "modtime" parameter contains the modification time of any
 * locally-cached content and is updated with the time from the PPD file on
 * the server.
 *
 * The "buffer" parameter contains the local PPD filename.  If it contains
 * the empty string, a new temporary file is created, otherwise the existing
 * file will be overwritten as needed.  The caller "owns" the file that is
 * created and must @code unlink@ the returned filename.
 *
 * On success, @code HTTP_STATUS_OK@ is returned for a new PPD file and
 * @code HTTP_STATUS_NOT_MODIFIED@ if the existing PPD file is up-to-date.  Any other
 * status is an error.
 *
 * For classes, @code cupsGetPPD3@ returns the PPD file for the first printer
 * in the class.
 *
 * @since CUPS 1.4/macOS 10.6@
 */

http_status_t				/* O  - HTTP status */
cupsGetPPD3(http_t     *http,		/* I  - HTTP connection or @code CUPS_HTTP_DEFAULT@ */
            const char *name,		/* I  - Destination name */
	    time_t     *modtime,	/* IO - Modification time */
	    char       *buffer,		/* I  - Filename buffer */
	    size_t     bufsize)		/* I  - Size of filename buffer */
{
  int		http_port;		/* Port number */
  char		http_hostname[HTTP_MAX_HOST];
					/* Hostname associated with connection */
  http_t	*http2;			/* Alternate HTTP connection */
  int		fd;			/* PPD file */
  char		localhost[HTTP_MAX_URI],/* Local hostname */
		hostname[HTTP_MAX_URI],	/* Hostname */
		resource[HTTP_MAX_URI];	/* Resource name */
  int		port;			/* Port number */
  http_status_t	status;			/* HTTP status from server */
  char		tempfile[1024] = "";	/* Temporary filename */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * Range check input...
  */

  DEBUG_printf(("cupsGetPPD3(http=%p, name=\"%s\", modtime=%p(%d), buffer=%p, "
                "bufsize=%d)", http, name, modtime,
		modtime ? (int)*modtime : 0, buffer, (int)bufsize));

  if (!name)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("No printer name"), 1);
    return (HTTP_STATUS_NOT_ACCEPTABLE);
  }

  if (!modtime)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("No modification time"), 1);
    return (HTTP_STATUS_NOT_ACCEPTABLE);
  }

  if (!buffer || bufsize <= 1)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad filename buffer"), 1);
    return (HTTP_STATUS_NOT_ACCEPTABLE);
  }

#ifndef _WIN32
 /*
  * See if the PPD file is available locally...
  */

  if (http)
    httpGetHostname(http, hostname, sizeof(hostname));
  else
  {
    strlcpy(hostname, cupsServer(), sizeof(hostname));
    if (hostname[0] == '/')
      strlcpy(hostname, "localhost", sizeof(hostname));
  }

  if (!_cups_strcasecmp(hostname, "localhost"))
  {
    char	ppdname[1024];		/* PPD filename */
    struct stat	ppdinfo;		/* PPD file information */


    snprintf(ppdname, sizeof(ppdname), "%s/ppd/%s.ppd", cg->cups_serverroot,
             name);
    if (!stat(ppdname, &ppdinfo) && !access(ppdname, R_OK))
    {
     /*
      * OK, the file exists and is readable, use it!
      */

      if (buffer[0])
      {
        unlink(buffer);

	if (symlink(ppdname, buffer) && errno != EEXIST)
        {
          _cupsSetError(IPP_STATUS_ERROR_INTERNAL, NULL, 0);

	  return (HTTP_STATUS_SERVER_ERROR);
	}
      }
      else
      {
        int		tries;		/* Number of tries */
        const char	*tmpdir;	/* TMPDIR environment variable */
	struct timeval	curtime;	/* Current time */

#ifdef __APPLE__
       /*
	* On macOS and iOS, the TMPDIR environment variable is not always the
	* best location to place temporary files due to sandboxing.  Instead,
	* the confstr function should be called to get the proper per-user,
	* per-process TMPDIR value.
	*/

        char		tmppath[1024];	/* Temporary directory */

	if ((tmpdir = getenv("TMPDIR")) != NULL && access(tmpdir, W_OK))
	  tmpdir = NULL;

	if (!tmpdir)
	{
	  if (confstr(_CS_DARWIN_USER_TEMP_DIR, tmppath, sizeof(tmppath)))
	    tmpdir = tmppath;
	  else
	    tmpdir = "/private/tmp";		/* This should never happen */
	}
#else
       /*
	* Previously we put root temporary files in the default CUPS temporary
	* directory under /var/spool/cups.  However, since the scheduler cleans
	* out temporary files there and runs independently of the user apps, we
	* don't want to use it unless specifically told to by cupsd.
	*/

	if ((tmpdir = getenv("TMPDIR")) == NULL)
	  tmpdir = "/tmp";
#endif /* __APPLE__ */

       /*
	* Make the temporary name using the specified directory...
	*/

	tries = 0;

	do
	{
	 /*
	  * Get the current time of day...
	  */

	  gettimeofday(&curtime, NULL);

	 /*
	  * Format a string using the hex time values...
	  */

	  snprintf(buffer, bufsize, "%s/%08lx%05lx", tmpdir,
		   (unsigned long)curtime.tv_sec,
		   (unsigned long)curtime.tv_usec);

	 /*
	  * Try to make a symlink...
	  */

	  if (!symlink(ppdname, buffer))
	    break;

	  tries ++;
	}
	while (tries < 1000);

        if (tries >= 1000)
	{
          _cupsSetError(IPP_STATUS_ERROR_INTERNAL, NULL, 0);

	  return (HTTP_STATUS_SERVER_ERROR);
	}
      }

      if (*modtime >= ppdinfo.st_mtime)
        return (HTTP_STATUS_NOT_MODIFIED);
      else
      {
        *modtime = ppdinfo.st_mtime;
	return (HTTP_STATUS_OK);
      }
    }
  }
#endif /* !_WIN32 */

 /*
  * Try finding a printer URI for this printer...
  */

  if (!http)
    if ((http = _cupsConnect()) == NULL)
      return (HTTP_STATUS_SERVICE_UNAVAILABLE);

  if (!cups_get_printer_uri(http, name, hostname, sizeof(hostname), &port,
                            resource, sizeof(resource), 0))
    return (HTTP_STATUS_NOT_FOUND);

  DEBUG_printf(("2cupsGetPPD3: Printer hostname=\"%s\", port=%d", hostname,
                port));

  if (cupsServer()[0] == '/' && !_cups_strcasecmp(hostname, "localhost") && port == ippPort())
  {
   /*
    * Redirect localhost to domain socket...
    */

    strlcpy(hostname, cupsServer(), sizeof(hostname));
    port = 0;

    DEBUG_printf(("2cupsGetPPD3: Redirecting to \"%s\".", hostname));
  }

 /*
  * Remap local hostname to localhost...
  */

  httpGetHostname(NULL, localhost, sizeof(localhost));

  DEBUG_printf(("2cupsGetPPD3: Local hostname=\"%s\"", localhost));

  if (!_cups_strcasecmp(localhost, hostname))
    strlcpy(hostname, "localhost", sizeof(hostname));

 /*
  * Get the hostname and port number we are connected to...
  */

  httpGetHostname(http, http_hostname, sizeof(http_hostname));
  http_port = httpAddrPort(http->hostaddr);

  DEBUG_printf(("2cupsGetPPD3: Connection hostname=\"%s\", port=%d",
                http_hostname, http_port));

 /*
  * Reconnect to the correct server as needed...
  */

  if (!_cups_strcasecmp(http_hostname, hostname) && port == http_port)
    http2 = http;
  else if ((http2 = httpConnect2(hostname, port, NULL, AF_UNSPEC,
				 cupsEncryption(), 1, 30000, NULL)) == NULL)
  {
    DEBUG_puts("1cupsGetPPD3: Unable to connect to server");

    return (HTTP_STATUS_SERVICE_UNAVAILABLE);
  }

 /*
  * Get a temp file...
  */

  if (buffer[0])
    fd = open(buffer, O_CREAT | O_TRUNC | O_WRONLY, 0600);
  else
    fd = cupsTempFd(tempfile, sizeof(tempfile));

  if (fd < 0)
  {
   /*
    * Can't open file; close the server connection and return NULL...
    */

    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, NULL, 0);

    if (http2 != http)
      httpClose(http2);

    return (HTTP_STATUS_SERVER_ERROR);
  }

 /*
  * And send a request to the HTTP server...
  */

  strlcat(resource, ".ppd", sizeof(resource));

  if (*modtime > 0)
    httpSetField(http2, HTTP_FIELD_IF_MODIFIED_SINCE,
                 httpGetDateString(*modtime));

  status = cupsGetFd(http2, resource, fd);

  close(fd);

 /*
  * See if we actually got the file or an error...
  */

  if (status == HTTP_STATUS_OK)
  {
    *modtime = httpGetDateTime(httpGetField(http2, HTTP_FIELD_DATE));

    if (tempfile[0])
      strlcpy(buffer, tempfile, bufsize);
  }
  else if (status != HTTP_STATUS_NOT_MODIFIED)
  {
    _cupsSetHTTPError(status);

    if (buffer[0])
      unlink(buffer);
    else if (tempfile[0])
      unlink(tempfile);
  }
  else if (tempfile[0])
    unlink(tempfile);

  if (http2 != http)
    httpClose(http2);

 /*
  * Return the PPD file...
  */

  DEBUG_printf(("1cupsGetPPD3: Returning status %d", status));

  return (status);
}


/*
 * 'cupsGetServerPPD()' - Get an available PPD file from the server.
 *
 * This function returns the named PPD file from the server.  The
 * list of available PPDs is provided by the IPP @code CUPS_GET_PPDS@
 * operation.
 *
 * You must remove (unlink) the PPD file when you are finished with
 * it. The PPD filename is stored in a static location that will be
 * overwritten on the next call to @link cupsGetPPD@, @link cupsGetPPD2@,
 * or @link cupsGetServerPPD@.
 *
 * @since CUPS 1.3/macOS 10.5@
 */

char *					/* O - Name of PPD file or @code NULL@ on error */
cupsGetServerPPD(http_t     *http,	/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
                 const char *name)	/* I - Name of PPD file ("ppd-name") */
{
  int			fd;		/* PPD file descriptor */
  ipp_t			*request;	/* IPP request */
  _ppd_globals_t	*pg = _ppdGlobals();
					/* Pointer to library globals */


 /*
  * Range check input...
  */

  if (!name)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("No PPD name"), 1);

    return (NULL);
  }

  if (!http)
    if ((http = _cupsConnect()) == NULL)
      return (NULL);

 /*
  * Get a temp file...
  */

  if ((fd = cupsTempFd(pg->ppd_filename, sizeof(pg->ppd_filename))) < 0)
  {
   /*
    * Can't open file; close the server connection and return NULL...
    */

    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, NULL, 0);

    return (NULL);
  }

 /*
  * Get the PPD file...
  */

  request = ippNewRequest(IPP_OP_CUPS_GET_PPD);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "ppd-name", NULL,
               name);

  ippDelete(cupsDoIORequest(http, request, "/", -1, fd));

  close(fd);

  if (cupsLastError() != IPP_STATUS_OK)
  {
    unlink(pg->ppd_filename);
    return (NULL);
  }
  else
    return (pg->ppd_filename);
}


/*
 * 'cups_get_printer_uri()' - Get the printer-uri-supported attribute for the
 *                            first printer in a class.
 */

static int				/* O - 1 on success, 0 on failure */
cups_get_printer_uri(
    http_t     *http,			/* I - Connection to server */
    const char *name,			/* I - Name of printer or class */
    char       *host,			/* I - Hostname buffer */
    int        hostsize,		/* I - Size of hostname buffer */
    int        *port,			/* O - Port number */
    char       *resource,		/* I - Resource buffer */
    int        resourcesize,		/* I - Size of resource buffer */
    int        depth)			/* I - Depth of query */
{
  int		i;			/* Looping var */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* Current attribute */
  char		uri[HTTP_MAX_URI],	/* printer-uri attribute */
		scheme[HTTP_MAX_URI],	/* Scheme name */
		username[HTTP_MAX_URI];	/* Username:password */
  static const char * const requested_attrs[] =
		{			/* Requested attributes */
		  "member-uris",
		  "printer-uri-supported"
		};


  DEBUG_printf(("4cups_get_printer_uri(http=%p, name=\"%s\", host=%p, hostsize=%d, resource=%p, resourcesize=%d, depth=%d)", http, name, host, hostsize, resource, resourcesize, depth));

 /*
  * Setup the printer URI...
  */

  if (httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL, "localhost", 0, "/printers/%s", name) < HTTP_URI_STATUS_OK)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Unable to create printer-uri"), 1);

    *host     = '\0';
    *resource = '\0';

    return (0);
  }

  DEBUG_printf(("5cups_get_printer_uri: printer-uri=\"%s\"", uri));

 /*
  * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requested-attributes
  */

  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", sizeof(requested_attrs) / sizeof(requested_attrs[0]), NULL, requested_attrs);

 /*
  * Do the request and get back a response...
  */

  snprintf(resource, (size_t)resourcesize, "/printers/%s", name);

  if ((response = cupsDoRequest(http, request, resource)) != NULL)
  {
    if ((attr = ippFindAttribute(response, "member-uris", IPP_TAG_URI)) != NULL)
    {
     /*
      * Get the first actual printer name in the class...
      */

      DEBUG_printf(("5cups_get_printer_uri: Got member-uris with %d values.", ippGetCount(attr)));

      for (i = 0; i < attr->num_values; i ++)
      {
        DEBUG_printf(("5cups_get_printer_uri: member-uris[%d]=\"%s\"", i, ippGetString(attr, i, NULL)));

	httpSeparateURI(HTTP_URI_CODING_ALL, attr->values[i].string.text, scheme, sizeof(scheme), username, sizeof(username), host, hostsize, port, resource, resourcesize);
	if (!strncmp(resource, "/printers/", 10))
	{
	 /*
	  * Found a printer!
	  */

          ippDelete(response);

	  DEBUG_printf(("5cups_get_printer_uri: Found printer member with host=\"%s\", port=%d, resource=\"%s\"", host, *port, resource));
	  return (1);
	}
      }
    }
    else if ((attr = ippFindAttribute(response, "printer-uri-supported", IPP_TAG_URI)) != NULL)
    {
      httpSeparateURI(HTTP_URI_CODING_ALL, _httpResolveURI(attr->values[0].string.text, uri, sizeof(uri), _HTTP_RESOLVE_DEFAULT, NULL, NULL), scheme, sizeof(scheme), username, sizeof(username), host, hostsize, port, resource, resourcesize);
      ippDelete(response);

      DEBUG_printf(("5cups_get_printer_uri: Resolved to host=\"%s\", port=%d, resource=\"%s\"", host, *port, resource));

      if (!strncmp(resource, "/classes/", 9))
      {
        _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("No printer-uri found for class"), 1);

	*host     = '\0';
	*resource = '\0';

        DEBUG_puts("5cups_get_printer_uri: Not returning class.");
	return (0);
      }

      return (1);
    }

    ippDelete(response);
  }

  if (cupsLastError() != IPP_STATUS_ERROR_NOT_FOUND)
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("No printer-uri found"), 1);

  *host     = '\0';
  *resource = '\0';

  DEBUG_puts("5cups_get_printer_uri: Printer URI not found.");
  return (0);
}
