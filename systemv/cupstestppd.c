/*
 * "$Id: cupstestppd.c 7807 2008-07-28 21:54:24Z mike $"
 *
 *   PPD test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2009 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   PostScript is a trademark of Adobe Systems, Inc.
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main()               - Main entry for test program.
 *   check_basics()       - Check for CR LF, mixed line endings, and blank
 *                          lines.
 *   check_constraints()  - Check UIConstraints in the PPD file.
 *   check_case()         - Check that there are no duplicate groups, options,
 *                          or choices that differ only by case.
 *   check_defaults()     - Check default option keywords in the PPD file.
 *   check_duplex()       - Check duplex keywords in the PPD file.
 *   check_filters()      - Check filters in the PPD file.
 *   check_profiles()     - Check ICC color profiles in the PPD file.
 *   check_sizes()        - Check media sizes in the PPD file.
 *   check_translations() - Check translations in the PPD file.
 *   show_conflicts()     - Show option conflicts in a PPD file.
 *   test_raster()        - Test PostScript commands for raster printers.
 *   usage()              - Show program usage...
 *   valid_path()         - Check whether a path has the correct capitalization.
 *   valid_utf8()         - Check whether a string contains valid UTF-8 text.
 */

/*
 * Include necessary headers...
 */

#include <cups/string.h>
#include <cups/cups.h>
#include <cups/dir.h>
#include <cups/ppd-private.h>
#include <cups/i18n.h>
#include <cups/raster.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>


/*
 * Error warning overrides...
 */

enum
{
  WARN_NONE = 0,
  WARN_CONSTRAINTS = 1,
  WARN_DEFAULTS = 2,
  WARN_FILTERS = 4,
  WARN_PROFILES = 8,
  WARN_TRANSLATIONS = 16,
  WARN_DUPLEX = 32,
  WARN_SIZES = 64,
  WARN_ALL = 127
};


/*
 * Error codes...
 */

enum
{
  ERROR_NONE = 0,
  ERROR_USAGE,
  ERROR_FILE_OPEN,
  ERROR_PPD_FORMAT,
  ERROR_CONFORMANCE
};


/*
 * Line endings...
 */

enum
{
  EOL_NONE = 0,
  EOL_CR,
  EOL_LF,
  EOL_CRLF
};


/*
 * Standard Adobe media keywords (must remain sorted)...
 */

static const char adobe_size_names[][PPD_MAX_NAME] =
{
  "10x11",
  "10x13",
  "10x14",
  "12x11",
  "15x11",
  "7x9",
  "8x10",
  "9x11",
  "9x12",
  "A0",
  "A1",
  "A10",
  "A2",
  "A3",
  "A3Extra",
  "A3Rotated",
  "A4",
  "A4Extra",
  "A4Plus",
  "A4Rotated",
  "A4Small",
  "A5",
  "A5Extra",
  "A5Rotated",
  "A6",
  "A6Rotated",
  "A7",
  "A8",
  "A9",
  "ARCHA",
  "ARCHB",
  "ARCHC",
  "ARCHD",
  "ARCHE",
  "AnsiA",
  "AnsiB",
  "AnsiC",
  "AnsiD",
  "AnsiE",
  "B0",
  "B1",
  "B1",
  "B10",
  "B2",
  "B3",
  "B4",
  "B4Rotated",
  "B5",
  "B5Rotated",
  "B6",
  "B6Rotated",
  "B7",
  "B8",
  "B9",
  "C4",
  "C5",
  "C6",
  "DL",
  "DoublePostcard",
  "DoublePostcardRotated",
  "Env10",
  "Env11",
  "Env12",
  "Env14",
  "Env9",
  "EnvC0",
  "EnvC1",
  "EnvC2",
  "EnvC3",
  "EnvC4",
  "EnvC5",
  "EnvC6",
  "EnvC65",
  "EnvC7",
  "EnvChou3",
  "EnvChou3Rotated",
  "EnvChou4",
  "EnvChou4Rotated",
  "EnvDL",
  "EnvISOB4",
  "EnvISOB5",
  "EnvISOB6",
  "EnvInvite",
  "EnvItalian",
  "EnvKaku2",
  "EnvKaku2Rotated",
  "EnvKaku3",
  "EnvKaku3Rotated",
  "EnvMonarch",
  "EnvPRC1",
  "EnvPRC10",
  "EnvPRC10Rotated",
  "EnvPRC1Rotated",
  "EnvPRC2",
  "EnvPRC2Rotated",
  "EnvPRC3",
  "EnvPRC3Rotated",
  "EnvPRC4",
  "EnvPRC4Rotated",
  "EnvPRC5",
  "EnvPRC5Rotated",
  "EnvPRC6",
  "EnvPRC6Rotated",
  "EnvPRC7",
  "EnvPRC7Rotated",
  "EnvPRC8",
  "EnvPRC8Rotated",
  "EnvPRC9",
  "EnvPRC9Rotated",
  "EnvPersonal",
  "EnvYou4",
  "EnvYou4Rotated",
  "Executive",
  "FanFoldGerman",
  "FanFoldGermanLegal",
  "FanFoldUS",
  "Folio",
  "ISOB0",
  "ISOB1",
  "ISOB10",
  "ISOB2",
  "ISOB3",
  "ISOB4",
  "ISOB5",
  "ISOB5Extra",
  "ISOB6",
  "ISOB7",
  "ISOB8",
  "ISOB9",
  "Ledger",
  "Legal",
  "LegalExtra",
  "Letter",
  "LetterExtra",
  "LetterPlus",
  "LetterRotated",
  "LetterSmall",
  "Monarch",
  "Note",
  "PRC16K",
  "PRC16KRotated",
  "PRC32K",
  "PRC32KBig",
  "PRC32KBigRotated",
  "PRC32KRotated",
  "Postcard",
  "PostcardRotated",
  "Quarto",
  "Statement",
  "SuperA",
  "SuperB",
  "Tabloid",
  "TabloidExtra"
};


/*
 * Local functions...
 */

static void	check_basics(const char *filename);
static int	check_constraints(ppd_file_t *ppd, int errors, int verbose,
		                  int warn);
static int	check_case(ppd_file_t *ppd, int errors, int verbose);
static int	check_defaults(ppd_file_t *ppd, int errors, int verbose,
		               int warn);
static int	check_duplex(ppd_file_t *ppd, int errors, int verbose,
		             int warn);
static int	check_filters(ppd_file_t *ppd, const char *root, int errors,
		              int verbose, int warn);
static int	check_profiles(ppd_file_t *ppd, const char *root, int errors,
		               int verbose, int warn);
static int	check_sizes(ppd_file_t *ppd, int errors, int verbose, int warn);
static int	check_translations(ppd_file_t *ppd, int errors, int verbose,
		                   int warn);
static void	show_conflicts(ppd_file_t *ppd);
static int	test_raster(ppd_file_t *ppd, int verbose);
static void	usage(void);
static int	valid_path(const char *keyword, const char *path, int errors,
		           int verbose, int warn);
static int	valid_utf8(const char *s);


