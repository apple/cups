/* Copyright (C) 1989, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevpcfb.c */
/* IBM PC frame buffer (EGA/VGA) drivers */
#include "memory_.h"
#include "gconfigv.h"			/* for USE_ASM */
#include "gx.h"
#include "gserrors.h"
#include "gsparam.h"
#include "gxdevice.h"
#include "gdevpccm.h"
#include "gdevpcfb.h"

/* Macro for casting gx_device argument */
#define fb_dev ((gx_device_ega *)dev)

/* Procedure record */
private dev_proc_map_rgb_color(ega0_map_rgb_color);
private dev_proc_map_rgb_color(ega1_map_rgb_color);
#define ega2_map_rgb_color pc_4bit_map_rgb_color
private dev_proc_map_color_rgb(ega01_map_color_rgb);
#define ega2_map_color_rgb pc_4bit_map_color_rgb
#if ega_bits_of_color == 0
#   define ega_map_rgb_color ega0_map_rgb_color
#   define ega_map_color_rgb ega01_map_color_rgb
#else
# if ega_bits_of_color == 1
#   define ega_map_rgb_color ega1_map_rgb_color
#   define ega_map_color_rgb ega01_map_color_rgb
# else
#   define ega_map_rgb_color ega2_map_rgb_color
#   define ega_map_color_rgb ega2_map_color_rgb
# endif
#endif
#define ega_std_procs(get_params, put_params)\
	ega_open,\
	NULL,			/* get_initial_matrix */\
	NULL,			/* sync_output */\
	NULL,			/* output_page */\
	ega_close,\
	ega_map_rgb_color,\
	ega_map_color_rgb,\
	ega_fill_rectangle,\
	ega_tile_rectangle,\
	ega_copy_mono,\
	ega_copy_color,\
	NULL,			/* draw_line */\
	ega_get_bits,\
	get_params,\
	put_params,\
	NULL,			/* map_cmyk_color */\
	NULL,			/* get_xfont_procs */\
	NULL,			/* get_xfont_device */\
	NULL,			/* map_rgb_alpha_color */\
	gx_page_device_get_page_device

private gx_device_procs ega_procs = {
	ega_std_procs(NULL, NULL)
};

private dev_proc_get_params(svga16_get_params);
private dev_proc_put_params(svga16_put_params);
private gx_device_procs svga16_procs = {
	ega_std_procs(svga16_get_params, svga16_put_params)
};

/* All the known instances */
		/* EGA */
gx_device_ega far_data gs_ega_device =
	ega_device("ega", ega_procs, 80, 350, 48.0/35.0, 0x10);
		/* VGA */
gx_device_ega far_data gs_vga_device =
	ega_device("vga", ega_procs, 80, 480, 1.0, 0x12);
		/* Generic SuperVGA, 800x600, 16-color mode */
gx_device_ega far_data gs_svga16_device =
	ega_device("svga16", svga16_procs, 100, 600, 1.0, 0x29 /*Tseng*/);

/* Save the BIOS state */
private pcfb_bios_state pcfb_save_state = { -1 };

/* Initialize the EGA for graphics mode */
int
ega_open(gx_device *dev)
{	/* Adjust the device resolution. */
	/* This is a hack, pending refactoring of the put_params machinery. */
	switch ( fb_dev->video_mode )
	{
	case 0x10:	/* EGA */
		gx_device_adjust_resolution(dev, 640, 350, 1); break;
	case 0x12:	/* VGA */
		gx_device_adjust_resolution(dev, 640, 480, 1); break;
	default:	/* 800x600 SuperVGA */
		gx_device_adjust_resolution(dev, 800, 600, 1); break;
	}
	if ( pcfb_save_state.display_mode < 0 )
		pcfb_get_state(&pcfb_save_state);
	/* Do implementation-specific initialization */
	pcfb_set_signals(dev);
	pcfb_set_mode(fb_dev->video_mode);
	set_s_map(-1);			/* enable all maps */
	return 0;
}

/* Reinitialize the EGA for text mode */
int
ega_close(gx_device *dev)
{	if ( pcfb_save_state.display_mode >= 0 )
	  pcfb_set_state(&pcfb_save_state);
	return 0;
}

