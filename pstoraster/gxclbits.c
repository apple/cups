/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gxclbits.c */
/* Halftone and bitmap writing for command lists */
#include "memory_.h"
#include "gx.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gsbitops.h"
#include "gxdevice.h"
#include "gxdevmem.h"			/* must precede gxcldev.h */
#include "gxcldev.h"

/*
 * Define when, if ever, to write character bitmaps in all bands.
 * Set this to:
 *	0 to always write in all bands;
 *	N to write in all bands when the character has been seen in N+1
 *	   bands on a page;
 *	max_ushort to never write in all bands.
 */
#define CHAR_ALL_BANDS_COUNT max_ushort

/* ------ Writing ------ */

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
cmd_put_bits(gx_device_clist_writer *cldev, gx_clist_state *pcls,
  const byte *data, uint width_bits, uint height, uint raster, int op_size,
  int compression_mask, byte **pdp, uint *psize)
{	uint full_raster = bitmap_raster(width_bits);
	uint short_raster = (width_bits + 7) >> 3;
	uint full_size =
	  (height <= 1 ? short_raster * height :
	   full_raster * (height - 1) + short_raster);
	uint uncompressed_size =
	  (height <= 1 ? full_size : full_raster * height);
	uint short_size =
	  (short_raster <= cmd_max_short_width_bytes ?
	   short_raster * height : full_size);
	byte *dp;
	int compress = 0;

	/* See if compressing the bits is possible and worthwhile. */
	/* Currently we can't compress if the compressed data */
	/* won't fit in the command reading buffer, or if the */
	/* decompressed data won't fit in the buffer and */
	/* decompress_elsewhere isn't set. */
	if ( short_size >= 50 &&
	     (compression_mask & ~decompress_elsewhere) != 0 &&
	     (uncompressed_size <= cbuf_size ||
	      (compression_mask & decompress_elsewhere) != 0)
	   )
	  {	stream_cursor_read r;
		byte *wbase;
		stream_cursor_write w;
		int status;
		uint wcount;

		*psize = op_size + uncompressed_size;
		if ( pcls != 0 )
		  { set_cmd_put_op(dp, cldev, pcls, 0, *psize);
		  }
		else
		  { set_cmd_put_all_op(dp, cldev, 0, *psize);
		  }
		cmd_uncount_op(0, *psize);
		wbase = dp + (op_size - 1);
		r.ptr = data - 1;
		r.limit = r.ptr + uncompressed_size;
		w.ptr = wbase;
		w.limit = w.ptr + uncompressed_size;
		if ( compression_mask & (1 << cmd_compress_cfe) )
		  { /* Try CCITTFax compression. */
		    stream_CFE_state sstate;
		    clist_cfe_init(&sstate, width_bits);
		    status =
		      (*s_CFE_template.process)
			((stream_state *)&sstate, &r, &w, true);
		    (*s_CFE_template.release)((stream_state *)&sstate);
		    compress = cmd_compress_cfe;
		  }
		else if ( compression_mask & (1 << cmd_compress_rle) )
		  { /* Try RLE compression. */
		    stream_RLE_state sstate;
		    clist_rle_init(&sstate);
		    status =
		      (*s_RLE_template.process)
			((stream_state *)&sstate, &r, &w, true);
		    compress = cmd_compress_rle;
		  }
		if ( compress != 0 && status == 0 &&
		     (wcount = w.ptr - wbase) <= short_size >> 1 &&
		     wcount <= cbuf_size
		   )
		  {	/* Use compressed representation. */
			cmd_shorten_list_op(cldev,
				(pcls ? &pcls->list : &cldev->all_band_list),
				uncompressed_size - wcount);
			*psize = op_size + wcount;
			goto out;
		  }
		if ( uncompressed_size > cbuf_size )
		  { cmd_shorten_list_op(cldev,
				(pcls ? &pcls->list : &cldev->all_band_list),
				*psize);
		    return gs_error_limitcheck;
		  }
		if ( uncompressed_size != short_size )
		  {	cmd_shorten_list_op(cldev,
				(pcls ? &pcls->list : &cldev->all_band_list),
				uncompressed_size - short_size);
			*psize = op_size + short_size;
		  }
		compress = 0;
	  }
	else if ( full_size > cbuf_size )
	  return gs_error_limitcheck;
	else
	  {	*psize = op_size + short_size;
		if ( pcls != 0 )
		  { set_cmd_put_op(dp, cldev, pcls, 0, *psize);
		  }
		else
		  { set_cmd_put_all_op(dp, cldev, 0, *psize);
		  }
		cmd_uncount_op(0, *psize);
	  }
	if ( short_raster > cmd_max_short_width_bytes &&
	     !(compression_mask & decompress_spread)
	   )
	  short_raster = full_raster;		/* = short_size / height */
	bytes_copy_rectangle(dp + op_size, short_raster, data, raster,
			     short_raster, height);
out:	*pdp = dp;
	return compress;
}

