//
// "$Id: ppdc-catalog.cxx 11800 2014-04-08 19:53:57Z msweet $"
//
// Shared message catalog class for the CUPS PPD Compiler.
//
// Copyright 2007-2014 by Apple Inc.
// Copyright 2002-2006 by Easy Software Products.
//
// These coded instructions, statements, and computer programs are the
// property of Apple Inc. and are protected by Federal copyright
// law.  Distribution and use rights are outlined in the file "LICENSE.txt"
// which should have been included with this file.  If this file is
// file is missing or damaged, see the license at "http://www.cups.org/".
//

//
// Include necessary headers...
//

#include "ppdc-private.h"


//
// Character encodings...
//

typedef enum
{
  PPDC_CS_AUTO,
  PPDC_CS_UTF8,
  PPDC_CS_UTF16BE,
  PPDC_CS_UTF16LE
} ppdc_cs_t;


//
// Local functions...
//

#if defined(__APPLE__) && defined(CUPS_BUNDLEDIR)
static void	apple_add_message(CFStringRef key, CFStringRef val, ppdcCatalog *c);
#endif /* __APPLE__ && CUPS_BUNDLEDIR */
static int	get_utf8(char *&ptr);
static int	get_utf16(cups_file_t *fp, ppdc_cs_t &cs);
static int	put_utf8(int ch, char *&ptr, char *end);
static int	put_utf16(cups_file_t *fp, int ch);


//
// 'ppdcCatalog::ppdcCatalog()' - Create a shared message catalog.
//

ppdcCatalog::ppdcCatalog(const char *l,	// I - Locale
                         const char *f)	// I - Message catalog file
  : ppdcShared()
{
  PPDC_NEW;

  locale   = new ppdcString(l);
  filename = new ppdcString(f);
  messages = new ppdcArray();

  if (l)
  {
    // Try loading the base messages for this locale...
    char	pofile[1024];		// Message catalog file


#if defined(__APPLE__) && defined(CUPS_BUNDLEDIR)
    char		applelang[256];	// Apple language ID
    CFURLRef		url;		// URL to cups.strings file
    CFReadStreamRef	stream = NULL;	// File stream
    CFPropertyListRef	plist = NULL;	// Localization file

    snprintf(pofile, sizeof(pofile), CUPS_BUNDLEDIR "/Resources/%s.lproj/cups.strings", _cupsAppleLanguage(l, applelang, sizeof(applelang)));
    if (access(pofile, 0))
    {
      // Try alternate lproj directory names...
      const char *tl = l;		// Temporary locale string

      if (!strncmp(l, "en", 2))
	tl = "English";
      else if (!strncmp(l, "nb", 2) || !strncmp(l, "nl", 2))
	tl = "Dutch";
      else if (!strncmp(l, "fr", 2))
	tl = "French";
      else if (!strncmp(l, "de", 2))
	tl = "German";
      else if (!strncmp(l, "it", 2))
	tl = "Italian";
      else if (!strncmp(l, "ja", 2))
	tl = "Japanese";
      else if (!strncmp(l, "es", 2))
	tl = "Spanish";

      snprintf(pofile, sizeof(pofile), CUPS_BUNDLEDIR "/Resources/%s.lproj/cups.strings", tl);
    }

    url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (UInt8 *)pofile, (CFIndex)strlen(pofile), false);
    if (url)
    {
      stream = CFReadStreamCreateWithFile(kCFAllocatorDefault, url);

      if (stream)
      {
       /*
	* Read the property list containing the localization data.
	*/

	CFReadStreamOpen(stream);

	plist = CFPropertyListCreateWithStream(kCFAllocatorDefault, stream, 0, kCFPropertyListImmutable, NULL, NULL);

	if (plist && CFGetTypeID(plist) == CFDictionaryGetTypeID())
	  CFDictionaryApplyFunction((CFDictionaryRef)plist, (CFDictionaryApplierFunction)apple_add_message, this);

	if (plist)
	  CFRelease(plist);

	CFRelease(stream);
      }

      CFRelease(url);
    }

#else
    _cups_globals_t	*cg = _cupsGlobals();
					// Global information

    snprintf(pofile, sizeof(pofile), "%s/%s/cups_%s.po", cg->localedir, l, l);

    if (load_messages(pofile) && strchr(l, '_'))
    {
      // Try the base locale...
      char	baseloc[3];		// Base locale...


      strlcpy(baseloc, l, sizeof(baseloc));
      snprintf(pofile, sizeof(pofile), "%s/%s/cups_%s.po", cg->localedir,
               baseloc, baseloc);

      load_messages(pofile);
    }
#endif /* __APPLE__ && CUPS_BUNDLEDIR */
  }

  if (f && *f)
    load_messages(f);
}


