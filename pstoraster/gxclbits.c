/* Copyright (C) 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxclbits.c,v 1.2 2000/03/08 23:14:51 mike Exp $ */
/* Halftone and bitmap writing for command lists */
#include "memory_.h"
#include "gx.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gsbitops.h"
#include "gxdevice.h"
#include "gxdevmem.h"		/* must precede gxcldev.h */
#include "gxcldev.h"
#include "gxfmap.h"

/*
 * Define when, if ever, to write character bitmaps in all bands.
 * Set this to:
 *      0 to always write in all bands;
 *      N to write in all bands when the character has been seen in N+1
 *         bands on a page;
 *      max_ushort to never write in all bands.
 */
#define CHAR_ALL_BANDS_COUNT max_ushort

/* ------ Writing ------ */

/*
 * Determine the (possibly unpadded) width in bytes for writing a bitmap,
 * per the algorithm in gxcldev.h.  If compression_mask has any of the
 * cmd_mask_compress_any bits set, we assume the bitmap will be compressed.
 * Return the total size of the bitmap.
 */
uint
clist_bitmap_bytes(uint width_bits, uint height, int compression_mask,
		   uint * width_bytes, uint * raster)
{
    uint full_raster = *raster = bitmap_raster(width_bits);
    uint short_raster = (width_bits + 7) >> 3;
    uint width_bytes_last;

    if (compression_mask & cmd_mask_compress_any)
	*width_bytes = width_bytes_last = full_raster;
    else if (short_raster <= cmd_max_short_width_bytes ||
	     height <= 1 ||
	     (compression_mask & decompress_spread) != 0
	)
	*width_bytes = width_bytes_last = short_raster;
    else
	*width_bytes = full_raster, width_bytes_last = short_raster;
    return
	(height == 0 ? 0 : *width_bytes * (height - 1) + width_bytes_last);
}

/*
 * Compress a bitmap, skipping extra padding bytes at the end of each row if
 * necessary.  We require height >= 1, raster >= bitmap_raster(width_bits).
 */
private int
cmd_compress_bitmap(stream_state * st, const byte * data, uint width_bits,
		    uint raster, uint height, stream_cursor_write * pw)
{
    uint width_bytes = bitmap_raster(width_bits);
    int status = 0;
    stream_cursor_read r;

    r.ptr = data - 1;
    if (raster == width_bytes) {
	r.limit = r.ptr + raster * height;
	status = (*st->template->process) (st, &r, pw, true);
    } else {			/* Compress row-by-row. */
	uint y;

	for (y = 1; (r.limit = r.ptr + width_bytes), y < height; ++y) {
	    status = (*st->template->process) (st, &r, pw, false);
	    if (status)
		break;
	    if (r.ptr != r.limit) {	/* We don't attempt to handle compressors that */
		/* require >1 input byte to make progress. */
		status = -1;
		break;
	    }
	    r.ptr += raster - width_bytes;
	}
	if (status == 0)
	    status = (*st->template->process) (st, &r, pw, true);
    }
    if (st->template->release)
	(*st->template->release) (st);
    return status;
}

/*
 * Put a bitmap in the buffer, compressing if appropriate.
 * pcls == 0 means put the bitmap in all bands.
 * Return <0 if error, otherwise the compression method.
 * A return value of gs_error_limitcheck means that the bitmap was too big
 * to fit in the command reading buffer.
 * Note that this leaves room for the command and initial arguments,
 * but doesn't fill them in.
 */