/*
 * 'main()' - Main entry for test program.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i, j, k, m, n;		/* Looping vars */
  int		len;			/* Length of option name */
  char		*opt;			/* Option character */
  const char	*ptr;			/* Pointer into string */
  int		files;			/* Number of files */
  int		verbose;		/* Want verbose output? */
  int		warn;			/* Which errors to just warn about */
  int		status;			/* Exit status */
  int		errors;			/* Number of conformance errors */
  int		ppdversion;		/* PPD spec version in PPD file */
  ppd_status_t	error;			/* Status of ppdOpen*() */
  int		line;			/* Line number for error */
  char		*root;			/* Root directory */
  int		xdpi,			/* X resolution */
		ydpi;			/* Y resolution */
  ppd_file_t	*ppd;			/* PPD file record */
  ppd_attr_t	*attr;			/* PPD attribute */
  ppd_size_t	*size;			/* Size record */
  ppd_group_t	*group;			/* UI group */
  ppd_option_t	*option;		/* Standard UI option */
  ppd_group_t	*group2;		/* UI group */
  ppd_option_t	*option2;		/* Standard UI option */
  ppd_choice_t	*choice;		/* Standard UI option choice */
  struct lconv	*loc;			/* Locale data */
  static char	*uis[] = { "BOOLEAN", "PICKONE", "PICKMANY" };
  static char	*sections[] = { "ANY", "DOCUMENT", "EXIT",
                                "JCL", "PAGE", "PROLOG" };


  _cupsSetLocale(argv);
  loc = localeconv();

 /*
  * Display PPD files for each file listed on the command-line...
  */

  ppdSetConformance(PPD_CONFORM_STRICT);

  verbose = 0;
  ppd     = NULL;
  files   = 0;
  status  = ERROR_NONE;
  root    = "";
  warn    = WARN_NONE;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-' && argv[i][1])
    {
      for (opt = argv[i] + 1; *opt; opt ++)
        switch (*opt)
	{
	  case 'R' :			/* Alternate root directory */
	      i ++;

	      if (i >= argc)
	        usage();

              root = argv[i];
	      break;

	  case 'W' :			/* Turn errors into warnings  */
	      i ++;

	      if (i >= argc)
	        usage();

              if (!strcmp(argv[i], "none"))
	        warn = WARN_NONE;
	      else if (!strcmp(argv[i], "constraints"))
	        warn |= WARN_CONSTRAINTS;
	      else if (!strcmp(argv[i], "defaults"))
	        warn |= WARN_DEFAULTS;
	      else if (!strcmp(argv[i], "duplex"))
	        warn |= WARN_DUPLEX;
	      else if (!strcmp(argv[i], "filters"))
	        warn |= WARN_FILTERS;
	      else if (!strcmp(argv[i], "profiles"))
	        warn |= WARN_PROFILES;
	      else if (!strcmp(argv[i], "sizes"))
	        warn |= WARN_SIZES;
	      else if (!strcmp(argv[i], "translations"))
	        warn |= WARN_TRANSLATIONS;
	      else if (!strcmp(argv[i], "all"))
	        warn = WARN_ALL;
	      else
	        usage();
	      break;

	  case 'q' :			/* Quiet mode */
	      if (verbose > 0)
	      {
        	_cupsLangPuts(stderr,
		              _("cupstestppd: The -q option is incompatible "
			        "with the -v option.\n"));
		return (1);
	      }

	      verbose --;
	      break;

	  case 'r' :			/* Relaxed mode */
              ppdSetConformance(PPD_CONFORM_RELAXED);
	      break;

	  case 'v' :			/* Verbose mode */
	      if (verbose < 0)
	      {
        	_cupsLangPuts(stderr,
		              _("cupstestppd: The -v option is incompatible "
			        "with the -q option.\n"));
		return (1);
	      }

	      verbose ++;
	      break;

	  default :
	      usage();
	      break;
	}
    }
    else
    {
     /*
      * Open the PPD file...
      */

      if (files && verbose >= 0)
        _cupsLangPuts(stdout, "\n");

      files ++;

      if (argv[i][0] == '-')
      {
       /*
        * Read from stdin...
	*/

        ppd = ppdOpen(stdin);

        if (verbose >= 0)
          printf("%s:", (ppd && ppd->pcfilename) ? ppd->pcfilename : "(stdin)");
      }
      else
      {
       /*
        * Read from a file...
	*/

        if (verbose >= 0)
          printf("%s:", argv[i]);

        ppd = ppdOpenFile(argv[i]);
      }

      if (ppd == NULL)
      {
        error = ppdLastError(&line);

	if (error <= PPD_ALLOC_ERROR)
	{
	  status = ERROR_FILE_OPEN;

	  if (verbose >= 0)
            _cupsLangPrintf(stdout,
	                    _(" FAIL\n"
			      "      **FAIL**  Unable to open PPD file - %s\n"),
			    strerror(errno));
	}
	else
	{
	  status = ERROR_PPD_FORMAT;

          if (verbose >= 0)
	  {
            _cupsLangPrintf(stdout,
	                    _(" FAIL\n"
			      "      **FAIL**  Unable to open PPD file - "
			      "%s on line %d.\n"),
			    ppdErrorString(error), line);

            switch (error)
	    {
	      case PPD_MISSING_PPDADOBE4 :
	          _cupsLangPuts(stdout,
		                _("                REF: Page 42, section 5.2.\n"));
	          break;
	      case PPD_MISSING_VALUE :
	          _cupsLangPuts(stdout,
		                _("                REF: Page 20, section 3.4.\n"));
	          break;
	      case PPD_BAD_OPEN_GROUP :
	      case PPD_NESTED_OPEN_GROUP :
	          _cupsLangPuts(stdout,
		                _("                REF: Pages 45-46, section 5.2.\n"));
	          break;
	      case PPD_BAD_OPEN_UI :
	      case PPD_NESTED_OPEN_UI :
	          _cupsLangPuts(stdout,
		                _("                REF: Pages 42-45, section 5.2.\n"));
	          break;
	      case PPD_BAD_ORDER_DEPENDENCY :
	          _cupsLangPuts(stdout,
		                _("                REF: Pages 48-49, section 5.2.\n"));
	          break;
	      case PPD_BAD_UI_CONSTRAINTS :
	          _cupsLangPuts(stdout,
		                _("                REF: Pages 52-54, section 5.2.\n"));
	          break;
	      case PPD_MISSING_ASTERISK :
	          _cupsLangPuts(stdout,
		                _("                REF: Page 15, section 3.2.\n"));
	          break;
	      case PPD_LINE_TOO_LONG :
	          _cupsLangPuts(stdout,
		                _("                REF: Page 15, section 3.1.\n"));
	          break;
	      case PPD_ILLEGAL_CHARACTER :
	          _cupsLangPuts(stdout,
		                _("                REF: Page 15, section 3.1.\n"));
	          break;
	      case PPD_ILLEGAL_MAIN_KEYWORD :
	          _cupsLangPuts(stdout,
		                _("                REF: Pages 16-17, section 3.2.\n"));
	          break;
	      case PPD_ILLEGAL_OPTION_KEYWORD :
	          _cupsLangPuts(stdout,
		                _("                REF: Page 19, section 3.3.\n"));
	          break;
	      case PPD_ILLEGAL_TRANSLATION :
	          _cupsLangPuts(stdout,
		                _("                REF: Page 27, section 3.5.\n"));
	          break;
              default :
	          break;
	    }

	    check_basics(argv[i]);
	  }
        }

	continue;
      }

     /*
      * Show the header and then perform basic conformance tests (limited
      * only by what the CUPS PPD functions actually load...)
      */

      errors     = 0;
      ppdversion = 43;

      if (verbose > 0)
        _cupsLangPuts(stdout,
	              _("\n    DETAILED CONFORMANCE TEST RESULTS\n"));

      if ((attr = ppdFindAttr(ppd, "FormatVersion", NULL)) != NULL &&
          attr->value)
        ppdversion = (int)(10 * _cupsStrScand(attr->value, NULL, loc) + 0.5);

      for (j = 0; j < ppd->num_filters; j ++)
        if (strstr(ppd->filters[j], "application/vnd.cups-raster"))
	{
	  if (!test_raster(ppd, verbose))
	    errors ++;
	  break;
	}

     /*
      * Look for default keywords with no matching option...
      */

      if (!(warn & WARN_DEFAULTS))
        errors = check_defaults(ppd, errors, verbose, 0);

      if ((attr = ppdFindAttr(ppd, "DefaultImageableArea", NULL)) == NULL)
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPuts(stdout,
			_("      **FAIL**  REQUIRED DefaultImageableArea\n"
			  "                REF: Page 102, section 5.15.\n"));
	}

	errors ++;
      }
      else if (ppdPageSize(ppd, attr->value) == NULL &&
	       strcmp(attr->value, "Unknown"))
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPrintf(stdout,
			  _("      **FAIL**  BAD DefaultImageableArea %s!\n"
			    "                REF: Page 102, section 5.15.\n"),
			  attr->value);
	}

	errors ++;
      }
      else
      {
	if (verbose > 0)
	  _cupsLangPuts(stdout, _("        PASS    DefaultImageableArea\n"));
      }

      if ((attr = ppdFindAttr(ppd, "DefaultPaperDimension", NULL)) == NULL)
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPuts(stdout,
			_("      **FAIL**  REQUIRED DefaultPaperDimension\n"
			  "                REF: Page 103, section 5.15.\n"));
	}

	errors ++;
      }
      else if (ppdPageSize(ppd, attr->value) == NULL &&
	       strcmp(attr->value, "Unknown"))
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPrintf(stdout,
			  _("      **FAIL**  BAD DefaultPaperDimension %s!\n"
			    "                REF: Page 103, section 5.15.\n"),
			  attr->value);
	}

	errors ++;
      }
      else if (verbose > 0)
	_cupsLangPuts(stdout, _("        PASS    DefaultPaperDimension\n"));

      for (j = 0, group = ppd->groups; j < ppd->num_groups; j ++, group ++)
	for (k = 0, option = group->options; k < group->num_options; k ++, option ++)
	{
	 /*
	  * Verify that we have a default choice...
	  */

	  if (option->defchoice[0])
	  {
	    if (ppdFindChoice(option, option->defchoice) == NULL &&
		strcmp(option->defchoice, "Unknown"))
	    {
	      if (verbose >= 0)
	      {
		if (!errors && !verbose)
		  _cupsLangPuts(stdout, _(" FAIL\n"));

		_cupsLangPrintf(stdout,
				_("      **FAIL**  BAD Default%s %s\n"
				  "                REF: Page 40, section 4.5.\n"),
				option->keyword, option->defchoice);
	      }

	      errors ++;
	    }
	    else if (verbose > 0)
	      _cupsLangPrintf(stdout,
			      _("        PASS    Default%s\n"),
			      option->keyword);
	  }
	  else
	  {
	    if (verbose >= 0)
	    {
	      if (!errors && !verbose)
		_cupsLangPuts(stdout, _(" FAIL\n"));

	      _cupsLangPrintf(stdout,
			      _("      **FAIL**  REQUIRED Default%s\n"
				"                REF: Page 40, section 4.5.\n"),
			      option->keyword);
	    }

	    errors ++;
	  }
	}

      if ((attr = ppdFindAttr(ppd, "FileVersion", NULL)) != NULL)
      {
        for (ptr = attr->value; *ptr; ptr ++)
	  if (!isdigit(*ptr & 255) && *ptr != '.')
	    break;

	if (*ptr)
	{
	  if (verbose >= 0)
	  {
	    if (!errors && !verbose)
	      _cupsLangPuts(stdout, _(" FAIL\n"));

	    _cupsLangPrintf(stdout,
			    _("      **FAIL**  Bad FileVersion \"%s\"\n"
			      "                REF: Page 56, section 5.3.\n"),
			    attr->value);
	  }

	  errors ++;
	}
	else if (verbose > 0)
	  _cupsLangPuts(stdout, _("        PASS    FileVersion\n"));
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPuts(stdout,
	                _("      **FAIL**  REQUIRED FileVersion\n"
			  "                REF: Page 56, section 5.3.\n"));
        }

	errors ++;
      }

      if ((attr = ppdFindAttr(ppd, "FormatVersion", NULL)) != NULL)
      {
        ptr = attr->value;
	if (*ptr == '4' && ptr[1] == '.')
	{
	  
	  for (ptr += 2; *ptr; ptr ++)
	    if (!isdigit(*ptr & 255))
	      break;
        }

	if (*ptr)
	{
	  if (verbose >= 0)
	  {
	    if (!errors && !verbose)
	      _cupsLangPuts(stdout, _(" FAIL\n"));

	    _cupsLangPrintf(stdout,
			    _("      **FAIL**  Bad FormatVersion \"%s\"\n"
			      "                REF: Page 56, section 5.3.\n"),
			    attr->value);
	  }

	  errors ++;
	}
	else if (verbose > 0)
	  _cupsLangPuts(stdout, _("        PASS    FormatVersion\n"));
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPuts(stdout,
	                _("      **FAIL**  REQUIRED FormatVersion\n"
			  "                REF: Page 56, section 5.3.\n"));
        }

	errors ++;
      }

      if (ppd->lang_encoding != NULL)
      {
	if (verbose > 0)
	  _cupsLangPuts(stdout, _("        PASS    LanguageEncoding\n"));
      }
      else if (ppdversion > 40)
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPuts(stdout,
	                _("      **FAIL**  REQUIRED LanguageEncoding\n"
			  "                REF: Pages 56-57, section 5.3.\n"));
        }

	errors ++;
      }

      if (ppd->lang_version != NULL)
      {
	if (verbose > 0)
	  _cupsLangPuts(stdout, _("        PASS    LanguageVersion\n"));
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPuts(stdout,
	                _("      **FAIL**  REQUIRED LanguageVersion\n"
			  "                REF: Pages 57-58, section 5.3.\n"));
        }

	errors ++;
      }

      if (ppd->manufacturer != NULL)
      {
        if (!strncasecmp(ppd->manufacturer, "Hewlett-Packard", 15) ||
	    !strncasecmp(ppd->manufacturer, "Hewlett Packard", 15))
	{
	  if (verbose >= 0)
	  {
	    if (!errors && !verbose)
	      _cupsLangPuts(stdout, _(" FAIL\n"));

	    _cupsLangPuts(stdout,
	                  _("      **FAIL**  BAD Manufacturer (should be "
			    "\"HP\")\n"
			    "                REF: Page 211, table D.1.\n"));
          }

	  errors ++;
	}
        else if (!strncasecmp(ppd->manufacturer, "OkiData", 7) ||
	         !strncasecmp(ppd->manufacturer, "Oki Data", 8))
	{
	  if (verbose >= 0)
	  {
	    if (!errors && !verbose)
	      _cupsLangPuts(stdout, _(" FAIL\n"));

	    _cupsLangPuts(stdout,
	                  _("      **FAIL**  BAD Manufacturer (should be "
			    "\"Oki\")\n"
			    "                REF: Page 211, table D.1.\n"));
          }

	  errors ++;
	}
	else if (verbose > 0)
	  _cupsLangPuts(stdout, _("        PASS    Manufacturer\n"));
      }
      else if (ppdversion >= 43)
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPuts(stdout,
	                _("      **FAIL**  REQUIRED Manufacturer\n"
			  "                REF: Pages 58-59, section 5.3.\n"));
        }

	errors ++;
      }

      if (ppd->modelname != NULL)
      {
        for (ptr = ppd->modelname; *ptr; ptr ++)
	  if (!isalnum(*ptr & 255) && !strchr(" ./-+", *ptr))
	    break;

	if (*ptr)
	{
	  if (verbose >= 0)
	  {
	    if (!errors && !verbose)
	      _cupsLangPuts(stdout, _(" FAIL\n"));

	    _cupsLangPrintf(stdout,
	                    _("      **FAIL**  BAD ModelName - \"%c\" not "
			      "allowed in string.\n"
			      "                REF: Pages 59-60, section 5.3.\n"),
	                    *ptr);
          }

	  errors ++;
	}
	else if (verbose > 0)
	  _cupsLangPuts(stdout, _("        PASS    ModelName\n"));
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPuts(stdout,
	                _("      **FAIL**  REQUIRED ModelName\n"
			  "                REF: Pages 59-60, section 5.3.\n"));
        }

	errors ++;
      }

      if (ppd->nickname != NULL)
      {
	if (verbose > 0)
	  _cupsLangPuts(stdout, _("        PASS    NickName\n"));
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPuts(stdout,
	                _("      **FAIL**  REQUIRED NickName\n"
	                  "                REF: Page 60, section 5.3.\n"));
        }

	errors ++;
      }

      if (ppdFindOption(ppd, "PageSize") != NULL)
      {
	if (verbose > 0)
	  _cupsLangPuts(stdout, _("        PASS    PageSize\n"));
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPuts(stdout,
	                _("      **FAIL**  REQUIRED PageSize\n"
			  "                REF: Pages 99-100, section 5.14.\n"));
        }

	errors ++;
      }

      if (ppdFindOption(ppd, "PageRegion") != NULL)
      {
	if (verbose > 0)
	  _cupsLangPuts(stdout, _("        PASS    PageRegion\n"));
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPuts(stdout,
	                _("      **FAIL**  REQUIRED PageRegion\n"
			  "                REF: Page 100, section 5.14.\n"));
        }

	errors ++;
      }

      if (ppd->pcfilename != NULL)
      {
	if (verbose > 0)
          _cupsLangPuts(stdout, _("        PASS    PCFileName\n"));
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPuts(stdout,
	                _("      **FAIL**  REQUIRED PCFileName\n"
			  "                REF: Pages 61-62, section 5.3.\n"));
        }

	errors ++;
      }

      if (ppd->product != NULL)
      {
        if (ppd->product[0] != '(' ||
	    ppd->product[strlen(ppd->product) - 1] != ')')
	{
	  if (verbose >= 0)
	  {
	    if (!errors && !verbose)
	      _cupsLangPuts(stdout, _(" FAIL\n"));

	    _cupsLangPuts(stdout,
	                  _("      **FAIL**  BAD Product - not \"(string)\".\n"
			    "                REF: Page 62, section 5.3.\n"));
          }

	  errors ++;
	}
	else if (verbose > 0)
	  _cupsLangPuts(stdout, _("        PASS    Product\n"));
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPuts(stdout,
	                _("      **FAIL**  REQUIRED Product\n"
			  "                REF: Page 62, section 5.3.\n"));
        }

	errors ++;
      }

      if ((attr = ppdFindAttr(ppd, "PSVersion", NULL)) != NULL &&
          attr->value != NULL)
      {
        char	junkstr[255];			/* Temp string */
	int	junkint;			/* Temp integer */


        if (sscanf(attr->value, "(%[^)])%d", junkstr, &junkint) != 2)
	{
	  if (verbose >= 0)
	  {
	    if (!errors && !verbose)
	      _cupsLangPuts(stdout, _(" FAIL\n"));

	    _cupsLangPuts(stdout,
	                  _("      **FAIL**  BAD PSVersion - not \"(string) "
			    "int\".\n"
			    "                REF: Pages 62-64, section 5.3.\n"));
          }

	  errors ++;
	}
	else if (verbose > 0)
	  _cupsLangPuts(stdout, _("        PASS    PSVersion\n"));
      }
      else
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPuts(stdout,
	                _("      **FAIL**  REQUIRED PSVersion\n"
			  "                REF: Pages 62-64, section 5.3.\n"));
        }

	errors ++;
      }

      if (ppd->shortnickname != NULL)
      {
        if (strlen(ppd->shortnickname) > 31)
	{
	  if (verbose >= 0)
	  {
	    if (!errors && !verbose)
	      _cupsLangPuts(stdout, _(" FAIL\n"));

	    _cupsLangPuts(stdout,
	                  _("      **FAIL**  BAD ShortNickName - longer "
			    "than 31 chars.\n"
			    "                REF: Pages 64-65, section 5.3.\n"));
          }

	  errors ++;
	}
	else if (verbose > 0)
	  _cupsLangPuts(stdout, _("        PASS    ShortNickName\n"));
      }
      else if (ppdversion >= 43)
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPuts(stdout,
	                _("      **FAIL**  REQUIRED ShortNickName\n"
			  "                REF: Page 64-65, section 5.3.\n"));
        }

	errors ++;
      }

      if (ppd->patches != NULL && strchr(ppd->patches, '\"') &&
          strstr(ppd->patches, "*End"))
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPuts(stdout,
	                _("      **FAIL**  BAD JobPatchFile attribute in file\n"
	                  "                REF: Page 24, section 3.4.\n"));
        }

	errors ++;
      }

     /*
      * Check for page sizes without the corresponding ImageableArea or
      * PaperDimension values...
      */

      if (ppd->num_sizes == 0)
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPuts(stdout,
	                _("      **FAIL**  REQUIRED PageSize\n"
			  "                REF: Page 41, section 5.\n"
			  "                REF: Page 99, section 5.14.\n"));
        }

	errors ++;
      }
      else
      {
	for (j = 0, size = ppd->sizes; j < ppd->num_sizes; j ++, size ++)
	{
	 /*
	  * Don't check custom size...
	  */

	  if (!strcmp(size->name, "Custom"))
	    continue;

	 /*
	  * Check for ImageableArea...
	  */

          if (size->left == 0.0 && size->bottom == 0.0 &&
	      size->right == 0.0 && size->top == 0.0)
	  {
	    if (verbose >= 0)
	    {
	      if (!errors && !verbose)
		_cupsLangPuts(stdout, _(" FAIL\n"));

	      _cupsLangPrintf(stdout,
	                      _("      **FAIL**  REQUIRED ImageableArea for "
			        "PageSize %s\n"
				"                REF: Page 41, section 5.\n"
				"                REF: Page 102, section 5.15.\n"),
	        	      size->name);
            }

	    errors ++;
	  }

	 /*
	  * Check for PaperDimension...
	  */

          if (size->width == 0.0 && size->length == 0.0)
	  {
	    if (verbose >= 0)
	    {
	      if (!errors && !verbose)
		_cupsLangPuts(stdout, _(" FAIL\n"));

	      _cupsLangPrintf(stdout,
	                      _("      **FAIL**  REQUIRED PaperDimension "
			        "for PageSize %s\n"
				"                REF: Page 41, section 5.\n"
				"                REF: Page 103, section 5.15.\n"),
	                      size->name);
            }

	    errors ++;
	  }
	}
      }

     /*
      * Check for valid Resolution, JCLResolution, or SetResolution values...
      */

      if ((option = ppdFindOption(ppd, "Resolution")) == NULL)
	if ((option = ppdFindOption(ppd, "JCLResolution")) == NULL)
          option = ppdFindOption(ppd, "SetResolution");

      if (option != NULL)
      {
        for (j = option->num_choices, choice = option->choices; j > 0; j --, choice ++)
        {
	 /*
	  * Verify that all resolution options are of the form NNNdpi
	  * or NNNxNNNdpi...
	  */

          xdpi = strtol(choice->choice, (char **)&ptr, 10);
	  if (ptr > choice->choice && xdpi > 0)
	  {
	    if (*ptr == 'x')
	      ydpi = strtol(ptr + 1, (char **)&ptr, 10);
	    else
	      ydpi = xdpi;
	  }
	  else
	    ydpi = xdpi;

	  if (xdpi <= 0 || xdpi > 99999 || ydpi <= 0 || ydpi > 99999 ||
	      strcmp(ptr, "dpi"))
	  {
	    if (verbose >= 0)
	    {
	      if (!errors && !verbose)
		_cupsLangPuts(stdout, _(" FAIL\n"));

	      _cupsLangPrintf(stdout,
	                      _("      **FAIL**  Bad %s choice %s!\n"
			        "                REF: Page 84, section 5.9\n"),
	                      option->keyword, choice->choice);
            }

	    errors ++;
	  }
	}
      }

      if ((attr = ppdFindAttr(ppd, "1284DeviceID", NULL)) &&
          strcmp(attr->name, "1284DeviceID"))
      {
	if (verbose >= 0)
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPrintf(stdout,
	                  _("      **FAIL**  %s must be 1284DeviceID!\n"
			    "                REF: Page 72, section 5.5\n"),
			  attr->name);
        }

	errors ++;
      }

      errors = check_case(ppd, errors, verbose);

      if (!(warn & WARN_CONSTRAINTS))
        errors = check_constraints(ppd, errors, verbose, 0);

      if (!(warn & WARN_FILTERS))
        errors = check_filters(ppd, root, errors, verbose, 0);

      if (!(warn & WARN_PROFILES))
        errors = check_profiles(ppd, root, errors, verbose, 0);

      if (!(warn & WARN_SIZES))
	errors = check_sizes(ppd, errors, verbose, 0);

      if (!(warn & WARN_TRANSLATIONS))
        errors = check_translations(ppd, errors, verbose, 0);

      if (!(warn & WARN_DUPLEX))
        errors = check_duplex(ppd, errors, verbose, 0);

      if ((attr = ppdFindAttr(ppd, "cupsLanguages", NULL)) != NULL &&
	  attr->value)
      {
       /*
	* This file contains localizations, check for conformance of the
	* base translation...
	*/

        if ((attr = ppdFindAttr(ppd, "LanguageEncoding", NULL)) != NULL)
	{
	  if (!attr->value || strcmp(attr->value, "ISOLatin1"))
	  {
	    if (!errors && !verbose)
	      _cupsLangPuts(stdout, _(" FAIL\n"));

            if (verbose >= 0)
	      _cupsLangPrintf(stdout,
	                      _("      **FAIL**  Bad LanguageEncoding %s - "
			        "must be ISOLatin1!\n"),
	                      attr->value ? attr->value : "(null)");

            errors ++;
	  }

          if (!ppd->lang_version || strcmp(ppd->lang_version, "English"))
	  {
	    if (!errors && !verbose)
	      _cupsLangPuts(stdout, _(" FAIL\n"));

            if (verbose >= 0)
	      _cupsLangPrintf(stdout,
	                      _("      **FAIL**  Bad LanguageVersion %s - "
			        "must be English!\n"),
	                      ppd->lang_version ? ppd->lang_version : "(null)");

            errors ++;
	  }
	  
	 /*
	  * Loop through all options and choices...
	  */

	  for (option = ppdFirstOption(ppd);
	       option;
	       option = ppdNextOption(ppd))
	  {
	   /*
	    * Check for special characters outside A0 to BF, F7, or F8
	    * that are used for languages other than English.
	    */

	    for (ptr = option->text; *ptr; ptr ++)
	      if ((*ptr & 0x80) && (*ptr & 0xe0) != 0xa0 &&
		  (*ptr & 0xff) != 0xf7 && (*ptr & 0xff) != 0xf8)
		break;

	    if (*ptr)
	    {
	      if (!errors && !verbose)
		_cupsLangPuts(stdout, _(" FAIL\n"));

	      if (verbose >= 0)
		_cupsLangPrintf(stdout,
				_("      **FAIL**  Default translation "
				  "string for option %s contains 8-bit "
				  "characters!\n"),
				option->keyword);

	      errors ++;
	    }

	    for (j = 0; j < option->num_choices; j ++)
	    {
	     /*
	      * Check for special characters outside A0 to BF, F7, or F8
	      * that are used for languages other than English.
	      */

	      for (ptr = option->choices[j].text; *ptr; ptr ++)
		if ((*ptr & 0x80) && (*ptr & 0xe0) != 0xa0 &&
		    (*ptr & 0xff) != 0xf7 && (*ptr & 0xff) != 0xf8)
		  break;

	      if (*ptr)
	      {
		if (!errors && !verbose)
		  _cupsLangPuts(stdout, _(" FAIL\n"));

		if (verbose >= 0)
		  _cupsLangPrintf(stdout,
				  _("      **FAIL**  Default translation "
				    "string for option %s choice %s contains "
				    "8-bit characters!\n"),
				  option->keyword,
				  option->choices[j].choice);

		errors ++;
	      }
	    }
	  }
	}
      }

     /*
      * Final pass/fail notification...
      */

      if (errors)
	status = ERROR_CONFORMANCE;
      else if (!verbose)
	_cupsLangPuts(stdout, _(" PASS\n"));

      if (verbose >= 0)
      {
        check_basics(argv[i]);

	if (warn & WARN_CONSTRAINTS)
	  errors = check_constraints(ppd, errors, verbose, 1);

	if (warn & WARN_DEFAULTS)
	  errors = check_defaults(ppd, errors, verbose, 1);

	if (warn & WARN_PROFILES)
	  errors = check_profiles(ppd, root, errors, verbose, 1);

	if (warn & WARN_FILTERS)
	  errors = check_filters(ppd, root, errors, verbose, 1);

        if (warn & WARN_SIZES)
	  errors = check_sizes(ppd, errors, verbose, 1);
        else
	  errors = check_sizes(ppd, errors, verbose, 2);

	if (warn & WARN_TRANSLATIONS)
	  errors = check_translations(ppd, errors, verbose, 1);

	if (warn & WARN_DUPLEX)
	  errors = check_duplex(ppd, errors, verbose, 1);

       /*
        * Look for legacy duplex keywords...
	*/

	if ((option = ppdFindOption(ppd, "JCLDuplex")) == NULL)
	  if ((option = ppdFindOption(ppd, "EFDuplex")) == NULL)
	    option = ppdFindOption(ppd, "KD03Duplex");

	if (option)
	  _cupsLangPrintf(stdout,
			  _("        WARN    Duplex option keyword %s may not "
			    "work as expected and should be named Duplex!\n"
			    "                REF: Page 122, section 5.17\n"),
			  option->keyword);

       /*
	* Look for default keywords with no corresponding option...
	*/

	for (j = 0; j < ppd->num_attrs; j ++)
	{
	  attr = ppd->attrs[j];

          if (!strcmp(attr->name, "DefaultColorSpace") ||
	      !strcmp(attr->name, "DefaultColorSep") ||
	      !strcmp(attr->name, "DefaultFont") ||
	      !strcmp(attr->name, "DefaultHalftoneType") ||
	      !strcmp(attr->name, "DefaultImageableArea") ||
	      !strcmp(attr->name, "DefaultLeadingEdge") ||
	      !strcmp(attr->name, "DefaultOutputOrder") ||
	      !strcmp(attr->name, "DefaultPaperDimension") ||
	      !strcmp(attr->name, "DefaultResolution") ||
	      !strcmp(attr->name, "DefaultScreenProc") ||
	      !strcmp(attr->name, "DefaultTransfer"))
	    continue;

	  if (!strncmp(attr->name, "Default", 7) && 
	      !ppdFindOption(ppd, attr->name + 7))
            _cupsLangPrintf(stdout,
	                    _("        WARN    %s has no corresponding "
			      "options!\n"),
	                    attr->name);
	}

        ppdMarkDefaults(ppd);
	if (ppdConflicts(ppd))
	{
	  _cupsLangPuts(stdout,
	                _("        WARN    Default choices conflicting!\n"));

          show_conflicts(ppd);
        }

        if (ppdversion < 43)
	{
          _cupsLangPrintf(stdout,
	                  _("        WARN    Obsolete PPD version %.1f!\n"
			    "                REF: Page 42, section 5.2.\n"),
	        	  0.1f * ppdversion);
	}

        if (!ppd->lang_encoding && ppdversion < 41)
	{
	  _cupsLangPuts(stdout,
	                _("        WARN    LanguageEncoding required by PPD "
			  "4.3 spec.\n"
			  "                REF: Pages 56-57, section 5.3.\n"));
	}

        if (!ppd->manufacturer && ppdversion < 43)
	{
	  _cupsLangPuts(stdout,
	                _("        WARN    Manufacturer required by PPD "
			  "4.3 spec.\n"
			  "                REF: Pages 58-59, section 5.3.\n"));
	}

       /*
	* Treat a PCFileName attribute longer than 12 characters as
	* a warning and not a hard error...
	*/

	if (ppd->pcfilename && strlen(ppd->pcfilename) > 12)
	{
	  _cupsLangPuts(stdout,
	                _("        WARN    PCFileName longer than 8.3 in "
			  "violation of PPD spec.\n"
			  "                REF: Pages 61-62, section 5.3.\n"));
        }

        if (!ppd->shortnickname && ppdversion < 43)
	{
	  _cupsLangPuts(stdout,
	                _("        WARN    ShortNickName required by PPD "
			  "4.3 spec.\n"
			  "                REF: Pages 64-65, section 5.3.\n"));
	}

       /*
        * Check the Protocols line and flag PJL + BCP since TBCP is
	* usually used with PJL...
	*/

        if (ppd->protocols)
	{
	  if (strstr(ppd->protocols, "PJL") &&
	      strstr(ppd->protocols, "BCP") &&
	      !strstr(ppd->protocols, "TBCP"))
	  {
	    _cupsLangPuts(stdout,
	                  _("        WARN    Protocols contains both PJL "
			    "and BCP; expected TBCP.\n"
			    "                REF: Pages 78-79, section 5.7.\n"));
	  }

	  if (strstr(ppd->protocols, "PJL") &&
	      (!ppd->jcl_begin || !ppd->jcl_end || !ppd->jcl_ps))
	  {
	    _cupsLangPuts(stdout,
	                  _("        WARN    Protocols contains PJL but JCL "
			    "attributes are not set.\n"
			    "                REF: Pages 78-79, section 5.7.\n"));
	  }
	}

       /*
        * Check for options with a common prefix, e.g. Duplex and Duplexer,
	* which are errors according to the spec but won't cause problems
	* with CUPS specifically...
	*/

	for (j = 0, group = ppd->groups; j < ppd->num_groups; j ++, group ++)
	  for (k = 0, option = group->options; k < group->num_options; k ++, option ++)
	  {
	    len = strlen(option->keyword);

	    for (m = 0, group2 = ppd->groups;
		 m < ppd->num_groups;
		 m ++, group2 ++)
	      for (n = 0, option2 = group2->options;
	           n < group2->num_options;
		   n ++, option2 ++)
		if (option != option2 &&
	            len < strlen(option2->keyword) &&
	            !strncmp(option->keyword, option2->keyword, len))
		{
		  _cupsLangPrintf(stdout,
		                  _("        WARN    %s shares a common "
				    "prefix with %s\n"
				    "                REF: Page 15, section "
				    "3.2.\n"),
		                  option->keyword, option2->keyword);
        	}
	  }
      }

      if (verbose > 0)
      {
        if (errors)
          _cupsLangPrintf(stdout, _("    %d ERRORS FOUND\n"), errors);
	else
	  _cupsLangPuts(stdout, _("    NO ERRORS FOUND\n"));
      }

     /*
      * Then list the options, if "-v" was provided...
      */ 

      if (verbose > 1)
      {
	_cupsLangPrintf(stdout,
                        "\n"
		        "    language_level = %d\n"
			"    color_device = %s\n"
			"    variable_sizes = %s\n"
			"    landscape = %d\n",
			ppd->language_level,
			ppd->color_device ? "TRUE" : "FALSE",
			ppd->variable_sizes ? "TRUE" : "FALSE",
			ppd->landscape);

	switch (ppd->colorspace)
	{
	  case PPD_CS_CMYK :
              _cupsLangPuts(stdout, "    colorspace = PPD_CS_CMYK\n");
	      break;
	  case PPD_CS_CMY :
              _cupsLangPuts(stdout, "    colorspace = PPD_CS_CMY\n");
	      break;
	  case PPD_CS_GRAY :
              _cupsLangPuts(stdout, "    colorspace = PPD_CS_GRAY\n");
	      break;
	  case PPD_CS_RGB :
              _cupsLangPuts(stdout, "    colorspace = PPD_CS_RGB\n");
	      break;
	  default :
              _cupsLangPuts(stdout, "    colorspace = <unknown>\n");
	      break;
	}

	_cupsLangPrintf(stdout, "    num_emulations = %d\n",
			ppd->num_emulations);
	for (j = 0; j < ppd->num_emulations; j ++)
	  _cupsLangPrintf(stdout, "        emulations[%d] = %s\n",
	                  j, ppd->emulations[j].name);

	_cupsLangPrintf(stdout, "    lang_encoding = %s\n",
	                ppd->lang_encoding);
	_cupsLangPrintf(stdout, "    lang_version = %s\n",
	                ppd->lang_version);
	_cupsLangPrintf(stdout, "    modelname = %s\n", ppd->modelname);
	_cupsLangPrintf(stdout, "    ttrasterizer = %s\n",
        		ppd->ttrasterizer == NULL ? "None" : ppd->ttrasterizer);
	_cupsLangPrintf(stdout, "    manufacturer = %s\n",
	                ppd->manufacturer);
	_cupsLangPrintf(stdout, "    product = %s\n", ppd->product);
	_cupsLangPrintf(stdout, "    nickname = %s\n", ppd->nickname);
	_cupsLangPrintf(stdout, "    shortnickname = %s\n",
	                ppd->shortnickname);
	_cupsLangPrintf(stdout, "    patches = %d bytes\n",
        		ppd->patches == NULL ? 0 : (int)strlen(ppd->patches));

	_cupsLangPrintf(stdout, "    num_groups = %d\n", ppd->num_groups);
	for (j = 0, group = ppd->groups; j < ppd->num_groups; j ++, group ++)
	{
	  _cupsLangPrintf(stdout, "        group[%d] = %s\n",
	                  j, group->text);

	  for (k = 0, option = group->options; k < group->num_options; k ++, option ++)
	  {
	    _cupsLangPrintf(stdout,
	                    "            options[%d] = %s (%s) %s %s %.0f "
			    "(%d choices)\n",
	        	    k, option->keyword, option->text, uis[option->ui],
			    sections[option->section], option->order,
			    option->num_choices);

            if (!strcmp(option->keyword, "PageSize") ||
        	!strcmp(option->keyword, "PageRegion"))
            {
              for (m = option->num_choices, choice = option->choices;
		   m > 0;
		   m --, choice ++)
	      {
		size = ppdPageSize(ppd, choice->choice);

		if (size == NULL)
		  _cupsLangPrintf(stdout,
                                  "                %s (%s) = ERROR",
				  choice->choice, choice->text);
        	else
		  _cupsLangPrintf(stdout,
                                  "                %s (%s) = %.2fx%.2fin "
				  "(%.1f,%.1f,%.1f,%.1f)",
		        	  choice->choice, choice->text,
				  size->width / 72.0, size->length / 72.0,
				  size->left / 72.0, size->bottom / 72.0,
				  size->right / 72.0, size->top / 72.0);

        	if (!strcmp(option->defchoice, choice->choice))
		  _cupsLangPuts(stdout, " *\n");
		else
		  _cupsLangPuts(stdout, "\n");
              }
	    }
	    else
	    {
	      for (m = option->num_choices, choice = option->choices;
		   m > 0;
		   m --, choice ++)
	      {
		_cupsLangPrintf(stdout, "                %s (%s)",
		                choice->choice, choice->text);

        	if (!strcmp(option->defchoice, choice->choice))
		  _cupsLangPuts(stdout, " *\n");
		else
		  _cupsLangPuts(stdout, "\n");
	      }
            }
	  }
	}

	_cupsLangPrintf(stdout, "    num_consts = %d\n",
	                ppd->num_consts);
	for (j = 0; j < ppd->num_consts; j ++)
	  _cupsLangPrintf(stdout,
                	  "        consts[%d] = *%s %s *%s %s\n",
        		  j, ppd->consts[j].option1, ppd->consts[j].choice1,
			  ppd->consts[j].option2, ppd->consts[j].choice2);

	_cupsLangPrintf(stdout, "    num_profiles = %d\n",
	                ppd->num_profiles);
	for (j = 0; j < ppd->num_profiles; j ++)
	  _cupsLangPrintf(stdout,
                	  "        profiles[%d] = %s/%s %.3f %.3f "
			  "[ %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f ]\n",
        		  j, ppd->profiles[j].resolution,
			  ppd->profiles[j].media_type,
			  ppd->profiles[j].gamma, ppd->profiles[j].density,
			  ppd->profiles[j].matrix[0][0],
			  ppd->profiles[j].matrix[0][1],
			  ppd->profiles[j].matrix[0][2],
			  ppd->profiles[j].matrix[1][0],
			  ppd->profiles[j].matrix[1][1],
			  ppd->profiles[j].matrix[1][2],
			  ppd->profiles[j].matrix[2][0],
			  ppd->profiles[j].matrix[2][1],
			  ppd->profiles[j].matrix[2][2]);

	_cupsLangPrintf(stdout, "    num_fonts = %d\n", ppd->num_fonts);
	for (j = 0; j < ppd->num_fonts; j ++)
	  _cupsLangPrintf(stdout, "        fonts[%d] = %s\n",
	                  j, ppd->fonts[j]);

	_cupsLangPrintf(stdout, "    num_attrs = %d\n", ppd->num_attrs);
	for (j = 0; j < ppd->num_attrs; j ++)
	  _cupsLangPrintf(stdout,
	                  "        attrs[%d] = %s %s%s%s: \"%s\"\n", j,
	        	  ppd->attrs[j]->name, ppd->attrs[j]->spec,
			  ppd->attrs[j]->text[0] ? "/" : "",
			  ppd->attrs[j]->text,
			  ppd->attrs[j]->value ?
			      ppd->attrs[j]->value : "(null)");
      }

      ppdClose(ppd);
    }

  if (!files)
    usage();

  return (status);
}


