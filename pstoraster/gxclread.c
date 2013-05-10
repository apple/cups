/*
  Copyright 1993-1999 by Easy Software Products.
  Copyright (C) 1991, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gxclread.c */
/* Command list reading for Ghostscript. */
#include "memory_.h"
#include "gx.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gsbitops.h"
#include "gscspace.h"
#include "gsdcolor.h"
#include "gxdevice.h"
#include "gsdevice.h"			/* for gs_deviceinitialmatrix */
#include "gxdevmem.h"			/* must precede gxcldev.h */
#include "gxcldev.h"
#include "gxclpath.h"
#include "gxpaint.h"			/* for gx_fill/stroke_params */
#include "gxhttile.h"
#include "gdevht.h"
#include "gzpath.h"
#include "gzcpath.h"
#include "gzacpath.h"
#include "stream.h"
#include "strimpl.h"

#define cdev crdev

/* Print a bitmap for tracing */
#ifdef DEBUG
private void
cmd_print_bits(const byte *data, int width, int height, int raster)
{	int i, j;
	dprintf3("[L]width=%d, height=%d, raster=%d\n",
		 width, height, raster);
	for ( i = 0; i < height; i++ )
	   {	const byte *row = data + i * raster;
		dprintf("[L]");
		for ( j = 0; j < raster; j++ )
		  dprintf1(" %02x", row[j]);
		dputc('\n');
	   }
}
#else
#  define cmd_print_bits(data, width, height, raster) DO_NOTHING
#endif

/* ------ Band file reading stream ------ */

/*
 * To separate banding per se from command list interpretation,
 * we make the command list interpreter simply read from a stream.
 * When we are actually doing banding, the stream filters the band file
 * and only passes through the commands for the current band (or all bands).
 */
typedef struct stream_band_read_state_s {
	stream_state_common;
	clist_file_ptr cfile;
	clist_file_ptr bfile;
	long bfile_end_pos;
	int band;
	uint left;		/* amount of data left in this run */
	cmd_block b_this;
} stream_band_read_state;

#define ss ((stream_band_read_state *)st)

private int
s_band_read_init(stream_state *st)
{	ss->left = 0;
	ss->b_this.band = 0;
	ss->b_this.pos = 0;
	clist_rewind(ss->bfile, false);
	return 0;
}

private int
s_band_read_process(stream_state *st, stream_cursor_read *ignore_pr,
  stream_cursor_write *pw, bool last)
{	register byte *q = pw->ptr;
	byte *wlimit = pw->limit;
	clist_file_ptr cfile = ss->cfile;
	clist_file_ptr bfile = ss->bfile;
	uint left = ss->left;
	int status = 1;
	uint count;

	while ( (count = wlimit - q) != 0 )
	  {	if ( left )
		  {	/* Read more data for the current run. */
			if ( count > left )
			  count = left;
			clist_fread_chars(q + 1, count, cfile);
			/****** CHECK FOR ferror ******/
			q += count;
			left -= count;
			process_interrupts();
			continue;
		  }
rb:		/* Scan for the next run for this band (or all bands). */
		if ( ss->b_this.band == cmd_band_end &&
		     clist_ftell(bfile) == ss->bfile_end_pos
		   )
		  { status = EOFC;
		    break;
		  }
		{ int band = ss->b_this.band;
		  long pos = ss->b_this.pos;
		  clist_fread_chars(&ss->b_this, sizeof(ss->b_this), bfile);
		  if ( !(band == ss->band || band == cmd_band_all) )
		    goto rb;
		  clist_fseek(cfile, pos, SEEK_SET);
		  left = (uint)(ss->b_this.pos - pos);
		}
	  }
	pw->ptr = q;
	ss->left = left;
	return status;
}

#undef ss

/* Stream template */
const stream_template s_band_read_template =
{	&st_stream_state, s_band_read_init, s_band_read_process, 1, cbuf_size
};


/* ------ Reading/rendering ------ */

private int clist_render_init(P2(gx_device_clist *, gx_device_ht *));
private int clist_render_band(P6(gx_device_clist_reader *, stream *, gx_device *, int, int, gs_memory_t *));

/* Copy a scan line to the client.  This is where rendering gets done. */
int
clist_get_bits(gx_device *dev, int y, byte *str, byte **actual_data)
{	gx_device *target = cdev->target;
	uint raster = gx_device_raster(target, 1);
	gx_device_memory mdev;
	gx_device_ht hdev;
	gx_device *tdev = (gx_device *)&mdev;

	/* Initialize for rendering if we haven't done so yet. */
	if ( cdev->ymin < 0 )
	{	int code = clist_render_init((gx_device_clist *)dev, &hdev);
		if ( code < 0 )
		  return code;
		if ( code != 0 )
		  { hdev.target = tdev;
		    tdev = (gx_device *)&hdev;
		  }
	}
	/* Render a band if necessary, and copy it incrementally. */
	if ( !(y >= cdev->ymin && y < cdev->ymax) )
	   {	int band = y / cdev->band_height;
		stream s;
		stream_band_read_state rs;
		byte sbuf[cbuf_size];
		int code;
		private const stream_procs no_procs =
		  {	s_std_noavailable, s_std_noseek, s_std_read_reset,
			s_std_read_flush, s_std_close, s_band_read_process
		  };

		if ( y < 0 || y > dev->height )
		  return_error(gs_error_rangecheck);
		code = (*cdev->make_buffer_device)
		  (&mdev, target, 0, true);
		if ( code < 0 )
		  return code;
		mdev.base = cdev->mdata;
		rs.template = &s_band_read_template;
		rs.memory = 0;
		rs.cfile = cdev->cfile;
		rs.bfile = cdev->bfile;
		rs.bfile_end_pos = cdev->bfile_end_pos;
		rs.band = band;
		s_band_read_init((stream_state *)&rs);
		s_std_init(&s, sbuf, cbuf_size, &no_procs, s_mode_read);
		s.foreign = 1;
		s.state = (stream_state *)&rs;
		/*
		 * The matrix in the memory device is irrelevant,
		 * because all we do with the device is call the device-level
		 * output procedures, but we may as well set it to
		 * something halfway reasonable.
		 */
		gs_deviceinitialmatrix(target, &mdev.initial_matrix);
		mdev.width = target->width;
		mdev.height = cdev->band_height;
		mdev.raster = raster;
		(*dev_proc(&mdev, open_device))((gx_device *)&mdev);
		/* We have to pick some allocator for rendering.... */
		if_debug1('l', "[l]rendering band %d\n", band);
		code = clist_render_band(cdev, &s, tdev, 0, band * mdev.height,
					 (cdev->memory != 0 ? cdev->memory :
					  &gs_memory_default));
		/* Reset the band boundaries now, so that we don't get */
		/* an infinite loop. */
		cdev->ymin = band * mdev.height;
		cdev->ymax = cdev->ymin + mdev.height;
		if ( cdev->ymax > dev->height )
		  cdev->ymax = dev->height;
		if ( code < 0 )
		  return code;
	   }
	{ byte *src = cdev->mdata + (y - cdev->ymin) * raster;
	  if ( actual_data == 0 )
	    memcpy(str, src, gx_device_raster(dev, 0));
	  else
	    *actual_data = src;
	}
	return 0;
}

