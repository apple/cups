/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxclip.c,v 1.1 2000/03/08 23:14:52 mike Exp $ */
/* Implementation of (path-based) clipping */
#include "gx.h"
#include "gxdevice.h"
#include "gxclip.h"
#include "gzpath.h"
#include "gzcpath.h"

/* Define whether to look for vertical clipping regions. */
#define CHECK_VERTICAL_CLIPPING

/* ------ Rectangle list clipper ------ */

/* Device for clipping with a region. */
/* We forward non-drawing operations, but we must be sure to intercept */
/* all drawing operations. */
private dev_proc_open_device(clip_open);
private dev_proc_fill_rectangle(clip_fill_rectangle);
private dev_proc_copy_mono(clip_copy_mono);
private dev_proc_copy_color(clip_copy_color);
private dev_proc_copy_alpha(clip_copy_alpha);
private dev_proc_fill_mask(clip_fill_mask);
private dev_proc_strip_tile_rectangle(clip_strip_tile_rectangle);
private dev_proc_strip_copy_rop(clip_strip_copy_rop);
private dev_proc_get_clipping_box(clip_get_clipping_box);
private dev_proc_get_bits_rectangle(clip_get_bits_rectangle);

/* The device descriptor. */
private const gx_device_clip gs_clip_device =
{std_device_std_body(gx_device_clip, 0, "clipper",
		     0, 0, 1, 1),
 {clip_open,
  gx_forward_get_initial_matrix,
  gx_default_sync_output,
  gx_default_output_page,
  gx_default_close_device,
  gx_forward_map_rgb_color,
  gx_forward_map_color_rgb,
  clip_fill_rectangle,
  gx_default_tile_rectangle,
  clip_copy_mono,
  clip_copy_color,
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
  clip_copy_alpha,
  gx_forward_get_band,
  gx_default_copy_rop,
  gx_default_fill_path,
  gx_default_stroke_path,
  clip_fill_mask,
  gx_default_fill_trapezoid,
  gx_default_fill_parallelogram,
  gx_default_fill_triangle,
  gx_default_draw_thin_line,
  gx_default_begin_image,
  gx_default_image_data,
  gx_default_end_image,
  clip_strip_tile_rectangle,
  clip_strip_copy_rop,
  clip_get_clipping_box,
  gx_default_begin_typed_image,
  clip_get_bits_rectangle,
  gx_forward_map_color_rgb_alpha,
  gx_no_create_compositor,
  gx_forward_get_hardware_params,
  gx_default_text_begin
 }
};

/* Make a clipping device. */
void
gx_make_clip_translate_device(gx_device_clip * dev, void *container,
			      const gx_clip_list * list, int tx, int ty)
{
    gx_device_init((gx_device *) dev, (gx_device *) & gs_clip_device,
		   NULL, true);
    dev->list = *list;
    dev->translation.x = tx;
    dev->translation.y = ty;
}
void
gx_make_clip_path_device(gx_device_clip * dev, const gx_clip_path * pcpath)
{
    gx_make_clip_device(dev, NULL, gx_cpath_list(pcpath));
}

/* Define debugging statistics for the clipping loops. */
#ifdef DEBUG
struct stats_clip_s {
    long
         loops, in, down, up, x, no_x;
} stats_clip;
private uint clip_interval = 10000;

# define INCR(v) (++(stats_clip.v))
# define INCR_THEN(v, e) (INCR(v), (e))
#else
# define INCR(v) DO_NOTHING
# define INCR_THEN(v, e) (e)
#endif

/*
 * Enumerate the rectangles of the x,w,y,h argument that fall within
 * the clipping region.
 */
