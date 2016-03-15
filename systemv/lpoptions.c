/*
 * "$Id: lpoptions.c 11558 2014-02-06 18:33:34Z msweet $"
 *
 * Printer option program for CUPS.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2006 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Include necessary headers...
 */

#include <cups/cups-private.h>


/*
 * Local functions...
 */

static void	list_group(ppd_file_t *ppd, ppd_group_t *group);
static void	list_options(cups_dest_t *dest);
static void	usage(void) __attribute__((noreturn));


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i, j;			/* Looping vars */
  int		changes;		/* Did we make changes? */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  int		num_dests;		/* Number of destinations */
  cups_dest_t	*dests;			/* Destinations */
  cups_dest_t	*dest;			/* Current destination */
  char		*printer,		/* Printer name */
		*instance,		/* Instance name */
 		*option;		/* Current option */


  _cupsSetLocale(argv);

 /*
  * Loop through the command-line arguments...
  */

  dest        = NULL;
  num_dests   = 0;
  dests       = NULL;
  num_options = 0;
  options     = NULL;
  changes     = 0;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
    {
      switch (argv[i][1])
      {
        case 'd' : /* -d printer */
	    if (argv[i][2])
	      printer = argv[i] + 2;
	    else
	    {
	      i ++;
	      if (i >= argc)
	        usage();

	      printer = argv[i];
	    }

            if ((instance = strrchr(printer, '/')) != NULL)
	      *instance++ = '\0';

	    if (num_dests == 0)
	      num_dests = cupsGetDests(&dests);

            if (num_dests == 0 || !dests ||
	        (dest = cupsGetDest(printer, instance, num_dests,
		                    dests)) == NULL)
	    {
	      _cupsLangPuts(stderr, _("lpoptions: Unknown printer or class."));
	      return (1);
	    }

	   /*
	    * Set the default destination...
	    */

	    for (j = 0; j < num_dests; j ++)
	      dests[j].is_default = 0;

	    dest->is_default = 1;

	    cupsSetDests(num_dests, dests);

	    for (j = 0; j < dest->num_options; j ++)
	      if (cupsGetOption(dest->options[j].name, num_options,
	                        options) == NULL)
		num_options = cupsAddOption(dest->options[j].name,
	                                    dest->options[j].value,
	                                    num_options, &options);
	    break;

	case 'h' : /* -h server */
	    if (argv[i][2])
	      cupsSetServer(argv[i] + 2);
	    else
	    {
	      i ++;
	      if (i >= argc)
	        usage();

	      cupsSetServer(argv[i]);
	    }
	    break;

        case 'E' : /* Encrypt connection */
	    cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);
	    break;

	case 'l' : /* -l (list options) */
            if (dest == NULL)
	    {
	      if (num_dests == 0)
		num_dests = cupsGetDests(&dests);

	      if ((dest = cupsGetDest(NULL, NULL, num_dests, dests)) == NULL)
	        dest = dests;
	    }

            if (dest == NULL)
	      _cupsLangPuts(stderr, _("lpoptions: No printers."));
	    else
	      list_options(dest);

            changes = -1;
	    break;

	case 'o' : /* -o option[=value] */
            if (dest == NULL)
	    {
	      if (num_dests == 0)
		num_dests = cupsGetDests(&dests);

	      if ((dest = cupsGetDest(NULL, NULL, num_dests, dests)) == NULL)
	        dest = dests;

	      if (dest == NULL)
              {
		_cupsLangPuts(stderr, _("lpoptions: No printers."));
                return (1);
              }

	      for (j = 0; j < dest->num_options; j ++)
		if (cupsGetOption(dest->options[j].name, num_options, options) == NULL)
		  num_options = cupsAddOption(dest->options[j].name,
	                                      dest->options[j].value,
	                                      num_options, &options);
	    }

	    if (argv[i][2])
	      num_options = cupsParseOptions(argv[i] + 2, num_options, &options);
	    else
	    {
	      i ++;
	      if (i >= argc)
	        usage();

	      num_options = cupsParseOptions(argv[i], num_options, &options);
	    }

	    changes = 1;
	    break;

	case 'p' : /* -p printer */
	    if (argv[i][2])
	      printer = argv[i] + 2;
	    else
	    {
	      i ++;
	      if (i >= argc)
	        usage();

	      printer = argv[i];
	    }

            if ((instance = strrchr(printer, '/')) != NULL)
	      *instance++ = '\0';

	    if (num_dests == 0)
	      num_dests = cupsGetDests(&dests);

            if ((dest = cupsGetDest(printer, instance, num_dests, dests)) == NULL)
	    {
	      num_dests = cupsAddDest(printer, instance, num_dests, &dests);
	      dest      = cupsGetDest(printer, instance, num_dests, dests);

              if (dest == NULL)
	      {
	        _cupsLangPrintf(stderr,
		                _("lpoptions: Unable to add printer or "
				  "instance: %s"),
				strerror(errno));
		return (1);
	      }
	    }

	    for (j = 0; j < dest->num_options; j ++)
	      if (cupsGetOption(dest->options[j].name, num_options, options) == NULL)
		num_options = cupsAddOption(dest->options[j].name,
	                                    dest->options[j].value,
	                                    num_options, &options);
	    break;

	case 'r' : /* -r option (remove) */
            if (dest == NULL)
	    {
	      if (num_dests == 0)
		num_dests = cupsGetDests(&dests);

	      if ((dest = cupsGetDest(NULL, NULL, num_dests, dests)) == NULL)
	        dest = dests;

	      if (dest == NULL)
              {
		_cupsLangPuts(stderr, _("lpoptions: No printers."));
                return (1);
              }

	      for (j = 0; j < dest->num_options; j ++)
		if (cupsGetOption(dest->options[j].name, num_options,
		                  options) == NULL)
		  num_options = cupsAddOption(dest->options[j].name,
	                                      dest->options[j].value,
	                                      num_options, &options);
	    }

	    if (argv[i][2])
	      option = argv[i] + 2;
	    else
	    {
	      i ++;
	      if (i >= argc)
	        usage();

	      option = argv[i];
	    }

            for (j = 0; j < num_options; j ++)
	      if (!_cups_strcasecmp(options[j].name, option))
	      {
	       /*
	        * Remove this option...
		*/

	        num_options --;

		if (j < num_options)
		  memmove(options + j, options + j + 1, sizeof(cups_option_t) * (size_t)(num_options - j));
		break;
              }

	    changes = 1;
	    break;

        case 'x' : /* -x printer */
	    if (argv[i][2])
	      printer = argv[i] + 2;
	    else
	    {
	      i ++;
	      if (i >= argc)
	        usage();

	      printer = argv[i];
	    }

            if ((instance = strrchr(printer, '/')) != NULL)
	      *instance++ = '\0';

	    if (num_dests == 0)
	      num_dests = cupsGetDests(&dests);

            if ((dest = cupsGetDest(printer, instance, num_dests,
	                            dests)) != NULL)
	    {
              cupsFreeOptions(dest->num_options, dest->options);

             /*
	      * If we are "deleting" the default printer, then just set the
	      * number of options to 0; if it is also the system default
	      * then cupsSetDests() will remove it for us...
	      */

	      if (dest->is_default)
	      {
		dest->num_options = 0;
		dest->options     = NULL;
	      }
	      else
	      {
		num_dests --;

		j = dest - dests;
		if (j < num_dests)
		  memmove(dest, dest + 1, (size_t)(num_dests - j) * sizeof(cups_dest_t));
	      }
	    }

	    cupsSetDests(num_dests, dests);
	    dest    = NULL;
	    changes = -1;
	    break;

	default :
	    usage();
      }
    }
    else
      usage();

  if (num_dests == 0)
    num_dests = cupsGetDests(&dests);

  if (dest == NULL)
  {
    if ((dest = cupsGetDest(NULL, NULL, num_dests, dests)) != NULL)
    {
      for (j = 0; j < dest->num_options; j ++)
	if (cupsGetOption(dest->options[j].name, num_options, options) == NULL)
	  num_options = cupsAddOption(dest->options[j].name,
	                              dest->options[j].value,
	                              num_options, &options);
    }
  }

  if (dest == NULL)
    return (0);

  if (changes > 0)
  {
   /*
    * Set printer options...
    */

    cupsFreeOptions(dest->num_options, dest->options);

    dest->num_options = num_options;
    dest->options     = options;

    cupsSetDests(num_dests, dests);
  }
  else if (changes == 0)
  {
    char	buffer[10240],		/* String for options */
		*ptr;			/* Pointer into string */

    num_options = dest->num_options;
    options     = dest->options;

    for (i = 0, ptr = buffer;
         ptr < (buffer + sizeof(buffer) - 1) && i < num_options;
	 i ++)
    {
      if (i)
        *ptr++ = ' ';

      if (!options[i].value[0])
        strlcpy(ptr, options[i].name, sizeof(buffer) - (size_t)(ptr - buffer));
      else if (strchr(options[i].value, ' ') != NULL ||
               strchr(options[i].value, '\t') != NULL)
	snprintf(ptr, sizeof(buffer) - (size_t)(ptr - buffer), "%s=\'%s\'", options[i].name, options[i].value);
      else
	snprintf(ptr, sizeof(buffer) - (size_t)(ptr - buffer), "%s=%s", options[i].name, options[i].value);

      ptr += strlen(ptr);
    }

    _cupsLangPuts(stdout, buffer);
  }

  return (0);
}

