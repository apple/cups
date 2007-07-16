/*
 * "$Id: hpgl-char.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   HP-GL/2 character processing for the Common UNIX Printing System (CUPS).
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
 *   
 */

/*
 * Include necessary headers...
 */

#include "hpgltops.h"


/*
 * 'define_font()' - Define the specified font...
 */

void
define_font(int f)			/* I - Font number */
{
  font_t	*font;			/* Font */
  const char	*fstring;		/* Font string - SA or SS */
  float		xform[2][2];		/* Transform matrix */


 /*
  * Get the correct font data...
  */

  if (f)
  {
    font    = &AlternateFont;
    fstring = "SA";
  }
  else
  {
    font    = &StandardFont;
    fstring = "SS";
  }

 /*
  * Compute the font matrix, accounting for any rotation...
  */

  switch (Rotation)
  {
    default :
    case 0 :
        xform[0][0] = font->xpitch * font->x * font->height;
	xform[0][1] = font->xpitch * font->y * font->height;
	xform[1][0] = -font->y * font->height;
	xform[1][1] = font->x * font->height;
        break;

    case 90 :
	xform[0][0] = -font->xpitch * font->y * font->height;
        xform[0][1] = font->xpitch * font->x * font->height;
	xform[1][0] = -font->x * font->height;
	xform[1][1] = -font->y * font->height;
        break;

    case 180 :
        xform[0][0] = -font->xpitch * font->x * font->height;
	xform[0][1] = -font->xpitch * font->y * font->height;
	xform[1][0] = font->y * font->height;
	xform[1][1] = -font->x * font->height;
        break;

    case 270 :
	xform[0][0] = font->xpitch * font->y * font->height;
        xform[0][1] = -font->xpitch * font->x * font->height;
	xform[1][0] = font->x * font->height;
	xform[1][1] = font->y * font->height;
        break;
  }

 /*
  * Send the font definition...
  */

  printf("/%s {\n"
         "	/%s%s%s%s findfont\n"
	 "	[ %f %f %f %f 0.0 0.0 ] makefont\n"
	 "	setfont\n"
	 "} bind def\n",
         fstring, font->spacing ? "Helvetica" : "Courier",
         (font->weight > 0 || font->posture) ? "-" : "",
         font->weight > 0 ? "Bold" : "",
         font->posture ? "Oblique" : "",
         xform[0][0], xform[0][1], xform[1][0], xform[1][1]);

  if (f == CharFont)
    printf("%s\n", fstring);
}


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

  AlternateFont.symbol_set = 277;
  AlternateFont.spacing    = 0;
  AlternateFont.pitch      = 9;
  AlternateFont.height     = 11.5;
  AlternateFont.posture    = 0;
  AlternateFont.weight     = 0;
  AlternateFont.typeface   = 48;
  AlternateFont.x          = 1.0;
  AlternateFont.y          = 0.0;

 /*
  * Loop through parameter value pairs...
  */

  for (i = 0; i < (num_params - 1); i += 2)
    switch ((int)params[i].value.number)
    {
      case 1 : /* Symbol Set */
          AlternateFont.symbol_set = (int)params[i + 1].value.number;
          break;
      case 2 : /* Font Spacing */
          AlternateFont.spacing = (int)params[i + 1].value.number;
          break;
      case 3 : /* Pitch */
          AlternateFont.pitch = params[i + 1].value.number;
          break;
      case 4 : /* Height */
          AlternateFont.height = params[i + 1].value.number;
          break;
      case 5 : /* Posture */
          AlternateFont.posture = (int)params[i + 1].value.number;
          break;
      case 6 : /* Stroke Weight */
          AlternateFont.weight = (int)params[i + 1].value.number;
          break;
      case 7 : /* Typeface */
          AlternateFont.typeface = (int)params[i + 1].value.number;
          break;
    }

  if (AlternateFont.spacing)
  {
   /*
    * Set proportional spacing font...
    */

    AlternateFont.xpitch = 1.0f;
  }
  else
  {
   /*
    * Set fixed-spaced font...
    */

    AlternateFont.xpitch = 0.6f * AlternateFont.height / AlternateFont.pitch;
  }

 /*
  * Define the font...
  */

  if (PageDirty)
  {
    printf("%% AD");
    for (i = 0; i < num_params; i ++)
      if (i)
        printf(",%g", params[i].value.number);
      else
        printf("%g", params[i].value.number);
    puts(";");

    define_font(1);
  }

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
  if (num_params != 2)
    return;

  if (CharFont)
  {
    AlternateFont.x = params[0].value.number;
    AlternateFont.y = params[1].value.number;
  }
  else
  {
    StandardFont.x = params[0].value.number;
    StandardFont.y = params[1].value.number;
  }

  if (PageDirty)
  {
    printf("%% DI%g,%g\n", params[0].value.number, params[1].value.number);

    define_font(CharFont);
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
  Outputf("currentmiterlimit 1.0 setmiterlimit\n");
  Outputf("MP\n");
  Outputf("%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);
  PenValid = 1;

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
  {
    Outputf("%.3f %.3f %.3f %.2f SP ST\n", Pens[CharPen].rgb[0],
	    Pens[CharPen].rgb[CharPen], Pens[CharPen].rgb[2],
	    Pens[CharPen].width * PenScaling);
    Outputf("%.3f %.3f %.3f %.2f SP\n", Pens[PenNumber].rgb[0],
	    Pens[PenNumber].rgb[1], Pens[PenNumber].rgb[2],
	    Pens[PenNumber].width * PenScaling);
  }

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

  StandardFont.symbol_set = 277;
  StandardFont.spacing    = 0;
  StandardFont.pitch      = 9;
  StandardFont.height     = 11.5;
  StandardFont.posture    = 0;
  StandardFont.weight     = 0;
  StandardFont.typeface   = 48;
  StandardFont.x          = 1.0;
  StandardFont.y          = 0.0;

 /*
  * Loop through parameter value pairs...
  */

  for (i = 0; i < (num_params - 1); i += 2)
    switch ((int)params[i].value.number)
    {
      case 1 : /* Symbol Set */
          StandardFont.symbol_set = (int)params[i + 1].value.number;
          break;
      case 2 : /* Font Spacing */
          StandardFont.spacing = (int)params[i + 1].value.number;
          break;
      case 3 : /* Pitch */
          StandardFont.pitch = params[i + 1].value.number;
          break;
      case 4 : /* Height */
          StandardFont.height = params[i + 1].value.number;
          break;
      case 5 : /* Posture */
          StandardFont.posture = (int)params[i + 1].value.number;
          break;
      case 6 : /* Stroke Weight */
          StandardFont.weight = (int)params[i + 1].value.number;
          break;
      case 7 : /* Typeface */
          StandardFont.typeface = (int)params[i + 1].value.number;
          break;
    }

  if (StandardFont.spacing || StandardFont.pitch <= 0.0)
  {
   /*
    * Set proportional spacing font...
    */

    StandardFont.xpitch = 1.0f;
  }
  else
  {
   /*
    * Set fixed-spaced font...
    */

    StandardFont.xpitch = 0.6f * StandardFont.height / StandardFont.pitch;
  }

 /*
  * Define the font...
  */

  if (PageDirty)
  {
    printf("%% SD");
    for (i = 0; i < num_params; i ++)
      if (i)
        printf(",%g", params[i].value.number);
      else
        printf("%g", params[i].value.number);
    puts(";");

    define_font(0);
  }

  CharHeight[0] = StandardFont.height;
}


/*
 * 'SI_absolute_size()' - Set the absolute size of text.
 */

void
SI_absolute_size(int     num_params,	/* I - Number of parameters */
                 param_t *params)	/* I - Parameters */
{
  float	xsize, ysize;			/* Font size... */


  if (num_params != 2)
    return;

 /*
  * The "SI" values are supposed to be cm, but they appear to be inches
  * when tested on real HP devices...
  */

  xsize = params[0].value.number * 72.0f;
  ysize = params[1].value.number * 72.0f * 0.6f;

  if (CharFont)
  {
    AlternateFont.xpitch = xsize / ysize;
    AlternateFont.height = ysize;
  }
  else
  {
    StandardFont.xpitch = xsize / ysize;
    StandardFont.height = ysize;
  }

  if (PageDirty)
  {
    printf("%% SI%g,%g\n", params[0].value.number, params[1].value.number);

    define_font(CharFont);
  }
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
 * End of "$Id: hpgl-char.c 6649 2007-07-11 21:46:42Z mike $".
 */
