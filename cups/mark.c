/*
 * "$Id$"
 *
 *   Option marking routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   PostScript is a trademark of Adobe Systems, Inc.
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   ppdConflicts()        - Check to see if there are any conflicts.
 *   ppdFindChoice()       - Return a pointer to an option choice.
 *   ppdFindMarkedChoice() - Return the marked choice for the specified option.
 *   ppdFindOption()       - Return a pointer to the specified option.
 *   ppdFirstOption()      - Return the first option in the PPD file.
 *   ppdNextOption()       - Return the next option in the PPD file.
 *   ppdIsMarked()         - Check to see if an option is marked...
 *   ppdMarkDefaults()     - Mark all default options in the PPD file.
 *   ppdMarkOption()       - Mark an option in a PPD file.
 *   ppd_defaults()        - Set the defaults for this group and all sub-groups.
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

static void	ppd_defaults(ppd_file_t *ppd, ppd_group_t *g);


/*
 * 'ppdConflicts()' - Check to see if there are any conflicts.
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
          !c1->marked)
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
          !c2->marked)
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

 /*
  * Return the number of conflicts found...
  */

  return (conflicts);
}


/*
 * 'ppdFindChoice()' - Return a pointer to an option choice.
 */

ppd_choice_t *				/* O - Choice pointer or NULL */
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

ppd_choice_t *				/* O - Pointer to choice or NULL */
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

ppd_option_t *				/* O - Pointer to option or NULL */
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
 * 'ppdIsMarked()' - Check to see if an option is marked...
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
 *
 * Notes:
 *
 *   -1 is returned if the given option would conflict with any currently
 *   selected option.
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
	        free(cparam->current.custom_string);

	      cparam->current.custom_string = strdup(choice + 7);
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
		free(cparam->current.custom_string);

	      cparam->current.custom_string = strdup(val->value);
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
      * Unmark ManualFeed True and possibly mark ManualFeed False
      * option...
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
 * Options are returned from all groups in sorted order.
 *
 * @since CUPS 1.2@
 */

ppd_option_t *				/* O - First option or NULL */
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
 * Options are returned from all groups in sorted order.
 *
 * @since CUPS 1.2@
 */

ppd_option_t *				/* O - Next option or NULL */
ppdNextOption(ppd_file_t *ppd)		/* I - PPD file */
{
  if (!ppd)
    return (NULL);
  else
    return ((ppd_option_t *)cupsArrayNext(ppd->options));
}


/*
 * 'ppd_defaults()' - Set the defaults for this group and all sub-groups.
 */

static void
ppd_defaults(ppd_file_t  *ppd,	/* I - PPD file */
             ppd_group_t *g)	/* I - Group to default */
{
  int		i;		/* Looping var */
  ppd_option_t	*o;		/* Current option */
  ppd_group_t	*sg;		/* Current sub-group */


  for (i = g->num_options, o = g->options; i > 0; i --, o ++)
    if (strcasecmp(o->keyword, "PageRegion") != 0)
      ppdMarkOption(ppd, o->keyword, o->defchoice);

  for (i = g->num_subgroups, sg = g->subgroups; i > 0; i --, sg ++)
    ppd_defaults(ppd, sg);
}


/*
 * End of "$Id$".
 */