int
cmd_put_bits(gx_device_clist_writer * cldev, gx_clist_state * pcls,
  const byte * data, uint width_bits, uint height, uint raster, int op_size,
	     int compression_mask, byte ** pdp, uint * psize)
{
    uint short_raster, full_raster;
    uint short_size =
    clist_bitmap_bytes(width_bits, height,
		       compression_mask & ~cmd_mask_compress_any,
		       &short_raster, &full_raster);
    uint uncompressed_raster;
    uint uncompressed_size =
    clist_bitmap_bytes(width_bits, height, compression_mask,
		       &uncompressed_raster, &full_raster);
    uint max_size = cbuf_size - op_size;
    gs_memory_t *mem = (cldev->memory ? cldev->memory : &gs_memory_default);
    byte *dp;
    int compress = 0;

    /*
     * See if compressing the bits is possible and worthwhile.
     * Currently we can't compress if the compressed data won't fit in
     * the command reading buffer, or if the decompressed data won't fit
     * in the buffer and decompress_elsewhere isn't set.
     */
    if (short_size >= 50 &&
	(compression_mask & cmd_mask_compress_any) != 0 &&
	(uncompressed_size <= max_size ||
	 (compression_mask & decompress_elsewhere) != 0)
	) {
	union ss_ {
	    stream_state ss;
	    stream_CFE_state cf;
	    stream_RLE_state rl;
	} sstate;
	int code;

	*psize = op_size + uncompressed_size;
	code = (pcls != 0 ?
		set_cmd_put_op(dp, cldev, pcls, 0, *psize) :
		set_cmd_put_all_op(dp, cldev, 0, *psize));
	if (code < 0)
	    return code;
	cmd_uncount_op(0, *psize);
	/*
	 * Note that we currently keep all the padding if we are
	 * compressing.  This is ridiculous, but it's too hard to
	 * change right now.
	 */
	if (compression_mask & (1 << cmd_compress_cfe)) {
	    /* Try CCITTFax compression. */
	    clist_cfe_init(&sstate.cf,
			   uncompressed_raster << 3 /*width_bits*/,
			   mem);
	    sstate.ss.template = &s_CFE_template;
	    compress = cmd_compress_cfe;
	} else if (compression_mask & (1 << cmd_compress_rle)) {
	    /* Try RLE compression. */
	    clist_rle_init(&sstate.rl);
	    sstate.ss.template = &s_RLE_template;
	    compress = cmd_compress_rle;
	}
	if (compress) {
	    byte *wbase = dp + (op_size - 1);
	    stream_cursor_write w;

	    /*
	     * We can give up on compressing if we generate too much
	     * output to fit in the command reading buffer, or too
	     * much to make compression worthwhile.
	     */
	    uint wmax = min(uncompressed_size, max_size);
	    int status;

	    w.ptr = wbase;
	    w.limit = w.ptr + min(wmax, short_size >> 1);
	    status = cmd_compress_bitmap((stream_state *) & sstate, data,
				  uncompressed_raster << 3 /*width_bits */ ,
					 raster, height, &w);
	    if (status == 0) {	/* Use compressed representation. */
		uint wcount = w.ptr - wbase;

		cmd_shorten_list_op(cldev,
			     (pcls ? &pcls->list : &cldev->band_range_list),
				    uncompressed_size - wcount);
		*psize = op_size + wcount;
		goto out;
	    }
	}
	if (uncompressed_size > max_size) {
	    cmd_shorten_list_op(cldev,
			     (pcls ? &pcls->list : &cldev->band_range_list),
				*psize);
	    return_error(gs_error_limitcheck);
	}
	if (uncompressed_size != short_size) {
	    cmd_shorten_list_op(cldev,
			     (pcls ? &pcls->list : &cldev->band_range_list),
				uncompressed_size - short_size);
	    *psize = op_size + short_size;
	}
	compress = 0;
    } else if (uncompressed_size > max_size)
	return_error(gs_error_limitcheck);
    else {
	int code;

	*psize = op_size + short_size;
	code = (pcls != 0 ?
		set_cmd_put_op(dp, cldev, pcls, 0, *psize) :
		set_cmd_put_all_op(dp, cldev, 0, *psize));
	if (code < 0)
	    return code;
	cmd_uncount_op(0, *psize);
    }
    bytes_copy_rectangle(dp + op_size, short_raster, data, raster,
			 short_raster, height);
out:
    *pdp = dp;
    return compress;
}

