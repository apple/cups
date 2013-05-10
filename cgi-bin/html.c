/*
 * "$Id$"
 *
 *   HTML support functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products.
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
 * Contents:
 *
 *   cgiEndHTML()      - End a HTML page.
 *   cgiFormEncode()   - Encode a string as a form variable...
 *   cgiStartHTML()    - Start a HTML page.
 *   cgi_null_passwd() - Return a NULL password for authentication.
 */

/*
 * Include necessary headers...
 */

#include "cgi-private.h"


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

  puts("Content-Type: text/html;charset=utf-8\n");

 /*
  * Send a standard header...
  */

  cgiSetVariable("TITLE", title);
  cgiSetServerVersion();

  cgiCopyTemplateLang("header.tmpl");
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
