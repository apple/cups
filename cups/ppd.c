/*
 * "$Id$"
 *
 *   PPD file routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
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
 *   This code and any derivative of it may be used and distributed
 *   freely under the terms of the GNU General Public License when
 *   used with GNU Ghostscript or its derivatives.  Use of the code
 *   (or any derivative of it) with software other than GNU
 *   GhostScript (or its derivatives) is governed by the CUPS license
 *   agreement.
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   ppdClose()             - Free all memory used by the PPD file.
 *   ppdErrorString()       - Returns the text assocated with a status.
 *   _ppdGetEncoding()      - Get the CUPS encoding value for the given
 *                            LanguageEncoding.
 *   ppdLastError()         - Return the status from the last ppdOpen*().
 *   ppdOpen()              - Read a PPD file into memory.
 *   ppdOpen2()             - Read a PPD file into memory.
 *   ppdOpenFd()            - Read a PPD file into memory.
 *   ppdOpenFile()          - Read a PPD file into memory.
 *   ppdSetConformance()    - Set the conformance level for PPD files.
 *   ppd_add_attr()         - Add an attribute to the PPD data.
 *   ppd_add_choice()       - Add a choice to an option.
 *   ppd_add_size()         - Add a page size.
 *   ppd_compare_attrs()    - Compare two attributes.
 *   ppd_compare_choices()  - Compare two choices...
 *   ppd_compare_consts()   - Compare two constraints.
 *   ppd_compare_coptions() - Compare two custom options.
 *   ppd_compare_cparams()  - Compare two custom parameters.
 *   ppd_compare_options()  - Compare two options.
 *   ppd_decode()           - Decode a string value...
 *   ppd_free_group()       - Free a single UI group.
 *   ppd_free_option()      - Free a single option.
 *   ppd_get_coption()      - Get a custom option record.
 *   ppd_get_cparam()       - Get a custom parameter record.
 *   ppd_get_group()        - Find or create the named group as needed.
 *   ppd_get_option()       - Find or create the named option as needed.
 *   ppd_hash_option()      - Generate a hash of the option name...
 *   ppd_read()             - Read a line from a PPD file, skipping comment
 *                            lines as necessary.
 */

/*
 * Include necessary headers.
 */

#include "globals.h"
#include "debug.h"
#include <stdlib.h>


/*
 * Definitions...
 */

#if defined(WIN32) || defined(__EMX__)
#  define READ_BINARY	"rb"		/* Open a binary file for reading */
#  define WRITE_BINARY	"wb"		/* Open a binary file for writing */
#else
#  define READ_BINARY	"r"		/* Open a binary file for reading */
#  define WRITE_BINARY	"w"		/* Open a binary file for writing */
#endif /* WIN32 || __EMX__ */

#define ppd_free(p)	if (p) free(p)	/* Safe free macro */

#define PPD_KEYWORD	1		/* Line contained a keyword */
#define PPD_OPTION	2		/* Line contained an option name */
#define PPD_TEXT	4		/* Line contained human-readable text */
#define PPD_STRING	8		/* Line contained a string or code */

#define PPD_HASHSIZE	512		/* Size of hash */


/*
 * Local functions...
 */

static ppd_attr_t	*ppd_add_attr(ppd_file_t *ppd, const char *name,
			              const char *spec, const char *text,
				      const char *value);
static ppd_choice_t	*ppd_add_choice(ppd_option_t *option, const char *name);
static ppd_size_t	*ppd_add_size(ppd_file_t *ppd, const char *name);
static int		ppd_compare_attrs(ppd_attr_t *a, ppd_attr_t *b);
static int		ppd_compare_choices(ppd_choice_t *a, ppd_choice_t *b);
static int		ppd_compare_consts(ppd_const_t *a, ppd_const_t *b);
static int		ppd_compare_coptions(ppd_coption_t *a,
			                     ppd_coption_t *b);
static int		ppd_compare_cparams(ppd_cparam_t *a, ppd_cparam_t *b);
static int		ppd_compare_options(ppd_option_t *a, ppd_option_t *b);
static int		ppd_decode(char *string);
static void		ppd_free_group(ppd_group_t *group);
static void		ppd_free_option(ppd_option_t *option);
static ppd_coption_t	*ppd_get_coption(ppd_file_t *ppd, const char *name);
static ppd_cparam_t	*ppd_get_cparam(ppd_coption_t *opt,
			                const char *param,
					const char *text);
static ppd_group_t	*ppd_get_group(ppd_file_t *ppd, const char *name,
			               const char *text, _cups_globals_t *cg,
				       cups_encoding_t encoding);
static ppd_option_t	*ppd_get_option(ppd_group_t *group, const char *name);
static int		ppd_hash_option(ppd_option_t *option);
static int		ppd_read(cups_file_t *fp, char *keyword, char *option,
			         char *text, char **string, int ignoreblank,
				 _cups_globals_t *cg);


/*
 * 'ppdClose()' - Free all memory used by the PPD file.
 */

void
ppdClose(ppd_file_t *ppd)		/* I - PPD file record */
{
  int			i;		/* Looping var */
  ppd_emul_t		*emul;		/* Current emulation */
  ppd_group_t		*group;		/* Current group */
  char			**font;		/* Current font */
  char			**filter;	/* Current filter */
  ppd_attr_t		**attr;		/* Current attribute */
  ppd_coption_t		*coption;	/* Current custom option */
  ppd_cparam_t		*cparam;	/* Current custom parameter */


 /*
  * Range check arguments...
  */

  if (!ppd)
    return;

 /*
  * Free all strings at the top level...
  */

  ppd_free(ppd->lang_encoding);
  ppd_free(ppd->nickname);
  ppd_free(ppd->patches);
  ppd_free(ppd->jcl_begin);
  ppd_free(ppd->jcl_end);
  ppd_free(ppd->jcl_ps);

 /*
  * Free any emulations...
  */

  if (ppd->num_emulations > 0)
  {
    for (i = ppd->num_emulations, emul = ppd->emulations; i > 0; i --, emul ++)
    {
      ppd_free(emul->start);
      ppd_free(emul->stop);
    }

    ppd_free(ppd->emulations);
  }

 /*
  * Free any UI groups, subgroups, and options...
  */

  if (ppd->num_groups > 0)
  {
    for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
      ppd_free_group(group);

    ppd_free(ppd->groups);
  }

  cupsArrayDelete(ppd->options);
  cupsArrayDelete(ppd->marked);

 /*
  * Free any page sizes...
  */

  if (ppd->num_sizes > 0)
    ppd_free(ppd->sizes);

 /*
  * Free any constraints...
  */

  if (ppd->num_consts > 0)
    ppd_free(ppd->consts);

 /*
  * Free any filters...
  */

  if (ppd->num_filters > 0)
  {
    for (i = ppd->num_filters, filter = ppd->filters; i > 0; i --, filter ++)
    {
      ppd_free(*filter);
    }

    ppd_free(ppd->filters);
  }

 /*
  * Free any fonts...
  */

  if (ppd->num_fonts > 0)
  {
    for (i = ppd->num_fonts, font = ppd->fonts; i > 0; i --, font ++)
    {
      ppd_free(*font);
    }

    ppd_free(ppd->fonts);
  }

 /*
  * Free any profiles...
  */

  if (ppd->num_profiles > 0)
    ppd_free(ppd->profiles);

 /*
  * Free any attributes...
  */

  if (ppd->num_attrs > 0)
  {
    for (i = ppd->num_attrs, attr = ppd->attrs; i > 0; i --, attr ++)
    {
      ppd_free((*attr)->value);
      ppd_free(*attr);
    }

    ppd_free(ppd->attrs);
  }

  cupsArrayDelete(ppd->sorted_attrs);

 /*
  * Free custom options...
  */

  for (coption = (ppd_coption_t *)cupsArrayFirst(ppd->coptions);
       coption;
       coption = (ppd_coption_t *)cupsArrayNext(ppd->coptions))
  {
    for (cparam = (ppd_cparam_t *)cupsArrayFirst(coption->params);
         cparam;
	 cparam = (ppd_cparam_t *)cupsArrayNext(coption->params))
    {
      switch (cparam->type)
      {
        case PPD_CUSTOM_PASSCODE :
        case PPD_CUSTOM_PASSWORD :
        case PPD_CUSTOM_STRING :
            ppd_free(cparam->current.custom_string);
	    break;

	default :
	    break;
      }

      free(cparam);
    }

    cupsArrayDelete(coption->params);

    free(coption);
  }

  cupsArrayDelete(ppd->coptions);

 /*
  * Free the whole record...
  */

  ppd_free(ppd);
}


/*
 * 'ppdErrorString()' - Returns the text assocated with a status.
 *
 * @since CUPS 1.1.19@
 */

const char *				/* O - Status string */
ppdErrorString(ppd_status_t status)	/* I - PPD status */
{
  static const char * const messages[] =/* Status messages */
		{
		  _("OK"),
		  _("Unable to open PPD file"),
		  _("NULL PPD file pointer"),
		  _("Memory allocation error"),
		  _("Missing PPD-Adobe-4.x header"),
		  _("Missing value string"),
		  _("Internal error"),
		  _("Bad OpenGroup"),
		  _("OpenGroup without a CloseGroup first"),
		  _("Bad OpenUI/JCLOpenUI"),
		  _("OpenUI/JCLOpenUI without a CloseUI/JCLCloseUI first"),
		  _("Bad OrderDependency"),
		  _("Bad UIConstraints"),
		  _("Missing asterisk in column 1"),
		  _("Line longer than the maximum allowed (255 characters)"),
		  _("Illegal control character"),
		  _("Illegal main keyword string"),
		  _("Illegal option keyword string"),
		  _("Illegal translation string"),
		  _("Illegal whitespace character"),
		  _("Bad custom parameter")
		};


  if (status < PPD_OK || status > PPD_ILLEGAL_WHITESPACE)
    return (_cupsLangString(cupsLangDefault(), _("Unknown")));
  else
    return (_cupsLangString(cupsLangDefault(), messages[status]));
}


/*
 * '_ppdGetEncoding()' - Get the CUPS encoding value for the given
 *                       LanguageEncoding.
 */

cups_encoding_t				/* O - CUPS encoding value */
_ppdGetEncoding(const char *name)	/* I - LanguageEncoding string */
{
  if (!strcasecmp(name, "ISOLatin1"))
    return (CUPS_ISO8859_1);
  else if (!strcasecmp(name, "ISOLatin2"))
    return (CUPS_ISO8859_2);
  else if (!strcasecmp(name, "ISOLatin5"))
    return (CUPS_ISO8859_5);
  else if (!strcasecmp(name, "JIS83-RKSJ"))
    return (CUPS_WINDOWS_932);
  else if (!strcasecmp(name, "MacStandard"))
    return (CUPS_MAC_ROMAN);
  else if (!strcasecmp(name, "WindowsANSI"))
    return (CUPS_WINDOWS_1252);
  else
    return (CUPS_UTF8);
}