/* Get/put the display mode parameter. */
int
svga16_get_params(gx_device *dev, gs_param_list *plist)
{	int code = gx_default_get_params(dev, plist);
	if ( code < 0 )
	  return code;
	return param_write_int(plist, "DisplayMode", &fb_dev->video_mode);
}
int
svga16_put_params(gx_device *dev, gs_param_list *plist)
{	int ecode = 0;
	int code;
	int imode = fb_dev->video_mode;
	const char _ds *param_name;

	switch ( code = param_read_int(plist, (param_name = "DisplayMode"), &imode) )
	  {
	  default:
		ecode = code;
		param_signal_error(plist, param_name, ecode);
	  case 0:
	  case 1:
		break;
	  }

	if ( ecode < 0 )
	  return ecode;
	code = gx_default_put_params(dev, plist);
	if ( code < 0 )
	  return code;

	if ( imode != fb_dev->video_mode )
	  {	if ( dev->is_open )
		  gs_closedevice(dev);
		fb_dev->video_mode = imode;
	  }

	return 0;
}

/* Map a r-g-b color to an EGA color code. */
#define Nb gx_color_value_bits
private gx_color_index
ega0_map_rgb_color(gx_device *dev, gx_color_value r, gx_color_value g,
  gx_color_value b)
{	return pc_4bit_map_rgb_color(dev, r, r, r);
}
private gx_color_index
ega1_map_rgb_color(gx_device *dev, gx_color_value r, gx_color_value g,
  gx_color_value b)
{
#define cvtop (gx_color_value)(1 << (Nb - 1))
	return pc_4bit_map_rgb_color(dev, r & cvtop, g & cvtop, b & cvtop);
}
#undef Nb

/* Map a color code to r-g-b. */
#define icolor (int)color
private int
ega01_map_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{
#define one (gx_max_color_value / 2 + 1)
	prgb[0] = (icolor & 4 ? one : 0);
	prgb[1] = (icolor & 2 ? one : 0);
	prgb[2] = (icolor & 1 ? one : 0);
	return 0;
#undef one
}
#undef icolor

/* ------ Internal routines ------ */

/* Structure for operation parameters. */
/* Note that this structure is known to assembly code. */
/* Not all parameters are used for every operation. */
typedef struct rop_params_s {
	fb_ptr dest;			/* pointer to frame buffer */
	int draster;			/* raster of frame buffer */
	const byte *src;		/* pointer to source data */
	int sraster;			/* source raster */
	int width;			/* width in bytes */
	int height;			/* height in scan lines */
	int shift;			/* amount to right shift source */
	int invert;			/* 0 or -1 to invert source */
	int data;			/* data for fill */
} rop_params;
typedef rop_params _ss *rop_ptr;

/* Assembly language routines */

#if USE_ASM
void memsetcol(P1(rop_ptr)); /* dest, draster, height, data */
#else
#define memsetcol cmemsetcol
private void
cmemsetcol(rop_ptr rop)
{	byte *addr = rop->dest;
	int yc = rop->height;
	byte data = rop->data;
	int draster = rop->draster;
	while ( yc-- )
	 { byte_discard(*addr);
	   *addr = data;
	   addr += draster;
	 }
}
#endif

#if USE_ASM
void memsetrect(P1(rop_ptr)); /* dest, draster, width, height, data */
#else
#define memsetrect cmemsetrect
private void
cmemsetrect(rop_ptr rop)
{	int yc = rop->height;
	int width = rop->width;
	if ( yc <= 0 || width <= 0 ) return;
	   {	byte *addr = rop->dest;
		byte data = rop->data;
		if ( width > 5 )	/* use memset */
		   {	int skip = rop->draster;
			do
			   {	memset(addr, data, width);
				addr += skip;
			   }
			while ( --yc );
		   }
		else			/* avoid the fixed overhead */
		   {	int skip = rop->draster - width;
			do
			   {	int cnt = width;
				do { *addr++ = data; } while ( --cnt );
				addr += skip;
			   }
			while ( --yc );
		   }
	   }
}
#endif

