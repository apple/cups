/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevbbox.c,v 1.3 2000/03/08 23:14:22 mike Exp $ */
/* Device for tracking bounding box */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsparam.h"
#include "gxdevice.h"
#include "gsdevice.h"		/* requires gsmatrix.h */
#include "gdevbbox.h"
#include "gxdcolor.h"		/* for gx_device_black/white */
#include "gxiparam.h"		/* for image source size */
#include "gxistate.h"
#include "gxpaint.h"
#include "gxpath.h"
#include "gxcpath.h"

/* GC descriptor */
public_st_device_bbox();

/* Device procedures */
private dev_proc_open_device(bbox_open_device);
private dev_proc_close_device(bbox_close_device);
private dev_proc_output_page(bbox_output_page);
private dev_proc_fill_rectangle(bbox_fill_rectangle);
private dev_proc_copy_mono(bbox_copy_mono);
private dev_proc_copy_color(bbox_copy_color);
private dev_proc_get_params(bbox_get_params);
private dev_proc_put_params(bbox_put_params);
private dev_proc_copy_alpha(bbox_copy_alpha);
private dev_proc_fill_path(bbox_fill_path);
private dev_proc_stroke_path(bbox_stroke_path);
private dev_proc_fill_mask(bbox_fill_mask);
private dev_proc_fill_trapezoid(bbox_fill_trapezoid);
private dev_proc_fill_parallelogram(bbox_fill_parallelogram);
private dev_proc_fill_triangle(bbox_fill_triangle);
private dev_proc_draw_thin_line(bbox_draw_thin_line);
private dev_proc_strip_tile_rectangle(bbox_strip_tile_rectangle);
private dev_proc_strip_copy_rop(bbox_strip_copy_rop);
private dev_proc_begin_typed_image(bbox_begin_typed_image);
private dev_proc_create_compositor(bbox_create_compositor);
private dev_proc_text_begin(bbox_text_begin);

/* The device prototype */
/*
 * Normally this would be private, but if the device is going to be used
 * stand-alone, it has to be public.
 */
/*private */ const
/*
 * The bbox device sets the resolution to some value R (currently 4000), and
 * the page size in device pixels to slightly smaller than the largest
 * representable values (around 500K), leaving a little room for stroke
 * widths, rounding, etc.  If an input file (or the command line) resets the
 * resolution to a value R' > R, the page size in pixels will get multiplied
 * by R'/R, and will thereby exceed the representable range, causing a
 * limitcheck.  That is why the bbox device must set the resolution to a
 * value larger than that of any real device.  A consequence of this is that
 * the page size in inches is limited to the maximum representable pixel
 * size divided by R, which gives a limit of about 120" in each dimension.
 */
#define max_coord (max_int_in_fixed - 1000)
#define max_resolution 4000
gx_device_bbox far_data gs_bbox_device =
{
    std_device_std_body(gx_device_bbox, 0, "bbox",
			max_coord, max_coord,
			max_resolution, max_resolution),
    {bbox_open_device,
     NULL,			/* get_initial_matrix */
     NULL,			/* sync_output */
     bbox_output_page,
     bbox_close_device,
     NULL,			/* map_rgb_color */
     NULL,			/* map_color_rgb */
     bbox_fill_rectangle,
     NULL,			/* tile_rectangle */
     bbox_copy_mono,
     bbox_copy_color,
     NULL,			/* draw_line */
     NULL,			/* get_bits */
     bbox_get_params,
     bbox_put_params,
     NULL,			/* map_cmyk_color */
     NULL,			/* get_xfont_procs */
     NULL,			/* get_xfont_device */
     NULL,			/* map_rgb_alpha_color */
     gx_page_device_get_page_device,
     NULL,			/* get_alpha_bits */
     bbox_copy_alpha,
     NULL,			/* get_band */
     NULL,			/* copy_rop */
     bbox_fill_path,
     bbox_stroke_path,
     bbox_fill_mask,
     bbox_fill_trapezoid,
     bbox_fill_parallelogram,
     bbox_fill_triangle,
     bbox_draw_thin_line,
     gx_default_begin_image,
     NULL,			/* image_data */
     NULL,			/* end_image */
     bbox_strip_tile_rectangle,
     bbox_strip_copy_rop,
     NULL,			/* get_clipping_box */
     bbox_begin_typed_image,
     NULL,			/* get_bits_rectangle */
     NULL,			/* map_color_rgb_alpha */
     bbox_create_compositor,
     NULL,			/* get_hardware_params */
     bbox_text_begin
    },
    0,				/* target */
    1				/*true *//* free_standing */
};

#undef max_coord
#undef max_resolution

