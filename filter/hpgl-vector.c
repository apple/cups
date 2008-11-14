/*
 * "$Id: hpgl-vector.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   HP-GL/2 vector routines for the Common UNIX Printing System (CUPS).
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
 *   AA_arc_absolute()    - Draw an arc.
 *   AR_arc_relative()    - Draw an arc relative to the current pen
 *   AT_arc_absolute3()   - Draw an arc using 3 points.
 *   CI_circle()          - Draw a circle.
 *   PA_plot_absolute()   - Plot a line using absolute coordinates.
 *   PD_pen_down()        - Start drawing.
 *   PE_polygon_encoded() - Draw an encoded polyline.
 *   PR_plot_relative()   - Plot a line using relative coordinates.
 *   PU_pen_up()          - Stop drawing.
 *   RT_arc_relative3()   - Draw an arc through 3 points relative to the
 *   decode_number()      - Decode an encoded number.
 *   plot_points()        - Plot the specified points.
 */

/*
 * Include necessary headers...
 */

#include "hpgltops.h"


/*
 * Local functions...
 */

static double	decode_number(unsigned char **, int, double);
static void	plot_points(int, param_t *);


/*
 * 'AA_arc_absolute()' - Draw an arc.
 */

void
AA_arc_absolute(int     num_params,	/* I - Number of parameters */
                param_t *params)	/* I - Parameters */
{
  float x, y,				/* Transformed coordinates */
	dx, dy;				/* Distance from current pen */
  float start, end,			/* Start and end angles */
	theta,				/* Current angle */
	dt,				/* Step between points */
	radius;				/* Radius of arc */


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

  start = (float)(180.0 * atan2(dy, dx) / M_PI);
  if (start < 0.0)
    start += 360.0f;

  end    = start + params[2].value.number;
  radius = (float)hypot(dx, dy);

  if (PenDown)
  {
    if (num_params > 3 && params[3].value.number > 0.0)
      dt = (float)fabs(params[3].value.number);
    else
      dt = 5.0;

    if (!PolygonMode)
      Outputf("MP\n");

    PenValid = 1;

    Outputf("%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);

    if (start < end)
      for (theta = start + dt; theta < end; theta += dt)
      {
	PenPosition[0] = (float)(x + radius * cos(M_PI * theta / 180.0));
	PenPosition[1] = (float)(y + radius * sin(M_PI * theta / 180.0));

	Outputf("%.3f %.3f LI\n", PenPosition[0], PenPosition[1]);
      }
    else
      for (theta = start - dt; theta > end; theta -= dt)
      {
	PenPosition[0] = (float)(x + radius * cos(M_PI * theta / 180.0));
	PenPosition[1] = (float)(y + radius * sin(M_PI * theta / 180.0));

	Outputf("%.3f %.3f LI\n", PenPosition[0], PenPosition[1]);
      }
  }

  PenPosition[0] = (float)(x + radius * cos(M_PI * end / 180.0));
  PenPosition[1] = (float)(y + radius * sin(M_PI * end / 180.0));

  if (PenDown)
  {
    Outputf("%.3f %.3f LI\n", PenPosition[0], PenPosition[1]);

    if (!PolygonMode)
      Outputf("ST\n");
  }
}


/*
 * 'AR_arc_relative()' - Draw an arc relative to the current pen
 *                       position.
 */

void
AR_arc_relative(int     num_params,	/* I - Number of parameters */
                param_t *params)	/* I - Parameters */
{
  float x, y,				/* Transformed coordinates */
	dx, dy;				/* Distance from current pen */
  float start, end,			/* Start and end angles */
	theta,				/* Current angle */
	dt,				/* Step between points */
	radius;				/* Radius of arc */


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

  start = (float)(180.0 * atan2(dy, dx) / M_PI);
  if (start < 0.0)
    start += 360.0f;

  end    = start + params[2].value.number;
  radius = (float)hypot(dx, dy);

  if (PenDown)
  {
    if (num_params > 3 && params[3].value.number > 0.0)
      dt = (float)fabs(params[3].value.number);
    else
      dt = 5.0;

    if (!PolygonMode)
      Outputf("MP\n");

    PenValid = 1;

    Outputf("%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);

    if (start < end)
      for (theta = start + dt; theta < end; theta += dt)
      {
	PenPosition[0] = (float)(x + radius * cos(M_PI * theta / 180.0));
	PenPosition[1] = (float)(y + radius * sin(M_PI * theta / 180.0));

	Outputf("%.3f %.3f LI\n", PenPosition[0], PenPosition[1]);
      }
    else
      for (theta = start - dt; theta > end; theta -= dt)
      {
	PenPosition[0] = (float)(x + radius * cos(M_PI * theta / 180.0));
	PenPosition[1] = (float)(y + radius * sin(M_PI * theta / 180.0));

	Outputf("%.3f %.3f LI\n", PenPosition[0], PenPosition[1]);
      }
  }

  PenPosition[0] = (float)(x + radius * cos(M_PI * end / 180.0));
  PenPosition[1] = (float)(y + radius * sin(M_PI * end / 180.0));

  if (PenDown)
  {
    Outputf("%.3f %.3f LI\n", PenPosition[0], PenPosition[1]);

    if (!PolygonMode)
      Outputf("ST\n");
  }
}


/*
 * 'AT_arc_absolute3()' - Draw an arc using 3 points.
 *
 * Note:
 *
 *   Currently this only draws two line segments through the
 *   specified points.
 */

void
AT_arc_absolute3(int     num_params,	/* I - Number of parameters */
                 param_t *params)	/* I - Parameters */
{
  if (num_params < 4)
    return;

  if (PenDown)
  {
    if (!PolygonMode)
      Outputf("MP\n");

    PenValid = 1;

    Outputf("%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);

    PenPosition[0] = Transform[0][0] * params[0].value.number +
                     Transform[0][1] * params[1].value.number +
                     Transform[0][2];
    PenPosition[1] = Transform[1][0] * params[0].value.number +
                     Transform[1][1] * params[1].value.number +
                     Transform[1][2];

    Outputf("%.3f %.3f LI\n", PenPosition[0], PenPosition[1]);
  }

  PenPosition[0] = Transform[0][0] * params[2].value.number +
                   Transform[0][1] * params[3].value.number +
                   Transform[0][2];
  PenPosition[1] = Transform[1][0] * params[2].value.number +
                   Transform[1][1] * params[3].value.number +
                   Transform[1][2];

  if (PenDown)
  {
    Outputf("%.3f %.3f LI\n", PenPosition[0], PenPosition[1]);

    if (!PolygonMode)
      Outputf("ST\n");
  }
}


/*
 * 'CI_circle()' - Draw a circle.
 */

void
CI_circle(int     num_params,	/* I - Number of parameters */
          param_t *params)	/* I - Parameters */
{
  float x, y;			/* Transformed coordinates */
  float theta,			/* Current angle */
	dt,			/* Step between points */
	radius;			/* Radius of circle */


  if (num_params < 1)
    return;

  if (!PenDown)
    return;

  radius = params[0].value.number;

  if (num_params > 1)
    dt = (float)fabs(params[1].value.number);
  else
    dt = 5.0;

  if (!PolygonMode)
    Outputf("MP\n");

  PenValid = 1;

  for (theta = 0.0; theta < 360.0; theta += dt)
  {
    x = (float)(PenPosition[0] +
                radius * cos(M_PI * theta / 180.0) * Transform[0][0] +
                radius * sin(M_PI * theta / 180.0) * Transform[0][1]);
    y = (float)(PenPosition[1] +
                radius * cos(M_PI * theta / 180.0) * Transform[1][0] +
                radius * sin(M_PI * theta / 180.0) * Transform[1][1]);

    Outputf("%.3f %.3f %s\n", x, y, theta == 0.0 ? "MO" : "LI");
  }

  Outputf("CP\n");
  if (!PolygonMode)
    Outputf("ST\n");
}


/*
 * 'PA_plot_absolute()' - Plot a line using absolute coordinates.
 */

void
PA_plot_absolute(int     num_params,	/* I - Number of parameters */
                 param_t *params)	/* I - Parameters */
{
  PenMotion = 0;

  if (num_params > 1)
    plot_points(num_params, params);
}


/*
 * 'PD_pen_down()' - Start drawing.
 */

void
PD_pen_down(int     num_params,		/* I - Number of parameters */
            param_t *params)		/* I - Parameters */
{
  PenDown = 1;

  if (num_params > 1)
    plot_points(num_params, params);
}


/*
 * 'PE_polygon_encoded()' - Draw an encoded polyline.
 */

void
PE_polyline_encoded(int     num_params,	/* I - Number of parameters */
                    param_t *params)	/* I - Parameters */
{
  unsigned char	*s;			/* Pointer into string */
  int		temp,			/* Temporary value */
		base_bits,		/* Data bits per byte */
		draw,			/* Draw or move */
		abscoords;		/* Use absolute coordinates */
  double	tx, ty,			/* Transformed coordinates */
		x, y,			/* Raw coordinates */
		frac_bits;		/* Multiplier for encoded number */


  base_bits = 6;
  frac_bits = 1.0;
  draw      = PenDown;
  abscoords = 0;

  if (num_params == 0)
    return;

  if (!PolygonMode)
  {
    Outputf("MP\n");
    PenValid = 0;
  }

  if (!PenValid)
  {
    Outputf("%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);
    PenValid = 1;
  }

  for (s = (unsigned char *)params[0].value.string; *s != '\0';)
    switch (*s)
    {
      case '7' :
          s ++;
          base_bits = 5;

#ifdef DEBUG
          fputs("DEBUG:     7-bit\n", stderr);
#endif /* DEBUG */

          Outputf("%% PE: 7-bit\n");
          break;
      case ':' :	/* Select pen */
          s ++;
          temp = (int)decode_number(&s, base_bits, 1.0) - 1;
	  if (temp < 0 || temp >= PenCount)
	  {
	    fprintf(stderr, "DEBUG: Bad pen number %d in PE\n", temp + 1);
	    return;
	  }

          PenNumber = temp;

#ifdef DEBUG
          fprintf(stderr, "DEBUG:     set pen #%d\n", PenNumber + 1);
#endif /* DEBUG */

          Outputf("%% PE: set pen #%d\n", PenNumber + 1);

	  if (PageDirty)
	    printf("%.3f %.3f %.3f %.2f SP\n", Pens[PenNumber].rgb[0],
		   Pens[PenNumber].rgb[1], Pens[PenNumber].rgb[2],
		   Pens[PenNumber].width * PenScaling);
          break;
      case '<' :	/* Next coords are a move-to */
          draw = 0;
          s ++;

#ifdef DEBUG
          fputs("DEBUG:     moveto\n", stderr);
#endif /* DEBUG */

	  Outputf("%% PE: moveto\n");
          break;
      case '>' :	/* Set fractional bits */
          s ++;
          temp      = (int)decode_number(&s, base_bits, 1.0);
          frac_bits = 1.0 / (1 << temp);

#ifdef DEBUG
          fprintf(stderr, "DEBUG:     set fractional bits %d\n", temp);
#endif /* DEBUG */

          Outputf("%% PE: set fractional bits %d\n", temp);
          break;
      case '=' :	/* Next coords are absolute */
          s ++;
          abscoords = 1;

#ifdef DEBUG
          fputs("DEBUG:     absolute\n", stderr);
#endif /* DEBUG */

          Outputf("%% PE: absolute\n");
          break;
      default :
          if (*s >= 63)
          {
           /*
            * Coordinate...
            */

            x = decode_number(&s, base_bits, frac_bits);
            y = decode_number(&s, base_bits, frac_bits);

#ifdef DEBUG
            fprintf(stderr, "DEBUG:     coords %.3f %.3f\n", x, y);
#endif /* DEBUG */

            Outputf("%% PE: coords %.3f %.3f\n", x, y);

            if (abscoords)
            {
	      tx = Transform[0][0] * x + Transform[0][1] * y +
        	   Transform[0][2];
	      ty = Transform[1][0] * x + Transform[1][1] * y +
        	   Transform[1][2];
	    }
	    else if (x == 0.0 && y == 0.0)
	    {
	      draw = 1;
	      continue;
	    }
	    else
	    {
	      tx = Transform[0][0] * x + Transform[0][1] * y +
        	   PenPosition[0];
	      ty = Transform[1][0] * x + Transform[1][1] * y +
        	   PenPosition[1];
	    }

            if (draw)
	    {
	      if (fabs(PenPosition[0] - tx) > 0.001 ||
        	  fabs(PenPosition[1] - ty) > 0.001)
        	Outputf("%.3f %.3f LI\n", tx, ty);
            }
	    else
              Outputf("%.3f %.3f MO\n", tx, ty);

	    PenPosition[0] = (float)tx;
	    PenPosition[1] = (float)ty;

	    draw           = 1;
	    abscoords      = 0;
          }
          else
          {
           /*
            * Junk - ignore...
            */

            if (*s != '\n' && *s != '\r')
              fprintf(stderr, "WARNING: ignoring illegal PE char \'%c\'...\n", *s);
            s ++;
          }
          break;
    }

  if (!PolygonMode)
    Outputf("ST\n");
}


/*
 * 'PR_plot_relative()' - Plot a line using relative coordinates.
 */

void
PR_plot_relative(int     num_params,	/* I - Number of parameters */
                 param_t *params)	/* I - Parameters */
{
  PenMotion = 1;

  if (num_params > 1)
    plot_points(num_params, params);
}


/*
 * 'PU_pen_up()' - Stop drawing.
 */

void
PU_pen_up(int     num_params,	/* I - Number of parameters */
          param_t *params)	/* I - Parameters */
{
  PenDown = 0;

  if (num_params > 1)
    plot_points(num_params, params);
}


/*
 * 'RT_arc_relative3()' - Draw an arc through 3 points relative to the
 *                        current pen position.
 *
 * Note:
 *
 *   This currently only draws two line segments through the specified
 *   points.
 */

void
RT_arc_relative3(int     num_params,	/* I - Number of parameters */
                 param_t *params)	/* I - Parameters */
{
  if (num_params < 4)
    return;

  if (PenDown)
  {
    if (!PolygonMode)
      Outputf("MP\n");

    PenValid = 1;

    Outputf("%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);

    PenPosition[0] = Transform[0][0] * params[0].value.number +
                     Transform[0][1] * params[1].value.number +
                     PenPosition[0];
    PenPosition[1] = Transform[1][0] * params[0].value.number +
                     Transform[1][1] * params[1].value.number +
                     PenPosition[1];

    Outputf("%.3f %.3f LI\n", PenPosition[0], PenPosition[1]);
  }

  PenPosition[0] = Transform[0][0] * params[2].value.number +
                   Transform[0][1] * params[3].value.number +
                   PenPosition[0];
  PenPosition[1] = Transform[1][0] * params[2].value.number +
                   Transform[1][1] * params[3].value.number +
                   PenPosition[1];

  if (PenDown)
  {
    Outputf("%.3f %.3f LI\n", PenPosition[0], PenPosition[1]);

    if (!PolygonMode)
      Outputf("ST\n");
  }
}


/*
 * 'decode_number()' - Decode an encoded number.
 */

static double				/* O - Value */
decode_number(unsigned char **s,	/* IO - String to decode */
              int           base_bits,	/* I - Number of data bits per byte */
	      double        frac_bits)	/* I - Multiplier for fractional data */
{
  double	temp,		/* Current value */
		shift;		/* Multiplier */
  int		sign;		/* Sign of result */


  sign = 0;

  if (base_bits == 5)
  {
    for (temp = 0.0, shift = frac_bits * 0.5; **s != '\0'; (*s) ++)
      if (**s >= 95 && **s < 127)
      {
        if (sign == 0)
        {
          if ((**s - 95) & 1)
            sign = -1;
          else
            sign = 1;

          temp += ((**s - 95) & ~1) * shift;
        }
        else
          temp += (**s - 95) * shift;
        break;
      }
      else if (**s < 63)
      {
        if (**s != '\r' && **s != '\n')
          fprintf(stderr, "DEBUG: Bad PE character 0x%02X!\n", **s);

        continue;
      }
      else
      {
        if (sign == 0)
        {
          if ((**s - 63) & 1)
            sign = -1;
          else
            sign = 1;

          temp += ((**s - 63) & ~1) * shift;
        }
        else
          temp += (**s - 63) * shift;

	shift *= 32.0;
      }
  }
  else
  {
    for (temp = 0.0, shift = frac_bits * 0.5; **s != '\0'; (*s) ++)
      if (**s >= 191 && **s < 255)
      {
        if (sign == 0)
        {
          if ((**s - 191) & 1)
            sign = -1;
          else
            sign = 1;

          temp += ((**s - 191) & ~1) * shift;
        }
        else
          temp += (**s - 191) * shift;
        break;
      }
      else if (**s < 63)
      {
        if (**s != '\r' && **s != '\n')
          fprintf(stderr, "DEBUG: Bad PE character 0x%02X!\n", **s);

        continue;
      }
      else
      {
        if (sign == 0)
        {
          if ((**s - 63) & 1)
            sign = -1;
          else
            sign = 1;

          temp += ((**s - 63) & ~1) * shift;
        }
        else
          temp += (**s - 63) * shift;

        shift *= 64.0;
      }
  }

  (*s) ++;

  return (temp * sign);
}


/*
 * 'plot_points()' - Plot the specified points.
 */

static void
plot_points(int     num_params,	/* I - Number of parameters */
            param_t *params)	/* I - Parameters */
{
  int	i;			/* Looping var */
  float	x, y;			/* Transformed coordinates */


  if (PenDown)
  {
    if (!PolygonMode)
    {
      Outputf("MP\n");
      Outputf("%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);

      PenValid = 1;
    }
  }

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
    }

    if (PenDown)
    {
      if (PolygonMode && i == 0)
        Outputf("%.3f %.3f MO\n", x, y);
      else if (fabs(PenPosition[0] - x) > 0.001 ||
               fabs(PenPosition[1] - y) > 0.001)
        Outputf("%.3f %.3f LI\n", x, y);
    }

    PenPosition[0] = x;
    PenPosition[1] = y;
  }

  if (PenDown)
  {
    if (!PolygonMode)
      Outputf("ST\n");
  }
}


/*
 * End of "$Id: hpgl-vector.c 6649 2007-07-11 21:46:42Z mike $".
 */