private int
clip_enumerate(gx_device_clip * rdev,
	       int (*process) (P5(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)),
	       clip_callback_data_t * pccd)
{
    gx_clip_rect *rptr = rdev->current;		/* const within algorithm */
    const int x = pccd->x, y = pccd->y;
    const int xe = x + pccd->w, ye = y + pccd->h;
    int xc, xec, yc, yec, yep;
    int code;

#ifdef DEBUG
    if (INCR(loops) % clip_interval == 0)
	if_debug6('q',
		  "[q]loops=%ld in=%ld down=%ld up=%ld x=%ld no_x=%ld\n", \
		  stats_clip.loops, stats_clip.in,
		  stats_clip.down, stats_clip.up,
		  stats_clip.x, stats_clip.no_x);
#endif
    if (pccd->w <= 0 || pccd->h <= 0)
	return 0;
    /* Check for the region being entirely within the current rectangle. */
    if (!rdev->list.outside) {
	if (y >= rptr->ymin && ye <= rptr->ymax &&
	    x >= rptr->xmin && xe <= rptr->xmax
	    ) {
	    return INCR_THEN(in, (*process) (pccd, x, y, xe, ye));
	}
    }
    /*
     * Warp the cursor forward or backward to the first rectangle row
     * that could include a given y value.  Assumes rptr is set, and
     * updates it.  Specifically, after this loop, either rptr == 0 (if
     * the y value is greater than all y values in the list), or y <
     * rptr->ymax and either rptr->prev == 0 or y >= rptr->prev->ymax.
     * Note that y <= rptr->ymin is possible.
     *
     * In the first case below, the while loop is safe because if there
     * is more than one rectangle, there is a 'stopper' at the end of
     * the list.
     */
    if (y >= rptr->ymax) {
	if ((rptr = rptr->next) != 0)
	    while (INCR_THEN(up, y >= rptr->ymax))
		rptr = rptr->next;
    } else
	while (rptr->prev != 0 && y < rptr->prev->ymax)
	    INCR_THEN(down, rptr = rptr->prev);
    if (rptr == 0 || (yc = rptr->ymin) >= ye) {
	if (rdev->list.count > 1)
	    rdev->current =
		(rptr != 0 ? rptr :
		 y >= rdev->current->ymax ? rdev->list.tail :
		 rdev->list.head);
	if (rdev->list.outside) {
	    return (*process) (pccd, x, y, xe, ye);
	} else
	    return 0;
    }
    rdev->current = rptr;
    if (yc < y)
	yc = y;
    if (rdev->list.outside) {
	for (yep = y;;) {
	    const int ymax = rptr->ymax;

	    xc = x;
	    if (yc > yep) {
		yec = yc, yc = yep;
		xec = xe;
		code = (*process) (pccd, xc, yc, xec, yec);
		if (code < 0)
		    return code;
		yc = yec;
	    }
	    yec = min(ymax, ye);
	    do {
		xec = rptr->xmin;
		if (xec > xc) {
		    if (xec > xe)
			xec = xe;
		    code = (*process) (pccd, xc, yc, xec, yec);
		    if (code < 0)
			return code;
		    xc = rptr->xmax;
		    if (xc >= xe)
			xc = max_int;
		} else {
		    xec = rptr->xmax;
		    if (xec > xc)
			xc = xec;
		}
	    }
	    while ((rptr = rptr->next) != 0 && rptr->ymax == ymax);
	    if (xc < xe) {
		xec = xe;
		code = (*process) (pccd, xc, yc, xec, yec);
		if (code < 0)
		    return code;
	    }
	    yep = yec;
	    if (rptr == 0 || (yc = rptr->ymin) >= ye)
		break;
	}
	if (yep < ye) {
	    xc = x, xec = xe, yc = yep, yec = ye;
	    code = (*process) (pccd, xc, yc, xec, yec);
	    if (code < 0)
		return code;
	}
    } else			/* !outside */
	for (;;) {
	    const int ymax = rptr->ymax;
	    gx_clip_rect *nptr;

	    yec = min(ymax, ye);
	    if_debug2('Q', "[Q]yc=%d yec=%d\n", yc, yec);
	    do {
		xc = rptr->xmin;
		xec = rptr->xmax;
		if (xc < x)
		    xc = x;
		if (xec > xe)
		    xec = xe;
		if (xec > xc) {
		    clip_rect_print('Q', "match", rptr);
		    if_debug2('Q', "[Q]xc=%d xec=%d\n", xc, xec);
		    INCR(x);
/*
 * Conditionally look ahead to detect unclipped vertical strips.  This is
 * really only valuable for 90 degree rotated images or (nearly-)vertical
 * lines with convex clipping regions; if we ever change images to use
 * source buffering and destination-oriented enumeration, we could probably
 * take out the code here with no adverse effects.
 */
#ifdef CHECK_VERTICAL_CLIPPING
		    if (xec - xc == pccd->w) {	/* full width */
			/* Look ahead for a vertical swath. */
			while ((nptr = rptr->next) != 0 &&
			       nptr->ymin == yec &&
			       nptr->ymax <= ye &&
			       nptr->xmin <= x &&
			       nptr->xmax >= xe
			    )
			    yec = nptr->ymax, rptr = nptr;
		    } else
			nptr = rptr->next;
#else
		    nptr = rptr->next;
#endif
		    code = (*process) (pccd, xc, yc, xec, yec);
		    if (code < 0)
			return code;
		} else {
		    INCR_THEN(no_x, nptr = rptr->next);
		}
	    }
	    while ((rptr = nptr) != 0 && rptr->ymax == ymax);
	    if (rptr == 0 || (yec = rptr->ymin) >= ye)
		break;
	    yc = yec;
	}
    return 0;
}

