/* Copyright (C) 1989, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxfont.h,v 1.2 2000/03/08 23:14:59 mike Exp $ */
/* Requires gsmatrix.h, gxdevice.h */

#ifndef gxfont_INCLUDED
#  define gxfont_INCLUDED

#include "gsccode.h"
#include "gsfont.h"
#include "gsuid.h"
#include "gsstruct.h"		/* for extern_st */
#include "gxftype.h"

/* A font object as seen by clients. */
/* See the PostScript Language Reference Manual for details. */

#ifndef gs_show_enum_DEFINED
#  define gs_show_enum_DEFINED
typedef struct gs_show_enum_s gs_show_enum;

#endif

/*
 * Fonts are "objects" to a limited extent, in that some of their
 * behavior is provided by a record of procedures in the font.
 * However, adding new types of fonts (subclasses) is not supported well.
 */

typedef struct gs_font_procs_s {

    /*
     * Define any needed procedure for initializing the composite
     * font stack in a show enumerator.  This is a no-op for
     * all but composite fonts.
     */

#define font_proc_init_fstack(proc)\
  int proc(P2(gs_show_enum *, gs_font *))
    font_proc_init_fstack((*init_fstack));

    /*
     * Define the font's algorithm for getting the next character from
     * a string being shown.  This is trivial, except for composite fonts.
     * Returns 0 if the current (base) font didn't change,
     * 1 if it did change, 2 if there are no more characters,
     * or an error code.
     *
     * This procedure is OBSOLETE as of release 4.61, superseded by
     * next_glyph; however, we have to continue supporting it for
     * backward compatibility.
     */

#define font_proc_next_char(proc)\
  int proc(P2(gs_show_enum *, gs_char *))
    font_proc_next_char((*next_char));

    /* A client-supplied character encoding procedure. */

#define font_proc_encode_char(proc)\
  gs_glyph proc(P3(gs_show_enum *, gs_font *, gs_char *))
    font_proc_encode_char((*encode_char));

    /*
     * A client-supplied BuildChar/BuildGlyph procedure.
     * The gs_char may be gs_no_char (for BuildGlyph), or the gs_glyph
     * may be gs_no_glyph (for BuildChar), but not both.
     */

#define font_proc_build_char(proc)\
  int proc(P5(gs_show_enum *, gs_state *, gs_font *, gs_char, gs_glyph))
    font_proc_build_char((*build_char));

    /* Callback procedures for external font rasterizers */
    /* (see gsccode.h for details.) */

    gx_xfont_callbacks callbacks;
    /*gs_proc_glyph_name((*glyph_name)); */

    /*
     * Define any special handling of gs_definefont.
     * We break this out so it can be different for composite fonts.
     */

#define font_proc_define_font(proc)\
  int proc(P2(gs_font_dir *, gs_font *))
                       font_proc_define_font((*define_font));

    /*
     * Define any special handling of gs_makefont.
     * We break this out so it can be different for composite fonts.
     */

#define font_proc_make_font(proc)\
  int proc(P4(gs_font_dir *, const gs_font *, const gs_matrix *,\
    gs_font **))
                       font_proc_make_font((*make_font));

    /*
     * Define the font's algorithm for getting the next character or
     * glyph from a string being shown.  We only use this if the
     * next_char procedure is 0 (for backward compatibility).
     */

#define font_proc_next_glyph(proc)\
  int proc(P3(gs_show_enum *, gs_char *, gs_glyph *))
                       font_proc_next_glyph((*next_glyph));

} gs_font_procs;

/* Default font procedures */
font_proc_init_fstack(gs_default_init_fstack);
font_proc_next_char(gs_default_next_char);
font_proc_encode_char(gs_no_encode_char);
font_proc_build_char(gs_no_build_char);
font_proc_define_font(gs_no_define_font);
font_proc_make_font(gs_no_make_font);
font_proc_make_font(gs_base_make_font);
font_proc_next_glyph(gs_default_next_glyph);