/* Add a command to set the tile size and depth. */
private uint
cmd_size_tile_params(const gx_strip_bitmap * tile)
{
    return 2 + cmd_size_w(tile->rep_width) + cmd_size_w(tile->rep_height) +
	(tile->rep_width == tile->size.x ? 0 :
	 cmd_size_w(tile->size.x / tile->rep_width)) +
	(tile->rep_height == tile->size.y ? 0 :
	 cmd_size_w(tile->size.y / tile->rep_height)) +
	(tile->rep_shift == 0 ? 0 : cmd_size_w(tile->rep_shift));
}
private void
cmd_store_tile_params(byte * dp, const gx_strip_bitmap * tile, int depth,
		      uint csize)
{
    byte *p = dp + 2;
    byte bd = depth - 1;

    *dp = cmd_count_op(cmd_opv_set_tile_size, csize);
    p = cmd_put_w(tile->rep_width, p);
    p = cmd_put_w(tile->rep_height, p);
    if (tile->rep_width != tile->size.x) {
	p = cmd_put_w(tile->size.x / tile->rep_width, p);
	bd |= 0x20;
    }
    if (tile->rep_height != tile->size.y) {
	p = cmd_put_w(tile->size.y / tile->rep_height, p);
	bd |= 0x40;
    }
    if (tile->rep_shift != 0) {
	cmd_put_w(tile->rep_shift, p);
	bd |= 0x80;
    }
    dp[1] = bd;
}

/* Add a command to set the tile index. */
/* This is a relatively high-frequency operation, so we declare it `inline'. */
inline private int
cmd_put_tile_index(gx_device_clist_writer *cldev, gx_clist_state *pcls,
		   uint indx)
{
    int idelta = indx - pcls->tile_index + 8;
    byte *dp;
    int code;

    if (!(idelta & ~15)) {
	code = set_cmd_put_op(dp, cldev, pcls,
			      cmd_op_delta_tile_index + idelta, 1);
	if (code < 0)
	    return code;
    } else {
	code = set_cmd_put_op(dp, cldev, pcls,
			      cmd_op_set_tile_index + (indx >> 8), 2);
	if (code < 0)
	    return code;
	dp[1] = indx & 0xff;
    }
    if_debug2('L', "[L]writing index=%u, offset=%lu\n",
	      indx, cldev->tile_table[indx].offset);
    return 0;
}

/* If necessary, write out data for a single color map. */
int
cmd_put_color_map(gx_device_clist_writer * cldev, cmd_map_index map_index,
		  const gx_transfer_map * map, gs_id * pid)
{
    byte *dp;
    int code;

    if (map == 0) {
	if (pid && *pid == gs_no_id)
	    return 0;	/* no need to write */
	code = set_cmd_put_all_op(dp, cldev, cmd_opv_set_misc, 2);
	if (code < 0)
	    return code;
	dp[1] = cmd_set_misc_map + map_index;
	if (pid)
	    *pid = gs_no_id;
    } else {
	if (pid && map->id == *pid)
	    return 0;	/* no need to write */
	code = set_cmd_put_all_op(dp, cldev, cmd_opv_set_misc,
				  2 + sizeof(map->values));
	if (code < 0)
	    return code;
	dp[1] = cmd_set_misc_map + 0x20 + map_index;
	memcpy(dp + 2, map->values, sizeof(map->values));
	if (pid)
	    *pid = map->id;
    }
    return 0;
}

/* ------ Tile cache management ------ */

/* We want consecutive ids to map to consecutive hash slots if possible, */
/* so we can use a delta representation when setting the index. */
/* NB that we cannot emit 'delta' style tile indices if VM error recovery */
/* is in effect, since reader & writer's tile indices may get out of phase */
/* as a consequence of error recovery occurring. */
#define tile_id_hash(id) (id)
#define tile_hash_next(index) ((index) + 413)	/* arbitrary large odd # */
typedef struct tile_loc_s {
    uint index;
    tile_slot *tile;
} tile_loc;

