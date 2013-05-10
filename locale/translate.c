/*
 * "$Id$"
 *
 *   HTTP-based translation program for the Common UNIX Printing System (CUPS).
 *
 *   This program uses Google to translate the CUPS template (cups.pot) to
 *   several different languages.  The translation isn't perfect, but it's
 *   a start (better than working from scratch.)
 *
 *   Copyright 2007 by Apple Inc.
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
 *   main()               - Main entry.
 *   save_messages()      - Save messages to a .po file.
 *   translate_messages() - Translate messages using Google.
 *   write_string()       - Write a quoted string to a file.
 */

/*
 * Include necessary headers...
 */

#include <cups/string.h>
#include <cups/file.h>
#include <cups/http.h>
#include <cups/i18n.h>
#include <stdlib.h>
#include <unistd.h>


/*
 * Local functions...
 */

int	save_messages(cups_array_t *cat, const char *filename);
int	translate_messages(cups_array_t *cat, const char *lang);
int	write_string(cups_file_t *fp, const char *s);


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  cups_array_t	*cat;			/* Message catalog */


  if (argc != 3)
  {
    fputs("Usage: translate cups_language.po language\n", stderr);
    return (1);
  }

  if (access(argv[1], 0))
    cat = _cupsMessageLoad("cups.pot");
  else
    cat = _cupsMessageLoad(argv[1]);

  if (!cat)
  {
    puts("Unable to load message catalog.");
    return (1);
  }

  if (!translate_messages(cat, argv[2]))
  {
    puts("Unable to translate message catalog.");
    return (1);
  }

  if (!save_messages(cat, argv[1]))
  {
    puts("Unable to save message catalog.");
    return (1);
  }

  return (0);
}


/*
 * 'save_messages()' - Save messages to a .po file.
 */

int					/* O - 1 on success, 0 on error */
save_messages(cups_array_t *cat,	/* I - Message catalog */
              const char   *filename)	/* I - File to save to */
{
  _cups_message_t *m;			/* Current message */
  cups_file_t	*fp;			/* File pointer */


 /*
  * Open the message catalog...
  */

  if ((fp = cupsFileOpen(filename, "w")) == NULL)
    return (0);

 /*
  * Save the messages to a file...
  */

  for (m = (_cups_message_t *)cupsArrayFirst(cat);
       m;
       m = (_cups_message_t *)cupsArrayNext(cat))
  {
    if (cupsFilePuts(fp, "msgid \"") < 0)
      break;

    if (!write_string(fp, m->id))
      break;

    if (cupsFilePuts(fp, "\"\nmsgstr \"") < 0)
      break;

    if (m->str)
    {
      if (!write_string(fp, m->str))
	break;
    }

    if (cupsFilePuts(fp, "\"\n") < 0)
      break;
  }

  cupsFileClose(fp);

  return (!m);
}


/*
 * 'translate_messages()' - Translate messages using Google.
 */

