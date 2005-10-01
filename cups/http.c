/*
 * "$Id$"
 *
 *   HTTP routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   httpCheck()          - Check to see if there is a pending response from
 *                          the server.
 *   httpClearCookie()    - Clear the cookie value(s).
 *   httpClose()          - Close an HTTP connection...
 *   httpConnect()        - Connect to a HTTP server.
 *   httpConnectEncrypt() - Connect to a HTTP server using encryption.
 *   httpDecode64()       - Base64-decode a string.
 *   httpDecode64_2()     - Base64-decode a string.
 *   httpDelete()         - Send a DELETE request to the server.
 *   httpEncode64()       - Base64-encode a string.
 *   httpEncode64_2()     - Base64-encode a string.
 *   httpEncryption()     - Set the required encryption on the link.
 *   httpFlush()          - Flush data from a HTTP connection.
 *   httpFlushWrite()     - Flush data in write buffer.
 *   httpGet()            - Send a GET request to the server.
 *   httpGetDateString()  - Get a formatted date/time string from a time value.
 *   httpGetDateString2() - Get a formatted date/time string from a time value.
 *   httpGetDateTime()    - Get a time value from a formatted date/time string.
 *   httpGetLength()      - Get the amount of data remaining from the
 *                          content-length or transfer-encoding fields.
 *   httpGetLength2()     - Get the amount of data remaining from the
 *                          content-length or transfer-encoding fields.
 *   httpGetSubField()    - Get a sub-field value.
 *   httpGets()           - Get a line of text from a HTTP connection.
 *   httpHead()           - Send a HEAD request to the server.
 *   httpInitialize()     - Initialize the HTTP interface library and set the
 *                          default HTTP proxy (if any).
 *   httpOptions()        - Send an OPTIONS request to the server.
 *   httpPost()           - Send a POST request to the server.
 *   httpPrintf()         - Print a formatted string to a HTTP connection.
 *   httpPut()            - Send a PUT request to the server.
 *   httpRead()           - Read data from a HTTP connection.
 *   httpReconnect()      - Reconnect to a HTTP server...
 *   httpSetCookie()      - Set the cookie value(s)...
 *   httpSetField()       - Set the value of an HTTP header.
 *   httpSetLength()      - Set the content-length and transfer-encoding.
 *   httpTrace()          - Send an TRACE request to the server.
 *   httpUpdate()         - Update the current HTTP state for incoming data.
 *   httpWait()           - Wait for data available on a connection.
 *   httpWrite()          - Write data to a HTTP connection.
 *   http_field()         - Return the field index for a field name.
 *   http_read_ssl()      - Read from a SSL/TLS connection.
 *   http_send()          - Send a request with all fields and the trailing
 *                          blank line.
 *   http_setup_ssl()     - Set up SSL/TLS on a connection.
 *   http_shutdown_ssl()  - Shut down SSL/TLS on a connection.
 *   http_upgrade()       - Force upgrade to TLS encryption.
 *   http_wait()          - Wait for data available on a connection.
 *   http_write()         - Write data to a connection.
 *   http_write_ssl()     - Write to a SSL/TLS connection.
 *   http_read_cdsa()       - Read function for CDSA decryption code.
 *   http_write_cdsa()      - Write function for CDSA encryption code.
 */

/*
 * Include necessary headers...
 */

#include "http-private.h"
#include "globals.h"
#include "debug.h"
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#ifndef WIN32
#  include <signal.h>
#  include <sys/time.h>
#  include <sys/resource.h>
#endif /* !WIN32 */


/*
 * Some operating systems have done away with the Fxxxx constants for
 * the fcntl() call; this works around that "feature"...
 */

#ifndef FNONBLK
#  define FNONBLK O_NONBLOCK
#endif /* !FNONBLK */


/*
 * Local functions...
 */

static http_field_t	http_field(const char *name);
static int		http_send(http_t *http, http_state_t request,
			          const char *uri);
static int		http_wait(http_t *http, int msec);
static int		http_write(http_t *http, const char *buffer,
			           int length);
static int		http_write_chunk(http_t *http, const char *buffer,
			                 int length);
#ifdef HAVE_SSL
#  ifdef HAVE_CDSASSL
static OSStatus		http_read_cdsa(SSLConnectionRef connection, void *data, size_t *dataLength);
#  endif /* HAVE_CDSASSL */
static int		http_read_ssl(http_t *http, char *buf, int len);
static int		http_setup_ssl(http_t *http);
static void		http_shutdown_ssl(http_t *http);
static int		http_upgrade(http_t *http);
#  ifdef HAVE_CDSASSL
static OSStatus		http_write_cdsa(SSLConnectionRef connection, const void *data, size_t *dataLength);
#  endif /* HAVE_CDSASSL */
static int		http_write_ssl(http_t *http, const char *buf, int len);
#endif /* HAVE_SSL */


/*
 * Local globals...
 */

static const char * const http_fields[] =
			{
			  "Accept-Language",
			  "Accept-Ranges",
			  "Authorization",
			  "Connection",
			  "Content-Encoding",
			  "Content-Language",
			  "Content-Length",
			  "Content-Location",
			  "Content-MD5",
			  "Content-Range",
			  "Content-Type",
			  "Content-Version",
			  "Date",
			  "Host",
			  "If-Modified-Since",
			  "If-Unmodified-since",
			  "Keep-Alive",
			  "Last-Modified",
			  "Link",
			  "Location",
			  "Range",
			  "Referer",
			  "Retry-After",
			  "Transfer-Encoding",
			  "Upgrade",
			  "User-Agent",
			  "WWW-Authenticate"
			};
static const char * const http_days[7] =
			{
			  "Sun",
			  "Mon",
			  "Tue",
			  "Wed",
			  "Thu",
			  "Fri",
			  "Sat"
			};
static const char * const http_months[12] =
			{
			  "Jan",
			  "Feb",
			  "Mar",
			  "Apr",
			  "May",
			  "Jun",
		          "Jul",
			  "Aug",
			  "Sep",
			  "Oct",
			  "Nov",
			  "Dec"
			};


/*
 * 'httpCheck()' - Check to see if there is a pending response from the server.
 */

int				/* O - 0 = no data, 1 = data available */
httpCheck(http_t *http)		/* I - HTTP connection */
{
  return (httpWait(http, 0));
}


/*
 * 'httpClearCookie()' - Clear the cookie value(s).
 */

void
httpClearCookie(http_t *http)			/* I - Connection */
{
  if (!http)
    return;

  if (http->cookie)
  {
    free(http->cookie);
    http->cookie = NULL;
  }
}


/*
 * 'httpClose()' - Close an HTTP connection...
 */

void
httpClose(http_t *http)		/* I - Connection to close */
{
  DEBUG_printf(("httpClose(http=%p)\n", http));

  if (!http)
    return;

  if (http->input_set)
    free(http->input_set);

  if (http->cookie)
    free(http->cookie);

#ifdef HAVE_SSL
  if (http->tls)
    http_shutdown_ssl(http);
#endif /* HAVE_SSL */

#ifdef WIN32
  closesocket(http->fd);
#else
  close(http->fd);
#endif /* WIN32 */

  free(http);
}


/*
 * 'httpConnect()' - Connect to a HTTP server.
 */

http_t *				/* O - New HTTP connection */
httpConnect(const char *host,		/* I - Host to connect to */
            int        port)		/* I - Port number */
{
  http_encryption_t	encryption;	/* Type of encryption to use */


 /*
  * Set the default encryption status...
  */

  if (port == 443)
    encryption = HTTP_ENCRYPT_ALWAYS;
  else
    encryption = HTTP_ENCRYPT_IF_REQUESTED;

  return (httpConnectEncrypt(host, port, encryption));
}


/*
 * 'httpConnectEncrypt()' - Connect to a HTTP server using encryption.
 */

