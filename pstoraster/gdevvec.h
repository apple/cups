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

/*$Id: gdevvec.h,v 1.1 2000/03/13 19:00:47 mike Exp $ */
/* Common definitions for "vector" devices */

#ifndef gdevvec_INCLUDED
#  define gdevvec_INCLUDED

#include "gp.h"			/* for gp_file_name_sizeof */
#include "gsropt.h"
#include "gxdevice.h"
#include "gdevbbox.h"
#include "gxiparam.h"
#include "gxistate.h"
#include "stream.h"

/******
 ****** NOTE: EVERYTHING IN THIS FILE IS SUBJECT TO CHANGE WITHOUT NOTICE.
 ****** USE AT YOUR OWN RISK.
 ******/

/*
 * "Vector" devices produce a stream of higher-level drawing commands rather
 * than a raster image.  (We don't like the term "vector", since the command
 * vocabulary typically includes text and raster images as well as actual
 * vectors, but it's widely used in the industry, and we weren't able to
 * find one that read better.)  Some examples of "vector" formats are PDF,
 * PostScript, PCL XL, HP-GL/2 + RTL, CGM, Windows Metafile, and Macintosh
 * PICT.
 *
 * This file extends the basic driver structure with elements likely to be
 * useful to vector devices.  These include:
 *
 *      - Tracking whether any marks have been made on the page;
 *
 *      - Keeping track of the page bounding box;
 *
 *      - A copy of the most recently written current graphics state
 *      parameters;
 *
 *      - An output stream (for drivers that compress or otherwise filter
 *      their output);
 *
 *      - A vector of procedures for writing changes to the graphics state.
 *
 *      - The ability to work with scaled output coordinate systems.
 *
 * We expect to add more elements and procedures as we gain more experience
 * with this kind of driver.
 */

/* ================ Types and structures ================ */

/* Define the abstract type for a vector device. */
typedef struct gx_device_vector_s gx_device_vector;

/* Define the maximum size of the output file name. */
#define fname_size (gp_file_name_sizeof - 1)

/* Define the longest dash pattern we can remember. */
#define max_dash 11

/*
 * Define procedures for writing common output elements.  Not all devices
 * will support all of these elements.  Note that these procedures normally
 * only write out commands, and don't update the driver state itself.  All
 * of them are optional, called only as indicated under the utility
 * procedures below.
 */
typedef enum {
    gx_path_type_none = 0,
    /*
     * All combinations of flags are legal.  Multiple commands are
     * executed in the order fill, stroke, clip.
     */
    gx_path_type_fill = 1,
    gx_path_type_stroke = 2,
    gx_path_type_clip = 4,
    gx_path_type_winding_number = 0,
    gx_path_type_even_odd = 8,
    gx_path_type_rule = gx_path_type_winding_number | gx_path_type_even_odd
} gx_path_type_t;
typedef enum {
    gx_rect_x_first,
    gx_rect_y_first
} gx_rect_direction_t;
typedef struct gx_device_vector_procs_s {
    /* Page management */
    int (*beginpage) (P1(gx_device_vector * vdev));
    /* Imager state */
    int (*setlinewidth) (P2(gx_device_vector * vdev, floatp width));
    int (*setlinecap) (P2(gx_device_vector * vdev, gs_line_cap cap));
    int (*setlinejoin) (P2(gx_device_vector * vdev, gs_line_join join));
    int (*setmiterlimit) (P2(gx_device_vector * vdev, floatp limit));
    int (*setdash) (P4(gx_device_vector * vdev, const float *pattern,
		       uint count, floatp offset));
    int (*setflat) (P2(gx_device_vector * vdev, floatp flatness));
    int (*setlogop) (P3(gx_device_vector * vdev, gs_logical_operation_t lop,
			gs_logical_operation_t diff));
    /* Other state */
    int (*setfillcolor) (P2(gx_device_vector * vdev, const gx_drawing_color * pdc));
    int (*setstrokecolor) (P2(gx_device_vector * vdev, const gx_drawing_color * pdc));
    /* Paths */
    /* dopath and dorect are normally defaulted */
    int (*dopath) (P3(gx_device_vector * vdev, const gx_path * ppath,
		      gx_path_type_t type));
    int (*dorect) (P6(gx_device_vector * vdev, fixed x0, fixed y0, fixed x1,
		      fixed y1, gx_path_type_t type));
    int (*beginpath) (P2(gx_device_vector * vdev, gx_path_type_t type));
    int (*moveto) (P6(gx_device_vector * vdev, floatp x0, floatp y0,
		      floatp x, floatp y, gx_path_type_t type));
    int (*lineto) (P6(gx_device_vector * vdev, floatp x0, floatp y0,
		      floatp x, floatp y, gx_path_type_t type));
    int (*curveto) (P10(gx_device_vector * vdev, floatp x0, floatp y0,
			floatp x1, floatp y1, floatp x2, floatp y2,
			floatp x3, floatp y3, gx_path_type_t type));
    int (*closepath) (P6(gx_device_vector * vdev, floatp x0, floatp y0,
		      floatp x_start, floatp y_start, gx_path_type_t type));
    int (*endpath) (P2(gx_device_vector * vdev, gx_path_type_t type));
} gx_device_vector_procs;

