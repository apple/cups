/* Copyright (C) 1992, 1993, 1994, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/*
 * gdevvglb.c
 *
 * This is a driver for 386 PCs using VGALIB for graphics on the console
 * display.
 *
 * Written by Sigfrid Lundberg, siglun@euler.teorekol.lu.se.
 * Modified by Erik Talvola, talvola@gnu.ai.mit.edu
 */

#include "gx.h"
#include "gxdevice.h"
#include "gserrors.h"

#include <errno.h>
#include <vga.h>

typedef struct gx_device_vgalib {
    gx_device_common;
} gx_device_vgalib;

#define vgalibdev ((gx_device_vgalib *)dev)

#define XDPI   60      /* to get a more-or-less square aspect ratio */
#define YDPI   60

#ifndef A4 /*Letter size*/
#define YSIZE (20.0 * YDPI / 2.5)
#define XSIZE (8.5 / 11)*YSIZE /* 8.5 x 11 inch page, by default */
#else				       /* A4 paper */
#define XSIZE 8.27
#define YSIZE 11.69
#endif

private dev_proc_open_device(vgalib_open);
private dev_proc_close_device(vgalib_close);
private dev_proc_draw_line(vgalib_draw_line);
private dev_proc_fill_rectangle(vgalib_fill_rectangle);
private dev_proc_tile_rectangle(vgalib_tile_rectangle);
private dev_proc_map_color_rgb(vgalib_map_color_rgb);
private dev_proc_map_rgb_color(vgalib_map_rgb_color);
private dev_proc_copy_mono(vgalib_copy_mono);
private dev_proc_copy_color(vgalib_copy_color);



private gx_device_procs vgalib_procs = {
    vgalib_open,
    gx_default_get_initial_matrix,
    gx_default_sync_output,
    gx_default_output_page,
    vgalib_close,
    vgalib_map_rgb_color,
    vgalib_map_color_rgb,
    vgalib_fill_rectangle,
    vgalib_tile_rectangle,
    vgalib_copy_mono,
    vgalib_copy_color,
    vgalib_draw_line
};

gx_device_vgalib far_data gs_vgalib_device = {
	std_device_std_body(gx_device_vgalib, &vgalib_procs, "vgalib",
	  0, 0, 1, 1)
};

int vgalib_open(gx_device *dev)
{
    int VGAMODE;
    int width = dev->width, height = dev->height;

    VGAMODE = vga_getdefaultmode();
    if (VGAMODE == -1) {
      vga_setmode(G640x480x16);
    } else {
      vga_setmode(VGAMODE);
    }
    vga_clear();
    if ( width == 0 )
      width = vga_getxdim() + 1;
    if ( height == 0 )
      height = vga_getydim() + 1;

  /*vgalib provides no facilities for finding out aspect ratios*/
    if ( dev->y_pixels_per_inch == 1 )
    {
	dev->y_pixels_per_inch = height / 11.0;
	dev->x_pixels_per_inch = dev->y_pixels_per_inch;
    }
   gx_device_set_width_height(dev, width, height);

				    /* Find out if the device supports color */
				  /* (default initialization is monochrome). */
			    /* We only recognize 16-color devices right now. */
    if ( vga_getcolors() > 1 )
    {

	int index,one,rgb[3];

	static gx_device_color_info vgalib_16color = dci_color(4, 2, 3);
	dev->color_info = vgalib_16color;

	for(index=0;index<gx_max_color_value;index++)
	{
	    one = (index & 8 ? gx_max_color_value : gx_max_color_value / 3);
	    rgb[0] = (index & 4 ? one : 0);
	    rgb[1] = (index & 2 ? one : 0);
	    rgb[2] = (index & 1 ? one : 0);
	    vga_setpalette((int)index, rgb[0], rgb[1], rgb[2]);
	}
    }

  return 0;
}

int vgalib_close(gx_device *dev)
{
    vga_getch();
    vga_setmode(TEXT);
    return 0;
}

gx_color_index vgalib_map_rgb_color(gx_device *dev, gx_color_value red,
				   gx_color_value green, gx_color_value blue)
{
    int index;

    index=((red > gx_max_color_value / 4 ? 4 : 0) +
	   (green > gx_max_color_value / 4 ? 2 : 0) +
	   (blue > gx_max_color_value / 4 ? 1 : 0) +
	   (red > gx_max_color_value / 4 * 3 ||
	    green > gx_max_color_value / 4 * 3 ? 8 : 0));

    return (gx_color_index)index;
}


