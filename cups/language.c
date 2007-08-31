/*
 * "$Id$"
 *
 *   I18N/language support for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
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
 *   _cupsEncodingName()    - Return the character encoding name string
 *                            for the given encoding enumeration.
 *   cupsLangDefault()      - Return the default language.
 *   cupsLangEncoding()     - Return the character encoding (us-ascii, etc.)
 *                            for the given language.
 *   cupsLangFlush()        - Flush all language data out of the cache.
 *   cupsLangFree()         - Free language data.
 *   cupsLangGet()          - Get a language.
 *   _cupsLangString()      - Get a message string.
 *   _cupsMessageFree()     - Free a messages array.
 *   _cupsMessageLoad()     - Load a .po file into a messages array.
 *   _cupsMessageLookup()   - Lookup a message string.
 *   appleLangDefault()     - Get the default locale string.
 *   cups_cache_lookup()    - Lookup a language in the cache...
 *   cups_message_compare() - Compare two messages.
 *   cups_unquote()         - Unquote characters in strings...
 */

/*
 * Include necessary headers...
 */

#include "globals.h"
#include "debug.h"
#include <stdlib.h>
#include <errno.h>
#ifdef HAVE_LANGINFO_H
#  include <langinfo.h>
#endif /* HAVE_LANGINFO_H */
#ifdef WIN32
#  include <io.h>
#else
#  include <unistd.h>
#endif /* WIN32 */
#ifdef HAVE_COREFOUNDATION_H
#  include <CoreFoundation/CoreFoundation.h>
#endif /* HAVE_COREFOUNDATION_H */


/*
 * Local globals...
 */

#ifdef HAVE_PTHREAD_H
static pthread_mutex_t	lang_mutex = PTHREAD_MUTEX_INITIALIZER;
					/* Mutex to control access to cache */
#endif /* HAVE_PTHREAD_H */
static cups_lang_t	*lang_cache = NULL;
					/* Language string cache */


/*
 * Local functions...
 */

#ifdef __APPLE__
static const char	*appleLangDefault(void);
#endif /* __APPLE__ */
static cups_lang_t	*cups_cache_lookup(const char *name,
			                   cups_encoding_t encoding);
static int		cups_message_compare(_cups_message_t *m1,
			                     _cups_message_t *m2);
static void		cups_unquote(char *d, const char *s);


/*
 * Local globals...
 */

static const char * const lang_encodings[] =
			{		/* Encoding strings */
			  "us-ascii",		"iso-8859-1",
			  "iso-8859-2",		"iso-8859-3",
			  "iso-8859-4",		"iso-8859-5",
			  "iso-8859-6",		"iso-8859-7",
			  "iso-8859-8",		"iso-8859-9",
			  "iso-8859-10",	"utf-8",
			  "iso-8859-13",	"iso-8859-14",
			  "iso-8859-15",	"windows-874",
			  "windows-1250",	"windows-1251",
			  "windows-1252",	"windows-1253",
			  "windows-1254",	"windows-1255",
			  "windows-1256",	"windows-1257",
			  "windows-1258",	"koi8-r",
			  "koi8-u",		"iso-8859-11",
			  "iso-8859-16",	"mac-roman",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "windows-932",	"windows-936",
			  "windows-949",	"windows-950",
			  "windows-1361",	"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "euc-cn",		"euc-jp",
			  "euc-kr",		"euc-tw"
			};


/*
 * '_cupsEncodingName()' - Return the character encoding name string
 *                         for the given encoding enumeration.
 */

const char *				/* O - Character encoding */
_cupsEncodingName(
    cups_encoding_t encoding)		/* I - Encoding value */
{
  if (encoding < 0 ||
      encoding >= (sizeof(lang_encodings) / sizeof(const char *)))
    return (lang_encodings[0]);
  else
    return (lang_encodings[encoding]);
}


/*
 * 'cupsLangDefault()' - Return the default language.
 */

cups_lang_t *				/* O - Language data */
cupsLangDefault(void)
{
  return (cupsLangGet(NULL));
}


/*
 * 'cupsLangEncoding()' - Return the character encoding (us-ascii, etc.)
 *                        for the given language.
 */

const char *				/* O - Character encoding */
cupsLangEncoding(cups_lang_t *lang)	/* I - Language data */
{
  if (lang == NULL)
    return ((char*)lang_encodings[0]);
  else
    return ((char*)lang_encodings[lang->encoding]);
}


/*
 * 'cupsLangFlush()' - Flush all language data out of the cache.
 */