/* Copy device parameters back from the target. */
private void
bbox_copy_params(gx_device_bbox * bdev, bool remap_white)
{
    gx_device *tdev = bdev->target;

    if (tdev != 0)
	gx_device_copy_params((gx_device *)bdev, tdev);
    if (remap_white)
	bdev->white = gx_device_white((gx_device *)bdev);
}

#define gx_dc_is_white(pdevc, bdev)\
  (gx_dc_is_pure(pdevc) && gx_dc_pure_color(pdevc) == (bdev)->white)

private int
bbox_close_device(gx_device * dev)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;
    gx_device *tdev = bdev->target;

    if ((gx_device *) bdev->box_device != dev) {
	/*
	 * This device was created as a wrapper for a compositor.
	 * Just free the devices.
	 */
	int code = (*dev_proc(tdev, close_device)) (tdev);

	gs_free_object(dev->memory, dev, "bbox_close_device(composite)");
	return code;
    } else {
	return (tdev == 0 ? 0 : (*dev_proc(tdev, close_device)) (tdev));
    }
}

/* Bounding box utilities */

private void near
bbox_initialize(gs_fixed_rect * pr)
{
    pr->p.x = pr->p.y = max_fixed;
    pr->q.x = pr->q.y = min_fixed;
}

private void near
bbox_add_rect(gs_fixed_rect * pr, fixed x0, fixed y0, fixed x1, fixed y1)
{
    if (x0 < pr->p.x)
	pr->p.x = x0;
    if (y0 < pr->p.y)
	pr->p.y = y0;
    if (x1 > pr->q.x)
	pr->q.x = x1;
    if (y1 > pr->q.y)
	pr->q.y = y1;
}
private void near
bbox_add_point(gs_fixed_rect * pr, fixed x, fixed y)
{
    bbox_add_rect(pr, x, y, x, y);
}
private void near
bbox_add_int_rect(gs_fixed_rect * pr, int x0, int y0, int x1, int y1)
{
    bbox_add_rect(pr, int2fixed(x0), int2fixed(y0), int2fixed(x1),
		  int2fixed(y1));
}

#define rect_is_page(dev, x, y, w, h)\
  (x <= 0 && y <= 0 && w >= x + dev->width && h >= y + dev->height)

     /* ---------------- Open/close/page ---------------- */

/* Initialize a bounding box device. */
void
gx_device_bbox_init(gx_device_bbox * dev, gx_device * target)
{
    gx_device_init((gx_device *) dev, (const gx_device *)&gs_bbox_device,
		   (target ? target->memory : NULL), true);
    gx_device_forward_fill_in_procs((gx_device_forward *) dev);
    dev->target = target;
    dev->box_device = dev;
    bbox_copy_params(dev, false);
    dev->free_standing = false;	/* being used as a component */
}

/* Read back the bounding box in 1/72" units. */
void
gx_device_bbox_bbox(gx_device_bbox * dev, gs_rect * pbbox)
{
    const gx_device_bbox *bbdev = dev->box_device;
    gs_matrix mat;
    gs_rect dbox;

    gs_deviceinitialmatrix((gx_device *) dev, &mat);
    dbox.p.x = fixed2float(bbdev->bbox.p.x);
    dbox.p.y = fixed2float(bbdev->bbox.p.y);
    dbox.q.x = fixed2float(bbdev->bbox.q.x);
    dbox.q.y = fixed2float(bbdev->bbox.q.y);
    gs_bbox_transform_inverse(&dbox, &mat, pbbox);
}


private int
bbox_open_device(gx_device * dev)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;

    if (bdev->free_standing) {
	gx_device_forward_fill_in_procs((gx_device_forward *) dev);
	bdev->box_device = bdev;
    }
    if (bdev->box_device == bdev)
	bbox_initialize(&bdev->bbox);
    /* gx_forward_open_device doesn't exist */
    {
	gx_device *tdev = bdev->target;
	int code = (tdev == 0 ? 0 : (*dev_proc(tdev, open_device)) (tdev));

	bbox_copy_params(bdev, true);
	return code;
    }
}

private int
bbox_output_page(gx_device * dev, int num_copies, int flush)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;

    if (bdev->free_standing) {
	/*
	 * This is a free-standing device.  Print the page bounding box.
	 */
	gs_rect bbox;

	gx_device_bbox_bbox(bdev, &bbox);
	dlprintf4("%%%%BoundingBox: %d %d %d %d\n",
		  (int)floor(bbox.p.x), (int)floor(bbox.p.y),
		  (int)ceil(bbox.q.x), (int)ceil(bbox.q.y));
	dlprintf4("%%%%HiResBoundingBox: %f %f %f %f\n",
		  bbox.p.x, bbox.p.y, bbox.q.x, bbox.q.y);
    }
    /*
     * Propagate the PageCount to the target,
     * since it changes every time gs_output_page is called.
     */
    if (bdev->target)
	bdev->target->PageCount = dev->PageCount;
    return gx_forward_output_page(dev, num_copies, flush);
}

