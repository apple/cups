/*
 * "$Id$"
 *
 *   HTTP support routines for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
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
 *   httpAssembleURI()    - Assemble a uniform resource identifier from its
 *                          components.
 *   httpAssembleURIf()   - Assemble a uniform resource identifier from its
 *                          components with a formatted resource.
 *   httpDecode64()       - Base64-decode a string.
 *   httpDecode64_2()     - Base64-decode a string.
 *   httpEncode64()       - Base64-encode a string.
 *   httpEncode64_2()     - Base64-encode a string.
 *   httpGetDateString()  - Get a formatted date/time string from a time value.
 *   httpGetDateString2() - Get a formatted date/time string from a time value.
 *   httpGetDateTime()    - Get a time value from a formatted date/time string.
 *   httpSeparate()       - Separate a Universal Resource Identifier into its
 *                          components.
 *   httpSeparate2()      - Separate a Universal Resource Identifier into its
 *                          components.
 *   httpSeparateURI()    - Separate a Universal Resource Identifier into its
 *                          components.
 *   httpStatus()         - Return a short string describing a HTTP status code.
 *   _cups_hstrerror()    - hstrerror() emulation function for Solaris and
 *                          others...
 *   _httpEncodeURI()     - Percent-encode a HTTP request URI.
 *   _httpResolveURI()    - Resolve a DNS-SD URI.
 *   http_copy_decode()   - Copy and decode a URI.
 *   http_copy_encode()   - Copy and encode a URI.
 *   resolve_callback()   - Build a device URI for the given service name.
 */

/*
 * Include necessary headers...
 */

#include "debug.h"
#include "globals.h"
#include <stdlib.h>
#ifdef HAVE_DNSSD
#  include <dns_sd.h>
#endif /* HAVE_DNSSD */


/*
 * Local types...
 */

typedef struct _http_uribuf_s		/* URI buffer */
{
  char		*buffer;		/* Pointer to buffer */
  size_t	bufsize;		/* Size of buffer */
} _http_uribuf_t;


/*
 * Local globals...
 */

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
 * Local functions...
 */

static const char	*http_copy_decode(char *dst, const char *src,
			                  int dstsize, const char *term,
					  int decode);
static char		*http_copy_encode(char *dst, const char *src,
			                  char *dstend, const char *reserved,
					  const char *term, int encode);
#ifdef HAVE_DNSSD
static void		resolve_callback(DNSServiceRef sdRef,
					 DNSServiceFlags flags,
					 uint32_t interfaceIndex,
					 DNSServiceErrorType errorCode,
					 const char *fullName,
					 const char *hostTarget,
					 uint16_t port, uint16_t txtLen,
					 const unsigned char *txtRecord,
					 void *context);
#endif /* HAVE_DNSSD */


/*
 * 'httpAssembleURI()' - Assemble a uniform resource identifier from its
 *                       components.
 *
 * This function escapes reserved characters in the URI depending on the
 * value of the "encoding" argument.  You should use this function in
 * place of traditional string functions whenever you need to create a
 * URI string.
 *
 * @since CUPS 1.2@
 */