/*
 * 'list_group()' - List printer-specific options from the PPD group.
 */

static void
list_group(ppd_file_t  *ppd,		/* I - PPD file */
           ppd_group_t *group)		/* I - Group to show */
{
  int		i, j;			/* Looping vars */
  ppd_option_t	*option;		/* Current option */
  ppd_choice_t	*choice;		/* Current choice */
  ppd_group_t	*subgroup;		/* Current subgroup */
  char		buffer[10240],		/* Option string buffer */
		*ptr;			/* Pointer into option string */


  for (i = group->num_options, option = group->options; i > 0; i --, option ++)
  {
    if (!_cups_strcasecmp(option->keyword, "PageRegion"))
      continue;

    snprintf(buffer, sizeof(buffer), "%s/%s:", option->keyword, option->text);

    for (j = option->num_choices, choice = option->choices,
             ptr = buffer + strlen(buffer);
         j > 0 && ptr < (buffer + sizeof(buffer) - 1);
	 j --, choice ++)
    {
      if (!_cups_strcasecmp(choice->choice, "Custom"))
      {
        ppd_coption_t	*coption;	/* Custom option */
        ppd_cparam_t	*cparam;	/* Custom parameter */
	static const char * const types[] =
	{				/* Parameter types */
	  "CURVE",
	  "INTEGER",
	  "INVCURVE",
	  "PASSCODE",
	  "PASSWORD",
	  "POINTS",
	  "REAL",
	  "STRING"
	};


        if ((coption = ppdFindCustomOption(ppd, option->keyword)) == NULL ||
	    cupsArrayCount(coption->params) == 0)
	  snprintf(ptr, sizeof(buffer) - (size_t)(ptr - buffer), " %sCustom", choice->marked ? "*" : "");
        else if (!_cups_strcasecmp(option->keyword, "PageSize") ||
	         !_cups_strcasecmp(option->keyword, "PageRegion"))
	  snprintf(ptr, sizeof(buffer) - (size_t)(ptr - buffer), " %sCustom.WIDTHxHEIGHT", choice->marked ? "*" : "");
        else
	{
	  cparam = (ppd_cparam_t *)cupsArrayFirst(coption->params);

	  if (cupsArrayCount(coption->params) == 1)
	    snprintf(ptr, sizeof(buffer) - (size_t)(ptr - buffer), " %sCustom.%s", choice->marked ? "*" : "", types[cparam->type]);
	  else
	  {
	    const char	*prefix;	/* Prefix string */


            if (choice->marked)
	      prefix = " *{";
	    else
	      prefix = " {";

	    while (cparam)
	    {
	      snprintf(ptr, sizeof(buffer) - (size_t)(ptr - buffer), "%s%s=%s", prefix, cparam->name, types[cparam->type]);
	      cparam = (ppd_cparam_t *)cupsArrayNext(coption->params);
	      prefix = " ";
	      ptr += strlen(ptr);
	    }

            if (ptr < (buffer + sizeof(buffer) - 1))
	      strlcpy(ptr, "}", sizeof(buffer) - (size_t)(ptr - buffer));
	  }
	}
      }
      else if (choice->marked)
        snprintf(ptr, sizeof(buffer) - (size_t)(ptr - buffer), " *%s", choice->choice);
      else
        snprintf(ptr, sizeof(buffer) - (size_t)(ptr - buffer), " %s", choice->choice);

      ptr += strlen(ptr);
    }

    _cupsLangPuts(stdout, buffer);
  }

  for (i = group->num_subgroups, subgroup = group->subgroups; i > 0; i --, subgroup ++)
    list_group(ppd, subgroup);
}