/* Open a clipping device */
private int
clip_open(register gx_device * dev)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    gx_device *tdev = rdev->target;

    /* Initialize the cursor. */
    rdev->current =
	(rdev->list.head == 0 ? &rdev->list.single : rdev->list.head);
    rdev->color_info = tdev->color_info;
    rdev->width = tdev->width;
    rdev->height = tdev->height;
    return 0;
}

/* Fill a rectangle */
int
clip_call_fill_rectangle(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    return (*dev_proc(pccd->tdev, fill_rectangle))
	(pccd->tdev, xc, yc, xec - xc, yec - yc, pccd->color[0]);
}
private int
clip_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
		    gx_color_index color)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;

    x += rdev->translation.x;
    y += rdev->translation.y;
    ccdata.tdev = rdev->target;
    ccdata.color[0] = color;
    ccdata.x = x, ccdata.y = y, ccdata.w = w, ccdata.h = h;
    return clip_enumerate(rdev, clip_call_fill_rectangle, &ccdata);
}

/* Copy a monochrome rectangle */
int
clip_call_copy_mono(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    return (*dev_proc(pccd->tdev, copy_mono))
	(pccd->tdev, pccd->data + (yc - pccd->y) * pccd->raster,
	 pccd->sourcex + xc - pccd->x, pccd->raster, gx_no_bitmap_id,
	 xc, yc, xec - xc, yec - yc, pccd->color[0], pccd->color[1]);
}
private int
clip_copy_mono(gx_device * dev,
	       const byte * data, int sourcex, int raster, gx_bitmap_id id,
	       int x, int y, int w, int h,
	       gx_color_index color0, gx_color_index color1)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;

    x += rdev->translation.x;
    y += rdev->translation.y;
    ccdata.tdev = rdev->target;
    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.color[0] = color0, ccdata.color[1] = color1;
    ccdata.x = x, ccdata.y = y, ccdata.w = w, ccdata.h = h;
    return clip_enumerate(rdev, clip_call_copy_mono, &ccdata);
}

/* Copy a color rectangle */
int
clip_call_copy_color(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    return (*dev_proc(pccd->tdev, copy_color))
	(pccd->tdev, pccd->data + (yc - pccd->y) * pccd->raster,
	 pccd->sourcex + xc - pccd->x, pccd->raster, gx_no_bitmap_id,
	 xc, yc, xec - xc, yec - yc);
}
private int
clip_copy_color(gx_device * dev,
		const byte * data, int sourcex, int raster, gx_bitmap_id id,
		int x, int y, int w, int h)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;

    x += rdev->translation.x;
    y += rdev->translation.y;
    ccdata.tdev = rdev->target;
    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.x = x, ccdata.y = y, ccdata.w = w, ccdata.h = h;
    return clip_enumerate(rdev, clip_call_copy_color, &ccdata);
}