/* ---------------- Low-level drawing ---------------- */

private int
bbox_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
		    gx_color_index color)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;
    gx_device_bbox *bbdev = bdev->box_device;

    /* Check for erasing the entire page. */
    if (rect_is_page(dev, x, y, w, h))
	bbox_initialize(&bbdev->bbox);
    else if (color != bdev->white)
	bbox_add_int_rect(&bbdev->bbox, x, y, x + w, y + h);
    /* gx_forward_fill_rectangle doesn't exist */
    {
	gx_device *tdev = bdev->target;

	return (tdev == 0 ? 0 :
		(*dev_proc(tdev, fill_rectangle)) (tdev, x, y, w, h, color));
    }
}

private int
bbox_copy_mono(gx_device * dev, const byte * data,
	    int dx, int raster, gx_bitmap_id id, int x, int y, int w, int h,
	       gx_color_index zero, gx_color_index one)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;
    gx_device_bbox *bbdev = bdev->box_device;

    if ((one != gx_no_color_index && one != bdev->white) ||
	(zero != gx_no_color_index && zero != bdev->white)
	)
	bbox_add_int_rect(&bbdev->bbox, x, y, x + w, y + h);
    /* gx_forward_copy_mono doesn't exist */
    {
	gx_device *tdev = bdev->target;

	return (tdev == 0 ? 0 :
		(*dev_proc(tdev, copy_mono))
		(tdev, data, dx, raster, id, x, y, w, h, zero, one));
    }
}

private int
bbox_copy_color(gx_device * dev, const byte * data,
	    int dx, int raster, gx_bitmap_id id, int x, int y, int w, int h)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;
    gx_device_bbox *bbdev = bdev->box_device;

    bbox_add_int_rect(&bbdev->bbox, x, y, x + w, y + h);
    /* gx_forward_copy_color doesn't exist */
    {
	gx_device *tdev = bdev->target;

	return (tdev == 0 ? 0 :
		(*dev_proc(tdev, copy_color))
		(tdev, data, dx, raster, id, x, y, w, h));
    }
}

private int
bbox_copy_alpha(gx_device * dev, const byte * data, int data_x,
		int raster, gx_bitmap_id id, int x, int y, int w, int h,
		gx_color_index color, int depth)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;
    gx_device_bbox *bbdev = bdev->box_device;

    bbox_add_int_rect(&bbdev->bbox, x, y, x + w, y + h);
    /* gx_forward_copy_alpha doesn't exist */
    {
	gx_device *tdev = bdev->target;

	return (tdev == 0 ? 0 :
		(*dev_proc(tdev, copy_alpha))
		(tdev, data, data_x, raster, id, x, y, w, h, color, depth));
    }
}

private int
bbox_strip_tile_rectangle(gx_device * dev, const gx_strip_bitmap * tiles,
   int x, int y, int w, int h, gx_color_index color0, gx_color_index color1,
			  int px, int py)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;
    gx_device_bbox *bbdev = bdev->box_device;

    if (rect_is_page(dev, x, y, w, h))
	bbox_initialize(&bbdev->bbox);
    else
	bbox_add_int_rect(&bbdev->bbox, x, y, x + w, y + h);
    /* Skip the call if there is no target. */
    {
	gx_device *tdev = bdev->target;

	return (tdev == 0 ? 0 :
		(*dev_proc(tdev, strip_tile_rectangle))
		(tdev, tiles, x, y, w, h, color0, color1, px, py));
    }
}

private int
bbox_strip_copy_rop(gx_device * dev,
		    const byte * sdata, int sourcex, uint sraster,
		    gx_bitmap_id id,
		    const gx_color_index * scolors,
		    const gx_strip_bitmap * textures,
		    const gx_color_index * tcolors,
		    int x, int y, int w, int h,
		    int phase_x, int phase_y, gs_logical_operation_t lop)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;
    gx_device_bbox *bbdev = bdev->box_device;

    bbox_add_int_rect(&bbdev->bbox, x, y, x + w, y + h);
    /* gx_forward_strip_copy_rop doesn't exist */
    {
	gx_device *tdev = bdev->target;

	return (tdev == 0 ? 0 :
		(*dev_proc(tdev, strip_copy_rop))
		(tdev, sdata, sourcex, sraster, id, scolors,
		 textures, tcolors, x, y, w, h, phase_x, phase_y, lop));
    }
}

/* ---------------- Parameters ---------------- */

