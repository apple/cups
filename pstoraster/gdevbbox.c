/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevbbox.c */
/* Device for tracking bounding box */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsparam.h"
#include "gxdevice.h"
#include "gsdevice.h"		/* requires gsmatrix.h */
#include "gdevbbox.h"
#include "gxistate.h"
#include "gxpaint.h"
#include "gxpath.h"
#include "gxcpath.h"

/* Define TEST to create an output_page procedure for testing. */
/*#define TEST*/

/* GC descriptor */
public_st_device_bbox();

/* Device procedures */
private dev_proc_open_device(bbox_open_device);
private dev_proc_close_device(gx_forward_close_device);	/* see below */
#ifdef TEST
private dev_proc_output_page(bbox_output_page);
#else
#define bbox_output_page gx_default_output_page
#endif
private dev_proc_fill_rectangle(bbox_fill_rectangle);
private dev_proc_copy_mono(bbox_copy_mono);
private dev_proc_copy_color(bbox_copy_color);
private dev_proc_draw_line(bbox_draw_line);
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
private dev_proc_begin_image(bbox_begin_image);
private dev_proc_image_data(bbox_image_data);
private dev_proc_end_image(bbox_end_image);
private dev_proc_strip_tile_rectangle(bbox_strip_tile_rectangle);
private dev_proc_strip_copy_rop(bbox_strip_copy_rop);

/* The device prototype */
#ifndef TEST
private const
#endif
gx_device_bbox far_data gs_bbox_device = {
	std_device_std_body(gx_device_bbox, 0, "bbox", 0, 0, 72, 72),
	{	bbox_open_device,
		NULL,			/* get_initial_matrix */
		NULL,			/* sync_output */
		bbox_output_page,
		gx_forward_close_device,
		NULL,			/* map_rgb_color */
		NULL,			/* map_color_rgb */
		bbox_fill_rectangle,
		NULL,			/* tile_rectangle */
		bbox_copy_mono,
		bbox_copy_color,
		bbox_draw_line,
		NULL,			/* get_bits */
		bbox_get_params,
		bbox_put_params,
		NULL,			/* map_cmyk_color */
		NULL,			/* get_xfont_procs */
		NULL,			/* get_xfont_device */
		NULL,			/* map_rgb_alpha_color */
		NULL,			/* get_page_device */
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
		bbox_begin_image,
		bbox_image_data,
		bbox_end_image,
		bbox_strip_tile_rectangle,
		bbox_strip_copy_rop
	},
	0				/* target */
};

/* Copy device parameters back from the target. */
private void
bbox_copy_params(gx_device_bbox *bdev)
{	gx_device *tdev = bdev->target;
	if ( tdev != 0 )
	  { /* This is kind of scatter-shot.... */
#define copy_param(p) bdev->p = tdev->p
#define copy_array_param(p) memcpy(bdev->p, tdev->p, sizeof(bdev->p))
	    copy_param(width);
	    copy_param(height);
	    copy_array_param(MediaSize);
	    copy_array_param(ImagingBBox);
	    copy_param(ImagingBBox_set);
	    copy_array_param(HWResolution);
	    copy_array_param(MarginsHWResolution);
	    copy_array_param(Margins);
	    copy_array_param(HWMargins);
	    copy_param(color_info);
#undef copy_param
#undef copy_array_param
	  }
	else
	  { /* If no target, make the width and height "infinite". */
	    /* Leave some room for stroke widths, rounding, etc. */
#define max_coord (min(max_int, fixed2int(max_fixed)) - 1000)
	    gx_device_set_width_height((gx_device *)bdev,
				       max_coord, max_coord);
#undef max_coord
	  }
	if ( dev_proc(bdev, map_rgb_color) != 0 )
	  bdev->white =
	    (*dev_proc(bdev, map_rgb_color))
	      ((gx_device *)bdev, gx_max_color_value, gx_max_color_value,
	       gx_max_color_value);
}

#define bdev ((gx_device_bbox *)dev)

#define gx_dc_is_white(pdevc, bdev)\
  (gx_dc_is_pure(pdevc) && gx_dc_pure_color(pdevc) == (bdev)->white)

/* Note that some of the "forward" procedures don't exist. */
/* We open-code all but this one below. */
private int
gx_forward_close_device(gx_device *dev)
{	gx_device *tdev = bdev->target;
	return (tdev == 0 ? 0 : (*dev_proc(tdev, close_device))(tdev));
}

