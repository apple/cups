/*
 * "$Id: ppd.c,v 1.15 1999/04/16 16:57:49 mike Exp $"
 *
 *   PPD file routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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
 */

/*
 * Include necessary headers.
 */

#include "ppd.h"
#include <stdlib.h>
#include <ctype.h>
#include "string.h"


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

#define PPD_KEYWORD	1		/* Line contained a keyword */
#define PPD_OPTION	2		/* Line contained an option name */
#define PPD_TEXT	4		/* Line contained human-readable text */
#define PPD_STRING	8		/* Line contained a string or code */


/*
 * Local functions...
 */

static int		ppd_read(FILE *fp, char *keyword, char *option,
			         unsigned char *text, unsigned char **string);
static void		ppd_decode(unsigned char *string);
static void		ppd_free_group(ppd_group_t *group);
static void		ppd_free_option(ppd_option_t *option);
static ppd_group_t	*ppd_get_group(ppd_file_t *ppd, char *name);
static ppd_option_t	*ppd_get_option(ppd_group_t *group, char *name);
static ppd_choice_t	*ppd_add_choice(ppd_option_t *option, char *name);


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


 /*
  * Range check the PPD file record...
  */

  if (ppd == NULL)
    return;

 /*
  * Free all strings at the top level...
  */

  free(ppd->lang_encoding);
  free(ppd->lang_version);
  free(ppd->modelname);
  free(ppd->ttrasterizer);
  free(ppd->manufacturer);
  free(ppd->product);
  free(ppd->nickname);
  free(ppd->shortnickname);

 /*
  * Free any emulations...
  */

  if (ppd->num_emulations > 0)
  {
    for (i = ppd->num_emulations, emul = ppd->emulations; i > 0; i --, emul ++)
    {
      free(emul->start);
      free(emul->stop);
    }

    free(ppd->emulations);
  }

 /*
  * Free any UI groups, subgroups, and options...
  */

  if (ppd->num_groups > 0)
  {
    for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
      ppd_free_group(group);

    free(ppd->groups);
  }

 /*
  * Free any page sizes...
  */

  if (ppd->num_sizes > 0)
    free(ppd->sizes);

 /*
  * Free any constraints...
  */

  if (ppd->num_consts > 0)
    free(ppd->consts);

 /*
  * Free any fonts...
  */

  if (ppd->num_fonts > 0)
  {
    for (i = ppd->num_fonts, font = ppd->fonts; i > 0; i --, font ++)
      free(*font);

    free(ppd->fonts);
  }

 /*
  * Free any profiles...
  */

  if (ppd->num_profiles > 0)
    free(ppd->profiles);

 /*
  * Free the whole record...
  */

  free(ppd);
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

    free(group->options);
  }

  if (group->num_subgroups > 0)
  {
    for (i = group->num_subgroups, subgroup = group->subgroups;
         i > 0;
	 i --, subgroup ++)
      ppd_free_group(subgroup);

    free(group->subgroups);
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
      free(choice->code);

    free(option->choices);
  }
}


/*
 * 'ppd_get_group()' - Find or create the named group as needed.
 */

static ppd_group_t *		/* O - Named group */
ppd_get_group(ppd_file_t *ppd,	/* I - PPD file */
              char       *name)	/* I - Name of group */
{
  int		i;		/* Looping var */
  ppd_group_t	*group;		/* Group */


  for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
    if (strcmp((char *)group->text, name) == 0)
      break;

  if (i == 0)
  {
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
    strcpy((char *)group->text, name);
  }

  return (group);
}


/*
 * 'ppd_get_option()' - Find or create the named option as needed.
 */

static ppd_option_t *			/* O - Named option */
ppd_get_option(ppd_group_t *group,	/* I - Group */
               char        *name)	/* I - Name of option */
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
    strcpy(option->keyword, name);
  }

  return (option);
}


