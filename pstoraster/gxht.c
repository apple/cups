/* Copyright (C) 1989, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gxht.c */
/* Halftone rendering routines for Ghostscript imaging library */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsbitops.h"
#include "gsutil.h"			/* for gs_next_ids */
#include "gxfixed.h"
#include "gzstate.h"
#include "gxdevice.h"			/* for gzht.h */
#include "gzht.h"

/* Define the sizes of the halftone cache. */
#define max_cached_tiles_HUGE 5000	/* not used */
#define max_ht_bits_HUGE 1000000	/* not used */
#define max_cached_tiles_LARGE 577
#define max_ht_bits_LARGE 100000
#define max_cached_tiles_SMALL 25
#define max_ht_bits_SMALL 1000

/* Define the binary halftone device color type. */
private dev_color_proc_load(gx_dc_ht_binary_load);
private dev_color_proc_fill_rectangle(gx_dc_ht_binary_fill_rectangle);
private struct_proc_enum_ptrs(dc_ht_binary_enum_ptrs);
private struct_proc_reloc_ptrs(dc_ht_binary_reloc_ptrs);
const gx_device_color_procs
  gx_dc_procs_ht_binary =
    { gx_dc_ht_binary_load, gx_dc_ht_binary_fill_rectangle,
      dc_ht_binary_enum_ptrs, dc_ht_binary_reloc_ptrs
    };
#undef gx_dc_type_ht_binary
const gx_device_color_procs _ds *gx_dc_type_ht_binary = &gx_dc_procs_ht_binary;
#define gx_dc_type_ht_binary (&gx_dc_procs_ht_binary)
/* GC procedures */
#define cptr ((gx_device_color *)vptr)
private ENUM_PTRS_BEGIN(dc_ht_binary_enum_ptrs) return 0;
	ENUM_PTR(0, gx_device_color, colors.binary.b_ht);
	case 1:
	{	gx_ht_tile *tile = cptr->colors.binary.b_tile;
		ENUM_RETURN(tile - tile->index);
	}
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(dc_ht_binary_reloc_ptrs) {
	uint index = cptr->colors.binary.b_tile->index;
	RELOC_PTR(gx_device_color, colors.binary.b_ht);
	RELOC_TYPED_OFFSET_PTR(gx_device_color, colors.binary.b_tile, index);
} RELOC_PTRS_END
#undef cptr

/* Other GC procedures */
private_st_ht_tiles();
private ENUM_PTRS_BEGIN_PROC(ht_tiles_enum_ptrs) {
	return 0;
} ENUM_PTRS_END_PROC
private RELOC_PTRS_BEGIN(ht_tiles_reloc_ptrs) {
	/* Reset the bitmap pointers in the tiles. */
	/* We know the first tile points to the base of the bits. */
	gx_ht_tile *ht_tiles = vptr;
	byte *bits = ht_tiles->tiles.data;
	uint diff;

	if ( bits == 0 )
	  return;
	bits = gs_reloc_struct_ptr(bits, gcst);
	if ( size == size_of(gx_ht_tile) )	/* only 1 tile */
	  {	ht_tiles->tiles.data = bits;
		return;
	  }
	diff = ht_tiles[1].tiles.data - ht_tiles[0].tiles.data;
	for ( ; size; ht_tiles++, size -= size_of(gx_ht_tile), bits += diff )
	  {	ht_tiles->tiles.data = bits;
	  }
} RELOC_PTRS_END
private_st_ht_cache();

/* Return the default sizes of the halftone cache. */
uint
gx_ht_cache_default_tiles(void)
{
#if arch_small_memory
	return max_cached_tiles_SMALL;
#else
	return (gs_if_debug_c('.') ? max_cached_tiles_SMALL :
		max_cached_tiles_LARGE);
#endif
}
uint
gx_ht_cache_default_bits(void)
{
#if arch_small_memory
	return max_ht_bits_SMALL;
#else
	return (gs_if_debug_c('.') ? max_ht_bits_SMALL :
		max_ht_bits_LARGE);
#endif
}