/* Add a command to set the tile size and depth. */
private int
cmd_put_tile_params(gx_device_clist_writer *cldev, const gx_strip_bitmap *tile,
  int depth)
{	int tcsize = 2 + cmd_size_w(tile->size.x) + cmd_size_w(tile->size.y) +
	  cmd_size_w(tile->rep_width) + cmd_size_w(tile->rep_height) +
	    cmd_size_w(tile->rep_shift);
	byte *dp;

	set_cmd_put_all_op(dp, cldev, cmd_opv_set_tile_size, tcsize);
	dp[1] = depth;
	dp += 2;
	dp = cmd_put_w(tile->size.x, dp);
	dp = cmd_put_w(tile->size.y, dp);
	dp = cmd_put_w(tile->rep_width, dp);
	dp = cmd_put_w(tile->rep_height, dp);
	cmd_put_w(tile->rep_shift, dp);
	return 0;
}

/* Add a command to set the tile index. */
/* This is a relatively high-frequency operation, so we make it a macro. */
/* Note that it may do a 'return' with an error code. */
#define cmd_put_tile_index_inline(cldev, pcls, index)\
 { int idelta = (index) - (pcls)->tile_index + 8; byte *dp;\
   if ( !(idelta & ~15) )\
     { set_cmd_put_op(dp, cldev, pcls, cmd_op_delta_tile_index + idelta, 1); }\
   else\
     { set_cmd_put_op(dp, cldev, pcls, cmd_op_set_tile_index + ((index) >> 8), 2);\
       dp[1] = (index) & 0xff;\
       if_debug2('L', "[L]writing index=%u, offset=%lu\n",\
		 index, cldev->tile_table[index].offset);\
     }\
 }

/* Add commands to represent a halftone order. */
/****** NOT USED YET ******/
int
cmd_put_ht_order(gx_device_clist_writer *cldev, const gx_ht_order *porder)
{	long offset;
	byte command[max(cmd_largest_size - 1, 100)];
	byte *cp;
	uint len;
	byte *dp;
	uint i, n;

	/* Put out the order parameters. */
	/****** SET offset ******/
	offset = 0; /****** bogus, to pacify compilers ******/
	cp = cmd_put_w(offset, command);	/****** WRONG ******/
	cp = cmd_put_w(porder->width, cp);
	cp = cmd_put_w(porder->height, cp);
	cp = cmd_put_w(porder->raster, cp);
	cp = cmd_put_w(porder->shift, cp);
	cp = cmd_put_w(porder->num_levels, cp);
	cp = cmd_put_w(porder->num_bits, cp);
	len = cp - command;
	set_cmd_put_all_op(dp, cldev, cmd_opv_set_ht_size, len + 1);
	memcpy(dp + 1, command, len);

	/* Put out the levels array. */
#define nlevels ((sizeof(command) - 2) / sizeof(*porder->levels))
	for ( i = 0; i < porder->num_levels; i += n )
	  { n = porder->num_levels - i;
	    if ( n > nlevels )
	      n = nlevels;
	    set_cmd_put_all_op(dp, cldev, cmd_opv_set_ht_data,
			       2 + n * sizeof(*porder->levels));
	    dp[1] = n;
	    memcpy(dp + 2, porder->levels + i, n * sizeof(*porder->levels));
	  }
#undef nlevels

	/* Put out the bits array. */
#define nbits ((sizeof(command) - 2) / sizeof(*porder->bits))
	for ( i = 0; i < porder->num_bits; i += n )
	  { n = porder->num_bits - i;
	    if ( n > nbits )
	      n = nbits;
	    set_cmd_put_all_op(dp, cldev, cmd_opv_set_ht_data,
			       2 + n * sizeof(*porder->bits));
	    dp[1] = n;
	    memcpy(dp + 2, porder->bits + i, n * sizeof(*porder->bits));
	  }
#undef nbits

	return 0;
}

