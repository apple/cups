/*
 * "$Id$"
 *
 *   HTTP support routines for the Common UNIX Printing System (CUPS) scheduler.
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
 *   httpSeparate3()      - Separate a Universal Resource Identifier into its
 *                          components.
 *   httpStatus()         - Return a short string describing a HTTP status code.
 *   _cups_hstrerror()    - hstrerror() emulation function for Solaris and
 *                          others...
 *   http_copy_decode()   - Copy and decode a URI.
 */

/*
 * Include necessary headers...
 */

#include "debug.h"
#include "globals.h"
#include <stdlib.h>


/*
 * Local globals...
 */

static const char * const http_days[7] =
			{
			  "Sun",			  "Mon",
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
			                  int dstsize, const char *term);


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
 */

void
httpSeparate(const char *uri,		/* I - Universal Resource Identifier */
             char       *scheme,	/* O - Scheme [32] (http, https, etc.) */
	     char       *username,	/* O - Username [1024] */
	     char       *host,		/* O - Hostname [1024] */
	     int        *port,		/* O - Port number to use */
             char       *resource)	/* O - Resource/filename [1024] */
{
  httpSeparate3(uri, scheme, 32, username, HTTP_MAX_URI, host, HTTP_MAX_URI,
                port, resource, HTTP_MAX_URI);
}


/*
 * 'httpSeparate2()' - Separate a Universal Resource Identifier into its
 *                     components.
 *
 * @since CUPS 1.1.21@
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
  httpSeparate3(uri, scheme, schemelen, username, usernamelen, host, hostlen,
                port, resource, resourcelen);
}


/*
 * 'httpSeparate3()' - Separate a Universal Resource Identifier into its
 *                     components.
 *
 * @since CUPS 1.2@
 */

http_uri_status_t			/* O - Result of separation */
httpSeparate3(const char *uri,		/* I - Universal Resource Identifier */
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

      uri = http_copy_decode(username, uri, usernamelen, "@");

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

      *host = '[';
      uri   = http_copy_decode(host + 1, uri + 1, hostlen - 1, "]");

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

      for (ptr = host + 1; *ptr; ptr ++)
        if (*ptr != ':' && *ptr != '.' && !isxdigit(*ptr & 255))
	{
	  *host = '\0';
	  return (HTTP_URI_BAD_HOSTNAME);
	}

     /*
      * Add the trailing "]"...
      */

      strlcat(host, "]", hostlen);

      uri ++;
    }
    else
    {
     /*
      * Grab hostname or IPv4 address...
      */

      uri = http_copy_decode(host, uri, hostlen, ":?/");

      if (!uri)
      {
        *host = '\0';
        return (HTTP_URI_BAD_HOSTNAME);
      }

     /*
      * Validate value...
      */

      for (ptr = host; *ptr; ptr ++)
        if (!strchr("abcdefghijklmnopqrstuvwxyz"
		    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		    "0123456789"
	            "-._~"
		    "!$&'()*+,;=", *ptr))
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

      if (*uri != '/')
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
    uri       = http_copy_decode(resource + 1, uri, resourcelen - 1, "");
  }
  else
    uri = http_copy_decode(resource, uri, resourcelen, "");

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
 */

const char *				/* O - String or NULL */
httpStatus(http_status_t status)	/* I - HTTP status code */
{
  switch (status)
  {
    case HTTP_CONTINUE :
        return ("Continue");
    case HTTP_SWITCHING_PROTOCOLS :
        return ("Switching Protocols");
    case HTTP_OK :
        return ("OK");
    case HTTP_CREATED :
        return ("Created");
    case HTTP_ACCEPTED :
        return ("Accepted");
    case HTTP_NO_CONTENT :
        return ("No Content");
    case HTTP_NOT_MODIFIED :
        return ("Not Modified");
    case HTTP_BAD_REQUEST :
        return ("Bad Request");
    case HTTP_UNAUTHORIZED :
        return ("Unauthorized");
    case HTTP_FORBIDDEN :
        return ("Forbidden");
    case HTTP_NOT_FOUND :
        return ("Not Found");
    case HTTP_REQUEST_TOO_LARGE :
        return ("Request Entity Too Large");
    case HTTP_URI_TOO_LONG :
        return ("URI Too Long");
    case HTTP_UPGRADE_REQUIRED :
        return ("Upgrade Required");
    case HTTP_NOT_IMPLEMENTED :
        return ("Not Implemented");
    case HTTP_NOT_SUPPORTED :
        return ("Not Supported");
    default :
        return ("Unknown");
  }
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
 * 'http_copy_decode()' - Copy and decode a URI.
 */

static const char *			/* O - New source pointer or NULL on error */
http_copy_decode(char       *dst,	/* O - Destination buffer */ 
                 const char *src,	/* I - Source pointer */
		 int        dstsize,	/* I - Destination size */
		 const char *term)	/* I - Terminating characters */
{
  char	*ptr,				/* Pointer into buffer */
	*end;				/* End of buffer */
  int	quoted;				/* Quoted character */


 /*
  * Copy the src to the destination until we hit a terminating character
  * or the end of the string.
  */

  for (ptr = dst, end = dst + dstsize - 1; *src && !strchr(term, *src); src ++)
    if (ptr < end)
    {
      if (*src == '%')
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
 * End of "$Id$".
 */