http_uri_status_t			/* O - URI status */
httpAssembleURI(
    http_uri_coding_t encoding,		/* I - Encoding flags */
    char              *uri,		/* I - URI buffer */
    int               urilen,		/* I - Size of URI buffer */
    const char        *scheme,		/* I - Scheme name */
    const char        *username,	/* I - Username */
    const char        *host,		/* I - Hostname or address */
    int               port,		/* I - Port number */
    const char        *resource)	/* I - Resource */
{
  char		*ptr,			/* Pointer into URI buffer */
		*end;			/* End of URI buffer */


 /*
  * Range check input...
  */

  if (!uri || urilen < 1 || !scheme || port < 0)
  {
    if (uri)
      *uri = '\0';

    return (HTTP_URI_BAD_ARGUMENTS);
  }

 /*
  * Assemble the URI starting with the scheme...
  */

  end = uri + urilen - 1;
  ptr = http_copy_encode(uri, scheme, end, NULL, NULL, 0);

  if (!ptr)
    goto assemble_overflow;

  if (!strcmp(scheme, "mailto"))
  {
   /*
    * mailto: only has :, no //...
    */

    if (ptr < end)
      *ptr++ = ':';
    else
      goto assemble_overflow;
  }
  else
  {
   /*
    * Schemes other than mailto: all have //...
    */

    if ((ptr + 2) < end)
    {
      *ptr++ = ':';
      *ptr++ = '/';
      *ptr++ = '/';
    }
    else
      goto assemble_overflow;
  }

 /*
  * Next the username and hostname, if any...
  */

  if (host)
  {
    if (username && *username)
    {
     /*
      * Add username@ first...
      */

      ptr = http_copy_encode(ptr, username, end, "/?@", NULL,
                             encoding & HTTP_URI_CODING_USERNAME);

      if (!ptr)
        goto assemble_overflow;

      if (ptr < end)
	*ptr++ = '@';
      else
        goto assemble_overflow;
    }

   /*
    * Then add the hostname.  Since IPv6 is a particular pain to deal
    * with, we have several special cases to deal with.  If we get
    * an IPv6 address with brackets around it, assume it is already in
    * URI format.  Since DNS-SD service names can sometimes look like
    * raw IPv6 addresses, we specifically look for "._tcp" in the name,
    * too...
    */

    if (host[0] != '[' && strchr(host, ':') && !strstr(host, "._tcp"))
    {
     /*
      * We have a raw IPv6 address...
      */

      if (strchr(host, '%'))
      {
       /*
        * We have a link-local address, add "[v1." prefix...
	*/

	if ((ptr + 4) < end)
	{
	  *ptr++ = '[';
	  *ptr++ = 'v';
	  *ptr++ = '1';
	  *ptr++ = '.';
	}
	else
          goto assemble_overflow;
      }
      else
      {
       /*
        * We have a normal address, add "[" prefix...
	*/

	if (ptr < end)
	  *ptr++ = '[';
	else
          goto assemble_overflow;
      }

     /*
      * Copy the rest of the IPv6 address, and terminate with "]".
      */

      while (ptr < end && *host)
      {
        if (*host == '%')
	{
          *ptr++ = '+';			/* Convert zone separator */
	  host ++;
	}
	else
	  *ptr++ = *host++;
      }

      if (*host)
        goto assemble_overflow;

      if (ptr < end)
	*ptr++ = ']';
      else
        goto assemble_overflow;
    }
    else
    {
     /*
      * Otherwise, just copy the host string...
      */

      ptr = http_copy_encode(ptr, host, end, ":/?#[]@", NULL,
                             encoding & HTTP_URI_CODING_HOSTNAME);

      if (!ptr)
        goto assemble_overflow;
    }

   /*
    * Finish things off with the port number...
    */

    if (port > 0)
    {
      snprintf(ptr, end - ptr + 1, ":%d", port);
      ptr += strlen(ptr);

      if (ptr >= end)
	goto assemble_overflow;
    }
  }

 /*
  * Last but not least, add the resource string...
  */

  if (resource)
  {
    char	*query;			/* Pointer to query string */


   /*
    * Copy the resource string up to the query string if present...
    */

    query = strchr(resource, '?');
    ptr   = http_copy_encode(ptr, resource, end, NULL, "?",
                             encoding & HTTP_URI_CODING_RESOURCE);
    if (!ptr)
      goto assemble_overflow;

    if (query)
    {
     /*
      * Copy query string without encoding...
      */

      ptr = http_copy_encode(ptr, query, end, NULL, NULL,
			     encoding & HTTP_URI_CODING_QUERY);
      if (!ptr)
	goto assemble_overflow;
    }
  }
  else if (ptr < end)
    *ptr++ = '/';
  else
    goto assemble_overflow;

 /*
  * Nul-terminate the URI buffer and return with no errors...
  */

  *ptr = '\0';

  return (HTTP_URI_OK);

 /*
  * Clear the URI string and return an overflow error; I don't usually
  * like goto's, but in this case it makes sense...
  */

  assemble_overflow:

  *uri = '\0';
  return (HTTP_URI_OVERFLOW);
}


/*
 * 'httpAssembleURIf()' - Assemble a uniform resource identifier from its
 *                        components with a formatted resource.
 *
 * This function creates a formatted version of the resource string
 * argument "resourcef" and escapes reserved characters in the URI
 * depending on the value of the "encoding" argument.  You should use
 * this function in place of traditional string functions whenever
 * you need to create a URI string.
 *
 * @since CUPS 1.2@
 */

