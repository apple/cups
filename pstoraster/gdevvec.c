/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevvec.c,v 1.1 2000/03/08 23:14:26 mike Exp $ */
/* Utilities for "vector" devices */
#include "math_.h"
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gp.h"
#include "gserrors.h"
#include "gsparam.h"
#include "gsutil.h"
#include "gxfixed.h"
#include "gdevvec.h"
#include "gscspace.h"
#include "gxdcolor.h"
#include "gxpaint.h"		/* requires gx_path, ... */
#include "gzpath.h"
#include "gzcpath.h"

/******
 ****** NOTE: EVERYTHING IN THIS FILE IS SUBJECT TO CHANGE WITHOUT NOTICE.
 ****** USE AT YOUR OWN RISK.
 ******/

/* Structure descriptors */
public_st_device_vector();
public_st_vector_image_enum();

/* ================ Default implementations of vector procs ================ */

int
gdev_vector_setflat(gx_device_vector * vdev, floatp flatness)
{
    return 0;
}

int
gdev_vector_dopath(gx_device_vector * vdev, const gx_path * ppath,
		   gx_path_type_t type)
{
    bool do_close = (type & gx_path_type_stroke) != 0;
    gs_fixed_rect rect;
    gs_point scale;
    double x_start = 0, y_start = 0, x_prev, y_prev;
    bool first = true;
    gs_path_enum cenum;
    int code;

    if (gx_path_is_rectangle(ppath, &rect))
	return (*vdev_proc(vdev, dorect)) (vdev, rect.p.x, rect.p.y, rect.q.x,
					   rect.q.y, type);
    scale = vdev->scale;
    code = (*vdev_proc(vdev, beginpath)) (vdev, type);
    gx_path_enum_init(&cenum, ppath);
    for (;;) {
	fixed vs[6];
	int pe_op = gx_path_enum_next(&cenum, (gs_fixed_point *) vs);
	double x, y;

      sw:switch (pe_op) {
	    case 0:		/* done */
		return (*vdev_proc(vdev, endpath)) (vdev, type);
	    case gs_pe_moveto:
		code = (*vdev_proc(vdev, moveto))
		    (vdev, x_prev, y_prev, (x = fixed2float(vs[0]) / scale.x),
		     (y = fixed2float(vs[1]) / scale.y), type);
		if (first)
		    x_start = x, y_start = y, first = false;
		break;
	    case gs_pe_lineto:
		code = (*vdev_proc(vdev, lineto))
		    (vdev, x_prev, y_prev, (x = fixed2float(vs[0]) / scale.x),
		     (y = fixed2float(vs[1]) / scale.y), type);
		break;
	    case gs_pe_curveto:
		code = (*vdev_proc(vdev, curveto))
		    (vdev, x_prev, y_prev,
		     fixed2float(vs[0]) / scale.x,
		     fixed2float(vs[1]) / scale.y,
		     fixed2float(vs[2]) / scale.x,
		     fixed2float(vs[3]) / scale.y,
		     (x = fixed2float(vs[4]) / scale.x),
		     (y = fixed2float(vs[5]) / scale.y),
		     type);
		break;
	    case gs_pe_closepath:
		x = x_start, y = y_start;
		if (do_close) {
		    code = (*vdev_proc(vdev, closepath))
			(vdev, x_prev, y_prev, x_start, y_start, type);
		    break;
		}
		pe_op = gx_path_enum_next(&cenum, (gs_fixed_point *) vs);
		if (pe_op != 0) {
		    code = (*vdev_proc(vdev, closepath))
			(vdev, x_prev, y_prev, x_start, y_start, type);
		    if (code < 0)
			return code;
		    goto sw;
		}
		return (*vdev_proc(vdev, endpath)) (vdev, type);
	    default:		/* can't happen */
		return_error(gs_error_unknownerror);
	}
	if (code < 0)
	    return code;
	x_prev = x, y_prev = y;
    }
}

