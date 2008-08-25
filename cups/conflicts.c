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
 *   cupsResolveConflicts()   - Resolve conflicts in a marked PPD.
 *   ppdConflicts()           - Check to see if there are any conflicts among
 *                              the marked option choices.
 *   ppdInstallableConflict() - Test whether an option choice conflicts with an
 *                              installable option.
 *   ppd_is_installable()     - Determine whether an option is in the
 *                              InstallableOptions group.
 *   ppd_load_constraints()   - Load constraints from a PPD file.
 *   ppd_test_constraints()   - See if any constraints are active.
 */

/*
 * Include necessary headers...
 */

#include "ppd-private.h"
#include "string.h"
#include "debug.h"


/*
 * Local constants...
 */

enum
{
  _PPD_NORMAL_CONSTRAINTS,
  _PPD_INSTALLABLE_CONSTRAINTS,
  _PPD_ALL_CONSTRAINTS
};


/*
 * Local functions...
 */

static int		ppd_is_installable(ppd_group_t *installable,
			                   const char *option);
static void		ppd_load_constraints(ppd_file_t *ppd);
static cups_array_t	*ppd_test_constraints(ppd_file_t *ppd,
			                      const char *option,
					      const char *choice,
			                      int num_options,
			                      cups_option_t *options,
					      int which);


/*
 * 'cupsResolveConflicts()' - Resolve conflicts in a marked PPD.
 *
 * This function attempts to resolve any conflicts in a marked PPD, returning
 * a list of option changes that are required to resolve any conflicts.  On
 * input, "num_options" and "options" contain any pending option changes that
 * have not yet been marked, while "option" and "choice" contain the most recent
 * selection which may or may not be in "num_options" or "options".
 *
 * On successful return, "num_options" and "options" are updated to contain
 * "option" and "choice" along with any changes required to resolve conflicts
 * specified in the PPD file.  If option conflicts cannot be resolved,
 * "num_options" and "options" are not changed.
 *
 * @code ppdResolveConflicts@ uses one of two sources of option constraint
 * information.  The preferred constraint information is defined by
 * @code cupsUIConstraints@ and @code cupsUIResolver@ attributes - in this
 * case, the PPD file provides constraint resolution actions.  In this case,
 * it should not be possible for @ppdResolveConflicts@ to fail, however it
 * will do so if a resolver loop is detected.
 *
 * The backup constraint infomration is defined by the
 * @code UIConstraints@ and @code NonUIConstraints@ attributes.  These
 * constraints are resolved algorithmically by selecting the default choice
 * for the conflicting option.  Unfortunately, this method is far more likely
 * to fail.
 *
 * @since CUPS 1.4@
 */