http_t *				/* O - New HTTP connection */
httpConnectEncrypt(
    const char        *host,		/* I - Host to connect to */
    int               port,		/* I - Port number */
    http_encryption_t encryption)	/* I - Type of encryption to use */
{
  int			i;		/* Looping var */
  http_t		*http;		/* New HTTP connection */
  struct hostent	*hostaddr;	/* Host address data */


  DEBUG_printf(("httpConnectEncrypt(host=\"%s\", port=%d, encryption=%d)\n",
                host ? host : "(null)", port, encryption));

  if (!host)
    return (NULL);

  httpInitialize();

 /*
  * Lookup the host...
  */

  if ((hostaddr = httpGetHostByName(host)) == NULL)
  {
   /*
    * This hack to make users that don't have a localhost entry in
    * their hosts file or DNS happy...
    */

    if (strcasecmp(host, "localhost") != 0)
      return (NULL);
    else if ((hostaddr = httpGetHostByName("127.0.0.1")) == NULL)
      return (NULL);
  }

 /*
  * Verify that it is an IPv4, IPv6, or domain address...
  */

  if ((hostaddr->h_addrtype != AF_INET || hostaddr->h_length != 4)
#ifdef AF_INET6
      && (hostaddr->h_addrtype != AF_INET6 || hostaddr->h_length != 16)
#endif /* AF_INET6 */
#ifdef AF_LOCAL
      && (hostaddr->h_addrtype != AF_LOCAL)
#endif /* AF_LOCAL */
      )
    return (NULL);

 /*
  * Allocate memory for the structure...
  */

  http = calloc(sizeof(http_t), 1);
  if (http == NULL)
    return (NULL);

  http->version  = HTTP_1_1;
  http->blocking = 1;
  http->activity = time(NULL);
  http->fd       = -1;

 /*
  * Set the encryption status...
  */

  if (port == 443)			/* Always use encryption for https */
    http->encryption = HTTP_ENCRYPT_ALWAYS;
  else
    http->encryption = encryption;

 /*
  * Loop through the addresses we have until one of them connects...
  */

  strlcpy(http->hostname, host, sizeof(http->hostname));

  for (i = 0; hostaddr->h_addr_list[i]; i ++)
  {
   /*
    * Load the address...
    */

    httpAddrLoad(hostaddr, port, i, &(http->hostaddr));

   /*
    * Connect to the remote system...
    */

    if (!httpReconnect(http))
      return (http);
  }

 /*
  * Could not connect to any known address - bail out!
  */

  free(http);
  return (NULL);
}


/*
 * 'httpDecode64()' - Base64-decode a string.
 */

char *					/* O - Decoded string */
httpDecode64(char       *out,		/* I - String to write to */
             const char *in)		/* I - String to read from */
{
  int	outlen;				/* Output buffer length */


 /*
  * Use the old maximum buffer size for binary compatibility...
  */

  outlen = 512;

  return (httpDecode64_2(out, &outlen, in));
}


/*
 * 'httpDecode64_2()' - Base64-decode a string.
 */

char *					/* O  - Decoded string */
httpDecode64_2(char       *out,		/* I  - String to write to */
	       int        *outlen,	/* IO - Size of output string */
               const char *in)		/* I  - String to read from */
{
  int	pos,				/* Bit position */
	base64;				/* Value of this character */
  char	*outptr,			/* Output pointer */
	*outend;			/* End of output buffer */


 /*
  * Range check input...
  */

  if (!out || !outlen || *outlen < 1 || !in || !*in)
    return (NULL);

 /*
  * Convert from base-64 to bytes...
  */

  for (outptr = out, outend = out + *outlen - 1, pos = 0; *in != '\0'; in ++)
  {
   /*
    * Decode this character into a number from 0 to 63...
    */

    if (*in >= 'A' && *in <= 'Z')
      base64 = *in - 'A';
    else if (*in >= 'a' && *in <= 'z')
      base64 = *in - 'a' + 26;
    else if (*in >= '0' && *in <= '9')
      base64 = *in - '0' + 52;
    else if (*in == '+')
      base64 = 62;
    else if (*in == '/')
      base64 = 63;
    else if (*in == '=')
      break;
    else
      continue;

   /*
    * Store the result in the appropriate chars...
    */

    switch (pos)
    {
      case 0 :
          if (outptr < outend)
            *outptr = base64 << 2;
	  pos ++;
	  break;
      case 1 :
          if (outptr < outend)
            *outptr++ |= (base64 >> 4) & 3;
          if (outptr < outend)
	    *outptr = (base64 << 4) & 255;
	  pos ++;
	  break;
      case 2 :
          if (outptr < outend)
            *outptr++ |= (base64 >> 2) & 15;
          if (outptr < outend)
	    *outptr = (base64 << 6) & 255;
	  pos ++;
	  break;
      case 3 :
          if (outptr < outend)
            *outptr++ |= base64;
	  pos = 0;
	  break;
    }
  }

  *outptr = '\0';

 /*
  * Return the decoded string and size...
  */

  *outlen = (int)(outptr - out);

  return (out);
}


/*
 * 'httpDelete()' - Send a DELETE request to the server.
 */

int					/* O - Status of call (0 = success) */
httpDelete(http_t     *http,		/* I - HTTP data */
           const char *uri)		/* I - URI to delete */
{
  return (http_send(http, HTTP_DELETE, uri));
}


/*
 * 'httpEncode64()' - Base64-encode a string.
 */

char *					/* O - Encoded string */
httpEncode64(char       *out,		/* I - String to write to */
             const char *in)		/* I - String to read from */
{
  return (httpEncode64_2(out, 512, in, strlen(in)));
}


/*
 * 'httpEncode64_2()' - Base64-encode a string.
 */

char *					/* O - Encoded string */
httpEncode64_2(char       *out,		/* I - String to write to */
	       int        outlen,	/* I - Size of output string */
               const char *in,		/* I - String to read from */
	       int        inlen)	/* I - Size of input string */
{
  char		*outptr,		/* Output pointer */
		*outend;		/* End of output buffer */
  static const char base64[] =		/* Base64 characters... */
  		{
		  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		  "abcdefghijklmnopqrstuvwxyz"
		  "0123456789"
		  "+/"
  		};


 /*
  * Range check input...
  */

  if (!out || outlen < 1 || !in || inlen < 1)
    return (NULL);

 /*
  * Convert bytes to base-64...
  */

  for (outptr = out, outend = out + outlen - 1; inlen > 0; in ++, inlen --)
  {
   /*
    * Encode the up to 3 characters as 4 Base64 numbers...
    */

    if (outptr < outend)
      *outptr ++ = base64[(in[0] & 255) >> 2];
    if (outptr < outend)
      *outptr ++ = base64[(((in[0] & 255) << 4) | ((in[1] & 255) >> 4)) & 63];

    in ++;
    inlen --;
    if (inlen <= 0)
    {
      if (outptr < outend)
        *outptr ++ = '=';
      if (outptr < outend)
        *outptr ++ = '=';
      break;
    }

    if (outptr < outend)
      *outptr ++ = base64[(((in[0] & 255) << 2) | ((in[1] & 255) >> 6)) & 63];

    in ++;
    inlen --;
    if (inlen <= 0)
    {
      if (outptr < outend)
        *outptr ++ = '=';
      break;
    }

    if (outptr < outend)
      *outptr ++ = base64[in[0] & 63];
  }

  *outptr = '\0';

 /*
  * Return the encoded string...
  */

  return (out);
}


/*
 * 'httpEncryption()' - Set the required encryption on the link.
 */

int					/* O - -1 on error, 0 on success */
httpEncryption(http_t            *http,	/* I - HTTP data */
               http_encryption_t e)	/* I - New encryption preference */
{
  DEBUG_printf(("httpEncryption(http=%p, e=%d)\n", http, e));

#ifdef HAVE_SSL
  if (!http)
    return (0);

  http->encryption = e;

  if ((http->encryption == HTTP_ENCRYPT_ALWAYS && !http->tls) ||
      (http->encryption == HTTP_ENCRYPT_NEVER && http->tls))
    return (httpReconnect(http));
  else if (http->encryption == HTTP_ENCRYPT_REQUIRED && !http->tls)
    return (http_upgrade(http));
  else
    return (0);
#else
  if (e == HTTP_ENCRYPT_ALWAYS || e == HTTP_ENCRYPT_REQUIRED)
    return (-1);
  else
    return (0);
#endif /* HAVE_SSL */
}


/*
 * 'httpFlush()' - Flush data from a HTTP connection.
 */

void
httpFlush(http_t *http)			/* I - HTTP data */
{
  char	buffer[8192];			/* Junk buffer */


  DEBUG_printf(("httpFlush(http=%p), state=%d\n", http, http->state));

  while (httpRead(http, buffer, sizeof(buffer)) > 0);
}


/*
 * 'httpFlushWrite()' - Flush data in write buffer.
 */

int					/* O - Bytes written or -1 on error */
httpFlushWrite(http_t *http)		/* I - HTTP data */
{
  int	bytes;				/* Bytes written */


  DEBUG_printf(("httpFlushWrite(http=%p)\n", http));

  if (!http || !http->wused)
    return (0);

  if (http->data_encoding == HTTP_ENCODE_CHUNKED)
    bytes = http_write_chunk(http, http->wbuffer, http->wused);
  else
    bytes = http_write(http, http->wbuffer, http->wused);

  http->wused = 0;

  return (bytes);
}


/*
 * 'httpGet()' - Send a GET request to the server.
 */

int					/* O - Status of call (0 = success) */
httpGet(http_t     *http,		/* I - HTTP data */
        const char *uri)		/* I - URI to get */
{
  return (http_send(http, HTTP_GET, uri));
}


/*
 * 'httpGetDateString()' - Get a formatted date/time string from a time value.
 *
 * @deprecated
 */

const char *				/* O - Date/time string */
httpGetDateString(time_t t)		/* I - UNIX time */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  return (httpGetDateString2(t, cg->http_date, sizeof(cg->http_date)));
}