/* Look up a tile or character in the cache.  If found, set the index and */
/* pointer; if not, set the index to the insertion point. */
private bool
clist_find_bits(gx_device_clist_writer * cldev, gx_bitmap_id id, tile_loc * ploc)
{
    uint index = tile_id_hash(id);
    const tile_hash *table = cldev->tile_table;
    uint mask = cldev->tile_hash_mask;
    ulong offset;

    for (; (offset = table[index &= mask].offset) != 0;
	 index = tile_hash_next(index)
	) {
	tile_slot *tile = (tile_slot *) (cldev->data + offset);

	if (tile->id == id) {
	    ploc->index = index;
	    ploc->tile = tile;
	    return true;
	}
    }
    ploc->index = index;
    return false;
}

/* Delete a tile from the cache. */
private void
clist_delete_tile(gx_device_clist_writer * cldev, tile_slot * slot)
{
    tile_hash *table = cldev->tile_table;
    uint mask = cldev->tile_hash_mask;
    uint index = slot->index;
    ulong offset;

    if_debug2('L', "[L]deleting index=%u, offset=%lu\n",
	      index, (ulong) ((byte *) slot - cldev->data));
    gx_bits_cache_free(&cldev->bits, (gx_cached_bits_head *) slot,
		       &cldev->chunk);
    table[index].offset = 0;
    /* Delete the entry from the hash table. */
    /* We'd like to move up any later entries, so that we don't need */
    /* a deleted mark, but it's too difficult to note this in the */
    /* band list, so instead, we just delete any entries that */
    /* would need to be moved. */
    while ((offset = table[index = tile_hash_next(index) & mask].offset) != 0) {
	tile_slot *tile = (tile_slot *) (cldev->data + offset);
	tile_loc loc;

	if (!clist_find_bits(cldev, tile->id, &loc)) {	/* We didn't find it, so it should be moved into a slot */
	    /* that we just vacated; instead, delete it. */
	    if_debug2('L', "[L]move-deleting index=%u, offset=%lu\n",
		      index, offset);
	    gx_bits_cache_free(&cldev->bits,
			     (gx_cached_bits_head *) (cldev->data + offset),
			       &cldev->chunk);
	    table[index].offset = 0;
	}
    }
}

