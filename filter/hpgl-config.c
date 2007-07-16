/*
 * "$Id: hpgl-config.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   HP-GL/2 configuration routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1993-2005 by Easy Software Products.
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
 *   update_transform()  - Update the page transformation matrix as needed.
 *   BP_begin_plot()     - Start a plot...
 *   DF_default_values() - Set all state info to the default values.
 *   IN_initialize()     - Initialize the plotter.
 *   IP_input_absolute() - Set P1 and P2 values for the plot.
 *   IR_input_relative() - Update P1 and P2.
 *   IW_input_window()   - Setup an input window.
 *   PG_advance_page()   - Eject the current page.
 *   PS_plot_size()      - Set the plot size.
 *   RO_rotate()         - Rotate the plot.
 *   RP_replot()         - Replot the current page.
 *   SC_scale()          - Set user-defined scaling.
 */

/*
 * Include necessary headers...
 */

#include "hpgltops.h"

#define max(a,b) ((a) < (b) ? (b) : (a))


/*
 * 'update_transform()' - Update the page transformation matrix as needed.
 */

void
update_transform(void)
{
  float	page_width,	/* Actual page width */
	page_height;	/* Actual page height */
  float	scaling;	/* Scaling factor */
  float	left, right,	/* Scaling window */
	bottom, top;
  float	width, height;	/* Scaling width and height */
  float	iw1[2], iw2[2];	/* Clipping window */


 /*
  * Get the page and input window sizes...
  */

  if (FitPlot)
  {
    page_width  = PageRight - PageLeft;
    page_height = PageTop - PageBottom;
  }
  else
  {
    page_width  = (P2[0] - P1[0]) * 72.0f / 1016.0f;
    page_height = (P2[1] - P1[1]) * 72.0f / 1016.0f;
  }

  fprintf(stderr, "DEBUG: page_width = %.0f, page_height = %.0f\n",
          page_width, page_height);

  if (page_width == 0 || page_height == 0)
    return;

 /*
  * Set the scaling window...
  */

  switch (ScalingType)
  {
    default : /* No user scaling */
        left   = P1[0];
	bottom = P1[1];
	right  = P2[0];
	top    = P2[1];
        break;

    case 0 : /* Anisotropic (non-uniform) scaling */
        left   = Scaling1[0];
	bottom = Scaling1[1];
	right  = Scaling2[0];
	top    = Scaling2[1];
        break;

    case 1 : /* Isotropic (uniform) scaling */
        left   = Scaling1[0];
	bottom = Scaling1[1];
	right  = Scaling2[0];
	top    = Scaling2[1];

	width  = right - left;
	height = top - bottom;
        
	if (width == 0 || height == 0)
	  return;

        if ((width * page_height) != (height * page_width))
	{
	  scaling = height * page_width / page_height;
	  if (width < scaling)
	  {
	    width = scaling;
	    left  = 0.5f * (left + right - width);
	    right = left + width;
	  }
	  else
	  {
	    height = width * page_height / page_width;
	    bottom = 0.5f * (bottom + top - height);
	    top    = bottom + height;
	  }
	}
        break;

    case 2 :
        left   = Scaling1[0];
	bottom = Scaling1[1];
	right  = left + page_width * Scaling2[0] * 1016.0f / 72.0f;
	top    = bottom + page_height * Scaling2[1] * 1016.0f / 72.0f;
        break;
  }

  width  = right - left;
  height = top - bottom;

  if (width == 0 || height == 0)
    return;

 /*
  * Scale the plot as needed...
  */

  if (Rotation == 0 || Rotation == 180)
    scaling = page_width / width;
  else
    scaling = page_width / height;

  if (FitPlot)
    scaling *= max(page_width, page_height) / max(PlotSize[1], PlotSize[0]);

 /*
  * Offset for the current P1 location...
  */

  if (FitPlot)
  {
    left   = 0;
    bottom = 0;
  }
  else
  {
    left   = P1[0] * 72.0f / 1016.0f;
    bottom = P1[1] * 72.0f / 1016.0f;
  }

 /*
  * Generate a new transformation matrix...
  */

  switch (Rotation)
  {
    default :
    case 0 :
	Transform[0][0] = scaling;
	Transform[0][1] = 0.0;
	Transform[0][2] = -left;
	Transform[1][0] = 0.0;
	Transform[1][1] = scaling;
	Transform[1][2] = -bottom;
	break;

    case 90 :
	Transform[0][0] = 0.0;
	Transform[0][1] = -scaling;
	Transform[0][2] = PageLength - left;
	Transform[1][0] = scaling;
	Transform[1][1] = 0.0;
	Transform[1][2] = -bottom;
	break;

    case 180 :
	Transform[0][0] = -scaling;
	Transform[0][1] = 0.0;
	Transform[0][2] = PageLength - left;
	Transform[1][0] = 0.0;
	Transform[1][1] = -scaling;
	Transform[1][2] = PageWidth - bottom;
	break;

    case 270 :
	Transform[0][0] = 0.0;
	Transform[0][1] = scaling;
	Transform[0][2] = -left;
	Transform[1][0] = -scaling;
	Transform[1][1] = 0.0;
	Transform[1][2] = PageWidth - bottom;
	break;
  }

  fprintf(stderr, "DEBUG: Transform = [ %.3f %.3f\n"
                  "DEBUG:               %.3f %.3f\n"
                  "DEBUG:               %.3f %.3f ]\n",
          Transform[0][0], Transform[1][0], Transform[0][1],
	  Transform[1][1], Transform[0][2], Transform[1][2]);

  if (FitPlot)
  {
    if (Rotation == 0 || Rotation == 180)
      PenScaling = page_width / PlotSize[1];
    else
      PenScaling = page_width / PlotSize[0];
  }
  else
    PenScaling = 1.0;

  if (PenScaling < 0.0)
    PenScaling = -PenScaling;

  if (PageDirty)
  {
    printf("%.2f setlinewidth\n", Pens[PenNumber].width * PenScaling);

    if (IW1[0] != IW2[0] && IW1[1] != IW2[1])
    {
      iw1[0] = IW1[0] * 72.0f / 1016.0f;
      iw1[1] = IW1[1] * 72.0f / 1016.0f;
      iw2[0] = IW2[0] * 72.0f / 1016.0f;
      iw2[1] = IW2[1] * 72.0f / 1016.0f;

      printf("initclip MP %.3f %.3f MO %.3f %.3f LI %.3f %.3f LI %.3f %.3f LI CP clip\n",
	     iw1[0], iw1[1], iw1[0], iw2[1], iw2[0], iw2[1], iw2[0], iw1[1]);
    }
  }
}