int
gdev_vector_dorect(gx_device_vector * vdev, fixed x0, fixed y0, fixed x1,
		   fixed y1, gx_path_type_t type)
{
    int code = (*vdev_proc(vdev, beginpath)) (vdev, type);

    if (code < 0)
	return code;
    code = gdev_vector_write_rectangle(vdev, x0, y0, x1, y1,
				       (type & gx_path_type_stroke) != 0,
				       gx_rect_x_first);
    if (code < 0)
	return code;
    return (*vdev_proc(vdev, endpath)) (vdev, type);
}

/* ================ Utility procedures ================ */

/* Recompute the cached color values. */
private void
gdev_vector_load_cache(gx_device_vector * vdev)
{
    vdev->black = gx_device_black((gx_device *)vdev);
    vdev->white = gx_device_white((gx_device *)vdev);
}

/* Initialize the state. */
void
gdev_vector_init(gx_device_vector * vdev)
{
    gdev_vector_reset(vdev);
    vdev->scale.x = vdev->scale.y = 1.0;
    vdev->in_page = false;
    gdev_vector_load_cache(vdev);
}

/* Reset the remembered graphics state. */
void
gdev_vector_reset(gx_device_vector * vdev)
{
    static const gs_imager_state state_initial =
    {gs_imager_state_initial(1)};

    vdev->state = state_initial;
    color_unset(&vdev->fill_color);
    color_unset(&vdev->stroke_color);
    vdev->clip_path_id =
	vdev->no_clip_path_id = gs_next_ids(1);
}

/* Open the output file and stream. */
int
gdev_vector_open_file_bbox(gx_device_vector * vdev, uint strmbuf_size,
			   bool bbox)
{				/* Open the file as positionable if possible. */
    int code = gx_device_open_output_file((gx_device *) vdev, vdev->fname,
					  true, true, &vdev->file);

    if (code < 0)
	return code;
    if ((vdev->strmbuf = gs_alloc_bytes(vdev->v_memory, strmbuf_size,
					"vector_open(strmbuf)")) == 0 ||
	(vdev->strm = s_alloc(vdev->v_memory,
			      "vector_open(strm)")) == 0 ||
	(bbox &&
	 (vdev->bbox_device =
	  gs_alloc_struct_immovable(vdev->v_memory,
				    gx_device_bbox, &st_device_bbox,
				    "vector_open(bbox_device)")) == 0)
	) {
	if (vdev->bbox_device)
	    gs_free_object(vdev->v_memory, vdev->bbox_device,
			   "vector_open(bbox_device)");
	vdev->bbox_device = 0;
	if (vdev->strm)
	    gs_free_object(vdev->v_memory, vdev->strm,
			   "vector_open(strm)");
	vdev->strm = 0;
	if (vdev->strmbuf)
	    gs_free_object(vdev->v_memory, vdev->strmbuf,
			   "vector_open(strmbuf)");
	vdev->strmbuf = 0;
	fclose(vdev->file);
	vdev->file = 0;
	return_error(gs_error_VMerror);
    }
    vdev->strmbuf_size = strmbuf_size;
    swrite_file(vdev->strm, vdev->file, vdev->strmbuf, strmbuf_size);
    /*
     * We don't want finalization to close the file, but we do want it
     * to flush the stream buffer.
     */
    vdev->strm->procs.close = vdev->strm->procs.flush;
    if (vdev->bbox_device) {
	gx_device_bbox_init(vdev->bbox_device, NULL);
	gx_device_set_resolution((gx_device *) vdev->bbox_device,
				 vdev->HWResolution[0],
				 vdev->HWResolution[1]);
	/* Do the right thing about upright vs. inverted. */
	/* (This is dangerous in general, since the procedure */
	/* might reference non-standard elements.) */
	set_dev_proc(vdev->bbox_device, get_initial_matrix,
		     dev_proc(vdev, get_initial_matrix));
	(*dev_proc(vdev->bbox_device, open_device))
	    ((gx_device *) vdev->bbox_device);
    }
    return 0;
}

