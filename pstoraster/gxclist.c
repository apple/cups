/* Copyright (C) 1991, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gxclist.c */
/* Command list writing for Ghostscript. */
#include "memory_.h"
#include "gx.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gsbitops.h"
#include "gsutil.h"			/* for gs_next_ids */
#include "gxdevice.h"
#include "gxdevmem.h"			/* must precede gxcldev.h */
#include "gxcldev.h"

#define cdev cwdev

/* Forward declarations of procedures */
private dev_proc_open_device(clist_open);
private dev_proc_output_page(clist_output_page);
private dev_proc_fill_rectangle(clist_fill_rectangle);
private dev_proc_copy_mono(clist_copy_mono);
private dev_proc_copy_color(clist_copy_color);
private dev_proc_copy_alpha(clist_copy_alpha);
extern dev_proc_get_bits(clist_get_bits);	/* in gxclread.c */
private dev_proc_get_band(clist_get_band);
private dev_proc_strip_tile_rectangle(clist_strip_tile_rectangle);
private dev_proc_strip_copy_rop(clist_strip_copy_rop);

/* The device procedures */
gx_device_procs gs_clist_device_procs =
{	clist_open,
	gx_forward_get_initial_matrix,
	gx_default_sync_output,
	clist_output_page,
	gx_default_close_device,
	gx_forward_map_rgb_color,
	gx_forward_map_color_rgb,
	clist_fill_rectangle,
	gx_default_tile_rectangle,
	clist_copy_mono,
	clist_copy_color,
	gx_default_draw_line,
	clist_get_bits,
	gx_forward_get_params,
	gx_forward_put_params,
	gx_forward_map_cmyk_color,
	gx_forward_get_xfont_procs,
	gx_forward_get_xfont_device,
	gx_forward_map_rgb_alpha_color,
	gx_forward_get_page_device,
	gx_forward_get_alpha_bits,
	clist_copy_alpha,
	clist_get_band,
	gx_default_copy_rop,
	gx_default_fill_path,
	gx_default_stroke_path,
	gx_default_fill_mask,
	gx_default_fill_trapezoid,
	gx_default_fill_parallelogram,
	gx_default_fill_triangle,
	gx_default_draw_thin_line,
	gx_default_begin_image,
	gx_default_image_data,
	gx_default_end_image,
	clist_strip_tile_rectangle,
	clist_strip_copy_rop,
};

/* ------ Define the command set and syntax ------ */

/* Define the clipping enable/disable opcodes. */
/* The path extensions initialize these to their proper values. */
byte cmd_opvar_disable_clip = 0xff;
byte cmd_opvar_enable_clip = 0xff;

#ifdef DEBUG
const char *cmd_op_names[16] = { cmd_op_name_strings };
private const char *cmd_misc_op_names[16] = { cmd_misc_op_name_strings };
const char **cmd_sub_op_names[16] =
{	cmd_misc_op_names, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};
private ulong far_data cmd_op_counts[256];
private ulong far_data cmd_op_sizes[256];
private ulong cmd_tile_reset, cmd_tile_found, cmd_tile_added;
extern ulong cmd_diffs[5];		/* in gxclpath.c */
private ulong cmd_same_band, cmd_other_band;
int
cmd_count_op(int op, uint size)
{	cmd_op_counts[op]++;
	cmd_op_sizes[op] += size;
	if ( gs_debug_c('L') )
	  { const char **sub = cmd_sub_op_names[op >> 4];
	    if ( sub )
	      dprintf2(", %s(%u)\n", sub[op & 0xf], size);
	    else
	      dprintf3(", %s %d(%u)\n", cmd_op_names[op >> 4], op & 0xf, size);
	    fflush(dstderr);
	  }
	return op;
}
void
cmd_uncount_op(int op, uint size)
{	cmd_op_counts[op]--;
	cmd_op_sizes[op] -= size;
}
#endif

/* Initialization for imager state. */
/* The initial scale is arbitrary. */
const gs_imager_state clist_imager_state_initial =
 { gs_imager_state_initial(300.0 / 72.0) };

