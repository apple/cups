/*
 * "$Id$"
 *
 *   Option marking routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
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
 *   cupsMarkOptions()     - Mark command-line options in a PPD file.
 *   ppdConflicts()        - Check to see if there are any conflicts among the
 *                           marked option choices.
 *   ppdFindChoice()       - Return a pointer to an option choice.
 *   ppdFindMarkedChoice() - Return the marked choice for the specified option.
 *   ppdFindOption()       - Return a pointer to the specified option.
 *   ppdIsMarked()         - Check to see if an option is marked.
 *   ppdMarkDefaults()     - Mark all default options in the PPD file.
 *   ppdMarkOption()       - Mark an option in a PPD file.
 *   ppdFirstOption()      - Return the first option in the PPD file.
 *   ppdNextOption()       - Return the next option in the PPD file.
 *   debug_marked()        - Output the marked array to stdout...
 *   ppd_defaults()        - Set the defaults for this group and all sub-groups.
 *   ppd_mark_choices()    - Mark one or more option choices from a string.
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include "string.h"
#include "debug.h"


/*
 * Local functions...
 */

#ifdef DEBUG
static void	debug_marked(ppd_file_t *ppd, const char *title);
#else
#  define debug_marked(ppd,title)
#endif /* DEBUG */
static void	ppd_defaults(ppd_file_t *ppd, ppd_group_t *g);
static int	ppd_mark_choices(ppd_file_t *ppd, const char *options);


/*
 * 'cupsMarkOptions()' - Mark command-line options in a PPD file.
 *
 * This function maps the IPP "finishings", "media", "mirror",
 * "multiple-document-handling", "output-bin", "printer-resolution", and
 * "sides" attributes to their corresponding PPD options and choices.
 */

