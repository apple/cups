/* Copyright (C) 1992, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevs3ga.c */
/* S3 86C911 driver */
#include "gx.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gdevpcfb.h"
#include "gdevsvga.h"

/* Shared routines from gdevsvga.c */
extern int vesa_get_mode(P0());
extern void vesa_set_mode(P1(int));

/* Macro for casting gx_device argument */
#define fb_dev ((gx_device_svga *)dev)

/* ------ The S3 86C911 device ------ */

private dev_proc_open_device(s3_open);
private dev_proc_fill_rectangle(s3_fill_rectangle);
private dev_proc_copy_mono(s3_copy_mono);
private gx_device_procs s3_procs = {
	s3_open,
	NULL,			/* get_initial_matrix */
	NULL,			/* sync_output */
	NULL,			/* output_page */
	svga_close,
	svga_map_rgb_color,
	svga_map_color_rgb,
	s3_fill_rectangle,
	NULL,			/* tile_rectangle */
	s3_copy_mono,
	svga_copy_color,	/****** DOESN'T WORK ******/
	NULL,			/* draw_line */
	svga_get_bits		/****** DOESN'T WORK ******/
};
gx_device_svga far_data gs_s3vga_device =
	svga_device(s3_procs, "s3vga", vesa_get_mode, vesa_set_mode, NULL);

/* Keep track of the character bitmap cache in off-screen memory. */
#define log2_cell_width 5
#define cell_width (1 << log2_cell_width)
#define cache_x_bits (log2_cache_width_bits - log2_cell_width)
#define log2_cell_height 5
#define cell_height (1 << log2_cell_height)
#define cache_y_bits (log2_cache_height - log2_cell_height)
#define log2_cache_width_bits 10
#define log2_cache_width_bytes (log2_cache_width_bits - 3)
#define log2_cache_height 8
#define log2_cache_capacity (cache_x_bits + cache_y_bits)
#define cache_capacity (1 << log2_cache_capacity)
private gx_bitmap_id cache_ids[cache_capacity];

/* Define additional registers and I/O addresses. */
#define crtc_addr 0x3d4		/* (color) */
#define crt_lock 0x35
#define crt_s3_lock1 0x38
#define crt_s3_lock2 0x39
#define s3_y_pos 0x82e8
#define s3_x_pos 0x86e8
#define s3_y_dest 0x8ae8
#define s3_x_dest 0x8ee8
#define s3_width 0x96e8
#define s3_status 0x9ae8	/* read only */
#define s3_command 0x9ae8	/* write only */
#define s3_back_color 0xa2e8
#define s3_fore_color 0xa6e8
#define s3_write_mask 0xaae8
#define s3_read_mask 0xaee8
#define s3_back_mix 0xb6e8
#define s3_fore_mix 0xbae8
#define s3_height 0xbee8
#define s3_mf_control 0xbee8
#  define mf_data_ones 0xa000
#  define mf_data_cpu 0xa080
#  define mf_data_display 0xa0c0
#define s3_pixel_data 0xe2e8
/* Wait for the command FIFO to empty. */
#define s3_wait_fifo()\
  while ( inport(s3_status) & 0xff )
/* Load the parameters for a rectangle operation. */
#define out_s3_rect(x, y, w, h)\
  (outport(s3_x_pos, x), outport(s3_y_pos, y),\
   outport(s3_width, (w) - 1), outport(s3_height, (h) - 1))

private int
s3_open(gx_device *dev)
{	static const mode_info mode_table[] = {
	   {	 640,  480, 0x201	},
	   {	 800,  600, 0x203	},
	   {	1024,  768, 0x205	},
	   {	-1, -1, -1	}
	};
	int code = svga_find_mode(dev, mode_table);
	if ( code < 0 ) return_error(gs_error_rangecheck);
	/* The enhanced modes all use a 1024-pixel raster. */
	fb_dev->raster = 1024;
	code = svga_open(dev);
	if ( code < 0 ) return code;
	/* Clear the cache */
	{	int i;
		for ( i = 0; i < cache_capacity; i++ )
			cache_ids[i] = gx_no_bitmap_id;
	}
	return 0;
}

