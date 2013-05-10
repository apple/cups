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

/* gdevht.h */
/* Definitions for halftoning device */
/* Requires gxdevice.h */
#include "gzht.h"

/*
 * A halftoning device converts between a non-halftoned device color space
 * (e.g., 8-bit gray) and a halftoned space (e.g., 1-bit black and white).
 * Currently, the target space must not exceed 8 bits per pixel, so that
 * we can pack two target colors and a halftone level into a gx_color_index.
 */
#define ht_target_max_depth 8
#define ht_level_depth (sizeof(gx_color_index) * 8 - ht_target_max_depth * 2)
typedef struct gx_device_ht_s {
	gx_device_forward_common;
		/* Following are set before opening. */
	const gx_device_halftone *dev_ht;
	gs_int_point ht_phase;		/* halftone phase from gstate */
		/* Following are computed when device is opened. */
	gs_int_point phase;		/* halftone tile offset */
} gx_device_ht;

/* Macro for casting gx_device argument */
#define htdev ((gx_device_ht *)dev)
