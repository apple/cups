/*
 * Printer option program for CUPS.
 *
 * Copyright © 2007-2018 by Apple Inc.
 * Copyright © 1997-2006 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups-private.h>
#include <cups/ppd-private.h>


/*
 * Local functions...
 */

static void	list_group(ppd_file_t *ppd, ppd_group_t *group);
static void	list_options(cups_dest_t *dest);
static void	usage(void) _CUPS_NORETURN;


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
  char		*opt,			/* Option pointer */
		*printer,		/* Printer name */
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
  {
    if (!strcmp(argv[i], "--help"))
      usage();
    else if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
	switch (*opt)
	{
	  case 'd' : /* -d printer */
	      if (opt[1] != '\0')
	      {
		printer = opt + 1;
		opt += strlen(opt) - 1;
	      }
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

	      if (num_dests == 0 || !dests || (dest = cupsGetDest(printer, instance, num_dests, dests)) == NULL)
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
	      if (opt[1] != '\0')
	      {
		cupsSetServer(opt + 1);
		opt += strlen(opt) - 1;
	      }
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

	      if (opt[1] != '\0')
	      {
		num_options = cupsParseOptions(opt + 1, num_options, &options);
		opt += strlen(opt) - 1;
	      }
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
	      if (opt[1] != '\0')
	      {
		printer = opt + 1;
		opt += strlen(opt) - 1;
	      }
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
		  _cupsLangPrintf(stderr, _("lpoptions: Unable to add printer or instance: %s"), strerror(errno));
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

	      if (opt[1] != '\0')
	      {
		option = opt + 1;
		opt += strlen(opt) - 1;
	      }
	      else
	      {
		i ++;
		if (i >= argc)
		  usage();

		option = argv[i];
	      }

              num_options = cupsRemoveOption(option, num_options, &options);

	      changes = 1;
	      break;

	  case 'x' : /* -x printer */
	      if (opt[1] != '\0')
	      {
		printer = opt + 1;
		opt += strlen(opt) - 1;
	      }
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

              num_dests = cupsRemoveDest(printer, instance, num_dests, &dests);

	      cupsSetDests(num_dests, dests);
	      dest    = NULL;
	      changes = -1;
	      break;

	  default :
	      usage();
	}
      }
    }
    else
    {
      usage();
    }
  }

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
  http_t	*http;			/* Connection to destination */
  char		resource[1024];		/* Resource path */
  int		i;			/* Looping var */
  const char	*filename;		/* PPD filename */
  ppd_file_t	*ppd;			/* PPD data */
  ppd_group_t	*group;			/* Current group */


  if ((http = cupsConnectDest(dest, CUPS_DEST_FLAGS_NONE, 30000, NULL, resource, sizeof(resource), NULL, NULL)) == NULL)
  {
    _cupsLangPrintf(stderr, _("lpoptions: Unable to get PPD file for %s: %s"),
		    dest->name, cupsLastErrorString());
    return;
  }

  if ((filename = cupsGetPPD2(http, dest->name)) == NULL)
  {
    httpClose(http);

    _cupsLangPrintf(stderr, _("lpoptions: Unable to get PPD file for %s: %s"),
		    dest->name, cupsLastErrorString());
    return;
  }

  httpClose(http);

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
  _cupsLangPuts(stdout, _("Usage: lpoptions [options] -d destination\n"
                          "       lpoptions [options] [-p destination] [-l]\n"
                          "       lpoptions [options] [-p destination] -o option[=value]\n"
                          "       lpoptions [options] -x destination"));
  _cupsLangPuts(stdout, _("Options:"));
  _cupsLangPuts(stdout, _("-d destination          Set default destination"));
  _cupsLangPuts(stdout, _("-E                      Encrypt the connection to the server"));
  _cupsLangPuts(stdout, _("-h server[:port]        Connect to the named server and port"));
  _cupsLangPuts(stdout, _("-l                      Show supported options and values"));
  _cupsLangPuts(stdout, _("-o name[=value]         Set default option and value"));
  _cupsLangPuts(stdout, _("-p destination          Specify a destination"));
  _cupsLangPuts(stdout, _("-U username             Specify the username to use for authentication"));
  _cupsLangPuts(stdout, _("-x destination          Remove default options for destination"));

  exit(1);
}
