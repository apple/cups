/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zcid.c,v 1.1 2000/03/08 23:15:32 mike Exp $ */
/* CID-keyed font operators */
#include "ghost.h"
#include "oper.h"
#include "gsmatrix.h"
#include "gsccode.h"
#include "gxfont.h"
#include "bfont.h"
#include "iname.h"
#include "store.h"

/* Imported from zfont42.c */
int build_gs_TrueType_font(P5(os_ptr op, font_type ftype,
			      const char *bcstr, const char *bgstr,
			      build_font_options_t options));

/* <string|name> <font_dict> .buildfont9/10 <string|name> <font> */
/* Build a type 9 or 10 (CID-keyed) font. */
/* Right now, we treat these like type 3 (with a BuildGlyph procedure). */
private int
build_gs_cid_font(os_ptr op, font_type ftype, const build_proc_refs * pbuild)
{
    int code;
    gs_font_base *pfont;

    check_type(*op, t_dictionary);
    code = build_gs_simple_font(op, &pfont, ftype, &st_gs_font_base,
				pbuild,
				bf_Encoding_optional |
				bf_FontBBox_required |
				bf_UniqueID_ignored);
    if (code < 0)
	return code;
    return define_gs_font((gs_font *) pfont);
}
private int
zbuildfont9(os_ptr op)
{
    build_proc_refs build;
    int code = build_proc_name_refs(&build, NULL, "%Type9BuildGlyph");

    if (code < 0)
	return code;
    return build_gs_cid_font(op, ft_CID_encrypted, &build);
}
private int
zbuildfont10(os_ptr op)
{
    build_proc_refs build;
    int code = build_gs_font_procs(op, &build);

    if (code < 0)
	return code;
    make_null(&build.BuildChar);	/* only BuildGlyph */
    return build_gs_cid_font(op, ft_CID_user_defined, &build);
}

/* <string|name> <font_dict> .buildfont11 <string|name> <font> */
private int
zbuildfont11(os_ptr op)
{
    return build_gs_TrueType_font(op, ft_CID_TrueType, (const char *)0,
				  "%Type11BuildGlyph",
				  bf_Encoding_optional |
				  bf_FontBBox_required |
				  bf_UniqueID_ignored |
				  bf_CharStrings_optional);
}

/* ------ Initialization procedure ------ */

const op_def zcid_op_defs[] =
{
    {"2.buildfont9", zbuildfont9},
    {"2.buildfont10", zbuildfont10},
    {"2.buildfont11", zbuildfont11},
    op_def_end(0)
};
