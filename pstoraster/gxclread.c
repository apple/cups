/*
  Copyright 1993-1999 by Easy Software Products.
  Copyright (C) 1991, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxclread.c,v 1.5 2000/03/08 23:14:54 mike Exp $ */
/* Command list reading for Ghostscript. */
#include "memory_.h"
#include "gx.h"
#include "gp.h"			/* for gp_fmode_rb */
#include "gpcheck.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gscoord.h"		/* requires gsmatrix.h */
#include "gsdevice.h"		/* for gs_deviceinitialmatrix */
#include "gxdevmem.h"		/* must precede gxcldev.h */
#include "gxcldev.h"
#include "gxgetbit.h"
#include "gxhttile.h"
#include "gdevht.h"
#include "stream.h"
#include "strimpl.h"

/* ------ Band file reading stream ------ */

/*
 * To separate banding per se from command list interpretation,
 * we make the command list interpreter simply read from a stream.
 * When we are actually doing banding, the stream filters the band file
 * and only passes through the commands for the current band (or band
 * ranges that include the current band).
 */
typedef struct stream_band_read_state_s {
    stream_state_common;
    gx_band_page_info page_info;
    int band;
    uint left;			/* amount of data left in this run */
    cmd_block b_this;
} stream_band_read_state;

private int
s_band_read_init(stream_state * st)
{
    stream_band_read_state *const ss = (stream_band_read_state *) st;

    ss->left = 0;
    ss->b_this.band_min = 0;
    ss->b_this.band_max = 0;
    ss->b_this.pos = 0;
    clist_rewind(ss->page_bfile, false, ss->page_bfname);
    return 0;
}

private int
s_band_read_process(stream_state * st, stream_cursor_read * ignore_pr,
		    stream_cursor_write * pw, bool last)
{
    stream_band_read_state *const ss = (stream_band_read_state *) st;
    register byte *q = pw->ptr;
    byte *wlimit = pw->limit;
    clist_file_ptr cfile = ss->page_cfile;
    clist_file_ptr bfile = ss->page_bfile;
    uint left = ss->left;
    int status = 1;
    uint count;

    while ((count = wlimit - q) != 0) {
	if (left) {		/* Read more data for the current run. */
	    if (count > left)
		count = left;
	    clist_fread_chars(q + 1, count, cfile);
	    if (clist_ferror_code(cfile) < 0) {
		status = ERRC;
		break;
	    }
	    q += count;
	    left -= count;
	    process_interrupts();
	    continue;
	}
      rb:			/* Scan for the next run for this band (or a band range */
	/* that includes the current band). */
	if (ss->b_this.band_min == cmd_band_end &&
	    clist_ftell(bfile) == ss->page_bfile_end_pos
	    ) {
	    status = EOFC;
	    break;
	} {
	    int bmin = ss->b_this.band_min;
	    int bmax = ss->b_this.band_max;
	    long pos = ss->b_this.pos;

	    clist_fread_chars(&ss->b_this, sizeof(ss->b_this), bfile);
	    if (!(ss->band >= bmin && ss->band <= bmax))
		goto rb;
	    clist_fseek(cfile, pos, SEEK_SET, ss->page_cfname);
	    left = (uint) (ss->b_this.pos - pos);
	    if_debug5('l', "[l]reading for bands (%d,%d) at bfile %ld, cfile %ld, length %u\n",
		      bmin, bmax,
		      clist_ftell(bfile) - 2 * sizeof(ss->b_this),
		      pos, left);
	}
    }
    pw->ptr = q;
    ss->left = left;
    return status;
}

/* Stream template */
const stream_template s_band_read_template = {
    &st_stream_state, s_band_read_init, s_band_read_process, 1, cbuf_size
};


/* ------ Reading/rendering ------ */

/* Forward references */

private int clist_render_init(P1(gx_device_clist *));
private int clist_playback_file_band(P7(clist_playback_action action,
					gx_device_clist_reader *cdev,
					gx_band_page_info *page_info,
					gx_device *target,
					int band, int x0, int y0));
private int clist_rasterize_lines(P6(gx_device *dev, int y, int lineCount,
				     byte *data_in, gx_device_memory *mdev,
				     int *pmy));

/*
 * Do device setup from params stored in command list. This is only for
 * async rendering & assumes that the first command in every command list
 * is a put_params command which sets all space-related parameters to the
 * value they will have for the duration of that command list.
 */