int					/* O  - 1 on success, 0 on failure */
cupsResolveConflicts(
    ppd_file_t    *ppd,			/* I  - PPD file */
    const char    *option,		/* I  - Newly selected option or @code NULL@ for none */
    const char    *choice,		/* I  - Newly selected choice or @code NULL@ for none */
    int           *num_options,		/* IO - Number of additional selected options */
    cups_option_t **options)		/* IO - Additional selected options */
{
  int			i,		/* Looping var */
			num_newopts,	/* Number of new options */
			num_resopts;	/* Number of resolver options */
  cups_option_t		*newopts,	/* New options */
			*resopts;	/* Resolver options */
  cups_array_t		*active,	/* Active constraints */
			*pass,		/* Resolvers for this pass */
			*resolvers;	/* Resolvers we have used */
  _ppd_cups_uiconsts_t	*consts;	/* Current constraints */
  _ppd_cups_uiconst_t	*constptr;	/* Current constraint */
  ppd_attr_t		*resolver;	/* Current resolver */
  const char		*value;		/* Selected option value */
  int			changed;	/* Did we change anything? */
  ppd_choice_t		*marked;	/* Marked choice */


 /*
  * Range check input...
  */

  if (!ppd || !num_options || !options || (option == NULL) != (choice == NULL))
    return (0);

 /*
  * Build a shadow option array...
  */

  num_newopts = 0;
  newopts     = NULL;

  for (i = 0; i < *num_options; i ++)
    num_newopts = cupsAddOption((*options)[i].name, (*options)[i].value,
                                num_newopts, &newopts);
  if (option && strcasecmp(option, "Collate"))
    num_newopts = cupsAddOption(option, choice, num_newopts, &newopts);

 /*
  * Loop until we have no conflicts...
  */

  cupsArraySave(ppd->sorted_attrs);

  resolvers = NULL;
  pass      = cupsArrayNew((cups_array_func_t)strcasecmp, NULL);

  while ((active = ppd_test_constraints(ppd, NULL, NULL, num_newopts, newopts,
                                        _PPD_ALL_CONSTRAINTS)) != NULL)
  {
    if (!resolvers)
      resolvers = cupsArrayNew((cups_array_func_t)strcasecmp, NULL);

    for (consts = (_ppd_cups_uiconsts_t *)cupsArrayFirst(active), changed = 0;
         consts;
	 consts = (_ppd_cups_uiconsts_t *)cupsArrayNext(active))
    {
      if (consts->resolver[0])
      {
       /*
        * Look up the resolver...
	*/

        if (cupsArrayFind(pass, consts->resolver))
	  continue;			/* Already applied this resolver... */

        if (cupsArrayFind(resolvers, consts->resolver))
	{
	 /*
	  * Resolver loop!
	  */

	  DEBUG_printf(("ppdResolveConflicts: Resolver loop with %s!\n",
	                consts->resolver));
          goto error;
	}

        if ((resolver = ppdFindAttr(ppd, "cupsUIResolver",
	                            consts->resolver)) == NULL)
        {
	  DEBUG_printf(("ppdResolveConflicts: Resolver %s not found!\n",
	                consts->resolver));
	  goto error;
	}

        if (!resolver->value)
	{
	  DEBUG_printf(("ppdResolveConflicts: Resolver %s has no value!\n",
	                consts->resolver));
	  goto error;
	}

       /*
        * Add the options from the resolver...
	*/

        cupsArrayAdd(pass, consts->resolver);
	cupsArrayAdd(resolvers, consts->resolver);

        if (option && choice)
	{
	  resopts     = NULL;
	  num_resopts = _ppdParseOptions(resolver->value, 0, &resopts);

          if ((value = cupsGetOption(option, num_newopts, newopts)) != NULL &&
	      !strcasecmp(value, choice))
	  {
	    DEBUG_printf(("cupsResolveConflicts: Resolver %s changes %s!\n",
	                  consts->resolver, option));
	    cupsFreeOptions(num_resopts, resopts);
	    goto error;
          }

	  cupsFreeOptions(num_resopts, resopts);
	}

        num_newopts = _ppdParseOptions(resolver->value, num_newopts, &newopts);
        changed     = 1;
      }
      else
      {
       /*
        * Try resolving by choosing the default values for non-installable
	* options, then by iterating through the possible choices...
	*/

        int		j;		/* Looping var */
	ppd_choice_t	*cptr;		/* Current choice */
        cups_array_t	*test;		/* Test array for conflicts */


        for (i = consts->num_constraints, constptr = consts->constraints;
	     i > 0;
	     i --, constptr ++)
	{
	  if (constptr->installable ||
	      !strcasecmp(constptr->option->keyword, "PageSize") ||
	      !strcasecmp(constptr->option->keyword, "PageRegion"))
	    continue;

	  if (option && !strcasecmp(constptr->option->keyword, option))
	    continue;

          if ((value = cupsGetOption(constptr->option->keyword, num_newopts,
	                             newopts)) == NULL)
          {
	    marked = ppdFindMarkedChoice(ppd, constptr->option->keyword);
	    value  = marked ? marked->choice : "";
	  }

         /*
	  * Try the default choice...
	  */

          test = NULL;

          if (strcasecmp(value, constptr->option->defchoice) &&
	      (test = ppd_test_constraints(ppd, constptr->option->keyword,
	                                   constptr->option->defchoice,
					   num_newopts, newopts,
					   _PPD_ALL_CONSTRAINTS)) == NULL)
	  {
	   /*
	    * That worked...
	    */

	    num_newopts = cupsAddOption(constptr->option->keyword,
	                                constptr->option->defchoice,
					num_newopts, &newopts);
            changed     = 1;
	  }
	  else
	  {
	   /*
	    * Try each choice instead...
	    */

            cupsArrayDelete(test);

            for (j = constptr->option->num_choices,
	             cptr = constptr->option->choices;
		 j > 0;
		 j --, cptr ++)
            {
	      test = NULL;

	      if (strcasecmp(value, cptr->choice) &&
	          strcasecmp(constptr->option->defchoice, cptr->choice) &&
	          (test = ppd_test_constraints(ppd, constptr->option->keyword,
	                                       cptr->choice, num_newopts,
					       newopts,
					       _PPD_ALL_CONSTRAINTS)) == NULL)
	      {
	       /*
		* This choice works...
		*/

		num_newopts = cupsAddOption(constptr->option->keyword,
					    cptr->choice, num_newopts,
					    &newopts);
		changed     = 1;
		break;
	      }

              cupsArrayDelete(test);
	    }
          }
        }
      }

      if (!changed)
      {
	DEBUG_puts("ppdResolveConflicts: Unable to automatically resolve "
		   "constraint!");
	goto error;
      }
    }

    cupsArrayClear(pass);
    cupsArrayDelete(active);
  }

 /*
  * Free the caller's option array...
  */

  cupsFreeOptions(*num_options, *options);

 /*
  * If Collate is the option we are testing, add it here.  Otherwise, remove
  * any Collate option from the resolve list since the filters automatically
  * handle manual collation...
  */

  if (option && !strcasecmp(option, "Collate"))
    num_newopts = cupsAddOption(option, choice, num_newopts, &newopts);
  else
    num_newopts = cupsRemoveOption("Collate", num_newopts, &newopts);

 /*
  * Return the new list of options to the caller...
  */

  *num_options = num_newopts;
  *options     = newopts;

  cupsArrayDelete(pass);
  cupsArrayDelete(resolvers);

  cupsArrayRestore(ppd->sorted_attrs);

  return (1);

 /*
  * If we get here, we failed to resolve...
  */

  error:

  cupsFreeOptions(num_newopts, newopts);

  cupsArrayDelete(pass);
  cupsArrayDelete(resolvers);

  cupsArrayRestore(ppd->sorted_attrs);

  return (0);
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
  int			i,		/* Looping variable */
			conflicts;	/* Number of conflicts */
  cups_array_t		*active;	/* Active conflicts */
  _ppd_cups_uiconsts_t	*c;		/* Current constraints */
  _ppd_cups_uiconst_t	*cptr;		/* Current constraint */
  ppd_option_t	*o;			/* Current option */


  if (!ppd)
    return (0);

 /*
  * Clear all conflicts...
  */

  for (o = ppdFirstOption(ppd); o; o = ppdNextOption(ppd))
    o->conflicted = 0;

 /*
  * Test for conflicts...
  */

  active    = ppd_test_constraints(ppd, NULL, NULL, 0, NULL,
                                   _PPD_ALL_CONSTRAINTS);
  conflicts = cupsArrayCount(active);

 /*
  * Loop through all of the UI constraints and flag any options
  * that conflict...
  */

  for (c = (_ppd_cups_uiconsts_t *)cupsArrayFirst(active);
       c;
       c = (_ppd_cups_uiconsts_t *)cupsArrayNext(active))
  {
    for (i = c->num_constraints, cptr = c->constraints;
         i > 0;
	 i --, cptr ++)
      cptr->option->conflicted = 1;
  }

  cupsArrayDelete(active);

 /*
  * Return the number of conflicts found...
  */

  return (conflicts);
}