void
cupsLangFlush(void)
{
  cups_lang_t	*lang,			/* Current language */
		*next;			/* Next language */


 /*
  * Free all languages in the cache...
  */

#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock(&lang_mutex);
#endif /* HAVE_PTHREAD_H */

  for (lang = lang_cache; lang != NULL; lang = next)
  {
   /*
    * Free all messages...
    */

    _cupsMessageFree(lang->strings);

   /*
    * Then free the language structure itself...
    */

    next = lang->next;
    free(lang);
  }

  lang_cache = NULL;

#ifdef HAVE_PTHREAD_H
  pthread_mutex_unlock(&lang_mutex);
#endif /* HAVE_PTHREAD_H */
}


/*
 * 'cupsLangFree()' - Free language data.
 *
 * This does not actually free anything; use cupsLangFlush() for that.
 */

void
cupsLangFree(cups_lang_t *lang)		/* I - Language to free */
{
#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock(&lang_mutex);
#endif /* HAVE_PTHREAD_H */

  if (lang != NULL && lang->used > 0)
    lang->used --;

#ifdef HAVE_PTHREAD_H
  pthread_mutex_unlock(&lang_mutex);
#endif /* HAVE_PTHREAD_H */
}


/*
 * 'cupsLangGet()' - Get a language.
 */