#if USE_ASM
void memrwcol(P1(rop_ptr)); /* dest, draster, src, sraster, height, shift, invert */
#  define memrwcol0(rop) memrwcol(rop)	/* same except shift = 0 */
#else
#  define memrwcol cmemrwcol
#  define memrwcol0 cmemrwcol0
private void
cmemrwcol(rop_ptr rop)
{	byte *dp = rop->dest;
	const byte *sp = rop->src;
	int yc = rop->height;
	int shift = rop->shift;
	byte invert = rop->invert;
	int sraster = rop->sraster, draster = rop->draster;
	while ( yc-- )
	 { byte_discard(*dp);
	   *dp = ((*sp >> shift) + (*sp << (8 - shift))) ^ invert;
	   dp += draster, sp += sraster;
	 }
}
private void
cmemrwcol0(rop_ptr rop)
{	byte *dp = rop->dest;
	const byte *sp = rop->src;
	int yc = rop->height;
	byte invert = rop->invert;
	int sraster = rop->sraster, draster = rop->draster;
	if ( yc > 0 ) do
	 { byte_discard(*dp);
	   *dp = *sp ^ invert;
	   dp += draster, sp += sraster;
	 }
	while ( --yc );
}
#endif

#if USE_ASM
void memrwcol2(P1(rop_ptr)); /* dest, draster, src, sraster, height, shift, invert */
#else
#define memrwcol2 cmemrwcol2
private void
cmemrwcol2(rop_ptr rop)
{	byte *dp = rop->dest;
	const byte *sp = rop->src;
	int yc = rop->height;
	int shift = rop->shift;
	byte invert = rop->invert;
	int sraster = rop->sraster, draster = rop->draster;
	while ( yc-- )
	 { byte_discard(*dp);
	   *dp = ((sp[1] >> shift) + (*sp << (8 - shift))) ^ invert;
	   dp += draster, sp += sraster;
	 }
}
#endif

/* Forward definitions */
int ega_write_dot(P4(gx_device *, int, int, gx_color_index));
private void near fill_rectangle(P4(rop_ptr, int, int, int));
private void near fill_row_only(P4(byte *, int, int, int));

/* Clean up after writing */
#define dot_end()\
  set_g_mask(0xff)			/* all bits on */

/* Write a dot using the EGA color codes. */
/* This doesn't have to be efficient. */
int
ega_write_dot(gx_device *dev, int x, int y, gx_color_index color)
{	byte data[4];
	data[0] = (byte)color;
	return ega_copy_color(dev, data, 1, 4, gx_no_bitmap_id, x, y, 1, 1);
}

/* Macro for testing bit-inclusion */
#define bit_included_in(x,y) !((x)&~(y))

