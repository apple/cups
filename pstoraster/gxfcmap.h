/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxfcmap.h,v 1.1 2000/03/13 18:58:45 mike Exp $ */
/* Internal CMap data definition */

/* This file should be called gxcmap.h, except that name is already used. */

#ifndef gxfcmap_INCLUDED
#  define gxfcmap_INCLUDED

#include "gsfcmap.h"
#include "gsuid.h"

/*
 * The main body of data in a CMap is two code maps, one for defined
 * characters, one for notdefs.  Each code map is a multi-level tree,
 * one level per byte decoded from the input string.  Each node of
 * the tree may be:
 *      a character code (1-4 bytes)
 *      a character name (gs_glyph)
 *      a CID (gs_glyph)
 *      a subtree
 */
typedef enum {
    cmap_char_code,
    cmap_glyph,			/* character name or CID */
    cmap_subtree
} gx_code_map_type;
typedef struct gx_code_map_s gx_code_map;
struct gx_code_map_s {
    byte first;			/* first char code covered by this node */
    byte last;			/* last char code ditto */
         uint /*gx_code_map_type */ type:2;
    uint num_bytes1:2;		/* # of bytes -1 for char_code */
         uint /*bool */ add_offset:1;	/* if set, add char - first to ccode / glyph */
    /* We would like to combine the two unions into a union of structs, */
    /* but no compiler seems to do the right thing about packing. */
    union bd_ {
	byte font_index;	/* for leaf, font index */
	/* (only non-zero if rearranged font) */
	byte count1;		/* for subtree, # of entries -1 */
    } byte_data;
    union d_ {
	gs_char ccode;		/* num_bytes bytes */
	gs_glyph glyph;
	gx_code_map *subtree;	/* [count] */
    } data;
    gs_cmap *cmap;		/* point back to CMap for GC mark proc */
};

/* The GC information for a gx_code_map is complex, because names must be */
/* traced. */
extern_st(st_code_map);
extern_st(st_code_map_element);
#define public_st_code_map()	/* in gsfcmap.c */\
  gs_public_st_composite(st_code_map, gx_code_map, "gx_code_map",\
    code_map_enum_ptrs, code_map_reloc_ptrs)
#define public_st_code_map_element() /* in gsfcmap.c */\
  gs_public_st_element(st_code_map_element, gx_code_map, "gx_code_map[]",\
    code_map_elt_enum_ptrs, code_map_elt_reloc_ptrs, st_code_map)

/* A CMap proper is relatively simple. */
struct gs_cmap_s {
    gs_cid_system_info CIDSystemInfo;	/* must be first */
    gs_uid uid;
    int WMode;
    gx_code_map def;		/* defined characters (cmap_subtree) */
    gx_code_map notdef;		/* notdef characters (cmap_subtree) */
    gs_glyph_mark_proc_t mark_glyph;	/* glyph marking procedure for GC */
    void *mark_glyph_data;	/* closure data */
};

/*extern_st(st_cmap); */
#define public_st_cmap()	/* in gsfcmap.c */\
  gs_public_st_suffix_add4(st_cmap, gs_cmap, "gs_cmap",\
    cmap_enum_ptrs, cmap_reloc_ptrs, st_cid_system_info,\
    uid.xvalues, def.data.subtree, notdef.data.subtree, mark_glyph_data)

#endif /* gxfcmap_INCLUDED */