/*
 * 'ppdInstallableConflict()' - Test whether an option choice conflicts with
 *                              an installable option.
 *
 * This function tests whether a particular option choice is available based
 * on constraints against options in the "InstallableOptions" group.
 *
 * @since CUPS 1.4@
 */

int					/* O - 1 if conflicting, 0 if not conflicting */
ppdInstallableConflict(
    ppd_file_t *ppd,			/* I - PPD file */
    const char *option,			/* I - Option */
    const char *choice)			/* I - Choice */
{
  cups_array_t	*active;		/* Active conflicts */


 /* 
  * Range check input...
  */

  if (!ppd || !option || !choice)
    return (0);

 /*
  * Test constraints using the new option...
  */

  active = ppd_test_constraints(ppd, option, choice, 0, NULL,
				_PPD_INSTALLABLE_CONSTRAINTS);

  cupsArrayDelete(active);

  return (active != NULL);
}


/*
 * 'ppd_is_installable()' - Determine whether an option is in the
 *                          InstallableOptions group.
 */

static int				/* O - 1 if installable, 0 if normal */
ppd_is_installable(
    ppd_group_t *installable,		/* I - InstallableOptions group */
    const char  *name)			/* I - Option name */
{
  if (installable)
  {
    int			i;		/* Looping var */
    ppd_option_t	*option;	/* Current option */


    for (i = installable->num_options, option = installable->options;
         i > 0;
	 i --, option ++)
      if (!strcasecmp(option->keyword, name))
        return (1);
  }

  return (0);
}


