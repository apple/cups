/* Copyright (C) 1990, 1991, 1993 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevherc.c */
/* IBM PC-compatible Hercules Graphics display driver */
/* using direct access to frame buffer */

#define FB_RASTER 90
#define SCREEN_HEIGHT 350
#define SCREEN_ASPECT_RATIO (54.0/35.0)
#define VIDEO_MODE 0x07
#define regen 0xb0000000L

#define interrupt			/* patch ANSI incompatibility */
#include "dos_.h"
typedef union REGS registers;
#include "gx.h"
#include "gsmatrix.h"			/* for gxdevice.h */
#include "gxbitmap.h"
#include "gxdevice.h"

/* outportb is defined in dos_.h */
#define outport2(port, index, data)\
  (outportb(port, index), outportb((port)+1, data))
/* Define the nominal page height in inches. */
#ifdef A4
#  define PAGE_HEIGHT_INCHES 11.69
#else
#  define PAGE_HEIGHT_INCHES 11.0
#endif

/* Dimensions of screen */
#define screen_size_x (FB_RASTER * 8)
#define screen_size_y SCREEN_HEIGHT
/* Other display parameters */
#define raster_x FB_RASTER
#define aspect_ratio SCREEN_ASPECT_RATIO
#define graphics_video_mode VIDEO_MODE

/* Procedures */

	/* See gxdevice.h for the definitions of the procedures. */

dev_proc_open_device(herc_open);
dev_proc_close_device(herc_close);
dev_proc_fill_rectangle(herc_fill_rectangle);
dev_proc_copy_mono(herc_copy_mono);
dev_proc_copy_color(herc_copy_color);

/* The device descriptor */
private gx_device_procs herc_procs = {
	herc_open,
	gx_default_get_initial_matrix,
	gx_default_sync_output,
	gx_default_output_page,
	herc_close,
	gx_default_map_rgb_color,
	gx_default_map_color_rgb,
	herc_fill_rectangle,
	gx_default_tile_rectangle,
	herc_copy_mono,
	herc_copy_color
};

gx_device far_data gs_herc_device = {
	std_device_std_body(gx_device, &herc_procs, "herc",
	  screen_size_x, screen_size_y,
	/* The following parameters map an appropriate fraction of */
	/* the screen to a full-page coordinate space. */
	/* This may or may not be what is desired! */
	  (screen_size_y * aspect_ratio) / PAGE_HEIGHT_INCHES,	/* x dpi */
	  screen_size_y / PAGE_HEIGHT_INCHES		/* y dpi */
	)
};


/* Forward declarations */
private int herc_get_mode(P0());
private void herc_set_mode(P1(int));

/* Save the HERC mode */
private int herc_save_mode = -1;

/* Reinitialize the herc for text mode */
int
herc_close(gx_device *dev)
{	if ( herc_save_mode >= 0 ) herc_set_mode(herc_save_mode);
	return 0;
}

/* ------ Internal routines ------ */

/* Read the device mode */
private int
herc_get_mode(void)
{	registers regs;
	regs.h.ah = 0xf;
	int86(0x10, &regs, &regs);
	return regs.h.al;
}

/* Set the device mode */
private void
herc_set_mode(int mode)
{	registers regs;
	regs.h.ah = 0;
	regs.h.al = mode;
	int86(0x10, &regs, &regs);
}


/****************************************************************/
/* Hercules graphics card functions				*/
/*								*/
/* -- Taken from Jan/Feb 1988 issue of Micro Cornucopia #39	*/
/*								*/
/* --rewritten for MSC 5.1 on 02/18/91 by Phillip Conrad	*/
/****************************************************************/


static const char paramg[12] = {0x35, 0x2d, 0x2e, 0x07, 0x5b, 0x02,
			      0x57, 0x57, 0x02, 0x03, 0x00, 0x00};
/* (Never used)
static const char paramt[12] = {0x61, 0x50, 0x52, 0x0f, 0x19, 0x06,
			      0x19, 0x19, 0x02, 0x0d, 0x0b, 0x0c};
*/

/* Type and macro for frame buffer pointers. */
/*** Intimately tied to the 80x86 (x<2) addressing architecture. ***/
typedef byte far *fb_ptr; 
#  define mk_fb_ptr(x, y)\
    (fb_ptr)((regen) + ((0x2000 * ((y) % 4) + (90 * ((y) >> 2))) + ((int)(x) >> 3)))


