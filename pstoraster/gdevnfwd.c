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

/*$Id: gdevnfwd.c,v 1.2 2000/03/08 23:14:24 mike Exp $ */
/* Null and forwarding device implementation */
#include "gx.h"
#include "gserrors.h"
#include "gxdevice.h"

/* ---------------- Forwarding procedures ---------------- */

/* Fill in NULL procedures in a forwarding device procedure record. */
/* We don't fill in: open_device, close_device, or the lowest-level */
/* drawing operations. */
void
gx_device_forward_fill_in_procs(register gx_device_forward * dev)
{
    gx_device_set_procs((gx_device *) dev);
    /* NOT open_device */
    fill_dev_proc(dev, get_initial_matrix, gx_forward_get_initial_matrix);
    fill_dev_proc(dev, sync_output, gx_forward_sync_output);
    fill_dev_proc(dev, output_page, gx_forward_output_page);
    /* NOT close_device */
    fill_dev_proc(dev, map_rgb_color, gx_forward_map_rgb_color);
    fill_dev_proc(dev, map_color_rgb, gx_forward_map_color_rgb);
    /* NOT fill_rectangle */
    /* NOT tile_rectangle */
    /* NOT copy_mono */
    /* NOT copy_color */
    /* NOT draw_line (OBSOLETE) */
    fill_dev_proc(dev, get_bits, gx_forward_get_bits);
    fill_dev_proc(dev, get_params, gx_forward_get_params);
    fill_dev_proc(dev, put_params, gx_forward_put_params);
    fill_dev_proc(dev, map_cmyk_color, gx_forward_map_cmyk_color);
    fill_dev_proc(dev, get_xfont_procs, gx_forward_get_xfont_procs);
    fill_dev_proc(dev, get_xfont_device, gx_forward_get_xfont_device);
    fill_dev_proc(dev, map_rgb_alpha_color, gx_forward_map_rgb_alpha_color);
    fill_dev_proc(dev, get_page_device, gx_forward_get_page_device);
    fill_dev_proc(dev, get_alpha_bits, gx_forward_get_alpha_bits);
    /* NOT copy_alpha */
    fill_dev_proc(dev, get_band, gx_forward_get_band);
    fill_dev_proc(dev, copy_rop, gx_forward_copy_rop);
    fill_dev_proc(dev, fill_path, gx_forward_fill_path);
    fill_dev_proc(dev, stroke_path, gx_forward_stroke_path);
    fill_dev_proc(dev, fill_mask, gx_forward_fill_mask);
    fill_dev_proc(dev, fill_trapezoid, gx_forward_fill_trapezoid);
    fill_dev_proc(dev, fill_parallelogram, gx_forward_fill_parallelogram);
    fill_dev_proc(dev, fill_triangle, gx_forward_fill_triangle);
    fill_dev_proc(dev, draw_thin_line, gx_forward_draw_thin_line);
    fill_dev_proc(dev, begin_image, gx_forward_begin_image);
    /* NOT image_data (OBSOLETE) */
    /* NOT end_image (OBSOLETE) */
    /* NOT strip_tile_rectangle */
    fill_dev_proc(dev, strip_copy_rop, gx_forward_strip_copy_rop);
    fill_dev_proc(dev, get_clipping_box, gx_forward_get_clipping_box);
    fill_dev_proc(dev, begin_typed_image, gx_forward_begin_typed_image);
    fill_dev_proc(dev, get_bits_rectangle, gx_forward_get_bits_rectangle);
    fill_dev_proc(dev, map_color_rgb_alpha, gx_forward_map_color_rgb_alpha);
    fill_dev_proc(dev, create_compositor, gx_no_create_compositor);
    fill_dev_proc(dev, get_hardware_params, gx_forward_get_hardware_params);
    fill_dev_proc(dev, text_begin, gx_forward_text_begin);
    gx_device_fill_in_procs((gx_device *) dev);
}

