/*
 * "$Id: ppd.c,v 1.51.2.15 2002/05/16 13:59:59 mike Exp $"
 *
 *   PPD file routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
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
 *   ppdClose()        - Free all memory used by the PPD file.
 *   ppd_free_group()  - Free a single UI group.
 *   ppd_free_option() - Free a single option.
 *   ppdOpen()         - Read a PPD file into memory.
 *   ppdOpenFd()       - Read a PPD file into memory.
 *   ppdOpenFile()     - Read a PPD file into memory.
 *   ppd_read()        - Read a line from a PPD file, skipping comment lines
 *                       as necessary.
 *   compare_strings() - Compare two strings.
 *   compare_groups()  - Compare two groups.
 *   compare_options() - Compare two options.
 *   compare_choices() - Compare two choices.
 */

/*
 * Include necessary headers.
 */

#include "ppd.h"
#include <stdlib.h>
#include <ctype.h>
#include "string.h"
#include "language.h"
#include "debug.h"


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

#define safe_free(p)	if (p) free(p)	/* Safe free macro */

#define PPD_KEYWORD	1		/* Line contained a keyword */
#define PPD_OPTION	2		/* Line contained an option name */
#define PPD_TEXT	4		/* Line contained human-readable text */
#define PPD_STRING	8		/* Line contained a string or code */


/*
 * Local functions...
 */

static int		compare_strings(char *s, char *t);
static int		compare_groups(ppd_group_t *g0, ppd_group_t *g1);
static int		compare_options(ppd_option_t *o0, ppd_option_t *o1);
static int		compare_choices(ppd_choice_t *c0, ppd_choice_t *c1);
static int		ppd_read(FILE *fp, char *keyword, char *option,
			         char *text, char **string);
static void		ppd_decode(char *string);
static void		ppd_fix(char *string);
static void		ppd_free_group(ppd_group_t *group);
static void		ppd_free_option(ppd_option_t *option);
static ppd_group_t	*ppd_get_group(ppd_file_t *ppd, const char *name,
			               const char *text);
static ppd_attr_t	*ppd_add_attr(ppd_file_t *ppd, const char *name,
			              const char *spec, const char *value);
static ppd_option_t	*ppd_get_option(ppd_group_t *group, const char *name);
static ppd_choice_t	*ppd_add_choice(ppd_option_t *option, const char *name);


/*
 * 'ppdClose()' - Free all memory used by the PPD file.
 */

void
ppdClose(ppd_file_t *ppd)	/* I - PPD file record */
{
  int		i;		/* Looping var */
  ppd_emul_t	*emul;		/* Current emulation */
  ppd_group_t	*group;		/* Current group */
  char		**font;		/* Current font */
  char		**filter;	/* Current filter */
  ppd_attr_t	**attr;		/* Current attribute */


 /*
  * Range check the PPD file record...
  */

  if (ppd == NULL)
    return;

 /*
  * Free all strings at the top level...
  */

  safe_free(ppd->patches);
  safe_free(ppd->jcl_begin);
  safe_free(ppd->jcl_ps);
  safe_free(ppd->jcl_end);
  safe_free(ppd->lang_encoding);
  safe_free(ppd->lang_version);
  safe_free(ppd->modelname);
  safe_free(ppd->ttrasterizer);
  safe_free(ppd->manufacturer);
  safe_free(ppd->product);
  safe_free(ppd->nickname);
  safe_free(ppd->shortnickname);

 /*
  * Free any emulations...
  */

  if (ppd->num_emulations > 0)
  {
    for (i = ppd->num_emulations, emul = ppd->emulations; i > 0; i --, emul ++)
    {
      safe_free(emul->start);
      safe_free(emul->stop);
    }

    safe_free(ppd->emulations);
  }

 /*
  * Free any UI groups, subgroups, and options...
  */

  if (ppd->num_groups > 0)
  {
    for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
      ppd_free_group(group);

    safe_free(ppd->groups);
  }

 /*
  * Free any page sizes...
  */

  if (ppd->num_sizes > 0)
    safe_free(ppd->sizes);

 /*
  * Free any constraints...
  */

  if (ppd->num_consts > 0)
    safe_free(ppd->consts);

 /*
  * Free any filters...
  */

  if (ppd->num_filters > 0)
  {
    for (i = ppd->num_filters, filter = ppd->filters; i > 0; i --, filter ++)
      safe_free(*filter);

    safe_free(ppd->filters);
  }

 /*
  * Free any fonts...
  */

  if (ppd->num_fonts > 0)
  {
    for (i = ppd->num_fonts, font = ppd->fonts; i > 0; i --, font ++)
      safe_free(*font);

    safe_free(ppd->fonts);
  }

 /*
  * Free any profiles...
  */

  if (ppd->num_profiles > 0)
    safe_free(ppd->profiles);

 /*
  * Free any attributes...
  */

  if (ppd->num_attrs > 0)
  {
    for (i = ppd->num_attrs, attr = ppd->attrs; i > 0; i --, attr ++)
    {
      safe_free((*attr)->value);
      safe_free(*attr);
    }

    safe_free(ppd->attrs);
  }

 /*
  * Free the whole record...
  */

  safe_free(ppd);
}


/*
 * 'ppd_add_attr()' - Add an attribute to the PPD data.
 */

static ppd_attr_t *		/* O - New attribute */
ppd_add_attr(ppd_file_t *ppd,	/* I - PPD file data */
             const char *name,	/* I - Attribute name */
             const char *spec,	/* I - Specifier string, if any */
	     const char *value)	/* I - Value of attribute */
{
  ppd_attr_t	**ptr,		/* New array */
		*temp;		/* New attribute */


 /*
  * Range check input...
  */

  if (ppd == NULL || name == NULL || spec == NULL)
    return (NULL);

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
  temp->value = (char *)value;

 /*
  * Return the attribute...
  */

  return (temp);
}


/*
 * '_ppd_attr_compare()' - Compare two attributes.
 */