/* Structure for operation parameters. */
/* Note that this structure is known to assembly code. */
/* Not all parameters are used for every operation. */
typedef struct rop_params_s {
	fb_ptr dest;			/* pointer to frame buffer */
	int draster;			/* raster of frame buffer */
	const byte far *src;		/* pointer to source data */
	int sraster;			/* source raster */
	int width;			/* width in bytes */
	int height;			/* height in scan lines */
	int shift;			/* amount to right shift source */
	int invert;			/* 0 or -1 to invert source */
	int data;			/* data for fill */
	int x_pos;		/*>>added--2/24/91 */
	int y_pos;
} rop_params;

/* Define the device port and register numbers, and the regen map base */
#define seq_addr 0x3b4		/* changed for HERC card (6845 ports)*/
#define graph_mode 0x3b8
#define graph_stat 0x3ba
#define graph_config 0x3bf

#ifndef regen
#define regen 0xa0000000L
#endif


/* Initialize the display for Hercules graphics mode */
int
herc_open(gx_device *dev)
{	int i;
	if ( herc_save_mode < 0 ) herc_save_mode = herc_get_mode();
/*	herc_set_mode(graphics_video_mode);  */
	outportb(graph_config,3);
	for(i=0;i<sizeof(paramg);i++)
	{
		outport2(seq_addr,i,paramg[i]); 
		
	}
	outportb(graph_mode,0x0a);	/* set page 0 */
	for(i=0;i<0x3FFFL;i++)	/* clear the screen */
		{
		int far *loc = (int far *)( regen  +(2L*i));
		*loc = 0;
		}

	return 0;
}

/* Macro for testing bit-inclusion */
#define bit_included_in(x,y) !((x)&~(y))

/* Copy a monochrome bitmap.  The colors are given explicitly. */
/* Color = gx_no_color_index means transparent (no effect on the image). */
int
herc_copy_mono(gx_device *dev,
  const byte *base, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h, gx_color_index izero, gx_color_index ione)
{	rop_params params;
#define czero (int)izero
#define cone (int)ione
	int dleft, sleft, count;
	int invert, zmask, omask;
	byte mask, rmask;

	if ( cone == czero )		/* vacuous case */
		return herc_fill_rectangle(dev, x, y, w, h, izero);

	/* clip */
	fit_copy(dev, base, sourcex, raster, id, x, y, w, h);
	params.dest = mk_fb_ptr(x, y);
	params.draster = raster_x;
	params.src = base + (sourcex >> 3);
	params.sraster = raster;
	params.height = h;
	params.shift = (x - sourcex) & 7;
	params.y_pos = y;
	params.x_pos = x;
	params.width = w;

	if(czero > cone) params.invert = -1;

	/* Macros for writing partial bytes. */
	/* bits has already been inverted by xor'ing with invert. */

#define write_byte_masked(ptr, bits, mask)\
  *ptr = ((bits | ~mask | zmask) & (*ptr | (bits & mask & omask)))

#define write_byte(ptr, bits)\
  *ptr = ((bits | zmask) & (*ptr | (bits & omask)))

	invert = (czero == 1 || cone == 0 ? -1 : 0); 
/*	invert = (czero == 1 || cone == 1 ? -1 : 0); */
	zmask = (czero == 0 || cone == 0 ? 0 : -1);
	omask = (czero == 1 || cone == 1 ? -1 : 0);

#undef czero
#undef cone

	/* Actually copy the bits. */

	sleft = 8 - (sourcex & 7);
	dleft = 8 - (x & 7);
	mask = 0xff >> (8 - dleft);
	count = w;
	if ( w < dleft )
		mask -= mask >> w,
		rmask = 0;
	else
		rmask = 0xff00 >> ((w - dleft) & 7);

	if (sleft == dleft)		/* optimize the aligned case */
	{
		w -= dleft;
		while ( --h >= 0 )
		{
			register const byte *bptr = params.src;
			register byte *optr = mk_fb_ptr(params.x_pos,params.y_pos);
			register int bits = *bptr ^ invert;	/* first partial byte */

			count = w;
						
			write_byte_masked(optr, bits, mask);
			
			/* Do full bytes. */

			while ((count -= 8) >= 0)
			{
				bits = *++bptr ^ invert;
				params.x_pos += 8;
				optr = mk_fb_ptr(params.x_pos,params.y_pos);
				write_byte(optr, bits);
			}
			/* Do last byte */
			
			if (count > -8)
			{
				bits = *++bptr ^ invert;
				params.x_pos += 8;
				optr = mk_fb_ptr(params.x_pos,params.y_pos);
				write_byte_masked(optr, bits, rmask);
			}
/*			dest += BPL; */
			params.y_pos++;
			params.x_pos = x;
			params.src += raster;
		}
	}
	else
	{
		int skew = (sleft - dleft) & 7;
		int cskew = 8 - skew;
		
		while (--h >= 0)
		{
			const byte *bptr = params.src;
			byte *optr = mk_fb_ptr(params.x_pos,params.y_pos);
			register int bits;

			count = w;
						
			/* Do the first partial byte */
			
			if (sleft >= dleft)
			{
				bits = *bptr >> skew;
			}	
			else /* ( sleft < dleft ) */
			{
				bits = *bptr++ << cskew;
				if (count > sleft)
					bits += *bptr >> skew;
			}
			bits ^= invert;
			write_byte_masked(optr, bits, mask);
			count -= dleft;
			params.x_pos += 8;
			optr = mk_fb_ptr(params.x_pos,params.y_pos);
			
			/* Do full bytes. */
			
			while ( count >= 8 )
			{
				bits = *bptr++ << cskew;
				bits += *bptr >> skew;
				bits ^= invert;
				write_byte(optr, bits);
				count -= 8;
				params.x_pos += 8;
				optr = mk_fb_ptr(params.x_pos,params.y_pos);
			}
			
			/* Do last byte */
			
			if (count > 0)
			{
				bits = *bptr++ << cskew;
 				if (count > skew)
					bits += *bptr >> skew;
				bits ^= invert;
				write_byte_masked(optr, bits, rmask);
			}
/*			dest += BPL;
			line += raster;
*/
			params.y_pos++;
			params.x_pos = x;
			params.src += raster;
		}
	}
	return 0;
}