/*
 * 'ppdLastError()' - Return the status from the last ppdOpen*().
 *
 * @since CUPS 1.1.19@
 */

ppd_status_t				/* O - Status code */
ppdLastError(int *line)			/* O - Line number */
{
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */


  if (line)
    *line = cg->ppd_line;

  return (cg->ppd_status);
}


/*
 * 'ppdOpen()' - Read a PPD file into memory.
 */

ppd_file_t *				/* O - PPD file record */
ppdOpen(FILE *fp)			/* I - File to read from */
{
  ppd_file_t	*ppd;			/* PPD file record */
  cups_file_t	*cf;			/* CUPS file */


 /*
  * Reopen the stdio file as a CUPS file...
  */

  if ((cf = cupsFileOpenFd(fileno(fp), "r")) == NULL)
    return (NULL);

 /*
  * Load the PPD file using the newer API...
  */

  ppd = ppdOpen2(cf);

 /*
  * Close the CUPS file and return the PPD...
  */

  cupsFileClose(cf);

  return (ppd);
}


/*
 * 'ppdOpen2()' - Read a PPD file into memory.
 *
 * @since CUPS 1.2@
 */

ppd_file_t *				/* O - PPD file record */
ppdOpen2(cups_file_t *fp)		/* I - File to read from */
{
  int			i, j, k;	/* Looping vars */
  int			count;		/* Temporary count */
  ppd_file_t		*ppd;		/* PPD file record */
  ppd_group_t		*group,		/* Current group */
			*subgroup;	/* Current sub-group */
  ppd_option_t		*option;	/* Current option */
  ppd_choice_t		*choice;	/* Current choice */
  ppd_const_t		*constraint;	/* Current constraint */
  ppd_size_t		*size;		/* Current page size */
  int			mask;		/* Line data mask */
  char			keyword[PPD_MAX_NAME],
  					/* Keyword from file */
			name[PPD_MAX_NAME],
					/* Option from file */
			text[PPD_MAX_LINE],
					/* Human-readable text from file */
			*string,	/* Code/text from file */
			*sptr,		/* Pointer into string */
			*nameptr,	/* Pointer into name */
			*temp,		/* Temporary string pointer */
			**tempfonts;	/* Temporary fonts pointer */
  float			order;		/* Order dependency number */
  ppd_section_t		section;	/* Order dependency section */
  ppd_profile_t		*profile;	/* Pointer to color profile */
  char			**filter;	/* Pointer to filter */
  cups_lang_t		*language;	/* Default language */
  struct lconv		*loc;		/* Locale data */
  int			ui_keyword;	/* Is this line a UI keyword? */
  cups_encoding_t	encoding;	/* Encoding of PPD file */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */
  static const char * const ui_keywords[] =
			{
#ifdef CUPS_USE_FULL_UI_KEYWORDS_LIST
 /*
  * Adobe defines some 41 keywords as "UI", meaning that they are
  * user interface elements and that they should be treated as such
  * even if the PPD creator doesn't use Open/CloseUI around them.
  *
  * Since this can cause previously invisible options to appear and
  * confuse users, the default is to only treat the PageSize and
  * PageRegion keywords this way.
  */
			  /* Boolean keywords */
			  "BlackSubstitution",
			  "Booklet",
			  "Collate",
			  "ManualFeed",
			  "MirrorPrint",
			  "NegativePrint",
			  "Sorter",
			  "TraySwitch",

			  /* PickOne keywords */
			  "AdvanceMedia",
			  "BindColor",
			  "BindEdge",
			  "BindType",
			  "BindWhen",
			  "BitsPerPixel",
			  "ColorModel",
			  "CutMedia",
			  "Duplex",
			  "FoldType",
			  "FoldWhen",
			  "InputSlot",
			  "JCLFrameBufferSize",
			  "JCLResolution",
			  "Jog",
			  "MediaColor",
			  "MediaType",
			  "MediaWeight",
			  "OutputBin",
			  "OutputMode",
			  "OutputOrder",
			  "PageRegion",
			  "PageSize",
			  "Resolution",
			  "Separations",
			  "Signature",
			  "Slipsheet",
			  "Smoothing",
			  "StapleLocation",
			  "StapleOrientation",
			  "StapleWhen",
			  "StapleX",
			  "StapleY"
#else /* !CUPS_USE_FULL_UI_KEYWORDS_LIST */
			  "PageRegion",
			  "PageSize"
#endif /* CUPS_USE_FULL_UI_KEYWORDS_LIST */
			};


 /*
  * Default to "OK" status...
  */

  cg->ppd_status = PPD_OK;
  cg->ppd_line   = 0;

 /*
  * Range check input...
  */

  if (fp == NULL)
  {
    cg->ppd_status = PPD_NULL_FILE;
    return (NULL);
  }

 /*
  * Grab the first line and make sure it reads '*PPD-Adobe: "major.minor"'...
  */

  mask = ppd_read(fp, keyword, name, text, &string, 0, cg);

  DEBUG_printf(("mask=%x, keyword=\"%s\"...\n", mask, keyword));

  if (mask == 0 ||
      strcmp(keyword, "PPD-Adobe") ||
      string == NULL || string[0] != '4')
  {
   /*
    * Either this is not a PPD file, or it is not a 4.x PPD file.
    */

    if (cg->ppd_status == PPD_OK)
      cg->ppd_status = PPD_MISSING_PPDADOBE4;

    ppd_free(string);

    return (NULL);
  }

  DEBUG_printf(("ppdOpen: keyword = %s, string = %p\n", keyword, string));

  ppd_free(string);

 /*
  * Allocate memory for the PPD file record...
  */

  if ((ppd = calloc(1, sizeof(ppd_file_t))) == NULL)
  {
    cg->ppd_status = PPD_ALLOC_ERROR;

    return (NULL);
  }

  ppd->language_level = 1;
  ppd->color_device   = 0;
  ppd->colorspace     = PPD_CS_GRAY;
  ppd->landscape      = -90;
  ppd->coptions       = cupsArrayNew((cups_array_func_t)ppd_compare_coptions,
                                     NULL);

 /*
  * Get the default language for the user...
  */

  language = cupsLangDefault();
  loc      = localeconv();

 /*
  * Read lines from the PPD file and add them to the file record...
  */

  group      = NULL;
  subgroup   = NULL;
  option     = NULL;
  choice     = NULL;
  ui_keyword = 0;
  encoding   = CUPS_ISO8859_1;

  while ((mask = ppd_read(fp, keyword, name, text, &string, 1, cg)) != 0)
  {
#ifdef DEBUG
    printf("mask = %x, keyword = \"%s\"", mask, keyword);

    if (name[0] != '\0')
      printf(", name = \"%s\"", name);

    if (text[0] != '\0')
      printf(", text = \"%s\"", text);

    if (string != NULL)
    {
      if (strlen(string) > 40)
        printf(", string = %p", string);
      else
        printf(", string = \"%s\"", string);
    }

    puts("");
#endif /* DEBUG */

    if (strcmp(keyword, "CloseUI") && strcmp(keyword, "CloseGroup") &&
	strcmp(keyword, "CloseSubGroup") && strncmp(keyword, "Default", 7) &&
        strcmp(keyword, "JCLCloseUI") && strcmp(keyword, "JCLOpenUI") &&
	strcmp(keyword, "OpenUI") && strcmp(keyword, "OpenGroup") &&
	strcmp(keyword, "OpenSubGroup") && string == NULL)
    {
     /*
      * Need a string value!
      */

      cg->ppd_status = PPD_MISSING_VALUE;

      goto error;
    }

   /*
    * Certain main keywords (as defined by the PPD spec) may be used
    * without the usual OpenUI/CloseUI stuff.  Presumably this is just
    * so that Adobe wouldn't completely break compatibility with PPD
    * files prior to v4.0 of the spec, but it is hopelessly
    * inconsistent...  Catch these main keywords and automatically
    * create the corresponding option, as needed...
    */

    if (ui_keyword)
    {
     /*
      * Previous line was a UI keyword...
      */

      option     = NULL;
      ui_keyword = 0;
    }

    if (option == NULL &&
        (mask & (PPD_KEYWORD | PPD_OPTION | PPD_STRING)) ==
	    (PPD_KEYWORD | PPD_OPTION | PPD_STRING))
    {
      for (i = 0; i < (int)(sizeof(ui_keywords) / sizeof(ui_keywords[0])); i ++)
        if (!strcmp(keyword, ui_keywords[i]))
	  break;

      if (i < (int)(sizeof(ui_keywords) / sizeof(ui_keywords[0])))
      {
       /*
        * Create the option in the appropriate group...
	*/

        ui_keyword = 1;

        DEBUG_printf(("**** FOUND ADOBE UI KEYWORD %s WITHOUT OPENUI!\n",
	              keyword));

        if (!group)
	{
          if ((group = ppd_get_group(ppd, "General", _("General"), cg,
	                             encoding)) == NULL)
	    goto error;

          DEBUG_printf(("Adding to group %s...\n", group->text));
          option = ppd_get_option(group, keyword);
	  group  = NULL;
	}
	else
          option = ppd_get_option(group, keyword);

	if (option == NULL)
	{
          cg->ppd_status = PPD_ALLOC_ERROR;

          goto error;
	}

       /*
	* Now fill in the initial information for the option...
	*/

	if (!strncmp(keyword, "JCL", 3))
          option->section = PPD_ORDER_JCL;
	else
          option->section = PPD_ORDER_ANY;

	option->order = 10.0f;

	if (i < 8)
          option->ui = PPD_UI_BOOLEAN;
	else
          option->ui = PPD_UI_PICKONE;

        for (j = 0; j < ppd->num_attrs; j ++)
	  if (!strncmp(ppd->attrs[j]->name, "Default", 7) &&
	      !strcmp(ppd->attrs[j]->name + 7, keyword) &&
	      ppd->attrs[j]->value)
	  {
	    DEBUG_printf(("Setting Default%s to %s via attribute...\n",
	                  option->keyword, ppd->attrs[j]->value));
	    strlcpy(option->defchoice, ppd->attrs[j]->value,
	            sizeof(option->defchoice));
	    break;
	  }

        if (!strcmp(keyword, "PageSize"))
	  strlcpy(option->text, _("Media Size"), sizeof(option->text));
	else if (!strcmp(keyword, "MediaType"))
	  strlcpy(option->text, _("Media Type"), sizeof(option->text));
	else if (!strcmp(keyword, "InputSlot"))
	  strlcpy(option->text, _("Media Source"), sizeof(option->text));
	else if (!strcmp(keyword, "ColorModel"))
	  strlcpy(option->text, _("Output Mode"), sizeof(option->text));
	else if (!strcmp(keyword, "Resolution"))
	  strlcpy(option->text, _("Resolution"), sizeof(option->text));
        else
	  strlcpy(option->text, keyword, sizeof(option->text));
      }
    }

    if (!strcmp(keyword, "LanguageLevel"))
      ppd->language_level = atoi(string);
    else if (!strcmp(keyword, "LanguageEncoding"))
    {
     /*
      * Say all PPD files are UTF-8, since we convert to UTF-8...
      */

      ppd->lang_encoding = strdup("UTF-8");
      encoding           = _ppdGetEncoding(string);
    }
    else if (!strcmp(keyword, "LanguageVersion"))
      ppd->lang_version = string;
    else if (!strcmp(keyword, "Manufacturer"))
      ppd->manufacturer = string;
    else if (!strcmp(keyword, "ModelName"))
      ppd->modelname = string;
    else if (!strcmp(keyword, "Protocols"))
      ppd->protocols = string;
    else if (!strcmp(keyword, "PCFileName"))
      ppd->pcfilename = string;
    else if (!strcmp(keyword, "NickName"))
    {
      if (encoding != CUPS_UTF8)
      {
        cups_utf8_t	utf8[256];	/* UTF-8 version of NickName */


        cupsCharsetToUTF8(utf8, string, sizeof(utf8), encoding);
	ppd->nickname = strdup((char *)utf8);
      }
      else
        ppd->nickname = strdup(string);
    }
    else if (!strcmp(keyword, "Product"))
      ppd->product = string;
    else if (!strcmp(keyword, "ShortNickName"))
      ppd->shortnickname = string;
    else if (!strcmp(keyword, "TTRasterizer"))
      ppd->ttrasterizer = string;
    else if (!strcmp(keyword, "JCLBegin"))
    {
      ppd->jcl_begin = strdup(string);
      ppd_decode(ppd->jcl_begin);	/* Decode quoted string */
    }
    else if (!strcmp(keyword, "JCLEnd"))
    {
      ppd->jcl_end = strdup(string);
      ppd_decode(ppd->jcl_end);		/* Decode quoted string */
    }
    else if (!strcmp(keyword, "JCLToPSInterpreter"))
    {
      ppd->jcl_ps = strdup(string);
      ppd_decode(ppd->jcl_ps);		/* Decode quoted string */
    }
    else if (!strcmp(keyword, "AccurateScreensSupport"))
      ppd->accurate_screens = !strcmp(string, "True");
    else if (!strcmp(keyword, "ColorDevice"))
      ppd->color_device = !strcmp(string, "True");
    else if (!strcmp(keyword, "ContoneOnly"))
      ppd->contone_only = !strcmp(string, "True");
    else if (!strcmp(keyword, "cupsFlipDuplex"))
      ppd->flip_duplex = !strcmp(string, "True");
    else if (!strcmp(keyword, "cupsManualCopies"))
      ppd->manual_copies = !strcmp(string, "True");
    else if (!strcmp(keyword, "cupsModelNumber"))
      ppd->model_number = atoi(string);
    else if (!strcmp(keyword, "cupsColorProfile"))
    {
      if (ppd->num_profiles == 0)
        profile = malloc(sizeof(ppd_profile_t));
      else
        profile = realloc(ppd->profiles, sizeof(ppd_profile_t) *
	                                 (ppd->num_profiles + 1));

      ppd->profiles     = profile;
      profile           += ppd->num_profiles;
      ppd->num_profiles ++;

      memset(profile, 0, sizeof(ppd_profile_t));
      strlcpy(profile->resolution, name, sizeof(profile->resolution));
      strlcpy(profile->media_type, text, sizeof(profile->media_type));

      profile->density      = (float)_cupsStrScand(string, &sptr, loc);
      profile->gamma        = (float)_cupsStrScand(sptr, &sptr, loc);
      profile->matrix[0][0] = (float)_cupsStrScand(sptr, &sptr, loc);
      profile->matrix[0][1] = (float)_cupsStrScand(sptr, &sptr, loc);
      profile->matrix[0][2] = (float)_cupsStrScand(sptr, &sptr, loc);
      profile->matrix[1][0] = (float)_cupsStrScand(sptr, &sptr, loc);
      profile->matrix[1][1] = (float)_cupsStrScand(sptr, &sptr, loc);
      profile->matrix[1][2] = (float)_cupsStrScand(sptr, &sptr, loc);
      profile->matrix[2][0] = (float)_cupsStrScand(sptr, &sptr, loc);
      profile->matrix[2][1] = (float)_cupsStrScand(sptr, &sptr, loc);
      profile->matrix[2][2] = (float)_cupsStrScand(sptr, &sptr, loc);
    }
    else if (!strcmp(keyword, "cupsFilter"))
    {
      if (ppd->num_filters == 0)
        filter = malloc(sizeof(char *));
      else
        filter = realloc(ppd->filters, sizeof(char *) * (ppd->num_filters + 1));

      if (filter == NULL)
      {
        ppd_free(filter);

        cg->ppd_status = PPD_ALLOC_ERROR;

	goto error;
      }

      ppd->filters     = filter;
      filter           += ppd->num_filters;
      ppd->num_filters ++;

     /*
      * Copy filter string and prevent it from being freed below...
      */

      *filter = string;
      string  = NULL;
    }
    else if (!strcmp(keyword, "Throughput"))
      ppd->throughput = atoi(string);
    else if (!strcmp(keyword, "Font"))
    {
     /*
      * Add this font to the list of available fonts...
      */

      if (ppd->num_fonts == 0)
        tempfonts = (char **)malloc(sizeof(char *));
      else
        tempfonts = (char **)realloc(ppd->fonts,
	                             sizeof(char *) * (ppd->num_fonts + 1));

      if (tempfonts == NULL)
      {
        cg->ppd_status = PPD_ALLOC_ERROR;

	goto error;
      }
      
      ppd->fonts                 = tempfonts;
      ppd->fonts[ppd->num_fonts] = strdup(name);
      ppd->num_fonts ++;
    }
    else if (!strncmp(keyword, "ParamCustom", 11))
    {
      ppd_coption_t	*coption;	/* Custom option */
      ppd_cparam_t	*cparam;	/* Custom parameter */
      int		corder;		/* Order number */
      char		ctype[33],	/* Data type */
			cminimum[65],	/* Minimum value */
			cmaximum[65];	/* Maximum value */


     /*
      * Get the custom option and parameter...
      */

      if ((coption = ppd_get_coption(ppd, keyword + 11)) == NULL)
      {
        cg->ppd_status = PPD_ALLOC_ERROR;

	goto error;
      }

      if ((cparam = ppd_get_cparam(coption, name, text)) == NULL)
      {
        cg->ppd_status = PPD_ALLOC_ERROR;

	goto error;
      }

     /*
      * Get the parameter data...
      */

      if (sscanf(string, "%d%32s%64s%64s", &corder, ctype, cminimum,
                 cmaximum) != 4)
      {
        cg->ppd_status = PPD_BAD_CUSTOM_PARAM;

	goto error;
      }

      cparam->order = corder;

      if (!strcmp(ctype, "curve"))
      {
        cparam->type = PPD_CUSTOM_CURVE;
	cparam->minimum.custom_curve = (float)_cupsStrScand(cminimum, NULL, loc);
	cparam->maximum.custom_curve = (float)_cupsStrScand(cmaximum, NULL, loc);
      }
      else if (!strcmp(ctype, "int"))
      {
        cparam->type = PPD_CUSTOM_INT;
	cparam->minimum.custom_int = atoi(cminimum);
	cparam->maximum.custom_int = atoi(cmaximum);
      }
      else if (!strcmp(ctype, "invcurve"))
      {
        cparam->type = PPD_CUSTOM_INVCURVE;
	cparam->minimum.custom_invcurve = (float)_cupsStrScand(cminimum, NULL, loc);
	cparam->maximum.custom_invcurve = (float)_cupsStrScand(cmaximum, NULL, loc);
      }
      else if (!strcmp(ctype, "passcode"))
      {
        cparam->type = PPD_CUSTOM_PASSCODE;
	cparam->minimum.custom_passcode = atoi(cminimum);
	cparam->maximum.custom_passcode = atoi(cmaximum);
      }
      else if (!strcmp(ctype, "password"))
      {
        cparam->type = PPD_CUSTOM_PASSWORD;
	cparam->minimum.custom_password = atoi(cminimum);
	cparam->maximum.custom_password = atoi(cmaximum);
      }
      else if (!strcmp(ctype, "points"))
      {
        cparam->type = PPD_CUSTOM_POINTS;
	cparam->minimum.custom_points = (float)_cupsStrScand(cminimum, NULL, loc);
	cparam->maximum.custom_points = (float)_cupsStrScand(cmaximum, NULL, loc);
      }
      else if (!strcmp(ctype, "real"))
      {
        cparam->type = PPD_CUSTOM_REAL;
	cparam->minimum.custom_real = (float)_cupsStrScand(cminimum, NULL, loc);
	cparam->maximum.custom_real = (float)_cupsStrScand(cmaximum, NULL, loc);
      }
      else if (!strcmp(ctype, "string"))
      {
        cparam->type = PPD_CUSTOM_STRING;
	cparam->minimum.custom_string = atoi(cminimum);
	cparam->maximum.custom_string = atoi(cmaximum);
      }
      else
      {
        cg->ppd_status = PPD_BAD_CUSTOM_PARAM;

	goto error;
      }

     /*
      * Now special-case for CustomPageSize...
      */

      if (!strcmp(coption->keyword, "PageSize"))
      {
	if (!strcmp(name, "Width"))
	{
	  ppd->custom_min[0] = cparam->minimum.custom_points;
	  ppd->custom_max[0] = cparam->maximum.custom_points;
	}
	else if (!strcmp(name, "Height"))
	{
	  ppd->custom_min[1] = cparam->minimum.custom_points;
	  ppd->custom_max[1] = cparam->maximum.custom_points;
	}
      }
    }
    else if (!strcmp(keyword, "HWMargins"))
    {
      for (i = 0, sptr = string; i < 4; i ++)
        ppd->custom_margins[i] = (float)_cupsStrScand(sptr, &sptr, loc);
    }
    else if (!strncmp(keyword, "Custom", 6) && !strcmp(name, "True") && !option)
    {
      DEBUG_puts("Processing Custom option...");

     /*
      * Get the option and custom option...
      */

      if ((option = ppdFindOption(ppd, keyword + 6)) == NULL)
      {
        int		groupidx = -1;	/* Index for current group */
	ppd_group_t	*gtemp;		/* Temporary group */


        DEBUG_printf(("%s option not found for %s...\n", keyword + 6, keyword));

        if (group)
          groupidx = group - ppd->groups; /* Save index for current group */

	if ((gtemp = ppd_get_group(ppd, "General", _("General"), cg,
	                           encoding)) == NULL)
	{
	  DEBUG_puts("Unable to get general group!");

	  goto error;
	}

        if (group)
          group = ppd->groups + groupidx; /* Restore group pointer */

	if ((option = ppd_get_option(gtemp, keyword + 6)) == NULL)
	{
	  DEBUG_printf(("Unable to get %s option!\n", keyword + 6));

          cg->ppd_status = PPD_ALLOC_ERROR;

	  goto error;
	}
      }

      if (!ppd_get_coption(ppd, keyword + 6))
      {
        cg->ppd_status = PPD_ALLOC_ERROR;

	goto error;
      }

     /*
      * Add the "custom" option...
      */

      if ((choice = ppd_add_choice(option, "Custom")) == NULL)
      {
	DEBUG_puts("Unable to add Custom choice!");

        cg->ppd_status = PPD_ALLOC_ERROR;

	goto error;
      }

      strlcpy(choice->text, text[0] ? text : _("Custom"),
              sizeof(choice->text));

      choice->code = string;
      string       = NULL;		/* Don't add as an attribute below */
      option       = NULL;

     /*
      * Now process custom page sizes specially...
      */

      if (!strcmp(keyword, "CustomPageSize"))
      {
	ppd->variable_sizes = 1;

       /*
	* Add a "Custom" page size entry...
	*/

	ppd_add_size(ppd, "Custom");

	if ((option = ppdFindOption(ppd, "PageRegion")) == NULL)
	{
	  int		groupidx = -1;	/* Index to current group */
	  ppd_group_t	*gtemp;		/* Temporary group */

          if (group)
            groupidx = group - ppd->groups; /* Save index for current group */

	  if ((gtemp = ppd_get_group(ppd, "General", _("General"), cg,
				     encoding)) == NULL)
	  {
	    DEBUG_puts("Unable to get general group!");

	    goto error;
	  }

          if (group)
            group = ppd->groups + groupidx; /* Restore group pointer */

	  option = ppd_get_option(gtemp, "PageRegion");
        }

	if ((choice = ppd_add_choice(option, "Custom")) == NULL)
	{
	  DEBUG_puts("Unable to add Custom choice!");

	  cg->ppd_status = PPD_ALLOC_ERROR;

	  goto error;
	}

	strlcpy(choice->text, text[0] ? text : _("Custom"),
		sizeof(choice->text));
        option = NULL;
      }
    }
    else if (!strcmp(keyword, "LandscapeOrientation"))
    {
      if (!strcmp(string, "Minus90"))
        ppd->landscape = -90;
      else if (!strcmp(string, "Plus90"))
        ppd->landscape = 90;
    }
    else if (!strcmp(keyword, "Emulators"))
    {
      for (count = 1, sptr = string; sptr != NULL;)
        if ((sptr = strchr(sptr, ' ')) != NULL)
	{
	  count ++;
	  while (*sptr == ' ')
	    sptr ++;
	}

      ppd->num_emulations = count;
      ppd->emulations     = calloc(count, sizeof(ppd_emul_t));

      for (i = 0, sptr = string; i < count; i ++)
      {
        for (nameptr = ppd->emulations[i].name;
	     *sptr != '\0' && *sptr != ' ';
	     sptr ++)
	  if (nameptr < (ppd->emulations[i].name + sizeof(ppd->emulations[i].name) - 1))
	    *nameptr++ = *sptr;

	*nameptr = '\0';

	while (*sptr == ' ')
	  sptr ++;
      }
    }
    else if (!strncmp(keyword, "StartEmulator_", 14))
    {
      ppd_decode(string);

      for (i = 0; i < ppd->num_emulations; i ++)
        if (!strcmp(keyword + 14, ppd->emulations[i].name))
	{
	  ppd->emulations[i].start = string;
	  string = NULL;
	}
    }
    else if (!strncmp(keyword, "StopEmulator_", 13))
    {
      ppd_decode(string);

      for (i = 0; i < ppd->num_emulations; i ++)
        if (!strcmp(keyword + 13, ppd->emulations[i].name))
	{
	  ppd->emulations[i].stop = string;
	  string = NULL;
	}
    }
    else if (!strcmp(keyword, "JobPatchFile"))
    {
      if (ppd->patches == NULL)
        ppd->patches = strdup(string);
      else
      {
        temp = realloc(ppd->patches, strlen(ppd->patches) +
	                             strlen(string) + 1);
        if (temp == NULL)
	{
          cg->ppd_status = PPD_ALLOC_ERROR;

	  goto error;
	}

        ppd->patches = temp;

        strcpy(ppd->patches + strlen(ppd->patches), string);
      }
    }
    else if (!strcmp(keyword, "OpenUI"))
    {
     /*
      * Don't allow nesting of options...
      */

      if (option && cg->ppd_conform == PPD_CONFORM_STRICT)
      {
        cg->ppd_status = PPD_NESTED_OPEN_UI;

	goto error;
      }

     /*
      * Add an option record to the current sub-group, group, or file...
      */

      DEBUG_printf(("name=\"%s\" (%d)\n", name, strlen(name)));

      if (name[0] == '*')
        _cups_strcpy(name, name + 1); /* Eliminate leading asterisk */

      for (i = (int)strlen(name) - 1; i > 0 && isspace(name[i] & 255); i --)
        name[i] = '\0'; /* Eliminate trailing spaces */

      DEBUG_printf(("OpenUI of %s in group %s...\n", name,
                    group ? group->text : "(null)"));

      if (subgroup != NULL)
        option = ppd_get_option(subgroup, name);
      else if (group == NULL)
      {
	if ((group = ppd_get_group(ppd, "General", _("General"), cg,
	                           encoding)) == NULL)
	  goto error;

        DEBUG_printf(("Adding to group %s...\n", group->text));
        option = ppd_get_option(group, name);
	group  = NULL;
      }
      else
        option = ppd_get_option(group, name);

      if (option == NULL)
      {
        cg->ppd_status = PPD_ALLOC_ERROR;

	goto error;
      }

     /*
      * Now fill in the initial information for the option...
      */

      if (string && !strcmp(string, "PickMany"))
        option->ui = PPD_UI_PICKMANY;
      else if (string && !strcmp(string, "Boolean"))
        option->ui = PPD_UI_BOOLEAN;
      else if (string && !strcmp(string, "PickOne"))
        option->ui = PPD_UI_PICKONE;
      else if (cg->ppd_conform == PPD_CONFORM_STRICT)
      {
        cg->ppd_status = PPD_BAD_OPEN_UI;

	goto error;
      }
      else
        option->ui = PPD_UI_PICKONE;

      for (j = 0; j < ppd->num_attrs; j ++)
	if (!strncmp(ppd->attrs[j]->name, "Default", 7) &&
	    !strcmp(ppd->attrs[j]->name + 7, name) &&
	    ppd->attrs[j]->value)
	{
	  DEBUG_printf(("Setting Default%s to %s via attribute...\n",
	                option->keyword, ppd->attrs[j]->value));
	  strlcpy(option->defchoice, ppd->attrs[j]->value,
	          sizeof(option->defchoice));
	  break;
	}

      if (text[0])
        cupsCharsetToUTF8((cups_utf8_t *)option->text, text,
	                   sizeof(option->text), encoding);
      else
      {
        if (!strcmp(name, "PageSize"))
	  strlcpy(option->text, _("Media Size"), sizeof(option->text));
	else if (!strcmp(name, "MediaType"))
	  strlcpy(option->text, _("Media Type"), sizeof(option->text));
	else if (!strcmp(name, "InputSlot"))
	  strlcpy(option->text, _("Media Source"), sizeof(option->text));
	else if (!strcmp(name, "ColorModel"))
	  strlcpy(option->text, _("Output Mode"), sizeof(option->text));
	else if (!strcmp(name, "Resolution"))
	  strlcpy(option->text, _("Resolution"), sizeof(option->text));
        else
	  strlcpy(option->text, name, sizeof(option->text));
      }

      option->section = PPD_ORDER_ANY;

      ppd_free(string);
      string = NULL;
    }
    else if (!strcmp(keyword, "JCLOpenUI"))
    {
     /*
      * Don't allow nesting of options...
      */

      if (option && cg->ppd_conform == PPD_CONFORM_STRICT)
      {
        cg->ppd_status = PPD_NESTED_OPEN_UI;

	goto error;
      }

     /*
      * Find the JCL group, and add if needed...
      */

      group = ppd_get_group(ppd, "JCL", _("JCL"), cg, encoding);

      if (group == NULL)
	goto error;

     /*
      * Add an option record to the current JCLs...
      */

      if (name[0] == '*')
        _cups_strcpy(name, name + 1);

      option = ppd_get_option(group, name);

      if (option == NULL)
      {
        cg->ppd_status = PPD_ALLOC_ERROR;

	goto error;
      }

     /*
      * Now fill in the initial information for the option...
      */

      if (string && !strcmp(string, "PickMany"))
        option->ui = PPD_UI_PICKMANY;
      else if (string && !strcmp(string, "Boolean"))
        option->ui = PPD_UI_BOOLEAN;
      else if (string && !strcmp(string, "PickOne"))
        option->ui = PPD_UI_PICKONE;
      else
      {
        cg->ppd_status = PPD_BAD_OPEN_UI;

	goto error;
      }

      for (j = 0; j < ppd->num_attrs; j ++)
	if (!strncmp(ppd->attrs[j]->name, "Default", 7) &&
	    !strcmp(ppd->attrs[j]->name + 7, name) &&
	    ppd->attrs[j]->value)
	{
	  DEBUG_printf(("Setting Default%s to %s via attribute...\n",
	                option->keyword, ppd->attrs[j]->value));
	  strlcpy(option->defchoice, ppd->attrs[j]->value,
	          sizeof(option->defchoice));
	  break;
	}

      if (text[0])
        cupsCharsetToUTF8((cups_utf8_t *)option->text, text,
	                   sizeof(option->text), encoding);
      else
        strlcpy(option->text, name, sizeof(option->text));

      option->section = PPD_ORDER_JCL;
      group = NULL;

      ppd_free(string);
      string = NULL;
    }
    else if (!strcmp(keyword, "CloseUI") || !strcmp(keyword, "JCLCloseUI"))
    {
      option = NULL;

      ppd_free(string);
      string = NULL;
    }
    else if (!strcmp(keyword, "OpenGroup"))
    {
     /*
      * Open a new group...
      */

      if (group != NULL)
      {
        cg->ppd_status = PPD_NESTED_OPEN_GROUP;

	goto error;
      }

      if (!string)
      {
        cg->ppd_status = PPD_BAD_OPEN_GROUP;

	goto error;
      }

     /*
      * Separate the group name from the text (name/text)...
      */

      if ((sptr = strchr(string, '/')) != NULL)
        *sptr++ = '\0';
      else
        sptr = string;

     /*
      * Fix up the text...
      */

      ppd_decode(sptr);

     /*
      * Find/add the group...
      */

      group = ppd_get_group(ppd, string, sptr, cg, encoding);

      if (group == NULL)
	goto error;

      ppd_free(string);
      string = NULL;
    }
    else if (!strcmp(keyword, "CloseGroup"))
    {
      group = NULL;

      ppd_free(string);
      string = NULL;
    }
    else if (!strcmp(keyword, "OrderDependency") ||
             !strcmp(keyword, "NonUIOrderDependency"))
    {
      order = (float)_cupsStrScand(string, &sptr, loc);

      if (!sptr || sscanf(sptr, "%40s%40s", name, keyword) != 2)
      {
        cg->ppd_status = PPD_BAD_ORDER_DEPENDENCY;

	goto error;
      }

      if (keyword[0] == '*')
        _cups_strcpy(keyword, keyword + 1);

      if (!strcmp(name, "ExitServer"))
        section = PPD_ORDER_EXIT;
      else if (!strcmp(name, "Prolog"))
        section = PPD_ORDER_PROLOG;
      else if (!strcmp(name, "DocumentSetup"))
        section = PPD_ORDER_DOCUMENT;
      else if (!strcmp(name, "PageSetup"))
        section = PPD_ORDER_PAGE;
      else if (!strcmp(name, "JCLSetup"))
        section = PPD_ORDER_JCL;
      else
        section = PPD_ORDER_ANY;

      if (option == NULL)
      {
        ppd_group_t	*gtemp;


       /*
        * Only valid for Non-UI options...
	*/

        for (i = ppd->num_groups, gtemp = ppd->groups; i > 0; i --, gtemp ++)
          if (gtemp->text[0] == '\0')
	    break;

        if (i > 0)
          for (i = 0; i < gtemp->num_options; i ++)
	    if (!strcmp(keyword, gtemp->options[i].keyword))
	    {
	      gtemp->options[i].section = section;
	      gtemp->options[i].order   = order;
	      break;
	    }
      }
      else
      {
        option->section = section;
	option->order   = order;
      }

      ppd_free(string);
      string = NULL;
    }
    else if (!strncmp(keyword, "Default", 7))
    {
      if (string == NULL)
        continue;

     /*
      * Drop UI text, if any, from value...
      */

      if (strchr(string, '/') != NULL)
        *strchr(string, '/') = '\0';

     /*
      * Assign the default value as appropriate...
      */

      if (!strcmp(keyword, "DefaultColorSpace"))
      {
       /*
        * Set default colorspace...
	*/

	if (!strcmp(string, "CMY"))
          ppd->colorspace = PPD_CS_CMY;
	else if (!strcmp(string, "CMYK"))
          ppd->colorspace = PPD_CS_CMYK;
	else if (!strcmp(string, "RGB"))
          ppd->colorspace = PPD_CS_RGB;
	else if (!strcmp(string, "RGBK"))
          ppd->colorspace = PPD_CS_RGBK;
	else if (!strcmp(string, "N"))
          ppd->colorspace = PPD_CS_N;
	else
          ppd->colorspace = PPD_CS_GRAY;
      }
      else if (option && !strcmp(keyword + 7, option->keyword))
      {
       /*
        * Set the default as part of the current option...
	*/

        DEBUG_printf(("Setting %s to %s...\n", keyword, string));

        strlcpy(option->defchoice, string, sizeof(option->defchoice));

        DEBUG_printf(("%s is now %s...\n", keyword, option->defchoice));
      }
      else
      {
       /*
        * Lookup option and set if it has been defined...
	*/

        ppd_option_t	*toption;	/* Temporary option */


        if ((toption = ppdFindOption(ppd, keyword + 7)) != NULL)
	{
	  DEBUG_printf(("Setting %s to %s...\n", keyword, string));
	  strlcpy(toption->defchoice, string, sizeof(toption->defchoice));
	}
      }
    }
    else if (!strcmp(keyword, "UIConstraints") ||
             !strcmp(keyword, "NonUIConstraints"))
    {
      if (ppd->num_consts == 0)
	constraint = calloc(2, sizeof(ppd_const_t));
      else
	constraint = realloc(ppd->consts,
	                     (ppd->num_consts + 2) * sizeof(ppd_const_t));

      if (constraint == NULL)
      {
        cg->ppd_status = PPD_ALLOC_ERROR;

	goto error;
      }

      ppd->consts = constraint;
      constraint += ppd->num_consts;
      ppd->num_consts ++;

      switch (sscanf(string, "%40s%40s%40s%40s", constraint->option1,
                     constraint->choice1, constraint->option2,
		     constraint->choice2))
      {
        case 0 : /* Error */
	case 1 : /* Error */
	    cg->ppd_status = PPD_BAD_UI_CONSTRAINTS;
	    goto error;

	case 2 : /* Two options... */
	   /*
	    * Check for broken constraints like "* Option"...
	    */

	    if (cg->ppd_conform == PPD_CONFORM_STRICT &&
	        (!strcmp(constraint->option1, "*") ||
	         !strcmp(constraint->choice1, "*")))
	    {
	      cg->ppd_status = PPD_BAD_UI_CONSTRAINTS;
	      goto error;
	    }

	   /*
	    * The following strcpy's are safe, as optionN and
	    * choiceN are all the same size (size defined by PPD spec...)
	    */

	    if (constraint->option1[0] == '*')
	      _cups_strcpy(constraint->option1, constraint->option1 + 1);
	    else if (cg->ppd_conform == PPD_CONFORM_STRICT)
	    {
	      cg->ppd_status = PPD_BAD_UI_CONSTRAINTS;
	      goto error;
	    }

	    if (constraint->choice1[0] == '*')
	      _cups_strcpy(constraint->option2, constraint->choice1 + 1);
	    else if (cg->ppd_conform == PPD_CONFORM_STRICT)
	    {
	      cg->ppd_status = PPD_BAD_UI_CONSTRAINTS;
	      goto error;
	    }

            constraint->choice1[0] = '\0';
            constraint->choice2[0] = '\0';
	    break;
	    
	case 3 : /* Two options, one choice... */
	   /*
	    * Check for broken constraints like "* Option"...
	    */

	    if (cg->ppd_conform == PPD_CONFORM_STRICT &&
	        (!strcmp(constraint->option1, "*") ||
	         !strcmp(constraint->choice1, "*") ||
	         !strcmp(constraint->option2, "*")))
	    {
	      cg->ppd_status = PPD_BAD_UI_CONSTRAINTS;
	      goto error;
	    }

	   /*
	    * The following _cups_strcpy's are safe, as optionN and
	    * choiceN are all the same size (size defined by PPD spec...)
	    */

	    if (constraint->option1[0] == '*')
	      _cups_strcpy(constraint->option1, constraint->option1 + 1);
	    else if (cg->ppd_conform == PPD_CONFORM_STRICT)
	    {
	      cg->ppd_status = PPD_BAD_UI_CONSTRAINTS;
	      goto error;
	    }

	    if (constraint->choice1[0] == '*')
	    {
	      if (cg->ppd_conform == PPD_CONFORM_STRICT &&
	          constraint->option2[0] == '*')
	      {
		cg->ppd_status = PPD_BAD_UI_CONSTRAINTS;
		goto error;
	      }

	      _cups_strcpy(constraint->choice2, constraint->option2);
	      _cups_strcpy(constraint->option2, constraint->choice1 + 1);
              constraint->choice1[0] = '\0';
	    }
	    else
	    {
	      if (constraint->option2[0] == '*')
  	        _cups_strcpy(constraint->option2, constraint->option2 + 1);
	      else if (cg->ppd_conform == PPD_CONFORM_STRICT)
	      {
		cg->ppd_status = PPD_BAD_UI_CONSTRAINTS;
		goto error;
	      }

              constraint->choice2[0] = '\0';
	    }
	    break;
	    
	case 4 : /* Two options, two choices... */
	   /*
	    * Check for broken constraints like "* Option"...
	    */

	    if (cg->ppd_conform == PPD_CONFORM_STRICT &&
	        (!strcmp(constraint->option1, "*") ||
	         !strcmp(constraint->choice1, "*") ||
	         !strcmp(constraint->option2, "*") ||
	         !strcmp(constraint->choice2, "*")))
	    {
	      cg->ppd_status = PPD_BAD_UI_CONSTRAINTS;
	      goto error;
	    }

	    if (constraint->option1[0] == '*')
	      _cups_strcpy(constraint->option1, constraint->option1 + 1);
	    else if (cg->ppd_conform == PPD_CONFORM_STRICT)
	    {
	      cg->ppd_status = PPD_BAD_UI_CONSTRAINTS;
	      goto error;
	    }

            if (cg->ppd_conform == PPD_CONFORM_STRICT &&
	        constraint->choice1[0] == '*')
	    {
	      cg->ppd_status = PPD_BAD_UI_CONSTRAINTS;
	      goto error;
	    }

	    if (constraint->option2[0] == '*')
  	      _cups_strcpy(constraint->option2, constraint->option2 + 1);
	    else if (cg->ppd_conform == PPD_CONFORM_STRICT)
	    {
	      cg->ppd_status = PPD_BAD_UI_CONSTRAINTS;
	      goto error;
	    }

            if (cg->ppd_conform == PPD_CONFORM_STRICT &&
	        constraint->choice2[0] == '*')
	    {
	      cg->ppd_status = PPD_BAD_UI_CONSTRAINTS;
	      goto error;
	    }
	    break;
      }

     /*
      * For CustomPageSize and InputSlot/ManualFeed, create a duplicate
      * constraint for PageRegion...
      */

      if (!strcasecmp(constraint->option1, "CustomPageSize") &&
          (!strcasecmp(constraint->option2, "InputSlot") ||
	   !strcasecmp(constraint->option2, "ManualFeed")))
      {
        ppd->num_consts ++;

        strcpy(constraint[1].option1, "PageRegion");
	strcpy(constraint[1].choice1, "Custom");
	strcpy(constraint[1].option2, constraint->option2);
	strcpy(constraint[1].choice2, constraint->choice2);
      }
      else if (!strcasecmp(constraint->option2, "CustomPageSize") &&
               (!strcasecmp(constraint->option1, "InputSlot") ||
	        !strcasecmp(constraint->option1, "ManualFeed")))
      {
        ppd->num_consts ++;

	strcpy(constraint[1].option1, constraint->option1);
	strcpy(constraint[1].choice1, constraint->choice1);
        strcpy(constraint[1].option2, "PageRegion");
	strcpy(constraint[1].choice2, "Custom");
      }

     /*
      * Handle CustomFoo option constraints...
      */

      if (!strncasecmp(constraint->option1, "Custom", 6) &&
          !strcasecmp(constraint->choice1, "True"))
      {
        _cups_strcpy(constraint->option1, constraint->option1 + 6);
	strcpy(constraint->choice1, "Custom");
      }

      if (!strncasecmp(constraint->option2, "Custom", 6) &&
          !strcasecmp(constraint->choice2, "True"))
      {
        _cups_strcpy(constraint->option2, constraint->option2 + 6);
	strcpy(constraint->choice2, "Custom");
      }

     /*
      * Don't add this one as an attribute...
      */

      ppd_free(string);
      string = NULL;
    }
    else if (!strcmp(keyword, "PaperDimension"))
    {
      if ((size = ppdPageSize(ppd, name)) == NULL)
	size = ppd_add_size(ppd, name);

      if (size == NULL)
      {
       /*
        * Unable to add or find size!
	*/

        cg->ppd_status = PPD_ALLOC_ERROR;

	goto error;
      }

      size->width  = (float)_cupsStrScand(string, &sptr, loc);
      size->length = (float)_cupsStrScand(sptr, NULL, loc);

      ppd_free(string);
      string = NULL;
    }
    else if (!strcmp(keyword, "ImageableArea"))
    {
      if ((size = ppdPageSize(ppd, name)) == NULL)
	size = ppd_add_size(ppd, name);

      if (size == NULL)
      {
       /*
        * Unable to add or find size!
	*/

        cg->ppd_status = PPD_ALLOC_ERROR;

	goto error;
      }

      size->left   = (float)_cupsStrScand(string, &sptr, loc);
      size->bottom = (float)_cupsStrScand(sptr, &sptr, loc);
      size->right  = (float)_cupsStrScand(sptr, &sptr, loc);
      size->top    = (float)_cupsStrScand(sptr, NULL, loc);

      ppd_free(string);
      string = NULL;
    }
    else if (option != NULL &&
             (mask & (PPD_KEYWORD | PPD_OPTION | PPD_STRING)) ==
	         (PPD_KEYWORD | PPD_OPTION | PPD_STRING) &&
	     !strcmp(keyword, option->keyword))
    {
      DEBUG_printf(("group = %p, subgroup = %p\n", group, subgroup));

      if (!strcmp(keyword, "PageSize"))
      {
       /*
        * Add a page size...
	*/

        if (ppdPageSize(ppd, name) == NULL)
	  ppd_add_size(ppd, name);
      }

     /*
      * Add the option choice...
      */

      choice = ppd_add_choice(option, name);

      if (text[0])
        cupsCharsetToUTF8((cups_utf8_t *)choice->text, text,
	                   sizeof(choice->text), encoding);
      else if (!strcmp(name, "True"))
        strcpy(choice->text, _("Yes"));
      else if (!strcmp(name, "False"))
        strcpy(choice->text, _("No"));
      else
        strlcpy(choice->text, name, sizeof(choice->text));

      if (option->section == PPD_ORDER_JCL)
        ppd_decode(string);		/* Decode quoted string */

      choice->code = string;
      string       = NULL;		/* Don't add as an attribute below */
    }

   /*
    * Add remaining lines with keywords and string values as attributes...
    */

    if (string &&
        (mask & (PPD_KEYWORD | PPD_STRING)) == (PPD_KEYWORD | PPD_STRING))
      ppd_add_attr(ppd, keyword, name, text, string);
    else
      ppd_free(string);
  }

 /*
  * Reset language preferences...
  */

  cupsLangFree(language);

#ifdef DEBUG
  if (!cupsFileEOF(fp))
    printf("Premature EOF at %lu...\n", (unsigned long)cupsFileTell(fp));
#endif /* DEBUG */

  if (cg->ppd_status != PPD_OK)
  {
   /*
    * Had an error reading the PPD file, cannot continue!
    */

    ppdClose(ppd);

    return (NULL);
  }

 /*
  * Create the sorted options array and set the option back-pointer for
  * each choice and custom option...
  */

  ppd->options = cupsArrayNew2((cups_array_func_t)ppd_compare_options, NULL,
                               (cups_ahash_func_t)ppd_hash_option,
			       PPD_HASHSIZE);

  for (i = ppd->num_groups, group = ppd->groups;
       i > 0;
       i --, group ++)
  {
    for (j = group->num_options, option = group->options;
         j > 0;
	 j --, option ++)
    {
      ppd_coption_t	*coption;	/* Custom option */


      cupsArrayAdd(ppd->options, option);

      for (k = 0; k < option->num_choices; k ++)
        option->choices[k].option = option;

      if ((coption = ppdFindCustomOption(ppd, option->keyword)) != NULL)
        coption->option = option;
    }
  }

 /*
  * Sort the constraints...
  */

  if (ppd->num_consts > 1)
    qsort(ppd->consts, ppd->num_consts, sizeof(ppd_const_t),
          (int (*)(const void *, const void *))ppd_compare_consts);

 /*
  * Create an array to track the marked choices...
  */

  ppd->marked = cupsArrayNew((cups_array_func_t)ppd_compare_choices, NULL);

 /*
  * Return the PPD file structure...
  */

  return (ppd);

 /*
  * Common exit point for errors to save code size...
  */

  error:

  ppd_free(string);

  ppdClose(ppd);

  cupsLangFree(language);

  return (NULL);
}