/* Add a tile to the cache. */
/* tile->raster holds the raster for the replicated tile; */
/* we pass the raster of the actual data separately. */
private int
clist_add_tile(gx_device_clist_writer * cldev, const gx_strip_bitmap * tiles,
	       uint sraster, int depth)
{
    uint raster = tiles->raster;
    uint size_bytes = raster * tiles->size.y;
    uint tsize =
    sizeof(tile_slot) + cldev->tile_band_mask_size + size_bytes;
    gx_cached_bits_head *slot_head;

#define slot ((tile_slot *)slot_head)

    if (cldev->bits.csize == cldev->tile_max_count) {	/* Don't let the hash table get too full: delete an entry. */
	/* Since gx_bits_cache_alloc returns an entry to delete when */
	/* it fails, just force it to fail. */
	gx_bits_cache_alloc(&cldev->bits, (ulong) cldev->chunk.size,
			    &slot_head);
	if (slot_head == 0) {	/* Wrap around and retry. */
	    cldev->bits.cnext = 0;
	    gx_bits_cache_alloc(&cldev->bits, (ulong) cldev->chunk.size,
				&slot_head);
#ifdef DEBUG
	    if (slot_head == 0) {
		lprintf("No entry to delete!\n");
		return_error(gs_error_Fatal);
	    }
#endif
	}
	clist_delete_tile(cldev, slot);
    }
    /* Allocate the space for the new entry, deleting entries as needed. */
    while (gx_bits_cache_alloc(&cldev->bits, (ulong) tsize, &slot_head) < 0) {
	if (slot_head == 0) {	/* Wrap around. */
	    if (cldev->bits.cnext == 0) {	/* Too big to fit.  We should probably detect this */
		/* sooner, since if we get here, we've cleared the */
		/* cache. */
		return_error(gs_error_limitcheck);
	    }
	    cldev->bits.cnext = 0;
	} else
	    clist_delete_tile(cldev, slot);
    }
    /* Fill in the entry. */
    slot->cb_depth = depth;
    slot->cb_raster = raster;
    slot->width = tiles->rep_width;
    slot->height = tiles->rep_height;
    slot->shift = slot->rep_shift = tiles->rep_shift;
    slot->x_reps = slot->y_reps = 1;
    slot->id = tiles->id;
    memset(ts_mask(slot), 0, cldev->tile_band_mask_size);
    bytes_copy_rectangle(ts_bits(cldev, slot), raster,
			 tiles->data, sraster,
			 (tiles->rep_width * depth + 7) >> 3,
			 tiles->rep_height);
    /* Make the hash table entry. */
    {
	tile_loc loc;

#ifdef DEBUG
	if (clist_find_bits(cldev, tiles->id, &loc))
	    lprintf1("clist_find_bits(0x%lx) should have failed!\n",
		     (ulong) tiles->id);
#else
	clist_find_bits(cldev, tiles->id, &loc);	/* always fails */
#endif
	slot->index = loc.index;
	cldev->tile_table[loc.index].offset =
	    (byte *) slot_head - cldev->data;
	if_debug2('L', "[L]adding index=%u, offset=%lu\n",
		  loc.index, cldev->tile_table[loc.index].offset);
    }
    slot->num_bands = 0;
    return 0;
}

/* ------ Driver procedure support ------ */

/* Change the tile parameters (size and depth). */
/* Currently we do this for all bands at once. */
private void
clist_new_tile_params(gx_strip_bitmap * new_tile, const gx_strip_bitmap * tiles,
		      int depth, const gx_device_clist_writer * cldev)
{				/*
				 * Adjust the replication factors.  If we can, we replicate
				 * the tile in X up to 32 bytes, and then in Y up to 4 copies,
				 * as long as we don't exceed a total tile size of 256 bytes,
				 * or more than 255 repetitions in X or Y, or make the tile so
				 * large that not all possible tiles will fit in the cache.
				 * Also, don't attempt Y replication if shifting is required. 
				 */
#define max_tile_reps_x 255
#define max_tile_bytes_x 32
#define max_tile_reps_y 4
#define max_tile_bytes 256
    uint rep_width = tiles->rep_width;
    uint rep_height = tiles->rep_height;
    uint rep_width_bits = rep_width * depth;
    uint tile_overhead =
    sizeof(tile_slot) + cldev->tile_band_mask_size;
    uint max_bytes = cldev->chunk.size / (rep_width_bits * rep_height);

    max_bytes -= min(max_bytes, tile_overhead);
    if (max_bytes > max_tile_bytes)
	max_bytes = max_tile_bytes;
    *new_tile = *tiles;
    {
	uint max_bits_x = max_bytes * 8 / rep_height;
	uint reps_x =
	min(max_bits_x, max_tile_bytes_x * 8) / rep_width_bits;
	uint reps_y;

	while (reps_x > max_tile_reps_x)
	    reps_x >>= 1;
	new_tile->size.x = max(reps_x, 1) * rep_width;
	new_tile->raster = bitmap_raster(new_tile->size.x * depth);
	if (tiles->shift != 0)
	    reps_y = 1;
	else {
	    reps_y = max_bytes / (new_tile->raster * rep_height);
	    if (reps_y > max_tile_reps_y)
		reps_y = max_tile_reps_y;
	    else if (reps_y < 1)
		reps_y = 1;
	}
	new_tile->size.y = reps_y * rep_height;
    }
#undef max_tile_reps_x
#undef max_tile_bytes_x
#undef max_tile_reps_y
#undef max_tile_bytes
}