//
// 'ppdcCatalog::~ppdcCatalog()' - Destroy a shared message catalog.
//

ppdcCatalog::~ppdcCatalog()
{
  PPDC_DELETE;

  locale->release();
  filename->release();
  messages->release();
}


//
// 'ppdcCatalog::add_message()' - Add a new message.
//

void
ppdcCatalog::add_message(
    const char *id,			// I - Message ID to add
    const char *string)			// I - Translation string
{
  ppdcMessage	*m;			// Current message
  char		text[1024];		// Text to translate


  // Range check input...
  if (!id)
    return;

  // Verify that we don't already have the message ID...
  for (m = (ppdcMessage *)messages->first();
       m;
       m = (ppdcMessage *)messages->next())
    if (!strcmp(m->id->value, id))
    {
      if (string)
      {
        m->string->release();
	m->string = new ppdcString(string);
      }
      return;
    }

  // Add the message...
  if (!string)
  {
    snprintf(text, sizeof(text), "TRANSLATE %s", id);
    string = text;
  }

  messages->add(new ppdcMessage(id, string));
}


//
// 'ppdcCatalog::find_message()' - Find a message in a catalog...
//

const char *				// O - Message text
ppdcCatalog::find_message(
    const char *id)			// I - Message ID
{
  ppdcMessage	*m;			// Current message


  if (!*id)
    return (id);

  for (m = (ppdcMessage *)messages->first();
       m;
       m = (ppdcMessage *)messages->next())
    if (!strcmp(m->id->value, id))
      return (m->string->value);

  return (id);
}


//
// 'ppdcCatalog::load_messages()' - Load messages from a .po file.
//