/* Copy a monochrome bitmap.  The colors are given explicitly. */
/* Color = gx_no_color_index means transparent (no effect on the image). */
int
ega_copy_mono(gx_device *dev,
  const byte *base, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h, gx_color_index izero, gx_color_index ione)
{	rop_params params;
#define czero (int)izero
#define cone (int)ione
	int dleft, count;
	byte mask, rmask;
	fb_ptr save_dest;
	int other_color = -1;
	fit_copy(dev, base, sourcex, raster, id, x, y, w, h);
	params.dest = mk_fb_ptr(x, y);
	params.draster = fb_dev->raster;
	params.src = base + (sourcex >> 3);
	params.sraster = raster;
	params.height = h;
	params.shift = (x - sourcex) & 7;
	/* Analyze the 16 possible cases: each of izero and ione may be */
	/* 0, 0xf, transparent, or some other color. */
	switch ( czero )
	   {
	case no_color:
		switch ( cone )
		   {
		default:		/* (T, other) */
			/* Must do 2 passes */
			other_color = cone;
			save_dest = params.dest;
			/* falls through */
		case 0:			/* (T, 0) */
			set_g_function(gf_AND);
			params.invert = -1;
			break;
		case 0xf:		/* (T, 0xf) */
			set_g_function(gf_OR);
			params.invert = 0;
			break;
		case no_color:		/* (T, T) */
			return 0;	/* nothing to do */
		   }
		break;
	case 0:
		params.invert = 0;
		switch ( cone )
		   {
		default:		/* (0, other) */
			set_g_const(0);
			set_g_const_map(cone ^ 0xf);
			/* falls through */
		case 0xf:		/* (0, 0xf) */
			break;
		case no_color:		/* (0, T) */
			set_g_function(gf_AND);
			break;
		   }
		break;
	case 0xf:
		params.invert = -1;
		switch ( cone )
		   {
		case 0:			/* (0xf, 0) */
			break;
		default:		/* (0xf, other) */
			set_g_const(0xf);
			set_g_const_map(cone);
			break;
		case no_color:		/* (0xf, T) */
			set_g_function(gf_OR);
			/* falls through */
		   }
		break;
	default:
		switch ( cone )
		   {
		default:		/* (other, not T) */
			if ( bit_included_in(czero, cone) )
			   {	set_g_const(czero);
				set_g_const_map(czero ^ cone ^ 0xf);
				params.invert = 0;
				break;
			   }
			else if ( bit_included_in(cone, czero) )
			   {	set_g_const(cone);
				set_g_const_map(cone ^ czero ^ 0xf);
				params.invert = -1;
				break;
			   }
			/* No way around it, fill with one color first. */
			save_dest = params.dest;
			fill_rectangle((rop_ptr)&params, x & 7, w, cone);
			params.dest = save_dest;
			set_g_function(gf_XOR);
			set_s_map(czero ^ cone);
			other_color = -2;	/* must reset s_map at end */
			params.invert = -1;
			break;
		case no_color:		/* (other, T) */
			/* Must do 2 passes */
			other_color = czero;
			save_dest = params.dest;
			set_g_function(gf_AND);
			params.invert = 0;
			break;
		   }
		break;
	   }
	/* Actually copy the bits. */
	dleft = 8 - (x & 7);
	mask = 0xff >> (8 - dleft);
	count = w - dleft;
	if ( count < 0 )
		mask -= mask >> w,
		rmask = 0;
	else
		rmask = 0xff00 >> (count & 7);
	/* params: dest, src, sraster, height, shift, invert */
	/* Smashes params.src, params.dest, count. */
copy:	set_g_mask(mask);
	if ( params.shift == 0 )	/* optimize the aligned case */
	   {	/* Do left column */
		memrwcol0((rop_ptr)&params);
		/* Do center */
		if ( (count -= 8) >= 0 )
		   {	out_g_mask(0xff);
			do
			   {	params.src++, params.dest++;
				memrwcol0((rop_ptr)&params);
			   }
			while ( (count -= 8) >= 0 );
		   }
		/* Do right column */
		if ( rmask )
		   {	params.src++, params.dest++;
			out_g_mask(rmask);
			memrwcol0((rop_ptr)&params);
		   }
	   }
	else
	   {	/* Do left column */
		int sleft = 8 - (sourcex & 7);
		if ( sleft >= dleft )
		   {	/* Source fits in one byte */
			memrwcol((rop_ptr)&params);
		   }
		else if ( w <= sleft )
		   {	/* Source fits in one byte, thin case */
			memrwcol((rop_ptr)&params);
			goto fin;
		   }
		else
		   {	memrwcol2((rop_ptr)&params);
			params.src++;
		   }
		/* Do center */
		if ( (count -= 8) >= 0 )
		   {	out_g_mask(0xff);
			do
			   {	params.dest++;
				memrwcol2((rop_ptr)&params);
				params.src++;
			   }
			while ( (count -= 8) >= 0 );
		   }
		/* Do right column */
		if ( rmask )
		   {	out_g_mask(rmask);
			params.dest++;
			if ( count + 8 <= params.shift )
				memrwcol((rop_ptr)&params);
			else
				memrwcol2((rop_ptr)&params);
		   }
	   }
fin:	if ( other_color != -1 )
	   {	if ( other_color >= 0 )
		   {	/* Do the second pass on (T, other) or (other, T). */
			count = w - dleft;
			params.src = base + (sourcex >> 3);
			params.dest = save_dest;
			params.invert ^= -1;
			set_s_map(other_color);
			set_g_function(gf_OR);
			other_color = -2;
			goto copy;
		   }
		else
		   {	/* Finished second pass, restore s_map */
			set_s_map(-1);
		   }
	   }
	set_g_function(gf_WRITE);
	set_g_const_map(0);
	dot_end();
	return 0;
#undef czero
#undef cone
}