/*
 * 'check_basics()' - Check for CR LF, mixed line endings, and blank lines.
 */

static void
check_basics(const char *filename)	/* I - PPD file to check */
{
  cups_file_t	*fp;			/* File pointer */
  int		ch;			/* Current character */
  int		col,			/* Current column */
		whitespace;		/* Only seen whitespace? */
  int		eol;			/* Line endings */
  int		linenum;		/* Line number */
  int		mixed;			/* Mixed line endings? */


  if ((fp = cupsFileOpen(filename, "r")) == NULL)
    return;

  linenum    = 1;
  col        = 0;
  eol        = EOL_NONE;
  mixed      = 0;
  whitespace = 1;

  while ((ch = cupsFileGetChar(fp)) != EOF)
  {
    if (ch == '\r' || ch == '\n')
    {
      if (ch == '\n')
      {
	if (eol == EOL_NONE)
	  eol = EOL_LF;
	else if (eol != EOL_LF)
	  mixed = 1;
      }
      else if (ch == '\r')
      {
	if (cupsFilePeekChar(fp) == '\n')
	{
	  cupsFileGetChar(fp);

          if (eol == EOL_NONE)
	    eol = EOL_CRLF;
	  else if (eol != EOL_CRLF)
	    mixed = 1;
	}
	else if (eol == EOL_NONE)
	  eol = EOL_CR;
        else if (eol != EOL_CR)
	  mixed = 1;
      }
      
      if (col > 0 && whitespace)
	_cupsLangPrintf(stdout,
		        _("        WARN    Line %d only contains whitespace!\n"),
			linenum);

      linenum ++;
      col        = 0;
      whitespace = 1;
    }
    else
    {
      if (ch != ' ' && ch != '\t')
        whitespace = 0;

      col ++;
    }
  }

  if (mixed)
    _cupsLangPuts(stdout,
		  _("        WARN    File contains a mix of CR, LF, and "
		    "CR LF line endings!\n"));

  if (eol == EOL_CRLF)
    _cupsLangPuts(stdout,
		  _("        WARN    Non-Windows PPD files should use lines "
		    "ending with only LF, not CR LF!\n"));

  cupsFileClose(fp);
}