/* Copy a rectangle with alpha */
int
clip_call_copy_alpha(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    return (*dev_proc(pccd->tdev, copy_alpha))
	(pccd->tdev, pccd->data + (yc - pccd->y) * pccd->raster,
	 pccd->sourcex + xc - pccd->x, pccd->raster, gx_no_bitmap_id,
	 xc, yc, xec - xc, yec - yc, pccd->color[0], pccd->depth);
}
private int
clip_copy_alpha(gx_device * dev,
		const byte * data, int sourcex, int raster, gx_bitmap_id id,
		int x, int y, int w, int h,
		gx_color_index color, int depth)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;

    x += rdev->translation.x;
    y += rdev->translation.y;
    ccdata.tdev = rdev->target;
    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.x = x, ccdata.y = y, ccdata.w = w, ccdata.h = h;
    ccdata.color[0] = color, ccdata.depth = depth;
    return clip_enumerate(rdev, clip_call_copy_alpha, &ccdata);
}

/* Fill a region defined by a mask. */
int
clip_call_fill_mask(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    return (*dev_proc(pccd->tdev, fill_mask))
	(pccd->tdev, pccd->data + (yc - pccd->y) * pccd->raster,
	 pccd->sourcex + xc - pccd->x, pccd->raster, gx_no_bitmap_id,
	 xc, yc, xec - xc, yec - yc, pccd->pdcolor, pccd->depth,
	 pccd->lop, NULL);
}
private int
clip_fill_mask(gx_device * dev,
	       const byte * data, int sourcex, int raster, gx_bitmap_id id,
	       int x, int y, int w, int h,
	       const gx_drawing_color * pdcolor, int depth,
	       gs_logical_operation_t lop, const gx_clip_path * pcpath)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;

    if (pcpath != 0)
	return gx_default_fill_mask(dev, data, sourcex, raster, id,
				    x, y, w, h, pdcolor, depth, lop,
				    pcpath);
    x += rdev->translation.x;
    y += rdev->translation.y;
    ccdata.tdev = rdev->target;
    ccdata.x = x, ccdata.y = y, ccdata.w = w, ccdata.h = h;
    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.pdcolor = pdcolor, ccdata.depth = depth, ccdata.lop = lop;
    return clip_enumerate(rdev, clip_call_fill_mask, &ccdata);
}

/* Strip-tile a rectangle. */
int
clip_call_strip_tile_rectangle(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    return (*dev_proc(pccd->tdev, strip_tile_rectangle))
	(pccd->tdev, pccd->tiles, xc, yc, xec - xc, yec - yc,
	 pccd->color[0], pccd->color[1], pccd->phase.x, pccd->phase.y);
}
private int
clip_strip_tile_rectangle(gx_device * dev, const gx_strip_bitmap * tiles,
			  int x, int y, int w, int h,
     gx_color_index color0, gx_color_index color1, int phase_x, int phase_y)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;

    x += rdev->translation.x;
    y += rdev->translation.y;
    ccdata.tdev = rdev->target;
    ccdata.x = x, ccdata.y = y, ccdata.w = w, ccdata.h = h;
    ccdata.tiles = tiles;
    ccdata.color[0] = color0, ccdata.color[1] = color1;
    ccdata.phase.x = phase_x, ccdata.phase.y = phase_y;
    return clip_enumerate(rdev, clip_call_strip_tile_rectangle, &ccdata);
}

