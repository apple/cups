/*
 * "$Id: hpgl-char.c,v 1.5 1999/03/21 16:26:58 mike Exp $"
 *
 *   HP-GL/2 character processing for the Common UNIX Printing System (CUPS).
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
 * 'AD_define_alternate()' - Define the alternate font.
 */

void
AD_define_alternate(int     num_params,	/* I - Number of parameters */
                    param_t *params)	/* I - Parameters */
{
  int	i;				/* Looping var */
  int	typeface,			/* Typeface number */
	posture,			/* Posture number */
	weight;				/* Weight number */
  float	height;				/* Height/size of font */


 /*
  * Set default font attributes...
  */

  typeface = 48;
  posture  = 0;
  weight   = 0;
  height   = 11.5;

 /*
  * Loop through parameter value pairs...
  */

  for (i = 0; i < (num_params - 1); i += 2)
    switch ((int)params[i].value.number)
    {
      case 4 :
          height = params[i + 1].value.number;
          break;
      case 5 :
          posture = params[i + 1].value.number;
          break;
      case 6 :
          weight = params[i + 1].value.number;
          break;
      case 7 :
          typeface = params[i + 1].value.number;
          break;
    }

 /*
  * Define the font...
  */

  Outputf("/SA { /%s%s%s%s findfont %.1f scalefont setfont } def\n",
          typeface == 48 ? "Courier" : "Helvetica",
          (weight != 0 || posture != 0) ? "-" : "",
          weight != 0 ? "Bold" : "",
          posture != 0 ? "Oblique" : "",
          height);

  CharHeight[1] = height;
}


/*
 * 'CF_character_fill()' - Set whether or not to fill or outline characters.
 */

void
CF_character_fill(int     num_params,	/* I - Number of parameters */
                  param_t *params)	/* I - Parameters */
{
  if (num_params == 0)
    CharFillMode = 0;
  else
    CharFillMode = params[0].value.number;

  if (num_params == 2)
    CharPen = params[1].value.number;
}


/*
 * 'CP_character_plot()' - Move the current pen position for the given number
 *                         of columns and rows.
 */

void
CP_character_plot(int     num_params,
                  param_t *params)
{
  if (num_params < 2)
    return;

  switch (Rotation)
  {
    case 0:
	PenPosition[0] += params[0].value.number * 1.2 / CharHeight[CharFont];
	PenPosition[1] += params[1].value.number * CharHeight[CharFont];
	break;
    case 90:
	PenPosition[0] -= params[1].value.number * 1.2 / CharHeight[CharFont];
	PenPosition[1] += params[0].value.number * CharHeight[CharFont];
	break;
    case 180:
	PenPosition[0] -= params[0].value.number * 1.2 / CharHeight[CharFont];
	PenPosition[1] -= params[1].value.number * CharHeight[CharFont];
	break;
    case 270:
	PenPosition[0] += params[1].value.number * 1.2 / CharHeight[CharFont];
	PenPosition[1] -= params[0].value.number * CharHeight[CharFont];
	break;
  }
}


/*
 * 'DI_absolute_direction()' - Set the direction vector for text.
 */

void
DI_absolute_direction(int     num_params,	/* I - Number of parameters */
                      param_t *params)		/* I - Parameters */
{
  Outputf(CharFont == 0 ? "SS\n" : "SA\n");

  if (num_params == 2)
    Outputf("currentfont [ %f %f %f %f 0.0 0.0 ] makefont setfont\n",
            params[0].value.number, -params[1].value.number,
            params[1].value.number, params[0].value.number);
}


/*
 * 'DR_relative_direction()' - Set the relative direction vector for text.
 */

void
DR_relative_direction(int     num_params,	/* I - Number of parameters */
                      param_t *params)		/* I - Parameters */
{
  (void)num_params;
  (void)params;
}


/*
 * 'DT_define_label_term()' - Set the label string terminator.
 */

void
DT_define_label_term(int     num_params,	/* I - Number of parameters */
                     param_t *params)		/* I - Parameters */
{
  if (num_params == 0)
    StringTerminator = '\003';
  else
    StringTerminator = params[0].value.string[0];
}


/*
 * 'DV_define_variable_path()' - Define a path for text.
 */

void
DV_define_variable_path(int     num_params,	/* I - Number of parameters */
                        param_t *params)	/* I - Parameters */
{
  (void)num_params;
  (void)params;
}


