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

/*$Id: gsfcmap.c,v 1.1 2000/03/08 23:14:40 mike Exp $ */
/* CMap character decoding */
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gxfcmap.h"

/* CMap structure descriptors */
public_st_cmap();
public_st_code_map();
public_st_code_map_element();

#define pcmap ((gx_code_map *)vptr)
/* Because code maps can be elements of arrays, */
/* their enum_ptrs procedure must never return 0 prematurely. */
private 
ENUM_PTRS_BEGIN(code_map_enum_ptrs) return 0;
ENUM_PTR(0, gx_code_map, cmap);
case 1:
switch (pcmap->type)
{
    case cmap_glyph:
	(*pcmap->cmap->mark_glyph)(pcmap->data.glyph,
				   pcmap->cmap->mark_glyph_data);
    default:
	ENUM_RETURN(0);
    case cmap_subtree:
	ENUM_RETURN_PTR(gx_code_map, data.subtree);
}
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(code_map_reloc_ptrs);
switch (pcmap->type) {
    case cmap_subtree:
	RELOC_PTR(gx_code_map, data.subtree);
	break;
    default:
	;
}
RELOC_PTR(gx_code_map, cmap);
RELOC_PTRS_END
#undef pcmap

/* CIDSystemInfo structure */
public_st_cid_system_info();

/* ---------------- Procedures ---------------- */

/*
 * Decode a character from a string using a code map, updating the index.
 * Return 0 for a CID or name, N > 0 for a character code where N is the
 * number of bytes in the code, or an error.  For undefined characters,
 * we set *pglyph = gs_no_glyph and return 0.
 */
private int
code_map_decode_next(const gx_code_map * pcmap, const gs_const_string * str,
		     uint * pindex, uint * pfidx,
		     gs_char * pchr, gs_glyph * pglyph)
{
    const gx_code_map *map = pcmap;
    uint chr = 0;

    for (;;) {
	int result;

	if_debug1('J', "[J]cmap char = 0x%x: ", chr);
	switch ((gx_code_map_type) map->type) {
	    case cmap_char_code:
		if_debug0('J', "char code");
		*pglyph = (gs_glyph)map->data.ccode;
		result = map->num_bytes1 + 1;
leaf:		if (chr > map->last)
		    goto undef;
		if (map->add_offset)
		    *pglyph += chr - map->first;
		*pfidx = map->byte_data.font_index;
		*pchr = chr & 0xff;
		if_debug3('J', " 0x%lx, fidx %u, result %d\n",
			  *pglyph, *pfidx, result);
		return result;
	    case cmap_glyph:
		if_debug0('J', "glyph");
		*pglyph = map->data.glyph;
		result = 0;
		goto leaf;
	    case cmap_subtree:
		if_debug0('J', "subtree\n");
		if (*pindex >= str->size)
		    return_error(gs_error_rangecheck);
		chr = str->data[(*pindex)++];
		if (chr >= map->data.subtree[0].first) {
		    /* Invariant: map[lo].first <= chr < map[hi].first. */
		    uint lo = 0, hi = map->byte_data.count1 + 1;

		    map = map->data.subtree;
		    while (lo + 1 < hi) {
			uint mid = (lo + hi) >> 1;

			if (chr >= map[mid].first)
			    lo = mid;
			else
			    hi = mid;
		    }
		    map = &map[lo];
		    continue;
		}
undef:		if_debug0('J', " undef\n");
		*pchr = 0;
		*pglyph = gs_no_glyph;
		return 0;
	    default:		/* (can't happen) */
		if_debug0('J', "error!\n");
		return_error(gs_error_invalidfont);
	}
    }
}

/*
 * Decode a character from a string using a CMap.
 * Return like code_map_decode_next.
 */
int
gs_cmap_decode_next(const gs_cmap * pcmap, const gs_const_string * str,
		    uint * pindex, uint * pfidx,
		    gs_char * pchr, gs_glyph * pglyph)
{
    uint save_index = *pindex;
    int code =
	code_map_decode_next(&pcmap->def, str, pindex, pfidx, pchr, pglyph);

    if (code != 0 || *pglyph != gs_no_glyph)
	return code;
    /* This is an undefined character.  Use the notdef map. */
    {
	uint next_index = *pindex;

	*pindex = save_index;
	code =
	    code_map_decode_next(&pcmap->notdef, str, pindex, pfidx,
				 pchr, pglyph);
	*pindex = next_index;
    }
    return code;
}