/* Allocate a halftone cache. */
gx_ht_cache *
gx_ht_alloc_cache(gs_memory_t *mem, uint max_tiles, uint max_bits)
{	gx_ht_cache *pcache =
	  gs_alloc_struct(mem, gx_ht_cache, &st_ht_cache,
			  "alloc_ht_cache(struct)");
	byte *tbits =
	  gs_alloc_bytes(mem, max_bits, "alloc_ht_cache(bits)");
	gx_ht_tile *ht_tiles =
	  gs_alloc_struct_array(mem, max_tiles, gx_ht_tile, &st_ht_tiles,
				"alloc_ht_cache(ht_tiles)");

	if ( pcache == 0 || tbits == 0 || ht_tiles == 0 )
	{	gs_free_object(mem, ht_tiles, "alloc_ht_cache(ht_tiles)");
		gs_free_object(mem, tbits, "alloc_ht_cache(bits)");
		gs_free_object(mem, pcache, "alloc_ht_cache(struct)");
		return 0;
	}
	pcache->bits = tbits;
	pcache->bits_size = max_bits;
	pcache->ht_tiles = ht_tiles;
	pcache->num_tiles = max_tiles;
	pcache->order.cache = pcache;
	pcache->order.transfer = 0;
	gx_ht_clear_cache(pcache);
	return pcache;
}

/* Make the cache order current, and return whether */
/* there is room for all possible tiles in the cache. */
bool
gx_check_tile_cache(gs_state *pgs)
{	const gx_ht_order *porder = &pgs->dev_ht->order;
	gx_ht_cache *pcache = pgs->ht_cache;

	if ( pcache->order.bits != porder->bits )
	  gx_ht_init_cache(pcache, porder);
	return pcache->levels_per_tile == 1;
}

/*
 * Determine whether a given (width, y, height) might fit into a single
 * (non-strip) tile. If so, return the byte offset of the appropriate row
 * from the beginning of the tile, and set *ppx to the x phase offset
 * within the tile; if not, return -1.
 */
int
gx_check_tile_size(gs_state *pgs, int w, int y, int h, int *ppx)
{	int tsy;
	const gx_strip_bitmap *ptile0 =
	  &pgs->ht_cache->ht_tiles[0].tiles;	/* a typical tile */
#define tile0 (*ptile0)

	if ( h > tile0.rep_height || w > tile0.rep_width ||
	     tile0.shift != 0
	   )
	  return -1;
	tsy = (y + imod(-pgs->ht_phase.y, tile0.rep_height)) %
	  tile0.rep_height;
	if ( tsy + h > tile0.size.y )
	  return -1;
	/* Tile fits in Y, might fit in X. */
	*ppx = imod(-pgs->ht_phase.x, tile0.rep_width);
	return tsy * tile0.raster;
#undef tile0
}

/* Render a given level into a halftone cache. */
private int render_ht(P4(gx_ht_tile *, int, const gx_ht_order *,
			 gx_bitmap_id));
gx_ht_tile *
gx_render_ht(gx_ht_cache *pcache, int b_level)
{	const gx_ht_order *porder = &pcache->order;
	int level = porder->levels[b_level];
	gx_ht_tile *bt = &pcache->ht_tiles[level / pcache->levels_per_tile];

	if ( bt->level != level )
	  { int code = render_ht(bt, level, porder, pcache->base_id + b_level);
	    if ( code < 0 )
	      return 0;
	  }
	return bt;
}

/* Load the device color into the halftone cache if needed. */
private int
gx_dc_ht_binary_load(gx_device_color *pdevc, const gs_state *pgs)
{	const gx_ht_order *porder = &pgs->dev_ht->order;
	gx_ht_cache *pcache = pgs->ht_cache;

	if ( pcache->order.bits != porder->bits )
	  gx_ht_init_cache(pcache, porder);
	/* Expand gx_render_ht inline for speed. */
	{ int b_level = pdevc->colors.binary.b_level;
	  int level = porder->levels[b_level];
	  gx_ht_tile *bt = &pcache->ht_tiles[level / pcache->levels_per_tile];

	  if ( bt->level != level )
	    { int code = render_ht(bt, level, porder,
				   pcache->base_id + b_level);
	      if ( code < 0 )
		return_error(gs_error_Fatal);
	    }
	  pdevc->colors.binary.b_tile = bt;
	}
	return 0;
}