/* We implement get_params to provide a way to read out the bounding box. */
private int
bbox_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;
    const gx_device_bbox *bbdev = bdev->box_device;
    int code = gx_forward_get_params(dev, plist);
    gs_param_float_array bba;
    float bbox[4];

    if (code < 0)
	return code;
    /*
     * We might be calling get_params before the device has been
     * initialized: in this case, bbdev = 0.
     */
    if (bbdev == 0)
	bbdev = (const gx_device_bbox *)dev;
    bbox[0] = fixed2float(bbdev->bbox.p.x);
    bbox[1] = fixed2float(bbdev->bbox.p.y);
    bbox[2] = fixed2float(bbdev->bbox.q.x);
    bbox[3] = fixed2float(bbdev->bbox.q.y);
    bba.data = bbox, bba.size = 4, bba.persistent = false;
    return param_write_float_array(plist, "PageBoundingBox", &bba);
}

/* We implement put_params to ensure that we keep the important */
/* device parameters up to date, and to prevent an /undefined error */
/* from PageBoundingBox. */
private int
bbox_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;
    int code;
    int ecode = 0;
    gs_param_name param_name;
    gs_param_float_array bba;

    code = param_read_float_array(plist, (param_name = "PageBoundingBox"),
				  &bba);
    switch (code) {
	case 0:
	    if (bba.size != 4) {
		ecode = gs_note_error(gs_error_rangecheck);
		goto e;
	    }
	    break;
	default:
	    ecode = code;
	  e:param_signal_error(plist, param_name, ecode);
	case 1:
	    bba.data = 0;
    }

    code = gx_forward_put_params(dev, plist);
    if (ecode < 0)
	code = ecode;
    if (code >= 0 && bba.data != 0) {
	gx_device_bbox *bbdev = bdev->box_device;

	bbdev->bbox.p.x = float2fixed(bba.data[0]);
	bbdev->bbox.p.y = float2fixed(bba.data[1]);
	bbdev->bbox.q.x = float2fixed(bba.data[2]);
	bbdev->bbox.q.y = float2fixed(bba.data[3]);
    }
    bbox_copy_params(bdev, true);
    return code;
}

/* ---------------- Polygon drawing ---------------- */

private fixed
edge_x_at_y(const gs_fixed_edge * edge, fixed y)
{
    return fixed_mult_quo(edge->end.x - edge->start.x,
			  y - edge->start.y,
			  edge->end.y - edge->start.y) + edge->start.x;
}
private int
bbox_fill_trapezoid(gx_device * dev,
		    const gs_fixed_edge * left, const gs_fixed_edge * right,
		    fixed ybot, fixed ytop, bool swap_axes,
		  const gx_device_color * pdevc, gs_logical_operation_t lop)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;

    if (!gx_dc_is_white(pdevc, bdev)) {
	gx_device_bbox *bbdev = bdev->box_device;
	fixed x0l =
	(left->start.y == ybot ? left->start.x :
	 edge_x_at_y(left, ybot));
	fixed x1l =
	(left->end.y == ytop ? left->end.x :
	 edge_x_at_y(left, ytop));
	fixed x0r =
	(right->start.y == ybot ? right->start.x :
	 edge_x_at_y(right, ybot));
	fixed x1r =
	(right->end.y == ytop ? right->end.x :
	 edge_x_at_y(right, ytop));
	fixed xminl = min(x0l, x1l), xmaxl = max(x0l, x1l);
	fixed xminr = min(x0r, x1r), xmaxr = max(x0r, x1r);
	fixed x0 = min(xminl, xminr), x1 = max(xmaxl, xmaxr);

	if (swap_axes)
	    bbox_add_rect(&bbdev->bbox, ybot, x0, ytop, x1);
	else
	    bbox_add_rect(&bbdev->bbox, x0, ybot, x1, ytop);
    }
    /* Skip the call if there is no target. */
    {
	gx_device *tdev = bdev->target;

	return (tdev == 0 ? 0 :
		(*dev_proc(tdev, fill_trapezoid))
		(tdev, left, right, ybot, ytop, swap_axes, pdevc, lop));
    }
}

private int
bbox_fill_parallelogram(gx_device * dev,
			fixed px, fixed py, fixed ax, fixed ay,
			fixed bx, fixed by, const gx_device_color * pdevc,
			gs_logical_operation_t lop)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;

    if (!gx_dc_is_white(pdevc, bdev)) {
	gx_device_bbox *bbdev = bdev->box_device;
	fixed pax = px + ax, pay = py + ay;

	bbox_add_rect(&bbdev->bbox, px, py, px + bx, py + by);
	bbox_add_rect(&bbdev->bbox, pax, pay, pax + bx, pay + by);
    }
    /* Skip the call if there is no target. */
    {
	gx_device *tdev = bdev->target;

	return (tdev == 0 ? 0 :
		(*dev_proc(tdev, fill_parallelogram))
		(tdev, px, py, ax, ay, bx, by, pdevc, lop));
    }
}

