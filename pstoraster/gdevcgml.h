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

/* gdevcgml.h */
/* Interface definitions for CGM-writing library */

/* The names in the following follow the CGM standard exactly, */
/* except that we have substituted the American spellings of */
/* color (colour) and center (centre). */

/* ================ Types ================ */

/* Define the abstract type for the CGM writer state. */
typedef struct cgm_state_s cgm_state;

/* Define the type for the allocator used by the CGM writer. */
typedef struct cgm_allocator_s {
	void *private_data;
	void *(*alloc)(P2(void *, uint));
	void (*free)(P2(void *, void *));
} cgm_allocator;

/* Define types for CGM coordinates. */
typedef int cgm_int;
typedef double cgm_real;
typedef union cgm_vdc_s {
	cgm_int integer;
	cgm_real real;
} cgm_vdc;
typedef struct cgm_int_point_s {
	cgm_int x, y;
} cgm_int_point;
typedef struct cgm_real_point_s {
	cgm_real x, y;
} cgm_real_point;
typedef union cgm_point_s {
	cgm_int_point integer;
	cgm_real_point real;
} cgm_point;

/* Define types for colors. */
typedef struct cgm_rgb_s {
	cgm_int r, g, b;
} cgm_rgb;
typedef union cgm_color_s {
	cgm_int index;
	cgm_rgb rgb;
} cgm_color;

/*
 * Define other types used in CGM elements or API calls.
 * If cgm_xxx is an enumerated type, let xxx' be xxx with any of the
 * following words dropped: mode, specification, type; then the values
 * of the enumerated type are named cgm_xxx'_yyy.
 */
typedef enum {
	cgm_vdc_integer = 0,
	cgm_vdc_real
} cgm_vdc_type;
typedef struct cgm_string_s {
	const char *chars;
	uint length;
} cgm_string;
typedef enum {
	cgm_scaling_abstract = 0,
	cgm_scaling_metric
} cgm_scaling_mode;
typedef enum {
	cgm_color_selection_indexed = 0,
	cgm_color_selection_direct
} cgm_color_selection_mode;
typedef enum {
	cgm_line_marker_absolute = 0,
	cgm_line_marker_scaled
} cgm_line_marker_specification_mode;
typedef cgm_line_marker_specification_mode
  cgm_line_width_specification_mode, cgm_marker_size_specification_mode,
  cgm_edge_width_specification_mode;
typedef union cgm_line_marker_extent_s {
	cgm_vdc absolute;
	cgm_real scaled;
} cgm_line_marker_extent;
typedef cgm_line_marker_extent
  cgm_line_width, cgm_marker_size, cgm_edge_width;