int					/* O - 1 on success, 0 on error */
translate_messages(cups_array_t *cat,	/* I - Message catalog */
                   const char *lang)	/* I - Output language... */
{
 /*
  * Google provides a simple translation/language tool for translating
  * from one language to another.  It is far from perfect, however it
  * can be used to get a basic translation done or update an existing
  * translation when no other resources are available.
  *
  * Translation requests are sent as HTTP POSTs to
  * "http://translate.google.com/translate_t" with the following form
  * variables:
  *
  *   Name      Description                         Value
  *   --------  ----------------------------------  ----------------
  *   hl        Help language?                      "en"
  *   ie        Input encoding                      "UTF8"
  *   langpair  Language pair                       "en|" + language
  *   oe        Output encoding                     "UTF8"
  *   text      Text to translate                   translation string
  */

  int		ret;			/* Return value */
  _cups_message_t *m;			/* Current message */
  int		tries;			/* Number of tries... */
  http_t	*http;			/* HTTP connection */
  http_status_t	status;			/* Status of POST request */
  char		*idptr,			/* Pointer into msgid */
		buffer[65536],		/* Input/output buffer */
		*bufptr,		/* Pointer into buffer */
		*bufend,		/* Pointer to end of buffer */
		length[16];		/* Content length */
  int		bytes;			/* Number of bytes read */


 /*
  * Connect to translate.google.com...
  */

  puts("Connecting to translate.google.com...");

  if ((http = httpConnect("translate.google.com", 80)) == NULL)
  {
    perror("Unable to connect to translate.google.com");
    return (0);
  }

 /*
  * Scan the current messages, requesting a translation of any untranslated
  * messages...
  */

  for (m = (_cups_message_t *)cupsArrayFirst(cat), ret = 1;
       m;
       m = (_cups_message_t *)cupsArrayNext(cat))
  {
   /*
    * Skip messages that are already translated...
    */

    if (m->str && m->str[0])
      continue;

   /*
    * Encode the form data into the buffer...
    */

    snprintf(buffer, sizeof(buffer),
             "hl=en&ie=UTF8&langpair=en|%s&oe=UTF8&text=", lang);
    bufptr = buffer + strlen(buffer);
    bufend = buffer + sizeof(buffer) - 5;

    for (idptr = m->id; *idptr && bufptr < bufend; idptr ++)
      if (*idptr == ' ')
        *bufptr++ = '+';
      else if (*idptr < ' ' || *idptr == '%')
      {
        sprintf(bufptr, "%%%02X", *idptr & 255);
	bufptr += 3;
      }
      else if (*idptr != '&')
        *bufptr++ = *idptr;

    *bufptr++ = '&';
    *bufptr = '\0';

    sprintf(length, "%d", (int)(bufptr - buffer));

   /*
    * Send the request...
    */

    printf("\"%s\" = ", m->id);
    fflush(stdout);

    tries = 0;

    do
    {
      httpClearFields(http);
      httpSetField(http, HTTP_FIELD_CONTENT_TYPE,
                   "application/x-www-form-urlencoded");
      httpSetField(http, HTTP_FIELD_CONTENT_LENGTH, length);

      if (httpPost(http, "/translate_t"))
      {
	httpReconnect(http);
	httpPost(http, "/translate_t");
      }

      httpWrite2(http, buffer, bufptr - buffer);

      while ((status = httpUpdate(http)) == HTTP_CONTINUE);

      if (status != HTTP_OK && status != HTTP_ERROR)
        httpFlush(http);

      tries ++;
    }
    while (status == HTTP_ERROR && tries < 10);

    if (status == HTTP_OK)
    {
     /*
      * OK, read the translation back...
      */

      bufptr = buffer;
      bufend = buffer + sizeof(buffer) - 1;

      while ((bytes = httpRead2(http, bufptr, bufend - bufptr)) > 0)
        bufptr += bytes;

      if (bytes < 0)
      {
       /*
        * Read error, abort!
	*/

        puts("READ ERROR!");
	ret = 0;
	break;
      }

      *bufptr = '\0';

     /*
      * Find the first textarea element - that will have the translation data...
      */

      if ((bufptr = strstr(buffer, "<textarea")) == NULL)
      {
       /*
        * No textarea, abort!
	*/

        puts("NO TEXTAREA!");
	ret = 0;
	break;
      }

      if ((bufptr = strchr(bufptr, '>')) == NULL)
      {
       /*
        * textarea doesn't end, abort!
	*/

        puts("TEXTAREA SHORT DATA!");
	ret = 0;
	break;
      }

      bufptr ++;

      if ((bufend = strstr(bufptr, "</textarea>")) == NULL)
      {
       /*
        * textarea doesn't close, abort!
	*/

        puts("/TEXTAREA SHORT DATA!");
	ret = 0;
	break;
      }

      *bufend = '\0';

     /*
      * Copy the translation...
      */

      m->str = strdup(bufptr);

     /*
      * Convert character entities to regular chars...
      */

      for (bufptr = strchr(m->str, '&');
           bufptr;
	   bufptr = strchr(bufptr + 1, '&'))
      {
        if (!strncmp(bufptr, "&lt;", 4))
	{
	  *bufptr = '<';
	  _cups_strcpy(bufptr + 1, bufptr + 4);
	}
        else if (!strncmp(bufptr, "&gt;", 4))
	{
	  *bufptr = '>';
	  _cups_strcpy(bufptr + 1, bufptr + 4);
	}
        else if (!strncmp(bufptr, "&amp;", 5))
	  _cups_strcpy(bufptr + 1, bufptr + 5);
      }

      printf("\"%s\"\n", m->str);
    }
    else if (status == HTTP_ERROR)
    {
      printf("NETWORK ERROR (%s)!\n", strerror(httpError(http)));
      ret = 0;
      break;
    }
    else
    {
      printf("HTTP ERROR %d!\n", status);
      ret = 0;
      break;
    }
  }

  httpClose(http);

  return (ret);
}


/*
 * 'write_string()' - Write a quoted string to a file.
 */

int					/* O - 1 on success, 0 on failure */
write_string(cups_file_t *fp,		/* I - File to write to */
             const char  *s)		/* I - String */
{
  while (*s)
  {
    switch (*s)
    {
      case '\n' :
          if (cupsFilePuts(fp, "\\n") < 0)
	    return (0);
	  break;

      case '\r' :
          if (cupsFilePuts(fp, "\\r") < 0)
	    return (0);
	  break;

      case '\t' :
          if (cupsFilePuts(fp, "\\t") < 0)
	    return (0);
	  break;

      case '\\' :
          if (cupsFilePuts(fp, "\\\\") < 0)
	    return (0);
	  break;

      case '\"' :
          if (cupsFilePuts(fp, "\\\"") < 0)
	    return (0);
	  break;

      default :
          if ((*s & 255) < ' ')
	  {
            if (cupsFilePrintf(fp, "\\%o", *s) < 0)
	      return (0);
	  }
	  else if (cupsFilePutChar(fp, *s) < 0)
	    return (0);
	  break;
    }

    s ++;
  }

  return (1);
}


/*
 * End of "$Id$".
 */