int					/* O - Result of comparison */
_ppd_attr_compare(ppd_attr_t **a,	/* I - First attribute */
                  ppd_attr_t **b)	/* I - Second attribute */
{
  int	ret;				/* Result of comparison */


  if ((ret = strcasecmp((*a)->name, (*b)->name)) != 0)
    return (ret);
  else if ((*a)->spec[0] && (*b)->spec[0])
    return (strcasecmp((*a)->spec, (*b)->spec));
  else
    return (0);
}


/*
 * 'ppd_free_group()' - Free a single UI group.
 */

static void
ppd_free_group(ppd_group_t *group)	/* I - Group to free */
{
  int		i;		/* Looping var */
  ppd_option_t	*option;	/* Current option */
  ppd_group_t	*subgroup;	/* Current sub-group */


  if (group->num_options > 0)
  {
    for (i = group->num_options, option = group->options;
         i > 0;
	 i --, option ++)
      ppd_free_option(option);

    safe_free(group->options);
  }

  if (group->num_subgroups > 0)
  {
    for (i = group->num_subgroups, subgroup = group->subgroups;
         i > 0;
	 i --, subgroup ++)
      ppd_free_group(subgroup);

    safe_free(group->subgroups);
  }
}


/*
 * 'ppd_free_option()' - Free a single option.
 */

static void
ppd_free_option(ppd_option_t *option)	/* I - Option to free */
{
  int		i;		/* Looping var */
  ppd_choice_t	*choice;	/* Current choice */


  if (option->num_choices > 0)
  {
    for (i = option->num_choices, choice = option->choices;
         i > 0;
         i --, choice ++)
      safe_free(choice->code);

    safe_free(option->choices);
  }
}


/*
 * 'ppd_get_group()' - Find or create the named group as needed.
 */

