/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevcgm.c */
/* CGM (Computer Graphics Metafile) driver */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gsparam.h"
#include "gdevcgml.h"
#include "gdevpccm.h"

/**************** Future optimizations:
	Do tile_rectangle with pattern
	Keep track of painted area,
	  do masked copy_mono with cell array if possible
 ****************/

#define fname_size 80

typedef struct gx_device_cgm_s {
	gx_device_common;
	char fname[fname_size + 1];
	FILE *file;
	cgm_state *st;
	bool in_picture;
} gx_device_cgm;
/* GC descriptor */
gs_private_st_suffix_add1_final(st_device_cgm, gx_device_cgm,
  "gx_device_cgm", device_cgm_enum_ptrs, device_cgm_reloc_ptrs,
  gx_device_finalize, st_device, st);

/* Device procedures */
private dev_proc_open_device(cgm_open);
private dev_proc_output_page(cgm_output_page);
private dev_proc_close_device(cgm_close);
private dev_proc_fill_rectangle(cgm_fill_rectangle);
#if 0
private dev_proc_tile_rectangle(cgm_tile_rectangle);
#else
#define cgm_tile_rectangle NULL
#endif
private dev_proc_copy_mono(cgm_copy_mono);
private dev_proc_copy_color(cgm_copy_color);
private dev_proc_get_params(cgm_get_params);
private dev_proc_put_params(cgm_put_params);

/* In principle, all the drawing operations should be polymorphic, */
/* but it's just as easy just to test the depth, since we're not */
/* very concerned about performance. */
#define cgm_device(dname, depth, max_value, dither, map_rgb_color, map_color_rgb)\
{	std_device_color_stype_body(gx_device_cgm, 0, dname, &st_device_cgm,\
	  850, 1100, 100, 100, depth, max_value, dither),\
	{	cgm_open,\
		NULL,			/* get_initial_matrix */\
		NULL,			/* sync_output */\
		cgm_output_page,\
		cgm_close,\
		map_rgb_color,\
		map_color_rgb,\
		cgm_fill_rectangle,\
		cgm_tile_rectangle,\
		cgm_copy_mono,\
		cgm_copy_color,\
		NULL,			/* draw_line */\
		NULL,			/* get_bits */\
		cgm_get_params,\
		cgm_put_params\
	},\
	 { 0 },		/* fname */\
	0,		/* file */\
	0,		/* st */\
	0 /*false*/	/* in_picture */\
}

gx_device_cgm far_data gs_cgmmono_device =
  cgm_device("cgmmono", 1, 1, 2,
    gx_default_map_rgb_color, gx_default_w_b_map_color_rgb);

gx_device_cgm far_data gs_cgm8_device =
  cgm_device("cgm8", 8, 6, 7,
    pc_8bit_map_rgb_color, pc_8bit_map_color_rgb);

gx_device_cgm far_data gs_cgm24_device =
  cgm_device("cgm24", 24, 255, 255,
    gx_default_rgb_map_rgb_color, gx_default_rgb_map_color_rgb);

/* Define allocator procedures for the CGM library. */
private void *
cgm_gs_alloc(void *private_data, uint size)
{	gx_device_cgm *cdev = private_data;
	gs_memory_t *mem =
	  (cdev->memory == 0 ? &gs_memory_default : cdev->memory);
	return gs_alloc_bytes(mem, size, "cgm_gs_alloc");
}
private void
cgm_gs_free(void *private_data, void *obj)
{	gx_device_cgm *cdev = private_data;
	gs_memory_t *mem =
	  (cdev->memory == 0 ? &gs_memory_default : cdev->memory);
	gs_free_object(mem, obj, "cgm_gs_free");
}

/* ---------------- Utilities ---------------- */

/* Convert a CGM result code to our error values. */
private int near
cgm_error_code(cgm_result result)
{	switch ( result )
	  {
	  default:
	  case cgm_result_wrong_state: return gs_error_unknownerror;
	  case cgm_result_out_of_range: return gs_error_rangecheck;
	  case cgm_result_io_error: return gs_error_ioerror;
	  }
}
#define check_result(result)\
  if ( result != cgm_result_ok ) return_error(cgm_error_code(result))

/* ---------------- Device control ---------------- */

