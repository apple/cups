/*
 * "$Id: language.c,v 1.11 1999/08/27 18:22:50 mike Exp $"
 *
 *   I18N/language support for the Common UNIX Printing System (CUPS).
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
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   cupsLangEncoding() - Return the character encoding (us-ascii, etc.)
 *                        for the given language.
 *   cupsLangFlush()    - Flush all language data out of the cache.
 *   cupsLangFree()     - Free language data.
 *   cupsLangGet()      - Get a language.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "string.h"
#include "language.h"


/*
 * Local globals...
 */

static cups_lang_t	*lang_cache = NULL;	/* Language string cache */
static char		*lang_blank = "";	/* Blank constant string */
static char		*lang_encodings[] =	/* Encoding strings */
			{
			  "us-ascii",
			  "iso8859-1",
			  "iso8859-2",
			  "iso8859-3",
			  "iso8859-4",
			  "iso8859-5",
			  "iso8859-6",
			  "iso8859-7",
			  "iso8859-8",
			  "iso8859-9",
			  "iso8859-10",
			  "utf8"
			};
static char		*lang_default[] =	/* Default POSIX locale */
			{
#include "cups_C.h"
			  NULL
			};


/*
 * 'cupsLangEncoding()' - Return the character encoding (us-ascii, etc.)
 *                        for the given language.
 */

char *					/* O - Character encoding */
cupsLangEncoding(cups_lang_t *lang)	/* I - Language data */
{
  if (lang == NULL)
    return (lang_encodings[0]);
  else
    return (lang_encodings[lang->encoding]);
}


/*
 * 'cupsLangFlush()' - Flush all language data out of the cache.
 */

void
cupsLangFlush(void)
{
  int		i;	/* Looping var */
  cups_lang_t	*lang,	/* Current language */
		*next;	/* Next language */


  for (lang = lang_cache; lang != NULL; lang = next)
  {
    for (i = 0; i < CUPS_MSG_MAX; i ++)
      if (lang->messages[i] != NULL && lang->messages[i] != lang_blank)
        free(lang->messages[i]);

    next = lang->next;
    free(lang);
  }
}


/*
 * 'cupsLangFree()' - Free language data.
 *
 * This does not actually free anything; use cupsLangFlush() for that.
 */

void
cupsLangFree(cups_lang_t *lang)	/* I - Language to free */
{
  if (lang != NULL && lang->used > 0)
    lang->used --;
}


/*
 * 'cupsLangGet()' - Get a language.
 */