typedef enum {
	cgm_transparency_off = 0,
	cgm_transparency_on
} cgm_transparency;
typedef enum {
	cgm_clip_off = 0,
	cgm_clip_on
} cgm_clip_indicator;
typedef struct cgm_precision_s {
	enum { cgm_representation_floating, cgm_representation_fixed } representation;
	int exponent_or_whole_width;
	int fraction_width;
} cgm_precision;
typedef enum {
	cgm_line_solid = 1,
	cgm_line_dash,
	cgm_line_dot,
	cgm_line_dash_dot,
	cgm_line_dash_dot_dot
} cgm_line_type;
typedef enum {
	cgm_marker_dot = 1,
	cgm_marker_plus,
	cgm_marker_asterisk,
	cgm_marker_circle,
	cgm_marker_cross
} cgm_marker_type;
typedef enum {
	cgm_text_precision_string = 0,
	cgm_text_precision_character,
	cgm_text_precision_stroke
} cgm_text_precision;
typedef enum {
	cgm_text_path_right = 0,
	cgm_text_path_left,
	cgm_text_path_up,
	cgm_text_path_down
} cgm_text_path;
typedef enum {
	cgm_text_horizontal_normal = 0,
	cgm_text_horizontal_left,
	cgm_text_horizontal_center,
	cgm_text_horizontal_right,
	cgm_text_horizontal_continuous
} cgm_text_alignment_horizontal;
typedef enum {
	cgm_text_vertical_normal = 0,
	cgm_text_vertical_top,
	cgm_text_vertical_cap,
	cgm_text_vertical_half,
	cgm_text_vertical_base,
	cgm_text_vertical_bottom,
	cgm_text_vertical_continuous
} cgm_text_alignment_vertical;
typedef enum {
	cgm_interior_style_hollow = 0,
	cgm_interior_style_solid,
	cgm_interior_style_pattern,
	cgm_interior_style_hatch,
	cgm_interior_style_empty
} cgm_interior_style;
typedef enum {
	cgm_hatch_horizontal = 1,
	cgm_hatch_vertical,
	cgm_hatch_positive_slope,
	cgm_hatch_negative_slope,
	cgm_hatch_combined_v_h_slant,
	cgm_hatch_combined_l_r_slant
} cgm_hatch_index;
typedef enum {
	cgm_arc_closure_pie = 0,
	cgm_arc_closure_chord
} cgm_arc_closure;
typedef enum {
	cgm_edge_out_invisible = 0,
	cgm_edge_out_visible,
	cgm_edge_out_close_invisible,
	cgm_edge_out_close_visible
} cgm_edge_out;
typedef struct cgm_polygon_edge_s {
	cgm_point vertex;
	cgm_edge_out edge_out;
} cgm_polygon_edge;
typedef enum {
	cgm_cell_mode_run_length = 0,
	cgm_cell_mode_packed
} cgm_cell_representation_mode;
typedef enum {
	cgm_edge_solid = 1,
	cgm_edge_dash,
	cgm_edge_dot,
	cgm_edge_dash_dot,
	cgm_edge_dash_dot_dot
} cgm_edge_type;
typedef enum {
	cgm_aspect_source_individual = 0,
	cgm_aspect_source_bundled
} cgm_aspect_source;
typedef enum {
	cgm_aspect_line_type = 0,
	cgm_aspect_line_width,
	cgm_aspect_line_color,
	cgm_aspect_marker_type,
	cgm_aspect_marker_size,
	cgm_aspect_marker_color,
	cgm_aspect_text_font_index,
	cgm_aspect_text_precision,
	cgm_aspect_character_expansion_factor,
	cgm_aspect_character_spacing,
	cgm_aspect_text_color,
	cgm_aspect_interior_style,
	cgm_aspect_fill_color,
	cgm_aspect_hatch_index,
	cgm_aspect_pattern_index,
	cgm_aspect_edge_type,
	cgm_aspect_edge_width,
	cgm_aspect_edge_color
} cgm_aspect_type;
typedef struct cgm_aspect_source_flag_s {
	cgm_aspect_type type;
	cgm_aspect_source source;
} cgm_aspect_source_flag;

/* ================ API ================ */

typedef enum {
	cgm_result_ok = 0,
	cgm_result_wrong_state = -1,
	cgm_result_out_of_range = -2,
	cgm_result_io_error = -3,
	cgm_result_out_of_memory = -4
} cgm_result;

/* ---------------- Initialize/terminate ---------------- */

cgm_state *cgm_initialize(P2(FILE *, const cgm_allocator *));
cgm_result cgm_terminate(P1(cgm_state *));

/* ---------------- Metafile elements ---------------- */

typedef struct cgm_metafile_elements_s {
	cgm_int metafile_version;
	cgm_string metafile_description;	
	cgm_vdc_type vdc_type;
	int integer_precision;
	cgm_precision real_precision;
	int index_precision;
	int color_precision;
	int color_index_precision;
	cgm_int maximum_color_index;
	cgm_color color_value_extent[2];
	const int *metafile_element_list;
	  int metafile_element_list_count;
	const cgm_string *font_list;
	  int font_list_count;
	/* character_set_list */
	/* character_coding_announcer */
} cgm_metafile_elements;
#define cgm_set_METAFILE_VERSION	(1L<<0)
#define cgm_set_METAFILE_DESCRIPTION	(1L<<1)
#define cgm_set_VDC_TYPE		(1L<<2)
#define cgm_set_INTEGER_PRECISION	(1L<<3)
#define cgm_set_REAL_PRECISION		(1L<<4)
#define cgm_set_INDEX_PRECISION		(1L<<5)
#define cgm_set_COLOR_PRECISION		(1L<<6)
#define cgm_set_COLOR_INDEX_PRECISION	(1L<<7)
#define cgm_set_MAXIMUM_COLOR_INDEX	(1L<<8)
#define cgm_set_COLOR_VALUE_EXTENT	(1L<<9)
#define cgm_set_METAFILE_ELEMENT_LIST	(1L<<10)
#define cgm_set_FONT_LIST		(1L<<11)
#define cgm_set_CHARACTER_SET_LIST	(1L<<12)
#define cgm_set_CHARACTER_CODING_ANNOUNCER	(1L<<13)

