/*
 * "$Id: html.c,v 1.2 1997/05/08 20:14:19 mike Exp $"
 *
 *   CGI HTML functions.
 *
 *   Copyright 1997 by Easy Software Products, All Rights Reserved.
 *
 * Contents:
 *
 *   cgiStartHTML() - Start an HTML document stream.
 *   cgiEndHTML()   - End an HTML document stream.
 *
 * Revision History:
 *
 *   $Log: html.c,v $
 *   Revision 1.2  1997/05/08 20:14:19  mike
 *   Renamed CGI_Name functions to cgiName functions.
 *   Updated documentation.
 *
 *   Revision 1.1  1997/05/08  19:55:53  mike
 *   Initial revision
 */

#include "cgi.h"
#include <stdarg.h>


/*
 * 'cgiStartHTML()' - Start an HTML document stream.
 */

void
cgiStartHTML(FILE *out,		/* I - Output file to use */
             char *title,	/* I - Title for page */
             ...)		/* I - Any addition args for title */
{
  va_list	ap;		/* Argument pointer */


  fputs("Content-type: text/html\n\n", out);
  fputs("<HTML>\n", out);
  fputs("<HEAD><TITLE>\n", out);

  va_start(ap, title);
  vfprintf(out, title, ap);
  va_end(ap);

  fputs("</TITLE></HEAD>\n", out);
  fputs("<BODY>", out);
}


/*
 * 'cgiEndHTML()' - End an HTML document stream.
 */

void
cgiEndHTML(FILE *out)	/* I - Output file to use */
{
  fputs("</BODY>", out);
  fputs("</HTML>\n", out);
}


/*
 * End of "$Id: html.c,v 1.2 1997/05/08 20:14:19 mike Exp $".
 */
