//
// "$Id$"
//
//   PPD file merge utility for the CUPS PPD Compiler.
//
//   Copyright 2007 by Apple Inc.
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
//   main()       - Main entry for the PPD merge utility.
//   ppd_locale() - Return the locale associated with a PPD file.
//   usage()      - Show usage and exit.
//

//
// Include necessary headers...
//

#include <cups/cups.h>
#include <cups/array.h>
#include <cups/string.h>
#include <errno.h>


//
// Local functions...
//

static const char	*ppd_locale(ppd_file_t *ppd);
static void		usage(void);


//
// 'main()' - Main entry for the PPD merge utility.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int		i;			// Looping var
  char		*opt;			// Current option
  ppd_file_t	*ppd;			// PPD file
  cups_array_t	*ppds;			// Array of PPD files
  const char	*inname,		// First input filename
		*outname;		// Output filename (if any)
  cups_file_t	*infile,		// Input file
		*outfile;		// Output file
  char		languages[1024];	// Languages in file


  // Scan the command-line...
  inname       = NULL;
  outname      = NULL;
  outfile      = NULL;
  languages[0] = '\0';
  ppds         = cupsArrayNew(NULL, NULL);

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
        switch (*opt)
	{
	  case 'o' :			// Output file
              if (outname)
	        usage();

	      i ++;
	      if (i >= argc)
        	usage();

	      outname = argv[i];
	      break;

	  default :			// Unknown
	      usage();
	      break;
        }
    }
    else
    {
      // Open and load the driver info file...
      if ((infile = cupsFileOpen(argv[i], "r")) == NULL)
      {
        fprintf(stderr, "ppdmerge: Unable to open %s - %s\n", argv[i],
	        strerror(errno));
	goto error;
      }

      // Open the PPD file...
      if ((ppd = ppdOpen2(infile)) == NULL)
      {
        ppd_status_t	status;		// PPD open status
	int		linenum;	// Line number
        char		line[1024];	// First line from file
	
	
        status = ppdLastError(&linenum);
	
	fprintf(stderr, "ppdmerge: Unable to open %s - %s on line %d.\n",
	        argv[i], ppdErrorString(status), linenum);
	
        fprintf(stderr, "%d: ", linenum);
        cupsFileRewind(infile);
	
        line[0] = '\0';
	
        while (cupsFileGets(infile, line, sizeof(line)))
	{
	  linenum --;
	  if (!linenum)
	    break;
	}
	
	fprintf(stderr, "%s\n", line);
	
        cupsFileClose(infile);

        goto error;
      }
      
      // Figure out the locale...
      const char *locale = ppd_locale(ppd);

      if (!locale)
      {
        fprintf(stderr, "ppdmerge: Bad LanguageVersion \"%s\" in %s!\n",
	        ppd->lang_version, argv[i]);
        cupsFileClose(infile);
	ppdClose(ppd);

	goto error;
      }
      
      if (!strcmp(locale, "en") && !inname && !outfile)
      {
        // Set the English PPD's filename...
        inname = argv[i];
	
        if (outname && !strcmp(inname, outname))
	{	
	  // Rename input filename so that we don't overwrite it...
	  char bckname[1024];		// Backup filename
	  
	  
	  snprintf(bckname, sizeof(bckname), "%s.bck", inname);
	  
	  if (rename(inname, bckname))
	  {
	    fprintf(stderr, "ppdmerge: Unable to backup %s to %s- %s\n",
	            inname, bckname, strerror(errno));
	    return (1);
	  }
	}

        // Open the output stream...
	if (outname)
	{
	  const char *ext = strrchr(outname, '.');
	  if (ext && !strcmp(ext, ".ppd.gz"))
	    outfile = cupsFileOpen(outname, "w9");
	  else
	    outfile = cupsFileOpen(outname, "w");
	}
	else
	  outfile = cupsFileStdout();

        // Copy the PPD file to the output stream...
	char line[1024];		// Line from file
	
	cupsFileRewind(infile);

	while (cupsFileGets(infile, line, sizeof(line)))
	{
	  if (!strncmp(line, "*cupsLanguages:", 15))
	  {
	    if (sscanf(line, "*cupsLanguages:%*[ \t]\"%1023[^\"]",
		       languages) != 1)
	      languages[0] = '\0';
	  }
	  else
	    cupsFilePrintf(outfile, "%s\n", line);
	}
      }
      else if (strcmp(locale, "en") && !strstr(languages, locale))
      {
	// Save this PPD for later processing...
        cupsArrayAdd(ppds, ppd);
      }
      else
      {
        // Don't need this PPD...
	fprintf(stderr, "ppdmerge: Ignoring PPD file %s...\n", argv[i]);
        ppdClose(ppd);
      }
      
      // Close and move on...
      cupsFileClose(infile);
    }

  // If no PPDs have been loaded, display the program usage message.
  if (!inname)
    usage();
      
  // Loop through the PPD files we loaded to provide the translations...
  for (ppd = (ppd_file_t *)cupsArrayFirst(ppds);
       ppd;
       ppd = (ppd_file_t *)cupsArrayNext(ppds))
  {
    // Output all of the UI text for this language...
    int			j, k, l;	// Looping vars
    ppd_group_t		*g;		// Option group
    ppd_option_t	*o;		// Option
    ppd_choice_t	*c;		// Choice
    const char		*locale = ppd_locale(ppd);
					// Locale

    
    cupsFilePrintf(outfile, "*%% %s localization\n", ppd->lang_version);
    
    cupsFilePrintf(outfile, "*%s.Translation ModelName/%s: \"\"\n", locale,
		   ppd->modelname);
    
    for (j = ppd->num_groups, g = ppd->groups; j > 0; j --, g ++)
    {
      cupsFilePrintf(outfile, "*%s.Translation %s/%s: \"\"\n", locale,
		     g->name, g->text);
      
      for (k = g->num_options, o = g->options; k > 0; k --, o ++)
      {
	cupsFilePrintf(outfile, "*%s.Translation %s/%s: \"\"\n", locale,
		       o->keyword, o->text);
	
	for (l = o->num_choices, c = o->choices; l > 0; l --, c ++)
	  cupsFilePrintf(outfile, "*%s.%s %s/%s: \"\"\n", locale,
			 o->keyword, c->choice, c->text);
      }
    }
    
    if (languages[0])
      strlcat(languages, " ", sizeof(languages));
    
    strlcat(languages, locale, sizeof(languages));

    ppdClose(ppd);
  }

  cupsArrayDelete(ppds);

  if (languages[0])
    cupsFilePrintf(outfile, "*cupsLanguages: \"%s\"\n", languages);

  cupsFileClose(outfile);

  // Return with no errors.
  return (0);

  // Error out...
