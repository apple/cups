/* Copyright (C) 1989, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gxchar.h */
/* Internal character definition for Ghostscript library */
/* Requires gsmatrix.h, gxfixed.h */
#include "gschar.h"

/* The type of cached characters is opaque. */
#ifndef cached_char_DEFINED
#  define cached_char_DEFINED
typedef struct cached_char_s cached_char;
#endif

/* The type of cached font/matrix pairs is opaque. */
#ifndef cached_fm_pair_DEFINED
#  define cached_fm_pair_DEFINED
typedef struct cached_fm_pair_s cached_fm_pair;
#endif

/* The type of font objects is opaque. */
#ifndef gs_font_DEFINED
#  define gs_font_DEFINED
typedef struct gs_font_s gs_font;
#endif

/* The types of memory and null devices may be opaque. */
#ifndef gx_device_memory_DEFINED
#  define gx_device_memory_DEFINED
typedef struct gx_device_memory_s gx_device_memory;
#endif
#ifndef gx_device_null_DEFINED
#  define gx_device_null_DEFINED
typedef struct gx_device_null_s gx_device_null;
#endif

/*
 * Define the stack for composite fonts.
 * If the current font is not composite, depth = -1.
 * If the current font is composite, 0 <= depth <= max_font_depth.
 * items[0] through items[depth] are occupied.
 * items[0].font is the root font; items[0].index = 0.
 * The root font must be composite, but may be of any map type.
 * items[0..N-1] are modal composite fonts, for some N <= depth.
 * items[N..depth-1] are non-modal composite fonts.
 * items[depth] is a base (non-composite) font.
 * Note that if depth >= 0, the font member of the graphics state
 * for a base font BuildChar/Glyph is the same as items[depth].font.
 */
#define max_font_depth 5
typedef struct gx_font_stack_item_s {
	gs_font *font;		/* font at this level */
	uint index;		/* index of this font in parent's */
				/* Encoding */
} gx_font_stack_item;
typedef struct gx_font_stack_s {
	int depth;
	gx_font_stack_item items[1 + max_font_depth];
} gx_font_stack;

/* An enumeration object for string display. */
typedef enum {
	sws_none,
	sws_cache,			/* setcachedevice[2] */
	sws_no_cache,			/* setcharwidth */
	sws_cache_width_only		/* setcharwidth for xfont char */
} show_width_status;
struct gs_show_enum_s {
	/* Following are set at creation time */
	gs_state *pgs;
	int level;			/* save the level of pgs */
	gs_const_string str;
	float wcx, wcy;			/* for widthshow */
	gs_char wchr;			/* ditto */
	float ax, ay;			/* for ashow */
	bool add;			/* true if a[width]show */
	int do_kern;			/* 1 if kshow, -1 if [x][y]show */
					/* or cshow, 0 otherwise */
	bool slow_show;			/* [a][width]show or kshow or */
					/* [x][y]show or cshow */
	gs_char_path_mode charpath_flag;
	int stringwidth_flag;		/* 0 for show/charpath, */
					/* 1 for stringwidth, */
					/* -1 for cshow */
	int can_cache;			/* -1 if can't use cache at all, */
					/* 0 if can read but not load, */
					/* 1 if can read and load */
	gs_int_rect ibox;		/* int version of quick-check */
					/* (inner) clipping box */
	gs_int_rect obox;		/* int version of (outer) clip box */
	int ftx, fty;			/* transformed font translation */
	/* Following are updated dynamically */
	gs_glyph (*encode_char)(P3(gs_show_enum *, gs_font *, gs_char *));
					/* copied from font, */
					/* except for glyphshow */
	gs_log2_scale_point log2_suggested_scale; /* suggested scaling */
					/* factors for oversampling, */
					/* based on FontBBox and CTM */
	gx_device_memory *dev_cache;	/* cache device */
	gx_device_memory *dev_cache2;	/* underlying alpha memory device, */
					/* if dev_cache is an alpha buffer */
	gx_device_null *dev_null;	/* null device for stringwidth */
	uint index;			/* index within string */
	gs_char current_char;		/* current char for render or move */
	gs_glyph current_glyph;		/* current glyph ditto */
	gs_fixed_point wxy;		/* width of current char */
					/* in device coords */
	gs_fixed_point origin;		/* unrounded origin of current char */
					/* in device coords, needed for */
					/* charpath and WMode=1 */
	cached_char *cc;		/* being accumulated */
	gs_point width;			/* total width of string, set at end */
	show_width_status width_status;
	gs_log2_scale_point log2_current_scale;
	gx_font_stack fstack;
	int (*continue_proc)(P1(gs_show_enum *));	/* continuation procedure */
};
#define gs_show_enum_s_DEFINED
#define private_st_gs_show_enum() /* in gschar.c */\
  gs_private_st_composite(st_gs_show_enum, gs_show_enum, "gs_show_enum",\
    show_enum_enum_ptrs, show_enum_reloc_ptrs)

/* Cached character procedures (in gxccache.c and gxccman.c) */
#ifndef gs_font_dir_DEFINED
#  define gs_font_dir_DEFINED	
typedef struct gs_font_dir_s gs_font_dir;
#endif
cached_char *
	gx_alloc_char_bits(P7(gs_font_dir *, gx_device_memory *, gx_device_memory *, ushort, ushort, const gs_log2_scale_point *, int));
void gx_open_cache_device(P2(gx_device_memory *, cached_char *));
void gx_free_cached_char(P2(gs_font_dir *, cached_char *));
void gx_add_cached_char(P5(gs_font_dir *, gx_device_memory *, cached_char *, cached_fm_pair *, const gs_log2_scale_point *));
void gx_add_char_bits(P3(gs_font_dir *, cached_char *, const gs_log2_scale_point *));
cached_char *
	gx_lookup_cached_char(P5(const gs_font *, const cached_fm_pair *, gs_glyph, int, int));
cached_char *
	gx_lookup_xfont_char(P6(const gs_state *, cached_fm_pair *, gs_char, gs_glyph, const gx_xfont_callbacks *, int));
int gx_image_cached_char(P2(gs_show_enum *, cached_char *));
