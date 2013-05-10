/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gsdcolor.h */
/* Device color representation for drivers */

#ifndef gsdcolor_INCLUDED
#  define gsdcolor_INCLUDED

#include "gxarith.h"		/* for imod */
#include "gxbitmap.h"
#include "gxhttile.h"
#include "gxcindex.h"

#ifndef gx_device_color_DEFINED
#  define gx_device_color_DEFINED
typedef struct gx_device_color_s gx_device_color;
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * The definitions in the following section of the file are the only
 * ones that should be used by read-only clients such as implementors
 * of high-level driver functions.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * A device color consists of a base color and an optional (tiled) mask.
 * The base color may be a pure color, a binary halftone, or a colored
 * bitmap (color halftone or colored Pattern).  The mask is used for
 * both colored and uncolored Patterns.
 */

/* Accessing a pure color. */
#define gx_dc_is_pure(pdc)\
  ((pdc)->type == gx_dc_type_pure)
#define gx_dc_writes_pure(pdc, lop)\
  (gx_dc_is_pure(pdc) && lop_no_S_is_T(lop))
#define gx_dc_pure_color(pdc)\
  ((pdc)->colors.pure)

/* Accessing the phase of a halftone. */
#define gx_dc_phase(pdc)\
  ((pdc)->phase)

/* Accessing a binary halftone. */
#define gx_dc_is_binary_halftone(pdc)\
  ((pdc)->type == gx_dc_type_ht_binary)
#define gx_dc_binary_tile(pdc)\
  (&(pdc)->colors.binary.b_tile->tiles)
#define gx_dc_binary_color0(pdc)\
  ((pdc)->colors.binary.color[0])
#define gx_dc_binary_color1(pdc)\
  ((pdc)->colors.binary.color[1])

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * The definitions in the following section of the file, plus the ones
 * just above, are the only ones that should be used by clients that
 * set as well as read device colors.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define color_is_set(pdc)\
  ((pdc)->type != gx_dc_type_none)
#define color_unset(pdc)\
  ((pdc)->type = gx_dc_type_none)

#define color_is_pure(pdc) gx_dc_is_pure(pdc)
#define color_writes_pure(pdc, lop) gx_dc_writes_pure(pdc, lop)
#define color_set_pure(pdc, color)\
  ((pdc)->colors.pure = (color),\
   (pdc)->type = gx_dc_type_pure)

/* Set the phase to an offset from the tile origin. */
#define color_set_phase(pdc, px, py)\
  ((pdc)->phase.x = (px),\
   (pdc)->phase.y = (py))
/* Set the phase from the halftone phase in a graphics state. */
#define color_set_phase_mod(pdc, px, py, tw, th)\
  color_set_phase(pdc, imod(-(px), tw), imod(-(py), th))

#define color_is_binary_halftone(pdc) gx_dc_is_binary_halftone(pdc)
#define color_set_binary_halftone(pdc, ht, color0, color1, level)\
  ((pdc)->colors.binary.b_ht = (ht),\
   (pdc)->colors.binary.color[0] = (color0),\
   (pdc)->colors.binary.color[1] = (color1),\
   (pdc)->colors.binary.b_level = (level),\
   (pdc)->type = gx_dc_type_ht_binary)
#define color_set_binary_tile(pdc, color0, color1, tile)\
  ((pdc)->colors.binary.b_ht = 0,\
   (pdc)->colors.binary.color[0] = (color0),\
   (pdc)->colors.binary.color[1] = (color1),\
   (pdc)->colors.binary.b_tile = (tile),\
   (pdc)->type = gx_dc_type_ht_binary)

#define color_is_colored_halftone(pdc)\
  ((pdc)->type == gx_dc_type_ht_colored)
#define _color_set_c(pdc, i, b, l)\
  ((pdc)->colors.colored.c_base[i] = (b),\
   (pdc)->colors.colored.c_level[i] = (l))
#define color_set_rgb_halftone(pdc, ht, br, lr, bg, lg, bb, lb, a)\
  ((pdc)->colors.colored.c_ht = (ht),\
   _color_set_c(pdc, 0, br, lr),\
   _color_set_c(pdc, 1, bg, lg),\
   _color_set_c(pdc, 2, bb, lb),\
   (pdc)->colors.colored.alpha = (a),\
   (pdc)->type = gx_dc_type_ht_colored)
#define color_set_cmyk_halftone(pdc, ht, bc, lc, bm, lm, by, ly, bk, lk)\
  ((pdc)->colors.colored.c_ht = (ht),\
   _color_set_c(pdc, 0, bc, lc),\
   _color_set_c(pdc, 1, bm, lm),\
   _color_set_c(pdc, 2, by, ly),\
   _color_set_c(pdc, 3, bk, lk),\
   (pdc)->colors.colored.alpha = max_ushort,\
   (pdc)->type = gx_dc_type_ht_colored)

#define color_set_pattern(pdc, pid, pt)\
 ((pdc)->id = (pid),\
  (pdc)->colors.pattern.p_tile = (pt),\
  (pdc)->type = gx_dc_type_pattern)
#define color_set_null_pattern(pdc)\
  color_set_pattern(pdc, gx_no_bitmap_id, (gx_color_tile *)0)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * The remaining definitions are internal ones that are included in this
 * file only because C's abstraction mechanisms aren't strong enough to
 * allow us to keep them separate and still have in-line access to the
 * commonly used members.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Define opaque types for objects referenced by device colors. */

#ifndef gx_ht_tile_DEFINED
#  define gx_ht_tile_DEFINED
typedef struct gx_ht_tile_s gx_ht_tile;
#endif

