/* Copyright (C) 1992, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zchar2.c,v 1.2 2000/03/08 23:15:31 mike Exp $ */
/* Level 2 character operators */
#include "ghost.h"
#include "oper.h"
#include "gschar.h"
#include "gsmatrix.h"		/* for gxfont.h */
#include "gsstruct.h"		/* for st_stream */
#include "gxfixed.h"		/* for gxfont.h */
#include "gxfont.h"
#include "gxchar.h"
#include "estack.h"
#include "ialloc.h"
#include "ichar.h"
#include "ifont.h"
#include "igstate.h"
#include "iname.h"
#include "store.h"
#include "stream.h"
#include "ibnum.h"
#include "gspath.h"		/* gs_rmoveto prototype */

/* Table of continuation procedures. */
private int xshow_continue(P1(os_ptr));
private int yshow_continue(P1(os_ptr));
private int xyshow_continue(P1(os_ptr));
static const op_proc_p xyshow_continues[4] =
{
    0, xshow_continue, yshow_continue, xyshow_continue
};

/* Forward references */
private int moveshow(P2(os_ptr, int));
private int moveshow_continue(P2(os_ptr, int));

/* <charname> glyphshow - */
private int
zglyphshow(os_ptr op)
{
    gs_glyph glyph;
    gs_show_enum *penum;
    int code;

    switch (gs_currentfont(igs)->FontType) {
	case ft_CID_encrypted:
	case ft_CID_user_defined:
	case ft_CID_TrueType:
	case ft_CID_bitmap:
	    check_int_leu(*op, gs_max_glyph - gs_min_cid_glyph);
	    glyph = (gs_glyph) op->value.intval + gs_min_cid_glyph;
	    break;
	default:
	    check_type(*op, t_name);
	    glyph = name_index(op);
    }
    if ((code = op_show_enum_setup(op, &penum)) != 0)
	return code;
    if ((code = gs_glyphshow_init(penum, igs, glyph)) < 0) {
	ifree_object(penum, "op_show_glyph");
	return code;
    }
    op_show_finish_setup(penum, 1, NULL);
    return op_show_continue(op - 1);
}

/* <string> <numarray|numstring> xshow - */
private int
zxshow(os_ptr op)
{
    return moveshow(op, 1);
}

/* <string> <numarray|numstring> yshow - */
private int
zyshow(os_ptr op)
{
    return moveshow(op, 2);
}

/* <string> <numarray|numstring> xyshow - */
private int
zxyshow(os_ptr op)
{
    return moveshow(op, 3);
}

/* Common code for {x,y,xy}show */
private int
moveshow(os_ptr op, int xymask)
{
    gs_show_enum *penum;
    int code = op_show_setup(op - 1, &penum);

    if (code != 0)
	return code;
    if ((code = gs_xyshow_n_init(penum, igs, (char *)op[-1].value.bytes, r_size(op - 1)) < 0)) {
	ifree_object(penum, "op_show_enum_setup");
	return code;
    }
    code = num_array_format(op);
    if (code < 0) {
	ifree_object(penum, "op_show_enum_setup");
	return code;
    }
    op_show_finish_setup(penum, 2, NULL);
    ref_assign(&sslot, op);
    return moveshow_continue(op - 2, xymask);
}

/* Continuation procedures */

private int
xshow_continue(os_ptr op)
{
    return moveshow_continue(op, 1);
}

private int
yshow_continue(os_ptr op)
{
    return moveshow_continue(op, 2);
}

private int
xyshow_continue(os_ptr op)
{
    return moveshow_continue(op, 3);
}

/* Get one value from the encoded number string or array. */
/* Sets pvalue->value.realval. */
private int
sget_real(const ref * nsp, int format, uint index, ref * pvalue)
{
    int code;

    switch (code = num_array_get(nsp, format, index, pvalue)) {
	case t_integer:
	    pvalue->value.realval = pvalue->value.intval;
	case t_real:
	    return t_real;
	case t_null:
	    code = gs_note_error(e_rangecheck);
	default:
	    return code;
    }
}

private int
moveshow_continue(os_ptr op, int xymask)
{
    const ref *nsp = &sslot;
    int format = num_array_format(nsp);
    int code;
    gs_show_enum *penum = senum;
    uint index = ssindex.value.intval;

    for (;;) {
	ref rwx, rwy;

	code = gs_show_next(penum);
	if (code != gs_show_move) {
	    ssindex.value.intval = index;
	    code = op_show_continue_dispatch(op, code);
	    if (code == o_push_estack) {	/* must be gs_show_render */
		make_op_estack(esp - 1, xyshow_continues[xymask]);
	    }
	    return code;
	}
	/* Move according to the next value(s) from the stream. */
	if (xymask & 1) {
	    code = sget_real(nsp, format, index++, &rwx);
	    if (code < 0)
		break;
	} else
	    rwx.value.realval = 0;
	if (xymask & 2) {
	    code = sget_real(nsp, format, index++, &rwy);
	    if (code < 0)
		break;
	} else
	    rwy.value.realval = 0;
	code = gs_rmoveto(igs, rwx.value.realval, rwy.value.realval);
	if (code < 0)
	    break;
    }
    /* An error occurred.  Clean up before returning. */
    return op_show_free(code);
}

/* ------ Initialization procedure ------ */

const op_def zchar2_op_defs[] =
{
    op_def_begin_level2(),
    {"1glyphshow", zglyphshow},
    {"2xshow", zxshow},
    {"2xyshow", zxyshow},
    {"2yshow", zyshow},
		/* Internal operators */
    {"0%xshow_continue", xshow_continue},
    {"0%yshow_continue", yshow_continue},
    {"0%xyshow_continue", xyshow_continue},
    op_def_end(0)
};
