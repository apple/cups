/*
 * "$Id$"
 *
 *   Option marking routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
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

int				/* O - Number of conflicts found */
ppdConflicts(ppd_file_t *ppd)	/* I - PPD to check */
{
  int		i, j, k,	/* Looping variables */
		conflicts;	/* Number of conflicts */
  ppd_const_t	*c;		/* Current constraint */
  ppd_group_t	*g, *sg;	/* Groups */
  ppd_option_t	*o1, *o2;	/* Options */
  ppd_choice_t	*c1, *c2;	/* Choices */


  if (ppd == NULL)
    return (0);

 /*
  * Clear all conflicts...
  */

  conflicts = 0;

  for (i = ppd->num_groups, g = ppd->groups; i > 0; i --, g ++)
  {
    for (j = g->num_options, o1 = g->options; j > 0; j --, o1 ++)
      o1->conflicted = 0;

    for (j = g->num_subgroups, sg = g->subgroups; j > 0; j --, sg ++)
      for (k = sg->num_options, o1 = sg->options; k > 0; k --, o1 ++)
        o1->conflicted = 0;
  }

 /*
  * Loop through all of the UI constraints and flag any options
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
          strcasecmp(c1->choice, "None") == 0 ||
          strcasecmp(c1->choice, "Off") == 0 ||
          strcasecmp(c1->choice, "False") == 0)
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
          strcasecmp(c2->choice, "None") == 0 ||
          strcasecmp(c2->choice, "Off") == 0 ||
          strcasecmp(c2->choice, "False") == 0)
        c2 = NULL;
    }

   /*
    * If both options are marked then there is a conflict...
    */

    if (c1 != NULL && c1->marked &&
        c2 != NULL && c2->marked)
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
  int		i;		/* Looping var */
  ppd_choice_t	*c;		/* Current choice */


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
  int		i;		/* Looping var */
  ppd_option_t	*o;		/* Pointer to option */
  ppd_choice_t	*c;		/* Pointer to choice */


  if ((o = ppdFindOption(ppd, option)) == NULL)
    return (NULL);

  for (i = o->num_choices, c = o->choices; i > 0; i --, c ++)
    if (c->marked)
      return (c);

  return (NULL);
}


/*
 * 'ppdFindOption()' - Return a pointer to the specified option.
 */

ppd_option_t *				/* O - Pointer to option or NULL */
ppdFindOption(ppd_file_t *ppd,		/* I - PPD file data */
              const char *option)	/* I - Option/Keyword name */
{
  ppd_option_t	key;			/* Option search key */


 /*
  * Range check input...
  */

  if (!ppd || !option)
    return (NULL);

 /*
  * Search...
  */

  strlcpy(key.keyword, option, sizeof(key.keyword));

  return ((ppd_option_t *)cupsArrayFind(ppd->options, &key));
}


/*
 * 'ppdIsMarked()' - Check to see if an option is marked...
 */

int				/* O - Non-zero if option is marked */
ppdIsMarked(ppd_file_t *ppd,	/* I - PPD file data */
            const char *option,	/* I - Option/Keyword name */
            const char *choice)	/* I - Choice name */
{
  ppd_option_t	*o;		/* Option pointer */
  ppd_choice_t	*c;		/* Choice pointer */


  if (ppd == NULL)
    return (0);

  if ((o = ppdFindOption(ppd, option)) == NULL)
    return (0);

  if ((c = ppdFindChoice(o, choice)) == NULL)
    return (0);

  return (c->marked);
}


/*
 * 'ppdMarkDefaults()' - Mark all default options in the PPD file.
 */