/* Copy a color pixelmap.  This is just like a bitmap, */
/* except that each pixel takes 4 bits instead of 1. */
int
ega_copy_color(gx_device *dev,
  const byte *base, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h)
{	const byte *line = base + (sourcex >> 1);
	unsigned mask = 0x80 >> (x & 7);
	int px = sourcex & 1;
	fb_ptr fb_line;
	int fb_raster = fb_dev->raster;
	fit_copy(dev, base, sourcex, raster, id, x, y, w, h);
	fb_line = mk_fb_ptr(x, y);
	set_g_mode(gm_FILL);
	select_g_mask();
	for ( ; ; px++ )
	{	const byte *bptr = line;
		fb_ptr fbptr = fb_line;
		int py = h;
		out_g_mask(mask);
		if ( px & 1 )
		{	do
			   {	byte_discard(*fbptr);	/* latch frame buffer data */
				*fbptr = *bptr;
				bptr += raster;
				fbptr += fb_raster;
			   }
			while ( --py );
			line++;
		}
		else
		{	do
			   {	byte_discard(*fbptr);	/* latch frame buffer data */
				*fbptr = *bptr >> 4;
				bptr += raster;
				fbptr += fb_raster;
			   }
			while ( --py );
		}
		if ( !--w )
			break;
		if ( (mask >>= 1) == 0 )
			mask = 0x80, fb_line++;
	}
	set_g_mode(gm_DATA);
	dot_end();
	return 0;
}

/* Fill a rectangle. */
int
ega_fill_rectangle(gx_device *dev, int x, int y, int w, int h,
  gx_color_index color)
{	rop_params params;
	fit_fill(dev, x, y, w, h);
	params.dest = mk_fb_ptr(x, y);
	if ( h == 1 )
		fill_row_only(params.dest, x & 7, w, (int)color);
	else
	   {	params.draster = fb_dev->raster;
		params.height = h;
		fill_rectangle((rop_ptr)&params, x & 7, w, (int)color);
		dot_end();
	   }
	return 0;
}

/* Tile a rectangle.  Note that the two colors must both be supplied, */
/* i.e. neither one can be gx_no_color_index (transparent): */
/* a transparent color means that the tile is colored, not a mask. */
int
ega_tile_rectangle(gx_device *dev, const gx_tile_bitmap *tile,
  int x, int y, int w, int h, gx_color_index czero, gx_color_index cone,
  int px, int py)
