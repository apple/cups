/*
 * "$Id: testppd.c,v 1.4 1999/01/27 18:31:57 mike Exp $"
 *
 *   PPD test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   PostScript is a trademark of Adobe Systems, Inc.
 *
 * Contents:
 *
 *   main() - Main entry for test program.
 */

/*
 * Include necessary headers...
 */

#include <config.h>
#include "ppd.h"


/*
 * 'main()' - Main entry for test program.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		i, j, k, m;	/* Looping vars */
  ppd_file_t	*ppd;		/* PPD file record */
  ppd_size_t	*size;		/* Size record */
  ppd_group_t	*group;		/* UI group */
  ppd_option_t	*option;	/* Standard UI option */
  ppd_choice_t	*choice;	/* Standard UI option choice */
  static char	*uis[] = { "PPD_UI_BOOLEAN", "PPD_UI_PICKONE", "PPD_UI_PICKMANY" };
  static char	*sections[] = { "PPD_ORDER_ANY", "PPD_ORDER_DOCUMENT", "PPD_ORDER_EXIT",
                                "PPD_ORDER_JCL", "PPD_ORDER_PAGE", "PPD_ORDER_PROLOG" };


 /*
  * Display PPD files for each file listed on the command-line...
  */

  if (argc == 1)
  {
    fputs("Usage: ppdtest filename1.ppd [... filenameN.ppd]\n", stderr);
    return (1);
  };

  for (i = 1; i < argc; i ++)
  {
    if ((ppd = ppdOpenFile(argv[i])) == NULL)
    {
      fprintf(stderr, "Unable to open \'%s\' as a PPD file!\n", argv[i]);
      continue;
    };

    printf("FILE: %s\n", argv[i]);
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
    };

    printf("    num_emulations = %d\n", ppd->num_emulations);
    for (j = 0; j < ppd->num_emulations; j ++)
      printf("        emulations[%d] = %s\n", j, ppd->emulations[j].name);

    printf("    num_jcls = %d\n", ppd->num_jcls);
    for (j = 0, option = ppd->jcls; j < ppd->num_jcls; j ++, option ++)
    {
      printf("        jcls[%d] = %s (%s) %s %s %.0f\n", j, option->keyword, option->text,
             uis[option->ui], sections[option->section], option->order);

      for (k = option->num_choices, choice = option->choices;
	   k > 0;
	   k --, choice ++)
	printf("            %s (%s)\n", choice->choice, choice->text);
    };

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
           ppd->patches == NULL ? 0 : strlen((char *)ppd->patches));

    printf("    num_options = %d\n", ppd->num_options);
    for (j = 0, option = ppd->options; j < ppd->num_options; j ++, option ++)
    {
      printf("        option[%d] = %s (%s) %s %s %.0f\n", j, option->keyword,
             option->text, uis[option->ui], sections[option->section], option->order);

      if (strcmp(option->keyword, "PageSize") == 0 ||
          strcmp(option->keyword, "PageRegion") == 0)
      {
        for (k = option->num_choices, choice = option->choices;
	     k > 0;
	     k --, choice ++)
	{
	  size = ppdPageSize(ppd, choice->choice);
	  if (size == NULL)
	    printf("            %s (%s) = ERROR\n", choice->choice, choice->text);
          else
	    printf("            %s (%s) = %.2fx%.2fin\n", choice->choice,
	           choice->text, size->width / 72.0, size->length / 72.0);
        };
      }
      else
      {
        for (k = option->num_choices, choice = option->choices;
	     k > 0;
	     k --, choice ++)
	  printf("            %s (%s)\n", choice->choice, choice->text);
      };
    };

    printf("    num_groups = %d\n", ppd->num_groups);
    for (j = 0, group = ppd->groups; j < ppd->num_groups; j ++, group ++)
    {
      printf("        group[%d] = %s\n", j, group->text);

      for (k = 0, option = group->options; k < group->num_options; k ++, option ++)
      {
	printf("            options[%d] = %s (%s) %s %s %.0f\n", k, option->keyword,
	       option->text, uis[option->ui], sections[option->section], option->order);

	for (m = option->num_choices, choice = option->choices;
	     m > 0;
	     m --, choice ++)
	  printf("                %s (%s)\n", choice->choice, choice->text);
      };
    };

    ppdClose(ppd);
  };

  return (0);
}


/*
 * End of "$Id: testppd.c,v 1.4 1999/01/27 18:31:57 mike Exp $".
 */