/* Copy a rectangle with RasterOp and strip texture. */
int
clip_call_strip_copy_rop(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    return (*dev_proc(pccd->tdev, strip_copy_rop))
	(pccd->tdev, pccd->data + (yc - pccd->y) * pccd->raster,
	 pccd->sourcex + xc - pccd->x, pccd->raster, gx_no_bitmap_id,
	 pccd->scolors, pccd->textures, pccd->tcolors,
	 xc, yc, xec - xc, yec - yc, pccd->phase.x, pccd->phase.y,
	 pccd->lop);
}
private int
clip_strip_copy_rop(gx_device * dev,
	      const byte * sdata, int sourcex, uint raster, gx_bitmap_id id,
		    const gx_color_index * scolors,
	   const gx_strip_bitmap * textures, const gx_color_index * tcolors,
		    int x, int y, int w, int h,
		    int phase_x, int phase_y, gs_logical_operation_t lop)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;

    x += rdev->translation.x;
    y += rdev->translation.y;
    ccdata.tdev = rdev->target;
    ccdata.x = x, ccdata.y = y, ccdata.w = w, ccdata.h = h;
    ccdata.data = sdata, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.scolors = scolors, ccdata.textures = textures,
	ccdata.tcolors = tcolors;
    ccdata.phase.x = phase_x, ccdata.phase.y = phase_y, ccdata.lop = lop;
    return clip_enumerate(rdev, clip_call_strip_copy_rop, &ccdata);
}

/* Get the (outer) clipping box, in client coordinates. */
private void
clip_get_clipping_box(gx_device * dev, gs_fixed_rect * pbox)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    gx_device *tdev = rdev->target;
    gs_fixed_rect tbox, cbox;
    fixed tx = int2fixed(rdev->translation.x), ty = int2fixed(rdev->translation.y);

    (*dev_proc(tdev, get_clipping_box)) (tdev, &tbox);
    /*
     * To get an accurate clipping box quickly in all cases, we should
     * save the outer box from the clipping path.  However,
     * this is not currently (or even always guaranteed to be)
     * available.  Instead, we compromise: if there is more than one
     * rectangle in the list, we return accurate Y values (which are
     * easy to obtain, because the list is Y-sorted) but copy the
     * X values from the target.
     */
    if (rdev->list.outside || rdev->list.count == 0) {
	cbox = tbox;
    } else if (rdev->list.count == 1) {
	cbox.p.x = int2fixed(rdev->list.single.xmin);
	cbox.p.y = int2fixed(rdev->list.single.ymin);
	cbox.q.x = int2fixed(rdev->list.single.xmax);
	cbox.q.y = int2fixed(rdev->list.single.ymax);
    } else {			/* The head and tail elements are dummies.... */
	cbox.p.x = tbox.p.x;
	cbox.p.y = int2fixed(rdev->list.head->next->ymin);
	cbox.q.x = tbox.q.x;
	cbox.q.y = int2fixed(rdev->list.tail->prev->ymax);
    }
    rect_intersect(tbox, cbox);
    if (tbox.p.x != min_fixed)
	tbox.p.x -= tx;
    if (tbox.p.y != min_fixed)
	tbox.p.y -= ty;
    if (tbox.q.x != max_fixed)
	tbox.q.x -= tx;
    if (tbox.q.y != max_fixed)
	tbox.q.y -= ty;
    *pbox = tbox;
}

/* Get bits back from the device. */
private int
clip_get_bits_rectangle(gx_device * dev, const gs_int_rect * prect,
			gs_get_bits_params_t * params, gs_int_rect ** unread)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    gx_device *tdev = rdev->target;
    int tx = rdev->translation.x, ty = rdev->translation.y;
    gs_int_rect rect;
    int code;

    rect.p.x = prect->p.x - tx, rect.p.y = prect->p.y - ty;
    rect.q.x = prect->q.x - tx, rect.q.y = prect->q.y - ty;
    code = (*dev_proc(tdev, get_bits_rectangle))
	(tdev, &rect, params, unread);
    if (code > 0) {
	/* Adjust unread rectangle coordinates */
	gs_int_rect *list = *unread;
	int i;

	for (i = 0; i < code; ++list, ++i) {
	    list->p.x += tx, list->p.y += ty;
	    list->q.x += tx, list->q.y += ty;
	}
    }
    return code;
}