#define zero (int)czero
#define one (int)cone
{	rop_params params;
	int xmod, width_bytes;
	int tile_height = tile->size.y;
	int xbit;
	int lcount;
	int mask, rmask;
	byte narrow;
	byte again;
	int const_bits, maps;
	int ymod, yleft;
	fit_fill(dev, x, y, w, h);
	/* We only handle the easiest cases directly. */
	if ( (tile->size.x & 7) || one == -1 || zero == -1 || px || py )
		return gx_default_tile_rectangle(dev, tile, x, y, w, h,
			czero, cone, px, py);
	/* Following is similar to aligned case of copy_mono */	
	params.dest = mk_fb_ptr(x, y);
	params.draster = fb_dev->raster;
	params.sraster = tile->raster;
	params.shift = 0;
	xbit = x & 7;
	/* Set up the graphics registers */
	const_bits = (zero ^ one) ^ 0xf;
	if ( const_bits )
	   {	set_g_const(zero);	/* either color will do */
		set_g_const_map(const_bits);
	   }
	if ( (maps = zero & ~one) != 0 )
	   {	set_s_map(maps += const_bits);
		params.invert = -1;
		again = one & ~zero;
	   }
	else
	   {	maps = one & ~zero;
		set_s_map(maps += const_bits);
		params.invert = 0;
		again = 0;
	   }
	xmod = (x % tile->size.x) >> 3;
	width_bytes = tile->size.x >> 3;
	mask = 0xff >> xbit;
	if ( w + xbit <= 8 )
		mask -= mask >> w,
		rmask = 0,
		narrow = 1;
	else
	   {	rmask = (0xff00 >> ((w + x) & 7)) & 0xff;
		if ( xbit )	w += xbit - 8;
		else		mask = 0, --xmod, --params.dest;
		narrow = 0;
	   }
	ymod = y % tile_height;
tile:	yleft = tile_height - ymod;
	params.src = tile->data + ymod * params.sraster + xmod;
	lcount = h;
	if ( narrow )			/* Optimize narrow case */
	   {	set_g_mask(mask);
		if ( lcount > yleft )
		   {	params.height = yleft;
			memrwcol0((rop_ptr)&params);
			params.dest += yleft * params.draster;
			params.src = tile->data + xmod;
			params.height = tile_height;
			lcount -= yleft;
			while ( lcount >= tile_height )
			   {	memrwcol0((rop_ptr)&params);
				params.dest += tile_height * params.draster;
				lcount -= tile_height;
			   }
		   }
		if ( lcount )
		   {	params.height = lcount;
			memrwcol0((rop_ptr)&params);
		   }
	   }
	else
	   {	fb_ptr line = params.dest;
		int xpos = width_bytes - xmod;
		while ( 1 )
		   {	int xleft = xpos;
			int count = w;
			params.height = (lcount > yleft ? yleft : lcount);
			/* Do first byte, if not a full byte. */
			if ( mask )
			   {	set_g_mask(mask);
				memrwcol0((rop_ptr)&params);
			   }
			/* Do full bytes */
			if ( (count -= 8) >= 0 )
			   {	set_g_mask(0xff);
				do
				   {	if ( !--xleft )
						xleft = width_bytes,
						params.src -= width_bytes;
					++params.src, ++params.dest;
					memrwcol0((rop_ptr)&params);
				   }
				while ( (count -= 8) >= 0 );
			   }
			/* Do last byte */
			if ( rmask )
			   {	if ( !--xleft )
					xleft = width_bytes,
					params.src -= width_bytes;
				set_g_mask(rmask);
				++params.src, ++params.dest;
				memrwcol0((rop_ptr)&params);
			   }
			if ( (lcount -= params.height) == 0 ) break;
			params.dest = line += params.height * params.draster;
			params.src = tile->data + xmod;
			yleft = tile_height;
		   }
	   }
	/* Now do the second color if needed */
	if ( again )
	   {	maps = again + const_bits;
		set_s_map(maps);
		again = 0;
		params.dest = mk_fb_ptr(x, y);
		if ( mask == 0 ) params.dest--;
		params.invert = 0;
		goto tile;
	   }
	if ( maps != 0xf )
		set_s_map(-1);
	if ( const_bits )
		set_g_const_map(0);
	dot_end();
	return 0;
}

/* Read scan lines back from the frame buffer. */
int
ega_get_bits(gx_device *dev, int y, byte *data, byte **actual_data)
{	/* The maximum width for an EGA/VGA device is 800 pixels.... */
	int width_bytes = (dev->width + 7) >> 3;
	int i;
	bits32 *dest;
	const byte *src;
	const byte *end;
	byte planes[100*4];
	/* Plane 0 is the least significant plane. */
	/* We know we're on a little-endian machine.... */
#define spread4(v)\
 v+0x00000000, v+0x08000000, v+0x80000000, v+0x88000000,\
 v+0x00080000, v+0x08080000, v+0x80080000, v+0x88080000,\
 v+0x00800000, v+0x08800000, v+0x80800000, v+0x88800000,\
 v+0x00880000, v+0x08880000, v+0x80880000, v+0x88880000
	static const bits32 far_data spread8[256] =
	{	spread4(0x0000), spread4(0x0800),
		spread4(0x8000), spread4(0x8800),
		spread4(0x0008), spread4(0x0808),
		spread4(0x8008), spread4(0x8808),
		spread4(0x0080), spread4(0x0880),
		spread4(0x8080), spread4(0x8880),
		spread4(0x0088), spread4(0x0888),
		spread4(0x8088), spread4(0x8888)
	};

	if ( y < 0 || y >= dev->height || dev->width > 800 )
	  return_error(gs_error_rangecheck);
	/* Read 4 planes into the holding buffer. */
	for ( i = 0; i < 4; ++i )
	  {	set_g_read_plane(i);
		memcpy(planes + 100 * i, mk_fb_ptr(0, y), width_bytes);
	  }
	/* Now assemble the final data from the planes. */
	for ( dest = (bits32 *)data, src = planes, end = src + width_bytes;
	      src < end; ++dest, ++src
	    )
	  *dest = (((((spread8[src[0]] >> 1) | spread8[src[100]]) >> 1) |
		    spread8[src[200]]) >> 1) | spread8[src[300]];
	if ( actual_data != 0 )
	  *actual_data = data;
	return 0;
}

