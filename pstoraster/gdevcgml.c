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

/* gdevcgml.c */
/* CGM-writing library */
#include "memory_.h"
#include "stdio_.h"
#include "gdevcgmx.h"

/* Forward references to command-writing procedures */
private void begin_command(P2(cgm_state *, cgm_op_index));
#define OP(op) begin_command(st, op)
private cgm_result end_command(P1(cgm_state *));
#define END_OP (void)end_command(st)
#define END return end_command(st)
/* Parameters */
private void put_int(P3(cgm_state *, cgm_int, int));
#define CI(ci) put_int(st, ci, st->metafile.color_index_precision)
#define I(i) put_int(st, i, st->metafile.integer_precision)
#define IX(ix) put_int(st, ix, st->metafile.index_precision)
#define E(e) put_int(st, (int)(e), 16)
private void put_real(P3(cgm_state *, cgm_real, const cgm_precision *));
#define R(r) put_real(st, r, &st->metafile.real_precision)
private void put_vdc(P2(cgm_state *, const cgm_vdc *));
#define VDC(vdc) put_vdc(st, vdc)
#define VDC2(vdc1, vdc2) VDC(vdc1); VDC(vdc2)
#define VDC4(vdc1, vdc2, vdc3, vdc4) VDC2(vdc1, vdc2); VDC2(vdc3, vdc4)
private void put_vdc_r(P3(cgm_state *, const cgm_line_marker_extent *, cgm_line_marker_specification_mode));
#define VDC_R(vdcr, mode) put_vdc_r(st, vdcr, mode)
private void put_point(P2(cgm_state *, const cgm_point *));
#define P(p) put_point(st, p)
private void put_points(P3(cgm_state *, const cgm_point *, int));
#define nP(p, n) put_points(st, p, n)
private void put_string(P3(cgm_state *, const char *, uint));
#define S(s, l) put_string(st, s, l)
private void put_color(P2(cgm_state *, const cgm_color *));
#define CO(co) put_color(st, co)
private void put_rgb(P2(cgm_state *, const cgm_rgb *));
#define CD(cd) put_rgb(st, cd)
/* Other data types */
#define put_byte(st, b)\
  if ( st->command_count == command_max_count ) write_command(st, false);\
  st->command[st->command_count++] = (byte)(b)
private void put_bytes(P3(cgm_state *, const byte *, uint));
private void write_command(P2(cgm_state *, bool));
private void put_real_precision(P2(cgm_state *, const cgm_precision *));

/* ================ Public routines ================ */

/* ---------------- Initialize/terminate ---------------- */

/* Initialize a CGM writer. */
cgm_state *
cgm_initialize(FILE *file, const cgm_allocator *cal)
{	cgm_state *st = (*cal->alloc)(cal->private_data, sizeof(cgm_state));
	if ( st == 0 )
	  return 0;
	st->file = file;
	st->allocator = *cal;
		/* Initialize metafile elements. */
	st->metafile.vdc_type = cgm_vdc_integer;
	st->metafile.integer_precision = 16;
	st->metafile.real_precision.representation = cgm_representation_fixed;
	st->metafile.real_precision.exponent_or_whole_width = 16;
	st->metafile.real_precision.fraction_width = 16;
	st->metafile.index_precision = 16;
	st->metafile.color_precision = 8;
	st->metafile.color_index_precision = 8;
	st->metafile.maximum_color_index = 63;
	/* color_value_extent */
	/*st->metafile.character_coding_announcer = 0;*/
		/* Initialize picture elements. */
	st->picture.scaling_mode = cgm_scaling_abstract;
	st->picture.color_selection_mode = cgm_color_selection_indexed;
	st->picture.line_width_specification_mode = cgm_line_marker_absolute;
	st->picture.marker_size_specification_mode = cgm_line_marker_absolute;
	st->picture.edge_width_specification_mode = cgm_line_marker_absolute;
	/* vdc_extent */
	/* background_color */
		/* Initialize control elements. */
	st->vdc_integer_precision = st->metafile.integer_precision;
	st->vdc_real_precision = st->metafile.real_precision;
	st->transparency = cgm_transparency_on;
	/* clip_rectangle */
	st->clip_indicator = cgm_clip_on;
		/* Initialize other state elements. */
	st->line_bundle_index = 1;
	st->line_type = cgm_line_solid;
	/* line_width */
	/* line_color */
	st->marker_bundle_index = 1;
	st->marker_type = cgm_marker_asterisk;
	/* marker_size */
	/* marker_color */
	st->text_bundle_index = 1;
	st->text_font_index = 1;
	st->text_precision = cgm_text_precision_string;
	st->character_expansion_factor = 1.0;
	st->character_spacing = 0.0;
	/* text_color */
	/* character_height */
	/* character_orientation */
	st->text_path = cgm_text_path_right;
	/* text_alignment */
	st->character_set_index = 1;
	st->alternate_character_set_index = 1;
	st->fill_bundle_index = 1;
	st->interior_style = cgm_interior_style_hollow;
	st->hatch_index = cgm_hatch_horizontal;
	st->pattern_index = 1;
	st->edge_bundle_index = 1;
	st->edge_type = cgm_edge_solid;
	/* edge_width */
	st->edge_visibility = false;
	/* fill_reference_point */
	/* pattern_table */
	/* pattern_size */
	/* color_table */
	memset(st->source_flags, (byte)cgm_aspect_source_individual,
	       sizeof(st->source_flags));
	return st;
}

