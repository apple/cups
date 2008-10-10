//
// "$Id$"
//
//   PPD to HTML utility for the CUPS PPD Compiler.
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
//   main()  - Main entry for the PPD to HTML utility.
//   usage() - Show usage and exit.
//

//
// Include necessary headers...
//

#include "ppdc.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <cups/i18n.h>


//
// Local functions...
//

static void	usage(void);


//
// 'main()' - Main entry for the PPD compiler.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int		i;			// Looping var
  ppdcSource	*src;			// PPD source file data
  ppdcDriver	*d;			// Current driver
  ppdcGroup	*g,			// Current group
		*composite;		// Composite of all drivers
  ppdcOption	*o,			// Current option
		*compo;			// Composite option
  ppdcChoice	*c;			// Current choice
  char		*opt;			// Current option char
  ppdcMediaSize	*size;			// Current media size
  char		*value;			// Value in option


  _cupsSetLocale(argv);

  // Scan the command-line...
  src = 0;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
        switch (*opt)
	{
          case 'D' :			// Define variable
	      i ++;
	      if (i >= argc)
	        usage();

              if ((value = strchr(argv[i], '=')) != NULL)
	      {
	        *value++ = '\0';

	        src->set_variable(argv[i], value);
	      }
	      else
	        src->set_variable(argv[i], "1");
              break;

          case 'I' :			// Include directory...
	      i ++;
	      if (i >= argc)
        	usage();

	      ppdcSource::add_include(argv[i]);
	      break;

	  default :			// Unknown
	      usage();
	      break;
	}
    }
    else
    {
      // Open and load the driver info file...
      src = new ppdcSource(argv[i]);

      // Create a composite group with all of the features from the
      // drivers in the info file...
      composite = new ppdcGroup("", "");

      for (d = (ppdcDriver *)src->drivers->first(); d; d = (ppdcDriver *)src->drivers->next())
        for (g = (ppdcGroup *)d->groups->first(); g; g = (ppdcGroup *)d->groups->next())
	  for (o = (ppdcOption *)g->options->first(); o; o = (ppdcOption *)g->options->next())
	  {
	    if ((compo = composite->find_option(o->name->value)) == NULL)
	      composite->add_option(new ppdcOption(o));
	  }

      puts("<html>");
      printf("<head><title>Driver Summary for %s</title></head>\n", argv[i]);
      printf("<body><h1>Driver Summary for %s</h1>\n", argv[i]);
      printf("<p><table border='1'><thead><tr><th>Printer</th><th>Media Size</th>");
      for (compo = (ppdcOption *)composite->options->first(); compo; compo = (ppdcOption *)composite->options->next())
        printf("<th>%s</th>", compo->text->value);
      puts("</tr></thead><tbody>");

      // Write HTML summary...
      for (d = (ppdcDriver *)src->drivers->first(); d; d = (ppdcDriver *)src->drivers->next())
      {
        // Write the summary for this driver...
	printf("<tr valign='top'><td nowrap>%s</td><td nowrap>", d->model_name->value);
	for (size = (ppdcMediaSize *)d->sizes->first(); size;
	     size = (ppdcMediaSize *)d->sizes->next())
          printf("%s<br>", size->text->value);
        printf("</td>");

        for (compo = (ppdcOption *)composite->options->first(); compo;
	     compo = (ppdcOption *)composite->options->next())
	  if ((o = d->find_option(compo->name->value)) != NULL)
	  {
	    printf("<td nowrap>");
	    for (c = (ppdcChoice *)o->choices->first(); c;
	         c = (ppdcChoice *)o->choices->next())
	      printf("%s<br>", c->text->value);
	    printf("</td>");
	  }
	  else
	    printf("<td>N/A</td>");

        puts("</tr>");
      }

      puts("</tbody></table></p>");
      puts("</body>");
      puts("</html>");
      // Delete the printer driver information...
      delete composite;
      delete src;
    }

  // If no drivers have been loaded, display the program usage message.
  if (!src)
    usage();

  // Return with no errors.
  return (0);
}


//
// 'usage()' - Show usage and exit.
//

static void
usage(void)
{
  _cupsLangPuts(stdout,
                _("Usage: ppdhtml [options] filename.drv >filename.html\n"
		  "  -D name=value        Set named variable to value.\n"
		  "Options:\n"
		  "  -I include-dir    Add include directory to search "
		  "path.\n"));

  exit(1);
}


//
// End of "$Id$".
//