/* Get the current stream, calling beginpage if in_page is false. */
stream *
gdev_vector_stream(gx_device_vector * vdev)
{
    if (!vdev->in_page) {
	(*vdev_proc(vdev, beginpage)) (vdev);
	vdev->in_page = true;
    }
    return vdev->strm;
}

/* Compare two drawing colors. */
/* Right now we don't attempt to handle non-pure colors. */
private bool
drawing_color_eq(const gx_drawing_color * pdc1, const gx_drawing_color * pdc2)
{
    return (gx_dc_is_pure(pdc1) ?
	    gx_dc_is_pure(pdc2) &&
	    gx_dc_pure_color(pdc1) == gx_dc_pure_color(pdc2) :
	    gx_dc_is_null(pdc1) ?
	    gx_dc_is_null(pdc2) :
	    false);
}

/* Update the logical operation. */
int
gdev_vector_update_log_op(gx_device_vector * vdev, gs_logical_operation_t lop)
{
    gs_logical_operation_t diff = lop ^ vdev->state.log_op;

    if (diff != 0) {
	int code = (*vdev_proc(vdev, setlogop)) (vdev, lop, diff);

	if (code < 0)
	    return code;
	vdev->state.log_op = lop;
    }
    return 0;
}

/* Update the fill color. */
int
gdev_vector_update_fill_color(gx_device_vector * vdev,
			      const gx_drawing_color * pdcolor)
{
    if (!drawing_color_eq(pdcolor, &vdev->fill_color)) {
	int code = (*vdev_proc(vdev, setfillcolor)) (vdev, pdcolor);

	if (code < 0)
	    return code;
	vdev->fill_color = *pdcolor;
    }
    return 0;
}

/* Update the state for filling a region. */
private int
update_fill(gx_device_vector * vdev, const gx_drawing_color * pdcolor,
	    gs_logical_operation_t lop)
{
    int code = gdev_vector_update_fill_color(vdev, pdcolor);

    if (code < 0)
	return code;
    return gdev_vector_update_log_op(vdev, lop);
}

/* Bring state up to date for filling. */
int
gdev_vector_prepare_fill(gx_device_vector * vdev, const gs_imager_state * pis,
	    const gx_fill_params * params, const gx_drawing_color * pdcolor)
{
    if (params->flatness != vdev->state.flatness) {
	int code = (*vdev_proc(vdev, setflat)) (vdev, params->flatness);

	if (code < 0)
	    return code;
	vdev->state.flatness = params->flatness;
    }
    return update_fill(vdev, pdcolor, pis->log_op);
}

/* Compare two dash patterns. */
private bool
dash_pattern_eq(const float *stored, const gx_dash_params * set, floatp scale)
{
    int i;

    for (i = 0; i < set->pattern_size; ++i)
	if (stored[i] != (float)(set->pattern[i] * scale))
	    return false;
    return true;
}