cups_lang_t *			/* O - Language data */
cupsLangGet(const char *language) /* I - Language or locale */
{
  int		i, count;	/* Looping vars */
  char		langname[16],	/* Requested language name */
		real[16],	/* Real language name */
		filename[1024],	/* Filename for language locale file */
		*localedir;	/* Directory for locale files */
  FILE		*fp;		/* Language locale file pointer */
  char		line[1024];	/* Line from file */
  cups_msg_t	msg;		/* Message number */
  char		*text;		/* Message text */
  cups_lang_t	*lang;		/* Current language... */


 /*
  * Convert the language string passed in to a locale string. "C" is the
  * standard POSIX locale and is copied unchanged.  Otherwise the
  * language string is converted from ll-cc (language-country) to ll_CC
  * to match the file naming convention used by all POSIX-compliant
  * operating systems.
  */

  if (language == NULL || language[0] == '\0' ||
      strcmp(language, "POSIX") == 0)
    strcpy(langname, "C");
  else
    strcpy(langname, language);

  if (strlen(langname) < 2)
    strcpy(real, "C");
  else
  {
    real[0] = tolower(langname[0]);
    real[1] = tolower(langname[1]);

    if (langname[2] == '_' || langname[2] == '-')
    {
      real[2]     = '_';
      real[3]     = toupper(langname[3]);
      real[4]     = toupper(langname[4]);
      real[5]     = '\0';
      langname[5] = '\0';
    }
    else
    {
      langname[2] = '\0';
      real[2]     = '\0';
    }
  }

 /*
  * Next try to open a locale file; we will try the country-localized file
  * first, and then look for generic language file.  If all else fails we
  * will use the POSIX locale.
  */

  if ((localedir = getenv("LOCALEDIR")) == NULL)
    localedir = CUPS_LOCALEDIR;

  sprintf(filename, "%s/%s/cups_%s", localedir, real, real);

  if ((fp = fopen(filename, "r")) == NULL)
    if (strlen(real) > 2)
    {
     /*
      * Nope, see if we can open a generic language file...
      */

      real[2] = '\0';
      sprintf(filename, "%s/%s/cups_%s", localedir, real, real);
      fp = fopen(filename, "r");
    }

 /*
  * Then see if we already have this language loaded...
  */

  for (lang = lang_cache; lang != NULL; lang = lang->next)
    if (strcmp(lang->language, langname) == 0)
    {
      lang->used ++;

      if (fp != NULL)
        fclose(fp);

      return (lang);
    }

 /*
  * OK, we have an open messages file; the first line will contain the
  * language encoding (us-ascii, iso-8859-1, etc.), and the rest will
  * be messages consisting of:
  *
  *    #### SP message text
  *
  * or:
  *
  *    message text
  *
  * If the line starts with a number, then message processing picks up
  * where the number indicates.  Otherwise the last message number is
  * incremented.
  *
  * All leading whitespace is deleted.
  */

  if (fp == NULL)
    strcpy(line, lang_default[0]);
  else if (fgets(line, sizeof(line), fp) == NULL)
  {
   /*
    * Can't read encoding!
    */

    fclose(fp);
    return (NULL);
  }

  i = strlen(line) - 1;
  if (line[i] == '\n')
    line[i] = '\0';	/* Strip LF */

 /*
  * See if there is a free language available; if so, use that
  * record...
  */

  for (lang = lang_cache; lang != NULL; lang = lang->next)
    if (lang->used == 0)
      break;

  if (lang == NULL)
  {
   /*
    * Allocate memory for the language and add it to the cache.
    */

    if ((lang = calloc(sizeof(cups_lang_t), 1)) == NULL)
    {
      fclose(fp);
      return (NULL);
    }

    lang->next = lang_cache;
    lang_cache = lang;
  }


 /*
  * Free all old strings as needed...
  */

  for (i = 0; i < CUPS_MSG_MAX; i ++)
  {
    if (lang->messages[i] != NULL && lang->messages[i] != lang_blank)
      free(lang->messages[i]);

    lang->messages[i] = lang_blank;
  }

 /*
  * Then assign the language and encoding fields...
  */

  lang->used ++;
  strcpy(lang->language, langname);

  for (i = 0; i < (sizeof(lang_encodings) / sizeof(lang_encodings[0])); i ++)
    if (strcmp(lang_encodings[i], line) == 0)
    {
      lang->encoding = (cups_encoding_t)i;
      break;
    }

 /*
  * Read the strings from the file...
  */

  msg   = (cups_msg_t)-1;
  count = 1;

  for (;;)
  {
   /*
    * Read a line from memory or from a file...
    */

    if (fp == NULL)
    {
      if (lang_default[count] == NULL)
        break;

      strcpy(line, lang_default[count]);
    }
    else if (fgets(line, sizeof(line), fp) == NULL)
      break;

    count ++;

   /*
    * Ignore blank lines...
    */

    i = strlen(line) - 1;
    if (line[i] == '\n')
      line[i] = '\0';	/* Strip LF */

    if (line[0] == '\0')
      continue;

   /*
    * Grab the message number and text...
    */

    if (isdigit(line[0]))
      msg = (cups_msg_t)atoi(line);
    else
      msg ++;

    if (msg < 0 || msg >= CUPS_MSG_MAX)
      continue;

    text = line;
    while (isdigit(*text))
      text ++;
    while (isspace(*text))
      text ++;
    
    lang->messages[msg] = strdup(text);
  }

 /*
  * Close the file and return...
  */

  if (fp != NULL)
    fclose(fp);

  return (lang);
}


/*
 * End of "$Id: language.c,v 1.11 1999/08/27 18:22:50 mike Exp $".
 */