/* Initialize for reading. */
private int
clist_render_init(gx_device_clist *dev, gx_device_ht *hdev)
{	int code = clist_flush_buffer(&dev->writer);
	if ( code < 0 )
	  return code;
	/* Write the terminating entry in the block file. */
	/* Note that because of copypage, there may be many such entries. */
	   {	cmd_block cb;
		cb.band = cmd_band_end;
		cb.pos = clist_ftell(cdev->cfile);
		clist_fwrite_chars(&cb, sizeof(cb), cdev->bfile);
		cdev->bfile_end_pos = clist_ftell(cdev->bfile);
		if_debug2('l', "[l]clist_render_init at cfile=%ld, bfile=%ld\n",
			  cb.pos, cdev->bfile_end_pos);
	   }
	cdev->ymin = cdev->ymax = 0;
	return 0;
}

#undef cdev

/* Get a variable-length integer operand. */
#define cmd_getw(var, p)\
  do\
   { if ( *p < 0x80 ) var = *p++;\
     else { const byte *_cbp; var = cmd_get_w(p, &_cbp); p = _cbp; }\
   }\
  while (0)
private long near
cmd_get_w(const byte *p, const byte **rp)
{	long val = *p++ & 0x7f;
	int shift = 7;
	for ( ; val += (long)(*p & 0x7f) << shift, *p++ > 0x7f; shift += 7 )
	  ;
	*rp = p;
	return val;
}

/* Render one band to a specified target device. */
private const byte *cmd_read_rect(P3(int, gx_cmd_rect *, const byte *));
private const byte *cmd_read_matrix(P2(gs_matrix *, const byte *));
private void clist_unpack_short_bits(P5(byte *, const byte *, int, int, uint));
private int clist_decode_segment(P6(gx_path *, int, fixed [6],
  gs_fixed_point *, int, int));
