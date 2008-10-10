//
// "$Id$"
//
//   GNU gettext message generator for the CUPS PPD Compiler.
//
//   This program is used to generate a dummy source file containing all of
//   the standard media and sample driver strings.  The results are picked up
//   by GNU gettext and placed in the CUPS message catalog.
//
//   Copyright 2008 by Apple Inc.
//
//   These coded instructions, statements, and computer programs are the
//   property of Apple Inc. and are protected by Federal copyright
//   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
//   which should have been included with this file.  If this file is
//   file is missing or damaged, see the license at "http://www.cups.org/".
//
// Usage:
//
//   ./genstrings >sample.c
//
// Contents:
//
//   main()          - Main entry for the PPD compiler.
//   write_cstring() - Write a translation string as a valid C string to stdout.
//

//
// Include necessary headers...
//

#include "ppdc.h"
#include <unistd.h>


//
// Local functions...
//

static void write_cstring(const char *s);


//
// 'main()' - Main entry for the PPD compiler.
//

int					// O - Exit status
main(void)
{
  int		i, j;			// Looping vars
  ppdcSource	*src;			// PPD source file data
  ppdcMediaSize	*size;			// Current media size
  ppdcDriver	*d;			// Current driver


  // Make sure we are in the right place...
  if (access("../data", 0) || access("sample.drv", 0))
  {
    puts("You must run genstrings from the ppdc directory.");
    return (1);
  }

  // Load the sample drivers...
  ppdcSource::add_include("../data");

  src = new ppdcSource("sample.drv");

  // First write all of the defined media sizes...
  for (size = (ppdcMediaSize *)src->sizes->first();
       size;
       size = (ppdcMediaSize *)src->sizes->next())
  {
    printf("/* Page size %s */ ", size->name->value);
    write_cstring(size->text->value);
  }

#if 0
  if (src->drivers->count > 0)
  {
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
      if (do_test)
      {
        // Test the PPD file for this driver...
	int	pid,			// Process ID
		fds[2];			// Pipe file descriptors


        if (pipe(fds))
	{
	  fprintf(stderr, "ppdc: Unable to create output pipes: %s\n",
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

	  fprintf(stderr, "ppdc: Unable to execute cupstestppd: %s\n",
	          strerror(errno));
	  return (errno);
	}
	else if (pid < 0)
	{
	  fprintf(stderr, "ppdc: Unable to execute cupstestppd: %s\n",
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
	  fprintf(stderr, "ppdc: Unable to create PPD file \"%s\" - %s.\n",
		  filename, strerror(errno));
	  return (1);
	}

	if (verbose)
	  printf("ppdc: Writing %s...\n", filename);
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
#endif // 0

  // Return with no errors.
  return (0);
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
// End of "$Id$".
//