/* Terminate a CGM writer. */
cgm_result
cgm_terminate(cgm_state *st)
{	(*st->allocator.free)(st->allocator.private_data, st);
	return cgm_result_ok;
}

/* ---------------- Metafile elements ---------------- */

cgm_result
cgm_BEGIN_METAFILE(cgm_state *st, const char *str, uint len)
{	OP(BEGIN_METAFILE);
	S(str, len);
	END;
}

cgm_result
cgm_set_metafile_elements(cgm_state *st, const cgm_metafile_elements *meta, long mask)
{	if ( (mask & cgm_set_METAFILE_VERSION) )
	  {	OP(METAFILE_VERSION);
		I(meta->metafile_version);
		END_OP;
		st->metafile.metafile_version = meta->metafile_version;
	  }
	if ( (mask & cgm_set_METAFILE_DESCRIPTION) )
	  {	OP(METAFILE_DESCRIPTION);
		S(meta->metafile_description.chars, meta->metafile_description.length);
		END_OP;
		st->metafile.metafile_description = meta->metafile_description;
	  }
	if ( (mask & cgm_set_VDC_TYPE) )
	  {	OP(VDC_TYPE);
		E(meta->vdc_type);
		END_OP;
		st->metafile.vdc_type = meta->vdc_type;
	  }
	if ( (mask & cgm_set_INTEGER_PRECISION) )
	  {	OP(INTEGER_PRECISION);
		I(meta->integer_precision);
		END_OP;
		st->metafile.integer_precision = meta->integer_precision;
	  }
	if ( (mask & cgm_set_REAL_PRECISION) )
	  {	OP(REAL_PRECISION);
		put_real_precision(st, &meta->real_precision);
		END_OP;
		st->metafile.real_precision = meta->real_precision;
	  }
	if ( (mask & cgm_set_INDEX_PRECISION) )
	  {	OP(INDEX_PRECISION);
		I(meta->index_precision);
		END_OP;
		st->metafile.index_precision = meta->index_precision;
	  }
	if ( (mask & cgm_set_COLOR_PRECISION) )
	  {	OP(COLOR_PRECISION);
		I(meta->color_precision);
		END_OP;
		st->metafile.color_index_precision = meta->color_index_precision;
	  }
	if ( (mask & cgm_set_COLOR_INDEX_PRECISION) )
	  {	OP(COLOR_INDEX_PRECISION);
		I(meta->color_index_precision);
		END_OP;
		st->metafile.color_index_precision = meta->color_index_precision;
	  }
	if ( (mask & cgm_set_MAXIMUM_COLOR_INDEX) )
	  {	OP(MAXIMUM_COLOR_INDEX);
		CI(meta->maximum_color_index);
		END_OP;
		st->metafile.maximum_color_index = meta->maximum_color_index;
	  }
	if ( (mask & cgm_set_METAFILE_ELEMENT_LIST) )
	  {	int i;
		const int *p;
		OP(METAFILE_ELEMENT_LIST);
		for ( i = 0, p = meta->metafile_element_list;
		      i < meta->metafile_element_list_count;
		      i++, p += 2
		    )
		  {	I(p[0]); I(p[1]);
		  }
		END_OP;
		st->metafile.metafile_element_list =
		  meta->metafile_element_list;
		st->metafile.metafile_element_list_count =
		  meta->metafile_element_list_count;
	  }
	/* element list */
	if ( (mask & cgm_set_FONT_LIST) )
	  {	int i;
		OP(FONT_LIST);
		for ( i = 0; i < meta->font_list_count; ++i )
		  S(meta->font_list[i].chars, meta->font_list[i].length);
		END_OP;
		st->metafile.font_list = meta->font_list;
		st->metafile.font_list_count = meta->font_list_count;
	  }
	/* character set list */
	/* character coding announcer */
	return st->result;
}