/*
 * 'ppd_add_choice()' - Add a choice to an option.
 */

static ppd_choice_t *			/* O - Named choice */
ppd_add_choice(ppd_option_t *option,	/* I - Option */
               char         *name)	/* I - Name of choice */
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
  strcpy(choice->choice, name);

  return (choice);
}


/*
 * 'ppd_add_size()' - Add a page size.
 */

static ppd_size_t *		/* O - Named size */
ppd_add_size(ppd_file_t *ppd,	/* I - PPD file */
             char       *name)	/* I - Name of size */
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
  strcpy(size->name, name);

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
  char		keyword[41],	/* Keyword from file */
		name[41];	/* Option from file */
  unsigned char	text[81],	/* Human-readable text from file */
		*string;	/* Code/text from file */
  char		*sptr,		/* Pointer into string */
		*nameptr;	/* Pointer into name */
  float		order;		/* Order dependency number */
  ppd_section_t	section;	/* Order dependency section */
  ppd_profile_t	*profile;	/* Pointer to color profile */


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

    if (string != NULL)
      free(string);

    return (NULL);
  }

  if (string != NULL)
    free(string);

 /*
  * Allocate memory for the PPD file record...
  */

  if ((ppd = calloc(sizeof(ppd_file_t), 1)) == NULL)
    return (NULL);

  ppd->language_level = 1;
  ppd->color_device   = 0;
  ppd->colorspace     = PPD_CS_GRAY;
  ppd->landscape      = 90;

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
      if (strlen((char *)string) > 40)
        printf(", string = %08x", string);
      else
        printf(", string = \"%s\"", string);
    }

    puts("");