int
clist_setup_params(gx_device *dev)
{
    gx_device_clist_reader * const crdev =
	&((gx_device_clist *)dev)->reader;
    int code = clist_render_init((gx_device_clist *)dev);
    if ( code < 0 )
	return code;

    code = clist_playback_file_band(playback_action_setup,
				    crdev, &crdev->page_info, 0, 0, 0, 0);

    /* put_params may have reinitialized device into a writer */
    clist_render_init((gx_device_clist *)dev);

    return code;
}

/* Find out where the band buffer for a given line is going to fall on the */
/* next call to get_bits. */
/****** THIS IS WRONG: IT DUPLICATES CODE INSIDE make_buffer_device
 ****** AND ASSUMES THAT make_buffer_device JUST SETS UP A MEMORY DEVICE.
 ******/
int	/* rets # lines from y till end of buffer, or -ve error code */
clist_locate_overlay_buffer(gx_device_printer *pdev, int y, byte **data)
{
    gx_device_clist_reader * const crdev =
	&((gx_device_clist *)pdev)->reader;
    gx_device * const dev = (gx_device *)pdev;
    gx_device *target = crdev->target;

    uint raster = gdev_mem_raster(target);
    byte *mdata = crdev->data + crdev->page_tile_cache_size;
    int band_height = crdev->page_band_height;
    int band = y / band_height;
    int band_begin_line = band * band_height;
    int bytes_from_band_begin_to_line = (y - band_begin_line) * raster;
    int band_end_line = band_begin_line + band_height;

    if (band_end_line > dev->height)
	band_end_line = dev->height;

    /* Make sure device will rasterize on next get_bits or get_overlay_bits */
    if ( crdev->ymin >= 0 )
	crdev->ymin = crdev->ymax = 0;

    *data = mdata + bytes_from_band_begin_to_line;
    return band_end_line - y;	/* # lines remaining in this band */
}

/* Do more rendering to a client-supplied memory image, return results */
int
clist_get_overlay_bits(gx_device_printer *pdev, int y, int line_count,
		       byte *data)
{
    gx_device_clist_reader * const crdev =
	&((gx_device_clist *)pdev)->reader;
    byte *data_orig = data;
    gx_device * const dev = (gx_device *)pdev;
    gx_device *target = crdev->target;
    uint raster = gdev_mem_raster(target);
    int lines_left = line_count;

    /* May have to render more than once to cover requested line range */
    while (lines_left > 0) {
	gx_device_memory mdev;
	int my;
	byte *data_transformed;
	int line_count_rasterized =
	    clist_rasterize_lines(dev, y, lines_left, data_orig, &mdev, &my);
	uint byte_count_rasterized = raster * line_count_rasterized;

	if (line_count_rasterized < 0)
	    return line_count_rasterized;
	data_transformed = mdev.base + raster * my;
	if (data_orig != data_transformed)
	    memcpy(data_orig, data_transformed, byte_count_rasterized);
	data_orig += byte_count_rasterized;
	lines_left -= line_count_rasterized;
    }
    return 0;
}

