/*
 * "$Id: lpoptions.c,v 1.6 2000/08/29 18:23:33 mike Exp $"
 *
 *   Printer option program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products.
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
 *   main()  - Main entry.
 *   usage() - Show program usage and exit.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <stdlib.h>


/*
 * Local functions...
 */

void	usage(void);


/*
 * 'main()' - Main entry.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		i, j;		/* Looping vars */
  char		server[1024];	/* Print server */
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

  snprintf(server, sizeof(server), "CUPS_SERVER=%s", cupsServer());
  putenv(server);

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
	      num_options = cupsAddOption(dest->options[j].name,
	                                  dest->options[j].value,
	                                  num_options, &options);
	    break;

	case 'h' : /* -h server */
	    if (argv[i][2])
	      snprintf(server, sizeof(server), "CUPS_SERVER=%s", argv[i] + 2);
	    else
	    {
	      i ++;
	      if (i >= argc)
	        usage();

	      snprintf(server, sizeof(server), "CUPS_SERVER=%s", argv[i]);
	    }

            putenv(server);
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
	    num_dests --;

	    j = dest - dests;
	    if (j < num_dests)
	      memcpy(dest, dest + 1, (num_dests - j) * sizeof(cups_dest_t));

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
	break;
      }

  if (dest == NULL)
    return (0);

  if (changes)
  {
   /*
    * Set printer options...
    */

    cupsFreeOptions(dest->num_options, dest->options);

    dest->num_options = num_options;
    dest->options     = options;

    cupsSetDests(num_dests, dests);
  }
  else
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
 * 'usage()' - Show program usage and exit.
 */

void
usage(void)
{
  puts("Usage: lpoptions -d printer");
  puts("       lpoptions -p printer -o option[=value] ...");
  puts("       lpoptions -x printer");

  exit(1);
}


/*
 * End of "$Id: lpoptions.c,v 1.6 2000/08/29 18:23:33 mike Exp $".
 */