/*
 * 'check_constraints()' - Check UIConstraints in the PPD file.
 */

static int				/* O - Errors found */
check_constraints(ppd_file_t *ppd,	/* I - PPD file */
                  int        errors,	/* I - Errors found */
                  int        verbose,	/* I - Verbosity level */
                  int        warn)	/* I - Warnings only? */
{
  int			i;		/* Looping var */
  const char		*prefix;	/* WARN/FAIL prefix */
  ppd_const_t		*c;		/* Current UIConstraints data */
  ppd_attr_t		*constattr;	/* Current cupsUIConstraints attribute */
  const char		*vptr;		/* Pointer into constraint value */
  char			option[PPD_MAX_NAME],
  					/* Option name/MainKeyword */
			choice[PPD_MAX_NAME],
					/* Choice/OptionKeyword */
			*ptr;		/* Pointer into option or choice */
  int			num_options;	/* Number of options */
  cups_option_t		*options;	/* Options */
  ppd_option_t		*o;		/* PPD option */


  prefix = warn ? "  WARN  " : "**FAIL**";


 /*
  * See what kind of constraint data we have in the PPD...
  */

  if ((constattr = ppdFindAttr(ppd, "cupsUIConstraints", NULL)) != NULL)
  {
   /*
    * Check new-style cupsUIConstraints data...
    */

    for (; constattr;
         constattr = ppdFindNextAttr(ppd, "cupsUIConstraints", NULL))
    {
      if (!constattr->value)
      {
	if (!warn && !errors && !verbose)
	  _cupsLangPuts(stdout, _(" FAIL\n"));

	_cupsLangPrintf(stdout,
			_("      %s  Empty cupsUIConstraints %s!\n"),
			prefix, constattr->spec);

	if (!warn)
	  errors ++;

        continue;
      }

      for (i = 0, vptr = strchr(constattr->value, '*');
           vptr;
	   i ++, vptr = strchr(vptr + 1, '*'));

      if (i == 0)
      {
	if (!warn && !errors && !verbose)
	  _cupsLangPuts(stdout, _(" FAIL\n"));

	_cupsLangPrintf(stdout,
			_("      %s  Bad cupsUIConstraints %s: \"%s\"!\n"),
			prefix, constattr->spec, constattr->value);

	if (!warn)
	  errors ++;

        continue;
      }

      cupsArraySave(ppd->sorted_attrs);

      if (constattr->spec[0] &&
          !ppdFindAttr(ppd, "cupsUIResolver", constattr->spec))
      {
	if (!warn && !errors && !verbose)
	  _cupsLangPuts(stdout, _(" FAIL\n"));

	_cupsLangPrintf(stdout,
			_("      %s  Missing cupsUIResolver %s!\n"),
			prefix, constattr->spec);

	if (!warn)
	  errors ++;
      }

      cupsArrayRestore(ppd->sorted_attrs);

      num_options = 0;
      options     = NULL;

      for (vptr = strchr(constattr->value, '*');
           vptr;
	   vptr = strchr(vptr, '*'))
      {
       /*
        * Extract "*Option Choice" or just "*Option"...
	*/

        for (vptr ++, ptr = option; *vptr && !isspace(*vptr & 255); vptr ++)
	  if (ptr < (option + sizeof(option) - 1))
	    *ptr++ = *vptr;

        *ptr = '\0';

        while (isspace(*vptr & 255))
	  vptr ++;

        if (*vptr == '*')
	  choice[0] = '\0';
	else
	{
	  for (ptr = choice; *vptr && !isspace(*vptr & 255); vptr ++)
	    if (ptr < (choice + sizeof(choice) - 1))
	      *ptr++ = *vptr;

	  *ptr = '\0';
	}

        if (!strncasecmp(option, "Custom", 6) && !strcasecmp(choice, "True"))
	{
	  _cups_strcpy(option, option + 6);
	  strcpy(choice, "Custom");
	}

        if ((o = ppdFindOption(ppd, option)) == NULL)
	{
	  if (!warn && !errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPrintf(stdout,
			  _("      %s  Missing option %s in "
			    "cupsUIConstraints %s: \"%s\"!\n"),
			  prefix, option, constattr->spec, constattr->value);
	  
	  if (!warn)
	    errors ++;

	  continue;
	}

        if (choice[0] && !ppdFindChoice(o, choice))
	{
	  if (!warn && !errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPrintf(stdout,
			  _("      %s  Missing choice *%s %s in "
			    "cupsUIConstraints %s: \"%s\"!\n"),
			  prefix, option, choice, constattr->spec,
			  constattr->value);

	  if (!warn)
	    errors ++;

	  continue;
	}

        if (choice[0])
	  num_options = cupsAddOption(option, choice, num_options, &options);
	else
	{
	  for (i = 0; i < o->num_choices; i ++)
	    if (strcasecmp(o->choices[i].choice, "None") &&
	        strcasecmp(o->choices[i].choice, "Off") &&
	        strcasecmp(o->choices[i].choice, "False"))
            {
	      num_options = cupsAddOption(option, o->choices[i].choice,
	                                  num_options, &options);
              break;
	    }
	}
      }

     /*
      * Resolvers must list at least two options...
      */

      if (num_options < 2)
      {
	if (!warn && !errors && !verbose)
	  _cupsLangPuts(stdout, _(" FAIL\n"));

	_cupsLangPrintf(stdout,
			_("      %s  cupsUIResolver %s does not list at least "
			  "two different options!\n"),
			prefix, constattr->spec);

	if (!warn)
	  errors ++;
      }

     /*
      * Test the resolver...
      */

      if (!cupsResolveConflicts(ppd, NULL, NULL, &num_options, &options))
      {
	if (!warn && !errors && !verbose)
	  _cupsLangPuts(stdout, _(" FAIL\n"));

	_cupsLangPrintf(stdout,
			_("      %s  cupsUIResolver %s causes a loop!\n"),
			prefix, constattr->spec);

	if (!warn)
	  errors ++;
      }

      cupsFreeOptions(num_options, options);
    }
  }
  else
  {
   /*
    * Check old-style [Non]UIConstraints data...
    */

    for (i = ppd->num_consts, c = ppd->consts; i > 0; i --, c ++)
    {
      if (!strncasecmp(c->option1, "Custom", 6) &&
          !strcasecmp(c->choice1, "True"))
      {
	strcpy(option, c->option1 + 6);
	strcpy(choice, "Custom");
      }
      else
      {
	strcpy(option, c->option1);
	strcpy(choice, c->choice1);
      }

      if ((o = ppdFindOption(ppd, option)) == NULL)
      {
	if (!warn && !errors && !verbose)
	  _cupsLangPuts(stdout, _(" FAIL\n"));

	_cupsLangPrintf(stdout,
			_("      %s  Missing option %s in "
			  "UIConstraints \"*%s %s *%s %s\"!\n"),
			prefix, c->option1,
			c->option1, c->choice1, c->option2, c->choice2);

	if (!warn)
	  errors ++;
      }
      else if (choice[0] && !ppdFindChoice(o, choice))
      {
	if (!warn && !errors && !verbose)
	  _cupsLangPuts(stdout, _(" FAIL\n"));

	_cupsLangPrintf(stdout,
			_("      %s  Missing choice *%s %s in "
			  "UIConstraints \"*%s %s *%s %s\"!\n"),
			prefix, c->option1, c->choice1,
			c->option1, c->choice1, c->option2, c->choice2);

	if (!warn)
	  errors ++;
      }

      if (!strncasecmp(c->option2, "Custom", 6) &&
          !strcasecmp(c->choice2, "True"))
      {
	strcpy(option, c->option2 + 6);
	strcpy(choice, "Custom");
      }
      else
      {
	strcpy(option, c->option2);
	strcpy(choice, c->choice2);
      }

      if ((o = ppdFindOption(ppd, option)) == NULL)
      {
	if (!warn && !errors && !verbose)
	  _cupsLangPuts(stdout, _(" FAIL\n"));

	_cupsLangPrintf(stdout,
			_("      %s  Missing option %s in "
			  "UIConstraints \"*%s %s *%s %s\"!\n"),
			prefix, c->option2,
			c->option1, c->choice1, c->option2, c->choice2);

	if (!warn)
	  errors ++;
      }
      else if (choice[0] && !ppdFindChoice(o, choice))
      {
	if (!warn && !errors && !verbose)
	  _cupsLangPuts(stdout, _(" FAIL\n"));

	_cupsLangPrintf(stdout,
			_("      %s  Missing choice *%s %s in "
			  "UIConstraints \"*%s %s *%s %s\"!\n"),
			prefix, c->option2, c->choice2,
			c->option1, c->choice1, c->option2, c->choice2);

	if (!warn)
	  errors ++;
      }
    }
  }

  return (errors);
}


/*
 * 'check_case()' - Check that there are no duplicate groups, options,
 *                  or choices that differ only by case.
 */

static int				/* O - Errors found */
check_case(ppd_file_t *ppd,		/* I - PPD file */
           int        errors,		/* I - Errors found */
	   int        verbose)		/* I - Verbosity level */
{
  int		i, j;			/* Looping vars */
  ppd_group_t	*groupa,		/* First group */
		*groupb;		/* Second group */
  ppd_option_t	*optiona,		/* First option */
		*optionb;		/* Second option */
  ppd_choice_t	*choicea,		/* First choice */
		*choiceb;		/* Second choice */


 /*
  * Check that the groups do not have any duplicate names...
  */

  for (i = ppd->num_groups, groupa = ppd->groups; i > 1; i --, groupa ++)
    for (j = i - 1, groupb = groupa + 1; j > 0; j --, groupb ++)
      if (!strcasecmp(groupa->name, groupb->name))
      {
	if (!errors && !verbose)
	  _cupsLangPuts(stdout, _(" FAIL\n"));

	if (verbose >= 0)
	  _cupsLangPrintf(stdout,
			  _("      **FAIL**  Group names %s and %s differ only "
			    "by case!\n"),
			  groupa->name, groupb->name);

	errors ++;
      }

 /*
  * Check that the options do not have any duplicate names...
  */

  for (optiona = ppdFirstOption(ppd); optiona; optiona = ppdNextOption(ppd))
  {
    cupsArraySave(ppd->options);
    for (optionb = ppdNextOption(ppd); optionb; optionb = ppdNextOption(ppd))
      if (!strcasecmp(optiona->keyword, optionb->keyword))
      {
	if (!errors && !verbose)
	  _cupsLangPuts(stdout, _(" FAIL\n"));

	if (verbose >= 0)
	  _cupsLangPrintf(stdout,
			  _("      **FAIL**  Option names %s and %s differ only "
			    "by case!\n"),
			  optiona->keyword, optionb->keyword);

	errors ++;
      }
    cupsArrayRestore(ppd->options);

   /*
    * Then the choices...
    */

    for (i = optiona->num_choices, choicea = optiona->choices;
         i > 1;
	 i --, choicea ++)
      for (j = i - 1, choiceb = choicea + 1; j > 0; j --, choiceb ++)
        if (!strcmp(choicea->choice, choiceb->choice))
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  if (verbose >= 0)
	    _cupsLangPrintf(stdout,
			    _("      **FAIL**  Multiple occurrences of %s "
			      "choice name %s!\n"),
			    optiona->keyword, choicea->choice);

	  errors ++;

	  choicea ++;
	  i --;
	  break;
	}
        else if (!strcasecmp(choicea->choice, choiceb->choice))
	{
	  if (!errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  if (verbose >= 0)
	    _cupsLangPrintf(stdout,
			    _("      **FAIL**  %s choice names %s and %s "
			      "differ only by case!\n"),
			    optiona->keyword, choicea->choice, choiceb->choice);

	  errors ++;
	}
  }

 /*
  * Return the number of errors found...
  */

  return (errors);
}


/*
 * 'check_defaults()' - Check default option keywords in the PPD file.
 */

static int				/* O - Errors found */
check_defaults(ppd_file_t *ppd,		/* I - PPD file */
	       int        errors,	/* I - Errors found */
	       int        verbose,	/* I - Verbosity level */
	       int        warn)		/* I - Warnings only? */
{
  int		j, k;			/* Looping vars */
  ppd_attr_t	*attr;			/* PPD attribute */
  ppd_option_t	*option;		/* Standard UI option */
  const char	*prefix;		/* WARN/FAIL prefix */


  prefix = warn ? "  WARN  " : "**FAIL**";

  for (j = 0; j < ppd->num_attrs; j ++)
  {
    attr = ppd->attrs[j];

    if (!strcmp(attr->name, "DefaultColorSpace") ||
	!strcmp(attr->name, "DefaultFont") ||
	!strcmp(attr->name, "DefaultHalftoneType") ||
	!strcmp(attr->name, "DefaultImageableArea") ||
	!strcmp(attr->name, "DefaultLeadingEdge") ||
	!strcmp(attr->name, "DefaultOutputOrder") ||
	!strcmp(attr->name, "DefaultPaperDimension") ||
	!strcmp(attr->name, "DefaultResolution") ||
	!strcmp(attr->name, "DefaultTransfer"))
      continue;

    if (!strncmp(attr->name, "Default", 7))
    {
      if ((option = ppdFindOption(ppd, attr->name + 7)) != NULL &&
	  strcmp(attr->value, "Unknown"))
      {
       /*
	* Check that the default option value matches a choice...
	*/

	for (k = 0; k < option->num_choices; k ++)
	  if (!strcmp(option->choices[k].choice, attr->value))
	    break;

	if (k >= option->num_choices)
	{
	  if (!warn && !errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  if (verbose >= 0)
	    _cupsLangPrintf(stdout,
			    _("      %s  %s %s does not exist!\n"),
			    prefix, attr->name, attr->value);

          if (!warn)
	    errors ++;
	}
      }
    }
  }

  return (errors);
}


/*
 * 'check_duplex()' - Check duplex keywords in the PPD file.
 */

static int				/* O - Errors found */
check_duplex(ppd_file_t *ppd,		/* I - PPD file */
             int        errors,		/* I - Error found */
	     int        verbose,	/* I - Verbosity level */
             int        warn)		/* I - Warnings only? */
{
  int		i;			/* Looping var */
  ppd_option_t	*option;		/* PPD option */
  ppd_choice_t	*choice;		/* Current choice */
  const char	*prefix;		/* Message prefix */


  prefix = warn ? "  WARN  " : "**FAIL**";

 /*
  * Check for a duplex option, and for standard values...
  */

  if ((option = ppdFindOption(ppd, "Duplex")) != NULL)
  {
    if (!ppdFindChoice(option, "None"))
    {
      if (verbose >= 0)
      {
	if (!warn && !errors && !verbose)
	  _cupsLangPuts(stdout, _(" FAIL\n"));

	_cupsLangPrintf(stdout,
			_("      %s  REQUIRED %s does not define "
			  "choice None!\n"
			  "                REF: Page 122, section 5.17\n"),
			prefix, option->keyword);
      }

      if (!warn)
	errors ++;
    }

    for (i = option->num_choices, choice = option->choices;
	 i > 0;
	 i --, choice ++)
      if (strcmp(choice->choice, "None") &&
	  strcmp(choice->choice, "DuplexNoTumble") &&
	  strcmp(choice->choice, "DuplexTumble") &&
	  strcmp(choice->choice, "SimplexTumble"))
      {
	if (verbose >= 0)
	{
	  if (!warn && !errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  _cupsLangPrintf(stdout,
			  _("      %s  Bad %s choice %s!\n"
			    "                REF: Page 122, section 5.17\n"),
			  prefix, option->keyword, choice->choice);
	}

	if (!warn)
	  errors ++;
      }
  }

  return (errors);
}


/*
 * 'check_filters()' - Check filters in the PPD file.
 */

static int				/* O - Errors found */
check_filters(ppd_file_t *ppd,		/* I - PPD file */
              const char *root,		/* I - Root directory */
	      int        errors,	/* I - Errors found */
	      int        verbose,	/* I - Verbosity level */
	      int        warn)		/* I - Warnings only? */
{
  int		i;			/* Looping var */
  ppd_attr_t	*attr;			/* PPD attribute */
  const char	*ptr;			/* Pointer into string */
  char		super[16],		/* Super-type for filter */
		type[256],		/* Type for filter */
		program[1024],		/* Program/filter name */
		pathprog[1024];		/* Complete path to program/filter */
  int		cost;			/* Cost of filter */
  const char	*prefix;		/* WARN/FAIL prefix */


  prefix = warn ? "  WARN  " : "**FAIL**";

 /*
  * cupsFilter
  */

  for (i = 0; i < ppd->num_filters; i ++)
  {
    if (sscanf(ppd->filters[i], "%15[^/]/%255s%d%*[ \t]%1023[^\n]", super, type,
               &cost, program) != 4)
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout,
			_("      %s  Bad cupsFilter value \"%s\"!\n"),
			prefix, ppd->filters[i]);

      if (!warn)
        errors ++;
    }
    else if (strcmp(program, "-"))
    {
      if (program[0] == '/')
	snprintf(pathprog, sizeof(pathprog), "%s%s", root, program);
      else
      {
	if ((ptr = getenv("CUPS_SERVERBIN")) == NULL)
	  ptr = CUPS_SERVERBIN;

	if (*ptr == '/' || !*root)
	  snprintf(pathprog, sizeof(pathprog), "%s%s/filter/%s", root, ptr,
		   program);
	else
	  snprintf(pathprog, sizeof(pathprog), "%s/%s/filter/%s", root, ptr,
		   program);
      }

      if (access(pathprog, X_OK))
      {
	if (!warn && !errors && !verbose)
	  _cupsLangPuts(stdout, _(" FAIL\n"));

	if (verbose >= 0)
	  _cupsLangPrintf(stdout, _("      %s  Missing cupsFilter "
				    "file \"%s\"\n"), prefix, program);

	if (!warn)
	  errors ++;
      }
      else
        errors = valid_path("cupsFilter", pathprog, errors, verbose, warn);
    }
  }

 /*
  * cupsPreFilter
  */

  for (attr = ppdFindAttr(ppd, "cupsPreFilter", NULL);
       attr;
       attr = ppdFindNextAttr(ppd, "cupsPreFilter", NULL))
  {
    if (strcmp(attr->name, "cupsPreFilter"))
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout,
			_("      %s  Bad spelling of %s - should be %s!\n"),
			prefix, attr->name, "cupsPreFilter");

      if (!warn)
        errors ++;
    }

    if (!attr->value ||
	sscanf(attr->value, "%15[^/]/%255s%d%*[ \t]%1023[^\n]", super, type,
	       &cost, program) != 4)
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout,
			_("      %s  Bad cupsPreFilter value \"%s\"!\n"),
			prefix, attr->value ? attr->value : "");

      if (!warn)
        errors ++;
    }
    else if (strcmp(program, "-"))
    {
      if (program[0] == '/')
	snprintf(pathprog, sizeof(pathprog), "%s%s", root, program);
      else
      {
	if ((ptr = getenv("CUPS_SERVERBIN")) == NULL)
	  ptr = CUPS_SERVERBIN;

	if (*ptr == '/' || !*root)
	  snprintf(pathprog, sizeof(pathprog), "%s%s/filter/%s", root, ptr,
		   program);
	else
	  snprintf(pathprog, sizeof(pathprog), "%s/%s/filter/%s", root, ptr,
		   program);
      }

      if (access(pathprog, X_OK))
      {
	if (!warn && !errors && !verbose)
	  _cupsLangPuts(stdout, _(" FAIL\n"));

	if (verbose >= 0)
	  _cupsLangPrintf(stdout, _("      %s  Missing cupsPreFilter "
				    "file \"%s\"\n"), prefix, program);

        if (!warn)
	  errors ++;
      }
      else
        errors = valid_path("cupsPreFilter", pathprog, errors, verbose, warn);
    }
  }