/* Copy a rasterized rectangle to the client, rasterizing if needed. */
int
clist_get_bits_rectangle(gx_device *dev, const gs_int_rect * prect,
			 gs_get_bits_params_t *params, gs_int_rect **unread)
{
    gs_get_bits_options_t options = params->options;
    int y = prect->p.y;
    int end_y = prect->q.y;
    int line_count = end_y - y;
    gs_int_rect band_rect;
    int lines_rasterized;
    gx_device_memory mdev;
    int my;
    int code;

    if (prect->p.x < 0 || prect->q.x > dev->width ||
	y < 0 || end_y > dev->height
	)
	return_error(gs_error_rangecheck);
    if (line_count <= 0 || prect->p.x >= prect->q.x)
	return 0;
    code = clist_rasterize_lines(dev, y, line_count, NULL, &mdev, &my);
    if (code < 0)
	return code;
    lines_rasterized = min(code, line_count);
    /* Return as much of the rectangle as falls within the rasterized lines. */
    band_rect = *prect;
    band_rect.p.y = my;
    band_rect.q.y = my + lines_rasterized;
    code = (*dev_proc(&mdev, get_bits_rectangle))
	((gx_device *)&mdev, &band_rect, params, unread);
    if (code < 0 || lines_rasterized == line_count)
	return code;
    /*
     * We'll have to return the rectangle in pieces.  Force GB_RETURN_COPY
     * rather than GB_RETURN_POINTER, and require all subsequent pieces to
     * use the same values as the first piece for all of the other format
     * options.  If copying isn't allowed, or if there are any unread
     * rectangles, punt.
     */
    if (!(options & GB_RETURN_COPY) || code > 0)
	return gx_default_get_bits_rectangle(dev, prect, params, unread);
    options = params->options;
    if (!(options & GB_RETURN_COPY)) {
	/* Redo the first piece with copying. */
	params->options = options =
	    (params->options & ~GB_RETURN_ALL) | GB_RETURN_COPY;
	lines_rasterized = 0;
    }
    {
	gs_get_bits_params_t band_params;
	int num_planes =
	    (options & GB_PACKING_CHUNKY ? 1 :
	     options & GB_PACKING_PLANAR ? mdev.color_info.num_components :
	     options & GB_PACKING_BIT_PLANAR ? mdev.color_info.depth :
	     0 /****** NOT POSSIBLE ******/);
	uint raster = gdev_mem_raster(&mdev);

	band_params = *params;
	while ((y += lines_rasterized) < end_y) {
	    int i;

	    /* Increment data pointers by lines_rasterized. */
	    for (i = 0; i < num_planes; ++i)
		band_params.data[i] += raster * lines_rasterized;
	    line_count = end_y - y;
	    code = clist_rasterize_lines(dev, y, line_count, NULL, &mdev, &my);
	    if (code < 0)
		return code;
	    lines_rasterized = min(code, line_count);
	    band_rect.p.y = my;
	    band_rect.q.y = my + lines_rasterized;
	    code = (*dev_proc(&mdev, get_bits_rectangle))
		((gx_device *)&mdev, &band_rect, &band_params, unread);
	    if (code < 0)
		return code;
	    params->options = options = band_params.options;
	    if (lines_rasterized == line_count)
		return code;
	}
    }
    return 0;
}

/* Copy scan lines to the client.  This is where rendering gets done. */
/* Processes min(requested # lines, # lines available thru end of band) */
private int	/* returns -ve error code, or # scan lines copied */
clist_rasterize_lines(gx_device *dev, int y, int line_count, byte *data_in,
		      gx_device_memory *mdev, int *pmy)
{
    gx_device_clist_reader * const crdev =
	&((gx_device_clist *)dev)->reader;
    gx_device *target = crdev->target;
    uint raster = gx_device_raster(target, true);
    byte *mdata = crdev->data + crdev->page_tile_cache_size;
    gx_device *tdev = (gx_device *)mdev;
    int code;

    /* Initialize for rendering if we haven't done so yet. */
    if (crdev->ymin < 0) {
	code = clist_end_page(&((gx_device_clist *)crdev)->writer);
	if ( code < 0 )
	    return code;
	code = clist_render_init((gx_device_clist *)dev);
	if ( code < 0 )
	    return code;
#if 0				/* **************** */
    gx_device_ht hdev;

	code = clist_render_init((gx_device_clist *) dev, &hdev);
	if (code < 0)
	    return code;
	if (code != 0) {
	    hdev.target = tdev;
	    tdev = (gx_device *) & hdev;
	}
#endif				/* **************** */
    }

    /* Render a band if necessary, and copy it incrementally. */
    code = (*crdev->make_buffer_device)(mdev, target, 0, true);
    if (code < 0)
	return code;
    mdev->width = target->width;
    mdev->raster = raster;
    if (data_in || !(y >= crdev->ymin && y < crdev->ymax)) {
	const gx_placed_page *ppages = crdev->pages;
	int num_pages = crdev->num_pages;
	gx_saved_page current_page;
	gx_placed_page placed_page;
	int band_height = crdev->page_band_height;
	int band = y / band_height;
	int band_begin_line = band * band_height;
	int band_end_line = band_begin_line + band_height;
	int i;

	if (band_end_line > dev->height)
	    band_end_line = dev->height;
	/* Clip line_count to current band */
	if (line_count > band_end_line - y)
	    line_count = band_end_line - y;

	if (y < 0 || y > dev->height)
	    return_error(gs_error_rangecheck);
	/****** QUESTIONABLE, BUT BETTER THAN OMITTING ******/
	mdev->color_info = dev->color_info;
	mdev->base = mdata;
	/*
	 * The matrix in the memory device is irrelevant,
	 * because all we do with the device is call the device-level
	 * output procedures, but we may as well set it to
	 * something halfway reasonable.
	 */
	gs_deviceinitialmatrix(target, &mdev->initial_matrix);
	mdev->height = band_height;
	(*dev_proc(mdev, open_device))((gx_device *)mdev);
	if_debug1('l', "[l]rendering band %d\n", band);
	/*
	 * Unfortunately, there is currently no way to get a mem
	 * device to rasterize into a given memory space, since
	 * a mem device's memory space must also contain internal
	 * structures.
	 */
	if (data_in)
	    memcpy(mdev->base + (y - band_begin_line) * raster,
		   data_in, line_count * raster);
	/*
	 * If we aren't rendering saved pages, do the current one.
	 * Note that this is the only case in which we may encounter
	 * a gx_saved_page with non-zero cfile or bfile.
	 */
	if (ppages == 0) {
	    current_page.info = crdev->page_info;
	    placed_page.page = &current_page;
	    placed_page.offset.x = placed_page.offset.y = 0;
	    ppages = &placed_page;
	    num_pages = 1;
	}
	for (i = 0; i < num_pages; ++i) {
	    const gx_placed_page *ppage = &ppages[i];

	    code = clist_playback_file_band(playback_action_render,
					    crdev, &ppage->page->info, tdev,
					    band, -ppage->offset.x,
					    band * mdev->height);
	    if ( code < 0 )
		break;
	}
	/* Reset the band boundaries now, so that we don't get */
	/* an infinite loop. */
	crdev->ymin = band_begin_line;
	crdev->ymax = band_end_line;
	if (code < 0)
	    return code;
	*pmy = y - crdev->ymin;
    } else {
	/* Just fill in enough of the memory device to access the
	 * already-rasterized scan lines; in particular, only set up scan
	 * line pointers for the requested Y range.
	 */
	mdev->base = mdata + (y - crdev->ymin) * raster;
	mdev->height = crdev->ymax - y;
	gdev_mem_open_scan_lines(mdev, min(line_count, mdev->height));
	*pmy = 0;
    }
    return (line_count > crdev->ymax - y ? crdev->ymax - y : line_count);
}

