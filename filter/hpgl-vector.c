/*
 * "$Id: hpgl-vector.c,v 1.1 1996/08/24 19:41:24 mike Exp $"
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
 *   $Log: hpgl-vector.c,v $
 *   Revision 1.1  1996/08/24 19:41:24  mike
 *   Initial revision
 *
 */

/*
 * Include necessary headers...
 */

#include "hpgl2ps.h"


void
AA_arc_absolute(int num_params, param_t *params)
{
  float x, y, dx, dy;
  float start, end, theta, dt, radius;


  if (num_params < 3)
    return;

  x = Transform[0][0] * params[0].value.number +
      Transform[0][1] * params[1].value.number +
      Transform[0][2];
  y = Transform[1][0] * params[0].value.number +
      Transform[1][1] * params[1].value.number +
      Transform[1][2];

  dx = PenPosition[0] - x;
  dy = PenPosition[1] - y;

  start  = 180.0 * atan2(dx, dy) / M_PI;
  end    = start + params[2].value.number;
  radius = fhypot(dx, dy);

  if (PenDown)
  {
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
        PenPosition[0] = x +
                         radius * cos(M_PI * theta / 180.0) * Transform[0][0] +
                         radius * sin(M_PI * theta / 180.0) * Transform[0][1];
        PenPosition[1] = y +
        		 radius * cos(M_PI * theta / 180.0) * Transform[1][0] +
        		 radius * sin(M_PI * theta / 180.0) * Transform[1][1];

	fprintf(OutputFile, "%.3f %.3f LI\n", PenPosition[0], PenPosition[1]);
      }
    else
      for (theta = start - dt; theta > end; theta -= dt)
      {
        PenPosition[0] = x +
                         radius * cos(M_PI * theta / 180.0) * Transform[0][0] +
                         radius * sin(M_PI * theta / 180.0) * Transform[0][1];
        PenPosition[1] = y +
        		 radius * cos(M_PI * theta / 180.0) * Transform[1][0] +
        		 radius * sin(M_PI * theta / 180.0) * Transform[1][1];

	fprintf(OutputFile, "%.3f %.3f LI\n", PenPosition[0], PenPosition[1]);
      };
  };

  PenPosition[0] = x +
                   radius * cos(M_PI * end / 180.0) * Transform[0][0] +
                   radius * sin(M_PI * end / 180.0) * Transform[0][1];
  PenPosition[1] = y +
        	   radius * cos(M_PI * end / 180.0) * Transform[1][0] +
        	   radius * sin(M_PI * end / 180.0) * Transform[1][1];

  if (PenDown)
  {
    fprintf(OutputFile, "%.3f %.3f LI\n", PenPosition[0], PenPosition[1]);
    if (!PolygonMode)
      fputs("ST\n", OutputFile);
  };
}


void
AR_arc_relative(int num_params, param_t *params)
{
  float x, y, dx, dy;
  float start, end, theta, dt, radius;


  if (num_params < 3)
    return;

  x = Transform[0][0] * params[0].value.number +
      Transform[0][1] * params[1].value.number +
      PenPosition[0];
  y = Transform[1][0] * params[0].value.number +
      Transform[1][1] * params[1].value.number +
      PenPosition[1];

  dx = PenPosition[0] - x;
  dy = PenPosition[1] - y;

  start  = 180.0 * atan2(dx, dy) / M_PI;
  end    = start + params[2].value.number;
  radius = fhypot(dx, dy);

  if (PenDown)
  {
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
        PenPosition[0] = x +
                         radius * cos(M_PI * theta / 180.0) * Transform[0][0] +
                         radius * sin(M_PI * theta / 180.0) * Transform[0][1];
        PenPosition[1] = y +
        		 radius * cos(M_PI * theta / 180.0) * Transform[1][0] +
        		 radius * sin(M_PI * theta / 180.0) * Transform[1][1];

	fprintf(OutputFile, "%.3f %.3f LI\n", PenPosition[0], PenPosition[1]);
      }
    else
      for (theta = start - dt; theta > end; theta -= dt)
      {
        PenPosition[0] = x +
                         radius * cos(M_PI * theta / 180.0) * Transform[0][0] +
                         radius * sin(M_PI * theta / 180.0) * Transform[0][1];
        PenPosition[1] = y +
        		 radius * cos(M_PI * theta / 180.0) * Transform[1][0] +
        		 radius * sin(M_PI * theta / 180.0) * Transform[1][1];

	fprintf(OutputFile, "%.3f %.3f LI\n", PenPosition[0], PenPosition[1]);
      };
  };

  PenPosition[0] = x +
                   radius * cos(M_PI * end / 180.0) * Transform[0][0] +
                   radius * sin(M_PI * end / 180.0) * Transform[0][1];
  PenPosition[1] = y +
        	   radius * cos(M_PI * end / 180.0) * Transform[1][0] +
        	   radius * sin(M_PI * end / 180.0) * Transform[1][1];

  if (PenDown)
  {
    fprintf(OutputFile, "%.3f %.3f LI\n", PenPosition[0], PenPosition[1]);
    if (!PolygonMode)
      fputs("ST\n", OutputFile);
  };
}


