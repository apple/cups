/*
  Copyright 1993-2001 by Easy Software Products.
  Copyright 1991, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.

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

/*$Id: gxclist.c,v 1.7 2001/03/27 15:45:20 mike Exp $ */
/* Command list document- and page-level code. */
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gp.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxdevmem.h"		/* must precede gxcldev.h */
#include "gxcldev.h"
#include "gxclpath.h"
#include "gsparams.h"

/* Forward declarations of driver procedures */
private dev_proc_open_device(clist_open);
private dev_proc_output_page(clist_output_page);
private dev_proc_close_device(clist_close);
private dev_proc_get_band(clist_get_band);

/* In gxclrect.c */
extern dev_proc_fill_rectangle(clist_fill_rectangle);
extern dev_proc_copy_mono(clist_copy_mono);
extern dev_proc_copy_color(clist_copy_color);
extern dev_proc_copy_alpha(clist_copy_alpha);
extern dev_proc_strip_tile_rectangle(clist_strip_tile_rectangle);
extern dev_proc_strip_copy_rop(clist_strip_copy_rop);

/* In gxclpath.c */
extern dev_proc_fill_path(clist_fill_path);
extern dev_proc_stroke_path(clist_stroke_path);

/* In gxclimag.c */
extern dev_proc_fill_mask(clist_fill_mask);
extern dev_proc_begin_image(clist_begin_image);
extern dev_proc_begin_typed_image(clist_begin_typed_image);
extern dev_proc_create_compositor(clist_create_compositor);

/* In gxclread.c */
extern dev_proc_get_bits_rectangle(clist_get_bits_rectangle);

/* Other forward declarations */
private int clist_put_current_params(P1(gx_device_clist_writer *cldev));

/* The device procedures */
const gx_device_procs gs_clist_device_procs = {
    clist_open,
    gx_forward_get_initial_matrix,
    gx_default_sync_output,
    clist_output_page,
    clist_close,
    gx_forward_map_rgb_color,
    gx_forward_map_color_rgb,
    clist_fill_rectangle,
    gx_default_tile_rectangle,
    clist_copy_mono,
    clist_copy_color,
    gx_default_draw_line,
    gx_default_get_bits,
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
    clist_fill_path,
    clist_stroke_path,
    clist_fill_mask,
    gx_default_fill_trapezoid,
    gx_default_fill_parallelogram,
    gx_default_fill_triangle,
    gx_default_draw_thin_line,
    clist_begin_image,
    gx_default_image_data,
    gx_default_end_image,
    clist_strip_tile_rectangle,
    clist_strip_copy_rop,
    gx_forward_get_clipping_box,
    clist_begin_typed_image,
    clist_get_bits_rectangle,
    gx_forward_map_color_rgb_alpha,
    clist_create_compositor,
    gx_forward_get_hardware_params,
    gx_default_text_begin
};

/* ------ Define the command set and syntax ------ */

/* Initialization for imager state. */
/* The initial scale is arbitrary. */
const gs_imager_state clist_imager_state_initial =
{gs_imager_state_initial(300.0 / 72.0)};

/*
 * The buffer area (data, data_size) holds a bitmap cache when both writing
 * and reading.  The rest of the space is used for the command buffer and
 * band state bookkeeping when writing, and for the rendering buffer (image
 * device) when reading.  For the moment, we divide the space up
 * arbitrarily, except that we allocate less space for the bitmap cache if
 * the device doesn't need halftoning.
 *
 * All the routines for allocating tables in the buffer are idempotent, so
 * they can be used to check whether a given-size buffer is large enough.
 */

/*
 * Calculate the desired size for the tile cache.
 */
private uint
clist_tile_cache_size(const gx_device * target, uint data_size)
{
    uint bits_size =
    (data_size / 5) & -align_cached_bits_mod;	/* arbitrary */

    if (!gx_device_must_halftone(target)) {	/* No halftones -- cache holds only Patterns & characters. */
	bits_size -= bits_size >> 2;
    }
#define min_bits_size 1024
    if (bits_size < min_bits_size)
	bits_size = min_bits_size;
#undef min_bits_size
    return bits_size;
}