/*
 * 'httpGetDateString2()' - Get a formatted date/time string from a time value.
 */

const char *				/* O - Date/time string */
httpGetDateString2(time_t t,		/* I - UNIX time */
                   char   *s,		/* I - String buffer */
		   int    slen)		/* I - Size of string buffer */
{
  struct tm	*tdate;			/* UNIX date/time data */


  tdate = gmtime(&t);
  snprintf(s, slen, "%s, %02d %s %d %02d:%02d:%02d GMT",
           http_days[tdate->tm_wday], tdate->tm_mday,
	   http_months[tdate->tm_mon], tdate->tm_year + 1900,
	   tdate->tm_hour, tdate->tm_min, tdate->tm_sec);

  return (s);
}


/*
 * 'httpGetDateTime()' - Get a time value from a formatted date/time string.
 */

time_t					/* O - UNIX time */
httpGetDateTime(const char *s)		/* I - Date/time string */
{
  int		i;			/* Looping var */
  char		mon[16];		/* Abbreviated month name */
  int		day, year;		/* Day of month and year */
  int		hour, min, sec;		/* Time */
  int		days;			/* Number of days since 1970 */
  static const int normal_days[] =	/* Days to a month, normal years */
		{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };
  static const int leap_days[] =	/* Days to a month, leap years */
		{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 };


  DEBUG_printf(("httpGetDateTime(s=\"%s\")\n", s));

 /*
  * Extract the date and time from the formatted string...
  */

  if (sscanf(s, "%*s%d%15s%d%d:%d:%d", &day, mon, &year, &hour, &min, &sec) < 6)
    return (0);

  DEBUG_printf(("    day=%d, mon=\"%s\", year=%d, hour=%d, min=%d, sec=%d\n",
                day, mon, year, hour, min, sec));

 /*
  * Convert the month name to a number from 0 to 11.
  */

  for (i = 0; i < 12; i ++)
    if (!strcasecmp(mon, http_months[i]))
      break;

  if (i >= 12)
    return (0);

  DEBUG_printf(("    i=%d\n", i));

 /*
  * Now convert the date and time to a UNIX time value in seconds since
  * 1970.  We can't use mktime() since the timezone may not be UTC but
  * the date/time string *is* UTC.
  */

  if ((year & 3) == 0 && ((year % 100) != 0 || (year % 400) == 0))
    days = leap_days[i] + day - 1;
  else
    days = normal_days[i] + day - 1;

  DEBUG_printf(("    days=%d\n", days));

  days += (year - 1970) * 365 +		/* 365 days per year (normally) */
          ((year - 1) / 4 - 492) -	/* + leap days */
	  ((year - 1) / 100 - 19) +	/* - 100 year days */
          ((year - 1) / 400 - 4);	/* + 400 year days */

  DEBUG_printf(("    days=%d\n", days));

  return (days * 86400 + hour * 3600 + min * 60 + sec);
}


/*
 * 'httpGetSubField()' - Get a sub-field value.
 */

char *					/* O - Value or NULL */
httpGetSubField(http_t       *http,	/* I - HTTP data */
                http_field_t field,	/* I - Field index */
                const char   *name,	/* I - Name of sub-field */
		char         *value)	/* O - Value string */
{
  const char	*fptr;			/* Pointer into field */
  char		temp[HTTP_MAX_VALUE],	/* Temporary buffer for name */
		*ptr;			/* Pointer into string buffer */


  DEBUG_printf(("httpGetSubField(http=%p, field=%d, name=\"%s\", value=%p)\n",
                http, field, name, value));

  if (http == NULL ||
      field < HTTP_FIELD_ACCEPT_LANGUAGE ||
      field > HTTP_FIELD_WWW_AUTHENTICATE ||
      name == NULL || value == NULL)
    return (NULL);

  for (fptr = http->fields[field]; *fptr;)
  {
   /*
    * Skip leading whitespace...
    */

    while (isspace(*fptr & 255))
      fptr ++;

    if (*fptr == ',')
    {
      fptr ++;
      continue;
    }

   /*
    * Get the sub-field name...
    */

    for (ptr = temp;
         *fptr && *fptr != '=' && !isspace(*fptr & 255) && ptr < (temp + sizeof(temp) - 1);
         *ptr++ = *fptr++);

    *ptr = '\0';

    DEBUG_printf(("httpGetSubField: name=\"%s\"\n", temp));

   /*
    * Skip trailing chars up to the '='...
    */

    while (isspace(*fptr & 255))
      fptr ++;

    if (!*fptr)
      break;

    if (*fptr != '=')
      continue;

   /*
    * Skip = and leading whitespace...
    */

    fptr ++;

    while (isspace(*fptr & 255))
      fptr ++;

    if (*fptr == '\"')
    {
     /*
      * Read quoted string...
      */

      for (ptr = value, fptr ++;
           *fptr && *fptr != '\"' && ptr < (value + HTTP_MAX_VALUE - 1);
	   *ptr++ = *fptr++);

      *ptr = '\0';

      while (*fptr && *fptr != '\"')
        fptr ++;

      if (*fptr)
        fptr ++;
    }
    else
    {
     /*
      * Read unquoted string...
      */

      for (ptr = value;
           *fptr && !isspace(*fptr & 255) && *fptr != ',' && ptr < (value + HTTP_MAX_VALUE - 1);
	   *ptr++ = *fptr++);

      *ptr = '\0';

      while (*fptr && !isspace(*fptr & 255) && *fptr != ',')
        fptr ++;
    }

    DEBUG_printf(("httpGetSubField: value=\"%s\"\n", value));

   /*
    * See if this is the one...
    */

    if (strcmp(name, temp) == 0)
      return (value);
  }

  value[0] = '\0';

  return (NULL);
}


/*
 * 'httpGetLength()' - Get the amount of data remaining from the
 *                     content-length or transfer-encoding fields.
 *
 * This function is deprecated and will not return lengths larger than
 * 2^31 - 1; use httpGetLength2() instead.
 */

int					/* O - Content length */
httpGetLength(http_t *http)		/* I - HTTP data */
{
 /*
  * Get the read content length and return the 32-bit value.
  */

  httpGetLength2(http);

  return (http->_data_remaining);
}


/*
 * 'httpGetLength2()' - Get the amount of data remaining from the
 *                      content-length or transfer-encoding fields.
 *
 * This function returns the complete content length, even for
 * content larger than 2^31 - 1.
 */

off_t					/* O - Content length */
httpGetLength2(http_t *http)		/* I - HTTP data */
{
  DEBUG_printf(("httpGetLength2(http=%p), state=%d\n", http, http->state));

  if (!strcasecmp(http->fields[HTTP_FIELD_TRANSFER_ENCODING], "chunked"))
  {
    DEBUG_puts("httpGetLength2: chunked request!");

    http->data_encoding  = HTTP_ENCODE_CHUNKED;
    http->data_remaining = 0;
  }
  else
  {
    http->data_encoding = HTTP_ENCODE_LENGTH;

   /*
    * The following is a hack for HTTP servers that don't send a
    * content-length or transfer-encoding field...
    *
    * If there is no content-length then the connection must close
    * after the transfer is complete...
    */

    if (http->fields[HTTP_FIELD_CONTENT_LENGTH][0] == '\0')
      http->data_remaining = 2147483647;
    else
      http->data_remaining = strtoll(http->fields[HTTP_FIELD_CONTENT_LENGTH],
                                     NULL, 10);

    DEBUG_printf(("httpGetLength2: content_length=" CUPS_LLFORMAT "\n",
                  CUPS_LLCAST http->data_remaining));
  }

  if (http->data_remaining <= INT_MAX)
    http->_data_remaining = (int)http->data_remaining;
  else
    http->_data_remaining = INT_MAX;

  return (http->data_remaining);
}


/*
 * 'httpGets()' - Get a line of text from a HTTP connection.
 */