cgm_result
cgm_END_METAFILE(cgm_state *st)
{	OP(END_METAFILE);
	END;
}

/* ---------------- Picture elements ---------------- */

cgm_result
cgm_BEGIN_PICTURE(cgm_state *st, const char *str, uint len)
{	OP(BEGIN_PICTURE);
	S(str, len);
	END;
}

cgm_result
cgm_set_picture_elements(cgm_state *st, const cgm_picture_elements *pic, long mask)
{	if ( (mask & cgm_set_SCALING_MODE) )
	  {	OP(SCALING_MODE);
		E(pic->scaling_mode);
		R(pic->scale_factor);
		st->picture.scaling_mode = pic->scaling_mode;
		st->picture.scale_factor = pic->scale_factor;
		END_OP;
	  }
	if ( (mask & cgm_set_COLOR_SELECTION_MODE) )
	  {	OP(COLOR_SELECTION_MODE);
		E(pic->color_selection_mode);
		END_OP;
		st->picture.color_selection_mode = pic->color_selection_mode;
	  }
	if ( (mask & cgm_set_LINE_WIDTH_SPECIFICATION_MODE) )
	  {	OP(LINE_WIDTH_SPECIFICATION_MODE);
		E(pic->line_width_specification_mode);
		END_OP;
		st->picture.line_width_specification_mode = pic->line_width_specification_mode;
	  }
	if ( (mask & cgm_set_MARKER_SIZE_SPECIFICATION_MODE) )
	  {	OP(MARKER_SIZE_SPECIFICATION_MODE);
		E(pic->marker_size_specification_mode);
		END_OP;
		st->picture.marker_size_specification_mode = pic->marker_size_specification_mode;
	  }
	if ( (mask & cgm_set_EDGE_WIDTH_SPECIFICATION_MODE) )
	  {	OP(EDGE_WIDTH_SPECIFICATION_MODE);
		E(pic->edge_width_specification_mode);
		END_OP;
		st->picture.edge_width_specification_mode = pic->edge_width_specification_mode;
	  }
	if ( (mask & cgm_set_VDC_EXTENT) )
	  {	OP(VDC_EXTENT);
		P(&pic->vdc_extent[0]);
		P(&pic->vdc_extent[1]);
		END_OP;
		st->picture.vdc_extent[0] = pic->vdc_extent[0];
		st->picture.vdc_extent[1] = pic->vdc_extent[1];
	  }
	if ( (mask & cgm_set_BACKGROUND_COLOR) )
	  {	OP(BACKGROUND_COLOR);
		CD(&pic->background_color.rgb);
		END;
		st->picture.background_color = pic->background_color;
	  }
	return st->result;
}

cgm_result
cgm_BEGIN_PICTURE_BODY(cgm_state *st)
{	OP(BEGIN_PICTURE_BODY);
	END;
}

cgm_result
cgm_END_PICTURE(cgm_state *st)
{	OP(END_PICTURE);
	END;
}

/* ---------------- Control elements ---------------- */

cgm_result
cgm_VDC_INTEGER_PRECISION(cgm_state *st, int precision)
{	if ( st->vdc_integer_precision != precision )
	  {	OP(VDC_INTEGER_PRECISION);
		I(precision);  st->vdc_integer_precision = precision;
		END;
	  }
	else
	  return cgm_result_ok;
}

cgm_result
cgm_VDC_REAL_PRECISION(cgm_state *st, const cgm_precision *precision)
{	OP(VDC_REAL_PRECISION);
	put_real_precision(st, precision);  st->vdc_real_precision = *precision;
	END;
}

cgm_result
cgm_AUXILIARY_COLOR(cgm_state *st, const cgm_color *color)
{	OP(AUXILIARY_COLOR);
	CO(color);  st->auxiliary_color = *color;
	END;
}

cgm_result
cgm_TRANSPARENCY(cgm_state *st, cgm_transparency transparency)
{	OP(TRANSPARENCY);
	E(transparency);  st->transparency = transparency;
	END;
}