#endif /* DEBUG */

    if (strcmp(keyword, "LanguageLevel") == 0)
      ppd->language_level = atoi((char *)string);
    else if (strcmp(keyword, "LanguageEncoding") == 0)
    {
      ppd->lang_encoding = (char *)string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "LanguageVersion") == 0)
    {
      ppd->lang_version = (char *)string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "Manufacturer") == 0)
    {
      ppd->manufacturer = string;
      string = NULL;			/* Don't free this string below */
    }
    else if (strcmp(keyword, "ModelName") == 0)
    {
      ppd->modelname = (char *)string;
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
      ppd->ttrasterizer = (char *)string;
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
      ppd->accurate_screens = strcmp((char *)string, "True") == 0;
    else if (strcmp(keyword, "ColorDevice") == 0)
      ppd->color_device = strcmp((char *)string, "True") == 0;
    else if (strcmp(keyword, "ContoneOnly") == 0)
      ppd->contone_only = strcmp((char *)string, "True") == 0;
    else if (strcmp(keyword, "DefaultColorSpace") == 0)
    {
      if (strcmp((char *)string, "CMY") == 0)
        ppd->colorspace = PPD_CS_CMY;
      else if (strcmp((char *)string, "CMYK") == 0)
        ppd->colorspace = PPD_CS_CMYK;
      else if (strcmp((char *)string, "RGB") == 0)
        ppd->colorspace = PPD_CS_RGB;
      else if (strcmp((char *)string, "RGBK") == 0)
        ppd->colorspace = PPD_CS_RGBK;
      else if (strcmp((char *)string, "N") == 0)
        ppd->colorspace = PPD_CS_N;
      else
        ppd->colorspace = PPD_CS_GRAY;
    }
    else if (strcmp(keyword, "cupsManualCopies") == 0)
      ppd->manual_copies = strcmp((char *)string, "True") == 0;
    else if (strcmp(keyword, "cupsModelNumber") == 0)
      ppd->model_number = atoi((char *)string);
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
      strcpy(profile->resolution, name);
      strcpy(profile->media_type, (char *)text);
      sscanf((char *)string, "%f%f%f%f%f%f%f%f%f%f", &(profile->density),
	     profile->matrix[0] + 0, profile->matrix[0] + 1,
	     profile->matrix[0] + 2, profile->matrix[1] + 0,
	     profile->matrix[1] + 1, profile->matrix[1] + 2,
	     profile->matrix[2] + 0, profile->matrix[2] + 1,
	     profile->matrix[2] + 2);
    }
    else if (strcmp(keyword, "VariablePaperSize") == 0 &&
             strcmp((char *)string, "True") == 0)
    {
      ppd->variable_sizes = 1;

     /*
      * Add a "Custom" page size entry...
      */

      ppd_add_size(ppd, "Custom");

     /*
      * Add a "Custom" page size option...
      */

      if ((group = ppd_get_group(ppd, "General")) == NULL)
      {
        ppdClose(ppd);
	free(string);
	return (NULL);
      }

      if ((option = ppd_get_option(group, "PageSize")) == NULL)
      {
        ppdClose(ppd);
	free(string);
	return (NULL);
      }

      if ((choice = ppd_add_choice(option, "Custom")) == NULL)
      {
        ppdClose(ppd);
	free(string);
	return (NULL);
      }

      strcpy((char *)choice->text, "Custom Size");
      group  = NULL;
      option = NULL;
    }
    else if (strcmp(keyword, "MaxMediaWidth") == 0)
      ppd->custom_max[0] = atof((char *)string);
    else if (strcmp(keyword, "MaxMediaHeight") == 0)
      ppd->custom_max[1] = atof((char *)string);
    else if (strcmp(keyword, "ParamCustomPageSize") == 0)
    {
      if (strcmp(name, "Width") == 0)
        sscanf((char *)string, "%*s%*s%f%f", ppd->custom_min + 0,
	       ppd->custom_max + 0);
      else if (strcmp(name, "Height") == 0)
        sscanf((char *)string, "%*s%*s%f%f", ppd->custom_min + 1,
	       ppd->custom_max + 1);
    }
    else if (strcmp(keyword, "HWMargins") == 0)
      sscanf((char *)string, "%f%f%f%f", ppd->custom_margins + 0,
             ppd->custom_margins + 1, ppd->custom_margins + 2,
             ppd->custom_margins + 3);
    else if (strcmp(keyword, "CustomPageSize") == 0 &&
             strcmp(name, "True") == 0)
    {
      if ((option = ppdFindOption(ppd, "PageSize")) == NULL)
      {
        ppdClose(ppd);
	free(string);
	return (NULL);
      }

      if ((choice = ppdFindChoice(option, "Custom")) == NULL)
      {
        ppdClose(ppd);
	free(string);
	return (NULL);
      }

      choice->code = string;
      string = NULL;
      option = NULL;
    }
    else if (strcmp(keyword, "LandscapeOrientation") == 0)
    {
      if (strcmp((char *)string, "Minus90") == 0)
        ppd->landscape = -90;
      else
        ppd->landscape = 90;
    }
    else if (strcmp(keyword, "Emulators") == 0)
    {
      for (count = 1, sptr = (char *)string; sptr != NULL;)
        if ((sptr = strchr(sptr, ' ')) != NULL)
	{
	  count ++;
	  while (*sptr == ' ')
	    sptr ++;
	}

      ppd->num_emulations = count;
      ppd->emulations     = calloc(sizeof(ppd_emul_t), count);

      for (i = 0, sptr = (char *)string; i < count; i ++)
      {
        for (nameptr = ppd->emulations[i].name; *sptr != '\0' && *sptr != ' ';)
	  *nameptr ++ = *sptr ++;

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
        ppd->patches = realloc(ppd->patches, strlen((char *)ppd->patches) +
	                                     strlen((char *)string) + 1);

        strcpy((char *)ppd->patches + strlen((char *)ppd->patches),
	       (char *)string);
      }
    }
    else if (strcmp(keyword, "OpenUI") == 0)
    {
     /*
      * Add an option record to the current sub-group, group, or file...
      */

      if (name[0] == '*')
        strcpy(name, name + 1);

      if (string == NULL)
      {
        ppdClose(ppd);
	return (NULL);
      }

      if (subgroup != NULL)
        option = ppd_get_option(subgroup, name);
      else
      {
        if (group == NULL)
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
	    group = ppd_get_group(ppd, "Printer");
	  else
	    group = ppd_get_group(ppd, "General");

          if (group == NULL)
	  {
	    ppdClose(ppd);
	    free(string);
	    return (NULL);
	  }

          option = ppd_get_option(group, name);
	  group  = NULL;
	}
	else
          option = ppd_get_option(group, name);
	group  = NULL;
      }

      if (option == NULL)
      {
	ppdClose(ppd);
	free(string);
	return (NULL);
      }

     /*
      * Now fill in the initial information for the option...
      */

      if (strcmp((char *)string, "PickMany") == 0)
        option->ui = PPD_UI_PICKMANY;
      else if (strcmp((char *)string, "Boolean") == 0)
        option->ui = PPD_UI_BOOLEAN;
      else
        option->ui = PPD_UI_PICKONE;

      strcpy((char *)option->text, (char *)text);

      option->section = PPD_ORDER_ANY;
    }
    else if (strcmp(keyword, "JCLOpenUI") == 0)
    {
     /*
      * Find the JCL group, and add if needed...
      */

      if (group != NULL)
      {
        ppdClose(ppd);
	free(string);
	return (NULL);
      }

      group = ppd_get_group(ppd, "JCL");

      if (group == NULL)
      {
        ppdClose(ppd);
	free(string);
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
	free(string);
	return (NULL);
      }

     /*
      * Now fill in the initial information for the option...
      */

      if (strcmp((char *)string, "PickMany") == 0)
        option->ui = PPD_UI_PICKMANY;
      else if (strcmp((char *)string, "Boolean") == 0)
        option->ui = PPD_UI_BOOLEAN;
      else
        option->ui = PPD_UI_PICKONE;

      strcpy((char *)option->text, (char *)text);

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
	free(string);
	return (NULL);
      }

      if (strchr((char *)string, '/') != NULL)	/* Just show human readable text */
        strcpy((char *)string, strchr((char *)string, '/') + 1);

      group = ppd_get_group(ppd, (char *)string);
    }
    else if (strcmp(keyword, "CloseGroup") == 0)
      group = NULL;
    else if (strcmp(keyword, "OpenSubGroup") == 0)
    {
     /*
      * Open a new sub-group...
      */

      if (group == NULL || subgroup != NULL)
      {
        ppdClose(ppd);
	free(string);
	return (NULL);
      }

      if (group->num_subgroups == 0)
	subgroup = malloc(sizeof(ppd_group_t));
      else
	subgroup = realloc(group->subgroups,
	                   (group->num_subgroups + 1) * sizeof(ppd_group_t));

      if (subgroup == NULL)
      {
	ppdClose(ppd);
	free(string);
	return (NULL);
      }

      group->subgroups = subgroup;
      subgroup += group->num_subgroups;
      group->num_subgroups ++;

      memset(subgroup, 0, sizeof(ppd_group_t));
      strcpy((char *)subgroup->text, (char *)string);
    }
    else if (strcmp(keyword, "CloseSubGroup") == 0)
      subgroup = NULL;
    else if (strcmp(keyword, "OrderDependency") == 0 ||
             strcmp(keyword, "NonUIOrderDependency") == 0)
    {
      if (sscanf((char *)string, "%f%s%s", &order, name, keyword) != 3)
      {
        ppdClose(ppd);
	free(string);
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
       /*
        * Only valid for Non-UI options...
	*/

        for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
          if (group->text[0] == '\0')
	    break;

        if (i > 0)
          for (i = 0; i < group->num_options; i ++)
	    if (strcmp(keyword, group->options[i].keyword) == 0)
	    {
	      group->options[i].section = section;
	      group->options[i].order   = order;
	      break;
	    }

        group = NULL;
      }
      else
      {
        option->section = section;
	option->order   = order;
      }
    }
    else if (strncmp(keyword, "Default", 7) == 0)
    {
      if (strchr((char *)string, '/') != NULL)
        *strchr((char *)string, '/') = '\0';

      if (option == NULL)
      {
       /*
        * Only valid for Non-UI options...
	*/

        for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
          if (group->text[0] == '\0')
	    break;

        if (i > 0)
          for (i = 0; i < group->num_options; i ++)
	    if (strcmp(keyword, group->options[i].keyword) == 0)
	    {
	      strcpy(group->options[i].defchoice, (char *)string);
	      break;
	    }

        group = NULL;
      }
      else
        strcpy(option->defchoice, (char *)string);
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
	free(string);
	return (NULL);
      }

      ppd->consts = constraint;
      constraint += ppd->num_consts;
      ppd->num_consts ++;

      switch (sscanf((char *)string, "%s%s%s%s", constraint->option1,
                     constraint->choice1, constraint->option2,
		     constraint->choice2))
      {
        case 0 : /* Error */
	case 1 : /* Error */
	    ppdClose(ppd);
  	    free(string);
	    break;

	case 2 : /* Two options... */
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
	    if (constraint->option1[0] == '*')
	      strcpy(constraint->option1, constraint->option1 + 1);

	    if (constraint->choice1[0] == '*')
	    {
	      strcpy(constraint->option2, constraint->choice1 + 1);
              constraint->choice1[0] = '\0';
	    }
	    else if (constraint->option2[0] == '*')
  	      strcpy(constraint->option2, constraint->option2 + 1);

            constraint->choice2[0] = '\0';
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
      if ((size = ppdPageSize(ppd, name)) != NULL)
        sscanf((char *)string, "%f%f", &(size->width), &(size->length));
    }
    else if (strcmp(keyword, "ImageableArea") == 0)
    {
      if ((size = ppdPageSize(ppd, name)) != NULL)
	sscanf((char *)string, "%f%f%f%f", &(size->left), &(size->bottom),
	       &(size->right), &(size->top));
    }
    else if (option != NULL &&
             (mask & (PPD_KEYWORD | PPD_OPTION | PPD_STRING)) ==
	         (PPD_KEYWORD | PPD_OPTION | PPD_STRING))
    {
      if (strcmp(keyword, "PageSize") == 0)
      {
       /*
        * Add a page size...
	*/

	ppd_add_size(ppd, name);
      }

     /*
      * Add the option choice...
      */

      choice = ppd_add_choice(option, name);

      if (mask & PPD_TEXT)
        strcpy((char *)choice->text, (char *)text);
      else if (strcmp(name, "True") == 0)
        strcpy((char *)choice->text, "Yes");
      else if (strcmp(name, "False") == 0)
        strcpy((char *)choice->text, "No");
      else
        strcpy((char *)choice->text, name);

      if (strncmp(keyword, "JCL", 3) == 0)
        ppd_decode(string);		/* Decode quoted string */

      choice->code = string;
      string = NULL;			/* Don't free this string below */
    }

    if (string != NULL)
      free(string);
  }