/* Initialize for reading. */
private int
clist_render_init(gx_device_clist *dev /******, gx_device_ht *hdev ******/)
{
    gx_device_clist_reader * const crdev = &dev->reader;

    crdev->ymin = crdev->ymax = 0;
    /* For normal rasterizing, pages and num_pages are zero. */
    crdev->pages = 0;
    crdev->num_pages = 0;
    return 0;
}

/* Playback the band file, taking the indicated action w/ its contents. */
private int
clist_playback_file_band(clist_playback_action action, 
			 gx_device_clist_reader *cdev,
			 gx_band_page_info *page_info, gx_device *target,
			 int band, int x0, int y0)
{
    int code = 0;
    int opened_bfile = 0;
    int opened_cfile = 0;

    /* We have to pick some allocator for rendering.... */
    gs_memory_t *mem =
	(cdev->memory != 0 ? cdev->memory : &gs_memory_default);
 
    /* setup stream */
    stream_band_read_state rs;
    rs.template = &s_band_read_template;
    rs.memory = 0;
    rs.band = band;
    rs.page_info = *page_info;

    /* If this is a saved page, open the files. */
    if (rs.page_cfile == 0) {
	code = clist_fopen(rs.page_cfname,
			   gp_fmode_rb, &rs.page_cfile, cdev->bandlist_memory,
			   cdev->bandlist_memory, true);
	opened_cfile = (code >= 0);
    }
    if (rs.page_bfile == 0 && code >= 0) {
	code = clist_fopen(rs.page_bfname,
			   gp_fmode_rb, &rs.page_bfile, cdev->bandlist_memory,
			   cdev->bandlist_memory, false);
	opened_bfile = (code >= 0);
    }
    if (rs.page_cfile != 0 && rs.page_bfile != 0) {
	stream s;
	byte sbuf[cbuf_size];
	static const stream_procs no_procs = {
	    s_std_noavailable, s_std_noseek, s_std_read_reset,
	    s_std_read_flush, s_std_close, s_band_read_process
	};

	s_band_read_init((stream_state *)&rs);
	s_std_init(&s, sbuf, cbuf_size, &no_procs, s_mode_read);
	s.foreign = 1;
	s.state = (stream_state *)&rs;
	code = clist_playback_band(action, cdev, &s, target, -x0, y0, mem);
    }

    /* Close the files if we just opened them. */
    if (opened_bfile && rs.page_bfile != 0)
	clist_fclose(rs.page_bfile, rs.page_bfname, false);
    if (opened_cfile && rs.page_cfile != 0)
	clist_fclose(rs.page_cfile, rs.page_cfname, false);

    return code;
}