cgm_result
cgm_CLIP_RECTANGLE(cgm_state *st, const cgm_point rectangle[2])
{	OP(CLIP_RECTANGLE);
	P(&rectangle[0]);  st->clip_rectangle[0] = rectangle[0];
	P(&rectangle[1]);  st->clip_rectangle[1] = rectangle[1];
	END;
}

cgm_result
cgm_CLIP_INDICATOR(cgm_state *st, cgm_clip_indicator clip)
{	OP(CLIP_INDICATOR);
	E(clip);  st->clip_indicator = clip;
	END;
}

/* ---------------- Graphical primitive elements ---------------- */

cgm_result
cgm_POLYLINE(cgm_state *st, const cgm_point *vertices, int count)
{	OP(POLYLINE);
	nP(vertices, count);
	END;
}

cgm_result
cgm_DISJOINT_POLYLINE(cgm_state *st, const cgm_point *endpoints, int count)
{	OP(DISJOINT_POLYLINE);
	nP(endpoints, count);
	END;
}

cgm_result
cgm_POLYMARKER(cgm_state *st, const cgm_point *positions, int count)
{	OP(POLYMARKER);
	nP(positions, count);
	END;
}

cgm_result
cgm_TEXT(cgm_state *st, const cgm_point *position, bool final, const char *str, uint len)
{	OP(TEXT);
	P(position); E(final); S(str, len);
	END;
}

cgm_result
cgm_RESTRICTED_TEXT(cgm_state *st, const cgm_vdc *delta_width, const cgm_vdc *delta_height, const cgm_point *position, bool final, const char *str, uint len)
{	OP(RESTRICTED_TEXT);
	VDC2(delta_width, delta_height); P(position); E(final); S(str, len);
	END;
}

cgm_result
cgm_APPEND_TEXT(cgm_state *st, bool final, const char *str, uint len)
{	OP(APPEND_TEXT);
	E(final); S(str, len);
	END;
}

cgm_result
cgm_POLYGON(cgm_state *st, const cgm_point *vertices, int count)
{	OP(POLYGON);
	nP(vertices, count);
	END;
}

cgm_result
cgm_POLYGON_SET(cgm_state *st, const cgm_polygon_edge *vertices, int count)
{	int i;
	OP(POLYGON);
	for ( i = 0; i < count; ++i )
	{	P(&vertices[i].vertex);
		E(vertices[i].edge_out);
	}
	END;
}

cgm_result
cgm_CELL_ARRAY(cgm_state *st, const cgm_point *pqr /*[3]*/, cgm_int nx, cgm_int ny, cgm_int local_color_precision, cgm_cell_representation_mode mode, const byte *values, uint source_bit, uint raster)
{	int precision = local_color_precision;
	int bits_per_pixel;
	uint row_bytes;
	const byte *row = values + (source_bit >> 3);
	int bit = source_bit & 7;
	int y;

	/* Currently we ignore the cell representation_mode, and always */
	/* produce cell arrays in 'packed' format. */
	mode = cgm_cell_mode_packed;
	OP(CELL_ARRAY);
	nP(pqr, 3); I(nx); I(ny); I(local_color_precision); E(mode);
	if ( precision == 0 )
	  precision = (st->picture.color_selection_mode ==
		         cgm_color_selection_indexed ?
		       st->metafile.color_index_precision :
		       st->metafile.color_precision);
	bits_per_pixel =
	  (st->picture.color_selection_mode == cgm_color_selection_indexed ?
	   precision : precision * 3);
	row_bytes = (bits_per_pixel * nx + 7) >> 3;
	for ( y = 0; y < ny; y++, row += raster )
	  {	if ( bit == 0 )
		  put_bytes(st, row, row_bytes);
		else
		  {	uint i;
			for ( i = 0; i < row_bytes; i++ )
			  {	byte b = (row[i] << bit) +
				  (row[i+1] >> (8 - bit));
				put_byte(st, b);
			  }
		  }
		if ( (row_bytes & 1) )
		  {	put_byte(st, 0);
		  }
	  }
	END;
}

cgm_result
cgm_RECTANGLE(cgm_state *st, const cgm_point *corner1, const cgm_point *corner2)
{	OP(RECTANGLE);
	P(corner1); P(corner2);
	END;
}

