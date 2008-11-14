/*
 * "$Id: hpgl-attr.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   HP-GL/2 attribute processing for the Common UNIX Printing System (CUPS).
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
 *   CR_color_range()     - Set the range for color values.
 *   AC_anchor_corner()   - Set the anchor corner.
 *   FT_fill_type()       - Set the fill type or pattern.
 *   LA_line_attributes() - Set the line drawing attributes.
 *   LT_line_type()       - Set the line type (style)...
 *   NP_number_pens()     - Set the number of pens to be used.
 *   PC_pen_color()       - Set the pen color...
 *   PW_pen_width()       - Set the pen width.
 *   RF_raster_fill()     - Set the raster fill pattern.
 *   SM_symbol_mode()     - Set where symbols are drawn.
 *   SP_select_pen()      - Select a pen for drawing.
 *   UL_user_line_type()  - Set a user-defined line type.
 *   WU_width_units()     - Set the units used for pen widths.
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
    fprintf(stderr,
            "DEBUG: HP-GL/2 \'CR\' command with invalid number of "
	    "parameters (%d)!\n", num_params);
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
    MiterLimit = 3.0f;
    LineCap    = 0;
    LineJoin   = 0;
  }
  else for (i = 0; i < (num_params - 1); i += 2)
    switch ((int)params[i].value.number)
    {
      case 1 :
          LineCap = params[i + 1].value.number == 1 ? 0 :
                    params[i + 1].value.number == 4 ? 1 : 2;
          break;
      case 2 :
          switch ((int)params[i + 1].value.number)
          {
            case 1 :
            case 2 :
            case 3 :
                LineJoin = 0;
                break;
            case 5 :
                LineJoin = 2;
                break;
            default :
                LineJoin = 1;
                break;
          }
          break;
      case 3 :
          MiterLimit = 1.0 + 0.5 * (params[i + 1].value.number - 1.0);
          break;
    }

  if (PageDirty)
  {
    printf("%.1f setmiterlimit\n", MiterLimit);
    printf("%d setlinecap\n", LineCap);
    printf("%d setlinejoin\n", LineJoin);
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
  {
    if (params[0].value.number < 1 || params[0].value.number > MAX_PENS)
    {
      fprintf(stderr,
	      "DEBUG: HP-GL/2 \'NP\' command with invalid number of "
	      "pens (%d)!\n", (int)params[0].value.number);
      PenCount = 8;
    }
    else
      PenCount = (int)params[0].value.number;
  }
  else
    fprintf(stderr,
            "DEBUG: HP-GL/2 \'NP\' command with invalid number of "
	    "parameters (%d)!\n", num_params);

  for (i = 0; i < PenCount; i ++)
    Pens[i].width = PenWidth;

  PC_pen_color(0, NULL);
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
		  { 0.0, 0.0, 0.0 },	/* Black */
		  { 1.0, 0.0, 0.0 },	/* Red */
		  { 0.0, 1.0, 0.0 },	/* Green */
		  { 1.0, 1.0, 0.0 },	/* Yellow */
		  { 0.0, 0.0, 1.0 },	/* Blue */
		  { 1.0, 0.0, 1.0 },	/* Magenta */
		  { 0.0, 1.0, 1.0 },	/* Cyan */
		  { 1.0, 1.0, 1.0 }	/* White */
		};


  if (num_params == 0)
  {
    for (i = 0; i < PenCount; i ++)
      if (i < 8)
      {
        Pens[i].rgb[0] = standard_colors[i][0];
        Pens[i].rgb[1] = standard_colors[i][1];
        Pens[i].rgb[2] = standard_colors[i][2];
      }
      else
      {
        Pens[i].rgb[0] = 0.0f;
        Pens[i].rgb[1] = 0.0f;
        Pens[i].rgb[2] = 0.0f;
      }

    if (PageDirty)
      printf("%.3f %.3f %.3f %.2f SP\n", Pens[PenNumber].rgb[0],
	     Pens[PenNumber].rgb[1], Pens[PenNumber].rgb[2],
	     Pens[PenNumber].width * PenScaling);
  }
  else if (num_params == 1 || num_params == 4)
  {
    i = (int)params[0].value.number - 1;

    if (i < 0 || i >= PenCount)
    {
      fprintf(stderr,
              "DEBUG: HP-GL/2 \'PC\' command with invalid pen (%d)!\n", i + 1);
      return;
    }

    if (num_params == 1)
    {
      Pens[i].rgb[0] = standard_colors[i & 7][0];
      Pens[i].rgb[1] = standard_colors[i & 7][1];
      Pens[i].rgb[2] = standard_colors[i & 7][2];
    }
    else
    {
      Pens[i].rgb[0] = (params[1].value.number - ColorRange[0][0]) /
                       (ColorRange[0][1] - ColorRange[0][0]);
      Pens[i].rgb[1] = (params[2].value.number - ColorRange[1][0]) /
                       (ColorRange[1][1] - ColorRange[1][0]);
      Pens[i].rgb[2] = (params[3].value.number - ColorRange[2][0]) /
                       (ColorRange[2][1] - ColorRange[2][0]);

      fprintf(stderr, "DEBUG: Pen %d %.0f %.0f %.0f = %.3f %.3f %.3f\n",
	      i, params[1].value.number, params[2].value.number,
	      params[3].value.number, Pens[i].rgb[0], Pens[i].rgb[1],
	      Pens[i].rgb[2]);
    }

    if (PageDirty && i == PenNumber)
      printf("%.3f %.3f %.3f %.2f SP\n", Pens[PenNumber].rgb[0],
	     Pens[PenNumber].rgb[1], Pens[PenNumber].rgb[2],
	     Pens[PenNumber].width * PenScaling);
  }
  else
    fprintf(stderr,
            "DEBUG: HP-GL/2 \'PC\' command with invalid number of "
	    "parameters (%d)!\n", num_params);
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
      w = 0.35f / 25.4f * 72.0f;
    else
      w = params[0].value.number / 25.4f * 72.0f;
  }
  else
  {
   /*
    * Relative...
    */

    w = (float)hypot(PlotSize[0], PlotSize[1]) / 1016.0f * 72.0f;

    if (num_params == 0)
      w *= 0.01f;
    else
      w *= params[0].value.number;
  }

  if (num_params == 2)
  {
    pen = (int)params[1].value.number - 1;

    if (pen < 0 || pen >= PenCount)
    {
      fprintf(stderr,
              "DEBUG: HP-GL/2 \'PW\' command with invalid pen (%d)!\n",
	      pen + 1);
      return;
    }

    Pens[pen].width = w;

    if (PageDirty && pen == PenNumber)
      printf("%.3f %.3f %.3f %.2f SP\n", Pens[PenNumber].rgb[0],
	     Pens[PenNumber].rgb[1], Pens[PenNumber].rgb[2],
	     Pens[PenNumber].width * PenScaling);
  }
  else if (num_params < 2)
  {
   /*
    * Set width for all pens...
    */

    for (pen = 0; pen < PenCount; pen ++)
      Pens[pen].width = w;

    if (PageDirty)
      printf("%.3f %.3f %.3f %.2f SP\n", Pens[PenNumber].rgb[0],
	     Pens[PenNumber].rgb[1], Pens[PenNumber].rgb[2],
	     Pens[PenNumber].width * PenScaling);
  }
  else
    fprintf(stderr,
            "DEBUG: HP-GL/2 \'PW\' command with invalid number of "
	    "parameters (%d)!\n", num_params);
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
    PenNumber = 0;
  else if (num_params > 1)
    fprintf(stderr,
            "DEBUG: HP-GL/2 \'SP\' command with invalid number of parameters "
	    "(%d)!\n", num_params);
  else if (params[0].value.number <= 0 || params[0].value.number >= PenCount)
    fprintf(stderr, "DEBUG: HP-GL/2 \'SP\' command with invalid pen (%d)!\n",
	    (int)params[0].value.number);
  else
    PenNumber = (int)params[0].value.number - 1;

  if (PageDirty)
    printf("%.3f %.3f %.3f %.2f SP\n", Pens[PenNumber].rgb[0],
	   Pens[PenNumber].rgb[1], Pens[PenNumber].rgb[2],
	   Pens[PenNumber].width * PenScaling);
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
    WidthUnits = (int)params[0].value.number;
  else
    fprintf(stderr,
            "DEBUG: HP-GL/2 \'WU\' command with invalid number of "
	    "parameters (%d)!\n", num_params);
}


/*
 * End of "$Id: hpgl-attr.c 6649 2007-07-11 21:46:42Z mike $".
 */
