/* Copyright (C) 1989, 1992, 1993, 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevmswn.h */
/* Shared definitions for Microsoft Windows 3.n drivers */
#include "string_.h"
#include <stdlib.h>
#include "dos_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "memory_.h"

#include "windows_.h"
#include <shellapi.h>
#include "gp_mswin.h"

typedef struct gx_device_win_s gx_device_win;

/* Utility routines in gdevmswn.c */
LPLOGPALETTE win_makepalette(P1(gx_device_win *));
int win_nomemory(P0());
void win_update(P1(gx_device_win *));

/* Device procedures shared by all implementations. */
/* Implementations may wrap their own code around _open and _close. */
dev_proc_open_device(win_open);
dev_proc_sync_output(win_sync_output);
dev_proc_output_page(win_output_page);
dev_proc_close_device(win_close);
dev_proc_map_rgb_color(win_map_rgb_color);
dev_proc_map_color_rgb(win_map_color_rgb);
dev_proc_get_params(win_get_params);
dev_proc_put_params(win_put_params);
dev_proc_get_xfont_procs(win_get_xfont_procs);
dev_proc_get_alpha_bits(win_get_alpha_bits);

/* Common part of the device descriptor. */

#define win_proc_copy_to_clipboard(proc)\
  void proc(P1(gx_device_win *))

#define win_proc_repaint(proc)\
  void proc(P8(gx_device_win *, HDC, int, int, int, int, int, int))

#define win_proc_alloc_bitmap(proc)\
  int proc(P2(gx_device_win *, gx_device *))

#define win_proc_free_bitmap(proc)\
  void proc(P1(gx_device_win *))

#define win_gsview_sizeof 80

#define gx_device_win_common\
	int BitsPerPixel;\
	int nColors;\
	int alpha_text;\
	int alpha_graphics;\
	byte *mapped_color_flags;\
		/* Implementation-specific procedures */\
	win_proc_alloc_bitmap((*alloc_bitmap));\
	win_proc_free_bitmap((*free_bitmap));\
		/* Handles */\
	HPALETTE himgpalette;\
	LPLOGPALETTE limgpalette

/* The basic window device */
struct gx_device_win_s {
	gx_device_common;
	gx_device_win_common;
};

/* Initial values for width and height */
#define INITIAL_RESOLUTION 96
#define INITIAL_WIDTH (INITIAL_RESOLUTION * 85 / 10 + 1)
#define INITIAL_HEIGHT (INITIAL_RESOLUTION * 11 + 1)

/* A macro for casting the device argument */
#define wdev ((gx_device_win *)dev)

/* RasterOp codes */
#define rop_write_at_1s 0xE20746L	/* BitBlt: write brush at 1's */
#define rop_write_at_0s 0xB8074AL	/* BitBlt: write brush at 0's */
#define rop_write_0_at_1s 0x220326L	/* BitBlt: ~S & D */
#define rop_write_0_at_0s 0x8800C6L	/* BitBlt: S & D */
#define rop_write_1s 0xFF0062L		/* write 1's */
#define rop_write_0s 0x000042L		/* write 0's */
#define rop_write_pattern 0xF00021L	/* PatBlt: write brush */

/* Compress a gx_color_value into an 8-bit Windows color value, */
/* using only the high order 5 bits. */
#define win_color_value(z)\
  ((((z) >> (gx_color_value_bits - 5)) << 3) +\
   ((z) >> (gx_color_value_bits - 3)))