error:

  for (ppd = (ppd_file_t *)cupsArrayFirst(ppds);
       ppd;
       ppd = (ppd_file_t *)cupsArrayNext(ppds))
    ppdClose(ppd);

  cupsArrayDelete(ppds);

  if (outfile)
    cupsFileClose(outfile);

  return (1);
}


//
// 'ppd_locale()' - Return the locale associated with a PPD file.
//

static const char *			// O - Locale string
ppd_locale(ppd_file_t *ppd)		// I - PPD file
{
  int		i,			// Looping var
		vlen;			// Length of LanguageVersion string
  static char	locale[255];		// Locale string
  static struct				// LanguageVersion translation table
  {
    const char	*version,		// LanguageVersion string */
		*language;		// Language code */
  }		languages[] =
  {
    { "chinese",		"zh" },
    { "czech",			"cs" },
    { "danish",			"da" },
    { "dutch",			"nl" },
    { "english",		"en" },
    { "finnish",		"fi" },
    { "french",			"fr" },
    { "german",			"de" },
    { "greek",			"el" },
    { "hungarian",		"hu" },
    { "italian",		"it" },
    { "japanese",		"ja" },
    { "korean",			"ko" },
    { "norwegian",		"no" },
    { "polish",			"pl" },
    { "portuguese",		"pt" },
    { "russian",		"ru" },
    { "simplified chinese",	"zh_CN" },
    { "slovak",			"sk" },
    { "spanish",		"es" },
    { "swedish",		"sv" },
    { "traditional chinese",	"zh_TW" },
    { "turkish",		"tr" }
  };


  for (i = 0; i < (int)(sizeof(languages) / sizeof(languages[0])); i ++)
  {
    vlen = strlen(languages[i].version);

    if (!strncasecmp(ppd->lang_version, languages[i].version, vlen))
    {
      if (ppd->lang_version[vlen] == '-' ||
          ppd->lang_version[vlen] == '_')
        snprintf(locale, sizeof(locale), "%s_%s", languages[i].language,
	         ppd->lang_version + vlen + 1);
      else
        strlcpy(locale, languages[i].language, sizeof(locale));

      return (locale);
    }
  }

  return (NULL);
}

//
// 'usage()' - Show usage and exit.
//

static void
usage(void)
{
  puts("Usage: ppdmerge [options] filename.ppd [ ... filenameN.ppd ]");
  puts("Options:");
  puts("  -o filename.ppd[.gz]");

  exit(1);
}


//
// End of "$Id$".
//