/* Bounding box utilities */

private void near
bbox_initialize(gs_fixed_rect *pr)
{	pr->p.x = pr->p.y = max_fixed;
	pr->q.x = pr->q.y = min_fixed;
}

private void near
bbox_add_rect(gs_fixed_rect *pr, fixed x0, fixed y0, fixed x1, fixed y1)
{	if ( x0 < pr->p.x )
	  pr->p.x = x0;
	if ( y0 < pr->p.y )
	  pr->p.y = y0;
	if ( x1 > pr->q.x )
	  pr->q.x = x1;
	if ( y1 > pr->q.y )
	  pr->q.y = y1;
}
private void near
bbox_add_point(gs_fixed_rect *pr, fixed x, fixed y)
{	bbox_add_rect(pr, x, y, x, y);
}
private void near
bbox_add_int_rect(gs_fixed_rect *pr, int x0, int y0, int x1, int y1)
{	bbox_add_rect(pr, int2fixed(x0), int2fixed(y0), int2fixed(x1),
		      int2fixed(y1));
}

#define rect_is_page(dev, x, y, w, h)\
  (x <= 0 && y <= 0 && w >= x + dev->width && h >= y + dev->height)

/* ---------------- Open/close/page ---------------- */

/* Initialize a bounding box device. */
void
gx_device_bbox_init(gx_device_bbox *dev, gx_device *target)
{	*dev = gs_bbox_device;
	gx_device_forward_fill_in_procs((gx_device_forward *)dev);
	bdev->target = target;
	bbox_copy_params(dev);
}

/* Read back the bounding box in 1/72" units. */
void
gx_device_bbox_bbox(gx_device_bbox *dev, gs_rect *pbbox)
{	gs_matrix mat;
	gs_rect dbox;

	gs_deviceinitialmatrix((gx_device *)dev, &mat);
	dbox.p.x = fixed2float(bdev->bbox.p.x);
	dbox.p.y = fixed2float(bdev->bbox.p.y);
	dbox.q.x = fixed2float(bdev->bbox.q.x);
	dbox.q.y = fixed2float(bdev->bbox.q.y);
	gs_bbox_transform_inverse(&dbox, &mat, pbbox);
}


private int
bbox_open_device(gx_device *dev)
{	bbox_initialize(&bdev->bbox);
#ifdef TEST
	gx_device_forward_fill_in_procs((gx_device_forward *)dev);
#endif
	/* gx_forward_open_device doesn't exist */
	{ gx_device *tdev = bdev->target;
	  int code = (tdev == 0 ? 0 : (*dev_proc(tdev, open_device))(tdev));
	  bbox_copy_params(bdev);
	  return code;
	}
}

#ifdef TEST
private int 
bbox_output_page(gx_device *dev, int num_copies, int flush)
{	gs_rect bbox;

	/* Print the page bounding box. */
	gx_device_bbox_bbox((gx_device_bbox *)dev, &bbox);
	dprintf2("[gdevbbox] lower left  = %f %f\n", bbox.p.x, bbox.p.y);
	dprintf2("[gdevbbox] upper right = %f %f\n", bbox.q.x, bbox.q.y);
	return gx_forward_output_page(dev, num_copies, flush);
}
#endif

/* ---------------- Low-level drawing ---------------- */

private int
bbox_fill_rectangle(gx_device *dev, int x, int y, int w, int h,
  gx_color_index color)
{	/* Check for erasing the entire page. */
	if ( rect_is_page(dev, x, y, w, h) )
	  bbox_initialize(&bdev->bbox);
	else if ( color != bdev->white )
	  bbox_add_int_rect(&bdev->bbox, x, y, x + w, y + h);
	/* gx_forward_fill_rectangle doesn't exist */
	{ gx_device *tdev = bdev->target;
	  return (tdev == 0 ? 0 :
		  (*dev_proc(tdev, fill_rectangle))(tdev, x, y, w, h, color));
	}
}

