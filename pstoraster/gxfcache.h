/* Copyright (C) 1992, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gxfcache.h */
/* Definitions for Ghostscript font and character caches */
/* Requires gsfont.h */
#include "gsuid.h"
#include "gsxfont.h"
#include "gxbcache.h"

/* ------ Font/matrix pair cache entry ------ */

#ifndef cached_fm_pair_DEFINED
#  define cached_fm_pair_DEFINED
typedef struct cached_fm_pair_s cached_fm_pair;
#endif

/*
 * Define the entry for a cached (font,matrix) pair.  If the UID
 * is valid, the font pointer may be 0, since we keep entries even for
 * fonts unloaded by a restore if they have valid UIDs.
 * We can't use the address of the pair for the hash value,
 * since the GC may move pairs in storage, so we create a hash
 * when we allocate the pair initially.
 */
struct cached_fm_pair_s {
	gs_font *font;			/* base font */
	gs_uid UID;			/* font UniqueID or XUID */
	uint hash;			/* hash for this pair */
	float mxx, mxy, myx, myy;	/* transformation */
	int num_chars;			/* # of cached chars with this */
					/* f/m pair */
	bool xfont_tried;		/* true if we looked up an xfont */
	gx_xfont *xfont;		/* the xfont (if any) */
	gs_memory_t *memory;		/* the allocator for the xfont */
	uint index;			/* index of this pair in mdata */
};
#define private_st_cached_fm_pair() /* in gxccman.c */\
  gs_private_st_ptrs3(st_cached_fm_pair, cached_fm_pair,\
    "cached_fm_pair", fm_pair_enum_ptrs, fm_pair_reloc_ptrs,\
    font, UID.xvalues, xfont)
#define private_st_cached_fm_pair_elt()	/* in gxccman.c */\
  gs_private_st_element(st_cached_fm_pair_element, cached_fm_pair,\
    "cached_fm_pair[]", fm_pair_element_enum_ptrs, fm_pair_element_reloc_ptrs,\
    st_cached_fm_pair)
/* If font == 0 and UID is invalid, this is a free entry. */
#define fm_pair_is_free(pair)\
  ((pair)->font == 0 && !uid_is_valid(&(pair)->UID))
#define fm_pair_set_free(pair)\
  ((pair)->font = 0, uid_set_invalid(&(pair)->UID))
#define fm_pair_init(pair)\
  (fm_pair_set_free(pair), (pair)->xfont_tried = false, (pair)->xfont = 0)

/* The font/matrix pair cache itself. */
typedef struct fm_pair_cache_s {
	uint msize, mmax;		/* # of cached font/matrix pairs */
	cached_fm_pair *mdata;
	uint mnext;			/* rover for allocating font/matrix pairs */
} fm_pair_cache;

/* ------ Character cache entry ------- */

/* Define the allocation chunk type. */
typedef gx_bits_cache_chunk char_cache_chunk;

/*
 * This is a subclass of the entry in a general bitmap cache.
 * The character cache contains both used and free blocks.
 * All blocks have a common header; free blocks have ONLY the header.
 */
typedef gx_cached_bits_head cached_char_head;
#define cc_head_is_free(cch) cb_head_is_free(cch)
#define cc_head_set_free(cch) cb_head_set_free(cch)
/*
 * Define the cache entry for an individual character.
 * The bits, if any, immediately follow the structure;
 * characters with only xfont definitions may not have bits.
 * An entry is 'real' if it is not free and if pair != 0.
 * We maintain the invariant that at least one of the following must be true
 * for all real entries:
 *	- cc_has_bits(cc);
 *	- cc->xglyph != gx_no_xglyph && cc_pair(cc)->xfont != 0.
 */
#ifndef cached_char_DEFINED
#  define cached_char_DEFINED
typedef struct cached_char_s cached_char;
#endif
struct cached_char_s {

		/* The code, font/matrix pair, wmode, and depth */
		/* are the 'key' in the cache. */
		/* gx_cached_bits_common includes depth. */

	gx_cached_bits_common;		/* (must be first) */
#define cc_depth(cc) ((cc)->cb_depth)
#define cc_set_depth(cc, d) ((cc)->cb_depth = (d))
	cached_fm_pair *pair;
#define cc_pair(cc) ((cc)->pair)
#define cc_set_pair_only(cc, p) ((cc)->pair = (p))
	gs_glyph code;			/* glyph code */
	byte wmode;			/* writing mode (0 or 1) */

		/* The following are neither 'key' nor 'value'. */

	char_cache_chunk *chunk;	/* chunk where this char */
					/* is allocated */
	uint loc;			/* relative location in chunk */
	uint pair_index;		/* index of pair in mdata */