private int
bbox_fill_triangle(gx_device * dev,
		 fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
		   const gx_device_color * pdevc, gs_logical_operation_t lop)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;

    if (!gx_dc_is_white(pdevc, bdev)) {
	gx_device_bbox *bbdev = bdev->box_device;

	bbox_add_rect(&bbdev->bbox, px, py, px + bx, py + by);
	bbox_add_point(&bbdev->bbox, px + ax, py + ay);
    }
    /* Skip the call if there is no target. */
    {
	gx_device *tdev = bdev->target;

	return (tdev == 0 ? 0 :
		(*dev_proc(tdev, fill_triangle))
		(tdev, px, py, ax, ay, bx, by, pdevc, lop));
    }
}

private int
bbox_draw_thin_line(gx_device * dev,
		    fixed fx0, fixed fy0, fixed fx1, fixed fy1,
		  const gx_device_color * pdevc, gs_logical_operation_t lop)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;

    if (!gx_dc_is_white(pdevc, bdev)) {
	gx_device_bbox *bbdev = bdev->box_device;

	bbox_add_rect(&bbdev->bbox, fx0, fy0, fx1, fy1);
    }
    /* Skip the call if there is no target. */
    {
	gx_device *tdev = bdev->target;

	return (tdev == 0 ? 0 :
		(*dev_proc(tdev, draw_thin_line))
		(tdev, fx0, fy0, fx1, fy0, pdevc, lop));
    }
}

/* ---------------- High-level drawing ---------------- */

#define adjust_box(pbox, adj)\
((pbox)->p.x -= (adj).x, (pbox)->p.y -= (adj).y,\
 (pbox)->q.x += (adj).x, (pbox)->q.y += (adj).y)

private int
bbox_fill_path(gx_device * dev, const gs_imager_state * pis, gx_path * ppath,
	       const gx_fill_params * params, const gx_device_color * pdevc,
	       const gx_clip_path * pcpath)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;
    gx_device *tdev = bdev->target;

    if (!gx_dc_is_white(pdevc, bdev)) {
	gs_fixed_rect ibox;
	gs_fixed_point adjust;

	if (gx_path_bbox(ppath, &ibox) < 0)
	    return 0;
	adjust = params->adjust;
	if (params->fill_zero_width)
	    gx_adjust_if_empty(&ibox, &adjust);
	adjust_box(&ibox, adjust);
	if (pcpath != NULL &&
	    !gx_cpath_includes_rectangle(pcpath, ibox.p.x, ibox.p.y,
					 ibox.q.x, ibox.q.y)
	    ) {
	    /* Let the target do the drawing, but break down the */
	    /* fill path into pieces for computing the bounding box. */
	    bdev->target = NULL;
	    gx_default_fill_path(dev, pis, ppath, params, pdevc, pcpath);
	    bdev->target = tdev;
	} else {		/* Just use the path bounding box. */
	    bbox_add_rect(&bdev->bbox, ibox.p.x, ibox.p.y, ibox.q.x,
			  ibox.q.y);
	}
    }
    /* Skip the call if there is no target. */
    return (tdev == 0 ? 0 :
	    (*dev_proc(tdev, fill_path))
	    (tdev, pis, ppath, params, pdevc, pcpath));
}

private int
bbox_stroke_path(gx_device * dev, const gs_imager_state * pis, gx_path * ppath,
		 const gx_stroke_params * params,
		 const gx_drawing_color * pdevc, const gx_clip_path * pcpath)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;
    gx_device *tdev = bdev->target;

    if (!gx_dc_is_white(pdevc, bdev)) {
	gs_fixed_rect ibox;
	gs_fixed_point expand;

	if (gx_path_bbox(ppath, &ibox) < 0)
	    return 0;
	if (gx_stroke_path_expansion(pis, ppath, &expand) < 0)
	    ibox.p.x = ibox.p.y = min_fixed, ibox.q.x = ibox.q.y = max_fixed;
	else
	    adjust_box(&ibox, expand);
	if (pcpath != NULL &&
	    !gx_cpath_includes_rectangle(pcpath, ibox.p.x, ibox.p.y,
					 ibox.q.x, ibox.q.y)
	    ) {
	    /* Let the target do the drawing, but break down the */
	    /* fill path into pieces for computing the bounding box. */
	    bdev->target = NULL;
	    gx_default_stroke_path(dev, pis, ppath, params, pdevc, pcpath);
	    bdev->target = tdev;
	} else {
	    /* Just use the path bounding box. */
	    gx_device_bbox *bbdev = bdev->box_device;

	    bbox_add_rect(&bbdev->bbox, ibox.p.x, ibox.p.y, ibox.q.x,
			  ibox.q.y);
	}
    }
    /* Skip the call if there is no target. */
    return (tdev == 0 ? 0 :
	    (*dev_proc(tdev, stroke_path))
	    (tdev, pis, ppath, params, pdevc, pcpath));
}

