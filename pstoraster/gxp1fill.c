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

/*$Id: gxp1fill.c,v 1.1 2000/03/08 23:15:03 mike Exp $ */
/* PatternType 1 filling algorithms */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsrop.h"
#include "gsmatrix.h"
#include "gxcspace.h"		/* for gscolor2.h */
#include "gxcolor2.h"
#include "gxdcolor.h"
#include "gxdevcli.h"
#include "gxdevmem.h"
#include "gxclip2.h"
#include "gxpcolor.h"
#include "gxp1fill.h"

/* Define the state for tile filling. */
typedef struct tile_fill_state_s {

    /* Original arguments */

    const gx_device_color *pdevc;	/* pattern color */
    int x0, y0, w0, h0;
    gs_logical_operation_t lop;
    const gx_rop_source_t *source;

    /* Variables set at initialization */

    gx_device_tile_clip cdev;
    gx_device *pcdev;		/* original device or &cdev */
    const gx_strip_bitmap *tmask;

    /* Following are only for uncolored patterns */

                    dev_color_proc_fill_rectangle((*fill_rectangle));

    /* Following are only for colored patterns */

    const gx_rop_source_t *rop_source;
    gx_device *orig_dev;
    int xoff, yoff;		/* set dynamically */

} tile_fill_state_t;

/* Initialize the filling state. */
private int
tile_fill_init(tile_fill_state_t * ptfs, const gx_device_color * pdevc,
	       gx_device * dev)
{
    gx_color_tile *m_tile = pdevc->mask.m_tile;

    ptfs->pdevc = pdevc;
    if (m_tile == 0) {		/* no clipping */
	ptfs->pcdev = dev;
	return 0;
    }
    ptfs->pcdev = (gx_device *) & ptfs->cdev;
    ptfs->tmask = &m_tile->tmask;
    return tile_clip_initialize(&ptfs->cdev, ptfs->tmask, dev, 0, 0);
}

/*
 * Fill with non-standard X and Y stepping.
 * ptile is pdevc->colors.pattern.{m,p}_tile.
 * tbits_or_tmask is whichever of tbits and tmask is supplying
 * the tile size.
 * This implementation could be sped up considerably!
 */
private int
tile_by_steps(tile_fill_state_t * ptfs, int x0, int y0, int w0, int h0,
	      const gx_color_tile * ptile,
	      const gx_strip_bitmap * tbits_or_tmask,
	      int (*fill_proc) (P5(const tile_fill_state_t * ptfs,
				   int x, int y, int w, int h)))
{
    int x1 = x0 + w0, y1 = y0 + h0;
    int i0, i1, j0, j1, i, j;
    gs_matrix step_matrix;	/* translated by phase */
    int code;

    ptfs->x0 = x0, ptfs->w0 = w0;
    ptfs->y0 = y0, ptfs->h0 = h0;
    step_matrix = ptile->step_matrix;
    {
	gs_rect bbox;		/* bounding box in device space */
	gs_rect ibbox;		/* bounding box in stepping space */
	double bbw = ptile->bbox.q.x - ptile->bbox.p.x;
	double bbh = ptile->bbox.q.y - ptile->bbox.p.y;

	bbox.p.x = x0, bbox.p.y = y0;
	bbox.q.x = x1, bbox.q.y = y1;
	gs_bbox_transform_inverse(&bbox, &step_matrix, &ibbox);
	if_debug10('T',
	  "[T]x,y=(%d,%d) w,h=(%d,%d) => (%g,%g),(%g,%g), offset=(%g,%g)\n",
		   x0, y0, w0, h0,
		   ibbox.p.x, ibbox.p.y, ibbox.q.x, ibbox.q.y,
		   step_matrix.tx, step_matrix.ty);
	i0 = (int)ceil(ibbox.p.x - bbw - 0.000001);
	i1 = (int)floor(ibbox.q.x + 0.000001);
	j0 = (int)ceil(ibbox.p.y - bbh - 0.000001);
	j1 = (int)floor(ibbox.q.y + 0.000001);
    }
    if_debug4('T', "[T]i=(%d,%d) j=(%d,%d)\n", i0, i1, j0, j1);
    for (i = i0; i <= i1; i++)
	for (j = j0; j <= j1; j++) {
	    int x = (int)(step_matrix.xx * i +
			  step_matrix.yx * j + step_matrix.tx);
	    int y = (int)(step_matrix.xy * i +
			  step_matrix.yy * j + step_matrix.ty);
	    int w = tbits_or_tmask->size.x;
	    int h = tbits_or_tmask->size.y;
	    int xoff, yoff;

	    if_debug4('T', "[T]i=%d j=%d x,y=(%d,%d)", i, j, x, y);
	    if (x < x0)
		xoff = x0 - x, x = x0, w -= xoff;
	    else
		xoff = 0;
	    if (y < y0)
		yoff = y0 - y, y = y0, h -= yoff;
	    else
		yoff = 0;
	    if (x + w > x1)
		w = x1 - x;
	    if (y + h > y1)
		h = y1 - y;
	    if_debug6('T', "=>(%d,%d) w,h=(%d,%d) x/yoff=(%d,%d)\n",
		      x, y, w, h, xoff, yoff);
	    if (w > 0 && h > 0) {
		if (ptfs->pcdev == (gx_device *) & ptfs->cdev)
		    tile_clip_set_phase(&ptfs->cdev,
				imod(xoff - x, ptfs->tmask->rep_width),
				imod(yoff - y, ptfs->tmask->rep_height));
		/* Set the offsets for colored pattern fills */
		ptfs->xoff = xoff;
		ptfs->yoff = yoff;
		code = (*fill_proc) (ptfs, x, y, w, h);
		if (code < 0)
		    return code;
	    }
	}
    return 0;
}

