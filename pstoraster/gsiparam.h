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

/*$Id: gsiparam.h,v 1.2 2000/03/08 23:14:43 mike Exp $ */
/* Image parameter definition */

#ifndef gsiparam_INCLUDED
#  define gsiparam_INCLUDED

#include "gsmatrix.h"

/* ---------------- Image parameters ---------------- */

/*
 * Unfortunately, we defined the gs_image_t type as designating an ImageType
 * 1 image or mask before we realized that there were going to be other
 * ImageTypes.  We could redefine this type to include a type field without
 * perturbing clients, but it would break implementations of driver
 * begin_image procedures, since they are currently only prepared to handle
 * ImageType 1 images and would have to be modified to check the ImageType.
 * Therefore, we use gs_image_common_t for an abstract image type, and
 * gs_image<n>_t for the various ImageTypes.
 */

/*
 * Define the data common to all image types.  The type structure is
 * opaque here, defined in gxiparam.h.
 */
#ifndef gx_image_type_DEFINED
#  define gx_image_type_DEFINED
typedef struct gx_image_type_s gx_image_type_t;

#endif
#define gs_image_common\
	const gx_image_type_t *type;\
		/*\
		 * Define the transformation from user space to image space.\
		 */\
	gs_matrix ImageMatrix
typedef struct gs_image_common_s {
    gs_image_common;
} gs_image_common_t;

#define public_st_gs_image_common() /* in gxiinit.c */\
  gs_public_st_simple(st_gs_image_common, gs_image_common_t,\
    "gs_image_common_t")

/*
 * Define the maximum number of components in image data.  When we
 * support DeviceN color spaces, we will have to rethink this.
 * 5 is either CMYK + alpha or mask + CMYK.
 */
#define gs_image_max_components 5

/*
 * Define the structure for defining data common to ImageType 1 images,
 * ImageType 3 DataDicts and MaskDicts, and ImageType 4 images -- i.e.,
 * all the image types that use explicitly supplied data.  It follows
 * closely the discussion on pp. 219-223 of the PostScript Language
 * Reference Manual, Second Edition, with the following exceptions:
 *
 *      DataSource and MultipleDataSources are not members of this
 *      structure, since the structure doesn't take a position on
 *      how the data are actually supplied.
 */
#define gs_data_image_common\
	gs_image_common;\
		/*\
		 * Define the width of source image in pixels.\
		 */\
	int Width;\
		/*\
		 * Define the height of source image in pixels.\
		 */\
	int Height;\
		/*\
		 * Define B, the number of bits per pixel component.\
		 * Currently this must be 1 for masks.\
		 */\
	int BitsPerComponent;\
		/*\
		 * Define the linear remapping of the input values.\
		 * For the I'th pixel component, we start by treating\
		 * the B bits of component data as a fraction F between\
		 * 0 and 1; the actual component value is then\
		 * Decode[I*2] + F * (Decode[I*2+1] - Decode[I*2]).\
		 * For masks, only the first two entries are used;\
		 * they must be 1,0 for write-0s masks, 0,1 for write-1s.\
		 */\
	float Decode[gs_image_max_components * 2];\
		/*\
		 * Define whether to smooth the image.\
		 */\
	bool Interpolate
typedef struct gs_data_image_s {
    gs_data_image_common;
} gs_data_image_t;

#define public_st_gs_data_image() /* in gxiinit.c */\
  gs_public_st_simple(st_gs_data_image, gs_data_image_t,\
    "gs_data_image_t")

/*
 * Define the data common to ImageType 1 images, ImageType 3 DataDicts,
 * and ImageType 4 images -- i.e., all the image types that provide pixel
 * (as opposed to mask) data.  The following are added to the PostScript
 * image parameters:
 *
 *      format is not PostScript or PDF standard: it is normally derived
 *      from MultipleDataSources.
 *
 *      ColorSpace is added from PDF.
 *
 *      CombineWithColor is not PostScript or PDF standard: see the
 *      RasterOp section of language.doc for a discussion of
 *      CombineWithColor.
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

/* Define an opaque type for a color space. */
#ifndef gs_color_space_DEFINED
#  define gs_color_space_DEFINED
typedef struct gs_color_space_s gs_color_space;

#endif

#define gs_pixel_image_common\
	gs_data_image_common;\
		/*\
		 * Define how the pixels are divided up into planes.\
		 */\
	gs_image_format_t format;\
		/*\
		 * Define the source color space (must be NULL for masks).\
		 */\
	const gs_color_space *ColorSpace;\
		/*\
		 * Define whether to use the drawing color as the\
		 * "texture" for RasterOp.  For more information,\
		 * see the discussion of RasterOp in language.doc.\
		 */\
	bool CombineWithColor
