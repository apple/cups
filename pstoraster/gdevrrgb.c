/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevrrgb.c */
/* RGB device with "render algorithm" */
#include "gdevprn.h"

/*
 * This is a 32-bit device in which each pixel holds 24 bits of RGB and 8
 * (actually 4) bits of "render algorithm".  It is not useful in itself, but
 * it is a good example of (1) how to handle "render algorithm" information
 * and (2) how to implement a printer device with a non-standard memory
 * device as its underlying buffer.
 */

/* Define default device parameters. */
#ifndef X_DPI
#  define X_DPI 300
#endif
#ifndef Y_DPI
#  define Y_DPI 300
#endif

/* The device descriptor */
private dev_proc_open_device(rrgb_open);
private dev_proc_map_rgb_color(rrgb_map_rgb_color);
private dev_proc_map_color_rgb(rrgb_map_color_rgb);
private dev_proc_print_page(rrgb_print_page);
private const gx_device_procs rrgb_procs =
  prn_color_procs(rrgb_open, gdev_prn_output_page, gdev_prn_close,
		  rrgb_map_rgb_color, rrgb_map_color_rgb);

gx_device_printer far_data gs_rrgb_device =
{ prn_device_body(gx_device_printer, rrgb_procs, "rrgb",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	3,32,255,255,256,256, rrgb_print_page)
};

/* Buffer device implementation */
private dev_proc_make_buffer_device(rrgb_make_buffer_device);
private dev_proc_strip_copy_rop(rrgb_strip_copy_rop);

#define ppdev ((gx_device_printer *)pdev)

/* Open the device.  We redefine this only so we can reset */
/* make_buffer_device. */
private int
rrgb_open(gx_device *pdev)
{	ppdev->printer_procs.make_buffer_device = rrgb_make_buffer_device;
	return gdev_prn_open(pdev);
}

/* Color mapping */
private gx_color_index
rrgb_map_rgb_color(gx_device *dev,
  gx_color_value r, gx_color_value g, gx_color_value b)
{	return gx_color_value_to_byte(b) +
	  ((uint)gx_color_value_to_byte(g) << 8) +
	    ((ulong)gx_color_value_to_byte(r) << 16);
}
private int
rrgb_map_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{	prgb[0] = gx_color_value_from_byte((color >> 16) & 0xff);
	prgb[1] = gx_color_value_from_byte((color >> 8) & 0xff);
	prgb[2]	= gx_color_value_from_byte(color & 0xff);
	return 0;
}

/* Print the page.  Just copy the bits to the file. */
private int
rrgb_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	/* Just dump the bits on the file. */
	int line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
	byte *in = (byte *)gs_malloc(line_size, 1, "rrgb_print_page");
	int lnum;

	if ( in == 0 )
	  return_error(gs_error_VMerror);
	for ( lnum = 0; lnum < pdev->height; ++lnum )
	  {	byte *data;
		gdev_prn_get_bits(pdev, lnum, in, &data);
		fwrite(data, 1, line_size, prn_stream);
	  }
	gs_free((char *)in, line_size, 1, "rrgb_print_page");
	return 0;
}

/* Reimplement the buffer device so that it stores the "render algorithm" */
/* in the top byte of each pixel. */
private int
rrgb_make_buffer_device(gx_device_memory *mdev,
  gx_device *target, gs_memory_t *mem, bool for_band)
{	int code = gx_default_make_buffer_device(mdev, target, mem, for_band);

	if ( code < 0 )
	  return code;
	mdev->std_procs.strip_copy_rop = rrgb_strip_copy_rop;
	return code;
}

