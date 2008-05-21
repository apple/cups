//
// "$Id$"
//
//   PPD file message catalog program for the CUPS PPD Compiler.
//
//   Copyright 2007-2008 by Apple Inc.
//   Copyright 2002-2005 by Easy Software Products.
//
//   These coded instructions, statements, and computer programs are the
//   property of Apple Inc. and are protected by Federal copyright
//   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
//   which should have been included with this file.  If this file is
//   file is missing or damaged, see the license at "http://www.cups.org/".
//
// Contents:
//
//   main()           - Main entry for the PPD compiler.
//   add_ui_strings() - Add all UI strings from the driver.
//   usage()          - Show usage and exit.
//

//
// Include necessary headers...
//

#include "ppdc.h"
#include <sys/stat.h>
#include <sys/types.h>


//
// Local functions...
//

static void	add_ui_strings(ppdcDriver *d, ppdcCatalog *catalog);
static void	usage(void);


//
// 'main()' - Main entry for the PPD compiler.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int		i;			// Looping var
  ppdcCatalog	*catalog;		// Message catalog
  ppdcSource	*src;			// PPD source file data
  ppdcDriver	*d;			// Current driver
  char		*opt;			// Current option
  int		verbose;		// Verbosity
  const char	*outfile;		// Output file


  // Scan the command-line...
  catalog = new ppdcCatalog("en");
  src     = 0;
  verbose = 0;
  outfile = 0;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
        switch (*opt)
	{
          case 'I' :			// Include directory...
	      i ++;
	      if (i >= argc)
        	usage();

              if (verbose > 1)
	        printf("ppdc: Adding include directory \"%s\"...\n", argv[i]);

	      ppdcSource::add_include(argv[i]);
	      break;

          case 'o' :			// Output file...
	      i ++;
	      if (i >= argc || outfile)
        	usage();

              outfile = argv[i];

	      catalog->load_messages(outfile);
	      break;

          case 'v' :			// Be verbose...
	      verbose ++;
	      break;

	  default :			// Unknown
	      usage();
	      break;
	}
    }
    else
    {
      // Open and load the driver info file...
      if (verbose > 1)
        printf("ppdc: Loading driver information file \"%s\"...\n", argv[i]);

      src = new ppdcSource(argv[i]);

      // Add UI strings...
      for (d = (ppdcDriver *)src->drivers->first();
           d;
	   d = (ppdcDriver *)src->drivers->next())
      {
	if (verbose)
	  printf("ppdc: Adding/updating UI text from %s...\n", argv[i]);

        add_ui_strings(d, catalog);
      }

      // Delete the printer driver information...
      delete src;
    }

  // Write the message catalog...
  if (!outfile)
    usage();
  else
    catalog->save_messages(outfile);

  delete catalog;

  // If no drivers have been loaded, display the program usage message.
  if (!src)
    usage();

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

  // Add the media size strings...
  ppdcMediaSize	*m;			// Current media size

  for (m = (ppdcMediaSize *)d->sizes->first();
       m;
       m = (ppdcMediaSize *)d->sizes->next())
    catalog->add_message(m->text->value);

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

    if (strcasecmp(g->name->value, "General"))
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


//
// 'usage()' - Show usage and exit.
//

static void
usage(void)
{
  puts("Usage: ppdpo [options] -o filename.po filename.drv [ ... filenameN.drv ]");
  puts("Options:");
  puts("  -I include-dir    Add include directory to search path.");
  puts("  -v                Be verbose (more v's for more verbosity).");

  exit(1);
}


//
// End of "$Id$".
//