http_uri_status_t			/* O - URI status */
httpAssembleURIf(
    http_uri_coding_t encoding,		/* I - Encoding flags */
    char              *uri,		/* I - URI buffer */
    int               urilen,		/* I - Size of URI buffer */
    const char        *scheme,		/* I - Scheme name */
    const char        *username,	/* I - Username */
    const char        *host,		/* I - Hostname or address */
    int               port,		/* I - Port number */
    const char        *resourcef,	/* I - Printf-style resource */
    ...)				/* I - Additional arguments as needed */
{
  va_list	ap;			/* Pointer to additional arguments */
  char		resource[1024];		/* Formatted resource string */
  int		bytes;			/* Bytes in formatted string */


 /*
  * Range check input...
  */

  if (!uri || urilen < 1 || !scheme || port < 0 || !resourcef)
  {
    if (uri)
      *uri = '\0';

    return (HTTP_URI_BAD_ARGUMENTS);
  }

 /*
  * Format the resource string and assemble the URI...
  */

  va_start(ap, resourcef);
  bytes = vsnprintf(resource, sizeof(resource), resourcef, ap);
  va_end(ap);

  if (bytes >= sizeof(resource))
  {
    *uri = '\0';
    return (HTTP_URI_OVERFLOW);
  }
  else
    return (httpAssembleURI(encoding,  uri, urilen, scheme, username, host,
                            port, resource));
}


/*
 * 'httpDecode64()' - Base64-decode a string.
 *
 * This function is deprecated. Use the httpDecode64_2() function instead
 * which provides buffer length arguments.
 *
 * @deprecated@
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
 *
 * @since CUPS 1.1.21@
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

  if (!out || !outlen || *outlen < 1 || !in)
    return (NULL);

  if (!*in)
  {
    *out    = '\0';
    *outlen = 0;

    return (out);
  }

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
 * 'httpEncode64()' - Base64-encode a string.
 *
 * This function is deprecated. Use the httpEncode64_2() function instead
 * which provides buffer length arguments.
 *
 * @deprecated@
 */

char *					/* O - Encoded string */
httpEncode64(char       *out,		/* I - String to write to */
             const char *in)		/* I - String to read from */
{
  return (httpEncode64_2(out, 512, in, (int)strlen(in)));
}


/*
 * 'httpEncode64_2()' - Base64-encode a string.
 *
 * @since CUPS 1.1.21@
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

  if (!out || outlen < 1 || !in)
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
    {
      if (inlen > 1)
        *outptr ++ = base64[(((in[0] & 255) << 4) | ((in[1] & 255) >> 4)) & 63];
      else
        *outptr ++ = base64[((in[0] & 255) << 4) & 63];
    }

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
    {
      if (inlen > 1)
        *outptr ++ = base64[(((in[0] & 255) << 2) | ((in[1] & 255) >> 6)) & 63];
      else
        *outptr ++ = base64[((in[0] & 255) << 2) & 63];
    }

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
 * 'httpGetDateString()' - Get a formatted date/time string from a time value.
 *
 * @deprecated@
 */

const char *				/* O - Date/time string */
httpGetDateString(time_t t)		/* I - UNIX time */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  return (httpGetDateString2(t, cg->http_date, sizeof(cg->http_date)));
}


/*
 * 'httpGetDateString2()' - Get a formatted date/time string from a time value.
 *
 * @since CUPS 1.2@
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
 * 'httpSeparate()' - Separate a Universal Resource Identifier into its
 *                    components.
 *
 * This function is deprecated; use the httpSeparateURI() function instead.
 *
 * @deprecated@
 */

void
httpSeparate(const char *uri,		/* I - Universal Resource Identifier */
             char       *scheme,	/* O - Scheme [32] (http, https, etc.) */
	     char       *username,	/* O - Username [1024] */
	     char       *host,		/* O - Hostname [1024] */
	     int        *port,		/* O - Port number to use */
             char       *resource)	/* O - Resource/filename [1024] */
{
  httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, 32, username,
                  HTTP_MAX_URI, host, HTTP_MAX_URI, port, resource,
		  HTTP_MAX_URI);
}