/*
 * 'ppd_load_constraints()' - Load constraints from a PPD file.
 */

static void
ppd_load_constraints(ppd_file_t *ppd)	/* I - PPD file */
{
  int		i;			/* Looping var */
  ppd_const_t	*oldconst;		/* Current UIConstraints data */
  ppd_attr_t	*constattr;		/* Current cupsUIConstraints attribute */
  _ppd_cups_uiconsts_t	*consts;	/* Current cupsUIConstraints data */
  _ppd_cups_uiconst_t	*constptr;	/* Current constraint */
  ppd_group_t	*installable;		/* Installable options group */
  const char	*vptr;			/* Pointer into constraint value */
  char		option[PPD_MAX_NAME],	/* Option name/MainKeyword */
		choice[PPD_MAX_NAME],	/* Choice/OptionKeyword */
		*ptr;			/* Pointer into option or choice */


 /*
  * Create an array to hold the constraint data...
  */

  ppd->cups_uiconstraints = cupsArrayNew(NULL, NULL);

 /*
  * Find the installable options group if it exists...
  */

  for (i = ppd->num_groups, installable = ppd->groups;
       i > 0;
       i --, installable ++)
    if (!strcasecmp(installable->name, "InstallableOptions"))
      break;

  if (i <= 0)
    installable = NULL;

 /*
  * See what kind of constraint data we have in the PPD...
  */

  if ((constattr = ppdFindAttr(ppd, "cupsUIConstraints", NULL)) != NULL)
  {
   /*
    * Load new-style cupsUIConstraints data...
    */

    for (; constattr;
         constattr = ppdFindNextAttr(ppd, "cupsUIConstraints", NULL))
    {
      if (!constattr->value)
      {
        DEBUG_puts("ppd_load_constraints: Bad cupsUIConstraints value!");
        continue;
      }

      for (i = 0, vptr = strchr(constattr->value, '*');
           vptr;
	   i ++, vptr = strchr(vptr + 1, '*'));

      if (i == 0)
      {
        DEBUG_puts("ppd_load_constraints: Bad cupsUIConstraints value!");
        continue;
      }

      if ((consts = calloc(1, sizeof(_ppd_cups_uiconsts_t))) == NULL)
      {
        DEBUG_puts("ppd_load_constraints: Unable to allocate memory for "
	           "cupsUIConstraints!");
        return;
      }

      if ((constptr = calloc(i, sizeof(_ppd_cups_uiconst_t))) == NULL)
      {
        free(consts);
        DEBUG_puts("ppd_load_constraints: Unable to allocate memory for "
	           "cupsUIConstraints!");
        return;
      }

      consts->num_constraints = i;
      consts->constraints     = constptr;

      strlcpy(consts->resolver, constattr->spec, sizeof(consts->resolver));

      for (i = 0, vptr = strchr(constattr->value, '*');
           vptr;
	   i ++, vptr = strchr(vptr, '*'), constptr ++)
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

        constptr->option      = ppdFindOption(ppd, option);
        constptr->choice      = ppdFindChoice(constptr->option, choice);
        constptr->installable = ppd_is_installable(installable, option);
	consts->installable   |= constptr->installable;

        if (!constptr->option || (!constptr->choice && choice[0]))
	{
	  DEBUG_printf(("ppd_load_constraints: Unknown option *%s %s!\n",
	                option, choice));
	  break;
	}
      }

      if (!vptr)
        cupsArrayAdd(ppd->cups_uiconstraints, consts);
      else
      {
        free(consts->constraints);
	free(consts);
      }
    }
  }
  else
  {
   /*
    * Load old-style [Non]UIConstraints data...
    */

    for (i = ppd->num_consts, oldconst = ppd->consts; i > 0; i --, oldconst ++)
    {
     /*
      * Weed out nearby duplicates, since the PPD spec requires that you
      * define both "*Foo foo *Bar bar" and "*Bar bar *Foo foo"...
      */

      if (i > 1 &&
          !strcasecmp(oldconst[0].option1, oldconst[1].option2) &&
	  !strcasecmp(oldconst[0].choice1, oldconst[1].choice2) &&
	  !strcasecmp(oldconst[0].option2, oldconst[1].option1) &&
	  !strcasecmp(oldconst[0].choice2, oldconst[1].choice1))
        continue;

     /*
      * Allocate memory...
      */

      if ((consts = calloc(1, sizeof(_ppd_cups_uiconsts_t))) == NULL)
      {
        DEBUG_puts("ppd_load_constraints: Unable to allocate memory for "
	           "UIConstraints!");
        return;
      }

      if ((constptr = calloc(2, sizeof(_ppd_cups_uiconst_t))) == NULL)
      {
        free(consts);
        DEBUG_puts("ppd_load_constraints: Unable to allocate memory for "
	           "UIConstraints!");
        return;
      }

     /*
      * Fill in the information...
      */

      consts->num_constraints = 2;
      consts->constraints     = constptr;

      if (!strncasecmp(oldconst->option1, "Custom", 6) &&
          !strcasecmp(oldconst->choice1, "True"))
      {
	constptr[0].option      = ppdFindOption(ppd, oldconst->option1 + 6);
	constptr[0].choice      = ppdFindChoice(constptr[0].option, "Custom");
        constptr[0].installable = 0;
      }
      else
      {
	constptr[0].option      = ppdFindOption(ppd, oldconst->option1);
	constptr[0].choice      = ppdFindChoice(constptr[0].option,
	                                        oldconst->choice1);
	constptr[0].installable = ppd_is_installable(installable,
						     oldconst->option1);
      }

      if (!constptr[0].option || (!constptr[0].choice && oldconst->choice1[0]))
      {
        DEBUG_printf(("ppd_load_constraints: Unknown option *%s %s!\n",
	              oldconst->option1, oldconst->choice1));
        free(consts->constraints);
	free(consts);
	continue;
      }

      if (!strncasecmp(oldconst->option2, "Custom", 6) &&
          !strcasecmp(oldconst->choice2, "True"))
      {
	constptr[1].option      = ppdFindOption(ppd, oldconst->option2 + 6);
	constptr[1].choice      = ppdFindChoice(constptr[1].option, "Custom");
        constptr[1].installable = 0;
      }
      else
      {
	constptr[1].option      = ppdFindOption(ppd, oldconst->option2);
	constptr[1].choice      = ppdFindChoice(constptr[1].option,
	                                        oldconst->choice2);
	constptr[1].installable = ppd_is_installable(installable,
						     oldconst->option2);
      }

      if (!constptr[1].option || (!constptr[1].choice && oldconst->choice2[0]))
      {
        DEBUG_printf(("ppd_load_constraints: Unknown option *%s %s!\n",
	              oldconst->option2, oldconst->choice2));
        free(consts->constraints);
	free(consts);
	continue;
      }

      consts->installable = constptr[0].installable || constptr[1].installable;

     /*
      * Add it to the constraints array...
      */

      cupsArrayAdd(ppd->cups_uiconstraints, consts);
    }
  }
}


