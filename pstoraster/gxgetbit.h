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

/*$Id: gxgetbit.h,v 1.1 2000/03/13 18:58:46 mike Exp $ */
/* Interface for get_bits_rectangle driver procedure */

#ifndef gxgetbit_INCLUDED
#  define gxgetbit_INCLUDED

#include "gxbitfmt.h"

/* The parameter record typedef is also in gxdevcli.h. */
#ifndef gs_get_bits_params_DEFINED
#  define gs_get_bits_params_DEFINED
typedef struct gs_get_bits_params_s gs_get_bits_params_t;
#endif

/*
 * We define the options for get_bits_rectangle here in a separate file
 * so that the great majority of driver implementors and clients, which
 * don't care about the details, don't need to be recompiled if the set
 * of options changes.
 */
typedef gx_bitmap_format_t gs_get_bits_options_t;

/*
 * Define the parameter record passed to get_bits_rectangle.
 * get_bits_rectangle may update members of this structure if
 * the options allow it to choose their values, and always updates options
 * to indicate what options were actually used (1 option per group).
 */
struct gs_get_bits_params_s {
    gs_get_bits_options_t options;
    byte *data[32];
    int x_offset;		/* in returned data */
    uint raster;
};

/*
 * gx_bitmap_format_t defines the options passed to get_bits_rectangle,
 * which indicate which formats are acceptable for the returned data.  If
 * successful, get_bits_rectangle sets the options member of the parameter
 * record to indicate what options were chosen -- 1 per group, and never the
 * _ANY option.  Note that the chosen option is not necessarily one that
 * appeared in the original options: for example, if GB_RASTER_ANY is the
 * only raster option originally set, the chosen option will be
 * GB_RASTER_STANDARD or GB_RASTER_SPECIFIED.
 *
 * If the options mask is 0, get_bits_rectangle must set it to the
 * complete set of supported options and return an error.  This allows
 * clients to determine what options are supported without actually doing
 * a transfer.
 *
 * All devices must support at least one option in each group, and must
 * support GB_COLORS_NATIVE.
 *
 * NOTE: the current default implementation supports only the following
 * options in their respective groups (i.e., any other options must be
 * supported directly by the device):
 *      GB_DEPTH_8
 *      GB_PACKING_CHUNKY
 *      GB_RETURN_COPY
 * The current default implementation also requires that all devices
 * support GB_PACKING_CHUNKY.  */

/* ---------------- Procedures ---------------- */

/* Try to implement get_bits_rectangle by returning a pointer. */
int gx_get_bits_return_pointer(P6(gx_device * dev, int x, int h,
				  gs_get_bits_params_t * params,
				  gs_get_bits_options_t stored,
				  byte * stored_base));

/* Implement get_bits_rectangle by copying. */
int gx_get_bits_copy(P8(gx_device * dev, int x, int w, int h,
			gs_get_bits_params_t * params,
			gs_get_bits_options_t stored,
			const byte * src_base, uint dev_raster));

#endif /* gxgetbit_INCLUDED */