int					/* O - 1 if conflicting */
cupsMarkOptions(
    ppd_file_t    *ppd,			/* I - PPD file */
    int           num_options,		/* I - Number of options */
    cups_option_t *options)		/* I - Options */
{
  int		i, j, k;		/* Looping vars */
  int		conflict;		/* Option conflicts */
  char		*val,			/* Pointer into value */
		*ptr,			/* Pointer into string */
		s[255];			/* Temporary string */
  const char	*page_size;		/* PageSize option */
  cups_option_t	*optptr;		/* Current option */
  ppd_option_t	*option;		/* PPD option */
  ppd_attr_t	*attr;			/* PPD attribute */
  static const char * const duplex_options[] =
		{			/* Duplex option names */
		  "Duplex",		/* Adobe */
		  "EFDuplex",		/* EFI */
		  "EFDuplexing",	/* EFI */
		  "KD03Duplex",		/* Kodak */
		  "JCLDuplex"		/* Samsung */
		};
  static const char * const duplex_one[] =
		{			/* one-sided names */
		  "None",
		  "False"
		};
  static const char * const duplex_two_long[] =
		{			/* two-sided-long-edge names */
		  "DuplexNoTumble",	/* Adobe */
		  "LongEdge",		/* EFI */
		  "Top"			/* EFI */
		};
  static const char * const duplex_two_short[] =
		{			/* two-sided-long-edge names */
		  "DuplexTumble",	/* Adobe */
		  "ShortEdge",		/* EFI */
		  "Bottom"		/* EFI */
		};


 /*
  * Check arguments...
  */

  if (!ppd || num_options <= 0 || !options)
    return (0);

  debug_marked(ppd, "Before...");

 /*
  * Mark options...
  */

  conflict  = 0;

  for (i = num_options, optptr = options; i > 0; i --, optptr ++)
    if (!strcasecmp(optptr->name, "media"))
    {
     /*
      * Loop through the option string, separating it at commas and
      * marking each individual option as long as the corresponding
      * PPD option (PageSize, InputSlot, etc.) is not also set.
      *
      * For PageSize, we also check for an empty option value since
      * some versions of MacOS X use it to specify auto-selection
      * of the media based solely on the size.
      */

      page_size = cupsGetOption("PageSize", num_options, options);

      for (val = optptr->value; *val;)
      {
       /*
        * Extract the sub-option from the string...
	*/

        for (ptr = s; *val && *val != ',' && (ptr - s) < (sizeof(s) - 1);)
	  *ptr++ = *val++;
	*ptr++ = '\0';

	if (*val == ',')
	  val ++;

       /*
        * Mark it...
	*/

        if (!page_size || !page_size[0])
	  if (ppdMarkOption(ppd, "PageSize", s))
            conflict = 1;

        if (cupsGetOption("InputSlot", num_options, options) == NULL)
	  if (ppdMarkOption(ppd, "InputSlot", s))
            conflict = 1;

        if (cupsGetOption("MediaType", num_options, options) == NULL)
	  if (ppdMarkOption(ppd, "MediaType", s))
            conflict = 1;

        if (cupsGetOption("EFMediaType", num_options, options) == NULL)
	  if (ppdMarkOption(ppd, "EFMediaType", s))		/* EFI */
            conflict = 1;

        if (cupsGetOption("EFMediaQualityMode", num_options, options) == NULL)
	  if (ppdMarkOption(ppd, "EFMediaQualityMode", s))	/* EFI */
            conflict = 1;

	if (strcasecmp(s, "manual") == 0 &&
	    cupsGetOption("ManualFeed", num_options, options) == NULL)
          if (ppdMarkOption(ppd, "ManualFeed", "True"))
	    conflict = 1;
      }
    }
    else if (!strcasecmp(optptr->name, "sides"))
    {
      for (j = 0; j < (int)(sizeof(duplex_options) / sizeof(duplex_options[0])); j ++)
        if (cupsGetOption(duplex_options[j], num_options, options) != NULL)
	  break;

      if (j < (int)(sizeof(duplex_options) / sizeof(duplex_options[0])))
      {
       /*
        * Don't override the PPD option with the IPP attribute...
	*/

        continue;
      }

      if (!strcasecmp(optptr->value, "one-sided"))
      {
       /*
        * Mark the appropriate duplex option for one-sided output...
	*/

        for (j = 0; j < (int)(sizeof(duplex_options) / sizeof(duplex_options[0])); j ++)
	  if ((option = ppdFindOption(ppd, duplex_options[j])) != NULL)
	    break;

	if (j < (int)(sizeof(duplex_options) / sizeof(duplex_options[0])))
	{
          for (k = 0; k < (int)(sizeof(duplex_one) / sizeof(duplex_one[0])); k ++)
            if (ppdFindChoice(option, duplex_one[k]))
	    {
	      if (ppdMarkOption(ppd, duplex_options[j], duplex_one[k]))
		conflict = 1;

	      break;
            }
        }
      }
      else if (!strcasecmp(optptr->value, "two-sided-long-edge"))
      {
       /*
        * Mark the appropriate duplex option for two-sided-long-edge output...
	*/

        for (j = 0; j < (int)(sizeof(duplex_options) / sizeof(duplex_options[0])); j ++)
	  if ((option = ppdFindOption(ppd, duplex_options[j])) != NULL)
	    break;

	if (j < (int)(sizeof(duplex_options) / sizeof(duplex_options[0])))
	{
          for (k = 0; k < (int)(sizeof(duplex_two_long) / sizeof(duplex_two_long[0])); k ++)
            if (ppdFindChoice(option, duplex_two_long[k]))
	    {
	      if (ppdMarkOption(ppd, duplex_options[j], duplex_two_long[k]))
		conflict = 1;

	      break;
            }
        }
      }
      else if (!strcasecmp(optptr->value, "two-sided-short-edge"))
      {
       /*
        * Mark the appropriate duplex option for two-sided-short-edge output...
	*/

        for (j = 0; j < (int)(sizeof(duplex_options) / sizeof(duplex_options[0])); j ++)
	  if ((option = ppdFindOption(ppd, duplex_options[j])) != NULL)
	    break;

	if (j < (int)(sizeof(duplex_options) / sizeof(duplex_options[0])))
	{
          for (k = 0; k < (int)(sizeof(duplex_two_short) / sizeof(duplex_two_short[0])); k ++)
            if (ppdFindChoice(option, duplex_two_short[k]))
	    {
	      if (ppdMarkOption(ppd, duplex_options[j], duplex_two_short[k]))
		conflict = 1;

	      break;
            }
        }
      }
    }
    else if (!strcasecmp(optptr->name, "resolution") ||
             !strcasecmp(optptr->name, "printer-resolution"))
    {
      if (ppdMarkOption(ppd, "Resolution", optptr->value))
        conflict = 1;
      if (ppdMarkOption(ppd, "SetResolution", optptr->value))
      	/* Calcomp, Linotype, QMS, Summagraphics, Tektronix, Varityper */
        conflict = 1;
      if (ppdMarkOption(ppd, "JCLResolution", optptr->value))	/* HP */
        conflict = 1;
      if (ppdMarkOption(ppd, "CNRes_PGP", optptr->value))	/* Canon */
        conflict = 1;
    }
    else if (!strcasecmp(optptr->name, "output-bin"))
    {
      if (!cupsGetOption("OutputBin", num_options, options))
        if (ppdMarkOption(ppd, "OutputBin", optptr->value))
          conflict = 1;
    }
    else if (!strcasecmp(optptr->name, "multiple-document-handling"))
    {
      if (!cupsGetOption("Collate", num_options, options) &&
          ppdFindOption(ppd, "Collate"))
      {
        if (strcasecmp(optptr->value, "separate-documents-uncollated-copies"))
	{
	  if (ppdMarkOption(ppd, "Collate", "True"))
            conflict = 1;
        }
	else
	{
	  if (ppdMarkOption(ppd, "Collate", "False"))
            conflict = 1;
        }
      }
    }
    else if (!strcasecmp(optptr->name, "finishings"))
    {
     /*
      * Lookup cupsIPPFinishings attributes for each value...
      */

      for (ptr = optptr->value; *ptr;)
      {
       /*
        * Get the next finishings number...
	*/

        if (!isdigit(*ptr & 255))
	  break;

        if ((j = strtol(ptr, &ptr, 10)) < 3)
	  break;

       /*
        * Skip separator as needed...
	*/

        if (*ptr == ',')
	  ptr ++;

       /*
        * Look it up in the PPD file...
	*/

	sprintf(s, "%d", j);

        if ((attr = ppdFindAttr(ppd, "cupsIPPFinishings", s)) == NULL)
	  continue;

       /*
        * Apply "*Option Choice" settings from the attribute value...
	*/

        if (ppd_mark_choices(ppd, attr->value))
	  conflict = 1;
      }
    }
    else if (!strcasecmp(optptr->name, "mirror"))
    {
      if (ppdMarkOption(ppd, "MirrorPrint", optptr->value))
	conflict = 1;
    }
    else if (ppdMarkOption(ppd, optptr->name, optptr->value))
      conflict = 1;

  debug_marked(ppd, "After...");

  return (conflict);
}