/*
 * Initialize the allocation for the tile cache.  Sets: tile_hash_mask,
 * tile_max_count, tile_table, chunk (structure), bits (structure).
 */
private int
clist_init_tile_cache(gx_device * dev, byte * init_data, ulong data_size)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    byte *data = init_data;
    uint bits_size = data_size;
    /*
     * Partition the bits area between the hash table and the actual
     * bitmaps.  The per-bitmap overhead is about 24 bytes; if the
     * average character size is 10 points, its bitmap takes about 24 +
     * 0.5 * 10/72 * xdpi * 10/72 * ydpi / 8 bytes (the 0.5 being a
     * fudge factor to account for characters being narrower than they
     * are tall), which gives us a guideline for the size of the hash
     * table.
     */
    uint avg_char_size =
    (uint) (dev->x_pixels_per_inch * dev->y_pixels_per_inch *
	    (0.5 * 10 / 72 * 10 / 72 / 8)) + 24;
    uint hc = bits_size / avg_char_size;
    uint hsize;

    while ((hc + 1) & hc)
	hc |= hc >> 1;		/* make mask (power of 2 - 1) */
    if (hc < 0xff)
	hc = 0xff;		/* make allowance for halftone tiles */
    else if (hc > 0xfff)
	hc = 0xfff;		/* cmd_op_set_tile_index has 12-bit operand */
    /* Make sure the tables will fit. */
    while (hc >= 3 && (hsize = (hc + 1) * sizeof(tile_hash)) >= bits_size)
	hc >>= 1;
    if (hc < 3)
	return_error(gs_error_rangecheck);
    cdev->tile_hash_mask = hc;
    cdev->tile_max_count = hc - (hc >> 2);
    cdev->tile_table = (tile_hash *) data;
    data += hsize;
    bits_size -= hsize;
    gx_bits_cache_chunk_init(&cdev->chunk, data, bits_size);
    gx_bits_cache_init(&cdev->bits, &cdev->chunk);
    return 0;
}

/*
 * Initialize the allocation for the bands.  Requires: target.  Sets:
 * page_band_height (=page_info.band_params.BandHeight), nbands.
 */
private int
clist_init_bands(gx_device * dev, uint data_size, int band_width,
		 int band_height)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    gx_device *target = cdev->target;
    int nbands;

    if (gdev_mem_data_size((gx_device_memory *) target, band_width,
			   band_height) > data_size
	)
	return_error(gs_error_rangecheck);
    cdev->page_band_height = band_height;
    nbands = (target->height + band_height - 1) / band_height;
    cdev->nbands = nbands;
#ifdef DEBUG
    if (gs_debug_c('l') | gs_debug_c(':'))
	dlprintf4("[:]width=%d, band_width=%d, band_height=%d, nbands=%d\n",
		  target->width, band_width, band_height, nbands);
#endif
    return 0;
}

/*
 * Initialize the allocation for the band states, which are used only
 * when writing.  Requires: nbands.  Sets: states, cbuf, cend.
 */
private int
clist_init_states(gx_device * dev, byte * init_data, uint data_size)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    ulong state_size = cdev->nbands * (ulong) sizeof(gx_clist_state);

    fprintf(stderr, "DEBUG: init_data = %p for cdev->states!\n", init_data);

    /*
     * The +100 in the next line is bogus, but we don't know what the
     * real check should be. We're effectively assuring that at least 100
     * bytes will be available to buffer command operands.
     */
    if (state_size + sizeof(cmd_prefix) + cmd_largest_size + 100 > data_size)
	return_error(gs_error_rangecheck);
    cdev->states = (gx_clist_state *) init_data;
    cdev->cbuf = init_data + state_size;
    cdev->cend = init_data + data_size;
    return 0;
}