/* Forward the color mapping procedures from a device to its target. */
void
gx_device_forward_color_procs(gx_device_forward * dev)
{
    set_dev_proc(dev, map_rgb_color, gx_forward_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gx_forward_map_color_rgb);
    set_dev_proc(dev, map_cmyk_color, gx_forward_map_cmyk_color);
    set_dev_proc(dev, map_rgb_alpha_color, gx_forward_map_rgb_alpha_color);
    set_dev_proc(dev, map_color_rgb_alpha, gx_forward_map_color_rgb_alpha);
}

void
gx_forward_get_initial_matrix(gx_device * dev, gs_matrix * pmat)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    if (tdev == 0)
	gx_default_get_initial_matrix(dev, pmat);
    else
	(*dev_proc(tdev, get_initial_matrix)) (tdev, pmat);
}

int
gx_forward_sync_output(gx_device * dev)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_default_sync_output(dev) :
	    (*dev_proc(tdev, sync_output)) (tdev));
}

int
gx_forward_output_page(gx_device * dev, int num_copies, int flush)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_default_output_page(dev, num_copies, flush) :
	    (*dev_proc(tdev, output_page)) (tdev, num_copies, flush));
}

gx_color_index
gx_forward_map_rgb_color(gx_device * dev, gx_color_value r, gx_color_value g,
			 gx_color_value b)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_default_map_rgb_color(dev, r, g, b) :
	    (*dev_proc(tdev, map_rgb_color)) (tdev, r, g, b));
}

int
gx_forward_map_color_rgb(gx_device * dev, gx_color_index color,
			 gx_color_value prgb[3])
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_default_map_color_rgb(dev, color, prgb) :
	    (*dev_proc(tdev, map_color_rgb)) (tdev, color, prgb));
}

int
gx_forward_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
			  gx_color_index color)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    if (tdev == 0)
	return_error(gs_error_Fatal);
    return (*dev_proc(tdev, fill_rectangle)) (tdev, x, y, w, h, color);
}

int
gx_forward_tile_rectangle(gx_device * dev, const gx_tile_bitmap * tile,
   int x, int y, int w, int h, gx_color_index color0, gx_color_index color1,
			  int px, int py)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    dev_proc_tile_rectangle((*proc));

    if (tdev == 0)
	tdev = dev, proc = gx_default_tile_rectangle;
    else
	proc = dev_proc(tdev, tile_rectangle);
    return (*proc) (tdev, tile, x, y, w, h, color0, color1, px, py);
}

int
gx_forward_copy_mono(gx_device * dev, const byte * data,
	    int dx, int raster, gx_bitmap_id id, int x, int y, int w, int h,
		     gx_color_index zero, gx_color_index one)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    if (tdev == 0)
	return_error(gs_error_Fatal);
    return (*dev_proc(tdev, copy_mono))
	(tdev, data, dx, raster, id, x, y, w, h, zero, one);
}

int
gx_forward_copy_color(gx_device * dev, const byte * data,
	    int dx, int raster, gx_bitmap_id id, int x, int y, int w, int h)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    if (tdev == 0)
	return_error(gs_error_Fatal);
    return (*dev_proc(tdev, copy_color))
	(tdev, data, dx, raster, id, x, y, w, h);
}

int
gx_forward_get_bits(gx_device * dev, int y, byte * data, byte ** actual_data)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_default_get_bits(dev, y, data, actual_data) :
	    (*dev_proc(tdev, get_bits)) (tdev, y, data, actual_data));
}

int
gx_forward_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_default_get_params(dev, plist) :
	    (*dev_proc(tdev, get_params)) (tdev, plist));
}

int
gx_forward_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_default_put_params(dev, plist) :
	    (*dev_proc(tdev, put_params)) (tdev, plist));
}

gx_color_index
gx_forward_map_cmyk_color(gx_device * dev, gx_color_value c, gx_color_value m,
			  gx_color_value y, gx_color_value k)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_default_map_cmyk_color(dev, c, m, y, k) :
	    (*dev_proc(tdev, map_cmyk_color)) (tdev, c, m, y, k));
}

const gx_xfont_procs *
gx_forward_get_xfont_procs(gx_device * dev)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_default_get_xfont_procs(dev) :
	    (*dev_proc(tdev, get_xfont_procs)) (tdev));
}