/*
 * 'ppdOpenFd()' - Read a PPD file into memory.
 */

ppd_file_t *				/* O - PPD file record */
ppdOpenFd(int fd)			/* I - File to read from */
{
  cups_file_t		*fp;		/* CUPS file pointer */
  ppd_file_t		*ppd;		/* PPD file record */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */


 /*
  * Set the line number to 0...
  */

  cg->ppd_line = 0;

 /*
  * Range check input...
  */

  if (fd < 0)
  {
    cg->ppd_status = PPD_NULL_FILE;

    return (NULL);
  }

 /*
  * Try to open the file and parse it...
  */

  if ((fp = cupsFileOpenFd(fd, "r")) != NULL)
  {
    ppd = ppdOpen2(fp);

    cupsFileClose(fp);
  }
  else
  {
    cg->ppd_status = PPD_FILE_OPEN_ERROR;
    ppd            = NULL;
  }

  return (ppd);
}


/*
 * 'ppdOpenFile()' - Read a PPD file into memory.
 */

ppd_file_t *				/* O - PPD file record */
ppdOpenFile(const char *filename)	/* I - File to read from */
{
  cups_file_t		*fp;		/* File pointer */
  ppd_file_t		*ppd;		/* PPD file record */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */


 /*
  * Set the line number to 0...
  */

  cg->ppd_line = 0;

 /*
  * Range check input...
  */

  if (filename == NULL)
  {
    cg->ppd_status = PPD_NULL_FILE;

    return (NULL);
  }

 /*
  * Try to open the file and parse it...
  */

  if ((fp = cupsFileOpen(filename, "r")) != NULL)
  {
    ppd = ppdOpen2(fp);

    cupsFileClose(fp);
  }
  else
  {
    cg->ppd_status = PPD_FILE_OPEN_ERROR;
    ppd            = NULL;
  }

  return (ppd);
}


