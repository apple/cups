/*
 * "$Id: hpgl-polygon.c,v 1.3 1998/09/10 20:00:01 mike Exp $"
 *
 *   HPGL polygon routines for espPrint, a collection of printer drivers.
 *
 *   Copyright 1993-1996 by Easy Software Products
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
 *   $Log: hpgl-polygon.c,v $
 *   Revision 1.3  1998/09/10 20:00:01  mike
 *   Wasn't doing an initial "move" when starting a polygon.
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
EA_edge_rect_absolute(int num_params, param_t *params)
{
  float x, y;


  if (num_params < 2)
    return;

  x = Transform[0][0] * params[0].value.number +
      Transform[0][1] * params[1].value.number +
      Transform[0][2];
  y = Transform[1][0] * params[0].value.number +
      Transform[1][1] * params[1].value.number +
      Transform[1][2];

  if (!PolygonMode)
    fputs("MP\n", OutputFile);

  fprintf(OutputFile, "%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);
  fprintf(OutputFile, "%.3f %.3f LI\n", PenPosition[0], y);
  fprintf(OutputFile, "%.3f %.3f LI\n", x, y);
  fprintf(OutputFile, "%.3f %.3f LI\n", x, PenPosition[1]);

  fputs("CP\n", OutputFile);
  if (!PolygonMode)
    fputs("ST\n", OutputFile);

  PageDirty = 1;
}


void
EP_edge_polygon(int num_params, param_t *params)
{
  fputs("ST\n", OutputFile);

  PageDirty = 1;
}


void
ER_edge_rect_relative(int num_params, param_t *params)
{
  float x, y;


  if (num_params < 2)
    return;

  x = Transform[0][0] * params[0].value.number +
      Transform[0][1] * params[1].value.number +
      PenPosition[0];
  y = Transform[1][0] * params[0].value.number +
      Transform[1][1] * params[1].value.number +
      PenPosition[1];

  if (!PolygonMode)
    fputs("MP\n", OutputFile);

  fprintf(OutputFile, "%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);
  fprintf(OutputFile, "%.3f %.3f LI\n", PenPosition[0], y);
  fprintf(OutputFile, "%.3f %.3f LI\n", x, y);
  fprintf(OutputFile, "%.3f %.3f LI\n", x, PenPosition[1]);

  fputs("CP\n", OutputFile);
  if (!PolygonMode)
    fputs("ST\n", OutputFile);

  PageDirty = 1;
}


void
EW_edge_wedge(int num_params, param_t *params)
{
  float x, y;
  float start, end, theta, dt, radius;


  if (num_params < 3)
    return;

  radius = params[0].value.number;
  start  = params[1].value.number;
  end    = start + params[2].value.number;

  if (num_params > 3)
    dt = fabs(params[3].value.number);
  else
    dt = 5.0;

  if (!PolygonMode)
    fputs("MP\n", OutputFile);

  fprintf(OutputFile, "%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);

  if (start < end)
    for (theta = start + dt; theta < end; theta += dt)
    {
      x = PenPosition[0] +
          radius * cos(M_PI * theta / 180.0) * Transform[0][0] +
          radius * sin(M_PI * theta / 180.0) * Transform[0][1];
      y = PenPosition[1] +
          radius * cos(M_PI * theta / 180.0) * Transform[1][0] +
          radius * sin(M_PI * theta / 180.0) * Transform[1][1];

      fprintf(OutputFile, "%.3f %.3f LI\n", x, y);
    }
  else
    for (theta = start - dt; theta > end; theta -= dt)
    {
      x = PenPosition[0] +
          radius * cos(M_PI * theta / 180.0) * Transform[0][0] +
          radius * sin(M_PI * theta / 180.0) * Transform[0][1];
      y = PenPosition[1] +
          radius * cos(M_PI * theta / 180.0) * Transform[1][0] +
          radius * sin(M_PI * theta / 180.0) * Transform[1][1];

      fprintf(OutputFile, "%.3f %.3f LI\n", x, y);
    };

  x = PenPosition[0] +
      radius * cos(M_PI * end / 180.0) * Transform[0][0] +
      radius * sin(M_PI * end / 180.0) * Transform[0][1];
  y = PenPosition[1] +
      radius * cos(M_PI * end / 180.0) * Transform[1][0] +
      radius * sin(M_PI * end / 180.0) * Transform[1][1];
  fprintf(OutputFile, "%.3f %.3f LI\n", x, y);

  fputs("CP\n", OutputFile);
  if (!PolygonMode)
    fputs("ST\n", OutputFile);

  PageDirty = 1;
}


void
FP_fill_polygon(int num_params, param_t *params)
{
  fputs("FI\n", OutputFile);

  PageDirty = 1;
}


void
PM_polygon_mode(int num_params, param_t *params)
{
  if (num_params == 0 ||
      params[0].value.number == 0)
  {
    fputs("MP\n", OutputFile);
    fprintf(OutputFile, "%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);
    PolygonMode = 1;
  }
  else if (params[0].value.number == 2)
    PolygonMode = 0;
}


void
RA_fill_rect_absolute(int num_params, param_t *params)
{
  float x, y;


  if (num_params < 2)
    return;

  x = Transform[0][0] * params[0].value.number +
      Transform[0][1] * params[1].value.number +
      Transform[0][2];
  y = Transform[1][0] * params[0].value.number +
      Transform[1][1] * params[1].value.number +
      Transform[1][2];

  if (!PolygonMode)
    fputs("MP\n", OutputFile);

  fprintf(OutputFile, "%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);
  fprintf(OutputFile, "%.3f %.3f LI\n", PenPosition[0], y);
  fprintf(OutputFile, "%.3f %.3f LI\n", x, y);
  fprintf(OutputFile, "%.3f %.3f LI\n", x, PenPosition[1]);

  fputs("CP\n", OutputFile);
  if (!PolygonMode)
    fputs("FI\n", OutputFile);

  PageDirty = 1;
}


void
RR_fill_rect_relative(int num_params, param_t *params)
{
  float x, y;


  if (num_params < 2)
    return;

  x = Transform[0][0] * params[0].value.number +
      Transform[0][1] * params[1].value.number +
      PenPosition[0];
  y = Transform[1][0] * params[0].value.number +
      Transform[1][1] * params[1].value.number +
      PenPosition[1];

  if (!PolygonMode)
    fputs("MP\n", OutputFile);

  fprintf(OutputFile, "%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);
  fprintf(OutputFile, "%.3f %.3f LI\n", PenPosition[0], y);
  fprintf(OutputFile, "%.3f %.3f LI\n", x, y);
  fprintf(OutputFile, "%.3f %.3f LI\n", x, PenPosition[1]);

  fputs("CP\n", OutputFile);
  if (!PolygonMode)
    fputs("FI\n", OutputFile);

  PageDirty = 1;
}


void
WG_fill_wedge(int num_params, param_t *params)
{
  float x, y;
  float start, end, theta, dt, radius;


  if (num_params < 3)
    return;

  radius = params[0].value.number;
  start  = params[1].value.number;
  end    = start + params[2].value.number;

  if (num_params > 3)
    dt = fabs(params[3].value.number);
  else
    dt = 5.0;

  if (!PolygonMode)
    fputs("MP\n", OutputFile);

  fprintf(OutputFile, "%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);

  if (start < end)
    for (theta = start + dt; theta < end; theta += dt)
    {
      x = PenPosition[0] +
          radius * cos(M_PI * theta / 180.0) * Transform[0][0] +
          radius * sin(M_PI * theta / 180.0) * Transform[0][1];
      y = PenPosition[1] +
          radius * cos(M_PI * theta / 180.0) * Transform[1][0] +
          radius * sin(M_PI * theta / 180.0) * Transform[1][1];

      fprintf(OutputFile, "%.3f %.3f LI\n", x, y);
    }
  else
    for (theta = start - dt; theta > end; theta -= dt)
    {
      x = PenPosition[0] +
          radius * cos(M_PI * theta / 180.0) * Transform[0][0] +
          radius * sin(M_PI * theta / 180.0) * Transform[0][1];
      y = PenPosition[1] +
          radius * cos(M_PI * theta / 180.0) * Transform[1][0] +
          radius * sin(M_PI * theta / 180.0) * Transform[1][1];

      fprintf(OutputFile, "%.3f %.3f LI\n", x, y);
    };

  x = PenPosition[0] +
      radius * cos(M_PI * end / 180.0) * Transform[0][0] +
      radius * sin(M_PI * end / 180.0) * Transform[0][1];
  y = PenPosition[1] +
      radius * cos(M_PI * end / 180.0) * Transform[1][0] +
      radius * sin(M_PI * end / 180.0) * Transform[1][1];
  fprintf(OutputFile, "%.3f %.3f LI\n", x, y);

  fputs("CP\n", OutputFile);
  if (!PolygonMode)
    fputs("FI\n", OutputFile);

  PageDirty = 1;
}


/*
 * End of "$Id: hpgl-polygon.c,v 1.3 1998/09/10 20:00:01 mike Exp $".
 */
