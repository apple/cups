/*
 * "$Id: cupstestppd.c,v 1.1.2.4 2003/01/29 01:40:39 mike Exp $"
 *
 *   PPD test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products, all rights reserved.
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
 *   main() - Main entry for test program.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/string.h>
#include <errno.h>


/*
 * Error codes...
 */

#define ERROR_NONE		0
#define ERROR_USAGE		1
#define ERROR_FILE_OPEN		2
#define ERROR_PPD_FORMAT	3
#define ERROR_CONFORMANCE	4


/*
 * 'main()' - Main entry for test program.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		i, j, k, m;	/* Looping vars */
  int		verbose;	/* Want verbose output? */
  int		status;		/* Exit status */
  int		errors;		/* Number of conformance errors */
  ppd_file_t	*ppd;		/* PPD file record */
  ppd_size_t	*size;		/* Size record */
  ppd_group_t	*group;		/* UI group */
  ppd_option_t	*option;	/* Standard UI option */
  ppd_choice_t	*choice;	/* Standard UI option choice */
  static char	*uis[] = { "BOOLEAN", "PICKONE", "PICKMANY" };
  static char	*sections[] = { "ANY", "DOCUMENT", "EXIT",
                                "JCL", "PAGE", "PROLOG" };


  setbuf(stdout, NULL);

 /*
  * Display PPD files for each file listed on the command-line...
  */

  verbose = 0;
  ppd     = NULL;
  status  = ERROR_NONE;

  for (i = 1; i < argc; i ++)
    if (strcmp(argv[i], "-q") == 0)
    {
      if (verbose > 0)
      {
        fputs("cupstestppd: The -q option is incompatible with the -v option.\n",
	      stderr);
	return (1);
      }

      verbose --;
    }
    else if (strcmp(argv[i], "-v") == 0)
    {
      if (verbose < 0)
      {
        fputs("cupstestppd: The -v option is incompatible with the -q option.\n",
	      stderr);
	return (1);
      }

      verbose ++;
    }
    else if (argv[i][0] == '-' && argv[i][1])
    {
      ppd = NULL;
      break;
    }
    else
    {
     /*
      * Open the PPD file...
      */

      if (argv[i][0] == '-')
      {
       /*
        * Read from stdin...
	*/

        if (verbose >= 0)
          puts("FILE: (stdin)\n");

        ppd = ppdOpen(stdin);
      }
      else
      {
       /*
        * Read from a file...
	*/

        if (verbose >= 0)
          printf("FILE: %s\n\n", argv[i]);

        ppd = ppdOpenFile(argv[i]);
      }

      if (ppd == NULL)
      {
        if (errno)
	{
	  status = ERROR_FILE_OPEN;

          if (verbose >= 0)
	    printf("    Unable to open PPD file - %s\n\n", strerror(errno));
	}
	else
	{
	  status = ERROR_PPD_FORMAT;

          if (verbose >= 0)
	    puts("    Unable to open PPD file using CUPS functions!\n");
        }

	continue;
      }

     /*
      * Show the header and then perform basic conformance tests (limited
      * only by what the CUPS PPD functions actually load...)
      */

      if (verbose >= 0)
        puts("    CONFORMANCE TESTS:");

      errors = 0;

      for (j = 0, group = ppd->groups; j < ppd->num_groups; j ++, group ++)
	for (k = 0, option = group->options; k < group->num_options; k ++, option ++)
	  if (option->defchoice[0])
	  {
	    if (verbose >= 0)
	      printf("        PASS    Default%s\n", option->keyword);
	  }
	  else
	  {
	    errors ++;

	    if (verbose >= 0)
	      printf("      **FAIL**  REQUIRED Default%s\n", option->keyword);
	  }

      if (ppd->num_fonts)
      {
        if (verbose >= 0)
	  puts("        PASS    Fonts");
      }
      else
      {
	errors ++;

	if (verbose >= 0)
	  puts("      **FAIL**  REQUIRED Fonts");
      }

      if (ppd->lang_encoding != NULL)
      {
	if (verbose >= 0)
	  puts("        PASS    LanguageEncoding");
      }
      else
      {
	errors ++;

	if (verbose >= 0)
	  puts("      **FAIL**  REQUIRED LanguageEncoding");
      }

      if (ppd->lang_version != NULL)
      {
	if (verbose >= 0)
	  puts("        PASS    LanguageVersion");
      }
      else
      {
	errors ++;

	if (verbose >= 0)
	  puts("      **FAIL**  REQUIRED LanguageVersion");
      }

      if (ppd->manufacturer != NULL)
      {
	if (verbose >= 0)
	  puts("        PASS    Manufacturer");
      }
      else
      {
	errors ++;

	if (verbose >= 0)
	  puts("      **FAIL**  REQUIRED Manufacturer");
      }

      if (ppd->modelname != NULL)
      {
	if (verbose >= 0)
	  puts("        PASS    ModelName");
      }
      else
      {
	errors ++;

	if (verbose >= 0)
	  puts("      **FAIL**  REQUIRED ModelName");
      }

      if (ppd->nickname != NULL)
      {
	if (verbose >= 0)
	  puts("        PASS    NickName");
      }
      else
      {
	errors ++;

	if (verbose >= 0)
	  puts("      **FAIL**  REQUIRED NickName");
      }

      if (ppdFindOption(ppd, "PageSize") != NULL)
      {
	if (verbose >= 0)
	  puts("        PASS    PageSize");
      }
      else
      {
	errors ++;

	if (verbose >= 0)
	  puts("      **FAIL**  REQUIRED PageSize");
      }

      if (ppdFindOption(ppd, "PageRegion") != NULL)
      {
	if (verbose >= 0)
	  puts("        PASS    PageRegion");
      }
      else
      {
	errors ++;

	if (verbose >= 0)
	  puts("      **FAIL**  REQUIRED PageRegion");
      }

      if (ppd->product != NULL)
      {
	if (verbose >= 0)
	  puts("        PASS    Product");
      }
      else
      {
	errors ++;

	if (verbose >= 0)
	  puts("      **FAIL**  REQUIRED Product");
      }

      if (ppd->shortnickname != NULL)
      {
	if (verbose >= 0)
	  puts("        PASS    ShortNickName");
      }
      else
      {
	errors ++;

	if (verbose >= 0)
	  puts("      **FAIL**  REQUIRED ShortNickName");
      }

      if (errors)
      {
        if (verbose >= 0)
	  puts("\n    **** CONFORMANCE TESTING FAILED ****");

	status = ERROR_CONFORMANCE;
      }

      if (verbose >= 0)
	puts("");

     /*
      * Then list the options, if "-v" was provided...
      */ 

      if (verbose > 0)
      {
	puts("");
	puts("    OPTIONS:");     
	printf("    language_level = %d\n", ppd->language_level);
	printf("    color_device = %s\n", ppd->color_device ? "TRUE" : "FALSE");
	printf("    variable_sizes = %s\n", ppd->variable_sizes ? "TRUE" : "FALSE");
	printf("    landscape = %d\n", ppd->landscape);

	switch (ppd->colorspace)
	{
	  case PPD_CS_CMYK :
              puts("    colorspace = PPD_CS_CMYK");
	      break;
	  case PPD_CS_CMY :
              puts("    colorspace = PPD_CS_CMY");
	      break;
	  case PPD_CS_GRAY :
              puts("    colorspace = PPD_CS_GRAY");
	      break;
	  case PPD_CS_RGB :
              puts("    colorspace = PPD_CS_RGB");
	      break;
	  default :
              puts("    colorspace = <unknown>");
	      break;
	}

	printf("    num_emulations = %d\n", ppd->num_emulations);
	for (j = 0; j < ppd->num_emulations; j ++)
	  printf("        emulations[%d] = %s\n", j, ppd->emulations[j].name);

	printf("    lang_encoding = %s\n", ppd->lang_encoding);
	printf("    lang_version = %s\n", ppd->lang_version);
	printf("    modelname = %s\n", ppd->modelname);
	printf("    ttrasterizer = %s\n",
               ppd->ttrasterizer == NULL ? "None" : ppd->ttrasterizer);
	printf("    manufacturer = %s\n", ppd->manufacturer);
	printf("    product = %s\n", ppd->product);
	printf("    nickname = %s\n", ppd->nickname);
	printf("    shortnickname = %s\n", ppd->shortnickname);
	printf("    patches = %d bytes\n",
               ppd->patches == NULL ? 0 : (int)strlen(ppd->patches));

	printf("    num_groups = %d\n", ppd->num_groups);
	for (j = 0, group = ppd->groups; j < ppd->num_groups; j ++, group ++)
	{
	  printf("        group[%d] = %s\n", j, group->text);

	  for (k = 0, option = group->options; k < group->num_options; k ++, option ++)
	  {
	    printf("            options[%d] = %s (%s) %s %s %.0f (%d choices)\n", k,
		   option->keyword, option->text, uis[option->ui],
		   sections[option->section], option->order,
		   option->num_choices);

            if (strcmp(option->keyword, "PageSize") == 0 ||
        	strcmp(option->keyword, "PageRegion") == 0)
            {
              for (m = option->num_choices, choice = option->choices;
		   m > 0;
		   m --, choice ++)
	      {
		size = ppdPageSize(ppd, choice->choice);

		if (size == NULL)
		  printf("                %s (%s) = ERROR", choice->choice, choice->text);
        	else
		  printf("                %s (%s) = %.2fx%.2fin (%.1f,%.1f,%.1f,%.1f)", choice->choice,
	        	 choice->text, size->width / 72.0, size->length / 72.0,
			 size->left / 72.0, size->bottom / 72.0,
			 size->right / 72.0, size->top / 72.0);

        	if (strcmp(option->defchoice, choice->choice) == 0)
		  puts(" *");
		else
		  putchar('\n');
              }
	    }
	    else
	    {
	      for (m = option->num_choices, choice = option->choices;
		   m > 0;
		   m --, choice ++)
	      {
		printf("                %s (%s)", choice->choice, choice->text);

        	if (strcmp(option->defchoice, choice->choice) == 0)
		  puts(" *");
		else
		  putchar('\n');
	      }
            }
	  }
	}

	printf("    num_profiles = %d\n", ppd->num_profiles);
	for (j = 0; j < ppd->num_profiles; j ++)
	  printf("        profiles[%d] = %s/%s %.3f %.3f [ %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f ]\n",
        	 j, ppd->profiles[j].resolution, ppd->profiles[j].media_type,
		 ppd->profiles[j].gamma, ppd->profiles[j].density,
		 ppd->profiles[j].matrix[0][0], ppd->profiles[j].matrix[0][1],
		 ppd->profiles[j].matrix[0][2], ppd->profiles[j].matrix[1][0],
		 ppd->profiles[j].matrix[1][1], ppd->profiles[j].matrix[1][2],
		 ppd->profiles[j].matrix[2][0], ppd->profiles[j].matrix[2][1],
		 ppd->profiles[j].matrix[2][2]);

	printf("    num_fonts = %d\n", ppd->num_fonts);
	for (j = 0; j < ppd->num_fonts; j ++)
	  printf("        fonts[%d] = %s\n", j, ppd->fonts[j]);
      }

      ppdClose(ppd);
    }

  if (!ppd && verbose >= 0)
  {
    puts("Usage: cupstestppd [-q] [-v] filename1.ppd [... filenameN.ppd]");
    puts("       program | cupstestppd [-q] [-v] -");

    return (ERROR_USAGE);
  }
  else
    return (status);
}


/*
 * End of "$Id: cupstestppd.c,v 1.1.2.4 2003/01/29 01:40:39 mike Exp $".
 */