/*
 * 'ppdSetConformance()' - Set the conformance level for PPD files.
 *
 * @since CUPS 1.1.20@
 */

void
ppdSetConformance(ppd_conform_t c)	/* I - Conformance level */
{
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */


  cg->ppd_conform = c;
}


/*
 * 'ppd_add_attr()' - Add an attribute to the PPD data.
 */

static ppd_attr_t *			/* O - New attribute */
ppd_add_attr(ppd_file_t *ppd,		/* I - PPD file data */
             const char *name,		/* I - Attribute name */
             const char *spec,		/* I - Specifier string, if any */
	     const char *text,		/* I - Text string, if any */
	     const char *value)		/* I - Value of attribute */
{
  ppd_attr_t	**ptr,			/* New array */
		*temp;			/* New attribute */


 /*
  * Range check input...
  */

  if (ppd == NULL || name == NULL || spec == NULL)
    return (NULL);

 /*
  * Create the array as needed...
  */

  if (!ppd->sorted_attrs)
    ppd->sorted_attrs = cupsArrayNew((cups_array_func_t)ppd_compare_attrs,
                                     NULL);

 /*
  * Allocate memory for the new attribute...
  */

  if (ppd->num_attrs == 0)
    ptr = malloc(sizeof(ppd_attr_t *));
  else
    ptr = realloc(ppd->attrs, (ppd->num_attrs + 1) * sizeof(ppd_attr_t *));

  if (ptr == NULL)
    return (NULL);

  ppd->attrs = ptr;
  ptr += ppd->num_attrs;

  if ((temp = calloc(1, sizeof(ppd_attr_t))) == NULL)
    return (NULL);

  *ptr = temp;

  ppd->num_attrs ++;

 /*
  * Copy data over...
  */

  strlcpy(temp->name, name, sizeof(temp->name));
  strlcpy(temp->spec, spec, sizeof(temp->spec));
  strlcpy(temp->text, text, sizeof(temp->text));
  temp->value = (char *)value;

 /*
  * Add the attribute to the sorted array...
  */

  cupsArrayAdd(ppd->sorted_attrs, temp);

 /*
  * Return the attribute...
  */

  return (temp);
}