cgm_result
	cgm_BEGIN_METAFILE(P3(cgm_state *, const char *, uint)),
	cgm_set_metafile_elements(P3(cgm_state *,
				     const cgm_metafile_elements *, long)),
	cgm_END_METAFILE(P1(cgm_state *));

/* ---------------- Picture elements ---------------- */

typedef struct cgm_picture_elements_s {
	cgm_scaling_mode scaling_mode;
	cgm_real scale_factor;
	cgm_color_selection_mode color_selection_mode;
	cgm_line_width_specification_mode line_width_specification_mode;
	cgm_marker_size_specification_mode marker_size_specification_mode;
	cgm_edge_width_specification_mode edge_width_specification_mode;
	cgm_point vdc_extent[2];
	cgm_color background_color;
} cgm_picture_elements;
#define cgm_set_SCALING_MODE		(1L<<0)
#define cgm_set_COLOR_SELECTION_MODE	(1L<<1)
#define cgm_set_LINE_WIDTH_SPECIFICATION_MODE	(1L<<2)
#define cgm_set_MARKER_SIZE_SPECIFICATION_MODE	(1L<<3)
#define cgm_set_EDGE_WIDTH_SPECIFICATION_MODE	(1L<<4)
#define cgm_set_VDC_EXTENT		(1L<<5)
#define cgm_set_BACKGROUND_COLOR	(1L<<6)

cgm_result
	cgm_BEGIN_PICTURE(P3(cgm_state *, const char *, uint)),
	cgm_set_picture_elements(P3(cgm_state *,
				    const cgm_picture_elements *, long)),
	cgm_BEGIN_PICTURE_BODY(P1(cgm_state *)),
	cgm_END_PICTURE(P1(cgm_state *));

/* ---------------- Control elements ---------------- */

cgm_result
	cgm_VDC_INTEGER_PRECISION(P2(cgm_state *, int)),
	cgm_VDC_REAL_PRECISION(P2(cgm_state *, const cgm_precision *)),
	cgm_AUXILIARY_COLOR(P2(cgm_state *, const cgm_color *)),
	cgm_TRANSPARENCY(P2(cgm_state *, cgm_transparency)),
	cgm_CLIP_RECTANGLE(P2(cgm_state *, const cgm_point [2])),
	cgm_CLIP_INDICATOR(P2(cgm_state *, cgm_clip_indicator));

/* ---------------- Graphical primitive elements ---------------- */

cgm_result
	cgm_POLYLINE(P3(cgm_state *, const cgm_point *, int)),
	cgm_DISJOINT_POLYLINE(P3(cgm_state *, const cgm_point *, int)),
	cgm_POLYMARKER(P3(cgm_state *, const cgm_point *, int)),
	cgm_TEXT(P5(cgm_state *, const cgm_point *, bool, const char *, uint)),
	cgm_RESTRICTED_TEXT(P7(cgm_state *, const cgm_vdc *, const cgm_vdc *,
			       const cgm_point *, bool, const char *, uint)),
	cgm_APPEND_TEXT(P4(cgm_state *, bool, const char *, uint)),
	cgm_POLYGON(P3(cgm_state *, const cgm_point *, int)),
	cgm_POLYGON_SET(P3(cgm_state *, const cgm_polygon_edge *, int)),
	cgm_CELL_ARRAY(P9(cgm_state *, const cgm_point * /*[3]*/, cgm_int,
			  cgm_int, cgm_int, cgm_cell_representation_mode,
			  const byte *, uint, uint)),
	cgm_RECTANGLE(P3(cgm_state *, const cgm_point *, const cgm_point *)),
	cgm_CIRCLE(P3(cgm_state *, const cgm_point *, const cgm_vdc *)),
	cgm_CIRCULAR_ARC_3_POINT(P4(cgm_state *, const cgm_point *,
				    const cgm_point *, const cgm_point *)),
	cgm_CIRCULAR_ARC_3_POINT_CLOSE(P5(cgm_state *, const cgm_point *,
					  const cgm_point *,
					  const cgm_point *, cgm_arc_closure)),
	cgm_CIRCULAR_ARC_CENTER(P7(cgm_state *, const cgm_point *,
				   const cgm_vdc *, const cgm_vdc *,
				   const cgm_vdc *, const cgm_vdc *,
				   const cgm_vdc *)),
	cgm_CIRCULAR_ARC_CENTER_CLOSE(P8(cgm_state *, const cgm_point *,
					 const cgm_vdc *, const cgm_vdc *,
					 const cgm_vdc *, const cgm_vdc *,
					 const cgm_vdc *, cgm_arc_closure)),
	cgm_ELLIPSE(P4(cgm_state *, const cgm_point *, const cgm_point *,
		       const cgm_point *)),
	cgm_ELLIPTICAL_ARC(P8(cgm_state *, const cgm_point *,
			      const cgm_point *, const cgm_point *,
			      const cgm_vdc *, const cgm_vdc *,
			      const cgm_vdc *, const cgm_vdc *)),
	cgm_ELLIPTICAL_ARC_CLOSE(P9(cgm_state *, const cgm_point *,
				    const cgm_point *, const cgm_point *,
				    const cgm_vdc *, const cgm_vdc *,
				    const cgm_vdc *, const cgm_vdc *,
				    cgm_arc_closure));

