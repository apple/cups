/*
 * "$Id: hpgl-char.c,v 1.8 1999/10/28 21:33:43 mike Exp $"
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
 *   AD_define_alternate()     - Define the alternate font.
 *   CF_character_fill()       - Set whether or not to fill or outline
 *                               characters.
 *   CP_character_plot()       - Move the current pen position for the given
 *                               number of columns and rows.
 *   DI_absolute_direction()   - Set the direction vector for text.
 *   DR_relative_direction()   - Set the relative direction vector for text.
 *   DT_define_label_term()    - Set the label string terminator.
 *   DV_define_variable_path() - Define a path for text.
 *   ES_extra_space()          - Set extra spacing (kerning) between characters.
 *   LB_label()                - Display a label string.
 *   LO_label_origin()         - Set the label origin.
 *   SA_select_alternate()     - Select the alternate font.
 *   SD_define_standard()      - Define the standard font...
 *   SI_absolute_size()        - Set the absolute size of text.
 *   SL_character_slant()      - Set the slant of text.
 *   SR_relative_size()        - Set the relative size of text.
 *   SS_select_standard()      - Select the standard font for text.
 *   TD_transparent_data()     - Send transparent print data.
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


 /*
  * Set default font attributes...
  */

  AlternateFont.typeface = 48;
  AlternateFont.posture  = 0;
  AlternateFont.weight   = 0;
  AlternateFont.height   = 11.5;

 /*
  * Loop through parameter value pairs...
  */

  for (i = 0; i < (num_params - 1); i += 2)
    switch ((int)params[i].value.number)
    {
      case 4 :
          AlternateFont.height = params[i + 1].value.number;
          break;
      case 5 :
          AlternateFont.posture = (int)params[i + 1].value.number;
          break;
      case 6 :
          AlternateFont.weight = (int)params[i + 1].value.number;
          break;
      case 7 :
          AlternateFont.typeface = (int)params[i + 1].value.number;
          break;
    }

 /*
  * Define the font...
  */

  if (PageDirty)
    printf("/SA {\n"
           "	/%s%s%s%s findfont\n"
	   "	[ %f %f %f %f 0.0 0.0 ] makefont\n"
	   "	setfont\n"
	   "} bind def\n",
           AlternateFont.typeface == 48 ? "Courier" : "Helvetica",
           (AlternateFont.weight != 0 || AlternateFont.posture != 0) ? "-" : "",
           AlternateFont.weight != 0 ? "Bold" : "",
           AlternateFont.posture != 0 ? "Oblique" : "",
           AlternateFont.x * AlternateFont.height,
	   -AlternateFont.y * AlternateFont.height,
	   AlternateFont.y * AlternateFont.height,
	   AlternateFont.x * AlternateFont.height);

  CharHeight[1] = AlternateFont.height;
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
    CharFillMode = (int)params[0].value.number;

  if (num_params == 2)
    CharPen = (int)params[1].value.number;
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
	PenPosition[0] += params[0].value.number * 1.2f / CharHeight[CharFont];
	PenPosition[1] += params[1].value.number * CharHeight[CharFont];
	break;
    case 90:
	PenPosition[0] -= params[1].value.number * 1.2f / CharHeight[CharFont];
	PenPosition[1] += params[0].value.number * CharHeight[CharFont];
	break;
    case 180:
	PenPosition[0] -= params[0].value.number * 1.2f / CharHeight[CharFont];
	PenPosition[1] -= params[1].value.number * CharHeight[CharFont];
	break;
    case 270:
	PenPosition[0] += params[1].value.number * 1.2f / CharHeight[CharFont];
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
  if (CharFont)
  {
    if (num_params == 2)
    {
      AlternateFont.x = params[0].value.number;
      AlternateFont.y = params[1].value.number;
    }

    if (PageDirty)
    {
      printf("/SA {\n"
             "	/%s%s%s%s findfont\n"
	     "	[ %f %f %f %f 0.0 0.0 ] makefont\n"
	     "	setfont\n"
	     "} bind def\n",
             AlternateFont.typeface == 48 ? "Courier" : "Helvetica",
             (AlternateFont.weight != 0 || AlternateFont.posture != 0) ? "-" : "",
             AlternateFont.weight != 0 ? "Bold" : "",
             AlternateFont.posture != 0 ? "Oblique" : "",
             AlternateFont.x * AlternateFont.height,
	     -AlternateFont.y * AlternateFont.height,
	     AlternateFont.y * AlternateFont.height,
	     AlternateFont.x * AlternateFont.height);
    }
  }
  else
  {
    if (num_params == 2)
    {
      StandardFont.x = params[0].value.number;
      StandardFont.y = params[1].value.number;
    }

    if (PageDirty)
    {
      printf("/SS {\n"
             "	/%s%s%s%s findfont\n"
	     "	[ %f %f %f %f 0.0 0.0 ] makefont\n"
	     "	setfont\n"
	     "} bind def\n",
             StandardFont.typeface == 48 ? "Courier" : "Helvetica",
             (StandardFont.weight != 0 || StandardFont.posture != 0) ? "-" : "",
             StandardFont.weight != 0 ? "Bold" : "",
             StandardFont.posture != 0 ? "Oblique" : "",
             StandardFont.x * StandardFont.height,
	     -StandardFont.y * StandardFont.height,
	     StandardFont.y * StandardFont.height,
	     StandardFont.x * StandardFont.height);
    }
  }
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
  Outputf("currentmiterlimit 1.0 \n");
  Outputf("MP\n");
  Outputf("%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);

  Outputf("(");
  for (s = params[0].value.string; *s != '\0'; s ++)
    if (strchr("()\\", *s) != NULL)
      Outputf("\\%c", *s);
    else
      Outputf("%c", *s);
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
  (void)num_params;
  (void)params;

  if (PageDirty)
    puts("SA");

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


 /*
  * Set default font attributes...
  */

  StandardFont.typeface = 48;
  StandardFont.posture  = 0;
  StandardFont.weight   = 0;
  StandardFont.height   = 11.5;
  StandardFont.x        = 1.0;
  StandardFont.y        = 0.0;

 /*
  * Loop through parameter value pairs...
  */

  for (i = 0; i < (num_params - 1); i += 2)
    switch ((int)params[i].value.number)
    {
      case 4 :
          StandardFont.height = params[i + 1].value.number;
          break;
      case 5 :
          StandardFont.posture = (int)params[i + 1].value.number;
          break;
      case 6 :
          StandardFont.weight = (int)params[i + 1].value.number;
          break;
      case 7 :
          StandardFont.typeface = (int)params[i + 1].value.number;
          break;
    }

 /*
  * Define the font...
  */

  if (PageDirty)
    printf("/SS {\n"
           "	/%s%s%s%s findfont\n"
	   "	[ %f %f %f %f 0.0 0.0 ] makefont\n"
	   "	setfont\n"
	   "} bind def\n",
           StandardFont.typeface == 48 ? "Courier" : "Helvetica",
           (StandardFont.weight != 0 || StandardFont.posture != 0) ? "-" : "",
           StandardFont.weight != 0 ? "Bold" : "",
           StandardFont.posture != 0 ? "Oblique" : "",
           StandardFont.x * StandardFont.height,
	   -StandardFont.y * StandardFont.height,
	   StandardFont.y * StandardFont.height,
	   StandardFont.x * StandardFont.height);

  CharHeight[0] = StandardFont.height;
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
  (void)num_params;
  (void)params;

  if (PageDirty)
    puts("SS");

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
 * End of "$Id: hpgl-char.c,v 1.8 1999/10/28 21:33:43 mike Exp $".
 */