/*
 * 'ppdConflicts()' - Check to see if there are any conflicts among the
 *                    marked option choices.
 *
 * The returned value is the same as returned by @link ppdMarkOption@.
 */

int					/* O - Number of conflicts found */
ppdConflicts(ppd_file_t *ppd)		/* I - PPD to check */
{
  int		i,			/* Looping variable */
		conflicts;		/* Number of conflicts */
  ppd_const_t	*c;			/* Current constraint */
  ppd_option_t	*o1, *o2;		/* Options */
  ppd_choice_t	*c1, *c2;		/* Choices */
  ppd_choice_t	key;			/* Search key */


  if (!ppd)
    return (0);

 /*
  * Clear all conflicts...
  */

  conflicts = 0;

  for (o1 = ppdFirstOption(ppd); o1; o1 = ppdNextOption(ppd))
    o1->conflicted = 0;

  cupsArraySave(ppd->marked);

 /*
  * Loop through all of the UI constraints and flag any options
  * that conflict...
  */

  for (i = ppd->num_consts, c = ppd->consts, o1 = o2 = NULL, c1 = c2 = NULL;
       i > 0;
       i --, c ++)
  {
   /*
    * Grab pointers to the first option...
    */

    if (!o1 || strcmp(c->option1, o1->keyword))
    {
      o1 = ppdFindOption(ppd, c->option1);
      c1 = NULL;
    }

    if (!o1)
      continue;
    else if (c->choice1[0] && (!c1 || strcmp(c->choice1, c1->choice)))
    {
     /*
      * This constraint maps to a specific choice.
      */

      key.option = o1;

      if ((c1 = (ppd_choice_t *)cupsArrayFind(ppd->marked, &key)) != NULL &&
          (!c1->marked || strcmp(c->choice1, c1->choice)))
        c1 = NULL;
    }
    else if (!c1)
    {
     /*
      * This constraint applies to any choice for this option.
      */

      key.option = o1;

      if ((c1 = (ppd_choice_t *)cupsArrayFind(ppd->marked, &key)) != NULL &&
          (!strcasecmp(c1->choice, "None") || !strcasecmp(c1->choice, "Off") ||
           !strcasecmp(c1->choice, "False")))
        c1 = NULL;
    }

   /*
    * Grab pointers to the second option...
    */

    if (!o2 || strcmp(c->option2, o2->keyword))
    {
      o2 = ppdFindOption(ppd, c->option2);
      c2 = NULL;
    }

    if (!o2)
      continue;
    else if (c->choice2[0] && (!c2 || strcmp(c->choice2, c2->choice)))
    {
     /*
      * This constraint maps to a specific choice.
      */

      key.option = o2;

      if ((c2 = (ppd_choice_t *)cupsArrayFind(ppd->marked, &key)) != NULL &&
          (!c2->marked || strcmp(c->choice2, c2->choice)))
        c2 = NULL;
    }
    else if (!c2)
    {
     /*
      * This constraint applies to any choice for this option.
      */

      key.option = o2;

      if ((c2 = (ppd_choice_t *)cupsArrayFind(ppd->marked, &key)) != NULL &&
          (!strcasecmp(c2->choice, "None") || !strcasecmp(c2->choice, "Off") ||
           !strcasecmp(c2->choice, "False")))
        c2 = NULL;
    }

   /*
    * If both options are marked then there is a conflict...
    */

    if (c1 && c1->marked && c2 && c2->marked)
    {
      DEBUG_printf(("%s->%s conflicts with %s->%s (%s %s %s %s)\n",
                    o1->keyword, c1->choice, o2->keyword, c2->choice,
		    c->option1, c->choice1, c->option2, c->choice2));
      conflicts ++;
      o1->conflicted = 1;
      o2->conflicted = 1;
    }
  }

  cupsArrayRestore(ppd->marked);

 /*
  * Return the number of conflicts found...
  */

  return (conflicts);
}