void
ppdMarkDefaults(ppd_file_t *ppd)/* I - PPD file record */
{
  int		i;		/* Looping variables */
  ppd_group_t	*g;		/* Current group */


  if (ppd == NULL)
    return;

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
  ppd_choice_t	*c;			/* Choice pointer */


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
      for (i = 0; i < o->num_choices; i ++)
	o->choices[i].marked = 0;
  }

 /*
  * Check for custom options...
  */

  if ((o = ppdFindOption(ppd, option)) == NULL)
    return (0);


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
      char		units[33];	/* Custom points units */

      if ((coption = ppdFindCustomOption(ppd, option)) != NULL)
      {
        if ((cparam = (ppd_cparam_t *)cupsArrayFirst(coption->params)) == NULL)
	  return (0);

        switch (cparam->type)
	{
	  case PPD_CUSTOM_CURVE :
	  case PPD_CUSTOM_INVCURVE :
	  case PPD_CUSTOM_REAL :
	      cparam->current.custom_real = atof(choice + 7);
	      break;

	  case PPD_CUSTOM_POINTS :
	      if (sscanf(choice + 7, "%f%s", &(cparam->current.custom_points),
	                 units) < 2)
		strcpy(units, "pt");

              if (!strcasecmp(units, "cm"))
	        cparam->current.custom_points *= 72.0 / 2.54;	      
              else if (!strcasecmp(units, "mm"))
	        cparam->current.custom_points *= 72.0 / 25.4;	      
              else if (!strcasecmp(units, "m"))
	        cparam->current.custom_points *= 72.0 / 0.0254;	      
              else if (!strcasecmp(units, "in"))
	        cparam->current.custom_points *= 72.0;	      
              else if (!strcasecmp(units, "ft"))
	        cparam->current.custom_points *= 12 * 72.0;	      
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
  }
  else if (choice[0] == '{')
  {
   /*
    * Handle multi-value custom options...
    */

    ppd_coption_t	*coption;	/* Custom option */
    ppd_cparam_t	*cparam;	/* Custom parameter */
    char		units[33];	/* Custom points units */
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
	      cparam->current.custom_real = atof(val->value);
	      break;

	  case PPD_CUSTOM_POINTS :
	      if (sscanf(val->value, "%f%s", &(cparam->current.custom_points),
	        	 units) < 2)
		strcpy(units, "pt");

              if (!strcasecmp(units, "cm"))
		cparam->current.custom_points *= 72.0 / 2.54;	      
              else if (!strcasecmp(units, "mm"))
		cparam->current.custom_points *= 72.0 / 25.4;	      
              else if (!strcasecmp(units, "m"))
		cparam->current.custom_points *= 72.0 / 0.0254;	      
              else if (!strcasecmp(units, "in"))
		cparam->current.custom_points *= 72.0;	      
              else if (!strcasecmp(units, "ft"))
		cparam->current.custom_points *= 12 * 72.0;	      
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

  c->marked = 1;

  if (o->ui != PPD_UI_PICKMANY)
  {
   /*
    * Unmark all other choices...
    */

    for (i = o->num_choices, c = o->choices; i > 0; i --, c ++)
      if (strcasecmp(c->choice, choice))
      {
        c->marked = 0;

	if (!strcasecmp(option, "PageSize") ||
	    !strcasecmp(option, "PageRegion"))
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
	      for (j = 0; j < o->num_choices; j ++)
        	o->choices[j].marked = 0;
	  }
	  else
	  {
	    if ((o = ppdFindOption(ppd, "PageSize")) != NULL)
	      for (j = 0; j < o->num_choices; j ++)
        	o->choices[j].marked = 0;
	  }
	}
	else if (!strcasecmp(option, "InputSlot"))
	{
	 /*
	  * Unmark ManualFeed True and possibly mark ManualFeed False
	  * option...
	  */

	  if ((o = ppdFindOption(ppd, "ManualFeed")) != NULL)
	    for (j = 0; j < o->num_choices; j ++)
              o->choices[j].marked = !strcasecmp(o->choices[j].choice, "False");
	}
	else if (!strcasecmp(option, "ManualFeed") &&
	         !strcasecmp(choice, "True"))
	{
	 /*
	  * Unmark InputSlot option...
	  */

	  if ((o = ppdFindOption(ppd, "InputSlot")) != NULL)
	    for (j = 0; j < o->num_choices; j ++)
              o->choices[j].marked = 0;
	}
      }
  }

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


  if (g == NULL)
    return;

  for (i = g->num_options, o = g->options; i > 0; i --, o ++)
    if (strcasecmp(o->keyword, "PageRegion") != 0)
      ppdMarkOption(ppd, o->keyword, o->defchoice);

  for (i = g->num_subgroups, sg = g->subgroups; i > 0; i --, sg ++)
    ppd_defaults(ppd, sg);
}


/*
 * End of "$Id$".
 */