typedef struct gs_pixel_image_s {
    gs_pixel_image_common;
} gs_pixel_image_t;

#define public_st_gs_pixel_image() /* in gxiinit.c */\
  gs_public_st_ptrs1(st_gs_pixel_image, gs_pixel_image_t,\
    "gs_data_image_t", pixel_image_enum_ptrs, pixel_image_reloc_ptrs,\
    ColorSpace)

/*
 * Define an ImageType 1 image.  ImageMask is an added member from PDF.
 * adjust and Alpha are not PostScript or PDF standard.
 */
typedef enum {
    /* No alpha.  This must be 0 for true-false tests. */
    gs_image_alpha_none = 0,
    /* Alpha precedes color components. */
    gs_image_alpha_first,
    /* Alpha follows color components. */
    gs_image_alpha_last
} gs_image_alpha_t;

typedef struct gs_image1_s {
    gs_pixel_image_common;
    /*
     * Define whether this is a mask or a solid image.
     * For masks, Alpha must be 'none'.
     */
    bool ImageMask;
    /*
     * Define whether to expand each destination pixel, to make
     * masked characters look better.  Only used for masks.
     */
    bool adjust;
    /*
     * Define whether there is an additional component providing
     * alpha information for each pixel, in addition to the
     * components implied by the color space.
     */
    gs_image_alpha_t Alpha;
} gs_image1_t;

/*
 * In standard PostScript Level 1 and 2, this is the only defined ImageType.
 */
typedef gs_image1_t gs_image_t;

/*
 * Define procedures for initializing the standard forms of image structures
 * to default values.  Note that because these structures may add more
 * members in the future, all clients constructing gs_*image*_t values
 * *must* start by initializing the value by calling one of the following
 * procedures.  Note also that these procedures do not set the image type.
 */
void
  /*
   * Sets ImageMatrix to the identity matrix.
   */
     gs_image_common_t_init(P1(gs_image_common_t * pic)),	/*
								 * Also sets Width = Height = 0, BitsPerComponent = 1,
								 * format = chunky, Interpolate = false.
								 * If num_components = N > 0, sets the first N elements of Decode to (0, 1);
								 * if num_components = N < 0, sets the first -N elements of Decode to (1, 0);
								 * if num_components = 0, doesn't set Decode.
								 */
     gs_data_image_t_init(P2(gs_data_image_t * pim, int num_components)),
  /*
   * Also sets CombineWithColor = false, ColorSpace = color_space, Alpha =
   * none.  num_components is obtained from ColorSpace; if ColorSpace =
   * NULL or ColorSpace is a Pattern space, num_components is taken as 0
   * (Decode is not initialized).
   */
    gs_pixel_image_t_init(P2(gs_pixel_image_t * pim,
			     const gs_color_space * color_space));

/*
 * Initialize an ImageType 1 image (or imagemask).  Also sets ImageMask,
 * adjust, and Alpha, and the image type.  For masks, write_1s = false
 * paints 0s, write_1s = true paints 1s.  This is consistent with the
 * "polarity" operand of the PostScript imagemask operator.
 */
void gs_image_t_init(P2(gs_image_t * pim, const gs_color_space * pcs));
void gs_image_t_init_mask(P2(gs_image_t * pim, bool write_1s));

/* init_gray and init_color require a (const) imager state. */
#define gs_image_t_init_gray(pim, pis)\
  gs_image_t_init(pim, gs_cspace_DeviceGray(pis))
#define gs_image_t_init_rgb(pim, pis)\
  gs_image_t_init(pim, gs_cspace_DeviceRGB(pis))
#define gs_image_t_init_cmyk(pim, pis)\
  gs_image_t_init(pim, gs_cspace_DeviceCMYK(pis))

/****** REMAINDER OF FILE UNDER CONSTRUCTION. PROCEED AT YOUR OWN RISK. ******/

#if 0

/* ---------------- Services ---------------- */

/*
   In order to make the driver's life easier, we provide the following callback
   procedure:
 */

int gx_map_image_color(P5(gx_device * dev,
			  const gs_image_t * pim,
			  const gx_color_rendering_info * pcri,
			  const uint components[4],
			  gx_drawing_color * pdcolor));

/*
   Map a source color to a drawing color.  The components are simply the pixel
   component values from the input data, i.e., 1 to 4 B-bit numbers from the
   source data.  Return 0 if the operation succeeded, or a negative error code.
 */

#endif /*************************************************************** */

#endif /* gsiparam_INCLUDED */