/*
 * 'ppdFindChoice()' - Return a pointer to an option choice.
 */

ppd_choice_t *				/* O - Choice pointer or @code NULL@ */
ppdFindChoice(ppd_option_t *o,		/* I - Pointer to option */
              const char   *choice)	/* I - Name of choice */
{
  int		i;			/* Looping var */
  ppd_choice_t	*c;			/* Current choice */


  if (o == NULL || choice == NULL)
    return (NULL);

  for (i = o->num_choices, c = o->choices; i > 0; i --, c ++)
    if (strcasecmp(c->choice, choice) == 0)
      return (c);

  return (NULL);
}


/*
 * 'ppdFindMarkedChoice()' - Return the marked choice for the specified option.
 */

ppd_choice_t *				/* O - Pointer to choice or @code NULL@ */
ppdFindMarkedChoice(ppd_file_t *ppd,	/* I - PPD file */
                    const char *option)	/* I - Keyword/option name */
{
  ppd_choice_t	key;			/* Search key for choice */


  if ((key.option = ppdFindOption(ppd, option)) == NULL)
    return (NULL);

  return ((ppd_choice_t *)cupsArrayFind(ppd->marked, &key));
}


/*
 * 'ppdFindOption()' - Return a pointer to the specified option.
 */

ppd_option_t *				/* O - Pointer to option or @code NULL@ */
ppdFindOption(ppd_file_t *ppd,		/* I - PPD file data */
              const char *option)	/* I - Option/Keyword name */
{
 /*
  * Range check input...
  */

  if (!ppd || !option)
    return (NULL);

  if (ppd->options)
  {
   /*
    * Search in the array...
    */

    ppd_option_t	key;		/* Option search key */


    strlcpy(key.keyword, option, sizeof(key.keyword));

    return ((ppd_option_t *)cupsArrayFind(ppd->options, &key));
  }
  else
  {
   /*
    * Search in each group...
    */

    int			i, j;		/* Looping vars */
    ppd_group_t		*group;		/* Current group */
    ppd_option_t	*optptr;	/* Current option */


    for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
      for (j = group->num_options, optptr = group->options;
           j > 0;
	   j --, optptr ++)
        if (!strcasecmp(optptr->keyword, option))
	  return (optptr);

    return (NULL);
  }
}