private int
clist_render_band(gx_device_clist_reader *cdev, stream *s, gx_device *target,
  int x0, int y0, gs_memory_t *mem)
{	byte cbuf[cbuf_size];
	    /* data_bits is for short copy_* bits and copy_* compressed, */
	    /* must be aligned */
#define data_bits_size cbuf_size
	byte *data_bits;
	register const byte *cbp;
	const byte *cb_limit;
	const byte *cb_end;
	int end_status = 0;
	int dev_depth = cdev->color_info.depth;
	int dev_depth_bytes = (dev_depth + 7) >> 3;
	gx_device *tdev = target;
	gx_clist_state state;
	gx_color_index _ss *set_colors = state.colors;
	tile_slot *state_slot;
	tile_slot tile_bits;		/* current tile parameters */
	tile_slot bits;			/* for reading bits */
	gx_strip_bitmap state_tile;
	gs_int_point tile_phase;
	gx_path path;
	bool in_path;
	gs_fixed_point ppos;
	gx_clip_path clip_path;
	gx_clip_path *pcpath = NULL;
	gx_device_cpath_accum clip_accum;
	struct _cas {
	  bool lop_enabled;
	  gs_fixed_point fill_adjust;
	} clip_save;
	gs_imager_state imager_state;
	float dash_pattern[cmd_max_dash];
	gx_fill_params fill_params;
	gx_stroke_params stroke_params;
	gx_ht_order order;
	uint ht_data_index;
	gs_color_space color_space;	/* only used for indexed spaces */
	const gs_color_space *pcs;
	void *image_info;
	int image_xywh[4];
	int data_x = 0;
	int code = 0;

#define cmd_get_value(var, cbp)\
  memcpy(&var, cbp, sizeof(var));\
  cbp += sizeof(var)
#define cmd_read(ptr, rsize, cbp)\
  if ( cb_end - cbp >= (rsize) )\
    memcpy(ptr, cbp, rsize), cbp += rsize;\
  else\
   { uint cleft = cb_end - cbp, rleft = (rsize) - cleft;\
     memcpy(ptr, cbp, cleft);\
     sgets(s, ptr + cleft, rleft, &rleft);\
     cbp = cb_end;\
   }
#define cmd_read_short_bits(ptr, bw, ht, ras, cbp)\
  cmd_read(ptr, (bw) * (ht), cbp);\
  clist_unpack_short_bits(ptr, ptr, bw, ht, ras)

	{ static const gx_clist_state cls_initial = { cls_initial_values };
	  state = cls_initial;
	}
	state_tile.id = gx_no_bitmap_id;
	state_tile.shift = state_tile.rep_shift = 0;
	tile_phase.x = tile_phase.y = 0;
	gx_path_init(&path, mem);
	in_path = false;
	/* Initialize the clipping region to the full band. */
	{ gs_fixed_rect cbox;
	  cbox.p.x = 0;
	  cbox.p.y = 0;
	  cbox.q.x = cdev->width;
	  cbox.q.y = cdev->height;
	  gx_cpath_from_rectangle(&clip_path, &cbox, mem);
	}
	imager_state = clist_imager_state_initial;
	imager_state.line_params.dash.pattern = dash_pattern;
	fill_params.fill_zero_width = false;
	/****** SET order.cache ******/
	order.transfer = 0;
	pcs = gs_color_space_DeviceGray();
	data_bits = gs_alloc_bytes(mem, data_bits_size,
				   "clist_render_band(data_bits)");
	if ( data_bits == 0 )
	  { code = gs_note_error(gs_error_VMerror);
	    goto out;
	  }
	cb_limit = cbuf + (cbuf_size - cmd_largest_size + 1);
	cb_end = cbuf + cbuf_size;
	cbp = cb_end;
	while ( !code )
	   {	int op;
		int compress, depth, raster;
		uint rep_width, rep_height;
		byte *source;
		gx_color_index colors[2];
		gx_color_index _ss *pcolor;

		/* Make sure the buffer contains a full command. */
#define set_cb_end(p)\
  cb_end = p;\
  cb_limit = cbuf + (cbuf_size - cmd_largest_size + 1);\
  if ( cb_limit > cb_end ) cb_limit = cb_end
		if ( cbp >= cb_limit )
		{	if ( end_status < 0 )
			  {	/* End of file or error. */
				if ( cbp == cb_end )
				  { code = (end_status == EOFC ? 0 :
					    gs_note_error(gs_error_ioerror));
				    break;
				  }
			  }
			else
			  {	uint nread;
				memmove(cbuf, cbp, cb_end - cbp);
				cbp = cbuf + (cb_end - cbp);
				nread = cb_end - cbp;
				/* Cast to remove 'const'. */
				end_status = sgets(s, (byte *)cbp,
						   nread, &nread);
				set_cb_end(cbp + nread);
				cbp = cbuf;
				process_interrupts();
			  }
		}
		op = *cbp++;
#ifdef DEBUG
		if ( gs_debug_c('L') )
		  { const char **sub = cmd_sub_op_names[op >> 4];
		    if ( sub )
		      dprintf1("[L]%s:\n", sub[op & 0xf]);
		    else
		      dprintf2("[L]%s %d\n", cmd_op_names[op >> 4], op & 0xf);
		  }
#endif
		switch ( op >> 4 )
		   {
		case cmd_op_misc >> 4:
			switch ( op )
			   {
			case cmd_opv_end_run:
				continue;
			case cmd_opv_set_tile_size:
				tile_bits.cb_depth = *cbp++;
				cmd_getw(tile_bits.width, cbp);
				cmd_getw(tile_bits.height, cbp);
				cmd_getw(state_tile.rep_width, cbp);
				cmd_getw(state_tile.rep_height, cbp);
				cmd_getw(tile_bits.shift, cbp);
				state_tile.rep_shift = tile_bits.shift;
				state_tile.shift =
				  (state_tile.rep_shift == 0 ? 0 :
				   (state_tile.rep_shift *
				    (tile_bits.height /
				     state_tile.rep_height))
				   % state_tile.rep_width);
				tile_bits.cb_raster =
				  bitmap_raster(tile_bits.width *
						tile_bits.cb_depth);
				/* Set state_tile the same as tile_bits. */
				state_tile.size.x = tile_bits.width;
				state_tile.size.y = tile_bits.height;
				state_tile.raster = tile_bits.cb_raster;
				break;
			case cmd_opv_set_tile_phase:
				cmd_getw(state.tile_phase.x, cbp);
				cmd_getw(state.tile_phase.y, cbp);
				break;
			case cmd_opv_set_tile_bits:
				bits = tile_bits;
				rep_width = state_tile.rep_width;
				rep_height = state_tile.rep_height;
				compress = 0;
stb:				{ ulong offset;
				  uint width_bits = rep_width * bits.cb_depth;
				  uint width_bytes = (width_bits + 7) >> 3;
				  uint bytes =
				    (rep_height == 0 ? 0 :
				     rep_height == 1 ? width_bytes :
				     compress ? bits.cb_raster * rep_height :
				     bits.cb_raster * (rep_height - 1) +
				      width_bytes);
				  byte *data;

				  cmd_getw(state.tile_index, cbp);
				  cmd_getw(offset, cbp);
				  cdev->tile_table[state.tile_index].offset =
				    offset;
				  state_slot =
				    (tile_slot *)(cdev->chunk.data + offset);
				  state_slot->cb_depth = bits.cb_depth;
				  state_slot->width = bits.width;
				  state_slot->height = bits.height;
				  state_slot->cb_raster = bits.cb_raster;
				  state_tile.data = data =
				    (byte *)(state_slot + 1);
#ifdef DEBUG
				  state_slot->index = state.tile_index;
#endif
				  if ( compress )
				    { /* Decompress the image data. */
				      /* We'd like to share this code */
				      /* with the similar code in copy_*, */
				      /* but right now we don't see how. */
				      stream_cursor_read r;
				      stream_cursor_write w;
				      /* We don't know the data length a */
				      /* priori, so to be conservative, */
				      /* we read the uncompressed size. */
				      uint cleft = cb_end - cbp;

				      if ( cleft < bytes )
					{ uint nread = cbuf_size - cleft;
					  memmove(cbuf, cbp, cleft);
					  end_status = sgets(s, cbuf + cleft, nread, &nread);
					  set_cb_end(cbuf + cleft + nread);
					  cbp = cbuf;
					}
				      r.ptr = cbp - 1;
				      r.limit = cb_end - 1;
				      w.ptr = data - 1;
				      w.limit = w.ptr + bytes;
				      switch ( compress )
					{
					case cmd_compress_rle: 
					  { stream_RLD_state sstate;
					    clist_rld_init(&sstate);
					    (*s_RLD_template.process)
					      ((stream_state *)&sstate, &r, &w, true);
					  } break;
					case cmd_compress_cfe:
					  { stream_CFD_state sstate;
					    clist_cfd_init(&sstate,
							   width_bits,
							   bits.height);
					    (*s_CFD_template.process)
					      ((stream_state *)&sstate, &r, &w, true);
					    (*s_CFD_template.release)
					      ((stream_state *)&sstate);
					  } break;
					default:
					  goto bad_op;
					}
				      cbp = r.ptr + 1;
				    }
				  else if ( width_bytes <= cmd_max_short_width_bytes ||
					    bits.width > rep_width
					  )
				    {	cmd_read_short_bits(data, width_bytes,
					  rep_height, bits.cb_raster, cbp);
				    }
				  else
				    {	cmd_read(data, bytes, cbp);
				    }
				  if ( bits.width > rep_width )
				    bits_replicate_horizontally(data,
				      rep_width * bits.cb_depth, rep_height,
				      bits.cb_raster,
				      bits.width * bits.cb_depth,
				      bits.cb_raster);
				  if ( bits.height > rep_height )
				    bits_replicate_vertically(data,
				      rep_height, bits.cb_raster,
				      bits.height);
#ifdef DEBUG
if ( gs_debug_c('L') )
  {				  dprintf4("[L]index=%u, offset=%lu, data=[%lu..%lu)\n",
					   state.tile_index, offset,
					   (ulong)(data - cdev->chunk.data),
					   (ulong)(data - cdev->chunk.data) +
					     bits.cb_raster * bits.height);
				  cmd_print_bits(data, bits.width, bits.height,
						 bits.cb_raster);
  }
#endif
				}
				continue;
			case cmd_opv_set_bits:
				compress = *cbp & 3;
				bits.cb_depth = *cbp++ >> 2;
				cmd_getw(bits.width, cbp);
				cmd_getw(bits.height, cbp);
				rep_width = bits.width;
				rep_height = bits.height;
				bits.cb_raster =
				  bitmap_raster(bits.width * bits.cb_depth);
				goto stb;
			case cmd_opv_set_tile_color:
				set_colors = state.tile_colors;
				continue;
			case cmd_opv_set_lop:
				cmd_getw(state.lop, cbp);
				continue;
			case cmd_opv_enable_lop:
				state.lop_enabled = 1;
				continue;
			case cmd_opv_disable_lop:
				state.lop_enabled = 0;
				continue;
			case cmd_opv_set_ht_size:
			     { ulong offset;
			       tile_slot *slot;
			       cmd_getw(offset, cbp);
			       slot = (tile_slot *)(cdev->chunk.data + offset);
			       order.bits = (gx_ht_bit *)(slot + 1);
			       cmd_getw(order.width, cbp);
			       cmd_getw(order.height, cbp);
			       cmd_getw(order.raster, cbp);
			       cmd_getw(order.shift, cbp);
			       cmd_getw(order.num_levels, cbp);
			       cmd_getw(order.num_bits, cbp);
			       order.full_height =
				 ht_order_full_height(&order);
			       order.levels =
				 (uint *)(order.bits + order.num_bits);
			     }
			     ht_data_index = 0;
			     continue;
			case cmd_opv_set_ht_data:
				{ int n = *cbp++;
				  if ( ht_data_index < order.num_levels )
				    { /* Setting levels */
				      byte *lptr = (byte *)
					(order.levels + ht_data_index);
				      cmd_read(lptr, n * sizeof(*order.levels),
					       cbp);
				    }
				  else
				    { /* Setting bits */
				      byte *bptr = (byte *)
					(order.bits +
					 (ht_data_index - order.num_levels));
				      cmd_read(bptr, n * sizeof(*order.bits),
					       cbp);
				    }
				  ht_data_index += n;
				}
				continue;
			case cmd_opv_next_data_x:
			     data_x = *cbp++;
			     continue;
			case cmd_opv_delta2_color0:
			     pcolor = &set_colors[0];
			     goto delta2_c;
			case cmd_opv_delta2_color1:
			     pcolor = &set_colors[1];
delta2_c:		     set_colors = state.colors;
			     { gx_color_index b = ((uint)*cbp << 8) + cbp[1];
			       cbp += 2;
			       if ( dev_depth > 24 )
				 *pcolor +=
				   ((b & 0xf000) << 12) + ((b & 0x0f00) << 8) +
				     ((b & 0x00f0) << 4) + (b & 0x000f) -
				       cmd_delta2_32_bias;
			       else
				 *pcolor +=
				   ((b & 0xf800) << 5) + ((b & 0x07e0) << 3) +
				     (b & 0x001f) - cmd_delta2_24_bias;
			     }
			     continue;
			case cmd_opv_set_copy_color:
			     state.color_is_alpha = 0;
			     continue;
			case cmd_opv_set_copy_alpha:
			     state.color_is_alpha = 1;
			     continue;
			default:
			     goto bad_op;
			   }
			tile_phase.x =
			  (state.tile_phase.x + x0) % state_tile.size.x;
			tile_phase.y =
			  (state.tile_phase.y + y0) % state_tile.size.y;
			continue;
		case cmd_op_set_color0 >> 4:
			pcolor = &set_colors[0];
			goto set_color;
		case cmd_op_set_color1 >> 4:
			pcolor = &set_colors[1];
set_color:		set_colors = state.colors;
			switch ( op & 0xf )
			 {
			 case 0:
			   break;
			 case 15:	/* special handling because this may */
					/* require more bits than depth */
			   *pcolor = gx_no_color_index;
			   continue;
			 default:
			   switch ( dev_depth_bytes )
			     {
			     case 4:
			       { gx_color_index b =
				   ((gx_color_index)(op & 0xf) << 8) + *cbp++;
				 *pcolor +=
				   ((b & 07000) << 15) + ((b & 0700) << 10) +
				     ((b & 070) << 5) + (b & 7) -
				       cmd_delta1_32_bias;
				 continue;
			       }
			     case 3:
			       { gx_color_index b = *cbp++;
				 *pcolor +=
				   ((gx_color_index)(op & 0xf) << 16) +
				     ((b & 0xf0) << 4) + (b & 0x0f) -
				       cmd_delta1_24_bias;
				 continue;
			       }
			     case 2:
			       break;
			     case 1:
			       *pcolor += (gx_color_index)(op & 0xf) - 8;
			       continue;
			     }
			 }
			{ gx_color_index color = 0;
			  switch ( dev_depth_bytes )
			    {
			    case 4: color |= (gx_color_index)*cbp++ << 24;
			    case 3: color |= (gx_color_index)*cbp++ << 16;
			    case 2: color |= (gx_color_index)*cbp++ << 8;
			    case 1: color |= (gx_color_index)*cbp++;
			    }
			  *pcolor = color;
			}
			continue;
		case cmd_op_fill_rect >> 4:
		case cmd_op_tile_rect >> 4:
			cbp = cmd_read_rect(op, &state.rect, cbp);
			break;
		case cmd_op_fill_rect_short >> 4:
		case cmd_op_tile_rect_short >> 4:
			state.rect.x += *cbp + cmd_min_short;
			state.rect.width += cbp[1] + cmd_min_short;
			if ( op & 0xf )
			  { state.rect.height += (op & 0xf) + cmd_min_dxy_tiny;
			    cbp += 2;
			  }
			else
			  { state.rect.y += cbp[2] + cmd_min_short;
			    state.rect.height += cbp[3] + cmd_min_short;
			    cbp += 4;
			  }
			break;
		case cmd_op_fill_rect_tiny >> 4:
		case cmd_op_tile_rect_tiny >> 4:
			if ( op & 8 )
			  state.rect.x += state.rect.width;
			else
			  { int txy = *cbp++;
			    state.rect.x += (txy >> 4) + cmd_min_dxy_tiny;
			    state.rect.y += (txy & 0xf) + cmd_min_dxy_tiny;
			  }
			state.rect.width += (op & 7) + cmd_min_dw_tiny;
			break;
		case cmd_op_copy_mono >> 4:
			depth = 1;
			goto copy;
		case cmd_op_copy_color_alpha >> 4:
			if ( state.color_is_alpha )
			  { if ( !(op & 8) )
			      depth = *cbp++;
			  }
			else
			  depth = dev_depth;
copy:			cmd_getw(state.rect.x, cbp);
			cmd_getw(state.rect.y, cbp);
			if ( op & 8 )
			  {	/* Use the current "tile". */
#ifdef DEBUG
				if ( state_slot->index != state.tile_index )
				  { lprintf2("state_slot->index = %d, state.tile_index = %d!\n",
					     state_slot->index,
					     state.tile_index);
				    code = gs_note_error(gs_error_ioerror);
				    goto out;
				  }
#endif
				depth = state_slot->cb_depth;
				state.rect.width = state_slot->width;
				state.rect.height = state_slot->height;
				raster = state_slot->cb_raster;
				source = (byte *)(state_slot + 1);
			  }
			else
			  {	/* Read width, height, bits. */
				/* depth was set already. */
				uint bytes;
				int width_bytes;
				cmd_getw(state.rect.width, cbp);
				cmd_getw(state.rect.height, cbp);
				raster =
				  bitmap_raster(state.rect.width * depth);
				width_bytes =
				  (state.rect.width * depth + 7) >> 3;
				bytes =
				  (state.rect.height == 0 ? 0 :
				   state.rect.height == 1 ? width_bytes :
				   op & 3 ? raster * state.rect.height :
				   raster * (state.rect.height - 1) +
				    width_bytes);
			/* copy_mono and copy_color/alpha */
			/* ensure that the bits will fit in a single buffer, */
			/* even after decompression if compressed. */
#ifdef DEBUG
				if ( bytes > cbuf_size )
				  {	lprintf6("bitmap size exceeds buffer!  width=%d raster=%d height=%d\n    file pos %ld buf pos %d/%d\n",
						 state.rect.width, raster,
						 state.rect.height,
						 stell(s), (int)(cbp - cbuf),
						 (int)(cb_end - cbuf));
					code = gs_note_error(gs_error_ioerror);
					goto out;
				  }
#endif
				if ( op & 3 )
			    {	/* Decompress the image data. */
				stream_cursor_read r;
				stream_cursor_write w;
				/* We don't know the data length a priori, */
				/* so to be conservative, we read */
				/* the uncompressed size. */
				uint cleft = cb_end - cbp;
				if ( cleft < bytes )
				  {	uint nread = cbuf_size - cleft;
					memmove(cbuf, cbp, cleft);
					end_status = sgets(s, cbuf + cleft, nread, &nread);
					set_cb_end(cbuf + cleft + nread);
					cbp = cbuf;
				  }
				r.ptr = cbp - 1;
				r.limit = cb_end - 1;
				w.ptr = data_bits - 1;
				w.limit = w.ptr + data_bits_size;
				switch ( op & 3 )
				  {
				  case cmd_compress_rle: 
				    { stream_RLD_state sstate;
				      clist_rld_init(&sstate);
				      /* The process procedure can't fail. */
				      (*s_RLD_template.process)
					((stream_state *)&sstate, &r, &w, true);
				    } break;
				  case cmd_compress_cfe:
				    { stream_CFD_state sstate;
				      clist_cfd_init(&sstate,
						     state.rect.width,
						     state.rect.height);
				      /* The process procedure can't fail. */
				      (*s_CFD_template.process)
					((stream_state *)&sstate, &r, &w, true);
				      (*s_CFD_template.release)
					((stream_state *)&sstate);
				    } break;
				  default:
				    goto bad_op;
				  }
				cbp = r.ptr + 1;
				source = data_bits;
			    }
				else if ( width_bytes <= cmd_max_short_width_bytes )
				  { source = data_bits;
				    cmd_read_short_bits(source, width_bytes,
							state.rect.height,
							raster, cbp);
				  }
				else
				  { cmd_read(cbuf, bytes, cbp);
				    source = cbuf;
				  }
#ifdef DEBUG
if ( gs_debug_c('L') )
   {				dprintf2("[L]  depth=%d, data_x=%d\n",
					 depth, data_x);
				cmd_print_bits(source, state.rect.width,
					       state.rect.height, raster);
   }
#endif
			  }
			break;
		case cmd_op_delta_tile_index >> 4:
			state.tile_index += (int)(op & 0xf) - 8;
			goto sti;
		case cmd_op_set_tile_index >> 4:
			state.tile_index =
			  ((op & 0xf) << 8) + *cbp++;
sti:			state_slot =
			  (tile_slot *)(cdev->chunk.data +
				cdev->tile_table[state.tile_index].offset);
			if_debug2('L', "[L]index=%u, offset=%lu\n",
				  state.tile_index,
				  cdev->tile_table[state.tile_index].offset);
			state_tile.data = (byte *)(state_slot + 1);
			continue;
		case cmd_op_misc2 >> 4:
			switch ( op )
			   {
			case cmd_opv_set_flatness:
				cmd_get_value(imager_state.flatness, cbp);
				continue;
			case cmd_opv_set_fill_adjust:
				cmd_get_value(imager_state.fill_adjust.x, cbp);
				cmd_get_value(imager_state.fill_adjust.y, cbp);
				continue;
			case cmd_opv_set_ctm:
				cbp = cmd_read_matrix(
				  (gs_matrix *)&imager_state.ctm, cbp);
				continue;
			case cmd_opv_set_line_width:
				{ float width;
				  cmd_get_value(width, cbp);
				  gx_set_line_width(&imager_state.line_params, width);
				}
				continue;
			case cmd_opv_set_misc:
				imager_state.overprint =
				  (*cbp & 0x80) != 0;
				imager_state.stroke_adjust =
				  (*cbp & 0x40) != 0;
				imager_state.line_params.cap =
				  (gs_line_cap)((*cbp >> 3) & 7);
				imager_state.line_params.join =
				  (gs_line_join)(*cbp & 7);
				cbp++;
				continue;
			case cmd_opv_set_miter_limit:
				{ float limit;
				  cmd_get_value(limit, cbp);
				  gx_set_miter_limit(&imager_state.line_params, limit);
				}
				continue;
			case cmd_opv_set_dash:
				{ int n = *cbp++;
				  float offset;
				  cmd_get_value(offset, cbp);
				  memcpy(dash_pattern, cbp, n * sizeof(float));
				  gx_set_dash(&imager_state.line_params.dash,
					      dash_pattern, n, offset,
					      NULL);
				  cbp += n * sizeof(float);
				}
				break;
			case cmd_opv_enable_clip:
				pcpath = &clip_path;
				break;
			case cmd_opv_disable_clip:
				pcpath = NULL;
				break;
			case cmd_opv_begin_clip:
				pcpath = NULL;
				gx_cpath_release(&clip_path);
				gx_cpath_accum_begin(&clip_accum, mem);
				tdev = (gx_device *)&clip_accum;
				clip_save.lop_enabled = state.lop_enabled;
				clip_save.fill_adjust =
				  imager_state.fill_adjust;
				state.lop_enabled = false;
				imager_state.fill_adjust.x =
				  imager_state.fill_adjust.y = fixed_half;
				break;
			case cmd_opv_end_clip:
				gx_cpath_accum_end(&clip_accum, &clip_path);
				gx_cpath_set_outside(&clip_path, *cbp++);
				pcpath = &clip_path;
				tdev = target;
				state.lop_enabled = clip_save.lop_enabled;
				imager_state.fill_adjust =
				  clip_save.fill_adjust;
				break;
			case cmd_opv_set_color_space:
				{ byte b = *cbp++;
				  /****** TO BE COMPLETED ******/
				  if ( b & 8 )
				    cmd_getw(color_space.params.indexed.hival,
					     cbp);
				}
				break;
			case cmd_opv_set_color_mapping:
				goto bad_op;	/****** NYI ******/
			case cmd_opv_begin_image:
				{ byte b = *cbp++;
				  int bpci = b >> 5;
				  static byte bpc[6] = {1, 1, 2, 4, 8, 12};
				  byte shape = *cbp++;
				  gs_image_t image;
				  gx_drawing_color devc;
				  int num_components;

				  cmd_getw(image.Width, cbp);
				  cmd_getw(image.Height, cbp);
				  if ( b & (1 << 3) )
				    { /* Non-standard ImageMatrix */
				      cbp = cmd_read_matrix(
					(gs_matrix *)&imager_state.ctm, cbp);
				    }
				  else
				    { image.ImageMatrix.xx = image.Width;
				      image.ImageMatrix.xy = 0;
				      image.ImageMatrix.yx = 0;
				      image.ImageMatrix.yy = -image.Height;
				      image.ImageMatrix.tx = 0;
				      image.ImageMatrix.ty = image.Height;
				    }
				  image.BitsPerComponent = bpc[bpci];
				  if ( bpci == 0 )
				    { image.ColorSpace = 0;
				      image.ImageMask = true;
				      image.Decode[0] = 0;
				      image.Decode[1] = 1;
				      num_components = 1;
				    }
				  else
				    { image.ColorSpace = pcs;
				      image.ImageMask = false;
				      if ( gs_color_space_get_index(pcs) == gs_color_space_index_Indexed )
					{ image.Decode[0] = 0;
					  image.Decode[1] =
					    (1 << image.BitsPerComponent) - 1;
					}
				      else
					{ static const float decode01[8] =
					    { 0, 1, 0, 1, 0, 1, 0, 1 };
					  memcpy(image.Decode, decode01,
						 sizeof(image.Decode));
					}
				      num_components =
					gs_color_space_num_components(pcs);
				    }
				  image.Interpolate = (b & (1 << 4)) != 0;
				  if ( b & (1 << 2) )
				    { /* Non-standard Decode */
				      byte dflags = *cbp++;
				      int i;

				      for ( i = 0; i < num_components * 2;
					    dflags <<= 2, i += 2
					  )
					switch ( (dflags >> 6) & 3 )
					  {
					  case 0:	/* default */
					    break;
					  case 1:	/* swapped default */
					    image.Decode[i] =
					      image.Decode[i+1];
					    image.Decode[i+1] = 0;
					    break;
					  case 3:
					    cmd_get_value(image.Decode[i],
							  cbp);
					    /* falls through */
					  case 2:
					    cmd_get_value(image.Decode[i+1],
							  cbp);
					  }
				    }
				  if ( b & (1 << 1) )
				    { if ( image.ImageMask )
					image.adjust = true;
				      else
					image.CombineWithColor = true;
				    }
				  color_set_pure(&devc, state.colors[1]);
				  code = (*dev_proc(tdev, begin_image))
				    (tdev, &imager_state, &image,
				     gs_image_format_chunky, shape,
				     &devc, pcpath, mem, &image_info);
				  if ( code < 0 )
				    goto out;
				  image_xywh[0] = 0;
				  image_xywh[1] = 0;
				  image_xywh[2] = image.Width;
				  image_xywh[3] = 1;
				}
				break;
			case cmd_opv_image_data:
				{ uint nbytes;

				  cmd_get_value(nbytes, cbp);
				  if ( nbytes == 0 )
				    { code = (*dev_proc(tdev, end_image))
					(tdev, image_info, true);
				    }
				  else
				    { byte b = *cbp++;
				      int diff, i;
				      const byte *data;

				      for ( i = 0; i < 4; b <<= 2, ++i )
					switch ( (b >> 6) & 3 )
					  {
					  case 1:
					    image_xywh[i]++;
					  case 0:
					    break;
					  case 2:
					    cmd_get_value(diff, cbp);
					    image_xywh[i] += diff;
					    break;
					  case 3:
					    cmd_get_value(diff, cbp);
					    image_xywh[i] += diff;
					    break;
					  }
				      if ( cb_end - cbp >= nbytes )
					{ data = cbp;
					  cbp += nbytes;
					}
				      else
					{ uint cleft = cb_end - cbp;
					  uint rleft = nbytes - cleft;

					  memmove(cbuf, cbp, cleft);
					  sgets(s, cbuf + cleft, rleft,
						&rleft);
					  data = cbuf;
					  cbp = cb_end;  /* force refill */
					}
				      code = (*dev_proc(tdev, image_data))
					(tdev, image_info, &data,
					 nbytes / image_xywh[3],
					 image_xywh[0], image_xywh[1],
					 image_xywh[2], image_xywh[3]);
				    }
				}
				if ( code < 0 )
				  goto out;
				continue;
			default:
				goto bad_op;
			   }
			continue;
		case cmd_op_segment >> 4:
		  {	fixed vs[6];
			int i, code;

			for ( i = 0;
			      i < clist_segment_op_num_operands[op & 0xf];
			      ++i
			    )
			  { fixed v;
			    int b = *cbp;
			    switch ( b >> 5 )
			      {
			      case 0: case 1:
				vs[i++] =
				  ((fixed)((b ^ 0x20) - 0x20) << 13) +
				    ((int)cbp[1] << 5) + (cbp[2] >> 3);
				cbp += 2;
				v = (int)((*cbp & 7) ^ 4) - 4;
				break;
			      case 2: case 3:
				v = (b ^ 0x60) - 0x20;
				break;
			      case 4: case 5:
				v = (((b ^ 0xa0) - 0x20) << 8) + *++cbp;
				break;
			      case 6:
				v = (b ^ 0xd0) - 0x10;
				vs[i] =
				  ((v << 8) + cbp[1]) << (_fixed_shift - 2);
				cbp += 2;
				continue;
			      default /*case 7*/:
				v = (int)(*++cbp ^ 0x80) - 0x80;
				for ( b = 0; b < sizeof(fixed) - 3; ++b )
				  v = (v << 8) + *++cbp;
				break;
			      }
			    cbp += 3;
			    /* Absent the cast in the next statement, */
			    /* the Borland C++ 4.5 compiler incorrectly */
			    /* sign-extends the result of the shift. */
			    vs[i] =
			      (v << 16) + (uint)(cbp[-2] << 8) + cbp[-1];
			  }
			if ( !in_path )
			  { ppos.x = int2fixed(state.rect.x);
			    ppos.y = int2fixed(state.rect.y);
			    in_path = true;
			  }
			code = clist_decode_segment(&path, op, vs, &ppos,
						    x0, y0);
			if ( code < 0 )
			  goto out;
		  }	continue;
		case cmd_op_path >> 4:
		  {	gx_device_color devc;
			gx_ht_tile ht_tile;
			switch ( op )
			  {
			  case cmd_opv_fill:
				fill_params.rule = gx_rule_winding_number;
				goto fill_pure;
			  case cmd_opv_eofill:
				fill_params.rule = gx_rule_even_odd;
fill_pure:			color_set_pure(&devc, state.colors[1]);
				goto fill;
			  case cmd_opv_htfill:
				fill_params.rule = gx_rule_winding_number;
				goto fill_ht;
			  case cmd_opv_hteofill:
				fill_params.rule = gx_rule_even_odd;
fill_ht:			ht_tile.tiles = state_tile;
				color_set_binary_tile(&devc,
						      state.tile_colors[0],
						      state.tile_colors[1],
						      &ht_tile);
				devc.phase = tile_phase;
fill:				fill_params.adjust = imager_state.fill_adjust;
				fill_params.flatness = imager_state.flatness;
				code = gx_fill_path_only(&path, tdev,
							 &imager_state,
							 &fill_params,
							 &devc, pcpath);
				break;
			  case cmd_opv_stroke:
				color_set_pure(&devc, state.colors[1]);
				goto stroke;
			  case cmd_opv_htstroke:
				ht_tile.tiles = state_tile;
				color_set_binary_tile(&devc,
						      state.tile_colors[0],
						      state.tile_colors[1],
						      &ht_tile);
				devc.phase = tile_phase;
stroke:				stroke_params.flatness = imager_state.flatness;
				code = gx_stroke_path_only(&path,
						(gx_path *)0, tdev,
						&imager_state, &stroke_params,
						&devc, pcpath);
				break;
			  default:
				goto bad_op;
			  }
		  }
			if ( in_path ) /* path might be empty! */
			  { state.rect.x = fixed2int_var(ppos.x);
			    state.rect.y = fixed2int_var(ppos.y);
			    in_path = false;
			  }
			gx_path_release(&path);
			gx_path_init(&path, mem);
			if ( code < 0 )
			  goto out;
			continue;
		default:
bad_op:			lprintf5("Bad op %02x band y0 = %d file pos %ld buf pos %d/%d\n",
				 op, y0, stell(s), (int)(cbp - cbuf), (int)(cb_end - cbuf));
			   {	const byte *pp;
				for ( pp = cbuf; pp < cb_end; pp += 10 )
				  { dprintf1("%4d:", (int)(pp - cbuf));
				    dprintf10(" %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
					pp[0], pp[1], pp[2], pp[3], pp[4],
					pp[5], pp[6], pp[7], pp[8], pp[9]);
				  }
			   }
			code = gs_note_error(gs_error_Fatal);
			goto out;
		   }
		if_debug4('L', "[L]  x=%d y=%d w=%d h=%d\n",
			  state.rect.x, state.rect.y, state.rect.width,
			  state.rect.height);
		switch ( op >> 4 )
		   {
		case cmd_op_fill_rect >> 4:
		case cmd_op_fill_rect_short >> 4:
		case cmd_op_fill_rect_tiny >> 4:
			if ( !state.lop_enabled )
			  { code = (*dev_proc(tdev, fill_rectangle))
			      (tdev, state.rect.x - x0, state.rect.y - y0,
			       state.rect.width, state.rect.height,
			       state.colors[1]);
			    break;
			  }
			source = NULL;
			data_x = 0;
			raster = 0;
			colors[0] = colors[1] = state.colors[1];
			pcolor = colors;
do_rop:			code = (*dev_proc(tdev, strip_copy_rop))
			  (tdev, source, data_x, raster, gx_no_bitmap_id,
			   pcolor, &state_tile,
			   (state.tile_colors[0] == gx_no_color_index &&
			    state.tile_colors[1] == gx_no_color_index ?
			    NULL : state.tile_colors),
			   state.rect.x - x0, state.rect.y - y0,
			   state.rect.width, state.rect.height,
			   tile_phase.x, tile_phase.y, state.lop);
			data_x = 0;
			break;
		case cmd_op_tile_rect >> 4:
		case cmd_op_tile_rect_short >> 4:
		case cmd_op_tile_rect_tiny >> 4:
			/* Currently we don't use lop with tile_rectangle. */
			code = (*dev_proc(tdev, strip_tile_rectangle))
			  (tdev, &state_tile,
			   state.rect.x - x0, state.rect.y - y0,
			   state.rect.width, state.rect.height,
			   state.tile_colors[0], state.tile_colors[1],
			   tile_phase.x, tile_phase.y);
			break;
		case cmd_op_copy_mono >> 4:
			if ( state.lop_enabled )
			  { pcolor = state.colors;
			    goto do_rop;
			  }
			if ( (op & cmd_copy_ht_color) || pcpath != NULL )
			  { gx_drawing_color dcolor;
			    gx_ht_tile ht_tile;
			    if ( op & cmd_copy_ht_color )
			      { /* Screwy C assignment rules don't allow: */
				/* dcolor.colors = state.tile_colors; */
				ht_tile.tiles = state_tile;
				color_set_binary_tile(&dcolor,
					state.tile_colors[0],
					state.tile_colors[1], &ht_tile);
				dcolor.phase = tile_phase;
			      }
			    else
			      { color_set_pure(&dcolor, state.colors[1]);
			      }
			    code = (*dev_proc(tdev, fill_mask))
			      (tdev, source, data_x, raster, gx_no_bitmap_id,
			       state.rect.x - x0, state.rect.y - y0,
			       state.rect.width, state.rect.height,
			       &dcolor, 1, state.lop, pcpath);
			  }
			else
			  code = (*dev_proc(tdev, copy_mono))
			    (tdev, source, data_x, raster, gx_no_bitmap_id,
			     state.rect.x - x0, state.rect.y - y0,
			     state.rect.width, state.rect.height,
			     state.colors[0], state.colors[1]);
			data_x = 0;
			break;
		case cmd_op_copy_color_alpha >> 4:
			if ( state.color_is_alpha )
			  { /****** CAN'T DO ROP WITH ALPHA ******/
			    code = (*dev_proc(tdev, copy_alpha))
			      (tdev, source, data_x, raster, gx_no_bitmap_id,
			       state.rect.x - x0, state.rect.y - y0,
			       state.rect.width, state.rect.height,
			       state.colors[1], depth);
			  }
			else
			  { if ( state.lop_enabled )
			      { pcolor = NULL;
				goto do_rop;
			      }
			    code = (*dev_proc(tdev, copy_color))
			      (tdev, source, data_x, raster, gx_no_bitmap_id,
			       state.rect.x - x0, state.rect.y - y0,
			       state.rect.width, state.rect.height);
			  }
			data_x = 0;
			break;
		default:		/* can't happen */
			goto bad_op;
		   }
	   }
	/* Clean up before we exit. */
out:	gx_cpath_release(&clip_path);
	gx_path_release(&path);
	gs_free_object(mem, data_bits, "clist_render_band(data_bits)");
	if ( code < 0 )
	  return_error(code);
	else
	  return code;
}