/* Initialize the device state */
private int
clist_open(gx_device *dev)
{	/*
	 * The buffer area (data, data_size) holds a bitmap cache when
	 * both writing and reading.  The rest of the space is used for
	 * the command buffer and band state bookkeeping when writing,
	 * and for the rendering buffer (image device) when reading.
	 * For the moment, we divide the space up arbitrarily, except that
	 * we allocate less space for the bitmap cache if the device
	 * doesn't need halftoning.
	 *
	 * This routine requires only data, data_size, and target
	 * to have been set in the device structure, and is idempotent,
	 * so it can be used to check whether a given-size buffer
	 * is large enough.
	 */
	byte *data = cdev->data;
	uint size = cdev->data_size;
/**** MRS - 64-bit align all data structures! ****/
#define alloc_data(n) data += ((n) + 7) & ~7, size -= ((n) + 7) & ~7
	gx_device *target = cdev->target;
	uint raster, nbands, band;
	gx_clist_state *states;
	ulong state_size;
        static const gx_clist_state cls_initial = { cls_initial_values };

	cdev->ymin = cdev->ymax = -1;	/* render_init not done yet */
	{ uint bits_size = (size / 5) & -align_cached_bits_mod;	/* arbitrary */
	  uint hc;
	  if ( (gx_device_has_color(target) ? target->color_info.max_color :
		target->color_info.max_gray) >= 31
	     )
	    { /* No halftones -- cache holds only Patterns & characters. */
	      bits_size -= bits_size >> 2;
	    }
#define min_bits_size 1024
	  if ( bits_size < min_bits_size )
	    bits_size = min_bits_size;
	  /* Partition the bits area between the hash table and the */
	  /* actual bitmaps.  The per-bitmap overhead is about 24 bytes; */
	  /* if the average character size is 10 points, its bitmap takes */
	  /* about 24 + 0.5 * 10/72 * xdpi * 10/72 * ydpi / 8 bytes */
	  /* (the 0.5 being a fudge factor to account for characters */
	  /* being narrower than they are tall), which gives us */
	  /* a guideline for the size of the hash table. */
	  { uint avg_char_size =
	      (uint)(dev->x_pixels_per_inch * dev->y_pixels_per_inch *
		     (0.5 * 10/72 * 10/72 / 8)) + 24;
	    hc = bits_size / avg_char_size;
	  }
	  while ( (hc + 1) & hc )
	    hc |= hc >> 1;	/* make mask (power of 2 - 1) */
	  if ( hc < 0xff )
	    hc = 0xff;		/* make allowance for halftone tiles */
	  else if ( hc > 0xfff )
	    hc = 0xfff;		/* cmd_op_set_tile_index has 12-bit operand */
	  cdev->tile_hash_mask = hc;
	  cdev->tile_max_count = hc - (hc >> 2);
	  hc = (hc + 1) * sizeof(tile_hash);
	  cdev->tile_table = (tile_hash *)data;
	  memset(data, 0, hc);
	  alloc_data(hc);
	  bits_size -= hc;
	  gx_bits_cache_chunk_init(&cdev->chunk, data, bits_size);
	  alloc_data(bits_size);
	  gx_bits_cache_init(&cdev->bits, &cdev->chunk);
	}
	raster = gx_device_raster(target, 1) + sizeof(byte *);
	cdev->band_height = size / raster;
	if ( cdev->band_height == 0 )	/* can't even fit one scan line */
	  return_error(gs_error_limitcheck);
	nbands = target->height / cdev->band_height + 1;
	cdev->nbands = nbands;
	if_debug4('l', "[l]width=%d, raster=%d, band_height=%d, nbands=%d\n",
		  target->width, raster, cdev->band_height, cdev->nbands);
	state_size = nbands * (ulong)sizeof(gx_clist_state);
	if ( state_size + sizeof(cmd_prefix) + cmd_largest_size + raster + 4 > size )		/* not enough room */
	  return_error(gs_error_limitcheck);
	cdev->mdata = data;
	cdev->states = states = (gx_clist_state *)data;
	alloc_data((uint)state_size);
	cdev->cbuf = data;
	cdev->cnext = data;
	cdev->cend = data + size;
	cdev->ccl = 0;
	cdev->all_band_list.head = cdev->all_band_list.tail = 0;
	for ( band = 0; band < nbands; band++)
          *states++ = cls_initial;
#undef alloc_data
	/* Round up the size of the band mask so that */
	/* the bits, which follow it, stay aligned. */
	cdev->tile_band_mask_size =
	  ((nbands + (align_bitmap_mod * 8 - 1)) >> 3) &
	    ~(align_bitmap_mod - 1);
	/* Initialize the all-band parameters to impossible values, */
	/* to force them to be written the first time they are used. */
	memset(&cdev->tile_params, 0, sizeof(cdev->tile_params));
	cdev->tile_depth = 0;
	cdev->imager_state = clist_imager_state_initial;
	cdev->clip_path = NULL;
	cdev->clip_path_id = gx_no_bitmap_id;
	cdev->color_space = 0;
	return 0;
}

/* Clean up after rendering a page. */
private int
clist_output_page(gx_device *dev, int num_copies, int flush)
{	if ( flush )
	   {	clist_rewind(cdev->cfile, true);
		clist_rewind(cdev->bfile, true);
		cdev->bfile_end_pos = 0;
	   }
	else
	   {	clist_fseek(cdev->cfile, 0L, SEEK_END);
		clist_fseek(cdev->bfile, 0L, SEEK_END);
	   }
	return clist_open(dev);		/* reinitialize */
}