private int
bbox_copy_mono(gx_device *dev, const byte *data,
  int dx, int raster, gx_bitmap_id id, int x, int y, int w, int h,
  gx_color_index zero, gx_color_index one)
{	bbox_add_int_rect(&bdev->bbox, x, y, x + w, y + h);
	/* gx_forward_copy_mono doesn't exist */
	{ gx_device *tdev = bdev->target;
	  return (tdev == 0 ? 0 :
		  (*dev_proc(tdev, copy_mono))(tdev, data, dx, raster, id,
					       x, y, w, h, zero, one));
	}
}

private int
bbox_copy_color(gx_device *dev, const byte *data,
  int dx, int raster, gx_bitmap_id id, int x, int y, int w, int h)
{	bbox_add_int_rect(&bdev->bbox, x, y, x + w, y + h);
	/* gx_forward_copy_color doesn't exist */
	{ gx_device *tdev = bdev->target;
	  return (tdev == 0 ? 0 :
		  (*dev_proc(tdev, copy_color))(tdev, data, dx, raster, id,
					       x, y, w, h));
	}
}

private int
bbox_draw_line(gx_device *dev,
  int x0, int y0, int x1, int y1, gx_color_index color)
{	int xp, yp, xq, yq;
	if ( x0 < x1 )
	  xp = x0, xq = x1 + 1;
	else
	  xp = x1, xq = x0 + 1;
	if ( y0 < y1 )
	  yp = y0, yq = y1 + 1;
	else
	  yp = y1, yq = y0 + 1;
	bbox_add_int_rect(&bdev->bbox, xp, yp, xq, yq);
	/* gx_forward_draw_line doesn't exist */
	{ gx_device *tdev = bdev->target;
	  return (tdev == 0 ? 0 :
		  (*dev_proc(tdev, draw_line))(tdev, x0, y0, x1, y1, color));
	}
}

private int
bbox_copy_alpha(gx_device *dev, const byte *data, int data_x,
  int raster, gx_bitmap_id id, int x, int y, int w, int h,
  gx_color_index color, int depth)
{	bbox_add_int_rect(&bdev->bbox, x, y, x + w, y + h);
	/* gx_forward_copy_alpha doesn't exist */
	{ gx_device *tdev = bdev->target;
	  return (tdev == 0 ? 0 :
		  (*dev_proc(tdev, copy_alpha))(tdev, data, data_x, raster, id,
						x, y, w, h, color, depth));
	}
}

private int
bbox_strip_tile_rectangle(gx_device *dev, const gx_strip_bitmap *tiles,
  int x, int y, int w, int h, gx_color_index color0, gx_color_index color1,
  int px, int py)
{	if ( rect_is_page(dev, x, y, w, h) )
	  bbox_initialize(&bdev->bbox);
	else
	  bbox_add_int_rect(&bdev->bbox, x, y, x + w, y + h);
	/* Skip the call if there is no target. */
	{ gx_device *tdev = bdev->target;
	  return (tdev == 0 ? 0 :
		  (*dev_proc(tdev, strip_tile_rectangle))(tdev, tiles, x, y,
						w, h, color0, color1, px, py));
	}
}

private int
bbox_strip_copy_rop(gx_device *dev,
  const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
  const gx_color_index *scolors,
  const gx_strip_bitmap *textures, const gx_color_index *tcolors,
  int x, int y, int w, int h,
  int phase_x, int phase_y, gs_logical_operation_t lop)
{	bbox_add_int_rect(&bdev->bbox, x, y, x + w, y + h);
	/* gx_forward_strip_copy_rop doesn't exist */
	{ gx_device *tdev = bdev->target;
	  return (tdev == 0 ? 0 :
		  (*dev_proc(tdev, strip_copy_rop))(tdev,
					      sdata, sourcex, sraster, id,
					      scolors, textures, tcolors,
					      x, y, w, h,
					      phase_x, phase_y, lop));
	}
}

/* ---------------- Parameters ---------------- */

/* We implement get_params to provide a way to read out the bounding box. */
private int
bbox_get_params(gx_device *dev, gs_param_list *plist)
{	int code = gx_forward_get_params(dev, plist);
	gs_param_float_array bba;
	float bbox[4];

	if ( code < 0 )
	  return code;
	bbox[0] = bdev->bbox.p.x;
	bbox[1] = bdev->bbox.p.y;
	bbox[2] = bdev->bbox.q.x;
	bbox[3] = bdev->bbox.q.y;
	bba.data = bbox, bba.size = 4, bba.persistent = false;
	return param_write_float_array(plist, "PageBoundingBox", &bba);
}