/* Fill a rectangle with a colored Pattern. */
/* Note that we treat this as "texture" for RasterOp. */
private int
tile_colored_fill(const tile_fill_state_t * ptfs,
		  int x, int y, int w, int h)
{
    gx_color_tile *ptile = ptfs->pdevc->colors.pattern.p_tile;
    gs_logical_operation_t lop = ptfs->lop;
    const gx_rop_source_t *source = ptfs->source;
    const gx_rop_source_t *rop_source = ptfs->rop_source;
    gx_device *dev = ptfs->orig_dev;
    int xoff = ptfs->xoff, yoff = ptfs->yoff;
    gx_strip_bitmap *bits = &ptile->tbits;
    const byte *data = bits->data;
    bool full_transfer = (w == ptfs->w0 && h == ptfs->h0);
    gx_bitmap_id source_id =
    (full_transfer ? rop_source->id : gx_no_bitmap_id);
    int code;

    if (source == NULL && lop_no_S_is_T(lop))
	code = (*dev_proc(ptfs->pcdev, copy_color))
	    (ptfs->pcdev, data + bits->raster * yoff, xoff,
	     bits->raster,
	     (full_transfer ? bits->id : gx_no_bitmap_id),
	     x, y, w, h);
    else {
	gx_strip_bitmap data_tile;

	data_tile.data = (byte *) data;		/* actually const */
	data_tile.raster = bits->raster;
	data_tile.size.x = data_tile.rep_width = ptile->tbits.size.x;
	data_tile.size.y = data_tile.rep_height = ptile->tbits.size.y;
	data_tile.id = bits->id;
	data_tile.shift = data_tile.rep_shift = 0;
	code = (*dev_proc(dev, strip_copy_rop))
	    (dev,
	     rop_source->sdata + (y - ptfs->y0) * rop_source->sraster,
	     rop_source->sourcex + (x - ptfs->x0),
	     rop_source->sraster, source_id,
	     (rop_source->use_scolors ? rop_source->scolors : NULL),
	     &data_tile, NULL,
	     x, y, w, h,
	     imod(xoff - x, data_tile.rep_width),
	     imod(yoff - y, data_tile.rep_height),
	     lop);
    }
    return code;
}
int
gx_dc_pattern_fill_rectangle(const gx_device_color * pdevc, int x, int y,
			     int w, int h, gx_device * dev,
			     gs_logical_operation_t lop,
			     const gx_rop_source_t * source)
{
    gx_color_tile *ptile = pdevc->colors.pattern.p_tile;
    const gx_rop_source_t *rop_source = source;
    gx_rop_source_t no_source;
    gx_strip_bitmap *bits;
    tile_fill_state_t state;
    int code;

    if (ptile == 0)		/* null pattern */
	return 0;
    if (rop_source == NULL)
	set_rop_no_source(rop_source, no_source, dev);
    bits = &ptile->tbits;
    code = tile_fill_init(&state, pdevc, dev);
    if (code < 0)
	return code;
    if (ptile->is_simple) {
	int px = imod(-(int)(ptile->step_matrix.tx + 0.5), bits->rep_width);
	int py = imod(-(int)(ptile->step_matrix.ty + 0.5), bits->rep_height);

	if (state.pcdev != dev)
	    tile_clip_set_phase(&state.cdev, px, py);
	if (source == NULL && lop_no_S_is_T(lop))
	    code = (*dev_proc(state.pcdev, strip_tile_rectangle))
		(state.pcdev, bits, x, y, w, h,
		 gx_no_color_index, gx_no_color_index, px, py);
	else
	    code = (*dev_proc(state.pcdev, strip_copy_rop))
		(state.pcdev,
		 rop_source->sdata, rop_source->sourcex,
		 rop_source->sraster, rop_source->id,
		 (rop_source->use_scolors ? rop_source->scolors : NULL),
		 bits, NULL, x, y, w, h, px, py, lop);
    } else {
	state.lop = lop;
	state.source = source;
	state.rop_source = rop_source;
	state.orig_dev = dev;
	code = tile_by_steps(&state, x, y, w, h, ptile,
			     &ptile->tbits, tile_colored_fill);
    }
    return code;
}