/*
 * 'ppdIsMarked()' - Check to see if an option is marked.
 */

int					/* O - Non-zero if option is marked */
ppdIsMarked(ppd_file_t *ppd,		/* I - PPD file data */
            const char *option,		/* I - Option/Keyword name */
            const char *choice)		/* I - Choice name */
{
  ppd_choice_t	key,			/* Search key */
		*c;			/* Choice pointer */


  if (!ppd)
    return (0);

  if ((key.option = ppdFindOption(ppd, option)) == NULL)
    return (0);

  if ((c = (ppd_choice_t *)cupsArrayFind(ppd->marked, &key)) == NULL)
    return (0);

  return (!strcmp(c->choice, choice));
}


/*
 * 'ppdMarkDefaults()' - Mark all default options in the PPD file.
 */

void
ppdMarkDefaults(ppd_file_t *ppd)	/* I - PPD file record */
{
  int		i;			/* Looping variables */
  ppd_group_t	*g;			/* Current group */
  ppd_choice_t	*c;			/* Current choice */


  if (!ppd)
    return;

 /*
  * Clean out the marked array...
  */

  for (c = (ppd_choice_t *)cupsArrayFirst(ppd->marked);
       c;
       c = (ppd_choice_t *)cupsArrayNext(ppd->marked))
    cupsArrayRemove(ppd->marked, c);

 /*
  * Then repopulate it with the defaults...
  */

  for (i = ppd->num_groups, g = ppd->groups; i > 0; i --, g ++)
    ppd_defaults(ppd, g);
}


/*
 * 'ppdMarkOption()' - Mark an option in a PPD file.
 */

