/*
 * "$Id: hpgl-attr.c,v 1.8 1999/03/21 16:26:58 mike Exp $"
 *
 *   HP-GL/2 attribute processing for the Common UNIX Printing System (CUPS).
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
 */

/*
 * Include necessary headers...
 */

#include "hpgltops.h"


/*
 * 'CR_color_range()' - Set the range for color values.
 */

void
CR_color_range(int     num_params,	/* I - Number of parameters */
               param_t *params)		/* I - Parameters */
{
  if (num_params == 0)
  {
   /*
    * Default to 0 to 255 for all color values.
    */

    ColorRange[0][0] = 0.0;
    ColorRange[0][1] = 255.0;
    ColorRange[1][0] = 0.0;
    ColorRange[1][1] = 255.0;
    ColorRange[2][0] = 0.0;
    ColorRange[2][1] = 255.0;
  }
  else if (num_params == 6)
  {
   /*
    * Set the range based on the parameters...
    */
    ColorRange[0][0] = params[0].value.number;
    ColorRange[0][1] = params[1].value.number - params[0].value.number;
    ColorRange[1][0] = params[2].value.number;
    ColorRange[1][1] = params[3].value.number - params[2].value.number;
    ColorRange[2][0] = params[4].value.number;
    ColorRange[2][1] = params[5].value.number - params[4].value.number;
  }
  else
    fprintf(stderr, "WARNING: HP-GL/2 \'CR\' command with invalid number of parameters (%d)!\n",
            num_params);
}


/*
 * 'AC_anchor_corner()' - Set the anchor corner.
 */

void
AC_anchor_corner(int     num_params,	/* I - Number of parameters */
                 param_t *params)	/* I - Parameters */
{
  (void)num_params;
  (void)params;
}


/*
 * 'FT_fill_type()' - Set the fill type or pattern.
 *
 * Note:
 *
 *   This needs to be updated to support non-solid fill.
 */

void
FT_fill_type(int     num_params,	/* I - Number of parameters */
             param_t *params)		/* I - Parameters */
{
  if (num_params == 0 ||
      params[0].value.number == 1 ||
      params[0].value.number == 2)
  {
    /**** SOLID PATTERN ****/
  }
}


/*
 * 'LA_line_attributes()' - Set the line drawing attributes.
 */

void
LA_line_attributes(int     num_params,	/* I - Number of parameters */
                   param_t *params)	/* I - Parameters */
{
  int	i;				/* Looping var */


  if (num_params == 0)
  {
    Outputf("3.0 setmiterlimit\n");
    Outputf("0 setlinecap\n");
    Outputf("0 setlinejoin\n");
  }
  else for (i = 0; i < (num_params - 1); i += 2)
    switch ((int)params[i].value.number)
    {
      case 1 :
          Outputf("%d setlinecap\n",
                  params[i + 1].value.number == 1 ? 0 :
                  params[i + 1].value.number == 4 ? 1 : 2);
          break;
      case 2 :
          switch ((int)params[i + 1].value.number)
          {
            case 1 :
            case 2 :
            case 3 :
                Outputf("0 setlinejoin\n");
                break;
            case 5 :
                Outputf("2 setlinejoin\n");
                break;
            default :
                Outputf("1 setlinejoin\n");
                break;
          }
          break;
      case 3 :
          Outputf("%f setmiterlimit\n",
                  1.0 + 0.5 * (params[i + 1].value.number - 1.0));
          break;
    }
}


/*
 * 'LT_line_type()' - Set the line type (style)...
 *
 * Note:
 *
 *   This needs to be updated to support line types.
 */

void
LT_line_type(int     num_params,	/* I - Number of parameters */
             param_t *params)		/* I - Parameters */
{
  (void)num_params;
  (void)params;
}


/*
 * 'NP_number_pens()' - Set the number of pens to be used.
 */

void
NP_number_pens(int     num_params,	/* I - Number of parameters */
               param_t *params)		/* I - Parameters */
{
  int	i;				/* Looping var */


  if (num_params == 0)
    PenCount = 8;
  else if (num_params == 1)
    PenCount = params[0].value.number;
  else
    fprintf(stderr, "WARNING: HP-GL/2 \'NP\' command with invalid number of parameters (%d)!\n",
            num_params);

  PC_pen_color(0, NULL);

  for (i = 0; i <= PenCount; i ++)
    Outputf(OutputFile, "/W%d { DefaultPenWidth PenScaling mul setlinewidth } bind def\n", i);
}


/*
 * 'PC_pen_color()' - Set the pen color...
 */