char *					/* O - Line or NULL */
httpGets(char   *line,			/* I - Line to read into */
         int    length,			/* I - Max length of buffer */
	 http_t *http)			/* I - HTTP data */
{
  char	*lineptr,			/* Pointer into line */
	*lineend,			/* End of line */
	*bufptr,			/* Pointer into input buffer */
	*bufend;			/* Pointer to end of buffer */
  int	bytes,				/* Number of bytes read */
	eol;				/* End-of-line? */


  DEBUG_printf(("httpGets(line=%p, length=%d, http=%p)\n", line, length, http));

  if (http == NULL || line == NULL)
    return (NULL);

 /*
  * Read a line from the buffer...
  */
    
  lineptr = line;
  lineend = line + length - 1;
  eol     = 0;

  while (lineptr < lineend)
  {
   /*
    * Pre-load the buffer as needed...
    */

#ifdef WIN32
    WSASetLastError(0);
#else
    errno = 0;
#endif /* WIN32 */

    while (http->used == 0)
    {
     /*
      * No newline; see if there is more data to be read...
      */

      if (!http->blocking && !http_wait(http, 1000))
        return (NULL);

#ifdef HAVE_SSL
      if (http->tls)
	bytes = http_read_ssl(http, http->buffer + http->used,
	                      HTTP_MAX_BUFFER - http->used);
      else
#endif /* HAVE_SSL */
        bytes = recv(http->fd, http->buffer + http->used,
	             HTTP_MAX_BUFFER - http->used, 0);

      DEBUG_printf(("httpGets: read %d bytes...\n", bytes));

      if (bytes < 0)
      {
       /*
	* Nope, can't get a line this time...
	*/

#ifdef WIN32
        if (WSAGetLastError() != http->error)
	{
	  http->error = WSAGetLastError();
	  continue;
	}

        DEBUG_printf(("httpGets: recv() error %d!\n", WSAGetLastError()));
#else
        DEBUG_printf(("httpGets: recv() error %d!\n", errno));

        if (errno == EINTR)
	  continue;
	else if (errno != http->error)
	{
	  http->error = errno;
	  continue;
	}
#endif /* WIN32 */

        return (NULL);
      }
      else if (bytes == 0)
      {
	http->error = EPIPE;

        return (NULL);
      }

     /*
      * Yup, update the amount used...
      */

      http->used += bytes;
    }

   /*
    * Now copy as much of the current line as possible...
    */

    for (bufptr = http->buffer, bufend = http->buffer + http->used;
         lineptr < lineend && bufptr < bufend;)
    {
      if (*bufptr == 0x0a)
      {
        eol = 1;
	bufptr ++;
	break;
      }
      else if (*bufptr == 0x0d)
	bufptr ++;
      else
	*lineptr++ = *bufptr++;
    }

    http->used -= bufptr - http->buffer;
    if (http->used > 0)
      memmove(http->buffer, bufptr, http->used);

    if (eol)
    {
     /*
      * End of line...
      */

      http->activity = time(NULL);

      *lineptr = '\0';
      
      DEBUG_printf(("httpGets: Returning \"%s\"\n", line));

      return (line);
    }
  }

  DEBUG_puts("httpGets: No new line available!");

  return (NULL);
}


/*
 * 'httpHead()' - Send a HEAD request to the server.
 */

int					/* O - Status of call (0 = success) */
httpHead(http_t     *http,		/* I - HTTP data */
         const char *uri)		/* I - URI for head */
{
  return (http_send(http, HTTP_HEAD, uri));
}


/*
 * 'httpInitialize()' - Initialize the HTTP interface library and set the
 *                      default HTTP proxy (if any).
 */

void
httpInitialize(void)
{
#ifdef HAVE_LIBSSL
#  ifndef WIN32
  struct timeval	curtime;	/* Current time in microseconds */
#  endif /* !WIN32 */
  int			i;		/* Looping var */
  unsigned char		data[1024];	/* Seed data */
#endif /* HAVE_LIBSSL */

#ifdef WIN32
  WSADATA	winsockdata;		/* WinSock data */
  static int	initialized = 0;	/* Has WinSock been initialized? */


  if (!initialized)
    WSAStartup(MAKEWORD(1,1), &winsockdata);
#elif defined(HAVE_SIGSET)
  sigset(SIGPIPE, SIG_IGN);
#elif defined(HAVE_SIGACTION)
  struct sigaction	action;		/* POSIX sigaction data */


 /*
  * Ignore SIGPIPE signals...
  */

  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &action, NULL);
#else
  signal(SIGPIPE, SIG_IGN);
#endif /* WIN32 */

#ifdef HAVE_GNUTLS
  gnutls_global_init();
#endif /* HAVE_GNUTLS */

#ifdef HAVE_LIBSSL
  SSL_load_error_strings();
  SSL_library_init();

 /*
  * Using the current time is a dubious random seed, but on some systems
  * it is the best we can do (on others, this seed isn't even used...)
  */

#ifdef WIN32
#else
  gettimeofday(&curtime, NULL);
  srand(curtime.tv_sec + curtime.tv_usec);
#endif /* WIN32 */

  for (i = 0; i < sizeof(data); i ++)
    data[i] = rand(); /* Yes, this is a poor source of random data... */

  RAND_seed(&data, sizeof(data));
#endif /* HAVE_LIBSSL */
}


/*
 * 'httpOptions()' - Send an OPTIONS request to the server.
 */

int					/* O - Status of call (0 = success) */
httpOptions(http_t     *http,		/* I - HTTP data */
            const char *uri)		/* I - URI for options */
{
  return (http_send(http, HTTP_OPTIONS, uri));
}


/*
 * 'httpPost()' - Send a POST request to the server.
 */

int					/* O - Status of call (0 = success) */
httpPost(http_t     *http,		/* I - HTTP data */
         const char *uri)		/* I - URI for post */
{
  return (http_send(http, HTTP_POST, uri));
}


/*
 * 'httpPrintf()' - Print a formatted string to a HTTP connection.
 */

int					/* O - Number of bytes written */
httpPrintf(http_t     *http,		/* I - HTTP data */
           const char *format,		/* I - printf-style format string */
	   ...)				/* I - Additional args as needed */
{
  int		bytes;			/* Number of bytes to write */
  char		buf[16384];		/* Buffer for formatted string */
  va_list	ap;			/* Variable argument pointer */


  DEBUG_printf(("httpPrintf(http=%p, format=\"%s\", ...)\n", http, format));

  va_start(ap, format);
  bytes = vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  DEBUG_printf(("httpPrintf: %s", buf));

  if (http->wused)
  {
    DEBUG_puts("    flushing existing data...");

    if (httpFlushWrite(http) < 0)
      return (-1);
  }

  return (http_write(http, buf, bytes));
}


/*
 * 'httpPut()' - Send a PUT request to the server.
 */

int					/* O - Status of call (0 = success) */
httpPut(http_t     *http,		/* I - HTTP data */
        const char *uri)		/* I - URI to put */
{
  return (http_send(http, HTTP_PUT, uri));
}


/*
 * 'httpRead()' - Read data from a HTTP connection.
 */

int					/* O - Number of bytes read */
httpRead(http_t *http,			/* I - HTTP data */
         char   *buffer,		/* I - Buffer for data */
	 int    length)			/* I - Maximum number of bytes */
{
  int		bytes;			/* Bytes read */
  char		len[32];		/* Length string */


  DEBUG_printf(("httpRead(http=%p, buffer=%p, length=%d)\n",
                http, buffer, length));

  if (http == NULL || buffer == NULL)
    return (-1);

  http->activity = time(NULL);

  if (length <= 0)
    return (0);

  if (http->data_encoding == HTTP_ENCODE_CHUNKED &&
      http->data_remaining <= 0)
  {
    DEBUG_puts("httpRead: Getting chunk length...");

    if (httpGets(len, sizeof(len), http) == NULL)
    {
      DEBUG_puts("httpRead: Could not get length!");
      return (0);
    }

    http->data_remaining = strtoll(len, NULL, 16);
    if (http->data_remaining < 0)
    {
      DEBUG_puts("httpRead: Negative chunk length!");
      return (0);
    }
  }

  DEBUG_printf(("httpRead: data_remaining=%d\n", http->data_remaining));

  if (http->data_remaining <= 0)
  {
   /*
    * A zero-length chunk ends a transfer; unless we are reading POST
    * data, go idle...
    */

    if (http->data_encoding == HTTP_ENCODE_CHUNKED)
      httpGets(len, sizeof(len), http);

    if (http->state == HTTP_POST_RECV)
      http->state ++;
    else
      http->state = HTTP_WAITING;

   /*
    * Prevent future reads for this request...
    */

    http->data_encoding = HTTP_ENCODE_LENGTH;

    return (0);
  }
  else if (length > http->data_remaining)
    length = http->data_remaining;

  if (http->used == 0 && length <= 256)
  {
   /*
    * Buffer small reads for better performance...
    */

    if (!http->blocking && !httpWait(http, 1000))
      return (0);

    if (http->data_remaining > sizeof(http->buffer))
      bytes = sizeof(http->buffer);
    else
      bytes = http->data_remaining;

#ifdef HAVE_SSL
    if (http->tls)
      bytes = http_read_ssl(http, http->buffer, bytes);
    else
#endif /* HAVE_SSL */
    {
      DEBUG_printf(("httpRead: reading %d bytes from socket into buffer...\n",
                    bytes));

      bytes = recv(http->fd, http->buffer, bytes, 0);

      DEBUG_printf(("httpRead: read %d bytes from socket into buffer...\n",
                    bytes));
    }

    if (bytes > 0)
      http->used = bytes;
    else if (bytes < 0)
    {
#ifdef WIN32
      http->error = WSAGetLastError();
      return (-1);
#else
      if (errno != EINTR)
      {
        http->error = errno;
        return (-1);
      }
#endif /* WIN32 */
    }
    else
    {
      http->error = EPIPE;
      return (0);
    }
  }

  if (http->used > 0)
  {
    if (length > http->used)
      length = http->used;

    bytes = length;

    DEBUG_printf(("httpRead: grabbing %d bytes from input buffer...\n", bytes));

    memcpy(buffer, http->buffer, length);
    http->used -= length;

    if (http->used > 0)
      memmove(http->buffer, http->buffer + length, http->used);
  }
#ifdef HAVE_SSL
  else if (http->tls)
  {
    if (!http->blocking && !httpWait(http, 1000))
      return (0);

    bytes = http_read_ssl(http, buffer, length);
  }
#endif /* HAVE_SSL */
  else
  {
    if (!http->blocking && !httpWait(http, 1000))
      return (0);

    DEBUG_printf(("httpRead: reading %d bytes from socket...\n", length));

    while ((bytes = recv(http->fd, buffer, length, 0)) < 0)
      if (errno != EINTR)
        break;

    DEBUG_printf(("httpRead: read %d bytes from socket...\n", bytes));
  }

  if (bytes > 0)
  {
    http->data_remaining -= bytes;

    if (http->data_remaining <= INT_MAX)
      http->_data_remaining = (int)http->data_remaining;
    else
      http->_data_remaining = INT_MAX;
  }
  else if (bytes < 0)
  {
#ifdef WIN32
    http->error = WSAGetLastError();
#else
    if (errno == EINTR)
      bytes = 0;
    else
      http->error = errno;
#endif /* WIN32 */
  }
  else
  {
    http->error = EPIPE;
    return (0);
  }

  if (http->data_remaining == 0)
  {
    if (http->data_encoding == HTTP_ENCODE_CHUNKED)
      httpGets(len, sizeof(len), http);

    if (http->data_encoding != HTTP_ENCODE_CHUNKED)
    {
      if (http->state == HTTP_POST_RECV)
	http->state ++;
      else
	http->state = HTTP_WAITING;
    }
  }

#ifdef DEBUG
  {
    int i, j, ch;
    printf("httpRead: Read %d bytes:\n", bytes);
    for (i = 0; i < bytes; i += 16)
    {
      printf("   ");

      for (j = 0; j < 16 && (i + j) < bytes; j ++)
        printf(" %02X", buffer[i + j] & 255);

      while (j < 16)
      {
        printf("   ");
	j ++;
      }

      printf("    ");
      for (j = 0; j < 16 && (i + j) < bytes; j ++)
      {
        ch = buffer[i + j] & 255;

	if (ch < ' ' || ch == 127)
	  ch = '.';

        putchar(ch);
      }
      putchar('\n');
    }
  }
#endif /* DEBUG */

  return (bytes);
}


