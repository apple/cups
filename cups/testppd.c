/*
 * "$Id: testppd.c,v 1.18.2.1 2001/12/26 16:52:13 mike Exp $"
 *
 *   PPD test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2001 by Easy Software Products, all rights reserved.
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
 * Contents:
 *
 *   main() - Main entry for test program.
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include "string.h"


/*
 * 'main()' - Main entry for test program.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		i, j, k, m;	/* Looping vars */
  const char	*filename;	/* File to load */
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

  if (argc == 1)
  {
    fputs("Usage: ppdtest filename1.ppd [... filenameN.ppd]\n", stderr);
    return (1);
  }

  for (i = 1; i < argc; i ++)
  {
    if (strstr(argv[i], ".ppd"))
      filename = argv[i];
    else
      filename = cupsGetPPD(argv[i]);

    if ((ppd = ppdOpenFile(filename)) == NULL)
    {
      fprintf(stderr, "Unable to open \'%s\' as a PPD file!\n", filename);
      continue;
    }

    printf("FILE: %s\n", filename);
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
           ppd->patches == NULL ? 0 : strlen(ppd->patches));

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

    ppdClose(ppd);
  }

  return (0);
}


/*
 * End of "$Id: testppd.c,v 1.18.2.1 2001/12/26 16:52:13 mike Exp $".
 */