/* I actually don't understand what I'm doing -- the only thing I want to
achieve are palettes that emulates BGI */
int vgalib_map_color_rgb(gx_device *dev, gx_color_index index,
			unsigned short rgb[3])
{
    gx_color_value one =
	(index & 8 ? gx_max_color_value : gx_max_color_value / 3);
    rgb[0] = (index & 4 ? one : 0);
    rgb[1] = (index & 2 ? one : 0);
    rgb[2] = (index & 1 ? one : 0);

    return 0;
}

int vgalib_draw_line(gx_device *dev, int x0, int y0, int x1, int y1,
		    gx_color_index color)
{
    if(!((x0==x1)&&(y0==y1)))
    {
	vga_setcolor((int)color);
	vga_drawline(x0,y0,x1,y1);
    }
    return 0;
}

int vgalib_tile_rectangle(gx_device *dev, const gx_tile_bitmap *tile,
			 int x, int y, int w, int h, gx_color_index czero,
			 gx_color_index cone, int px, int py)
{
    if ( czero != gx_no_color_index && cone != gx_no_color_index )
    {
	vgalib_fill_rectangle(dev, x, y, w, h, czero);
	czero = gx_no_color_index;
    }
    return gx_default_tile_rectangle(dev, tile, x, y, w, h, czero, cone, px,
py);
}

int vgalib_fill_rectangle(gx_device *dev, int x, int y, int w, int h,
			 gx_color_index color)
{
    int i,j;

    fit_fill(dev, x, y, w, h);
    vga_setcolor((int)color);
    for(i=y;i<y+h;i++)
	for(j=x;j<x+w;j++)
	    vga_drawpixel(j,i);
    return 0;
}

int vgalib_copy_mono(gx_device *dev, const byte *base, int sourcex,
		    int raster, gx_bitmap_id id, int x, int y, int width,
		    int height, gx_color_index zero, gx_color_index one)

{
    const byte *ptr_line = base + (sourcex >> 3);
    int left_bit = 0x80 >> (sourcex & 7);
    int dest_y = y, end_x = x + width;
    int invert = 0;
    int color;

    fit_copy(dev, base, sourcex, raster, id, x, y, width, height);

    if ( zero == gx_no_color_index )
    {
	if ( one == gx_no_color_index )
	    return 0;
    color = (int)one;
    }
    else
    {
	if ( one == gx_no_color_index )
	{
	    color = (int)zero;
	    invert = -1;
	}
	else
	{			       /* Pre-clear the rectangle to zero */
	    vga_setcolor(zero);
	    vgalib_fill_rectangle(dev,x,y,width,height,zero);
	    color = (int)one;
	}
    }

    vga_setcolor(color);

    while( height-- )
    {				       /* for each line */
	const byte *ptr_source = ptr_line;
	register int dest_x = x;
	register int bit = left_bit;

	while ( dest_x < end_x )
	{			       /* for each bit in the line */
	    if ( (*ptr_source ^ invert)  & bit )
	    vga_drawpixel(dest_x,dest_y);
	    dest_x++;
	    if ( (bit >>= 1) == 0 )
		bit = 0x80, ptr_source++;
	}

	dest_y++;
	ptr_line += raster;
    }
    return 0;
}


/* Copy a color pixel map.  This is just like a bitmap, except that */
/* each pixel takes 4 bits instead of 1 when device driver has color. */
int vgalib_copy_color(gx_device *dev, const byte *base, int sourcex,
		     int raster, gx_bitmap_id id, int x, int y,
		     int width, int height)
{

    fit_copy(dev, base, sourcex, raster, id, x, y, width, height);

    if ( gx_device_has_color(dev) )
    {				       /* color device, four bits per pixel */
	const byte *line = base + (sourcex >> 1);
	int dest_y = y, end_x = x + width;

	if ( width <= 0 )
	    return 0;
	while ( height-- )
	{			       /* for each line */
	    const byte *source = line;
	    register int dest_x = x;

	    if ( sourcex & 1 )
	    {			       /* odd nibble first */
		int color =  *source++ & 0xf;
		vga_setcolor(color);
		vga_drawpixel(dest_x,dest_y);
		dest_x++;
	    }
				       /* Now do full bytes */
	    while ( dest_x < end_x )
	    {
		int color = *source >> 4;
		vga_setcolor(color);
		vga_drawpixel(dest_x,dest_y);
		dest_x++;

		if ( dest_x < end_x )
		{
		    color =  *source++ & 0xf;
		    vga_setcolor(color);
		    vga_drawpixel(dest_x,dest_y);
		    dest_x++;
		}
	    }

	    dest_y++;
	    line += raster;
	}
    }
    else
    {				  /* monochrome device: one bit per pixel */
		/* bitmap is the same as bgi_copy_mono: one bit per pixel */
	vgalib_copy_mono(dev, base, sourcex, raster, id, x, y, width, height,
			(gx_color_index)0, (gx_color_index)7);
    }

    return 0;
}