/*
 * 'httpSeparate2()' - Separate a Universal Resource Identifier into its
 *                     components.
 *
 * This function is deprecated; use the httpSeparateURI() function instead.
 *
 * @since CUPS 1.1.21@
 * @deprecated@
 */

void
httpSeparate2(const char *uri,		/* I - Universal Resource Identifier */
              char       *scheme,	/* O - Scheme (http, https, etc.) */
	      int        schemelen,	/* I - Size of scheme buffer */
	      char       *username,	/* O - Username */
	      int        usernamelen,	/* I - Size of username buffer */
	      char       *host,		/* O - Hostname */
	      int        hostlen,	/* I - Size of hostname buffer */
	      int        *port,		/* O - Port number to use */
              char       *resource,	/* O - Resource/filename */
	      int        resourcelen)	/* I - Size of resource buffer */
{
  httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, schemelen, username,
                  usernamelen, host, hostlen, port, resource, resourcelen);
}


/*
 * 'httpSeparateURI()' - Separate a Universal Resource Identifier into its
 *                       components.
 *
 * @since CUPS 1.2@
 */

http_uri_status_t			/* O - Result of separation */
httpSeparateURI(
    http_uri_coding_t decoding,		/* I - Decoding flags */
    const char        *uri,		/* I - Universal Resource Identifier */
    char              *scheme,		/* O - Scheme (http, https, etc.) */
    int               schemelen,	/* I - Size of scheme buffer */
    char              *username,	/* O - Username */
    int               usernamelen,	/* I - Size of username buffer */
    char              *host,		/* O - Hostname */
    int               hostlen,		/* I - Size of hostname buffer */
    int               *port,		/* O - Port number to use */
    char              *resource,	/* O - Resource/filename */
    int               resourcelen)	/* I - Size of resource buffer */
{
  char			*ptr,		/* Pointer into string... */
			*end;		/* End of string */
  const char		*sep;		/* Separator character */
  http_uri_status_t	status;		/* Result of separation */


 /*
  * Initialize everything to blank...
  */

  if (scheme && schemelen > 0)
    *scheme = '\0';

  if (username && usernamelen > 0)
    *username = '\0';

  if (host && hostlen > 0)
    *host = '\0';

  if (port)
    *port = 0;

  if (resource && resourcelen > 0)
    *resource = '\0';

 /*
  * Range check input...
  */

  if (!uri || !port || !scheme || schemelen <= 0 || !username ||
      usernamelen <= 0 || !host || hostlen <= 0 || !resource ||
      resourcelen <= 0)
    return (HTTP_URI_BAD_ARGUMENTS);

  if (!*uri)
    return (HTTP_URI_BAD_URI);

 /*
  * Grab the scheme portion of the URI...
  */

  status = HTTP_URI_OK;

  if (!strncmp(uri, "//", 2))
  {
   /*
    * Workaround for HP IPP client bug...
    */

    strlcpy(scheme, "ipp", schemelen);
    status = HTTP_URI_MISSING_SCHEME;
  }
  else if (*uri == '/')
  {
   /*
    * Filename...
    */

    strlcpy(scheme, "file", schemelen);
    status = HTTP_URI_MISSING_SCHEME;
  }
  else
  {
   /*
    * Standard URI with scheme...
    */

    for (ptr = scheme, end = scheme + schemelen - 1;
         *uri && *uri != ':' && ptr < end;)
      if (isalnum(*uri & 255) || *uri == '-' || *uri == '+' || *uri == '.')
        *ptr++ = *uri++;
      else
        break;

    *ptr = '\0';

    if (*uri != ':')
    {
      *scheme = '\0';
      return (HTTP_URI_BAD_SCHEME);
    }

    uri ++;
  }

 /*
  * Set the default port number...
  */

  if (!strcmp(scheme, "http"))
    *port = 80;
  else if (!strcmp(scheme, "https"))
    *port = 443;
  else if (!strcmp(scheme, "ipp"))
    *port = 631;
  else if (!strcasecmp(scheme, "lpd"))
    *port = 515;
  else if (!strcmp(scheme, "socket"))	/* Not yet registered with IANA... */
    *port = 9100;
  else if (strcmp(scheme, "file") && strcmp(scheme, "mailto"))
    status = HTTP_URI_UNKNOWN_SCHEME;

 /*
  * Now see if we have a hostname...
  */

  if (!strncmp(uri, "//", 2))
  {
   /*
    * Yes, extract it...
    */

    uri += 2;

   /*
    * Grab the username, if any...
    */

    if ((sep = strpbrk(uri, "@/")) != NULL && *sep == '@')
    {
     /*
      * Get a username:password combo...
      */

      uri = http_copy_decode(username, uri, usernamelen, "@",
                             decoding & HTTP_URI_CODING_USERNAME);

      if (!uri)
      {
        *username = '\0';
        return (HTTP_URI_BAD_USERNAME);
      }

      uri ++;
    }

   /*
    * Then the hostname/IP address...
    */

    if (*uri == '[')
    {
     /*
      * Grab IPv6 address...
      */

      uri ++;
      if (!strncmp(uri, "v1.", 3))
        uri += 3;			/* Skip IPvN leader... */

      uri = http_copy_decode(host, uri, hostlen, "]",
                             decoding & HTTP_URI_CODING_HOSTNAME);

      if (!uri)
      {
        *host = '\0';
        return (HTTP_URI_BAD_HOSTNAME);
      }

     /*
      * Validate value...
      */

      if (*uri != ']')
      {
        *host = '\0';
        return (HTTP_URI_BAD_HOSTNAME);
      }

      uri ++;

      for (ptr = host; *ptr; ptr ++)
        if (*ptr == '+')
	{
	 /*
	  * Convert zone separator to % and stop here...
	  */

	  *ptr = '%';
	  break;
	}
	else if (*ptr != ':' && *ptr != '.' && !isxdigit(*ptr & 255))
	{
	  *host = '\0';
	  return (HTTP_URI_BAD_HOSTNAME);
	}
    }
    else
    {
     /*
      * Validate the hostname or IPv4 address first...
      */

      for (ptr = (char *)uri; *ptr; ptr ++)
        if (strchr(":?/", *ptr))
	  break;
        else if (!strchr("abcdefghijklmnopqrstuvwxyz"
			 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			 "0123456789"
	        	 "-._~"
			 "%"
			 "!$&'()*+,;=\\", *ptr))
	{
	  *host = '\0';
	  return (HTTP_URI_BAD_HOSTNAME);
	}

     /*
      * Then copy the hostname or IPv4 address to the buffer...
      */

      uri = http_copy_decode(host, uri, hostlen, ":?/",
                             decoding & HTTP_URI_CODING_HOSTNAME);

      if (!uri)
      {
        *host = '\0';
        return (HTTP_URI_BAD_HOSTNAME);
      }
    }

   /*
    * Validate hostname for file scheme - only empty and localhost are
    * acceptable.
    */

    if (!strcmp(scheme, "file") && strcmp(host, "localhost") && host[0])
    {
      *host = '\0';
      return (HTTP_URI_BAD_HOSTNAME);
    }

   /*
    * See if we have a port number...
    */

    if (*uri == ':')
    {
     /*
      * Yes, collect the port number...
      */

      *port = strtol(uri + 1, (char **)&uri, 10);

      if (*uri != '/' && *uri)
      {
        *port = 0;
        return (HTTP_URI_BAD_PORT);
      }
    }
  }

 /*
  * The remaining portion is the resource string...
  */

  if (*uri == '?' || !*uri)
  {
   /*
    * Hostname but no path...
    */

    status    = HTTP_URI_MISSING_RESOURCE;
    *resource = '/';

   /*
    * Copy any query string...
    */

    if (*uri == '?')
      uri = http_copy_decode(resource + 1, uri, resourcelen - 1, NULL,
                             decoding & HTTP_URI_CODING_QUERY);
    else
      resource[1] = '\0';
  }
  else
  {
    uri = http_copy_decode(resource, uri, resourcelen, "?",
                           decoding & HTTP_URI_CODING_RESOURCE);

    if (uri && *uri == '?')
    {
     /*
      * Concatenate any query string...
      */

      char *resptr = resource + strlen(resource);

      uri = http_copy_decode(resptr, uri, resourcelen - (int)(resptr - resource),
                             NULL, decoding & HTTP_URI_CODING_QUERY);
    }
  }

  if (!uri)
  {
    *resource = '\0';
    return (HTTP_URI_BAD_RESOURCE);
  }

 /*
  * Return the URI separation status...
  */

  return (status);
}