/*
 * 'httpReconnect()' - Reconnect to a HTTP server...
 */

int					/* O - 0 on success, non-zero on failure */
httpReconnect(http_t *http)		/* I - HTTP data */
{
  int		val;			/* Socket option value */
  int		status;			/* Connect status */


  DEBUG_printf(("httpReconnect(http=%p)\n", http));

  if (!http)
    return (-1);

#ifdef HAVE_SSL
  if (http->tls)
    http_shutdown_ssl(http);
#endif /* HAVE_SSL */

 /*
  * Close any previously open socket...
  */

  if (http->fd >= 0)
#ifdef WIN32
    closesocket(http->fd);
#else
    close(http->fd);
#endif /* WIN32 */

 /*
  * Create the socket and set options to allow reuse.
  */

  if ((http->fd = socket(http->hostaddr.addr.sa_family, SOCK_STREAM, 0)) < 0)
  {
#ifdef WIN32
    http->error  = WSAGetLastError();
#else
    http->error  = errno;
#endif /* WIN32 */
    http->status = HTTP_ERROR;
    return (-1);
  }

#ifdef FD_CLOEXEC
  fcntl(http->fd, F_SETFD, FD_CLOEXEC);	/* Close this socket when starting *
					 * other processes...              */
#endif /* FD_CLOEXEC */

  val = 1;
  setsockopt(http->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&val, sizeof(val));

#ifdef SO_REUSEPORT
  val = 1;
  setsockopt(http->fd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
#endif /* SO_REUSEPORT */

 /*
  * Using TCP_NODELAY improves responsiveness, especially on systems
  * with a slow loopback interface...  Since we write large buffers
  * when sending print files and requests, there shouldn't be any
  * performance penalty for this...
  */

  val = 1;
#ifdef WIN32
  setsockopt(http->fd, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(val)); 
#else
  setsockopt(http->fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)); 
#endif /* WIN32 */

 /*
  * Connect to the server...
  */

#ifdef AF_INET6
  if (http->hostaddr.addr.sa_family == AF_INET6)
    status = connect(http->fd, (struct sockaddr *)&(http->hostaddr),
                     sizeof(http->hostaddr.ipv6));
  else
#endif /* AF_INET6 */
#ifdef AF_LOCAL
  if (http->hostaddr.addr.sa_family == AF_LOCAL)
    status = connect(http->fd, (struct sockaddr *)&(http->hostaddr),
                     SUN_LEN(&(http->hostaddr.un)));
  else
#endif /* AF_LOCAL */
  status = connect(http->fd, (struct sockaddr *)&(http->hostaddr),
                   sizeof(http->hostaddr.ipv4));

  if (status < 0)
  {
#ifdef WIN32
    http->error  = WSAGetLastError();
#else
    http->error  = errno;
#endif /* WIN32 */
    http->status = HTTP_ERROR;

#ifdef WIN32
    closesocket(http->fd);
#else
    close(http->fd);
#endif

    http->fd = -1;

    return (-1);
  }

  http->error  = 0;
  http->status = HTTP_CONTINUE;

#ifdef HAVE_SSL
  if (http->encryption == HTTP_ENCRYPT_ALWAYS)
  {
   /*
    * Always do encryption via SSL.
    */

    if (http_setup_ssl(http) != 0)
    {
#ifdef WIN32
      closesocket(http->fd);
#else
      close(http->fd);
#endif /* WIN32 */

      return (-1);
    }
  }
  else if (http->encryption == HTTP_ENCRYPT_REQUIRED)
    return (http_upgrade(http));
#endif /* HAVE_SSL */

  return (0);
}


/*
 * 'httpSetCookie()' - Set the cookie value(s)...
 */

void
httpSetCookie(http_t     *http,		/* I - Connection */
              const char *cookie)	/* I - Cookie string */
{
  if (!http)
    return;

  if (http->cookie)
    free(http->cookie);

  if (cookie)
    http->cookie = strdup(cookie);
  else
    http->cookie = NULL;
}


/*
 * 'httpSetField()' - Set the value of an HTTP header.
 */

void
httpSetField(http_t       *http,	/* I - HTTP data */
             http_field_t field,	/* I - Field index */
	     const char   *value)	/* I - Value */
{
  if (http == NULL ||
      field < HTTP_FIELD_ACCEPT_LANGUAGE ||
      field > HTTP_FIELD_WWW_AUTHENTICATE ||
      value == NULL)
    return;

  strlcpy(http->fields[field], value, HTTP_MAX_VALUE);
}


/*
 * 'httpSetLength()' - Set the content-length and content-encoding.
 */

void
httpSetLength(http_t *http,		/* I - HTTP data */
              off_t  length)		/* I - Length (0 for chunked) */
{
  if (!http || length < 0)
    return;

  if (!length)
  {
    strcpy(http->fields[HTTP_FIELD_TRANSFER_ENCODING], "chunked");
    http->fields[HTTP_FIELD_CONTENT_LENGTH][0] = '\0';
  }
  else
  {
    http->fields[HTTP_FIELD_TRANSFER_ENCODING][0] = '\0';
    snprintf(http->fields[HTTP_FIELD_CONTENT_LENGTH], HTTP_MAX_VALUE,
             CUPS_LLFMT, CUPS_LLCAST length);
  }
}


/*
 * 'httpTrace()' - Send an TRACE request to the server.
 */

int					/* O - Status of call (0 = success) */
httpTrace(http_t     *http,		/* I - HTTP data */
          const char *uri)		/* I - URI for trace */
{
  return (http_send(http, HTTP_TRACE, uri));
}


/*
 * 'httpUpdate()' - Update the current HTTP state for incoming data.
 */