/* Change tile for clist_tile_rectangle. */
int
clist_change_tile(gx_device_clist_writer * cldev, gx_clist_state * pcls,
		  const gx_strip_bitmap * tiles, int depth)
{
    tile_loc loc;
    int code;

#define tile_params_differ(cldev, tiles, depth)\
  ((tiles)->rep_width != (cldev)->tile_params.rep_width ||\
   (tiles)->rep_height != (cldev)->tile_params.rep_height ||\
   (tiles)->rep_shift != (cldev)->tile_params.rep_shift ||\
   (depth) != (cldev)->tile_depth)

  top:if (clist_find_bits(cldev, tiles->id, &loc)) {	/* The bitmap is in the cache.  Check whether this band */
	/* knows about it. */
	int band_index = pcls - cldev->states;
	byte *bptr = ts_mask(loc.tile) + (band_index >> 3);
	byte bmask = 1 << (band_index & 7);

	if (*bptr & bmask) {	/* Already known.  Just set the index. */
	    if (pcls->tile_index == loc.index)
		return 0;
	    cmd_put_tile_index(cldev, pcls, loc.index);
	} else {
	    uint extra = 0;

	    if tile_params_differ
		(cldev, tiles, depth) {		/*
						 * We have a cached tile whose parameters differ from
						 * the current ones.  Because of the way tile IDs are
						 * managed, this is currently only possible when mixing
						 * Patterns and halftones, but if we didn't generate new
						 * IDs each time the main halftone cache needed to be
						 * refreshed, this could also happen simply from
						 * switching screens.
						 */
		int band;

		clist_new_tile_params(&cldev->tile_params, tiles, depth,
				      cldev);
		cldev->tile_depth = depth;
		/* No band knows about the new parameters. */
		for (band = cldev->tile_known_min;
		     band <= cldev->tile_known_max;
		     ++band
		    )
		    cldev->states[band].known &= ~tile_params_known;
		cldev->tile_known_min = cldev->nbands;
		cldev->tile_known_max = -1;
		}
	    if (!(pcls->known & tile_params_known)) {	/* We're going to have to write the tile parameters. */
		extra = cmd_size_tile_params(&cldev->tile_params);
	    } {			/*
				 * This band doesn't know this tile yet, so output the
				 * bits.  Note that the offset we write is the one used by
				 * the reading phase, not the writing phase.  Note also
				 * that the size of the cached and written tile may differ
				 * from that of the client's tile.  Finally, note that
				 * this tile's size parameters are guaranteed to be
				 * compatible with those stored in the device
				 * (cldev->tile_params).
				 */
		ulong offset = (byte *) loc.tile - cldev->chunk.data;
		uint rsize =
		    extra + 1 + cmd_size_w(loc.index) + cmd_size_w(offset);
		byte *dp;
		uint csize;
		int code =
		cmd_put_bits(cldev, pcls, ts_bits(cldev, loc.tile),
			     tiles->rep_width * depth, tiles->rep_height,
			     loc.tile->cb_raster, rsize,
			     (cldev->tile_params.size.x > tiles->rep_width ?
			      decompress_elsewhere | decompress_spread :
			      decompress_elsewhere),
			     &dp, &csize);

		if (code < 0)
		    return code;
		if (extra) {	/* Write the tile parameters before writing the bits. */
		    cmd_store_tile_params(dp, &cldev->tile_params, depth,
					  extra);
		    dp += extra;
		    /* This band now knows the parameters. */
		    pcls->known |= tile_params_known;
		    if (band_index < cldev->tile_known_min)
			cldev->tile_known_min = band_index;
		    if (band_index > cldev->tile_known_max)
			cldev->tile_known_max = band_index;
		}
		*dp = cmd_count_op(cmd_opv_set_tile_bits, csize - extra);
		dp++;
		dp = cmd_put_w(loc.index, dp);
		cmd_put_w(offset, dp);
		*bptr |= bmask;
		loc.tile->num_bands++;
	    }
	}
	pcls->tile_index = loc.index;
	pcls->tile_id = loc.tile->id;
	return 0;
    }
    /* The tile is not in the cache, add it. */
    {
	gx_strip_bitmap new_tile;
	gx_strip_bitmap *ptile;

	/* Ensure that the tile size is compatible. */
	if (tile_params_differ(cldev, tiles, depth)) {	/* We'll reset cldev->tile_params when we write the bits. */
	    clist_new_tile_params(&new_tile, tiles, depth, cldev);
	    ptile = &new_tile;
	} else {
	    cldev->tile_params.id = tiles->id;
	    cldev->tile_params.data = tiles->data;
	    ptile = &cldev->tile_params;
	}
	code = clist_add_tile(cldev, ptile, tiles->raster, depth);
	if (code < 0)
	    return code;
    }
    goto top;
#undef tile_params_differ
}