		/* The rest of the structure is the 'value'. */
		/* gx_cached_bits_common has width, height, raster, */
		/* shift (not used here), id. */

#define cc_raster(cc) ((cc)->raster)
#define cc_set_raster(cc, r) ((cc)->raster = (r))
	gx_xglyph xglyph;		/* the xglyph for the xfont, if any */
	gs_fixed_point wxy;		/* width in device coords */
	gs_fixed_point offset;		/* (-llx, -lly) in device coords */
};
#define cc_is_free(cc) cc_head_is_free(&(cc)->head)
#define cc_set_free(cc) cc_head_set_free(&(cc)->head)
#define cc_set_pair(cc, p)\
  ((cc)->pair_index = ((cc)->pair = (p))->index)
#define cc_has_bits(cc) ((cc)->id != gx_no_bitmap_id)
/*
 * Memory management for cached_chars is a little unusual.
 * cached_chars are never instantiated on their own; a pointer to
 * a cached_char points into the middle of a cache chunk.
 * Consequently, such pointers can't be traced or relocated
 * in the usual way.  What we do instead is allocate the cache
 * outside garbage-collectable space; we do all the tracing and relocating
 * of pointers *from* the cache (currently only the head.pair pointer)
 * when we trace or relocate the font "directory" that owns the cache.
 *
 * Since cached_chars are (currently) never instantiated on their own,
 * they only have a descriptor so that cached_char_ptr can trace them.
 */
#define private_st_cached_char() /* in gxccman.c */\
  gs_private_st_composite(st_cached_char, cached_char, "cached_char",\
    cached_char_enum_ptrs, cached_char_reloc_ptrs)
#define private_st_cached_char_ptr() /* in gxccman.c */\
  gs_private_st_composite(st_cached_char_ptr, cached_char *,\
    "cached_char *", cc_ptr_enum_ptrs, cc_ptr_reloc_ptrs)
#define private_st_cached_char_ptr_elt() /* in gxccman.c */\
  gs_private_st_element(st_cached_char_ptr_element, cached_char *,\
    "cached_char *[]", cc_ptr_element_enum_ptrs, cc_ptr_element_reloc_ptrs,\
    st_cached_char_ptr)

/*
 * Define the alignment and size of the cache structures.
 */
#define align_cached_char_mod align_cached_bits_mod
#define sizeof_cached_char\
  round_up(sizeof(cached_char), align_cached_char_mod)
#define cc_bits(cc) ((byte *)(cc) + sizeof_cached_char)

/* Define the hash index for a (glyph, fm_pair) key. */
#define chars_head_index(glyph, pair)\
  ((uint)(glyph) * 59 + (pair)->hash * 73)	/* scramble it a bit */

/* Define the glyph marking procedure for the GC. */
typedef bool (*cc_mark_glyph_proc_t)(P1(gs_glyph));

/* ------ Character cache ------ */

/*
 * So that we can find all the entries in the cache without
 * following chains of pointers, we use open hashing rather than
 * chained hashing for the lookup table.
 */
typedef struct char_cache_s {
		/* gx_bits_cache_common provides chunks, cnext, */
		/* bsize, csize. */
	gx_bits_cache_common;
	gs_memory_t *memory;
	cached_char **table;		/* hash table */
	uint table_mask;		/* (a power of 2 -1) */
	uint bmax;			/* max bsize */
	uint cmax;			/* max csize */
	uint bspace;			/* space allocated for chunks */
	uint lower;			/* min size at which cached chars */
					/* should be stored compressed */
	uint upper;			/* max size of a single cached char */
	cc_mark_glyph_proc_t mark_glyph;
} char_cache;

/* ------ Font/character cache ------ */

/* A font "directory" (font/character cache manager). */
struct gs_font_dir_s {

		/* Original (unscaled) fonts */

	gs_font *orig_fonts;

		/* Scaled font cache */

	gs_font *scaled_fonts;		/* list of recently scaled fonts */
	uint ssize, smax;

		/* Font/matrix pair cache */

	fm_pair_cache fmcache;

		/* Character cache */

	char_cache ccache;
};
#define private_st_font_dir()	/* in gsfont.c */\
  gs_private_st_composite(st_font_dir, gs_font_dir, "gs_font_dir",\
    font_dir_enum_ptrs, font_dir_reloc_ptrs)

/* Enumerate the pointers in a font directory, except for orig_fonts. */
#define font_dir_do_ptrs(m)\
  /*m(-,orig_fonts)*/ m(0,scaled_fonts) m(1,fmcache.mdata)\
  m(2,ccache.table)
#define st_font_dir_max_ptrs 3

/* Character cache procedures (in gxccache.c and gxccman.c) */
int gx_char_cache_alloc(P6(gs_memory_t *, gs_font_dir *, uint, uint, uint, uint));
void gx_char_cache_init(P1(gs_font_dir *));
void gx_purge_selected_cached_chars(P3(gs_font_dir *, bool (*)(P2(cached_char *, void *)), void *));
cached_fm_pair *
	gx_lookup_fm_pair(P2(gs_font *, const gs_state *));
cached_fm_pair *
	gx_add_fm_pair(P4(gs_font_dir *, gs_font *, const gs_uid *, const gs_state *));
void gx_lookup_xfont(P3(const gs_state *, cached_fm_pair *, int));
void gs_purge_fm_pair(P3(gs_font_dir *, cached_fm_pair *, int));