http_status_t				/* O - HTTP status */
httpUpdate(http_t *http)		/* I - HTTP data */
{
  char		line[32768],		/* Line from connection... */
		*value;			/* Pointer to value on line */
  http_field_t	field;			/* Field index */
  int		major, minor,		/* HTTP version numbers */
		status;			/* Request status */


  DEBUG_printf(("httpUpdate(http=%p), state=%d\n", http, http->state));

 /*
  * Flush pending data, if any...
  */

  if (http->wused)
  {
    DEBUG_puts("    flushing buffer...");

    if (httpFlushWrite(http) < 0)
      return (HTTP_ERROR);
  }

 /*
  * If we haven't issued any commands, then there is nothing to "update"...
  */

  if (http->state == HTTP_WAITING)
    return (HTTP_CONTINUE);

 /*
  * Grab all of the lines we can from the connection...
  */

  while (httpGets(line, sizeof(line), http) != NULL)
  {
    DEBUG_printf(("httpUpdate: Got \"%s\"\n", line));

    if (line[0] == '\0')
    {
     /*
      * Blank line means the start of the data section (if any).  Return
      * the result code, too...
      *
      * If we get status 100 (HTTP_CONTINUE), then we *don't* change states.
      * Instead, we just return HTTP_CONTINUE to the caller and keep on
      * tryin'...
      */

      if (http->status == HTTP_CONTINUE)
        return (http->status);

      if (http->status < HTTP_BAD_REQUEST)
        http->digest_tries = 0;

#ifdef HAVE_SSL
      if (http->status == HTTP_SWITCHING_PROTOCOLS && !http->tls)
      {
	if (http_setup_ssl(http) != 0)
	{
#  ifdef WIN32
	  closesocket(http->fd);
#  else
	  close(http->fd);
#  endif /* WIN32 */

	  return (HTTP_ERROR);
	}

        return (HTTP_CONTINUE);
      }
#endif /* HAVE_SSL */

      httpGetLength2(http);

      switch (http->state)
      {
        case HTTP_GET :
	case HTTP_POST :
	case HTTP_POST_RECV :
	case HTTP_PUT :
	    http->state ++;
	case HTTP_POST_SEND :
	    break;

	default :
	    http->state = HTTP_WAITING;
	    break;
      }

      return (http->status);
    }
    else if (strncmp(line, "HTTP/", 5) == 0)
    {
     /*
      * Got the beginning of a response...
      */

      if (sscanf(line, "HTTP/%d.%d%d", &major, &minor, &status) != 3)
        return (HTTP_ERROR);

      http->version = (http_version_t)(major * 100 + minor);
      http->status  = (http_status_t)status;
    }
    else if ((value = strchr(line, ':')) != NULL)
    {
     /*
      * Got a value...
      */

      *value++ = '\0';
      while (isspace(*value & 255))
        value ++;

     /*
      * Be tolerants of servers that send unknown attribute fields...
      */

      if (!strcasecmp(line, "expect"))
      {
       /*
        * "Expect: 100-continue" or similar...
	*/

        http->expect = (http_status_t)atoi(value);
      }
      else if (!strcasecmp(line, "cookie"))
      {
       /*
        * "Cookie: name=value[; name=value ...]" - replaces previous cookies...
	*/

        httpSetCookie(http, value);
      }
      else if ((field = http_field(line)) == HTTP_FIELD_UNKNOWN)
      {
        DEBUG_printf(("httpUpdate: unknown field %s seen!\n", line));
        continue;
      }
      else
        httpSetField(http, field, value);
    }
    else
    {
      http->status = HTTP_ERROR;
      return (HTTP_ERROR);
    }
  }

 /*
  * See if there was an error...
  */

  if (http->error == EPIPE && http->status > HTTP_CONTINUE)
    return (http->status);

  if (http->error)
  {
    DEBUG_printf(("httpUpdate: socket error %d - %s\n", http->error,
                  strerror(http->error)));
    http->status = HTTP_ERROR;
    return (HTTP_ERROR);
  }

 /*
  * If we haven't already returned, then there is nothing new...
  */

  return (HTTP_CONTINUE);
}


/*
 * 'httpWait()' - Wait for data available on a connection.
 */

int					/* O - 1 if data is available, 0 otherwise */
httpWait(http_t *http,			/* I - HTTP data */
         int    msec)			/* I - Milliseconds to wait */
{
 /*
  * First see if there is data in the buffer...
  */

  if (http == NULL)
    return (0);

  if (http->used)
    return (1);

 /*
  * If not, check the SSL/TLS buffers and do a select() on the connection...
  */

  return (http_wait(http, msec));
}


/*
 * 'httpWrite()' - Write data to a HTTP connection.
 */
 
int					/* O - Number of bytes written */
httpWrite(http_t     *http,		/* I - HTTP data */
          const char *buffer,		/* I - Buffer for data */
	  int        length)		/* I - Number of bytes to write */
{
  int	bytes;				/* Bytes written */


  DEBUG_printf(("httpWrite(http=%p, buffer=%p, length=%d)\n", http,
                buffer, length));

 /*
  * Range check input...
  */

  if (http == NULL || buffer == NULL)
    return (-1);

 /*
  * Mark activity on the connection...
  */

  http->activity = time(NULL);

 /*
  * Buffer small writes for better performance...
  */

  if (length > 0)
  {
    if (http->wused && (length + http->wused) > sizeof(http->wbuffer))
    {
      DEBUG_printf(("    flushing buffer (wused=%d, length=%d)\n",
                    http->wused, length));

      httpFlushWrite(http);
    }

    if ((length + http->wused) <= sizeof(http->wbuffer))
    {
     /*
      * Write to buffer...
      */

      DEBUG_printf(("    copying %d bytes to wbuffer...\n", length));

      memcpy(http->wbuffer + http->wused, buffer, length);
      http->wused += length;
      bytes = length;
    }
    else
    {
     /*
      * Otherwise write the data directly...
      */

      DEBUG_printf(("    writing %d bytes to socket...\n", length));

      if (http->data_encoding == HTTP_ENCODE_CHUNKED)
	bytes = http_write_chunk(http, buffer, length);
      else
	bytes = http_write(http, buffer, length);

      DEBUG_printf(("    wrote %d bytes...\n", bytes));
    }

    if (http->data_encoding == HTTP_ENCODE_LENGTH)
      http->data_remaining -= bytes;
  }

 /*
  * Handle end-of-request processing...
  */

  if ((http->data_encoding == HTTP_ENCODE_CHUNKED && length == 0) ||
      (http->data_encoding == HTTP_ENCODE_LENGTH && http->data_remaining == 0))
  {
   /*
    * Finished with the transfer; unless we are sending POST or PUT
    * data, go idle...
    */

    DEBUG_puts("httpWrite: changing states...");

    if (http->wused)
      httpFlushWrite(http);

    if (http->data_encoding == HTTP_ENCODE_CHUNKED)
    {
     /*
      * Send a 0-length chunk at the end of the request...
      */

      http_write(http, "0\r\n\r\n", 5);

     /*
      * Reset the data state...
      */

      http->data_encoding  = HTTP_ENCODE_LENGTH;
      http->data_remaining = 0;
    }

    if (http->state == HTTP_POST_RECV)
      http->state ++;
    else if (http->state == HTTP_PUT_RECV)
      http->state = HTTP_STATUS;
    else
      http->state = HTTP_WAITING;
  }

  return (bytes);
}


/*
 * 'http_field()' - Return the field index for a field name.
 */

static http_field_t		/* O - Field index */
http_field(const char *name)	/* I - String name */
{
  int	i;			/* Looping var */


  for (i = 0; i < HTTP_FIELD_MAX; i ++)
    if (strcasecmp(name, http_fields[i]) == 0)
      return ((http_field_t)i);

  return (HTTP_FIELD_UNKNOWN);
}


#if defined(HAVE_SSL) && defined(HAVE_CDSASSL)
/*
 * 'http_read_cdsa()' - Read function for CDSA decryption code.
 */

static OSStatus				/* O  - -1 on error, 0 on success */
http_read_cdsa(
    SSLConnectionRef connection,	/* I  - SSL/TLS connection */
    void             *data,		/* I  - Data buffer */
    size_t           *dataLength)	/* IO - Number of bytes */
{
  ssize_t	bytes;			/* Number of bytes read */


  bytes = recv((int)connection, data, *dataLength, 0);
  if (bytes >= 0)
  {
    *dataLength = bytes;
    return (0);
  }
  else
    return (-1);
}
#endif /* HAVE_SSL && HAVE_CDSASSL */


#ifdef HAVE_SSL
/*
 * 'http_read_ssl()' - Read from a SSL/TLS connection.
 */

static int				/* O - Bytes read */
http_read_ssl(http_t *http,		/* I - HTTP data */
	      char   *buf,		/* I - Buffer to store data */
	      int    len)		/* I - Length of buffer */
{
#  if defined(HAVE_LIBSSL)
  return (SSL_read((SSL *)(http->tls), buf, len));

#  elif defined(HAVE_GNUTLS)
  return (gnutls_record_recv(((http_tls_t *)(http->tls))->session, buf, len));

#  elif defined(HAVE_CDSASSL)
  OSStatus	error;			/* Error info */
  size_t	processed;		/* Number of bytes processed */


  error = SSLRead((SSLContextRef)http->tls, buf, len, &processed);

  if (error == 0)
    return (processed);
  else
  {
    http->error = error;

    return (-1);
  }
#  endif /* HAVE_LIBSSL */
}
#endif /* HAVE_SSL */


/*
 * 'http_send()' - Send a request with all fields and the trailing blank line.
 */