cups_lang_t *				/* O - Language data */
cupsLangGet(const char *language)	/* I - Language or locale */
{
  int			i;		/* Looping var */
#ifndef __APPLE__
  char			locale[255];	/* Copy of locale name */
#endif /* !__APPLE__ */
  char			langname[16],	/* Requested language name */
			country[16],	/* Country code */
			charset[16],	/* Character set */
			*csptr,		/* Pointer to CODESET string */
			*ptr,		/* Pointer into language/charset */
			real[48],	/* Real language name */
			filename[1024];	/* Filename for language locale file */
  cups_encoding_t	encoding;	/* Encoding to use */
  cups_lang_t		*lang;		/* Current language... */
  _cups_globals_t	*cg = _cupsGlobals();
  					/* Pointer to library globals */
  static const char * const locale_encodings[] =
		{			/* Locale charset names */
		  "ASCII",	"ISO88591",	"ISO88592",	"ISO88593",
		  "ISO88594",	"ISO88595",	"ISO88596",	"ISO88597",
		  "ISO88598",	"ISO88599",	"ISO885910",	"UTF8",
		  "ISO885913",	"ISO885914",	"ISO885915",	"CP874",
		  "CP1250",	"CP1251",	"CP1252",	"CP1253",
		  "CP1254",	"CP1255",	"CP1256",	"CP1257",
		  "CP1258",	"KOI8R",	"KOI8U",	"ISO885911",
		  "ISO885916",	"MACROMAN",	"",		"",

		  "",		"",		"",		"",
		  "",		"",		"",		"",
		  "",		"",		"",		"",
		  "",		"",		"",		"",
		  "",		"",		"",		"",
		  "",		"",		"",		"",
		  "",		"",		"",		"",
		  "",		"",		"",		"",

		  "CP932",	"CP936",	"CP949",	"CP950",
		  "CP1361",	"",		"",		"",
		  "",		"",		"",		"",
		  "",		"",		"",		"",
		  "",		"",		"",		"",
		  "",		"",		"",		"",
		  "",		"",		"",		"",
		  "",		"",		"",		"",

		  "",		"",		"",		"",
		  "",		"",		"",		"",
		  "",		"",		"",		"",
		  "",		"",		"",		"",
		  "",		"",		"",		"",
		  "",		"",		"",		"",
		  "",		"",		"",		"",
		  "",		"",		"",		"",

		  "EUCCN",	"EUCJP",	"EUCKR",	"EUCTW"
		};


  DEBUG_printf(("cupsLangGet(language=\"%s\")\n", language ? language : "(null)"));

#ifdef __APPLE__
 /*
  * Set the character set to UTF-8...
  */

  strcpy(charset, "UTF8");

 /*
  * Apple's setlocale doesn't give us the user's localization 
  * preference so we have to look it up this way...
  */

  if (!language && (language = getenv("LANG")) == NULL)
    language = appleLangDefault();

#else
 /*
  * Set the charset to "unknown"...
  */

  charset[0] = '\0';

 /*
  * Use setlocale() to determine the currently set locale, and then
  * fallback to environment variables to avoid setting the locale,
  * since setlocale() is not thread-safe!
  */

  if (!language)
  {
   /*
    * First see if the locale has been set; if it is still "C" or
    * "POSIX", use the environment to get the default...
    */

#  ifdef LC_MESSAGES
    ptr = setlocale(LC_MESSAGES, NULL);
#  else
    ptr = setlocale(LC_ALL, NULL);
#  endif /* LC_MESSAGES */

    DEBUG_printf(("cupsLangGet: current locale is \"%s\"\n",
                  ptr ? ptr : "(null)"));

    if (!ptr || !strcmp(ptr, "C") || !strcmp(ptr, "POSIX"))
    {
     /*
      * Get the character set from the LC_CTYPE locale setting...
      */

      if ((ptr = getenv("LC_CTYPE")) == NULL)
        if ((ptr = getenv("LC_ALL")) == NULL)
	  if ((ptr = getenv("LANG")) == NULL)
	    ptr = "en_US";

      if ((csptr = strchr(ptr, '.')) != NULL)
      {
       /*
        * Extract the character set from the environment...
	*/

	for (ptr = charset, csptr ++; *csptr; csptr ++)
	  if (ptr < (charset + sizeof(charset) - 1) && isalnum(*csptr & 255))
	    *ptr++ = *csptr;

        *ptr = '\0';
      }

     /*
      * Get the locale for messages from the LC_MESSAGES locale setting...
      */

      if ((ptr = getenv("LC_MESSAGES")) == NULL)
        if ((ptr = getenv("LC_ALL")) == NULL)
	  if ((ptr = getenv("LANG")) == NULL)
	    ptr = "en_US";
    }

    if (ptr)
    {
      strlcpy(locale, ptr, sizeof(locale));
      language = locale;

      DEBUG_printf(("cupsLangGet: new language value is \"%s\"\n",
                    language ? language : "(null)"));
    }
  }
#endif /* __APPLE__ */

 /*
  * If "language" is NULL at this point, then chances are we are using
  * a language that is not installed for the base OS.
  */

  if (!language)
  {
   /*
    * Switch to the POSIX ("C") locale...
    */

    language = "C";
  }

#ifdef CODESET
 /*
  * On systems that support the nl_langinfo(CODESET) call, use
  * this value as the character set...
  */

  if (!charset[0] && (csptr = nl_langinfo(CODESET)) != NULL)
  {
   /*
    * Copy all of the letters and numbers in the CODESET string...
    */

    for (ptr = charset; *csptr; csptr ++)
      if (isalnum(*csptr & 255) && ptr < (charset + sizeof(charset) - 1))
        *ptr++ = *csptr;

    *ptr = '\0';

    DEBUG_printf(("cupsLangGet: charset set to \"%s\" via nl_langinfo(CODESET)...\n",
                  charset));
  }
#endif /* CODESET */

 /*
  * If we don't have a character set by now, default to UTF-8...
  */

  if (!charset[0])
    strcpy(charset, "UTF8");

 /*
  * Parse the language string passed in to a locale string. "C" is the
  * standard POSIX locale and is copied unchanged.  Otherwise the
  * language string is converted from ll-cc[.charset] (language-country)
  * to ll_CC[.CHARSET] to match the file naming convention used by all
  * POSIX-compliant operating systems.  Invalid language names are mapped
  * to the POSIX locale.
  */

  country[0] = '\0';

  if (language == NULL || !language[0] ||
      !strcmp(language, "POSIX"))
    strcpy(langname, "C");
  else
  {
   /*
    * Copy the parts of the locale string over safely...
    */

    for (ptr = langname; *language; language ++)
      if (*language == '_' || *language == '-' || *language == '.')
	break;
      else if (ptr < (langname + sizeof(langname) - 1))
        *ptr++ = tolower(*language & 255);

    *ptr = '\0';

    if (*language == '_' || *language == '-')
    {
     /*
      * Copy the country code...
      */

      for (language ++, ptr = country; *language; language ++)
	if (*language == '.')
	  break;
	else if (ptr < (country + sizeof(country) - 1))
          *ptr++ = toupper(*language & 255);

      *ptr = '\0';
    }

    if (*language == '.' && !charset[0])
    {
     /*
      * Copy the encoding...
      */

      for (language ++, ptr = charset; *language; language ++)
        if (isalnum(*language & 255) && ptr < (charset + sizeof(charset) - 1))
          *ptr++ = toupper(*language & 255);

      *ptr = '\0';
    }

   /*
    * Force a POSIX locale for an invalid language name...
    */

    if (strlen(langname) != 2)
    {
      strcpy(langname, "C");
      country[0] = '\0';
      charset[0] = '\0';
    }
  }

  DEBUG_printf(("cupsLangGet: langname=\"%s\", country=\"%s\", charset=\"%s\"\n",
                langname, country, charset));

 /*
  * Figure out the desired encoding...
  */

  encoding = CUPS_AUTO_ENCODING;

  if (charset[0])
  {
    for (i = 0;
         i < (int)(sizeof(locale_encodings) / sizeof(locale_encodings[0]));
	 i ++)
      if (!strcasecmp(charset, locale_encodings[i]))
      {
	encoding = (cups_encoding_t)i;
	break;
      }

    if (encoding == CUPS_AUTO_ENCODING)
    {
     /*
      * Map alternate names for various character sets...
      */

      if (!strcasecmp(charset, "iso-2022-jp") ||
          !strcasecmp(charset, "sjis"))
	encoding = CUPS_WINDOWS_932;
      else if (!strcasecmp(charset, "iso-2022-cn"))
	encoding = CUPS_WINDOWS_936;
      else if (!strcasecmp(charset, "iso-2022-kr"))
	encoding = CUPS_WINDOWS_949;
      else if (!strcasecmp(charset, "big5"))
	encoding = CUPS_WINDOWS_950;
    }
  }

  DEBUG_printf(("cupsLangGet: encoding=%d(%s)\n", encoding,
                encoding == CUPS_AUTO_ENCODING ? "auto" :
		    lang_encodings[encoding]));

 /*
  * See if we already have this language/country loaded...
  */

  if (country[0])
  {
    snprintf(real, sizeof(real), "%s_%s", langname, country);

    snprintf(filename, sizeof(filename), "%s/%s/cups_%s.po", cg->localedir,
             real, real);
  }
  else
  {
    strcpy(real, langname);
    filename[0] = '\0';			/* anti-compiler-warning-code */
  }

#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock(&lang_mutex);
#endif /* HAVE_PTHREAD_H */

  if ((lang = cups_cache_lookup(real, encoding)) != NULL)
  {
#ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock(&lang_mutex);
#endif /* HAVE_PTHREAD_H */

    return (lang);
  }

  if (!country[0] || access(filename, 0))
  {
   /*
    * Country localization not available, look for generic localization...
    */

    snprintf(filename, sizeof(filename), "%s/%s/cups_%s.po", cg->localedir,
             langname, langname);

    if (access(filename, 0))
    {
     /*
      * No generic localization, so use POSIX...
      */

      DEBUG_printf(("access(\"%s\", 0): %s\n", filename, strerror(errno)));

      snprintf(filename, sizeof(filename), "%s/C/cups_C.po", cg->localedir);
    }
  }

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
#ifdef HAVE_PTHREAD_H
      pthread_mutex_unlock(&lang_mutex);
#endif /* HAVE_PTHREAD_H */

      return (NULL);
    }

    lang->next = lang_cache;
    lang_cache = lang;
  }
  else
  {
   /*
    * Free all old strings as needed...
    */

    _cupsMessageFree(lang->strings);
  }

 /*
  * Then assign the language and encoding fields...
  */

  lang->used ++;
  strlcpy(lang->language, real, sizeof(lang->language));

  if (encoding != CUPS_AUTO_ENCODING)
    lang->encoding = encoding;
  else
    lang->encoding = CUPS_UTF8;

 /*
  * Read the strings from the file...
  */

  lang->strings = _cupsMessageLoad(filename);

 /*
  * Return...
  */

