/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
  to anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer
  to the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given
  to you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises supports the work of the GNU Project, but is not
  affiliated with the Free Software Foundation or the GNU Project.  GNU
  Ghostscript, as distributed by Aladdin Enterprises, does not require any
  GNU software to build or run it.
*/

/*$Id: gscsel.h,v 1.1 2000/03/13 18:57:55 mike Exp $ */
/* Definitions for color operand selection */

#ifndef gscsel_INCLUDED
#  define gscsel_INCLUDED

/*
 * Define whether we are mapping a "source" or a "texture" color for
 * RasterOp.  Right the source and texture only have separate halftone
 * phases in the graphics state, but someday they might have more.
 */
typedef enum {
    gs_color_select_all = -1,	/* for setting only, not for reading */
    gs_color_select_texture = 0,	/* 0 is the one is used for currenthtphase */
    gs_color_select_source = 1
} gs_color_select_t;

#define gs_color_select_count 2

#endif /* gscsel_INCLUDED */
