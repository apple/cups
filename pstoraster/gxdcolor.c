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

/*$Id: gxdcolor.c,v 1.2 2000/03/08 23:14:56 mike Exp $ */
/* Pure and null device color implementation */
#include "gx.h"
#include "gserrors.h"
#include "gsbittab.h"
#include "gxdcolor.h"
#include "gxdevice.h"

/* Define the standard device color types. */

/* 'none' means the color is not defined. */
private dev_color_proc_load(gx_dc_no_load);
private dev_color_proc_fill_rectangle(gx_dc_no_fill_rectangle);
private dev_color_proc_fill_masked(gx_dc_no_fill_masked);
private dev_color_proc_equal(gx_dc_no_equal);
const gx_device_color_type_t gx_dc_type_data_none = {
    &st_bytes,
    gx_dc_no_load, gx_dc_no_fill_rectangle, gx_dc_no_fill_masked,
    gx_dc_no_equal
};
#undef gx_dc_type_none
const gx_device_color_type_t *const gx_dc_type_none = &gx_dc_type_data_none;
#define gx_dc_type_none (&gx_dc_type_data_none)

/* 'null' means the color has no effect when used for drawing. */
private dev_color_proc_load(gx_dc_null_load);
private dev_color_proc_fill_rectangle(gx_dc_null_fill_rectangle);
private dev_color_proc_fill_masked(gx_dc_null_fill_masked);
private dev_color_proc_equal(gx_dc_null_equal);
const gx_device_color_type_t gx_dc_type_data_null = {
    &st_bytes,
    gx_dc_null_load, gx_dc_null_fill_rectangle, gx_dc_null_fill_masked,
    gx_dc_null_equal
};
#undef gx_dc_type_null
const gx_device_color_type_t *const gx_dc_type_null = &gx_dc_type_data_null;
#define gx_dc_type_null (&gx_dc_type_data_null)

private dev_color_proc_load(gx_dc_pure_load);
private dev_color_proc_fill_rectangle(gx_dc_pure_fill_rectangle);
private dev_color_proc_fill_masked(gx_dc_pure_fill_masked);
private dev_color_proc_equal(gx_dc_pure_equal);
const gx_device_color_type_t gx_dc_type_data_pure = {
    &st_bytes,
    gx_dc_pure_load, gx_dc_pure_fill_rectangle, gx_dc_pure_fill_masked,
    gx_dc_pure_equal
};
#undef gx_dc_type_pure
const gx_device_color_type_t *const gx_dc_type_pure = &gx_dc_type_data_pure;
#define gx_dc_type_pure (&gx_dc_type_data_pure)

/*
 * Get the black and white pixel values of a device.  The documentation for
 * the driver API says that map_rgb_color will do the right thing on CMYK
 * devices.  Unfortunately, that isn't true at present, and fixing it is too
 * much work.
 */
gx_color_index
gx_device_black(gx_device *dev)
{
    return
	(dev->color_info.num_components == 4 ?
	 (*dev_proc(dev, map_cmyk_color))
	  (dev, (gx_color_index)0, (gx_color_index)0, (gx_color_index)0,
	   gx_max_color_value) :
	 (*dev_proc(dev, map_rgb_color))
	  (dev, (gx_color_index)0, (gx_color_index)0, (gx_color_index)0));
}
gx_color_index
gx_device_white(gx_device *dev)
{
    return
	(dev->color_info.num_components == 4 ?
	 (*dev_proc(dev, map_cmyk_color))
	  (dev, (gx_color_index)0, (gx_color_index)0, (gx_color_index)0,
	   (gx_color_index)0) :
	 (*dev_proc(dev, map_rgb_color))
	  (dev, gx_max_color_value, gx_max_color_value, gx_max_color_value));
}