/* Print statistics. */
#ifdef DEBUG
void
cmd_print_stats(void)
{	int ci, cj;
	dprintf3("[l]counts: reset = %lu, found = %lu, added = %lu\n",
	         cmd_tile_reset, cmd_tile_found, cmd_tile_added);
	dprintf5("     diff 2.5 = %lu, 3 = %lu, 4 = %lu, 2 = %lu, >4 = %lu\n",
		 cmd_diffs[0], cmd_diffs[1], cmd_diffs[2], cmd_diffs[3],
		 cmd_diffs[4]);
	dprintf2("     same_band = %lu, other_band = %lu\n",
		 cmd_same_band, cmd_other_band);
	for ( ci = 0; ci < 0x100; ci += 0x10 )
	   {	const char **sub = cmd_sub_op_names[ci >> 4];
		if ( sub != 0 )
		  { dprintf1("[l]  %s =", cmd_op_names[ci >> 4]);
		    for ( cj = ci; cj < ci + 0x10; cj += 2 )
		      dprintf6("\n\t%s = %lu(%lu), %s = %lu(%lu)",
			       sub[cj-ci],
			       cmd_op_counts[cj], cmd_op_sizes[cj],
			       sub[cj-ci+1],
			       cmd_op_counts[cj+1], cmd_op_sizes[cj+1]);
		  }
		else
		  { ulong tcounts = 0, tsizes = 0;
		    for ( cj = ci; cj < ci + 0x10; cj++ )
		      tcounts += cmd_op_counts[cj],
		      tsizes += cmd_op_sizes[cj];
		    dprintf3("[l]  %s (%lu,%lu) =\n\t",
			     cmd_op_names[ci >> 4], tcounts, tsizes);
		    for ( cj = ci; cj < ci + 0x10; cj++ )
		      if ( cmd_op_counts[cj] == 0 )
			dputs(" -");
		      else
			dprintf2(" %lu(%lu)", cmd_op_counts[cj],
				 cmd_op_sizes[cj]);
		  }
		dputs("\n");
	   }
}
#endif				/* DEBUG */

/* ------ Writing ------ */

/* Utilities */

#define cmd_set_rect(rect)\
  ((rect).x = x, (rect).y = y,\
   (rect).width = width, (rect).height = height)

/* Write the commands for one band. */
private int
cmd_write_band(gx_device_clist_writer *cldev, int band, cmd_list *pcl)
{	const cmd_prefix *cp = pcl->head;

	if ( cp != 0 )
	{	clist_file_ptr cfile = cldev->cfile;
		clist_file_ptr bfile = cldev->bfile;
		cmd_block cb;
		char end = cmd_count_op(cmd_opv_end_run, 1);
		int code;

		cb.band = band;
		cb.pos = clist_ftell(cfile);
		if_debug2('l', "[l]writing for band %d at %ld\n",
			  band, cb.pos);
		clist_fwrite_chars(&cb, sizeof(cb), bfile);
		pcl->tail->next = 0;	/* terminate the list */
		for ( ; cp != 0; cp = cp->next )
		  {
#ifdef DEBUG
		    if ( (const byte *)cp < cldev->cbuf ||
			 (const byte *)cp >= cldev->cend ||
			 cp->size > cldev->cend - (const byte *)cp
		       )
		      { lprintf1("cmd_write_band error at 0x%lx\n", (ulong)cp);
			return_error(gs_error_Fatal);
		      }
#endif
		    clist_fwrite_chars(cp + 1, cp->size, cfile);
		  }
		pcl->head = pcl->tail = 0;
		clist_fwrite_chars(&end, 1, cfile);
		process_interrupts();
		if ( (code = clist_ferror_code(bfile)) < 0 ||
		     (code = clist_ferror_code(cfile)) < 0
		   )
		  return_error(code);
	}
	return 0;
}

/* Write out the buffered commands, and reset the buffer. */
private int
cmd_write_buffer(gx_device_clist_writer *cldev)
{	int nbands = cldev->nbands;
	gx_clist_state *pcls;
	int band;
	int code = cmd_write_band(cldev, cmd_band_all, &cldev->all_band_list);

	for ( band = 0, pcls = cldev->states;
	      code >= 0 && band < nbands; band++, pcls++
	    )
	  code = cmd_write_band(cldev, band, &pcls->list);
	cldev->cnext = cldev->cbuf;
	cldev->ccl = 0;
	cldev->all_band_list.head = cldev->all_band_list.tail = 0;
#ifdef DEBUG
	if ( gs_debug_c('l') )
	  cmd_print_stats();
#endif
	return_check_interrupt(code);
}
/* Export under a different name for gxclread.c */
int
clist_flush_buffer(gx_device_clist_writer *cldev)
{	return cmd_write_buffer(cldev);
}

/* Add a command to the appropriate band list, */
/* and allocate space for its data. */
/* Return the pointer to the data area. */
/* If an error occurs, set cldev->error_code and return 0. */
#define cmd_headroom (sizeof(cmd_prefix) + arch_align_ptr_mod)
byte *
cmd_put_list_op(gx_device_clist_writer *cldev, cmd_list *pcl, uint size)
{	byte *dp = cldev->cnext;
	if ( size + cmd_headroom > cldev->cend - dp )
	  { int code = cldev->error_code = cmd_write_buffer(cldev);
	    if ( code < 0 )
	      return 0;
	    return cmd_put_list_op(cldev, pcl, size);
	  }
	if ( cldev->ccl == pcl )
	  { /* We're adding another command for the same band. */
	    /* Tack it onto the end of the previous one. */
	    cmd_count_add1(cmd_same_band);
#ifdef DEBUG
	    if ( pcl->tail->size > dp - (byte *)(pcl->tail + 1) )
	      { lprintf1("cmd_put_list_op error at 0x%lx\n", (ulong)pcl->tail);
	      }
#endif
	    pcl->tail->size += size;
	  }
	else
	  { /* Skip to an appropriate alignment boundary. */
	    /* (We assume the command buffer itself is aligned.) */
	    cmd_prefix *cp =
	      (cmd_prefix *)(dp +
			     ((cldev->cbuf - dp) & (arch_align_ptr_mod - 1)));
	    cmd_count_add1(cmd_other_band);
	    dp = (byte *)(cp + 1);
	    if ( pcl->tail != 0 )
	      {
#ifdef DEBUG
		if ( pcl->tail < pcl->head ||
		     pcl->tail->size > dp - (byte *)(pcl->tail + 1)
		   )
		  { lprintf1("cmd_put_list_op error at 0x%lx\n",
			     (ulong)pcl->tail);
		  }
#endif
		pcl->tail->next = cp;
	      }
	    else
	      pcl->head = cp;
	    pcl->tail = cp;
	    cldev->ccl = pcl;
	    cp->size = size;
	  }
	cldev->cnext = dp + size;
	return dp;
}
#ifdef DEBUG
byte *
cmd_put_op(gx_device_clist_writer *cldev, gx_clist_state *pcls, uint size)
{	if_debug3('L', "[L]band %d: size=%u, left=%u",
		  (int)(pcls - cldev->states),
		  size, (uint)(cldev->cend - cldev->cnext));
	return cmd_put_list_op(cldev, &pcls->list, size);
}
#endif