/*
 * 'BP_begin_plot()' - Start a plot...
 */

void
BP_begin_plot(int     num_params,	/* I - Number of parameters */
              param_t *params)		/* I - Parameters */
{
  (void)num_params;
  (void)params;
}


/*
 * 'DF_default_values()' - Set all state info to the default values.
 */

void
DF_default_values(int     num_params,	/* I - Number of parameters */
                  param_t *params)	/* I - Parameters */
{
  (void)num_params;
  (void)params;

  NP_number_pens(0, NULL);
  AC_anchor_corner(0, NULL);
  AD_define_alternate(0, NULL);
  SD_define_standard(0, NULL);
  CF_character_fill(0, NULL);
  DI_absolute_direction(0, NULL);
  DT_define_label_term(0, NULL);
  DV_define_variable_path(0, NULL);
  ES_extra_space(0, NULL);
  FT_fill_type(0, NULL);
  IW_input_window(0, NULL);
  LA_line_attributes(0, NULL);
  LO_label_origin(0, NULL);
  LT_line_type(0, NULL);
  PA_plot_absolute(0, NULL);
  PolygonMode = 0;
  RF_raster_fill(0, NULL);
  SC_scale(0, NULL);
  SM_symbol_mode(0, NULL);
  SS_select_standard(0, NULL);
  TD_transparent_data(0, NULL);
  UL_user_line_type(0, NULL);
}


/*
 * 'IN_initialize()' - Initialize the plotter.
 */

void
IN_initialize(int     num_params,	/* I - Number of parameters */
              param_t *params)		/* I - Parameters */
{
  (void)num_params;
  (void)params;

  DF_default_values(0, NULL);
  PU_pen_up(0, NULL);
  RO_rotate(0, NULL);
  PS_plot_size(0, NULL);
  WU_width_units(0, NULL);
  PW_pen_width(0, NULL);

  PenWidth = 1;

  PenPosition[0] = PenPosition[1] = 0.0;
}


/*
 * 'IP_input_absolute()' - Set P1 and P2 values for the plot.
 */

