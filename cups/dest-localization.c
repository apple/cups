/*
 * "$Id: dest-localization.c 4216 2013-03-11 13:57:36Z msweet $"
 *
 *   Destination localization support for CUPS.
 *
 *   Copyright 2012-2013 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsLocalizeDestOption()	 - Get the localized string for a destination
 *				   option.
 *   cupsLocalizeDestValue()	 - Get the localized string for a destination
 *				   option+value pair.
 *   cups_create_localizations() - Create the localizations array for a
 *				   destination.
 *   cups_read_strings()	 - Read a pair of strings from a .strings file.
 *   cups_scan_strings()	 - Scan a quoted string.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"


/*
 * Local functions...
 */

static void	cups_create_localizations(http_t *http, cups_dinfo_t *dinfo);
static int	cups_read_strings(cups_file_t *fp, char *buffer, size_t bufsize,
		                  char **id, char **str);
static char	*cups_scan_strings(char *buffer);


/*
 * 'cupsLocalizeDestOption()' - Get the localized string for a destination
 *                              option.
 *
 * The returned string is stored in the destination information and will become
 * invalid if the destination information is deleted.
 *
 * @since CUPS 1.6/OS X 10.8@
 */

const char *				/* O - Localized string */
cupsLocalizeDestOption(
    http_t       *http,			/* I - Connection to destination */
    cups_dest_t  *dest,			/* I - Destination */
    cups_dinfo_t *dinfo,		/* I - Destination information */
    const char   *option)		/* I - Option to localize */
{
  _cups_message_t	key,		/* Search key */
			*match;		/* Matching entry */


  if (!http || !dest || !dinfo)
    return (option);

  if (!dinfo->localizations)
    cups_create_localizations(http, dinfo);

  if (cupsArrayCount(dinfo->localizations) == 0)
    return (option);

  key.id = (char *)option;
  if ((match = (_cups_message_t *)cupsArrayFind(dinfo->localizations,
                                                &key)) != NULL)
    return (match->str);
  else
    return (option);
}


/*
 * 'cupsLocalizeDestValue()' - Get the localized string for a destination
 *                             option+value pair.
 *
 * The returned string is stored in the destination information and will become
 * invalid if the destination information is deleted.
 *
 * @since CUPS 1.6/OS X 10.8@
 */

const char *				/* O - Localized string */
cupsLocalizeDestValue(
    http_t       *http,			/* I - Connection to destination */
    cups_dest_t  *dest,			/* I - Destination */
    cups_dinfo_t *dinfo,		/* I - Destination information */
    const char   *option,		/* I - Option to localize */
    const char   *value)		/* I - Value to localize */
{
  _cups_message_t	key,		/* Search key */
			*match;		/* Matching entry */
  char			pair[256];	/* option.value pair */


  if (!http || !dest || !dinfo)
    return (value);

  if (!dinfo->localizations)
    cups_create_localizations(http, dinfo);

  if (cupsArrayCount(dinfo->localizations) == 0)
    return (value);

  snprintf(pair, sizeof(pair), "%s.%s", option, value);
  key.id = pair;
  if ((match = (_cups_message_t *)cupsArrayFind(dinfo->localizations,
                                                &key)) != NULL)
    return (match->str);
  else
    return (value);
}


/*
 * 'cups_create_localizations()' - Create the localizations array for a
 *                                 destination.
 */

