/* Copyright (C) 1992, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gsimage.h */
/* Client interface to image painting */
/* Requires gsstate.h */
#include "gsiparam.h"

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
int gs_image_init(P4(gs_image_enum *penum, const gs_image_t *pim,
		     bool MultipleDataSources, gs_state *pgs));
int gs_image_next(P4(gs_image_enum *penum, const byte *dbytes,
		     uint dsize, uint *pused));
/*
 * Return the number of bytes of data per row
 * (per plane, if MultipleDataSources is true).
 */
uint gs_image_bytes_per_row(P1(const gs_image_enum *penum));
/* Clean up after processing an image. */
void gs_image_cleanup(P1(gs_image_enum *penum));
