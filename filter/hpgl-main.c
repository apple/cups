/*
 * "$Id: hpgl-main.c,v 1.1 1996/08/24 19:41:24 mike Exp $"
 *
 *   for espPrint, a collection of printer/image software.
 *
 *   Copyright (c) 1993-1995 by Easy Software Products
 *
 *   These coded instructions, statements, and computer  programs  contain
 *   unpublished  proprietary  information  of Easy Software Products, and
 *   are protected by Federal copyright law.  They may  not  be  disclosed
 *   to  third  parties  or  copied or duplicated in any form, in whole or
 *   in part, without the prior written consent of Easy Software Products.
 *
 * Contents:
 *
 * Revision History:
 *
 *   $Log: hpgl-main.c,v $
 *   Revision 1.1  1996/08/24 19:41:24  mike
 *   Initial revision
 *
 */

/*
 * Include necessary headers...
 */

#define _MAIN_C_
#include "hpgl2ps.h"


typedef struct
{
  char name[4];
  void (*func)(int, param_t *);
} name_t;

static name_t commands[] =
{
  "IN", IN_initialize,
  "DF", DF_default_values,
  "IP", IP_input_absolute,
  "IR", IR_input_relative,
  "IW", IW_input_window,
  "PG", PG_advance_page,
  "RO", RO_rotate,
  "RP", RP_replot,
  "SC", SC_scale,
  "AA", AA_arc_absolute,
  "AR", AR_arc_relative,
  "AT", AT_arc_absolute3,
  "CI", CI_circle,
  "PA", PA_plot_absolute,
  "PD", PD_pen_down,
  "PE", PE_polyline_encoded,
  "PR", PR_plot_relative,
  "PU", PU_pen_up,
  "RT", RT_arc_relative3,
  "EA", EA_edge_rect_absolute,
  "EP", EP_edge_polygon,
  "ER", ER_edge_rect_relative,
  "EW", EW_edge_wedge,
  "FP", FP_fill_polygon,
  "PM", PM_polygon_mode,
  "RA", RA_fill_rect_absolute,
  "RR", RR_fill_rect_relative,
  "WG", WG_fill_wedge,
  "AD", AD_define_alternate,
  "CF", CF_character_fill,
  "CP", CP_character_plot,
  "DI", DI_absolute_direction,
  "DR", DR_relative_direction,
  "DT", DT_define_label_term,
  "DV", DV_define_variable_path,
  "ES", ES_extra_space,
  "LB", LB_label,
  "LO", LO_label_origin,
  "SA", SA_select_alternate,
  "SD", SD_define_standard,
  "SI", SI_absolute_size,
  "SL", SL_character_slant,
  "SR", SR_relative_size,
  "SS", SS_select_standard,
  "TD", TD_transparent_data,
  "AC", AC_anchor_corner,
  "FT", FT_fill_type,
  "LA", LA_line_attributes,
  "LT", LT_line_type,
  "PW", PW_pen_width,
  "RF", RF_raster_fill,
  "SM", SM_symbol_mode,
  "SP", SP_select_pen,
  "UL", UL_user_line_type,
  "WU", WU_width_units
};
#define NUM_COMMANDS (sizeof(commands) / sizeof(name_t))


static int
compare_names(void *p1, void *p2)
{
  return (strcasecmp(((name_t *)p1)->name, ((name_t *)p2)->name));
}


void
Usage(void)
{
  fputs("Usage: hpgl2ps -P printer [filename]\n", stderr);
  exit(1);
}


void
main(int  argc,
     char *argv[])
{
  int		i,		/* Looping var */
		val;		/* Temporary value */
  char		*opt;		/* Current option character */
  char		*filename,	/* Input filename, if specified (NULL otherwise). */
		*printer_name,
		*outfile;
  PDInfoStruct	*info;
  time_t	modtime;
  param_t	*params;
  int		num_params;
  name_t	*command,
		name;


 /*
  * Process any command-line args...
  */

  filename     = NULL;
  printer_name = NULL;
  outfile      = NULL;

  if (argc < 3)
    Usage();

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      for (opt = argv[i] + 1; *opt != '\0'; opt ++)
        switch (*opt)
        {
          case 'P' : /* Specify the printer name */
              i ++;
              if (i >= argc)
                Usage();

              printer_name = argv[i];
              break;

          case 'r' : /* Rotate */
              i ++;
              if (i >= argc)
                Usage();
              break;

          case 'z' : /* Page zoom */
              i ++;
              if (i >= argc)
                Usage();
              break;

          case 'L' : /* Log file */
              i ++;
              if (i >= argc)
                Usage();

              freopen(argv[i], "w", stderr);
              break;

          case 'O' : /* Output file */
              i ++;
              if (i >= argc)
                Usage();

              outfile = argv[i];
              break;

          case 'D' : /* Produce debugging messages */
              Verbosity ++;
              break;

          default :
              Usage();
              break;
        }
    else if (filename != NULL)
      Usage();
    else
      filename = argv[i];

  if (Verbosity)
  {
    fputs("hpgl2ps: Command-line args are:", stderr);
    for (i = 1; i < argc; i ++)
      fprintf(stderr, " %s", argv[i]);
    fputs("\n", stderr);
  };

 /*
  * Check for necessary args...
  */

  if (printer_name == NULL)
    Usage();

 /*
  * Open the POD database files and get the printer definition record.
  */
  
  if (PDLocalReadInfo(printer_name, &info, &modtime) < 0)
  {
    fprintf(stderr, "hpgl2ps: Could not open required POD database files for printer \'%s\'.\n", 
            printer_name);
    fprintf(stderr, "          Are you sure all required POD files are properly installed?\n");

    PDPerror("hpgl2ps");
    exit(1);
  };

  if (filename == NULL)
    InputFile = stdin;
  else if ((InputFile = fopen(filename, "r")) == NULL)
  {
    fprintf(stderr, "hpgl2ps: Could not open \'%s\' for reading.\n", filename);

    PDPerror("hpgl2ps");
    exit(1);
  };

  if (outfile == NULL)
    OutputFile = stdout;
  else if ((OutputFile = fopen(outfile, "w")) == NULL)
  {
    fprintf(stderr, "hpgl2ps: Could not create \'%s\' for writing.\n", outfile);

    PDPerror("hpgl2ps");
    exit(1);
  };

  OutputProlog(info);

  qsort(commands, NUM_COMMANDS, sizeof(name_t), compare_names);

  while ((num_params = ParseCommand(name.name, &params)) >= 0)
  {
#ifdef DEBUG
    fprintf(stderr, "%s(%d)", name.name, num_params);
    for (i = 0; i < num_params; i ++)
      if (params[i].type == PARAM_STRING)
        fprintf(stderr, " \'%s\'", params[i].value.string);
      else
        fprintf(stderr, " %f", params[i].value.number);
    fputs("\n", stderr);
#endif /* DEBUG */

    if ((command = bsearch(&name, commands, NUM_COMMANDS, sizeof(name_t),
                           compare_names)) != NULL)
      (*command->func)(num_params, params);

    FreeParameters(num_params, params);
  };

  OutputTrailer(info);
}


/*
 * End of "$Id: hpgl-main.c,v 1.1 1996/08/24 19:41:24 mike Exp $".
 */
