/*
 * "$Id: translate.c,v 1.1.6.1 2004/06/29 13:15:09 mike Exp $"
 *
 *   HTTP-based translation program for the Common UNIX Printing System (CUPS).
 *
 *   This program uses AltaVista's "babelfish" page to translate the POSIX
 *   message file (C/cups_C) to several different languages.  The translation
 *   isn't perfect, but it's a good start (better than working from scratch.)
 *
 *   Copyright 1997-1999 by Easy Software Products.
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3142 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main() - Main entry.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <cups/http.h>


/*
 * 'main()' - Main entry.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  http_t	*http;		/* HTTP connection */
  http_status_t	status;		/* Status of GET command */
  char		line[1024],	/* Line from file */
		*lineptr,	/* Pointer into line */
		buffer[2048],	/* Input/output buffer */
		*bufptr,	/* Pointer into buffer */
		length[16];	/* Content length */
  int		bytes;		/* Number of bytes read */
  FILE		*in,		/* Input file */
		*out;		/* Output file */


  if (argc != 3)
  {
    fputs("Usage: translate outfile language\n", stderr);
    return (1);
  }

  if ((in = fopen("C/cups_C", "r")) == NULL)
  {
    perror("translate: Unable to open input file");
    return (1);
  }

  if ((out = fopen(argv[1], "w")) == NULL)
  {
    perror("translate: Unable to create output file");
    fclose(in);
    return (1);
  }

 /*
  * Do character set...
  */

  fgets(line, sizeof(line), in);
  fputs("iso-8859-1\n", out);	/* Right now that's all that Babelfish does */

 /*
  * Then strings...
  */

  while (fgets(line, sizeof(line), in) != NULL)
  {
   /*
    * Strip trailing newline if necessary...
    */

    lineptr = line + strlen(line) - 1;
    if (*lineptr == '\n')
      *lineptr = '\0';

   /*
    * Skip leading numbers and whitespace...
    */

    lineptr = line;
    while (isdigit(*lineptr))
      putc(*lineptr++, out);

    while (isspace(*lineptr))
      putc(*lineptr++, out);

    if (*lineptr == '\0')
    {
      putc('\n', out);
      continue;
    }

   /*
    * Encode the line into the buffer...
    */

    sprintf(buffer, "doit=done&lp=en_%s&urltext=[", argv[2]);
    bufptr = buffer + strlen(buffer);

    while (*lineptr)
    {
      if (*lineptr == ' ')
        *bufptr++ = '+';
      else if (*lineptr < ' ' || *lineptr == '%')
      {
        sprintf(bufptr, "%%%02X", *lineptr & 255);
	bufptr += 3;
      }
      else
        *bufptr++ = *lineptr;

      lineptr ++;
    }

    *bufptr++ = '&';
    *bufptr = '\0';

    sprintf(length, "%d", bufptr - buffer);

   /*
    * Send the request...
    */

    if ((http = httpConnect("dns.easysw.com", 80)) == NULL)
    {
      perror("translate: Unable to contact proxy server");
      fclose(in);
      fclose(out);
      return (1);
    }

    lineptr = line;
    while (isdigit(*lineptr))
      lineptr ++;
    while (isspace(*lineptr))
      lineptr ++;

    printf("%s = ", lineptr);
    fflush(stdout);

    http->version = HTTP_1_0;
    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_CONTENT_TYPE,
                 "application/x-www-form-urlencoded");
    httpSetField(http, HTTP_FIELD_CONTENT_LENGTH, length);
    if (httpPost(http, "http://babelfish.altavista.digital.com/cgi-bin/translate?"))
      httpPost(http, "http://babelfish.altavista.digital.com/cgi-bin/translate?");

    httpWrite(http, buffer, bufptr - buffer);

    while ((status = httpUpdate(http)) == HTTP_CONTINUE);

    if (status == HTTP_OK)
    {
      int sawparen = 0;
      int skipws = 1;
      int sawbracket = 0;

      while ((bytes = httpRead(http, buffer, sizeof(buffer))) > 0)
      {
        buffer[bytes] = '\0';

        for (bufptr = buffer; *bufptr; bufptr ++)
	{
	  if (*bufptr == '>')
	    sawbracket = 0;
	  else if (*bufptr == '<')
	  {
	    sawbracket = 1;
	    if (sawparen)
	      break;
	  }
	  else if (*bufptr == '[' && !sawbracket)
	    sawparen = 1;
	  else if (sawparen)
	  {
	    if (skipws)
	    {
	      if (!isspace(*bufptr))
              {
	        skipws = 0;
		*bufptr = toupper(*bufptr);
	      }
	    }

            if (!skipws)
	    {
              if (*bufptr == '\n')
	      {
		putc(' ', out);
		putchar(' ');
	      }
	      else
	      {
        	putc(*bufptr, out);
        	putchar(*bufptr);
              }
            }
	  }
        }

        if (sawparen && sawbracket)
	  break;
      }

      httpFlush(http);
      putc('\n', out);
      putchar('\n');
    }
    else
    {
      printf("HTTP error %d\n", status);

      fprintf(out, "%s\n", lineptr);
      httpFlush(http);
    }

    httpClose(http);
  }

  fclose(in);
  fclose(out);

  return (0);
}


/*
 * End of "$Id: translate.c,v 1.1.6.1 2004/06/29 13:15:09 mike Exp $".
 */
