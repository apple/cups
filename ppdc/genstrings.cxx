//
// "$Id: genstrings.cxx 11800 2014-04-08 19:53:57Z msweet $"
//
// GNU gettext message generator for the CUPS PPD Compiler.
//
// This program is used to generate a dummy source file containing all of
// the standard media and sample driver strings.  The results are picked up
// by GNU gettext and placed in the CUPS message catalog.
//
// Copyright 2008-2014 by Apple Inc.
//
// These coded instructions, statements, and computer programs are the
// property of Apple Inc. and are protected by Federal copyright
// law.  Distribution and use rights are outlined in the file "LICENSE.txt"
// which should have been included with this file.  If this file is
// file is missing or damaged, see the license at "http://www.cups.org/".
//
// Usage:
//
//   ./genstrings >sample.c
//

//
// Include necessary headers...
//

#include "ppdc-private.h"
#include <unistd.h>


//
// Local functions...
//

static void	add_ui_strings(ppdcDriver *d, ppdcCatalog *catalog);
static void	write_cstring(const char *s);


//
// 'main()' - Main entry for the PPD compiler.
//

int					// O - Exit status
main(void)
{
  ppdcSource	*src;			// PPD source file data
  ppdcCatalog	*catalog;		// Catalog to hold all of the UI strings


  // Make sure we are in the right place...
  if (access("../data", 0) || access("sample.drv", 0))
  {
    puts("You must run genstrings from the ppdc directory.");
    return (1);
  }

  // Load the sample drivers...
  ppdcSource::add_include("../data");

  src     = new ppdcSource("sample.drv");
  catalog = new ppdcCatalog(NULL);

  catalog->add_message("ISOLatin1");
  catalog->add_message("English");

  // Add the media size strings...
  ppdcMediaSize	*size;			// Current media size

  for (size = (ppdcMediaSize *)src->sizes->first();
       size;
       size = (ppdcMediaSize *)src->sizes->next())
    catalog->add_message(size->text->value);

  // Then collect all of the UI strings from the sample drivers...
  ppdcDriver	*d;			// Current driver

  for (d = (ppdcDriver *)src->drivers->first();
       d;
       d = (ppdcDriver *)src->drivers->next())
    add_ui_strings(d, catalog);

  // Finally, write all of the strings...
  ppdcMessage *message;

  for (message = (ppdcMessage *)catalog->messages->first();
       message;
       message = (ppdcMessage *)catalog->messages->next())
    write_cstring(message->id->value);

  src->release();
  catalog->release();

  // Return with no errors.
  return (0);
}


//
// 'add_ui_strings()' - Add all UI strings from the driver.
//

static void
add_ui_strings(ppdcDriver  *d,		// I - Driver data
               ppdcCatalog *catalog)	// I - Message catalog
{
  // Add the make/model/language strings...
  catalog->add_message(d->manufacturer->value);
  catalog->add_message(d->model_name->value);

  // Add the group/option/choice strings...
  ppdcGroup	*g;			// Current group
  ppdcOption	*o;			// Current option
  ppdcChoice	*c;			// Current choice

  for (g = (ppdcGroup *)d->groups->first();
       g;
       g = (ppdcGroup *)d->groups->next())
  {
    if (!g->options->count)
      continue;

    if (_cups_strcasecmp(g->name->value, "General"))
      catalog->add_message(g->text->value);

    for (o = (ppdcOption *)g->options->first();
         o;
	 o = (ppdcOption *)g->options->next())
    {
      if (!o->choices->count)
        continue;

      if (o->text->value && strcmp(o->name->value, o->text->value))
        catalog->add_message(o->text->value);
      else
        catalog->add_message(o->name->value);

      for (c = (ppdcChoice *)o->choices->first();
           c;
	   c = (ppdcChoice *)o->choices->next())
	if (c->text->value && strcmp(c->name->value, c->text->value))
          catalog->add_message(c->text->value);
        else
          catalog->add_message(c->name->value);
    }
  }

  // Add profile and preset strings...
  ppdcAttr *a;				// Current attribute
  for (a = (ppdcAttr *)d->attrs->first();
       a;
       a = (ppdcAttr *)d->attrs->next())
  {
    if (a->text->value && a->text->value[0] &&
        (a->localizable ||
	 !strncmp(a->name->value, "Custom", 6) ||
         !strncmp(a->name->value, "ParamCustom", 11) ||
         !strcmp(a->name->value, "APCustomColorMatchingName") ||
         !strcmp(a->name->value, "APPrinterPreset") ||
         !strcmp(a->name->value, "cupsICCProfile") ||
         !strcmp(a->name->value, "cupsIPPReason") ||
         !strcmp(a->name->value, "cupsMarkerName")))
    {
      catalog->add_message(a->text->value);

      if ((a->localizable && a->value->value[0]) ||
          !strcmp(a->name->value, "cupsIPPReason"))
        catalog->add_message(a->value->value);
    }
    else if (!strncmp(a->name->value, "Custom", 6) ||
             !strncmp(a->name->value, "ParamCustom", 11))
      catalog->add_message(a->name->value);
  }
}


//
// 'write_cstring()' - Write a translation string as a valid C string to stdout.
//

static void
write_cstring(const char *s)		/* I - String to write */
{
  fputs("_(\"", stdout);
  if (s)
  {
    while (*s)
    {
      if (*s == '\\')
        fputs("\\\\", stdout);
      else if (*s == '\"')
        fputs("\\\"", stdout);
      else if (*s == '\t')
        fputs("\\t", stdout);
      else if (*s == '\n')
        fputs("\\n", stdout);
      else
        putchar(*s);

      s ++;
    }
  }
  puts("\");");
}


//
// End of "$Id: genstrings.cxx 11800 2014-04-08 19:53:57Z msweet $".
//