/* Unpack a short bitmap */
private void
clist_unpack_short_bits(byte *dest, const byte *src, register int width_bytes,
  int height, uint raster)
{	uint bytes = width_bytes * height;
	const byte *pdata = src + bytes;
	byte *udata = dest + height * raster;
	while ( --height >= 0 )
	  { udata -= raster, pdata -= width_bytes;
	    switch ( width_bytes )
	      {
	      default: memmove(udata, pdata, width_bytes); break;
	      case 6: udata[5] = pdata[5];
	      case 5: udata[4] = pdata[4];
	      case 4: udata[3] = pdata[3];
	      case 3: udata[2] = pdata[2];
	      case 2: udata[1] = pdata[1];
	      case 1: udata[0] = pdata[0];
	      case 0: ;
	    }
	  }
}

/* Read a rectangle. */
private const byte *
cmd_read_rect(int op, register gx_cmd_rect *prect, register const byte *cbp)
{	cmd_getw(prect->x, cbp);
	if ( op & 0xf )
	  prect->y += ((op >> 2) & 3) - 2;
	else
	  { cmd_getw(prect->y, cbp);
	  }
	cmd_getw(prect->width, cbp);
	if ( op & 0xf )
	  prect->height += (op & 3) - 2;
	else
	  { cmd_getw(prect->height, cbp);
	  }
	return cbp;
}