/* Open the device */
private int
cgm_open(gx_device *dev)
{	gx_device_cgm *cdev = (gx_device_cgm *)dev;
	cgm_allocator cal;
	static const int elements[] = { -1, 1 };
	cgm_metafile_elements meta;
	cgm_result result;

	cdev->file = fopen(cdev->fname, "wb");
	if ( cdev->file == 0 )
	  return_error(gs_error_ioerror);
	cal.private_data = cdev;
	cal.alloc = cgm_gs_alloc;
	cal.free = cgm_gs_free;
	cdev->st = cgm_initialize(cdev->file, &cal);
	if ( cdev->st == 0 )
	  return_error(gs_error_VMerror);
	result = cgm_BEGIN_METAFILE(cdev->st, "", 0);
	check_result(result);
	meta.metafile_version = 1;
	meta.vdc_type = cgm_vdc_integer;
	meta.integer_precision = sizeof(cgm_int) * 8;
	meta.index_precision = sizeof(cgm_int) * 8;
	meta.color_precision = 8;
	/* If we use color indices at all, they are only 1 byte. */
	meta.color_index_precision = 8;
	meta.maximum_color_index = (1L << cdev->color_info.depth) - 1;
	meta.metafile_element_list = elements,
	  meta.metafile_element_list_count = countof(elements) / 2;
	result = cgm_set_metafile_elements(cdev->st, &meta,
					   cgm_set_METAFILE_VERSION |
					   cgm_set_VDC_TYPE |
					   cgm_set_INTEGER_PRECISION |
					   cgm_set_INDEX_PRECISION |
					   cgm_set_COLOR_PRECISION |
					   cgm_set_COLOR_INDEX_PRECISION |
					   cgm_set_MAXIMUM_COLOR_INDEX |
					   cgm_set_METAFILE_ELEMENT_LIST);
	check_result(result);
	cdev->in_picture = false;
	return 0;
}

/* Output a page */
private int
cgm_output_page(gx_device *dev, int num_copies, int flush)
{	gx_device_cgm *cdev = (gx_device_cgm *)dev;
	if ( cdev->in_picture )
	{	cgm_result result = cgm_END_PICTURE(cdev->st);
		check_result(result);
		cdev->in_picture = false;
	}
	return 0;
}

/* Close the device */
private int
cgm_close(gx_device *dev)
{	gx_device_cgm *cdev = (gx_device_cgm *)dev;
	int code = cgm_output_page(dev, 1, 0);
	cgm_result result;

	if ( code < 0 )
	  return code;
	result = cgm_END_METAFILE(cdev->st);
	check_result(result);
	result = cgm_terminate(cdev->st);
	check_result(result);
	cdev->st = 0;
	fclose(cdev->file);
	cdev->file = 0;
	return 0;
}

/* Get parameters.  CGM devices add OutputFile to the default set. */
private int
cgm_get_params(gx_device *dev, gs_param_list *plist)
{	gx_device_cgm *cdev = (gx_device_cgm *)dev;
	int code = gx_default_get_params(dev, plist);
	gs_param_string ofns;

	if ( code < 0 ) return code;
	ofns.data = (const byte *)cdev->fname,
	  ofns.size = strlen(cdev->fname),
	  ofns.persistent = false;
	return param_write_string(plist, "OutputFile", &ofns);
}

/* Put parameters. */
private int
cgm_put_params(gx_device *dev, gs_param_list *plist)
{	gx_device_cgm *cdev = (gx_device_cgm *)dev;
	int ecode = 0;
	int code;
	const char _ds *param_name;
	gs_param_string ofs;

	switch ( code = param_read_string(plist, (param_name = "OutputFile"), &ofs) )
	{
	case 0:
		if ( ofs.size > fname_size )
		  ecode = gs_error_limitcheck;
		else
		  break;
		goto ofe;
	default:
		ecode = code;
ofe:		param_signal_error(plist, param_name, ecode);
	case 1:
		ofs.data = 0;
		break;
	}

	if ( ecode < 0 )
	  return ecode;
	code = gx_default_put_params(dev, plist);
	if ( code < 0 )
	  return code;

	if ( ofs.data != 0 )
	  {	/* Close the file if it's open. */
		if ( cdev->file != 0 )
		  {	fclose(cdev->file);
			cdev->file = 0;
		  }
		memcpy(cdev->fname, ofs.data, ofs.size);
		cdev->fname[ofs.size] = 0;
		cdev->file = fopen(cdev->fname, "wb");
		if ( cdev->file == 0 )
		  return_error(gs_error_ioerror);
	  }

	return 0;
}