/*
 * Initialize all the data allocations.  Requires: target.  Sets:
 * page_tile_cache_size, page_info.band_params.BandWidth,
 * page_info.band_params.BandBufferSpace, + see above.
 */
private int
clist_init_data(gx_device * dev, byte * init_data, uint data_size)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    gx_device *target = cdev->target;
    const int band_width =
    cdev->page_info.band_params.BandWidth =
    (cdev->band_params.BandWidth ? cdev->band_params.BandWidth :
     target->width);
    int band_height = cdev->band_params.BandHeight;
    const uint band_space =
    cdev->page_info.band_params.BandBufferSpace =
    (cdev->band_params.BandBufferSpace ?
     cdev->band_params.BandBufferSpace : data_size);
    byte *data = init_data;
    uint size = band_space;
    uint bits_size;
    int code;

    if (band_height) {		/*
				 * The band height is fixed, so the band buffer requirement
				 * is completely determined.
				 */
	uint band_data_size =
	gdev_mem_data_size((gx_device_memory *) target,
			   band_width, band_height);

	if (band_data_size >= band_space)
	    return_error(gs_error_rangecheck);
	bits_size = min(band_space - band_data_size, data_size >> 1);
	/**** MRS - make sure bits_size is 64-bit aligned for clist data!!! ****/
	bits_size = (bits_size + 7) & ~7;
    } else {			/*
				 * Choose the largest band height that will fit in the
				 * rendering-time buffer.
				 */
	bits_size = clist_tile_cache_size(target, band_space);
	bits_size = min(bits_size, data_size >> 1);
	/**** MRS - make sure bits_size is 64-bit aligned for clist data!!! ****/
	bits_size = (bits_size + 7) & ~7;
	band_height = gdev_mem_max_height((gx_device_memory *) target,
					  band_width,
					  band_space - bits_size);
	if (band_height == 0)
	    return_error(gs_error_rangecheck);
    }
    code = clist_init_tile_cache(dev, data, bits_size);
    if (code < 0)
	return code;
    cdev->page_tile_cache_size = bits_size;
    data += bits_size;
    size -= bits_size;
    code = clist_init_bands(dev, size, band_width, band_height);
    if (code < 0)
	return code;
    return clist_init_states(dev, data, data_size - bits_size);
}
/*
 * Reset the device state (for writing).  This routine requires only
 * data, data_size, and target to be set, and is idempotent.
 */
private int
clist_reset(gx_device * dev)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    int code = clist_init_data(dev, cdev->data, cdev->data_size);
    int nbands;

    if (code < 0)
	return (cdev->permanent_error = code);
    /* Now initialize the rest of the state. */
    cdev->permanent_error = 0;
    nbands = cdev->nbands;
    cdev->ymin = cdev->ymax = -1;	/* render_init not done yet */
    memset(cdev->tile_table, 0, (cdev->tile_hash_mask + 1) *
	   sizeof(*cdev->tile_table));
    cdev->cnext = cdev->cbuf;
    cdev->ccl = 0;
    cdev->band_range_list.head = cdev->band_range_list.tail = 0;
    cdev->band_range_min = 0;
    cdev->band_range_max = nbands - 1;
    {
	int band;
	gx_clist_state *states = cdev->states;

	for (band = 0; band < nbands; band++, states++) {
	    static const gx_clist_state cls_initial =
	    {cls_initial_values};

	    *states = cls_initial;
	}
    }
    /*
     * Round up the size of the per-tile band mask so that the bits,
     * which follow it, stay aligned.
     */
    cdev->tile_band_mask_size =
	((nbands + (align_bitmap_mod * 8 - 1)) >> 3) &
	~(align_bitmap_mod - 1);
    /*
     * Initialize the all-band parameters to impossible values,
     * to force them to be written the first time they are used.
     */
    memset(&cdev->tile_params, 0, sizeof(cdev->tile_params));
    cdev->tile_depth = 0;
    cdev->tile_known_min = nbands;
    cdev->tile_known_max = -1;
    cdev->imager_state = clist_imager_state_initial;
    cdev->clip_path = NULL;
    cdev->clip_path_id = gs_no_id;
    cdev->color_space = 0;
    {
	int i;

	for (i = 0; i < countof(cdev->transfer_ids); ++i)
	    cdev->transfer_ids[i] = gs_no_id;
    }
    cdev->black_generation_id = gs_no_id;
    cdev->undercolor_removal_id = gs_no_id;
    cdev->device_halftone_id = gs_no_id;
    return 0;
}
/*
 * Initialize the device state (for writing).  This routine requires only
 * data, data_size, and target to be set, and is idempotent.
 */
