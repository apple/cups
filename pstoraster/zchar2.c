/* Copyright (C) 1992, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zchar2.c */
/* Level 2 character operators */
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "gschar.h"
#include "gsmatrix.h"		/* for gxfont.h */
#include "gsstruct.h"		/* for st_stream */
#include "gxfixed.h"		/* for gxfont.h */
#include "gxfont.h"
#include "estack.h"
#include "ialloc.h"
#include "ichar.h"
#include "ifont.h"
#include "igstate.h"
#include "iname.h"
#include "store.h"
#include "stream.h"
#include "ibnum.h"
#include "gspath.h"             /* gs_rmoveto prototype */

/* Table of continuation procedures. */
private int xshow_continue(P1(os_ptr));
private int yshow_continue(P1(os_ptr));
private int xyshow_continue(P1(os_ptr));
static const op_proc_p xyshow_continues[4] = {
	0, xshow_continue, yshow_continue, xyshow_continue
};

/* Forward references */
private int cshow_continue(P1(os_ptr));
private int moveshow(P2(os_ptr, int));
private int moveshow_continue(P2(os_ptr, int));

/* <proc> <string> cshow - */
private int
zcshow(os_ptr op)
{	os_ptr proc_op = op - 1;
	os_ptr str_op = op;
	gs_show_enum *penum;
	int code;

	/* Even though this is not documented anywhere by Adobe, */
	/* the Adobe interpreters apparently allow the string and */
	/* the procedure to be provided in either order! */
	if ( r_is_proc(proc_op) )
	  ;
	else if ( r_is_proc(op) )	/* operands reversed */
	  { proc_op = op;
	    str_op = op - 1;
	  }
	else
	  { check_op(2);
	    return_error(e_typecheck);
	  }
	if ( (code = op_show_setup(str_op, &penum)) != 0 )
	  return code;
	if ( (code = gs_cshow_n_init(penum, igs, (char *)str_op->value.bytes,
				     r_size(str_op))) < 0
	   )
	   {	ifree_object(penum, "op_show_enum_setup");
		return code;
	   }
	op_show_finish_setup(penum, 2, NULL);
	sslot = *proc_op;			/* save kerning proc */
	return cshow_continue(op - 2);
}
private int
cshow_continue(os_ptr op)
{	gs_show_enum *penum = senum;
	int code;
	check_estack(4);	/* in case we call the procedure */
	code = gs_show_next(penum);
	if ( code != gs_show_move )
	{	code = op_show_continue_dispatch(op, code);
		if ( code == o_push_estack )	/* must be gs_show_render */
		{	make_op_estack(esp - 1, cshow_continue);
		}
		return code;
	}
	/* Push the character code and width, and call the procedure. */
	{	ref *pslot = &sslot;
		gs_point wpt;

		gs_show_current_width(penum, &wpt);
		push(3);
		make_int(op - 2, gs_show_current_char(penum));
		make_real(op - 1, wpt.x);
		make_real(op, wpt.y);
		push_op_estack(cshow_continue);
		push_op_estack(zsetfont);	/* restore font afterwards */
		*++esp = *pfont_dict(gs_currentfont(igs));
		gs_setfont(igs, gs_show_current_font(penum));
		*++esp = *pslot;	/* user procedure */
	}
	return o_push_estack;
}

/* <charname> glyphshow - */
private int
zglyphshow(os_ptr op)
{	gs_show_enum *penum;
	int code;
	check_type(*op, t_name);
	if ( (code = op_show_enum_setup(op, &penum)) != 0 )
	  return code;
	if ( (code = gs_glyphshow_init(penum, igs, (gs_glyph)name_index(op))) < 0
	   )
	{	ifree_object(penum, "op_show_enum_setup");
		return code;
	}
	op_show_finish_setup(penum, 1, NULL);
	return op_show_continue(op - 1);
}

/* - rootfont <font> */
private int
zrootfont(os_ptr op)
{	push(1);
	*op = *pfont_dict(gs_rootfont(igs));
	return 0;
}