#ifdef HAVE_PTHREAD_H
  pthread_mutex_unlock(&lang_mutex);
#endif /* HAVE_PTHREAD_H */

  return (lang);
}


/*
 * '_cupsLangString()' - Get a message string.
 *
 * The returned string is UTF-8 encoded; use cupsUTF8ToCharset() to
 * convert the string to the language encoding.
 */

const char *				/* O - Localized message */
_cupsLangString(cups_lang_t *lang,	/* I - Language */
                const char  *message)	/* I - Message */
{
 /*
  * Range check input...
  */

  if (!lang || !message)
    return (message);

#ifdef HAVE_PTHREAD_H
  {
    const char *s;			/* Localized message */

    pthread_mutex_lock(&lang_mutex);

    s = _cupsMessageLookup(lang->strings, message);

    pthread_mutex_unlock(&lang_mutex);

    return (s);
  }
#else
  return (_cupsMessageLookup(lang->strings, message));
#endif /* HAVE_PTHREAD_H */
}


/*
 * '_cupsMessageFree()' - Free a messages array.
 */

void
_cupsMessageFree(cups_array_t *a)	/* I - Message array */
{
  _cups_message_t	*m;		/* Current message */


  for (m = (_cups_message_t *)cupsArrayFirst(a);
       m;
       m = (_cups_message_t *)cupsArrayNext(a))
  {
   /*
    * Remove the message from the array, then free the message and strings.
    */

    cupsArrayRemove(a, m);

    if (m->id)
      free(m->id);

    if (m->str)
      free(m->str);

    free(m);
  }

 /*
  * Free the array...
  */

  cupsArrayDelete(a);
}


/*
 * '_cupsMessageLoad()' - Load a .po file into a messages array.
 */