private int
clist_init(gx_device * dev)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    int code = clist_reset(dev);

    if (code >= 0) {
	cdev->image_enum_id = gs_no_id;
	cdev->error_is_retryable = 0;
	cdev->driver_call_nesting = 0;
	cdev->ignore_lo_mem_warnings = 0;
    }
    return code;
}

/* (Re)init open band files for output (set block size, etc). */
private int	/* ret 0 ok, -ve error code */
clist_reinit_output_file(gx_device *dev)
{    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    int code = 0;

	/* bfile needs to guarantee cmd_blocks for: 1 band range, nbands */
	/*  & terminating entry */
	int b_block = sizeof(cmd_block) * (cdev->nbands + 2);

	/* cfile needs to guarantee one writer buffer */
	/*  + one end_clip cmd (if during image's clip path setup) */
	/*  + an end_image cmd for each band (if during image) */
	/*  + end_cmds for each band and one band range */
	int c_block
	 = cdev->cend - cdev->cbuf + 2 + cdev->nbands * 2 + (cdev->nbands + 1);

	/* All this is for partial page rendering's benefit, do only */
	/* if partial page rendering is available */
	if ( clist_test_VMerror_recoverable(cdev) )
	  { if (cdev->page_bfile != 0)
	      code = clist_set_memory_warning(cdev->page_bfile, b_block);
	    if (code >= 0 && cdev->page_cfile != 0)
	      code = clist_set_memory_warning(cdev->page_cfile, c_block);
	  }
	return code;
}

/* Write out the current parameters that must be at the head of each page */
/* if async rendering is in effect */
private int
clist_emit_page_header(gx_device *dev)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    int code = 0;

    if ( (cdev->disable_mask & clist_disable_pass_thru_params) )
	{ do
	    if (  ( code = clist_put_current_params(cdev) ) >= 0  )
	        break;
	  while (  ( code = clist_VMerror_recover(cdev, code) ) < 0  );
	cdev->permanent_error = (code < 0) ? code : 0;
	if (cdev->permanent_error < 0)
	    cdev->error_is_retryable = 0;
	}
    return code;
}

/* Open the device's bandfiles */
private int
clist_open_output_file(gx_device *dev)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    char fmode[4];
    int code;

    if (cdev->do_not_open_or_close_bandfiles)
	return 0; /* external bandfile open/close managed externally */
    cdev->page_cfile = 0;	/* in case of failure */
    cdev->page_bfile = 0;	/* ditto */
    code = clist_init(dev);
    if (code < 0)
	return code;
    strcpy(fmode, "w+");
    strcat(fmode, gp_fmode_binary_suffix);
    cdev->page_cfname[0] = 0;	/* create a new file */
    cdev->page_bfname[0] = 0;	/* ditto */
    cdev->page_bfile_end_pos = 0;
    if ((code = clist_fopen(cdev->page_cfname, fmode, &cdev->page_cfile,
			    cdev->bandlist_memory, cdev->bandlist_memory,
			    true)) < 0 ||
	(code = clist_fopen(cdev->page_bfname, fmode, &cdev->page_bfile,
			    cdev->bandlist_memory, cdev->bandlist_memory,
			    true)) < 0 ||
	(code = clist_reinit_output_file(dev)) < 0
	) {
	clist_close_output_file(dev);
	cdev->permanent_error = code;
	cdev->error_is_retryable = 0;
    }
    return code;
}

