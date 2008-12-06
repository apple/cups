/*
 * "$Id: hpgl-main.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   HP-GL/2 filter main entry for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1993-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main()          - Main entry for HP-GL/2 filter.
 *   compare_names() - Compare two command names.
 */

/*
 * Include necessary headers...
 */

/*#define DEBUG*/
#define _HPGL_MAIN_C_
#include "hpgltops.h"
#include <cups/i18n.h>


/*
 * HP-GL/2 command table...
 */

typedef struct
{
  char	name[4];			/* Name of command */
  void	(*func)(int, param_t *);	/* Function to call */
} name_t;

static name_t commands[] =
{
  { "BP", BP_begin_plot },
  { "DF", DF_default_values },
  { "IN", IN_initialize },
  { "IP", IP_input_absolute },
  { "IR", IR_input_relative },
  { "IW", IW_input_window },
  { "PG", PG_advance_page },
  { "RO", RO_rotate },
  { "RP", RP_replot },
  { "SC", SC_scale },
  { "AA", AA_arc_absolute },
  { "AR", AR_arc_relative },
  { "AT", AT_arc_absolute3 },
  { "CI", CI_circle },
  { "PA", PA_plot_absolute },
  { "PD", PD_pen_down },
  { "PE", PE_polyline_encoded },
  { "PR", PR_plot_relative },
  { "PS", PS_plot_size },
  { "PU", PU_pen_up },
  { "RT", RT_arc_relative3 },
  { "EA", EA_edge_rect_absolute },
  { "EP", EP_edge_polygon },
  { "ER", ER_edge_rect_relative },
  { "EW", EW_edge_wedge },
  { "FP", FP_fill_polygon },
  { "PM", PM_polygon_mode },
  { "RA", RA_fill_rect_absolute },
  { "RR", RR_fill_rect_relative },
  { "WG", WG_fill_wedge },
  { "AD", AD_define_alternate },
  { "CF", CF_character_fill },
  { "CP", CP_character_plot },
  { "DI", DI_absolute_direction },
  { "DR", DR_relative_direction },
  { "DT", DT_define_label_term },
  { "DV", DV_define_variable_path },
  { "ES", ES_extra_space },
  { "LB", LB_label },
  { "LO", LO_label_origin },
  { "SA", SA_select_alternate },
  { "SD", SD_define_standard },
  { "SI", SI_absolute_size },
  { "SL", SL_character_slant },
  { "SR", SR_relative_size },
  { "SS", SS_select_standard },
  { "TD", TD_transparent_data },
  { "AC", AC_anchor_corner },
  { "FT", FT_fill_type },
  { "LA", LA_line_attributes },
  { "LT", LT_line_type },
  { "NP", NP_number_pens },
  { "PC", PC_pen_color },
  { "CR", CR_color_range },
  { "PW", PW_pen_width },
  { "RF", RF_raster_fill },
  { "SM", SM_symbol_mode },
  { "SP", SP_select_pen },
  { "UL", UL_user_line_type },
  { "WU", WU_width_units }
};
#define NUM_COMMANDS (sizeof(commands) / sizeof(name_t))


/*
 * Local functions...
 */

static int	compare_names(const void *p1, const void *p2);


/*
 * 'main()' - Main entry for HP-GL/2 filter.
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  FILE		*fp;		/* Input file */
  int		num_params;	/* Number of parameters */
  param_t	*params;	/* Command parameters */
  name_t	*command,	/* Command */
		name;		/* Name of command */
  int		num_options;	/* Number of print options */
  cups_option_t	*options;	/* Print options */
  const char	*val;		/* Option value */
  int		shading;	/* -1 = black, 0 = grey, 1 = color */


 /*
  * Make sure status messages are not buffered...
  */

  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

 /*
  * Check command-line...
  */

  if (argc < 6 || argc > 7)
  {
    fprintf(stderr, _("Usage: %s job-id user title copies options [file]\n"),
            argv[0]);
    return (1);
  }

 /*
  * If we have 7 arguments, print the file named on the command-line.
  * Otherwise, send stdin instead...
  */

  if (argc == 6)
    fp = stdin;
  else
  {
   /*
    * Try to open the print file...
    */

    if ((fp = fopen(argv[6], "rb")) == NULL)
    {
      perror("DEBUG: unable to open print file - ");
      return (1);
    }
  }

 /*
  * Process command-line options and write the prolog...
  */

  options     = NULL;
  num_options = cupsParseOptions(argv[5], 0, &options);

  PPD = SetCommonOptions(num_options, options, 1);

  PlotSize[0] = PageWidth;
  PlotSize[1] = PageLength;

  shading  = 1;
  PenWidth = 1.0;

  if ((val = cupsGetOption("blackplot", num_options, options)) != NULL &&
      strcasecmp(val, "no") && strcasecmp(val, "off") &&
      strcasecmp(val, "false"))
    shading = 0;

  if ((val = cupsGetOption("fitplot", num_options, options)) != NULL &&
      !strcasecmp(val, "true"))
    FitPlot = 1;
  else if ((val = cupsGetOption("fit-to-page", num_options, options)) != NULL &&
      !strcasecmp(val, "true"))
    FitPlot = 1;

  if ((val = cupsGetOption("penwidth", num_options, options)) != NULL)
    PenWidth = (float)atoi(val) * 0.001f;

 /*
  * Write the PostScript prolog and initialize the plotting "engine"...
  */

  OutputProlog(argv[3], argv[2], shading);

  IP_input_absolute(0, NULL);

 /*
  * Sort the command array...
  */

  qsort(commands, NUM_COMMANDS, sizeof(name_t),
        (int (*)(const void *, const void *))compare_names);

 /*
  * Read commands until we reach the end of file.
  */

  while ((num_params = ParseCommand(fp, name.name, &params)) >= 0)
  {
    Outputf("%% %s(%d)\n", name.name, num_params);

#ifdef DEBUG
    {
      int i;
      fprintf(stderr, "DEBUG: %s(%d)", name.name, num_params);
      for (i = 0; i < num_params; i ++)
	if (params[i].type == PARAM_STRING)
          fprintf(stderr, " \'%s\'", params[i].value.string);
	else
          fprintf(stderr, " %f", params[i].value.number);
      fputs("\n", stderr);
    }
#endif /* DEBUG */

    if ((command = bsearch(&name, commands, NUM_COMMANDS, sizeof(name_t),
                           (int (*)(const void *, const void *))compare_names)) != NULL)
      (*command->func)(num_params, params);

    FreeParameters(num_params, params);
  }

  OutputTrailer();

  if (fp != stdin)
    fclose(fp);

  return (0);
}


/*
 * 'compare_names()' - Compare two command names.
 */

static int			/* O - Result of strcasecmp() on names */
compare_names(const void *p1,	/* I - First name */
              const void *p2)	/* I - Second name */
{
  return (strcasecmp(((name_t *)p1)->name, ((name_t *)p2)->name));
}


/*
 * End of "$Id: hpgl-main.c 6649 2007-07-11 21:46:42Z mike $".
 */
