/*
 * "$Id: hpgl-config.c,v 1.9 1999/02/17 21:51:57 mike Exp $"
 *
 *   HPGL configuration routines for espPrint, a collection of printer drivers.
 *
 *   Copyright 1993-1998 by Easy Software Products
 *
 *   These coded instructions, statements, and computer programs contain
 *   unpublished proprietary information of Easy Software Products, and
 *   are protected by Federal copyright law. They may not be disclosed
 *   to third parties or copied or duplicated in any form, in whole or
 *   in part, without the prior written consent of Easy Software Products.
 *
 * Contents:
 *
 * Revision History:
 *
 *   $Log: hpgl-config.c,v $
 *   Revision 1.9  1999/02/17 21:51:57  mike
 *   Updated scaling code to ignore the PlotSize.
 *
 *   Added support for IW command.
 *
 *   Revision 1.8  1998/09/16  14:37:29  mike
 *   Fixed landscape printing bug.
 *   Fixed margins when page is rotated.
 *
 *   Revision 1.7  1998/08/31  20:35:49  mike
 *   Updated pen width code to automatically adjust scaling as needed.
 *   Updated PS code to adjust width/height by a factor of 0.75 for better
 *   scaling of plots.
 *
 *   Revision 1.7  1998/08/31  20:35:49  mike
 *   Updated pen width code to automatically adjust scaling as needed.
 *   Updated PS code to adjust width/height by a factor of 0.75 for better
 *   scaling of plots.
 *
 *   Revision 1.6  1998/05/20 13:18:48  mike
 *   Updated to offset page by the left/bottom margin no matter what.
 *
 *   Revision 1.5  1998/03/18  20:47:27  mike
 *   Fixed problem with some HPGL files not plotting - added IP call after
 *   a PS command.
 *
 *   Revision 1.4  1998/03/18  20:46:04  mike
 *   Updated to do optional page scaling.
 *   Now support 0, 90, 180, and 270 degree rotations.
 *
 *   Revision 1.3  1997/12/11  13:49:06  mike
 *   Updated PS_plot_size() code - now if a single size is provide we scale
 *   to the smallest page dimension (not just the length as before).
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
update_transform(void)
{
  float p1[2], p2[2];
  float	width, height;
  float	xoff, yoff;
  float	plotsize[2];
  float	scaling,
	pen_scaling;


  width  = IW2[0] - IW1[0];
  height = IW2[1] - IW1[1];

  if (width == 0 || height == 0)
    return;

  if (FitPlot)
  {
    if (Rotation == 0 || Rotation == 180)
    {
      scaling = PageWidth / width;

      if (scaling > (PageHeight / height))
      {
	if ((scaling * height) > PageHeight)
          scaling = PageHeight / height;
      }
      else if ((PageHeight / height * width) <= PageWidth)
        scaling = PageHeight / height;
    }
    else
    {
      scaling = PageWidth / height;

      if (scaling > (PageHeight / width))
      {
	if ((scaling * width) > PageHeight)
          scaling = PageHeight / width;
      }
      else if ((PageHeight / width * height) <= PageWidth)
        scaling = PageHeight / width;
    };

    plotsize[0] = width * scaling;
    plotsize[1] = height * scaling;
  }
  else
  {
    plotsize[0] = width * 72.0 / 1016.0;
    plotsize[1] = height * 72.0 / 1016.0;
    scaling     = 72.0 / 1016.0;
  };

  switch (ScalingType)
  {
    case 2 :
        if (Scaling2[0] != 0.0 && Scaling2[1] != 0.0)
        {
          p1[0] = P1[0] / fabs(Scaling2[0]);
          p1[1] = P1[1] / fabs(Scaling2[1]);
          p2[0] = P2[0] / fabs(Scaling2[0]);
          p2[1] = P2[1] / fabs(Scaling2[1]);
  	  break;
  	};

    case -1 :
        p1[0] = P1[0];
        p1[1] = P1[1];
        p2[0] = P2[0];
        p2[1] = P2[1];
	break;

    default :
        p1[0] = Scaling1[0];
        p1[1] = Scaling1[1];
        p2[0] = Scaling2[0];
        p2[1] = Scaling2[1];
	break;
  };

  if (p2[0] == p1[0])
    p2[0] ++;
  if (p2[1] == p1[1])
    p2[1] ++;

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
  };

  if (Verbosity)
  {
    fprintf(stderr, "hpgl2ps: IW1 = { %d %d }, IW2 = { %d %d }\n",
            IW1[0], IW1[1], IW2[0], IW2[1]);
    fprintf(stderr, "hpgl2ps: P1 = { %d %d }, P2 = { %d %d }\n",
            P1[0], P1[1], P2[0], P2[1]);
    fprintf(stderr, "hpgl2ps: PlotSize = { %f %f }\n",
            PlotSize[0], PlotSize[1]);
    fprintf(stderr, "hpgl2ps: Rotation = %d\n", Rotation);
    fprintf(stderr, "hpgl2ps: Scaling1 = { %d %d }, Scaling2 = { %d %d }\n",
            Scaling1[0], Scaling1[1], Scaling2[0], Scaling2[1]);
    fprintf(stderr, "hpgl2ps: p1 = { %f %f }, p2 = { %f %f }\n",
            p1[0], p1[1], p2[0], p2[1]);
    fprintf(stderr, "hpgl2ps: width = %f, height = %f\n", width, height);
    fprintf(stderr, "hpgl2ps: plotsize = { %f %f }\n",
            plotsize[0], plotsize[1]);
    fprintf(stderr, "hpgl2ps: scaling = %f\n", scaling);
    fprintf(stderr, "hpgl2ps: transform matrix = [ %f %f %f %f %f %f ]\n",
            Transform[0][0], Transform[1][0], 
            Transform[0][1], Transform[1][1], 
            Transform[0][2], Transform[1][2]);
  };

  if (FitPlot)
  {
    pen_scaling = Transform[0][0] + Transform[0][1];
    if (pen_scaling < 0.0)
      pen_scaling = -pen_scaling;
  }
  else
    pen_scaling = 1.0;

  fprintf(OutputFile, "/PenScaling %.3f def W%d\n", pen_scaling, PenNumber);
}


void
BP_begin_plot(int num_params, param_t *params)
{
}


void
DF_default_values(int num_params, param_t *params)
{
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


void
IN_initialize(int num_params, param_t *params)
{
  DF_default_values(0, NULL);
  PU_pen_up(0, NULL);
  RO_rotate(0, NULL);
  IP_input_absolute(0, NULL);
  WU_width_units(0, NULL);
  PW_pen_width(0, NULL);
  SP_select_pen(0, NULL);

  PenPosition[0] = PenPosition[1] = 0.0;
}


void
IP_input_absolute(int num_params, param_t *params)
{
  if (num_params == 0)
  {
    P1[0] = 0.0;
    P1[1] = 0.0;
    P2[0] = PlotSize[0] / 72.0 * 1016.0;
    P2[1] = PlotSize[1] / 72.0 * 1016.0;
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
  };

  IW1[0] = P1[0];
  IW1[1] = P1[1];
  IW2[0] = P2[0];
  IW2[1] = P2[1];

  update_transform();
}


void
IR_input_relative(int num_params, param_t *params)
{
  if (num_params == 0)
  {
    P1[0] = PageLeft / 72.0 * 1016.0;
    P1[1] = PageBottom / 72.0 * 1016.0;
    P2[0] = (PageLeft + PageWidth) / 72.0 * 1016.0;
    P2[1] = (PageBottom + PageHeight) / 72.0 * 1016.0;
  }
  else if (num_params == 2)
  {
    P2[0] -= P1[0];
    P2[1] -= P1[1];
    P1[0] = params[0].value.number * PageWidth / 72.0 * 1016.0 / 100.0;
    P1[1] = params[1].value.number * PageHeight / 72.0 * 1016.0 / 100.0;
    P2[0] += P1[0];
    P2[1] += P1[1];
  }
  else if (num_params == 4)
  {
    P1[0] = params[0].value.number * PageWidth / 72.0 * 1016.0 / 100.0;
    P1[1] = params[1].value.number * PageHeight / 72.0 * 1016.0 / 100.0;
    P2[0] = params[2].value.number * PageWidth / 72.0 * 1016.0 / 100.0;
    P2[1] = params[3].value.number * PageHeight / 72.0 * 1016.0 / 100.0;
  };

  IW1[0] = P1[0];
  IW1[1] = P1[1];
  IW2[0] = P2[0];
  IW2[1] = P2[1];

  update_transform();
}


void
IW_input_window(int num_params, param_t *params)
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
  };

  update_transform();
}


void
PG_advance_page(int num_params, param_t *params)
{
  fputs("grestore\n", OutputFile);
  fputs("showpage\n", OutputFile);
  fputs("%%EndPage\n", OutputFile);

  PageCount ++;
  PageDirty = 0;
  fprintf(OutputFile, "%%%%Page: %d\n", PageCount);
  fputs("gsave\n", OutputFile);

  switch (PageRotation)
  {
    case 0 :
        fprintf(OutputFile, "%.1f %.1f translate\n", PageLeft, PageBottom);
	break;
    case 90 :
        fprintf(OutputFile, "%.1f %.1f translate\n", PageBottom, PageRight);
	break;
    case 180 :
        fprintf(OutputFile, "%.1f %.1f translate\n", PageRight, PageTop);
	break;
    case 270 :
        fprintf(OutputFile, "%.1f %.1f translate\n", PageTop, PageLeft);
	break;
  };

}


void
PS_plot_size(int num_params, param_t *params)
{
  switch (num_params)
  {
    case 0 :
       /*
        * This is a hack for programs that assume a DesignJet's hard limits...
        */

        PlotSize[0] = 72.0 * 36.0;
        PlotSize[1] = 72.0 * 48.0;
        break;
    case 1 :
        if (Rotation == 0 || Rotation == 180)
        {
          PlotSize[1] = 72.0 * params[0].value.number / 1016.0;
          PlotSize[0] = 0.75 * PlotSize[1];
        }
        else
        {
          PlotSize[0] = 72.0 * params[0].value.number / 1016.0;
          PlotSize[1] = 0.75 * PlotSize[0];
        };
        break;
    case 2 :
        if (Rotation == 0 || Rotation == 180)
        {
          PlotSize[0] = 72.0 * params[1].value.number / 1016.0;
          PlotSize[1] = 72.0 * params[0].value.number / 1016.0;
        }
        else
        {
          PlotSize[0] = 72.0 * params[0].value.number / 1016.0;
          PlotSize[1] = 72.0 * params[1].value.number / 1016.0;
        };
        break;
  };

 /*
  * This is required for buggy files that don't set the input window.
  */

  IP_input_absolute(0, NULL);
}


void
RO_rotate(int num_params, param_t *params)
{
  if (num_params == 0)
    Rotation = 0;
  else
    Rotation = params[0].value.number;

  update_transform();
}


void
RP_replot(int num_params, param_t *params)
{
  fputs("copypage\n", OutputFile);
}


void
SC_scale(int num_params, param_t *params)
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
      ScalingType = params[4].value.number;
    else
      ScalingType = 0;
  };

  update_transform();
}


/*
 * End of "$Id: hpgl-config.c,v 1.9 1999/02/17 21:51:57 mike Exp $".
 */
