/*
 * "$Id: html.c,v 1.6 2001/01/22 15:03:22 mike Exp $"
 *
 *   CGI HTML functions.
 *
 *   Copyright 1997-2001 by Easy Software Products.
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License as published by the Free
 *   Software Foundation; either version 2 of the License, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *   for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
    fprintf(out, "\t<META NAME=\"KEYWORDS\" CONTENT=\"%s\">\n", keywords);
  if (description)
    fprintf(out, "\t<META NAME=\"DESCRIPTION\" CONTENT=\"%s\">\n", description);

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
 * End of "$Id: html.c,v 1.6 2001/01/22 15:03:22 mike Exp $".
 */