cups_array_t *				/* O - New message array */
_cupsMessageLoad(const char *filename)	/* I - Message catalog to load */
{
  cups_file_t		*fp;		/* Message file */
  cups_array_t		*a;		/* Message array */
  _cups_message_t	*m;		/* Current message */
  char			s[4096],	/* String buffer */
			*ptr,		/* Pointer into buffer */
			*temp;		/* New string */
  int			length;		/* Length of combined strings */


  DEBUG_printf(("_cupsMessageLoad(filename=\"%s\")\n", filename));

 /*
  * Create an array to hold the messages...
  */

  if ((a = cupsArrayNew((cups_array_func_t)cups_message_compare, NULL)) == NULL)
    return (NULL);

 /*
  * Open the message catalog file...
  */

  if ((fp = cupsFileOpen(filename, "r")) == NULL)
    return (a);

 /*
  * Read messages from the catalog file until EOF...
  *
  * The format is the GNU gettext .po format, which is fairly simple:
  *
  *     msgid "some text"
  *     msgstr "localized text"
  *
  * The ID and localized text can span multiple lines using the form:
  *
  *     msgid ""
  *     "some long text"
  *     msgstr ""
  *     "localized text spanning "
  *     "multiple lines"
  */

  m = NULL;

  while (cupsFileGets(fp, s, sizeof(s)) != NULL)
  {
   /*
    * Skip blank and comment lines...
    */

    if (s[0] == '#' || !s[0])
      continue;

   /*
    * Strip the trailing quote...
    */

    if ((ptr = strrchr(s, '\"')) == NULL)
      continue;

    *ptr = '\0';

   /*
    * Find start of value...
    */
    
    if ((ptr = strchr(s, '\"')) == NULL)
      continue;

    ptr ++;

   /*
    * Unquote the text...
    */

    cups_unquote(ptr, ptr);

   /*
    * Create or add to a message...
    */

    if (!strncmp(s, "msgid", 5))
    {
     /*
      * Add previous message as needed...
      */

      if (m)
        cupsArrayAdd(a, m);

     /*
      * Create a new message with the given msgid string...
      */

      if ((m = (_cups_message_t *)calloc(1, sizeof(_cups_message_t))) == NULL)
      {
        cupsFileClose(fp);
	return (a);
      }

      m->id = strdup(ptr);
    }
    else if (s[0] == '\"' && m)
    {
     /*
      * Append to current string...
      */

      length = (int)strlen(m->str ? m->str : m->id);

      if ((temp = realloc(m->str ? m->str : m->id,
                          length + strlen(ptr) + 1)) == NULL)
      {
	cupsFileClose(fp);
	return (a);
      }

      if (m->str)
      {
       /*
        * Copy the new portion to the end of the msgstr string - safe
	* to use strcpy because the buffer is allocated to the correct
	* size...
	*/

        m->str = temp;

	strcpy(m->str + length, ptr);
      }
      else
      {
       /*
        * Copy the new portion to the end of the msgid string - safe
	* to use strcpy because the buffer is allocated to the correct
	* size...
	*/

        m->id = temp;

	strcpy(m->id + length, ptr);
      }
    }
    else if (!strncmp(s, "msgstr", 6) && m)
    {
     /*
      * Set the string...
      */

      m->str = strdup(ptr);
    }
  }

 /*
  * Add the last message string to the array as needed...
  */

  if (m)
    cupsArrayAdd(a, m);

 /*
  * Close the message catalog file and return the new array...
  */

  cupsFileClose(fp);

  return (a);
}


/*
 * '_cupsMessageLookup()' - Lookup a message string.
 */

const char *				/* O - Localized message */
_cupsMessageLookup(cups_array_t *a,	/* I - Message array */
                   const char   *m)	/* I - Message */
{
  _cups_message_t	key,		/* Search key */
			*match;		/* Matching message */


 /*
  * Lookup the message string; if it doesn't exist in the catalog,
  * then return the message that was passed to us...
  */

  key.id = (char *)m;
  match  = (_cups_message_t *)cupsArrayFind(a, &key);

  if (match && match->str)
    return (match->str);
  else
    return (m);
}


#ifdef __APPLE__
/*
 * Code & data to translate OSX's language names to their ISO 639-1 locale.
 *
 * The first version uses the new CoreFoundation API added in 10.3 (Panther),
 * the second is for 10.2 (Jaguar).
 */

#  ifdef HAVE_CF_LOCALE_ID

typedef struct
{
  const char * const name;		/* Language name */
  const char * const locale;		/* Locale name */
} _apple_name_locale_t;

static const _apple_name_locale_t apple_name_locale[] =
{
  { "en"	, "en_US" },
  { "nb"	, "no"    },
  { "zh-Hans"	, "zh_CN" },
  { "zh-Hant"	, "zh_TW" }
};

/*
 * 'appleLangDefault()' - Get the default locale string.
 */