/* Bring state up to date for stroking. */
int
gdev_vector_prepare_stroke(gx_device_vector * vdev, const gs_imager_state * pis,
	  const gx_stroke_params * params, const gx_drawing_color * pdcolor,
			   floatp scale)
{
    int pattern_size = pis->line_params.dash.pattern_size;
    float dash_offset = pis->line_params.dash.offset * scale;
    float half_width = pis->line_params.half_width * scale;

    if (pattern_size > max_dash)
	return_error(gs_error_limitcheck);
    if (dash_offset != vdev->state.line_params.dash.offset ||
	pattern_size != vdev->state.line_params.dash.pattern_size ||
	(pattern_size != 0 &&
	 !dash_pattern_eq(vdev->dash_pattern, &pis->line_params.dash,
			  scale))
	) {
	float pattern[max_dash];
	int i, code;

	for (i = 0; i < pattern_size; ++i)
	    pattern[i] = pis->line_params.dash.pattern[i] * scale;
	code = (*vdev_proc(vdev, setdash))
	    (vdev, pattern, pattern_size, dash_offset);
	if (code < 0)
	    return code;
	memcpy(vdev->dash_pattern, pattern, pattern_size * sizeof(float));

	vdev->state.line_params.dash.pattern_size = pattern_size;
	vdev->state.line_params.dash.offset = dash_offset;
    }
    if (params->flatness != vdev->state.flatness) {
	int code = (*vdev_proc(vdev, setflat)) (vdev, params->flatness);

	if (code < 0)
	    return code;
	vdev->state.flatness = params->flatness;
    }
    if (half_width != vdev->state.line_params.half_width) {
	int code = (*vdev_proc(vdev, setlinewidth))
	(vdev, pis->line_params.half_width * 2);

	if (code < 0)
	    return code;
	vdev->state.line_params.half_width = half_width;
    }
    if (pis->line_params.miter_limit != vdev->state.line_params.miter_limit) {
	int code = (*vdev_proc(vdev, setmiterlimit))
	(vdev, pis->line_params.miter_limit);

	if (code < 0)
	    return code;
	gx_set_miter_limit(&vdev->state.line_params,
			   pis->line_params.miter_limit);
    }
    if (pis->line_params.cap != vdev->state.line_params.cap) {
	int code = (*vdev_proc(vdev, setlinecap))
	(vdev, pis->line_params.cap);

	if (code < 0)
	    return code;
	vdev->state.line_params.cap = pis->line_params.cap;
    }
    if (pis->line_params.join != vdev->state.line_params.join) {
	int code = (*vdev_proc(vdev, setlinejoin))
	(vdev, pis->line_params.join);

	if (code < 0)
	    return code;
	vdev->state.line_params.join = pis->line_params.join;
    } {
	int code = gdev_vector_update_log_op(vdev, pis->log_op);

	if (code < 0)
	    return code;
    }
    if (!drawing_color_eq(pdcolor, &vdev->stroke_color)) {
	int code = (*vdev_proc(vdev, setstrokecolor)) (vdev, pdcolor);

	if (code < 0)
	    return code;
	vdev->stroke_color = *pdcolor;
    }
    return 0;
}

/* Write a polygon as part of a path. */
/* May call beginpath, moveto, lineto, closepath, endpath. */
int
gdev_vector_write_polygon(gx_device_vector * vdev, const gs_fixed_point * points,
			  uint count, bool close, gx_path_type_t type)
{
    int code = 0;

    if (type != gx_path_type_none &&
	(code = (*vdev_proc(vdev, beginpath)) (vdev, type)) < 0
	)
	return code;
    if (count > 0) {
	double x = fixed2float(points[0].x) / vdev->scale.x, y = fixed2float(points[0].y) / vdev->scale.y;
	double x_start = x, y_start = y, x_prev, y_prev;
	uint i;

	code = (*vdev_proc(vdev, moveto))
	    (vdev, 0.0, 0.0, x, y, type);
	if (code >= 0)
	    for (i = 1; i < count && code >= 0; ++i) {
		x_prev = x, y_prev = y;
		code = (*vdev_proc(vdev, lineto))
		    (vdev, x_prev, y_prev,
		     (x = fixed2float(points[i].x) / vdev->scale.x),
		     (y = fixed2float(points[i].y) / vdev->scale.y),
		     type);
	    }
	if (code >= 0 && close)
	    code = (*vdev_proc(vdev, closepath))
		(vdev, x, y, x_start, y_start, type);
    }
    return (code >= 0 && type != gx_path_type_none ?
	    (*vdev_proc(vdev, endpath)) (vdev, type) : code);
}

/* Write a rectangle as part of a path. */
/* May call moveto, lineto, closepath. */
int
gdev_vector_write_rectangle(gx_device_vector * vdev, fixed x0, fixed y0,
	      fixed x1, fixed y1, bool close, gx_rect_direction_t direction)
{
    gs_fixed_point points[4];

    points[0].x = x0, points[0].y = y0;
    points[2].x = x1, points[2].y = y1;
    if (direction == gx_rect_x_first)
	points[1].x = x1, points[1].y = y0,
	    points[3].x = x0, points[3].y = y1;
    else
	points[1].x = x0, points[1].y = y1,
	    points[3].x = x1, points[3].y = y0;
    return gdev_vector_write_polygon(vdev, points, 4, close,
				     gx_path_type_none);
}