/*
 * 'ppd_test_constraints()' - See if any constraints are active.
 */

static cups_array_t *			/* O - Array of active constraints */
ppd_test_constraints(
    ppd_file_t    *ppd,			/* I - PPD file */
    const char    *option,		/* I - Current option */
    const char    *choice,		/* I - Current choice */
    int           num_options,		/* I - Number of additional options */
    cups_option_t *options,		/* I - Additional options */
    int           which)		/* I - Which constraints to test */
{
  int			i;		/* Looping var */
  _ppd_cups_uiconsts_t	*consts;	/* Current constraints */
  _ppd_cups_uiconst_t	*constptr;	/* Current constraint */
  ppd_choice_t		key,		/* Search key */
			*marked;	/* Marked choice */
  cups_array_t		*active = NULL;	/* Active constraints */
  const char		*value;		/* Current value */
  int			option_conflict;/* Conflict with current option? */


  DEBUG_printf(("ppd_test_constraints(ppd=%p, num_options=%d, options=%p, "
                "which=%d)\n", ppd, num_options, options, which));

  if (!ppd->cups_uiconstraints)
    ppd_load_constraints(ppd);

  DEBUG_printf(("ppd_test_constraints: %d constraints!\n",
	        cupsArrayCount(ppd->cups_uiconstraints)));

  cupsArraySave(ppd->marked);

  for (consts = (_ppd_cups_uiconsts_t *)cupsArrayFirst(ppd->cups_uiconstraints);
       consts;
       consts = (_ppd_cups_uiconsts_t *)cupsArrayNext(ppd->cups_uiconstraints))
  {
    DEBUG_printf(("ppd_test_constraints: installable=%d, resolver=\"%s\", "
                  "num_constraints=%d option1=\"%s\", choice1=\"%s\", "
		  "option2=\"%s\", choice2=\"%s\", ...\n",
		  consts->installable, consts->resolver, consts->num_constraints,
		  consts->constraints[0].option->keyword,
		  consts->constraints[0].choice ?
		      consts->constraints[0].choice->choice : "",
		  consts->constraints[1].option->keyword,
		  consts->constraints[1].choice ?
		      consts->constraints[1].choice->choice : ""));

    if (which != _PPD_ALL_CONSTRAINTS && which != consts->installable)
      continue;

    DEBUG_puts("ppd_test_constraints: Testing...");

    for (i = consts->num_constraints, constptr = consts->constraints,
             option_conflict = 0;
         i > 0;
	 i --, constptr ++)
    {
      DEBUG_printf(("ppd_test_constraints: %s=%s?\n", constptr->option->keyword,
		    constptr->choice ? constptr->choice->choice : ""));

      if (constptr->choice &&
          (!strcasecmp(constptr->option->keyword, "PageSize") ||
           !strcasecmp(constptr->option->keyword, "PageRegion")))
      {
       /*
        * PageSize and PageRegion are used depending on the selected input slot
	* and manual feed mode.  Validate against the selected page size instead
	* of an individual option...
	*/

        if (option && choice &&
	    (!strcasecmp(option, "PageSize") ||
	     !strcasecmp(option, "PageRegion")))
	{
	  value = choice;

	  if (!strcasecmp(value, constptr->choice->choice))
	    option_conflict = 1;
        }
	else if ((value = cupsGetOption("PageSize", num_options,
	                                options)) == NULL)
	  if ((value = cupsGetOption("PageRegion", num_options,
	                             options)) == NULL)
	    if ((value = cupsGetOption("media", num_options, options)) == NULL)
	    {
	      ppd_size_t *size = ppdPageSize(ppd, NULL);

              if (size)
	        value = size->name;
	    }

        if (!value || strcasecmp(value, constptr->choice->choice))
	{
	  DEBUG_puts("ppd_test_constraints: NO");
	  break;
	}
      }
      else if (constptr->choice)
      {
        if (option && choice && !strcasecmp(option, constptr->option->keyword))
	{
	  if (strcasecmp(choice, constptr->choice->choice))
	    break;

	  option_conflict = 1;
	}
        else if ((value = cupsGetOption(constptr->option->keyword, num_options,
	                                options)) != NULL)
        {
	  if (strcasecmp(value, constptr->choice->choice))
	  {
	    DEBUG_puts("ppd_test_constraints: NO");
	    break;
	  }
	}
        else if (!constptr->choice->marked)
	{
	  DEBUG_puts("ppd_test_constraints: NO");
	  break;
	}
      }
      else if (option && choice &&
               !strcasecmp(option, constptr->option->keyword))
      {
	if (!strcasecmp(choice, "None") || !strcasecmp(choice, "Off") ||
	    !strcasecmp(choice, "False"))
          break;

	option_conflict = 1;
      }
      else if ((value = cupsGetOption(constptr->option->keyword, num_options,
	                              options)) != NULL)
      {
	if (!strcasecmp(value, "None") || !strcasecmp(value, "Off") ||
	    !strcasecmp(value, "False"))
	{
	  DEBUG_puts("ppd_test_constraints: NO");
	  break;
	}
      }
      else
      {
        key.option = constptr->option;

	if ((marked = (ppd_choice_t *)cupsArrayFind(ppd->marked, &key))
	        != NULL &&
	    (!strcasecmp(marked->choice, "None") ||
	     !strcasecmp(marked->choice, "Off") ||
	     !strcasecmp(marked->choice, "False")))
	{
	  DEBUG_puts("ppd_test_constraints: NO");
	  break;
	}
      }
    }

    if (i <= 0 && (!option || option_conflict))
    {
      if (!active)
        active = cupsArrayNew(NULL, NULL);

      cupsArrayAdd(active, consts);
      DEBUG_puts("ppd_test_constraints: Added...");
    }
  }

  cupsArrayRestore(ppd->marked);

  DEBUG_printf(("ppd_test_constraints: Found %d active constraints!\n",
                cupsArrayCount(active)));

  return (active);
}


/*
 * End of "$Id$".
 */
