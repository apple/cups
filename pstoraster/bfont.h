/* Copyright (C) 1992, 1995, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: bfont.h,v 1.2 2000/03/08 23:14:19 mike Exp $ */
/* Interpreter internal routines and data needed for building fonts */
/* Requires gxfont.h */

#ifndef bfont_INCLUDED
#  define bfont_INCLUDED

#include "ifont.h"

/* In zfont.c */
int add_FID(P2(ref * pfdict, gs_font * pfont));

font_proc_make_font(zdefault_make_font);
font_proc_make_font(zbase_make_font);
/* The global font directory */
extern gs_font_dir *ifont_dir;

/* Structure for passing BuildChar and BuildGlyph procedures. */
typedef struct build_proc_refs_s {
    ref BuildChar;
    ref BuildGlyph;
} build_proc_refs;

/* Options for collecting parameters from a font dictionary. */
/* The comment indicates where the option is tested. */
typedef enum {
    bf_options_none = 0,
    bf_Encoding_optional = 1,	/* build_gs_font */
    bf_FontBBox_required = 2,	/* build_gs_simple_font */
    bf_UniqueID_ignored = 4,	/* build_gs_simple_font */
    bf_CharStrings_optional = 8,	/* build_gs_primitive_font */
    bf_notdef_required = 16	/* build_gs_primitive_font */
} build_font_options_t;

/* In zfont2.c */
int build_proc_name_refs(P3(build_proc_refs * pbuild,
			    const char *bcstr,
			    const char *bgstr));
int build_gs_font_procs(P2(os_ptr, build_proc_refs *));
int build_gs_primitive_font(P6(os_ptr, gs_font_base **, font_type,
			       gs_memory_type_ptr_t, const build_proc_refs *,
			       build_font_options_t));
int build_gs_simple_font(P6(os_ptr, gs_font_base **, font_type,
			    gs_memory_type_ptr_t, const build_proc_refs *,
			    build_font_options_t));
void lookup_gs_simple_font_encoding(P1(gs_font_base *));
int build_gs_font(P6(os_ptr, gs_font **, font_type,
		     gs_memory_type_ptr_t, const build_proc_refs *,
		     build_font_options_t));
int define_gs_font(P1(gs_font *));

#endif /* bfont_INCLUDED */