private int
bbox_fill_mask(gx_device * dev,
	       const byte * data, int dx, int raster, gx_bitmap_id id,
	       int x, int y, int w, int h,
	       const gx_drawing_color * pdcolor, int depth,
	       gs_logical_operation_t lop, const gx_clip_path * pcpath)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;
    gx_device *tdev = bdev->target;

    if (pcpath != NULL &&
	!gx_cpath_includes_rectangle(pcpath, int2fixed(x), int2fixed(y),
				     int2fixed(x + w),
				     int2fixed(y + h))
	) {
	/* Let the target do the drawing, but break down the */
	/* image into pieces for computing the bounding box. */
	bdev->target = NULL;
	gx_default_fill_mask(dev, data, dx, raster, id, x, y, w, h,
			     pdcolor, depth, lop, pcpath);
	bdev->target = tdev;
    } else {
	/* Just use the mask bounding box. */
	gx_device_bbox *bbdev = bdev->box_device;

	bbox_add_int_rect(&bbdev->bbox, x, y, x + w, y + h);
    }
    /* Skip the call if there is no target. */
    return (tdev == 0 ? 0 :
	    (*dev_proc(tdev, fill_mask))
	    (tdev, data, dx, raster, id, x, y, w, h,
	     pdcolor, depth, lop, pcpath));
}

/* ------ Bitmap imaging ------ */

typedef struct bbox_image_enum_s {
    gx_image_enum_common;
    gs_memory_t *memory;
    gs_matrix matrix;		/* map from image space to device space */
    const gx_clip_path *pcpath;
    gx_image_enum_common_t *target_info;
    int x0, x1;
    int y, height;
} bbox_image_enum;

gs_private_st_ptrs2(st_bbox_image_enum, bbox_image_enum, "bbox_image_enum",
bbox_image_enum_enum_ptrs, bbox_image_enum_reloc_ptrs, pcpath, target_info);

private image_enum_proc_plane_data(bbox_image_plane_data);
private image_enum_proc_end_image(bbox_image_end_image);
private const gx_image_enum_procs_t bbox_image_enum_procs =
{
    bbox_image_plane_data, bbox_image_end_image
};

private int
bbox_image_begin(const gs_imager_state * pis, const gs_matrix * pmat,
		 const gs_image_common_t * pic, const gs_int_rect * prect,
		 const gx_clip_path * pcpath, gs_memory_t * memory,
		 bbox_image_enum ** ppbe)
{
    int code;
    gs_matrix mat;
    bbox_image_enum *pbe;

    if (pmat == 0)
	pmat = &ctm_only(pis);
    if ((code = gs_matrix_invert(&pic->ImageMatrix, &mat)) < 0 ||
	(code = gs_matrix_multiply(&mat, pmat, &mat)) < 0
	)
	return code;
    pbe = gs_alloc_struct(memory, bbox_image_enum, &st_bbox_image_enum,
			  "bbox_image_begin");
    if (pbe == 0)
	return_error(gs_error_VMerror);
    pbe->memory = memory;
    pbe->matrix = mat;
    pbe->pcpath = pcpath;
    pbe->target_info = 0;	/* in case no target */
    if (prect) {
	pbe->x0 = prect->p.x, pbe->x1 = prect->q.x;
	pbe->y = prect->p.y, pbe->height = prect->q.y - prect->p.y;
    } else {
	gs_int_point size;
	int code = (*pic->type->source_size) (pis, pic, &size);

	if (code < 0) {
	    gs_free_object(memory, pbe, "bbox_image_begin");
	    return code;
	}
	pbe->x0 = 0, pbe->x1 = size.x;
	pbe->y = 0, pbe->height = size.y;
    }
    *ppbe = pbe;
    return 0;
}

private void
bbox_image_copy_target_info(bbox_image_enum * pbe, gx_device_bbox * dev)
{
    const gx_image_enum_common_t *target_info = pbe->target_info;

    pbe->num_planes = target_info->num_planes;
    memcpy(pbe->plane_depths, target_info->plane_depths,
	   pbe->num_planes * sizeof(pbe->plane_depths[0]));
    if (dev->target == 0) {
	gx_image_end(pbe->target_info, false);
	pbe->target_info = 0;
    }
}