/* Close the device by freeing the temporary files. */
/* Note that this does not deallocate the buffer. */
int
clist_close_output_file(gx_device *dev)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;

    if (cdev->page_cfile != NULL) {
	clist_fclose(cdev->page_cfile, cdev->page_cfname, true);
	cdev->page_cfile = NULL;
    }
    if (cdev->page_bfile != NULL) {
	clist_fclose(cdev->page_bfile, cdev->page_bfname, true);
	cdev->page_bfile = NULL;
    }
    return 0;
}

/* Open the device by initializing the device state and opening the */
/* scratch files. */
private int
clist_open(gx_device *dev)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    int code;

    cdev->permanent_error = 0;
    code = clist_init(dev);
    if (code < 0)
	return code;
    code = clist_open_output_file(dev);
    if ( code >= 0)
	code = clist_emit_page_header(dev);
    return code;
}

private int
clist_close(gx_device *dev)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;

    if (cdev->do_not_open_or_close_bandfiles)
	return 0;	
    return clist_close_output_file(dev);
}

/* The output_page procedure should never be called! */
private int
clist_output_page(gx_device * dev, int num_copies, int flush)
{
    return_error(gs_error_Fatal);
}

/* Reset (or prepare to append to) the command list after printing a page. */
int
clist_finish_page(gx_device *dev, bool flush)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    int code;

    if (flush) {
	if (cdev->page_cfile != 0)
	    clist_rewind(cdev->page_cfile, true, cdev->page_cfname);
	if (cdev->page_bfile != 0)
	    clist_rewind(cdev->page_bfile, true, cdev->page_bfname);
	cdev->page_bfile_end_pos = 0;
    } else {
	if (cdev->page_cfile != 0)
	    clist_fseek(cdev->page_cfile, 0L, SEEK_END, cdev->page_cfname);
	if (cdev->page_bfile != 0)
	    clist_fseek(cdev->page_bfile, 0L, SEEK_END, cdev->page_bfname);
    }
    code = clist_init(dev);		/* reinitialize */
    if (code >= 0)
	code = clist_reinit_output_file(dev);
    if (code >= 0)
	code = clist_emit_page_header(dev);

    return code;
}

/* ------ Writing ------ */

/* End a page by flushing the buffer and terminating the command list. */
int	/* ret 0 all-ok, -ve error code, or +1 ok w/low-mem warning */
clist_end_page(gx_device_clist_writer * cldev)
{
    int code = cmd_write_buffer(cldev, cmd_opv_end_page);
    cmd_block cb;
    int ecode = 0;

    if (code >= 0) {
	/*
	 * Write the terminating entry in the block file.
	 * Note that because of copypage, there may be many such entries.
	 */
	cb.band_min = cb.band_max = cmd_band_end;
	cb.pos = (cldev->page_cfile == 0 ? 0 : clist_ftell(cldev->page_cfile));
	clist_fwrite_chars(&cb, sizeof(cb), cldev->page_bfile);
	cldev->page_bfile_end_pos = clist_ftell(cldev->page_bfile);
    }
    if (code >= 0) {
	ecode |= code;
	cldev->page_bfile_end_pos = clist_ftell(cldev->page_bfile);
    }
    if (code < 0)
	ecode = code;

    /* Reset warning margin to 0 to release reserve memory if mem files */
    if (cldev->page_bfile != 0)
	clist_set_memory_warning(cldev->page_bfile, 0);
    if (cldev->page_cfile != 0)
	clist_set_memory_warning(cldev->page_cfile, 0);

#ifdef DEBUG
    if (gs_debug_c('l') | gs_debug_c(':'))
	dlprintf2("[:]clist_end_page at cfile=%ld, bfile=%ld\n",
		  cb.pos, cldev->page_bfile_end_pos);
#endif
    return 0;
}