int					// O - 0 on success, -1 on failure
ppdcCatalog::load_messages(
    const char *f)			// I - Message catalog file
{
  cups_file_t	*fp;			// Message file
  char		line[4096],		// Line buffer
		*ptr,			// Pointer into buffer
		id[4096],		// Translation ID
		str[4096];		// Translation string
  int		linenum;		// Line number


  // Open the message catalog file...
  if ((fp = cupsFileOpen(f, "r")) == NULL)
    return (-1);

  if ((ptr = (char *)strrchr(f, '.')) == NULL)
    goto unknown_load_format;
  else if (!strcmp(ptr, ".strings"))
  {
   /*
    * Read messages in OS X ".strings" format, which are either UTF-8/UTF-16
    * text files of the format:
    *
    *     "id" = "str";
    *
    * Strings files can also contain C-style comments.
    */

    ppdc_cs_t	cs = PPDC_CS_AUTO;	// Character set for file
    int		ch;			// Current character from file
    char	*end;			// End of buffer


    id[0]  = '\0';
    str[0] = '\0';
    ptr    = NULL;
    end    = NULL;

    while ((ch = get_utf16(fp, cs)) != 0)
    {
      if (ptr)
      {
        if (ch == '\\')
	{
	  if ((ch = get_utf16(fp, cs)) == 0)
	    break;

	  if (ch == 'n')
	    ch = '\n';
	  else if (ch == 't')
	    ch = '\t';
        }
	else if (ch == '\"')
	{
	  *ptr = '\0';
	  ptr  = NULL;
	}

        if (ptr)
	  put_utf8(ch, ptr, end);
      }
      else if (ch == '/')
      {
        // Start of a comment?
	if ((ch = get_utf16(fp, cs)) == 0)
	  break;

        if (ch == '*')
	{
	  // Skip C comment...
	  int lastch = 0;

          while ((ch = get_utf16(fp, cs)) != 0)
	  {
	    if (ch == '/' && lastch == '*')
	      break;

	    lastch = ch;
	  }
	}
	else if (ch == '/')
	{
	  // Skip C++ comment...
	  while ((ch = get_utf16(fp, cs)) != 0)
	    if (ch == '\n')
	      break;
	}
      }
      else if (ch == '\"')
      {
        // Start quoted string...
	if (id[0])
	{
	  ptr = str;
	  end = str + sizeof(str) - 1;
	}
	else
	{
	  ptr = id;
	  end = id + sizeof(id) - 1;
	}
      }
      else if (ch == ';')
      {
        // Add string...
	add_message(id, str);
	id[0] = '\0';
      }
    }
  }
  else if (!strcmp(ptr, ".po") || !strcmp(ptr, ".gz"))
  {
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

    int	which,				// In msgid?
	haveid,				// Did we get a msgid string?
	havestr;			// Did we get a msgstr string?

    linenum = 0;
    id[0]   = '\0';
    str[0]  = '\0';
    haveid  = 0;
    havestr = 0;
    which   = 0;

    while (cupsFileGets(fp, line, sizeof(line)))
    {
      linenum ++;

      // Skip blank and comment lines...
      if (line[0] == '#' || !line[0])
	continue;

      // Strip the trailing quote...
      if ((ptr = (char *)strrchr(line, '\"')) == NULL)
      {
	_cupsLangPrintf(stderr,
	                _("ppdc: Expected quoted string on line %d of %s."),
			linenum, f);
	cupsFileClose(fp);
	return (-1);
      }

      *ptr = '\0';

      // Find start of value...
      if ((ptr = strchr(line, '\"')) == NULL)
      {
	_cupsLangPrintf(stderr,
	                _("ppdc: Expected quoted string on line %d of %s."),
			linenum, f);
	cupsFileClose(fp);
	return (-1);
      }

      ptr ++;

      // Unquote the text...
      char *sptr, *dptr;			// Source/destination pointers

      for (sptr = ptr, dptr = ptr; *sptr;)
      {
	if (*sptr == '\\')
	{
	  sptr ++;
	  if (isdigit(*sptr))
	  {
	    *dptr = 0;

	    while (isdigit(*sptr))
	    {
	      *dptr = *dptr * 8 + *sptr - '0';
	      sptr ++;
	    }

	    dptr ++;
	  }
	  else
	  {
	    if (*sptr == 'n')
	      *dptr++ = '\n';
	    else if (*sptr == 'r')
	      *dptr++ = '\r';
	    else if (*sptr == 't')
	      *dptr++ = '\t';
	    else
	      *dptr++ = *sptr;

	    sptr ++;
	  }
	}
	else
	  *dptr++ = *sptr++;
      }

      *dptr = '\0';

      // Create or add to a message...
      if (!strncmp(line, "msgid", 5))
      {
	if (haveid && havestr)
	  add_message(id, str);

	strlcpy(id, ptr, sizeof(id));
	str[0] = '\0';
	haveid  = 1;
	havestr = 0;
	which   = 1;
      }
      else if (!strncmp(line, "msgstr", 6))
      {
	if (!haveid)
	{
	  _cupsLangPrintf(stderr,
	                  _("ppdc: Need a msgid line before any "
			    "translation strings on line %d of %s."),
			  linenum, f);
	  cupsFileClose(fp);
	  return (-1);
	}

	strlcpy(str, ptr, sizeof(str));
	havestr = 1;
	which   = 2;
      }
      else if (line[0] == '\"' && which == 2)
	strlcat(str, ptr, sizeof(str));
      else if (line[0] == '\"' && which == 1)
	strlcat(id, ptr, sizeof(id));
      else
      {
	_cupsLangPrintf(stderr, _("ppdc: Unexpected text on line %d of %s."),
			linenum, f);
	cupsFileClose(fp);
	return (-1);
      }
    }

    if (haveid && havestr)
      add_message(id, str);
  }
  else
    goto unknown_load_format;

 /*
  * Close the file and return...
  */

  cupsFileClose(fp);

  return (0);

 /*
  * Unknown format error...
  */

  unknown_load_format:

  _cupsLangPrintf(stderr,
                  _("ppdc: Unknown message catalog format for \"%s\"."), f);
  cupsFileClose(fp);
  return (-1);
}


//
// 'ppdcCatalog::save_messages()' - Save the messages to a .po file.
//