static ppd_group_t *		/* O - Named group */
ppd_get_group(ppd_file_t *ppd,	/* I - PPD file */
              const char *name,	/* I - Name of group */
	      const char *text)	/* I - Text for group */
{
  int		i;		/* Looping var */
  ppd_group_t	*group;		/* Group */


  DEBUG_printf(("ppd_get_group(%p, \"%s\")\n", ppd, name));

  for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
    if (strcmp(group->name, name) == 0)
      break;

  if (i == 0)
  {
    DEBUG_printf(("Adding group %s...\n", name));

    if (ppd->num_groups == 0)
      group = malloc(sizeof(ppd_group_t));
    else
      group = realloc(ppd->groups,
	              (ppd->num_groups + 1) * sizeof(ppd_group_t));

    if (group == NULL)
      return (NULL);

    ppd->groups = group;
    group += ppd->num_groups;
    ppd->num_groups ++;

    memset(group, 0, sizeof(ppd_group_t));
    strlcpy(group->name, name, sizeof(group->name));
    strlcpy(group->text, text, sizeof(group->text));
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


  for (i = group->num_options, option = group->options; i > 0; i --, option ++)
    if (strcmp(option->keyword, name) == 0)
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

static ppd_size_t *		/* O - Named size */
ppd_add_size(ppd_file_t *ppd,	/* I - PPD file */
             const char *name)	/* I - Name of size */
{
  ppd_size_t	*size;		/* Size */


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
 * 'ppdOpen()' - Read a PPD file into memory.
 */

ppd_file_t *			/* O - PPD file record */
ppdOpen(FILE *fp)		/* I - File to read from */
{
  int		i, j, k, m;	/* Looping vars */
  int		count;		/* Temporary count */
  ppd_file_t	*ppd;		/* PPD file record */
  ppd_group_t	*group,		/* Current group */
		*subgroup;	/* Current sub-group */
  ppd_option_t	*option;	/* Current option */
  ppd_choice_t	*choice;	/* Current choice */
  ppd_const_t	*constraint;	/* Current constraint */
  ppd_size_t	*size;		/* Current page size */
  int		mask;		/* Line data mask */
  char		keyword[PPD_MAX_NAME],
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
  float		order;		/* Order dependency number */
  ppd_section_t	section;	/* Order dependency section */
  ppd_profile_t	*profile;	/* Pointer to color profile */
  char		**filter;	/* Pointer to filter */
  cups_lang_t	*language;	/* Default language */


 /*
  * Get the default language for the user...
  */

  language = cupsLangDefault();

 /*
  * Range check input...
  */

  if (fp == NULL)
    return (NULL);

 /*
  * Grab the first line and make sure it reads '*PPD-Adobe: "major.minor"'...
  */

  mask = ppd_read(fp, keyword, name, text, &string);

  if (mask == 0 ||
      strcmp(keyword, "PPD-Adobe") != 0 ||
      string == NULL || string[0] != '4')
  {
   /*
    * Either this is not a PPD file, or it is not a 4.x PPD file.
    */

    safe_free(string);

    return (NULL);
  }

  DEBUG_printf(("ppdOpen: keyword = %s, string = %p\n", keyword, string));

  safe_free(string);

 /*
  * Allocate memory for the PPD file record...
  */

  if ((ppd = calloc(sizeof(ppd_file_t), 1)) == NULL)
    return (NULL);

  ppd->language_level = 1;
  ppd->color_device   = 0;
  ppd->colorspace     = PPD_CS_GRAY;
  ppd->landscape      = -90;

 /*
  * Read lines from the PPD file and add them to the file record...
  */

  group    = NULL;
  subgroup = NULL;
  option   = NULL;
  choice   = NULL;

  while ((mask = ppd_read(fp, keyword, name, text, &string)) != 0)
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

    if (strcmp(keyword, "CloseUI") != 0 &&
        strcmp(keyword, "JCLCloseUI") != 0 &&
        strcmp(keyword, "CloseGroup") != 0 &&
	strcmp(keyword, "CloseSubGroup") != 0 &&
	strncmp(keyword, "Default", 7) != 0 &&
	string == NULL)
    {
     /*
      * Need a string value!
      */

      ppdClose(ppd);
      return (NULL);
    }

    if (strcmp(keyword, "LanguageLevel") == 0)
      ppd->language_level = atoi(string);
    else if (strcmp(keyword, "LanguageEncoding") == 0)
    {
      ppd->lang_encoding = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "LanguageVersion") == 0)
    {
      ppd->lang_version = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "Manufacturer") == 0)
    {
      ppd->manufacturer = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "ModelName") == 0)
    {
      ppd->modelname = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "PCFileName") == 0)
    {
      ppd->pcfilename = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "NickName") == 0)
    {
      ppd->nickname = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "Product") == 0)
    {
      ppd->product = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "ShortNickName") == 0)
    {
      ppd->shortnickname = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "TTRasterizer") == 0)
    {
      ppd->ttrasterizer = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "JCLBegin") == 0)
    {
      ppd_decode(string);		/* Decode quoted string */
      ppd->jcl_begin = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "JCLEnd") == 0)
    {
      ppd_decode(string);		/* Decode quoted string */
      ppd->jcl_end = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "JCLToPSInterpreter") == 0)
    {
      ppd_decode(string);		/* Decode quoted string */
      ppd->jcl_ps = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "AccurateScreensSupport") == 0)
      ppd->accurate_screens = strcmp(string, "True") == 0;
    else if (strcmp(keyword, "ColorDevice") == 0)
      ppd->color_device = strcmp(string, "True") == 0;
    else if (strcmp(keyword, "ContoneOnly") == 0)
      ppd->contone_only = strcmp(string, "True") == 0;
    else if (strcmp(keyword, "DefaultColorSpace") == 0)
    {
      if (strcmp(string, "CMY") == 0)
        ppd->colorspace = PPD_CS_CMY;
      else if (strcmp(string, "CMYK") == 0)
        ppd->colorspace = PPD_CS_CMYK;
      else if (strcmp(string, "RGB") == 0)
        ppd->colorspace = PPD_CS_RGB;
      else if (strcmp(string, "RGBK") == 0)
        ppd->colorspace = PPD_CS_RGBK;
      else if (strcmp(string, "N") == 0)
        ppd->colorspace = PPD_CS_N;
      else
        ppd->colorspace = PPD_CS_GRAY;
    }
    else if (strcmp(keyword, "cupsFlipDuplex") == 0)
      ppd->flip_duplex = strcmp(string, "True") == 0;
    else if (strcmp(keyword, "cupsManualCopies") == 0)
      ppd->manual_copies = strcmp(string, "True") == 0;
    else if (strcmp(keyword, "cupsModelNumber") == 0)
      ppd->model_number = atoi(string);
    else if (strcmp(keyword, "cupsColorProfile") == 0)
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
      sscanf(string, "%f%f%f%f%f%f%f%f%f%f%f", &(profile->density),
	     &(profile->gamma),
	     profile->matrix[0] + 0, profile->matrix[0] + 1,
	     profile->matrix[0] + 2, profile->matrix[1] + 0,
	     profile->matrix[1] + 1, profile->matrix[1] + 2,
	     profile->matrix[2] + 0, profile->matrix[2] + 1,
	     profile->matrix[2] + 2);
    }
    else if (strcmp(keyword, "cupsFilter") == 0)
    {
      if (ppd->num_filters == 0)
        filter = malloc(sizeof(char *));
      else
        filter = realloc(ppd->filters, sizeof(char *) * (ppd->num_filters + 1));

      if (filter == NULL)
      {
        safe_free(filter);
	ppdClose(ppd);
	return (NULL);
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
    else if (strcmp(keyword, "Throughput") == 0)
      ppd->throughput = atoi(string);
    else if (strcmp(keyword, "Font") == 0)
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
        safe_free(string);
        ppdClose(ppd);
	return (NULL);
      }
      
      ppd->fonts                 = tempfonts;
      ppd->fonts[ppd->num_fonts] = strdup(name);
      ppd->num_fonts ++;
    }
    else if (strcmp(keyword, "VariablePaperSize") == 0 &&
             strcmp(string, "True") == 0 &&
	     !ppd->variable_sizes)
    {
      ppd->variable_sizes = 1;

     /*
      * Add a "Custom" page size entry...
      */

      ppd_add_size(ppd, "Custom");

     /*
      * Add a "Custom" page size option...
      */

      if ((option = ppdFindOption(ppd, "PageSize")) == NULL)
      {
        ppd_group_t	*temp;


	if ((temp = ppd_get_group(ppd, "General",
                                  cupsLangString(language,
                                                 CUPS_MSG_GENERAL))) == NULL)
	{
          ppdClose(ppd);
	  safe_free(string);
	  return (NULL);
	}

	if ((option = ppd_get_option(temp, "PageSize")) == NULL)
	{
          ppdClose(ppd);
	  safe_free(string);
	  return (NULL);
	}
      }

      if ((choice = ppd_add_choice(option, "Custom")) == NULL)
      {
        ppdClose(ppd);
	safe_free(string);
	return (NULL);
      }

      strlcpy(choice->text, cupsLangString(language, CUPS_MSG_VARIABLE),
              sizeof(choice->text));
      option = NULL;
    }
    else if (strcmp(keyword, "MaxMediaWidth") == 0)
      ppd->custom_max[0] = (float)atof(string);
    else if (strcmp(keyword, "MaxMediaHeight") == 0)
      ppd->custom_max[1] = (float)atof(string);
    else if (strcmp(keyword, "ParamCustomPageSize") == 0)
    {
      if (strcmp(name, "Width") == 0)
        sscanf(string, "%*s%*s%f%f", ppd->custom_min + 0,
	       ppd->custom_max + 0);
      else if (strcmp(name, "Height") == 0)
        sscanf(string, "%*s%*s%f%f", ppd->custom_min + 1,
	       ppd->custom_max + 1);
    }
    else if (strcmp(keyword, "HWMargins") == 0)
      sscanf(string, "%f%f%f%f", ppd->custom_margins + 0,
             ppd->custom_margins + 1, ppd->custom_margins + 2,
             ppd->custom_margins + 3);
    else if (strcmp(keyword, "CustomPageSize") == 0 &&
             strcmp(name, "True") == 0)
    {
      if (!ppd->variable_sizes)
      {
	ppd->variable_sizes = 1;

       /*
	* Add a "Custom" page size entry...
	*/

	ppd_add_size(ppd, "Custom");

       /*
	* Add a "Custom" page size option...
	*/

        if ((option = ppdFindOption(ppd, "PageSize")) == NULL)
	{
	  ppd_group_t	*temp;


	  if ((temp = ppd_get_group(ppd, "General",
                                    cupsLangString(language,
                                                   CUPS_MSG_GENERAL))) == NULL)
	  {
	    DEBUG_puts("Unable to get general group!");
            ppdClose(ppd);
	    safe_free(string);
	    return (NULL);
	  }

	  if ((option = ppd_get_option(temp, "PageSize")) == NULL)
	  {
	    DEBUG_puts("Unable to get PageSize option!");
            ppdClose(ppd);
	    safe_free(string);
	    return (NULL);
	  }
        }

	if ((choice = ppd_add_choice(option, "Custom")) == NULL)
	{
	  DEBUG_puts("Unable to add Custom choice!");
          ppdClose(ppd);
	  safe_free(string);
	  return (NULL);
	}

	strlcpy(choice->text, cupsLangString(language, CUPS_MSG_VARIABLE),
        	sizeof(choice->text));
	option = NULL;
      }

      if ((option = ppdFindOption(ppd, "PageSize")) == NULL)
      {
	DEBUG_puts("Unable to find PageSize option!");
	ppdClose(ppd);
	safe_free(string);
	return (NULL);
      }

      if ((choice = ppdFindChoice(option, "Custom")) == NULL)
      {
	DEBUG_puts("Unable to find Custom choice!");
        ppdClose(ppd);
	safe_free(string);
	return (NULL);
      }

      choice->code = string;
      string = NULL;
      option = NULL;
    }
    else if (strcmp(keyword, "LandscapeOrientation") == 0)
    {
      if (strcmp(string, "Minus90") == 0)
        ppd->landscape = -90;
      else if (strcmp(string, "Plus90") == 0)
        ppd->landscape = 90;
    }
    else if (strcmp(keyword, "Emulators") == 0)
    {
      for (count = 1, sptr = string; sptr != NULL;)
        if ((sptr = strchr(sptr, ' ')) != NULL)
	{
	  count ++;
	  while (*sptr == ' ')
	    sptr ++;
	}

      ppd->num_emulations = count;
      ppd->emulations     = calloc(sizeof(ppd_emul_t), count);

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
    else if (strncmp(keyword, "StartEmulator_", 14) == 0)
    {
      ppd_decode(string);

      for (i = 0; i < ppd->num_emulations; i ++)
        if (strcmp(keyword + 14, ppd->emulations[i].name) == 0)
	{
	  ppd->emulations[i].start = string;
	  string = NULL;
	}
    }
    else if (strncmp(keyword, "StopEmulator_", 13) == 0)
    {
      ppd_decode(string);

      for (i = 0; i < ppd->num_emulations; i ++)
        if (strcmp(keyword + 13, ppd->emulations[i].name) == 0)
	{
	  ppd->emulations[i].stop = string;
	  string = NULL;
	}
    }
    else if (strcmp(keyword, "JobPatchFile") == 0)
    {
      if (ppd->patches == NULL)
      {
        ppd->patches = string;
	string       = NULL;
      }
      else
      {
        temp = realloc(ppd->patches, strlen(ppd->patches) +
	                             strlen(string) + 1);
        if (temp == NULL)
	{
	  safe_free(string);
	  ppdClose(ppd);
	  return (NULL);
	}

        ppd->patches = temp;

        strcpy(ppd->patches + strlen(ppd->patches), string);
      }
    }
    else if (strcmp(keyword, "OpenUI") == 0)
    {
     /*
      * Add an option record to the current sub-group, group, or file...
      */

      if (name[0] == '*')
        strcpy(name, name + 1); /* Eliminate leading asterisk */

      for (i = strlen(name) - 1; i > 0 && isspace(name[i]); i --)
        name[i] = '\0'; /* Eliminate trailing spaces */

      DEBUG_printf(("OpenUI of %s in group %s...\n", name,
                    group ? group->text : "(null)"));

      if (subgroup != NULL)
        option = ppd_get_option(subgroup, name);
      else if (group == NULL)
      {
        if (strcmp(name, "Collate") != 0 &&
            strcmp(name, "Duplex") != 0 &&
            strcmp(name, "InputSlot") != 0 &&
            strcmp(name, "ManualFeed") != 0 &&
            strcmp(name, "MediaType") != 0 &&
            strcmp(name, "MediaColor") != 0 &&
            strcmp(name, "MediaWeight") != 0 &&
            strcmp(name, "OutputBin") != 0 &&
            strcmp(name, "OutputMode") != 0 &&
            strcmp(name, "OutputOrder") != 0 &&
	    strcmp(name, "PageSize") != 0 &&
            strcmp(name, "PageRegion") != 0)
	  group = ppd_get_group(ppd, "Extra",
	                        cupsLangString(language, CUPS_MSG_EXTRA));
	else
	  group = ppd_get_group(ppd, "General",
	                        cupsLangString(language, CUPS_MSG_GENERAL));

        if (group == NULL)
	{
	  ppdClose(ppd);
	  safe_free(string);
	  return (NULL);
	}

        DEBUG_printf(("Adding to group %s...\n", group->text));
        option = ppd_get_option(group, name);
	group  = NULL;
      }
      else
        option = ppd_get_option(group, name);

      if (option == NULL)
      {
	ppdClose(ppd);
	safe_free(string);
	return (NULL);
      }

     /*
      * Now fill in the initial information for the option...
      */

      if (strcmp(string, "PickMany") == 0)
        option->ui = PPD_UI_PICKMANY;
      else if (strcmp(string, "Boolean") == 0)
        option->ui = PPD_UI_BOOLEAN;
      else
        option->ui = PPD_UI_PICKONE;

      if (text[0])
      {
        strlcpy(option->text, text, sizeof(option->text));
	ppd_fix(option->text);
      }
      else
      {
        if (strcmp(name, "PageSize") == 0)
	  strlcpy(option->text, cupsLangString(language, CUPS_MSG_MEDIA_SIZE),
                  sizeof(option->text));
	else if (strcmp(name, "MediaType") == 0)
	  strlcpy(option->text, cupsLangString(language, CUPS_MSG_MEDIA_TYPE),
                  sizeof(option->text));
	else if (strcmp(name, "InputSlot") == 0)
	  strlcpy(option->text, cupsLangString(language, CUPS_MSG_MEDIA_SOURCE),
                  sizeof(option->text));
	else if (strcmp(name, "ColorModel") == 0)
	  strlcpy(option->text, cupsLangString(language, CUPS_MSG_OUTPUT_MODE),
                  sizeof(option->text));
	else if (strcmp(name, "Resolution") == 0)
	  strlcpy(option->text, cupsLangString(language, CUPS_MSG_RESOLUTION),
                  sizeof(option->text));
        else
	  strlcpy(option->text, name, sizeof(option->text));
      }

      option->section = PPD_ORDER_ANY;
    }
    else if (strcmp(keyword, "JCLOpenUI") == 0)
    {
     /*
      * Find the JCL group, and add if needed...
      */

      group = ppd_get_group(ppd, "JCL", "JCL");

      if (group == NULL)
      {
        ppdClose(ppd);
	safe_free(string);
	return (NULL);
      }

     /*
      * Add an option record to the current JCLs...
      */

      if (name[0] == '*')
        strcpy(name, name + 1);

      option = ppd_get_option(group, name);

      if (option == NULL)
      {
        ppdClose(ppd);
	safe_free(string);
	return (NULL);
      }

     /*
      * Now fill in the initial information for the option...
      */

      if (strcmp(string, "PickMany") == 0)
        option->ui = PPD_UI_PICKMANY;
      else if (strcmp(string, "Boolean") == 0)
        option->ui = PPD_UI_BOOLEAN;
      else
        option->ui = PPD_UI_PICKONE;

      strlcpy(option->text, text, sizeof(option->text));

      option->section = PPD_ORDER_JCL;
      group = NULL;
    }
    else if (strcmp(keyword, "CloseUI") == 0 ||
             strcmp(keyword, "JCLCloseUI") == 0)
      option = NULL;
    else if (strcmp(keyword, "OpenGroup") == 0)
    {
     /*
      * Open a new group...
      */

      if (group != NULL)
      {
        ppdClose(ppd);
	safe_free(string);
	return (NULL);
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
      ppd_fix(sptr);

     /*
      * Find/add the group...
      */

      group = ppd_get_group(ppd, string, sptr);
    }
    else if (strcmp(keyword, "CloseGroup") == 0)
      group = NULL;
    else if (strcmp(keyword, "OrderDependency") == 0 ||
             strcmp(keyword, "NonUIOrderDependency") == 0)
    {
      if (sscanf(string, "%f%40s%40s", &order, name, keyword) != 3)
      {
        ppdClose(ppd);
	safe_free(string);
	return (NULL);
      }

      if (keyword[0] == '*')
        strcpy(keyword, keyword + 1);

      if (strcmp(name, "ExitServer") == 0)
        section = PPD_ORDER_EXIT;
      else if (strcmp(name, "Prolog") == 0)
        section = PPD_ORDER_PROLOG;
      else if (strcmp(name, "DocumentSetup") == 0)
        section = PPD_ORDER_DOCUMENT;
      else if (strcmp(name, "PageSetup") == 0)
        section = PPD_ORDER_PAGE;
      else if (strcmp(name, "JCLSetup") == 0)
        section = PPD_ORDER_JCL;
      else
        section = PPD_ORDER_ANY;

      if (option == NULL)
      {
        ppd_group_t	*temp;


       /*
        * Only valid for Non-UI options...
	*/

        for (i = ppd->num_groups, temp = ppd->groups; i > 0; i --, temp ++)
          if (temp->text[0] == '\0')
	    break;

        if (i > 0)
          for (i = 0; i < temp->num_options; i ++)
	    if (strcmp(keyword, temp->options[i].keyword) == 0)
	    {
	      temp->options[i].section = section;
	      temp->options[i].order   = order;
	      break;
	    }
      }
      else
      {
        option->section = section;
	option->order   = order;
      }
    }
    else if (strncmp(keyword, "Default", 7) == 0)
    {
      if (string == NULL)
        continue;

      if (strchr(string, '/') != NULL)
        *strchr(string, '/') = '\0';

      if (option == NULL)
      {
        ppd_group_t	*temp;


       /*
        * Only valid for Non-UI options...
	*/

        for (i = ppd->num_groups, temp = ppd->groups; i > 0; i --, temp ++)
          if (temp->text[0] == '\0')
	    break;

        if (i > 0)
          for (i = 0; i < temp->num_options; i ++)
	    if (strcmp(keyword, temp->options[i].keyword) == 0)
	    {
	      strlcpy(temp->options[i].defchoice, string,
                      sizeof(temp->options[i].defchoice));
	      break;
	    }
      }
      else
        strlcpy(option->defchoice, string, sizeof(option->defchoice));
    }
    else if (strcmp(keyword, "UIConstraints") == 0 ||
             strcmp(keyword, "NonUIConstraints") == 0)
    {
      if (ppd->num_consts == 0)
	constraint = calloc(sizeof(ppd_const_t), 1);
      else
	constraint = realloc(ppd->consts,
	                     (ppd->num_consts + 1) * sizeof(ppd_const_t));

      if (constraint == NULL)
      {
	ppdClose(ppd);
	safe_free(string);
	return (NULL);
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
	    ppdClose(ppd);
  	    safe_free(string);
	    break;

	case 2 : /* Two options... */
	   /*
	    * The following strcpy's are safe, as optionN and
	    * choiceN are all the same size (size defined by PPD spec...)
	    */

	    if (constraint->option1[0] == '*')
	      strcpy(constraint->option1, constraint->option1 + 1);

	    if (constraint->choice1[0] == '*')
	      strcpy(constraint->option2, constraint->choice1 + 1);
	    else
	      strcpy(constraint->option2, constraint->choice1);

            constraint->choice1[0] = '\0';
            constraint->choice2[0] = '\0';
	    break;
	    
	case 3 : /* Two options, one choice... */
	   /*
	    * The following strcpy's are safe, as optionN and
	    * choiceN are all the same size (size defined by PPD spec...)
	    */

	    if (constraint->option1[0] == '*')
	      strcpy(constraint->option1, constraint->option1 + 1);

	    if (constraint->choice1[0] == '*')
	    {
	      strcpy(constraint->choice2, constraint->option2);
	      strcpy(constraint->option2, constraint->choice1 + 1);
              constraint->choice1[0] = '\0';
	    }
	    else
	    {
	      if (constraint->option2[0] == '*')
  	        strcpy(constraint->option2, constraint->option2 + 1);

              constraint->choice2[0] = '\0';
	    }
	    break;
	    
	case 4 : /* Two options, two choices... */
	    if (constraint->option1[0] == '*')
	      strcpy(constraint->option1, constraint->option1 + 1);

	    if (constraint->option2[0] == '*')
  	      strcpy(constraint->option2, constraint->option2 + 1);
	    break;
      }
    }
    else if (strcmp(keyword, "PaperDimension") == 0)
    {
      if ((size = ppdPageSize(ppd, name)) == NULL)
	size = ppd_add_size(ppd, name);

      if (size == NULL)
      {
       /*
        * Unable to add or find size!
	*/

        ppdClose(ppd);
	safe_free(string);
	return (NULL);
      }

      sscanf(string, "%f%f", &(size->width), &(size->length));
    }
    else if (strcmp(keyword, "ImageableArea") == 0)
    {
      if ((size = ppdPageSize(ppd, name)) == NULL)
	size = ppd_add_size(ppd, name);

      if (size == NULL)
      {
       /*
        * Unable to add or find size!
	*/

        ppdClose(ppd);
	safe_free(string);
	return (NULL);
      }

      sscanf(string, "%f%f%f%f", &(size->left), &(size->bottom),
	     &(size->right), &(size->top));
    }
    else if (option != NULL &&
             (mask & (PPD_KEYWORD | PPD_OPTION | PPD_STRING)) ==
	         (PPD_KEYWORD | PPD_OPTION | PPD_STRING))
    {
      DEBUG_printf(("group = %p, subgroup = %p\n", group, subgroup));

      if (strcmp(keyword, "PageSize") == 0)
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

      if (mask & PPD_TEXT)
      {
        strlcpy(choice->text, text, sizeof(choice->text));
        ppd_fix(choice->text);
      }
      else if (strcmp(name, "True") == 0)
        strcpy(choice->text, "Yes");
      else if (strcmp(name, "False") == 0)
        strcpy(choice->text, "No");
      else
        strlcpy(choice->text, name, sizeof(choice->text));

      if (option->section == PPD_ORDER_JCL)
        ppd_decode(string);		/* Decode quoted string */

      choice->code = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "OpenSubGroup") != 0 &&
             strcmp(keyword, "CloseSubGroup") != 0)
    {
      char	spec[PPD_MAX_NAME + PPD_MAX_TEXT];

      snprintf(spec, sizeof(spec), "%s/%s", name, text);
      ppd_add_attr(ppd, keyword, spec, string);

      string = NULL;			/* Don't free this string below */
    }

    safe_free(string);
  }

