/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility to
  anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer to
  the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given to
  you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises is not affiliated with the Free Software Foundation or
  the GNU Project.  GNU Ghostscript, as distributed by Aladdin Enterprises,
  does not depend on any other GNU software.
*/

/* zchar42.c */
/* Type 42 character display operator */
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "gsmatrix.h"
#include "gspaint.h"		/* for gs_fill, gs_stroke */
#include "gspath.h"
#include "gxfixed.h"
#include "gxchar.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "gxistate.h"
#include "gxpath.h"
#include "gzstate.h"		/* only for ->path */
#include "dstack.h"		/* only for systemdict */
#include "estack.h"
#include "ichar.h"
#include "icharout.h"
#include "ifont.h"		/* for font_param */
#include "igstate.h"
#include "store.h"

/* Imported procedures */
int gs_type42_append(P7(uint glyph_index, gs_imager_state *pis,
  gx_path *ppath, const gs_log2_scale_point *pscale, bool charpath_flag,
  int paint_type, gs_font_type42 *pfont));
int gs_type42_get_metrics(P3(gs_font_type42 *pfont, uint glyph_index,
  float psbw[4]));

/* <font> <code|name> <name> <glyph_index> .type42execchar - */
private int type42_fill(P1(os_ptr));
private int type42_stroke(P1(os_ptr));
private int
ztype42execchar(register os_ptr op)
{	gs_font *pfont;
#define pbfont ((gs_font_base *)pfont)
#define pfont42 ((gs_font_type42 *)pfont)
	int code = font_param(op - 3, &pfont);
	gs_show_enum *penum = op_show_find();
	int present;
	float sbw[4];

	code = font_param(op - 3, &pfont);
	if ( code < 0 )
	  return code;
	if ( penum == 0 || pfont->FontType != ft_TrueType )
	  return_error(e_undefined);
	/*
	 * Any reasonable implementation would execute something like
	 *	1 setmiterlimit 0 setlinejoin 0 setlinecap
	 * here, but apparently the Adobe implementations aren't reasonable.
	 *
	 * If this is a stroked font, set the stroke width.
	 */
	if ( pfont->PaintType )
	  gs_setlinewidth(igs, pfont->StrokeWidth);
	check_estack(3);	/* for continuations */
	/*
	 * Execute the definition of the character.
	 */
	if ( r_is_proc(op) )
	  return zchar_exec_char_proc(op);
	/*
	 * The definition must be a Type 42 glyph index.
	 * Note that we do not require read access: this is deliberate.
	 */
	check_type(*op, t_integer);
	check_ostack(3);	/* for lsb values */
	present = zchar_get_metrics(pbfont, op - 1, sbw);
	if ( present < 0 )
	  return present;
	/* Establish a current point. */
	code = gs_moveto(igs, 0.0, 0.0);
	if ( code < 0 )
	  return code;
	/* Get the metrics and set the cache device. */
	if ( present == metricsNone )
	  { code = gs_type42_get_metrics(pfont42, (uint)op->value.intval, sbw);
	    if ( code < 0 )
	      return code;
	  }
	return zchar_set_cache(op, pbfont, op - 1,
			       (present == metricsSideBearingAndWidth ?
				sbw : NULL),
			       sbw + 2, &pbfont->FontBBox,
			       type42_fill, type42_stroke);
#undef pfont42
#undef pbfont
}

/* Continue after a CDevProc callout. */
private int type42_finish(P2(os_ptr op, int (*cont)(P1(gs_state *))));
private int
type42_fill(os_ptr op)
{	return type42_finish(op, gs_fill);
}
private int
type42_stroke(os_ptr op)
{	return type42_finish(op, gs_stroke);
}
/* <font> <code|name> <name> <glyph_index> <sbx> <sby> %type42_{fill|stroke} - */
/* <font> <code|name> <name> <glyph_index> %type42_{fill|stroke} - */
private int
type42_finish(os_ptr op, int (*cont)(P1(gs_state *)))
{	gs_font *pfont;
#define pfont42 ((gs_font_type42 *)pfont)
	int code;
	gs_show_enum *penum = op_show_find();
	float sbxy[2];
	gs_point sbpt;
	gs_point *psbpt = 0;
	os_ptr opc = op;

	if ( !r_has_type(op - 3, t_dictionary) )
	  {	check_op(6);
		code = num_params(op, 2, sbxy);
		if ( code < 0 )
		  return code;
		sbpt.x = sbxy[0];
		sbpt.y = sbxy[1];
		psbpt = &sbpt;
		opc -= 2;
	  }
	check_type(*opc, t_integer);
	code = font_param(opc - 3, &pfont);
	if ( code < 0 )
	  return code;
	if ( penum == 0 || pfont->FontType != ft_TrueType )
	  return_error(e_undefined);
	code = gs_type42_append((uint)opc->value.intval,
				(gs_imager_state *)penum->pgs,
				penum->pgs->path,
				&penum->log2_current_scale,
				gs_show_in_charpath(penum) != cpm_show,
				pfont->PaintType, pfont42);
	if ( code < 0 )
	  return code;
	pop((psbpt == 0 ? 4 : 6));
	return (*cont)(penum->pgs);
#undef pfont42
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zchar42_op_defs) {
	{"4.type42execchar", ztype42execchar},
END_OP_DEFS(0) }