/* Change "tile" for clist_copy_*.  tiles->[rep_]shift must be zero. */
int
clist_change_bits(gx_device_clist_writer * cldev, gx_clist_state * pcls,
		  const gx_strip_bitmap * tiles, int depth)
{
    tile_loc loc;
    int code;

  top:if (clist_find_bits(cldev, tiles->id, &loc)) {	/* The bitmap is in the cache.  Check whether this band */
	/* knows about it. */
	uint band_index = pcls - cldev->states;
	byte *bptr = ts_mask(loc.tile) + (band_index >> 3);
	byte bmask = 1 << (band_index & 7);

	if (*bptr & bmask) {	/* Already known.  Just set the index. */
	    if (pcls->tile_index == loc.index)
		return 0;
	    cmd_put_tile_index(cldev, pcls, loc.index);
	} else {		/* Not known yet.  Output the bits. */
	    /* Note that the offset we write is the one used by */
	    /* the reading phase, not the writing phase. */
	    ulong offset = (byte *) loc.tile - cldev->chunk.data;
	    uint rsize = 2 + cmd_size_w(loc.tile->width) +
	    cmd_size_w(loc.tile->height) + cmd_size_w(loc.index) +
	    cmd_size_w(offset);
	    byte *dp;
	    uint csize;
	    gx_clist_state *bit_pcls = pcls;
	    int code;

	    if (loc.tile->num_bands == CHAR_ALL_BANDS_COUNT)
		bit_pcls = NULL;
	    code = cmd_put_bits(cldev, bit_pcls, ts_bits(cldev, loc.tile),
				loc.tile->width * depth,
				loc.tile->height, loc.tile->cb_raster,
				rsize,
			     (1 << cmd_compress_cfe) | decompress_elsewhere,
				&dp, &csize);

	    if (code < 0)
		return code;
	    *dp = cmd_count_op(cmd_opv_set_bits, csize);
	    dp[1] = (depth << 2) + code;
	    dp += 2;
	    dp = cmd_put_w(loc.tile->width, dp);
	    dp = cmd_put_w(loc.tile->height, dp);
	    dp = cmd_put_w(loc.index, dp);
	    cmd_put_w(offset, dp);
	    if (bit_pcls == NULL) {
		memset(ts_mask(loc.tile), 0xff,
		       cldev->tile_band_mask_size);
		loc.tile->num_bands = cldev->nbands;
	    } else {
		*bptr |= bmask;
		loc.tile->num_bands++;
	    }
	}
	pcls->tile_index = loc.index;
	pcls->tile_id = loc.tile->id;
	return 0;
    }
    /* The tile is not in the cache. */
    code = clist_add_tile(cldev, tiles, tiles->raster, depth);
    if (code < 0)
	return code;
    goto top;
}
