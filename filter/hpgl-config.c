/*
 * "$Id: hpgl-config.c,v 1.1 1996/08/24 19:41:24 mike Exp $"
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
 *   $Log: hpgl-config.c,v $
 *   Revision 1.1  1996/08/24 19:41:24  mike
 *   Initial revision
 *
 */

/*
 * Include necessary headers...
 */

#include "hpgl2ps.h"


void
update_transform(void)
{
  float p1[2], p2[2];


  switch (ScalingType)
  {
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

  switch (Rotation)
  {
    case 0 :
	Transform[0][0] = PageWidth / (p2[0] - p1[0]);
	Transform[0][1] = 0.0;
	Transform[1][0] = 0.0;
	Transform[1][1] = PageHeight / (p2[1] - p1[1]);
	Transform[2][0] = -p1[0] * PageWidth / (p2[0] - p1[0]);
	Transform[2][1] = -p1[1] * PageHeight / (p2[1] - p1[1]);
	break;

    case 90 :
	Transform[0][0] = 0.0;
	Transform[0][1] = PageWidth / (p1[1] - p2[1]);
	Transform[1][0] = PageHeight / (p2[0] - p1[0]);
	Transform[1][1] = 0.0;
	Transform[2][0] = p1[1] * PageWidth / (p1[1] - p2[1]);
	Transform[2][1] = -p1[0] * PageHeight / (p2[0] - p1[0]);
	break;

    case 180 :
	Transform[0][0] = PageWidth / (p1[0] - p2[0]);
	Transform[0][1] = 0.0;
	Transform[1][0] = 0.0;
	Transform[1][1] = PageHeight / (p1[1] - p2[1]);
	Transform[2][0] = p1[0] * PageWidth / (p1[0] - p2[0]);
	Transform[2][1] = p1[1] * PageHeight / (p1[1] - p2[1]);
	break;

    case 270 :
	Transform[0][0] = 0.0;
	Transform[0][1] = PageWidth / (p2[1] - p1[1]);
	Transform[1][0] = PageHeight / (p1[0] - p2[0]);
	Transform[1][1] = 0.0;
	Transform[2][0] = -p1[1] * PageWidth / (p2[1] - p1[1]);
	Transform[2][1] = p1[0] * PageHeight / (p1[0] - p2[0]);
	break;
  };
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
IP_input_absolute(int num_params, param_t *params)
{
  if (num_params == 0)
  {
    P1[0] = 0;
    P1[1] = 0;
    P2[0] = PageWidth / 72.0 * 1016.0 - 1.0;
    P2[1] = PageHeight / 72.0 * 1016.0 - 1.0;
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

  update_transform();
}


void
IR_input_relative(int num_params, param_t *params)
{
  if (num_params == 0)
  {
    P1[0] = 0;
    P1[1] = 0;
    P2[0] = PageWidth / 72.0 * 1016.0 - 1.0;
    P2[1] = PageHeight / 72.0 * 1016.0 - 1.0;
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

  update_transform();
}


void
IW_input_window(int num_params, param_t *params)
{
}


void
PG_advance_page(int num_params, param_t *params)
{
  fputs("grestore\n", OutputFile);
  fputs("showpage\n", OutputFile);
  fputs("%%EndPage\n", OutputFile);

  PageCount ++;
  fprintf(OutputFile, "%%%%Page: %d\n", PageCount);
  fputs("gsave\n", OutputFile);
  fprintf(OutputFile, "%.1f %.1f translate\n", PageLeft, PageBottom);
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
 * End of "$Id: hpgl-config.c,v 1.1 1996/08/24 19:41:24 mike Exp $".
 */