/*
 * 'httpStatus()' - Return a short string describing a HTTP status code.
 *
 * The returned string is localized to the current POSIX locale and is based
 * on the status strings defined in RFC 2616.
 */

const char *				/* O - Localized status string */
httpStatus(http_status_t status)	/* I - HTTP status code */
{
  const char	*s;			/* Status string */
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */


  if (!cg->lang_default)
    cg->lang_default = cupsLangDefault();

  switch (status)
  {
    case HTTP_CONTINUE :
        s = _("Continue");
	break;
    case HTTP_SWITCHING_PROTOCOLS :
        s = _("Switching Protocols");
	break;
    case HTTP_OK :
        s = _("OK");
	break;
    case HTTP_CREATED :
        s = _("Created");
	break;
    case HTTP_ACCEPTED :
        s = _("Accepted");
	break;
    case HTTP_NO_CONTENT :
        s = _("No Content");
	break;
    case HTTP_MOVED_PERMANENTLY :
        s = _("Moved Permanently");
	break;
    case HTTP_SEE_OTHER :
        s = _("See Other");
	break;
    case HTTP_NOT_MODIFIED :
        s = _("Not Modified");
	break;
    case HTTP_BAD_REQUEST :
        s = _("Bad Request");
	break;
    case HTTP_UNAUTHORIZED :
        s = _("Unauthorized");
	break;
    case HTTP_FORBIDDEN :
        s = _("Forbidden");
	break;
    case HTTP_NOT_FOUND :
        s = _("Not Found");
	break;
    case HTTP_REQUEST_TOO_LARGE :
        s = _("Request Entity Too Large");
	break;
    case HTTP_URI_TOO_LONG :
        s = _("URI Too Long");
	break;
    case HTTP_UPGRADE_REQUIRED :
        s = _("Upgrade Required");
	break;
    case HTTP_NOT_IMPLEMENTED :
        s = _("Not Implemented");
	break;
    case HTTP_NOT_SUPPORTED :
        s = _("Not Supported");
	break;
    case HTTP_EXPECTATION_FAILED :
        s = _("Expectation Failed");
	break;
    case HTTP_SERVICE_UNAVAILABLE :
        s = _("Service Unavailable");
	break;

    default :
        s = _("Unknown");
	break;
  }

  return (_cupsLangString(cg->lang_default, s));
}


