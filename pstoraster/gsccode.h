/* Copyright (C) 1993, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsccode.h,v 1.2 2000/03/08 23:14:34 mike Exp $ */
/* Types for character codes */

#ifndef gsccode_INCLUDED
#  define gsccode_INCLUDED

/*
 * Define a character code.  Normally this is just a single byte from a
 * string, but because of composite fonts, character codes must be
 * at least 32 bits.
 */
typedef ulong gs_char;

#define gs_no_char ((gs_char)~0L)

/*
 * Define a character glyph code, a.k.a. character name.
 * gs_glyphs from 0 to 2^31-1 are (PostScript) names; gs_glyphs 2^31 and
 * above are CIDs, biased by 2^31.
 */
typedef ulong gs_glyph;

#define gs_no_glyph ((gs_glyph)0x7fffffff)
#define gs_min_cid_glyph ((gs_glyph)0x80000000)
#define gs_max_glyph max_ulong

/* Define a procedure for marking a gs_glyph during garbage collection. */
typedef bool(*gs_glyph_mark_proc_t) (P2(gs_glyph glyph, void *proc_data));

/* Define a procedure for mapping a gs_glyph to its (string) name. */
#define gs_proc_glyph_name(proc)\
  const char *proc(P2(gs_glyph, uint *))
/* The following typedef is needed because ansi2knr can't handle */
/* gs_proc_glyph_name((*procname)) in a formal argument list. */
typedef gs_proc_glyph_name((*gs_proc_glyph_name_t));

/* Define a procedure for accessing the known encodings. */
#define gs_proc_known_encode(proc)\
  gs_glyph proc(P2(gs_char, int))
typedef gs_proc_known_encode((*gs_proc_known_encode_t));

/* Define the callback procedure vector for character to xglyph mapping. */
typedef struct gx_xfont_callbacks_s {
    gs_proc_glyph_name((*glyph_name));
    gs_proc_known_encode((*known_encode));
} gx_xfont_callbacks;

#endif /* gsccode_INCLUDED */