/* ------ Internal routines ------ */

/* Mask table for rectangle fill. */
static const byte rmask_tab[9] =
   {	0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff
   };

/* Fill a rectangle specified by pointer into frame buffer, */
/* starting bit within byte, width, and height. */
/* Smashes rop->dest. */
private void near
fill_rectangle(register rop_ptr rop, int bit, int w, int color)
  /* rop: dest, draster, height */
{	set_g_const(color);
	set_g_const_map(0xf);
	select_g_mask();
	if ( bit + w <= 8 )
	   {	/* Less than one byte */
		out_g_mask(rmask_tab[w] >> bit);
		memsetcol(rop);
	   }
	else
	   {	byte right_mask;
		if ( bit )
		   {	out_g_mask(0xff >> bit);
			memsetcol(rop);
			rop->dest++;
			w += bit - 8;
		   }
		if ( w >= 8 )
		   {	out_g_mask(0xff);	/* all bits */
			rop->width = w >> 3;
			memsetrect(rop);
			rop->dest += rop->width;
			w &= 7;
		   }
		if ( (right_mask = rmask_tab[w]) != 0 )
		   {	out_g_mask(right_mask);
			memsetcol(rop);
		   }
	   }
	set_g_const_map(0);
}

/* Fill a single row specified by pointer into frame buffer, */
/* starting bit within byte, and width; clean up afterwards. */
#define r_m_w(ptr) (*(ptr))++		/* read & write, data irrelevant */
private void near
fill_row_only(byte *dest, int bit, int w, int color)
  /* rop: dest */
{	if ( bit + w <= 8 )
	   {	/* Less than one byte. */
		/* Optimize filling with black or white. */
		switch ( color )
		{
		case 0:
			set_g_mask(rmask_tab[w] >> bit);
			*dest &= color;		/* read, then write 0s; */
				/* some compilers optimize &= 0 to a store. */
			out_g_mask(0xff);		/* dot_end */
			break;
		case 0xf:
			set_g_mask(rmask_tab[w] >> bit);
			*dest |= 0xff;		/* read, then write 1s; */
				/* some compilers optimize &= 0 to a store. */
			out_g_mask(0xff);		/* dot_end */
			break;
		default:
			set_g_const(color);
			set_g_const_map(0xf);
			set_g_mask(rmask_tab[w] >> bit);
			r_m_w(dest);
			out_g_mask(0xff);		/* dot_end */
			set_g_const_map(0);
		}
	   }
	else
	   {	byte right_mask;
		int byte_count;
		set_g_const(color);
		set_g_const_map(0xf);
		select_g_mask();
		if ( bit )
		   {	out_g_mask(0xff >> bit);
			r_m_w(dest);
			dest++;
			w += bit - 8;
		   }
		byte_count = w >> 3;
		if ( (right_mask = rmask_tab[w & 7]) != 0 )
		   {	out_g_mask(right_mask);
			r_m_w(dest + byte_count);
		   }
		out_g_mask(0xff);
		if ( byte_count )
		   {	memset(dest, 0, byte_count);	/* data irrelevant */
		   }
		set_g_const_map(0);
	   }
}