/* Write a clipping path by calling the path procedures. */
int
gdev_vector_write_clip_path(gx_device_vector * vdev, const gx_clip_path * pcpath)
{
    const gx_clip_rect *prect;
    gx_clip_rect page_rect;
    int code;

    if (pcpath == 0) {		/* There's no special provision for initclip. */
	/* Write a rectangle that covers the entire page. */
	page_rect.xmin = page_rect.ymin = 0;
	page_rect.xmax = vdev->width;
	page_rect.ymax = vdev->height;
	page_rect.next = 0;
	prect = &page_rect;
    } else if (pcpath->path_valid)
	return (*vdev_proc(vdev, dopath)) (vdev, &pcpath->path,
					   gx_path_type_clip);
    else {
	const gx_clip_list *list = gx_cpath_list(pcpath);

	prect = list->head;
	if (prect == 0)
	    prect = &list->single;
    }
    /* Write out the rectangles. */
    code = (*vdev_proc(vdev, beginpath)) (vdev, gx_path_type_clip);
    for (; code >= 0 && prect != 0; prect = prect->next)
	if (prect->xmax > prect->xmin && prect->ymax > prect->ymin)
	    code = gdev_vector_write_rectangle
		(vdev, int2fixed(prect->xmin), int2fixed(prect->ymin),
		 int2fixed(prect->xmax), int2fixed(prect->ymax),
		 false, gx_rect_x_first);
    if (code >= 0)
	code = (*vdev_proc(vdev, endpath)) (vdev, gx_path_type_clip);
    return code;
}

/* Update the clipping path if needed. */
int
gdev_vector_update_clip_path(gx_device_vector * vdev,
			     const gx_clip_path * pcpath)
{
    if (pcpath) {
	if (pcpath->id != vdev->clip_path_id) {
	    int code = gdev_vector_write_clip_path(vdev, pcpath);

	    if (code < 0)
		return code;
	    vdev->clip_path_id = pcpath->id;
	}
    } else {
	if (vdev->clip_path_id != vdev->no_clip_path_id) {
	    int code = gdev_vector_write_clip_path(vdev, NULL);

	    if (code < 0)
		return code;
	    vdev->clip_path_id = vdev->no_clip_path_id;
	}
    }
    return 0;
}

/* Close the output file and stream. */
void
gdev_vector_close_file(gx_device_vector * vdev)
{
    gs_free_object(vdev->v_memory, vdev->bbox_device,
		   "vector_close(bbox_device)");
    vdev->bbox_device = 0;
    sclose(vdev->strm);
    gs_free_object(vdev->v_memory, vdev->strm, "vector_close(strm)");
    vdev->strm = 0;
    gs_free_object(vdev->v_memory, vdev->strmbuf, "vector_close(strmbuf)");
    vdev->strmbuf = 0;
    fclose(vdev->file);		/* we prevented sclose from doing this */
    vdev->file = 0;
}

/* ---------------- Image enumeration ---------------- */