/* Default implementations of procedures */
/* setflat does nothing */
int gdev_vector_setflat(P2(gx_device_vector * vdev, floatp flatness));

/* dopath may call dorect, beginpath, moveto/lineto/curveto/closepath, */
/* endpath */
int gdev_vector_dopath(P3(gx_device_vector * vdev, const gx_path * ppath,
			  gx_path_type_t type));

/* dorect may call beginpath, moveto, lineto, closepath */
int gdev_vector_dorect(P6(gx_device_vector * vdev, fixed x0, fixed y0,
			  fixed x1, fixed y1, gx_path_type_t type));

/* Finally, define the extended device structure. */
#define gx_device_vector_common\
	gx_device_common;\
	gs_memory_t *v_memory;\
		/* Output element writing procedures */\
	const gx_device_vector_procs *vec_procs;\
		/* Output file */\
	char fname[fname_size + 1];\
	FILE *file;\
	stream *strm;\
	byte *strmbuf;\
	uint strmbuf_size;\
		/* Graphics state */\
	gs_imager_state state;\
	float dash_pattern[max_dash];\
	gx_drawing_color fill_color, stroke_color;\
	gs_id no_clip_path_id;	/* indicates no clipping */\
	gs_id clip_path_id;\
		/* Other state */\
	gs_point scale;		/* device coords / scale => output coords */\
	bool in_page;		/* true if any marks on this page */\
	gx_device_bbox *bbox_device;	/* for tracking bounding box */\
		/* Cached values */\
	gx_color_index black, white
#define vdev_proc(vdev, p) ((vdev)->vec_procs->p)

#define vector_initial_values\
	0,		/* v_memory */\
	0,		/* vec_procs */\
	 { 0 },		/* fname */\
	0,		/* file */\
	0,		/* strm */\
	0,		/* strmbuf */\
	0,		/* strmbuf_size */\
	 { 0 },		/* state */\
	 { 0 },		/* dash_pattern */\
	 { 0 },		/* fill_color ****** WRONG ****** */\
	 { 0 },		/* stroke_color ****** WRONG ****** */\
	gs_no_id,	/* clip_path_id */\
	gs_no_id,	/* no_clip_path_id */\
	 { X_DPI/72.0, Y_DPI/72.0 },	/* scale */\
	0/*false*/,	/* in_page */\
	0,		/* bbox_device */\
	gx_no_color_index,	/* black */\
	gx_no_color_index	/* white */

struct gx_device_vector_s {
    gx_device_vector_common;
};

/* st_device_vector is never instantiated per se, but we still need to */
/* extern its descriptor for the sake of subclasses. */
extern_st(st_device_vector);
#define public_st_device_vector()	/* in gdevvec.c */\
  gs_public_st_suffix_add3_final(st_device_vector, gx_device_vector,\
    "gx_device_vector", device_vector_enum_ptrs,\
    device_vector_reloc_ptrs, gx_device_finalize, st_device, strm, strmbuf,\
    bbox_device)
#define st_device_vector_max_ptrs (st_device_max_ptrs + 3)

/* ================ Utility procedures ================ */

/* Initialize the state. */
void gdev_vector_init(P1(gx_device_vector * vdev));

/* Reset the remembered graphics state. */
void gdev_vector_reset(P1(gx_device_vector * vdev));

/* Open the output file and stream, with optional bbox tracking. */
int gdev_vector_open_file_bbox(P3(gx_device_vector * vdev, uint strmbuf_size,
				  bool bbox));

#define gdev_vector_open_file(vdev, strmbuf_size)\
  gdev_vector_open_file_bbox(vdev, strmbuf_size, false)

/* Get the current stream, calling beginpage if in_page is false. */
stream *gdev_vector_stream(P1(gx_device_vector * vdev));

/* Bring the logical operation up to date. */
/* May call setlogop. */
int gdev_vector_update_log_op(P2(gx_device_vector * vdev,
				 gs_logical_operation_t lop));

/* Bring the fill color up to date. */
/* May call setfillcolor. */
int gdev_vector_update_fill_color(P2(gx_device_vector * vdev,
				     const gx_drawing_color * pdcolor));

/* Bring state up to date for filling. */
/* May call setflat, setfillcolor, setlogop. */
int gdev_vector_prepare_fill(P4(gx_device_vector * vdev,
				const gs_imager_state * pis,
				const gx_fill_params * params,
				const gx_drawing_color * pdcolor));