/* ---------------- Drawing ---------------- */

/* Set the corner points for a rectangle.  It appears (although */
/* this is not obvious from the CGM specification) that rectangles */
/* are specified with closed, rather than half-open, intervals. */
#define cgm_set_rect(points, xo, yo, w, h)\
  points[1].integer.x = (points[0].integer.x = xo) + (w) - 1,\
  points[1].integer.y = (points[0].integer.y = yo) + (h) - 1

/* Set the points for a cell array. */
#define cgm_set_cell_points(pqr, xo, yo, w, h)\
  pqr[0].integer.x = (xo),\
  pqr[0].integer.y = (yo),\
  pqr[1].integer.x = (xo) + (w),\
  pqr[1].integer.y = (yo) + (h),\
  pqr[2].integer.x = (xo) + (w),\
  pqr[2].integer.y = (yo)

/* Begin a picture if necessary. */
#define begin_picture(cdev)\
  if ( !cdev->in_picture ) cgm_begin_picture(cdev)
private int
cgm_begin_picture(gx_device_cgm *cdev)
{	cgm_picture_elements pic;
	cgm_result result;
	cgm_edge_width edge;

	result = cgm_BEGIN_PICTURE(cdev->st, "", 0);
	check_result(result);
	pic.scaling_mode = cgm_scaling_abstract;
	pic.color_selection_mode =
	  (cdev->color_info.depth <= 8 ?
	   cgm_color_selection_indexed :
	   cgm_color_selection_direct);
	pic.line_width_specification_mode = cgm_line_marker_absolute;
	pic.edge_width_specification_mode = cgm_line_marker_absolute;
	cgm_set_rect(pic.vdc_extent, 0, 0, cdev->width, cdev->height);
	result = cgm_set_picture_elements(cdev->st, &pic,
					  cgm_set_SCALING_MODE |
					  cgm_set_COLOR_SELECTION_MODE |
					  cgm_set_LINE_WIDTH_SPECIFICATION_MODE |
					  cgm_set_EDGE_WIDTH_SPECIFICATION_MODE |
					  cgm_set_VDC_EXTENT);
	check_result(result);
	result = cgm_BEGIN_PICTURE_BODY(cdev->st);
	check_result(result);
	result = cgm_VDC_INTEGER_PRECISION(cdev->st,
					   (cdev->width <= 0x7fff &&
					    cdev->height <= 0x7fff ?
					    16 : sizeof(cdev->width) * 8));
	check_result(result);
	edge.absolute.integer = 0;
	result = cgm_EDGE_WIDTH(cdev->st, &edge);
	check_result(result);
	if ( cdev->color_info.depth <= 8 )
	{	cgm_color colors[256];
		int i;
		for ( i = 0; i < (1 << cdev->color_info.depth); i++ )
		{	gx_color_value rgb[3];
			(*dev_proc(cdev, map_color_rgb))((gx_device *)cdev,
						(gx_color_index)i, rgb);
			colors[i].rgb.r =
			  rgb[0] >> (gx_color_value_bits - 8);
			colors[i].rgb.g =
			  rgb[1] >> (gx_color_value_bits - 8);
			colors[i].rgb.b =
			  rgb[2] >> (gx_color_value_bits - 8);
		}
		result = cgm_COLOR_TABLE(cdev->st, 0, colors,
					 1 << cdev->color_info.depth);
		check_result(result);
	}
	cdev->in_picture = true;
	return 0;
}

/* Convert a gx_color_index to a CGM color. */
private void
cgm_color_from_color_index(cgm_color *pcc, const gx_device_cgm *cdev,
  gx_color_index color)
{	if ( cdev->color_info.depth <= 8 )
		pcc->index = color;
	else
	{	pcc->rgb.r = color >> 16;
		pcc->rgb.g = (color >> 8) & 255;
		pcc->rgb.b = color & 255;
	}
}

