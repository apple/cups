/*
 * "$Id: hpgl-polygon.c,v 1.6 1999/03/21 21:12:18 mike Exp $"
 *
 *   HP-GL/2 polygon routines for the Common UNIX Printing System (CUPS).
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
 * 'EA_edge_rect_absolute()' - Draw a rectangle.
 */

void
EA_edge_rect_absolute(int     num_params,	/* I - Number of parameters */
                      param_t *params)		/* I - Parameters */
{
  float	x, y;		/* Transformed coordinates */


  if (num_params < 2)
    return;

  x = Transform[0][0] * params[0].value.number +
      Transform[0][1] * params[1].value.number +
      Transform[0][2];
  y = Transform[1][0] * params[0].value.number +
      Transform[1][1] * params[1].value.number +
      Transform[1][2];

  if (!PolygonMode)
    Outputf("MP\n");

  Outputf("%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);
  Outputf("%.3f %.3f LI\n", PenPosition[0], y);
  Outputf("%.3f %.3f LI\n", x, y);
  Outputf("%.3f %.3f LI\n", x, PenPosition[1]);

  Outputf("CP\n");
  if (!PolygonMode)
    Outputf("ST\n");
}


/*
 * 'EP_edge_polygon()' - Stroke the edges of a polygon.
 */

void
EP_edge_polygon(int     num_params,	/* I - Number of parameters */
                param_t *params)	/* I - Parameters */
{
  (void)num_params;
  (void)params;

  Outputf("ST\n");
}


/*
 * 'ER_edge_rect_relative()' - Draw a rectangle relative to the current
 *                             pen position.
 */

void
ER_edge_rect_relative(int     num_params,	/* I - Number of parameters */
                      param_t *params)		/* I - Parameters */
{
  float x, y;		/* Transformed coordinates */


  if (num_params < 2)
    return;

  x = Transform[0][0] * params[0].value.number +
      Transform[0][1] * params[1].value.number +
      PenPosition[0];
  y = Transform[1][0] * params[0].value.number +
      Transform[1][1] * params[1].value.number +
      PenPosition[1];

  if (!PolygonMode)
    Outputf("MP\n");

  Outputf("%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);
  Outputf("%.3f %.3f LI\n", PenPosition[0], y);
  Outputf("%.3f %.3f LI\n", x, y);
  Outputf("%.3f %.3f LI\n", x, PenPosition[1]);

  Outputf("CP\n");
  if (!PolygonMode)
    Outputf("ST\n");
}


/*
 * 'EW_edge_wedge()' - Draw a pie wedge.
 */

void
EW_edge_wedge(int     num_params,	/* I - Number of parameters */
              param_t *params)		/* I - Parameters */
{
  float x, y;				/* Transformed coordinates */
  float start, end,			/* Start and end of arc */
	theta,				/* Current angle */
	dt,				/* Step between points */
	radius;				/* Radius of arc */


  if (num_params < 3)
    return;

  radius = params[0].value.number;
  start  = params[1].value.number;
  end    = start + params[2].value.number;

  if (num_params > 3)
    dt = (float)fabs(params[3].value.number);
  else
    dt = 5.0f;

  if (!PolygonMode)
    Outputf("MP\n");

  Outputf("%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);

  if (start < end)
    for (theta = start + dt; theta < end; theta += dt)
    {
      x = (float)(PenPosition[0] +
                  radius * cos(M_PI * theta / 180.0) * Transform[0][0] +
                  radius * sin(M_PI * theta / 180.0) * Transform[0][1]);
      y = (float)(PenPosition[1] +
                  radius * cos(M_PI * theta / 180.0) * Transform[1][0] +
                  radius * sin(M_PI * theta / 180.0) * Transform[1][1]);

      Outputf("%.3f %.3f LI\n", x, y);
    }
  else
    for (theta = start - dt; theta > end; theta -= dt)
    {
      x = (float)(PenPosition[0] +
                  radius * cos(M_PI * theta / 180.0) * Transform[0][0] +
                  radius * sin(M_PI * theta / 180.0) * Transform[0][1]);
      y = (float)(PenPosition[1] +
                  radius * cos(M_PI * theta / 180.0) * Transform[1][0] +
                  radius * sin(M_PI * theta / 180.0) * Transform[1][1]);

      Outputf("%.3f %.3f LI\n", x, y);
    }

  x = (float)(PenPosition[0] +
              radius * cos(M_PI * end / 180.0) * Transform[0][0] +
              radius * sin(M_PI * end / 180.0) * Transform[0][1]);
  y = (float)(PenPosition[1] +
              radius * cos(M_PI * end / 180.0) * Transform[1][0] +
              radius * sin(M_PI * end / 180.0) * Transform[1][1]);
  Outputf("%.3f %.3f LI\n", x, y);

  Outputf("CP\n");
  if (!PolygonMode)
    Outputf("ST\n");
}


/*
 * 'FP_fill_polygon()' - Fill a polygon.
 */

void
FP_fill_polygon(int     num_params,	/* I - Number of parameters */
                param_t *params)	/* I - Parameters */
{
  (void)num_params;
  (void)params;

  Outputf("FI\n");
}


/*
 * 'PM_polygon_mode()' - Set the polygon drawing mode.
 */

void
PM_polygon_mode(int     num_params,	/* I - Number of parameters */
                param_t *params)	/* I - Parameters */
{
  if (num_params == 0 ||
      params[0].value.number == 0)
  {
    Outputf("MP\n");
    Outputf("%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);
    PolygonMode = 1;
  }
  else if (params[0].value.number == 2)
    PolygonMode = 0;
}


/*
 * 'RA_fill_rect_absolute()' - Fill a rectangle.
 */

void
RA_fill_rect_absolute(int     num_params,	/* I - Number of parameters */
                      param_t *params)		/* I - Parameters */
{
  float x, y;			/* Transformed coordinates */


  if (num_params < 2)
    return;

  x = Transform[0][0] * params[0].value.number +
      Transform[0][1] * params[1].value.number +
      Transform[0][2];
  y = Transform[1][0] * params[0].value.number +
      Transform[1][1] * params[1].value.number +
      Transform[1][2];

  if (!PolygonMode)
    Outputf("MP\n");

  Outputf("%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);
  Outputf("%.3f %.3f LI\n", PenPosition[0], y);
  Outputf("%.3f %.3f LI\n", x, y);
  Outputf("%.3f %.3f LI\n", x, PenPosition[1]);

  Outputf("CP\n");
  if (!PolygonMode)
    Outputf("FI\n");
}


/*
 * 'RR_fill_rect_relative()' - Fill a rectangle relative to the current
 *                             pen position.
 */

void
RR_fill_rect_relative(int     num_params,	/* I - Number of parameters */
                      param_t *params)		/* I - Parameters */
{
  float x, y;			/* Transformed coordinates */


  if (num_params < 2)
    return;

  x = Transform[0][0] * params[0].value.number +
      Transform[0][1] * params[1].value.number +
      PenPosition[0];
  y = Transform[1][0] * params[0].value.number +
      Transform[1][1] * params[1].value.number +
      PenPosition[1];

  if (!PolygonMode)
    Outputf("MP\n");

  Outputf("%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);
  Outputf("%.3f %.3f LI\n", PenPosition[0], y);
  Outputf("%.3f %.3f LI\n", x, y);
  Outputf("%.3f %.3f LI\n", x, PenPosition[1]);

  Outputf("CP\n");
  if (!PolygonMode)
    Outputf("FI\n");
}


/*
 * 'WG_fill_wedge()' - Fill a pie wedge.
 */

void
WG_fill_wedge(int     num_params,	/* I - Number of parameters */
              param_t *params)		/* I - Parameters */
{
  float x, y;				/* Transformed coordinates */
  float start, end,			/* Start and end angles */
	theta,				/* Current angle */
	dt,				/* Step between points */
	radius;				/* Radius of arc */


  if (num_params < 3)
    return;

  radius = params[0].value.number;
  start  = params[1].value.number;
  end    = start + params[2].value.number;

  if (num_params > 3)
    dt = (float)fabs(params[3].value.number);
  else
    dt = 5.0;

  if (!PolygonMode)
    Outputf("MP\n");

  Outputf("%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);

  if (start < end)
    for (theta = start + dt; theta < end; theta += dt)
    {
      x = (float)(PenPosition[0] +
                  radius * cos(M_PI * theta / 180.0) * Transform[0][0] +
                  radius * sin(M_PI * theta / 180.0) * Transform[0][1]);
      y = (float)(PenPosition[1] +
                  radius * cos(M_PI * theta / 180.0) * Transform[1][0] +
                  radius * sin(M_PI * theta / 180.0) * Transform[1][1]);

      Outputf("%.3f %.3f LI\n", x, y);
    }
  else
    for (theta = start - dt; theta > end; theta -= dt)
    {
      x = (float)(PenPosition[0] +
                  radius * cos(M_PI * theta / 180.0) * Transform[0][0] +
                  radius * sin(M_PI * theta / 180.0) * Transform[0][1]);
      y = (float)(PenPosition[1] +
                  radius * cos(M_PI * theta / 180.0) * Transform[1][0] +
                  radius * sin(M_PI * theta / 180.0) * Transform[1][1]);

      Outputf("%.3f %.3f LI\n", x, y);
    }

  x = (float)(PenPosition[0] +
              radius * cos(M_PI * end / 180.0) * Transform[0][0] +
              radius * sin(M_PI * end / 180.0) * Transform[0][1]);
  y = (float)(PenPosition[1] +
              radius * cos(M_PI * end / 180.0) * Transform[1][0] +
              radius * sin(M_PI * end / 180.0) * Transform[1][1]);
  Outputf("%.3f %.3f LI\n", x, y);

  Outputf("CP\n");
  if (!PolygonMode)
    Outputf("FI\n");
}


/*
 * End of "$Id: hpgl-polygon.c,v 1.6 1999/03/21 21:12:18 mike Exp $".
 */