cgm_result
cgm_CIRCLE(cgm_state *st, const cgm_point *center, const cgm_vdc *radius)
{	OP(CIRCLE);
	P(center); VDC(radius);
	END;
}

cgm_result
cgm_CIRCULAR_ARC_3_POINT(cgm_state *st, const cgm_point *start, const cgm_point *intermediate, const cgm_point *end)
{	OP(CIRCULAR_ARC_3_POINT);
	P(start); P(intermediate); P(end);
	END;
}

cgm_result
cgm_CIRCULAR_ARC_3_POINT_CLOSE(cgm_state *st, const cgm_point *start, const cgm_point *intermediate, const cgm_point *end, cgm_arc_closure closure)
{	OP(CIRCULAR_ARC_3_POINT_CLOSE);
	P(start); P(intermediate); P(end); E(closure);
	END;
}

cgm_result
cgm_CIRCULAR_ARC_CENTER(cgm_state *st, const cgm_point *center, const cgm_vdc *dx_start, const cgm_vdc *dy_start, const cgm_vdc *dx_end, const cgm_vdc *dy_end, const cgm_vdc *radius)
{	OP(CIRCULAR_ARC_CENTER);
	P(center); VDC4(dx_start, dy_start, dx_end, dy_end); VDC(radius);
	END;
}

cgm_result
cgm_CIRCULAR_ARC_CENTER_CLOSE(cgm_state *st, const cgm_point *center, const cgm_vdc *dx_start, const cgm_vdc *dy_start, const cgm_vdc *dx_end, const cgm_vdc *dy_end, const cgm_vdc *radius, cgm_arc_closure closure)
{	OP(CIRCULAR_ARC_CENTER_CLOSE);
	P(center); VDC4(dx_start, dy_start, dx_end, dy_end); VDC(radius);
	E(closure);
	END;
}

cgm_result
cgm_ELLIPSE(cgm_state *st, const cgm_point *center, const cgm_point *cd1_end, const cgm_point *cd2_end)
{	OP(ELLIPSE);
	P(center); P(cd1_end); P(cd2_end);
	END;
}

cgm_result
cgm_ELLIPTICAL_ARC(cgm_state *st, const cgm_point *center, const cgm_point *cd1_end, const cgm_point *cd2_end, const cgm_vdc *dx_start, const cgm_vdc *dy_start, const cgm_vdc *dx_end, const cgm_vdc *dy_end)
{	OP(ELLIPTICAL_ARC);
	P(center); P(cd1_end); P(cd2_end);
	VDC4(dx_start, dy_start, dx_end, dy_end);
	END;
}

cgm_result
cgm_ELLIPTICAL_ARC_CLOSE(cgm_state *st, const cgm_point *center, const cgm_point *cd1_end, const cgm_point *cd2_end, const cgm_vdc *dx_start, const cgm_vdc *dy_start, const cgm_vdc *dx_end, const cgm_vdc *dy_end, cgm_arc_closure closure)
{	OP(ELLIPTICAL_ARC_CLOSE);
	P(center); P(cd1_end); P(cd2_end);
	VDC4(dx_start, dy_start, dx_end, dy_end); E(closure);
	END;
}

/* ---------------- Attribute elements ---------------- */

cgm_result
cgm_LINE_BUNDLE_INDEX(cgm_state *st, cgm_int index)
{	OP(LINE_BUNDLE_INDEX);
	IX(index);  st->line_bundle_index = index;
	END;
}

cgm_result
cgm_LINE_TYPE(cgm_state *st, cgm_line_type line_type)
{	OP(LINE_TYPE);
	IX((int)line_type);  st->line_type = line_type;
	END;
}

cgm_result
cgm_LINE_WIDTH(cgm_state *st, const cgm_line_width *line_width)
{	OP(LINE_WIDTH);
	VDC_R(line_width, st->picture.line_width_specification_mode);
	st->line_width = *line_width;
	END;
}

cgm_result
cgm_LINE_COLOR(cgm_state *st, const cgm_color *color)
{	OP(LINE_COLOR);
	CO(color);  st->line_color = *color;
	END;
}

cgm_result
cgm_MARKER_BUNDLE_INDEX(cgm_state *st, cgm_int index)
{	OP(MARKER_BUNDLE_INDEX);
	IX(index);  st->marker_bundle_index = index;
	END;
}

cgm_result
cgm_MARKER_TYPE(cgm_state *st, cgm_marker_type marker_type)
{	OP(MARKER_TYPE);
	IX((int)marker_type);  st->marker_type = marker_type;
	END;
}