/*
 * 'ppd_add_choice()' - Add a choice to an option.
 */

static ppd_choice_t *			/* O - Named choice */
ppd_add_choice(ppd_option_t *option,	/* I - Option */
               const char   *name)	/* I - Name of choice */
{
  ppd_choice_t	*choice;		/* Choice */


  if (option->num_choices == 0)
    choice = malloc(sizeof(ppd_choice_t));
  else
    choice = realloc(option->choices,
	             sizeof(ppd_choice_t) * (option->num_choices + 1));

  if (choice == NULL)
    return (NULL);

  option->choices = choice;
  choice += option->num_choices;
  option->num_choices ++;

  memset(choice, 0, sizeof(ppd_choice_t));
  strlcpy(choice->choice, name, sizeof(choice->choice));

  return (choice);
}


/*
 * 'ppd_add_size()' - Add a page size.
 */

static ppd_size_t *			/* O - Named size */
ppd_add_size(ppd_file_t *ppd,		/* I - PPD file */
             const char *name)		/* I - Name of size */
{
  ppd_size_t	*size;			/* Size */


  if (ppd->num_sizes == 0)
    size = malloc(sizeof(ppd_size_t));
  else
    size = realloc(ppd->sizes, sizeof(ppd_size_t) * (ppd->num_sizes + 1));

  if (size == NULL)
    return (NULL);

  ppd->sizes = size;
  size += ppd->num_sizes;
  ppd->num_sizes ++;

  memset(size, 0, sizeof(ppd_size_t));
  strlcpy(size->name, name, sizeof(size->name));

  return (size);
}