#ifdef DEBUG
  if (!feof(fp))
    printf("Premature EOF at %lu...\n", (unsigned long)ftell(fp));
#endif /* DEBUG */

 /*
  * Set the option back-pointer for each choice...
  */

  qsort(ppd->groups, ppd->num_groups, sizeof(ppd_group_t),
        (int (*)(const void *, const void *))compare_groups);

  for (i = ppd->num_groups, group = ppd->groups;
       i > 0;
       i --, group ++)
  {
    qsort(group->options, group->num_options, sizeof(ppd_option_t),
          (int (*)(const void *, const void *))compare_options);

    for (j = group->num_options, option = group->options;
         j > 0;
	 j --, option ++)
    {
      qsort(option->choices, option->num_choices, sizeof(ppd_choice_t),
            (int (*)(const void *, const void *))compare_choices);

      for (k = 0; k < option->num_choices; k ++)
        option->choices[k].option = (void *)option;
    }

    qsort(group->subgroups, group->num_subgroups, sizeof(ppd_group_t),
          (int (*)(const void *, const void *))compare_groups);

    for (j = group->num_subgroups, subgroup = group->subgroups;
         j > 0;
	 j --, subgroup ++)
    {
      qsort(subgroup->options, subgroup->num_options, sizeof(ppd_option_t),
            (int (*)(const void *, const void *))compare_options);

      for (k = group->num_options, option = group->options;
           k > 0;
	   k --, option ++)
      {
	qsort(option->choices, option->num_choices, sizeof(ppd_choice_t),
              (int (*)(const void *, const void *))compare_choices);

        for (m = 0; m < option->num_choices; m ++)
          option->choices[m].option = (void *)option;
      }
    }
  }

 /*
  * Sort the attributes...
  */

  if (ppd->num_attrs > 1)
    qsort(ppd->attrs, ppd->num_attrs, sizeof(ppd_attr_t *),
          (int (*)(const void *, const void *))_ppd_attr_compare);

  return (ppd);
}


