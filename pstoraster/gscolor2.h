/* Copyright (C) 1992, 1993 Aladdin Enterprises.  All rights reserved.
  
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

/* gscolor2.h */
/* Client interface to Level 2 color facilities */
/* (requires gscspace.h, gsmatrix.h) */
#include "gsccolor.h"
#include "gsuid.h"		/* for pattern template */
#include "gxbitmap.h"		/* for makebitmappattern */

/* Note: clients should use rc_alloc_struct_0 (in gsrefct.h) to allocate */
/* CIE color spaces or rendering structures; makepattern uses */
/* rc_alloc_struct_1 to allocate pattern instances. */

/* General color routines */
const gs_color_space *gs_currentcolorspace(P1(const gs_state *));
int	gs_setcolorspace(P2(gs_state *, gs_color_space *));
const gs_client_color *gs_currentcolor(P1(const gs_state *));
int	gs_setcolor(P2(gs_state *, const gs_client_color *));
bool	gs_currentoverprint(P1(const gs_state *));
void	gs_setoverprint(P2(gs_state *, bool));

/* CIE-specific routines */
#ifndef gs_cie_render_DEFINED
#  define gs_cie_render_DEFINED
typedef struct gs_cie_render_s gs_cie_render;
#endif
const gs_cie_render *gs_currentcolorrendering(P1(const gs_state *));
int	gs_setcolorrendering(P2(gs_state *, gs_cie_render *));

/* Pattern template */
typedef struct gs_client_pattern_s {
	gs_uid uid;		/* XUID or nothing */
	int PaintType;
	int TilingType;
	gs_rect BBox;
	float XStep;
	float YStep;
	int (*PaintProc)(P2(const gs_client_color *, gs_state *));
	void *client_data;		/* additional client data */
} gs_client_pattern;
#define private_st_client_pattern() /* in gspcolor.c */\
  gs_private_st_ptrs1(st_client_pattern, gs_client_pattern,\
    "client pattern", client_pattern_enum_ptrs, client_pattern_reloc_ptrs,\
    client_data)
#define st_client_pattern_max_ptrs 1

/* Pattern-specific routines */
/* The gs_memory_t argument for makepattern may be null, meaning use the */
/* same allocator as for the gs_state argument. */
int	gs_makepattern(P5(gs_client_color *, const gs_client_pattern *,
			  const gs_matrix *, gs_state *, gs_memory_t *));
int	gs_setpattern(P2(gs_state *, const gs_client_color *));
int	gs_setpatternspace(P1(gs_state *));
const gs_client_pattern	*gs_getpattern(P1(const gs_client_color *));

/* makebitmappattern is a hack for PCL printing. */
/* If the Boolean argument is true, the result is a mask; */
/* if false, the result is a black-and-white solid pattern */
/* (with black=1). */
int	gs_makebitmappattern(P5(gs_client_color *, const gx_tile_bitmap *,
				bool, gs_state *, gs_memory_t *));