/* We implement put_params to ensure that we keep the important */
/* device parameters up to date. */
private int
bbox_put_params(gx_device *dev, gs_param_list *plist)
{	int code = gx_forward_put_params(dev, plist);
	bbox_copy_params(bdev);
	return code;
}

/* ---------------- Polygon drawing ---------------- */

private fixed
edge_x_at_y(const gs_fixed_edge *edge, fixed y)
{	return fixed_mult_quo(edge->end.x - edge->start.x,
			      y - edge->start.y,
			      edge->end.y - edge->start.y);
}
private int
bbox_fill_trapezoid(gx_device *dev,
  const gs_fixed_edge *left, const gs_fixed_edge *right,
  fixed ybot, fixed ytop, bool swap_axes,
  const gx_device_color *pdevc, gs_logical_operation_t lop)
{	if ( !gx_dc_is_white(pdevc, bdev) )
	  { fixed x0l =
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

	    if ( swap_axes )
	      bbox_add_rect(&bdev->bbox, ybot, x0, ytop, x1);
	    else
	      bbox_add_rect(&bdev->bbox, x0, ybot, x1, ytop);
	  }
	/* Skip the call if there is no target. */
	{ gx_device *tdev = bdev->target;
	  return (tdev == 0 ? 0 :
		  (*dev_proc(tdev, fill_trapezoid))
		   (tdev, left, right, ybot, ytop, swap_axes, pdevc, lop));
	}
}

private int
bbox_fill_parallelogram(gx_device *dev,
  fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
  const gx_device_color *pdevc, gs_logical_operation_t lop)
{	if ( !gx_dc_is_white(pdevc, bdev) )
	  { fixed pax = px + ax, pay = py + ay;
	    bbox_add_rect(&bdev->bbox, px, py, px + bx, py + by);
	    bbox_add_rect(&bdev->bbox, pax, pay, pax + bx, pay + by);
	  }
	/* Skip the call if there is no target. */
	{ gx_device *tdev = bdev->target;
	  return (tdev == 0 ? 0 :
		  (*dev_proc(tdev, fill_parallelogram))(tdev, px, py, ax, ay,
							bx, by, pdevc, lop));
	}
}

private int
bbox_fill_triangle(gx_device *dev,
  fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
  const gx_device_color *pdevc, gs_logical_operation_t lop)
{	if ( !gx_dc_is_white(pdevc, bdev) )
	  { bbox_add_rect(&bdev->bbox, px, py, px + bx, py + by);
	    bbox_add_point(&bdev->bbox, px + ax, py + ay);
	  }
	/* Skip the call if there is no target. */
	{ gx_device *tdev = bdev->target;
	  return (tdev == 0 ? 0 :
		  (*dev_proc(tdev, fill_triangle))(tdev, px, py, ax, ay,
						   bx, by, pdevc, lop));
	}

}

private int
bbox_draw_thin_line(gx_device *dev,
  fixed fx0, fixed fy0, fixed fx1, fixed fy1,
  const gx_device_color *pdevc, gs_logical_operation_t lop)
{	if ( !gx_dc_is_white(pdevc, bdev) )
	  bbox_add_rect(&bdev->bbox, fx0, fy0, fx1, fy1);
	/* Skip the call if there is no target. */
	{ gx_device *tdev = bdev->target;
	  return (tdev == 0 ? 0 :
		  (*dev_proc(tdev, draw_thin_line))(tdev, fx0, fy0, fx1, fy0,
						    pdevc, lop));
	}
}

/* ---------------- High-level drawing ---------------- */

#define adjust_box(pbox, adj)\
  ((pbox)->p.x -= (adj).x, (pbox)->p.y -= (adj).y,\
   (pbox)->q.x += (adj).x, (pbox)->q.y += (adj).y)