/* Set a null RasterOp source. */
private const gx_rop_source_t gx_rop_no_source_0 = {gx_rop_no_source_body(0)};
void
gx_set_rop_no_source(const gx_rop_source_t **psource,
		     gx_rop_source_t *pno_source, gx_device *dev)
{
    gx_color_index black = gx_device_black(dev);

    if ( black == 0 )
	*psource = &gx_rop_no_source_0;
    else {
	*pno_source = gx_rop_no_source_0;
        gx_rop_source_set_color(pno_source, black);
	*psource = pno_source;
    }
}

/* ------ Undefined color ------ */

private int
gx_dc_no_load(gx_device_color *pdevc, const gs_imager_state *ignore_pis,
	      gx_device *ignore_dev, gs_color_select_t ignore_select)
{
    return 0;
}

private int
gx_dc_no_fill_rectangle(const gx_device_color *pdevc, int x, int y,
			int w, int h, gx_device *dev,
			gs_logical_operation_t lop,
			const gx_rop_source_t *source)
{
    gx_device_color filler;

    if (w <= 0 || h <= 0)
	return 0;
    if (lop_uses_T(lop))
	return_error(gs_error_Fatal);
    color_set_pure(&filler, 0);	 /* any valid value for dev will do */
    return gx_dc_pure_fill_rectangle(&filler, x, y, w, h, dev, lop, source);
}

private int
gx_dc_no_fill_masked(const gx_device_color *pdevc, const byte *data,
		     int data_x, int raster, gx_bitmap_id id,
		     int x, int y, int w, int h, gx_device *dev,
		     gs_logical_operation_t lop, bool invert)
{
    if (w <= 0 || h <= 0)
	return 0;
    return_error(gs_error_Fatal);
}

private bool
gx_dc_no_equal(const gx_device_color *pdevc1, const gx_device_color *pdevc2)
{
    return false;
}

/* ------ Null color ------ */

private int
gx_dc_null_load(gx_device_color *pdevc, const gs_imager_state *ignore_pis,
		gx_device *ignore_dev, gs_color_select_t ignore_select)
{
    return 0;
}

private int
gx_dc_null_fill_rectangle(const gx_device_color * pdevc, int x, int y,
			  int w, int h, gx_device * dev,
			  gs_logical_operation_t lop,
			  const gx_rop_source_t * source)
{
    return 0;
}

private int
gx_dc_null_fill_masked(const gx_device_color * pdevc, const byte * data,
		       int data_x, int raster, gx_bitmap_id id,
		       int x, int y, int w, int h, gx_device * dev,
		       gs_logical_operation_t lop, bool invert)
{
    return 0;
}

private bool
gx_dc_null_equal(const gx_device_color * pdevc1, const gx_device_color * pdevc2)
{
    return pdevc2->type == pdevc1->type;
}

/* ------ Pure color ------ */

private int
gx_dc_pure_load(gx_device_color * pdevc, const gs_imager_state * ignore_pis,
		gx_device * ignore_dev, gs_color_select_t ignore_select)
{
    return 0;
}

/* Fill a rectangle with a pure color. */
/* Note that we treat this as "texture" for RasterOp. */
private int
gx_dc_pure_fill_rectangle(const gx_device_color * pdevc, int x, int y,
		  int w, int h, gx_device * dev, gs_logical_operation_t lop,
			  const gx_rop_source_t * source)
{
    if (source == NULL && lop_no_S_is_T(lop))
	return (*dev_proc(dev, fill_rectangle)) (dev, x, y, w, h,
						 pdevc->colors.pure);
    {
	gx_color_index colors[2];
	gx_rop_source_t no_source;

	colors[0] = colors[1] = pdevc->colors.pure;
	if (source == NULL)
	    set_rop_no_source(source, no_source, dev);
	return (*dev_proc(dev, strip_copy_rop))
	    (dev, source->sdata, source->sourcex, source->sraster,
	     source->id, (source->use_scolors ? source->scolors : NULL),
	     NULL /*arbitrary */ , colors, x, y, w, h, 0, 0, lop);
    }
}