/*
 * 'ppd_compare_attrs()' - Compare two attributes.
 */

static int				/* O - Result of comparison */
ppd_compare_attrs(ppd_attr_t *a,	/* I - First attribute */
                  ppd_attr_t *b)	/* I - Second attribute */
{
  int	ret;				/* Result of comparison */


  if ((ret = strcasecmp(a->name, b->name)) != 0)
    return (ret);
  else
    return (strcasecmp(a->spec, b->spec));
}


/*
 * 'ppd_compare_choices()' - Compare two choices...
 */

static int				/* O - Result of comparison */
ppd_compare_choices(ppd_choice_t *a,	/* I - First choice */
                    ppd_choice_t *b)	/* I - Second choice */
{
  return (a->option - b->option);
}


/*
 * 'ppd_compare_consts()' - Compare two constraints.
 */

static int				/* O - Result of comparison */
ppd_compare_consts(ppd_const_t *a,	/* I - First constraint */
                   ppd_const_t *b)	/* I - Second constraint */
{
  int	ret;				/* Result of comparison */


  if ((ret = strcmp(a->option1, b->option1)) != 0)
    return (ret);
  else if ((ret = strcmp(a->choice1, b->choice1)) != 0)
    return (ret);
  else if ((ret = strcmp(a->option2, b->option2)) != 0)
    return (ret);
  else
    return (strcmp(a->choice2, b->choice2));
}