/* Fill a rectangle. */
int
s3_fill_rectangle(gx_device *dev, int x, int y, int w, int h,
  gx_color_index color)
{	fit_fill(dev, x, y, w, h);
	s3_wait_fifo();
	outport(s3_fore_mix, 0x27);
	outport(s3_fore_color, (int)color);
	outport(s3_mf_control, mf_data_ones);
	out_s3_rect(x, y, w, h);
	outport(s3_command, 0x40b3);
	return 0;
}

/* Copy a monochrome bitmap.  The colors are given explicitly. */
/* Color = gx_no_color_index means transparent (no effect on the image). */
private int
s3_copy_mono(gx_device *dev,
  const byte *base, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h, gx_color_index czero, gx_color_index cone)
{	int sbit;
	const byte *sptr;
	int run;
	byte lmask;
	byte lmerge = 0;
	int cache_index, cache_x, cache_y;
	int i, j;
	fit_copy(dev, base, sourcex, raster, id, x, y, w, h);
	sbit = sourcex & 7;
	sptr = base + (sourcex >> 3);
	run = (sbit + w + 7) >> 3;
	lmask = 0xff >> sbit;
	/* See whether the cache is applicable. */
	if ( id != gx_no_bitmap_id && w <= cell_width - 7 &&
	     h <= cell_height
	   )
	{	cache_index = (int)(id & (cache_capacity - 1));
		cache_x = ((cache_index & ((1 << cache_x_bits) - 1)) <<
			log2_cell_width) + 7;
		cache_y = ((cache_index >> cache_x_bits) <<
			log2_cell_height) + 768;
		if ( cache_ids[cache_index] != id )
		{	cache_ids[cache_index] = id;
			/* Copy the bitmap to the cache. */
			s3_wait_fifo();
			out_s3_rect(cache_x - sbit, cache_y, w + sbit, h);
			outport(s3_fore_mix, 0x22);	/* 1s */
			outport(s3_back_mix, 0x01);	/* 0s */
			outport(s3_mf_control, mf_data_cpu);
			outport(s3_command, 0x41b3);
			{	const int skip = raster - run;
				for ( i = h; i > 0; i--, sptr += skip )
				  for ( j = run; j > 0; j--, sptr++ )
					outportb(s3_pixel_data, *sptr);
			}
		}
		s3_wait_fifo();
	}
	else
	{	cache_index = -1;
		if ( lmask != 0xff )
		{	/* The hardware won't do the masking for us. */
			if ( czero != gx_no_color_index )
			{	if ( cone != gx_no_color_index )
				{	s3_fill_rectangle(dev, x, y, w, h, czero);
					czero = gx_no_color_index;
				}
				else
				{	lmerge = ~lmask;
				}
			}
		}
		s3_wait_fifo();
		out_s3_rect(x - sbit, y, w + sbit, h);
	}
	/* Load the colors for the real transfer. */
	if ( cone != gx_no_color_index )
	{	outport(s3_fore_mix, 0x27);
		outport(s3_fore_color, (int)cone);
	}
	else
		outport(s3_fore_mix, 0x63);
	if ( czero != gx_no_color_index )
	{	outport(s3_back_mix, 0x07);
		outport(s3_back_color, (int)czero);
	}
	else
		outport(s3_back_mix, 0x63);
	s3_wait_fifo();
	if ( cache_index < 0 )		/* direct transfer */
	{	outport(s3_mf_control, mf_data_cpu);
		outport(s3_command, 0x41b3);
		if ( run == 1 && !lmerge )	/* special case for chars */
		{	for ( i = h; i > 0; i--, sptr += raster )
				outportb(s3_pixel_data, *sptr & lmask);
		}
		else
		{	const int skip = raster - run;
			for ( i = h; i > 0; i--, sptr += skip )
			{	outportb(s3_pixel_data, (*sptr++ & lmask) | lmerge);
				for ( j = run; j > 1; j--, sptr++ )
					outportb(s3_pixel_data, *sptr);
			}
		}
	}
	else
	{	/* Copy the character from the cache to the screen. */
		out_s3_rect(cache_x, cache_y, w, h);
		outport(s3_x_dest, x);
		outport(s3_y_dest, y);
		outport(s3_mf_control, mf_data_display);
		outport(s3_command, 0xc0b3);
	}
	return 0;
}