/* ------ Tile cache management ------ */

/* We want consecutive ids to map to consecutive hash slots if possible, */
/* so we can use a delta representation when setting the index. */
#define tile_id_hash(id) (id)
#define tile_hash_next(index) ((index) + 413)	/* arbitrary large odd # */
typedef struct tile_loc_s {
	uint index;
	tile_slot *tile;
} tile_loc;

/* Look up a tile or character in the cache.  If found, set the index and */
/* pointer; if not, set the index to the insertion point. */
private bool
clist_find_bits(gx_device_clist_writer *cldev, gx_bitmap_id id, tile_loc *ploc)
{	uint index = tile_id_hash(id);
	const tile_hash *table = cldev->tile_table;
	uint mask = cldev->tile_hash_mask;
	ulong offset;

	for ( ; (offset = table[index &= mask].offset) != 0;
	      index = tile_hash_next(index)
	    )
	  { tile_slot *tile = (tile_slot *)(cldev->data + offset);
	    if ( tile->id == id )
	      { ploc->index = index;
		ploc->tile = tile;
		return true;
	      }
	  }
	ploc->index = index;
	return false;
}

/* Delete a tile from the cache. */
private void
clist_delete_tile(gx_device_clist_writer *cldev, tile_slot *slot)
{	tile_hash *table = cldev->tile_table;
	uint mask = cldev->tile_hash_mask;
	uint index = slot->index;
	ulong offset;

	if_debug2('L', "[L]deleting index=%u, offset=%lu\n",
		  index, (ulong)((byte *)slot - cldev->data));
	gx_bits_cache_free(&cldev->bits, (gx_cached_bits_head *)slot,
			   &cldev->chunk);
	table[index].offset = 0;
	/* Delete the entry from the hash table. */
	/* We'd like to move up any later entries, so that we don't need */
	/* a deleted mark, but it's too difficult to note this in the */
	/* band list, so instead, we just delete any entries that */
	/* would need to be moved. */
	while ( (offset = table[index = tile_hash_next(index) & mask].offset) != 0 )
	  { tile_slot *tile = (tile_slot *)(cldev->data + offset);
	    tile_loc loc;
	    if ( !clist_find_bits(cldev, tile->id, &loc) )
	      { /* We didn't find it, so it should be moved into a slot */
		/* that we just vacated; instead, delete it. */
		if_debug2('L', "[L]move-deleting index=%u, offset=%lu\n",
			  index, offset);
		gx_bits_cache_free(&cldev->bits,
				(gx_cached_bits_head *)(cldev->data + offset),
				   &cldev->chunk);
		table[index].offset = 0;
	      }
	  }
}