/*
 * 'ppdOpenFd()' - Read a PPD file into memory.
 */

ppd_file_t *			/* O - PPD file record */
ppdOpenFd(int fd)		/* I - File to read from */
{
  FILE		*fp;		/* File pointer */
  ppd_file_t	*ppd;		/* PPD file record */


 /*
  * Range check input...
  */

  if (fd < 0)
    return (NULL);

 /*
  * Try to open the file and parse it...
  */

  if ((fp = fdopen(fd, "r")) != NULL)
  {
    setbuf(fp, NULL);

    ppd = ppdOpen(fp);

    safe_free(fp);
  }
  else
    ppd = NULL;

  return (ppd);
}


/*
 * 'ppdOpenFile()' - Read a PPD file into memory.
 */

ppd_file_t *			/* O - PPD file record */
ppdOpenFile(const char *filename) /* I - File to read from */
{
  FILE		*fp;		/* File pointer */
  ppd_file_t	*ppd;		/* PPD file record */


 /*
  * Range check input...
  */

  if (filename == NULL)
    return (NULL);

 /*
  * Try to open the file and parse it...
  */

  if ((fp = fopen(filename, "r")) != NULL)
  {
    ppd = ppdOpen(fp);

    fclose(fp);
  }
  else
    ppd = NULL;

  return (ppd);
}


