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

/*$Id: gxbitfmt.h,v 1.1 2000/03/13 18:58:45 mike Exp $ */
/* Definitions for bitmap storage formats */

#ifndef gxbitfmt_INCLUDED
#  define gxbitfmt_INCLUDED

/*
 * Several operations, such as the get_bits_rectangle driver procedure, can
 * take and/or produce data in a flexible variety of formats; the ability to
 * describe how bitmap data is stored is useful in other contexts as well.
 * We define bitmap storage formats using a bit mask: this allows a
 * procedure to ask for, or offer to provide, data in more than one format.
 */

typedef ulong gx_bitmap_format_t;

    /*
     * Define the supported color space alternatives.
     */

#define GB_COLORS_NATIVE (1L<<0)  /* native representation (DevicePixel) */
#define GB_COLORS_GRAY   (1L<<1)  /* DeviceGray */
#define GB_COLORS_RGB    (1L<<2)  /* DeviceRGB */
#define GB_COLORS_CMYK   (1L<<3)  /* DeviceCMYK */

#define GB_COLORS_STANDARD_ALL\
  (GB_COLORS_GRAY | GB_COLORS_RGB | GB_COLORS_CMYK)
#define GB_COLORS_ALL\
  (GB_COLORS_NATIVE | GB_COLORS_STANDARD_ALL)
#define gb_colors_for_device(dev)\
  ((dev)->color_info.num_components == 4 ? GB_COLORS_CMYK :\
   (dev)->color_info.num_components == 3 ? GB_COLORS_RGB : GB_COLORS_GRAY)
#define GB_COLORS_NAMES\
  "colors_native", "colors_Gray", "colors_RGB", "colors_CMYK"

    /*
     * Define whether alpha information is included.  For GB_COLORS_NATIVE,
     * all values other than GB_ALPHA_NONE are equivalent.
     */

#define GB_ALPHA_NONE  (1L<<4)  /* no alpha */
#define GB_ALPHA_FIRST (1L<<5)  /* include alpha as first component */
#define GB_ALPHA_LAST  (1L<<6)  /* include alpha as last component */
  /*unused*/           /*(1L<<7)*/

#define GB_ALPHA_ALL\
  (GB_ALPHA_NONE | GB_ALPHA_FIRST | GB_ALPHA_LAST)
#define GB_ALPHA_NAMES\
  "alpha_none", "alpha_first", "alpha_last", "?alpha_unused?"

    /*
     * Define the supported depths per component for GB_COLORS_STANDARD.
     */

#define GB_DEPTH_1  (1L<<8)
#define GB_DEPTH_2  (1L<<9)
#define GB_DEPTH_4  (1L<<10)
#define GB_DEPTH_8  (1L<<11)
#define GB_DEPTH_12 (1L<<12)
#define GB_DEPTH_16 (1L<<13)
  /*unused1*/       /*(1L<<14)*/
  /*unused2*/       /*(1L<<15)*/

#define GB_DEPTH_ALL\
  (GB_DEPTH_1 | GB_DEPTH_2 | GB_DEPTH_4 | GB_DEPTH_8 |\
   GB_DEPTH_12 | GB_DEPTH_16)
#define GB_DEPTH_NAMES\
  "depth_1", "depth_2", "depth_4", "depth_8",\
  "depth_12", "depth_16", "?depth_unused1?", "?depth_unused2?"

/* Find the maximum depth of an options mask. */
#define GB_OPTIONS_MAX_DEPTH(opt)\
"\
\000\001\002\002\004\004\004\004\010\010\010\010\010\010\010\010\
\014\014\014\014\014\014\014\014\014\014\014\014\014\014\014\014\
\020\020\020\020\020\020\020\020\020\020\020\020\020\020\020\020\
\020\020\020\020\020\020\020\020\020\020\020\020\020\020\020\020\
"[((opt) >> 8) & 0x3f]
/* Find the depth of an options mask with exactly 1 bit set. */
#define GB_OPTIONS_DEPTH(opt)\
  ((((opt) >> 8) & 0xf) |\
   "\000\000\014\020"[((opt) >> 12) & 3])

    /*
     * Define the supported packing formats.  Currently, GB_PACKING_PLANAR is
     * only partially supported, and GB_PACKING_BIT_PLANAR is hardly supported
     * at all.
     */

