//
// "$Id$"
//
//   DDK driver interface main entry for the CUPS PPD Compiler.
//
//   Copyright 2007 by Apple Inc.
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
//   main()      - Enumerate or display PPD files.
//   cat_ppd()   - Display a PPD file.
//   list_ppds() - List PPDs.
//

//
// Include necessary headers...
//

#include "ppdc.h"
#include <cups/cups.h>
#include <cups/dir.h>
#include <sys/stat.h>
#include <sys/types.h>


//
// Local functions...
//

static int	cat_ppd(ppdcSource *src, const char *name);
static int	list_drvs(const char *pathname, const char *prefix);
static int	list_ppds(ppdcSource *src, const char *name);


//
// 'main()' - Enumerate or display PPD files.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  const char	*datadir;		// CUPS_DATADIR
  ppdcSource	*src;			// PPD source file data
  char		filename[1024],		// Full path to .drv file(s)
		scheme[32],		// URI scheme ("drv")
		userpass[256],		// User/password info (unused)
		host[2],		// Hostname (unused)
		resource[1024],		// Resource path (/dir/to/filename.drv)
		*pc_file_name;		// Filename portion of URI
  int		port,			// Port number (unused)
		status;			// Exit status


  // Determine where CUPS has installed the data files...
  if ((datadir = getenv("CUPS_DATADIR")) == NULL)
    datadir = CUPS_DATADIR;

  // List all available PPDs or cat a single PPD...
  if (argc == 2 && !strcmp(argv[1], "list"))
  {
    snprintf(filename, sizeof(filename), "%s/drv", datadir);
    return (list_drvs(filename, "/"));
  }
  else if (argc == 3 && !strcmp(argv[1], "cat"))
  {
    httpSeparateURI(HTTP_URI_CODING_ALL, argv[2], scheme, sizeof(scheme),
                    userpass, sizeof(userpass), host, sizeof(host), &port,
		    resource, sizeof(resource));

    if (strstr(resource, "../") ||
        (pc_file_name = strrchr(resource, '/')) == NULL ||
	pc_file_name == resource)
    {
      fprintf(stderr, "ERROR: Bad driver info URI \"%s\"!\n", argv[2]);
      return (1);
    }

    *pc_file_name++ = '\0';

    snprintf(filename, sizeof(filename), "%s/drv%s", datadir, resource);

    src = new ppdcSource(filename);

    status = cat_ppd(src, pc_file_name);

    delete src;

    return (status);
  }
  
  fprintf(stderr, "ERROR: Usage: %s cat URI\n", argv[0]);
  fprintf(stderr, "ERROR: Usage: %s list\n", argv[0]);

  return (1);
}


//
// 'cat_ppd()' - Display a PPD file.
//

static int				// O - Exit status
cat_ppd(ppdcSource *src,		// I - Driver info file
        const char *name)		// I - PC filename
{
  ppdcDriver	*d;			// Current driver
  cups_file_t	*out;			// Stdout via CUPS file API 
 
  
  for (d = (ppdcDriver *)src->drivers->first();
       d;
       d = (ppdcDriver *)src->drivers->next())
    if (!strcmp(name, d->pc_file_name->value))
    {
      out = cupsFileStdout();

      d->write_ppd_file(out, NULL, NULL, src, PPDC_LFONLY);
      cupsFileClose(out);
      return (0);
    }
  
  return (1);
}


//
// 'list_drvs()' - List all drv files in the given path...
//

static int				// O - Exit status
list_drvs(const char *pathname,		// I - Full path to directory
          const char *prefix)		// I - Prefix for directory
{
  char		*ext,			// Extension on file
		filename[1024],		// Full path to .drv file(s)
		newprefix[1024];	// New prefix for directory
  cups_dir_t	*dir;			// Current directory
  cups_dentry_t	*dent;			// Current directory entry


  if ((dir = cupsDirOpen(pathname)) == NULL)
    return (1);

  while ((dent = cupsDirRead(dir)) != NULL)
  {
    // Skip "dot" files...
    if (dent->filename[0] == '.')
      continue;

    // See if this is a file or directory...
    snprintf(filename, sizeof(filename), "%s/%s", pathname, dent->filename);

    if (S_ISDIR(dent->fileinfo.st_mode))
    {
      // Descend into the subdirectory...
      snprintf(newprefix, sizeof(newprefix), "%s%s/", prefix, dent->filename);

      if (list_drvs(filename, newprefix))
      {
	cupsDirClose(dir);
	return (1);
      }
    }
    else if ((ext = strrchr(dent->filename, '.')) != NULL &&
             (!strcmp(ext, ".drv") || !strcmp(ext, ".drv.gz")))
    {
      // List the PPDs in this driver info file...
      ppdcSource *src = new ppdcSource(filename);
					// Driver info file

      snprintf(newprefix, sizeof(newprefix), "%s%s", prefix, dent->filename);
      list_ppds(src, newprefix);
      delete src;
    }
  }

  cupsDirClose(dir);

  return (0);
}