/* ---------------- Attribute elements ---------------- */

cgm_result
	cgm_LINE_BUNDLE_INDEX(P2(cgm_state *, cgm_int)),
	cgm_LINE_TYPE(P2(cgm_state *, cgm_line_type)),
	cgm_LINE_WIDTH(P2(cgm_state *, const cgm_line_width *)),
	cgm_LINE_COLOR(P2(cgm_state *, const cgm_color *)),
	cgm_MARKER_BUNDLE_INDEX(P2(cgm_state *, cgm_int)),
	cgm_MARKER_TYPE(P2(cgm_state *, cgm_marker_type)),
	cgm_MARKER_SIZE(P2(cgm_state *, const cgm_marker_size *)),
	cgm_MARKER_COLOR(P2(cgm_state *, const cgm_color *)),
	cgm_TEXT_BUNDLE_INDEX(P2(cgm_state *, cgm_int)),
	cgm_TEXT_FONT_INDEX(P2(cgm_state *, cgm_int)),
	cgm_TEXT_PRECISION(P2(cgm_state *, cgm_text_precision)),
	cgm_CHARACTER_EXPANSION_FACTOR(P2(cgm_state *, cgm_real)),
	cgm_CHARACTER_SPACING(P2(cgm_state *, cgm_real)),
	cgm_TEXT_COLOR(P2(cgm_state *, const cgm_color *)),
	cgm_CHARACTER_HEIGHT(P2(cgm_state *, const cgm_vdc *)),
	cgm_CHARACTER_ORIENTATION(P5(cgm_state *, const cgm_vdc *,
				     const cgm_vdc *, const cgm_vdc *,
				     const cgm_vdc *)),
	cgm_TEXT_PATH(P2(cgm_state *, cgm_text_path)),
	cgm_TEXT_ALIGNMENT(P5(cgm_state *, cgm_text_alignment_horizontal,
			      cgm_text_alignment_vertical,
			      cgm_real, cgm_real)),
	cgm_CHARACTER_SET_INDEX(P2(cgm_state *, cgm_int)),
  /* The following should be cgm_ALTERNATE_..., but the VAX DEC C */
  /* compiler gives an error for names longer than 31 characters. */
	cgm_ALT_CHARACTER_SET_INDEX(P2(cgm_state *, cgm_int)),
	cgm_FILL_BUNDLE_INDEX(P2(cgm_state *, cgm_int)),
	cgm_INTERIOR_STYLE(P2(cgm_state *, cgm_interior_style)),
	cgm_FILL_COLOR(P2(cgm_state *, const cgm_color *)),
	cgm_HATCH_INDEX(P2(cgm_state *, cgm_hatch_index)),
	cgm_PATTERN_INDEX(P2(cgm_state *, cgm_int)),
	cgm_EDGE_BUNDLE_INDEX(P2(cgm_state *, cgm_int)),
	cgm_EDGE_TYPE(P2(cgm_state *, cgm_edge_type)),
	cgm_EDGE_WIDTH(P2(cgm_state *, const cgm_edge_width *)),
	cgm_EDGE_COLOR(P2(cgm_state *, const cgm_color *)),
	cgm_EDGE_VISIBILITY(P2(cgm_state *, bool)),
	cgm_FILL_REFERENCE_POINT(P2(cgm_state *, const cgm_point *)),
/* PATTERN_TABLE */
	cgm_PATTERN_SIZE(P5(cgm_state *, const cgm_vdc *, const cgm_vdc *,
			    const cgm_vdc *, const cgm_vdc *)),
	cgm_COLOR_TABLE(P4(cgm_state *, cgm_int, const cgm_color *, int)),
	cgm_ASPECT_SOURCE_FLAGS(P3(cgm_state *,
				   const cgm_aspect_source_flag *, int));