#define GB_PACKING_CHUNKY     (1L<<16)
#define GB_PACKING_PLANAR     (1L<<17)  /* 1 plane per component */
#define GB_PACKING_BIT_PLANAR (1L<<18)  /* 1 plane per bit */
  /*unused*/                  /*(1L<<19)*/

#define GB_PACKING_ALL\
  (GB_PACKING_CHUNKY | GB_PACKING_PLANAR | GB_PACKING_BIT_PLANAR)
#define GB_PACKING_NAMES\
  "packing_chunky", "packing_planar", "packing_bit_planar", "?packing_unused?"

    /*
     * Define the possible methods of returning data.
     */

#define GB_RETURN_COPY    (1L<<20)  /* copy to client's buffer */
#define GB_RETURN_POINTER (1L<<21)  /* return pointers to data */

#define GB_RETURN_ALL\
  (GB_RETURN_COPY | GB_RETURN_POINTER)
#define GB_RETURN_NAMES\
  "return_copy", "return_pointer"

    /*
     * Define the allowable alignments.  This is only relevant for
     * GB_RETURN_POINTER: for GB_RETURN_COPY, any alignment is acceptable.
     */

#define GB_ALIGN_STANDARD (1L<<22)  /* require standard bitmap alignment */
#define GB_ALIGN_ANY      (1L<<23)  /* any alignment is acceptable */

#define GB_ALIGN_ALL\
  (GB_ALIGN_ANY | GB_ALIGN_STANDARD)
#define GB_ALIGN_NAMES\
  "align_any", "align_standard"

    /*
     * Define the allowable X offsets.  GB_OFFSET_ANY is only relevant for
     * GB_RETURN_POINTER: for GB_RETURN_COPY, clients must specify the
     * offset so they know how much space to allocate.
     */

#define GB_OFFSET_0         (1L<<24)  /* no offsetting */
#define GB_OFFSET_SPECIFIED (1L<<25)  /* client-specified offset */
#define GB_OFFSET_ANY       (1L<<26)  /* any offset is acceptable */
					/* (for GB_RETURN_POINTER only) */
  /*unused*/                /*(1L<<27)*/

#define GB_OFFSET_ALL\
  (GB_OFFSET_0 | GB_OFFSET_SPECIFIED | GB_OFFSET_ANY)
#define GB_OFFSET_NAMES\
  "offset_0", "offset_specified", "offset_any", "?offset_unused?"

    /*
     * Define the allowable rasters.  GB_RASTER_ANY is only relevant for
     * GB_RETURN_POINTER, for the same reason as GB_OFFSET_ANY.
     * Note also that if GB_ALIGN_STANDARD and GB_RASTER_SPECIFIED are
     * both chosen and more than one scan line is being transferred,
     * the raster value must also be aligned (i.e., 0 mod align_bitmap_mod).
     * Implementors are not required to check this.
     */

    /*
     * Standard raster is bitmap_raster(dev->width) for return_ptr,
     * bitmap_raster(x_offset + width) for return_copy,
     * padding per alignment.
     */
#define GB_RASTER_STANDARD  (1L<<28)
#define GB_RASTER_SPECIFIED (1L<<29)  /* any client-specified raster */
#define GB_RASTER_ANY       (1L<<30)  /* any raster is acceptable (for */
					/* GB_RETURN_POINTER only) */

#define GB_RASTER_ALL\
  (GB_RASTER_STANDARD | GB_RASTER_SPECIFIED | GB_RASTER_ANY)
#define GB_RASTER_NAMES\
  "raster_standard", "raster_specified", "raster_any"

/* Define names for debugging printout. */
#define GX_BITMAP_FORMAT_NAMES\
  GB_COLORS_NAMES, GB_ALPHA_NAMES, GB_DEPTH_NAMES, GB_PACKING_NAMES,\
  GB_RETURN_NAMES, GB_ALIGN_NAMES, GB_OFFSET_NAMES, GB_RASTER_NAMES

#endif /* gxbitfmt_INCLUDED */