int					// O - 0 on success, -1 on error
ppdcCatalog::save_messages(
    const char *f)			// I - File to save to
{
  cups_file_t	*fp;			// Message file
  ppdcMessage	*m;			// Current message
  char		*ptr;			// Pointer into string
  int		utf16;			// Output UTF-16 .strings file?
  int		ch;			// Current character


  // Open the file...
  if ((ptr = (char *)strrchr(f, '.')) == NULL)
    return (-1);

  if (!strcmp(ptr, ".gz"))
    fp = cupsFileOpen(f, "w9");
  else
    fp = cupsFileOpen(f, "w");

  if (!fp)
    return (-1);

  // For .strings files, write a BOM for big-endian output...
  utf16 = !strcmp(ptr, ".strings");

  if (utf16)
    put_utf16(fp, 0xfeff);

  // Loop through all of the messages...
  for (m = (ppdcMessage *)messages->first();
       m;
       m = (ppdcMessage *)messages->next())
  {
    if (utf16)
    {
      put_utf16(fp, '\"');

      ptr = m->id->value;
      while ((ch = get_utf8(ptr)) != 0)
	switch (ch)
	{
	  case '\n' :
	      put_utf16(fp, '\\');
	      put_utf16(fp, 'n');
	      break;
	  case '\\' :
	      put_utf16(fp, '\\');
	      put_utf16(fp, '\\');
	      break;
	  case '\"' :
	      put_utf16(fp, '\\');
	      put_utf16(fp, '\"');
	      break;
	  default :
	      put_utf16(fp, ch);
	      break;
	}

      put_utf16(fp, '\"');
      put_utf16(fp, ' ');
      put_utf16(fp, '=');
      put_utf16(fp, ' ');
      put_utf16(fp, '\"');

      ptr = m->string->value;
      while ((ch = get_utf8(ptr)) != 0)
	switch (ch)
	{
	  case '\n' :
	      put_utf16(fp, '\\');
	      put_utf16(fp, 'n');
	      break;
	  case '\\' :
	      put_utf16(fp, '\\');
	      put_utf16(fp, '\\');
	      break;
	  case '\"' :
	      put_utf16(fp, '\\');
	      put_utf16(fp, '\"');
	      break;
	  default :
	      put_utf16(fp, ch);
	      break;
	}

      put_utf16(fp, '\"');
      put_utf16(fp, ';');
      put_utf16(fp, '\n');
    }
    else
    {
      cupsFilePuts(fp, "msgid \"");
      for (ptr = m->id->value; *ptr; ptr ++)
	switch (*ptr)
	{
	  case '\n' :
	      cupsFilePuts(fp, "\\n");
	      break;
	  case '\\' :
	      cupsFilePuts(fp, "\\\\");
	      break;
	  case '\"' :
	      cupsFilePuts(fp, "\\\"");
	      break;
	  default :
	      cupsFilePutChar(fp, *ptr);
	      break;
	}
      cupsFilePuts(fp, "\"\n");

      cupsFilePuts(fp, "msgstr \"");
      for (ptr = m->string->value; *ptr; ptr ++)
	switch (*ptr)
	{
	  case '\n' :
	      cupsFilePuts(fp, "\\n");
	      break;
	  case '\\' :
	      cupsFilePuts(fp, "\\\\");
	      break;
	  case '\"' :
	      cupsFilePuts(fp, "\\\"");
	      break;
	  default :
	      cupsFilePutChar(fp, *ptr);
	      break;
	}
      cupsFilePuts(fp, "\"\n");

      cupsFilePutChar(fp, '\n');
    }
  }

  cupsFileClose(fp);

  return (0);
}


#if defined(__APPLE__) && defined(CUPS_BUNDLEDIR)
//
// 'apple_add_message()' - Add a message from a localization dictionary.
//

static void
apple_add_message(CFStringRef key,	// I - Localization key
                  CFStringRef val,	// I - Localized value
                  ppdcCatalog *c)	// I - Message catalog
{
  char	id[1024],			// Message id
	str[1024];			// Localized message


  if (CFStringGetCString(key, id, sizeof(id), kCFStringEncodingUTF8) &&
      CFStringGetCString(val, str, sizeof(str), kCFStringEncodingUTF8))
    c->add_message(id, str);
}
#endif /* __APPLE__ && CUPS_BUNDLEDIR */