/* Add a command for all bands. */
byte *
cmd_put_all_op(gx_device_clist_writer *cldev, uint size)
{	cmd_prefix *tail;
	if_debug2('L', "[L]all-band: size=%u, left=%u",
		  size, (uint)(cldev->cend - cldev->cnext));
	if ( cldev->all_band_list.head == 0 ||
	     (tail = cldev->all_band_list.tail,
	      cldev->cnext != (byte *)(tail + 1) + tail->size)
	   )
	  { if ( (cldev->error_code = cmd_write_buffer(cldev)) < 0 )
	      return 0;
	  }
	return cmd_put_list_op(cldev, &cldev->all_band_list, size);
}

/* Write a variable-size positive integer. */
int
cmd_size_w(register uint w)
{	register int size = 1;
	while ( w > 0x7f ) w >>= 7, size++;
	return size;
}
byte *
cmd_put_w(register uint w, register byte *dp)
{	while ( w > 0x7f ) *dp++ = w | 0x80, w >>= 7;
	*dp = w;
	return dp + 1;
}

/* Write a rectangle. */
int
cmd_size_rect(register const gx_cmd_rect *prect)
{	return
	  cmd_sizew(prect->x) + cmd_sizew(prect->y) +
	  cmd_sizew(prect->width) + cmd_sizew(prect->height);
}
private byte *
cmd_put_rect(register const gx_cmd_rect *prect, register byte *dp)
{	cmd_putw(prect->x, dp);
	cmd_putw(prect->y, dp);
	cmd_putw(prect->width, dp);
	cmd_putw(prect->height, dp);
	return dp;
}

int
cmd_write_rect_cmd(gx_device_clist_writer *cldev, gx_clist_state *pcls,
  int op, int x, int y, int width, int height)
{	int dx = x - pcls->rect.x;
	int dy = y - pcls->rect.y;
	int dwidth = width - pcls->rect.width;
	int dheight = height - pcls->rect.height;
#define check_range_xy(rmin, rmax)\
  ((unsigned)(dx - rmin) <= (rmax - rmin) &&\
   (unsigned)(dy - rmin) <= (rmax - rmin))
#define check_range_w(rmin, rmax)\
  ((unsigned)(dwidth - rmin) <= (rmax - rmin))
#define check_ranges(rmin, rmax)\
  (check_range_xy(rmin, rmax) && check_range_w(rmin, rmax) &&\
   (unsigned)(dheight - rmin) <= (rmax - rmin))
	cmd_set_rect(pcls->rect);
	if ( dheight == 0 && check_range_w(cmd_min_dw_tiny, cmd_max_dw_tiny) &&
	     check_range_xy(cmd_min_dxy_tiny, cmd_max_dxy_tiny)
	   )
	  {	byte op_tiny = op + 0x20 + dwidth - cmd_min_dw_tiny;
		byte *dp;

		if ( dx == width - dwidth && dy == 0 )
		  { set_cmd_put_op(dp, cldev, pcls, op_tiny + 8, 1);
		  }
		else
		  { set_cmd_put_op(dp, cldev, pcls, op_tiny, 2);
		    dp[1] = (dx << 4) + dy - (cmd_min_dxy_tiny * 0x11);
		  }
	  }
#define rmin cmd_min_short
#define rmax cmd_max_short
	else if ( check_ranges(rmin, rmax) )
	   {	int dh = dheight - cmd_min_dxy_tiny;
		byte *dp;
		if ( (unsigned)dh <= cmd_max_dxy_tiny - cmd_min_dxy_tiny &&
		     dh != 0 && dy == 0
		   )
		   {	op += dh;
			set_cmd_put_op(dp, cldev, pcls, op + 0x10, 3);
			if_debug3('L', "    rs2:%d,%d,0,%d\n",
				  dx, dwidth, dheight);
		   }
		else
		   {	set_cmd_put_op(dp, cldev, pcls, op + 0x10, 5);
			if_debug4('L', "    rs4:%d,%d,%d,%d\n",
				  dx, dwidth, dy, dheight);
			dp[3] = dy - rmin;
			dp[4] = dheight - rmin;
		   }
		dp[1] = dx - rmin;
		dp[2] = dwidth - rmin;
	   }
#undef rmin
#undef rmax
	else if ( dy >= -2 && dy <= 1 && dheight >= -2 && dheight <= 1 &&
		  (dy + dheight) != -4
		)
	  {	byte *dp;
		int rcsize = 1 + cmd_sizew(x) + cmd_sizew(width);
		set_cmd_put_op(dp, cldev, pcls,
			       op + ((dy + 2) << 2) + dheight + 2, rcsize);
		++dp;
		cmd_put2w(x, width, dp);
	  }
	else
	   {	byte *dp;
		int rcsize = 1 + cmd_size_rect(&pcls->rect);
		set_cmd_put_op(dp, cldev, pcls, op, rcsize);
		if_debug5('L', "    r%d:%d,%d,%d,%d\n",
			  rcsize - 1, dx, dwidth, dy, dheight);
		cmd_put_rect(&pcls->rect, dp + 1);
	   }
	return 0;
}

