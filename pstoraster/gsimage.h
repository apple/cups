/* Copyright (C) 1992, 1995, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsimage.h,v 1.2 2000/03/08 23:14:43 mike Exp $ */
/* Requires gsstate.h */

#ifndef gsimage_INCLUDED
#  define gsimage_INCLUDED

#include "gsiparam.h"

/*
 * Create an image enumerator given image parameters and a graphics state.
 * This calls the device's begin_typed_image procedure with appropriate
 * parameters.  Note that this is an enumerator that requires entire
 * rows of data, not the buffered enumerator used by the procedures below:
 * for this reason, we may move the prototype elsewhere in the future.
 */
#ifndef gx_image_enum_common_t_DEFINED
#  define gx_image_enum_common_t_DEFINED
typedef struct gx_image_enum_common_s gx_image_enum_common_t;

#endif
int gs_image_begin_typed(P4(const gs_image_common_t * pic, gs_state * pgs,
			  bool uses_color, gx_image_enum_common_t ** ppie));

/*
 * The image painting interface uses an enumeration style:
 * the client initializes an enumerator, then supplies data incrementally.
 */
typedef struct gs_image_enum_s gs_image_enum;
gs_image_enum *gs_image_enum_alloc(P2(gs_memory_t *, client_name_t));

/*
 * image_init returns 1 for an empty image, 0 normally, <0 on error.
 * Note that image_init serves for both image and imagemask,
 * depending on the value of ImageMask in the image structure.
 */
#ifndef gx_device_DEFINED
#  define gx_device_DEFINED
typedef struct gx_device_s gx_device;

#endif
int gs_image_common_init(P5(gs_image_enum * penum, gx_image_enum_common_t * pie,
			    const gs_data_image_t * pim,
			    gs_memory_t * mem, gx_device * dev));
int gs_image_init(P4(gs_image_enum * penum, const gs_image_t * pim,
		     bool MultipleDataSources, gs_state * pgs));
int gs_image_next(P4(gs_image_enum * penum, const byte * dbytes,
		     uint dsize, uint * pused));

/*
 * Return the number of bytes of data per row
 * (per plane, if MultipleDataSources is true).
 */
uint gs_image_bytes_per_plane_row(P2(const gs_image_enum * penum, int plane));

#define gs_image_bytes_per_row(penum)\
  gs_image_bytes_per_plane_row(penum, 0)
/* Clean up after processing an image. */
void gs_image_cleanup(P1(gs_image_enum * penum));

#endif /* gsimage_INCLUDED */
