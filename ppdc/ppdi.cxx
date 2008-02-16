//
// "$Id$"
//
//   PPD file import utility for the CUPS PPD Compiler.
//
//   Copyright 2007 by Apple Inc.
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
//   main()  - Main entry for the PPD import utility.
//   usage() - Show usage and exit.
//

//
// Include necessary headers...
//

#include "ppdc.h"
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>


//
// Local functions...
//

static void	usage(void);


//
// 'main()' - Main entry for the PPD import utility.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int		i;			// Looping var
  char		*opt;			// Current option
  const char	*srcfile;		// Output file
  ppdcSource	*src;			// PPD source file data


  // Scan the command-line...
  srcfile = NULL;
  src     = NULL;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
        switch (*opt)
	{
	  case 'o' :			// Output file
              if (srcfile || src)
	        usage();

	      i ++;
	      if (i >= argc)
        	usage();

	      srcfile = argv[i];
	      break;

	  case 'I' :			// Include dir
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
      if (!srcfile)
        srcfile = "ppdi.drv";

      if (!src)
      {
        if (access(srcfile, 0))
	  src = new ppdcSource();
	else
          src = new ppdcSource(srcfile);
      }

      // Import the PPD file...
      src->import_ppd(argv[i]);
    }

  // If no drivers have been loaded, display the program usage message.
  if (!src)
    usage();

  // Write the driver info file back to disk...
  src->write_file(srcfile);

  // Delete the printer driver information...
  delete src;

  // Return with no errors.
  return (0);
}


//
// 'usage()' - Show usage and exit.
//

static void
usage(void)
{
  puts("Usage: ppdi [options] filename.ppd [ ... filenameN.ppd ]");
  puts("Options:");
  puts("  -I include-dir");
  puts("  -o filename.drv");

  exit(1);
}


//
// End of "$Id$".
//