/* Bring state up to date for stroking.  Note that we pass the scale */
/* for the line width and dash offset explicitly. */
/* May call setlinewidth, setlinecap, setlinejoin, setmiterlimit, */
/* setdash, setflat, setstrokecolor, setlogop. */
int gdev_vector_prepare_stroke(P5(gx_device_vector * vdev,
				  const gs_imager_state * pis,
				  const gx_stroke_params * params,
				  const gx_drawing_color * pdcolor,
				  floatp scale));

/* Write a polygon as part of a path (type = gx_path_type_none) */
/* or as a path. */
/* May call moveto, lineto, closepath (if close); */
/* may call beginpath & endpath if type != none. */
int gdev_vector_write_polygon(P5(gx_device_vector * vdev,
				 const gs_fixed_point * points, uint count,
				 bool close, gx_path_type_t type));

/* Write a rectangle.  This is just a special case of write_polygon. */
int gdev_vector_write_rectangle(P7(gx_device_vector * vdev,
				   fixed x0, fixed y0, fixed x1, fixed y1,
				   bool close, gx_rect_direction_t dir));

/* Write a clipping path by calling the path procedures. */
/* May call the same procedures as writepath. */
int gdev_vector_write_clip_path(P2(gx_device_vector * vdev,
				   const gx_clip_path * pcpath));

/* Bring the clipping state up to date. */
/* May call write_rectangle (q.v.), write_clip_path (q.v.). */
int gdev_vector_update_clip_path(P2(gx_device_vector * vdev,
				    const gx_clip_path * pcpath));

/* Close the output file and stream. */
void gdev_vector_close_file(P1(gx_device_vector * vdev));

/* ---------------- Image enumeration ---------------- */

/* Define a common set of state parameters for enumerating images. */
#define gdev_vector_image_enum_common\
	gx_image_enum_common;\
		/* Set by begin_image */\
	gs_memory_t *memory;	/* from begin_image */\
	gx_image_enum_common_t *default_info;	/* non-0 iff using default implementation */\
	gx_image_enum_common_t *bbox_info;	/* non-0 iff passing image data to bbox dev */\
	int width, height;\
	int bits_per_pixel;	/* (per plane) */\
	uint bits_per_row;	/* (per plane) */\
		/* Updated dynamically by image_data */\
	int y			/* 0 <= y < height */
typedef struct gdev_vector_image_enum_s {
    gdev_vector_image_enum_common;
} gdev_vector_image_enum_t;

extern_st(st_vector_image_enum);
#define public_st_vector_image_enum()	/* in gdevvec.c */\
  gs_public_st_ptrs2(st_vector_image_enum, gdev_vector_image_enum_t,\
    "gdev_vector_image_enum_t", vector_image_enum_enum_ptrs,\
    vector_image_enum_reloc_ptrs, default_info, bbox_info)

/*
 * Initialize for enumerating an image.  Note that the last argument is an
 * already-allocated enumerator, not a pointer to the place to store the
 * enumerator.
 */
int gdev_vector_begin_image(P10(gx_device_vector * vdev,
			const gs_imager_state * pis, const gs_image_t * pim,
			gs_image_format_t format, const gs_int_rect * prect,
	      const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
		    gs_memory_t * mem, const gx_image_enum_procs_t * pprocs,
				gdev_vector_image_enum_t * pie));

/* End an image, optionally supplying any necessary blank padding rows. */
/* Return 0 if we used the default implementation, 1 if not. */
int gdev_vector_end_image(P4(gx_device_vector * vdev,
       gdev_vector_image_enum_t * pie, bool draw_last, gx_color_index pad));

/* ================ Device procedures ================ */

/* Redefine get/put_params to handle OutputFile. */
dev_proc_put_params(gdev_vector_put_params);
dev_proc_get_params(gdev_vector_get_params);

/* ---------------- Defaults ---------------- */

/* fill_rectangle may call setfillcolor, dorect. */
dev_proc_fill_rectangle(gdev_vector_fill_rectangle);
/* fill_path may call prepare_fill, writepath, write_clip_path. */
dev_proc_fill_path(gdev_vector_fill_path);
/* stroke_path may call prepare_stroke, write_path, write_clip_path. */
dev_proc_stroke_path(gdev_vector_stroke_path);
/* fill_trapezoid, fill_parallelogram, and fill_triangle may call */
/* setfillcolor, setlogop, beginpath, moveto, lineto, endpath. */
dev_proc_fill_trapezoid(gdev_vector_fill_trapezoid);
dev_proc_fill_parallelogram(gdev_vector_fill_parallelogram);
dev_proc_fill_triangle(gdev_vector_fill_triangle);

#endif /* gdevvec_INCLUDED */
