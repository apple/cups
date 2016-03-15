//
// "$Id: ppdc.cxx 11800 2014-04-08 19:53:57Z msweet $"
//
// PPD file compiler main entry for the CUPS PPD Compiler.
//
// Copyright 2007-2014 by Apple Inc.
// Copyright 2002-2007 by Easy Software Products.
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
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>


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
			make_model[1024],
					// Make and model
			pcfilename[1024],
					// Lowercase pcfilename
			filename[1024];	// PPD filename
  int			comp,		// Compress
			do_test,	// Test PPD files
			single_language,// Generate single-language files
			use_model_name,	// Use ModelName for filename
			verbose;	// Verbosity
  ppdcLineEnding	le;		// Line ending to use
  ppdcArray		*locales;	// List of locales
  cups_array_t		*filenames;	// List of generated filenames


  _cupsSetLocale(argv);

  // Scan the command-line...
  catalog         = NULL;
  comp            = 0;
  do_test         = 0;
  le              = PPDC_LFONLY;
  locales         = NULL;
  outdir          = "ppd";
  single_language = 0;
  src             = new ppdcSource();
  use_model_name  = 0;
  verbose         = 0;
  filenames       = cupsArrayNew((cups_array_func_t)_cups_strcasecmp, NULL);

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
				_("ppdc: Adding include directory \"%s\"."),
				argv[i]);

	      ppdcSource::add_include(argv[i]);
	      break;

	  case 'c' :			// Message catalog...
	      i ++;
              if (i >= argc)
                usage();

              if (verbose > 1)
	        _cupsLangPrintf(stdout,
		                _("ppdc: Loading messages from \"%s\"."),
				argv[i]);

              if (!catalog)
	        catalog = new ppdcCatalog("en");

              if (catalog->load_messages(argv[i]))
	      {
        	_cupsLangPrintf(stderr,
		                _("ppdc: Unable to load localization file "
				  "\"%s\" - %s"), argv[i], strerror(errno));
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
				  "\"%s\"."), argv[i]);

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
	        single_language = 1;

        	if (verbose > 1)
	          _cupsLangPrintf(stdout,
		                  _("ppdc: Loading messages for locale "
				    "\"%s\"."), argv[i]);

        	if (catalog)
	          catalog->release();

        	catalog = new ppdcCatalog(argv[i]);

		if (catalog->messages->count == 0 && strcmp(argv[i], "en"))
		{
        	  _cupsLangPrintf(stderr,
				  _("ppdc: Unable to find localization for "
				    "\"%s\" - %s"), argv[i], strerror(errno));
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
	                _("ppdc: Loading driver information file \"%s\"."),
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
	                _("ppdc: Unable to create output directory %s: %s"),
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
	                  _("ppdc: Unable to create output pipes: %s"),
	                  strerror(errno));
	  return (1);
	}

	if ((pid = fork()) == 0)
	{
	  // Child process comes here...
	  dup2(fds[0], 0);

	  close(fds[0]);
	  close(fds[1]);

	  execlp("cupstestppd", "cupstestppd", "-", (char *)0);

	  _cupsLangPrintf(stderr,
	                  _("ppdc: Unable to execute cupstestppd: %s"),
			  strerror(errno));
	  return (errno);
	}
	else if (pid < 0)
	{
	  _cupsLangPrintf(stderr, _("ppdc: Unable to execute cupstestppd: %s"),
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
	{
	  if (!_cups_strncasecmp(d->model_name->value, d->manufacturer->value,
	                   strlen(d->manufacturer->value)))
	  {
	    // Model name already starts with the manufacturer...
            outname = d->model_name->value;
	  }
	  else
	  {
	    // Add manufacturer to the front of the model name...
	    snprintf(make_model, sizeof(make_model), "%s %s",
	             d->manufacturer->value, d->model_name->value);
	    outname = make_model;
	  }
	}
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
	    pcfilename[j] = (char)tolower(outname[j] & 255);

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

        if (cupsArrayFind(filenames, filename))
	  _cupsLangPrintf(stderr,
	                  _("ppdc: Warning - overlapping filename \"%s\"."),
			  filename);
	else
	  cupsArrayAdd(filenames, strdup(filename));

	fp = cupsFileOpen(filename, comp ? "w9" : "w");
	if (!fp)
	{
	  _cupsLangPrintf(stderr,
	                  _("ppdc: Unable to create PPD file \"%s\" - %s."),
			  filename, strerror(errno));
	  return (1);
	}

	if (verbose)
	  _cupsLangPrintf(stdout, _("ppdc: Writing %s."), filename);
      }

     /*
      * Write the PPD file...
      */

      ppdcArray *templocales = locales;

      if (!templocales && !single_language)
      {
	templocales = new ppdcArray();
	for (ppdcCatalog *tempcatalog = (ppdcCatalog *)src->po_files->first();
	     tempcatalog;
	     tempcatalog = (ppdcCatalog *)src->po_files->next())
	{
	  tempcatalog->locale->retain();
	  templocales->add(tempcatalog->locale);
	}
      }

      if (d->write_ppd_file(fp, catalog, templocales, src, le))
      {
	cupsFileClose(fp);
	return (1);
      }

      if (templocales != locales)
        templocales->release();

      cupsFileClose(fp);
    }
  }
  else
    usage();

  // Delete the printer driver information...
  src->release();

  // Message catalog...
  if (catalog)
    catalog->release();

  // Return with no errors.
  return (0);
}


//
// 'usage()' - Show usage and exit.
//

static void
usage(void)
{
  _cupsLangPuts(stdout, _("Usage: ppdc [options] filename.drv [ ... "
                          "filenameN.drv ]"));
  _cupsLangPuts(stdout, _("Options:"));
  _cupsLangPuts(stdout, _("  -D name=value           Set named variable to "
                          "value."));
  _cupsLangPuts(stdout, _("  -I include-dir          Add include directory to "
                          "search path."));
  _cupsLangPuts(stdout, _("  -c catalog.po           Load the specified "
                          "message catalog."));
  _cupsLangPuts(stdout, _("  -d output-dir           Specify the output "
                          "directory."));
  _cupsLangPuts(stdout, _("  -l lang[,lang,...]      Specify the output "
                          "language(s) (locale)."));
  _cupsLangPuts(stdout, _("  -m                      Use the ModelName value "
                          "as the filename."));
  _cupsLangPuts(stdout, _("  -t                      Test PPDs instead of "
                          "generating them."));
  _cupsLangPuts(stdout, _("  -v                      Be verbose."));
  _cupsLangPuts(stdout, _("  -z                      Compress PPD files using "
                          "GNU zip."));
  _cupsLangPuts(stdout, _("  --cr                    End lines with CR (Mac "
                          "OS 9)."));
  _cupsLangPuts(stdout, _("  --crlf                  End lines with CR + LF "
                          "(Windows)."));
  _cupsLangPuts(stdout, _("  --lf                    End lines with LF "
                          "(UNIX/Linux/OS X)."));

  exit(1);
}


//
// End of "$Id: ppdc.cxx 11800 2014-04-08 19:53:57Z msweet $".
//