/*
 * 'ppd_compare_coptions()' - Compare two custom options.
 */

static int				/* O - Result of comparison */
ppd_compare_coptions(ppd_coption_t *a,	/* I - First option */
                     ppd_coption_t *b)	/* I - Second option */
{
  return (strcasecmp(a->keyword, b->keyword));
}


/*
 * 'ppd_compare_cparams()' - Compare two custom parameters.
 */

static int				/* O - Result of comparison */
ppd_compare_cparams(ppd_cparam_t *a,	/* I - First parameter */
                    ppd_cparam_t *b)	/* I - Second parameter */
{
  return (strcasecmp(a->name, b->name));
}


/*
 * 'ppd_compare_options()' - Compare two options.
 */

static int				/* O - Result of comparison */
ppd_compare_options(ppd_option_t *a,	/* I - First option */
                    ppd_option_t *b)	/* I - Second option */
{
  return (strcasecmp(a->keyword, b->keyword));
}


/*
 * 'ppd_decode()' - Decode a string value...
 */

static int				/* O - Length of decoded string */
ppd_decode(char *string)		/* I - String to decode */
{
  char	*inptr,				/* Input pointer */
	*outptr;			/* Output pointer */


  inptr  = string;
  outptr = string;

  while (*inptr != '\0')
    if (*inptr == '<' && isxdigit(inptr[1] & 255))
    {
     /*
      * Convert hex to 8-bit values...
      */

      inptr ++;
      while (isxdigit(*inptr & 255))
      {
	if (isalpha(*inptr))
	  *outptr = (tolower(*inptr) - 'a' + 10) << 4;
	else
	  *outptr = (*inptr - '0') << 4;

	inptr ++;

        if (!isxdigit(*inptr & 255))
	  break;

	if (isalpha(*inptr))
	  *outptr |= tolower(*inptr) - 'a' + 10;
	else
	  *outptr |= *inptr - '0';

	inptr ++;
	outptr ++;
      }

      while (*inptr != '>' && *inptr != '\0')
	inptr ++;
      while (*inptr == '>')
	inptr ++;
    }
    else
      *outptr++ = *inptr++;

  *outptr = '\0';

  return ((int)(outptr - string));
}


/*
 * 'ppd_free_group()' - Free a single UI group.
 */

static void
ppd_free_group(ppd_group_t *group)	/* I - Group to free */
{
  int		i;			/* Looping var */
  ppd_option_t	*option;		/* Current option */
  ppd_group_t	*subgroup;		/* Current sub-group */


  if (group->num_options > 0)
  {
    for (i = group->num_options, option = group->options;
         i > 0;
	 i --, option ++)
      ppd_free_option(option);

    ppd_free(group->options);
  }

  if (group->num_subgroups > 0)
  {
    for (i = group->num_subgroups, subgroup = group->subgroups;
         i > 0;
	 i --, subgroup ++)
      ppd_free_group(subgroup);

    ppd_free(group->subgroups);
  }
}


/*
 * 'ppd_free_option()' - Free a single option.
 */

static void
ppd_free_option(ppd_option_t *option)	/* I - Option to free */
{
  int		i;			/* Looping var */
  ppd_choice_t	*choice;		/* Current choice */


  if (option->num_choices > 0)
  {
    for (i = option->num_choices, choice = option->choices;
         i > 0;
         i --, choice ++)
    {
      ppd_free(choice->code);
    }

    ppd_free(option->choices);
  }
}


/*
 * 'ppd_get_coption()' - Get a custom option record.
 */

static ppd_coption_t	*		/* O - Custom option... */
ppd_get_coption(ppd_file_t *ppd,	/* I - PPD file */
                const char *name)	/* I - Name of option */
{
  ppd_coption_t	*copt;			/* New custom option */


 /*
  * See if the option already exists...
  */

  if ((copt = ppdFindCustomOption(ppd, name)) != NULL)
    return (copt);

 /*
  * Not found, so create the custom option record...
  */

  if ((copt = calloc(1, sizeof(ppd_coption_t))) == NULL)
    return (NULL);

  strlcpy(copt->keyword, name, sizeof(copt->keyword));

  copt->params = cupsArrayNew((cups_array_func_t)ppd_compare_cparams, NULL);

  cupsArrayAdd(ppd->coptions, copt);

 /*
  * Return the new record...
  */

  return (copt);
}


/*
 * 'ppd_get_cparam()' - Get a custom parameter record.
 */

static ppd_cparam_t *			/* O - Extended option... */
ppd_get_cparam(ppd_coption_t *opt,	/* I - PPD file */
               const char    *param,	/* I - Name of parameter */
	       const char    *text)	/* I - Human-readable text */
{
  ppd_cparam_t	*cparam;		/* New custom parameter */


 /*
  * See if the parameter already exists...
  */

  if ((cparam = ppdFindCustomParam(opt, param)) != NULL)
    return (cparam);

 /*
  * Not found, so create the custom parameter record...
  */

  if ((cparam = calloc(1, sizeof(ppd_cparam_t))) == NULL)
    return (NULL);

  strlcpy(cparam->name, param, sizeof(cparam->name));
  strlcpy(cparam->text, text[0] ? text : param, sizeof(cparam->text));

 /*
  * Add this record to the array...
  */

  cupsArrayAdd(opt->params, cparam);

 /*
  * Return the new record...
  */

  return (cparam);
}


/*
 * 'ppd_get_group()' - Find or create the named group as needed.
 */

static ppd_group_t *			/* O - Named group */
ppd_get_group(ppd_file_t      *ppd,	/* I - PPD file */
              const char      *name,	/* I - Name of group */
	      const char      *text,	/* I - Text for group */
              _cups_globals_t *cg,	/* I - Global data */
	      cups_encoding_t encoding)	/* I - Encoding of text */
{
  int		i;			/* Looping var */
  ppd_group_t	*group;			/* Group */


  DEBUG_printf(("ppd_get_group(ppd=%p, name=\"%s\", text=\"%s\", cg=%p)\n",
                ppd, name, text, cg));

  for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
    if (!strcmp(group->name, name))
      break;

  if (i == 0)
  {
    DEBUG_printf(("Adding group %s...\n", name));

    if (cg->ppd_conform == PPD_CONFORM_STRICT && strlen(text) >= sizeof(group->text))
    {
      cg->ppd_status = PPD_ILLEGAL_TRANSLATION;

      return (NULL);
    }
	    
    if (ppd->num_groups == 0)
      group = malloc(sizeof(ppd_group_t));
    else
      group = realloc(ppd->groups,
	              (ppd->num_groups + 1) * sizeof(ppd_group_t));

    if (group == NULL)
    {
      cg->ppd_status = PPD_ALLOC_ERROR;

      return (NULL);
    }

    ppd->groups = group;
    group += ppd->num_groups;
    ppd->num_groups ++;

    memset(group, 0, sizeof(ppd_group_t));
    strlcpy(group->name, name, sizeof(group->name));

    cupsCharsetToUTF8((cups_utf8_t *)group->text, text,
	               sizeof(group->text), encoding);
  }

  return (group);
}


/*
 * 'ppd_get_option()' - Find or create the named option as needed.
 */

static ppd_option_t *			/* O - Named option */
ppd_get_option(ppd_group_t *group,	/* I - Group */
               const char  *name)	/* I - Name of option */
{
  int		i;			/* Looping var */
  ppd_option_t	*option;		/* Option */


  DEBUG_printf(("ppd_get_option(group=%p(\"%s\"), name=\"%s\")\n",
                group, group->name, name));

  for (i = group->num_options, option = group->options; i > 0; i --, option ++)
    if (!strcmp(option->keyword, name))
      break;

  if (i == 0)
  {
    if (group->num_options == 0)
      option = malloc(sizeof(ppd_option_t));
    else
      option = realloc(group->options,
	               (group->num_options + 1) * sizeof(ppd_option_t));

    if (option == NULL)
      return (NULL);

    group->options = option;
    option += group->num_options;
    group->num_options ++;

    memset(option, 0, sizeof(ppd_option_t));
    strlcpy(option->keyword, name, sizeof(option->keyword));
  }

  return (option);
}


/*
 * 'ppd_hash_option()' - Generate a hash of the option name...
 */

static int				/* O - Hash index */
ppd_hash_option(ppd_option_t *option)	/* I - Option */
{
  int		hash = 0;		/* Hash index */
  const char	*k;			/* Pointer into keyword */


  for (hash = option->keyword[0], k = option->keyword + 1; *k;)
    hash = 33 * hash + *k++;

  return (hash & 511);
}


/*
 * 'ppd_read()' - Read a line from a PPD file, skipping comment lines as
 *                necessary.
 */

