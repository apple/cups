/*
 * "$Id: mark.c,v 1.24.2.4 2002/08/10 00:05:45 mike Exp $"
 *
 *   Option marking routines for the Common UNIX Printing System (CUPS).
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   ppdConflicts()        - Check to see if there are any conflicts.
 *   ppdFindChoice()       - Return a pointer to an option choice.
 *   ppdFindMarkedChoice() - Return the marked choice for the specified option.
 *   ppdFindOption()       - Return a pointer to the specified option.
 *   ppdIsMarked()         - Check to see if an option is marked...
 *   ppdMarkDefaults()     - Mark all default options in the PPD file.
 *   ppdMarkOption()       - Mark an option in a PPD file.
 *   ppd_defaults()        - Set the defaults for this group and all sub-groups.
 */

/*
 * Include necessary headers...
 */

#include "ppd.h"
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
  int		i, j, k;	/* Looping vars */
  ppd_option_t	*o;		/* Pointer to option */
  ppd_group_t	*g,		/* Pointer to group */
		*sg;		/* Pointer to subgroup */


  if (ppd == NULL || option == NULL)
    return (NULL);

  for (i = ppd->num_groups, g = ppd->groups; i > 0; i --, g ++)
  {
    for (j = g->num_options, o = g->options; j > 0; j --, o ++)
      if (strcasecmp(o->keyword, option) == 0)
	return (o);

    for (j = g->num_subgroups, sg = g->subgroups; j > 0; j --, sg ++)
      for (k = sg->num_options, o = sg->options; k > 0; k --, o ++)
	if (strcasecmp(o->keyword, option) == 0)
	  return (o);
  }

  return (NULL);
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
  int		i;		/* Looping var */
  ppd_option_t	*o;		/* Option pointer */
  ppd_choice_t	*c;		/* Choice pointer */


  if (ppd == NULL)
    return (0);

  if (strcasecmp(option, "PageSize") == 0 && strncasecmp(choice, "Custom.", 7) == 0)
  {
   /*
    * Handle variable page sizes...
    */

    ppdPageSize(ppd, choice);
    choice = "Custom";
  }

  if ((o = ppdFindOption(ppd, option)) == NULL)
    return (0);

  for (i = o->num_choices, c = o->choices; i > 0; i --, c ++)
    if (strcasecmp(c->choice, choice) == 0)
      break;

  if (i)
  {
   /*
    * Option found; mark it and then handle unmarking any other options.
    */

    c->marked = 1;

    if (o->ui != PPD_UI_PICKMANY)
      for (i = o->num_choices, c = o->choices; i > 0; i --, c ++)
	if (strcasecmp(c->choice, choice) != 0)
          c->marked = 0;

    if (strcasecmp(option, "PageSize") == 0 || strcasecmp(option, "PageRegion") == 0)
    {
     /*
      * Mark current page size...
      */

      for (i = 0; i < ppd->num_sizes; i ++)
	ppd->sizes[i].marked = strcasecmp(ppd->sizes[i].name, choice) == 0;

     /*
      * Unmark the current PageSize or PageRegion setting, as appropriate...
      */

      if (strcasecmp(option, "PageSize") == 0)
      {
	if ((o = ppdFindOption(ppd, "PageRegion")) != NULL)
	  for (i = 0; i < o->num_choices; i ++)
            o->choices[i].marked = 0;
      }
      else
      {
	if ((o = ppdFindOption(ppd, "PageSize")) != NULL)
	  for (i = 0; i < o->num_choices; i ++)
            o->choices[i].marked = 0;
      }
    }
    else if (strcasecmp(option, "InputSlot") == 0)
    {
     /*
      * Unmark ManualFeed option...
      */

      if ((o = ppdFindOption(ppd, "ManualFeed")) != NULL)
	for (i = 0; i < o->num_choices; i ++)
          o->choices[i].marked = 0;
    }
    else if (strcasecmp(option, "ManualFeed") == 0)
    {
     /*
      * Unmark InputSlot option...
      */

      if ((o = ppdFindOption(ppd, "InputSlot")) != NULL)
	for (i = 0; i < o->num_choices; i ++)
          o->choices[i].marked = 0;
    }
  }

  return (ppdConflicts(ppd));
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
 * End of "$Id: mark.c,v 1.24.2.4 2002/08/10 00:05:45 mike Exp $".
 */