//
// 'get_utf8()' - Get a UTF-8 character.
//

static int				// O  - Unicode character or 0 on EOF
get_utf8(char *&ptr)			// IO - Pointer to character
{
  int	ch;				// Current character


  if ((ch = *ptr++ & 255) < 0xc0)
    return (ch);

  if ((ch & 0xe0) == 0xc0)
  {
    // Two-byte UTF-8...
    if ((*ptr & 0xc0) != 0x80)
      return (0);

    ch = ((ch & 0x1f) << 6) | (*ptr++ & 0x3f);
  }
  else if ((ch & 0xf0) == 0xe0)
  {
    // Three-byte UTF-8...
    if ((*ptr & 0xc0) != 0x80)
      return (0);

    ch = ((ch & 0x0f) << 6) | (*ptr++ & 0x3f);

    if ((*ptr & 0xc0) != 0x80)
      return (0);

    ch = (ch << 6) | (*ptr++ & 0x3f);
  }
  else if ((ch & 0xf8) == 0xf0)
  {
    // Four-byte UTF-8...
    if ((*ptr & 0xc0) != 0x80)
      return (0);

    ch = ((ch & 0x07) << 6) | (*ptr++ & 0x3f);

    if ((*ptr & 0xc0) != 0x80)
      return (0);

    ch = (ch << 6) | (*ptr++ & 0x3f);

    if ((*ptr & 0xc0) != 0x80)
      return (0);

    ch = (ch << 6) | (*ptr++ & 0x3f);
  }

  return (ch);
}


//
// 'get_utf16()' - Get a UTF-16 character...
//

static int				// O  - Unicode character or 0 on EOF
get_utf16(cups_file_t *fp,		// I  - File to read from
          ppdc_cs_t   &cs)		// IO - Character set of file
{
  int		ch;			// Current character
  unsigned char	buffer[3];		// Bytes


  if (cs == PPDC_CS_AUTO)
  {
    // Get byte-order-mark, if present...
    if (cupsFileRead(fp, (char *)buffer, 2) != 2)
      return (0);

    if (buffer[0] == 0xfe && buffer[1] == 0xff)
    {
      // Big-endian UTF-16...
      cs = PPDC_CS_UTF16BE;

      if (cupsFileRead(fp, (char *)buffer, 2) != 2)
	return (0);
    }
    else if (buffer[0] == 0xff && buffer[1] == 0xfe)
    {
      // Little-endian UTF-16...
      cs = PPDC_CS_UTF16LE;

      if (cupsFileRead(fp, (char *)buffer, 2) != 2)
	return (0);
    }
    else if (buffer[0] == 0x00 && buffer[1] != 0x00)
    {
      // No BOM, assume big-endian UTF-16...
      cs = PPDC_CS_UTF16BE;
    }
    else if (buffer[0] != 0x00 && buffer[1] == 0x00)
    {
      // No BOM, assume little-endian UTF-16...
      cs = PPDC_CS_UTF16LE;
    }
    else
    {
      // No BOM, assume UTF-8...
      cs = PPDC_CS_UTF8;

      cupsFileRewind(fp);
    }
  }
  else if (cs != PPDC_CS_UTF8)
  {
    if (cupsFileRead(fp, (char *)buffer, 2) != 2)
      return (0);
  }

  if (cs == PPDC_CS_UTF8)
  {
    // UTF-8 character...
    if ((ch = cupsFileGetChar(fp)) < 0)
      return (0);

    if ((ch & 0xe0) == 0xc0)
    {
      // Two-byte UTF-8...
      if (cupsFileRead(fp, (char *)buffer, 1) != 1)
        return (0);

      if ((buffer[0] & 0xc0) != 0x80)
        return (0);

      ch = ((ch & 0x1f) << 6) | (buffer[0] & 0x3f);
    }
    else if ((ch & 0xf0) == 0xe0)
    {
      // Three-byte UTF-8...
      if (cupsFileRead(fp, (char *)buffer, 2) != 2)
        return (0);

      if ((buffer[0] & 0xc0) != 0x80 ||
          (buffer[1] & 0xc0) != 0x80)
        return (0);

      ch = ((((ch & 0x0f) << 6) | (buffer[0] & 0x3f)) << 6) |
           (buffer[1] & 0x3f);
    }
    else if ((ch & 0xf8) == 0xf0)
    {
      // Four-byte UTF-8...
      if (cupsFileRead(fp, (char *)buffer, 3) != 3)
        return (0);

      if ((buffer[0] & 0xc0) != 0x80 ||
          (buffer[1] & 0xc0) != 0x80 ||
          (buffer[2] & 0xc0) != 0x80)
        return (0);

      ch = ((((((ch & 0x07) << 6) | (buffer[0] & 0x3f)) << 6) |
             (buffer[1] & 0x3f)) << 6) | (buffer[2] & 0x3f);
    }
  }
  else
  {
    // UTF-16 character...
    if (cs == PPDC_CS_UTF16BE)
      ch = (buffer[0] << 8) | buffer[1];
    else
      ch = (buffer[1] << 8) | buffer[0];

    if (ch >= 0xd800 && ch <= 0xdbff)
    {
      // Handle multi-word encoding...
      int lch;

      if (cupsFileRead(fp, (char *)buffer, 2) != 2)
        return (0);

      if (cs == PPDC_CS_UTF16BE)
	lch = (buffer[0] << 8) | buffer[1];
      else
	lch = (buffer[1] << 8) | buffer[0];

      if (lch < 0xdc00 || lch >= 0xdfff)
        return (0);

      ch = (((ch & 0x3ff) << 10) | (lch & 0x3ff)) + 0x10000;
    }
  }

  return (ch);
}


