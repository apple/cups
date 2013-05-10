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

/* gsiparam.h */
/* Image parameter definition */
/* Requires gsmatrix.h */

#ifndef gsiparam_INCLUDED
#  define gsiparam_INCLUDED

/* ---------------- Image parameters ---------------- */

/* Define an opaque type for a color space. */
#ifndef gs_color_space_DEFINED
#  define gs_color_space_DEFINED
typedef struct gs_color_space_s gs_color_space;
#endif

/*
 * Define the structure for specifying image data.  It follows closely
 * the discussion on pp. 219-223 of the PostScript Language Reference Manual,
 * Second Edition, with the following exceptions:
 *
 *	ColorSpace and ImageMask are added members from PDF.
 *
 *	MultipleDataSources is not a member of this structure,
 *	but an argument to gs_image_init.
 *
 *	adjust and CombineWithColor are not PostScript or PDF standard
 *	(see the RasterOp section of language.doc for a discussion of
 *	CombineWithColor).
 */
typedef enum {
		/* Single plane, chunky pixels. */
	gs_image_format_chunky = 0,
		/* num_components planes, chunky components. */
	gs_image_format_component_planar = 1,
		/* BitsPerComponent * num_components planes, 1 bit per plane */
		/****** NOT SUPPORTED YET, DO NOT USE ******/
	gs_image_format_bit_planar = 2
} gs_image_format_t;

typedef struct gs_image_s {
		/*
		 * Define the width of source image in pixels.
		 */
	int Width;
		/*
		 * Define the height of source image in pixels.
		 */
	int Height;
		/*
		 * Define the transformation from user space to image space.
		 */
	gs_matrix ImageMatrix;
		/*
		 * Define B, the number of bits per pixel component.
		 * Currently this must be 1 for masks.
		 */
	int BitsPerComponent;
		/*
		 * Define the source color space (must be NULL for masks).
		 */
	const gs_color_space *ColorSpace;
		/*
		 * Define the linear remapping of the input values.
		 * For the I'th pixel component, we start by treating
		 * the B bits of component data as a fraction F between
		 * 0 and 1; the actual component value is then
		 * Decode[I*2] + F * (Decode[I*2+1] - Decode[I*2]).
		 * For masks, only the first two entries are used;
		 * they must be 1,0 for write-0s masks, 0,1 for write-1s.
		 */
	float Decode[8];
		/*
		 * Define whether to smooth the image.
		 */
	bool Interpolate;
		/*
		 * Define whether this is a mask or a solid image.
		 */
	bool ImageMask;
		/***
		 *** The following are not PostScript standard.
		 ***/
		/*
		 * Define whether to expand each destination pixel, to make
		 * masked characters look better (only used for masks).
		 */
	bool adjust;
		/*
		 * Define whether to use the drawing color as the
		 * "texture" for RasterOp.  For more information,
		 * see the discussion of RasterOp in language.doc.
		 */
	bool CombineWithColor;
} gs_image_t;

/*
 * Define procedures for initializing a gs_image_t to default values.
 * For masks, write_1s = false paints 0s, write_1s = true paints 1s.
 * This is consistent with the "polarity" operand of the PostScript
 * imagemask operator.
 */
void
  gs_image_t_init_gray(P1(gs_image_t *pim)),
  gs_image_t_init_color(P1(gs_image_t *pim)),/* general color, initially RGB */
  gs_image_t_init_mask(P2(gs_image_t *pim, bool write_1s));

/* ---------------- Driver interface for images ---------------- */

/*
 * When we call the driver image_data procedure, we pass values X, Y, W,
 * and H to specify the rectangle of source data.  The default is to pass
 * the entire image in a single call, in which case X = Y = 0,
 * W = Width, H = Height.  However, banding may require multiple calls,
 * each call passing only a subset of the image data; also,
 * some environments may not pass full rows or the full height.
 * We define some flags that define what kind of subset may be passed.
 * These flags may be or'ed together.  For example:
 *
 *	Normal PostScript images will have flags = 2+16
 *	Normal PCL-5 images will have flags = 2+16+64
 *	Banded unrotated images will have flags = 1+2+16
 *	Banded rotated images will have flags = 1+2+4+8+16+64
 *
 * Add 32 to any of the above for very long rows.
 */
typedef enum {
		/* We may skip some rows at the top (beginning), */
		/* i.e., the first Y value may not be zero. */
	gs_image_shape_clip_top = 1,
		/* We may skip some rows at the bottom (end), */
		/* i.e., the last Y+H value may not equal Height. */
	gs_image_shape_clip_bottom = 2,
		/* We may skip some data on the left side, */
		/* i.e., some X value may not be zero. */
	gs_image_shape_clip_left = 4,
		/* We may skip some data on the right side, */
		/* i.e., some X+W value may not equal Width. */
	gs_image_shape_clip_right = 8,
		/* We may pass rows of image in more than one call, */
		/* i.e., Y may not have the same value on all calls. */
	gs_image_shape_rows = 16,
		/* We may pass a single row in pieces, */
		/* i.e., there may be multiple calls with the same Y. */
	gs_image_shape_split_row = 32,
		/* Different rows may have different widths, */
		/* i.e., X or X+W may not have the same value on all calls. */
	gs_image_shape_varying_width = 64
} gs_image_shape_t;

/****** REMAINDER OF FILE UNDER CONSTRUCTION. PROCEED AT YOUR OWN RISK. ******/

#if 0	/****************************************************************/

/* Define the color space of the image data. */
typedef enum {
	gs_color_space_index_DeviceGray = 0,	/* 1 */
	gs_color_space_index_DeviceRGB,		/* 3 */
	gs_color_space_index_DeviceCMYK,	/* 4 */
	gs_color_space_index_CIEDEFG,		/* 4 */
	gs_color_space_index_CIEDEF,		/* 3 */
	gs_color_space_index_CIEABC,		/* 3 */
	gs_color_space_index_CIEA		/* 1 */
} gs_image_color_space_index;

/*
 * Define an opaque structure type for referring to the internal structures
 * that define color rendering information (transfer function, black
 * generation, undercolor removal, CIE rendering dictionary, halftoning).
 */
typedef struct gx_color_rendering_info_s gx_color_rendering_info;

/*
 * Define a structure for defining the color space of an image.
 * ****** MUST RECONCILE THIS WITH gs_color_space. ******
 */
typedef
	struct {
		/* Define the index of the base color space. */
	  gs_color_space_index base_index;
		/* Provide the parameters of a CIE space, if required. */
	  void *base_data;
		/* If this is an indexed space, palette.data points to */
		/* the palette, consisting of N bytes per entry where */
		/* N is the number of color space components; if this is */
		/* not an indexed space, palette.data = 0. */
	  gs_const_string palette;
	}
gx_image_color_space;

/* ---------------- Procedures ---------------- */

/****** MOVE pcri TO IMAGER STATE ******/

/* ---------------- Services ---------------- */

/*
In order to make the driver's life easier, we provide the following callback
procedure:
*/

int gx_map_image_color(P5(gx_device *dev,
			  const gs_image_t *pim,
			  const gx_color_rendering_info *pcri,
			  const uint components[4],
			  gx_drawing_color *pdcolor));

/*
Map a source color to a drawing color.  The components are simply the pixel
component values from the input data, i.e., 1 to 4 B-bit numbers from the
source data.  Return 0 if the operation succeeded, or a negative error code.
*/

#endif	/****************************************************************/

#endif					/* gsiparam_INCLUDED */