gx_device *
gx_forward_get_xfont_device(gx_device * dev)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_default_get_xfont_device(dev) :
	    (*dev_proc(tdev, get_xfont_device)) (tdev));
}

gx_color_index
gx_forward_map_rgb_alpha_color(gx_device * dev, gx_color_value r,
		   gx_color_value g, gx_color_value b, gx_color_value alpha)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ?
	    gx_default_map_rgb_alpha_color(dev, r, g, b, alpha) :
	    (*dev_proc(tdev, map_rgb_alpha_color)) (tdev, r, g, b, alpha));
}

gx_device *
gx_forward_get_page_device(gx_device * dev)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    gx_device *pdev;

    if (tdev == 0)
	return gx_default_get_page_device(dev);
    pdev = (*dev_proc(tdev, get_page_device)) (tdev);
    return (pdev == tdev ? dev : pdev);
}

int
gx_forward_get_alpha_bits(gx_device * dev, graphics_object_type type)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ?
	    gx_default_get_alpha_bits(dev, type) :
	    (*dev_proc(tdev, get_alpha_bits)) (tdev, type));
}

int
gx_forward_get_band(gx_device * dev, int y, int *band_start)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ?
	    gx_default_get_band(dev, y, band_start) :
	    (*dev_proc(tdev, get_band)) (tdev, y, band_start));
}

int
gx_forward_copy_rop(gx_device * dev,
	     const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
		    const gx_color_index * scolors,
	     const gx_tile_bitmap * texture, const gx_color_index * tcolors,
		    int x, int y, int width, int height,
		    int phase_x, int phase_y, gs_logical_operation_t lop)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    dev_proc_copy_rop((*proc));

    if (tdev == 0)
	tdev = dev, proc = gx_default_copy_rop;
    else
	proc = dev_proc(tdev, copy_rop);
    return (*proc) (tdev, sdata, sourcex, sraster, id, scolors,
		    texture, tcolors, x, y, width, height,
		    phase_x, phase_y, lop);
}

int
gx_forward_fill_path(gx_device * dev, const gs_imager_state * pis,
		     gx_path * ppath, const gx_fill_params * params,
	      const gx_drawing_color * pdcolor, const gx_clip_path * pcpath)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    dev_proc_fill_path((*proc));

    if (tdev == 0)
	tdev = dev, proc = gx_default_fill_path;
    else
	proc = dev_proc(tdev, fill_path);
    return (*proc) (tdev, pis, ppath, params, pdcolor, pcpath);
}

int
gx_forward_stroke_path(gx_device * dev, const gs_imager_state * pis,
		       gx_path * ppath, const gx_stroke_params * params,
	      const gx_drawing_color * pdcolor, const gx_clip_path * pcpath)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    dev_proc_stroke_path((*proc));

    if (tdev == 0)
	tdev = dev, proc = gx_default_stroke_path;
    else
	proc = dev_proc(tdev, stroke_path);
    return (*proc) (tdev, pis, ppath, params, pdcolor, pcpath);
}

int
gx_forward_fill_mask(gx_device * dev,
		     const byte * data, int dx, int raster, gx_bitmap_id id,
		     int x, int y, int w, int h,
		     const gx_drawing_color * pdcolor, int depth,
		     gs_logical_operation_t lop, const gx_clip_path * pcpath)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    dev_proc_fill_mask((*proc));

    if (tdev == 0)
	tdev = dev, proc = gx_default_fill_mask;
    else
	proc = dev_proc(tdev, fill_mask);
    return (*proc) (tdev, data, dx, raster, id, x, y, w, h, pdcolor, depth,
		    lop, pcpath);
}

int
gx_forward_fill_trapezoid(gx_device * dev,
		    const gs_fixed_edge * left, const gs_fixed_edge * right,
			  fixed ybot, fixed ytop, bool swap_axes,
	       const gx_drawing_color * pdcolor, gs_logical_operation_t lop)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    dev_proc_fill_trapezoid((*proc));

    if (tdev == 0)
	tdev = dev, proc = gx_default_fill_trapezoid;
    else
	proc = dev_proc(tdev, fill_trapezoid);
    return (*proc) (tdev, left, right, ybot, ytop, swap_axes, pdcolor, lop);
}