#ifndef gx_device_halftone_DEFINED
#  define gx_device_halftone_DEFINED
typedef struct gx_device_halftone_s gx_device_halftone;
#endif

typedef struct gx_color_tile_s gx_color_tile;

/*
 * The device color in the graphics state is computed from client color
 * specifications, and kept current through changes in transfer function,
 * device, and (if relevant) halftone phase.
 * (gx_set_dev_color sets the device color if needed.)
 * For binary halftones (and eventually colored halftones as well),
 * the bitmaps are only cached, so internal clients (the painting operators)
 * must call gx_color_load to ensure that the bitmap is available.
 * Device color elements set by gx_color_load are marked with @ below.
 *
 * Base colors are represented as follows:
 *
 *	Pure color (gx_dc_pure):
 *		colors.pure = the color;
 *	Binary halftone (gx_dc_ht_binary):
 *		colors.binary.b_ht = the device halftone;
 *		colors.binary.color[0] = the color for 0s (darker);
 *		colors.binary.color[1] = the color for 1s (lighter);
 *		colors.binary.b_level = the number of pixels to lighten,
 *		  0 < halftone_level < P, the number of pixels in the tile;
 *	@	colors.binary.b_tile points to an entry in the binary
 *		  tile cache.
 *	Colored halftone (gx_dc_ht_colored):
 *		colors.colored.c_ht = the device halftone;
 *		colors.colored.c_level[0..N-1] = the halftone levels,
 *		  like b_level;
 *		colors.colored.c_base[0..N-1] = the base colors;
 *		  N=3 for RGB devices, 4 for CMYK devices;
 *		  0 <= c_level[i] < P;
 *		  0 <= c_base[i] <= dither_rgb;
 *		colors.colored.alpha = the opacity.
 *	Colored pattern (gx_dc_pattern):
 *		(id and mask are also set, see below)
 *	@	colors.pattern.p_tile points to a gx_color_tile in
 *		  the pattern cache, or is NULL for a null pattern.
 *
 * The phase element is used for all colors except pure ones.  It holds
 * the negative of the graphics state halftone phase, modulo the halftone
 * tile or colored pattern size.
 *
 * The id and mask elements of a device color are only used for patterns:
 *	Non-pattern:
 *		id and mask are unused.
 *	Pattern:
 *		id gives the ID of the pattern (and its mask);
 *	@	mask points to a gx_color_tile in the pattern cache,
 *		  or is NULL for a pattern that doesn't require a mask.
 *		  (The 'bits' of the tile are not accessed.)
 *		  For colored patterns requiring a mask, p_tile and mask
 *		  point to the same cache entry.
 * For masked colors, gx_set_dev_color replaces the type with a different
 * type that applies the mask when painting.  These types are not defined
 * here, because they are only used in Level 2.
 */

/* A device color type is just a pointer to the procedures. */
typedef struct gx_device_color_procs_s gx_device_color_procs;
typedef const gx_device_color_procs _ds *gx_device_color_type;

struct gx_device_color_s {
	/* See the comment above for descriptions of the members. */
	/* We use b_, c_, and p_ member names because */
	/* some old compilers don't allow the same name to be used for */
	/* two different structure members even when it's unambiguous. */
	union _c {
		gx_color_index pure;
		struct _bin {
			const gx_device_halftone *b_ht;
			gx_color_index color[2];
			uint b_level;
			gx_ht_tile *b_tile;
		} binary;
		struct _col {
			const gx_device_halftone *c_ht;
			byte c_base[4];
			uint c_level[4];
			ushort /*gx_color_value*/ alpha;
		} colored;
		struct _pat {
			gx_color_tile *p_tile;
		} /*(colored)*/ pattern;
	} colors;
	gs_int_point phase;
	gx_bitmap_id id;
	gx_color_tile *mask;
	/* We put the type last to preserve word alignment */
	/* on platforms with short ints. */
	gx_device_color_type type;
};
/*extern_st(st_device_color);*/		/* in gxdcolor.h */
#define public_st_device_color() /* in gxcmap.c */\
  gs_public_st_composite(st_device_color, gx_device_color, "gx_device_color",\
    device_color_enum_ptrs, device_color_reloc_ptrs)
#define st_device_color_max_ptrs 2

/*
 * Define the standard device color types.
 * We define them here as pointers to the real types only because a few
 * C compilers don't allow declaring externs with abstract struct types;
 * we redefine them as macros in gxdcolor.h where the concrete type for
 * gx_device_color_procs is available.
 * We spell out the definition of gx_device_color type because some
 * C compilers can't handle the typedef correctly.
 */
#ifndef gx_dc_type_none
extern const gx_device_color_procs _ds *gx_dc_type_none;  /* gxdcolor.c */
#endif
#ifndef gx_dc_type_pure
extern const gx_device_color_procs _ds *gx_dc_type_pure;  /* gxdcolor.c */
#endif
		/*
		 * We don't declare gx_dc_pattern here, so as not to create
		 * a spurious external reference in Level 1 systems.
		 */
#ifndef gx_dc_type_pattern
/*extern const gx_device_color_procs _ds *gx_dc_type_pattern;*/  /* gspcolor.c */
#endif
#ifndef gx_dc_type_ht_binary
extern const gx_device_color_procs _ds *gx_dc_type_ht_binary;  /* gxht.c */
#endif
#ifndef gx_dc_type_ht_colored
extern const gx_device_color_procs _ds *gx_dc_type_ht_colored;  /* gxcht.c */
#endif

#endif					/* gsdcolor_INCLUDED */