cgm_result
cgm_MARKER_SIZE(cgm_state *st, const cgm_marker_size *marker_size)
{	OP(MARKER_SIZE);
	VDC_R(marker_size, st->picture.marker_size_specification_mode);
	st->marker_size = *marker_size;
	END;
}

cgm_result
cgm_MARKER_COLOR(cgm_state *st, const cgm_color *color)
{	OP(MARKER_COLOR);
	CO(color);  st->marker_color = *color;
	END;
}

cgm_result
cgm_TEXT_BUNDLE_INDEX(cgm_state *st, cgm_int index)
{	OP(TEXT_BUNDLE_INDEX);
	IX(index);  st->text_bundle_index = index;
	END;
}

cgm_result
cgm_TEXT_FONT_INDEX(cgm_state *st, cgm_int index)
{	OP(TEXT_FONT_INDEX);
	IX(index);  st->text_font_index = index;
	END;
}

cgm_result
cgm_TEXT_PRECISION(cgm_state *st, cgm_text_precision precision)
{	OP(TEXT_PRECISION);
	E(precision);  st->text_precision = precision;
	END;
}

cgm_result
cgm_CHARACTER_EXPANSION_FACTOR(cgm_state *st, cgm_real factor)
{	OP(CHARACTER_EXPANSION_FACTOR);
	R(factor);  st->character_expansion_factor = factor;
	END;
}

cgm_result
cgm_CHARACTER_SPACING(cgm_state *st, cgm_real spacing)
{	OP(CHARACTER_SPACING);
	R(spacing);  st->character_spacing = spacing;
	END;
}

cgm_result
cgm_TEXT_COLOR(cgm_state *st, const cgm_color *color)
{	OP(TEXT_COLOR);
	CO(color);  st->text_color = *color;
	END;
}

cgm_result
cgm_CHARACTER_HEIGHT(cgm_state *st, const cgm_vdc *height)
{	OP(CHARACTER_HEIGHT);
	VDC(height);  st->character_height = *height;
	END;
}

cgm_result
cgm_CHARACTER_ORIENTATION(cgm_state *st, const cgm_vdc *x_up, const cgm_vdc *y_up, const cgm_vdc *x_base, const cgm_vdc *y_base)
{	OP(CHARACTER_ORIENTATION);
	VDC4(x_up, y_up, x_base, y_base);
	st->character_orientation[0] = *x_up;
	st->character_orientation[1] = *y_up;
	st->character_orientation[2] = *x_base;
	st->character_orientation[3] = *y_base;
	END;
}

cgm_result
cgm_TEXT_PATH(cgm_state *st, cgm_text_path text_path)
{	OP(TEXT_PATH);
	E(text_path);  st->text_path = text_path;
	END;
}

cgm_result
cgm_TEXT_ALIGNMENT(cgm_state *st, cgm_text_alignment_horizontal align_h, cgm_text_alignment_vertical align_v, cgm_real align_cont_h, cgm_real align_cont_v)
{	OP(TEXT_ALIGNMENT);
	E(align_h); E(align_v); R(align_cont_h); R(align_cont_v);
	END;
}

cgm_result
cgm_CHARACTER_SET_INDEX(cgm_state *st, cgm_int index)
{	OP(CHARACTER_SET_INDEX);
	IX(index);  st->character_set_index = index;
	END;
}

/* See gdevcgml.c for why this isn't named cgm_ALTERNATE_.... */
cgm_result
cgm_ALT_CHARACTER_SET_INDEX(cgm_state *st, cgm_int index)
{	OP(ALTERNATE_CHARACTER_SET_INDEX);
	IX(index);  st->alternate_character_set_index = index;
	END;
}

cgm_result
cgm_FILL_BUNDLE_INDEX(cgm_state *st, cgm_int index)
{	OP(FILL_BUNDLE_INDEX);
	IX(index);  st->fill_bundle_index = index;
	END;
}

cgm_result
cgm_INTERIOR_STYLE(cgm_state *st, cgm_interior_style interior_style)
{	OP(INTERIOR_STYLE);
	E(interior_style);  st->interior_style = interior_style;
	END;
}

cgm_result
cgm_FILL_COLOR(cgm_state *st, const cgm_color *color)
{	OP(FILL_COLOR);
	CO(color);  st->fill_color = *color;
	END;
}