/*
 * 'compare_strings()' - Compare two strings.
 */

static int			/* O - Result of comparison */
compare_strings(char *s,	/* I - First string */
                char *t)	/* I - Second string */
{
  int	diff,			/* Difference between digits */
	digits;			/* Number of digits */


 /*
  * Loop through both strings, returning only when a difference is
  * seen.  Also, compare whole numbers rather than just characters, too!
  */

  while (*s && *t)
  {
    if (isdigit(*s) && isdigit(*t))
    {
     /*
      * Got a number; start by skipping leading 0's...
      */

      while (*s == '0')
        s ++;
      while (*t == '0')
        t ++;

     /*
      * Skip equal digits...
      */

      while (isdigit(*s) && *s == *t)
      {
        s ++;
	t ++;
      }

     /*
      * Bounce out if *s and *t aren't both digits...
      */

      if (isdigit(*s) && !isdigit(*t))
        return (1);
      else if (!isdigit(*s) && isdigit(*t))
        return (-1);
      else if (!isdigit(*s) || !isdigit(*t))
        continue;     

      if (*s < *t)
        diff = -1;
      else
        diff = 1;

     /*
      * Figure out how many more digits there are...
      */

      digits = 0;

      while (isdigit(*s))
      {
        digits ++;
	s ++;
      }

      while (isdigit(*t))
      {
        digits --;
	t ++;
      }

     /*
      * Return if the number or value of the digits is different...
      */

      if (digits < 0)
        return (-1);
      else if (digits > 0)
        return (1);
      else
        return (diff);
    }
    else if (tolower(*s) < tolower(*t))
      return (-1);
    else if (tolower(*s) > tolower(*t))
      return (1);
    else
    {
      s ++;
      t ++;
    }
  }

 /*
  * Return the results of the final comparison...
  */

  if (*s)
    return (1);
  else if (*t)
    return (-1);
  else
    return (0);
}