int					/* O - Number of conflicts */
ppdMarkOption(ppd_file_t *ppd,		/* I - PPD file record */
              const char *option,	/* I - Keyword */
              const char *choice)	/* I - Option name */
{
  int		i, j;			/* Looping vars */
  ppd_option_t	*o;			/* Option pointer */
  ppd_choice_t	*c,			/* Choice pointer */
		*oldc,			/* Old choice pointer */
		key;			/* Search key for choice */
  struct lconv	*loc;			/* Locale data */


  DEBUG_printf(("ppdMarkOption(ppd=%p, option=\"%s\", choice=\"%s\")\n",
        	ppd, option, choice));

 /*
  * Range check input...
  */

  if (!ppd || !option || !choice)
    return (0);

 /*
  * AP_D_InputSlot is the "default input slot" on MacOS X, and setting
  * it clears the regular InputSlot choices...
  */

  if (!strcasecmp(option, "AP_D_InputSlot"))
  {
    if ((o = ppdFindOption(ppd, "InputSlot")) != NULL)
    {
      key.option = o;
      if ((oldc = (ppd_choice_t *)cupsArrayFind(ppd->marked, &key)) != NULL)
      {
        oldc->marked = 0;
        cupsArrayRemove(ppd->marked, oldc);
      }
    }
  }

 /*
  * Check for custom options...
  */

  if ((o = ppdFindOption(ppd, option)) == NULL)
    return (0);

  loc = localeconv();

  if (!strncasecmp(choice, "Custom.", 7))
  {
   /*
    * Handle a custom option...
    */

    if ((c = ppdFindChoice(o, "Custom")) == NULL)
      return (0);

    if (!strcasecmp(option, "PageSize"))
    {
     /*
      * Handle custom page sizes...
      */

      ppdPageSize(ppd, choice);
    }
    else
    {
     /*
      * Handle other custom options...
      */

      ppd_coption_t	*coption;	/* Custom option */
      ppd_cparam_t	*cparam;	/* Custom parameter */
      char		*units;		/* Custom points units */


      if ((coption = ppdFindCustomOption(ppd, option)) != NULL)
      {
        if ((cparam = (ppd_cparam_t *)cupsArrayFirst(coption->params)) == NULL)
	  return (0);

        switch (cparam->type)
	{
	  case PPD_CUSTOM_CURVE :
	  case PPD_CUSTOM_INVCURVE :
	  case PPD_CUSTOM_REAL :
	      cparam->current.custom_real = (float)_cupsStrScand(choice + 7,
	                                                         NULL, loc);
	      break;

	  case PPD_CUSTOM_POINTS :
	      cparam->current.custom_points = (float)_cupsStrScand(choice + 7,
	                                                           &units,
	                                                           loc);

              if (units)
	      {
        	if (!strcasecmp(units, "cm"))
	          cparam->current.custom_points *= 72.0f / 2.54f;	      
        	else if (!strcasecmp(units, "mm"))
	          cparam->current.custom_points *= 72.0f / 25.4f;	      
        	else if (!strcasecmp(units, "m"))
	          cparam->current.custom_points *= 72.0f / 0.0254f;	      
        	else if (!strcasecmp(units, "in"))
	          cparam->current.custom_points *= 72.0f;	      
        	else if (!strcasecmp(units, "ft"))
	          cparam->current.custom_points *= 12.0f * 72.0f;	      
              }
	      break;

	  case PPD_CUSTOM_INT :
	      cparam->current.custom_int = atoi(choice + 7);
	      break;

	  case PPD_CUSTOM_PASSCODE :
	  case PPD_CUSTOM_PASSWORD :
	  case PPD_CUSTOM_STRING :
	      if (cparam->current.custom_string)
	        _cupsStrFree(cparam->current.custom_string);

	      cparam->current.custom_string = _cupsStrAlloc(choice + 7);
	      break;
	}
      }
    }

   /*
    * Make sure that we keep the option marked below...
    */

    choice = "Custom";
  }
  else if (choice[0] == '{')
  {
   /*
    * Handle multi-value custom options...
    */

    ppd_coption_t	*coption;	/* Custom option */
    ppd_cparam_t	*cparam;	/* Custom parameter */
    char		*units;		/* Custom points units */
    int			num_vals;	/* Number of values */
    cups_option_t	*vals,		/* Values */
			*val;		/* Value */


    if ((c = ppdFindChoice(o, "Custom")) == NULL)
      return (0);

    if ((coption = ppdFindCustomOption(ppd, option)) != NULL)
    {
      num_vals = cupsParseOptions(choice + 1, 0, &vals);

      for (i = 0, val = vals; i < num_vals; i ++, val ++)
      {
        if ((cparam = ppdFindCustomParam(coption, val->name)) == NULL)
	  continue;

	switch (cparam->type)
	{
	  case PPD_CUSTOM_CURVE :
	  case PPD_CUSTOM_INVCURVE :
	  case PPD_CUSTOM_REAL :
	      cparam->current.custom_real = (float)_cupsStrScand(val->value,
	                                                         NULL, loc);
	      break;

	  case PPD_CUSTOM_POINTS :
	      cparam->current.custom_points = (float)_cupsStrScand(val->value,
	                                                           &units,
	                                                           loc);

	      if (units)
	      {
        	if (!strcasecmp(units, "cm"))
		  cparam->current.custom_points *= 72.0f / 2.54f;
        	else if (!strcasecmp(units, "mm"))
		  cparam->current.custom_points *= 72.0f / 25.4f;
        	else if (!strcasecmp(units, "m"))
		  cparam->current.custom_points *= 72.0f / 0.0254f;
        	else if (!strcasecmp(units, "in"))
		  cparam->current.custom_points *= 72.0f;
        	else if (!strcasecmp(units, "ft"))
		  cparam->current.custom_points *= 12.0f * 72.0f;
	      }
	      break;

	  case PPD_CUSTOM_INT :
	      cparam->current.custom_int = atoi(val->value);
	      break;

	  case PPD_CUSTOM_PASSCODE :
	  case PPD_CUSTOM_PASSWORD :
	  case PPD_CUSTOM_STRING :
	      if (cparam->current.custom_string)
		_cupsStrFree(cparam->current.custom_string);

	      cparam->current.custom_string = _cupsStrAlloc(val->value);
	      break;
	}
      }

      cupsFreeOptions(num_vals, vals);
    }
  }
  else
  {
    for (i = o->num_choices, c = o->choices; i > 0; i --, c ++)
      if (!strcasecmp(c->choice, choice))
        break;

    if (!i)
      return (0);
  }

 /*
  * Option found; mark it and then handle unmarking any other options.
  */

  if (o->ui != PPD_UI_PICKMANY)
  {
   /*
    * Unmark all other choices...
    */

    if ((oldc = (ppd_choice_t *)cupsArrayFind(ppd->marked, c)) != NULL)
    {
      oldc->marked = 0;
      cupsArrayRemove(ppd->marked, oldc);
    }

    if (!strcasecmp(option, "PageSize") || !strcasecmp(option, "PageRegion"))
    {
     /*
      * Mark current page size...
      */

      for (j = 0; j < ppd->num_sizes; j ++)
	ppd->sizes[j].marked = !strcasecmp(ppd->sizes[j].name,
		                           choice);

     /*
      * Unmark the current PageSize or PageRegion setting, as
      * appropriate...
      */

      if (!strcasecmp(option, "PageSize"))
      {
	if ((o = ppdFindOption(ppd, "PageRegion")) != NULL)
        {
          key.option = o;
          if ((oldc = (ppd_choice_t *)cupsArrayFind(ppd->marked, &key)) != NULL)
          {
            oldc->marked = 0;
            cupsArrayRemove(ppd->marked, oldc);
          }
        }
      }
      else
      {
	if ((o = ppdFindOption(ppd, "PageSize")) != NULL)
        {
          key.option = o;
          if ((oldc = (ppd_choice_t *)cupsArrayFind(ppd->marked, &key)) != NULL)
          {
            oldc->marked = 0;
            cupsArrayRemove(ppd->marked, oldc);
          }
        }
      }
    }
    else if (!strcasecmp(option, "InputSlot"))
    {
     /*
      * Unmark ManualFeed option...
      */

      if ((o = ppdFindOption(ppd, "ManualFeed")) != NULL)
      {
        key.option = o;
        if ((oldc = (ppd_choice_t *)cupsArrayFind(ppd->marked, &key)) != NULL)
        {
          oldc->marked = 0;
          cupsArrayRemove(ppd->marked, oldc);
        }
      }
    }
    else if (!strcasecmp(option, "ManualFeed") &&
	     !strcasecmp(choice, "True"))
    {
     /*
      * Unmark InputSlot option...
      */

      if ((o = ppdFindOption(ppd, "InputSlot")) != NULL)
      {
        key.option = o;
        if ((oldc = (ppd_choice_t *)cupsArrayFind(ppd->marked, &key)) != NULL)
        {
          oldc->marked = 0;
          cupsArrayRemove(ppd->marked, oldc);
        }
      }
    }
  }

  c->marked = 1;

  cupsArrayAdd(ppd->marked, c);

 /*
  * Return the number of conflicts...
  */

  return (ppdConflicts(ppd));
}