/* Fill a rectangle with a binary halftone. */
/* Note that we treat this as "texture" for RasterOp. */
private int
gx_dc_ht_binary_fill_rectangle(const gx_device_color *pdevc, int x, int y,
  int w, int h, gx_device *dev, gs_logical_operation_t lop,
  const gx_rop_source_t *source)
{	if ( source == NULL && lop_no_S_is_T(lop) )
	  return (*dev_proc(dev, strip_tile_rectangle))(dev,
				&pdevc->colors.binary.b_tile->tiles,
				x, y, w, h, pdevc->colors.binary.color[0],
				pdevc->colors.binary.color[1],
				pdevc->phase.x, pdevc->phase.y);
	/* Adjust the logical operation per transparent colors. */
	if ( pdevc->colors.binary.color[0] == gx_no_color_index )
	  lop = rop3_use_D_when_T_0(lop);
	if ( pdevc->colors.binary.color[1] == gx_no_color_index )
	  lop = rop3_use_D_when_T_1(lop);
	if ( source == NULL )
	  source = &gx_rop_no_source;
	return (*dev_proc(dev, strip_copy_rop))(dev, source->sdata,
				source->sourcex, source->sraster, source->id,
				(source->use_scolors ? source->scolors : NULL),
				&pdevc->colors.binary.b_tile->tiles,
				pdevc->colors.binary.color,
				x, y, w, h, pdevc->phase.x, pdevc->phase.y,
				lop);
}

/* Initialize the tile cache for a given screen. */
/* Cache as many different levels as will fit. */
void
gx_ht_init_cache(gx_ht_cache *pcache, const gx_ht_order *porder)
{	uint width = porder->width;
	uint height = porder->height;
	uint size = width * height + 1;
	int width_unit =
	  (width <= ht_mask_bits / 2 ? ht_mask_bits / width * width :
	   width);
	int height_unit = height;
	uint raster = porder->raster;
	uint tile_bytes = raster * height;
	uint shift = porder->shift;
	int num_cached;
	int i;
	byte *tbits = pcache->bits;

	/* Make sure num_cached is within bounds */
	num_cached = pcache->bits_size / tile_bytes;
	if ( num_cached > size )
	  num_cached = size;
	if ( num_cached > pcache->num_tiles )
	  num_cached = pcache->num_tiles;
	if ( num_cached == size &&
	     tile_bytes * num_cached <= pcache->bits_size / 2
	   )
	  {	/*
		 * We can afford to replicate every tile in the cache,
		 * which will reduce breakage when tiling.  Since
		 * horizontal breakage is more expensive than vertical,
		 * and since wide shallow fills are more common than
		 * narrow deep fills, we replicate the tile horizontally.
		 * We do have to be careful not to replicate the tile
		 * to an absurdly large size, however.
		 */
		uint rep_raster =
		  ((pcache->bits_size / num_cached) / height) &
		    ~(align_bitmap_mod - 1);
		uint rep_count = rep_raster * 8 / width;
		/*
		 * There's no real value in replicating the tile
		 * beyond the point where the byte width of the replicated
		 * tile is a multiple of a long.
		 */
		if ( rep_count > sizeof(ulong) * 8 )
		  rep_count = sizeof(ulong) * 8;
		width_unit = width * rep_count;
		raster = bitmap_raster(width_unit);
		tile_bytes = raster * height;
	  }
	pcache->base_id = gs_next_ids(porder->num_levels + 1);
	pcache->order = *porder;
	pcache->num_cached = num_cached;
	pcache->levels_per_tile = (size + num_cached - 1) / num_cached;
	memset(tbits, 0, pcache->bits_size);
	for ( i = 0; i < num_cached; i++, tbits += tile_bytes )
	{	register gx_ht_tile *bt = &pcache->ht_tiles[i];
		bt->level = 0;
		bt->index = i;
		bt->tiles.data = tbits;
		bt->tiles.raster = raster;
		bt->tiles.size.x = width_unit;
		bt->tiles.size.y = height_unit;
		bt->tiles.rep_width = width;
		bt->tiles.rep_height = height;
		bt->tiles.shift = bt->tiles.rep_shift = shift;
	}
}

/*
 * Compute and save the rendering of a given gray level
 * with the current halftone.  The cache holds multiple tiles,
 * where each tile covers a range of possible levels.
 * We adjust the tile whose range includes the desired level incrementally;
 * this saves a lot of time for the average image, where gray levels
 * don't change abruptly.  Note that the "level" is the number of bits,
 * not the index in the levels vector.
 */
