/* Copyright (C) 1995, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevht.h,v 1.2 2000/03/08 23:14:23 mike Exp $ */
/* Requires gxdevice.h */

#ifndef gdevht_INCLUDED
#  define gdevht_INCLUDED

#include "gzht.h"

/*
 * A halftoning device converts between a non-halftoned device color space
 * (e.g., 8-bit gray) and a halftoned space (e.g., 1-bit black and white).
 * We represent colors by packing the two colors being halftoned and the
 * halftone level into a gx_color_index.
 */
typedef struct gx_device_ht_s {
    gx_device_forward_common;
    /* Following + target are set before opening. */
    const gx_device_halftone *dev_ht;
    gs_int_point ht_phase;	/* halftone phase from gstate */
    /* Following are computed when device is opened. */
    int color_shift;		/* # of bits of color */
    int level_shift;		/* = color_shift * 2 */
    gx_color_index color_mask;	/* (1 << color_shift) - 1 */
    gs_int_point phase;		/* halftone tile offset */
} gx_device_ht;

#endif /* gdevht_INCLUDED */
