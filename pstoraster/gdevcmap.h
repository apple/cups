/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevcmap.h,v 1.1 2000/03/13 19:00:46 mike Exp $ */
/* Interface to special color mapping device */

#ifndef gdevcmap_INCLUDED
#  define gdevcmap_INCLUDED

/* Define the color mapping algorithms. */
typedef enum {

    /* Don't change the color. */

    device_cmap_identity = 0,

    /* Snap each RGB primary component to 0 or 1 individually. */

    device_cmap_snap_to_primaries,

    /* Snap black to white, other colors to black. */

    device_cmap_color_to_black_over_white,

    /* Convert to a gray shade of the correct brightness. */

    device_cmap_monochrome

} gx_device_color_mapping_method_t;

#define device_cmap_max_method device_cmap_monochrome

/* Define the color mapping forwarding device. */
typedef struct gx_device_cmap_s {
    gx_device_forward_common;
    gx_device_color_mapping_method_t mapping_method;
} gx_device_cmap;

extern_st(st_device_cmap);
#define public_st_device_cmap()	/* in gdevcmap.c */\
  gs_public_st_suffix_add0_final(st_device_cmap, gx_device_cmap,\
    "gx_device_cmap", device_cmap_enum_ptrs, device_cmap_reloc_ptrs,\
    gx_device_finalize, st_device_forward)

/* Initialize a color mapping device.  Do this just once after allocation. */
int gdev_cmap_init(P3(gx_device_cmap * dev, gx_device * target,
		      gx_device_color_mapping_method_t mapping_method));

/* Set the color mapping method.  This may be called at any time. */
int gdev_cmap_set_method(P2(gx_device_cmap * dev,
			  gx_device_color_mapping_method_t mapping_method));

#endif /* gdevcmap_INCLUDED */