void
PC_pen_color(int     num_params,	/* I - Number of parameters */
             param_t *params)		/* I - Parameters */
{
  int		i;			/* Looping var */
  static float	standard_colors[8][3] =	/* Standard colors for first 8 pens */
		{
		  { 1.0, 1.0, 1.0 },	/* White */
		  { 0.0, 0.0, 0.0 },	/* Black */
		  { 1.0, 0.0, 0.0 },	/* Red */
		  { 0.0, 1.0, 0.0 },	/* Green */
		  { 1.0, 1.0, 0.0 },	/* Yellow */
		  { 0.0, 0.0, 1.0 },	/* Blue */
		  { 1.0, 0.0, 1.0 },	/* Magenta */
		  { 0.0, 1.0, 1.0 }	/* Cyan */
		};

  if (num_params == 0)
  {
    for (i = 0; i <= PenCount; i ++)
      if (i < 8)
	Outputf("/P%d { %.3f %.3f %.3f setrgbcolor } bind def\n",
        	i, standard_colors[i][0],
        	standard_colors[i][1], standard_colors[i][2]);
      else
	Outputf("/P%d { 0.0 0.0 0.0 setrgbcolor } bind def\n", i);
  }
  else if (num_params == 1)
  {
    i = params[0].value.number;

    Outputf("/P%d { %.3f %.3f %.3f setrgbcolor } bind def\n",
            i, standard_colors[i & 7][0], standard_colors[i & 7][1],
            standard_colors[i & 7][2]);
  }
  else if (num_params == 4)
    Outputf("/P%d { %.3f %.3f %.3f setrgbcolor } bind def\n",
            (int)params[0].value.number, 
            (params[1].value.number - ColorRange[0][0]) / ColorRange[0][1],
            (params[2].value.number - ColorRange[1][0]) / ColorRange[1][1],
            (params[3].value.number - ColorRange[2][0]) / ColorRange[2][1]);
  else
    fprintf(stderr, "WARNING: HP-GL/2 \'PC\' command with invalid number of parameters (%d)!\n",
            num_params);
}


/*
 * 'PW_pen_width()' - Set the pen width.
 */

void
PW_pen_width(int     num_params,	/* I - Number of parameters */
             param_t *params)		/* I - Parameters */
{
  int	pen;				/* Pen number */
  float	w;				/* Width value */


  if (WidthUnits == 0)
  {
   /*
    * Metric...
    */

    if (num_params == 0)
      w = 0.35 / 25.4 * 72.0;
    else
      w = params[0].value.number / 25.4 * 72.0;
  }
  else
  {
   /*
    * Relative...
    */

    w = hypot(PlotSize[0], PlotSize[1]) / 1016.0 * 72.0;

    if (num_params == 0)
      w *= 0.01;
    else
      w *= params[0].value.number;
  }

  if (num_params == 2)
    Outputf("/W%d { %.1f PenScaling mul setlinewidth } bind def W%d\n",
            (int)params[1].value.number, w, (int)params[1].value.number);
  else if (num_params < 2)
  {
   /*
    * Set width for all pens...
    */

    for (pen = 0; pen <= PenCount; pen ++)
      Outputf("/W%d { %.1f PenScaling mul setlinewidth } bind def\n",
              pen, w);

    Outputf("W%d\n", PenNumber);
  }
  else
    fprintf(stderr, "WARNING: HP-GL/2 \'PW\' command with invalid number of parameters (%d)!\n",
            num_params);
}


/*
 * 'RF_raster_fill()' - Set the raster fill pattern.
 *
 * Note:
 *
 *   This needs to be implemented.
 */

void
RF_raster_fill(int     num_params,	/* I - Number of parameters */
               param_t *params)		/* I - Parameters */
{
  (void)num_params;
  (void)params;
}


/*
 * 'SM_symbol_mode()' - Set where symbols are drawn.
 */

void
SM_symbol_mode(int     num_params,	/* I - Number of parameters */
               param_t *params)		/* I - Parameters */
{
  (void)num_params;
  (void)params;
}


/*
 * 'SP_select_pen()' - Select a pen for drawing.
 */

void
SP_select_pen(int     num_params,	/* I - Number of parameters */
              param_t *params)		/* I - Parameters */
{
  if (num_params == 0)
    PenNumber = 1;
  else if (params[0].value.number <= PenCount)
    PenNumber = params[0].value.number;
  else
    fprintf(stderr, "WARNING: HP-GL/2 \'SP\' command with invalid number or value of parameters (%d, %d)!\n",
            num_params, (int)params[0].value.number);

  Outputf("P%d W%d\n", PenNumber, PenNumber);
}


/*
 * 'UL_user_line_type()' - Set a user-defined line type.
 */

void
UL_user_line_type(int     num_params,	/* I - Number of parameters */
                  param_t *params)	/* I - Parameters */
{
  (void)num_params;
  (void)params;
}


/*
 * 'WU_width_units()' - Set the units used for pen widths.
 */

void
WU_width_units(int     num_params,	/* I - Number of parameters */
               param_t *params)		/* I - Parameters */
{
  if (num_params == 0)
    WidthUnits = 0;
  else if (num_params == 1)
    WidthUnits = params[0].value.number;
  else
    fprintf(stderr, "WARNING: HP-GL/2 \'WU\' command with invalid number of parameters (%d)!\n",
            num_params);
}


/*
 * End of "$Id: hpgl-attr.c,v 1.8 1999/03/21 16:26:58 mike Exp $".
 */