/* Define the encodings of the different settable colors. */
const clist_select_color_t
  clist_select_color0 = {cmd_op_set_color0, cmd_opv_delta2_color0, 0},
  clist_select_color1 = {cmd_op_set_color1, cmd_opv_delta2_color1, 0},
  clist_select_tile_color0 = {cmd_op_set_color0, cmd_opv_delta2_color0, 1},
  clist_select_tile_color1 = {cmd_op_set_color1, cmd_opv_delta2_color1, 1};
int
cmd_put_color(gx_device_clist_writer *cldev, gx_clist_state *pcls,
  const clist_select_color_t *select,
  gx_color_index color, gx_color_index *pcolor)
{	byte *dp;
	long diff = (long)color - (long)(*pcolor);
	byte op, op_delta2;

	if ( diff == 0 )
	  return 0;
	if ( select->tile_color )
	  set_cmd_put_op(dp, cldev, pcls, cmd_opv_set_tile_color, 1);
	op = select->set_op;
	op_delta2 = select->delta2_op;
	if ( color == gx_no_color_index )
	  {	/*
		 * We must handle this specially, because it may take more
		 * bytes than the color depth.
		 */
		set_cmd_put_op(dp, cldev, pcls, op + 15, 1);
	  }
	else
	   {	long delta;
		byte operand;

		switch ( (cldev->color_info.depth + 15) >> 3 )
		  {
		  case 5:
			if ( !((delta = diff + cmd_delta1_32_bias) &
			      ~cmd_delta1_32_mask) &&
			     (operand =
			      (byte)((delta >> 23) + ((delta >> 18) & 1))) != 0 &&
			     operand != 15
			   )
			  { set_cmd_put_op(dp, cldev, pcls,
					   (byte)(op + operand), 2);
			    dp[1] = (byte)(((delta >> 10) & 0300) +
					   (delta >> 5) + delta);
			    break;
			  }
		  	if ( !((delta = diff + cmd_delta2_32_bias) &
			       ~cmd_delta2_32_mask)
			   )
			  { set_cmd_put_op(dp, cldev, pcls, op_delta2, 3);
			    dp[1] = (byte)((delta >> 20) + (delta >> 16));
			    dp[2] = (byte)((delta >> 4) + delta);
			    break;
			  }
			set_cmd_put_op(dp, cldev, pcls, op, 5);
			*++dp = (byte)(color >> 24);
			goto b3;
		  case 4:
			if ( !((delta = diff + cmd_delta1_24_bias) &
			       ~cmd_delta1_24_mask) &&
			     (operand = (byte)(delta >> 16)) != 0 &&
			     operand != 15
			   )
			  { set_cmd_put_op(dp, cldev, pcls,
					   (byte)(op + operand), 2);
			    dp[1] = (byte)((delta >> 4) + delta);
			    break;
			  }
		  	if ( !((delta = diff + cmd_delta2_24_bias) &
			       ~cmd_delta2_24_mask)
			   )
			  { set_cmd_put_op(dp, cldev, pcls, op_delta2, 3);
			    dp[1] = ((byte)(delta >> 13) & 0xf8) +
			      ((byte)(delta >> 11) & 7);
			    dp[2] = (byte)(((delta >> 3) & 0xe0) + delta);
			    break;
			  }
			set_cmd_put_op(dp, cldev, pcls, op, 4);
b3:			*++dp = (byte)(color >> 16);
			goto b2;
		  case 3:
			set_cmd_put_op(dp, cldev, pcls, op, 3);
b2:			*++dp = (byte)(color >> 8);
			goto b1;
		  case 2:
			if ( diff >= -7 && diff < 7 )
			  { set_cmd_put_op(dp, cldev, pcls,
					   op + (int)diff + 8, 1);
			    break;
			  }
			set_cmd_put_op(dp, cldev, pcls, op, 2);
b1:			dp[1] = (byte)color;
		  }
	   }
	*pcolor = color;
	return 0;
}

/* Put out a command to set the tile colors. */
int
cmd_set_tile_colors(gx_device_clist_writer *cldev, gx_clist_state *pcls,
  gx_color_index color0, gx_color_index color1)
{	if ( color0 != pcls->tile_colors[0] )
	   {	int code = cmd_put_color(cldev, pcls,
					 &clist_select_tile_color0,
					 color0, &pcls->tile_colors[0]);
		if ( code < 0 )
		  return code;
	   }
	if ( color1 != pcls->tile_colors[1] )
	   {	int code = cmd_put_color(cldev, pcls,
					 &clist_select_tile_color1,
					 color1, &pcls->tile_colors[1]);
		if ( code < 0 )
		  return code;
	   }
	return 0;
}

