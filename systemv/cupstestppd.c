/*
 * "$Id: cupstestppd.c,v 1.1.2.9 2003/02/14 21:24:12 mike Exp $"
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
 *   main()  - Main entry for test program.
 *   usage() - Show program usage...
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/string.h>
#include <errno.h>
#include <stdlib.h>


/*
 * Error codes...
 */

#define ERROR_NONE		0
#define ERROR_USAGE		1
#define ERROR_FILE_OPEN		2
#define ERROR_PPD_FORMAT	3
#define ERROR_CONFORMANCE	4


/*
 * Local functions...
 */

void	usage(void);


/*
 * 'main()' - Main entry for test program.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		i, j, k, m;	/* Looping vars */
  char		*opt;		/* Option character */
  const char	*ptr;		/* Pointer into string */
  int		files;		/* Number of files */
  int		verbose;	/* Want verbose output? */
  int		status;		/* Exit status */
  int		errors;		/* Number of conformance errors */
  int		ppdversion;	/* PPD spec version in PPD file */
  ppd_status_t	error;		/* Status of ppdOpen*() */
  int		line;		/* Line number for error */
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
  files   = 0;
  status  = ERROR_NONE;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-' && argv[i][1])
    {
      for (opt = argv[i] + 1; *opt; opt ++)
        switch (*opt)
	{
	  case 'q' :
	      if (verbose > 0)
	      {
        	fputs("cupstestppd: The -q option is incompatible with the -v option.\n",
		      stderr);
		return (1);
	      }

	      verbose --;
	      break;

	  case 'v' :
	      if (verbose < 0)
	      {
        	fputs("cupstestppd: The -v option is incompatible with the -q option.\n",
		      stderr);
		return (1);
	      }

	      verbose ++;
	      break;

	  default :
	      usage();
	      break;
	}
    }
    else
    {
     /*
      * Open the PPD file...
      */

      files ++;

      if (argv[i][0] == '-')
      {
       /*
        * Read from stdin...
	*/

        if (verbose >= 0)
          printf("(stdin):");

        ppd = ppdOpen(stdin);
      }
      else if (strlen(argv[i]) > 3 &&
               !strcmp(argv[i] + strlen(argv[i]) - 3, ".gz"))
      {
       /*
        * Read from a gzipped file...
	*/

        char	command[1024];	/* Command string */
	FILE	*gunzip;	/* Pipe file */


        if (verbose >= 0)
          printf("%s:", argv[i]);

        snprintf(command, sizeof(command), "gunzip -c %s", argv[i]);
	gunzip = popen(command, "r");
	ppd    = ppdOpen(gunzip);

	if (gunzip != NULL)
	  pclose(gunzip);
      }
      else
      {
       /*
        * Read from a file...
	*/

        if (verbose >= 0)
          printf("%s:", argv[i]);

        ppd = ppdOpenFile(argv[i]);
      }

      if (ppd == NULL)
      {
        error = ppdLastError(&line);

	if (error <= PPD_NULL_FILE)
	{
	  status = ERROR_FILE_OPEN;

          if (verbose == 0)
	    puts(" ERROR");
	  else if (verbose > 0)
	    printf("\n    Unable to open PPD file - %s\n\n", strerror(errno));
	}
	else
	{
	  status = ERROR_PPD_FORMAT;

          if (verbose == 0)
	    puts(" FAIL");
          else if (verbose > 0)
	    printf("\n    Unable to open PPD file - %s on line %d.\n",
	           ppdErrorString(error), line);
        }

	continue;
      }

     /*
      * Show the header and then perform basic conformance tests (limited
      * only by what the CUPS PPD functions actually load...)
      */

      if (verbose > 0)
        puts("\n    CONFORMANCE TESTS:");

      errors     = 0;
      ppdversion = 43;
      
      if ((ptr = ppdFindAttr(ppd, "FormatVersion", NULL)) != NULL)
      {
        ppdversion = (int)(10 * atof(ptr) + 0.5);

	if (ppdversion < 43 && verbose > 0)
	  printf("        WARN    Obsolete PPD version %s!\n", ptr);
      }

      if (ppdFindAttr(ppd, "DefaultImageableArea", NULL) != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    DefaultImageableArea");
      }
      else
      {
	errors ++;

	if (verbose > 0)
	  puts("      **FAIL**  REQUIRED DefaultImageableArea");
      }

      if (ppdFindAttr(ppd, "DefaultPaperDimension", NULL) != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    DefaultPaperDimension");
      }
      else
      {
	errors ++;

	if (verbose > 0)
	  puts("      **FAIL**  REQUIRED DefaultPaperDimension");
      }

      for (j = 0, group = ppd->groups; j < ppd->num_groups; j ++, group ++)
	for (k = 0, option = group->options; k < group->num_options; k ++, option ++)
	  if (option->defchoice[0])
	  {
	    if (verbose > 0)
	      printf("        PASS    Default%s\n", option->keyword);
	  }
	  else
	  {
	    errors ++;

	    if (verbose > 0)
	      printf("      **FAIL**  REQUIRED Default%s\n", option->keyword);
	  }

      if (ppdFindAttr(ppd, "FileVersion", NULL) != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    FileVersion");
      }
      else
      {
	errors ++;

	if (verbose > 0)
	  puts("      **FAIL**  REQUIRED FileVersion");
      }

      if (ppdFindAttr(ppd, "FormatVersion", NULL) != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    FormatVersion");
      }
      else
      {
	errors ++;

	if (verbose > 0)
	  puts("      **FAIL**  REQUIRED FormatVersion");
      }

      if (ppd->lang_encoding != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    LanguageEncoding");
      }
      else if (ppdversion < 41)
      {
	if (verbose > 0)
	  puts("        WARN    REQUIRED LanguageEncoding");
      }
      else
      {
	errors ++;

	if (verbose > 0)
	  puts("      **FAIL**  REQUIRED LanguageEncoding");
      }

      if (ppd->lang_version != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    LanguageVersion");
      }
      else
      {
	errors ++;

	if (verbose > 0)
	  puts("      **FAIL**  REQUIRED LanguageVersion");
      }

      if (ppd->manufacturer != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    Manufacturer");
      }
      else if (ppdversion < 43)
      {
	if (verbose > 0)
	  puts("        WARN    REQUIRED Manufacturer");
      }
      else
      {
	errors ++;

	if (verbose > 0)
	  puts("      **FAIL**  REQUIRED Manufacturer");
      }

      if (ppd->modelname != NULL)
      {
        for (ptr = ppd->modelname; *ptr; ptr ++)
	  if (!isalnum(*ptr) && !strchr(" ./-+", *ptr))
	    break;

	if (*ptr)
	{
	  errors ++;

	  if (verbose > 0)
	    puts("      **FAIL**  BAD ModelName");
	}
	else if (verbose > 0)
	  puts("        PASS    ModelName");
      }
      else
      {
	errors ++;

	if (verbose > 0)
	  puts("      **FAIL**  REQUIRED ModelName");
      }

      if (ppd->nickname != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    NickName");
      }
      else
      {
	errors ++;

	if (verbose > 0)
	  puts("      **FAIL**  REQUIRED NickName");
      }

      if (ppdFindOption(ppd, "PageSize") != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    PageSize");
      }
      else
      {
	errors ++;

	if (verbose > 0)
	  puts("      **FAIL**  REQUIRED PageSize");
      }

      if (ppdFindOption(ppd, "PageRegion") != NULL)
      {
	if (verbose > 0)
	  puts("        PASS    PageRegion");
      }
      else
      {
	errors ++;

	if (verbose > 0)
	  puts("      **FAIL**  REQUIRED PageRegion");
      }

      if (ppd->pcfilename != NULL)
      {
	if (verbose > 0)
	{
	 /*
	  * Treat a PCFileName attribute longer than 12 characters as
	  * a warning and not a hard error...
	  */

	  if (strlen(ppd->pcfilename) > 12)
	    puts("        PASS    WARNING-TOO-LONG PCFileName");
	  else
	    puts("        PASS    PCFileName");
	}
      }
      else
      {
	errors ++;

	if (verbose > 0)
	  puts("      **FAIL**  REQUIRED PCFileName");
      }

      if (ppd->product != NULL)
      {
        if (ppd->product[0] != '(' ||
	    ppd->product[strlen(ppd->product) - 1] != ')')
	{
	  errors ++;

	  if (verbose > 0)
	    puts("      **FAIL**  BAD Product");
	}
	else if (verbose > 0)
	  puts("        PASS    Product");
      }
      else
      {
	errors ++;

	if (verbose > 0)
	  puts("      **FAIL**  REQUIRED Product");
      }

      if ((ptr = ppdFindAttr(ppd, "PSVersion", NULL)) != NULL)
      {
        char	junkstr[255];			/* Temp string */
	int	junkint;			/* Temp integer */


        if (sscanf(ptr, "(%[^)])%d", junkstr, &junkint) != 2)
	{
	  errors ++;

	  if (verbose > 0)
	    puts("      **FAIL**  BAD PSVersion");
	}
	else if (verbose > 0)
	  puts("        PASS    PSVersion");
      }
      else
      {
	errors ++;

	if (verbose > 0)
	  puts("      **FAIL**  REQUIRED PSVersion");
      }

      if (ppd->shortnickname != NULL)
      {
        if (strlen(ppd->shortnickname) > 31)
	{
	  errors ++;

	  if (verbose > 0)
	    puts("      **FAIL**  BAD ShortNickName");
	}
	else if (verbose > 0)
	  puts("        PASS    ShortNickName");
      }
      else if (ppdversion < 43)
      {
	if (verbose > 0)
	  puts("        WARN    REQUIRED ShortNickName");
      }
      else
      {
	errors ++;

	if (verbose > 0)
	  puts("      **FAIL**  REQUIRED ShortNickName");
      }

      if (errors)
      {
        if (verbose > 0)
	  puts("\n    **** CONFORMANCE TESTING FAILED ****");

	status = ERROR_CONFORMANCE;
      }

      if (verbose > 0)
	puts("");

     /*
      * Then list the options, if "-v" was provided...
      */ 

      if (verbose > 1)
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

      if (verbose == 0)
      {
        if (errors)
	  puts(" FAIL");
	else
	  puts(" PASS");
      }

      ppdClose(ppd);
    }

  if (!files)
    usage();

  return (status);
}


/*
 * 'usage()' - Show program usage...
 */

void
usage(void)
{
  puts("Usage: cupstestppd [-q] [-v[v]] filename1.ppd[.gz] [... filenameN.ppd[.gz]]");
  puts("       program | cupstestppd [-q] [-v[v]] -");

  exit(ERROR_USAGE);
}


/*
 * End of "$Id: cupstestppd.c,v 1.1.2.9 2003/02/14 21:24:12 mike Exp $".
 */