/*
 * 'compare_groups()' - Compare two groups.
 */

static int			/* O - Result of comparison */
compare_groups(ppd_group_t *g0,	/* I - First group */
               ppd_group_t *g1)	/* I - Second group */
{
  return (compare_strings(g0->text, g1->text));
}


/*
 * 'compare_options()' - Compare two options.
 */

static int			/* O - Result of comparison */
compare_options(ppd_option_t *o0,/* I - First option */
                ppd_option_t *o1)/* I - Second option */
{
  return (compare_strings(o0->text, o1->text));
}


/*
 * 'compare_choices()' - Compare two choices.
 */

static int			/* O - Result of comparison */
compare_choices(ppd_choice_t *c0,/* I - First choice */
                ppd_choice_t *c1)/* I - Second choice */
{
  return (compare_strings(c0->text, c1->text));
}


/*
 * 'ppd_read()' - Read a line from a PPD file, skipping comment lines as
 *                necessary.
 */

static int			/* O - Bitmask of fields read */
ppd_read(FILE *fp,		/* I - File to read from */
         char *keyword,		/* O - Keyword from line */
	 char *option,		/* O - Option from line */
         char *text,		/* O - Human-readable text from line */
	 char **string)		/* O - Code/string data */
{
  int		ch,		/* Character from file */
		colon,		/* Colon seen? */
		endquote,	/* Waiting for an end quote */
		mask;		/* Mask to be returned */
  char		*keyptr,	/* Keyword pointer */
		*optptr,	/* Option pointer */
		*textptr,	/* Text pointer */
		*strptr,	/* Pointer into string */
		*lineptr,	/* Current position in line buffer */
		line[65536];	/* Line buffer (64k) */


 /*
  * Range check everything...
  */

  if (fp == NULL || keyword == NULL || option == NULL || text == NULL ||
      string == NULL)
    return (0);

 /*
  * Now loop until we have a valid line...
  */

  *string = NULL;

  do
  {
   /*
    * Read the line...
    */

    lineptr  = line;
    endquote = 0;
    colon    = 0;

    while ((ch = getc(fp)) != EOF &&
           (lineptr - line) < (sizeof(line) - 1))
    {
      if (ch == '\r' || ch == '\n')
      {
       /*
	* Line feed or carriage return...
	*/

	if (lineptr == line)		/* Skip blank lines */
          continue;

	if (ch == '\r')
	{
	 /*
          * Check for a trailing line feed...
	  */

	  if ((ch = getc(fp)) == EOF)
	    break;
	  if (ch != 0x0a)
	    ungetc(ch, fp);
	}

	ch = '\n';

	if (!endquote)			/* Continue for multi-line text */
          break;

	*lineptr++ = '\n';
      }
      else
      {
       /*
	* Any other character...
	*/

	*lineptr++ = ch;

	if (ch == ':' && strncmp(line, "*%", 2) != 0)
	  colon = 1;

	if (ch == '\"' && colon)
        {
	  endquote = !endquote;

          if (!endquote)
	  {
	   /*
	    * End of quoted string; ignore trailing characters...
	    */

	    while ((ch = getc(fp)) != EOF)
	      if (ch == '\n')
		break;
	      else if (ch == '\r')
	      {
		ch = getc(fp);
		if (ch != '\n')
	          ungetc(ch, fp);

		ch = '\n';
		break;
	      }

            break;
	  }
	}
      }
    }

    if (endquote)
    {
     /*
      * Didn't finish this quoted string...
      */

      while ((ch = getc(fp)) != EOF)
        if (ch == '\"')
	  break;
    }

    if (ch != '\n')
    {
     /*
      * Didn't finish this line...
      */

      while ((ch = getc(fp)) != EOF)
	if (ch == '\r' || ch == '\n')
	{
	 /*
	  * Line feed or carriage return...
	  */

	  if (ch == '\r')
	  {
	   /*
            * Check for a trailing line feed...
	    */

	    if ((ch = getc(fp)) == EOF)
	      break;
	    if (ch != 0x0a)
	      ungetc(ch, fp);
	  }

	  break;
	}
    }

    if (lineptr > line && lineptr[-1] == '\n')
      lineptr --;

    *lineptr = '\0';

/*    DEBUG_printf(("LINE = \"%s\"\n", line));*/

    if (ch == EOF && lineptr == line)
      return (0);

   /*
    * Now parse it...
    */

    mask    = 0;
    lineptr = line + 1;

    keyword[0] = '\0';
    option[0]  = '\0';
    text[0]    = '\0';
    *string    = NULL;

    if (line[0] != '*')			/* All lines start with an asterisk */
      continue;

    if (strcmp(line, "*") == 0 ||	/* (Bad) comment line */
        strncmp(line, "*%", 2) == 0 ||	/* Comment line */
        strncmp(line, "*?", 2) == 0 ||	/* Query line */
        strcmp(line, "*End") == 0)	/* End of multi-line string */
      continue;

   /*
    * Get a keyword...
    */

    keyptr = keyword;

    while (*lineptr != '\0' && *lineptr != ':' && !isspace(*lineptr))
      if ((keyptr - keyword) < (PPD_MAX_NAME - 1))
	*keyptr++ = *lineptr++;

    *keyptr = '\0';

    if (strcmp(keyword, "End") == 0)
      continue;

    mask |= PPD_KEYWORD;

/*    DEBUG_printf(("keyword = \"%s\", lineptr = \"%s\"\n", keyword, lineptr));*/

    if (isspace(*lineptr))
    {
     /*
      * Get an option name...
      */

      while (isspace(*lineptr))
        lineptr ++;

      optptr = option;

      while (*lineptr != '\0' && *lineptr != '\n' && *lineptr != ':' &&
             *lineptr != '/')
        if ((optptr - option) < (PPD_MAX_NAME - 1))
	  *optptr++ = *lineptr++;

      *optptr = '\0';
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
	  if ((textptr - text) < (PPD_MAX_LINE - 1))
	    *textptr++ = *lineptr++;

	*textptr = '\0';
	ppd_decode(text);

	mask |= PPD_TEXT;
      }

/*      DEBUG_printf(("text = \"%s\", lineptr = \"%s\"\n", text, lineptr));*/
    }

    if (*lineptr == ':')
    {
     /*
      * Get string...
      */

      *string = malloc(strlen(lineptr) + 1);

      while (*lineptr == ':' || isspace(*lineptr))
        lineptr ++;

      strptr = *string;

      while (*lineptr != '\0')
      {
	if (*lineptr != '\"')
	  *strptr++ = *lineptr++;
	else
	  lineptr ++;
      }

      *strptr = '\0';

/*      DEBUG_printf(("string = \"%s\", lineptr = \"%s\"\n", *string, lineptr));*/

      mask |= PPD_STRING;
    }
  }
  while (mask == 0);

  return (mask);
}