cgm_result
cgm_HATCH_INDEX(cgm_state *st, cgm_hatch_index hatch_index)
{	OP(HATCH_INDEX);
	IX((int)hatch_index);  st->hatch_index = hatch_index;
	END;
}

cgm_result
cgm_PATTERN_INDEX(cgm_state *st, cgm_int index)
{	OP(PATTERN_INDEX);
	IX(index);  st->pattern_index = index;
	END;
}

cgm_result
cgm_EDGE_BUNDLE_INDEX(cgm_state *st, cgm_int index)
{	OP(EDGE_BUNDLE_INDEX);
	IX(index);  st->edge_bundle_index = index;
	END;
}

cgm_result
cgm_EDGE_TYPE(cgm_state *st, cgm_edge_type edge_type)
{	OP(EDGE_TYPE);
	IX((int)edge_type);  st->edge_type = edge_type;
	END;
}

cgm_result
cgm_EDGE_WIDTH(cgm_state *st, const cgm_edge_width *edge_width)
{	OP(EDGE_WIDTH);
	VDC_R(edge_width, st->picture.edge_width_specification_mode);
	st->edge_width = *edge_width;
	END;
}

cgm_result
cgm_EDGE_COLOR(cgm_state *st, const cgm_color *color)
{	OP(EDGE_COLOR);
	CO(color);
	END;
}

cgm_result
cgm_EDGE_VISIBILITY(cgm_state *st, bool visibility)
{	OP(EDGE_VISIBILITY);
	E(visibility);  st->edge_visibility = visibility;
	END;
}

cgm_result
cgm_FILL_REFERENCE_POINT(cgm_state *st, const cgm_point *reference_point)
{	OP(FILL_REFERENCE_POINT);
	P(reference_point);  st->fill_reference_point = *reference_point;
	END;
}

/* PATTERN_TABLE */

cgm_result
cgm_PATTERN_SIZE(cgm_state *st, const cgm_vdc *x_height, const cgm_vdc *y_height, const cgm_vdc *x_width, const cgm_vdc *y_width)
{	OP(PATTERN_SIZE);
	VDC4(x_height, y_height, x_width, y_width);
	st->pattern_size[0] = *x_height;
	st->pattern_size[1] = *y_height;
	st->pattern_size[2] = *x_width;
	st->pattern_size[3] = *y_width;
	END;
}

cgm_result
cgm_COLOR_TABLE(cgm_state *st, cgm_int index, const cgm_color *values, int count)
{	int i;
	OP(COLOR_TABLE);
	CI(index);
	for ( i = 0; i < count; ++i )
	  CD(&values[i].rgb);
	END;
}

cgm_result cgm_ASPECT_SOURCE_FLAGS(cgm_state *st, const cgm_aspect_source_flag *flags, int count)
{	int i;
	OP(ASPECT_SOURCE_FLAGS);
	for ( i = 0; i < count; ++i )
	{	E(flags[i].type); E(flags[i].source);
		st->source_flags[flags[i].type] = (byte)flags[i].source;
	}
	END;
}

/* ================ Internal routines ================ */

/* Begin a command. */
private void
begin_command(cgm_state *st, cgm_op_index op)
{	uint op_word = (uint)op << cgm_op_id_shift;
	st->command[0] = (byte)(op_word >> 8);
	st->command[1] = (byte)(op_word);
	st->command_count = 4;		/* leave room for extension */
	st->command_first = true;
	st->result = cgm_result_ok;
}

/* Write the buffer for a partial command. */
/* Note that we always write an even number of bytes. */
private void
write_command(cgm_state *st, bool last)
{	byte *command = st->command;
	int count = st->command_count;
	if ( st->command_first )
	{	if ( count <= 34 )
		{	command[2] = command[0];
			command[3] = command[1] + count - 4;
			command += 2, count -= 2;
		}
		else
		{	int pcount = count - 4;
			command[1] |= 31;
			command[2] = (byte)(pcount >> 8);
			if ( !last ) command[2] |= 0x80;
			command[3] = (byte)pcount;
		}
		st->command_first = false;
	}
	else
	{	int pcount = count - 2;
		command[0] = (byte)(pcount >> 8);
		if ( !last ) command[0] |= 0x80;
		command[1] = (byte)pcount;
	}
	fwrite(command, sizeof(byte), count + (count & 1), st->file);
	st->command_count = 2;		/* leave room for extension header */
	if ( ferror(st->file) )
	  st->result = cgm_result_io_error;
}

