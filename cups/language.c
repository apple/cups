/*
 * "$Id: language.c,v 1.20.2.13 2003/02/04 05:36:15 mike Exp $"
 *
 *   I18N/language support for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products.
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsEncodingName() - Return the character encoding name string
 *                        for the given encoding enumeration.
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

#if defined(__APPLE__)
#  include <CoreFoundation/CoreFoundation.h>
static const char *appleLangDefault(void);
#endif /* __APPLE__ */


/*
 * Local globals...
 */

static cups_lang_t	*lang_cache = NULL;	/* Language string cache */
static const char	*lang_blank = "";	/* Blank constant string */
static const char * const lang_encodings[] =	/* Encoding strings */
			{
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
			  "iso-8859-16",	"unknown",
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
static const char *const lang_default[] =	/* Default POSIX locale */
			{
#include "cups_C.h"
			  NULL
			};


/*
 * 'cupsEncodingName()' - Return the character encoding name string
 *                        for the given encoding enumeration.
 */

const char *				/* O - Character encoding */
cupsEncodingName(cups_encoding_t encoding)
					/* I - Encoding enum */
{
  if (encoding < 0 ||
      encoding >= (sizeof(lang_encodings) / sizeof(const char *)))
    return (lang_encodings[0]);
  else
    return (lang_encodings[encoding]);
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
  char		langname[32],	/* Requested language name */
		*langptr,	/* Pointer into language name */
		real[32],	/* Real language name */
		*realptr,	/* Pointer into real language name */
		filename[1024],	/* Filename for language locale file */
		*localedir;	/* Directory for locale files */
  FILE		*fp;		/* Language locale file pointer */
  char		line[1024];	/* Line from file */
  cups_msg_t	msg;		/* Message number */
  char		*text;		/* Message text */
  cups_lang_t	*lang;		/* Current language... */


#ifdef __APPLE__
 /*
  * Apple's setlocale doesn't give us the user's localization 
  * preference so we have to look it up this way...
  */

  if (language == NULL)
  {
    language = appleLangDefault();
    setlocale(LC_ALL, language);
  }
#elif defined(LC_MESSAGES)
  if (language == NULL)
    language = setlocale(LC_MESSAGES, "");
#else
  if (language == NULL)
    language = setlocale(LC_ALL, "");
#endif /* __APPLE__ */

 /*
  * Convert the language string passed in to a locale string. "C" is the
  * standard POSIX locale and is copied unchanged.  Otherwise the
  * language string is converted from ll-cc (language-country) to ll_cc
  * to match the file naming convention used by all POSIX-compliant
  * operating systems.
  */

  if (language == NULL || language[0] == '\0' ||
      strcmp(language, "POSIX") == 0)
    strcpy(langname, "C");
  else
  {
   /*
    * Copy the locale string over safely...
    */

    strlcpy(langname, language, sizeof(langname));
  }

  if (strlen(langname) < 2)
    strcpy(real, "C");
  else
  {
   /*
    * Convert the language name to a normalized form, e.g.:
    *
    *     ll[_CC[.charset]]
    */

    real[0] = tolower(langname[0]);
    real[1] = tolower(langname[1]);
    realptr = real + 2;
    langptr = langname + 2;

    if (*langptr == '_' || *langptr == '-')
    {
     /*
      * Add country code...
      */

      *realptr++ = '_';
      langptr ++;

      *realptr++ = toupper(*langptr++);
      *realptr++ = toupper(*langptr++);
    }

    if (*langptr == '.')
    {
     /*
      * Add charset...
      */

      *langptr++ = '\0';
      *realptr++ = '.';

      while (*langptr)
      {
        if ((realptr - real) < (sizeof(real) - 1) &&
	    *langptr != '-' && *langptr != '_')
	  *realptr++ = tolower(*langptr++);
        else
          langptr ++;
      }
    }

    *realptr = '\0';
  }

 /*
  * See if we already have this language loaded...
  */

  for (lang = lang_cache; lang != NULL; lang = lang->next)
    if (strcmp(lang->language, langname) == 0)
    {
      lang->used ++;

      return (lang);
    }


 /*
  * Next try to open a locale file; we will try the charset-localized
  * file first, then the country-localized file, and finally look for
  * a generic language file.  If all else fails we will use the POSIX
  * locale.
  */

  if ((localedir = getenv("LOCALEDIR")) == NULL)
    localedir = CUPS_LOCALEDIR;

  do
  {
    snprintf(filename, sizeof(filename), "%s/%s/cups_%s", localedir,
             real, real);

    if ((fp = fopen(filename, "r")) == NULL)
    {
      if ((realptr = strchr(real, '.')) != NULL)
        *realptr = '\0';
      else if ((realptr = strchr(real, '_')) != NULL)
        *realptr = '\0';
    }
  }
  while (fp == NULL && strchr(real, '_') != NULL && strchr(real, '.') != NULL);

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
    strlcpy(line, lang_default[0], sizeof(line));
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

    lang->messages[i] = (char *)lang_blank;
  }

 /*
  * Then assign the language and encoding fields...
  */

  lang->used ++;
  strlcpy(lang->language, langname, sizeof(lang->language));

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

      strlcpy(line, lang_default[count], sizeof(line));
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


#ifdef __APPLE__
/*
 * Code & data to translate OSX's language names to their ISO 639-1 locale.
 *
 * In Radar bug #2563420 there's a request to have CoreFoundation export a
 * function to do this mapping. If this function gets implemented we should
 * use it.
 */

typedef struct
{
  const char * const name;			/* Language name */
  const char * const locale;			/* Locale name */
} apple_name_locale_t;

static const apple_name_locale_t apple_name_locale[] =
{
  { "English"    , "en_US" },{ "French"     , "fr" },  { "German"      , "de" },  { "Italian"   , "it" },  
  { "Dutch"      , "nl" },   { "Swedish"    , "sv" },  { "Spanish"     , "es" },  { "Danish"    , "da" },  
  { "Portuguese" , "pt" },   { "Norwegian"  , "no" },  { "Hebrew"      , "he" },  { "Japanese"  , "ja" },  
  { "Arabic"     , "ar" },   { "Finnish"    , "fi" },  { "Greek"       , "el" },  { "Icelandic" , "is" },  
  { "Maltese"    , "mt" },   { "Turkish"    , "tr" },  { "Croatian"    , "hr" },  { "Chinese"   , "zh" },  
  { "Urdu"       , "ur" },   { "Hindi"      , "hi" },  { "Thai"        , "th" },  { "Korean"    , "ko" },  
  { "Lithuanian" , "lt" },   { "Polish"     , "pl" },  { "Hungarian"   , "hu" },  { "Estonian"  , "et" },  
  { "Latvian"    , "lv" },   { "Sami"       , "se" },  { "Faroese"     , "fo" },  { "Farsi"     , "fa" },  
  { "Russian"    , "ru" },   { "Chinese"    , "zh" },  { "Dutch"       , "nl" },  { "Irish"     , "ga" },  
  { "Albanian"   , "sq" },   { "Romanian"   , "ro" },  { "Czech"       , "cs" },  { "Slovak"    , "sk" },  
  { "Slovenian"  , "sl" },   { "Yiddish"    , "yi" },  { "Serbian"     , "sr" },  { "Macedonian", "mk" },  
  { "Bulgarian"  , "bg" },   { "Ukrainian"  , "uk" },  { "Byelorussian", "be" },  { "Uzbek"     , "uz" },  
  { "Kazakh"     , "kk" },   { "Azerbaijani", "az" },  { "Azerbaijani" , "az" },  { "Armenian"  , "hy" },  
  { "Georgian"   , "ka" },   { "Moldavian"  , "mo" },  { "Kirghiz"     , "ky" },  { "Tajiki"    , "tg" },  
  { "Turkmen"    , "tk" },   { "Mongolian"  , "mn" },  { "Mongolian"   , "mn" },  { "Pashto"    , "ps" },  
  { "Kurdish"    , "ku" },   { "Kashmiri"   , "ks" },  { "Sindhi"      , "sd" },  { "Tibetan"   , "bo" },  
  { "Nepali"     , "ne" },   { "Sanskrit"   , "sa" },  { "Marathi"     , "mr" },  { "Bengali"   , "bn" },  
  { "Assamese"   , "as" },   { "Gujarati"   , "gu" },  { "Punjabi"     , "pa" },  { "Oriya"     , "or" },  
  { "Malayalam"  , "ml" },   { "Kannada"    , "kn" },  { "Tamil"       , "ta" },  { "Telugu"    , "te" },  
  { "Sinhalese"  , "si" },   { "Burmese"    , "my" },  { "Khmer"       , "km" },  { "Lao"       , "lo" },  
  { "Vietnamese" , "vi" },   { "Indonesian" , "id" },  { "Tagalog"     , "tl" },  { "Malay"     , "ms" },  
  { "Malay"      , "ms" },   { "Amharic"    , "am" },  { "Tigrinya"    , "ti" },  { "Oromo"     , "om" },  
  { "Somali"     , "so" },   { "Swahili"    , "sw" },  { "Kinyarwanda" , "rw" },  { "Rundi"     , "rn" },  
  { "Nyanja"     , ""   },   { "Malagasy"   , "mg" },  { "Esperanto"   , "eo" },  { "Welsh"     , "cy" },  
  { "Basque"     , "eu" },   { "Catalan"    , "ca" },  { "Latin"       , "la" },  { "Quechua"   , "qu" },  
  { "Guarani"    , "gn" },   { "Aymara"     , "ay" },  { "Tatar"       , "tt" },  { "Uighur"    , "ug" },  
  { "Dzongkha"   , "dz" },   { "Javanese"   , "jv" },  { "Sundanese"   , "su" },  { "Galician"  , "gl" },  
  { "Afrikaans"  , "af" },   { "Breton"     , "br" },  { "Inuktitut"   , "iu" },  { "Scottish"  , "gd" },  
  { "Manx"       , "gv" },   { "Irish"      , "ga" },  { "Tongan"      , "to" },  { "Greek"     , "el" },  
  { "Greenlandic", "kl" },   { "Azerbaijani", "az" }
};


/*
 * 'appleLangDefault()' - Get the default locale string.
 */

static const char *				/* O - Locale string */
appleLangDefault(void)
{
  int			i;			/* Looping var */
  CFPropertyListRef 	localizationList;	/* List of localization data */
  CFStringRef		localizationName;	/* Current name */
  char			buff[256];		/* Temporary buffer */
  static const char	*language = NULL;	/* Cached language */


 /*
  * Only do the lookup and translation the first time.
  */

  if (language == NULL)
  {
    localizationList = CFPreferencesCopyAppValue(CFSTR("AppleLanguages"),
                                                 kCFPreferencesCurrentApplication);

    if (localizationList != NULL &&
        CFGetTypeID(localizationList) == CFArrayGetTypeID())
    {
      localizationName = CFArrayGetValueAtIndex(localizationList, 0);

      if (localizationName != NULL &&
          CFGetTypeID(localizationName) == CFStringGetTypeID())
      {
	CFIndex length = CFStringGetLength(localizationName);

	if (length <= sizeof(buff) &&
	    CFStringGetCString(localizationName, buff, sizeof(buff), kCFStringEncodingASCII))
	{
	  buff[sizeof(buff) - 1] = '\0';

	  for (i = 0;
	       i < sizeof(apple_name_locale) / sizeof(apple_name_locale[0]);
	       i++)
	  {
	    if (strcasecmp(buff, apple_name_locale[i].name) == 0)
	    {
	      language = nameToLocaleTable[i].locale;
	      break;
	    }
	  }
	}
      }

      CFRelease(localizationList);
    }
  
   /*
    * If we didn't find the language, default to en_US...
    */

    if (language == NULL)
      language = apple_name_locale[0].locale;
  }

 /*
  * Return the cached locale...
  */

  return (language);
}
#endif /* __APPLE__ */


/*
 * End of "$Id: language.c,v 1.20.2.13 2003/02/04 05:36:15 mike Exp $".
 */