#ifdef __APPLE__
 /*
  * APDialogExtension
  */

  for (attr = ppdFindAttr(ppd, "APDialogExtension", NULL); 
       attr != NULL; 
       attr = ppdFindNextAttr(ppd, "APDialogExtension", NULL))
  {
    if (strcmp(attr->name, "APDialogExtension"))
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout,
			_("      %s  Bad spelling of %s - should be %s!\n"),
			prefix, attr->name, "APDialogExtension");

      if (!warn)
        errors ++;
    }

    if (!attr->value || access(attr->value, 0))
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout, _("      %s  Missing "
				  "APDialogExtension file \"%s\"\n"),
			prefix, attr->value ? attr->value : "<NULL>");

      if (!warn)
	errors ++;
    }
    else
      errors = valid_path("APDialogExtension", attr->value, errors, verbose,
                          warn);
  }

 /*
  * APPrinterIconPath
  */

  if ((attr = ppdFindAttr(ppd, "APPrinterIconPath", NULL)) != NULL)
  {
    if (strcmp(attr->name, "APPrinterIconPath"))
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout,
			_("      %s  Bad spelling of %s - should be %s!\n"),
			prefix, attr->name, "APPrinterIconPath");

      if (!warn)
        errors ++;
    }

    if (!attr->value || access(attr->value, 0))
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout, _("      %s  Missing "
				  "APPrinterIconPath file \"%s\"\n"),
			prefix, attr->value ? attr->value : "<NULL>");

      if (!warn)
	errors ++;
    }
    else
      errors = valid_path("APPrinterIconPath", attr->value, errors, verbose,
                          warn);
  }

 /*
  * APPrinterLowInkTool
  */

  if ((attr = ppdFindAttr(ppd, "APPrinterLowInkTool", NULL)) != NULL)
  {
    if (strcmp(attr->name, "APPrinterLowInkTool"))
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout,
			_("      %s  Bad spelling of %s - should be %s!\n"),
			prefix, attr->name, "APPrinterLowInkTool");

      if (!warn)
        errors ++;
    }

    if (!attr->value || access(attr->value, 0))
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout, _("      %s  Missing "
				  "APPrinterLowInkTool file \"%s\"\n"),
			prefix, attr->value ? attr->value : "<NULL>");

      if (!warn)
	errors ++;
    }
    else
      errors = valid_path("APPrinterLowInkTool", attr->value, errors, verbose,
                          warn);
  }

 /*
  * APPrinterUtilityPath
  */

  if ((attr = ppdFindAttr(ppd, "APPrinterUtilityPath", NULL)) != NULL)
  {
    if (strcmp(attr->name, "APPrinterUtilityPath"))
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout,
			_("      %s  Bad spelling of %s - should be %s!\n"),
			prefix, attr->name, "APPrinterUtilityPath");

      if (!warn)
        errors ++;
    }

    if (!attr->value || access(attr->value, 0))
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout, _("      %s  Missing "
				  "APPrinterUtilityPath file \"%s\"\n"),
			prefix, attr->value ? attr->value : "<NULL>");

      if (!warn)
	errors ++;
    }
    else
      errors = valid_path("APPrinterUtilityPath", attr->value, errors, verbose,
                          warn);
  }

 /*
  * APScanAppBundleID and APScanAppPath
  */

  if ((attr = ppdFindAttr(ppd, "APScanAppPath", NULL)) != NULL)
  {
    if (strcmp(attr->name, "APScanAppPath"))
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout,
			_("      %s  Bad spelling of %s - should be %s!\n"),
			prefix, attr->name, "APScanAppPath");

      if (!warn)
        errors ++;
    }

    if (!attr->value || access(attr->value, 0))
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout, _("      %s  Missing "
				  "APScanAppPath file \"%s\"\n"),
			prefix, attr->value ? attr->value : "<NULL>");

      if (!warn)
	errors ++;
    }
    else
      errors = valid_path("APScanAppPath", attr->value, errors, verbose,
                          warn);

    if (ppdFindAttr(ppd, "APScanAppBundleID", NULL))
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout, _("      %s  Cannot provide both "
				  "APScanAppPath and APScanAppBundleID!\n"),
			prefix);

      if (!warn)
	errors ++;
    }
  }
