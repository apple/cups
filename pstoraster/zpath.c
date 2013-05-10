/* Copyright (C) 1989, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zpath.c */
/* Basic path operators */
#include "math_.h"
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "igstate.h"
#include "gsmatrix.h"
#include "gspath.h"
#include "store.h"

/* Forward references */
private int near common_to(P2(os_ptr,
  int (*)(P3(gs_state *, floatp, floatp))));
private int near common_curve(P2(os_ptr,
  int (*)(P7(gs_state *, floatp, floatp, floatp, floatp, floatp, floatp))));

/* - newpath - */
private int
znewpath(register os_ptr op)
{	return gs_newpath(igs);
}

/* - currentpoint <x> <y> */
private int
zcurrentpoint(register os_ptr op)
{	gs_point pt;
	int code = gs_currentpoint(igs, &pt);
	if ( code < 0 ) return code;
	push(2);
	make_real(op - 1, pt.x);
	make_real(op, pt.y);
	return 0;
}

/* <x> <y> moveto - */
int
zmoveto(os_ptr op)
{	return common_to(op, gs_moveto);
}

/* <dx> <dy> rmoveto - */
int
zrmoveto(os_ptr op)
{	return common_to(op, gs_rmoveto);
}

/* <x> <y> lineto - */
int
zlineto(os_ptr op)
{	return common_to(op, gs_lineto);
}

/* <dx> <dy> rlineto - */
int
zrlineto(os_ptr op)
{	return common_to(op, gs_rlineto);
}

/* Common code for [r](move/line)to */
private int near
common_to(os_ptr op, int (*add_proc)(P3(gs_state *, floatp, floatp)))
{	float opxy[2];
	int code;
	if (	(code = num_params(op, 2, opxy)) < 0 ||
		(code = (*add_proc)(igs, opxy[0], opxy[1])) < 0
	   ) return code;
	pop(2);
	return 0;
}

/* <x1> <y1> <x2> <y2> <x3> <y3> curveto - */
int
zcurveto(register os_ptr op)
{	return common_curve(op, gs_curveto);
}

/* <dx1> <dy1> <dx2> <dy2> <dx3> <dy3> rcurveto - */
int
zrcurveto(register os_ptr op)
{	return common_curve(op, gs_rcurveto);
}

/* Common code for [r]curveto */
private int near
common_curve(os_ptr op,
  int (*add_proc)(P7(gs_state *, floatp, floatp, floatp, floatp, floatp, floatp)))
{	float opxy[6];
	int code;
	if ( (code = num_params(op, 6, opxy)) < 0 ) return code;
	code = (*add_proc)(igs, opxy[0], opxy[1], opxy[2], opxy[3], opxy[4], opxy[5]);
	if ( code >= 0 ) pop(6);
	return code;
}

/* - closepath - */
int
zclosepath(register os_ptr op)
{	return gs_closepath(igs);
}

/* - initclip - */
private int
zinitclip(register os_ptr op)
{	return gs_initclip(igs);
}

/* - clip - */
private int
zclip(register os_ptr op)
{	return gs_clip(igs);
}

/* - eoclip - */
private int
zeoclip(register os_ptr op)
{	return gs_eoclip(igs);
}

/* <bool> .setclipoutside - */
private int
zsetclipoutside(register os_ptr op)
{	int code;
	check_type(*op, t_boolean);
	code = gs_setclipoutside(igs, op->value.boolval);
	if ( code >= 0 )
	  pop(1);
	return code;
}

/* - .currentclipoutside <bool> */
private int
zcurrentclipoutside(register os_ptr op)
{	push(1);
	make_bool(op, gs_currentclipoutside(igs));
	return 0;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zpath_op_defs) {
	{"0clip", zclip},
	{"0closepath", zclosepath},
	{"0.currentclipoutside", zcurrentclipoutside},
	{"0currentpoint", zcurrentpoint},
	{"6curveto", zcurveto},
	{"0eoclip", zeoclip},
	{"0initclip", zinitclip},
	{"2lineto", zlineto},
	{"2moveto", zmoveto},
	{"0newpath", znewpath},
	{"6rcurveto", zrcurveto},
	{"2rlineto", zrlineto},
	{"2rmoveto", zrmoveto},
	{"1.setclipoutside", zsetclipoutside},
END_OP_DEFS(0) }
