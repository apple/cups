/*
 * "$Id: hpgl-main.c,v 1.11 1999/03/23 18:39:05 mike Exp $"
 *
 *   HP-GL/2 filter main entry for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-1999 by Easy Software Products.
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
 *   main()          - Main entry for HP-GL/2 filter.
 *   compare_names() - Compare two command names.
 */

/*
 * Include necessary headers...
 */

/*#define DEBUG*/
#define _HPGL_MAIN_C_
#include "hpgltops.h"


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
  float		temp;		/* Swapping variable */
  int		num_params;	/* Number of parameters */
  param_t	*params;	/* Command parameters */
  name_t	*command,	/* Command */
		name;		/* Name of command */
  ppd_file_t	*ppd;		/* PPD file */
  ppd_size_t	*pagesize;	/* Page size */
  int		num_options;	/* Number of print options */
  cups_option_t	*options;	/* Print options */
  char		*val;		/* Option value */
  int		shading;	/* -1 = black, 0 = grey, 1 = color */
  float		penwidth;	/* Default pen width */


  if (argc < 6 || argc > 7)
  {
    fputs("ERROR: hpgltops job-id user title copies options [file]\n", stderr);
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
      perror("ERROR: unable to open print file - ");
      return (1);
    }
  }

 /*
  * Process command-line options and write the prolog...
  */

  ppd = ppdOpenFile(getenv("PPD"));

  options     = NULL;
  num_options = cupsParseOptions(argv[5], 0, &options);

  ppdMarkDefaults(ppd);
  cupsMarkOptions(ppd, num_options, options);

  if ((pagesize = ppdPageSize(ppd, NULL)) != NULL)
  {
    PageWidth  = pagesize->width;
    PageLength = pagesize->length;
    PageTop    = pagesize->top;
    PageBottom = pagesize->bottom;
    PageLeft   = pagesize->left;
    PageRight  = pagesize->right;
  }

  LanguageLevel = ppd->language_level;
  ColorDevice   = ppd->color_device;

  ppdClose(ppd);

  shading  = 1;
  penwidth = 1.0;

  if ((val = cupsGetOption("blackplot", num_options, options)) != NULL)
    shading = 0;

  if ((val = cupsGetOption("fitplot", num_options, options)) != NULL)
    FitPlot = 1;

  if ((val = cupsGetOption("penwidth", num_options, options)) != NULL)
    penwidth = (float)atof(val);

  if ((val = cupsGetOption("sides", num_options, options)) != NULL &&
      strncmp(val, "two-", 4) == 0)
    Duplex = 1;

  if ((val = cupsGetOption("Duplex", num_options, options)) != NULL &&
      strcmp(val, "NoTumble") == 0)
    Duplex = 1;

  if ((val = cupsGetOption("landscape", num_options, options)) != NULL)
    Orientation = 1;

  if ((val = cupsGetOption("orientation-requested", num_options, options)) != NULL)
  {
   /*
    * Map IPP orientation values to 0 to 3:
    *
    *   3 = 0 degrees   = 0
    *   4 = 90 degrees  = 1
    *   5 = -90 degrees = 3
    *   6 = 180 degrees = 2
    */

    Orientation = atoi(val) - 3;
    if (Orientation >= 2)
      Orientation ^= 1;
  }

  if ((val = cupsGetOption("page-left", num_options, options)) != NULL)
  {
    switch (Orientation)
    {
      case 0 :
          PageLeft = (float)atof(val);
	  break;
      case 1 :
          PageBottom = (float)atof(val);
	  break;
      case 2 :
          PageRight = PageWidth - (float)atof(val);
	  break;
      case 3 :
          PageTop = PageLength - (float)atof(val);
	  break;
    }
  }

  if ((val = cupsGetOption("page-right", num_options, options)) != NULL)
  {
    switch (Orientation)
    {
      case 0 :
          PageRight = PageWidth - (float)atof(val);
	  break;
      case 1 :
          PageTop = PageLength - (float)atof(val);
	  break;
      case 2 :
          PageLeft = (float)atof(val);
	  break;
      case 3 :
          PageBottom = (float)atof(val);
	  break;
    }
  }

  if ((val = cupsGetOption("page-bottom", num_options, options)) != NULL)
  {
    switch (Orientation)
    {
      case 0 :
          PageBottom = (float)atof(val);
	  break;
      case 1 :
          PageRight = PageWidth - (float)atof(val);
	  break;
      case 2 :
          PageTop = PageLength - (float)atof(val);
	  break;
      case 3 :
          PageLeft = (float)atof(val);
	  break;
    }
  }

  if ((val = cupsGetOption("page-top", num_options, options)) != NULL)
  {
    switch (Orientation)
    {
      case 0 :
          PageTop = PageLength - (float)atof(val);
	  break;
      case 1 :
          PageLeft = (float)atof(val);
	  break;
      case 2 :
          PageBottom = (float)atof(val);
	  break;
      case 3 :
          PageRight = PageWidth - (float)atof(val);
	  break;
    }
  }

  switch (Orientation)
  {
    case 0 : /* Portait */
        break;

    case 1 : /* Landscape */
	temp       = PageLeft;
	PageLeft   = PageBottom;
	PageBottom = temp;

	temp       = PageRight;
	PageRight  = PageTop;
	PageTop    = temp;

	temp       = PageWidth;
	PageWidth  = PageLength;
	PageLength = temp;
	break;

    case 2 : /* Reverse Portrait */
	temp       = PageWidth - PageLeft;
	PageLeft   = PageWidth - PageRight;
	PageRight  = temp;

	temp       = PageLength - PageBottom;
	PageBottom = PageLength - PageTop;
	PageTop    = temp;
        break;

    case 3 : /* Reverse Landscape */
	temp       = PageWidth - PageLeft;
	PageLeft   = PageWidth - PageRight;
	PageRight  = temp;

	temp       = PageLength - PageBottom;
	PageBottom = PageLength - PageTop;
	PageTop    = temp;

	temp       = PageLeft;
	PageLeft   = PageBottom;
	PageBottom = temp;

	temp       = PageRight;
	PageRight  = PageTop;
	PageTop    = temp;

	temp       = PageWidth;
	PageWidth  = PageLength;
	PageLength = temp;
	break;
  }

 /*
  * Write the PostScript prolog and initialize the plotting "engine"...
  */

  OutputProlog(argv[3], argv[2], shading, penwidth);

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
 * End of "$Id: hpgl-main.c,v 1.11 1999/03/23 18:39:05 mike Exp $".
 */