private int
bbox_fill_path(gx_device *dev, const gs_imager_state *pis, gx_path *ppath,
  const gx_fill_params *params, const gx_device_color *pdevc,
  const gx_clip_path *pcpath)
{	gx_device *tdev = bdev->target;

	if ( !gx_dc_is_white(pdevc, bdev) )
	  { gs_fixed_rect ibox;
	    gs_fixed_point adjust;

	    if ( gx_path_bbox(ppath, &ibox) < 0 )
	      return 0;
	    adjust = params->adjust;
	    if ( params->fill_zero_width )
	      gx_adjust_if_empty(&ibox, &adjust);
	    adjust_box(&ibox, adjust);
	    if ( pcpath != NULL &&
		 !gx_cpath_includes_rectangle(pcpath, ibox.p.x, ibox.p.y,
					      ibox.q.x, ibox.q.y)
	       )
	      { /* Let the target do the drawing, but break down the */
		/* fill path into pieces for computing the bounding box. */
		bdev->target = NULL;
		gx_default_fill_path(dev, pis, ppath, params, pdevc, pcpath);
		bdev->target = tdev;
	      }
	    else
	      { /* Just use the path bounding box. */
		bbox_add_rect(&bdev->bbox, ibox.p.x, ibox.p.y, ibox.q.x,
			      ibox.q.y);
	      }
	  }
	/* Skip the call if there is no target. */
	return (tdev == 0 ? 0 :
		(*dev_proc(tdev, fill_path))(tdev, pis, ppath, params, pdevc,
					     pcpath));
}

private int
bbox_stroke_path(gx_device *dev, const gs_imager_state *pis, gx_path *ppath,
  const gx_stroke_params *params,
  const gx_drawing_color *pdevc, const gx_clip_path *pcpath)
{	gx_device *tdev = bdev->target;

	if ( !gx_dc_is_white(pdevc, bdev) )
	  { gs_fixed_rect ibox;
	    gs_fixed_point expand;

	    if ( gx_path_bbox(ppath, &ibox) < 0 )
	      return 0;
	    gx_stroke_expansion(pis, &expand);
	    adjust_box(&ibox, expand);
	    if ( pcpath != NULL &&
		 !gx_cpath_includes_rectangle(pcpath, ibox.p.x, ibox.p.y,
					      ibox.q.x, ibox.q.y)
	       )
	      { /* Let the target do the drawing, but break down the */
		/* fill path into pieces for computing the bounding box. */
		bdev->target = NULL;
		gx_default_stroke_path(dev, pis, ppath, params, pdevc, pcpath);
		bdev->target = tdev;
	      }
	    else
	      { /* Just use the path bounding box. */
		bbox_add_rect(&bdev->bbox, ibox.p.x, ibox.p.y, ibox.q.x,
			      ibox.q.y);
	      }
	  }
	/* Skip the call if there is no target. */
	return (tdev == 0 ? 0 :
		(*dev_proc(tdev, stroke_path))(tdev, pis, ppath, params,
					       pdevc, pcpath));
}

private int
bbox_fill_mask(gx_device *dev,
  const byte *data, int dx, int raster, gx_bitmap_id id,
  int x, int y, int w, int h,
  const gx_drawing_color *pdcolor, int depth,
  gs_logical_operation_t lop, const gx_clip_path *pcpath)
{	gx_device *tdev = bdev->target;

	if ( pcpath != NULL &&
	     !gx_cpath_includes_rectangle(pcpath, int2fixed(x), int2fixed(y),
					  int2fixed(x + w),
					  int2fixed(y + h))
	   )
	  { /* Let the target do the drawing, but break down the */
	    /* image into pieces for computing the bounding box. */
	    bdev->target = NULL;
	    gx_default_fill_mask(dev, data, dx, raster, id, x, y, w, h,
				 pdcolor, depth, lop, pcpath);
	    bdev->target = tdev;
	  }
	else
	  { /* Just use the mask bounding box. */
	    bbox_add_int_rect(&bdev->bbox, x, y, x + w, y + h);
	  }
	/* Skip the call if there is no target. */
	return (tdev == 0 ? 0 :
		(*dev_proc(tdev, fill_mask))(tdev, data, dx, raster, id, x, y,
					 w, h, pdcolor, depth, lop, pcpath));
}

/* ------ Bitmap imaging ------ */

typedef struct bbox_image_enum_s {
	gs_memory_t *memory;
	gs_matrix matrix;	/* map from image space to device space */
	const gx_clip_path *pcpath;
	void *target_info;
} bbox_image_enum;
gs_private_st_ptrs2(st_bbox_image_enum, bbox_image_enum, "bbox_image_enum",
  bbox_image_enum_enum_ptrs, bbox_image_enum_reloc_ptrs, pcpath, target_info);