/* Put out a command to set the tile phase. */
int
cmd_set_tile_phase(gx_device_clist_writer *cldev, gx_clist_state *pcls,
  int px, int py)
{	int pcsize;
	byte *dp;

	pcls->tile_phase.x = px;
	pcls->tile_phase.y = py;
	pcsize = 1 + cmd_sizexy(pcls->tile_phase);
	set_cmd_put_op(dp, cldev, pcls, (byte)cmd_opv_set_tile_phase, pcsize);
	++dp;
	cmd_putxy(pcls->tile_phase, dp);
	return 0;
}

/* Write a command to enable or disable the logical operation. */
int
cmd_put_enable_lop(gx_device_clist_writer *cldev, gx_clist_state *pcls,
  int enable)
{	byte *dp;
	set_cmd_put_op(dp, cldev, pcls,
		       (byte)(enable ? cmd_opv_enable_lop :
			      cmd_opv_disable_lop),
		       1);
	pcls->lop_enabled = enable;
	return 0;
}

/* Write a command to enable or disable clipping. */
/* This routine is only called if the path extensions are included. */
int
cmd_put_enable_clip(gx_device_clist_writer *cldev, gx_clist_state *pcls,
  int enable)
{	byte *dp;
	set_cmd_put_op(dp, cldev, pcls,
		       (byte)(enable ? cmd_opvar_enable_clip :
			      cmd_opvar_disable_clip),
		       1);
	pcls->clip_enabled = enable;
	return 0;
}

/* Write a command to set the logical operation. */
int
cmd_set_lop(gx_device_clist_writer *cldev, gx_clist_state *pcls,
  gs_logical_operation_t lop)
{	byte *dp;
	set_cmd_put_op(dp, cldev, pcls,
		       cmd_opv_set_lop, 1 + cmd_sizew(lop));
	++dp;
	cmd_putw(lop, dp);
	pcls->lop = lop;
	return 0;
}

/* ---------------- Driver interface ---------------- */

private int
clist_fill_rectangle(gx_device *dev, int x, int y, int width, int height,
  gx_color_index color)
{	fit_fill(dev, x, y, width, height);
	BEGIN_RECT
	cmd_disable_lop(cdev, pcls);
	if ( color != pcls->colors[1] )
	  {	int code = cmd_put_color(cdev, pcls, &clist_select_color1,
					 color, &pcls->colors[1]);
		if ( code < 0 )
		  return code;
	  }
	{ int code = cmd_write_rect_cmd(cdev, pcls, cmd_op_fill_rect, x, y,
					width, height);
	  if ( code < 0 )
	    return code;
	}
	END_RECT
	return 0;
}

private int
clist_strip_tile_rectangle(gx_device *dev, const gx_strip_bitmap *tile,
  int x, int y, int width, int height,
  gx_color_index color0, gx_color_index color1, int px, int py)
{	int depth =
	  (color1 == gx_no_color_index && color0 == gx_no_color_index ?
	   dev->color_info.depth : 1);
	fit_fill(dev, x, y, width, height);
	BEGIN_RECT
	ulong offset_temp;

	cmd_disable_lop(cdev, pcls);
	if ( !cls_has_tile_id(cdev, pcls, tile->id, offset_temp) )
	   {	if ( tile->id == gx_no_bitmap_id ||
		     clist_change_tile(cdev, pcls, tile, depth) < 0
		   )
		  {	int code =
			  gx_default_strip_tile_rectangle(dev, tile,
						x, y, width, height,
						color0, color1, px, py);
			if ( code < 0 )
			  return code;
			goto endr;
		  }
	   }
	if ( color0 != pcls->tile_colors[0] || color1 != pcls->tile_colors[1] )
	  {	int code = cmd_set_tile_colors(cdev, pcls, color0, color1);
		if ( code < 0 )
		  return code;
	  }
	if ( px != pcls->tile_phase.x || py != pcls->tile_phase.y )
	  {	int code = cmd_set_tile_phase(cdev, pcls, px, py);
		if ( code < 0 )
		  return code;
	  }
	{ int code = cmd_write_rect_cmd(cdev, pcls, cmd_op_tile_rect, x, y,
					width, height);
	  if ( code < 0 )
	    return code;
	}
endr:	;
	END_RECT
	return 0;
}

