/*
 * "$Id: html.c,v 1.3 1999/09/10 13:38:32 mike Exp $"
 *
 *   CGI HTML functions.
 *
 *   Copyright 1997-1999 by Easy Software Products, All Rights Reserved.
 *
 * Contents:
 *
 *   cgiStartHTML() - Start an HTML document stream.
 *   cgiEndHTML()   - End an HTML document stream.
 */

#include "cgi.h"
#include <stdarg.h>


/*
 * 'cgiStartHTML()' - Start an HTML document stream.
 */

void
cgiStartHTML(FILE       *out,		/* I - Output file to use */
             const char *stylesheet,	/* I - Stylesheet to use */
	     const char *author,	/* I - Author name */
	     const char *keywords,	/* I - Search keywords */
	     const char *description,	/* I - Description of document */
             const char *title,		/* I - Title for page */
             ...)			/* I - Any addition args for title */
{
  va_list	ap;			/* Argument pointer */


  fputs("Content-type: text/html\n\n", out);
  fputs("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\" "
        "\"http://www.w3.org/TR/REC-html40/loose.dtd\">\n", out);
  fputs("<HTML>\n", out);
  fputs("<HEAD>\n", out);

  fputs("\t<TITLE>\n", out);
    va_start(ap, title);
    vfprintf(out, title, ap);
    va_end(ap);
  fputs("</TITLE>\n", out);

  if (stylesheet)
    fprintf(out, "\t<LINK REL=\"STYLESHEET\" TYPE=\"text/css\" HREF=\"%s\">\n",
            stylesheet);
  if (author)
    fprintf(out, "\t<META NAME=\"AUTHOR\" CONTENT=\"%s\">\n", author);
  if (keywords)
    fprintf(out, "\t<META NAME=\"KEYWORDS\" CONTENT=\"%s\">\n", author);
  if (description)
    fprintf(out, "\t<META NAME=\"DESCRIPTION\" CONTENT=\"%s\">\n", author);

  fputs("</HEAD>\n", out);
  fputs("<BODY>\n", out);
}


/*
 * 'cgiEndHTML()' - End an HTML document stream.
 */

void
cgiEndHTML(FILE *out)	/* I - Output file to use */
{
  fputs("</BODY>\n", out);
  fputs("</HTML>\n", out);
}


/*
 * End of "$Id: html.c,v 1.3 1999/09/10 13:38:32 mike Exp $".
 */
