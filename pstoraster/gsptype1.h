/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsptype1.h,v 1.1 2000/03/13 18:57:56 mike Exp $ */
/* Client interface to PatternType 1 Patterns */

#ifndef gsptype1_INCLUDED
#  define gsptype1_INCLUDED

#include "gspcolor.h"
#include "gxbitmap.h"

/* ---------------- Types and structures ---------------- */

/* PatternType 1 template */
typedef struct gs_pattern1_template_s {
    gs_uid uid;			/* must be first, see gspcolor.h */
    int PaintType;		/* must be second, ditto */
    int TilingType;
    gs_rect BBox;
    float XStep;
    float YStep;
    int (*PaintProc) (P2(const gs_client_color *, gs_state *));
    void *client_data;		/* additional client data */
} gs_pattern1_template_t;

#define private_st_pattern1_template() /* in gspcolor.c */\
  gs_private_st_ptrs2(st_pattern1_template, gs_pattern1_template_t,\
    "PatternType 1 template", pattern_template_enum_ptrs,\
    pattern1_template_reloc_ptrs, uid.xvalues, client_data)
/* Backward compatibility */
typedef gs_pattern1_template_t gs_client_pattern;

/* ---------------- Procedures ---------------- */

/*
 * Construct a PatternType 1 Pattern color space.  If the base space is
 * NULL, the color space can only be used with colored patterns.
 */
extern int gs_cspace_build_Pattern1(
				       gs_color_space ** ppcspace,
				       const gs_color_space * pbase_cspace,
				       gs_memory_t * pmem
);

/* Initialize a PatternType 1 pattern. */
void gs_pattern1_init(P1(gs_pattern1_template_t *));

#define gs_client_pattern_init(ppat) gs_pattern1_init(ppat)

/*
 * Define versions of make_pattern and get_pattern specifically for
 * PatternType 1 patterns.
 *
 * The gs_memory_t argument for gs_makepattern may be NULL, meaning use the
 * same allocator as for the gs_state argument.  Note that gs_makepattern
 * uses rc_alloc_struct_1 to allocate pattern instances.
 */
int gs_makepattern(P5(gs_client_color *, const gs_client_pattern *,
		      const gs_matrix *, gs_state *, gs_memory_t *));
const gs_client_pattern *gs_getpattern(P1(const gs_client_color *));

/*
 * Make a pattern from a bitmap or pixmap. The pattern may be colored or
 * uncolored, as determined by the mask operand. This code is intended
 * primarily for use by PCL.
 *
 * By convention, if pmat is null the identity matrix will be used, and if
 * id is no_UniqueID the code will assign a unique id. Thes conventions allow
 * gs_makebitmappattern to be implemented as a macro. Also, if mem is a
 * null pointer, the memory allocator for the graphic state is used.
 *
 * For mask patterns, pix_depth must be 1, while pcspace and white_index are
 * ignored; the polarity of the mask considers ones part of the mask, while
 * zeros are not. For colored patterns pspace must point to an indexed color
 * space and the image must used the canoncial Decode array for this color
 * space. For both cases no interpolation or adjustment is provided.
 *
 * For backwards compatibility, if mask is false, pcspace is null, and
 * pix_depth is 1, the pattern will be rendered with a color space that maps
 * 0 to white and 1 to black.
 *
 * The image must be described by a gx_tile_bitmap structure (this is actually
 * somewhat awkward, but the only option available at the moment), and the
 * pattern step will exactly match the image size. The client need not maintain
 * the gx_tile_bitmap structure after the completion of this call, but the
 * raw image data itself must be kept until the pattern is no longer needed.
 *
 * NB: For proper handling of transparency in PCL, there must be only a single
 *     white value accessed by the pattern image. If the palette contains
 *     multiple white values, the PCL component must remap the image data to
 *     ensure that all white indices are mapped to the single, given white
 *     index.
 */
extern int gs_makepixmappattern(gs_client_color * pcc,
				const gs_depth_bitmap * pbitmap,
				bool mask,
				const gs_matrix * pmat,
				long id,
				const gs_color_space * pcspace,
				uint white_index,
				gs_state * pgs,
				gs_memory_t * mem
);

/*
 *  Backwards compatibility feature, to allow the existing
 *  gs_makebitmappattern operation to still function.
 */
extern int gs_makebitmappattern_xform(gs_client_color * pcc,
				      const gx_tile_bitmap * ptile,
				      bool mask,
				      const gs_matrix * pmat,
				      long id,
				      gs_state * pgs,
				      gs_memory_t * mem
);

#define gs_makebitmappattern(pcc, tile, mask, pgs, mem)                 \
    gs_makebitmappattern_xform(pcc, tile, mask, 0, no_UniqueID, pgs, mem)

#endif /* gsptype1_INCLUDED */