int
gx_forward_fill_parallelogram(gx_device * dev,
		 fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
	       const gx_drawing_color * pdcolor, gs_logical_operation_t lop)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    dev_proc_fill_parallelogram((*proc));

    if (tdev == 0)
	tdev = dev, proc = gx_default_fill_parallelogram;
    else
	proc = dev_proc(tdev, fill_parallelogram);
    return (*proc) (tdev, px, py, ax, ay, bx, by, pdcolor, lop);
}

int
gx_forward_fill_triangle(gx_device * dev,
		 fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
	       const gx_drawing_color * pdcolor, gs_logical_operation_t lop)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    dev_proc_fill_triangle((*proc));

    if (tdev == 0)
	tdev = dev, proc = gx_default_fill_triangle;
    else
	proc = dev_proc(tdev, fill_triangle);
    return (*proc) (tdev, px, py, ax, ay, bx, by, pdcolor, lop);
}

int
gx_forward_draw_thin_line(gx_device * dev,
			  fixed fx0, fixed fy0, fixed fx1, fixed fy1,
	       const gx_drawing_color * pdcolor, gs_logical_operation_t lop)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    dev_proc_draw_thin_line((*proc));

    if (tdev == 0)
	tdev = dev, proc = gx_default_draw_thin_line;
    else
	proc = dev_proc(tdev, draw_thin_line);
    return (*proc) (tdev, fx0, fy0, fx1, fy1, pdcolor, lop);
}

int
gx_forward_begin_image(gx_device * dev,
		       const gs_imager_state * pis, const gs_image_t * pim,
		       gs_image_format_t format, const gs_int_rect * prect,
	      const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
		       gs_memory_t * memory, gx_image_enum_common_t ** pinfo)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    dev_proc_begin_image((*proc));

    if (tdev == 0)
	tdev = dev, proc = gx_default_begin_image;
    else
	proc = dev_proc(tdev, begin_image);
    return (*proc) (tdev, pis, pim, format, prect, pdcolor, pcpath,
		    memory, pinfo);
}

int
gx_forward_strip_tile_rectangle(gx_device * dev, const gx_strip_bitmap * tiles,
   int x, int y, int w, int h, gx_color_index color0, gx_color_index color1,
				int px, int py)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    dev_proc_strip_tile_rectangle((*proc));

    if (tdev == 0)
	tdev = dev, proc = gx_default_strip_tile_rectangle;
    else
	proc = dev_proc(tdev, strip_tile_rectangle);
    return (*proc) (tdev, tiles, x, y, w, h, color0, color1, px, py);
}

int
gx_forward_strip_copy_rop(gx_device * dev,
	     const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
			  const gx_color_index * scolors,
	   const gx_strip_bitmap * textures, const gx_color_index * tcolors,
			  int x, int y, int width, int height,
		       int phase_x, int phase_y, gs_logical_operation_t lop)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    dev_proc_strip_copy_rop((*proc));

    if (tdev == 0)
	tdev = dev, proc = gx_default_strip_copy_rop;
    else
	proc = dev_proc(tdev, strip_copy_rop);
    return (*proc) (tdev, sdata, sourcex, sraster, id, scolors,
		    textures, tcolors, x, y, width, height,
		    phase_x, phase_y, lop);
}

void
gx_forward_get_clipping_box(gx_device * dev, gs_fixed_rect * pbox)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    if (tdev == 0)
	gx_default_get_clipping_box(dev, pbox);
    else
	(*dev_proc(tdev, get_clipping_box)) (tdev, pbox);
}

int
gx_forward_begin_typed_image(gx_device * dev,
			const gs_imager_state * pis, const gs_matrix * pmat,
		   const gs_image_common_t * pim, const gs_int_rect * prect,
	      const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
		      gs_memory_t * memory, gx_image_enum_common_t ** pinfo)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    dev_proc_begin_typed_image((*proc));

    if (tdev == 0)
	tdev = dev, proc = gx_default_begin_typed_image;
    else
	proc = dev_proc(tdev, begin_typed_image);
    return (*proc) (tdev, pis, pmat, pim, prect, pdcolor, pcpath,
		    memory, pinfo);
}