#ifndef HAVE_HSTRERROR
/*
 * '_cups_hstrerror()' - hstrerror() emulation function for Solaris and others...
 */

const char *				/* O - Error string */
_cups_hstrerror(int error)		/* I - Error number */
{
  static const char * const errors[] =	/* Error strings */
		{
		  "OK",
		  "Host not found.",
		  "Try again.",
		  "Unrecoverable lookup error.",
		  "No data associated with name."
		};


  if (error < 0 || error > 4)
    return ("Unknown hostname lookup error.");
  else
    return (errors[error]);
}
#endif /* !HAVE_HSTRERROR */


/*
 * '_httpEncodeURI()' - Percent-encode a HTTP request URI.
 */

char *					/* O - Encoded URI */
_httpEncodeURI(char       *dst,		/* I - Destination buffer */
               const char *src,		/* I - Source URI */
	       size_t     dstsize)	/* I - Size of destination buffer */
{
  http_copy_encode(dst, src, dst + dstsize - 1, NULL, NULL, 1);
  return (dst);
}


/*
 * '_httpResolveURI()' - Resolve a DNS-SD URI.
 */

const char *				/* O - Resolved URI */
_httpResolveURI(
    const char *uri,			/* I - DNS-SD URI */
    char       *resolved_uri,		/* I - Buffer for resolved URI */
    size_t     resolved_size,		/* I - Size of URI buffer */
    int        log)			/* I - Log progress to stderr? */
{
  char			scheme[32],	/* URI components... */
			userpass[256],
			hostname[1024],
			resource[1024];
  int			port;
  http_uri_status_t	status;		/* URI decode status */


  DEBUG_printf(("_httpResolveURI(uri=\"%s\", resolved_uri=%p, "
                "resolved_size=" CUPS_LLFMT ")\n", uri, resolved_uri,
		CUPS_LLCAST resolved_size));

 /*
  * Get the device URI...
  */

  if ((status = httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme,
                                sizeof(scheme), userpass, sizeof(userpass),
				hostname, sizeof(hostname), &port, resource,
				sizeof(resource))) < HTTP_URI_OK)
  {
    if (log)
      _cupsLangPrintf(stderr, _("Bad device URI \"%s\"!\n"), uri);

    DEBUG_printf(("_httpResolveURI: httpSeparateURI returned %d!\n", status));
    DEBUG_puts("_httpResolveURI: Returning NULL");
    return (NULL);
  }

 /*
  * Resolve it as needed...
  */

  if (strstr(hostname, "._tcp"))
  {
#ifdef HAVE_DNSSD
    DNSServiceRef	ref;		/* DNS-SD service reference */
    char		*regtype,	/* Pointer to type in hostname */
			*domain;	/* Pointer to domain in hostname */
    _http_uribuf_t	uribuf;		/* URI buffer */

   /*
    * Separate the hostname into service name, registration type, and domain...
    */

    for (regtype = strstr(hostname, "._tcp") - 2;
         regtype > hostname;
	 regtype --)
      if (regtype[0] == '.' && regtype[1] == '_')
      {
       /*
        * Found ._servicetype in front of ._tcp...
	*/

        *regtype++ = '\0';
	break;
      }

    if (regtype <= hostname)
    {
      DEBUG_puts("_httpResolveURI: Bad hostname, returning NULL");
      return (NULL);
    }

    domain = regtype + strlen(regtype) - 1;
    if (domain > regtype && *domain == '.')
      *domain = '\0';

    for (domain = strchr(regtype, '.');
         domain;
	 domain = strchr(domain + 1, '.'))
      if (domain[1] != '_')
        break;

    if (domain)
      *domain++ = '\0';

    uribuf.buffer  = resolved_uri;
    uribuf.bufsize = resolved_size;

    resolved_uri[0] = '\0';

    DEBUG_printf(("_httpResolveURI: Resolving hostname=\"%s\", regtype=\"%s\", "
                  "domain=\"%s\"\n", hostname, regtype, domain));
    if (log)
    {
      fputs("STATE: +connecting-to-device\n", stderr);
      _cupsLangPrintf(stderr, _("INFO: Looking for \"%s\"...\n"), hostname);
    }

    if (DNSServiceResolve(&ref, 0, 0, hostname, regtype, domain,
			  resolve_callback,
			  &uribuf) == kDNSServiceErr_NoError)
    {
      if (DNSServiceProcessResult(ref) != kDNSServiceErr_NoError &&
          resolved_uri[0])
        uri = NULL;
      else
        uri = resolved_uri;

      DNSServiceRefDeallocate(ref);
    }
    else
      uri = NULL;

    if (log)
      fputs("STATE: -connecting-to-device\n", stderr);

#else
   /*
    * No DNS-SD support...
    */

    uri = NULL;
#endif /* HAVE_DNSSD */

    if (log && !uri)
      _cupsLangPuts(stderr, _("Unable to find printer!\n"));
  }

  DEBUG_printf(("_httpResolveURI: Returning \"%s\"\n", uri));

  return (uri);
}


