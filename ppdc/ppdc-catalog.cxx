//
// "$Id$"
//
//   Shared message catalog class for the CUPS PPD Compiler.
//
//   Copyright 2007-2008 by Apple Inc.
//   Copyright 2002-2006 by Easy Software Products.
//
//   These coded instructions, statements, and computer programs are the
//   property of Apple Inc. and are protected by Federal copyright
//   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
//   which should have been included with this file.  If this file is
//   file is missing or damaged, see the license at "http://www.cups.org/".
//
// Contents:
//
//   ppdcCatalog::ppdcCatalog()   - Create a shared message catalog.
//   ppdcCatalog::~ppdcCatalog()  - Destroy a shared message catalog.
//   ppdcCatalog::add_message()   - Add a new message.
//   ppdcCatalog::find_message()  - Find a message in a catalog...
//   ppdcCatalog::load_messages() - Load messages from a .po file.
//   ppdcCatalog::save_messages() - Save the messages to a .po file.
//

//
// Include necessary headers...
//

#include "ppdc.h"
#include <cups/globals.h>


//
// 'ppdcCatalog::ppdcCatalog()' - Create a shared message catalog.
//

ppdcCatalog::ppdcCatalog(const char *l,	// I - Locale
                         const char *f)	// I - Message catalog file
  : ppdcShared()
{
  _cups_globals_t	*cg = _cupsGlobals();
					// Global information


  locale   = new ppdcString(l);
  filename = new ppdcString(f);
  messages = new ppdcArray();

  if (l)
  {
    // Try loading the base messages for this locale...
    char	pofile[1024];		// Message catalog file


    snprintf(pofile, sizeof(pofile), "%s/po/%s.po", cg->cups_datadir, l);

    if (load_messages(pofile) && strchr(l, '_'))
    {
      // Try the base locale...
      char	baseloc[3];		// Base locale...


      strlcpy(baseloc, l, sizeof(baseloc));
      snprintf(pofile, sizeof(pofile), "%s/po/%s.po", cg->cups_datadir,
               baseloc);

      load_messages(pofile);
    }
  }

  if (f)
    load_messages(f);
}


//
// 'ppdcCatalog::~ppdcCatalog()' - Destroy a shared message catalog.
//

ppdcCatalog::~ppdcCatalog()
{
  delete locale;
  delete filename;
  delete messages;
}


//
// 'ppdcCatalog::add_message()' - Add a new message.
//

void
ppdcCatalog::add_message(const char *id)// I - Message ID to add
{
  ppdcMessage	*m;			// Current message
  char		text[1024];		// Text to translate


  // Range check input...
  if (!id || !*id)
    return;

  // Verify that we don't already have the message ID...
  for (m = (ppdcMessage *)messages->first();
       m;
       m = (ppdcMessage *)messages->next())
    if (!strcmp(m->id->value, id))
      return;

  // Add the message...
  snprintf(text, sizeof(text), "TRANSLATE %s", id);
  messages->add(new ppdcMessage(id, text));
}


//
// 'ppdcCatalog::find_message()' - Find a message in a catalog...
//

const char *				// O - Message text
ppdcCatalog::find_message(
    const char *id)			// I - Message ID
{
  ppdcMessage	*m;			// Current message


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
  ppdcMessage	*temp;			// Current message
  char		line[4096],		// Line buffer
		*ptr,			// Pointer into buffer
		id[4096],		// Translation ID
		str[4096];		// Translation string
  int		linenum;		// Line number


  // Open the message catalog file...
  if ((fp = cupsFileOpen(f, "r")) == NULL)
    return (-1);

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

  linenum = 0;
  id[0]   = '\0';
  str[0]  = '\0';

  while (cupsFileGets(fp, line, sizeof(line)))
  {
    linenum ++;

    // Skip blank and comment lines...
    if (line[0] == '#' || !line[0])
      continue;

    // Strip the trailing quote...
    if ((ptr = strrchr(line, '\"')) == NULL)
    {
      fprintf(stderr, "load_messages: Expected quoted string on line %d of %s!\n",
              linenum, f);
      cupsFileClose(fp);
      return (-1);
    }

    *ptr = '\0';

    // Find start of value...
    if ((ptr = strchr(line, '\"')) == NULL)
    {
      fprintf(stderr, "load_messages: Expected quoted string on line %d of %s!\n",
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
      if (id[0] && str[0])
      {
	temp = new ppdcMessage(id, str);

	messages->add(temp);
      }

      strlcpy(id, ptr, sizeof(id));
      str[0] = '\0';
    }
    else if (!strncmp(line, "msgstr", 6))
    {
      if (!id[0])
      {
	fprintf(stderr, "load_messages: Need a msgid line before any "
                	"translation strings on line %d of %s!\n",
		linenum, f);
	cupsFileClose(fp);
	return (-1);
      }

      strlcpy(str, ptr, sizeof(str));
    }
    else if (line[0] == '\"' && str[0])
      strlcat(str, ptr, sizeof(str));
    else if (line[0] == '\"' && id[0])
      strlcat(id, ptr, sizeof(id));
    else
    {
      fprintf(stderr, "load_messages: Unexpected text on line %d of %s!\n",
              linenum, f);
      cupsFileClose(fp);
      return (-1);
    }
  }

  if (id[0] && str[0])
  {
    temp = new ppdcMessage(id, str);

    messages->add(temp);
  }

  cupsFileClose(fp);

  return (0);
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
  const char	*ptr;			// Pointer into string


  if ((fp = cupsFileOpen(f, "w")) == NULL)
    return (-1);

  for (m = (ppdcMessage *)messages->first();
       m;
       m = (ppdcMessage *)messages->next())
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

  cupsFileClose(fp);

  return (0);
}


//
// End of "$Id$".
//