/* <string> <numarray|numstring> xshow - */
private int
zxshow(os_ptr op)
{	return moveshow(op, 1);
}

/* <string> <numarray|numstring> yshow - */
private int
zyshow(os_ptr op)
{	return moveshow(op, 2);
}

/* <string> <numarray|numstring> xyshow - */
private int
zxyshow(os_ptr op)
{	return moveshow(op, 3);
}

/* Common code for {x,y,xy}show */
private int
moveshow(os_ptr op, int xymask)
{	gs_show_enum *penum;
	int code = op_show_setup(op - 1, &penum);

	if ( code != 0 )
	  return code;
	if ( (code = gs_xyshow_n_init(penum, igs, (char *)op[-1].value.bytes, r_size(op - 1)) < 0) )
	{	ifree_object(penum, "op_show_enum_setup");
		return code;
	}
	code = num_array_format(op);
	if ( code < 0 )
	{	ifree_object(penum, "op_show_enum_setup");
		return code;
	}
	op_show_finish_setup(penum, 2, NULL);
	ref_assign(&sslot, op);
	return moveshow_continue(op - 2, xymask);
}

/* Continuation procedures */

private int
xshow_continue(os_ptr op)
{	return moveshow_continue(op, 1);
}

private int
yshow_continue(os_ptr op)
{	return moveshow_continue(op, 2);
}

private int
xyshow_continue(os_ptr op)
{	return moveshow_continue(op, 3);
}

/* Get one value from the encoded number string or array. */
/* Sets pvalue->value.realval. */
private int
sget_real(const ref *nsp, int format, uint index, ref *pvalue)
{	int code;
	switch ( code = num_array_get(nsp, format, index, pvalue) )
	{
	case t_integer: pvalue->value.realval = pvalue->value.intval;
	case t_real: return t_real;
	case t_null: code = gs_note_error(e_rangecheck);
	default: return code;
	}
}

private int
moveshow_continue(os_ptr op, int xymask)
{	const ref *nsp = &sslot;
	int format = num_array_format(nsp);
	int code;
	gs_show_enum *penum = senum;
	uint index = ssindex.value.intval;

	for ( ; ; )
	  { ref rwx, rwy;
	    code = gs_show_next(penum);
	    if ( code != gs_show_move )
	      { ssindex.value.intval = index;
		code = op_show_continue_dispatch(op, code);
		if ( code == o_push_estack )	/* must be gs_show_render */
		{	make_op_estack(esp - 1, xyshow_continues[xymask]);
		}
		return code;
	      }
	    /* Move according to the next value(s) from the stream. */
	    if ( xymask & 1 )
	      { code = sget_real(nsp, format, index++, &rwx);
		if ( code < 0 ) break;
	      }
	    else
	      rwx.value.realval = 0;
	    if ( xymask & 2 )
	      { code = sget_real(nsp, format, index++, &rwy);
		if ( code < 0 ) break;
	      }
	    else
	      rwy.value.realval = 0;
	    code = gs_rmoveto(igs, rwx.value.realval, rwy.value.realval);
	    if ( code < 0 ) break;
	  }
	/* An error occurred.  Clean up before returning. */
	op_show_free();
	return code;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zchar2_op_defs) {
	{"2cshow", zcshow},
	{"0rootfont", zrootfont},
		/* Internal operators */
	{"0%cshow_continue", cshow_continue},
END_OP_DEFS(0) }
BEGIN_OP_DEFS(zchar2_l2_op_defs) {
		op_def_begin_level2(),
	{"1glyphshow", zglyphshow},
	{"2xshow", zxshow},
	{"2xyshow", zxyshow},
	{"2yshow", zyshow},
		/* Internal operators */
	{"0%xshow_continue", xshow_continue},
	{"0%yshow_continue", yshow_continue},
	{"0%xyshow_continue", xyshow_continue},
END_OP_DEFS(0) }
