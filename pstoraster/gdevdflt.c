/* Copyright (C) 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevdflt.c,v 1.2 2000/03/08 23:14:22 mike Exp $ */
/* Default device implementation */
#include "gx.h"
#include "gserrors.h"
#include "gsropt.h"
#include "gxcomp.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#undef mdev

/* ---------------- Default device procedures ---------------- */

/* Fill in NULL procedures in a device procedure record. */
void
gx_device_fill_in_procs(register gx_device * dev)
{
    gx_device_set_procs(dev);
    fill_dev_proc(dev, open_device, gx_default_open_device);
    fill_dev_proc(dev, get_initial_matrix, gx_default_get_initial_matrix);
    fill_dev_proc(dev, sync_output, gx_default_sync_output);
    fill_dev_proc(dev, output_page, gx_default_output_page);
    fill_dev_proc(dev, close_device, gx_default_close_device);
    fill_dev_proc(dev, map_rgb_color, gx_default_map_rgb_color);
    fill_dev_proc(dev, map_color_rgb, gx_default_map_color_rgb);
    /* NOT fill_rectangle */
    fill_dev_proc(dev, tile_rectangle, gx_default_tile_rectangle);
    fill_dev_proc(dev, copy_mono, gx_default_copy_mono);
    fill_dev_proc(dev, copy_color, gx_default_copy_color);
    fill_dev_proc(dev, obsolete_draw_line, gx_default_draw_line);
    fill_dev_proc(dev, get_bits, gx_default_get_bits);
    fill_dev_proc(dev, get_params, gx_default_get_params);
    fill_dev_proc(dev, put_params, gx_default_put_params);
    fill_dev_proc(dev, map_cmyk_color, gx_default_map_cmyk_color);
    fill_dev_proc(dev, get_xfont_procs, gx_default_get_xfont_procs);
    fill_dev_proc(dev, get_xfont_device, gx_default_get_xfont_device);
    fill_dev_proc(dev, map_rgb_alpha_color, gx_default_map_rgb_alpha_color);
    fill_dev_proc(dev, get_page_device, gx_default_get_page_device);
    fill_dev_proc(dev, get_alpha_bits, gx_default_get_alpha_bits);
    fill_dev_proc(dev, copy_alpha, gx_default_copy_alpha);
    fill_dev_proc(dev, get_band, gx_default_get_band);
    fill_dev_proc(dev, copy_rop, gx_default_copy_rop);
    fill_dev_proc(dev, fill_path, gx_default_fill_path);
    fill_dev_proc(dev, stroke_path, gx_default_stroke_path);
    fill_dev_proc(dev, fill_mask, gx_default_fill_mask);
    fill_dev_proc(dev, fill_trapezoid, gx_default_fill_trapezoid);
    fill_dev_proc(dev, fill_parallelogram, gx_default_fill_parallelogram);
    fill_dev_proc(dev, fill_triangle, gx_default_fill_triangle);
    fill_dev_proc(dev, draw_thin_line, gx_default_draw_thin_line);
    fill_dev_proc(dev, begin_image, gx_default_begin_image);
    /*
     * We always replace image_data and end_image with the new
     * procedures, and, if in a DEBUG configuration, print a warning
     * if the definitions aren't the default ones.
     */
#ifdef DEBUG
#  define CHECK_NON_DEFAULT(proc, default, procname)\
    BEGIN\
	if ( dev_proc(dev, proc) != NULL && dev_proc(dev, proc) != default )\
	    dprintf2("**** Warning: device %s implements obsolete procedure %s\n",\
		     dev->dname, procname);\
    END
#else
#  define CHECK_NON_DEFAULT(proc, default, procname)\
    DO_NOTHING
#endif
    CHECK_NON_DEFAULT(image_data, gx_default_image_data, "image_data");
    set_dev_proc(dev, image_data, gx_default_image_data);
    CHECK_NON_DEFAULT(end_image, gx_default_end_image, "end_image");
    set_dev_proc(dev, end_image, gx_default_end_image);
#undef CHECK_NON_DEFAULT
    fill_dev_proc(dev, strip_tile_rectangle, gx_default_strip_tile_rectangle);
    fill_dev_proc(dev, strip_copy_rop, gx_default_strip_copy_rop);
    fill_dev_proc(dev, get_clipping_box, gx_default_get_clipping_box);
    fill_dev_proc(dev, begin_typed_image, gx_default_begin_typed_image);
    fill_dev_proc(dev, get_bits_rectangle, gx_default_get_bits_rectangle);
    fill_dev_proc(dev, map_color_rgb_alpha, gx_default_map_color_rgb_alpha);
    fill_dev_proc(dev, create_compositor, gx_default_create_compositor);
    fill_dev_proc(dev, get_hardware_params, gx_default_get_hardware_params);
    fill_dev_proc(dev, text_begin, gx_default_text_begin);
}