/*
 * 'ppd_decode()' - Decode a string value...
 */

static void
ppd_decode(char *string)	/* I - String to decode */
{
  char	*inptr,			/* Input pointer */
	*outptr;		/* Output pointer */


  inptr  = string;
  outptr = string;

  while (*inptr != '\0')
    if (*inptr == '<' && isxdigit(inptr[1]))
    {
     /*
      * Convert hex to 8-bit values...
      */

      inptr ++;
      while (isxdigit(*inptr))
      {
	if (isalpha(*inptr))
	  *outptr = (tolower(*inptr) - 'a' + 10) << 4;
	else
	  *outptr = (*inptr - '0') << 4;

	inptr ++;

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
}


/*
 * 'ppd_fix()' - Fix WinANSI characters in the range 0x80 to 0x9f to be
 *               valid ISO-8859-1 characters...
 */

static void
ppd_fix(char *string)		/* IO - String to fix */
{
  unsigned char		*p;	/* Pointer into string */
  static unsigned char	lut[32] =/* Lookup table for characters */
			{
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  0x20,
			  'l',
			  '`',
			  '\'',
			  '^',
			  '~',
			  0x20, /* bar */
			  0x20, /* circumflex */
			  0x20, /* dot */
			  0x20, /* double dot */
			  0x20,
			  0x20, /* circle */
			  0x20, /* ??? */
			  0x20,
			  '\"', /* should be right quotes */
			  0x20, /* ??? */
			  0x20  /* accent */
			};


  for (p = (unsigned char *)string; *p; p ++)
    if (*p >= 0x80 && *p < 0xa0)
      *p = lut[*p - 0x80];
}


/*
 * End of "$Id: ppd.c,v 1.51.2.15 2002/05/16 13:59:59 mike Exp $".
 */
