/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gzacpath.h */
/* Private representation of clipping path accumulator */
/* Requires gxdevice.h, gzcpath.h */

/* Device for accumulating a rectangle list. */
typedef struct gx_device_cpath_accum_s {
	gx_device_common;
	gs_memory_t *list_memory;		/* set by client */
	gs_int_rect bbox;
	gx_clip_list list;
} gx_device_cpath_accum;

/* Start accumulating a clipping path. */
void gx_cpath_accum_begin(P2(gx_device_cpath_accum *padev, gs_memory_t *mem));

/* Finish accumulating a clipping path. */
int gx_cpath_accum_end(P2(const gx_device_cpath_accum *padev,
			  gx_clip_path *pcpath));

/* Discard an accumulator in case of error. */
void gx_cpath_accum_discard(P1(gx_device_cpath_accum *padev));