static int				/* O - Bitmask of fields read */
ppd_read(cups_file_t    *fp,		/* I - File to read from */
         char           *keyword,	/* O - Keyword from line */
	 char           *option,	/* O - Option from line */
         char           *text,		/* O - Human-readable text from line */
	 char           **string,	/* O - Code/string data */
         int            ignoreblank,	/* I - Ignore blank lines? */
	 _cups_globals_t *cg)		/* I - Global data */
{
  int		ch,			/* Character from file */
		col,			/* Column in line */
		colon,			/* Colon seen? */
		endquote,		/* Waiting for an end quote */
		mask,			/* Mask to be returned */
		startline,		/* Start line */
		textlen;		/* Length of text */
  char		*keyptr,		/* Keyword pointer */
		*optptr,		/* Option pointer */
		*textptr,		/* Text pointer */
		*strptr,		/* Pointer into string */
		*lineptr,		/* Current position in line buffer */
		*line;			/* Line buffer */
  int		linesize;		/* Current size of line buffer */

 /*
  * Range check everything...
  */

  if (!fp || !keyword || !option || !text || !string)
    return (0);

 /*
  * Now loop until we have a valid line...
  */

  *string   = NULL;
  col       = 0;
  startline = cg->ppd_line + 1;
  linesize  = 1024;
  line      = malloc(linesize);

  if (!line)
    return (0);

  do
  {
   /*
    * Read the line...
    */

    lineptr  = line;
    endquote = 0;
    colon    = 0;

    while ((ch = cupsFileGetChar(fp)) != EOF)
    {
      if (lineptr >= (line + linesize - 1))
      {
       /*
        * Expand the line buffer...
	*/

        char *temp;			/* Temporary line pointer */


        linesize += 1024;
	if (linesize > 262144)
	{
	 /*
	  * Don't allow lines longer than 256k!
	  */

          cg->ppd_line   = startline;
          cg->ppd_status = PPD_LINE_TOO_LONG;

	  free(line);

	  return (0);
	}

        temp = realloc(line, linesize);
	if (!temp)
	{
          cg->ppd_line   = startline;
          cg->ppd_status = PPD_LINE_TOO_LONG;

	  free(line);

	  return (0);
	}

        lineptr = temp + (lineptr - line);
	line    = temp;
      }

      if (ch == '\r' || ch == '\n')
      {
       /*
	* Line feed or carriage return...
	*/

        cg->ppd_line ++;
	col = 0;

	if (ch == '\r')
	{
	 /*
          * Check for a trailing line feed...
	  */

	  if ((ch = cupsFilePeekChar(fp)) == EOF)
	  {
	    ch = '\n';
	    break;
	  }

	  if (ch == 0x0a)
	    cupsFileGetChar(fp);
	}

	if (lineptr == line && ignoreblank)
          continue;			/* Skip blank lines */

	ch = '\n';

	if (!endquote)			/* Continue for multi-line text */
          break;

	*lineptr++ = '\n';
      }
      else if (ch < ' ' && ch != '\t' && cg->ppd_conform == PPD_CONFORM_STRICT)
      {
       /*
        * Other control characters...
	*/

        cg->ppd_line   = startline;
        cg->ppd_status = PPD_ILLEGAL_CHARACTER;

        free(line);

        return (0);
      }
      else if (ch != 0x1a)
      {
       /*
	* Any other character...
	*/

	*lineptr++ = ch;
	col ++;

	if (col > (PPD_MAX_LINE - 1))
	{
	 /*
          * Line is too long...
	  */

          cg->ppd_line   = startline;
          cg->ppd_status = PPD_LINE_TOO_LONG;

          free(line);

          return (0);
	}

	if (ch == ':' && strncmp(line, "*%", 2) != 0)
	  colon = 1;

	if (ch == '\"' && colon)
	  endquote = !endquote;
      }
    }

    if (endquote)
    {
     /*
      * Didn't finish this quoted string...
      */

      while ((ch = cupsFileGetChar(fp)) != EOF)
        if (ch == '\"')
	  break;
	else if (ch == '\r' || ch == '\n')
	{
	  cg->ppd_line ++;
	  col = 0;

	  if (ch == '\r')
	  {
	   /*
            * Check for a trailing line feed...
	    */

	    if ((ch = cupsFilePeekChar(fp)) == EOF)
	      break;
	    if (ch == 0x0a)
	      cupsFileGetChar(fp);
	  }

	  ch = '\n';
	}
	else if (ch < ' ' && ch != '\t' && cg->ppd_conform == PPD_CONFORM_STRICT)
	{
	 /*
          * Other control characters...
	  */

          cg->ppd_line   = startline;
          cg->ppd_status = PPD_ILLEGAL_CHARACTER;

          free(line);

          return (0);
	}
	else if (ch != 0x1a)
	{
	  col ++;

	  if (col > (PPD_MAX_LINE - 1))
	  {
	   /*
            * Line is too long...
	    */

            cg->ppd_line   = startline;
            cg->ppd_status = PPD_LINE_TOO_LONG;

            free(line);

            return (0);
	  }
	}
    }

    if (ch != '\n')
    {
     /*
      * Didn't finish this line...
      */

      while ((ch = cupsFileGetChar(fp)) != EOF)
	if (ch == '\r' || ch == '\n')
	{
	 /*
	  * Line feed or carriage return...
	  */

          cg->ppd_line ++;
	  col = 0;

	  if (ch == '\r')
	  {
	   /*
            * Check for a trailing line feed...
	    */

	    if ((ch = cupsFilePeekChar(fp)) == EOF)
	      break;
	    if (ch == 0x0a)
	      cupsFileGetChar(fp);
	  }

	  break;
	}
	else if (ch < ' ' && ch != '\t' && cg->ppd_conform == PPD_CONFORM_STRICT)
	{
	 /*
          * Other control characters...
	  */

          cg->ppd_line   = startline;
          cg->ppd_status = PPD_ILLEGAL_CHARACTER;

          free(line);

          return (0);
	}
	else if (ch != 0x1a)
	{
	  col ++;

	  if (col > (PPD_MAX_LINE - 1))
	  {
	   /*
            * Line is too long...
	    */

            cg->ppd_line   = startline;
            cg->ppd_status = PPD_LINE_TOO_LONG;

            free(line);

            return (0);
	  }
	}
    }

    if (lineptr > line && lineptr[-1] == '\n')
      lineptr --;

    *lineptr = '\0';

    DEBUG_printf(("LINE = \"%s\"\n", line));

   /*
    * The dynamically created PPDs for older style Mac OS X
    * drivers include a large blob of data inserted as comments
    * at the end of the file.  As an optimization we can stop
    * reading the PPD when we get to the start of this data.
    */

    if (!strcmp(line, "*%APLWORKSET START"))
    {
      free(line);
      return (0);
    }

    if (ch == EOF && lineptr == line)
    {
      free(line);
      return (0);
    }

   /*
    * Now parse it...
    */

    mask    = 0;
    lineptr = line + 1;

    keyword[0] = '\0';
    option[0]  = '\0';
    text[0]    = '\0';
    *string    = NULL;

    if ((!line[0] ||			/* Blank line */
         !strncmp(line, "*%", 2) ||	/* Comment line */
         !strcmp(line, "*End")) &&	/* End of multi-line string */
        ignoreblank)			/* Ignore these? */
    {
      startline = cg->ppd_line + 1;
      continue;
    }

    if (!strcmp(line, "*"))		/* (Bad) comment line */
    {
      if (cg->ppd_conform == PPD_CONFORM_RELAXED)
      {
	startline = cg->ppd_line + 1;
	continue;
      }
      else
      {
        cg->ppd_line   = startline;
        cg->ppd_status = PPD_ILLEGAL_MAIN_KEYWORD;

        free(line);
        return (0);
      }
    }

    if (line[0] != '*')			/* All lines start with an asterisk */
    {
     /*
      * Allow lines consisting of just whitespace...
      */

      for (lineptr = line; *lineptr; lineptr ++)
        if (!isspace(*lineptr & 255))
	  break;

      if (*lineptr)
      {
        cg->ppd_status = PPD_MISSING_ASTERISK;
        free(line);
        return (0);
      }
      else if (ignoreblank)
        continue;
      else
      {
        free(line);
        return (0);
      }
    }

   /*
    * Get a keyword...
    */

    keyptr = keyword;

    while (*lineptr != '\0' && *lineptr != ':' && !isspace(*lineptr & 255))
    {
      if (*lineptr <= ' ' || *lineptr > 126 || *lineptr == '/' ||
          (keyptr - keyword) >= (PPD_MAX_NAME - 1))
      {
        cg->ppd_status = PPD_ILLEGAL_MAIN_KEYWORD;
        free(line);
	return (0);
      }

      *keyptr++ = *lineptr++;
    }

    *keyptr = '\0';

    if (!strcmp(keyword, "End"))
      continue;

    mask |= PPD_KEYWORD;

/*    DEBUG_printf(("keyword = \"%s\", lineptr = \"%s\"\n", keyword, lineptr));*/

    if (isspace(*lineptr & 255))
    {
     /*
      * Get an option name...
      */

      while (isspace(*lineptr & 255))
        lineptr ++;

      optptr = option;

      while (*lineptr != '\0' && !isspace(*lineptr & 255) && *lineptr != ':' &&
             *lineptr != '/')
      {
	if (*lineptr <= ' ' || *lineptr > 126 ||
	    (optptr - option) >= (PPD_MAX_NAME - 1))
        {
          cg->ppd_status = PPD_ILLEGAL_OPTION_KEYWORD;
          free(line);
	  return (0);
	}

        *optptr++ = *lineptr++;
      }

      *optptr = '\0';

      if (isspace(*lineptr & 255) && cg->ppd_conform == PPD_CONFORM_STRICT)
      {
        cg->ppd_status = PPD_ILLEGAL_WHITESPACE;
        free(line);
	return (0);
      }

      while (isspace(*lineptr & 255))
	lineptr ++;

      mask |= PPD_OPTION;

/*      DEBUG_printf(("option = \"%s\", lineptr = \"%s\"\n", option, lineptr));*/

      if (*lineptr == '/')
      {
       /*
        * Get human-readable text...
	*/

        lineptr ++;
	
	textptr = text;

	while (*lineptr != '\0' && *lineptr != '\n' && *lineptr != ':')
	{
	  if (((unsigned char)*lineptr < ' ' && *lineptr != '\t') ||
	      (textptr - text) >= (PPD_MAX_LINE - 1))
	  {
	    cg->ppd_status = PPD_ILLEGAL_TRANSLATION;
            free(line);
	    return (0);
	  }

	  *textptr++ = *lineptr++;
        }

	*textptr = '\0';
	textlen  = ppd_decode(text);

	if (textlen > PPD_MAX_TEXT && cg->ppd_conform == PPD_CONFORM_STRICT)
	{
	  cg->ppd_status = PPD_ILLEGAL_TRANSLATION;
          free(line);
	  return (0);
	}
	    
	mask |= PPD_TEXT;
      }

/*      DEBUG_printf(("text = \"%s\", lineptr = \"%s\"\n", text, lineptr));*/
    }

    if (isspace(*lineptr & 255) && cg->ppd_conform == PPD_CONFORM_STRICT)
    {
      cg->ppd_status = PPD_ILLEGAL_WHITESPACE;
      free(line);
      return (0);
    }

    while (isspace(*lineptr & 255))
      lineptr ++;

    if (*lineptr == ':')
    {
     /*
      * Get string after triming leading and trailing whitespace...
      */

      lineptr ++;
      while (isspace(*lineptr & 255))
        lineptr ++;

      strptr = lineptr + strlen(lineptr) - 1;
      while (strptr >= lineptr && isspace(*strptr & 255))
        *strptr-- = '\0';

      if (*strptr == '\"')
      {
       /*
        * Quoted string by itself...
	*/

	*string = malloc(strlen(lineptr) + 1);

	strptr = *string;

	for (; *lineptr != '\0'; lineptr ++)
	  if (*lineptr != '\"')
	    *strptr++ = *lineptr;

	*strptr = '\0';
      }
      else
        *string = strdup(lineptr);

/*      DEBUG_printf(("string = \"%s\", lineptr = \"%s\"\n", *string, lineptr));*/

      mask |= PPD_STRING;
    }
  }
  while (mask == 0);

  free(line);

  return (mask);
}


/*
 * End of "$Id$".
 */