//
// 'list_ppds()' - List PPDs in a driver info file.
//

static int				// O - Exit status
list_ppds(ppdcSource *src,		// I - Driver info file
          const char *name)		// I - Name of driver info file
{
  ppdcDriver	*d;			// Current driver
  ppdcAttr	*attr;			// 1284DeviceID attribute
  char		uri[1024];		// Driver URI


  for (d = (ppdcDriver *)src->drivers->first();
       d;
       d = (ppdcDriver *)src->drivers->next())
  {
    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "drv", "", "", 0,
                     "%s/%s", name, d->pc_file_name->value);

    attr = d->find_attr("1284DeviceID", NULL);

    printf("\"%s\" en \"%s\" \"%s\" \"%s\"\n", uri, d->manufacturer->value,
           d->model_name->value, attr ? attr->value->value : "");
  }

  return (0);
}




#if 0



  // Scan the command-line...
  catalog = NULL;
  outdir  = "ppd";
  src     = 0;
  verbose = 0;
  locales = NULL;
  comp    = PPDC_NO_COMPRESSION;
  le      = PPDC_LFONLY;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
        switch (*opt)
	{
	  case 'c' :			// Message catalog...
	      i ++;
              if (i >= argc)
                usage();

              if (verbose > 1)
	        printf("ppdc: Loading messages from \"%s\"...\n", argv[i]);

              if (!catalog)
	        catalog = new ppdcCatalog("en");

              if (catalog->load_messages(argv[i]))
	      {
        	fprintf(stderr,
		        "ppdc: Unable to load localization file \"%s\" - %s\n",
	        	argv[i], strerror(errno));
                return (1);
	      }
	      break;

          case 'd' :			// Output directory...
	      i ++;
	      if (i >= argc)
        	usage();

              if (verbose > 1)
	        printf("ppdc: Writing PPD files to directory \"%s\"...\n",
		       argv[i]);

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
	          printf("ppdc: Loading messages for locale \"%s\"...\n",
			 argv[i]);

        	if (catalog)
	          delete catalog;

        	catalog = new ppdcCatalog(argv[i]);

		if (catalog->messages->count == 0)
		{
        	  fprintf(stderr,
		          "ppdc: Unable to find localization for \"%s\" - %s\n",
	        	  argv[i], strerror(errno));
                  return (1);
		}
	      }
	      break;

          case 'I' :			// Include directory...
	      i ++;
	      if (i >= argc)
        	usage();

              if (verbose > 1)
	        printf("ppdc: Adding include directory \"%s\"...\n", argv[i]);

	      ppdcSource::add_include(argv[i]);
	      break;

          case 'v' :			// Be verbose...
	      verbose ++;
	      break;
	    
          case 'z' :			// Compress files...
	      comp = PPDC_GZIP_COMPRESSION;
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
        printf("ppdc: Loading driver information file \"%s\"...\n", argv[i]);

      src = new ppdcSource(argv[i]);

      // Create the output directory...
      if (mkdir(outdir, 0777))
      {
        if (errno != EEXIST)
	{
	  fprintf(stderr, "ppdc: Unable to create output directory %s: %s\n",
	          outdir, strerror(errno));
          return (1);
	}
      }

      // Write PPD files...
      for (d = (ppdcDriver *)src->drivers->first();
           d;
	   d = (ppdcDriver *)src->drivers->next())
      {
        // Write the PPD file for this driver...
	for (j = 0; d->pc_file_name->value[j]; j ++)
	  pcfilename[j] = tolower(d->pc_file_name->value[j]);

	pcfilename[j] = '\0';

	if (comp == PPDC_GZIP_COMPRESSION)
	  snprintf(filename, sizeof(filename), "%s/%s.gz", outdir, pcfilename);
	else
	  snprintf(filename, sizeof(filename), "%s/%s", outdir, pcfilename);

	if (verbose)
	  printf("ppdc: Writing %s...\n", filename);

	if (d->write_ppd_file(filename, catalog, locales, src, le, comp))
	  return (1);
      }

      // Delete the printer driver information...
      delete src;
    }

  if (catalog)
    delete catalog;

  // If no drivers have been loaded, display the program usage message.
  if (!src)
    usage();

  // Return with no errors.
  return (0);
}
#endif // 0


//
// End of "$Id$".
//
