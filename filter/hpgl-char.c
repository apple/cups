/*
 * "$Id: hpgl-char.c,v 1.1 1996/08/24 19:41:24 mike Exp $"
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
 *   $Log: hpgl-char.c,v $
 *   Revision 1.1  1996/08/24 19:41:24  mike
 *   Initial revision
 *
 */

/*
 * Include necessary headers...
 */

#include "hpgl2ps.h"


void
AD_define_alternate(int num_params, param_t *params)
{
  int i;
  int typeface, posture, weight;
  float height;


  typeface = 48;
  posture  = 0;
  weight   = 0;
  height   = 11.5;

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
    };

  fprintf(OutputFile, "/SA { /%s%s%s%s findfont %.1f scalefont setfont } def\n",
          typeface == 48 ? "Courier" : "Helvetica",
          (weight != 0 || posture != 0) ? "-" : "",
          weight != 0 ? "Bold" : "",
          posture != 0 ? "Oblique" : "",
          height);

  CharHeight[1] = height;
}


void
CF_character_fill(int num_params, param_t *params)
{
  if (num_params == 0)
    CharFillMode = 0;
  else
    CharFillMode = params[0].value.number;

  if (num_params == 2)
    CharPen = params[1].value.number;
}


void
CP_character_plot(int num_params, param_t *params)
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
  };
}


void
DI_absolute_direction(int num_params, param_t *params)
{
  fputs(CharFont == 0 ? "SS\n" : "SA\n", OutputFile);

  if (num_params == 2)
    fprintf(OutputFile, "currentfont [ %f %f %f %f 0.0 0.0 ] makefont setfont\n",
            params[0].value.number, -params[1].value.number,
            params[1].value.number, params[0].value.number);
}


void
DR_relative_direction(int num_params, param_t *params)
{
}


void
DT_define_label_term(int num_params, param_t *params)
{
  if (num_params == 0)
    StringTerminator = '\003';
  else
    StringTerminator = params[0].value.string[0];
}


void
DV_define_variable_path(int num_params, param_t *params)
{
}


void
ES_extra_space(int num_params, param_t *params)
{
}


void
LB_label(int num_params, param_t *params)
{
  char *s;


  if (num_params == 0)
    return;

  fputs("gsave\n", OutputFile);
  fputs("MP\n", OutputFile);
  fprintf(OutputFile, "%.3f %.3f MO\n", PenPosition[0], PenPosition[1]);

  fputs("(", OutputFile);
  for (s = params[0].value.string; *s != '\0'; s ++)
    if (strchr("()\\", *s) != NULL)
      fprintf(OutputFile, "\\%c", *s);
    else
      putc(*s, OutputFile);
  fputs(") true charpath\n", OutputFile);

  if (CharFillMode != 1)
    fputs("FI\n", OutputFile);
  if (CharFillMode == 1 || CharFillMode == 3)
    fprintf(OutputFile, "P%d ST\n", CharPen);

  fputs("grestore\n", OutputFile);
}


void
LO_label_origin(int num_params, param_t *params)
{
}


void
SA_select_alternate(int num_params, param_t *params)
{
  fputs("SA\n", OutputFile);
  CharFont = 1;
}


void
SD_define_standard(int num_params, param_t *params)
{
  int i;
  int typeface, posture, weight;
  float height;


  typeface = 48;
  posture  = 0;
  weight   = 0;
  height   = 11.5;

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
    };

  fprintf(OutputFile, "/SS { /%s%s%s%s findfont %.1f scalefont setfont } def\n",
          typeface == 48 ? "Courier" : "Helvetica",
          (weight != 0 || posture != 0) ? "-" : "",
          weight != 0 ? "Bold" : "",
          posture != 0 ? "Oblique" : "",
          height);

  CharHeight[0] = height;
}


void
SI_absolute_size(int num_params, param_t *params)
{
}


void
SL_character_slant(int num_params, param_t *params)
{
}


void
SR_relative_size(int num_params, param_t *params)
{
}


void
SS_select_standard(int num_params, param_t *params)
{
  fputs("SS\n", OutputFile);
  CharFont = 0;
}


void
TD_transparent_data(int num_params, param_t *params)
{
}


/*
 * End of "$Id: hpgl-char.c,v 1.1 1996/08/24 19:41:24 mike Exp $".
 */