/* Fill a rectangle with an uncolored Pattern. */
/* Note that we treat this as "texture" for RasterOp. */
private int
tile_masked_fill(const tile_fill_state_t * ptfs,
		 int x, int y, int w, int h)
{
    if (ptfs->source == NULL)
	return (*ptfs->fill_rectangle)
	    (ptfs->pdevc, x, y, w, h, ptfs->pcdev, ptfs->lop, NULL);
    else {
	const gx_rop_source_t *source = ptfs->source;
	gx_rop_source_t step_source;

	step_source.sdata = source->sdata + (y - ptfs->y0) * source->sraster;
	step_source.sourcex = source->sourcex + (x - ptfs->x0);
	step_source.sraster = source->sraster;
	step_source.id = (w == ptfs->w0 && h == ptfs->h0 ?
			  source->id : gx_no_bitmap_id);
	step_source.scolors[0] = source->scolors[0];
	step_source.scolors[1] = source->scolors[1];
	step_source.use_scolors = source->use_scolors;
	return (*ptfs->fill_rectangle)
	    (ptfs->pdevc, x, y, w, h, ptfs->pcdev, ptfs->lop, &step_source);
    }
}
int
gx_dc_pure_masked_fill_rect(const gx_device_color * pdevc,
			    int x, int y, int w, int h, gx_device * dev,
			    gs_logical_operation_t lop,
			    const gx_rop_source_t * source)
{
    gx_color_tile *ptile = pdevc->mask.m_tile;
    tile_fill_state_t state;
    int code;

    /*
     * This routine should never be called if there is no masking,
     * but we leave the checks below just in case.
     */
    code = tile_fill_init(&state, pdevc, dev);
    if (code < 0)
	return code;
    if (state.pcdev != dev) {
	int px = imod(-(int)(ptile->step_matrix.tx + 0.5),
		      ptile->tmask.rep_width);
	int py = imod(-(int)(ptile->step_matrix.ty + 0.5),
		      ptile->tmask.rep_height);

	tile_clip_set_phase(&state.cdev, px, py);
    }
    if (state.pcdev == dev || ptile->is_simple)
	return (*gx_dc_type_data_pure.fill_rectangle)
	    (pdevc, x, y, w, h, state.pcdev, lop, source);
    else {
	state.lop = lop;
	state.source = source;
	state.fill_rectangle = gx_dc_type_data_pure.fill_rectangle;
	return tile_by_steps(&state, x, y, w, h, ptile, &ptile->tmask,
			     tile_masked_fill);
    }
}
int
gx_dc_binary_masked_fill_rect(const gx_device_color * pdevc,
			      int x, int y, int w, int h, gx_device * dev,
			      gs_logical_operation_t lop,
			      const gx_rop_source_t * source)
{
    gx_color_tile *ptile = pdevc->mask.m_tile;
    tile_fill_state_t state;
    int code;

    code = tile_fill_init(&state, pdevc, dev);
    if (code < 0)
	return code;
    if (state.pcdev == dev || ptile->is_simple)
	return (*gx_dc_type_data_ht_binary.fill_rectangle)
	    (pdevc, x, y, w, h, state.pcdev, lop, source);
    else {
	state.lop = lop;
	state.source = source;
	state.fill_rectangle = gx_dc_type_data_ht_binary.fill_rectangle;
	return tile_by_steps(&state, x, y, w, h, ptile, &ptile->tmask,
			     tile_masked_fill);
    }
}
int
gx_dc_colored_masked_fill_rect(const gx_device_color * pdevc,
			       int x, int y, int w, int h, gx_device * dev,
			       gs_logical_operation_t lop,
			       const gx_rop_source_t * source)
{
    gx_color_tile *ptile = pdevc->mask.m_tile;
    tile_fill_state_t state;
    int code;

    code = tile_fill_init(&state, pdevc, dev);
    if (code < 0)
	return code;
    if (state.pcdev == dev || ptile->is_simple)
	return (*gx_dc_type_data_ht_colored.fill_rectangle)
	    (pdevc, x, y, w, h, state.pcdev, lop, source);
    else {
	state.lop = lop;
	state.source = source;
	state.fill_rectangle = gx_dc_type_data_ht_colored.fill_rectangle;
	return tile_by_steps(&state, x, y, w, h, ptile, &ptile->tmask,
			     tile_masked_fill);
    }
}
