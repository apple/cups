/*
 * "$Id: hpgl-config.c,v 1.12 1999/03/21 21:12:14 mike Exp $"
 *
 *   HP-GL/2 configuration routines for the Common UNIX Printing System (CUPS).
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
 * 'update_transform()' - Update the page transformation matrix as needed.
 */

void
update_transform(void)
{
  float	width,		/* Plot width */
	height;		/* Plot height */
  float	page_width,	/* Actual page width in points */
	page_length;	/* Actual page length in points */
  float	scaling;	/* Scaling factor */


 /*
  * Get the page and input window sizes...
  */

  width       = IW2[0] - IW1[0];
  height      = IW2[1] - IW1[1];
  page_width  = PageRight - PageLeft;
  page_length = PageTop - PageBottom;

  if (width == 0 || height == 0)
    return;

 /*
  * Scale the plot as needed...
  */

  if (FitPlot)
  {
    if (Rotation == 0 || Rotation == 180)
    {
      scaling = page_width / width;

      if (scaling > (page_length / height))
        scaling = page_length / height;
      else if ((page_length / height * width) <= page_width)
        scaling = page_length / height;
    }
    else
    {
      scaling = page_width / height;

      if (scaling > (page_length / width))
        scaling = page_length / width;
      else if ((page_length / width * height) <= page_width)
        scaling = page_length / width;
    }
  }
  else
    scaling = 72.0f / 1016.0f;

 /*
  * Generate a new transformation matrix...
  */

  switch (Rotation)
  {
    case 0 :
	Transform[0][0] = scaling;
	Transform[0][1] = 0.0;
	Transform[0][2] = -IW1[0] * scaling;
	Transform[1][0] = 0.0;
	Transform[1][1] = scaling;
	Transform[1][2] = -IW1[1] * scaling;
	break;

    case 90 :
	Transform[0][0] = 0.0;
	Transform[0][1] = -scaling;
	Transform[0][2] = (height - IW1[0]) * scaling;
	Transform[1][0] = scaling;
	Transform[1][1] = 0.0;
	Transform[1][2] = -IW1[1] * scaling;
	break;

    case 180 :
	Transform[0][0] = -scaling;
	Transform[0][1] = 0.0;
	Transform[0][2] = (height - IW1[0]) * scaling;
	Transform[1][0] = 0.0;
	Transform[1][1] = -scaling;
	Transform[1][2] = (width - IW1[1]) * scaling;
	break;

    case 270 :
	Transform[0][0] = 0.0;
	Transform[0][1] = scaling;
	Transform[0][2] = -IW1[0] * scaling;
	Transform[1][0] = -scaling;
	Transform[1][1] = 0.0;
	Transform[1][2] = (width - IW1[1]) * scaling;
	break;
  }

  if (FitPlot)
  {
    PenScaling = Transform[0][0] + Transform[0][1];

    if (PenScaling < 0.0)
      PenScaling = -PenScaling;
  }
  else
    PenScaling = 1.0;

  if (PageDirty)
    printf("/PenScaling %.3f def W%d\n", PenScaling, PenNumber);
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
  IP_input_absolute(0, NULL);
  WU_width_units(0, NULL);
  PW_pen_width(0, NULL);
  SP_select_pen(0, NULL);

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
    P1[0] = 0.0;
    P1[1] = 0.0;
    P2[0] = PlotSize[0] / 72.0f * 1016.0f;
    P2[1] = PlotSize[1] / 72.0f * 1016.0f;
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

  IW1[0] = P1[0];
  IW1[1] = P1[1];
  IW2[0] = P2[0];
  IW2[1] = P2[1];

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
    P1[0] = params[0].value.number * (PageRight - PageLeft) / 72.0f * 1016.0f / 100.0f;
    P1[1] = params[1].value.number * (PageTop - PageBottom) / 72.0f * 1016.0f / 100.0f;
    P2[0] += P1[0];
    P2[1] += P1[1];
  }
  else if (num_params == 4)
  {
    P1[0] = params[0].value.number * (PageRight - PageLeft) / 72.0f * 1016.0f / 100.0f;
    P1[1] = params[1].value.number * (PageTop - PageBottom) / 72.0f * 1016.0f / 100.0f;
    P2[0] = params[2].value.number * (PageRight - PageLeft) / 72.0f * 1016.0f / 100.0f;
    P2[1] = params[3].value.number * (PageTop - PageBottom) / 72.0f * 1016.0f / 100.0f;
  }

  IW1[0] = P1[0];
  IW1[1] = P1[1];
  IW2[0] = P2[0];
  IW2[1] = P2[1];

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
    IW1[0] = P1[0];
    IW1[1] = P1[1];
    IW2[0] = P2[0];
    IW2[1] = P2[1];
  }
  else if (num_params == 4)
  {
    IW1[0] = params[0].value.number;
    IW1[1] = params[1].value.number;
    IW2[0] = params[2].value.number;
    IW2[1] = params[3].value.number;
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
  if (PageDirty)
  {
    puts("grestore");
    puts("showpage");
    puts("%%EndPage");

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
    case 0 :
       /*
        * This is a hack for programs that assume a DesignJet's hard limits...
        */

        PlotSize[0] = 72.0f * 36.0f;
        PlotSize[1] = 72.0f * 48.0f;
        break;
    case 1 :
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
        break;
    case 2 :
        if (Rotation == 0 || Rotation == 180)
        {
          PlotSize[0] = 72.0f * params[1].value.number / 1016.0f;
          PlotSize[1] = 72.0f * params[0].value.number / 1016.0f;
        }
        else
        {
          PlotSize[0] = 72.0f * params[0].value.number / 1016.0f;
          PlotSize[1] = 72.0f * params[1].value.number / 1016.0f;
        }
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
    ScalingType = -1;
  else if (num_params > 3)
  {
    Scaling1[0] = params[0].value.number;
    Scaling2[0] = params[1].value.number;
    Scaling1[1] = params[2].value.number;
    Scaling2[1] = params[3].value.number;

    if (num_params > 4)
      ScalingType = (int)params[4].value.number;
    else
      ScalingType = 0;
  }

  update_transform();
}


/*
 * End of "$Id: hpgl-config.c,v 1.12 1999/03/21 21:12:14 mike Exp $".
 */