/* Reimplement copy_rop so it saves the "render algorithm". */
/* This is messy: we have to copy each (partial) scan line from the */
/* 32-bit representation into a 24-bit buffer, do the operation, */
/* and then write it back.  The code is modeled on the default */
/* implementation in gdevmrop.c (q.v.). */
private void
rrgb_copy_4to3(byte *dest, const byte *src, int width)
{	const byte *p = src;
	byte *q = dest;
	int n;

	for ( n = width; n > 0; p += 4, q += 3, --n )
	  q[0] = p[1], q[1] = p[2], q[2] = p[3];
}
private void
rrgb_copy_3to4(byte *dest, const byte *src, int width, byte upper)
{	const byte *p = src;
	byte *q = dest;
	int n;

	for ( n = width; n > 0; p += 3, q += 4, --n )
	  q[0] = upper, q[1] = p[0], q[2] = p[1], q[3] = p[2];
}
int
rrgb_strip_copy_rop(gx_device *dev,
  const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
  const gx_color_index *scolors,
  const gx_strip_bitmap *textures, const gx_color_index *tcolors,
  int x, int y, int width, int height,
  int phase_x, int phase_y, gs_logical_operation_t lop)
{	gs_rop3_t rop = lop & lop_rop_mask;
	const gx_device_memory *mdproto = gdev_mem_device_for_bits(24);
	gs_memory_t *mem = &gs_memory_default;
	gx_device_memory mdev;
	bool
	  uses_d = rop3_uses_D(rop),
	  copy_s = rop3_uses_S(rop) && scolors == NULL,
	  copy_t = rop3_uses_T(rop) && tcolors == NULL;
	byte *srow = 0;
	byte *trow = 0;
	const byte *srdata;
	int sx;
	gx_strip_bitmap tsubst;
	const gx_strip_bitmap *tptr;
	int tx;
	int code;
	int py;

	gs_make_mem_device(&mdev, mdproto, 0, -1, dev);
	mdev.width = width;
	mdev.height = 1;
	mdev.bitmap_memory = mem;
	code = (*dev_proc(&mdev, open_device))((gx_device *)&mdev);
	if ( code < 0 )
	  return code;
	if ( copy_s )
	  { srow = gs_alloc_bytes(mem, width * 3, "rrgb source buffer");
	    if ( srow == 0 )
	      { code = gs_note_error(gs_error_VMerror);
	        goto x;
	      }
	  }
	if ( copy_t )
	  { trow = gs_alloc_bytes(mem, textures->rep_width * 3,
				  "rrgb texture buffer");
	    if ( trow == 0 )
	      { code = gs_note_error(gs_error_VMerror);
	        goto x;
	      }
	  }
	for ( py = y; py < y + height; ++py )
	  { byte *ddata = scan_line_base((gx_device_memory *)dev, y) + x * 4;

	    if ( uses_d )
	      { rrgb_copy_4to3(scan_line_base(&mdev, 0), ddata, width);
	      }
	    if ( copy_s )
	      { rrgb_copy_4to3(srow, sdata + sourcex * 4, width);
		srdata = srow, sx = 0;
	      }
	    else
	      srdata = sdata + y * sraster, sx = sourcex;
	    if ( copy_t )
	      { tsubst = *textures;
		rrgb_copy_4to3(trow,
			       tsubst.data + ((py + phase_y) %
				 tsubst.rep_height) * tsubst.raster,
			       textures->rep_width);
		tsubst.data = trow;
		tsubst.size.x = tsubst.rep_width;
		tsubst.size.y = 1;
		tsubst.id = gx_no_bitmap_id;
		tsubst.rep_height = 1;
		tx = py / tsubst.rep_height * tsubst.rep_shift;
		tptr = &tsubst;
	      }
	    else
	      tptr = textures, tx = 0;
	    code = (*dev_proc(&mdev, strip_copy_rop))((gx_device *)&mdev,
			srdata, sx, 0 /*unused*/, gx_no_bitmap_id, scolors,
			tptr, tcolors, 0, 0, width, 1,
			phase_x + tx, phase_y + py, lop);
	    if ( code < 0 )
	      break;
	    rrgb_copy_3to4(ddata, scan_line_base(&mdev, 0), width,
			   (lop >> lop_ral_shift) & lop_ral_mask);
	  }
x:	gs_free_object(mem, trow, "rrgb texture buffer");
	gs_free_object(mem, srow, "rrgb source buffer");
	(*dev_proc(&mdev, close_device))((gx_device *)&mdev);
	return code;
}