#endif	/* __APPLE__ */

  return (errors);
}


/*
 * 'check_profiles()' - Check ICC color profiles in the PPD file.
 */

static int				/* O - Errors found */
check_profiles(ppd_file_t *ppd,		/* I - PPD file */
               const char *root,	/* I - Root directory */
	       int        errors,	/* I - Errors found */
	       int        verbose,	/* I - Verbosity level */
	       int        warn)		/* I - Warnings only? */
{
  int		i;			/* Looping var */
  ppd_attr_t	*attr;			/* PPD attribute */
  const char	*ptr;			/* Pointer into string */
  const char	*prefix;		/* WARN/FAIL prefix */
  char		filename[1024];		/* Profile filename */
  int		num_profiles = 0;	/* Number of profiles */
  unsigned	hash,			/* Current hash value */
		hashes[1000];		/* Hash values of profile names */
  const char	*specs[1000];		/* Specifiers for profiles */


  prefix = warn ? "  WARN  " : "**FAIL**";

  for (attr = ppdFindAttr(ppd, "cupsICCProfile", NULL);
       attr;
       attr = ppdFindNextAttr(ppd, "cupsICCProfile", NULL))
  {
   /*
    * Check for valid selector...
    */

    for (i = 0, ptr = strchr(attr->spec, '.'); ptr; ptr = strchr(ptr + 1, '.'))
      i ++;

    if (!attr->value || i < 2)
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout,
			_("      %s  Bad cupsICCProfile %s!\n"),
			prefix, attr->spec);

      if (!warn)
        errors ++;

      continue;
    }

   /*
    * Check for valid profile filename...
    */

    if (attr->value[0] == '/')
      snprintf(filename, sizeof(filename), "%s%s", root, attr->value);
    else
    {
      if ((ptr = getenv("CUPS_DATADIR")) == NULL)
	ptr = CUPS_DATADIR;

      if (*ptr == '/' || !*root)
	snprintf(filename, sizeof(filename), "%s%s/profiles/%s", root, ptr,
		 attr->value);
      else
	snprintf(filename, sizeof(filename), "%s/%s/profiles/%s", root, ptr,
		 attr->value);
    }

    if (access(filename, 0))
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout, _("      %s  Missing cupsICCProfile "
				  "file \"%s\"!\n"), prefix, attr->value);

      if (!warn)
	errors ++;
    }
    else
      errors = valid_path("cupsICCProfile", filename, errors, verbose, warn);

   /*
    * Check for hash collisions...
    */

    hash = _ppdHashName(attr->spec);

    if (num_profiles > 0)
    {
      for (i = 0; i < num_profiles; i ++)
	if (hashes[i] == hash)
	  break;

      if (i < num_profiles)
      {
	if (!warn && !errors && !verbose)
	  _cupsLangPuts(stdout, _(" FAIL\n"));

	if (verbose >= 0)
	  _cupsLangPrintf(stdout,
			  _("      %s  cupsICCProfile %s hash value "
			    "collides with %s!\n"), prefix, attr->spec,
			  specs[i]);

	if (!warn)
	  errors ++;
      }
    }

   /*
    * Remember up to 1000 profiles...
    */

    if (num_profiles < 1000)
    {
      hashes[num_profiles] = hash;
      specs[num_profiles]  = attr->spec;
      num_profiles ++;
    }
  }

  return (errors);
}