/* End a command. */
private cgm_result
end_command(cgm_state *st)
{	write_command(st, true);
	return st->result;
}

/* Put an integer value. */
private void
put_int(cgm_state *st, cgm_int value, int precision)
{	switch ( precision )
	{
	case 32:
		put_byte(st, value >> 24);
	case 24:
		put_byte(st, value >> 16);
	case 16:
		put_byte(st, value >> 8);
	case 8:
		put_byte(st, value);
	}
}

/* Put a real value. */
private void
put_real(cgm_state *st, cgm_real value, const cgm_precision *pr)
{	if ( pr->representation == cgm_representation_floating )
	{
	}
	else
	{	/* Casting to integer simply discards the fraction, */
		/* so we need to be careful with negative values. */
		long whole = (long)value;
		double fpart;
		if ( value < whole ) --whole;
		fpart = value - whole;
		put_int(st, whole, pr->exponent_or_whole_width);
		if ( pr->fraction_width == 16 )
		{	uint fraction = (uint)(fpart * (1.0 * 0x10000));
			put_int(st, fraction, 16);
		}
		else	/* pr->fraction_width == 32 */
		{	ulong fraction =
			  (ulong)(fpart * (1.0 * 0x10000 * 0x10000));
			put_int(st, fraction, 32);
		}
	}
}

/* Put a real precision. */
private void
put_real_precision(cgm_state *st, const cgm_precision *precision)
{	I((int)precision->representation);
	I(precision->exponent_or_whole_width);
	I(precision->fraction_width);
}

/* Put a VDC. */
private void
put_vdc(cgm_state *st, const cgm_vdc *pvdc)
{	if ( st->metafile.vdc_type == cgm_vdc_integer )
	  put_int(st, pvdc->integer, st->vdc_integer_precision);
	else
	  put_real(st, pvdc->real, &st->vdc_real_precision);
}

/* Put a VDC or a real. */
private void
put_vdc_r(cgm_state *st, const cgm_line_marker_extent *extent,
  cgm_line_marker_specification_mode mode)
{	if ( mode == cgm_line_marker_absolute )
	  VDC(&extent->absolute);
	else
	  R(extent->scaled);
}

/* Put a point (pair of VDCs). */
private void
put_point(cgm_state *st, const cgm_point *ppt)
{	if ( st->metafile.vdc_type == cgm_vdc_integer )
	{	put_int(st, ppt->integer.x, st->vdc_integer_precision);
		put_int(st, ppt->integer.y, st->vdc_integer_precision);
	}
	else
	{	put_real(st, ppt->real.x, &st->vdc_real_precision);
		put_real(st, ppt->real.y, &st->vdc_real_precision);
	}
}

/* Put a list of points. */
private void
put_points(cgm_state *st, const cgm_point *ppt, int count)
{	int i;
	for ( i = 0; i < count; i++ )
	  P(ppt + i);
}

/* Put bytes. */
private void
put_bytes(cgm_state *st, const byte *data, uint length)
{	int count;
	while ( length > (count = command_max_count - st->command_count) )
	{	memcpy(st->command + st->command_count, data, count);
		st->command_count += count;
		write_command(st, false);
		data += count;
		length -= count;
	}
	memcpy(st->command + st->command_count, data, length);
	st->command_count += length;
}

/* Put a string. */
private void
put_string(cgm_state *st, const char *data, uint length)
{	/* The CGM specification seems to imply that the continuation */
	/* mechanism for commands and the mechanism for strings */
	/* are orthogonal; we take this interpretation. */
	if ( length >= 255 )
	{	put_byte(st, 255);
		while ( length > 32767 )
		{	put_int(st, 65535, 2);
			put_bytes(st, (const byte *)data, 32767);
			data += 32767;
			length -= 32767;
		}
	}
	put_byte(st, length);
	put_bytes(st, (const byte *)data, length);
}

/* Put a color. */
private void
put_color(cgm_state *st, const cgm_color *color)
{	if ( st->picture.color_selection_mode == cgm_color_selection_indexed )
	  CI(color->index);
	else
	  CD(&color->rgb);
}

/* Put an RGB value. */
private void
put_rgb(cgm_state *st, const cgm_rgb *rgb)
{	put_int(st, rgb->r, st->metafile.color_precision);
	put_int(st, rgb->g, st->metafile.color_precision);
	put_int(st, rgb->b, st->metafile.color_precision);
}