/* Recover recoverable VM error if possible without flushing */
int	/* ret -ve err, >= 0 if recovered w/# = cnt pages left in page queue */
clist_VMerror_recover(gx_device_clist_writer *cldev,
		      int old_error_code)
{
    int code = old_error_code;
    int pages_remain;

    if (!clist_test_VMerror_recoverable(cldev) ||
	!cldev->error_is_retryable ||
	old_error_code != gs_error_VMerror
	)
	return old_error_code;

    /* Do some rendering, return if enough memory is now free */
    do {
	pages_remain =
	    (*cldev->free_up_bandlist_memory)( (gx_device *)cldev, false );
	if (pages_remain < 0) {
	    code = pages_remain;	/* abort, error or interrupt req */
	    break;
	}
	if (clist_reinit_output_file( (gx_device *)cldev ) == 0) {
	    code = pages_remain;	/* got enough memory to continue */
	    break;
	}
    } while (pages_remain);

    if_debug1('L', "[L]soft flush of command list, status: %d\n", code);
    return code;
}

/* If recoverable VM error, flush & try to recover it */
int	/* ret 0 ok, else -ve error */
clist_VMerror_recover_flush(gx_device_clist_writer *cldev,
			    int old_error_code)
{
    int free_code = 0;
    int reset_code = 0;
    int code;

    /* If the device has the ability to render partial pages, flush
     * out the bandlist, and reset the writing state. Then, get the
     * device to render this band. When done, see if there's now enough
     * memory to satisfy the minimum low-memory guarantees. If not, 
     * get the device to render some more. If there's nothing left to
     * render & still insufficient memory, declare an error condition.
     */
    if (!clist_test_VMerror_recoverable(cldev) ||
	old_error_code != gs_error_VMerror
	)
	return old_error_code;	/* sorry, don't have any means to recover this error */
    free_code = (*cldev->free_up_bandlist_memory)( (gx_device *)cldev, true );

    /* Reset the state of bands to "don't know anything" */
    reset_code = clist_reset( (gx_device *)cldev );
    if (reset_code >= 0)
	reset_code = clist_open_output_file( (gx_device *)cldev );
    if ( reset_code >= 0 &&
	 (cldev->disable_mask & clist_disable_pass_thru_params)
	 )
	reset_code = clist_put_current_params(cldev);
    if (reset_code < 0) {
	cldev->permanent_error = reset_code;
	cldev->error_is_retryable = 0;
    }
 
    code = (reset_code < 0 ? reset_code : free_code < 0 ? old_error_code : 0);
    if_debug1('L', "[L]hard flush of command list, status: %d\n", code);
    return code;
}

/* Write the target device's current parameter list */
private int	/* ret 0 all ok, -ve error */
clist_put_current_params(gx_device_clist_writer *cldev)
{
    gx_device *target = cldev->target;
    gs_c_param_list param_list;
    int code;

    /*
     * If a put_params call fails, the device will be left in a closed
     * state, but higher-level code won't notice this fact.  We flag this by
     * setting permanent_error, which prevents writing to the command list.
     */

    if (cldev->permanent_error)
	return cldev->permanent_error;
    gs_c_param_list_write(&param_list, cldev->memory);
    code = (*dev_proc(target, get_params))
	(target, (gs_param_list *)&param_list);
    if (code >= 0) {
	gs_c_param_list_read(&param_list);
	code = cmd_put_params( cldev, (gs_param_list *)&param_list );
    }
    gs_c_param_list_release(&param_list);

    return code;
}

/* ---------------- Driver interface ---------------- */

private int
clist_get_band(gx_device * dev, int y, int *band_start)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    int band_height = cdev->page_band_height;
    int start;

    if (y < 0)
	y = 0;
    else if (y >= dev->height)
	y = dev->height;
    *band_start = start = y - y % band_height;
    return min(dev->height - start, band_height);
}