int
gx_forward_get_bits_rectangle(gx_device * dev, const gs_int_rect * prect,
		       gs_get_bits_params_t * params, gs_int_rect ** unread)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    dev_proc_get_bits_rectangle((*proc));

    if (tdev == 0)
	tdev = dev, proc = gx_default_get_bits_rectangle;
    else
	proc = dev_proc(tdev, get_bits_rectangle);
    return (*proc) (tdev, prect, params, unread);
}

int
gx_forward_map_color_rgb_alpha(gx_device * dev, gx_color_index color,
			       gx_color_value prgba[4])
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_default_map_color_rgb_alpha(dev, color, prgba) :
	    (*dev_proc(tdev, map_color_rgb_alpha)) (tdev, color, prgba));
}

int
gx_forward_get_hardware_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_default_get_hardware_params(dev, plist) :
	    (*dev_proc(tdev, get_hardware_params)) (tdev, plist));
}

int
gx_forward_text_begin(gx_device * dev, gs_imager_state * pis,
		      const gs_text_params_t * text, const gs_font * font,
gx_path * path, const gx_device_color * pdcolor, const gx_clip_path * pcpath,
		      gs_memory_t * memory, gs_text_enum_t ** ppenum)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    dev_proc_text_begin((*proc));

    if (tdev == 0)
	tdev = dev, proc = gx_default_text_begin;
    else
	proc = dev_proc(tdev, text_begin);
    return (*proc) (tdev, pis, text, font, path, pdcolor, pcpath,
		    memory, ppenum);
}

/* ---------------- The null device(s) ---------------- */

private dev_proc_fill_rectangle(null_fill_rectangle);
private dev_proc_copy_mono(null_copy_mono);
private dev_proc_copy_color(null_copy_color);
private dev_proc_put_params(null_put_params);
private dev_proc_copy_alpha(null_copy_alpha);
private dev_proc_copy_rop(null_copy_rop);
private dev_proc_fill_path(null_fill_path);
private dev_proc_stroke_path(null_stroke_path);
private dev_proc_fill_trapezoid(null_fill_trapezoid);
private dev_proc_fill_parallelogram(null_fill_parallelogram);
private dev_proc_fill_triangle(null_fill_triangle);
private dev_proc_draw_thin_line(null_draw_thin_line);

/* We would like to have null implementations of begin/data/end image, */
/* but we can't do this, because image_data must keep track of the */
/* Y position so it can return 1 when done. */
private dev_proc_strip_copy_rop(null_strip_copy_rop);

#define null_procs(get_page_device) {\
	gx_default_open_device,\
	gx_forward_get_initial_matrix,\
	gx_default_sync_output,\
	gx_default_output_page,\
	gx_default_close_device,\
	gx_forward_map_rgb_color,\
	gx_forward_map_color_rgb,\
	null_fill_rectangle,\
	gx_default_tile_rectangle,\
	null_copy_mono,\
	null_copy_color,\
	gx_default_draw_line,\
	gx_default_get_bits,\
	gx_forward_get_params,\
	null_put_params,\
	gx_forward_map_cmyk_color,\
	gx_forward_get_xfont_procs,\
	gx_forward_get_xfont_device,\
	gx_forward_map_rgb_alpha_color,\
	get_page_device,	/* differs */\
	gx_forward_get_alpha_bits,\
	null_copy_alpha,\
	gx_forward_get_band,\
	null_copy_rop,\
	null_fill_path,\
	null_stroke_path,\
	gx_default_fill_mask,\
	null_fill_trapezoid,\
	null_fill_parallelogram,\
	null_fill_triangle,\
	null_draw_thin_line,\
	gx_default_begin_image,\
	gx_default_image_data,\
	gx_default_end_image,\
	gx_default_strip_tile_rectangle,\
	null_strip_copy_rop,\
	gx_default_get_clipping_box,\
	gx_default_begin_typed_image,\
	gx_default_get_bits_rectangle,\
	gx_forward_map_color_rgb_alpha,\
	gx_non_imaging_create_compositor,\
	gx_forward_get_hardware_params,\
	gx_default_text_begin\
}