/*
 * 'ES_extra_space()' - Set extra spacing (kerning) between characters.
 */

void
ES_extra_space(int     num_params,	/* I - Number of parameters */
               param_t *params)		/* I - Parameters */
{
  (void)num_params;
  (void)params;
}


/*
 * 'LB_label()' - Display a label string.
 */

void
LB_label(int     num_params,		/* I - Number of parameters */
         param_t *params)		/* I - Parameters */
{
  char	*s;				/* Pointer into string */


  if (num_params == 0)
    return;

  Outputf("gsave\n");
  Outputf("currentmiterlimit 1.0 setmiterlimit\n");
  Outputf("MP\n");
  Outputf("%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);

  Outputf("(");
  for (s = params[0].value.string; *s != '\0'; s ++)
    if (strchr("()\\", *s) != NULL)
      Outputf("\\%c", *s);
    else
      putc(*s, OutputFile);
  Outputf(") true charpath\n");

  if (CharFillMode != 1)
    Outputf("FI\n");
  if (CharFillMode == 1 || CharFillMode == 3)
    Outputf("P%d ST\n", CharPen);

  Outputf("setmiterlimit\n");
  Outputf("grestore\n");
}


/*
 * 'LO_label_origin()' - Set the label origin.
 */

void
LO_label_origin(int     num_params,	/* I - Number of parameters */
                param_t *params)	/* I - Parameters */
{
  (void)num_params;
  (void)params;
}


/*
 * 'SA_select_alternate()' - Select the alternate font.
 */

void
SA_select_alternate(int     num_params,	/* I - Number of parameters */
                    param_t *params)	/* I - Parameters */
{
  Outputf("SA\n");
  CharFont = 1;
}


/*
 * 'SD_define_standard()' - Define the standard font...
 */
 
void
SD_define_standard(int     num_params,	/* I - Number of parameters */
                   param_t *params)	/* I - Parameters */
{
  int	i;				/* Looping var */
  int	typeface,			/* Typeface number */
	posture,			/* Posture number */
	weight;				/* Weight number */
  float	height;				/* Height/size of font */


 /*
  * Set default font attributes...
  */

  typeface = 48;
  posture  = 0;
  weight   = 0;
  height   = 11.5;

 /*
  * Loop through parameter value pairs...
  */

  for (i = 0; i < (num_params - 1); i += 2)
    switch ((int)params[i].value.number)
    {
      case 4 :
          height = params[i + 1].value.number;
          break;
      case 5 :
          posture = params[i + 1].value.number;
          break;
      case 6 :
          weight = params[i + 1].value.number;
          break;
      case 7 :
          typeface = params[i + 1].value.number;
          break;
    }

 /*
  * Define the font...
  */

  Outputf("/SS { /%s%s%s%s findfont %.1f scalefont setfont } def\n",
          typeface == 48 ? "Courier" : "Helvetica",
          (weight != 0 || posture != 0) ? "-" : "",
          weight != 0 ? "Bold" : "",
          posture != 0 ? "Oblique" : "",
          height);

  CharHeight[0] = height;
}


/*
 * 'SI_absolute_size()' - Set the absolute size of text.
 */

void
SI_absolute_size(int     num_params,	/* I - Number of parameters */
                 param_t *params)	/* I - Parameters */
{
  (void)num_params;
  (void)params;
}


/*
 * 'SL_character_slant()' - Set the slant of text.
 */

void
SL_character_slant(int     num_params,	/* I - Number of parameters */
                   param_t *params)	/* I - Parameters */
{
  (void)num_params;
  (void)params;
}


/*
 * 'SR_relative_size()' - Set the relative size of text.
 */

void
SR_relative_size(int     num_params,	/* I - Number of parameters */
                 param_t *params)	/* I - Parameters */
{
  (void)num_params;
  (void)params;
}


/*
 * 'SS_select_standard()' - Select the standard font for text.
 */

void
SS_select_standard(int     num_params,	/* I - Number of parameters */
                   param_t *params)	/* I - Parameters */
{
  Outputf("SS\n");
  CharFont = 0;
}


/*
 * 'TD_transparent_data()' - Send transparent print data.
 */

void
TD_transparent_data(int     num_params,	/* I - Number of parameters */
                    param_t *params)	/* I - Parameters */
{
  (void)num_params;
  (void)params;
}


/*
 * End of "$Id: hpgl-char.c,v 1.5 1999/03/21 16:26:58 mike Exp $".
 */
