/*
 * "$Id: testppd.c,v 1.2 1998/06/12 20:33:20 mike Exp $"
 *
 *   Test program for the PostScript Printer Description (PPD) file library.
 *
 *   Copyright 1997-1998 by Easy Software Products.
 *
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
 *   This library is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU Library General Public License as published
 *   by the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 *   USA.
 *
 * Contents:
 *
 *   main() - Main entry for test program.
 *
 * Revision History:
 *
 *   $Log: testppd.c,v $
 *   Revision 1.2  1998/06/12 20:33:20  mike
 *   First working version.
 *
 *   Revision 1.1  1998/06/12 14:28:48  mike
 *   Initial revision
 */

/*
 * Include necessary headers...
 */

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
      printf("        jcls[%d] = %s (%s)\n", j, option->keyword, option->text);

      for (k = option->num_choices, choice = option->choices;
	   k > 0;
	   k --, choice ++)
	printf("            %s (%s)\n", choice->option, choice->text);
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

    printf("    num_options = %d\n", ppd->num_options);
    for (j = 0, option = ppd->options; j < ppd->num_options; j ++, option ++)
    {
      printf("        option[%d] = %s (%s)\n", j, option->keyword, option->text);

      if (strcmp(option->keyword, "PageSize") == 0 ||
          strcmp(option->keyword, "PageRegion") == 0)
      {
        for (k = option->num_choices, choice = option->choices;
	     k > 0;
	     k --, choice ++)
	{
	  size = ppdPageSize(ppd, choice->option);
	  if (size == NULL)
	    printf("            %s (%s) = ERROR\n", choice->option, choice->text);
          else
	    printf("            %s (%s) = %.2fx%.2fin\n", choice->option,
	           choice->text, size->width / 72.0, size->length / 72.0);
        };
      }
      else
      {
        for (k = option->num_choices, choice = option->choices;
	     k > 0;
	     k --, choice ++)
	  printf("            %s (%s)\n", choice->option, choice->text);
      };
    };

    printf("    num_groups = %d\n", ppd->num_groups);
    for (j = 0, group = ppd->groups; j < ppd->num_groups; j ++, group ++)
    {
      printf("        group[%d] = %s\n", j, group->text);

      for (k = 0, option = group->options; k < group->num_options; k ++, option ++)
      {
	printf("            options[%d] = %s (%s)\n", k, option->keyword, option->text);

	for (m = option->num_choices, choice = option->choices;
	     m > 0;
	     m --, choice ++)
	  printf("                %s (%s)\n", choice->option, choice->text);
      };
    };

    ppdClose(ppd);
  };

  return (0);
}



/*
 * End of "$Id: testppd.c,v 1.2 1998/06/12 20:33:20 mike Exp $".
 */