static const char *			/* O - Locale string */
appleLangDefault(void)
{
  int			i;		/* Looping var */
  CFBundleRef		bundle;		/* Main bundle (if any) */
  CFArrayRef		bundleList;	/* List of localizations in bundle */
  CFPropertyListRef 	localizationList;
					/* List of localization data */
  CFStringRef		languageName;	/* Current name */
  CFStringRef		localeName;	/* Canonical from of name */
  char			*lang;		/* LANG environment variable */
  _cups_globals_t	*cg = _cupsGlobals();
  					/* Pointer to library globals */


 /*
  * Only do the lookup and translation the first time.
  */

  if (!cg->language[0])
  {
    if ((lang = getenv("LANG")))
    {
      strlcpy(cg->language, lang, sizeof(cg->language));
      return (cg->language);
    }
    else if ((bundle = CFBundleGetMainBundle()) != NULL)
    {
      bundleList = CFBundleCopyBundleLocalizations(bundle);
      localizationList =
	  CFBundleCopyPreferredLocalizationsFromArray(bundleList);

      CFRelease(bundleList);

      if (CFArrayGetCount(localizationList) == 0)
      {
       /*
	* If we can't find a common language supported by the application,
	* then the app will default to English...
	*/

	CFRelease(localizationList);
	strlcpy(cg->language, "en_US.UTF-8", sizeof(cg->language));
	return  (cg->language);
      }
    }
    else
      localizationList =
	  CFPreferencesCopyAppValue(CFSTR("AppleLanguages"),
				    kCFPreferencesCurrentApplication);

    if (localizationList)
    {
      if (CFGetTypeID(localizationList) == CFArrayGetTypeID() &&
	  CFArrayGetCount(localizationList) > 0)
      {
	languageName = CFArrayGetValueAtIndex(localizationList, 0);

	if (languageName &&
	    CFGetTypeID(languageName) == CFStringGetTypeID())
	{
	  localeName = CFLocaleCreateCanonicalLocaleIdentifierFromString(
			   kCFAllocatorDefault, languageName);

	  if (localeName)
	  {
	    CFStringGetCString(localeName, cg->language, sizeof(cg->language),
			       kCFStringEncodingASCII);
	    CFRelease(localeName);

	    DEBUG_printf(("appleLangDefault: cg->language=\"%s\"\n",
			  cg->language));

	   /*
	    * Map new language identifiers to locales...
	    */

	    for (i = 0;
		 i < sizeof(apple_name_locale) / sizeof(apple_name_locale[0]);
		 i++)
	    {
	      if (!strcmp(cg->language, apple_name_locale[i].name))
	      {
		DEBUG_printf(("appleLangDefault: mapping \"%s\" to \"%s\"...\n",
			      cg->language, apple_name_locale[i].locale));
		strlcpy(cg->language, apple_name_locale[i].locale, 
			sizeof(cg->language));
		break;
	      }
	    }

	   /*
	    * Convert language subtag into region subtag...
	    */

	    if (cg->language[2] == '-')
	      cg->language[2] = '_';

	    if (!strchr(cg->language, '.'))
	      strlcat(cg->language, ".UTF-8", sizeof(cg->language));
	  }
	}
      }

      CFRelease(localizationList);
    }
  
   /*
    * If we didn't find the language, default to en_US...
    */

    if (!cg->language[0])
      strlcpy(cg->language, "en_US.UTF-8", sizeof(cg->language));
  }

 /*
  * Return the cached locale...
  */

  return (cg->language);
}
#  else
/*
 * Code & data to translate OSX 10.2's language names to their ISO 639-1
 * locale.
 */

typedef struct
{
  const char * const name;		/* Language name */
  const char * const locale;		/* Locale name */
} _apple_name_locale_t;