void
AT_arc_absolute3(int num_params, param_t *params)
{
  if (num_params < 4)
    return;

  PenPosition[0] = Transform[0][0] * params[2].value.number +
                   Transform[0][1] * params[3].value.number +
                   Transform[0][2];
  PenPosition[1] = Transform[1][0] * params[2].value.number +
                   Transform[1][1] * params[3].value.number +
                   Transform[1][2];
}


void
CI_circle(int num_params, param_t *params)
{
  float x, y, dx, dy;
  float start, end, theta, dt, radius;


  if (num_params < 1)
    return;

  radius = params[0].value.number;

  if (num_params > 1)
    dt = fabs(params[3].value.number);
  else
    dt = 5.0;

  if (!PolygonMode)
    fputs("MP\n", OutputFile);

  for (theta = 0.0; theta < 360.0; theta += dt)
  {
    x = PenPosition[0] +
        radius * cos(M_PI * theta / 180.0) * Transform[0][0] +
        radius * sin(M_PI * theta / 180.0) * Transform[0][1];
    y = PenPosition[1] +
        radius * cos(M_PI * theta / 180.0) * Transform[1][0] +
        radius * sin(M_PI * theta / 180.0) * Transform[1][1];

    fprintf(OutputFile, "%.3f %.3f %s\n", x, y, theta == 0.0 ? "MO" : "LI");
  };

  fputs("CP\n", OutputFile);
  if (!PolygonMode)
    fputs("ST\n", OutputFile);
}


static void
plot_points(int num_params, param_t *params)
{
  int i;
  float x, y;


  if (PenDown)
  {
    if (!PolygonMode)
      fputs("MP\n", OutputFile);
    fprintf(OutputFile, "%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);
  };

  for (i = 0; i < num_params; i += 2)
  {
    if (PenMotion == 0)
    {
      x = Transform[0][0] * params[i + 0].value.number +
          Transform[0][1] * params[i + 1].value.number +
          Transform[0][2];
      y = Transform[1][0] * params[i + 0].value.number +
          Transform[1][1] * params[i + 1].value.number +
          Transform[1][2];
    }
    else
    {
      x = Transform[0][0] * params[i + 0].value.number +
          Transform[0][1] * params[i + 1].value.number +
          PenPosition[0];
      y = Transform[1][0] * params[i + 0].value.number +
          Transform[1][1] * params[i + 1].value.number +
          PenPosition[1];
    };

    if (PenDown)
      fprintf(OutputFile, "%.3f %.3f LI\n", x, y);

    PenPosition[0] = x;
    PenPosition[1] = y;
  };

  if (PenDown && !PolygonMode)
    fputs("ST\n", OutputFile);
}


void
PA_plot_absolute(int num_params, param_t *params)
{
  PenMotion = 0;

  if (num_params > 1)
    plot_points(num_params, params);
}


void
PD_pen_down(int num_params, param_t *params)
{
  PenDown = 1;

  if (num_params > 1)
    plot_points(num_params, params);
}


void
PE_polyline_encoded(int num_params, param_t *params)
{
}


void
PR_plot_relative(int num_params, param_t *params)
{
  PenMotion = 1;

  if (num_params > 1)
    plot_points(num_params, params);
}


void
PU_pen_up(int num_params, param_t *params)
{
  PenDown = 0;

  if (num_params > 1)
    plot_points(num_params, params);
}


void
RT_arc_relative3(int num_params, param_t *params)
{
  if (num_params < 4)
    return;

  PenPosition[0] = Transform[0][0] * params[2].value.number +
                   Transform[0][1] * params[3].value.number +
                   PenPosition[0];
  PenPosition[1] = Transform[1][0] * params[2].value.number +
                   Transform[1][1] * params[3].value.number +
                   PenPosition[1];
}


/*
 * End of "$Id: hpgl-vector.c,v 1.1 1996/08/24 19:41:24 mike Exp $".
 */