#ifdef DEBUG
  if (!feof(fp))
    printf("Premature EOF at %d...\n", ftell(fp));
#endif /* DEBUG */

 /*
  * Set the option back-pointer for each choice...
  */

  for (i = ppd->num_groups, group = ppd->groups;
       i > 0;
       i --, group ++)
  {
    for (j = group->num_options, option = group->options;
         j > 0;
	 j --, option ++)
      for (k = 0; k < option->num_choices; k ++)
        option->choices[k].option = (void *)option;

    for (j = group->num_subgroups, subgroup = group->subgroups;
         j > 0;
	 j --, subgroup ++)
      for (k = group->num_options, option = group->options;
           k > 0;
	   k --, option ++)
        for (m = 0; m < option->num_choices; m ++)
          option->choices[m].option = (void *)option;
  }

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

    free(fp);
  }
  else
    ppd = NULL;

  return (ppd);
}


/*
 * 'ppdOpenFile()' - Read a PPD file into memory.
 */

ppd_file_t *			/* O - PPD file record */
ppdOpenFile(char *filename)	/* I - File to read from */
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
 * 'ppd_read()' - Read a line from a PPD file, skipping comment lines as
 *                necessary.
 */

static int			/* O - Bitmask of fields read */
ppd_read(FILE          *fp,	/* I - File to read from */
         char          *keyword,/* O - Keyword from line */
	 char          *option,	/* O - Option from line */
         unsigned char *text,	/* O - Human-readable text from line */
	 unsigned char **string)/* O - Code/string data */
{
  int		ch,		/* Character from file */
		endquote,	/* Waiting for an end quote */
		mask;		/* Mask to be returned */
  char		*keyptr,	/* Keyword pointer */
		*optptr;	/* Option pointer */
  unsigned char	*textptr,	/* Text pointer */
		*strptr;	/* Pointer into string */
  unsigned char	*lineptr,	/* Current position in line buffer */
		line[262144];	/* Line buffer (256k) */


 /*
  * Range check everything...
  */

  if (fp == NULL || keyword == NULL || option == NULL || text == NULL ||
      string == NULL)
    return (0);

 /*
  * Now loop until we have a valid line...
  */

  do
  {
   /*
    * Read the line...
    */

    lineptr  = line;
    endquote = 0;

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

	*lineptr++ = '\n';

	if (!endquote)			/* Continue for multi-line text */
          break;
      }
      else
      {
       /*
	* Any other character...
	*/

	*lineptr++ = ch;

	if (ch == '\"')
          endquote = !endquote;
      }
    }

    if (lineptr > line && lineptr[-1] == '\n')
      lineptr --;

    *lineptr = '\0';

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

    if (strncmp((char *)line, "*%", 2) == 0 ||	/* Comment line */
        strncmp((char *)line, "*?", 2) == 0 ||	/* Query line */
        strcmp((char *)line, "*End") == 0)	/* End of multi-line string */
      continue;

   /*
    * Get a keyword...
    */

    keyptr = keyword;

    while (*lineptr != '\0' && *lineptr != ':' && !isspace(*lineptr) &&
	   (keyptr - keyword) < 40)
      *keyptr++ = *lineptr++;

    *keyptr = '\0';
    mask |= PPD_KEYWORD;

    if (*lineptr == ' ' || *lineptr == '\t')
    {
     /*
      * Get an option name...
      */

      while (*lineptr == ' ' || *lineptr == '\t')
        lineptr ++;

      optptr = option;

      while (*lineptr != '\0' && *lineptr != '\n' && *lineptr != ':' &&
             *lineptr != '/' && (optptr - option) < 40)
	*optptr++ = *lineptr++;

      *optptr = '\0';
      mask |= PPD_OPTION;

      if (*lineptr == '/')
      {
       /*
        * Get human-readable text...
	*/

        lineptr ++;
	
	textptr = text;

	while (*lineptr != '\0' && *lineptr != '\n' && *lineptr != ':' &&
               (textptr - text) < 80)
	  *textptr++ = *lineptr++;

	*textptr = '\0';
	ppd_decode(text);

	mask |= PPD_TEXT;
      }
    }

    if (*lineptr == ':')
    {
     /*
      * Get string...
      */

      *string = malloc(strlen((char *)lineptr) + 1);

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
ppd_decode(unsigned char *string)	/* I - String to decode */
{
  unsigned char	*inptr,			/* Input pointer */
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
 * End of "$Id: ppd.c,v 1.15 1999/04/16 16:57:49 mike Exp $".
 */