static const _apple_name_locale_t apple_name_locale[] =
{
  { "English"     , "en_US.UTF-8" },	{ "French"     , "fr.UTF-8" },
  { "German"      , "de.UTF-8" },	{ "Italian"    , "it.UTF-8" },  
  { "Dutch"       , "nl.UTF-8" },	{ "Swedish"    , "sv.UTF-8" },
  { "Spanish"     , "es.UTF-8" },	{ "Danish"     , "da.UTF-8" },  
  { "Portuguese"  , "pt.UTF-8" },	{ "Norwegian"  , "no.UTF-8" },
  { "Hebrew"      , "he.UTF-8" },	{ "Japanese"   , "ja.UTF-8" },  
  { "Arabic"      , "ar.UTF-8" },	{ "Finnish"    , "fi.UTF-8" },
  { "Greek"       , "el.UTF-8" },	{ "Icelandic"  , "is.UTF-8" },  
  { "Maltese"     , "mt.UTF-8" },	{ "Turkish"    , "tr.UTF-8" },
  { "Croatian"    , "hr.UTF-8" },	{ "Chinese"    , "zh.UTF-8" },  
  { "Urdu"        , "ur.UTF-8" },	{ "Hindi"      , "hi.UTF-8" },
  { "Thai"        , "th.UTF-8" },	{ "Korean"     , "ko.UTF-8" },  
  { "Lithuanian"  , "lt.UTF-8" },	{ "Polish"     , "pl.UTF-8" },
  { "Hungarian"   , "hu.UTF-8" },	{ "Estonian"   , "et.UTF-8" },  
  { "Latvian"     , "lv.UTF-8" },	{ "Sami"       , "se.UTF-8" },
  { "Faroese"     , "fo.UTF-8" },	{ "Farsi"      , "fa.UTF-8" },  
  { "Russian"     , "ru.UTF-8" },	{ "Chinese"    , "zh.UTF-8" },
  { "Dutch"       , "nl.UTF-8" },	{ "Irish"      , "ga.UTF-8" },  
  { "Albanian"    , "sq.UTF-8" },	{ "Romanian"   , "ro.UTF-8" },
  { "Czech"       , "cs.UTF-8" },	{ "Slovak"     , "sk.UTF-8" },  
  { "Slovenian"   , "sl.UTF-8" },	{ "Yiddish"    , "yi.UTF-8" },
  { "Serbian"     , "sr.UTF-8" },	{ "Macedonian" , "mk.UTF-8" },  
  { "Bulgarian"   , "bg.UTF-8" },	{ "Ukrainian"  , "uk.UTF-8" },
  { "Byelorussian", "be.UTF-8" },	{ "Uzbek"      , "uz.UTF-8" },  
  { "Kazakh"      , "kk.UTF-8" },	{ "Azerbaijani", "az.UTF-8" },
  { "Azerbaijani" , "az.UTF-8" },	{ "Armenian"   , "hy.UTF-8" },  
  { "Georgian"    , "ka.UTF-8" },	{ "Moldavian"  , "mo.UTF-8" },
  { "Kirghiz"     , "ky.UTF-8" },	{ "Tajiki"     , "tg.UTF-8" },  
  { "Turkmen"     , "tk.UTF-8" },	{ "Mongolian"  , "mn.UTF-8" },
  { "Mongolian"   , "mn.UTF-8" },	{ "Pashto"     , "ps.UTF-8" },  
  { "Kurdish"     , "ku.UTF-8" },	{ "Kashmiri"   , "ks.UTF-8" },
  { "Sindhi"      , "sd.UTF-8" },	{ "Tibetan"    , "bo.UTF-8" },  
  { "Nepali"      , "ne.UTF-8" },	{ "Sanskrit"   , "sa.UTF-8" },
  { "Marathi"     , "mr.UTF-8" },	{ "Bengali"    , "bn.UTF-8" },  
  { "Assamese"    , "as.UTF-8" },	{ "Gujarati"   , "gu.UTF-8" },
  { "Punjabi"     , "pa.UTF-8" },	{ "Oriya"      , "or.UTF-8" },  
  { "Malayalam"   , "ml.UTF-8" },	{ "Kannada"    , "kn.UTF-8" },
  { "Tamil"       , "ta.UTF-8" },	{ "Telugu"     , "te.UTF-8" },  
  { "Sinhalese"   , "si.UTF-8" },	{ "Burmese"    , "my.UTF-8" },
  { "Khmer"       , "km.UTF-8" },	{ "Lao"        , "lo.UTF-8" },  
  { "Vietnamese"  , "vi.UTF-8" },	{ "Indonesian" , "id.UTF-8" },
  { "Tagalog"     , "tl.UTF-8" },	{ "Malay"      , "ms.UTF-8" },  
  { "Malay"       , "ms.UTF-8" },	{ "Amharic"    , "am.UTF-8" },
  { "Tigrinya"    , "ti.UTF-8" },	{ "Oromo"      , "om.UTF-8" },  
  { "Somali"      , "so.UTF-8" },	{ "Swahili"    , "sw.UTF-8" },
  { "Kinyarwanda" , "rw.UTF-8" },	{ "Rundi"      , "rn.UTF-8" },  
  { "Nyanja"      , "" },		{ "Malagasy"   , "mg.UTF-8" },
  { "Esperanto"   , "eo.UTF-8" },	{ "Welsh"      , "cy.UTF-8" },  
  { "Basque"      , "eu.UTF-8" },	{ "Catalan"    , "ca.UTF-8" },
  { "Latin"       , "la.UTF-8" },	{ "Quechua"    , "qu.UTF-8" },  
  { "Guarani"     , "gn.UTF-8" },	{ "Aymara"     , "ay.UTF-8" },
  { "Tatar"       , "tt.UTF-8" },	{ "Uighur"     , "ug.UTF-8" },  
  { "Dzongkha"    , "dz.UTF-8" },	{ "Javanese"   , "jv.UTF-8" },
  { "Sundanese"   , "su.UTF-8" },	{ "Galician"   , "gl.UTF-8" },  
  { "Afrikaans"   , "af.UTF-8" },	{ "Breton"     , "br.UTF-8" },
  { "Inuktitut"   , "iu.UTF-8" },	{ "Scottish"   , "gd.UTF-8" },  
  { "Manx"        , "gv.UTF-8" },	{ "Irish"      , "ga.UTF-8" },
  { "Tongan"      , "to.UTF-8" },	{ "Greek"      , "el.UTF-8" },  
  { "Greenlandic" , "kl.UTF-8" },	{ "Azerbaijani", "az.UTF-8" }
};