private int
bbox_begin_image(gx_device *dev,
  const gs_imager_state *pis, const gs_image_t *pim,
  gs_image_format_t format, gs_image_shape_t shape,
  const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,
  gs_memory_t *memory, void **pinfo)
{	int code;
	gs_matrix mat;
	bbox_image_enum *pbe;

	if ( (code = gs_matrix_invert(&pim->ImageMatrix, &mat)) < 0 ||
	     (code = gs_matrix_multiply(&mat, &ctm_only(pis), &mat)) < 0
	   )
	  return_error(gs_error_rangecheck);
	pbe = gs_alloc_struct(memory, bbox_image_enum, &st_bbox_image_enum,
			      "bbox_begin_image");
	if ( pbe == 0 )
	  return_error(gs_error_VMerror);
	pbe->memory = memory;
	pbe->matrix = mat;
	pbe->pcpath = pcpath;
	pbe->target_info = 0;		/* in case no target */
	*pinfo = pbe;
	/* Skip the call if there is no target. */
	{ gx_device *tdev = bdev->target;
	  return (tdev == 0 ? 0 :
		  (*dev_proc(tdev, begin_image))(tdev, pis, pim, format, shape,
						 pdcolor, pcpath, memory,
						 &pbe->target_info));
	}
}

private int
bbox_image_data(gx_device *dev,
  void *info, const byte **planes, uint raster,
  int x, int y, int width, int height)
{	gx_device *tdev = bdev->target;
	bbox_image_enum *pbe = info;
	const gx_clip_path *pcpath = pbe->pcpath;
	gs_rect sbox, dbox;
	gs_point corners[4];
	gs_fixed_rect ibox;

	sbox.p.x = x;
	sbox.p.y = y;
	sbox.q.x = x + width;
	sbox.q.y = y + height;
	gs_bbox_transform_only(&sbox, &pbe->matrix, corners);
	gs_points_bbox(corners, &dbox);
	ibox.p.x = float2fixed(dbox.p.x);
	ibox.p.y = float2fixed(dbox.p.y);
	ibox.q.x = float2fixed(dbox.q.x);
	ibox.q.y = float2fixed(dbox.q.y);
	if ( pcpath != NULL &&
	     !gx_cpath_includes_rectangle(pcpath, ibox.p.x, ibox.p.y,
					  ibox.q.x, ibox.q.y)
	   )
	  { /* Let the target do the drawing, but drive two triangles */
	    /* through the clipping path to get an accurate bounding box. */
	    gx_device_clip cdev;
	    gx_drawing_color devc;
	    fixed x0 = float2fixed(corners[0].x),
	      y0 = float2fixed(corners[0].y);
	    fixed bx2 = float2fixed(corners[2].x) - x0,
	      by2 = float2fixed(corners[2].y) - y0;

	    gx_make_clip_path_device(&cdev, pcpath);
	    cdev.target = dev;
	    (*dev_proc(&cdev, open_device))((gx_device *)&cdev);
	    color_set_pure(&devc, 0);		/* any color will do */
	    bdev->target = NULL;
	    gx_default_fill_triangle((gx_device *)&cdev, x0, y0,
				     float2fixed(corners[1].x) - x0,
				     float2fixed(corners[1].y) - y0,
				     bx2, by2, &devc, lop_default);
	    gx_default_fill_triangle((gx_device *)&cdev, x0, y0,
				     float2fixed(corners[3].x) - x0,
				     float2fixed(corners[3].y) - y0,
				     bx2, by2, &devc, lop_default);
	    bdev->target = tdev;
	  }
	else
	  { /* Just use the bounding box. */
	    bbox_add_rect(&bdev->bbox, ibox.p.x, ibox.p.y, ibox.q.x, ibox.q.y);
	  }
	/* Skip the call if there is no target. */
	return (tdev == 0 ? 0 :
		(*dev_proc(tdev, image_data))(tdev, pbe->target_info, planes,
					      raster, x, y, width, height));
}

private int
bbox_end_image(gx_device *dev, void *info, bool draw_last)
{	bbox_image_enum *pbe = info;
	/* Skip the call if there is no target. */
	gx_device *tdev = bdev->target;

	return (tdev == 0 ? 0 :
		(*dev_proc(tdev, end_image))(tdev, pbe->target_info,
					     draw_last));
}