void
IP_input_absolute(int     num_params,	/* I - Number of parameters */
                  param_t *params)	/* I - Parameters */
{
  if (num_params == 0)
  {
    P1[0] = PageLeft / 72.0f * 1016.0f;
    P1[1] = PageBottom / 72.0f * 1016.0f;
    P2[0] = PageRight / 72.0f * 1016.0f;
    P2[1] = PageTop / 72.0f * 1016.0f;
  }
  else if (num_params == 2)
  {
    P2[0] -= P1[0];
    P2[1] -= P1[1];
    P1[0] = params[0].value.number;
    P1[1] = params[1].value.number;
    P2[0] += P1[0];
    P2[1] += P1[1];
  }
  else if (num_params == 4)
  {
    P1[0] = params[0].value.number;
    P1[1] = params[1].value.number;
    P2[0] = params[2].value.number;
    P2[1] = params[3].value.number;
  }

  IW1[0] = 0.0;
  IW1[1] = 0.0;
  IW2[0] = 0.0;
  IW2[1] = 0.0;

  if (ScalingType < 0)
  {
    Scaling1[0] = P1[0];
    Scaling1[0] = P1[1];
    Scaling2[0] = P2[0];
    Scaling2[1] = P2[1];
  }

  update_transform();
}


/*
 * 'IR_input_relative()' - Update P1 and P2.
 */

void
IR_input_relative(int     num_params,	/* I - Number of parameters */
                  param_t *params)	/* I - Parameters */
{
  if (num_params == 0)
  {
    P1[0] = PageLeft / 72.0f * 1016.0f;
    P1[1] = PageBottom / 72.0f * 1016.0f;
    P2[0] = PageRight / 72.0f * 1016.0f;
    P2[1] = PageTop / 72.0f * 1016.0f;
  }
  else if (num_params == 2)
  {
    P2[0] -= P1[0];
    P2[1] -= P1[1];
    P1[0] = params[0].value.number * PlotSize[0] / 72.0f * 1016.0f / 100.0f;
    P1[1] = params[1].value.number * PlotSize[1] / 72.0f * 1016.0f / 100.0f;
    P2[0] += P1[0];
    P2[1] += P1[1];
  }
  else if (num_params == 4)
  {
    P1[0] = params[0].value.number * PlotSize[0] / 72.0f * 1016.0f / 100.0f;
    P1[1] = params[1].value.number * PlotSize[1] / 72.0f * 1016.0f / 100.0f;
    P2[0] = params[2].value.number * PlotSize[0] / 72.0f * 1016.0f / 100.0f;
    P2[1] = params[3].value.number * PlotSize[1] / 72.0f * 1016.0f / 100.0f;
  }

  IW1[0] = 0.0;
  IW1[1] = 0.0;
  IW2[0] = 0.0;
  IW2[1] = 0.0;

  if (ScalingType < 0)
  {
    Scaling1[0] = P1[0];
    Scaling1[0] = P1[1];
    Scaling2[0] = P2[0];
    Scaling2[1] = P2[1];
  }

  update_transform();
}


/*
 * 'IW_input_window()' - Setup an input window.
 */

void
IW_input_window(int     num_params,	/* I - Number of parameters */
                param_t *params)	/* I - Parameters */
{
  if (num_params == 0)
  {
    IW1[0] = PageLeft / 72.0f * 1016.0f;
    IW1[1] = PageBottom / 72.0f * 1016.0f;
    IW2[0] = PageRight / 72.0f * 1016.0f;
    IW2[1] = PageTop / 72.0f * 1016.0f;
  }
  else if (num_params == 4)
  {

    if (ScalingType < 0)
    {
      IW1[0] = params[0].value.number;
      IW1[1] = params[1].value.number;
      IW2[0] = params[2].value.number;
      IW2[1] = params[3].value.number;
    }
    else
    {
      IW1[0] = (Transform[0][0] * params[0].value.number +
                Transform[0][1] * params[1].value.number +
                Transform[0][2]) / 72.0f * 1016.0f;
      IW1[1] = (Transform[1][0] * params[0].value.number +
                Transform[1][1] * params[1].value.number +
                Transform[1][2]) / 72.0f * 1016.0f;
      IW2[0] = (Transform[0][0] * params[2].value.number +
                Transform[0][1] * params[3].value.number +
                Transform[0][2]) / 72.0f * 1016.0f;
      IW2[1] = (Transform[1][0] * params[2].value.number +
                Transform[1][1] * params[3].value.number +
                Transform[1][2]) / 72.0f * 1016.0f;
    }

    fprintf(stderr, "DEBUG: IW%.0f,%.0f,%.0f,%.0f = [ %.0f %.0f %.0f %.0f ]\n",
	    params[0].value.number, params[1].value.number,
	    params[2].value.number, params[3].value.number,
	    IW1[0], IW1[1], IW2[0], IW2[1]);
  }


  update_transform();
}