/* Fill a rectangle. */
private int
cgm_fill_rectangle(gx_device *dev, int x, int y, int w, int h,
  gx_color_index color)
{	gx_device_cgm *cdev = (gx_device_cgm *)dev;
	cgm_color fill_color;
	cgm_point points[2];
	cgm_result result;

	fit_fill(dev, x, y, w, h);
	if ( !cdev->in_picture )
	  {	/* Check for erasepage. */
		if ( color == (*dev_proc(dev, map_rgb_color))(dev,
					gx_max_color_value, gx_max_color_value,
					gx_max_color_value)
		   )
		  return 0;
		cgm_begin_picture(cdev);
	  }
	cgm_color_from_color_index(&fill_color, cdev, color);
	result = cgm_FILL_COLOR(cdev->st, &fill_color);
	check_result(result);
	result = cgm_INTERIOR_STYLE(cdev->st, cgm_interior_style_solid);
	check_result(result);
	cgm_set_rect(points, x, y, w, h);
	result = cgm_RECTANGLE(cdev->st, &points[0], &points[1]);
	check_result(result);
	return 0;
}

#if 0
/* Tile a rectangle.  We should do this with a pattern if possible. */
private int
cgm_tile_rectangle(gx_device *dev, const gx_tile_bitmap *tile,
  int x, int y, int w, int h, gx_color_index zero, gx_color_index one,
  int px, int py)
{
}
#endif

/* Copy a monochrome bitmap.  Unfortunately, CGM doesn't provide a */
/* masked fill operation; if one of the colors is transparent, */
/* we have to do the copy by filling lots of tiny little rectangles. */
/* A much better way to implement this would be to remember whether */
/* the destination region is still white; if so, we can use a cell array */
/* (or, even better, a pattern).  However, we still need the slow method */
/* for the case where we don't know the background color or it isn't white. */
private int
cgm_copy_mono(gx_device *dev,
  const byte *base, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{	gx_device_cgm *cdev = (gx_device_cgm *)dev;
	/* The current implementation is about as inefficient as */
	/* one could possibly imagine! */
	int ix, iy;
	cgm_result result;

	fit_copy(dev, base, sourcex, raster, id, x, y, w, h);
	begin_picture(cdev);
	if ( zero == 0 && one == 1 && cdev->color_info.depth == 1 )
	  {	cgm_point pqr[3];
		cgm_set_cell_points(pqr, x, y, w, h);
		result = cgm_CELL_ARRAY(cdev->st, pqr, w, h, 1,
					cgm_cell_mode_packed,
					base, sourcex, raster);
		check_result(result);
	  }
	else
	  {	result = cgm_INTERIOR_STYLE(cdev->st, cgm_interior_style_solid);
		check_result(result);
		for ( iy = 0; iy < h; iy++ )
		  for ( ix = 0; ix < w; ix++ )
		    {	int px = ix + sourcex;
			const byte *pixel = &base[iy * raster + (px >> 3)];
			byte mask = 0x80 >> (px & 7);
			gx_color_index color = (*pixel & mask ? one : zero);
			if ( color != gx_no_color_index )
			  {	cgm_color fill_color;
				cgm_point points[2];
				cgm_color_from_color_index(&fill_color, cdev, color);
				cgm_set_rect(points, x, y, 1, 1);
				result = cgm_RECTANGLE(cdev->st, &points[0], &points[1]);
				check_result(result);
			  }
		    }
	  }
	return 0;
}

/* Copy a color bitmap. */
private int
cgm_copy_color(gx_device *dev,
  const byte *base, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h)
{	gx_device_cgm *cdev = (gx_device_cgm *)dev;
	int depth = cdev->color_info.depth;
	uint source_bit = sourcex * depth;
	cgm_point pqr[3];
	cgm_result result;

	if ( depth == 1 )
	  return cgm_copy_mono(dev, base, sourcex, raster, id,
			       x, y, w, h,
			       (gx_color_index)0, (gx_color_index)1);
	fit_copy(dev, base, sourcex, raster, id, x, y, w, h);
	begin_picture(cdev);
	cgm_set_cell_points(pqr, x, y, w, h);
	result = cgm_CELL_ARRAY(cdev->st, pqr, w, h, 0, cgm_cell_mode_packed,
				base, source_bit, raster);
	check_result(result);
	return 0;
}
