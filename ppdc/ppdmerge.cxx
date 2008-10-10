//
// "$Id$"
//
//   PPD file merge utility for the CUPS PPD Compiler.
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
//   main()       - Main entry for the PPD merge utility.
//   ppd_locale() - Return the locale associated with a PPD file.
//   usage()      - Show usage and exit.
//

//
// Include necessary headers...
//

#include <cups/ppd-private.h>
#include <cups/cups.h>
#include <cups/array.h>
#include <cups/string.h>
#include <cups/i18n.h>
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
  cups_array_t	*languages;		// Languages in file
  const char	*locale;		// Current locale
  char		line[1024];		// Line from file


  _cupsSetLocale(argv);

  // Scan the command-line...
  inname    = NULL;
  outname   = NULL;
  outfile   = NULL;
  languages = NULL;
  ppds      = cupsArrayNew(NULL, NULL);

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
      // Open and load the PPD file...
      if ((infile = cupsFileOpen(argv[i], "r")) == NULL)
      {
        _cupsLangPrintf(stderr, _("%s: Unable to open %s - %s\n"), "ppdmerge",
	                argv[i], strerror(errno));
	return (1);
      }

      // Open the PPD file...
      if ((ppd = ppdOpen2(infile)) == NULL)
      {
        ppd_status_t	status;		// PPD open status
	int		linenum;	// Line number
	
	
        status = ppdLastError(&linenum);
	
	_cupsLangPrintf(stderr, _("%s: Unable to open %s - %s on line %d.\n"),
	                "ppdmerge", argv[i], ppdErrorString(status), linenum);
	
        _cupsLangPrintf(stderr, "%d: ", linenum);
        cupsFileRewind(infile);
	
        line[0] = '\0';
	
        while (cupsFileGets(infile, line, sizeof(line)))
	{
	  linenum --;
	  if (!linenum)
	    break;
	}
	
	_cupsLangPrintf(stderr, "%s\n", line);
	
        cupsFileClose(infile);
	return (1);
      }
      
      // Figure out the locale...
      if ((locale = ppd_locale(ppd)) == NULL)
      {
        _cupsLangPrintf(stderr,
	                _("ppdmerge: Bad LanguageVersion \"%s\" in %s!\n"),
			ppd->lang_version, argv[i]);
        cupsFileClose(infile);
	ppdClose(ppd);
	return (1);
      }

      if (!strcmp(locale, "en") && !inname && !outfile)
      {
        // Set the English PPD's filename...
	inname    = argv[i];
	languages = _ppdGetLanguages(ppd);

        if (outname && !strcmp(inname, outname))
	{	
	  // Rename input filename so that we don't overwrite it...
	  char bckname[1024];		// Backup filename
	  
	  
	  snprintf(bckname, sizeof(bckname), "%s.bck", inname);
	  
	  if (rename(inname, bckname))
	  {
	    _cupsLangPrintf(stderr,
	                    _("ppdmerge: Unable to backup %s to %s- %s\n"),
			    inname, bckname, strerror(errno));
	    return (1);
	  }

	  inname = bckname;
	}
      }
      else if (strcmp(locale, "en"))
      {
	// Save this PPD for later processing...
        cupsArrayAdd(ppds, ppd);
      }
      else
      {
        // Don't need this PPD...
	_cupsLangPrintf(stderr, _("ppdmerge: Ignoring PPD file %s...\n"),
	                argv[i]);
        ppdClose(ppd);
      }
      
      // Close and move on...
      cupsFileClose(infile);
    }

  // If no PPDs have been loaded, display the program usage message.
  if (!inname)
    usage();

  // Loop through the PPD files we loaded to generate a new language list...
  if (!languages)
    languages = cupsArrayNew((cups_array_func_t)strcmp, NULL);

  for (ppd = (ppd_file_t *)cupsArrayFirst(ppds);
       ppd;
       ppd = (ppd_file_t *)cupsArrayNext(ppds))
  {
    locale = ppd_locale(ppd);

    if (cupsArrayFind(languages, (void *)locale))
    {
      // Already have this language, remove the PPD from the list.
      ppdClose(ppd);
      cupsArrayRemove(ppds, ppd);
    }
    else
      cupsArrayAdd(languages, (void *)locale);
  }

  // Copy the English PPD starting with a cupsLanguages line...
  infile = cupsFileOpen(inname, "r");

  if (outname)
  {
    const char *ext = strrchr(outname, '.');
    if (ext && !strcmp(ext, ".gz"))
      outfile = cupsFileOpen(outname, "w9");
    else
      outfile = cupsFileOpen(outname, "w");
  }
  else
    outfile = cupsFileStdout();

  cupsFileGets(infile, line, sizeof(line));
  cupsFilePrintf(outfile, "%s\n", line);
  if ((locale = (char *)cupsArrayFirst(languages)) != NULL)
  {
    cupsFilePrintf(outfile, "*cupsLanguages: \"%s", locale);
    while ((locale = (char *)cupsArrayNext(languages)) != NULL)
      cupsFilePrintf(outfile, " %s", locale);
    cupsFilePuts(outfile, "\"\n");
  }

  while (cupsFileGets(infile, line, sizeof(line)))
  {
    if (strncmp(line, "*cupsLanguages:", 15))
      cupsFilePrintf(outfile, "%s\n", line);
  }

  // Loop through the other PPD files we loaded to provide the translations...
  for (ppd = (ppd_file_t *)cupsArrayFirst(ppds);
       ppd;
       ppd = (ppd_file_t *)cupsArrayNext(ppds))
  {
    // Output all of the UI text for this language...
    int			j, k, l;	// Looping vars
    ppd_group_t		*g;		// Option group
    ppd_option_t	*o;		// Option
    ppd_choice_t	*c;		// Choice
    ppd_coption_t	*co;		// Custom option
    ppd_cparam_t	*cp;		// Custom parameter
    ppd_attr_t		*attr;		// PPD attribute

    locale = ppd_locale(ppd);

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

	if ((co = ppdFindCustomOption(ppd, o->keyword)) != NULL)
	{
	  snprintf(line, sizeof(line), "Custom%s", o->keyword);
	  attr = ppdFindAttr(ppd, line, "True");
	  cupsFilePrintf(outfile, "*%s.Custom%s True/%s: \"\"\n", locale,
			 o->keyword, attr->text);
	  for (cp = ppdFirstCustomParam(co); cp; cp = ppdNextCustomParam(co))
	    cupsFilePrintf(outfile, "*%s.ParamCustom%s %s/%s: \"\"\n", locale,
			   o->keyword, cp->name, cp->text);
	}
      }
    }

    ppdClose(ppd);
  }

  cupsArrayDelete(ppds);

  cupsFileClose(outfile);

  // Return with no errors.
  return (0);
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
  _cupsLangPuts(stdout,
                _("Usage: ppdmerge [options] filename.ppd "
		  "[ ... filenameN.ppd ]\n"
		  "Options:\n"
                  "  -o filename.ppd[.gz]\n"));

  exit(1);
}


//
// End of "$Id$".
//