/* Read a transformation matrix. */
private const byte *
cmd_read_matrix(gs_matrix *pmat, const byte *cbp)
{	byte b = *cbp++;
	float coeff[6];
	int i;

	for ( i = 0; i < 4; i += 2, b <<= 2 )
	  if ( !(b & 0xc0) )
	    coeff[i] = coeff[i^3] = 0.0;
	  else
	    { float value;
	      cmd_get_value(value, cbp);
	      coeff[i] = value;
	      switch ( (b >> 6) & 3 )
		{
		case 1:
		  coeff[i^3] = value; break;
		case 2:
		  coeff[i^3] = -value; break;
		case 3:
		  cmd_get_value(coeff[i^3], cbp);
		}
	    }
	for ( ; i < 6; ++i, b <<= 1 )
	  if ( b & 0x80 )
	    { cmd_get_value(coeff[i], cbp);
	    }
	  else
	    coeff[i] = 0.0;
	pmat->xx = coeff[0];
	pmat->xy = coeff[1];
	pmat->yx = coeff[2];
	pmat->yy = coeff[3];
	pmat->tx = coeff[4];
	pmat->ty = coeff[5];
	return cbp;
}

/* ------ Path operations ------ */

/* Decode a path segment. */
private int
clist_decode_segment(gx_path *ppath, int op, fixed vs[6],
  gs_fixed_point *ppos, int x0, int y0)
{	fixed px = ppos->x - int2fixed(x0);
	fixed py = ppos->y - int2fixed(y0);
	int code;
#define A vs[0]
#define B vs[1]
#define C vs[2]
#define D vs[3]
#define E vs[4]
#define F vs[5]

	switch ( op )
	   {
	case cmd_opv_rmoveto:
		code = gx_path_add_point(ppath, px += A, py += B);
		break;
	case cmd_opv_rlineto:
		code = gx_path_add_line(ppath, px += A, py += B);
		break;
	case cmd_opv_hlineto:
		code = gx_path_add_line(ppath, px += A, py);
		break;
	case cmd_opv_vlineto:
		code = gx_path_add_line(ppath, px, py += A);
		break;
	case cmd_opv_rrcurveto: /* a b c d e f => a b a+c b+d a+c+e b+d+f */
		E += (C += A);
		F += (D += B);
curve:		code = gx_path_add_curve(ppath, px + A, py + B,
					 px + C, py + D,
					 px + E, py + F);
		px += E, py += F;
		break;
	case cmd_opv_hvcurveto:	/* a b c d => a 0 a+b c a+b c+d */
		F = C + D, D = C, E = C = A + B, B = 0;
		goto curve;
	case cmd_opv_vhcurveto:	/* a b c d => 0 a b a+c b+d a+c */
		E = B + D, F = D = A + C, C = B, B = A, A = 0;
		goto curve;
	case cmd_opv_nrcurveto:	/* a b c d => 0 0 a b a+c b+d */
		F = B + D, E = A + C, D = B, C = A, B = A = 0;
		goto curve;
	case cmd_opv_rncurveto:	/* a b c d => a b a+c b+d a+c b+d */
		F = D += B, E = C += A;
		goto curve;
	case cmd_opv_rmlineto:
		if ( (code = gx_path_add_point(ppath, px += A, py += B)) < 0 )
		  break;
		code = gx_path_add_line(ppath, px += C, py += D);
		break;
	case cmd_opv_rm2lineto:
		if ( (code = gx_path_add_point(ppath, px += A, py += B)) < 0 ||
		     (code = gx_path_add_line(ppath, px += C, py += D)) < 0
		   )
		  break;
		code = gx_path_add_line(ppath, px += E, py += F);
		break;
	case cmd_opv_rm3lineto:
		if ( (code = gx_path_add_point(ppath, px += A, py += B)) < 0 ||
		     (code = gx_path_add_line(ppath, px += C, py += D)) < 0 ||
		     (code = gx_path_add_line(ppath, px += E, py += F)) < 0
		   )
		  break;
		code = gx_path_add_line(ppath, px -= C, py -= D);
		break;
	case cmd_opv_closepath:
		code = gx_path_close_subpath(ppath);
		gx_path_current_point(ppath, (gs_fixed_point *)vs);
		px = A, py = B;
		break;
	default:
		return_error(gs_error_rangecheck);
	   }
#undef A
#undef B
#undef C
#undef D
#undef E
#undef F
	ppos->x = px + int2fixed(x0);
	ppos->y = py + int2fixed(y0);
	return code;
}