/* Initialize for enumerating an image. */
int
gdev_vector_begin_image(gx_device_vector * vdev,
			const gs_imager_state * pis, const gs_image_t * pim,
			gs_image_format_t format, const gs_int_rect * prect,
	      const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
		    gs_memory_t * mem, const gx_image_enum_procs_t * pprocs,
			gdev_vector_image_enum_t * pie)
{
    const gs_color_space *pcs = pim->ColorSpace;
    int num_components;
    int bits_per_pixel;
    int code;

    if (pim->ImageMask)
	bits_per_pixel = num_components = 1;
    else
	num_components = gs_color_space_num_components(pcs),
	    bits_per_pixel = pim->BitsPerComponent;
    code = gx_image_enum_common_init((gx_image_enum_common_t *) pie,
				     (const gs_image_common_t *)pim,
				     pprocs, (gx_device *) vdev,
				     bits_per_pixel, num_components,
				     format);
    if (code < 0)
	return code;
    pie->bits_per_pixel = bits_per_pixel * num_components /
	pie->num_planes;
    pie->default_info = 0;
    pie->bbox_info = 0;
    if ((code = gdev_vector_update_log_op(vdev, pis->log_op)) < 0 ||
	(code = gdev_vector_update_clip_path(vdev, pcpath)) < 0 ||
	((pim->ImageMask ||
	  (pim->CombineWithColor && rop3_uses_T(pis->log_op))) &&
	 (code = gdev_vector_update_fill_color(vdev, pdcolor)) < 0) ||
	(vdev->bbox_device &&
	 (code = (*dev_proc(vdev->bbox_device, begin_image))
	  ((gx_device *) vdev->bbox_device, pis, pim, format, prect,
	   pdcolor, pcpath, mem, &pie->bbox_info)) < 0)
	)
	return code;
    pie->memory = mem;
    if (prect)
	pie->width = prect->q.x - prect->p.x,
	    pie->height = prect->q.y - prect->p.y;
    else
	pie->width = pim->Width, pie->height = pim->Height;
    pie->bits_per_row = pie->width * pie->bits_per_pixel;
    pie->y = 0;
    return 0;
}

/* End an image, optionally supplying any necessary blank padding rows. */
/* Return 0 if we used the default implementation, 1 if not. */
int
gdev_vector_end_image(gx_device_vector * vdev,
	 gdev_vector_image_enum_t * pie, bool draw_last, gx_color_index pad)
{
    int code;

    if (pie->default_info) {
	code = gx_default_end_image((gx_device *) vdev, pie->default_info,
				    draw_last);
	if (code >= 0)
	    code = 0;
    } else {			/* Fill out to the full image height. */
	if (pie->y < pie->height && pad != gx_no_color_index) {
	    uint bytes_per_row = (pie->bits_per_row + 7) >> 3;
	    byte *row = gs_alloc_bytes(pie->memory, bytes_per_row,
				       "gdev_vector_end_image(fill)");

	    if (row == 0)
		return_error(gs_error_VMerror);
/****** FILL VALUE IS WRONG ******/
	    memset(row, (byte) pad, bytes_per_row);
	    for (; pie->y < pie->height; pie->y++)
		gx_image_data((gx_image_enum_common_t *) pie,
			      (const byte **)&row, 0,
			      bytes_per_row, 1);
	    gs_free_object(pie->memory, row,
			   "gdev_vector_end_image(fill)");
	}
	code = 1;
    }
    if (vdev->bbox_device) {
	int bcode = gx_image_end(pie->bbox_info, draw_last);

	if (bcode < 0)
	    code = bcode;
    }
    gs_free_object(pie->memory, pie, "gdev_vector_end_image");
    return code;
}

/* ================ Device procedures ================ */

#define vdev ((gx_device_vector *)dev)

/* Get parameters. */
int
gdev_vector_get_params(gx_device * dev, gs_param_list * plist)
{
    int code = gx_default_get_params(dev, plist);
    int ecode;
    gs_param_string ofns;

    if (code < 0)
	return code;
    ofns.data = (const byte *)vdev->fname,
	ofns.size = strlen(vdev->fname),
	ofns.persistent = false;
    if ((ecode = param_write_string(plist, "OutputFile", &ofns)) < 0)
	return ecode;
    return code;
}

