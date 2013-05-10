/*
 * "$Id$"
 *
 *   HTML support functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cgiEndHTML()        - End a HTML page.
 *   cgiEndMultipart()   - End the delivery of a multipart web page.
 *   cgiFormEncode()     - Encode a string as a form variable...
 *   cgiStartHTML()      - Start a HTML page.
 *   cgiStartMultipart() - Start a multipart delivery of a web page...
 *   cgi_null_passwd()   - Return a NULL password for authentication.
 */

/*
 * Include necessary headers...
 */

#include "cgi-private.h"


/*
 * Local globals...
 */

static const char	*cgi_multipart = NULL;
					/* Multipart separator, if any */


/*
 * Local functions...
 */

static const char	*cgi_null_passwd(const char *prompt);


/*
 * 'cgiEndHTML()' - End a HTML page.
 */

void
cgiEndHTML(void)
{
 /*
  * Send the standard trailer...
  */

  cgiCopyTemplateLang("trailer.tmpl");
}


/*
 * 'cgiEndMultipart()' - End the delivery of a multipart web page.
 */

void
cgiEndMultipart(void)
{
  if (cgi_multipart)
    printf("\n%s--\n", cgi_multipart);
}


/*
 * 'cgiFormEncode()' - Encode a string as a form variable...
 */

char *					/* O - Destination string */
cgiFormEncode(char       *dst,		/* I - Destination string */
              const char *src,		/* I - Source string */
	      size_t     dstsize)	/* I - Size of destination string */
{
  char			*dstptr,	/* Pointer into destination */
			*dstend;	/* End of destination */
  static const char	*hex =		/* Hexadecimal characters */
			"0123456789ABCDEF";


 /*
  * Mark the end of the string...
  */

  dstend = dst + dstsize - 1;

 /*
  * Loop through the source string and copy...
  */

  for (dstptr = dst; *src && dstptr < dstend;)
  {
    switch (*src)
    { 
      case ' ' :
         /*
	  * Encode spaces with a "+"...
	  */

          *dstptr++ = '+';
	  src ++;
	  break;

      case '&' :
      case '%' :
      case '+' :
         /*
	  * Encode special characters with %XX escape...
	  */

          if (dstptr < (dstend - 2))
	  {
	    *dstptr++ = '%';
	    *dstptr++ = hex[(*src & 255) >> 4];
	    *dstptr++ = hex[*src & 15];
	    src ++;
	  }
          break;

      default :
         /*
	  * Copy other characters literally...
	  */

          *dstptr++ = *src++;
	  break;
    }
  }

 /*
  * Nul-terminate the destination string...
  */

  *dstptr = '\0';

 /*
  * Return the encoded string...
  */

  return (dst);
}


/*
 * 'cgiStartHTML()' - Start a HTML page.
 */

void
cgiStartHTML(const char *title)		/* I - Title of page */
{
 /*
  * Disable any further authentication attempts...
  */

  cupsSetPasswordCB(cgi_null_passwd);

 /*
  * Tell the client to expect UTF-8 encoded HTML...
  */

  if (cgi_multipart)
    puts(cgi_multipart);

  puts("Content-Type: text/html;charset=utf-8\n");

 /*
  * Send a standard header...
  */

  cgiSetVariable("TITLE", title);
  cgiSetServerVersion();

  cgiCopyTemplateLang("header.tmpl");
}


/*
 * 'cgiStartMultipart()' - Start a multipart delivery of a web page...
 */

void
cgiStartMultipart(void)
{
  puts("MIME-Version: 1.0");
  puts("Content-Type: multipart/x-mixed-replace; boundary=\"CUPS-MULTIPART\"\n");
  cgi_multipart = "--CUPS-MULTIPART";
}


/*
 * 'cgi_null_passwd()' - Return a NULL password for authentication.
 */

static const char *			/* O - NULL */
cgi_null_passwd(const char *prompt)	/* I - Prompt string (unused) */
{
  (void)prompt;

  fprintf(stderr, "DEBUG: cgi_null_passwd(prompt=\"%s\") called!\n",
          prompt ? prompt : "(null)");

  return (NULL);
}


/*
 * End of "$Id$".
 */
