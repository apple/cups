/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zcfont.c,v 1.1 2000/03/08 23:15:31 mike Exp $ */
/* Composite font-related character operators */
#include "ghost.h"
#include "oper.h"
#include "gschar.h"
#include "gsmatrix.h"		/* for gxfont.h */
#include "gxfixed.h"		/* for gxfont.h */
#include "gxfont.h"
#include "gxchar.h"
#include "estack.h"
#include "ichar.h"
#include "ifont.h"
#include "igstate.h"
#include "store.h"

/* Forward references */
private int cshow_continue(P1(os_ptr));
private int cshow_restore_font(P1(os_ptr));

/* <proc> <string> cshow - */
private int
zcshow(os_ptr op)
{
    os_ptr proc_op = op - 1;
    os_ptr str_op = op;
    gs_show_enum *penum;
    int code;

    /*
     * Even though this is not documented anywhere by Adobe,
     * the Adobe interpreters apparently allow the string and
     * the procedure to be provided in either order!
     */
    if (r_is_proc(proc_op))
	;
    else if (r_is_proc(op)) {	/* operands reversed */
	proc_op = op;
	str_op = op - 1;
    } else {
	check_op(2);
	return_error(e_typecheck);
    }
    if ((code = op_show_setup(str_op, &penum)) != 0)
	return code;
    if ((code = gs_cshow_n_init(penum, igs, (char *)str_op->value.bytes,
				r_size(str_op))) < 0
	) {
	ifree_object(penum, "op_show_enum_setup");
	return code;
    }
    op_show_finish_setup(penum, 2, NULL);
    sslot = *proc_op;		/* save kerning proc */
    return cshow_continue(op - 2);
}
private int
cshow_continue(os_ptr op)
{
    gs_show_enum *penum = senum;
    int code;

    check_estack(4);		/* in case we call the procedure */
    code = gs_show_next(penum);
    if (code != gs_show_move) {
	code = op_show_continue_dispatch(op, code);
	if (code == o_push_estack)	/* must be gs_show_render */
	    make_op_estack(esp - 1, cshow_continue);
	return code;
    }
    /* Push the character code and width, and call the procedure. */
    {
	ref *pslot = &sslot;
	gs_point wpt;
	gs_font *font = gs_show_current_font(penum);
	gs_font *scaled_font;

	gs_show_current_width(penum, &wpt);
#if 0
		/****************
		 * The following code is logically correct (or at least,
		 * probably more correct than the code it replaces),
		 * but because of the issues about creating the scaled
		 * font in the correct VM space that make_font (in zfont.c)
		 * has to deal with, it creates references pointing to
		 * the wrong spaces.  Therefore, we've removed it.
		 *****************/
	{
	    int fdepth = penum->fstack.depth;

	    if (fdepth <= 0)
		scaled_font = font;	/* not composite */
	    else {
		code = gs_makefont(font->dir, font,
				   &penum->fstack.items[fdepth - 1].font->
				   FontMatrix, &scaled_font);
		if (code < 0)
		    return code;
	    }
	}
#else
	scaled_font = font;
#endif
	push(3);
	make_int(op - 2, gs_show_current_char(penum));
	make_real(op - 1, wpt.x);
	make_real(op, wpt.y);
	push_op_estack(cshow_continue);
	if (scaled_font != gs_rootfont(igs))
	    push_op_estack(cshow_restore_font);
	/* cshow does not change rootfont for user procedure */
	gs_set_currentfont(igs, scaled_font);
	*++esp = *pslot;	/* user procedure */
    }
    return o_push_estack;
}
private int
cshow_restore_font(os_ptr op)
{	/* We have 1 more entry on the e-stack (cshow_continue). */
    return gs_show_restore_font(esenum(esp - 1));
}

/* - rootfont <font> */
private int
zrootfont(os_ptr op)
{
    push(1);
    *op = *pfont_dict(gs_rootfont(igs));
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zcfont_op_defs[] =
{
    {"2cshow", zcshow},
    {"0rootfont", zrootfont},
		/* Internal operators */
    {"0%cshow_continue", cshow_continue},
    {"0%cshow_restore_font", cshow_restore_font},
    op_def_end(0)
};