private int
bbox_begin_typed_image(gx_device * dev,
		       const gs_imager_state * pis, const gs_matrix * pmat,
		   const gs_image_common_t * pic, const gs_int_rect * prect,
		       const gx_drawing_color * pdcolor,
		       const gx_clip_path * pcpath,
		       gs_memory_t * memory, gx_image_enum_common_t ** pinfo)
{
    bbox_image_enum *pbe;
    int code =
    bbox_image_begin(pis, pmat, pic, prect, pcpath, memory, &pbe);

    if (code < 0)
	return code;
    /* We fill in num_planes and plane_depths later. */
    /* format is irrelevant. */
    code = gx_image_enum_common_init((gx_image_enum_common_t *) pbe, pic,
				     &bbox_image_enum_procs, dev,
				     0, 0, gs_image_format_chunky);
    if (code < 0)
	return code;
    *pinfo = (gx_image_enum_common_t *) pbe;
    /*
     * If there is no target, we still have to call default_begin_image
     * to get the correct num_planes and plane_depths.
     */
    {
	gx_device_bbox *const bdev = (gx_device_bbox *) dev;
	gx_device *tdev = bdev->target;

	dev_proc_begin_typed_image((*begin_typed_image));

	if (tdev == 0) {
	    tdev = dev;
	    begin_typed_image = gx_default_begin_typed_image;
	} else {
	    begin_typed_image = dev_proc(tdev, begin_typed_image);
	}
	code = (*begin_typed_image)
	    (tdev, pis, pmat, pic, prect, pdcolor, pcpath, memory,
	     &pbe->target_info);
	if (code < 0)
	    return code;
	bbox_image_copy_target_info(pbe, bdev);
    }
    return 0;
}

private int
bbox_image_plane_data(gx_device * dev,
 gx_image_enum_common_t * info, const gx_image_plane_t * planes, int height)
{

    gx_device_bbox *const bdev = (gx_device_bbox *) dev;
    gx_device *tdev = bdev->target;
    bbox_image_enum *pbe = (bbox_image_enum *) info;
    const gx_clip_path *pcpath = pbe->pcpath;
    gs_rect sbox, dbox;
    gs_point corners[4];
    gs_fixed_rect ibox;

    sbox.p.x = pbe->x0;
    sbox.p.y = pbe->y;
    sbox.q.x = pbe->x1;
    sbox.q.y = pbe->y += height;
    gs_bbox_transform_only(&sbox, &pbe->matrix, corners);
    gs_points_bbox(corners, &dbox);
    ibox.p.x = float2fixed(dbox.p.x);
    ibox.p.y = float2fixed(dbox.p.y);
    ibox.q.x = float2fixed(dbox.q.x);
    ibox.q.y = float2fixed(dbox.q.y);
    if (pcpath != NULL &&
	!gx_cpath_includes_rectangle(pcpath, ibox.p.x, ibox.p.y,
				     ibox.q.x, ibox.q.y)
	) {
	/* Let the target do the drawing, but drive two triangles */
	/* through the clipping path to get an accurate bounding box. */
	gx_device_clip cdev;
	gx_drawing_color devc;
	fixed x0 = float2fixed(corners[0].x), y0 = float2fixed(corners[0].y);
	fixed bx2 = float2fixed(corners[2].x) - x0, by2 = float2fixed(corners[2].y) - y0;

	gx_make_clip_path_device(&cdev, pcpath);
	cdev.target = dev;
	(*dev_proc(&cdev, open_device)) ((gx_device *) & cdev);
	color_set_pure(&devc, 0);	/* any color will do */
	bdev->target = NULL;
	gx_default_fill_triangle((gx_device *) & cdev, x0, y0,
				 float2fixed(corners[1].x) - x0,
				 float2fixed(corners[1].y) - y0,
				 bx2, by2, &devc, lop_default);
	gx_default_fill_triangle((gx_device *) & cdev, x0, y0,
				 float2fixed(corners[3].x) - x0,
				 float2fixed(corners[3].y) - y0,
				 bx2, by2, &devc, lop_default);
	bdev->target = tdev;
    } else {
	/* Just use the bounding box. */
	gx_device_bbox *bbdev = bdev->box_device;

	bbox_add_rect(&bbdev->bbox, ibox.p.x, ibox.p.y, ibox.q.x, ibox.q.y);
    }
    /* Skip the call if there is no target. */
    return (tdev == 0 ? pbe->y >= pbe->height :
	    gx_image_plane_data(pbe->target_info, planes, height));
}