const gx_device_null gs_null_device =
{
    std_device_std_body_type_open(gx_device_null, 0, "null", &st_device_null,
				  0, 0, 72, 72),
    null_procs(gx_default_get_page_device /* not a page device */ ),
    0				/* target */
};

const gx_device_null gs_nullpage_device =
{
std_device_std_body_type_open(gx_device_null, 0, "nullpage", &st_device_null,
			      72 /*nominal */ , 72 /*nominal */ , 72, 72),
    null_procs(gx_page_device_get_page_device /* a page device */ ),
    0				/* target */
};

private int
null_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
		    gx_color_index color)
{
    return 0;
}
private int
null_copy_mono(gx_device * dev, const byte * data,
	    int dx, int raster, gx_bitmap_id id, int x, int y, int w, int h,
	       gx_color_index zero, gx_color_index one)
{
    return 0;
}
private int
null_copy_color(gx_device * dev, const byte * data,
		int data_x, int raster, gx_bitmap_id id,
		int x, int y, int width, int height)
{
    return 0;
}
private int
null_put_params(gx_device * dev, gs_param_list * plist)
{
    /*
     * If this is not a page device, we must defeat attempts to reset
     * the size; otherwise this is equivalent to gx_forward_put_params.
     */
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    int code;

    if (tdev != 0)
	return (*dev_proc(tdev, put_params)) (tdev, plist);
    code = gx_default_put_params(dev, plist);
    if (code < 0 || (*dev_proc(dev, get_page_device)) (dev) == dev)
	return code;
    dev->width = dev->height = 0;
    return code;
}
private int
null_copy_alpha(gx_device * dev, const byte * data, int data_x,
	   int raster, gx_bitmap_id id, int x, int y, int width, int height,
		gx_color_index color, int depth)
{
    return 0;
}
private int
null_copy_rop(gx_device * dev,
	      const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
	      const gx_color_index * scolors,
	      const gx_tile_bitmap * texture, const gx_color_index * tcolors,
	      int x, int y, int width, int height,
	      int phase_x, int phase_y, gs_logical_operation_t lop)
{
    return 0;
}
private int
null_fill_path(gx_device * dev, const gs_imager_state * pis,
	       gx_path * ppath, const gx_fill_params * params,
	       const gx_drawing_color * pdcolor, const gx_clip_path * pcpath)
{
    return 0;
}
private int
null_stroke_path(gx_device * dev, const gs_imager_state * pis,
		 gx_path * ppath, const gx_stroke_params * params,
	      const gx_drawing_color * pdcolor, const gx_clip_path * pcpath)
{
    return 0;
}
private int
null_fill_trapezoid(gx_device * dev,
		    const gs_fixed_edge * left, const gs_fixed_edge * right,
		    fixed ybot, fixed ytop, bool swap_axes,
	       const gx_drawing_color * pdcolor, gs_logical_operation_t lop)
{
    return 0;
}
private int
null_fill_parallelogram(gx_device * dev,
		 fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
	       const gx_drawing_color * pdcolor, gs_logical_operation_t lop)
{
    return 0;
}
private int
null_fill_triangle(gx_device * dev,
		 fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
	       const gx_drawing_color * pdcolor, gs_logical_operation_t lop)
{
    return 0;
}
private int
null_draw_thin_line(gx_device * dev,
		    fixed fx0, fixed fy0, fixed fx1, fixed fy1,
	       const gx_drawing_color * pdcolor, gs_logical_operation_t lop)
{
    return 0;
}
private int
null_strip_copy_rop(gx_device * dev,
	     const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
		    const gx_color_index * scolors,
	   const gx_strip_bitmap * textures, const gx_color_index * tcolors,
		    int x, int y, int width, int height,
		    int phase_x, int phase_y, gs_logical_operation_t lop)
{
    return 0;
}