/*
 * 'http_copy_decode()' - Copy and decode a URI.
 */

static const char *			/* O - New source pointer or NULL on error */
http_copy_decode(char       *dst,	/* O - Destination buffer */ 
                 const char *src,	/* I - Source pointer */
		 int        dstsize,	/* I - Destination size */
		 const char *term,	/* I - Terminating characters */
		 int        decode)	/* I - Decode %-encoded values */
{
  char	*ptr,				/* Pointer into buffer */
	*end;				/* End of buffer */
  int	quoted;				/* Quoted character */


 /*
  * Copy the src to the destination until we hit a terminating character
  * or the end of the string.
  */

  for (ptr = dst, end = dst + dstsize - 1;
       *src && (!term || !strchr(term, *src));
       src ++)
    if (ptr < end)
    {
      if (*src == '%' && decode)
      {
        if (isxdigit(src[1] & 255) && isxdigit(src[2] & 255))
	{
	 /*
	  * Grab a hex-encoded character...
	  */

          src ++;
	  if (isalpha(*src))
	    quoted = (tolower(*src) - 'a' + 10) << 4;
	  else
	    quoted = (*src - '0') << 4;

          src ++;
	  if (isalpha(*src))
	    quoted |= tolower(*src) - 'a' + 10;
	  else
	    quoted |= *src - '0';

          *ptr++ = quoted;
	}
	else
	{
	 /*
	  * Bad hex-encoded character...
	  */

	  *ptr = '\0';
	  return (NULL);
	}
      }
      else
	*ptr++ = *src;
    }

  *ptr = '\0';

  return (src);
}


