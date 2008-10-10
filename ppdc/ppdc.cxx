//
// "$Id$"
//
//   PPD file compiler main entry for the CUPS PPD Compiler.
//
//   Copyright 2007-2008 by Apple Inc.
//   Copyright 2002-2007 by Easy Software Products.
//
//   These coded instructions, statements, and computer programs are the
//   property of Apple Inc. and are protected by Federal copyright
//   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
//   which should have been included with this file.  If this file is
//   file is missing or damaged, see the license at "http://www.cups.org/".
//
// Contents:
//
//   main()  - Main entry for the PPD compiler.
//   usage() - Show usage and exit.
//

//
// Include necessary headers...
//

#include "ppdc.h"
#include <unistd.h>
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
  int			i, j;		// Looping vars
  ppdcCatalog		*catalog;	// Message catalog
  const char		*outdir;	// Output directory
  ppdcSource		*src;		// PPD source file data
  ppdcDriver		*d;		// Current driver
  cups_file_t		*fp;		// PPD file
  char			*opt,		// Current option
			*value,		// Value in option
			*outname,	// Output filename
			pcfilename[1024],
					// Lowercase pcfilename
			filename[1024];	// PPD filename
  int			comp,		// Compress
			do_test,	// Test PPD files
			use_model_name,	// Use ModelName for filename
			verbose;	// Verbosity
  ppdcLineEnding	le;		// Line ending to use
  ppdcArray		*locales;	// List of locales


  _cupsSetLocale(argv);

  // Scan the command-line...
  catalog        = NULL;
  comp           = 0;
  do_test        = 0;
  le             = PPDC_LFONLY;
  locales        = NULL;
  outdir         = "ppd";
  src            = new ppdcSource();
  use_model_name = 0;
  verbose        = 0;

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

              if (verbose > 1)
	        _cupsLangPrintf(stdout,
				_("ppdc: Adding include directory \"%s\"...\n"),
				argv[i]);

	      ppdcSource::add_include(argv[i]);
	      break;

	  case 'c' :			// Message catalog...
	      i ++;
              if (i >= argc)
                usage();

              if (verbose > 1)
	        _cupsLangPrintf(stdout,
		                _("ppdc: Loading messages from \"%s\"...\n"),
				argv[i]);

              if (!catalog)
	        catalog = new ppdcCatalog("en");

              if (catalog->load_messages(argv[i]))
	      {
        	_cupsLangPrintf(stderr,
		                _("ppdc: Unable to load localization file "
				  "\"%s\" - %s\n"), argv[i], strerror(errno));
                return (1);
	      }
	      break;

          case 'd' :			// Output directory...
	      i ++;
	      if (i >= argc)
        	usage();

              if (verbose > 1)
	        _cupsLangPrintf(stdout,
				_("ppdc: Writing PPD files to directory "
				  "\"%s\"...\n"), argv[i]);

	      outdir = argv[i];
	      break;

          case 'l' :			// Language(s)...
	      i ++;
	      if (i >= argc)
        	usage();

              if (strchr(argv[i], ','))
	      {
	        // Comma-delimited list of languages...
		char	temp[1024],	// Copy of language list
			*start,		// Start of current locale name
			*end;		// End of current locale name


		locales = new ppdcArray();

		strlcpy(temp, argv[i], sizeof(temp));
		for (start = temp; *start; start = end)
		{
		  if ((end = strchr(start, ',')) != NULL)
		    *end++ = '\0';
		  else
		    end = start + strlen(start);

                  if (end > start)
		    locales->add(new ppdcString(start));
		}
	      }
	      else
	      {
        	if (verbose > 1)
	          _cupsLangPrintf(stdout,
		                  _("ppdc: Loading messages for locale "
				    "\"%s\"...\n"), argv[i]);

        	if (catalog)
	          delete catalog;

        	catalog = new ppdcCatalog(argv[i]);

		if (catalog->messages->count == 0)
		{
        	  _cupsLangPrintf(stderr,
				  _("ppdc: Unable to find localization for "
				    "\"%s\" - %s\n"), argv[i], strerror(errno));
                  return (1);
		}
	      }
	      break;

          case 'm' :			// Use ModelName for filename
	      use_model_name = 1;
	      break;

          case 't' :			// Test PPDs instead of generating them
	      do_test = 1;
	      break;

          case 'v' :			// Be verbose...
	      verbose ++;
	      break;
	    
          case 'z' :			// Compress files...
	      comp = 1;
	      break;

	  case '-' :			// --option
	      if (!strcmp(opt, "-lf"))
	      {
		le  = PPDC_LFONLY;
		opt += strlen(opt) - 1;
		break;
	      }
	      else if (!strcmp(opt, "-cr"))
	      {
		le  = PPDC_CRONLY;
		opt += strlen(opt) - 1;
		break;
	      }
	      else if (!strcmp(opt, "-crlf"))
	      {
		le  = PPDC_CRLF;
		opt += strlen(opt) - 1;
		break;
	      }
	    
	  default :			// Unknown
	      usage();
	      break;
	}
    }
    else
    {
      // Open and load the driver info file...
      if (verbose > 1)
        _cupsLangPrintf(stdout,
	                _("ppdc: Loading driver information file \"%s\"...\n"),
			argv[i]);

      src->read_file(argv[i]);
    }


  if (src->drivers->count > 0)
  {
    // Create the output directory...
    if (mkdir(outdir, 0777))
    {
      if (errno != EEXIST)
      {
	_cupsLangPrintf(stderr,
	                _("ppdc: Unable to create output directory %s: %s\n"),
	        outdir, strerror(errno));
        return (1);
      }
    }

    // Write PPD files...
    for (d = (ppdcDriver *)src->drivers->first();
         d;
	 d = (ppdcDriver *)src->drivers->next())
    {
      if (do_test)
      {
        // Test the PPD file for this driver...
	int	pid,			// Process ID
		fds[2];			// Pipe file descriptors


        if (pipe(fds))
	{
	  _cupsLangPrintf(stderr,
	                  _("ppdc: Unable to create output pipes: %s\n"),
	                  strerror(errno));
	  return (1);
	}

	if ((pid = fork()) == 0)
	{
	  // Child process comes here...
	  close(0);
	  dup(fds[0]);

	  close(fds[0]);
	  close(fds[1]);

	  execlp("cupstestppd", "cupstestppd", "-", (char *)0);

	  _cupsLangPrintf(stderr,
	                  _("ppdc: Unable to execute cupstestppd: %s\n"),
			  strerror(errno));
	  return (errno);
	}
	else if (pid < 0)
	{
	  _cupsLangPrintf(stderr,
	                  _("ppdc: Unable to execute cupstestppd: %s\n"),
			  strerror(errno));
	  return (errno);
	}

	close(fds[0]);
	fp = cupsFileOpenFd(fds[1], "w");
      }
      else
      {
	// Write the PPD file for this driver...
	if (use_model_name)
	  outname = d->model_name->value;
	else if (d->file_name)
	  outname = d->file_name->value;
	else
	  outname = d->pc_file_name->value;

	if (strstr(outname, ".PPD"))
	{
	  // Convert PCFileName to lowercase...
	  for (j = 0;
	       outname[j] && j < (int)(sizeof(pcfilename) - 1);
	       j ++)
	    pcfilename[j] = tolower(outname[j] & 255);

	  pcfilename[j] = '\0';
	}
	else
	{
	  // Leave PCFileName as-is...
	  strlcpy(pcfilename, outname, sizeof(pcfilename));
	}

	// Open the PPD file for writing...
	if (comp)
	  snprintf(filename, sizeof(filename), "%s/%s.gz", outdir, pcfilename);
	else
	  snprintf(filename, sizeof(filename), "%s/%s", outdir, pcfilename);

	fp = cupsFileOpen(filename, comp ? "w9" : "w");
	if (!fp)
	{
	  _cupsLangPrintf(stderr,
	                  _("ppdc: Unable to create PPD file \"%s\" - %s.\n"),
			  filename, strerror(errno));
	  return (1);
	}

	if (verbose)
	  _cupsLangPrintf(stdout, _("ppdc: Writing %s...\n"), filename);
      }

     /*
      * Write the PPD file...
      */

      if (d->write_ppd_file(fp, catalog, locales, src, le))
      {
	cupsFileClose(fp);
	return (1);
      }

      cupsFileClose(fp);
    }
  }
  else
    usage();

  // Delete the printer driver information...
  delete src;

  // Message catalog...
  if (catalog)
    delete catalog;

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
                _("Usage: ppdc [options] filename.drv [ ... filenameN.drv ]\n"
		  "Options:\n"
		  "  -D name=value        Set named variable to value.\n"
		  "  -I include-dir       Add include directory to search "
		  "path.\n"
		  "  -c catalog.po        Load the specified message catalog.\n"
		  "  -d output-dir        Specify the output directory.\n"
		  "  -l lang[,lang,...]   Specify the output language(s) "
		  "(locale).\n"
		  "  -m                   Use the ModelName value as the "
		  "filename.\n"
		  "  -t                   Test PPDs instead of generating "
		  "them.\n"
		  "  -v                   Be verbose (more v's for more "
		  "verbosity).\n"
		  "  -z                   Compress PPD files using GNU zip.\n"
		  "  --cr                 End lines with CR (Mac OS 9).\n"
		  "  --crlf               End lines with CR + LF (Windows).\n"
		  "  --lf                 End lines with LF (UNIX/Linux/Mac "
		  "OS X).\n"));

  exit(1);
}


//
// End of "$Id$".
//
