/*
 * "$Id: hpgl-attr.c,v 1.5 1999/02/17 21:51:57 mike Exp $"
 *
 *   HPGL attribute processing for espPrint, a collection of printer drivers.
 *
 *   Copyright 1993-1998 by Easy Software Products
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
 *   $Log: hpgl-attr.c,v $
 *   Revision 1.5  1999/02/17 21:51:57  mike
 *   Updated scaling code to ignore the PlotSize.
 *
 *   Added support for IW command.
 *
 *   Revision 1.4  1998/08/31  20:35:49  mike
 *   Updated pen width code to automatically adjust scaling as needed.
 *   Updated PS code to adjust width/height by a factor of 0.75 for better
 *   scaling of plots.
 *
 *   Revision 1.4  1998/08/31  20:35:49  mike
 *   Updated pen width code to automatically adjust scaling as needed.
 *   Updated PS code to adjust width/height by a factor of 0.75 for better
 *   scaling of plots.
 *
 *   Revision 1.3  1998/03/17  21:59:57  mike
 *   Added CR (color range) support.
 *
 *   Revision 1.2  1996/10/14  16:50:14  mike
 *   Updated for 3.2 release.
 *   Added 'blackplot', grayscale, and default pen width options.
 *   Added encoded polyline support.
 *   Added fit-to-page code.
 *   Added pen color palette support.
 *
 *   Revision 1.1  1996/08/24  19:41:24  mike
 *   Initial revision
 */

/*
 * Include necessary headers...
 */

#include "hpgl2ps.h"


void
CR_color_range(int num_params, param_t *params)
{
  if (num_params == 0)
  {
    ColorRange[0][0] = 0.0;
    ColorRange[0][1] = 255.0;
    ColorRange[1][0] = 0.0;
    ColorRange[1][1] = 255.0;
    ColorRange[2][0] = 0.0;
    ColorRange[2][1] = 255.0;
  }
  else if (num_params == 6)
  {
    ColorRange[0][0] = params[0].value.number;
    ColorRange[0][1] = params[1].value.number - params[0].value.number;
    ColorRange[1][0] = params[2].value.number;
    ColorRange[1][1] = params[3].value.number - params[2].value.number;
    ColorRange[2][0] = params[4].value.number;
    ColorRange[2][1] = params[5].value.number - params[4].value.number;
  };
}


void
AC_anchor_corner(int num_params, param_t *params)
{
}


void
FT_fill_type(int num_params, param_t *params)
{
  if (num_params == 0 ||
      params[0].value.number == 1 ||
      params[0].value.number == 2)
  {
    /**** SOLID PATTERN ****/
  };    
}


void
LA_line_attributes(int num_params, param_t *params)
{
  int i;


  if (num_params == 0)
  {
    fputs("3.0 setmiterlimit\n", OutputFile);
    fputs("0 setlinecap\n", OutputFile);
    fputs("0 setlinejoin\n", OutputFile);
  }
  else for (i = 0; i < (num_params - 1); i += 2)
    switch ((int)params[i].value.number)
    {
      case 1 :
          fprintf(OutputFile, "%d setlinecap\n",
                  params[i + 1].value.number == 1 ? 0 :
                  params[i + 1].value.number == 4 ? 1 : 2);
          break;
      case 2 :
          switch ((int)params[i + 1].value.number)
          {
            case 1 :
            case 2 :
            case 3 :
                fputs("0 setlinejoin\n", OutputFile);
                break;
            case 5 :
                fputs("2 setlinejoin\n", OutputFile);
                break;
            default :
                fputs("1 setlinejoin\n", OutputFile);
                break;
          };
          break;
      case 3 :
          fprintf(OutputFile, "%f setmiterlimit\n",
                  1.0 + 0.5 * (params[i + 1].value.number - 1.0));
          break;
    };
}


void
LT_line_type(int num_params, param_t *params)
{
}


void
NP_number_pens(int num_params, param_t *params)
{
  int	i;


  if (num_params < 1)
    PenCount = 8;
  else
    PenCount = params[0].value.number;

  PC_pen_color(0, NULL);

  for (i = 0; i <= PenCount; i ++)
    fprintf(OutputFile, "/W%d { DefaultPenWidth PenScaling mul setlinewidth } bind def\n", i);
}


void
PC_pen_color(int num_params, param_t *params)
{
  int		i;
  static float	standard_colors[8][3] =
  {
    { 1.0, 1.0, 1.0 },
    { 0.0, 0.0, 0.0 },
    { 1.0, 0.0, 0.0 },
    { 0.0, 1.0, 0.0 },
    { 1.0, 1.0, 0.0 },
    { 0.0, 0.0, 1.0 },
    { 1.0, 0.0, 1.0 },
    { 0.0, 1.0, 1.0 }
  };

  if (num_params == 0)
  {
    for (i = 0; i <= PenCount; i ++)
      if (i < 8)
	fprintf(OutputFile, "/P%d { %.3f %.3f %.3f setrgbcolor } bind def\n",
        	i, standard_colors[i][0],
        	standard_colors[i][1], standard_colors[i][2]);
      else
	fprintf(OutputFile, "/P%d { 0.0 0.0 0.0 setrgbcolor } bind def\n", i);
  }
  else
  {
    i = params[0].value.number;

    if (num_params == 1)
      fprintf(OutputFile, "/P%d { %.3f %.3f %.3f setrgbcolor } bind def\n",
              i, standard_colors[i][0],
              standard_colors[i][1], standard_colors[i][2]);
    else
      fprintf(OutputFile, "/P%d { %.3f %.3f %.3f setrgbcolor } bind def\n",
              i, 
              (params[1].value.number - ColorRange[0][0]) / ColorRange[0][1],
              (params[2].value.number - ColorRange[1][0]) / ColorRange[1][1],
              (params[3].value.number - ColorRange[2][0]) / ColorRange[2][1]);
  };
}


void
PW_pen_width(int num_params, param_t *params)
{
  float	w;


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
  };

  if (num_params > 1)
    fprintf(OutputFile, "/W%d { %.1f PenScaling mul setlinewidth } bind def W%d\n",
            (int)params[1].value.number, w, (int)params[1].value.number);
  else
    fprintf(OutputFile, "/W%d { %.1f PenScaling mul setlinewidth } bind def W%d\n", PenNumber,
            w, PenNumber);
}


void
RF_raster_fill(int num_params, param_t *params)
{
}


void
SM_symbol_mode(int num_params, param_t *params)
{
}


void
SP_select_pen(int num_params, param_t *params)
{
  if (num_params == 0)
    PenNumber = 1;
  else if (params[0].value.number <= PenCount)
    PenNumber = params[0].value.number;

  fprintf(OutputFile, "P%d W%d\n", PenNumber, PenNumber);
}


void
UL_user_line_type(int num_params, param_t *params)
{
}


void
WU_width_units(int num_params, param_t *params)
{
  if (num_params == 0)
    WidthUnits = 0;
  else
    WidthUnits = params[0].value.number;
}


/*
 * End of "$Id: hpgl-attr.c,v 1.5 1999/02/17 21:51:57 mike Exp $".
 */