/*
 * 'check_sizes()' - Check media sizes in the PPD file.
 */

static int				/* O - Errors found */
check_sizes(ppd_file_t *ppd,		/* I - PPD file */
	    int        errors,		/* I - Errors found */
	    int        verbose,		/* I - Verbosity level */
	    int        warn)		/* I - Warnings only? */
{
  int		i;			/* Looping vars */
  ppd_size_t	*size;			/* Current size */
  int		width,			/* Custom width */
		length;			/* Custom length */
  char		name[PPD_MAX_NAME],	/* Size name without dot suffix */
		*nameptr;		/* Pointer into name */
  const char	*prefix;		/* WARN/FAIL prefix */
  ppd_option_t	*page_size,		/* PageSize option */
		*page_region;		/* PageRegion option */


  prefix = warn ? "  WARN  " : "**FAIL**";

  if ((page_size = ppdFindOption(ppd, "PageSize")) == NULL && warn != 2)
  {
    if (!warn && !errors && !verbose)
      _cupsLangPuts(stdout, _(" FAIL\n"));

    if (verbose >= 0)
      _cupsLangPrintf(stdout,
		      _("      %s  Missing REQUIRED PageSize option!\n"
		        "                REF: Page 99, section 5.14.\n"),
		      prefix);

    if (!warn)
      errors ++;
  }

  if ((page_region = ppdFindOption(ppd, "PageRegion")) == NULL && warn != 2)
  {
    if (!warn && !errors && !verbose)
      _cupsLangPuts(stdout, _(" FAIL\n"));

    if (verbose >= 0)
      _cupsLangPrintf(stdout,
		      _("      %s  Missing REQUIRED PageRegion option!\n"
		        "                REF: Page 100, section 5.14.\n"),
		      prefix);

    if (!warn)
      errors ++;
  }

  for (i = ppd->num_sizes, size = ppd->sizes; i > 0; i --, size ++)
  {
   /*
    * Check that the size name is standard...
    */

    if (!strcmp(size->name, "Custom"))
    {
     /*
      * Skip custom page size...
      */

      continue;
    }
    else if (warn != 2 && size->name[0] == 'w' &&
             sscanf(size->name, "w%dh%d", &width, &length) == 2)
    {
     /*
      * Validate device-specific size wNNNhNNN should have proper width and
      * length...
      */

      if (fabs(width - size->width) >= 1.0 ||
          fabs(length - size->length) >= 1.0)
      {
	if (!warn && !errors && !verbose)
	  _cupsLangPuts(stdout, _(" FAIL\n"));

	if (verbose >= 0)
	  _cupsLangPrintf(stdout,
			  _("      %s  Size \"%s\" has unexpected dimensions "
			    "(%gx%g)!\n"),
			  prefix, size->name, size->width, size->length);

	if (!warn)
	  errors ++;
      }
    }
    else if (warn && verbose >= 0)
    {
     /*
      * Lookup the size name in the standard size table...
      */

      strlcpy(name, size->name, sizeof(name));
      if ((nameptr = strchr(name, '.')) != NULL)
        *nameptr = '\0';

      if (!bsearch(name, adobe_size_names,
                   sizeof(adobe_size_names) /
		       sizeof(adobe_size_names[0]),
		   sizeof(adobe_size_names[0]),
		   (int (*)(const void *, const void *))strcmp))
      {
	_cupsLangPrintf(stdout,
			_("      %s  Non-standard size name \"%s\"!\n"
			  "                REF: Page 187, section B.2.\n"),
			prefix, size->name);
      }
    }

   /*
    * Verify that the size is defined for both PageSize and PageRegion...
    */

    if (warn != 2 && !ppdFindChoice(page_size, size->name))
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout,
			_("      %s  Size \"%s\" defined for %s but not for "
			  "%s!\n"),
			prefix, size->name, "PageRegion", "PageSize");

      if (!warn)
	errors ++;
    }
    else if (warn != 2 && !ppdFindChoice(page_region, size->name))
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout,
			_("      %s  Size \"%s\" defined for %s but not for "
			  "%s!\n"),
			prefix, size->name, "PageSize", "PageRegion");

      if (!warn)
	errors ++;
    }
  }

  return (errors);
}


/*
 * 'check_translations()' - Check translations in the PPD file.
 */