/*
 * 'list_options()' - List printer-specific options from the PPD file.
 */

static void
list_options(cups_dest_t *dest)		/* I - Destination to list */
{
  int		i;			/* Looping var */
  const char	*filename;		/* PPD filename */
  ppd_file_t	*ppd;			/* PPD data */
  ppd_group_t	*group;			/* Current group */


  if ((filename = cupsGetPPD(dest->name)) == NULL)
  {
    _cupsLangPrintf(stderr, _("lpoptions: Unable to get PPD file for %s: %s"),
		    dest->name, cupsLastErrorString());
    return;
  }

  if ((ppd = ppdOpenFile(filename)) == NULL)
  {
    unlink(filename);
    _cupsLangPrintf(stderr, _("lpoptions: Unable to open PPD file for %s."),
		    dest->name);
    return;
  }

  ppdMarkDefaults(ppd);
  cupsMarkOptions(ppd, dest->num_options, dest->options);

  for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
    list_group(ppd, group);

  ppdClose(ppd);
  unlink(filename);
}


/*
 * 'usage()' - Show program usage and exit.
 */

static void
usage(void)
{
  _cupsLangPuts(stdout,
                _("Usage: lpoptions [-h server] [-E] -d printer\n"
		  "       lpoptions [-h server] [-E] [-p printer] -l\n"
		  "       lpoptions [-h server] [-E] -p printer -o "
		  "option[=value] ...\n"
		  "       lpoptions [-h server] [-E] -x printer"));

  exit(1);
}


/*
 * End of "$Id: lpoptions.c 11558 2014-02-06 18:33:34Z msweet $".
 */