/*
 * 'ppdFirstOption()' - Return the first option in the PPD file.
 *
 * Options are returned from all groups in ascending alphanumeric order.
 *
 * @since CUPS 1.2@
 */

ppd_option_t *				/* O - First option or @code NULL@ */
ppdFirstOption(ppd_file_t *ppd)		/* I - PPD file */
{
  if (!ppd)
    return (NULL);
  else
    return ((ppd_option_t *)cupsArrayFirst(ppd->options));
}


/*
 * 'ppdNextOption()' - Return the next option in the PPD file.
 *
 * Options are returned from all groups in ascending alphanumeric order.
 *
 * @since CUPS 1.2@
 */

ppd_option_t *				/* O - Next option or @code NULL@ */
ppdNextOption(ppd_file_t *ppd)		/* I - PPD file */
{
  if (!ppd)
    return (NULL);
  else
    return ((ppd_option_t *)cupsArrayNext(ppd->options));
}


#ifdef DEBUG
/*
 * 'debug_marked()' - Output the marked array to stdout...
 */

static void
debug_marked(ppd_file_t *ppd,		/* I - PPD file data */
             const char *title)		/* I - Title for list */
{
  ppd_choice_t	*c;			/* Current choice */


  DEBUG_printf(("cupsMarkOptions: %s\n", title));

  for (c = (ppd_choice_t *)cupsArrayFirst(ppd->marked);
       c;
       c = (ppd_choice_t *)cupsArrayNext(ppd->marked))
    DEBUG_printf(("cupsMarkOptions: %s=%s\n", c->option->keyword, c->choice));
}
#endif /* DEBUG */