//
// 'put_utf8()' - Add a UTF-8 character to a string.
//

static int				// O  - 0 on success, -1 on failure
put_utf8(int  ch,			// I  - Unicode character
         char *&ptr,			// IO - String pointer
	 char *end)			// I  - End of buffer
{
  if (ch < 0x80)
  {
    // One-byte ASCII...
    if (ptr >= end)
      return (-1);

    *ptr++ = (char)ch;
  }
  else if (ch < 0x800)
  {
    // Two-byte UTF-8...
    if ((ptr + 1) >= end)
      return (-1);

    *ptr++ = (char)(0xc0 | (ch >> 6));
    *ptr++ = (char)(0x80 | (ch & 0x3f));
  }
  else if (ch < 0x10000)
  {
    // Three-byte UTF-8...
    if ((ptr + 2) >= end)
      return (-1);

    *ptr++ = (char)(0xe0 | (ch >> 12));
    *ptr++ = (char)(0x80 | ((ch >> 6) & 0x3f));
    *ptr++ = (char)(0x80 | (ch & 0x3f));
  }
  else
  {
    // Four-byte UTF-8...
    if ((ptr + 3) >= end)
      return (-1);

    *ptr++ = (char)(0xf0 | (ch >> 18));
    *ptr++ = (char)(0x80 | ((ch >> 12) & 0x3f));
    *ptr++ = (char)(0x80 | ((ch >> 6) & 0x3f));
    *ptr++ = (char)(0x80 | (ch & 0x3f));
  }

  return (0);
}


//
// 'put_utf16()' - Write a UTF-16 character to a file.
//

static int				// O - 0 on success, -1 on failure
put_utf16(cups_file_t *fp,		// I - File to write to
          int         ch)		// I - Unicode character
{
  unsigned char	buffer[4];		// Output buffer


  if (ch < 0x10000)
  {
    // One-word UTF-16 big-endian...
    buffer[0] = (unsigned char)(ch >> 8);
    buffer[1] = (unsigned char)ch;

    if (cupsFileWrite(fp, (char *)buffer, 2) == 2)
      return (0);
  }
  else
  {
    // Two-word UTF-16 big-endian...
    ch -= 0x10000;

    buffer[0] = (unsigned char)(0xd8 | (ch >> 18));
    buffer[1] = (unsigned char)(ch >> 10);
    buffer[2] = (unsigned char)(0xdc | ((ch >> 8) & 0x03));
    buffer[3] = (unsigned char)ch;

    if (cupsFileWrite(fp, (char *)buffer, 4) == 4)
      return (0);
  }

  return (-1);
}


//
// End of "$Id: ppdc-catalog.cxx 11800 2014-04-08 19:53:57Z msweet $".
//