/* Put parameters. */
int
gdev_vector_put_params(gx_device * dev, gs_param_list * plist)
{
    int ecode = 0;
    int code;
    gs_param_name param_name;
    gs_param_string ofns;

    switch (code = param_read_string(plist, (param_name = "OutputFile"), &ofns)) {
	case 0:
	    if (ofns.size > fname_size)
		ecode = gs_error_limitcheck;
	    else
		break;
	    goto ofe;
	default:
	    ecode = code;
	  ofe:param_signal_error(plist, param_name, ecode);
	case 1:
	    ofns.data = 0;
	    break;
    }

    if (ecode < 0)
	return ecode;
    {
	bool open = dev->is_open;

	/* Don't let gx_default_put_params close the device. */
	dev->is_open = false;
	code = gx_default_put_params(dev, plist);
	dev->is_open = open;
    }
    if (code < 0)
	return code;

    if (ofns.data != 0 &&
	bytes_compare(ofns.data, ofns.size,
		      (const byte *)vdev->fname, strlen(vdev->fname))
	) {
	memcpy(vdev->fname, ofns.data, ofns.size);
	vdev->fname[ofns.size] = 0;
	if (vdev->file != 0) {
	    gdev_vector_close_file(vdev);
	    return gdev_vector_open_file(vdev, vdev->strmbuf_size);
	}
    }
    gdev_vector_load_cache(vdev);	/* in case color mapping changed */
    return 0;
}

/* ---------------- Defaults ---------------- */

int
gdev_vector_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
			   gx_color_index color)
{
    gx_drawing_color dcolor;

    /* Ignore the initial fill with white. */
    if (!vdev->in_page && color == vdev->white)
	return 0;
    color_set_pure(&dcolor, color);
    {
	int code = update_fill(vdev, &dcolor, rop3_T);

	if (code < 0)
	    return code;
    }
    if (vdev->bbox_device) {
	int code = (*dev_proc(vdev->bbox_device, fill_rectangle))
	((gx_device *) vdev->bbox_device, x, y, w, h, color);

	if (code < 0)
	    return code;
    }
    return (*vdev_proc(vdev, dorect)) (vdev, int2fixed(x), int2fixed(y),
				       int2fixed(x + w), int2fixed(y + h),
				       gx_path_type_fill);
}

int
gdev_vector_fill_path(gx_device * dev, const gs_imager_state * pis,
		      gx_path * ppath, const gx_fill_params * params,
		 const gx_device_color * pdevc, const gx_clip_path * pcpath)
{
    int code;

    if ((code = gdev_vector_prepare_fill(vdev, pis, params, pdevc)) < 0 ||
	(code = gdev_vector_update_clip_path(vdev, pcpath)) < 0 ||
	(vdev->bbox_device &&
	 (code = (*dev_proc(vdev->bbox_device, fill_path))
	  ((gx_device *) vdev->bbox_device, pis, ppath, params,
	   pdevc, pcpath)) < 0) ||
	(code = (*vdev_proc(vdev, dopath))
	 (vdev, ppath,
	  (params->rule > 0 ? gx_path_type_even_odd :
	   gx_path_type_winding_number) | gx_path_type_fill)) < 0
	)
	return gx_default_fill_path(dev, pis, ppath, params, pdevc, pcpath);
    return code;
}

int
gdev_vector_stroke_path(gx_device * dev, const gs_imager_state * pis,
			gx_path * ppath, const gx_stroke_params * params,
	      const gx_drawing_color * pdcolor, const gx_clip_path * pcpath)
{
    int code;

/****** HANDLE SCALE ******/
    if ((code = gdev_vector_prepare_stroke(vdev, pis, params, pdcolor,
					   dev->HWResolution[0])) < 0 ||
	(code = gdev_vector_update_clip_path(vdev, pcpath)) < 0 ||
	(vdev->bbox_device &&
	 (code = (*dev_proc(vdev->bbox_device, stroke_path))
	  ((gx_device *) vdev->bbox_device, pis, ppath, params,
	   pdcolor, pcpath)) < 0) ||
	(code = (*vdev_proc(vdev, dopath))
	 (vdev, ppath, gx_path_type_stroke)) < 0
	)
	return gx_default_stroke_path(dev, pis, ppath, params, pdcolor, pcpath);
    return code;
}