private int
render_ht(gx_ht_tile *pbt, int level /* [1..num_bits-1] */,
  const gx_ht_order *porder, gx_bitmap_id new_id)
{	int old_level = pbt->level;
	register gx_ht_bit *p = &porder->bits[old_level];
	register byte *data = pbt->tiles.data;

	if_debug7('H', "[H]Halftone cache slot 0x%lx: old=%d, new=%d, w=%d(%d), h=%d(%d):\n",
		  (ulong)data, old_level, level,
		  pbt->tiles.size.x, porder->width,
		  pbt->tiles.size.y, porder->num_bits / porder->width);
#ifdef DEBUG
	if ( level < 0 || level > porder->num_bits )
	{	lprintf3("Error in render_ht: level=%d, old_level=%d, num_bits=%d\n", level, old_level, porder->num_bits);
		return_error(gs_error_Fatal);
	}
#endif
	/* Invert bits between the two pointers. */
	/* Note that we can use the same loop to turn bits either */
	/* on or off, using xor. */
	/* The Borland compiler generates truly dreadful code */
	/* if we don't assign the offset to a temporary. */
#if arch_ints_are_short
#  define invert_data(i)\
     { uint off = p[i].offset; *(ht_mask_t *)&data[off] ^= p[i].mask; }
#else
#  define invert_data(i) *(ht_mask_t *)&data[p[i].offset] ^= p[i].mask
#endif
#ifdef DEBUG
#  define invert(i)\
     { if_debug3('H', "[H]invert level=%d offset=%u mask=0x%x\n",\
	         (int)(p + i - porder->bits), p[i].offset, p[i].mask);\
       invert_data(i);\
     }
#else
#  define invert(i) invert_data(i)
#endif
sw:	switch ( level - old_level )
	{
	default:
		if ( level > old_level )
		{	invert(0); invert(1); invert(2); invert(3);
			p += 4; old_level += 4;
		}
		else
		{	invert(-1); invert(-2); invert(-3); invert(-4);
			p -= 4; old_level -= 4;
		}
		goto sw;
	case 7: invert(6);
	case 6: invert(5);
	case 5: invert(4);
	case 4: invert(3);
	case 3: invert(2);
	case 2: invert(1);
	case 1: invert(0);
	case 0: break;			/* Shouldn't happen! */
	case -7: invert(-7);
	case -6: invert(-6);
	case -5: invert(-5);
	case -4: invert(-4);
	case -3: invert(-3);
	case -2: invert(-2);
	case -1: invert(-1);
	}
#undef invert
	pbt->level = level;
	pbt->tiles.id = new_id;
	/*
	 * Check whether we want to replicate the tile in the cache.
	 * Since we only do this when all the renderings will fit
	 * in the cache, we only do it once per level, and it doesn't
	 * have to be very efficient.
	 */
	/****** TEST IS WRONG if width > rep_width but tile.raster ==
	 ****** order raster.
	 ******/
	if ( pbt->tiles.raster > porder->raster )
	  bits_replicate_horizontally(data, pbt->tiles.rep_width,
				      pbt->tiles.rep_height, porder->raster,
				      pbt->tiles.size.x, pbt->tiles.raster);
	if ( pbt->tiles.size.y > pbt->tiles.rep_height &&
	     pbt->tiles.shift == 0
	   )
	  bits_replicate_vertically(data, pbt->tiles.rep_height,
				    pbt->tiles.raster, pbt->tiles.size.y);
#ifdef DEBUG
if ( gs_debug_c('H') )
	{	const byte *p = pbt->tiles.data;
		int wb = pbt->tiles.raster;
		const byte *ptr = p + wb * pbt->tiles.size.y;

		while ( p < ptr )
		{	dprintf8(" %d%d%d%d%d%d%d%d",
				 *p >> 7, (*p >> 6) & 1, (*p >> 5) & 1,
				 (*p >> 4) & 1, (*p >> 3) & 1, (*p >> 2) & 1,
				 (*p >> 1) & 1, *p & 1);
			if ( (++p - data) % wb == 0 ) dputc('\n');
		}
	}
#endif
	return 0;
}