/* Copy a color pixelmap.  This is just like a bitmap, */
int
herc_copy_color(gx_device *dev,
  const byte *base, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h)
{	return herc_copy_mono(dev, base, sourcex, raster, id,
		x, y, w, h,(gx_color_index)0, (gx_color_index)1);
}

#  define mk_fb_yptr(x, y)\
    (fb_ptr)((regen) + ((0x2000 * ((y) % 4) + (90 * ((y) >> 2))) + x))

/* Fill a rectangle. */
int
herc_fill_rectangle(gx_device *dev, int x, int y, int w, int h,
  gx_color_index color)
{	rop_params params;

	int x2, y2, xlen;
	byte led, red, d;
	byte far *ptr;
	int xloc;

	fit_fill(dev, x, y, w, h);

	params.dest = mk_fb_ptr(x, y);
	params.y_pos = y;
	params.x_pos = x;

	x2 = x + w - 1;
	y2 = y + h - 1;

	xlen = (x2 >> 3) - (x >> 3) - 1;
	led = 0xff >> (x & 7);
	red = 0xff << (7 - (x2 & 7));

	ptr =  mk_fb_ptr(x,y);

	if (color)
	{
		/* here to set pixels */
		
		if (xlen == -1)
		{
			/* special for rectangles that fit in a byte */
			
			d = led & red;
			for(; h >= 0; h--, ptr = mk_fb_ptr(x,params.y_pos))
			{
				*ptr |= d;
				params.y_pos++;
			}
			return 0;
		}
		
		/* normal fill */
		
		xloc = params.x_pos >> 3;
		for(; h >= 0; h--, ptr = mk_fb_ptr(x,params.y_pos))
		{	register int x_count = xlen;
			register byte far *p = ptr;
			*p |= led;
/*			 params.x_pos += 8; */
			xloc++;
			 p = mk_fb_yptr(xloc,params.y_pos);
			while ( x_count-- ) {
				 *p = 0xff;
/*				 params.x_pos += 8; */
				xloc++;
				 p = mk_fb_yptr(xloc,params.y_pos);
				}
			*p |= red;
/*			params.x_pos = x; */
			xloc = params.x_pos >> 3;
			params.y_pos++;
		}
	}

	/* here to clear pixels */

	led = ~led;
	red = ~red;

	if (xlen == -1)
	{
		/* special for rectangles that fit in a byte */
		
		d = led | red;
		for(; h >= 0; h--, ptr  = mk_fb_ptr(x,params.y_pos))
			{
			*ptr &= d;
			params.y_pos++;
			}
		return 0;
	}

	/* normal fill */
		
	xloc = x >> 3;
	for(; h >= 0; h--, ptr = mk_fb_ptr(x,params.y_pos))
	{	register int x_count = xlen;
		register byte far *p = ptr;
		*p &= led;
/*		 params.x_pos += 8; */
		xloc++;
		 p = mk_fb_yptr(xloc,params.y_pos);
		while ( x_count-- ) {
			 *p = 0x00;
/*			 params.x_pos += 8; */
			xloc++;
			 p = mk_fb_yptr(xloc,params.y_pos);
			}
		*p &= red;
/*		params.x_pos = x; */
		xloc = params.x_pos >> 3;
		params.y_pos++;
	}
	return 0;
}