/*
 * 'appleLangDefault()' - Get the default locale string.
 */

static const char *			/* O - Locale string */
appleLangDefault(void)
{
  int			i;		/* Looping var */
  CFPropertyListRef 	localizationList;
					/* List of localization data */
  CFStringRef		localizationName;
					/* Current name */
  char			buff[256];	/* Temporary buffer */
  _cups_globals_t	*cg = _cupsGlobals();
  					/* Pointer to library globals */
  char			*lang;		/* LANG environment variable */


 /*
  * Only do the lookup and translation the first time.
  */

  if (!cg->language[0])
  {
    if ((lang = getenv("LANG")))
      strlcpy(cg->language, lang, sizeof(cg->language));
    else
    {
      localizationList =
          CFPreferencesCopyAppValue(CFSTR("AppleLanguages"),
                                    kCFPreferencesCurrentApplication);

      if (localizationList != NULL)
      {
	if (CFGetTypeID(localizationList) == CFArrayGetTypeID() &&
	    CFArrayGetCount(localizationList) > 0)
	{
	  localizationName = CFArrayGetValueAtIndex(localizationList, 0);

	  if (localizationName != NULL &&
              CFGetTypeID(localizationName) == CFStringGetTypeID())
	  {
	    CFIndex length = CFStringGetLength(localizationName);

	    if (length <= sizeof(buff) &&
		CFStringGetCString(localizationName, buff, sizeof(buff),
	                           kCFStringEncodingASCII))
	    {
	      buff[sizeof(buff) - 1] = '\0';

	      for (i = 0;
		   i < sizeof(apple_name_locale) / sizeof(apple_name_locale[0]);
		   i++)
	      {
		if (!strcasecmp(buff, apple_name_locale[i].name))
		{
		  strlcpy(cg->language, apple_name_locale[i].locale, 
			  sizeof(cg->language));
		  break;
		}
	      }
	    }
	  }
	}

	CFRelease(localizationList);
      }
    }
  
   /*
    * If we didn't find the language, default to en_US...
    */

    if (!cg->language[0])
      strlcpy(cg->language, apple_name_locale[0].locale, sizeof(cg->language));
  }

 /*
  * Return the cached locale...
  */

  return (cg->language);
}
#  endif /* HAVE_CF_LOCALE_ID */
#endif /* __APPLE__ */


/*
 * 'cups_cache_lookup()' - Lookup a language in the cache...
 */

static cups_lang_t *			/* O - Language data or NULL */
cups_cache_lookup(const char      *name,/* I - Name of locale */
                  cups_encoding_t encoding)
					/* I - Encoding of locale */
{
  cups_lang_t	*lang;			/* Current language */


  DEBUG_printf(("cups_cache_lookup(name=\"%s\", encoding=%d(%s))\n", name,
                encoding, encoding == CUPS_AUTO_ENCODING ? "auto" :
		              lang_encodings[encoding]));

 /*
  * Loop through the cache and return a match if found...
  */

  for (lang = lang_cache; lang != NULL; lang = lang->next)
  {
    DEBUG_printf(("cups_cache_lookup: lang=%p, language=\"%s\", encoding=%d(%s)\n",
                  lang, lang->language, lang->encoding,
		  lang_encodings[lang->encoding]));

    if (!strcmp(lang->language, name) &&
        (encoding == CUPS_AUTO_ENCODING || encoding == lang->encoding))
    {
      lang->used ++;

      DEBUG_puts("cups_cache_lookup: returning match!");

      return (lang);
    }
  }

  DEBUG_puts("cups_cache_lookup: returning NULL!");

  return (NULL);
}


/*
 * 'cups_message_compare()' - Compare two messages.
 */

static int				/* O - Result of comparison */
cups_message_compare(
    _cups_message_t *m1,		/* I - First message */
    _cups_message_t *m2)		/* I - Second message */
{
  return (strcmp(m1->id, m2->id));
}


/*
 * 'cups_unquote()' - Unquote characters in strings...
 */

static void
cups_unquote(char       *d,		/* O - Unquoted string */
             const char *s)		/* I - Original string */
{
  while (*s)
  {
    if (*s == '\\')
    {
      s ++;
      if (isdigit(*s))
      {
	*d = 0;

	while (isdigit(*s))
	{
	  *d = *d * 8 + *s - '0';
	  s ++;
	}

	d ++;
      }
      else
      {
	if (*s == 'n')
	  *d ++ = '\n';
	else if (*s == 'r')
	  *d ++ = '\r';
	else if (*s == 't')
	  *d ++ = '\t';
	else
	  *d++ = *s;

	s ++;
      }
    }
    else
      *d++ = *s++;
  }

  *d = '\0';
}


/*
 * End of "$Id$".
 */