/* Add a tile to the cache. */
/* tile->raster holds the raster for the replicated tile; */
/* we pass the raster of the actual data separately. */
private int
clist_add_tile(gx_device_clist_writer *cldev, const gx_strip_bitmap *tiles,
  uint sraster, int depth)
{	uint raster = tiles->raster;
	uint size_bytes = raster * tiles->size.y;
	uint tsize =
	  sizeof(tile_slot) + cldev->tile_band_mask_size + size_bytes;
	gx_cached_bits_head *slot_head;
#define slot ((tile_slot *)slot_head)

	if ( cldev->bits.csize == cldev->tile_max_count )
	  { /* Don't let the hash table get too full: delete an entry. */
	    /* Since gx_bits_cache_alloc returns an entry to delete when */
	    /* it fails, just force it to fail. */
	    gx_bits_cache_alloc(&cldev->bits, (ulong)cldev->chunk.size,
				&slot_head);
	    if ( slot_head == 0 )
	      { /* Wrap around and retry. */
		cldev->bits.cnext = 0;
		gx_bits_cache_alloc(&cldev->bits, (ulong)cldev->chunk.size,
				    &slot_head);
#ifdef DEBUG
		if ( slot_head == 0 )
		  { lprintf("No entry to delete!\n");
		    return_error(gs_error_Fatal);
		  }
#endif
	      }
	    clist_delete_tile(cldev, slot);
	  }
	/* Allocate the space for the new entry, deleting entries as needed. */
	while ( gx_bits_cache_alloc(&cldev->bits, (ulong)tsize, &slot_head) < 0 )
	  { if ( slot_head == 0 )
	      { /* Wrap around. */
		if ( cldev->bits.cnext == 0 )
		  { /* Too big to fit.  We should probably detect this */
		    /* sooner, since if we get here, we've cleared the */
		    /* cache. */
		    return_error(gs_error_limitcheck);
		  }
		cldev->bits.cnext = 0;
	      }
	    else
	      clist_delete_tile(cldev, slot);
	  }
	/* Fill in the entry. */
	slot->cb_depth = depth;
	slot->cb_raster = raster;
	slot->width = tiles->rep_width;
	slot->height = tiles->rep_height;
	slot->shift = tiles->rep_shift;
	slot->id = tiles->id;
	memset(ts_mask(slot), 0, cldev->tile_band_mask_size);
	bytes_copy_rectangle(ts_bits(cldev, slot), raster,
			     tiles->data, sraster,
			     (tiles->rep_width * depth + 7) >> 3,
			     tiles->rep_height);
	/* Make the hash table entry. */
	{ tile_loc loc;
#ifdef DEBUG
	  if ( clist_find_bits(cldev, tiles->id, &loc) )
	    lprintf1("clist_find_bits(0x%lx) should have failed!\n",
		     (ulong)tiles->id);
#else
	  clist_find_bits(cldev, tiles->id, &loc);	/* always fails */
#endif
	  slot->index = loc.index;
	  cldev->tile_table[loc.index].offset =
	    (byte *)slot_head - cldev->data;
	  if_debug2('L', "[L]adding index=%u, offset=%lu\n",
		    loc.index, cldev->tile_table[loc.index].offset);
	}
	slot->num_bands = 0;
	return 0;
}

/* ------ Driver procedure support ------ */

/* Change tile for clist_tile_rectangle. */
int
clist_change_tile(gx_device_clist_writer *cldev, gx_clist_state *pcls,
  const gx_strip_bitmap *tiles, int depth)
{	tile_loc loc;
	int code;

top:	if ( clist_find_bits(cldev, tiles->id, &loc) )
	  { /* The bitmap is in the cache.  Check whether this band */
	    /* knows about it. */
	    uint band_index = pcls - cldev->states;
	    byte *bptr = ts_mask(loc.tile) + (band_index >> 3);
	    byte bmask = 1 << (band_index & 7);

	    if ( *bptr & bmask )
	      { /* Already known.  Just set the index. */
		if ( pcls->tile_index == loc.index )
		  return 0;
		cmd_put_tile_index_inline(cldev, pcls, loc.index);
	      }
	    else
	      { /*
		 * Not known yet.  Output the bits.  Note that the offset we
		 * write is the one used by the reading phase, not the
		 * writing phase.  Note also that the size of the cached and
		 * written tile may differ from that of the client's tile.
		 * Finally, note that this tile's size parameters are
		 * guaranteed to be compatible with those stored in the
		 * device (cldev->tile_params).
		 */
		ulong offset = (byte *)loc.tile - cldev->chunk.data;
		uint rsize = 1 + cmd_size_w(loc.index) + cmd_size_w(offset);
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

		if ( code < 0 )
		  return code;
		*dp = cmd_count_op(cmd_opv_set_tile_bits, csize);
		dp++;
		dp = cmd_put_w(loc.index, dp);
		cmd_put_w(offset, dp);
		*bptr |= bmask;
		loc.tile->num_bands++;
	      }
	    pcls->tile_index = loc.index;
	    pcls->tile_id = loc.tile->id;
	    return 0;
	  }
	/* The tile is not in the cache. */
	/* Ensure that the tile size is compatible. */
	if ( tiles->rep_width != cldev->tile_params.rep_width ||
	     tiles->rep_height != cldev->tile_params.rep_height ||
	     tiles->rep_shift != cldev->tile_params.rep_shift ||
	     depth != cldev->tile_depth
	   )
	  { /* Change the tile dimensions. We do this for all bands at once. */
	    gx_strip_bitmap new_tile;
	    uint rep_width_bits = tiles->rep_width * depth;

	    /*
	     * Adjust the replication factors.  If we can, we replicate
	     * the tile in X up to 32 bytes, and then in Y up to 4 copies,
	     * as long as we don't exceed a total tile size of 256 bytes.
	     * However, don't attempt Y replication if shifting is required. 
	     */
#define max_tile_bytes_x 32
#define max_tile_reps_y 4
#define max_tile_bytes 256
	    new_tile = *tiles;
	    { uint max_bits_x = max_tile_bytes * 8 / new_tile.rep_height;
	      uint reps_x =
		min(max_bits_x, max_tile_bytes_x * 8) / rep_width_bits;
	      uint reps_y;

	      new_tile.size.x = max(reps_x, 1) * new_tile.rep_width;
	      new_tile.raster = bitmap_raster(new_tile.size.x * depth);
	      if ( tiles->shift != 0 )
		reps_y = 1;
	      else
		{ reps_y =
		    max_tile_bytes / (new_tile.raster * new_tile.rep_height);
		  if ( reps_y > max_tile_reps_y )
		    reps_y = max_tile_reps_y;
		  else if ( reps_y < 1 )
		    reps_y = 1;
		}
	      new_tile.size.y = reps_y * new_tile.rep_height;
	    }
#undef max_tile_bytes_x
#undef max_tile_reps_y
#undef max_tile_bytes
	    code = cmd_put_tile_params(cldev, &new_tile, depth);
	    if ( code < 0 )
	      return code;
	    cldev->tile_params = new_tile;
	    cldev->tile_depth = depth;
	  }
	else
	  { cldev->tile_params.id = tiles->id;
	    cldev->tile_params.data = tiles->data;
	  }

	code = clist_add_tile(cldev, &cldev->tile_params,
			      tiles->raster, depth);
	if ( code < 0 )
	  return code;
	goto top;
}

