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

/*$Id: gxshade.h,v 1.1 2000/03/13 18:58:46 mike Exp $ */
/* Internal definitions for shading rendering */

#ifndef gxshade_INCLUDED
#  define gxshade_INCLUDED

#include "gsshade.h"
#include "gxfixed.h"		/* for gxmatrix.h */
#include "gxmatrix.h"		/* for gs_matrix_fixed */
#include "stream.h"

/*
   All shadings are defined with respect to some parameter that varies
   continuously over some range; the shading defines a mapping from the
   parameter values to colors and user space coordinates.  Here are the
   mappings for the 7 currently defined shading types:

   Type Param space     Param => color          Param => User space
   ---- -----------     --------------          -------------------
   1    2-D Domain      Function                Matrix
   2    1-D Domain      Function + Extend       perp. to Coords
   3    1-D Domain      Function + Extend       circles per Coords
   4,5  triangle x      Gouraud interp. on      Gouraud interp. on
	2-D in tri.	Decode => corner        triangle corners
			values => Function
   6    patch x (u,v)   Decode => bilinear      Sc + Sd - Sb on each patch
	in patch	interp. on corner
			values => Function
   7    see 6           see 6                   Sum(i) Sum(j) Pij*Bi(u)*Bj(v)

   To be able to render a portion of a shading usefully, we must be able to
   do two things:

   - Determine what range of parameter values is sufficient to cover
   the region being filled;

   - Evaluate the color at enough points to fill the region (in
   device space).

   Note that the latter may be implemented by a mix of evaluation and
   interpolation, especially for types 3, 6, and 7 where an exact mapping
   may be prohibitively expensive.

   Except for type 3, where circles turn into ellipses, the CTM can get
   folded into the parameter => user space mapping, since in all other
   cases, the mapping space is closed under linear transformations of
   the output.
 */

/* Define types and rendering procedures for the individual shadings. */
typedef struct gs_shading_Fb_s {
    gs_shading_head_t head;
    gs_shading_Fb_params_t params;
} gs_shading_Fb_t;
shading_fill_rectangle_proc(gs_shading_Fb_fill_rectangle);

typedef struct gs_shading_A_s {
    gs_shading_head_t head;
    gs_shading_A_params_t params;
} gs_shading_A_t;
shading_fill_rectangle_proc(gs_shading_A_fill_rectangle);

typedef struct gs_shading_R_s {
    gs_shading_head_t head;
    gs_shading_R_params_t params;
} gs_shading_R_t;
shading_fill_rectangle_proc(gs_shading_R_fill_rectangle);

typedef struct gs_shading_FfGt_s {
    gs_shading_head_t head;
    gs_shading_FfGt_params_t params;
} gs_shading_FfGt_t;
shading_fill_rectangle_proc(gs_shading_FfGt_fill_rectangle);

typedef struct gs_shading_LfGt_s {
    gs_shading_head_t head;
    gs_shading_LfGt_params_t params;
} gs_shading_LfGt_t;
shading_fill_rectangle_proc(gs_shading_LfGt_fill_rectangle);

typedef struct gs_shading_Cp_s {
    gs_shading_head_t head;
    gs_shading_Cp_params_t params;
} gs_shading_Cp_t;
shading_fill_rectangle_proc(gs_shading_Cp_fill_rectangle);

typedef struct gs_shading_Tpp_s {
    gs_shading_head_t head;
    gs_shading_Tpp_params_t params;
} gs_shading_Tpp_t;
shading_fill_rectangle_proc(gs_shading_Tpp_fill_rectangle);

/* We should probably get this from somewhere else.... */
#define max_color_components 4

/* Define a stream for decoding packed coordinate values. */
typedef struct shade_coord_stream_s shade_coord_stream_t;
struct shade_coord_stream_s {
    stream ds;			/* stream if DataSource isn't one already -- */
				/* first for GC-ability (maybe unneeded?) */
    stream *s;			/* DataSource or &ds */
    uint bits;			/* shifted bits of current byte */
    int left;			/* # of bits left in bits */
    const gs_shading_mesh_params_t *params;
    const gs_matrix_fixed *pctm;
    int (*get_value)(P3(shade_coord_stream_t *cs, int num_bits, uint *pvalue));
    int (*get_decoded)(P4(shade_coord_stream_t *cs, int num_bits,
			  const float decode[2], float *pvalue));
};