int
gx_default_open_device(gx_device * dev)
{
    return 0;
}

/* Get the initial matrix for a device with inverted Y. */
/* This includes essentially all printers and displays. */
void
gx_default_get_initial_matrix(gx_device * dev, register gs_matrix * pmat)
{
    pmat->xx = dev->HWResolution[0] / 72.0;	/* x_pixels_per_inch */
    pmat->xy = 0;
    pmat->yx = 0;
    pmat->yy = dev->HWResolution[1] / -72.0;	/* y_pixels_per_inch */
/****** tx/y is WRONG for devices with ******/
/****** arbitrary initial matrix ******/
    pmat->tx = 0;
    pmat->ty = dev->height;
}
/* Get the initial matrix for a device with upright Y. */
/* This includes just a few printers and window systems. */
void
gx_upright_get_initial_matrix(gx_device * dev, register gs_matrix * pmat)
{
    pmat->xx = dev->HWResolution[0] / 72.0;	/* x_pixels_per_inch */
    pmat->xy = 0;
    pmat->yx = 0;
    pmat->yy = dev->HWResolution[1] / 72.0;	/* y_pixels_per_inch */
/****** tx/y is WRONG for devices with ******/
/****** arbitrary initial matrix ******/
    pmat->tx = 0;
    pmat->ty = 0;
}

int
gx_default_sync_output(gx_device * dev)
{
    return 0;
}

int
gx_default_output_page(gx_device * dev, int num_copies, int flush)
{
    return (*dev_proc(dev, sync_output)) (dev);
}

int
gx_default_close_device(gx_device * dev)
{
    return 0;
}

const gx_xfont_procs *
gx_default_get_xfont_procs(gx_device * dev)
{
    return NULL;
}

gx_device *
gx_default_get_xfont_device(gx_device * dev)
{
    return dev;
}

gx_device *
gx_default_get_page_device(gx_device * dev)
{
    return NULL;
}
gx_device *
gx_page_device_get_page_device(gx_device * dev)
{
    return dev;
}

int
gx_default_get_alpha_bits(gx_device * dev, graphics_object_type type)
{
    return 1;
}

int
gx_default_get_band(gx_device * dev, int y, int *band_start)
{
    return 0;
}

void
gx_default_get_clipping_box(gx_device * dev, gs_fixed_rect * pbox)
{
    pbox->p.x = 0;
    pbox->p.y = 0;
    pbox->q.x = int2fixed(dev->width);
    pbox->q.y = int2fixed(dev->height);
}
void
gx_get_largest_clipping_box(gx_device * dev, gs_fixed_rect * pbox)
{
    pbox->p.x = min_fixed;
    pbox->p.y = min_fixed;
    pbox->q.x = max_fixed;
    pbox->q.y = max_fixed;
}

int
gx_no_create_compositor(gx_device * dev, gx_device ** pcdev,
			const gs_composite_t * pcte, const gs_imager_state * pis, gs_memory_t * memory)
{
    return_error(gs_error_unknownerror);	/* not implemented */
}
int
gx_default_create_compositor(gx_device * dev, gx_device ** pcdev,
			     const gs_composite_t * pcte, const gs_imager_state * pis, gs_memory_t * memory)
{
    return (*pcte->type->procs.create_default_compositor)
	(pcte, pcdev, dev, pis, memory);
}
int
gx_non_imaging_create_compositor(gx_device * dev, gx_device ** pcdev,
				 const gs_composite_t * pcte, const gs_imager_state * pis, gs_memory_t * memory)
{
    *pcdev = dev;
    return 0;
}

/* The following is not really a device procedure.  See gxdevice.h. */

/* Create an ordinary memory device for page or band buffering. */
int
gx_default_make_buffer_device(gx_device_memory * mdev,
		       gx_device * target, gs_memory_t * mem, bool for_band)
{
    const gx_device_memory *mdproto =
    gdev_mem_device_for_bits(target->color_info.depth);

    if (mdproto == 0)
	return_error(gs_error_rangecheck);
    if (target == (gx_device *) mdev)
	assign_dev_procs(mdev, mdproto);
    else
	gs_make_mem_device(mdev, mdproto, mem, (for_band ? 1 : 0),
			   (target == (gx_device *) mdev ? 0 : target));
    return 0;
}

/* ---------------- Default per-instance procedures ---------------- */

int
gx_default_install(gx_device * dev, gs_state * pgs)
{
    return 0;
}

int
gx_default_begin_page(gx_device * dev, gs_state * pgs)
{
    return 0;
}

int
gx_default_end_page(gx_device * dev, int reason, gs_state * pgs)
{
    return (reason != 2 ? 1 : 0);
}