/* Fill a mask with a pure color. */
/* Note that there is no source in this case: the mask is the source. */
private int
gx_dc_pure_fill_masked(const gx_device_color * pdevc, const byte * data,
	int data_x, int raster, gx_bitmap_id id, int x, int y, int w, int h,
		   gx_device * dev, gs_logical_operation_t lop, bool invert)
{
    if (lop_no_S_is_T(lop)) {
	gx_color_index color0, color1;

	if (invert)
	    color0 = pdevc->colors.pure, color1 = gx_no_color_index;
	else
	    color1 = pdevc->colors.pure, color0 = gx_no_color_index;
	return (*dev_proc(dev, copy_mono))
	    (dev, data, data_x, raster, id, x, y, w, h, color0, color1);
    } {
	gx_color_index scolors[2];
	gx_color_index tcolors[2];

	scolors[0] = gx_device_black(dev);
	scolors[1] = gx_device_white(dev);
	tcolors[0] = tcolors[1] = pdevc->colors.pure;
	return (*dev_proc(dev, strip_copy_rop))
	    (dev, data, data_x, raster, id, scolors,
	     NULL, tcolors, x, y, w, h, 0, 0,
	     (invert ? rop3_invert_S(lop) : lop) | lop_S_transparent);
    }
}

private bool
gx_dc_pure_equal(const gx_device_color * pdevc1, const gx_device_color * pdevc2)
{
    return pdevc2->type == pdevc1->type &&
	gx_dc_pure_color(pdevc1) == gx_dc_pure_color(pdevc2);
}

/* ------ Default implementations ------ */

/* Fill a mask with a color by parsing the mask into rectangles. */
int
gx_dc_default_fill_masked(const gx_device_color * pdevc, const byte * data,
	int data_x, int raster, gx_bitmap_id id, int x, int y, int w, int h,
		   gx_device * dev, gs_logical_operation_t lop, bool invert)
{
    int lbit = data_x & 7;
    const byte *row = data + (data_x >> 3);
    uint one = (invert ? 0 : 0xff);
    uint zero = one ^ 0xff;
    int iy;

    for (iy = 0; iy < h; ++iy, row += raster) {
	const byte *p = row;
	int bit = lbit;
	int left = w;
	int l0;

	while (left) {
	    int run, code;

	    /* Skip a run of zeros. */
	    run = byte_bit_run_length[bit][*p ^ one];
	    if (run) {
		if (run < 8) {
		    if (run >= left)
			break;	/* end of row while skipping */
		    bit += run, left -= run;
		} else if ((run -= 8) >= left)
		    break;	/* end of row while skipping */
		else {
		    left -= run;
		    ++p;
		    while (left > 8 && *p == zero)
			left -= 8, ++p;
		    run = byte_bit_run_length_0[*p ^ one];
		    if (run >= left)	/* run < 8 unless very last byte */
			break;	/* end of row while skipping */
		    else
			bit = run & 7, left -= run;
		}
	    }
	    l0 = left;
	    /* Scan a run of ones, and then paint it. */
	    run = byte_bit_run_length[bit][*p ^ zero];
	    if (run < 8) {
		if (run >= left)
		    left = 0;
		else
		    bit += run, left -= run;
	    } else if ((run -= 8) >= left)
		left = 0;
	    else {
		left -= run;
		++p;
		while (left > 8 && *p == one)
		    left -= 8, ++p;
		run = byte_bit_run_length_0[*p ^ zero];
		if (run >= left)	/* run < 8 unless very last byte */
		    left = 0;
		else
		    bit = run & 7, left -= run;
	    }
	    code = gx_device_color_fill_rectangle(pdevc,
			  x + w - l0, y + iy, l0 - left, 1, dev, lop, NULL);
	    if (code < 0)
		return code;
	}
    }
    return 0;
}