/* Define one vertex of a mesh. */
typedef struct mesh_vertex_s {
    gs_fixed_point p;
    float cc[max_color_components];
} mesh_vertex_t;

/* Initialize a packed value stream. */
void shade_next_init(P3(shade_coord_stream_t * cs,
			const gs_shading_mesh_params_t * params,
			const gs_imager_state * pis));

/* Get the next flag value. */
int shade_next_flag(P2(shade_coord_stream_t * cs, int BitsPerFlag));

/* Get one or more coordinate pairs. */
int shade_next_coords(P3(shade_coord_stream_t * cs, gs_fixed_point * ppt,
			 int num_points));

/* Get a color.  Currently all this does is look up Indexed colors. */
int shade_next_color(P2(shade_coord_stream_t * cs, float *pc));

/* Get the next vertex for a mesh element. */
int shade_next_vertex(P2(shade_coord_stream_t * cs, mesh_vertex_t * vertex));

/*
   Currently, all shading fill procedures follow the same algorithm:

   - Conservatively inverse-transform the rectangle being filled to a linear
   or rectangular range of values in the parameter space.

   - Compute the color values at the extrema of the range.

   - If possible, compute the parameter range corresponding to a single
   device pixel.

   - Recursively do the following, passing the parameter range and extremal
   color values as the recursion arguments:

   - If the color values are equal to within the tolerance given by the
   smoothness in the graphics state, or if the range of parameters maps
   to a single device pixel, fill the range with the (0) or (0,0) color.

   - Otherwise, subdivide and recurse.  If the parameter range is 2-D,
   subdivide the axis with the largest color difference.

   For shadings based on a function, if the function is not monotonic, the
   smoothness test must only be applied when the parameter range extrema are
   all interpolated from the same entries in the Function.  (We don't
   currently do this.)

 */

/* Define the common structure for recursive subdivision. */
#define shading_fill_state_common\
  gx_device *dev;\
  gs_imager_state *pis;\
  int num_components;		/* # of color components */\
  float cc_max_error[max_color_components]
typedef struct shading_fill_state_s {
    shading_fill_state_common;
} shading_fill_state_t;

/* Initialize the common parts of the recursion state. */
void shade_init_fill_state(P4(shading_fill_state_t * pfs,
			      const gs_shading_t * psh, gx_device * dev,
			      gs_imager_state * pis));

/* Transform a bounding box into device space. */
int shade_bbox_transform2fixed(P3(const gs_rect * rect,
				  const gs_imager_state * pis,
				  gs_fixed_rect * rfixed));

/* Check whether 4 colors fall within the smoothness criterion. */
bool shade_colors4_converge(P2(const gs_client_color cc[4],
			       const shading_fill_state_t * pfs));

/* Fill one piece of a shading. */
#ifndef gx_device_color_DEFINED
#  define gx_device_color_DEFINED
typedef struct gx_device_color_s gx_device_color;
#endif
int shade_fill_path(P3(const shading_fill_state_t * pfs, gx_path * ppath,
		       gx_device_color * pdevc));

#endif /* gxshade_INCLUDED */

#if 0				/*************************************************************** */

/*
 * Here is a sketch of what will be needed to generalize Patterns for
 * (the) new PatternType(s).
 */
typedef struct gs_pattern_instance_s {
    rc_header rc;		/* ?? */
    const gs_pattern_type_t *type;
    gs_uid XUID;		/* ?? */
    gs_state *saved;		/* ?? */
    void *data;
} gs_pattern_instance_t;
typedef struct gs_pattern1_instance_data_s {
    ...
} gs_pattern1_instance_data_t;

#define gs_pattern2_instance_data_common\
  const gs_shading_t *shading;\
  gx_device_color *background;\
  const gs_color_space *color_space;\
  gs_matrix param_to_device_matrix
typedef struct gs_pattern2_instance_data_common_s {
    gs_pattern2_instance_data_common;
} gs_pattern2_instance_data_common_t;

#endif /*************************************************************** */