static int			/* O - 0 on success, non-zero on error */
http_send(http_t       *http,	/* I - HTTP data */
          http_state_t request,	/* I - Request code */
	  const char   *uri)	/* I - URI */
{
  int		i;		/* Looping var */
  char		*ptr,		/* Pointer in buffer */
		buf[1024];	/* Encoded URI buffer */
  static const char * const codes[] =
		{		/* Request code strings */
		  NULL,
		  "OPTIONS",
		  "GET",
		  NULL,
		  "HEAD",
		  "POST",
		  NULL,
		  NULL,
		  "PUT",
		  NULL,
		  "DELETE",
		  "TRACE",
		  "CLOSE"
		};
  static const char hex[] = "0123456789ABCDEF";
				/* Hex digits */


  DEBUG_printf(("http_send(http=%p, request=HTTP_%s, uri=\"%s\")\n",
                http, codes[request], uri));

  if (http == NULL || uri == NULL)
    return (-1);

 /*
  * Encode the URI as needed...
  */

  for (ptr = buf; *uri != '\0' && ptr < (buf + sizeof(buf) - 1); uri ++)
    if (*uri <= ' ' || *uri >= 127)
    {
      if (ptr < (buf + sizeof(buf) - 1))
        *ptr ++ = '%';
      if (ptr < (buf + sizeof(buf) - 1))
        *ptr ++ = hex[(*uri >> 4) & 15];
      if (ptr < (buf + sizeof(buf) - 1))
        *ptr ++ = hex[*uri & 15];
    }
    else
      *ptr ++ = *uri;

  *ptr = '\0';

 /*
  * See if we had an error the last time around; if so, reconnect...
  */

  if (http->status == HTTP_ERROR || http->status >= HTTP_BAD_REQUEST)
    httpReconnect(http);

 /*
  * Send the request header...
  */

  http->state = request;
  if (request == HTTP_POST || request == HTTP_PUT)
    http->state ++;

  http->status = HTTP_CONTINUE;

#ifdef HAVE_SSL
  if (http->encryption == HTTP_ENCRYPT_REQUIRED && !http->tls)
  {
    httpSetField(http, HTTP_FIELD_CONNECTION, "Upgrade");
    httpSetField(http, HTTP_FIELD_UPGRADE, "TLS/1.0,SSL/2.0,SSL/3.0");
  }
#endif /* HAVE_SSL */

  if (httpPrintf(http, "%s %s HTTP/1.1\r\n", codes[request], buf) < 1)
  {
    http->status = HTTP_ERROR;
    return (-1);
  }

  for (i = 0; i < HTTP_FIELD_MAX; i ++)
    if (http->fields[i][0] != '\0')
    {
      DEBUG_printf(("%s: %s\n", http_fields[i], http->fields[i]));

      if (httpPrintf(http, "%s: %s\r\n", http_fields[i], http->fields[i]) < 1)
      {
	http->status = HTTP_ERROR;
	return (-1);
      }
    }

  if (httpPrintf(http, "\r\n") < 1)
  {
    http->status = HTTP_ERROR;
    return (-1);
  }

  httpGetLength2(http);
  httpClearFields(http);

  return (0);
}


#ifdef HAVE_SSL
/*
 * 'http_setup_ssl()' - Set up SSL/TLS support on a connection.
 */

static int				/* O - Status of connection */
http_setup_ssl(http_t *http)		/* I - HTTP data */
{
#  ifdef HAVE_LIBSSL
  SSL_CTX	*context;	/* Context for encryption */
  SSL		*conn;		/* Connection for encryption */
#  elif defined(HAVE_GNUTLS)
  http_tls_t	*conn;		/* TLS session object */
  gnutls_certificate_client_credentials *credentials;
				/* TLS credentials */
#  elif defined(HAVE_CDSASSL)
  SSLContextRef	conn;		/* Context for encryption */
  OSStatus	error;		/* Error info */
#  endif /* HAVE_LIBSSL */


  DEBUG_printf(("http_setup_ssl(http=%p)\n", http));

#  ifdef HAVE_LIBSSL
  context = SSL_CTX_new(SSLv23_client_method());

  SSL_CTX_set_options(context, SSL_OP_NO_SSLv2); /* Only use SSLv3 or TLS */

  conn = SSL_new(context);

  SSL_set_fd(conn, http->fd);
  if (SSL_connect(conn) != 1)
  {
#    ifdef DEBUG
    unsigned long	error;	/* Error code */

    while ((error = ERR_get_error()) != 0)
      printf("http_setup_ssl: %s\n", ERR_error_string(error, NULL));
#    endif /* DEBUG */

    SSL_CTX_free(context);
    SSL_free(conn);

#    ifdef WIN32
    http->error  = WSAGetLastError();
#    else
    http->error  = errno;
#    endif /* WIN32 */
    http->status = HTTP_ERROR;

    return (HTTP_ERROR);
  }

#  elif defined(HAVE_GNUTLS)
  conn = (http_tls_t *)malloc(sizeof(http_tls_t));

  if (conn == NULL)
  {
    http->error  = errno;
    http->status = HTTP_ERROR;

    return (-1);
  }

  credentials = (gnutls_certificate_client_credentials *)
                    malloc(sizeof(gnutls_certificate_client_credentials));
  if (credentials == NULL)
  {
    free(conn);

    http->error = errno;
    http->status = HTTP_ERROR;

    return (-1);
  }

  gnutls_certificate_allocate_credentials(credentials);

  gnutls_init(&(conn->session), GNUTLS_CLIENT);
  gnutls_set_default_priority(conn->session);
  gnutls_credentials_set(conn->session, GNUTLS_CRD_CERTIFICATE, *credentials);
  gnutls_transport_set_ptr(conn->session, http->fd);

  if ((gnutls_handshake(conn->session)) != GNUTLS_E_SUCCESS)
  {
    http->error  = errno;
    http->status = HTTP_ERROR;

    return (-1);
  }

  conn->credentials = credentials;

#  elif defined(HAVE_CDSASSL)
  error = SSLNewContext(false, &conn);

  if (!error)
    error = SSLSetIOFuncs(conn, http_read_cdsa, http_write_cdsa);

  if (!error)
    error = SSLSetConnection(conn, (SSLConnectionRef)http->fd);

  if (!error)
    error = SSLSetAllowsExpiredCerts(conn, true);

  if (!error)
    error = SSLSetAllowsAnyRoot(conn, true);

  if (!error)
    error = SSLHandshake(conn);

  if (error != 0)
  {
    http->error  = error;
    http->status = HTTP_ERROR;

    SSLDisposeContext(conn);

    close(http->fd);

    return (-1);
  }
#  endif /* HAVE_CDSASSL */

  http->tls = conn;
  return (0);
}
#endif /* HAVE_SSL */


#ifdef HAVE_SSL
/*
 * 'http_shutdown_ssl()' - Shut down SSL/TLS on a connection.
 */

static void
http_shutdown_ssl(http_t *http)	/* I - HTTP data */
{
#  ifdef HAVE_LIBSSL
  SSL_CTX	*context;	/* Context for encryption */
  SSL		*conn;		/* Connection for encryption */


  conn    = (SSL *)(http->tls);
  context = SSL_get_SSL_CTX(conn);

  SSL_shutdown(conn);
  SSL_CTX_free(context);
  SSL_free(conn);

#  elif defined(HAVE_GNUTLS)
  http_tls_t      *conn;	/* Encryption session */
  gnutls_certificate_client_credentials *credentials;
				/* TLS credentials */


  conn = (http_tls_t *)(http->tls);
  credentials = (gnutls_certificate_client_credentials *)(conn->credentials);

  gnutls_bye(conn->session, GNUTLS_SHUT_RDWR);
  gnutls_deinit(conn->session);
  gnutls_certificate_free_credentials(*credentials);
  free(credentials);
  free(conn);

#  elif defined(HAVE_CDSASSL)
  SSLClose((SSLContextRef)http->tls);
  SSLDisposeContext((SSLContextRef)http->tls);
#  endif /* HAVE_LIBSSL */

  http->tls = NULL;
}
#endif /* HAVE_SSL */


#ifdef HAVE_SSL
/*
 * 'http_upgrade()' - Force upgrade to TLS encryption.
 */

