/* Copyright (C) 1992, 1993 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevpccm.c */
/* Support routines for PC color mapping */
#include "gx.h"
#include "gsmatrix.h"			/* for gxdevice.h */
#include "gxdevice.h"
#include "gdevpccm.h"			/* interface */

/* Color mapping routines for EGA/VGA-style color. */
/* Colors are 4 bits: 8=intensity, 4=R, 2=G, 1=B. */

/* Define the color spectrum */
#define v_black 0
#define v_blue 1
#define v_green 2
#define v_cyan 3
#define v_red 4
#define v_magenta 5
#define v_brown 6
#define v_white 7
#define v_dgray 8			/* dark gray is not very usable */
#define v_lblue 9
#define v_lgreen 10
#define v_lcyan 11
#define v_lred 12
#define v_lmagenta 13
#define v_yellow 14
#define v_bwhite 15

/* ------ EGA/VGA (4-bit) color mapping ------ */

gx_color_index
pc_4bit_map_rgb_color(gx_device *dev, gx_color_value r, gx_color_value g,
  gx_color_value b)
{
#define Nb gx_color_value_bits
	static const byte grays[4] = { v_black, v_dgray, v_white, v_bwhite };
#define tab3(v0,v1,v23) { v0, v1, v23, v23 }
	static const byte g0r0[4] = tab3(v_black,v_blue,v_lblue);
	static const byte g0r1[4] = tab3(v_red,v_magenta,v_lmagenta);
	static const byte g0r2[4] = tab3(v_lred,v_lmagenta,v_lmagenta);
	static const byte _ds *g0[4] = tab3(g0r0, g0r1, g0r2);
	static const byte g1r0[4] = tab3(v_green,v_cyan,v_lcyan);
	static const byte g1r1[4] = tab3(v_brown,v_white,v_lcyan);
	static const byte g1r2[4] = tab3(v_yellow,v_lred,v_lmagenta);
	static const byte _ds *g1[4] = tab3(g1r0, g1r1, g1r2);
	static const byte g2r0[4] = tab3(v_lgreen,v_lgreen,v_lcyan);
	static const byte g2r1[4] = tab3(v_lgreen,v_lgreen,v_lcyan);
	static const byte g2r2[4] = tab3(v_yellow,v_yellow,v_bwhite);
	static const byte _ds *g2[4] = tab3(g2r0, g2r1, g2r2);
	static const byte _ds * _ds *ga[4] = tab3(g0, g1, g2);
#undef tab3
#define q4mask (-1 << (Nb - 2))
	if ( !((r ^ g) & q4mask) && !((g ^ b) & q4mask) )	/* gray */
#undef q4mask
		return (gx_color_index)grays[r >> (Nb - 2)];
	else
#define q3cv(v) ((v - (v >> 2)) >> (Nb - 2))
		return (gx_color_index)ga[q3cv(g)][q3cv(r)][q3cv(b)];
#undef q3cv
#undef Nb
}
int
pc_4bit_map_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{
#define icolor (int)color
	gx_color_value one;
	switch ( icolor )
	{
	case v_white:			/* "dark white" */
		prgb[0] = prgb[1] = prgb[2] =
		  gx_max_color_value - (gx_max_color_value / 3);
		return 0;
	case v_dgray:
		prgb[0] = prgb[1] = prgb[2] = gx_max_color_value / 3;
		return 0;
	}
	one = (icolor & 8 ? gx_max_color_value : gx_max_color_value / 2);
	prgb[0] = (icolor & 4 ? one : 0);
	prgb[1] = (icolor & 2 ? one : 0);
	prgb[2] = (icolor & 1 ? one : 0);
	return 0;
#undef icolor
}

/* ------ SVGA 8-bit color mapping ------ */

/*
 * For 8-bit color, we use a fixed palette with approximately
 * 3 bits of R, 3 bits of G, and 2 bits of B.
 * We have to trade off even spacing of colors along each axis
 * against the desire to have real gray shades;
 * we compromise by using a 7x7x5 "cube" with extra gray shades
 * (1/6, 1/3, 2/3, and 5/6), instead of the obvious 8x8x4.
 */

gx_color_index
pc_8bit_map_rgb_color(gx_device *dev, gx_color_value r, gx_color_value g,
  gx_color_value b)
{	uint rv = r / (gx_max_color_value / 7 + 1);
	uint gv = g / (gx_max_color_value / 7 + 1);
	return (gx_color_index)
		(rv == gv && gv == b / (gx_max_color_value / 7 + 1) ?
		 rv + (256-7) :
		 (rv * 7 + gv) * 5 + b / (gx_max_color_value / 5 + 1));
}
int
pc_8bit_map_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{	static const gx_color_value ramp7[8] =
	{	0,
		gx_max_color_value / 6,
		gx_max_color_value / 3,
		gx_max_color_value / 2,
		gx_max_color_value - (gx_max_color_value / 3),
		gx_max_color_value - (gx_max_color_value / 6),
		gx_max_color_value,
		/* The 8th entry is not actually ever used, */
		/* except to fill out the palette. */
		gx_max_color_value
	};
	static const gx_color_value ramp5[5] =
	{	0,
		gx_max_color_value / 4,
		gx_max_color_value / 2,
		gx_max_color_value - (gx_max_color_value / 4),
		gx_max_color_value
	};
#define icolor (uint)color
	if ( icolor >= 256-7 )
	{	prgb[0] = prgb[1] = prgb[2] = ramp7[icolor - (256-7)];
	}
	else
	{	prgb[0] = ramp7[icolor / 35];
		prgb[1] = ramp7[(icolor / 5) % 7];
		prgb[2] = ramp5[icolor % 5];
	}
#undef icolor
	return 0;
}

/* Write a palette on a file. */
int
pc_write_palette(gx_device *dev, uint max_index, FILE *file)
{	uint i, c;
	gx_color_value rgb[3];
	for ( i = 0; i < max_index; i++ )
	{	(*dev_proc(dev, map_color_rgb))(dev, (gx_color_index)i, rgb);
		for ( c = 0; c < 3; c++ )
		{	byte b = rgb[c] >> (gx_color_value_bits - 8);
			fputc(b, file);
		}
	}
	return 0;
}