private int
bbox_image_end_image(gx_device * dev, gx_image_enum_common_t * info,
		     bool draw_last)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;
    bbox_image_enum *pbe = (bbox_image_enum *) info;
    void *target_info = pbe->target_info;

    /* Skip the call if there is no target. */
    gx_device *tdev = bdev->target;
    int code =
	(tdev == 0 ? 0 : gx_image_end(target_info, draw_last));

    gs_free_object(pbe->memory, pbe, "bbox_end_image");
    return code;
}

private int
bbox_create_compositor(gx_device * dev,
		       gx_device ** pcdev, const gs_composite_t * pcte,
		       const gs_imager_state * pis, gs_memory_t * memory)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;
    gx_device *target = bdev->target;

    /*
     * If there isn't a target, all we care about is the bounding box,
     * so don't bother with actually compositing.
     */
    if (target == 0) {
	*pcdev = dev;
	return 0;
    }
    /*
     * Create a compositor for the target, and then wrap another
     * bbox device around it, but still accumulating the bounding
     * box in the same place.
     */
    {
	gx_device *cdev;
	gx_device_bbox *bbcdev;
	int code = (*dev_proc(target, create_compositor))
	(target, &cdev, pcte, pis, memory);

	if (code < 0)
	    return code;
	bbcdev = gs_alloc_struct_immovable(memory, gx_device_bbox,
					   &st_device_bbox,
					   "bbox_create_compositor");
	if (bbcdev == 0) {
	    (*dev_proc(cdev, close_device)) (cdev);
	    return_error(gs_error_VMerror);
	}
	gx_device_bbox_init(bbcdev, target);
	bbcdev->target = cdev;
	bbcdev->box_device = bdev;
	*pcdev = (gx_device *) bbcdev;
	return 0;
    }
}

/* ------ Text imaging ------ */

extern_st(st_gs_text_enum);

typedef struct bbox_text_enum_s {
    gs_text_enum_common;
    gs_text_enum_t *target_info;
} bbox_text_enum;

gs_private_st_suffix_add1(st_bbox_text_enum, bbox_text_enum, "bbox_text_enum",
			bbox_text_enum_enum_ptrs, bbox_text_enum_reloc_ptrs,
			  st_gs_text_enum, target_info);

private text_enum_proc_process(bbox_text_process);
private text_enum_proc_set_cache(bbox_text_set_cache);
private rc_free_proc(bbox_text_free);

private const gs_text_enum_procs_t bbox_text_procs =
{
    bbox_text_process, bbox_text_set_cache
};

private int
bbox_text_begin(gx_device * dev, gs_imager_state * pis,
		const gs_text_params_t * text, const gs_font * font,
gx_path * path, const gx_device_color * pdcolor, const gx_clip_path * pcpath,
		gs_memory_t * memory, gs_text_enum_t ** ppenum)
{
    gx_device_bbox *const bdev = (gx_device_bbox *) dev;
    gx_device *tdev = bdev->target;
    bbox_text_enum *pbte;
    int code;

    if (tdev == 0)
	return gx_default_text_begin(dev, pis, text, font, path, pdcolor,
				     pcpath, memory, ppenum);
    rc_alloc_struct_1(pbte, bbox_text_enum, &st_bbox_text_enum, memory,
		      return_error(gs_error_VMerror),
		      "bbox_text_begin");
    pbte->rc.free = bbox_text_free;
    code =
	(*dev_proc(tdev, text_begin))
	(tdev, pis, text, font, path, pdcolor, pcpath, memory,
	 &pbte->target_info);
    if (code < 0) {
	gs_free_object(memory, pbte, "bbox_text_begin");
	return code;
    }
    *(gs_text_enum_t *) pbte = *pbte->target_info;	/* copy common info */
    pbte->procs = &bbox_text_procs;
    *ppenum = (gs_text_enum_t *) pbte;
    return code;
}

private int
bbox_text_process(gs_text_enum_t * pte)
{
    bbox_text_enum *const pbte = (bbox_text_enum *) pte;
    int code = gs_text_process(pbte->target_info);

    if (code < 0)
	return code;
    /* Copy back the dynamic information for the client. */
    pte->index = pbte->target_info->index;
    return code;
}

private int
bbox_text_set_cache(gs_text_enum_t * pte, const double *values,
		    gs_text_cache_control_t control)
{
    bbox_text_enum *const pbte = (bbox_text_enum *) pte;
    gs_text_enum_t *tpte = pbte->target_info;
    int code = tpte->procs->set_cache(tpte, values, control);

    if (code < 0)
	return code;
    /* Copy back the dynamic information for the client. */
    pte->index = tpte->index;
    return code;
}

private void
bbox_text_free(gs_memory_t * memory, void *vpte, client_name_t cname)
{
    bbox_text_enum *const pbte = (bbox_text_enum *) vpte;

    gs_text_release(pbte->target_info, cname);
    rc_free_struct_only(memory, vpte, cname);
}