static int			/* O - Status of connection */
http_upgrade(http_t *http)	/* I - HTTP data */
{
  int		ret;		/* Return value */
  http_t	myhttp;		/* Local copy of HTTP data */


  DEBUG_printf(("http_upgrade(%p)\n", http));

 /*
  * Copy the HTTP data to a local variable so we can do the OPTIONS
  * request without interfering with the existing request data...
  */

  memcpy(&myhttp, http, sizeof(myhttp));

 /*
  * Send an OPTIONS request to the server, requiring SSL or TLS
  * encryption on the link...
  */

  httpClearFields(&myhttp);
  httpSetField(&myhttp, HTTP_FIELD_CONNECTION, "upgrade");
  httpSetField(&myhttp, HTTP_FIELD_UPGRADE, "TLS/1.0, SSL/2.0, SSL/3.0");

  if ((ret = httpOptions(&myhttp, "*")) == 0)
  {
   /*
    * Wait for the secure connection...
    */

    while (httpUpdate(&myhttp) == HTTP_CONTINUE);
  }

  httpFlush(&myhttp);

 /*
  * Copy the HTTP data back over, if any...
  */

  http->fd         = myhttp.fd;
  http->error      = myhttp.error;
  http->activity   = myhttp.activity;
  http->status     = myhttp.status;
  http->version    = myhttp.version;
  http->keep_alive = myhttp.keep_alive;
  http->used       = myhttp.used;

  if (http->used)
    memcpy(http->buffer, myhttp.buffer, http->used);

  http->auth_type   = myhttp.auth_type;
  http->nonce_count = myhttp.nonce_count;

  memcpy(http->nonce, myhttp.nonce, sizeof(http->nonce));

  http->tls        = myhttp.tls;
  http->encryption = myhttp.encryption;

 /*
  * See if we actually went secure...
  */

  if (!http->tls)
  {
   /*
    * Server does not support HTTP upgrade...
    */

    DEBUG_puts("Server does not support HTTP upgrade!");

#  ifdef WIN32
    closesocket(http->fd);
#  else
    close(http->fd);
#  endif

    http->fd = -1;

    return (-1);
  }
  else
    return (ret);
}
#endif /* HAVE_SSL */


/*
 * 'http_wait()' - Wait for data available on a connection.
 */

static int				/* O - 1 if data is available, 0 otherwise */
http_wait(http_t *http,			/* I - HTTP data */
          int    msec)			/* I - Milliseconds to wait */
{
#ifndef WIN32
  struct rlimit		limit;          /* Runtime limit */
#endif /* !WIN32 */
  struct timeval	timeout;	/* Timeout */
  int			nfds;		/* Result from select() */
  int			set_size;	/* Size of select set */


  DEBUG_printf(("http_wait(http=%p, msec=%d)\n", http, msec));

 /*
  * Check the SSL/TLS buffers for data first...
  */

#ifdef HAVE_SSL
  if (http->tls)
  {
#  ifdef HAVE_LIBSSL
    if (SSL_pending((SSL *)(http->tls)))
      return (1);
#  elif defined(HAVE_GNUTLS)
    if (gnutls_record_check_pending(((http_tls_t *)(http->tls))->session))
      return (1);
#  elif defined(HAVE_CDSASSL)
    size_t bytes;			/* Bytes that are available */

    if (!SSLGetBufferedReadSize((SSLContextRef)http->tls, &bytes) && bytes > 0)
      return (1);
#  endif /* HAVE_LIBSSL */
  }
#endif /* HAVE_SSL */

 /*
  * Then try doing a select() to poll the socket...
  */

  if (!http->input_set)
  {
#ifdef WIN32
   /*
    * Windows has a fixed-size select() structure, different (surprise,
    * surprise!) from all UNIX implementations.  Just allocate this
    * fixed structure...
    */

    http->input_set = calloc(1, sizeof(fd_set));
#else
   /*
    * Allocate the select() input set based upon the max number of file
    * descriptors available for this process...
    */

    getrlimit(RLIMIT_NOFILE, &limit);

    set_size = (limit.rlim_cur + 31) / 8 + 4;
    if (set_size < sizeof(fd_set))
      set_size = sizeof(fd_set);

    http->input_set = calloc(1, set_size);
#endif /* WIN32 */

    if (!http->input_set)
      return (0);
  }

  do
  {
    FD_SET(http->fd, http->input_set);

    if (msec >= 0)
    {
      timeout.tv_sec  = msec / 1000;
      timeout.tv_usec = (msec % 1000) * 1000;

      nfds = select(http->fd + 1, http->input_set, NULL, NULL, &timeout);
    }
    else
      nfds = select(http->fd + 1, http->input_set, NULL, NULL, NULL);
  }
#ifdef WIN32
  while (nfds < 0 && WSAGetLastError() == WSAEINTR);
#else
  while (nfds < 0 && errno == EINTR);
#endif /* WIN32 */

  FD_CLR(http->fd, http->input_set);

  return (nfds > 0);
}


/*
 * 'http_write()' - Write a buffer to a HTTP connection.
 */
 
static int				/* O - Number of bytes written */
http_write(http_t     *http,		/* I - HTTP data */
          const char *buffer,		/* I - Buffer for data */
	  int        length)		/* I - Number of bytes to write */
{
  int	tbytes,				/* Total bytes sent */
	bytes;				/* Bytes sent */


  tbytes = 0;

  while (length > 0)
  {
#ifdef HAVE_SSL
    if (http->tls)
      bytes = http_write_ssl(http, buffer, length);
    else
#endif /* HAVE_SSL */
    bytes = send(http->fd, buffer, length, 0);

    if (bytes < 0)
    {
#ifdef WIN32
      if (WSAGetLastError() != http->error)
      {
        http->error = WSAGetLastError();
	continue;
      }
#else
      if (errno == EINTR)
        continue;
      else if (errno != http->error && errno != ECONNRESET)
      {
        http->error = errno;
	continue;
      }
#endif /* WIN32 */

      DEBUG_puts("http_write: error writing data...\n");

      return (-1);
    }

    buffer += bytes;
    tbytes += bytes;
    length -= bytes;
  }

#ifdef DEBUG
  {
    int i, j, ch;
    printf("http_write: wrote %d bytes: \n", tbytes);
    for (i = 0, buffer -= tbytes; i < tbytes; i += 16)
    {
      printf("   ");

      for (j = 0; j < 16 && (i + j) < tbytes; j ++)
        printf(" %02X", buffer[i + j] & 255);

      while (j < 16)
      {
        printf("   ");
	j ++;
      }

      printf("    ");
      for (j = 0; j < 16 && (i + j) < tbytes; j ++)
      {
        ch = buffer[i + j] & 255;

	if (ch < ' ' || ch == 127)
	  ch = '.';

        putchar(ch);
      }
      putchar('\n');
    }
  }
#endif /* DEBUG */

  return (tbytes);
}


#if defined(HAVE_SSL) && defined(HAVE_CDSASSL)
/*
 * 'http_write_cdsa()' - Write function for CDSA encryption code.
 */

static OSStatus				/* O  - -1 on error, 0 on success */
http_write_cdsa(
    SSLConnectionRef connection,	/* I  - SSL/TLS connection */
    const void       *data,		/* I  - Data buffer */
    size_t           *dataLength)	/* IO - Number of bytes */
{
  ssize_t bytes;			/* Number of write written */


  bytes = write((int)connection, data, *dataLength);
  if (bytes >= 0)
  {
    *dataLength = bytes;
    return (0);
  }
  else
    return (-1);
}
#endif /* HAVE_SSL && HAVE_CDSASSL */


/*
 * 'http_write_chunk()' - Write a chunked buffer.
 */

static int				/* O - Number bytes written */
http_write_chunk(http_t     *http,	/* I - HTTP data */
                 const char *buffer,	/* I - Buffer to write */
		 int        length)	/* I - Length of buffer */
{
  char	header[255];			/* Chunk header */
  int	bytes;				/* Bytes written */

  DEBUG_printf(("http_write_chunk(http=%p, buffer=%p, length=%d)\n",
                http, buffer, length));

 /*
  * Write the chunk header, data, and trailer.
  */

  sprintf(header, "%x\r\n", length);
  if (http_write(http, header, strlen(header)) < 0)
  {
    DEBUG_puts("    http_write of length failed!");
    return (-1);
  }

  if ((bytes = http_write(http, buffer, length)) < 0)
  {
    DEBUG_puts("    http_write of buffer failed!");
    return (-1);
  }

  if (http_write(http, "\r\n", 2) < 0)
  {
    DEBUG_puts("    http_write of CR LF failed!");
    return (-1);
  }

  return (bytes);
}


#ifdef HAVE_SSL
/*
 * 'http_write_ssl()' - Write to a SSL/TLS connection.
 */

static int				/* O - Bytes written */
http_write_ssl(http_t     *http,	/* I - HTTP data */
	       const char *buf,		/* I - Buffer holding data */
	       int        len)		/* I - Length of buffer */
{
#  if defined(HAVE_LIBSSL)
  return (SSL_write((SSL *)(http->tls), buf, len));

#  elif defined(HAVE_GNUTLS)
  return (gnutls_record_send(((http_tls_t *)(http->tls))->session, buf, len));
#  elif defined(HAVE_CDSASSL)
  OSStatus	error;			/* Error info */
  size_t	processed;		/* Number of bytes processed */


  error = SSLWrite((SSLContextRef)http->tls, buf, len, &processed);

  if (error == 0)
    return (processed);
  else
  {
    http->error = error;
    return (-1);
  }
#  endif /* HAVE_LIBSSL */
}
#endif /* HAVE_SSL */


/*
 * End of "$Id$".
 */