private int
clist_copy_mono(gx_device *dev,
    const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1)
{	int y0;
	gx_bitmap_id orig_id = id;

	fit_copy(dev, data, data_x, raster, id, x, y, width, height);
	y0 = y;
	BEGIN_RECT
	int dx = data_x & 7;
	int w1 = dx + width;
	const byte *row = data + (y - y0) * raster + (data_x >> 3);
	int code;

	cmd_disable_lop(cdev, pcls);
	cmd_disable_clip(cdev, pcls);
	if ( color0 != pcls->colors[0] )
	  {	code = cmd_set_color0(cdev, pcls, color0);
		if ( code < 0 )
		  return code;
	  }
	if ( color1 != pcls->colors[1] )
	  {	code = cmd_set_color1(cdev, pcls, color1);
		if ( code < 0 )
		  return code;
	  }
	/* Don't bother to check for a possible cache hit: */
	/* tile_rectangle and fill_mask handle those cases. */
copy:	{	gx_cmd_rect rect;
		int rsize;
		byte op = (byte)cmd_op_copy_mono;
		byte *dp;
		uint csize;
		int code;

		rect.x = x, rect.y = y;
		rect.width = w1, rect.height = height;
		rsize = (dx ? 3 : 1) + cmd_size_rect(&rect);
		code = cmd_put_bits(cdev, pcls, row, w1, height, raster,
				    rsize, (orig_id == gx_no_bitmap_id ?
					    1 << cmd_compress_rle :
					    cmd_mask_compress_any),
				    &dp, &csize);
		if ( code < 0 )
		  { if ( code != gs_error_limitcheck )
		      return code;
		    /* The bitmap was too large; split up the transfer. */
		    if ( height > 1 )
		      {	/* Split the transfer by reducing the height.
			 * See the comment above BEGIN_RECT in gxcldev.h.
			 */
			height >>= 1;
			goto copy;
		      }
		    {	/* Split a single (very long) row. */
			int w2 = w1 >> 1;
			code = clist_copy_mono(dev, row, dx + w2,
				raster, gx_no_bitmap_id, x + w2, y,
				w1 - w2, 1, color0, color1);
			if ( code < 0 )
			  return code;
			w1 = w2;
			goto copy;
		    }
		  }
		op += code;
		if ( dx )
		  { *dp++ = cmd_count_op(cmd_opv_next_data_x, 2);
		    *dp++ = dx;
		  }
		*dp++ = cmd_count_op(op, csize);
		cmd_put2w(x, y, dp);
		cmd_put2w(w1, height, dp);
		pcls->rect = rect;
	   }
	END_RECT
	return 0;
}

private int
clist_copy_color(gx_device *dev,
  const byte *data, int data_x, int raster, gx_bitmap_id id,
  int x, int y, int width, int height)
{	int depth = dev->color_info.depth;
	int y0;
	int data_x_bit;

	fit_copy(dev, data, data_x, raster, id, x, y, width, height);
	y0 = y;
	data_x_bit = data_x * depth;
	BEGIN_RECT
	int dx = (data_x_bit & 7) / depth;
	int w1 = dx + width;
	const byte *row = data + (y - y0) * raster + (data_x_bit >> 3);
	int code;

	cmd_disable_lop(cdev, pcls);
	cmd_disable_clip(cdev, pcls);
	if ( pcls->color_is_alpha )
	  { byte *dp;
	    set_cmd_put_op(dp, cdev, pcls, cmd_opv_set_copy_color, 1);
	    pcls->color_is_alpha = 0;
	  }
copy:	  {	gx_cmd_rect rect;
		int rsize;
		byte op = (byte)cmd_op_copy_color_alpha;
		byte *dp;
		uint csize;

		rect.x = x, rect.y = y;
		rect.width = w1, rect.height = height;
		rsize = (dx ? 3 : 1) + cmd_size_rect(&rect);
		code = cmd_put_bits(cdev, pcls, row, w1 * depth,
				    height, raster, rsize,
				    1 << cmd_compress_rle, &dp, &csize);
		if ( code < 0 )
		  { if ( code != gs_error_limitcheck )
		      return code;
		    /* The bitmap was too large; split up the transfer. */
		    if ( height > 1 )
		      {	/* Split the transfer by reducing the height.
			 * See the comment above BEGIN_RECT in gxcldev.h.
			 */
			height >>= 1;
			goto copy;
		      }
		    {	/* Split a single (very long) row. */
			int w2 = w1 >> 1;
			code = clist_copy_color(dev, row, dx + w2,
				raster, gx_no_bitmap_id, x + w2, y,
				w1 - w2, 1);
			if ( code < 0 )
			  return code;
			w1 = w2;
			goto copy;
		    }
		  }
		op += code;
		if ( dx )
		  { *dp++ = cmd_count_op(cmd_opv_next_data_x, 2);
		    *dp++ = dx;
		  }
		*dp++ = cmd_count_op(op, csize);
		cmd_put2w(x, y, dp);
		cmd_put2w(w1, height, dp);
		pcls->rect = rect;

	  }
	END_RECT
	return 0;
}