static int				/* O - Errors found */
check_translations(ppd_file_t *ppd,	/* I - PPD file */
                   int        errors,	/* I - Errors found */
                   int        verbose,	/* I - Verbosity level */
                   int        warn)	/* I - Warnings only? */
{
  int		j;			/* Looping var */
  ppd_attr_t	*attr;			/* PPD attribute */
  cups_array_t	*languages;		/* Array of languages */
  int		langlen;		/* Length of language */
  char		*language,		/* Current language */
		keyword[PPD_MAX_NAME],	/* Localization keyword (full) */
		llkeyword[PPD_MAX_NAME],/* Localization keyword (base) */
		ckeyword[PPD_MAX_NAME],	/* Custom option keyword (full) */
		cllkeyword[PPD_MAX_NAME];
					/* Custom option keyword (base) */
  ppd_option_t	*option;		/* Standard UI option */
  ppd_coption_t	*coption;		/* Custom option */
  ppd_cparam_t	*cparam;		/* Custom parameter */
  char		ll[3];			/* Base language */
  const char	*prefix;		/* WARN/FAIL prefix */
  const char	*text;			/* Pointer into UI text */


  prefix = warn ? "  WARN  " : "**FAIL**";

  if ((languages = _ppdGetLanguages(ppd)) != NULL)
  {
   /*
    * This file contains localizations, check them...
    */

    for (language = (char *)cupsArrayFirst(languages);
         language;
	 language = (char *)cupsArrayNext(languages))
    {
      langlen = strlen(language);
      if (langlen != 2 && langlen != 5)
      {
	if (!warn && !errors && !verbose)
	  _cupsLangPuts(stdout, _(" FAIL\n"));

	if (verbose >= 0)
	  _cupsLangPrintf(stdout,
			  _("      %s  Bad language \"%s\"!\n"),
			  prefix, language);

	if (!warn)
	  errors ++;

	continue;
      }

      if (!strcmp(language, "en"))
        continue;

      strlcpy(ll, language, sizeof(ll));

     /*
      * Loop through all options and choices...
      */

      for (option = ppdFirstOption(ppd);
	   option;
	   option = ppdNextOption(ppd))
      {
        if (!strcmp(option->keyword, "PageRegion"))
	  continue;

	snprintf(keyword, sizeof(keyword), "%s.Translation", language);
	snprintf(llkeyword, sizeof(llkeyword), "%s.Translation", ll);

	if ((attr = ppdFindAttr(ppd, keyword, option->keyword)) == NULL &&
	    (attr = ppdFindAttr(ppd, llkeyword, option->keyword)) == NULL)
	{
	  if (!warn && !errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  if (verbose >= 0)
	    _cupsLangPrintf(stdout,
			    _("      %s  Missing \"%s\" translation "
			      "string for option %s!\n"),
			    prefix, language, option->keyword);

          if (!warn)
	    errors ++;
	}
	else if (!valid_utf8(attr->text))
	{
	  if (!warn && !errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  if (verbose >= 0)
	    _cupsLangPrintf(stdout,
			    _("      %s  Bad UTF-8 \"%s\" translation "
			      "string for option %s!\n"),
			    prefix, language, option->keyword);

	  if (!warn)
	    errors ++;
	}

	snprintf(keyword, sizeof(keyword), "%s.%s", language,
		 option->keyword);
	snprintf(llkeyword, sizeof(llkeyword), "%s.%s", ll,
		 option->keyword);

	for (j = 0; j < option->num_choices; j ++)
	{
         /*
	  * First see if this choice is a number; if so, don't require
	  * translation...
	  */

          for (text = option->choices[j].text; *text; text ++)
	    if (!strchr("0123456789-+.", *text))
	      break;

          if (!*text)
	    continue;

	 /*
	  * Check custom choices differently...
	  */

	  if (!strcasecmp(option->choices[j].choice, "Custom") &&
	      (coption = ppdFindCustomOption(ppd,
					     option->keyword)) != NULL)
	  {
	    snprintf(ckeyword, sizeof(ckeyword), "%s.Custom%s",
		     language, option->keyword);

	    if ((attr = ppdFindAttr(ppd, ckeyword, "True")) != NULL &&
		!valid_utf8(attr->text))
	    {
	      if (!warn && !errors && !verbose)
		_cupsLangPuts(stdout, _(" FAIL\n"));

	      if (verbose >= 0)
		_cupsLangPrintf(stdout,
				_("      %s  Bad UTF-8 \"%s\" "
				  "translation string for option %s, "
				  "choice %s!\n"),
				prefix, language,
				ckeyword + 1 + strlen(language),
				"True");

              if (!warn)
		errors ++;
	    }

	    if (strcasecmp(option->keyword, "PageSize"))
	    {
	      for (cparam = (ppd_cparam_t *)cupsArrayFirst(coption->params);
		   cparam;
		   cparam = (ppd_cparam_t *)cupsArrayNext(coption->params))
	      {
		snprintf(ckeyword, sizeof(ckeyword), "%s.ParamCustom%s",
			 language, option->keyword);
		snprintf(cllkeyword, sizeof(cllkeyword), "%s.ParamCustom%s",
			 ll, option->keyword);

		if ((attr = ppdFindAttr(ppd, ckeyword,
					cparam->name)) == NULL &&
		    (attr = ppdFindAttr(ppd, cllkeyword,
					cparam->name)) == NULL)
		{
		  if (!warn && !errors && !verbose)
		    _cupsLangPuts(stdout, _(" FAIL\n"));

		  if (verbose >= 0)
		    _cupsLangPrintf(stdout,
				    _("      %s  Missing \"%s\" "
				      "translation string for option %s, "
				      "choice %s!\n"),
				    prefix, language,
				    ckeyword + 1 + strlen(language),
				    cparam->name);

                  if (!warn)
		    errors ++;
		}
		else if (!valid_utf8(attr->text))
		{
		  if (!warn && !errors && !verbose)
		    _cupsLangPuts(stdout, _(" FAIL\n"));

		  if (verbose >= 0)
		    _cupsLangPrintf(stdout,
				    _("      %s  Bad UTF-8 \"%s\" "
				      "translation string for option %s, "
				      "choice %s!\n"),
				    prefix, language,
				    ckeyword + 1 + strlen(language),
				    cparam->name);

		  if (!warn)
		    errors ++;
		}
	      }
	    }
	  }
	  else if ((attr = ppdFindAttr(ppd, keyword,
				       option->choices[j].choice)) == NULL &&
		   (attr = ppdFindAttr(ppd, llkeyword,
				       option->choices[j].choice)) == NULL)
	  {
	    if (!warn && !errors && !verbose)
	      _cupsLangPuts(stdout, _(" FAIL\n"));

	    if (verbose >= 0)
	      _cupsLangPrintf(stdout,
			      _("      %s  Missing \"%s\" "
				"translation string for option %s, "
				"choice %s!\n"),
			      prefix, language, option->keyword,
			      option->choices[j].choice);

	    if (!warn)
	      errors ++;
	  }
	  else if (!valid_utf8(attr->text))
	  {
	    if (!warn && !errors && !verbose)
	      _cupsLangPuts(stdout, _(" FAIL\n"));

	    if (verbose >= 0)
	      _cupsLangPrintf(stdout,
			      _("      %s  Bad UTF-8 \"%s\" "
				"translation string for option %s, "
				"choice %s!\n"),
			      prefix, language, option->keyword,
			      option->choices[j].choice);

	    if (!warn)
	      errors ++;
	  }
	}
      }
    }

   /*
    * Verify that we have the base language for each localized one...
    */

    for (language = (char *)cupsArrayFirst(languages);
	 language;
	 language = (char *)cupsArrayNext(languages))
      if (language[2])
      {
       /*
	* Lookup the base language...
	*/

	cupsArraySave(languages);

	strlcpy(ll, language, sizeof(ll));

	if (!cupsArrayFind(languages, ll) &&
	    strcmp(ll, "zh") && strcmp(ll, "en"))
	{
	  if (!warn && !errors && !verbose)
	    _cupsLangPuts(stdout, _(" FAIL\n"));

	  if (verbose >= 0)
	    _cupsLangPrintf(stdout,
			    _("      %s  No base translation \"%s\" "
			      "is included in file!\n"), prefix, ll);

	  if (!warn)
	    errors ++;
	}

	cupsArrayRestore(languages);
      }

   /*
    * Free memory used for the languages...
    */

    _ppdFreeLanguages(languages);
  }

  return (errors);
}


/*
 * 'show_conflicts()' - Show option conflicts in a PPD file.
 */

static void
show_conflicts(ppd_file_t *ppd)		/* I - PPD to check */
{
  int		i, j;			/* Looping variables */
  ppd_const_t	*c;			/* Current constraint */
  ppd_option_t	*o1, *o2;		/* Options */
  ppd_choice_t	*c1, *c2;		/* Choices */


 /*
  * Loop through all of the UI constraints and report any options
  * that conflict...
  */

  for (i = ppd->num_consts, c = ppd->consts; i > 0; i --, c ++)
  {
   /*
    * Grab pointers to the first option...
    */

    o1 = ppdFindOption(ppd, c->option1);

    if (o1 == NULL)
      continue;
    else if (c->choice1[0] != '\0')
    {
     /*
      * This constraint maps to a specific choice.
      */

      c1 = ppdFindChoice(o1, c->choice1);
    }
    else
    {
     /*
      * This constraint applies to any choice for this option.
      */

      for (j = o1->num_choices, c1 = o1->choices; j > 0; j --, c1 ++)
        if (c1->marked)
	  break;

      if (j == 0 ||
          !strcasecmp(c1->choice, "None") ||
          !strcasecmp(c1->choice, "Off") ||
          !strcasecmp(c1->choice, "False"))
        c1 = NULL;
    }

   /*
    * Grab pointers to the second option...
    */

    o2 = ppdFindOption(ppd, c->option2);

    if (o2 == NULL)
      continue;
    else if (c->choice2[0] != '\0')
    {
     /*
      * This constraint maps to a specific choice.
      */

      c2 = ppdFindChoice(o2, c->choice2);
    }
    else
    {
     /*
      * This constraint applies to any choice for this option.
      */

      for (j = o2->num_choices, c2 = o2->choices; j > 0; j --, c2 ++)
        if (c2->marked)
	  break;

      if (j == 0 ||
          !strcasecmp(c2->choice, "None") ||
          !strcasecmp(c2->choice, "Off") ||
          !strcasecmp(c2->choice, "False"))
        c2 = NULL;
    }

   /*
    * If both options are marked then there is a conflict...
    */

    if (c1 != NULL && c1->marked && c2 != NULL && c2->marked)
      _cupsLangPrintf(stdout,
                      _("        WARN    \"%s %s\" conflicts with \"%s %s\"\n"
                        "                (constraint=\"%s %s %s %s\")\n"),
        	      o1->keyword, c1->choice, o2->keyword, c2->choice,
		      c->option1, c->choice1, c->option2, c->choice2);
  }
}


/*
 * 'test_raster()' - Test PostScript commands for raster printers.
 */

static int				/* O - 1 on success, 0 on failure */
test_raster(ppd_file_t *ppd,		/* I - PPD file */
            int        verbose)		/* I - Verbosity */
{
  cups_page_header2_t	header;		/* Page header */


  ppdMarkDefaults(ppd);
  if (cupsRasterInterpretPPD(&header, ppd, 0, NULL, 0))
  {
    if (!verbose)
      _cupsLangPuts(stdout, _(" FAIL\n"));

    if (verbose >= 0)
      _cupsLangPrintf(stdout,
		      _("      **FAIL**  Default option code cannot be "
			"interpreted: %s\n"), cupsRasterErrorString());

    return (0);
  }

 /*
  * Try a test of custom page size code, if available...
  */

  if (!ppdPageSize(ppd, "Custom.612x792"))
    return (1);

  ppdMarkOption(ppd, "PageSize", "Custom.612x792");

  if (cupsRasterInterpretPPD(&header, ppd, 0, NULL, 0))
  {
    if (!verbose)
      _cupsLangPuts(stdout, _(" FAIL\n"));

    if (verbose >= 0)
      _cupsLangPrintf(stdout,
		      _("      **FAIL**  Default option code cannot be "
			"interpreted: %s\n"), cupsRasterErrorString());

    return (0);
  }

  return (1);
}


/*
 * 'usage()' - Show program usage...
 */

static void
usage(void)
{
  _cupsLangPuts(stdout,
                _("Usage: cupstestppd [options] filename1.ppd[.gz] "
		  "[... filenameN.ppd[.gz]]\n"
		  "       program | cupstestppd [options] -\n"
		  "\n"
		  "Options:\n"
		  "\n"
		  "    -R root-directory    Set alternate root\n"
		  "    -W {all,none,constraints,defaults,duplex,filters,"
		  "profiles,sizes,translations}\n"
		  "                         Issue warnings instead of errors\n"
		  "    -q                   Run silently\n"
		  "    -r                   Use 'relaxed' open mode\n"
		  "    -v                   Be slightly verbose\n"
		  "    -vv                  Be very verbose\n"));

  exit(ERROR_USAGE);
}


/*
 * 'valid_path()' - Check whether a path has the correct capitalization.
 */

static int				/* O - Errors found */
valid_path(const char *keyword,		/* I - Keyword using path */
           const char *path,		/* I - Path to check */
	   int        errors,		/* I - Errors found */
	   int        verbose,		/* I - Verbosity level */
	   int        warn)		/* I - Warnings only? */
{
  cups_dir_t	*dir;			/* Current directory */
  cups_dentry_t	*dentry;		/* Current directory entry */
  char		temp[1024],		/* Temporary path */
		*ptr;			/* Pointer into temporary path */
  const char	*prefix;		/* WARN/FAIL prefix */


  prefix = warn ? "  WARN  " : "**FAIL**";

 /*
  * Loop over the components of the path, checking that the entry exists with
  * the same capitalization...
  */

  strlcpy(temp, path, sizeof(temp));

  while ((ptr = strrchr(temp, '/')) != NULL)
  {
   /*
    * Chop off the trailing component so temp == dirname and ptr == basename.
    */

    *ptr++ = '\0';

   /*
    * Try opening the directory containing the base name...
    */

    if (temp[0])
      dir = cupsDirOpen(temp);
    else
      dir = cupsDirOpen("/");

    if (!dir)
      dentry = NULL;
    else
    {
      while ((dentry = cupsDirRead(dir)) != NULL)
      {
        if (!strcmp(dentry->filename, ptr))
	  break;
      }

      cupsDirClose(dir);
    }

   /*
    * Display an error if the filename doesn't exist with the same
    * capitalization...
    */

    if (!dentry)
    {
      if (!warn && !errors && !verbose)
	_cupsLangPuts(stdout, _(" FAIL\n"));

      if (verbose >= 0)
	_cupsLangPrintf(stdout,
			_("      %s  %s file \"%s\" has the wrong "
			  "capitalization!\n"), prefix, keyword, path);

      if (!warn)
	errors ++;

      break;
    }
  }

  return (errors);
}


/*
 * 'valid_utf8()' - Check whether a string contains valid UTF-8 text.
 */

static int				/* O - 1 if valid, 0 if not */
valid_utf8(const char *s)		/* I - String to check */
{
  while (*s)
  {
    if (*s & 0x80)
    {
     /*
      * Check for valid UTF-8 sequence...
      */

      if ((*s & 0xc0) == 0x80)
        return (0);			/* Illegal suffix byte */
      else if ((*s & 0xe0) == 0xc0)
      {
       /*
        * 2-byte sequence...
        */

        s ++;

        if ((*s & 0xc0) != 0x80)
          return (0);			/* Missing suffix byte */
      }
      else if ((*s & 0xf0) == 0xe0)
      {
       /*
        * 3-byte sequence...
        */

        s ++;

        if ((*s & 0xc0) != 0x80)
          return (0);			/* Missing suffix byte */

        s ++;

        if ((*s & 0xc0) != 0x80)
          return (0);			/* Missing suffix byte */
      }
      else if ((*s & 0xf8) == 0xf0)
      {
       /*
        * 4-byte sequence...
        */

        s ++;

        if ((*s & 0xc0) != 0x80)
          return (0);			/* Missing suffix byte */

        s ++;

        if ((*s & 0xc0) != 0x80)
          return (0);			/* Missing suffix byte */

        s ++;

        if ((*s & 0xc0) != 0x80)
          return (0);			/* Missing suffix byte */
      }
      else
        return (0);			/* Bad sequence */
    }

    s ++;
  }

  return (1);
}


/*
 * End of "$Id: cupstestppd.c 7807 2008-07-28 21:54:24Z mike $".
 */