/*
 * 'ppd_defaults()' - Set the defaults for this group and all sub-groups.
 */

static void
ppd_defaults(ppd_file_t  *ppd,		/* I - PPD file */
             ppd_group_t *g)		/* I - Group to default */
{
  int		i;			/* Looping var */
  ppd_option_t	*o;			/* Current option */
  ppd_group_t	*sg;			/* Current sub-group */


  for (i = g->num_options, o = g->options; i > 0; i --, o ++)
    if (strcasecmp(o->keyword, "PageRegion") != 0)
      ppdMarkOption(ppd, o->keyword, o->defchoice);

  for (i = g->num_subgroups, sg = g->subgroups; i > 0; i --, sg ++)
    ppd_defaults(ppd, sg);
}


/*
 * 'ppd_mark_choices()' - Mark one or more option choices from a string.
 */

static int				/* O - 1 if there are conflicts, 0 otherwise */
ppd_mark_choices(ppd_file_t *ppd,	/* I - PPD file */
                 const char *options)	/* I - "*Option Choice ..." string */
{
  char	option[PPD_MAX_NAME],		/* Current option */
	choice[PPD_MAX_NAME],		/* Current choice */
	*ptr;				/* Pointer into option or choice */
  int	conflict = 0;			/* Do we have a conflict? */


  if (!options)
    return (0);

 /*
  * Read all of the "*Option Choice" pairs from the string, marking PPD
  * options as we go...
  */

  while (*options)
  {
   /*
    * Skip leading whitespace...
    */

    while (isspace(*options & 255))
      options ++;

    if (*options != '*')
      break;

   /*
    * Get the option name...
    */

    options ++;
    ptr = option;
    while (*options && !isspace(*options & 255) &&
	       ptr < (option + sizeof(option) - 1))
      *ptr++ = *options++;

    if (ptr == option)
      break;

    *ptr = '\0';

   /*
    * Get the choice...
    */

    while (isspace(*options & 255))
      options ++;

    if (!*options)
      break;

    ptr = choice;
    while (*options && !isspace(*options & 255) &&
	       ptr < (choice + sizeof(choice) - 1))
      *ptr++ = *options++;

    *ptr = '\0';

   /*
    * Mark the option...
    */

    if (ppdMarkOption(ppd, option, choice))
      conflict = 1;
  }

 /*
  * Return whether we had any conflicts...
  */

  return (conflict);
}


/*
 * End of "$Id$".
 */
