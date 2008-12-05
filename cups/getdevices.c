/*
 * "$Id$"
 *
 *   cupsGetDevices implementation for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2008 by Apple Inc.
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
 *   cupsGetDevices() - Get available printer devices.
 */

/*
 * Include necessary headers...
 */

#include "globals.h"
#include "debug.h"


/*
 * 'cupsGetDevices()' - Get available printer devices.
 *
 * This function sends a CUPS-Get-Devices request and streams the discovered
 * devices to the specified callback function. The "timeout" parameter controls
 * how long the request lasts, while the "include_schemes" and "exclude_schemes"
 * parameters provide comma-delimited lists of backends to include or omit from
 * the request respectively.
 *
 * @since CUPS 1.4@
 */

ipp_status_t				/* O - Request status - @code IPP_OK@ on success. */
cupsGetDevices(
    http_t           *http,		/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
    int              timeout,		/* I - Timeout in seconds or @code CUPS_TIMEOUT_DEFAULT@ */
    const char       *include_schemes,	/* I - Comma-separated URI schemes to include or @code CUPS_INCLUDE_ALL@ */
    const char       *exclude_schemes,	/* I - Comma-separated URI schemes to exclude or @code CUPS_EXCLUDE_NONE@ */
    cups_device_cb_t callback,		/* I - Callback function */
    void             *user_data)	/* I - User data pointer */
{
  ipp_t		*request,		/* CUPS-Get-Devices request */
		*response;		/* CUPS-Get-Devices response */
  ipp_attribute_t *attr;		/* Current attribute */
  const char	*device_class,		/* device-class value */
		*device_id,		/* device-id value */
		*device_info,		/* device-info value */
		*device_location,	/* device-location value */
		*device_make_and_model,	/* device-make-and-model value */
		*device_uri;		/* device-uri value */
  int		blocking;		/* Current blocking-IO mode */
  cups_option_t	option;			/* in/exclude-schemes option */
  http_status_t	status;			/* HTTP status of request */
  ipp_state_t	state;			/* IPP response state */


 /*
  * Range check input...
  */

  if (!callback)
    return (IPP_INTERNAL_ERROR);

  if (!http)
    http = _cupsConnect();

  if (!http)
    return (IPP_SERVICE_UNAVAILABLE);

 /*
  * Create a CUPS-Get-Devices request...
  */

  request = ippNewRequest(CUPS_GET_DEVICES);

  if (timeout > 0)
    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "timeout",
                  timeout);

  if (include_schemes)
  {
    option.name  = "include-schemes";
    option.value = (char *)include_schemes;

    cupsEncodeOptions2(request, 1, &option, IPP_TAG_OPERATION);
  }

  if (exclude_schemes)
  {
    option.name  = "exclude-schemes";
    option.value = (char *)exclude_schemes;

    cupsEncodeOptions2(request, 1, &option, IPP_TAG_OPERATION);
  }

 /*
  * Send the request and do any necessary authentication...
  */

  do
  {
    DEBUG_puts("cupsGetDevices: Sending request...");
    status = cupsSendRequest(http, request, "/", ippLength(request));

    DEBUG_puts("cupsGetDevices: Waiting for response status...");
    while (status == HTTP_CONTINUE)
      status = httpUpdate(http);

    if (status != HTTP_OK)
    {
      httpFlush(http);

      if (status == HTTP_UNAUTHORIZED)
      {
       /*
	* See if we can do authentication...
	*/

	int auth_result;

	DEBUG_puts("cupsGetDevices: Need authorization...");

	if ((auth_result = cupsDoAuthentication(http, "POST", "/")) == 0)
	  httpReconnect(http);
	else if (auth_result < 0)
	  http->status = status = HTTP_FORBIDDEN;
      }

#ifdef HAVE_SSL
      else if (status == HTTP_UPGRADE_REQUIRED)
      {
       /*
	* Force a reconnect with encryption...
	*/

	DEBUG_puts("cupsGetDevices: Need encryption...");

	if (!httpReconnect(http))
	  httpEncryption(http, HTTP_ENCRYPT_REQUIRED);
      }
#endif /* HAVE_SSL */
    }
  }
  while (status == HTTP_UNAUTHORIZED || status == HTTP_UPGRADE_REQUIRED);

  DEBUG_printf(("cupsGetDevices: status=%d\n", status));

  ippDelete(request);

  if (status != HTTP_OK)
  {
    _cupsSetHTTPError(status);
    return (cupsLastError());
  }

 /*
  * Read the response in non-blocking mode...
  */

  blocking = httpGetBlocking(http);
  httpBlocking(http, 0);

  response              = ippNew();
  device_class          = NULL;
  device_id             = NULL;
  device_info           = NULL;
  device_location       = "";
  device_make_and_model = NULL;
  device_uri            = NULL;
  attr                  = NULL;

  DEBUG_puts("cupsGetDevices: Reading response...");

  do
  {
    if ((state = ippRead(http, response)) == IPP_ERROR)
      break;

    DEBUG_printf(("cupsGetDevices: state=%d, response->last=%p\n", state,
                  response->last));

    if (!response->attrs)
      continue;

    while (attr != response->last)
    {
      if (!attr)
	attr = response->attrs;
      else
        attr = attr->next;

      DEBUG_printf(("cupsGetDevices: attr->name=\"%s\", attr->value_tag=%d\n",
                    attr->name ? attr->name : "(null)", attr->value_tag));

      if (!attr->name)
      {
        if (device_class && device_id && device_info && device_make_and_model &&
	    device_uri)
          (*callback)(device_class, device_id, device_info,
	              device_make_and_model, device_uri, device_location,
		      user_data);

	device_class          = NULL;
	device_id             = NULL;
	device_info           = NULL;
	device_location       = "";
	device_make_and_model = NULL;
	device_uri            = NULL;
      }
      else if (!strcmp(attr->name, "device-class") &&
               attr->value_tag == IPP_TAG_KEYWORD)
        device_class = attr->values[0].string.text;
      else if (!strcmp(attr->name, "device-id") &&
               attr->value_tag == IPP_TAG_TEXT)
        device_id = attr->values[0].string.text;
      else if (!strcmp(attr->name, "device-info") &&
               attr->value_tag == IPP_TAG_TEXT)
        device_info = attr->values[0].string.text;
      else if (!strcmp(attr->name, "device-location") &&
               attr->value_tag == IPP_TAG_TEXT)
        device_location = attr->values[0].string.text;
      else if (!strcmp(attr->name, "device-make-and-model") &&
               attr->value_tag == IPP_TAG_TEXT)
        device_make_and_model = attr->values[0].string.text;
      else if (!strcmp(attr->name, "device-uri") &&
               attr->value_tag == IPP_TAG_URI)
        device_uri = attr->values[0].string.text;
    }
  }
  while (state != IPP_DATA);

  DEBUG_printf(("cupsGetDevices: state=%d, response->last=%p\n", state,
		response->last));

  if (device_class && device_id && device_info && device_make_and_model &&
      device_uri)
    (*callback)(device_class, device_id, device_info,
		device_make_and_model, device_uri, device_location, user_data);

 /*
  * Set the IPP status and return...
  */

  httpBlocking(http, blocking);
  httpFlush(http);

  if (status == IPP_ERROR)
    _cupsSetError(IPP_ERROR, NULL, 0);
  else
  {
    attr = ippFindAttribute(response, "status-message", IPP_TAG_TEXT);

    DEBUG_printf(("cupsGetDevices: status-code=%s, status-message=\"%s\"\n",
		  ippErrorString(response->request.status.status_code),
		  attr ? attr->values[0].string.text : ""));

    _cupsSetError(response->request.status.status_code,
		  attr ? attr->values[0].string.text :
		      ippErrorString(response->request.status.status_code), 0);
  }

  ippDelete(response);

  return (cupsLastError());
}


/*
 * End of "$Id$".
 */