private int
clist_copy_alpha(gx_device *dev, const byte *data, int data_x,
  int raster, gx_bitmap_id id, int x, int y, int width, int height,
  gx_color_index color, int depth)
{	/* I don't like copying the entire body of clist_copy_color */
	/* just to change 2 arguments and 1 opcode, */
	/* but I don't see any alternative that doesn't require */
	/* another level of procedure call even in the common case. */
	int log2_depth = depth >> 1;	/* works for 1,2,4 */
	int y0;
	int data_x_bit;

	fit_copy(dev, data, data_x, raster, id, x, y, width, height);
	y0 = y;
	data_x_bit = data_x << log2_depth;
	BEGIN_RECT
	int dx = (data_x_bit & 7) >> log2_depth;
	int w1 = dx + width;
	const byte *row = data + (y - y0) * raster + (data_x_bit >> 3);
	int code;

	cmd_disable_lop(cdev, pcls);
	cmd_disable_clip(cdev, pcls);
	if ( !pcls->color_is_alpha )
	  { byte *dp;
	    set_cmd_put_op(dp, cdev, pcls, cmd_opv_set_copy_alpha, 1);
	    pcls->color_is_alpha = 1;
	  }
	if ( color != pcls->colors[1] )
	  {	int code = cmd_set_color1(cdev, pcls, color);
		if ( code < 0 )
		  return code;
	  }
copy:	  {	gx_cmd_rect rect;
		int rsize;
		byte op = (byte)cmd_op_copy_color_alpha;
		byte *dp;
		uint csize;

		rect.x = x, rect.y = y;
		rect.width = w1, rect.height = height;
		rsize = (dx ? 4 : 2) + cmd_size_rect(&rect);
		code = cmd_put_bits(cdev, pcls, row, w1 << log2_depth,
				    height, raster, rsize,
				    1 << cmd_compress_rle, &dp, &csize);
		if ( code < 0 )
		  { if ( code != gs_error_limitcheck )
		      return code;
		    /* The bitmap was too large; split up the transfer. */
		    if ( height > 1 )
		      {	/* Split the transfer by reducing the height.
			 * See the comment above BEGIN_RECT in gxcldev.h.
			 */
			height >>= 1;
			goto copy;
		      }
		    {	/* Split a single (very long) row. */
			int w2 = w1 >> 1;
			code = clist_copy_alpha(dev, row, dx + w2,
				raster, gx_no_bitmap_id, x + w2, y,
				w1 - w2, 1, color, depth);
			if ( code < 0 )
			  return code;
			w1 = w2;
			goto copy;
		    }
		  }
		op += code;
		if ( dx )
		  { *dp++ = cmd_count_op(cmd_opv_next_data_x, 2);
		    *dp++ = dx;
		  }
		*dp++ = cmd_count_op(op, csize);
		*dp++ = depth;
		cmd_put2w(x, y, dp);
		cmd_put2w(w1, height, dp);
		pcls->rect = rect;
	  }
	END_RECT
	return 0;
}

private int
clist_get_band(gx_device *dev, int y, int *band_start)
{	int start;
	if ( y < 0 )
	  y = 0;
	else if ( y >= dev->height )
	  y = dev->height;
	*band_start = start = y - y % cdev->band_height;
	return min(dev->height - start, cdev->band_height);
}

private int
clist_strip_copy_rop(gx_device *dev,
  const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
  const gx_color_index *scolors,
  const gx_strip_bitmap *textures, const gx_color_index *tcolors,
  int x, int y, int width, int height,
  int phase_x, int phase_y, gs_logical_operation_t lop)
{	gs_rop3_t rop = lop & lop_rop_mask;
	gx_strip_bitmap tile_with_id;
	const gx_strip_bitmap *tiles = textures;

	if ( scolors != 0 && scolors[0] != scolors[1] )
	  {	fit_fill(dev, x, y, width, height);
	  }
	else
	  {	fit_copy(dev, sdata, sourcex, sraster, id,
			 x, y, width, height);
	  }
	/*
	 * We shouldn't need to put the logic below inside BEGIN/END_RECT,
	 * but the lop_enabled flags are per-band.
	 */
	BEGIN_RECT
	int code;

	if ( lop != pcls->lop )
	  {	code = cmd_set_lop(cdev, pcls, lop);
		if ( code < 0 )
		  return code;
	  }
	cmd_enable_lop(cdev, pcls);
	if ( rop3_uses_T(rop) )
	  {	if ( tcolors == 0 || tcolors[0] != tcolors[1] )
		  { ulong offset_temp;
		    if ( !cls_has_tile_id(cdev, pcls, tiles->id, offset_temp) )
		      { /* Change tile.  If there is no id, generate one. */
			if ( tiles->id == gx_no_bitmap_id )
			  { tile_with_id = *tiles;
			    tile_with_id.id = gs_next_ids(1);
			    tiles = &tile_with_id;
			  }
			code = clist_change_tile(cdev, pcls, tiles,
						 (tcolors != 0 ? 1 :
						  dev->color_info.depth));
			if ( code < 0 )
			  return code;
			if ( phase_x != pcls->tile_phase.x ||
			     phase_y != pcls->tile_phase.y
			   )
			  { code = cmd_set_tile_phase(cdev, pcls, phase_x,
						      phase_y);
			    if ( code < 0 )
			      return code;
			  }
		      }
		  }
		/* Set the tile colors. */
		code = (tcolors != 0 ?
			cmd_set_tile_colors(cdev, pcls, tcolors[0],
					    tcolors[1]) :
			cmd_set_tile_colors(cdev, pcls, gx_no_color_index,
					    gx_no_color_index));
		if ( code < 0 )
		  return code;
	  }
	/* Set lop_enabled to -1 so that fill_rectangle / copy_* */
	/* won't attempt to set it to 0. */
	pcls->lop_enabled = -1;
	if ( scolors != 0 )
	  { if ( scolors[0] == scolors[1] )
	      code = clist_fill_rectangle(dev, x, y, width, height,
					  scolors[1]);
	    else
	      code = clist_copy_mono(dev, sdata, sourcex, sraster, id,
				     x, y, width, height,
				     scolors[0], scolors[1]);
	  }
	else
	  code = clist_copy_color(dev, sdata, sourcex, sraster, id,
				  x, y, width, height);
	pcls->lop_enabled = 1;
	if ( code < 0 )
	  return 0;
	END_RECT
	return 0;	  
}