/* The font names are only needed for xfont lookup. */
typedef struct gs_font_name_s {
#define gs_font_name_max 47	/* must be >= 40 */
    /* The +1 is so we can null-terminate for debugging printout. */
    byte chars[gs_font_name_max + 1];
    uint size;
} gs_font_name;

/*
 * Define a generic font.  We include PaintType and StrokeWidth here because
 * they affect rendering algorithms outside the Type 1 font machinery.
 *
 * ****** NOTE: If you define any subclasses of gs_font, you *must* define
 * ****** the finalization procedure as gs_font_finalize.  Finalization
 * ****** procedures are not automatically inherited.
 */
#define gs_font_common\
	gs_font *next, *prev;		/* chain for original font list or */\
					/* scaled font cache */\
	gs_memory_t *memory;		/* allocator for this font */\
	gs_font_dir *dir;		/* directory where registered */\
	gs_font *base;			/* original (unscaled) base font */\
	void *client_data;		/* additional client data */\
	gs_matrix FontMatrix;\
	font_type FontType;\
	bool BitmapWidths;\
	fbit_type ExactSize, InBetweenSize, TransformedChar;\
	int WMode;			/* 0 or 1 */\
	int PaintType;			/* PaintType for Type 1/4/42 fonts, */\
					/* 0 for others */\
	float StrokeWidth;		/* StrokeWidth for Type 1/4/42 */\
					/* fonts (if present), 0 for others */\
	gs_font_procs procs;\
	/* We store both the FontDirectory key (key_name) and, */\
	/* if present, the FontName (font_name). */\
	gs_font_name key_name, font_name
					/*typedef struct gs_font_s gs_font; *//* in gsfont.h and other places */
struct gs_font_s {
    gs_font_common;
};

extern_st(st_gs_font);		/* (abstract) */
struct_proc_finalize(gs_font_finalize);		/* public for concrete subclasses */
#define public_st_gs_font()	/* in gsfont.c */\
  gs_public_st_complex_only(st_gs_font, gs_font, "gs_font",\
    0, font_enum_ptrs, font_reloc_ptrs, gs_font_finalize)
#define st_gs_font_max_ptrs 5
#define private_st_gs_font_ptr()	/* in gsfont.c */\
  gs_private_st_ptr(st_gs_font_ptr, gs_font *, "gs_font *",\
    font_ptr_enum_ptrs, font_ptr_reloc_ptrs)
#define st_gs_font_ptr_max_ptrs 1
extern_st(st_gs_font_ptr_element);
#define public_st_gs_font_ptr_element()	/* in gsfont.c */\
  gs_public_st_element(st_gs_font_ptr_element, gs_font *, "gs_font *[]",\
    font_ptr_element_enum_ptrs, font_ptr_element_reloc_ptrs, st_gs_font_ptr)

/* Define a base (not composite) font. */
#define gs_font_base_common\
	gs_font_common;\
	gs_rect FontBBox;\
	gs_uid UID;\
	int encoding_index;		/* 0=Std, 1=ISOLatin1, 2=Symbol, */\
					/* 3=Dingbats, -1=other */\
	int nearest_encoding_index	/* (may be >= 0 even if */\
				/* encoding_index = -1) */
#ifndef gs_font_base_DEFINED
#  define gs_font_base_DEFINED
typedef struct gs_font_base_s gs_font_base;

#endif
struct gs_font_base_s {
    gs_font_base_common;
};

extern_st(st_gs_font_base);
#define public_st_gs_font_base()	/* in gsfont.c */\
  gs_public_st_suffix_add1_final(st_gs_font_base, gs_font_base,\
    "gs_font_base", font_base_enum_ptrs, font_base_reloc_ptrs,\
    gs_font_finalize, st_gs_font, UID.xvalues)
#define st_gs_font_base_max_ptrs (st_gs_font_max_ptrs + 1)

#endif /* gxfont_INCLUDED */