/*
 * 'PG_advance_page()' - Eject the current page.
 */

void
PG_advance_page(int     num_params,	/* I - Number of parameters */
                param_t *params)	/* I - Parameters */
{
  (void)num_params;
  (void)params;

  if (PageDirty)
  {
    puts("grestore");
    puts("showpage");

    PageDirty = 0;
  }
}


/*
 * 'PS_plot_size()' - Set the plot size.
 */

void
PS_plot_size(int     num_params,	/* I - Number of parameters */
             param_t *params)		/* I - Parameters */
{
  switch (num_params)
  {
    case 0 : /* PS ; */
        if (Rotation == 0 || Rotation == 180)
        {
          PlotSize[0] = PageWidth;
          PlotSize[1] = PageLength;
	}
	else
	{
          PlotSize[0] = PageLength;
          PlotSize[1] = PageWidth;
	}

	PlotSizeSet = 0;
        break;
    case 1 : /* PS length ; */
        if (Rotation == 0 || Rotation == 180)
        {
          PlotSize[1] = 72.0f * params[0].value.number / 1016.0f;
          PlotSize[0] = 0.75f * PlotSize[1];
        }
        else
        {
          PlotSize[0] = 72.0f * params[0].value.number / 1016.0f;
          PlotSize[1] = 0.75f * PlotSize[0];
        }

	PlotSizeSet = 1;
        break;
    case 2 : /* PS length, width ; */
       /*
        * Unfortunately, it appears that NO application correctly
	* sends a two-argument PS command as documented in the
	* HP-GL/2 Reference Manual from HP.  Instead, applications
	* send the width before the length, which causes all sorts
	* of problems when scaling.
	*
	* Rather than fight it, we now look for them as width,length
	* instead of length,width.
	*
	* Don't like it?  Send mail to the folks that make Ideas, Pro/E,
	* AutoCAD, etc.
	*/

        if (Rotation == 0 || Rotation == 180)
        {
          PlotSize[0] = 72.0f * params[0].value.number / 1016.0f;
          PlotSize[1] = 72.0f * params[1].value.number / 1016.0f;
        }
        else
        {
          PlotSize[0] = 72.0f * params[1].value.number / 1016.0f;
          PlotSize[1] = 72.0f * params[0].value.number / 1016.0f;
        }

	PlotSizeSet = 1;
        break;
  }

 /*
  * This is required for buggy files that don't set the input window.
  */

  IP_input_absolute(0, NULL);
}


/*
 * 'RO_rotate()' - Rotate the plot.
 */

void
RO_rotate(int     num_params,	/* I - Number of parameters */
          param_t *params)	/* I - Parameters */
{
  if (num_params == 0)
    Rotation = 0;
  else
    Rotation = (int)params[0].value.number;

  update_transform();
}


/*
 * 'RP_replot()' - Replot the current page.
 */

void
RP_replot(int     num_params,	/* I - Number of parameters */
          param_t *params)	/* I - Parameters */
{
  (void)num_params;
  (void)params;
}


/*
 * 'SC_scale()' - Set user-defined scaling.
 */

void
SC_scale(int     num_params,	/* I - Number of parameters */
         param_t *params)	/* I - Parameters */
{
  if (num_params == 0)
  {
    ScalingType = -1;
    Scaling1[0] = P1[0];
    Scaling1[0] = P1[1];
    Scaling2[0] = P2[0];
    Scaling2[1] = P2[1];
  }
  else if (num_params > 3)
  {
    Scaling1[0] = params[0].value.number;
    Scaling2[0] = params[1].value.number;
    Scaling1[1] = params[2].value.number;
    Scaling2[1] = params[3].value.number;

    if (num_params > 4)
      ScalingType = (int)params[4].value.number;
    else
      ScalingType = 1;
  }

  update_transform();
}


/*
 * End of "$Id: hpgl-config.c 6649 2007-07-11 21:46:42Z mike $".
 */