/* Change "tile" for clist_copy_*.  tiles->[rep_]shift must be zero. */
int
clist_change_bits(gx_device_clist_writer *cldev, gx_clist_state *pcls,
  const gx_strip_bitmap *tiles, int depth)
{	tile_loc loc;
	int code;

top:	if ( clist_find_bits(cldev, tiles->id, &loc) )
	  { /* The bitmap is in the cache.  Check whether this band */
	    /* knows about it. */
	    uint band_index = pcls - cldev->states;
	    byte *bptr = ts_mask(loc.tile) + (band_index >> 3);
	    byte bmask = 1 << (band_index & 7);

	    if ( *bptr & bmask )
	      { /* Already known.  Just set the index. */
		if ( pcls->tile_index == loc.index )
		  return 0;
		cmd_put_tile_index_inline(cldev, pcls, loc.index);
	      }
	    else
	      { /* Not known yet.  Output the bits. */
		/* Note that the offset we write is the one used by */
		/* the reading phase, not the writing phase. */
		ulong offset = (byte *)loc.tile - cldev->chunk.data;
		uint rsize = 2 + cmd_size_w(loc.tile->width) +
		  cmd_size_w(loc.tile->height) + cmd_size_w(loc.index) +
		    cmd_size_w(offset);
		byte *dp;
		uint csize;
		gx_clist_state *bit_pcls = pcls;
		int code;

		if ( loc.tile->num_bands == CHAR_ALL_BANDS_COUNT )
		  bit_pcls = NULL;
		code = cmd_put_bits(cldev, bit_pcls, ts_bits(cldev, loc.tile),
				    loc.tile->width * depth,
				    loc.tile->height, loc.tile->cb_raster,
				    rsize,
				    (1 << cmd_compress_cfe) | decompress_elsewhere,
				    &dp, &csize);

		if ( code < 0 )
		  return code;
		*dp = cmd_count_op(cmd_opv_set_bits, csize);
		dp[1] = (depth << 2) + code;
		dp += 2;
		dp = cmd_put_w(loc.tile->width, dp);
		dp = cmd_put_w(loc.tile->height, dp);
		dp = cmd_put_w(loc.index, dp);
		cmd_put_w(offset, dp);
		if ( bit_pcls == NULL )
		  { memset(ts_mask(loc.tile), 0xff,
			   cldev->tile_band_mask_size);
		    loc.tile->num_bands = cldev->nbands;
		  }
		else
		  { *bptr |= bmask;
		    loc.tile->num_bands++;
		  }
	      }
	    pcls->tile_index = loc.index;
	    pcls->tile_id = loc.tile->id;
	    return 0;
	  }
	/* The tile is not in the cache. */
	code = clist_add_tile(cldev, tiles, tiles->raster, depth);
	if ( code < 0 )
	  return code;
	goto top;
}
