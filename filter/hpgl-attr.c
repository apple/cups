/*
 * "$Id: hpgl-attr.c,v 1.1 1996/08/24 19:41:24 mike Exp $"
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
 *   $Log: hpgl-attr.c,v $
 *   Revision 1.1  1996/08/24 19:41:24  mike
 *   Initial revision
 *
 */

/*
 * Include necessary headers...
 */

#include "hpgl2ps.h"


void
AC_anchor_corner(int num_params, param_t *params)
{
}


void
FT_fill_type(int num_params, param_t *params)
{
  if (num_params == 0 ||
      params[0].value.number == 1 ||
      params[0].value.number == 2)
  {
    /**** SOLID PATTERN ****/
  };    
}


void
LA_line_attributes(int num_params, param_t *params)
{
  int i;


  if (num_params == 0)
  {
    fputs("5.0 setmiterlimit\n", OutputFile);
    fputs("0 setlinecap\n", OutputFile);
    fputs("0 setlinejoin\n", OutputFile);
  }
  else for (i = 0; i < (num_params - 1); i += 2)
    switch ((int)params[i].value.number)
    {
      case 1 :
          fprintf(OutputFile, "%d setlinecap\n",
                  params[i + 1].value.number == 1 ? 0 :
                  params[i + 1].value.number == 4 ? 1 : 2);
          break;
      case 2 :
          switch ((int)params[i + 1].value.number)
          {
            case 1 :
            case 2 :
            case 3 :
                fputs("0 setlinejoin\n", OutputFile);
                break;
            case 5 :
                fputs("2 setlinejoin\n", OutputFile);
                break;
            default :
                fputs("1 setlinejoin\n", OutputFile);
                break;
          };
          break;
      case 3 :
          fprintf(OutputFile, "%f setmiterlimit\n", params[i + 1].value.number);
          break;
    };
}


void
LT_line_type(int num_params, param_t *params)
{
}


void
PW_pen_width(int num_params, param_t *params)
{
  if (num_params == 0)
    PenWidth = 0.45 / 25.4 * 72.0;
  else
    PenWidth = params[0].value.number / 25.4 * 72.0;

  fprintf(OutputFile, "%.1f setlinewidth\n", PenWidth);
}


void
RF_raster_fill(int num_params, param_t *params)
{
}


void
SM_symbol_mode(int num_params, param_t *params)
{
}


void
SP_select_pen(int num_params, param_t *params)
{
  if (num_params == 0)
    fputs("P0\n", OutputFile);
  else
    fprintf(OutputFile, "P%d\n", (int)(params[0].value.number));
}


void
UL_user_line_type(int num_params, param_t *params)
{
}


void
WU_width_units(int num_params, param_t *params)
{
}


/*
 * End of "$Id: hpgl-attr.c,v 1.1 1996/08/24 19:41:24 mike Exp $".
 */