/*
 * 'http_copy_encode()' - Copy and encode a URI.
 */

static char *				/* O - End of current URI */
http_copy_encode(char       *dst,	/* O - Destination buffer */ 
                 const char *src,	/* I - Source pointer */
		 char       *dstend,	/* I - End of destination buffer */
                 const char *reserved,	/* I - Extra reserved characters */
		 const char *term,	/* I - Terminating characters */
		 int        encode)	/* I - %-encode reserved chars? */
{
  static const char hex[] = "0123456789ABCDEF";


  while (*src && dst < dstend)
  {
    if (term && *src == *term)
      return (dst);

    if (encode && (*src == '%' || *src <= ' ' || *src & 128 ||
                   (reserved && strchr(reserved, *src))))
    {
     /*
      * Hex encode reserved characters...
      */

      if ((dst + 2) >= dstend)
        break;

      *dst++ = '%';
      *dst++ = hex[(*src >> 4) & 15];
      *dst++ = hex[*src & 15];

      src ++;
    }
    else
      *dst++ = *src++;
  }

  *dst = '\0';

  if (*src)
    return (NULL);
  else
    return (dst);
}


#ifdef HAVE_DNSSD
/*
 * 'resolve_callback()' - Build a device URI for the given service name.
 */

static void
resolve_callback(
    DNSServiceRef       sdRef,		/* I - Service reference */
    DNSServiceFlags     flags,		/* I - Results flags */
    uint32_t            interfaceIndex,	/* I - Interface number */
    DNSServiceErrorType errorCode,	/* I - Error, if any */
    const char          *fullName,	/* I - Full service name */
    const char          *hostTarget,	/* I - Hostname */
    uint16_t            port,		/* I - Port number */
    uint16_t            txtLen,		/* I - Length of TXT record */
    const unsigned char *txtRecord,	/* I - TXT record data */
    void                *context)	/* I - Pointer to URI buffer */
{
  const char		*scheme;	/* URI scheme */
  char			rp[257];	/* Remote printer */
  const void		*value;		/* Value from TXT record */
  uint8_t		valueLen;	/* Length of value */
  _http_uribuf_t	*uribuf;	/* URI buffer */


  DEBUG_printf(("resolve_callback(sdRef=%p, flags=%x, interfaceIndex=%u, "
	        "errorCode=%d, fullName=\"%s\", hostTarget=\"%s\", port=%u, "
	        "txtLen=%u, txtRecord=%p, context=%p)\n", sdRef, flags,
	        interfaceIndex, errorCode, fullName, hostTarget, port, txtLen,
	        txtRecord, context));

 /*
  * Figure out the scheme from the full name...
  */

  if (strstr(fullName, "._ipp"))
    scheme = "ipp";
  else if (strstr(fullName, "._printer."))
    scheme = "lpd";
  else if (strstr(fullName, "._pdl-datastream."))
    scheme = "socket";
  else
    scheme = "riousbprint";

 /*
  * Extract the "remote printer" key from the TXT record...
  */

  if ((value = TXTRecordGetValuePtr(txtLen, txtRecord, "rp",
                                    &valueLen)) != NULL)
  {
   /*
    * Convert to resource by concatenating with a leading "/"...
    */

    rp[0] = '/';
    memcpy(rp + 1, value, valueLen);
    rp[valueLen + 1] = '\0';
  }
  else
    rp[0] = '\0';

 /*
  * Assemble the final device URI...
  */

  uribuf = (_http_uribuf_t *)context;

  httpAssembleURI(HTTP_URI_CODING_ALL, uribuf->buffer, uribuf->bufsize, scheme,
                  NULL, hostTarget, ntohs(port), rp);

  DEBUG_printf(("resolve_callback: Resolved URI is \"%s\"...\n",
                uribuf->buffer));
}
#endif /* HAVE_DNSSD */


/*
 * End of "$Id$".
 */
