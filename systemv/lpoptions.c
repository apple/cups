/*
 * "$Id: lpoptions.c,v 1.9.2.4 2002/08/18 17:24:28 mike Exp $"
 *
 *   Printer option program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
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
 * Contents:
 *
 *   main()         - Main entry.
 *   list_group()   - List printer-specific options from the PPD group.
 *   list_options() - List printer-specific options from the PPD file.
 *   usage()        - Show program usage and exit.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/string.h>
#include <stdlib.h>


/*
 * Local functions...
 */

void	list_group(ppd_group_t *group);
void	list_options(cups_dest_t *dest);
void	usage(void);


/*
 * 'main()' - Main entry.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		i, j;		/* Looping vars */
  int		changes;	/* Did we make changes? */
  int		num_options;	/* Number of options */
  cups_option_t	*options;	/* Options */
  int		num_dests;	/* Number of destinations */
  cups_dest_t	*dests;		/* Destinations */
  cups_dest_t	*dest;		/* Current destination */
  char		*printer,	/* Printer name */
		*instance,	/* Instance name */ 
 		*option;	/* Current option */


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

            if ((dest = cupsGetDest(printer, instance, num_dests, dests)) == NULL)
	    {
	      fputs("lpoptions: Unknown printer or class!\n", stderr);
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
	      if (cupsGetOption(dest->options[j].name, num_options, options) == NULL)
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

	case 'l' : /* -l (list options) */
            if (dest == NULL)
	    {
	      if (num_dests == 0)
		num_dests = cupsGetDests(&dests);

	      for (i = num_dests, dest = dests; i > 0; i --, dest ++)
	        if (dest->is_default)
		  break;

              if (i == 0)
	        dest = dests;
	    }

            if (dest == NULL)
	      fputs("lpoptions: No printers!?!\n", stderr);
	    else
	      list_options(dest);

            changes = -1;
	    break;

	case 'o' : /* -o option[=value] */
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
	        perror("lpoptions: Unable to add printer or instance");
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
	      if (strcasecmp(options[j].name, option) == 0)
	      {
	       /*
	        * Remove this option...
		*/

	        num_options --;

		if (j < num_options)
		  memcpy(options + j, options + j + 1,
		         sizeof(cups_option_t) * (num_options - j));
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

            if ((dest = cupsGetDest(printer, instance, num_dests, dests)) == NULL)
	    {
	      if (instance)
		fprintf(stderr, "lpoptions: No such printer/instance: %s/%s\n",
		        printer, instance);
	      else
		fprintf(stderr, "lpoptions: No such printer: %s\n", printer);

	      return (1);
	    }

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
		memcpy(dest, dest + 1, (num_dests - j) * sizeof(cups_dest_t));
	    }

	    cupsSetDests(num_dests, dests);
	    dest = NULL;
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
    for (i = 0; i < num_dests; i ++)
      if (dests[i].is_default)
      {
        dest = dests + i;

	for (j = 0; j < dest->num_options; j ++)
	  if (cupsGetOption(dest->options[j].name, num_options, options) == NULL)
	    num_options = cupsAddOption(dest->options[j].name,
	                                dest->options[j].value,
	                                num_options, &options);
	break;
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
    num_options = dest->num_options;
    options     = dest->options;

    for (i = 0; i < num_options; i ++)
    {
      if (i)
        putchar(' ');

      if (!options[i].value[0])
        printf("%s", options[i].name);
      else if (strchr(options[i].value, ' ') != NULL ||
               strchr(options[i].value, '\t') != NULL)
	printf("%s=\'%s\'", options[i].name, options[i].value);
      else
	printf("%s=%s", options[i].name, options[i].value);
    }

    putchar('\n');
  }

  return (0);
}

/*
 * 'list_group()' - List printer-specific options from the PPD group.
 */

void
list_group(ppd_group_t *group)	/* I - Group to show */
{
  int		i, j;		/* Looping vars */
  ppd_option_t	*option;	/* Current option */
  ppd_choice_t	*choice;	/* Current choice */
  ppd_group_t	*subgroup;	/* Current subgroup */


  for (i = group->num_options, option = group->options; i > 0; i --, option ++)
  {
    printf("%s/%s:", option->keyword, option->text);

    for (j = option->num_choices, choice = option->choices; j > 0; j --, choice ++)
      if (choice->marked)
        printf(" *%s", choice->choice);
      else
        printf(" %s", choice->choice);

    putchar('\n');
  }

  for (i = group->num_subgroups, subgroup = group->subgroups; i > 0; i --, subgroup ++)
    list_group(subgroup);
}


/*
 * 'list_options()' - List printer-specific options from the PPD file.
 */

void
list_options(cups_dest_t *dest)	/* I - Destination to list */
{
  int		i;		/* Looping var */
  const char	*filename;	/* PPD filename */
  ppd_file_t	*ppd;		/* PPD data */
  ppd_group_t	*group;		/* Current group */


  if ((filename = cupsGetPPD(dest->name)) == NULL)
  {
    fprintf(stderr, "lpoptions: Destination %s has no PPD file!\n", dest->name);
    return;
  }

  if ((ppd = ppdOpenFile(filename)) == NULL)
  {
    unlink(filename);
    fprintf(stderr, "lpoptions: Unable to open PPD file for %s!\n", dest->name);
    return;
  }

  ppdMarkDefaults(ppd);
  cupsMarkOptions(ppd, dest->num_options, dest->options);

  for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
    list_group(group);

  ppdClose(ppd);
  unlink(filename);
}


/*
 * 'usage()' - Show program usage and exit.
 */

void
usage(void)
{
  puts("Usage: lpoptions -d printer");
  puts("       lpoptions [-p printer] -l");
  puts("       lpoptions -p printer -o option[=value] ...");
  puts("       lpoptions -x printer");

  exit(1);
}


/*
 * End of "$Id: lpoptions.c,v 1.9.2.4 2002/08/18 17:24:28 mike Exp $".
 */