int
gdev_vector_fill_trapezoid(gx_device * dev, const gs_fixed_edge * left,
	const gs_fixed_edge * right, fixed ybot, fixed ytop, bool swap_axes,
		  const gx_device_color * pdevc, gs_logical_operation_t lop)
{
    fixed xl = left->start.x;
    fixed wl = left->end.x - xl;
    fixed yl = left->start.y;
    fixed hl = left->end.y - yl;
    fixed xr = right->start.x;
    fixed wr = right->end.x - xr;
    fixed yr = right->start.y;
    fixed hr = right->end.y - yr;
    fixed x0l = xl + fixed_mult_quo(wl, ybot - yl, hl);
    fixed x1l = xl + fixed_mult_quo(wl, ytop - yl, hl);
    fixed x0r = xr + fixed_mult_quo(wr, ybot - yr, hr);
    fixed x1r = xr + fixed_mult_quo(wr, ytop - yr, hr);

#define y0 ybot
#define y1 ytop
    int code = update_fill(vdev, pdevc, lop);
    gs_fixed_point points[4];

    if (code < 0)
	return gx_default_fill_trapezoid(dev, left, right, ybot, ytop,
					 swap_axes, pdevc, lop);
    if (swap_axes)
	points[0].y = x0l, points[1].y = x0r,
	    points[0].x = points[1].x = y0,
	    points[2].y = x1r, points[3].y = x1l,
	    points[2].x = points[3].x = y1;
    else
	points[0].x = x0l, points[1].x = x0r,
	    points[0].y = points[1].y = y0,
	    points[2].x = x1r, points[3].x = x1l,
	    points[2].y = points[3].y = y1;
#undef y0
#undef y1
    if (vdev->bbox_device) {
	int code = (*dev_proc(vdev->bbox_device, fill_trapezoid))
	((gx_device *) vdev->bbox_device, left, right, ybot, ytop,
	 swap_axes, pdevc, lop);

	if (code < 0)
	    return code;
    }
    return gdev_vector_write_polygon(vdev, points, 4, true,
				     gx_path_type_fill);
}

int
gdev_vector_fill_parallelogram(gx_device * dev,
		 fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
		  const gx_device_color * pdevc, gs_logical_operation_t lop)
{
    fixed pax = px + ax, pay = py + ay;
    int code = update_fill(vdev, pdevc, lop);
    gs_fixed_point points[4];

    if (code < 0)
	return gx_default_fill_parallelogram(dev, px, py, ax, ay, bx, by,
					     pdevc, lop);
    if (vdev->bbox_device) {
	code = (*dev_proc(vdev->bbox_device, fill_parallelogram))
	    ((gx_device *) vdev->bbox_device, px, py, ax, ay, bx, by,
	     pdevc, lop);
	if (code < 0)
	    return code;
    }
    points[0].x = px, points[0].y = py;
    points[1].x = pax, points[0].y = pay;
    points[2].x = pax + bx, points[2].y = pay + by;
    points[3].x = px + bx, points[3].y = py + by;
    return gdev_vector_write_polygon(vdev, points, 4, true,
				     gx_path_type_fill);
}

int
gdev_vector_fill_triangle(gx_device * dev,
		 fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
		  const gx_device_color * pdevc, gs_logical_operation_t lop)
{
    int code = update_fill(vdev, pdevc, lop);
    gs_fixed_point points[3];

    if (code < 0)
	return gx_default_fill_triangle(dev, px, py, ax, ay, bx, by,
					pdevc, lop);
    if (vdev->bbox_device) {
	code = (*dev_proc(vdev->bbox_device, fill_triangle))
	    ((gx_device *) vdev->bbox_device, px, py, ax, ay, bx, by,
	     pdevc, lop);
	if (code < 0)
	    return code;
    }
    points[0].x = px, points[0].y = py;
    points[1].x = px + ax, points[1].y = py + ay;
    points[2].x = px + bx, points[2].y = py + by;
    return gdev_vector_write_polygon(vdev, points, 3, true,
				     gx_path_type_fill);
}

#undef vdev