static void
cups_create_localizations(
    http_t       *http,			/* I - Connection to destination */
    cups_dinfo_t *dinfo)		/* I - Destination informations */
{
  http_t		*http2;		/* Connection for strings file */
  http_status_t		status;		/* Request status */
  ipp_attribute_t	*attr;		/* "printer-strings-uri" attribute */
  char			scheme[32],	/* URI scheme */
  			userpass[256],	/* Username/password info */
  			hostname[256],	/* Hostname */
  			resource[1024],	/* Resource */
  			http_hostname[256],
  					/* Hostname of connection */
			tempfile[1024];	/* Temporary filename */
  int			port;		/* Port number */
  http_encryption_t	encryption;	/* Encryption to use */
  cups_file_t		*temp;		/* Temporary file */


 /*
  * Create an empty message catalog...
  */

  dinfo->localizations = _cupsMessageNew(NULL);

 /*
  * See if there are any localizations...
  */

  if ((attr = ippFindAttribute(dinfo->attrs, "printer-strings-uri",
                               IPP_TAG_URI)) == NULL)
  {
   /*
    * Nope...
    */

    DEBUG_puts("4cups_create_localizations: No printer-strings-uri (uri) "
               "value.");
    return;				/* Nope */
  }

 /*
  * Pull apart the URI and determine whether we need to try a different
  * server...
  */

  if (httpSeparateURI(HTTP_URI_CODING_ALL, attr->values[0].string.text,
                      scheme, sizeof(scheme), userpass, sizeof(userpass),
                      hostname, sizeof(hostname), &port, resource,
                      sizeof(resource)) < HTTP_URI_STATUS_OK)
  {
    DEBUG_printf(("4cups_create_localizations: Bad printer-strings-uri value "
                  "\"%s\".", attr->values[0].string.text));
    return;
  }

  httpGetHostname(http, http_hostname, sizeof(http_hostname));

  if (!_cups_strcasecmp(http_hostname, hostname) &&
      port == httpAddrPort(http->hostaddr))
  {
   /*
    * Use the same connection...
    */

    http2 = http;
  }
  else
  {
   /*
    * Connect to the alternate host...
    */

    if (!strcmp(scheme, "https"))
      encryption = HTTP_ENCRYPTION_ALWAYS;
    else
      encryption = HTTP_ENCRYPTION_IF_REQUESTED;

    if ((http2 = httpConnect2(hostname, port, NULL, AF_UNSPEC, encryption, 1,
                              30000, NULL)) == NULL)
    {
      DEBUG_printf(("4cups_create_localizations: Unable to connect to "
                    "%s:%d: %s", hostname, port, cupsLastErrorString()));
      return;
    }
  }

 /*
  * Get a temporary file...
  */

  if ((temp = cupsTempFile2(tempfile, sizeof(tempfile))) == NULL)
  {
    DEBUG_printf(("4cups_create_localizations: Unable to create temporary "
                  "file: %s", cupsLastErrorString()));
    if (http2 != http)
      httpClose(http2);
    return;
  }

  status = cupsGetFd(http2, resource, cupsFileNumber(temp));

  DEBUG_printf(("4cups_create_localizations: GET %s = %s", resource,
                httpStatus(status)));

  if (status == HTTP_STATUS_OK)
  {
   /*
    * Got the file, read it...
    */

    char		buffer[8192],	/* Message buffer */
    			*id,		/* ID string */
    			*str;		/* Translated message */
    _cups_message_t	*m;		/* Current message */

    lseek(cupsFileNumber(temp), 0, SEEK_SET);

    while (cups_read_strings(temp, buffer, sizeof(buffer), &id, &str))
    {
      if ((m = malloc(sizeof(_cups_message_t))) == NULL)
        break;

      m->id  = strdup(id);
      m->str = strdup(str);

      if (m->id && m->str)
        cupsArrayAdd(dinfo->localizations, m);
      else
      {
        if (m->id)
          free(m->id);

        if (m->str)
          free(m->str);

        free(m);
        break;
      }
    }
  }

  DEBUG_printf(("4cups_create_localizations: %d messages loaded.",
                cupsArrayCount(dinfo->localizations)));

 /*
  * Cleanup...
  */

  unlink(tempfile);
  cupsFileClose(temp);

  if (http2 != http)
    httpClose(http2);
}


/*
 * 'cups_read_strings()' - Read a pair of strings from a .strings file.
 */

static int				/* O - 1 on success, 0 on failure */
cups_read_strings(cups_file_t *strings,	/* I - .strings file */
                  char        *buffer,	/* I - Line buffer */
                  size_t      bufsize,	/* I - Size of line buffer */
		  char        **id,	/* O - Pointer to ID string */
		  char        **str)	/* O - Pointer to translation string */
{
  char	*bufptr;			/* Pointer into buffer */


  while (cupsFileGets(strings, buffer, bufsize))
  {
    if (buffer[0] != '\"')
      continue;

    *id    = buffer + 1;
    bufptr = cups_scan_strings(buffer);

    if (*bufptr != '\"')
      continue;

    *bufptr++ = '\0';

    while (*bufptr && *bufptr != '\"')
      bufptr ++;

    if (!*bufptr)
      continue;

    *str   = bufptr + 1;
    bufptr = cups_scan_strings(bufptr);

    if (*bufptr != '\"')
      continue;

    *bufptr = '\0';

    return (1);
  }

  return (0);
}


/*
 * 'cups_scan_strings()' - Scan a quoted string.
 */

static char *				/* O - End of string */
cups_scan_strings(char *buffer)		/* I - Start of string */
{
  char	*bufptr;			/* Pointer into string */


  for (bufptr = buffer + 1; *bufptr && *bufptr != '\"'; bufptr ++)
  {
    if (*bufptr == '\\')
    {
      if (bufptr[1] >= '0' && bufptr[1] <= '3' &&
	  bufptr[2] >= '0' && bufptr[2] <= '7' &&
	  bufptr[3] >= '0' && bufptr[3] <= '7')
      {
       /*
	* Decode \nnn octal escape...
	*/

	*bufptr = ((((bufptr[1] - '0') << 3) | (bufptr[2] - '0')) << 3) |
		  (bufptr[3] - '0');
	_cups_strcpy(bufptr + 1, bufptr + 4);
      }
      else
      {
       /*
	* Decode \C escape...
	*/

	_cups_strcpy(bufptr, bufptr + 1);
	if (*bufptr == 'n')
	  *bufptr = '\n';
	else if (*bufptr == 'r')
	  *bufptr = '\r';
	else if (*bufptr == 't')
	  *bufptr = '\t';
      }
    }
  }

  return (bufptr);
}



/*
 * End of "$Id: dest-localization.c 4216 2013-03-11 13:57:36Z msweet $".
 */
